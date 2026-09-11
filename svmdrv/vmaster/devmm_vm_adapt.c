/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#include "vtsdrv_interface.h"

#include "devmm_common.h"
#include "devmm_vpc_mng_msg_def.h"
#include "svm_heap_mng.h"
#include "svm_master_convert.h"
#include "svm_master_memcpy.h"
#include "svm_master_addr_map.h"
#include "svm_master_dma_desc_mng.h"
#include "devmm_proc_mem_copy.h"
#include "svm_dma.h"
#include "svm_proc_mng.h"
#include "svm_msg_client.h"
#include "devmm_page_cache.h"
#include "svm_master_mem_map.h"
#include "svm_master_remote_map.h"
#include "svm_master_mem_repair.h"
#include "svm_shmem_node.h"
#include "svm_mem_query.h"
#include "svm_dma_prepare_pool.h"
#include "devmm_vm_vpc.h"
#include "svm_hot_reset.h"
#include "svm_vm_version_adapt.h"
#include "svm_interface.h"
#include "devmm_vm_adapt.h"

#define DEVMM_DEFAULT_DEV_ID 0

struct devmm_vm_convert_para {
    struct devmm_memory_attributes *src_attr;
    struct devmm_memory_attributes *dst_attr;
    u64 host_va;
    u32 pin_flg;
    u32 pa_num;
    u32 dev_id;
    int write;
};

u32 devmm_get_convert_limit_size(u32 devid, u32 vfid)
{
    return devmm_get_convert_64m_size();
}

int devmm_init_process_notice_pm(struct devmm_svm_process *svm_proc)
{
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_init_process);
    struct devmm_vpc_mng_msg_init_process proc = {{{0}}};

    proc.head.dev_id = DEVMM_DEFAULT_DEV_ID; /* not care device */
    proc.head.process_id.hostpid = svm_proc->process_id.hostpid;
    proc.head.msg_id = DEVMM_VPC_INIT_PROC;

    return devmm_vm_vpc_mng_msg_send(&proc, len, len);
}

int devmm_release_process_notice_pm(struct devmm_svm_process *svm_proc)
{
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_init_process);
    struct devmm_vpc_mng_msg_init_process proc = {{{0}}};

    proc.head.dev_id = DEVMM_DEFAULT_DEV_ID; /* not care device */
    proc.head.process_id.hostpid = svm_proc->process_id.hostpid;
    proc.head.msg_id = DEVMM_VPC_UNINIT_PROC;

    return devmm_vm_vpc_mng_msg_send(&proc, len, len);
}

STATIC int devmm_vm_create_device_page(struct devmm_svm_process *svm_proc,
    struct devmm_page_query_arg *arg)
{
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_create_dev_page);
    struct devmm_vpc_mng_msg_create_dev_page create_msg = {{{0}}};
    u32 *page_bitmap = NULL;

    create_msg.head.msg_id = DEVMM_VPC_MNG_CREATE_DEV_PAGE;
    create_msg.head.dev_id = arg->dev_id;
    create_msg.head.process_id.hostpid = svm_proc->process_id.hostpid;

    create_msg.va = (u64)arg->va;
    create_msg.size = (u64)arg->size;
    create_msg.offset = (u64)arg->offset;
    create_msg.page_size = arg->page_size;
    page_bitmap = devmm_get_page_bitmap(svm_proc, create_msg.va);
    create_msg.bitmap = page_bitmap ? devmm_page_read_bitmap(page_bitmap) : 0;
    create_msg.page_cnt = devmm_get_pagecount_by_size(arg->va, arg->size, arg->page_size);
    return devmm_vm_vpc_mng_msg_send(&create_msg, len, len);
}

int devmm_query_page_by_msg(struct devmm_svm_process *svm_proc, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num)
{
    struct devmm_page_query_arg create_arg = query_arg;
    u64 aligned_va, total_size, total_num, ready_num;
    u64 num_per_msg = DEVMM_PAGE_NUM_PER_MSG;
    int ret;

    aligned_va = ka_base_round_down(query_arg.va, query_arg.page_size);
    total_size = ka_base_round_up((query_arg.size + query_arg.offset), query_arg.page_size);
    total_num = total_size / query_arg.page_size;

    create_arg.offset = 0;
    create_arg.va = aligned_va;
    create_arg.size = ka_base_min(total_num, num_per_msg) * query_arg.page_size;
    for (ready_num = 0; ready_num < total_num;) {
        ret = devmm_vm_create_device_page(svm_proc, &create_arg);
        if (ret != 0) {
            devmm_drv_err("Create device page fail. (devid=%u; va=0x%llx; size=%llu)\n",
                create_arg.dev_id, create_arg.va, create_arg.size);
            return ret;
        }
        ready_num += create_arg.size / create_arg.page_size;
        create_arg.va += create_arg.size;
        create_arg.size = ka_base_min(total_num - ready_num, num_per_msg) * query_arg.page_size;
    }
    return 0;
}

void devmm_free_pages_cache(struct devmm_svm_process *svm_proc,
    u32 devid, u32 page_num, u32 page_size, u64 va, bool reuse)
{
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_del_dev_page_cache);
    struct devmm_vpc_mng_msg_del_dev_page_cache msg = {{{0}}};
    int ret;

    msg.head.msg_id = DEVMM_VPC_MNG_DELETE_DEV_PAGE_CACHE;
    msg.head.dev_id = devid;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;

    msg.type = DEL_PAGE_CACHE_TYPE_VA;
    msg.va = va;
    msg.page_num = page_num;
    msg.page_size = page_size;
    msg.reuse = reuse;

    ret = devmm_vm_vpc_mng_msg_send(&msg, len, len);
    if (ret != 0) {
        devmm_drv_err("Vm management message send fail. (hostpid=%d; devid=%d; ret=%d)\n",
            svm_proc->process_id.hostpid, devid, ret);
    }
}

void devmm_destroy_dev_pages_cache(struct devmm_svm_process *svm_proc, u32 devid)
{
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_del_dev_page_cache);
    struct devmm_vpc_mng_msg_del_dev_page_cache msg = {{{0}}};
    int ret;

    msg.head.msg_id = DEVMM_VPC_MNG_DELETE_DEV_PAGE_CACHE;
    msg.head.dev_id = devid;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;

    msg.type = DEL_PAGE_CACHE_TYPE_DEV;

    ret = devmm_vm_vpc_mng_msg_send(&msg, len, len);
    if (ret != 0) {
        devmm_drv_err("Vm management message send fail. (hostpid=%d; devid=%d; ret=%d)\n",
            svm_proc->process_id.hostpid, devid, ret);
    }
}

static int devmm_ka_mm_pin_user_pages_fast(unsigned long start, int nr_pages, int write, ka_page_t **pages)
{
    return ka_mm_pin_user_pages_fast(start, nr_pages, (write != 0) ? KA_FOLL_WRITE : 0, pages);
}

STATIC int devmm_get_host_pa_list(struct devmm_svm_process *svm_proc, u64 host_va,
    struct devmm_pa_list *pa_list, u32 pin_flg, int write)
{
    ka_vm_area_struct_t *vma = NULL;
    ka_page_t **pages = NULL;
    u64 vaddr = host_va;
    int i;

    if (pin_flg != DEVMM_USER_PIN_PAGES) {
        vma = devmm_find_vma(svm_proc, vaddr);
        if (vma == NULL) {
            devmm_drv_err("Can not find vma. (va=0x%llx; hostpid=%d)\n", vaddr, svm_proc->process_id.hostpid);
            return -EINVAL;
        }
        for (i = 0; i < pa_list->pa_num; i++) {
            int ret;
            vaddr = ka_base_round_down(vaddr, KA_MM_PAGE_SIZE);
            ret = devmm_va_to_pa(vma, vaddr, &pa_list->palist[i]);
            if (ret != 0) {
                devmm_drv_err("Va_to_paka_list_add_pgtable fail. (va=%llx)\n", vaddr);
                return ret;
            }
            vaddr += KA_MM_PAGE_SIZE;
        }
    } else {
        int got_num;
        pages = (ka_page_t **)devmm_kmalloc_ex(sizeof(ka_page_t *) * pa_list->pa_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (pages == NULL) {
            devmm_drv_err("Alloc memory failed. (host_va=%llx; pa_num=%d)\n", host_va, pa_list->pa_num);
            return -ENOMEM;
        }

        got_num = devmm_ka_mm_pin_user_pages_fast(host_va, (int)pa_list->pa_num, write, pages);
        if (got_num != pa_list->pa_num) {
            devmm_drv_err("Get_user_pages fail. (host_va=%llx; pg_num=%d; got_num=%d)\n",
                host_va, got_num, pa_list->pa_num);
            devmm_unpin_user_pages(pages, pa_list->pa_num, (u64)(u32)got_num);
            devmm_kfree_ex(pages);
            return -EFAULT;
        }

        for (i = 0; i < got_num; i++) {
            pa_list->palist[i] = ka_mm_page_to_phys(pages[i]);
        }
        devmm_kfree_ex(pages);
    }

    return 0;
}

STATIC void devmm_free_host_pa_list(u32 pa_num, u64 *palist, u32 pin_flg)
{
    ka_page_t *pg = NULL;
    u32 i;

    if (pin_flg != DEVMM_USER_PIN_PAGES) {
        return;
    }

    for (i = 0; i < pa_num; i++) {
        pg = devmm_pa_to_page(palist[i]);
        ka_mm_unpin_user_page(pg);
    }
}

STATIC void devmm_free_all_host_pa_list(struct devmm_pa_batch *pa_batch)
{
    struct devmm_pa_batch *pa_batch_temp = NULL;
    struct devmm_pa_batch *pa_batch_cur = pa_batch;
    u32 stamp = (u32)ka_jiffies;

    while (pa_batch_cur != NULL) {
        pa_batch_temp = pa_batch_cur;
        devmm_free_host_pa_list(pa_batch_cur->pa_list.pa_num, pa_batch_cur->pa_list.palist,
            pa_batch_cur->pa_list.pin_flg);
        pa_batch_cur = pa_batch_cur->next;
        devmm_kfree_ex(pa_batch_temp);
        devmm_try_cond_resched(&stamp);
    }
}

STATIC void devmm_vm_fill_vpc_attr(struct devmm_vpc_memory_attributes *vpc_va_attr,
    struct devmm_memory_attributes *va_attr)
{
    vpc_va_attr->is_local_host = va_attr->is_local_host;
    vpc_va_attr->is_host_pin = va_attr->is_host_pin;
    vpc_va_attr->is_svm = va_attr->is_svm;
    vpc_va_attr->is_svm_huge = va_attr->is_svm_huge;
    vpc_va_attr->is_svm_host = va_attr->is_svm_host;
    vpc_va_attr->is_svm_device = va_attr->is_svm_device;
    vpc_va_attr->is_svm_non_page = va_attr->is_svm_non_page;
    vpc_va_attr->is_ipc_open = va_attr->is_ipc_open;
    vpc_va_attr->copy_use_va = va_attr->copy_use_va;
    vpc_va_attr->bitmap = va_attr->bitmap;
    vpc_va_attr->logical_devid = va_attr->logical_devid;
    vpc_va_attr->devid = va_attr->devid;
    vpc_va_attr->vfid = va_attr->vfid;
    vpc_va_attr->page_size = va_attr->page_size;
    vpc_va_attr->host_page_size = va_attr->host_page_size;
    vpc_va_attr->va = va_attr->va;
}

STATIC void devmm_vm_fill_vpc_copy_para(struct devmm_vpc_mem_copy_convrt_para *vpc_copy_para,
    struct devmm_mem_copy_convrt_para *copy_para)
{
    vpc_copy_para->dst = copy_para->dst;
    vpc_copy_para->src = copy_para->src;
    vpc_copy_para->count = copy_para->count;
    vpc_copy_para->direction = copy_para->direction;
    vpc_copy_para->blk_size = copy_para->blk_size;
    vpc_copy_para->create_msg = copy_para->create_msg;
}

STATIC int devmm_vm_memcpy_process(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr)
{
    struct devmm_vpc_mng_msg_mem_copy *copy_msg = NULL;
    u32 pin_flg, dev_id, size;
    u64 host_va = 0;
    u32 pa_num = 0;
    int write, ret;

    if (para->direction == DEVMM_COPY_DEVICE_TO_DEVICE) {
        dev_id = src_attr->devid;
        pin_flg = 0;
        write = 0;
    } else if (para->direction == DEVMM_COPY_HOST_TO_DEVICE) {
        host_va = para->src;
        dev_id = dst_attr->devid;
        pin_flg = src_attr->is_local_host ? DEVMM_USER_PIN_PAGES : DEVMM_UNPIN_PAGES;
        write = 0;
        pa_num = (u32)devmm_get_pagecount_by_size(host_va, para->count, KA_MM_PAGE_SIZE);
    } else {
        host_va = para->dst;
        dev_id = src_attr->devid;
        pin_flg = dst_attr->is_local_host ? DEVMM_USER_PIN_PAGES : DEVMM_UNPIN_PAGES;
        write = 1;
        pa_num = (u32)devmm_get_pagecount_by_size(host_va, para->count, KA_MM_PAGE_SIZE);
    }

    size = sizeof(struct devmm_vpc_mng_msg_mem_copy) + pa_num * sizeof(u64);
    copy_msg = (struct devmm_vpc_mng_msg_mem_copy *)devmm_kvzalloc(size);
    if (copy_msg == NULL) {
        devmm_drv_err("Kvzalloc failed. (hostpid=%d; size=%lld)\n", svm_proc->process_id.hostpid, (u64)size);
        return -ENOMEM;
    }

    copy_msg->head.msg_id = DEVMM_VPC_MNG_COPY_MEM;
    copy_msg->head.dev_id = dev_id;
    copy_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    copy_msg->head.extend_num = (u16)pa_num;

    devmm_vm_fill_vpc_copy_para(&copy_msg->para, para);
    devmm_vm_fill_vpc_attr(&copy_msg->src_attr, src_attr);
    devmm_vm_fill_vpc_attr(&copy_msg->dst_attr, dst_attr);
    copy_msg->pa_list.pa_num = pa_num;

    if (pa_num > 0) {
        ret = devmm_get_host_pa_list(svm_proc, host_va, &copy_msg->pa_list, pin_flg, write);
        if (ret != 0) {
            devmm_kvfree(copy_msg);
            devmm_drv_err("Get pa list failed. (hostpid=%d pa_num=%d)\n", svm_proc->process_id.hostpid, pa_num);
            return ret;
        }
    }

    /* sent to pm */
    ret = devmm_vm_vpc_mng_msg_send(copy_msg, size, size);
    if (ret != 0) {
        devmm_drv_err("Vpc message send fail. (hostpid=%d; ret=%d)\n", svm_proc->process_id.hostpid, ret);
    }

    devmm_free_host_pa_list(copy_msg->pa_list.pa_num, copy_msg->pa_list.palist, pin_flg);
    devmm_kvfree(copy_msg);

    return ret;
}

void devmm_async_cpy_inc_addr_ref(struct devmm_svm_process *svm_proc, u64 src, u64 dst, u64 size)
{
}

int devmm_ioctl_memcpy_process_res(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr)
{
    /* update va in attributes */
    src_attr->va = para->src;
    dst_attr->va = para->dst;
    para->task_mode = DEVMM_CPY_SYNC_MODE;  /* vm change to sync mode */
    return devmm_vm_memcpy_process(svm_proc, para, src_attr, dst_attr);
}

int devmm_cpy_result_refresh(struct devmm_svm_process *svm_proc,
    struct devmm_mem_async_copy_para *async_copy_para)
{
    async_copy_para->cpy_state = DEVMM_ASYNC_CPY_FINISH_VALUE;
    return 0;
}

static int devmm_sumbit_convert_sync_dma(struct devmm_svm_process *svm_proc, u64 src, u64 dst, u64 len)
{
    struct devmm_ioctl_arg arg = {{0}};
    int ret;

    arg.data.copy_para.src = src;
    arg.data.copy_para.dst = dst;
    arg.data.copy_para.ByteCount = len;
    arg.data.copy_para.direction = DEVMM_COPY_INVILED_DIRECTION;

    ret = devmm_ioctl_memcpy_proc(svm_proc, &arg);
    if (ret != 0) {
        devmm_drv_err("Memcpy fail. (hostpid=%d; ret=%d; src=0x%llx; dst=0x%llx)\n",
            svm_proc->process_id.hostpid, ret, src, dst);
    }

    return ret;
}

int devmm_sumbit_convert_dma_proc(struct devmm_svm_process *svm_proc, struct DMA_ADDR *dma_addr, int sync_flag)
{
    u64 offset = (u64)(uintptr_t)dma_addr->phyAddr.priv;
    struct devmm_pa_node *pa_node = NULL;
    ka_rb_node_t *node = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    ka_task_mutex_lock(&svm_proc->host_pa_list.rbtree_mutex);
    node = ka_base_get_rb_root_node(&svm_proc->host_pa_list.pa_rbtree);
    while (node != NULL) {
        pa_node = ka_base_rb_entry(node, struct devmm_pa_node, pa_node);
        if (offset < pa_node->offset) {
            node = ka_base_get_rb_node_left(node);
        } else if (offset > pa_node->offset) {
            node = ka_base_get_rb_node_right(node);
        } else {
            if (pa_node->status != CONVERT_NODE_IDLE) {
                ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
                devmm_drv_err("Dma node is in using. (offset=0x%llx)\n", offset);
                return -ETXTBSY;
            }
            pa_node->status = CONVERT_NODE_COPYING;
            ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
            ret = devmm_sumbit_convert_sync_dma(svm_proc, pa_node->src, pa_node->dst, pa_node->len);

            ka_task_mutex_lock(&svm_proc->host_pa_list.rbtree_mutex);
            pa_node->status = CONVERT_NODE_IDLE;
            ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);

            return ret;
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);

    return -EFAULT;
}

int devmm_wait_convert_dma_result(struct devmm_svm_process *svm_proc, struct DMA_ADDR *dma_addr)
{
    return 0;
}

STATIC int devmm_add_pa_to_list(struct devmm_svm_process *svm_proc,
    struct devmm_mem_convrt_addr_para *convrt_para, struct devmm_pa_batch **pa_batch)
{
    struct devmm_pa_node *pa_node = NULL;
    ka_rb_node_t **new_node = NULL;
    ka_rb_node_t *parent = NULL;
    int ret;

    pa_node = devmm_kzalloc_ex(sizeof(struct devmm_pa_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_node == NULL) {
        devmm_drv_err("Kzalloc failed.\n");
        return -ENOMEM;
    }

    pa_node->offset = (u64)(uintptr_t)convrt_para->dmaAddr.phyAddr.priv;
    pa_node->pa_batch = *pa_batch;
    pa_node->src = convrt_para->pSrc;
    pa_node->dst = convrt_para->pDst;
    pa_node->len = convrt_para->len * convrt_para->height;
    pa_node->status = CONVERT_NODE_IDLE;
    ka_task_mutex_lock(&svm_proc->host_pa_list.rbtree_mutex);
    new_node = ka_base_get_rb_root_node_addr(&svm_proc->host_pa_list.pa_rbtree);
    /* Figure out where to put new node */
    while (*new_node) {
        struct devmm_pa_node *this = ka_base_rb_entry(*new_node, struct devmm_pa_node, pa_node);
        parent = *new_node;
        if (pa_node->offset < this->offset) {
            new_node = ka_base_get_rb_node_left_addr(*new_node);
        } else if (pa_node->offset > this->offset) {
            new_node = ka_base_get_rb_node_right_addr(*new_node);
        } else {
            ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
            devmm_drv_err("Insert same. (offset=0x%llx)\n", pa_node->offset);
            ret = -EFAULT;
            goto free_pa_node;
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(&pa_node->pa_node, parent, new_node);
    ka_base_rb_insert_color(&pa_node->pa_node, &svm_proc->host_pa_list.pa_rbtree);
    ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
    return 0;

free_pa_node:
    devmm_kfree_ex(pa_node);
    pa_node = NULL;
    return ret;
}

STATIC void devmm_convert2d_make_palist(struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para, struct devmm_pa_batch **pa_batch_head)
{
    u64 height = arg->data.convrt_para.height;
    struct devmm_pa_batch *pa_batch_tmp = NULL;

    if (convrt_para[0].pa_batch != NULL) {
        int i;
        *pa_batch_head = convrt_para[0].pa_batch;
        pa_batch_tmp = convrt_para[0].pa_batch;
        convrt_para[0].pa_batch = NULL;
        for (i = 1; i < height; i++) {
            if (convrt_para[i].pa_batch != NULL) {
                pa_batch_tmp->next = convrt_para[i].pa_batch;
                pa_batch_tmp = pa_batch_tmp->next;
                convrt_para[i].pa_batch = NULL;
            }
        }
        pa_batch_tmp = NULL;
    }
}

static struct devmm_pa_batch *devmm_one_addr_make_palist(u32 pa_num, u32 pin_flg,
    struct devmm_vpc_mng_msg_convert_addr *convert_msg)
{
    struct devmm_pa_batch *pa_batch = NULL;
    u32 size;
    int ret;

    size = sizeof(struct devmm_pa_batch) + pa_num * sizeof(u64);
    pa_batch = (struct devmm_pa_batch *)devmm_kvzalloc(size);
    if (pa_batch == NULL) {
        devmm_drv_err("Pa_list devmm_kvzalloc_ex failed. (size=%u)\n", size);
        return NULL;
    }

    pa_batch->pa_list.pa_num = pa_num;
    pa_batch->pa_list.pin_flg = pin_flg;
    ret = memcpy_s(pa_batch->pa_list.palist, pa_num * sizeof(u64), convert_msg->pa_list.palist, pa_num * sizeof(u64));
    if (ret != 0) {
        devmm_kvfree(pa_batch);
        return NULL;
    }

    return pa_batch;
}

STATIC void devmm_one_addr_free_palist(struct devmm_pa_batch *pa_batch)
{
    if (pa_batch != NULL) {
        devmm_kvfree(pa_batch);
    }
}

STATIC void devmm_vm_fill_vpc_convert_dma_para(struct devmm_vpc_mem_convrt_addr_para *vpc_convrt_para,
    struct devmm_mem_convrt_addr_para *convrt_para, u32 dir)
{
    if (dir == DEVMM_VPC_VM_TO_PM) {
        vpc_convrt_para->pSrc = convrt_para->pSrc;
        vpc_convrt_para->pDst = convrt_para->pDst;
        vpc_convrt_para->spitch = convrt_para->spitch;
        vpc_convrt_para->dpitch = convrt_para->dpitch;
        vpc_convrt_para->fixed_size = convrt_para->fixed_size;
        vpc_convrt_para->direction = convrt_para->direction;
        vpc_convrt_para->dmaAddr = convrt_para->dmaAddr;
        vpc_convrt_para->virt_id = convrt_para->virt_id;
    } else {
        convrt_para->dmaAddr = vpc_convrt_para->dmaAddr;
    }
}

int devmm_convert2d_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para)
{
    struct devmm_vpc_mng_msg_convert_dma_addr *convert_dma_msg = NULL;
    u64 height = arg->data.convrt_para.height;
    struct devmm_pa_batch *pa_batch_head = NULL;
    u32 size;
    int ret;
    u64 i;

    size = sizeof(struct devmm_vpc_mng_msg_convert_dma_addr);
    convert_dma_msg = (struct devmm_vpc_mng_msg_convert_dma_addr *)devmm_kzalloc_ex(size, KA_GFP_KERNEL);
    if (convert_dma_msg == NULL) {
        devmm_drv_err("Kzalloc failed. (hostpid=%d; size=%lld)\n", svm_proc->process_id.hostpid, (u64)size);
        ret = -ENOMEM;
        goto devmm_free_palist;
    }

    convert_dma_msg->head.msg_id = DEVMM_VPC_MNG_CONVERT_DMA_ADDR;
    convert_dma_msg->head.dev_id = convrt_para[0].dev_id;
    convert_dma_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    convert_dma_msg->head.extend_num = 0;
    convert_dma_msg->convrt_para.convert_id = (u32)convrt_para[0].convert_id;
    convert_dma_msg->convrt_para.destroy_flag = convrt_para[0].destroy_flag;
    devmm_vm_fill_vpc_convert_dma_para(&convert_dma_msg->convrt_para, &arg->data.convrt_para, DEVMM_VPC_VM_TO_PM);

    ret = devmm_vm_vpc_mng_msg_send(convert_dma_msg, size, size);
    if (ret != 0) {
        devmm_drv_err("Vpc message send fail. (hostpid=%d; ret=%d)\n", svm_proc->process_id.hostpid, ret);
        goto devmm_free_convert_dma_msg;
    }

    if (convrt_para[0].destroy_flag == 0) {
        devmm_vm_fill_vpc_convert_dma_para(&convert_dma_msg->convrt_para, &arg->data.convrt_para, DEVMM_VPC_PM_TO_VM);
        devmm_convert2d_make_palist(arg, convrt_para, &pa_batch_head);
        (void)devmm_add_pa_to_list(svm_proc, &arg->data.convrt_para, &pa_batch_head);
    } else {
        goto devmm_free_convert_dma_msg;
    }

    devmm_kfree_ex(convert_dma_msg);
    return 0;

devmm_free_convert_dma_msg:
    devmm_kfree_ex(convert_dma_msg);
devmm_free_palist:
    for (i = 0; i < height; i++) {
        if (convrt_para[i].pa_batch == NULL) {
            continue;
        }

        devmm_free_host_pa_list(convrt_para[i].pa_batch->pa_list.pa_num, convrt_para[i].pa_batch->pa_list.palist,
            convrt_para[i].pa_batch->pa_list.pin_flg);
        devmm_one_addr_free_palist(convrt_para[i].pa_batch);
    }
    return ret;
}

STATIC void devmm_set_vm_convert_para(struct devmm_memory_attributes *src_attr,
    struct devmm_memory_attributes *dst_attr, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_vm_convert_para *vm_convert_para)
{
    if (convrt_para->direction == DEVMM_COPY_DEVICE_TO_DEVICE) {
        vm_convert_para->dev_id = src_attr->devid;
        vm_convert_para->pin_flg = 0;
        vm_convert_para->write = 0;
    } else if (convrt_para->direction == DEVMM_COPY_HOST_TO_DEVICE) {
        vm_convert_para->host_va = convrt_para->pSrc;
        vm_convert_para->dev_id = dst_attr->devid;
        vm_convert_para->pin_flg = src_attr->is_local_host ? DEVMM_USER_PIN_PAGES : DEVMM_UNPIN_PAGES;
        vm_convert_para->write = 0;
        vm_convert_para->pa_num =
            devmm_get_pagecount_by_size(vm_convert_para->host_va, convrt_para->len, (u32)KA_MM_PAGE_SIZE);
    } else {
        vm_convert_para->host_va = convrt_para->pDst;
        vm_convert_para->dev_id = src_attr->devid;
        vm_convert_para->pin_flg = dst_attr->is_local_host ? DEVMM_USER_PIN_PAGES : DEVMM_UNPIN_PAGES;
        vm_convert_para->write = 1;
        vm_convert_para->pa_num =
            devmm_get_pagecount_by_size(vm_convert_para->host_va, convrt_para->len, (u32)KA_MM_PAGE_SIZE);
    }
    vm_convert_para->src_attr = src_attr;
    vm_convert_para->dst_attr = dst_attr;
}

STATIC void devmm_vm_fill_vpc_convert_para(struct devmm_vpc_mem_convrt_addr_para *vpc_convrt_para,
    struct devmm_mem_convrt_addr_para *convrt_para, u32 dir)
{
    if (dir == DEVMM_VPC_VM_TO_PM) {
        vpc_convrt_para->pSrc = convrt_para->pSrc;
        vpc_convrt_para->pDst = convrt_para->pDst;
        vpc_convrt_para->len = convrt_para->len;
        vpc_convrt_para->direction = convrt_para->direction;
        vpc_convrt_para->dmaAddr = convrt_para->dmaAddr;
        vpc_convrt_para->convert_id = (u32)convrt_para->convert_id;
    } else {
        convrt_para->dmaAddr = vpc_convrt_para->dmaAddr;
        convrt_para->dma_node_num = vpc_convrt_para->dma_node_num;
    }
}

STATIC void devmm_set_convert_msg(struct devmm_vm_convert_para *vm_convert_para,
    struct devmm_svm_process *svm_proc, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_vpc_mng_msg_convert_addr *convert_msg)
{
    u32 page_size = ka_base_min((u32)KA_MM_PAGE_SIZE, devmm_svm->device_page_size);

    convert_msg->head.msg_id = DEVMM_VPC_MNG_CONVERT_ADDR;
    convert_msg->head.dev_id = vm_convert_para->dev_id;
    convert_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    convert_msg->head.extend_num = (u16)vm_convert_para->pa_num;

    devmm_vm_fill_vpc_convert_para(&convert_msg->convrt_para, convrt_para, DEVMM_VPC_VM_TO_PM);
    devmm_vm_fill_vpc_attr(&convert_msg->src_attr, vm_convert_para->src_attr);
    devmm_vm_fill_vpc_attr(&convert_msg->dst_attr, vm_convert_para->dst_attr);
    convert_msg->pa_list.pa_num = vm_convert_para->pa_num;
    convert_msg->page_size = page_size;
}

int devmm_ioctl_convert_addr_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr)
{
    struct devmm_vpc_mng_msg_convert_addr *convert_msg = NULL;
    struct devmm_vm_convert_para vm_convert_para = {0};
    struct devmm_pa_batch *pa_batch = NULL;
    u32 size;
    int ret;

    devmm_set_vm_convert_para(src_attr, dst_attr, convrt_para, &vm_convert_para);
    size = sizeof(struct devmm_vpc_mng_msg_convert_addr) + vm_convert_para.pa_num * sizeof(u64);
    convert_msg = (struct devmm_vpc_mng_msg_convert_addr *)devmm_kvzalloc(size);
    if (convert_msg == NULL) {
        devmm_drv_err("Kvzalloc failed. (hostpid=%d; size=%lld)\n", svm_proc->process_id.hostpid, (u64)size);
        return -ENOMEM;
    }
    devmm_set_convert_msg(&vm_convert_para, svm_proc, convrt_para, convert_msg);

    if (vm_convert_para.pa_num > 0) {
        ret = devmm_get_host_pa_list(svm_proc, vm_convert_para.host_va, &convert_msg->pa_list,
            vm_convert_para.pin_flg, vm_convert_para.write);
        if (ret != 0) {
            devmm_drv_err("Get pa list failed. (hostpid=%d; pa_num=%d)\n", svm_proc->process_id.hostpid,
                vm_convert_para.pa_num);
            goto devmm_free_convert_msg;
        }
        pa_batch = devmm_one_addr_make_palist(vm_convert_para.pa_num, vm_convert_para.pin_flg, convert_msg);
        if (pa_batch == NULL) {
            devmm_drv_err("Make_palist failed. (hostpid=%d; pa_num=%d)\n", svm_proc->process_id.hostpid,
                vm_convert_para.pa_num);
            ret = -ENOMEM;
            goto devmm_free_host_palist;
        }
    }
    ret = devmm_vm_vpc_mng_msg_send(convert_msg, size,
        sizeof(struct devmm_vpc_mng_msg_convert_addr) - sizeof(struct devmm_pa_list));
    if (ret != 0) {
        devmm_drv_err("Vpc message send fail. (hostpid=%d; ret=%d)\n", svm_proc->process_id.hostpid, ret);
        goto devmm_free_palist;
    }

    devmm_vm_fill_vpc_convert_para(&convert_msg->convrt_para, convrt_para, DEVMM_VPC_PM_TO_VM);
    convrt_para->dev_id = vm_convert_para.dev_id;
    convrt_para->pa_batch = pa_batch;
    goto devmm_free_convert_msg;

devmm_free_palist:
    devmm_one_addr_free_palist(pa_batch);
devmm_free_host_palist:
    devmm_free_host_pa_list(convert_msg->pa_list.pa_num, convert_msg->pa_list.palist, vm_convert_para.pin_flg);
devmm_free_convert_msg:
    devmm_kvfree(convert_msg);
    return ret;
}

STATIC void devmm_vm_fill_vpc_destroy_para(struct devmm_vpc_mem_desty_addr_para *vpc_desty_para,
    struct devmm_mem_destroy_addr_para *destroy_para, u32 dir)
{
    if (dir == DEVMM_VPC_VM_TO_PM) {
        vpc_desty_para->dmaAddr = destroy_para->dmaAddr;
    } else {
        destroy_para->pSrc = vpc_desty_para->pSrc;
        destroy_para->pDst = vpc_desty_para->pDst;
        destroy_para->spitch = vpc_desty_para->spitch;
        destroy_para->dpitch = vpc_desty_para->dpitch;
        destroy_para->len = vpc_desty_para->len;
        destroy_para->height = vpc_desty_para->height;
        destroy_para->fixed_size = vpc_desty_para->fixed_size;
        destroy_para->dmaAddr = vpc_desty_para->dmaAddr;
    }
}

int devmm_ioctl_destroy_addr_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    u32 size = (u32)sizeof(struct devmm_vpc_mng_msg_destroy_addr);
    u64 offset = (u64)(uintptr_t)arg->data.desty_para.dmaAddr.phyAddr.priv;
    struct devmm_vpc_mng_msg_destroy_addr destroy_msg = {{{0}}};
    struct devmm_pa_node *pa_node = NULL;
    ka_rb_node_t *node = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    ka_task_mutex_lock(&svm_proc->host_pa_list.rbtree_mutex);
    node = ka_base_get_rb_root_node(&svm_proc->host_pa_list.pa_rbtree);
    while (node != NULL) {
        pa_node = ka_base_rb_entry(node, struct devmm_pa_node, pa_node);
        if (offset < pa_node->offset) {
            node = ka_base_get_rb_node_left(node);
        } else if (offset > pa_node->offset) {
            node = ka_base_get_rb_node_right(node);
        } else {
            if (pa_node->status != CONVERT_NODE_IDLE) {
                ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
                devmm_drv_err("Dma node is in using. (offset=0x%llx)\n", offset);
                return -ETXTBSY;
            }
            pa_node->status = CONVERT_NODE_FREEING;
            ka_base_rb_erase(&pa_node->pa_node, &svm_proc->host_pa_list.pa_rbtree);
            ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
            devmm_free_all_host_pa_list(pa_node->pa_batch);
            devmm_kfree_ex(pa_node);
            pa_node = NULL;
            goto pm_destroy;
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
    devmm_drv_err("Find nothing to destory. (offset=0x%llx)\n", offset);
    return -EINVAL;

pm_destroy:
    destroy_msg.head.msg_id = DEVMM_VPC_MNG_DESTROY_ADDR;
    destroy_msg.head.dev_id = arg->head.devid;
    destroy_msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    devmm_vm_fill_vpc_destroy_para(&destroy_msg.desty_para, &arg->data.desty_para, DEVMM_VPC_VM_TO_PM);
    ret = devmm_vm_vpc_mng_msg_send(&destroy_msg, size, size);
    if (ret != 0) {
        devmm_drv_err("Vpc message send fail. (hostpid=%d; ret=%d)\n", svm_proc->process_id.hostpid, ret);
        return ret;
    }
    devmm_vm_fill_vpc_destroy_para(&destroy_msg.desty_para, &arg->data.desty_para, DEVMM_VPC_PM_TO_VM);

    return ret;
}

bool devmm_check_va_is_convert(struct devmm_svm_process *svm_proc, u64 va)
{
    return (!(ka_base_rb_first(&svm_proc->host_pa_list.pa_rbtree) == NULL));
}

STATIC void devmm_vm_fill_vpc_translate_para(struct devmm_vpc_translate_info *vpc_trans,
    struct devmm_translate_info *trans)
{
    vpc_trans->va = trans->va;
    vpc_trans->page_size = trans->page_size;
}

int devmm_set_translate_pa_addr_to_device(struct devmm_svm_process *svm_process,
    struct devmm_translate_info trans, u64 *pa_offset)
{
    struct devmm_vpc_mng_msg_translate_addr translate_msg = {{{0}}};
    u32 size = (u32)sizeof(struct devmm_vpc_mng_msg_translate_addr);
    int ret;

    translate_msg.head.msg_id = DEVMM_VPC_MNG_TRANSLATE_ADDR;
    translate_msg.head.dev_id = trans.dev_id;
    translate_msg.head.logical_devid = trans.page_insert_dev_id;
    translate_msg.head.process_id.hostpid = svm_process->process_id.hostpid;

    devmm_vm_fill_vpc_translate_para(&translate_msg.trans, &trans);

    ret = devmm_vm_vpc_mng_msg_send(&translate_msg, size, size);
    if (ret != 0) {
        devmm_drv_err("Vpc message send fail. (hostpid=%d; ret=%d)\n", svm_process->process_id.hostpid, ret);
        return ret;
    }

    *pa_offset = translate_msg.pa_offset;
    return ret;
}

int devmm_clear_translate_pa_addr(u32 dev_id, u32 vfid, u64 va, u64 len, u32 host_pid)
{
    u32 size = (u32)sizeof(struct devmm_vpc_mng_msg_clear_translate_addr);
    struct devmm_vpc_mng_msg_clear_translate_addr clear_msg = {{{0}}};
    int ret;

    clear_msg.head.msg_id = DEVMM_VPC_MNG_CLEAR_TRANSLATE_ADDR;
    clear_msg.head.dev_id = dev_id;
    clear_msg.head.process_id.hostpid = (int)host_pid;

    clear_msg.va = va;
    clear_msg.len = len;

    ret = devmm_vm_vpc_mng_msg_send(&clear_msg, size, size);
    if (ret != 0) {
        devmm_drv_err("Vpc message send fail. (hostpid=%d; ret=%d)\n", host_pid, ret);
    }
    return ret;
}

int devmm_page_fault_h2d_sync(struct devmm_devid svm_id, ka_page_t **pages, unsigned long va, u32 adjust_order,
                              const struct devmm_svm_heap *heap)
{
    struct devmm_vpc_mng_msg_page_fault *page_fault = NULL;
    u32 page_num = (1ul << adjust_order);
    u32 len = (u32)(sizeof(struct devmm_vpc_mng_msg_page_fault) + page_num * sizeof(u64));
    int ret;
    u32 i;

    (void)heap;

    page_fault = (struct devmm_vpc_mng_msg_page_fault *)devmm_kvzalloc(len);
    if (page_fault == NULL) {
        devmm_drv_err("Kvzalloc error. (len=%d; dev_id=%u; va=0x%lx; adjust_order=%u)\n", len,
            svm_id.devid, va, adjust_order);
        return -ENOMEM;
    }

    page_fault->head.msg_id = DEVMM_VPC_MNG_HOST_PAGE_FAULT;
    page_fault->head.dev_id = svm_id.devid;
    page_fault->head.process_id.hostpid = devmm_get_current_pid();
    page_fault->head.extend_num = (u16)page_num;

    page_fault->va = va;
    page_fault->page_size = KA_MM_PAGE_SIZE;
    page_fault->pa_list.pa_num = page_num;

    for (i = 0; i < page_fault->pa_list.pa_num; i++) {
        page_fault->pa_list.palist[i] = ka_mm_page_to_phys(pages[i]);
    }

    ret = devmm_vm_vpc_mng_msg_send(page_fault, len, len);
    if (ret != 0) {
        devmm_drv_err("Vpc management message failed. (dev_id=%u; va=0x%lx; ret=%d)\n", svm_id.devid, va, ret);
    }

    devmm_kvfree(page_fault);
    page_fault = NULL;
    return ret;
}

int devmm_vm_chan_page_fault_d2h_process(struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_page_fault *page_fault = (struct devmm_vpc_mng_msg_page_fault *)proc_info->data;
    ka_pid_t pid = page_fault->head.process_id.hostpid;
    struct devmm_svm_process *svm_proc = NULL;
    struct devmm_svm_heap *heap = NULL;
    u32 len;
    int ret;
#ifndef EMU_ST
    ret = devmm_svm_proc_and_heap_get(&page_fault->head.process_id, page_fault->va, &svm_proc, &heap);
    if (ret != 0) {
        devmm_drv_err("Process exit or heap error. (hostpid=%d; va=0x%lx)\n", pid, page_fault->va);
        return -EINVAL;
    }

    page_fault->pa_list.pa_num = heap->chunk_page_size / KA_MM_PAGE_SIZE;
    page_fault->page_size = KA_MM_PAGE_SIZE;

    len = (u32)(sizeof(struct devmm_vpc_mng_msg_page_fault) + page_fault->pa_list.pa_num * sizeof(u64));
    if ((proc_info->in_data_len < len) || (proc_info->out_data_len < proc_info->in_data_len)) {
        devmm_svm_proc_and_heap_put(svm_proc, heap);
        devmm_drv_err("In_data_len or out_data_len error. (dev_id=%d; hostpid=%d; in_data_len=%d; out_data_len=%d)\n",
            page_fault->head.dev_id, pid, proc_info->in_data_len, proc_info->out_data_len);
        return -EINVAL;
    }
    if (devmm_page_fault_get_va_ref(svm_proc, page_fault->va) != 0) {
        devmm_drv_err("Va don't allow fault by device, va is in operation. (hostpid=%d; va=0x%lx; devid=%u)\n",
            svm_proc->process_id.hostpid, page_fault->va, page_fault->head.dev_id);
        devmm_svm_proc_and_heap_put(svm_proc, heap);
        return -EINVAL;
    }
    devmm_svm_set_mapped_with_heap(svm_proc, page_fault->va, heap->chunk_page_size,
                                   page_fault->head.logical_devid, heap);

    if (page_fault->stage == PAGE_FAULT_STAGE_QUERY) {
        ret = devmm_get_host_pa_list(svm_proc, page_fault->va, &page_fault->pa_list, DEVMM_PIN_PAGES, 0);
        if (ret != 0) {
            devmm_page_fault_put_va_ref(svm_proc, page_fault->va);
            devmm_svm_proc_and_heap_put(svm_proc, heap);
            devmm_drv_err("Get pa list failed. (hostpid=%d; pa_num=%d)\n", pid, page_fault->pa_list.pa_num);
            return ret;
        }
        *proc_info->real_out_len = len;
    } else {
        devmm_unmap_pages(svm_proc, page_fault->va, heap->chunk_page_size / KA_MM_PAGE_SIZE);
        *proc_info->real_out_len = 0;
    }

    devmm_svm_clear_mapped_with_heap(svm_proc, page_fault->va,
        heap->chunk_page_size, DEVMM_INVALID_DEVICE_PHYID, heap);
    devmm_page_fault_put_va_ref(svm_proc, page_fault->va);
    devmm_svm_proc_and_heap_put(svm_proc, heap);
#endif
    return 0;
}

int devmm_msg_to_agent_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    struct devmm_vpc_mng_msg_agent_mem_map msg = {{{0}}};
    u32 len = sizeof(struct devmm_vpc_mng_msg_agent_mem_map);
    int ret;

    msg.head.msg_id = DEVMM_VPC_MNG_AGENT_MEM_MAP;
    msg.head.dev_id = info->devid;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;

    msg.va = info->va;
    msg.size = info->size;
    msg.pg_type = info->pg_type;

    msg.module_id = info->module_id;
    msg.phy_addr_blk_id = info->phy_addr_blk_id;
    msg.offset_pg_num = (u32)info->offset_pg_num;
    msg.phy_addr_blk_pg_num = info->phy_addr_blk_pg_num;

    ret = devmm_vm_vpc_mng_msg_send(&msg, len, len);
#ifndef EMU_ST
    if (ret != 0) {
        return ret;
    }

    return devmm_vm_is_need_return_head_result() ? (int)msg.result : 0;
#else
    return ret;
#endif
}

void devmm_destroy_all_convert_dma_addr(struct devmm_svm_process *svm_proc)
{
    struct devmm_pa_node *pa_node = NULL;
    ka_rb_node_t *node = NULL;
    u32 stamp;

    ka_task_mutex_lock(&svm_proc->host_pa_list.rbtree_mutex);
    node = ka_base_rb_first(&svm_proc->host_pa_list.pa_rbtree);
    stamp = (u32)ka_jiffies;
    while (node != NULL) {
        pa_node = ka_base_rb_entry(node, struct devmm_pa_node, pa_node);
        node = ka_base_rb_next(node);
        devmm_free_all_host_pa_list(pa_node->pa_batch);
        ka_base_rb_erase(&pa_node->pa_node, &svm_proc->host_pa_list.pa_rbtree);
        devmm_kfree_ex(pa_node);
        pa_node = NULL;
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&svm_proc->host_pa_list.rbtree_mutex);
    return;
}

bool devmm_is_same_dev(u32 src_devid, u32 dst_devid)
{
    u32 pm_src_devid, pm_dst_devid;

    if (!devmm_device_is_ready(src_devid)) {
        return false;
    }
    pm_src_devid = svm_vm_dev_info[src_devid].pm_dev_id;

    if (!devmm_device_is_ready(dst_devid)) {
        return false;
    }
    pm_dst_devid = svm_vm_dev_info[dst_devid].pm_dev_id;

    if (pm_src_devid == pm_dst_devid) {
        return true;
    }
    return false;
}

u32 devmm_get_vfid_from_dev_id(struct devmm_memory_attributes *attr)
{
    return svm_vm_dev_info[attr->devid].pm_fid;
}

/* stub */
void devmm_init_dev_pages_cache(struct devmm_svm_process *svm_proc)
{
}

void devmm_destroy_pages_cache(struct devmm_svm_process *svm_proc)
{
}

int devmm_chan_page_fault_d2h_process_dma_copy(struct devmm_chan_page_fault *fault_msg, u64 *pas,
    u32 *szs, u32 num)
{
    return -1;
}

int devmm_p2p_page_create_msg(struct devmm_svm_process *svm_pro, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num)
{
    devmm_drv_err("Not support p2p.\n");
    return -1;
}

int devmm_init_shm_pro_node(struct devmm_svm_process *svm_proc)
{
    return 0;
}

void devmm_unint_shm_pro_node(struct devmm_svm_process *svm_proc)
{
}

void devmm_remote_unmap_and_delete_node(struct devmm_svm_process *svm_proc, struct devmm_shm_node *node)
{
    return;
}

void devmm_delete_shm_pro_node(struct devmm_shm_pro_node *shm_pro_node)
{
    return;
}

struct devmm_shm_node *devmm_get_shm_node_by_dva(struct devmm_shm_head *shm_head,
    u64 dst_va, u64 size, u32 devid, u32 vfid)
{
    return NULL;
}

ka_semaphore_t *devmm_get_shm_sem(struct devmm_svm_process *svm_proc, u32 map_type)
{
    return NULL;
}

struct devmm_shm_pro_node *devmm_get_shm_pro_node(struct devmm_svm_process *svm_proc, u32 map_type)
{
    return NULL;
}

struct devmm_shm_process_head *devmm_get_shm_pro_head(u32 map_type)
{
    return NULL;
}

int devmm_ioctl_mem_remote_unmap(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_mem_remote_map(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_ipc_mem_open(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_ipc_mem_close(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_ipc_mem_create(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_ipc_mem_set_pid(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_ipc_mem_destroy(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_ipc_mem_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

void devmm_ipc_node_init(void)
{
};

void devmm_ipc_node_uninit(void)
{
}

void devmm_destroy_ipc_mem_node_by_proc(struct devmm_svm_process *svm_proc, u32 devid)
{
    return;
}

struct devmm_svm_process *devmm_ipc_query_owner_info(struct devmm_svm_process *svm_proc,
    u64 va, u64 *owner_va, struct devmm_svm_process_id *id, u32 *sdid)
{
    return NULL;
}

int devmm_ipc_query_owner_attr_by_va(struct devmm_svm_process *svm_proc, u64 va, void *node_attr,
    struct devmm_ipc_owner_attr *owner_attr)
{
    return -ENOSPC;
}

void devmm_try_free_ipc_mem(struct devmm_svm_process *svm_proc, u64 vptr, u64 page_num, u32 page_size)
{
}

int devmm_ipc_get_owner_proc_attr(struct devmm_svm_process *svm_proc, struct devmm_memory_attributes *attr,
    struct devmm_svm_process **owner_proc, struct devmm_svm_heap **heap, struct devmm_memory_attributes *owner_attr)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

void devmm_ipc_put_owner_proc_attr(struct devmm_svm_process *owner_proc, struct devmm_svm_heap *heap)
{
}

bool devmm_check_addr_valid(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    return false;
}

int devmm_get_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size,
    u64 *pa_list, u32 pa_num)
{
    return -EOPNOTSUPP;
}

void devmm_put_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size,
    u64 *pa_list, u32 pa_num)
{
    return;
}

u32 devmm_get_mem_page_size(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    return -EOPNOTSUPP;
}

bool devmm_svm_need_ib_register_peer(void)
{
    return false;
}

void devmm_get_sys_mem(void)
{
}

int devmm_ioctl_map_dev_reserve(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

int devmm_ioctl_mem_repair(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
#ifndef EMU_ST
    devmm_drv_run_info("Vm not support.\n");
#endif
    return -EOPNOTSUPP;
}

void devmm_destory_shm_pro_node(struct devmm_svm_process *svm_proc)
{
    return;
}

void devmm_uninit_shm_by_devid(u32 dev_id)
{
    return;
}

void devmm_try_destroy_remote_map_nodes(struct devmm_svm_process *svm_proc, u32 log_dev, u32 phy_dev, u32 vfid)
{
    return;
}

void devmm_destory_remote_map_mem_by_devid(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap * heap, u32 logical_devid)
{
    return;
}

int devdrv_get_connect_protocol(u32 dev_id)
{
    return 0;
}

int devmm_unmap_mem(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    return 0;
}

u32 devdrv_get_dev_chip_type(u32 dev_id)
{
#ifdef CFG_SOC_PLATFORM_MINIV2
    return 2; /* HISI_MINI_V2        2 */
#else
    return 1; /* HISI_CLOUD_V1       1 */
#endif
}

void devmm_wait_task_finish(u32 dev_id, ka_atomic_t *finish_num, int submit_num)
{
    return;
}

void devmm_task_del(struct devmm_dma_copy_task *copy_task)
{
    return;
}

int devmm_shm_config_txatu(struct devmm_svm_process *svm_proc, u32 devid)
{
    return -EOPNOTSUPP;
}

int devmm_locked_host_unmap(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, u64 src_va, u64 size, u64 dst_va)
{
    return -EOPNOTSUPP;
}

int devmm_map_svm_mem_to_agent(struct devmm_svm_process *svm_proc, struct devmm_devid * devids,
    struct devmm_mem_remote_map_para * map_para, struct devmm_memory_attributes * attr)
{
    return -EOPNOTSUPP;
}
int devmm_find_pa_cache(struct devmm_svm_process *svm_process, u32 logic_id, u64 va, u32 page_size, u64 *pa)
{
    return -EOPNOTSUPP;
}

int devmm_remote_pa_to_bar_pa(u32 devid, u64 *remote_pa, u64 pa_num, u64 *bar_pa)
{
    return -EOPNOTSUPP;
}

bool devmm_current_is_vdev(void)
{
    return true;
}

void devmm_notify_ts_drv_to_release(u32 devid, ka_pid_t pid)
{
    vtsdrv_wait_pm_recycle_exit(devid, pid);
    return;
}

int devmm_ioctl_query_process_status(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return 0;
}

int devmm_destroy_dma_addr(struct DMA_ADDR *dma_addr)
{
    return 0;
}

int devmm_copy_one_convert_addr_info(u32 dev_id, u32 index, u32 dir, struct devmm_share_memory_data *data)
{
    return 0;
}

void devmm_clear_one_convert_dma_addr(u32 dev_id, u32 vfid, u32 index)
{
}

int devmm_destroy_addr_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return devmm_destroy_addr_batch_sync(svm_proc, arg);
}

int devmm_dev_res_init(struct devmm_dev_res_mng *dev_res_mng)
{
    return 0;
}

int devdrv_pcimsg_free_non_trans_queue(void *msg_chan)
{
    return 0;
}

int devdrv_set_msg_chan_priv(void *msg_chan, void *priv)
{
    return 0;
}

void devmm_host_dev_uninit(u32 dev_id)
{
}

void devmm_free_raw_dmanode_list(struct devmm_copy_res *res)
{
}

void devmm_free_copy_mem(struct devmm_copy_res *res)
{
}

void devmm_dma_desc_nodes_destroy_by_task_release(struct devmm_svm_process *svm_proc)
{
}

void devmm_uninit_convert_addr_mng(u32 dev_id)
{
}

int devmm_ioctl_ipc_set_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return 0;
}

int devmm_ioctl_ipc_get_attr(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return 0;
}

void devmm_ipc_mem_init(void)
{
}

void devmm_ipc_mem_uninit(void)
{
}

void devmm_dma_desc_stats_info_print(struct devmm_svm_process *svm_proc)
{
}


int devmm_pa_node_list_dma_map(u32 dev_id, struct devmm_copy_side *side)
{
    return 0;
}

int devmm_get_non_svm_addr_pa_list(struct devmm_svm_process *svm_proc, u64 va, u64 size,
    struct devmm_copy_side *side, int write)
{
    return 0;
}

void devmm_pa_node_list_dma_unmap(u32 dev_id, struct devmm_copy_side *side)
{
}

int devmm_svm_addr_pa_list_get(struct devmm_svm_process *svm_proc, u64 va, u64 size,
    struct devmm_copy_side *side)
{
    return 0;
}

void devmm_clear_host_pa_node_list(int pin_flg, struct devmm_copy_side *side)
{
}

void devmm_ipc_node_clean_all_by_dev_res_mng(struct devmm_dev_res_mng *res_mng)
{
}

bool devmm_is_active_reboot_status(void)
{
    return false;
}

void devmm_mem_free_preprocess_by_dev_and_va(struct devmm_svm_process *svm_proc, u32 devid, u64 free_va, u64 free_len)
{
    return;
}

void devmm_mem_free_preprocess_by_dev(struct devmm_svm_process *svm_proc, u32 devid)
{
    return;
}

int hal_kernel_p2p_get_pages(u64 va, u64 len, void (*free_callback)(void *data), void *data,
    struct p2p_page_table **page_table)
{
    return -EOPNOTSUPP;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_p2p_get_pages);

int hal_kernel_p2p_put_pages(struct p2p_page_table *page_table)
{
    return -EOPNOTSUPP;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_p2p_put_pages);

bool devmm_is_mdev_vm_boot_mode(u32 devid)
{
    return false;
}

u64 devmm_get_host_uva_start(void)
{
    return DEVMM_DEFAULT_HOST_UVA_START;
}

#ifndef EMU_ST
int devmm_get_host_phy_mach_flag(u32 devid, u32 *host_flag)
{
    return 0;
}

bool devmm_is_hccs_vm_scene(u32 dev_id, u32 host_mode)
{
    return false;
}

int devmm_vm_pa_blks_to_pm_pa_blks(u32 devid, struct devmm_dma_block *blks, u64 num, struct devmm_dma_block *out_blks)
{
    return 0;
}

void devmm_dma_prepare_pool_init(u32 devid)
{
    return;
}

void devmm_dma_prepare_pool_uninit(u32 devid)
{
    return;
}

void *devmm_dma_prepare_get_from_pool(u32 devid, u32 dma_node_num, struct devdrv_dma_prepare **dma_prepare)
{
    return NULL;
}

void devmm_dma_prepare_put_to_pool(u32 devid, void *fd)
{
    return;
}

int devmm_get_host_run_mode(u32 devid)
{
    return DEVMM_HOST_IS_VIRT;
}

int devdrv_get_bar_wc_flag(u32 devid, u32 *value)
{
    return 0;
}

int devdrv_get_pfvf_type_by_devid(u32 dev_id)
{
    return DEVDRV_SRIOV_TYPE_VF;
}

int devmm_vm_pa_to_pm_pa(u32 devid, u64 *paddr, u64 num, u64 *out_paddr, u64 out_num)
{
    return -EOPNOTSUPP;
}
#endif

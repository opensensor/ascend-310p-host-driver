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

#include "devmm_vpc_mng_msg_def.h"
#include "devmm_pm_vpc.h"
#include "devmm_common.h"
#include "devmm_page_cache.h"
#include "devmm_channel.h"
#include "svm_msg_client.h"
#include "devmm_proc_mem_copy.h"
#include "svm_heap_mng.h"
#include "devmm_dev.h"
#include "svm_proc_mng.h"
#include "svm_master_convert.h"
#include "svm_master_memcpy.h"
#include "svm_task_dev_res_mng.h"
#include "svm_master_mem_map.h"
#include "svm_pm_version_adapt.h"
#include "devmm_pm_adapt.h"

int devmm_pm_init_process(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_init_process *init_proc = (struct devmm_vpc_mng_msg_init_process *)proc_info->data;
    u32 vm_id = init_proc->head.process_id.vm_id;
    ka_pid_t pid = init_proc->head.process_id.hostpid;
    int ret;

    if (devmm_pm_svm_proc_test_and_inc(vm_id) != 0) {
        devmm_drv_err("Pm svm process num exceed. (vm_id=%d; hostpid=%d)\n", vm_id, pid);
        return -ESRCH;
    }
    /* equal to pm mmap process */
    svm_proc = devmm_alloc_svm_proc();
    if (svm_proc == NULL) {
        devmm_pm_svm_proc_sub(vm_id);
        devmm_drv_err("Svm process has no enough resource. (vm_id=%d; hostpid=%d)\n", vm_id, pid);
        return -EFAULT;
    }

    ret = devmm_add_svm_proc_pid_lock(svm_proc, &init_proc->head.process_id, devmm_get_current_pid());
    if (ret != 0) {
        devmm_free_svm_proc(svm_proc);
        devmm_pm_svm_proc_sub(vm_id);
        devmm_drv_err("Set process id to svm_proc fail. (vm_id=%d; hostpid=%d)\n", vm_id, pid);
        return ret;
    }
    devmm_pm_release_work_init(svm_proc);

    /* equal to pm ioctl init process */
    ka_task_mutex_lock(&svm_proc->proc_lock);
    devmm_init_dev_pages_cache_inner(svm_proc);
    devmm_set_svm_proc_state(svm_proc, DEVMM_SVM_INITED_FLAG);
    ka_task_mutex_unlock(&svm_proc->proc_lock);

    (void)devmm_add_to_svm_proc_hashtable(svm_proc);
    devmm_drv_info("Pm init process details. (hostpid=%d; vmid=%d; vfid=%d; devpid=%u; status=%x; proc_idx=%u)\n",
        svm_proc->process_id.hostpid, svm_proc->process_id.vm_id, svm_proc->process_id.vfid,
        svm_proc->devpid, svm_proc->inited, svm_proc->proc_idx);
    *proc_info->real_out_len = 0;

    return 0;
}

STATIC void devmm_pm_convert_delete_res_node(struct devmm_pm_convert_node *convert_node)
{
    struct devmm_pm_copy_res *res_node = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_list_for_each_safe(pos, n, &convert_node->pm_convrt.list_head) {
        res_node = ka_list_entry(pos, struct devmm_pm_copy_res, list);
        if (res_node != NULL && res_node->res != NULL) {
            devmm_clear_pm_host_addr(res_node->res);
            devmm_pm_idle_convert_len_add(res_node->res->vm_id, (u32)res_node->res->dev_id,
                (u32)res_node->res->fid, res_node->res->byte_count);
            devmm_free_copy_mem(res_node->res);
            ka_list_del(&res_node->list);
            devmm_kfree_ex(res_node);
        }
        devmm_try_cond_resched(&stamp);
    }
}

void devmm_pm_increase_convert_free_id(u32 dev_id, u32 fid, u64 height)
{
    ka_base_atomic_add(height, &devmm_svm->convert_ids[dev_id].vdev_free_id_num[fid]);
}

STATIC void devmm_pm_destroy_all_convert_res(struct devmm_svm_process *svm_proc)
{
    struct devmm_pm_convert_node *convert_node = NULL;
    ka_hlist_node_t *tmp = NULL;
    u32 hash_tag, dev_id, fid;
    u32 stamp = (u32)ka_jiffies;

    ka_task_mutex_lock(&svm_proc->convert_res.hlist_mutex);
    ka_hash_for_each_safe(svm_proc->convert_res.hlist, hash_tag, tmp, convert_node, res_hlist)
    {
        dev_id = convert_node->pm_convrt.dev_id;
        fid = convert_node->pm_convrt.fid;
        devmm_pm_convert_delete_res_node(convert_node);
        ka_hash_del(&convert_node->res_hlist);
        devmm_pm_increase_convert_free_id(dev_id, fid, convert_node->pm_convrt.height);
        devmm_kfree_ex(convert_node);
#ifndef EMU_ST
        devmm_try_cond_resched(&stamp);
#endif
    }
    ka_task_mutex_unlock(&svm_proc->convert_res.hlist_mutex);
}

void devmm_pm_release_vm_agent_proc(struct devmm_svm_process *svm_proc)
{
    devmm_pm_destroy_all_convert_res(svm_proc);
    devmm_task_dev_res_nodes_destroy_by_task(svm_proc);
    devmm_destroy_pages_cache_inner(svm_proc);
    devmm_free_proc_priv_data(svm_proc);
    devmm_free_svm_proc(svm_proc);
}

STATIC void devmm_pm_release_work(ka_work_struct_t *work)
{
    struct devmm_svm_process *svm_proc =
        (struct devmm_svm_process *)ka_container_of(work, struct devmm_svm_process, release_work.work);

    devmm_svm_proc_wait_exit(svm_proc);
    (void)devmm_notify_deviceprocess(svm_proc);
    devmm_pm_release_vm_agent_proc(svm_proc);

    return;
}

void devmm_pm_release_work_init(struct devmm_svm_process *svm_proc)
{
    KA_TASK_INIT_DELAYED_WORK(&svm_proc->release_work, devmm_pm_release_work);
}

int devmm_pm_release_process(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_init_process *init_proc = (struct devmm_vpc_mng_msg_init_process *)proc_info->data;
    ka_pid_t pid = init_proc->head.process_id.hostpid;
    u32 vm_id = init_proc->head.process_id.vm_id;

    devmm_task_dev_res_nodes_destroy_by_task(svm_proc);
    (void)ka_task_schedule_delayed_work(&svm_proc->release_work, ka_system_msecs_to_jiffies(0));
    devmm_pm_svm_proc_sub(vm_id);
    *proc_info->real_out_len = 0;
    devmm_drv_info("Pm release process details. (svm_id=%d; pid=%d)\n", vm_id, pid);

    return 0;
}

static bool devmm_page_size_is_invalid(u32 page_size)
{
    return ((page_size != devmm_svm->svm_page_size) && (page_size != devmm_svm->device_hpage_size) &&
        (page_size != devmm_svm->device_page_size));
}

int devmm_pm_create_device_page(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_create_dev_page *create_msg = (struct devmm_vpc_mng_msg_create_dev_page *)proc_info->data;
    struct devmm_page_query_arg query_arg = {{0}};
    u32 vm_id = create_msg->head.process_id.vm_id;
    ka_pid_t pid = create_msg->head.process_id.hostpid;
    int ret;

    if (devmm_page_size_is_invalid(create_msg->page_size) || (create_msg->offset >= create_msg->page_size)) {
        devmm_drv_err("Invalid pagesize or offset. (pagesize=%u; offset=%llu)\n",
                      create_msg->page_size, create_msg->offset);
        return -EINVAL;
    }

    query_arg.process_id.vfid = (u16)(fid);
    query_arg.process_id.hostpid = pid;
    query_arg.va = create_msg->va;
    query_arg.size = create_msg->size;
    query_arg.offset = create_msg->offset;
    query_arg.page_size = create_msg->page_size;
    query_arg.dev_id = create_msg->head.dev_id;
    query_arg.msg_id = DEVMM_CHAN_PAGE_CREATE_H2D_ID;
    query_arg.addr_type = DEVMM_ADDR_TYPE_DMA;
    devmm_page_bitmap_set_devid(&create_msg->bitmap, create_msg->head.dev_id);
    query_arg.bitmap = create_msg->bitmap;
    query_arg.page_insert_dev_id = devmm_get_vm_dev_id(create_msg->head.dev_id, fid);
    create_msg->page_cnt = devmm_get_pagecount_by_size(create_msg->va, create_msg->size, create_msg->page_size);
    ret = devmm_page_create_query_msg(svm_proc, query_arg, NULL, (u32*)&create_msg->page_cnt);
    if (ret != 0) {
        devmm_drv_err("Create query message error. (vm_id=%d; hostpid=%d; ret=%d)\n", vm_id, pid, ret);
        return ret;
    }

    *proc_info->real_out_len = 0;
    return 0;
}

int devmm_pm_del_device_page_cache(struct devmm_svm_process *svm_proc,
    u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_del_dev_page_cache *msg = (struct devmm_vpc_mng_msg_del_dev_page_cache *)proc_info->data;
    u32 vm_dev_id = devmm_get_vm_dev_id(msg->head.dev_id, fid);
    u32 vm_id = msg->head.process_id.vm_id;
    ka_pid_t pid = msg->head.process_id.hostpid;

    switch (msg->type) {
        case DEL_PAGE_CACHE_TYPE_VA:
            if (devmm_page_size_is_invalid(msg->page_size)) {
                devmm_drv_err("Invalid pagesize. (pagesize=%u)\n", msg->page_size);
                return -EINVAL;
            }
            devmm_free_pages_cache_inner(svm_proc, vm_dev_id, msg->page_num, msg->page_size, msg->va, msg->reuse);
            break;
        case DEL_PAGE_CACHE_TYPE_DEV:
            devmm_destroy_dev_pages_cache_inner(svm_proc, vm_dev_id);
            break;
        default:
            devmm_drv_err("Delete type error. (vm_id=%d; hostpid=%d; type=%d)\n", vm_id, pid, msg->type);
            return -EINVAL;
    }

    *proc_info->real_out_len = 0;
    return 0;
}

STATIC void devmm_dma_unmap_vm_pa(u32 dev_id, u32 fid, struct devmm_copy_side *host_side)
{
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    for (i = 0; i < host_side->num; i++) {
        if (host_side->blks[i].pa == 0) {
            break;
        }

        vmngh_dma_unmap_guest_page(dev_id, fid, (ka_sg_table_t *)(uintptr_t)(host_side->blks[i].pa));
        host_side->blks[i].pa = 0;
        devmm_try_cond_resched(&stamp);
    }
}

STATIC int devmm_dma_map_vm_pa(u32 dev_id, u32 fid, struct devmm_pa_list *pa_list, struct devmm_copy_side *host_side)
{
    ka_sg_table_t *sg = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    for (i = 0; i < pa_list->pa_num; i++) {
        ka_dma_addr_t dma_addr = vmngh_dma_map_guest_page(dev_id, fid, (unsigned long)pa_list->palist[i],
                                                       host_side->blk_page_size, &sg);
        if (dma_addr == DMA_MAP_ERROR) {
#ifndef EMU_ST
            devmm_drv_err("Dma map failed. (dev_id=%d; fid=%d; i=%d)\n", dev_id, fid, i);
            devmm_dma_unmap_vm_pa(dev_id, fid, host_side);
            return -ENOMEM;
#endif
        }

        if (sg->nents != 1) {
            devmm_drv_err("Dma map num error. (dev_id=%d; fid=%d; i=%d; nents=%d)\n", dev_id, fid, i, sg->nents);
            devmm_dma_unmap_vm_pa(dev_id, fid, host_side);
            return -EFAULT;
        }

        host_side->blks[i].dma = dma_addr;
        host_side->blks[i].pa = (unsigned long)(uintptr_t)sg; /* save sg in pa pos for latter unmap */
        host_side->blks[i].sz = ka_mm_sg_dma_len(sg->sgl);
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

int devmm_dma_map_vm_pa_batch(u32 dev_id, u32 fid, struct devmm_pa_list *pa_list, struct devmm_copy_side *host_side)
{
    int ret;
    u32 i;

    for (i = 0; i < pa_list->pa_num; i++) {
        host_side->blks[i].sz = (u32)host_side->blk_page_size;
        host_side->blks[i].pa = pa_list->palist[i];
        pa_list->palist[i] = pa_list->palist[i] >> KA_MM_PAGE_SHIFT;
    }
    /* hw_dvt_hypervisor_dma_unmap_guest_page_batch is null, shoud not call unmap */
    ret = vmngh_dma_map_guest_page_batch(dev_id, fid,
        (unsigned long *)pa_list->palist, (unsigned long *)pa_list->palist, pa_list->pa_num);
    if (ret != 0) {
        devmm_drv_err("Dma map failed. (dev_id=%d; fid=%d; i=%d)\n", dev_id, fid, i);
        return ret;
    }

    /* Adding offset in PAGE, in case of PM KA_MM_PAGE_SIZE is larger than VM */
    for (i = 0; i < pa_list->pa_num; i++) {
        host_side->blks[i].dma = pa_list->palist[i] + (host_side->blks[i].pa & (KA_MM_PAGE_SIZE - 1));
    }

    return 0;
}

void devmm_dma_unmap_vm_pa_proc(u32 dev_id, u32 fid, struct devmm_copy_side *host_side)
{
    if (vmngh_dma_pool_active(dev_id, fid) == false) {
        devmm_dma_unmap_vm_pa(dev_id, fid, host_side);
    }
}

int devmm_dma_map_vm_pa_proc(u32 dev_id, u32 fid, struct devmm_pa_list *pa_list, struct devmm_copy_side *host_side)
{
    int ret;

    if (vmngh_dma_pool_active(dev_id, fid) == false) {
        ret = devmm_dma_map_vm_pa(dev_id, fid, pa_list, host_side);
    } else {
        ret = devmm_dma_map_vm_pa_batch(dev_id, fid, pa_list, host_side);
    }
    return ret;
}

STATIC int devmm_pm_devid_convert(u32 vm_id, u32 dir,
    struct devmm_memory_attributes *src, struct devmm_memory_attributes *dst)
{
    if (dir == DEVMM_COPY_DEVICE_TO_HOST) {
        if (!devmm_is_vm_dev_valid(vm_id, src->devid)) {
            return -EINVAL;
        }
        src->devid = devmm_get_dev_id_from_vm_dev_id(vm_id, src->devid);
    } else if (dir == DEVMM_COPY_HOST_TO_DEVICE) {
        if (!devmm_is_vm_dev_valid(vm_id, dst->devid)) {
            return -EINVAL;
        }
        dst->devid = devmm_get_dev_id_from_vm_dev_id(vm_id, dst->devid);
    } else if (dir == DEVMM_COPY_DEVICE_TO_DEVICE) {
        if (!devmm_is_vm_dev_valid(vm_id, src->devid)) {
            return -EINVAL;
        }
        src->devid = devmm_get_dev_id_from_vm_dev_id(vm_id, src->devid);

        if (!devmm_is_vm_dev_valid(vm_id, dst->devid)) {
            return -EINVAL;
        }
        dst->devid = devmm_get_dev_id_from_vm_dev_id(vm_id, dst->devid);
    } else {
        return -EINVAL;
    }

    return 0;
}

STATIC void devmm_pm_attr_vfid_update(struct devmm_memory_attributes *src,
    struct devmm_memory_attributes *dst, u32 vfid)
{
    src->vfid = vfid;
    dst->vfid = vfid;
}

STATIC int devmm_prepare_pm_host_addr(u32 dir, struct devmm_copy_res *res,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr, struct devmm_pa_list *pa_list)
{
    struct devmm_copy_side *host_side = NULL;
    u32 block_num = 0;
    int ret;

    if (dir == DEVMM_COPY_DEVICE_TO_HOST) {
        host_side = &res->to;
        host_side->blk_page_size = dst_attr->host_page_size;
        block_num = res->to.blks_num;
    } else if (dir == DEVMM_COPY_HOST_TO_DEVICE) {
        host_side = &res->from;
        host_side->blk_page_size = src_attr->host_page_size;
        block_num = res->from.blks_num;
    }

    if (host_side != NULL) {
        if (pa_list->pa_num > block_num) {
            devmm_drv_err("Panum invalid. (pa_num=%u; block_num=%u)\n", pa_list->pa_num, block_num);
            return -EINVAL;
        }
        host_side->num = pa_list->pa_num;

        ret = devmm_dma_map_vm_pa_proc((u32)res->dev_id, (u32)res->fid, pa_list, host_side);
        if (ret != 0) {
            return ret;
        }

        /* devmm_merg_pa_by_num */
    }

    return 0;
}

void devmm_clear_pm_host_addr(struct devmm_copy_res *res)
{
    if (res->copy_direction == DEVMM_COPY_DEVICE_TO_HOST) { /* D2H */
        devmm_dma_unmap_vm_pa_proc((u32)res->dev_id, (u32)res->fid, &res->to);
    }
    if (res->copy_direction == DEVMM_COPY_HOST_TO_DEVICE) { /* H2D */
        devmm_dma_unmap_vm_pa_proc((u32)res->dev_id, (u32)res->fid, &res->from);
    }
}


STATIC void devmm_pm_fill_copy_para(struct devmm_mem_copy_convrt_para *copy_para,
    struct devmm_vpc_mem_copy_convrt_para *vpc_copy_para)
{
    copy_para->src = vpc_copy_para->src;
    copy_para->dst = vpc_copy_para->dst;
    copy_para->count = vpc_copy_para->count;
    copy_para->blk_size = vpc_copy_para->blk_size;
    copy_para->direction = vpc_copy_para->direction;
    copy_para->create_msg = vpc_copy_para->create_msg;
}

STATIC void devmm_pm_fill_va_attr(struct devmm_memory_attributes *va_attr,
    struct devmm_vpc_memory_attributes *vpc_va_attr)
{
    va_attr->is_local_host = vpc_va_attr->is_local_host;
    va_attr->is_host_pin = vpc_va_attr->is_host_pin;
    va_attr->is_svm = vpc_va_attr->is_svm;
    va_attr->is_svm_huge = vpc_va_attr->is_svm_huge;
    va_attr->is_svm_host = vpc_va_attr->is_svm_host;
    va_attr->is_svm_device = vpc_va_attr->is_svm_device;
    va_attr->is_svm_non_page = vpc_va_attr->is_svm_non_page;
    va_attr->is_ipc_open = vpc_va_attr->is_ipc_open;
    va_attr->bitmap = vpc_va_attr->bitmap;
    va_attr->logical_devid = vpc_va_attr->logical_devid;
    va_attr->devid = vpc_va_attr->devid;
    va_attr->vfid = vpc_va_attr->vfid;
    va_attr->page_size = vpc_va_attr->page_size;
    /* host_page_size is new addition, if vm version is older than pm, adapt to KA_MM_PAGE_SIZE */
    va_attr->host_page_size = (vpc_va_attr->host_page_size == 0) ?
      KA_MM_PAGE_SIZE : vpc_va_attr->host_page_size;
    va_attr->va = vpc_va_attr->va;
    va_attr->copy_use_va = false; /* vm now not support vpc_va_attr->copy_use_va */
}

int devmm_pm_memcpy_process_res(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_mem_copy *copy_msg = (struct devmm_vpc_mng_msg_mem_copy *)proc_info->data;
    u32 vm_id = copy_msg->head.process_id.vm_id;
    ka_pid_t pid = copy_msg->head.process_id.hostpid;
    struct devmm_mem_copy_convrt_para copy_para = {0};
    struct devmm_memory_attributes src_attr = {0};
    struct devmm_memory_attributes dst_attr = {0};
    u32 dir = copy_msg->para.direction;
    struct devmm_copy_res *res = NULL;
    int ret;

    if (copy_msg->para.count > DEVMM_CONVERT_64M_SIZE) {
        devmm_drv_err("Invalid count. (count=%llu)\n", copy_msg->para.count);
        return -EINVAL;
    }

    if (devmm_page_size_is_invalid(copy_msg->para.blk_size)) {
        devmm_drv_err("Invalid blk_size. (blk_size=%u)\n", copy_msg->para.blk_size);
        return -EINVAL;
    }

    devmm_pm_fill_copy_para(&copy_para, &copy_msg->para);
    devmm_pm_fill_va_attr(&src_attr, &copy_msg->src_attr);
    devmm_pm_fill_va_attr(&dst_attr, &copy_msg->dst_attr);
    ret = devmm_pm_devid_convert(vm_id - 1, dir, &src_attr, &dst_attr);
    if (ret != 0) {
        devmm_drv_err("Device id convert failed. (hostpid=%d; dir=%d; src_devid=%d; dst_devid=%d)\n",
            pid, dir, src_attr.devid, dst_attr.devid);
        return -EFAULT;
    }

    devmm_pm_attr_vfid_update(&src_attr, &dst_attr, fid);

    res = devmm_alloc_copy_res(copy_para.count, &src_attr, &dst_attr, true);
    if (res == NULL) {
        devmm_drv_err("Copy buf devmm_kmalloc_ex fail. (byte_count=%lu)\n", copy_para.count);
        return -ENOMEM;
    }

    res->dev_id = (int)copy_msg->head.dev_id;
    res->vm_copy_flag = true;
    res->vm_id = vm_id;
    res->fid = (int)fid;
    res->svm_pro = svm_proc;

    ret = devmm_prepare_pm_host_addr(dir, res, &src_attr, &dst_attr, &copy_msg->pa_list);
    if (ret != 0) {
        devmm_drv_err("Host address convert failed. (vm_id=%d; hostpid=%d)\n", vm_id, pid);
        devmm_free_copy_mem(res);
        return ret;
    }
    copy_para.res = res;

    ret = devmm_ioctl_memcpy_process(svm_proc, &copy_para, &src_attr, &dst_attr);
    if (ret != 0) {
        devmm_drv_err("Memcpy failed. (vm_id=%d; hostpid=%d)\n", vm_id, pid);
        return ret;
    }

    *proc_info->real_out_len = 0;

    return 0;
}

STATIC int devmm_pm_alloc_convert_node(u32 dev_id, u32 fid, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_copy_res *res, struct devmm_pm_convert_node **alloc_convert_node)
{
    struct devmm_pm_convert_node *convert_node = NULL;
    struct devmm_pm_copy_res *pm_res = NULL;

    convert_node = devmm_kzalloc_ex(sizeof(struct devmm_pm_convert_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (convert_node == NULL) {
        devmm_drv_err("Kzalloc convert_node failed.\n");
        return -ENOMEM;
    }
    KA_INIT_LIST_HEAD(&convert_node->pm_convrt.list_head);

    pm_res = devmm_kzalloc_ex(sizeof(struct devmm_pm_copy_res), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pm_res == NULL) {
        devmm_kfree_ex(convert_node);
        devmm_drv_err("Kzalloc pm_res failed.\n");
        return -ENOMEM;
    }
    pm_res->res = res;
    KA_INIT_LIST_HEAD(&pm_res->list);
    ka_list_add(&pm_res->list, &convert_node->pm_convrt.list_head);

    convert_node->pm_convrt.convert_id = convrt_para->convert_id;
    convert_node->pm_convrt.dev_id = dev_id;
    convert_node->pm_convrt.fid = fid;
    convert_node->pm_convrt.height = 1;
    convert_node->pm_convrt.len = convrt_para->len;
    convert_node->pm_convrt.dma_node_num = res->dma_node_num;
    convert_node->pm_convrt.pm_res = pm_res;
    *alloc_convert_node = convert_node;
    return 0;
}

STATIC int devmm_pm_add_convert_node(struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_copy_res *res, struct devmm_pm_convert_node *convert_node)
{
    struct devmm_pm_copy_res *pm_res = NULL;

    pm_res = devmm_kzalloc_ex(sizeof(struct devmm_pm_copy_res), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pm_res == NULL) {
        devmm_drv_err("Kzalloc pm_res failed.\n");
        return -ENOMEM;
    }
    pm_res->res = res;
    KA_INIT_LIST_HEAD(&pm_res->list);
    ka_list_add_tail(&pm_res->list, &convert_node->pm_convrt.list_head);

    convert_node->pm_convrt.height++;
    convert_node->pm_convrt.dma_node_num += res->dma_node_num;
    return 0;
}

bool devmm_pm_is_convert_id_available(u32 dev_id, u32 fid)
{
    struct devmm_pm_convert_ids *convert_ids = &devmm_svm->convert_ids[dev_id];
    u32 free_convert_id;

    free_convert_id = (u32)ka_base_atomic_read(&convert_ids->vdev_free_id_num[fid]);
    if (free_convert_id == 0) {
        return false;
    }
    return true;
}

STATIC int devmm_pm_add_convert_res_to_list(u32 dev_id, u32 fid, struct devmm_svm_process *svm_pro,
    struct devmm_mem_convrt_addr_para *convrt_para, struct devmm_copy_res *res)
{
    struct devmm_pm_convert_ids *convert_ids = &devmm_svm->convert_ids[dev_id];
    u32 hash_tag = convrt_para->convert_id % DEVMM_CONVERT_RES_HLIST_NUM;
    u32 convert_dma_depth = devmm_get_convert_dma_depth();
    struct devmm_pm_convert_node *convert_node = NULL;
    int ret;

    ka_task_mutex_lock(&svm_pro->convert_res.hlist_mutex);
    ka_hash_for_each_possible(svm_pro->convert_res.hlist, convert_node, res_hlist, hash_tag)
    {
        if (convrt_para->convert_id == convert_node->pm_convrt.convert_id) {
            if (convert_node->pm_convrt.dma_node_num + convrt_para->len / KA_MM_PAGE_SIZE > convert_dma_depth) {
                devmm_drv_err("Dma depth is too large. (convert_id=%llu; dma_node_num=%u; res_dma_node_num=%u)\n",
                    convrt_para->convert_id, convert_node->pm_convrt.dma_node_num, res->dma_node_num);
                ka_task_mutex_unlock(&svm_pro->convert_res.hlist_mutex);
                return -EINVAL;
            }
            if (devmm_pm_is_convert_id_available(dev_id, fid) == false) {
                ka_task_mutex_unlock(&svm_pro->convert_res.hlist_mutex);
                devmm_drv_err("Not enough free convert id. (dev_id=%u; fid=%u)\n", dev_id, fid);
                return -ENOMEM;
            }
            ret = devmm_pm_add_convert_node(convrt_para, res, convert_node);
            if (ret != 0) {
                ka_task_mutex_unlock(&svm_pro->convert_res.hlist_mutex);
                return ret;
            }
            ka_task_mutex_unlock(&svm_pro->convert_res.hlist_mutex);
            ka_base_atomic_dec(&convert_ids->vdev_free_id_num[fid]);
            return 0;
        }
    }

    if (devmm_pm_is_convert_id_available(dev_id, fid) == false) {
        ka_task_mutex_unlock(&svm_pro->convert_res.hlist_mutex);
        devmm_drv_err("Pm not enough free convert id. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -ENOMEM;
    }
    ret = devmm_pm_alloc_convert_node(dev_id, fid, convrt_para, res, &convert_node);
    if (ret != 0) {
        ka_task_mutex_unlock(&svm_pro->convert_res.hlist_mutex);
        return ret;
    }
    ka_hash_add(svm_pro->convert_res.hlist, &convert_node->res_hlist, hash_tag);
    ka_task_mutex_unlock(&svm_pro->convert_res.hlist_mutex);
    ka_base_atomic_dec(&convert_ids->vdev_free_id_num[fid]);
    return 0;
}

STATIC void devmm_pm_free_convert_res(u32 dev_id, u32 fid, struct devmm_pm_convert_node *convert_node)
{
    devmm_pm_convert_delete_res_node(convert_node);
    devmm_pm_increase_convert_free_id(dev_id, fid, convert_node->pm_convrt.height);
    devmm_kfree_ex(convert_node);
}

STATIC void devmm_pm_fill_convrt_para(struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_vpc_mem_convrt_addr_para *vpc_convrt_para, u32 dir)
{
    if (dir == DEVMM_VPC_VM_TO_PM) {
        convrt_para->pSrc = vpc_convrt_para->pSrc;
        convrt_para->pDst = vpc_convrt_para->pDst;
        convrt_para->len = vpc_convrt_para->len;
        convrt_para->direction = vpc_convrt_para->direction;
        convrt_para->dmaAddr = vpc_convrt_para->dmaAddr;
        convrt_para->convert_id = vpc_convrt_para->convert_id;
    } else {
        vpc_convrt_para->dmaAddr = convrt_para->dmaAddr;
        vpc_convrt_para->dma_node_num = convrt_para->dma_node_num;
    }
}

int devmm_pm_convert_addr_proc(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_convert_addr *convert_msg = (struct devmm_vpc_mng_msg_convert_addr *)proc_info->data;
    struct devmm_mem_convrt_addr_para convrt_para = {0};
    struct devmm_memory_attributes src_attr = {0};
    struct devmm_memory_attributes dst_attr = {0};
    u32 dir = (u32)convert_msg->convrt_para.direction;
    u32 vm_id = convert_msg->head.process_id.vm_id;
    ka_pid_t pid = convert_msg->head.process_id.hostpid;
    struct devmm_copy_res *res = NULL;
    int ret;

    if ((convert_msg->convrt_para.len == 0) || (convert_msg->convrt_para.len > DEVMM_CONVERT_64M_SIZE) ||
        devmm_page_size_is_invalid(convert_msg->page_size)) {
        devmm_drv_err("Invalid pagesize or len. (pagesize=%u; len=%llu)\n", convert_msg->page_size,
            convert_msg->convrt_para.len);
        return -EINVAL;
    }

    ret = devmm_pm_idle_convert_len_sub(vm_id, convert_msg->head.dev_id, fid, convert_msg->convrt_para.len);
    if (ret != 0) {
        devmm_drv_err("No idle convert_len. (fid=%u; convert_len=%llu)\n",
            fid, convert_msg->convrt_para.len);
        return ret;
    }

    devmm_pm_fill_convrt_para(&convrt_para, &convert_msg->convrt_para, DEVMM_VPC_VM_TO_PM);
    devmm_pm_fill_va_attr(&src_attr, &convert_msg->src_attr);
    devmm_pm_fill_va_attr(&dst_attr, &convert_msg->dst_attr);
    if (devmm_pm_devid_convert(vm_id - 1, dir, &src_attr, &dst_attr) != 0) {
        devmm_drv_err("Device id convert failed. (hostpid=%d; dir=%d; src_devid=%d; dst_devid=%d)\n",
            pid, dir, src_attr.devid, dst_attr.devid);
        ret = -EFAULT;
        goto idle_convert_len_add;
    }

    devmm_pm_attr_vfid_update(&src_attr, &dst_attr, fid);

    res = devmm_alloc_copy_res((u64)convrt_para.len, &src_attr, &dst_attr, true);
    if (res == NULL) {
        devmm_drv_err("Copy buf devmm_kmalloc_ex fail. (byte_count=%llu)\n", convrt_para.len);
        ret = -ENOMEM;
        goto idle_convert_len_add;
    }

    res->dev_id = (int)convert_msg->head.dev_id;
    res->vm_copy_flag = true;
    res->fid = (int)fid;
    res->svm_pro = svm_proc;
    res->vm_id = vm_id;

    ret = devmm_prepare_pm_host_addr(dir, res, &src_attr, &dst_attr, &convert_msg->pa_list);
    if (ret != 0) {
        devmm_drv_err("Host address convert failed. (vm_id=%d; hostpid=%d)\n", vm_id, pid);
        goto devmm_free_res;
    }

    ret = devmm_convert_addr_process(svm_proc, &convrt_para, &src_attr, &dst_attr, res);
    if (ret != 0) {
        devmm_drv_err("Convert address failed. (ret=%d; vm_id=%d)\n", ret, vm_id);
        goto devmm_free_res;
    }

    /* solve the safety problem, check the res and height */
    ret = devmm_pm_add_convert_res_to_list(convert_msg->head.dev_id, fid, svm_proc, &convrt_para, res);
    if (ret != 0) {
        devmm_clear_pm_host_addr(res);
        devmm_drv_err("Add convert res failed. (ret=%d; vm_id=%u)\n", ret, vm_id);
        goto devmm_free_res;
    }
    devmm_pm_fill_convrt_para(&convrt_para, &convert_msg->convrt_para, DEVMM_VPC_PM_TO_VM);
    *proc_info->real_out_len = sizeof(struct devmm_vpc_mng_msg_convert_addr) - sizeof(struct devmm_pa_list);
    return 0;

devmm_free_res:
    devmm_free_copy_mem(res);
idle_convert_len_add:
    devmm_pm_idle_convert_len_add(vm_id, convert_msg->head.dev_id, fid, (u64)convrt_para.len);
    return ret;
}

STATIC void devmm_pm_make_convert_para(u32 dev_id, u32 fid, struct devmm_pm_convert_node *convert_node,
    struct devmm_ioctl_arg *arg, struct devmm_mem_convrt_addr_para *convrt_para)
{
    struct devmm_pm_copy_res *res_node = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u64 i = 0;

    ka_list_for_each_safe(pos, n, &convert_node->pm_convrt.list_head) {
        res_node = ka_list_entry(pos, struct devmm_pm_copy_res, list);
        if (res_node != NULL && res_node->res != NULL) {
            /* total num of convrt_para is height, total num of res_node is also height */
            convrt_para[i++].dmaAddr.phyAddr.priv = res_node->res;
            ka_list_del(&res_node->list);
            devmm_kfree_ex(res_node);
        }
    }
    arg->data.convrt_para.height = convert_node->pm_convrt.height;
    arg->data.convrt_para.len = convert_node->pm_convrt.len;

    devmm_kfree_ex(convert_node);
}

struct devmm_pm_convert_node *devmm_pm_get_convert_node(struct devmm_svm_process *svm_proc,
    struct devmm_vpc_mng_msg_convert_dma_addr *convert_msg)
{
    u32 hash_tag = convert_msg->convrt_para.convert_id % DEVMM_CONVERT_RES_HLIST_NUM;
    struct devmm_pm_convert_node *convert_node = NULL;
    ka_hlist_node_t *tmp = NULL;

    ka_task_mutex_lock(&svm_proc->convert_res.hlist_mutex);
    ka_hash_for_each_possible_safe(svm_proc->convert_res.hlist, convert_node, tmp, res_hlist, hash_tag)
    {
        if (convert_msg->convrt_para.convert_id == convert_node->pm_convrt.convert_id) {
            ka_hash_del(&convert_node->res_hlist);
            ka_task_mutex_unlock(&svm_proc->convert_res.hlist_mutex);
            return convert_node;
        }
    }
    ka_task_mutex_unlock(&svm_proc->convert_res.hlist_mutex);
    return NULL;
}

STATIC void devmm_pm_fill_convrt_dma_para(struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_vpc_mem_convrt_addr_para *vpc_convrt_para, u32 dir)
{
    if (dir == DEVMM_VPC_VM_TO_PM) {
        convrt_para->pSrc = vpc_convrt_para->pSrc;
        convrt_para->pDst = vpc_convrt_para->pDst;
        convrt_para->spitch = vpc_convrt_para->spitch;
        convrt_para->dpitch = vpc_convrt_para->dpitch;
        convrt_para->fixed_size = vpc_convrt_para->fixed_size;
        convrt_para->direction = vpc_convrt_para->direction;
        convrt_para->dmaAddr = vpc_convrt_para->dmaAddr;
        convrt_para->virt_id = vpc_convrt_para->virt_id;
    } else {
        vpc_convrt_para->dmaAddr = convrt_para->dmaAddr;
    }
}

STATIC int devmm_pm_convert2d_proc(struct devmm_svm_process *svm_proc, u32 fid,
    struct devmm_vpc_mng_msg_convert_dma_addr *convert_msg, struct devmm_pm_convert_node *node)
{
    struct devmm_task_dev_res_node *task_dev_res_node = NULL;
    struct svm_id_inst id_inst;
    u64 spitch = convert_msg->convrt_para.spitch;
    u64 dpitch = convert_msg->convrt_para.dpitch;
    u64 fixed_size = convert_msg->convrt_para.fixed_size;
    enum devmm_copy_direction dir = convert_msg->convrt_para.direction;
    struct devmm_mem_convrt_addr_para *convrt_para = NULL;
    u32 vm_id = convert_msg->head.process_id.vm_id;
    ka_pid_t pid = convert_msg->head.process_id.hostpid;
    u32 dev_id = convert_msg->head.dev_id;
    struct devmm_ioctl_arg arg;
    u64 height, width;
    int ret;

    height = node->pm_convrt.height;
    width = node->pm_convrt.len;
    ret = devmm_check_memcpy2d_input(dir, spitch, dpitch, width, height);
    if (ret != 0) {
        devmm_pm_free_convert_res(dev_id, fid, node);
        devmm_drv_err("Pm_convert check input error.\n");
        return ret;
    }
    if (fixed_size >= width * height) {
        devmm_pm_free_convert_res(dev_id, fid, node);
        devmm_drv_err("Fixed_size should smaller than len*height. (len=%llu; height=%llu)\n", width, height);
        return -EINVAL;
    }

    arg.head.devid = dev_id;
    arg.head.vfid = fid;
    devmm_pm_fill_convrt_dma_para(&arg.data.convrt_para, &convert_msg->convrt_para, DEVMM_VPC_VM_TO_PM);
    convrt_para = devmm_kvzalloc(height * sizeof(struct devmm_mem_convrt_addr_para));
    if (convrt_para == NULL) {
        devmm_pm_free_convert_res(dev_id, fid, node);
        devmm_drv_err("Convrt_para pm devmm_kvzalloc_ex fail. (address_num=%llu; size=%llu)\n", height,
            height * sizeof(struct devmm_mem_convrt_addr_para));
        return -ENOMEM;
    }

    svm_id_inst_pack(&id_inst, dev_id, fid);
    task_dev_res_node = devmm_task_dev_res_node_get_by_task(svm_proc, &id_inst);
    if (task_dev_res_node == NULL) {
        /* first create */
        task_dev_res_node = devmm_task_dev_res_node_create(svm_proc, &id_inst);
        if (task_dev_res_node == NULL) {
            devmm_drv_err("Create task_dev_res_node failed. (devid=%u; vfid=%u)\n", id_inst.devid, id_inst.vfid);
            devmm_pm_free_convert_res(dev_id, fid, node);
            devmm_kvfree(convrt_para);
            return -EINVAL;
        }
    } else {
        devmm_task_dev_res_node_put(task_dev_res_node);
    }

    devmm_pm_make_convert_para(dev_id, fid, node, &arg, convrt_para);
    ret = devmm_convert2d_proc_inner(svm_proc, &arg, convrt_para);
    if (ret != 0) {
        devmm_drv_err("Convert2d proc inner failed. (vm_id=%u; hostpid=%d)\n", vm_id, pid);
    }
    devmm_pm_fill_convrt_dma_para(&arg.data.convrt_para, &convert_msg->convrt_para, DEVMM_VPC_PM_TO_VM);
    devmm_kvfree(convrt_para);
    return ret;
}

int devmm_pm_convert_dma_addr_proc(struct devmm_svm_process *svm_proc, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_convert_dma_addr *convert_msg =
        (struct devmm_vpc_mng_msg_convert_dma_addr *)proc_info->data;
    struct devmm_pm_convert_node *convert_node = NULL;
    u32 vm_id = convert_msg->head.process_id.vm_id;
    ka_pid_t pid = convert_msg->head.process_id.hostpid;
    u32 dev_id = convert_msg->head.dev_id;
    int ret = 0;

    convert_node = devmm_pm_get_convert_node(svm_proc, convert_msg);
    if (convert_node == NULL) {
        devmm_drv_err("Pm find convert_node fail. (vm_id=%u; hostpid=%d)\n", vm_id, pid);
        return -EINVAL;
    }
    if (convert_msg->convrt_para.destroy_flag != 0) {
        devmm_pm_free_convert_res(dev_id, fid, convert_node);
    } else {
        ret = devmm_pm_convert2d_proc(svm_proc, fid, convert_msg, convert_node);
    }

    *proc_info->real_out_len = sizeof(struct devmm_vpc_mng_msg_convert_dma_addr);
    return ret;
}

STATIC void devmm_pm_fill_destroy_para(struct devmm_mem_destroy_addr_para *desty_para,
    struct devmm_vpc_mem_desty_addr_para *vpc_desty_para, u32 dir)
{
    if (dir == DEVMM_VPC_VM_TO_PM) {
        desty_para->dmaAddr = vpc_desty_para->dmaAddr;
    } else {
        vpc_desty_para->pSrc = desty_para->pSrc;
        vpc_desty_para->pDst = desty_para->pDst;
        vpc_desty_para->spitch = desty_para->spitch;
        vpc_desty_para->dpitch = desty_para->dpitch;
        vpc_desty_para->len = desty_para->len;
        vpc_desty_para->height = desty_para->height;
        vpc_desty_para->fixed_size = desty_para->fixed_size;
        vpc_desty_para->dmaAddr = desty_para->dmaAddr;
    }
}

int devmm_pm_destroy_addr_proc(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_destroy_addr *destroy_msg = (struct devmm_vpc_mng_msg_destroy_addr *)proc_info->data;
    u32 vm_id = destroy_msg->head.process_id.vm_id;
    ka_pid_t pid = destroy_msg->head.process_id.hostpid;
    struct devmm_ioctl_arg arg;
    int ret;

    arg.head.devid = destroy_msg->head.dev_id;
    arg.head.vfid = fid;
    arg.data.desty_para.host_pid = pid;

    devmm_pm_fill_destroy_para(&arg.data.desty_para, &destroy_msg->desty_para, DEVMM_VPC_VM_TO_PM);
    ret = devmm_ioctl_destroy_addr_proc_inner(svm_proc, &arg);
    if (ret != 0) {
        devmm_drv_err("Destroy address failed. (vm_id=%d; hostpid=%d; ret=%d)\n", vm_id, pid, ret);
    }
    devmm_pm_fill_destroy_para(&arg.data.desty_para, &destroy_msg->desty_para, DEVMM_VPC_PM_TO_VM);
    *proc_info->real_out_len = sizeof(struct devmm_vpc_mng_msg_destroy_addr);

    return ret;
}

STATIC void devmm_pm_fill_translate_para(struct devmm_translate_info *trans,
    struct devmm_vpc_translate_info *vpc_trans)
{
    trans->va = vpc_trans->va;
    trans->page_size = vpc_trans->page_size;
    trans->is_svm_continuty = 0;
    trans->is_vm_translate = 1;
}

int devmm_pm_translate_addr_proc(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_translate_addr *translate_msg = (struct devmm_vpc_mng_msg_translate_addr *)proc_info->data;
    u32 logical_devid = translate_msg->head.logical_devid;
    u32 vm_id = translate_msg->head.process_id.vm_id;
    ka_pid_t pid = translate_msg->head.process_id.hostpid;
    u32 dev_id = translate_msg->head.dev_id;
    struct devmm_translate_info trans = {0};
    int ret;

    trans.dev_id = dev_id;
    trans.vfid = fid;
    trans.page_insert_dev_id = logical_devid;
    devmm_pm_fill_translate_para(&trans, &translate_msg->trans);
    ret = devmm_set_translate_pa_addr_to_device_inner(svm_proc, trans, &translate_msg->pa_offset);
    if (ret != 0) {
        devmm_drv_err("Translate address failed. (vm_id=%d; hostpid=%d)\n", vm_id, pid);
    }

    *proc_info->real_out_len = sizeof(struct devmm_vpc_mng_msg_translate_addr);

    return ret;
}

int devmm_pm_clear_translate_addr_proc(struct devmm_svm_process *svm_proc,
    u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_clear_translate_addr *clear_msg =
        (struct devmm_vpc_mng_msg_clear_translate_addr *)proc_info->data;
    u32 vm_id = clear_msg->head.process_id.vm_id;
    u32 pid = (u32)clear_msg->head.process_id.hostpid;
    u32 dev_id = clear_msg->head.dev_id;
    int ret;

    ret = devmm_clear_translate_pa_addr_inner(dev_id, fid, clear_msg->va, clear_msg->len, pid);
    if (ret != 0) {
        devmm_drv_err("Clear translate address failed. (vm_id=%d; hostpid=%u)\n", vm_id, pid);
    }

    *proc_info->real_out_len = 0;

    return ret;
}

int devmm_pm_page_fault_h2d_sync(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_page_fault *page_fault = (struct devmm_vpc_mng_msg_page_fault *)proc_info->data;
    struct devmm_chan_page_fault *fault_msg = NULL;
    u32 vm_id = page_fault->head.process_id.vm_id;
    ka_pid_t pid = page_fault->head.process_id.hostpid;
    u32 dev_id = page_fault->head.dev_id;
    struct devmm_copy_side *side = NULL;
    u32 j, idx, len;
    int ret;

    if (page_fault->pa_list.pa_num >= DEVMM_PAGE_NUM_PER_FAULT) {
        devmm_drv_err("Invalid pa_num error. (pa_num=%d)\n", page_fault->pa_list.pa_num);
        return -EINVAL;
    }

    len = (u32)(sizeof(struct devmm_chan_page_fault) + sizeof(struct devmm_copy_side) +
        sizeof(struct devmm_dma_block) * page_fault->pa_list.pa_num);
    fault_msg = (struct devmm_chan_page_fault *)devmm_kvzalloc(len);
    if (fault_msg == NULL) {
        devmm_drv_err("Kvzalloc error. (vm_id=%d; hostpid=%d;  len=%d; dev_id=%u)\n", vm_id, pid, len, dev_id);
        return -ENOMEM;
    }
    side = (struct devmm_copy_side *)(fault_msg + 1);
    side->blks = (struct devmm_dma_block *)(side + 1);

    fault_msg->head.msg_id = DEVMM_CHAN_PAGE_FAULT_H2D_ID;
    fault_msg->head.process_id.hostpid = pid;
    fault_msg->head.dev_id = (u16)dev_id;
    fault_msg->head.process_id.vfid = (u16)fid;
    fault_msg->num = page_fault->pa_list.pa_num;
    fault_msg->va = page_fault->va;

    side->blk_page_size = page_fault->page_size;
    side->num = page_fault->pa_list.pa_num;

    ret = devmm_dma_map_vm_pa(dev_id, fid, &page_fault->pa_list, side);
    if (ret != 0) {
        devmm_kvfree(fault_msg);
        devmm_drv_err("Dma map failed. (hostpid=%d)\n", pid);
        return ret;
    }

    for (idx = 0, j = 0; idx < fault_msg->num; idx++) {
        fault_msg->blks[idx].sz = (u32)side->blk_page_size;
        fault_msg->blks[idx].pa = side->blks[idx].dma;
        devmm_merg_phy_blk(fault_msg->blks, idx, &j);
    }
    fault_msg->num = j;

    /* sync send msg:device todo copy data process */
    ret = devmm_chan_msg_send_inner(fault_msg, sizeof(*fault_msg), 0);
    if (ret != 0) {
        devmm_drv_err("Message send failed. (hostpid=%d)\n", pid);
    }

    devmm_dma_unmap_vm_pa(dev_id, fid, side);
    devmm_kvfree(fault_msg);

    *proc_info->real_out_len = 0;

    return ret;
}

/* vm send to device msg pre post process */
int devmm_pm_enable_device(struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_chan_setup_device *setup = (struct devmm_chan_setup_device *)proc_info->data;
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)proc_info->data;

    if (proc_info->in_data_len < sizeof(struct devmm_chan_setup_device)) {
        devmm_drv_err("Invalid in_data_len. (in_data_len=%u)\n", proc_info->in_data_len);
#ifndef EMU_ST
        return -EINVAL;
#endif
    }

    if (head->dev_id >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Device_id invalid. (dev_id=%d)\n", head->dev_id);
        return -ENODEV;
    }
    devmm_drv_info("Vm setup device. (dev_id=%d; fid=%d; pid=%d; process_id=%d; vm_id=%d, devpid=%d)\n",
        dev_id, fid, head->dev_id, svm_proc->process_id.hostpid, svm_proc->process_id.vm_id, setup->devpid);
    /* use devid in vm, if use dev id in pm,  A VM using the same device virtual machine
    two function blocks will cause devid duplication */
    svm_proc->deviceinfo[head->dev_id].devpid = setup->devpid;
    devmm_set_phyid_devid_to_svm_process(svm_proc, head->dev_id, dev_id);
    devmm_set_vfid_to_svm_process(svm_proc, head->dev_id, fid);
    return 0;
}

int devmm_pm_disable_device(struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_chan_close_device *close = (struct devmm_chan_close_device *)proc_info->data;
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)proc_info->data;

    if (proc_info->in_data_len < sizeof(struct devmm_chan_close_device)) {
        devmm_drv_err("Invalid in_data_len. (in_data_len=%u)\n", proc_info->in_data_len);
        return -EINVAL;
    }

    if (close->head.dev_id >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Device_id invalid. (dev_id=%d)\n", close->head.dev_id);
        return -ENODEV;
    }
    devmm_drv_info("Vm close device. (dev_id=%d; fid=%d; pid=%d; process_id=%d; vm_id=%d)\n",
        dev_id, fid, head->dev_id, svm_proc->process_id.hostpid, svm_proc->process_id.vm_id);
    svm_proc->deviceinfo[close->head.dev_id].devpid = DEVMM_SETUP_INVAL_PID;
    return 0;
}

int devmm_pm_channel_msg_default_pre_proc(struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)proc_info->data;
    devmm_drv_err("Vm not surport. (dev_id=%d; hostpid=%d; msg_id=%d)\n",
        head->dev_id, head->process_id.hostpid, head->msg_id);
    return -EINVAL;
}

/* device send to vm msg proc */
STATIC int devmm_pm_chan_page_fault_d2h_process(u32 dev_id, u32 fid, void *msg)
{
#ifndef EMU_ST
    struct devmm_chan_page_fault *fault_msg = (struct devmm_chan_page_fault *)msg;
    struct devmm_vpc_mng_msg_page_fault *page_fault = NULL;
    struct devmm_copy_side *side = NULL;
    u32 num = DEVMM_PAGE_NUM_PER_FAULT;
    u64 *pas = NULL;
    u32 len, malloc_len;
    u32 *szs = NULL;
    int ret;
    u32 i;

    len = (u32)(sizeof(struct devmm_vpc_mng_msg_page_fault) + num * sizeof(u64));
    malloc_len = len;
    malloc_len += sizeof(struct devmm_copy_side) + sizeof(struct devmm_dma_block) * num;
    malloc_len += sizeof(unsigned long) * num + sizeof(u32) * num;

    page_fault = (struct devmm_vpc_mng_msg_page_fault *)devmm_kvzalloc(malloc_len);
    if (page_fault == NULL) {
        devmm_drv_err("Kvzalloc error. (len=%d)\n", malloc_len);
        return -ENOMEM;
    }

    side = (struct devmm_copy_side *)((u8 *)page_fault + len);
    side->blks = (struct devmm_dma_block *)(side + 1);

    pas = (u64 *)(side->blks + num);
    szs = (u32 *)(pas + num);

    page_fault->head.msg_id = DEVMM_VPC_MNG_DEVICE_PAGE_FAULT;
    page_fault->head.dev_id = devmm_get_vm_dev_id(dev_id, fid);
    page_fault->head.logical_devid = page_fault->head.dev_id;
    page_fault->head.process_id.hostpid = fault_msg->head.process_id.hostpid;
    page_fault->va = fault_msg->va;
    page_fault->stage = PAGE_FAULT_STAGE_QUERY;

    /* query palist from vm */
    ret = devmm_pm_vpc_mng_msg_send(dev_id, fid, page_fault, len, len);
    if (ret != 0) {
        devmm_kvfree(page_fault);
        devmm_drv_err("Message send failed. (hostpid=%d; va=0x%llx)\n",
            fault_msg->head.process_id.hostpid, fault_msg->va);
        return ret;
    }

    if (page_fault->pa_list.pa_num > num) {
        devmm_drv_err("Pa_list_pa_num is invalid. (pa_num=%u)\n", page_fault->pa_list.pa_num);
        devmm_kvfree(page_fault);
        return -EINVAL;
    }
    side->num = page_fault->pa_list.pa_num;
    side->blk_page_size = page_fault->page_size;
    ret = devmm_dma_map_vm_pa(dev_id, fid, &page_fault->pa_list, side);
    if (ret != 0) {
        devmm_kvfree(page_fault);
        devmm_drv_err("Dma map failed. (hostpid=%d)\n", fault_msg->head.process_id.hostpid);
        return ret;
    }

    for (i = 0; i < side->num; i++) {
        pas[i] = side->blks[i].dma;
    }
    num = side->num;
    devmm_merg_pa_by_num(pas, num, KA_MM_PAGE_SIZE, szs, &num);

    ret = devmm_chan_page_fault_d2h_process_dma_copy(fault_msg, pas, szs, num);
    devmm_dma_unmap_vm_pa(dev_id, fid, side);
    if (ret != 0) {
        devmm_kvfree(page_fault);
        devmm_drv_err("Dma unmap failed. (hostpid=%d; va=0x%llx)\n",
            fault_msg->head.process_id.hostpid, fault_msg->va);
        return ret;
    }

    /* notice vm free page */
    page_fault->stage = PAGE_FAULT_STAGE_FREE;
    ret = devmm_pm_vpc_mng_msg_send(dev_id, fid, page_fault, len, len);
    if (ret != 0) {
        devmm_drv_err("Message send failed. (hostpid=%d; va=0x%llx\n",
            fault_msg->head.process_id.hostpid, fault_msg->va);
    }
    devmm_kvfree(page_fault);

    return ret;
#else
    return 0;
#endif
}

static void devmm_pm_agent_vmma_info_pack(struct devmm_vpc_mng_msg_agent_mem_map *para, u32 vfid,
    struct devmm_vmma_info *info)
{
    info->va = para->va;
    info->size = para->size;
    info->pg_type = para->pg_type;
    info->pg_size = (para->pg_type == MEM_HUGE_PAGE_TYPE) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;
    info->pg_num = info->size / info->pg_size;

    info->side = MEM_DEV_SIDE;
    info->logic_devid = para->head.logical_devid;
    info->devid = para->head.dev_id;
    info->vfid = vfid;
    info->page_insert_dev_id = devmm_get_vm_dev_id(para->head.dev_id, vfid);

    info->module_id = para->module_id;
    info->phy_addr_blk_id = para->phy_addr_blk_id;
    info->offset_pg_num = para->offset_pg_num;
    info->phy_addr_blk_pg_num = para->phy_addr_blk_pg_num;
}

int devmm_pm_agent_mem_map(struct devmm_svm_process *svm_proc, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_agent_mem_map *para = (struct devmm_vpc_mng_msg_agent_mem_map *)proc_info->data;
    struct devmm_vmma_info info = {0};
    int ret;

    devmm_pm_agent_vmma_info_pack(para, fid, &info);
    ret = devmm_msg_to_agent_mem_map(svm_proc, &info);
#ifndef EMU_ST
    if (devmm_pm_is_need_return_head_result(devmm_get_vm_id_by_process_vm_id(para->head.process_id.vm_id))) {
        *(proc_info->real_out_len) = (u32)sizeof(struct devmm_vpc_mng_msg_agent_mem_map);
        para->result = (u32)ret;
        ret = 0;
    }
#endif
    return ret;
}

bool devmm_pm_is_vm_scene(void *msg)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 fid = msg_head->process_id.vfid;
    u32 dev_id = msg_head->dev_id;
    if (devmm_is_pm_dev_fid_valid(dev_id, fid) == false) {
        return DEVMM_FALSE;
    }
    return DEVMM_TRUE;
}

int devmm_pm_chan_msg_dispatch(void *msg, u32 in_data_len, u32 out_data_len, u32 *ack_len)
{
    struct devmm_chan_page_query *query = (struct devmm_chan_page_query *)msg;
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    ka_pid_t pid = msg_head->process_id.hostpid;
    u32 fid = msg_head->process_id.vfid;
    u32 dev_id = msg_head->dev_id;
    u32 msg_id = msg_head->msg_id;
    int vm_id = vmngh_ctrl_get_vm_id(dev_id, fid);
    int ret;

    if ((vm_id >= DEVMM_VM_MAX_NUM) || (vm_id < 0)) {
        devmm_drv_err("Vm_id is error. (dev_id=%d; fid=%d; pid=%d; msg_id=%d; vm_id=%d;)\n",
            dev_id, fid, pid, msg_id, vm_id);
        return -ENODEV;
    }

    if (msg_id == DEVMM_CHAN_SHM_GET_PAGES_D2H_ID || msg_id == DEVMM_CHAN_SHM_PUT_PAGES_D2H_ID) {
        devmm_drv_err("Vm not surport. (dev_id=%d; fid=%d; pid=%d; msg_id=%d; vm_id=%d)\n",
            dev_id, fid, pid, msg_id, vm_id);
        return -ENOMSG;
    }

    if (msg_id == DEVMM_CHAN_PAGE_FAULT_D2H_ID) {
        msg_head->vfid = msg_head->process_id.vfid;
        return devmm_pm_chan_page_fault_d2h_process(dev_id, fid, msg);
    }

    /* send to vm */
    msg_head->dev_id = (u16)devmm_get_vm_dev_id(dev_id, fid);
    msg_head->logical_devid = msg_head->dev_id;
    msg_head->process_id.devid = 0;
    msg_head->process_id.vfid = 0;
    ret = devmm_pm_vpc_msg_send(dev_id, fid, msg, in_data_len, out_data_len);
    msg_head->dev_id = (u16)dev_id;
    msg_head->process_id.devid = (u16)dev_id;
    msg_head->process_id.vfid = (u16)fid;
    if (ret != 0 || msg_head->result != 0) {
#ifndef EMU_ST
        devmm_drv_err("Message send failed. (dev_id=%d; fid=%d; pid=%d; msg_id=%d; vm_id=%d; ret=%d; result=%d)\n",
            dev_id, fid, pid, msg_id, vm_id, ret, msg_head->result);
#endif
        return (ret != 0) ? ret : msg_head->result;
    }

    if (msg_id == DEVMM_CHAN_QUERY_VAFLGS_D2H_ID) {
        devmm_page_bitmap_set_devid(&query->bitmap, dev_id);
    }

    *ack_len = out_data_len;
    return 0;
};

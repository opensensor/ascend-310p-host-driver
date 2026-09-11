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
#include <linux/kref.h>

#include "ascend_hal_define.h"

#include "devmm_proc_info.h"
#include "devmm_proc_mem_copy.h"
#include "devmm_common.h"
#include "svm_heap_mng.h"
#include "svm_master_memcpy.h"
#include "svm_master_proc_mng.h"
#include "svm_master_dev_capability.h"
#include "svm_task_dev_res_mng.h"
#include "svm_rbtree.h"
#include "svm_master_addr_ref_ops.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_master_convert.h"

#define DEVMM_DESTROY_BATCH_MAX_NUM  DEVMM_MAX_SHM_DATA_NUM

STATIC u32 devmm_convert_64m = DEVMM_CONVERT_64M_SIZE;
STATIC u32 devmm_convert_128m = DEVMM_CONVERT_128M_SIZE;
STATIC u32 devmm_dma_depth = DEVMM_CONVERT_DMA_DEPTH;
u32 devmm_get_convert_64m_size(void)
{
    return devmm_convert_64m;
}
u32 devmm_get_convert_128m_size(void)
{
    return devmm_convert_128m;
}
u32 devmm_get_convert_dma_depth(void)
{
    return devmm_dma_depth;
}
static u32 devmm_get_convert_max_size(u32 devid, u32 vfid)
{
    if (devmm_is_mdev_vm(devid, vfid)) {
        return DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT * KA_MM_PAGE_SIZE;
    } else {
        return devmm_get_convert_64m_size();
    }
}

static u64 rb_handle_of_convert_node(ka_rb_node_t *node)
{
    struct devmm_convert_node *tmp = ka_base_rb_entry(node, struct devmm_convert_node, task_dev_res_node);
    return (u64)tmp->info.dma_addr.phyAddr.priv;
}

int devmm_convert_node_state_trans(struct devmm_convert_node *node, int src_state, int dst_state)
{
    int ret = -EINVAL;

    ka_task_write_lock(&node->rwlock);
    if (node->state == src_state) {
        node->state = dst_state;
        ret = 0;
    }
    ka_task_write_unlock(&node->rwlock);
    return ret;
}

static void devmm_convert_node_release(ka_kref_t *kref)
{
    struct devmm_convert_node *node = ka_container_of(kref, struct devmm_convert_node, ref);

    devmm_kfree_ex(node);
}

void devmm_convert_node_put(struct devmm_convert_node *node)
{
    ka_base_kref_put(&node->ref, devmm_convert_node_release);
}

static void _devmm_convert_node_get(struct devmm_convert_node *node)
{
    ka_base_kref_get(&node->ref);
}

struct devmm_convert_node *devmm_convert_node_get(struct devmm_task_dev_res_node *task_dev_res, u64 handle)
{
    struct devmm_convert_node_rb_info *rb_info = &task_dev_res->convert_rb_info;
    struct devmm_convert_node *convert_node = NULL;
    ka_rb_node_t *node = NULL;

    ka_task_down_read(&rb_info->rw_sem);
    node = devmm_rb_search(&rb_info->root, handle, rb_handle_of_convert_node);
    if (node != NULL) {
        convert_node = ka_base_rb_entry(node, struct devmm_convert_node, task_dev_res_node);
        _devmm_convert_node_get(convert_node);
    }
    ka_task_up_read(&rb_info->rw_sem);
    return convert_node;
}

struct devmm_convert_node *devmm_convert_node_get_by_task(struct devmm_svm_process *svm_proc, u64 handle)
{
    struct devmm_svm_proc_master *master_info = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_task_dev_res_info *info = &master_info->task_dev_res_info;
    struct devmm_task_dev_res_node *task_dev_res_node = NULL;
    struct devmm_task_dev_res_node *n = NULL;
    struct devmm_convert_node *convert_node = NULL;

    ka_task_down_read(&info->rw_sem);
    ka_list_for_each_entry_safe(task_dev_res_node, n, &info->head, task_node) {
        convert_node = devmm_convert_node_get(task_dev_res_node, handle);
        if (convert_node != NULL) {
            ka_task_up_read(&info->rw_sem);
            return convert_node;
        }
    }
    ka_task_up_read(&info->rw_sem);
    return NULL;
}

int devmm_convert_node_create(struct devmm_svm_process *svm_proc, struct devmm_convert_node_info *info)
{
    struct devmm_convert_node_rb_info *rb_info = NULL;
    struct devmm_convert_node *node = NULL;
    struct svm_id_inst inst;
    int ret;

    node = devmm_kzalloc_ex(sizeof(struct devmm_convert_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        devmm_drv_err("Kzalloc failed.\n");
        return -ENOMEM;
    }

    ka_base_kref_init(&node->ref);
    ka_task_rwlock_init(&node->rwlock);
    node->info = *info;
    node->state = CONVERT_NODE_IDLE;
    KA_BASE_RB_CLEAR_NODE(&node->task_dev_res_node);

    svm_id_inst_pack(&inst, info->id_inst.devid, info->id_inst.vfid);
    node->task_dev_res = devmm_task_dev_res_node_get_by_task(svm_proc, &inst);
    if (node->task_dev_res == NULL) {
        devmm_drv_err("Get task_dev_res_node failed. (devid=%u; vfid=%u)\n", inst.devid, inst.vfid);
        devmm_kfree_ex(node);
        return -EINVAL;
    }

    rb_info = &node->task_dev_res->convert_rb_info;
    ka_task_down_write(&rb_info->rw_sem);
    ret = devmm_rb_insert(&rb_info->root, &node->task_dev_res_node, rb_handle_of_convert_node);
    ka_task_up_write(&rb_info->rw_sem);
    if (ret != 0) {
        devmm_drv_err("Insert fail. (devid=%u; vfid=%u)\n", inst.devid, inst.vfid);
        devmm_task_dev_res_node_put(node->task_dev_res);
        devmm_kfree_ex(node);
    }
    return ret;
}

static bool devmm_convert_node_is_ready_to_destroy(struct devmm_convert_node *node)
{
    struct devmm_share_memory_head *convert_mng = NULL;
    struct devmm_share_memory_data data;
    u32 dev_id = node->info.id_inst.devid;
    u32 vfid = node->info.id_inst.vfid;
    int host_pid = node->info.host_pid;
    u32 index = node->info.index;
    int ret;

    convert_mng = &devmm_svm->pa_info[dev_id];

    if (index >= convert_mng->total_data_num) {
        devmm_drv_debug("Index is out of range. (index=%d; total_data_num=%d)\n", index, convert_mng->total_data_num);
        return false;
    }

    if (convert_mng->share_memory_mng[index].va == 0) {
        devmm_drv_debug("Vaddress is invalid. (host_pid=%d, index=%d)\n", host_pid, index);
        return false;
    }

    ret = devmm_copy_one_convert_addr_info(dev_id, index, CONVERT_INFO_FROM_DEVICE, &data);
    if (ret != 0) {
        devmm_drv_debug("Convert dma address failed. (dev_id=%d; hostpid=%d; ret=%d)\n", dev_id, host_pid, ret);
        return false;
    }

    if (convert_mng->share_memory_mng[index].va != data.va) {
        devmm_drv_debug("Vaddress is not match. (host_pid=%d; index=%d)\n", host_pid, index);
        return false;
    }

    if (data.host_pid != host_pid) {
        devmm_drv_debug("Host pid is not match. (host_pid=%d; curr_pid=%d)\n", data.host_pid, host_pid);
        return false;
    }

    if (data.vfid != vfid) {
        devmm_drv_debug("Vfid is not match. (vfid=%d; curr_vfid=%d)\n", data.vfid, vfid);
        return false;
    }

    if (data.image_word == DEVMM_READED_MAGIC_WORD) {
        devmm_drv_debug("Do not finish image word, GE and RTS should call unbind_stream before rtfree. "
            "(host_pid=%d; index=%d; image_word=%x)\n", host_pid, index, data.image_word);
        return false;
    }
    if (sizeof(struct DMA_ADDR) > DEVMM_DATA_SIZE) {
        devmm_drv_debug("Data size less than dma_addr_size. (data_size=%u; dma_addr_size=%lu)\n",
            DEVMM_DATA_SIZE, sizeof(struct DMA_ADDR));
        return false;
    }

    return true;
}

static void devmm_convert_node_subres_recycle(struct devmm_convert_node *node)
{
    if (devmm_dev_capability_convert_support_offset(node->info.id_inst.devid)) {
        devmm_clear_one_convert_dma_addr(node->info.id_inst.devid, node->info.id_inst.vfid, node->info.index);
    }
}

static void devmm_convert_node_erase(struct devmm_convert_node *node)
{
    struct devmm_convert_node_rb_info *rb_info = &node->task_dev_res->convert_rb_info;

    ka_task_down_write(&rb_info->rw_sem);
    (void)devmm_rb_erase(&rb_info->root, &node->task_dev_res_node);
    ka_task_up_write(&rb_info->rw_sem);

    devmm_task_dev_res_node_put(node->task_dev_res);

    (void)devmm_destroy_dma_addr(&node->info.dma_addr);

    ka_base_kref_put(&node->ref, devmm_convert_node_release);
}

static void _devmm_convert_node_destroy(struct devmm_convert_node *node)
{
    devmm_convert_node_subres_recycle(node);
    devmm_convert_node_erase(node);
}

int devmm_convert_node_destroy(struct devmm_convert_node *node, int state, bool check_ts_finish)
{
    int ret;

    if (devmm_dev_capability_convert_support_offset(node->info.id_inst.devid)) {
        if ((check_ts_finish == true) && (devmm_convert_node_is_ready_to_destroy(node) == false)) {
            return -EBUSY;
        }
    }

    ret = devmm_convert_node_state_trans(node, state, CONVERT_NODE_FREEING);
    if (ret != 0) {
        devmm_drv_debug("State trans fail.\n");
        return ret;
    }

    _devmm_convert_node_destroy(node);
    return 0;
}

static void _devmm_convert_node_destroy_by_task_release(struct devmm_convert_node *node)
{
    int ret;

    ret = devmm_convert_node_state_trans(node, CONVERT_NODE_IDLE, CONVERT_NODE_FREEING);
    if (ret == 0) {
        _devmm_convert_node_destroy(node);
        return;
    }

    ret = devmm_convert_node_state_trans(node, CONVERT_NODE_COPYING, CONVERT_NODE_FREEING);
    if (ret == 0) {
        _devmm_convert_node_destroy(node);
        return;
    }

    ret = devmm_convert_node_state_trans(node, CONVERT_NODE_WAITING, CONVERT_NODE_FREEING);
    if (ret == 0) {
        _devmm_convert_node_destroy(node);
        return;
    }

    ret = devmm_convert_node_state_trans(node, CONVERT_NODE_PREPARE_FREE, CONVERT_NODE_FREEING);
    if (ret == 0) {
        /* Corresponds to devmm_destroy_addr_batch->devmm_convert_nodes_get_by_batch_para */
        devmm_convert_node_put(node);
        _devmm_convert_node_destroy(node);
        return;
    }

    ret = devmm_convert_node_state_trans(node, CONVERT_NODE_SUBRES_RECYCLED, CONVERT_NODE_FREEING);
    if (ret == 0) {
        /* Corresponds to devmm_destroy_addr_batch->devmm_convert_nodes_get_by_batch_para */
        devmm_convert_node_put(node);
        /* Node's resource has already been recycled */
        devmm_convert_node_erase(node);
    }

    return;
}

static struct devmm_convert_node *devmm_erase_one_convert_node(struct devmm_task_dev_res_node *task_dev_res,
    rb_erase_condition condition)
{
    struct devmm_convert_node_rb_info *rb_info = &task_dev_res->convert_rb_info;
    ka_rb_node_t *node = NULL;

    ka_task_down_write(&rb_info->rw_sem);
    node = devmm_rb_erase_one_node(&rb_info->root, condition);
    ka_task_up_write(&rb_info->rw_sem);

    return ((node == NULL) ? NULL : ka_base_rb_entry(node, struct devmm_convert_node, task_dev_res_node));
}

void devmm_convert_nodes_destroy_by_task_release(struct devmm_task_dev_res_node *task_dev_res, u32 *num)
{
    struct devmm_convert_node *node = NULL;
    u32 stamp = (u32)ka_jiffies;

    *num = 0;
    while (1) {
        node = devmm_erase_one_convert_node(task_dev_res, NULL);
        if (node == NULL) {
            return;
        }

        (*num)++;
        _devmm_convert_node_destroy_by_task_release(node);
        devmm_try_cond_resched(&stamp);
    }
}

static bool convert_node_recycle_condition(struct devmm_convert_node *node)
{
    bool is_ready_to_destroy = true;
    int ret;

    if (devmm_dev_capability_convert_support_offset(node->info.id_inst.devid)) {
        is_ready_to_destroy = devmm_convert_node_is_ready_to_destroy(node);
    }

    if (is_ready_to_destroy) {
        ret = devmm_convert_node_state_trans(node, CONVERT_NODE_PREPARE_FREE, CONVERT_NODE_SUBRES_RECYCLED);
        if (ret == 0) {
            /* Not put, or the work will access freed mem */
            return true;
        }
    }

    return false;
}

static void devmm_covnert_nodes_subres_recycle_by_task_dev_res(struct devmm_task_dev_res_node *task_dev_res, u32 *num)
{
    struct devmm_convert_node_rb_info *rb_info = &task_dev_res->convert_rb_info;
    struct devmm_convert_node *pos = NULL;
    struct devmm_convert_node *tmp = NULL;
    u32 max_recycle_num = 100;  /* max recycle num is 100 */

    *num = 0;
    ka_task_down_read(&rb_info->rw_sem);
    rbtree_postorder_for_each_entry_safe(pos, tmp, &rb_info->root, task_dev_res_node) {
        if (convert_node_recycle_condition(pos) == true) {
            /* Only recycle node subres, not put node, or the worker will access freed mem */
            devmm_convert_node_subres_recycle(pos);
            (*num)++;

            if (*num == max_recycle_num) {
                break;
            }
        }
    }
    ka_task_up_read(&rb_info->rw_sem);
}

void devmm_convert_nodes_subres_recycle_by_dev_res_mng(struct devmm_dev_res_mng *mng, u32 *num)
{
    struct devmm_task_dev_res_info *info = &mng->task_dev_res_info;
    struct devmm_task_dev_res_node *node = NULL;
    ka_list_head_t *head = &info->head;
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;
    u32 stamp = (u32)ka_jiffies;

    *num = 0;
    ka_task_down_read(&info->rw_sem);
    ka_list_for_each_safe(pos, n, head) {
        node = ka_list_entry(pos, struct devmm_task_dev_res_node, dev_res_node);
        devmm_covnert_nodes_subres_recycle_by_task_dev_res(node, num);
        if (*num != 0) {
            devmm_drv_debug("Convert nodes destroyed by dev_res_mng. (hostpid=%d; devid=%u; vfid=%u; destroy_num=%u)\n",
                node->host_pid, node->id_inst.devid, node->id_inst.vfid, *num);
            break;
        }

        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_read(&info->rw_sem);
}

static int devmm_convert_check_addr_attr(struct devmm_memory_attributes *src_attr,
    struct devmm_memory_attributes *dst_attr)
{
    if (src_attr->is_svm_non_page) {
        devmm_drv_err("Src addr have no data, should access first. (src_va=0x%llx)\n", src_attr->va);
        return -EINVAL;
    }

    if ((src_attr->is_svm_device == false) && (dst_attr->is_svm_device == false)) {
        devmm_drv_err("Src and dst are both no device addr. (src_va=0x%llx; dst_va=0x%llx)\n",
            src_attr->va, dst_attr->va);
        return -EINVAL;
    }

    if ((src_attr->is_svm_device) && (dst_attr->is_svm_device) && (src_attr->devid == dst_attr->devid)) {
        devmm_drv_err("Src and dst are same device. (src_va=0x%llx; dst_va=0x%llx)\n",
            src_attr->va, dst_attr->va);
        return -EINVAL;
    }

    if (src_attr->is_svm_readonly || dst_attr->is_svm_readonly) {
        devmm_drv_err("Va is readonly, not allowed convert. (src_va=0x%llx; dst_va=0x%llx)\n",
            src_attr->va, dst_attr->va);
        return -EINVAL;
    }

    return 0;
}

static int devmm_convert_pre_process(struct devmm_svm_process *svm_proc,
    struct devmm_mem_convrt_addr_para *para, struct devmm_memory_attributes *src_attr,
    struct devmm_memory_attributes *dst_attr)
{
    enum devmm_copy_direction dir;
    int ret;

    ret = devmm_get_memory_attributes(svm_proc, para->pSrc, src_attr);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_get_memory_attributes(svm_proc, para->pDst, dst_attr);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_convert_check_addr_attr(src_attr, dst_attr);
    if (ret != 0) {
        return ret;
    }

    if (dst_attr->is_svm_non_page) {
        ret = devmm_insert_host_page_range(svm_proc, para->pDst, para->len, dst_attr);
        if (ret != 0) {
            devmm_drv_err("Insert host range failed. (src=0x%llx; dst=0x%llx; len=%llu)\n",
                para->pSrc, para->pDst, para->len);
            return ret;
        }
        dst_attr->is_svm_host = 1;
        dst_attr->is_svm_non_page = 0;
    }

    devmm_find_memcpy_dir(&dir, src_attr, dst_attr);
    if (dir == DEVMM_COPY_INVILED_DIRECTION) {
        devmm_drv_err("Invalid dir. (src=0x%llx; dst=0x%llx; len=%llu)\n", para->pSrc, para->pDst, para->len);
        return -EINVAL;
    }
    /* for drvMemConvertAddr to get the real dircection */
    para->direction = (para->direction >= DEVMM_COPY_INVILED_DIRECTION) ? dir : para->direction;
    /* p2p copy, should trans dst addr to host bar, not support va copy */
    dst_attr->copy_use_va = (para->direction == DEVMM_COPY_DEVICE_TO_DEVICE) ? false : dst_attr->copy_use_va;

    return devmm_check_va_direction(para->direction, dir, DEVMM_CPY_CONVERT_MODE, false, dst_attr);
}

int devmm_convert_one_addr(struct devmm_svm_process *svm_pro, struct devmm_mem_convrt_addr_para *convrt_para)
{
    struct devmm_memory_attributes src_attr;
    struct devmm_memory_attributes dst_attr;
    int ret;

    devmm_drv_debug("Convert one line of addresses. (dst=0x%llx; src=0x%llx; byte_count=%llu; direction=%d)\n",
        convrt_para->pDst, convrt_para->pSrc, convrt_para->len, convrt_para->direction);

    ret = devmm_convert_pre_process(svm_pro, convrt_para, &src_attr, &dst_attr);
    if (ret != 0) {
        devmm_drv_err("Convert pre proc failed. (ret=%d)\n", ret);
        return ret;
    }

    return devmm_ioctl_convert_addr_proc(svm_pro, convrt_para, &src_attr, &dst_attr);
}

void devmm_destroy_one_addr(struct devmm_copy_res *res)
{
    devmm_free_raw_dmanode_list(res);
    devmm_free_copy_mem(res);
}

STATIC int devmm_convert2d_fill_convrt_para(struct devmm_svm_process *svm_pro,
    struct devmm_ioctl_arg *arg, struct devmm_mem_convrt_addr_para *convrt_para)
{
    struct devmm_mem_convrt_addr_para *convrt2d_para = &arg->data.convrt_para;
    u32 convert_dma_depth = devmm_get_convert_dma_depth();
    u64 len_node_num, src_begin_va, dst_begin_va, i;
    u64 src = convrt2d_para->pSrc;
    u64 dst = convrt2d_para->pDst;
    u64 spitch = convrt2d_para->spitch;
    u64 dpitch = convrt2d_para->dpitch;
    u64 len = convrt2d_para->len;
    u64 height = convrt2d_para->height;
    u64 fixed_size = convrt2d_para->fixed_size;
    enum devmm_copy_direction dir = convrt2d_para->direction;
    u32 stamp = (u32)ka_jiffies;
    u32 dma_node_num = 0;
    u64 convrt_para_id;
    int ret = 0;

    src_begin_va = src;
    dst_begin_va = dst;
    len_node_num = len / KA_MM_PAGE_SIZE;
    convrt_para_id = (u64)ka_base_atomic64_inc_return(&svm_pro->convert_res.convert_id);
    for (i = 0; i < height; i++) {
        convrt_para[i].pSrc = src_begin_va;
        convrt_para[i].pDst = dst_begin_va;
        convrt_para[i].len = len;
        convrt_para[i].direction = dir;
        convrt_para[i].convert_id = convrt_para_id;
        /* adapt asan, if need_write is false, it may cause data consistency issues when addr has not been visited */
        convrt_para[i].need_write = devmm_svm_mem_is_enable(svm_pro) ? false : true;
        ret = devmm_convert_one_addr(svm_pro, &convrt_para[i]);
        if (ret != 0) {
            convrt2d_para->height = i;
            if (i != 0) {
                convrt_para[0].destroy_flag = 1;
                devmm_convert2d_proc(svm_pro, arg, convrt_para);
            }
            devmm_drv_err_if((ret != -EOPNOTSUPP), "Fill_convrt error. (dst=0x%llx; src=0x%llx; dpitch=%llu; "
                "spitch=%llu; len=%llu; height=%llu; direction=%u; fixed_size=%llu; current_dst=0x%llx; "
                "current_src=0x%llx; total_addr=%llu; current_addr=%llu)\n", dst, src, dpitch, spitch, len, height,
                dir, fixed_size, dst_begin_va, src_begin_va, height, i);
            break;
        }
        src_begin_va += spitch;
        dst_begin_va += dpitch;
        dma_node_num += convrt_para[i].dma_node_num;
        if (dma_node_num + len_node_num > convert_dma_depth) {
            convrt2d_para->height = i + 1;
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    return ret;
}

static int devmm_convert2d_addr(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    u64 src = arg->data.convrt_para.pSrc;
    u64 dst = arg->data.convrt_para.pDst;
    u64 spitch = arg->data.convrt_para.spitch;
    u64 dpitch = arg->data.convrt_para.dpitch;
    u64 len = arg->data.convrt_para.len;
    u64 height = arg->data.convrt_para.height;
    u64 fixed_size = arg->data.convrt_para.fixed_size;
    enum devmm_copy_direction dir = arg->data.convrt_para.direction;
    struct devmm_mem_convrt_addr_para convrt_para_one_height = {0};
    struct devmm_mem_convrt_addr_para *convrt_para = NULL;
    int ret;

    devmm_drv_debug("Convert2d address. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; len=%llu; "
        "height=%llu; fixed_size=%llu; direction=%u)\n", dst, src, dpitch, spitch, len, height,
        fixed_size, dir);

    if (height == 1) {
        convrt_para = &convrt_para_one_height;
    } else {
        convrt_para = devmm_kvzalloc(height * sizeof(struct devmm_mem_convrt_addr_para));
        if (convrt_para == NULL) {
            devmm_drv_err("Convrt_para devmm_kvzalloc_ex failed. (address_num=%llu; size=%llu)\n", height,
                height * sizeof(struct devmm_mem_convrt_addr_para));
            return -ENOMEM;
        }
    }

    ret = devmm_convert2d_fill_convrt_para(svm_pro, arg, convrt_para);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Convrt2d fill_convrt failed. (address_num=%llu)\n", height);
        goto devmm_free_convrt;
    }

    ret = devmm_convert2d_proc(svm_pro, arg, convrt_para);
devmm_free_convrt:
    if (height != 1 && convrt_para != NULL) {
        devmm_kvfree(convrt_para);
    }
    return ret;
}

STATIC int devmm_make_convrt2d_input(struct devmm_mem_convrt_addr_para *para, struct devmm_devid devids)
{
    u32 one_addr_limit_size = devmm_get_convert_limit_size(devids.devid, devids.vfid);
    u32 convert_max_size = devmm_get_convert_max_size(devids.devid, devids.vfid);
    u64 finished_addr_cnt, width_offset, width_left;
    u64 spitch = para->spitch;
    u64 dpitch = para->dpitch;
    u64 width = para->len;
    u64 height = para->height;
    u64 fixed_size = para->fixed_size;

    if (width == 0) {
        devmm_drv_err("Convert width is zero.\n");
        return -EINVAL;
    }

    finished_addr_cnt = fixed_size / width;
    width_offset = fixed_size % width;
    width_left = width - width_offset;
    para->pSrc = para->pSrc + finished_addr_cnt * spitch + width_offset;
    para->pDst = para->pDst + finished_addr_cnt * dpitch + width_offset;
    para->fixed_size = 0;
    if (width <= one_addr_limit_size) {
        if (width_offset != 0) {
            para->len = width_left;
            para->height = 1;
        } else {
            para->height = height - finished_addr_cnt;
        }
    } else {
        if (width_left < one_addr_limit_size) {
            para->len = width_left;
            para->height = 1;
        } else {
            if (convert_max_size == 0) {
                devmm_drv_err("Convert 64m size is zero.\n");
                return -EINVAL;
            }
            para->len = convert_max_size;
            para->height = width_left / convert_max_size;
            para->spitch = para->len;
            para->dpitch = para->len;
        }
    }
    if (para->height > DEVMM_CONVERT_DMA_DEPTH) {
        para->height = DEVMM_CONVERT_DMA_DEPTH;
    }
    return 0;
}

static int devmm_ioctl_convert_para_check(struct devmm_mem_convrt_addr_para *para)
{
    int ret;

    ret = devmm_check_memcpy2d_input(para->direction, para->spitch, para->dpitch, para->len, para->height);
    if (ret != 0) {
        return ret;
    }
    if (para->height > DEVMM_CONVERT2D_HEIGHT_MAX) {
        devmm_drv_err("Height is larger than 5M. (height=%llu)\n", para->height);
        return -EINVAL;
    }
    if (para->fixed_size >= (para->len * para->height)) {
        devmm_drv_err("Fixed_size should smaller than len*height. (fixed_size=%llu; len=%llu; height=%llu)\n",
            para->fixed_size, para->len, para->height);
        return -EINVAL;
    }

    return 0;
}

static int devmm_convert_vmmas_occupy_inc(struct devmm_svm_process *svm_proc,
    u64 va, u64 pitch, u64 width, u64 height)
{
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i, j;
    int ret;

    if (devmm_va_is_in_svm_range(va) == false) {
        return 0;
    }

    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Invalid addr. (va0x%llx)\n", va);
        return -EINVAL;
    }

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        return 0;
    }

    for (i = 0; i < height; i++) {
        ret = devmm_vmmas_occupy_inc(&heap->vmma_mng, va + pitch * i, width);
        if (ret != 0) {
            devmm_drv_err("Vmmas occupy inc failed. (va=0x%llx; size=%llu)\n", va + pitch * i, width);
            goto vmmas_occupy_dec;
        }
        devmm_try_cond_resched(&stamp);
    }
    return 0;
vmmas_occupy_dec:
    for (j = 0; j < i; j++) {
        devmm_vmmas_occupy_dec(&heap->vmma_mng, va + pitch * j, width);
        devmm_try_cond_resched(&stamp);
    }
    return -EINVAL;
}

static void devmm_convert_vmmas_occupy_dec(struct devmm_svm_process *svm_proc,
    u64 va, u64 pitch, u64 width, u64 height)
{
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    if (devmm_va_is_in_svm_range(va) == false) {
        return;
    }

    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        return;
    }

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        return;
    }

    for (i = 0; i < height; i++) {
        devmm_vmmas_occupy_dec(&heap->vmma_mng, va + pitch * i, width);
        devmm_try_cond_resched(&stamp);
    }
}

static void devmm_convert_dec_page_ref(struct devmm_svm_process *svm_proc, u64 va, u64 len, bool is_ioctl_convert)
{
    struct devmm_svm_heap *heap = devmm_svm_get_heap(svm_proc, va);
    if ((devmm_check_heap_is_entity(heap) == false)) {
        return;
    }

    if (heap->heap_sub_type == SUB_HOST_TYPE) {
        /*
         * for heap_sub_type is SUB_HOST_TYPE, ioctl_convert should dec ref, and ioctl_destroy_addr not need dec ref
         * because for va is SUB_HOST_TYPE, ioctl_convert_addr-> ioctl_close_dev -> ioctl_free_pages should succeed.
         */
        if (is_ioctl_convert) {
            devmm_dec_page_ref(svm_proc, va, len);
        }
    } else {
        /* for heap_sub_type is not SUB_HOST_TYPE, ioctl_destroy_addr should dec page ref */
        if (is_ioctl_convert == false) {
            devmm_dec_page_ref(svm_proc, va, len);
        }
    }
}

int devmm_ioctl_convert_addr(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_convrt_addr_para *para = &arg->data.convrt_para;
    u64 expected_height, actual_height;
    int ret;

    ret = devmm_ioctl_convert_para_check(para);
    if (ret != 0) {
        return ret;
    }

    /* Convert width, spitch, dpitch... may change */
    ret = devmm_make_convrt2d_input(para, arg->head);
    if (ret != 0) {
        return ret;
    }

    expected_height = para->height;
    ret = devmm_convert_vmmas_occupy_inc(svm_pro, para->pSrc, para->spitch, para->len, expected_height);
    if (ret != 0) {
        return ret;
    }
    ret = devmm_convert_vmmas_occupy_inc(svm_pro, para->pDst, para->dpitch, para->len, expected_height);
    if (ret != 0) {
        devmm_convert_vmmas_occupy_dec(svm_pro, para->pSrc, para->spitch, para->len, expected_height);
        return ret;
    }

    ret = devmm_convert2d_addr(svm_pro, arg);
    if (ret != 0) {
        devmm_convert_vmmas_occupy_dec(svm_pro, para->pDst, para->dpitch, para->len, expected_height);
        devmm_convert_vmmas_occupy_dec(svm_pro, para->pSrc, para->spitch, para->len, expected_height);
        return ret;
    }

    /* Actual convert height may smaller than expected height. */
    actual_height = para->height;
    devmm_convert_vmmas_occupy_dec(svm_pro, para->pSrc + para->spitch * actual_height,
        para->spitch, para->len, expected_height - actual_height);
    devmm_convert_vmmas_occupy_dec(svm_pro, para->pDst + para->dpitch * actual_height,
        para->dpitch, para->len, expected_height - actual_height);

    devmm_convert_dec_page_ref(svm_pro, para->pSrc, SVM_ADDR_REF_OPS_UNKNOWN_SIZE, true);
    devmm_convert_dec_page_ref(svm_pro, para->pDst, SVM_ADDR_REF_OPS_UNKNOWN_SIZE, true);

    devmm_drv_debug("Convert address. (devid=%u; vfid=%u; dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; "
        "len=%llu; height=%llu; fixed_size=%llu; direction=%u; converted_fixed_size=%u; offset=%llu)\n",
        arg->head.devid, arg->head.vfid, para->pDst, para->pSrc, para->dpitch, para->spitch, para->len,
        para->height, para->fixed_size, para->direction, para->dmaAddr.fixed_size, para->dmaAddr.offsetAddr.offset);

    return 0;
}

int devmm_ioctl_destroy_addr(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_destroy_addr_para *para = &arg->data.desty_para;
    int ret;

    arg->data.desty_para.len = 0;
    arg->data.desty_para.host_pid = svm_pro->process_id.hostpid;
    ret = devmm_ioctl_destroy_addr_proc(svm_pro, arg);
    if (ret != 0) {
        devmm_drv_err("Destroy failed. (ret=%d; devid=%d)\n", ret, arg->head.devid);
        return ret;
    }

    devmm_convert_vmmas_occupy_dec(svm_pro, para->pSrc, para->spitch, para->len, para->height);
    devmm_convert_vmmas_occupy_dec(svm_pro, para->pDst, para->dpitch, para->len, para->height);

    devmm_convert_dec_page_ref(svm_pro, para->pSrc, SVM_ADDR_REF_OPS_UNKNOWN_SIZE, false);
    devmm_convert_dec_page_ref(svm_pro, para->pDst, SVM_ADDR_REF_OPS_UNKNOWN_SIZE, false);

    return 0;
}

static inline void devmm_convert_nodes_put(struct devmm_convert_node **node, u32 num)
{
    int i;

    for (i = 0; i < num; i++) {
        devmm_convert_node_put(node[i]);
    }
}

static inline void devmm_convert_nodes_get(struct devmm_convert_node **node, u32 num)
{
    int i;

    for (i = 0; i < num; i++) {
        _devmm_convert_node_get(node[i]);
    }
}

static int devmm_convert_nodes_state_trans(struct devmm_convert_node **node,
    u32 num, int src_state, int dst_state)
{
    int ret, i, j;

    for (i = 0; i < num; i++) {
        ret = devmm_convert_node_state_trans(node[i], src_state, dst_state);
        if (ret != 0) {
            goto reset_state;
        }
    }

    return 0;
reset_state:
    for (j = 0; j < i; j++) {
        (void)devmm_convert_node_state_trans(node[j], dst_state, src_state);
    }
    return -EBUSY;
}

static void devmm_convert_nodes_destroy_srcu_work(u64 *arg, u64 arg_size)
{
    struct devmm_convert_node **convert_node = (struct devmm_convert_node **)arg;
    u64 total_num = arg_size / sizeof(struct devmm_convert_node *);
    u32 stamp = (u32)ka_jiffies;
    u32 destroy_num = 0;
    int ret, i;

    for (i = 0; i < total_num; i++) {
        ret = devmm_convert_node_destroy(convert_node[i], CONVERT_NODE_PREPARE_FREE, true);
        if (ret != 0) {
            ret = devmm_convert_node_state_trans(convert_node[i], CONVERT_NODE_SUBRES_RECYCLED, CONVERT_NODE_FREEING);
            if (ret == 0) {
                /* Node's resource has already been recycled, so only erase */
                devmm_convert_node_erase(convert_node[i]);
            }
        }

        if (ret == 0) {
            /* Corresponds to devmm_destroy_addr_batch->devmm_convert_nodes_get_by_batch_para */
            devmm_convert_node_put(convert_node[i]);
            destroy_num++;
        }
        /*
         * If kernel schedule is preemptable,
         * exit_to_usermode may case schedule which will increase the time of syscall.
         * If sched_switch from syscall to this work, ka_system_usleep_range will schedule,
         * which allows the system call to return quickly.
         */
        if (i == 0) {
            ka_system_usleep_range(10, 20);  /* usleep 10us ~ 20us */
        }
        devmm_try_cond_resched_by_time(&stamp, 10);  /* resched by 10ms */
    }
    devmm_drv_debug("Convert nodes destroy work. (total_num=%llu; destroy_num=%u)\n", total_num, destroy_num);
}

static int devmm_convert_nodes_destroy_async(struct devmm_srcu_work *srcu_work,
    struct devmm_convert_node **node, u32 num)
{
    return devmm_srcu_subwork_add(srcu_work, DEVMM_SRCU_SUBWORK_DEFAULT_TYPE,
        devmm_convert_nodes_destroy_srcu_work, (u64 *)node, sizeof(struct devmm_convert_node *) * num);
}

static int devmm_convert_nodes_get_by_batch_para(struct devmm_svm_process *svm_proc,
    struct devmm_destroy_addr_batch_para *batch_para, struct devmm_convert_node **node)
{
    struct DMA_ADDR **dma_addr = batch_para->dmaAddr;
    struct DMA_ADDR tmp;
    u32 stamp = (u32)ka_jiffies;
    u64 handle;
    u32 i, j;

    for (i = 0; i < batch_para->num; i++) {
        if (ka_base_copy_from_user(&tmp, (void __ka_user *)(uintptr_t)dma_addr[i], sizeof(struct DMA_ADDR)) != 0) {
            devmm_drv_err("Copy_from_user fail.\n");
            return -EINVAL;
        }

        handle = (u64)(tmp.phyAddr.priv);
        node[i] = devmm_convert_node_get_by_task(svm_proc, handle);
        if (node[i] == NULL) {
            devmm_drv_err("Invaild handle. (i=%u)\n", i);
            goto put_nodes;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
put_nodes:
    for (j = 0; j < i; j++) {
        devmm_convert_node_put(node[j]);
    }
    return -EINVAL;
}

int devmm_destroy_addr_batch_async(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_destroy_addr_batch_para *batch_para = &arg->data.destroy_batch_para;
    struct devmm_convert_node **node = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret, i;

    node = devmm_kvzalloc(sizeof(struct devmm_convert_node *) * batch_para->num);
    if (node == NULL) {
        devmm_drv_err("Kvzalloc fail. (num=%u)\n", batch_para->num);
        return -ENOMEM;
    }

    ret = devmm_convert_nodes_get_by_batch_para(svm_proc, batch_para, node);
    if (ret != 0) {
        devmm_drv_err("Get convert node fail. (ret=%d)\n", ret);
        goto free_node;
    }

    ret = devmm_convert_nodes_state_trans(node, batch_para->num, CONVERT_NODE_IDLE, CONVERT_NODE_PREPARE_FREE);
    if (ret != 0) {
        devmm_drv_err("Trans state failed. (ret=%d)\n", ret);
        goto put_nodes;
    }

    devmm_convert_nodes_get(node, batch_para->num);
    ret = devmm_convert_nodes_destroy_async(&svm_proc->srcu_work, node, batch_para->num);
    if (ret != 0) {
        devmm_drv_err("Submit async destroy failed. (ret=%d)\n", ret);
        devmm_convert_nodes_put(node, batch_para->num);     /* Corresponds to devmm_convert_nodes_get */
        (void)devmm_convert_nodes_state_trans(node, batch_para->num, CONVERT_NODE_PREPARE_FREE, CONVERT_NODE_IDLE);
        goto put_nodes;
    }

    for (i = 0; i < batch_para->num; i++) {
        struct devmm_convert_node_info *info = &node[i]->info;
        devmm_convert_vmmas_occupy_dec(svm_proc, info->src_va, info->spitch, info->width, info->height);
        devmm_convert_vmmas_occupy_dec(svm_proc, info->dst_va, info->dpitch, info->width, info->height);
        devmm_dec_page_ref(svm_proc, info->src_va, SVM_ADDR_REF_OPS_UNKNOWN_SIZE);
        devmm_dec_page_ref(svm_proc, info->dst_va, SVM_ADDR_REF_OPS_UNKNOWN_SIZE);
        devmm_try_cond_resched(&stamp);
    }

put_nodes:
    devmm_convert_nodes_put(node, batch_para->num);
free_node:
    devmm_kvfree(node);
    return ret;
}

int devmm_destroy_addr_batch_sync(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_destroy_addr_batch_para *batch_para = &arg->data.destroy_batch_para;
    struct DMA_ADDR dma_addr;
    struct devmm_ioctl_arg tmp_arg = {{0}};
    int ret, i;

    for (i = 0; i < batch_para->num; i++) {
        if (ka_base_copy_from_user(&dma_addr, (void __ka_user *)(uintptr_t)batch_para->dmaAddr[i], sizeof(struct DMA_ADDR)) != 0) {
            devmm_drv_err("Copy_from_user fail. (i=%d)\n", i);
            return -EINVAL;
        }

        tmp_arg.head = arg->head;
        tmp_arg.data.desty_para.dmaAddr = dma_addr;
        ret = devmm_ioctl_destroy_addr(svm_proc, &tmp_arg);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}

static int devmm_destroy_addr_batch_para_check(struct devmm_destroy_addr_batch_para *batch_para)
{
    if ((batch_para->num == 0) || (batch_para->num > DEVMM_DESTROY_BATCH_MAX_NUM)) {
        devmm_drv_err("Num is invalid.");
        return -EINVAL;
    }

    if (batch_para->dmaAddr == NULL) {
        devmm_drv_err("dmaAddr[] is null.\n");
        return -EINVAL;
    }

    return 0;
}

static int devmm_destroy_batch_para_init(struct devmm_destroy_addr_batch_para *batch_para)
{
    struct DMA_ADDR **tmp_user_ptr = batch_para->dmaAddr;
    struct DMA_ADDR **dma_addr = NULL;
    u64 arg_size;
    u32 i;

    arg_size = sizeof(struct DMA_ADDR *) * batch_para->num;
    dma_addr = devmm_kvzalloc(arg_size);
    if (dma_addr == NULL) {
        devmm_drv_err("Kvzalloc fail. (num=%u)\n", batch_para->num);
        return -ENOMEM;
    }

    if (ka_base_copy_from_user(dma_addr, (void __ka_user *)(uintptr_t)tmp_user_ptr, arg_size) != 0) {
        devmm_drv_err("Copy_from_user fail.\n");
        devmm_kvfree(dma_addr);
        return -EINVAL;
    }

    for (i = 0; i < batch_para->num; i++) {
        if (dma_addr[i] == NULL) {
            devmm_drv_err("Ptr[] is null. (i=%u)\n", i);
            devmm_kvfree(dma_addr);
            return -EINVAL;
        }
    }
    batch_para->dmaAddr = dma_addr;
    return 0;
}

static void devmm_destroy_batch_para_uninit(struct devmm_destroy_addr_batch_para *batch_para)
{
    devmm_kvfree(batch_para->dmaAddr);
    batch_para->dmaAddr = NULL;
}

int devmm_ioctl_destroy_addr_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_destroy_addr_batch_para *batch_para = &arg->data.destroy_batch_para;
    int ret;

    ret = devmm_destroy_addr_batch_para_check(batch_para);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_destroy_batch_para_init(batch_para);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_destroy_addr_batch(svm_proc, arg);
    devmm_destroy_batch_para_uninit(batch_para);
    return ret;
}


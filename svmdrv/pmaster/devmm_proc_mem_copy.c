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

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/mm_types.h>
#include <linux/atomic.h>

#include "ascend_kernel_hal.h"

#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "svm_shmem_interprocess.h"
#include "devmm_channel.h"
#include "svm_msg_client.h"
#include "devmm_common.h"
#include "devmm_page_cache.h"
#include "devmm_register_dma.h"
#include "svm_dma.h"
#include "svm_heap_mng.h"
#include "svm_proc_mng.h"
#include "svm_master_proc_mng.h"
#include "svm_master_convert.h"
#include "svm_res_idr.h"
#include "svm_master_dev_capability.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_srcu_work.h"

#ifdef CFG_FEATURE_VFIO
#include "devmm_pm_adapt.h"
#include "devmm_pm_vpc.h"
#endif
#include "devmm_proc_mem_copy.h"

#ifndef EMU_ST
#define CONVERT_COPY_MAX_SIZE (1 * 1024 * 1024)
#else
#define CONVERT_COPY_MAX_SIZE (64 * 64)
#endif
#define CONVERT_MAX_RECYCLE_NUM (DEVMM_MAX_SHM_TS_SIZE / CONVERT_COPY_MAX_SIZE)
#define DEVMM_NODE_RESERVE_NUM 2 /* DEVDRV_RESERVE_NUM */
#define CONVERT_COPY_USE_DMA 0
#define CONVERT_COPY_USE_BAR 1

enum {
    DEVMM_DMA_CPY_SIDE_HOST = 0,
    DEVMM_DMA_CPY_SIDE_DEVICE = 1,
};

struct devmm_dma_sqcq_size {
    u32 sq_desc_size;
    u32 cq_desc_size;
};

u32 devmm_get_convert_limit_size(u32 devid, u32 vfid)
{
    if (devmm_is_mdev_vm(devid, vfid)) {
        return DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT * KA_MM_PAGE_SIZE;
    } else {
        return devmm_get_convert_128m_size();
    }
}

static u32 devmm_get_page_insert_dev_id(u32 vm_id, u32 dev_id, u32 logic_id, u32 fid)
{
#ifdef CFG_FEATURE_VFIO
    if (vm_id != 0) {
        return devmm_get_vm_dev_id(dev_id, fid);
    }
#endif
    return logic_id;
}

static int devmm_judge_shm_info(u32 dev_id, struct devmm_chan_exchange_pginfo *info)
{
    u32 i;

    if (info->ts_shm_block_num == 0 ||
        info->ts_shm_block_num > DEVMM_MAX_BLOCK_NUM) {
        devmm_drv_err("Block number is invalid. (block_num=%u)\n", info->ts_shm_block_num);
        return -EINVAL;
    }
    if (info->ts_shm_data_num == 0 ||
        info->ts_shm_data_num > DEVMM_MAX_SHM_DATA_NUM) {
        devmm_drv_err("Data number is invalid. (data_num=%u)\n", info->ts_shm_data_num);
        return -EINVAL;
    }

    for (i = 0; i < info->ts_shm_block_num; i++) {
        if (info->ts_shm_dma_addr[i] == 0) {
            devmm_drv_err("Ts_shm_dma_addr invalid. (dev_id=%u; i=%u; block_num=%u; data_num=%u; "
                "is_addr_valid=%u)\n", dev_id, i, info->ts_shm_block_num, info->ts_shm_data_num,
                (info->ts_shm_addr[i] != 0));
            return -EINVAL;
        }
    }
    return 0;
}

#ifndef EMU_ST
static inline int devmm_get_bar_addr_info(u32 devid, u64 device_phy_addr, u64 *base_addr, size_t *size)
{
    return devdrv_get_addr_info(devid, DEVDRV_ADDR_TS_SHARE_MEM, 0, base_addr, size);
}
#endif

static void devmm_mmap_convert_bar_addr(u32 dev_id, struct devmm_share_memory_head *convert_mng,
    struct devmm_chan_exchange_pginfo *info)
{
    u32 data_num_per_block, block_size, i, unmap_num, per_data_len;
    u64 base_addr, size;
    int ret;

    convert_mng->support_bar_write = 0;
    /* virtual machine remap bar is nGnRE, drv can not use weaker order preserving, slower than dma */
    if ((info->ts_shm_support_bar_write == DEVMM_SHM_TS_NOT_MAP_BAR) ||
        (devmm_get_host_run_mode(dev_id) != DEVMM_HOST_IS_PHYS)) {
        return;
    }
    per_data_len = (u32)sizeof(struct devmm_share_memory_data);
    data_num_per_block = convert_mng->total_data_num / convert_mng->total_block_num;
    block_size = data_num_per_block * per_data_len;

    if (info->ts_shm_support_bar_write == DEVMM_SHM_TS_RESERVE_MAP_BAR) {
        ret = devmm_get_bar_addr_info(dev_id, info->ts_shm_addr[0], &base_addr, (size_t *)&size);
        if ((ret != 0) || (base_addr == 0) || (size < (convert_mng->total_data_num * per_data_len))) {
            devmm_drv_info("Devdrv_get_addr_info not support, not write by bar. (ret=%d; base=%llx, size=%llu)\n",
                ret, base_addr, size);
            return;
        }
        for (i = 0; i < convert_mng->total_block_num; i++) {
            convert_mng->block_addr[i] = base_addr + i * block_size;
        }
    } else {
        for (i = 0; i < convert_mng->total_block_num; i++) {
            ret = devdrv_devmem_addr_d2h(dev_id, info->ts_shm_addr[i], &(convert_mng->block_addr[i]));
            if (ret != 0) {
                devmm_drv_info("Devdrv_devmem_addr_d2h not support, not write by bar.\n");
                return;
            }
        }
    }

    for (i = 0; i < convert_mng->total_block_num; i++) {
        convert_mng->block_addr[i] = (u64)ka_mm_ioremap_wc(convert_mng->block_addr[i], block_size);
        if (convert_mng->block_addr[i] == 0) {
            devmm_drv_warn("Bar address d2h ioremap fail.\n");
            unmap_num = i;
            for (i = 0; i < unmap_num; i++) {
                ka_mm_iounmap((void *)convert_mng->block_addr[i]);
            }
            return;
        }
    }
    convert_mng->support_bar_write = 1;
    devmm_drv_info("Ts share mem support write by bar.\n");

    return;
}

int devmm_init_convert_addr_mng(u32 dev_id, struct devmm_chan_exchange_pginfo *info)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];
    u32 index, i;

    if (devmm_judge_shm_info(dev_id, info) != 0) {
        return -EINVAL;
    }

    convert_mng->total_block_num = info->ts_shm_block_num;
    convert_mng->total_data_num = info->ts_shm_data_num;
    convert_mng->free_index = 0;

    for (i = 0; i < convert_mng->total_block_num; i++) {
        convert_mng->block_dma_addr[i] = info->ts_shm_dma_addr[i];
        convert_mng->block_id[i] = info->ts_shm_block_id[i];
    }

    ka_task_mutex_init(&convert_mng->mutex);
    convert_mng->share_memory_mng = (struct devmm_share_memory_mng *)devmm_vzalloc_ex(
        (u64)(sizeof(struct devmm_share_memory_mng)) * (u64)(info->ts_shm_data_num));
    if (convert_mng->share_memory_mng == NULL) {
        devmm_drv_err("Vzalloc failed.\n");
        return -ENOMEM;
    }
    for (index = 0; index < info->ts_shm_data_num; index++) {
        convert_mng->share_memory_mng[index].id = 1;
    }

    devmm_mmap_convert_bar_addr(dev_id, convert_mng, info);

    return 0;
}

static void devmm_unmap_convert_bar_addr(struct devmm_share_memory_head *convert_mng)
{
    int i;

    if (convert_mng->support_bar_write == 1) {
        for (i = 0; i < convert_mng->total_block_num; i++) {
            ka_mm_iounmap((void *)convert_mng->block_addr[i]);
        }
    }
}

void devmm_uninit_convert_addr_mng(u32 dev_id)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];

    if (convert_mng->share_memory_mng != NULL) {
        devmm_unmap_convert_bar_addr(convert_mng);
        devmm_vfree_ex(convert_mng->share_memory_mng);
        convert_mng->share_memory_mng = NULL;
    }
}

static void devmm_get_convert_shm_addr_info(struct devmm_share_memory_head *convert_mng,
    u32 index, u32 addr_type, u64 *shm_addr, u32 *size)
{
    u32 data_num_per_block = convert_mng->total_data_num / convert_mng->total_block_num;
    u32 data_id = index % data_num_per_block;
    u32 shm_id = index / data_num_per_block;
    u64 shm_base_addr;

    shm_base_addr = (addr_type == CONVERT_COPY_USE_BAR) ?
        convert_mng->block_addr[shm_id] : convert_mng->block_dma_addr[shm_id];
    *size = sizeof(struct devmm_share_memory_data);
    *shm_addr = shm_base_addr + (u64)data_id * (u64)(*size);
}

static int devmm_copy_convert_addr_info_by_dma(u32 dev_id, u32 index, u32 dir,
    struct devmm_share_memory_data *data, u32 num)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];
    struct devdrv_dma_node dma_node = {0};
    ka_dma_data_direction_t dma_dir;
    ka_device_t *dev = NULL;
    phys_addr_t dma_addr;
    u64 dev_dma_addr;
    int ret;

    dev = devmm_device_get_by_devid(dev_id);
    if (dev == NULL) {
        devmm_drv_err("Device is not ready. (devid=%u)\n", dev_id);
        return -ENODEV;
    }

    devmm_get_convert_shm_addr_info(convert_mng, index,
        CONVERT_COPY_USE_DMA, &dev_dma_addr, &dma_node.size);
    dma_node.size = dma_node.size * num;
    dma_dir = (dir == CONVERT_INFO_TO_DEVICE) ? KA_DMA_TO_DEVICE : KA_DMA_FROM_DEVICE;

    dma_addr = devdrv_dma_map_single(dev, data, (size_t)dma_node.size, dma_dir);
    if (ka_mm_dma_mapping_error(dev, dma_addr) != 0) {
        devmm_device_put_by_devid(dev_id);
        devmm_drv_err("Dma map single failed. (dev_id=%d)\n", dev_id);
        return -ENOMEM;
    }

    if (dir == CONVERT_INFO_TO_DEVICE) {
        dma_node.src_addr = (u64)dma_addr;
        dma_node.dst_addr = dev_dma_addr;
        dma_node.direction = DEVDRV_DMA_HOST_TO_DEVICE;
    } else {
        dma_node.src_addr = dev_dma_addr;
        dma_node.dst_addr = (u64)dma_addr;
        dma_node.direction = DEVDRV_DMA_DEVICE_TO_HOST;
    }

    ret = devmm_dma_sync_link_copy(dev_id, 0, &dma_node, 1);

    devdrv_dma_unmap_single(dev, dma_addr, (size_t)dma_node.size, dma_dir);
    devmm_device_put_by_devid(dev_id);

    return ret;
}

static int devmm_copy_one_convert_addr_info_by_dma(u32 dev_id, u32 index, u32 dir, struct devmm_share_memory_data *data)
{
    u32 data_size = sizeof(struct devmm_share_memory_data);
    u32 align_size = ka_base_round_up(data_size, KA_MM_PAGE_SIZE);
    struct devmm_share_memory_data *data_tmp = NULL;
    int ret;

    /* The size of data_tmp must be aligned with KA_MM_PAGE_SIZE, otherwise there is a risk of DMA copy security attack */
    data_tmp = (struct devmm_share_memory_data *)devmm_kmalloc_ex(align_size, KA_GFP_KERNEL);
    if (data_tmp == NULL) {
        devmm_drv_err("Alloc memory failed. (dev_id=%d)\n", dev_id);
        return -ENOMEM;
    }
    if (dir == CONVERT_INFO_TO_DEVICE) {
        (void)memcpy_s(data_tmp, data_size, data, data_size);
    }

    ret = devmm_copy_convert_addr_info_by_dma(dev_id, index, dir, data_tmp, 1);
    if ((ret == 0) && (dir == CONVERT_INFO_FROM_DEVICE)) {
        (void)memcpy_s(data, data_size, data_tmp, data_size);
    }
    devmm_kfree_ex(data_tmp);

    return ret;
}

static int devmm_copy_one_convert_addr_info_by_bar(u32 dev_id, u32 index, u32 dir,
    struct devmm_share_memory_data *data)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];
    u64 dev_addr;
    u32 size;
    int ret;

    devmm_get_convert_shm_addr_info(convert_mng, index,
        CONVERT_COPY_USE_BAR, &dev_addr, &size);
    if (dir == CONVERT_INFO_TO_DEVICE) {
        ret = memcpy_s((void *)dev_addr, size, (const void *)data, size);
    } else {
        ret = memcpy_s((void *)data, size, (const void *)dev_addr, size);
    }
    return ret;
}

static bool devmm_can_copy_convert_addr_use_bar(u32 dev_id)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];

    if (convert_mng->support_bar_write != 1) {
        return false;
    } else {
        return true;
    }
}

int devmm_copy_one_convert_addr_info(u32 dev_id, u32 index, u32 dir, struct devmm_share_memory_data *data)
{
    int ret;

    if (devmm_can_copy_convert_addr_use_bar(dev_id) == false) {
        ret = devmm_copy_one_convert_addr_info_by_dma(dev_id, index, dir, data);
    } else {
        ret = devmm_copy_one_convert_addr_info_by_bar(dev_id, index, dir, data);
    }
    return ret;
}

void devmm_free_convert_index(u32 dev_id, u32 vfid, u32 index)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];

    /*
     * svm_releas_work is non-exclusive with uninit_instance,
     * check share_memory_mng for reliability, optimize it later.
     */
    if (convert_mng->share_memory_mng == NULL) {
#ifndef EMU_ST
        return;
#endif
    }

    convert_mng->share_memory_mng[index].va = 0;
    convert_mng->share_memory_mng[index].id++;
    /* data.id is 0 after clear, in case recycle same index */
    if (convert_mng->share_memory_mng[index].id == 0) {
        convert_mng->share_memory_mng[index].id = 1;
    }
    convert_mng->share_memory_mng[index].vfid = 0;
    convert_mng->share_memory_mng[index].host_pid = 0;
    convert_mng->share_memory_mng[index].data_type = 0;
    convert_mng->vdev_free_data_num[vfid]++;
}

static bool devmm_convert_index_can_recycle(struct devmm_share_memory_head *convert_mng,
    struct devmm_share_memory_data *data, u32 index)
{
    if ((convert_mng->share_memory_mng[index].va != 0) &&
        (convert_mng->share_memory_mng[index].data_type == DEVMM_DMA)) {
        devmm_drv_debug("Offset is in used by convert.\n");
        return false;
    }

    if ((convert_mng->share_memory_mng[index].id != data->id) && (data->id != 0)) {
        devmm_drv_debug("Unexpected, id is mismatch. (index=%u; id=%u; data.id=%u)\n",
            index, convert_mng->share_memory_mng[index].id, data->id);
        return false;
    }
#ifndef EMU_ST
    /* devmm_clear_share_memory_data finish, index had been release, can use again */
    if (data->host_pid == 0) {
        return true;
    }
    devmm_drv_debug("Proc is alive. (vm_id=%u; host_pid=%d)\n", data->vm_id, data->host_pid);
    if ((data->data_type == DEVMM_NON_DMA) && (data->image_word == DEVMM_FIN_MAGIC_WORD)) {
        return true;
    }
    devmm_drv_debug("Offset is still in using. (data_type=%u; image_word=%u)\n", data->data_type, data->image_word);
#else
    return true;
#endif
    return false;
}

static void devmm_try_free_convert_index(struct devmm_share_memory_head *convert_mng,
    struct devmm_share_memory_data *data, u32 dev_id, u32 num, u32 *recycle_num)
{
    u32 i;

    for (i = 0; i < num; i++) {
        u32 recycle_index = convert_mng->recycle_index + i;
        if (devmm_convert_index_can_recycle(convert_mng, &data[i], recycle_index)) {
            if (data[i].vfid < DEVMM_MAX_VF_NUM) {
                devmm_free_convert_index(dev_id, data[i].vfid, recycle_index);
                (*recycle_num)++;
            } else {
                devmm_drv_err("Unexpected, data.vfid is out of range. (vfid=%u)\n", data[i].vfid);
            }
        }
    }
}

STATIC void devmm_recycle_finish_convert_addr_index(u32 dev_id)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];
    struct devmm_share_memory_data *data = NULL;
    u32 max_num, recycle_num, num, total_num;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    data = (struct devmm_share_memory_data *)devmm_kzalloc_ex(CONVERT_COPY_MAX_SIZE, KA_GFP_KERNEL);
    max_num = (u32)(CONVERT_COPY_MAX_SIZE / sizeof(struct devmm_share_memory_data));
    if (data == NULL) {
        data = (struct devmm_share_memory_data *)devmm_kzalloc_ex(KA_MM_PAGE_SIZE, KA_GFP_KERNEL);
        max_num = (u32)(KA_MM_PAGE_SIZE / sizeof(struct devmm_share_memory_data));
    }

    if (data == NULL) {
        devmm_drv_err("Alloc memory failed. (dev_id=%d)\n", dev_id);
        return;
    }

    recycle_num = 0;
    total_num = 0;

    while ((recycle_num == 0) && (total_num < convert_mng->total_data_num)) {
        if (convert_mng->recycle_index + max_num <= convert_mng->total_data_num) {
            num = max_num;
        } else {
            num = convert_mng->total_data_num - convert_mng->recycle_index;
        }
        ret = devmm_copy_convert_addr_info_by_dma(dev_id, convert_mng->recycle_index,
            CONVERT_INFO_FROM_DEVICE, data, num);
        if (ret != 0) {
            devmm_drv_warn("Convert dma address failed. (dev_id=%d; ret=%d)\n", dev_id, ret);
            goto next;
        }

        devmm_try_free_convert_index(convert_mng, data, dev_id, num, &recycle_num);

next:
        convert_mng->recycle_index = (convert_mng->recycle_index + num) % convert_mng->total_data_num;
        total_num += num;
        devmm_try_cond_resched(&stamp);
    }

    devmm_kfree_ex(data);
}

static int devmm_get_idle_convert_addr_index(struct devmm_share_memory_head *convert_mng,
    u32 host_pid, u32 vfid, u64 va, u8 data_type)
{
    u32 i, index;

    for (i = 0; i < convert_mng->total_data_num; i++) {
        index = (convert_mng->free_index + i) % convert_mng->total_data_num;
        if (convert_mng->share_memory_mng[index].va == 0) {
            convert_mng->share_memory_mng[index].va = va;
            convert_mng->share_memory_mng[index].vfid = (u8)vfid;
            convert_mng->share_memory_mng[index].host_pid = host_pid;
            convert_mng->share_memory_mng[index].data_type = data_type;
            convert_mng->free_index = (index + 1) % convert_mng->total_data_num;
            convert_mng->vdev_free_data_num[vfid]--;
            return (int)index;
        }
    }
    return -1;
}

static int devmm_occupy_idle_convert_addr_index(u32 dev_id, u32 host_pid, u32 vfid, u64 va, u8 data_type)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];
    u32 stamp = (u32)ka_jiffies;
    u32 retry_cnt;
    int index;

    retry_cnt = 0;
    ka_task_mutex_lock(&convert_mng->mutex);
    while (1) {
        if (((vfid != 0) && (convert_mng->vdev_free_data_num[vfid] == 0)) == false) {
            index = devmm_get_idle_convert_addr_index(convert_mng, host_pid, vfid, va, data_type);
            if (index != -1) {
                ka_task_mutex_unlock(&convert_mng->mutex);
                return index;
            }
        }
        if (retry_cnt >= CONVERT_MAX_RECYCLE_NUM) {
            break;
        }
        devmm_recycle_finish_convert_addr_index(dev_id);
        retry_cnt++;
        devmm_try_cond_resched(&stamp);
    };
    devmm_drv_debug("Occupy idle address error. (vfid=%d; free_offset=0; total_num=%d)\n",
        vfid, convert_mng->vdev_total_data_num[vfid]);
    ka_task_mutex_unlock(&convert_mng->mutex);
    return -1;
}

static void devmm_free_convert_addr_index(u32 dev_id, u32 vfid, u32 index)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];

    ka_task_mutex_lock(&convert_mng->mutex);
    devmm_free_convert_index(dev_id, vfid, index);
    ka_task_mutex_unlock(&convert_mng->mutex);
}

/*
 * index: 0 ~ total_data_num, the view of host
 * offset: 0 ~ shm_size, the view of device
 */
static u64 devmm_convert_index_to_offset(u32 devid, u64 index)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[devid];
    u64 data_num_per_block, data_id, block_id;

    data_num_per_block = convert_mng->total_data_num / convert_mng->total_block_num;
    block_id = convert_mng->block_id[index / data_num_per_block];
    data_id = index % data_num_per_block;

    return ((block_id * data_num_per_block + data_id) * (u64)sizeof(struct devmm_share_memory_data));
}

static int devmm_set_convert_ka_dma_addr_to_device(struct devmm_svm_process *svm_pro,
    struct devmm_ioctl_arg *arg, u64 *offset, u32 *index)
{
    struct DMA_ADDR *dma_addr = &arg->data.convrt_para.dmaAddr;
    struct devmm_share_memory_head *convert_mng = NULL;
    struct devmm_share_memory_data data;
    u32 virt_id = arg->data.convrt_para.virt_id;
    u64 va = (u64)arg->data.convrt_para.pDst;
    u32 host_pid = (u32)svm_pro->process_id.hostpid;
    u32 fixed_size = dma_addr->fixed_size;
    u32 dev_id = arg->head.devid;
    u32 vfid = arg->head.vfid;
    int ret, idle_index;

    idle_index = devmm_occupy_idle_convert_addr_index(dev_id, host_pid, vfid, va, DEVMM_DMA);
    if (idle_index < 0) {
        devmm_drv_debug("No idle convert index. (dev_id=%d; hostpid=%d)\n", dev_id, svm_pro->process_id.hostpid);
        return -ENODATA;    /* return -ENODATA the caller will retry */
    }

    convert_mng = &devmm_svm->pa_info[dev_id];
    /* vf ts use fixed size to flow control */
    ret = memcpy_s(data.data, DEVMM_DATA_SIZE, dma_addr, sizeof(struct DMA_ADDR));
    if (ret != 0) {
        devmm_free_convert_addr_index(dev_id, vfid, (u32)idle_index);
        devmm_drv_err("Memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    data.image_word = DEVMM_WRITED_MAGIC_WORD;
    data.id = convert_mng->share_memory_mng[idle_index].id;
    data.vm_id = svm_pro->process_id.vm_id;
    data.host_pid = (int)host_pid;
    data.data_type = DEVMM_DMA;
    data.vfid = (u16)(vfid);
    data.va = va;

    dma_addr->offsetAddr.offset = devmm_convert_index_to_offset(dev_id, (u64)idle_index);
    dma_addr->offsetAddr.devid = virt_id;
    dma_addr->fixed_size = fixed_size;
    dma_addr->virt_id = virt_id;
    ret = devmm_copy_one_convert_addr_info(dev_id, (u32)idle_index, (u32)CONVERT_INFO_TO_DEVICE, &data);
    if (ret != 0) {
        devmm_free_convert_addr_index(dev_id, vfid, (u32)idle_index);
        devmm_drv_err("Convert dma address failed. (dev_id=%d; hostpid=%d; ret=%d)\n",
            dev_id, host_pid, ret);
        return ret;
    }

    *offset = dma_addr->offsetAddr.offset;
    *index = (u32)idle_index;
    return 0;
}

void devmm_clear_one_convert_dma_addr(u32 dev_id, u32 vfid, u32 index)
{
    struct devmm_share_memory_data data = {0};
    int ret;

    ret = devmm_copy_one_convert_addr_info(dev_id, index, CONVERT_INFO_TO_DEVICE, &data);
    if (ret != 0) {
        devmm_drv_err("Convert dma address failed. (dev_id=%d; ret=%d)\n", dev_id, ret);
    }

    devmm_free_convert_addr_index(dev_id, vfid, index);
}

static int devmm_fill_translate_offset_data(struct devmm_svm_process *svm_proc,
    struct devmm_translate_info *trans, u64 pa, struct devmm_share_memory_data *data)
{
    struct translate_offset_data translate_data = {0};
    u64 continuty_len;
    int ret;

    if (trans->is_vm_translate) {
        /*
         * VM's msg is untrusted, PM don't know how much size should query.
         * So PM should query continuty_len as much as possible within the MAX_CONTINUTY_PHYS_SIZE range limited by OS.
         */
        continuty_len = devmm_get_continuty_len_after_dev_va(svm_proc, trans->page_insert_dev_id,
            trans->va, trans->page_size);
    } else {
        if (trans->is_svm_continuty) {
            continuty_len = trans->alloced_size - (trans->va - trans->alloced_va);
        } else {
            continuty_len = trans->page_size - (trans->va - ka_base_round_down(trans->va, trans->page_size));
        }
    }

    translate_data.pa = pa;
    translate_data.continuty_len = continuty_len;
    ret = memcpy_s(data->data, DEVMM_DATA_SIZE, &translate_data, sizeof(struct translate_offset_data));
    if (ret != 0) {
        devmm_drv_err("Memcpy_s failed. (ret = %d)\n", ret);
    }

    return ret;
}

int devmm_set_translate_pa_addr_to_device_inner(struct devmm_svm_process *svm_proc,
    struct devmm_translate_info trans, u64 *pa_offset)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[trans.dev_id];
    struct devmm_share_memory_data data = {0};
    int host_pid = svm_proc->process_id.hostpid;
    u16 vm_id = svm_proc->process_id.vm_id;
    int index, ret;
    u64 pa;

    ret = devmm_find_pa_cache(svm_proc, trans.page_insert_dev_id, trans.va, trans.page_size, &pa);
    if (ret != 0) {
        devmm_drv_err("Find pa cache failed. (dev_id=%d; hostpid=%d; va=%llx)\n",
            trans.dev_id, host_pid, trans.va);
        return ret;
    }

    ret = devmm_fill_translate_offset_data(svm_proc, &trans, pa, &data);
    if (ret != 0) {
        devmm_drv_err("Fill translate data error. (ret=%d)\n", ret);
        return ret;
    }

    index = devmm_occupy_idle_convert_addr_index(trans.dev_id, (u32)host_pid, trans.vfid, trans.va, DEVMM_NON_DMA);
    if (index < 0) {
        devmm_drv_debug("No idle translate index. (dev_id=%d; hostpid=%d)\n", trans.dev_id, host_pid);
        return -ENODATA;
    }

    data.image_word = DEVMM_WRITED_MAGIC_WORD;
    data.id = convert_mng->share_memory_mng[index].id;
    data.vm_id = vm_id;
    data.host_pid = host_pid;
    data.data_type = DEVMM_NON_DMA;
    data.vfid = (u16)trans.vfid;
    data.va = trans.va;

    *pa_offset = devmm_convert_index_to_offset(trans.dev_id, (u64)index);

    ret = devmm_copy_one_convert_addr_info(trans.dev_id, (u32)index, CONVERT_INFO_TO_DEVICE, &data);
    if (ret != 0) {
        devmm_free_convert_addr_index(trans.dev_id, trans.vfid, (u32)index);
        devmm_drv_err("Convert dma address failed. (dev_id=%d; hostpid=%d; ret=%d)\n", trans.dev_id, host_pid, ret);
    }

    devmm_drv_debug("Translate info. (index=%d; data.host_pid=%d; data.vfid=%u; data.va=0x%llx; devid=%u)\n",
        index, data.host_pid, data.vfid, data.va, trans.dev_id);
    return ret;
}

static int devmm_try_get_idle_translate_mem(struct devmm_svm_process *svm_proc,
    struct devmm_translate_info trans, u64 *pa_offset)
{
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;
    u32 stamp = (u32)ka_jiffies;
    u32 num;
    int ret;

    svm_id_inst_pack(&id_inst, trans.dev_id, trans.vfid);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        devmm_drv_err("Dev uninit instance. (devid=%u; vfid=%u)", id_inst.devid, id_inst.vfid);
        return -ENODEV;
    }

    while (1) {
        ret = devmm_set_translate_pa_addr_to_device_inner(svm_proc, trans, pa_offset);
        if (ret != -ENODATA) {
            break;
        }

        devmm_convert_nodes_subres_recycle_by_dev_res_mng(dev_res_mng, &num);
        if (num == 0) {
            break;
        }
        devmm_try_cond_resched(&stamp);
    }

    if (ret != 0) {
        devmm_drv_err("Get idle offset failed. (ret=%d)", ret);
    }

    devmm_dev_res_mng_put(dev_res_mng);
    return ret;
}

int devmm_set_translate_pa_addr_to_device(struct devmm_svm_process *svm_process,
    struct devmm_translate_info trans, u64 *pa_offset)
{
    return devmm_try_get_idle_translate_mem(svm_process, trans, pa_offset);
}

int devmm_clear_translate_pa_addr_inner(u32 dev_id, u32 vfid, u64 va, u64 len, u32 host_pid)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];
    struct devmm_share_memory_data data;
    u32 stamp = (u32)ka_jiffies;
    int ret;
    u32 i;

    for (i = 0; i < convert_mng->total_data_num; i++) {
        if ((convert_mng->share_memory_mng[i].va < va) || (convert_mng->share_memory_mng[i].va >= (va + len))) {
            continue;
        }

        if ((convert_mng->share_memory_mng[i].host_pid != host_pid) ||
            (convert_mng->share_memory_mng[i].vfid != (u8)vfid) ||
            (convert_mng->share_memory_mng[i].data_type != DEVMM_NON_DMA)) {
            continue;
        }

        ret = devmm_copy_one_convert_addr_info(dev_id, i, CONVERT_INFO_FROM_DEVICE, &data);
        if (ret != 0) {
            devmm_drv_warn("Convert dma address failed. (dev_id=%d; hostpid=%d; ret=%d)\n", dev_id, host_pid, ret);
            continue;
        }

        if ((data.host_pid != host_pid) || (data.data_type != DEVMM_NON_DMA) ||
            (data.vfid != vfid) || (convert_mng->share_memory_mng[i].va != data.va)) {
            continue;
        }

        if (data.image_word == DEVMM_READED_MAGIC_WORD) {
            /* The log cannot be modified, because in the failure mode library. */
            devmm_drv_err("Vaddress is not finish image word, please GE and RTS call unbind_stream before rtfree. "
                          "(devid=%u; idx=%d; host_pid=%d; va=%llx; image_word=%x; offset=%llu)\n",
                          dev_id, i, host_pid, convert_mng->share_memory_mng[i].va,
                          data.image_word, devmm_convert_index_to_offset(dev_id, i));
            return -EBUSY;
        }
        devmm_free_convert_addr_index(dev_id, vfid, i);
        devmm_try_cond_resched(&stamp);
    }
    return 0;
}

int devmm_clear_translate_pa_addr(u32 dev_id, u32 vfid, u64 va, u64 len, u32 host_pid)
{
    return devmm_clear_translate_pa_addr_inner(dev_id, vfid, va, len, host_pid);
}

static int devmm_va_to_palist_check(struct devmm_svm_process *svm_proc, u64 va, u64 sz)
{
    unsigned long page_sq, page_num;
    struct devmm_svm_heap *heap = NULL;
    u32 *bitmap = NULL;

    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Oper may out heap size. (src=0x%llx; count=%llu)\n", va, sz);
        return -EADDRNOTAVAIL;
    }
    bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if ((bitmap == NULL) || (devmm_check_va_add_size_by_heap(heap, va, sz) != 0)) {
        devmm_drv_err("Bitmap is error. (va=0x%llx; sz=%llu)\n", va, sz);
        return -EINVAL;
    }
    page_num = devmm_get_pagecount_by_size(va, sz, heap->chunk_page_size);
    for (page_sq = 0; page_sq < page_num; page_sq++) {
        if (!devmm_page_bitmap_is_page_available(bitmap + page_sq)) {
            devmm_drv_err("Vaddress not alloced. (va=0x%llx; page_sq=%lu; bitmap=0x%x)\n",
                va, page_sq, *(bitmap + page_sq));
            devmm_print_pre_alloced_va(svm_proc, va + page_sq * heap->chunk_page_size);
            return -EINVAL;
        }
    }

    return 0;
}

static int devmm_va_to_paka_list_add_pgtable(struct devmm_svm_process *svm_proc, u64 va,
    u64 sz, struct devmm_copy_side *side)
{
    ka_vm_area_struct_t *vma = NULL;
    unsigned long vaddr, va_max_offset;
    u64 *pas = NULL;
    u32 pg_num = 0;
    int ret = 0;

    ret = devmm_va_to_palist_check(svm_proc, va, sz);
    if (ret != 0) {
        devmm_drv_err("Check failed. (src=0x%llx; count=%llu).\n", va, sz);
        return ret;
    }

    vaddr = ka_base_round_down(va, KA_MM_PAGE_SIZE);
    va_max_offset = ka_base_round_up(va + sz, KA_MM_PAGE_SIZE);
    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Find vma failed. (vaddr=0x%lx, hostpid=%d)\n", vaddr, svm_proc->process_id.hostpid);
        return -EADDRNOTAVAIL;
    }

    pas = (u64 *)devmm_kvalloc(side->num * sizeof(u64), 0);
    if (pas == NULL) {
        devmm_drv_err("Kvzalloc fail. (alloc_size=%lu)\n", side->num * sizeof(u64));
        return -ENOMEM;
    }

    ret = devmm_va_to_pa_range(vma, vaddr, side->num, pas);
    if (ret != 0) {
        devmm_drv_err("Vaddress to paddress range fail. (vaddr=0x%lx; num=%u; ret=%d)\n", vaddr, side->num, ret);
        devmm_kvfree(pas);
        return ret;
    }

    for (; vaddr < va_max_offset; vaddr += KA_MM_PAGE_SIZE, pg_num++) {
        if (pg_num >= side->num) {
            devmm_drv_err("Page num error. (va=0x%lx; size=%lld; num=%u; blk_num=%d)\n",
                vaddr, sz, side->num, side->blks_num);
            devmm_kvfree(pas);
            return -EINVAL;
        }
        side->blks[pg_num].pa = pas[pg_num];
        side->blks[pg_num].dma = (ka_dma_addr_t)side->blks[pg_num].pa;
        side->blks[pg_num].page = devmm_pa_to_page(pas[pg_num]);
        side->blks[pg_num].sz = KA_MM_PAGE_SIZE;
    }

    devmm_kvfree(pas);
    pas = NULL;
    if (pg_num != side->num) {
        devmm_drv_err("Page num error. (pg_num=%d; va=0x%lx; size=%lld; num=%u)\n", pg_num, vaddr, sz, side->num);
        return -EINVAL;
    }

    return ret;
}

int devmm_pa_node_list_dma_map(u32 dev_id, struct devmm_copy_side *side)
{
    bool is_hccs_vm_th = false;
    ka_device_t *dev = NULL;
    ka_page_t *page = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 host_flag;
    u32 i, j;
    int ret;

    ret = devmm_get_host_phy_mach_flag(dev_id, &host_flag);
    if (ret != 0) {
        return ret;
    }
    is_hccs_vm_th = devmm_is_hccs_vm_scene(dev_id, host_flag);

    dev = devmm_device_get_by_devid(dev_id);
    if (dev == NULL) {
        devmm_drv_err("Device is NULL.\n");
        return -EINVAL;
    }

    for (i = 0; i < side->num; i++) {
        if (is_hccs_vm_th) {
            side->blks[i].dma = side->blks[i].pa;
        } else {
            page = devmm_pa_to_page(side->blks[i].pa);
            side->blks[i].dma = devdrv_dma_map_page(dev, page, 0, side->blks[i].sz, KA_DMA_BIDIRECTIONAL);
            if (ka_mm_dma_mapping_error(dev, side->blks[i].dma) != 0) {
                devmm_drv_err("Dma mapping error. (i=%d; dma_mapping_error=%d)\n",
                    i, ka_mm_dma_mapping_error(dev, side->blks[i].dma));
                goto devmm_fill_dma_blklist_err;
            }
        }
        devmm_try_cond_resched(&stamp);
    }
    devmm_device_put_by_devid(dev_id);

    return 0;
devmm_fill_dma_blklist_err:
    stamp = (u32)ka_jiffies;
    for (j = 0; j < i; j++) {
        if (is_hccs_vm_th == false) {
            devdrv_dma_unmap_page(dev, side->blks[j].dma, side->blks[j].sz, KA_DMA_BIDIRECTIONAL);
            devmm_try_cond_resched(&stamp);
        }
    }
    devmm_device_put_by_devid(dev_id);

    return -EIO;
}

void devmm_pa_node_list_dma_unmap(u32 dev_id, struct devmm_copy_side *side)
{
    bool is_in_softirq = (bool)in_softirq();
    ka_device_t *dev = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 i, host_flag;
    int ret;

    ret = devmm_get_host_phy_mach_flag(dev_id, &host_flag);
    if (ret != 0) {
        return;
    }

    if (devmm_is_hccs_vm_scene(dev_id, host_flag)) {
        return;
    }

    dev = devmm_device_get_by_devid(dev_id);
    if (dev == NULL) {
        return;
    }

    for (i = 0; i < side->num; i++) {
        devdrv_dma_unmap_page(dev, side->blks[i].dma, side->blks[i].sz, KA_DMA_BIDIRECTIONAL);
        if (is_in_softirq == false) {
            devmm_try_cond_resched(&stamp);
        }
    }
    devmm_device_put_by_devid(dev_id);
}

#ifndef EMU_ST
static int devmm_d2d_pa_to_bar_dma(u32 src_dev_id, u32 dst_dev_id, u64 dst_pa, u64 *dst_dma)
{
    int ret;

    devmm_drv_debug("Info. (src_dev_id=%u; dst_dev_id=%u; is_pcie_connect=%u)\n",
        src_dev_id, dst_dev_id, devmm_is_pcie_connect(dst_dev_id));
    if (devmm_is_pcie_connect(dst_dev_id)) {
        ret = devdrv_devmem_addr_d2h(dst_dev_id, dst_pa, dst_dma);
        if (ret != 0) {
            devmm_drv_err("Pcie fill pa-to-barpa fail. (dev_id=%u; ret=%d)\n", dst_dev_id, ret);
            return ret;
        }

        ret = devdrv_devmem_addr_bar_to_dma(src_dev_id, dst_dev_id, *dst_dma, dst_dma);
        if (ret != 0) {
            devmm_drv_err("Pcie fill bar2dma fail. (src_dev_id=%u; dst_dev_id=%u; ret=%d)\n",
                src_dev_id, dst_dev_id, ret);
            return ret;
        }
    } else {
        *dst_dma = dst_pa; /* In hccs vm through scene, pcie transfer real bar_mem addr on the interface above, not device paddr */
    }
    return 0;
}
#endif

static int devmm_fill_dmanode(struct devmm_dma_block *blks, struct devmm_dma_block *dst_blks,
                              struct devdrv_dma_node *dma_node, struct devmm_copy_res *res)
{
    int copy_direction = res->copy_direction;
    u32 src_dev_id = (u32)res->dev_id;
    u32 dst_dev_id = (u32)res->dst_dev_id;

    if (copy_direction == DEVMM_COPY_HOST_TO_DEVICE) {
        dma_node->src_addr = blks->dma;
        dma_node->dst_addr = dst_blks->pa;
        dma_node->direction = DEVDRV_DMA_HOST_TO_DEVICE;
        dma_node->loc_passid = (u32)dst_blks->ssid;
    } else if (copy_direction == DEVMM_COPY_DEVICE_TO_HOST) {
        dma_node->src_addr = blks->pa;
        dma_node->dst_addr = dst_blks->dma;
        dma_node->direction = DEVDRV_DMA_DEVICE_TO_HOST;
        dma_node->loc_passid = (u32)blks->ssid;
    } else if (copy_direction == DEVMM_COPY_DEVICE_TO_DEVICE) {
        int ret;
        dma_node->src_addr = blks->pa;
#ifndef EMU_ST
        ret = devmm_d2d_pa_to_bar_dma(src_dev_id, dst_dev_id, dst_blks->pa, &dma_node->dst_addr);
        if (ret != 0) {
            return ret;
        }
#endif
        dma_node->direction = DEVDRV_DMA_DEVICE_TO_HOST;
        dma_node->loc_passid = (u32)blks->ssid;
    } else {
        devmm_drv_err("The copy direction not support. (copy_direction=%d)\n", copy_direction);
        return -EINVAL;
    }

    return 0;
}

static int devmm_make_dmanode_list(u64 src, u64 dst, size_t count, struct devmm_copy_res *res)
{
    size_t off_src, off_dst, total_len;
    u32 idx_src, idx_dst, idx_dma;
    u32 stamp = (u32)ka_jiffies;
    int ret = 0;

    off_src = src & (res->from.blk_page_size - 1);
    off_dst = dst & (res->to.blk_page_size - 1);

    for (total_len = 0, idx_src = 0, idx_dst = 0, idx_dma = 0; (total_len < count) && (idx_dst < res->to.num) &&
        (idx_src < res->from.num) && (idx_dma < res->dma_node_alloc_num);) {
        size_t sz_src, sz_dst, end_blk_src, end_blk_dst;
        sz_src = res->from.blks[idx_src].sz;
        sz_dst = res->to.blks[idx_dst].sz;
        end_blk_src = ka_base_min(sz_src - off_src, count - total_len);
        end_blk_dst = ka_base_min(sz_dst - off_dst, count - total_len);

        ret = devmm_fill_dmanode(&res->from.blks[idx_src], &res->to.blks[idx_dst], &res->dma_node[idx_dma], res);
        if (ret != 0) {
            break;
        }
        res->dma_node[idx_dma].src_addr += off_src;
        res->dma_node[idx_dma].dst_addr += off_dst;

        if (end_blk_src < end_blk_dst) {
            res->dma_node[idx_dma].size = (u32)end_blk_src;
            idx_src++;
            off_src = 0;
            off_dst += end_blk_src;
        } else if (end_blk_src > end_blk_dst) {
            res->dma_node[idx_dma].size = (u32)end_blk_dst;
            idx_dst++;
            off_dst = 0;
            off_src += end_blk_dst;
        } else {
            res->dma_node[idx_dma].size = (u32)end_blk_dst;
            idx_dst++;
            off_dst = 0;
            idx_src++;
            off_src = 0;
        }
        total_len += res->dma_node[idx_dma].size;
        devmm_drv_debug("Make dma nodes info. (src=0x%llx; dst=0x%llx; total_len=0x%lx; count=0x%x; "
            "idx_src=%u; idx_dst=%u; idx_dma=%u)\n",
            res->dma_node[idx_dma].src_addr, res->dma_node[idx_dma].dst_addr,
            total_len, res->dma_node[idx_dma].size, idx_src, idx_dst, idx_dma);
        idx_dma++;
        devmm_try_cond_resched(&stamp);
    }
    res->dma_node_num = idx_dma;

    if (total_len != count) {
        devmm_drv_err("Make dma node-size check fail, please check addr size. "
            "(total_len=%lu; count=%lu; idx_dma=%u; did=%d; dst_did=%d; "
            "src=0x%llx; dst=0x%llx; idx_src=%u; from_num=%u; idx_dst=%u; to_num=%u)\n", total_len, count,
            idx_dma, res->dev_id, res->dst_dev_id, src, dst, idx_src, res->from.num, idx_dst, res->to.num);
            ret = -EINVAL;
    }
    devmm_drv_debug("Exit make dma nodes. (src=0x%llx; dst=0x%llx; total_len=0x%lx; count=0x%lx; "
        "idx_src=%u; from_num=%u; idx_dst=%u; to_num=%u; idx_dma=%u; dma_node_num=%u)\n", src, dst,
        total_len, count, idx_src, res->from.num, idx_dst, res->to.num, idx_dma, res->dma_node_num);

    return ret;
}

STATIC void devmm_get_device_cpy_blk_num(int is_svm_huge, u64 byte_count, struct devmm_copy_side *side)
{
    if (is_svm_huge != 0) {
        side->num = (u32)DEVMM_SIZE_TO_HUGEPAGE_MAX_NUM(byte_count);
        side->blk_page_size = devmm_svm->device_hpage_size;
    } else {
        side->num = (u32)DEVMM_SIZE_TO_PAGE_MAX_NUM(byte_count, devmm_svm->device_page_size);
        side->blk_page_size = devmm_svm->device_page_size;
    }
}

STATIC u32 devmm_get_blk_num(u64 byte_count, struct devmm_memory_attributes *attr)
{
    u32 blk_num, page_size;

    if (attr->copy_use_va) {
        blk_num = 1;
    } else {
        if (attr->is_svm_device) {
            page_size = (attr->is_svm_huge) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;
        } else {
            page_size = attr->host_page_size;
        }

        blk_num = (u32)DEVMM_SIZE_TO_PAGE_NUM(byte_count, page_size);
    }

    return blk_num;
}

struct devmm_copy_res *devmm_alloc_copy_res(u64 byte_count,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr, bool is_need_clear)
{
    u64 buf_head_len, src_blks_len, dst_blks_len, buf_dma_node_len, buf_total_len;
    u32 src_blk_num, dst_blk_num, dma_node_num;
    struct devmm_copy_res *res = NULL;

    buf_head_len = sizeof(struct devmm_copy_res);

    src_blk_num = devmm_get_blk_num(byte_count, src_attr);
    src_blks_len = sizeof(struct devmm_dma_block) * src_blk_num;

    dst_blk_num = devmm_get_blk_num(byte_count, dst_attr);
    dst_blks_len = sizeof(struct devmm_dma_block) * dst_blk_num;

    dma_node_num = src_blk_num + dst_blk_num + DEVMM_BLKNUM_ADD_NUM;
    buf_dma_node_len = sizeof(struct devdrv_dma_node) * (dma_node_num + 1); /* add 1 to async status node */

    buf_total_len = buf_head_len + src_blks_len + dst_blks_len + buf_dma_node_len;

    res = (struct devmm_copy_res *)devmm_kvalloc(buf_total_len, (is_need_clear ? __KA_GFP_ZERO : 0));
    if (res == NULL) {
        devmm_drv_err("Vmalloc buf failed. (buf_total_len=%llu)\n", buf_total_len);
        return NULL;
    }

    /* Reduce zeroing, improve performance */
    if ((is_need_clear == false) && (memset_s((void *)res, buf_head_len, 0, buf_head_len) != 0)) {
        devmm_kvfree(res);
        devmm_drv_err("Memsets buf failed. (buf_total_len=%llu)\n", buf_total_len);
        return NULL;
    }

    res->from.blks = (struct devmm_dma_block *)(uintptr_t)((u64)(uintptr_t)res + buf_head_len);
    res->to.blks = (struct devmm_dma_block *)(uintptr_t)((u64)(uintptr_t)res->from.blks + src_blks_len);
    res->dma_node = (struct devdrv_dma_node *)(uintptr_t)((u64)(uintptr_t)res->to.blks + dst_blks_len);
    res->dma_prepare = NULL;
    res->copy_task = NULL;
    res->byte_count = byte_count;

    res->from.blks_num = src_blk_num;
    res->to.blks_num = dst_blk_num;

    res->dma_node_alloc_num = dma_node_num;
    res->height = 0;
    res->next = NULL;
    return res;
}

void devmm_free_copy_mem(struct devmm_copy_res *res)
{
    if (res != NULL) {
#ifdef CFG_FEATURE_VFIO
        if (res->vm_copy_flag == true) {
            devmm_clear_pm_host_addr(res);
        }
#endif
        devmm_kvfree(res);
    }
}

static bool devmm_page_is_belong_to_compound_page(ka_page_t *head, u64 pg_num, ka_page_t *tar_pg)
{
    if ((head == NULL) || (pg_num == 0)) {
        return false;
    }

    if (((u64)(uintptr_t)tar_pg >= (u64)(uintptr_t)head) &&
        ((u64)(uintptr_t)tar_pg <= (u64)(uintptr_t)(head + pg_num - 1))) {
        return true;
    }

    return false;
}

/* For performance, the same compound page only get_page once. */
static void _devmm_svm_addr_pa_list_get(struct devmm_copy_side *side)
{
    ka_page_t *comp_head = NULL;
    ka_page_t *pg = NULL;
    u64 comp_num = 0;
    u32 i;

    for (i = 0; i < side->num; i++) {
        pg = side->blks[i].page;
        if ((comp_head == NULL) && (PageCompound(pg) != 0)) {
            comp_head = ka_mm_compound_head(pg);
            comp_num = 1ULL << ka_mm_compound_order(comp_head);
            ka_mm_get_page(pg);
            side->blks[i].pg_is_get = true;
        } else if (devmm_page_is_belong_to_compound_page(comp_head, comp_num, pg)) {
            side->blks[i].pg_is_get = false;
        } else {
            ka_mm_get_page(pg);
            side->blks[i].pg_is_get = true;
        }
    }
}

int devmm_svm_addr_pa_list_get(struct devmm_svm_process *svm_proc, u64 va, u64 size,
    struct devmm_copy_side *side)
{
    int ret;

    ret = devmm_va_to_paka_list_add_pgtable(svm_proc, va, size, side);
    if (ret != 0) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Vaddress to palist add pgtable fail. "
                      "(va=0x%llx; size=0x%llx; blks_num=%d)\n", va, size, side->blks_num);
        return ret;
    }

    _devmm_svm_addr_pa_list_get(side);
    devmm_drv_debug("Get pages details. (va=0x%llx; size=0x%llx; blks_num=%d; num=%d)\n",
                    va, size, side->blks_num, side->num);
    return 0;
}

static void devmm_svm_addr_pa_list_put(struct devmm_copy_side *side)
{
    bool is_in_softirq = (bool)in_softirq();
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    for (i = 0; i < side->blks_num; i++) {
        if (side->blks[i].page != NULL) {
            if (side->blks[i].pg_is_get) {
                ka_mm_put_page(side->blks[i].page);
            }
            side->blks[i].page = NULL;
        } else {
            break;
        }
        if (is_in_softirq == false) {
            devmm_try_cond_resched(&stamp);
        }
    }
}

static int devmm_get_user_pages(struct devmm_svm_process *svm_proc,
    u64 va, u32 num, int write, ka_page_t **pages)
{
    if (svm_proc->process_id.hostpid == devmm_get_current_pid()) {
        return devmm_pin_user_pages_fast(va, num, write, pages);
    } else {
        return devmm_get_user_pages_remote(svm_proc->tsk, svm_proc->mm, va, write, num, pages);
    }
}

int devmm_get_non_svm_addr_pa_list(struct devmm_svm_process *svm_proc, u64 va, u64 size,
    struct devmm_copy_side *side, int write)
{
    int ret;
    u32 i;
    ka_page_t **pages = (ka_page_t **)devmm_kvalloc(side->num * sizeof(ka_page_t *), 0);

    if (pages == NULL) {
        devmm_drv_err("Alloc pages fail. (va=0x%llx; size=0x%llx; num=%d)\n", va, size, side->num);
        return -ENOMEM;
    }

    ret = devmm_get_user_pages(svm_proc, va, side->num, write, pages);
    if (ret != 0) {
        devmm_drv_err("Get user pages fail. (ret=%d; va=0x%llx; size=0x%llx; num=%d)\n", ret, va, size, side->num);
        devmm_kvfree(pages);
        return ret;
    }

    for (i = 0; i < side->num; i++) {
        side->blks[i].pa = ka_mm_page_to_phys(pages[i]);
        side->blks[i].dma = (ka_dma_addr_t)side->blks[i].pa;
        side->blks[i].page = pages[i];
        side->blks[i].sz = KA_MM_PAGE_SIZE;
    }

    devmm_kvfree(pages);

    devmm_drv_debug("Get pages details. (va=%llx; size=0x%llx; blks_num=%d; num=%d)\n",
                    va, size, side->blks_num, side->num);
    return 0;
}

static inline void devmm_put_non_svm_addr_pa_list(struct devmm_copy_side *side)
{
    bool is_in_softirq = (bool)in_softirq();
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    for (i = 0; i < side->blks_num; i++) {
        if (side->blks[i].page != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
            ka_mm_unpin_user_page(side->blks[i].page);
#else
            ka_mm_put_page(side->blks[i].page);
#endif
        } else {
            break;
        }
        if (is_in_softirq == false) {
            devmm_try_cond_resched(&stamp);
        }
    }
}

/* Only use in agentSmmu scene */
int devmm_vm_pa_to_pm_pa(u32 devid, u64 *paddr, u64 num, u64 *out_paddr, u64 out_num)
{
    u32 stamp = (u32)ka_jiffies;
    u64 i;
    int ret;

    for (i = 0; ((i < num) && (i < out_num));) {
        u64 real_num  = ka_base_min_t(u64, num - i, DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM);

        ret = devdrv_smmu_iova_to_phys(devid, &paddr[i], real_num, &out_paddr[i]);
        if (ret != 0) {
            devmm_drv_err("Can not transfer iova to phys. (devid=%u; i=%llu; num=%llu)\n", devid, i, num);
            return ret;
        }
        i += real_num;
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

int devmm_vm_pa_blks_to_pm_pa_blks(u32 devid, struct devmm_dma_block *blks, u64 num, struct devmm_dma_block *out_blks)
{
    u64 iova_list[DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM];
    u32 stamp = (u32)ka_jiffies;
    u64 i, j;
    int ret;

    for (i = 0; i < num;) {
        u64 real_num = ka_base_min_t(u64, num - i, DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM);

        for (j = 0; j < real_num; ++j) {
            iova_list[j] = blks[i + j].pa;
        }

        ret = devmm_vm_pa_to_pm_pa(devid, iova_list, real_num, iova_list, real_num);
        if (ret != 0) {
            return ret;
        }

        for (j = 0; j < real_num; ++j) {
            out_blks[i + j].pa = iova_list[j];
        }
        i += real_num;
        devmm_try_cond_resched(&stamp);
    }
    return 0;
}

static int devmm_make_host_pa_node_list(struct devmm_mem_copy_convrt_para *para, struct devmm_svm_process *svm_proc,
    struct devmm_memory_attributes *attr, struct devmm_copy_side *side)
{
    struct devmm_copy_res *res = para->res;
    u32 i, merg_idx, host_flag;
    int ret, write;

    side->blk_page_size = KA_MM_PAGE_SIZE;
    side->num = (u32)devmm_get_pagecount_by_size(attr->va, para->count, KA_MM_PAGE_SIZE);

    ret = devmm_get_host_phy_mach_flag((u32)res->dev_id, &host_flag);
    if (ret != 0) {
        return ret;
    }

    if (side->num > side->blks_num) {
        devmm_drv_err("Block num too big. (num=%d; blks_num=%d)\n", side->num, side->blks_num);
        return -EINVAL;
    }

    write = ((para->direction == DEVMM_COPY_HOST_TO_DEVICE) && (para->need_write == false)) ? 0 : 1;
    if (attr->is_svm) {
        res->pin_flg = DEVMM_UNPIN_PAGES;
        ret = devmm_svm_addr_pa_list_get(svm_proc, attr->va, para->count, side);
        if (ret == 0) {
            res->pin_flg = DEVMM_PIN_PAGES;
        }
    } else {
        res->pin_flg = DEVMM_USER_PIN_PAGES;
        ret = devmm_get_non_svm_addr_pa_list(svm_proc, attr->va, para->count, side, write);
    }

    if (ret != 0) {
        devmm_drv_err("Get pa list failed. (pin_flg=%d; va=%llx)\n", res->pin_flg, attr->va);
        return ret;
    }

    /* clear blks page ptr for later unpin page check */
    if (side->num < side->blks_num) {
        side->blks[side->num].page = NULL;
        side->blks[side->num].pa = 0;
        side->blks[side->num].dma = 0;
        side->blks[side->num].sz = 0;
    }

    if (devmm_is_hccs_vm_scene((u32)res->dev_id, host_flag)) {
        ret = devmm_vm_pa_blks_to_pm_pa_blks((u32)res->dev_id, side->blks, side->num, side->blks);
        if (ret != 0) {
            return ret;
        }
    }

    for (merg_idx = 0, i = 0; i < side->num; i++) {
        devmm_merg_blk(side->blks, i, &merg_idx);
    }
    side->num = merg_idx;

    return ret;
}

void devmm_clear_host_pa_node_list(int pin_flg, struct devmm_copy_side *side)
{
    if (pin_flg == DEVMM_PIN_PAGES) {
        devmm_svm_addr_pa_list_put(side);
    } else if (pin_flg == DEVMM_USER_PIN_PAGES) {
        devmm_put_non_svm_addr_pa_list(side);
    }
}

static INLINE void devmm_fill_sva_dma_blk_node(u64 va, u64 sz,
    struct devmm_memory_attributes *attr, struct devmm_copy_side *side)
{
    u64 aligned_va, aligned_size;

    side->blk_page_size = (attr->is_svm_huge) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;

    aligned_va = ka_base_round_down(va, side->blk_page_size);
    aligned_size = ka_base_round_up(sz + (va - aligned_va), side->blk_page_size);

    side->num = 1;
    side->blks[0].pa = aligned_va;
    side->blks[0].sz = aligned_size;
    side->blks[0].ssid = attr->ssid;
}

static int devmm_fill_dma_blk_node(struct devmm_mem_copy_convrt_para *para, struct devmm_svm_process *svm_proc,
    struct devmm_memory_attributes *attr, struct devmm_copy_side *side)
{
    struct devmm_memory_attributes *tmp_attr = attr;
    struct devmm_svm_process *owner_proc = svm_proc;
    struct devmm_memory_attributes owner_attr;
    struct devmm_copy_res *res = para->res;
    struct devmm_page_query_arg query_arg = {{0}};
    struct devmm_svm_heap *heap = NULL;
    int tmp_addr_type;
    int ret, i;

    tmp_addr_type = (tmp_attr->devid == res->dev_id) ? DEVMM_ADDR_TYPE_DMA : DEVMM_ADDR_TYPE_PHY;
    if (tmp_attr->is_ipc_open) {
        ret = devmm_ipc_get_owner_proc_attr(svm_proc, tmp_attr, &owner_proc, &heap, &owner_attr);
        if (ret != 0) {
            devmm_drv_err("Get ipc owner attributes failed. (va=%llx)\n", tmp_attr->va);
            return ret;
        }

        devmm_drv_debug("Get ipc owner attributes success. (attr_va=%llx; pid=%d; owner_attr_va=%llx)\n",
                        tmp_attr->va, owner_proc->process_id.hostpid, owner_attr.va);

        tmp_attr = &owner_attr;
        tmp_addr_type = (tmp_attr->devid == res->dev_id) ? DEVMM_ADDR_TYPE_DMA : DEVMM_ADDR_TYPE_PHY;
        tmp_attr->copy_use_va = false;
    }
    if (tmp_attr->is_mem_import) {
        /* import process memcpy, use export process's dma addr and it's devid */
        tmp_addr_type = DEVMM_ADDR_TYPE_DMA;
    }

    devmm_get_device_cpy_blk_num(tmp_attr->is_svm_huge, para->count, side);
    query_arg.process_id.hostpid = owner_proc->process_id.hostpid;
    query_arg.process_id.vfid = (uint16_t)res->fid;
    query_arg.bitmap = tmp_attr->bitmap;
    query_arg.dev_id = tmp_attr->devid;
    query_arg.va = tmp_attr->va;
    query_arg.size = para->count;
    query_arg.offset = tmp_attr->va & (tmp_attr->page_size - 1);
    query_arg.page_size = tmp_attr->page_size;
    query_arg.msg_id = (para->create_msg) ? DEVMM_CHAN_PAGE_CREATE_QUERY_H2D_ID : DEVMM_CHAN_PAGE_QUERY_H2D_ID;
    query_arg.addr_type = (u32)tmp_addr_type;
    query_arg.logical_devid = tmp_attr->logical_devid;
    query_arg.page_insert_dev_id = devmm_get_page_insert_dev_id(owner_proc->process_id.vm_id,
        tmp_attr->devid, tmp_attr->logical_devid, (u32)res->fid);
    if (tmp_attr->copy_use_va == true) {
        ret = devmm_query_page_by_msg(owner_proc, query_arg, NULL, &side->num);
    } else {
        ret = devmm_query_page_by_msg(owner_proc, query_arg, side->blks, &side->num);
        for (i = 0; (i < side->num) && (ret == 0); i++) {
            side->blks[i].ssid = 0;
        }
    }

    if (svm_proc != owner_proc) {
        devmm_ipc_put_owner_proc_attr(owner_proc, heap);
    }

    if ((ret != 0) || (side->num == 0)) {
        ret = (ret != 0) ? ret : -EINVAL;
        devmm_drv_err("Query fail. (num=%d; ret=%d)\n", side->num, ret);
        return ret;
    }

    return ret;
}

static int devmm_make_device_addr_node_list(struct devmm_mem_copy_convrt_para *para, struct devmm_svm_process *svm_proc,
    struct devmm_memory_attributes *attr, struct devmm_copy_side *side)
{
    if (attr->copy_use_va) {
        int ret;
        if (para->create_msg) {
            ret = devmm_fill_dma_blk_node(para, svm_proc, attr, side);
            if (ret != 0) {
                return ret;
            }
            devmm_fill_sva_dma_blk_node(attr->va, para->count, attr, side);
        } else if (attr->is_ipc_open) {
            /*
             * if support sva copy, converted to ipc_create:
             * a. h2d or d2h: device va is ipc_open;
             * b. d2d: src addr converted to ipc_create.
             */
            struct devmm_memory_attributes owner_attr = {0};
            struct devmm_svm_process *owner_proc = NULL;
            struct devmm_svm_heap *heap = NULL;
            ret = devmm_ipc_get_owner_proc_attr(svm_proc, attr, &owner_proc, &heap, &owner_attr);
            if (ret != 0) {
                devmm_drv_err("Get ipc owner attributes failed. (va=%llx)\n", attr->va);
                return ret;
            }
            devmm_fill_sva_dma_blk_node(owner_attr.va, para->count, &owner_attr, side);
            devmm_ipc_put_owner_proc_attr(owner_proc, heap);
        } else {
            devmm_fill_sva_dma_blk_node(attr->va, para->count, attr, side);
        }
        return 0;
    } else {
        return devmm_fill_dma_blk_node(para, svm_proc, attr, side);
    }
}

/* devdrv_dma_link_prepare may return fail, if dma_node_num is too large */
static struct devdrv_dma_prepare *devmm_dma_link_prepare(u32 devid,
    struct devdrv_dma_node *dma_node, u32 dma_node_num, u32 *prepared_num)
{
    struct devdrv_dma_prepare *dma_prepare = NULL;
    u32 i;

    for (i = 0; i < DMA_LINK_PREPARE_TRY_TIMES; i++) {
        u32 tmp_num = dma_node_num >> i;
        if (tmp_num == 0) {
            return NULL;
        }
        dma_prepare = devdrv_dma_link_prepare(devid, DEVDRV_DMA_DATA_TRAFFIC,
            dma_node, tmp_num, DEVDRV_DMA_DESC_FILL_FINISH);
        if (dma_prepare != NULL) {
            *prepared_num = tmp_num;
            break;
        }
        devmm_drv_warn("Retry alloc sq&cq address. (dma_try_alloc_num=%u)\n", tmp_num);
    }
    return dma_prepare;
}

static u32 devmm_get_dma_node_num_by_convrt_para(struct devmm_mem_convrt_addr_para *convrt_para, u32 height)
{
    struct devmm_copy_res *res = NULL;
    u32 total_node_num = 0;
    u32 i;

    for (i = 0; i < height; i++) {
        res = (struct devmm_copy_res *)convrt_para[i].dmaAddr.phyAddr.priv;
        total_node_num += res->dma_node_num;
    }
    return total_node_num;
}

static int devmm_fill_dma_node_by_convrt_para(struct devmm_mem_convrt_addr_para *convrt_para, u32 height,
    struct devdrv_dma_node *dma_node, u32 dma_node_num)
{
    u32 stamp = (u32)ka_jiffies;
    u32 copyed_num = 0;
    u32 i;
    int ret;

    for (i = 0; i < height; i++) {
        struct devmm_copy_res *res = (struct devmm_copy_res *)convrt_para[i].dmaAddr.phyAddr.priv;

        ret = memcpy_s((void *)dma_node + sizeof(struct devdrv_dma_node) * copyed_num,
            sizeof(struct devdrv_dma_node) * (dma_node_num - copyed_num),
            (const void *)res->dma_node, sizeof(struct devdrv_dma_node) * res->dma_node_num);
        if (ret != 0) {
            return ret;
        }
        copyed_num += res->dma_node_num;
        devmm_try_cond_resched(&stamp);
    }
    return 0;
}

static int devmm_dma_node_prepare(struct devmm_mem_convrt_addr_para *convrt_para, u32 height,
    struct devdrv_dma_node **dma_node, u32 *dma_node_num)
{
    struct devdrv_dma_node *tmp = NULL;
    struct devmm_copy_res *res = NULL;
    u32 num;
    int ret;

    if (height == 1) {
        res = (struct devmm_copy_res *)convrt_para[0].dmaAddr.phyAddr.priv;
        *dma_node = res->dma_node;
        *dma_node_num = res->dma_node_num;
    } else {
        num = devmm_get_dma_node_num_by_convrt_para(convrt_para, height);
        tmp = devmm_kvzalloc(sizeof(struct devdrv_dma_node) * num);
        if (tmp == NULL) {
            return -ENOMEM;
        }

        ret = devmm_fill_dma_node_by_convrt_para(convrt_para, height, tmp, num);
        if (ret != 0) {
            devmm_kvfree(tmp);
            return ret;
        }

        *dma_node = tmp;
        *dma_node_num = num;
    }
    return 0;
}

static void devmm_dma_node_free(u32 height, struct devdrv_dma_node *dma_node)
{
    if (height != 1) {
        devmm_kvfree(dma_node);
    }
}

STATIC int devmm_convert2d_make_sqcq_addr(struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para, struct devmm_copy_res *res)
{
    struct devdrv_dma_node *dma_node = NULL;
    u32 height = (u32)arg->data.convrt_para.height;
    u32 dma_node_num;
    int ret;

    ret = devmm_dma_node_prepare(convrt_para, height, &dma_node, &dma_node_num);
    if (ret != 0) {
        return ret;
    }

    res->dma_prepare = devmm_dma_link_prepare(arg->head.devid, dma_node, dma_node_num, &res->dma_node_alloc_num);
    devmm_dma_node_free(height, dma_node);
    return ((res->dma_prepare == NULL) ? -ENOMEM : 0);
}

STATIC void devmm_convert2d_make_res_list(struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para)
{
    u64 height = arg->data.convrt_para.height;
    struct devmm_copy_res *res_node = NULL;
    struct devmm_copy_res *res = NULL;
    u64 i;

    res = (struct devmm_copy_res *)convrt_para[0].dmaAddr.phyAddr.priv;
    for (i = 1; i < height; i++) {
        res_node = res;
        res = (struct devmm_copy_res *)convrt_para[i].dmaAddr.phyAddr.priv;
        res_node->next = res;
    }
}

static void devmm_convert2d_get_fixed_size(struct devmm_mem_convrt_addr_para *convrt2d_para,
    struct devmm_mem_convrt_addr_para *convrt_para, struct devmm_copy_res *convrt_res)
{
    u64 height = convrt2d_para->height;
    struct devmm_copy_res *res = NULL;
    u32 dma_alloc_num, dma_left_num;
    u32 dma_node_num = 0;
    u64 i;

    convrt2d_para->dmaAddr.fixed_size = 0;
    dma_alloc_num = convrt_res->dma_node_alloc_num;
    for (i = 0; i < height; i++) {
        res = (struct devmm_copy_res *)convrt_para[i].dmaAddr.phyAddr.priv;
        dma_node_num += res->dma_node_num;
        if (dma_node_num <= dma_alloc_num) {
            convrt2d_para->dmaAddr.fixed_size += (u32)convrt2d_para->len;
        } else {
            dma_left_num = dma_alloc_num - dma_node_num + res->dma_node_num;
            for (i = 0; i < dma_left_num; i++) {
                convrt2d_para->dmaAddr.fixed_size += res->dma_node[i].size;
            }
            break;
        }
    }
}

static u32 devmm_get_dma_link_copy_flag(struct devmm_ioctl_arg *arg, struct devmm_copy_res *res)
{
    if (devmm_dev_capability_convert_support_offset(arg->head.devid)) {
        return ((res->cpy_len <= KA_MM_PAGE_SIZE) && (res->dma_node_num == 1) && (res->height == 1)) ? 0 : 1;
    } else {
        return 1;
    }
}

static int devmm_make_convert2d_para(struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para)
{
    struct devmm_mem_convrt_addr_para *convrt2d_para = &arg->data.convrt_para;
    struct devmm_copy_res *res = NULL;
    u32 dma_link_copy;

    devmm_convert2d_make_res_list(arg, convrt_para);

    res = (struct devmm_copy_res *)convrt_para[0].dmaAddr.phyAddr.priv;
    res->spitch = convrt2d_para->spitch;
    res->dpitch = convrt2d_para->dpitch;
    res->height = convrt2d_para->height;

    dma_link_copy = devmm_get_dma_link_copy_flag(arg, res);
    if (dma_link_copy == 0) {
        convrt2d_para->dmaAddr.phyAddr.src = (void *)(uintptr_t)res->dma_node[0].src_addr;
        convrt2d_para->dmaAddr.phyAddr.dst = (void *)(uintptr_t)res->dma_node[0].dst_addr;
        convrt2d_para->dmaAddr.phyAddr.len = res->dma_node[0].size;
        convrt2d_para->dmaAddr.phyAddr.flag = 0;
        convrt2d_para->dmaAddr.fixed_size = (u32)res->cpy_len;
    } else {
        int ret;
        convrt2d_para->dmaAddr.phyAddr.flag = 0;
        ret = devmm_convert2d_make_sqcq_addr(arg, convrt_para, res);
        if (ret != 0) {
            devmm_drv_err("Convrt2d make sqcq fail. (address_num=%llu)\n", convrt2d_para->height);
            return ret;
        }
        convrt2d_para->dmaAddr.phyAddr.src = (void *)(uintptr_t)res->dma_prepare->sq_dma_addr;
        convrt2d_para->dmaAddr.phyAddr.dst = (void *)(uintptr_t)res->dma_prepare->cq_dma_addr;

        convrt2d_para->dmaAddr.phyAddr.len = res->dma_node_alloc_num;
        convrt2d_para->dmaAddr.phyAddr.flag = 1;
        devmm_convert2d_get_fixed_size(convrt2d_para, convrt_para, res);
    }
    res->fixed_size = convrt2d_para->dmaAddr.fixed_size;

    return 0;
}

static int devmm_try_get_idle_convert_mem(struct devmm_svm_process *svm_pro,
    struct devmm_ioctl_arg *arg, u64 *offset, u32 *index)
{
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;
    u32 stamp = (u32)ka_jiffies;
    u32 num;
    int ret;

    svm_id_inst_pack(&id_inst, arg->head.devid, arg->head.vfid);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        devmm_drv_err("Dev uninit instance. (devid=%u; vfid=%u)", id_inst.devid, id_inst.vfid);
        return -ENODEV;
    }

    while (1) {
        ret = devmm_set_convert_ka_dma_addr_to_device(svm_pro, arg, offset, index);
        if (ret != -ENODATA) {
            break;
        }

        devmm_convert_nodes_subres_recycle_by_dev_res_mng(dev_res_mng, &num);
        if (num == 0) {
            break;
        }
        devmm_try_cond_resched(&stamp);
    }

    if (ret != 0) {
        devmm_drv_err("Get idle offset and index failed. (ret=%d)", ret);
    }

    devmm_dev_res_mng_put(dev_res_mng);
    return ret;
}

static void devmm_convert_node_para_pack(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    u64 offset, u32 index, struct devmm_convert_node_info *info)
{
    info->dma_addr = arg->data.convrt_para.dmaAddr;
    info->src_va = (u64)arg->data.convrt_para.pSrc;
    info->dst_va = (u64)arg->data.convrt_para.pDst;
    info->spitch = arg->data.convrt_para.spitch;
    info->dpitch = arg->data.convrt_para.dpitch;
    info->width = arg->data.convrt_para.len;
    info->height = arg->data.convrt_para.height;
    info->fixed_size = arg->data.convrt_para.dmaAddr.fixed_size;
    info->offset = offset;
    info->index = index;
    info->host_pid = svm_proc->process_id.hostpid;
    svm_id_inst_pack(&info->id_inst, arg->head.devid, arg->head.vfid);
}

int devmm_convert2d_proc_inner(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para)
{
    struct devmm_convert_node_info info;
    u64 offset = 0;
    u32 index = 0;
    int ret;

    /* first res is used for the list head */
    arg->data.convrt_para.dmaAddr.phyAddr.priv = convrt_para[0].dmaAddr.phyAddr.priv;
    ret = devmm_make_convert2d_para(arg, convrt_para);
    if (ret != 0) {
        devmm_drv_err("Make convert2d para fail. (ret=%d)\n", ret);
        goto devmm_free_convrt_para;
    }

    if (devmm_dev_capability_convert_support_offset(arg->head.devid)) {
        ret = devmm_try_get_idle_convert_mem(svm_pro, arg, &offset, &index);
        if (ret != 0) {
            goto devmm_free_convrt_para;
        }
    }

    devmm_convert_node_para_pack(svm_pro, arg, offset, index, &info);
    ret = devmm_convert_node_create(svm_pro, &info);
    if (ret != 0) {
        devmm_drv_err("Create convert_node failed. (ret=%d)\n", ret);
        goto clear_addr;
    }
    return 0;

clear_addr:
    if (devmm_dev_capability_convert_support_offset(arg->head.devid)) {
        devmm_clear_one_convert_dma_addr(arg->head.devid, arg->head.vfid, index);
    }
devmm_free_convrt_para:
    (void)devmm_destroy_dma_addr(&arg->data.convrt_para.dmaAddr);

    return ret;
}

static void devmm_convert2d_destroy_inner(struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para)
{
    u64 height = arg->data.convrt_para.height;
    struct devmm_copy_res *res = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < height; i++) {
        res = (struct devmm_copy_res *)convrt_para[i].dmaAddr.phyAddr.priv;
        devmm_free_raw_dmanode_list(res);
        devmm_free_copy_mem(res);
        devmm_try_cond_resched(&stamp);
    }
}

int devmm_convert2d_proc(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para)
{
    u32 destroy_flag = convrt_para[0].destroy_flag;
    int ret = 0;

    if (destroy_flag != 0) {
        devmm_convert2d_destroy_inner(arg, convrt_para);
    } else {
        ret = devmm_convert2d_proc_inner(svm_pro, arg, convrt_para);
    }
    return ret;
}


static u64 devmm_get_occupied_blks_num_by_size(struct devmm_dma_block *blks, u64 num, u64 *left_size)
{
    u64 i;

    for (i = 0; i < num; ++i) {
        if (*left_size <= blks[i].sz) {
            break;
        }
        *left_size -= blks[i].sz;
    }

    return i + 1;
}

static void devmm_make_single_dma_blk_list_by_cache(struct devmm_copy_side *side,
    struct devmm_register_dma_node *node, u64 va, u64 size)
{
    u64 offset, offset_size; /* Offset is within the range of node->num */
    u64 align_va = ka_base_round_down(va, KA_MM_PAGE_SIZE);
    u64 blk_va = node->align_va; /* node->align_va is round_down, blk_va represent the starting address of each block */
    u64 left_size = ka_base_round_up(size + (va - align_va), KA_MM_PAGE_SIZE);
    /* Each node blk size greater or equal to KA_MM_PAGE_SIZE, so occupied_blks_num will less than side blks_num */
    u32 occupied_blks_num;

    side->blk_page_size = KA_MM_PAGE_SIZE;
    side->register_dma_node = node; /* use for clear dma blk */

    for (offset = 0; offset < node->num; ++offset) {
        if ((blk_va + node->blks[offset].sz) > align_va) {
            break;
        }
        blk_va += node->blks[offset].sz;
    }

    offset_size = align_va - blk_va;
    side->blks[0].dma = node->blks[offset].dma + offset_size;
    side->blks[0].pa = node->blks[offset].pa + offset_size;
    side->blks[0].page = node->blks[offset].page;
    if ((offset_size + left_size) <= node->blks[offset].sz) {
        side->blks[0].sz = left_size;
        side->num = 1;
    } else {
        side->blks[0].sz = node->blks[offset].sz - offset_size;
        left_size -= side->blks[0].sz;
        offset++;
        occupied_blks_num = (u32)devmm_get_occupied_blks_num_by_size(&node->blks[offset],
            node->num - offset, &left_size);
        (void)memcpy_s(&side->blks[1], sizeof(struct devmm_dma_block) * occupied_blks_num,
            &node->blks[offset], sizeof(struct devmm_dma_block) * occupied_blks_num);
        side->blks[occupied_blks_num].sz = left_size;
        side->num = occupied_blks_num + 1;
    }

    devmm_drv_debug("Get host pages cache node. (va=0x%llx; size=%llu; node_va=0x%llx; node_size=%llu;"
        "blks_num=%u; dev_id=%d)\n", va, size, node->align_va, node->align_size, side->num, node->devid);
}

static int devmm_make_single_dma_blk_list_by_dma_map(struct devmm_mem_copy_convrt_para *para,
    struct devmm_svm_process *svm_proc, struct devmm_copy_res *res,
    struct devmm_copy_side *side, struct devmm_memory_attributes *attr)
{
    int ret;

    ret = devmm_make_host_pa_node_list(para, svm_proc, attr, side);
    if (ret != 0) {
        devmm_drv_err("Make host address node list failed. (dev_id=%d; va=0x%llx)\n", res->dev_id, attr->va);
        return ret;
    }

    ret = devmm_pa_node_list_dma_map((u32)res->dev_id, side);
    if (ret != 0) {
        devmm_drv_err("Dma map failed. (dev_id=%d; va=0x%llx)\n", res->dev_id, attr->va);
        devmm_clear_host_pa_node_list(res->pin_flg, side);
        return ret;
    }
    return 0;
}

static int devmm_make_single_dma_blk_list(struct devmm_mem_copy_convrt_para *para, struct devmm_svm_process *svm_proc,
    struct devmm_copy_res *res, struct devmm_copy_side *side, struct devmm_memory_attributes *attr)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_register_dma_node *node = NULL;
    int ret;

    if (devmm_is_master(attr)) {
        if (attr->is_mem_import && (attr->mem_share_devid != uda_get_host_id())) {
#ifndef EMU_ST
            return -EOPNOTSUPP;
#endif
        }

        side->side_type = DEVMM_DMA_CPY_SIDE_HOST;

        /* The request from the virtual machine, the host address has been converted
           when the physical machine receives the message */
        if (res->vm_copy_flag == true) {
            return 0;
        }
        node = devmm_register_dma_node_get(&master_data->register_dma_mng[res->dev_id],
            attr->va, para->count);
        if (node != NULL) {
            devmm_make_single_dma_blk_list_by_cache(side, node, attr->va, (u64)para->count);
            return 0;
        } else {
            return devmm_make_single_dma_blk_list_by_dma_map(para, svm_proc, res, side, attr);
        }
    } else {
        side->side_type = DEVMM_DMA_CPY_SIDE_DEVICE;
        /* p2p memcpy, dst addr use pa, then find its bar, dma map src dev dma addr */
        ret = devmm_make_device_addr_node_list(para, svm_proc, attr, side);
        if (ret != 0) {
            devmm_drv_err("Make device address node list failed. (dev_id=%d; va=0x%llx)\n", res->dev_id, attr->va);
            return ret;
        }
    }

    return 0;
}

static void devmm_clear_single_dma_blk_list(struct devmm_copy_res *res, struct devmm_copy_side *side)
{
    if (side->side_type == DEVMM_DMA_CPY_SIDE_HOST) {
        /* The request from the virtual machine, the host address has been converted
           when the physical machine receives the message */
        if (res->vm_copy_flag == true) {
            return;
        }

        if (side->register_dma_node != NULL) {
            devmm_register_dma_node_put(side->register_dma_node);
        } else {
            devmm_pa_node_list_dma_unmap((u32)res->dev_id, side);
            devmm_clear_host_pa_node_list(res->pin_flg, side);
        }
    }
}

static int devmm_make_raw_dmanode_list(struct devmm_mem_copy_convrt_para *para,
                                       struct devmm_svm_process *svm_proc,
                                       struct devmm_copy_res *res,
                                       struct devmm_memory_attributes *src_attr,
                                       struct devmm_memory_attributes *dst_attr)
{
    int ret;

    ret = devmm_make_single_dma_blk_list(para, svm_proc, res, &res->from, src_attr);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Fill source dma blk list fail.\n");
        return ret;
    }

    ret = devmm_make_single_dma_blk_list(para, svm_proc, res, &res->to, dst_attr);
    if (ret != 0) {
        devmm_clear_single_dma_blk_list(res, &res->from);
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Fill destination dma blk list fail.\n");
        return ret;
    }

    return 0;
}

void devmm_free_raw_dmanode_list(struct devmm_copy_res *res)
{
#ifdef CFG_FEATURE_VFIO
    if (res->vm_copy_flag == true) {
        devmm_clear_pm_host_addr(res);
        return;
    }
#endif
    devmm_clear_single_dma_blk_list(res, &res->from);
    devmm_clear_single_dma_blk_list(res, &res->to);
}

static inline int devmm_async_task_record(struct devmm_svm_process *svm_proc,
    struct devmm_dma_copy_task *copy_task, int *idr_id)
{
    struct devmm_svm_proc_master *master_data = NULL;
    int ret;

    master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    ret = devmm_idr_alloc(&master_data->async_copy_record.task_idr, (char *)copy_task, idr_id);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static inline void *devmm_async_task_remove(struct devmm_svm_process *svm_proc, int idr)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    return devmm_idr_get_and_remove(&master_data->async_copy_record.task_idr, idr, NULL);
}

static inline void devmm_async_task_record_inc(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    ka_base_atomic_inc(&(master_data->async_copy_record.task_cnt[devid]));
}

static inline void devmm_async_task_record_dec(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    atomic_dec_if_positive(&(master_data->async_copy_record.task_cnt[devid]));
}

static void devmm_dma_copy_task_set_para(struct devmm_mem_copy_convrt_para *para,
    struct devmm_dma_copy_task *copy_task)
{
    copy_task->task_id = para->task_id;
    ka_base_atomic_set(&copy_task->finish_num, 0);
    ka_base_atomic_set(&copy_task->occupy_num, 1);
    copy_task->task_mode = para->task_mode;
    copy_task->dev_id = para->dev_id;
    copy_task->async_status = 0;
    copy_task->submit_status = 0;
    copy_task->src = para->src;
    copy_task->dst = para->dst;
    copy_task->size = para->count;
    copy_task->cpy_ret = 0;
    copy_task->submit_num = 0;

    para->copy_task = copy_task;
}

static int devmm_dma_async_copy_task_add(struct devmm_svm_process *svm_proc,
    struct devmm_mem_copy_convrt_para *para)
{
    struct devmm_dma_copy_task *copy_task = NULL;

    copy_task = (struct devmm_dma_copy_task *)devmm_kvalloc(sizeof(struct devmm_dma_copy_task), 0);
    if (copy_task == NULL) {
        devmm_drv_err("Alloc task struct fail. (src=0x%llx, dst=0x%llx, id=%u)\n",
            para->src, para->dst, para->task_id);
        return -ENOMEM;
    }
    if (para->task_mode == DEVMM_CPY_ASYNC_API_MODE) {
        int ret;
        ret = devmm_async_task_record(svm_proc, copy_task, &para->task_query_id);
        if (ret != 0) {
            devmm_kvfree(copy_task);
            return ret;
        }
        devmm_async_task_record_inc(svm_proc, para->dev_id);
    }

    devmm_dma_copy_task_set_para(para, copy_task);

    return 0;
}

void devmm_task_del(struct devmm_dma_copy_task *copy_task)
{
    if (copy_task == NULL) {
        return;
    }

    if (copy_task->task_mode == DEVMM_CPY_ASYNC_API_MODE) {
        int occupy_num = ka_base_atomic_dec_return(&copy_task->occupy_num);
        if (ka_unlikely(occupy_num != 0)) {
            return;
        }
    }
    devmm_kvfree(copy_task);
}

static inline void devmm_set_async_api_cpy_task_result(struct devmm_dma_copy_task *copy_task)
{
    copy_task->async_status = (copy_task->cpy_ret != 0) ? DEVMM_ASYNC_CPY_ERR_VALUE : DEVMM_ASYNC_CPY_FINISH_VALUE;
}

/* will call by pcie irq tasklet, can not sleep */
static void devmm_dma_async_callback(void *data, u32 trans_id, u32 status)
{
    struct devmm_copy_res *res = (struct devmm_copy_res *)data;
    struct devmm_dma_copy_task *copy_task = res->copy_task; /* async copy copy task can not null */
    struct devmm_svm_process *svm_proc = NULL;
    u32 task_mode = copy_task->task_mode;
    u32 dev_id = copy_task->dev_id;

    copy_task->cpy_ret |= status;
    if ((task_mode != DEVMM_CPY_ASYNC_API_MODE) && (task_mode != DEVMM_CPY_CONVERT_MODE)) {
        ka_base_atomic_inc(&copy_task->finish_num);
        devmm_free_raw_dmanode_list(res);
        devmm_free_copy_mem(res);
    } else {
        u32 finish_num, submit_num, occupy_num;

        occupy_num = (u32)ka_base_atomic_dec_return(&copy_task->occupy_num);
        submit_num = copy_task->submit_num;
        devmm_isb();  /* to protect devmm_dma_wait_task_finish free copy_task when submit error */
        finish_num = (u32)ka_base_atomic_inc_return(&copy_task->finish_num);
        if ((submit_num != 0) && (finish_num >= submit_num)) {
            svm_proc = res->svm_pro;
            devmm_set_async_api_cpy_task_result(copy_task);
        }
        if (task_mode == DEVMM_CPY_ASYNC_API_MODE) {
            if (occupy_num == 0) {
                devmm_kvfree(copy_task);
            }
            devmm_free_raw_dmanode_list(res);
            devmm_free_copy_mem(res);
        }

        /*
         * If async_task_cnt dec to 0, copy_task will be freed by proc release,
         * for async api mode, if callback is concurrent with proc release, copy_task may be double free,
         * so should ensure that async_task_cnt_dec is after the free copy_task.
         */
        devmm_isb();
        if ((submit_num != 0) && (finish_num >= submit_num)) {
            devmm_async_task_record_dec(svm_proc, dev_id);
        }
    }
}

void devmm_wait_task_finish(u32 dev_id, ka_atomic_t *finish_num, int submit_num)
{
    u32 wait_times = 0;

    while ((submit_num > ka_base_atomic_read(finish_num)) && (wait_times++ < DMA_COPY_TASK_WAIT_TIMES)) {
        if (devmm_is_dma_link_abnormal(dev_id)) {
#ifndef EMU_ST
            return;
#endif
        }

        ka_system_usleep_range(DMA_COPY_TASK_WAIT_SLEEP_MIN_TIME, DMA_COPY_TASK_WAIT_SLEEP_MAX_TIME);
    }
}

static int devmm_dma_wait_task_finish(struct devmm_copy_res *res, struct devmm_mem_copy_convrt_para *para)
{
    struct devmm_dma_copy_task *copy_task = res->copy_task;
    struct devmm_svm_process *svm_proc = res->svm_pro;
    bool need_delete_task = true;
    int ret;

    if (copy_task == NULL) {
        return -EINVAL;
    }

    devmm_wait_task_finish(para->dev_id, &copy_task->finish_num, (int)para->seq);
    if (para->task_query_id != DEVMM_SVM_INVALID_INDEX) {
        void *tmp = NULL;

        tmp = devmm_async_task_remove(svm_proc, para->task_query_id);
        if (tmp == NULL) {
            need_delete_task = false;
            ret = -EINVAL;
            devmm_drv_err("Remove fail. (id=%d)\n", para->task_query_id);
        }
        para->task_query_id = DEVMM_SVM_INVALID_INDEX;
        devmm_async_task_record_dec(svm_proc, para->dev_id);
    }

    if (need_delete_task) {
        ret = (int)copy_task->cpy_ret;
        devmm_task_del(res->copy_task);
    }
    res->copy_task = NULL;
    para->copy_task = NULL;

    return ret;
}

static int devmm_set_async_cpy_para(struct devmm_svm_process *svm_proc,
    struct devmm_mem_copy_convrt_para *para, struct devmm_copy_res *res)
{
    if (para->seq == 0) {
        int ret;
        ret = devmm_dma_async_copy_task_add(svm_proc, para);
        if (ret != 0) {
            return ret;
        }
    }

    if (para->last_seq_flag == true) {
        para->copy_task->submit_num = para->seq + 1;
    }
    res->copy_task = para->copy_task;

    return 0;
}

static inline int devmm_copy_get_dma_instance(int task_id)
{
    return (task_id == -1) ? 0 : task_id;
}

static int devmm_async_copy_process(struct devmm_copy_res *res,
    struct devmm_mem_copy_convrt_para *para)
{
    int instance = devmm_copy_get_dma_instance((int)res->task_id);
    int ret;

    if ((para->last_seq_flag == true) && (para->task_mode == DEVMM_CPY_SYNC_MODE)) {
        ret = devmm_dma_sync_link_copy_limit(res, instance);
        if (ret != 0) {
            devmm_drv_err("Dma synchronize copy fail. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
                          ret, para->src, para->dst, para->count);
            return ret;
        }
        ret = devmm_dma_wait_task_finish(res, para);
        if (ret != 0) {
            devmm_drv_err("Dma asynchronous task fail. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
                ret, para->src, para->dst, para->count);
            return ret;
        }
        devmm_free_raw_dmanode_list(res);
        devmm_free_copy_mem(res);
    } else {
        ret = devmm_dma_async_link_copy(res, instance, (void*)res, devmm_dma_async_callback);
        if (ret != 0) {
            devmm_drv_err("Dma asynchronous copy fail. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
                          ret, para->src, para->dst, para->count);
            return ret;
        }
        if (para->last_seq_flag) {
            /* cannot use res->copy_task, maybe free by devmm_dma_async_callback */
            para->copy_task->submit_status = DEVMM_ASYNC_SUBMIT_FINISH_VALUE;
        }
    }

    return 0;
}

static inline int devmm_sync_copy_process(struct devmm_copy_res *res,
    struct devmm_mem_copy_convrt_para *para)
{
    int ret;

    ret = devmm_dma_sync_link_copy((u32)res->dev_id, (u32)res->fid, res->dma_node, res->dma_node_num);
    if (ret != 0) {
        devmm_drv_err("Dma copy fail. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
            ret, para->src, para->dst, para->count);
        return ret;
    }
    devmm_free_raw_dmanode_list(res);
    devmm_free_copy_mem(res);

    return 0;
}

static void devmm_memcpy_exception_handle(struct devmm_copy_res *res, struct devmm_mem_copy_convrt_para *para)
{
    if (res->cpy_mode != DEVMM_DMA_ASYNC_MODE) {
        return;
    }
    devmm_drv_warn_if(devmm_dma_wait_task_finish(res, para) != 0, "Devmm dma wait task error.\n");

    return;
}

static inline int devmm_dma_cpy_mode_is_sync(struct devmm_mem_copy_convrt_para *para, bool vm_copy_flag)
{
    /* In vm scene, the asynchronous dma mode cannot be used, small packet use synchronous dma */
    return (((para->task_mode == DEVMM_CPY_SYNC_MODE) && ((para->last_seq_flag == true) && (para->seq == 0))) ||
        (vm_copy_flag == true));
}

static inline int devmm_memcpy_get_cpy_mode(struct devmm_mem_copy_convrt_para *para, bool vm_copy_flag)
{
    if (devmm_dma_cpy_mode_is_sync(para, vm_copy_flag) != 0) {
        return DEVMM_DMA_SYNC_MODE;
    } else {
        return DEVMM_DMA_ASYNC_MODE;
    }
}

static void devmm_get_dma_copy_devid(struct devmm_svm_process *svm_proc,
    struct devmm_memory_attributes *attr, int *dev_id, int *vfid)
{
    struct devmm_svm_process *owner_proc = NULL;
    struct devmm_svm_process_id proc_id;
    u64 owner_va;

    *dev_id = (int)attr->devid;
    *vfid = (int)attr->vfid;

    if ((attr->copy_use_va == true) && (attr->is_ipc_open == false)) {
        return;
    }

    if (attr->is_mem_import) {
        *dev_id = (int)attr->mem_share_devid;
        *vfid = 0;
        devmm_drv_debug("Source vaddress is import memory. (va=%llx; devid=%d; vfid=%d)\n", attr->va, *dev_id, *vfid);
    }

    if (attr->is_ipc_open) {
        owner_proc = devmm_ipc_query_owner_info(svm_proc, attr->va, &owner_va, &proc_id, NULL);
        if (owner_proc != NULL) {
            *dev_id = proc_id.devid;
            *vfid = proc_id.vfid;

            devmm_drv_debug("Source vaddress is ipc memory. (va=%llx; devid=%d; vfid=%d)\n", attr->va, *dev_id, *vfid);
        }
    }
}

static void devmm_memcpy_set_buf_info(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para,
    struct devmm_copy_res *res, struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr)
{
    res->cpy_mode = devmm_memcpy_get_cpy_mode(para, res->vm_copy_flag);
    res->svm_pro = svm_proc;
    res->copy_direction = (int)para->direction;
    res->cpy_len = para->count;
    res->task_id = para->task_id;
    res->src_va = para->src;
    res->dst_va = para->dst;
#ifndef DEVMM_HOST_UT
    res->copy_task = para->copy_task;
#endif

    if (res->copy_direction == DEVMM_COPY_HOST_TO_DEVICE) {
        devmm_get_dma_copy_devid(svm_proc, dst_attr, &res->dev_id, &res->fid);
    } else if (res->copy_direction == DEVMM_COPY_DEVICE_TO_HOST) {
        devmm_get_dma_copy_devid(svm_proc, src_attr, &res->dev_id, &res->fid);
    } else {
        devmm_get_dma_copy_devid(svm_proc, dst_attr, &res->dst_dev_id, &res->fid);
        devmm_get_dma_copy_devid(svm_proc, src_attr, &res->dev_id, &res->fid);
    }
    para->dev_id = (u32)res->dev_id;
    para->vfid = (u32)res->fid;
}

int devmm_mem_dma_cpy_process(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para)
{
    struct devmm_copy_res *res = para->res;
    int ret;

    if (res->cpy_mode == DEVMM_DMA_SYNC_MODE) {
        ret = devmm_sync_copy_process(res, para);
    } else {
        ret = devmm_set_async_cpy_para(svm_proc, para, res);
        if (ret != 0) {
           return ret;
        }
        ka_base_atomic_inc(&res->copy_task->occupy_num);
        ret = devmm_async_copy_process(res, para);
        if (ret != 0) {
            /* temporary: if devmm_dma_wait_task_finish return err, the res->copy_task is null */
            if (res->copy_task != NULL) {
                ka_base_atomic_dec(&res->copy_task->occupy_num);
            }
        }
    }

    return ret;
}

int devmm_ioctl_memcpy_process(struct devmm_svm_process *svm_proc,
                               struct devmm_mem_copy_convrt_para *para,
                               struct devmm_memory_attributes *src_attr,
                               struct devmm_memory_attributes *dst_attr)
{
    struct devmm_copy_res *res = para->res;
    int ret;

    devmm_memcpy_set_buf_info(svm_proc, para, res, src_attr, dst_attr);

    ret = devmm_make_raw_dmanode_list(para, svm_proc, res, src_attr, dst_attr);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Raw dmanode fail. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
                      ret, para->src, para->dst, para->count);
        goto free_copy_mem;
    }
    ret = devmm_make_dmanode_list(para->src, para->dst, para->count, res);
    if (ret != 0) {
        devmm_drv_err("Cp make dmanode list fail. (num=%d; ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
                      res->to.num, ret, para->src, para->dst, para->count);
        goto dma_node_unpin_flag;
    }

    ret = devmm_mem_dma_cpy_process(svm_proc, para);
    if (ret != 0) {
        goto dma_node_unpin_flag;
    }

    return ret;

dma_node_unpin_flag:
    devmm_free_raw_dmanode_list(res);
free_copy_mem:
    devmm_memcpy_exception_handle(res, para);
    devmm_free_copy_mem(res);

    return ret;
}

STATIC void devmm_handle_alloc_res_fail(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para)
{
    struct devmm_copy_res res;

    res.vm_copy_flag = false;
    res.task_id = para->task_id;
    res.cpy_mode = devmm_memcpy_get_cpy_mode(para, false);
    res.copy_task = para->copy_task;
    res.svm_pro = svm_proc;
    devmm_memcpy_exception_handle(&res, para);
}

int devmm_ioctl_memcpy_process_res(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr)
{
    /* update va in attributes */
    dst_attr->va = para->dst;
    src_attr->va = para->src;

    para->res = devmm_alloc_copy_res(para->count, src_attr, dst_attr, false);
    if (para->res == NULL) {
        devmm_drv_err("Copy resouce devmm_kmalloc_ex fail. (byte_count=%lu)\n", para->count);
        devmm_handle_alloc_res_fail(svm_proc, para);
        return -ENOMEM;
    }
    para->res->vm_copy_flag = false;
    para->res->vm_id = 0;

    return devmm_ioctl_memcpy_process(svm_proc, para, src_attr, dst_attr);
}

void devmm_async_cpy_inc_addr_ref(struct devmm_svm_process *svm_proc, u64 src, u64 dst, u64 size)
{
    /* ioctl api already check src dst ok, here will not fail */
    (void)devmm_inc_page_ref(svm_proc, dst, size);
    (void)devmm_inc_page_ref(svm_proc, src, size);
}

static void devmm_async_cpy_dec_addr_ref(struct devmm_svm_process *svm_proc, u64 src, u64 dst, u64 size)
{
    devmm_dec_page_ref(svm_proc, dst, size);
    devmm_dec_page_ref(svm_proc, src, size);
}

static bool copy_task_remove_condition(void *find_ptr)
{
    struct devmm_dma_copy_task *copy_task = (struct devmm_dma_copy_task *)find_ptr;

    return (copy_task->submit_status == DEVMM_ASYNC_SUBMIT_FINISH_VALUE);
}

static struct devmm_dma_copy_task *devmm_copy_task_get_and_remove_by_id(
    struct devmm_svm_process *svm_proc, int id)
{
    struct devmm_svm_proc_master *master_data = NULL;
    struct devmm_dma_copy_task *copy_task = NULL;

    master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    copy_task = (struct devmm_dma_copy_task *)devmm_idr_get_and_remove(
        &master_data->async_copy_record.task_idr, id, copy_task_remove_condition);
    return copy_task;
}

static int devmm_dma_cpy_result_refresh(u32 dev_id, int task_id)
{
    int instance;

    instance = devmm_copy_get_dma_instance(task_id);
    return devdrv_dma_done_schedule(dev_id, DEVDRV_DMA_DATA_TRAFFIC, instance);
}

static int devmm_wait_async_cpy_result(struct devmm_svm_process *svm_proc,
    struct devmm_dma_copy_task *copy_task, u64 *async_status)
{
    u64 loop_time = 20000000ul; /* 20s */
    u64 i;
    int ret;

    loop_time = loop_time + loop_time * copy_task->submit_num;
    for (i = 0; i < loop_time; i++) {
        if (copy_task->async_status != 0) {
            *async_status = copy_task->async_status;
            return 0;
        } else {
#ifndef EMU_ST
            ret = devmm_dma_cpy_result_refresh(copy_task->dev_id, (int)copy_task->task_id);
            if (ret != 0) {
                devmm_drv_err("Refresh cpy result failed. (ret=%d; devid=%u; task_id=%u)\n",
                    ret, copy_task->dev_id, copy_task->task_id);
                return ret;
            }
#endif
            if ((copy_task->async_status == 0) && (loop_time % 20 == 0)) { /* 20 times */
                ka_system_usleep_range(1, 2); /* 1-2 us */
            }
        }
    }

    return -ETIMEDOUT;
}

int devmm_cpy_result_refresh(struct devmm_svm_process *svm_proc,
    struct devmm_mem_async_copy_para *async_copy_para)
{
    struct devmm_dma_copy_task *copy_task = NULL;
    int ret;

    copy_task = devmm_copy_task_get_and_remove_by_id(svm_proc, async_copy_para->task_id);
    if (copy_task == NULL) {
        return -EINVAL;
    }
    ret = devmm_wait_async_cpy_result(svm_proc, copy_task, (u64 *)&async_copy_para->cpy_state);
    if (ret == 0) {
        devmm_async_cpy_dec_addr_ref(svm_proc, copy_task->src, copy_task->dst, copy_task->size);
    }
    devmm_task_del(copy_task);
    return ret;
}

int devmm_convert_addr_process(struct devmm_svm_process *svm_process, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr, struct devmm_copy_res *res)
{
    struct devmm_mem_copy_convrt_para para = {0};
    int ret;

    para.dst = convrt_para->pDst;
    para.src = convrt_para->pSrc;
    para.count = (size_t)convrt_para->len;
    para.direction = convrt_para->direction;
    para.create_msg = false;
    para.res = res;
    para.need_write = convrt_para->need_write;

    convrt_para->dmaAddr.phyAddr.priv = (void *)res;

    devmm_memcpy_set_buf_info(svm_process, &para, res, src_attr, dst_attr);

    ret = devmm_make_raw_dmanode_list(&para, svm_process, res, src_attr, dst_attr);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Raw dmanode list fail. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
                      ret, para.src, para.dst, para.count);
        return ret;
    }

    ret = devmm_make_dmanode_list(para.src, para.dst, para.count, res);
    if (ret != 0) {
        devmm_drv_err("Devmm make dmanode list fail. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu)\n",
                      ret, para.src, para.dst, para.count);
        goto convert_dmalist_free;
    }
    convrt_para->dma_node_num = res->dma_node_num;

    return 0;

convert_dmalist_free:
    devmm_free_raw_dmanode_list(res);
    return ret;
}

int devmm_destroy_dma_addr(struct DMA_ADDR *dma_addr)
{
    struct devmm_copy_res *res = dma_addr->phyAddr.priv;
    struct devdrv_dma_prepare *dma_prepare = (res ? res->dma_prepare : NULL);
    struct devmm_copy_res *res_delete = NULL;
    struct devmm_copy_res *res_tmp = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret = 0;

    if ((dma_addr->phyAddr.flag != 0) && (dma_prepare != NULL)) {
        ret = devdrv_dma_link_free(dma_prepare);
        if (ret != 0) {
            /* not need return, will free buf */
            devmm_drv_err("Dma link free fail. (ret=%d)\n", ret);
        }
    }

    res_delete = res;
    while (res_delete != NULL) {
        res_tmp = res_delete->next;
#ifdef CFG_FEATURE_VFIO
        if ((res_delete->vm_copy_flag == true) && (res_delete->height != 0)) {
            devmm_pm_increase_convert_free_id((u32)res_delete->dev_id, (u32)res_delete->fid, res_delete->height);
        }
        devmm_pm_idle_convert_len_add(res_delete->vm_id, (u32)res_delete->dev_id, (u32)res_delete->fid,
            res_delete->byte_count);
#endif
        devmm_free_raw_dmanode_list(res_delete);
        devmm_free_copy_mem(res_delete);
        res_delete = res_tmp;
        devmm_try_cond_resched(&stamp);
    }

    return ret;
}

static inline struct devmm_convert_dma *devmm_proc_find_convert_dma(struct devmm_svm_process *svm_proc, u32 index)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;

    return &master_data->convert_dma[index];
}

int devmm_ioctl_convert_addr_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr)
{
    struct devmm_copy_res *res = NULL;
    int ret;

    res = devmm_alloc_copy_res((u64)convrt_para->len, src_attr, dst_attr, false);
    if (res == NULL) {
        devmm_drv_err("Copy buf devmm_kmalloc_ex fail. (src=0x%llx; dst=0x%llx; byte_count=%llu)\n",
                      convrt_para->pSrc, convrt_para->pDst, convrt_para->len);
        return -ENOMEM;
    }
    res->vm_copy_flag = false;
    res->vm_id = 0;

    ret = devmm_convert_addr_process(svm_proc, convrt_para, src_attr, dst_attr, res);
    if (ret != 0) {
        devmm_free_copy_mem(res);
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Convert address failed. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}

static int devmm_fill_destroy_addr_arg_by_convert_node(struct devmm_convert_node *node,
    struct devmm_mem_destroy_addr_para *destroy_para)
{
    destroy_para->pSrc = node->info.src_va;
    destroy_para->pDst = node->info.dst_va;
    destroy_para->spitch = node->info.spitch;
    destroy_para->dpitch = node->info.dpitch;
    destroy_para->len = node->info.width;
    destroy_para->height = node->info.height;
    destroy_para->fixed_size = node->info.fixed_size;

    return 0;
}

int devmm_ioctl_destroy_addr_proc_inner(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_destroy_addr_para *desty_para = &arg->data.desty_para;
    struct DMA_ADDR *dma_addr = &desty_para->dmaAddr;
    struct devmm_convert_node *convert_node = NULL;
    u64 handle = (u64)dma_addr->phyAddr.priv;
    int ret;

    convert_node = devmm_convert_node_get_by_task(svm_proc, handle);
    if (convert_node == NULL) {
        devmm_drv_err("Get convert_node failed, dma_addr is invalid or node has been destroyed.\n");
        return -EFAULT;
    }

    ret = devmm_fill_destroy_addr_arg_by_convert_node(convert_node, desty_para);
    if (ret != 0) {
        goto put_node;
    }

    ret = devmm_convert_node_destroy(convert_node, CONVERT_NODE_IDLE, true);
    if (ret != 0) {
        devmm_drv_err("Node is busy. (ret=%d; state=%d)\n", ret, convert_node->state);
    }

put_node:
    devmm_convert_node_put(convert_node);
    return ret;
}

int devmm_ioctl_destroy_addr_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return devmm_ioctl_destroy_addr_proc_inner(svm_proc, arg);
}

bool devmm_check_va_is_convert(struct devmm_svm_process *svm_proc, u64 va)
{
    struct devmm_convert_dma *convert_dma = NULL;
    u32 index;

    for (index = 0; index < DEVMM_CONVERT_TREE_NUM; index++) {
        convert_dma = devmm_proc_find_convert_dma(svm_proc, index);
        if (!(ka_base_rb_first(&convert_dma->dma_rbtree) == NULL)) {
            return true;
        }
    }
    return false;
}

static int devmm_commit_dma_cpy_process(struct devmm_svm_process *svm_proc,
    struct devmm_mem_copy_convrt_para *para)
{
    struct devmm_copy_res *res = para->res;
    int ret;

    if (ka_likely(res->cpy_mode == DEVMM_DMA_ASYNC_MODE)) {
        if (para->seq == 0) {
            devmm_dma_copy_task_set_para(para, res->copy_task);
            devmm_async_task_record_inc(svm_proc, para->dev_id);
        }
        if (para->last_seq_flag == true) {
            res->copy_task->submit_num = para->seq + 1;
        }
        ret = devmm_dma_async_link_copy(res, (int)para->task_id, (void*)res, devmm_dma_async_callback);
        if (ka_unlikely(ret != 0)) {
            devmm_wait_task_finish(para->dev_id, &res->copy_task->finish_num, (int)para->seq);
            devmm_async_task_record_dec(svm_proc, para->dev_id);
        }
    } else {
        ret = devmm_dma_sync_link_copy((u32)res->dev_id, (u32)res->fid, res->dma_node, res->dma_node_num);
    }

    return ret;
}

static inline void devmm_fill_commit_copy_convert_para(struct devmm_copy_res *copy_res,
    u32 seq, bool last_seq_flag, struct devmm_mem_copy_convrt_para *copy_convert_para)
{
    copy_convert_para->res = copy_res;
    copy_convert_para->seq = seq;
    copy_convert_para->task_mode = DEVMM_CPY_CONVERT_MODE;
    copy_convert_para->task_id = DEVMM_DMA_CONVERT_MODE_CHANNEL;
    copy_convert_para->last_seq_flag = last_seq_flag;
    copy_convert_para->dev_id = (u32)copy_res->dev_id;
    copy_convert_para->src = copy_res->src_va;
    copy_convert_para->dst = copy_res->dst_va;
    copy_convert_para->task_query_id = DEVMM_SVM_INVALID_INDEX;
    copy_convert_para->copy_task = copy_res->copy_task;
}

int devmm_sumbit_convert_dma_proc(struct devmm_svm_process *svm_proc, struct DMA_ADDR *dma_addr, int sync_flag)
{
    struct devmm_mem_copy_convrt_para copy_convert_para = {0};
    struct devmm_convert_node *convert_node = NULL;
    struct devmm_copy_res *copy_res = NULL;
    u64 handle = (u64)dma_addr->phyAddr.priv;
    u32 stamp = (u32)ka_jiffies;
    int ret = -EFAULT;
    int cpy_mode;
    bool is_last;
    u32 seq;

    convert_node = devmm_convert_node_get_by_task(svm_proc, handle);
    if (convert_node == NULL) {
        devmm_drv_err("Err dma_addr.\n");
        return -EFAULT;
    }

    ret = devmm_convert_node_state_trans(convert_node, CONVERT_NODE_IDLE, CONVERT_NODE_PREPARE_SUBMIT);
    if (ret != 0) {
        devmm_drv_err("Trans state failed. (ret=%d; state=%d)\n", ret, convert_node->state);
        devmm_convert_node_put(convert_node);
        return ret;
    }

    copy_res = (struct devmm_copy_res *)convert_node->info.dma_addr.phyAddr.priv;
    if ((sync_flag == MEMCPY_SUMBIT_ASYNC) &&
        (devmm_proc_dev_is_async_allow(svm_proc, copy_res->dev_id) == false)) {
        devmm_drv_err("Device is reset, async copy is not allow. (phy_devid=%d)\n", copy_res->dev_id);
        (void)devmm_convert_node_state_trans(convert_node, CONVERT_NODE_PREPARE_SUBMIT, CONVERT_NODE_IDLE);
        devmm_convert_node_put(convert_node);
        return -EFAULT;
    }

    cpy_mode = (sync_flag == MEMCPY_SUMBIT_SYNC) ? DEVMM_DMA_SYNC_MODE : DEVMM_DMA_ASYNC_MODE;
    seq = 0;
    while (copy_res != NULL) {
        copy_res->cpy_mode = cpy_mode;
        copy_res->copy_task = &convert_node->copy_task;
        is_last = (copy_res->next == NULL);
        devmm_fill_commit_copy_convert_para(copy_res, seq, is_last, &copy_convert_para);
        ret = devmm_commit_dma_cpy_process(svm_proc, &copy_convert_para);
        if (ret != 0) {
            break;
        }
        seq++;
        copy_res = copy_res->next;
        devmm_try_cond_resched(&stamp);
    }

    if (ka_unlikely((sync_flag == MEMCPY_SUMBIT_SYNC) || (ret != 0))) {
        (void)devmm_convert_node_state_trans(convert_node, CONVERT_NODE_PREPARE_SUBMIT, CONVERT_NODE_IDLE);
    } else {
        (void)devmm_convert_node_state_trans(convert_node, CONVERT_NODE_PREPARE_SUBMIT, CONVERT_NODE_COPYING);
    }
    devmm_convert_node_put(convert_node);
    return ret;
}

int devmm_wait_convert_dma_result(struct devmm_svm_process *svm_proc, struct DMA_ADDR *dma_addr)
{
    struct devmm_convert_node *convert_node = NULL;
    u64 handle = (u64)dma_addr->phyAddr.priv;
    int ret, cpy_ret, dma_state;
    u64 async_copy_state;

    convert_node = devmm_convert_node_get_by_task(svm_proc, handle);
    if (convert_node == NULL) {
        devmm_drv_err("Err dma_addr.\n");
        return -EFAULT;
    }

    ret = devmm_convert_node_state_trans(convert_node, CONVERT_NODE_COPYING, CONVERT_NODE_WAITING);
    if (ret != 0) {
        devmm_drv_err("Trans state failed. (ret=%d; state=%d)\n", ret, convert_node->state);
        devmm_convert_node_put(convert_node);
        return -ETXTBSY;
    }

    ret = devmm_wait_async_cpy_result(svm_proc, &convert_node->copy_task, &async_copy_state);
    /* set dma node unuse when result is ok,  dma node can not use again when result is fail */
    dma_state = (ret == 0) ? CONVERT_NODE_IDLE : CONVERT_NODE_COPYING;
    (void)devmm_convert_node_state_trans(convert_node, CONVERT_NODE_WAITING, dma_state);

    cpy_ret = ((ret == 0) && (async_copy_state == DEVMM_ASYNC_CPY_FINISH_VALUE)) ? 0 : -EPIPE;
    devmm_convert_node_put(convert_node);
    return cpy_ret;
}

void devmm_destroy_all_convert_dma_addr(struct devmm_svm_process *svm_proc)
{
}

int devmm_destroy_addr_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return devmm_destroy_addr_batch_sync(svm_proc, arg);
}

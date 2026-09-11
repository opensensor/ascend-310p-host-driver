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

#include "kernel_version_adapt.h"
#include "virtmng_interface.h"

#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "svm_hot_reset.h"
#include "svm_dma.h"

static inline u32 devmm_get_total_dma_node_size(struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    u32 size = 0;
    u32 i;

    for (i = 0; i < node_cnt; i++) {
        size += dma_node[i].size;
    }

    return size;
}

static inline void devmm_get_link_copy_type(u32 size, enum devdrv_dma_data_type *type, int *wait_type)
{
    /* If node_cnt is 1 and data size less then DEVMM_DMA_SMALL_PACKET_SIZE, */
    /* use PCIE_MSG channel to transfer data. use WAIT_QUREY type */
    if (size < DEVMM_DMA_SMALL_PACKET_SIZE) {
        *type = DEVDRV_DMA_DATA_PCIE_MSG;
        *wait_type = DEVDRV_DMA_WAIT_QUREY;
    } else if (size < DEVMM_DMA_QUERY_WAIT_PACKET_SIZE) {
        *type = DEVDRV_DMA_DATA_TRAFFIC;
        *wait_type = DEVDRV_DMA_WAIT_QUREY;
    } else {
        *type = DEVDRV_DMA_DATA_TRAFFIC;
        *wait_type = DEVDRV_DMA_WAIT_INTR;
    }
}

static int devmm_bandwidth_limit_check(u32 dev_id, u32 vfid, u32 dir, u32 data_len, u32 node_cnt)
{
#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_VFIO_DEVICE)
    struct vmng_bandwidth_check_info check_info;

    if (vfid == 0) {
        return 0;
    }

    check_info.dir = dir;
    check_info.vfid = vfid;
    check_info.dev_id = dev_id;
    check_info.data_len = data_len;
    check_info.node_cnt = node_cnt;
    check_info.handle_mode = VMNG_BW_BANDWIDTH_CHECK_SLEEPABLE;

    return vmng_bandwidth_limit_check(&check_info);
#else
    return 0;
#endif
}

int devmm_dma_sync_link_copy(u32 dev_id, u32 vfid, struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    enum devdrv_dma_data_type type;
    int ret, wait_type;
    int retry_cnt = 0;
    u32 size;
    static ka_atomic_t copy_num = KA_BASE_ATOMIC_INIT(0);

    if (devmm_get_stop_business_flag(dev_id)) {
        devmm_drv_err("Device is offline, can not dma copy. (devid=%u)\n", dev_id);
#ifndef EMU_ST
        return -ENXIO;
#endif
    }

    size = devmm_get_total_dma_node_size(dma_node, node_cnt);
    ret = devmm_bandwidth_limit_check(dev_id, vfid, (u32)dma_node->direction, size, node_cnt);
    if (ret != 0) {
        devmm_drv_err("Check bw failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    devmm_get_link_copy_type(size, &type, &wait_type);
    if (wait_type == DEVDRV_DMA_WAIT_QUREY) {
        if (ka_base_atomic_inc_return(&copy_num) >= DMA_WAIT_QUREY_THREAD_NUM) {
            wait_type = DEVDRV_DMA_WAIT_INTR;
            ka_base_atomic_dec(&copy_num);
        }
    }

    while (g_devmm_true) {
        ret = devdrv_dma_sync_link_copy(dev_id, type, wait_type, dma_node, node_cnt);
        if (ka_likely(((ret == -ENOSPC) && (retry_cnt < DEVMM_DMA_RETRY_CNT)) == false)) {
            break;
        }
         /* dma queue is full, delay resubmit */
        ka_system_usleep_range(DEVMM_DMA_WAIT_MIN_TIME, DEVMM_DMA_WAIT_MAX_TIME);
        retry_cnt++;
    };

    if (wait_type == DEVDRV_DMA_WAIT_QUREY) {
        ka_base_atomic_dec(&copy_num);
    }

    if (ret != 0) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Devdrv_dma_sync_link_copy fail. (dev_id=%u; node_cnt=%u; ret=%d)\n",
            dev_id, node_cnt, ret);
    }
    return ret;
}

STATIC int devmm_dma_sync_link_copy_plus(struct devmm_copy_res *res, int instance)
{
    int retry_cnt = 0;
    int ret = -1;

    while (g_devmm_true) {
        /* d2d use source device dma. h2d&d2h use device dma */
        ret = devdrv_dma_sync_link_copy_plus((u32)res->dev_id, DEVDRV_DMA_DATA_TRAFFIC, DEVDRV_DMA_WAIT_INTR,
            instance, res->dma_node, res->dma_node_num);
        if (ka_likely(((ret == -ENOSPC) && (retry_cnt < DEVMM_ASYNC_DMA_RETRY_CNT)) == false)) {
            break;
        }
        /* dma queue is full, delay resubmit */
        ka_system_usleep_range(DEVMM_ASYNC_DMA_WAIT_MIN_TIME, DEVMM_ASYNC_DMA_WAIT_MAX_TIME);
        retry_cnt++;
    };
    if (ret != 0) {
        devmm_drv_err("Devdrv_dma_sync_link_copy_plus fail. (dev_id=%d; node_cnt=%u; ret=%d)\n",
            res->dev_id, res->dma_node_num, ret);
    }

    return ret;
}

int devmm_dma_sync_link_copy_limit(struct devmm_copy_res *res, int instance)
{
    int ret;

    if (devmm_get_stop_business_flag((u32)res->dev_id)) {
        devmm_drv_err("Device is offline, can not dma copy. (devid=%d)\n", res->dev_id);
#ifndef EMU_ST
        return -ENXIO;
#endif
    }

    ret = devmm_bandwidth_limit_check((u32)res->dev_id, (u32)res->fid, (u32)res->dma_node->direction,
        (u32)res->cpy_len, res->dma_node_num);
    if (ret != 0) {
        devmm_drv_err("Check bw failed. (dev_id=%d; vfid=%d; ret=%d)\n", res->dev_id, res->fid, ret);
        return ret;
    }

    return devmm_dma_sync_link_copy_plus(res, instance);
}

int devmm_dma_async_link_copy(struct devmm_copy_res *res, int instance, void *priv, dma_finish_notify call_back)
{
    static int sync_flag[DEVMM_MAX_DEVICE_NUM] = {0};
    struct devdrv_asyn_dma_para_info para_info;
    u32 dev_id = (u32)res->dev_id;
    int ret;

    devmm_drv_debug("Dma async link copy. (dev_id=%u; node_cnt=%u; src=%llx; dst=%llx; size=%x)",
        dev_id, res->dma_node_num, res->dma_node[0].src_addr, res->dma_node[0].dst_addr, res->dma_node[0].size);

    if (devmm_get_stop_business_flag(dev_id)) {
        devmm_drv_err("Device is offline, can not dma copy. (devid=%u)\n", dev_id);
#ifndef EMU_ST
        return -ENXIO;
#endif
    }

    ret = devmm_bandwidth_limit_check(dev_id, (u32)res->fid, (u32)res->dma_node->direction,
        (u32)res->cpy_len, res->dma_node_num);
    if (ret != 0) {
        devmm_drv_err("Bandwidth_limit check fail.(dev_id=%u; vfid=%d; ret=%d)", dev_id, res->fid, ret);
        return ret;
    }
    if (sync_flag[dev_id] == 0) {
        para_info.priv = priv;
        para_info.remote_msi_vector = 0;
        para_info.trans_id = (u32)instance;
        para_info.finish_notify = call_back;
        para_info.interrupt_and_attr_flag = DEVDRV_LOCAL_IRQ_FLAG; /* set DEVDRV_ATTR_FLAG will strict order */
        /* d2d use source device dma. h2d&d2h use device dma */
        ret = devdrv_dma_async_link_copy_plus(dev_id, DEVDRV_DMA_DATA_TRAFFIC, instance,
            res->dma_node, res->dma_node_num, &para_info);
        /* dma queue is full, delay resubmit */
        if (ret == -ENOSPC) {
            sync_flag[dev_id] = 1;
            /* try sync link cpy */
            ret = devmm_dma_sync_link_copy_plus(res, -1);
            if (ret == 0) {
                call_back(priv, (u32)instance, 0);
            }
        }
    } else {
        /* dma is very busy, direct call sync link cpy */
        ret = devmm_dma_sync_link_copy_plus(res, -1);
        if (ret == 0) {
            call_back(priv, (u32)instance, 0);
        }
        sync_flag[dev_id] = 0;
    }

    return ret;
}

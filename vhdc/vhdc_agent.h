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

#ifndef _VHDC_AGENT_H_
#define _VHDC_AGENT_H_

#include "drv_log.h"
#include "hdcdrv_mem_com.h"
#include "hdcdrv_core_com.h"
#include "virtmng_interface.h"

#define VHDC_VPC_MSG_SEND_RETRY_TIME 100
#define VHDC_BLK_VPC_MSG_SEND_RETRY_TIME 10000

#define VHDC_VDEV_BUSY_TIMEOUT 3000 /* 3s */
#define PCI_VENDOR_ID_HUAWEI 0x19e5

#define VHDC_MODE_KERNEL 0
#define VHDC_MODE_USER 1

struct vhdca_pdev {
    u32 valid;
    ka_atomic64_t busy;

    u32 fid;
    u32 dev_id;
    ka_device_t *dev;
    ka_delayed_work_t init;

    int segment;
    struct hdcdrv_mem_pool huge_mem_pool[HDCDRV_MEM_POOL_TYPE_NUM];
    struct hdcdrv_mem_pool small_mem_pool[HDCDRV_MEM_POOL_TYPE_NUM];

    void *sync_mem_buf;
    ka_mutex_t sync_mem_mutex;
    int vm_version;
};

struct vhdca_ctrl {
    struct hdcdrv_cdev vhdca_cdev;
    int segment;
    int vdev_count;
    struct vhdca_pdev pdev[VMNG_PDEV_MAX];
    ka_delayed_work_t recycle_mem;
    struct hdcdrv_dev_fmem fmem;
    int pm_version;
};

extern struct vhdca_ctrl *g_vhdca_ctrl;

int vhdca_vpc_msg_send(u32 dev_id, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info, u32 timeout);

static inline void vhdca_vdev_user_inc(struct vhdca_pdev *dev)
{
    ka_base_atomic64_inc(&dev->busy);
}

static inline void vhdca_vdev_user_dec(struct vhdca_pdev *dev)
{
    ka_base_atomic64_dec(&dev->busy);
}

static inline bool vhdca_vdev_is_busy(struct vhdca_pdev *dev)
{
    return (ka_base_atomic64_read(&dev->busy) > 0) ? true : false;
}

#endif /* _VHDC_AGENT_H_ */

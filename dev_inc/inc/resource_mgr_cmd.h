/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
* Create: 2022-09-30
*/

#ifndef RESOURCE_MGR_CMD_H
#define RESOURCE_MGR_CMD_H

#include "drv_type.h"
#ifndef STATIC
#ifdef EMU_ST
#define STATIC
#else
#define STATIC static
#endif
#endif

#define DAVINCI_INTF_MODULE_VRESOURCE_MGR "vResMgr"
#define VMNG_MAGIC 'F'
#define VMNG_GET_VF_EVENT_BLOCK _IO(VMNG_MAGIC, 1)
#define VMNG_SET_VF_EVENT_BLOCK_DONE _IO(VMNG_MAGIC, 2)
#define VMNG_CMD_MAX_NR 3

enum vmng_resource_status {
    VMNG_RM_CLIENT_INIT,
    VMNG_RM_CLIENT_UNINIT,
    VMNG_RM_CLIENT_RESET,
    VMNG_RM_CLIENT_SRIOV_ENABLE,
    VMNG_RM_CLIENT_SRIOV_DISABLE,
    VMNG_RM_CLIENT_STATE_MAX
};

struct vmng_resource_mgr_ctl {
    u32 dev_id;
    u32 vfid;
    u32 status;
    int fd;

    u64 aicpu_bitmap;
    u64 datacpu_bitmap;
    u64 ctrlcpu_bitmap;
    u64 memnode_bitmap;
    u64 mem_size;
};

#endif

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

#ifndef VIRTMNG_PUBLIC_DEF_H
#define VIRTMNG_PUBLIC_DEF_H

#ifdef EMU_ST
#ifndef TSDRV_UT
#include "ut_log.h"
#endif
#else
#include "drv_log.h"
#endif
#include "virtmng_interface.h"
#ifndef EMU_ST
#ifndef DAVINCI_DEVICE
#include "ka_task_pub.h"
#endif
#endif

#ifndef EMU_ST
#ifndef DAVINCI_DEVICE
#include "ka_task_pub.h"
#endif
#endif

#ifdef VIRTMNG_HOST
#define module_vmng "vmng_host"
#else /* VIRTMNG_AGENT */
#ifdef VIRTMNG_AGENT
#define module_vmng "vmng_agent"
#else /* VIRTMNG_DEV */
#define module_vmng "vmng_dev"
#endif
#endif /* End of VIRTMNG_HOST */

#if defined(DAVINCI_DEVICE) || defined(EMU_ST) 
#define vmng_err(fmt, ...) drv_err(module_vmng, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define vmng_warn(fmt, ...) drv_warn(module_vmng, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define vmng_event(fmt, ...) drv_event(module_vmng, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define vmng_info(fmt, ...) drv_info(module_vmng, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#else
#define vmng_err(fmt, ...) drv_err(module_vmng, "<%s:%d> " fmt, ka_task_get_current_comm(), \
                                   ka_task_get_current_tgid(), ##__VA_ARGS__)
#define vmng_warn(fmt, ...) drv_warn(module_vmng, "<%s:%d> " fmt, ka_task_get_current_comm(), \
                                   ka_task_get_current_tgid(), ##__VA_ARGS__)
#define vmng_event(fmt, ...) drv_event(module_vmng, "<%s:%d> " fmt, ka_task_get_current_comm(), \
                                   ka_task_get_current_tgid(), ##__VA_ARGS__)
#define vmng_info(fmt, ...) drv_info(module_vmng, "<%s:%d> " fmt, ka_task_get_current_comm(), \
                                   ka_task_get_current_tgid(), ##__VA_ARGS__)
#endif

#if defined(DAVINCI_DEVICE) || defined(EMU_ST) 
#define vmng_err_limit(fmt, ...) do { \
    if (printk_ratelimit() != 0) { \
       drv_err(module_vmng, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__); \
    } \
} while (0)
#else
#define vmng_err_limit(fmt, ...) do { \
    if (printk_ratelimit() != 0) { \
        drv_err(module_vmng, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__); \
    } \
} while (0)
#endif

#ifdef DRV_VMNG_DEBUG
#if defined(DAVINCI_DEVICE) || defined(EMU_ST) 
#define vmng_debug(fmt, ...) drv_debug(module_vmng, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#else
#define vmng_debug(fmt, ...) drv_debug(module_vmng, "<%s:%d> " fmt, ka_task_get_current_comm(), \
                                       ka_task_get_current_tgid(), ##__VA_ARGS__)
#endif
#else
#define vmng_debug(fmt, ...)
#endif /* End of DRV_VMNG_DEBUG */

extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
extern int strcpy_s(char *strDest, size_t destMax, const char *strSrc);
extern int strcat_s(char *strDest, size_t destMax, const char *strSrc);
extern int strncat_s(char *strDest, size_t destMax, const char *strSrc, size_t count);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);

#ifndef STATIC
#ifdef VIRTMNG_UT
#define STATIC
#else
#define STATIC static
#endif
#endif

#ifndef EOK
#define EOK 0x0
#endif

/* ---------------------------- cut down ---------------- */
#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define HISI_EP_DEVICE_ID_MINIV1 0xd100
#define HISI_EP_DEVICE_ID_MINIV2 0xd500
#define HISI_EP_DEVICE_ID_CLOUD 0xd801
#define HISI_EP_DEVICE_ID_CLOUD_V2 0xd802

#define VMNG_CTRL_DEVICE_ID_INIT (-1)

#define VMNG_AGENT_SIDE 0x0
#define VMNG_HOST_SIDE 0x1

#define VMNG_DOORBELL_ADDR_SIZE 4

#define VMNG_TASK_WAIT 0x110u
#define VMNG_TASK_TIMEOUT 0x221u
#define VMNG_TASK_SUCCESS 0x332u

#define VMNG_VM_START_WAIT 0x110u
#define VMNG_VM_START_TIMEOUT 0x221u
#define VMNG_VM_START_SUCCESS 0x332u
#define VMNG_VM_REMOVE_WAIT 0x442u
#define VMNG_VM_SHUTDOWN_WAIT 0x443u
#define VMNG_VM_SUSPEND_SUCCESS 0x554u
#define VMNG_VM_RM_VDEV_WAIT 0x665u
#define VMNG_VM_RM_HOST_PDEV_WAIT 0x776u

#define VMNG_BAR_0 0x0
#define VMNG_BAR_2 0x2
#define VMNG_BAR_4 0x4

enum {
    IO_REGION_INDEX,
    MEM_REGION_INDEX,
    NUM_REGION_INDEX
};

/* bar2 for message sq */
#define VMNG_SHR_PARA_ADDR_BASE 0x0
#define VMNG_SHR_PARA_ADDR_SIZE 0x1000
#define VMNG_MSG_ADDR_BASE (VMNG_SHR_PARA_ADDR_BASE + VMNG_SHR_PARA_ADDR_SIZE)
#define VMNG_MSG_ADDR_SIZE 0x2000000

/* bar4 for external module */
#define VMNG_BAR4_TSDRV_BASE 0x0

#define VMNG_START_DB_IRQ_IDX 0x0

#ifndef EMU_ST
#define VMNG_MSG_ALLOC_WAIT_TIMEOUT_MS 2000
#define VMNG_MSG_SYNC_WAIT_TIMEOUT_US 5000000
#else
#define VMNG_MSG_ALLOC_WAIT_TIMEOUT_MS 20
#define VMNG_MSG_SYNC_WAIT_TIMEOUT_US 500
#endif
#define VMNG_MSG_SYNC_WAIT_CYCLE_US 5
#define VMNG_MSG_SYNC_WAIT_NUM (VMNG_MSG_SYNC_WAIT_TIMEOUT_US / VMNG_MSG_SYNC_WAIT_CYCLE_US)

#define VMNG_MODULE_REMOVE_BY_PRERESET 0
#define VMNG_MODULE_REMOVE_BY_MODULE_EXIT 1

#define VMNG_VALID 1
#define VMNG_INVALID 0

#define VMNG_PROCFS_VALID 1
#define VMNG_PROCFS_INVALID 0

#define VMNG_OK 0
#define VMNG_ERR (-1)

enum vmng_ctrl_msg_type {
    VMNG_CTRL_MSG_TYPE_SYNC = 0,
    VMNG_CTRL_MSG_TYPE_INIT_CLIENT,
    VMNG_CTRL_MSG_TYPE_UNINIT_CLIENT,
    VMNG_CTRL_MSG_TYPE_ALLOC_VF,
    VMNG_CTRL_MSG_TYPE_FREE_VF,
    VMNG_CTRL_MSG_TYPE_RESET_VF,
    VMNG_CTRL_MSG_TYPE_REFRESH_VF,
    VMNG_CTRL_MSG_TYPE_ENQUIRE_VF,
    VMNG_CTRL_MSG_TYPE_SRIOV_INFO,
    VMNG_CTRL_MSG_TYPE_IOVA_INFO,
    VMNG_CTRL_MSG_TYPE_SYNC_ID,
    VMNG_CTRL_MSG_TYPE_MAX
};

enum vmng_intance_flag {
    VMNG_INSTANCE_FLAG_UNINIT = 0,
    VMNG_INSTANCE_FLAG_INIT
};

struct vmng_shr_para {
    u32 start_flag;
    u32 wdog_timer;
    u32 agent_device;
    u32 agent_dev_id;
    u32 agent_bdf;
    u32 reserve;
    u32 dtype;
    u32 aicore_num;
    u64 ddrmem_size;
    u64 hbmmem_size;
    u32 chan_num;
    u32 msix_offset;
};

struct vmng_ctrl_msg_sync {
    int dev_id;
    unsigned int aicore_num;
};

struct vmng_ctrl_msg_info {
    u32 dev_id;
    u32 vfid;
    u32 vfg_type;
    u32 dtype;
    u32 core_num;
    u32 total_core_num;
    u64 ddr_size;
    u64 hbm_size;
    union {
        struct vmng_vf_res_info vf_cfg;
        struct vmng_soc_resource_enquire enquire;
        struct vmng_soc_resource_refresh refresh;
        int sriov_status;
        struct vmng_mdev_iova_info iova_info;
        struct vmng_vf_sync_remote_id id_info;
    };
};

struct vmng_ctrl_msg {
    int type;
    int error_code;
    union {
        struct vmng_ctrl_msg_sync sync_msg;
        struct vmng_ctrl_msg_info info_msg;
    };
};

#endif

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/types.h>
#ifndef AOS_LLVM_BUILD
#include <linux/uio_driver.h>
#include <linux/irq.h>
#else
#include <linux/irqflags.h>
#endif
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/version.h>

#include "devdrv_common.h"
#include "devdrv_interface.h"
#include "devdrv_cqsq.h"
#include "devdrv_devinit.h"
#include "devdrv_driver_pm.h"
#include "devdrv_id.h"
#include "devdrv_recycle.h"
#include "devdrv_shm.h"
#include "devdrv_config_p2p.h"
#include "devdrv_manager_comm.h"
#include "devdrv_cb.h"
#include "devdrv_cbctrl.h"
#include "tsdrv_interface.h"
#include "tsdrv_device.h"
#include "tsdrv_id.h"
#include "tsdrv_get_ssid.h"
#include "tsdrv_ctx.h"
#include "davinci_interface.h"
#include "davinci_api.h"
#include "tsdrv_comm_fops.h"
#include "logic_cq.h"
#include "tsdrv_logic_cq.h"
#include "tsdrv_common.h"
#include "tsdrv_cmo_id.h"
#include "tsdrv_notify.h"

#include "tsdrv_hwinfo_platform.h"
#include "tsdrv_nvme.h"
#include "devdrv_ipc_notify.h"
#include "tsdrv_kernel_common.h"
#include "tsdrv_vsq.h"
#include "dbl/feature_loader.h"
#include "ascend_kernel_hal.h"
#include "tsdrv_sqcq_timestamp.h"

#ifndef CFG_SOC_PLATFORM_MDC_V51
#include "hvtsdrv_cqsq.h"
#endif
#include "devdrv_manager.h"
#include "tsdrv_bar_init.h"
#include "tsdrv_drvops.h"
#include "tsdrv_osal_intr.h"
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2)
#include "devdrv_manager_common.h"
#endif

#ifndef CFG_MANAGER_HOST_ENV
#if defined(CFG_SOC_PLATFORM_MDC)
#include "bind_core.h"
#endif
#endif

#include "tsdrv_log.h"
#include "tsdrv_rts_streamid.h"
#ifdef CFG_SOC_PLATFORM_MDC_V51
#include "tsdrv_ts_node.h"
#endif
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define STARS_L2_REG_NUM_PER_MID_REG 32
#else
#define STARS_L2_REG_NUM_PER_MID_REG 8
#endif

#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3)
#define STARS_INTR_MID_REG_NUM 2
#define STARS_INTR_L2_REG_NUM 64
#define STARS_INTR_REG_BIT_NUM 32

#define STARS_INTR_BIT_NUM_PER_GROUP 4
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define STARS_INTR_CQ_NUM_PER_GROUP 128
#else
#define STARS_INTR_CQ_NUM_PER_GROUP 32
#endif

#ifdef CFG_MANAGER_HOST_ENV
/* index 0: cq 0-2023(bit 0: 0-31, bit 1: 32-63 ...), 1: cq 1024-2047 */
#define STARS_MID_NORMAL_CQE_WRITTEN_NS_PENDING(index) (0x1c8 + (index) * 4)

/* index 0: cq 0-31(bit 0: cq0, bit 1: cq1 ...), 1: cq 32-63, ... */
#define STARS_L2_NORMAL_CQE_WRITTEN_NS_PENDING(index) (0x2500 + (index) * 4)

#define STARS_CQ_INT_HOST_OFFSET 0
#define STARS_CQ_INT_VF_OFFSET   0x100000
#define STARS_CQ_NS_INT_CFG(rtsq_hs) (STARS_CQ_INT_HOST_OFFSET + (rtsq_hs) * 4)

#define STARS_CQ_NS_STATUS2(rtsq_hs) \
    (STARS_CQ_INT_HOST_OFFSET + (rtsq_hs) * 0x10000 + 0x100010)

#define STARS_CQ_NS_CLR2(rtsq_hs) \
    (STARS_CQ_INT_HOST_OFFSET + (rtsq_hs) * 0x10000 + 0x100014)

#define STARS_MID_NORMAL_CQE_WRITTEN_PREVIOUS_NS_STATUS2(rtsq_hs) \
    (STARS_CQ_INT_HOST_OFFSET + ((rtsq_hs) * 0x10000) + 0x100040)

#define STARS_L2_NORMAL_CQE_WRITTEN_NS_STATUS2(rtsq_hs, rtsq_ms) \
    (STARS_CQ_INT_HOST_OFFSET + (rtsq_hs) * 0x10000 + (rtsq_ms) * 0x4 + 0x100090)

#define STARS_L2_NORMAL_CQE_WRITTEN_NS_CLR2(rtsq_hs, rtsq_ms) \
    (STARS_CQ_INT_HOST_OFFSET + (rtsq_hs) * 0x10000 + (rtsq_ms) * 0x4 + 0x1000A0)

#else
#define STARS_MID_NORMAL_CQE_WRITTEN_NS_PENDING(index) (0x190 + (index) * 4)
#define STARS_L2_NORMAL_CQE_WRITTEN_NS_PENDING(index) (0x1e00 + (index) * 4)
#endif
#define STARS_L2_NORMAL_CQE_WRITTEN_NS_INTERRUPT_CLR(index) (0x704 + (index) * 4)

#define STARS_L1_NORMAL_CQE_WRITTEN_NS_STATUS1 0x2024
#define STARS_MID_NORMAL_CQE_WRITTEN_PREVIOUS_NS_STATUS1(index) (0x2040 + (index) * 4)
#define STARS_L2_NORMAL_CQE_WRITTEN_NS_STATUS1(rtsq_m) (0x2180 + (rtsq_m) * 4)
#define STARS_L2_NORMAL_CQE_WRITTEN_NS_CLR1(rtsq_m) (0x2280 + (rtsq_m) * 4)
#define STARS_L1_NORMAL_CQE_WRITTEN_NS_MASK1 0x2020
#endif

#ifndef AOS_LLVM_BUILD
#ifdef VM_FAULT_SIGSEGV
#define DEVDRV_FAULT_ERROR VM_FAULT_SIGSEGV
#else
#define DEVDRV_FAULT_ERROR VM_FAULT_ERROR
#endif
#endif

STATIC int devdrv_ioctl_alloc_stream(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int devdrv_ioctl_alloc_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int devdrv_ioctl_free_stream(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int devdrv_ioctl_free_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int devdrv_ioctl_alloc_model(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int devdrv_ioctl_free_model(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int devdrv_ioctl_alloc_ipc_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int devdrv_ioctl_free_ipc_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
STATIC int tsdrv_get_ssid(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);

static int tsdrv_get_dev_info(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    arg->devInfo.virt_type = (int)TSDRV_PHYSICAL_TYPE;

    TSDRV_PRINT_INFO("Get dev info succeed. (virt_type=%d)\n", arg->devInfo.virt_type);
    return 0;
}

STATIC int (* const devdrv_ioctl_handlers[TSDRV_MAX_CMD])(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg) = {
    [_KA_IOC_NR(TSDRV_ALLOC_STREAM_ID)] = devdrv_ioctl_alloc_stream,
    [_KA_IOC_NR(TSDRV_FREE_STREAM_ID)] = devdrv_ioctl_free_stream,
    [_KA_IOC_NR(TSDRV_ALLOC_EVENT_ID)] = devdrv_ioctl_alloc_event,
    [_KA_IOC_NR(TSDRV_FREE_EVENT_ID)] = devdrv_ioctl_free_event,
    [_KA_IOC_NR(TSDRV_REPORT_WAIT)] = devdrv_ioctl_report_wait,
    [_KA_IOC_NR(TSDRV_ALLOC_MODEL_ID)] = devdrv_ioctl_alloc_model,
    [_KA_IOC_NR(TSDRV_FREE_MODEL_ID)] = devdrv_ioctl_free_model,
    [_KA_IOC_NR(TSDRV_ALLOC_NOTIFY_ID)] = devdrv_alloc_notify_id,
    [_KA_IOC_NR(TSDRV_FREE_NOTIFY_ID)] = devdrv_free_notify_id,
    [_KA_IOC_NR(TSDRV_ALLOC_CMO_ID)] = tsdrv_ioctl_alloc_cmo,
    [_KA_IOC_NR(TSDRV_FREE_CMO_ID)] = tsdrv_ioctl_free_cmo,
    [_KA_IOC_NR(TSDRV_ALLOC_IPC_EVENT_ID)] = devdrv_ioctl_alloc_ipc_event,
    [_KA_IOC_NR(TSDRV_FREE_IPC_EVENT_ID)] = devdrv_ioctl_free_ipc_event,
    [_KA_IOC_NR(TSDRV_CBSQCQ_ALLOC_ID)] = devdrv_ioctl_cbsqcq_alloc,
    [_KA_IOC_NR(TSDRV_CBSQCQ_FREE_ID)] = devdrv_ioctl_cbsqcq_free,
    [_KA_IOC_NR(TSDRV_GET_DEV_INFO)] = tsdrv_get_dev_info,
    [_KA_IOC_NR(TSDRV_CBSQCQ_WAIT_ID)] = NULL,
    [_KA_IOC_NR(TSDRV_CBSQCQ_SEND_TASK)] = devdrv_ioctl_cbsq_send,
    [_KA_IOC_NR(TSDRV_SQCQ_ALLOC_ID)] = devdrv_ioctl_sqcq_alloc,
    [_KA_IOC_NR(TSDRV_SQCQ_FREE_ID)] = devdrv_ioctl_sqcq_free,
    [_KA_IOC_NR(TSDRV_SQ_MSG_SEND)] = tsdrv_ioctl_sq_msg_send,
    [_KA_IOC_NR(TSDRV_CQ_REPORT_RELEASE)] = tsdrv_ioctl_cq_report_release,
    [_KA_IOC_NR(TSDRV_GET_SQ_HEAD)] = tsdrv_ioctl_get_sq_head,
    [_KA_IOC_NR(TSDRV_GET_SSID)] = tsdrv_get_ssid,
#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
    [_KA_IOC_NR(TSDRV_SHM_SQCQ_ALLOC)] = shm_ioctl_cqsq_alloc,
    [_KA_IOC_NR(TSDRV_SHM_SQCQ_FREE)] = shm_ioctl_cqsq_free,
    [_KA_IOC_NR(TSDRV_LOGIC_CQ_ALLOC)] = logic_ioctl_cq_alloc,
    [_KA_IOC_NR(TSDRV_LOGIC_CQ_FREE)] = logic_ioctl_cq_free,
    [_KA_IOC_NR(TSDRV_LOGIC_CQ_WAIT)] = logic_ioctl_cq_wait,
#endif
    [_KA_IOC_NR(TSDRV_ID_INFO_QUERY)] = tsdrv_ioctl_id_info_query,
    [_KA_IOC_NR(TSDRV_SQCQ_SET)] = tsdrv_ioctl_sqcq_set,
    [_KA_IOC_NR(TSDRV_ENABLE_STREAM_ID)] = tsdrv_ioctl_enable_stream,
    [_KA_IOC_NR(TSDRV_DISABLE_STREAM_ID)] = tsdrv_ioctl_disable_stream,
    [_KA_IOC_NR(TSDRV_RES_CONFIG)] = tsdrv_ioctl_res_config,
    [_KA_IOC_NR(TSDRV_SQCQ_QUERY)] = tsdrv_ioctl_sqcq_query,
};

#ifdef CFG_MANAGER_HOST_ENV
static const struct pci_device_id devdrv_driver_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd100),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd105),           0 },
    { KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd801),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd501),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd802),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd803),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd804),           0 },
    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}
};
KA_MODULE_DEVICE_TABLE(pci, devdrv_driver_tbl);
#endif

int tsdrv_get_pfvf_type_by_devid(u32 devid)
{
#if defined(CFG_FEATURE_RC_MODE) || defined(CFG_SOC_PLATFORM_HELPER) || defined(CFG_SOC_PLATFORM_MDC)
    return DEVDRV_SRIOV_TYPE_PF;
#else
    return devdrv_get_pfvf_type_by_devid(devid);
#endif
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_pfvf_type_by_devid);

bool devdrv_is_support_uio_d(void)
{
    struct tsdrv_drv_ops *ops = tsdrv_get_drv_ops();
    int cp_type;
    u32 chip_id, vfid, host_pid;
    u32 location = 0xFFFF;
    int ret;

    if ((tsdrv_get_env_type() == TSDRV_ENV_OFFLINE) && (ops->cp_proc_query_hpid != NULL) &&
        (ops->cp_proc_query_master_location != NULL)) {
        ret = ops->cp_proc_query_hpid(current->tgid, &chip_id, &vfid, &host_pid, &cp_type);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_DEBUG("no cp. (pid=%d; ret=%d)\n", current->tgid, ret);
            return false;
#endif
        }

        ret = ops->cp_proc_query_master_location(host_pid, current->tgid, chip_id, vfid, cp_type, &location);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_DEBUG("Query master location failed. (pid=%d; ret=%d)\n", current->tgid, ret);
            return false;
#endif
        }

        if ((cp_type == DEVDRV_PROCESS_CP1) && (location == DEVDRV_HOST_MASTER)) {
            TSDRV_PRINT_DEBUG("Cp info. (pid=%d; cp_type=%d)\n", current->tgid, cp_type);
            return true;
        }
    }
    return false;
}

int devdrv_open(ka_inode_t *inode, ka_file_t *filep)
{
    u32 vdevid = drv_davinci_get_device_id(filep);
    struct tsdrv_ctx *ctx = NULL;
    enum tsdrv_dev_status status;
    u32 devid, fid, tsnum;
    bool is_remote_open = false;
    int err;

    if (vdevid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid=%u, inode=%pK\n", vdevid, inode);
        return -EFAULT;
    }

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    err = devmng_get_vdavinci_info(vdevid, &devid, &fid);
    if (err != 0) {
        TSDRV_PRINT_ERR("Invalid para, vdevid(%u), devid(%u), fid(%u)\n", vdevid, devid, fid);
        return -EFAULT;
    }
#else
    devid = vdevid;
    fid = TSDRV_PM_FID;
#endif
    TSDRV_PRINT_DEBUG("Open device. (vdevid=%u; devid=%u; fid=%u; tgid=%u)\n", vdevid, devid, fid, current->tgid);

    if (!tsdrv_is_in_pm(devid)) {
        TSDRV_PRINT_WARN("devid %u is in host_flag %d, not support!\n", devid, (int)tsdrv_get_host_flag(devid));
        return -EFAULT;
    }

    if (davinci_intf_confirm_user() == false) {
        TSDRV_PRINT_ERR("the user is not allowed to open devdrv_device%u.\n", devid);
        return -ENODEV;
    }
#ifndef AOS_LLVM_BUILD
    err = devdrv_manager_container_check_devid_in_container_ns(devid, current);
    if (err != 0) {
        TSDRV_PRINT_ERR("check device in container failed. dev_id(%u)\n", devid);
        return -ENODEV;
    }
#endif
    if (tsdrv_dev_ref_inc_return(devid, TSDRV_PM_FID) > TSDRV_MAX_DEV_REF) {
        TSDRV_PRINT_ERR("device%u will be unregistered, cannot open again.\n", devid);
        tsdrv_dev_ref_dec(devid, TSDRV_PM_FID);
        return -EEXIST;
    }

    status = tsdrv_get_dev_status(devid, TSDRV_PM_FID);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        tsdrv_dev_ref_dec(devid, TSDRV_PM_FID);
        TSDRV_PRINT_ERR("invalid status=%d, devid=%u\n", (int)status, devid);
        return -EEXIST;
#endif
    }

    if (!devdrv_is_support_uio_d()) {   /* In mc2 scenario, should support host and device open. */
        /* In ctrl-cpu open scenario, only support host or device open device. */
        err = tsdrv_set_runtime_run_conflict_check(devid);
        if (err != 0) {
            TSDRV_PRINT_WARN("set runtime run fail, devid=%u\n", devid);
            tsdrv_dev_ref_dec(devid, TSDRV_PM_FID);
            return err;
        }
        is_remote_open = false;
    } else {
        is_remote_open = true;
    }

    ctx = tsdrv_dev_ctx_get(devid, TSDRV_PM_FID, current->tgid);
    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("devid %u tgid %d open repeated.\n", devid, current->tgid);
        err = -EFAULT;
        goto err_tsdrv_dev_ctx_get;
#endif
    }

    tsnum = tsdrv_get_dev_tsnum(devid);
    err = tsdrv_nvme_ctx_init(ctx, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_nvme_ctx_init;
#endif
    }
    err = callback_ctx_init(ctx, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_cb_ctx_init;
#endif
    }
    tsdrv_set_ctx_normal_type(ctx);
    tsdrv_set_ctx_run(ctx);
    tsdrv_set_ctx_remote_open(ctx, is_remote_open);

    filep->private_data = ctx;

    TSDRV_PRINT_DEBUG("devid %u tgid %d.\n", devid, ctx->tgid);
    return 0;
#ifndef TSDRV_UT
err_cb_ctx_init:
    tsdrv_nvme_ctx_exit(ctx, tsnum);
err_nvme_ctx_init:
    tsdrv_dev_ctx_put(ctx);
err_tsdrv_dev_ctx_get:
    if (is_remote_open == false) {
        (void)tsdrv_set_runtime_available_conflict_check(devid);
    }

    tsdrv_dev_ref_dec(devid, TSDRV_PM_FID);
    return err;
#endif
}

STATIC int tsdrv_release_prepare(ka_file_t *file_op, unsigned long mode)
{
    struct tsdrv_ctx *ctx = file_op->private_data;
    enum tsdrv_dev_status status;
    u32 devid, fid, tsnum;
    int32_t ret;

    if ((mode != NOTIFY_MODE_RELEASE_PREPARE) || (ctx == NULL)) {
        TSDRV_PRINT_ERR("invalid mode(%lu) or ctx is null.\n", mode);
        return -ENOMEM;
    }

    devid = tsdrv_get_devid_by_ctx(ctx);
    fid = tsdrv_get_fid_by_ctx(ctx);
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (fid >= TSDRV_MAX_FID_NUM)) {
        TSDRV_PRINT_ERR("invalid devid=%u or fid=%u\n", devid, fid);
        return -ENODEV;
    }

    status = tsdrv_get_dev_status(devid, fid);
    if (status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("[devid=%u,fid=%u]invalid dev status=%d\n", devid, fid, (int)status);
        return -EEXIST;
    }

    TSDRV_PRINT_DEBUG("[devid=%u,fid=%u]tsdrv start release prepare\n", devid, fid);
    /*  set ctx release state, so we do not handle ioctl and cq irq for this ctx anymore */
    ret = tsdrv_set_ctx_releasing(ctx);
    if (ret != 0) {
        TSDRV_PRINT_ERR("[devid=%u,fid=%u]set ctx releasing fail, tgid=%d ret=%d\n", devid, fid, ctx->tgid, ret);
        return ret;
    }
    tsdrv_dev_set_ctx_recycle(ctx);

    tsnum = tsdrv_get_dev_tsnum(devid);
    logic_sqcq_ctx_exit(ctx, tsnum);

    devdrv_cbcqsq_to_recyclelist(devid, fid, tsnum, ctx);

    TSDRV_PRINT_DEBUG("[devid=%u,fid=%u]tsdrv end release prepare\n", devid, fid);
    return 0;
}

int devdrv_release(ka_inode_t *inode, ka_file_t *filep)
{
    struct tsdrv_ctx *ctx = filep->private_data;
    struct tsdrv_dev_resource *dev_res = NULL;
    enum tsdrv_dev_status status;
    bool is_remote_open = false;
    u32 tsnum, devid;
    int ret;

    if (ctx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ctx is NULL, inode=%pK.\n", inode);
        return -ENOMEM;
#endif
    }

    is_remote_open = tsdrv_get_ctx_remote_open(ctx);
    devid = tsdrv_get_devid_by_ctx(ctx);
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid=%u\n", devid);
        return -ENODEV;
    }
    status = tsdrv_get_dev_status(devid, TSDRV_PM_FID);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid dev status=%d devid=%u\n", (int)status, devid);
        ret = -EEXIST;
        goto OUT;
#endif
    }
    TSDRV_PRINT_DEBUG("tsdrv start release process, devid=%u\n", devid);

    tsnum = tsdrv_get_dev_tsnum(devid);
    dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    if (dev_res == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, TSDRV_PM_FID);
        ret = -EINVAL;
        goto OUT;
#endif
    }

    ret = tsdrv_proc_recycle(devid, ctx, dev_res);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Proc recycle failed. (devid=%u; ret=%d)\n", devid, ret);
        goto OUT;
#endif
    }

    tsdrv_dev_set_ctx_recycle_status(ctx, TSDRV_CTX_INVALID);

    if (is_remote_open == false) {
        ret = tsdrv_set_runtime_available_conflict_check(devid);
        if (ret != 0) {
            TSDRV_PRINT_ERR("set runtime status fail, devid=%u ret=%d.\n", devid, ret);
        }
    } else {
        TSDRV_PRINT_DEBUG("Remote open do not need set running status. (devid=%u)\n", devid);
    }

OUT:
    tsdrv_dev_ref_dec(devid, TSDRV_PM_FID);
    return ret;
}

#if (defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3))
STATIC void devdrv_invalid_ts_report_cache(u64 base, u32 len)
{
#ifdef __aarch64__
    int i;
    int l = (len + DEVDRV_CACHELINE_SIZE_64 - 1) / DEVDRV_CACHELINE_SIZE_64;

    base &= (~(DEVDRV_CACHELINE_SIZE_64 - 1));
    asm volatile("dsb st" : : : "memory");
    for (i = 0; i < l; i++) {
        asm volatile("DC IVAC ,%x0"::"r"(base + i * DEVDRV_CACHELINE_SIZE_64));
        dsb(ishst);
    }
    asm volatile("dsb st" : : : "memory");
#endif
}
#endif

int devdrv_is_need_invalid_cache(void)
{
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    int product_type = devdrv_manager_get_product_type();
    switch (product_type) {
        case HOST_TYPE_ARM_3559:
        case HOST_TYPE_ARM_3519:
            return 1;
        default:
            return 0;
    }
#else
    return 0;
#endif
}

STATIC bool tsdrv_cq_tail_phase_check(struct devdrv_ts_cq_info *cq_info,
    struct devdrv_cq_sub_info *cq_sub_info)
{
    struct devdrv_report *report = NULL;
    u32 phase;

#if (defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3))
    u32 len;
    len = cq_info->slot_size * cq_info->depth;

    if (devdrv_is_need_invalid_cache()) {
        devdrv_invalid_ts_report_cache((u64)cq_sub_info->virt_addr, len);
    }
    tsdrv_get_drv_ops()->flush_cache((u64)(uintptr_t)cq_sub_info->virt_addr, len);
#endif

    report = (struct devdrv_report *)(uintptr_t)(cq_sub_info->virt_addr +
        ((unsigned long)cq_info->slot_size * cq_info->tail));
    phase = devdrv_report_get_phase(report);
    if (cq_info->phase == phase) {
        return true;
    }
    return false;
}

void tsdrv_update_cq_tail_and_sq_head(struct tsdrv_ts_resource *ts_res,
    struct devdrv_ts_cq_info *cq_info, struct devdrv_cq_sub_info *cq_sub_info)
{
#ifndef TSDRV_UT
    u32 next_tail, sq_head, devid, fid, sqid;
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct tsdrv_id_info *sq_id_info = NULL;
    struct devdrv_report *report = NULL;
    u32 head_for_check = cq_info->head;
    phys_addr_t info_mem_addr;

    devid = tsdrv_get_devid_by_res(ts_res);
    fid = tsdrv_get_fid_by_ctx(cq_sub_info->ctx);
    info_mem_addr = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;

    report = (struct devdrv_report *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
        ((unsigned long)cq_info->slot_size * cq_info->tail)));

    while (devdrv_report_get_phase(report) == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        next_tail = (cq_info->tail + 1) % cq_info->depth;
        /* cq_tail cannot exceed cq_head, if it exceeds, the report will be lost */
        if (next_tail == head_for_check) {
            break;
        }
        sqid = tsdrv_report_get_sq_id(report);
        sq_head = tsdrv_report_get_sq_head(report);
        if ((sqid >= DEVDRV_MAX_SQ_NUM) || (sq_head >= DEVDRV_MAX_SQ_DEPTH)) {
            break;
        }
        sq_info = devdrv_calc_sq_info(info_mem_addr, sqid);
        sq_info->head = sq_head;
        next_tail = cq_info->tail + 1;
        if (next_tail > (cq_info->depth - 1)) {
            cq_info->phase = ((cq_info->phase == DEVDRV_PHASE_STATE_0) ? DEVDRV_PHASE_STATE_1 : DEVDRV_PHASE_STATE_0);
            cq_info->tail = 0;
        } else {
            cq_info->tail++;
        }

        if (!tsdrv_is_in_pm(devid)) {
            sq_id_info = devdrv_find_one_id(&ts_res->id_res[TSDRV_SQ_ID], sq_info->index);
            if (sq_id_info == NULL) {
                break;
            }
            TSDRV_PRINT_DEBUG("update vsq head, devid(%u), fid(%u), sqid(%u), vsqid(%u)\n",
                devid, fid, sq_info->index, sq_id_info->virt_id);
#ifndef CFG_SOC_PLATFORM_MDC_V51
            hvtsdrv_update_vsq_head(ts_res, sq_id_info->virt_id, sq_head);
#endif
        }

#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
        tsdrv_update_vsq_head(ts_res, sqid, sq_head);
#endif
        report = (struct devdrv_report *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
            ((unsigned long)cq_info->slot_size * cq_info->tail)));
    }
#endif
}

STATIC int tsdrv_wait_cq_report(u32 devid, u32 tsid, struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_ctx_to_ts_res(ctx, tsid);
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    int timeout = arg->report_para.timeout;
    unsigned long jiffies_timeout;
    bool cq_update = false;
    int ret;

    cq_info = devdrv_get_cq_exist(ts_res, &ctx->ts_ctx[tsid]);
    if (cq_info == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("get cq failed, devid(%u) tsid(%u)\n", devid, tsid);
        return false;
#endif
    }

    cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
    ka_task_spin_lock_irq(&cq_sub_info->spinlock);
    if ((cq_sub_info->ctx == NULL) || (cq_sub_info->virt_addr == 0)) {
#ifndef TSDRV_UT
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        TSDRV_PRINT_ERR("cq does not exit, devid(%u) tsid(%u) ctx(%pK)\n", devid, tsid, cq_sub_info->ctx);
        return false;
#endif
    }

    /* when irq is faster than irq wait, it need not enter wait_event */
    cq_update = tsdrv_cq_tail_phase_check(cq_info, cq_sub_info);
    if (cq_update == true) {
#ifndef TSDRV_UT
        tsdrv_update_cq_tail_and_sq_head(ts_res, cq_info, cq_sub_info);
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
        arg->report_para.cq_tail = cq_info->tail;
        TSDRV_PRINT_DEBUG("devid(%u), tsid(%u), cqid(%u), head(%u), tail(%u)\n",
            devid, tsid, cq_info->index, cq_info->head, cq_info->tail);
        return 1;
#endif
    }

    /* there is no proper cq, don't care current cq_tail_update value */
    ctx->ts_ctx[tsid].cq_tail_updated = 0;
    if (timeout == -1) {
        ka_task_wait_event_interruptible_lock_irq(ctx->ts_ctx[tsid].report_wait,
            ctx->ts_ctx[tsid].cq_tail_updated == CQ_HEAD_UPDATE_FLAG, cq_sub_info->spinlock);
        goto succeed;
    } else {
        jiffies_timeout = ka_system_msecs_to_jiffies((u32)timeout);
        ret = ka_task_wait_event_interruptible_lock_irq_timeout(ctx->ts_ctx[tsid].report_wait,
            ctx->ts_ctx[tsid].cq_tail_updated == CQ_HEAD_UPDATE_FLAG, cq_sub_info->spinlock, jiffies_timeout);
        ret = 1;
        if (ret > 0) {
            goto succeed;
        }
    }
    ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
    TSDRV_PRINT_DEBUG("wait event fail, ret(%d), devid(%u), tsid(%u), timeout(%d)(s),"
        "cq_id(%u), head(%u), tail(%u), phase(%u)\n", ret, devid, tsid, timeout,
        cq_info->index, cq_info->head, cq_info->tail, cq_info->phase);
    return -ENODEV;
succeed:
    tsdrv_update_cq_tail_and_sq_head(ts_res, cq_info, cq_sub_info);
    ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
    arg->report_para.cq_tail = cq_info->tail;
    TSDRV_PRINT_DEBUG("devid(%u), tsid(%u), cqid(%u), head(%u), tail(%u)\n",
        devid, tsid, cq_info->index, cq_info->head, cq_info->tail);
    /* retrurn 1 means there is proper cq received */
    return 1;
}

int devdrv_ioctl_report_wait(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int timeout = arg->report_para.timeout;
    u32 tsid = arg->tsid;
    u32 devid;
    int ret;

    devid = tsdrv_get_devid_by_ctx(ctx);
    if ((timeout <= 0) && (timeout != -1)) {
        TSDRV_PRINT_ERR("invalid timeout(%d)(s) devid(%u)\n", timeout, devid);
        return -ENODEV;
    }
    ret = tsdrv_wait_cq_report(devid, tsid, ctx, arg);
    if (tsdrv_mirror_ctx_abnormal(ctx)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("mirror process hang, devid=%u tgid=%d pid=%d\n", devid, ctx->tgid, ctx->pid);
        ret = (int)DEDVRV_DEV_PROCESS_HANG;
#endif
    } else if (tsdrv_get_mirror_ctx_status(ctx) == DEVDRV_STATUS_HDC_CLOSE_FLAG) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hdc connect down, devid(%u) tsid(%u)\n", devid, tsid);
        ret = DEVDRV_HDC_CONNECT_DOWN;
#endif
    } else if (tsdrv_get_ts_status(devid, tsid) != TS_WORK) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("ts is down, devid(%u) tsid(%u)\n", devid, tsid);
        ret = DEVDRV_BUS_DOWN;
#endif
    }
    arg->report_para.timeout = ret;
    return 0;
}

STATIC int tsdrv_alloc_stream_from_local_list(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *stream_info = NULL;
    int no_stream = 0;
    int stream_id;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);

    ts_resource = tsdrv_ctx_to_ts_res(ctx, arg->tsid);
    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_STREAM_ID].id_mutex_t);
    stream_info = devdrv_alloc_stream(ctx, arg->tsid, &no_stream);
    if (stream_info != NULL) {
        stream_id = stream_info->id;
    } else {
        arg->result = ID_IS_EXHAUSTED;
        stream_id = tsdrv_get_stream_id_max_num(devid, arg->tsid);
    }

    if ((stream_info == NULL) && !no_stream) {
        ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].id_mutex_t);
        arg->result = ID_NO_EXHAUSTED;
        TSDRV_PRINT_ERR("devdrv_alloc_stream failed.\n");
        return -ENOKEY;
    }
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_STREAM_ID].id_mutex_t);

    arg->id_para.res_id = (u32)stream_id;
    return 0;
}

STATIC int devdrv_ioctl_alloc_stream(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    return tsdrv_alloc_stream_from_local_list(ctx, arg);
}

STATIC int devdrv_free_stream_from_local_list(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int ret;
    ret = devdrv_free_stream(ctx, arg->tsid, arg->id_para.res_id, DEVDRV_FREE_BY_USER);
    if (ret != 0) {
        TSDRV_PRINT_ERR("free stream fail, ret(%d), stream_id(%u)\n", ret, arg->id_para.res_id);
    }
    return ret;
}

STATIC int devdrv_ioctl_free_stream(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifdef CFG_SOC_PLATFORM_MDC_V51
    if (tsdrv_get_chip_type() == CHIP_TYPE_MDC_BS9SX1A) {
        return devdrv_free_stream_from_hwts(ctx, arg);
    } else {
        return devdrv_free_stream_from_local_list(ctx, arg);
    }
#else
    return devdrv_free_stream_from_local_list(ctx, arg);
#endif
}

STATIC int devdrv_ioctl_alloc_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_id_info *event_id = NULL;

    ts_resource = tsdrv_ctx_to_ts_res(ctx, arg->tsid);
    ka_task_mutex_lock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
    event_id = devdrv_alloc_event_id(ctx, arg->tsid);
    ka_task_mutex_unlock(&ts_resource->id_res[TSDRV_EVENT_SW_ID].id_mutex_t);
    if (KA_IS_ERR_OR_NULL(event_id)) {
        if (PTR_ERR(event_id) == -EBUSY) {
            TSDRV_PRINT_WARN("event id exhausted, return event_id=%u\n", DEVDRV_MAX_SW_EVENT_ID - 1);
            arg->id_para.res_id = DEVDRV_MAX_SW_EVENT_ID - 1;
            arg->result = ID_IS_EXHAUSTED;
            return PTR_ERR(event_id);
        }
        /* event id is not exhausted, return minimum event id */
#ifndef TSDRV_UT
        arg->id_para.res_id = DEVDRV_MIN_HW_EVENT_ID;
        return PTR_ERR(event_id);
#endif
    }
    arg->id_para.res_id = event_id->id;
    return 0;
}

STATIC int devdrv_ioctl_free_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int ret = devdrv_free_event_id(ctx, arg->tsid, arg->id_para.res_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("free event fail, ret(%d)\n", ret);
        return ret;
    }

    return 0;
}

STATIC int devdrv_ioctl_alloc_ipc_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifndef CFG_SOC_PLATFORM_MINIV2
    /* In other platform, this func will enable tsfw to treat eventid as notify id,
     * which maybe cause some unpredictable err.
    */
    return 0;
#else
    struct tsdrv_res_query info;
    u32 id;
    int ret;

    ret = devdrv_alloc_ipc_event_id(ctx, arg->tsid, &id);
    if (ret != 0) {
        if (ret == -EBUSY) {
            arg->event_para.event_id = DEVDRV_MAX_HW_EVENT_ID;
        }
        return ret;
    }

    if (tsdrv_get_drv_ops()->resid_query != NULL) {
        info.cmd = TSDRV_RES_QUERY_ADDR;
        info.id_type = TSDRV_EVENT_HW_ID;
        info.id = id;

        ret = tsdrv_get_drv_ops()->resid_query(tsdrv_get_devid_by_ctx(ctx), arg->tsid, &info);
        if (ret != 0) {
            TSDRV_PRINT_ERR("get table value failed, (id=%d)\n", id);
            (void)devdrv_free_ipc_event_id(ctx, arg->tsid, (int)id, DEVDRV_NOTIFY_INFORM_TS);
            return ret;
        }
        arg->event_para.value = info.value;
        TSDRV_PRINT_DEBUG("get table value ok, (id=%d)\n", id);
    }

    arg->event_para.event_id = id;
    return 0;
#endif
}

STATIC int devdrv_ioctl_free_ipc_event(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifndef CFG_SOC_PLATFORM_MINIV2
    return 0;
#else
    int ret;

    ret = devdrv_free_ipc_event_id(ctx, arg->tsid, arg->event_para.event_id, DEVDRV_NOTIFY_INFORM_TS);
    if (ret != 0) {
        TSDRV_PRINT_ERR("free ipc event id fail ret(%d)\n", ret);
        return ret;
    }

    return 0;
#endif
}

STATIC int devdrv_ioctl_alloc_model(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int model_id;
    u32 devid = tsdrv_get_devid_by_ctx(ctx);

    model_id = devdrv_alloc_model_id(ctx, arg);
    if ((u32)model_id >= tsdrv_get_model_id_max_num(devid, arg->tsid)) {
        TSDRV_PRINT_ERR("alloc model id fail model_id(%d)\n", model_id);
        return -EINVAL;
    }
    arg->id_para.res_id = model_id;

    return 0;
}

STATIC int devdrv_ioctl_free_model(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    int ret = devdrv_free_model_id(ctx, arg->tsid, arg->id_para.res_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("free model id fail ret(%d)\n", ret);
        return ret;
    }

    return 0;
}

static long tsdrv_ioctl(unsigned int cmd, void __user *uarg, struct tsdrv_ctx *ctx)
{
    struct devdrv_ioctl_arg *ioctl_arg = NULL;
    u32 devid;
    u32 tsnum;
    int err = 0;

    if ((_KA_IOC_NR(cmd)) == (_KA_IOC_NR(TSDRV_CBSQCQ_WAIT_ID))) {
        return devdrv_ioctl_cbcq_wait(ctx, uarg);
    }

    if (devdrv_ioctl_handlers[_KA_IOC_NR(cmd)] == NULL) {
        TSDRV_PRINT_ERR("invalid cmd(%u)\n", _KA_IOC_NR(cmd));
        return -EFAULT;
    }

    ioctl_arg = tsdrv_kzalloc(sizeof(struct devdrv_ioctl_arg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ioctl_arg == NULL) {
        TSDRV_PRINT_ERR("kmalloc ioctl_arg fail.\n");
        return -EINVAL;
    }

    if (_KA_IOC_DIR(cmd) & _KA_IOC_WRITE) {
        if (copy_from_user_safe(ioctl_arg, uarg, sizeof(struct devdrv_ioctl_arg)) != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("copy from user fail cmd(%u)\n", _KA_IOC_NR(cmd));
            err = -ENOMEM;
            goto out;
#endif
        }
    }

    devid = tsdrv_get_devid_by_ctx(ctx);
    tsnum = tsdrv_get_dev_tsnum(devid);
    if (ioctl_arg->tsid >= tsnum) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid tsid(%u)\n", ioctl_arg->tsid);
        err = -EFAULT;
        goto out;
#endif
    }

    err = devdrv_ioctl_handlers[_KA_IOC_NR(cmd)](ctx, ioctl_arg);
    if (_KA_IOC_DIR(cmd) & _KA_IOC_READ) {
        if (copy_to_user_safe(uarg, ioctl_arg, sizeof(struct devdrv_ioctl_arg)) != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("copy to user fail cmd(%u)\n", _KA_IOC_NR(cmd));
            err = -EFAULT;
            goto out;
#endif
        }
    }
out:
    tsdrv_kfree(ioctl_arg);
    ioctl_arg = NULL;
    return err;
}

long devdrv_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    void __user *uarg = (void __user *)(uintptr_t)arg;
    struct tsdrv_ctx *ctx = filep->private_data;
    enum tsdrv_dev_status dev_status;
    int32_t ret;
    u32 devid;

    ret = (_KA_IOC_TYPE(cmd) != TSDRV_ID_MAGIC) || (_KA_IOC_NR(cmd) >= TSDRV_MAX_CMD) ||
        (uarg == NULL) || (ctx == NULL);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid param, cmd_type=%u cmd_nr=%u, arg=0x%pK, ctx=0x%pK\n",
            _KA_IOC_TYPE(cmd), _KA_IOC_NR(cmd), (void *)arg, (void *)(u64)(uintptr_t)ctx);
#endif
        return -EINVAL;
    }

    devid = tsdrv_get_devid_by_ctx(ctx);
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid=%u\n", devid);
        return -ENODEV;
    }

    /* hot reset will not destroy davinci device, so we have to check device status here */
    dev_status = tsdrv_get_dev_status(devid, TSDRV_PM_FID);
    if (dev_status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("invalid dev_status=%d devid=%u\n", (int)dev_status, devid);
        return -EBUSY;
    }

    if (tsdrv_ctx_is_run(ctx) != true) {
        TSDRV_PRINT_ERR("ctx is not run state, devid=%u tgid=%d\n", devid, ctx->tgid);
        return -EFAULT;
    }

    return tsdrv_ioctl(cmd, uarg, ctx);
}

/* mini mmap!! */
STATIC void devdrv_vm_open(struct vm_area_struct *vma)
{
}

STATIC void devrv_vm_close(struct vm_area_struct *vma)
{
}

STATIC int tsdrv_get_ssid(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
    u32 devid = tsdrv_get_devid_by_ctx(ctx);
    u32 fid = tsdrv_get_fid_by_ctx(ctx);
    u32 tsid = 0;
    int ssid;

    ssid = tsdrv_msg_sync_ssid(devid, tsid, (u32)ctx->tgid, fid);
    if (ssid == TSDRV_INVALID_SSID) {
        return -EINVAL;
    }
    ctx->ssid = ssid;

    if (devdrv_is_support_uio_d()) {
        arg->id_para.res_id = (u32)ctx->ssid;
    } else {
        arg->id_para.res_id = (u32)ctx->tgid;
    }

    return 0;
}

int devdrv_get_ssid(u32 devid, u32 tsid, int pid, u32 *ssid)
{
    enum tsdrv_dev_status dev_status;
    struct tsdrv_ctx *ctx = NULL;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= tsdrv_get_dev_tsnum(devid)) || (ssid == NULL)) {
        TSDRV_PRINT_ERR("Invalid para. (devid=%u; tsid=%u)\n", devid, tsid);
        return -EINVAL;
    }

    dev_status = tsdrv_get_dev_status(devid, TSDRV_PM_FID);
    if (dev_status != TSDRV_DEV_ACTIVE) {
        TSDRV_PRINT_ERR("Invalid dev status. (devid=%u; status=%d)\n", devid, (int)dev_status);
        return -EBUSY;
    }

    ctx = tsdrv_dev_proc_ctx_get(devid, TSDRV_PM_FID, pid);
    if (ctx == NULL) {
        TSDRV_PRINT_ERR("Invalid para. (devid=%u; tsid=%u; pid=%d)\n", devid, tsid, pid);
        return -ENODEV;
    }

    *ssid = tsdrv_msg_sync_ssid(devid, tsid, pid, TSDRV_PM_FID);
    if (*ssid == TSDRV_INVALID_SSID) {
        tsdrv_dev_proc_ctx_put(ctx);
        return -EINVAL;
    }

    tsdrv_dev_proc_ctx_put(ctx);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_get_ssid);

int hvtsdrv_ioctl_get_ssid(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg)
{
#ifndef TSDRV_UT
    return tsdrv_get_ssid(ctx, arg);
#endif
}
#ifndef AOS_LLVM_BUILD
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
STATIC vm_fault_t devdrv_vmf_fault(struct vm_fault *vmf)
#else
STATIC int devdrv_vmf_fault(struct vm_fault *vmf)
#endif
{
#ifndef TSDRV_UT
    TSDRV_PRINT_DEBUG("devdrv vm fault. flags:0x%x, start:0x%pK, end:0x%pK\n",
        vmf->flags, (void *)(uintptr_t)vmf->vma->vm_start, (void *)(uintptr_t)vmf->vma->vm_end);
    return DEVDRV_FAULT_ERROR;
#endif
}

#else
STATIC int devdrv_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    TSDRV_PRINT_DEBUG("devdrv vm fault. start:0x%pK, end:0x%pK\n",
        (void *)(uintptr_t)vma->vm_start, (void *)(uintptr_t)vma->vm_end);
    return DEVDRV_FAULT_ERROR;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
STATIC int devrv_vm_mremap(struct vm_area_struct * area)
{
    return -ENOTSUPP;
}
#endif

struct vm_operations_struct devdrv_vm_ops = {
    .open = devdrv_vm_open,
    .close = devrv_vm_close,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    .mremap = devrv_vm_mremap,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    .fault = devdrv_vmf_fault,
#else
    .fault = devdrv_vm_fault,
#endif
};

STATIC int devdrv_mmap_param_check(const ka_file_t *filep, const struct vm_area_struct *vma)
{
#ifndef TSDRV_UT
    if (filep == NULL) {
        TSDRV_PRINT_ERR("filep check failed\n");
        return -ENODEV;
    }
    if (filep->private_data == NULL) {
        TSDRV_PRINT_ERR("filep->private_data is NULL\n");
        return -EFAULT;
    }
    if (vma == NULL) {
        TSDRV_PRINT_ERR("vma is NULL\n");
        return -EINVAL;
    }
#endif
    return 0;
}

int devdrv_mmap(ka_file_t *filep, struct vm_area_struct *vma)
{
    struct tsdrv_ctx *ctx = NULL;
    int err = devdrv_mmap_param_check(filep, vma);
    if (err != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("mmap param check failed, err = %d\n", err);
        return -EINVAL;
#endif
    }

    ctx = filep->private_data;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    ka_mm_set_vm_flags(vma, KA_VM_DONTEXPAND | KA_VM_LOCKED | KA_VM_PFNMAP | KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_IO);
    if (!(vma->vm_flags & KA_VM_WRITE)) {
        vm_flags_clear(vma, VM_MAYWRITE);
    }
#else
    vma->vm_flags |= KA_VM_DONTEXPAND;
    vma->vm_flags |= KA_VM_LOCKED;
    vma->vm_flags |= KA_VM_PFNMAP;
    vma->vm_flags |= KA_VM_DONTDUMP;
    vma->vm_flags |= KA_VM_DONTCOPY;
    vma->vm_flags |= KA_VM_IO;
    if (!(vma->vm_flags & KA_VM_WRITE)) {
        vma->vm_flags &= ~VM_MAYWRITE;
    }
#endif
    vma->vm_ops = &devdrv_vm_ops;

    TSDRV_PRINT_DEBUG("vma->vm_start = 0x%pK, vma->vm_end = 0x%pK, vm_flag=0x%lx\n",
        (void *)(uintptr_t)vma->vm_start, (void *)(uintptr_t)vma->vm_end, ka_mm_get_vm_flags(vma));
    vma->vm_private_data = ctx;

    return 0;
}
#endif
STATIC int tsdrv_reboot_notify_handle(ka_notifier_block_t *notifier, unsigned long event, void *data)
{
#ifndef TSDRV_UT
    struct tsdrv_dev_resource *dev_res = NULL;
    enum tsdrv_dev_status status;
    u32 devid;

    if (event != SYS_RESTART && event != SYS_HALT && event != SYS_POWER_OFF) {
        return KA_NOTIFY_DONE;
    }

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
        if (dev_res == NULL) {
            continue;
        }
        status = ka_base_atomic_cmpxchg(&dev_res->status, TSDRV_DEV_ACTIVE, TSDRV_DEV_REBOOTNOTICE);
        if (status != TSDRV_DEV_ACTIVE) {
            TSDRV_PRINT_INFO("device %u status %d is not active, event 0x%lx.\n", devid, (int)status, event);
        }
        TSDRV_PRINT_DEBUG("device %u event 0x%lx.\n", devid, event);
    }
#endif
    return NOTIFY_OK;
}

const struct notifier_operations notifier_ops = {
    .notifier_call =  tsdrv_release_prepare,
};

STATIC ka_notifier_block_t tsdrv_reboot_notifier = {
    .notifier_call = tsdrv_reboot_notify_handle,
    .priority = 1,
};

STATIC bool devdrv_cq_info_is_invalid(struct devdrv_ts_cq_info *cq_info, int cq_index, u32 devid, u32 tsid)
{
    /* After the interrupt-associated cq is added to the linked list, this code can be deleted. */
    if (cq_info->type == TS_SQCQ_TYPE) {
        TSDRV_PRINT_DEBUG("Return TS_SQCQ_TYPE.\n");
        return true;
    }

    /* callback cq may not alloced */
    if (cq_info->alloc_status == SQCQ_INACTIVE) {
        return true;
    }

    if (callback_cq_match(cq_info->vfid, cq_index) == 0) {
        TSDRV_PRINT_DEBUG("fid(%u), cqid(%u), head(%u), tail(%u), cq phase(%u)\n",
            cq_info->vfid, cq_info->index, cq_info->head, cq_info->tail, cq_info->phase);
        callback_cq_proc(devid, cq_info->vfid, tsid, cq_index);
        return true;
    }

    if ((u32)cq_index != cq_info->index) {
        TSDRV_PRINT_ERR("cq_index != cq_info->index, cq_index: %d, cq_info->index: %u.\n", cq_index,
            cq_info->index);
        return true;
    }

    return false;
}

STATIC struct devdrv_report *tsdrv_normal_cq_get_report(enum tsdrv_env_type env_type,
    struct devdrv_cq_sub_info *cq_sub_info, const struct devdrv_ts_cq_info *cq_info, u32 index)
{
#ifndef TSDRV_UT
    struct devdrv_report *report = NULL;

#if (defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2))
    tsdrv_get_drv_ops()->flush_cache((u64)(uintptr_t)cq_sub_info->virt_addr,
        cq_info->slot_size * cq_info->depth);
#endif
    report = (struct devdrv_report *)((uintptr_t)((unsigned long)cq_sub_info->virt_addr +
        ((unsigned long)cq_info->slot_size * index)));
    if (env_type == TSDRV_ENV_ONLINE) {
        ka_mm_dma_sync_single_for_cpu(cq_sub_info->dev, cq_sub_info->phy_addr, cq_sub_info->size,
            KA_DMA_FROM_DEVICE);
    }
    return report;
#endif
}

STATIC void tsdrv_wakeup_normal_cq(struct tsdrv_ctx *ctx, u32 tsid)
{
#ifndef TSDRV_UT
    ctx->ts_ctx[tsid].cq_tail_updated = CQ_HEAD_UPDATE_FLAG;
    ka_mb();
    if (ka_task_waitqueue_active(&ctx->ts_ctx[tsid].report_wait) != 0) {
        ka_task_wake_up(&ctx->ts_ctx[tsid].report_wait);
    }
#endif
}

STATIC void tsdrv_rr_cq_proc(struct devdrv_int_context *int_context)
{
    struct devdrv_cq_sub_info *cq_sub_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_report *report = NULL;
    struct tsdrv_ctx *ctx = NULL;
    enum tsdrv_env_type env_type;
    phys_addr_t info_mem_addr;
    u32 tsid;
#ifndef CFG_SOC_PLATFORM_MDC_V51
    u32 fid;
#endif
    int cq_index;
    u32 devid;
    u32 phase;

    ts_resource = int_context->ts_resource;
    tsid = ts_resource->tsid;
    devid = tsdrv_get_devid_by_res(ts_resource);
    TSDRV_PRINT_DEBUG("Receive cq interrupts. (devid=%u; tsid=%u)\n", devid, tsid);

    info_mem_addr = ts_resource->mem_info[DEVDRV_INFO_MEM].virt_addr;
    env_type = tsdrv_get_env_type();
    for (cq_index = int_context->first_cq_index; cq_index <= int_context->last_cq_index; cq_index++) {
        cq_info = devdrv_calc_cq_info(info_mem_addr, cq_index);
        TSDRV_PRINT_DEBUG("receive report irq, (cq_index=%d)\n", cq_index);
        if (devdrv_cq_info_is_invalid(cq_info, cq_index, devid, tsid) == true) {
            continue;
        }
        cq_sub_info = (struct devdrv_cq_sub_info *)cq_info->cq_sub;
        ka_task_spin_lock_irq(&cq_sub_info->spinlock);
        if (cq_sub_info->virt_addr == 0) {
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            continue;
        }

        ctx = cq_sub_info->ctx;
        /* in recycle state, it need not to wake up vm wait wq */
#ifndef CFG_SOC_PLATFORM_MDC_V51
        if (!tsdrv_is_in_pm(devid)) {
            if (cq_info->type == SHM_SQCQ_TYPE) {
                TSDRV_PRINT_DEBUG("Handle shm cq. (cqid=%u)\n", cq_sub_info->index);
                shm_sqcq_phy_cq_handler(devid, tsid, cq_info);
                ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
                continue;
            }
            tsdrv_sqcq_dfx_set_phy_cq_tick(devid, cq_info->vfid, tsid, cq_index);   /* nothread tick for container*/
            if (ctx == NULL) {  /* ctx is null means this is vm scenario. */
                hvtsdrv_cq_handler(devid, cq_info->vfid, tsid, cq_info);
            } else if (tsdrv_ctx_is_run(ctx) == true) {
                fid = tsdrv_get_fid_by_ctx(ctx);
                hvtsdrv_cq_handler(devid, fid, tsid, cq_info);
            }
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            continue;
        }
#endif
        if (ctx == NULL) {
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            TSDRV_PRINT_DEBUG("The ctx is NULL. (devid=%u; cqid=%d)\n", devid, cq_index);
            continue;
        }
#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
        if (cq_info->type == SHM_SQCQ_TYPE) {
            shm_sqcq_phy_cq_handler(devid, tsid, cq_info);
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            continue;
        }
        if (cq_info->type == LOGIC_SQCQ_TYPE) {
            tsdrv_sqcq_dfx_set_phy_cq_tick(devid, cq_info->vfid, tsid, cq_index);   /* nothread tick */
            logic_sqcq_phy_cq_handler(devid, tsid, cq_info);
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            continue;
        }
#endif
        if (cq_sub_info->complete_handle != NULL) {
            cq_sub_info->complete_handle(cq_info);
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            TSDRV_PRINT_DEBUG("receive report irq");
            continue;
        }
        /* due to runtime no thread, cq tail not be updated. */
        if (cq_info->type == CTRL_SQCQ_TYPE) {
            tsdrv_sqcq_dfx_set_phy_cq_tick(devid, cq_info->vfid, tsid, cq_index);   /* ctrl type tick */
            tsdrv_parse_cqe_to_logic_cq(ts_resource, cq_info);
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            continue;
        }
        report = tsdrv_normal_cq_get_report(env_type, cq_sub_info, cq_info, cq_info->tail);
        phase = devdrv_report_get_phase(report);
#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3)
        TSDRV_PRINT_DEBUG("report: cqid(%u), phase(%u), warn(%u), evt(%u), place_hold(%u), "
            "sq_id(%u), sq_head(%u), stream_id(%u), task_id(%u), status(%u)\n",
            cq_info->index, (u32)report->phase, (u32)report->warn, (u32)report->event_record, (u32)report->place_hold,
            (u32)report->sq_id, (u32)report->sq_head, (u32)report->stream_id, (u32)report->task_id, report->status_low);
#endif
        /* for checking a thread is waiting for wake up */
        if (ctx->ts_ctx[tsid].cq_tail_updated != 0) { /* condition is true, continue */
            ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
            TSDRV_PRINT_DEBUG("[dev_id=%u]:receive report irq, cq id: %u,"
                "no runtime thread is waiting, not judge.\n",
                devid, cq_info->index);
            continue;
        }
        if (phase == cq_info->phase) {
            tsdrv_wakeup_normal_cq(ctx, tsid);
        }
        /* release spinlock after access cq's uio mem */
        ka_task_spin_unlock_irq(&cq_sub_info->spinlock);
    }
}

STATIC void devdrv_find_cq_index(unsigned long data)
{
    struct devdrv_int_context *int_context = (struct devdrv_int_context *)((uintptr_t)data);
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 devid, fid, tsid;

    ts_resource = int_context->ts_resource;
    tsid = ts_resource->tsid;
    fid = tsdrv_get_fid_by_res(ts_resource);
    devid = tsdrv_get_devid_by_res(ts_resource);
    tsdrv_sqcq_dfx_set_cq_tasklet_tick(devid, fid, tsid);

    tsdrv_rr_cq_proc(int_context);
}

STATIC ka_irqreturn_t devdrv_irq_handler(int irq, void *data)
{
#ifndef TSDRV_UT
    struct devdrv_int_context *int_context = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    unsigned long flags;
    u32 devid, fid, tsid;

    ka_task_local_irq_save(flags);
    int_context = (struct devdrv_int_context *)data;
    ts_resource = int_context->ts_resource;
    tsid = ts_resource->tsid;
    fid = tsdrv_get_fid_by_res(ts_resource);
    devid = tsdrv_get_devid_by_res(ts_resource);

    ka_system_tasklet_schedule(&int_context->find_cq_task);

    tsdrv_sqcq_dfx_set_cq_irq_tick(devid, fid, tsid);

    ka_task_local_irq_restore(flags);
#endif /* TSDRV_UT */
    return KA_IRQ_HANDLED;
}

#if defined(CFG_SOC_PLATFORM_MDC)
STATIC void tsdrv_set_cpumask_to_ctrlcpu(struct cpumask *cpumask, u32 ctrl_cpu_num)
{
    u32 i;
    for (i = 0; i < ctrl_cpu_num; i++) {
        ka_base_cpumask_set_cpu(i, cpumask);
    }
}


STATIC void tsdrv_clear_aicpu_from_cpumask(struct cpumask * cpumask, u32 ctrl_cpu_num, u32 ai_cpu_num)
{
    u32 i;
    for (i = ctrl_cpu_num; i < (ctrl_cpu_num + ai_cpu_num); i++) {
        ka_base_cpumask_clear_cpu(i, cpumask);
    }
}

/**
 * tsdrv_set_irq_cpumask: set irq cpu affinity
 * @irq_node_name:  The node name that defined to bind irq to core
 * @irq_cpu_name:   The property name that contains cpus
 *
 * By default, the CPU list obtained from DTS is bound. If the configured
 * CPU list contains AICPU, delete the AICPU. In other cases, bind all ctrlcpus.
 */
int32_t tsdrv_set_irq_cpumask(u32 devid, u32 irq, const char *irq_node_name, const char *irq_cpu_name)
{
#ifndef AOS_LLVM_BUILD
    struct devdrv_info *info = devdrv_manager_get_devdrv_info(devid);
#else
    struct devdrv_info *info = dms_get_devinfo(devid);
#endif
    struct cpumask *irq_cpumask = NULL;
    int32_t ret;
    u32 cpu_num;

    if (info == NULL) {
        TSDRV_PRINT_ERR("devdrv info is null.\n");
        return -EINVAL;
    }

    irq_cpumask = find_irq_cpumask(irq);
    if (irq_cpumask == NULL) {
        TSDRV_PRINT_ERR("devid(%u) irq(%u) cpu mask is null\n", devid, irq);
        return -EINVAL;
    }

    cpu_num = info->ctrl_cpu_core_num + info->ai_cpu_core_num;
    ret = get_irq_cpumask(irq_cpumask, irq_node_name, irq_cpu_name, cpu_num);
    if (ret != 0) {
        tsdrv_set_cpumask_to_ctrlcpu(irq_cpumask, info->ctrl_cpu_core_num);
    } else {
        tsdrv_clear_aicpu_from_cpumask(irq_cpumask, info->ctrl_cpu_core_num, info->ai_cpu_core_num);
        if (ka_base_cpumask_empty(irq_cpumask)) {
            tsdrv_set_cpumask_to_ctrlcpu(irq_cpumask, info->ctrl_cpu_core_num);
        }
    }

    ret = ka_base_irq_set_affinity_hint(irq, irq_cpumask);
    if (ret < 0) {
        TSDRV_PRINT_ERR("irq %u set affinity_hint failed. errcode=%d\n", irq, ret);
        return ret;
    }

    TSDRV_PRINT_INFO("node %s\'s core binding result of irq %u is: %*pbl\n",
        irq_node_name, irq, cpu_num, irq_cpumask);
    return 0;
}
#endif

STATIC int devdrv_request_irq_bh(u32 devid, struct tsdrv_ts_resource *ts_resource, struct tsdrv_cq_hwinfo *cq_hwinfo)
{
    u32 cq_irq_num;
    int err = 0;
    u32 i, j;
#ifdef CFG_SOC_PLATFORM_HELPER
    u32 first_ccpu, last_ccpu;
#endif

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    /* in ascend310, reserve the last irq for cbcq */
    cq_irq_num = cq_hwinfo->cq_irq_num - 1;
#else
    cq_irq_num = cq_hwinfo->cq_irq_num;
#endif

    TSDRV_PRINT_DEBUG("irq num(%u).\n", cq_irq_num);
    for (i = 0; i < cq_irq_num; i++) {
        ts_resource->int_context[i].ts_resource = ts_resource;
        ts_resource->int_context[i].index = i;
        devdrv_calc_irq_cq_range(i, cq_irq_num, DEVDRV_MAX_CQ_NUM,
            (u32 *)&ts_resource->int_context[i].first_cq_index, (u32 *)&ts_resource->int_context[i].last_cq_index);
#ifndef TSDRV_UT
        ka_system_tasklet_init(&ts_resource->int_context[i].find_cq_task, devdrv_find_cq_index,
            (unsigned long)(uintptr_t)&ts_resource->int_context[i]);
#endif
        TSDRV_PRINT_DEBUG("(cq_hwinfo->cq_irq[%u]=%d; cqid=%u)\n", i, cq_hwinfo->cq_irq[i],
            ts_resource->int_context[i].first_cq_index + i);

        err = tsdrv_request_irq(devid, cq_hwinfo->cq_request_irq[i], devdrv_irq_handler,
            &ts_resource->int_context[i], "devdrv_driver");
        TSDRV_PRINT_DEBUG("i = %u, cqirq(%u), request(%u).\n", i, cq_hwinfo->cq_irq[i], cq_hwinfo->cq_request_irq[i]);
        if (err != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("request irq fail, index %d irq %d err %d\n", i, cq_hwinfo->cq_irq[i], err);
            goto request_failed;
#endif
        }

#ifdef CFG_SOC_PLATFORM_HELPER
        err = tsdrv_get_cpu_index_range(devid, &first_ccpu, &last_ccpu);
        if (err != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Failed to get ctrl cpu range. (devid=%u; ret=%d)\n", devid, err);
            goto request_failed;
#endif
        }
        tsdrv_irq_set_affinity_hint(devid, cq_hwinfo->cq_request_irq[i], first_ccpu, last_ccpu);
#endif

#if defined(CFG_SOC_PLATFORM_MDC)
        /* set workqueue cpu affinity */
        if (tsdrv_set_irq_cpumask(devid, cq_hwinfo->cq_request_irq[i], "devdrv_driver", "cq_request_cpu") != 0) {
            TSDRV_PRINT_WARN("irq %u set affinity err\n", cq_hwinfo->cq_request_irq[i]);
        }
#endif
    }
    return err;
#ifndef TSDRV_UT
request_failed:
    j = i;
    for (i = 0; i < j; i++) {
        (void)tsdrv_unrequest_irq(devid, cq_hwinfo->cq_request_irq[i], &ts_resource->int_context[i]);
    }
    for (i = 0; i <= j; i++) {
        ka_system_tasklet_kill(&ts_resource->int_context[i].find_cq_task);
    }
    return err;
#endif
}

struct tsdrv_thread_bind_intr_mng *tsdrv_thread_bind_irq_mng_get(u32 devid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    return &ts_resource->thread_bind_intr_mng;
}

STATIC ka_irqreturn_t tsdrv_thread_bind_irq_handler(int irq, void *data)
{
#ifndef TSDRV_UT
    struct tsdrv_thread_bind_intr_ctx *intr_ctx = (struct tsdrv_thread_bind_intr_ctx *)data;

    if (intr_ctx->valid != 0) {
        ka_wait_queue_head_t *wait_queue = intr_ctx->wait_queue;
        ka_atomic_t *wait_flag = intr_ctx->wait_flag;

        if (wait_flag != NULL) {
            ka_base_atomic_set(wait_flag, 1);
            ka_wmb();
        }

        if (wait_queue != NULL) {
            ka_task_wake_up(wait_queue);
        }
    }
#endif
    return KA_IRQ_HANDLED;
}

int tsdrv_thread_bind_irq_init(u32 devid, u32 tsid)
{
    struct tsdrv_thread_bind_intr_mng *intr_mng = tsdrv_thread_bind_irq_mng_get(devid, tsid);
    struct tsdrv_cq_hwinfo *cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    int ret = 0;
    u32 i, j;

    ka_task_mutex_init(&intr_mng->mutex);

    for (i = 0; i < cq_hwinfo->thread_bind_irq_num; i++) {
        intr_mng->intr_ctx[i].valid = 0;
        intr_mng->intr_ctx[i].wait_flag = NULL;
        intr_mng->intr_ctx[i].wait_queue = NULL;

        ret = tsdrv_request_irq(devid, cq_hwinfo->cq_request_irq[cq_hwinfo->cq_irq_num + i],
            tsdrv_thread_bind_irq_handler, &intr_mng->intr_ctx[i], "tsdrv_thread_bind");
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("thread bind irq %d id %d num register failed.",
                cq_hwinfo->cq_request_irq[cq_hwinfo->cq_irq_num + i], i);
            goto err;
#endif
        }
    }

    return 0;
#ifndef TSDRV_UT
err:
    j = i;
    for (i = 0; i < j; i++) {
        (void)tsdrv_unrequest_irq(devid, cq_hwinfo->cq_request_irq[cq_hwinfo->cq_irq_num + i], &intr_mng->intr_ctx[i]);
    }
    ka_task_mutex_destroy(&intr_mng->mutex);

    return ret;
#endif
}

void tsdrv_thread_bind_irq_uninit(u32 devid, u32 tsid)
{
    struct tsdrv_thread_bind_intr_mng *intr_mng = tsdrv_thread_bind_irq_mng_get(devid, tsid);
    struct tsdrv_cq_hwinfo *cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    u32 i;

    for (i = 0; i < cq_hwinfo->thread_bind_irq_num; i++) {
        (void)tsdrv_unrequest_irq(devid, cq_hwinfo->cq_request_irq[cq_hwinfo->cq_irq_num + i], &intr_mng->intr_ctx[i]);
    }

    ka_task_mutex_destroy(&intr_mng->mutex);
}

int tsdrv_thread_bind_irq_alloc(u32 devid, u32 tsid)
{
#ifndef TSDRV_UT
    struct tsdrv_thread_bind_intr_mng *intr_mng = tsdrv_thread_bind_irq_mng_get(devid, tsid);
    struct tsdrv_cq_hwinfo *cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    u32 i;

    ka_task_mutex_lock(&intr_mng->mutex);

    for (i = 0; i < cq_hwinfo->thread_bind_irq_num; i++) {
        if (intr_mng->intr_ctx[i].valid == 0) {
            intr_mng->intr_ctx[i].valid = 1;
            ka_task_mutex_unlock(&intr_mng->mutex);
            return (int)cq_hwinfo->cq_irq[cq_hwinfo->cq_irq_num + i];
        }
    }

    ka_task_mutex_unlock(&intr_mng->mutex);
#endif
    return -1;
}

int tsdrv_thread_bind_irq_id_get(struct tsdrv_cq_hwinfo *cq_hwinfo, int irq)
{
#ifndef TSDRV_UT
    u32 i;

    for (i = 0; i < cq_hwinfo->thread_bind_irq_num; i++) {
        if (cq_hwinfo->cq_irq[cq_hwinfo->cq_irq_num + i] == (u16)irq) {
            return i;
        }
    }
#endif
    return -1;
}

void tsdrv_thread_bind_irq_free(u32 devid, u32 tsid, int irq)
{
#ifndef TSDRV_UT
    struct tsdrv_thread_bind_intr_mng *intr_mng = tsdrv_thread_bind_irq_mng_get(devid, tsid);
    struct tsdrv_cq_hwinfo *cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    int irq_id = tsdrv_thread_bind_irq_id_get(cq_hwinfo, irq);

    ka_task_mutex_lock(&intr_mng->mutex);

    if (((u32)irq_id < cq_hwinfo->thread_bind_irq_num) && (irq_id >= 0)) {
        if (intr_mng->intr_ctx[irq_id].valid == 1) {
            intr_mng->intr_ctx[irq_id].valid = 0;
            intr_mng->intr_ctx[irq_id].wait_flag = NULL;
            intr_mng->intr_ctx[irq_id].wait_queue = NULL;
        }
    }

    ka_task_mutex_unlock(&intr_mng->mutex);
#endif
}

#ifndef TSDRV_UT
void tsdrv_thread_bind_irq_set_wait_para(u32 devid, u32 tsid, int irq, ka_wait_queue_head_t *wait_queue,
    ka_atomic_t *wait_flag)
{
    struct tsdrv_thread_bind_intr_mng *intr_mng = tsdrv_thread_bind_irq_mng_get(devid, tsid);
    struct tsdrv_cq_hwinfo *cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    int irq_id = tsdrv_thread_bind_irq_id_get(cq_hwinfo, irq);

    ka_task_mutex_lock(&intr_mng->mutex);

    if (((u32)irq_id < cq_hwinfo->thread_bind_irq_num) && (irq_id >= 0)) {
        if (intr_mng->intr_ctx[irq_id].valid == 1) {
            intr_mng->intr_ctx[irq_id].wait_flag = wait_flag;
            intr_mng->intr_ctx[irq_id].wait_queue = wait_queue;
        }
    }

    ka_task_mutex_unlock(&intr_mng->mutex);
}
#endif

STATIC void devdrv_free_irq_bh(u32 devid, struct tsdrv_ts_resource *ts_resource, struct tsdrv_cq_hwinfo *cq_hwinfo)
{
    u32 cq_irq_num;
    u32 i;

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(CFG_SOC_PLATFORM_MINIV3)
    cq_irq_num = cq_hwinfo->cq_irq_num - 1;
#else
    cq_irq_num = cq_hwinfo->cq_irq_num;
#endif

    for (i = 0; i < cq_irq_num; i++) {
        (void)tsdrv_unrequest_irq(devid, cq_hwinfo->cq_request_irq[i], &ts_resource->int_context[i]);
        ka_system_tasklet_kill(&ts_resource->int_context[i].find_cq_task);
    }
}

int tsdrv_cq_irq_init(u32 devid, u32 tsnum)
{
    struct tsdrv_dev_resource *ts_dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    struct tsdrv_cq_hwinfo *cq_hwinfo = NULL;
    u32 tsid;
    int err;
    u32 i;

    if (ts_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, TSDRV_PM_FID);
        return -EINVAL;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
        err = devdrv_request_irq_bh(devid, &ts_dev_res->ts_resource[tsid], cq_hwinfo);
        if (err != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("request irq fail, devid(%u) tsid(%u)\n", devid, tsid);
            goto err_request_irq;
#endif
        }

        err = tsdrv_thread_bind_irq_init(devid, tsid);
        if (err != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("thread bind irq init fail, devid(%u) tsid(%u)\n", devid, tsid);
            devdrv_free_irq_bh(devid, &ts_dev_res->ts_resource[tsid], cq_hwinfo);
            goto err_request_irq;
#endif
        }
    }
    return 0;
#ifndef TSDRV_UT
err_request_irq:
    for (i = 0; i < tsid; i++) {
        devdrv_free_irq_bh(devid, &ts_dev_res->ts_resource[i], cq_hwinfo);
        tsdrv_thread_bind_irq_uninit(devid, i);
    }
    return -ENODEV;
#endif
}

void tsdrv_cq_irq_exit(u32 devid, u32 tsnum)
{
    struct tsdrv_dev_resource *ts_dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    struct tsdrv_cq_hwinfo *cq_hwinfo = NULL;
    u32 tsid;

    if (ts_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", devid, TSDRV_PM_FID);
        return;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
        devdrv_free_irq_bh(devid, &ts_dev_res->ts_resource[tsid], cq_hwinfo);
        tsdrv_thread_bind_irq_uninit(devid, tsid);
    }
}

STATIC void tsdrv_drv_ops_init(void)
{
#ifndef AOS_LLVM_BUILD
    struct tsdrv_drv_ops *tsdrv_drvops = devdrv_manager_get_drv_ops();
#else
    struct tsdrv_drv_ops *tsdrv_drvops = tsdrv_get_drv_ops();
#endif
    if (tsdrv_drvops == NULL) {
        TSDRV_PRINT_ERR("tsdrv_drvops is null\n");
        return;
    }

    tsdrv_drvops->wakeup_all_ctx = NULL;

    tsdrv_drvops->create_functional_sq = devdrv_create_functional_sq;
    tsdrv_drvops->functional_set_sq_func = devdrv_functional_set_sq_func;
    tsdrv_drvops->functional_send_sq = devdrv_functional_send_sq;
    tsdrv_drvops->destroy_functional_sq = devdrv_destroy_functional_sq;
    tsdrv_drvops->create_functional_cq = devdrv_create_functional_cq;
    tsdrv_drvops->functional_set_cq_func = devdrv_functional_set_cq_func;
    tsdrv_drvops->destroy_functional_cq = devdrv_destroy_functional_cq;

    tsdrv_drvops->mailbox_kernel_sync_no_feedback = tsdrv_mailbox_kernel_sync_no_feedback;

#ifdef CFG_FEATURE_IPC_NOTIFY
#ifndef AOS_LLVM_BUILD
    devdrv_manager_ops_sem_down_write();
#endif
    tsdrv_drvops->ipc_notify_create = devdrv_manager_ipc_notify_create;
    tsdrv_drvops->ipc_notify_destroy = devdrv_manager_ipc_notify_destroy;
    tsdrv_drvops->ipc_notify_open = devdrv_manager_ipc_notify_open;
    tsdrv_drvops->ipc_notify_close = devdrv_manager_ipc_notify_close;
    tsdrv_drvops->ipc_notify_set_pid = devdrv_manager_ipc_notify_set_pid;
    tsdrv_drvops->ipc_notify_record = devdrv_manager_ipc_notify_record;
    tsdrv_drvops->ipc_notify_release_recycle = devdrv_manager_ipc_notify_release_recycle;
    tsdrv_drvops->ipc_notify_set_attr = devdrv_manager_ipc_notify_set_attr;
    tsdrv_drvops->ipc_notify_get_info = devdrv_manager_ipc_notify_get_info;
    tsdrv_drvops->ipc_notify_get_attr = devdrv_manager_ipc_notify_get_attr;
#ifndef AOS_LLVM_BUILD
    devdrv_manager_ops_sem_up_write();
#endif
#endif

    return;
}

STATIC void tsdrv_drv_ops_uninit(void)
{
#ifndef AOS_LLVM_BUILD
    struct tsdrv_drv_ops *tsdrv_drvops = devdrv_manager_get_drv_ops();
#else
    struct tsdrv_drv_ops *tsdrv_drvops = tsdrv_get_drv_ops();
#endif
    if (tsdrv_drvops == NULL) {
        TSDRV_PRINT_ERR("tsdrv_drvops is null\n");
        return;
    }

    tsdrv_drvops->create_functional_sq = NULL;
    tsdrv_drvops->functional_set_sq_func = NULL;
    tsdrv_drvops->functional_send_sq = NULL;
    tsdrv_drvops->create_functional_cq = NULL;
    tsdrv_drvops->functional_set_cq_func = NULL;
    tsdrv_drvops->destroy_functional_cq = NULL;
    tsdrv_drvops->destroy_functional_sq = NULL;

    tsdrv_drvops->mailbox_kernel_sync_no_feedback = NULL;

    tsdrv_drvops->wakeup_all_ctx = NULL;

#ifdef CFG_FEATURE_IPC_NOTIFY
#ifndef AOS_LLVM_BUILD
    devdrv_manager_ops_sem_down_write();
#endif
    tsdrv_drvops->ipc_notify_create = NULL;
    tsdrv_drvops->ipc_notify_open = NULL;
    tsdrv_drvops->ipc_notify_close = NULL;
    tsdrv_drvops->ipc_notify_destroy = NULL;
    tsdrv_drvops->ipc_notify_set_pid = NULL;
    tsdrv_drvops->ipc_notify_release_recycle = NULL;
    tsdrv_drvops->ipc_notify_set_attr = NULL;
    tsdrv_drvops->ipc_notify_get_attr = NULL;
    tsdrv_drvops->ipc_notify_get_info = NULL;
#ifndef AOS_LLVM_BUILD
    devdrv_manager_ops_sem_up_write();
#endif
#endif
    return;
}

STATIC int devdrv_ids_init(u32 devid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = NULL;
    int ret;

    ts_res = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    ret = tsdrv_vsq_info_mem_create(devid, TSDRV_PM_FID, tsid);
    if (ret != 0) {
        TSDRV_PRINT_ERR("init vsq info failed with ret(%d)\n", ret);
#ifndef TSDRV_UT
        goto init_vsq_info_fail;
#endif
    }
#endif
    ret = devdrv_shm_init(devid, TSDRV_PM_FID, tsid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("shm init failed with ret(%d)\n", ret);
        goto shm_init_fail;
#endif
    }
    ret = devdrv_cqsq_init(devid, tsid, DEVDRV_MAX_SQ_NUM, DEVDRV_MAX_CQ_NUM);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devdrv_cqsq_init error with ret(%d)\n", ret);
        goto sqcq_init_fail;
#endif
    }
    ret = devdrv_stream_id_init(devid, TSDRV_PM_FID, tsid, tsdrv_get_stream_id_max_num(devid, tsid));
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devdrv_id_init fail with ret(%d)\n", ret);
        goto stream_id_init_fail;
#endif
    }
    ret = devdrv_event_id_init(devid, TSDRV_PM_FID, tsid, DEVDRV_MAX_SW_EVENT_ID);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devdrv_id_init fail with ret(%d)\n", ret);
        goto event_id_init_fail;
#endif
    }
    ret = devdrv_model_id_init(devid, TSDRV_PM_FID, tsid, tsdrv_get_model_id_max_num(devid, tsid));
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devdrv_model_id_init fail with ret(%d)\n", ret);
        goto model_id_init_failed;
#endif
    }
    ret = devdrv_notify_id_init(devid, TSDRV_PM_FID, tsid, DEVDRV_MAX_NOTIFY_ID);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devdrv_model_id_init fail with ret(%d)\n", ret);
        goto notify_id_init_failed;
#endif
    }
    ret = tsdrv_cmo_id_init(devid, TSDRV_PM_FID, tsid, DEVDRV_MAX_CMO_ID);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devdrv_cmo_id_init fail with ret(%d)\n", ret);
        goto cmo_id_init_failed;
#endif
    }

    return 0;
#ifndef TSDRV_UT
cmo_id_init_failed:
    devdrv_notify_id_destroy(devid, TSDRV_PM_FID, tsid);
notify_id_init_failed:
    devdrv_model_id_destroy(devid, TSDRV_PM_FID, tsid);
model_id_init_failed:
    devdrv_event_id_destroy(devid, TSDRV_PM_FID, tsid);
event_id_init_fail:
    devdrv_stream_id_destroy(devid, TSDRV_PM_FID, tsid);
stream_id_init_fail:
    devdrv_cqsq_destroy(devid, tsid);
sqcq_init_fail:
    devdrv_shm_destroy(devid, TSDRV_PM_FID, tsid);
shm_init_fail:
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    tsdrv_vsq_info_mem_destroy(devid, TSDRV_PM_FID, tsid);
init_vsq_info_fail:
#endif

    return ret;
#endif  /* TSDRV_UT */
}

STATIC void devdrv_ids_destroy(u32 devid, u32 tsid)
{
    tsdrv_cmo_id_destroy(devid, TSDRV_PM_FID, tsid);
    devdrv_notify_id_destroy(devid, TSDRV_PM_FID, tsid);
    devdrv_model_id_destroy(devid, TSDRV_PM_FID, tsid);
    devdrv_event_id_destroy(devid, TSDRV_PM_FID, tsid);
    devdrv_stream_id_destroy(devid, TSDRV_PM_FID, tsid);
    devdrv_cqsq_destroy(devid, tsid);
    devdrv_shm_destroy(devid, TSDRV_PM_FID, tsid);
#ifdef CFG_FEATURE_SUPPORT_VSQ_MNG
    tsdrv_vsq_info_mem_destroy(devid, TSDRV_PM_FID, tsid);
#endif
}

int devdrv_wakeup_cce_context_status(ka_pid_t pid, u32 devid, u32 status)
{
#ifndef TSDRV_UT
    struct tsdrv_ctx *ctx = NULL;

    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid(%u)\n", devid);
        return -ENODEV;
    }
    if (pid <= 0) {
        TSDRV_PRINT_ERR("wrong pid=%d devid=%u\n", pid, devid);
        return -ESRCH;
    }
    if (tsdrv_get_env_type() == TSDRV_ENV_OFFLINE) {
        return -ESRCH;
    }
    ctx = tsdrv_dev_proc_ctx_get(devid, TSDRV_PM_FID, pid);
    if (ctx == NULL) {
        return -ESRCH;
    }
    tsdrv_set_mirror_ctx_status(ctx, status);
    tsdrv_dev_proc_ctx_put(ctx);
    return 0;
#endif /* TSDRV_UT */
}
KA_EXPORT_SYMBOL_GPL(devdrv_wakeup_cce_context_status);

STATIC int devdrv_basic_module_init(struct devdrv_info *dev_info)
{
    u32 tsnum = dev_info->ts_num;
    u32 devid = dev_info->dev_id;
    u32 tsid;
    int err;
    u32 i;

    err = tsdrv_mbox_init(devid, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_mbox_init;
#endif
    }
    err = tsdrv_dfx_cqsq_init(devid, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_dfx_cqsq_init;
#endif
    }
    err = tsdrv_cq_irq_init(devid, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_cb_irq_init;
#endif
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = devdrv_ids_init(devid, tsid);
        if (err != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("id init failed\n");
            goto devdrv_ids_init_fail;
#endif
        }
    }
    err = tsdrv_nvme_init(devid, TSDRV_PM_FID, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_nvme_init;
#endif
    }

    /* must be initiated after id */
    err = callback_dev_init(devid, TSDRV_PM_FID, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_callback_init;
#endif
    }

    err = tsdrv_sqcq_dfx_dev_init(devid, TSDRV_PM_FID, tsnum);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_sqcq_dfx_dev_init;
#endif
    }

    return 0;
#ifndef TSDRV_UT
err_sqcq_dfx_dev_init:
    callback_dev_exit(devid, TSDRV_PM_FID, tsnum);
err_callback_init:
    tsdrv_nvme_exit(devid, TSDRV_PM_FID, tsnum);
err_nvme_init:
    for (i = 0; i < tsid; i++) {
        devdrv_ids_destroy(devid, i);
    }
devdrv_ids_init_fail:
    tsdrv_cq_irq_exit(devid, tsnum);
err_cb_irq_init:
    tsdrv_dfx_cqsq_exit(devid, tsnum);
err_dfx_cqsq_init:
    tsdrv_mbox_exit(devid, tsnum);
err_mbox_init:
    tsdrv_dev_unregister(dev_info->dev_id, TSDRV_PM_FID);
    DRV_PRINT_END();
    return err;
#endif /* TSDRV_UT */
}

STATIC void devdrv_basic_module_exit(struct devdrv_info *dev_info)
{
#ifndef TSDRV_UT
    u32 tsnum = dev_info->ts_num;
    u32 devid = dev_info->dev_id;
    u32 i;

    tsdrv_sqcq_dfx_dev_exit(devid, TSDRV_PM_FID, tsnum);
    callback_dev_exit(devid, TSDRV_PM_FID, tsnum);

    tsdrv_nvme_exit(devid, TSDRV_PM_FID, tsnum);

    for (i = 0; i < tsnum; i++) {
        devdrv_ids_destroy(devid, i);
    }

    tsdrv_cq_irq_exit(devid, tsnum);

    tsdrv_dfx_cqsq_exit(devid, tsnum);

    tsdrv_mbox_exit(devid, tsnum);

    tsdrv_dev_unregister(dev_info->dev_id, TSDRV_PM_FID);
    DRV_PRINT_END();
#endif
}

/* *
 * devdrv_drv_register - register a new devdrv device
 * @devdrv_info: devdrv device info
 *
 * returns zero on success
 */
int devdrv_drv_register(struct devdrv_info *dev_info)
{
    struct tsdrv_device *tsdrv_dev = NULL;
    u32 tsnum = dev_info->ts_num;
    u32 devid = dev_info->dev_id;
    u32 chipid = dev_info->chip_id;
    u32 dieid = dev_info->die_id;
    u32 tsid;
    int err;

    tsdrv_delay_free_mem(devid);
    TSDRV_PRINT_INFO("TS register start. (devid=%u; tsnum=%u)\n", devid, tsnum);
    err = tsdrv_device_init(devid);
    if (err != 0) {
        return err;
    }
    err = tsdrv_hwts_init(devid);
    if (err != 0) {
#ifndef TSDRV_UT
        goto err_hwts_init;
#endif
    }
    tsdrv_dev = tsdrv_get_dev(devid);
    tsdrv_dev_set_id_range(dev_info, tsnum);
#ifdef CFG_SOC_PLATFORM_MDC_V51
    tsdrv_set_logic_cq_id_max_num(dev_info);
#endif
    tsdrv_dev_ctx_init(tsdrv_dev->dev_ctx, TSDRV_MAX_FID_NUM);

    err = tsdrv_dev_hwinfo_init(devid);
    if (err != 0) {
        goto err_dev_hwinfo_init;
    }

    err = tsdrv_set_dev_hwinfo(dev_info);
    if (err != 0) {
        goto err_set_dev_hwinfo;
    }

    err = tsdrv_bar_space_init_conflict_check(devid);
    if (err != 0) {
        goto err_conflict_chk;
    }

    if (tsdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        err = tsdrv_dev_register(devid, TSDRV_PM_FID, FULL_CAPACITY);
        if (err != 0) {
            goto err_dev_register;
        }
        err = devdrv_basic_module_init(dev_info);
        if (err != 0) {
            goto err_base_module_init;
        }
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = tsdrv_alloc_ts_sqcq(devid, TSDRV_PM_FID, tsid);
        if (err != 0) {
            goto err_ts_sqcq_init;
        }
    }

    tsdrv_init_taskid_share_memory(devid, tsnum, chipid, dieid);
    tsdrv_dev_ref_set(devid, TSDRV_PM_FID, 0);
    tsdrv_set_dev_status(devid, TSDRV_PM_FID, TSDRV_DEV_ACTIVE, TSDRV_PHYSICAL_TYPE);
    tsdrv_set_runtime_run_init(devid);
    TSDRV_PRINT_INFO("TS register finish. (devid=%u; fid=%d; tsnum=%u; host_flag=%d)\n",
        devid, TSDRV_PM_FID, tsnum, (int)tsdrv_get_host_flag(devid));
    return 0;
err_ts_sqcq_init:
    if (tsdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        devdrv_basic_module_exit(dev_info);
    }
err_base_module_init:
    if (tsdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        tsdrv_dev_unregister(devid, TSDRV_PM_FID);
    }
err_dev_register:
err_conflict_chk:
err_set_dev_hwinfo:
    tsdrv_dev_hwinfo_exit(devid);
err_dev_hwinfo_init:
#ifndef TSDRV_UT
    tsdrv_hwts_uninit(devid);
err_hwts_init:
#endif
    tsdrv_device_exit(devid);
    return err;
}
KA_EXPORT_SYMBOL_GPL(devdrv_drv_register);

/* *
 * devdrv_drv_unregister - unregister a devdrv device
 * @devdrv_info: devdrv device info
 *
 * returns zero on success
 */
void devdrv_drv_unregister(struct devdrv_info *dev_info)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    u32 devid = dev_info->dev_id;
    enum tsdrv_dev_status status;
    u32 tsnum;
    u32 tsid;

    status = tsdrv_get_dev_status(devid, TSDRV_PM_FID);
    if (status != TSDRV_DEV_ACTIVE &&
        status != TSDRV_DEV_HOTRESET &&
        status != TSDRV_DEV_REBOOTNOTICE) {
        TSDRV_PRINT_DEBUG("devid=%u; status=%u\n", devid, (unsigned int)status);
        return;
    }

    TSDRV_PRINT_INFO("devid:%u start to unregister.\n", devid);
    dev_res = tsdrv_get_dev_resource(devid, TSDRV_PM_FID);
    if (dev_res == NULL) {
        return;
    }

    tsdrv_dev_ref_check(devid, TSDRV_PM_FID);
    ka_base_atomic_set(&dev_res->status, TSDRV_DEV_REBOOTING);

    tsnum = tsdrv_get_dev_tsnum(devid);

    if (tsdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_PF) {
        /* cq irq must exit first because some variables will be used in irq, which may cause access to freed memory */
        tsdrv_cq_irq_exit(devid, tsnum);
        tsdrv_nvme_exit(devid, TSDRV_PM_FID, tsnum);
        tsdrv_sqcq_dfx_dev_exit(devid, TSDRV_PM_FID, tsnum);
        callback_dev_exit(devid, TSDRV_PM_FID, tsnum); // callback must exit before mailbox exit
        callback_g_info_exit_all_fids(devid, tsnum);
        tsdrv_mbox_exit(devid, tsnum);
        tsdrv_dfx_cqsq_exit(devid, tsnum);

        /* the wq of ts_res may be in waitting ts reports (ts may be in down state), it must be exit before ids destroy
         * because the wq will visit ids addr, eg model id
         */
        ka_task_mutex_lock(&dev_res->dev_res_lock);
        tsdrv_ts_res_wq_exit(dev_res->ts_resource, tsnum);
        ka_task_mutex_unlock(&dev_res->dev_res_lock);

        for (tsid = 0; tsid < tsnum; tsid++) {
            devdrv_ids_destroy(devid, tsid);
        }
        tsdrv_dev_unregister(devid, TSDRV_PM_FID);
    }
    tsdrv_dev_hwinfo_exit(devid);
    tsdrv_hwts_uninit(devid);
    tsdrv_device_exit(devid);
    TSDRV_PRINT_INFO("devid:%u unregister success\n", devid);
}
KA_EXPORT_SYMBOL_GPL(devdrv_drv_unregister);

STATIC int __init devdrv_devinit(void)
{
    struct hdcdrv_register_symbol hdc_symbol = {
        KA_THIS_MODULE,
        devdrv_wakeup_cce_context_status};
    ka_file_operations_t *fop = NULL;
    u32 devid;
    int ret;
#ifdef CFG_SOC_PLATFORM_MDC_V51
    devdrv_init_ts_node_num();
    if (devdrv_get_ts_node_num() == 0) {
        TSDRV_PRINT_INFO("tsdrv devinit end, ts node does not exist!\n");
        return 0;
    }
#endif
    TSDRV_PRINT_INFO("tsdrv devinit start\n");
    tsdrv_drv_ops_init();

    fop = tsdrv_get_comm_fops();
    ret = drv_davinci_register_sub_parallel_module(DAVINCI_INTF_MODULE_TSDRV, fop);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("register sub module fail, err(%d)\n", ret);
        goto davinci_register_module_fail;
#endif
    }

    ret = drv_ascend_register_notify(DAVINCI_INTF_MODULE_TSDRV, &notifier_ops);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("register devdrv_device notify fail, err(%d)\n", ret);
        goto davinci_register_notify_fail;
#endif
    }

    ret = ka_dfx_register_reboot_notifier(&tsdrv_reboot_notifier);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("register reboot notifier fail, err(%d)\n", ret);
        goto davinci_register_notify_fail;
#endif
    }

    ret = tsdrv_device_setup();
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("tsdrv dev init fail, err(%d)\n", ret);
        goto tsdrv_device_setup_fail;
#endif
    }

    tsdrv_dev_id_info_init();

    /* fixit: it started before mbox initiated */
    devdrv_config_p2p();

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        tsdrv_mem_delay_free_init(devid);
    }

    if (tsdrv_get_drv_ops()->hdc_register_symbol != NULL) {
#ifndef TSDRV_UT
        tsdrv_get_drv_ops()->hdc_register_symbol(&hdc_symbol);
#endif
    }

    ret = module_feature_auto_init();
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Register event update fail. (ret=%d)\n", ret);
        goto module_feature_auto_init_fail;
#endif
    }

    TSDRV_PRINT_INFO("tsdrv devinit end\n");
    return 0;
#ifndef TSDRV_UT
module_feature_auto_init_fail:
    tsdrv_device_cleanup();
tsdrv_device_setup_fail:
    (void)ka_dfx_unregister_reboot_notifier(&tsdrv_reboot_notifier);
davinci_register_notify_fail:
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TSDRV);
davinci_register_module_fail:
    tsdrv_drv_ops_uninit();
    return -ENODEV;
#endif
}
ka_module_init(devdrv_devinit);

STATIC void __exit devdrv_devexit(void)
{
    u32 devid;
    int32_t ret;

    TSDRV_PRINT_INFO("tsdrv devexit start\n");

    if (tsdrv_get_drv_ops()->hdc_unregister_symbol != NULL) {
#ifndef TSDRV_UT
        tsdrv_get_drv_ops()->hdc_unregister_symbol();
#endif
    }

    devdrv_deconfig_p2p();

    (void)ka_dfx_unregister_reboot_notifier(&tsdrv_reboot_notifier);
    ret = drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TSDRV);
    if (ret != 0) {
        TSDRV_PRINT_ERR("unregister sub module failed err(%d)\n", ret);
    }
    tsdrv_drv_ops_uninit();
    tsdrv_device_cleanup();

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        tsdrv_delay_free_mem(devid);
    }

    TSDRV_PRINT_INFO("tsdrv devexit end\n");
}

ka_module_exit(devdrv_devexit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");

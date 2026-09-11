/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Description: suspend and resume in the pm system
 * Author: huawei
 * Create: 2023-12-22
 */
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/of.h>
#include "devdrv_pci.h"
#include "devdrv_pcie_link_info.h"
#include "devdrv_util.h"
#include "devdrv_dma.h"
#include "devdrv_ctrl.h"
#include "devdrv_interface.h"
#include "devdrv_msg_def.h"
#include "devdrv_msg.h"
#include "devdrv_pm.h"
#include "res_drv.h"
#include "devdrv_mem_alloc.h"
#ifdef CFG_FEATURE_PCIE_SENTRY
#include "devdrv_sentry.h"
#endif

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

STATIC struct pci_saved_state *pci_saved_state = NULL;
STATIC DECLARE_WAIT_QUEUE_HEAD(resume_notify_wq);
STATIC int g_resume_notify = 0;

#ifndef CFG_FEATURE_PCIE_SENTRY
bool devdrv_is_sentry_work_mode(void)
{
    return false;
}

STATIC int devdrv_sentry_save_rp_config_space(void)
{
    return 0;
}

STATIC void devdrv_sentry_wake_up_exit_event(void)
{
    return;
}

STATIC int devdrv_sentry_restore_rp_config_space(void)
{
    return 0;
}

STATIC int devdrv_sentry_runtime_resume_device(struct pci_dev *pdev)
{
    return 0;
}
#endif

STATIC void devdrv_dma_suspend(struct devdrv_dma_dev *dma_dev, u32 sriov_flag)
{
    u32 i;
    struct devdrv_dma_channel *dma_chan = NULL;

    if (dma_dev == NULL) {
        devdrv_err("dma dev is null.\n");
        return;
    }

    tasklet_kill(&dma_dev->single_fault_task);
    if ((devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_PHY_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_SRIOV_VF_BOOT)) {
        if ((dma_dev->guard_work.dma_guard_work.work.func != NULL) &&
            (dma_dev->guard_work.work_magic == DEVDRV_DMA_GUARD_WORK_MAGIC)) {
            cancel_delayed_work_sync(&dma_dev->guard_work.dma_guard_work);
        }
    }

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        if ((sriov_flag == DEVDRV_SRIOV_ENABLE) && (dma_dev->func_id == 0) && (i == 0)) {
            continue;
        }
        dma_chan = &dma_dev->dma_chan[i];

        /* chan has not init */
        if (dma_chan->dev == NULL) {
            continue;
        }

        if (dma_chan->err_irq_state == DEVDRV_IRQ_IS_INIT) {
            dma_chan->err_irq_state = DEVDRV_IRQ_IS_UNINIT;
            devdrv_dma_set_interrupt_mask(dma_chan);
            (void)cancel_work_sync(&dma_chan->err_work);
        }

        if (dma_chan->done_irq_state == DEVDRV_IRQ_IS_INIT) {
            dma_chan->done_irq_state = DEVDRV_IRQ_IS_UNINIT;
            devdrv_dma_set_interrupt_mask(dma_chan);
            if (devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == true) {
                destroy_workqueue(dma_chan->dma_done_workqueue);
            }
            tasklet_kill(&dma_chan->dma_done_task);
        }

        if (devdrv_is_mdev_pm_boot_mode(dma_dev->dev_id) == false) {
            (void)devdrv_dma_chan_reset(dma_chan, sriov_flag);
        }

        devdrv_free_dma_sq_cq(dma_chan);
    }
}

STATIC void devdrv_dma_interrupt_resume_chan(struct devdrv_dma_dev *dma_dev, u32 entry_id)
{
    struct devdrv_dma_channel *dma_chan = &dma_dev->dma_chan[entry_id];
    u32 dma_chan_id = dma_chan->chan_id;
    int ret = 0;

    if (devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == true) {
        dma_chan->dma_done_workqueue = create_singlethread_workqueue("dma-done-work");
        if (dma_chan->dma_done_workqueue == NULL) {
            devdrv_err("Create dma done work fail\n");
            return;
        }
    }

    tasklet_init(&dma_chan->dma_done_task, devdrv_dma_done_task, (uintptr_t)dma_chan);
    devdrv_dma_set_interrupt_unmask(dma_chan);
    dma_chan->done_irq_state = DEVDRV_IRQ_IS_INIT;

    if (dma_chan->err_irq_flag != 0) {
        /* err interrupt we do some dfx words, so use wordqueue which can sleep */
        INIT_WORK(&dma_chan->err_work, devdrv_dma_err_task);
        dma_chan->err_irq_state = DEVDRV_IRQ_IS_INIT;
    } else {
        dma_chan->err_irq = -1;
    }

    if (dma_dev->sq_cq_side == DEVDRV_DMA_REMOTE_SIDE) {
        ret = devdrv_notify_dma_err_irq(dma_dev->drvdata, dma_chan_id, dma_chan->err_irq);
        if (ret != 0) {
            devdrv_err("Notify err_irq failed. (dev_id=%u; entry_id=%u; dma_chan_id=%u; ret=%d)\n",
                       dma_dev->dev_id, entry_id, dma_chan_id, ret);
        }
    }
    return;
}

STATIC int devdrv_dma_resume_chan(struct devdrv_dma_dev *dma_dev, u32 entry_id, u32 dma_chan_id, u32 sriov_flag)
{
    struct devdrv_dma_channel *dma_chan = &dma_dev->dma_chan[entry_id];
    int ret;

    devdrv_dma_chan_info_init(dma_dev, entry_id, dma_chan_id);

    /* reset DMA channel before init */
    ret = devdrv_dma_chan_reset(dma_chan, sriov_flag);
    if (ret != 0) {
        devdrv_err("dma_ch_cfg_reset failed. (dev_id=%u; chan=%u; ret=%d)\n", dma_dev->dev_id, entry_id, ret);
        return ret;
    }

    ret = devdrv_alloc_dma_sq_cq(dma_chan);
    if (ret != 0) {
        devdrv_err("Alloc dma sq cq failed. (dev_id=%u; chan=%u)\n", dma_dev->dev_id, entry_id);
        return ret;
    }

    ret = devdrv_dma_chan_init(dma_chan, (u32)devdrv_get_env_boot_type(dma_dev->dev_id));
    if (ret != 0) {
        devdrv_err("Remote init failed. (dev_id=%u; chan=%u; ret=%d)\n", dma_dev->dev_id, entry_id, ret);
        devdrv_free_dma_sq_cq(dma_chan);
        return ret;
    }

    devdrv_dma_interrupt_resume_chan(dma_dev, entry_id);

    return ret;
}

STATIC int devdrv_dma_resume(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_dma_dev *dma_dev = pci_ctrl->dma_dev;
    u32 chan_id;
    u32 i;

    if (dma_dev == NULL) {
        devdrv_err("Dma dev released. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -ENOMEM;
    }

    if (devdrv_check_dl_dlcmsm_state(pci_ctrl) != 0) {
        devdrv_err("Check dlcmsm state failed.\n");
        return -EINVAL;
    }

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        chan_id = pci_ctrl->res.dma_res.use_chan[i];
        devdrv_debug("Get chan_id. (dev_id=%d; dma_chan=%d)\n",
                     dma_dev->dev_id, chan_id);
        if (devdrv_dma_resume_chan(dma_dev, i, (chan_id + dma_dev->remote_chan_begin), DEVDRV_SRIOV_DISABLE) != 0) {
            devdrv_err("Dma chan init failed. (dev_id=%u; chan_id=%u)\n", pci_ctrl->dev_id, i);
            devdrv_dma_suspend(dma_dev, DEVDRV_SRIOV_DISABLE);
            return -EINVAL;
        }
    }

    devdrv_set_dma_status(dma_dev, DEVDRV_DMA_ALIVE);
    /* DMA guard work */
    if (((devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_PHY_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_SRIOV_VF_BOOT)) &&
        (dma_dev->guard_work.work_magic == DEVDRV_DMA_GUARD_WORK_MAGIC)) {
        schedule_delayed_work(&dma_dev->guard_work.dma_guard_work, 0);
    }

    tasklet_init(&dma_dev->single_fault_task, devdrv_dma_stop_business, (uintptr_t)dma_dev);
    return 0;
}

STATIC void devdrv_msg_suspend(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;
    struct devdrv_msg_chan *msg_chan = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct devdrv_msg_slave_mem_node *node = NULL;
    u32 i;

    if (msg_dev == NULL) {
        devdrv_err("msg_dev has been free. (dev_id=%u)\n", pci_ctrl->dev_id);
        return;
    }

    for (i = 0; i < msg_dev->chan_cnt; i++) {
        msg_chan = &msg_dev->msg_chan[i];
        if (msg_chan->status == DEVDRV_ENABLE) {
            devdrv_free_msg_queue_res(msg_chan);
        }
    }

    mutex_lock(&msg_dev->mutex);
    if (list_empty_careful(&msg_dev->slave_mem_list) == 0) {
        list_for_each_safe(pos, n, &msg_dev->slave_mem_list)
        {
            node = list_entry(pos, struct devdrv_msg_slave_mem_node, list);
            list_del(&node->list);
            devdrv_kfree(node);
            node = NULL;
        }
    }
    mutex_unlock(&msg_dev->mutex);
    devdrv_info("Call devdrv_msg_suspend success. (dev_id=%u)\n", pci_ctrl->dev_id);
}

STATIC int devdrv_msg_resume(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;
    int ret;

    devdrv_info("Message resume start. (msix_irq_num=%d\n", pci_ctrl->msix_irq_num);

    if (msg_dev == NULL) {
        devdrv_err("msg_dev is null.\n");
        return -EINVAL;
    }

    INIT_LIST_HEAD(&msg_dev->slave_mem_list);
    msg_dev->slave_mem.offset = DEVDRV_MSG_QUEUE_MEM_BASE;
    msg_dev->slave_mem.len = (u32)(pci_ctrl->res.msg_mem.size - msg_dev->slave_mem.offset);

    ret = devdrv_init_admin_msg_chan(msg_dev);
    if (ret != 0) {
        devdrv_err("Admin queue init failed. (dev_id=%u; ret=%d)\n", pci_ctrl->dev_id, ret);
    }

    return ret;
}

STATIC void devdrv_load_probe_suspend(struct devdrv_pci_ctrl *pci_ctrl)
{
    devdrv_set_devctrl_startup_flag(pci_ctrl->dev_id, DEVDRV_DEV_STARTUP_UNPROBED);

    if (pci_ctrl->ops.unbind_irq != NULL) {
        pci_ctrl->ops.unbind_irq(pci_ctrl);
    }
    return;
}

STATIC int devdrv_suspend(struct pci_dev *pdev)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = (struct devdrv_pdev_ctrl *)pci_get_drvdata(pdev);
    struct devdrv_pci_ctrl *pci_ctrl;
    int ret = 0, i;

    for (i = pdev_ctrl->dev_num - 1; i >= 0; i--) {
        pci_ctrl = pdev_ctrl->pci_ctrl[i];
        if (pci_ctrl == NULL) {
            devdrv_err("pci_ctrl is null.\n");
            return -EINVAL;
        }
        if (pci_ctrl->load_status_flag != DEVDRV_LOAD_HALF_PROBE_STATUS) {
            devdrv_err("Load half probe uninit. (dev_id=%u)\n", pci_ctrl->dev_id);
            return -EINVAL;
        }

        if (pci_ctrl->device_status != DEVDRV_DEVICE_ALIVE) {
            devdrv_err("Device is not ready. (dev_id=%u, status=%u)\n", pci_ctrl->dev_id, pci_ctrl->device_status);
            return -EBUSY;
        }
        devdrv_set_device_status(pci_ctrl, DEVDRV_DEVICE_SUSPEND);
        if (pci_ctrl->load_vector >= 0) {
            (void)devdrv_unregister_irq_func((void *)pci_ctrl, pci_ctrl->load_vector, pci_ctrl);
            pci_ctrl->shr_para->host_interrupt_flag = 0;
            pci_ctrl->load_vector = -1;
        }
        msleep(WAIT_SUSPEND_SLEEP);
        devdrv_set_startup_status(pci_ctrl, DEVDRV_STARTUP_STATUS_INIT);
        devdrv_set_device_boot_status(pci_ctrl, DSMI_BOOT_STATUS_UNINIT);

        devdrv_dev_offline(pci_ctrl);

        if (pci_ctrl->ops.devdrv_deal_suspend_handshake != NULL) {
            ret = pci_ctrl->ops.devdrv_deal_suspend_handshake(pci_ctrl);
            if (ret != 0) {
                devdrv_err("Deal suspend handshake failed. (ret=%d)\n", ret);
                return ret;
            }
        } else {
            devdrv_err("suspend handshake callback uninit. (dev_id=%u)\n", pci_ctrl->dev_id);
            return -EINVAL;
        }
        if (pci_ctrl->ops.single_fault_uninit != NULL) {
            (void)pci_ctrl->ops.single_fault_uninit(pci_ctrl);
        }
        devdrv_set_host_phy_mach_flag(pci_ctrl->dev_id, 0x0);
        devdrv_guard_work_uninit(pci_ctrl);
        devdrv_free_common_msg_queue(pci_ctrl);
        devdrv_dma_suspend(pci_ctrl->dma_dev, DEVDRV_SRIOV_DISABLE);
        devdrv_msg_suspend(pci_ctrl);
        devdrv_load_probe_suspend(pci_ctrl);
        pci_ctrl->load_status_flag = DEVDRV_LOAD_INIT_STATUS;
    }
    devdrv_uncfg_pdev(pdev);
    return ret;
}

__attribute__((unused)) int drv_pcie_resume_notify(struct device *dev)
{
    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        return 0;
    }

    g_resume_notify = 1;
    wake_up_interruptible(&resume_notify_wq);
    return 0;
}

__attribute__((unused)) int drv_pcie_suspend(struct device *dev)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;
    struct pci_dev *pdev = NULL;
    int ret;

    pdev = container_of(dev, struct pci_dev, dev);
    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        return 0;
    }

    if (devdrv_get_pcie_channel_status() == DEVDRV_PCIE_COMMON_CHANNEL_LINKDOWN) {
        devdrv_err("channel status linkdown err.\n");
        return -EINVAL;
    }
    devdrv_set_pcie_channel_status(DEVDRV_PCIE_COMMON_CHANNEL_SUSPEND);
    devdrv_info("Suspend pci driver start.\n");
    pdev_ctrl = (struct devdrv_pdev_ctrl *)pci_get_drvdata(pdev);
    if (pdev_ctrl == NULL) {
        devdrv_err("get devdrv_pdev_ctrl failed.\n");
        return -EINVAL;
    }

    ret = pci_save_state(pdev);
    if (ret != 0) {
        devdrv_err("pci_save_state failed. (ret=%d)\n", ret);
        return ret;
    }

    pci_saved_state = pci_store_saved_state(pdev);
    if (pci_saved_state == NULL) {
        devdrv_err("pci_store_saved_state failed.\n");
        return -ENOMEM;
    }

    if (devdrv_is_sentry_work_mode()) {
        ret = devdrv_sentry_save_rp_config_space();
        if (ret != 0) {
            devdrv_err("save rp config space failed. (ret=%d)\n", ret);
            goto free_pci_state;
        }
    }

    ret = devdrv_suspend(pdev);
    if (ret != 0) {
        devdrv_err("Suspend pci driver failed. (ret=%d)\n", ret);
        goto free_pci_state;
    }
    devdrv_info("Suspend pci driver success.\n");
    return 0;
free_pci_state:
    pci_load_and_free_saved_state(pdev, &pci_saved_state);
    return ret;
}

void devdrv_load_half_resume(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    devdrv_info("devdrv_load_half_resume start. (dev_id=%d)\n", pci_ctrl->dev_id);

    (void)devdrv_msg_resume(pci_ctrl);

    ret = devdrv_dma_resume(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Dma device resume fail. (dev_id=%u)\n", pci_ctrl->dev_id);
        goto msg_suspend;
    }

    devdrv_register_half_devctrl(pci_ctrl);

    /* alloc common msg queue */
    ret = devdrv_alloc_common_msg_queue(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Alloc common_queue failed. (dev_id=%d; ret=%d)\n", pci_ctrl->dev_id, ret);
        goto dma_suspend;
    }

    devdrv_guard_work_init(pci_ctrl);
    devdrv_set_host_phy_mach_flag(pci_ctrl->dev_id, DEVDRV_HOST_PHY_MACH_FLAG);

    ret = devdrv_dev_online(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Online init failed. (dev_id=%d; ret=%d)\n", pci_ctrl->dev_id, ret);
        goto free_common_msg_queue;
    }
    if (pci_ctrl->ops.single_fault_init != NULL) {
        (void)pci_ctrl->ops.single_fault_init(pci_ctrl);
    }
    devdrv_set_pcie_channel_status(DEVDRV_PCIE_COMMON_CHANNEL_OK);

    ret = uda_dev_ctrl(pci_ctrl->dev_id, UDA_CTRL_RESUME);
    if (ret != 0) {
        pci_ctrl->load_status_flag = DEVDRV_LOAD_INIT_STATUS;
        devdrv_err("uda dev resume failed. (dev_id=%d; ret=%d)\n", pci_ctrl->dev_id, ret);
        return;
    }

    if (devdrv_is_sentry_work_mode()) {
        devdrv_sentry_wake_up_exit_event();
    }
    pci_ctrl->load_half_probe = 1;

    devdrv_set_startup_status(pci_ctrl, DEVDRV_STARTUP_STATUS_FINISH);
    pci_ctrl->load_status_flag = DEVDRV_LOAD_HALF_PROBE_STATUS;

    devdrv_info("Half resume finish, resume success. (dev_id=%d; bdf=%02x:%02x.%d)\n", pci_ctrl->dev_id,
        pci_ctrl->pdev->bus->number, PCI_SLOT(pci_ctrl->pdev->devfn), PCI_FUNC(pci_ctrl->pdev->devfn));

    return;

free_common_msg_queue:
    devdrv_guard_work_uninit(pci_ctrl);
    devdrv_free_common_msg_queue(pci_ctrl);

dma_suspend:
    devdrv_dma_suspend(pci_ctrl->dma_dev, DEVDRV_SRIOV_DISABLE);

msg_suspend:
    devdrv_msg_suspend(pci_ctrl);
    pci_ctrl->load_status_flag = DEVDRV_LOAD_INIT_STATUS;

    return;
}

STATIC void devdrv_resume_interrupt(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para->msix_offset = 0;
}

STATIC int devdrv_resume(struct pci_dev *pdev, const struct pci_device_id *data, int dev_index)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = (struct devdrv_pdev_ctrl *)pci_get_drvdata(pdev);
    struct devdrv_pci_ctrl *pci_ctrl = pdev_ctrl->pci_ctrl[dev_index];

    atomic_set(&pci_ctrl->ref_cnt, 0);

    devdrv_set_startup_status(pci_ctrl, DEVDRV_STARTUP_STATUS_INIT);

    devdrv_pcictrl_shr_para_init(pci_ctrl);

    if (pci_ctrl->ops.bind_irq != NULL) {
        pci_ctrl->ops.bind_irq(pci_ctrl);
    }

    devdrv_resume_interrupt(pci_ctrl);
    pci_ctrl->ops.set_dev_shr_info(pci_ctrl);
    pci_ctrl->load_status_flag = DEVDRV_LOAD_INIT_STATUS;
    pci_ctrl->load_vector = pci_ctrl->res.intr.device_os_load_irq;
    devdrv_set_device_status(pci_ctrl, DEVDRV_DEVICE_RESUME);
    (void)devdrv_register_irq_func((void *)pci_ctrl, pci_ctrl->load_vector, devdrv_half_probe_irq, pci_ctrl,
        "devdrv_half_probe_irq");
    devdrv_init_shr_info_after_half_probe(pci_ctrl);
    devdrv_set_devctrl_startup_flag(pci_ctrl->dev_id, DEVDRV_DEV_STARTUP_TOP_HALF_OK);

    pci_ctrl->module_exit_flag = DEVDRV_REMOVE_CALLED_BY_PRERESET;

    devdrv_info("Resume driver ends, waiting for receive msix intr. (devid=%u)\n",
        pci_ctrl->dev_id);

    /* other process in low-half part (devdrv_load_half_resume) after receive msix intr */
    return 0;
}

STATIC int drv_pcie_resume(struct pci_dev *pdev, const struct pci_device_id *data)
{
    u8 bus_num = pdev->bus->number;
    u8 device_num = PCI_SLOT(pdev->devfn);
    u8 func_num = PCI_FUNC(pdev->devfn);
    int i = 0, ret = 0;
    int dev_num = devdrv_get_pdev_davinci_dev_num(data->device, pdev->subsystem_device);

    devdrv_info("Resume driver IN. (dev_num=%d; bdf=%02x:%02x.%d)\n",
        dev_num, bus_num, device_num, func_num);

    ret = devdrv_cfg_pdev(pdev);
    if (ret != 0) {
        devdrv_err("Call devdrv_cfg_pdev failed. (ret=%d)\n", ret);
        return ret;
    }

    for (i = 0; i < dev_num; i++) {
        (void)devdrv_resume(pdev, data, i);
    }

    return ret;
}

STATIC struct pci_dev *devdrv_pci_get_device(void)
{
    u32 device_arr[] = {MINI_V3_DEVICE, MDC_V2_DEVICE};
    int size = sizeof(device_arr) / sizeof(u32);
    struct pci_dev *pdev = NULL;
    int i;

    for (i = 0; i < size; ++i) {
        pdev = pci_get_device(PCI_VENDOR_ID_HX, device_arr[i], pdev);
#ifdef CFG_SOC_PLATFORM_MDC_V11
        if (pdev == NULL) {
            pdev = pci_get_device(PCI_VENDOR_ID_HUAWEI, device_arr[i], pdev);
        }
#endif
        if (pdev != NULL) {
            return pdev;
        }
    }

    return NULL;
}

void devdrv_resume_device(void)
{
    const struct pci_device_id *data = NULL;
    struct pci_dev *pdev = NULL;
    int ret = 0;

    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        return;
    }

    pdev = devdrv_pci_get_device();
    if (pdev == NULL) {
        devdrv_err("Call pci_get_device failed.\n");
        return;
    }

    devdrv_info("Resume pci driver start. (vendor=0x%x; device=0x%x)\n", pdev->vendor, pdev->device);

    if (devdrv_is_sentry_work_mode()) {
        ret = devdrv_sentry_restore_rp_config_space();
        if (ret != 0) {
            pci_dev_put(pdev);
            devdrv_err("sentry restore rp config failed. (ret=%d)\n", ret);
            return;
        }
        ret = devdrv_sentry_runtime_resume_device(pdev);
        if (ret != 0) {
            pci_dev_put(pdev);
            devdrv_err("sentry runtime resume failed. (ret=%d)\n", ret);
            return;
        }
    } else {
        ret = wait_event_interruptible_timeout(resume_notify_wq, (g_resume_notify == 1),
                                            msecs_to_jiffies(WAIT_RESUME_NOTIFY_TIMEOUT));
        if (ret <= 0) {
            devdrv_warn("wait resume notify event timeout or bad wakeup. (ret=%d)\n", ret);
        }
        g_resume_notify = 0;
    }

    ret = pci_load_and_free_saved_state(pdev, &pci_saved_state);
    if (ret != 0) {
        pci_dev_put(pdev);
        devdrv_err("Load and free saved state failed.\n");
        return;
    }
    pci_restore_state(pdev);

    data = pci_match_id(g_devdrv_driver_ver.id_table, pdev);
    if (data == NULL) {
        pci_dev_put(pdev);
        devdrv_err("Call pci_match_id failed.\n");
        return;
    }

    ret = drv_pcie_resume(pdev, data);
    if (ret != 0) {
        pci_dev_put(pdev);
        devdrv_err("Resume pci driver failed. (vendor=0x%x; device=0x%x; ret=%d)\n",
            data->vendor, data->device, ret);
        return;
    }
    pci_dev_put(pdev);
    devdrv_info("Resume pci driver success. (vendor=0x%x; device=0x%x)\n", data->vendor, data->device);
    return;
}
EXPORT_SYMBOL(devdrv_resume_device);

STATIC int devdrv_get_suspend_retry_times(void)
{
    u32 suspend_time_config = 0;
    static int suspend_count = -1U;
    struct device_node *np;
    int ret = 0;

    if (suspend_count != -1) {
        return suspend_count;
    }

    suspend_count = DEVDRV_GET_EP_SUSPEND_TIMEOUT;
    np = of_find_compatible_node(NULL, NULL, "ascend-pcie-rc");
    if (np != NULL) {
        ret = of_property_read_u32(np, "suspend_time", &suspend_time_config);
        if (ret != 0) {
            devdrv_info("can not read Ep suspend_time by dts.\n");
        } else {
            devdrv_info("read Ep suspend_time success. (time=%d(s)).\n", suspend_time_config);
            suspend_time_config = suspend_time_config * DEVDRV_SECOND_TO_USECOND;
            suspend_count = (int)(suspend_time_config / DEVDRV_GET_EP_SUSPEND_DELAY);
        }
        of_node_put(np);
    }
    return suspend_count;
}

STATIC int devdrv_get_ep_suspend_status(struct devdrv_msg_dev *msg_dev)
{
    struct devdrv_ep_suspend_status reply = {0};
    u32 dev_id = msg_dev->pci_ctrl->dev_id;
    int count = 0;
    int ret;
    int suspend_count = 0;
    int hdc_precheck_ret = 0;
    struct devdrv_ep_suspend_cmd cmd_data;

    suspend_count = devdrv_get_suspend_retry_times();
    cmd_data.hand_shake_flag = DEVDRV_HANDSHAKE_GET_EP_SUSPEND;
    /* send the cmd */
    do {
        hdc_precheck_ret = devdrv_hdc_suspend_precheck(count);
        if (hdc_precheck_ret == 0) {
            ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_GET_EP_SUSPEND_STATUS,
                &cmd_data, sizeof(cmd_data), &reply, sizeof(reply));
            if (ret != 0) {
                devdrv_err("Send admin msg fail for get ep suspend status. (dev_id=%u)\n", dev_id);
                return ret;
            }
            if (reply.status == DEVDRV_EP_SUSPEND_READY) {
                devdrv_info("EP suspend status is ready. (dev_id=%d, count=%d)\n", dev_id, count);
                break;
            }
        }

        usleep_range(DEVDRV_GET_EP_SUSPEND_DELAY, DEVDRV_GET_EP_SUSPEND_DELAY + DEVDRV_GET_EP_SUSPEND_RANGE);
        count++;

        if ((count % DEVDRV_SUSPEND_ONE_SECOND) == 0) {
            devdrv_info("wait ep suspend status to be ready. (dev_id=%d, wait_time=%d(s), hdc_check=%d)\n",
                dev_id, count / DEVDRV_SUSPEND_ONE_SECOND, hdc_precheck_ret);
        }
        if (count > suspend_count) {
            devdrv_err("Get ep suspend status timeout. (dev_id=%u, status=%u, hdc_check=%d)\n",
                dev_id, reply.status, hdc_precheck_ret);
            return -ETIMEDOUT;
        }
    } while (true);

    devdrv_info("Get ep suspend status success. (dev_id=%d, status=%u)\n", dev_id, reply.status);
    return 0;
}

int devdrv_dev_get_suspend_status(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret = 0;
    struct mutex *devdrv_ctrl_mutex = devdrv_get_ctrl_mutex();

    mutex_lock(devdrv_ctrl_mutex);
    devdrv_info("Get ep suspend status begin. (dev_id=%d)\n", pci_ctrl->dev_id);
    ret = devdrv_get_ep_suspend_status(pci_ctrl->msg_dev);
    if (ret != 0) {
        devdrv_err("Get ep suspend status failed. (dev_id=%u, ret=%d)\n", pci_ctrl->dev_id, ret);
    }
    mutex_unlock(devdrv_ctrl_mutex);

    return ret;
}

bool devdrv_get_fault_proc_status(u32 flag)
{
#define PCIE_FAULT_REPORT_FLAG  0U
#define PCIE_FAULT_DUMP_FLAG    1U
    u32 pcie_channel_status;
    u32 peer_status;

    pcie_channel_status = devdrv_get_pcie_channel_status();
    peer_status = devdrv_get_peer_pcie_status();
    if (flag == PCIE_FAULT_REPORT_FLAG) {
        return (pcie_channel_status != (u32)DEVDRV_PCIE_COMMON_CHANNEL_INIT) &&
               (pcie_channel_status != (u32)DEVDRV_PCIE_COMMON_CHANNEL_RESUME) &&
               ((pcie_channel_status != (u32)DEVDRV_PCIE_COMMON_CHANNEL_DEAD) ||
                (peer_status != (u32)DEVDRV_PEER_STATUS_NORMAL)) &&
               (pcie_channel_status != (u32)DEVDRV_PCIE_COMMON_CHANNEL_SUSPEND);
    } else if (flag == PCIE_FAULT_DUMP_FLAG) {
        return (pcie_channel_status != (u32)DEVDRV_PCIE_COMMON_CHANNEL_INIT);
    }

    return false;
}
EXPORT_SYMBOL(devdrv_get_fault_proc_status);

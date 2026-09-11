/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-14
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>

#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "securec.h"
#include "uda.h"
#include "dbl_mem_alloc_interface.h"
#include "davinci_api.h"
#include "davinci_interface.h"
#include "udis_management.h"
#include "udis_timer.h"
#include "udis_msg.h"
#include "udis_log.h"

#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#define UDIS_HOST_NOTIFIER "udis_host"
#define UDIS_HB_MONITOR "udis_hb_mon"
#define UDIS_HB_MONITOR_PERIOD_MS 1000

#define PCI_VENDOR_ID_HUAWEI            0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
static const struct pci_device_id g_udis_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd100),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd105),           0 },
    { KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd801),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd501),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd802),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd803),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd804),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd805),           0 },
    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}
};
KA_MODULE_DEVICE_TABLE(pci, g_udis_tbl);

STATIC int udis_common_chan_register(unsigned int udevid)
{
    int ret;
    struct devdrv_common_msg_client *client = NULL;

    client = udis_get_common_msg_client();
    if (client == NULL) {
        udis_err("Get common msg client failed. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    ret = devdrv_register_common_msg_client(client);
    if (ret != 0) {
        udis_err("Udis register common msg channel failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }
    return 0;
}

STATIC void udis_common_chan_unregister(unsigned int udevid)
{
    struct devdrv_common_msg_client *client = NULL;
    client = udis_get_common_msg_client();
    if (client == NULL) {
        udis_err("Get common msg client failed. (udevid=%u)\n", udevid);
        return;
    }
    devdrv_unregister_common_msg_client(udevid, client);
}

STATIC int udis_cb_constructor(unsigned int udevid, struct udis_ctrl_block *udis_cb)
{
    int ret, i;

    udis_cb->udis_info_buf = (struct udis_info_stu *)devdrv_dma_alloc_coherent(uda_get_device(udevid),
        UDIS_MODULE_OFFSET * UDIS_MODULE_MAX, &udis_cb->udis_info_buf_dma, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (udis_cb->udis_info_buf == NULL) {
        udis_err("Failed to call devdrv_dma_alloc_coherent. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = memset_s(udis_cb->udis_info_buf, UDIS_MODULE_OFFSET * UDIS_MODULE_MAX, 0,
        UDIS_MODULE_OFFSET * UDIS_MODULE_MAX);
    if (ret != 0) {
        udis_err("Call memset_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        ret = -EIO;
        goto free_dma_buf;
    }

    for (i = UPDATE_ONLY_ONCE; i < UPDATE_TYPE_MAX; ++i) {
        INIT_LIST_HEAD(&udis_cb->addr_list[i]);
    }

    ka_task_init_rwsem(&udis_cb->udis_info_lock);
    ka_task_init_rwsem(&udis_cb->addr_list_lock);
    return 0;

free_dma_buf:
    devdrv_dma_free_coherent(uda_get_device(udevid), UDIS_MODULE_OFFSET * UDIS_MODULE_MAX,
        udis_cb->udis_info_buf, udis_cb->udis_info_buf_dma);
    udis_cb->udis_info_buf = NULL;
    udis_cb->udis_info_buf_dma = 0;
    return ret;
}

STATIC void udis_cb_destructor(unsigned int udevid, struct udis_ctrl_block *udis_cb)
{
    int i;
    struct udis_dma_node *addr_node, *tmp;

    for (i = UPDATE_ONLY_ONCE; i < UPDATE_TYPE_MAX; ++i) {
        list_for_each_entry_safe(addr_node, tmp, &udis_cb->addr_list[i], list) {
            list_del(&addr_node->list);
            dbl_kfree(addr_node);
            addr_node = NULL;
        }
    }

    devdrv_dma_free_coherent(uda_get_device(udevid), UDIS_MODULE_OFFSET * UDIS_MODULE_MAX,
        udis_cb->udis_info_buf, udis_cb->udis_info_buf_dma);
    udis_cb->udis_info_buf = NULL;
    udis_cb->udis_info_buf_dma = 0;
}

STATIC int udis_init_ucb(unsigned int udevid)
{
    int ret;
    struct udis_ctrl_block *ucb = NULL;

    ucb = (struct udis_ctrl_block*)dbl_kzalloc(sizeof(struct udis_ctrl_block), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ucb == NULL) {
        udis_err("Failed to alloc for udis ctrl block. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = udis_cb_constructor(udevid, ucb);
    if (ret != 0) {
        dbl_kfree(ucb);
        ucb = NULL;
        udis_err("Failed to init udis ctrl block. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    udis_cb_write_lock(udevid);
    if (udis_get_ctrl_block(udevid) != NULL) {
        udis_cb_write_unlock(udevid);
        udis_cb_destructor(udevid, ucb);
        dbl_kfree(ucb);
        ucb = NULL;
        udis_info("Udis ctrl block has been init. (udevid=%u)\n", udevid);
        return 0;
    }
    ucb->state = UDIS_DEV_READY;
    (void)udis_set_ctrl_block(udevid, ucb);
    udis_cb_write_unlock(udevid);

    return 0;
}

STATIC void udis_uninit_ucb(unsigned int udevid)
{
    struct udis_ctrl_block *ucb = NULL;

    udis_cb_write_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_cb_write_unlock(udevid);
        udis_info("Udis ctrl block has been uninit. (udevid=%u)\n", udevid);
        return;
    }
    ucb->state = UDIS_DEV_UNINIT;
    (void)udis_set_ctrl_block(udevid, NULL);
    udis_cb_write_unlock(udevid);

    udis_cb_destructor(udevid, ucb);
    dbl_kfree(ucb);
    ucb = NULL;

    return;
}

enum {
    UDIS_LINK_DMA_NODES_NOTIFY_FUNC = 0,
    UDIS_CTRL_BLOCK_NOTIFY_FUNC,
    UDIS_COMMON_CHANNEL_NOTIFY_FUNC,
    UDIS_PERIOD_LINK_DMA_TASK_NOTIFY_FUNC,
    UDIS_NOTIFY_FUNC_TALBE_SIZE
};

STATIC int (*udis_device_up_notify_func_table[UDIS_NOTIFY_FUNC_TALBE_SIZE])(unsigned int udevid) = {
    [UDIS_LINK_DMA_NODES_NOTIFY_FUNC] = udis_link_dma_nodes_init,
    [UDIS_CTRL_BLOCK_NOTIFY_FUNC] = udis_init_ucb,
    [UDIS_COMMON_CHANNEL_NOTIFY_FUNC] = udis_common_chan_register,
    [UDIS_PERIOD_LINK_DMA_TASK_NOTIFY_FUNC] = period_link_dma_task_init,
};

STATIC void (*udis_device_down_notify_func_table[UDIS_NOTIFY_FUNC_TALBE_SIZE])(unsigned int udevid)= {
    [UDIS_LINK_DMA_NODES_NOTIFY_FUNC] = udis_link_dma_nodes_uninit,
    [UDIS_CTRL_BLOCK_NOTIFY_FUNC] = udis_uninit_ucb,
    [UDIS_COMMON_CHANNEL_NOTIFY_FUNC] = udis_common_chan_unregister,
    [UDIS_PERIOD_LINK_DMA_TASK_NOTIFY_FUNC] = period_link_dma_task_uninit,
};

STATIC int udis_device_up_notify(unsigned int udevid)
{
    int ret;
    int i, j;

    for (i = UDIS_LINK_DMA_NODES_NOTIFY_FUNC; i < UDIS_NOTIFY_FUNC_TALBE_SIZE; ++i) {
        ret = udis_device_up_notify_func_table[i](udevid);
        if (ret != 0) {
            udis_err("Call udis device up notify func failed. (udevid=%u; ret=%d; func_index=%d)\n", udevid, ret, i);
            goto out;
        }
    }

    ret = udis_send_host_ready_msg_to_device(udevid);
    if (ret != 0) {
        udis_err("Udis send host ready msg to device failed. (udevid=%u; ret=%d)\n", udevid, ret);
        goto out;
    }

    return 0;

out:
    for (j = i - 1; j >= 0; --j) {
        udis_device_down_notify_func_table[j](udevid);
    }
    return ret;
}

STATIC void udis_dev_down_notify(unsigned int udevid, enum udis_dev_state state)
{
    int i, func_idx;
    struct udis_ctrl_block *ucb = NULL;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (state >= UDIS_DEV_STATE_MAX)) {
        udis_err("Invalid udevid or state. (udevid=%u; state=%u)\n", udevid, state);
        return;
    }

    udis_cb_write_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_cb_write_unlock(udevid);
        return;
    }

    /*
    VM hot reset: hot reset、uninit
    VM reboot/clear：uninit
    */
    if (!uda_is_pf_dev(udevid) && ucb->state == UDIS_DEV_READY) {
        (void)udis_send_host_vf_uninit_notify(udevid);
    }

    ucb->state = state;
    udis_cb_write_unlock(udevid);

    func_idx = (state == UDIS_DEV_UNINIT ? UDIS_LINK_DMA_NODES_NOTIFY_FUNC : UDIS_PERIOD_LINK_DMA_TASK_NOTIFY_FUNC);

    for (i = UDIS_PERIOD_LINK_DMA_TASK_NOTIFY_FUNC; i >= func_idx; --i) {
        udis_device_down_notify_func_table[i](udevid);
    }

    return;
}

STATIC void udis_hb_broken_notify(unsigned int udevid)
{
    return udis_dev_down_notify(udevid, UDIS_DEV_HEART_BEAT_LOSS);
}

STATIC int udis_notify_device_hotreset_cancel(unsigned int udevid)
{
    int ret = 0;
    struct udis_ctrl_block *ucb = NULL;

    udis_cb_write_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_cb_write_unlock(udevid);
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return -ENODEV;
    }
    ucb->state = UDIS_DEV_READY;
    udis_cb_write_unlock(udevid);

    ret = period_link_dma_task_init(udevid);
    if (ret != 0) {
        udis_err("Init period link dma task failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    return 0;
}

STATIC int udis_host_notifier_func(unsigned int udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = udis_device_up_notify(udevid);
    } else if (action == UDA_UNINIT) {
        udis_dev_down_notify(udevid, UDIS_DEV_UNINIT);
    } else if (action == UDA_HOTRESET) {
        udis_dev_down_notify(udevid, UDIS_DEV_HOTRESET);
    } else if (action == UDA_PRE_HOTRESET) {
        udis_dev_down_notify(udevid, UDIS_DEV_PREHOTRESET);
    } else if ((action == UDA_HOTRESET_CANCEL) || (action == UDA_PRE_HOTRESET_CANCEL)) {
        ret = udis_notify_device_hotreset_cancel(udevid);
    }

    udis_info("udis notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

STATIC int udis_heart_broken_moniter(unsigned int udevid, unsigned long privilege_data)
{
    unsigned int i;
    unsigned int status;
    int ret;
    struct ascend_intf_get_status_para hb_mon_para = {0};
    static unsigned int g_last_dev_status[UDIS_DEVICE_UDEVID_MAX] = {0};

    hb_mon_para.type = DAVINCI_STATUS_TYPE_DEVICE;

    for (i = 0; i < UDIS_DEVICE_UDEVID_MAX; ++i) {
        hb_mon_para.para.device_id = i;
        status = 0;
        ret = ascend_intf_get_status(hb_mon_para, &status);
        if (ret != 0) {
            udis_err("Failed to get device status. (udevid=%u; ret=%d)\n", i, ret);
            return ret;
        }

        if (((status & DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST) != 0) &&
            ((g_last_dev_status[i] & DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST) == 0)) {
            udis_hb_broken_notify(i);
        }
        g_last_dev_status[i] = status;
    }
    return 0;
}

STATIC int udis_hb_monitor_init(void)
{
    int ret;
    struct udis_timer_task task = {0};

    task.period_ms = UDIS_HB_MONITOR_PERIOD_MS;
    task.cur_ms = 0;
    task.work_type = COMMON_WORK;
    task.privilege_data = 0;
    task.period_task_func = udis_heart_broken_moniter;

    ret = strcpy_s(task.task_name, TASK_NAME_MAX_LEN, UDIS_HB_MONITOR);
    if (ret != 0) {
        udis_err("Failed to copy task name. (ret=%d; name=%s)\n", ret, UDIS_HB_MONITOR);
        return -ENOMEM;
    }

    ret = hal_kernel_register_period_task(0, &task);
    if (ret != 0) {
        udis_err("Failed to register udis heart beat monitor task. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}

STATIC void udis_hb_monitor_uninit(void)
{
    int ret;
    char task_name[TASK_NAME_MAX_LEN] = {0};

    ret = strcpy_s(task_name, TASK_NAME_MAX_LEN, UDIS_HB_MONITOR);
    if (ret != 0) {
        udis_err("strcpy_s failed. (ret=%d; name=%s)\n", ret, UDIS_HB_MONITOR);
        return;
    }

    ret = hal_kernel_unregister_period_task(0, task_name);
    if (ret != 0) {
        udis_err("Failed to unregister heart broken monitor. (ret=%d)\n", ret);
        return;
    }
}

STATIC int __init udis_init(void)
{
    int ret = 0;
    int i;
    struct uda_dev_type type = {0};

    for (i = 0; i < UDIS_DEVICE_UDEVID_MAX; ++i) {
        udis_cb_rwlock_init(i);
    }

    ret = udis_common_chan_init();
    if (ret != 0) {
        udis_err("Udis common chan init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = udis_timer_init();
    if (ret != 0) {
        udis_err("Init udis timer failed. (ret=%d)\n", ret);
        goto udis_timer_init_fail;
    }

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(UDIS_HOST_NOTIFIER, &type, UDA_PRI4, udis_host_notifier_func);
    if (ret != 0) {
        udis_err("Rigister uda notifier failed. (ret=%d)\n", ret);
        goto uda_register_fail;
    }

    ret = udis_hb_monitor_init();
    if (ret != 0) {
        udis_err("udis bh monitor init failed. (ret=%d)\n", ret);
        goto hb_monitor_init_fail;
    }
    udis_cmd_init();
    udis_info("udis driver init success.\n");
    return 0;

hb_monitor_init_fail:
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(UDIS_HOST_NOTIFIER, &type);
uda_register_fail:
    udis_timer_uninit();
udis_timer_init_fail:
    udis_common_chan_uninit();
    return ret;
}

STATIC void __exit udis_exit(void)
{
    struct uda_dev_type type = {0};

    udis_cmd_exit();
    udis_hb_monitor_uninit();
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(UDIS_HOST_NOTIFIER, &type);
    udis_timer_uninit();
    udis_common_chan_uninit();

    udis_info("udis driver exit success.\n");
    return;
}

module_init(udis_init);
module_exit(udis_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");

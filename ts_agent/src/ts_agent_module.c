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

#include "ts_agent_log.h"
#include "ts_agent_interface.h"
#include "ts_agent_vsq_worker.h"
#include "ts_agent_vsq_proc.h"
#include "hvtsdrv_tsagent.h"
#ifdef CFG_SOC_PLATFORM_STARS
#include "securec.h"
#include "tsch/task_struct.h"
#include "ts_agent_update_sqe.h"
#include "trs_adapt.h"
#endif
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"

STATIC int __init ts_agent_init(void)
{
    int ret;
#ifdef CFG_SOC_PLATFORM_STARS
    struct trs_sqcq_agent_ops ops = {0};
    init_task_convert_func();
    ret = tsagent_stream_id_to_sq_id_init();
    if (ret != EOK) { return ret; }
    tsagent_dev_base_info_init();

    ts_agent_info("ts_agent_init begin.");
    // register methods to ts_drv
    #if defined(CFG_SOC_PLATFORM_CLOUD_V2)
    tsagent_sq_base_info_init();
    ops.device_init = tsagent_device_init;
    ops.device_uninit = tsagent_device_uninit;
    #endif
    ops.sqe_update = tsagent_sqe_update;
    #if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3_EP)
    ops.mb_update = tsagent_mailbox_update;
    ops.cqe_update = tsagent_cqe_update;
    #endif
    trs_sqcq_agent_ops_register(&ops);
#else
    struct hvtsdrv_tsagent_ops intf_info;
    ts_agent_info("ts_agent_init begin.");
    init_task_convert_func();
    ret = init_all_vf_work_ctx();
    if (ret != 0) {
        ts_agent_err("init all vf work ctx failed, ret=%d.", ret);
        return ret;
    }

    // register methods to ts_drv
    intf_info.tsagent_vf_create = ts_agent_vf_create;
    intf_info.tsagent_vf_destroy = ts_agent_vf_destroy;
    intf_info.tsagent_vsq_proc = ts_agent_vsq_proc;
    intf_info.tsagent_trans_mailbox_msg = ts_agent_trans_mailbox_msg;
    hvtsdrv_tsagent_register(&intf_info);
#endif
    ts_agent_info("ts_agent_init end.");
    return 0;
}

STATIC void __exit ts_agent_exit(void)
{
    ts_agent_info("ts_agent_exit begin.");
#ifdef CFG_SOC_PLATFORM_STARS
    trs_sqcq_agent_ops_unregister();
    tsagent_stream_id_to_sq_id_uninit();
#else
    // unregister to tsdrv
    hvtsdrv_tsagent_unregister();
    // clear ts agent work queue
    destroy_all_vf_work_ctx();
#endif
    ts_agent_info("ts_agent_exit end.");
}

#define PCI_VENDOR_ID_HUAWEI 0x19e5

static const ka_pci_device_id_t g_ts_agent_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    { 0x20C6, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    {}};
KA_MODULE_DEVICE_TABLE(pci, g_ts_agent_tbl);

ka_module_init(ts_agent_init);
ka_module_exit(ts_agent_exit);

MODULE_DESCRIPTION("Ascend task scheduler agent");
KA_MODULE_LICENSE("GPL v2");
KA_MODULE_INFO(supported, "ts agent");
KA_MODULE_VERSION("v0.1");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");

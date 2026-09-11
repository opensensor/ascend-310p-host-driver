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

#ifndef VIRTMNGAGENT_CTRL_H
#define VIRTMNGAGENT_CTRL_H

#include "virtmng_interface.h"
#include "virtmngagent_unit.h"

struct vmnga_dev_state_record {
    u32 dev_num;
    u32 startup_devids[VMNG_PDEV_MAX];
    vmnga_dev_startup_notify startup_notify;
    vmnga_dev_state_notify state_notify;
};

#define VMNGA_DEVFN_BIT 3
#define VMNGA_DEVFN_DEV_VAL 0x1F
#define VMNGA_DEVFN_FN_VAL 0x7

struct vmnga_unit *vmnga_get_top_half_unit_by_id(u32 dev_id);
struct vmnga_unit *vmnga_get_bottom_half_unit_by_id(u32 dev_id);

ka_pci_dev_t *vmnga_get_pdev_by_id(u32 dev_id);
int vmnga_register_ctrls(struct vmnga_unit *unit);
void vmnga_unregister_ctrls(const struct vmnga_unit *unit);
void vmnga_register_ctrls_half(struct vmnga_unit *unit);
void vmnga_unregister_ctrls_half(struct vmnga_unit *unit);
int vmnga_init_instance_after_probe(u32 dev_id);
void vmnga_uninit_instance_remove_pdev(u32 dev_id);
int vmnga_suspend_instance_all_client(u32 dev_id);
void vmnga_agent_stop(u32 dev_id);
int vmnga_ctrl_init(void);
void vmnga_set_ctrl_startup_flag(u32 dev_id, enum vmng_startup_flag_type flag);
int vmnga_get_ctrl_startup_flag(u32 dev_id);

void vmnga_record_dev_startup(u32 dev_id);
void vmnga_report_dev_startup(u32 dev_id);
void vmnga_dev_state_notifier(struct vmnga_unit *unit);

#endif

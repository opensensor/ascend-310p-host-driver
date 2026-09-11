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

#ifndef __DMS_DEV_NODE_H__
#define __DMS_DEV_NODE_H__

#include "dms_define.h"

struct dms_node *dms_get_devnode_cb(u32 dev_id, int node_type, int node_id);
struct dms_node *dms_get_devnode_cb_nolock(u32 dev_id, int node_type, int node_id);
int dms_register_dev_node(struct dms_node *node);
int dms_unregister_dev_node(struct dms_node *node);
int dms_devnode_get_state(u32 dev_id, int node_type, int node_id, u32 *state);
int dms_devnode_get_capacity(u32 dev_id, int node_type, int node_id, unsigned long long *cap);
int dms_traverse_devnode_get_capacity(u32 dev_id, struct dsmi_dtm_node_s node_info[],
    unsigned int size, unsigned int *out_size);
int dms_devnode_enable_device(u32 dev_id, int node_type, int node_id);
int dms_devnode_disable_device(u32 dev_id, int node_type, int node_id);
int dms_devnode_get_power_info(u32 dev_id, int node_type, int node_id, void *buf, unsigned int size);
int dms_devnode_set_power_info(u32 dev_id, int node_type, int node_id, void *buf, unsigned int size);
int dms_devnode_set_power_state(u32 dev_id, int node_type, int node_id, DSMI_POWER_STATE state);
int dms_devnode_fault_diag(u32 dev_id, int node_type, int node_id, int *state);
ssize_t dms_devnode_print_node_list(char *buf);
void dev_node_release(int owner_pid);
int dms_update_devnode_cb(u32 dev_id, int node_type, int node_id,
    int (*update_dms_node)(struct dms_node *node));

#endif

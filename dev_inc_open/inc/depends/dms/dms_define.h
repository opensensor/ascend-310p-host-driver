/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-09-12
 */

#ifndef DMS_DEFINE_H
#define DMS_DEFINE_H

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include "dms/dms_node_type.h"

#define DMS_MAX_DEV_NAME_LEN 16
#define DEVICE_NODE_NUM_MAX 4

typedef enum {
    FREQSCALING = 0,
    POWERGATINGEN,
    CLOCKGATINGEN,
    RESETEN,
    DEVICE_STATUS,
    MAXDEVOPSIDNUMS = 0xff,
} DEV_POWER_OP_TYPE;

typedef union {
    unsigned int level;
    int flag;
} DEV_POWER_OP_VAL;

typedef struct {
    DEV_POWER_OP_TYPE op_type;
    DEV_POWER_OP_VAL op_val;
} DEVPOWER_OP;

typedef enum {
    POWER_STATE_SUSPEND,
    POWER_STATE_POWEROFF,
    POWER_STATE_RESET,
    POWER_STATE_BIST,
    POWER_STATE_MAX
} DSMI_POWER_STATE;

struct dms_node_operations;

struct dms_node {
    struct list_head list;
    int node_type;
    int node_id;
    int pid;
    char node_name[DMS_MAX_DEV_NAME_LEN];
    unsigned long long capacity;
    int permission;
    int owner_devid;
    struct dms_node *owner_device;
    int inner_node_id;

    struct dms_node *sub_node[DEVICE_NODE_NUM_MAX];
    int state;
    struct dms_node_operations *ops;
};

struct dms_dev_data_attr {
    int info_type;
    int info_life;
    int read_permission;
    int write_permission;
};

typedef struct dms_fault_inject_info {
    unsigned int device_id;
    unsigned int node_type;
    unsigned int node_id;
    unsigned int sub_node_type;
    unsigned int sub_node_id;
    unsigned int fault_index;
    unsigned int event_id;
    unsigned int reserve1;
    unsigned int reserve2;
} dms_fault_inject_t;

struct dms_node_operations {
    int (*init)(struct dms_node *device);
    void (*uninit)(struct dms_node *device);
    int (*get_info_list)(struct dms_node *device, struct dms_dev_data_attr *InfoList);
    int (*get_state)(struct dms_node *device, unsigned int *state);
    int (*get_capacity)(struct dms_node *device, unsigned long long *Capacity);
    int (*set_power_state)(struct dms_node *device, DSMI_POWER_STATE power_state);
    int (*scan)(struct dms_node *device, int *state);
    int (*fault_diag)(struct dms_node *device, int *state);
    int (*event_notify)(struct dms_node *device, int event);
    int (*get_link_state)(struct dms_node *device1, 
        struct dms_node *device2, unsigned int *state);
    int (*set_link_state)(struct dms_node *device1, 
        struct dms_node *device2, unsigned int state);
    int (*fault_inject)(dms_fault_inject_t info);
    int (*get_fault_inject_info)(struct dms_node *node, dms_fault_inject_t *buf,
        unsigned int buf_cnt, unsigned int *real_cnt);
    
    int (*enable_device)(struct dms_node *device);
    int (*disable_device)(struct dms_node *device);
    int (*set_power_info)(struct dms_node *device, void *buf, unsigned int size);
    int (*get_power_info)(struct dms_node *device, void *buf, unsigned int size);
};

struct dms_node *dms_get_devnode_cb(u32 dev_id, int node_type, int node_id);
int dms_register_dev_node(struct dms_node *node);
int dms_update_devnode_cb(u32 dev_id, int node_type, int node_id,
    int (*update_dms_node)(struct dms_node *node));
int dms_unregister_dev_node(struct dms_node *node);

#endif

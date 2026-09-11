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
 * Create: 2025-07-23
 */

#ifndef _UDIS_H_
#define _UDIS_H_

#include <linux/types.h>

typedef enum {
    UDIS_MODULE_DEVMNG = 0,
    UDIS_MODULE_MEMORY,
    UDIS_MODULE_LP,
    UDIS_MODULE_SVM,
    UDIS_MODULE_NETWORK,
    UDIS_MODULE_TS,
    UDIS_MODULE_MAX
} UDIS_MODULE_TYPE;

typedef enum {
    UPDATE_ONLY_ONCE = 0,
    UPDATE_IMMEDIATELY,
    UPDATE_PERIOD_LEVEL_1, /* 100ms */
    UPDATE_TYPE_MAX
} UDIS_UPDATE_TYPE;

#define UDIS_INFO_BLOCK_SIZE 256
#define UDIS_INFO_BLOCK_HEAD_LEN 48
#define UDIS_MAX_DATA_LEN (UDIS_INFO_BLOCK_SIZE - UDIS_INFO_BLOCK_HEAD_LEN)
#define UDIS_MAX_NAME_LEN 16
#define TASK_NAME_MAX_LEN 16

struct udis_dev_info {
    char name[UDIS_MAX_NAME_LEN];
    UDIS_UPDATE_TYPE update_type;
    unsigned int acc_ctrl;
    unsigned long last_update_time;
    unsigned int data_len;
    char data[UDIS_MAX_DATA_LEN];
    char reserved[12];
};

struct udis_addr_info {
    char name[UDIS_MAX_NAME_LEN];
    dma_addr_t dma_addr;
    unsigned int data_len;
    UDIS_UPDATE_TYPE update_type;
    unsigned int acc_ctrl;
    char reserved[28];
};

typedef int (*udis_trigger)(unsigned int udevid, struct udis_dev_info *udis_info);

int hal_kernel_set_udis_info(unsigned int udevid, UDIS_MODULE_TYPE module_type, const struct udis_dev_info *udis_info);
int hal_kernel_get_udis_info(unsigned int udevid, UDIS_MODULE_TYPE module_type, struct udis_dev_info *udis_info);
int hal_kernel_register_udis_addr(unsigned int udevid, UDIS_MODULE_TYPE module_type, struct udis_addr_info *addr_info);
int hal_kernel_unregister_udis_addr(unsigned int udevid, UDIS_MODULE_TYPE module_type, char name[]);
int hal_kernel_register_udis_func(UDIS_MODULE_TYPE module_type, const char *name, udis_trigger func);
int hal_kernel_unregister_udis_func(UDIS_MODULE_TYPE module_type, const char *name);
#endif
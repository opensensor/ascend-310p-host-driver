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

#ifndef DEVDRV_CHIP_DEV_MAP_H
#define DEVDRV_CHIP_DEV_MAP_H

#include "ka_list_pub.h"

#include "devdrv_user_common.h"

#define RANDOM_SIZE 24
#define NOT_ASSIGN_TO_CHIP 0
#define ASSIGN_TO_CHIP 1

typedef struct _devdrv_chip_dev_node {
    unsigned int dev_id;
    unsigned int assign_to_chip_flag;
    unsigned int belong_to_chip;
    char random_number[RANDOM_SIZE];
    ka_list_head_t dev_node_list;
} devdrv_chip_dev_node_t;

typedef struct _devdrv_chip_node {
    unsigned int chip_id;
    unsigned int dev_num; /* device number belong to this chip */
    ka_list_head_t dev_head;   /* link list head for devices of one chip */
    ka_list_head_t chip_node_list;  /* link node for chips */
} devdrv_chip_node_t;

typedef struct _devdrv_chip_dev_map {
    unsigned int chip_num; /* total chip number */
    ka_list_head_t chip_head;
    unsigned int dev_num; /* total device num */
    devdrv_chip_dev_node_t *all_dev_info;
} devdrv_chip_dev_map_t;

int devdrv_manager_get_chip_count(int *count);
int devdrv_manager_get_chip_list(struct devdrv_chip_list *chip_info);
int devdrv_manager_get_device_from_chip(struct devdrv_chip_dev_list *chip_dev_list);
int devdrv_manager_get_chip_from_device(struct devdrv_get_dev_chip_id *chip_from_dev);

#endif
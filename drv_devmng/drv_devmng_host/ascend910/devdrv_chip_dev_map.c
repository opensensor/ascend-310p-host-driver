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

#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_base_pub.h"

#include "devdrv_manager_common.h"
#include "devdrv_manager.h"
#include "devdrv_manager_container.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_chip_dev_map.h"

STATIC int devdrv_manager_chip_dev_map_init(devdrv_chip_dev_map_t **map_info)
{
    if (map_info == NULL) {
        devdrv_drv_err("Invalid parameter, map_info is null.\n");
        return -EINVAL;
    }

    *map_info = (devdrv_chip_dev_map_t *)dbl_kzalloc(sizeof(devdrv_chip_dev_map_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if ((*map_info) == NULL) {
        devdrv_drv_err("Alloc memory for chip dev map structure fail.\n");
        return -ENOMEM;
    }
    KA_INIT_LIST_HEAD(&(*map_info)->chip_head);
    return 0;
}

STATIC int devdrv_get_random_number(devdrv_chip_dev_node_t *dev_node)
{
    int ret;

    ret = memset_s(dev_node->random_number, RANDOM_SIZE, 0, RANDOM_SIZE);
    if (ret != 0) {
        devdrv_drv_err("memset for random sign fail, ret=%d.\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_CHIP_DIE
    ret = devdrv_manager_get_random_from_dev_info(dev_node->dev_id, dev_node->random_number, RANDOM_SIZE);
    if (ret) {
        devdrv_drv_err("Get random from device info failed. (dev_id=%u; ret=%d)\n", dev_node->dev_id, ret);
        return ret;
    }
#else
    dev_node->random_number[0] = dev_node->dev_id / 2; /* 2 device assigned to 1 chip */
#endif

    return 0;
}

STATIC int devdrv_get_dev_num_and_list(unsigned int *dev_num, unsigned int *dev_id_list, unsigned int list_size)
{
    unsigned int i;

    *dev_num = devdrv_manager_get_devnum();
    if ((*dev_num > DEVDRV_MAX_DAVINCI_NUM) || (*dev_num == 0)) {
        devdrv_drv_err("invalid dev_num(%u)\n", *dev_num);
        return -EINVAL;
    }

    *dev_num = (list_size > *dev_num ? *dev_num : list_size);
    for (i = 0; i < *dev_num; i++) {
        dev_id_list[i] = i;
    }

    return 0;
}

STATIC int devdrv_get_all_dev_random_num(devdrv_chip_dev_node_t *dev_info,
    unsigned int *dev_id_list, unsigned int dev_num)
{
    unsigned int i;
    devdrv_chip_dev_node_t *dev_info_node = NULL;

    for (i = 0; i < dev_num; i++) {
        dev_info_node = dev_info + i;
        dev_info_node->dev_id = dev_id_list[i];

        if (devdrv_get_random_number(dev_info_node) != 0) {
            devdrv_drv_err("devdrv_get_random_number fail.\n");
            return -EINVAL;
        }
    }

    return 0;
}

STATIC int devdrv_dev_list_init(devdrv_chip_dev_map_t *map_info)
{
    int ret;
    unsigned int i, j;
    devdrv_chip_dev_node_t *dev_node_cur = NULL;
    devdrv_chip_dev_node_t *dev_node_cmp = NULL;
    devdrv_chip_dev_node_t *dev_list_head = map_info->all_dev_info;

    map_info->chip_num = 0;
    for (i = 0; i < map_info->dev_num; i++) {
        dev_node_cur = dev_list_head + i;
        if (dev_node_cur->assign_to_chip_flag == ASSIGN_TO_CHIP) {
            continue;
        }

        for (j = i + 1; j < map_info->dev_num; j++) {
            dev_node_cmp = dev_list_head + j;
            ret = ka_base_memcmp(dev_node_cur->random_number, dev_node_cmp->random_number, RANDOM_SIZE);
            if (ret != 0) {
                continue;
            }

            /* if current node and compare node have the same random number, set the same chip_num */
            dev_node_cmp->assign_to_chip_flag = ASSIGN_TO_CHIP;
            dev_node_cmp->belong_to_chip = map_info->chip_num;
        }

        dev_node_cur->assign_to_chip_flag = ASSIGN_TO_CHIP;
        dev_node_cur->belong_to_chip = map_info->chip_num;
        map_info->chip_num++;

        if (map_info->chip_num > DEVDRV_MAX_CHIP_NUM) {
            devdrv_drv_err("chip node num[%u] invalid.\n", map_info->chip_num);
            return -EINVAL;
        }
    }

    /* dfx info */
    for (i = 0; i < map_info->dev_num; i++) {
        dev_node_cur = dev_list_head + i;
        devdrv_drv_debug("dev[%u] chip_flag[%u] chip_id[%u].\n", dev_node_cur->dev_id,
            dev_node_cur->assign_to_chip_flag, dev_node_cur->belong_to_chip);
    }

    return 0;
}

STATIC int devdrv_creat_chip_and_dev_linklist(devdrv_chip_dev_map_t *map_info)
{
    unsigned int i, j;
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n_dev = NULL;
    ka_list_head_t *pos_dev = NULL;
    devdrv_chip_node_t *chip_node = NULL;
    devdrv_chip_dev_node_t *dev_node = NULL;

    for (i = 0; i < map_info->chip_num; i++) {
        chip_node = (devdrv_chip_node_t *)dbl_kzalloc(sizeof(devdrv_chip_node_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (chip_node == NULL) {
            devdrv_drv_err("alloc memory for chip node fail.\n");
            return -ENOMEM;
        }

        ka_list_add_tail(&chip_node->chip_node_list, &map_info->chip_head);
        KA_INIT_LIST_HEAD(&chip_node->dev_head);
        chip_node->chip_id = i;

        /* devices has the same chip_num assign to the same chip */
        for (j = 0; j < map_info->dev_num; j++) {
            dev_node = map_info->all_dev_info + j;
            if ((dev_node->assign_to_chip_flag == ASSIGN_TO_CHIP) && (dev_node->belong_to_chip == i)) {
                ka_list_add_tail(&dev_node->dev_node_list, &chip_node->dev_head);
                chip_node->dev_num++;
            }
        }
    }

    /* dfx info */
    if (ka_list_empty_careful(&map_info->chip_head)) {
        devdrv_drv_debug("chip link list is empty.\n");
        return 0;
    }

    ka_list_for_each_safe(pos, n, &map_info->chip_head) {
        chip_node = ka_list_entry(pos, devdrv_chip_node_t, chip_node_list);
        if (ka_list_empty_careful(&chip_node->dev_head)) {
            devdrv_drv_debug("device link list is empty.\n");
            continue;
        }

        ka_list_for_each_safe(pos_dev, n_dev, &chip_node->dev_head) {
            dev_node = ka_list_entry(pos_dev, devdrv_chip_dev_node_t, dev_node_list);
            devdrv_drv_debug("dev[%u] belong to chip[%u].\n", dev_node->dev_id, dev_node->belong_to_chip);
        }
    }

    return 0;
}

STATIC void devdrv_chip_dev_map_resource_free(devdrv_chip_dev_map_t **map_info)
{
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;
    devdrv_chip_node_t *chip_node = NULL;
    devdrv_chip_dev_map_t *chip_dev_map = NULL;
    if ((map_info == NULL) || ((*map_info) == NULL)) {
        return;
    }
    chip_dev_map = *map_info;
    if (!ka_list_empty_careful(&chip_dev_map->chip_head)) {
        ka_list_for_each_safe(pos, n, &chip_dev_map->chip_head) {
            chip_node = ka_list_entry(pos, devdrv_chip_node_t, chip_node_list);
            ka_list_del(&chip_node->chip_node_list);
            dbl_kfree(chip_node);
            chip_node = NULL;
        }
    }

    if (chip_dev_map->all_dev_info != NULL) {
        dbl_kfree(chip_dev_map->all_dev_info);
        chip_dev_map->all_dev_info = NULL;
    }

    chip_dev_map->chip_num = 0;
    chip_dev_map->dev_num = 0;
    dbl_kfree(chip_dev_map);
    *map_info = NULL;
}

STATIC int devdrv_creat_chip_dev_map(devdrv_chip_dev_map_t *chip_dev_map)
{
    int ret;
    unsigned int dev_num;
    unsigned int *dev_id_list = NULL;
    devdrv_chip_dev_node_t *dev_node = NULL;

    /* 0 input parameter check */
    if (chip_dev_map == NULL) {
        devdrv_drv_err("Input parameter chip_dev_map is NULL.\n");
        return -EINVAL;
    }

    /* 1 get device number and device id list */
    dev_id_list = (unsigned int*)dbl_kzalloc(sizeof(unsigned int) * DEVDRV_MAX_DAVINCI_NUM,
                                             KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (dev_id_list == NULL) {
        devdrv_drv_err("Allocate memory for device list failed.\n");
        return -ENOMEM;
    }
    ret = devdrv_get_dev_num_and_list(&dev_num, dev_id_list, DEVDRV_MAX_DAVINCI_NUM);
    if (ret != 0) {
        dbl_kfree(dev_id_list);
        dev_id_list = NULL;
        devdrv_drv_err("Get device list failed. (ret=%d)\n", ret);
        return ret;
    }

    /* 2 get random num of all device */
    dev_node = (devdrv_chip_dev_node_t *)dbl_kzalloc(dev_num * sizeof(devdrv_chip_dev_node_t),
                                                     KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (dev_node == NULL) {
        dbl_kfree(dev_id_list);
        dev_id_list = NULL;
        devdrv_drv_err("Alloc memory for device fail.\n");
        return -ENOMEM;
    }

    chip_dev_map->dev_num = dev_num;
    chip_dev_map->all_dev_info = dev_node;

    ret = devdrv_get_all_dev_random_num(chip_dev_map->all_dev_info, dev_id_list, dev_num);
    if (ret != 0) {
        devdrv_drv_err("devdrv_dev_random_map_init fail, ret=%d.\n", ret);
        goto FREE_EXIT;
    }

    /* 3 compute chip num for each device */
    ret = devdrv_dev_list_init(chip_dev_map);
    if (ret != 0) {
        devdrv_drv_err("devdrv_dev_list_init fail. (ret=%d)\n", ret);
        goto FREE_EXIT;
    }

    /* 4 compute device list for each chip */
    ret = devdrv_creat_chip_and_dev_linklist(chip_dev_map);
    if (ret != 0) {
        devdrv_drv_err("devdrv_creat_chip_and_dev_linklist fail. (ret=%d)\n", ret);
        goto FREE_EXIT;
    }

    dbl_kfree(dev_id_list);
    dev_id_list = NULL;
    return 0;

FREE_EXIT:
    dbl_kfree(dev_id_list);
    dev_id_list = NULL;
    return ret;
}

int devdrv_manager_get_chip_count(int *count)
{
    int ret;
    devdrv_chip_dev_map_t *chip_dev_map = NULL;

    /* 1 malloc memory for chip_dev_map */
    ret = devdrv_manager_chip_dev_map_init(&chip_dev_map);
    if (ret != 0) {
        devdrv_drv_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }
    /* 2 build the relationship between device and chip */
    ret = devdrv_creat_chip_dev_map(chip_dev_map);
    if (ret != 0) {
        devdrv_drv_err("devdrv_chip_dev_map_init fail, ret=%d.\n", ret);
        devdrv_chip_dev_map_resource_free(&chip_dev_map);
        return ret;
    }

    /* 3 get chip count */
    *count = chip_dev_map->chip_num;
    devdrv_chip_dev_map_resource_free(&chip_dev_map);
    return 0;
}

int devdrv_manager_get_chip_list(struct devdrv_chip_list *chip_info)
{
    ka_list_head_t *n = NULL;
    ka_list_head_t *pos = NULL;
    devdrv_chip_node_t *chip_node = NULL;
    devdrv_chip_dev_map_t *chip_dev_map = NULL;

    /* 1 malloc memory for chip_dev_map */
    if (devdrv_manager_chip_dev_map_init(&chip_dev_map) != 0) {
        devdrv_drv_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }

    /* 2 build the relationship between device and chip */
    if (devdrv_creat_chip_dev_map(chip_dev_map) != 0) {
        devdrv_drv_err("Create chip device map failed.\n");
        devdrv_chip_dev_map_resource_free(&chip_dev_map);
        return -EINVAL;
    }

    if (!ka_list_empty_careful(&chip_dev_map->chip_head)) {
        int num = 0;
        ka_list_for_each_safe(pos, n, &chip_dev_map->chip_head) {
            chip_node = ka_list_entry(pos, devdrv_chip_node_t, chip_node_list);
            if (num >= DEVDRV_MAX_CHIP_NUM) {
                devdrv_drv_err("chip node num[%d] invalid.\n", num);
                devdrv_chip_dev_map_resource_free(&chip_dev_map);
                return -EINVAL;
            }

            chip_info->chip_list[num] = chip_node->chip_id;
            num++;
        }
    }

    chip_info->count = chip_dev_map->chip_num;
    devdrv_chip_dev_map_resource_free(&chip_dev_map);
    return 0;
}

int devdrv_manager_get_device_from_chip(struct devdrv_chip_dev_list *chip_dev_list)
{
    int num = 0;
    ka_list_head_t *n_dev = NULL;
    ka_list_head_t *pos_dev = NULL;
    ka_list_head_t *n_chip = NULL;
    ka_list_head_t *pos_chip = NULL;
    devdrv_chip_node_t *chip_node = NULL;
    devdrv_chip_dev_node_t *dev_node = NULL;
    devdrv_chip_dev_map_t *chip_dev_map = NULL;

    /* 0 check input parameter */
    if (chip_dev_list->chip_id >= DEVDRV_MAX_CHIP_NUM) {
        devdrv_drv_err("Chip id[%d] invalid.\n", chip_dev_list->chip_id);
        return -EINVAL;
    }

    /* 1 malloc memory for chip_dev_map */
    if (devdrv_manager_chip_dev_map_init(&chip_dev_map) != 0) {
        devdrv_drv_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }

    /* 2 build the relationship between device and chip */
    if (devdrv_creat_chip_dev_map(chip_dev_map) != 0) {
        devdrv_drv_err("Create chip device map failed.\n");
        goto ERROR_OUT;
    }

    if (ka_list_empty_careful(&chip_dev_map->chip_head)) {
        devdrv_drv_err("Chip head list is empty.\n");
        goto ERROR_OUT;
    }
    ka_list_for_each_safe(pos_chip, n_chip, &chip_dev_map->chip_head) {
        chip_node = ka_list_entry(pos_chip, devdrv_chip_node_t, chip_node_list);
        if (chip_node->chip_id == chip_dev_list->chip_id) {
            if (ka_list_empty_careful(&chip_node->dev_head)) {
                devdrv_drv_err("Device list is empty. (chip_id=%d)\n", chip_dev_list->chip_id);
                goto ERROR_OUT;
            }

            ka_list_for_each_safe(pos_dev, n_dev, &chip_node->dev_head) {
                dev_node = ka_list_entry(pos_dev, devdrv_chip_dev_node_t, dev_node_list);
                chip_dev_list->dev_list[num] = dev_node->dev_id;
                num++;
            }
            chip_dev_list->count = num;
            devdrv_chip_dev_map_resource_free(&chip_dev_map);
            return 0;
        }
    }

ERROR_OUT:
    devdrv_chip_dev_map_resource_free(&chip_dev_map);
    return -EINVAL;
}

int devdrv_manager_get_chip_from_device(struct devdrv_get_dev_chip_id *chip_from_dev)
{
    ka_list_head_t *n_dev = NULL;
    ka_list_head_t *pos_dev = NULL;
    ka_list_head_t *n_chip = NULL;
    ka_list_head_t *pos_chip = NULL;
    devdrv_chip_node_t *chip_node = NULL;
    devdrv_chip_dev_node_t *dev_node = NULL;
    devdrv_chip_dev_map_t *chip_dev_map = NULL;

    if (chip_from_dev->dev_id >= DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_err("Dev_id[%d] invalid.\n", chip_from_dev->dev_id);
        return -EINVAL;
    }

    /* 1 malloc memory for chip_dev_map */
    if (devdrv_manager_chip_dev_map_init(&chip_dev_map) != 0) {
        devdrv_drv_err("Allocate memory for chip dev map failed.\n");
        return -ENOMEM;
    }

    /* 2 build the relationship between device and chip */
    if (devdrv_creat_chip_dev_map(chip_dev_map) != 0) {
        devdrv_drv_err("Create chip device map failed.\n");
        goto ERR_OUT;
    }

    /* 3 get chip_id by device_id */
    if (ka_list_empty_careful(&chip_dev_map->chip_head)) {
        devdrv_drv_err("Chip head list is empty.\n");
        goto ERR_OUT;
    }
    ka_list_for_each_safe(pos_chip, n_chip, &chip_dev_map->chip_head) {
        chip_node = ka_list_entry(pos_chip, devdrv_chip_node_t, chip_node_list);
        if (ka_list_empty_careful(&chip_node->dev_head)) {
            devdrv_drv_err("device link list is empty.\n");
            goto ERR_OUT;
        }

        ka_list_for_each_safe(pos_dev, n_dev, &chip_node->dev_head) {
            dev_node = ka_list_entry(pos_dev, devdrv_chip_dev_node_t, dev_node_list);
            if (chip_from_dev->dev_id == dev_node->dev_id) {
                chip_from_dev->chip_id = chip_node->chip_id;
                devdrv_chip_dev_map_resource_free(&chip_dev_map);
                return 0;
            }
        }
    }

ERR_OUT:
    devdrv_chip_dev_map_resource_free(&chip_dev_map);
    return -EINVAL;
}


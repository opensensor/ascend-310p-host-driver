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

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/securec.h>

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "dfm_dev_register.h"
#include "dbl_mem_alloc_interface.h"
#include "dfm_report.h"
#include "dms_dev_node.h"

#ifdef CFG_BUILD_DEBUG

#define DFM_MAX_MODULE_NUM   (64)
static struct dfm_struct *g_dfm_structs[DFM_MAX_MODULE_NUM];
static u32 g_dfm_struct_num = 0;

void dfm_struct_save(struct dfm_struct *dfm)
{
    if (g_dfm_struct_num >= DFM_MAX_MODULE_NUM) {
        return;
    }

    g_dfm_structs[g_dfm_struct_num] = dfm;
    g_dfm_struct_num++;
}

struct dfm_struct **dfm_get_struct_array(u32 *dfm_num)
{
    *dfm_num = g_dfm_struct_num;
    return g_dfm_structs;
}

#else  // !CFG_BUILD_DEBUG

static inline void dfm_struct_save(__attribute__((unused)) struct dfm_struct *dfm)
{
}

struct dfm_struct **dfm_get_struct_array(u32 *dfm_num)
{
    *dfm_num = 0;
    return NULL;
}
#endif

int dfm_struct_init(struct dfm_struct *dfm, u32 node_num, u32 sensor_num)
{
    u32 i;

    if (dfm == NULL || node_num == 0 || sensor_num == 0) {
        dfm_err("dfm is null or node_num is 0.(node_num=%u, sensor_num=%u)\n", node_num, sensor_num);
        return -EINVAL;
    }

    dfm->dev_nodes = (struct dfm_node *)dbl_kzalloc(node_num * sizeof(struct dfm_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dfm->dev_nodes == NULL) {
        dfm_err("kmalloc dfm node failed.\n");
        return -ENOMEM;
    }
    dfm->node_num = node_num;

    for (i = 0; i < node_num; ++i) {
        dfm->dev_nodes[i].sensor_num = sensor_num;
        dfm->dev_nodes[i].sensors = (struct dfm_sensor *)dbl_kzalloc(
            sensor_num * sizeof(struct dfm_sensor), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (dfm->dev_nodes[i].sensors == NULL) {
            dfm_err("kmalloc sensors failed. sensor_num=%u\n", sensor_num);
            return -ENOMEM;
        }
    }
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dfm_struct_init);

void dfm_struct_final(struct dfm_struct *dfm, u32 node_num, u32 sensor_num)
{
    u32 i;

    if (dfm == NULL) {
        dfm_err("dfm is null.\n");
        return;
    }

    if (dfm->dev_nodes != NULL) {
        for (i = 0; i < node_num; i++) {
            if (dfm->dev_nodes[i].sensors == NULL) {
                continue;
            }

            dbl_kfree(dfm->dev_nodes[i].sensors);
            dfm->dev_nodes[i].sensors = NULL;
            dfm->dev_nodes[i].sensor_num = 0;
        }

        dbl_kfree(dfm->dev_nodes);
        dfm->dev_nodes = NULL;
        dfm->node_num = 0;
    }
}
KA_EXPORT_SYMBOL_GPL(dfm_struct_final);

struct dfm_node *dfm_get_node(struct dfm_struct *dfm, u32 node_id)
{
    if (node_id >= dfm->node_num) {
        dfm_err("invalid node id. (node id=%u)\n", node_id);
        return NULL;
    }
    return &dfm->dev_nodes[node_id];
}

int dfm_duplicate_dms_nodes(struct dfm_struct *dfm, dms_node_t *node)
{
    dms_node_t *new_node = NULL;
    u32 i, j, succ;

    if ((dfm == NULL) || (node == NULL)) {
        dfm_err("the dfm or the device node is null.\n");
        return -ENOMEM;
    }

    if (dfm->dev_nodes == NULL) {
        dfm_err("the dfm nodes has not be initialized, please call dfm_init_struct first.\n");
        return -ENOMEM;
    }

    succ = 0;
    for (i = 0; i < dfm->node_num; ++i) {
        new_node = (dms_node_t *)dbl_kmalloc(sizeof(dms_node_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (new_node == NULL) {
            dfm_err("create dms node failed.(index=%u)", i);
            goto _fail;
        }

        if (memcpy_s(new_node, sizeof(dms_node_t), node, sizeof(dms_node_t)) != EOK) {
            dfm_err("memcpy dsm node failed.(index=%u)", i);
            dbl_kfree(new_node);
            new_node = NULL;
            goto _fail;
        }

        new_node->node_id = i;
        dfm->dev_nodes[i].node = new_node;
        dfm->dev_nodes[i].node->owner_devid = (int)dfm->dev_id;
        succ++;
    }
    return 0;

_fail:
    for (j = 0; j < succ; ++j) {
        dbl_kfree(dfm->dev_nodes[j].node);
        dfm->dev_nodes[j].node = NULL;
    }
    return -ENOMEM;
}
KA_EXPORT_SYMBOL(dfm_duplicate_dms_nodes);

void dfm_free_dms_nodes(struct dfm_struct *dfm)
{
    u32 i;

    if (dfm == NULL) {
        dfm_err("the dfm is null.\n");
        return;
    }

    for (i = 0; i < dfm->node_num; ++i) {
        if (dfm->dev_nodes[i].node != NULL) {
            dbl_kfree(dfm->dev_nodes[i].node);
            dfm->dev_nodes[i].node = NULL;
        }
    }
}
KA_EXPORT_SYMBOL(dfm_free_dms_nodes);

struct dfm_sensor *dfm_get_sensor(struct dfm_node *dev_node, u8 sensor_type)
{
    u32 i;

    for (i = 0; i < dev_node->sensor_num; ++i) {
        if (dev_node->sensors[i].dms_sensor.sensor_type == sensor_type) {
            return &dev_node->sensors[i];
        }
    }
    return NULL;
}

STATIC int dfm_register_dms_node(dms_node_t *dev_node)
{
    int ret;

    if (dev_node == NULL) {
        dfm_err("Invalid para, h_dev is NULL.\n");
        return -EINVAL;
    }

    ret = dms_register_dev_node(dev_node);
    if (ret != 0) {
        dfm_err("Register dev_node failed. (node name=%.*s, ret=%d.)\n",
            DMS_MAX_DEV_NAME_LEN, dev_node->node_name, ret);
    }
    return ret;
}

int dfm_register_dms_sensors(dms_node_t *dev_node, u32 sensor_num, struct dfm_sensor *sensors)
{
    int ret;
    unsigned int i;

    if ((sensor_num == 0) || (sensors == NULL)) {
        dfm_err("Invalid para, sensor is null\n");
        return -EINVAL;
    }

    for (i = 0; i < sensor_num; i++) {
        INIT_LIST_HEAD(&sensors[i].event_list);
        spin_lock_init(&sensors[i].sensor_lock);
        sensors[i].error_num = 0;

        ret = dms_sensor_register(dev_node, &sensors[i].dms_sensor);
        if (ret != 0) {
            dfm_err("Register sensor failed. (sensor_idx=%u; ret=%d.)\n", i, ret);
            goto out;
        }
    }
    return 0;

out:
    if (i > 0) {
        (void)dms_sensor_node_unregister(dev_node);
    }
    return ret;
}

STATIC void dfm_unregister_dms_node(dms_node_t *dev_node)
{
    int ret;

    if (dev_node == NULL) {
        dfm_err("Invalid para, h_dev is NULL.\n");
        return;
    }

    ret = dms_unregister_dev_node(dev_node);
    if (ret != 0) {
        dfm_warn("SOC unregister dev node warn, (node_name=%.*s, ret=%d)\n",
            DMS_MAX_DEV_NAME_LEN, dev_node->node_name, ret);
    }
}

STATIC int dfm_register_dfm_node(struct dfm_node *node)
{
    int ret;

    ret = dfm_register_dms_node(node->node);
    if (ret != 0) {
        dfm_err("register dms node failed.(ret=%d)\n", ret);
        return ret;
    }

    ret = dfm_register_dms_sensors(node->node, node->sensor_num, node->sensors);
    if (ret != 0) {
        dfm_err("register %.*s's sensors failed.(ret=%d)\n", DMS_MAX_DEV_NAME_LEN, node->node->node_name, ret);
        goto _fail;
    }

    if (node->fpdc_notify != NULL) {
        ret = fpdc_register_fault_notifier(node->node->node_type, node->fpdc_notify);
        if (ret != 0) {
            dfm_err("fpdc_register_fault_notifier failed. (dev_name=%.*s; ret=%d.)\n",
                DMS_MAX_DEV_NAME_LEN, node->node->node_name, ret);
            goto _fail;
        }
    }

    return 0;

_fail:
    dms_sensor_node_unregister(node->node);
    dfm_unregister_dms_node(node->node);
    return ret;
}

void dfm_unregister_dfm_node(struct dfm_node *node)
{
    u32 sensor_idx;

    if (node->fpdc_notify != NULL) {
        fpdc_unregister_fault_notifier(node->node->node_type);
    }

    for (sensor_idx = 0; sensor_idx < node->sensor_num; ++sensor_idx) {
        dfm_clear_event_list(&node->sensors[sensor_idx]);
    }
    dms_sensor_node_unregister(node->node);
    dfm_unregister_dms_node(node->node);
}

int dfm_register_nodes(struct dfm_struct *dfm)
{
    u32 i, j;

    if (dfm == NULL) {
        dfm_err("dfm is null\n");
        return -EINVAL;
    }
    if ((dfm->node_num == 0) || (dfm->dev_nodes == NULL)) {
        return -EINVAL;
    }

    for (i = 0; i < dfm->node_num; ++i) {
        if (dfm_register_dfm_node(&dfm->dev_nodes[i]) != 0) {
            dfm_err("register dms node[%u] failed\n", i);
            goto _fail;
        }
    }

    dfm_struct_save(dfm);
    return 0;

_fail:
    for (j = 0; j < i; ++j) {
        dfm_unregister_dfm_node(&dfm->dev_nodes[j]);
    }
    return -EFAULT;
}
KA_EXPORT_SYMBOL_GPL(dfm_register_nodes);

void dfm_unregister_nodes(const struct dfm_struct *dfm)
{
    u32 i;
    if (dfm == NULL || dfm->dev_nodes == NULL || dfm->node_num == 0) {
        return;
    }

    for (i = 0; i < dfm->node_num; ++i) {
        dfm_unregister_dfm_node(&dfm->dev_nodes[i]);
    }
}
KA_EXPORT_SYMBOL_GPL(dfm_unregister_nodes);

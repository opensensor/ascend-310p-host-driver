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

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/poll.h>
#include <linux/sort.h>
#include <linux/vmalloc.h>
#include <linux/of_address.h>
#include <linux/of.h>

#include "ka_driver_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "securec.h"

#include "dms_define.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_common.h"
#include "dms_probe.h"

#ifndef AOS_LLVM_BUILD
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
#include <linux/namei.h>
#endif
#else
#include <linux/namei.h>
#endif

#define HOST_LOCAL 1000

struct state_item_list {
    struct state_item item;
    struct list_head list;
};

static struct dms_system_ctrl_block *g_dms_system_ccb = NULL;

#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
static struct dms_state_table g_dms_state_table = {};

static void dms_state_table_list_node_init(struct state_item_list *item_node, uint32_t node_type, uint32_t node_id)
{
    INIT_LIST_HEAD(&item_node->list);
    item_node->item.node_type = node_type;
    item_node->item.node_id = node_id;
}

static void dms_state_table_parse_node(struct list_head *dms_node_list, struct device_node *node, uint32_t *cnt)
{
    struct state_item_list *item_node = NULL;
    uint32_t *id = NULL;
    uint32_t type, num, i, real_num;
    int ret;

    ret = of_property_read_u32(node, "node_type", &type);
    if (ret != 0) {
        dms_warn("state table node type unrecognizable, (ret=%d)\n", ret);
        return;
    }
    ret = of_property_read_u32(node, "node_num", &num);
    if (ret != 0) {
        dms_warn("state table node num unrecognizable, (type=%u; ret=%d)\n", type, ret);
        return;
    }
    if (num == 0) {
        dms_warn("state table node num warn, (num=0)\n");
        return;
    }

    id = kmalloc(sizeof(uint32_t) * num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (id == NULL) {
        dms_warn("state table node id alloc warn, (type=%u; num=%u)\n", type, num);
        return;
    }

    ret = of_property_read_variable_u32_array(node, "node_id", id, 0, num);
    if (ret < 0) {
        dms_warn("state table node id read warn, (type=%u; num=%u; ret=%d)\n", type, num, ret);
        kfree(id);
        return;
    }

    real_num  = (uint32_t)ret;
    if (real_num != num) {
        dms_warn("state table node num not equal, (type=%u; num=%u; real=%u)\n", type, num, real_num);
    }

    for (i = 0; i < real_num; i++) {
        item_node = kzalloc(sizeof(struct state_item_list), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (item_node == NULL) {
            dms_warn("state table item node alloc warn, (type=%u; i=%u)\n", type, i);
            break;
        }
        dms_state_table_list_node_init(item_node, type, id[i]);
        list_add(&item_node->list, dms_node_list);
        *cnt = *cnt + 1;
    }
    kfree(id);
}

static void dms_state_table_entry_init(struct state_item *item, struct state_item_list *item_node)
{
    item->node_type = item_node->item.node_type;
    item->node_id = item_node->item.node_id;
    item->node = NULL;
}

int state_item_cmp(const void *a, const void *b)
{
    const struct state_item *ia = (const struct state_item *)a;
    const struct state_item *ib = (const struct state_item *)b;
    if (ia->node_type != ib->node_type) {
        return ia->node_type - ib->node_type;
    } else {
        return ia->node_id - ib->node_id;
    }
}

static void dms_init_device_state_table(void)
{
    struct list_head dms_node_list = {};
    struct device_node *np = NULL;
    struct device_node *child = NULL;
    struct state_item_list *item_node = NULL;
    struct state_item_list *temp_node = NULL;
    uint32_t cnt = 0, i = 0;
    INIT_LIST_HEAD(&dms_node_list);

    np = of_find_compatible_node(NULL, NULL, "hisi,dms_device_table");
    if (np == NULL) {
        dms_info("dms state table no valid node\n");
        return;
    }

    for_each_child_of_node(np, child) {
        dms_state_table_parse_node(&dms_node_list, child, &cnt);
    }
    ka_driver_of_node_put(np);

    if (cnt == 0) {
        dms_info("dms state table cnt is 0\n");
        return;
    } 
    g_dms_state_table.item = vzalloc(sizeof(struct state_item) * cnt);
    if (g_dms_state_table.item == NULL) {
        dms_warn("device state table alloc no mem, (size=%zu)\n", sizeof(struct state_item) * cnt);
        list_for_each_entry_safe(item_node, temp_node, &dms_node_list, list) {
            list_del(&item_node->list);
            kfree(item_node);
        }
        return;
    }
    g_dms_state_table.num = cnt;

    list_for_each_entry_safe(item_node, temp_node, &dms_node_list, list) {
        dms_state_table_entry_init(&g_dms_state_table.item[i], item_node);
        list_del(&item_node->list);
        kfree(item_node);
        i++;
    }
    sort(g_dms_state_table.item, cnt, sizeof(struct state_item), state_item_cmp, NULL);
    dms_info("dms state table init success, (cnt=%u).\n", cnt);
}

static void dms_uninit_device_state_table(void)
{
    if (g_dms_state_table.item != NULL) {
        vfree(g_dms_state_table.item);
        g_dms_state_table.item = NULL;
    }
    g_dms_state_table.num = 0;
}

struct dms_state_table *dms_get_state_table(void)
{
    return &g_dms_state_table;
}
KA_EXPORT_SYMBOL(dms_get_state_table);
#endif

struct dms_system_ctrl_block* dms_get_sys_ctrl_cb(void)
{
    return g_dms_system_ccb;
}
KA_EXPORT_SYMBOL(dms_get_sys_ctrl_cb);

static void dms_free_sys_ctrl_cb(void)
{
    if (g_dms_system_ccb != NULL) {
        dbl_kfree(g_dms_system_ccb);
        g_dms_system_ccb = NULL;
    }
}

int dms_check_device_id(int dev_id)
{
    if ((dev_id < 0) || (dev_id >= DEVICE_NUM_MAX)) {
        return -1;
    }
    return 0;
}
KA_EXPORT_SYMBOL(dms_check_device_id);

DMS_MODE_T dms_get_rc_ep_mode(void)
{
#ifdef CFG_FEATURE_EP_MODE
    return DMS_EP_MODE;
#else
    return DMS_RC_MODE;
#endif
}
KA_EXPORT_SYMBOL(dms_get_rc_ep_mode);

struct dms_dev_ctrl_block* dms_get_dev_cb(int dev_id)
{
    struct dms_system_ctrl_block* cb = dms_get_sys_ctrl_cb();
    int ret;
    if (cb == NULL) {
        dms_err("g_dms_system_ccb is NULL. \n");
        return NULL;
    }
    ret = dms_check_device_id(dev_id);
    if (ret != 0) {
        dms_err("dev_id out of range. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return NULL;
    }
    return &cb->dev_cb_table[dev_id];
}
KA_EXPORT_SYMBOL(dms_get_dev_cb);

bool dms_is_devid_valid(int dev_id)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;

    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%d)\n", dev_id);
        return false ;
    }

    if (dev_cb->state != DMS_IN_USED) {
        dms_err("Device state not in used. (dev_id=%d; state=%u)\n", dev_id, dev_cb->state);
        return false ;
    }

    return true;
}
KA_EXPORT_SYMBOL(dms_is_devid_valid);

STATIC int dms_init_dev_cb(void)
{
    int i;
    struct dms_dev_ctrl_block* cb;

    g_dms_system_ccb = dbl_kzalloc(sizeof(struct dms_system_ctrl_block), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_dms_system_ccb == NULL) {
        dms_err("Memory alloc for g_dms_system_ccb failed.\n");
        return -ENOMEM;
    }

    cb = &g_dms_system_ccb->base_cb;
    mutex_init(&cb->node_lock);
    INIT_LIST_HEAD(&cb->dev_node_list);

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        cb = &g_dms_system_ccb->dev_cb_table[i];
        mutex_init(&cb->node_lock);
        INIT_LIST_HEAD(&cb->dev_node_list);
    }
    return 0;
}

STATIC void dms_uninit_dev_cb(void)
{
    int i;
    struct dms_dev_ctrl_block* cb;

    cb = &g_dms_system_ccb->base_cb;
    mutex_lock(&cb->node_lock);
    /* destroy nodelist */
    mutex_unlock(&cb->node_lock);
    mutex_destroy(&cb->node_lock);
    INIT_LIST_HEAD(&cb->dev_node_list);

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        cb = &g_dms_system_ccb->dev_cb_table[i];
        mutex_lock(&cb->node_lock);
        /* destroy nodelist */
        /* exit dev sensor */
        mutex_unlock(&cb->node_lock);
        mutex_destroy(&cb->node_lock);
        INIT_LIST_HEAD(&cb->dev_node_list);
    }

    dms_free_sys_ctrl_cb();
}

STATIC int __init dms_dtm_init(void)
{
    int ret;
    dms_info("dms_dtm_init start.\n");

    ret = dms_init_dev_cb();
    if (ret != 0) {
        dms_err("dms init dev cb failed.(ret=%d)\n", ret);
        return ret;
    }
#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
    dms_init_device_state_table();
#endif
    dms_info("Dms driver init success.\n");
    return ret;
}

STATIC void __exit dms_dtm_exit(void)
{
    dms_info("dms_dtm_exit start.\n");
#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
    dms_uninit_device_state_table();
#endif
    dms_uninit_dev_cb();
    dms_info("Dms dtm driver exit success.\n");
}

module_init(dms_dtm_init);
module_exit(dms_dtm_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");

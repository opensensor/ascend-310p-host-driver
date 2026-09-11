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

#include <linux/delay.h>

#include "securec.h"
#include "dbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"
#include "udis_msg.h"

#include "ka_memory_pub.h"

#define UDIS_DMA_NODE_NUM_INIT 128
#define UDIS_LINK_DMA_NODES_SCALE_UP_FACTOR 2

STATIC struct devdrv_common_msg_client g_udis_common_chan;
STATIC struct udis_link_dma_nodes g_link_dma_nodes[UDIS_DEVICE_UDEVID_MAX] = {0};

struct devdrv_common_msg_client *udis_get_common_msg_client(void)
{
    return  &g_udis_common_chan;
}

struct udis_link_dma_nodes *udis_get_link_dma_nodes(unsigned int udevid)
{
    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalide param. (udevid=%u)\n", udevid);
        return NULL;
    }
    return  &g_link_dma_nodes[udevid];
}

int udis_link_dma_nodes_init(unsigned int udevid)
{
    struct udis_link_dma_nodes *link_dma_nodes = NULL;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalide param. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    link_dma_nodes = udis_get_link_dma_nodes(udevid);
    if (link_dma_nodes == NULL) {
        udis_err("link_dma_nodes is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    if  (link_dma_nodes->dma_nodes != NULL) {
        udis_info("dma_nodes is not NULL, no need to init. (udevid=%u)\n", udevid);
        return 0;
    }

    link_dma_nodes->dma_nodes = (struct devdrv_dma_node *)dbl_kzalloc((sizeof(struct devdrv_dma_node) *
        UDIS_DMA_NODE_NUM_INIT), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (link_dma_nodes->dma_nodes == NULL) {
        udis_err("Failed to alloc dma node array. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }
    link_dma_nodes->capacity = UDIS_DMA_NODE_NUM_INIT;
    link_dma_nodes->node_num = 0;
    return 0;
}

void udis_link_dma_nodes_uninit(unsigned int udevid)
{
    struct udis_link_dma_nodes *link_dma_nodes = NULL;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalide param. (udevid=%u)\n", udevid);
        return;
    }

    link_dma_nodes = udis_get_link_dma_nodes(udevid);
    if (link_dma_nodes == NULL) {
        udis_err("dma_nodes_arr is NULL. (udevid=%u)\n", udevid);
        return;
    }

    if  (link_dma_nodes->dma_nodes == NULL) {
        udis_info("dma_nodes is NULL, no need to uninit. (udevid=%u)\n", udevid);
        return;
    }

    link_dma_nodes->capacity = 0;
    link_dma_nodes->node_num = 0;
    dbl_kfree(link_dma_nodes->dma_nodes);
    link_dma_nodes->dma_nodes = NULL;
    return;
}

int udis_link_dma_nodes_scale_up(unsigned int udevid)
{
    int ret;
    struct devdrv_dma_node *old_nodes = NULL;
    struct devdrv_dma_node *new_nodes = NULL;
    struct udis_link_dma_nodes *link_dma_nodes = NULL;
    unsigned int new_capacity = 0;
    unsigned int old_capacity = 0;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalide param. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    link_dma_nodes = udis_get_link_dma_nodes(udevid);
    if (link_dma_nodes == NULL) {
        udis_err("link_dms_nodes is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    if (link_dma_nodes->dma_nodes == NULL) {
        udis_err("dma_nodes is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    old_capacity = link_dma_nodes->capacity;
    new_capacity = old_capacity * UDIS_LINK_DMA_NODES_SCALE_UP_FACTOR;
    old_nodes = link_dma_nodes->dma_nodes;

    new_nodes = (struct devdrv_dma_node*)dbl_kzalloc(sizeof(struct devdrv_dma_node) * new_capacity,
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (new_nodes == NULL) {
        udis_err("Failed to alloc new dma node array. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = memcpy_s(new_nodes, sizeof(struct devdrv_dma_node) * old_capacity,
        old_nodes, sizeof(struct devdrv_dma_node) * old_capacity);
    if (ret != 0) {
        dbl_kfree(new_nodes);
        new_nodes = NULL;
        udis_err("memcpy_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -ENOMEM;
    }
    dbl_kfree(old_nodes);
    old_nodes = NULL;
    link_dma_nodes->dma_nodes = new_nodes;
    link_dma_nodes->capacity = new_capacity;

    return 0;
}

STATIC int udis_update_link_dma_nodes(unsigned int udevid, const struct udis_ctrl_block *udis_cb,
    UDIS_UPDATE_TYPE update_type)
{
    int ret = 0;
    unsigned int node_num = 0;
    struct udis_link_dma_nodes *link_dma_nodes = NULL;
    struct udis_dma_node *cur;

    if (update_type < UPDATE_PERIOD_LEVEL_1) {
        udis_info("Not period update type, no need to flush link dma nodes. (udevid=%u; update_type=%u; node_num=%u)\n",
            udevid, update_type, node_num);
        return 0;
    }

    link_dma_nodes = udis_get_link_dma_nodes(udevid);
    if ((link_dma_nodes == NULL) || (link_dma_nodes->dma_nodes == NULL)) {
        udis_err("dma_nodes_arr or dma_nodes is NULL. (udevid=%u; update_type=%u)\n", udevid, update_type);
        return -ENOMEM;
    }

    list_for_each_entry(cur, &udis_cb->addr_list[update_type], list) {
        link_dma_nodes->dma_nodes[node_num].src_addr = cur->dev_dma_addr;
        link_dma_nodes->dma_nodes[node_num].dst_addr = cur->host_dma_addr;
        link_dma_nodes->dma_nodes[node_num].size = cur->data_len;
        link_dma_nodes->dma_nodes[node_num].direction = DEVDRV_DMA_DEVICE_TO_HOST;
        link_dma_nodes->dma_nodes[node_num].loc_passid = DEVDRV_DMA_PASSID_DEFAULT;
        ++node_num;
        if (node_num < link_dma_nodes->capacity) {
            continue;
        }
        ret = udis_link_dma_nodes_scale_up(udevid);
        if (ret != 0) {
            udis_err("Link dma nodes scale up failed. (udevid=%u; ret=%d)\n", udevid, ret);
            goto out;
        }
    }

out:
    link_dma_nodes->node_num = node_num;
    return ret;
}

STATIC int udis_check_addr_node(const struct udis_dma_node *addr_node)
{
    int is_discrete_info;
    unsigned int name_len = 0;

    if (addr_node == NULL) {
        udis_err("Addr node is NULL\n");
        return -EINVAL;
    }

    if (addr_node->module_type >= UDIS_MODULE_MAX) {
        udis_err("Invalid module_type. (module_type=%u; max_module_type=%u)\n",
            addr_node->module_type, UDIS_MODULE_MAX - 1);
        return -EINVAL;
    }

    name_len = strnlen(addr_node->name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        udis_err("Invalid name_len. (module_type=%u; name_len=%u; max_name_len=%u)\n",
            addr_node->module_type, name_len, UDIS_MAX_NAME_LEN - 1);
        return -EINVAL;
    }

    if (addr_node->update_type >= UPDATE_TYPE_MAX) {
        udis_err("Invalid update type. (mode_type=%u; name=%s; update_type=%u; max_update_type=%u)\n",
            addr_node->module_type, addr_node->name, addr_node->update_type, UPDATE_TYPE_MAX - 1);
        return -EINVAL;
    }

    is_discrete_info = strcmp(addr_node->name, UDIS_UNIFIED_MODULE_INFO);
    if (((addr_node->dev_dma_addr == UDIS_BAD_DMA_ADDR)) || (addr_node->data_len == 0) ||
        ((is_discrete_info != 0) && (addr_node->data_len > UDIS_MAX_DATA_LEN)) ||
        ((is_discrete_info == 0) && (addr_node->data_len != UDIS_UNIFIED_MODULE_OFFSET))) {
        udis_err("Invalid param. (module_type=%u; name=%s; dma_addr_is_bad=%d; data_len=%u)\n",
            addr_node->module_type, addr_node->name,
            addr_node->dev_dma_addr == UDIS_BAD_DMA_ADDR, addr_node->data_len);
        return -EINVAL;
    }
    return 0;
}

STATIC int udis_addr_node_register(unsigned int udevid, struct udis_ctrl_block *udis_cb,
    struct udis_dma_node *addr_node)
{
    int ret;
    dma_addr_t host_dma_addr = 0;

    ret = udis_alloc_info_block(udevid, addr_node, &host_dma_addr);
    if (ret != 0) {
        udis_err("Init info block failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
            addr_node->module_type, addr_node->name, ret);
        return ret;
    }
    addr_node->host_dma_addr = host_dma_addr;

    ret = udis_addr_list_add_node(udevid, udis_cb, addr_node);
    if (ret != 0) {
        udis_err("Add node to addr list failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
            addr_node->module_type, addr_node->name, ret);
        goto free_info_block;
    }

    ret  = udis_update_link_dma_nodes(udevid, udis_cb, addr_node->update_type);
    if (ret != 0) {
        udis_err("Update link dma nodes failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
            addr_node->module_type, addr_node->name, ret);
        goto remove_node;
    }
    return 0;

remove_node:
    udis_addr_list_remove_node(udevid, udis_cb, addr_node);
free_info_block:
    udis_free_info_block(udevid, addr_node->module_type, addr_node->name, host_dma_addr);
    return ret;
}

STATIC int udis_register_addr(void *msg, u32 *ack_len)
{
    int ret;
    unsigned int udevid;
    struct udis_msg_info *msg_info = NULL;
    struct udis_dma_node *addr_node = NULL;
    struct udis_ctrl_block *udis_cb = NULL;
    struct udis_dma_node *repeat_node = NULL;

    msg_info = (struct udis_msg_info *)msg;
    udevid = msg_info->head.dev_id;
    addr_node = (struct udis_dma_node *)msg_info->payload;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (udis_check_addr_node(addr_node) != 0)) {
        udis_err("Invalid udevid or udis dma addr node. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_cb_read_lock(udevid);
    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        ret = -ENODEV;
        goto unlock_cb_read_lock;
    }

    down_write(&udis_cb->addr_list_lock);
    repeat_node = udis_addr_list_find_node(udis_cb, addr_node->module_type, addr_node->name);
    if (repeat_node != NULL) {
        udis_info("Already have the same addr node. (udevid=%u; module_type=%u; name=%s)\n",
            udevid, addr_node->module_type, addr_node->name);
        *ack_len = 0;
        ret = 0;
        goto unlock_addr_list_lock;
    }

    ret = udis_addr_node_register(udevid, udis_cb, addr_node);
    if (ret != 0) {
        udis_err("Add new addr node failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
            udevid, addr_node->module_type, addr_node->name, ret);
        goto unlock_addr_list_lock;
    }

    (void)udis_dma_sync_copy(udevid, udis_cb, addr_node);

    *ack_len = 0;
    udis_info("Register addr node success.\
 (udevid=%u; module_type=%u; name=%s; update_type=%u; acc_ctrl=%u; data_len=%u)\n", udevid, addr_node->module_type,
        addr_node->name, addr_node->update_type, addr_node->acc_ctrl, addr_node->data_len);

unlock_addr_list_lock:
    up_write(&udis_cb->addr_list_lock);
unlock_cb_read_lock:
    udis_cb_read_unlock(udevid);
    return ret;
}

STATIC int udis_unregister_addr(void *msg, u32 *ack_len)
{
    int ret;
    unsigned int udevid;
    struct udis_msg_info *msg_info = NULL;
    struct udis_dma_node *addr_node = NULL;
    struct udis_dma_node *target_node = NULL;
    struct udis_ctrl_block *udis_cb = NULL;

    msg_info = (struct udis_msg_info *)msg;
    udevid = msg_info->head.dev_id;
    addr_node = (struct udis_dma_node *)msg_info->payload;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (udis_check_addr_node(addr_node) != 0)) {
        udis_err("Invalid udevid or udis dma addr node. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_cb_read_lock(udevid);
    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_cb_read_unlock(udevid);
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    down_write(&udis_cb->addr_list_lock);

    target_node = udis_addr_list_find_node(udis_cb, addr_node->module_type, addr_node->name);
    if (target_node == NULL) {
        udis_info("No matched node found. (udevid=%u; module_type=%u; name=%s)\n", udevid, addr_node->module_type,
            addr_node->name);
        *ack_len = 0;
        ret = 0;
        goto out;
    }

    list_del(&target_node->list);
    ret = udis_update_link_dma_nodes(udevid, udis_cb, addr_node->update_type);
    if (ret != 0) {
        list_add(&target_node->list, &udis_cb->addr_list[target_node->update_type]);
        udis_err("Update link dma nodes failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
            addr_node->module_type, addr_node->name, ret);
        goto out;
    }
    udis_free_info_block(udevid, target_node->module_type, target_node->name, target_node->host_dma_addr);
    dbl_kfree(target_node);
    target_node = NULL;

    *ack_len = 0;
    udis_info("Unregister addr node success.\
 (udevid=%u; module_type=%u; name=%s; update_type=%u; acc_ctrl=%u; data_len=%u)\n", udevid, addr_node->module_type,
        addr_node->name, addr_node->update_type, addr_node->acc_ctrl, addr_node->data_len);
out:
    up_write(&udis_cb->addr_list_lock);
    udis_cb_read_unlock(udevid);
    return ret;
}

STATIC int (*udis_chan_msg_processes[UDIS_CHAN_D2H_MAX_ID])(void *msg, u32 *ack_len) = {
    [UDIS_CHAN_D2H_REGIST] = udis_register_addr,
    [UDIS_CHAN_D2H_UNREGIST] = udis_unregister_addr,
};

STATIC int udis_rx_common_msg_process(unsigned int dev_id, void *data, unsigned int in_data_len, unsigned int out_data_len,
    unsigned int *real_out_len)
{
    unsigned int msg_id;
    struct udis_msg_info *msg_info = NULL;

    if ((dev_id >= UDIS_DEVICE_UDEVID_MAX) || (data == NULL) || (real_out_len == NULL) ||
        (in_data_len < sizeof(struct udis_msg_info))) {
        udis_err("date(%pK) or real_out_len(%pK) is NULL. (devid=%u; in_data_len=%u)\n",
            data, real_out_len, dev_id, in_data_len);
        return -EINVAL;
    }
    msg_info = (struct udis_msg_info *)data;
    msg_id = msg_info->head.msg_id;
    if (msg_id >= UDIS_CHAN_D2H_MAX_ID) {
        udis_err("Invalid msg_id. (dev_id=%u; msg_id=%u)\n", dev_id, msg_id);
        return -EINVAL;
    }

    if (udis_chan_msg_processes[msg_id] == NULL) {
        udis_err("udis_chan_msg_processes[%u] is NULL. (dev_id=%u)\n", msg_id, dev_id);
        return -EINVAL;
    }
    msg_info->head.dev_id = dev_id;
    return udis_chan_msg_processes[msg_id](data, real_out_len);
}

int udis_send_host_vf_uninit_notify(unsigned int udevid)
{
    int ret;
    unsigned int out_len = 0;
    struct udis_msg_info udis_msg_info = {{0}, {0}};

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX)) {
        udis_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_msg_info.head.dev_id = udevid;
    udis_msg_info.head.msg_id = UDIS_CHAN_H2D_UNINIT;
    udis_msg_info.head.valid = (u16)UDIS_MSG_VALID;
    udis_msg_info.head.result = (u16)UDIS_MSG_INVALID_RESULT;
    udis_msg_info.payload[0] = 1;

    ret = devdrv_common_msg_send(udevid, &udis_msg_info, sizeof(udis_msg_info),
            sizeof(udis_msg_info), &out_len, DEVDRV_COMMON_MSG_UDIS);
    if (ret != 0) {
        udis_err("H2D Send host uninit msg to device failed. (devid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    return 0;
}

int udis_send_host_ready_msg_to_device(unsigned int udevid)
{
    int ret;
    unsigned int out_len = 0;
    struct udis_msg_info udis_msg_info = {{0}, {0}};

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX)) {
        udis_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_msg_info.head.dev_id = udevid;
    udis_msg_info.head.msg_id = UDIS_CHAN_H2D_READY;
    udis_msg_info.head.valid = (u16)UDIS_MSG_VALID;
    udis_msg_info.head.result = (u16)UDIS_MSG_INVALID_RESULT;
    udis_msg_info.payload[0] = 1;

    ret = devdrv_common_msg_send(udevid, &udis_msg_info, sizeof(udis_msg_info),
        sizeof(udis_msg_info), &out_len, DEVDRV_COMMON_MSG_UDIS);
    if (ret != 0) {
        udis_err("H2D Send host ready msg to device failed. (devid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    return 0;
}

int udis_common_chan_init(void)
{
    g_udis_common_chan.type = DEVDRV_COMMON_MSG_UDIS;
    g_udis_common_chan.common_msg_recv = udis_rx_common_msg_process;
    g_udis_common_chan.init_notify = NULL;
    return 0;
}

void udis_common_chan_uninit(void)
{
    g_udis_common_chan.type = DEVDRV_COMMON_MSG_TYPE_MAX;
    g_udis_common_chan.common_msg_recv = NULL;
    g_udis_common_chan.init_notify = NULL;
}
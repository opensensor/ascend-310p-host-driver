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
#include <linux/workqueue.h>

#include "ka_task_pub.h"

#include "securec.h"
#include "uda.h"
#include "dbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"
#include "udis_msg.h"

STATIC struct agentdrv_common_msg_client g_udis_common_msg_clent;
STATIC atomic_t g_udis_msg_status[UDIS_DEVICE_UDEVID_MAX] = {0};
STATIC struct udis_msg_notify_work g_udis_send_all_node_work[UDIS_DEVICE_UDEVID_MAX];
STATIC struct workqueue_struct *g_udis_wq;

STATIC bool udis_msg_is_send_ready(unsigned int udevid)
{
    return (atomic_read(&g_udis_msg_status[udevid]) == UDIS_NEED_SEND_ALL_NODE_FLAG);
}

STATIC void udis_msg_schedule_send_work(unsigned int udevid)
{
#ifndef CFG_UDIS_SUPPORT_VF
    if (!uda_is_phy_dev(udevid)) {
        return;
    }
#endif

    if ((atomic_inc_return(&g_udis_msg_status[udevid]) == UDIS_NEED_SEND_ALL_NODE_FLAG) &&
        (g_udis_send_all_node_work[udevid].work.func != NULL)) {
        queue_work(g_udis_wq, &g_udis_send_all_node_work[udevid].work);
    }
}

STATIC void udis_msg_host_dev_uninit(unsigned int udevid)
{
    atomic_set(&g_udis_msg_status[udevid], 0);
}

STATIC int udis_host_ready_notify(void *msg, u32 *ack_len)
{
    unsigned int udevid;
    struct udis_msg_info *msg_info = NULL;
    int *host_is_ready = NULL;

    msg_info = (struct udis_msg_info *)msg;
    udevid = msg_info->head.dev_id;
    host_is_ready = (int *)msg_info->payload;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX || *host_is_ready != 1) {
        udis_err("Invalid parameter. (udevid=%u; host_is_ready=%d)\n", udevid, host_is_ready);
        return -EINVAL;
    }

    udis_msg_schedule_send_work(udevid);

    *ack_len = sizeof(int);
    return 0;
}

STATIC int udis_host_uninit_notify(void *msg, u32 *ack_len)
{
    unsigned int udevid;
    struct udis_msg_info *msg_info = NULL;
    int *host_uninit = NULL;

    msg_info = (struct udis_msg_info *)msg;
    udevid = msg_info->head.dev_id;
    host_uninit = (int *)msg_info->payload;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX || *host_uninit != 1) {
        udis_err("Invalid parameter. (udevid=%u; host_is_ready=%d)\n", udevid, *host_uninit);
        return -EINVAL;
    }

    udis_msg_host_dev_uninit(udevid);

    *ack_len = 0;
    return 0;
}

int udis_send_dma_node_to_host(unsigned int udevid, struct udis_dma_node *dma_node, enum udis_d2h_chan_type udis_chan)
{
    int ret;
    u32 out_len = 0;
    struct udis_msg_info udis_msg_info = {{0}, {0}};

    if (udevid >= UDIS_DEVICE_UDEVID_MAX || dma_node == NULL) {
        udis_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (!udis_msg_is_send_ready(udevid)) {
        udis_debug("udis mgs chan is not ready. (udevid=%u)\n", udevid);
        return 0;
    }

    udis_msg_info.head.dev_id = udevid;
    udis_msg_info.head.msg_id = udis_chan;
    udis_msg_info.head.valid = (u16)UDIS_MSG_VALID;
    udis_msg_info.head.result = (u16)UDIS_MSG_INVALID_RESULT;
    memcpy_s(udis_msg_info.payload, UDIS_MSG_PAYLOAD_LEN, dma_node, sizeof(struct udis_dma_node));

    ret = agentdrv_common_msg_send(udevid, &udis_msg_info, sizeof(udis_msg_info),
        sizeof(udis_msg_info), &out_len, AGENTDRV_COMMON_MSG_UDIS);
    if (ret != 0) {
        udis_warn("D2H Send udis dma_node to host warning. (devid=%u; ret=%d)\n", udevid, ret);
    }

    return ret;
}

void udis_all_dma_node_op_to_host(unsigned int udevid, struct udis_ctrl_block *ucb, enum udis_d2h_chan_type chan_type)
{
    int i, ret;
    struct udis_dma_node *node, *node_tmp = NULL;

    if (ucb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return;
    }

    down_write(&ucb->addr_list_lock);
    for(i = 0; i < UPDATE_TYPE_MAX; i++) {
        list_for_each_entry_safe(node, node_tmp, &ucb->addr_list[i], list) {
            ret = udis_send_dma_node_to_host(udevid, node, chan_type);
            if ((chan_type == UDIS_CHAN_D2H_REGIST) && ret == 0) {
                continue;
            } else {
                list_del(&node->list);
                dbl_kfree(node);
                node = NULL;
            }
        }
    }
    up_write(&ucb->addr_list_lock);

    return;
}

STATIC void udis_send_all_node_work(struct work_struct *work_data)
{
    struct udis_msg_notify_work *send_work = NULL;
    unsigned int udevid;
    struct udis_ctrl_block *udis_cb = NULL;

    send_work = container_of(work_data, struct udis_msg_notify_work, work);
    udevid = send_work->udevid;

    udis_cb_read_lock(udevid);
    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        goto out;
    }

    udis_all_dma_node_op_to_host(udevid, udis_cb, UDIS_CHAN_D2H_REGIST);
    udis_info("udis send all node finish. (udevid=%u)\n", udevid);
out:
    udis_cb_read_unlock(udevid);
    return;
}

void udis_msg_send_work_init(unsigned int udevid)
{
    g_udis_send_all_node_work[udevid].udevid = udevid;
    KA_TASK_INIT_WORK(&g_udis_send_all_node_work[udevid].work, udis_send_all_node_work);
}

void udis_msg_send_work_uninit(unsigned int udevid)
{
    struct udis_ctrl_block *udis_cb = NULL;

    udis_cb_read_lock(udevid);
    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        goto out;
    }

    udis_all_dma_node_op_to_host(udevid, udis_cb, UDIS_CHAN_D2H_UNREGIST);
out:
    udis_cb_read_unlock(udevid);
    (void)cancel_work_sync(&g_udis_send_all_node_work[udevid].work);
    atomic_set(&g_udis_msg_status[udevid], 0);
}

STATIC int (*udis_chan_msg_processes[UDIS_CHAN_H2D_MAX_ID])(void *msg, u32 *ack_len) = {
    [UDIS_CHAN_H2D_READY] = udis_host_ready_notify,
    [UDIS_CHAN_H2D_UNINIT] = udis_host_uninit_notify,
};

int udis_rx_common_msg_process(unsigned int udevid, void *data, unsigned int in_data_len, unsigned int out_data_len,
    unsigned int *real_out_len)
{
    unsigned int msg_id;
    struct udis_msg_info *msg_info = NULL;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (data == NULL) || (real_out_len == NULL) ||
        (in_data_len < sizeof(struct udis_msg_info))) {
        udis_err("Invalid param. (devid=%u; data_is_NULL=%d; real_out_len_is_NULL=%d; in_data_len=%u)\n",
            udevid, data == NULL, real_out_len == NULL, in_data_len);
        return -EINVAL;
    }
    msg_info = (struct udis_msg_info *)data;
    msg_id = msg_info->head.msg_id;
    if (msg_id >= UDIS_CHAN_H2D_MAX_ID) {
        udis_err("Invalid msg_id. (dev_id=%u; msg_id=%u)\n", udevid, msg_id);
        return -EINVAL;
    }

    if (udis_chan_msg_processes[msg_id] == NULL) {
        udis_err("udis_chan_msg_processes[%u] is NULL. (dev_id=%u)\n", msg_id, udevid);
        return -EINVAL;
    }
    msg_info->head.dev_id = udevid;
    return udis_chan_msg_processes[msg_id](data, real_out_len);
}

STATIC void udis_common_msg_notify(u32 udevid)
{
    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("common msg notify fail, udevid = %u\n", udevid);
        return;
    }
    udis_msg_schedule_send_work(udevid);
    return;
}

int udis_common_chan_init(void)
{
    int ret;

    g_udis_wq = ka_task_create_workqueue("udis_msg_send");
    if (g_udis_wq == NULL) {
        udis_err("create udis_msg_send wq failed.\n");
        return -ENOMEM;
    }

    g_udis_common_msg_clent.type = AGENTDRV_COMMON_MSG_UDIS;
    g_udis_common_msg_clent.common_msg_recv = udis_rx_common_msg_process;
    g_udis_common_msg_clent.init_notify = udis_common_msg_notify;

    ret = agentdrv_register_common_msg_client(&g_udis_common_msg_clent);
    if (ret != 0) {
        udis_err("udis register agentdrv common msg chan failed.\n");
        goto register_udis_channel_failed;
    }

    return 0;

register_udis_channel_failed:
    g_udis_common_msg_clent.type = AGENTDRV_COMMON_MSG_TYPE_MAX;
    g_udis_common_msg_clent.common_msg_recv = NULL;
    g_udis_common_msg_clent.init_notify = NULL;

    return ret;
}

void udis_common_chan_uninit(void)
{
    agentdrv_unregister_common_msg_client(&g_udis_common_msg_clent);

    g_udis_common_msg_clent.type = AGENTDRV_COMMON_MSG_TYPE_MAX;
    g_udis_common_msg_clent.common_msg_recv = NULL;
    g_udis_common_msg_clent.init_notify = NULL;

    flush_workqueue(g_udis_wq);
    destroy_workqueue(g_udis_wq);
}

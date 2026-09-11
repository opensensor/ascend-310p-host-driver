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

#include "virtmngagent_msg_admin.h"
#include "virtmngagent_msg.h"
#include "virtmngagent_vpc.h"
#include "virtmngagent_msg_common.h"
#include "virtmng_msg_admin.h"
#include "virtmng_public_def.h"
#include "virtmng_resource.h"

#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

STATIC int vmnga_admin_para_check(const struct vmng_msg_dev *msg_dev,
    const struct vmng_msg_chan_rx_proc_info *proc_info)
{
    if (msg_dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (proc_info == NULL) {
        vmng_err("Input parameter is error. (dev_id=%d)\n", msg_dev->dev_id);
        return -EINVAL;
    }
    if (proc_info->data == NULL) {
        vmng_err("Input parameter is error. (dev_id=%d)\n", msg_dev->dev_id);
        return -EINVAL;
    }
    return 0;
}

STATIC void vmnga_parse_irq_from_cmd(struct vmng_create_cluster_cmd *cmd, struct vmng_msg_chan_irqs *int_irq_ary)
{
    int_irq_ary->rx_recv_irq = (u32 *)(&(cmd->int_irq[0]));
    int_irq_ary->rx_resp_irq = int_irq_ary->rx_recv_irq + cmd->res.tx_num;
    int_irq_ary->tx_send_irq = int_irq_ary->rx_recv_irq + (u64)cmd->res.tx_num * VMNG_MSG_SQ_TXRX;
    int_irq_ary->tx_finish_irq = int_irq_ary->rx_recv_irq + (u64)cmd->res.tx_num * VMNG_MSG_SQ_TXRX + cmd->res.rx_num;
}

STATIC int vmnga_fill_msg_cluster_res(struct vmng_msg_chan_res *res, u32 tx_base, u32 tx_num, u32 rx_base, u32 rx_num)
{
    u32 tx_max;
    u32 rx_max;

    tx_max = tx_base + tx_num;
    rx_max = rx_base + rx_num;
    if ((tx_base >= VMNG_MSG_CHAN_NUM_MAX) || (tx_num >= VMNG_MSG_CHAN_NUM_MAX) || (rx_base >= VMNG_MSG_CHAN_NUM_MAX) ||
        (rx_num >= VMNG_MSG_CHAN_NUM_MAX) || (tx_max > VMNG_MSG_CHAN_NUM_MAX) || (rx_max > VMNG_MSG_CHAN_NUM_MAX)) {
        vmng_err("Prarmeter check failed. (tx_base=%u; tx_num=%u; rx_base=%u; rx_num=%u)\n",
                 tx_base, tx_num, rx_base, rx_num);
        return -EINVAL;
    }
    res->tx_base = rx_base;
    res->tx_num = rx_num;
    res->rx_base = tx_base;
    res->rx_num = tx_num;
    return 0;
}

STATIC int vmnga_assign_msg_func(enum vmng_msg_chan_type chan_type, struct vmng_msg_proc *msg_proc)
{
    if (chan_type == VMNG_MSG_CHAN_TYPE_COMMON) {
        msg_proc->rx_recv_proc = vmnga_msg_cluster_recv_common;
        msg_proc->tx_finish_proc = NULL;
    } else if (vmng_is_vpc_chan(chan_type) == true) {
        msg_proc->rx_recv_proc = vmnga_msg_cluster_recv_vpc;
        msg_proc->tx_finish_proc = NULL;
    } else if (vmng_is_blk_chan(chan_type) == true) {
        msg_proc->rx_recv_proc = vmnga_msg_cluster_recv_vpc;
        msg_proc->tx_finish_proc = vmng_msg_tx_finish_task;
    } else {
        vmng_err("Unknown channel type. (chan_type=%u)\n", chan_type);
        return -EINVAL;
    }
    return 0;
}

STATIC int vmnga_admin_rx_alloc_msg_cluster(struct vmng_msg_dev *msg_dev, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_create_cluster_cmd *cmd = NULL;
    struct vmng_create_cluster_reply *reply = NULL;
    struct vmng_msg_chan_res res;
    enum vmng_msg_chan_type chan_type;
    struct vmng_msg_chan_irqs int_irq_ary;
    struct vmng_msg_proc msg_proc;
    int ret;

    /* 1. parameter check  */
    ret = vmnga_admin_para_check(msg_dev, proc_info);
    if (ret != 0) {
        vmng_err("Call vmnga_admin_para_check failed.\n");
        return ret;
    }

    cmd = (struct vmng_create_cluster_cmd *)proc_info->data;

    /* 2. parse res from cmd */
    ret = vmnga_fill_msg_cluster_res(&res, cmd->res.tx_base, cmd->res.tx_num, cmd->res.rx_base, cmd->res.rx_num);
    if (ret != 0) {
        vmng_err("Fill msg_cluster failed. (dev_id=%u; chan_type=%u)\n", msg_dev->dev_id, cmd->chan_type);
        return ret;
    }

    /* 3. parase chan_type from cmd */
    chan_type = cmd->chan_type;
    if (chan_type >= VMNG_MSG_CHAN_TYPE_MAX) {
        vmng_err("chan_type is invalid. (dev_id=%u; chan_type=%u)\n", msg_dev->dev_id, chan_type);
        return -EINVAL;
    }

    /* 4. parase irqs from cmd */
    vmnga_parse_irq_from_cmd(cmd, &int_irq_ary);

    /* 5. decide which recv to register */
    ret = vmnga_assign_msg_func(chan_type, &msg_proc);
    if (ret != 0) {
        vmng_err("chan_type is invalid. (dev_id=%u; chan_type=%u)\n", msg_dev->dev_id, chan_type);
        return ret;
    }

    /* 6. alloc local msg cluster */
    ret = vmng_alloc_local_msg_cluster(msg_dev, chan_type, &res, &int_irq_ary, &msg_proc);
    if (ret != 0) {
        vmng_err("Alloc cluster failed. (dev_id=%u; chan_type=%u; ret=%d)\n", msg_dev->dev_id, chan_type, ret);
        return ret;
    }
    msg_dev->msg_cluster[chan_type].status = VMNG_MSG_CLUSTER_STATUS_ENABLE;

    vmng_info("Alloc cluster success. (dev_id=%u; chan_type=%u; tx_base=%u; tx_num=%u; rx_base=%u; rx_num=%u)\n",
        msg_dev->dev_id, cmd->chan_type, cmd->res.tx_base, cmd->res.tx_num, cmd->res.rx_base, cmd->res.rx_num);

    /* 7. reply to host */
    reply = (struct vmng_create_cluster_reply *)proc_info->data;
    reply->remote_alloc_finish = VMNG_CREATE_CLUSTER_FINISH;
    *(proc_info->real_out_len) = sizeof(struct vmng_create_cluster_reply);

    return ret;
}

static vmnga_admin_func g_vmnga_msg_admin_func_ops[] = {
    vmnga_admin_rx_alloc_msg_cluster,
    NULL
};
#define VMNG_ADMIN_PROC_OPCODE_MAX (sizeof(g_vmnga_msg_admin_func_ops) / sizeof(void *))

int vmnga_register_admin_rx_func(int opcode, vmnga_admin_func admin_func)
{
    if (opcode >= VMNG_ADMIN_PROC_OPCODE_MAX) {
        vmng_err("opcode is invalid. (opcode=%u)\n", opcode);
        return -EINVAL;
    }
    if (g_vmnga_msg_admin_func_ops[opcode] != NULL) {
        vmng_err("opcode has registered. (opcode=%u)\n", opcode);
        return -EINVAL;
    }
    g_vmnga_msg_admin_func_ops[opcode] = admin_func;
    return 0;
}
KA_EXPORT_SYMBOL(vmnga_register_admin_rx_func);

void vmnga_unregister_admin_rx_func(int opcode)
{
    if (opcode >= VMNG_ADMIN_PROC_OPCODE_MAX) {
        vmng_err("opcode is invalid. (opcode=%u)\n", opcode);
        return;
    }
    g_vmnga_msg_admin_func_ops[opcode] = NULL;
}
KA_EXPORT_SYMBOL(vmnga_unregister_admin_rx_func);

STATIC int vmnga_admin_rx_msg_proc(void *msg_chan, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_msg_chan_rx *admin_msg_chan = (struct vmng_msg_chan_rx *)msg_chan;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)admin_msg_chan->msg_dev;
    u32 opcode;
    int ret;

    if (proc_info == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", msg_dev->dev_id);
        return -EINVAL;
    }
    if (proc_info->data == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", msg_dev->dev_id);
        return -EINVAL;
    }

    opcode = proc_info->opcode_d2;
    if (opcode >= VMNG_ADMIN_PROC_OPCODE_MAX) {
        vmng_err("opcode is invalid. (dev_id=%u; opcode=%u)\n", msg_dev->dev_id, opcode);
        return -ENOSYS;
    }

    if (g_vmnga_msg_admin_func_ops[opcode] == NULL) {
        vmng_err("Msg admin proc function is NULL. (dev_id=%u;opcode=%d)\n", msg_dev->dev_id, opcode);
        return -EINVAL;
    }

    ret = g_vmnga_msg_admin_func_ops[opcode](msg_dev, proc_info);
    if (ret != 0) {
        vmng_err("Admin func ops error. (dev_id=%u; ret=%d)\n", msg_dev->dev_id, ret);
        return -EINVAL;
    }

    return ret;
}

int vmnga_init_msg_admin(struct vmng_msg_dev *msg_dev)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_chan_rx *msg_chan_rx = NULL;
    struct vmng_msg_chan_tx *msg_chan_tx = NULL;
    int ret;

    vmng_info("Init message admin. (dev_id=%u)\n", msg_dev->dev_id);

    msg_cluster = &(msg_dev->msg_cluster[VMNG_MSG_CHAN_TYPE_ADMIN]);
    ka_task_mutex_init(&msg_cluster->mutex);

    msg_dev->admin_tx = &(msg_dev->msg_chan_tx[VMNG_MSG_CHAN_TYPE_ADMIN]);
    msg_chan_tx = msg_dev->admin_tx;
    msg_chan_tx->tx_send_irq = msg_dev->db_irq_base + VMNG_DB_MSG_ADMIN;
    msg_chan_tx->tx_finish_irq = 0;
    msg_chan_tx->send_irq_to_remote = msg_dev->ops.send_irq_to_remote;
    ka_task_mutex_init(&msg_chan_tx->mutex);

    msg_dev->admin_rx = &(msg_dev->msg_chan_rx[VMNG_MSG_CHAN_TYPE_ADMIN]);
    msg_chan_rx = msg_dev->admin_rx;
    msg_chan_rx->rx_recv_irq = msg_dev->msix_irq_base + VMNG_MSIX_MSG_ADMIN;
    msg_chan_rx->rx_proc = vmnga_admin_rx_msg_proc;
    msg_chan_rx->rx_wq = ka_task_create_singlethread_workqueue("vpc_admin_msg_chan_proc");
    if (msg_chan_rx->rx_wq == NULL) {
        vmng_err("Create workqueue failed. (dev_id=%u)\n", msg_dev->dev_id);
        return -EINVAL;
    }
    KA_TASK_INIT_WORK(&msg_chan_rx->rx_work, vmng_msg_rx_msg_task);

    ret = vmnga_rx_irq_init(msg_chan_rx);
    if (ret != 0) {
        ka_task_destroy_workqueue(msg_chan_rx->rx_wq);
        msg_chan_rx->rx_wq = NULL;
        vmng_err("Call vmnga_rx_irq_init failed. (dev_id=%u)\n", msg_dev->dev_id);
        return -EINVAL;
    }

    msg_chan_tx->status = VMNG_MSG_CHAN_STATUS_IDLE;
    msg_chan_rx->status = VMNG_MSG_CHAN_STATUS_IDLE;

    vmng_info("Admin init OK. (dev_id=%u; tx_int=%u; rx_int=%u) \n", msg_dev->dev_id, msg_chan_tx->tx_send_irq,
        msg_chan_rx->rx_recv_irq);
    return 0;
}

void vmnga_uninit_vpc_msg_admin(struct vmng_msg_dev *msg_dev)
{
    struct vmng_msg_chan_rx *msg_chan_rx = msg_dev->admin_rx;
    struct vmng_msg_chan_tx *msg_chan_tx = msg_dev->admin_tx;
    int ret;

    ret = vmnga_rx_irq_uninit(msg_chan_rx);
    if (ret != 0) {
        vmng_err("Call vmnga_rx_irq_uninit failed. (dev_id=%u)\n", msg_dev->dev_id);
    }

    msg_chan_rx->status = VMNG_MSG_CHAN_STATUS_DISABLE;
    msg_chan_tx->status = VMNG_MSG_CHAN_STATUS_DISABLE;
    if (msg_chan_rx->rx_wq != NULL) {
        ka_task_destroy_workqueue(msg_chan_rx->rx_wq);
        msg_chan_rx->rx_wq = NULL;
    }
}

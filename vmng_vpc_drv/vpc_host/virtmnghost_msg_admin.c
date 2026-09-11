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

#include "virtmnghost_msg_admin.h"
#include "virtmng_msg_admin.h"
#include "virtmng_interface.h"
#include "virtmng_msg_common.h"
#include "virtmng_public_def.h"

#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

static vmngh_admin_func g_vmngh_msg_admin_func_ops[] = {
    NULL,
};
#define VMNG_ADMIN_PROC_OPCODE_MAX (sizeof(g_vmngh_msg_admin_func_ops) / sizeof(void *))

int vmngh_register_admin_rx_func(int opcode, vmngh_admin_func admin_func)
{
    if (opcode >= VMNG_ADMIN_PROC_OPCODE_MAX) {
        vmng_err("opcode is invalid. (opcode=%u)\n", opcode);
        return -EINVAL;
    }
    if (g_vmngh_msg_admin_func_ops[opcode] != NULL) {
        vmng_err("opcode has registered. (opcode=%u)\n", opcode);
        return -EINVAL;
    }
    g_vmngh_msg_admin_func_ops[opcode] = admin_func;
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_register_admin_rx_func);

void vmngh_unregister_admin_rx_func(int opcode)
{
    if (opcode >= VMNG_ADMIN_PROC_OPCODE_MAX) {
        vmng_err("opcode is invalid. (opcode=%u)\n", opcode);
        return;
    }
    g_vmngh_msg_admin_func_ops[opcode] = NULL;
}
KA_EXPORT_SYMBOL(vmngh_unregister_admin_rx_func);

STATIC int vmngh_admin_rx_msg_proc(void *msg_chan, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_msg_chan_rx *admin_msg_chan = (struct vmng_msg_chan_rx *)msg_chan;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)admin_msg_chan->msg_dev;
    u32 opcode;
    int ret;

    if (proc_info == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (proc_info->data == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }

    opcode = proc_info->opcode_d2;
    if (opcode >= VMNG_ADMIN_PROC_OPCODE_MAX) {
        vmng_err("opcode is invalid. (opcode=%u)\n", opcode);
        return -ENOSYS;
    }

    if (g_vmngh_msg_admin_func_ops[opcode] == NULL) {
        vmng_err("Got an admin opcode, but no process function. (devid=%u; fid=%u; opcode=%u)\n",
            msg_dev->dev_id, msg_dev->fid, opcode);
        return -EINVAL;
    }
    ret = g_vmngh_msg_admin_func_ops[opcode](msg_dev, proc_info);
    if (ret != 0) {
        vmng_err("Admin func ops error. (devid=%u; fid=%u; ret=%d)\n", msg_dev->dev_id, msg_dev->fid, ret);
        return -EINVAL;
    }

    return ret;
}

void vmngh_init_admin_msg(struct vmng_msg_dev *msg_dev)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_chan_rx *msg_chan_rx = NULL;
    struct vmng_msg_chan_tx *msg_chan_tx = NULL;

    /* admin cluster */
    msg_cluster = &(msg_dev->msg_cluster[VMNG_MSG_CHAN_TYPE_ADMIN]);
    ka_task_mutex_init(&msg_cluster->mutex);

    /* tx admin */
    msg_dev->admin_tx = &(msg_dev->msg_chan_tx[VMNG_MSG_CHAN_TYPE_ADMIN]);
    msg_chan_tx = msg_dev->admin_tx;
    msg_chan_tx->tx_send_irq = msg_dev->msix_irq_base + VMNG_MSIX_MSG_ADMIN;
    msg_chan_tx->tx_finish_irq = 0;
    msg_chan_tx->send_irq_to_remote = msg_dev->ops.send_irq_to_remote;
    ka_task_mutex_init(&msg_chan_tx->mutex);
    msg_chan_tx->status = VMNG_MSG_CHAN_STATUS_IDLE;

    /* rx admin */
    msg_dev->admin_rx = &(msg_dev->msg_chan_rx[VMNG_MSG_CHAN_TYPE_ADMIN]);
    msg_chan_rx = msg_dev->admin_rx;
    msg_chan_rx->rx_recv_irq = msg_dev->db_irq_base + VMNG_DB_MSG_ADMIN;
    msg_chan_rx->rx_proc = vmngh_admin_rx_msg_proc;
    msg_chan_rx->rx_wq = ka_task_create_singlethread_workqueue("vpc_admin_msg_chan_proc");
    if (msg_chan_rx->rx_wq == NULL) {
        vmng_err("Creat workqueue failed. (dev_id=%u)\n", msg_dev->dev_id);
        return;
    }
    KA_TASK_INIT_WORK(&msg_chan_rx->rx_work, vmng_msg_rx_msg_task);
    msg_chan_rx->status = VMNG_MSG_CHAN_STATUS_IDLE;

    /* mount to msg_dev */
    msg_dev->admin_db_id = msg_chan_rx->rx_recv_irq;

    vmng_info("Admin init OK. (dev_id=%u; fid=%u; tx_irq=%u; rx_irq=%u)\n", msg_dev->dev_id, msg_dev->fid,
        msg_chan_tx->tx_send_irq, msg_chan_rx->rx_recv_irq);
}

void vmngh_uninit_admin_msg(struct vmng_msg_dev *msg_dev)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_chan_rx *msg_chan_rx = NULL;
    struct vmng_msg_chan_tx *msg_chan_tx = NULL;

    /* admin cluster */
    msg_cluster = &(msg_dev->msg_cluster[VMNG_MSG_CHAN_TYPE_ADMIN]);

    /* tx admin */
    msg_dev->admin_tx = &(msg_dev->msg_chan_tx[VMNG_MSG_CHAN_TYPE_ADMIN]);
    msg_chan_tx = msg_dev->admin_tx;
    msg_chan_tx->status = VMNG_MSG_CHAN_STATUS_DISABLE;

    /* rx admin */
    msg_dev->admin_rx = &(msg_dev->msg_chan_rx[VMNG_MSG_CHAN_TYPE_ADMIN]);
    msg_chan_rx = msg_dev->admin_rx;
    msg_chan_rx->status = VMNG_MSG_CHAN_STATUS_DISABLE;

    if (msg_chan_rx->rx_wq != NULL) {
        ka_task_destroy_workqueue(msg_chan_rx->rx_wq);
        msg_chan_rx->rx_wq = NULL;
    }

    vmng_info("Admin uninit OK. (dev_id=%u; fid=%u)\n", msg_dev->dev_id, msg_dev->fid);
}

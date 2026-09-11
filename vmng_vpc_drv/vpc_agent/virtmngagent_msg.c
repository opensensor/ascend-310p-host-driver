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

#include "virtmngagent_msg.h"
#include "virtmng_msg_common.h"
#include "virtmngagent_vpc_unit.h"
#include "virtmngagent_msg_admin.h"
#include "virtmng_msg_pub.h"
#include "virtmng_msg_admin.h"
#include "virtmng_public_def.h"
#include "virtmng_interface.h"
#include "virtmng_resource.h"
#include "vmng_mem_alloc_interface.h"

#include "ka_kernel_def_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"

struct vmng_msg_dev_head {
    ka_list_head_t msg_dev_head;
    ka_mutex_t mutex;
};

struct vmng_msg_dev_head g_msg_dev_head;

struct vmng_msg_dev *vmnga_get_msg_dev_by_id(u32 dev_id)
{
    struct vmng_msg_dev *pos;

    ka_list_for_each_entry(pos, &g_msg_dev_head.msg_dev_head, list) {
        if (pos->dev_id == dev_id) {
            return pos;
        }
    }
    return NULL;
}

STATIC void vmnga_msg_dev_list_free(void)
{
    struct vmng_msg_dev *pos;
    struct vmng_msg_dev *n;

    ka_list_for_each_entry_safe(pos, n, &g_msg_dev_head.msg_dev_head, list) {
        ka_list_del(&pos->list);
        vmng_kfree(pos);
        pos = NULL;
    }
}

STATIC void vmnga_send_int_to_remote(void *msg_dev_in, u32 vector)
{
    struct vmng_msg_dev *msg_dev = msg_dev_in;

    vmnga_set_doorbell(msg_dev->db_base, vector, 0x1);
}

STATIC ka_irqreturn_t vmnga_tx_finish_msix_hander(int irq, void *data)
{
    struct vmng_msg_chan_tx *msg_chan = (struct vmng_msg_chan_tx *)data;
    struct vmng_msg_cluster *msg_cluster = NULL;

    msg_cluster = msg_chan->msg_cluster;
    if (msg_cluster->msg_proc.tx_finish_proc == NULL) {
        vmng_err("tx_finish_proc is NULL. (dev_id=%u; chan_type=%u)\n", msg_cluster->dev_id, msg_cluster->chan_type);
        return KA_IRQ_NONE;
    }

    msg_cluster->msg_proc.tx_finish_proc((unsigned long)(uintptr_t)msg_chan);

    return KA_IRQ_HANDLED;
}

STATIC int vmnga_tx_irq_init(struct vmng_msg_chan_tx *msg_chan)
{
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    int ret;

    if (msg_chan->tx_finish_irq == 0) { /* no need to register tx finish irq */
        return 0;
    }
    ret = vmnga_register_vpc_irq_func(msg_dev->unit, msg_chan->tx_finish_irq, vmnga_tx_finish_msix_hander,
        (void *)msg_chan, "vmnga_tx_finish");
    if (ret != 0) {
        vmng_err("Call vmnga_register_vpc_irq_func failed. (dev_id=%u; tx_finish_irq=%u; ret=%d)\n",
                 msg_dev->dev_id, msg_chan->tx_finish_irq, ret);
        return ret;
    }

    return 0;
}

STATIC int vmnga_tx_irq_uninit(struct vmng_msg_chan_tx *msg_chan)
{
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    int ret;

    if (msg_chan->tx_finish_irq == 0) {
        return 0;
    }
    ret = vmnga_unregister_vpc_irq_func(msg_dev->unit, msg_chan->tx_finish_irq, (void *)msg_chan);
    if (ret != 0) {
        vmng_err("Call vmnga_unregister_vpc_irq_func failed. (dev_id=%u; tx_finish_irq=%u; ret=%d)\n",
                 msg_dev->dev_id, msg_chan->tx_finish_irq, ret);
        return ret;
    }
    vmng_event("Tx irq unregister. (dev_id=%u; tx_irq=%u)\n", msg_dev->dev_id, msg_chan->tx_finish_irq);
    return 0;
}

STATIC ka_irqreturn_t vmnga_msg_msix_hander(int irq, void *data)
{
    struct vmng_msg_chan_rx *msg_chan = (struct vmng_msg_chan_rx *)data;

    vmng_msg_push_rx_queue_work(msg_chan);

    return KA_IRQ_HANDLED;
}

int vmnga_rx_irq_init(struct vmng_msg_chan_rx *msg_chan)
{
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    int ret;

    if (msg_chan->rx_recv_irq == 0) {
        return 0;
    }

    ret = vmnga_register_vpc_irq_func(msg_dev->unit, msg_chan->rx_recv_irq, vmnga_msg_msix_hander, (void *)msg_chan,
        "vmnga_rx_recv");
    if (ret != 0) {
        vmng_err("Call vmnga_register_vpc_irq_func failed. (dev_id=%u)\n", msg_dev->dev_id);
        return ret;
    }
    return 0;
}

int vmnga_rx_irq_uninit(struct vmng_msg_chan_rx *msg_chan)
{
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    int ret;

    if (msg_chan->rx_recv_irq == 0) {
        return 0;
    }

    vmng_event("Irq unregister. (dev_id=%u; irq=%u)\n", msg_dev->dev_id, msg_chan->rx_recv_irq);
    ret = vmnga_unregister_vpc_irq_func(msg_dev->unit, msg_chan->rx_recv_irq, (void *)msg_chan);
    if (ret != 0) {
        vmng_err("Call vmnga_unregister_vpc_irq_func failed. (dev_id=%u)\n", msg_dev->dev_id);
        return ret;
    }
    return 0;
}

STATIC struct vmng_msg_dev *vmnga_msg_dev_alloc(struct vmnga_vpc_unit *unit)
{
    struct vmng_msg_dev *msg_dev = NULL;
    int ret = 0;

    vmng_info("Get msg_dev size. (dev_id=%u; size=%lu)\n", unit->dev_id, sizeof(struct vmng_msg_dev));
    msg_dev = vmng_kzalloc(sizeof(struct vmng_msg_dev), KA_GFP_KERNEL);
    if (msg_dev == NULL) {
        vmng_err("Call kzalloc failed. (dev_id=%u; size=%lu)\n", unit->dev_id, sizeof(struct vmng_msg_dev));
        return NULL;
    }
    unit->msg_dev = msg_dev;
    msg_dev->unit = unit;
    msg_dev->dev_id = unit->dev_id;
    msg_dev->fid = 0;
    msg_dev->chan_num = VMNG_MSG_CHAN_NUM_MAX;
    msg_dev->db_base = unit->db_base;
    msg_dev->db_irq_base = VMNG_DB_BASE_MSG;
    msg_dev->mem_base = unit->msg_base;
    msg_dev->msix_irq_base = VMNG_MSIX_BASE_MSG;
    msg_dev->status = VMNG_MSG_DEV_ALIVE;

    msg_dev->ops.send_irq_to_remote = vmnga_send_int_to_remote;
    msg_dev->ops.tx_irq_init = vmnga_tx_irq_init;
    msg_dev->ops.tx_irq_uninit = vmnga_tx_irq_uninit;
    msg_dev->ops.rx_irq_init = vmnga_rx_irq_init;
    msg_dev->ops.rx_irq_uninit = vmnga_rx_irq_uninit;

    msg_dev->work_queue = ka_task_create_workqueue("vmnga_wq");
    if (msg_dev->work_queue == NULL) {
        vmng_err("Create msg work_queue failed. (dev_id=%u)\n", unit->dev_id);
        vmng_kfree(msg_dev);
        msg_dev = NULL;
        return NULL;
    }
    ret = vmng_msg_chan_init(VMNG_AGENT_SIDE, msg_dev);
    if (ret != 0) {
        vmng_err("Call vmng_msg_chan_init failed. (dev_id=%u; ret=%d)\n", unit->dev_id, ret);
        ka_task_destroy_workqueue(msg_dev->work_queue);
        vmng_kfree(msg_dev);
        msg_dev = NULL;
        return NULL;
    }
    ka_task_mutex_lock(&g_msg_dev_head.mutex);
    ka_list_add(&msg_dev->list, &g_msg_dev_head.msg_dev_head);
    ka_task_mutex_unlock(&g_msg_dev_head.mutex);

    return msg_dev;
}

int vmnga_vpc_msg_init(void *unit_in)
{
    struct vmnga_vpc_unit *unit = (struct vmnga_vpc_unit *)unit_in;
    struct vmng_msg_dev *msg_dev = NULL;
    int ret;

    if (unit_in == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    /* msg dev */
    msg_dev = vmnga_msg_dev_alloc(unit);
    if (msg_dev == NULL) {
        vmng_err("Call vmnga_msg_dev_alloc failed. (dev_id=%u)\n", unit->dev_id);
        return -EINVAL;
    }

    /* Admin */
    ret = vmnga_init_msg_admin(msg_dev);
    if (ret != 0) {
        vmng_err("Call vmnga_init_msg_admin failed. (dev_id=%u; ret=%d)\n", unit->dev_id, ret);
        vmng_free_msg_dev(msg_dev);
    }
    return ret;
}
KA_EXPORT_SYMBOL(vmnga_vpc_msg_init);

void vmnga_uninit_vpc_msg(struct vmng_msg_dev *msg_dev)
{
    vmnga_uninit_vpc_msg_admin(msg_dev);
    vmng_free_msg_dev(msg_dev);
}
KA_EXPORT_SYMBOL(vmnga_uninit_vpc_msg);

STATIC int __init vmnga_vpc_init_module(void)
{
    vmng_info("Init vmnga vpc module finish.\n");
    KA_INIT_LIST_HEAD(&g_msg_dev_head.msg_dev_head);
    ka_task_mutex_init(&g_msg_dev_head.mutex);
    return 0;
}
ka_module_init(vmnga_vpc_init_module);

STATIC void __exit vmnga_vpc_exit_module(void)
{
    vmng_info("Exit vmnga vpc module finish.\n");
    vmnga_msg_dev_list_free();
}
ka_module_exit(vmnga_vpc_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("virt vpc agent driver");
KA_MODULE_LICENSE("GPL");

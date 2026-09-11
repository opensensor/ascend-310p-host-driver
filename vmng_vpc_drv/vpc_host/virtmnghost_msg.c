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

#include "hw_vdavinci.h"
#include "vpc_soc_adapt.h"
#include "virtmng_msg_pub.h"
#include "virtmng_public_def.h"
#include "virtmnghost_vpc_unit.h"
#include "virtmnghost_msg_admin.h"
#include "virtmng_msg_admin.h"
#include "vmng_mem_alloc_interface.h"
#include "virtmnghost_msg.h"

#include "ka_list_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

struct vmng_msg_dev_head {
    ka_list_head_t msg_dev_head;
    ka_mutex_t mutex;
};

struct vmng_msg_dev_head g_msg_dev_head;

struct vmng_msg_dev *vmngh_get_msg_dev_by_id(u32 dev_id, u32 fid)
{
    struct vmng_msg_dev *pos;

    ka_list_for_each_entry(pos, &g_msg_dev_head.msg_dev_head, list) {
        if (pos->dev_id == dev_id && pos->fid == fid) {
            return pos;
        }
    }
    return NULL;
}

STATIC void vmngh_msg_dev_list_free(void)
{
    struct vmng_msg_dev *pos;
    struct vmng_msg_dev *n;

    ka_list_for_each_entry_safe(pos, n, &g_msg_dev_head.msg_dev_head, list) {
        ka_list_del(&pos->list);
        vmng_kfree(pos);
        pos = NULL;
    }
}

STATIC int vmngh_msg_get_cluster_from_db(struct vmng_msg_dev *msg_dev, u32 db_vector)
{
    u32 i;
    u32 db_id_beg;
    u32 db_id_end;

    if (db_vector == msg_dev->admin_db_id) {
        return VMNG_MSG_CHAN_TYPE_ADMIN;
    }

    for (i = VMNG_MSG_CHAN_TYPE_COMMON; i < VMNG_MSG_CHAN_TYPE_MAX; i++) {
        if (msg_dev->msg_cluster[i].status == 0) {
            continue;
        }
        db_id_beg = msg_dev->msg_cluster[i].msg_chan_rx_beg->rx_recv_irq;
        db_id_end = db_id_beg + msg_dev->msg_cluster[i].res.rx_num - 1;
        if ((db_vector >= db_id_beg) && (db_vector <= db_id_end)) {
            return (int)i;
        }
    }

    vmng_err("Get db_vector. (dev_id=%u; fid=%u; db=%u)\n", msg_dev->dev_id, msg_dev->fid, db_vector);
    for (i = VMNG_MSG_CHAN_TYPE_COMMON; i < VMNG_MSG_CHAN_TYPE_MAX; i++) {
        if (msg_dev->msg_cluster[i].status == 0) {
            continue;
        }
        db_id_beg = msg_dev->msg_cluster[i].msg_chan_rx_beg->rx_recv_irq;
        db_id_end = db_id_beg + msg_dev->msg_cluster[i].res.rx_num - 1;
        vmng_err("Get db_id. (type=%u; status=%u; beg=%u; end=%u)\n",
                 i, msg_dev->msg_cluster[i].status, db_id_beg, db_id_end);
    }
    return -EINVAL;
}

STATIC struct vmng_msg_chan_rx *vmngh_msg_get_chan_from_db(struct vmng_msg_dev *msg_dev, u32 db_vector)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_chan_rx *msg_chan = NULL;
    int cluster_id;
    int msg_chan_id;

    if (db_vector == msg_dev->admin_db_id) {
        msg_chan = msg_dev->admin_rx;
    } else {
        cluster_id = vmngh_msg_get_cluster_from_db(msg_dev, db_vector);
        if (cluster_id < 0) {
            vmng_err("Get cluster from db failed. (dev_id=%u; fid=%u; cluster=%d)\n",
                     cluster_id, msg_dev->dev_id, msg_dev->fid);
            return NULL;
        }
        msg_cluster = &(msg_dev->msg_cluster[cluster_id]);
        msg_chan_id = (int)(db_vector - msg_cluster->msg_chan_rx_beg->rx_recv_irq);
        msg_chan = msg_cluster->msg_chan_rx_beg + msg_chan_id;
    }

    return msg_chan;
}

int vmngh_msg_db_hanlder(struct vmng_msg_dev *msg_dev, u32 db_vector)
{
    struct vmng_msg_chan_rx *msg_chan = NULL;

    if (msg_dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    msg_chan = vmngh_msg_get_chan_from_db(msg_dev, db_vector);
    if (msg_chan == NULL) {
        vmng_err("Get chan from db failed. (dev_id=%u; fid=%u; db_vector=%u)\n",
                 msg_dev->dev_id, msg_dev->fid, db_vector);
        return -EINVAL;
    }
    vmng_msg_push_rx_queue_work(msg_chan);
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_msg_db_hanlder);

STATIC struct vmng_msg_chan_tx *vmngh_get_tx_chan_by_tx_finsih_db(struct vmng_msg_dev *msg_dev, u32 tx_finish_irq)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_chan_tx *msg_chan = NULL;
    enum vmng_msg_chan_type type;
    u32 i;

    /* search all block chan to get msg chan */
    for (type = VMNG_MSG_CHAN_TYPE_VPC + 1; type <= VMNG_MSG_CHAN_TYPE_BLOCK; type++) {
        msg_cluster = &(msg_dev->msg_cluster[type]);
        if (msg_cluster->status == VMNG_MSG_CLUSTER_STATUS_DISABLE) {
            continue;
        }
        for (i = 0; i < msg_cluster->res.tx_num; i++) {
            msg_chan = msg_cluster->msg_chan_tx_beg + i;
            if (msg_chan->tx_finish_irq == tx_finish_irq) {
                return msg_chan;
            }
        }
    }
    return NULL;
}

int vmngh_tx_finish_db_hander(struct vmng_msg_dev *msg_dev, u32 db_vector)
{
    struct vmng_msg_chan_tx *msg_chan = NULL;
    struct vmng_msg_cluster *msg_cluster = NULL;

    msg_chan = vmngh_get_tx_chan_by_tx_finsih_db(msg_dev, db_vector);
    if (msg_chan == NULL) {
        vmng_err("Get tx channel failed. (dev_id=%u; fid=%u; tx_finish=%u)\n",
                 msg_dev->dev_id, msg_dev->fid, db_vector);
        return -EINVAL;
    }
    msg_cluster = (struct vmng_msg_cluster *)msg_chan->msg_cluster;
    if (msg_cluster->msg_proc.tx_finish_proc == NULL) {
        vmng_err("Parameter tx_finish_proc is NULL. (dev_id=%u; fid=%u; chan_type=%u)\n", msg_dev->dev_id, msg_dev->fid,
            msg_cluster->chan_type);
        return -EINVAL;
    }

    msg_cluster->msg_proc.tx_finish_proc((unsigned long)(uintptr_t)msg_chan);

    return 0;
}

STATIC void vmng_fill_create_cluster_cmd(struct vmng_create_cluster_cmd *cmd, struct vmng_msg_cluster *msg_cluster,
    enum vmng_msg_chan_type chan_type, const struct vmng_msg_chan_irqs *int_irq_ary, u32 irq_array_len)
{
    u32 i;

    cmd->opcode = VMNGH_ADMIN_OPCODE_CREATE_CLUSTER;
    cmd->chan_type = chan_type;
    cmd->mem_off_base = 0;
    cmd->res.tx_base = msg_cluster->res.tx_base;
    cmd->res.tx_num = msg_cluster->res.tx_num;
    cmd->res.rx_base = msg_cluster->res.rx_base;
    cmd->res.rx_num = msg_cluster->res.rx_num;
    for (i = 0; i < irq_array_len / sizeof(u32); i++) {
        cmd->int_irq[i] = *(int_irq_ary->tx_send_irq + i);
    }
}

STATIC int vmngh_alloc_remote_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    struct vmng_msg_chan_irqs *int_irq_ary)
{
    struct vmng_tx_msg_proc_info tx_info;
    struct vmng_create_cluster_cmd *cmd = NULL;
    struct vmng_create_cluster_reply *reply = NULL;
    struct vmng_msg_cluster *msg_cluster = NULL;
    u32 irq_array_len;
    u32 send_len;
    u32 recv_len = sizeof(struct vmng_create_cluster_reply);
    u32 cmd_len;
    int ret;

    msg_cluster = &(msg_dev->msg_cluster[chan_type]);
    irq_array_len =
        (u32)(((u64)msg_cluster->res.tx_num * VMNG_MSG_SQ_TXRX + (u64)msg_cluster->res.rx_num * VMNG_MSG_SQ_TXRX) *
        sizeof(u32));
    send_len = sizeof(struct vmng_create_cluster_cmd) + irq_array_len;
    cmd_len = (send_len >= recv_len) ? send_len : recv_len;
    cmd = vmng_kzalloc(cmd_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cmd == NULL) {
        vmng_err("Call kzalloc failed. (dev_id=%u; fid=%u; chan_type=%u)\n",
                 msg_dev->dev_id, msg_dev->fid, chan_type);
        return -EINVAL;
    }
    vmng_fill_create_cluster_cmd(cmd, msg_cluster, chan_type, int_irq_ary, irq_array_len);

    tx_info.data = (void *)cmd;
    tx_info.in_data_len = cmd_len;
    tx_info.out_data_len = recv_len;
    tx_info.real_out_len = 0;
    ret = vmng_admin_msg_send(msg_dev->admin_tx, &tx_info, (u32)VMNG_MSG_CHAN_TYPE_ADMIN,
        (u32)VMNGH_ADMIN_OPCODE_CREATE_CLUSTER);
    if (ret != 0) {
        vmng_err("Message send failed. (dev_id=%u; fid=%u; ret=%d)\n", msg_dev->dev_id, msg_dev->fid, ret);
        goto FREE_CMD;
    }
    reply = (struct vmng_create_cluster_reply *)cmd;
    if (reply->remote_alloc_finish != VMNG_CREATE_CLUSTER_FINISH) {
        vmng_err("Finish status is error. (dev_id=%u; fid=%u; status=0x%x)\n",
                 msg_dev->dev_id, msg_dev->fid, reply->remote_alloc_finish);
        ret = -EINVAL;
        goto FREE_CMD;
    }
FREE_CMD:
    vmng_kfree(cmd);
    cmd = NULL;
    return ret;
}

STATIC int vmngh_alloc_msg_cluster_irqs(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *irq_array)
{
    u32 irq_array_len;
    u32 i;

    irq_array_len = (u32)(((u64)res->tx_num * VMNG_MSG_SQ_TXRX + (u64)res->rx_num * VMNG_MSG_SQ_TXRX) * sizeof(u32));
    if (irq_array_len == 0) {
        vmng_info("Parameter tx_num and rx_num both zero. (dev_id=%u; fid=%u; tx_num=%u; rx_num=%u)\n",
            msg_dev->dev_id, msg_dev->fid, res->tx_num, res->rx_num);
        return 0;
    }
    /* irq array: | tx_send_irq | tx_finish_irq | rx_recv_irq | rx_resp_irq | */
    irq_array->tx_send_irq = (u32 *)vmng_kzalloc(irq_array_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (irq_array->tx_send_irq == NULL) {
        vmng_err("Alloc irq_array failed. (dev_id=%u; fid=%u)\n", msg_dev->dev_id, msg_dev->fid);
        return -ENOMEM;
    }
    irq_array->tx_finish_irq = irq_array->tx_send_irq + res->tx_num;
    irq_array->rx_recv_irq = irq_array->tx_finish_irq + res->tx_num;
    irq_array->rx_resp_irq = irq_array->rx_recv_irq + res->rx_num;

    for (i = 0; i < res->tx_num; i++) {
        irq_array->tx_send_irq[i] = msg_dev->msix_irq_base + res->tx_base + i;
        irq_array->tx_finish_irq[i] = 0; /* zero means no irq */
    }
    for (i = 0; i < res->rx_num; i++) {
        irq_array->rx_recv_irq[i] = msg_dev->db_irq_base + res->rx_base + i;
        irq_array->rx_resp_irq[i] = 0; /* zero means no irq */
    }

    vmngh_set_blk_irq_array_adapt(msg_dev, chan_type, res, irq_array);

    return 0;
}

STATIC void vmngh_free_msg_cluster_irqs(struct vmng_msg_chan_irqs *irq_array)
{
    if (irq_array->tx_send_irq != NULL) {
        vmng_kfree(irq_array->tx_send_irq);
    }
}

STATIC int vmngh_alloc_msg_cluster_para_check(const struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_proc *msg_proc)
{
    u32 dev_id = msg_dev->dev_id;
    u32 fid = msg_dev->fid;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; chan_type=%u)\n", dev_id, fid, chan_type);
        return -EINVAL;
    }

    if (chan_type >= VMNG_MSG_CHAN_TYPE_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; chan_type=%u)\n", dev_id, fid, chan_type);
        return -EINVAL;
    }

    if (msg_proc == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; chan_type=%u)\n", dev_id, fid, chan_type);
        return -EINVAL;
    }
    return 0;
}

int vmngh_alloc_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    struct vmng_msg_proc *msg_proc)
{
    struct vmng_msg_chan_res *res = NULL;
    struct vmng_msg_chan_irqs irq_array = {0};
    int ret;

    /* 0. para check */
    if (vmngh_alloc_msg_cluster_para_check(msg_dev, chan_type, msg_proc) != 0) {
        vmng_err("Alloc message cluster parameter check failed.\n");
        return -EINVAL;
    }

    /* 1. prepare parameter, msg_dev, res, irqs */
    res = vmngh_get_msg_cluster_res(msg_dev, chan_type);
    if (res == NULL) {
        vmng_err("Get message cluster res. (dev_id=%u; fid=%u; chan_type=%u)\n",
                 msg_dev->dev_id, msg_dev->fid, chan_type);
        return -EINVAL;
    }

    if (res->rx_num == 0 && res->tx_num == 0) {
        return 0;
    }

    ret = vmngh_alloc_msg_cluster_irqs(msg_dev, chan_type, res, &irq_array);
    if (ret != 0) {
        vmng_err("Alloc irq failed. (dev_id=%u; fid=%u; chan_type=%u; ret=%d)\n",
                 msg_dev->dev_id, msg_dev->fid, chan_type,
            ret);
        return ret;
    }

    /* 2. local msg cluster initial */
    ret = vmng_alloc_local_msg_cluster(msg_dev, chan_type, res, &irq_array, msg_proc);
    if (ret != 0) {
        vmng_err("Alloc msg cluster failed. (dev_id=%u; fid=%u; chan_type=%u; ret=%d)\n", msg_dev->dev_id, msg_dev->fid,
            chan_type, ret);
        goto FREE_IRQS;
    }

    /* 3. remote msg cluster initial through admin */
    ret = vmngh_alloc_remote_msg_cluster(msg_dev, chan_type, &irq_array);
    if (ret != 0) {
        vmng_err("Alloc remote msg cluster failed. (dev_id=%u; fid=%u; chan_type=%u; ret=%d)\n",
            msg_dev->dev_id, msg_dev->fid, chan_type, ret);
        goto FREE_MSG_CLUSTER;
    }

    msg_dev->msg_cluster[chan_type].status = VMNG_MSG_CLUSTER_STATUS_ENABLE;
    vmng_info("Alloc success. (dev_id=%u; fid=%u; chan_type=%u)\n", msg_dev->dev_id, msg_dev->fid, chan_type);
    goto FREE_IRQS;

FREE_MSG_CLUSTER:
    vmng_free_msg_cluster(msg_dev, chan_type);
    msg_dev->msg_cluster[chan_type].status = VMNG_MSG_CLUSTER_STATUS_DISABLE;

FREE_IRQS:
    vmngh_free_msg_cluster_irqs(&irq_array);
    return ret;
}

/* interrupts operations */
STATIC void vmngh_send_int_to_remote(void *msg_dev_in, u32 vector)
{
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_dev_in;
    struct vmngh_vpc_unit *unit = (struct vmngh_vpc_unit *)msg_dev->unit;
    u32 msix_offset = unit->shr_para->msix_offset;
    int ret;

    ret = hw_dvt_hypervisor_inject_msix(unit->vdavinci, vector + msix_offset);
    if (ret != 0) {
        vmng_err("Call hw_dvt_hypervisor_inject_msix error. (dev_id=%u; fid=%u; vector=%u; ret=%d)\n",
                 msg_dev->dev_id, msg_dev->fid, vector, ret);
    }
}

STATIC int vmngh_tx_irq_init(struct vmng_msg_chan_tx *msg_chan)
{
    return 0;
}

STATIC int vmngh_tx_irq_uninit(struct vmng_msg_chan_tx *msg_chan)
{
    return 0;
}

int vmngh_rx_irq_init(struct vmng_msg_chan_rx *msg_chan)
{
    return 0;
}

int vmngh_rx_irq_uninit(struct vmng_msg_chan_rx *msg_chan)
{
    return 0;
}

/* alloc here, free in public func vmng_free_msg_dev */
STATIC struct vmng_msg_dev *vmngh_alloc_msg_dev(struct vmngh_vpc_unit *unit)
{
    struct vmng_msg_dev *msg_dev = NULL;
    int ret;

    msg_dev = vmng_kzalloc(sizeof(struct vmng_msg_dev), KA_GFP_KERNEL);
    if (msg_dev == NULL) {
        vmng_err("Call kzalloc fail. (dev_id=%u; fid=%u; size=%lu)\n",
                 unit->dev_id, unit->fid, sizeof(struct vmng_msg_dev));
        return NULL;
    }
    unit->msg_dev = msg_dev;
    msg_dev->unit = unit;
    msg_dev->dev_id = unit->dev_id;
    msg_dev->fid = unit->fid;
    msg_dev->chan_num = VMNG_MSG_CHAN_NUM_MAX;
    msg_dev->db_base = unit->db_base;
    msg_dev->db_irq_base = VMNG_DB_BASE_MSG;
    msg_dev->mem_base = unit->msg_base;
    msg_dev->msix_irq_base = VMNG_MSIX_BASE_MSG;

    msg_dev->ops.send_irq_to_remote = vmngh_send_int_to_remote;
    msg_dev->ops.tx_irq_init = vmngh_tx_irq_init;
    msg_dev->ops.tx_irq_uninit = vmngh_tx_irq_uninit;
    msg_dev->ops.rx_irq_init = vmngh_rx_irq_init;
    msg_dev->ops.rx_irq_uninit = vmngh_rx_irq_uninit;

    msg_dev->work_queue = ka_task_create_workqueue("vmngh_wq");
    if (msg_dev->work_queue == NULL) {
        vmng_err("Create msg work_queue failed. (dev_id=%u; fid=%u)\n", unit->dev_id, unit->fid);
        vmng_kfree(msg_dev);
        msg_dev = NULL;
        return NULL;
    }

    ret = vmng_msg_chan_init(VMNG_HOST_SIDE, msg_dev);
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

/* function: vmngh_msg_init
 * description: initialize message device, but irqs not been mounted,
 * irqs mouted when message channel alloced.
 */
int vmngh_init_vpc_msg(void *unit_in)
{
    struct vmng_msg_dev *msg_dev = NULL;
    struct vmngh_vpc_unit *unit = (struct vmngh_vpc_unit *)unit_in;

    if (unit == NULL) {
        vmng_err("Input unit is NULL.\n");
        return -EINVAL;
    }

    msg_dev = vmngh_alloc_msg_dev(unit);
    if (msg_dev == NULL) {
        vmng_err("Alloc message failed. (dev_id=%u; fid=%u)\n", unit->dev_id, unit->fid);
        return -EINVAL;
    }

    vmngh_init_admin_msg(msg_dev);

    vmng_info("Message device init success. (dev_id=%u; fid=%u)\n", unit->dev_id, unit->fid);
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_init_vpc_msg);

void vmngh_uninit_vpc_msg(void *unit_in)
{
    struct vmngh_vpc_unit *unit = (struct vmngh_vpc_unit *)unit_in;
    vmngh_uninit_admin_msg(unit->msg_dev);
    vmng_free_msg_dev(unit->msg_dev);
    unit->msg_dev = NULL;
}
KA_EXPORT_SYMBOL(vmngh_uninit_vpc_msg);

STATIC int __init vmngh_vpc_init_module(void)
{
    vmng_info("Init vmngh vpc module finish.\n");
    KA_INIT_LIST_HEAD(&g_msg_dev_head.msg_dev_head);
    ka_task_mutex_init(&g_msg_dev_head.mutex);
    return 0;
}
ka_module_init(vmngh_vpc_init_module);

STATIC void __exit vmngh_vpc_exit_module(void)
{
    vmngh_msg_dev_list_free();
    vmng_info("Exit vmngh vpc module finish.\n");
}
ka_module_exit(vmngh_vpc_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("virt vpc host driver");
KA_MODULE_LICENSE("GPL");

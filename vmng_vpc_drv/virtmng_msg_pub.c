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

#include "virtmng_msg_pub.h"
#include "virtmng_stack.h"
#include "virtmng_public_def.h"
#include "virtmng_msg_admin.h"
#include "virtmng_msg_pub.h"
#include "vmng_mem_alloc_interface.h"

#include "ka_system_pub.h"
#include "ka_barrier_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"

STATIC void vmng_uninit_msg_cluster_rx_chan(struct vmng_msg_cluster *msg_cluster, const struct vmng_msg_ops *ops);

bool vmng_is_vpc_chan(enum vmng_msg_chan_type chan_type)
{
    if ((chan_type > VMNG_MSG_CHAN_TYPE_COMMON) && (chan_type <= VMNG_MSG_CHAN_TYPE_VPC)) {
        return true;
    }
    return false;
}

bool vmng_is_blk_chan(enum vmng_msg_chan_type chan_type)
{
    if ((chan_type > VMNG_MSG_CHAN_TYPE_VPC) && (chan_type <= VMNG_MSG_CHAN_TYPE_BLOCK)) {
        return true;
    }
    return false;
}

enum vmng_msg_block_type vmng_vpc_to_blk_type(enum vmng_vpc_type vpc_type)
{
    enum vmng_msg_block_type blk_type;

    switch (vpc_type) {
        case VMNG_VPC_TYPE_HDC:
            blk_type = VMNG_MSG_BLOCK_TYPE_HDC;
            break;
        default:
            blk_type = VMNG_MSG_BLOCK_TYPE_MAX;
            break;
    }

    return blk_type;
}

enum vmng_vpc_type vmng_blk_to_vpc_type(enum vmng_msg_block_type blk_type)
{
    enum vmng_vpc_type vpc_type;

    switch (blk_type) {
        case VMNG_MSG_BLOCK_TYPE_HDC:
            vpc_type = VMNG_VPC_TYPE_HDC;
            break;
        default:
            vpc_type = VMNG_VPC_TYPE_MAX;
            break;
    }
    return vpc_type;
}

enum vmng_msg_chan_type vmng_vpc_type_to_msg_chan_type(enum vmng_vpc_type vpc_type)
{
    return (enum vmng_msg_chan_type)(vpc_type + VMNG_VPC_TO_MSG_CHAN_OFFSET);
}

enum vmng_vpc_type vmng_msg_chan_type_to_vpc_type(enum vmng_msg_chan_type chan_type)
{
    return (enum vmng_vpc_type)(chan_type - VMNG_VPC_TO_MSG_CHAN_OFFSET);
}

enum vmng_msg_chan_type vmng_block_type_to_msg_chan_type(enum vmng_msg_block_type block_type)
{
    return (enum vmng_msg_chan_type)(block_type + VMNG_BLK_TO_MSG_CHAN_OFFSET);
}

enum vmng_msg_block_type vmng_msg_chan_type_to_block_type(enum vmng_msg_chan_type chan_type)
{
    return (enum vmng_msg_block_type)(chan_type - VMNG_BLK_TO_MSG_CHAN_OFFSET);
}

int vmng_msg_chan_tx_info_para_check(const struct vmng_tx_msg_proc_info *tx_info)
{
    if (tx_info == NULL) {
        vmng_err("Input parameter tx_info is error.\n");
        return -EINVAL;
    }
    if (tx_info->data == NULL) {
        vmng_err("Input parameter data is error.\n");
        return -EINVAL;
    }
    if (tx_info->in_data_len >= VMNG_MSG_SQ_DATA_MAX_SIZE) {
        vmng_err("Input parameter in_data_len is error. (in_data_len=%u)\n", tx_info->in_data_len);
        return -EINVAL;
    }
    if (tx_info->out_data_len >= VMNG_MSG_SQ_DATA_MAX_SIZE) {
        vmng_err("Input parameter out_data_len is error. (out_data_len=%u)\n", tx_info->out_data_len);
        return -EINVAL;
    }
    return 0;
}

void vmng_msg_push_rx_queue_work(struct vmng_msg_chan_rx *msg_chan)
{
    struct vmng_msg_desc *desc = NULL;

    if ((msg_chan->sq_rx == NULL) || (msg_chan->rx_wq == NULL)) {
        vmng_err("sq_rx is NULL or rx_wq is NULL. (type=%u;chan_id=%u).\n", msg_chan->chan_type, msg_chan->chan_id);
        return;
    }
    msg_chan->stamp = jiffies;
    vmng_debug("Get message channel. (vpc=%u; channel=%u)\n", msg_chan->chan_type, msg_chan->chan_id);
    desc = (struct vmng_msg_desc *)msg_chan->sq_rx;
    if ((desc->out_data_len < VMNG_MSG_SQ_DATA_MAX_SIZE) && (desc->in_data_len < VMNG_MSG_SQ_DATA_MAX_SIZE) &&
        (desc->status == VMNG_MSG_SQ_STATUS_PREPARE)) {
        ka_task_queue_work(msg_chan->rx_wq, &msg_chan->rx_work);
    } else {
        vmng_err("desc_status is error. (vpc=%u; chan=%u; desc_status=0x%x)\n",
                 msg_chan->chan_type, msg_chan->chan_id, desc->status);
    }
}

int vmng_sync_msg_wait_undesire(const u32 *status, int status_init, int status_enter_proc, u32 cycle, u32 len)
{
    int timeout = (int)len;

    while (timeout > 0) {
        if (((*status) != status_init) && ((*status) != status_enter_proc)) {
            return 0;
        }
        ka_rmb();
        ka_system_usleep_range(cycle, cycle);
        timeout -= (int)cycle;
    }
    return -EINVAL;
}

STATIC int vmng_wait_tx_finish_irq(struct vmng_msg_chan_tx *msg_chan)
{
    int ret;
    u32 exit_wait_cnt = VMNG_MSG_TX_FINISH_WAIT_CNT;

    /*lint -e666*/
    ret = ka_task_wait_event_interruptible(msg_chan->tx_block_wq, (msg_chan->tx_block_status == VMNG_NO_BLK_STATUS));
    /*lint +e666*/
    if (ret == -ERESTARTSYS) {
        while (exit_wait_cnt-- > 0) {
            ka_system_usleep_range(1000000, 1000000);  // 1000000us ~ 1000000us
            if (msg_chan->tx_block_status == VMNG_NO_BLK_STATUS) {
                break;
            }
        }
    }

    if (ret < 0) {
        vmng_err("Block chan timeout. (msg_chan=%u; ret=%d)\n", msg_chan->chan_id, ret);
        return -EINVAL;
    }
    return ret;
}

void vmng_msg_tx_finish_task(unsigned long data)
{
    struct vmng_msg_chan_tx *msg_chan = (struct vmng_msg_chan_tx *)(uintptr_t)data;

    msg_chan->tx_block_status = VMNG_NO_BLK_STATUS;
    ka_wmb();
    ka_task_wake_up_interruptible(&msg_chan->tx_block_wq);
}

int vmng_msg_reply_data(struct vmng_msg_chan_tx *msg_chan, void *data, u32 out_data_len, u32 *real_out_data_len)
{
    struct vmng_msg_desc *bd_desc = NULL;
    u32 status;

    if ((msg_chan->sq_tx == NULL)) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }

    bd_desc = (struct vmng_msg_desc *)msg_chan->sq_tx;
    status = bd_desc->status;

    switch (status) {
        case VMNG_MSG_SQ_STATUS_NO_PROC:
            vmng_err("Remote no message proc func. (chan=%u; status=0x%x)\n", msg_chan->chan_id, status);
            return -ENOSYS;
        case VMNG_MSG_SQ_STATUS_ENTER_PROC:
            vmng_err("Status enter rx_proc, but it cost too long, tx timeout. (channel=%u; status=0x%x)\n",
                msg_chan->chan_id, status);
            return -EINVAL;
        case VMNG_MSG_SQ_STATUS_PROC_RET_FAILED:
            vmng_err("Remote msg proc returned failed. (chan=%u; status=0x%x)\n", msg_chan->chan_id, status);
            return -EINVAL;
        case VMNG_MSG_SQ_STATUS_PROC_DATA_ERROR:
            vmng_err("Remote msg proc returned data overflow. (chan=%u; status=0x%x)\n", msg_chan->chan_id, status);
            return -EINVAL;
        case VMNG_MSG_SQ_STATUS_PROC_SUCCESS:
            break;
        default:
            vmng_err("Remote returned status not expect. (chan=%u; status=0x%x)\n", msg_chan->chan_id, status);
            return -EINVAL;
    }

    *real_out_data_len = bd_desc->real_out_len;
    if ((*real_out_data_len > out_data_len) || (*real_out_data_len > VMNG_MSG_SQ_DATA_MAX_SIZE)) {
        vmng_err("Real out length is overflow. (real_out_len=%d; out_date_len=%d)\n",
            *real_out_data_len, out_data_len);
        return -EINVAL;
    }
    /* real_out_len could be zero */
    if (*real_out_data_len > 0) {
        ka_mm_memcpy_fromio(data, (void *)bd_desc->data, *real_out_data_len);
    }

    return 0;
}

int vmng_msg_fill_desc(const struct vmng_tx_msg_proc_info *tx_info, u32 opcode_d1, u32 opcode_d2,
    struct vmng_msg_chan_tx *msg_chan, u32 **p_sq_status)
{
    struct vmng_msg_desc *bd_desc = NULL;

    if (msg_chan->sq_tx == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }

    bd_desc = (struct vmng_msg_desc *)msg_chan->sq_tx;
    bd_desc->status = VMNG_MSG_SQ_STATUS_IDLE;
    bd_desc->in_data_len = tx_info->in_data_len;
    bd_desc->out_data_len = tx_info->out_data_len;
    bd_desc->real_out_len = 0;
    bd_desc->opcode_d1 = opcode_d1;
    bd_desc->opcode_d2 = opcode_d2;
    ka_mm_memcpy_toio(bd_desc->data, tx_info->data, tx_info->in_data_len);
    *p_sq_status = &(bd_desc->status);

    return 0;
}

STATIC int vmng_alloc_chan_wait(struct vmng_msg_cluster *msg_cluster)
{
#ifndef EMU_ST
    long time;
    u32 chn_alloc = (u32)-1;
    int ret;
    struct vmng_stack *stack = NULL;
    struct vmng_msg_chan_tx *msg_chan = NULL;

    if (msg_cluster->res.tx_num == 0) {
        vmng_err("chan_type no tx abitily. (dev_id=%u; fid=%u; chan_type=%u) \n", msg_cluster->dev_id,
            msg_cluster->fid, msg_cluster->chan_type);
        return -EINVAL;
    }

    time = (int)ka_system_msecs_to_jiffies(VMNG_MSG_ALLOC_WAIT_TIMEOUT_MS);
    ret = ka_task_down_timeout(&msg_cluster->cluster_sema, time);
    if (ret != 0) {
        vmng_info("Down wait timeout.\n");
        return -ENOSPC;
    }

    ka_task_mutex_lock(&msg_cluster->mutex);
    if (msg_cluster->alloc_stack == NULL) {
        ka_task_mutex_unlock(&msg_cluster->mutex);
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    stack = msg_cluster->alloc_stack;
    ret = vmng_stack_pop(stack, msg_cluster->res.tx_num, &chn_alloc);
    if (ret != 0) {
        ka_task_mutex_unlock(&msg_cluster->mutex);
        ka_task_up(&msg_cluster->cluster_sema);
        vmng_err_limit("Call vmng_stack_pop error. (ret=%d)\n", ret);
        return ret;
    }

    msg_chan = msg_cluster->msg_chan_tx_beg + chn_alloc;
    if (msg_chan->status != VMNG_MSG_CHAN_STATUS_IDLE) {
        vmng_stack_push(msg_cluster->alloc_stack, msg_cluster->res.tx_num, chn_alloc);
        ka_task_mutex_unlock(&msg_cluster->mutex);
        ka_task_up(&msg_cluster->cluster_sema);
        vmng_err("Status not idle. (index=%u; status=%u)\n", chn_alloc, msg_chan->status);
        return -EINVAL;
    }
    msg_chan->status = VMNG_MSG_CHAN_STATUS_USED;
    ka_task_mutex_unlock(&msg_cluster->mutex);
    vmng_debug("Alloc success. (index=%u; chan_id=%u)\n", chn_alloc, msg_chan->chan_id);
    return (int)chn_alloc;
#else
    return 0;
#endif
}

STATIC void vmng_wakup_alloc_wait(struct vmng_msg_cluster *msg_cluster, int chan)
{
#ifndef EMU_ST
    struct vmng_msg_chan_tx *msg_chan = NULL;

    if (msg_cluster->msg_chan_tx_beg == NULL) {
        vmng_err("Input parameter is error.\n");
        return;
    }
    if ((chan < 0) || (chan >= (int)msg_cluster->res.tx_num)) {
        vmng_err("Input parameter is invalid. (chan=%d)\n", chan);
        return;
    }

    msg_chan = msg_cluster->msg_chan_tx_beg + chan;
    msg_chan->status = VMNG_MSG_CHAN_STATUS_IDLE;

    ka_task_mutex_lock(&msg_cluster->mutex);
    if (vmng_stack_push(msg_cluster->alloc_stack, msg_cluster->res.tx_num, (u32)chan) != 0) {
        vmng_err("Stack push failed. (dev_id=%u; fid=%u)\n", msg_cluster->dev_id, msg_cluster->fid);
    }
    ka_task_mutex_unlock(&msg_cluster->mutex);
    ka_task_up(&msg_cluster->cluster_sema);
#endif
}

STATIC int vmng_sync_msg_send_para_chk(struct vmng_msg_cluster *msg_cluster,
    const struct vmng_tx_msg_proc_info *tx_info)
{
    u32 dev_id;
    u32 fid;

    if (msg_cluster == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    dev_id = msg_cluster->dev_id;
    fid = msg_cluster->fid;
    if (tx_info == NULL) {
        vmng_err("Input parameter tx_info is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (tx_info->data == NULL) {
        vmng_err("Input parameter tx_info data is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (tx_info->in_data_len >= VMNG_MSG_SQ_DATA_MAX_SIZE) {
        vmng_err("Input parameter in data_len is error. (dev_id=%u; fid=%u; in_data_len=%u)\n",
                 dev_id, fid, tx_info->in_data_len);
        return -EINVAL;
    }

    if (tx_info->out_data_len >= VMNG_MSG_SQ_DATA_MAX_SIZE) {
        vmng_err("Input parameter is out_data_len error. (dev_id=%u; fid=%u; out_data_len=%u)\n",
                 dev_id, fid, tx_info->out_data_len);
        return -EINVAL;
    }
    if (msg_cluster->status != VMNG_MSG_CLUSTER_STATUS_ENABLE) {
        vmng_err("Cluster status is error. (dev_id=%u; fid=%u; status=%u)\n", dev_id, fid, msg_cluster->status);
        return -EINVAL;
    }
    if (msg_cluster->msg_chan_tx_beg == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    return 0;
}

int vmng_sync_msg_send(struct vmng_msg_cluster *msg_cluster, struct vmng_tx_msg_proc_info *tx_info, u32 opcode_d1,
    u32 opcode_d2, u32 timeout)
{
    struct vmng_msg_chan_tx *msg_chan = NULL;
    u32 *p_sq_status = NULL; /* srv and agent shr status. */
    int chan_use, ret;

    ret = vmng_sync_msg_send_para_chk(msg_cluster, tx_info);
    if (ret != 0) {
        vmng_err("Parameter check error.\n");
        return ret;
    }

    chan_use = vmng_alloc_chan_wait(msg_cluster);
    if (chan_use < 0) {
        vmng_info("Call vmng_alloc_chan_wait. (dev_id=%u; fid=%u; ret=%d)\n",
            msg_cluster->dev_id, msg_cluster->fid, chan_use);
        return chan_use;
    }
    msg_chan = msg_cluster->msg_chan_tx_beg + chan_use;

    if (msg_chan->send_irq_to_remote == NULL) {
        vmng_err("Send irq func is NULL. (dev_id=%u; fid=%u)\n", msg_cluster->dev_id, msg_cluster->fid);
        goto WAKE_UP_AND_RELEASE_CHAN;
    }

    ret = vmng_msg_fill_desc(tx_info, opcode_d1, opcode_d2, msg_chan, &p_sq_status);
    if (ret != 0) {
        vmng_err("Fill descriptor failed. (dev_id=%u; fid=%u; ret=%d)\n", msg_cluster->dev_id, msg_cluster->fid, ret);
        goto WAKE_UP_AND_RELEASE_CHAN;
    }
    *p_sq_status = VMNG_MSG_SQ_STATUS_PREPARE;
    ka_mb();

    if (vmng_is_blk_chan(msg_cluster->chan_type) == false) {
        msg_chan->send_irq_to_remote(msg_chan->msg_dev, msg_chan->tx_send_irq);
        ret = vmng_sync_msg_wait_undesire(p_sq_status, VMNG_MSG_SQ_STATUS_PREPARE, VMNG_MSG_SQ_STATUS_ENTER_PROC,
            VMNG_MSG_SYNC_WAIT_CYCLE_US, timeout);
    } else {
        /* must set status first, then send irq */
        msg_chan->tx_block_status = VMNG_BLK_STATUS;
        msg_chan->send_irq_to_remote(msg_chan->msg_dev, msg_chan->tx_send_irq);
        ret = vmng_wait_tx_finish_irq(msg_chan);
    }
    if (ret < 0) {
        vmng_err("Wait rx timeout. (dev_id=%u; fid=%u; status=0x%x)\n",
            msg_cluster->dev_id, msg_cluster->fid, *p_sq_status);
        goto WAKE_UP_AND_RELEASE_CHAN;
    }

    ret = vmng_msg_reply_data(msg_chan, tx_info->data, tx_info->out_data_len, &tx_info->real_out_len);
    if (ret < 0) {
        vmng_err("Call vmng_msg_reply_data failed. (dev_id=%u; fid=%u; ret=%d)\n",
                 msg_cluster->dev_id, msg_cluster->fid, ret);
        goto WAKE_UP_AND_RELEASE_CHAN;
    }

WAKE_UP_AND_RELEASE_CHAN:
    vmng_wakup_alloc_wait(msg_cluster, chan_use);

    return ret;
}

/* rx_msg_task ----> admin_rx_proc
 * |--> rx_chan_proc_call_cluster --> common_rx_proc
 * |--> vpc_rx_proc
 */
void vmng_msg_rx_msg_task(ka_work_struct_t *p_work)
{
    struct vmng_msg_chan_rx *msg_chan = ka_container_of(p_work, struct vmng_msg_chan_rx, rx_work);
    struct vmng_msg_desc *desc = NULL;
    struct vmng_msg_chan_rx_proc_info proc_info;
    struct vmng_msg_dev *msg_dev = NULL;
    int ret;

    if (msg_chan->sq_rx == NULL) {
        vmng_err("sq_rx is invalid. (msg_chan=%u)\n", msg_chan->chan_id);
        return;
    }
    msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    vmng_msg_dfx_resq_time(msg_dev->dev_id, msg_dev->fid, msg_chan, "vpc msg sche time", VMNG_MSG_WORK_SCHE_TIME);

    desc = (struct vmng_msg_desc *)msg_chan->sq_rx;
    proc_info.data = desc->data;
    proc_info.in_data_len = desc->in_data_len;
    proc_info.out_data_len = desc->out_data_len;
    proc_info.real_out_len = &(desc->real_out_len);
    proc_info.opcode_d1 = desc->opcode_d1;
    proc_info.opcode_d2 = desc->opcode_d2;
    if ((desc->status == VMNG_MSG_SQ_STATUS_PREPARE) && (msg_chan->rx_proc != NULL)) {
        desc->status = VMNG_MSG_SQ_STATUS_ENTER_PROC;
        ret = msg_chan->rx_proc((void *)msg_chan, &proc_info);
        if (ret == 0) {
            if (desc->real_out_len <= desc->out_data_len) {
                desc->status = VMNG_MSG_SQ_STATUS_PROC_SUCCESS;
            } else {
                vmng_err("Status proc data error. (dev_id=%u; fid=%u; status=0x%x; ret=%d)\n",
                         msg_dev->dev_id, msg_dev->fid, desc->status, ret);
                desc->status = VMNG_MSG_SQ_STATUS_PROC_DATA_ERROR;
            }
        } else if (ret == -ENOSYS) {
            vmng_err("Status no proc. (dev_id=%u; fid=%u; status=0x%x; ret=%d)\n",
                     msg_dev->dev_id, msg_dev->fid, desc->status, ret);
            desc->status = VMNG_MSG_SQ_STATUS_NO_PROC;
        } else {
            vmng_err("Status proc is invalid. (dev_id=%u; fid=%u; status=0x%x; ret=%d)\n",
                     msg_dev->dev_id, msg_dev->fid, desc->status, ret);
            desc->status = VMNG_MSG_SQ_STATUS_PROC_RET_FAILED;
        }
        ka_wmb();
        vmng_msg_dfx_resq_time(msg_dev->dev_id, msg_dev->fid, msg_chan, "vpc msg process time", VMNG_MSG_PROCESS_TIME);
        if ((msg_chan->chan_type > VMNG_MSG_CHAN_TYPE_VPC) && (msg_chan->chan_type <= VMNG_MSG_CHAN_TYPE_BLOCK)) {
            msg_dev->ops.send_irq_to_remote(msg_chan->msg_dev, msg_chan->resp_int_id);
        }
    } else {
        vmng_warn("Channel ID is invalid. (type=%d; chan_id=%d)\n", msg_chan->chan_type, msg_chan->chan_id);
    }
}

STATIC int vmng_init_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_proc *msg_proc)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_stack *alloc_stack = NULL;

    msg_cluster = &(msg_dev->msg_cluster[chan_type]);
    msg_cluster->dev_id = msg_dev->dev_id;
    msg_cluster->fid = msg_dev->fid;
    msg_cluster->chan_type = chan_type;
    msg_cluster->msg_dev = (void *)msg_dev;
    msg_cluster->res.tx_base = res->tx_base;
    msg_cluster->res.tx_num = res->tx_num;
    msg_cluster->res.rx_base = res->rx_base;
    msg_cluster->res.rx_num = res->rx_num;
    msg_cluster->msg_proc.rx_recv_proc = msg_proc->rx_recv_proc;
    msg_cluster->msg_proc.tx_finish_proc = msg_proc->tx_finish_proc;
    if ((msg_cluster->res.tx_base >= VMNG_MSG_CHAN_NUM_MAX) || (msg_cluster->res.rx_base >= VMNG_MSG_CHAN_NUM_MAX)) {
        vmng_err("tx_base or rx_base out of range. (dev_id=%u; fid=%u; chan_type=%u; tx_base=%u; rx_base=%u)\n",
            msg_dev->dev_id, msg_dev->fid, chan_type, msg_cluster->res.tx_base, msg_cluster->res.rx_base);
        return -EINVAL;
    }
    msg_cluster->msg_chan_tx_beg = &(msg_dev->msg_chan_tx[msg_cluster->res.tx_base]);
    msg_cluster->msg_chan_rx_beg = &(msg_dev->msg_chan_rx[msg_cluster->res.rx_base]);
    /* tx alloc varavious alloc */
    ka_task_init_waitqueue_head(&msg_cluster->tx_alloc_wq);
    ka_task_mutex_init(&msg_cluster->mutex);
    ka_task_sema_init(&msg_cluster->cluster_sema, res->tx_num);
    alloc_stack = vmng_kzalloc(sizeof(struct vmng_stack) + msg_cluster->res.tx_num * sizeof(u32), KA_GFP_KERNEL);
    if (alloc_stack == NULL) {
        vmng_err("Alloc stack failed. (dev_id=%u; fid=%u)\n", msg_dev->dev_id, msg_dev->fid);
        return -EINVAL;
    }
    msg_cluster->alloc_stack = alloc_stack;
    vmng_stack_init(alloc_stack, msg_cluster->res.tx_num);

    /* finish build */
    msg_cluster->status = VMNG_MSG_CLUSTER_STATUS_INIT;

    return 0;
}

STATIC void vmng_uninit_msg_cluseter(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type)
{
    struct vmng_msg_cluster *msg_cluster = NULL;

    msg_cluster = &(msg_dev->msg_cluster[chan_type]);
    if (msg_cluster->status == VMNG_MSG_CLUSTER_STATUS_DISABLE) {
        return;
    }
    vmng_debug("Cluster free. (vpc=%u)\n", chan_type);
    ka_task_mutex_lock(&msg_cluster->mutex);
    if (msg_cluster->alloc_stack != NULL) {
        vmng_kfree(msg_cluster->alloc_stack);
        msg_cluster->alloc_stack = NULL;
    }
    ka_task_mutex_unlock(&msg_cluster->mutex);
    ka_task_wake_up_interruptible(&msg_cluster->tx_alloc_wq);
    msg_cluster->status = VMNG_MSG_CLUSTER_STATUS_DISABLE;
}

STATIC void vmng_init_msg_cluster_tx_chan(struct vmng_msg_cluster *msg_cluster, struct vmng_msg_ops *ops,
    const u32 *tx_send_irq, u32 *tx_finish_irq)
{
    struct vmng_msg_chan_tx *msg_chan = NULL;
    u32 i;

    for (i = 0; i < msg_cluster->res.tx_num; i++) {
        msg_chan = msg_cluster->msg_chan_tx_beg + i;
        msg_chan->msg_cluster = msg_cluster;
        msg_chan->chan_type = msg_cluster->chan_type;

        /* int to remote */
        msg_chan->tx_send_irq = *(tx_send_irq + i);
        msg_chan->tx_finish_irq = *(tx_finish_irq + i);
        msg_chan->send_irq_to_remote = ops->send_irq_to_remote;
        if (msg_cluster->msg_proc.tx_finish_proc != NULL) {
            ka_task_init_waitqueue_head(&msg_chan->tx_block_wq);
        }
        if ((ops->tx_irq_init != NULL) && (ops->tx_irq_init(msg_chan) != 0)) {
            vmng_err("Tx irq init failed. (dev_id=%u; fid=%u; chan_type=%u)\n", msg_cluster->dev_id, msg_cluster->fid,
                msg_cluster->chan_type);
            return;
        }

        ka_task_mutex_init(&msg_chan->mutex);
        msg_chan->status = VMNG_MSG_CHAN_STATUS_IDLE;
    }
}

STATIC void vmng_uninit_msg_cluseter_tx_chan(struct vmng_msg_cluster *msg_cluster, const struct vmng_msg_ops *ops)
{
    struct vmng_msg_chan_tx *msg_chan = NULL;
    u32 i;

    for (i = 0; i < msg_cluster->res.tx_num; i++) {
        msg_chan = msg_cluster->msg_chan_tx_beg + i;
        if (msg_chan->status == VMNG_MSG_CHAN_STATUS_DISABLE) {
            continue;
        }
        if ((ops->tx_irq_uninit != NULL) && (ops->tx_irq_uninit(msg_chan) != 0)) {
            vmng_err("Tx irq uninit failed. (dev_id=%u; fid=%u; chan_type=%u)\n", msg_cluster->dev_id,
                msg_cluster->fid, msg_cluster->chan_type);
            return;
        }
        msg_chan->msg_cluster = NULL;
        msg_chan->tx_send_irq = 0;
        msg_chan->tx_finish_irq = 0;
        msg_chan->status = VMNG_MSG_CHAN_STATUS_DISABLE;
    }
}

/* service side use dev_id and fid for parameter */
STATIC int vmng_rx_chan_proc_call_cluster(void *msg_chan, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_msg_chan_rx *msg_chan_in = (struct vmng_msg_chan_rx *)msg_chan;
    struct vmng_msg_cluster *msg_cluster = (struct vmng_msg_cluster *)msg_chan_in->msg_cluster;
    int ret;

    if (msg_cluster->msg_proc.rx_recv_proc != NULL) {
        ret = msg_cluster->msg_proc.rx_recv_proc(msg_chan_in, proc_info);
        if (ret != 0) {
            vmng_err("Rx message channel is error. (cluster=%u; chan=%u; ret=%d)\n",
                     msg_cluster->chan_type, msg_chan_in->chan_id, ret);
            return ret;
        }
    } else {
        vmng_warn("Cluster proc is NULL. (cluster=%u; chan_id=%u)\n", msg_cluster->chan_type, msg_chan_in->chan_id);
    }
    return 0;
}

STATIC void vmng_init_msg_cluster_rx_chan(struct vmng_msg_cluster *msg_cluster, struct vmng_msg_ops *ops,
    const u32 *rx_recv_irq, u32 *resp_int_id)
{
    struct vmng_msg_chan_rx *msg_chan = NULL;
    u32 i;
    int ret;

    if (ops->rx_irq_init == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", msg_cluster->dev_id, msg_cluster->fid);
        return;
    }

    for (i = 0; i < msg_cluster->res.rx_num; i++) {
        msg_chan = msg_cluster->msg_chan_rx_beg + i;
        msg_chan->msg_cluster = msg_cluster;
        msg_chan->chan_type = msg_cluster->chan_type;

        /* resp int to remote, if nessenry */
        msg_chan->resp_int_id = *(resp_int_id + i);
        msg_chan->send_int = NULL;
        /* rx msg proc */
        msg_chan->rx_recv_irq = *(rx_recv_irq + i);
        msg_chan->rx_proc = vmng_rx_chan_proc_call_cluster;
        msg_chan->rx_wq = ka_task_create_singlethread_workqueue("vpc_msg_chan_proc");
        if (msg_chan->rx_wq == NULL) {
            vmng_uninit_msg_cluster_rx_chan(msg_cluster, ops);
            vmng_err("Creat workqueue failed. (dev_id=%u; fid=%u; chan_id=%u)\n", msg_cluster->dev_id,
                msg_cluster->fid, msg_chan->chan_id);
            return;
        }
        /* init workqueue of irq half bottom */
        KA_TASK_INIT_WORK(&msg_chan->rx_work, vmng_msg_rx_msg_task);

        ret = ops->rx_irq_init(msg_chan);
        if (ret != 0) {
            ka_task_destroy_workqueue(msg_chan->rx_wq);
            msg_chan->rx_wq = NULL;
            vmng_uninit_msg_cluster_rx_chan(msg_cluster, ops);
            vmng_err("Rx irq init failed. (dev_id=%u; fid=%u; chan_id=%u; ret=%d)\n",
                msg_cluster->dev_id, msg_cluster->fid, msg_chan->chan_id, ret);
            return;
        }
        msg_chan->status = VMNG_MSG_CHAN_STATUS_IDLE;

        vmng_debug("Get msg_chan value. (dev_id=%u; fid=%u; chan=%u; recv_irq=%u; resp_id=%u; sq=0x%llx)\n",
            msg_cluster->dev_id, msg_cluster->fid, msg_chan->chan_id, msg_chan->rx_recv_irq,
            msg_chan->resp_int_id, (u64)msg_chan->sq_rx);
    }
}

STATIC void vmng_uninit_msg_cluster_rx_chan(struct vmng_msg_cluster *msg_cluster, const struct vmng_msg_ops *ops)
{
    struct vmng_msg_chan_rx *msg_chan = NULL;
    u32 i;

    for (i = 0; i < msg_cluster->res.rx_num; i++) {
        msg_chan = msg_cluster->msg_chan_rx_beg + i;
        if (msg_chan->status == VMNG_MSG_CHAN_STATUS_DISABLE) {
            continue;
        }
        if ((ops->rx_irq_uninit != NULL) && (ops->rx_irq_uninit(msg_chan) != 0)) {
            vmng_err("Rx irq uninit failed. (dev_id=%u; fid=%u; chan_type=%u)\n", msg_cluster->dev_id,
                msg_cluster->fid, msg_cluster->chan_type);
            return;
        }
        msg_chan->msg_cluster = NULL;
        msg_chan->rx_recv_irq = 0;
        msg_chan->resp_int_id = 0;
        msg_chan->status = VMNG_MSG_CHAN_STATUS_DISABLE;
        if (msg_chan->rx_wq != NULL) {
            ka_task_destroy_workqueue(msg_chan->rx_wq);
            msg_chan->rx_wq = NULL;
        }
    }
}

int vmng_alloc_local_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type,
    const struct vmng_msg_chan_res *res, struct vmng_msg_chan_irqs *int_irq_ary, struct vmng_msg_proc *msg_proc)
{
    struct vmng_msg_cluster *admin_msg_cluster = NULL;
    struct vmng_msg_cluster *msg_cluster = NULL;
    int ret;

    msg_cluster = &(msg_dev->msg_cluster[chan_type]);
    admin_msg_cluster = &(msg_dev->msg_cluster[VMNG_MSG_CHAN_TYPE_ADMIN]);

    ka_task_mutex_lock(&admin_msg_cluster->mutex);
    if (msg_cluster->status != VMNG_MSG_CLUSTER_STATUS_DISABLE) {
        ka_task_mutex_unlock(&admin_msg_cluster->mutex);
        vmng_err("Message cluster already register. (dev_id=%u; fid=%u; chan_type=%u)\n",
            msg_dev->dev_id, msg_dev->fid, chan_type);
        return -EINVAL;
    }

    /* init cluster process */
    ret = vmng_init_msg_cluster(msg_dev, chan_type, res, msg_proc);
    if (ret != 0) {
        ka_task_mutex_unlock(&admin_msg_cluster->mutex);
        vmng_err("Cluster init error. (dev_id=%u; fid=%u; ret=%d)\n", msg_dev->dev_id, msg_dev->fid, ret);
        return ret;
    }
    ka_task_mutex_unlock(&admin_msg_cluster->mutex);

    vmng_init_msg_cluster_tx_chan(&(msg_dev->msg_cluster[chan_type]), &(msg_dev->ops), int_irq_ary->tx_send_irq,
        int_irq_ary->tx_finish_irq);
    vmng_init_msg_cluster_rx_chan(&(msg_dev->msg_cluster[chan_type]), &(msg_dev->ops), int_irq_ary->rx_recv_irq,
        int_irq_ary->rx_resp_irq);

    return 0;
}

void vmng_free_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_chan_type chan_type)
{
    vmng_uninit_msg_cluster_rx_chan(&(msg_dev->msg_cluster[chan_type]), &(msg_dev->ops));
    vmng_uninit_msg_cluseter_tx_chan(&(msg_dev->msg_cluster[chan_type]), &(msg_dev->ops));
    vmng_uninit_msg_cluseter(msg_dev, chan_type);
}

STATIC void vmng_msg_get_sq_beg(u32 side, u32 *sq_tx_beg, u32 *sq_rx_beg)
{
    if (side == VMNG_AGENT_SIDE) {
        *sq_tx_beg = 0x0;
        *sq_rx_beg = 0x1;
    } else {
        *sq_tx_beg = 0x1;
        *sq_rx_beg = 0x0;
    }
}

/* function: vmng_msg_chan_init
 * description: initialize message channal, alloc sq queue
 */
int vmng_msg_chan_init(u32 side, struct vmng_msg_dev *msg_dev)
{
    struct vmng_msg_chan_tx *msg_chan_tx = NULL;
    struct vmng_msg_chan_rx *msg_chan_rx = NULL;
    u32 sq_tx_beg;
    u32 sq_rx_beg;
    int i;

    vmng_msg_get_sq_beg(side, &sq_tx_beg, &sq_rx_beg);

    for (i = 0; i < msg_dev->chan_num; i++) {
        msg_chan_tx = &(msg_dev->msg_chan_tx[i]);
        msg_chan_tx->status = VMNG_MSG_CHAN_STATUS_DISABLE;
        msg_chan_tx->chan_id = i;
        msg_chan_tx->msg_dev = msg_dev;
        msg_chan_tx->sq_tx = msg_dev->mem_base + (u64)((u64)i * VMNG_MSG_SQ_TXRX + sq_tx_beg) * VMNG_MSG_QUEUE_SQ_SIZE;
    }

    for (i = 0; i < msg_dev->chan_num; i++) {
        msg_chan_rx = &(msg_dev->msg_chan_rx[i]);
        msg_chan_rx->status = VMNG_MSG_CHAN_STATUS_DISABLE;
        msg_chan_rx->chan_id = i;
        msg_chan_rx->msg_dev = msg_dev;
        msg_chan_rx->sq_rx = msg_dev->mem_base + (u64)((u64)i * VMNG_MSG_SQ_TXRX + sq_rx_beg) * VMNG_MSG_QUEUE_SQ_SIZE;

        msg_chan_rx->sq_rx_safe_data = vmng_kzalloc(VMNG_MSG_SQ_DATA_MAX_SIZE, KA_GFP_KERNEL);
        if (msg_chan_rx->sq_rx_safe_data == NULL) {
            vmng_err("Kzalloc safe data failed. (size=%lu)\n", VMNG_MSG_SQ_DATA_MAX_SIZE);
            goto failed;
        }
    }
    return 0;
failed:
    for (i -= 1; i >= 0; i--) {
        msg_chan_rx = &(msg_dev->msg_chan_rx[i]);
        if (msg_chan_rx->sq_rx_safe_data != NULL) {
            vmng_kfree(msg_chan_rx->sq_rx_safe_data);
            msg_chan_rx->sq_rx_safe_data = NULL;
        }
    }
    return -EINVAL;
}

static void vmng_msg_chan_uninit(struct vmng_msg_dev *msg_dev)
{
    struct vmng_msg_chan_rx *msg_chan_rx = NULL;
    int i;

    for (i = 0; i < msg_dev->chan_num; i++) {
        msg_chan_rx = &(msg_dev->msg_chan_rx[i]);
        if (msg_chan_rx->sq_rx_safe_data != NULL) {
            vmng_kfree(msg_chan_rx->sq_rx_safe_data);
            msg_chan_rx->sq_rx_safe_data = NULL;
        }
    }
}

void vmng_free_msg_dev(struct vmng_msg_dev *msg_dev)
{
    enum vmng_msg_chan_type i;

    if (msg_dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return;
    }

    for (i = VMNG_MSG_CHAN_TYPE_COMMON; i < VMNG_MSG_CHAN_TYPE_MAX; i++) {
        vmng_free_msg_cluster(msg_dev, i);
    }
    vmng_msg_chan_uninit(msg_dev);
    ka_task_destroy_workqueue(msg_dev->work_queue);
    ka_list_del(&msg_dev->list);
    vmng_kfree(msg_dev);
}

void vmng_msg_dfx_resq_time(u32 dev_id, u32 fid, struct vmng_msg_chan_rx *msg_chan, const char *errstr, u32 timeout)
{
    u32 resq_time;
 
    if (vmng_is_blk_chan(msg_chan->chan_type) == false) {
        resq_time = jiffies_to_msecs(jiffies - msg_chan->stamp);
        if (resq_time > timeout) {
            vmng_info("Get resq time. (dev_id=%u; fid=%u; resq_time=%ums; chan_type=%d; chan_id=%d; err=\"%s\")\n",
                dev_id, fid, resq_time, msg_chan->chan_type, msg_chan->chan_id, errstr);
        }
    }
}
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

#include "virtmng_public_def.h"
#include "virtmng_msg_pub.h"
#include "virtmnghost_msg.h"
#include "virtmnghost_vpc_unit.h"
#include "virtmnghost_msg_common.h"
#include "virtmnghost_vpc.h"

#include "ka_kernel_def_pub.h"

STATIC int vmngh_vpc_para_check(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vpc=%u)\n", dev_id, fid, vpc_type);
        return -EINVAL;
    }
    if (vpc_type >= VMNG_VPC_TYPE_MAX) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vpc=%u)\n", dev_id, fid, vpc_type);
        return -EINVAL;
    }
    return 0;
}

STATIC int vmngh_vpc_safe_mode_process(void *msg_chan_in, struct vmng_msg_chan_rx_proc_info *proc_info, 
    struct vmng_vpc_client *vpc_client, struct vmng_rx_msg_proc_info *client_proc_info)
{
    struct vmng_msg_chan_rx *msg_chan = (struct vmng_msg_chan_rx *)msg_chan_in;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    u32 dev_id = msg_dev->dev_id;
    u32 fid = msg_dev->fid;
    int ret;

    ret =  memcpy_s(msg_chan->sq_rx_safe_data, client_proc_info->in_data_len, proc_info->data, client_proc_info->in_data_len);
    if (ret != 0) {
        vmng_err("Memcpy_s failed. (dev_id=%u; fid=%u; vpc_type=%u; ret=%d)\n", dev_id, fid, vpc_client->vpc_type, ret);
        return ret;
    }
    client_proc_info->data = msg_chan->sq_rx_safe_data;

    ret = vpc_client->msg_recv(dev_id, fid, client_proc_info);
    if (ret != 0) {
        vmng_err("Client safe message process error. (dev_id=%u; fid=%u; chan_type=%u; ret=%d)\n",
                 dev_id, fid, msg_chan->chan_type, ret);
        return ret;
    }
    if (*(client_proc_info->real_out_len) > client_proc_info->out_data_len) {
        vmng_err("Real out len check failed. (dev_id=%u; fid=%u; chan_type=%u; real_out_len=%u; out_data_len=%u)\n",
            dev_id, fid, msg_chan->chan_type, *(client_proc_info->real_out_len), client_proc_info->out_data_len);
        return -EINVAL;
    }
    if (*(client_proc_info->real_out_len) == 0) {
        return 0;
    }
    ret = memcpy_s(proc_info->data, *(client_proc_info->real_out_len), client_proc_info->data,
        *(client_proc_info->real_out_len));
    if (ret != 0) {
        vmng_err("Memcpy_s failed. (dev_id=%u; fid=%u; vpc_type=%u; ret=%d)\n", dev_id, fid, vpc_client->vpc_type, ret);
        return ret;
    }
    return 0;
}

STATIC int vmngh_vpc_normal_mode_process(void *msg_chan_in, struct vmng_msg_chan_rx_proc_info *proc_info, 
    struct vmng_vpc_client *vpc_client, struct vmng_rx_msg_proc_info *client_proc_info)
{
    struct vmng_msg_chan_rx *msg_chan = (struct vmng_msg_chan_rx *)msg_chan_in;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    int ret;

    client_proc_info->data = proc_info->data;
    ret = vpc_client->msg_recv(msg_dev->dev_id, msg_dev->fid, client_proc_info);
    if (ret != 0) {
        vmng_err("Client normal message process error. (dev_id=%u; fid=%u; chan_type=%u; ret=%d)\n",
                 msg_dev->dev_id, msg_dev->fid, msg_chan->chan_type, ret);
        return ret;
    }
    return 0;
}
/* two paths to init msg_cluster, proactive allocating or passive allocated by admin */
STATIC int vmngh_vpc_msg_recv(void *msg_chan_in, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_msg_chan_rx *msg_chan = (struct vmng_msg_chan_rx *)msg_chan_in;
    struct vmng_msg_cluster *msg_cluster = (struct vmng_msg_cluster *)msg_chan->msg_cluster;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_cluster->msg_dev;
    struct vmng_rx_msg_proc_info client_proc_info;
    struct vmng_vpc_client *vpc_client = NULL;
    enum vmng_vpc_type vpc_type;
    enum vmng_msg_block_type blk_type;
    u32 dev_id;
    u32 fid;
    int ret = 0;

    dev_id = msg_dev->dev_id;
    fid = msg_dev->fid;
    client_proc_info.in_data_len = proc_info->in_data_len;
    client_proc_info.out_data_len = proc_info->out_data_len;
    client_proc_info.real_out_len = proc_info->real_out_len;

    if ((client_proc_info.in_data_len > VMNG_MSG_SQ_DATA_MAX_SIZE) || (client_proc_info.out_data_len > 
        VMNG_MSG_SQ_DATA_MAX_SIZE)) {
        vmng_err("Data length check failed. (dev_id=%u; fid=%u; in_data_len=%u; out_data_len=%u)\n", 
            dev_id, fid, client_proc_info.in_data_len, client_proc_info.out_data_len);
        return -EINVAL;
    }

    if (vmng_is_blk_chan(msg_cluster->chan_type) == true) {
        blk_type = vmng_msg_chan_type_to_block_type(msg_cluster->chan_type);
        vpc_type = vmng_blk_to_vpc_type(blk_type);
    } else if (vmng_is_vpc_chan(msg_cluster->chan_type) == true) {
        vpc_type = vmng_msg_chan_type_to_vpc_type(msg_cluster->chan_type);
    } else {
        vmng_err("chan_type is not match vpc. (dev_id=%u; fid=%u; chan_type=%u)\n",
                 dev_id, fid, msg_cluster->chan_type);
        return -EINVAL;
    }
    if (vpc_type >= VMNG_VPC_TYPE_MAX) {
        vmng_err("vpc_type is invalid. (dev_id=%u; fid=%u; chan_type=%u; vpc=%u)\n",
                 dev_id, fid, msg_cluster->chan_type, vpc_type);
        return -EINVAL;
    }

    vpc_client = &(msg_dev->vpc_clients[vpc_type]);
    if ((vpc_client == NULL) || (vpc_client->msg_recv == NULL)) {
        vmng_warn("vpc_client is invalid. (dev_id=%u; fid=%u; vpc_type=%u)\n", dev_id, fid, vpc_type);
        return -ENOSYS;
    }
    if (vpc_client->safe_mode == VMNG_VPC_SAFE_MODE) {
        ret = vmngh_vpc_safe_mode_process(msg_chan_in, proc_info, vpc_client, &client_proc_info);
    } else {
        ret = vmngh_vpc_normal_mode_process(msg_chan_in, proc_info, vpc_client, &client_proc_info);
    }
    return ret;
}

STATIC int vmngh_vpc_msg_send_para_check(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type,
    struct vmng_tx_msg_proc_info *tx_info)
{
    if (vmngh_vpc_para_check(dev_id, fid, vpc_type) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vpc_type=%u)\n", dev_id, fid, vpc_type);
        return -EINVAL;
    }
    if (vmng_msg_chan_tx_info_para_check(tx_info) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vpc_type=%u)\n", dev_id, fid, vpc_type);
        return -EINVAL;
    }
    return 0;
}

int vmngh_vpc_msg_send(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info,
    u32 timeout)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_dev *msg_dev = NULL;
    enum vmng_msg_chan_type chan_type;
    enum vmng_msg_block_type blk_type;
    const u32 opcode_idle = 0;
    bool flag = false;
    int ret;

    if (vmngh_vpc_msg_send_para_check(dev_id, fid, vpc_type, tx_info) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vpc_type=%u)\n", dev_id, fid, vpc_type);
        return -EINVAL;
    }

    if (timeout == VPC_BLK_MODE_TIMEOUT) {
        blk_type = vmng_vpc_to_blk_type(vpc_type);
        chan_type = vmng_block_type_to_msg_chan_type(blk_type);
        flag = vmng_is_blk_chan(chan_type);
    } else {
        chan_type = vmng_vpc_type_to_msg_chan_type(vpc_type);
        flag = vmng_is_vpc_chan(chan_type);
    }
    if (flag == false) {
        vmng_err("Call vmng_is_vpc_chan fail. (dev_id=%u; fid=%u; vpc=%u; chan_type=%u; timeout=%u)\n",
                 dev_id, fid, vpc_type, chan_type, timeout);
        return -EINVAL;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u, fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    msg_cluster = &(msg_dev->msg_cluster[chan_type]);

    ret = vmng_sync_msg_send(msg_cluster, tx_info, chan_type, opcode_idle, timeout);
    if (ret != 0) {
        vmng_err("Message send failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        return ret;
    }

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_vpc_msg_send);

STATIC int vmngh_alloc_vpc_msg_cluster(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type)
{
    struct vmng_msg_dev *msg_dev = NULL;
    enum vmng_msg_chan_type chan_type;
    struct vmng_msg_proc msg_proc;

    if (vmngh_vpc_para_check(dev_id, fid, vpc_type) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vpc_type=%u)\n", dev_id, fid, vpc_type);
        return -EINVAL;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Got message device failed. (dev_id=%u; fid=%u; vpc=%u)\n", dev_id, fid, vpc_type);
        return -EINVAL;
    }

    chan_type = vmng_vpc_type_to_msg_chan_type(vpc_type);
    if ((chan_type > VMNG_MSG_CHAN_TYPE_VPC) || (chan_type <= VMNG_MSG_CHAN_TYPE_COMMON)) {
        vmng_err("Got chan_type failed. (dev_id=%u; fid=%u; vpc=%u; chan_type=%u)\n", dev_id, fid, vpc_type, chan_type);
        return -EINVAL;
    }

    msg_proc.rx_recv_proc = vmngh_vpc_msg_recv;
    msg_proc.tx_finish_proc = NULL;

    return vmngh_alloc_msg_cluster(msg_dev, chan_type, &msg_proc);
}

STATIC void vmngh_free_vpc_msg_cluster(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type)
{
    struct vmng_msg_dev *msg_dev = NULL;
    enum vmng_msg_chan_type chan_type;

    if (vmngh_vpc_para_check(dev_id, fid, vpc_type) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vpc_type=%u)\n", dev_id, fid, vpc_type);
        return;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Got message device failed. (dev_id=%u; fid=%u; vpc=%u)\n", dev_id, fid, vpc_type);
        return;
    }

    chan_type = vmng_vpc_type_to_msg_chan_type(vpc_type);
    if (chan_type >= VMNG_MSG_CHAN_TYPE_MAX) {
        vmng_err("Got chan_type failed. (dev_id=%u; fid=%u; vpc=%u; chan_type=%u)\n", dev_id, fid, vpc_type, chan_type);
        return;
    }

    vmng_free_msg_cluster(msg_dev, chan_type);
}

int vmngh_alloc_all_vpc_msg_cluster(u32 dev_id, u32 fid)
{
    enum vmng_vpc_type vpc_type;
    enum vmng_vpc_type i;
    int ret;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    for (vpc_type = VMNG_VPC_TYPE_TEST; vpc_type < VMNG_VPC_TYPE_MAX; vpc_type++) {
        ret = vmngh_alloc_vpc_msg_cluster(dev_id, fid, vpc_type);
        if (ret != 0) {
            vmng_err("Alloc msg cluster failed. (dev_id=%u; fid=%u; vpc=%u)\n", dev_id, fid, vpc_type);
            goto FREE_VPC_MSG_CLUSTER;
        }
        vmng_debug("Alloc msg cluster success. (dev_id=%u; fid=%u; vpc=%u)\n", dev_id, fid, vpc_type);
    }

    return 0;

FREE_VPC_MSG_CLUSTER:
    for (i = VMNG_VPC_TYPE_TEST; i < vpc_type; i++) {
        vmngh_free_vpc_msg_cluster(dev_id, fid, i);
    }
    return ret;
}

void vmngh_free_all_vpc_msg_cluster(u32 dev_id, u32 fid)
{
    enum vmng_vpc_type vpc_type;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return;
    }
    for (vpc_type = VMNG_VPC_TYPE_TEST; vpc_type < VMNG_VPC_TYPE_MAX; vpc_type++) {
        vmngh_free_vpc_msg_cluster(dev_id, fid, vpc_type);
    }
}

STATIC int vmngh_alloc_block_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_block_type block_type)
{
    enum vmng_msg_chan_type chan_type;
    struct vmng_msg_proc msg_proc;

    chan_type = vmng_block_type_to_msg_chan_type(block_type);
    if (chan_type > VMNG_MSG_CHAN_TYPE_BLOCK) {
        vmng_err("chan_type is invalid. (dev_id=%u; fid=%u; block=%u; chan_type=%u)\n",
                 msg_dev->dev_id, msg_dev->fid, block_type,
            chan_type);
        return -EINVAL;
    }

    msg_proc.rx_recv_proc = vmngh_vpc_msg_recv;
    msg_proc.tx_finish_proc = vmng_msg_tx_finish_task;

    return vmngh_alloc_msg_cluster(msg_dev, chan_type, &msg_proc);
}

STATIC void vmngh_free_block_msg_cluster(struct vmng_msg_dev *msg_dev, enum vmng_msg_block_type block_type)
{
    enum vmng_msg_chan_type chan_type;

    chan_type = vmng_block_type_to_msg_chan_type(block_type);
    if ((chan_type > VMNG_MSG_CHAN_TYPE_BLOCK) || (chan_type <= VMNG_MSG_CHAN_TYPE_VPC)) {
        vmng_err("chan_type is invalid. (dev_id=%u; fid=%u; block_type=%u; chan_type=%u)\n",
            msg_dev->dev_id, msg_dev->fid, block_type, chan_type);
        return;
    }
    vmng_free_msg_cluster(msg_dev, chan_type);
}

int vmngh_alloc_all_block_msg_cluster(struct vmng_msg_dev *msg_dev)
{
    enum vmng_msg_block_type block_type;
    int ret;

    for (block_type = VMNG_MSG_BLOCK_TYPE_HDC; block_type < VMNG_MSG_BLOCK_TYPE_MAX; block_type++) {
        ret = vmngh_alloc_block_msg_cluster(msg_dev, block_type);
        if (ret != 0) {
            vmng_err("Alloc failed. (dev_id=%u; fid=%u; block=%u; ret=%d)\n",
                     msg_dev->dev_id, msg_dev->fid, block_type, ret);
            goto FREE_BLOCK_MSG_CLUSTER;
        }
    }

    return 0;

FREE_BLOCK_MSG_CLUSTER:
    for (block_type = VMNG_MSG_BLOCK_TYPE_HDC; block_type < VMNG_MSG_BLOCK_TYPE_MAX; block_type++) {
        vmngh_free_block_msg_cluster(msg_dev, block_type);
    }
    return ret;
}

void vmngh_free_all_block_msg_cluster(struct vmng_msg_dev *msg_dev)
{
    enum vmng_msg_block_type block_type;

    for (block_type = VMNG_MSG_BLOCK_TYPE_HDC; block_type < VMNG_MSG_BLOCK_TYPE_MAX; block_type++) {
        vmngh_free_block_msg_cluster(msg_dev, block_type);
    }
}

int vmngh_prepare_msg_chan(struct vmng_msg_dev *msg_dev)
{
    u32 dev_id = msg_dev->dev_id;
    u32 fid = msg_dev->fid;
    int ret;

    ret = vmngh_alloc_common_msg_cluster(msg_dev);
    if (ret != 0) {
        vmng_err("Alloc message cluster failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto DIRECTLY_OUT;
    }

    ret = vmngh_alloc_all_vpc_msg_cluster(dev_id, fid);
    if (ret != 0) {
        vmng_err("Alloc vpc msg cluster failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto FREE_COMMON_MSG_CLUSTER;
    }
    ret = vmngh_alloc_all_block_msg_cluster(msg_dev);
    if (ret != 0) {
        vmng_err("Alloc blk msg cluster failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto FREE_VPC_MSG_CLUSTER;
    }
    return ret;

FREE_VPC_MSG_CLUSTER:
    vmngh_free_all_vpc_msg_cluster(msg_dev->dev_id, msg_dev->fid);

FREE_COMMON_MSG_CLUSTER:
    vmngh_free_common_msg_cluster(msg_dev);

DIRECTLY_OUT:
    return ret;
}
KA_EXPORT_SYMBOL(vmngh_prepare_msg_chan);

void vmngh_unprepare_msg_chan(struct vmng_msg_dev *msg_dev)
{
    vmngh_free_all_block_msg_cluster(msg_dev);
    vmngh_free_all_vpc_msg_cluster(msg_dev->dev_id, msg_dev->fid);
    vmngh_free_common_msg_cluster(msg_dev);
}
KA_EXPORT_SYMBOL(vmngh_unprepare_msg_chan);

static int vmngh_vpc_register_client_common(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client, u32 safe_mode)
{
    struct vmng_msg_dev *msg_dev = NULL;

    if (vpc_client == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (vpc_client->vpc_type >= VMNG_VPC_TYPE_MAX) {
        vmng_err("Input parameter is error. (vpc_type=%u)\n", vpc_client->vpc_type);
        return -EINVAL;
    }
    if (vpc_client->msg_recv == NULL) {
        vmng_err("Input parameter is error. (vpc_type=%u)\n", vpc_client->vpc_type);
        return -EINVAL;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u, fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    msg_dev->vpc_clients[vpc_client->vpc_type].vpc_type = vpc_client->vpc_type;
    msg_dev->vpc_clients[vpc_client->vpc_type].init = vpc_client->init;
    msg_dev->vpc_clients[vpc_client->vpc_type].msg_recv = vpc_client->msg_recv;
    msg_dev->vpc_clients[vpc_client->vpc_type].safe_mode = safe_mode;
    return 0;
}

int vmngh_vpc_register_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmngh_vpc_register_client_common(dev_id, fid, vpc_client, VMNG_VPC_NORMAL_MODE);
}
KA_EXPORT_SYMBOL(vmngh_vpc_register_client);

int vmngh_vpc_register_client_safety(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmngh_vpc_register_client_common(dev_id, fid, vpc_client, VMNG_VPC_SAFE_MODE);
}
KA_EXPORT_SYMBOL(vmngh_vpc_register_client_safety);

int vmngh_vpc_unregister_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    struct vmng_msg_dev *msg_dev = NULL;

    if (vpc_client == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }

    if (vpc_client->vpc_type >= VMNG_VPC_TYPE_MAX) {
        vmng_err("Input parameter is error. (vpc_type=%u)\n", vpc_client->vpc_type);
        return -EINVAL;
    }
    if (vpc_client->msg_recv == NULL) {
        vmng_err("Input parameter is error. (vpc_type=%u)\n", vpc_client->vpc_type);
        return -EINVAL;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u, fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    msg_dev->vpc_clients[vpc_client->vpc_type].init = NULL;
    msg_dev->vpc_clients[vpc_client->vpc_type].msg_recv = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_vpc_unregister_client);

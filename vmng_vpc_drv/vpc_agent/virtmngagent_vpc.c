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

#include "virtmng_interface.h"
#include "virtmngagent_msg.h"
#include "virtmng_public_def.h"
#include "virtmng_msg_pub.h"
#include "virtmngagent_vpc.h"

#include "ka_kernel_def_pub.h"

int vmnga_msg_cluster_recv_vpc(void *msg_chan_in, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_msg_chan_rx *msg_chan = (struct vmng_msg_chan_rx *)msg_chan_in;
    struct vmng_msg_cluster *msg_cluster = (struct vmng_msg_cluster *)msg_chan->msg_cluster;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_cluster->msg_dev;
    struct vmng_rx_msg_proc_info client_proc_info;
    enum vmng_vpc_type vpc_type;
    enum vmng_msg_block_type blk_type;
    u32 dev_id;
    int ret;

    dev_id = msg_dev->dev_id;
    client_proc_info.data = proc_info->data;
    client_proc_info.in_data_len = proc_info->in_data_len;
    client_proc_info.out_data_len = proc_info->out_data_len;
    client_proc_info.real_out_len = proc_info->real_out_len;

    if (vmng_is_blk_chan(msg_cluster->chan_type) == true) {
        blk_type = vmng_msg_chan_type_to_block_type(msg_cluster->chan_type);
        vpc_type = vmng_blk_to_vpc_type(blk_type);
    } else if (vmng_is_vpc_chan(msg_cluster->chan_type) == true) {
        vpc_type = vmng_msg_chan_type_to_vpc_type(msg_cluster->chan_type);
    } else {
        vmng_err("Device is not vpc or blk chan. (dev=%u; chan_type=%u)\n", dev_id, msg_cluster->chan_type);
        return -EINVAL;
    }
    if (vpc_type >= VMNG_VPC_TYPE_MAX) {
        vmng_err("vpc_type is invalid. (dev_id=%u; chan_type=%u; vpc_type=%u)\n",
                 dev_id, msg_cluster->chan_type, vpc_type);
        return -EINVAL;
    }
    if (msg_dev->vpc_clients[vpc_type].msg_recv == NULL) {
        vmng_warn("msg_recv is invalid. (vpc_type=%u; dev_id=%u)\n", vpc_type, dev_id);
        return -ENOSYS;
    }
    ret = msg_dev->vpc_clients[vpc_type].msg_recv(dev_id, 0, &client_proc_info);
    if (ret != 0) {
        vmng_err("Client mesage recv error. (vpc_type=%u; dev_id=%u; ret=%d)\n", vpc_type, dev_id, ret);
        return ret;
    }
    return 0;
}

STATIC int vmnga_vpc_msg_send_para_check(u32 dev_id, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info)
{
    if (vpc_type >= VMNG_VPC_TYPE_MAX) {
        vmng_err("vpc_type is invalid. (dev_id=%u; vpc=%u)\n", dev_id, vpc_type);
        return -EINVAL;
    }
    if (vmng_msg_chan_tx_info_para_check(tx_info) != 0) {
        vmng_err("Tx information invalid. (dev_id=%u; vpc=%u)\n", dev_id, vpc_type);
        return -EINVAL;
    }
    return 0;
}

int vmnga_vpc_msg_send(u32 dev_id, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info,
    u32 timeout)
{
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_dev *msg_dev = NULL;
    enum vmng_msg_chan_type chan_type;
    enum vmng_msg_block_type blk_type;
    const u32 OPCODE_IDLE = 0;
    bool flag = false;
    int ret;

    if (vmnga_vpc_msg_send_para_check(dev_id, vpc_type, tx_info) != 0) {
        vmng_err("Send parameter check failed. (dev_id=%u; vpc=%u)\n", dev_id, vpc_type);
        return -EINVAL;
    }

    msg_dev = vmnga_get_msg_dev_by_id(dev_id);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (msg_dev->status != VMNG_MSG_DEV_ALIVE) {
        vmng_warn("Msg dev is not alive. (dev_id=%u;status=%d)\n", dev_id, msg_dev->status);
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
        vmng_err("Convert to msg_chan type failed. (dev_id=%u; vpc=%u; timeout=%u)\n", dev_id, vpc_type, timeout);
        return -EINVAL;
    }

    msg_cluster = &(msg_dev->msg_cluster[chan_type]);
    if (msg_cluster->status != VMNG_MSG_CLUSTER_STATUS_ENABLE) {
        vmng_err("Cluster status is invalid. (dev_id=%u; vpc=%u; status=%u)\n", dev_id, vpc_type, msg_cluster->status);
        return -EINVAL;
    }
    ret = vmng_sync_msg_send(msg_cluster, tx_info, chan_type, OPCODE_IDLE, timeout);
    return ret;
}
KA_EXPORT_SYMBOL(vmnga_vpc_msg_send);

int vmnga_vpc_register_client(u32 dev_id, const struct vmng_vpc_client *vpc_client)
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

    msg_dev = vmnga_get_msg_dev_by_id(dev_id);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    msg_dev->vpc_clients[vpc_client->vpc_type].vpc_type = vpc_client->vpc_type;
    msg_dev->vpc_clients[vpc_client->vpc_type].init = vpc_client->init;
    msg_dev->vpc_clients[vpc_client->vpc_type].msg_recv = vpc_client->msg_recv;

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_vpc_register_client);

int vmnga_vpc_unregister_client(u32 dev_id, const struct vmng_vpc_client *vpc_client)
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

    msg_dev = vmnga_get_msg_dev_by_id(dev_id);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    msg_dev->vpc_clients[vpc_client->vpc_type].init = NULL;
    msg_dev->vpc_clients[vpc_client->vpc_type].msg_recv = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_vpc_unregister_client);

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

#include "virtmngagent_msg_common.h"
#include "virtmng_msg_common.h"
#include "virtmng_interface.h"
#include "virtmng_public_def.h"
#include "virtmngagent_msg.h"
#include "virtmng_msg_pub.h"

#include "ka_kernel_def_pub.h"

static int (*g_vmnga_msg_common_recv_pcie_proc[])(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info) = {
    vmng_msg_recv_common_verfiy_info,
};

int vmnga_common_msg_send(u32 dev_id, enum vmng_msg_common_type cmn_type, struct vmng_tx_msg_proc_info *tx_info)
{
    enum vmng_msg_chan_type chan_type = VMNG_MSG_CHAN_TYPE_COMMON;
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_dev *msg_dev = NULL;
    int ret;

    if (cmn_type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; cmn_type=%u)\n", dev_id, cmn_type);
        return -EINVAL;
    }
    if (vmng_msg_chan_tx_info_para_check(tx_info) != 0) {
        vmng_err("Call vmng_msg_chan_tx_info_para_check failed. (dev_id=%u; cmn_type=%u)\n", dev_id, cmn_type);
        return -EINVAL;
    }

    msg_dev = vmnga_get_msg_dev_by_id(dev_id);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    msg_cluster = &(msg_dev->msg_cluster[chan_type]);
    if (msg_cluster->status != VMNG_MSG_CLUSTER_STATUS_ENABLE) {
        vmng_err("Cluster_status is invalid. (dev_id=%u; cmn=%u; cluster_status=%u)\n",
                 dev_id, cmn_type, msg_cluster->status);
        return -EINVAL;
    }

    ret = vmng_sync_msg_send(msg_cluster, tx_info, chan_type, cmn_type, VMNG_MSG_SYNC_WAIT_TIMEOUT_US);
    if (ret != 0) {
        vmng_err("Call vmng_sync_msg_send failed. (dev_id=%u; cmn_type=%u; ret=%d)\n", dev_id, cmn_type, ret);
        return ret;
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmnga_common_msg_send);

int vmnga_msg_cluster_recv_common(void *msg_chan_in,struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_msg_chan_rx *msg_chan = (struct vmng_msg_chan_rx *)msg_chan_in;
    struct vmng_msg_cluster *msg_cluster_in = (struct vmng_msg_cluster *)msg_chan->msg_cluster;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_cluster_in->msg_dev;
    struct vmng_rx_msg_proc_info client_proc_info;
    u32 common_msg_type;
    u32 dev_id;
    int ret;

    dev_id = msg_dev->dev_id;
    client_proc_info.data = proc_info->data;
    client_proc_info.in_data_len = proc_info->in_data_len;
    client_proc_info.out_data_len = proc_info->out_data_len;
    client_proc_info.real_out_len = proc_info->real_out_len;
    common_msg_type = proc_info->opcode_d2;
    if (common_msg_type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("common_type is invalid. (dev_id=%u; common_type=%u)\n", dev_id, common_msg_type);
        return -EINVAL;
    }

    if (msg_dev->common_msg.common_fun[common_msg_type] == NULL) {
        vmng_warn("msg_recv is NULL. (dev_id=%u; common_type=%u)\n", dev_id, common_msg_type);
        return -ENOSYS;
    }
    ret = msg_dev->common_msg.common_fun[common_msg_type](dev_id, 0, &client_proc_info);
    if (ret != 0) {
        vmng_err("Client message recv error. (dev_id=%u; cmn_type=%u; ret=%d)\n", dev_id, common_msg_type, ret);
        return ret;
    }
    return 0;
}

int vmnga_msg_common_recv_pcie(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    int ret;
    u32 cmd;

    cmd = *((u32 *)proc_info->data);
    if (cmd >= VMNG_P2V_MSG_COMMON_PCIE_CMD_MAX) {
        vmng_err("cmd is invalid. (dev_id=%u; cmd=%u)\n", dev_id, cmd);
        return -EINVAL;
    }
    ret = g_vmnga_msg_common_recv_pcie_proc[cmd](dev_id, fid, proc_info);
    if (ret != 0) {
        vmng_err("Common pcie recv error. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    return 0;
}

int vmnga_register_common_msg_client(u32 dev_id, const struct vmng_common_msg_client *msg_client)
{
    struct vmng_msg_dev *msg_dev = NULL;

    if (msg_client == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (msg_client->type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; client_type=%u)\n", dev_id, msg_client->type);
        return -EINVAL;
    }
    if (msg_client->common_msg_recv == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; client_type=%u)\n", dev_id, msg_client->type);
        return -EINVAL;
    }

    msg_dev = vmnga_get_msg_dev_by_id(dev_id);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    msg_dev->common_msg.common_fun[msg_client->type] = msg_client->common_msg_recv;

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_register_common_msg_client);

int vmnga_unregister_common_msg_client(u32 dev_id, const struct vmng_common_msg_client *msg_client)
{
    struct vmng_msg_dev *msg_dev = NULL;

    if (msg_client == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (msg_client->type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; client_type=%u)\n", dev_id, msg_client->type);
        return -EINVAL;
    }

    msg_dev = vmnga_get_msg_dev_by_id(dev_id);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    msg_dev->common_msg.common_fun[msg_client->type] = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_unregister_common_msg_client);

void vmnga_register_extended_common_msg_client(struct vmng_msg_dev *msg_dev)
{
    msg_dev->common_msg.common_fun[VMNG_MSG_COMMON_TYPE_EXTENSION] = vmnga_msg_common_recv_pcie;
}
KA_EXPORT_SYMBOL(vmnga_register_extended_common_msg_client);

void vmnga_unregister_extended_common_msg_client(struct vmng_msg_dev *msg_dev)
{
    msg_dev->common_msg.common_fun[VMNG_MSG_COMMON_TYPE_EXTENSION] = NULL;
}
KA_EXPORT_SYMBOL(vmnga_unregister_extended_common_msg_client);


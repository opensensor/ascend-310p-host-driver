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

#include "virtmng_msg_common.h"
#include "virtmng_extension.h"
#include "virtmng_public_def.h"
#include "virtmng_interface.h"
#include "virtmng_msg_pub.h"
#include "virtmnghost_vpc_unit.h"
#include "virtmnghost_msg.h"
#include "virtmnghost_msg_common.h"

#include "ka_kernel_def_pub.h"

STATIC int vmngh_common_msg_send_para_check(u32 dev_id, u32 fid, enum vmng_msg_common_type cmn_type,
    const struct vmng_tx_msg_proc_info *tx_info)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; cmn_type=%u)\n", dev_id, fid, cmn_type);
        return -EINVAL;
    }
    if (cmn_type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; cmn_type=%u)\n", dev_id, fid, cmn_type);
        return -EINVAL;
    }
    if (tx_info == NULL) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; cmn_type=%u)\n", dev_id, fid, cmn_type);
        return -EINVAL;
    }
    return 0;
}

int vmngh_common_msg_send(u32 dev_id, u32 fid, enum vmng_msg_common_type cmn_type,
    struct vmng_tx_msg_proc_info *tx_info)
{
    enum vmng_msg_chan_type chan_type = VMNG_MSG_CHAN_TYPE_COMMON;
    struct vmng_msg_cluster *msg_cluster = NULL;
    struct vmng_msg_dev *msg_dev = NULL;
    int ret;

    ret = vmngh_common_msg_send_para_check(dev_id, fid, cmn_type, tx_info);
    if (ret != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; cmn_type=%u)\n", dev_id, fid, cmn_type);
        return -EINVAL;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u, fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    msg_cluster = &(msg_dev->msg_cluster[chan_type]);
    if (msg_cluster->status != VMNG_MSG_CLUSTER_STATUS_ENABLE) {
        vmng_err("Cluster status error. (dev_id=%u; fid=%u; cmn=%u; status=%u)\n",
                 dev_id, fid, cmn_type, msg_cluster->status);
        return -EINVAL;
    }

    ret = vmng_sync_msg_send(msg_cluster, tx_info, chan_type, cmn_type, VMNG_MSG_SYNC_WAIT_TIMEOUT_US);
    if (ret != 0) {
        vmng_err("Message send failed. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        return ret;
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_common_msg_send);

STATIC int vmngh_msg_cluster_recv_common(void *msg_chan_in, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_msg_chan_rx *msg_chan = (struct vmng_msg_chan_rx *)msg_chan_in;
    struct vmng_msg_dev *msg_dev = (struct vmng_msg_dev *)msg_chan->msg_dev;
    struct vmng_rx_msg_proc_info client_proc_info;
    u32 common_msg_type, dev_id, fid;
    int ret;

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
    common_msg_type = proc_info->opcode_d2;
    if (common_msg_type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("common_type is invalid. (dev_id=%u; fid %u; common_type=%u)\n", dev_id, fid, common_msg_type);
        return -EINVAL;
    }

    ret = memcpy_s(msg_chan->sq_rx_safe_data, client_proc_info.in_data_len, proc_info->data, client_proc_info.in_data_len);
    if (ret != 0) {
        vmng_err("Memcpy_s failed. (dev_id=%u; fid=%u; common_type=%u; ret=%d)\n", dev_id, fid, common_msg_type, ret);
        return ret;
    }
    client_proc_info.data = msg_chan->sq_rx_safe_data;

    if (msg_dev->common_msg.common_fun[common_msg_type] == NULL) {
        vmng_err("msg_recv is error. (dev_id=%u; fid %u; common_type=%u)\n", dev_id, fid, common_msg_type);
        return -ENOSYS;
    }

    ret = msg_dev->common_msg.common_fun[common_msg_type](dev_id, fid, &client_proc_info);
    if (ret != 0) {
        vmng_err("Message recv error. (dev_id=%u; fid=%u; common_type=%u; ret=%d)\n",
                 dev_id, fid, common_msg_type, ret);
        return ret;
    }
    if (*client_proc_info.real_out_len > client_proc_info.out_data_len) {
        vmng_err("Real out len check failed. (dev_id=%u; fid=%u; common_type=%u; real_out_len=%u; out_data_len=%u)\n",
            dev_id, fid, common_msg_type, *client_proc_info.real_out_len, client_proc_info.out_data_len);
    }
    ret = memcpy_s(proc_info->data, *client_proc_info.real_out_len, client_proc_info.data, *client_proc_info.real_out_len);
    if (ret != 0) {
        vmng_err("Memcpy_s failed. (dev_id=%u; fid=%u; common_type=%u; ret=%d)\n", dev_id, fid, common_msg_type, ret);
        return ret;
    }
    return 0;
}

int vmngh_alloc_common_msg_cluster(struct vmng_msg_dev *msg_dev)
{
    enum vmng_msg_chan_type chan_type = VMNG_MSG_CHAN_TYPE_COMMON;
    struct vmng_msg_proc msg_proc;

    msg_proc.rx_recv_proc = vmngh_msg_cluster_recv_common;
    msg_proc.tx_finish_proc = NULL;

    return vmngh_alloc_msg_cluster(msg_dev, chan_type, &msg_proc);
}

void vmngh_free_common_msg_cluster(struct vmng_msg_dev *msg_dev)
{
    vmng_free_msg_cluster(msg_dev, VMNG_MSG_CHAN_TYPE_COMMON);
}

static int (*g_vmngh_msg_common_recv_pcie_proc[VMNG_V2P_MSG_COMMON_PCIE_CMD_MAX])(u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info) = {
        vmng_msg_recv_common_verfiy_info,
    };

STATIC int vmngh_msg_common_recv_extension(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    int ret;
    u32 cmd;

    cmd = *((u32 *)proc_info->data);
    if (cmd >= VMNG_V2P_MSG_COMMON_PCIE_CMD_MAX) {
        vmng_err("cmd is invalid. (dev_id=%u; fid=%u; cmd=%u)\n", dev_id, fid, cmd);
        return -EINVAL;
    }
    ret = g_vmngh_msg_common_recv_pcie_proc[cmd](dev_id, fid, proc_info);
    if (ret != 0) {
        vmng_err("Receive proc error. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        return ret;
    }
    return 0;
}

/* when register and unregister, vdev create must be ok.
 * because it need msg_dev to store client, just judge top_half_probe_ok.
 */
int vmngh_register_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    struct vmng_msg_dev *msg_dev = NULL;

    if (msg_client == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (msg_client->type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, msg_client->type);
        return -EINVAL;
    }
    if (msg_client->common_msg_recv == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, msg_client->type);
        return -EINVAL;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u, fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    msg_dev->common_msg.common_fun[msg_client->type] = msg_client->common_msg_recv;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_register_common_msg_client);

int vmngh_unregister_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    struct vmng_msg_dev *msg_dev = NULL;

    if (msg_client == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (msg_client->type >= VMNG_MSG_COMMON_TYPE_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, msg_client->type);
        return -EINVAL;
    }

    msg_dev = vmngh_get_msg_dev_by_id(dev_id, fid);
    if (msg_dev == NULL) {
        vmng_err("Get msg_dev failed. (dev_id=%u, fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    msg_dev->common_msg.common_fun[msg_client->type] = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_unregister_common_msg_client);

void vmngh_register_extended_common_msg_client(struct vmng_msg_dev *msg_dev)
{
    msg_dev->common_msg.common_fun[VMNG_MSG_COMMON_TYPE_EXTENSION] = vmngh_msg_common_recv_extension;
}
KA_EXPORT_SYMBOL(vmngh_register_extended_common_msg_client);

void vmngh_unregister_extended_common_msg_client(struct vmng_msg_dev *msg_dev)
{
    msg_dev->common_msg.common_fun[VMNG_MSG_COMMON_TYPE_EXTENSION] = NULL;
}
KA_EXPORT_SYMBOL(vmngh_unregister_extended_common_msg_client);

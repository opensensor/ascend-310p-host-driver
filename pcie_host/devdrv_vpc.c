/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#include <linux/delay.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#include "vpc_interface.h"
#include "devdrv_interface.h"
#include "devdrv_util.h"
#include "devdrv_dma.h"
#include "devdrv_pci.h"
#include "devdrv_ctrl.h"
#include "devdrv_mem_alloc.h"
#include "devdrv_vpc.h"

STATIC struct devdrv_dma_dev *devdrv_get_dma_dev_by_devid(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_dma_dev *dma_dev = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (devid=%u)\n", dev_id);
        return NULL;
    }

    dma_dev = pci_ctrl->dma_dev;
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (devid=%u)\n", dev_id);
        return NULL;
    }

    return dma_dev;
}

STATIC struct devdrv_dma_channel *devdrv_vpc_get_dma_chan_by_id(struct devdrv_dma_dev *dma_dev, u32 chan_id)
{
    u32 i;

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        if (chan_id == dma_dev->dma_chan[i].chan_id) {
            return &dma_dev->dma_chan[i];
        }
    }

    return NULL;
}

int devdrv_vpc_dma_iova_addr_check(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_dma_sq_node *sq_node,
    enum devdrv_dma_direction direction)
{
    u64 dma_src_start_addr;
    u64 dma_src_end_addr;
    u64 src_addr;
    u64 dst_addr;
    u32 dma_len;

    /* only vf's mdev pm need check, others no need check */
    if ((pci_ctrl->env_boot_mode != DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT) &&
        (pci_ctrl->env_boot_mode != DEVDRV_MDEV_VF_PM_BOOT)) {
        return 0;
    }

    if (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) {
        return 0;
    }

    if (pci_ctrl->iova_range->init_flag != DEVDRV_DMA_IOVA_RANGE_INIT) {
        return 0;
    }

    dma_src_start_addr = pci_ctrl->iova_range->start_addr;
    dma_src_end_addr = pci_ctrl->iova_range->end_addr;
    src_addr = ((u64)sq_node->src_addr_h << DEVDRV_ADDR_SHIFT_32) | sq_node->src_addr_l;
    dst_addr = ((u64)sq_node->dst_addr_h << DEVDRV_ADDR_SHIFT_32) | sq_node->dst_addr_l;
    dma_len = sq_node->length;

    if ((direction == DEVDRV_DMA_HOST_TO_DEVICE) || (sq_node->opcode == DEVDRV_DMA_READ)) {
        if (((src_addr >= dma_src_start_addr) && (src_addr < dma_src_end_addr)) ||
            ((src_addr < dma_src_start_addr) && ((src_addr + dma_len > dma_src_start_addr) ||
            (src_addr + dma_len <= src_addr)))) {
            devdrv_err_spinlock("DMA H2D, sq src addr check fail. (devid=%u)\n", pci_ctrl->dev_id);
            return -EINVAL;
        }
    } else if ((direction == DEVDRV_DMA_DEVICE_TO_HOST) || (sq_node->opcode == DEVDRV_DMA_WRITE)) {
        if (((dst_addr >= dma_src_start_addr) && (dst_addr < dma_src_end_addr)) ||
            ((dst_addr < dma_src_start_addr) && ((dst_addr + dma_len > dma_src_start_addr) ||
            (dst_addr + dma_len <= dst_addr)))) {
            devdrv_err_spinlock("DMA D2H, sq dst addr check fail. (devid=%u)\n", pci_ctrl->dev_id);
            return -EINVAL;
        }
    } else {
        devdrv_err_spinlock("DMA direction is invalid. (devid=%u)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }

    return 0;
}

STATIC int devdrv_vpc_dma_fill_sq_desc_and_submit(u32 dev_id, struct devdrv_vpc_cmd_sq_submit *sq_cmd)
{
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_channel *dma_chan = NULL;
    struct devdrv_dma_copy_para para = {0};
    struct devdrv_asyn_dma_para_info asyn_info = {0};
    u32 node_cnt = sq_cmd->node_cnt;
    u32 chan_id = sq_cmd->chan_id;
    int sq_idle_bd_cnt;
    int ret;

    dma_dev = devdrv_get_dma_dev_by_devid(dev_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    if (node_cnt > DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT) {
        devdrv_err("Node cnt is invalid. (devid=%u, chan_id=%u, node_cnt=%u)\n", dev_id, chan_id, node_cnt);
        return -ENOSPC;
    }

    dma_chan = devdrv_vpc_get_dma_chan_by_id(dma_dev, chan_id);
    if (dma_chan == NULL) {
        devdrv_err("Chan id is invalid. (devid=%u, chan_id=%u)\n", dev_id, chan_id);
        return -EINVAL;
    }

    sq_idle_bd_cnt = devdrv_dma_get_sq_idle_bd_cnt(dma_chan);
    if ((sq_idle_bd_cnt < 0) || ((u32)sq_idle_bd_cnt < node_cnt)) {
        devdrv_warn("No space. (dev_id=%u; chan_id=%u; idle_bd=%d; need=%u)\n",
            dev_id, chan_id, sq_idle_bd_cnt, node_cnt);
        return -ENOSPC;
    }

    para.instance = sq_cmd->instance;
    para.type = sq_cmd->type;
    para.wait_type = sq_cmd->wait_type;
    para.copy_type = DEVDRV_DMA_ASYNC;
    para.pa_va_flag = DEVDRV_DMA_VA_COPY;
    if (sq_cmd->asyn_info_flag == DEVDRV_VPC_DMA_ASYN_INFO_IS_NULL) {
        para.asyn_info = NULL;
    } else {
        para.asyn_info = &asyn_info;
        para.asyn_info->trans_id = sq_cmd->asyn_info.trans_id;
        para.asyn_info->remote_msi_vector = sq_cmd->asyn_info.remote_msi_vector;
        para.asyn_info->interrupt_and_attr_flag = sq_cmd->asyn_info.interrupt_and_attr_flag;
        para.asyn_info->priv = sq_cmd->asyn_info.priv;
        para.asyn_info->finish_notify = sq_cmd->asyn_info.finish_notify;
    }

    ret = devdrv_dma_chan_copy(dev_id, dma_chan, &sq_cmd->dma_node[0], node_cnt, &para);
    if (ret != 0) {
        devdrv_err("Dma copy failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int devdrv_vpc_dma_sqcq_head_update(u32 dev_id, struct devdrv_vpc_cmd_sqcq_update *update_cmd)
{
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_channel *dma_chan = NULL;
    u32 chan_id = update_cmd->chan_id;
    u32 cq_head;
    u32 sq_head;

    dma_dev = devdrv_get_dma_dev_by_devid(dev_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    dma_chan = devdrv_vpc_get_dma_chan_by_id(dma_dev, chan_id);
    if (dma_chan == NULL) {
        devdrv_err("Chan id is invalid. (devid=%u, chan_id=%u)\n", dev_id, chan_id);
        return -EINVAL;
    }

    cq_head = update_cmd->cq_head;
    sq_head = update_cmd->sq_head;
    if ((sq_head >= dma_chan->sq_depth) || (cq_head >= dma_chan->cq_depth)) {
        devdrv_err("Sq head or cq head is invalid. (devid=%u, sq_head=%u, cq_head=%u)\n", dev_id, sq_head, cq_head);
        return -EINVAL;
    }

    dma_chan->sq_head = sq_head;
    devdrv_set_dma_cq_head(dma_chan->io_base, cq_head);

    return 0;
}

STATIC int devdrv_vpc_dma_init_and_alloc_sq_cq(u32 dev_id, u32 chan_id)
{
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_channel *dma_chan = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    dma_dev = pci_ctrl->dma_dev;
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }
    (void)devdrv_sriov_dma_init_chan(dma_dev);

    dma_chan = devdrv_vpc_get_dma_chan_by_id(dma_dev, chan_id);
    if (dma_chan == NULL) {
        devdrv_err("Chan id is invalid. (devid=%u, chan_id=%u)\n", dev_id, chan_id);
        return -EINVAL;
    }

    ret = devdrv_alloc_dma_sq_cq(dma_chan);
    if (ret != 0) {
        devdrv_err("Alloc dma_sq_cq failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return -EINVAL;
    }
    pci_ctrl->shr_para->sq_desc_dma = dma_chan->sq_desc_dma;

    return 0;
}

STATIC int devdrv_vpc_free_dma_sq_cq(u32 dev_id, u32 chan_id)
{
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_channel *dma_chan = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    dma_dev = pci_ctrl->dma_dev;
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    dma_chan = devdrv_vpc_get_dma_chan_by_id(dma_dev, chan_id);
    if (dma_chan == NULL) {
        devdrv_err("Chan id is invalid. (devid=%u, chan_id=%u)\n", dev_id, chan_id);
        return -EINVAL;
    }

    devdrv_free_dma_sq_cq(dma_chan);
    pci_ctrl->shr_para->sq_desc_dma = 0;

    return 0;
}

STATIC int devdrv_vpc_dma_link_prepare(u32 dev_id, struct devdrv_vpc_cmd_dma_desc_info *dma_info)
{
    struct devdrv_dma_prepare *dma_prepare = NULL;
    u32 node_cnt = dma_info->node_cnt;

    if (node_cnt > DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT) {
        devdrv_err("Dma node cnt is invalid. (devid=%u; node_cnt=%u)\n", dev_id, node_cnt);
        return -EINVAL;
    }

    dma_prepare = devdrv_dma_link_prepare(dev_id, dma_info->type, &dma_info->dma_node[0],
        node_cnt, dma_info->fill_status);
    if (dma_prepare == NULL) {
        devdrv_err("Get dma_link_prepare failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    dma_info->dma_desc_info.cq_dma_addr = dma_prepare->cq_dma_addr;
    dma_info->dma_desc_info.cq_size = dma_prepare->cq_size;
    dma_info->dma_desc_info.sq_dma_addr = dma_prepare->sq_dma_addr;
    dma_info->dma_desc_info.sq_size = dma_prepare->sq_size;

    return 0;
}

STATIC int devdrv_vpc_dma_link_free(u32 dev_id, struct devdrv_vpc_cmd_dma_desc_info *dma_info)
{
    struct devdrv_dma_prepare dma_prepare = {0};
    int ret;

    dma_prepare.devid = dev_id;
    dma_prepare.cq_dma_addr = dma_info->dma_desc_info.cq_dma_addr;
    dma_prepare.cq_size = dma_info->dma_desc_info.cq_size;
    dma_prepare.sq_dma_addr = dma_info->dma_desc_info.sq_dma_addr;
    dma_prepare.sq_size = dma_info->dma_desc_info.sq_size;

    ret = devdrv_dma_link_free(&dma_prepare);
    if (ret != 0) {
        devdrv_err("Set dma_link_free failed. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    return 0;
}

STATIC int devdrv_vpc_dma_prepare_alloc_sq_addr(u32 dev_id, struct devdrv_vpc_cmd_dma_desc_info *dma_info)
{
    struct devdrv_dma_prepare *dma_prepare = NULL;
    u32 node_cnt = dma_info->node_cnt;
    int ret;

    dma_prepare = devdrv_kzalloc(sizeof(struct devdrv_dma_prepare), GFP_KERNEL | __GFP_ACCOUNT);
    if (dma_prepare == NULL) {
        devdrv_err("Alloc dma_prepare failed. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }

    ret = devdrv_dma_prepare_alloc_sq_addr(dev_id, node_cnt, dma_prepare);
    if (ret != 0) {
        devdrv_kfree(dma_prepare);
        devdrv_err("devdrv_dma_prepare_alloc_sq_addr fail. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    dma_info->dma_desc_info.sq_dma_addr = dma_prepare->sq_dma_addr;
    dma_info->dma_desc_info.sq_size = dma_prepare->sq_size;
    dma_info->dma_desc_info.cq_dma_addr = (~(dma_addr_t)0);
    dma_info->dma_desc_info.cq_size = 0;

    return 0;
}

STATIC int devdrv_vpc_dma_prepare_free_sq_addr(u32 dev_id, struct devdrv_vpc_cmd_dma_desc_info *dma_info)
{
    struct devdrv_dma_prepare dma_prepare = {0};

    dma_prepare.devid = dev_id;
    dma_prepare.sq_dma_addr = dma_info->dma_desc_info.sq_dma_addr;
    dma_prepare.sq_size = dma_info->dma_desc_info.sq_size;

    devdrv_dma_prepare_free_sq_addr(dev_id, &dma_prepare);

    return 0;
}

STATIC int devdrv_vpc_dma_fill_desc_of_sq(u32 dev_id, struct devdrv_vpc_cmd_dma_desc_info *dma_info)
{
    struct devdrv_dma_prepare dma_prepare = {0};
    u32 node_cnt = dma_info->node_cnt;
    int ret;

    if (node_cnt > DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT) {
        devdrv_err("Dma node cnt is invalid. (devid=%u; node_cnt=%u)\n", dev_id, node_cnt);
        return -EINVAL;
    }

    dma_prepare.sq_dma_addr = dma_info->dma_desc_info.sq_dma_addr;
    dma_prepare.sq_size = dma_info->dma_desc_info.sq_size;
    ret = devdrv_dma_fill_desc_of_sq(dev_id, &dma_prepare, &dma_info->dma_node[0], node_cnt, dma_info->fill_status);
    if (ret != 0) {
        devdrv_err("Fill sq node failed. (dev_id=%u)\n", dev_id);
    }

    return ret;
}

int devdrv_vpc_msg_send(u32 dev_id, u32 cmd_type, struct devdrv_vpc_msg *vpc_msg, u32 data_len, u32 timeout)
{
#ifdef CFG_FEATURE_SRIOV
    struct vmng_tx_msg_proc_info tx_info;
    u32 retry_time = DEVDRV_VPC_RETRY_TIMES;
    int ret = 0;

    vpc_msg->cmd = cmd_type;
    vpc_msg->error_code = -1;

    tx_info.data = vpc_msg;
    tx_info.in_data_len = data_len;
    tx_info.out_data_len = sizeof(struct devdrv_vpc_msg);
    tx_info.real_out_len = 0;

    do {
        ret = vpc_msg_send(dev_id, VPC_VM_FID, VMNG_VPC_TYPE_PCIE, &tx_info, timeout);
        if (ret != -ENOSPC) {
            break;
        }

        usleep_range(100, 200);
        retry_time--;
    } while (retry_time != 0);

    return ret;
#else
    return 0;
#endif
}

STATIC int devdrv_vpc_msg_para_check(u32 dev_id, u32 fid, const struct vmng_rx_msg_proc_info *proc_info)
{
    u32 in_data_len;
    u32 out_data_len;
    u32 max_len = sizeof(struct devdrv_dma_node) * DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT + sizeof(struct devdrv_vpc_msg);

    if ((dev_id >= MAX_DEV_CNT) || (fid != 0)) {
        devdrv_err("Vpc_msg_para_check. (devid=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    if ((proc_info == NULL) || (proc_info->real_out_len == NULL) || (proc_info->data == NULL)) {
        devdrv_err("Vpc_msg_para_check. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    in_data_len = proc_info->in_data_len;
    out_data_len = proc_info->out_data_len;

    if ((in_data_len < sizeof(struct devdrv_vpc_msg)) || (in_data_len >= max_len)) {
        devdrv_err("Vpc_msg_in_data_len_check. (devid=%u; fid=%u; in_data_len=%u)\n", dev_id, fid, in_data_len);
        return -EINVAL;
    }

    if ((out_data_len < sizeof(struct devdrv_vpc_msg)) || (out_data_len >= max_len)) {
        devdrv_err("Vpc_msg_in_data_len_check. (devid=%u; fid=%u; in_data_len=%u)\n", dev_id, fid, out_data_len);
        return -EINVAL;
    }
    return 0;
}

STATIC int devdrv_vpc_msg_handle(u32 dev_id, u32 cmd, union devdrv_vpc_cmd *cmd_data, int *is_need_data_to_vm)
{
    int ret = 0;

    switch (cmd) {
        case DEVDRV_VPC_MSG_TYPE_SQ_SUBMIT:
            ret = devdrv_vpc_dma_fill_sq_desc_and_submit(dev_id, &cmd_data->sq_cmd);
            break;
        case DEVDRV_VPC_MSG_TYPE_SQCQ_HEAD_UPDATE:
            ret = devdrv_vpc_dma_sqcq_head_update(dev_id, &cmd_data->update_cmd);
            break;
        case DEVDRV_VPC_MSG_TYPE_DMA_INIT_AND_ALLOC_SQCQ:
            ret = devdrv_vpc_dma_init_and_alloc_sq_cq(dev_id, cmd_data->dma_init.chan_id);
            break;
        case DEVDRV_VPC_MSG_TYPE_FREE_DMA_SQCQ:
            ret = devdrv_vpc_free_dma_sq_cq(dev_id, cmd_data->dma_init.chan_id);
            break;
        case DEVDRV_VPC_MSG_TYPE_DMA_LINK_PREPARE:
            ret = devdrv_vpc_dma_link_prepare(dev_id, &cmd_data->dma_info);
            *is_need_data_to_vm = DEVDRV_NEED_ACK_DATA_TO_VM;
            break;
        case DEVDRV_VPC_MSG_TYPE_DMA_LINK_FREE:
            ret = devdrv_vpc_dma_link_free(dev_id, &cmd_data->dma_info);
            break;
        case DEVDRV_VPC_MSG_TYPE_DMA_LINK_SQ_ALLOC:
            ret = devdrv_vpc_dma_prepare_alloc_sq_addr(dev_id, &cmd_data->dma_info);
            *is_need_data_to_vm = DEVDRV_NEED_ACK_DATA_TO_VM;
            break;
        case DEVDRV_VPC_MSG_TYPE_DMA_LINK_SQ_FREE:
            ret = devdrv_vpc_dma_prepare_free_sq_addr(dev_id, &cmd_data->dma_info);
            break;
        case DEVDRV_VPC_MSG_TYPE_DMA_SQ_DESC_FILL:
            ret = devdrv_vpc_dma_fill_desc_of_sq(dev_id, &cmd_data->dma_info);
            break;
        default:
            devdrv_err("Vpc msg type is illegal. (cmd=%u)\n", cmd);
            ret = -EINVAL;
            break;
    }

    return ret;
}

STATIC int devdrv_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devdrv_vpc_msg *msg = NULL;
    int is_need_data_to_vm = 0;
    int ret = -1;

    if (devdrv_vpc_msg_para_check(dev_id, fid, proc_info) != 0) {
        devdrv_err("Vpc msg check is fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    msg = (struct devdrv_vpc_msg *)proc_info->data;
    if (msg == NULL) {
        devdrv_err("Vpc msg is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_vpc_msg_handle(dev_id, msg->cmd, &msg->cmd_data, &is_need_data_to_vm);
    if (ret != 0) {
        devdrv_err("Vpc msg handle fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }

    if (is_need_data_to_vm == DEVDRV_NEED_ACK_DATA_TO_VM) {
        *(proc_info->real_out_len) = DEVDRV_VPC_MSG_ACK_TO_VM_DEFAULT_LEN;
    } else {
        *(proc_info->real_out_len) = DEVDRV_VPC_MSG_ACK_TO_VM_MIN_LEN;
    }
    msg->error_code = ret;
    return 0;
}

struct vmng_vpc_client devdrv_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_PCIE,
    .init = NULL,
    .msg_recv = devdrv_vpc_msg_recv,
};

int devdrv_vpc_client_init(u32 devid)
{
#ifdef CFG_FEATURE_SRIOV
    int ret;

    ret = vpc_register_client(devid, 0, &devdrv_vpc_client);
    if (ret != 0) {
        devdrv_err("Calling vpc_register_client fail. (ret=%d)\n", ret);
        return ret;
    }

    devdrv_info("devdrv_vpc_client init success\n");
#endif
    return 0;
}
EXPORT_SYMBOL(devdrv_vpc_client_init);

int devdrv_vpc_client_uninit(u32 devid)
{
#ifdef CFG_FEATURE_SRIOV
    int ret;

    ret = vpc_unregister_client(devid, 0, &devdrv_vpc_client);
    if (ret != 0) {
        devdrv_err("Calling vpc_unregister_client fail. (ret=%d)\n", ret);
        return ret;
    }

    devdrv_info("devdrv_vpc_client uninit success\n");
#endif
    return 0;
}
EXPORT_SYMBOL(devdrv_vpc_client_uninit);

STATIC int devdrv_mdev_dma_iova_addr_range_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->iova_range = devdrv_kzalloc(sizeof(struct devdrv_dma_iova_addr_range), GFP_KERNEL);
    if (pci_ctrl->iova_range == NULL) {
        devdrv_err("Alloc iova_range fail. (devid=%u)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }

    pci_ctrl->iova_range->init_flag = DEVDRV_DMA_IOVA_RANGE_UNINIT;
    pci_ctrl->iova_range->start_addr = 0;
    pci_ctrl->iova_range->end_addr = 0;

    return 0;
}

STATIC void devdrv_mdev_dma_iova_addr_range_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->iova_range == NULL) {
        return;
    }

    devdrv_kfree(pci_ctrl->iova_range);
    pci_ctrl->iova_range = NULL;
}

int devdrv_mdev_pm_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    if ((pci_ctrl->env_boot_mode != DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT) &&
        (pci_ctrl->env_boot_mode != DEVDRV_MDEV_VF_PM_BOOT)) {
        return 0;
    }

    ret = devdrv_mdev_dma_iova_addr_range_init(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Iova addr range init fail. (dev_id=%u)\n", pci_ctrl->dev_id);
        return ret;
    }

    devdrv_info("Calling mdev_pm_init success. (dev_id=%u)\n", pci_ctrl->dev_id);
    return 0;
}

int devdrv_mdev_vm_init(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifdef CFG_FEATURE_SRIOV
    struct vmng_vpc_unit vpc_info;
    int ret;

    if ((pci_ctrl->env_boot_mode != DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT) &&
        (pci_ctrl->env_boot_mode != DEVDRV_MDEV_VF_VM_BOOT)) {
        return 0;
    }

    vpc_info.pdev = pci_ctrl->pdev;
    vpc_info.dev_id = pci_ctrl->dev_id;
    vpc_info.fid = 0;

    vpc_info.mmio.bar0_base = pci_ctrl->mdev_rsv_mem_phy_base;
    vpc_info.mmio.bar0_size = pci_ctrl->mdev_rsv_mem_phy_size;
    vpc_info.mmio.bar2_base = 0;
    vpc_info.mmio.bar2_size = 0;
    vpc_info.mmio.bar4_base = 0;
    vpc_info.mmio.bar4_size = 0;

    vpc_info.msix_info.entries = &pci_ctrl->msix_ctrl.entries[0];
    vpc_info.msix_info.msix_irq_num = DEVDRV_VPC_MSI_NUM;
    vpc_info.msix_info.msix_irq_offset = DEVDRV_VPC_MSI_BASE;

    ret = vmng_vpc_init(&vpc_info, SERVER_TYPE_VM_PCIE);
    if (ret != 0) {
        devdrv_err("Calling mdev_vm_init fail. (dev_id=%u;ret=%d)\n", pci_ctrl->dev_id, ret);
        return ret;
    }

    devdrv_info("Calling mdev_vm_init success. (dev_id=%u)\n", pci_ctrl->dev_id);
#endif
    return 0;
}

int devdrv_mdev_vpc_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    ret = devdrv_mdev_pm_init(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Calling mdev_pm_init fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_mdev_vm_init(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Calling mdev_vm_init fail. (ret=%d)\n", ret);
        return ret;
    }

    devdrv_info("Calling mdev_vpc_init success\n");
    return 0;
}

void devdrv_mdev_pm_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    if ((pci_ctrl->env_boot_mode != DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT) &&
        (pci_ctrl->env_boot_mode != DEVDRV_MDEV_VF_PM_BOOT)) {
        return;
    }

    devdrv_mdev_dma_iova_addr_range_uninit(pci_ctrl);
    devdrv_info("Calling mdev_pm_uninit success. (dev_id=%u)\n", pci_ctrl->dev_id);
}

void devdrv_mdev_vm_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifdef CFG_FEATURE_SRIOV
    struct vmng_vpc_unit vpc_info = {0};
    int ret;

    if ((pci_ctrl->env_boot_mode != DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT) &&
        (pci_ctrl->env_boot_mode != DEVDRV_MDEV_VF_VM_BOOT)) {
        return;
    }

    vpc_info.pdev = pci_ctrl->pdev;
    vpc_info.dev_id = pci_ctrl->dev_id;
    vpc_info.fid = 0;

    ret = vmng_vpc_uninit(&vpc_info, SERVER_TYPE_VM_PCIE);
    if (ret != 0) {
        devdrv_err("Calling vmng_vpc_uninit fail. (ret=%d)\n", ret);
        return;
    }
    devdrv_info("Calling mdev_vm_uninit success\n");
#endif
}

void devdrv_mdev_vpc_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    devdrv_mdev_pm_uninit(pci_ctrl);
    devdrv_mdev_vm_uninit(pci_ctrl);

    return;
}

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

#ifndef DEVDRV_VPC_H
#define DEVDRV_VPC_H
#include "devdrv_interface.h"
#include "devdrv_pci.h"

#define DEVDRV_VPC_RETRY_TIMES 100
#define DEVDRV_VPC_MSI_BASE 128
#define DEVDRV_VPC_MSI_NUM 128

#define DEVDRV_VPC_DMA_ASYN_INFO_NOT_NULL 0
#define DEVDRV_VPC_DMA_ASYN_INFO_IS_NULL  1
struct devdrv_vpc_cmd_sq_submit {
    u32 dev_id;
    u32 chan_id;
    int instance;
    enum devdrv_dma_data_type type;
    int wait_type;
    int pa_va_flag;
    struct devdrv_asyn_dma_para_info asyn_info;
    int asyn_info_flag;
    u32 node_cnt;
    struct devdrv_dma_node dma_node[];
};

struct devdrv_vpc_cmd_sqcq_update {
    u32 dev_id;
    u32 chan_id;
    u32 cq_head;
    u32 sq_head;
};

struct devdrv_vpc_cmd_dma_init {
    u32 dev_id;
    u32 chan_id;
};

struct devdrv_vpc_cmd_dma_desc_info {
    u32 dev_id;
    struct devdrv_dma_desc_info dma_desc_info;
    enum devdrv_dma_data_type type;
    u32 fill_status;
    u32 node_cnt;
    struct devdrv_dma_node dma_node[];
};

union devdrv_vpc_cmd {
    struct devdrv_vpc_cmd_sq_submit sq_cmd;
    struct devdrv_vpc_cmd_sqcq_update update_cmd;
    struct devdrv_vpc_cmd_dma_init dma_init;
    struct devdrv_vpc_cmd_dma_desc_info dma_info;
};

#define DEVDRV_VPC_MSG_TYPE_SQ_SUBMIT               1
#define DEVDRV_VPC_MSG_TYPE_SQCQ_HEAD_UPDATE        2
#define DEVDRV_VPC_MSG_TYPE_DMA_INIT_AND_ALLOC_SQCQ 3
#define DEVDRV_VPC_MSG_TYPE_FREE_DMA_SQCQ           4
#define DEVDRV_VPC_MSG_TYPE_DMA_LINK_PREPARE        5
#define DEVDRV_VPC_MSG_TYPE_DMA_LINK_FREE           6
#define DEVDRV_VPC_MSG_TYPE_DMA_LINK_SQ_ALLOC       7
#define DEVDRV_VPC_MSG_TYPE_DMA_LINK_SQ_FREE        8
#define DEVDRV_VPC_MSG_TYPE_DMA_SQ_DESC_FILL        9
#define DEVDRV_VPC_MSG_TYPE_MAX                     10

struct devdrv_vpc_msg {
    u32 cmd;
    u32 version;
    int error_code;
    union devdrv_vpc_cmd cmd_data;
};
#define DEVDRV_NEED_ACK_DATA_TO_VM 1         /* all struct devdrv_vpc_msg need ack to vm */
#define DEVDRV_VPC_MSG_ACK_TO_VM_MIN_LEN 12  /* data no need ack to vm, but the error_code need ack to vm */
#define DEVDRV_VPC_MSG_ACK_TO_VM_DEFAULT_LEN sizeof(struct devdrv_vpc_msg)
/**
 * The map rams operation on each vf takes 6~7s
 * If there are 12 vf in vm, (12 - 1) * 7s = 77s is needed.
*/
#define DEVDRV_VPC_MSG_MAX_TIMEOUT 100000000        // 100s max timeout for vm init process
#define DEVDRV_VPC_MSG_DEFAULT_TIMEOUT 5000000      // 5s is default vpc timeout

int devdrv_vpc_msg_send(u32 dev_id, u32 cmd_type, struct devdrv_vpc_msg *vpc_msg, u32 data_len, u32 timeout);
int devdrv_mdev_vpc_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_mdev_vpc_uninit(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_mdev_pm_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_mdev_pm_uninit(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_mdev_vm_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_mdev_vm_uninit(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_vpc_dma_iova_addr_check(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_dma_sq_node *sq_node,
    enum devdrv_dma_direction direction);
#endif
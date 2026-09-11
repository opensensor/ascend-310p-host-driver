/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
* Create: 2022-12-13
*/

#ifndef _VPC_INTERFACE_H
#define _VPC_INTERFACE_H

#include <linux/types.h>
#include <linux/pci.h>

#define VPC_VM_FID 0                            // set fid 0 when send vpc msg in vm
#define VPC_DEFAULT_TIMEOUT 5000000             // default vpc timeout is 5 seconds
#define VPC_BLK_MODE_TIMEOUT 0xFFFFFFFF         // wait (interruptible) until recv rx irq

/* msg */
struct vmng_tx_msg_proc_info {
    void *data;
    u32 in_data_len;
    u32 out_data_len;
    u32 real_out_len;
};

struct vmng_rx_msg_proc_info {
    void *data;
    u32 in_data_len;
    u32 out_data_len;
    u32 *real_out_len;
};

enum vmng_vpc_type {
    VMNG_VPC_TYPE_TEST = 0,
    VMNG_VPC_TYPE_HDC,
    VMNG_VPC_TYPE_DEVMM,
    VMNG_VPC_TYPE_DEVMM_MNG,
    VMNG_VPC_TYPE_TSDRV,        // both default and sriov mdev need tsdrv
    VMNG_VPC_TYPE_DEVMNG = 5,
    VMNG_VPC_TYPE_PCIE = 5,     // for sriov
    VMNG_VPC_TYPE_HDC_CTRL = 6,
    VMNG_VPC_TYPE_DVPP = 6,     // for sriov
    VMNG_VPC_TYPE_ESCHED = 7,
    VMNG_VPC_TYPE_QUEUE = 8,
    VMNG_VPC_TYPE_RESERVE_1 = 9,
    VMNG_VPC_TYPE_RESERVE_2 = 10,
    VMNG_VPC_TYPE_MAX = 11
};

#define VMNG_VPC_NORMAL_MODE 0
#define VMNG_VPC_SAFE_MODE 1
struct vmng_vpc_client {
    enum vmng_vpc_type vpc_type;
    int (*init)(void);
    int (*msg_recv)(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
    int safe_mode;
};

/* common msg */
enum vmng_msg_common_type {
    VMNG_MSG_COMMON_TYPE_EXTENSION = 0,
    VMNG_MSG_COMMON_TYPE_TEST,
    VMNG_MSG_COMMON_TYPE_HDC,
    VMNG_MSG_COMMON_TYPE_TSDRV,
    VMNG_MSG_COMMON_TYPE_HDC_TRAFFIC,
    VMNG_MSG_COMMON_TYPE_DEVMNG,
    VMNG_MSG_COMMON_TYPE_MAX
};

struct vmng_common_msg_client {
    enum vmng_msg_common_type type;
    void (*init)(u32 dev_id, u32 fid, int status);
    int (*common_msg_recv)(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
};

int vmngh_vpc_register_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client);
int vmngh_vpc_register_client_safety(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client);
int vmngh_vpc_unregister_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client);
int vmngh_vpc_msg_send(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info,
    u32 timeout);

int vmnga_vpc_register_client(u32 dev_id, const struct vmng_vpc_client *vpc_client);
int vmnga_vpc_unregister_client(u32 dev_id, const struct vmng_vpc_client *vpc_client);
int vmnga_vpc_msg_send(u32 dev_id, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info,
    u32 timeout);

int vmngh_register_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client);
int vmngh_unregister_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client);
int vmnga_register_common_msg_client(u32 dev_id, const struct vmng_common_msg_client *msg_client);
int vmnga_unregister_common_msg_client(u32 dev_id, const struct vmng_common_msg_client *msg_client);

int vmngh_common_msg_send(u32 dev_id, u32 fid, enum vmng_msg_common_type cmn_type,
    struct vmng_tx_msg_proc_info *tx_info);
int vmnga_common_msg_send(u32 dev_id, enum vmng_msg_common_type cmn_type, struct vmng_tx_msg_proc_info *tx_info);

#define VIRTMNGAGENT_MSIX_MAX 128

struct vmnga_vpc_msix_info {
    struct msix_entry *entries;
    u32 msix_irq_num;
    u32 msix_irq_offset;
};

struct vmnga_mmio {
    phys_addr_t bar0_base;
    u64 bar0_size;
    phys_addr_t bar2_base;
    u64 bar2_size;
    phys_addr_t bar4_base;
    u64 bar4_size;
};

struct vmng_vpc_unit {
    struct pci_dev *pdev;                       /* pci dev */
    struct vmnga_mmio mmio;
    struct vmnga_vpc_msix_info msix_info;       /* misx interrupts ctrl struct */
    u32 dev_id;
    u32 fid;
};

/* Unified VPC Interface */
int vpc_register_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client);
int vpc_register_client_safety(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client);
int vpc_unregister_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client);
int vpc_msg_send(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info,
    u32 timeout);

int vpc_register_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client);
int vpc_unregister_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client);
int vpc_common_msg_send(u32 dev_id, u32 fid, enum vmng_msg_common_type cmn_type,
                        struct vmng_tx_msg_proc_info *tx_info);

#define SERVER_TYPE_VM_PCIE 0
int vmng_vpc_init(struct vmng_vpc_unit *unit_in, int server_type);
int vmng_vpc_uninit(struct vmng_vpc_unit *unit_in, int server_type);

#endif
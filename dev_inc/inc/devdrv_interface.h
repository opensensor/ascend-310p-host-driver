/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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

#ifndef __DEVDRV_MSG_INTERFACE_H__
#define __DEVDRV_MSG_INTERFACE_H__

#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/types.h>
#ifndef AOS_LLVM_BUILD
#include <linux/dma-direction.h>
#else
#include <aos_dma.h>
#endif
#include "dbl/uda.h"
#define DEVDRV_PLATFORM_TYPE_FPGA 0
#define DEVDRV_PLATFORM_TYPE_EMU 1
#define DEVDRV_PLATFORM_TYPE_ESL 2
#define DEVDRV_PLATFORM_TYPE_ASIC 3

#define DEVDRV_SYSTEM_START_FAIL 0x68021000
#define DEVDRV_PCIE_AER_ERROR 0x64021001

/*
 * p_type stores the platform type: 0-FPGA 1-EMU 2-ESL 3-ASIC
 * version stores the soc version: 0x300 matchs b300,
 * 0x201 matchs b201 , as so on.
 */
int devdrv_get_platform_type(unsigned int *p_type, unsigned int *version);

#define DEVDRV_VIRT_MACH_SIGN               0x503250
#define DEVDRV_HOST_PHY_MACH_FLAG           0x5a6b7c8d    /* host physical mathine flag */
#define DEVDRV_HOST_VM_MACH_FLAG            0x1a2b3c4d    /* vm mathine flag */
#define DEVDRV_HOST_CONTAINER_MACH_FLAG     0xa4b3c2d1    /* container mathine flag */
#define DEVDRV_HOST_PHY_MACH_FLAG_OFFSET    0x400         /* HOST_FLAG offset in BAR4 */
#define DEVDRV_ERROR_CODE_OFFSET            0x404         /* ERROR_CODE offset in BAR4 */

#define DEVDRV_COMMON_MSG_NOTIFY_TIMEOUT       30000 /* 300s total, each sleep 10ms */

/****************************************************************************
 * ***************************** chip type * ********************************
 ***************************************************************************/
/* ******************** common for host and device ******************** */
#define HISI_MINI_V1        0
#define HISI_CLOUD_V1       1
#define HISI_MINI_V2        2
#define HISI_CLOUD_V2       3
#define HISI_MINI_V3        4
#define HISI_MDC_V2         5
#define HISI_CHIP_NUM       6
#define HISI_CHIP_UNKNOWN   7

u32 devdrv_get_dev_chip_type(u32 dev_id);

/****************************************************************************
 * ***************************** DMA_COPY API ********************************
 ***************************************************************************/
/* the direction of dma */
enum devdrv_dma_direction {
    DEVDRV_DMA_HOST_TO_DEVICE = 0x0,
    DEVDRV_DMA_DEVICE_TO_HOST = 0x1,
    DEVDRV_DMA_LOCAL_TO_LOCAL = 0x2
};

#define DEVDRV_PHY_BOOT            0
#define DEVDRV_SRIOV_VF_BOOT       1
#define DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT   2
#define DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT   3
#define DEVDRV_MDEV_VF_PHY_BOOT    4
#define DEVDRV_MDEV_VF_VM_BOOT     5
#define DEVDRV_MAX_EVN_BOOT_TYPE   6

#define DEVDRV_DMA_DATA_COMM_CHAN_NUM 1
#define DEVDRV_DMA_DATA_PCIE_MSG_CHAN_NUM 1

#define DEVDRV_DMA_DESC_FILL_CONTINUE 0
#define DEVDRV_DMA_DESC_FILL_FINISH 1

/* DMA copy type, cloud has 30 chan pcie use 22, mini has 16 pcie use 12 */
enum devdrv_dma_data_type {
    DEVDRV_DMA_DATA_COMMON = 0, /* used for IDE,vnic,file transfer,hdc low level service, have 1 dma channel */
    DEVDRV_DMA_DATA_PCIE_MSG,   /* used for non trans msg, admin msg, p2p msg, have 1 dma channel */
    DEVDRV_DMA_DATA_TRAFFIC,    /* used for devmm(online), hdc(offline), have the remaining part of dma channel */
    DEVDRV_DMA_DATA_MANAGE,     /* used for mdc scene hdc manage service type(64~95) */
    DEVDRV_DMA_DATA_TYPE_MAX
};

#define DEVDRV_DMA_PASSID_DEFAULT 0
/* DMA link copy */
struct devdrv_dma_node {
    u64 src_addr;
    u64 dst_addr;
    u32 size;
    enum devdrv_dma_direction direction;
    u32 loc_passid;
};

#define DEVDRV_DMA_WAIT_INTR 1
#define DEVDRV_DMA_WAIT_QUREY 2

#define DEVDRV_REMOTE_IRQ_FLAG 0x1
#define DEVDRV_LOCAL_IRQ_FLAG 0x2
#define DEVDRV_LOCAL_REMOTE_IRQ_FLAG 0x3
#define DEVDRV_ATTR_FLAG 0x4
#define DEVDRV_WD_BARRIER_FLAG 0x8
#define DEVDRV_RD_BARRIER_FLAG 0x10

/*
 * asynchronous dma parameter structer
 * interrupt_and_attr_flag: bit0 remote interrupt flag
 *                          bit1 local interrupt flag
 *                          bit2 attr of sq BD flag
 *                          bit3 wd barrier flag
 *                          bit4 rd barrier flag
 * remote_msi_vector : remote msi interrupt num
 */
struct devdrv_asyn_dma_para_info {
    u32 interrupt_and_attr_flag;
    u32 remote_msi_vector;
    void *priv;
    u32 trans_id;
    void (*finish_notify)(void *, u32, u32);
};

/* sync DMA copy */
int devdrv_dma_sync_copy(u32 dev_id, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                         enum devdrv_dma_direction direction);
/* async DMA copy */
int devdrv_dma_async_copy(u32 dev_id, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                          enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info);
/* sync DMA link copy */
int devdrv_dma_sync_link_copy(u32 dev_id, enum devdrv_dma_data_type type, int wait_type,
                              struct devdrv_dma_node *dma_node, u32 node_cnt);
/* async DMA link copy */
int devdrv_dma_async_link_copy(u32 dev_id, enum devdrv_dma_data_type type, struct devdrv_dma_node *dma_node,
                               u32 node_cnt, struct devdrv_asyn_dma_para_info *para_info);

/* sync DMA copy assign dma chan */
int devdrv_dma_sync_copy_plus(u32 dev_id, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst, u32 size,
                              enum devdrv_dma_direction direction);
/* async DMA copy assign dma chan */
int devdrv_dma_async_copy_plus(u32 dev_id, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst, u32 size,
                               enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info);
/* sync DMA link copy assign dma chan */
int devdrv_dma_sync_link_copy_plus(u32 dev_id, enum devdrv_dma_data_type type, int wait_type, int instance,
                                   struct devdrv_dma_node *dma_node, u32 node_cnt);
/* async DMA link copy assign dma chan */
int devdrv_dma_async_link_copy_plus(u32 dev_id, enum devdrv_dma_data_type type, int instance,
                                    struct devdrv_dma_node *dma_node, u32 node_cnt,
                                    struct devdrv_asyn_dma_para_info *para_info);

struct devdrv_dma_prepare {
    u32 devid;
    u64 sq_size;
    u64 cq_size;
    dma_addr_t sq_dma_addr;
    dma_addr_t cq_dma_addr;
    void *sq_base;
    void *cq_base;
};

int devdrv_dma_get_sq_cq_desc_size(u32 devid, u32 *sq_desc_size, u32 *cq_desc_size);
int devdrv_dma_fill_desc_of_sq(u32 devid, void *sq_base, struct devdrv_dma_node *dma_node, u32 node_cnt,
                               u32 fill_status);

/* async DAM link prepare */
struct devdrv_dma_prepare *devdrv_dma_link_prepare(u32 devid, enum devdrv_dma_data_type type,
                                                   struct devdrv_dma_node *dma_node, u32 node_cnt);
/* async DAM link free */
int devdrv_dma_link_free(struct devdrv_dma_prepare *dma_prepare);

/****************************************************************************
 * ***************************** MSG Send API ********************************
 ***************************************************************************/
/* ******************** common for host and device ******************** */
/* **************************** host **************************** */
#define DEVDRV_MSG_CHAN_LEVEL_LOW 0
#define DEVDRV_MSG_CHAN_LEVEL_HIGH 1
#define DEVDRV_NON_TRANS_MSG_DEFAULT_DESC_SIZE 0x10000

/* msg chan operate */
void devdrv_msg_ring_doorbell(void *msg_chan);
void devdrv_msg_ring_cq_doorbell(void *msg_chan);
int devdrv_get_msg_chan_devid(void *msg_chan);
int devdrv_set_msg_chan_priv(void *msg_chan, void *priv);
void *devdrv_get_msg_chan_priv(void *msg_chan);
void *devdrv_get_msg_chan_host_sq_head(void *msg_chan, u32 *head);
void devdrv_move_msg_chan_host_sq_head(void *msg_chan);
void *devdrv_get_msg_chan_host_cq_head(void *msg_chan);
void devdrv_move_msg_chan_host_cq_head(void *msg_chan);
void devdrv_set_msg_chan_slave_sq_head(void *msg_chan, u32 head);
void *devdrv_get_msg_chan_slave_sq_tail(void *msg_chan, u32 *tail);
void devdrv_move_msg_chan_slave_sq_tail(void *msg_chan);
bool devdrv_msg_chan_slave_sq_full_check(void *msg_chan);
void *devdrv_get_msg_chan_slave_cq_tail(void *msg_chan);
void devdrv_move_msg_chan_slave_cq_tail(void *msg_chan);
#ifdef CFG_FEATURE_SEC_COMM_L3
void *devdrv_get_msg_chan_host_rsv_sq_tail(void *msg_chan, u32 *tail);
void *devdrv_get_msg_chan_host_rsv_cq_tail(void *msg_chan);
int devdrv_dma_copy_sq_desc_to_slave(void *msg_chan, struct devdrv_asyn_dma_para_info *para,
                             enum devdrv_dma_data_type data_type, int instance);
int devdrv_dma_copy_cq_desc_to_slave(void *msg_chan, struct devdrv_asyn_dma_para_info *para,
                             enum devdrv_dma_data_type data_type, int instance);
#endif

/* **************************** device **************************** */
/* msg client type */
enum agentdrv_msg_client_type {
    agentdrv_msg_client_pcivnic = 0,
    agentdrv_msg_client_smmu,
    agentdrv_msg_client_devmm,
    agentdrv_msg_client_common,
    agentdrv_msg_client_devmanager,
    agentdrv_msg_client_tsdrv,
    agentdrv_msg_client_hdc,
    agentdrv_msg_client_queue,
    agentdrv_msg_client_s2s,
    agentdrv_msg_client_max
};

/* trans msg client */
struct agentdrv_trans_msg_client {
    enum agentdrv_msg_client_type type;
    int (*init_trans_msg_chan)(void *msg_ch);
    int (*uninit_trans_msg_chan)(void *msg_ch);
    void (*rx_trans_msg_notify)(void *msg_ch);
    void (*tx_trans_finish_notify)(void *msg_ch);
};
int agentdrv_register_trans_msg_client(const struct agentdrv_trans_msg_client *msg_client);
int agentdrv_unregister_trans_msg_client(const struct agentdrv_trans_msg_client *msg_client);

#define AGENTDRV_NOTIFY_NORMAL 0
#define AGENTDRV_NOTIFY_FINISH 7788
#define AGENTDRV_NOTIFY_BUSY   8899

/* non-trans msg client */
struct agentdrv_non_trans_msg_client {
    enum agentdrv_msg_client_type type;
    u32 flag;
    int (*init_non_trans_msg_chan)(void *msg_chan);
    void (*uninit_non_trans_msg_chan)(void *msg_chan);
    int (*non_trans_msg_process)(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    /* irq top half, can not sleep or time-consuming operation */
    int (*non_trans_msg_irq_notify)(void *msg_chan, void *data, u32 in_data_len);
};

/* register non-trans msg client */
int agentdrv_register_non_trans_msg_client(const struct agentdrv_non_trans_msg_client *msg_client);

/* unregister non-trans msg client */
int agentdrv_unregister_non_trans_msg_client(const struct agentdrv_non_trans_msg_client *msg_client);
int agentdrv_enable_non_trans_msg_client(const struct agentdrv_non_trans_msg_client *msg_client);

/* non-trans msg sync send */
int agentdrv_sync_msg_send(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);

/* msg chan operate */
int agentdrv_get_remote_rx_msg_notify_irq(void *msg_chan);
int agentdrv_get_remote_tx_finish_notify_irq(void *msg_chan);
int agentdrv_get_msg_chan_devid(void *msg_chan);
int agentdrv_set_msg_chan_priv(void *msg_chan, void *priv);
void *agentdrv_get_msg_chan_priv(void *msg_chan);
void agentdrv_set_msg_chan_local_sq_head(void *msg_chan, u32 head);
void *agentdrv_get_msg_chan_local_sq_tail(void *msg_chan, u32 *tail);
dma_addr_t agentdrv_get_msg_chan_local_sq_tail_dma_addr(void *msg_chan);
void agentdrv_move_msg_chan_local_sq_tail(void *msg_chan);
bool agentdrv_msg_chan_local_sq_full_check(void *msg_chan);
dma_addr_t agentdrv_get_msg_chan_host_sq_tail_dma_addr(void *msg_chan);
void *agentdrv_get_msg_chan_local_cq_tail(void *msg_chan);
dma_addr_t agentdrv_get_msg_chan_local_cq_tail_dma_addr(void *msg_chan);
void agentdrv_move_msg_chan_local_cq_tail(void *msg_chan);
dma_addr_t agentdrv_get_msg_chan_host_cq_tail_dma_addr(void *msg_chan);
void *agentdrv_get_msg_chan_reserve_sq_head(void *msg_chan, u32 *head);
void agentdrv_move_msg_chan_reserve_sq_head(void *msg_chan);
void *agentdrv_get_msg_chan_reserve_cq_head(void *msg_chan);
void agentdrv_move_msg_chan_reserve_cq_head(void *msg_chan);

/* comon msg */
enum agentdrv_common_msg_type {
    AGENTDRV_COMMON_MSG_PCIVNIC = 0,
    AGENTDRV_COMMON_MSG_SMMU,
    AGENTDRV_COMMON_MSG_DEVMM,
    AGENTDRV_COMMON_MSG_VMNG,
    AGENTDRV_COMMON_MSG_PROFILE = 4,
    AGENTDRV_COMMON_MSG_DEVDRV_MANAGER,
    AGENTDRV_COMMON_MSG_DEVDRV_TSDRV,
    AGENTDRV_COMMON_MSG_HDC,
    AGENTDRV_COMMON_MSG_SYSFS,
    AGENTDRV_COMMON_MSG_ESCHED,
    AGENTDRV_COMMON_MSG_DP_PROC_MNG,
    AGENTDRV_COMMON_MSG_UDIS,
    AGENTDRV_COMMON_MSG_TYPE_MAX
};

struct agentdrv_common_msg_client {
    enum agentdrv_common_msg_type type;
    int (*common_msg_recv)(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    void (*init_notify)(u32 devid);
};

int agentdrv_register_common_msg_client(struct agentdrv_common_msg_client *msg_client);
int agentdrv_unregister_common_msg_client(const struct agentdrv_common_msg_client *msg_client);
int agentdrv_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                             enum agentdrv_common_msg_type msg_type);

/****************************************************************************
 * ***************************** client register *****************************
 ***************************************************************************/
/* **************************** host **************************** */
enum devdrv_dev_type {
    DEV_TYPE_PCI = 0,
    DEV_TYPE_USB,
    DEV_TYPE_PLATFORM,
    DEV_TYPE_MAX
};

/*
mini启动状态:
DEVDRV_CTRL_STARTUP_UNPROBED:未probe
DEVDRV_CTRL_STARTUP_PROBED:probe到，alloc ID ok
DEVDRV_DEV_STARTUP_TOP_HALF_OK:完成上半部初始化，bar map and init interrupt ok
DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK:完成下半部初始化，pcie msg chan and dma ok
*/
enum devdrv_dev_startup_flag_type {
    DEVDRV_DEV_STARTUP_UNPROBED = 0,
    DEVDRV_DEV_STARTUP_PROBED,
    DEVDRV_DEV_STARTUP_TOP_HALF_OK,
    DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK
};

struct devdrv_ops;

struct devdrv_ctrl {
    u32 dev_id;
    u32 func_id;
    u32 master_flag;
    u32 virtfn_flag;
    enum devdrv_dev_startup_flag_type startup_flg;
    unsigned long timestamp;
    void *priv;
    enum devdrv_dev_type dev_type;
    const struct devdrv_ops *ops; /* inited by register caller */
    struct device *dev;
    struct pci_bus *bus;
    struct pci_dev *pdev;
};

struct devdrv_client_instance {
    void *priv;
    struct devdrv_ctrl *dev_ctrl;
    u32 flag; /* if init_instance already called or not */
    struct mutex flag_mutex;
};

/* **************************** device **************************** */
enum agentdrv_client_type {
    AGENTDRV_CLIENT_PCIVNIC = 0,
    AGENTDRV_CLIENT_SMMU,
    AGENTDRV_CLIENT_DEVMM,
    AGENTDRV_CLIENT_PROFILE,
    AGENTDRV_CLIENT_DEVMANAGER,
    AGENTDRV_CLIENT_TSDRV,
    AGENTDRV_CLIENT_HDC,
    AGENTDRV_CLIENT_VMNG,
    AGENTDRV_CLIENT_QUEUE,
    AGENTDRV_CLIENT_DP_PROC_MNG,
    AGENTDRV_CLIENT_TYPE_MAX
};

struct agentdrv_dev {
    u32 agent_id;
    struct pci_dev *pdev;
};

struct agentdrv_client_instance {
    void *priv;
    struct agentdrv_dev *adev;
};

struct agentdrv_client {
    enum agentdrv_client_type type;
    int (*init_instance)(struct agentdrv_client_instance *instance);
    void (*uninit_instance)(struct agentdrv_client_instance *instance);
    int (*flr_uninstance)(struct agentdrv_client_instance *instance);
};

bool agentdrv_check_is_flr_finish(u32 dev_id);

/****************************************************************************
 * ***************************** p2p mem trans *****************************
 ***************************************************************************/
/* **************************** host & device **************************** */
/* devid: when host call it is host devid, device call it is device devid */
int devdrv_devmem_addr_h2d(u32 devid, phys_addr_t host_bar_addr, phys_addr_t *device_phy_addr);
int devdrv_devmem_addr_d2h(u32 devid, phys_addr_t device_phy_addr, phys_addr_t *host_bar_addr);

/* devid1 devid2: host dev number
   return value: true surport pcie, false not surport
   Description: cloud: ai server or evb: diffrent node surport pcie, pcie card: all surport
                mini: all surport pcie */
#ifdef DRV_CLOUD
bool devdrv_p2p_surport_pcie(u32 devid1, u32 devid2);
#endif

/* **************************** host **************************** */
int devdrv_device_txatu_config(int pid, u32 devid, dma_addr_t host_dma_addr, u64 size);
int devdrv_device_txatu_del(int pid, u32 devid, dma_addr_t host_dma_addr);

/* Convert the dst_devid bar address to the devid dma address */
int devdrv_devmem_addr_bar_to_dma(u32 devid, u32 dst_devid, phys_addr_t host_bar_addr, dma_addr_t *dma_addr);

int devdrv_enable_p2p(int pid, u32 dev_id, u32 peer_dev_id);
int devdrv_disable_p2p(int pid, u32 dev_id, u32 peer_dev_id);
bool devdrv_is_p2p_enabled(u32 dev_id, u32 peer_dev_id);
void devdrv_flush_p2p(int pid); /* used in user process crush */
int devdrv_get_p2p_capability(u32 dev_id, u64 *capability);
int devdrv_get_p2p_access_status(u32 devid, u32 peer_devid, int *status);

/* **************************** device **************************** */
enum devdrv_p2p_addr_type {
    DEVDRV_P2P_IO_TS_DB = 0,
    DEVDRV_P2P_IO_TS_SRAM,
    DEVDRV_P2P_IO_HWTS,
    DEVDRV_P2P_HOST_MEM,
    DEVDRV_P2P_ADDR_TYPE_MAX
};

/* local_devid:device devid, devid: host devid */
int agentdrv_get_p2p_addr(u32 local_devid, u32 devid, enum devdrv_p2p_addr_type type, phys_addr_t *phy_addr, u32 *size);

/* device to device msg */
enum agentdrv_p2p_msg_type {
    AGENTDRV_P2P_MSG_DEVMM = 0,
    AGENTDRV_P2P_MSG_TYPE_MAX
};

/* devid: device devid, in_len <=28, out_len <=1k */
typedef int (*p2p_msg_recv)(u32 devid, void *data, u32 data_len, u32 in_len, u32 *out_len);
int agentdrv_register_p2p_msg_proc_func(enum agentdrv_p2p_msg_type msg_type, p2p_msg_recv func);

/* local_devid:device devid, devid: host devid */
int agentdrv_p2p_msg_send(u32 local_devid, u32 devid, enum agentdrv_p2p_msg_type msg_type, void *data, u32 data_len,
                          u32 in_len, u32 *out_len);

/* target_addr is phy addr */
int agentdrv_devmem_txatu_target_to_base(u32 local_devid, u32 devid, phys_addr_t target_addr, phys_addr_t *base_addr);
int agentdrv_devmem_txatu_host_target_to_base(u32 local_devid, phys_addr_t target_addr, phys_addr_t *base_addr);

/* return:  0-3, -1 not pcie space addr */
int agentdrv_get_devid_from_phy_addr(phys_addr_t phy_addr);

/* This function called by host to get device index with host device id
 * on cloud,  an smp system has 4 chips, so the device index is 0 ~ 3
 * on mini, device index is 0
 */
int devdrv_get_device_index(u32 host_dev_id);

/* devid: local devid */
int agentdrv_get_host_devid(u32 devid);

#define DEVDRV_DEV_ONLINE 1
#define DEVDRV_DEV_OFFLINE 0

#define DEVDRV_DEV_ONLINE_NOTIFY 0
#define DEVDRV_HOST_CFG_NOTIFY 1

/* devid: device devid, online_devid: host online devid, status: DEVDRV_DEV_* */
typedef int (*devdrv_notify_func)(u32 devid, u32 notify_type, u32 online_devid, u32 status);
int agentdrv_register_dev_online_proc_func(devdrv_notify_func func);

/* **************************** numa remap **************************** */
#define AGENTDRV_NUMA_REMAP_NUM 2
typedef struct agentdrv_numa_address {
    u64 start;
    u64 end;
} agentdrv_numa_address_t;

typedef struct agentdrv_numa_remap {
    agentdrv_numa_address_t address[AGENTDRV_NUMA_REMAP_NUM];
} agentdrv_numa_remap_t;

int agentdrv_numa_addr_unmap(u32 dev_id, u32 vfid);
int agentdrv_numa_addr_remap(u32 dev_id, u32 vfid, agentdrv_numa_remap_t *numa_remap);

/****************************************************************************
 * ******************************** others ***********************************
 ***************************************************************************/

struct devdrv_black_callback {
    int (*callback)(u32 devid, u32 code, struct timespec stamp);
};
int devdrv_register_black_callback(struct devdrv_black_callback *black_callback);
void devdrv_unregister_black_callback(struct devdrv_black_callback *black_callback);

/* Before PCIe suspend, the HDC registration interface is used to check whether
 * there are unreleased sessions. If yes, the PCIe cannot enter into suspend mode */
typedef int (*devdrv_suspend_pre_check)(int flag);
/* When peer reboot/panic, local pcie can detect the peer fault and should notify HDC
   to stop bussiness and so on */
typedef int (*devdrv_peer_fault_notify)(u32 status);

#define DEVDRV_PEER_STATUS_NORMAL 0x1
#define DEVDRV_PEER_STATUS_RESET 0x2
#define DEVDRV_PEER_STATUS_LINKDOWN 0x4

void agentdrv_pci_suspend_check_register(devdrv_suspend_pre_check suspend_check);
void agentdrv_pci_suspend_check_unregister(void);
void agentdrv_peer_fault_notifier_register(devdrv_peer_fault_notify fault_notifier);
void agentdrv_peer_fault_notifier_unregister(void);

#define HOST_TYPE_NORMAL 0
#define HOST_TYPE_ARM_3559 3559
#define HOST_TYPE_ARM_3519 3519

int devdrv_get_host_type(void);

/* device启动状态 */
enum dsmi_boot_status {
    DSMI_BOOT_STATUS_UNINIT = 0, /* 未初始化 */
    DSMI_BOOT_STATUS_BIOS,       /* BIOS启动中 */
    DSMI_BOOT_STATUS_OS,         /* OS启动中 */
    DSMI_BOOT_STATUS_FINISH      /* 启动完成 */
};

enum devdrv_addr_type {
    DEVDRV_ADDR_TS_DOORBELL = 0,
    DEVDRV_ADDR_TS_SRAM,
    DEVDRV_ADDR_TS_SQ_BASE,
    DEVDRV_ADDR_TEST_BASE,
    DEVDRV_ADDR_LOAD_RAM,
    DEVDRV_ADDR_HWTS,
    DEVDRV_ADDR_IMU_LOG_BASE,
    DEVDRV_ADDR_HDR_BASE, /* bbox history data record area */
    DEVDRV_ADDR_BBOX_BASE,
    DEVDRV_ADDR_REG_SRAM_BASE,
    DEVDRV_ADDR_TSDRV_RESV_BASE,
    DEVDRV_ADDR_DEVMNG_RESV_BASE,
    DEVDRV_ADDR_HBM_ECC_MEM_BASE,
    DEVDRV_ADDR_VF_BANDWIDTH_BASE,
    DEVDRV_ADDR_HBM_BASE,

    /* stars addr info */
    DEVDRV_ADDR_STARS_SQCQ_BASE,
    DEVDRV_ADDR_STARS_SQCQ_INTR_BASE,
    DEVDRV_ADDR_STARS_TOPIC_SCHED_BASE,
    DEVDRV_ADDR_STARS_TOPIC_SCHED_RES_MEM_BASE,
    DEVDRV_ADDR_STARS_CDQM_BASE,
    DEVDRV_ADDR_STARS_INTR_BASE,
    DEVDRV_ADDR_TS_SHARE_MEM,
    DEVDRV_ADDR_TYPE_MAX
};

/* module init finish */
#define DEVDRV_HOST_MODULE_PCIVNIC 0
#define DEVDRV_HOST_MODULE_HDC 1
#define DEVDRV_HOST_MODULE_DEVMM 2
#define DEVDRV_HOST_MODULE_DEVMNG 3
#define DEVDRV_HOST_MODULE_MAX 4
#define DEVDRV_HOST_MODULE_MASK ((0x1UL << DEVDRV_HOST_MODULE_MAX) - 1)
#define DEVDRV_VALUE_SIZE 512

int devdrv_set_module_init_finish(int dev_id, int module);

struct devdrv_pcie_id_info {
    unsigned int venderid;    /* 厂商id */
    unsigned int subvenderid; /* 厂商子id */
    unsigned int deviceid;    /* 设备id */
    unsigned int subdeviceid; /* 设备子id */
    unsigned int bus;         /* 总线号 */
    unsigned int device;      /* 设备物理号 */
    unsigned int fn;          /* 设备功能号 */
    int domain;               /* 域信息 */
    unsigned char reserve[32];
};

struct devdrv_pci_dev_info {
    u8 bus_no;
    u8 device_no;
    u8 function_no;
};

#define DEVDRV_PCIE_RC_MODE 0
#define DEVDRV_PCIE_EP_MODE 1

int agentdrv_get_rc_ep_mode(u32 *mode);
int agentdrv_read_capability(u32 dev_id, u32 *cap_value);
int agentdrv_clear_capability(u32 dev_id);

int devdrv_get_dev_num(void);
int devdrv_get_slot_num(void);
int devdrv_get_davinci_dev_num(void);
int devdrv_get_device_boot_status(u32 devid, u32 *boot_status);
int devdrv_get_pci_dev_info(u32 devid, struct devdrv_pci_dev_info *dev_info);
int devdrv_get_ts_drv_irq_vector_id(u32 devid, u32 index, unsigned int *entry);
int devdrv_get_topic_sched_irq_vector_id(u32 devid, u32 index, unsigned int *entry);
int devdrv_get_cdqm_irq_vector_id(u32 devid, u32 index, unsigned int *entry);
int devdrv_get_irq_vector(u32 devid, u32 entry, unsigned int *irq);
int devdrv_register_irq_func_expand(u32 dev_id, int vector_index, irqreturn_t (*callback_func)(int, void *),
                                    void *para, const char *name);
int devdrv_unregister_irq_func_expand(u32 dev_id, int vector_index, void *para);
int devdrv_get_addr_info(u32 devid, enum devdrv_addr_type type, u32 index, u64 *addr, size_t *size);
int agentdrv_get_addr_info(u32 devid, enum devdrv_addr_type type, u32 index, u64 *addr, size_t *size);
int devdrv_pcie_read_proc(u32 dev_id, enum devdrv_addr_type type, u32 offset, unsigned char *value, u32 len);
int devdrv_pcie_write_proc(u32 dev_id, enum devdrv_addr_type type, u32 offset, unsigned char *value, u32 len);
int devdrv_pcie_prereset(u32 dev_id);
int devdrv_pcie_reinit(u32 dev_id);
int devdrv_hot_reset_device(u32 dev_id);
int devdrv_hot_reset_smp_device(u64 smp_device_bitmap, u32 smp_device_number);
int devdrv_device_status_abnormal_check(void *msg_chan);

struct agentdrv_cpu_info {
    u32 ccpu_num;
    u32 ccpu_os_sched;
    u32 dcpu_num;
    u32 dcpu_os_sched;
    u32 aicpu_num;
    u32 aicpu_os_sched;
    u32 tscpu_num;
    u32 tscpu_os_sched;
};

struct agentdrv_ts_dma_chan_info {
    u32 chan_id_base;
    u32 chan_num;
    u32 chan_done_irq_base;
    u32 pf_num;
    u32 func_total;
    u32 msix_offset;
};

typedef enum drvdrv_dev_state {
    GOING_TO_S0 = 0,      /* host go to S0 state, mini ready */
    GOING_TO_SUSPEND,     /* host go to suspend */
    GOING_TO_S3,          /* host go to S3, release resource */
    GOING_TO_S4,          /* host go to S4, release resource */
    GOING_TO_D0,          /* mini go to D0, mini ready */
    GOING_TO_D3,          /* mini go to D3, host stop comm with mini */
    GOING_TO_DISABLE_DEV, /* mini is removed or disabled */
    GOING_TO_ENABLE_DEV,  /* mini is probed or enabled */
    STATE_MAX
} drvdrv_dev_state;

struct devdrv_pcie_link_info_para {
    unsigned int link_status;
    unsigned int rate_mode;
    unsigned int lane_num;
    unsigned int reserved[8];
};

int agentdrv_get_cpu_info(u32 devid, struct agentdrv_cpu_info *cpu_info);
int agentdrv_get_cpudomain_info(int dev_id, int chip_id, int func_index, struct agentdrv_cpu_info *cpu_info);

#define HCCS_GROUP_SUPPORT_MAX_CHIPNUM  16
int devdrv_get_hccs_link_status_and_group_id(u32 devid, u32 *hccs_status, u32 hccs_group_id[], u32 group_id_num);

#define AGENTDRV_ADMODE_FULL_MATCH     1
#define AGENTDRV_ADMODE_NOT_FULL_MATCH 0

#define CONNECT_PROTOCOL_PCIE 0
#define CONNECT_PROTOCOL_HCCS 1
#define CONNECT_PROTOCOL_UNKNOWN 2
int devdrv_get_connect_protocol(u32 dev_id);

#define DEVDRV_SRIOV_TYPE_PF 0
#define DEVDRV_SRIOV_TYPE_VF 1
int devdrv_get_pfvf_type_by_devid(u32 dev_id);

#define DEVDRV_SRIOV_HOST_MAX_PF_DEV_CNT 64
#define DEVDRV_SRIOV_HOST_VF_DEVID_START 100
int devdrv_get_host_pfvf_id_by_devid(u32 dev_id, u32 *pf_id, u32 *vf_id);

bool devdrv_is_mdev_vm_boot_mode(u32 dev_id);

bool agentdrv_is_mdev_vm_full_spec(u32 dev_id);

int devdrv_get_devid_by_pfvf_id(u32 pf_id, u32 vf_id, u32 *dev_id);
int devdrv_get_pfvf_id_by_devid(u32 dev_id, u32 *pf_id, u32 *vf_id);
int agentdrv_sriov_init_instance(u32 dev_id, u32 vm_full_spec_enable, u32 computility, u32 total,
    unsigned long *dma_bitmap);
int agentdrv_sriov_uninit_instance(u32 dev_id);
bool devdrv_is_sriov_support(u32 dev_id);
int devdrv_mdev_set_pm_iova_addr_range(int devid, dma_addr_t iova_base, u64 iova_size);

void *devdrv_dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_addr, gfp_t gfp);
void *devdrv_dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_addr, gfp_t gfp);
void devdrv_dma_free_coherent(struct device *dev, size_t size, void *addr, dma_addr_t dma_addr);
dma_addr_t devdrv_dma_map_single(struct device *dev, void *ptr, size_t size, enum dma_data_direction dir);
void devdrv_dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);
dma_addr_t devdrv_dma_map_page(struct device *dev, struct page *page,
    size_t offset, size_t size, enum dma_data_direction dir);
void devdrv_dma_unmap_page(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);

/* agent smmu only can transform 32 addr one time */
#define DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM 120
int agentdrv_smmu_iova_to_phys(u32 dev_id, dma_addr_t *va, u32 va_cnt, phys_addr_t *pa);

/* for virtualization passthrough, if host_flag is 0x5a6b7c8d, host is physical mathine */
void devdrv_set_host_phy_mach_flag(u32 devid, u32 host_flag);
int devdrv_get_host_phy_mach_flag(u32 devid, u32 *host_flag);
int agentdrv_get_host_phy_mach_flag(u32 dev_id, u32 *host_flag);
void agentdrv_set_host_phy_mach_flag(u32 dev_id, u32 host_flag);

/* input:  devid: device dev number
   output:  chan_id_base: The starting dma channel number assigned to ts
            chan_num: Number of dma channels assigned to ts
            chan_done_irq_base: valid in cloud, The starting number of the dma done interrupt */
int agentdrv_get_ts_dma_chan_info(u32 devid, struct agentdrv_ts_dma_chan_info *chan_info);
int devdrv_dma_alloc_sq_desc_for_ts(u32 dev_id, u64 *dma_addr, u64 *phy_addr, u32 *len);
int devdrv_dma_alloc_cq_desc_for_ts(u32 dev_id, u64 *dma_addr, u64 *phy_addr, u32 *len);
int devdrv_dma_map_for_ts(u32 dev_id, bool is_sq,
    u64 *phy_addr, u32 *len, u64 *dma_addr);
/* for dev manager to register when insmod,then pcie report probed info to dev manager
probe_num:num of mini probed
devids: devids to be reported
len:length of devids
 */
typedef int (*devdrv_dev_startup_notify)(u32 probe_num, const u32 devids[], u32 array_len, u32 len);
typedef int (*devdrv_dev_state_notify)(u32 devid, u32 state);
void drvdrv_dev_startup_register(devdrv_dev_startup_notify startup_callback);
void drvdrv_dev_state_notifier_register(devdrv_dev_state_notify state_callback);
void devdrv_dev_state_notifier_unregister(void);
int devdrv_get_bbox_reservd_mem(unsigned int devid, unsigned long long *dma_addr, struct page **dma_pages,
                                unsigned int *len);

typedef void (*devdrv_heartbeat_broken_callback)(unsigned long devid);
void devdrv_heartbeat_broken_notify_register(devdrv_heartbeat_broken_callback hb_broken_register);
void devdrv_hb_broken_stop_msg_send(u32 devid);
int devdrv_get_master_devid_in_the_same_os(u32 dev_id, u32 *master_dev_id);
int devdrv_get_davinci_dev_num_by_pdev(struct pci_dev *pdev);
int devdrv_get_dev_id_by_pdev(struct pci_dev *pdev);
void *devdrv_get_devdrv_priv(struct pci_dev *pdev);
int devdrv_get_theoretical_capability(u32 devid, u64 *bandwidth, u64 *packspeed);
int devdrv_get_real_capability_ratio(u32 devid, u32 *bandwidth_ratio, u32 *packspeed_ratio);
int devdrv_get_dev_topology(u32 devid, u32 peer_devid, int *topo_type);
int agentdrv_set_msg_dev_status(u32 dev_id, int status);
bool agentdrv_get_dma_urca_err(u32 dev_id);
int devdrv_get_pcie_link_info(u32 dev_id, struct devdrv_pcie_link_info_para* pcie_link_info);
int devdrv_force_linkdown(u32 devid);

#define AGENTDRV_PROF_ENABLE  0x5a5a
#define AGENTDRV_PROF_DISABLE 0
#define AGENTDRV_PROF_INVALID_PID (-1) /* DCMI use invalid pid */

#define AGENTDRV_PROF_DATA_NUM 3

struct agentdrv_profiling_buf {
    u64 time;
    u32 dev_id;

    u32 tx_p_bw[AGENTDRV_PROF_DATA_NUM];
    u32 tx_np_bw[AGENTDRV_PROF_DATA_NUM];
    u32 tx_cpl_bw[AGENTDRV_PROF_DATA_NUM];
    u32 tx_np_lantency[AGENTDRV_PROF_DATA_NUM];

    u32 rx_p_bw[AGENTDRV_PROF_DATA_NUM];
    u32 rx_np_bw[AGENTDRV_PROF_DATA_NUM];
    u32 rx_cpl_bw[AGENTDRV_PROF_DATA_NUM];
};

int hisi_pcie_pmu_enable_profiling(u32 dev_id);
int hisi_pcie_pmu_disable_profiling(u32 dev_id);
int hisi_pcie_pmu_get_profiling_info(u32 dev_id, struct agentdrv_profiling_buf *info);

#define AGENTDRV_S2S_TO_DEVICE 0x0
#define AGENTDRV_S2S_TO_HOST 0x1
/* device to device msg */
enum agentdrv_s2s_msg_type {
    AGENTDRV_S2S_MSG_DEVMM = 0,
    AGENTDRV_S2S_MSG_TRSDRV = 1,
    AGENTDRV_S2S_MSG_USER_TEST = 2,
    AGENTDRV_S2S_MSG_PCIVNIC = 3,
    AGENTDRV_S2S_MSG_TYPE_MAX
};

#define DEVDRV_S2S_IDLE_MODE     0
#define DEVDRV_S2S_SYNC_MODE     1
#define DEVDRV_S2S_ASYNC_MODE    2

struct data_input_info {
    void *data;
    u32 data_len;
    u32 in_len;
    u32 out_len;
    u32 msg_mode;
};

#define DEVDRV_S2S_KEEP_RECV  1
#define DEVDRV_S2S_END_RECV   2

struct data_recv_info {
    void *data;
    u32 data_len;
    u32 out_len;
    u32 flag;
};

/* devid: device devid, in_len <=28, out_len <=1k */
typedef int (*s2s_msg_recv)(u32 local_devid, u32 sdid, struct data_input_info *data_info);
int agentdrv_register_s2s_msg_proc_func(enum agentdrv_s2s_msg_type msg_type, s2s_msg_recv func);
int agentdrv_unregister_s2s_msg_proc_func(enum agentdrv_s2s_msg_type msg_type);

/* local_devid:recv_side devid, sdid: send_side sdid */
int agentdrv_s2s_msg_send(u32 local_devid, u32 sdid, enum agentdrv_s2s_msg_type msg_type, u32 direction,
    struct data_input_info *data_info);
int agentdrv_s2s_async_msg_recv(u32 local_devid, u32 sdid, enum agentdrv_s2s_msg_type msg_type,
    struct data_recv_info *data_info);
int agentdrv_set_heartbeat_count(u32 devid, u64 count);
int agentdrv_get_heartbeat_count(u32 devid, u64 *count);
int devdrv_get_heartbeat_count(u32 devid, u64* count);
int devdrv_set_heartbeat_count(u32 devid, u64 count);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DEVDRV_MSG_INT */

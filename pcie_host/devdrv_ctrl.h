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

#ifndef _DEVDRV_CTRL_H_
#define _DEVDRV_CTRL_H_

#include "devdrv_interface.h"
#include "devdrv_msg.h"
#include "devdrv_pci.h"

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif

#define DEVDRV_MAX_DELAY_COUNT 20
#define DEVDRV_HOT_RESET_DELAY 3
#define DEVDRV_HOT_RESET_LONG_DELAY 5  /* FOR HCCS over PCIE */
#define DEVDRV_HOT_RESET_AFTER_DELAY 10
#define DEVDRV_HOT_RESAN_AFTER_DELAY 50
#define MAX_OS_EACH_AISERVER 2

#define DEVDRV_EP_SUSPEND_READY 1
#define DEVDRV_SECOND_TO_USECOND 1000000U
#define DEVDRV_GET_EP_SUSPEND_DELAY 20000U
#define DEVDRV_GET_EP_SUSPEND_TIMEOUT 900
#define DEVDRV_GET_EP_SUSPEND_RANGE 10
#define DEVDRV_SUSPEND_ONE_SECOND 50
#ifdef CFG_FEATURE_SRIOV
#define MAX_DEV_CNT 1124
#else
#define MAX_DEV_CNT 64
#endif
#define MAX_PF_DEV_CNT 64
#define MAX_VF_CNT_OF_PF 16U
#define DEVDRV_SRIOV_VF_DEVID_START 100U
#define DEVDRV_MAX_SRIOV_INSTANCE 8U

#define DEVDRV_PCIE_HOT_RESET_FLAG 0x5b5bb5b5
#define DEVDRV_LOAD_FILE_DELAY 10000
#define DEVDRV_DEVFN_BIT 3
#define DEVDRV_DEVFN_DEV_VAL 0x1F
#define DEVDRV_DEVFN_FN_VAL 0x7
#define DEVDRV_RESERVE_NUM 2
#define DEVDRV_MOVE_BIT_32 32
#define DEVDRV_MAX_FUNC_NUM 2
#define DEVDRV_DMI_DEV_TYPE_DEV_SLOT (-4)   /* same value as DMI_DEV_TYPE_DEV_SLOT defined in dmi.h */

#define DEVDRV_MAX_CAPABILITY_VALUE_NUM 2
#define DEVDRV_MAX_CAPABILITY_VALUE_OFFSET 4

#define DEVDRV_P2P_CAPABILITY_SHIFT_32 32
#define DEVDRV_P2P_ACCESS_DISABLE 0
#define DEVDRV_P2P_ACCESS_ENABLE  1
#define DEVDRV_P2P_ACCESS_UNKNOWN 2

#define DEVDRV_H2D_ATTR_INVALID 0
#define DEVDRV_H2D_ATTR_VALID 1

#define DEVDRV_P2P_CAPA_SIGN_BIT 24
#define DEVDRV_P2P_CAPA_SIGN_VAL 0xFFFFFF
#define DEVDRV_P2P_GROUP_SIGN_BIT 51
#define DEVDRV_P2P_GROUP_SIGN_VAL 0xF

#define REF_CNT_CHECK_PRINT_FREQUENCY  1000  /* 1s */
#define REF_CNT_CHECK_LIMIT 120000  /* 120s */
#define REF_CNT_CHECK_WAIT_L 1000   /* 1000us */
#define REF_CNT_CHECK_WAIT_H 1010   /* 1010us */

/* PCIe Speed value in regs */
enum devdrv_pcie_speed_reg_enum {
    DEVDRV_BANDW_PCIESPD_GEN1_REG = 1,
    DEVDRV_BANDW_PCIESPD_GEN2_REG = 2,
    DEVDRV_BANDW_PCIESPD_GEN3_REG = 3,
    DEVDRV_BANDW_PCIESPD_GEN4_REG = 4,
    DEVDRV_BANDW_PCIESPD_GEN5_REG = 5,
    DEVDRV_BANDW_PCIESPD_GEN6_REG = 6,
    DEVDRV_BANDW_PCIESPD_GEN_MAX
};

/* PCIe Speed value define */
#define DEVDRV_BANDW_INVALID_SPEED 0
#define DEVDRV_BANDW_X1_SPEED_GEN1 (256 * 1024 * 1024ULL)       // 256MB/s
#define DEVDRV_BANDW_X1_SPEED_GEN2 (512 * 1024 * 1024ULL)       // 512MB/s
#define DEVDRV_BANDW_X1_SPEED_GEN3 (1 * 1024 * 1024 * 1024ULL)  // 1GB/s
#define DEVDRV_BANDW_X1_SPEED_GEN4 (2 * 1024 * 1024 * 1024ULL)  // 2GB/s
#define DEVDRV_BANDW_X1_SPEED_GEN5 (4 * 1024 * 1024 * 1024ULL)  // 4GB/s
#define DEVDRV_BANDW_X1_SPEED_GEN6 (8 * 1024 * 1024 * 1024ULL)  // 8GB/s

struct devdrv_ops {
    /* trans msg */
    struct devdrv_msg_chan *(*alloc_trans_chan)(void *priv, struct devdrv_trans_msg_chan_info *chan_info);
    int (*realease_trans_chan)(struct devdrv_msg_chan *msg_chan);

    /* non-trans msg */
    struct devdrv_msg_chan *(*alloc_non_trans_chan)(void *priv, struct devdrv_non_trans_msg_chan_info *chan_info);
    int (*release_non_trans_chan)(struct devdrv_msg_chan *msg_chan);
};

/* record startup mini and report to dev manager */
#define DEVDRV_STATE_UNPROBE 0
#define DEVDRV_STATE_PROBE   1
struct devdrv_dev_state_ctrl {
    /* 所有mini个数 */
    u32 dev_num;
    /* 初次上报标记 */
    u32 first_flag;
    u32 reset_devid;
    u32 devid[MAX_DEV_CNT];
    u32 state_flag[MAX_DEV_CNT];
    devdrv_dev_startup_notify startup_callback;
    devdrv_dev_state_notify state_notifier_callback;
};

struct devdrv_pci_state_ctrl {
    devdrv_suspend_pre_check suspend_pre_check;
    devdrv_peer_fault_notify peer_fault_notify;
};

struct devdrv_pci_ctrl_mng {
    rwlock_t lock[MAX_DEV_CNT];
    struct devdrv_pci_ctrl **mng_table;
};

#define DEVDRV_P2P_MAX_PROC_NUM 32

struct devdrv_p2p_attr_info {
    int ref;
    int proc_ref[DEVDRV_P2P_MAX_PROC_NUM];
    int pid[DEVDRV_P2P_MAX_PROC_NUM];
};

#define DEVDRV_H2D_MAX_PROC_NUM 32
#define DEVDRV_H2D_MAX_ATU_NUM 6

struct devdrv_h2d_attr_info {
    int ref;
    int proc_ref[DEVDRV_H2D_MAX_PROC_NUM];
    int pid[DEVDRV_H2D_MAX_PROC_NUM];
    dma_addr_t addr;
    u64 size;
    u32 valid;
};

#define DEVDRV_RES_GEAR_RESV_NUM  5

struct devdrv_res_gear {
    u32 irq_res_gear;
    u32 reserve[DEVDRV_RES_GEAR_RESV_NUM];
};

#define DEVDRV_DMA_IOVA_RANGE_UNINIT 0
#define DEVDRV_DMA_IOVA_RANGE_INIT   1
struct devdrv_dma_iova_addr_range {
    int init_flag;
    u64 start_addr;
    u64 end_addr;
};

struct devdrv_dma_desc_rbtree_node {
    struct rb_node node;
    u64 hash_va;
    struct devdrv_dma_prepare *dma_prepare;
};
void devdrv_dma_desc_node_uninit(struct devdrv_pci_ctrl *pci_ctrl);

typedef int (*pci_bridge_secondary_bus_reset_func)(struct pci_dev *dev);
typedef void (*pci_reset_bridge_secondary_bus_func)(struct pci_dev *dev);

void devdrv_set_hccs_link_status(u32 dev_id, u32 val);

int devdrv_p2p_para_check(int pid, u32 dev_id, u32 peer_dev_id);
void devdrv_clear_p2p_resource(u32 devid);
void devdrv_clear_h2d_txatu_resource(u32 devid);
bool devdrv_is_dev_hot_reset(void);
struct devdrv_ctrl *get_devdrv_ctrl(void);
int devdrv_sriov_get_pf_devid_by_vf_ctrl(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_alloc_devid_check_ctrls(const struct devdrv_ctrl *ctrl_this);
int devdrv_alloc_devid_inturn(u32 beg, u32 stride);
int devdrv_alloc_devid_stride_2(struct devdrv_ctrl *ctrl_this);

void devdrv_slave_dev_delete(u32 dev_id);

u32 devdrv_get_devid_by_dev(const struct devdrv_msg_dev *msg_dev);

int devdrv_register_pci_devctrl(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_set_device_boot_status(struct devdrv_pci_ctrl *pci_ctrl, u32 status);

void devdrv_set_host_phy_mach_flag(u32 devid, u32 host_flag);
int devdrv_dev_online(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_dev_offline(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_ctrl_init(void);
void devdrv_ctrl_uninit(void);
void devdrv_set_ctrl_priv(u32 dev_id, void *priv);
int devdrv_init_pci_devctrl(struct devdrv_pci_ctrl *pci_ctrl);
struct devdrv_ctrl *devdrv_get_top_half_devctrl_by_id(u32 dev_id);
struct devdrv_ctrl *devdrv_get_bottom_half_devctrl_by_id(u32 dev_id);
struct devdrv_ctrl *devdrv_get_devctrl_by_id(u32 i);
struct devdrv_pci_ctrl *devdrv_get_pci_ctrl_by_id(u32 dev_id);
struct devdrv_pci_ctrl *devdrv_get_top_half_pci_ctrl_by_id(u32 dev_id);
struct devdrv_pci_ctrl *devdrv_get_bottom_half_pci_ctrl_by_id(u32 dev_id);
void devdrv_init_dev_startup_ctrl(void);
void drvdrv_dev_startup_record(u32 dev_id);
void drvdrv_dev_startup_report(u32 dev_id);
void drvdrv_dev_state_notifier(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_set_devctrl_startup_flag(u32 dev_id, enum devdrv_dev_startup_flag_type flag);
void devdrv_register_half_devctrl(struct devdrv_pci_ctrl *pci_ctrl);
struct pci_dev *devdrv_get_device_pf(struct pci_dev *pdev, unsigned int pf_num);
void devdrv_set_startup_status(struct devdrv_pci_ctrl *pci_ctrl, int status);
void devdrv_probe_wait(int devid);
int devdrv_alloc_attr_info(void);
void devdrv_free_attr_info(void);
void devdrv_clients_instance_uninit(void);
int devdrv_get_connect_protocol_by_dev(struct device *dev);

int devdrv_pci_ctrl_mng_init(void);
void devdrv_pci_ctrl_mng_uninit(void);
struct devdrv_pci_ctrl_mng *devdrv_get_pci_ctrl_mng(void);
void devdrv_pci_ctrl_add(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pci_ctrl_del_wait(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pci_ctrl_del(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl);
struct devdrv_pci_ctrl *devdrv_pci_ctrl_get(u32 dev_id);
struct devdrv_pci_ctrl *devdrv_pci_ctrl_get_no_ref(u32 dev_id);
void devdrv_pci_ctrl_put(struct devdrv_pci_ctrl *pci_ctrl);

u32 devdrv_get_irq_res_gear(void);
void devdrv_parse_res_gear(void);
u64 devdrv_get_bandwidth_info(struct pci_dev *pdev);

void *devdrv_pcimsg_alloc_non_trans_queue_inner(u32 dev_id, struct devdrv_non_trans_msg_chan_info *chan_info);
int devdrv_pcimsg_free_non_trans_queue_inner(void *msg_chan);

int devdrv_get_sriov_and_mdev_mode(u32 dev_id, u32 *boot_mode);
struct mutex *devdrv_get_ctrl_mutex(void);
int devdrv_hdc_suspend_precheck(int count);
void devdrv_peer_fault_notifier(u32 status);
int devdrv_get_pcie_id_info(u32 devid, struct devdrv_pcie_id_info *dev_info);
int devdrv_get_runtime_runningplat(u32 devid, u64 *running_plat);
int devdrv_set_runtime_runningplat(u32 devid, u64 running_plat);

#ifdef CFG_FEATURE_SRIOV
extern void *hw_dvt_hypervisor_dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_addr, gfp_t gfp);
extern void hw_dvt_hypervisor_dma_free_coherent(struct device *dev, size_t size, void *addr, dma_addr_t dma_addr);
extern dma_addr_t hw_dvt_hypervisor_dma_map_single(struct device *dev, void *ptr, size_t size,
    enum dma_data_direction dir);
extern void hw_dvt_hypervisor_dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir);
extern dma_addr_t hw_dvt_hypervisor_dma_map_page(struct device *dev, struct page *page, size_t offset, size_t size,
    enum dma_data_direction dir);
extern void hw_dvt_hypervisor_dma_unmap_page(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir);
#endif
#endif

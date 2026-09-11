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

#ifndef __DEVDRV_ADMIN_MSG_H_
#define __DEVDRV_ADMIN_MSG_H_

#include "devdrv_interface.h"

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVDRV_P2P_SUPPORT_MAX_DEVICE 4U
#define DEVDRV_H2D_SUPPORT_MAX_DEVICE 4U
#else
#ifdef CFG_FEATURE_P2P_EXTEND
#define DEVDRV_P2P_SUPPORT_MAX_DEVICE 64U
#define DEVDRV_H2D_SUPPORT_MAX_DEVICE 64U
#else
#define DEVDRV_P2P_SUPPORT_MAX_DEVICE 16U
#define DEVDRV_H2D_SUPPORT_MAX_DEVICE 16U
#endif
#endif

enum msg_queue_type {
    TRANSPARENT_MSG_QUEUE = 0,
    NON_TRANSPARENT_MSG_QUEUE,
    MSG_QUEUE_TYPE_MAX
};

#define DEVDRV_MSG_QUEUE_MEM_BASE 0x40000 /* 256KB */
#define DEVDRV_MSG_QUEUE_MEM_ALIGN 0x400 /* 1KB */

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVDRV_MSG_TIMEOUT 100000000       /* 100s for fpga  */
#define DEVDRV_MAX_MSG_TIMEOUT 100000000   /* 100s for fpga  */
#define DEVDRV_ADMIN_MSG_TIMEOUT 100000000 /* 100s for fpga  */
#define DEVDRV_S2S_MSG_TMOUT       200000000 /* 200s for fpga  */
#define DEVDRV_S2S_MSG_TMOUT_SHORT 20000000  /* 20s for fpga */
#define DEVDRV_MSG_D2H_TIMEOUT 100000000   /* 100s */
#define DEVDRV_MSG_TIMEOUT_LOG 30000000    /* 30s */
#define DEVDRV_MSG_IRQ_TIMEOUT 50000000    /* 50s */
#define DEVDRV_ADMIN_MSG_IRQ_TIMEOUT 50000000 /* 50s */
#define DEVDRV_MSG_IRQ_TIMEOUT_LOG 30000000 /* 30s */
#define DEVDRV_MSG_WAIT_MIN_TIME 200       /* 200us */
#define DEVDRV_MSG_WAIT_MAX_TIME 400       /* 400us */
#define DEVDRV_ADMIN_MSG_WAIT_MIN_TIME 200 /* 200us */
#define DEVDRV_ADMIN_MSG_WAIT_MAX_TIME 400 /* 400us */
#define DEVDRV_MSG_SCHED_STATUS_CHECK_TIME 10 /* 10s for fpga */
#else
#ifdef CFG_ENABLE_ASAN
#define DEVDRV_MSG_TIMEOUT 60000000        /* 60s for asan */
#define DEVDRV_MAX_MSG_TIMEOUT 60000000    /* 60s for asan */
#define DEVDRV_ADMIN_MSG_TIMEOUT 60000000  /* 60s for asan */
#define DEVDRV_S2S_MSG_TMOUT       100000000 /* 100s for asan */
#define DEVDRV_S2S_MSG_TMOUT_SHORT 3000000  /* 3s for asan */
#else
#define DEVDRV_MAX_MSG_TIMEOUT 35000000   /* 35s */
#if defined(CFG_SOC_PLATFORM_MDC_V11) || defined(CFG_SOC_PLATFORM_MDC_V51_LITE)
#define DEVDRV_MSG_TIMEOUT 5000000        /* 5s */
#define DEVDRV_ADMIN_MSG_TIMEOUT 3000000 /* 3s for admin */
#else
#define DEVDRV_MSG_TIMEOUT 5000000        /* 5s */
#define DEVDRV_ADMIN_MSG_TIMEOUT 15000000 /* 15s for admin */
#endif
#define DEVDRV_S2S_MSG_TMOUT       30000000 /* 30s */
#define DEVDRV_S2S_MSG_TMOUT_SHORT 1000000  /* 1s */
#endif
#define DEVDRV_MSG_D2H_TIMEOUT 30000000   /* 30s */
#define DEVDRV_MSG_TIMEOUT_LOG 1000000    /* 1s */
#define DEVDRV_MSG_IRQ_TIMEOUT 1000000    /* 1s */
#define DEVDRV_MSG_IRQ_TIMEOUT_LOG 100000 /* 100ms */
#if defined(CFG_SOC_PLATFORM_MDC_V11) || defined(CFG_SOC_PLATFORM_MDC_V51_LITE)
#define DEVDRV_ADMIN_MSG_IRQ_TIMEOUT 3000000 /* 3s for admin */
#define DEVDRV_MSG_WAIT_MIN_TIME 50        /* 50us */
#define DEVDRV_MSG_WAIT_MAX_TIME 60        /* 60us */
#define DEVDRV_ADMIN_MSG_WAIT_MIN_TIME 50 /* 50us */
#define DEVDRV_ADMIN_MSG_WAIT_MAX_TIME 60 /* 60us */
#else
#define DEVDRV_ADMIN_MSG_IRQ_TIMEOUT 5000000 /* 5s for admin */
#define DEVDRV_MSG_WAIT_MIN_TIME 1        /* 1us */
#define DEVDRV_MSG_WAIT_MAX_TIME 2        /* 2us */
#define DEVDRV_ADMIN_MSG_WAIT_MIN_TIME 10 /* 10us */
#define DEVDRV_ADMIN_MSG_WAIT_MAX_TIME 11 /* 11us */
#endif
#define DEVDRV_MSG_SCHED_STATUS_CHECK_TIME 3
#endif
#define DEVDRV_MSG_WAIT_DMA_FINISH_TIMEOUT 100 /* 100ms */
#define DEVDRV_MSG_TIME_VOERFLOW 5000          /* 5s */
#if defined(CFG_SOC_PLATFORM_MDC_V11) || defined(CFG_SOC_PLATFORM_MDC_V51_LITE)
#define DEVDRV_MSG_RETRY_LIMIT 0
#else
#define DEVDRV_MSG_RETRY_LIMIT 5
#endif
#define DEVDRV_NON_TRANS_CB_TIME 1000          /* 1s */

#define DEVDRV_DMA_READ_RESPONSE_ERROR 0x20
#define DEVDRV_DMA_WRITE_RESPONSE_ERROR 0x40
#define DEVDRV_DMA_DATA_POISON_RECEIVED 0x80
#define DEVDRV_DMA_COPY_RETRY_DELAY 3000000     /* 3s */
#define DEVDRV_S2S_MSG_RETRY_LIMIT 3

/*
 * pcie use first 1MB DDR space
 * base       length       use
 * 0          1k           reserved
 * 1k         1k           share_param
 * 2k         16k          p2p msg
 * 18k-120k                not used
 * 120k       4k           for tsdrv use
 * 124k       4k           for devmng use
 * 128k       1M-128k-4k   pcie msg chan
 * 1M-4k      4k           test
 */
#define DEVDRV_SHR_PARA_ADDR_OFFSET 0x400
#ifdef CFG_FEATURE_P2P_EXTEND
#define DEVDRV_SHR_PARA_ADDR_SIZE 0x800
#else
#define DEVDRV_SHR_PARA_ADDR_SIZE 0x400
#endif
#define DEVDRV_SHR_MEM_CACHE 1
#define DEVDRV_SHR_MEM_NORMAL 0

#define DEVDRV_P2P_MSG_SIZE 0x400 /* msg 1KB */
#define DEVDRV_P2P_SEND_MSG_ADDR_OFFSET 0
#define DEVDRV_P2P_SEND_MSG_ADDR_SIZE (DEVDRV_P2P_MSG_SIZE * DEVDRV_P2P_SUPPORT_MAX_DEVICE)
#define DEVDRV_P2P_RECV_MSG_ADDR_OFFSET (DEVDRV_P2P_SEND_MSG_ADDR_OFFSET + DEVDRV_P2P_SEND_MSG_ADDR_SIZE)
#define DEVDRV_P2P_RECV_MSG_ADDR_SIZE (DEVDRV_P2P_MSG_SIZE * DEVDRV_P2P_SUPPORT_MAX_DEVICE)
#define DEVDRV_P2P_MSG_ADDR_OFFSET (DEVDRV_SHR_PARA_ADDR_OFFSET + DEVDRV_SHR_PARA_ADDR_SIZE)
#define DEVDRV_P2P_MSG_ADDR_TOTAL_SIZE (DEVDRV_P2P_SEND_MSG_ADDR_SIZE + DEVDRV_P2P_RECV_MSG_ADDR_SIZE)

#define DEVDRV_PCIE_RESV_SPACE_NOT_USED_BASE 0x3B000
#define DEVDRV_RESERVE_TSDRV_RESV_OFFSET DEVDRV_PCIE_RESV_SPACE_NOT_USED_BASE
#define DEVDRV_RESERVE_TSDRV_RESV_SIZE 0x1000
#define DEVDRV_RESERVE_DEVMNG_RESV_OFFSET (DEVDRV_RESERVE_TSDRV_RESV_OFFSET + DEVDRV_RESERVE_TSDRV_RESV_SIZE)
#define DEVDRV_RESERVE_DEVMNG_RESV_SIZE 0x1000
#define DEVDRV_RESERVE_HBM_ECC_OFFSET (DEVDRV_RESERVE_DEVMNG_RESV_OFFSET + DEVDRV_RESERVE_DEVMNG_RESV_SIZE)
#define DEVDRV_RESERVE_HBM_ECC_SIZE 0x3000
#define DEVDRV_VF_BANDWIDTH_OFFSET 0x400 /* TS SRAM 1K~3K resv for bandwidth ctrl */
#define DEVDRV_VF_BANDWIDTH_SIZE 0x800 /* 2K */

/* cmd opreation code, first 8 bits is mudule name, later 8 bits is op type */
enum devdrv_admin_msg_opcode {
    /* msg chan */
    DEVDRV_CREATE_MSG_QUEUE,
    DEVDRV_FREE_MSG_QUEUE,
    DEVDRV_NOTIFY_DMA_ERR_IRQ,
    DEVDRV_GET_GELNERAL_INTERRUPT_DB_INFO,
    DEVDRV_NOTIFY_DEV_ONLINE,
    DEVDRV_CFG_P2P_MSG_CHAN,
    DEVDRV_CFG_TX_ATU,
    DEVDRV_GET_RX_ATU,
    DEVDRV_DMA_CHAN_REMOTE_OP,
    DEVDRV_HCCS_HOST_DMA_ADDR_MAP,
    DEVDRV_HCCS_HOST_DMA_ADDR_UNMAP,
    DEVDRV_SRIOV_EVENT_NOTIFY,
    DEVDRV_GET_EP_SUSPEND_STATUS,
    DEVDRV_HCCS_LINK_CHECK,
    DEVDRV_ADMIN_MSG_MAX
};

#define DEVDRV_MSG_CMD_BEGIN             1
#define DEVDRV_MSG_CMD_FINISH_SUCCESS    2
#define DEVDRV_MSG_CMD_FINISH_FAILED     3
#define DEVDRV_MSG_CMD_INVALID_PARA      4
#define DEVDRV_MSG_CMD_NULL_PROCESS_CB   5
#define DEVDRV_MSG_CMD_SEND_HOST_FAILED  6
#define DEVDRV_MSG_CMD_CB_PROCESS_FAILED 7
#define DEVDRV_MSG_CMD_COPY_FAILED       8
#define DEVDRV_MSG_CMD_IRQ_BEGIN  0x69727173   /* irq handle begin, schedule work */
#define DEVDRV_MSG_CMD_IRQ_FINISH 0x8F7D6C5B   /* irq notify success, return */
#define DEVDRV_MSG_CMD_IRQ_BUSY   0x1A3B7C9D   /* irq notify busy, return for host retry */

#define DEVDRV_MSG_HANDLE_STATE_INIT 1
#define DEVDRV_MSG_HANDLE_STATE_SCHEDING 2

#define DEVDRV_OP_ADD 1
#define DEVDRV_OP_DEL 2

#define DEVDRV_MSIX_READY_FLAG 0x72636F6B
struct devdrv_create_queue_command {
    u32 msg_type;   /* enum devdrv_msg_client_type */
    u32 queue_type; /* enum msg_queue_type */
    u32 queue_id;
    u64 sq_dma_base_host;
    u64 cq_dma_base_host;
    u32 sq_desc_size;
    u32 cq_desc_size;
    u16 sq_depth;
    u16 cq_depth;
    u32 sq_slave_mem_offset;
    u32 cq_slave_mem_offset;
    s32 irq_tx_finish_notify;
    s32 irq_rx_msg_notify;
};

struct devdrv_free_queue_cmd {
    u32 queue_id;
};

struct devdrv_notify_dma_err_irq_cmd {
    u32 dma_chan_id;
    s32 err_irq;
};

struct devdrv_general_interrupt_db_info {
    u32 db_start;
    u32 db_num;
};

struct devdrv_notify_dev_online_cmd {
    u32 devid;
    u32 status;
};

struct devdrv_p2p_msg_chan_cfg_cmd {
    u32 op;
    u32 devid;
};

struct devdrv_tx_atu_cfg_cmd {
    u32 op;
    u32 devid;
    u32 atu_type;
    u64 phy_addr;
    u64 target_addr;
    u64 target_size;
    u64 atu_base_addr;
};

struct devdrv_get_rx_atu_cmd {
    u32 devid;
    u32 bar_num;
};

struct devdrv_ep_suspend_status {
    u32 status;
    u32 reserved;
};

struct devdrv_ep_suspend_cmd {
    u32 hand_shake_flag;
};

struct devdrv_alloc_msg_chan_reply{
    dma_addr_t sq_rsv_dma_addr_d;
    dma_addr_t cq_rsv_dma_addr_d;
};

#define DMA_CHAN_REMOTE_OP_RESET 0
#define DMA_CHAN_REMOTE_OP_INIT 1
#define DMA_CHAN_REMOTE_OP_ERR_PROC 2

struct devdrv_dma_chan_remote_op {
    u32 op;
    u32 chan_id;
    u32 pf_num;
    u32 vf_num;
    u64 sq_desc_dma;
    u64 cq_desc_dma;
    u32 sq_depth;
    u32 cq_depth;
    u32 sqcq_side;
    u32 sriov_flag;
};

struct devdrv_hccs_host_dma_addr {
    u64 phy_addr;
    u64 dma_addr;
    u64 size;
};

/* DMA single node read and write command */
struct devdrv_dma_single_node_command {
    struct devdrv_dma_node dma_node;
};

/* DMA chained read and write command */
struct devdrv_dma_chain_command {
    u64 dma_node_base;
    u32 node_cnt;
};

struct devdrv_admin_msg_command {
    u32 opcode;
    u32 status;
    char data[];
};

struct devdrv_admin_msg_reply {
    u32 len;  // contain 'len' own occupied space
    char data[];
};

#define DEVDRV_SRIOV_ENABLE 1
#define DEVDRV_SRIOV_DISABLE 0
struct devdrv_sriov_event_notify_cmd {
    u32 devid;
    u32 status;
};

/* The command channel uses a memory synchronous call */
#define DEVDRV_ADMIN_MSG_QUEUE_DEPTH 1
#define DEVDRV_ADMIN_MSG_QUEUE_BD_SIZE 0x400

#define DEVDRV_ADMIN_MSG_HEAD_LEN sizeof(struct devdrv_admin_msg_command)
#define DEVDRV_ADMIN_MSG_DATA_LEN (DEVDRV_ADMIN_MSG_QUEUE_BD_SIZE - DEVDRV_ADMIN_MSG_HEAD_LEN)

struct devdrv_non_trans_msg_desc {
    u64 seq_num;      /* msg sequence number */
    u32 in_data_len;  /* input real length */
    u32 out_data_len; /* output max length */
    u32 real_out_len; /* output real length */
    u32 msg_type;     /* enum devdrv_common_msg_type */
    u32 status;       /* DEVDRV_MSG_CMD_* */
    u32 reserve;
    char data[];
};

#define DEVDRV_NON_TRANS_MSG_HEAD_LEN sizeof(struct devdrv_non_trans_msg_desc)

#define DEVDRV_SHR_PARA_PRE_CFG_LEN 8

#define PHY_MATCH_FLAG_OFFSET_IN_SHR_MEM 0x30
#define DEVDRV_SHR_PARA_RESV 27
struct devdrv_shr_para {
    int load_flag;      /* D2H: device bios notice host to load device os via pcie. 0: no, 1 yes */
    int chip_id;        /* D2H: device bios notice host: cloud ai server, index in one node(4P 0-3); others 0 */
    int node_id;        /* D2H: device bios notice host: cloud ai server has total 8P, one node has 4p, which node */
    int slot_id;        /* D2H: device bios notice host: slot_id (0-7) */
    int board_type;     /* D2H: device bios notice host: cloud pcie card, ai server, evb */
    int chip_type;      /* D2H: mini cloud */
    int driver_version; /* H2D: host notice device bios: driver version */
    int dev_num;        /* D2H: device bios notice host: only cloud v2 set, other chip device driver set */
    int hot_reset_flag; /* CCPU set, bios read */
    int hot_reset_pcie_flag; /* H2D: make sure pcie report hotreset */
    int total_func_num; /* D2H: device driver set */
    /*
     * D2H: device bios capability flag: bit0: imu export reg, other reserved,
     * make sure phy_match_flag offset=0x30
     */
    u32 capability;
    u32 phy_match_flag;      /* HOST set, phy machine flag */
    /* D2H: bios set hccs link status, bit[x]=1 means the hccs between this chip and chip[x] is linked */
    u32 hccs_status;
    u32 hccs_group_id[HCCS_GROUP_SUPPORT_MAX_CHIPNUM]; /* D2H: bios set expected hccs grouping of the whole system */
    int host_dev_id;         /* H2D: the dev id in host side */
    int host_interrupt_flag; /* H2D: host notice device has receive interrupt begin half probe. 0: no, 1 yes */
    u32 ep_pf_index;         /* H2D: EP PF INDEX */
    int connect_protocol;    /* H2D: pcie, hccs */
    u32 admin_msg_status;    /* host set begin, device irq set irq_begin, host continue */
    u64 admin_chan_sq_base;  /* H2D: cloud v2(hccs), host notice phy addr; others, host notice dma addr */
    u64 host_mem_bar_base;   /* H2D: mem bar 4 base addr */
    u64 host_io_bar_base;    /* H2D: io bar 2 base addr */
    u64 tx_atu_base_addr1;   /* D2H: cloud only, high 256GB address space */
    u64 tx_atu_base_size1;   /* D2H: cloud only, high 256GB space size */
    u64 tx_atu_base_addr2;   /* D2H: cloud only, low 128MB address space */
    u64 tx_atu_base_size2;   /* D2H: cloud only, low 128MB space size */
    u32 pre_cfg[DEVDRV_SHR_PARA_PRE_CFG_LEN];          /* H2D: resv 8 */
    u64 resv[DEVDRV_SHR_PARA_RESV]; /* Keep offset constant and reserve for new flags */
    volatile u64 host_heartbeat_count; /* devmng set in host, get in device */
    volatile u64 heartbeat_count; /* devmng set in device, get in host */
    volatile u64 runtime_runningplat; /* runtime running in device(0xdece) or host(0xa5a5), runtime stop:0xABCD */
    u32 msix_offset;         /* H2D: 1pf2p drv notice msix offset of p1 to device drv */
    u32 vf_id;               /* D2H: device driver set */
    u64 sq_desc_dma;         /* H2D: host set, mdev pm dma desc addr */
    u64 dma_bitmap;          /* D2H: dma channel bitmap for vf, 0-irrelevant, 1-allocated */
    u32 rc_msix_ready_flag;  /* H2D: rc has registered MSI interrupt */
    u32 sid;                 /* H2D: host pdev's sid, agent smmu use it for iova to pa */
    u32 bar_wc_flag;         /* OS set, bit0~5:bar0~bar5, 1 is wc attribute, 0 is not attribute */
    int addr_mode;           /* HOST set, only vf user */
    u64 p2p_msg_base_addr[DEVDRV_P2P_SUPPORT_MAX_DEVICE]; /* H2D: cloud only, p2p msg base for dma */
    u64 p2p_db_base_addr[DEVDRV_P2P_SUPPORT_MAX_DEVICE];  /* H2D: cloud only, p2p doorbell base for dma */
    u32 vm_full_spec_flag;   /* D2H: mdev only, is 1:1 vf mdev */
};

#endif

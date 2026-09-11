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
#include <linux/version.h>
#include <linux/dmi.h>

#include "res_drv.h"
#include "devdrv_util.h"
#include "devdrv_ctrl.h"
#include "devdrv_pm.h"
#include "devdrv_pcie_link_info.h"
#include "res_drv_mini_v3.h"

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVDRV_MINI_V3_P2P_SUPPORT_MAX_DEVICE 4
#else
#define DEVDRV_MINI_V3_P2P_SUPPORT_MAX_DEVICE 8
#endif

#define PCI_BAR_RSV_MEM 0
#define PCI_BAR_IO 2
#define PCI_BAR_MEM 4

/* BAR 0 64MB */
#define DEVDRV_RESERVE_MEM_MSG_OFFSET 0x20000

#define DEVDRV_RESERVE_MEM_MSG_SIZE (16 * 1024 * 1024)        /* 16MB */
#define DEVDRV_MDC_RESERVE_MEM_MSG_SIZE (8 * 1024 * 1024 - 128 * 1024)   /* 8MB - 128KB */

#define DEVDRV_RESERVE_MEM_TEST_OFFSET 0x1010000
#define DEVDRV_RESERVE_MEM_TEST_SIZE (64 * 1024)              /* 64KB */

#define DEVDRV_RESERVE_MEM_TOPIC_SCHED_OFFSET 0x1070000
#define DEVDRV_RESERVE_MEM_TOPIC_SCHED_SIZE (960 * 1024)     /* 0.9375MB */

#define DEVDRV_RESERVE_MEM_DEVMNG_OFFSET 0x1160000
#define DEVDRV_RESERVE_MEM_DEVMNG_SIZE (1 * 1024 * 1024)      /* 1MB */

#define DEVDRV_RESERVE_MEM_IMU_LOG_OFFSET 0x1260000
#define DEVDRV_RESERVE_MEM_IMU_LOG_SIZE (5 * 1024 * 1024)     /* 5MB */

#define DEVDRV_RESERVE_MEM_KERNEL_LOG_OFFSET 0x1760000
#define DEVDRV_RESERVE_MEM_KERNEL_LOG_SIZE (2 * 1024 * 1024)  /* 2MB */

#define DEVDRV_RESERVE_MEM_TSDRV_LOG_OFFSET 0x1960000
#define DEVDRV_RESERVE_MEM_TSDRV_LOG_SIZE (2 * 1024 * 1024)   /* 2MB */

#define DEVDRV_RESERVE_MEM_HBOOT_LOG_OFFSET 0x1b60000
#define DEVDRV_RESERVE_MEM_HBOOT_LOG_SIZE (8 * 1024 * 1024)   /* 8MB */

#define DEVDRV_RESERVE_MEM_BBOX_OFFSET 0x2360000
#define DEVDRV_RESERVE_MEM_BBOX_SIZE (15 * 1024 * 1024)       /* 15MB */

#define DEVDRV_RESERVE_MEM_HDR_OFFSET 0x2fe0000
#define DEVDRV_RESERVE_MEM_HDR_SIZE (512 * 1024)              /* 512KB */

#define DEVDRV_RESERVE_MEM_TS_SQ_OFFSET 0x3260000
#define DEVDRV_RESERVE_MEM_TS_SQ_SIZE (72 * 1024 * 1024)      /* 72MB */

/* BAR 2 128MB */
#define DEVDRV_IEP_DMA_OFFSET 0x0
#define DEVDRV_IEP_DMA_SIZE (6 * 0x100)                       /* channel 6~11 for host */

#define DEVDRV_IO_LOAD_SRAM_OFFSET 0x4000
#define DEVDRV_IO_LOAD_SRAM_SIZE (102 * 1024)                 /* 102KB */

#define DEVDRV_IEP_SDI0_OFFSET 0x24000
#define DEVDRV_IEP_SDI0_SIZE (64 * 1024)                      /* 64KB */

#define DEVDRV_IO_TS_SRAM_OFFSET 0x34000
#define DEVDRV_IO_TS_SRAM_SIZE (128 * 1024)                   /* 128KB */

#define DEVDRV_IO_TS_DB_OFFSET 0x54000
#define DEVDRV_IO_TS_DB_SIZE (2 * 1024 * 1024)                /* 2MB */

#define DEVDRV_IO_STARS_TOPIC_SCHED_OFFSET 0x254000
#define DEVDRV_IO_STARS_TOPIC_SCHED_SIZE (8 * 1024 * 1024)    /* 8MB */

#define DEVDRV_IO_HWTS_EVENT_OFFSET 0xa54000
#define DEVDRV_IO_HWTS_EVENT_SIZE (1 * 1024 * 1024)          /* 1MB */

#define DEVDRV_IO_STARS_SQCQ_OFFSET 0xb54000
#define DEVDRV_IO_STARS_SQCQ_SIZE (2 * 1024 * 1024)           /* 2MB */

#define DEVDRV_IO_STARS_SQCQ_INTR_OFFSET 0xd54000
#define DEVDRV_IO_STARS_SQCQ_INTR_SIZE (2 * 1024 * 1024)      /* 2MB */

#define DEVDRV_IO_STARS_CDQM_OFFSET 0x1000000
#define DEVDRV_IO_STARS_CDQM_SIZE (16 * 1024 * 1024)          /* 16MB */

#define DEVDRV_IEP_SDI0_DB_OFFSET 0x1000
#define DEVDRV_DB_IOMAP_SIZE  0x1000

/* ************ interrupt defined for normal host ************* */
#define DEVDRV_MSI_X_MAX_VECTORS 128
#define DEVDRV_MSI_X_MIN_VECTORS 128

/* device os load notify use irq vector 0, later 0 alse use to admin msg chan */
#define DEVDRV_LOAD_MSI_X_VECTOR_NUM 0

/* irq used to msg trans, a msg chan need two vector. one for tx finish, the other for rx msg.
   msg chan 0 is used to admin(chan 0) role */
#define DEVDRV_MSG_MSI_X_VECTOR_BASE 0
#define DEVDRV_MSG_MSI_X_VECTOR_NUM 64

/* irq used to dma, a dma chan need 22 vector. one for cq, the other for err.
  host support 11 dma chan witch is related to enum devdrv_dma_data_type */
#define DEVDRV_DMA_MSI_X_VECTOR_BASE 64
#define DEVDRV_DMA_MSI_X_VECTOR_NUM 12

/* irq used to tsdrv: mailbox, log, stars cq */
#define DEVDRV_TSDRV_MSI_X_VECTOR_BASE 80
#define DEVDRV_TSDRV_MSI_X_VECTOR_NUM 40

#define DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_BASE 125
#define DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_NUM 1

#define DEVDRV_CDQM_MSI_X_VECTOR_BASE 126
#define DEVDRV_CDQM_MSI_X_VECTOR_NUM 2

#define DMA_CHAN_REMOTE_USED_NUM            6
#define DMA_CHAN_REMOTE_USED_START_INDEX    6
#define DMA_CHAN_BAR_SPACE_USED_START_INDEX 0

#define DEVDRV_MDC_MAX_DMA_CH_SQ_DEPTH 0x5000
#define DEVDRV_MDC_MAX_DMA_CH_CQ_DEPTH 0x5000
#define DEVDRV_MAX_DMA_CH_SQ_DEPTH 0x10000
#define DEVDRV_MAX_DMA_CH_CQ_DEPTH 0x10000
#define DEVDRV_DMA_CH_SQ_DESC_RSV  0x400

#define DEVDRV_MAX_MSG_PF_CHAN_CNT 60
#define DEVDRV_MAX_MSG_VF_CHAN_CNT 0
/* msg chan cnt for modules */
#define DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX          1   /* non_trans:1 */
#define DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX         1   /* non_trans:1 */
#define DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX        1   /* non_trans:1 */
#define DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_TSDRV_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */
#define DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX          17   /* trans:16 non_trans:1 */
#define DEVDRV_QUEUE_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */

#define MINI_V3_BLOCKS_NUM 10
#define MINI_V3_MODULE_NUM 6

#define AP_IOB_TX_INT_MSK2_REG  0x808
#define AP_IOB_TX_INT_MSK3_REG  0x80C
#define AP_IOB_RX_INT_MSK_REG   0x5700
#define AP_ABNORMAL_INT_MSK_REG 0xC504
#define AP_INT_ECC_ERR_MSK_REG  0xC608
#define CORE_TL_INT_MSK0_REG    0x20574
#define CORE_MAC_INT_MSK_REG    0x70058
#define CORE_DL_INT_MSK_REG     0x6008C
#define PCIE_CE_STATUS_REG      0x8020
#define PCIE_UNF_STATUS_REG     0x8028
#define PCIE_UF_STATUS_REG      0x8030
#define PCIE_CE_ENA_REG         0x8008
#define PCIE_UNF_ENA_REG        0x8010
#define PCIE_UF_ENA_REG         0x8018
#define PCIE_CE_MASK_REG        0x800C
#define PCIE_UNF_MASK_REG       0x8014
#define PCIE_UF_MASK_REG        0x801C

STATIC struct devdrv_load_file_cfg mini_v3_file[MINI_V3_BLOCKS_NUM] = {
    {
        .file_name = "/driver/device/ascend_310b_usercfg.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b_syscfg.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b_ddr.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b_hrt.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b_atf.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b_hboot2.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b_tee.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b_dt.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b.image",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/driver/device/ascend_310b.cpio.gz",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
};

STATIC struct devdrv_depend_module mini_v3_module[MINI_V3_MODULE_NUM] = {
    {
        .module_name = "drv_soft_fault",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ascend_queue",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "drv_dp_proc_mng_host",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "drv_pcie_vnic_host",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ascend_soc_platform",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ascend_trs_shrid",
        .status = DEVDRV_MODULE_UNPROBED,
    },
};

STATIC unsigned int devdrv_msg_chan_cnt_mini_v3[devdrv_msg_client_max] = {
    DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX,    /* used for pcivnic */
    DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX,       /* used for test */
    DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX,      /* used for svm */
    DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX,     /* used for common */
    DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX,    /* used for device manager */
    DEVDRV_TSDRV_MSG_CHAN_CNT_MAX,          /* used for tsdrv */
    DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX,        /* used for hdc */
    DEVDRV_QUEUE_MSG_CHAN_CNT_MAX,          /* used for queue */
};

STATIC void devdrv_mini_v3_init_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.msg_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_MSG_OFFSET;

    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        pci_ctrl->res.msg_mem.size = DEVDRV_RESERVE_MEM_MSG_SIZE;
    } else {
        pci_ctrl->res.msg_mem.size = DEVDRV_MDC_RESERVE_MEM_MSG_SIZE;
    }

    pci_ctrl->res.test.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_TEST_OFFSET;
    pci_ctrl->res.test.size = DEVDRV_RESERVE_MEM_TEST_SIZE;

    pci_ctrl->res.devmng_resv.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_DEVMNG_OFFSET;
    pci_ctrl->res.devmng_resv.size = DEVDRV_RESERVE_MEM_DEVMNG_SIZE;
    pci_ctrl->res.hbm_ecc_mem.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_HBM_ECC_OFFSET;
    pci_ctrl->res.hbm_ecc_mem.size = DEVDRV_RESERVE_HBM_ECC_SIZE;

    pci_ctrl->res.imu_log.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_IMU_LOG_OFFSET;
    pci_ctrl->res.imu_log.size = DEVDRV_RESERVE_MEM_IMU_LOG_SIZE;

    pci_ctrl->res.tsdrv_resv.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_TSDRV_LOG_OFFSET;
    pci_ctrl->res.tsdrv_resv.size = DEVDRV_RESERVE_MEM_TSDRV_LOG_SIZE;

    pci_ctrl->res.bbox.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_KERNEL_LOG_OFFSET;
    pci_ctrl->res.bbox.size = DEVDRV_RESERVE_MEM_KERNEL_LOG_SIZE;

    pci_ctrl->res.ts_sq.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_TS_SQ_OFFSET;
    pci_ctrl->res.ts_sq.size = DEVDRV_RESERVE_MEM_TS_SQ_SIZE;

    pci_ctrl->res.msg_db.addr = pci_ctrl->io_phy_base + DEVDRV_IEP_SDI0_OFFSET + DEVDRV_IEP_SDI0_DB_OFFSET;
    pci_ctrl->res.msg_db.size = DEVDRV_DB_IOMAP_SIZE;

    pci_ctrl->res.ts_db.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_DB_OFFSET;
    pci_ctrl->res.ts_db.size = DEVDRV_IO_TS_DB_SIZE;

    pci_ctrl->res.ts_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_SRAM_OFFSET;
    pci_ctrl->res.ts_sram.size = DEVDRV_IO_TS_SRAM_SIZE;

    pci_ctrl->res.load_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_LOAD_SRAM_OFFSET;
    pci_ctrl->res.load_sram.size = DEVDRV_IO_LOAD_SRAM_SIZE;

    pci_ctrl->res.stars_sqcq.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_SQCQ_OFFSET;
    pci_ctrl->res.stars_sqcq.size = DEVDRV_IO_STARS_SQCQ_SIZE;

    pci_ctrl->res.stars_topic_sched.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_TOPIC_SCHED_OFFSET;
    pci_ctrl->res.stars_topic_sched.size = DEVDRV_IO_STARS_TOPIC_SCHED_SIZE;

    pci_ctrl->res.stars_cdqm.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_CDQM_OFFSET;
    pci_ctrl->res.stars_cdqm.size = DEVDRV_IO_STARS_CDQM_SIZE;

    pci_ctrl->res.stars_sqcq_intr.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_SQCQ_INTR_OFFSET;
    pci_ctrl->res.stars_sqcq_intr.size = DEVDRV_IO_STARS_SQCQ_INTR_SIZE;

    pci_ctrl->res.stars_topic_sched_rsv_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_TOPIC_SCHED_OFFSET;
    pci_ctrl->res.stars_topic_sched_rsv_mem.size = DEVDRV_RESERVE_MEM_TOPIC_SCHED_SIZE;

    pci_ctrl->res.ts_event.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HWTS_EVENT_OFFSET;
    pci_ctrl->res.ts_event.size = DEVDRV_IO_HWTS_EVENT_SIZE;

    pci_ctrl->res.hdr.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_HDR_OFFSET;
    pci_ctrl->res.hdr.size = DEVDRV_RESERVE_MEM_HDR_SIZE;

    /* not support */
    pci_ctrl->res.vpc.size = 0;
    pci_ctrl->res.dvpp.size = 0;
    pci_ctrl->res.hwts.size = 0;
    pci_ctrl->res.ts_notify.size = 0;
    pci_ctrl->res.reg_sram.size = 0;
    pci_ctrl->res.stars_intr.size = 0;
    pci_ctrl->res.vf_bandwidth.addr = 0;
    pci_ctrl->res.vf_bandwidth.size = 0;
    pci_ctrl->res.ts_share_mem.addr = 0;
    pci_ctrl->res.ts_share_mem.size = 0;
    pci_ctrl->res.l3d_sram.size = 0;
    pci_ctrl->res.kdump.addr = 0;
    pci_ctrl->res.kdump.size = 0;
    pci_ctrl->res.vmcore.addr = 0;
    pci_ctrl->res.vmcore.size = 0;
    pci_ctrl->res.devmng_info_mem.size = 0;
    pci_ctrl->res.bbox_ddr_dump.size = 0;
    pci_ctrl->res.ts_log.size = 0;
    pci_ctrl->res.chip_dfx.size = 0;
}

STATIC void devdrv_mini_v3_init_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_intr_info *intr = &pci_ctrl->res.intr;

    intr->min_vector = DEVDRV_MSI_X_MIN_VECTORS;
    intr->max_vector = DEVDRV_MSI_X_MAX_VECTORS;
    intr->device_os_load_irq = DEVDRV_LOAD_MSI_X_VECTOR_NUM;
    intr->msg_irq_base = DEVDRV_MSG_MSI_X_VECTOR_BASE;
    intr->msg_irq_num = DEVDRV_MSG_MSI_X_VECTOR_NUM;
    intr->dma_irq_base = DEVDRV_DMA_MSI_X_VECTOR_BASE;
    intr->dma_irq_num = DEVDRV_DMA_MSI_X_VECTOR_NUM;
    intr->tsdrv_irq_base = DEVDRV_TSDRV_MSI_X_VECTOR_BASE;
    intr->tsdrv_irq_num = DEVDRV_TSDRV_MSI_X_VECTOR_NUM;
    intr->topic_sched_irq_base = DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_BASE;
    intr->topic_sched_irq_num = DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_NUM;
    intr->cdqm_irq_base = DEVDRV_CDQM_MSI_X_VECTOR_BASE;
    intr->cdqm_irq_num = DEVDRV_CDQM_MSI_X_VECTOR_NUM;
    intr->msg_irq_vector2_base = 0;
    intr->msg_irq_vector2_num = 0;
    intr->tsdrv_irq_vector2_base = 0;
    intr->tsdrv_irq_vector2_num = 0;
}

STATIC void devdrv_mini_v3_init_dma_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    u32 i;

    pci_ctrl->res.dma_res.dma_addr = pci_ctrl->io_base + DEVDRV_IEP_DMA_OFFSET;
    pci_ctrl->res.dma_res.dma_chan_addr = pci_ctrl->res.dma_res.dma_addr;
    pci_ctrl->res.dma_res.dma_chan_start_id = DMA_CHAN_REMOTE_USED_START_INDEX;
    pci_ctrl->res.dma_res.chan_start_id = DMA_CHAN_BAR_SPACE_USED_START_INDEX;
    pci_ctrl->res.dma_res.dma_chan_num = DMA_CHAN_REMOTE_USED_NUM;
    for (i = 0; i < pci_ctrl->res.dma_res.dma_chan_num; i++) {
        pci_ctrl->res.dma_res.use_chan[i] = i;
    }
    pci_ctrl->res.dma_res.pf_num = pci_ctrl->func_id;
    pci_ctrl->res.dma_res.vf_id = 0;

    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        pci_ctrl->res.dma_res.sq_depth = DEVDRV_MAX_DMA_CH_SQ_DEPTH;
        pci_ctrl->res.dma_res.cq_depth = DEVDRV_MAX_DMA_CH_CQ_DEPTH;
    } else {
        pci_ctrl->res.dma_res.sq_depth = DEVDRV_MDC_MAX_DMA_CH_SQ_DEPTH;
        pci_ctrl->res.dma_res.cq_depth = DEVDRV_MDC_MAX_DMA_CH_CQ_DEPTH;
    }
    pci_ctrl->res.dma_res.sq_rsv_num = DEVDRV_DMA_CH_SQ_DESC_RSV;
}

STATIC void devdrv_mini_v3_init_load_file_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.load_file.load_file_num = MINI_V3_BLOCKS_NUM;
    pci_ctrl->res.load_file.load_file_cfg = mini_v3_file;
}

STATIC void devdrv_mini_v3_init_depend_module_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.depend_info.module_num = MINI_V3_MODULE_NUM;
    pci_ctrl->res.depend_info.module_list = mini_v3_module;
}

#define DEV_ID_START 0
#define DEV_ID_START_MDC 1
#define DEV_ID_STRIDE 1

STATIC int devdrv_mini_v3_alloc_devid(struct devdrv_ctrl *ctrl_this)
{
    u32 dev_id_start;
    int dev_id;

    dev_id = devdrv_alloc_devid_check_ctrls(ctrl_this);
    if (dev_id != -1) {
        return dev_id;
    }

    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        dev_id_start = DEV_ID_START;
    } else {
        dev_id_start = DEV_ID_START_MDC;
    }

    return devdrv_alloc_devid_inturn(dev_id_start, DEV_ID_STRIDE);
}

STATIC int devdrv_mini_v3_is_p2p_access_cap(struct devdrv_pci_ctrl *pci_ctrl,
                                            struct devdrv_pci_ctrl *peer_pci_ctrl)
{
    devdrv_info("This chip_type not support p2p. (dev_id=%u; chip_type=%d)\n", pci_ctrl->dev_id,
        pci_ctrl->shr_para->chip_type);
    return DEVDRV_P2P_ACCESS_DISABLE;
}

STATIC enum devdrv_load_wait_mode devdrv_mini_v3_get_load_wait_mode(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->ops.pre_cfg != NULL) {
        pci_ctrl->ops.pre_cfg(pci_ctrl);
    }

    return DEVDRV_LOAD_WAIT_INTERVAL;
}

STATIC int devdrv_mini_v3_get_pf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_PF_CHAN_CNT;
}

STATIC int devdrv_mini_v3_get_vf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_VF_CHAN_CNT;
}

STATIC int devdrv_mini_v3_get_p2p_support_max_devnum(void)
{
    return DEVDRV_MINI_V3_P2P_SUPPORT_MAX_DEVICE;
}

STATIC void devdrv_mask_suspend_fault(void)
{
    void __iomem *apb_base = NULL;

    apb_base = ioremap(PCIE_BASE_ADDR, PCIE_REG_SIZE);
    if (apb_base == NULL) {
        devdrv_err("ioremap fail, apb_base is NULL.\n");
        return;
    }

    /* ras */
    writel(0xFFFFFFFFU, apb_base + AP_IOB_TX_INT_MSK2_REG);
    writel(0xFFFFFFFFU, apb_base + AP_IOB_TX_INT_MSK3_REG);
    writel(0x1FF, apb_base + AP_IOB_RX_INT_MSK_REG);
    writel(0xF, apb_base + AP_ABNORMAL_INT_MSK_REG);
    writel(0xFF, apb_base + AP_INT_ECC_ERR_MSK_REG);
    writel(0xFFFFFFFFU, apb_base + CORE_TL_INT_MSK0_REG);
    writel(0xFFFFFFFFU, apb_base + CORE_MAC_INT_MSK_REG);
    writel(0x7FFFF, apb_base + CORE_DL_INT_MSK_REG);

    /* aer */
    writel(0x0U, apb_base + PCIE_CE_ENA_REG);
    writel(0x0U, apb_base + PCIE_UNF_ENA_REG);
    writel(0x0U, apb_base + PCIE_UF_ENA_REG);
    writel(0xFFFFU, apb_base + PCIE_CE_MASK_REG);
    writel(0xFFFFU, apb_base + PCIE_UNF_MASK_REG);
    writel(0xFFFFU, apb_base + PCIE_UF_MASK_REG);

    iounmap(apb_base);
    apb_base = NULL;
    return;
}

STATIC int devdrv_mini_v3_deal_suspend_handshake(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    ret = devdrv_dev_get_suspend_status(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Get suspend status failed. (ret=%d; dev_id=%u)\n", ret, pci_ctrl->dev_id);
        return ret;
    }
    devdrv_mask_suspend_fault();
    devdrv_set_dma_status(pci_ctrl->dma_dev, DEVDRV_DMA_DEAD);
    /* report remove to dev manager */
    drvdrv_dev_state_notifier(pci_ctrl);
    if ((pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_PF) &&
        (pci_ctrl->module_exit_flag != DEVDRV_REMOVE_CALLED_BY_MODULE_EXIT)) {
        devdrv_set_device_status(pci_ctrl, DEVDRV_DEVICE_DEAD);
    }
    devdrv_set_pcie_channel_status(DEVDRV_PCIE_COMMON_CHANNEL_DEAD);
    ret = uda_dev_ctrl(pci_ctrl->dev_id, UDA_CTRL_SUSPEND);
    if (ret != 0) {
        devdrv_err("uda ctrl suspend fail. (ret=%d; dev_id=%u)\n", ret, pci_ctrl->dev_id);
        return ret;
    }
    return 0;
}

STATIC void devdrv_mini_v3_set_dev_shr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para->host_dev_id = (int)pci_ctrl->dev_id;
    pci_ctrl->shr_para->host_mem_bar_base = (u64)pci_ctrl->mem_phy_base;
    pci_ctrl->shr_para->host_io_bar_base = (u64)pci_ctrl->io_phy_base;
}

STATIC void devdrv_mini_v3_ops_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->ops.shr_para_rebuild = NULL;
    pci_ctrl->ops.alloc_devid = devdrv_mini_v3_alloc_devid;
    pci_ctrl->ops.is_p2p_access_cap = devdrv_mini_v3_is_p2p_access_cap;
    pci_ctrl->ops.probe_wait = devdrv_probe_wait;
    pci_ctrl->ops.bind_irq = devdrv_bind_irq;
    pci_ctrl->ops.unbind_irq = devdrv_unbind_irq;
    pci_ctrl->ops.get_load_wait_mode = devdrv_mini_v3_get_load_wait_mode;
    pci_ctrl->ops.get_pf_max_msg_chan_cnt = devdrv_mini_v3_get_pf_msg_chan_cnt;
    pci_ctrl->ops.get_vf_max_msg_chan_cnt = devdrv_mini_v3_get_vf_msg_chan_cnt;
    pci_ctrl->ops.get_p2p_support_max_devnum = devdrv_mini_v3_get_p2p_support_max_devnum;
    pci_ctrl->ops.get_hccs_link_info = NULL;
    pci_ctrl->ops.is_mdev_vm_full_spec = NULL;
    pci_ctrl->ops.devdrv_deal_suspend_handshake = devdrv_mini_v3_deal_suspend_handshake;
    pci_ctrl->ops.is_all_dev_unified_addr = NULL;
    pci_ctrl->ops.flush_cache = NULL;
    pci_ctrl->ops.get_peh_link_info = NULL;
    pci_ctrl->ops.set_dev_shr_info = devdrv_mini_v3_set_dev_shr_info;
    pci_ctrl->ops.link_speed_slow_to_normal = NULL;
    pci_ctrl->ops.get_p2p_addr = NULL;
    pci_ctrl->ops.get_server_id = NULL;
    pci_ctrl->ops.get_max_server_num = NULL;
    pci_ctrl->ops.check_ep_suspend_status = NULL;
    pci_ctrl->ops.pre_cfg = NULL;
}

STATIC void devdrv_mini_v3_init_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i;
    for (i = 0; i < devdrv_msg_client_max; i++) {
        pci_ctrl->res.msg_chan_cnt[i] = (int)devdrv_msg_chan_cnt_mini_v3[i];
    }
}

int devdrv_mini_v3_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    resource_size_t offset;
    unsigned long size;

    pci_ctrl->mem_bar_id = PCI_BAR_MEM;
    pci_ctrl->mem_phy_base = 0;
    pci_ctrl->mem_phy_size = 0;

    offset = pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM) + DEVDRV_RESERVE_MEM_MSG_OFFSET;
    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        size = DEVDRV_RESERVE_MEM_MSG_SIZE;
    } else {
        size = DEVDRV_MDC_RESERVE_MEM_MSG_SIZE;
    }

    pci_ctrl->mem_base = ioremap(offset, size);
    if (pci_ctrl->mem_base == NULL) {
        devdrv_err("Ioremap mem_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->rsv_mem_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM);
    pci_ctrl->rsv_mem_phy_size = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_RSV_MEM);

    offset = pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO) + DEVDRV_IEP_SDI0_OFFSET + DEVDRV_IEP_SDI0_DB_OFFSET;
    size = DEVDRV_DB_IOMAP_SIZE;
    pci_ctrl->msi_base = ioremap(offset, size);
    if (pci_ctrl->msi_base == NULL) {
        devdrv_err("Ioremap msi_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->io_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO);
    pci_ctrl->io_phy_size = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_IO);

    pci_ctrl->io_base = ioremap(pci_ctrl->io_phy_base, pci_ctrl->io_phy_size);
    if (pci_ctrl->io_base == NULL) {
        devdrv_err("Ioremap io_base failed. (size=%llu)\n", pci_ctrl->io_phy_size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->shr_para = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET + DEVDRV_SHR_PARA_ADDR_OFFSET;

    pci_ctrl->res.phy_match_flag_addr = (u8 *)pci_ctrl->shr_para + PHY_MATCH_FLAG_OFFSET_IN_SHR_MEM;
    pci_ctrl->res.nvme_db_base = pci_ctrl->msi_base;
    pci_ctrl->res.nvme_pf_ctrl_base = pci_ctrl->io_base + DEVDRV_IEP_SDI0_OFFSET;
    pci_ctrl->res.load_sram_base = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET;

    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        devdrv_parse_res_gear();
    }
    devdrv_mini_v3_init_bar_addr_info(pci_ctrl);
    devdrv_mini_v3_init_intr_info(pci_ctrl);
    devdrv_mini_v3_init_dma_info(pci_ctrl);
    devdrv_mini_v3_init_load_file_info(pci_ctrl);
    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        devdrv_mini_v3_init_depend_module_info(pci_ctrl);
    }
    devdrv_mini_v3_ops_init(pci_ctrl);
    devdrv_mini_v3_init_msg_cnt(pci_ctrl);

    pci_ctrl->remote_dev_id = 0;
    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        pci_ctrl->os_load_flag = (u32)pci_ctrl->shr_para->load_flag;
    } else {
        pci_ctrl->os_load_flag = 0;
    }

    pci_ctrl->local_reserve_mem_base = NULL;
    return 0;
}

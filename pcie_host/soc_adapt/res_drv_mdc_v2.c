/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Description:used for ascend610lite
 * Author: huawei
 * Create: 2023-11-15
 */
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
#include <linux/panic_notifier.h>
#endif

#include "res_drv.h"
#include "devdrv_util.h"
#include "devdrv_ctrl.h"
#include "devdrv_pm.h"
#include "nvme_adapt.h"
#include "devdrv_pcie_link_info.h"
#include "res_drv_mdc_v2.h"

#define DEVDRV_MDC_V2_P2P_SUPPORT_MAX_DEVICE 0

/* msg chan cnt for modules */
#define DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX          1   /* non_trans:1 */
#define DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX         1   /* non_trans:1 */
#define DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX        1   /* non_trans:1 */
#define DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_TSDRV_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */
#define DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX           17  /* trans:16 non_trans:1 */
#define DEVDRV_QUEUE_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */

#define DEVDRV_MAX_MSG_PF_CHAN_CNT 80
#define DEVDRV_MAX_MSG_VF_CHAN_CNT 0

#define APB_TIMEOUT_INT_MASK            0x20004U
#define IOB_TX_INT_MASK1                0x810U
#define IOB_TX_INT_MASK2                0x814U
#define IOB_TX_INT_MASK3                0x818U
#define IOB_TX_INT_MASK4                0x81CU
#define IOB_TX_INT_MASK5                0x854U
#define IB_ODR_INT_MASK                 0x6BE4U
#define SDI_AXIM_INT_MASK               0x13084U
#define TL_INT_MASK0                    0x104574U
#define MAC_REG_MAC_INT_MSK             0x107058U
#define DL_INT_MASK                     0x10608CU
#define PCIE_CE_ENA_REG                 0x8008U
#define PCIE_UNF_ENA_REG                0x8010U
#define PCIE_UF_ENA_REG                 0x8018U
#define PCIE_CE_MASK_REG                0x800CU
#define PCIE_UNF_MASK_REG               0x8014U
#define PCIE_UF_MASK_REG                0x801CU

#ifndef DRV_UT
#define PCI_BAR_RSV_MEM 2
#define PCI_BAR_IO 0
#define PCI_BAR_MEM 4
#define PCI_BAR_UNSUPPORT 0xffffffff

#define DEVDRV_IO_LOAD_SRAM_OFFSET          0x20000
#define DEVDRV_IO_LOAD_SRAM_ADAPT_OFFSET    0x10
#define DEVDRV_IO_LOAD_SRAM_SIZE            0x80000     /* 512K */
#define DEVDRV_IO_TS_SRAM_OFFSET            0xA0000     /* ts sram */
#define DEVDRV_IO_TS_SRAM_SIZE              0x1000      /* 4K */
#define DEVDRV_IO_TS_DB_OFFSET              0x100000    /* ts doorbell */
#define DEVDRV_IO_TS_DB_SIZE                0x400000    /* 4M */
#define DEVDRV_IO_HWTS_OFFSET               0x500000    /* hwts aic */
#define DEVDRV_IO_HWTS_SIZE                 0x100000
#define DEVDRV_IO_HDR_OFFSET                0x600000
#define DEVDRV_IO_HDR_SIZE                  0x80000

#define DEVDRV_IEP_SDI0_OFFSET      0x0
#define DEVDRV_IEP_SDI0_SIZE        0x1000      /* 4K */
#define DEVDRV_IEP_DMA_OFFSET       0x4000      /* bar0 + 16K, need cfg remap register */
#define DEVDRV_IEP_DMA_SIZE         0x8000      /* 32K */

/* mem base */
#define DEVDRV_RESERVE_MEM_MSG_OFFSET   0x100000ULL
#define DEVDRV_RESERVE_MEM_MSG_SIZE     0x800000

#define DEVDRV_RESERVE_MEM_DB_BASE      0x0ULL     /*  doorbell base */
#define DEVDRV_DB_IOMAP_SIZE          0x20000 /* include msi table */

#ifdef CFG_SOC_MDC_V51_LITE_FPGA
#define DMA_CHAN_REMOTE_USED_NUM            4
#define DMA_CHAN_REMOTE_USED_START_INDEX    4
#else
#define DMA_CHAN_REMOTE_USED_NUM            6
#define DMA_CHAN_REMOTE_USED_START_INDEX    6
#endif

/* mdc v2 not support double pf */
#define DMA_CHAN_DOUBLE_PF_REMOTE_USED_NUM            10
#define DMA_CHAN_DOUBLE_PF_REMOTE_USED_START_INDEX    16U

#define DEVDRV_RM_DOORBELL_QUEUE_PER_PF 128

#define DEVDRV_MAX_DMA_CH_SQ_DEPTH 0x5000
#define DEVDRV_MAX_DMA_CH_CQ_DEPTH 0x5000
#define DEVDRV_DMA_CH_SQ_DESC_RSV  0x400

#define DEVDRV_BANDW_PACKSPEED 1000000 // (1000000 * dma_node)/s
#define DEVDRV_BANDWIDTH_REAL_RATIO 60
#define DEVDRV_PACKSPEED_REAL_RATIO 100

/* ************ interrupt defined for normal host ************* */
#define DEVDRV_MSI_X_MAX_VECTORS 256
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
#ifdef CFG_SOC_MDC_V51_LITE_FPGA
#define DEVDRV_DMA_MSI_X_VECTOR_NUM 8
#else
#define DEVDRV_DMA_MSI_X_VECTOR_NUM 12
#endif

/* irq used to tsdrv: mailbox, log, stars cq */
#define DEVDRV_TSDRV_MSI_X_VECTOR_BASE 0
#define DEVDRV_TSDRV_MSI_X_VECTOR_NUM 0

#define DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_BASE 0
#define DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_NUM 0

#define DEVDRV_CDQM_MSI_X_VECTOR_BASE 0
#define DEVDRV_CDQM_MSI_X_VECTOR_NUM 0

#define MDC_V2_BLOCKS_NUM              0

#define PCIE_NOTIFY_PRO 0

STATIC int devdrv_mdc_v2_init_bar0_res(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->io_base = ioremap(pci_ctrl->io_phy_base, pci_ctrl->io_phy_size);
    if (pci_ctrl->io_base == NULL) {
        devdrv_err("Ioremap io_base failed. (size=%llu)\n", pci_ctrl->io_phy_size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->shr_para = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET + DEVDRV_SHR_PARA_ADDR_OFFSET;
    pci_ctrl->res.phy_match_flag_addr = (u8 *)pci_ctrl->shr_para + PHY_MATCH_FLAG_OFFSET_IN_SHR_MEM;
    pci_ctrl->res.nvme_pf_ctrl_base = pci_ctrl->io_base + DEVDRV_IEP_SDI0_OFFSET;

    /* load sram is used in main process with offset 0x800 and 0x808, in total 16 bytes. Since mdc v2 pcie
     has only 1k sram, and don't use load sram, so load sram base offset -16 bytes */
    pci_ctrl->res.load_sram_base = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET - DEVDRV_IO_LOAD_SRAM_ADAPT_OFFSET;

    return 0;
}

STATIC int devdrv_mdc_v2_init_bar2_res(struct devdrv_pci_ctrl *pci_ctrl)
{
    resource_size_t offset;
    unsigned long size;

    offset = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_MSG_OFFSET;
    size = DEVDRV_RESERVE_MEM_MSG_SIZE;
    pci_ctrl->mem_base = NULL;

    offset = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_DB_BASE;
    size = DEVDRV_DB_IOMAP_SIZE;
    pci_ctrl->msi_base = ioremap(offset, size);
    if (pci_ctrl->msi_base == NULL) {
        devdrv_err("Ioremap msi_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }
    pci_ctrl->res.nvme_db_base = pci_ctrl->msi_base;

    return 0;
}

STATIC int devdrv_mdc_v2_init_bar_res(struct devdrv_pci_ctrl *pci_ctrl)
{
    int davinci_dev_num = devdrv_get_davinci_dev_num_by_pdev(pci_ctrl->pdev);
    phys_addr_t davinci_dev_rsv_mem_bar_len = 0;
    phys_addr_t davinci_dev_io_bar_len = 0;
    int ret;

    if (davinci_dev_num <= 0) {
        devdrv_err("davinci_dev_num is invalid.\n");
        return -ENODEV;
    }
    davinci_dev_rsv_mem_bar_len = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_RSV_MEM) / davinci_dev_num;
    davinci_dev_io_bar_len = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_IO) / davinci_dev_num;

    if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
        pci_ctrl->rsv_mem_bar_offset += davinci_dev_rsv_mem_bar_len;
        pci_ctrl->io_bar_offset += davinci_dev_io_bar_len;
    }

    pci_ctrl->mem_bar_id = PCI_BAR_UNSUPPORT;
    pci_ctrl->mem_phy_base = 0;
    pci_ctrl->mem_phy_size = 0;

    pci_ctrl->rsv_mem_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM);
    pci_ctrl->rsv_mem_phy_size = (u64)davinci_dev_rsv_mem_bar_len;
    pci_ctrl->rsv_mem_phy_base += pci_ctrl->rsv_mem_bar_offset;

    pci_ctrl->io_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO);
    pci_ctrl->io_phy_size = (u64)davinci_dev_io_bar_len;
    pci_ctrl->io_phy_base += pci_ctrl->io_bar_offset;

    ret = devdrv_mdc_v2_init_bar0_res(pci_ctrl);
    if (ret != 0) {
        return ret;
    }
    ret = devdrv_mdc_v2_init_bar2_res(pci_ctrl);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

STATIC void devdrv_mdc_v2_init_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_pci_ctrl *pci_ctrl_main = NULL;

    pci_ctrl->res.msg_db.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_DB_BASE;
    /* 1pf2p:p1 use nvme doorbell after p0 within same ATU:p0:0~127 nvme queue; p1:128~255 */
    if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
        pci_ctrl_main = devdrv_get_pdev_main_davinci_dev(pci_ctrl->pdev);
        if (pci_ctrl_main == NULL) {
            devdrv_err("Get pci_ctrl_main failed.\n");
            return;
        }
        pci_ctrl->res.msg_db.addr =
            pci_ctrl_main->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_DB_BASE +
            DEVDRV_RM_DOORBELL_QUEUE_PER_PF * DEVDRV_DB_QUEUE_TYPE * DEVDRV_MSG_CHAN_DB_OFFSET;
    }
    pci_ctrl->res.msg_db.size = DEVDRV_DB_IOMAP_SIZE;

    pci_ctrl->res.msg_mem.addr = 0; // after enable security communication, no longer reserve mem
    pci_ctrl->res.msg_mem.size = DEVDRV_RESERVE_MEM_MSG_SIZE; // used for local reserve mem assign

    /* not support */
    pci_ctrl->res.ts_db.size = 0;
    pci_ctrl->res.ts_sram.size = 0;
    pci_ctrl->res.ts_sq.size = 0;
    pci_ctrl->res.test.size = 0;
    pci_ctrl->res.load_sram.size = 0;
    pci_ctrl->res.hdr.size = 0;
    pci_ctrl->res.imu_log.size = 0;
    pci_ctrl->res.hwts.size = 0;
    pci_ctrl->res.ts_share_mem.size = 0;
    pci_ctrl->res.ts_notify.size = 0;
    pci_ctrl->res.ts_event.size = 0;
    pci_ctrl->res.vpc.size = 0;
    pci_ctrl->res.dvpp.size = 0;
    pci_ctrl->res.bbox.size = 0;
    pci_ctrl->res.stars_sqcq.size = 0;
    pci_ctrl->res.stars_sqcq_intr.size = 0;
    pci_ctrl->res.stars_topic_sched.size = 0;
    pci_ctrl->res.stars_topic_sched_rsv_mem.size = 0;
    pci_ctrl->res.stars_cdqm.size = 0;
    pci_ctrl->res.stars_intr.size = 0;
    pci_ctrl->res.reg_sram.size = 0;
    pci_ctrl->res.kdump.addr = 0;
    pci_ctrl->res.kdump.size = 0;
    pci_ctrl->res.vmcore.addr = 0;
    pci_ctrl->res.vmcore.size = 0;

    /* pcie reserve bar space for other modules */
    pci_ctrl->res.tsdrv_resv.addr = 0;
    pci_ctrl->res.tsdrv_resv.size = 0;
    pci_ctrl->res.devmng_resv.addr = 0;
    pci_ctrl->res.devmng_resv.size = 0;
    pci_ctrl->res.vf_bandwidth.addr = 0;
    pci_ctrl->res.vf_bandwidth.size = 0;
    pci_ctrl->res.l3d_sram.size = 0;
    pci_ctrl->res.devmng_info_mem.size = 0;
    pci_ctrl->res.bbox_ddr_dump.size = 0;
    pci_ctrl->res.ts_log.size = 0;
    pci_ctrl->res.chip_dfx.size = 0;
}

STATIC void devdrv_mdc_v2_init_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
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

STATIC void devdrv_mdc_v2_init_dma_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    int total_func_num = pci_ctrl->shr_para->total_func_num;
    int davinci_dev_num = devdrv_get_davinci_dev_num_by_pdev(pci_ctrl->pdev);
    struct devdrv_pci_ctrl *pci_ctrl_main = NULL;
    u32 i;

    pci_ctrl->res.dma_res.dma_addr = pci_ctrl->io_base + DEVDRV_IEP_DMA_OFFSET;
    pci_ctrl->res.dma_res.dma_chan_addr = pci_ctrl->res.dma_res.dma_addr;
    if (total_func_num > 1) {
        pci_ctrl->res.dma_res.dma_chan_num = (u32)(DMA_CHAN_DOUBLE_PF_REMOTE_USED_NUM / total_func_num);
        pci_ctrl->res.dma_res.dma_chan_start_id = DMA_CHAN_DOUBLE_PF_REMOTE_USED_START_INDEX +
            pci_ctrl->res.dma_res.dma_chan_num * pci_ctrl->func_id;
    } else {
        pci_ctrl->res.dma_res.dma_chan_start_id = DMA_CHAN_REMOTE_USED_START_INDEX;
        pci_ctrl->res.dma_res.dma_chan_num = DMA_CHAN_REMOTE_USED_NUM;
    }

    /* 1PF2P */
    if (davinci_dev_num > 1) {
        pci_ctrl->shr_para->total_func_num = 1;

        pci_ctrl_main = devdrv_get_pdev_main_davinci_dev(pci_ctrl->pdev);
        if (pci_ctrl_main == NULL) {
            devdrv_err("pci_ctrl_main is invalid.\n");
            return;
        }
        pci_ctrl->res.dma_res.dma_addr = pci_ctrl_main->io_base + DEVDRV_IEP_DMA_OFFSET;
        pci_ctrl->res.dma_res.dma_chan_addr = pci_ctrl->res.dma_res.dma_addr;
        pci_ctrl->res.dma_res.dma_chan_num = (u32)(DMA_CHAN_DOUBLE_PF_REMOTE_USED_NUM / davinci_dev_num);
        pci_ctrl->res.dma_res.dma_chan_start_id = DMA_CHAN_DOUBLE_PF_REMOTE_USED_START_INDEX +
            pci_ctrl->res.dma_res.dma_chan_num * pci_ctrl->dev_id_in_pdev;
    }
    pci_ctrl->res.dma_res.chan_start_id = pci_ctrl->res.dma_res.dma_chan_start_id;
    for (i = 0; i < pci_ctrl->res.dma_res.dma_chan_num; i++) {
        pci_ctrl->res.dma_res.use_chan[i] = i;
    }
    pci_ctrl->res.dma_res.pf_num = pci_ctrl->func_id;
    pci_ctrl->res.dma_res.vf_id = 0;
    pci_ctrl->res.dma_res.sq_depth = DEVDRV_MAX_DMA_CH_SQ_DEPTH;
    pci_ctrl->res.dma_res.sq_rsv_num = DEVDRV_DMA_CH_SQ_DESC_RSV;
    pci_ctrl->res.dma_res.cq_depth = DEVDRV_MAX_DMA_CH_CQ_DEPTH;
}

STATIC void devdrv_mdc_v2_init_load_file_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.load_file.load_file_num = MDC_V2_BLOCKS_NUM;
}

STATIC void devdrv_mdc_v2_init_depend_module_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.depend_info.module_num = 0;
    pci_ctrl->res.depend_info.module_list = NULL;
}

STATIC void devdrv_mdc_v2_init_link_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    int davinci_dev_num = devdrv_get_davinci_dev_num_by_pdev(pci_ctrl->pdev);
    if (davinci_dev_num <= 0) {
        devdrv_err("davinci_dev_num is invalid.\n");
        return;
    }

    pci_ctrl->res.link_info.bandwidth = devdrv_get_bandwidth_info(pci_ctrl->pdev) / davinci_dev_num;
    pci_ctrl->res.link_info.packspeed = (u64)(DEVDRV_BANDW_PACKSPEED / davinci_dev_num);
    pci_ctrl->res.link_info.bandwidth_ratio = DEVDRV_BANDWIDTH_REAL_RATIO;
    pci_ctrl->res.link_info.packspeed_ratio = DEVDRV_PACKSPEED_REAL_RATIO;
}
#endif

STATIC unsigned int devdrv_msg_chan_cnt_mdc_v2[devdrv_msg_client_max] = {
    DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX,    /* used for pcivnic */
    DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX,       /* used for test */
    DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX,      /* used for svm */
    DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX,     /* used for common */
    DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX,    /* used for device manager */
    DEVDRV_TSDRV_MSG_CHAN_CNT_MAX,          /* used for tsdrv */
    DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX,        /* used for hdc */
    DEVDRV_QUEUE_MSG_CHAN_CNT_MAX,          /* used for queue */
};

#define MDC_V2_PCIE_DEVID 1
STATIC int devdrv_mdc_v2_alloc_devid(struct devdrv_ctrl *ctrl_this)
{
    return MDC_V2_PCIE_DEVID;
}

STATIC enum devdrv_load_wait_mode devdrv_mdc_v2_get_load_wait_mode(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->ops.pre_cfg != NULL) {
        pci_ctrl->ops.pre_cfg(pci_ctrl);
    }

    return DEVDRV_LOAD_WAIT_INTERVAL;
}

STATIC int devdrv_mdc_v2_get_pf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_PF_CHAN_CNT;
}

STATIC int devdrv_mdc_v2_get_vf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_VF_CHAN_CNT;
}

STATIC int devdrv_mdc_v2_get_p2p_support_max_devnum(void)
{
    return DEVDRV_MDC_V2_P2P_SUPPORT_MAX_DEVICE;
}

STATIC int devdrv_mdc_v2_is_p2p_access_cap(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_pci_ctrl *peer_pci_ctrl)
{
    devdrv_info("This chip type not support p2p. (dev_id=%u; chip_type=%d)\n",
        pci_ctrl->dev_id, pci_ctrl->shr_para->chip_type);
    return DEVDRV_P2P_ACCESS_DISABLE;
}

STATIC void devdrv_mdc_v2_mask_suspend_fault(void)
{
    void __iomem *apb_base = NULL;

    apb_base = ioremap(PCIE_BASE_ADDR, PCIE_REG_SIZE);
    if (apb_base == NULL) {
        devdrv_err("ioremap fail, apb_base is NULL.\n");
        return;
    }

    /* ras */
    writel(0xFFFFFFFFU, apb_base + APB_TIMEOUT_INT_MASK);
    writel(0xFFFFFFFFU, apb_base + IOB_TX_INT_MASK1);
    writel(0xFFFFFFFFU, apb_base + IOB_TX_INT_MASK2);
    writel(0xFFFFFFFFU, apb_base + IOB_TX_INT_MASK3);
    writel(0xFFFFFFFFU, apb_base + IOB_TX_INT_MASK5);
    writel(0xFFFFFFFFU, apb_base + IB_ODR_INT_MASK);
    writel(0xFFFFFFFFU, apb_base + SDI_AXIM_INT_MASK);
    writel(0xFFFFFFFFU, apb_base + TL_INT_MASK0);
    writel(0xFFFFFFFFU, apb_base + MAC_REG_MAC_INT_MSK);
    writel(0xFFFFFFFFU, apb_base + DL_INT_MASK);

    /* aer */
    writel(0x0U, apb_base + PCIE_CE_ENA_REG);
    writel(0x0U, apb_base + PCIE_UNF_ENA_REG);
    writel(0x0U, apb_base + PCIE_UF_ENA_REG);
    writel(0xFFFFFU, apb_base + PCIE_CE_MASK_REG);
    writel(0xFFFFFU, apb_base + PCIE_UNF_MASK_REG);
    writel(0xFFFFFU, apb_base + PCIE_UF_MASK_REG);

    iounmap(apb_base);
    apb_base = NULL;
    return;
}

STATIC int devdrv_mdc_v2_deal_suspend_handshake(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    ret = devdrv_dev_get_suspend_status(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Get suspend status failed. (ret=%d; dev_id=%u)\n", ret, pci_ctrl->dev_id);
        return ret;
    }
    /* mask ras */
    devdrv_mdc_v2_mask_suspend_fault();

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

STATIC int devdrv_check_ep_suspend_status(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_ep_suspend_status reply = {0};
    u32 dev_id = pci_ctrl->dev_id;
    int ret;
    struct devdrv_ep_suspend_cmd cmd_data;
    int count = 1;

    ret = devdrv_hdc_suspend_precheck(count);
    if (ret != 0) {
        devdrv_warn_limit("hdc suspend check result. (ret=%d, dev_id=%u)\n", ret, dev_id);
        return -EBUSY;
    }

    cmd_data.hand_shake_flag = DEVDRV_SENTRY_CHECK_EP_SUSPEND;
    ret = devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_GET_EP_SUSPEND_STATUS,
        &cmd_data, sizeof(cmd_data), &reply, sizeof(reply));
    if (ret != 0) {
        devdrv_err("send admin msg fail for check ep suspend status. (ret=%d, dev_id=%u)\n", ret, dev_id);
        return ret;
    }
    if (reply.status != DEVDRV_EP_SUSPEND_READY) {
        devdrv_warn_limit("ep suspend not ready. (status=%u, dev_id=%u)\n", reply.status, dev_id);
        return -EBUSY;
    }
    return 0;
}

STATIC void devdrv_mdc_v2_set_dev_shr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para->host_dev_id = (int)pci_ctrl->dev_id;
    pci_ctrl->shr_para->host_mem_bar_base = (u64)pci_ctrl->mem_phy_base;
    pci_ctrl->shr_para->host_io_bar_base = (u64)pci_ctrl->io_phy_base;
}

#ifndef DRV_UT
STATIC int devdrv_local_reboot_handle(struct notifier_block *self, unsigned long event, void *data)
{
    int ret;

    ret = devdrv_set_err_out_gpio();
    if (ret != 0) {
        devdrv_err("set out gpio fail. (ret=%d)\n", ret);
        return ret;
    }

    devdrv_info("local reboot, notify peer device.\n");
    return 0;
}

STATIC int devdrv_local_panic_handle(struct notifier_block *self, unsigned long event, void *data)
{
    int ret;

    ret = devdrv_set_err_out_gpio();
    if (ret != 0) {
        devdrv_err("set out gpio fail. (ret=%d)\n", ret);
        return ret;
    }

    devdrv_info("local panic, notify peer device\n");
    return 0;
}

STATIC struct notifier_block pcie_reboot_notifier = {
    .notifier_call = devdrv_local_reboot_handle,
    .priority = PCIE_NOTIFY_PRO,
};

STATIC struct notifier_block pcie_panic_notifier = {
    .notifier_call = devdrv_local_panic_handle,
    .priority = PCIE_NOTIFY_PRO,
};

STATIC irqreturn_t devdrv_mdc_v2_peer_fault_proc(int irq, void *data)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)data;

    devdrv_info("peer fault happen, stop local pcie business\n");
    if ((pci_ctrl == NULL) || (pci_ctrl->dma_dev == NULL)) {
        devdrv_err("resource had been released\n");
        return IRQ_HANDLED;
    }
    devdrv_peer_fault_notifier(DEVDRV_PEER_STATUS_RESET);
    devdrv_set_peer_pcie_status(DEVDRV_PEER_STATUS_RESET);
    devdrv_set_pcie_channel_status(DEVDRV_PCIE_COMMON_CHANNEL_DEAD);
    devdrv_set_device_status(pci_ctrl, DEVDRV_DEVICE_DEAD);
    devdrv_set_dma_status(pci_ctrl->dma_dev, DEVDRV_DMA_DEAD);
    tasklet_schedule(&pci_ctrl->dma_dev->single_fault_task);
    return IRQ_HANDLED;
}

STATIC int devdrv_mdc_v2_register_peer_fault_handle(struct devdrv_pci_ctrl *pci_ctrl)
{
    const char *gpio5_01_name = "pcie-err-in-soca";
    int ret;

    if (!gpio_is_valid(PCIE_ERR_IN_GPIO5_01)) {
        devdrv_err("invalid gpio. (gpio5_01=%d)\n", PCIE_ERR_IN_GPIO5_01);
        return -EINVAL;
    }

    ret = gpio_request_one(PCIE_ERR_IN_GPIO5_01, GPIOF_IN, gpio5_01_name);
    if (ret != 0) {
        devdrv_err("gpio request fail. (ret=%d)\n", ret);
        return -EINVAL;
    }

    ret = devm_request_irq(&pci_ctrl->pdev->dev, gpio_to_irq(PCIE_ERR_IN_GPIO5_01),
            devdrv_mdc_v2_peer_fault_proc, IRQF_TRIGGER_RISING, gpio5_01_name, pci_ctrl);
    if (ret != 0) {
        devdrv_err("request irq failed. (ret=%d, gpio_irqname=%s)\n", ret, gpio5_01_name);
        gpio_free(PCIE_ERR_IN_GPIO5_01);
        return ret;
    }

    return 0;
}

STATIC int devdrv_mdc_v2_unregister_peer_fault_handle(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (irq_has_action(gpio_to_irq(PCIE_ERR_IN_GPIO5_01)) != 0) {
        devm_free_irq(&pci_ctrl->pdev->dev, gpio_to_irq(PCIE_ERR_IN_GPIO5_01), pci_ctrl);
    }

    if (gpio_is_valid(PCIE_ERR_IN_GPIO5_01)) {
        gpio_free(PCIE_ERR_IN_GPIO5_01);
    }

    return 0;
}

STATIC int devdrv_mdc_v2_register_local_fault_notifier(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    ret = register_reboot_notifier(&pcie_reboot_notifier);
    if (ret != 0) {
        devdrv_err("register_reboot_notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = atomic_notifier_chain_register(&panic_notifier_list, &pcie_panic_notifier);
    if (ret != 0) {
        devdrv_err("atomic_notifier_chain_register failed. (ret=%d)\n", ret);
        goto unregister_reboot;
    }

    return 0;
unregister_reboot:
    (void)unregister_reboot_notifier(&pcie_reboot_notifier);

    return ret;
}

STATIC int devdrv_mdc_v2_unregister_local_fault_notifier(struct devdrv_pci_ctrl *pci_ctrl)
{
    (void)atomic_notifier_chain_unregister(&panic_notifier_list, &pcie_panic_notifier);
    (void)unregister_reboot_notifier(&pcie_reboot_notifier);

    return 0;
}
#endif

STATIC int devdrv_mdc_v2_single_fault_init(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifndef DRV_UT
    int ret;

    ret = devdrv_mdc_v2_register_peer_fault_handle(pci_ctrl);
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_mdc_v2_register_local_fault_notifier(pci_ctrl);
    if (ret != 0) {
        goto unregister_peer_fault;
    }

    return 0;
unregister_peer_fault:
    devdrv_mdc_v2_unregister_peer_fault_handle(pci_ctrl);

    return ret;
#else
    return 0;
#endif
}

STATIC int devdrv_mdc_v2_single_fault_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifndef DRV_UT
    (void)devdrv_mdc_v2_unregister_local_fault_notifier(pci_ctrl);
    (void)devdrv_mdc_v2_unregister_peer_fault_handle(pci_ctrl);
#endif
    return 0;
}

STATIC void devdrv_mdc_v2_ops_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->ops.shr_para_rebuild = NULL;
    pci_ctrl->ops.alloc_devid = devdrv_mdc_v2_alloc_devid;
    pci_ctrl->ops.is_p2p_access_cap = devdrv_mdc_v2_is_p2p_access_cap;
    pci_ctrl->ops.probe_wait = devdrv_probe_wait;
    pci_ctrl->ops.bind_irq = devdrv_bind_irq;
    pci_ctrl->ops.unbind_irq = devdrv_unbind_irq;
    pci_ctrl->ops.get_load_wait_mode = devdrv_mdc_v2_get_load_wait_mode;
    pci_ctrl->ops.get_pf_max_msg_chan_cnt = devdrv_mdc_v2_get_pf_msg_chan_cnt;
    pci_ctrl->ops.get_vf_max_msg_chan_cnt = devdrv_mdc_v2_get_vf_msg_chan_cnt;
    pci_ctrl->ops.get_p2p_support_max_devnum = devdrv_mdc_v2_get_p2p_support_max_devnum;
    pci_ctrl->ops.get_hccs_link_info = NULL;
    pci_ctrl->ops.is_mdev_vm_full_spec = NULL;
    pci_ctrl->ops.devdrv_deal_suspend_handshake = devdrv_mdc_v2_deal_suspend_handshake;
    pci_ctrl->ops.is_all_dev_unified_addr = NULL;
    pci_ctrl->ops.flush_cache = NULL;
    pci_ctrl->ops.get_peh_link_info = NULL;
    pci_ctrl->ops.set_dev_shr_info = devdrv_mdc_v2_set_dev_shr_info;
    pci_ctrl->ops.link_speed_slow_to_normal = NULL;
    pci_ctrl->ops.get_p2p_addr = NULL;
    pci_ctrl->ops.get_server_id = NULL;
    pci_ctrl->ops.get_max_server_num = NULL;
    pci_ctrl->ops.check_ep_suspend_status = devdrv_check_ep_suspend_status;
    pci_ctrl->ops.single_fault_init = devdrv_mdc_v2_single_fault_init;
    pci_ctrl->ops.single_fault_uninit = devdrv_mdc_v2_single_fault_uninit;
    pci_ctrl->ops.pre_cfg = NULL;
}

STATIC void devdrv_mdc_v2_init_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i;

    for (i = 0; i < devdrv_msg_client_max; i++) {
        pci_ctrl->res.msg_chan_cnt[i] = (int)devdrv_msg_chan_cnt_mdc_v2[i];
    }
}

/*
 * BAR      SIZE
 * BAR0     8M
 * BAR2     128MB
 * BAR4     2GB
 */
int devdrv_mdc_v2_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifndef DRV_UT
    int davinci_dev_num = devdrv_get_davinci_dev_num_by_pdev(pci_ctrl->pdev);
    struct devdrv_pci_ctrl *pci_ctrl_main = NULL;
    u64 flag_r = 0;
    int count = 0;
    int ret;

    ret = devdrv_mdc_v2_init_bar_res(pci_ctrl);
    if (ret != 0) {
        devdrv_err("mdc v2 bar init fail.(ret=%d)\n", ret);
        return ret;
    }
    /* 1pf2p:p1 use nvme doorbell after p0 within same ATU:p0:0~127 nvme queue; p1:128~255 */
    if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
        pci_ctrl_main = devdrv_get_pdev_main_davinci_dev(pci_ctrl->pdev);
        if (pci_ctrl_main == NULL) {
            devdrv_err("pci_ctrl_main is invalid.\n");
            devdrv_res_uninit(pci_ctrl);
            return -ENODEV;
        }
        pci_ctrl->res.nvme_db_base =
            pci_ctrl_main->res.nvme_db_base +
            DEVDRV_RM_DOORBELL_QUEUE_PER_PF * DEVDRV_DB_QUEUE_TYPE * DEVDRV_MSG_CHAN_DB_OFFSET;
    } else if (davinci_dev_num > 1) {
        /* P0 wait for hccs link ready, then P1 probe can acess P1 resource */
        while (1) {
            flag_r = readq(pci_ctrl->res.load_sram_base);
            if ((flag_r == DEVDRV_NORMAL_BOOT_MODE) || (flag_r == DEVDRV_SLOW_BOOT_MODE)) {
                break;
            }
            count++;
            if (count >= DEVDRV_WAIT_BOOT_MODE_MAX) {
                devdrv_err("Wait boot mode from bios time out. (flag_r=0x%llx)\n", flag_r);
                devdrv_res_uninit(pci_ctrl);
                return -ENODEV;
            }
            msleep(DEVDRV_WAIT_BOOT_MODE_SLEEP_TIME);
        }
    }

    pci_ctrl->local_reserve_mem_base = ioremap_cache(DEVDRV_RESERVE_MEM_MSG_OFFSET, DEVDRV_RESERVE_MEM_MSG_SIZE);
    if (pci_ctrl->local_reserve_mem_base == NULL) {
        devdrv_err("ioremap local_reserve_mem_base failed.\n");
        return -ENOMEM;
    }
    pci_ctrl->res.rsv_phy_addr = DEVDRV_RESERVE_MEM_MSG_OFFSET;
    devdrv_mdc_v2_init_bar_addr_info(pci_ctrl);
    devdrv_mdc_v2_init_intr_info(pci_ctrl);
    devdrv_mdc_v2_init_dma_info(pci_ctrl);
    devdrv_mdc_v2_init_msg_cnt(pci_ctrl);
    devdrv_mdc_v2_init_link_info(pci_ctrl);

    pci_ctrl->remote_dev_id = pci_ctrl->dev_id_in_pdev;
    pci_ctrl->os_load_flag = 0;
    pci_ctrl->shr_para->chip_id = (int)pci_ctrl->remote_dev_id;

    devdrv_mdc_v2_init_load_file_info(pci_ctrl);
    devdrv_mdc_v2_init_depend_module_info(pci_ctrl);
    devdrv_mdc_v2_ops_init(pci_ctrl);
#endif

    return 0;
}

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

#include "res_drv.h"
#include "devdrv_util.h"
#include "devdrv_ctrl.h"
#include "nvme_adapt.h"
#include "res_drv_mini_v2.h"

#define DEVDRV_DRIVER_VERSION 0xa5a50001 /* 0xa5a5: magic; 0x0001: driver version code */
#define DEVDRV_MINI_V2_P2P_SUPPORT_MAX_DEVICE 40

/* msg chan cnt for modules */
#define DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX          1   /* non_trans:1 */
#define DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX         1   /* non_trans:1 */
#define DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX        1   /* non_trans:1 */
#define DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_TSDRV_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */
#define DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX           65  /* trans:64 non_trans:1 */
#define DEVDRV_DEV_HDC_MSG_CHAN_CNT_GEAR_4        17  /* trans:16 non_trans:1 */
#define DEVDRV_QUEUE_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */

#define DEVDRV_MAX_MSG_PF_CHAN_CNT 80
#define DEVDRV_MAX_MSG_VF_CHAN_CNT 0

#define PCI_BAR_RSV_MEM 2
#define PCI_BAR_IO 0
#define PCI_BAR_MEM 4

#define DEVDRV_IO_LOAD_SRAM_OFFSET  0x20000
#define DEVDRV_IO_LOAD_SRAM_SIZE    0x80000     /* 512K */
#define DEVDRV_IO_TS_SRAM_OFFSET    0xA0000     /* ts sram */
#define DEVDRV_IO_TS_SRAM_SIZE      0x1000      /* 4K */
#define DEVDRV_IO_TS_DB_OFFSET      0x100000    /* ts doorbell */
#define DEVDRV_IO_TS_DB_SIZE        0x400000    /* 4M */
#define DEVDRV_IO_HWTS_OFFSET       0x500000    /* hwts aic */
#define DEVDRV_IO_HWTS_SIZE         0x100000
#define DEVDRV_IO_HDR_OFFSET        0x600000
#define DEVDRV_IO_HDR_SIZE          0x80000

#ifndef DRV_UT

#define DEVDRV_IEP_SDI0_OFFSET      0x0
#define DEVDRV_IEP_SDI0_SIZE        0x1000      /* 4K */
#define DEVDRV_IEP_DMA_OFFSET       0x4000      /* bar0 + 16K, need cfg remap register */
#define DEVDRV_IEP_DMA_SIZE         0x8000      /* 32K */

/* mem base */
#define DEVDRV_RESERVE_MEM_TS_SQ_OFFSET 0x1000000
#define DEVDRV_RESERVE_MEM_TS_SQ_SIZE   0x4000000

#define DEVDRV_RESERVE_MEM_TEST_OFFSET  0x5000000
#define DEVDRV_RESERVE_MEM_TEST_SIZE    0x200000

#define DEVDRV_RESERVE_MEM_MSG_OFFSET   0x100000
#define DEVDRV_RESERVE_MEM_MSG_SIZE     0x800000

#define DEVDRV_RESERVE_IMU_LOG_OFFSET   0x5200000
#define DEVDRV_RESERVE_IMU_LOG_SIZE     0x800000

#define DEVDRV_RESERVE_MEM_DB_BASE      0x0     /*  doorbell base */
#define DEVDRV_DB_IOMAP_SIZE          0x20000 /* include msi table */

#define DEVDRV_RESERVE_TS_SHARE_MEM_OFFSET 0x6000000
#define DEVDRV_RESERVE_TS_SHARE_MEM_SIZE 0x2000000

#define DMA_CHAN_REMOTE_USED_NUM            6
#define DMA_CHAN_REMOTE_USED_START_INDEX    6

#define DMA_CHAN_DOUBLE_PF_REMOTE_USED_NUM            10
#define DMA_CHAN_DOUBLE_PF_REMOTE_USED_START_INDEX    16

#define DEVDRV_RM_DOORBELL_QUEUE_PER_PF 128

#define DEVDRV_MAX_DMA_CH_SQ_DEPTH 0x10000
#define DEVDRV_MAX_DMA_CH_CQ_DEPTH 0x10000
#define DEVDRV_DMA_CH_SQ_DESC_RSV  0x400

#define DEVDRV_BANDW_PACKSPEED 1000000 // (1000000 * dma_node)/s
#define DEVDRV_BANDWIDTH_REAL_RATIO 60
#define DEVDRV_PACKSPEED_REAL_RATIO 100

#define MINI_V2_BLOCKS_NUM              11

STATIC struct devdrv_load_file_cfg mini_v2_file[MINI_V2_BLOCKS_NUM] = {
    {
        .file_name = "/driver/device/Ascend310P_ddr.image",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P_lowpwr.image",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P_hsm.image",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P.fd",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P_dt.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P.image",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P.cpio.gz",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P_tee.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/Ascend310P.crl",
        .file_type = DEVDRV_NON_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/CMS/user.crl",
        .file_type = DEVDRV_NON_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
    {
        .file_name = "/CMS/user.xer",
        .file_type = DEVDRV_NON_CRITICAL_FILE,
        .fail_mode = DEVDRV_NOTICE_BIOS,
    },
};

#define MINI_V2_MODULE_NUM  9
STATIC struct devdrv_depend_module mini_v2_module[MINI_V2_MODULE_NUM] = {
    {
        .module_name = "drv_soft_fault",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ascend_xsmem",
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
        .module_name = "ts_agent",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "drv_virtmng_host",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "drv_tsdrv_platform_host",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "drv_devmm_host",
        .status = DEVDRV_MODULE_UNPROBED,
    },
};

STATIC struct devdrv_irq_gear_info mini_v2_normal_host_irq_info[DEVDRV_RES_GEAR_VAL_CNT] = {
    {
        /* irq_gear 0 */
        .flag = DEVDRV_IRQ_GEAR_INFO_INVALID,
    },
    {
        /* irq_gear 1 */
        .flag = DEVDRV_IRQ_GEAR_INFO_INVALID,
    },
    {
        /* irq_gear 2 */
        .flag = DEVDRV_IRQ_GEAR_INFO_INVALID,
    },
    {
        /* irq_gear 3 */
        .flag = DEVDRV_IRQ_GEAR_INFO_INVALID,
    },
    {
        /* irq_gear 4 */
        .flag = DEVDRV_IRQ_GEAR_INFO_VALID,
        .intr = {
            .min_vector = 128,
            .max_vector = 128,
            .device_os_load_irq = 0,
            .msg_irq_base = 0,
            .msg_irq_num = 58,
            .dma_irq_base = 58,
            .dma_irq_num = 22,
            .tsdrv_irq_base = 80,
            .tsdrv_irq_num = 48,
            .topic_sched_irq_num = 0,
            .cdqm_irq_num = 0,
            .msg_irq_vector2_base = 128,
            .msg_irq_vector2_num = 0,
            .tsdrv_irq_vector2_base = 0,
            .tsdrv_irq_vector2_num = 0,
        },
    },
    {
        /* irq_gear 5 */
        .flag = DEVDRV_IRQ_GEAR_INFO_INVALID,
    },
    {
        /* irq_gear 6 */
        .flag = DEVDRV_IRQ_GEAR_INFO_VALID,
        .intr = {
            .min_vector = 128,
            .max_vector = 256,
            .device_os_load_irq = 0,
            .msg_irq_base = 0,
            .msg_irq_num = 58,
            .dma_irq_base = 58,
            .dma_irq_num = 22,
            .tsdrv_irq_base = 80,
            .tsdrv_irq_num = 48,
            .topic_sched_irq_num = 0,
            .cdqm_irq_num = 0,
            .msg_irq_vector2_base = 128,
            .msg_irq_vector2_num = 0,
            .tsdrv_irq_vector2_base = 0,
            .tsdrv_irq_vector2_num = 0,
        },
    },
};

STATIC void devdrv_mini_v2_init_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl)
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

    pci_ctrl->res.msg_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_MSG_OFFSET;
    pci_ctrl->res.msg_mem.size = DEVDRV_RESERVE_MEM_MSG_SIZE;

    pci_ctrl->res.ts_db.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_DB_OFFSET;
    pci_ctrl->res.ts_db.size = DEVDRV_IO_TS_DB_SIZE;

    pci_ctrl->res.ts_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_SRAM_OFFSET;
    pci_ctrl->res.ts_sram.size = DEVDRV_IO_TS_SRAM_SIZE;

    pci_ctrl->res.ts_sq.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_TS_SQ_OFFSET;
    pci_ctrl->res.ts_sq.size = DEVDRV_RESERVE_MEM_TS_SQ_SIZE;

    pci_ctrl->res.test.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_TEST_OFFSET;
    pci_ctrl->res.test.size = DEVDRV_RESERVE_MEM_TEST_SIZE;

    pci_ctrl->res.load_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_LOAD_SRAM_OFFSET;
    pci_ctrl->res.load_sram.size = DEVDRV_IO_LOAD_SRAM_SIZE;

    pci_ctrl->res.hdr.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HDR_OFFSET;
    pci_ctrl->res.hdr.size = DEVDRV_IO_HDR_SIZE;

    pci_ctrl->res.imu_log.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_IMU_LOG_OFFSET;
    pci_ctrl->res.imu_log.size = DEVDRV_RESERVE_IMU_LOG_SIZE;

    pci_ctrl->res.hwts.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HWTS_OFFSET;
    pci_ctrl->res.hwts.size = DEVDRV_IO_HWTS_SIZE;

    pci_ctrl->res.ts_share_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_TS_SHARE_MEM_OFFSET;
    pci_ctrl->res.ts_share_mem.size = DEVDRV_RESERVE_TS_SHARE_MEM_SIZE;
    /* not support */
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
    pci_ctrl->res.l3d_sram.size = 0;
    pci_ctrl->res.kdump.addr = 0;
    pci_ctrl->res.kdump.size = 0;
    pci_ctrl->res.vmcore.addr = 0;
    pci_ctrl->res.vmcore.size = 0;
    pci_ctrl->res.bbox_ddr_dump.size = 0;
    pci_ctrl->res.ts_log.size = 0;
    pci_ctrl->res.chip_dfx.size = 0;
    /* pcie reserve bar space for other modules */
    pci_ctrl->res.tsdrv_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_TSDRV_RESV_OFFSET;
    pci_ctrl->res.tsdrv_resv.size = DEVDRV_RESERVE_TSDRV_RESV_SIZE;
    pci_ctrl->res.devmng_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_DEVMNG_RESV_OFFSET;
    pci_ctrl->res.devmng_resv.size = DEVDRV_RESERVE_DEVMNG_RESV_SIZE;
    pci_ctrl->res.vf_bandwidth.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_SRAM_OFFSET + DEVDRV_VF_BANDWIDTH_OFFSET;
    pci_ctrl->res.vf_bandwidth.size = DEVDRV_VF_BANDWIDTH_SIZE;
    pci_ctrl->res.hbm_ecc_mem.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_HBM_ECC_OFFSET;
    pci_ctrl->res.hbm_ecc_mem.size = DEVDRV_RESERVE_HBM_ECC_SIZE;

    pci_ctrl->res.devmng_info_mem.size = 0;
}

STATIC void devdrv_mini_v2_init_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_intr_info *intr = &pci_ctrl->res.intr;
    u32 irq_res_gear;

    irq_res_gear = devdrv_get_irq_res_gear();
    if ((irq_res_gear <= DEVDRV_RES_GEAR_MAX_VAL) &&
        (mini_v2_normal_host_irq_info[irq_res_gear].flag == DEVDRV_IRQ_GEAR_INFO_VALID)) {
        *intr = mini_v2_normal_host_irq_info[irq_res_gear].intr;
    } else {
        devdrv_info("Irq gear is invalid, use default gear. (irq_res_gear=%u)\n", irq_res_gear);
        *intr = mini_v2_normal_host_irq_info[DEVDRV_RES_GEAR_MAX_VAL].intr;
    }

    intr->msg_irq_vector2_num = intr->max_vector - intr->msg_irq_vector2_base;
}

STATIC void devdrv_mini_v2_init_dma_info(struct devdrv_pci_ctrl *pci_ctrl)
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

STATIC void devdrv_mini_v2_init_load_file_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.load_file.load_file_num = MINI_V2_BLOCKS_NUM;
    pci_ctrl->res.load_file.load_file_cfg = mini_v2_file;
}

STATIC void devdrv_mini_v2_init_depend_module_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.depend_info.module_num = MINI_V2_MODULE_NUM;
    pci_ctrl->res.depend_info.module_list = mini_v2_module;
}

STATIC void devdrv_mini_v2_init_link_info(struct devdrv_pci_ctrl *pci_ctrl)
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

STATIC unsigned int devdrv_msg_chan_cnt_mini_v2[devdrv_msg_client_max] = {
    DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX,    /* used for pcivnic */
    DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX,       /* used for test */
    DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX,      /* used for svm */
    DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX,     /* used for common */
    DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX,    /* used for device manager */
    DEVDRV_TSDRV_MSG_CHAN_CNT_MAX,          /* used for tsdrv */
    DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX,        /* used for hdc */
    DEVDRV_QUEUE_MSG_CHAN_CNT_MAX,          /* used for queue */
};

STATIC void devdrv_shr_para_rebuild(struct devdrv_pci_ctrl *pci_ctrl)
{
    /* miniv2 should rebuild shr_para addr, because 2P card can't access the slave's l3 sram */
    pci_ctrl->shr_para = pci_ctrl->mem_base + DEVDRV_SHR_PARA_ADDR_OFFSET;
}

STATIC enum devdrv_load_wait_mode devdrv_mini_v2_get_load_wait_mode(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->ops.pre_cfg != NULL) {
        pci_ctrl->ops.pre_cfg(pci_ctrl);
    }

    return DEVDRV_LOAD_WAIT_INTERVAL;
}

STATIC int devdrv_mini_v2_get_pf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_PF_CHAN_CNT;
}

STATIC int devdrv_mini_v2_get_vf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_VF_CHAN_CNT;
}

STATIC int devdrv_mini_v2_get_p2p_support_max_devnum(void)
{
    return DEVDRV_MINI_V2_P2P_SUPPORT_MAX_DEVICE;
}

STATIC int devdrv_mini_v2_is_p2p_access_cap(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_pci_ctrl *peer_pci_ctrl)
{
    if (devdrv_p2p_para_check(0, pci_ctrl->dev_id, peer_pci_ctrl->dev_id) != 0) {
        devdrv_info("devid and peer_devid is 1pf2die's die1, not support p2p. (dev_id=%u; peer_devid=%u)\n",
            pci_ctrl->dev_id, peer_pci_ctrl->dev_id);
        return DEVDRV_P2P_ACCESS_DISABLE;
    }

    return DEVDRV_P2P_ACCESS_ENABLE;
}

STATIC void devdrv_mini_v2_set_dev_shr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para->host_dev_id = (int)pci_ctrl->dev_id;
    pci_ctrl->shr_para->driver_version = DEVDRV_DRIVER_VERSION;
    /* chip1 use chip0 bar base */
    pci_ctrl->shr_para->host_mem_bar_base = (u64)pci_resource_start(pci_ctrl->pdev, PCI_BAR_MEM);
    pci_ctrl->shr_para->host_io_bar_base = (u64)pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO);
}

STATIC int devdrv_mini_v2_get_p2p_addr(struct devdrv_pci_ctrl *pci_ctrl, u32 remote_dev_id,
    enum devdrv_p2p_addr_type type, phys_addr_t *phy_addr, size_t *size)
{
    u32 dev_id = pci_ctrl->dev_id;
    u64 io_txatu_base;

    io_txatu_base = pci_ctrl->target_bar[remote_dev_id].io_txatu_base;
    if (io_txatu_base == 0) {
        devdrv_err("Invalid io tx atu base. (dev_id=%u; remote_dev_id=%u; type=%u)\n",
            dev_id, remote_dev_id, (u32)type);
        return -EINVAL;
    }

    switch (type) {
        case DEVDRV_P2P_IO_TS_DB:
            *phy_addr = (phys_addr_t)(io_txatu_base + DEVDRV_IO_TS_DB_OFFSET);
            *size = DEVDRV_IO_TS_DB_SIZE;
            break;
        case DEVDRV_P2P_IO_TS_SRAM:
            *phy_addr = (phys_addr_t)(io_txatu_base + DEVDRV_IO_TS_SRAM_OFFSET);
            *size = DEVDRV_IO_TS_SRAM_SIZE;
            break;
        case DEVDRV_P2P_IO_HWTS:
            *phy_addr = (phys_addr_t)(io_txatu_base + DEVDRV_IO_HWTS_OFFSET);
            *size = DEVDRV_IO_HWTS_SIZE;
            break;
        default:
            devdrv_err("P2P address type not support. (dev_id=%u; remote_dev_id=%u; type=%u)\n",
                dev_id, remote_dev_id, (u32)type);
            return -EINVAL;
    }

    return 0;
}

STATIC void devdrv_mini_v2_ops_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->ops.shr_para_rebuild = devdrv_shr_para_rebuild;
    pci_ctrl->ops.alloc_devid = devdrv_alloc_devid_stride_2;
    pci_ctrl->ops.is_p2p_access_cap = devdrv_mini_v2_is_p2p_access_cap;
    pci_ctrl->ops.probe_wait = devdrv_probe_wait;
    pci_ctrl->ops.bind_irq = devdrv_bind_irq;
    pci_ctrl->ops.unbind_irq = devdrv_unbind_irq;
    pci_ctrl->ops.get_load_wait_mode = devdrv_mini_v2_get_load_wait_mode;
    pci_ctrl->ops.get_pf_max_msg_chan_cnt = devdrv_mini_v2_get_pf_msg_chan_cnt;
    pci_ctrl->ops.get_vf_max_msg_chan_cnt = devdrv_mini_v2_get_vf_msg_chan_cnt;
    pci_ctrl->ops.get_p2p_support_max_devnum = devdrv_mini_v2_get_p2p_support_max_devnum;
    pci_ctrl->ops.get_hccs_link_info = NULL;
    pci_ctrl->ops.is_mdev_vm_full_spec = NULL;
    pci_ctrl->ops.devdrv_deal_suspend_handshake = NULL;
    pci_ctrl->ops.is_all_dev_unified_addr = NULL;
    pci_ctrl->ops.flush_cache = NULL;
    pci_ctrl->ops.get_peh_link_info = NULL;
    pci_ctrl->ops.set_dev_shr_info = devdrv_mini_v2_set_dev_shr_info;
    pci_ctrl->ops.link_speed_slow_to_normal = NULL;
    pci_ctrl->ops.get_p2p_addr = devdrv_mini_v2_get_p2p_addr;
    pci_ctrl->ops.get_server_id = NULL;
    pci_ctrl->ops.get_max_server_num = NULL;
    pci_ctrl->ops.check_ep_suspend_status = NULL;
    pci_ctrl->ops.pre_cfg = NULL;
}

STATIC void devdrv_mini_v2_init_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i;
    u32 irq_res_gear = devdrv_get_irq_res_gear();

    for (i = 0; i < devdrv_msg_client_max; i++) {
        pci_ctrl->res.msg_chan_cnt[i] = (int)devdrv_msg_chan_cnt_mini_v2[i];
    }

    switch (irq_res_gear) {
        case 4: /* irq resource gear is 4 */
            pci_ctrl->res.msg_chan_cnt[devdrv_msg_client_hdc] = DEVDRV_DEV_HDC_MSG_CHAN_CNT_GEAR_4;
            break;
        default:
            pci_ctrl->res.msg_chan_cnt[devdrv_msg_client_hdc] = DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX;
    }
}

/*
 * 1PF2P bar map to single pf bar space:
 * bar      offset  len     use for
 * bar0     0       8MB     P0 bar0
 *          8MB     8MB     P1 bar0
 * bar2     0       128MB   P0 bar2
 *          128MB   128MB   P1 bar2
 * bar4     0       2GB     P0 bar4
 *          2GB     2GB     P1 bar4
 */
int devdrv_mini_v2_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifndef DRV_UT
    resource_size_t offset;
    unsigned long size;
    phys_addr_t davinci_dev_mem_bar_len = 0;
    phys_addr_t davinci_dev_rsv_mem_bar_len = 0;
    phys_addr_t davinci_dev_io_bar_len = 0;
    struct devdrv_pci_ctrl *pci_ctrl_main = NULL;
    int davinci_dev_num = devdrv_get_davinci_dev_num_by_pdev(pci_ctrl->pdev);
    u64 flag_r = 0;
    int count = 0;

    if (davinci_dev_num <= 0) {
        devdrv_err("davinci_dev_num is invalid.\n");
        return -ENODEV;
    }
    davinci_dev_mem_bar_len = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_MEM) / davinci_dev_num;
    davinci_dev_rsv_mem_bar_len = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_RSV_MEM) / davinci_dev_num;
    davinci_dev_io_bar_len = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_IO) / davinci_dev_num;

    if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
        pci_ctrl->mem_bar_offset += davinci_dev_mem_bar_len;
        pci_ctrl->rsv_mem_bar_offset += davinci_dev_rsv_mem_bar_len;
        pci_ctrl->io_bar_offset += davinci_dev_io_bar_len;
    }

    pci_ctrl->mem_bar_id = PCI_BAR_MEM;

    pci_ctrl->mem_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_MEM);
    pci_ctrl->mem_phy_size = (u64)davinci_dev_mem_bar_len;
    pci_ctrl->mem_phy_base += pci_ctrl->mem_bar_offset;

    pci_ctrl->rsv_mem_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM);
    pci_ctrl->rsv_mem_phy_size = (u64)davinci_dev_rsv_mem_bar_len;
    pci_ctrl->rsv_mem_phy_base += pci_ctrl->rsv_mem_bar_offset;

    offset = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_MSG_OFFSET;
    size = DEVDRV_RESERVE_MEM_MSG_SIZE;
    pci_ctrl->mem_base = ioremap(offset, size);
    if (pci_ctrl->mem_base == NULL) {
        devdrv_err("Ioremap mem_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    offset = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_DB_BASE;
    size = DEVDRV_DB_IOMAP_SIZE;
    pci_ctrl->msi_base = ioremap(offset, size);
    if (pci_ctrl->msi_base == NULL) {
        devdrv_err("Ioremap msi_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->io_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO);
    pci_ctrl->io_phy_size = (u64)davinci_dev_io_bar_len;
    pci_ctrl->io_phy_base += pci_ctrl->io_bar_offset;

    pci_ctrl->io_base = ioremap(pci_ctrl->io_phy_base, pci_ctrl->io_phy_size);
    if (pci_ctrl->io_base == NULL) {
        devdrv_err("Ioremap io_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->shr_para = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET + DEVDRV_SHR_PARA_ADDR_OFFSET;

    pci_ctrl->res.phy_match_flag_addr = (u8 *)pci_ctrl->mem_base +
        DEVDRV_SHR_PARA_ADDR_OFFSET + PHY_MATCH_FLAG_OFFSET_IN_SHR_MEM;
    pci_ctrl->res.nvme_db_base = pci_ctrl->msi_base;
    pci_ctrl->res.nvme_pf_ctrl_base = pci_ctrl->io_base + DEVDRV_IEP_SDI0_OFFSET;
    pci_ctrl->res.load_sram_base = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET;
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

    devdrv_parse_res_gear();
    devdrv_mini_v2_init_bar_addr_info(pci_ctrl);
    devdrv_mini_v2_init_intr_info(pci_ctrl);
    devdrv_mini_v2_init_dma_info(pci_ctrl);
    devdrv_mini_v2_init_msg_cnt(pci_ctrl);
    devdrv_mini_v2_init_link_info(pci_ctrl);

    pci_ctrl->remote_dev_id = pci_ctrl->dev_id_in_pdev;
    pci_ctrl->os_load_flag = (pci_ctrl->remote_dev_id == 0) ? 1 : 0;
    pci_ctrl->shr_para->chip_id = (int)pci_ctrl->remote_dev_id;

    devdrv_mini_v2_init_load_file_info(pci_ctrl);
    devdrv_mini_v2_init_depend_module_info(pci_ctrl);
    devdrv_mini_v2_ops_init(pci_ctrl);

    pci_ctrl->local_reserve_mem_base = NULL;
#endif

    return 0;
}

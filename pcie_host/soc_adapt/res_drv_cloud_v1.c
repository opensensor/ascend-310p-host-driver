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
#include "res_drv_cloud_v1.h"

#define DEVDRV_CLOUD_V1_P2P_SUPPORT_MAX_DEVICE 16

#define PCI_BAR_RSV_MEM 0
#define PCI_BAR_IO 2
#define PCI_BAR_MEM 4

#define DEVDRV_IO_APB_OFFSET 0
#define DEVDRV_IO_APB_SIZE 0x200000
#define DEVDRV_IO_IEP_OFFSET 0x500000
#define DEVDRV_IO_IEP_SIZE 0x500000
#define DEVDRV_IO_TS_DB_OFFSET 0xA00000
#define DEVDRV_IO_TS_DB_SIZE 0x400000
#define DEVDRV_IO_TS_SRAM_OFFSET 0xE00000
#define DEVDRV_IO_TS_SRAM_SIZE 0x20000
#define DEVDRV_IO_REG_SRAM_OFFSET 0xE20900
#define DEVDRV_IO_REG_SRAM_SIZE 0xC000
#define DEVDRV_IO_LOAD_SRAM_OFFSET 0xE20000
#define DEVDRV_IO_LOAD_SRAM_SIZE 0x40000
#define DEVDRV_IO_HDR_OFFSET    0xE70000
#define DEVDRV_IO_HDR_SIZE    0x80000

#define DEVDRV_IO_HWTS_OFFSET 0xE60000
#define DEVDRV_IO_HWTS_SIZE 0x10000
#define DEVDRV_IO_HWTS_EVENT_OFFSET 0xE64000
#define DEVDRV_IO_HWTS_EVENT_SIZE 0x4000
#define DEVDRV_IO_HWTS_NOTIFY_OFFSET 0xE6E000
#define DEVDRV_IO_HWTS_NOTIFY_SIZE 0x2000

#define DEVDRV_ADDR_IMU_LOG_OFFSET 0x1000000
#define DEVDRV_ADDR_IMU_LOG_SIZE 0x800000

#define DEVDRV_IEP_SDI0_OFFSET DEVDRV_IO_IEP_OFFSET
#define DEVDRV_IEP_SDI0_SIZE 0x200000
#define DEVDRV_IEP_DMA_OFFSET (DEVDRV_IEP_SDI0_OFFSET + DEVDRV_IEP_SDI0_SIZE * 2)
#define DEVDRV_IEP_DMA_SIZE 0x4000

/* mem base */
#define DEVDRV_RESERVE_MEM_MSG_OFFSET 0x800000
#define DEVDRV_RESERVE_MEM_MSG_SIZE 0xa00000
#define DEVDRV_RESERVE_MEM_DRV_BASE 0x80000   /* driver use merroy base */

#define DEVDRV_RESERVE_MEM_TEST_OFFSET 0x200000
#define DEVDRV_RESERVE_MEM_TEST_SIZE 0x200000

#define DEVDRV_RESERVE_MEM_TS_SQ_OFFSET 0x2000000
#define DEVDRV_RESERVE_MEM_TS_SQ_SIZE 0x2000000

#define DEVDRV_RESERVE_MEM_DB_BASE 0x0 /*  doorbell base */
#define DEVDRV_DB_IOMAP_SIZE  0x100000  /* include msi table */

#define DEVDRV_RSV_MEM_BAR_SIZE 0x4000000
#define DEVDRV_RESERVE_TS_SHARE_MEM_OFFSET 0x4000000
#define DEVDRV_RESERVE_TS_SHARE_MEM_SIZE 0x2000000

/* ************ interrupt defined for normal host ************* */
#define DEVDRV_MSI_X_MAX_VECTORS 256
#define DEVDRV_MSI_X_MIN_VECTORS 128

/* device os load notify use irq vector 0, later 0 alse use to admin msg chan */
#define DEVDRV_LOAD_MSI_X_VECTOR_NUM 0

/* irq used to msg trans, a msg chan need two vector. one for tx finish, the other for rx msg.
   msg chan 0 is used to admin(chan 0) role */
#define DEVDRV_MSG_MSI_X_VECTOR_BASE 0
#define DEVDRV_MSG_MSI_X_VECTOR_NUM 58

/* irq used to dma, a dma chan need 22 vector. one for cq, the other for err.
  host support 11 dma chan witch is related to enum devdrv_dma_data_type */
#define DEVDRV_DMA_MSI_X_VECTOR_BASE 58
#define DEVDRV_DMA_MSI_X_VECTOR_NUM 22

/* irq used to devmm */
#define DEVDRV_TSDRV_MSI_X_VECTOR_BASE 80
#define DEVDRV_TSDRV_MSI_X_VECTOR_NUM 48

/* msg chan irq section2 */
#define DEVDRV_MSG_MSI_X_VECTOR_2_BASE 128

#define DMA_CHAN_REMOTE_USED_NUM            11
#define DMA_CHAN_REMOTE_USED_START_INDEX    11

#define DEVDRV_MAX_DMA_CH_SQ_DEPTH 0x10000
#define DEVDRV_MAX_DMA_CH_CQ_DEPTH 0x10000
#define DEVDRV_DMA_CH_SQ_DESC_RSV  0x400

#define CLOUD_V1_BLOCKS_NUM        4
#define CLOUD_V1_MODULE_NUM        8
#define DEVDRV_MAX_MSG_PF_CHAN_CNT 72
#define DEVDRV_MAX_MSG_VF_CHAN_CNT 0

/* msg chan cnt for modules */
#define DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX          1   /* non_trans:1 */
#define DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX         1   /* non_trans:1 */
#define DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX        1   /* non_trans:1 */
#define DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_TSDRV_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */
#define DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX          65   /* trans:64 non_trans:1 */
#define DEVDRV_QUEUE_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */

#define DEVDRV_BANDW_PACKSPEED 1000000 // (1000000 * dma_node)/s
#define DEVDRV_BANDWIDTH_REAL_RATIO 70
#define DEVDRV_PACKSPEED_REAL_RATIO 100

STATIC unsigned int devdrv_msg_chan_cnt_cloud_v1[devdrv_msg_client_max] = {
    DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX,    /* used for pcivnic */
    DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX,       /* used for test */
    DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX,      /* used for svm */
    DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX,     /* used for common */
    DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX,    /* used for device manager */
    DEVDRV_TSDRV_MSG_CHAN_CNT_MAX,          /* used for tsdrv */
    DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX,        /* used for hdc */
    DEVDRV_QUEUE_MSG_CHAN_CNT_MAX,          /* used for queue */
};

STATIC struct devdrv_load_file_cfg cloud_v1_file[CLOUD_V1_BLOCKS_NUM] = {
    {
        .file_name = "/driver/device/ascend_910.image",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910.cpio.gz",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_tee.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910.crl",
        .file_type = DEVDRV_NON_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
};

STATIC struct devdrv_depend_module cloud_v1_module[CLOUD_V1_MODULE_NUM] = {
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
};

STATIC void devdrv_cloud_v1_init_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.msg_db.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_DB_BASE;
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

    pci_ctrl->res.hwts.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HWTS_OFFSET;
    pci_ctrl->res.hwts.size = DEVDRV_IO_HWTS_SIZE;
    pci_ctrl->res.ts_notify.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HWTS_NOTIFY_OFFSET;
    pci_ctrl->res.ts_notify.size = DEVDRV_IO_HWTS_NOTIFY_SIZE;
    pci_ctrl->res.ts_event.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HWTS_EVENT_OFFSET;
    pci_ctrl->res.ts_event.size = DEVDRV_IO_HWTS_EVENT_SIZE;

    pci_ctrl->res.imu_log.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_ADDR_IMU_LOG_OFFSET;
    pci_ctrl->res.imu_log.size = DEVDRV_ADDR_IMU_LOG_SIZE;

    pci_ctrl->res.reg_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_REG_SRAM_OFFSET;
    pci_ctrl->res.reg_sram.size = DEVDRV_IO_REG_SRAM_SIZE;

    /* pcie reserve bar space for other modules */
    pci_ctrl->res.tsdrv_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_TSDRV_RESV_OFFSET;
    pci_ctrl->res.tsdrv_resv.size = DEVDRV_RESERVE_TSDRV_RESV_SIZE;
    pci_ctrl->res.devmng_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_DEVMNG_RESV_OFFSET;
    pci_ctrl->res.devmng_resv.size = DEVDRV_RESERVE_DEVMNG_RESV_SIZE;
    pci_ctrl->res.hbm_ecc_mem.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_HBM_ECC_OFFSET;
    pci_ctrl->res.hbm_ecc_mem.size = DEVDRV_RESERVE_HBM_ECC_SIZE;
    pci_ctrl->res.vf_bandwidth.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_SRAM_OFFSET +
                                      DEVDRV_VF_BANDWIDTH_OFFSET;
    pci_ctrl->res.vf_bandwidth.size = DEVDRV_VF_BANDWIDTH_SIZE;
    if (pci_ctrl->rsv_mem_phy_size > DEVDRV_RSV_MEM_BAR_SIZE) {
        pci_ctrl->res.ts_share_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_TS_SHARE_MEM_OFFSET;
        pci_ctrl->res.ts_share_mem.size = DEVDRV_RESERVE_TS_SHARE_MEM_SIZE;
    } else {
        pci_ctrl->res.ts_share_mem.addr = 0;
        pci_ctrl->res.ts_share_mem.size = 0;
    }
    /* not support */
    pci_ctrl->res.vpc.size = 0;
    pci_ctrl->res.dvpp.size = 0;
    pci_ctrl->res.bbox.size = 0;
    pci_ctrl->res.stars_sqcq.size = 0;
    pci_ctrl->res.stars_sqcq_intr.size = 0;
    pci_ctrl->res.stars_topic_sched.size = 0;
    pci_ctrl->res.stars_topic_sched_rsv_mem.size = 0;
    pci_ctrl->res.stars_cdqm.size = 0;
    pci_ctrl->res.stars_intr.size = 0;
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

STATIC void devdrv_cloud_v1_init_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
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
    intr->msg_irq_vector2_base = DEVDRV_MSG_MSI_X_VECTOR_2_BASE;
    intr->msg_irq_vector2_num = intr->max_vector - DEVDRV_MSG_MSI_X_VECTOR_2_BASE;
    intr->tsdrv_irq_vector2_base = 0;
    intr->tsdrv_irq_vector2_num = 0;
    intr->topic_sched_irq_num = 0;
    intr->cdqm_irq_num = 0;
}

STATIC void devdrv_cloud_v1_init_dma_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    u32 i;

    pci_ctrl->res.dma_res.dma_addr = pci_ctrl->io_base + DEVDRV_IEP_DMA_OFFSET;
    pci_ctrl->res.dma_res.dma_chan_addr = pci_ctrl->res.dma_res.dma_addr;
    pci_ctrl->res.dma_res.dma_chan_start_id = DMA_CHAN_REMOTE_USED_START_INDEX;
    pci_ctrl->res.dma_res.chan_start_id = pci_ctrl->res.dma_res.dma_chan_start_id;
    pci_ctrl->res.dma_res.dma_chan_num = DMA_CHAN_REMOTE_USED_NUM;
    for (i = 0; i < pci_ctrl->res.dma_res.dma_chan_num; i++) {
        pci_ctrl->res.dma_res.use_chan[i] = i;
    }
    pci_ctrl->res.dma_res.pf_num = pci_ctrl->func_id;
    pci_ctrl->res.dma_res.vf_id = 0;
    pci_ctrl->res.dma_res.sq_depth = DEVDRV_MAX_DMA_CH_SQ_DEPTH;
    pci_ctrl->res.dma_res.sq_rsv_num = DEVDRV_DMA_CH_SQ_DESC_RSV;
    pci_ctrl->res.dma_res.cq_depth = DEVDRV_MAX_DMA_CH_CQ_DEPTH;
}

STATIC void devdrv_cloud_v1_init_load_file_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.load_file.load_file_num = CLOUD_V1_BLOCKS_NUM;
    pci_ctrl->res.load_file.load_file_cfg = cloud_v1_file;
}

STATIC void devdrv_cloud_v1_init_depend_module_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.depend_info.module_num = CLOUD_V1_MODULE_NUM;
    pci_ctrl->res.depend_info.module_list = cloud_v1_module;
}

STATIC void devdrv_cloud_v1_init_link_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.link_info.bandwidth = devdrv_get_bandwidth_info(pci_ctrl->pdev);
    pci_ctrl->res.link_info.packspeed = DEVDRV_BANDW_PACKSPEED;
    pci_ctrl->res.link_info.bandwidth_ratio = DEVDRV_BANDWIDTH_REAL_RATIO;
    pci_ctrl->res.link_info.packspeed_ratio = DEVDRV_PACKSPEED_REAL_RATIO;
}

/* cloud ai server, has 2 node(4p system), host side number 0-3 --- node 0, 4-7 --- node 1
   cloud evb, has 2 node(2p system), host side number 0-1 --- node 0, 2-3 --- node 1 */
STATIC int devdrv_get_cloud_server_devid(struct devdrv_shr_para __iomem *para)
{
    int dev_id = -1;
    struct devdrv_ctrl *p_ctrls = get_devdrv_ctrl();

    if ((para->slot_id >= BOARD_CLOUD_MAX_DEV_NUM) || (para->slot_id < 0)) {
        devdrv_err("Input parameter is invalid. (slot_id=%d)\n", para->slot_id);
        return dev_id;
    }

    dev_id = para->slot_id;

    devdrv_info("Get para value. (board_type=%d; node_id=%d; dev_id=%d; host_dev_id=%d)\n",
                para->board_type, para->node_id, para->chip_id, dev_id);

    if (p_ctrls[dev_id].priv != NULL) {
        devdrv_err("This dev_id is already registered. (dev_id=%d)\n", dev_id);
        dev_id = -1;
    } else {
        p_ctrls[dev_id].startup_flg = DEVDRV_DEV_STARTUP_PROBED;
    }
    return dev_id;
}

STATIC int devdrv_get_cloud_pcie_card_devid(struct devdrv_pci_ctrl *pci_ctrl)
{
    int dev_id = -1;
#if  LINUX_VERSION_CODE>= KERNEL_VERSION(4, 5, 0)

#ifdef CFG_FEATURE_DMI
    struct devdrv_shr_para __iomem *para = NULL;
    const struct dmi_device *dmi_dev = NULL;
    const struct dmi_device *from = NULL;
    const struct dmi_dev_onboard *dev_data = NULL;
    struct devdrv_ctrl *p_ctrls = get_devdrv_ctrl();

    if ((pci_ctrl == NULL) || (pci_ctrl->pdev == NULL) || (pci_ctrl->pdev->bus == NULL)) {
        devdrv_err("Input parameter is invalid.\n");
        return dev_id;
    }

    do {
        from = dmi_dev;
        dmi_dev = dmi_find_device(DEVDRV_DMI_DEV_TYPE_DEV_SLOT, NULL, from);
        if (dmi_dev != NULL) {
            dev_data = (struct dmi_dev_onboard *)dmi_dev->device_data;
            if ((dev_data != NULL) && (dev_data->bus == pci_ctrl->pdev->bus->number) &&
                (PCI_SLOT(((unsigned int)(dev_data->devfn))) == PCI_SLOT(pci_ctrl->pdev->devfn))) {
                dev_id = dev_data->instance;
                break;
            }
        }
    } while (dmi_dev != NULL);

    if ((dev_id >= MAX_DEV_CNT) || (dev_id < 0)) {
        devdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return dev_id;
    }

    para = pci_ctrl->shr_para;
    if (para != NULL) {
        devdrv_info("Get para value. board_type=%d; node_id=%d; dev_id=%d; host_dev_id=%d)\n",
                    para->board_type, para->node_id, para->chip_id, dev_id);
    }

    if (p_ctrls[dev_id].priv != NULL) {
        devdrv_err("This dev_id is already registered. (dev_id=%u)\n", dev_id);
        dev_id = -1;
    } else {
        p_ctrls[dev_id].startup_flg = DEVDRV_DEV_STARTUP_PROBED;
    }
#else
    dev_id = devdrv_alloc_devid_inturn(0, 1);
#endif

#else
    dev_id = devdrv_alloc_devid_inturn(0, 1);
#endif
    return dev_id;
}

STATIC int devdrv_alloc_devid_cloud(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_shr_para __iomem *para = NULL;
    int dev_id = -1;
    int board_type;

    para = pci_ctrl->shr_para;
    board_type = para->board_type;

    if ((board_type == BOARD_CLOUD_AI_SERVER) || (board_type == BOARD_CLOUD_EVB)) {
        dev_id = devdrv_get_cloud_server_devid(para);
    } else {
        /* cloud pcie card devid get from pcie slot, hccn tool will set pcie card ip address by pcie slot */
        dev_id = devdrv_get_cloud_pcie_card_devid(pci_ctrl);
    }

    return dev_id;
}

STATIC int devdrv_cloud_v1_alloc_devid(struct devdrv_ctrl *ctrl_this)
{
    int dev_id;

    dev_id = devdrv_alloc_devid_check_ctrls(ctrl_this);
    if (dev_id != -1) {
        return dev_id;
    }

    return devdrv_alloc_devid_cloud((struct devdrv_pci_ctrl *)ctrl_this->priv);
}

STATIC int devdrv_cloud_v1_is_p2p_access_cap(struct devdrv_pci_ctrl *pci_ctrl,
                                            struct devdrv_pci_ctrl *peer_pci_ctrl)
{
    if (pci_ctrl->shr_para->board_type == BOARD_CLOUD_AI_SERVER &&
        peer_pci_ctrl->shr_para->board_type == BOARD_CLOUD_AI_SERVER) {
        devdrv_info("devid and peer_devid in ai server. (dev_id=%u; peer_devid=%u)\n", pci_ctrl->dev_id,
                    peer_pci_ctrl->dev_id);
        return DEVDRV_P2P_ACCESS_ENABLE;
    }

    return DEVDRV_P2P_ACCESS_UNKNOWN;
}

STATIC enum devdrv_load_wait_mode devdrv_cloud_v1_get_load_wait_mode(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->ops.pre_cfg != NULL) {
        pci_ctrl->ops.pre_cfg(pci_ctrl);
    }

    return DEVDRV_LOAD_WAIT_INTERVAL;
}

STATIC int devdrv_cloud_v1_get_pf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_PF_CHAN_CNT;
}

STATIC int devdrv_cloud_v1_get_vf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_VF_CHAN_CNT;
}

STATIC int devdrv_cloud_v1_get_p2p_support_max_devnum(void)
{
    return DEVDRV_CLOUD_V1_P2P_SUPPORT_MAX_DEVICE;
}

STATIC void devdrv_cloud_v1_set_dev_shr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para->host_dev_id = (int)pci_ctrl->dev_id;
    pci_ctrl->shr_para->host_mem_bar_base = (u64)pci_ctrl->mem_phy_base;
    pci_ctrl->shr_para->host_io_bar_base = (u64)pci_ctrl->io_phy_base;
}

STATIC void devdrv_cloud_v1_ops_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->ops.shr_para_rebuild = NULL;
    pci_ctrl->ops.alloc_devid = devdrv_cloud_v1_alloc_devid;
    pci_ctrl->ops.is_p2p_access_cap = devdrv_cloud_v1_is_p2p_access_cap;
    pci_ctrl->ops.probe_wait = devdrv_probe_wait;
    pci_ctrl->ops.bind_irq = devdrv_bind_irq;
    pci_ctrl->ops.unbind_irq = devdrv_unbind_irq;
    pci_ctrl->ops.get_load_wait_mode = devdrv_cloud_v1_get_load_wait_mode;
    pci_ctrl->ops.get_pf_max_msg_chan_cnt = devdrv_cloud_v1_get_pf_msg_chan_cnt;
    pci_ctrl->ops.get_vf_max_msg_chan_cnt = devdrv_cloud_v1_get_vf_msg_chan_cnt;
    pci_ctrl->ops.get_p2p_support_max_devnum = devdrv_cloud_v1_get_p2p_support_max_devnum;
    pci_ctrl->ops.get_hccs_link_info = NULL;
    pci_ctrl->ops.is_mdev_vm_full_spec = NULL;
    pci_ctrl->ops.devdrv_deal_suspend_handshake = NULL;
    pci_ctrl->ops.is_all_dev_unified_addr = NULL;
    pci_ctrl->ops.flush_cache = NULL;
    pci_ctrl->ops.get_peh_link_info = NULL;
    pci_ctrl->ops.set_dev_shr_info = devdrv_cloud_v1_set_dev_shr_info;
    pci_ctrl->ops.link_speed_slow_to_normal = NULL;
    pci_ctrl->ops.get_p2p_addr = NULL;
    pci_ctrl->ops.get_server_id = NULL;
    pci_ctrl->ops.get_max_server_num = NULL;
    pci_ctrl->ops.check_ep_suspend_status = NULL;
    pci_ctrl->ops.pre_cfg = NULL;
}

STATIC void devdrv_cloud_v1_init_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i;
    for (i = 0; i < devdrv_msg_client_max; i++) {
        pci_ctrl->res.msg_chan_cnt[i] = (int)devdrv_msg_chan_cnt_cloud_v1[i];
    }
}

int devdrv_cloud_v1_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    resource_size_t offset;
    unsigned long size;

    pci_ctrl->mem_bar_id = PCI_BAR_MEM;

    pci_ctrl->mem_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_MEM);
    pci_ctrl->mem_phy_size = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_MEM);

    offset = pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM) + DEVDRV_RESERVE_MEM_MSG_OFFSET;
    size = DEVDRV_RESERVE_MEM_MSG_SIZE;
    pci_ctrl->mem_base = ioremap(offset, size);
    if (pci_ctrl->mem_base == NULL) {
        devdrv_err("Ioremap mem_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->rsv_mem_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM);
    pci_ctrl->rsv_mem_phy_size = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_RSV_MEM);

    offset = pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM) + DEVDRV_RESERVE_MEM_DB_BASE;
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
        devdrv_err("Ioremap io_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->shr_para = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET + DEVDRV_SHR_PARA_ADDR_OFFSET;

    pci_ctrl->res.phy_match_flag_addr = pci_ctrl->msi_base +
        DEVDRV_RESERVE_MEM_DRV_BASE + DEVDRV_HOST_PHY_MACH_FLAG_OFFSET;
    pci_ctrl->res.nvme_db_base = pci_ctrl->msi_base;
    pci_ctrl->res.nvme_pf_ctrl_base = pci_ctrl->io_base + DEVDRV_IEP_SDI0_OFFSET;
    pci_ctrl->res.load_sram_base = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET;

    devdrv_cloud_v1_init_bar_addr_info(pci_ctrl);
    devdrv_cloud_v1_init_intr_info(pci_ctrl);
    devdrv_cloud_v1_init_dma_info(pci_ctrl);
    devdrv_cloud_v1_init_load_file_info(pci_ctrl);
    devdrv_cloud_v1_init_depend_module_info(pci_ctrl);
    devdrv_cloud_v1_ops_init(pci_ctrl);
    devdrv_cloud_v1_init_msg_cnt(pci_ctrl);
    devdrv_cloud_v1_init_link_info(pci_ctrl);

    pci_ctrl->remote_dev_id = (u32)pci_ctrl->shr_para->chip_id;
    pci_ctrl->os_load_flag = (pci_ctrl->remote_dev_id == 0) ? 1 : 0;
    pci_ctrl->local_reserve_mem_base = NULL;
    return 0;
}

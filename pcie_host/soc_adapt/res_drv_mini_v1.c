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
#include "res_drv_mini_v1.h"

#define DEVDRV_MINI_V1_P2P_SUPPORT_MAX_DEVICE 8

#define PCI_BAR_RSV_MEM 0
#define PCI_BAR_IO 2
#define PCI_BAR_MEM 4

#define DEVDRV_IO_IEP_OFFSET 0x300000
#define DEVDRV_IO_IEP_SIZE 0x300000
#define DEVDRV_IO_TS_DB_OFFSET 0x600000
#define DEVDRV_IO_TS_DB_SIZE 0x400000
#define DEVDRV_IO_TS_SRAM_OFFSET 0xA00000
#define DEVDRV_IO_TS_SRAM_SIZE 0x20000
#define DEVDRV_IO_LOAD_SRAM_OFFSET 0xA20000
#define DEVDRV_IO_LOAD_SRAM_SIZE 0x40000
#define DEVDRV_IO_HDR_OFFSET      0xA60000
#define DEVDRV_IO_HDR_SIZE        0x80000

#define MEM_BAR_LEN_64M 0x4000000
#define MEM_BAR_LEN_128M 0x8000000
#define MEM_BAR_LEN_4G 0x100000000ULL
#define MEM_BAR_LEN_8G 0x200000000ULL
#define MEM_BAR_LEN_16G 0x400000000ULL

#define DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_64M 0x2000000
#define DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_128M 0x2000000
#define DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_4G 0x60000000
#define DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_8G 0x60000000
#define DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_16G 0x60000000
#define DEVDRV_BAR4_RESERVE_TS_SQ_SIZE 0x2000000

#define DEVDRV_BAR4_RESERVE_TEST_OFFSET_64M 0x800000
#define DEVDRV_BAR4_RESERVE_TEST_OFFSET_128M 0x800000
#define DEVDRV_BAR4_RESERVE_TEST_OFFSET_4G 0x6FE00000
#define DEVDRV_BAR4_RESERVE_TEST_OFFSET_8G 0x6FE00000
#define DEVDRV_BAR4_RESERVE_TEST_OFFSET_16G 0x6FE00000
#define DEVDRV_BAR4_RESERVE_TEST_SIZE 0x200000

#define DEVDRV_IEP_SDI0_OFFSET  DEVDRV_IO_IEP_OFFSET
#define DEVDRV_IEP_SDI0_SIZE    0x200000
#define DEVDRV_IEP_DMA_OFFSET (DEVDRV_IEP_SDI0_OFFSET + DEVDRV_IEP_SDI0_SIZE)
#define DEVDRV_IEP_DMA_SIZE 0x4000

#define DEVDRV_NVME_PF_LOCAL_CTRL_REG_BASE 0x80200

/* mem base, at pos 0 on bar4 */
#define DEVDRV_RESERVE_MEM_MSG_OFFSET 0
#define DEVDRV_RESERVE_MEM_MSG_SIZE 0x800000
#define DEVDRV_RESERVE_MEM_DB_BASE  0x0  /* doorbell base */
#define DEVDRV_DB_IOMAP_SIZE  0x20000  /* include msi table */

#define DMA_CHAN_REMOTE_USED_NUM 6
#define DMA_CHAN_REMOTE_USED_START_INDEX 6

/* sq is 16bit in cq desc, sq size 32byte, guaranteed total size is less than 4M */
#define DEVDRV_MAX_DMA_CH_SQ_DEPTH 0x3000
#define DEVDRV_MAX_DMA_CH_CQ_DEPTH 0x3000
#define DEVDRV_DMA_CH_SQ_DESC_RSV  128

#define MINI_V1_BLOCKS_NUM         7
#define MINI_V1_MODULE_NUM         5
#define DEVDRV_MAX_MSG_PF_CHAN_CNT 80
#define DEVDRV_MAX_MSG_VF_CHAN_CNT 0

#define DEVDRV_WAIT_DDR_INIT_TIME 1000 /* Wait 1s for DDR init */

/* msg chan cnt for modules */
#define DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX          1   /* non_trans:1 */
#define DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX         1   /* non_trans:1 */
#define DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX        1   /* non_trans:1 */
#define DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX       1   /* non_trans:1 */
#define DEVDRV_TSDRV_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */
#define DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX          17   /* irq_reso_dear max (trans:16 non_trans:1) */
#define DEVDRV_QUEUE_MSG_CHAN_CNT_MAX             1   /* non_trans:1 */

#define DEVDRV_DEV_HDC_MSG_CHAN_CNT_MIN           3   /* irq_reso_dear 0 and 35X9 (trans:2 non_trans:1) */

STATIC unsigned int devdrv_msg_chan_cnt_mini_v1[devdrv_msg_client_max] = {
    DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX,    /* used for pcivnic */
    DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX,       /* used for test */
    DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX,      /* used for svm */
    DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX,     /* used for common */
    DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX,    /* used for device manager */
    DEVDRV_TSDRV_MSG_CHAN_CNT_MAX,          /* used for tsdrv */
    DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX,        /* used for hdc */
    DEVDRV_QUEUE_MSG_CHAN_CNT_MAX,          /* used for queue */
};

STATIC struct devdrv_load_file_cfg mini_v1_file[MINI_V1_BLOCKS_NUM] = {
    {
        .file_name = "/driver/device/davinci_mini.fd",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/davinci_mini_dt.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/davinci_mini.image",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/davinci_mini.cpio.gz",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/davinci_mini_lpm3.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/davinci_mini_tee.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_310.crl",
        .file_type = DEVDRV_NON_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
};

STATIC struct devdrv_depend_module mini_v1_module[MINI_V1_MODULE_NUM] = {
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
        .module_name = "drv_tsdrv_platform_host",
        .status = DEVDRV_MODULE_UNPROBED,
    },
};

STATIC struct devdrv_irq_gear_info mini_v1_normal_host_irq_info[DEVDRV_RES_GEAR_VAL_CNT] = {
    {
        /* irq_gear 0 */
        .flag = DEVDRV_IRQ_GEAR_INFO_VALID,
        .intr = {
            .min_vector = 32,
            .max_vector = 32,
            .device_os_load_irq = 0,
            .msg_irq_base = 0,
            .msg_irq_num = 16,
            .dma_irq_base = 16,
            .dma_irq_num = 12,
            .tsdrv_irq_base = 28,
            .tsdrv_irq_num = 4,
            .topic_sched_irq_num = 0,
            .cdqm_irq_num= 0,
            .msg_irq_vector2_base = 0,
            .msg_irq_vector2_num = 0,
            .tsdrv_irq_vector2_base = 0,
            .tsdrv_irq_vector2_num = 0,
        },
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
        .flag = DEVDRV_IRQ_GEAR_INFO_INVALID,
    },
    {
        /* irq_gear 5 */
        .flag = DEVDRV_IRQ_GEAR_INFO_INVALID,
    },
    {
        /* irq_gear 6 */
        .flag = DEVDRV_IRQ_GEAR_INFO_VALID,
        .intr = {
            .min_vector = 116,
            .max_vector = 116,
            .device_os_load_irq = 0,
            .msg_irq_base = 0,
            .msg_irq_num = 64,
            .dma_irq_base = 64,
            .dma_irq_num = 12,
            .tsdrv_irq_base = 80,
            .tsdrv_irq_num = 36,
            .topic_sched_irq_num = 0,
            .cdqm_irq_num= 0,
            .msg_irq_vector2_base = 0,
            .msg_irq_vector2_num = 0,
            .tsdrv_irq_vector2_base = 0,
            .tsdrv_irq_vector2_num = 0,
        },
    },
};

STATIC struct devdrv_irq_gear_info mini_v1_35x9_host_irq_info = {
    .flag = DEVDRV_IRQ_GEAR_INFO_VALID,
    .intr = {
        .min_vector = 32,
        .max_vector = 32,
        .device_os_load_irq = 0,
        .msg_irq_base = 0,
        .msg_irq_num = 16,
        .dma_irq_base = 16,
        .dma_irq_num = 12,
        .tsdrv_irq_base = 28,
        .tsdrv_irq_num = 4,
        .topic_sched_irq_num = 0,
        .cdqm_irq_num= 0,
        .msg_irq_vector2_base = 0,
        .msg_irq_vector2_num = 0,
        .tsdrv_irq_vector2_base = 0,
        .tsdrv_irq_vector2_num = 0,
    },
};

STATIC void devdrv_get_offset_by_size(u32 dev_id, u64 phy_size, u32 *ts_sq_offset, u32 *test_offset)
{
    switch (phy_size) {
        case MEM_BAR_LEN_64M:
            *ts_sq_offset = DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_64M;
            *test_offset = DEVDRV_BAR4_RESERVE_TEST_OFFSET_64M;
            break;
        case MEM_BAR_LEN_128M:
            *ts_sq_offset = DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_128M;
            *test_offset = DEVDRV_BAR4_RESERVE_TEST_OFFSET_128M;
            break;
        case MEM_BAR_LEN_4G:
            *ts_sq_offset = DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_4G;
            *test_offset = DEVDRV_BAR4_RESERVE_TEST_OFFSET_4G;
            break;
        case MEM_BAR_LEN_8G:
            *ts_sq_offset = DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_8G;
            *test_offset = DEVDRV_BAR4_RESERVE_TEST_OFFSET_8G;
            break;
        case MEM_BAR_LEN_16G:
            *ts_sq_offset = DEVDRV_BAR4_RESERVE_TS_SQ_OFFSET_16G;
            *test_offset = DEVDRV_BAR4_RESERVE_TEST_OFFSET_16G;
            break;
        default:
            devdrv_err("Memory bar size is invalid. (dev_id=%u; mem_phy_size=0x%llx)\n", dev_id, phy_size);
            *ts_sq_offset = 0;
            *test_offset = 0;
            break;
    }
}

STATIC void devdrv_mini_v1_init_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    u32 ts_sq_offset, test_offset;

    pci_ctrl->res.msg_db.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_DB_BASE;
    pci_ctrl->res.msg_db.size = DEVDRV_DB_IOMAP_SIZE;

    pci_ctrl->res.msg_mem.addr = pci_ctrl->mem_phy_base + DEVDRV_RESERVE_MEM_MSG_OFFSET;
    pci_ctrl->res.msg_mem.size = DEVDRV_RESERVE_MEM_MSG_SIZE;

    devdrv_get_offset_by_size(pci_ctrl->dev_id, pci_ctrl->mem_phy_size, &ts_sq_offset, &test_offset);

    pci_ctrl->res.ts_db.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_DB_OFFSET;
    pci_ctrl->res.ts_db.size = DEVDRV_IO_TS_DB_SIZE;

    pci_ctrl->res.ts_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_SRAM_OFFSET;
    pci_ctrl->res.ts_sram.size = DEVDRV_IO_TS_SRAM_SIZE;

    if (ts_sq_offset != 0) {
        pci_ctrl->res.ts_sq.addr = pci_ctrl->mem_phy_base + ts_sq_offset;
        pci_ctrl->res.ts_sq.size = DEVDRV_BAR4_RESERVE_TS_SQ_SIZE;

        pci_ctrl->res.test.addr = pci_ctrl->mem_phy_base + test_offset;
        pci_ctrl->res.test.size = DEVDRV_BAR4_RESERVE_TEST_SIZE;
    } else {
        /* failed when get addr */
        pci_ctrl->res.ts_sq.size = 0;
        pci_ctrl->res.test.size = 0;
    }

    pci_ctrl->res.load_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_LOAD_SRAM_OFFSET;
    pci_ctrl->res.load_sram.size = DEVDRV_IO_LOAD_SRAM_SIZE;

    pci_ctrl->res.hdr.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HDR_OFFSET;
    pci_ctrl->res.hdr.size = DEVDRV_IO_HDR_SIZE;

    /* not support */
    pci_ctrl->res.vpc.size = 0;
    pci_ctrl->res.dvpp.size = 0;
    pci_ctrl->res.hwts.size = 0;
    pci_ctrl->res.ts_notify.size = 0;
    pci_ctrl->res.ts_event.size = 0;
    pci_ctrl->res.imu_log.size = 0;
    pci_ctrl->res.bbox.size = 0;
    pci_ctrl->res.stars_sqcq.size = 0;
    pci_ctrl->res.stars_sqcq_intr.size = 0;
    pci_ctrl->res.stars_topic_sched.size = 0;
    pci_ctrl->res.stars_topic_sched_rsv_mem.size = 0;
    pci_ctrl->res.stars_cdqm.size = 0;
    pci_ctrl->res.stars_intr.size = 0;
    pci_ctrl->res.reg_sram.size = 0;
    pci_ctrl->res.vf_bandwidth.addr = 0;
    pci_ctrl->res.vf_bandwidth.size = 0;
    pci_ctrl->res.ts_share_mem.addr = 0;
    pci_ctrl->res.ts_share_mem.size = 0;
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
    pci_ctrl->res.hbm_ecc_mem.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_HBM_ECC_OFFSET;
    pci_ctrl->res.hbm_ecc_mem.size = DEVDRV_RESERVE_HBM_ECC_SIZE;

    pci_ctrl->res.devmng_info_mem.size = 0;
}

STATIC void devdrv_mini_v1_init_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_intr_info *intr = &pci_ctrl->res.intr;
    u32 irq_res_gear = devdrv_get_irq_res_gear();

    if ((devdrv_get_host_type() == HOST_TYPE_ARM_3559) || (devdrv_get_host_type() == HOST_TYPE_ARM_3519)) {
        *intr = mini_v1_35x9_host_irq_info.intr;
    } else {
        if ((irq_res_gear <= DEVDRV_RES_GEAR_MAX_VAL) &&
            (mini_v1_normal_host_irq_info[irq_res_gear].flag == DEVDRV_IRQ_GEAR_INFO_VALID)) {
            *intr = mini_v1_normal_host_irq_info[irq_res_gear].intr;
        } else {
            devdrv_info("Irq gear is invalid, use default gear. (irq_res_gear=%u)\n", irq_res_gear);
            *intr = mini_v1_normal_host_irq_info[DEVDRV_RES_GEAR_MAX_VAL].intr;
        }
    }
}

STATIC void devdrv_mini_v1_init_dma_info(struct devdrv_pci_ctrl *pci_ctrl)
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

STATIC void devdrv_mini_v1_init_load_file_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.load_file.load_file_num = MINI_V1_BLOCKS_NUM;
    pci_ctrl->res.load_file.load_file_cfg = mini_v1_file;
}

STATIC void devdrv_mini_v1_init_depend_module_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.depend_info.module_num = MINI_V1_MODULE_NUM;
    pci_ctrl->res.depend_info.module_list = mini_v1_module;
}

STATIC void devdrv_mini_v1_init_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i;
    for (i = 0; i < devdrv_msg_client_max; i++) {
        pci_ctrl->res.msg_chan_cnt[i] = (int)devdrv_msg_chan_cnt_mini_v1[i];
    }
    if ((devdrv_get_host_type() == HOST_TYPE_ARM_3559) || (devdrv_get_host_type() == HOST_TYPE_ARM_3519)) {
        pci_ctrl->res.msg_chan_cnt[devdrv_msg_client_hdc] = DEVDRV_DEV_HDC_MSG_CHAN_CNT_MIN;
    } else {
        u32 irq_res_gear = devdrv_get_irq_res_gear();
        switch (irq_res_gear) {
            case 0:
                pci_ctrl->res.msg_chan_cnt[devdrv_msg_client_hdc] = DEVDRV_DEV_HDC_MSG_CHAN_CNT_MIN;
                break;
            default:
                pci_ctrl->res.msg_chan_cnt[devdrv_msg_client_hdc] = DEVDRV_DEV_HDC_MSG_CHAN_CNT_MAX;
        }
    }
}

STATIC int devdrv_mini_v1_alloc_devid(struct devdrv_ctrl *ctrl_this)
{
    int dev_id;

    dev_id = devdrv_alloc_devid_check_ctrls(ctrl_this);
    if (dev_id != -1) {
        return dev_id;
    }

    return devdrv_alloc_devid_inturn(0, 1);
}

STATIC int devdrv_mini_v1_is_p2p_access_cap(struct devdrv_pci_ctrl *pci_ctrl,
                                            struct devdrv_pci_ctrl *peer_pci_ctrl)
{
    devdrv_info("This chip_type not support p2p. (dev_id=%u; chip_type=%d)\n", pci_ctrl->dev_id,
        pci_ctrl->shr_para->chip_type);
    return DEVDRV_P2P_ACCESS_DISABLE;
}

STATIC enum devdrv_load_wait_mode devdrv_mini_v1_get_load_wait_mode(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->ops.pre_cfg != NULL) {
        pci_ctrl->ops.pre_cfg(pci_ctrl);
    }

    return DEVDRV_LOAD_WAIT_UNKNOWN;
}

STATIC int devdrv_mini_v1_get_pf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_PF_CHAN_CNT;
}

STATIC int devdrv_mini_v1_get_vf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_VF_CHAN_CNT;
}

STATIC int devdrv_mini_v1_get_p2p_support_max_devnum(void)
{
    return DEVDRV_MINI_V1_P2P_SUPPORT_MAX_DEVICE;
}

STATIC void devdrv_mini_v1_set_dev_shr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para->host_dev_id = (int)pci_ctrl->dev_id;
    pci_ctrl->shr_para->host_mem_bar_base = (u64)pci_ctrl->mem_phy_base;
    pci_ctrl->shr_para->host_io_bar_base = (u64)pci_ctrl->io_phy_base;
}

STATIC void devdrv_mini_v1_ops_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->ops.shr_para_rebuild = NULL;
    pci_ctrl->ops.alloc_devid = devdrv_mini_v1_alloc_devid;
    pci_ctrl->ops.is_p2p_access_cap = devdrv_mini_v1_is_p2p_access_cap;
    pci_ctrl->ops.probe_wait = NULL;
    pci_ctrl->ops.bind_irq = NULL;
    pci_ctrl->ops.unbind_irq = NULL;
    pci_ctrl->ops.get_load_wait_mode = devdrv_mini_v1_get_load_wait_mode;
    pci_ctrl->ops.get_pf_max_msg_chan_cnt = devdrv_mini_v1_get_pf_msg_chan_cnt;
    pci_ctrl->ops.get_vf_max_msg_chan_cnt = devdrv_mini_v1_get_vf_msg_chan_cnt;
    pci_ctrl->ops.get_p2p_support_max_devnum = devdrv_mini_v1_get_p2p_support_max_devnum;
    pci_ctrl->ops.get_hccs_link_info = NULL;
    pci_ctrl->ops.is_mdev_vm_full_spec = NULL;
    pci_ctrl->ops.devdrv_deal_suspend_handshake = NULL;
    pci_ctrl->ops.is_all_dev_unified_addr = NULL;
    pci_ctrl->ops.flush_cache = NULL;
    pci_ctrl->ops.get_peh_link_info = NULL;
    pci_ctrl->ops.set_dev_shr_info = devdrv_mini_v1_set_dev_shr_info;
    pci_ctrl->ops.link_speed_slow_to_normal = NULL;
    pci_ctrl->ops.get_p2p_addr = NULL;
    pci_ctrl->ops.get_server_id = NULL;
    pci_ctrl->ops.get_max_server_num = NULL;
    pci_ctrl->ops.check_ep_suspend_status = NULL;
    pci_ctrl->ops.pre_cfg = NULL;
}

int devdrv_mini_v1_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    resource_size_t offset;
    unsigned long size;
    u64 flag_r = 0;
    int count = 0;

    pci_ctrl->mem_bar_id = PCI_BAR_MEM;

    pci_ctrl->mem_phy_base = (phys_addr_t)pci_resource_start(pci_ctrl->pdev, PCI_BAR_MEM);
    pci_ctrl->mem_phy_size = (u64)pci_resource_len(pci_ctrl->pdev, PCI_BAR_MEM);

    offset = pci_resource_start(pci_ctrl->pdev, PCI_BAR_MEM) + DEVDRV_RESERVE_MEM_MSG_OFFSET;
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

    pci_ctrl->shr_para = pci_ctrl->mem_base + DEVDRV_SHR_PARA_ADDR_OFFSET;

    pci_ctrl->res.phy_match_flag_addr = pci_ctrl->mem_base + DEVDRV_HOST_PHY_MACH_FLAG_OFFSET;
    pci_ctrl->res.nvme_db_base = pci_ctrl->msi_base;
    pci_ctrl->res.nvme_pf_ctrl_base = pci_ctrl->io_base + DEVDRV_IEP_SDI0_OFFSET + DEVDRV_NVME_PF_LOCAL_CTRL_REG_BASE;
    pci_ctrl->res.load_sram_base = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET;

    while (1) {
        flag_r = readq(pci_ctrl->res.load_sram_base);
        if ((flag_r == DEVDRV_NORMAL_BOOT_MODE) || (flag_r == DEVDRV_SLOW_BOOT_MODE)) {
            break;
        }
        count++;
        if (count >= DEVDRV_WAIT_BOOT_MODE_MAX) {
            devdrv_err("Wait boot mode from bios time out. (flag_r=0x%llx)\n", flag_r);
            devdrv_res_uninit(pci_ctrl);
            return -EFAULT;
        }
        msleep(DEVDRV_WAIT_BOOT_MODE_SLEEP_TIME);
    }

    msleep(DEVDRV_WAIT_DDR_INIT_TIME);

    devdrv_parse_res_gear();
    devdrv_mini_v1_init_bar_addr_info(pci_ctrl);
    devdrv_mini_v1_init_intr_info(pci_ctrl);
    devdrv_mini_v1_init_dma_info(pci_ctrl);
    devdrv_mini_v1_init_load_file_info(pci_ctrl);
    devdrv_mini_v1_init_depend_module_info(pci_ctrl);
    devdrv_mini_v1_ops_init(pci_ctrl);
    devdrv_mini_v1_init_msg_cnt(pci_ctrl);

    pci_ctrl->remote_dev_id = 0;
    pci_ctrl->os_load_flag = 1;

    pci_ctrl->shr_para->load_flag = 1;
    pci_ctrl->shr_para->chip_id = 0;
    pci_ctrl->shr_para->node_id = 0;
    pci_ctrl->shr_para->slot_id = 0;
    pci_ctrl->shr_para->dev_num = 1;
    pci_ctrl->local_reserve_mem_base = NULL;

    return 0;
}

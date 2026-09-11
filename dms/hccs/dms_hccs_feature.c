/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-11-11
*/
#include <linux/mutex.h>
#include <linux/securec.h>
#include <asm/io.h>
#include <linux/uaccess.h>

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "devdrv_user_common.h"
#include "dms_define.h"
#include "dms_cmd_def.h"
#include "dms_template.h"
#include "dms_basic_info.h"
#include "dms_timer.h"
#include "devdrv_manager_hccs.h"
#include "devdrv_common.h"
#include "ascend_kernel_hal.h"
#include "ascend_platform.h"
#include "dms_hccs_feature.h"

#ifdef CFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
STATIC const unsigned long hdlc_phy_addr[PCS_NUM] = {HDLC0_BASE_ADDR};

unsigned int hccs_reg_read(unsigned long vir_addr)
{
    void __iomem *p_dst = NULL;
    unsigned int val = 0;

    p_dst = (void __iomem *)(uintptr_t)vir_addr;
    if (p_dst != NULL) {
        val = readl((const volatile void __iomem *)p_dst);
    }

    devdrv_drv_debug("rd,addr:0x%016lX:0x%08X\n", vir_addr, val);

    return val;
}

void hccs_reg_write(unsigned long vir_addr, unsigned int val)
{
    void __iomem *p_dst = NULL;

    p_dst = (void __iomem *)(uintptr_t)vir_addr;
    if (p_dst != NULL) {
        writel(val, (volatile void __iomem *)p_dst);
    }
}
#else
STATIC unsigned long pcs_phy_addr[PCS_NUM] = {
    PCS0_BASE_ADDR, PCS1_BASE_ADDR, PCS2_BASE_ADDR, PCS3_BASE_ADDR,
    PCS4_BASE_ADDR, PCS5_BASE_ADDR, PCS6_BASE_ADDR, PCS7_BASE_ADDR
    };

/*
|HPCS4  HPCS5 | HPCS6  HPCS7 | HPCS0  HPCS1 | HPCS2  HPCS3 |
|----------------------------------------------------------|
|LINK0  LINK1 | LINK0  LINK1 | LINK0  LINK1 | LINK0  LINK1 |
|    HDLC0    |     HDLC1    |     HDLC2    |     HDLC3    |

link_num = pcs_index % 2
*/
STATIC const unsigned long hdlc_phy_addr[PCS_NUM] = {
    HDLC2_BASE_ADDR, HDLC2_BASE_ADDR,
    HDLC3_BASE_ADDR, HDLC3_BASE_ADDR,
    HDLC0_BASE_ADDR, HDLC0_BASE_ADDR,
    HDLC1_BASE_ADDR, HDLC1_BASE_ADDR,
    };
#endif

#ifdef CFG_FEATURE_GET_PCS_BITMAP_BY_BOARD_TYPE
/*
 *  BOARD ID
 *  bit7: 0 1die; 1 2die
 *  bit4-6: 000 EVB
 *          001 PCIE(1die), module(2die)
 *          others: module
 */
#define BOARD_ID_TYPE_MASK 0xF0
#define BOARD_ID_TYPE_OFFSET 4
#define BOARD_TYPE_EVB_SINGLE_DIE 0
#define BOARD_TYPE_EVB_DOUBLE_DIE 8
#define BOARD_TYPE_PCIE_SINGLE_DIE 1
#define PCS_BITMAP_PCIE_EVB 0xFC
#define PCS_BITMAP_MODULE 0xFE
int dms_get_hpcs_bitmap_by_board_type(unsigned int dev_id, unsigned long long *bitmap)
{
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    unsigned int board_id;
    unsigned int board_type;

    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get device ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV ;
    }

    if (dev_cb->dev_info == NULL) {
        dms_err("Device ctrl dev_info is null. (dev_id=%u)\n", dev_id);
        return -ENODEV ;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    board_id = dev_info->board_id;
    board_type = (board_id & BOARD_ID_TYPE_MASK) >> BOARD_ID_TYPE_OFFSET;
    if ((board_type == BOARD_TYPE_EVB_SINGLE_DIE) ||
        (board_type == BOARD_TYPE_EVB_DOUBLE_DIE) ||
        (board_type == BOARD_TYPE_PCIE_SINGLE_DIE)) {
        *bitmap = PCS_BITMAP_PCIE_EVB;
    } else {
        *bitmap = PCS_BITMAP_MODULE;
    }

    return 0;
}

#else
int dms_get_hpcs_bitmap_default(unsigned int dev_id, unsigned long long *bitmap)
{
    *bitmap = 0;
    return 0;
}

#endif

#ifdef CFG_FEATURE_HCCS_GET_STATUS
#define BIOS_HPCS_BITMAP_EFFECT_VER     (2)
int dms_get_hpcs_status_by_dev_id(unsigned int dev_id, unsigned long long pcs_bitmap, unsigned long long phy_addr_offset, hccs_info_t *hccs_status)
{
    int i;
    u32 pcs_status_reg;
    void __iomem *hccs_base_addr = NULL;

    for (i = 0; i < PCS_NUM; i++) {
        if (!(pcs_bitmap & (1 << i))) {
            continue;
        }

        hccs_base_addr = ioremap(pcs_phy_addr[i] + phy_addr_offset, KA_MM_PAGE_SIZE);
        if (hccs_base_addr == NULL) {
            dms_err("Remap addr for pcs status failed. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }

        pcs_status_reg = HCCS_REG_RD(hccs_base_addr, ST_CH_PCS_LANE_MODE_CHANGE_OFFSET);
        iounmap(hccs_base_addr);
        hccs_base_addr = NULL;

        if ((((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_mode_working == ST_PCS_MODE_WORKING_X4) &&
            (((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_use_working == ST_PCS_USE_WORKING_X4)) {
            hccs_status->pcs_status = 0;
        } else {
            hccs_status->pcs_status = (1 << PCS_STATUS_OFFSET) | (i << PCS_INDEX_OFFSET) |
                (((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_mode_working << PCS_MODE_WORKING_OFFSET) |
                (((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_use_working << PCS_USE_WORKING_OFFSET);
            break;
        }
    }
    return 0;
}

int dms_get_hdlc_status_by_dev_id(unsigned int dev_id, unsigned long long pcs_bitmap, unsigned long long phy_addr_offset, hccs_info_t *hccs_status)
{
    int i;
    u32 hdlc_status_reg;
    void __iomem *hccs_base_addr = NULL;

    for (i = 0; i < PCS_NUM; i++) {
        if (!(pcs_bitmap & (1 << i))) {
            continue;
        }
        hccs_base_addr = ioremap(hdlc_phy_addr[i] + phy_addr_offset, HDLC_REG_MAP_SIZE);
        if (hccs_base_addr == NULL) {
            dms_err("Remap addr for hdlc status failed. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }
        hdlc_status_reg = HCCS_REG_RD(hccs_base_addr, HDLC_FSM_ADDR + (i % HDLC_LINK_NUM * HDLC_REG_SIZE));
        iounmap(hccs_base_addr);
        hccs_base_addr = NULL;
        if ((hdlc_status_reg & HDLC_INIT_SUCCESS) == HDLC_INIT_SUCCESS) {
            hccs_status->hdlc_status = HDLC_INIT_SUCCESS;
        } else {
            hccs_status->hdlc_status = hdlc_status_reg;
            break;
        }
    }
    return 0;
}

int dms_get_hccs_status_by_dev_id(unsigned int dev_id, hccs_info_t *hccs_status)
{
    int ret;
    u32 die_id, chip_id;
    unsigned long long hccs_bitmap = 0;
    devdrv_hardware_info_t hardware_info = {0};

    ret = hal_kernel_get_hardware_info(dev_id, &hardware_info);
    if (ret != 0) {
        dms_err("Failed to invoke hal_kernel_get_hardware_info. (devid=%u)\n", dev_id);
        return ret;
    }

    if (hardware_info.base_hw_info.version >= BIOS_HPCS_BITMAP_EFFECT_VER) {
        ret = devdrv_get_chip_die_id(dev_id, &chip_id, &die_id);
        if ((ret != 0) || (die_id > 1)) { /* hccs_hpcs_bitmap: u16; pcs_bitmap: u8; die_id <= 1 */
            dms_err("Failed to get die_id by devid. (devid=%u; dieid=%u; ret=%d)\n", dev_id, die_id, ret);
            return ret;
        }
        hccs_bitmap = ((u8 *)&hardware_info.base_hw_info.hccs_hpcs_bitmap)[die_id];
    } else {
        ret = DMS_GET_HPCS_BITMAP(dev_id, &hccs_bitmap);
        if (ret != 0) {
            dms_err("Get hpcs bitmap failed. (device_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
    }

    ret = dms_get_hpcs_status_by_dev_id(dev_id, hccs_bitmap, hardware_info.phy_addr_offset, hccs_status);
    if (ret != 0) {
        return ret;
    }
    ret = dms_get_hdlc_status_by_dev_id(dev_id, hccs_bitmap, hardware_info.phy_addr_offset, hccs_status);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

STATIC int dms_get_hccs_status(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    hccs_info_t hccs_status = {0};

    if (in->buff_size < sizeof(hccs_info_t)) {
        dms_err("The buff_size is too small. (buff_size=%u; min_size=%zu)\n", in->buff_size, sizeof(hccs_info_t));
        return -EINVAL;
    }

    ret = dms_get_hccs_status_by_dev_id(in->dev_id, &hccs_status);
    if (ret != 0) {
        return ret;
    }

    ret = copy_to_user((void *)(uintptr_t)in->buff, &hccs_status, sizeof(hccs_info_t));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    *out_size = sizeof(hccs_info_t);
    return 0;
}
#endif

#ifdef CFG_FEATURE_HCCS_GET_LANE_INFO
#define PCS_USED_FOR_HCCS               (1)
#define PCS_USED_NOT_FOR_HCCS           (0)
#define LANE_INFO_MODE_CHANGE_OFFSET    (0)
#define LANE_INFO_USED_LANE_OFFSET      (1)
#define LANE_INFO_MODE_WORK_OFFSET      (9)
int dms_get_hccs_lane_details(unsigned int dev_id, hccs_lane_info_t *hccs_lane_info)
{
    int i = 0, j = 0, ret;
    u8 pcs_bitmap = 0;
    u32 die_id, chip_id, pcs_status_reg;
    void __iomem *hccs_base_addr = NULL;
    devdrv_hardware_info_t hardware_info = {0};

    if (hccs_lane_info == NULL) {
        return -EINVAL;
    }

    ret = hal_kernel_get_hardware_info(dev_id, &hardware_info);
    if (ret != 0) {
        dms_err("Failed to invoke hal_kernel_get_hardware_info. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (hardware_info.base_hw_info.version < BIOS_HPCS_BITMAP_EFFECT_VER) {
        return -EOPNOTSUPP;
    }

    ret = devdrv_get_chip_die_id(dev_id, &chip_id, &die_id);
    if ((ret != 0) || (die_id > 1)) { /* hccs_hpcs_bitmap: u16; pcs_bitmap: u8; die_id <= 1 */
        dms_err("Failed to get die_id by devid. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    pcs_bitmap = ((u8 *)&hardware_info.base_hw_info.hccs_hpcs_bitmap)[die_id];
    hccs_lane_info->hccs_port_pcs_bitmap = pcs_bitmap;

    for (i = 0; i < PCS_NUM; i++) {
        if ((pcs_bitmap & (1 << i)) == PCS_USED_NOT_FOR_HCCS) {
            continue;
        }

        hccs_base_addr = ioremap(pcs_phy_addr[i] + hardware_info.phy_addr_offset, KA_MM_PAGE_SIZE);
        if (hccs_base_addr == NULL) {
            dms_err("Failed to ioremap for hpcs info. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }

        pcs_status_reg = HCCS_REG_RD(hccs_base_addr, ST_CH_PCS_LANE_MODE_CHANGE_OFFSET);
        iounmap(hccs_base_addr);
        hccs_base_addr = NULL;

        /* if the switch is not complete, it will skip here. */
        if (((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_mode_change_done == 0) {
            hccs_lane_info->pcs_lane_bitmap[j++] = 0;
            continue;
        }
        hccs_lane_info->pcs_lane_bitmap[j] |=
            (((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_mode_change_done << LANE_INFO_MODE_CHANGE_OFFSET);
        hccs_lane_info->pcs_lane_bitmap[j] |=
            (((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_use_working << LANE_INFO_USED_LANE_OFFSET);
        hccs_lane_info->pcs_lane_bitmap[j] |=
            (((hccs_pcs_status_reg_t *)&pcs_status_reg)->st_pcs_mode_working << LANE_INFO_MODE_WORK_OFFSET);
        j++;
    }
    return 0;
}
KA_EXPORT_SYMBOL(dms_get_hccs_lane_details);

STATIC int dms_get_hccs_lane_info(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    hccs_lane_info_t hccs_lane_info = {0};

    if (in->buff_size < sizeof(hccs_lane_info_t)) {
        dms_err("The buff_size is too small. (buff_size=%u; min_size=%zu)\n", in->buff_size, sizeof(hccs_lane_info_t));
        return -EINVAL;
    }

    ret = dms_get_hccs_lane_details(in->dev_id, &hccs_lane_info);
    if (ret != 0) {
        return ret;
    }

    ret = copy_to_user((void *)(uintptr_t)in->buff, &hccs_lane_info, sizeof(hccs_lane_info_t));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    dms_debug("Calling dms_get_hccs_lane_info succeeded. (dev_id=%u)\n", in->dev_id);
    *out_size = sizeof(hccs_lane_info_t);
    return 0;
}
#endif

#define HCCS_STATISTIC_TIMER_EXPIRE_MS 500
#define HCCS_STATISTIC_READ_ERR_MAX_CNT 6   /* 500ms * 6 */

#define DMS_TIMER_TASK_INVALID_ID UINT_MAX

#ifndef ASCEND_PDEV_MAX_NUM
#define ASCEND_PDEV_MAX_NUM 2
#endif

#define UINT32_BIT_WIDTH 32

#define HCCS_REG_RD_ACC(to, base_addr, reg) hccs_reg_read_acc(to, ((unsigned long)(uintptr_t)base_addr) + (reg))

#ifdef CFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
#define HCCS_CHANNEL_NUM 3

STATIC const unsigned long hdlc_tx_chan_addr[HCCS_CHANNEL_NUM] = {
    HDLC_TX_CNT_CH0_ADDR, HDLC_TX_CNT_CH1_ADDR, HDLC_TX_CNT_CH2_ADDR
};

STATIC const unsigned long hdlc_rx_chan_addr[HCCS_CHANNEL_NUM] = {
    HDLC_RX_CNT_CH0_ADDR, HDLC_RX_CNT_CH1_ADDR, HDLC_RX_CNT_CH2_ADDR
};
#endif

struct hccs_statistic_cache {
    hccs_statistic_info_ext_t info;
    int read_status;
    unsigned int read_err_cnt;
    unsigned int task_id;
    struct mutex lock;
#ifdef CFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
    unsigned long long chan_tx_cnt[PCS_NUM][HCCS_CHANNEL_NUM];
    unsigned long long chan_rx_cnt[PCS_NUM][HCCS_CHANNEL_NUM];
#endif
};

STATIC struct hccs_statistic_cache g_hccs_statistic_cache[ASCEND_PDEV_MAX_NUM] = {0};

STATIC void hccs_reg_read_acc(unsigned long long *value, unsigned long addr)
{
    unsigned long long last_value = *value;
    unsigned int last_value_low32;
    unsigned int curr_value_low32;
    unsigned long long carry_count;

    curr_value_low32 = hccs_reg_read(addr);
    last_value_low32 = (unsigned int)last_value;
    carry_count = last_value >> UINT32_BIT_WIDTH;

    if (curr_value_low32 < last_value_low32) {
        /* The u32 register value overflow, a carry is needed. */
        carry_count += 1;
    }
    *value = (carry_count << UINT32_BIT_WIDTH) | curr_value_low32;
}

STATIC int read_hccs_statistic_info(unsigned int dev_id, struct hccs_statistic_cache *cache)
{
    unsigned int i = 0;
    int ret;
#ifdef CFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
    unsigned int chan = 0;
#else
    unsigned long link_index;
#endif
    void __iomem *hccs_base_addr = NULL;
    devdrv_hardware_info_t hardware_info = {0};

    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err_ratelimited("Invalid input. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = hal_kernel_get_hardware_info(dev_id, &hardware_info);
    if (ret != 0) {
        dms_err_ratelimited("Failed to invoke hal_kernel_get_hardware_info. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    for (i = 0; i < PCS_NUM; i++) {
        hccs_base_addr = ioremap(hdlc_phy_addr[i] + hardware_info.phy_addr_offset, HDLC_REG_MAP_SIZE);
        if (hccs_base_addr == NULL) {
            dms_err_ratelimited("Failed to ioremap for hdlc info. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }

#ifdef CFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
        cache->info.retry_cnt[i] = HCCS_REG_RD(hccs_base_addr, HDLC_RETRY_CNT_ADDR + (i * HDLC_REG_SIZE));
        cache->info.crc_err_cnt[i] = HCCS_REG_RD(hccs_base_addr, HDLC_CRC_ERR_CNT_ADDR + (i * HDLC_REG_SIZE));
        cache->info.tx_cnt[i] = 0;
        cache->info.rx_cnt[i] = 0;
        for (chan = 0; chan < HCCS_CHANNEL_NUM; chan++) {
            HCCS_REG_RD_ACC(&cache->chan_tx_cnt[i][chan], hccs_base_addr, hdlc_tx_chan_addr[chan] + (i * HDLC_REG_SIZE));
            HCCS_REG_RD_ACC(&cache->chan_rx_cnt[i][chan], hccs_base_addr, hdlc_rx_chan_addr[chan] + (i * HDLC_REG_SIZE));
            cache->info.tx_cnt[i] += cache->chan_tx_cnt[i][chan];
            cache->info.rx_cnt[i] += cache->chan_rx_cnt[i][chan];
        }
#else
        link_index = i % HDLC_LINK_NUM;
        cache->info.retry_cnt[i] = HCCS_REG_RD(hccs_base_addr, HDLC_RETRY_CNT_ADDR + (link_index * HDLC_REG_SIZE));
        cache->info.crc_err_cnt[i] = HCCS_REG_RD(hccs_base_addr, HDLC_CRC_ERR_CNT_ADDR + (link_index * HDLC_REG_SIZE));
        HCCS_REG_RD_ACC(&cache->info.tx_cnt[i], hccs_base_addr, HDLC_TX_CNT_ADDR + (link_index * HDLC_REG_SIZE));
        HCCS_REG_RD_ACC(&cache->info.rx_cnt[i], hccs_base_addr, HDLC_RX_CNT_ADDR + (link_index * HDLC_REG_SIZE));
#endif
        iounmap(hccs_base_addr);
        hccs_base_addr = NULL;

        dms_debug("Get hccs staticstic info. (hpcs_id=%d; tx_cnt=0x%llx; rx_cnt=0x%llx; retry_cnt=0x%llx; crc_err_cnt=0x%llx)\n", i,
            cache->info.tx_cnt[i], cache->info.rx_cnt[i], cache->info.retry_cnt[i], cache->info.crc_err_cnt[i]);
    }

    return 0;
}

STATIC int dms_refresh_hccs_statistic_cache(u64 user_data)
{
    int ret;
    unsigned int dev_id = (u32)user_data;
    struct hccs_statistic_cache *cache;

    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid input. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    cache = &g_hccs_statistic_cache[dev_id];
    if (cache->read_err_cnt >= HCCS_STATISTIC_READ_ERR_MAX_CNT) {
        return -EFAULT;
    }

    mutex_lock(&cache->lock);
    ret = read_hccs_statistic_info(dev_id, cache);
    if (ret != 0) {
        cache->read_err_cnt += 1;
        if (cache->read_err_cnt >= HCCS_STATISTIC_READ_ERR_MAX_CNT) {
            cache->read_status = ret;
            dms_err("Read hccs statistic info failed consecutively %d times, stopped the timer. (dev_id=%u; ret=%d)\n",
                HCCS_STATISTIC_READ_ERR_MAX_CNT, dev_id, ret);
        }
    } else {
        cache->read_err_cnt = 0;
        cache->read_status = 0;
    }
    mutex_unlock(&cache->lock);

    return ret;
}

STATIC int dms_get_hccs_statistic_info_ext(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    struct hccs_statistic_cache *cache;

    if (in->buff_size < sizeof(hccs_statistic_info_ext_t)) {
        dms_err("The buff_size is too small. (buff_size=%u; min_size=%zu)\n",
            in->buff_size, sizeof(hccs_statistic_info_ext_t));
        return -EINVAL;
    }

    if (in->dev_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid input. (dev_id=%u)\n", in->dev_id);
        return -EINVAL;
    }

    cache = &g_hccs_statistic_cache[in->dev_id];
    mutex_lock(&cache->lock);
    ret = cache->read_status;
    if (ret != 0) {
        mutex_unlock(&cache->lock);
        return ret;
    }

    ret = (int)copy_to_user((void *)(uintptr_t)in->buff, &cache->info, sizeof(hccs_statistic_info_ext_t));
    mutex_unlock(&cache->lock);

    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    dms_debug("Calling dms_get_hccs_statistic_info_ext succeeded. (dev_id=%u)\n", in->dev_id);
    *out_size = sizeof(hccs_statistic_info_ext_t);
    return 0;
}

STATIC int dms_get_hccs_statistic_details(unsigned int dev_id, hccs_statistic_info_t *hccs_statistic)
{
    unsigned int pcs;
    int ret;
    struct hccs_statistic_cache *cache;

    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid input. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    cache = &g_hccs_statistic_cache[dev_id];
    mutex_lock(&cache->lock);
    ret = cache->read_status;
    if (ret != 0) {
        mutex_unlock(&cache->lock);
        return ret;
    }
    for (pcs = 0; pcs < PCS_NUM; pcs++) {
        hccs_statistic->retry_cnt[pcs] = cache->info.retry_cnt[pcs];
        hccs_statistic->crc_err_cnt[pcs] = cache->info.crc_err_cnt[pcs];
        hccs_statistic->rx_cnt[pcs] = cache->info.rx_cnt[pcs];
        hccs_statistic->tx_cnt[pcs] = cache->info.tx_cnt[pcs];
    }
    mutex_unlock(&cache->lock);
    return 0;
}

STATIC int dms_get_hccs_statistic_info(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    hccs_statistic_info_t hccs_statistic_info = {0};

    if (in->buff_size < sizeof(hccs_statistic_info_t)) {
        dms_err("The buff_size is too small. (buff_size=%u; min_size=%zu)\n",
            in->buff_size, sizeof(hccs_statistic_info_t));
        return -EINVAL;
    }

    ret = dms_get_hccs_statistic_details(in->dev_id, &hccs_statistic_info);
    if (ret != 0) {
        return ret;
    }

    ret = (int)copy_to_user((void *)(uintptr_t)in->buff, &hccs_statistic_info, sizeof(hccs_statistic_info_t));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    dms_debug("Calling dms_get_hccs_statistic_info succeeded. (dev_id=%u)\n", in->dev_id);
    *out_size = sizeof(hccs_statistic_info_t);
    return 0;
}

int dms_feature_get_hccs_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    u32 physical_dev_id = UDA_INVALID_UDEVID, vfid = 0;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = {0};
    unsigned int out_size = 0;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if (input->buff == NULL) {
        dms_err("Input buffer is null or buffer size is not valid. (buff_is_null=%d; buff_size=%u)\n",
            (input->buff != NULL), input->buff_size);
        return -EINVAL;
    }

    output = (struct dms_get_device_info_out *)out;
    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(input->dev_id, &physical_dev_id, &vfid);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id (logical_id=%u; physical_id=%u; ret=%d)\n",
            input->dev_id, physical_dev_id, ret);
        return -EINVAL;
    }
    input->dev_id = physical_dev_id;

    switch (input->sub_cmd) {
        case DMS_HCCS_SUB_CMD_STATUS:
#ifdef CFG_FEATURE_HCCS_GET_STATUS
            ret = dms_get_hccs_status(input, &out_size);
            if (ret != 0) {
                dms_err("Failed to get hccs status. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
                return ret;
            }
#else
            return -EOPNOTSUPP;
#endif
            break;
        case DMS_HCCS_SUB_CMD_LANE_INFO:
#ifdef CFG_FEATURE_HCCS_GET_LANE_INFO
            ret = dms_get_hccs_lane_info(input, &out_size);
            if (ret != 0) {
                dms_ex_notsupport_err(ret, "Failed to get hccs lane info. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
                return ret;
            }
#else
            return -EOPNOTSUPP;
#endif
            break;
        case DMS_HCCS_SUB_CMD_STATISTIC_INFO:
            ret = dms_get_hccs_statistic_info(input, &out_size);
            if (ret != 0) {
                dms_ex_notsupport_err(ret, "Get hccs statistic fail. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
                return ret;
            }
            break;
        case DMS_HCCS_SUB_CMD_STATISTIC_INFO_EXT:
            ret = dms_get_hccs_statistic_info_ext(input, &out_size);
            if (ret != 0) {
                dms_ex_notsupport_err(ret, "Get hccs statistic fail. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
                return ret;
            }
            break;
        default:
            dms_err("Input sub_cmd is invalid. (dev_id=%u; sub_cmd=%d)\n", input->dev_id, input->sub_cmd);
            return -EINVAL;
    }

    output->out_size = out_size;
    return 0;
}

int dms_hccs_statistic_task_register(u32 dev_id)
{
    int ret;
    unsigned int pcs;
#ifdef CFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
    unsigned int chan;
#endif
    struct dms_timer_task hccs_statistic_task = {0};
    struct hccs_statistic_cache *cache;

    /* Virtual devices are not supported, return 0 to avoid affecting their startup. */
    if (!uda_is_phy_dev(dev_id)) {
        return 0;
    }

    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid input. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    cache = &g_hccs_statistic_cache[dev_id];
    mutex_lock(&cache->lock);
    for (pcs = 0; pcs < PCS_NUM; pcs++) {
        cache->info.rx_cnt[pcs] = 0;
        cache->info.tx_cnt[pcs] = 0;
        cache->info.retry_cnt[pcs] = 0;
        cache->info.crc_err_cnt[pcs] = 0;
#ifdef CFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
        for (chan = 0; chan < HCCS_CHANNEL_NUM; chan++) {
            cache->chan_rx_cnt[pcs][chan] = 0;
        }
#endif
    }
    cache->read_err_cnt = 0;
    cache->read_status = 0;

    hccs_statistic_task.expire_ms = HCCS_STATISTIC_TIMER_EXPIRE_MS;
    hccs_statistic_task.user_data = dev_id;
    hccs_statistic_task.handler_mode = INDEPENDENCE_WORK;
    hccs_statistic_task.exec_task = dms_refresh_hccs_statistic_cache;

    ret = dms_timer_task_register(&hccs_statistic_task, &cache->task_id);
    if (ret != 0) {
        cache->read_status = ret;  /* ENOSPC or ENOMEM */
        cache->task_id = DMS_TIMER_TASK_INVALID_ID;
        dms_err("Dms timer hccs statistic task register failed. (ret=%d)\n", ret);
    }
    mutex_unlock(&cache->lock);
    return ret;
}

int dms_hccs_statistic_task_unregister(u32 dev_id)
{
    int ret = 0;
    struct hccs_statistic_cache *cache;

    /* Virtual devices are not supported, return 0 to avoid affecting their startup. */
    if (!uda_is_phy_dev(dev_id)) {
        return 0;
    }

    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid input. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    cache = &g_hccs_statistic_cache[dev_id];
    mutex_lock(&cache->lock);
    if (cache->task_id != DMS_TIMER_TASK_INVALID_ID) {
        ret = dms_timer_task_unregister(cache->task_id);
        if (ret != 0) {
            dms_err("Dms timer hccs statistic task unregister failed. (ret=%d)\n", ret);
        }
    }
    cache->task_id = DMS_TIMER_TASK_INVALID_ID;
    cache->read_status = -ENODEV;
    mutex_unlock(&cache->lock);

    return ret;
}

int dms_hccs_feature_init(void)
{
    unsigned int i;

    for (i = 0; i < ASCEND_PDEV_MAX_NUM; ++i) {
        mutex_init(&g_hccs_statistic_cache[i].lock);
    }
    return 0;
}

void dms_hccs_feature_exit(void)
{
    unsigned int i;

    for (i = 0; i < ASCEND_PDEV_MAX_NUM; ++i) {
        mutex_destroy(&g_hccs_statistic_cache[i].lock);
    }
}

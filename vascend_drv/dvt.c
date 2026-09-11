/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/vfio.h>

#include <linux/slab.h>
#include <linux/module.h>

#include "kvmdt.h"
#include "mmio.h"
#include "vfio_ops.h"
#include "dvt_sysfs.h"
#include "dvt.h"
#include "priv_ops.h"

struct hw_device_info {
    unsigned short vendor;
    unsigned short device;
};

struct vfg_info {
    int stats[VDAVINCI_VFG_MAX];
};

static struct vdavinci_type types_310i[TYPE_MAX_310I] = {
    {"vir01", TYPE_VIR01_310I, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        1, 3, 1, 1, 2, 1, 0, 1, 1},
    {"vir02", TYPE_VIR02_310I, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        2, 6, 2, 3, 4, 2, 1, 3, 1},
    {"vir02_1c", TYPE_VIR02_1C_310I, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        2, 6, 1, 3, 4, 2, 0, 3, 1},
    {"vir04", TYPE_VIR04_310I, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 12, 4, 6, 8, 4, 2, 6, 1},
    {"vir04_3c", TYPE_VIR04_3C_310I, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 12, 3, 6, 8, 4, 1, 6, 1},
    {"vir04_3c_ndvpp", TYPE_VIR04_3C_NDVPP_310I, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 12, 3, 0, 0, 0, 0, 0, 1},
    {"vir04_4c_dvpp", TYPE_VIR04_4C_DVPP_310I, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 12, 4, 12, 16, 8, 3, 12, 1},
};

static struct vdavinci_type types_310v[TYPE_MAX_310V] = {
    {"vir01", TYPE_VIR01_310V, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        1, 6, 1, 1, 2, 1, 0, 1, 1},
    {"vir02", TYPE_VIR02_310V, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        2, 12, 2, 3, 4, 2, 1, 3, 1},
    {"vir02_1c", TYPE_VIR02_1C_310V, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        2, 12, 1, 3, 4, 2, 0, 3, 1},
    {"vir04", TYPE_VIR04_310V, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 24, 4, 6, 8, 4, 2, 6, 1},
    {"vir04_3c", TYPE_VIR04_3C_310V, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 24, 3, 6, 8, 4, 1, 6, 1},
    {"vir04_3c_ndvpp", TYPE_VIR04_3C_NDVPP_310V, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 24, 3, 0, 0, 0, 0, 0, 1},
    {"vir04_4c_dvpp", TYPE_VIR04_4C_DVPP_310V, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 24, 4, 12, 16, 8, 3, 12, 1},
};

STATIC struct vdavinci_type types_910[TYPE_MAX_910] = {
    {"vir02", TYPE_VIR02_910, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        2, 0, 0, 0, 0, 0, 0, 0, 1},
    {"vir04", TYPE_VIR04_910, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        4, 0, 0, 0, 0, 0, 0, 0, 1},
    {"vir08", TYPE_VIR08_910, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        8, 0, 0, 0, 0, 0, 0, 0, 1},
    {"vir16", TYPE_VIR16_910, DVT_MMIO_BAR0_SIZE, DVT_MMIO_BAR2_SIZE, 0,
        16, 0, 0, 0, 0, 0, 0, 0, 1},
};

/* 24core, 64G */
static struct vdavinci_type types_910b_v1[TYPE_MAX_910B_V1] = {
    {"vir06_2c_16g", TYPE_VIR06_2C_16G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 6, 16, 2, 2, 7, 1, 0, 0, 1},
    {"vir06_1c_16g", TYPE_VIR06_1C_16G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 6, 16, 1, 2, 7, 1, 0, 0, 1},
    {"vir12_3c_32g_nm", TYPE_VIR12_3C_32G_NM, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 12, 32, 3, 0, 0, 0, 0, 0, 1},
    {"vir12_4c_32g_m", TYPE_VIR12_4C_32G_M, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 12, 32, 4, 10, 28, 4, 0, 2, 1},
    {"vir12_3c_32g", TYPE_VIR12_3C_32G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 12, 32, 3, 5, 14, 2, 0, 1, 1},
    {"vir12_4c_32g", TYPE_VIR12_4C_32G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 12, 32, 4, 5, 14, 2, 0, 1, 1},
};

/* 20core, 32G */
static struct vdavinci_type types_910b_v2[TYPE_MAX_910B_V2] = {
    {"vir05_1c_8g", TYPE_VIR05_1C_8G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 5, 8, 1, 2, 6, 1, 0, 0, 1},
    {"vir05_2c_8g", TYPE_VIR05_2C_8G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 5, 8, 2, 2, 6, 1, 0, 0, 1},
    {"vir10_3c_16g_nm", TYPE_VIR10_3C_16G_NM, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 16, 3, 0, 0, 0, 0, 0, 1},
    {"vir10_4c_16g_m", TYPE_VIR10_4C_16G_M, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 16, 4, 9, 24, 4, 0, 2, 1},
    {"vir10_3c_16g", TYPE_VIR10_3C_16G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 16, 3, 4, 12, 2, 0, 1, 1},
    {"vir10_4c_16g", TYPE_VIR10_4C_16G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 16, 4, 4, 12, 2, 0, 1, 1},

};

/* 20core, 64G */
static struct vdavinci_type types_910b_v3[TYPE_MAX_910B_V3] = {
    {"vir05_1c_16g", TYPE_VIR05_1C_16G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 5, 16, 1, 2, 6, 1, 0, 0, 1},
    {"vir05_2c_16g", TYPE_VIR05_2C_16G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 5, 16, 2, 2, 6, 1, 0, 0, 1},
    {"vir10_3c_32g_nm", TYPE_VIR10_3C_32G_NM, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 32, 3, 0, 0, 0, 0, 0, 1},
    {"vir10_4c_32g_m", TYPE_VIR10_4C_32G_M, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 32, 4, 9, 24, 4, 0, 2, 1},
    {"vir10_3c_32g", TYPE_VIR10_3C_32G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 32, 3, 4, 12, 2, 0, 1, 1},
    {"vir10_4c_32g", TYPE_VIR10_4C_32G, VF_MMIO_BAR0_SIZE_910B, VF_MMIO_BAR2_SIZE_910B,
        VF_MMIO_BAR4_SIZE_910B, 10, 32, 4, 4, 12, 2, 0, 1, 1},

};

/* 24core, 64G */
static struct vdavinci_type types_910_93_v1[] = {
    {"vir06_2c_16g", TYPE_VIR06_2C_16G_910_93_V1, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 6, 16, 2, 2, 7, 1, 0, 0, 1},
    {"vir06_1c_16g", TYPE_VIR06_1C_16G_910_93_V1, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 6, 16, 1, 2, 7, 1, 0, 0, 1},
    {"vir12_3c_32g_nm", TYPE_VIR12_3C_32G_NM_910_93_V1, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 12, 32, 3, 0, 0, 0, 0, 0, 1},
    {"vir12_4c_32g_m", TYPE_VIR12_4C_32G_M_910_93_V1, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 12, 32, 4, 10, 28, 4, 0, 2, 1},
    {"vir12_3c_32g", TYPE_VIR12_3C_32G_910_93_V1, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 12, 32, 3, 5, 14, 2, 0, 1, 1},
    {"vir12_4c_32g", TYPE_VIR12_4C_32G_910_93_V1, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 12, 32, 4, 5, 14, 2, 0, 1, 1},
    {"vir24_7c_64g", TYPE_VIR24_7C_64G_910_93_V1, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 24, 64, 7, 10, 28, 4, 0, 2, 1}
};

/* 20core, 32G */
static struct vdavinci_type types_910_93_v2[] = {
    {"vir05_1c_8g", TYPE_VIR05_1C_8G_910_93_V2, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 5, 8, 1, 2, 6, 1, 0, 0, 1},
    {"vir05_2c_8g", TYPE_VIR05_2C_8G_910_93_V2, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 5, 8, 2, 2, 6, 1, 0, 0, 1},
    {"vir10_3c_16g_nm", TYPE_VIR10_3C_16G_NM_910_93_V2, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 16, 3, 0, 0, 0, 0, 0, 1},
    {"vir10_4c_16g_m", TYPE_VIR10_4C_16G_M_910_93_V2, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 16, 4, 9, 24, 4, 0, 2, 1},
    {"vir10_3c_16g", TYPE_VIR10_3C_16G_910_93_V2, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 16, 3, 4, 12, 2, 0, 1, 1},
    {"vir10_4c_16g", TYPE_VIR10_4C_16G_910_93_V2, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 16, 4, 4, 12, 2, 0, 1, 1},
    {"vir20_7c_32g", TYPE_VIR20_7C_32G_910_93_V2, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 24, 32, 7, 9, 24, 4, 0, 2, 1},
};

/* 20core, 64G */
static struct vdavinci_type types_910_93_v3[] = {
    {"vir05_1c_16g", TYPE_VIR05_1C_16G_910_93_V3, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 5, 16, 1, 2, 6, 1, 0, 0, 1},
    {"vir05_2c_16g", TYPE_VIR05_2C_16G_910_93_V3, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 5, 16, 2, 2, 6, 1, 0, 0, 1},
    {"vir10_3c_32g_nm", TYPE_VIR10_3C_32G_NM_910_93_V3, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 32, 3, 0, 0, 0, 0, 0, 1},
    {"vir10_4c_32g_m", TYPE_VIR10_4C_32G_M_910_93_V3, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 32, 4, 9, 24, 4, 0, 2, 1},
    {"vir10_3c_32g", TYPE_VIR10_3C_32G_910_93_V3, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 32, 3, 4, 12, 2, 0, 1, 1},
    {"vir10_4c_32g", TYPE_VIR10_4C_32G_910_93_V3, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 10, 32, 4, 4, 12, 2, 0, 1, 1},
    {"vir20_7c_64g", TYPE_VIR20_7C_64G_910_93_V3, VF_MMIO_BAR0_SIZE_910_93, VF_MMIO_BAR2_SIZE_910_93,
        VF_MMIO_BAR4_SIZE_910_93, 20, 64, 7, 9, 24, 4, 0, 2, 1},
};

/* 36core, 128G */
static struct vdavinci_type types_950_bin0[] = {
    {"vir16_7c_60g", TYPE_VIR16_7C_60G_BIN0, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 16, 60, 7, 2, 4, 2, 0, 0, 1},
    {"vir08_3c_30g", TYPE_VIR08_3C_30G_BIN0, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 8, 30, 3, 1, 2, 1, 0, 0, 1},
    {"vir04_1c_15g", TYPE_VIR04_1C_15G_BIN0, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 4, 15, 1, 0, 0, 0, 0, 0, 1},
};
 
/* 32core, 128G */
static struct vdavinci_type types_950_bin1[] = {
    {"vir16_7c_60g", TYPE_VIR16_7C_60G_BIN1, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 16, 60, 5, 2, 4, 2, 0, 0, 1},
    {"vir08_3c_30g", TYPE_VIR08_3C_30G_BIN1, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 8, 30, 2, 1, 2, 1, 0, 0, 1},
    {"vir04_1c_15g", TYPE_VIR04_1C_15G_BIN1, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 4, 15, 1, 0, 0, 0, 0, 0, 1},
};
 
/* 28core, 128G */
static struct vdavinci_type types_950_bin2[] = {
    {"vir14_5c_60g", TYPE_VIR14_5C_60G_BIN2, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 14, 60, 5, 1, 4, 1, 0, 0, 1},
    {"vir07_2c_30g", TYPE_VIR07_2C_30G_BIN2, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 7, 30, 2, 0, 0, 0, 0, 0, 1},
};
 
/* 28core, 112G */
static struct vdavinci_type types_950_bin3[] = {
    {"vir14_5c_52g", TYPE_VIR14_5C_52G_BIN3, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 14, 52, 5, 1, 4, 1, 0, 0, 1},
    {"vir07_2c_26g", TYPE_VIR07_2C_26G_BIN3, VF_MMIO_BAR0_SIZE_950,
     VF_MMIO_BAR2_SIZE_950, VF_MMIO_BAR4_SIZE_950, 7, 26, 2, 0, 0, 0, 0, 0, 1},
};

struct types_num {
    unsigned short device;
    unsigned int aicore;
    unsigned long mem_size;
    unsigned int size;
    struct vdavinci_type *types;
};

static const struct types_num vdavinci_types_num[] = {
    {PCI_DEVICE_ID_ASCEND310P, 8, 24, TYPE_MAX_310I, types_310i},
    {PCI_DEVICE_ID_ASCEND310P, 8, 48, TYPE_MAX_310V, types_310v},
    {PCI_DEVICE_ID_ASCEND910, 32, 0, TYPE_MAX_910, types_910},
    {PCI_DEVICE_ID_ASCEND910B, 24, 64, TYPE_MAX_910B_V1, types_910b_v1},
    {PCI_DEVICE_ID_ASCEND910B, 20, 32, TYPE_MAX_910B_V2, types_910b_v2},
    {PCI_DEVICE_ID_ASCEND910B, 20, 64, TYPE_MAX_910B_V3, types_910b_v3},
    {PCI_DEVICE_ID_ASCEND910_93, 24, 64, TYPE_MAX_910_93_V1, types_910_93_v1},
    {PCI_DEVICE_ID_ASCEND910_93, 20, 32, TYPE_MAX_910_93_V2, types_910_93_v2},
    {PCI_DEVICE_ID_ASCEND910_93, 20, 64, TYPE_MAX_910_93_V3, types_910_93_v3},
    {PCI_DEVICE_ID_ASCEND950, 36, 128, TYPE_MAX_950_BIN0, types_950_bin0},
    {PCI_DEVICE_ID_ASCEND950, 32, 128, TYPE_MAX_950_BIN1, types_950_bin1},
    {PCI_DEVICE_ID_ASCEND950, 28, 128, TYPE_MAX_950_BIN2, types_950_bin2},
    {PCI_DEVICE_ID_ASCEND950, 28, 112, TYPE_MAX_950_BIN3, types_950_bin3},
    {}
};

struct mmio_init_ops vdavinci_mmio_pf_devices_ops[] = {
#ifdef DAVINCI_TEST
    { PCI_DEVICE_ID_ASCEND310, hw_vdavinci_310_mmio_init, hw_vdavinci_310_mmio_uninit},
#endif
    { PCI_DEVICE_ID_ASCEND310P, hw_vdavinci_310pro_mmio_init, hw_vdavinci_310pro_mmio_uninit},
    { PCI_DEVICE_ID_ASCEND910, hw_vdavinci_910_mmio_init, hw_vdavinci_910_mmio_uninit},
    { PCI_DEVICE_ID_ASCEND910B, hw_vdavinci_910b_mmio_init, hw_vdavinci_910b_mmio_uninit},
    { PCI_DEVICE_ID_ASCEND910_93, hw_vdavinci_910_93_mmio_init, hw_vdavinci_910_93_mmio_uninit},
    { PCI_DEVICE_ID_ASCEND950, hw_vdavinci_950_mmio_init, hw_vdavinci_950_mmio_uninit},
    { }
};

struct mmio_init_ops vdavinci_mmio_vf_devices_ops[] = {
    { PCI_DEVICE_ID_ASCEND910B, hw_vdavinci_910b_vf_mmio_init, hw_vdavinci_910b_vf_mmio_uninit},
    { PCI_DEVICE_ID_ASCEND910_93, hw_vdavinci_910_93_vf_mmio_init, hw_vdavinci_910_93_vf_mmio_uninit},
    { PCI_DEVICE_ID_ASCEND950, hw_vdavinci_950_vf_mmio_init, hw_vdavinci_950_vf_mmio_uninit},
    { }
};

static const struct pci_device_id g_vascend_tbl[] = {{ PCI_VDEVICE(HUAWEI, PCI_DEVICE_ID_ASCEND310P), 0 },
                                                     { PCI_VDEVICE(HUAWEI, PCI_DEVICE_ID_ASCEND910), 0 },
                                                     { PCI_VDEVICE(HUAWEI, PCI_DEVICE_ID_ASCEND910B), 0 },
                                                     { PCI_VDEVICE(HUAWEI, PCI_DEVICE_ID_ASCEND910_93), 0 },
                                                     { PCI_VDEVICE(HUAWEI, PCI_DEVICE_ID_ASCEND950), 0 },
                                                     {}};
MODULE_DEVICE_TABLE(pci, g_vascend_tbl);

static struct vdavinci_drv_ops g_vascend_drv_ops = {
    .vdavinci_init = hw_dvt_init,
    .vdavinci_uninit = hw_dvt_uninit,
    .vdavinci_hypervisor_inject_msix = hw_dvt_hypervisor_inject_msix,
    .vdavinci_hypervisor_read_gpa = hw_dvt_hypervisor_read_gpa,
    .vdavinci_hypervisor_write_gpa = hw_dvt_hypervisor_write_gpa,
    .vdavinci_hypervisor_gfn_to_mfn = hw_dvt_hypervisor_gfn_to_mfn,
    .vdavinci_hypervisor_dma_pool_init = hw_dvt_hypervisor_dma_pool_init,
    .vdavinci_hypervisor_dma_pool_uninit = hw_dvt_hypervisor_dma_pool_uninit,
    .vdavinci_hypervisor_dma_map_guest_page = hw_dvt_hypervisor_dma_map_guest_page,
    .vdavinci_hypervisor_dma_unmap_guest_page = hw_dvt_hypervisor_dma_unmap_guest_page,
    .vdavinci_hypervisor_dma_pool_active = hw_dvt_hypervisor_dma_pool_active,
    .vdavinci_hypervisor_dma_map_guest_page_batch = hw_dvt_hypervisor_dma_map_guest_page_batch,
    .vdavinci_hypervisor_dma_unmap_guest_page_batch = hw_dvt_hypervisor_dma_unmap_guest_page_batch,
    .vdavinci_hypervisor_is_valid_gfn = hw_dvt_hypervisor_is_valid_gfn,
    .vdavinci_hypervisor_mmio_get = hw_dvt_hypervisor_mmio_get,
    .vdavinci_hypervisor_dma_alloc_coherent = vdavinci_dma_alloc_coherent,
    .vdavinci_hypervisor_dma_free_coherent = vdavinci_dma_free_coherent,
    .vdavinci_hypervisor_dma_map_page = vdavinci_dma_map_page,
    .vdavinci_hypervisor_dma_unmap_page = vdavinci_dma_unmap_page,
    .vdavinci_hypervisor_dma_map_single= vdavinci_dma_map_single,
    .vdavinci_hypervisor_dma_unmap_single= vdavinci_dma_unmap_single,
    .vdavinci_get_reserve_iova_for_check = get_reserve_iova_for_check,
};

static inline bool hw_vdavinci_smmu_check(struct hw_dvt *dvt, struct device *dev)
{
    struct iommu_domain *domain = NULL;

    if (!hw_vdavinci_sriov_support(dvt)) {
        return true;
    }
    domain = iommu_get_domain_for_dev(dev);
    if (domain == NULL) {
        return false;
    }

    return true;
}

bool hw_vdavinci_vf_used_num_zero(struct hw_dvt *dvt)
{
    if (dvt == NULL) {
        return true;
    }

    return dvt->sriov.vf_used == 0;
}

STATIC int hw_get_vdavinci_type(struct hw_dvt *dvt, struct vdavinci_type **type)
{
    int i;
    struct hw_pf_info *pf_info = NULL;
    unsigned int aicore = 0;
    unsigned long mem_size = 0;

    pf_info = &dvt->pf[0];
    for (i = 0; vdavinci_types_num[i].types != NULL; i++) {
        aicore = vdavinci_types_num[i].aicore;
        mem_size = vdavinci_types_num[i].mem_size;

        if (dvt->device == vdavinci_types_num[i].device &&
            pf_info->aicore_num == aicore &&
            (mem_size == 0 || pf_info->mem_size == mem_size)) {
            dvt->vdavinci_type_num = vdavinci_types_num[i].size;
            *type = vdavinci_types_num[i].types;
            return 0;
        }
    }
    vascend_warn(dvt->vdavinci_priv->dev,
                 "can not find any match device, "
                 "device id: 0x%x, pf aicore: %u, pf mem_size: %lu",
                 dvt->device, pf_info->aicore_num, pf_info->mem_size);
    return -ENOTSUPP;
}

static unsigned int hw_get_vdavinci_instance_num(struct hw_pf_info *pf_info,
                                                 struct vdavinci_type *tp)
{
    unsigned int aicore_num, aicpu_num, jpegd_num;
    unsigned long mem_size;
    unsigned int num = 0;
    unsigned int left = 0;

    aicore_num = pf_info->aicore_num;
    aicpu_num = pf_info->aicpu_num;
    jpegd_num = pf_info->jpegd_num;
    mem_size = pf_info->mem_size;
    num = aicore_num / tp->aicore_num;
    if (tp->aicpu_num != 0) {
        left = aicpu_num * tp->share / tp->aicpu_num;
        num = min_t(unsigned int, num, left);
    }
    if (tp->jpegd_num != 0) {
        num = min_t(unsigned int, num, jpegd_num / tp->jpegd_num);
    }
    if (tp->mem_size != 0) {
        num = min_t(unsigned int, num, mem_size / tp->mem_size);
    }
    return num;
}

STATIC void get_used_aicpu_num(struct hw_dvt *dvt,
                               unsigned int *numerator,
                               unsigned int *denominator,
                               unsigned int dev_index)
{
    unsigned int i;
    struct hw_vdavinci_type *t = NULL;
    *numerator = 0;
    *denominator = 1;

    for (i = 0; i < dvt->vdavinci_type_num * dvt->dev_num; i++) {
        t = &dvt->types[i];
        if (t->dev_index != dev_index) {
            continue;
        }
        if (t->vf_num != 0) {
            *numerator = t->aicpu_num * (*denominator) * t->vf_num + (*numerator) * t->share;
            *denominator = t->share * (*denominator);
        }
    }
}

unsigned int hw_dvt_get_used_aicpu_num(struct hw_dvt *dvt, unsigned int dev_index)
{
    unsigned int numerator = 0;
    unsigned int denominator = 1;

    get_used_aicpu_num(dvt, &numerator, &denominator, dev_index);
    return DIV_ROUND_UP(numerator, denominator);
}

static unsigned int hw_get_vf_num_of_aicpu(struct hw_dvt *dvt,
                                  unsigned int dev_index,
                                  struct hw_vdavinci_type *tp)
{
    unsigned int numerator = 0;
    unsigned int denominator = 1;
    unsigned int left_aicpu = 0;
    struct hw_pf_info *pf_info = &dvt->pf[dev_index];

    get_used_aicpu_num(dvt, &numerator, &denominator, dev_index);

    left_aicpu = pf_info->aicpu_num * denominator - numerator;

    return left_aicpu * tp->share / (denominator * tp->aicpu_num);
}

/* *
 * hw_dvt_update_vdavinci_types - update converted quantity of device num
 */
void hw_dvt_update_vdavinci_types(struct hw_dvt *dvt, unsigned int dev_index)
{
    unsigned int i;
    unsigned int left = 0;
    struct hw_vdavinci_type *tp = dvt->types + dev_index * dvt->vdavinci_type_num;
    struct hw_pf_info *pf_info = &dvt->pf[dev_index];

    for (i = 0; i < dvt->vdavinci_type_num; i++) {
        tp->avail_instance = pf_info->reserved_aicore_num / tp->aicore_num;
        if (tp->aicpu_num != 0) {
            left = hw_get_vf_num_of_aicpu(dvt, dev_index, tp);
            tp->avail_instance = min_t(unsigned int, tp->avail_instance, left);
        }
        if (tp->jpegd_num != 0) {
            tp->avail_instance = min_t(unsigned int, tp->avail_instance,
                                       pf_info->reserved_jpegd_num / tp->jpegd_num);
        }
        if (tp->mem_size != 0) {
            tp->avail_instance = min_t(unsigned int, tp->avail_instance,
                                       pf_info->reserved_mem_size / tp->mem_size);
        }
        if (tp->avail_instance + pf_info->instance_num > DVT_MAX_VDAVINCI) {
            tp->avail_instance = DVT_MAX_VDAVINCI - pf_info->instance_num;
        }

        tp++;
    }
}

STATIC int hw_dvt_init_vdavinci_type(struct hw_vdavinci_type *type,
                                     struct vdavinci_type *tp,
                                     struct hw_pf_info *pf_info,
                                     unsigned int dev_index)
{
    int ret = 0;
    unsigned int dev_aicore_num, raw_bar4_mem, per_bar4_mem;
 
    dev_aicore_num = pf_info->aicore_num;
    raw_bar4_mem = roundup(DVT_MMIO_BAR4_SIZE, dev_aicore_num) / dev_aicore_num;
    per_bar4_mem = roundup_pow_of_two(raw_bar4_mem);
 
    type->type = tp->type;
    type->dev_index = dev_index;
    type->bar0_size = tp->bar0_size;
    type->bar2_size = tp->bar2_size;
    type->aicore_num = tp->aicore_num;
    if (tp->bar4_size != 0) {
        type->bar4_size = tp->bar4_size;
    } else {
        type->bar4_size = type->aicore_num * per_bar4_mem;
    }
    type->mem_size = tp->mem_size;
    type->aicpu_num = tp->aicpu_num;
    type->vpc_num = tp->vpc_num;
    type->jpegd_num = tp->jpegd_num;
    type->jpege_num = tp->jpege_num;
    type->venc_num = tp->venc_num;
    type->vdec_num = tp->vdec_num;
    type->share = tp->share;
    type->avail_instance = hw_get_vdavinci_instance_num(pf_info, tp);
    type->vf_num = 0;
    if (type->avail_instance > DVT_MAX_VDAVINCI) {
        type->avail_instance = DVT_MAX_VDAVINCI;
    }
 
    ret = snprintf_s(type->template_name, HW_DVT_MAX_TYPE_NAME,
                     HW_DVT_MAX_TYPE_NAME - 1, "%s", tp->template_name);
    if (ret < 0) {
        pr_err("vdavinci type init failed, ret : %d\n", ret);
        return ret;
    }
    if (dev_index == 0) {
        ret = snprintf_s(type->name, HW_DVT_MAX_TYPE_NAME,
                         HW_DVT_MAX_TYPE_NAME - 1, "%s", tp->template_name);
    } else {
        ret = snprintf_s(type->name, HW_DVT_MAX_TYPE_NAME,
                         HW_DVT_MAX_TYPE_NAME - 1, "p%u_%s", dev_index, tp->template_name);
    }
    if (ret < 0) {
        pr_err("vdavinci type init failed, ret : %d\n", ret);
        return ret;
    }
 
    return 0;
}

/* *
 * hw_dvt_init_vdavinci_types - initialize vDavinci type list
 * @dvt : DVT device
 *
 * Initialize vDavinci type list based on available resource.
 *
 */
int hw_dvt_init_vdavinci_types(struct hw_dvt *dvt)
{
    unsigned int i, j;
    int ret = 0, type_index = 0;
    struct hw_pf_info *pf_info;
    unsigned int type_max = 0;
    struct vdavinci_type *tp = NULL;
    struct hw_vdavinci_type *types = NULL;

    ret = hw_get_vdavinci_type(dvt, &tp);
    if (ret) {
        return ret;
    }
    type_max = dvt->vdavinci_type_num;
    if (type_max == 0) {
        return -EINVAL;
    }
    dvt->types = kcalloc(type_max * dvt->dev_num,
        sizeof(struct hw_vdavinci_type), GFP_KERNEL);
    if (dvt->types == NULL) {
        return -ENOMEM;
    }
    for (i = 0; i < dvt->dev_num; i++) {
        pf_info = &dvt->pf[i];
        for (j = 0; j < type_max; j++, type_index++) {
            types = &dvt->types[type_index];
            ret = hw_dvt_init_vdavinci_type(types, &tp[j], pf_info, i);
            if (ret < 0) {
                pr_err("vdavinci type init failed, index : %d, ret : %d\n", type_index, ret);
                kfree(dvt->types);
                dvt->types = NULL;
                return ret;
            }
        }
    }

    return 0;
}

void hw_dvt_clean_vdavinci_types(struct hw_dvt *dvt)
{
    kfree(dvt->types);
    dvt->types = NULL;
}

STATIC struct hw_vdavinci_type *hw_dvt_find_vdavinci_type(struct hw_dvt *dvt,
                                                          const char *name)
{
    unsigned int i;
    struct hw_vdavinci_type *t = NULL;

    if (!hw_vdavinci_is_enabled(dvt)) {
        return NULL;
    }
    for (i = 0; i < dvt->vdavinci_type_num * dvt->dev_num; i++) {
        t = &dvt->types[i];
        if (strncmp(t->name, name + strlen(VDAVINCI_PREFIX),
            sizeof(t->name)) == 0) {
            return t;
        }
    }

    return NULL;
}

int hw_dvt_set_mmio_ops(struct hw_dvt *dvt, struct mmio_init_ops *ops)
{
    int i;

    for (i = 0; ops[i].mmio_init != NULL && ops[i].mmio_uninit != NULL; i++) {
        if (dvt->device == ops[i].device) {
            dvt->mmio_init = ops[i].mmio_init;
            dvt->mmio_uninit = ops[i].mmio_uninit;
            return 0;
        }
    }

    return -ENOTSUPP;
}

bool davinci_vfg_support(unsigned short vendor, unsigned short device)
{
    static struct hw_device_info devices[] = {
        {PCI_VENDOR_ID_HUAWEI, PCI_DEVICE_ID_ASCEND310P}
    };
    int i;

    for (i = 0; i < sizeof(devices) / sizeof(struct hw_device_info); i++) {
        if (vendor == devices[i].vendor && device == devices[i].device) {
            return true;
        }
    }

    return false;
}

STATIC int vdavinci_vfg_stats_cb(struct device *dev, void *data)
{
    struct hw_vdavinci *vdavinci = NULL;
    struct vfg_info *info = data;
    int *stats = info->stats;

    if (!device_is_mdev(dev)) {
        return 0;
    }

    vdavinci = (struct hw_vdavinci *)get_mdev_drvdata(dev);
    if (vdavinci->vfg_id >= VDAVINCI_VFG_MAX) {
        return 0;
    }
    stats[vdavinci->vfg_id]++;

    return 0;
}

STATIC ssize_t davinci_sysfs_vfg_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    int i;
    struct vfg_info info = {{0}};
    int *stats = info.stats;
    ssize_t offset = 0;

    device_for_each_child(dev, &info, vdavinci_vfg_stats_cb);
    for (i = 0; i < VDAVINCI_VFG_MAX; i++) {
        int ret;

        ret = snprintf_s(buf + offset, PAGE_SIZE, PAGE_SIZE - 1,
            "VFG %d: %d\n", i, stats[i]);
        if (ret < 0) {
            return -1;
        }
        offset += ret;
    }
    return offset;
}

static DEVICE_ATTR(vfg_info, S_IRUSR | S_IRGRP, davinci_sysfs_vfg_info, NULL);
static struct attribute *davinci_vfg_attrs[] = {
    &dev_attr_vfg_info.attr,
    NULL,
};
static const struct attribute_group davinci_vfg_attr_group = {
    .attrs = davinci_vfg_attrs,
};
STATIC int davinci_vfg_info_create(struct pci_dev *pcidev)
{
    if (!davinci_vfg_support(pcidev->vendor, pcidev->device)) {
        return 0;
    }
    return sysfs_create_group(&pcidev->dev.kobj, &davinci_vfg_attr_group);
}
STATIC void davinci_vfg_info_remove(struct pci_dev *pcidev)
{
    if (!davinci_vfg_support(pcidev->vendor, pcidev->device)) {
        return;
    }
    sysfs_remove_group(&pcidev->dev.kobj, &davinci_vfg_attr_group);
}

struct hw_vdavinci_ops g_hw_vdavinci_ops = {
    .emulate_cfg_read = hw_vdavinci_emulate_cfg_read,
    .emulate_cfg_write = hw_vdavinci_emulate_cfg_write,
    .emulate_mmio_read = hw_vdavinci_emulate_mmio_read,
    .emulate_mmio_write = hw_vdavinci_emulate_mmio_write,
    .vdavinci_create = hw_dvt_create_vdavinci,
    .vdavinci_destroy = hw_dvt_destroy_vdavinci,
    .vdavinci_release = hw_dvt_release_vdavinci,
    .vdavinci_reset = hw_dvt_reset_vdavinci,
    .vdavinci_activate = hw_dvt_activate_vdavinci,
    .vdavinci_deactivate = hw_dvt_deactivate_vdavinci,
    .dvt_find_vdavinci_type = hw_dvt_find_vdavinci_type,
};

STATIC int hw_dvt_init_device_info(struct hw_dvt *dvt)
{
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;
    struct pci_dev *pdev = container_of(vdavinci_priv->dev, struct pci_dev, dev);
    struct pci_driver *pdrv = pdev->driver;
    int ret = 0;

    dvt->vendor = pdev->vendor;
    dvt->device = pdev->device;
    if (!hw_vdavinci_smmu_check(dvt, vdavinci_priv->dev)) {
        vascend_warn(&pdev->dev, "iommu is not enable, vdavinci is not support for this device");
        return -ENOTSUPP;
    }
    ret = hw_dvt_set_mmio_ops(dvt, vdavinci_mmio_pf_devices_ops);
    if (ret) {
        vascend_warn(&pdev->dev, "vdavinci is not support for this device: 0x%x\n",
                     dvt->device);
        return ret;
    }
 
    ret = hw_dvt_set_mmio_device_info(dvt);
    if (ret != 0) {
        vascend_warn(&pdev->dev, "vdavinci is not support for this device: 0x%x\n",
                     dvt->device);
        return ret;
    }
    pdrv->sriov_configure = NULL;
    if (hw_vdavinci_sriov_support(dvt)) {
        pdrv->sriov_configure = hw_dvt_sriov_enable;
    }
 
    return 0;
}

STATIC void hw_dvt_clean_device_info(struct hw_dvt *dvt)
{
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;
    struct pci_dev *pdev = container_of(vdavinci_priv->dev, struct pci_dev, dev);
    struct pci_driver *pdrv = pdev->driver;

    mutex_destroy(&dvt->lock);
    dvt->mmio_init = NULL;
    pdrv->sriov_configure = NULL;
}

STATIC int hw_dvt_register_mdev(struct hw_dvt *dvt, struct hw_kvmdt_ops *kvmdt_ops)
{
    int ret = 0;
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;

    vascend_info(vdavinci_priv->dev, "enter register mdev\n");

    ret = kvmdt_ops->register_mdev(vdavinci_priv->dev, dvt);
    if (ret) {
        vascend_err(vdavinci_priv->dev, "Failed to register mdev, err: %d\n", ret);
        return ret;
    }

    vascend_info(vdavinci_priv->dev, "leave register mdev\n");

    return 0;
}

STATIC void hw_dvt_unregister_mdev(struct hw_dvt *dvt, struct hw_kvmdt_ops *kvmdt_ops)
{
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;
    kvmdt_ops->unregister_mdev(vdavinci_priv->dev, dvt);
}

STATIC int hw_dvt_vdavinci_getdevinfo(struct hw_dvt *dvt, int dev_index, 
                                      struct dvt_devinfo *resource_info)
{
    int ret;
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;

    ret = vdavinci_priv_vdev_getdevinfo(vdavinci_priv, vdavinci_priv->dev,
                                        dev_index, resource_info);
    if (ret != 0) {
        vascend_err(vdavinci_priv->dev,
                    "Failed to get dev info, pf : %u, reason : %d\n", dev_index, ret);
        return ret;
    }

    return 0;
}

int hw_dvt_init_dev_pf_info(struct hw_dvt *dvt)
{
    int ret;
    unsigned int i, j;
    struct hw_pf_info *pf_info;
    struct dvt_devinfo dev_resource_info;

    for (i = 0; i < dvt->dev_num; i++) {
        pf_info = &dvt->pf[i];
        ret = hw_dvt_vdavinci_getdevinfo(dvt, i, &dev_resource_info);
        if (ret != 0) {
            goto clean_pf_info;
        }
        pf_info->dev_index = i;
        pf_info->reserved_aicore_num = dev_resource_info.aicore_num;
        pf_info->reserved_aicpu_num = dev_resource_info.aicpu_num;
        pf_info->reserved_jpegd_num = dev_resource_info.jpegd_num;
        pf_info->reserved_mem_size = dev_resource_info.mem_size;
        pf_info->instance_num = 0;

        pf_info->aicore_num = dev_resource_info.aicore_num;
        pf_info->mem_size = dev_resource_info.mem_size;
        pf_info->aicpu_num = dev_resource_info.aicpu_num;
        pf_info->vpc_num = dev_resource_info.vpc_num;
        pf_info->jpegd_num = dev_resource_info.jpegd_num;
        pf_info->jpege_num = dev_resource_info.jpege_num;
        pf_info->venc_num = dev_resource_info.venc_num;
        pf_info->vdec_num = dev_resource_info.vdec_num;
        pf_info->ddrmem_size = dev_resource_info.ddrmem_size;
        pf_info->hbmmem_size = dev_resource_info.hbmmem_size;
        idr_init(&pf_info->vdavinci_idr);
        hw_dvt_update_vdavinci_types(dvt, i);
    }

    return 0;

clean_pf_info:
    for (j = 0; j < i; j++) {
        pf_info = &dvt->pf[j];
        pf_info->dev_index = 0;
        pf_info->aicore_num = 0;
        pf_info->mem_size = 0;
        pf_info->aicpu_num = 0;
        pf_info->vpc_num = 0;
        pf_info->jpegd_num = 0;
        pf_info->jpege_num = 0;
        pf_info->venc_num = 0;
        pf_info->vdec_num = 0;
        pf_info->ddrmem_size = 0;
        pf_info->hbmmem_size = 0;
        idr_destroy(&pf_info->vdavinci_idr);
    }
    return ret;
}

void hw_dvt_uninit_dev_pf_info(struct hw_dvt *dvt)
{
    unsigned int i;
    struct hw_pf_info *pf_info;

    for (i = 0; i < dvt->dev_num; i++) {
        pf_info = &dvt->pf[i];
        pf_info->dev_index = 0;
        pf_info->aicore_num = 0;
        pf_info->mem_size = 0;
        pf_info->aicpu_num = 0;
        pf_info->vpc_num = 0;
        pf_info->jpegd_num = 0;
        pf_info->jpege_num = 0;
        pf_info->venc_num = 0;
        pf_info->vdec_num = 0;
        pf_info->ddrmem_size = 0;
        pf_info->hbmmem_size = 0;
        pf_info->reserved_aicore_num = 0;
        pf_info->reserved_aicpu_num = 0;
        pf_info->reserved_jpegd_num = 0;
        pf_info->reserved_mem_size = 0;
        pf_info->instance_num = 0;
        idr_destroy(&pf_info->vdavinci_idr);
    }
}

STATIC int hw_dvt_init_dev_pf_num(struct hw_dvt **dev_dvt,
                                  struct vdavinci_priv *vdavinci_priv)
{
    unsigned int dev_num;
    struct hw_dvt *dvt;

    dev_num = vdavinci_priv_vdev_getdevnum(vdavinci_priv, vdavinci_priv->dev);
    if (dev_num == 0 || dev_num > HW_DVT_MAX_DEV_NUM) {
        vascend_err(vdavinci_priv->dev, "pf num is invalid\n");
        return -EINVAL;
    }

    dvt = kzalloc(sizeof(struct hw_dvt), GFP_KERNEL);
    if (dvt == NULL) {
        return -ENOMEM;
    }

    dvt->dev_num = dev_num;
    *dev_dvt = dvt;

    vdavinci_priv->dvt = dvt;
    dvt->vdavinci_priv = vdavinci_priv;

    return 0;
}

STATIC void hw_dvt_uninit_dev_pf_num(struct hw_dvt *dvt,
                                     struct vdavinci_priv *vdavinci_priv)
{
    if (vdavinci_priv->dvt != dvt) {
        vascend_err(vdavinci_priv->dev, "dvt does not match\n");
        return;
    }
    dvt->dev_num = 0;
    dvt->vdavinci_priv = NULL;
    kfree(vdavinci_priv->dvt);
    vdavinci_priv->dvt = NULL;
}

STATIC int hw_dvt_setup_device(struct hw_dvt *dvt)
{
    int ret = 0;
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;

    mutex_init(&dvt->lock);
    ret = hw_dvt_init_device_info(dvt);
    if (ret != 0) {
        goto out_mutex;
    }

    ret = hw_dvt_init_dev_pf_info(dvt);
    if (ret != 0) {
        goto out_clean_dvt;
    }

    ret = hw_dvt_init_vdavinci_types(dvt);
    if (ret != 0) {
        vascend_warn(vdavinci_priv->dev, "vdavinci might be not support, ret: %d\n", ret);
        goto out_clean_pf;
    }

    return 0;

out_clean_pf:
    hw_dvt_uninit_dev_pf_info(dvt);
out_clean_dvt:
    hw_dvt_clean_device_info(dvt);
out_mutex:
    mutex_destroy(&dvt->lock);
    return ret;
}

STATIC void hw_dvt_cleanup_device(struct hw_dvt *dvt)
{
    hw_dvt_clean_vdavinci_types(dvt);
    hw_dvt_uninit_dev_pf_info(dvt);
    hw_dvt_clean_device_info(dvt);
    mutex_destroy(&dvt->lock);
}

int hw_dvt_init_device(struct vdavinci_priv *vdavinci_priv)
{
    int ret;
    struct hw_dvt *dvt = NULL;

    vascend_info(vdavinci_priv->dev, "enter init device\n");

    if (!try_module_get(THIS_MODULE)) {
        vascend_err(((struct vdavinci_priv *)vdavinci_priv)->dev,
                    "Fail to get module\n");
        return -1;
    }

    ret = hw_dvt_init_dev_pf_num(&dvt, vdavinci_priv);
    if (ret != 0) {
        goto out_module;
    }

    ret = hw_dvt_setup_device(dvt);
    if (ret != 0) {
        goto out_dev_pf;
    }

    ret = hw_dvt_register_mdev(dvt, &g_hw_kvmdt_ops);
    if (ret != 0) {
        vascend_err(vdavinci_priv->dev, "Failed to register hypervisor: %d\n", ret);
        goto out_clenup_dvt;
    }

    hw_dvt_debugfs_init(dvt);
    ret = davinci_vfg_info_create(container_of(vdavinci_priv->dev, struct pci_dev, dev));
    if (ret) {
        vascend_err(vdavinci_priv->dev, "Failed to create vfg_info: %d\n", ret);
        goto out_clean_debugfs;
    }
    vascend_info(vdavinci_priv->dev, "leave init device\n");
    return 0;

out_clean_debugfs:
    hw_dvt_debugfs_clean(dvt);
out_clenup_dvt:
    hw_dvt_cleanup_device(dvt);
out_dev_pf:
    hw_dvt_uninit_dev_pf_num(dvt, vdavinci_priv);
out_module:
    module_put(THIS_MODULE);
    return ret;
}

STATIC void hw_dvt_cleanup_sriov(struct vdavinci_priv *vdavinci_priv)
{
    struct hw_dvt *dvt = NULL;
    struct pci_dev *pdev = NULL;
    bool is_local_locked = false;

    dvt = vdavinci_priv->dvt;

    if (!dvt->is_sriov_enabled) {
        return;
    }

    pdev = container_of(vdavinci_priv->dev, struct pci_dev, dev);
    if (!mutex_is_locked(&pdev->dev.mutex)) {
        device_lock(&pdev->dev);
        is_local_locked = true;
    }

    (void)hw_dvt_sriov_enable(pdev, 0);

    if (is_local_locked) {
        device_unlock(&pdev->dev);
    }
    vascend_warn(vdavinci_priv->dev,
                 "device's sriov is enabled, so disable it before uninit process\n");
}

int hw_dvt_uninit_device(struct vdavinci_priv *vdavinci_priv)
{
    struct hw_dvt *dvt;
    struct pci_dev *pdev = NULL;
    if (!vdavinci_priv) {
        return -EINVAL;
    }

    dvt = vdavinci_priv->dvt;
    if (dvt == NULL) {
        vascend_warn(vdavinci_priv->dev, "vdavinci is not registered\n");
        return 0;
    }

    if (!hw_vdavinci_vf_used_num_zero(dvt)) {
        vascend_err(vdavinci_priv->dev, "vf's num is not zero\n");
        return -EINVAL;
    }

    pdev = container_of(vdavinci_priv->dev, struct pci_dev, dev);
    hw_dvt_cleanup_sriov(vdavinci_priv);
    davinci_vfg_info_remove(pdev);
    hw_dvt_debugfs_clean(dvt);
    hw_dvt_unregister_mdev(dvt, &g_hw_kvmdt_ops);
    hw_dvt_cleanup_device(dvt);
    hw_dvt_uninit_dev_pf_num(dvt, vdavinci_priv);
    module_put(THIS_MODULE);

    vascend_info(vdavinci_priv->dev, "unregister mdev success\n");

    return 0;
}

/* return the vf device which the vdavinci device belong */
struct device *vdavinci_resource_dev(struct hw_vdavinci *vdavinci)
{
    if (vdavinci == NULL) {
        return NULL;
    }

    return vdavinci->dev.resource_dev;
}

/**
 * first, return the vf device otherwise return mdev parent device
 */
struct device *vdavinci_get_device(struct hw_vdavinci *vdavinci)
{
    struct device *dev = vdavinci_resource_dev(vdavinci);
    if (!dev) {
        dev = get_mdev_parent(vdavinci->vdev.mdev);
    }

    return dev;
}

struct device *vdavinci_to_dev(struct hw_vdavinci *vdavinci)
{
    struct device *dev = NULL;

    if (vdavinci == NULL) {
        return NULL;
    }

    dev = vdavinci_resource_dev(vdavinci);
    if (dev) {
        return dev;
    }

    dev = vdavinci->dvt->vdavinci_priv->dev;
    if (dev) {
        return dev;
    }

    return get_mdev_parent(vdavinci->vdev.mdev);
}

bool hw_vdavinci_sriov_support(struct hw_dvt *dvt)
{
    if (dvt->device == PCI_DEVICE_ID_ASCEND910B ||
        dvt->device == PCI_DEVICE_ID_ASCEND910_93 ||
        dvt->device == PCI_DEVICE_ID_ASCEND950) {
        return true;
    }

    return false;
}

STATIC int __init dvt_init(void)
{
    int ret;

    ret = register_vdavinci_virtual_ops(&g_vascend_drv_ops);
    if (ret != 0) {
        return ret;
    }
    ret = vdavinci_register_driver(&hw_vdavinci_mdev_driver);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

STATIC void __exit dvt_exit(void)
{
    vdavinci_unregister_driver(&hw_vdavinci_mdev_driver);
    unregister_vdavinci_virtual_ops();
}

module_init(dvt_init);
module_exit(dvt_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Ascend mediated vNPU host virtualization driver");
MODULE_INFO(supported, "Huawei Ascend Virtualization");
MODULE_VERSION("v0.1");
MODULE_AUTHOR("Huawei Corperation");

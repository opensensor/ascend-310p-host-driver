/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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

#ifndef _DRV_PKICMS_H_
#define _DRV_PKICMS_H_

#include <linux/types.h>

typedef enum {
    VERIFY_TYPE_SOC = 0,
    VERIFY_TYPE_CMS,
    VERIFY_TYPE_MAX
} VERIFY_TYPE;

typedef enum {
    ITEE_IMG_ID = 0,
    DTB_IMG_ID,
    ZIMAGE_ID,
    FS_IMG_ID,
    SD_PEK_DTB_IMG_ID,
    SD_IMG_ID,
    PEK_IMG_ID,
    DP_IMG_ID,
    ROOTFS_IMG_ID,
    APP_IMG_ID,
    DTB_DP_PEK_IMG_ID,
    DTB_SD_PEK_IMG_ID,
    DP_PEK_IMG_ID,
    SD_PEK_IMG_ID,
    DP_CORE_IMG_ID,
    ABL_PATCH_IMG_ID,
    TSFW_PATCH_IMG_ID,
    TSFW_PLUGIN_IMG_ID,
    IMAGE_ID_MAX
} HAL_IMG_ID;

typedef enum {
    SOC_VERIFY_IMG_TSCH_FW = 0,
    SOC_VERIFY_IMG_AICPU_KERNELS,
    SOC_VERIFY_IMG_FFTS_PLUS_FW,
    SOC_VERIFY_IMG_TSCH_FW_PATCH,
    SOC_VERIFY_IMG_TSCH_FW_PLUGIN,
    SOC_VERIFY_MAX
} SOC_VERIFY_IMG_ID;

s32 soc_verify(u32 dev_id, s32 img_id, u8 *image_head_base, u32 size);

#endif /* _DRV_PKICMS_H_ */

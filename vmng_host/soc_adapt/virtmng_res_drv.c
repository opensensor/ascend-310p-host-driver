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

#include "virtmng_res_drv.h"
#include "virtmng_res_mini_v2.h"
#include "virtmng_res_cloud_v2.h"
#include "virtmng_res_cloud_v1.h"
#include "virtmng_public_def.h"
#include "devdrv_interface.h"

#include "ka_base_pub.h"

STATIC int (*vmngh_res_init_func[HISI_CHIP_NUM])(struct vmngh_ctrl_ops *ops) = {
    [HISI_MINI_V1] = NULL,
    [HISI_CLOUD_V1] = vmngh_res_init_cloud_v1,
    [HISI_MINI_V2] = vmngh_res_init_mini_v2,
    [HISI_CLOUD_V2] = vmngh_res_init_cloud_v2,
    [HISI_MINI_V3] = NULL,
};

int vmngh_res_drv_init(u32 dev_id, struct vmngh_ctrl_ops *ops)
{
    u32 chip_type;

    chip_type = devdrv_get_dev_chip_type(dev_id);
    if ((chip_type < HISI_CHIP_NUM) && (vmngh_res_init_func[chip_type] != NULL)) {
        return vmngh_res_init_func[chip_type](ops);
    }

    return VMNG_OK;
}

void vmngh_res_drv_uninit(struct vmngh_ctrl_ops *ops)
{
    if (ops != NULL) {
        (void)memset_s(ops, sizeof(struct vmngh_ctrl_ops), 0, sizeof(struct vmngh_ctrl_ops));
    }
}

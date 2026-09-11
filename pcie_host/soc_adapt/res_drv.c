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
#include "res_drv.h"
#include "res_drv_mini_v1.h"
#include "res_drv_mini_v2.h"
#include "res_drv_cloud_v1.h"
#include "res_drv_cloud_v2.h"
#include "res_drv_mini_v3.h"
#include "res_drv_mdc_v2.h"
#include "devdrv_interface.h"

int (*devdrv_res_init_func[HISI_CHIP_NUM])(struct devdrv_pci_ctrl *pci_ctrl) = {
    [HISI_MINI_V1] = devdrv_mini_v1_res_init,
    [HISI_CLOUD_V1] = devdrv_cloud_v1_res_init,
    [HISI_MINI_V2] = devdrv_mini_v2_res_init,
    [HISI_CLOUD_V2] = devdrv_cloud_v2_res_init,
    [HISI_MINI_V3] = devdrv_mini_v3_res_init,
    [HISI_MDC_V2] = devdrv_mdc_v2_res_init,
};

int devdrv_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret = -EINVAL;

    if (pci_ctrl->chip_type < HISI_CHIP_NUM) {
        ret = devdrv_res_init_func[pci_ctrl->chip_type](pci_ctrl);
    }

    return ret;
}

void devdrv_res_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para = NULL;

    if (pci_ctrl->io_base != NULL) {
        iounmap(pci_ctrl->io_base);
        pci_ctrl->io_base = NULL;
    }

    if (pci_ctrl->msi_base != NULL) {
        iounmap(pci_ctrl->msi_base);
        pci_ctrl->msi_base = NULL;
    }

    if (pci_ctrl->mem_base != NULL) {
        iounmap(pci_ctrl->mem_base);
        pci_ctrl->mem_base = NULL;
    }

    if (pci_ctrl->local_reserve_mem_base != NULL) {
        iounmap(pci_ctrl->local_reserve_mem_base);
        pci_ctrl->local_reserve_mem_base = NULL;
    }
}

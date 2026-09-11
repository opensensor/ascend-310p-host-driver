/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-10-12
*/
#include "devdrv_pm.h"
#include "dms_sensor_init.h"
#include "dms_define.h"
#include "dbl/feature_loader.h"

static struct devdrv_pm *g_smf_pm = NULL;
int dms_pm_register(void)
{
    g_smf_pm = devdrv_manager_register_pm(0, dms_smf_suspend, dms_smf_resume);
    if (g_smf_pm == NULL) {
        dms_warn("smf register pm not OK.\n");
    } else {
        dms_info("smf register pm success.\n");
    }
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_pm_register, FEATURE_LOADER_STAGE_5);

void dms_pm_unregister(void)
{
    if (g_smf_pm != NULL) {
        devdrv_manager_unregister_pm(g_smf_pm);
        g_smf_pm = NULL;
    }
    return;
}
DECLAER_FEATURE_AUTO_UNINIT(dms_pm_unregister, FEATURE_LOADER_STAGE_5);
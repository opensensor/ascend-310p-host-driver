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
#include "dms_power.h"
#include "dms_hotreset.h"
#include "dms_pm_chain.h"

int dms_power_init(void)
{
    int ret;

#ifdef CFG_FEATURE_HOTRESET
    ret = dms_power_hotreset_init();
    if (ret != 0) {
        dms_err("Dms event hotreset event init failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

#ifdef AOS_LLVM_BUILD
    ret = dms_pm_chain_init();
    if (ret != 0) {
        dms_err("Dms power init failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

    dms_debug("Dms power init success.\n");
    return 0;
}

void dms_power_exit(void)
{
#ifdef CFG_FEATURE_HOTRESET
    dms_power_hotreset_exit();
#endif
    dms_debug("Dms power exit success.\n");
}

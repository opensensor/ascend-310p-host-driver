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
 * Create: 2022-07-31
 */

#include <linux/io.h>
#include <linux/module.h>

#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"
#include "dbl/runenv_config.h"
#include "runenv_config_module.h"
#include "runenv_device.h"

/* The name cannot be changed because it is exposed to the user. */
static u32 run_mode = DBL_RC_MODE;
ka_module_param(run_mode, uint, KA_S_IRUSR);

int dbl_set_rc_ep_mode(u32 mode)
{
    if (mode == DBL_EP_MODE || mode == DBL_RC_MODE) {
        recfg_info("Set RC/EP mode succ. (mode=%u)\n", mode);
        run_mode = mode;
    } else {
        recfg_err("Invalid param RC/EP mode. (mode=%u)\n", mode);
        return -EINVAL;
    }

    return 0;
}
KA_EXPORT_SYMBOL(dbl_set_rc_ep_mode);

u32 dbl_get_rc_ep_mode(void)
{
    return run_mode;
}
KA_EXPORT_SYMBOL(dbl_get_rc_ep_mode);

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
 *
 * Description:
 * Author: huawei
 * Create: 2025-05-26
 */

#include <linux/rwsem.h>

#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#include "dbl/runenv_config.h"
#include "runenv_config_module.h"
#include "ascend_platform.h"

static unsigned int g_cust_op_enhance_mode[ASCEND_DEVICE_DAVINCI_NUM_MAX] = { DBL_CUST_OP_ENHANCE_DISABLE };
static KA_TASK_DEFINE_RWLOCK(g_cust_op_enhance_lock);

int dbl_get_cust_op_enhance_mode(unsigned int udevid,  unsigned int *mode)
{
    if ((udevid >= ASCEND_DEVICE_DAVINCI_NUM_MAX) || (mode == NULL)) {
        return -EINVAL;
    }

    ka_task_read_lock(&g_cust_op_enhance_lock);
    *mode = g_cust_op_enhance_mode[udevid];
    ka_task_read_unlock(&g_cust_op_enhance_lock);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(dbl_get_cust_op_enhance_mode);

int dbl_set_cust_op_enhance_mode(unsigned int udevid, unsigned int mode)
{
    if (udevid >= ASCEND_DEVICE_DAVINCI_NUM_MAX) {
        recfg_err("Invalid parameter. (udevid=%u)", udevid);
        return -EINVAL;
    }

    if ((mode != DBL_CUST_OP_ENHANCE_DISABLE) && (mode != DBL_CUST_OP_ENHANCE_ENABLE)) {
        recfg_err("Invalid parameter. (set_mode=%u)", mode);
        return -EINVAL;
    }

    ka_task_write_lock(&g_cust_op_enhance_lock);
    g_cust_op_enhance_mode[udevid] = mode;
    ka_task_write_unlock(&g_cust_op_enhance_lock);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dbl_set_cust_op_enhance_mode);

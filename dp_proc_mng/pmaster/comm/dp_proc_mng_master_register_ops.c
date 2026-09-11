/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */
#include <linux/fs.h>
#include <linux/slab.h>

#include "dp_proc_mng_mem_sharing.h"
#include "dp_proc_mng_register_ops.h"

int dp_proc_mng_register_ops_init(void)
{
    return dp_mem_mng_init();
}

void dp_proc_mng_unregister_ops_init(void)
{
    dp_mem_mng_uninit();
}

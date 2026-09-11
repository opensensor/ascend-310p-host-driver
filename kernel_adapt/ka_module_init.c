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
 * Create: 2024-11-11
 */

#ifndef EMU_ST

#include <linux/module.h>
#include <linux/version.h>
#include "kernel_adapt_init.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 19, 25)
STATIC int __init ka_module_init(void)
#else
STATIC int ka_module_init(void)
#endif
{
    return 0;
}


#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 19, 25)
STATIC void __exit ka_module_exit(void)
#else
STATIC void ka_module_exit(void)
#endif
{
}
module_init(ka_module_init);
module_exit(ka_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kernel open adapt module");

#else

int ka_module_init(void)
{
    return 0;
}

void ka_module_exit(void)
{
    return;
}

#endif

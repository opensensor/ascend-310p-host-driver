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
* Create: 2022-05-06
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include "dms_define.h"

#include "ka_kernel_def_pub.h"

#ifdef CFG_FEATURE_EMMC_INFO
#  include "dms_emmc_info.h"
#endif

STATIC int __init dms_mng_init(void)
{
    dms_info("dms_mng init successfully.\n");
#ifdef CFG_FEATURE_EMMC_INFO
    (void)dms_emmc_init();
#endif
    return 0;
}

STATIC void __exit dms_mng_exit(void)
{
    dms_info("dms_mng exit successfully.\n");
#ifdef CFG_FEATURE_EMMC_INFO
    dms_emmc_uninit();
#endif
}

module_init(dms_mng_init);
module_exit(dms_mng_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI DMS Manager driver");

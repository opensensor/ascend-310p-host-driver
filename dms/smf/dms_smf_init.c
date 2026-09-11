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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>

#include "ka_kernel_def_pub.h"

#include "dms_interface.h"
#include "dms_sensor_init.h"
#include "dms_sensor.h"
#include "dms_event.h"
#include "dms_define.h"

STATIC int __init dms_smf_init(void)
{
    (void)dms_event_init();
    dms_sensor_init();
    return 0;
}

STATIC void __exit dms_smf_exit(void)
{
    (void)dms_sensor_exit();
    dms_event_exit();
    (void)dms_sen_exit_sensor_event();
}

module_init(dms_smf_init);
module_exit(dms_smf_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");

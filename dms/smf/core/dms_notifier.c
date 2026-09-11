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

#include <linux/export.h>

#include "dms_notifier.h"
#include "ka_kernel_def_pub.h"


static RAW_NOTIFIER_HEAD(g_dms_chain);

int dms_notifyer_call(u64 mode, void* v)
{
    return raw_notifier_call_chain(&g_dms_chain, mode, v);
}
KA_EXPORT_SYMBOL(dms_notifyer_call);

int dms_register_notifier(struct notifier_block* nb)
{
    return raw_notifier_chain_register(&g_dms_chain, nb);
}
KA_EXPORT_SYMBOL(dms_register_notifier);

int dms_unregister_notifier(struct notifier_block* nb)
{
    return raw_notifier_chain_unregister(&g_dms_chain, nb);
}
KA_EXPORT_SYMBOL(dms_unregister_notifier);


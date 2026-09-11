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

#include "urd_notifier.h"
#include "ka_kernel_def_pub.h"
/*
 * URD interface notification chain, which is used to
 * notify other modules of user process startup and exit events
 */
static RAW_NOTIFIER_HEAD(g_urd_notifier_chain);

int urd_notifier_call(u64 mode, void* data)
{
    return ka_dfx_raw_notifier_call_chain(&g_urd_notifier_chain, mode, data);
}

int urd_register_notifier(struct notifier_block* nb)
{
    return ka_dfx_raw_notifier_chain_register(&g_urd_notifier_chain, nb);
}
KA_EXPORT_SYMBOL_GPL(urd_register_notifier);

int urd_unregister_notifier(struct notifier_block* nb)
{
    return ka_dfx_raw_notifier_chain_unregister(&g_urd_notifier_chain, nb);
}
KA_EXPORT_SYMBOL_GPL(urd_unregister_notifier);


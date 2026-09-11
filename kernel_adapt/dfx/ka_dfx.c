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

#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
#include <linux/kallsyms.h>
#endif

#include "ka_dfx_pub.h"
#include "ka_define.h"
#include "ka_dfx.h"

int ka_dfx_vprintk_emit(int facility, int level, const char *fmt, va_list args)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
    return vprintk_emit(facility, level, NULL, fmt, args);
#else
    return vprintk_emit(facility, level, NULL, 0, fmt, args);
#endif
}
EXPORT_SYMBOL(ka_dfx_vprintk_emit);

unsigned long ka_dfx_kallsyms_lookup_name(const char *name)
{
    unsigned long symbol = 0;

    if (name == NULL) {
        return 0;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
    symbol = (unsigned long)__symbol_get(name);
    if (symbol == 0) {
        return 0;
    }
    __symbol_put(name);
#else
    symbol = kallsyms_lookup_name(name);
#endif
    return symbol;
}
EXPORT_SYMBOL(ka_dfx_kallsyms_lookup_name);


int ka_dfx_atomic_notifier_panic_chain_register(ka_notifier_block_t *nb)
{
    return atomic_notifier_chain_register(&panic_notifier_list, nb);
}
EXPORT_SYMBOL(ka_dfx_atomic_notifier_panic_chain_register);


void ka_dfx_atomic_notifier_panic_chain_unregister(ka_notifier_block_t *nb)
{
    atomic_notifier_chain_unregister(&panic_notifier_list, nb);
}
EXPORT_SYMBOL(ka_dfx_atomic_notifier_panic_chain_unregister);
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
#include <linux/random.h>
#include "securec.h"
#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_base.h"


unsigned short const *ka_base_get_crc16_table(void)
{
    return crc16_table;
}
EXPORT_SYMBOL_GPL(ka_base_get_crc16_table);

unsigned char const *_ka_base_get_ctype(void)
{
    return _ctype;
}
EXPORT_SYMBOL_GPL(_ka_base_get_ctype);

void *ka_base_pde_data(const ka_inode_t *inode)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0))
    return pde_data(inode);
#else
    return PDE_DATA(inode);
#endif
}
EXPORT_SYMBOL_GPL(ka_base_pde_data);

u32 ka_base_get_random_u32(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
    return get_random_u32();
#else
    return get_random_int();
#endif
}
EXPORT_SYMBOL_GPL(ka_base_get_random_u32);
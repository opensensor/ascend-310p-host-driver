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

#include "securec.h"
#include "ka_driver_pub.h"

int ka_driver_get_acpi_disabled(void)
{
    return acpi_disabled;
}
EXPORT_SYMBOL_GPL(ka_driver_get_acpi_disabled);

#ifndef __cplusplus
ka_class_t *ka_driver_class_create(ka_module_t *owner, const char *name)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    return class_create(name);
#else
    return class_create(owner, name);
#endif
}
EXPORT_SYMBOL_GPL(ka_driver_class_create);

#ifndef EMU_ST
typedef char* (*ka_class_devnode_const)(const struct device *dev, umode_t *mode); // typedef function pointer
int ka_driver_class_set_devnode(ka_class_t *cls, ka_class_devnode devnode)
{
    if (cls == NULL || devnode == NULL) {
        return -EINVAL;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
    cls->devnode = (ka_class_devnode_const)(devnode);
#else
    cls->devnode = devnode;
#endif
    return 0;
}
EXPORT_SYMBOL_GPL(ka_driver_class_set_devnode);
#endif

#endif
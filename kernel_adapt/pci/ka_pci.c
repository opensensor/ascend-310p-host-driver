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

#include <linux/pci.h>
#include "ka_pci_pub.h"
#include "ka_define.h"

ka_bus_type_t *ka_pci_get_bus_type(void)
{
    return &pci_bus_type;
}
EXPORT_SYMBOL_GPL(ka_pci_get_bus_type);

unsigned char ka_pci_get_bus_number(ka_pci_dev_t *pdev)
{
    return pdev->bus->number;
}
EXPORT_SYMBOL_GPL(ka_pci_get_bus_number);

unsigned short ka_pci_get_vendor_id(ka_pci_dev_t *pdev)
{
    return pdev->vendor;
}
EXPORT_SYMBOL_GPL(ka_pci_get_vendor_id);

unsigned short ka_pci_get_device_id(ka_pci_dev_t *pdev)
{
    return pdev->device;
}
EXPORT_SYMBOL_GPL(ka_pci_get_device_id);

unsigned short ka_pci_get_subsystem_vendor_id(ka_pci_dev_t *pdev)
{
    return pdev->subsystem_vendor;
}
EXPORT_SYMBOL_GPL(ka_pci_get_subsystem_vendor_id);

unsigned short ka_pci_get_subsystem_device_id(ka_pci_dev_t *pdev)
{
    return pdev->subsystem_device;
}
EXPORT_SYMBOL_GPL(ka_pci_get_subsystem_device_id);

void ka_pci_irq_vector(ka_pci_dev_t *pdev, u32 entry, u32 devid, u32 *irq,
                       int (*irq_vector_func)(u32, u32, unsigned int *))
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    *irq = pci_irq_vector(pdev, entry);
#else
    (void)irq_vector_func(devid, entry, irq);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_irq_vector);

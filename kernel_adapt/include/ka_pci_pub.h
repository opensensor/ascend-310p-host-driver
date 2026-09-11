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
#ifndef KA_PCI_PUB_H
#define KA_PCI_PUB_H

#include <linux/fs.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/version.h>

#include "ka_macro_pub.h"
#include "ka_common_pub.h"

#define ka_pci_power_t      pci_power_t
#define KA_PCI_D0            PCI_D0
#define KA_PCI_D1            PCI_D1
#define KA_PCI_D2            PCI_D2
#define KA_PCI_D3hot         PCI_D3hot
#define KA_PCI_D3cold        PCI_D3cold
#define KA_PCI_UNKNOWN       PCI_UNKNOWN
#define KA_PCI_POWER_ERROR   PCI_POWER_ERROR

typedef struct bus_type ka_bus_type_t;
typedef struct msix_entry ka_msix_entry_t;
typedef struct irq_affinity ka_irq_affinity_t;
typedef struct pci_bus ka_pci_bus_t;
typedef struct pci_device_id ka_pci_device_id_t;
typedef struct pci_driver ka_pci_driver_t;

#define ka_pci_init_pd_name(pd_name) \
    .name = pd_name,
#define ka_pci_init_pd_id_table(pd_id_table) \
    .id_table = pd_id_table,
#define ka_pci_init_pd_probe(pd_probe) \
    .probe = pd_probe,
#define ka_pci_init_pd_remove(pd_remove) \
    .remove = pd_remove,
#define ka_pci_init_pd_driver(pd_name, pd_pm) \
    .driver = { \
        .name = pd_name, \
        .pm = pd_pm, \
    },
#define ka_pci_init_pd_err_handler(pd_err_handler) \
    .err_handler = pd_err_handler,
#define ka_pci_init_pd_shutdown(pd_shutdown) \
    .shutdown = pd_shutdown,

#define ka_pci_disable_msi(dev)    pci_disable_msi(dev)
#define ka_pci_disable_msix(dev)    pci_disable_msix(dev)
#define ka_pci_get_domain_bus_and_slot(domain, bus, devfn)    pci_get_domain_bus_and_slot(domain, bus, devfn)
#define ka_pci_get_device(vendor, device, from)    pci_get_device(vendor, device, from)
#define ka_pci_find_capability(dev, cap)    pci_find_capability(dev, cap)
#define ka_pci_bus_find_capability(bus, devfn, cap)    pci_bus_find_capability(bus, devfn, cap)
#define ka_pci_set_power_state(dev, state)    pci_set_power_state(dev, state)
#define ka_pci_save_state(dev)    pci_save_state(dev)
#define ka_pci_restore_state(dev)    pci_restore_state(dev)
#define ka_pci_enable_device_io(dev)    pci_enable_device_io(dev)
#define ka_pci_enable_device_mem(dev)    pci_enable_device_mem(dev)
#define ka_pci_enable_device(dev)    pci_enable_device(dev)
#define ka_pci_disable_device(dev)    pci_disable_device(dev)
#define ka_pci_request_region(pdev, bar, res_name)    pci_request_region(pdev, bar, res_name)
#define ka_pci_request_selected_regions(pdev, bars, res_name)    pci_request_selected_regions(pdev, bars, res_name)
#define ka_pci_release_selected_regions(pdev, bars)    pci_release_selected_regions(pdev, bars)
#define ka_pci_request_selected_regions_exclusive(pdev, bars, res_name)   \
            pci_request_selected_regions_exclusive(pdev, bars, res_name)
#define ka_pci_release_regions(pdev)    pci_release_regions(pdev)
#define ka_pci_request_regions(pdev, res_name)    pci_request_regions(pdev, res_name)
#define ka_pci_request_regions_exclusive(pdev, res_name)    pci_request_regions_exclusive(pdev, res_name)
#define ka_pci_set_master(dev)    pci_set_master(dev)
#define ka_pci_clear_master(dev)    pci_clear_master(dev)
#define ka_pci_select_bars(dev, flags)    pci_select_bars(dev, flags)
#define ka_pci_match_id(ids, dev)    pci_match_id(ids, dev)
#define ka_pci_resource_len(dev, bar) pci_resource_len(dev, bar)
#define ka_pci_resource_start(dev, bar) pci_resource_start(dev, bar)
#define ka_pci_register_driver(drv)    pci_register_driver(drv)
#define ka_pci_unregister_driver(drv)    pci_unregister_driver(drv)
#define ka_pci_dev_put(dev)    pci_dev_put(dev)
#define ka_pci_bus_read_config_word(bus, devfn, where, val)    pci_bus_read_config_word(bus, devfn, where, val)
#define ka_pci_bus_read_config_dword(bus, devfn, where, val)    pci_bus_read_config_dword(bus, devfn, where, val)
#define ka_pci_read_config_byte(dev, where, val)    pci_read_config_byte(dev, where, val)
#define ka_pci_read_config_word(dev, where, val)    pci_read_config_word(dev, where, val)
#define ka_pci_read_config_dword(dev, where, val)    pci_read_config_dword(dev, where, val)
#define ka_pci_write_config_word(dev, where, val)    pci_write_config_word(dev, where, val)
#define ka_pci_write_config_dword(dev, where, val)    pci_write_config_dword(dev, where, val)
#define ka_pci_set_drvdata(pdev, data)    pci_set_drvdata(pdev, data)
#define ka_pci_get_drvdata(pdev)    pci_get_drvdata(pdev)
#define ka_pci_sriov_get_totalvfs(pdev)    pci_sriov_get_totalvfs(pdev)
#define ka_pci_to_pci_dev(dev)    to_pci_dev(dev)
#define ka_pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs, flags, affd)  \
            pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs, flags, affd)
#define ka_pci_free_irq_vectors(dev)    pci_free_irq_vectors(dev)
#define ka_pci_enable_msix_range(dev, entries, minvec, maxvec)    pci_enable_msix_range(dev, entries, minvec, maxvec)

ka_bus_type_t *ka_pci_get_bus_type(void);
unsigned char ka_pci_get_bus_number(ka_pci_dev_t *pdev);
#define KA_PCI_SLOT(devfn) PCI_SLOT(devfn)
#define KA_PCI_FUNC(devfn) PCI_FUNC(devfn)
unsigned short ka_pci_get_vendor_id(ka_pci_dev_t *pdev);
unsigned short ka_pci_get_device_id(ka_pci_dev_t *pdev);
unsigned short ka_pci_get_subsystem_vendor_id(ka_pci_dev_t *pdev);
unsigned short ka_pci_get_subsystem_device_id(ka_pci_dev_t *pdev);

void ka_pci_irq_vector(ka_pci_dev_t *pdev, u32 entry, u32 devid, u32 *irq,
                       int (*irq_vector_func)(u32, u32, unsigned int *));
#endif

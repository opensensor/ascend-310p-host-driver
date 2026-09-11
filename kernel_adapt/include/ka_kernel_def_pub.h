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

#ifndef KA_KERNEL_DEF_PUB_H
#define KA_KERNEL_DEF_PUB_H

#include <linux/pci.h>
#include <linux/module.h>

#define KA_PCI_VDEVICE(vend, dev)    PCI_VDEVICE(vend, dev)
#define KA_PCI_DEVICE(vend,dev)      PCI_DEVICE(vend,dev)

#define KA_PCI_ANY_ID PCI_ANY_ID

#define KA_THIS_MODULE                          THIS_MODULE
#define ka_module_init(x)                       module_init(x)
#define ka_module_exit(x)                       module_exit(x)
#define ka_module_param(name, type, perm)       module_param(name, type, perm)
#define ka_module_param_array(name, type, nump, perm) module_param_array(name, type, nump, perm)
#define KA_MODULE_PARM_DESC(_parm, desc)        MODULE_PARM_DESC(_parm, desc)
#define KA_MODULE_LICENSE(_license)             MODULE_LICENSE(_license)
#define KA_MODULE_DESCRIPTION(_description)     MODULE_DESCRIPTION(_description)
#define KA_MODULE_DEVICE_TABLE(type, name)	    MODULE_DEVICE_TABLE(type, name)
#define KA_MODULE_SOFTDEP(_softdep)             MODULE_SOFTDEP(_softdep)
#define KA_MODULE_AUTHOR(_author)               MODULE_AUTHOR(_author)
#define KA_MODULE_VERSION(_version)             MODULE_VERSION(_version)
#define KA_MODULE_INFO(tag, info)               MODULE_INFO(tag, info)

#define KA_EXPORT_SYMBOL(sym)                   EXPORT_SYMBOL(sym)
#define KA_EXPORT_SYMBOL_GPL(sym)               EXPORT_SYMBOL_GPL(sym)

#endif
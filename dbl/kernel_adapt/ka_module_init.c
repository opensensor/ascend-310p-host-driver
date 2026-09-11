/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2024-7-20
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/pci.h>

#include "ka_memory_mng.h"
#include "ka_proc_fs.h"
#include "event_notify_proc.h"
#include "ka_module_init.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kernel adapt module");

#define PCI_VENDOR_ID_HUAWEI            0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
static const struct pci_device_id ka_device_tbl[] = {
    { PCI_VDEVICE(HUAWEI, 0xd100), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd105), 0 },
    { PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd801), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd500), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd501), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd802), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd803), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd804), 0 },
    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    {}
};
MODULE_DEVICE_TABLE(pci, ka_device_tbl);

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC int __init ka_module_init(void)
{
    int ret = 0;

    ka_mem_mng_init();
    ka_proc_fs_init();

    ret = event_notify_proc_init();
    if (ret != 0) {
        ka_err("Event notify init failed. (ret=%d)", ret);
        goto TRACE_SCHED_FAILED;
    }
    return 0;

TRACE_SCHED_FAILED:
    ka_proc_fs_uninit();
    ka_mem_mng_uninit();
    return ret;
}

STATIC void __exit ka_module_exit(void)
{
    event_notify_proc_uninit();
    ka_proc_fs_uninit();
    ka_mem_mng_uninit();
}
module_init(ka_module_init);
module_exit(ka_module_exit);


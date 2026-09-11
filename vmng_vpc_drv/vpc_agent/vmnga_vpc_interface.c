/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-12-13
*/
#include "virtmngagent_vpc_unit.h"
#include "virtmngagent_msg.h"
#include "virtmng_public_def.h"
#include "virtmngagent_msg_common.h"
#include "vmng_mem_alloc_interface.h"
#include "vpc_interface.h"

#include "ka_memory_pub.h"
#include "ka_barrier_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "ka_dfx_pub.h"

int vpc_register_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmnga_vpc_register_client(dev_id, vpc_client);
}
KA_EXPORT_SYMBOL(vpc_register_client);

int vpc_register_client_safety(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmnga_vpc_register_client(dev_id, vpc_client);
}
KA_EXPORT_SYMBOL(vpc_register_client_safety);

int vpc_unregister_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmnga_vpc_unregister_client(dev_id, vpc_client);
}
KA_EXPORT_SYMBOL(vpc_unregister_client);

int vpc_msg_send(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info,
    u32 timeout)
{
    return vmnga_vpc_msg_send(dev_id, vpc_type, tx_info, timeout);
}
KA_EXPORT_SYMBOL(vpc_msg_send);

int vpc_register_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    return vmnga_register_common_msg_client(dev_id, msg_client);
}
KA_EXPORT_SYMBOL(vpc_register_common_msg_client);

int vpc_unregister_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    return vmnga_unregister_common_msg_client(dev_id, msg_client);
}
KA_EXPORT_SYMBOL(vpc_unregister_common_msg_client);

int vpc_common_msg_send(u32 dev_id, u32 fid, enum vmng_msg_common_type cmn_type,
                        struct vmng_tx_msg_proc_info *tx_info)
{
    return vmnga_common_msg_send(dev_id, cmn_type, tx_info);
}
KA_EXPORT_SYMBOL(vpc_common_msg_send);

#define VMNG_DB_MEM_BAR_SIZE 0x20000
#define VMNG_DB_MEM_BAR_OFFSET (VMNG_DB_MEM_BAR_SIZE)
STATIC int vmnga_map_bar_va_cloud_v2(const struct vmng_vpc_unit *unit, struct vmnga_vpc_unit *vpc_unit)
{
    ka_pci_dev_t *pdev = unit->pdev;

    vpc_unit->db_base = ka_mm_ioremap(unit->mmio.bar0_base, VMNG_DB_MEM_BAR_SIZE);
    if (vpc_unit->db_base == NULL) {
        ka_dfx_pr_err("Ioremap db_base failed. (bdf=%02x:%02x.%d)\n", ka_pci_get_bus_number(pdev),
            KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        goto DB_ERR;
    }
    vpc_unit->shr_para = ka_mm_ioremap(unit->mmio.bar0_base + VMNG_SHR_PARA_ADDR_BASE + VMNG_DB_MEM_BAR_OFFSET,
                                 VMNG_SHR_PARA_ADDR_SIZE);
    if (vpc_unit->shr_para == NULL) {
        ka_dfx_pr_err("Ioremap shr failed. (bdf=%02x:%02x.%d)\n", ka_pci_get_bus_number(pdev),
            KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        goto SHR_ERR;
    }
    vpc_unit->msg_base = ka_mm_ioremap(unit->mmio.bar0_base + VMNG_MSG_ADDR_BASE + VMNG_DB_MEM_BAR_OFFSET,
                                 VMNG_MSG_ADDR_SIZE);
    if (vpc_unit->msg_base == NULL) {
        ka_dfx_pr_err("Ioremap msg base failed. (bdf=%02x:%02x.%d)\n", ka_pci_get_bus_number(pdev),
            KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        goto MSG_ERRR;
    }

    return 0;

MSG_ERRR:
    ka_mm_iounmap(vpc_unit->shr_para);
    vpc_unit->shr_para = NULL;
SHR_ERR:
    ka_mm_iounmap(vpc_unit->db_base);
    vpc_unit->db_base = NULL;
DB_ERR:
    return -ENOMEM;
}

STATIC void vmnga_unmap_bar_va_cloud_v2(struct vmnga_vpc_unit *vpc_unit)
{
    ka_mm_iounmap(vpc_unit->msg_base);
    vpc_unit->msg_base = NULL;
    ka_mm_iounmap(vpc_unit->shr_para);
    vpc_unit->shr_para = NULL;
    ka_mm_iounmap(vpc_unit->db_base);
    vpc_unit->db_base = NULL;
}

STATIC ka_irqreturn_t vmnga_vpc_start_irq(int irq, void *data)
{
    struct vmnga_vpc_unit *unit = data;

    if (data == NULL) {
        vmng_err("Input parameter is error. (irq=%d)\n", irq);
        return KA_IRQ_NONE; /* KA_IRQ_NONE: interrupt was not from this device or was handled */
    }
    ka_base_atomic_set(&unit->start_dev.start_flag, VMNG_TASK_SUCCESS);
    ka_task_wake_up_interruptible(&unit->start_dev.wq);
    return KA_IRQ_HANDLED;
}

#define VMNGA_START_FB_TIMEOUT_MS 1000
#define GB_UNIT (1024 * 1024 * 1024)
#define MIN_MEM_SIZE 50

STATIC int vpc_top_half_init_finish(struct vmnga_vpc_unit *vpc_unit)
{
    struct vmnga_vpc_start_dev *start_dev = &vpc_unit->start_dev;
    ka_sysinfo_t mem_info;
    u64 mem_size;
    u32 time;
    int ret;

    start_dev->msix_id = VMNG_MSIX_BASE_LOAD;
    ret = vmnga_register_vpc_irq_func((void *)vpc_unit, start_dev->msix_id, vmnga_vpc_start_irq,
                                      (void *)vpc_unit, "vmnga_start");
    if (ret != 0) {
        vmng_err("register vpc start irq failed. (ret=%d)\n", ret);
        return ret;
    }

    /* prepare for wait */
    start_dev->db_id = VMNG_DB_BASE_LOAD;
    ka_task_init_waitqueue_head(&start_dev->wq);

    /* set bar shr para, then ring doorbell. */
    vpc_unit->shr_para->start_flag = VMNG_VM_START_WAIT;
    ka_wmb();
    /* notify srv */
    vmnga_set_doorbell(vpc_unit->db_base, start_dev->db_id, 1);

    vmnga_register_extended_common_msg_client(vpc_unit->msg_dev);

    ka_si_meminfo(&mem_info);
    mem_size = mem_info.totalram * KA_MM_PAGE_SIZE / GB_UNIT + 1;
    if (mem_size < MIN_MEM_SIZE) {
        mem_size = MIN_MEM_SIZE;
    }
    vmng_info("Start wait begin. (dev_id=%u; size=%llu)\n", vpc_unit->dev_id, mem_size);
    time = (u32)ka_system_msecs_to_jiffies((unsigned int)(VMNGA_START_FB_TIMEOUT_MS * mem_size));
    ret = (int)ka_task_wait_event_interruptible_timeout(start_dev->wq,
        (ka_base_atomic_read(&start_dev->start_flag) == VMNG_TASK_SUCCESS), time);
    if (ret <= 0) {
        vmng_err("Wait host time out. (dev_id=%u;ret=%d)\n", vpc_unit->dev_id, ret);
        vmnga_unregister_extended_common_msg_client(vpc_unit->msg_dev);
        (void)vmnga_unregister_vpc_irq_func((void *)vpc_unit, start_dev->msix_id, (void *)vpc_unit);
        return -ETIMEDOUT;
    }
    vmng_info("Wait host start ok, (dev_id=%u;time_remain=%d)\n", vpc_unit->dev_id, ret);
    return 0;
}

STATIC void vpc_top_half_free(struct vmnga_vpc_unit *vpc_unit)
{
    struct vmnga_vpc_start_dev *start_dev = &vpc_unit->start_dev;

    vmnga_unregister_extended_common_msg_client(vpc_unit->msg_dev);
    (void)vmnga_unregister_vpc_irq_func((void *)vpc_unit, start_dev->msix_id, (void *)vpc_unit);
}

int vmng_vpc_init(struct vmng_vpc_unit *unit_in, int server_type)
{
    struct vmng_vpc_unit *unit = (struct vmng_vpc_unit *)unit_in;
    struct vmnga_vpc_unit *vpc_unit;
    int ret;

    if (server_type == SERVER_TYPE_VM_PCIE) {
        vpc_unit = vmng_kzalloc(sizeof(struct vmnga_vpc_unit), KA_GFP_KERNEL);
        if (vpc_unit == NULL) {
            vmng_err("vmng_kzalloc vpc unit failed.\n");
            return -ENOMEM;
        }
        vpc_unit->dev_id = unit->dev_id;
        vpc_unit->fid = 0;
        ret = vmnga_map_bar_va_cloud_v2(unit, vpc_unit);
        if (ret != 0) {
            vmng_err("Get bar va failed. (ret=%d)\n", ret);
            vmng_kfree(vpc_unit);
            return ret;
        }

        vpc_unit->pdev = unit->pdev;
        ret = memcpy_s(vpc_unit->msix_ctrl.entries, sizeof(ka_msix_entry_t) * VIRTMNGAGENT_MSIX_MAX,
            unit->msix_info.entries + unit->msix_info.msix_irq_offset,
            sizeof(ka_msix_entry_t) * unit->msix_info.msix_irq_num);
        if (ret != 0) {
            vmng_err("Memcpy failed. (dev_id=%u;ret=%d)\n", vpc_unit->dev_id, ret);
            vmnga_unmap_bar_va_cloud_v2(vpc_unit);
            vmng_kfree(vpc_unit);
            return ret;
        }
        vpc_unit->msix_ctrl.msix_irq_base = 0;
        vpc_unit->msix_ctrl.msix_irq_num = unit->msix_info.msix_irq_num;
        vpc_unit->shr_para->msix_offset = unit->msix_info.msix_irq_offset;
        ret = vmnga_vpc_msg_init(vpc_unit);
        if (ret != 0) {
            vmng_err("vmnga vpc msg init failed. (ret=%d)\n", ret);
            vmnga_unmap_bar_va_cloud_v2(vpc_unit);
            vmng_kfree(vpc_unit);
            return ret;
        }
        // notify remote after init vpc channel.
        ret = vpc_top_half_init_finish(vpc_unit);
        if (ret != 0) {
            vmng_err("vpc top half init failed.\n");
            vmnga_uninit_vpc_msg(vpc_unit->msg_dev);
            vmnga_unmap_bar_va_cloud_v2(vpc_unit);
            vmng_kfree(vpc_unit);
            return ret;
        }
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmng_vpc_init);

int vmng_vpc_uninit(struct vmng_vpc_unit *unit_in, int server_type)
{
    struct vmng_msg_dev *msg_dev;
    struct vmnga_vpc_unit *vpc_unit;

    if (unit_in == NULL) {
        vmng_err("Input unit_in is NULL.\n");
        return -EINVAL;
    }

    if (server_type == SERVER_TYPE_VM_PCIE) {
        msg_dev = vmnga_get_msg_dev_by_id(unit_in->dev_id);
        if (msg_dev == NULL) {
            vmng_err("Cannot get msg_dev. (dev_id=%u)\n", unit_in->dev_id);
            return -EINVAL;
        }
        vpc_unit = (struct vmnga_vpc_unit *)msg_dev->unit;
        vpc_top_half_free(vpc_unit);
        vmnga_uninit_vpc_msg(msg_dev);
        vmnga_unmap_bar_va_cloud_v2(vpc_unit);
        vmng_kfree(vpc_unit);
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmng_vpc_uninit);

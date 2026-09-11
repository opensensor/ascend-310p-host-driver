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

#include "virtmngagent_pci.h"
#include "virtmngagent_msg.h"
#include "virtmngagent_unit.h"
#include "virtmngagent_ctrl.h"
#include "virtmngagent_msg_common.h"
#include "virtmngagent_msg_admin.h"
#include "virtmng_public_def.h"
#include "virtmng_msg_admin.h"
#include "virtmng_resource.h"

#include "dbl/uda.h"
#include "vmng_mem_alloc_interface.h"

#include "ka_memory_pub.h"
#include "ka_barrier_pub.h"
#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_driver_pub.h"

/* V2P for TX, call send. */
STATIC void vmnga_admin_notify_rm_pdev(struct vmng_msg_dev *msg_dev, u32 flag)
{
    struct vmng_agent_rm_pdev_cmd_reply cmd;
    struct vmng_tx_msg_proc_info tx_info;
    u32 dev_id;
    int ret;

    if (msg_dev == NULL) {
        vmng_err("msg_dev is NULL.\n");
        return;
    }

    dev_id = msg_dev->dev_id;
    cmd.opcode = VMNGA_ADMIN_OPCODE_RM_PDEV;
    cmd.rm_mode = flag;
    cmd.finish = 0x0;

    tx_info.data = &cmd;
    tx_info.in_data_len = sizeof(struct vmng_agent_rm_pdev_cmd_reply);
    tx_info.out_data_len = sizeof(struct vmng_agent_rm_pdev_cmd_reply);
    tx_info.real_out_len = 0;

    msg_dev->status = VMNG_MSG_DEV_REMOVE;
    ret = vmng_admin_msg_send(msg_dev->admin_tx, &tx_info, (u32)VMNG_MSG_CHAN_TYPE_ADMIN,
        (u32)VMNGA_ADMIN_OPCODE_RM_PDEV);
    if ((ret != 0) || (tx_info.real_out_len != tx_info.out_data_len)) {
        vmng_err("Message send failed. (dev_id=%d; ret=%d; out_len=%u)\n", dev_id, ret, tx_info.real_out_len);
        return;
    }
    if (cmd.finish != VMNG_VM_SUSPEND_SUCCESS) {
        vmng_err("Proc host stop failed. (dev_id=%d; ret=%d)\n", dev_id, cmd.finish);
    }
}

/* P2V for RX, callby with admin recv. */
STATIC int vmnga_admin_para_check(const struct vmng_msg_dev *msg_dev,
    const struct vmng_msg_chan_rx_proc_info *proc_info)
{
    if (msg_dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (proc_info == NULL) {
        vmng_err("Input parameter is error. (dev_id=%d)\n", msg_dev->dev_id);
        return -EINVAL;
    }
    if (proc_info->data == NULL) {
        vmng_err("Input parameter is error. (dev_id=%d)\n", msg_dev->dev_id);
        return -EINVAL;
    }
    return 0;
}

STATIC int vmnga_admin_rx_host_rm_vdev(struct vmng_msg_dev *msg_dev, struct vmng_msg_chan_rx_proc_info *proc_info)
{
    struct vmng_host_rm_vdev_cmd_reply *cmd = NULL;
    u32 dev_id;

    if (vmnga_admin_para_check(msg_dev, proc_info) != 0) {
        vmng_err("Call vmnga_admin_para_check error.\n");
        return -EINVAL;
    }
    dev_id = msg_dev->dev_id;
    cmd = (struct vmng_host_rm_vdev_cmd_reply *)proc_info->data;
    if ((cmd->rm_mode == VMNG_VM_RM_VDEV_WAIT) || (cmd->rm_mode == VMNG_VM_RM_HOST_PDEV_WAIT)) {
        vmng_info("rm_mode is invalid. (dev_id=%u; rm_mode=0x%x)\n", dev_id, cmd->rm_mode);
        vmnga_agent_stop(dev_id);
        *(proc_info->real_out_len) = sizeof(struct vmng_host_rm_vdev_cmd_reply);
        cmd->finish = VMNG_VM_SUSPEND_SUCCESS;
        return 0;
    } else {
        cmd->finish = 0;
        return -1;
    }
}

STATIC int vmnga_online(struct vmnga_unit *unit)
{
    return 0;
}

STATIC int vmnga_offline(struct vmnga_unit *unit)
{
    return 0;
}

STATIC int vmnga_probe_half(struct vmnga_unit *unit)
{
    struct uda_dev_type type;
    struct uda_dev_para para;
    u32 dev_id = unit->dev_id;
    int ret;
#ifndef DRV_UT
    vmng_info("Half probe begin. (dev_id=%u; dtype=%u)\n", dev_id, unit->shr_para->dtype);
    vmnga_register_ctrls_half(unit);

    /* Callby init instance of other ko register. */
    uda_davinci_remote_real_entity_type_pack(&type);
    uda_dev_para_pack(&para, dev_id, UDA_INVALID_UDEVID, HISI_CHIP_UNKNOWN, &unit->pdev->dev);
    ret = uda_add_dev(&type, &para, &dev_id);
    if (ret != 0) {
        vmng_err("Add dev failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto UNREGISTER_CTRLS;
    }

    ret = vmnga_online(unit);
    if (ret != 0) {
        vmng_err("Call vmnga_online failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto REMOVE_DEV;
    }

    unit->startup_status = VMNG_STARTUP_BOTTOM_HALF_OK;

    vmng_info("Half probe finish. (bdf=%02x:%02x.%d; epid=0x%x; dev_id=%u)\n", ka_pci_get_bus_number(unit->pdev),
        KA_PCI_SLOT(unit->pdev->devfn), KA_PCI_FUNC(unit->pdev->devfn),
        ka_pci_get_device_id(unit->pdev), dev_id);
    return 0;

REMOVE_DEV:
    (void)uda_remove_dev(&type, dev_id);

UNREGISTER_CTRLS:
    vmnga_unregister_ctrls_half(unit);
#endif
    return ret;
}

STATIC void vmnga_free_bottom(struct vmnga_unit *unit)
{
    struct uda_dev_type type;
    int ret;

    ret = vmnga_offline(unit);
    if (ret != 0) {
        vmng_err("Call vmnga_offline failed. (dev_id=%u; ret=%d)\n", unit->dev_id, ret);
    }
#ifndef DRV_UT
    uda_davinci_remote_real_entity_type_pack(&type);
    (void)uda_remove_dev(&type, unit->dev_id);
#endif
    vmnga_unregister_ctrls_half(unit);
    vmng_info("Free bottom ok. (dev_id=%u)\n", unit->dev_id);
}

STATIC int vmnga_cfg_pdev(ka_pci_dev_t *pdev)
{
    u8 bus_num = ka_pci_get_bus_number(pdev);
    u8 device_num = KA_PCI_SLOT(pdev->devfn);
    u8 func_num = KA_PCI_FUNC(pdev->devfn);
    int ret;

    ret = ka_pci_enable_device_mem(pdev);
    if (ret != 0) {
        vmng_err("Enable device error. (bdf=%02x:%02x.%d; ret=%d)\n",
                 bus_num, device_num, func_num, ret);
        return ret;
    }

    /*lint -e598 -e648 */
    ret = ka_mm_dma_set_mask_and_coherent(&pdev->dev, KA_DMA_BIT_MASK(DEVDRV_DMA_BIT_MASK_64));
    /*lint +e598 +e648 */
    if (ret != 0) {
        vmng_debug("Try set mask 32. (bus=%02x; dev_num=%02x; func=%d; ret=%d)\n",
                  bus_num, device_num, func_num, ret);
        ret = ka_mm_dma_set_mask_and_coherent(&pdev->dev, KA_DMA_BIT_MASK(DEVDRV_DMA_BIT_MASK_32));
        if (ret != 0) {
            vmng_err("Set mask 32 failed. (bus=%02x; dev_num=%02x; func=%d; ret=%d)\n",
                     bus_num, device_num, func_num, ret);
            goto DISABLE_DEVICE;
        }
    }

    ret = ka_pci_request_regions(pdev, "virtmng");
    if (ret != 0) {
        vmng_err("Call ka_pci_request_regions failed. (bus=%02x; dev_num=%02x; func=%d; ret=%d)\n",
                 bus_num, device_num, func_num, ret);
        goto DISABLE_DEVICE;
    }

    ka_pci_set_master(pdev);

    return 0;

DISABLE_DEVICE:
    ka_pci_disable_device(pdev);
    return ret;
}

STATIC void vmnga_uncfg_pdev(ka_pci_dev_t *pdev)
{
    ka_pci_clear_master(pdev);
    ka_pci_release_regions(pdev);
    ka_pci_disable_device(pdev);
}

STATIC struct vmnga_unit *vmnga_alloc_unit(ka_pci_dev_t *pdev)
{
    struct vmnga_unit *unit = NULL;

    unit = vmng_kzalloc(sizeof(struct vmnga_unit), KA_GFP_KERNEL);
    if (unit == NULL) {
        vmng_err("Call kzalloc failed. (bdf=%02x:%02x.%d)\n", ka_pci_get_bus_number(pdev),
            KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        return NULL;
    }

    unit->dev_id = (u32)VMNG_CTRL_DEVICE_ID_INIT;
    unit->startup_status = VMNG_STARTUP_PROBED;
    ka_pci_set_drvdata(pdev, unit);
    unit->pdev = pdev;
    unit->ep_device_id = ka_pci_get_device_id(pdev);

    return unit;
}

STATIC void vmnga_free_unit(struct vmnga_unit *unit)
{
    vmng_kfree(unit);
}

STATIC void vmnga_get_bar_addr(struct vmnga_unit *unit, ka_pci_dev_t *pdev)
{
    unit->mmio.bar0_base = (phys_addr_t)ka_pci_resource_start(pdev, PCI_BAR_0);
    unit->mmio.bar0_size = (u64)ka_pci_resource_len(pdev, PCI_BAR_0);
    unit->mmio.bar2_base = (phys_addr_t)ka_pci_resource_start(pdev, PCI_BAR_2);
    unit->mmio.bar2_size = (u64)ka_pci_resource_len(pdev, PCI_BAR_2);
    unit->mmio.bar4_base = (phys_addr_t)ka_pci_resource_start(pdev, PCI_BAR_4);
    unit->mmio.bar4_size = (u64)ka_pci_resource_len(pdev, PCI_BAR_4);
}

STATIC int vmnga_map_bar_va(struct vmnga_unit *unit)
{
    ka_pci_dev_t *pdev = unit->pdev;

    unit->db_base = ka_mm_ioremap(unit->mmio.bar0_base, unit->mmio.bar0_size);
    if (unit->db_base == NULL) {
        vmng_err("Call ka_mm_ioremap failed. (bdf=%02x:%02x.%d)\n", ka_pci_get_bus_number(pdev),
            KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        goto DB_ERR;
    }
    unit->shr_para = ka_mm_ioremap(unit->mmio.bar2_base + VMNG_SHR_PARA_ADDR_BASE, VMNG_SHR_PARA_ADDR_SIZE);
    if (unit->shr_para == NULL) {
        vmng_err("Call ka_mm_ioremap failed. (bdf=%02x:%02x.%d)\n", ka_pci_get_bus_number(pdev),
            KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        goto SHR_ERR;
    }
    unit->msg_base = ka_mm_ioremap(unit->mmio.bar2_base + VMNG_MSG_ADDR_BASE, VMNG_MSG_ADDR_SIZE);
    if (unit->msg_base == NULL) {
        vmng_err("Call ka_mm_ioremap failed. (bdf=%02x:%02x.%d)\n", ka_pci_get_bus_number(pdev),
            KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        goto MSG_ERRR;
    }

    return 0;

MSG_ERRR:
    ka_mm_iounmap(unit->shr_para);
    unit->shr_para = NULL;
SHR_ERR:
    ka_mm_iounmap(unit->db_base);
    unit->db_base = NULL;
DB_ERR:
    return -ENOMEM;
}

STATIC void vmnga_unmap_bar_va(struct vmnga_unit *unit)
{
    ka_mm_iounmap(unit->msg_base);
    unit->msg_base = NULL;
    ka_mm_iounmap(unit->shr_para);
    unit->shr_para = NULL;
    ka_mm_iounmap(unit->db_base);
    unit->db_base = NULL;
}

STATIC void vmnga_bind_irq(struct vmnga_unit *unit)
{
    u32 i;
    u32 cpu_id, irq;

    for (i = 0; i < unit->msix_ctrl.msix_irq_num; i++) {
        cpu_id = ka_base_cpumask_local_spread(i + unit->dev_id, ka_driver_dev_to_node(&unit->pdev->dev));
        irq = unit->msix_ctrl.entries[i].vector;
        (void)ka_base_irq_set_affinity_hint(irq, ka_system_get_cpu_mask(cpu_id));
    }
}

STATIC void vmnga_unbind_irq(struct vmnga_unit *unit)
{
    u32 irq;
    u32 i;

    for (i = 0; i < unit->msix_ctrl.msix_irq_num; i++) {
        irq = unit->msix_ctrl.entries[i].vector;
        (void)ka_base_irq_set_affinity_hint(irq, NULL);
    }
}

STATIC int vmnga_alloc_interrupt(struct vmnga_unit *unit)
{
    u32 vector_num;
    uint16_t i;

    for (i = 0; i < VIRTMNGAGENT_MSIX_MAX; i++) {
        unit->msix_ctrl.entries[i].entry = i;
    }
    vector_num = (u32)ka_pci_enable_msix_range(unit->pdev, unit->msix_ctrl.entries, VIRTMNGAGENT_MSIX_MIN,
        VIRTMNGAGENT_MSIX_MAX);
    if ((vector_num < VIRTMNGAGENT_MSIX_MIN) || (vector_num > VIRTMNGAGENT_MSIX_MAX)) {
        vmng_err("Call ka_pci_enable_msix_range failed. (dev_id=%u; vector_num=%u)\n", unit->dev_id, vector_num);
        return -ENOSPC;
    }
    unit->msix_ctrl.msix_irq_num = vector_num;

    vmnga_bind_irq(unit);
    vmng_info("Alloc irq success. (dev_id=%u; vector_num=%u)\n", unit->dev_id, vector_num);
    return 0;
}

STATIC void vmnga_free_interrupt(struct vmnga_unit *unit)
{
    vmnga_unbind_irq(unit);
    ka_pci_disable_msix(unit->pdev);
}

STATIC ka_irqreturn_t vmnga_start_irq(int irq, void *data)
{
    struct vmnga_unit *unit = data;

    if (data == NULL) {
        vmng_err("Input parameter is error. (irq=%d)\n", irq);
        return KA_IRQ_NONE; /* KA_IRQ_NONE: interrupt was not from this device or was handled */
    }
    ka_base_atomic_set(&unit->start_dev.start_flag, VMNG_TASK_SUCCESS);
    ka_task_wake_up_interruptible(&unit->start_dev.wq);
    return KA_IRQ_HANDLED;
}

STATIC void vmnga_start_task(ka_work_struct_t *p_work)
{
    struct vmnga_unit *unit = ka_container_of(p_work, struct vmnga_unit, start_work);
    struct vmnga_start_dev *start_dev = &unit->start_dev;
    u32 dev_id = unit->dev_id;
    u32 time;
    int ret;
    ka_sysinfo_t mem_info;
    u64 mem_size;

    /* prepare for wait */
    start_dev->db_id = VMNG_DB_BASE_LOAD;
    ka_task_init_waitqueue_head(&start_dev->wq);

    /* set bar shr para, then ring doorbell. */
    unit->shr_para->start_flag = VMNG_VM_START_WAIT;
    ka_wmb();
    /* notify srv */
    vmnga_set_doorbell(unit->db_base, start_dev->db_id, 1);

    ka_si_meminfo(&mem_info);
    mem_size = mem_info.totalram * KA_MM_PAGE_SIZE / GB_UNIT + 1;
    if (mem_size < MIN_MEM_SIZE) {
        mem_size = MIN_MEM_SIZE;
    }
    vmng_info("Start wait begin. (dev_id=%u; size=%llu)\n", dev_id, mem_size);
    time = (u32)ka_system_msecs_to_jiffies((unsigned int)(VMNGA_START_FB_TIMEOUT_MS * mem_size));
    /*lint -e666*/
    ret = (int)ka_task_wait_event_interruptible_timeout(start_dev->wq,
        (ka_base_atomic_read(&start_dev->start_flag) == VMNG_TASK_SUCCESS), time);
    /*lint +e666*/
    if (ret <= 0) {
        vmng_err("Wait host time out. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return;
    }
    vmng_info("Wait host ok. (dev_id=%u; time_remain=%d)\n", dev_id, ret);

    ret = vmnga_probe_half(unit);
    if (ret != 0) {
        vmng_err("Bottom half failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
}
STATIC void vmnga_start_half_probe_task(struct vmnga_unit *unit)
{
    ka_atomic_t cpu_start;
    int first_cpu = -1;
    int cpu;

    ka_base_for_each_online_cpu(cpu) {
        if (first_cpu == -1) {
            first_cpu = cpu;
        }
        if (ka_base_atomic_read(&cpu_start) <= cpu) {
            ka_base_atomic_set(&cpu_start, cpu + 1);
            vmng_info("Start half probe work. (devid=%u; cpu=%d).\n", unit->dev_id, cpu);
            ka_task_schedule_work_on(cpu, &unit->start_work);
            return;
        }
    }
    if (first_cpu == -1) {
        ka_task_schedule_work(&unit->start_work);
        return;
    }
    // Schedule on first cpu if not cpu is found after search each online cpu
    // If there are 4 cpus, possible schedule sequence is 0 -> 2 -> 3 -> 4 -> 0 -> 2
    ka_base_atomic_set(&cpu_start, first_cpu + 1);
    vmng_info("Start half probe work. (devid=%u; cpu=%d).\n", unit->dev_id, first_cpu);
    ka_task_schedule_work_on(first_cpu, &unit->start_work);
    return;
}

STATIC int vmnga_start_init(struct vmnga_unit *unit)
{
    int ret;
    struct vmnga_start_dev *start_dev = &unit->start_dev;

    /* prepare for host return msix */
    start_dev->msix_id = VMNG_MSIX_BASE_LOAD;
    ret = vmnga_register_irq_func((void *)unit, start_dev->msix_id, vmnga_start_irq, (void *)unit, "vmnga_start");
    if (ret != 0) {
        vmng_err("Call vmnga_register_irq_func failed. (dev_id=%u; ret=%d)\n", unit->dev_id, ret);
        return ret;
    }
    vmnga_set_ctrl_startup_flag(unit->dev_id, VMNG_STARTUP_TOP_HALF_OK);
    KA_TASK_INIT_WORK(&unit->start_work, vmnga_start_task);
    vmnga_start_half_probe_task(unit);

    vmng_info("Start init ok. (dev_id=%u)\n", unit->dev_id);
    return 0;
}

STATIC void vmnga_free_start(struct vmnga_unit *unit)
{
    int ret;

    vmnga_unbind_irq(unit);
    ka_task_cancel_work_sync(&unit->start_work);
    ret = vmnga_unregister_irq_func((void *)unit, unit->start_dev.msix_id, (void *)unit);
    if (ret != 0) {
        vmng_err("Unregister start irq failed. (dev_id=%u; ret=%d)\n", unit->dev_id, ret);
    }
}

STATIC int vmnga_probe_top_prepare(ka_pci_dev_t *pdev, struct vmnga_unit **unit_ptr)
{
    struct vmnga_unit *unit = NULL;
    u8 bus_num = ka_pci_get_bus_number(pdev);
    u8 device_num = KA_PCI_SLOT(pdev->devfn);
    u8 func_num = KA_PCI_FUNC(pdev->devfn);
    u32 dev_id;
    int ret;
    const unsigned int BUS_OFFSET = 8;

    unit = vmnga_alloc_unit(pdev);
    if (unit == NULL) {
        ret = -ENOMEM;
        vmng_err("Alloc failed. (bdf=%02x:%02x.%d)\n", bus_num, device_num, func_num);
        return ret;
    }

    vmng_info("Alloc unit ok. (bdf=%02x:%02x.%d)\n", bus_num, device_num, func_num);
    vmnga_get_bar_addr(unit, unit->pdev);
    ret = vmnga_map_bar_va(unit);
    if (ret != 0) {
        vmng_err("Iomap to va failed. (bdf=%02x:%02x.%d; ret=%d)\n", bus_num, device_num, func_num, ret);
        goto FREE_UNIT;
    }
    vmng_info("Alloc bar ok. (bdf=%02x:%02x.%d; ret=%d)\n", bus_num, device_num, func_num, ret);

    /* devid alloc here */
    ret = vmnga_register_ctrls(unit);
    if (ret != 0) {
        vmng_err("Register ctrl failed. (bdf=%02x:%02x.%d; ret=%d)\n",
                 bus_num, device_num, func_num, ret);
        goto UNMAP_BAR_VA;
    }
    dev_id = unit->dev_id;
    unit->shr_para->agent_device = ka_pci_get_device_id(pdev);
    unit->shr_para->agent_bdf = (bus_num << BUS_OFFSET) | (pdev->devfn);
    unit->shr_para->agent_dev_id = dev_id;
    unit->shr_para->chan_num = VMNG_MSG_CHAN_NUM_MAX;
    *unit_ptr = unit;
    vmng_info("Alloc dev_id. (bdf=%02x:%02x.%d; dev_id=%u)\n", bus_num, device_num, func_num, dev_id);
    vmnga_record_dev_startup(dev_id);
    vmnga_report_dev_startup(dev_id);

    return 0;

UNMAP_BAR_VA:
    vmnga_unmap_bar_va(unit);
FREE_UNIT:
    vmnga_free_unit(unit);
    return ret;
}

STATIC void vmnga_unprobe_top_prepare(struct vmnga_unit *unit)
{
    u32 dev_id = unit->dev_id;

    vmnga_unregister_ctrls(unit);
    vmnga_unmap_bar_va(unit);
    vmnga_free_unit(unit);
    vmng_info("Unprobe top prepare ok. (dev_id=%d)\n", dev_id);
}

STATIC int vmnga_alloc_vpc_unit(struct vmnga_unit *unit)
{
    struct vmnga_vpc_unit *vpc_unit;

    vpc_unit = vmng_kzalloc(sizeof(struct vmnga_vpc_unit), KA_GFP_KERNEL);
    if (vpc_unit == NULL) {
        vmng_err("Kzalloc vpc unit failed.\n");
        return -ENOMEM;
    }
    vpc_unit->pdev = unit->pdev;
    vpc_unit->db_base = unit->db_base;
    vpc_unit->msg_base = unit->msg_base;
    vpc_unit->ts_msg_base = unit->ts_msg_base;
    vpc_unit->shr_para = unit->shr_para;
    vpc_unit->dev_id = unit->dev_id;
    vpc_unit->fid = 0;
    if (memcpy_s(&vpc_unit->msix_ctrl, sizeof(vpc_unit->msix_ctrl), &unit->msix_ctrl, sizeof(unit->msix_ctrl)) != 0) {
        vmng_err("Memcpy msix ctrl failed.\n");
        vmng_kfree(vpc_unit);
        return -EINVAL;
    }
    unit->vpc_unit = vpc_unit;
    return 0;
}

STATIC int vmnga_probe_top_feature(struct vmnga_unit *unit)
{
    u32 dev_id = unit->dev_id;
    int ret;

    ret = vmnga_alloc_interrupt(unit);
    if (ret != 0) {
        vmng_err("Alloc interrupt failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = vmnga_alloc_vpc_unit(unit);
    if (ret != 0) {
        vmng_err("Alloc vpc unit failed.\n");
        goto FREE_INTERRUPT;
    }

    ret = vmnga_vpc_msg_init((void *)unit->vpc_unit);
    if (ret != 0) {
        vmng_err("Call vmnga_vpc_msg_init failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto FREE_VPC_UNIT;
    }
    unit->msg_dev = unit->vpc_unit->msg_dev;
    vmng_info("Message device init ok. (dev_id=%u)\n", dev_id);
    vmnga_register_extended_common_msg_client(unit->msg_dev);
    return ret;

FREE_VPC_UNIT:
    vmng_kfree(unit->vpc_unit);
    unit->vpc_unit = NULL;
FREE_INTERRUPT:
    vmnga_free_interrupt(unit);
    return ret;
}

STATIC void vmnga_unprobe_top_feature(struct vmnga_unit *unit)
{
    vmnga_unregister_extended_common_msg_client(unit->msg_dev);
    vmnga_uninit_vpc_msg(unit->msg_dev);
    unit->msg_dev = NULL;
    vmng_kfree(unit->vpc_unit);
    unit->vpc_unit = NULL;
    vmnga_free_interrupt(unit);
    vmng_info("Unprobe top feature ok. (dev_id=%u)\n", unit->dev_id);
}

STATIC int vmnga_probe(ka_pci_dev_t *pdev, const ka_pci_device_id_t *data)
{
    u8 bus_num = ka_pci_get_bus_number(pdev);
    u8 device_num = KA_PCI_SLOT(pdev->devfn);
    u8 func_num = KA_PCI_FUNC(pdev->devfn);
    struct vmnga_unit *unit = NULL;
    u32 dev_id;
    int ret;

    vmng_info("Probe driver input. (bdf=%02x:%02x.%d; epid=0x%x)\n",
              bus_num, device_num, func_num, ka_pci_get_device_id(pdev));
    ret = vmnga_cfg_pdev(pdev);
    if (ret != 0) {
        vmng_err("Call vmnga_cfg_pdev failed. (bdf=%02x:%02x.%d; ret=%d)\n",
                 bus_num, device_num, func_num, ret);
        return ret;
    }

    ret = vmnga_probe_top_prepare(pdev, &unit);
    if (ret != 0) {
        vmng_err("Probe prepare failed. (bdf=%02x:%02x.%d; ret=%d)\n",
                 bus_num, device_num, func_num, ret);
        goto UNCFG_PDEV;
    }
    dev_id = unit->dev_id;

    ret = vmnga_probe_top_feature(unit);
    if (ret != 0) {
        vmng_err("Probe feature failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto UNPROBE_PREPARE;
    }

    ret = vmnga_start_init(unit);
    if (ret != 0) {
        vmng_err("Start to host failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto UNPROBE_FEATURE;
    }

    unit->module_exit_flag = VMNG_MODULE_REMOVE_BY_PRERESET;
    vmng_event("Top probe success. (dev_id=%u)\n", dev_id);
    return 0;

UNPROBE_FEATURE:
    vmnga_unprobe_top_feature(unit);

UNPROBE_PREPARE:
    vmnga_unprobe_top_prepare(unit);

UNCFG_PDEV:
    vmnga_uncfg_pdev(pdev);

    return ret;
}

STATIC void vmnga_free_top(struct vmnga_unit *unit)
{
    ka_pci_dev_t *pdev = unit->pdev;

    vmnga_free_start(unit);
    vmnga_unprobe_top_feature(unit);
    vmnga_unprobe_top_prepare(unit);
    vmnga_uncfg_pdev(pdev);
}

STATIC void vmnga_remove(ka_pci_dev_t *pdev)
{
    struct vmnga_unit *unit = NULL;

    unit = (struct vmnga_unit *)ka_pci_get_drvdata(pdev);
    if (unit == NULL) {
        vmng_err("Call ka_pci_get_drvdata failed. (bdf=%02x:%02x.%d)\n",
                 ka_pci_get_bus_number(pdev), KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        return;
    }
    vmnga_dev_state_notifier(unit);
    if (unit->msg_base != NULL) {
        vmnga_admin_notify_rm_pdev(unit->msg_dev, VMNG_VM_REMOVE_WAIT);
    }
    if (vmnga_get_ctrl_startup_flag(unit->dev_id) == VMNG_STARTUP_BOTTOM_HALF_OK) {
        vmnga_free_bottom(unit);
    }
    if (vmnga_get_ctrl_startup_flag(unit->dev_id) == VMNG_STARTUP_TOP_HALF_OK) {
        vmnga_free_top(unit);
    }
}

STATIC void vmnga_shutdown(ka_pci_dev_t *pdev)
{
    struct vmnga_unit *unit = NULL;

    unit = (struct vmnga_unit *)ka_pci_get_drvdata(pdev);
    if (unit == NULL) {
        vmng_err("Call ka_pci_get_drvdata failed. (bdf=%02x:%02x.%d)\n",
                 ka_pci_get_bus_number(pdev), KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
        return;
    }

    if (unit->msg_base != NULL) {
        vmnga_admin_notify_rm_pdev(unit->msg_dev, VMNG_VM_SHUTDOWN_WAIT);
    }
    if (vmnga_get_ctrl_startup_flag(unit->dev_id) == VMNG_STARTUP_BOTTOM_HALF_OK) {
        vmnga_free_bottom(unit);
    }
    if (vmnga_get_ctrl_startup_flag(unit->dev_id) == VMNG_STARTUP_TOP_HALF_OK) {
        vmnga_free_top(unit);
    }
}


static char g_vmnga_driver_name[] = "virtmng_agent";
static const ka_pci_device_id_t g_vmnga_tbl[] = {{ KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_MINIV1), 0},
                                                   { KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_MINIV2), 0},
                                                   { KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD), 0},
                                                   { KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD_V2), 0},
                                                   {}};
KA_MODULE_DEVICE_TABLE(pci, g_vmnga_tbl);

static ka_pci_driver_t g_vmnga_driver_ver = {
    ka_pci_init_pd_name(g_vmnga_driver_name)
    ka_pci_init_pd_id_table(g_vmnga_tbl)
    ka_pci_init_pd_probe(vmnga_probe)
    ka_pci_init_pd_remove(vmnga_remove)
    ka_pci_init_pd_driver("virtmng_agent", NULL)
    ka_pci_init_pd_err_handler(NULL)
    ka_pci_init_pd_shutdown(vmnga_shutdown)
};

int g_vmnga_pci_device_num = 0;

/* Assume that one type device id can be found, the g_vmnga_pci_device_num just store once. */
STATIC void vmnga_init_dev_num(void)
{
    ka_pci_dev_t *pdev = NULL;
    int dev_num;
    int id_num = (int)(sizeof(g_vmnga_tbl) / sizeof(ka_pci_device_id_t));
    int i;

    for (i = 0; i < id_num; i++) {
        pdev = NULL;
        dev_num = -1;
        do {
            pdev = ka_pci_get_device(g_vmnga_tbl[i].vendor, g_vmnga_tbl[i].device, pdev);
            dev_num++;
        } while (pdev != NULL);

        vmng_info("Get dev_num. (vendor=%x; device=%x; dev_num=%d)\n",
                  g_vmnga_tbl[i].vendor, g_vmnga_tbl[i].device, dev_num);
        if (dev_num > 0) {
            break;
        }
    }
#ifndef DRV_UT
    (void)uda_set_detected_phy_dev_num((u32)dev_num);
#endif
    vmng_info("Get findout total device number. (dev_num=%d)\n", dev_num);
    g_vmnga_pci_device_num = dev_num;
}


STATIC int __init vmnga_init_module(void)
{
    int ret;

    vmng_info("Init module begin. (name=\"%s\")\n", g_vmnga_driver_name);
    vmnga_init_dev_num();
    ret = vmnga_ctrl_init();
    if (ret != 0) {
        vmng_err("Ctrl init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = vmnga_register_admin_rx_func(VMNGH_ADMIN_OPCODE_RM_VDEV, vmnga_admin_rx_host_rm_vdev);
    if (ret != 0) {
        vmng_err("Register admin rx func failed. (ret=%d)\n", ret);
        return ret;
    }

    /*lint -e64 */
    ret = ka_pci_register_driver(&g_vmnga_driver_ver);
    if (ret != 0) {
        vmng_err("Register pcie ko client failed. (ret=%d)\n", ret);
        vmnga_unregister_admin_rx_func(VMNGH_ADMIN_OPCODE_RM_VDEV);
        return ret;
    } /*lint +e64*/

    vmng_info("Init module finish. (name=\"%s\")\n", g_vmnga_driver_name);
    return 0;
}
ka_module_init(vmnga_init_module);

STATIC void __exit vmnga_exit_module(void)
{
    struct vmnga_unit *unit = NULL;
    u32 i;

    for (i = 0; i < VMNG_PDEV_MAX; i++) {
        unit = vmnga_get_top_half_unit_by_id(i);
        if (unit == NULL) {
            continue;
        }
        unit->module_exit_flag = VMNG_MODULE_REMOVE_BY_MODULE_EXIT;
    }
    ka_pci_unregister_driver(&g_vmnga_driver_ver);
    vmnga_unregister_admin_rx_func(VMNGH_ADMIN_OPCODE_RM_VDEV);
    vmng_info("Exit module finish. (name=\"%s\")\n", g_vmnga_driver_name);
    return;
}
ka_module_exit(vmnga_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("virt mng agent driver");
KA_MODULE_LICENSE("GPL");

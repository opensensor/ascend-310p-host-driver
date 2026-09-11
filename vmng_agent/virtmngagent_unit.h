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

#ifndef VIRTMNGAGENT_UNIT_H
#define VIRTMNGAGENT_UNIT_H

#include "virtmng_interface.h"
#include "virtmngagent_vpc_unit.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_pci_pub.h"

#include "ka_task_pub.h"

#define VIRTMNGAGENT_MSIX_MAX 128
#define VIRTMNGAGENT_MSIX_MIN 128

struct vmnga_res_msix {
    u32 vpc_base;
    u32 vpc_size;
    u32 ts_base;
    u32 ts_size;
};

struct vmnga_msxi_ctrl {
    ka_msix_entry_t entries[VIRTMNGAGENT_MSIX_MAX];
    u32 msix_irq_num;
    u32 msix_irq_base;
};

struct vmnga_watchdog_dev {
    void __iomem *val_base;
    u32 db;
};

struct vmnga_start_dev {
    ka_wait_queue_head_t wq;      /* wait queue for start check */
    ka_wait_queue_head_t wq_stop; /* wait queue for stop host */
    ka_atomic_t start_flag;       /* start flag for irq to work */
    u32 db_id;
    u32 msix_id;
};

struct vmnga_unit {
    ka_pci_dev_t *pdev;                       /* pci dev */
    struct vmnga_mmio mmio;                     /* store pcie bar address PA and size */
    void __iomem *db_base;                      /* doorbell base address VA , bar0 */
    void __iomem *msg_base;                     /* msg base address VA ; part of bar2 */
    void __iomem *ts_msg_base;                  /* ts msg base address VA, bar4 */
    struct vmng_shr_para __iomem *shr_para;     /* share para address VA, use for host and agent; part of bar2 */
    struct vmnga_msxi_ctrl msix_ctrl;           /* misx interrupts ctrl struct */
    struct vmnga_start_dev start_dev;           /* start check, agent notify host, then feedback */
    ka_work_struct_t start_work;              /* start check work */
    struct vmng_msg_dev *msg_dev;               /* msg dev total, alloc and store point. */
    enum vmng_startup_flag_type startup_status; /* startup status */
    u32 dev_id;                                 /* device id alloced for davinci chip */
    u32 func_id;                                /* bdf, function of pcie */
    u32 ep_device_id;                           /* device id of davinci chip, see HISI_EP_DEVICE_ID_MINIV1 etc. */
    u32 module_exit_flag;
    struct vmnga_vpc_unit *vpc_unit;
};

void vmnga_bar_wr(void __iomem *io_base, u32 offset, u32 val);
void vmnga_bar_rd(const void __iomem *io_base, u32 offset, u32 *val);
void vmnga_set_doorbell(void __iomem *io_base, u32 db_id, u32 val);
int vmnga_register_irq_func(void *drvdata, u32 vector_index, ka_irqreturn_t (*callback_func)(int, void *), void *para,
    const char *name);
int vmnga_unregister_irq_func(void *drvdata, u32 vector_index, void *para);

#endif

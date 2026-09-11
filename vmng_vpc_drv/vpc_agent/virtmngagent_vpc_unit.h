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

#ifndef VIRTMNGAGENT_VPC_UNIT_H
#define VIRTMNGAGENT_VPC_UNIT_H

#include "ka_pci_pub.h"
#include "virtmng_msg_pub.h"
#include "virtmng_public_def.h"
#include "vpc_interface.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"

struct vmnga_vpc_msxi_ctrl {
    ka_msix_entry_t entries[VIRTMNGAGENT_MSIX_MAX];
    u32 msix_irq_num;
    u32 msix_irq_base;
};

struct vmnga_vpc_start_dev {
    ka_wait_queue_head_t wq;      /* wait queue for start check */
    ka_wait_queue_head_t wq_stop; /* wait queue for stop host */
    ka_atomic_t start_flag;       /* start flag for irq to work */
    u32 db_id;
    u32 msix_id;
};

struct vmnga_vpc_unit {
    ka_pci_dev_t *pdev;                       /* pci dev */
    void __iomem *db_base;                      /* doorbell base address VA , bar0 */
    void __iomem *msg_base;                     /* msg base address VA ; part of bar2 */
    void __iomem *ts_msg_base;                  /* ts msg base address VA, bar4 */
    struct vmng_shr_para __iomem *shr_para;     /* share para address VA, use for host and agent; part of bar2 */
    struct vmnga_vpc_msxi_ctrl msix_ctrl;       /* misx interrupts ctrl struct */
    struct vmnga_vpc_start_dev start_dev;       /* start dev info remote and wait remote reply */
    struct vmng_msg_dev *msg_dev;               /* msg dev total, alloc and store point. */
    u32 dev_id;                                 /* device id alloced for davinci chip */
    u32 fid;
};

void vmnga_bar_wr(void __iomem *io_base, u32 offset, u32 val);
void vmnga_bar_rd(const void __iomem *io_base, u32 offset, u32 *val);
void vmnga_set_doorbell(void __iomem *io_base, u32 db_id, u32 val);
int vmnga_register_vpc_irq_func(void *drvdata, u32 vector_index, ka_irqreturn_t (*callback_func)(int, void *), void *para,
    const char *name);
int vmnga_unregister_vpc_irq_func(void *drvdata, u32 vector_index, void *para);

#endif

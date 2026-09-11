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
#ifndef VIRTMNGHOST_VPC_UNIT_H
#define VIRTMNGHOST_VPC_UNIT_H

#include "ka_pci_pub.h"
#include "virtmng_msg_pub.h"
#include "virtmng_public_def.h"

struct vmngh_vpc_unit {
    void *vdavinci;
    ka_pci_dev_t *pdev;                       /* pci dev */
    void __iomem *db_base;                      /* doorbell base address VA , bar0 */
    void __iomem *msg_base;                     /* msg base address VA ; part of bar2 */
    void __iomem *ts_msg_base;                  /* ts msg base address VA, bar4 */
    struct vmng_msg_dev *msg_dev;               /* msg dev total, alloc and store point. */
    struct vmng_shr_para *shr_para;
    u32 dev_id;                                 /* device id alloced for davinci chip */
    u32 fid;
};
int vmngh_dev_id_check(u32 dev_id, u32 fid);


#endif
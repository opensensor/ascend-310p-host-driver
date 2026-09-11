/*
 * Copyright (C) 2019-2022. Huawei Technologies Co., Ltd. All rights reserved.
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
#ifndef VTSDRV_PROC_H
#define VTSDRV_PROC_H

#include "vtsdrv_common.h"
#include "virtmng_interface.h"

long vtsdrv_ioctl(ka_file_t *file, u32 cmd, unsigned long arg);
s32 vtsdrv_open(ka_inode_t *node, ka_file_t *file);
s32 vtsdrv_release(ka_inode_t *node, ka_file_t *file);
int vtsdrv_mmap(ka_file_t *filep, struct vm_area_struct *vma);
void vtsdrv_com_msg_init(u32 dev_id, u32 fid, s32 status);
s32 vtsdrv_com_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
s32 vtsdrv_vpc_init(void);
s32 vtsdrv_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
s32 vtsdrv_vpc_msg_send(u32 devid, struct vtsdrv_msg *msg);
s32 vtsdrv_common_msg_send(u32 devid, struct vtsdrv_msg *msg);
int vtsdrv_release_prepare(ka_file_t *file_op, unsigned long mode);

#endif

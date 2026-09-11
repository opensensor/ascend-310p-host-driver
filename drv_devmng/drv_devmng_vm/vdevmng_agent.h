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

#ifndef __DEVDRV_VDEVMNG_AGENT__HOST_H
#define __DEVDRV_VDEVMNG_AGENT__HOST_H

int vdevmng_register_client(void);
void vdevmng_unregister_client(void);
int vdevmng_vpc_msg_send(u32 devid, struct vdevmng_ioctl_msg *iomsg);
int vdevmng_common_msg_send(u32 devid, enum VDEVMNG_CTRL_MSG_TYPE type,
    struct vdevmng_ctrl_msg* ctrl_msg);

#endif

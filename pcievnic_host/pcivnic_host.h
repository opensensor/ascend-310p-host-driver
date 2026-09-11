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

#ifndef _PCIVNIC_HOST_H_
#define _PCIVNIC_HOST_H_

#define PCIVNIC_INIT_NETDEV_RETRY_TIMES 3

struct pcivnic_netdev *pcivnic_alloc_vnic_dev(void);
void pcivnic_free_vnic_dev(void);
int pcivnic_register_client(void);
int pcivnic_unregister_client(void);
int pcivnic_init_netdev(struct pcivnic_netdev *vnic_dev);
#endif

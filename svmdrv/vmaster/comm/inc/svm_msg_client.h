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

#ifndef SVM_MSG_CLIENT_H
#define SVM_MSG_CLIENT_H

#include <linux/device.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "devdrv_interface.h"
#include "svm_dev_res_mng.h"

ka_device_t *devmm_device_get_by_devid(u32 dev_id);
void devmm_device_put_by_devid(u32 dev_id);

bool devmm_device_is_ready(u32 dev_id);

void devmm_init_msg(void);
int devmm_chan_msg_send(void *msg, unsigned int len, unsigned int out_len);
int devmm_chan_msg_send_inner(void *msg, unsigned int len, unsigned int out_len);
int devmm_msg_chan_init(void);
void devmm_msg_chan_uninit(void);

int devmm_dev_res_init(struct devmm_dev_res_mng *dev_res_mng);

#endif


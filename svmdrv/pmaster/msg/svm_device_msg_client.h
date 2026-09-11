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

#ifndef SVM_DEVICE_MSG_CLIENT_H
#define SVM_DEVICE_MSG_CLIENT_H

#include <linux/device.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "devdrv_interface.h"
#include "svm_dev_res_mng.h"

bool devmm_device_agent_is_ready(u32 devid);

int devmm_device_chan_msg_send(u32 dev_id, void *msg, u32 len, u32 out_len);
int devmm_common_msg_send(void *msg, unsigned int len, unsigned int out_len);

int devmm_dev_res_init(struct devmm_dev_res_mng *dev_res_mng);
int devmm_query_smmu_status(ka_device_t *dev);

#endif

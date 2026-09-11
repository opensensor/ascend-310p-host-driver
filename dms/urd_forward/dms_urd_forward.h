/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-04-16
*/

#ifndef __URD_FORWARD_H__
#define __URD_FORWARD_H__

#include <linux/types.h>
#include <linux/time.h>

#include "ascend_hal_error.h"
#include "dms_define.h"
#include "drv_type.h"
#include "urd_feature.h"
#include "devdrv_manager_msg.h"

#define DMS_URD_FORWARD_CMD_NAME "DMS_URD_FORWARD_CMD_NAME"

int dms_urd_forward_init(void);
void dms_urd_forward_uninit(void);
int dms_send_msg_to_device_by_h2d(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_send_msg_to_device_by_h2d_kernel(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_send_msg_to_device_by_h2d_get_va(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_send_msg_to_device_lp_stress_set(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int urd_forward_get_memory_fault_syscnt(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_urd_forward_send_to_device(u32 phy_id, u32 vfid, struct urd_forward_msg *urd_msg, char *out, u32 out_len);
int dms_set_urd_msg(DMS_FEATURE_S *feature_cfg, char *in, u32 in_len, u32 out_len, struct urd_forward_msg *urd_msg);
int dms_send_msg_to_device_by_h2d_multi_packets(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_send_msg_to_device_by_h2d_multi_packets_kernel(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif
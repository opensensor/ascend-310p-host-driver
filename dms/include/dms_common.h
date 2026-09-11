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

#ifndef __DMS_COMMON_H__
#define __DMS_COMMON_H__

#include "dms_cmd_def.h"

struct dms_ctrl_cb {
    struct list_head device_list;
};

struct dms_info {
    struct list_head list;
    unsigned int plat_info; /* 0:device side, 1: host side */
    unsigned int machine_mode; /* 0:RC_MODE,  1: EP_MODE */
    unsigned int amp_or_smp; /* 0:AMP_MODE, 1: SMP_MODE */
};

struct dms_h2d_msg_head {
    u32 dev_id;
    u32 msg_id;
    u16 valid;  /* validity judgement, 0x5A5A is valide */
    u16 result; /* process result from rp, zero for succ, non zero for fail */
};

#define DMS_INFO_PAYLOAD_LEN    512UL
#define DMS_H2D_MSG_PAYLOAD_LEN (DMS_INFO_PAYLOAD_LEN - sizeof(struct dms_h2d_msg_head))
struct dms_h2d_msg {
    struct dms_h2d_msg_head header;
    u8 payload[DMS_H2D_MSG_PAYLOAD_LEN];
};


#endif

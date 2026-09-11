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

#include "virtmng_public_def.h"
#include "virtmng_msg_common.h"

#include "ka_base_pub.h"
#include "ka_system_pub.h"

enum vmng_data_offset {
    DATA_SHIFT_OFFSET = 1,
    DATA_REDUCE_OFFSET = 0x324,
    DATA_ADD_OFFSET = 1,
};

static void vmng_msg_cmn_test_rand_sleep(u32 dly_us)
{
    const u32 DATA_OFFSET = 16;
    u32 time;
    u32 rd;

    rd = ka_base_get_random_u32();
    rd = rd >> DATA_OFFSET;
    time = (dly_us * rd) >> DATA_OFFSET;
    vmng_debug("Get time value. (rd=%u; time=%u)\n", rd, time);
    ka_system_usleep_range(time, time);
}

int vmng_msg_recv_common_verfiy_info(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct vmng_msg_common_pcie_txd_verify *test_data = proc_info->data;
    const u32 SLEEP_TIME = 100;

    vmng_debug("Get parameter value. (dev_id=%u; fid=%u; cnt=%u; d1=%u; d2=%u)\n",
               dev_id, fid, test_data->cnt, test_data->d1, test_data->d2);
    test_data->cnt++;
    test_data->d1 = test_data->d1 >> DATA_SHIFT_OFFSET;
    test_data->d2 = test_data->d2 - DATA_REDUCE_OFFSET;
    *(proc_info->real_out_len) = sizeof(struct vmng_msg_common_pcie_txd_verify);
    vmng_msg_cmn_test_rand_sleep(SLEEP_TIME);
    return 0;
}

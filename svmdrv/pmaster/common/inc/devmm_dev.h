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

#ifndef DEVMM_DEV_H
#define DEVMM_DEV_H
#include "svm_cmd.h"
#include "ka_base_pub.h"
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_net_pub.h"
#include "ka_common_pub.h"
#include "ka_list_pub.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "ka_driver_pub.h"

struct devmm_private_data {
    void *process;
    int custom_flag;
    ka_atomic_t init_flag;
    ka_atomic_t next_seg_id;
};

enum devmm_endpoint_type devmm_get_end_type(void);
bool devmm_device_is_pf(u32 devid);
int devmm_get_pfvf_id_by_devid(u32 devid, u32 *pf_id, u32 *vf_id);

#endif /* __DEVMM_DEV_H__ */

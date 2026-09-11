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

#ifndef URD_CONTAINER_H
#define URD_CONTAINER_H

#include <linux/types.h>

#ifdef AOS_LLVM_BUILD
static inline int urd_container_logical_id_to_physical_id(
    u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid)
{
    *physical_dev_id = logical_dev_id;
    *vfid = 0;
    return 0;
}

static inline int urd_container_is_in_container(void)
{
    return false;
}

static inline int urd_container_is_in_admin_container(void)
{
    return false;
}

static inline bool urd_is_pf_device(unsigned int dev_id)
{
    return true;
}
#else  /* NOT AOS_LLVM_BUILD */
bool urd_is_pf_device(unsigned int dev_id);

int urd_container_is_in_admin_container(void);

int urd_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);

int urd_container_is_in_container(void);
#endif /* AOS_LLVM_BUILD */

#endif

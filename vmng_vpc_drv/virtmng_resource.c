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

#include "virtmng_resource.h"

bool vmng_is_external_db(u32 db_index)
{
    if ((db_index >= VMNG_DB_BASE_EXTERNAL_MIN) && (db_index < VMNG_DB_BASE_EXTERNAL_MAX)) {
        return true;
    }
    return false;
}

bool vmng_is_external_msix(u32 msix_index)
{
    if ((msix_index >= VMNG_MSIX_BASE_EXTERNAL_MIN) && (msix_index < VMNG_MSIX_BASE_EXTERNAL_MAX)) {
        return true;
    }
    return false;
}

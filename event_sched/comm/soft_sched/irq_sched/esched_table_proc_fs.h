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

#ifndef ESCHED_TABLE_PROC_FS_H
#define ESCHED_TABLE_PROC_FS_H

#include "esched_table.h"

void esched_table_proc_fs_add_table(struct esched_table_mng *tm, struct esched_table *table);
void esched_table_proc_fs_del_table(struct esched_table_mng *tm, struct esched_table *table);
void esched_table_proc_fs_add_dev(struct esched_table_mng *tm);
void esched_table_proc_fs_del_dev(struct esched_table_mng *tm);
void esched_table_proc_fs_init(void);
void esched_table_proc_fs_uninit(void);


#endif

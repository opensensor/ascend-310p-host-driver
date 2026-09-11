/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */
#ifndef XSMEM_NS_ADAPT_H
#define XSMEM_NS_ADAPT_H
#include "ka_task_pub.h"

int xsmem_ns_pool_num_inc(unsigned long mnt_ns, int algo);
void xsmem_ns_pool_num_dec(unsigned long mnt_ns, int algo);

int xsmem_get_tgid_by_vpid(ka_pid_t vpid, ka_pid_t *tgid);
int xsmem_strcat_with_ns(char *str_dest, unsigned int dest_max, const char *str_src);
int xsmem_blockid_get(unsigned long mnt_ns);
void xsmem_blockid_put(unsigned long mnt_ns, int id);

#endif
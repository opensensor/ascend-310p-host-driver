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
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/cgroup.h>
#include <linux/memcontrol.h>
#include "svm_proc_mng.h"
#include "svm_cgroup_mng.h"
#include "kernel_cgroup_mem_adapt.h"

ka_mem_cgroup_t *devmm_enable_cgroup(ka_mem_cgroup_t **memcg, ka_pid_t pid)
{
    return NULL;
}

void devmm_disable_cgroup(ka_mem_cgroup_t *memcg, ka_mem_cgroup_t *old_memcg)
{
}

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

#ifndef DEVMM_CGROUP_MNG_H
#define DEVMM_CGROUP_MNG_H

#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "devmm_addr_mng.h"

ka_mem_cgroup_t *devmm_enable_cgroup(ka_mem_cgroup_t **memcg, ka_pid_t pid);
void devmm_disable_cgroup(ka_mem_cgroup_t *memcg, ka_mem_cgroup_t *old_memcg);
bool devmm_can_get_continuity_page(void);

#endif /* DEVMM_CGROUP_MNG_H__ */

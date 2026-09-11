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
#ifndef SVM_MASTER_ADDR_MAP_H
#define SVM_MASTER_ADDR_MAP_H

#include "devmm_proc_info.h"
#include "svm_cmd.h"
#include "svm_shmem_interprocess.h"

int devmm_ioctl_map_dev_reserve(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg);

#endif /* SVM_MASTER_ADDR_MAP_H */


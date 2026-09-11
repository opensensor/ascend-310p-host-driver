/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-02-06
 */
#ifndef SVM_MASTER_DMA_DESC_MNG_H
#define SVM_MASTER_DMA_DESC_MNG_H

#include "devmm_proc_info.h"

void devmm_dma_desc_nodes_destroy_by_task_release(struct devmm_svm_process *svm_proc);
void devmm_dma_desc_stats_info_print(struct devmm_svm_process *svm_proc);

#endif /* SVM_MASTER_DMA_DESC_MNG_H */

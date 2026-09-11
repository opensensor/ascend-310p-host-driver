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

#ifndef SVM_RES_IDR_H
#define SVM_RES_IDR_H
#include "ka_base_pub.h"

struct svm_idr {
    ka_idr_t idr;
    int next_id;
    int max_id;
    ka_mutex_t lock;
};

typedef bool (*idr_remove_condition)(void *find_ptr);
void devmm_idr_init(struct svm_idr *idr, int max_id);
int devmm_idr_alloc(struct svm_idr *idr, char *ptr, int *id_out);
char *devmm_idr_get_and_remove(struct svm_idr *idr, int id, idr_remove_condition condition);
void devmm_idr_destroy(struct svm_idr *idr, void (*free_ptr)(const void *ptr));
bool devmm_idr_is_empty(struct svm_idr *idr);

#endif

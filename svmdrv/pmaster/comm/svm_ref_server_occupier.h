/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef SVM_REF_SERVER_OCCUPIER_H
#define SVM_REF_SERVER_OCCUPIER_H

#include "ka_base_pub.h"

struct devmm_ref_server_occupier {
    ka_rb_node_t node;
 
    u32 sdid;
    ka_atomic64_t occupy_num;
};

struct devmm_ref_server_occupier_mng {
    ka_rb_root_t root;
    ka_rw_semaphore_t rw_sem;
};

void devmm_ref_server_occupier_mng_init(struct devmm_ref_server_occupier_mng *mng);
void devmm_ref_server_occupier_mng_uninit(struct devmm_ref_server_occupier_mng *mng);

int devmm_ref_server_occupier_add(struct devmm_ref_server_occupier_mng *mng, u32 sdid);
int devmm_ref_server_occupier_del(struct devmm_ref_server_occupier_mng *mng, u32 sdid);
int devmm_for_each_ref_server_occupier(struct devmm_ref_server_occupier_mng *mng,
    int (*func)(struct devmm_ref_server_occupier *occupier, void *priv), void *priv);

#endif

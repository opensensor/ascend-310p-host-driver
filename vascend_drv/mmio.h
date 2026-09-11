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

#ifndef _DVT_MMIO_H_
#define _DVT_MMIO_H_
#include <linux/types.h>

struct hw_dvt;
struct hw_vdavinci;

#define DOORBELL_MAX    1024
#define DOORBELL_SIZE    4

typedef int (*dvt_mmio_func)(struct hw_vdavinci *, unsigned int,
                             void *, unsigned int);

enum MMIO_INFO_TYPE {
    DOORBELL = 0,
    MMIO_INFO_TYPE_MAX,
};

struct hw_dvt_mmio_info {
    enum MMIO_INFO_TYPE type;
    u32 offset;
    u32 end;
    u64 ro_mask;
    u32 align;
    dvt_mmio_func read;
    dvt_mmio_func write;
};

void hw_vdavinci_reset_mmio(struct hw_vdavinci *vdavinci);
#ifdef DAVINCI_TEST
int hw_vdavinci_310_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_310_mmio_uninit(struct hw_vdavinci *vdavinci);
#endif
int hw_vdavinci_310pro_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_310pro_mmio_uninit(struct hw_vdavinci *vdavinci);
int hw_vdavinci_910_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_910_mmio_uninit(struct hw_vdavinci *vdavinci);
int hw_vdavinci_910b_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_910b_mmio_uninit(struct hw_vdavinci *vdavinci);
int hw_vdavinci_910_93_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_910_93_mmio_uninit(struct hw_vdavinci *vdavinci);
int hw_vdavinci_950_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_950_mmio_uninit(struct hw_vdavinci *vdavinci);

int hw_vdavinci_910b_vf_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_910b_vf_mmio_uninit(struct hw_vdavinci *vdavinci);
int hw_vdavinci_910_93_vf_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_910_93_vf_mmio_uninit(struct hw_vdavinci *vdavinci);
int hw_vdavinci_950_vf_mmio_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_950_vf_mmio_uninit(struct hw_vdavinci *vdavinci);

int hw_dvt_set_mmio_device_info(struct hw_dvt *dvt);
#endif

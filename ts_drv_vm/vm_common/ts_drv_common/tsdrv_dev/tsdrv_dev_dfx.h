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

#ifndef TSDRV_DEV_DFX_H
#define TSDRV_DEV_DFX_H
#ifdef AOS_LLVM_BUILD
#include <linux/proc_fs.h>
#endif
#include "tsdrv_device.h"
#include "tsdrv_kernel_common.h"
#include "tsdrv_common.h"
/* tsid[17-16]--fid[15-11 bit]--devid[10-0 bit]  */
#define TSDRV_DFX_DEV_OFFSET    11U
#define TSDRV_DFX_DEV_MASK      0x7FFU
#define TSDRV_DFX_FID_OFFSET    5U
#define TSDRV_DFX_FID_MASK      0x1FU
#define TSDRV_DFX_TS_OFFSET     2U
#define TSDRV_DFX_TS_MASK       0x3U

#define TSDRV_DFX_NAME_LEN      20U
#ifndef AOS_LLVM_BUILD
struct tsdrv_dfx_fid {
    ka_dentry_t *fid_dentry; /* directory for fidX */
};

struct tsdrv_dfx_dev {
    u32 fid_num;
    ka_dentry_t *dev_dentry;
    struct tsdrv_dfx_fid dfx_fid[TSDRV_MAX_FID_NUM];
};

struct tsdrv_dfx {
    ka_mutex_t lock;
    u32 dev_num;
    ka_dentry_t *root_dentry;
    struct tsdrv_dfx_dev dfx_dev[TSDRV_MAX_DAVINCI_NUM];
};

struct tsdrv_dfx *tsdrv_get_dfx_root(void);
struct tsdrv_dfx_dev *tsdrv_get_dev_dfx(u32 devid);
struct tsdrv_dfx_fid *tsdrv_get_fid_dfx(u32 devid, u32 fid);
#else
#define TS_DRV_ATTR_RD (KA_S_IRUSR | KA_S_IRGRP)
struct tsdrv_proc_dfx_fid {
    ka_proc_dir_entry_t *fid_dentry; /* directory for fidX */
};

struct tsdrv_proc_dfx_dev {
    u32 fid_num;
    ka_proc_dir_entry_t *dev_dentry;
    struct tsdrv_proc_dfx_fid dfx_fid[TSDRV_MAX_FID_NUM];
};

struct tsdrv_proc_dfx {
    ka_mutex_t lock;
    u32 dev_num;
    ka_proc_dir_entry_t *root_dentry;
    struct tsdrv_proc_dfx_dev dfx_dev[TSDRV_MAX_DAVINCI_NUM];
};

struct proc_dfx_private_data {
    u32 dfx_id;
    u32 finish_flag;
};

struct tsdrv_proc_dfx *tsdrv_get_dfx_root(void);
struct tsdrv_proc_dfx_dev *tsdrv_get_dev_dfx(u32 devid);
struct tsdrv_proc_dfx_fid *tsdrv_get_fid_dfx(u32 devid, u32 fid);
#endif

int tsdrv_dev_dfx_init(void);
void tsdrv_dev_dfx_exit(void);

int tsdrv_dev_dfx_dir_create(u32 devid, u32 fid);
void tsdrv_dev_dfx_dir_remove(u32 devid, u32 fid);

static inline u32 tsdrv_pack_dfx_id(u32 devid, u32 fid, u32 tsid)
{
    return (tsid << (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) | (fid << TSDRV_DFX_DEV_OFFSET) | devid;
}

static inline u32 tsdrv_dfx_id_to_devid(u32 dfx_id)
{
    return dfx_id & TSDRV_DFX_DEV_MASK;
}

static inline u32 tsdrv_dfx_id_to_fid(u32 dfx_id)
{
    return (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;
}

static inline u32 tsdrv_dfx_id_to_tsid(u32 dfx_id)
{
    return (dfx_id >> (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) & TSDRV_DFX_TS_MASK;
}
#ifndef AOS_LLVM_BUILD
ssize_t simple_read_from_buffer_safe(void __user *to, size_t count, loff_t *ppos,
    const void *from, size_t available);
#else
ssize_t simple_read_from_buffer_safe(void __user *to, const void *from, size_t available);
#endif
#endif /* TSDRV_DEV_DFX_H */

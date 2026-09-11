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

#include <linux/stat.h>
#ifndef AOS_LLVM_BUILD
#include <linux/debugfs.h>
#endif
#include "securec.h"
#include "tsdrv_id_dfx.h"
#include "tsdrv_dev_dfx.h"
#include "tsdrv_device.h"
#include "tsdrv_event_id_dfx.h"
#include "tsdrv_id_config_dfx.h"
#ifndef AOS_LLVM_BUILD
int tsdrv_id_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_dfx_fid *dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    struct dentry *id_dir = NULL;
    int err;

    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }

    id_dir = ka_fs_debugfs_create_dir("id", dfx_fid->fid_dentry);
    if (id_dir == NULL) {
        TSDRV_PRINT_ERR("id dir create fail, devid=%u fid=%u\n", devid, fid);
        return -EFAULT;
    }

    err = event_id_dfx_create(devid, fid, id_dir);
    if (err != 0) {
        TSDRV_PRINT_ERR("event id dfx create fail, err=%d\n", err);
        return err;
    }

    err = id_config_dfx_create(devid, fid, id_dir);
    if (err != 0) {
        TSDRV_PRINT_ERR("id config dfx create fail, err=%d\n", err);
        return err;
    }
    return 0;
}
#else
int tsdrv_id_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_proc_dfx_fid *dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    ka_proc_dir_entry_t *id_dir = NULL;
    int err;

    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }

    id_dir = ka_fs_proc_mkdir("id", dfx_fid->fid_dentry);
    if (id_dir == NULL) {
        TSDRV_PRINT_ERR("id dir create fail, devid=%u fid=%u\n", devid, fid);
        return -EFAULT;
    }

    err = event_id_dfx_create(devid, fid, id_dir);
    if (err != 0) {
        TSDRV_PRINT_ERR("event id dfx create fail, err=%d\n", err);
        return err;
    }

    err = id_config_dfx_create(devid, fid, id_dir);
    if (err != 0) {
        TSDRV_PRINT_ERR("id config dfx create fail, err=%d\n", err);
        return err;
    }
    return 0;
}
#endif
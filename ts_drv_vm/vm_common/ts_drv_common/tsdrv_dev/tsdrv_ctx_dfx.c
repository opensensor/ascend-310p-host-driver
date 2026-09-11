/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#include <linux/slab.h>
#include <linux/atomic.h>

#include "securec.h"
#include "tsdrv_ctx_dfx.h"
#include "tsdrv_dev_dfx.h"
#include "tsdrv_device.h"
#include "tsdrv_ctx.h"
#include "tsdrv_common.h"

#define CTX_DFX_BUF_LEN        (10 * 1024)
#define CTX_DFX_BUF_MIN_LEN    30
#define CTX_DFX_MAX_OPEN_NUM 128

STATIC const char *ctx_status_str[TSDRV_CTX_STATUS_MAX] = {
    [TSDRV_CTX_INVALID] = "INVALID",
    [TSDRV_CTX_VALID] = "VALID",
    [TSDRV_CTX_RUN]  = "RUN",
    [TSDRV_CTX_WAIT] = "WAIT",
    [TSDRV_CTX_RELEASE] = "RELEASE",
    [TSDRV_CTX_RECYCLE] = "RECYCLE",
    [TSDRV_CTX_RECYCLE_ERR] = "RECYCLE_ERR",
    [TSDRV_CTX_STOP_RECYCLE] = "STOP_RECYCLE",
};

STATIC ka_atomic_t g_ctx_dfx_open_cnt = KA_BASE_ATOMIC_INIT(0);

struct ctx_dfx_private_data {
    u32 dfx_id;
    char *buff;
#ifdef AOS_LLVM_BUILD
    u32 finish_flag;
#endif
};

STATIC int tsdrv_ctx_dfx_open(ka_inode_t *inode, ka_file_t *filp)
{
    struct ctx_dfx_private_data *data = NULL;

    if (ka_base_atomic_inc_return(&g_ctx_dfx_open_cnt) > CTX_DFX_MAX_OPEN_NUM) {
        ka_base_atomic_dec(&g_ctx_dfx_open_cnt);
        TSDRV_PRINT_ERR("over max ctx dfx open num=%d\n", CTX_DFX_MAX_OPEN_NUM);
        return -ENODEV;
    }

    data = tsdrv_kzalloc(sizeof(struct ctx_dfx_private_data), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (data == NULL) {
        TSDRV_PRINT_ERR("data kzalloc failed\n");
        ka_base_atomic_dec(&g_ctx_dfx_open_cnt);
        return -ENODEV;
    }
#ifndef AOS_LLVM_BUILD
    data->dfx_id = (u32)(uintptr_t)inode->i_private;
#else
    data->dfx_id =  (u32)(uintptr_t)ka_fs_PDE_DATA(inode);
    data->finish_flag = 0;
#endif
    data->buff = tsdrv_kzalloc(CTX_DFX_BUF_LEN, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (data->buff == NULL) {
        TSDRV_PRINT_ERR("data->buff kzalloc failed\n");
        tsdrv_kfree(data);
        ka_base_atomic_dec(&g_ctx_dfx_open_cnt);
        return -ENODEV;
    }
    filp->private_data = (void *)data;

    return 0;
}

STATIC ssize_t tsdrv_ctx_dfx_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    struct ctx_dfx_private_data *data = (struct ctx_dfx_private_data *)file->private_data;
    u32 tsid = (data->dfx_id >> (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) & TSDRV_DFX_TS_MASK;
    u32 fid = (data->dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;
    u32 devid = data->dfx_id & TSDRV_DFX_DEV_MASK;
    struct tsdrv_device *tsdrv_dev = NULL;
    int buf_size = CTX_DFX_BUF_LEN;
    struct tsdrv_ctx *ctx = NULL;
    enum tsdrv_ctx_status status;
    char *str = data->buff;
    size_t output_len = 0;
    u32 ctx_num, idx, i;
    ka_pid_t tgid, pid;
    int len;
#ifdef AOS_LLVM_BUILD
    if (data->finish_flag) {
        TSDRV_PRINT_INFO("read finish. return 0\n");
        return 0;
    }
    data->finish_flag = 1;
#endif
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM) || (fid >= TSDRV_MAX_FID_NUM)) {
            return 0;
    }
    ctx_num = tsdrv_dev_get_ctx_num(devid, fid);
    len = snprintf_s(str, buf_size, buf_size - 1, "total:%3u        use:%3u         free:%3u \n\n",
        TSDRV_MAX_CTX_NUM, ctx_num, TSDRV_MAX_CTX_NUM - ctx_num);
    DRV_CHECK_EXP_ACT(len < 0, return 0, "snprintf_s fail, len(%d)\n", len);
    str += len;
    buf_size -= len;
    output_len += len;
    DRV_CHECK_EXP_ACT(buf_size < CTX_DFX_BUF_MIN_LEN, return 0, "buf_size(%d) err,line:%d\n", buf_size, __LINE__);

    len = snprintf_s(str, buf_size, buf_size - 1, "idx            tgid            pid            status\n");
    DRV_CHECK_EXP_ACT(len < 0, return 0, "snprintf_s fail, len(%d)\n", len);
    str += len;
    buf_size -= len;
    output_len += len;
    DRV_CHECK_EXP_ACT(buf_size < CTX_DFX_BUF_MIN_LEN, return 0, "buf_size(%d) err,line:%d\n", buf_size, __LINE__);

    tsdrv_dev = tsdrv_get_dev(devid);
    ctx = tsdrv_dev->dev_ctx[fid].ctx;

    for (i = 0; i < TSDRV_MAX_CTX_NUM; i++) {
        ka_task_spin_lock_bh(&ctx->ctx_lock);
        status = ka_base_atomic_read(&ctx[i].status);
        idx = ctx[i].ctx_index;
        tgid = ctx[i].tgid;
        pid = ctx[i].pid;
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        if ((status == TSDRV_CTX_INVALID) || (status >= TSDRV_CTX_STATUS_MAX)) {
            continue;
        }
        len = snprintf_s(str, buf_size, buf_size - 1, "%5u%16d%15d%18s\n", idx, tgid, pid, ctx_status_str[status]);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "snprintf_s fail, len(%d)\n", len);
        str += len;
        buf_size -= len;
        output_len += len;
        DRV_CHECK_EXP_ACT(buf_size < CTX_DFX_BUF_MIN_LEN, return 0, "buf_size(%d) err,line:%d\n", buf_size, __LINE__);
    }
#ifndef AOS_LLVM_BUILD
    return simple_read_from_buffer_safe(user_buf, count, ppos, data->buff, output_len);
#else
    return simple_read_from_buffer_safe(user_buf, data->buff, output_len);
#endif
}

STATIC int tsdrv_ctx_dfx_release(ka_inode_t *inode, ka_file_t *filep)
{
    struct ctx_dfx_private_data *data = (struct ctx_dfx_private_data *)filep->private_data;

    if (data == NULL) {
        TSDRV_PRINT_ERR("data is NULL.\n");
        return -ENOMEM;
    } else {
        if (data->buff != NULL) {
            tsdrv_kfree(data->buff);
        }
        tsdrv_kfree(data);
        ka_base_atomic_dec(&g_ctx_dfx_open_cnt);
        filep->private_data = NULL;
        return 0;
    }
}
#ifndef AOS_LLVM_BUILD
STATIC const ka_file_operations_t g_ctx_dbgfs_ops = {
    ka_fs_init_f_open(tsdrv_ctx_dfx_open)
    ka_fs_init_f_read(tsdrv_ctx_dfx_read)
    ka_fs_init_f_release(tsdrv_ctx_dfx_release)
};

int tsdrv_ctx_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_dfx_fid *dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    ka_dentry_t *ts_id_dentry[DEVDRV_MAX_TS_NUM] = {NULL};
    u32 dfx_id = (fid << TSDRV_DFX_DEV_OFFSET) | devid;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    char name[TSDRV_DFX_NAME_LEN] = {0};
    ka_dentry_t *ctx_dir = NULL;
    u32 tsid, i;
    int err;

    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }

    ctx_dir = ka_fs_debugfs_create_dir("ctx_status", dfx_fid->fid_dentry);
    if (ctx_dir == NULL) {
        TSDRV_PRINT_ERR("ctx_status dir create fail, devid=%u fid=%u\n", devid, fid);
        return -EFAULT;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = snprintf_s(name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "ctx_ts%u", tsid);
        if (err < 0) {
            TSDRV_PRINT_ERR("copy dfx name fail, err(%d)\n", err);
            goto ERR;
        }

        dfx_id = (tsid << (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) | (fid << TSDRV_DFX_DEV_OFFSET) | devid;
        ts_id_dentry[tsid] = ka_fs_debugfs_create_file(name, KA_S_IRUGO, ctx_dir, (void *)(uintptr_t)dfx_id, &g_ctx_dbgfs_ops);
        if (ts_id_dentry[tsid] == NULL) {
            TSDRV_PRINT_ERR("create status fail, devid(%u) fid(%u)", devid, fid);
            goto ERR;
        }
    }
    return 0;
ERR:
    for (i = 0; i < tsid; i++) {
        if (ts_id_dentry[i] != NULL) {
            ka_fs_debugfs_remove_recursive(ts_id_dentry[i]);
        }
    }

    return -ENODEV;
}
#else
ka_ssize_t tsdrv_ctx_proc_dfx_read(ka_file_t *file, char __user *buf, size_t count, loff_t *ppos)
{
    return tsdrv_ctx_dfx_read(file, buf, count, ppos);
}

STATIC const ka_file_operations_t g_ctx_proc_ops = {
    ka_fs_init_f_open = tsdrv_ctx_dfx_open
    ka_fs_init_f_read(tsdrv_ctx_proc_dfx_read)
    ka_fs_init_f_release(tsdrv_ctx_dfx_release)
};

int tsdrv_ctx_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_proc_dfx_fid *dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    ka_proc_dir_entry_t *ts_id_dentry[DEVDRV_MAX_TS_NUM] = {NULL};
    u32 dfx_id = (fid << TSDRV_DFX_DEV_OFFSET) | devid;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    char name[TSDRV_DFX_NAME_LEN] = {0};
    ka_proc_dir_entry_t *ctx_dir = NULL;
    u32 tsid, i;
    int err;

    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }

    ctx_dir = ka_fs_proc_mkdir("ctx_status", dfx_fid->fid_dentry);
    if (ctx_dir == NULL) {
        TSDRV_PRINT_ERR("ctx_status dir create fail, devid=%u fid=%u\n", devid, fid);
        return -EFAULT;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = snprintf_s(name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "ctx_ts%u", tsid);
        if (err < 0) {
            TSDRV_PRINT_ERR("copy dfx name fail, err(%d)\n", err);
            goto ERR;
        }
        dfx_id = (tsid << (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) | (fid << TSDRV_DFX_DEV_OFFSET) | devid;
        ts_id_dentry[tsid] = ka_fs_proc_create_data(name, TS_DRV_ATTR_RD, ctx_dir, &g_ctx_proc_ops,
                                              (void *)(uintptr_t)dfx_id);
        if (ts_id_dentry[tsid] == NULL) {
            TSDRV_PRINT_ERR("create status fail, devid(%u) fid(%u)", devid, fid);
            goto ERR;
        }
    }
    return 0;
ERR:
    for (i = 0; i < tsid; i++) {
        if (ts_id_dentry[i] != NULL) {
            ka_fs_proc_remove(ts_id_dentry[i]);
        }
    }
    ka_fs_proc_remove(ctx_dir);
    return -ENODEV;
}
#endif

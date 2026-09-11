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
#ifndef AOS_LLVM_BUILD
#include <linux/debugfs.h>
#endif

#include "ka_task_pub.h"
#include "ka_fs_pub.h"
#include "securec.h"
#include "tsdrv_event_id_dfx.h"

struct event_dfx_array {
    ka_pid_t tgid;
    ka_pid_t pid;
    u32 event_id;
};

struct event_dfx_data {
    void *buf;
    u32 dfx_id;

    u32 head;
    struct event_dfx_array *event;
#ifdef AOS_LLVM_BUILD
    u32 finish_flag;
#endif
};

#define EVENT_ID_DFX_MAX_CTX_NUM 5
#define EVENT_ID_DFX_MIN_LEN 50
#define EVENT_ID_DFX_BUF_SIZE (50 * (DEVDRV_MAX_SW_EVENT_ID))

static ka_atomic_t g_event_id_dfx_max_ctx = KA_BASE_ATOMIC_INIT(0);

STATIC int event_id_dfx_open(ka_inode_t *inode, ka_file_t *filp);
STATIC int event_id_dfx_release(ka_inode_t *inode, ka_file_t *filep);
STATIC ssize_t event_id_dfx_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos);

#ifdef AOS_LLVM_BUILD
ssize_t event_id_proc_dfx_read(ka_file_t *file, char __user *buf, size_t count, loff_t *ppos)
{
    return event_id_dfx_read(file, buf, count, ppos);
}
#endif

STATIC const ka_file_operations_t event_id_dfx_ops = {
    ka_fs_init_f_open(event_id_dfx_open)
#ifndef AOS_LLVM_BUILD
    ka_fs_init_f_read(event_id_dfx_read)
#else
    ka_fs_init_f_read(event_id_proc_dfx_read)
#endif
    ka_fs_init_f_release(event_id_dfx_release)
};

STATIC int event_id_dfx_open(ka_inode_t *inode, ka_file_t *filp)
{
    struct event_dfx_data *data = NULL;

    if (ka_base_atomic_inc_return(&g_event_id_dfx_max_ctx) > EVENT_ID_DFX_MAX_CTX_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("max event dfx ctx num=%d\n", EVENT_ID_DFX_MAX_CTX_NUM);
        goto err_ctx_cnt_inc;
#endif
    }
    data = tsdrv_vmalloc(sizeof(struct event_dfx_data), KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (data == NULL) {
#ifndef TSDRV_UT
        goto err_data_alloc;
#endif
    }
    data->event = tsdrv_vmalloc(sizeof(struct event_dfx_array) * DEVDRV_MAX_SW_EVENT_ID,
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (data->event == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("event id dfx kmalloc fail\n");
        goto err_data_event_alloc;
#endif
    }
    data->buf = tsdrv_vmalloc(EVENT_ID_DFX_BUF_SIZE, KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (data->buf == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("alloc output buf fail\n");
        goto err_data_buf_alloc;
#endif
    }
    data->head = 0;
#ifndef AOS_LLVM_BUILD
    data->dfx_id = (u32)(uintptr_t)inode->i_private;
#else
    data->dfx_id =  (u32)(uintptr_t)ka_fs_PDE_DATA(inode);
    data->finish_flag = 0;
#endif
    filp->private_data = data;
    return 0;
#ifndef TSDRV_UT
err_data_buf_alloc:
    tsdrv_vfree(data->event);
    data->event = NULL;
err_data_event_alloc:
    tsdrv_vfree(data);
err_data_alloc:
err_ctx_cnt_inc:
    ka_base_atomic_dec(&g_event_id_dfx_max_ctx);
    return -ENOMEM;
#endif
}

STATIC int event_id_dfx_release(ka_inode_t *inode, ka_file_t *file)
{
    struct event_dfx_data *data = (struct event_dfx_data *)file->private_data;

    if (data != NULL) {
        if (data->buf != NULL) {
            tsdrv_vfree(data->buf);
            data->buf = NULL;
        }
        if (data->event != NULL) {
            tsdrv_vfree(data->event);
            data->event = NULL;
        }
        tsdrv_vfree(data);
    }
    ka_base_atomic_dec(&g_event_id_dfx_max_ctx);
    return 0;
}

static void event_id_dfx_collect_ctx(struct event_dfx_data * data, struct tsdrv_ctx *ctx, u32 tsnum)
{
    enum tsdrv_ctx_status status;
    u32 tsid;

    ka_task_spin_lock_bh(&ctx->ctx_lock);
    status = ka_base_atomic_read(&ctx->status);
    if (status == TSDRV_CTX_INVALID) {
        ka_task_spin_unlock_bh(&ctx->ctx_lock);
        return;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        ka_list_head_t *p = NULL, *n = NULL;

        if (ka_list_empty_careful(&ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_list) != 0) {
            continue;
        }

        ka_list_for_each_safe(p, n, &ctx->ts_ctx[tsid].id_ctx[TSDRV_EVENT_SW_ID].id_list) {
            struct tsdrv_id_info *id_info = ka_list_entry(p, struct tsdrv_id_info, list);
#ifndef TSDRV_UT
            if (data->head >= DEVDRV_MAX_SW_EVENT_ID) {
                break;
            }
#endif
            data->event[data->head].tgid = ctx->tgid;
            data->event[data->head].pid = ctx->pid;
            data->event[data->head].event_id = id_info->id;
            data->head++;
        }
    }
    ka_task_spin_unlock_bh(&ctx->ctx_lock);
}

static void event_id_dfx_collect_all(struct event_dfx_data *data)
{
    u32 i;

    for (i = 0; i < TSDRV_MAX_CTX_NUM; i++) {
        u32 dfx_id = data->dfx_id;
        u32 devid = tsdrv_dfx_id_to_devid(dfx_id);
        u32 fid = tsdrv_dfx_id_to_fid(dfx_id);
        u32 tsnum = tsdrv_get_dev_tsnum(devid);
        struct tsdrv_device *tsdrv_dev = NULL;
        struct tsdrv_ctx *ctx = NULL;

        tsdrv_dev = tsdrv_get_dev(devid);
        ctx = &tsdrv_dev->dev_ctx[fid].ctx[i];
        event_id_dfx_collect_ctx(data, ctx, tsnum);
    }
}

static int event_id_dfx_out_buf_prepare(struct event_dfx_data *data)
{
    int buf_size = EVENT_ID_DFX_BUF_SIZE;
    char *str = data->buf;
    int out_len = 0;
    int len;

    len = snprintf_s(str, buf_size, buf_size - 1, "[tgid]:[pid]:[id]\n");
    DRV_CHECK_EXP_ACT(len < 0, return out_len, "");
    str += len;
    buf_size -= len;
    out_len += len;
    DRV_CHECK_EXP_ACT(buf_size < EVENT_ID_DFX_MIN_LEN, return out_len, "");

    while (data->head != 0) {
        data->head--;
        if (data->head >= DEVDRV_MAX_SW_EVENT_ID) {
            TSDRV_PRINT_ERR("invalid data->head=%u\n", data->head);
            return out_len;
        }
#ifndef TSDRV_UT
        len = snprintf_s(str, buf_size, buf_size - 1, "[%d]:[%d]:[%u]\n", data->event[data->head].tgid,
            data->event[data->head].pid, data->event[data->head].event_id);
#endif
        if (len < 0) {
            break;
        }
        str += len;
        buf_size -= len;
        out_len += len;
        if (buf_size < EVENT_ID_DFX_MIN_LEN) {
            break;
        }
    }
    return out_len;
}

STATIC ssize_t event_id_dfx_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    struct event_dfx_data *data = (struct event_dfx_data *)file->private_data;
    int out_len;
#ifdef AOS_LLVM_BUILD
    if (data->finish_flag) {
        TSDRV_PRINT_INFO("read finish. return 0\n");
        return 0;
    }
    data->finish_flag = 1;
#endif
    event_id_dfx_collect_all(data);
    out_len = event_id_dfx_out_buf_prepare(data);
#ifndef AOS_LLVM_BUILD
    return simple_read_from_buffer_safe(user_buf, count, ppos, data->buf, out_len);
#else
    return simple_read_from_buffer_safe(user_buf, data->buf, out_len);
#endif
}
#ifndef AOS_LLVM_BUILD
static int event_id_dfx_ts_create(u32 devid, u32 fid, ka_dentry_t *parent)
{
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        char name[TSDRV_DFX_NAME_LEN] = {0};
        ka_dentry_t *event_id = NULL;
        u32 dfx_id;
        int err;

        err = snprintf_s(name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "event_id_ts%u", tsid);
        if (err < 0) {
            return -EFAULT;
        }
        dfx_id = tsdrv_pack_dfx_id(devid, fid, tsid);
        event_id = ka_fs_debugfs_create_file(name, KA_S_IRUGO, parent, (void *)(uintptr_t)dfx_id, &event_id_dfx_ops);
        if (event_id == NULL) {
            return -ENOMEM;
        }
    }
    return 0;
}

int event_id_dfx_create(u32 devid, u32 fid, ka_dentry_t *parent)
{
    DRV_CHECK_EXP_ACT(devid >= TSDRV_MAX_DAVINCI_NUM, return -ENODEV, "invalid devid=%u\n", devid);
    DRV_CHECK_EXP_ACT(fid >= TSDRV_MAX_FID_NUM, return -ENODEV, "invalid fid=%u\n", fid);
    DRV_CHECK_PTR(parent, return -ENOMEM, "parent is NULL\n");

    return event_id_dfx_ts_create(devid, fid, parent);
}

#else
static int event_id_dfx_ts_create(u32 devid, u32 fid, ka_proc_dir_entry_t *parent)
{
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        char name[TSDRV_DFX_NAME_LEN] = {0};
        ka_proc_dir_entry_t *event_id = NULL;
        u32 dfx_id;
        int err;

        err = snprintf_s(name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "event_id_ts%u", tsid);
        if (err < 0) {
            return -EFAULT;
        }
        dfx_id = tsdrv_pack_dfx_id(devid, fid, tsid);
        event_id = ka_fs_proc_create_data(name, TS_DRV_ATTR_RD, parent, &event_id_dfx_ops, (void *)(uintptr_t)dfx_id);
        if (event_id == NULL) {
            return -ENOMEM;
        }
    }
    return 0;
}

int event_id_dfx_create(u32 devid, u32 fid, ka_proc_dir_entry_t *parent)
{
    DRV_CHECK_EXP_ACT(devid >= TSDRV_MAX_DAVINCI_NUM, return -ENODEV, "invalid devid=%u\n", devid);
    DRV_CHECK_EXP_ACT(fid >= TSDRV_MAX_FID_NUM, return -ENODEV, "invalid fid=%u\n", fid);
    DRV_CHECK_PTR(parent, return -ENOMEM, "parent is NULL\n");

    return event_id_dfx_ts_create(devid, fid, parent);
}
#endif

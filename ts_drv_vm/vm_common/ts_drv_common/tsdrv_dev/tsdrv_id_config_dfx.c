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

#include <linux/types.h>
#ifndef AOS_LLVM_BUILD
#include <linux/debugfs.h>
#endif
#include "securec.h"
#include "tsdrv_id.h"
#include "logic_cq.h"
#include "tsdrv_interface.h"
#include "tsdrv_id_config_dfx.h"
#include "ka_task_pub.h"
#include "ka_fs_pub.h"

#define TSDRV_DFX_ID_BUF_LEN    512U
#define TSDRV_DFX_ID_BUF_MINI_LEN 30

STATIC int id_config_dfx_open(ka_inode_t *inode, ka_file_t *filp);
STATIC ssize_t id_config_dfx_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos);

static const char *avail_id_name[TSDRV_MAX_ID] = {
    [TSDRV_STREAM_ID] = "avail_stream",
    [TSDRV_NOTIFY_ID] = "avail_notify",
    [TSDRV_MODEL_ID] = "avail_model",
    [TSDRV_EVENT_SW_ID] = "avail_event",
    [TSDRV_EVENT_HW_ID] = "avail_hw_event",
    [TSDRV_SQ_ID] = "avail_sq",
    [TSDRV_CQ_ID] = "avail_cq",
    [TSDRV_CMO_ID] = "avail_cmo"
};

static const char *cur_id_name[TSDRV_MAX_ID] = {
    [TSDRV_STREAM_ID] = "cur_stream",
    [TSDRV_NOTIFY_ID] = "cur_notify",
    [TSDRV_MODEL_ID] = "cur_model",
    [TSDRV_EVENT_SW_ID] = "cur_event",
    [TSDRV_EVENT_HW_ID] = "cur_hw_event",
    [TSDRV_SQ_ID] = "cur_sq",
    [TSDRV_CQ_ID] = "cur_cq",
    [TSDRV_CMO_ID] = "cur_cmo"
};
#ifndef AOS_LLVM_BUILD
STATIC const ka_file_operations_t id_config_dfx_ops = {
    ka_fs_init_f_open(id_config_dfx_open)
    ka_fs_init_f_read(id_config_dfx_read)
    ka_fs_init_f_release(NULL)
};

STATIC int id_config_dfx_open(ka_inode_t *inode, ka_file_t *filp)
{
    u32 dfx_id = (u32)(uintptr_t)inode->i_private;
    u32 devid = tsdrv_dfx_id_to_devid(dfx_id);
    u32 tsid = tsdrv_dfx_id_to_tsid(dfx_id);
    u32 fid = tsdrv_dfx_id_to_fid(dfx_id);

    TSDRV_PRINT_DEBUG("devid(%u) fid(%u) tsid(%u)\n", devid, fid, tsid);

    filp->private_data = (void *)(uintptr_t)dfx_id;

    return 0;
}
STATIC ssize_t id_config_dfx_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    u32 dfx_id = (u32)(uintptr_t)file->private_data;
    u32 devid = tsdrv_dfx_id_to_devid(dfx_id);
    u32 tsid = tsdrv_dfx_id_to_tsid(dfx_id);
    u32 fid = tsdrv_dfx_id_to_fid(dfx_id);
    struct tsdrv_ts_resource *ts_res = NULL;
    char buf[TSDRV_DFX_ID_BUF_LEN] = {0};
    s32 buf_size = TSDRV_DFX_ID_BUF_LEN;
    u32 cur_num, event_num;
    size_t output_len = 0;
    char *str = buf;
    int len, i;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);

    for (i = (int)TSDRV_STREAM_ID; i < (int)TSDRV_MAX_ID; i++) {
        if (i == (int)TSDRV_EVENT_HW_ID || i == (int)TSDRV_EVENT_SW_ID) {
            continue;
        }
        len = snprintf_s(str, buf_size, buf_size - 1, "%s=%u\n",
            avail_id_name[i], ts_res->id_res[i].id_available_num);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "%s print fail, len(%d)\n", avail_id_name[i], len);
        str += len;
        buf_size -= len;
        output_len += len;
        DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
            __LINE__, buf_size);
    }

#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
    len = snprintf_s(str, (size_t)buf_size, buf_size - 1, "avail_logic_cq=%u\n", logic_cq_num_get(devid, fid, tsid));
    DRV_CHECK_EXP_ACT(len < 0, return 0, "logic_cq print fail, len(%d)\n", len);
    str += len;
    buf_size -= len;
    output_len += len;

    DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err, line:%d, buf_size=(%d)\n",
        __LINE__, buf_size);
#endif

    len = snprintf_s(str, (size_t)buf_size, buf_size - 1, "avail_event=%u\n",
        ts_res->id_res[TSDRV_EVENT_HW_ID].id_available_num +
        ts_res->id_res[TSDRV_EVENT_SW_ID].id_available_num);
    DRV_CHECK_EXP_ACT(len < 0, return 0, "avail_event print fail, len(%d)\n", len);
    str += len;
    buf_size -= len;
    output_len += len;

    if (fid == 0) {
        int err;

        for (i = (int)TSDRV_STREAM_ID; i < (int)TSDRV_MAX_ID; i++) {
            if (i == (int)TSDRV_EVENT_HW_ID || i == (int)TSDRV_EVENT_SW_ID) {
                continue;
            }
            DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
                __LINE__, buf_size);
            err = tsdrv_get_ts_id_cur_num(devid, tsid, i, &cur_num);
            DRV_CHECK_EXP_ACT(err != 0, return 0, "get cur_stream_num fail, err(%d)\n", err);
            len = snprintf_s(str, buf_size, buf_size - 1, "%s=%u\n", cur_id_name[i], cur_num);
            DRV_CHECK_EXP_ACT(len < 0, return 0, "%s print fail, len(%d)\n", cur_id_name[i], len);
            str += len;
            buf_size -= len;
            output_len += len;
        }

        DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
            __LINE__, buf_size);
        err = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_HW_ID, &cur_num);
        DRV_CHECK_EXP_ACT(err != 0, return 0, "get cur_hw_event_num fail, err(%d)\n", err);
        err = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_SW_ID, &event_num);
        DRV_CHECK_EXP_ACT(err != 0, return 0, "get cur_sw_event_num fail, err(%d)\n", err);
        len = snprintf_s(str, buf_size, buf_size - 1, "cur_event=%u\n", cur_num + event_num);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "cur_event print fail, len(%d)\n", len);
        str += len;
        buf_size -= len;
        output_len += len;
    } else {
        for (i = (int)TSDRV_STREAM_ID; i < (int)TSDRV_MAX_ID; i++) {
            if (i == (int)TSDRV_EVENT_HW_ID || i == (int)TSDRV_EVENT_SW_ID) {
                continue;
            }
            DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
                __LINE__, buf_size);
#ifndef TSDRV_UT
            len = snprintf_s(str, buf_size, buf_size - 1, "%s=%u\n", cur_id_name[i],
                ts_res->id_res[i].id_capacity);
#endif
            DRV_CHECK_EXP_ACT(len < 0, return 0, "%s print fail, len(%d)\n", cur_id_name[i], len);
            str += len;
            buf_size -= len;
            output_len += len;
        }

        DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
            __LINE__, buf_size);
        len = snprintf_s(str, (size_t)buf_size, buf_size - 1, "cur_event=%u\n",
            ts_res->id_res[TSDRV_EVENT_SW_ID].id_capacity + ts_res->id_res[TSDRV_EVENT_HW_ID].id_capacity);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "cur_event print fail, len(%d)\n", len);
        str += len;
        buf_size -= len;
        output_len += len;
    }
    return simple_read_from_buffer_safe(user_buf, count, ppos, buf, output_len);
}

static int id_config_dfx_ts_create(u32 devid, u32 fid, ka_dentry_t *parent)
{
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        char name[TSDRV_DFX_NAME_LEN] = {0};
        ka_dentry_t *id_config = NULL;
        u32 dfx_id;
        int err;

        err = snprintf_s(name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "id_config_ts%u", tsid);
        if (err < 0) {
            TSDRV_PRINT_ERR("copy dfx id name fail, err(%d)\n", err);
            return -EFAULT;
        }
        dfx_id = tsdrv_pack_dfx_id(devid, fid, tsid);
        id_config  = ka_fs_debugfs_create_file(name, KA_S_IRUGO, parent, (void *)(uintptr_t)dfx_id, &id_config_dfx_ops);
        if (id_config == NULL) {
            TSDRV_PRINT_ERR("create status fail, devid(%u) fid(%u)", devid, fid);
            return -ENODEV;
        }
    }
    return 0;
}

int id_config_dfx_create(u32 devid, u32 fid, ka_dentry_t *parent)
{
    DRV_CHECK_EXP_ACT(devid >= TSDRV_MAX_DAVINCI_NUM, return -ENODEV, "invalid devid=%u\n", devid);
    DRV_CHECK_EXP_ACT(fid >= TSDRV_MAX_FID_NUM, return -ENODEV, "invalid fid=%u\n", fid);
    DRV_CHECK_PTR(parent, return -ENOMEM, "parent is NULL\n");

    return id_config_dfx_ts_create(devid, fid, parent);
}
#else
STATIC int id_config_proc_dfx_open(ka_inode_t *inode, ka_file_t *filp)
{
    struct proc_dfx_private_data *data = tsdrv_kzalloc(sizeof(struct proc_dfx_private_data), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (data == NULL) {
        TSDRV_PRINT_ERR("data kzalloc failed\n");
        return -ENODEV;
    }
    data->dfx_id = (u32)(uintptr_t)ka_base_pde_data(inode);
    u32 devid = tsdrv_dfx_id_to_devid(data->dfx_id);
    u32 tsid = tsdrv_dfx_id_to_tsid(data->dfx_id);
    u32 fid = tsdrv_dfx_id_to_fid(data->dfx_id);

    TSDRV_PRINT_INFO("devid(%u) fid(%u) tsid(%u)\n", devid, fid, tsid);
    data->finish_flag = 0;
    filp->private_data = (void *)(uintptr_t)data;
    return 0;
}

ssize_t id_config_proc_dfx_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    struct proc_dfx_private_data *data = (struct proc_dfx_private_data *)file->private_data;
    u32 dfx_id = data->dfx_id;
    u32 devid = tsdrv_dfx_id_to_devid(dfx_id);
    u32 tsid = tsdrv_dfx_id_to_tsid(dfx_id);
    u32 fid = tsdrv_dfx_id_to_fid(dfx_id);
    struct tsdrv_ts_resource *ts_res = NULL;
    char buf[TSDRV_DFX_ID_BUF_LEN] = {0};
    s32 buf_size = TSDRV_DFX_ID_BUF_LEN;
    u32 cur_num, event_num;
    size_t output_len = 0;
    char *str = buf;
    int len, i;

    if (data->finish_flag) {
        TSDRV_PRINT_INFO("read finish. return 0\n");
        return 0;
    }
    data->finish_flag = 1;

    ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);

    for (i = (int)TSDRV_STREAM_ID; i < (int)TSDRV_MAX_ID; i++) {
        if (i == (int)TSDRV_EVENT_HW_ID || i == (int)TSDRV_EVENT_SW_ID) {
            continue;
        }
        len = snprintf_s(str, buf_size, buf_size - 1, "%s=%u\n",
            avail_id_name[i], ts_res->id_res[i].id_available_num);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "%s print fail, len(%d)\n", avail_id_name[i], len);
        str += len;
        buf_size -= len;
        output_len += len;
        DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
            __LINE__, buf_size);
    }

#ifdef CFG_FEATURE_RUNTIME_NO_THREAD
    len = snprintf_s(str, (size_t)buf_size, buf_size - 1, "avail_logic_cq=%u\n", logic_cq_num_get(devid, fid, tsid));
    DRV_CHECK_EXP_ACT(len < 0, return 0, "logic_cq print fail, len(%d)\n", len);
    str += len;
    buf_size -= len;
    output_len += len;

    DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err, line:%d, buf_size=(%d)\n",
        __LINE__, buf_size);
#endif

    len = snprintf_s(str, (size_t)buf_size, buf_size - 1, "avail_event=%u\n",
        ts_res->id_res[TSDRV_EVENT_HW_ID].id_available_num +
        ts_res->id_res[TSDRV_EVENT_SW_ID].id_available_num);
    DRV_CHECK_EXP_ACT(len < 0, return 0, "avail_event print fail, len(%d)\n", len);
    str += len;
    buf_size -= len;
    output_len += len;

    if (fid == 0) {
        int err;

        for (i = (int)TSDRV_STREAM_ID; i < (int)TSDRV_MAX_ID; i++) {
            if (i == (int)TSDRV_EVENT_HW_ID || i == (int)TSDRV_EVENT_SW_ID) {
                continue;
            }
            DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
                __LINE__, buf_size);
            err = tsdrv_get_ts_id_cur_num(devid, tsid, i, &cur_num);
            DRV_CHECK_EXP_ACT(err != 0, return 0, "get cur_stream_num fail, err(%d)\n", err);
            len = snprintf_s(str, buf_size, buf_size - 1, "%s=%u\n", cur_id_name[i], cur_num);
            DRV_CHECK_EXP_ACT(len < 0, return 0, "%s print fail, len(%d)\n", cur_id_name[i], len);
            str += len;
            buf_size -= len;
            output_len += len;
        }

        DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
            __LINE__, buf_size);
        err = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_HW_ID, &cur_num);
        DRV_CHECK_EXP_ACT(err != 0, return 0, "get cur_hw_event_num fail, err(%d)\n", err);
        err = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_SW_ID, &event_num);
        DRV_CHECK_EXP_ACT(err != 0, return 0, "get cur_sw_event_num fail, err(%d)\n", err);
        len = snprintf_s(str, buf_size, buf_size - 1, "cur_event=%u\n", cur_num + event_num);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "cur_event print fail, len(%d)\n", len);
        str += len;
        buf_size -= len;
        output_len += len;
    } else {
        for (i = (int)TSDRV_STREAM_ID; i < (int)TSDRV_MAX_ID; i++) {
            if (i == (int)TSDRV_EVENT_HW_ID || i == (int)TSDRV_EVENT_SW_ID) {
                continue;
            }
            DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
                __LINE__, buf_size);
            len = snprintf_s(str, buf_size, buf_size - 1, "%s=%u\n", cur_id_name[i],
                ts_res->id_res[i].id_capacity);
            DRV_CHECK_EXP_ACT(len < 0, return 0, "%s print fail, len(%d)\n", cur_id_name[i], len);
            str += len;
            buf_size -= len;
            output_len += len;
        }

        DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_ID_BUF_MINI_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
            __LINE__, buf_size);
        len = snprintf_s(str, (size_t)buf_size, buf_size - 1, "cur_event=%u\n",
            ts_res->id_res[TSDRV_EVENT_SW_ID].id_capacity + ts_res->id_res[TSDRV_EVENT_HW_ID].id_capacity);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "cur_event print fail, len(%d)\n", len);
        str += len;
        buf_size -= len;
        output_len += len;
    }

    return simple_read_from_buffer_safe(user_buf, buf, output_len);
}

STATIC int id_config_proc_dfx_release(ka_inode_t *inode, ka_file_t *filep)
{
    struct proc_dfx_private_data *data = (struct proc_dfx_private_data *)filep->private_data;
    TSDRV_PRINT_INFO("release");
    if (data == NULL) {
        TSDRV_PRINT_ERR("data is NULL.\n");
        return -ENOMEM;
    } else {
        tsdrv_kfree(data);
        filep->private_data = NULL;
        return 0;
    }
}

STATIC const ka_file_operations_t id_config_proc_dfx_ops = {
    ka_fs_init_f_open(id_config_proc_dfx_open)
    ka_fs_init_f_read(id_config_proc_dfx_read)
    ka_fs_init_f_release(id_config_proc_dfx_release)
};

static int id_config_dfx_ts_create(u32 devid, u32 fid, ka_proc_dir_entry_t *parent)
{
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        char name[TSDRV_DFX_NAME_LEN] = {0};
        ka_proc_dir_entry_t *id_config = NULL;
        u32 dfx_id;
        int err;

        err = snprintf_s(name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "id_config_ts%u", tsid);
        if (err < 0) {
            TSDRV_PRINT_ERR("copy dfx id name fail, err(%d)\n", err);
            return -EFAULT;
        }
        dfx_id = tsdrv_pack_dfx_id(devid, fid, tsid);
        id_config  = ka_fs_proc_create_data(name, TS_DRV_ATTR_RD, parent, &id_config_proc_dfx_ops, (void *)(uintptr_t)dfx_id);
        if (id_config == NULL) {
            TSDRV_PRINT_ERR("create status fail, devid(%u) fid(%u)", devid, fid);
            return -ENODEV;
        }
    }
    return 0;
}

int id_config_dfx_create(u32 devid, u32 fid, ka_proc_dir_entry_t *parent)
{
    DRV_CHECK_EXP_ACT(devid >= TSDRV_MAX_DAVINCI_NUM, return -ENODEV, "invalid devid=%u\n", devid);
    DRV_CHECK_EXP_ACT(fid >= TSDRV_MAX_FID_NUM, return -ENODEV, "invalid fid=%u\n", fid);
    DRV_CHECK_PTR(parent, return -ENOMEM, "parent is NULL\n");

    return id_config_dfx_ts_create(devid, fid, parent);
}
#endif

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
#include "securec.h"
#include "tsdrv_mailbox_dfx.h"
#include "tsdrv_dev_dfx.h"
#include "tsdrv_device.h"
#include "devdrv_mailbox.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"

#define TSDRV_DFX_MBOX_BUF_LEN        512U
#define TSDRV_DFX_MBOX_BUF_MIN_LEN    30

struct tsdrv_mailbox_dfx *mbox_dfx_record[DEVDRV_MAX_DAVINCI_NUM][DEVDRV_MAX_TS_NUM];

struct tsdrv_mailbox_dfx *tsdrv_get_mbox_dfx_addr(u32 devid, u32 tsid)
{
    return mbox_dfx_record[devid][tsid];
}

int tsdrv_create_mbox_dfx_addr(u32 devid, u32 tsid)
{
    struct tsdrv_mailbox_dfx *mbox_dfx = NULL;

    if (mbox_dfx_record[devid][tsid] != NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Repeat create mbox dfx addr. (devid=%u; tsid=%u)\n", devid, tsid);
        return -EINVAL;
#endif
    }

    mbox_dfx = (struct tsdrv_mailbox_dfx *)tsdrv_vzalloc(sizeof(struct tsdrv_mailbox_dfx));
    if (mbox_dfx == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Alloc mbox dfx addr failed. (devid=%u; tsid=%u)\n", devid, tsid);
        return -ENOMEM;
#endif
    }

    mbox_dfx_record[devid][tsid] = mbox_dfx;
    return 0;
}

void tsdrv_destroy_mbox_dfx_addr(u32 devid, u32 tsid)
{
    if (mbox_dfx_record[devid][tsid] == NULL) {
        return;
    }

    tsdrv_vfree(mbox_dfx_record[devid][tsid]);
    mbox_dfx_record[devid][tsid] = NULL;
}

STATIC int tsdrv_dfx_mbox_open(ka_inode_t *inode, ka_file_t *filp)
{
    u32 dfx_id = (u32)(uintptr_t)inode->i_private;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    u32 tsid = (dfx_id >> (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) & TSDRV_DFX_TS_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;

    TSDRV_PRINT_INFO("devid(%u) fid(%u) tsid(%u)\n", devid, fid, tsid);

    filp->private_data = (void *)(uintptr_t)dfx_id;

    return 0;
}

STATIC ssize_t tsdrv_dfx_mbox_read(ka_file_t *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    u32 dfx_id = (u32)(uintptr_t)file->private_data;
    u32 tsid = (dfx_id >> (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) & TSDRV_DFX_TS_MASK;
    u32 fid = (dfx_id >> TSDRV_DFX_DEV_OFFSET) & TSDRV_DFX_FID_MASK;
    struct tsdrv_mailbox_dfx *mbox_dfx = NULL;
    u32 devid = dfx_id & TSDRV_DFX_DEV_MASK;
    char buf[TSDRV_DFX_MBOX_BUF_LEN] = {0};
    s32 buf_size = TSDRV_DFX_MBOX_BUF_LEN;
    size_t output_len = 0;
    char *str = buf;
    int len, i, tmp;

    TSDRV_PRINT_INFO("devid(%u) fid(%u) tsid(%u)\n", devid, fid, tsid);

    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        return 0;
    }

    mbox_dfx = tsdrv_get_mbox_dfx_addr(devid, tsid);
    if (fid == 0) {
        len = snprintf_s(str, buf_size, buf_size - 1, "mailbox_dfx: total send(%lu),rec(%lu),lost(%lu),"
            "irqnum(%lu),upnum(%lu),free(%u)\n",
            mbox_dfx->send_num, mbox_dfx->receive_num, mbox_dfx->lost_num,
            mbox_dfx->irq_num, mbox_dfx->up_num, mbox_dfx->free_num);
        DRV_CHECK_EXP_ACT(len < 0, return 0, "snprintf_s fail, len(%d)\n", len);
        str += len;
        buf_size -= len;
        output_len += len;
        DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_MBOX_BUF_MIN_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
            __LINE__, buf_size);

        tmp = (mbox_dfx->free_num <= MAILBOX_DFX_COUNT) ? mbox_dfx->free_num : MAILBOX_DFX_COUNT;
        for (i = 0; i < tmp; i++) {
#ifndef TSDRV_UT
            len = snprintf_s(str, buf_size, buf_size - 1, "index:%d [%llu us],free_type(%u),free_valid(%u)\n",
                i, mbox_dfx->free_time[i],
                mbox_dfx->free_type[i], mbox_dfx->free_valid[i]);
#endif
            DRV_CHECK_EXP_ACT(len < 0, return 0, "snprintf_s fail, len(%d)\n", len);
            str += len;
            buf_size -= len;
            output_len += len;
            DRV_CHECK_EXP_ACT(buf_size < TSDRV_DFX_MBOX_BUF_MIN_LEN, return 0, "buf_size err,line:%d, buf_size=(%d)\n",
                __LINE__, buf_size);
        }
    }
    return simple_read_from_buffer_safe(user_buf, count, ppos, buf, output_len);
}


STATIC const ka_file_operations_t g_mailbox_dfx_ops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(tsdrv_dfx_mbox_open)
    ka_fs_init_f_read(tsdrv_dfx_mbox_read)
};

int tsdrv_mailbox_dfx_file_create(u32 devid, u32 fid)
{
    struct tsdrv_dfx_fid *dfx_fid = tsdrv_get_fid_dfx(devid, fid);
    ka_dentry_t *ts_id_dentry[DEVDRV_MAX_TS_NUM] = {NULL};
    u32 dfx_id = (fid << TSDRV_DFX_DEV_OFFSET) | devid;
    u32 tsnum = tsdrv_get_dev_tsnum(devid);
    char name[TSDRV_DFX_NAME_LEN] = {0};
    ka_dentry_t *mbox_dir = NULL;
    u32 tsid, i;
    int err;

    if (dfx_fid->fid_dentry == NULL) {
        TSDRV_PRINT_ERR("fid entry is NULL, devid(%u) fid(%u)\n", devid, fid);
        return -ENODEV;
    }

    mbox_dir = ka_fs_debugfs_create_dir("mailbox", dfx_fid->fid_dentry);
    if (mbox_dir == NULL) {
        TSDRV_PRINT_ERR("mailbox dir create fail, devid=%u fid=%u\n", devid, fid);
        return -EFAULT;
    }

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = snprintf_s(name, TSDRV_DFX_NAME_LEN, TSDRV_DFX_NAME_LEN - 1, "mailbox_ts%u", tsid);
        if (err < 0) {
            TSDRV_PRINT_ERR("copy dfx mbox name fail, err(%d)\n", err);
            goto ERR;
        }

        dfx_id = (tsid << (TSDRV_DFX_DEV_OFFSET + TSDRV_DFX_FID_OFFSET)) | (fid << TSDRV_DFX_DEV_OFFSET) | devid;
        ts_id_dentry[tsid] = ka_fs_debugfs_create_file(name, KA_S_IRUGO, mbox_dir, (void *)(uintptr_t)dfx_id,
            &g_mailbox_dfx_ops);
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

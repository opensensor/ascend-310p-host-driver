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
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/securec.h>

#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_fs_pub.h"

#include "devdrv_manager_common.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_time.h"
#include "dms_common.h"

#define DATE_LEN 80
#define TWENTY_CENTURY 1900
#define JANUARY 1
#define KILO 1000
static inline u16 dms_timezone_check_begin(u16 begin)
{
    return ((begin) & ((u16)DMS_TIMEZONE_BODY_BOTH << DMS_TIMEZONE_HEAD_BIT));
}

static inline u16 dms_timezone_get_data(u16 data)
{
    return ((data) & ~((u16)DMS_TIMEZONE_BODY_DATA << DMS_TIMEZONE_BODY_BIT));
}

static inline u16 dms_timezone_check_finish(u16 finish)
{
    return ((finish) & ((u16)DMS_TIMEZONE_BODY_BOTH << DMS_TIMEZONE_BODY_BIT));
}

struct dms_time_sync_info g_dms_time_sync_info[DEVICE_NUM_MAX] = {0};

int dms_time_sync_info_init(u32 dev_id)
{
    g_dms_time_sync_info[dev_id].new_timezone = (char *)dbl_kzalloc(DMS_LOCALTIME_FILE_SIZE, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_dms_time_sync_info[dev_id].new_timezone == NULL) {
        dms_err("Kzalloc return NULL, failed to alloc mem for new localtime.\n");
        return -ENOMEM;
    }
    g_dms_time_sync_info[dev_id].system_state = DMS_SYSTEM_WORKING;
    mutex_init(&g_dms_time_sync_info[dev_id].time_sync_lock);

    return DRV_ERROR_NONE;
}

/* called at  dms init  and dms exit  */
void dms_time_sync_info_free(u32 dev_id)
{
    if (g_dms_time_sync_info[dev_id].new_timezone != NULL) {
        dbl_kfree(g_dms_time_sync_info[dev_id].new_timezone);
        g_dms_time_sync_info[dev_id].new_timezone = NULL;
    }
    mutex_destroy(&g_dms_time_sync_info[dev_id].time_sync_lock);
}

struct dms_time_sync_info *dms_get_time_sync_info(u32 dev_id)
{
    return &g_dms_time_sync_info[dev_id];
}

STATIC void dms_turn_second_to_date(long long second, long long nsec, char *date, int len)
{
    struct tm date_tm;
    int ret = 0;

    ret = memset_s(&date_tm, sizeof(struct tm), 0, sizeof(struct tm));
    if (ret != 0) {
        dms_err("Memset failed. (ret=%d)\n", ret);
        return;
    }
    time64_to_tm(second, 0, &date_tm);

    ret = sprintf_s(date, len, "%04ld-%02d-%02d %02d:%02d:%02d.%06lld",
                    date_tm.tm_year + TWENTY_CENTURY, date_tm.tm_mon + JANUARY, date_tm.tm_mday,
                    date_tm.tm_hour, date_tm.tm_min, date_tm.tm_sec, nsec / KILO);
    if (ret < 0) {
        dms_err("Sprintf failed. (ret=%d)\n", ret);
        return;
    }
}

STATIC void dms_check_config_time_update(struct dms_walltime_info *time_info)
{
    struct timespec time_now;
    char host_date_buffer[DATE_LEN] = {0};
    char device_date_buffer[DATE_LEN] = {0};

    time_now = current_kernel_time();
    if ((time_now.tv_sec - DMS_TIME_UPDATE_THRESHOLD_DEV > time_info->wall_time.tv_sec) ||
        (time_now.tv_sec < time_info->wall_time.tv_sec - DMS_TIME_UPDATE_THRESHOLD_DEV)) {
        time_info->time_update = DMS_TIME_NEED_UPDATE;
#ifndef CFG_ENV_ESL
        dms_turn_second_to_date(time_info->wall_time.tv_sec, time_info->wall_time.tv_nsec,
            host_date_buffer, DATE_LEN);
        dms_turn_second_to_date(time_now.tv_sec, time_now.tv_nsec,
            device_date_buffer, DATE_LEN);
        dms_info("Host time different from device, start update. (host_time=%s; dev_time=%s; dev_id=%u)\n",
            host_date_buffer, device_date_buffer, time_info->dev_id);
#endif
    }
}

int dms_get_walltime_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len)
{
    int ret;
    struct dms_walltime_info *time_info = NULL;
    struct dms_h2d_msg *time_msg = NULL;

    time_msg = (struct dms_h2d_msg *)msg;
    if ((time_msg->header.valid != DMS_TIME_MSG_VALID) ||
        (in_len < sizeof(struct dms_h2d_msg))) {
        dms_err("Invalid message from host. (valid=%u; in_len=%u)\n", time_msg->header.valid, in_len);
        return -EINVAL;
    }

    time_msg->header.result = 0;
    *ack_len = sizeof(*time_msg);
    time_info = (struct dms_walltime_info *)time_msg->payload;

    dms_check_config_time_update(time_info);
    if (time_info->time_update == DMS_TIME_UPDATE_DONE) {
        return DRV_ERROR_NONE;
    }

    ret = ka_system_do_settimeofday64(&time_info->wall_time);
    if (ret) {
        dms_err("Do_settimeofday64 failed,"
            "fail to set time, (ret=%d; sizeof_sec=%lu),"
            "(sizeof_nsec=%lu; sec_val=%lld; nsec_val=%lld).\n",
            ret, sizeof(time_info->wall_time.tv_sec), sizeof(time_info->wall_time.tv_nsec),
            (long long)time_info->wall_time.tv_sec, (long long)time_info->wall_time.tv_nsec);
    }
    return ret;
}

STATIC int dms_cover_localtime_in_device(struct dms_time_sync_info *time_info, long write_size)
{
    struct file *fp = NULL;
    loff_t pos = 0;
    long ret;

    mutex_lock(&time_info->time_sync_lock);
    fp = filp_open(DMS_LOCALTIME_FILE_PATH, (O_WRONLY | KA_O_TRUNC), 0);
    if (IS_ERR(fp)) {
        mutex_unlock(&time_info->time_sync_lock);
        dms_err("Filp_open error. (path=%s, errno=%ld).\n", DMS_LOCALTIME_FILE_PATH, PTR_ERR(fp));
        return -EINVAL;
    }

    ret = kernel_write(fp, time_info->new_timezone, write_size, &pos);
    (void)filp_close(fp, NULL);
    mutex_unlock(&time_info->time_sync_lock);
    if (ret != write_size) {
        dms_err("Kernel_write error. (ret=%ld, write=%ld)\n", ret, write_size);
        return -EINVAL;
    }
    dms_info("Write localtime in device success. (w_size=%ld)\n", write_size);

    return DRV_ERROR_NONE;
}

STATIC int dms_timezone_handle(u32 dev_id, u16 recv_result, u16 write_size, u16 per_size, char *recv_buf)
{
    int ret;
    u16 send_times, per_write_size;
    static u16 recv_times = 0;
    struct dms_time_sync_info *time_info = NULL;

    dms_info("Recv localtime msg from host. (w_size:%u; per_size:%u; r_times:%u; r_result:%u)\n",
        write_size, per_size, recv_times, recv_result);
    if (dms_timezone_check_begin(recv_result)) {
        recv_times = 0;
    }

    send_times = (write_size % per_size) ? (write_size / per_size + 1) : (write_size / per_size);
    per_write_size = (write_size - recv_times * per_size) >= per_size ? per_size : (write_size - recv_times * per_size);

    if ((recv_times >= send_times) || (recv_times * per_size + per_write_size) > DMS_LOCALTIME_FILE_SIZE) {
        dms_err("recv times error. (dev_id=%u; send=%u; recv=%u)\n", dev_id, send_times, recv_times);
        recv_times = 0;
        return -EINVAL;
    }

    time_info = dms_get_time_sync_info(dev_id);
    if ((time_info == NULL) || (time_info->new_timezone == NULL)) {
        dms_err("Invalid manager info.\n");
        return -EINVAL;
    }

    mutex_lock(&time_info->time_sync_lock);
    ret = memcpy_s(&time_info->new_timezone[recv_times * per_size],
        DMS_LOCALTIME_FILE_SIZE - recv_times * per_size, recv_buf, per_write_size);
    mutex_unlock(&time_info->time_sync_lock);
    if (ret) {
        dms_err("Localtime file memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }
    recv_times++;

    if (recv_times == send_times) {
        if (dms_timezone_check_finish(recv_result) == 0) {
            dms_err("Localtime file recv finish flag error. (send=%u; recv=%u)\n", send_times, recv_times);
            recv_times = 0;
            return -EINVAL;
        }

        recv_times = 0;
        ret = dms_cover_localtime_in_device(time_info, write_size);
        if (ret) {
            dms_err("Cover localtime file failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

int dms_get_timezone_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len)
{
    int ret;
    u16 write_size;
    u16 recv_result;
    u16 per_size;
    char *recv_buf = NULL;
    struct dms_h2d_msg *localtime_msg = NULL;

    localtime_msg = (struct dms_h2d_msg *)msg;
    if ((localtime_msg->header.valid != DMS_TIME_MSG_VALID) ||
        (in_len < sizeof(struct dms_h2d_msg))) {
        dms_err("Invalid message from host. (valid=%u; in_len=%u)\n", localtime_msg->header.valid, in_len);
        return -EINVAL;
    }

    per_size = (u16)sizeof(localtime_msg->payload);
    *ack_len = sizeof(*localtime_msg);
    recv_buf = (char *)localtime_msg->payload;
    recv_result = localtime_msg->header.result;

    write_size = dms_timezone_get_data(recv_result);
    if (write_size == 0) {
        dms_err("Localtime file is too big. (dev_id=%u; file_size=%u)\n", devid, write_size);
        return -EINVAL;
    }

    ret = dms_timezone_handle(devid, recv_result, write_size, per_size, recv_buf);
    if (ret) {
        dms_err("Dms localtime handle failed. (devid=%u; ret=%d)\n", devid, ret);
    }

    localtime_msg->header.result = 0;
    return DRV_ERROR_NONE;
}

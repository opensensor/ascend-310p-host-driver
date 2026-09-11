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

#ifndef __DMS_TIME_H__
#define __DMS_TIME_H__

#include <linux/mutex.h>
#include <linux/time64.h>
#include <linux/time.h>

#include "dms_define.h"
#include "drv_type.h"

#define DMS_TIMEZONE_HEAD_BIT   15
#define DMS_TIMEZONE_BODY_BIT   14
#define DMS_TIMEZONE_BODY_DATA  0x03
#define DMS_TIMEZONE_BODY_BOTH  0x01

#define DMS_TIME_NEED_UPDATE 1
#define DMS_TIME_UPDATE_DONE 0
#define DMS_TIME_UPDATE_THRESHOLD 6
#define DMS_TIME_UPDATE_THRESHOLD_DEV 1

#define DMS_LOCALTIME_FILE_PATH "/etc/localtime"
#define DMS_LOCALTIME_FILE_SIZE (16 * 1024)

#define DMS_TIME_MSG_VALID 0x5A5A

/* host reboot signal */
#define DMS_SYSTEM_WORKING 0
#define DMS_REBOOT_PREPARE 1

/* device local time synchronize sevice state */
#define DMS_TIMEZONE_SYNC_RUNNING 2
#define DMS_TIMEZONE_SYNC_IDLE 3
#define DMS_TIMEZONE_SYNC_STOP 4

/* sleep time, waiting for localtime sync finish */
#define DMS_TIMEZONE_SLEEP_MS 100
#define DMS_TIMEZONE_MAX_COUNT 60

struct dms_time_sync_info {
    u32 system_state;               /* for system state: prepare to reboot or working */
    u32 timer_node_id;              /* for timer task id, used at unregister task process */
    char *pre_timezone;
    char *new_timezone;
    struct mutex time_sync_lock;    /* protect for localtime read */
    u32 timezone_sync_state;        /* for localtime sync state: syncing、stop or idle */
    u32 time_update_flag;           /* for localtime sync: need to sync or not */
    unsigned long pre_walltime;
};

struct dms_walltime_info {
    u32 dev_id;
    u32 time_update;                /* if time_update = 1, you need send time to device to update */
    struct timespec64 wall_time;
};

int dms_time_sync_reboot_handle(void);
int dms_is_sync_timezone(void);
void set_time_need_update(u32 dev_id);
void clear_time_update_flag(u32 dev_id);
int get_time_update_flag(u32 dev_id);
struct dms_time_sync_info* dms_get_time_sync_info(u32 dev_id);
int dms_time_sync_event(u64 user_data);
int dms_time_sync_init(u32 dev_id);
void dms_time_sync_exit(u32 dev_id);
int dms_is_sync_localtime(struct dms_time_sync_info *time_info);
int dms_get_walltime_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len);
int dms_get_timezone_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len);
int dms_time_sync_info_init(u32 dev_id);
void dms_time_sync_info_free(u32 dev_id);
int dms_heartbeat_is_stop(u32 dev_id);

#endif


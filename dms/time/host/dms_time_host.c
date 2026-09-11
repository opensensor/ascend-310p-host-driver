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
#include <linux/kallsyms.h>

#include "ka_fs_pub.h"
#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"

#include "devdrv_manager_common.h"
#include "dms_timer.h"
#include "dms_time.h"
#include "devdrv_interface.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_common.h"
#include "davinci_api.h"
#include "davinci_interface.h"
#include "dms_kernel_version_adapt.h"
#include "kernel_version_adapt.h"

#define TIME_SYNC_TIMER_EXPIRE_MS   6000
#define INVALID_TIMER_NODE_ID       0xFFFFFFFF

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
int g_heart_lost[DEVICE_NUM_MAX] = {0};
#endif

static inline u16 dms_timezone_set_begin(u16 begin)
{
    return ((begin) | ((u16)DMS_TIMEZONE_BODY_BOTH << DMS_TIMEZONE_HEAD_BIT));
}

static inline u16 dms_timezone_set_data(u16 data)
{
    return ((data) & ~((u16)DMS_TIMEZONE_BODY_DATA << DMS_TIMEZONE_BODY_BIT));
}

static inline u16 dms_timezone_set_finish(u16 finish)
{
    return ((finish) | ((u16)DMS_TIMEZONE_BODY_BOTH << DMS_TIMEZONE_BODY_BIT));
}

struct dms_time_sync_info g_dms_time_sync_info[DEVICE_NUM_MAX] = {{0}};

int dms_heartbeat_is_stop(u32 dev_id)
{
    int ret;
    unsigned int status = 0;
    struct ascend_intf_get_status_para para = {0};

    para.type = DAVINCI_STATUS_TYPE_DEVICE;
    para.para.device_id = dev_id;

    ret = ascend_intf_get_status(para, &status);
    if (ret != 0) {
        dms_warn("Get device status warn. (dev_id=%u; ret=%d))\n", dev_id, ret);
    }

    return 0x1 & (status >> 1); /* #define DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST (1<<1) */
}

int dms_time_sync_info_init(u32 dev_id)
{
    g_dms_time_sync_info[dev_id].pre_timezone = (char *)dbl_kzalloc(DMS_LOCALTIME_FILE_SIZE, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_dms_time_sync_info[dev_id].pre_timezone == NULL) {
        dms_err("Kzalloc return NULL, failed to alloc mem for old localtime.\n");
        return -ENOMEM;
    }

    g_dms_time_sync_info[dev_id].new_timezone = (char *)dbl_kzalloc(DMS_LOCALTIME_FILE_SIZE, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_dms_time_sync_info[dev_id].new_timezone == NULL) {
        dms_err("Kzalloc return NULL, failed to alloc mem for new localtime.\n");
        dbl_kfree(g_dms_time_sync_info[dev_id].pre_timezone);
        g_dms_time_sync_info[dev_id].pre_timezone = NULL;
        return -ENOMEM;
    }

    g_dms_time_sync_info[dev_id].system_state = DMS_SYSTEM_WORKING;
    g_dms_time_sync_info[dev_id].timezone_sync_state = DMS_TIMEZONE_SYNC_IDLE;
    mutex_init(&g_dms_time_sync_info[dev_id].time_sync_lock);
    g_dms_time_sync_info[dev_id].timer_node_id = INVALID_TIMER_NODE_ID;

    return DRV_ERROR_NONE;
}

/* called at  dms init  and dms exit  */
void dms_time_sync_info_free(u32 dev_id)
{
    if (g_dms_time_sync_info[dev_id].pre_timezone != NULL) {
        dbl_kfree(g_dms_time_sync_info[dev_id].pre_timezone);
        g_dms_time_sync_info[dev_id].pre_timezone = NULL;
    }

    if (g_dms_time_sync_info[dev_id].new_timezone != NULL) {
        dbl_kfree(g_dms_time_sync_info[dev_id].new_timezone);
        g_dms_time_sync_info[dev_id].new_timezone = NULL;
    }
    mutex_destroy(&g_dms_time_sync_info[dev_id].time_sync_lock);
}


/* dms init and pcie hot reset will call this function */
void set_time_need_update(u32 dev_id)
{
    g_dms_time_sync_info[dev_id].time_update_flag = DMS_TIME_NEED_UPDATE;
}

void clear_time_update_flag(u32 dev_id)
{
    g_dms_time_sync_info[dev_id].time_update_flag = DMS_TIME_UPDATE_DONE;
}

int get_time_update_flag(u32 dev_id)
{
    return g_dms_time_sync_info[dev_id].time_update_flag;
}

STATIC unsigned long get_pre_walltime(u32 dev_id)
{
    return g_dms_time_sync_info[dev_id].pre_walltime;
}

STATIC void set_pre_walltime(u32 dev_id, unsigned long walltime)
{
    g_dms_time_sync_info[dev_id].pre_walltime = walltime;
}

STATIC void set_system_status(u32 dev_id, u32 status)
{
    g_dms_time_sync_info[dev_id].system_state = status;
}


struct dms_time_sync_info* dms_get_time_sync_info(u32 dev_id)
{
    return &g_dms_time_sync_info[dev_id];
}

/* detect someone maybe change time */
STATIC int dms_detect_time_change(u32 dev_id, unsigned long new_time, struct dms_walltime_info *time_info)
{
    unsigned long pre_walltime;

    pre_walltime = get_pre_walltime(dev_id);
    if ((new_time < pre_walltime) || (new_time - pre_walltime > DMS_TIME_UPDATE_THRESHOLD)) {
        time_info->time_update = DMS_TIME_NEED_UPDATE;
    } else {
        time_info->time_update = DMS_TIME_UPDATE_DONE;
    }
    set_pre_walltime(dev_id, new_time);

    return DRV_ERROR_NONE;
}

STATIC int dms_send_time_msg(u32 dev_id, struct dms_h2d_msg *time_msg, u32 *out_len)
{
    int ret;

    ret = devdrv_common_msg_send(dev_id, time_msg, sizeof(struct dms_h2d_msg),
                                 sizeof(struct dms_h2d_msg), (u32 *)out_len,
                                 DEVDRV_COMMON_MSG_DEVDRV_MANAGER);

    return ret;
}

STATIC int dms_send_timezone(u32 dev_id, const char *new_timezone, u16 read_size)
{
    u16 i;
    int ret = DRV_ERROR_NONE;
    int out_len;
    u16 per_size;
    u16 send_times;
    u16 read_size_tmp;
    struct dms_h2d_msg time_msg = {{0}, {0}};

    time_msg.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_LOCALTIME_SYNC;
    time_msg.header.valid = DMS_TIME_MSG_VALID;
    time_msg.header.dev_id = dev_id;

    per_size = (u16)sizeof(time_msg.payload);
    send_times = (read_size % per_size) ? (read_size / per_size + 1) : (read_size / per_size);
    for (i = 0; i < send_times; i++) {
        ret = memcpy_s(time_msg.payload, per_size, &new_timezone[i * per_size], per_size);
        if (ret) {
            dms_err("Memcpy_s error. (devid=%u; ret=%d; i=%u)\n", dev_id, ret, i);
            return ret;
        }

        read_size_tmp = dms_timezone_set_data(read_size);
        read_size_tmp = (i == 0) ? dms_timezone_set_begin(read_size_tmp) : read_size_tmp;
        read_size_tmp = (i == (send_times - 1)) ? dms_timezone_set_finish(read_size_tmp) : read_size_tmp;

        time_msg.header.result = read_size_tmp;
        ret = dms_send_time_msg(dev_id, &time_msg, &out_len);
        if (ret || (time_msg.header.result != 0)) {
            dms_warn("Send msg unsuccessful. (dev_id=%u; ret=%d; result=%u; times=%u; sendtime=%u)\n",
                dev_id, ret, time_msg.header.result, i, send_times);
            return ret;
        }
    }

    dms_info("Send localtime msg success. (dev_id=%u; result=%u; sendtime=%u)\n",
        dev_id, time_msg.header.result, send_times);
    return ret;
}

STATIC int dms_time_read_timezone(u32 dev_id, char *buf, u16 *read_size)
{
    struct file *fp = NULL;
    loff_t pos = 0;
    long read_bytes;

    fp = filp_open(DMS_LOCALTIME_FILE_PATH, KA_O_RDONLY, 0);
    if (IS_ERR(fp)) {
        dms_err("Filp_open error. (dev_id=%u; path=%s)\n", dev_id, DMS_LOCALTIME_FILE_PATH);
        return -EINVAL;
    }

    read_bytes = ka_fs_kernel_read(fp, buf, *read_size, &pos);
    filp_close(fp, NULL);
    if ((read_bytes <= 0) || (read_bytes >= (long)*read_size)) {
        dms_err("Kernel_read error. (dev_id=%u; ret=%ld)\n", dev_id, read_bytes);
        return -EINVAL;
    }

    *read_size = (u16)read_bytes;
    return 0;
}

STATIC int dms_timezone_sync(u32 dev_id, struct dms_time_sync_info *time_info)
{
    int ret;
    u16 read_size;

    if (time_info->timezone_sync_state == DMS_TIMEZONE_SYNC_STOP) {
        dms_info("Stop localtime synchronization, (dev_id=%d).\n", dev_id);
        return DRV_ERROR_NONE;
    }
    time_info->timezone_sync_state = DMS_TIMEZONE_SYNC_RUNNING;
    ka_mb();
    if (time_info->system_state == DMS_REBOOT_PREPARE) {
        time_info->timezone_sync_state = DMS_TIMEZONE_SYNC_STOP;
        dms_info("System reboot now...(dev_id=%d) send localtime stop.\n", dev_id);
        return DRV_ERROR_NONE;
    }

    read_size = (u16)DMS_LOCALTIME_FILE_SIZE;
    ret = dms_time_read_timezone(dev_id, time_info->new_timezone, &read_size);
    if (ret) {
        dms_err("Read timezone failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (get_time_update_flag(dev_id) != DMS_TIME_NEED_UPDATE) {
        ret = memcmp(time_info->pre_timezone, time_info->new_timezone, DMS_LOCALTIME_FILE_SIZE);
        if (ret) {
            set_time_need_update(dev_id);
        }
    }

    if (get_time_update_flag(dev_id) == DMS_TIME_NEED_UPDATE) {
        ret = dms_send_timezone(dev_id, time_info->new_timezone, (u16)read_size);
        if (ret) {
            dms_warn("Send timezone to device unsuccessful. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
        ret = memcpy_s(time_info->pre_timezone, DMS_LOCALTIME_FILE_SIZE,
            time_info->new_timezone, DMS_LOCALTIME_FILE_SIZE);
        if (ret) {
            dms_err("Memcpy_s failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
        clear_time_update_flag(dev_id);
    }

    return 0;
}


STATIC int dms_wall_time_sync(u32 dev_id)
{
    int ret;
    int out_len = 0;
    struct timespec send_wall_time;
    struct timespec check_wall_time;
    unsigned long new_time = 0;
    struct dms_h2d_msg time_msg;
    struct dms_walltime_info *send_time_info = NULL;

    send_time_info = dbl_kzalloc(sizeof(struct dms_walltime_info), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (send_time_info == NULL) {
        dms_err("Kmalloc in time event fail once, give up sync time to device. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_NONE;
    }

    send_wall_time = current_kernel_time();
    new_time = send_wall_time.tv_sec;
    send_time_info->dev_id = dev_id;
    send_time_info->wall_time.tv_sec = send_wall_time.tv_sec;
    send_time_info->wall_time.tv_nsec = send_wall_time.tv_nsec;
    dms_detect_time_change(dev_id, new_time, send_time_info);

    time_msg.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_WALL_TIME_SYNC;
    time_msg.header.valid = DMS_TIME_MSG_VALID;
    /* give a random value for checking result later */
    time_msg.header.result = (u16)0x1A;
    /* inform corresponding devid to device side */
    time_msg.header.dev_id = send_time_info->dev_id;

    ret = memcpy_s(time_msg.payload, sizeof(time_msg.payload), send_time_info,
                   sizeof(struct dms_walltime_info));
    if (ret != 0) {
        dms_err("Copy from time_info failed, (ret=%d; dev_id=%u).\n", ret, send_time_info->dev_id);
        goto  out;
    }

    ret = dms_send_time_msg(dev_id, &time_msg, &out_len);
    if (ret || (time_msg.header.result != 0)) {
        dms_warn("Send walltime to device unsuccessful, (ret=%d; dev_id=%u).\n", ret, send_time_info->dev_id);
    } else {
        check_wall_time = current_kernel_time();
    }

out:
    dbl_kfree(send_time_info);
    return DRV_ERROR_NONE;
}

int dms_time_sync_event(u64 user_data)
{
    int ret;
    struct dms_time_sync_info *time_sync_info;
    u32 dev_id = (u32)user_data;

    if (dms_heartbeat_is_stop(dev_id)) {
        /* Device heart beat lost, stop time sync. */
        return 0;
    }

    time_sync_info = dms_get_time_sync_info(dev_id);
    /* sync host walltime to device */
    ret = dms_wall_time_sync(dev_id);
    if (ret) {
        dms_err("Sync wall time to device failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    /* localtime sync to device */
    ret = dms_timezone_sync(dev_id, time_sync_info);
    if (ret) {
        dms_warn("Sync localtime to device unsuccessful. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}


int dms_time_sync_init(u32 dev_id)
{
    int ret;
    u32 timer_node_id = 0;
    struct dms_timer_task time_sync_task = {0};
    struct dms_time_sync_info *time_sync_info = NULL;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return 0;
    }

    ret = dms_time_sync_info_init(dev_id);
    if (ret) {
        dms_err("Dms time_sync info init failed, (dev_id=%u; ret=%d).\n", dev_id, ret);
        return ret;
    }

    set_time_need_update(dev_id);
    time_sync_task.expire_ms = TIME_SYNC_TIMER_EXPIRE_MS;
    time_sync_task.count_ms = 0;
    time_sync_task.user_data = dev_id;
    time_sync_task.handler_mode = COMMON_WORK;
    time_sync_task.exec_task = dms_time_sync_event;

    ret = dms_timer_task_register(&time_sync_task, &timer_node_id);
    if (ret) {
        dms_err("Dms timer task register failed, (dev_id=%u; ret=%d).\n", dev_id, ret);
        return ret;
    }

    time_sync_info = dms_get_time_sync_info(dev_id);
    time_sync_info->timer_node_id = timer_node_id;

    return DRV_ERROR_NONE;
}

void dms_time_sync_exit(u32 dev_id)
{
    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return;
    }

    if (g_dms_time_sync_info[dev_id].timer_node_id != INVALID_TIMER_NODE_ID) {
        /* unregister timer task */
        dms_timer_task_unregister(g_dms_time_sync_info[dev_id].timer_node_id);
    }

    /* unregister from timer firstly, then free the resources */
    dms_time_sync_info_free(dev_id);

    g_dms_time_sync_info[dev_id].timer_node_id = INVALID_TIMER_NODE_ID;

    dms_info("Time sync even unregister from timer. (dev_id=%u) \n", dev_id);
}

int dms_time_sync_reboot_handle(void)
{
    u32 i;

    for (i = 0; i < DEVICE_NUM_MAX; ++i) {
        set_system_status(i, DMS_REBOOT_PREPARE);
    }

    return DRV_ERROR_NONE;
}

/* for reboot handle check */
int dms_is_sync_timezone(void)
{
    u32 i;

    for (i = 0; i < DEVICE_NUM_MAX; ++i) {
        if (g_dms_time_sync_info[i].timezone_sync_state == DMS_TIMEZONE_SYNC_RUNNING) {
            return true;
        }
    }
    return false;
}


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
*
* Description:
* Author: huawei
* Create: 2025-08-06
*/

#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/moduleparam.h>

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"

#include "davinci_interface.h"
#include "davinci_api.h"
#include "soft_fault_define.h"
#include "heart_beat/heart_beat.h"
#include "heart_beat/hb_read.h"

#define HEART_BEAT_READ_TIMER_EXPIRE_SEC 6

struct hb_read_timer {
    struct hrtimer timer;
    struct work_struct hb_read_work; /* work for read the heart beat count */
    struct workqueue_struct *hb_read_wq;
    struct timespec64 last_normal_record;
    struct timespec64 last_time_record;
    unsigned long long miss_count;
};

static struct hb_read_block g_hb_read_block[DEVICE_NUM_MAX] = {{0}};
static struct hb_read_timer g_hb_read_timer = {{{{0}}}, {{0}}};
static int g_heart_beat_switch = 1;
ka_module_param(g_heart_beat_switch, int, KA_S_IRUGO);

struct hb_read_block *get_heart_beat_read_item(unsigned int dev_id)
{
    if (dev_id >= DEVICE_NUM_MAX) {
        return NULL;
    }
    return &g_hb_read_block[dev_id];
}

bool hb_is_need_judge(unsigned long cur_time, struct hb_read_block *hb_info)
{
    long interval_time_ms = 0;

    /* if the interval time between last judgment and this is less than 6s skip this judgment */
    if (cur_time >= hb_info->last_read_time) {
        interval_time_ms = (cur_time - hb_info->last_read_time) / NSEC_PER_MSEC;
    }

    /* If the interval since the last read work is less than 5000ms, skip this read work */
    if ((interval_time_ms < 5000)) {
        return false;
    }

    return true;
}

static void log_heart_beat_debug_info(struct hb_read_block *hb_info, unsigned long long cur_time, unsigned long long last_time)
{
    unsigned long long interval_time = (cur_time - last_time) / KA_NSEC_PER_SEC;

    /* If the interval between two read work over 12s, record the dfx log */
    if (interval_time >= 12) {
        soft_drv_warn("miss read heart beat over 12s. (dev_id=%u; interval_time=%llus, miss times=%llu) \n",
                      hb_info->dev_id, interval_time, hb_info->miss_read_count);
    }

    if (hb_info->lost_count > 0) {
        soft_drv_warn("heart beat debug info. (lost count=%u; total lost count=%llu; total miss read count=%llu)\n",
                       hb_info->lost_count, hb_info->total_lost_count, hb_info->miss_read_count);
    }
    return;
}

static bool check_is_need_read(unsigned int dev_id, struct devdrv_manager_info *manager_info, struct hb_read_block *hb_read_item)
{
    unsigned long cur_time = 0;

    if (manager_info == NULL) {
        return false;
    }

#ifndef CFG_EDGE_HOST
    if (manager_info->msg_chan_rdy[dev_id] == 0) {
        return false;
    }
#endif

    if (hb_read_item == NULL || hb_read_item->hb_stutas != HEART_BEAT_READY) {
        return false;
    }

    cur_time = ka_system_ktime_get_raw_ns();
    if (!hb_is_need_judge(cur_time, hb_read_item)) {
        return false;
    }

    return true;
}

bool check_is_heart_beat_lost(struct hb_read_block *hb_read_item, unsigned long cur_count, unsigned int max_lost_count)
{
    if (cur_count == hb_read_item->old_count) {
        hb_read_item->lost_count++;
        hb_read_item->total_lost_count++;
    } else {
        hb_read_item->lost_count = 0;
    }

    if (hb_read_item->lost_count >= max_lost_count) {
        return true;
    } else {
        hb_read_item->old_count = cur_count;
        return false;
    }
}

void hb_read_item_work_stop(unsigned int dev_id, struct hb_read_block* hb_read_item)
{
    hb_read_item->hb_stutas = HEART_BEAT_LOST;
    hb_read_item->old_count = 0;
    hb_read_item->lost_count = 0;
    hb_read_item->total_lost_count = 0;
    hb_read_item->miss_read_count = 0;
    hb_read_item->last_read_time = 0;
    (void)ascend_intf_report_device_status(dev_id, DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST);
}

void hb_read_one_device_count(unsigned int dev_id)
{
    struct hb_read_block *hb_read_item = get_heart_beat_read_item(dev_id);
    unsigned long long cur_count = 0;
    unsigned long long cur_time;
    int ret;
    unsigned int max_lost_count = KA_UINT_MAX;
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    if (!check_is_need_read(dev_id, manager_info, hb_read_item)) {
        return;
    }
    /* if 'max_lost_count' has been init before, it will skip here. */
    max_lost_count = heart_beat_get_max_lost_count(dev_id);
    if (max_lost_count == KA_UINT_MAX) {
        return;
    }

    ret = hb_get_heart_beat_count(dev_id, &cur_count);
    if (ret != 0) {
        return;
    }
     
    cur_time = ka_system_ktime_get_raw_ns();

    /* only user in host */
    (void)check_and_update_link_abnormal_status(dev_id, cur_count);

    log_heart_beat_debug_info(hb_read_item, cur_time, hb_read_item->last_read_time);
    hb_read_item->last_read_time = cur_time;

    if (check_is_heart_beat_lost(hb_read_item, cur_count, max_lost_count)) {
        soft_drv_err(
            "Heartbeat lost! (device_id=%u; old_count=%llu; current_count=%llu; lost_count=%d; "
            "total_lost_count=%llu; miss_read_count=%llu).\n",
            dev_id, hb_read_item->old_count, cur_count, hb_read_item->lost_count,
            hb_read_item->total_lost_count, hb_read_item->miss_read_count);
        manager_info->device_status[dev_id] = DRV_STATUS_COMMUNICATION_LOST;
        hb_read_item_work_stop(dev_id, hb_read_item);
        ka_task_queue_work(hb_read_item->hb_lost_wq, &hb_read_item->hb_lost_work);
    } else {
        hb_read_item->old_count = cur_count;
    }
}

static void heart_beat_read_work(struct work_struct *hb_read_work)
{
    unsigned int dev_id;

    for (dev_id = 0; dev_id < DEVICE_NUM_MAX; dev_id++) {
        hb_read_one_device_count(dev_id);
    }
    return;
}

static enum hrtimer_restart hb_read_timer_irq_handler(struct hrtimer *htr)
{
    ka_task_queue_work(g_hb_read_timer.hb_read_wq, &g_hb_read_timer.hb_read_work);
    ka_system_hrtimer_forward_now(htr, ka_system_ktime_set(HEART_BEAT_READ_TIMER_EXPIRE_SEC, 0));
    return HRTIMER_RESTART;
}

static void heart_beat_lost_work(struct work_struct *hb_lost_work)
{
    int ret;
    unsigned int dev_id;
    struct hb_read_block *hb_read_item = container_of(hb_lost_work, struct hb_read_block, hb_lost_work);

    dev_id = hb_read_item->dev_id;
    ret = hb_report_heart_beat_lost_event(dev_id);
    if (ret != 0) {
        soft_drv_err("report heart beat lost event failed. (device id=%u)", dev_id);
        return;
    }

    return;
}

int heart_beat_read_item_init(unsigned int dev_id)
{
    struct hb_read_block *hb_read_item = get_heart_beat_read_item(dev_id);
    if (hb_read_item == NULL) {
        soft_drv_err("heart beat info not init. (device id=%u)", dev_id);
        return -EINVAL;
    }

    if (hb_read_item->hb_lost_wq == NULL) {
        hb_read_item->hb_lost_wq = ka_task_create_singlethread_workqueue("hb_lost_wq");
        if (hb_read_item->hb_lost_wq == NULL) {
            soft_drv_err("create workqueue failed. (device id=%u)", dev_id);
            return -ENOMEM;
        }
    }
    INIT_WORK(&hb_read_item->hb_lost_work, heart_beat_lost_work);
    hb_read_item_work_start(dev_id, hb_read_item);
    return 0;
}

void heart_beat_read_item_uninit(unsigned int dev_id)
{
    struct hb_read_block *hb_read_item = get_heart_beat_read_item(dev_id);
    if (hb_read_item == NULL) {
        return;
    }

    if (hb_read_item->hb_stutas != HEART_BEAT_LOST) {
        hb_read_item->hb_stutas = HEART_BEAT_EXIT;
    }

    if (hb_read_item->hb_lost_wq != NULL) {
        ka_task_flush_workqueue(hb_read_item->hb_lost_wq);
        ka_task_destroy_workqueue(hb_read_item->hb_lost_wq);
        hb_read_item->hb_lost_wq = NULL;
    }
    soft_drv_info("heart beat read item uninit success. (dev_id=%u) \n", dev_id);
}

int heart_beat_read_timer_init(void)
{ 
    if (g_heart_beat_switch != 1) {
        soft_drv_warn("Heat beat is not enabled.\n");
        return 0;
    }
    
    hrtimer_setup(&g_hb_read_timer.timer, hb_read_timer_irq_handler, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    INIT_WORK(&g_hb_read_timer.hb_read_work, heart_beat_read_work);
    g_hb_read_timer.hb_read_wq = alloc_workqueue("%s", WQ_HIGHPRI, 1, "hb_read_work");

    hrtimer_start(&g_hb_read_timer.timer, ka_system_ktime_set(HEART_BEAT_READ_TIMER_EXPIRE_SEC, 0), HRTIMER_MODE_REL);
    return 0;
}

void heart_beat_read_timer_exit(void)
{
    int ret;

    ret = ka_system_hrtimer_cancel(&g_hb_read_timer.timer);
    if (ret < 0) {
        soft_drv_warn("hrtimer cannot cancel. (ret=%d)\n", ret);
    }

    if (g_hb_read_timer.hb_read_wq != NULL) {
        ka_task_flush_workqueue(g_hb_read_timer.hb_read_wq);
        ka_task_destroy_workqueue(g_hb_read_timer.hb_read_wq);
        g_hb_read_timer.hb_read_wq = NULL;
    }
}
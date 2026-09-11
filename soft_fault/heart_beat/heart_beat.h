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

#ifndef __HEART_BEAT_H
#define __HEART_BEAT_H


#define HEART_BEAT_NOT_INIT   0
#define HEART_BEAT_READY   1
#define HEART_BEAT_LOST   2
#define HEART_BEAT_EXIT   3

#define HEART_BEAT_DEATH_COUNT 4
#define HEART_BEAT_HCCS_DEATH_COUNT 8

int hb_set_heart_beat_count(unsigned int dev_id, unsigned long long count);
int hb_get_heart_beat_count(unsigned int dev_id, unsigned long long* count);

int hb_report_heart_beat_lost_event(unsigned int dev_id);

int heart_beat_read_timer_init(void);
void heart_beat_read_timer_exit(void);
int heart_beat_write_timer_init(void);
void heart_beat_write_timer_exit(void);

void heart_beat_write_status_init(unsigned int udevid);
int heart_beat_read_item_init(unsigned int udevid);

void heart_beat_write_status_uninit(unsigned int udevid);
void heart_beat_read_item_uninit(unsigned int udevid);

unsigned int heart_beat_get_max_lost_count(unsigned int dev_id);

int check_and_update_link_abnormal_status(unsigned int dev_id, unsigned long long count);

int heartbeat_dev_register(unsigned int dev_id);
void heartbeat_dev_unregister(void);

int heart_beat_register_urgent_timer(unsigned int dev_id);
void heart_beat_unregister_urgent_timer(unsigned int dev_id);

struct hb_write_block* hb_get_write_item(unsigned int dev_id);
#endif // __HEART_BEAT_H
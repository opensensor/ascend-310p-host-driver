/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */

#ifndef SCHED_CMD_H
#define SCHED_CMD_H

#ifndef __KERNEL__
#include <sys/ioctl.h>
#endif

#include "ascend_hal_define.h"
#include "event_sched_inner.h"

#define SCHED_CHAR_DEV_NAME_MAX_LEN (30)
#define SCHED_CHAR_DEV_NAME "event_sched"
#ifdef AOS_LLVM_BUILD
#define SCHED_CHAR_DEV_FULL_NAME "/local/dev/event_sched"
#define SCHED_CHAR_DEV_DRIVER_NAME "/dev/event_sched"
#elif defined(CFG_FEATURE_EXTERNAL_CDEV)
#define SCHED_CHAR_DEV_FULL_NAME davinci_intf_get_dev_path()
#define DAVINCI_ESCHED_SUB_MODULE_NAME "ESCHED"
#else
#define SCHED_CHAR_DEV_FULL_NAME "/dev/event_sched"
#endif

#define SCHED_CMD_MAX_NR (30)

#define SCHED_ID_MAGIC 'W'

#define SCHED_SET_SCHED_CPU_ID _IOWR_BAD(SCHED_ID_MAGIC, 0, sizeof(struct sched_ioctl_para_cpu_info))
#define SCHED_PROC_ADD_GRP_ID _IOWR_BAD(SCHED_ID_MAGIC, 1, sizeof(struct sched_ioctl_para_add_grp))
#define SCHED_SET_EVENT_PRIORITY_ID _IOWR_BAD(SCHED_ID_MAGIC, 2, sizeof(struct sched_ioctl_para_set_event_pri))
#define SCHED_SET_PROCESS_PRIORITY_ID _IOWR_BAD(SCHED_ID_MAGIC, 3, sizeof(struct sched_ioctl_para_set_proc_pri))
#define SCHED_THREAD_SUBSCRIBE_EVENT_ID _IOWR_BAD(SCHED_ID_MAGIC, 4, sizeof(struct sched_ioctl_para_subscribe))
#define SCHED_GET_EXACT_EVENT_ID _IOWR_BAD(SCHED_ID_MAGIC, 5, sizeof(struct sched_ioctl_para_get_event))
#define SCHED_ACK_EVENT_ID _IOWR_BAD(SCHED_ID_MAGIC, 6, sizeof(struct sched_ioctl_para_ack))
#define SCHED_WAIT_EVENT_ID _IOWR_BAD(SCHED_ID_MAGIC, 7, sizeof(struct sched_ioctl_para_wait))
#define SCHED_SUBMIT_EVENT_ID _IOWR_BAD(SCHED_ID_MAGIC, 8, sizeof(struct sched_ioctl_para_submit))
#define SCHED_ATTACH_PROCESS_TO_CHIP_ID _IOWR_BAD(SCHED_ID_MAGIC, 9, sizeof(struct sched_ioctl_para_attach))
#define SCHED_DETTACH_PROCESS_FROM_CHIP_ID _IOWR_BAD(SCHED_ID_MAGIC, 10, sizeof(struct sched_ioctl_para_detach))
#define SCHED_GRP_SET_EVENT_MAX_NUM \
    _IOWR_BAD(SCHED_ID_MAGIC, 11, sizeof(struct sched_ioctl_para_set_event_max_num))
#define SCHED_QUERY_INFO _IOWR_BAD(SCHED_ID_MAGIC, 12, sizeof(struct sched_ioctl_para_query_info))
#define SCHED_QUERY_SYNC_MSG_TRACE _IOWR_BAD(SCHED_ID_MAGIC, 13, sizeof(struct sched_ioctl_para_trace))

#define SCHED_GET_NODE_EVENT_TRACE _IOWR_BAD(SCHED_ID_MAGIC, 20, sizeof(struct sched_ioctl_para_get_event_trace))
#define SCHED_TRIGGER_SCHED_TRACE_RECORD_VALUE \
    _IOWR_BAD(SCHED_ID_MAGIC, 21, sizeof(struct sched_ioctl_para_trigger_sched_trace_record))

#define SCHED_ADD_TABLE_ENTRY _IOWR_BAD(SCHED_ID_MAGIC, 25, sizeof(struct sched_ioctl_para_add_table_entry))
#define SCHED_DEL_TABLE_ENTRY _IOWR_BAD(SCHED_ID_MAGIC, 26, sizeof(struct sched_ioctl_para_del_table_entry))
#define SCHED_QUERY_TABLE_ENTRY_STAT \
    _IOWR_BAD(SCHED_ID_MAGIC, 27, sizeof(struct sched_ioctl_para_query_table_entry_stat))

struct sched_ioctl_para_set_event_pri {
    unsigned int dev_id;
    unsigned int event_id;
    unsigned int pri;
};

struct sched_ioctl_para_set_proc_pri {
    unsigned int dev_id;
    unsigned int pri;
};

struct sched_ioctl_para_subscribe {
    unsigned int dev_id;
    unsigned int gid;
    unsigned int tid;
    unsigned long long event_bitmap;
};

struct sched_ioctl_para_set_event_max_num {
    unsigned int dev_id;
    unsigned int gid;
    unsigned int event_id;
    unsigned int max_num;
};

struct sched_ioctl_para_add_grp {
    unsigned int dev_id;
    unsigned int gid;
    unsigned int sched_mode;
    unsigned int thread_num;
    char grp_name[EVENT_MAX_GRP_NAME_LEN];
};

struct sched_ioctl_para_query_info {
    unsigned int dev_id;
    ESCHED_QUERY_TYPE type;
    struct esched_input_info input;
    struct esched_output_info output;
};

#define SCHED_SURPORT_MAX_CPU 512U
#define SCHED_MASK_BIT_NUM 64U
#define SCHED_MASK_NUM (SCHED_SURPORT_MAX_CPU / SCHED_MASK_BIT_NUM)
struct sched_sched_cpu_mask {
    unsigned long long mask[SCHED_MASK_NUM];
};

struct sched_ioctl_para_cpu_info {
    unsigned int dev_id;
    struct sched_sched_cpu_mask cpu_mask;
};

struct sched_ioctl_para_get_event_trace {
    unsigned int dev_id;
    char *buff;
    unsigned int buff_len;
    unsigned int data_len;
};

#define SCHED_STR_MAX_LEN 16
struct sched_ioctl_para_trigger_sched_trace_record {
    unsigned int dev_id;
    char record_reason[SCHED_STR_MAX_LEN];
    char key[SCHED_STR_MAX_LEN];
};

struct sched_ioctl_para_detach {
    unsigned int dev_id;
};


struct sched_ioctl_para_attach {
    unsigned int dev_id;
};

struct sched_ioctl_para_ack {
    unsigned int dev_id;
    unsigned int event_id;
    unsigned int subevent_id;
    unsigned int msg_len;
    char *msg;
};

struct sched_ioctl_para_submit {
    unsigned int dev_id;
    struct sched_published_event_info event_info;
};

struct sched_get_event_input {
    unsigned int dev_id;
    unsigned int grp_id;
    unsigned int thread_id;
    unsigned int event_id;
    unsigned int msg_len; /* input msg buff size */
    char *msg;
};

struct sched_ioctl_para_get_event {
    struct sched_get_event_input input;
    struct sched_subscribed_event event;
};

struct sched_wait_input {
    unsigned int dev_id;
    unsigned int grp_id;
    unsigned int thread_id;
    int timeout;
    unsigned int msg_len; /* input msg buff size */
    char *msg;
};

struct sched_ioctl_para_wait {
    struct sched_wait_input input;
    struct sched_subscribed_event event;
};

struct sched_ioctl_para_add_table_entry {
    unsigned int dev_id;
    unsigned int table_id;
    struct esched_table_key key;
    struct esched_table_entry entry;
};

struct sched_ioctl_para_del_table_entry {
    unsigned int dev_id;
    unsigned int table_id;
    struct esched_table_key key;
};

struct esched_table_query_input {
    unsigned int chip_id;
    unsigned int table_id;
    struct esched_table_key key;
};

struct sched_ioctl_para_query_table_entry_stat {
    struct esched_table_query_input input;
    struct esched_table_key_entry_stat stat;
};

struct sched_trace_input {
    unsigned int dev_id;
    unsigned int dev_pid;
    unsigned int gid;
    unsigned int tid;
};

struct sched_ioctl_para_trace {
    struct sched_trace_input input;
    struct sched_sync_event_trace trace;
};

#endif

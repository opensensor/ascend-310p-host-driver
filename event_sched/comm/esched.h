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

#ifndef EVENT_ESCHED_CORE_H
#define EVENT_ESCHED_CORE_H

#include "event_sched_inner.h"
#include "ascend_hal_define.h"
#include "sched_cmd.h"
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"
#include "ka_hashtable_pub.h"

#ifdef CFG_FEATURE_IDENTIFY_CP
#include "devdrv_manager_comm.h"
#endif

#ifndef EMU_ST
#include "drv_log.h"
#include "drv_share_log.h"
#else
#include "ut_log.h"
#endif

#ifdef CFG_FEATURE_VFIO
#include "esched_vf.h"
#endif

#ifdef CFG_FEATURE_HARDWARE_SCHED
#include "esched_drv.h"
#endif

#define SCHED_USEC_PER_SEC 1000000
#define SCHED_NSEC_PER_USEC 1000
#define SCHED_MSEC_SEC 1000

#define SCHED_VALID 1
#define SCHED_INVALID 0

#define SCHED_TRACE_ENABLE 1
#define SCHED_TRACE_DISABLE 0
#define SCHED_CURRENT_TGID (ka_task_get_current_tgid())
#define SCHED_CURRENT_COMM (ka_task_get_current_comm())
#define SCHED_CURRENT_PID (ka_task_get_current_pid())

/* In event sched, the meaning of chip is the same as that of device.  */
#ifdef CFG_FEATURE_EXTERNAL_CDEV

#ifdef ESCHED_HOST
#define SCHED_MAX_CHIP_NUM 2048 /* dc host */
#else
#define SCHED_MAX_CHIP_NUM 64 /* dc device */
#endif

#else

#ifdef ESCHED_HOST
#define SCHED_MAX_CHIP_NUM 1 /* helper host (Independent working mode) */
#else
#define SCHED_MAX_CHIP_NUM 1 /* mdc */
#endif

#endif

#define SCHED_MAX_EVENT_TYPE_NUM EVENT_MAX_NUM // 64

#ifdef CFG_FEATURE_MORE_PID_PRIORITY
#define SCHED_DEFAULT_EVENT_PRI PRIORITY_LEVEL1
#define SCHED_DEFAULT_PROC_PRI  PRIORITY_LEVEL2
#define SCHED_MAX_EVENT_PRI_NUM PRIORITY_LEVEL3
#define SCHED_MAX_PROC_PRI_NUM  PRIORITY_MAX
#else
#define SCHED_DEFAULT_EVENT_PRI PRIORITY_LEVEL1
#define SCHED_DEFAULT_PROC_PRI  PRIORITY_LEVEL1
#define SCHED_MAX_EVENT_PRI_NUM PRIORITY_LEVEL3
#define SCHED_MAX_PROC_PRI_NUM  PRIORITY_LEVEL3
#endif

#define SCHED_MAX_WEIGHT 1024
#define SCHED_MAX_EVENT_MSG_LEN  EVENT_MAX_MSG_LEN

#define SCHED_MAX_BATCH_EVENT_NUM (128 * 1024)

#define SCHED_GRP_EXCLUSIVE_STATUS_IDLE 0
#define SCHED_GRP_EXCLUSIVE_STATUS_RUN 1

#define SCHED_THREAD_STATUS_IDLE 0
#define SCHED_THREAD_STATUS_READY 1
#define SCHED_THREAD_STATUS_RUN 2

/* must define same with GROUP_TYPE */
#define SCHED_MODE_UNINIT 0
#define SCHED_MODE_SCHED_CPU 1
#define SCHED_MODE_NON_SCHED_CPU 2

#define SCHED_PROC_HASH_TABLE_BIT 10U
#define SCHED_PROC_HASH_TABLE_SIZE (0x00000001U << SCHED_PROC_HASH_TABLE_BIT)

/* For efficient queue management, the follows Must be a power of two */
#ifdef CFG_FEATURE_DEEPER_EVENT_QUE
# define SCHED_NODE_EVENT_RES_NUM (1024 * 64)
# define SCHED_CPU_EVENT_RES_NUM (1024 * 32)
# define SCHED_PUBLISH_EVENT_QUE_DEPTH (1024 * 8)
#else
# if defined(CFG_FEATURE_HARDWARE_SCHED) && !defined(ESCHED_HOST)
#  define SCHED_NODE_EVENT_RES_NUM 1024
#  define SCHED_CPU_EVENT_RES_NUM 256
#  define SCHED_PUBLISH_EVENT_QUE_DEPTH (1024 * 2)
# else
#  define SCHED_NODE_EVENT_RES_NUM (1024 * 8)
#  define SCHED_CPU_EVENT_RES_NUM (1024 * 2)
#  define SCHED_PUBLISH_EVENT_QUE_DEPTH (1024 * 16)
# endif
#endif

#define SCHED_GROUP_EVENT_LIST_DEPTH 1024    /* non_sched event share the same list in group */

#define SCHED_SWICH_THREAD_NUM 65536
#define SCHED_SWICH_THREAD_NUM_MASK (SCHED_SWICH_THREAD_NUM - 1)
#define SCHED_SWICH_THREAD_SHOW_NUM 32
#define SCHED_SWICH_THREAD_RECORD_NUM (SCHED_SWICH_THREAD_NUM - 8192)

#ifdef CFG_FEATURE_SCHEDULE_REALTIME
#define SCHED_THREAD_TIMEOUT 3000 /* Thread processing timeout, unit ms */
#else
#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define SCHED_THREAD_TIMEOUT 600000 /* Thread processing timeout, unit ms */
#else
#define SCHED_THREAD_TIMEOUT 60000 /* Thread processing timeout, unit ms */
#endif
#endif

#define SCHED_GUARD_WORK_PERIOD 10U /* unit ms */

/* When all threads in group timeout, drop the enent, unit ms */
#define SCHED_GROUP_DROP_EVENT_TIMEOUT 1000

#define SCHED_PUBLISH_FORM_USER 0
#define SCHED_PUBLISH_FORM_KERNEL 1

#define SCHED_WAKEUP_MODE_PULL 0
#define SCHED_WAKEUP_MODE_PUSH 1

#ifdef CFG_FEATURE_VFIO
/* prevent expansion after a process fails to get event */
#define SCHED_SEARCH_MAX_CNT ((SCHED_PUBLISH_EVENT_QUE_DEPTH) / 2 + 16)
#else
#define SCHED_SEARCH_MAX_CNT 128
#endif

#define SCHED_WAKED_BY_SELF 0
#define SCHED_WAKED_BY_OTHER_THREAD 1
#define SCHED_WAKED_BY_PUBLISHER 2
#define SCHED_WAKED_BY_EMPTY_CHECK 3
#define SCHED_WAKED_BY_DEL_CHECK 4
#define SCHED_WAKED_BY_GUARDWORK 5
#define SCHED_WAKED_BY_TIMEOUT 6
#define SCHED_WAKED_BY_EXCLUSIVE_FINISH 7

#define SCHED_HANDLE_EVENT_NORMAL                0
#define SCHED_CANNOT_HANDLE_THREAD_TIMEOUT       1
#define SCHED_CANNOT_HANDLE_NO_AVAIABLE_RESOURCE 2
#define SCHED_CANNOT_HANDLE_NO_SUBSCRIBE_THREAD  3
#define SCHED_CANNOT_HANDLE_GRP_EXCLUSIVE        4
#define SCHED_CANNOT_HANDLE_NOT_WAITING          5

#define SCHED_SAMPLE_TIME 600U /* unit s */
#define SCHED_SAMPLE_MIN_PERIOD SCHED_GUARD_WORK_PERIOD
#define SCHED_SAMPLE_MAX_PERIOD 1000U /* unit ms */
#define SCHED_SAMPLE_DEFAULT_PERIOD 10U /* unit ms, The value must be a multiple of SCHED_GUARD_WORK_PERIOD */
#define SCHED_SAMPLE_DATA_NUM (SCHED_SAMPLE_TIME * 1000U / SCHED_SAMPLE_DEFAULT_PERIOD)

#define ESCHED_WAKEUP_TIMEINTERVAL 3000 /* 3s */

/* log limit type */
#define SCHED_LOG_LIMIT_GET_REAL_PID  0
#define SCHED_LOG_LIMIT_GET_PROC_CTX  1
#define SCHED_LOG_LIMIT_EVENT_LIST    2
#define SCHED_LOG_LIMIT_MAX_NUM       3

#if defined(EVENT_SCHED_UT) || defined(EMU_ST)
extern unsigned long long sched_cur_cpu_tick;
#define SCHED_GET_CUR_SYSTEM_COUNTER(cnt) (cnt = sched_cur_cpu_tick)
#define SCHED_GET_SYSTEM_FREQ(cnt) (cnt = 1000000)
#else
#define SCHED_GET_CUR_SYSTEM_COUNTER(cnt) asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cnt) :)
#define SCHED_GET_SYSTEM_FREQ(cnt) asm volatile("mrs %0, CNTFRQ_EL0"  : "=r" (cnt) :)
#endif

/* in linux, tgid is thread group id, all thread is same in a process; pid, all the thread has it's unique pid
                     USER VIEW
<-- PID 43 --> <----------------- PID 42 ----------------->
                     +---------+
                     | process |
                    _| pid:42  |_
                  _/ | tgid=42 | \_ (new thread) _
        _ (fork) _/   +---------+                  \
       /                                        +---------+
+---------+                                    | process |
| process |                                    | pid=44  |
| pid=43  |                                    | tgid=42 |
| tgid=43 |                                    +---------+
+---------+
<-- PID 43 --> <--------- PID 42 --------> <--- PID 44 --->
                    KERNEL VIEW
*/
#define module_event_sched "event_sched"

static inline u32 sched_get_cur_processor_id(void)
{
    return ka_system_raw_smp_processor_id();
}
#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
#define sched_err(fmt, ...) do { \
    drv_err(module_event_sched, "<%s:%d:%d:%d> " fmt, \
    SCHED_CURRENT_COMM, SCHED_CURRENT_TGID, SCHED_CURRENT_PID, sched_get_cur_processor_id(), ##__VA_ARGS__); \
} while (0)
#else
#define sched_err(fmt, ...) do { \
    drv_err(module_event_sched, "<%s:%d:%d:%d> " fmt, \
    SCHED_CURRENT_COMM, SCHED_CURRENT_TGID, SCHED_CURRENT_PID, sched_get_cur_processor_id(), ##__VA_ARGS__); \
    share_log_err(ESCHED_SHARE_LOG_START, fmt, ##__VA_ARGS__); \
} while (0)
#endif
#define sched_warn(fmt, ...) do { \
    drv_warn(module_event_sched, "<%s:%d:%d:%d> " fmt, \
    SCHED_CURRENT_COMM, SCHED_CURRENT_TGID, SCHED_CURRENT_PID, sched_get_cur_processor_id(), ##__VA_ARGS__); \
} while (0)
#define sched_info(fmt, ...) do { \
    drv_info(module_event_sched, "<%s:%d:%d:%d> " fmt, \
    SCHED_CURRENT_COMM, SCHED_CURRENT_TGID, SCHED_CURRENT_PID, sched_get_cur_processor_id(), ##__VA_ARGS__); \
} while (0)
#define sched_debug(fmt, ...) do { \
    drv_pr_debug(module_event_sched, "<%s:%d:%d:%d> " fmt, \
    SCHED_CURRENT_COMM, SCHED_CURRENT_TGID, SCHED_CURRENT_PID, sched_get_cur_processor_id(), ##__VA_ARGS__); \
} while (0)

#if (defined AOS_LLVM_BUILD) || (defined EMU_ST)
#define sched_err_printk_ratelimited(fmt, ...)					\
    sched_err(fmt, ##__VA_ARGS__)
#else
#define SCHDED_RATELIMIT_INTERVAL (5 * HZ)
#define SCHDED_RATELIMIT_BURST 10
#define sched_err_printk_ratelimited(fmt, ...)					\
({									\
	static KA_BASE_DEFINE_RATELIMIT_STATE(sched_rate_limit,				\
				      SCHDED_RATELIMIT_INTERVAL,	\
				      SCHDED_RATELIMIT_BURST);		\
									\
    if (__ka_base_ratelimit(&sched_rate_limit))	{					\
        sched_err(fmt, ##__VA_ARGS__);				\
    } \
})
#endif


#define SCHED_MODULE_ID ka_get_module_id(HAL_MODULE_TYPE_EVENT_SCHEDULE, KA_SUB_MODULE_TYPE_0)

#define sched_kzalloc(size, flags) ka_kzalloc(size, flags, SCHED_MODULE_ID)
#define sched_kfree(addr) ka_kfree(addr, SCHED_MODULE_ID)

#define sched_vmalloc(size, gfp_mask, prot) __ka_vmalloc(size, gfp_mask, prot, SCHED_MODULE_ID)
#define sched_vzalloc(size)  ka_vzalloc(size, SCHED_MODULE_ID)
#define sched_vfree(addr) ka_vfree(addr, SCHED_MODULE_ID)

struct sched_numa_node;
struct sched_cpu_ctx;
struct sched_proc_ctx;
struct sched_grp_ctx;

/* show the destination information for the event alloced from event pool */
struct sched_event_trace {
    u32 type;
    u32 a;
    u32 b;
    u32 c;
};

struct sched_event_timestamp {
    u64 publish_user;
    u64 publish_user_of_day;
    u64 publish_in_kernel;
    u64 publish_add_event_list;
    u64 publish_out_kernel;
    u64 publish_wakeup;
    u64 subscribe_in_kernel;
    u64 subscribe_waked;
    u64 subscribe_out_kernel;
};

struct sched_event {
    ka_list_head_t list;
    struct sched_event_que *que; /* for quikly free event to poll */
    int32_t publish_pid;
    u32 publish_cpuid;
    int32_t wait_event_num_in_que;
#ifdef CFG_FEATURE_VFIO
    int32_t vfid;
#endif
    int32_t pid;
    u32 gid;
    u32 event_id;
    u32 subevent_id;
    u32 msg_len;
    char msg[SCHED_MAX_EVENT_MSG_LEN];
    struct sched_event_thread_map *event_thread_map;
    void *priv; /* Private event info for ack/finish. */
    int32_t (*event_ack_func)(u32 devid, u32 subevent_id, const char *msg, u32 msg_len, void *priv);
    void (*event_finish_func)(struct sched_event_func_info *finish_info, unsigned int finish_scene, void *priv);
    struct sched_event_timestamp timestamp;
    u64 proc_time;
    struct sched_event_trace trace;
};

struct sched_event_que_stat {
    u64 enque_full;
    u64 deque_empty;
    u32 max_use;
};

/*
 * sched event que.
 * head : Point to where the effective memory on the ring begins
 * tail : Point to the released ring position, when the memory is released, put it in this position
 */
struct sched_event_que {
    u32 depth;
    u32 mask;
    u32 head;
    u32 tail;
    struct sched_event **ring;
    ka_spinlock_t lock;
    struct sched_event_que_stat stat;
};

struct sched_event_list {
    u32 cur_num;
    u32 depth;
    u64 last_sched_num;
    u64 sched_num;
    u64 total_num;
    ka_spinlock_t lock;        /* used for sched event (aicpu) */
    ka_list_head_t head;
#ifdef CFG_FEATURE_VFIO
    u32 slice_cur_event_num[VMNG_VDEV_MAX_PER_PDEV];
#endif
};
/*
Use cases for event subscriptions:
1. Without subscription, all events sent to the group can be processed by all threads in the group;
2. Based on the group subscription, the group only processes the subscribed events, and the non-subscribed
   events are directly discarded, and all threads in the group can process;
3. Based on the thread subscription, the events sent to this group are forwarded if a processing thread
   can be found, and discarded if not found;
4, Based on group subscriptions, and then individually subscribe to a thread of events;

In order to accurately publish events to the specified thread, the in-process data organization form:
    proc
       |--grp0
       |     |--event0
       |     |       |--subscribe tread0
       |     |       |--subscribe tread1
       |     |
       |     |--event1
       |     |       |--subscribe tread0
       |     |       |--subscribe tread2
       |     |
       |     |--thread0
       |     |       |--bind to cpuid1, subscribe event 0,1
       |     |
       |     |--thread1
       |     |       |--bind to cpuid2, subscribe event 0
       |     |
       |     |--thread2
       |     |       |--bind to cpuid0, subscribe event 1
       |
       |--grp1
       |     |--event0
       |     |       |--subscribe tread0
       |     |       |--subscribe tread1
       |     |
       |     |--event1
       |     |       |--subscribe tread1
       |     |       |--subscribe tread4
*/
struct sched_event_thread_map {
#ifndef CFG_SOC_PLATFORM_MDC_V51
    ka_kref_t ref;
#endif
    u32 thread_num;
    u32 *thread; /* The threads who subscribe the event */
};

struct sched_event_timeout_info {
    u32 timeout_flag;
    u32 timeout_thread_num;
    u64 timeout_timestamp;
};

struct sched_thread_stat {
    u32 sched_event;
    ka_atomic_t discard_event;
    u32 timeout_cnt;
};

struct sched_thread_ctx {
    u32 valid;
    ka_atomic_t status;
    u32 wait_flag; /* set valid in wait event, invalid in wait event return */
#ifdef CFG_FEATURE_THREAD_SWAPOUT
    u32 swapout_flag;
#endif
    u32 timeout_flag;
    u32 timeout_detected;
    u32 tid; /* Threads in the group, numbered from 0 */
    u32 bind_cpuid;
    u32 kernel_tid; /* system task numver, SCHED_CURRENT_PID */
    int32_t pre_normal_wakeup_reason;
    int32_t normal_wakeup_reason;
    int32_t event_finish_scene;
    char name[TASK_COMM_LEN];
    u64 start_time;
    u64 end_time;
    u64 callback_end_time;
    u64 max_proc_time;
    u64 total_proc_time;
    u64 max_sched_time;
    u64 total_sched_time;
    u64 non_sched_publish_wakeup;
    u64 subscribe_event_bitmap;
    ka_mutex_t thread_mutex;
    ka_spinlock_t thread_finish_lock;
    struct sched_thread_stat stat;
    struct sched_grp_ctx *grp_ctx;
    struct sched_event *event;
    ka_wait_queue_head_t wq;
};

struct sched_grp_ctx {
    int32_t pid;
    u32 gid;
    u32 sched_mode; /* SCHED_MODE_ */
    u32 last_thread;
    u32 cfg_thread_num;
    u32 thread_num;
    u32 *thread; /* The threads in group */
    ka_spinlock_t lock;
    struct sched_proc_ctx *proc_ctx;
    struct sched_thread_ctx *thread_ctx; /* store each thread context */
    u32 *cpuid_to_tid;
    struct sched_event_thread_map event_thread_map[SCHED_MAX_EVENT_TYPE_NUM];
    struct sched_event_timeout_info event_timeout_info[SCHED_MAX_EVENT_TYPE_NUM];
    u32 max_event_num[SCHED_MAX_EVENT_TYPE_NUM];
    ka_atomic_t event_num[SCHED_MAX_EVENT_TYPE_NUM];
    ka_atomic_t drop_event_num[SCHED_MAX_EVENT_TYPE_NUM];
    u32 is_exclusive;
    ka_atomic_t run_status;
    ka_atomic_t wait_cpu_mask;
    u32 cur_tid;

    struct sched_event_list published_event_list[SCHED_MAX_EVENT_PRI_NUM];
    ka_atomic_t cur_event_num;
    char name[EVENT_MAX_GRP_NAME_LEN];
};

struct sched_event_stat {
    ka_atomic_t publish_event_num;
    ka_atomic_t sched_event_num;
};

struct sched_proc_ctx {
    ka_hlist_node_t link; /* hash find link */
    ka_list_head_t list; /* recycle list */
    ka_atomic_t refcnt;
    int32_t pid;
    u32 task_id;
    int32_t host_pid;
#ifndef AOS_LLVM_BUILD
    ka_mnt_namespace_t *mnt_ns;
#endif
#ifdef CFG_FEATURE_IDENTIFY_CP
    enum devdrv_process_type cp_type;
#endif
    int32_t vfid;
    u32 status;
    u64 start_timestamp;
    u64 exit_timestamp;
    u32 pri;
    ka_atomic_t publish_event_num;
    ka_atomic_t sched_event_num;
    struct sched_event_stat proc_event_stat[SCHED_MAX_EVENT_TYPE_NUM];
    char name[TASK_COMM_LEN];
    struct sched_numa_node *node;
    u32 event_pri[SCHED_MAX_EVENT_TYPE_NUM];
    struct sched_grp_ctx grp_ctx[SCHED_MAX_GRP_NUM];
    ka_work_struct_t release_work;
    struct sched_sync_event_trace sched_dfx[SCHED_MAX_EX_GRP_NUM][SCHED_MAX_SYNC_THREAD_NUM_PER_GRP];
};

struct esched_thread {
    int pid;
    u32 task_id;
    u32 gid;
    u32 tid;
};

struct esched_abnormal_thread_record {
    u32 event_id;
    u32 sub_event_id;
    u64 cpu_sched_time;
    u64 curr_timestamp;
    struct esched_thread thread_info;
};

struct sched_cpu_sched_thread {
    u64 publish_timestamp;
    u64 add_queue_list_timestamp;
    u64 sched_timestamp;
    u64 finish_timestamp;
    u64 wait_time; /* tick */
    u64 callback_timestamp;
    u64 waked_to_wakeup_time;
    int32_t pid;
    u32 pid_pri;
    u32 gid;
    u32 tid;
    u32 event_id;
    u32 event_pri;
    u32 cur_event_num;
    int32_t normal_wakeup_reason;
};

typedef enum sample_type {
    NODE_SAMPLE_TYPE = 0,
    PID_SAMPLE_TYPE = 1
} SAMPLE_TYPE_VALUE;

struct sched_cpu_stat {
    u64 sched_event_num;
    u64 check_sched_num;
    u64 proc_exit_drop_num;
    u64 exclusive_sched_num;
    u64 timeout_cnt;
};

struct sched_cpu_perf_stat {
    u64 publish_syscall_total_time;
    u64 publish_syscall_max_time;
    u64 publish_in_kernel_total_time;
    u64 publish_in_kernel_max_time;
    u64 wakeup_total_time;
    u64 wakeup_max_time;
    u64 publish_subscribe_total_time;
    u64 publish_subscribe_max_time;
    u64 total_wakeup_event_num;
    u64 total_publish_event_num;
    u64 total_sched_event_num;
    u64 total_publish_fail_event_num;
    u64 publish_event_num[SCHED_MAX_EVENT_TYPE_NUM];
    u64 sched_event_num[SCHED_MAX_EVENT_TYPE_NUM];
    u64 publish_fail_event_num[SCHED_MAX_EVENT_TYPE_NUM];
    u64 total_use_time;
};

struct sched_cpu_sample_data {
    u64 timestamp;
    u64 total_use_time; /* tick */
};

struct sched_cpu_sample {
    u32 record_num;
    struct sched_cpu_sample_data data[SCHED_SAMPLE_DATA_NUM];
};

struct sched_cpu_sched_trace {
    u64 trace_num;
    u32 record_index;
    struct sched_cpu_sched_thread sched_thread_list[SCHED_SWICH_THREAD_NUM];
};

enum sched_cpu_status {
    CPU_STATUS_IDLE = 0,
    CPU_STATUS_READY,
    CPU_STATUS_RUN
};

struct sched_cpu_ctx {
    u32 cpuid; /* cpu index in the numa node */
    ka_atomic_t cur_event_num;
    ka_atomic_t cpu_status;
    u32 cannot_handle_event_reason;
    u64 last_sched_timestamp;
    u64 last_sched_event;
    u64 last_thread_start_time;
    struct sched_cpu_stat stat;
    struct sched_cpu_perf_stat perf_stat;
    /* The thread currently being scheduled by the cpu. If it is empty, it indicates that cpu is idle. */
    struct esched_thread thread;
    struct esched_abnormal_thread_record cpu_abnormal_thread;
    ka_spinlock_t sched_lock;
    struct sched_numa_node *node;
    struct sched_event *event_base; /* for free mem of event ring in event_res */
    struct sched_event_que event_res; /* The events posted by this cpu, get event resources from here */
#ifdef CFG_FEATURE_HARDWARE_SCHED
    struct topic_data_chan *topic_chan;
#endif
    struct sched_cpu_sample *sample;
    struct sched_cpu_sched_trace *sched_trace;
};

#define SCHED_ABNORMAL_EVENT_MAX_NUM 1024
#define SCHED_DEFAULT_PROC_TIME_THRES 10000 /* us */
#define SCHED_DEFAULT_PUBLISH_SYSCALL_TIME_THRES 4 /* us */
#define SCHED_DEFAULT_PUBLISH_IN_KERNEL_TIME_THRES 3 /* us */
#define SCHED_DEFAULT_WAKEUP_TIME_THRES 1000 /* us */
#define NON_SCHED_DEFAULT_WAKEUP_TIME_THRES 100000 /* us */
#define SCHED_DEFAULT_PUBLISH_SUBSCRIBE_TIME_THRES 10000 /* us */

struct sched_abnormal_event_item {
    struct sched_event_timestamp timestamp;
    u64 proc_time;
    u32 event_id;
    int32_t publish_pid;
    u32 publish_cpuid;
    int32_t wait_event_num_in_que;
    int32_t pid;
    u32 gid;
    u32 tid;
    u32 bind_cpuid;
    u32 kernel_tid; /* system task numver, SCHED_CURRENT_PID */
    char name[TASK_COMM_LEN];
};

struct sched_abnormal_event {
    u64 timestamp_thres;
    ka_atomic_t cur_index;
    struct sched_abnormal_event_item event_info[SCHED_ABNORMAL_EVENT_MAX_NUM];
};

struct sched_node_abnormal_event {
    struct sched_abnormal_event proc;
    struct sched_abnormal_event publish_syscall;
    struct sched_abnormal_event publish_in_kernel;
    struct sched_abnormal_event wakeup;
    struct sched_abnormal_event publish_subscribe;
    struct sched_abnormal_event thread_run;
};

#define SCHED_EVENT_TRACE_MAX_NUM 2048
struct sched_node_event_trace {
    ka_atomic_t cur_index;
    volatile int32_t enable_flag;
    struct sched_abnormal_event_item event_info[SCHED_EVENT_TRACE_MAX_NUM];
};

#define SCHED_ASSIST_TIME_THRES_VALUE 1000 /* unit us */

struct sched_event_sample_data {
    u64 timestamp;
    u64 total_event_num;
    u64 cur_event_num;
    u64 publish_event_num[SCHED_MAX_EVENT_TYPE_NUM];
    u64 sched_event_num[SCHED_MAX_EVENT_TYPE_NUM];
};

struct sched_event_sample {
    u32 record_num;
    struct sched_event_sample_data data[SCHED_SAMPLE_DATA_NUM];
};

#define SCHED_RECORD_TRACE_CNT 80

struct sched_trace_record_info {
    ka_spinlock_t lock;
    u32 valid;
    u32 num;
    u64 timestamp;
    char record_reason[SCHED_STR_MAX_LEN];
    char key[SCHED_STR_MAX_LEN];
};

struct pid_entry {
    ka_list_head_t list;
    int pid;
};

#define SCHED_DEBUG_EVENT_TRACE_BIT 0
#define SCHED_DEBUG_SAMPLE_BIT 1
#define SCHED_DEBUG_TRIGGER_SAMPLE_FILE_BIT 2
#define SCHED_DEBUG_SCHED_TRACE_RECORD_BIT 3
#define SCHED_DEBUG_TRIGGER_SCHED_TRACE_RECORD_FILE_BIT 4

struct sched_dev_ops {
    int (*sumbit_event)(u32 dev_id, u32 event_src,
        struct sched_published_event_info *event_info, struct sched_published_event_func *event_func);
    struct sched_thread_ctx* (*sched_cpu_get_next_thread)(struct sched_cpu_ctx *cpu_ctx);
    void (*sched_cpu_to_idle)(struct sched_thread_ctx *thread_ctx, struct sched_cpu_ctx *cpu_ctx);
    struct sched_event* (*sched_cpu_get_event)(struct sched_cpu_ctx *cpu_ctx,
        struct sched_thread_ctx *thread_ctx, u32 event_id);

    int (*conf_sched_cpu)(struct sched_numa_node *node, u32 sched_cpu_num);
    int (*map_host_pid)(struct sched_proc_ctx *proc_ctx);
    void (*unmap_host_pid)(struct sched_proc_ctx *proc_ctx);

    void (*task_exit_check_sched_cpu)(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx);
    void (*try_resched_cpu)(struct sched_numa_node *node);
    int (*node_res_init)(struct sched_numa_node *node);
    void (*node_res_uninit)(struct sched_numa_node *node);
};

#define NON_SCHED_DEFAULT_CPUID 0

struct sched_numa_node {
    u32 node_id;
    ka_atomic_t refcnt;
    u32 cpu_num; /* os total cpu num, size of cpu_ctx */
    u32 sched_cpu_num;
    u32 *sched_cpuid; /* Stores the absolute ID (starts from 0 in OS) of the CPU used for scheduling. */
    u32 debug_flag;
    u32 sample_period;
    u32 sample_interval;
    u32 sched_set_cpu_flag;
    int sample_proc_id;
    u32 cur_task_id;
    ka_mutex_t node_mutex;
    ka_delayed_work_t guard_work;
    ka_mutex_t proc_mng_mutex;
    ka_list_head_t del_proc_head; /* After the process exits, a linked list of resources needs to be released */
    ka_mutex_t pid_list_mutex;
    ka_list_head_t pid_list;
    ka_rwlock_t proc_hash_table_rwlock[SCHED_PROC_HASH_TABLE_SIZE];
    KA_DECLARE_HASHTABLE(proc_hash_table, SCHED_PROC_HASH_TABLE_BIT);
    struct sched_cpu_ctx **cpu_ctx;
    struct sched_event *event_base; /* for free mem of event ring in event_res */
    struct sched_event_que event_res; /* public events resources */
    struct sched_node_event_trace event_trace;
    struct sched_node_abnormal_event abnormal_event;
    struct sched_event_sample *node_event_sample;
    struct sched_event_sample *proc_event_sample;
    ka_mutex_t node_guard_work_mutex;
    struct sched_trace_record_info trace_record;
#ifdef CFG_FEATURE_HARDWARE_SCHED
    struct sched_hard_res hard_res;
#endif
    /* Priority grouped event list published to this node */
    struct sched_event_list published_event_list[SCHED_MAX_PROC_PRI_NUM][SCHED_MAX_EVENT_PRI_NUM];
    ka_atomic_t cur_event_num;
#ifdef CFG_FEATURE_VFIO
    struct sched_slice_ctx slice_ctx;
#endif
    struct sched_dev_ops ops;
};

/* inline */
/* get time of day */
static inline u64 sched_do_gettimeofday(void)
{
    ka_timeval_t submit_event_time;

    ka_system_do_gettimeofday(&submit_event_time);
    return ((u64)ka_system_get_timeval_sec(&submit_event_time) * SCHED_USEC_PER_SEC) + (u64)ka_system_get_timeval_nsec(&submit_event_time);
}

/* get cpu tick */
static inline u64 sched_get_cur_cpu_tick(void)
{
#if defined(__arm__) || defined(__aarch64__) || defined(EVENT_SCHED_UT)
    u64 cnt = 0;
    SCHED_GET_CUR_SYSTEM_COUNTER(cnt);
    return cnt;
#else
    ka_timespec64_t timestamp;
    ka_system_ktime_get_ts64(&timestamp);
    return (((u64)ka_system_timespec64_get_tv_sec(&timestamp) * SCHED_USEC_PER_SEC) +
        ((u64)ka_system_timespec64_get_tv_nsec(&timestamp) / SCHED_NSEC_PER_USEC));
#endif
}

static inline u64 sched_get_cur_timestamp(void)
{
    return sched_get_cur_cpu_tick();
}

static inline u64 sched_get_sys_freq(void)
{
#if defined(__arm__) || defined(__aarch64__) || defined(EVENT_SCHED_UT)
    u64 cnt = 0;
    SCHED_GET_SYSTEM_FREQ(cnt);
    return cnt;
#else
    return SCHED_USEC_PER_SEC; /* sched_get_cur_cpu_tick return microsecond */
#endif
}

static inline u64 microsecond_to_tick(u64 duration)
{
    u64 freq = sched_get_sys_freq();
    if (freq == 0) {
        return 0;
    }

    // tickFreq is record by second, to microsecond need multiply 1000000
    return duration * (freq / SCHED_USEC_PER_SEC);
}

static inline u64 tick_to_microsecond(u64 tick)
{
    u64 freq = sched_get_sys_freq();
    // tickFreq is record by second, to microsecond need multiply 1000000
    if ((freq / SCHED_USEC_PER_SEC) != 0) {
        return tick / (freq / SCHED_USEC_PER_SEC);
    }

    return 0;
}

static inline u64 tick_to_millisecond(u64 tick)
{
    u64 freq = sched_get_sys_freq();
    // tickFreq is record by second, to millisecond need multiply 1000
    if ((freq / SCHED_MSEC_SEC) != 0) {
        return tick / (freq / SCHED_MSEC_SEC);
    }

    return 0;
}

static inline u64 millisecond_to_tick(u64 duration)
{
    u64 freq = sched_get_sys_freq();
    if (freq == 0) {
        return 0;
    }

    // tickFreq is record by second, to millisecond need multiply 1000
    return duration * (freq / SCHED_MSEC_SEC);
}

static inline bool sched_is_cpu_sched(const struct sched_numa_node *node, u32 cpuid)
{
    return ((cpuid != 0) && (node->cpu_ctx[cpuid] != NULL));
}

static inline struct sched_cpu_ctx *sched_get_cpu_ctx(struct sched_numa_node *node, u32 cpuid)
{
    return node->cpu_ctx[cpuid];
}

static inline struct sched_proc_ctx *sched_get_proc_ctx(struct sched_numa_node *node, int32_t pid)
{
    struct sched_proc_ctx *proc_ctx = NULL;

    /*lint -e666 */
    ka_hash_for_each_possible(node->proc_hash_table, proc_ctx, link, pid) {
        if (proc_ctx->pid == pid) {
            return proc_ctx;
        }
    }
    /*lint +e666 */

    return NULL;
}

void sched_free_process(struct sched_proc_ctx *proc_ctx);
struct sched_numa_node *sched_get_numa_node(u32 chip_id);

void esched_setup_dev_soft_ops(struct sched_dev_ops *ops);
int32_t esched_create_dev(u32 devid, struct sched_dev_ops *ops);
void esched_destroy_dev(u32 devid);
struct sched_numa_node *esched_dev_get(u32 devid);
void esched_dev_put(struct sched_numa_node *node);
int32_t sched_get_firt_ctrlcpu(void);

static inline struct sched_proc_ctx *esched_proc_get(struct sched_numa_node *node, int32_t pid)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    u32 bucket_index = ka_hash_min(pid, SCHED_PROC_HASH_TABLE_BIT);

    ka_task_read_lock_bh(&node->proc_hash_table_rwlock[bucket_index]);
    proc_ctx = sched_get_proc_ctx(node, pid);
    /* The SCHED_INVALID status means another thread has been waiting for the lock to delete the proc from
       hash table, so the current thread does not need to continue and should release the lock immediately. */
    if ((proc_ctx == NULL) || (proc_ctx->status == SCHED_INVALID)) {
        ka_task_read_unlock_bh(&node->proc_hash_table_rwlock[bucket_index]);
        return NULL;
    }

    ka_base_atomic_inc(&proc_ctx->refcnt);
    ka_task_read_unlock_bh(&node->proc_hash_table_rwlock[bucket_index]);
    return proc_ctx;
}

static inline void esched_proc_put(struct sched_proc_ctx *proc_ctx)
{
    /* When the refcnt is 0, the proc_ctx to be freed should have been deleted from the hash table. */
    if (ka_base_atomic_dec_return(&proc_ctx->refcnt) > 0) {
        return;
    }

#ifndef AOS_LLVM_BUILD
    if (ka_base_in_atomic()) {
        (void)esched_dev_get(proc_ctx->node->node_id);
        ka_task_schedule_work_on(sched_get_firt_ctrlcpu(), &proc_ctx->release_work);
        return;
    }
#endif
    sched_free_process(proc_ctx);
}

static inline struct sched_proc_ctx *esched_chip_proc_get(u32 chip_id, int32_t pid)
{
    struct sched_numa_node *node = sched_get_numa_node(chip_id);

    if (node == NULL) {
        sched_err("Para err. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return NULL;
    }

    return esched_proc_get(node, pid);
}

static inline bool esched_dst_engine_is_local(u32 dst_engine)
{
#ifndef CFG_FEATURE_REMOTE_SUBMIT
    if ((dst_engine == TS_CPU) || (dst_engine == DVPP_CPU)) {
        return false;
    }
    return true;
#else
    if ((dst_engine == ACPU_LOCAL) || (dst_engine == CCPU_LOCAL)) {
        return true;
    }

#ifndef ESCHED_HOST
    if ((dst_engine == ACPU_DEVICE) || (dst_engine == CCPU_DEVICE) || (dst_engine == DCPU_DEVICE)) {
        return true;
    }
#else
    if ((dst_engine == ACPU_HOST) || (dst_engine == CCPU_HOST)) {
        return true;
    }
#endif

    /* set local flag false when dst_engine is TS_CPU or DVPP_CPU */
    return false;
#endif
}

static inline void esched_chip_proc_put(struct sched_proc_ctx *proc_ctx)
{
    esched_proc_put(proc_ctx);
}

static inline struct sched_grp_ctx *sched_get_grp_ctx(struct sched_proc_ctx *proc_ctx, u32 gid)
{
    return &proc_ctx->grp_ctx[gid];
}

static inline struct sched_thread_ctx *sched_get_thread_ctx(struct sched_grp_ctx *grp_ctx, u32 tid)
{
    return &grp_ctx->thread_ctx[tid];
}

static inline struct sched_event_list *sched_get_non_sched_event_list(struct sched_grp_ctx *grp_ctx, u32 event_pri)
{
    return &grp_ctx->published_event_list[event_pri];
}

static inline struct sched_event_list *sched_get_sched_event_list(struct sched_numa_node *node, u32 proc_pri,
    u32 event_pri)
{
    return &node->published_event_list[proc_pri][event_pri];
}

static inline bool sched_is_que_empty(const struct sched_event_que *que)
{
    return (que->head == que->tail);
}

static inline u32 sched_que_element_num(struct sched_event_que *que)
{
    return (que->tail - que->head);
}

static inline bool sched_is_que_full(struct sched_event_que *que)
{
    return (sched_que_element_num(que) == que->depth);
}

static inline bool esched_is_cpu_idle(struct sched_cpu_ctx *cpu_ctx)
{
    return (cpu_ctx->thread.pid == 0);
}

static inline bool esched_is_cpu_cur_thread(struct sched_cpu_ctx *cpu_ctx, struct sched_thread_ctx *thread_ctx)
{
    return ((cpu_ctx->thread.pid == thread_ctx->grp_ctx->pid) &&
            (cpu_ctx->thread.task_id == thread_ctx->grp_ctx->proc_ctx->task_id) &&
            (cpu_ctx->thread.gid == thread_ctx->grp_ctx->gid) && (cpu_ctx->thread.tid == thread_ctx->tid));
}

static inline void esched_cpu_cur_thread_set(struct sched_cpu_ctx *cpu_ctx, struct sched_thread_ctx *thread_ctx)
{
    cpu_ctx->thread.pid = thread_ctx->grp_ctx->pid;
    cpu_ctx->thread.task_id = thread_ctx->grp_ctx->proc_ctx->task_id;
    cpu_ctx->thread.gid = thread_ctx->grp_ctx->gid;
    cpu_ctx->thread.tid = thread_ctx->tid;
}

static inline void esched_cpu_cur_thread_clr(struct sched_cpu_ctx *cpu_ctx)
{
    cpu_ctx->thread.pid = 0;
}

static inline void esched_cpu_idle(struct sched_cpu_ctx *cpu_ctx)
{
    ka_base_atomic_set(&cpu_ctx->cpu_status, CPU_STATUS_IDLE);
    esched_cpu_cur_thread_clr(cpu_ctx);
}

static inline struct sched_thread_ctx *esched_cpu_cur_thread_get_inner(struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_proc_ctx *proc_ctx = esched_proc_get(cpu_ctx->node, cpu_ctx->thread.pid);
    struct sched_thread_ctx *thread_ctx = NULL;

    if (proc_ctx != NULL) {
        if (cpu_ctx->thread.task_id == proc_ctx->task_id) {
            thread_ctx = sched_get_thread_ctx(sched_get_grp_ctx(proc_ctx, cpu_ctx->thread.gid), cpu_ctx->thread.tid);
            if (thread_ctx->valid == SCHED_VALID) {
                return thread_ctx;
            }
        }

        esched_proc_put(proc_ctx);
    }

    return NULL;
}

static inline struct sched_thread_ctx *esched_cpu_cur_thread_get(struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_thread_ctx *thread_ctx = NULL;

    if (cpu_ctx->thread.pid == 0) {
        return NULL;
    }

    ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
    thread_ctx = esched_cpu_cur_thread_get_inner(cpu_ctx);
    ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);

    return thread_ctx;
}

static inline void esched_cpu_cur_thread_put(struct sched_thread_ctx *thread_ctx)
{
    esched_proc_put(thread_ctx->grp_ctx->proc_ctx);
}

static inline struct sched_thread_ctx *esched_get_proc_thread_on_cpu(struct sched_proc_ctx *proc_ctx,
    struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_thread_ctx *thread_ctx = NULL;

    if ((cpu_ctx->thread.pid != proc_ctx->pid) || (cpu_ctx->thread.task_id != proc_ctx->task_id)) {
        return NULL;
    }
    ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
    thread_ctx = sched_get_thread_ctx(sched_get_grp_ctx(proc_ctx, cpu_ctx->thread.gid), cpu_ctx->thread.tid);
    ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);

    return thread_ctx;
}

bool esched_log_limited(u32 type);
u32 esched_get_dev_num(void);
u32 esched_get_numa_node_num(void);
u32 sched_get_previous_cpu_num(u32 chip_id);
int32_t sched_grp_add_thread(struct sched_grp_ctx *grp_ctx, u32 tid, u32 cpuid);
int32_t sched_grp_set_max_event_num(u32 chip_id, int32_t pid, u32 gid, u32 event_id, u32 max_num);
void sched_trace_record(struct sched_numa_node *node, struct sched_trace_record_info *trace_record);
struct sched_event *sched_alloc_event(struct sched_numa_node *node);
void sched_wake_up_thread(struct sched_thread_ctx *thread_ctx);
int32_t sched_event_enque_lock(struct sched_event_que *que, struct sched_event *event);
void sched_thread_finish(struct sched_thread_ctx *thread_ctx, u32 finish_scene);
int sched_event_add_thread(struct sched_event *event, u32 tid);
void sched_publish_state_update(struct sched_numa_node *node, struct sched_event *event, int32_t ret);

#ifndef AOS_LLVM_BUILD
ka_mnt_namespace_t *sched_get_proc_mnt_ns(u32 chip_id, int pid);
void esched_try_cond_resched_by_time(u32 *pre_stamp, u32 time_threshold);
#endif

int32_t sched_grp_event_num_update(struct sched_grp_ctx *grp_ctx, u32 event_id);

#ifdef CFG_FEATURE_HARDWARE_SCHED
void esched_setup_dev_hw_ops(struct sched_dev_ops *ops);
#endif
struct sched_thread_ctx *sched_get_next_thread(struct sched_cpu_ctx *cpu_ctx);
void sched_cpu_to_idle(struct sched_thread_ctx *thread_ctx, struct sched_cpu_ctx *cpu_ctx);
void sched_check_sched_task(struct sched_numa_node *node);
int32_t sched_publish_event_to_sched_grp(struct sched_event *event, struct sched_grp_ctx *grp_ctx);
void sched_task_exit_check_sched_cpu(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx);

#ifdef CFG_FEATURE_VFIO
extern int32_t sched_publish_check_event_source(u32 event_src, struct sched_proc_ctx *dest_proc_ctx);
extern int32_t sched_vf_proc_ctx_init(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx);
extern void sched_vf_proc_ctx_num_inc(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx);
extern void sched_vf_proc_ctx_num_dec(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx);
extern void sched_release_cpu_res(struct sched_thread_ctx *thread_ctx);
extern void sched_event_list_vf_init(struct sched_event_list *event_list);
extern bool sched_use_vf_cpu_handle_event(struct sched_numa_node *node, u32 vfid, struct sched_cpu_ctx *cpu_ctx);
extern bool sched_vf_has_free_cpu(struct sched_vf_ctx *vf_ctx);
extern struct sched_vf_ctx *sched_get_vf_ctx(struct sched_numa_node *node, u32 vfid);
#endif
#if defined(CFG_FEATURE_VFIO) || (defined(CFG_FEATURE_HARDWARE_MIA) && !defined(ESCHED_HOST))
extern bool sched_is_cpu_belongs_to_vf(struct sched_numa_node *node, u32 vfid, struct sched_cpu_ctx *cpu_ctx);
#endif
bool sched_grp_can_handle_event(struct sched_grp_ctx *grp_ctx, struct sched_event *event);
int32_t sched_publish_event_to_non_sched_grp(struct sched_event *event, struct sched_grp_ctx *grp_ctx);
int32_t sched_non_sched_event_add_tail(struct sched_event_list *event_list, struct sched_event *event);

/* ioctrl */
int32_t sched_set_sched_cpu(u32 chip_id, struct sched_sched_cpu_mask *cpu_mask);
int32_t sched_proc_add_grp(u32 chip_id, u32 gid, const char *grp_name, u32 sched_mode, u32 thread_num);
int32_t sched_set_event_priority(u32 chip_id, int32_t pid, u32 event_id, u32 pri);
int32_t sched_set_process_priority(u32 chip_id, int32_t pid, u32 pri);
int32_t sched_thread_subscribe_event(u32 chip_id, int32_t pid, u32 gid, u32 tid, u64 event_bitmap);
int32_t sched_grp_subscribe_event(u32 chip_id, int32_t pid, u32 gid, u64 event_bitmap);
int32_t sched_get_exact_event(u32 chip_id, int32_t pid, u32 gid, u32 tid, u32 event_id,
    struct sched_subscribed_event *subscribed_event);
struct sched_ack_event_para {
    u32 event_id;
    u32 subevent_id;
    const char *msg;
    u32 msg_len;
};
int32_t sched_ack_event(u32 chip_id, int32_t pid, struct sched_ack_event_para *para);
int32_t sched_wait_event(u32 chip_id, int32_t pid, u32 gid, u32 tid, int32_t timeout,
    struct sched_subscribed_event *subscribed_event);
int32_t sched_publish_event(u32 chip_id, u32 event_src,
    struct sched_published_event_info *event_info, struct sched_published_event_func *event_func);
int32_t esched_submit_event_distribute(u32 chip_id, u32 event_src,
    struct sched_published_event_info *event_info, struct sched_published_event_func *event_func);
int32_t sched_get_event_trace(u32 chip_id, char *buff, u32 buff_len, u32 *data_len);
int32_t sched_trigger_sched_trace_record(u32 chip_id, const char *reason, const char *key);
int32_t sched_get_sched_cpuid_in_node(struct sched_numa_node *node, u32 *cpuid);
struct sched_thread_ctx *sched_get_cur_thread_in_grp(struct sched_grp_ctx *grp_ctx);
int32_t sched_publish_event_to_remote(u32 chip_id, u32 event_src,
    const struct sched_published_event_info *event_info, struct sched_published_event_func *event_func);
int sched_query_remote_task_gid_msg_send(u32 chip_id, int pid, const char *grp_name, u32 *gid);
int sched_query_remote_task_gid(u32 chip_id, int pid, const char *grp_name, u32 *gid);
void sched_wakeup_process_all_thread(struct sched_proc_ctx *proc_ctx);
int sched_query_remote_trace_msg_send(u32 chip_id,
    u32 pid, u32 gid, u32 tid, struct sched_sync_event_trace *sched_trace);
void sched_query_thread_run_time(u32 chip_id);

int sched_query_tid_in_grp(u32 chip_id, int pid, u32 gid, u32 os_tid, u32 *tid);

/* open and release */
int32_t sched_add_process(u32 chip_id, int32_t pid);
int32_t sched_del_process(u32 chip_id, int32_t pid);

/* module init uninit */
int32_t esched_init(void);
void esched_uninit(void);

int32_t esched_client_init(void);
void esched_client_uninit(void);

void esched_wait_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event);
void esched_publish_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event);
void esched_submit_trace_update(u32 event_src, struct sched_numa_node *node, struct sched_published_event_info *event_info);
void sched_get_cpuid_in_node(struct sched_numa_node *node, u32 *cpuid);

#endif


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

#ifndef _HDCDRV_MAIN_H_
#define _HDCDRV_MAIN_H_

#include "drv_log.h"
#include "devdrv_interface.h"
#include "virtmng_interface.h"
#include "hdcdrv_cmd.h"
#include "hdcdrv_epoll.h"
#include "hdcdrv_mem.h"
#include "hdcdrv_core_com.h"
#include "hdcdrv_mem_com.h"

#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_compiler_pub.h"
#include "ka_common_pub.h"

#ifdef CFG_FEATURE_VFIO
#include "hdcdrv_adapter.h"
#endif

#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_VFIO_DEVICE)
#include "virtmng_interface.h"
#endif

#ifdef HDC_ENV_DEVICE
#include <linux/securec.h>
#else
#include "securec.h"
#endif

#define PCIE_DEV_NAME "/dev/hisi_hdc"

#define HDCDRV_ASYNC_RELEASE_WORK_NAME "hdcdrv-async-release-work"
#define HDCDRV_RX_MSG_NORIFY_WORK_NAME "hdcdrv-rx-msg-notify-work"

#define HDCDRV_STR_NAME(n)      (#n)
#define HDCDRV_STR_NAME_LEN     32

#define HDCDRV_SESSION_FD_INVALID (-2)
#define HDCDRV_INVALID_SQ_HEAD 17

#define HDCDRV_MODE_KERNEL 0
#define HDCDRV_MODE_USER   1

#define HDCDRV_SERVICE_HIGH_LEVEL 0
#define HDCDRV_SERVICE_LOW_LEVEL  1

#define HDCDRV_SERVICE_SHORT_CONN	0
#define HDCDRV_SERVICE_LONG_CONN	1

#define HDCDRV_SERVICE_SCOPE_GLOBAL    0
#define HDCDRV_SERVICE_SCOPE_PROCESS   1

#define HDCDRV_SERVICE_LOG_LIMIT    1
#define HDCDRV_SERVICE_NO_LOG_LIMIT 0

#define HDCDRV_DESC_QUEUE_DEPTH	       16

#define HDCDRV_RX_BUDGET 16
#define HDCDRV_TX_BUDGET 16

#define HDCDRV_SESSION_STATUS_IDLE 0
#define HDCDRV_SESSION_STATUS_CONN 1
#define HDCDRV_SESSION_STATUS_REMOTE_CLOSED 2
#define HDCDRV_SESSION_STATUS_CLOSING 3
#define HDCDRV_SESSION_STATUS_MAX 4

#define HDCDRV_MSG_TX_STATUS_FULL  1
#define HDCDRV_MSG_TX_STATUS_EMPTY 0

#define HDCDRV_MSG_CHAN_TYPE_NORMAL 0
#define HDCDRV_MSG_CHAN_TYPE_FAST   1

#define HDCDRV_MANAGE_SERVICE_CHAN_NUM 2U

#define HDCDRV_MSG_CHAN_FLAG_NOT_ALLOCED 0
#define HDCDRV_MSG_CHAN_FLAG_ALLOCED     1

#define HDCDRV_INVALID_CHAN_ID 0XFFFFFFFF

#define HDCDRV_WAKE_UP_WAIT_TIMEOUT 1000 /* 1s */

/* msg chan cnt for 3559 host */
#define HDCDRV_NORMAL_MSG_CHAN_CNT 1

#define HDCDRV_RX_DEFAULT_TIMEOUT (3 * KA_HZ)
#define HDCDRV_TX_DEFAULT_TIMEOUT (3 * KA_HZ)

#define HDCDRV_INVALID_REMOTE_SESSION_ID (-2)

#define HDCDRV_BUF_MAX_CNT 16

#define HDCDRV_SET_PID_TIMEOUT 10
#define HDCDRV_KERNEL_UNKNOW_PID (-3)

#define HDCDRV_DELAY_REMOTE_CLOSE_BIT    0
#define HDCDRV_DELAY_UNKNOWN_SESSION_BIT 1

#define HDCDRV_DEBUG_MODE_TIMEOUT  (600 * KA_HZ)
#define HDCDRV_DFX_PRINT_LIMIT_CNT 100U

#define HDCDRV_CLOSE_TYPE_NONE 0
#define HDCDRV_CLOSE_TYPE_USER 1
#define HDCDRV_CLOSE_TYPE_KERNEL 2
#define HDCDRV_CLOSE_TYPE_RELEASE 3
#define HDCDRV_CLOSE_TYPE_NOT_SET_OWNER 4
#define HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST 5
#define HDCDRV_CLOSE_TYPE_MAX 6

#define HDCDRV_RX 0
#define HDCDRV_TX 1

#define HDCDRV_RX_SCHED_WORK    1
#define HDCDRV_RX_SCHED_TASKLET 0

#define HDCDRV_MEM_DFX_LOCAL 0
#define HDCDRV_MEM_DFX_REMOTE 1
#define HDCDRV_MEM_DFX_DEFAULT_TIME 1800000 /* 30 MIN */
#define HDCDRV_MEM_DFX_ALLOC_USED 1
#define HDCDRV_MEM_DFX_FREE_USER 2
#define HDCDRV_MEM_DFX_FREE_RELEASE 3
#define HDCDRV_MEM_DFX_REMOTE_FREE 4
#define HDCDRV_INIT_INSTANCE_TIMEOUT (4 * KA_HZ)

#define HDCDRV_SHOW_PKT_LEN 128
#define HDCDRV_SHOW_PKT_RX_NUM 50

#define HDCDRV_RECYCLE_DELAY_TIME 10

#define HDCDRV_MEMPOOL_FREE_IN_PM 0
#define HDCDRV_MEMPOOL_FREE_IN_VM 1

#if defined (CFG_FEATURE_VFIO) || defined (CFG_FEATURE_VFIO_DEVICE) || defined (CFG_FEATURE_SRIOV)
#define HDCDRV_SERVER_PROCESS_MAX_NUM (32 * HDCDRV_DEV_MAX_VDEV_PER_DEVICE)
#else
#ifdef CFG_FEATURE_PROCESS_NUM_LITE
#define HDCDRV_SERVER_PROCESS_MAX_NUM 32
#else
#define HDCDRV_SERVER_PROCESS_MAX_NUM 64
#endif
#endif
#define HDCDRV_DFX_MAX_PROCESS_NUM 64  // only for dfx, max support 64 process

#define HDCDRV_SESSION_UNIQUE_VALUE_MASK 0x3FFFFFFFU
#define HDCDRV_SESSION_UNIQUE_VALUE_HOST_FLAG 0x80000000U
#define HDCDRV_SESSION_UNIQUE_VALUE_DEVICE_FLAG 0xC0000000U

#define DEVDRV_STATUS_HDC_CLOSE_FLAG  0xEEEEDDDDU

#define HDCDRV_DEVID_MASK 0x3FULL
#define HDCDRV_FID_MASK 0x1FULL
#define HDCDRV_FID_OFFSET 32
#define HDCDRV_DEVID_OFFSET 48
#define HDCDRV_VMID_MASK 0x7FULL
#define HDCDRV_VMID_OFFSET 32

#define HDCDRV_DMA_CHAN_INVALID_INSTANCE (-1)

#define HDCDRV_CHAN_DFX_DELAY_TIME (15 * 60 * 1000) /* 15min */

// The length of plog can be truncated directly. No need to worry about the length change of the log
#define HDCDRV_LOG_MAX_LEN 1024
#define HDCDRV_SYSFS_DATA_MAX_LEN 4096U

#define HDCDRV_TIME_COMPARE_2MS 2
#define HDCDRV_TIME_COMPARE_4MS 4

/*
 * linux kernel < 3.11 not defined KA_VM_FAULT_SIGSEGV,
 * euler LINUX_VERSION_CODE 3.10 defined KA_VM_FAULT_SIGSEGV
 */
#ifdef KA_VM_FAULT_SIGSEGV
#define HDCDRV_FAULT_ERROR KA_VM_FAULT_SIGSEGV
#else
#define HDCDRV_FAULT_ERROR KA_VM_FAULT_SIGBUS
#endif

/* hdc mempool level */
enum {
    HDC_MEM_POOL_LEVEL_0 = 0,
    HDC_MEM_POOL_LEVEL_1,
    HDC_MEM_POOL_LEVEL_2,
    HDC_MEM_POOL_LEVEL_3,
    HDC_MEM_POOL_LEVEL_4,
    HDC_MEM_POOL_LEVEL_INVALID
};

#define HDCDRV_SQ_RESV_LEN 2

/* dma payload is 128 bytes; cq/sq must aligned with 128, not within same cacheline */
/* sq cq description */
struct hdcdrv_sq_desc {
    int local_session;
    int remote_session;
    u64 src_data_addr;
    u64 dst_data_addr;
    u64 src_ctrl_addr;
    u64 dst_ctrl_addr;
    int data_len;
    int ctrl_len;
    u32 src_pid;
    u32 src_fid;
    u32 inner_checker;
    u32 desc_crc;
    u32 valid;
#ifdef CFG_FEATURE_PFSTAT
    u32 trans_id;
#endif
} __attribute__((aligned(128)));

#define HDCDRV_SQ_DESC_SIZE sizeof(struct hdcdrv_sq_desc)
#define HDCDRV_SQ_DESC_CRC_LEN ka_offsetof(struct hdcdrv_sq_desc, desc_crc)

struct hdcdrv_cq_desc {
    int status; /* if status > 0  is pcie err, other hdc err */
    u32 sq_head;
    int session;
    u32 desc_crc;
    u32 valid;
} __attribute__((aligned(128)));

#define HDCDRV_CQ_DESC_SIZE sizeof(struct hdcdrv_cq_desc)
#define HDCDRV_CQ_DESC_CRC_LEN ka_offsetof(struct hdcdrv_cq_desc, desc_crc)

/* ctrl msg */
struct hdcdrv_ctrl_msg_sync {
    int segment;
    int peer_dev_id;
};

struct hdcdrv_ctrl_msg_connect {
    int service_type;
    int client_session;
    u32 fast_chan_id;
    u32 normal_chan_id;
    int run_env;
    int euid;
    int uid;
    int root_privilege;
    u32 fid;
    u32 unique_val;
    u64 local_pid;
    u64 peer_pid;
};

struct hdcdrv_ctrl_msg_connect_reply {
    int client_session;
    int server_session;
    int run_env;
    u32 fid;
    u32 unique_val;
    u64 local_pid;
    u64 peer_pid;
    u32 fast_chan_id;
    u32 normal_chan_id;
};

struct hdcdrv_ctrl_msg_close {
    int local_session;
    int remote_session;
    int session_close_state;
    u32 fid;
    u32 unique_val;
};

struct hdcdrv_ctrl_msg_chan_set {
    int normal_chan_num;
};

struct hdcdrv_ctrl_msg_get_stat {
    u32 msg_len;
    u32 para;
};

struct hdcdrv_ctrl_msg {
    int type;
    int error_code;
    union {
        struct hdcdrv_ctrl_msg_sync sync_msg;
        struct hdcdrv_ctrl_msg_connect connect_msg;
        struct hdcdrv_ctrl_msg_connect_reply connect_msg_reply;
        struct hdcdrv_ctrl_msg_close close_msg;
        struct hdcdrv_ctrl_msg_chan_set chan_set_msg;
        struct hdcdrv_ctrl_msg_get_stat stat;
    };
};

struct hdcdrv_fast_page_info {
    u32 send_inner_page_offset;
    u32 page_start;
    u32 page_end;
};

struct hdcdrv_fast_mem_info {
    struct hdcdrv_fast_addr_info src_data;
    struct hdcdrv_fast_addr_info dst_data;
    struct hdcdrv_fast_addr_info src_ctrl;
    struct hdcdrv_fast_addr_info dst_ctrl;
};

struct hdcdrv_latency_info {
	u64 send_timestamp;
	u64 wait_sq_timestamp;
	u64 dma_copy_timestamp;
	u64 rx_task_timestamp;
    u64 sq_dma_timestamp;
    u64 cq_dma_timestamp;
};

struct hdcdrv_buf_desc {
    void *buf;
    u64 addr;  // src_data_addr
    int len;   // data_len
    int ctrl_len;

    u64 src_data_addr_va; // primal src_data_addr
    u64 src_ctrl_addr_va; // primal src_ctrl_addr
    u64 dst_data_addr;
    u64 src_ctrl_addr;
    u64 dst_ctrl_addr;

    int status;
    int skip_flag;
    int local_session;
    int remote_session;

    u32 inner_checker;
	struct hdcdrv_latency_info latency_info;

    struct hdcdrv_fast_mem *src_data;
    struct hdcdrv_fast_mem *dst_data;
    struct hdcdrv_fast_mem *src_ctrl;
    struct hdcdrv_fast_mem *dst_ctrl;
    ka_sg_table_t *dma_sgt;
    u32 dev_id;
    u32 fid; /* Only when the VM is not 0, the container is 0, and the physical machine is 0. */
    u32 pid;
#ifdef CFG_FEATURE_MIRROR
    int mem_id;
#endif
};

struct hdcdrv_recv_buf_desc_list {
    struct hdcdrv_buf_desc buf_d;
    struct hdcdrv_recv_buf_desc_list *next;
};

struct hdcdrv_connect_list {
    int session_fd;
    struct hdcdrv_connect_list *next;
};

struct hdcdrv_dbg_stats {
    unsigned long long hdcdrv_msg_chan_send1;
    unsigned long long hdcdrv_rx_msg_notify1;
    unsigned long long hdcdrv_rx_msg_notify_task_check1;
    unsigned long long hdcdrv_rx_msg_notify_task_check2;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_over2ms;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_over4ms;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_new;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_max;
    unsigned long long hdcdrv_rx_msg_notify_task1;
    unsigned long long hdcdrv_rx_msg_notify_task2;
    unsigned long long hdcdrv_rx_msg_notify_task3;
    unsigned long long hdcdrv_rx_msg_notify_task4;
    unsigned long long hdcdrv_rx_msg_notify_task5;
    unsigned long long hdcdrv_rx_msg_notify_task6;
    unsigned long long hdcdrv_rx_msg_notify_task7;
    unsigned long long hdcdrv_rx_msg_notify_task8;
    unsigned long long hdcdrv_rx_msg_notify_task9;
    unsigned long long hdcdrv_normal_dma_copy1;
    unsigned long long hdcdrv_rx_msg_callback1;
    unsigned long long hdcdrv_rx_msg_callback2;
    unsigned long long hdcdrv_rx_msg_callback3;
    unsigned long long hdcdrv_rx_msg_task_check1;
    unsigned long long hdcdrv_rx_msg_task_check2;
    unsigned long long hdcdrv_msg_chan_recv_task1;
    unsigned long long hdcdrv_msg_chan_recv_task2;
    unsigned long long hdcdrv_msg_chan_recv_task3;
    unsigned long long hdcdrv_msg_chan_recv_task4;
    unsigned long long hdcdrv_msg_chan_recv_task5;
    unsigned long long hdcdrv_msg_chan_recv_task6;
    unsigned long long hdcdrv_msg_chan_recv_task7;
    unsigned long long hdcdrv_msg_chan_recv_task8;
    unsigned long long hdcdrv_tx_finish_notify1;
    unsigned long long hdcdrv_tx_finish_task_check1;
    unsigned long long hdcdrv_tx_finish_task_check2;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_over2ms;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_over4ms;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_new;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_max;
    unsigned long long hdcdrv_tx_finish_notify_task1;
    unsigned long long hdcdrv_tx_finish_notify_task2;
    unsigned long long hdcdrv_tx_finish_notify_task3;
    unsigned long long hdcdrv_tx_finish_notify_task4;
    unsigned long long hdcdrv_tx_finish_notify_task5;
    unsigned long long hdcdrv_tx_finish_notify_task6;
    unsigned long long hdcdrv_tx_finish_notify_task7;
    unsigned long long hdcdrv_tx_finish_notify_task8;
    unsigned long long hdcdrv_tx_finish_notify_session_no_update;
    unsigned long long hdcdrv_tx_finish_notify_sq_head_no_update;
    unsigned long long hdcdrv_mem_avail1;
    unsigned long long hdcdrv_recv_data_times;
    unsigned long long hdcdrv_wait_mem_normal;
    unsigned long long hdcdrv_wait_mem_fifo_full;
    unsigned long long hdcdrv_msg_chan_send2;
};

struct hdcdrv_stats {
    unsigned long long tx;
    unsigned long long tx_bytes;
    unsigned long long rx;
    unsigned long long rx_bytes;
    unsigned long long tx_finish;
    unsigned long long rx_finish;
    unsigned long long tx_full;
    unsigned long long tx_fail;
    unsigned long long rx_fail;
    unsigned long long rx_full;
    unsigned long long rx_total;
    unsigned long long alloc_mem_err;
};

struct hdcdrv_service_stats {
    u64 accept_session_num;
    u64 connect_session_num_total;
    u64 connect_session_num;
    u64 close_session_num;
    u32 send_print_cnt;
    u64 send_jiffies;
    u32 send_print_cnt1;
    u64 send_jiffies1;
    u32 recv_print_cnt;
    u64 recv_jiffies;
    u32 recv_print_cnt1;
    u64 recv_jiffies1;
    u64 accept_wait_stamp;
    struct hdcdrv_link_ctrl_msg_stats connect_msg_stat;
    struct hdcdrv_link_ctrl_msg_stats reply_msg_stat;
    struct hdcdrv_link_ctrl_msg_stats close_msg_stat;
};

struct hdcdrv_service {
    int listen_status;
    u64 listen_pid;
    u32 fid;
    ka_wait_queue_head_t wq_conn_avail;
    struct hdcdrv_connect_list *conn_list_head;
    ka_list_head_t serv_list;
    struct hdcdrv_stats data_stat;
    struct hdcdrv_service_stats service_stat;
    struct hdcdrv_ctx *ctx;
    struct hdcdrv_epoll_fd *epfd;
    ka_mutex_t mutex;
};

struct hdcdrv_serv_list_node {
    struct hdcdrv_service service;
    ka_list_head_t list;
};

struct hdcdrv_msg_chan_tasklet_status {
    unsigned long long schedule_in;
    unsigned long long schedule_in_last;
    int no_schedule_cnt;
};

struct hdcdrv_msg_chan {
    int chan_id;
    u32 dev_id;
    int type;
    void *chan;
    int rx_trigger_flag;
    int submit_dma_head;
    int dma_head;
    int rx_head;
    int session_cnt;
    int sq_head;
    int dma_need_submit_flag;
    int is_allocated;
    u32 dfx_rx_stamp; /* period prinf rx statistic */
    u32 dfx_tx_stamp; /* period prinf tx statistic */
    enum devdrv_dma_data_type data_type;
    struct hdcdrv_stats stat;
    struct hdcdrv_dbg_stats dbg_stat;
    ka_tasklet_struct_t tx_finish_task;
    ka_tasklet_struct_t rx_task;
    ka_tasklet_struct_t tx_sq_task;
    ka_tasklet_struct_t tx_cq_task;
    ka_workqueue_struct_t *rx_workqueue;
    ka_work_struct_t rx_notify_work;
    ka_tasklet_struct_t rx_notify_task;
    struct hdcdrv_msg_chan_tasklet_status tx_finish_task_status;
    struct hdcdrv_msg_chan_tasklet_status rx_notify_task_status;
    struct hdcdrv_msg_chan_tasklet_status rx_task_status;
    struct hdcdrv_buf_desc tx[HDCDRV_DESC_QUEUE_DEPTH];
    struct hdcdrv_buf_desc rx[HDCDRV_DESC_QUEUE_DEPTH];
    ka_wait_queue_head_t send_wait;
    ka_list_head_t wait_mem_list;
    ka_device_t *dev;
    struct devdrv_dma_node *node;
    ka_mutex_t mutex;
    unsigned long long hdcdrv_rx_stamp;
    unsigned long long hdcdrv_tx_stamp;
};

struct hdcdrv_dev {
    u32 valid;
    u32 dev_id;
    int peer_dev_id;
    u32 normal_chan_num;
    int msg_chan_cnt;
    int alloced_chan_cnt;
    struct hdcdrv_service service[HDCDRV_SUPPORT_MAX_SERVICE];
    struct hdcdrv_msg_chan *msg_chan[HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN];
    void *ctrl_msg_chan;
    void *sync_mem_buf;
    ka_mutex_t sync_mem_mutex;
    ka_delayed_work_t init;
    ka_device_t *dev;
    ka_mutex_t mutex;
    struct hdcdrv_mem_pool huge_mem_pool[HDCDRV_MEM_POOL_TYPE_NUM];
    struct hdcdrv_mem_pool reserve_huge_mem_pool[HDCDRV_MEM_POOL_TYPE_NUM];
    struct hdcdrv_mem_pool small_mem_pool[HDCDRV_MEM_POOL_TYPE_NUM];
    ka_semaphore_t hdc_instance_sem;
    struct hdcdrv_dev_fmem fmem;
    u32 host_pm_or_vm_flag;
};

struct hdcdrv_fast_rx {
    u64 data_addr;
    u64 ctrl_addr;
    int data_len;
    int ctrl_len;
};

struct hdcdrv_session_fast_rx {
    int head;
    int tail;
    struct hdcdrv_fast_rx rx_list[HDCDRV_BUF_MAX_CNT];
};

struct hdcdrv_session_normal_rx {
    int head;
    int tail;
    struct hdcdrv_buf_desc rx_list[HDCDRV_SESSION_RX_LIST_MAX_PKT];
};

struct hdcdrv_timeout_jif {
    unsigned long long send_timeout;
    unsigned long long fast_send_timeout;
};

struct hdcdrv_session_work_node {
    ka_list_head_t list;
    void *buf;
    ka_sg_table_t *dma_sgt;
    u32 dev_id;
    u32 fid;
};

struct hdcdrv_session_work {
    ka_work_struct_t swork;
    ka_list_head_t root_list;
    ka_task_spinlock_t lock;
};

struct hdcdrv_session {
    ka_atomic_t status;
    u32 inner_checker;
    int local_session_fd;
    int remote_session_fd;
    int dev_id;
    int service_type;
    u32 chan_id;
    u32 fast_chan_id;
    u32 delay_work_flag;
    int run_env;
    u64 create_pid;
    u64 peer_create_pid;
    u64 owner_pid;
    int remote_close_state;
    int local_close_state;
    u32 unique_val;
    int euid;
    int uid;
    int root_privilege;
    u32 container_id;
    u32 local_fid;
    u32 remote_fid;
    u32 owner;
    u64 remote_close_jiff;
    struct hdcdrv_timeout_jif timeout_jiffies;
    struct hdcdrv_msg_chan *msg_chan;
    struct hdcdrv_msg_chan *fast_msg_chan;
    struct hdcdrv_service *service;
    struct hdcdrv_ctx *ctx;
    struct hdcdrv_epoll_fd *epfd;
    struct hdcdrv_stats stat;
    struct hdcdrv_dbg_stats dbg_stat;
    ka_wait_queue_head_t wq_conn;
    ka_wait_queue_head_t wq_rx;
    ka_delayed_work_t remote_close;
    ka_delayed_work_t close_unknow_session;
#ifdef CFG_FEATURE_VFIO
    struct hdcdrv_session_work session_work;
#endif
    ka_task_spinlock_t lock;
    ka_mutex_t mutex;
    struct hdcdrv_session_normal_rx normal_rx;
    struct hdcdrv_session_fast_rx fast_rx;
    ka_wait_queue_head_t wq_mem_release_event;
    struct kfifo mem_release_fifo;
    ka_task_spinlock_t mem_release_lock;
    u32 mem_release_wait_flag;
    int pid_flag;
    int remote_session_close_flag;
    u32 result_type;
    u32 session_cur_alloc_idx;
    u32 work_cancel_cnt;
    u64 connect_wait_reply_stamp;
    u64 recv_peek_stamp;
    u64 task_start_time;
};

struct hdcdrv_ctx {
    ka_rb_node_t ctx_node;
    u64 node_hash;
    int dev_id;
    int service_type;
    int session_fd;
    u64 pid;
    u32 fid;
    int refcnt;
    u64 task_start_time;
    struct hdcdrv_session *session;
    struct hdcdrv_service *service;
    struct hdcdrv_epoll_fd *epfd;
    struct hdcdrv_ctx_fmem ctx_fmem;            // record fast_mem used for normal service
    struct hdcdrv_ctx_fmem abnormal_ctx_fmem;   // record fast_mem unmap va failed when alloc abnormal
    struct hdcdrv_ctx_fmem async_ctx;           // record async release fast node
    ka_work_struct_t async_release_work;
};

struct hdcdrv_debug {
    long long valid;
    long long pid;
};

struct hdcdrv_mem_list_entry {
    u32 devid;
    u64 hash_va;
    ka_list_head_t list;
};

struct hdcdrv_mem_list {
    u32 list_len;
    struct hdcdrv_mem_list_entry ka_list_entry;
    ka_task_spinlock_t lock;
};

struct hdcdrv_service_attr {
    int level;
    int conn_feature;
    int service_scope;
    int log_limit;
};

#ifdef CFG_FEATURE_VFIO

#define HDCDRV_VDEV_MEM_POOL_TYPE_RX_SMALL 0
#define HDCDRV_VDEV_MEM_POOL_TYPE_RX_HUGE 1
#define HDCDRV_VDEV_MEM_POOL_TYPE_TX_SMALL 2
#define HDCDRV_VDEV_MEM_POOL_TYPE_TX_HUGE 3
#define HDCDRV_VDEV_MEM_POOL_TYPE_MAX 4
#define HDCDRV_VDEV_RX_MEM_POOL_TYPE_MAX 2
struct vhdch_vdev {
    ka_atomic64_t busy;
    u32 valid;
    u32 fid;
    u32 dev_id;
    u32 vm_id;
    u32 type;
    int msg_chan_cnt;
    int cur_alloc_long_session;
    int cur_alloc_short_session;
    int ctx_num;
    int fast_node_num_avaliable;
    int fnode_phy_num_avaliable;
    u32 msgchan_map[HDCDRV_SUPPORT_MAX_MSG_CHAN_PER_VDEV];
    int mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_MAX];
    int rx_wait_sche[HDCDRV_VDEV_RX_MEM_POOL_TYPE_MAX];
    ka_task_spinlock_t lock;
    ka_task_spinlock_t mem_lock;
    ka_rb_root_t rb_ctx;
    ka_rb_root_t rb_mem;
    struct hdcdrv_service service[HDCDRV_SUPPORT_MAX_SERVICE];
    struct hdcdrv_service_attr service_attr[HDCDRV_SUPPORT_MAX_SERVICE];
    ka_mutex_t mutex;
    ka_mutex_t release_mutex;
    int vm_version;
};
#endif

struct hdcdrv_ctrl {
    struct hdcdrv_cdev hdc_cdev;
    ka_delayed_work_t recycle;
    ka_delayed_work_t recycle_mem;
    int segment;
    int cur_alloc_session;
    int cur_alloc_short_session;
    ka_atomic_t unique_val;
    struct hdcdrv_service_attr service_attr[HDCDRV_SUPPORT_MAX_SERVICE];
    struct hdcdrv_dev devices[HDCDRV_SUPPORT_MAX_DEV];
    struct hdcdrv_session *sessions;
#ifdef CFG_FEATURE_VFIO
    struct vhdch_vdev vdev[VMNG_PDEV_MAX][VMNG_VDEV_MAX_PER_PDEV];
#endif
    ka_mutex_t mutex;
    struct hdcdrv_debug debug_state;
    struct hdcdrv_epoll epolls;
    struct hdcdrv_dev_fmem fmem;
    int pm_version;
    int running_statue; /* 0-mdc normal; 1-mdc suspend; 2-mdc resume */
};

struct hdcdrv_peer_st_ctrl {
    ka_rwlock_t lock;
    u32 current_state;
};

extern struct hdcdrv_ctrl *hdc_ctrl;
extern struct hdcdrv_node_tree_ctrl *hdc_node_tree;
extern int hdcdrv_get_service_level(int service_type);
extern long hdcdrv_session_alive_check(int session_fd, int dev_id, u32 unique_val);

long hdcdrv_fast_alloc_mem(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_alloc_mem *cmd);
long hdccom_fast_alloc_mem(void *ctx, struct hdcdrv_cmd_alloc_mem *cmd, struct hdcdrv_fast_node **f_node_ret);
long hdcdrv_fast_free_mem(const void *ctx, struct hdcdrv_cmd_free_mem *cmd);
long hdcdrv_kernel_get_session_local_create_pid(int session, int *value);
long hdcdrv_kernel_get_session_run_env(int session, const char *session_id, int *run_env);
long hdcdrv_kernel_recv_peek_timeout(int session, const char *session_id, int *len, int timeout);
long hdcdrv_kernel_send(int session, const char *session_id, void *buf, int len);
long hdcdrv_kernel_set_service_level(int service_type, int level);
void hdcdrv_set_debug_mode(int flag);
bool hdcdrv_is_service_init(unsigned int service_type);

long hdcdrv_fast_register_mem(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_register_mem *cmd);

extern int hdcdrv_init_session_release_mem_event(struct hdcdrv_session *session);
extern void hdcdrv_free_session_release_mem_event(struct hdcdrv_session *session);
extern void hdcdrv_update_session_release_mem_event(struct hdcdrv_session *session,
                                                    const struct hdcdrv_wait_mem_fin_msg *in_msg);
extern int hdcdrv_wait_session_release_mem_event(struct hdcdrv_session *session,
    int time_out, unsigned int result_type, struct hdcdrv_wait_mem_fin_msg *out_msg);
extern void hdcdrv_destroy_session_del_mem_release_event(struct hdcdrv_session *session);
extern void hdcdrv_peer_fault_del_mem_release_event(struct hdcdrv_session *session);
extern long hdcdrv_fast_wait_mem(struct hdcdrv_cmd_wait_mem *cmd);

extern long hdcdrv_dev_para_check(int dev_id, int service_type);
struct hdcdrv_fast_node *hdcdrv_fast_node_search_timeout(ka_task_spinlock_t *lock, ka_rb_root_t *root,
    u64 hash_va, int timeout);
extern int hdcdrv_fast_node_insert_new_tree(int devid,
    u64 pid, u32 fid, u32 rb_side, struct hdcdrv_fast_node *new_node);
int hdcdrv_fast_node_insert(ka_task_spinlock_t *lock,
    ka_rb_root_t *root, struct hdcdrv_fast_node *fast_node, u64 search_type);
void hdcdrv_fast_node_erase(ka_task_spinlock_t *lock, ka_rb_root_t *root, struct hdcdrv_fast_node *fast_node);
extern void hdcdrv_set_device_status(int devid, u32 valid);
extern u32 hdcdrv_get_device_status(int devid);
extern void hdcdrv_set_device_para(u32 devid, int normal_chan_num);
extern long hdcdrv_session_alloc_idx_check(int session_fd, u32 alloc_idx);
extern long hdcdrv_session_para_check(int session_fd, int device_id);
extern long hdcdrv_session_valid_check(int session_fd, int device_id, u64 check_pid);
extern long hdcdrv_session_inner_check(int session_fd, u32 checker);
extern int hdcdrv_set_msg_chan_priv(void *msg_chan, void *priv);
extern struct hdcdrv_msg_chan *hdcdrv_get_msg_chan_priv(void *msg_chan);
extern struct hdcdrv_sq_desc *hdcdrv_get_w_sq_desc(void *msg_chan, u32 *tail);
extern void hdcdrv_set_w_sq_desc_head(void *msg_chan, u32 head);
extern int hdcdrv_copy_sq_desc_to_remote(struct hdcdrv_msg_chan *msg_dev, const struct hdcdrv_sq_desc *sq_desc,
                                          enum devdrv_dma_data_type data_type);
extern bool hdcdrv_w_sq_full_check(void *msg_chan);
extern struct hdcdrv_sq_desc *hdcdrv_get_r_sq_desc(void *msg_chan, u32 *head);
extern void hdcdrv_move_r_sq_desc(void *msg_chan);
extern enum devdrv_dma_direction hdcdrv_get_dma_direction(void);
extern struct hdcdrv_cq_desc *hdcdrv_get_w_cq_desc(void *msg_chan);
extern void hdcdrv_copy_cq_desc_to_remote(struct hdcdrv_msg_chan *msg_dev, const struct hdcdrv_cq_desc *cq_desc,
                                          enum devdrv_dma_data_type data_type);
extern struct hdcdrv_cq_desc *hdcdrv_get_r_cq_desc(void *msg_chan);
extern void hdcdrv_move_r_cq_desc(void *msg_chan);

extern void hdcdrv_rx_msg_notify(void *chan);
extern void hdcdrv_tx_finish_notify(void *chan);

extern long hdcdrv_ctrl_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
extern int hdcdrv_ctrl_msg_recv(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
extern void hdcdrv_notify_msg_recv(u32 devid, void *data, u32 in_data_len);

extern struct hdcdrv_dev *hdcdrv_add_dev(ka_device_t *dev, u32 dev_id);
extern int hdcdrv_add_msg_chan_to_dev(u32 dev_id, void *chan);

extern int hdcdrv_add_ctrl_msg_chan_to_dev(u32 dev_id, void *chan);
extern void hdcdrv_free_msg_chan(struct hdcdrv_dev *hdc_dev);
extern void hdcdrv_set_segment(int segment);
extern void hdcdrv_set_peer_dev_id(int dev_id, int peer_dev_id);
extern int hdcdrv_get_peer_dev_id(int dev_id);

extern long hdcdrv_epoll_operation(struct hdcdrv_ctx *ctx, u32 drv_cmd, union hdcdrv_cmd *cmd_data,
    bool *copy_to_user_flag, u32 fid);

extern int hdcdrv_init(void);
extern void hdcdrv_uninit(void);
extern struct hdcdrv_dev *hdcdrv_get_dev(u32 dev_id);
extern void hdcdrv_reset_dev(struct hdcdrv_dev *hdc_dev);
extern void hdcdrv_stop_work(struct hdcdrv_dev *hdc_dev);
extern void hdcdrv_remove_dev(struct hdcdrv_dev *hdc_dev);
extern void hdcdrv_del_dev(u32 dev_id);
extern void hdcdrv_free_dev_mem(u32 dev_id);
extern int hdcdrv_get_session_run_env(u32 dev_id, u32 fid);
extern long hdcdrv_close(const struct hdcdrv_cmd_close *cmd, int close_state);
extern int hdcdrv_set_mem_info(int devid, u32 fid, u32 rb_side, struct hdcdrv_ctrl_msg_sync_mem_info *msg);
extern int hdcdrv_mem_adapter(const struct hdcdrv_fast_addr_info *src_info,
    const struct hdcdrv_fast_addr_info *dst_info, struct devdrv_dma_node *node, int *node_idx, int len);
extern int hdcdrv_check_session_owner(const struct hdcdrv_session *session, u64 check_pid);
extern int hdcdrv_session_task_start_time_compare(const u64 proc_start_time, const u64 session_start_time);

bool hdcdrv_is_vf_device(u32 dev_id, u32 fid);
#ifndef HDCDRV_ENV_DEVICE
extern int hdcdrv_container_vir_to_phs_devid(u32 virtual_devid, u32 *physical_devid, u32 *vfid);
extern int hdcdrv_check_in_container(void);
#endif
extern u32 hdcdrv_get_container_id(void);
extern int hdcdrv_check_hostpid(int hostpid, u32 devid, u32 vfid);
extern u32 hdcdrv_get_fid(u64 pid);
extern int hdcdrv_get_localpid(u32 hostpid, u32 chip_id, int cp_type, u32 vfid, int *pid);
extern u64 hdcdrv_get_peer_pid(u32 devid, u64 host_pid, u32 fid, u64 peer_pid, int service_type);
extern u32 hdcdrv_get_vmid_from_pid(u64        pid);
extern int hdcdrv_service_scope_init(int service_type);
extern u32 hdcdrv_alloc_fast_msg_chan(int dev_id, int service_type, u32 chan_start, u32 chan_end);
extern void hdcdrv_alloc_msg_chan(int dev_id, int service_type, u32 *normal_chan_id, u32 *fast_chan_id);
extern void hdcdrv_alloc_session_chan(int dev_id, int fid, int service_type, u32 *normal_chan_id, u32 *fast_chan_id);
extern u64 hdcdrv_rebuild_pid(u32 devid, u32 fid, u64 pid);
static inline void hdcdrv_set_session_status(struct hdcdrv_session *session, int status)
{
    ka_base_atomic_set(&session->status, status);
}

static inline int hdcdrv_get_session_status(const struct hdcdrv_session *session)
{
    return ka_base_atomic_read(&session->status);
}
long hdcdrv_kernel_server_create(int dev_id, int service_type);
long hdcdrv_kernel_server_destroy(int dev_id, int service_type);
long hdcdrv_kernel_accept(int dev_id, int service_type, int *session, const char *session_id);
long hdcdrv_kernel_connect(int dev_id, int service_type, int *session, const char *session_id);
long hdcdrv_kernel_close(int session, const char *session_id);
long hdcdrv_kernel_send_timeout(int session, const char *session_id, void *buf, int len, int timeout);
int hdcdrv_get_segment(void);
long hdcdrv_ioctl_com(struct hdcdrv_ctx *ctx, unsigned int cmd, union hdcdrv_cmd *cmd_data, bool *copy_flag, u32 fid);
void hdcdrv_free_mem(struct hdcdrv_session *session, void *buf, int flag, struct hdcdrv_buf_desc *desc);
struct hdcdrv_ctx *hdcdrv_alloc_ctx(void);
void hdcdrv_free_ctx(const struct hdcdrv_ctx *ctx);
void hdcdrv_release_by_ctx(struct hdcdrv_ctx *ctx);
int hdcdrv_get_service_conn_feature(int service_type);

#ifdef CFG_FEATURE_VFIO
int vhdch_alloc_mem_vm(struct hdccom_alloc_mem_para *para, struct hdcdrv_buf_desc *desc);
void vhdch_free_mem_vm(u32 dev_id, u32 fid, void *buf);
int vhdch_alloc_mem_container(struct hdccom_alloc_mem_para *para, struct hdcdrv_buf_desc *desc);
void vhdch_free_mem_container(u32 dev_id, u32 fid, u32 chan_id, void *buf);
int vhdch_session_pre_alloc(u32 dev_id, u32 fid, int service_type);
void vhdch_session_free(u32 dev_id, u32 fid, int service_type);
u32 vdhch_alloc_normal_msg_chan(u32 dev_id, u32 fid, int service_type);
u32 vdhch_alloc_fast_msg_chan(u32 dev_id, u32 fid, int service_type);
void hdcdrv_session_work_free(struct hdcdrv_session_work *s_work);
int hdcdrv_dma_map_guest_page(u32 dev_id, u32 fid, unsigned long in_addr,
    unsigned long size, struct hdcdrv_buf_desc *desc);
void hdcdrv_dma_unmap_guest_page(u32 dev_id, u32 fid, ka_sg_table_t *dma_sgt);
struct hdcdrv_service *vhdch_alloc_service(u32 devid, u32 fid, int service_type, u64 host_pid);
struct hdcdrv_service *vhdch_search_service(u32 devid, u32 fid, int service_type, u64 host_pid);
#endif

void hdcdrv_iova_fmem_unmap(u32 dev_id, u32 fid, struct hdcdrv_fast_mem* f_mem, u32 num);
int hdcdrv_iova_fmem_map(u32 dev_id, u32 fid, struct hdcdrv_fast_mem* f_mem);
void hdcdrv_kvfree(const void *addr, int level);
struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_sep(int devid);
struct hdcdrv_service *hdcdrv_search_service(u32 dev_id, u32 fid, int service_type, u64 host_pid);
int hdcdrv_service_level_init(int service_type);
int hdcdrv_service_conn_feature_init(int service_type);
int hdcdrv_service_scope_init(int service_type);
int hdcdrv_service_log_limit_init(int service_type);
void hdcdrv_service_init(struct hdcdrv_service *service);
long hdcdrv_server_free(struct hdcdrv_service *service, int dev_id, int service_type);
void hdcdrv_service_res_uninit(struct hdcdrv_service *service, int server_type);
typedef int (*wake_up_context_status)(ka_pid_t pid, u32 devid, u32 status);
typedef int (*report_process_status)(ka_pid_t pid, u32 status);
typedef int (*bandwidth_limit_check)(struct vmng_bandwidth_check_info *info);
int hdcdrv_report_process_status(ka_pid_t pid, u32 status);

void hdcdrv_get_mempool_size(u32 *small_packet_num, u32 *huge_packet_num);
u32 hdcdrv_get_hdc_mempool_level(void);
int hdcdrv_resume(u32 dev_id, ka_device_t *dev);
int hdcdrv_suspend(u32 dev_id);
int hdcdrv_uninit_instance(u32 dev_id);
int hdcdrv_init_instance(u32 dev_id, ka_device_t *dev);
extern void hdcdrv_set_running_status(int statue);

#ifndef HDC_ENV_DEVICE
extern int hdcdrv_get_msgchan_refcnt(u32 dev_id);
extern int hdcdrv_put_msgchan_refcnt(u32 dev_id);
#endif

int devdrv_wakeup_cce_context_status(ka_pid_t pid, u32 devid, u32 status);
int vmng_bandwidth_limit_check(struct vmng_bandwidth_check_info *info);

void hdcdrv_rx_msg_schedule_task(struct hdcdrv_msg_chan *msg_chan);
int hdcdrv_get_link_status(struct devdrv_pcie_link_info_para *link_info);
int hdcdrv_force_link_down(void);

/*
 * hdcdrv_session_free_check : Check whether all sessions are released
 *
 * During PCIe suspend, this function is called to check whether the HDC
 * driver meets the conditions for entering the suspend state.
 */
int hdcdrv_session_free_check(int show_log);
/* When peer reboot/panic, local pcie can detect the peer fault and should notify HDC
   to stop bussiness and so on */
int hdcdrv_peer_fault_notify(u32 status);

#endif  // _DEVDRV_MAIN_H_

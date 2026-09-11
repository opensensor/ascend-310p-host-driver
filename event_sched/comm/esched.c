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
#include "ka_common_pub.h"
#include "ka_barrier_pub.h"
#include "ka_system_pub.h"
#include "ka_base_pub.h"
#include "ka_compiler_pub.h"

#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
#include <linux/aos/cpu_domain_info.h>
#include "drv_cpu_type.h"
#endif

#include "securec.h"
#include "kernel_version_adapt.h"
#include "dbl/feature_loader.h"

#ifdef CFG_FEATURE_HARDWARE_SCHED
#include "esched_drv_adapt.h"
#include "esched_drv.h"
#include "esched_vf.h"
#endif

#ifdef CFG_FEATURE_PM
#include "esched_pm.h"
#endif

#include "esched_fops.h"
#include "esched_sysfs.h"
#include "esched.h"

/*
* Due to engineering problems, there will be false alarm,
* here to shield until the problem is solved.
*/
/*lint -e144 -e666 -e102 -e1112 -e145 -e151 -e437 -e679 -e30 -e514 -e65 -e446*/
#ifdef EVENT_SCHED_UT
#  define STATIC_INLINE
#else
#  define STATIC_INLINE static inline
#endif

#ifndef __KA_GFP_HIGHMEM
#define ESCHED_GFP_HIGHMEM 0
#else
#define ESCHED_GFP_HIGHMEM __KA_GFP_HIGHMEM
#endif

STATIC KA_TASK_DEFINE_MUTEX(sched_dev_mutex);
STATIC ka_rwlock_t sched_dev_rwlock;

struct sched_numa_node *sched_node[SCHED_MAX_CHIP_NUM];
bool sched_node_is_valid[SCHED_MAX_CHIP_NUM];

#define LOG_TIME_INTERVAL (10)
bool esched_log_limited(u32 type)
{
#ifndef EMU_ST
    static ka_timeval_t last_stamp[SCHED_LOG_LIMIT_MAX_NUM] = {{0}};
    ka_timeval_t cur_stamp;

    ka_system_do_gettimeofday(&cur_stamp);

    if (type >= SCHED_LOG_LIMIT_MAX_NUM) {
        sched_err("Log type is invalid.\n");
        return true;
    }
    if ((ka_system_get_timeval_sec(&cur_stamp) -
        ka_system_get_timeval_sec(&last_stamp[type])) > LOG_TIME_INTERVAL) {
        last_stamp[type] = cur_stamp;
        return false;
    }
    return true;
#endif
}

#if !defined (CFG_SOC_PLATFORM_MDC_V51) && !defined (CFG_SOC_PLATFORM_MDC_V11)
STATIC int32_t esched_get_trace_index(struct sched_event *event,
    u32 *gid, u32 *event_id, u32 *subevent_id)
{
    struct event_sync_msg *sync_msg = NULL;

    if (event->event_id == EVENT_DRV_MSG) {
        if (event->msg_len < sizeof(struct event_sync_msg)) {
            return DRV_ERROR_PARA_ERROR;
        }
        sync_msg = (struct event_sync_msg *)event->msg;
        *gid = sync_msg->gid;
        *event_id = sync_msg->event_id;
        *subevent_id = sync_msg->subevent_id;
    } else {
        *gid = event->gid;
        *event_id = event->event_id;
        *subevent_id = event->subevent_id;
    }
    return 0;
}

void esched_wait_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event)
{
    int32_t ret;
    int dfx_gid, dfx_tid;
    u32 gid, event_id, subevent_id;

    if ((proc_ctx== NULL) || (event == NULL)) {
        sched_warn("wait event abnormal.\n");
        return;
    }
    ret = esched_get_trace_index(event, &gid, &event_id, &subevent_id);
    if (ret != 0) {
        return;
    }

    dfx_gid = gid - SCHED_MAX_DEFAULT_GRP_NUM;
    dfx_tid = event_id - SCHED_SYNC_START_EVENT_ID;
    if ((dfx_gid < 0) || (dfx_gid >= SCHED_MAX_EX_GRP_NUM) ||
        (dfx_tid < 0) || (dfx_tid >= SCHED_MAX_SYNC_THREAD_NUM_PER_GRP)) {
        return;
    }

#ifdef ESCHED_HOST
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_wait_start_timestamp = event->timestamp.subscribe_in_kernel;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_wait_end_timestamp = event->timestamp.subscribe_out_kernel;
#else
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].dst_wait_start_timestamp = event->timestamp.subscribe_in_kernel;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].dst_wait_end_timestamp = event->timestamp.subscribe_out_kernel;
#endif
    return;
}


void esched_publish_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event)
{
    int32_t ret;
    int dfx_gid, dfx_tid;
    u32 gid, event_id, subevent_id;

    if ((proc_ctx== NULL) || (event == NULL)) {
        sched_warn("wait event abnormal.\n");
        return;
    }
    ret = esched_get_trace_index(event, &gid, &event_id, &subevent_id);
    if (ret != 0) {
        return;
    }

    dfx_gid = gid - SCHED_MAX_DEFAULT_GRP_NUM;
    dfx_tid = event_id - SCHED_SYNC_START_EVENT_ID;
    if ((dfx_gid < 0) || (dfx_gid >= SCHED_MAX_EX_GRP_NUM) ||
        (dfx_tid < 0) || (dfx_tid >= SCHED_MAX_SYNC_THREAD_NUM_PER_GRP)) {
        return;
    }
#ifdef ESCHED_HOST
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_publish_timestamp = sched_get_cur_timestamp();
#else
    (void)memset_s(&proc_ctx->sched_dfx[dfx_gid][dfx_tid],
        sizeof(struct sched_sync_event_trace), 0, sizeof(struct sched_sync_event_trace));
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].dst_publish_timestamp = sched_get_cur_timestamp();
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].gid = gid;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].event_id = event_id;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].subevent_id = subevent_id;
#endif
    return;
}

void esched_submit_trace_update(u32 event_src,
    struct sched_numa_node *node, struct sched_published_event_info *event_info)
{
    int dfx_gid, dfx_tid;
    u32 gid, event_id, subevent_id;
    int pid = SCHED_CURRENT_TGID;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct event_sync_msg *sync_msg = NULL;

    if (event_info->event_id == EVENT_DRV_MSG) {
        if (event_info->msg_len < sizeof(struct event_sync_msg)) {
            return;
        }
        sync_msg = (struct event_sync_msg *)event_info->msg;
        gid = sync_msg->gid;
        event_id = sync_msg->event_id;
        subevent_id = sync_msg->subevent_id;
    } else {
        gid = event_info->gid;
        event_id = event_info->event_id;
        subevent_id = event_info->subevent_id;
    }

    dfx_gid = gid - SCHED_MAX_DEFAULT_GRP_NUM;
    dfx_tid = event_id - SCHED_SYNC_START_EVENT_ID;
    if ((dfx_gid < 0) || (dfx_gid >= SCHED_MAX_EX_GRP_NUM) ||
        (dfx_tid < 0) || (dfx_tid >= SCHED_MAX_SYNC_THREAD_NUM_PER_GRP)) {
        return;
    }

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        return;
    }

#ifdef ESCHED_HOST
    (void)memset_s(&proc_ctx->sched_dfx[dfx_gid][dfx_tid],
        sizeof(struct sched_sync_event_trace), 0, sizeof(struct sched_sync_event_trace));
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].gid = gid;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].event_id = event_id;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].subevent_id = subevent_id;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_submit_user_timestamp = event_info->publish_timestamp;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_submit_kernel_timestamp = sched_get_cur_timestamp();
#else
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].dst_submit_user_timestamp = event_info->publish_timestamp;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].dst_submit_kernel_timestamp = sched_get_cur_timestamp();
#endif
    esched_proc_put(proc_ctx);

    return;
}
#else
void esched_wait_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event)
{
    return;
}
void esched_publish_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event)
{
    return;
}

void esched_submit_trace_update(u32 event_src,
    struct sched_numa_node *node, struct sched_published_event_info *event_info)
{
    return;
}
#endif
void sched_publish_state_update(struct sched_numa_node *node, struct sched_event *event, int32_t ret)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 cpuid;

    sched_get_cpuid_in_node(node, &cpuid);
    cpu_ctx = sched_get_cpu_ctx(node, cpuid);
    if ((cpu_ctx == NULL) || (event->event_id >= (u32)SCHED_MAX_EVENT_TYPE_NUM)) {
        return;
    }

    if (ret != DRV_ERROR_NONE) {
        cpu_ctx->perf_stat.total_publish_fail_event_num++;
        cpu_ctx->perf_stat.publish_fail_event_num[event->event_id]++;
    }
    cpu_ctx->perf_stat.total_publish_event_num++;
    cpu_ctx->perf_stat.publish_event_num[event->event_id]++;

    return;
}

u32 esched_get_dev_num(void)
{
    struct sched_numa_node *node = NULL;
    u32 i, dev_num = 0;

    for (i = 0; i < SCHED_MAX_CHIP_NUM; i++) {
        node = sched_get_numa_node(i);
        if (node != NULL) {
            dev_num++;
        }
    }

    return dev_num;
}

/* esched_dev_get must be called before. */
struct sched_numa_node *sched_get_numa_node(u32 chip_id)
{
    if (chip_id >= SCHED_MAX_CHIP_NUM) {
        return NULL;
    }

    return sched_node[chip_id];
}

u32 sched_get_previous_cpu_num(u32 chip_id)
{
    u32 cpu_num = 0;
    u32 i;

    for (i = 0; i < chip_id; i++) {
        cpu_num += ka_nr_cpus_node(i);
    }
    return cpu_num;
}

void sched_get_cpuid_in_node(struct sched_numa_node *node, u32 *cpuid)
{
    u32 cur_processor_id = sched_get_cur_processor_id();
    if ((cur_processor_id >= node->cpu_num) || (sched_get_cpu_ctx(node, cur_processor_id) == NULL)) {
        cur_processor_id = 0; /* Returns the cpuid that can find the cpu_ctx for subsequent statistics. */
    }

    *cpuid = cur_processor_id;
}

int32_t sched_get_sched_cpuid_in_node(struct sched_numa_node *node, u32 *cpuid)
{
    u32 processor_id = sched_get_cur_processor_id();
    if ((processor_id >= node->cpu_num) || (processor_id == 0) || (sched_get_cpu_ctx(node, processor_id) == NULL)) {
        return DRV_ERROR_PARA_ERROR;
    }

    *cpuid = processor_id;

    return 0;
}

struct sched_thread_ctx *sched_get_cur_thread_in_grp(struct sched_grp_ctx *grp_ctx)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 i;

    for (i = 0; i < grp_ctx->thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->thread[i]);
        if (thread_ctx->kernel_tid == (u32)SCHED_CURRENT_PID) {
            return thread_ctx;
        }
    }

    return NULL;
}

static inline struct sched_thread_ctx *sched_get_cur_thread_in_proc(struct sched_proc_ctx *proc_ctx)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    int32_t i;

    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, (uint32_t)i);
        if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
            continue;
        }

        thread_ctx = sched_get_cur_thread_in_grp(grp_ctx);
        if (thread_ctx != NULL) {
            return thread_ctx;
        }
    }

    return NULL;
}

STATIC_INLINE void *sched_ka_vmalloc(unsigned long size, ka_gfp_t gfp_mask, ka_pgprot_t prot)
{
#ifdef AOS_LLVM_BUILD
    return sched_vmalloc(size, gfp_mask, prot);
#else
    return sched_vmalloc(size, gfp_mask, prot);
#endif
}

#if !defined (AOS_LLVM_BUILD) && !defined (EVENT_SCHED_UT) && !defined (EMU_ST)
ka_mnt_namespace_t *sched_get_proc_mnt_ns(u32 chip_id, int pid)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    ka_mnt_namespace_t *ns = NULL;

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to get proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return NULL;
    }

    ns = proc_ctx->mnt_ns;
    esched_chip_proc_put(proc_ctx);

    return ns;
}
#endif

/* return enque event num */
STATIC_INLINE int32_t sched_event_enque(struct sched_event_que *que, struct sched_event *event)
{
    if (sched_is_que_full(que)) {
        que->stat.enque_full++;
        return 0;
    }

    que->ring[que->tail & que->mask] = event;
    ka_wmb();
    que->tail++;

    return 1;
}

#ifndef CFG_SOC_PLATFORM_MDC_V51
STATIC void sched_free_thread_map(ka_kref_t *kref)
{
    struct sched_event_thread_map *thread_map = ka_container_of(kref, struct sched_event_thread_map, ref);

    sched_kfree(thread_map);
}
#endif

STATIC void sched_get_thread_map(struct sched_event_thread_map *thread_map)
{
#ifndef CFG_SOC_PLATFORM_MDC_V51
    ka_base_kref_get(&thread_map->ref);
#endif
}

STATIC void sched_put_thread_map(struct sched_event_thread_map *thread_map)
{
#ifndef CFG_SOC_PLATFORM_MDC_V51
    ka_base_kref_put(&thread_map->ref, sched_free_thread_map);
#endif
}

STATIC void sched_clear_event(struct sched_event *event)
{
    if (event->event_thread_map != NULL) {
        sched_put_thread_map(event->event_thread_map);
        event->event_thread_map = NULL;
    }
}

int32_t sched_event_enque_lock(struct sched_event_que *que, struct sched_event *event)
{
    int32_t num;

    sched_clear_event(event);

    ka_task_spin_lock_bh(&que->lock);
    num = sched_event_enque(que, event);
    ka_task_spin_unlock_bh(&que->lock);

    return num;
}

STATIC_INLINE struct sched_event *sched_event_deque(struct sched_event_que *que)
{
    struct sched_event *event = NULL;
    u32 use;

    if (sched_is_que_empty(que)) {
        que->stat.deque_empty++;
        return NULL;
    }

    ka_rmb();
    event = que->ring[que->head & que->mask];
    que->head++;

    use = que->depth - sched_que_element_num(que);
    if (use > que->stat.max_use) {
        que->stat.max_use = use;
    }

    return event;
}

STATIC_INLINE struct sched_event *sched_event_deque_lock(struct sched_event_que *que)
{
    struct sched_event *event = NULL;

    ka_task_spin_lock_bh(&que->lock);
    event = sched_event_deque(que);
    ka_task_spin_unlock_bh(&que->lock);

    return event;
}

STATIC int32_t sched_event_que_init(struct sched_event_que *que, u32 depth)
{
    que->ring = (struct sched_event **)sched_kzalloc(sizeof(struct sched_event *) * depth,
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT | KA_GFP_ATOMIC);
    if (que->ring == NULL) {
        sched_err("Failed to kzalloc memory for que->ring. (size=0x%lx)\n", sizeof(struct sched_event) * depth);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    que->depth = depth;
    que->mask = depth - 1;
    que->head = 0;
    que->tail = 0;

    ka_task_spin_lock_init(&que->lock);

    return 0;
}

STATIC void sched_event_que_uninit(struct sched_event_que *que)
{
    if (que->ring != NULL) {
        sched_kfree(que->ring);
        que->ring = NULL;
    }
}

STATIC_INLINE int32_t sched_event_add_tail(struct sched_event_list *event_list, struct sched_event *event)
{
    int32_t num = 0;

    if (event_list->cur_num >= event_list->depth) {
        return num;
    }

    ka_task_spin_lock_bh(&event_list->lock);
    if (event_list->cur_num < event_list->depth) {
#ifdef CFG_FEATURE_VFIO
        event_list->slice_cur_event_num[event->vfid]++;
#endif
        event_list->cur_num++;
        event_list->total_num++;
        ka_list_add_tail(&event->list, &event_list->head);
        event->timestamp.publish_add_event_list = sched_get_cur_timestamp();
        num = 1;
    }
    ka_task_spin_unlock_bh(&event_list->lock);

    return num;
}

STATIC_INLINE bool sched_is_grp_event_timeout(struct sched_event_timeout_info *event_timeout_info)
{
    u64 interval = 0;
    u64 cur_time;

    if (event_timeout_info->timeout_flag == SCHED_VALID) {
        cur_time = sched_get_cur_timestamp();
        if (cur_time > event_timeout_info->timeout_timestamp) {
            interval = cur_time - event_timeout_info->timeout_timestamp;
        }
        if (tick_to_millisecond(interval) >= SCHED_GROUP_DROP_EVENT_TIMEOUT) {
            return true;
        }
    }

    return false;
}

int sched_event_add_thread(struct sched_event *event, u32 tid)
{
    event->event_thread_map = sched_kzalloc(sizeof(struct sched_event_thread_map) + sizeof(u32),
        KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (event->event_thread_map == NULL) {
        sched_err("Failed to kzalloc memory for event_thread_map and thread. (size=0x%lx)\n",
                  sizeof(struct sched_event_thread_map) + sizeof(u32));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

#ifndef CFG_SOC_PLATFORM_MDC_V51
    ka_base_kref_init(&event->event_thread_map->ref);
#endif
    event->event_thread_map->thread = (u32 *)(event->event_thread_map + 1);

    event->event_thread_map->thread[0] = tid;
    event->event_thread_map->thread_num = 1;

    return 0;
}

STATIC struct sched_event_thread_map *sched_get_event_thread_map(struct sched_grp_ctx *grp_ctx,
                                                                 struct sched_event *event)
{
    return (event->event_thread_map != NULL) ? event->event_thread_map : &grp_ctx->event_thread_map[event->event_id];
}

STATIC_INLINE bool sched_is_cpu_handle_event(struct sched_grp_ctx *grp_ctx, struct sched_event *event,
    struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_event_timeout_info *event_timeout_info = &grp_ctx->event_timeout_info[event->event_id];
    struct sched_event_thread_map *event_thread_map = NULL;
    struct sched_proc_ctx *proc_ctx = grp_ctx->proc_ctx;
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 i;

    event_thread_map = sched_get_event_thread_map(grp_ctx, event);

    for (i = 0; i < event_thread_map->thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, event_thread_map->thread[i]);
        if (thread_ctx->bind_cpuid == cpu_ctx->cpuid) {
            /* group timeout or the proc has quit, return true to drop the event */
            if (sched_is_grp_event_timeout(event_timeout_info) || (proc_ctx->status == SCHED_INVALID)) {
                return true;
            }

            if (thread_ctx->timeout_flag == SCHED_VALID) {
                cpu_ctx->cannot_handle_event_reason = SCHED_CANNOT_HANDLE_THREAD_TIMEOUT;
                return false;
            }

            if (thread_ctx->wait_flag == SCHED_INVALID) {
                cpu_ctx->cannot_handle_event_reason = SCHED_CANNOT_HANDLE_NOT_WAITING;
                return false;
            }

#ifdef CFG_FEATURE_VFIO
            return sched_use_vf_cpu_handle_event(proc_ctx->node, (u32)proc_ctx->vfid, cpu_ctx);
#else
            return true;
#endif
        }
    }

    cpu_ctx->cannot_handle_event_reason = SCHED_CANNOT_HANDLE_NO_SUBSCRIBE_THREAD;
    return false;
}

STATIC_INLINE bool sched_is_event_can_handle(struct sched_event *event, struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    int32_t run_status;
    u32 cur_cpuid = cpu_ctx->cpuid;

    cpu_ctx->cannot_handle_event_reason = SCHED_HANDLE_EVENT_NORMAL;

    proc_ctx = esched_proc_get(cpu_ctx->node, event->pid);
    if ((proc_ctx == NULL) || (proc_ctx->start_timestamp > event->timestamp.publish_in_kernel)) {
        /* The process that handles the event has exited. Return to the external system to release the event */
        if (proc_ctx != NULL) {
            esched_proc_put(proc_ctx);
        }
        return true;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, event->gid);
    if (grp_ctx->is_exclusive == SCHED_VALID) {
        run_status = ka_base_atomic_cmpxchg(&grp_ctx->run_status, SCHED_GRP_EXCLUSIVE_STATUS_IDLE,
                                    SCHED_GRP_EXCLUSIVE_STATUS_RUN);
        if (run_status == SCHED_GRP_EXCLUSIVE_STATUS_RUN) {
            ka_base_atomic_or(0x1U << cur_cpuid, &grp_ctx->wait_cpu_mask);
            cpu_ctx->cannot_handle_event_reason = SCHED_CANNOT_HANDLE_GRP_EXCLUSIVE;
            esched_proc_put(proc_ctx);
            return false;
        }

        ka_base_atomic_and(~(0x1U << cur_cpuid), &grp_ctx->wait_cpu_mask);
        grp_ctx->cur_tid = grp_ctx->cpuid_to_tid[cur_cpuid];
    }

    if (sched_is_cpu_handle_event(grp_ctx, event, cpu_ctx) == false) {
        if (grp_ctx->is_exclusive == SCHED_VALID) {
            ka_base_atomic_set(&grp_ctx->run_status, SCHED_GRP_EXCLUSIVE_STATUS_IDLE);
        }
        esched_proc_put(proc_ctx);
        return false;
    }

    esched_proc_put(proc_ctx);

    return true;
}

STATIC_INLINE struct sched_event *sched_event_search(
    struct sched_event_list *event_list, struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_event *event = NULL, *tmp = NULL;
    int32_t search_cnt = 0;

    ka_task_spin_lock_bh(&event_list->lock);
    ka_list_for_each_entry_safe(event, tmp, &event_list->head, list) {
        if (sched_is_event_can_handle(event, cpu_ctx)) {
            ka_list_del(&event->list);
            event_list->cur_num--;
            event_list->sched_num++;
#ifdef CFG_FEATURE_VFIO
            event_list->slice_cur_event_num[event->vfid]--;
#endif
            ka_task_spin_unlock_bh(&event_list->lock);
            return event;
        }

        /* The search times are not increased only if there are no compute resources */
        if (cpu_ctx->cannot_handle_event_reason == SCHED_CANNOT_HANDLE_NO_AVAIABLE_RESOURCE) {
            continue;
        }

        search_cnt++;
        if (search_cnt >= SCHED_SEARCH_MAX_CNT) {
            break;
        }
    }

    ka_task_spin_unlock_bh(&event_list->lock);

    return NULL;
}

STATIC_INLINE struct sched_event *sched_get_specified_event(struct sched_event_list *event_list, int32_t pid,
    u32 event_id, u32 gid)
{
    struct sched_event *event = NULL, *tmp = NULL;

    ka_task_spin_lock_bh(&event_list->lock);
    ka_list_for_each_entry_safe(event, tmp, &event_list->head, list) {
        if ((event->event_id == event_id) && (event->pid == pid) && (event->gid == gid)) {
            ka_list_del(&event->list);
            event_list->cur_num--;
            event_list->sched_num++;
#ifdef CFG_FEATURE_VFIO
            event_list->slice_cur_event_num[event->vfid]--;
#endif
            ka_task_spin_unlock_bh(&event_list->lock);
            return event;
        }
    }
    ka_task_spin_unlock_bh(&event_list->lock);

    return NULL;
}

STATIC_INLINE struct sched_event *sched_cpu_get_specified_event(struct sched_cpu_ctx *cpu_ctx,
    struct sched_thread_ctx *thread_ctx, u32 event_id)
{
    struct sched_proc_ctx *proc_ctx = thread_ctx->grp_ctx->proc_ctx;
    struct sched_numa_node *node = cpu_ctx->node;
    struct sched_event_list *event_list = NULL;
    struct sched_event *event = NULL;

    if (ka_base_atomic_read(&node->cur_event_num) == 0) {
        return NULL;
    }

    event_list = sched_get_sched_event_list(node, proc_ctx->pri, proc_ctx->event_pri[event_id]);
    event = sched_get_specified_event(event_list, proc_ctx->pid, event_id, thread_ctx->grp_ctx->gid);
    if (event != NULL) {
        ka_base_atomic_dec(&node->cur_event_num);
    }

    return event;
}

STATIC bool sched_thread_map_has_thread(struct sched_event_thread_map *event_thread_map, u32 tid)
{
    u32 i;

    for (i = 0; i < event_thread_map->thread_num; i++) {
        if (tid == event_thread_map->thread[i]) {
            return true;
        }
    }

    return false;
}

STATIC bool sched_thread_can_handle_event(struct sched_thread_ctx *thread_ctx, struct sched_event *event)
{
    if (!(thread_ctx->subscribe_event_bitmap & (0x1ULL << (event->event_id)))) {
        return false;
    }

    if (event->event_thread_map == NULL) {
        return true;
    }

    if (!sched_thread_map_has_thread(event->event_thread_map, thread_ctx->tid)) {
        return false;
    }

    return true;
}

bool sched_grp_can_handle_event(struct sched_grp_ctx *grp_ctx, struct sched_event *event)
{
    struct sched_event_thread_map *grp_thread_map = &grp_ctx->event_thread_map[event->event_id];
    struct sched_event_thread_map *event_thread_map = event->event_thread_map;
    u32 i;

    if (grp_thread_map->thread_num == 0) {
        return false;
    }

    if (event_thread_map == NULL) {
        return true;
    }

    for (i = 0; i < event_thread_map->thread_num; i++) {
        if (!sched_thread_map_has_thread(grp_thread_map, event_thread_map->thread[i])) {
            return false;
        }
    }
    return true;
}

STATIC_INLINE struct sched_event *sched_search_non_sched_event(struct sched_event_list *event_list,
    struct sched_thread_ctx *thread_ctx)
{
    struct sched_event *event = NULL, *tmp = NULL;

    ka_task_spin_lock_bh(&event_list->lock);
    ka_list_for_each_entry_safe(event, tmp, &event_list->head, list) {
        if (sched_thread_can_handle_event(thread_ctx, event)) {
            ka_list_del(&event->list);
            event_list->cur_num--;
            event_list->sched_num++;
            ka_task_spin_unlock_bh(&event_list->lock);
            return event;
        }
    }
    ka_task_spin_unlock_bh(&event_list->lock);

    return NULL;
}

STATIC_INLINE void sched_record_event_item(struct sched_abnormal_event_item *event_info,
    struct sched_event *event, struct sched_thread_ctx *thread_ctx)
{
    event_info->timestamp = event->timestamp;
    event_info->event_id = event->event_id;
    event_info->publish_pid = event->publish_pid;
    event_info->publish_cpuid = event->publish_cpuid;
    event_info->wait_event_num_in_que = event->wait_event_num_in_que;
    event_info->proc_time = event->proc_time;

    if (thread_ctx != NULL) {
        event_info->pid = thread_ctx->grp_ctx->pid;
        event_info->gid = thread_ctx->grp_ctx->gid;
        event_info->tid = thread_ctx->tid;
        event_info->bind_cpuid = thread_ctx->bind_cpuid;
        event_info->kernel_tid = thread_ctx->kernel_tid;
        if (strncpy_s(event_info->name, TASK_COMM_LEN, thread_ctx->name, ka_base_strlen(thread_ctx->name)) != 0) {
#ifndef EMU_ST
            sched_warn("Unable to invoke the strncpy_s to copy name. (kernel_tid=%u)\n", thread_ctx->kernel_tid);
#endif
        }
        event_info->name[TASK_COMM_LEN - 1] = '\0';
    }
}

STATIC_INLINE void sched_record_abnormal_event(struct sched_abnormal_event *abnormal_event,
    struct sched_event *event, struct sched_thread_ctx *thread_ctx, u64 timestamp, u32 sched_mode)
{
    int32_t id;

    if (timestamp >= abnormal_event->timestamp_thres) {
        id = ka_base_atomic_inc_return(&abnormal_event->cur_index);
        id %= SCHED_ABNORMAL_EVENT_MAX_NUM;
        sched_record_event_item(&abnormal_event->event_info[id], event, thread_ctx);
    }
}

STATIC_INLINE void sched_record_non_sched_abnormal_event(struct sched_abnormal_event *abnormal_event,
    struct sched_event *event, struct sched_thread_ctx *thread_ctx, u64 timestamp, u32 sched_mode)
{
    int32_t id;

    if (timestamp >= microsecond_to_tick(NON_SCHED_DEFAULT_WAKEUP_TIME_THRES)) {
        id = ka_base_atomic_inc_return(&abnormal_event->cur_index);
        id %= SCHED_ABNORMAL_EVENT_MAX_NUM;
        sched_record_event_item(&abnormal_event->event_info[id], event, thread_ctx);
    }
}

STATIC_INLINE void sched_record_event_trace(struct sched_node_event_trace *event_trace,
    struct sched_event *event, struct sched_thread_ctx *thread_ctx)
{
    int32_t id;

    if (event_trace->enable_flag == SCHED_TRACE_ENABLE) {
        id = ka_base_atomic_inc_return(&event_trace->cur_index);
        id %= SCHED_EVENT_TRACE_MAX_NUM;
        sched_record_event_item(&event_trace->event_info[id], event, thread_ctx);
    }
}

STATIC_INLINE void sched_wakeup_stat(struct sched_numa_node *node, struct sched_event *event, u32 bind_cpuid)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_cpu_perf_stat *perf_stat = NULL;

    (void)event;

    cpu_ctx = sched_get_cpu_ctx(node, bind_cpuid);
    perf_stat = &cpu_ctx->perf_stat;

    perf_stat->total_wakeup_event_num++;
}

void sched_wake_up_thread(struct sched_thread_ctx *thread_ctx)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    int32_t old_status = ka_base_atomic_cmpxchg(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE, SCHED_THREAD_STATUS_READY);
    if (old_status == SCHED_THREAD_STATUS_IDLE) {
        if (thread_ctx->grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
            if (thread_ctx->event != NULL) {
                thread_ctx->event->timestamp.publish_wakeup = sched_get_cur_timestamp();
                sched_wakeup_stat(thread_ctx->grp_ctx->proc_ctx->node, thread_ctx->event, thread_ctx->bind_cpuid);
            }
        } else if (thread_ctx->grp_ctx->sched_mode == SCHED_MODE_NON_SCHED_CPU) {
            thread_ctx->non_sched_publish_wakeup = sched_get_cur_timestamp();
        }
        ka_task_wake_up_interruptible(&thread_ctx->wq);
    } else {
        if (thread_ctx->grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
            cpu_ctx = sched_get_cpu_ctx(thread_ctx->grp_ctx->proc_ctx->node, thread_ctx->bind_cpuid);
            esched_cpu_idle(cpu_ctx);
            sched_err("The thread status is abnormal. (thread_status=%d; bind_cpuid=%u; pid=%d; "
                "gid=%u; tid=%u; kernel_tid=%u; curr_wakeup_reason=%d; pre_wakeup_reason=%d)\n",
                old_status, thread_ctx->bind_cpuid, thread_ctx->grp_ctx->proc_ctx->pid,
                thread_ctx->grp_ctx->gid, thread_ctx->tid, thread_ctx->kernel_tid,
                thread_ctx->normal_wakeup_reason, thread_ctx->pre_normal_wakeup_reason);
        }
    }
}

STATIC_INLINE void sched_free_thread_event(struct sched_thread_ctx *thread_ctx, struct sched_event *event, u32 finish_scene)
{
    struct sched_proc_ctx *proc_ctx = thread_ctx->grp_ctx->proc_ctx;

    /*
     * The statistics can be increased only after processing is completed. If statistics are collected
     * when the event is dequeued, the process will exit and access the null pointer
     */
    ka_base_atomic_inc(&proc_ctx->sched_event_num);
    ka_base_atomic_inc(&proc_ctx->proc_event_stat[event->event_id].sched_event_num);
    ka_base_atomic_dec(&thread_ctx->grp_ctx->event_num[event->event_id]);
    (void)sched_event_enque_lock(event->que, event);

    thread_ctx->event = NULL;
    thread_ctx->event_finish_scene = (int32_t)finish_scene;
}

STATIC_INLINE void sched_event_finish(struct sched_thread_ctx *thread_ctx, struct sched_event *event, u32 finish_scene)
{
    u32 local_finish_scene = (finish_scene >= SCHED_TASK_FINISH_SCENE_NORMAL_SOFT_TIMEOUT) ? SCHED_TASK_FINISH_SCENE_NORMAL : finish_scene;

    sched_debug("Sched event finish. (event_id=%u; event pid=%d; publish_in_kernel=%llu; "
        "bind_cpuid=%u; pid=%d; gid=%u; tid=%u; kernel_tid=%u)\n",
        event->event_id, event->pid, event->timestamp.publish_in_kernel, thread_ctx->bind_cpuid,
        thread_ctx->grp_ctx->proc_ctx->pid, thread_ctx->grp_ctx->gid, thread_ctx->tid, thread_ctx->kernel_tid);

    if (event->event_finish_func != NULL) {
        u32 devid = thread_ctx->grp_ctx->proc_ctx->node->node_id;
        struct sched_event_func_info finish_info = {devid, event->subevent_id, event->msg, event->msg_len};
        event->event_finish_func(&finish_info, local_finish_scene, event->priv);
    }

    sched_free_thread_event(thread_ctx, event, finish_scene);
}

static inline void sched_event_drop(u32 dev_id, struct sched_event *event)
{
    if (event->event_finish_func != NULL) {
#ifndef EMU_ST
        struct sched_event_func_info finish_info = {dev_id, event->subevent_id, event->msg, event->msg_len};
        event->event_finish_func(&finish_info, SCHED_TASK_FINISH_SCENE_NORMAL, event->priv);
#endif
    }

    (void)sched_event_enque_lock(event->que, event);
}

STATIC_INLINE struct sched_thread_ctx *sched_get_next_thread_in_list(struct sched_cpu_ctx *cpu_ctx,
    struct sched_event_list *event_list)
{
    struct sched_event *event = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
#ifdef CFG_FEATURE_VFIO
    struct sched_vf_ctx *vf_ctx = NULL;
#endif

    do {
        event = sched_event_search(event_list, cpu_ctx);
        /* no more event */
        if (event == NULL) {
            thread_ctx = NULL;
            break;
        }

        ka_base_atomic_dec(&cpu_ctx->node->cur_event_num);

        /* After the thread wakeup, then put the proc */
        proc_ctx = esched_proc_get(cpu_ctx->node, event->pid);
        if ((proc_ctx == NULL) || (proc_ctx->start_timestamp > event->timestamp.publish_in_kernel)) {
            cpu_ctx->stat.proc_exit_drop_num++;

            if (proc_ctx != NULL) {
                sched_debug("Event drop proc change. (pid=%d; proc_start_timestamp=%llu; publish_in_kernel=%llu)\n",
                    event->pid, proc_ctx->start_timestamp, event->timestamp.publish_in_kernel);
                esched_proc_put(proc_ctx);
            }
            sched_debug("Event drop. (cpuid=%u; pid=%d; gid=%u)\n", cpu_ctx->cpuid, event->pid, event->gid);
            sched_event_drop(cpu_ctx->node->node_id, event);
            continue;
        }

        grp_ctx = sched_get_grp_ctx(proc_ctx, event->gid);
#ifdef CFG_FEATURE_VFIO
        vf_ctx = sched_get_vf_ctx(grp_ctx->proc_ctx->node, (u32)grp_ctx->proc_ctx->vfid);
        ka_base_atomic64_dec(&vf_ctx->stat.cur_event_num);
#endif

        thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->cpuid_to_tid[cpu_ctx->cpuid]);
        thread_ctx->event = event;

        /* The thread has timed out. discarded event */
        if (thread_ctx->timeout_flag == SCHED_VALID) {
            sched_event_finish(thread_ctx, event, SCHED_TASK_FINISH_SCENE_NORMAL_SOFT_TIMEOUT);
            ka_base_atomic_inc(&thread_ctx->stat.discard_event);
            thread_ctx = NULL;
            if (grp_ctx->is_exclusive == SCHED_VALID) {
                ka_base_atomic_set(&grp_ctx->run_status, SCHED_GRP_EXCLUSIVE_STATUS_IDLE);
            }
            esched_proc_put(proc_ctx);
            continue;
        }
#ifdef CFG_FEATURE_VFIO
        ka_base_atomic64_inc(&vf_ctx->stat.sched_event_num);
#endif
    } while (thread_ctx == NULL);

    return thread_ctx;
}

struct sched_thread_ctx *sched_get_next_thread(struct sched_cpu_ctx *cpu_ctx)
{
    u32 i, j;
    struct sched_thread_ctx *thread_ctx = NULL;

    for (i = 0; i < SCHED_MAX_PROC_PRI_NUM; i++) {
        for (j = 0; j < SCHED_MAX_EVENT_PRI_NUM; j++) {
            thread_ctx = sched_get_next_thread_in_list(cpu_ctx, sched_get_sched_event_list(cpu_ctx->node, i, j));
            if (thread_ctx != NULL) {
                return thread_ctx;
            }
        }
    }

    return NULL;
}

STATIC void sched_wake_up_cpu_task(struct sched_cpu_ctx *cpu_ctx, int32_t wakeup_reason)
{
    struct sched_thread_ctx *thread_ctx = NULL;

#if defined(CFG_FEATURE_HARDWARE_MIA) && !defined(ESCHED_HOST)
    if (!sched_is_cpu_belongs_to_vf(cpu_ctx->node, 0, cpu_ctx)) {
        return;
    }
#endif

    if (ka_task_spin_trylock_bh(&cpu_ctx->sched_lock) == 0) {
        return;
    }

    if (esched_is_cpu_idle(cpu_ctx)) {
        thread_ctx = sched_get_next_thread(cpu_ctx);
        if (thread_ctx != NULL) {
            esched_cpu_cur_thread_set(cpu_ctx, thread_ctx);
            thread_ctx->pre_normal_wakeup_reason = thread_ctx->normal_wakeup_reason;
            thread_ctx->normal_wakeup_reason = wakeup_reason;
            ka_wmb();
            sched_wake_up_thread(thread_ctx);
            /* proc get in sched_get_next_thread, after wakeup, should put proc */
            esched_proc_put(thread_ctx->grp_ctx->proc_ctx);
        }
    }
    ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
}

STATIC_INLINE void sched_fill_subcribed_event(struct sched_thread_ctx *thread_ctx,
    struct sched_subscribed_event *subscribed_event)
{
    struct sched_event *event = thread_ctx->event;

    subscribed_event->host_pid = event->publish_pid; /* use host_pid to record publish pid */
    subscribed_event->pid = event->pid;
    subscribed_event->gid = event->gid;
    subscribed_event->event_id = event->event_id;
    subscribed_event->subevent_id = event->subevent_id;
    subscribed_event->publish_user_timestamp = event->timestamp.publish_user;
    subscribed_event->subscribe_timestamp = thread_ctx->start_time;
    subscribed_event->publish_kernel_timestamp = event->timestamp.publish_in_kernel;
    subscribed_event->publish_finish_timestamp = event->timestamp.publish_out_kernel;
    subscribed_event->publish_wakeup_timestamp = event->timestamp.publish_wakeup;
    subscribed_event->msg_len = 0;

    if (event->msg_len > SCHED_MAX_EVENT_MSG_LEN) {
        sched_err("pid %d gid %u event id %u copy_to_user fail, msg length exceeds!, msg length:%u\n",
            event->pid, event->gid, event->event_id, event->msg_len);
        return;
    }

    if (event->msg_len > 0) {
        if (copy_to_user_safe(subscribed_event->msg, (void *)event->msg, event->msg_len) != 0) {
            sched_err("Failed to invoke the copy_to_user_safe. (pid=%d; gid=%u; event_id=%u)\n",
                event->pid, event->gid, event->event_id);
        }
    }

    subscribed_event->msg_len = event->msg_len;
}

STATIC_INLINE int32_t sched_wait_for_publish_event(struct sched_thread_ctx *thread_ctx, int32_t timeout)
{
    int32_t ret = 0;
    long wait_ret = 0;
    struct sched_proc_ctx *proc_ctx = thread_ctx->grp_ctx->proc_ctx;
    uint32_t jf_timeout;

    if ((timeout == SCHED_WAIT_TIMEOUT_NO_WAIT) || (timeout == SCHED_THREAD_SWAPOUT)) {
        /* no wait or swap out */
        if (proc_ctx->status == SCHED_INVALID) {
            return DRV_ERROR_PROCESS_EXIT;
        } else if (ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_READY) {
            return 0;
        } else {
            return DRV_ERROR_NO_EVENT;
        }
    }

    if (timeout > 0) {
        /* monitor process exit or thread ready, wait function will check the condition firstly */
        jf_timeout = ka_system_msecs_to_jiffies((uint32_t)timeout);
        wait_ret = ka_task_wait_event_interruptible_timeout(thread_ctx->wq,
            ((ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_READY) || (proc_ctx->status == SCHED_INVALID)),
            jf_timeout);
        if (wait_ret == 0) { /* timeout */
            return DRV_ERROR_WAIT_TIMEOUT;
        } else if (wait_ret > 0) {
            wait_ret = 0;
        }
    } else {
        /* always wait */
        wait_ret = ka_task_wait_event_interruptible(thread_ctx->wq,
            ((ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_READY) || (proc_ctx->status == SCHED_INVALID)));
    }

    if (wait_ret == 0) {
        ret = (proc_ctx->status == SCHED_INVALID) ? DRV_ERROR_PROCESS_EXIT : 0;
    } else {
        sched_warn("Show warning details. (pid=%d; gid=%u; tid=%u; ret=%ld)\n",
            proc_ctx->pid, thread_ctx->grp_ctx->gid, thread_ctx->tid, wait_ret);
        ret = (wait_ret == -ERESTARTSYS) ? DRV_ERROR_WAIT_INTERRUPT : DRV_ERROR_INNER_ERR;
    }

    return ret;
}

STATIC_INLINE void sched_record_switch_thread_run(struct sched_cpu_ctx *cpu_ctx, struct sched_thread_ctx *thread_ctx)
{
    struct sched_cpu_sched_trace *sched_trace = cpu_ctx->sched_trace;
    struct sched_cpu_sched_thread *sched_thread = NULL;

    cpu_ctx->stat.sched_event_num++;

    if (sched_trace != NULL) {
        sched_trace->record_index = (u32)(sched_trace->trace_num++ & SCHED_SWICH_THREAD_NUM_MASK);
        sched_thread = &sched_trace->sched_thread_list[sched_trace->record_index];

        sched_thread->pid = thread_ctx->grp_ctx->pid;
        sched_thread->pid_pri = thread_ctx->grp_ctx->proc_ctx->pri;
        sched_thread->gid = thread_ctx->grp_ctx->gid;
        sched_thread->tid = thread_ctx->tid;
        sched_thread->publish_timestamp = thread_ctx->event->timestamp.publish_in_kernel;
        sched_thread->add_queue_list_timestamp = thread_ctx->event->timestamp.publish_add_event_list;
        sched_thread->sched_timestamp = thread_ctx->start_time;
        sched_thread->finish_timestamp = 0;
        sched_thread->callback_timestamp = 0;
        sched_thread->normal_wakeup_reason = thread_ctx->normal_wakeup_reason;
        sched_thread->wait_time = thread_ctx->start_time - thread_ctx->event->timestamp.publish_in_kernel;
        sched_thread->event_id = thread_ctx->event->event_id;
        sched_thread->event_pri = thread_ctx->grp_ctx->proc_ctx->event_pri[sched_thread->event_id];
        sched_thread->waked_to_wakeup_time = ((thread_ctx->event->timestamp.publish_wakeup != 0) ?
            (thread_ctx->event->timestamp.subscribe_waked - thread_ctx->event->timestamp.publish_wakeup) : 0);
    }
}

STATIC_INLINE void sched_record_switch_thread_stop(struct sched_cpu_ctx *cpu_ctx, struct sched_thread_ctx *thread_ctx)
{
    struct sched_cpu_sched_trace *sched_trace = cpu_ctx->sched_trace;
    struct sched_cpu_sched_thread *sched_thread = NULL;

    if (sched_trace != NULL) {
        sched_thread = &sched_trace->sched_thread_list[sched_trace->record_index];
        sched_thread->cur_event_num = ka_base_atomic_read(&cpu_ctx->node->cur_event_num);
        sched_thread->finish_timestamp = thread_ctx->end_time;
        sched_thread->callback_timestamp = thread_ctx->callback_end_time;
    }

    cpu_ctx->perf_stat.total_use_time += thread_ctx->end_time - thread_ctx->start_time;
}

STATIC_INLINE void sched_update_thread_event_time(struct sched_thread_ctx *thread_ctx)
{
    u64 thread_sched_time;

    thread_ctx->start_time = sched_get_cur_timestamp();
    thread_sched_time = thread_ctx->start_time - thread_ctx->event->timestamp.publish_user;
    if (thread_ctx->max_sched_time < thread_sched_time) {
        thread_ctx->max_sched_time = thread_sched_time;
    }

    thread_ctx->total_sched_time += thread_sched_time;
}

STATIC_INLINE void sched_wakeup_next_thread(struct sched_cpu_ctx *cpu_ctx,
    struct sched_thread_ctx *cur_thread, struct sched_thread_ctx *next_thread)
{
    /*
     * if the next thread to wake up is not the current thread, set the current thread to idle, then wake up
     * next thread; otherwise, just need to set the current thread to the ready state
     */
    if (next_thread != cur_thread) {
        ka_base_atomic_set(&cur_thread->status, SCHED_THREAD_STATUS_IDLE);
        esched_cpu_cur_thread_set(cpu_ctx, next_thread);
        cur_thread->pre_normal_wakeup_reason = cur_thread->normal_wakeup_reason;
        next_thread->normal_wakeup_reason = SCHED_WAKED_BY_OTHER_THREAD;
        ka_wmb();
        sched_wake_up_thread(next_thread);
    } else {
        cur_thread->pre_normal_wakeup_reason = cur_thread->normal_wakeup_reason;
        cur_thread->normal_wakeup_reason = SCHED_WAKED_BY_SELF;
        ka_base_atomic_set(&cur_thread->status, SCHED_THREAD_STATUS_READY);
    }
}

void sched_cpu_to_idle(struct sched_thread_ctx *thread_ctx, struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_thread_ctx *next_thread = NULL;

    ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
    if (ka_base_atomic_read(&cpu_ctx->node->cur_event_num) > 0) {
        next_thread = sched_get_next_thread(cpu_ctx);
        if (next_thread == NULL) {
            ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
            esched_cpu_idle(cpu_ctx);
        } else {
            sched_wakeup_next_thread(cpu_ctx, thread_ctx, next_thread);
        }
    } else {
        ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
        esched_cpu_idle(cpu_ctx);
    }
    ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);

    if (next_thread != NULL) {
        esched_proc_put(next_thread->grp_ctx->proc_ctx);
    }
}

STATIC_INLINE void sched_fill_sched_thread_subcribed_event(struct sched_cpu_ctx *cpu_ctx,
    struct sched_thread_ctx *thread_ctx, struct sched_subscribed_event *subscribed_event)
{
    cpu_ctx->last_sched_timestamp = sched_get_cur_timestamp();
    thread_ctx->event->timestamp.subscribe_waked = sched_get_cur_timestamp();
    sched_update_thread_event_time(thread_ctx);
    sched_fill_subcribed_event(thread_ctx, subscribed_event);
    ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_RUN);
    thread_ctx->stat.sched_event++;
    sched_record_switch_thread_run(cpu_ctx, thread_ctx);
}

STATIC_INLINE int32_t sched_wait_sched_event(struct sched_thread_ctx *thread_ctx,
    struct sched_subscribed_event *subscribed_event, u32 cpuid, int32_t timeout)
{
    struct sched_grp_ctx *grp_ctx = thread_ctx->grp_ctx;
    struct sched_numa_node *node = grp_ctx->proc_ctx->node;
    struct sched_cpu_ctx *cpu_ctx = sched_get_cpu_ctx(node, cpuid);
    struct sched_thread_ctx *next_thread = NULL;
    int32_t ret;

    if (thread_ctx->bind_cpuid != cpuid) {
        sched_err("It's not running on the bound cpu. (pid=%d; gid=%u; tid=%u; cpuid=%u; bind_cpuid=%u)\n",
                  grp_ctx->pid, grp_ctx->gid, thread_ctx->tid, cpuid, thread_ctx->bind_cpuid);
        return DRV_ERROR_RUN_IN_ILLEGAL_CPU;
    }

    if (ka_likely(ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_RUN)) {
        ka_base_atomic_set(&cpu_ctx->cpu_status, CPU_STATUS_IDLE);
        next_thread = node->ops.sched_cpu_get_next_thread(cpu_ctx);
        /* No events to process, set the cpu state to idle */
        if (next_thread == NULL) {
            if (node->ops.sched_cpu_to_idle != NULL) {
                node->ops.sched_cpu_to_idle(thread_ctx, cpu_ctx);
            }
        } else {
            /*
             * if the next thread to wake up is not the current thread, set the current thread to idle, then wake up
             * next thread; otherwise, just need to set the current thread to the ready state
             */
            ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
            sched_wakeup_next_thread(cpu_ctx, thread_ctx, next_thread);
            ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
            /* proc get in sched_get_next_thread, after wakeup, should put proc */
            esched_proc_put(next_thread->grp_ctx->proc_ctx);
        }
    } else if (esched_is_cpu_idle(cpu_ctx)) {
        /* check and schedule task */
        if (ka_base_atomic_read(&node->cur_event_num) != 0) {
            sched_wake_up_cpu_task(cpu_ctx, SCHED_WAKED_BY_EMPTY_CHECK);
        }
    }

    ka_task_mutex_unlock(&thread_ctx->thread_mutex);
    ret = sched_wait_for_publish_event(thread_ctx, timeout);

    ka_task_mutex_lock(&thread_ctx->thread_mutex);
    if (ret == 0) {
        ka_rmb();
        if (thread_ctx->event != NULL) {
            ka_base_atomic_set(&cpu_ctx->cpu_status, CPU_STATUS_RUN);
            sched_fill_sched_thread_subcribed_event(cpu_ctx, thread_ctx, subscribed_event);
        } else {
            sched_err("It is awake, but has no event proc status. "
                "(pid=%d; gid=%u; tid=%u; status=%d; thread_status=%d; pre_finish_scene=%d, event=0x%llx)\n",
                grp_ctx->pid, grp_ctx->gid, thread_ctx->tid, grp_ctx->proc_ctx->status,
                ka_base_atomic_read(&thread_ctx->status), thread_ctx->event_finish_scene, (u64)(uintptr_t)thread_ctx->event);
            ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
            ka_wmb();
            esched_cpu_idle(cpu_ctx);
            ret = DRV_ERROR_INNER_ERR;
        }
    }

    return ret;
}

STATIC_INLINE struct sched_event *sched_get_next_non_sched_event(struct sched_thread_ctx *thread_ctx)
{
    struct sched_grp_ctx *grp_ctx = thread_ctx->grp_ctx;
    struct sched_event_list *event_list = NULL;
    struct sched_event *event = NULL;
    u32 i;

    for (i = 0; i < SCHED_MAX_EVENT_PRI_NUM; i++) {
        event_list = sched_get_non_sched_event_list(grp_ctx, i);

        event = sched_search_non_sched_event(event_list, thread_ctx);
        if (event != NULL) {
            ka_base_atomic_dec(&grp_ctx->cur_event_num);
            return event;
        }
    }

    return NULL;
}

STATIC_INLINE void sched_fill_non_sched_thread_subcribed_event(
    struct sched_thread_ctx *thread_ctx, struct sched_subscribed_event *subscribed_event)
{
    thread_ctx->event->timestamp.subscribe_waked = sched_get_cur_timestamp();
    thread_ctx->event->timestamp.publish_wakeup = thread_ctx->non_sched_publish_wakeup;
    thread_ctx->non_sched_publish_wakeup = 0;
    thread_ctx->stat.sched_event++;
    thread_ctx->start_time = sched_get_cur_timestamp(); /* reset start time */
    ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_RUN);
    sched_fill_subcribed_event(thread_ctx, subscribed_event);
    sched_update_thread_event_time(thread_ctx);
}

STATIC_INLINE int32_t sched_wait_non_sched_event(struct sched_thread_ctx *thread_ctx,
    struct sched_subscribed_event *subscribed_event, int32_t timeout)
{
    int32_t ret;

    if (ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_RUN) {
        ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
        ka_wmb();
    }

again:
    thread_ctx->event = sched_get_next_non_sched_event(thread_ctx);
    if (thread_ctx->event != NULL) {
        sched_fill_non_sched_thread_subcribed_event(thread_ctx, subscribed_event);
        return 0;
    }

    ka_task_mutex_unlock(&thread_ctx->thread_mutex);
    ret = sched_wait_for_publish_event(thread_ctx, timeout);
    ka_task_mutex_lock(&thread_ctx->thread_mutex);
    if ((ret == 0) || (ret == DRV_ERROR_WAIT_TIMEOUT)) { /* Timeout also attempts to search for more robustness */
        thread_ctx->event = sched_get_next_non_sched_event(thread_ctx);
        if (thread_ctx->event != NULL) {
            sched_fill_non_sched_thread_subcribed_event(thread_ctx, subscribed_event);
            return 0;
        }

        /*
         * Scenario: The publishing thread first enters the queue and then schedule out. the consumer thread
         * is allready in the run state. The event in the queue is consumed then the thread state is set to idle,
         * the publishing thread is schedule back, the thread state is set to ready wake up consumer. Then the
         * consumer thread will find that the queue is empty
         */
        if (ret == 0) {
            ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
            ka_wmb();
            goto again;
        }
    }

    return ret;
}

STATIC_INLINE void sched_exclusive_grp_finish(struct sched_grp_ctx *grp_ctx, u32 cur_cpuid)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 wait_cpu_mask, i;

    ka_base_atomic_set(&grp_ctx->run_status, SCHED_GRP_EXCLUSIVE_STATUS_IDLE);

    wait_cpu_mask = (u32)ka_base_atomic_read(&grp_ctx->wait_cpu_mask);
    if (wait_cpu_mask == 0) {
        return;
    }

    for (i = 0; i < grp_ctx->thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->thread[i]);
        /* try to wakeup other cpu */
        if (thread_ctx->bind_cpuid == cur_cpuid) {
            continue;
        }

        /* cpu not wait run the grp thread */
        if ((wait_cpu_mask & (0x1U << thread_ctx->bind_cpuid)) == 0) {
            continue;
        }

        cpu_ctx = sched_get_cpu_ctx(grp_ctx->proc_ctx->node, thread_ctx->bind_cpuid);
        if (!esched_is_cpu_idle(cpu_ctx)) {
            continue;
        }

        cpu_ctx->stat.exclusive_sched_num++;
        sched_wake_up_cpu_task(cpu_ctx, SCHED_WAKED_BY_EXCLUSIVE_FINISH);

        /* one thread in the group has been wakeup, do not need to try another */
        if (ka_base_atomic_read(&grp_ctx->run_status) == SCHED_GRP_EXCLUSIVE_STATUS_RUN) {
            break;
        }
    }
}

void sched_thread_finish(struct sched_thread_ctx *thread_ctx, u32 finish_scene)
{
    struct sched_event *event = thread_ctx->event;
    struct sched_numa_node *node = thread_ctx->grp_ctx->proc_ctx->node;
    u64 proc_time;

    if (event == NULL) {
        sched_err("The variable event is NULL. (pid=%d; gid=%u; tid=%u)\n",
                  thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid, thread_ctx->tid);
        return;
    }

    thread_ctx->end_time = sched_get_cur_timestamp();

    proc_time = thread_ctx->end_time - thread_ctx->start_time;

    /* upgrade max proc time and total proc time */
    if (proc_time > thread_ctx->max_proc_time) {
        thread_ctx->max_proc_time = proc_time;
    }
    thread_ctx->total_proc_time += proc_time;

    event->proc_time = proc_time;
    sched_record_abnormal_event(&node->abnormal_event.proc, event, thread_ctx, proc_time,
        thread_ctx->grp_ctx->sched_mode);
    if ((node->debug_flag & (0x1 << SCHED_DEBUG_EVENT_TRACE_BIT)) != 0) {
        sched_record_event_trace(&node->event_trace, event, thread_ctx);
    }

    if (thread_ctx->grp_ctx->is_exclusive == SCHED_VALID) {
        sched_exclusive_grp_finish(thread_ctx->grp_ctx, thread_ctx->bind_cpuid);
    }

    /* finish callback mast be last */
    sched_event_finish(thread_ctx, event, finish_scene);

    thread_ctx->callback_end_time = sched_get_cur_timestamp();
    sched_record_switch_thread_stop(sched_get_cpu_ctx(node, thread_ctx->bind_cpuid), thread_ctx);
}

STATIC_INLINE void sched_wait_stat(struct sched_numa_node *node, struct sched_thread_ctx *thread_ctx)
{
    u64 wakeup_time = 0;
    u64 publish_subscribe_time;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_cpu_perf_stat *perf_stat = NULL;
    struct sched_event *event = thread_ctx->event;

    if (event->timestamp.publish_wakeup != 0) {
        wakeup_time = event->timestamp.subscribe_waked - event->timestamp.publish_wakeup;
    }
    publish_subscribe_time = event->timestamp.subscribe_out_kernel - event->timestamp.publish_user;

    if (thread_ctx->grp_ctx->sched_mode == SCHED_MODE_NON_SCHED_CPU) {
        sched_record_non_sched_abnormal_event(&node->abnormal_event.wakeup, event, thread_ctx, wakeup_time,
            thread_ctx->grp_ctx->sched_mode);
    } else {
        sched_record_abnormal_event(&node->abnormal_event.wakeup, event, thread_ctx, wakeup_time,
            thread_ctx->grp_ctx->sched_mode);
    }
    sched_record_abnormal_event(&node->abnormal_event.publish_subscribe, event, thread_ctx, publish_subscribe_time,
        thread_ctx->grp_ctx->sched_mode);

    cpu_ctx = sched_get_cpu_ctx(node, thread_ctx->bind_cpuid);
    perf_stat = &cpu_ctx->perf_stat;

    perf_stat->sched_event_num[event->event_id]++;
    perf_stat->total_sched_event_num++;
    perf_stat->wakeup_total_time += wakeup_time;
    if (wakeup_time > perf_stat->wakeup_max_time) {
        perf_stat->wakeup_max_time = wakeup_time;
    }

    perf_stat->publish_subscribe_total_time += publish_subscribe_time;
    if (publish_subscribe_time > perf_stat->publish_subscribe_max_time) {
        perf_stat->publish_subscribe_max_time = publish_subscribe_time;
    }
}

STATIC_INLINE int32_t sched_wait_event_para_check(u32 chip_id, int32_t pid, u32 gid, int32_t timeout)
{
    if (gid >= SCHED_MAX_GRP_NUM) {
        sched_err("The value of variable gid is out of range. (chip_id=%u; pid=%d; gid=%u; max=%d)\n",
            chip_id, pid, gid, SCHED_MAX_GRP_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

#if (defined CFG_FEATURE_HARDWARE_SCHED) && \
    (!defined CFG_FEATURE_THREAD_SWAPOUT) && (!defined CFG_FEATURE_HARD_SOFT_SCHED)
    if (timeout == SCHED_THREAD_SWAPOUT) {
#ifndef EMU_ST
        sched_warn("Swapout thread_ctx from cpu not support yet.\n");
#endif
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif

    return 0;
}

int32_t sched_get_event_trace(u32 chip_id, char *buff, u32 buff_len, u32 *data_len)
{
    struct sched_numa_node *node = NULL;
    struct sched_node_event_trace *event_trace = NULL;
    u32 copy_len;
    int id;
    *data_len = 0;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    event_trace = &node->event_trace;
    id = ka_base_atomic_read(&event_trace->cur_index) % SCHED_EVENT_TRACE_MAX_NUM;
    event_trace->enable_flag = SCHED_TRACE_DISABLE;

    copy_len = (SCHED_EVENT_TRACE_MAX_NUM - (u32)id - 1) * sizeof(struct sched_abnormal_event_item);
    copy_len = (copy_len > buff_len) ? buff_len : copy_len;

    if (copy_len != 0) {
        if (copy_to_user_safe((void *)(uintptr_t)buff, (void *)&(event_trace->event_info[id + 1]), copy_len) != 0) {
            sched_err("Failed to invoke the copy_to_user_safe.\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }

    buff_len -= copy_len;
    *data_len += copy_len;
    if (buff_len == 0) {
        return 0;
    }

    copy_len = (SCHED_EVENT_TRACE_MAX_NUM * sizeof(struct sched_abnormal_event_item)) - copy_len;
    copy_len = (copy_len > buff_len) ? buff_len : copy_len;
    if (copy_len != 0) {
        if (copy_to_user_safe((void *)(uintptr_t)buff + *data_len,
            (void *)&(event_trace->event_info[0]), copy_len) != 0) {
            sched_err("Failed to invoke the copy_to_user_safe.\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }
    *data_len += copy_len;

    event_trace->enable_flag = SCHED_TRACE_ENABLE;

    return 0;
}

STATIC_INLINE void sched_thread_status_check(struct sched_thread_ctx *thread_ctx)
{
    struct sched_event_timeout_info *event_timeout_info = NULL;
    u64 event_bitmap;
    u32 i;

    if (thread_ctx->kernel_tid != (u32)SCHED_CURRENT_PID) {
        if (thread_ctx->grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
            sched_warn("It restarted. (pid=%d; gid=%u; tid=%u; kernel_tid=%u; current_pid=%d)\n",
                thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid,
                thread_ctx->tid, thread_ctx->kernel_tid, SCHED_CURRENT_PID);
        }
        thread_ctx->kernel_tid = SCHED_CURRENT_PID;
    }

    if (thread_ctx->timeout_flag == SCHED_VALID) {
        sched_warn("It recovered from a timeout. (pid=%d; gid=%u; tid=%u)\n",
                   thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid, thread_ctx->tid);
        thread_ctx->timeout_flag = SCHED_INVALID;
        thread_ctx->timeout_detected = SCHED_INVALID;
        ka_task_spin_lock_bh(&thread_ctx->grp_ctx->lock);
        for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
            event_bitmap = thread_ctx->subscribe_event_bitmap;
            if ((event_bitmap & (0x1ULL << i)) != 0) {
                event_timeout_info = &thread_ctx->grp_ctx->event_timeout_info[i];
                event_timeout_info->timeout_thread_num--;
                event_timeout_info->timeout_flag = SCHED_INVALID;
                event_timeout_info->timeout_timestamp = 0;
            }
        }
        ka_task_spin_unlock_bh(&thread_ctx->grp_ctx->lock);
    }
}

STATIC_INLINE int32_t sched_thread_update(struct sched_thread_ctx *thread_ctx, u32 cpuid, u32 tid, int32_t timeout)
{
    int32_t ret;

    if (thread_ctx->wait_flag == SCHED_VALID) {
        sched_err("The same thread waits for events at the same time. "
            "(pid=%d; gid=%u; tid=%u; current_pid=%d; sched_mode=%u)\n",
            thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid, tid, SCHED_CURRENT_PID, thread_ctx->grp_ctx->sched_mode);
        ret = DRV_ERROR_THREAD_EXCEEDS_SPEC;
        return ret;
    }

    /* swapout current thread_ctx from cpu if timeout equal to -2, not wait event */
    if (timeout != SCHED_THREAD_SWAPOUT) {
        thread_ctx->wait_flag = SCHED_VALID;
    } else {
#ifdef CFG_FEATURE_THREAD_SWAPOUT
        thread_ctx->swapout_flag = SCHED_VALID;
#endif
#ifdef CFG_FEATURE_HARD_SOFT_SCHED
        esched_drv_dev_to_hw_soft_sched_mode(&thread_ctx->grp_ctx->proc_ctx->node->hard_res);
#endif
    }

    /* first time get event */
    if (thread_ctx->valid == SCHED_INVALID) {
        ret = sched_grp_add_thread(thread_ctx->grp_ctx, tid, cpuid);
        if (ret != 0) {
            thread_ctx->wait_flag = SCHED_INVALID;
            return ret;
        }
    } else {
        ka_task_spin_lock_bh(&thread_ctx->thread_finish_lock);
        if (ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_RUN) {
            sched_thread_finish(thread_ctx, SCHED_TASK_FINISH_SCENE_NORMAL);
#ifdef CFG_FEATURE_VFIO
            sched_release_cpu_res(thread_ctx);
#endif
        }
        ka_task_spin_unlock_bh(&thread_ctx->thread_finish_lock);
    }

    sched_thread_status_check(thread_ctx);
    return 0;
}

int32_t sched_wait_event(u32 chip_id, int32_t pid, u32 gid, u32 tid, int32_t timeout,
    struct sched_subscribed_event *subscribed_event)
{
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 cpuid = 0;
    int32_t ret;
    u64 subscribe_in_kernel = sched_get_cur_timestamp();

    ret = sched_wait_event_para_check(chip_id, pid, gid, timeout);
    if (ret != 0) {
        return ret;
    }

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
#ifndef EMU_ST
        sched_err_printk_ratelimited("Failed to get proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, pid);
#endif
        return DRV_ERROR_NO_PROCESS;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        sched_err("It is not added. (chip_id=%u; pid=%d; gid=%u)\n", chip_id, pid, gid);
        esched_proc_put(proc_ctx);
        return DRV_ERROR_NO_GROUP;
    }

    if ((timeout == SCHED_THREAD_SWAPOUT) && (grp_ctx->sched_mode == SCHED_MODE_NON_SCHED_CPU)) {
        sched_info("Swapout thread_ctx from cpu not support for non sched mode.\n");
        esched_proc_put(proc_ctx);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (tid >= grp_ctx->cfg_thread_num) {
        sched_err("The value of variable tid is out of range. (chip_id=%u; pid=%d; gid=%u; gid=%u; max=%u)\n",
            chip_id, pid, gid, tid, grp_ctx->cfg_thread_num);
        esched_proc_put(proc_ctx);
        return DRV_ERROR_PARA_ERROR;
    }

    if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
        ret = sched_get_sched_cpuid_in_node(node, &cpuid);
        if (ret != 0) {
            sched_err("Not sched cpu. (chip_id=%u; cpu_num=%u; cur_processor_id=%u)\n",
                node->node_id, node->cpu_num, sched_get_cur_processor_id());
            esched_proc_put(proc_ctx);
            return ret;
        }
    }

    thread_ctx = sched_get_thread_ctx(grp_ctx, tid);

    /* permit multi thread wait event using the same tid */
    ka_task_mutex_lock(&thread_ctx->thread_mutex);

    ret = sched_thread_update(thread_ctx, cpuid, tid, timeout);
    if (ret != 0) {
        sched_err("Failed to invoke the sched_thread_update. (ret=%d)\n", ret);
        goto out;
    }

    if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
        ret = sched_wait_sched_event(thread_ctx, subscribed_event, cpuid, timeout);
    } else {
        ret = sched_wait_non_sched_event(thread_ctx, subscribed_event, timeout);
    }

    if (ret == 0) {
        thread_ctx->event->timestamp.subscribe_in_kernel = subscribe_in_kernel;
        thread_ctx->event->timestamp.subscribe_out_kernel = sched_get_cur_timestamp();
        sched_wait_stat(node, thread_ctx);
        esched_wait_trace_update(proc_ctx, thread_ctx->event);
    }

    thread_ctx->wait_flag = SCHED_INVALID;

out:
    ka_task_mutex_unlock(&thread_ctx->thread_mutex);
    esched_proc_put(proc_ctx);
    return ret;
}

STATIC void sched_fill_exact_event(struct sched_cpu_ctx *cpu_ctx, struct sched_thread_ctx *thread_ctx,
    struct sched_event *event, struct sched_subscribed_event *subscribed_event)
{
    u64 subscribe_in_kernel = sched_get_cur_timestamp();

    /* Inherit the hook function of the wait event in the split operator scenario. */
#ifndef CFG_FEATURE_HARDWARE_SCHED
    event->priv = thread_ctx->event->priv;
#endif
    event->event_ack_func = thread_ctx->event->event_ack_func;
    event->event_finish_func = thread_ctx->event->event_finish_func;

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
    if ((cpu_ctx != NULL) && (!esched_drv_cpu_is_hw_sched_mode(cpu_ctx->topic_chan))) {
        thread_ctx->event->event_finish_func = NULL;
    }
#endif
    sched_thread_finish(thread_ctx, SCHED_TASK_FINISH_SCENE_NORMAL_GETEVENT);
    thread_ctx->event = event;

    thread_ctx->event->timestamp.subscribe_in_kernel = subscribe_in_kernel;
    thread_ctx->event->timestamp.subscribe_out_kernel = sched_get_cur_timestamp();
    thread_ctx->event->timestamp.subscribe_waked = sched_get_cur_timestamp();
    thread_ctx->start_time = sched_get_cur_timestamp(); /* reset start time */
    thread_ctx->stat.sched_event++;

    sched_fill_subcribed_event(thread_ctx, subscribed_event);
}

STATIC int32_t sched_get_exact_sched_event(struct sched_numa_node *node,
    struct sched_thread_ctx *thread_ctx, u32 event_id, struct sched_subscribed_event *subscribed_event)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_event *event = NULL;
    u32 cpuid;
    int32_t ret;

    ret = sched_get_sched_cpuid_in_node(node, &cpuid);
    if (ret != 0) {
        sched_err("Not sched cpu. (chip_id=%u; cpu_num=%u; cur_processor_id=%u)\n",
            node->node_id, node->cpu_num, sched_get_cur_processor_id());
        return ret;
    }

    cpu_ctx = sched_get_cpu_ctx(node, cpuid);
    if (!esched_is_cpu_cur_thread(cpu_ctx, thread_ctx)) {
        sched_err("The thread does not match. (chip_id=%u; cpu_id=%u; event_id=%u; pid=%d; gid=%u; tid=%u)\n",
            node->node_id, cpuid, event_id, thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid, thread_ctx->tid);
        return DRV_ERROR_PROCESS_NOT_MATCH;
    }

    event = node->ops.sched_cpu_get_event(cpu_ctx, thread_ctx, event_id);
    if (event == NULL) {
        return DRV_ERROR_NO_EVENT;
    }

    sched_fill_exact_event(cpu_ctx, thread_ctx, event, subscribed_event);
    cpu_ctx->stat.sched_event_num++;

    return 0;
}

STATIC int32_t sched_get_exact_non_sched_event(struct sched_thread_ctx *thread_ctx,
    u32 event_id, struct sched_subscribed_event *subscribed_event)
{
    struct sched_grp_ctx *grp_ctx = thread_ctx->grp_ctx;
    struct sched_proc_ctx *proc_ctx = grp_ctx->proc_ctx;
    struct sched_event_list *event_list = NULL;
    struct sched_event *event = NULL;

    if (ka_base_atomic_read(&grp_ctx->cur_event_num) == 0) {
        return DRV_ERROR_NO_EVENT;
    }

    event_list = sched_get_non_sched_event_list(grp_ctx, proc_ctx->event_pri[event_id]);
    event = sched_get_specified_event(event_list, proc_ctx->pid, event_id, grp_ctx->gid);
    if (event == NULL) {
        return DRV_ERROR_NO_EVENT;
    }

    sched_fill_exact_event(NULL, thread_ctx, event, subscribed_event);
    ka_base_atomic_dec(&grp_ctx->cur_event_num);

    return 0;
}

int32_t sched_get_exact_event(u32 chip_id, int32_t pid, u32 gid, u32 tid, u32 event_id,
    struct sched_subscribed_event *subscribed_event)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    int ret = 0;

    if ((event_id >= SCHED_MAX_EVENT_TYPE_NUM) || (gid >= SCHED_MAX_GRP_NUM)) {
        sched_err("The event_id, gid or tid is invalid. (event_id=%u; max=%d; gid=%u; max=%d)\n",
                  event_id, SCHED_MAX_EVENT_TYPE_NUM, gid, SCHED_MAX_GRP_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%d)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        sched_err("Get proc_ctx failed. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NO_PROCESS;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        sched_err("It is not added. (chip_id=%u; pid=%d; gid=%u)\n", chip_id, pid, gid);
        ret = DRV_ERROR_NO_GROUP;
        goto out;
    }

    if (tid >= grp_ctx->cfg_thread_num) {
        sched_err("The value of variable tid is out of range. (chip_id=%u; pid=%d; gid=%u; gid=%u; max=%u)\n",
            chip_id, pid, gid, tid, grp_ctx->cfg_thread_num);
        ret = DRV_ERROR_PARA_ERROR;
        goto out;
    }
    thread_ctx = sched_get_thread_ctx(grp_ctx, tid);

    if (ka_base_atomic_read(&thread_ctx->status) != SCHED_THREAD_STATUS_RUN) {
        sched_err("It's not running. (chip_id=%u; pid=%d; gid=%u; tid=%u; event_id=%u)\n",
                  chip_id, pid, gid, tid, event_id);
        ret = DRV_ERROR_THREAD_NOT_RUNNIG;
        goto out;
    }
    if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
        ret = sched_get_exact_sched_event(node, thread_ctx, event_id, subscribed_event);
    } else {
        ret = sched_get_exact_non_sched_event(thread_ctx, event_id, subscribed_event);
    }

out:
    esched_proc_put(proc_ctx);
    return ret;
}

STATIC int sched_ack_event_proc(struct sched_proc_ctx *proc_ctx,
    struct sched_thread_ctx *thread_ctx, struct sched_ack_event_para *para)
{
    struct sched_event *event = NULL;
    int ret = 0;

    if (ka_base_atomic_read(&thread_ctx->status) != SCHED_THREAD_STATUS_RUN) {
        sched_err("Thread no run. (pid=%d; gid=%u; tid=%u)\n",
            thread_ctx->grp_ctx->gid, thread_ctx->grp_ctx->pid, thread_ctx->tid);
        return DRV_ERROR_THREAD_NOT_RUNNIG;
    }

    if (thread_ctx->grp_ctx->proc_ctx != proc_ctx) {
        sched_err("The chip current running thread is in another pid. "
            "(proc_pid=%d; thread_pid=%d)\n", proc_ctx->pid, thread_ctx->grp_ctx->pid);
        return DRV_ERROR_PROCESS_NOT_MATCH;
    }

    if (thread_ctx->kernel_tid != (u32)SCHED_CURRENT_PID) {
        sched_err("The kernel_tid and pid do not match. (pid=%d; gid=%d; tid=%d; kernel_tid=%d; current_pid=%d)\n",
            thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid, thread_ctx->tid, thread_ctx->kernel_tid, SCHED_CURRENT_PID);
        return DRV_ERROR_RUN_IN_ILLEGAL_CPU;
    }

    ka_task_spin_lock_bh(&thread_ctx->thread_finish_lock);
    event = thread_ctx->event;
    if (event == NULL) {
        ka_task_spin_unlock_bh(&thread_ctx->thread_finish_lock);
        sched_err("Thread no event. (pid=%d; gid=%u; tid=%u)\n",
            thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid, thread_ctx->tid);
        return DRV_ERROR_NO_EVENT;
    }

    if (event->event_ack_func != NULL) {
        ret = event->event_ack_func(proc_ctx->node->node_id, para->subevent_id, para->msg, para->msg_len, event->priv);
        event->event_ack_func = NULL;
    }
    ka_task_spin_unlock_bh(&thread_ctx->thread_finish_lock);

    sched_debug("Debug details. (pid=%d; gid=%u; event_id=%u)\n", event->pid, event->gid, event->event_id);
    if (ret != 0) {
        sched_err("Ack failed. (tid=%u; event_id=%u; subevent_id=%u; ack_event_id=%u; ack_subevent_id=%u)\n",
            thread_ctx->tid, event->event_id, event->subevent_id, para->event_id, para->subevent_id);
    }

    return ret;
}

int32_t sched_ack_event(u32 chip_id, int32_t pid, struct sched_ack_event_para *para)
{
    struct sched_numa_node *node = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    int32_t ret;
    u32 cpuid;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        sched_err("Get proc_ctx failed. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NO_PROCESS;
    }

    if (sched_get_sched_cpuid_in_node(node, &cpuid) == 0) {
        cpu_ctx = sched_get_cpu_ctx(node, cpuid);
        thread_ctx = esched_cpu_cur_thread_get(cpu_ctx);
        if (thread_ctx != NULL) {
            ret = sched_ack_event_proc(proc_ctx, thread_ctx, para);
            esched_cpu_cur_thread_put(thread_ctx);
        } else {
            sched_err("No running thread. (cpuid=%u; pid=%d; task_id=%u; taskgid=%u; tid=%u)\n",
                cpu_ctx->cpuid, cpu_ctx->thread.pid, cpu_ctx->thread.task_id, cpu_ctx->thread.gid, cpu_ctx->thread.tid);
            ret = DRV_ERROR_THREAD_NOT_RUNNIG;
        }
    } else {
        thread_ctx = sched_get_cur_thread_in_proc(proc_ctx);
        if (thread_ctx != NULL) {
            ret = sched_ack_event_proc(proc_ctx, thread_ctx, para);
        } else {
            sched_err("No running thread. (chip_id=%u; pid=%d)\n", chip_id, proc_ctx->pid);
            ret = DRV_ERROR_THREAD_NOT_RUNNIG;
        }
    }

    esched_proc_put(proc_ctx);

    return ret;
}

STATIC_INLINE void sched_wakeup_sched_grp(struct sched_numa_node *node, struct sched_event *event,
    struct sched_grp_ctx *grp_ctx, struct sched_event_thread_map *event_thread_map)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    int32_t old_status;
    u32 i, idx;

    for (i = 0; i < event_thread_map->thread_num; i++) {
        idx = grp_ctx->last_thread + 1U + i;
        if (idx >= event_thread_map->thread_num) {
            idx %= event_thread_map->thread_num;
        }

        thread_ctx = sched_get_thread_ctx(grp_ctx, event_thread_map->thread[idx]);
        if (thread_ctx->timeout_flag == SCHED_VALID) {
            continue;
        }

        cpu_ctx = sched_get_cpu_ctx(node, thread_ctx->bind_cpuid);
#if defined(CFG_FEATURE_VFIO) || (defined(CFG_FEATURE_HARDWARE_MIA) && !defined(ESCHED_HOST))
        if (!sched_is_cpu_belongs_to_vf(node, (u32)grp_ctx->proc_ctx->vfid, cpu_ctx)) {
            continue;
        }
#endif

#ifdef CFG_FEATURE_HARD_SOFT_SCHED
        if (esched_drv_cpu_is_hw_sched_mode(cpu_ctx->topic_chan)) {
            continue;
        }
#endif

        old_status = ka_base_atomic_cmpxchg(&cpu_ctx->cpu_status, CPU_STATUS_IDLE, CPU_STATUS_READY);
        if (old_status != (int)CPU_STATUS_IDLE) {
            continue;
        }

        ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
        /* CPU in idle state, try to wake up */
        if (!esched_is_cpu_idle(cpu_ctx)) {
            ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
            continue;
        }

        thread_ctx = sched_get_next_thread(cpu_ctx);
        if (thread_ctx == NULL) {
            /* The cpu schedule no event, indicating that the event just published has been scheduled by
               another cpu, no need to continue to wake up other cpu */
            ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
            (void)ka_base_atomic_cmpxchg(&cpu_ctx->cpu_status, CPU_STATUS_READY, CPU_STATUS_IDLE);
            return;
        }

        esched_cpu_cur_thread_set(cpu_ctx, thread_ctx);
        thread_ctx->pre_normal_wakeup_reason = thread_ctx->normal_wakeup_reason;
        thread_ctx->normal_wakeup_reason = SCHED_WAKED_BY_PUBLISHER;
        sched_wake_up_thread(thread_ctx);
        grp_ctx->last_thread = idx;

        /* The published event has been scheduled and will no longer wake up subsequent CPUs */
        if (thread_ctx->event == event) {
            /* proc get in sched_get_next_thread, after wakeup, should put proc */
            esched_proc_put(thread_ctx->grp_ctx->proc_ctx);
            ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
            return;
        }

        /* proc get in sched_get_next_thread, after wakeup, should put proc */
        esched_proc_put(thread_ctx->grp_ctx->proc_ctx);

        ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
    }
}

STATIC_INLINE void sched_wakeup_non_sched_grp(struct sched_grp_ctx *grp_ctx,
    struct sched_event_thread_map *event_thread_map)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 i;

    /* find a thread is in idle state to wakeup.
       thread is found from event_thread_map, so the thread can handle the event */
    for (i = 0; i < event_thread_map->thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, event_thread_map->thread[i]);
        if (ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_IDLE) {
            thread_ctx->pre_normal_wakeup_reason = thread_ctx->normal_wakeup_reason;
            thread_ctx->normal_wakeup_reason = SCHED_WAKED_BY_PUBLISHER;
            sched_wake_up_thread(thread_ctx);
            break;
        }
    }
}

int32_t sched_grp_event_num_update(struct sched_grp_ctx *grp_ctx, u32 event_id)
{
    int event_num = ka_base_atomic_inc_return(&grp_ctx->event_num[event_id]);
    if ((u32)event_num > grp_ctx->max_event_num[event_id]) {
        if (event_num > 0) {
            /* When the finish handle is callback in user mode, the thread may send a event to itself. */
            if (((u32)event_num == (grp_ctx->max_event_num[event_id] + 1U)) && (grp_ctx->pid == SCHED_CURRENT_TGID)) {
                struct sched_thread_ctx *thread_ctx = sched_get_cur_thread_in_grp(grp_ctx);
                if (thread_ctx != NULL) {
                    return 0;
                }
            }

            ka_base_atomic_dec(&grp_ctx->event_num[event_id]);
            ka_base_atomic_inc(&grp_ctx->drop_event_num[event_id]);
            return DRV_ERROR_QUEUE_FULL;
        } else {
            sched_warn("Grp event num turned over. (pid=%d; gid=%u; event_id=%u; event_num=%d)\n",
                grp_ctx->pid, grp_ctx->gid, event_id, event_num);
            ka_base_atomic_set(&grp_ctx->event_num[event_id], 0);
        }
    }

    return 0;
}

int32_t sched_publish_event_to_sched_grp(struct sched_event *event, struct sched_grp_ctx *grp_ctx)
{
    struct sched_proc_ctx *proc_ctx = grp_ctx->proc_ctx;
    struct sched_event_thread_map *event_thread_map = NULL;
    struct sched_event_timeout_info *event_timeout_info = NULL;
    struct sched_event_list *event_list = NULL;
    u32 event_pri;

#ifdef CFG_FEATURE_VFIO
    struct sched_vf_ctx *vf_ctx = NULL;
    vf_ctx = sched_get_vf_ctx(proc_ctx->node, (u32)proc_ctx->vfid);
    if (ka_base_atomic_read(&vf_ctx->status) != SCHED_VF_STATUS_NORMAL) {
        sched_err("The status is invalid. (pid=%d; gid=%u; event_id=%u; vfid=%d)\n",
            proc_ctx->pid, grp_ctx->gid, event->event_id, proc_ctx->vfid);
        return DRV_ERROR_NO_SUBSCRIBE_THREAD;
    }
#endif

    event_pri = proc_ctx->event_pri[event->event_id];
    event_timeout_info = &grp_ctx->event_timeout_info[event->event_id];
    if (event_timeout_info->timeout_flag == SCHED_VALID) {
        return DRV_ERROR_SUBSCRIBE_THREAD_TIMEOUT;
    }

    if (!sched_grp_can_handle_event(grp_ctx, event)) {
        sched_debug("There is no subscribe thread. "
                    "(pid=%d; gid=%u; event_id=%u)\n", grp_ctx->pid, grp_ctx->gid, event->event_id);
        return DRV_ERROR_NO_SUBSCRIBE_THREAD;
    }

    if (sched_grp_event_num_update(grp_ctx, event->event_id) != 0) {
#ifndef EMU_ST
        sched_event_drop(proc_ctx->node->node_id, event);
#endif
        sched_debug("Drop event. (pid=%u; gid=%u; event_id=%u)\n", event->pid, event->gid, event->event_id);
        return 0;
    }

    /* Consider the abnormal exit of the process, must count before enque */
    ka_base_atomic_inc(&proc_ctx->publish_event_num);
    ka_base_atomic_inc(&proc_ctx->proc_event_stat[event->event_id].publish_event_num);

    event_list = sched_get_sched_event_list(proc_ctx->node, proc_ctx->pri, event_pri);

    event->wait_event_num_in_que = (int)event_list->cur_num;
#ifdef CFG_FEATURE_VFIO
    /* Limit the number of events in the queue by force ratio of VF */
    if (event_list->slice_cur_event_num[vf_ctx->vfid] >= vf_ctx->que_depth) {
        ka_base_atomic_dec(&proc_ctx->publish_event_num);
        return DRV_ERROR_QUEUE_FULL;
    }
#endif

    /* event may have dequeue, and put thread_map after add tail */
    event_thread_map = sched_get_event_thread_map(grp_ctx, event);
    sched_get_thread_map(event_thread_map);
    if (sched_event_add_tail(event_list, event) == 0) {
        sched_put_thread_map(event_thread_map);
        ka_base_atomic_dec(&proc_ctx->publish_event_num);
        if (!esched_log_limited(SCHED_LOG_LIMIT_EVENT_LIST)) {
            sched_warn("There is no space for event_list. (pid=%d; gid=%u; event_id=%u; subevent_id=%u)\n",
                grp_ctx->pid, grp_ctx->gid, event->event_id, event->subevent_id);
        }
        return DRV_ERROR_QUEUE_FULL;
    }

    ka_base_atomic_inc(&proc_ctx->node->cur_event_num);

#ifdef CFG_FEATURE_VFIO
    ka_base_atomic64_inc(&vf_ctx->stat.cur_event_num);
    ka_base_atomic64_inc(&vf_ctx->stat.publish_event_num);
#endif

    ka_wmb();
    /* event may have been handle by consumer before, be careful */
    event->trace.type = SCHED_MODE_SCHED_CPU;
    event->trace.a = 0;
    event->trace.b = proc_ctx->pri;
    event->trace.c = event_pri;

#ifdef CFG_FEATURE_VFIO
    if (sched_vf_has_free_cpu(vf_ctx)) {
        sched_wakeup_sched_grp(proc_ctx->node, event, grp_ctx, event_thread_map);
    }
#else
    sched_wakeup_sched_grp(proc_ctx->node, event, grp_ctx, event_thread_map);
#endif
    sched_put_thread_map(event_thread_map);

    return 0;
}

int32_t sched_publish_event_to_non_sched_grp(struct sched_event *event, struct sched_grp_ctx *grp_ctx)
{
    struct sched_event_thread_map *event_thread_map = NULL;
    struct sched_event_list *event_list = NULL;
    struct sched_proc_ctx *proc_ctx = grp_ctx->proc_ctx;

    if (!sched_grp_can_handle_event(grp_ctx, event)) {
        sched_warn("There is no subscribe thread. "
                   "(pid=%d; gid=%u; event_id=%u)\n", grp_ctx->pid, grp_ctx->gid, event->event_id);
        return DRV_ERROR_NO_SUBSCRIBE_THREAD;
    }

    if (sched_grp_event_num_update(grp_ctx, event->event_id) != 0) {
        (void)sched_event_enque_lock(event->que, event);
        return 0;
    }

    /* Consider the abnormal exit of the process, must count before enque */
    ka_base_atomic_inc(&proc_ctx->publish_event_num);
    ka_base_atomic_inc(&proc_ctx->proc_event_stat[event->event_id].publish_event_num);
    ka_base_atomic_inc(&grp_ctx->cur_event_num);

    event_list = sched_get_non_sched_event_list(grp_ctx, proc_ctx->event_pri[event->event_id]);
    event->wait_event_num_in_que = event_list->cur_num;

    /* event may have dequeue, and put thread_map after add tail */
    event_thread_map = sched_get_event_thread_map(grp_ctx, event);
    sched_get_thread_map(event_thread_map);
    /* enque failed, free to its event poll */
    if (sched_event_add_tail(event_list, event) == 0) {
        sched_put_thread_map(event_thread_map);
        ka_base_atomic_dec(&proc_ctx->publish_event_num);
        ka_base_atomic_dec(&proc_ctx->proc_event_stat[event->event_id].publish_event_num);
        ka_base_atomic_dec(&grp_ctx->cur_event_num);
        if (!esched_log_limited(SCHED_LOG_LIMIT_EVENT_LIST)) {
            sched_warn("There is no space for event_list. (pid=%d; gid=%u; event_id=%u; subevent_id=%u)\n",
                grp_ctx->pid, grp_ctx->gid, event->event_id, event->subevent_id);
        }
        return DRV_ERROR_QUEUE_FULL;
    }

    ka_wmb();
    /* event may have been handle by consumer before, be careful */
    event->trace.type = SCHED_MODE_NON_SCHED_CPU;
    event->trace.a = (uint32_t)proc_ctx->pid;
    event->trace.b = grp_ctx->gid;
    event->trace.c = proc_ctx->event_pri[event->event_id];

    sched_wakeup_non_sched_grp(grp_ctx, event_thread_map);
    sched_put_thread_map(event_thread_map);
    return 0;
}

struct sched_event *sched_alloc_event(struct sched_numa_node *node)
{
    struct sched_event *event = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 cpuid;

    sched_get_cpuid_in_node(node, &cpuid);
    cpu_ctx = sched_get_cpu_ctx(node, cpuid);
    event = sched_event_deque_lock(&cpu_ctx->event_res);
    /* There are no events in the CPU's event pool. get from the node's event pool. */
    if (event == NULL) {
        event = sched_event_deque_lock(&node->event_res);
    }

    return event;
}

STATIC_INLINE struct sched_event *sched_publish_fill_event(struct sched_numa_node *node,
    struct sched_proc_ctx *proc_ctx, struct sched_published_event_info *event_info,
    struct sched_published_event_func *event_func, u32 event_src)
{
    struct sched_event *event = sched_alloc_event(node);
#ifndef CFG_SOC_PLATFORM_MDC_V51
    int32_t ret;
#endif

    if (event == NULL) {
        return NULL;
    }

    event->publish_cpuid = sched_get_cur_processor_id();
    if ((event_src == SCHED_PUBLISH_FORM_USER) && (event_info->dst_engine == CCPU_DEVICE)) {
        event->publish_pid = SCHED_CURRENT_TGID;
    } else {
        event->publish_pid = -1;
    }
    event->gid = event_info->gid;
    event->pid = event_info->pid;
    event->event_id = event_info->event_id;
    event->subevent_id = event_info->subevent_id;
    event->event_finish_func = event_func->event_finish_func;
    event->event_ack_func = event_func->event_ack_func;
    event->msg_len = event_info->msg_len;
    event->priv = event_info->priv;
    if (event->msg_len > 0) {
        if (memcpy_s(event->msg, SCHED_MAX_EVENT_MSG_LEN, event_info->msg, event_info->msg_len) != 0) {
            /* Need to returned to the event resource que after failure */
            (void)sched_event_enque_lock(event->que, event);
            sched_err("Failed to invoke the memcpy_s. (pid=%d; gid=%u; event_id=%u)\n",
                event_info->pid, event_info->gid, event_info->event_id);
            return NULL;
        }
    }
#ifdef CFG_FEATURE_VFIO
    event->vfid = proc_ctx->vfid;
#endif

    /* Performance tuning, recourd publish time */
    event->timestamp.publish_user = event_info->publish_timestamp;
    event->timestamp.publish_user_of_day = event_info->publish_timestamp_of_day;

    if (event_info->tid != SCHED_INVALID_TID) {
#ifndef CFG_SOC_PLATFORM_MDC_V51
        ret = sched_event_add_thread(event, event_info->tid);
        if (ret != 0) {
            (void)sched_event_enque_lock(event->que, event);
            sched_err("Failed to invoke sched_event_add_thread. (pid=%d; gid=%u; event_id=%u)\n",
                event_info->pid, event_info->gid, event_info->event_id);
            return NULL;
        }
#else
        event->event_thread_map = NULL;
#endif
    } else {
        event->event_thread_map = NULL;
    }

    return event;
}

STATIC_INLINE void sched_publish_perf_stat(struct sched_numa_node *node, struct sched_event *event, u32 sched_mode, int32_t ret)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 cpuid;
    u64 syscall_time;
    u64 in_kernel_time;
    struct sched_cpu_perf_stat *perf_stat = NULL;

    sched_publish_state_update(node, event, ret);

    if (ret != 0) {
        return;
    }

    sched_get_cpuid_in_node(node, &cpuid);
    cpu_ctx = sched_get_cpu_ctx(node, cpuid);
    perf_stat = &cpu_ctx->perf_stat;

    syscall_time = event->timestamp.publish_in_kernel - event->timestamp.publish_user;
    in_kernel_time = event->timestamp.publish_out_kernel - event->timestamp.publish_in_kernel;

    sched_record_abnormal_event(&cpu_ctx->node->abnormal_event.publish_syscall, event, NULL,
        syscall_time, sched_mode);
    sched_record_abnormal_event(&cpu_ctx->node->abnormal_event.publish_in_kernel, event, NULL,
        in_kernel_time, sched_mode);

    perf_stat->publish_syscall_total_time += syscall_time;
    if (syscall_time > perf_stat->publish_syscall_max_time) {
        perf_stat->publish_syscall_max_time = syscall_time;
    }

    perf_stat->publish_in_kernel_total_time += in_kernel_time;
    if (in_kernel_time > perf_stat->publish_in_kernel_max_time) {
        perf_stat->publish_in_kernel_max_time = in_kernel_time;
    }
}

int32_t sched_publish_event(u32 chip_id, u32 event_src,
    struct sched_published_event_info *event_info, struct sched_published_event_func *event_func)
{
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_event *event = NULL;
    int32_t pid = event_info->pid;
    u32 gid = event_info->gid;
    int32_t ret;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
#if !defined (CFG_SOC_PLATFORM_MDC_V51) && !defined (CFG_SOC_PLATFORM_MDC_V11)
        sched_warn("Proc_ctx not find. (chip_id=%u; pid=%d)\n", chip_id, pid);
#endif
        return DRV_ERROR_NO_PROCESS;
    }

#ifdef CFG_FEATURE_VFIO
    ret = sched_publish_check_event_source(event_src, proc_ctx);
    if (ret != 0) {
        goto out;
    }
#endif

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        sched_err("The group is not added. (chip_id=%u; pid=%d; gid=%u)\n",
            proc_ctx->node->node_id, pid, gid);
        ret = DRV_ERROR_UNINIT;
        goto out;
    }

    if ((event_info->tid != SCHED_INVALID_TID) && (event_info->tid >= grp_ctx->cfg_thread_num)) {
        sched_err("The tid out of group thread range. (pid=%d; gid=%u; tid=%u; max=%u)\n",
            pid, gid, event_info->tid, grp_ctx->cfg_thread_num);
        ret = DRV_ERROR_PARA_ERROR;
        goto out;
    }

    event = sched_publish_fill_event(node, proc_ctx, event_info, event_func, event_src);
    if (event == NULL) {
        /* record load inner */
        ret = DRV_ERROR_QUEUE_FULL;
        goto out;
    }

    event->timestamp.publish_in_kernel = sched_get_cur_timestamp();
    event->timestamp.publish_wakeup = 0; /* clear 0 */

    if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
#ifdef CFG_FEATURE_SOFT_NON_SCHED_ONLY
        ret = DRV_ERROR_NOT_SUPPORT;
#else
        ret = sched_publish_event_to_sched_grp(event, grp_ctx);
#endif
    } else {
        ret = sched_publish_event_to_non_sched_grp(event, grp_ctx);
    }

    /* publish failed, return the event to resource que */
    if (ret != 0) {
        (void)sched_event_enque_lock(event->que, event);
    } else {
        event->timestamp.publish_out_kernel = sched_get_cur_timestamp();
    }
    sched_publish_perf_stat(node, event, grp_ctx->sched_mode, ret);

out:
    esched_proc_put(proc_ctx);
    return ret;
}

int esched_submit_event_distribute(u32 chip_id, u32 event_src,
    struct sched_published_event_info *event_info, struct sched_published_event_func *event_func)
{
#ifdef CFG_FEATURE_REMOTE_SUBMIT
    bool local_flag = false;
    u32 dst_engine = event_info->dst_engine;

    local_flag = esched_dst_engine_is_local(dst_engine);

    if (local_flag == false) {
        return sched_publish_event_to_remote(chip_id, event_src, event_info, event_func);
    }
#endif
    return sched_publish_event(chip_id, event_src, event_info, event_func);
}

int32_t sched_trigger_sched_trace_record(u32 chip_id, const char *reason, const char *key)
{
    struct sched_trace_record_info *trace_record = NULL;
    struct sched_numa_node *node = NULL;
    uint32_t sched_record_num = 0;
    int32_t cpy_reason_ret, cpy_key_ret;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    trace_record = &node->trace_record;

    if ((node->debug_flag & (0x1 << SCHED_DEBUG_SCHED_TRACE_RECORD_BIT)) == 0) {
        sched_info("It is disabled. (chip_id=%u; record_reason=\"%s\"; key=\"%s\")\n",
                   node->node_id, trace_record->record_reason, trace_record->key);
        return 0;
    }

    sched_record_num = sched_sysfs_record_num_data();
    if (trace_record->num >= sched_record_num) {
        return 0;
    }

    ka_task_spin_lock_bh(&trace_record->lock);
    if (trace_record->valid == SCHED_VALID) {
        ka_task_spin_unlock_bh(&trace_record->lock);
        sched_info("It is recording now. "
                   "(chip_id=%u; reason=\"%s\"; key=\"%s\"; current_reason=\"%s\"; current_key=\"%s\")\n",
                   chip_id, trace_record->record_reason, trace_record->key, reason, key);
        return 0;
    }
#if !defined(EVENT_SCHED_UT) && !defined(EMU_ST)
    cpy_reason_ret = strncpy_s(trace_record->record_reason, SCHED_STR_MAX_LEN, reason, ka_base_strlen(reason));
    cpy_key_ret = strncpy_s(trace_record->key, SCHED_STR_MAX_LEN, key, ka_base_strlen(key));
    trace_record->record_reason[SCHED_STR_MAX_LEN - 1] = '\0';
    trace_record->key[SCHED_STR_MAX_LEN - 1] = '\0';
    trace_record->num++;
    trace_record->timestamp = sched_get_cur_timestamp();
    trace_record->valid = SCHED_VALID;
    ka_task_spin_unlock_bh(&trace_record->lock);
    sched_debug("Trace record. (chip_id=%u; index=%u; reason=\"%s\"; key=\"%s\")\n",
        chip_id, trace_record->num, trace_record->record_reason, trace_record->key);
    if ((cpy_reason_ret != 0) || (cpy_key_ret != 0)) {
        sched_warn("Unable to strncpy_s. (cpy_reason_ret=%d; cpy_key_ret=%d)\n", cpy_reason_ret, cpy_key_ret);
    }
#endif
    return 0;
}

int32_t sched_set_event_priority(u32 chip_id, int32_t pid, u32 event_id, u32 pri)
{
    struct sched_proc_ctx *proc_ctx = NULL;

    if (pri >= SCHED_MAX_EVENT_PRI_NUM) {
        sched_err("The variable is out of range. (chip_id=%u; pid=%d; pri=%u; max=%d)\n",
                  chip_id, pid, pri, (int)SCHED_MAX_EVENT_PRI_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    if (event_id >= SCHED_MAX_EVENT_TYPE_NUM) {
        sched_err("The variable event_id is out of range. (chip_id=%u; pid=%d; event_id=%u; max=%d)\n", chip_id, pid,
                  event_id, SCHED_MAX_EVENT_TYPE_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to invoke the sched_get_chip_proc_ctx to get proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NO_PROCESS;
    }

    proc_ctx->event_pri[event_id] = pri;
    esched_chip_proc_put(proc_ctx);
    return 0;
}

int32_t sched_set_process_priority(u32 chip_id, int32_t pid, u32 pri)
{
    struct sched_proc_ctx *proc_ctx = NULL;

    if (pri >= SCHED_MAX_PROC_PRI_NUM) {
        sched_err("The variable is out of range. (chip_id=%u; pid=%d; pri=%u; max=%d)\n",
                  chip_id, pid, pri, (int)SCHED_MAX_PROC_PRI_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to invoke the sched_get_chip_proc_ctx to get proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NO_PROCESS;
    }

    proc_ctx->pri = pri;
    esched_chip_proc_put(proc_ctx);
    return 0;
}

STATIC void sched_adjust_event_thread_map(struct sched_grp_ctx *grp_ctx, struct sched_thread_ctx *thread_ctx,
    u32 idx, u64 cur_event_bitmap, u64 event_bitmap)
{
    u32 i, findout;
    struct sched_event_thread_map *event_thread_map = NULL;
    struct sched_event_timeout_info *event_timeout_info = NULL;

    event_thread_map = &grp_ctx->event_thread_map[idx];
    event_timeout_info = &grp_ctx->event_timeout_info[idx];

    /* thread add new event */
    if ((event_bitmap & (0x1ULL << idx)) != 0) {
        event_thread_map->thread[event_thread_map->thread_num] = thread_ctx->tid;
        event_thread_map->thread_num++;
        event_timeout_info->timeout_flag = SCHED_INVALID;
        event_timeout_info->timeout_timestamp = 0;
    } else {
        /* thread delete event */
        findout = SCHED_INVALID;
        for (i = 0; i < event_thread_map->thread_num; i++) {
            /* findout the pos, move the last thread to current pos */
            if (event_thread_map->thread[i] == thread_ctx->tid) {
                event_thread_map->thread_num--;
                event_thread_map->thread[i] = event_thread_map->thread[event_thread_map->thread_num];
                findout = SCHED_VALID;
                break;
            }
        }
    }
}

STATIC void sched_adjust_thread_event(struct sched_grp_ctx *grp_ctx,
    struct sched_thread_ctx *thread_ctx, u64 event_bitmap)
{
    u64 cur_event_bitmap = thread_ctx->subscribe_event_bitmap;
    u32 i;

    if (cur_event_bitmap == event_bitmap) {
        return;
    }

    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        /* subscribed event is diff */
        if ((cur_event_bitmap & (0x1ULL << i)) != (event_bitmap & (0x1ULL << i))) {
            sched_adjust_event_thread_map(grp_ctx, thread_ctx, i, cur_event_bitmap, event_bitmap);
        }
    }
}

STATIC void sched_upgrade_thread_event(struct sched_grp_ctx *grp_ctx, u32 tid)
{
    u32 i;
    u64 event_bitmap;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_event_thread_map *event_thread_map = NULL;
    struct sched_event_timeout_info *event_timeout_info = NULL;

    thread_ctx = sched_get_thread_ctx(grp_ctx, tid);
    event_bitmap = thread_ctx->subscribe_event_bitmap;

    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        if ((event_bitmap & (0x1ULL << i)) != 0) {
            event_thread_map = &grp_ctx->event_thread_map[i];
            event_timeout_info = &grp_ctx->event_timeout_info[i];
            event_thread_map->thread[event_thread_map->thread_num] = tid;
            ka_wmb();
            event_thread_map->thread_num++;
            event_timeout_info->timeout_flag = SCHED_INVALID;
            event_timeout_info->timeout_timestamp = 0;
        }
    }
}

STATIC void sched_thread_subscribe_event_cfg(struct sched_grp_ctx *grp_ctx, int32_t tid, u64 event_bitmap)
{
    struct sched_thread_ctx *thread_ctx = NULL;

    thread_ctx = sched_get_thread_ctx(grp_ctx, (uint32_t)tid);

    ka_task_spin_lock_bh(&grp_ctx->lock);
    if (thread_ctx->valid == SCHED_VALID) {
        sched_adjust_thread_event(grp_ctx, thread_ctx, event_bitmap);
    }
    thread_ctx->subscribe_event_bitmap = event_bitmap;
    ka_task_spin_unlock_bh(&grp_ctx->lock);
}

int32_t sched_thread_subscribe_event(u32 chip_id, int32_t pid, u32 gid, u32 tid, u64 event_bitmap)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;

    if (gid >= SCHED_MAX_GRP_NUM) {
        sched_err("The variable chip_id is out of range. "
                  "(chip_id=%u; pid=%d; gid=%u; max=%d)\n", chip_id, pid, gid, SCHED_MAX_GRP_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    if (event_bitmap == 0) {
        sched_err("The value of variable event_bitmap is invalid. "
                  "(chip_id=%u; pid=%d; gid=%u; event_bitmap=0x%llx)\n", chip_id, pid, gid, event_bitmap);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to invoke the sched_get_chip_proc_ctx to get proc_ctx. (chip_id=%d; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NO_PROCESS;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        sched_err("It's not initialized yet. (pid=%d; gid=%u; tid=%u)\n", pid, grp_ctx->gid, tid);
        esched_chip_proc_put(proc_ctx);
        return DRV_ERROR_NO_GROUP;
    }

    if (tid >= grp_ctx->cfg_thread_num) {
        sched_err("The variable tid is out of range. (chip_id=%u; pid=%d; gid=%u; tid=%u; max=%u)\n",
                  chip_id, pid, gid, tid, grp_ctx->cfg_thread_num);
        esched_chip_proc_put(proc_ctx);
        return DRV_ERROR_PARA_ERROR;
    }

    sched_thread_subscribe_event_cfg(grp_ctx, (int32_t)tid, event_bitmap);
    esched_chip_proc_put(proc_ctx);

    return 0;
}

int32_t sched_grp_set_max_event_num(u32 chip_id, int32_t pid, u32 gid, u32 event_id, u32 max_num)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;

    if ((gid >= SCHED_MAX_GRP_NUM) || (event_id >= SCHED_MAX_EVENT_TYPE_NUM) || (max_num == 0)) {
        sched_err("The variable chip_id is out of range. (chip_id=%u; pid=%d; gid=%u; event_id=%u; max_num=%u)\n",
            chip_id, pid, gid, event_id, max_num);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to invoke the sched_get_chip_proc_ctx to get proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NO_PROCESS;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        sched_err("It's not initialized yet. (pid=%d; gid=%u)\n", pid, grp_ctx->gid);
        esched_chip_proc_put(proc_ctx);
        return DRV_ERROR_NO_GROUP;
    }

    grp_ctx->max_event_num[event_id] = max_num;
    esched_chip_proc_put(proc_ctx);

    return 0;
}

int32_t sched_grp_add_thread(struct sched_grp_ctx *grp_ctx, u32 tid, u32 cpuid)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 bind_cpuid;
    u32 devid = grp_ctx->proc_ctx->node->node_id;

    ka_task_spin_lock_bh(&grp_ctx->lock);

    /* non sched group mode cpu id to 0 */
    bind_cpuid = (grp_ctx->sched_mode == SCHED_MODE_NON_SCHED_CPU) ? 0 : cpuid;

    /* sched group thread exceeds specification */
    if ((grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) &&
        ((grp_ctx->cpuid_to_tid[bind_cpuid] != grp_ctx->cfg_thread_num) ||
        (grp_ctx->thread_num >= grp_ctx->cfg_thread_num))) {
        ka_task_spin_unlock_bh(&grp_ctx->lock);
        sched_err("The schedule group thread exceeds specification. (bind_cpuid=%u; cur_cpuid=%u; pid=%d; gid=%u;"
                  " tid=%u; add_tid=%u; sched_mode=%u; thread_num=%u)\n",
                  bind_cpuid, cpuid, grp_ctx->pid, grp_ctx->gid, grp_ctx->cpuid_to_tid[bind_cpuid], tid,
                  grp_ctx->sched_mode, grp_ctx->thread_num);
        return DRV_ERROR_THREAD_EXCEEDS_SPEC;
    }

    thread_ctx = sched_get_thread_ctx(grp_ctx, tid);
    thread_ctx->tid = tid;

    /* thread in non-sched group has not bound to cpu, bind_cpuid can't used */
    thread_ctx->bind_cpuid = bind_cpuid;
    ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
    ka_task_init_waitqueue_head(&thread_ctx->wq);

    thread_ctx->kernel_tid = SCHED_CURRENT_PID;
    if (strncpy_s(thread_ctx->name, TASK_COMM_LEN, SCHED_CURRENT_COMM, ka_base_strlen(SCHED_CURRENT_COMM)) != 0) {
#ifndef EMU_ST
        sched_warn("Unable to invoke the strncpy_s to copy thread name. (kernel_tid=%u)\n", thread_ctx->kernel_tid);
#endif
    }
    thread_ctx->name[TASK_COMM_LEN - 1] = '\0';

    grp_ctx->cpuid_to_tid[bind_cpuid] = tid;
    ka_wmb();
    sched_upgrade_thread_event(grp_ctx, tid);

    thread_ctx->valid = SCHED_VALID;

    grp_ctx->thread[grp_ctx->thread_num] = tid;
    ka_wmb();
    grp_ctx->thread_num++;

    ka_task_spin_unlock_bh(&grp_ctx->lock);
    sched_debug("Grp add thread success. (devid=%u; cpuid=%u; pid=%d; gid=%u; grp_name=%s; sched_mode=%u; tid=%u;"
        " kernel_tid=%u; subscribe_event_bitmap=%llx)\n", devid, cpuid, grp_ctx->pid, grp_ctx->gid, grp_ctx->name,
        grp_ctx->sched_mode, thread_ctx->tid, thread_ctx->kernel_tid, thread_ctx->subscribe_event_bitmap);
    return 0;
}

/* If one os_tid creates multiple esched tid in the same non sched grp, the earliest created esched tid is returned. */
int sched_query_tid_in_grp(u32 chip_id, int pid, u32 gid, u32 os_tid, u32 *tid)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 i;
    struct sched_numa_node *node = NULL;

    if (gid >= SCHED_MAX_GRP_NUM) {
        sched_err("The variable gid is out of range. (chip_id=%u; pid=%d; gid=%u; max=%d)\n",
            chip_id, pid, gid, SCHED_MAX_GRP_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    if (tid == NULL) {
        sched_err("The variable tid is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, pid);
        esched_dev_put(node);
        return DRV_ERROR_NO_PROCESS;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    ka_task_spin_lock_bh(&grp_ctx->lock);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        ka_task_spin_unlock_bh(&grp_ctx->lock);
        sched_err("Grp not init. (chip_id=%u; pid=%d; gid=%u)\n", chip_id, pid, gid);
        esched_chip_proc_put(proc_ctx);
        esched_dev_put(node);
        return DRV_ERROR_NO_GROUP;
    }

    for (i = 0; i < grp_ctx->thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->thread[i]);
        if (thread_ctx->kernel_tid == os_tid) {
            *tid = grp_ctx->thread[i];
            ka_task_spin_unlock_bh(&grp_ctx->lock);
            esched_chip_proc_put(proc_ctx);
            esched_dev_put(node);
            return 0;
        }
    }

    ka_task_spin_unlock_bh(&grp_ctx->lock);
    esched_chip_proc_put(proc_ctx);
    esched_dev_put(node);
    sched_err("Failed to find tid. (chip_id=%u; pid=%d; gid=%u; os_tid=%u)\n", chip_id, pid, gid, os_tid);
    return DRV_ERROR_UNINIT;
}
KA_EXPORT_SYMBOL_GPL(sched_query_tid_in_grp);

int sched_query_local_task_gid(unsigned int chip_id, int pid, const char *grp_name, unsigned int *gid)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    int ret = DRV_ERROR_UNINIT;
    u32 i;
    struct sched_numa_node *node = NULL;

    if ((grp_name == NULL) || (gid == NULL)) {
#if !defined(EVENT_SCHED_UT) && !defined(EMU_ST)
        sched_err("The variable is NULL. (grp_name=%u; gid=%u)\n", (grp_name == NULL ? 0 : 1), (gid == NULL ? 0 : 1));
        return DRV_ERROR_PARA_ERROR;
#endif
    }

    if (ka_base_strnlen(grp_name, EVENT_MAX_GRP_NAME_LEN) >= EVENT_MAX_GRP_NAME_LEN) {
        sched_err("grp_name len out of range. (length=%lu)\n", ka_base_strnlen(grp_name, EVENT_MAX_GRP_NAME_LEN));
        return DRV_ERROR_PARA_ERROR;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    proc_ctx = esched_chip_proc_get((u32)chip_id, pid);
    if (proc_ctx == NULL) {
        sched_warn("get proc_ctx not added. (chip_id=%u; pid=%d)\n", chip_id, pid);
        esched_dev_put(node);
        return DRV_ERROR_NO_PROCESS;
    }

    for (i = SCHED_MAX_DEFAULT_GRP_NUM; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, i);
        if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
            break;
        }

        if (ka_base_strcmp(grp_ctx->name, grp_name) == 0) {
            *gid = (unsigned int)i;
            ret = DRV_ERROR_NONE;
            break;
        }
    }

    esched_chip_proc_put(proc_ctx);
    esched_dev_put(node);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(sched_query_local_task_gid);

int sched_query_local_trace(unsigned int chip_id,
    int pid, unsigned int gid, unsigned int tid, struct sched_sync_event_trace *sched_trace)
{
    int ret;
    int dfx_gid = gid - SCHED_MAX_DEFAULT_GRP_NUM;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_numa_node *node = NULL;

    if ((sched_trace == NULL) || (tid >= SCHED_MAX_SYNC_THREAD_NUM_PER_GRP) ||
        (dfx_gid >= SCHED_MAX_EX_GRP_NUM) || (dfx_gid < 0)) {
        sched_err("The variable is NULL. (pid=%d; gid=%u, tid=%u)\n", pid, gid, tid);
        return DRV_ERROR_PARA_ERROR;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_warn("get proc_ctx not added. (chip_id=%u; pid=%d)\n", chip_id, pid);
        esched_dev_put(node);
        return DRV_ERROR_NO_PROCESS;
    }

    ret = memcpy_s(sched_trace, sizeof(struct sched_sync_event_trace),
                   &proc_ctx->sched_dfx[dfx_gid][tid], sizeof(struct sched_sync_event_trace));
    if (ret != 0) {
        sched_err("Memcpy failed. (chip_id=%u; pid=%d; ret=%d)\n", chip_id, pid, ret);
        esched_chip_proc_put(proc_ctx);
        esched_dev_put(node);
        return DRV_ERROR_INNER_ERR;
    }
    sched_query_thread_run_time(chip_id);
    sched_err_printk_ratelimited("trace query. (pid=%u; gid=%u; tid=%u; event_id=%u; subevent_id=%u; curr_tick=%llu)\n",
        pid, gid, tid, sched_trace->event_id, sched_trace->subevent_id, sched_get_cur_timestamp());

    esched_chip_proc_put(proc_ctx);
    esched_dev_put(node);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(sched_query_local_trace);

int sched_query_sync_event_trace(unsigned int chip_id,
    unsigned int dev_pid, unsigned int gid, unsigned int tid, struct sched_sync_event_trace *trace_result)
{
    int32_t ret;
    int local_pid = SCHED_CURRENT_TGID;
#ifdef CFG_FEATURE_REMOTE_SUBMIT
#ifdef ESCHED_HOST
    struct sched_sync_event_trace remote_trace;
#endif
#endif
    if (trace_result == NULL) {
        sched_err("The variable is NULL. (chip_id=%u; pid=%d, trace_id=%u)\n", chip_id, local_pid, tid);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = sched_query_local_trace(chip_id, local_pid, gid, tid, trace_result);
    if (ret != 0) {
        sched_err("query sync event trace fail. (chip_id=%u; pid=%d; trace_id=%u)\n", chip_id, local_pid, tid);
        return DRV_ERROR_PARA_ERROR;
    }
#ifdef CFG_FEATURE_REMOTE_SUBMIT
#ifdef ESCHED_HOST
#ifndef EMU_ST
    ret = sched_query_remote_trace_msg_send(chip_id, dev_pid, gid, tid, &remote_trace);
    if (ret != 0) {
        sched_err_printk_ratelimited("query remote event trace fail. (ret=%d, chip_id=%u; pid=%d; trace_id=%u)\n",
            ret, chip_id, dev_pid, tid);
        return 0;
    }
    if (trace_result->subevent_id == remote_trace.subevent_id) {
        trace_result->dst_publish_timestamp = remote_trace.dst_publish_timestamp;
        trace_result->dst_wait_start_timestamp = remote_trace.dst_wait_start_timestamp;
        trace_result->dst_wait_end_timestamp = remote_trace.dst_wait_end_timestamp;
        trace_result->dst_submit_user_timestamp = remote_trace.dst_submit_user_timestamp;
        trace_result->dst_submit_kernel_timestamp = remote_trace.dst_submit_kernel_timestamp;
    } else {
        sched_err_printk_ratelimited("remote trace query fail. (chip_id=%u; pid=%d; tid=%u; local_subevent=%d;remote_subevent=%d)\n",
            chip_id, dev_pid, tid, trace_result->subevent_id, remote_trace.subevent_id);
    }
#endif
#endif
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(sched_query_sync_event_trace);

#ifdef CFG_FEATURE_REMOTE_SUBMIT
int sched_query_remote_task_gid(u32 chip_id, int pid, const char *grp_name, u32 *gid)
{
    if ((grp_name == NULL) || (gid == NULL)) {
#if !defined(EVENT_SCHED_UT) && !defined(EMU_ST)
        sched_err("The variable is NULL. (grp_name=%u; gid=%u)\n", (grp_name == NULL ? 0 : 1), (gid == NULL ? 0 : 1));
        return DRV_ERROR_PARA_ERROR;
#endif
    }

    return sched_query_remote_task_gid_msg_send(chip_id, pid, grp_name, gid);
}
KA_EXPORT_SYMBOL_GPL(sched_query_remote_task_gid);
#endif


STATIC void sched_event_list_init(struct sched_event_list *event_list, int32_t depth)
{
    ka_task_spin_lock_init(&event_list->lock);
    KA_INIT_LIST_HEAD(&event_list->head);
    event_list->cur_num = 0;
    event_list->sched_num = 0;
    event_list->total_num = 0;
    event_list->depth = (u32)depth;
#ifdef CFG_FEATURE_VFIO
    sched_event_list_vf_init(event_list);
#endif
}

STATIC void sched_node_event_list_init(struct sched_numa_node *node)
{
    int32_t i, j;

    for (i = 0; i < SCHED_MAX_PROC_PRI_NUM; i++) {
        for (j = 0; j < SCHED_MAX_EVENT_PRI_NUM; j++) {
            sched_event_list_init(&node->published_event_list[i][j], SCHED_PUBLISH_EVENT_QUE_DEPTH);
        }
    }
}

STATIC void sched_group_event_list_init(struct sched_grp_ctx *grp_ctx)
{
    int32_t i;

    for (i = 0; i < SCHED_MAX_EVENT_PRI_NUM; i++) {
        sched_event_list_init(&grp_ctx->published_event_list[i], SCHED_GROUP_EVENT_LIST_DEPTH);
    }
}

static void sched_grp_event_num_init(struct sched_grp_ctx *grp_ctx)
{
    int i;

    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        grp_ctx->max_event_num[i] = KA_UINT_MAX;
        ka_base_atomic_set(&grp_ctx->event_num[i], 0);
        ka_base_atomic_set(&grp_ctx->drop_event_num[i], 0);
    }
}

STATIC int32_t sched_init_grp_thread(struct sched_grp_ctx *grp_ctx, u32 thread_num,
    struct sched_thread_ctx *grp_thread_ctx_base)
{
    struct sched_numa_node *node = grp_ctx->proc_ctx->node;
    u32 i, j;

    grp_ctx->cpuid_to_tid = (u32 *)sched_kzalloc(sizeof(u32) * node->cpu_num, KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (grp_ctx->cpuid_to_tid == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    grp_ctx->thread = (u32 *)sched_kzalloc(sizeof(u32) * thread_num, KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (grp_ctx->thread == NULL) {
        sched_kfree(grp_ctx->cpuid_to_tid);
        grp_ctx->cpuid_to_tid = NULL;
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    grp_ctx->thread_ctx = grp_thread_ctx_base;

    for (i = 0; i < (u32)SCHED_MAX_EVENT_TYPE_NUM; i++) {
#ifndef CFG_SOC_PLATFORM_MDC_V51
        ka_base_kref_init(&grp_ctx->event_thread_map[i].ref);
#endif
        grp_ctx->event_thread_map[i].thread = (u32 *)sched_kzalloc(sizeof(u32) * thread_num,
            KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
        if (grp_ctx->event_thread_map[i].thread == NULL) {
            grp_ctx->thread_ctx = NULL;
            sched_kfree(grp_ctx->thread);
            grp_ctx->thread = NULL;
            sched_kfree(grp_ctx->cpuid_to_tid);
            grp_ctx->cpuid_to_tid = NULL;
            for (j = 0; j < i; j++) {
                sched_kfree(grp_ctx->event_thread_map[j].thread);
                grp_ctx->event_thread_map[j].thread = NULL;
            }
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    /* set cpuid to tid invalid */
    for (i = 0; i < node->cpu_num; i++) {
        grp_ctx->cpuid_to_tid[i] = thread_num;
    }

    for (i = 0; i < thread_num; i++) {
        ka_task_mutex_init(&grp_ctx->thread_ctx[i].thread_mutex);
        grp_ctx->thread_ctx[i].grp_ctx = grp_ctx;
    }

    return 0;
}

int32_t sched_proc_add_grp(u32 chip_id, u32 gid, const char *grp_name, u32 sched_mode, u32 thread_num)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_thread_ctx *grp_thread_ctx_base = NULL;
    int32_t pid = SCHED_CURRENT_TGID;
    int32_t ret;

    if ((gid >= SCHED_MAX_GRP_NUM) || (thread_num == 0) || (thread_num > SCHED_MAX_THREAD_NUM_IN_GRP)) {
        sched_err("Invalid para. (chip_id=%u; pid=%d; gid=%u; thread_num=%u)\n", chip_id, pid, gid, thread_num);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((grp_name == NULL) || (ka_base_strnlen(grp_name, EVENT_MAX_GRP_NAME_LEN) >= EVENT_MAX_GRP_NAME_LEN)) {
#ifndef EMU_ST
        sched_err("The variable grp_name is invalid.\n");
#endif
        return DRV_ERROR_PARA_ERROR;
    }

    if ((sched_mode != GRP_TYPE_BIND_DP_CPU) &&
        (sched_mode != GRP_TYPE_BIND_CP_CPU) &&
        (sched_mode != GRP_TYPE_BIND_DP_CPU_EXCLUSIVE)) {
        sched_err("The sched_mode is invalid. (chip_id=%u; pid=%d; gid=%u; sched_mode=%u)\n",
            chip_id, pid, gid, sched_mode);
        return DRV_ERROR_PARA_ERROR;
    }

    proc_ctx = esched_chip_proc_get(chip_id, pid);
    if (proc_ctx == NULL) {
        sched_err("Failed to invoke the sched_get_chip_proc_ctx to get proc_ctx. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NO_PROCESS;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);

    grp_thread_ctx_base = (struct sched_thread_ctx *)sched_ka_vmalloc(sizeof(struct sched_thread_ctx) * thread_num,
        KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_ACCOUNT | ESCHED_GFP_HIGHMEM, KA_PAGE_KERNEL);
    if (grp_thread_ctx_base == NULL) {
        sched_err("Failed to vzalloc memory for thread_ctx array. (size=0x%lx)\n",
            sizeof(struct sched_thread_ctx) * thread_num);
        esched_chip_proc_put(proc_ctx);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ka_task_spin_lock_bh(&grp_ctx->lock);

    if (grp_ctx->sched_mode != SCHED_MODE_UNINIT) {
        ka_task_spin_unlock_bh(&grp_ctx->lock);
        sched_vfree(grp_thread_ctx_base);
        sched_warn("The schedule mode has been configured. (chip_id=%u; pid=%d; gid=%u; sched_mode=%u)\n",
            chip_id, pid, gid, grp_ctx->sched_mode);
        esched_chip_proc_put(proc_ctx);
        return DRV_ERROR_GROUP_EXIST;
    }

    ret = strcpy_s(grp_ctx->name, EVENT_MAX_GRP_NAME_LEN, grp_name);
    if (ret != 0) {
        ka_task_spin_unlock_bh(&grp_ctx->lock);
        sched_vfree(grp_thread_ctx_base);
        sched_err("Failed to invoke strcpy_s. (chip_id=%u; pid=%d; gid=%u; ret=%d)\n", chip_id, pid, gid, ret);
        esched_chip_proc_put(proc_ctx);
        return DRV_ERROR_INNER_ERR;
    }

    ret = sched_init_grp_thread(grp_ctx, thread_num, grp_thread_ctx_base);
    if (ret != 0) {
        ka_task_spin_unlock_bh(&grp_ctx->lock);
        sched_vfree(grp_thread_ctx_base);
        sched_err("Failed to invoke sched_init_grp_thread. (chip_id=%u; pid=%d; gid=%u; max=%d; thread_num=%u)\n",
                  chip_id, pid, gid, SCHED_MAX_GRP_NUM, thread_num);
        esched_chip_proc_put(proc_ctx);
        return ret;
    }

    grp_ctx->cfg_thread_num = thread_num;

    if (sched_mode == GRP_TYPE_BIND_DP_CPU) {
        grp_ctx->sched_mode = SCHED_MODE_SCHED_CPU;
        grp_ctx->is_exclusive = SCHED_INVALID;
    } else if (sched_mode == GRP_TYPE_BIND_DP_CPU_EXCLUSIVE) {
        grp_ctx->sched_mode = SCHED_MODE_SCHED_CPU;
        grp_ctx->is_exclusive = SCHED_VALID;
    } else {
        sched_group_event_list_init(grp_ctx);
        ka_base_atomic_set(&grp_ctx->cur_event_num, 0);

        grp_ctx->sched_mode = SCHED_MODE_NON_SCHED_CPU;
        grp_ctx->is_exclusive = SCHED_INVALID;
    }

    sched_grp_event_num_init(grp_ctx);

    ka_task_spin_unlock_bh(&grp_ctx->lock);
    esched_chip_proc_put(proc_ctx);

    sched_debug("Add success. (chip_id=%u; pid=%d; gid=%u; sched_mode=%u)\n", chip_id, pid, gid, grp_ctx->sched_mode);

    return 0;
}

STATIC void sched_proc_del_grp(struct sched_proc_ctx *proc_ctx, u32 gid)
{
    struct sched_grp_ctx *grp_ctx = NULL;
    int i;

    grp_ctx = sched_get_grp_ctx(proc_ctx, gid);
    if (grp_ctx->sched_mode != SCHED_MODE_UNINIT) {
        sched_kfree(grp_ctx->thread);
        grp_ctx->thread = NULL;
        sched_vfree(grp_ctx->thread_ctx);
        grp_ctx->thread_ctx = NULL;
        for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
            sched_kfree(grp_ctx->event_thread_map[i].thread);
            grp_ctx->event_thread_map[i].thread = NULL;
        }
        sched_kfree(grp_ctx->cpuid_to_tid);
        grp_ctx->cpuid_to_tid = NULL;
        grp_ctx->sched_mode = SCHED_MODE_UNINIT;
    }
}

/* NOTICE : esched_dev_put needs to be called in advance to ensure the node is online. */
STATIC void sched_proc_release_work(ka_work_struct_t *p_work)
{
    struct sched_proc_ctx *proc_ctx = ka_container_of(p_work, struct sched_proc_ctx, release_work);
    struct sched_numa_node *node = proc_ctx->node;

    sched_free_process(proc_ctx);
    esched_dev_put(node);
}

STATIC void sched_proc_ctx_init(struct sched_proc_ctx *proc_ctx, int32_t pid)
{
    u32 i;
    struct sched_grp_ctx *grp_ctx = NULL;

    ka_base_atomic_set(&proc_ctx->refcnt, 0);
    proc_ctx->pid = pid;
    proc_ctx->pri = SCHED_DEFAULT_PROC_PRI;
    proc_ctx->start_timestamp = sched_get_cur_timestamp();
    proc_ctx->status = SCHED_VALID;
#ifndef AOS_LLVM_BUILD
    proc_ctx->mnt_ns = ka_task_get_current_mnt_ns();
#endif

    if (strncpy_s(proc_ctx->name, TASK_COMM_LEN, SCHED_CURRENT_COMM, ka_base_strlen(SCHED_CURRENT_COMM)) != 0) {
#ifndef EMU_ST
        sched_warn("Unable to invoke the strncpy_s to copy process name. (pid=%d)\n", pid);
#endif
    }
    proc_ctx->name[TASK_COMM_LEN - 1] = '\0';

    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        proc_ctx->event_pri[i] = SCHED_DEFAULT_EVENT_PRI;
    }

    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, i);
        ka_task_spin_lock_init(&grp_ctx->lock);
        grp_ctx->proc_ctx = proc_ctx;
        grp_ctx->pid = pid;
        grp_ctx->gid = i;
    }

    KA_TASK_INIT_WORK(&proc_ctx->release_work, sched_proc_release_work);
}

static void sched_add_pid_to_node(struct sched_numa_node *node, int pid)
{
    struct pid_entry *entry = NULL;

    entry = (struct pid_entry *)sched_kzalloc(sizeof(struct pid_entry), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (entry == NULL) {
        sched_err("Failed to kzalloc memory. (size=0x%lx)\n", sizeof(struct pid_entry));
        return;
    }

    entry->pid = pid;
    ka_task_mutex_lock(&node->pid_list_mutex);
    ka_list_add_tail(&entry->list, &node->pid_list);
    ka_task_mutex_unlock(&node->pid_list_mutex);
}

static void sched_del_pid_from_node(struct sched_numa_node *node, int pid)
{
    struct pid_entry *entry = NULL;
    struct pid_entry *tmp = NULL;

    ka_task_mutex_lock(&node->pid_list_mutex);
    ka_list_for_each_entry_safe(entry, tmp, &node->pid_list, list) {
        if (entry->pid == pid) {
            ka_list_del(&entry->list);
            sched_kfree(entry);
            break;
        }
    }
    ka_task_mutex_unlock(&node->pid_list_mutex);
}

static void sched_add_proc_to_hash_table(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    u32 bucket_index = ka_hash_min(proc_ctx->pid, SCHED_PROC_HASH_TABLE_BIT);

    /* Must increase refcnt before hash_add, to make sure the refcnt of proc_ctx in the hash table cannot be 0. */
    ka_base_atomic_inc(&proc_ctx->refcnt);
    ka_task_write_lock_bh(&node->proc_hash_table_rwlock[bucket_index]);
    ka_hash_add(node->proc_hash_table, &proc_ctx->link, proc_ctx->pid);
    ka_task_write_unlock_bh(&node->proc_hash_table_rwlock[bucket_index]);
}

static void sched_del_proc_from_hash_table(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    u32 bucket_index = ka_hash_min(proc_ctx->pid, SCHED_PROC_HASH_TABLE_BIT);

    ka_task_write_lock_bh(&node->proc_hash_table_rwlock[bucket_index]);
    ka_hash_del(&proc_ctx->link);
    ka_task_write_unlock_bh(&node->proc_hash_table_rwlock[bucket_index]);
    /* Must decrease refcnt after hash_del, to make sure the refcnt of proc_ctx in the hash table cannot be 0. */
    ka_base_atomic_dec(&proc_ctx->refcnt);
}

int32_t sched_add_process(u32 chip_id, int32_t pid)
{
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;
    int32_t ret;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&node->proc_mng_mutex);

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx != NULL) {
        esched_proc_put(proc_ctx);
        ka_task_mutex_unlock(&node->proc_mng_mutex);
        sched_warn("Repeatedly add. (chip_id=%u; pid=%d)\n", chip_id, pid);
        return DRV_ERROR_PROCESS_REPEAT_ADD;
    }

    proc_ctx = (struct sched_proc_ctx *)sched_ka_vmalloc(sizeof(struct sched_proc_ctx),
        KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_ACCOUNT | ESCHED_GFP_HIGHMEM, KA_PAGE_KERNEL);
    if (proc_ctx == NULL) {
        ka_task_mutex_unlock(&node->proc_mng_mutex);
        sched_err("Failed to kzalloc memory for variable proc_ctx. (size=0x%lx)\n", sizeof(struct sched_proc_ctx));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    sched_proc_ctx_init(proc_ctx, pid);
    proc_ctx->node = node;

#ifdef CFG_FEATURE_VFIO
    ret = sched_vf_proc_ctx_init(node, proc_ctx);
    if (ret != 0) {
        ka_task_mutex_unlock(&node->proc_mng_mutex);
        sched_vfree(proc_ctx);
        proc_ctx = NULL;
        return ret;
    }

    sched_vf_proc_ctx_num_inc(node, proc_ctx);
#endif

#ifndef CFG_SOC_PLATFORM_CLOUD_V3
    if (node->ops.map_host_pid != NULL) {
        (void)node->ops.map_host_pid(proc_ctx);
    }
#endif

    (void)ret;
    sched_add_pid_to_node(node, proc_ctx->pid);
    sched_add_proc_to_hash_table(node, proc_ctx);

    proc_ctx->task_id = node->cur_task_id++;

    sched_debug("Add proc success. (chip_id=%u; pid=%d; vfid=%d)\n", chip_id, pid, proc_ctx->vfid);
    ka_task_mutex_unlock(&node->proc_mng_mutex);

    return 0;
}

void sched_wakeup_process_all_thread(struct sched_proc_ctx *proc_ctx)
{
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 i, j;

    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, i);
        /* group has not been init */
        if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
            continue;
        }

        for (j = 0; j < grp_ctx->thread_num; j++) {
            /* allready set proc status to invalid, no need set thread status when wakeup */
            thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->thread[j]);
            ka_task_wake_up_interruptible(&thread_ctx->wq);
        }
    }
}

STATIC void sched_check_cpu_current_thread(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 i;

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        thread_ctx = esched_get_proc_thread_on_cpu(proc_ctx, cpu_ctx);
        if (thread_ctx == NULL) {
            continue;
        }

        /* take lock chech again */
        ka_task_mutex_lock(&thread_ctx->thread_mutex);

        if (esched_is_cpu_cur_thread(cpu_ctx, thread_ctx)) {
            /*
             * The last time the CPU awakens is the thread in the process to be exited. Because the process exits,it
             * will not be called into the kernel again. The remaining work of the wait event needs to be completed
             */
            ka_task_spin_lock_bh(&thread_ctx->thread_finish_lock);
            /* sched_thread_finish maybe sched a next event and cpu cur thread maybe change. */
            sched_thread_finish(thread_ctx, SCHED_TASK_FINISH_SCENE_PROC_EXIT);
            ka_task_spin_unlock_bh(&thread_ctx->thread_finish_lock);
#ifdef CFG_FEATURE_VFIO
            sched_release_cpu_res(thread_ctx);
#endif

            /* Force the timeout thread to idle state then to schedule the next thread */
            ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
            ka_wmb();
            /* Set cpu idle only if there is no event refresh cpu cur thread. */
            ka_task_spin_lock_bh(&cpu_ctx->sched_lock);
            if (esched_is_cpu_cur_thread(cpu_ctx, thread_ctx)) {
                esched_cpu_idle(cpu_ctx);
            }
            ka_task_spin_unlock_bh(&cpu_ctx->sched_lock);
            sched_wake_up_cpu_task(cpu_ctx, SCHED_WAKED_BY_DEL_CHECK);
        }

        ka_task_mutex_unlock(&thread_ctx->thread_mutex);
    }
}

void sched_task_exit_check_sched_cpu(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    sched_check_cpu_current_thread(node, proc_ctx);

    /* wake up all thread, let it return to use space */
    sched_wakeup_process_all_thread(proc_ctx);
}

STATIC void sched_clear_non_sched_event_list(struct sched_grp_ctx *grp_ctx, struct sched_event_list *event_list)
{
    struct sched_event *event = NULL, *tmp = NULL;
    u32 recycle_num = 0;

    ka_task_spin_lock_bh(&event_list->lock);
    ka_list_for_each_entry_safe(event, tmp, &event_list->head, list) {
        if (event->event_finish_func != NULL) {
            struct sched_event_func_info finish_info = {grp_ctx->proc_ctx->node->node_id,
                event->subevent_id, event->msg, event->msg_len};
            event->event_finish_func(&finish_info, SCHED_TASK_FINISH_SCENE_NORMAL, event->priv);
        }

        ka_list_del(&event->list);
        event_list->cur_num--;
        event_list->sched_num++;
        ka_base_atomic_dec(&grp_ctx->cur_event_num);
        ka_base_atomic_inc(&grp_ctx->proc_ctx->sched_event_num);
        ka_base_atomic_inc(&grp_ctx->proc_ctx->proc_event_stat[event->event_id].sched_event_num);
        (void)sched_event_enque_lock(event->que, event);
        recycle_num++;
    }

    ka_task_spin_unlock_bh(&event_list->lock);
    if (recycle_num > 0) {
        sched_info("The recycle_num is bigger than zero. (pid=%d; gid=%u; recycle_num=%u)\n",
            grp_ctx->pid, grp_ctx->gid, recycle_num);
    }
}

STATIC void sched_clear_cur_non_sched_event(struct sched_proc_ctx *proc_ctx)
{
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 i, j;

    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, i);
        if ((grp_ctx->sched_mode != SCHED_MODE_NON_SCHED_CPU) || (grp_ctx->thread_num == 0)) {
            continue;
        }

        /* finish cur thread */
        for (j = 0; j < grp_ctx->thread_num; j++) {
            thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->thread[j]);
            if (ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_IDLE) {
                continue;
            }

            /* thread status has been set to ready, but it not wait event again, so thread event is null */
            if (thread_ctx->event != NULL) {
                sched_thread_finish(thread_ctx, SCHED_TASK_FINISH_SCENE_NORMAL_PROCESS_FREE);
            }
            ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
        }

        /* clear the residual events in list */
        if (ka_base_atomic_read(&grp_ctx->cur_event_num) == 0) {
            continue;
        }

        for (j = 0; j < SCHED_MAX_EVENT_PRI_NUM; j++) {
            sched_clear_non_sched_event_list(grp_ctx, sched_get_non_sched_event_list(grp_ctx, j));
        }
    }
}

void sched_free_process(struct sched_proc_ctx *proc_ctx)
{
    struct sched_numa_node *node = proc_ctx->node;
    u32 i;

    ka_task_mutex_lock(&proc_ctx->node->proc_mng_mutex);
    ka_list_del(&proc_ctx->list);
    ka_task_mutex_unlock(&proc_ctx->node->proc_mng_mutex);

    node->ops.task_exit_check_sched_cpu(node, proc_ctx);

    sched_clear_cur_non_sched_event(proc_ctx);

    sched_debug("Free proc. (chip_id=%u; pid=%d; publish_event=%d; sched_event=%d; "
        "cur_timestamp=%llu; alive_time=%llu)\n", proc_ctx->node->node_id, proc_ctx->pid,
        ka_base_atomic_read(&proc_ctx->publish_event_num), ka_base_atomic_read(&proc_ctx->sched_event_num),
        sched_get_cur_timestamp(), millisecond_to_tick(proc_ctx->exit_timestamp - proc_ctx->start_timestamp));
    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        sched_proc_del_grp(proc_ctx, i);
    }

    sched_vfree(proc_ctx);
}

int32_t sched_del_process(u32 chip_id, int32_t pid)
{
    struct sched_numa_node *node = NULL;
    struct sched_proc_ctx *proc_ctx = NULL;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ka_task_mutex_lock(&node->proc_mng_mutex);

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        ka_task_mutex_unlock(&node->proc_mng_mutex);
        return DRV_ERROR_NOT_EXIST;
    }

    sched_debug("Del proc start. (chip_id=%u; pid=%d; publish_event=%d; sched_event=%d; cur_timestamp=%llu)\n",
               chip_id, pid, ka_base_atomic_read(&proc_ctx->publish_event_num),
               ka_base_atomic_read(&proc_ctx->sched_event_num), sched_get_cur_timestamp());
    sched_sysfs_show_proc_info(chip_id, pid);

    proc_ctx->status = SCHED_INVALID;

    sched_del_proc_from_hash_table(node, proc_ctx);
    sched_del_pid_from_node(node, pid);

    ka_list_add_tail(&proc_ctx->list, &node->del_proc_head);

    ka_task_mutex_unlock(&node->proc_mng_mutex);

#ifdef CFG_FEATURE_VFIO
    sched_vf_proc_ctx_num_dec(node, proc_ctx);
#endif

    proc_ctx->exit_timestamp = sched_get_cur_timestamp();

    node->ops.task_exit_check_sched_cpu(node, proc_ctx);

#ifndef CFG_SOC_PLATFORM_CLOUD_V3
    if (node->ops.unmap_host_pid != NULL) {
        node->ops.unmap_host_pid(proc_ctx);
    }
#endif
    /* stop recoreding node profiling data */
    if (node->sample_proc_id == pid) {
        node->sample_proc_id = 0;
    }

    sched_debug("Del proc End. (chip_id=%u; pid=%d; publish_event=%d; sched_event=%d; "
               "cur_timestamp=%llu; alive_time=%llu)\n",
               chip_id, pid, ka_base_atomic_read(&proc_ctx->publish_event_num),
               ka_base_atomic_read(&proc_ctx->sched_event_num), sched_get_cur_timestamp(),
               millisecond_to_tick(proc_ctx->exit_timestamp - proc_ctx->start_timestamp));

    esched_proc_put(proc_ctx);
    return 0;
}

STATIC void sched_cpu_sample_process(struct sched_numa_node *node)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_cpu_sample *sample = NULL;
    struct sched_thread_ctx *cur_thread = NULL;
    u32 i, idx;

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        sample = cpu_ctx->sample;
        if (sample == NULL) {
            continue;
        }

        idx = sample->record_num++;
        sample->data[idx].timestamp = sched_get_cur_timestamp();
        sample->data[idx].total_use_time = cpu_ctx->perf_stat.total_use_time;

        cur_thread = esched_cpu_cur_thread_get(cpu_ctx);
        if (cur_thread != NULL) {
            sample->data[idx].total_use_time += ((sample->data[idx].timestamp > cur_thread->start_time) ?
                (sample->data[idx].timestamp - cur_thread->start_time) : 0);
            esched_cpu_cur_thread_put(cur_thread);
        }
    }
}

STATIC void sched_proc_event_sample_process(struct sched_numa_node *node)
{
    struct sched_event_sample *sample = node->proc_event_sample;
    struct sched_proc_ctx *proc_stat = NULL;
    u32 i, idx;

    proc_stat = esched_proc_get(node, node->sample_proc_id);
    if (proc_stat != NULL) {
        idx = sample->record_num++;
        sample->data[idx].timestamp = sched_get_cur_timestamp();
        sample->data[idx].total_event_num = ka_base_atomic_read(&proc_stat->publish_event_num);
        sample->data[idx].cur_event_num = ka_base_atomic_read(&proc_stat->publish_event_num) -
            ka_base_atomic_read(&proc_stat->sched_event_num);
        for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
            sample->data[idx].publish_event_num[i] =
                ka_base_atomic_read(&proc_stat->proc_event_stat[i].publish_event_num);
            sample->data[idx].sched_event_num[i] =
                ka_base_atomic_read(&proc_stat->proc_event_stat[i].sched_event_num);
        }
        esched_proc_put(proc_stat);
    }
}

STATIC void sched_node_event_sample_process(struct sched_numa_node *node)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_event_sample *sample = node->node_event_sample;
    u32 i, j, idx;
    u64 total_event_num = 0;
    u64 total_sched_event_num = 0;
    u64 *publish_event_num = NULL;
    u64 *sched_event_num = NULL;

    publish_event_num = (u64 *)sched_kzalloc(sizeof(u64) * SCHED_MAX_EVENT_TYPE_NUM * 2, KA_GFP_KERNEL); /* 2 is the mul */
    if (publish_event_num == NULL) {
        sched_err("Failed to alloc memory.(size=%lx)\n", sizeof(u64) * SCHED_MAX_EVENT_TYPE_NUM * 2);
        return;
    }
    sched_event_num = publish_event_num + SCHED_MAX_EVENT_TYPE_NUM;

    for (i = 0; i < node->cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, i);
        if (cpu_ctx == NULL) {
            continue;
        }

        total_event_num += cpu_ctx->perf_stat.total_publish_event_num;
        total_sched_event_num += cpu_ctx->perf_stat.total_sched_event_num;
        for (j = 0 ; j < SCHED_MAX_EVENT_TYPE_NUM; j++) {
            publish_event_num[j] += cpu_ctx->perf_stat.publish_event_num[j];
            sched_event_num[j] += cpu_ctx->perf_stat.sched_event_num[j];
        }
    }

    idx = sample->record_num++;
    sample->data[idx].timestamp = sched_get_cur_timestamp();
    sample->data[idx].total_event_num = total_event_num;
    sample->data[idx].cur_event_num = total_event_num - total_sched_event_num;

    for (j = 0 ; j < SCHED_MAX_EVENT_TYPE_NUM; j++) {
        sample->data[idx].publish_event_num[j] = publish_event_num[j];
        sample->data[idx].sched_event_num[j] = sched_event_num[j];
    }

    sched_kfree(publish_event_num);
}

STATIC void sched_sample_process(struct sched_numa_node *node)
{
    static u32 num = 0;

    ka_task_mutex_lock(&node->node_guard_work_mutex);
    if ((node->debug_flag & (0x1 << SCHED_DEBUG_SAMPLE_BIT)) == 0) {
        ka_task_mutex_unlock(&node->node_guard_work_mutex);
        return;
    }

    num++;
    if ((num % node->sample_interval) != 0) {
        ka_task_mutex_unlock(&node->node_guard_work_mutex);
        return;
    }

    if (node->node_event_sample->record_num < SCHED_SAMPLE_DATA_NUM) {
        sched_cpu_sample_process(node);
        sched_node_event_sample_process(node);
    }

    if (node->proc_event_sample->record_num < SCHED_SAMPLE_DATA_NUM) {
        sched_proc_event_sample_process(node);
    }
    ka_task_mutex_unlock(&node->node_guard_work_mutex);
}

STATIC void sched_trace_record_process(struct sched_numa_node *node)
{
    struct sched_trace_record_info *trace_record = &node->trace_record;
    u32 valid;
    char recore_reason[SCHED_STR_MAX_LEN] = "guardwork";
    char key[SCHED_STR_MAX_LEN];
    static u32 trace_cnt = 0;
    static u32 recore_cnt = 0;

    if ((node->debug_flag & (0x1 << SCHED_DEBUG_SCHED_TRACE_RECORD_BIT)) == 0) {
        return;
    }

    ka_task_spin_lock_bh(&trace_record->lock);
    valid = trace_record->valid;
    ka_task_spin_unlock_bh(&trace_record->lock);
    if (valid == SCHED_VALID) {
        sched_trace_record(node, trace_record);
        trace_record->valid = SCHED_INVALID;
        recore_cnt = 0;
    } else if ((node->debug_flag & (0x1 << SCHED_DEBUG_TRIGGER_SCHED_TRACE_RECORD_FILE_BIT)) != 0) {
        recore_cnt++;
        if (recore_cnt == SCHED_RECORD_TRACE_CNT) {
            recore_cnt = 0;
            if (snprintf_s(key, SCHED_STR_MAX_LEN, SCHED_STR_MAX_LEN - 1, "%d", trace_cnt) < 0) {
#ifndef EMU_ST
                sched_warn("Unable to snprintf_s variable key.\n");
#endif
                return;
            }
            if (sched_trigger_sched_trace_record(node->node_id, recore_reason, key) != 0) {
#ifndef EMU_ST
                sched_warn("Unable to invoke the sched_trigger_sched_trace_record.\n");
#endif
                return;
            }
            trace_cnt++;
        }
    }
}

void sched_check_sched_task(struct sched_numa_node *node)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 i;

    /* check and schedule task */
    if (ka_base_atomic_read(&node->cur_event_num) == 0) {
        return;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
#if defined(CFG_FEATURE_HARDWARE_MIA) && !defined(ESCHED_HOST)
        if (!sched_is_cpu_belongs_to_vf(node, 0, cpu_ctx)) {
            continue;
        }
#endif
        if (esched_is_cpu_idle(cpu_ctx)) {
            if (cpu_ctx->last_sched_event == cpu_ctx->stat.sched_event_num) {
                cpu_ctx->stat.check_sched_num++;
                sched_wake_up_cpu_task(cpu_ctx, SCHED_WAKED_BY_GUARDWORK);
                sched_debug("Guard work wakeup. (node_id=%u; cpuid=%u; sched_event_num=%llu; check_sched_num=%llu)\n",
                    node->node_id, cpu_ctx->cpuid, cpu_ctx->stat.sched_event_num, cpu_ctx->stat.check_sched_num);
            } else {
                cpu_ctx->last_sched_event = cpu_ctx->stat.sched_event_num;
            }
        }
    }
}

static void sched_proc_check_non_sched_grp(struct sched_grp_ctx *grp_ctx)
{
    u32 i;

    for (i = 0; i < SCHED_MAX_EVENT_PRI_NUM; i++) {
        struct sched_event_list *event_list = sched_get_non_sched_event_list(grp_ctx, i);
        struct sched_event *event = NULL;

        ka_task_spin_lock_bh(&event_list->lock);
        if (event_list->last_sched_num == event_list->sched_num) {
            if (!ka_list_empty_careful(&event_list->head)) {
                event = ka_list_first_entry(&event_list->head, struct sched_event, list);
                sched_wakeup_non_sched_grp(grp_ctx, sched_get_event_thread_map(grp_ctx, event));
            }
        }
        event_list->last_sched_num = event_list->sched_num;
        ka_task_spin_unlock_bh(&event_list->lock);
    }
}

STATIC void sched_check_non_sched_task(struct sched_numa_node *node)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct pid_entry *entry = NULL;
    u32 i;

    ka_task_mutex_lock(&node->pid_list_mutex);
    ka_list_for_each_entry(entry, &node->pid_list, list) {
        proc_ctx = esched_proc_get(node, entry->pid);
        if (proc_ctx == NULL) {
            continue;
        }

        for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
            grp_ctx = sched_get_grp_ctx(proc_ctx, i);
            if ((grp_ctx->sched_mode != SCHED_MODE_NON_SCHED_CPU) || (grp_ctx->thread_num == 0) ||
                (ka_base_atomic_read(&grp_ctx->cur_event_num) == 0)) {
                continue;
            }

            sched_proc_check_non_sched_grp(grp_ctx);
        }

        esched_proc_put(proc_ctx);
    }
    ka_task_mutex_unlock(&node->pid_list_mutex);
}

STATIC u64 sched_get_thread_running_time(struct sched_thread_ctx *thread_ctx)
{
    u64 diff, start_time, cur_time;

    /* first event time not set when timeout */
    if (thread_ctx->start_time == 0) {
        thread_ctx->start_time = sched_get_cur_timestamp();
    }

    /* without isb, cur time may be small than start time
       Considering system performance, abandon isb and let diff be 0 when out of order */
    start_time = thread_ctx->start_time;

    cur_time = sched_get_cur_timestamp();
    diff = tick_to_millisecond(cur_time - start_time);
    if (cur_time < start_time) {
        diff = 0;
    }

    return diff;
}

#ifdef CFG_FEATURE_TIMEOUT_PROCESS
/* all thread in group are timeout, group timeout_flag valid */
STATIC void sched_check_group_timeout(struct sched_thread_ctx *thread_ctx)
{
    u32 i;
    u64 event_bitmap;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_event_thread_map *event_thread_map = NULL;
    struct sched_event_timeout_info *event_timeout_info = NULL;

    grp_ctx = thread_ctx->grp_ctx;
    event_bitmap = thread_ctx->subscribe_event_bitmap;

    ka_task_spin_lock_bh(&grp_ctx->lock);
    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        if ((event_bitmap & (0x1ULL << i)) != 0) {
            event_thread_map = &grp_ctx->event_thread_map[i];
            event_timeout_info = &grp_ctx->event_timeout_info[i];
            event_timeout_info->timeout_thread_num++;
            if (event_thread_map->thread_num == event_timeout_info->timeout_thread_num) {
                event_timeout_info->timeout_flag = SCHED_VALID;
                event_timeout_info->timeout_timestamp = sched_get_cur_timestamp();
                sched_debug("Show debug details. "
                    "(chip_id=%u; pid=%d; gid=%u; event_id=%u; event_bitmap=%llu; timeout=%dms; "
                    "thread_num=%u; cur_timestamp=%llu)\n",
                    grp_ctx->proc_ctx->node->node_id, thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid,
                    i, event_bitmap, SCHED_THREAD_TIMEOUT,
                    event_thread_map->thread_num, event_timeout_info->timeout_timestamp);
            }
        }
    }
    ka_task_spin_unlock_bh(&grp_ctx->lock);
}
#endif

STATIC void sched_check_thread_timeout(struct sched_numa_node *node)
{
    u32 i;
    u64 diff;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        thread_ctx = esched_cpu_cur_thread_get(cpu_ctx);
        if (thread_ctx != NULL) {
            diff = sched_get_thread_running_time(thread_ctx);
            /* not timeout or has detect timeout */
            if ((diff < SCHED_THREAD_TIMEOUT) || (thread_ctx->timeout_detected == SCHED_VALID)) {
                esched_cpu_cur_thread_put(thread_ctx);
                continue;
            }
            /* take mutex chech again */
            ka_task_mutex_lock(&thread_ctx->thread_mutex);
            /* cpu's cur thread still this thread */
            if ((!esched_is_cpu_cur_thread(cpu_ctx, thread_ctx)) ||
                (ka_base_atomic_read(&thread_ctx->status) != SCHED_THREAD_STATUS_RUN)) {
                ka_task_mutex_unlock(&thread_ctx->thread_mutex);
                esched_cpu_cur_thread_put(thread_ctx);
                continue;
            }
            diff = sched_get_thread_running_time(thread_ctx);
            if (diff > SCHED_THREAD_TIMEOUT) {
                sched_warn("Show warning details. "
                    "(chip_id=%u; cpuid=%u; pid=%d; gid=%u; tid=%u; diff=%llu; timeout=%dms; start_time=%llums; "
                    "cur_time=%llums; max_proc_time=%llums; thread_status=%d; cur_event_num=%d)\n",
                    node->node_id, cpu_ctx->cpuid, thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->gid,
                    thread_ctx->tid, diff, SCHED_THREAD_TIMEOUT, tick_to_millisecond(thread_ctx->start_time),
                    tick_to_millisecond(sched_get_cur_timestamp()), tick_to_millisecond(thread_ctx->max_proc_time),
                    (int)ka_base_atomic_read(&thread_ctx->status), (int)ka_base_atomic_read(&cpu_ctx->cur_event_num));
                /* Force the timeout thread to idle state then to schedule the next thread */
                thread_ctx->stat.timeout_cnt++;
                thread_ctx->timeout_detected = SCHED_VALID;
                cpu_ctx->stat.timeout_cnt++;
#ifdef CFG_FEATURE_TIMEOUT_PROCESS
                sched_thread_finish(thread_ctx, SCHED_TASK_FINISH_SCENE_NORMAL);
#ifdef CFG_FEATURE_VFIO
                sched_release_cpu_res(thread_ctx);
#endif
                thread_ctx->timeout_flag = SCHED_VALID;
                ka_base_atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
                ka_wmb();
                sched_check_group_timeout(thread_ctx);
                esched_cpu_idle(cpu_ctx);
                sched_wake_up_cpu_task(cpu_ctx, SCHED_WAKED_BY_TIMEOUT);
#endif
            }
            ka_task_mutex_unlock(&thread_ctx->thread_mutex);
            esched_cpu_cur_thread_put(thread_ctx);
        }
    }
}

STATIC void sched_record_thread_run_time(struct sched_numa_node *node)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_event *event = NULL;
    u64 thread_run_time, start_time;
    u32 i;

    /* check every sched cpu to find runing thread */
    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        thread_ctx = esched_cpu_cur_thread_get(cpu_ctx);
        if (thread_ctx == NULL) {
            continue;
        }

        thread_run_time = millisecond_to_tick(sched_get_thread_running_time(thread_ctx));
        event = thread_ctx->event;
        if ((thread_run_time < node->abnormal_event.thread_run.timestamp_thres) ||
            (ka_base_atomic_read(&thread_ctx->status) != SCHED_THREAD_STATUS_RUN) || (event == NULL)) {
            esched_cpu_cur_thread_put(thread_ctx);
            continue;
        }

        /* every event record only once */
        start_time = thread_ctx->start_time;
        if (cpu_ctx->last_thread_start_time != start_time) {
            event->proc_time = thread_run_time;
            sched_record_abnormal_event(&node->abnormal_event.thread_run, event, thread_ctx, thread_run_time,
                thread_ctx->grp_ctx->sched_mode);
        }

        cpu_ctx->last_thread_start_time = start_time;
        esched_cpu_cur_thread_put(thread_ctx);
    }
}

void sched_query_thread_run_time(u32 chip_id)
{
    u32 i;
    u64 thread_run_time_ms;
    struct sched_event *event = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(chip_id);

    if (node == NULL) {
        sched_err("numa node is invalid. (chip_id=%u)\n", chip_id);
        return;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        thread_ctx = esched_cpu_cur_thread_get(cpu_ctx);
        if (thread_ctx == NULL) {
            continue;
        }
        thread_run_time_ms = sched_get_thread_running_time(thread_ctx);
        event = thread_ctx->event;
        if ((ka_base_atomic_read(&thread_ctx->status) != SCHED_THREAD_STATUS_RUN) || (event == NULL)) {
            (void)memset_s(&cpu_ctx->cpu_abnormal_thread,
                sizeof(struct esched_abnormal_thread_record), 0, sizeof(struct esched_abnormal_thread_record));
            esched_cpu_cur_thread_put(thread_ctx);
            continue;
        }
        cpu_ctx->cpu_abnormal_thread.event_id = event->event_id;
        cpu_ctx->cpu_abnormal_thread.sub_event_id = event->subevent_id;
        cpu_ctx->cpu_abnormal_thread.thread_info = cpu_ctx->thread;
        cpu_ctx->cpu_abnormal_thread.cpu_sched_time = thread_run_time_ms;
        cpu_ctx->cpu_abnormal_thread.curr_timestamp = sched_get_cur_timestamp();

        esched_cpu_cur_thread_put(thread_ctx);
    }
    esched_dev_put(node);
    return;
}

STATIC void sched_node_guard_work(ka_work_struct_t *p_work)
{
    struct sched_numa_node *node = ka_container_of(p_work, struct sched_numa_node, guard_work.work);

    sched_sample_process(node);
    sched_trace_record_process(node);
    sched_check_non_sched_task(node);
    sched_check_thread_timeout(node);
    sched_record_thread_run_time(node);
    node->ops.try_resched_cpu(node);

    (void)ka_task_schedule_delayed_work_on(0, &node->guard_work, ka_system_msecs_to_jiffies(SCHED_GUARD_WORK_PERIOD));
}

STATIC void sched_node_proc_mng_init(struct sched_numa_node *node)
{
    u32 j;

    KA_INIT_LIST_HEAD(&node->del_proc_head);
    KA_INIT_LIST_HEAD(&node->pid_list);

    /* init proc hash table */
    ka_task_mutex_init(&node->proc_mng_mutex);
    ka_task_mutex_init(&node->pid_list_mutex);
    ka_hash_init(node->proc_hash_table);
    for (j = 0; j < SCHED_PROC_HASH_TABLE_SIZE; j++) {
        ka_task_rwlock_init(&node->proc_hash_table_rwlock[j]);
    }
}

STATIC void sched_node_proc_mng_uninit(struct sched_numa_node *node)
{
    ka_task_mutex_destroy(&node->pid_list_mutex);
    ka_task_mutex_destroy(&node->proc_mng_mutex);
}

STATIC int32_t sched_cpu_ctx_init(struct sched_cpu_ctx *cpu_ctx, u32 cpuid)
{
    u32 i;
    int32_t ret;
    struct sched_event *event = NULL;

    cpu_ctx->cpuid = cpuid;

    ret = sched_event_que_init(&cpu_ctx->event_res, SCHED_CPU_EVENT_RES_NUM);
    if (ret != 0) {
        sched_err("Failed to invoke the sched_event_que_init. (cpuid=%u)\n", cpuid);
        return ret;
    }

    /* alloc event resouce enque to event_res que */
    event = (struct sched_event *)sched_vzalloc(sizeof(struct sched_event) * SCHED_CPU_EVENT_RES_NUM);
    if (event == NULL) {
        sched_err("Failed to vzalloc memory for variable event. "
                  "(size=0x%lx)\n", sizeof(struct sched_event) * SCHED_CPU_EVENT_RES_NUM);
        ret = DRV_ERROR_OUT_OF_MEMORY;
        goto uninit_res_que;
    }

    for (i = 0; i < SCHED_CPU_EVENT_RES_NUM; i++) {
        event[i].que = &cpu_ctx->event_res; /* save res que for quely free event */
        (void)sched_event_enque(&cpu_ctx->event_res, &event[i]);
    }

    cpu_ctx->event_base = event;
    ka_base_atomic_set(&cpu_ctx->cpu_status, CPU_STATUS_IDLE);
    ka_task_spin_lock_init(&cpu_ctx->sched_lock);

    return 0;

uninit_res_que:
    sched_event_que_uninit(&cpu_ctx->event_res);

    return ret;
}

STATIC void sched_cpu_ctx_uninit(struct sched_cpu_ctx *cpu_ctx)
{
    if (cpu_ctx->sched_trace != NULL) {
        sched_vfree(cpu_ctx->sched_trace);
        cpu_ctx->sched_trace = NULL;
    }

    if (cpu_ctx->sample != NULL) {
        sched_vfree(cpu_ctx->sample);
        cpu_ctx->sample = NULL;
    }

    sched_vfree(cpu_ctx->event_base);
    cpu_ctx->event_base = NULL;

    sched_event_que_uninit(&cpu_ctx->event_res);
}

STATIC int32_t sched_create_one_cpu_ctx(struct sched_numa_node *node, u32 cpuid)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    int32_t ret;

    cpu_ctx = (struct sched_cpu_ctx *)sched_vzalloc(sizeof(struct sched_cpu_ctx));
    if (cpu_ctx == NULL) {
        sched_err("Node cpu ctx create failed. (node=%u; cpuid=%u)\n", node->node_id, cpuid);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = sched_cpu_ctx_init(cpu_ctx, cpuid);
    if (ret != 0) {
        sched_vfree(cpu_ctx);
        sched_err("Node cpu ctx init failed. (node=%u; cpuid=%u)\n", node->node_id, cpuid);
        return ret;
    }

    node->cpu_ctx[cpuid] = cpu_ctx;
    cpu_ctx->node = node;

    return 0;
}

STATIC void sched_destroy_one_cpu_ctx(struct sched_cpu_ctx *cpu_ctx)
{
    sched_cpu_ctx_uninit(cpu_ctx);
    sched_vfree(cpu_ctx);
}

STATIC int32_t sched_init_sched_cpu(struct sched_numa_node *node, struct sched_sched_cpu_mask *mask, u32 sched_cpu_num)
{
#ifndef CFG_FEATURE_HARDWARE_SCHED
    struct sched_cpu_ctx *cpu_ctx = NULL;
#endif
    int32_t ret;
    u32 i, j;

    for (i = 0; i < sched_cpu_num; i++) {
        u32 cpuid = node->sched_cpuid[i];
        ret = sched_create_one_cpu_ctx(node, cpuid);
        if (ret != 0) {
            for (j = 0; j < i; j++) {
                sched_destroy_one_cpu_ctx(sched_get_cpu_ctx(node, node->sched_cpuid[j]));
                node->cpu_ctx[node->sched_cpuid[j]] = NULL;
            }
            return ret;
        }

#ifndef CFG_FEATURE_HARDWARE_SCHED
        cpu_ctx = sched_get_cpu_ctx(node, cpuid);

        cpu_ctx->sample = (struct sched_cpu_sample *)sched_vzalloc(sizeof(struct sched_cpu_sample));
        if (cpu_ctx->sample == NULL) {
#ifndef EMU_ST
            sched_warn("Malloc sample warn. (size=0x%lx)\n", sizeof(struct sched_cpu_sample));
#endif
        }

        cpu_ctx->sched_trace = (struct sched_cpu_sched_trace *)sched_vzalloc(sizeof(struct sched_cpu_sched_trace));
        if (cpu_ctx->sched_trace == NULL) {
#ifndef EMU_ST
            sched_warn("Malloc sched_trace warn . (size=0x%lx)\n", sizeof(struct sched_cpu_sched_trace));
#endif
        }
#endif
    }

    return 0;
}

STATIC void sched_uninit_sched_cpu(struct sched_numa_node *node, u32 sched_cpu_num)
{
    u32 i;

    for (i = 0; i < sched_cpu_num; i++) {
        sched_destroy_one_cpu_ctx(sched_get_cpu_ctx(node, node->sched_cpuid[i]));
        node->cpu_ctx[node->sched_cpuid[i]] = NULL;
    }
}

static void sched_record_cpu_mask(struct sched_numa_node *node, struct sched_sched_cpu_mask *cpu_mask)
{
    u32 i;
    for (i = 0; i < SCHED_MASK_NUM; i++) {
        if (cpu_mask->mask[i] != 0) {
            sched_info("Show cpu mask. (devid=%u; index=%u; mask=0x%llx)\n", node->node_id, i, cpu_mask->mask[i]);
        }
    }
}

static int sched_init_sched_cpu_num(struct sched_numa_node *node, struct sched_sched_cpu_mask *cpu_mask, u32 *cpu_num)
{
    u32 i, j, cpuid, sched_cpu_num;

    sched_cpu_num = 0;
    for (i = 0; i < SCHED_MASK_NUM; i++) {
        for (j = 0; j < SCHED_MASK_BIT_NUM; j++) {
            if (((cpu_mask->mask[i] >> j) & 0x1) == 0) {
                continue;
            }

            cpuid = i * SCHED_MASK_NUM + j;
            if ((sched_cpu_num >= node->cpu_num) || (cpuid >= node->cpu_num) || (cpuid == 0)) {
                sched_err("Mask out of range. (cpu_num=%u; cpuid=%u)\n", node->cpu_num, cpuid);
                return DRV_ERROR_INVALID_VALUE;
            }

            node->sched_cpuid[sched_cpu_num] = cpuid;
            sched_cpu_num++;
            sched_debug("Set sched cpu. (devid=%u; cpuid=%u)\n", node->node_id, cpuid);
        }
    }

    *cpu_num = sched_cpu_num;

    return 0;
}

static int32_t _sched_set_sched_cpu(struct sched_numa_node *node, struct sched_sched_cpu_mask *cpu_mask)
{
    u32 sched_cpu_num;
    int32_t ret;

    sched_record_cpu_mask(node, cpu_mask);

    ret = sched_init_sched_cpu_num(node, cpu_mask, &sched_cpu_num);
    if (ret != 0) {
        return ret;
    }

    if (sched_cpu_num > 0) {
        ret = sched_init_sched_cpu(node, cpu_mask, sched_cpu_num);
        if (ret != 0) {
            return ret;
        }

        ka_wmb();

        if (node->ops.conf_sched_cpu != NULL) {
            ret = node->ops.conf_sched_cpu(node, sched_cpu_num);
            if (ret != 0) {
                ka_wmb();
                sched_uninit_sched_cpu(node, sched_cpu_num);
                sched_err("Set sched cpu failed. (chip_id=%u; cpu_num=%u; sched_cpu_num=%u)\n",
                    node->node_id, node->cpu_num, sched_cpu_num);
                return ret;
            }
        }

        node->sched_cpu_num = sched_cpu_num; /* must init after alloc cpu ctx */
    }

    sched_info("Set sched cpu success. (chip_id=%u; cpu_num=%u; sched_cpu_num=%u)\n",
        node->node_id, node->cpu_num, node->sched_cpu_num);

    return 0;
}

int32_t sched_set_sched_cpu(u32 chip_id, struct sched_sched_cpu_mask *cpu_mask)
{
    struct sched_numa_node *node = NULL;
    int32_t ret;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("The chip_id is invalid. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    /* All processes will call configuration once, only the first process will configure */
    ka_task_mutex_lock(&node->node_mutex);
    if (node->sched_set_cpu_flag == SCHED_VALID) {
        ka_task_mutex_unlock(&node->node_mutex);
        return 0;
    }

    ret = _sched_set_sched_cpu(node, cpu_mask);
    if (ret == 0) {
        node->sched_set_cpu_flag = SCHED_VALID;
    }

    ka_task_mutex_unlock(&node->node_mutex);

    return ret;
}

STATIC int32_t sched_node_cpu_init(struct sched_numa_node *node)
{
    int32_t ret;

    node->cpu_ctx = (struct sched_cpu_ctx **)sched_vzalloc(sizeof(struct sched_cpu_ctx *) * node->cpu_num);
    if (node->cpu_ctx == NULL) {
        sched_err("Node cpu ctx init failed. (size=0x%lx)\n", sizeof(struct sched_cpu_ctx *) * node->cpu_num);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    node->sched_cpuid = (u32 *)sched_vzalloc(sizeof(u32) * node->cpu_num);
    if (node->sched_cpuid == NULL) {
        sched_vfree(node->cpu_ctx);
        node->cpu_ctx = NULL;
        sched_err("Node sched_cpuid ctx failed. (size=0x%lx)\n", sizeof(u32) * node->cpu_num);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = sched_create_one_cpu_ctx(node, NON_SCHED_DEFAULT_CPUID);
    if (ret != 0) {
        sched_vfree(node->cpu_ctx);
        sched_vfree(node->sched_cpuid);
        node->cpu_ctx = NULL;
        node->sched_cpuid = NULL;
        return ret;
    }

    return ret;
}

STATIC void sched_node_cpu_uninit(struct sched_numa_node *node)
{
    u32 i;

    for (i = 0; i < node->cpu_num; i++) {
        if (node->cpu_ctx[i] != NULL) {
            sched_destroy_one_cpu_ctx(node->cpu_ctx[i]);
            node->cpu_ctx[i] = NULL;
        }
    }

    sched_vfree(node->cpu_ctx);
    node->cpu_ctx = NULL;
    sched_vfree(node->sched_cpuid);
    node->sched_cpuid = NULL;
}

STATIC int32_t sched_node_event_res_init(struct sched_numa_node *node)
{
    u32 i;
    int32_t ret;
    struct sched_event *event = NULL;

    ret = sched_event_que_init(&node->event_res, SCHED_NODE_EVENT_RES_NUM);
    if (ret != 0) {
        sched_err("Failed to invoke the sched_event_que_init. (node_id=%u)\n", node->node_id);
        return ret;
    }

    /* alloc event resouce enque to event_res que */
    event = (struct sched_event *)sched_vzalloc(sizeof(struct sched_event) * SCHED_NODE_EVENT_RES_NUM);
    if (event == NULL) {
        sched_err("Failed to vzalloc memory for variable event. "
                  "(size=0x%lx)\n", sizeof(struct sched_event) * SCHED_NODE_EVENT_RES_NUM);
        sched_event_que_uninit(&node->event_res);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    for (i = 0; i < SCHED_NODE_EVENT_RES_NUM; i++) {
        event[i].que = &node->event_res; /* save res que for quely free event */
        (void)sched_event_enque(&node->event_res, &event[i]);
    }

    node->event_base = event;

    return 0;
}

STATIC void sched_node_event_res_uninit(struct sched_numa_node *node)
{
    sched_vfree(node->event_base);
    node->event_base = NULL;

    sched_event_que_uninit(&node->event_res);
}
STATIC void sched_node_abnormal_event_init(struct sched_numa_node *node)
{
    struct sched_node_abnormal_event *abnormal_event = &node->abnormal_event;
    abnormal_event->publish_syscall.timestamp_thres = microsecond_to_tick(SCHED_DEFAULT_PUBLISH_SYSCALL_TIME_THRES);
    abnormal_event->publish_in_kernel.timestamp_thres = microsecond_to_tick(SCHED_DEFAULT_PUBLISH_IN_KERNEL_TIME_THRES);
    abnormal_event->wakeup.timestamp_thres = microsecond_to_tick(SCHED_DEFAULT_WAKEUP_TIME_THRES);
    abnormal_event->publish_subscribe.timestamp_thres = microsecond_to_tick(SCHED_DEFAULT_PUBLISH_SUBSCRIBE_TIME_THRES);
    abnormal_event->proc.timestamp_thres = microsecond_to_tick(SCHED_DEFAULT_PROC_TIME_THRES);
    abnormal_event->thread_run.timestamp_thres = microsecond_to_tick(SCHED_DEFAULT_PROC_TIME_THRES);
}
#if !defined (CFG_SOC_PLATFORM_MDC_V51) && !defined (CFG_SOC_PLATFORM_MDC_V11)
int32_t sched_get_firt_ctrlcpu(void)
{
    return 0;
}
#else
int32_t sched_get_firt_ctrlcpu(void)
{
    int32_t ret, cpu_id;
    cpumask_t ctrlcpu = {};
    ret = drv_get_ctrlcpu_mask(&ctrlcpu);
    if (ret != 0) {
        return 0;
    }
    if (ka_base_cpumask_empty(&ctrlcpu)) {
        return 0;
    }
#ifndef AOS_LLVM_BUILD
    cpu_id = ka_base_find_first_bit(ctrlcpu.bits, NR_CPUS);
#else
    cpu_id = ka_base_find_first_bit(ctrlcpu.bits, NR_CPU);
#endif
    return cpu_id;
}
#endif

#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
STATIC int32_t sched_node_sched_cpu_init(u32 dev_id)
{
    struct cpu_domain_info cpu_info;
    struct sched_sched_cpu_mask cpu_mask;
    struct sched_numa_node *node = NULL;
    int32_t ret;
    u32 i = 0;

    node = sched_get_numa_node(dev_id);
    if (node == NULL) {
        sched_err("Failed to invoke the sched_get_numa_node to get node. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = get_cpudomain_info(&cpu_info);
    if (ret != 0) {
        sched_err("Get cpu domain info failed, err=%d.\n", ret);
        return ret;
    }

    (void)memset_s(&cpu_mask, sizeof(cpu_mask), 0, sizeof(cpu_mask));
    for (i = 0; i < cpu_info.aicpu_num; i++) {
#ifdef AOS_LLVM_BUILD
        cpu_mask.mask[0] |= (1U << (i + cpu_info.ctrlcpu_num));
#else
	cpu_mask.mask[0] |= (1U << (i + cpu_info.ctrlcpu_num + cpu_info.datacpu_num));
#endif
    }

    if ((cpu_info.ctrlcpu_num + cpu_info.datacpu_num + cpu_info.aicpu_num) > SCHED_MASK_BIT_NUM) {
        sched_err("Invalid para. (sched_ccpu_num=%u; sched_dcpu_num=%u; sched_aicpu_num=%u)\n",
            cpu_info.ctrlcpu_num, cpu_info.datacpu_num, cpu_info.aicpu_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = _sched_set_sched_cpu(node, &cpu_mask);
    if (ret == 0) {
        node->sched_set_cpu_flag = SCHED_VALID;
    }
    return ret;
}
#endif

STATIC int32_t esched_init_numa_node(struct sched_numa_node *node)
{
    int32_t ret;

    node->cpu_num = ka_system_num_online_cpus();

    ret = sched_node_cpu_init(node);
    if (ret != 0) {
        return ret;
    }

    ret = sched_node_event_res_init(node);
    if (ret != 0) {
        sched_node_cpu_uninit(node);
        sched_err("Node res init failed. (node=%u)\n", node->node_id);
        return ret;
    }

    sched_node_proc_mng_init(node);
    sched_node_abnormal_event_init(node);
    ka_task_spin_lock_init(&node->trace_record.lock);
    ka_task_mutex_init(&node->node_mutex);
    ka_task_mutex_init(&node->node_guard_work_mutex);
    node->debug_flag = 0;
    node->sample_period = SCHED_SAMPLE_DEFAULT_PERIOD;
    node->sample_interval = node->sample_period / SCHED_GUARD_WORK_PERIOD;
    node->event_trace.enable_flag = SCHED_TRACE_ENABLE;
    ka_base_atomic_set(&node->refcnt, 1);

    sched_node_event_list_init(node);

    sched_debug("Node dev init success. (devid=%u)\n", node->node_id);
    return 0;
}

#ifndef AOS_LLVM_BUILD
void esched_try_cond_resched_by_time(u32 *pre_stamp, u32 time_threshold)
{
    u32 timeinterval;

    timeinterval = ka_system_jiffies_to_msecs(ka_jiffies - *pre_stamp);
    if (timeinterval > time_threshold) {
#if !defined(EVENT_SCHED_UT) && !defined(EMU_ST)
        ka_task_cond_resched();
        *pre_stamp = (u32)ka_jiffies;
#endif
    }
}
#endif

STATIC void esched_uninit_numa_node(struct sched_numa_node *node)
{
    sched_info("Node dev uninit success. (devid=%u)\n", node->node_id);

    sched_sysfs_node_debug_uninit(node);
    sched_node_event_res_uninit(node);
    sched_node_proc_mng_uninit(node);

    ka_task_mutex_destroy(&node->node_guard_work_mutex);
    sched_node_cpu_uninit(node);
}

STATIC int32_t esched_create_dev_lock(u32 devid, struct sched_dev_ops *ops)
{
    struct sched_numa_node *node = NULL;
    int32_t ret;
    int32_t cpu_id;

    ka_task_read_lock_bh(&sched_dev_rwlock);
    if ((sched_node_is_valid[devid]) || (sched_node[devid] != NULL)) {
        ka_task_read_unlock_bh(&sched_dev_rwlock); 	    
        sched_err("Repeat init. (devid=%u)\n", devid);
        return DRV_ERROR_REPEATED_INIT;
    }
    ka_task_read_unlock_bh(&sched_dev_rwlock);

    node = (struct sched_numa_node *)sched_vzalloc(sizeof(struct sched_numa_node));
    if (node == NULL) {
        sched_err("Valloc Mem failed. (size=0x%lx)\n", sizeof(struct sched_numa_node));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    node->node_id = devid;
    node->ops = *ops;

    ret = esched_init_numa_node(node);
    if (ret != 0) {
        sched_vfree(node);
        return ret;
    }

    KA_TASK_INIT_DELAYED_WORK(&node->guard_work, sched_node_guard_work);
    cpu_id = sched_get_firt_ctrlcpu();
    (void)ka_task_schedule_delayed_work_on(cpu_id, &node->guard_work, ka_system_msecs_to_jiffies(SCHED_GUARD_WORK_PERIOD));

    sched_node[devid] = node;
	
    if (ops->node_res_init != NULL) {
        ret = ops->node_res_init(node);
        if (ret != 0) {
            (void)ka_task_cancel_delayed_work_sync(&node->guard_work);
			esched_uninit_numa_node(node);
			sched_vfree(node);
			sched_node[devid] = NULL;
            sched_err("Node hard res init failed. (devid=%u, ret=%d)\n", devid, ret);
            return ret;
        }
    }

    ka_task_write_lock_bh(&sched_dev_rwlock);
	sched_node_is_valid[devid] = true;
    ka_task_write_unlock_bh(&sched_dev_rwlock);

#ifdef CFG_SOC_PLATFORM_MDC_V51
    ret = sched_node_sched_cpu_init(devid);
    if (ret != 0) {
        ka_task_write_lock_bh(&sched_dev_rwlock);
        sched_node_is_valid[devid] = false;
        ka_task_write_unlock_bh(&sched_dev_rwlock);
        (void)ka_task_cancel_delayed_work_sync(&node->guard_work);
        esched_uninit_numa_node(node);
        sched_vfree(node);
        ka_task_write_lock_bh(&sched_dev_rwlock);
        sched_node[devid] = NULL;
        ka_task_write_unlock_bh(&sched_dev_rwlock);
        sched_err("Node sched cpu init failed. (devid=%u)\n", devid);
        return ret;
    }
#endif

    return 0;
}

STATIC void sched_free_dev(struct sched_numa_node *node)
{
	u32 devid = node->node_id;
    (void)ka_task_cancel_delayed_work_sync(&node->guard_work);
    if (node->ops.node_res_uninit != NULL) {
        node->ops.node_res_uninit(node);
    }
    esched_uninit_numa_node(node);
    sched_vfree(node);
	ka_mb();
	sched_node[devid] = NULL;
}

STATIC void esched_destroy_dev_lock(u32 devid)
{
    struct sched_numa_node *node = sched_node[devid];

    if (node != NULL) {
        ka_task_write_lock_bh(&sched_dev_rwlock);
        sched_node_is_valid[devid] = false;
        ka_task_write_unlock_bh(&sched_dev_rwlock);

        if (ka_base_atomic_dec_return(&node->refcnt) > 0) {
            return;
        }

        sched_free_dev(node);
    }
}

int32_t esched_create_dev(u32 devid, struct sched_dev_ops *ops)
{
    int32_t ret;

    ka_task_mutex_lock(&sched_dev_mutex);
    ret = esched_create_dev_lock(devid, ops);
    ka_task_mutex_unlock(&sched_dev_mutex);

    return ret;
}

void esched_destroy_dev(u32 devid)
{
    ka_task_mutex_lock(&sched_dev_mutex);
    esched_destroy_dev_lock(devid);
    ka_task_mutex_unlock(&sched_dev_mutex);
}

struct sched_numa_node *esched_dev_get(u32 devid)
{
    struct sched_numa_node *node = NULL;

    if (devid >= SCHED_MAX_CHIP_NUM) {
        return NULL;
    }

    ka_task_read_lock_bh(&sched_dev_rwlock);
    if ((sched_node_is_valid[devid]) && (sched_node[devid] != NULL)) {
		node = sched_node[devid];
        ka_base_atomic_inc(&node->refcnt);
    }
    ka_task_read_unlock_bh(&sched_dev_rwlock);

    return node;
}

void esched_dev_put(struct sched_numa_node *node)
{
    if (ka_base_atomic_dec_return(&node->refcnt) > 0) {
        return;
    }

    sched_free_dev(node);
}

#ifndef ESCHED_HOST
u32 esched_get_numa_node_num(void)
{
    return ((u32)ka_system_cpu_to_node(ka_system_num_online_cpus() - 1) + 1);
}
#endif

void esched_setup_dev_soft_ops(struct sched_dev_ops *ops)
{
    ops->sumbit_event = esched_submit_event_distribute;
    ops->sched_cpu_get_next_thread = sched_get_next_thread;
    ops->sched_cpu_to_idle = sched_cpu_to_idle;
    ops->sched_cpu_get_event = sched_cpu_get_specified_event;
    ops->conf_sched_cpu = NULL;
    ops->map_host_pid = NULL;
    ops->unmap_host_pid = NULL;
    ops->task_exit_check_sched_cpu = sched_task_exit_check_sched_cpu;
    ops->try_resched_cpu = sched_check_sched_task;
    ops->node_res_init = NULL;
    ops->node_res_uninit = NULL;
}

STATIC int32_t esched_create_default_dev(void)
{
#ifdef ESCHED_HOST

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    /*  dc host, create dev on mng call init instance */
#else // CFG_FEATURE_EXTERNAL_CDEV else
    struct sched_dev_ops ops;
    u32 devid = 0;
    int ret;

    /*  helper host (Independent working mode) */
    esched_setup_dev_soft_ops(&ops);
    ret = esched_create_dev(devid, &ops);
    if (ret != 0) {
        sched_err("Create dev failed. (devid=%u)\n", devid);
        return ret;
    }
#endif // CFG_FEATURE_EXTERNAL_CDEV end

#else // ESCHED_HOST else

#if defined (CFG_SOC_PLATFORM_MDC_V51) /* dc use uda online */
    /* device phy dev */
    u32 dev_num = esched_get_numa_node_num();
    struct sched_dev_ops ops;
    u32 devid = 0;
    int ret;

    esched_setup_dev_soft_ops(&ops);

    for (devid = 0; devid < dev_num; devid++) {
        ret = esched_create_dev(devid, &ops);
        if (ret != 0) {
            u32 i;
            for (i = 0; i < devid; i++) {
                esched_destroy_dev(i);
            }
            sched_err("Create dev failed. (devid=%u)\n", devid);
            return ret;
        }
    }
#endif // CFG_SOC_PLATFORM_MDC_V51 end

#endif // ESCHED_HOST end

    return 0;
}

STATIC void esched_destroy_default_dev(void)
{
#ifdef ESCHED_HOST

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    /*  dc host, destroy dev on mng call uninit instance */
#else
    u32 devid = 0;

    /*  helper host (Independent working mode) */
    esched_destroy_dev(devid);
#endif

#else // ESCHED_HOST else

#if defined (CFG_SOC_PLATFORM_MDC_V51) /* dc use uda offline */

    /* device phy dev */
    u32 dev_num = esched_get_numa_node_num();
    u32 devid = 0;

    for (devid = 0; devid < dev_num; devid++) {
        esched_destroy_dev(devid);
    }
#endif // CFG_SOC_PLATFORM_MDC_V51 end

#endif // ESCHED_HOST end
}

#ifndef ESCHED_HOST
#if !defined (CFG_SOC_PLATFORM_MDC_V51)
#include "dbl/uda.h"

#define ESCHED_NOTIFIER "esched"

static void esched_remove_one_dev(u32 dev_id)
{
    module_feature_auto_uninit_dev(dev_id);
    esched_destroy_dev(dev_id);
}

static int esched_add_one_dev(u32 dev_id)
{
    struct sched_dev_ops ops;
    int ret;

#ifdef CFG_FEATURE_HARDWARE_SCHED
    esched_setup_dev_hw_ops(&ops);
#else
    esched_setup_dev_soft_ops(&ops);
#endif

    ret = esched_create_dev(dev_id, &ops);
    if (ret != 0) {
        sched_err("Create dev failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

#ifdef CFG_SOC_PLATFORM_MDC_V11
    ret = sched_node_sched_cpu_init(dev_id);
    if (ret != 0) {
        esched_remove_one_dev(dev_id);
        sched_err("Node sched cpu init failed. (dev_id=%u)\n", dev_id);
        return ret;
    }
#endif

#ifdef CFG_FEATURE_VFIO
    sched_node_vf_init(dev_id);
#endif

    ret = module_feature_auto_init_dev(dev_id);
    if (ret != 0) {
#ifndef EMU_ST
        esched_destroy_dev(dev_id);
        sched_err("module feature init failed. (dev_id=%u)\n", dev_id);
        return ret;
#endif
    }

    return 0;
}

static int esched_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    switch (action) {
        case UDA_INIT:
            ret = esched_add_one_dev(udevid);
            break;
        case UDA_UNINIT:
            esched_remove_one_dev(udevid);
            break;
#if defined(CFG_FEATURE_HARDWARE_MIA) && !defined(EMU_ST)
        case UDA_TO_MIA:
            esched_drv_reset_phy_dev(udevid);
            break;
        case UDA_TO_SIA:
            esched_drv_restore_phy_dev(udevid);
            break;
#endif
#ifdef CFG_FEATURE_PM
        case UDA_SUSPEND:
            ret = esched_pm_suspend(udevid);
            break;
        case UDA_RESUME:
            ret = esched_pm_resume(udevid);
            break;
#ifdef CFG_SOC_PLATFORM_MDC_V11
        case UDA_SHUTDOWN:
            ret = esched_pm_shutdown(udevid);
            break;
#endif
#endif
        default:
            /* Ignore other actions. */
            return 0;
    }

    sched_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

static int esched_notifier_register(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_local_real_entity_type_pack(&type);
    ret = uda_notifier_register(ESCHED_NOTIFIER, &type, UDA_PRI2, esched_notifier_func);
    if (ret != 0) {
        sched_err("Register notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

static void esched_notifier_unregister(void)
{
    struct uda_dev_type type;

    uda_davinci_local_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(ESCHED_NOTIFIER, &type);
}
#endif
#endif

int32_t esched_init(void)
{
    u32 i;
    int ret;

    for (i = 0; i < SCHED_MAX_CHIP_NUM; i++) {
        sched_node[i] = NULL;
		sched_node_is_valid[i] = false;
    }

    ka_task_rwlock_init(&sched_dev_rwlock);

    ret = esched_create_default_dev();
    if (ret != 0) {
        return ret;
    }
#ifndef ESCHED_HOST
#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_HARDWARE_MIA)
    sched_vf_init();
#endif
#endif

#ifdef CFG_FEATURE_HARDWARE_SCHED
    (void)trs_chan_register_abnormal_handle(ABNORMAL_TASK_TYPE_AICPU, esched_drv_abnormal_task_handle);
#endif

#if !defined (ESCHED_HOST) && !defined (CFG_SOC_PLATFORM_MDC_V51)
    ret = esched_notifier_register();
    if (ret != 0) {
#ifdef CFG_FEATURE_HARDWARE_SCHED
        (void)trs_chan_unregister_abnormal_handle(ABNORMAL_TASK_TYPE_AICPU);
#endif

#ifndef ESCHED_HOST
#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_HARDWARE_MIA)
        sched_vf_uninit();
#endif
#endif
        esched_destroy_default_dev();
        return ret;
    }
#endif

#ifdef CFG_FEATURE_REMOTE_SUBMIT
    (void)esched_client_init();
#endif

    return 0;
}

void esched_uninit(void)
{
#ifdef CFG_FEATURE_REMOTE_SUBMIT
    esched_client_uninit();
#endif

#ifndef ESCHED_HOST
#if !defined (CFG_SOC_PLATFORM_MDC_V51)
    esched_notifier_unregister();
#endif
#endif

#ifdef CFG_FEATURE_HARDWARE_SCHED
    (void)trs_chan_unregister_abnormal_handle(ABNORMAL_TASK_TYPE_AICPU);
#endif

#ifndef ESCHED_HOST
#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_HARDWARE_MIA)
    sched_vf_uninit();
#endif
#endif

    esched_destroy_default_dev();
    /*lint +e144 +e666 +e102 +e1112 +e145 +e151 +e437 +e679 +e30 +e514 +e65 +e446*/
}

#ifdef EVENT_SCHED_UT
/* The drive UT will be switched as a whole.
    Currently, it only covers the newly added code without logical test
 */
int32_t tmp_sched_ut_test_cloudv2(void)
{
    return 0;
}
#endif


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

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pid.h>

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_list_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "dbl/runenv_config.h"
#include "dbl/uda.h"

#include "dms_define.h"
#include "dms_event.h"
#include "dms_event_dfx.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_user_common.h"
#include "smf_event_adapt.h"
#include "dms_event_distribute.h"

struct pid_info_t {
    pid_t tgid;
    pid_t pid;
};

#define DMS_INVALID_PID (-1)

#define DMS_EXCEPTION_KFIFO_CELL (sizeof(DMS_EVENT_NODE_STRU))
#define DMS_EXCEPTION_KFIFO_SIZE (DMS_EXCEPTION_KFIFO_CELL * DMS_MAX_EVENT_NUM)

/* ns_id 0-127 for normal container, 128 for host or admin container */
STATIC DMS_EVENT_STRU_T* g_dms_event_ns_stu[UDA_NS_NUM + 1];
static DMS_EVENT_DISTRIBUTE_HANDLE_T g_dms_event_handle_list[DMS_EVENT_DISTRIBUTE_FUNC_MAX];
static struct mutex g_dms_event_handle_mutex;
static struct mutex g_dms_event_ns_node_mutex;
static struct dms_device_event g_remote_fault_event[DEVDRV_PF_DEV_MAX_NUM];
static struct mutex g_remote_fault_event_mutex;

STATIC void release_one_event_proc(DMS_EVENT_STRU_T* event_ns_stu, u32 proc_idx);

struct dms_device_event* dms_get_device_event(u32 dev_id)
{
    if (dev_id >= DEVDRV_PF_DEV_MAX_NUM) {
        return NULL;
    }

    return &g_remote_fault_event[dev_id];
}
KA_EXPORT_SYMBOL(dms_get_device_event);

void dms_device_fault_event_init(void)
{
    int i;

    ka_task_mutex_init(&g_remote_fault_event_mutex);
    ka_task_mutex_lock(&g_remote_fault_event_mutex);
    for (i = 0; i < DEVDRV_PF_DEV_MAX_NUM; i++) {
        ka_task_mutex_init(&g_remote_fault_event[i].lock);
        INIT_LIST_HEAD(&g_remote_fault_event[i].head);
        g_remote_fault_event[i].event_num = 0;
    }
    ka_task_mutex_unlock(&g_remote_fault_event_mutex);
}

void dms_release_one_device_remote_event(unsigned int dev_id)
{
    struct dms_device_event* remote_fault_event = NULL;
    DMS_EVENT_NODE_STRU* pos = NULL;
    DMS_EVENT_NODE_STRU* n = NULL;

    remote_fault_event = dms_get_device_event(dev_id);
    if (remote_fault_event == NULL) {
        return;
    }

    ka_task_mutex_lock(&remote_fault_event->lock);
    ka_list_for_each_entry_safe(pos, n, &remote_fault_event->head, node) {
        list_del(&pos->node);
        dbl_kfree(pos);
        pos = NULL;
    }
    remote_fault_event->event_num = 0;
    ka_task_mutex_unlock(&remote_fault_event->lock);
    remote_fault_event = NULL;
    return;
}
KA_EXPORT_SYMBOL(dms_release_one_device_remote_event);

void dms_device_fault_event_exit(void)
{
    u32 i;

    ka_task_mutex_lock(&g_remote_fault_event_mutex);
    for (i = 0; i < DEVDRV_PF_DEV_MAX_NUM; i++) {
        dms_release_one_device_remote_event(i);
        ka_task_mutex_destroy(&g_remote_fault_event[i].lock);
    }
    ka_task_mutex_unlock(&g_remote_fault_event_mutex);
    ka_task_mutex_destroy(&g_remote_fault_event_mutex);
}

void dms_event_distribute_stru_init(void)
{
    u32 i;

    ka_task_mutex_init(&g_dms_event_handle_mutex);
    ka_task_mutex_init(&g_dms_event_ns_node_mutex);

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (i = 0; i < (UDA_NS_NUM + 1); i++) {
        g_dms_event_ns_stu[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        g_dms_event_handle_list[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
}

static void dms_event_ns_stu_free(u32 ns_id)
{
    if (g_dms_event_ns_stu[ns_id] != NULL) {
        dbl_kfree(g_dms_event_ns_stu[ns_id]);
        g_dms_event_ns_stu[ns_id] = NULL;
    }
}

STATIC void dms_event_release_one_ns_node(u32 ns_id)
{
    u32 i;

    if (g_dms_event_ns_stu[ns_id] == NULL) {
        return;
    }

    for (i = 0; i < DMS_EVENT_EXCEPTION_PROCESS_MAX; i++) {
        release_one_event_proc(g_dms_event_ns_stu[ns_id], i);
    }
    if (g_dms_event_ns_stu[ns_id]->process_num == 0) {
        dms_event_ns_stu_free(ns_id);
    }
}

void dms_event_distribute_stru_exit(void)
{
    u32 i;

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (i = 0; i < (UDA_NS_NUM + 1); i++) {
        dms_event_release_one_ns_node(i);
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        g_dms_event_handle_list[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);

    ka_task_mutex_destroy(&g_dms_event_handle_mutex);
    ka_task_mutex_destroy(&g_dms_event_ns_node_mutex);
}

STATIC int add_exception_to_event_proc(DMS_EVENT_PROCESS_STRU *event_proc, DMS_EVENT_NODE_STRU *exception_node)
{
    DMS_EVENT_NODE_STRU exception_buf = {{0}};

    ka_task_mutex_lock(&event_proc->process_mutex);
    if (event_proc->process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        return DRV_ERROR_NONE;
    }
    while (ka_base_kfifo_avail(&event_proc->event_fifo) < DMS_EXCEPTION_KFIFO_CELL) {
        if (!ka_base_kfifo_out(&event_proc->event_fifo, &exception_buf, DMS_EXCEPTION_KFIFO_CELL)) {
            dms_warn("kfifo_out warn.\n");
        }
        dms_event("Dms event is covered. (dev_id=%u; event_id=0x%X; event_serial_num=%d; notify_serial_num=%d; "
                  "pid=%d; tid=%d)\n",
                  exception_buf.event.deviceid, exception_buf.event.event_id, exception_buf.event.event_serial_num,
                  exception_buf.event.notify_serial_num, event_proc->process_pid, event_proc->process_tid);
    }

    ka_base_kfifo_in(&event_proc->event_fifo, exception_node, DMS_EXCEPTION_KFIFO_CELL);
    event_proc->exception_num = ka_base_kfifo_len(&event_proc->event_fifo) / DMS_EXCEPTION_KFIFO_CELL;
    ka_task_mutex_unlock(&event_proc->process_mutex);

    ka_task_wake_up_interruptible(&event_proc->event_wait);
    return DRV_ERROR_NONE;
}

int dms_event_distribute_to_process(DMS_EVENT_NODE_STRU *exception_node)
{
    int ret;
    int fail_num = 0;
    u32 ns_id;
    u32 idx;

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (ns_id = 0; ns_id <= UDA_NS_NUM; ns_id++) {
        if (g_dms_event_ns_stu[ns_id] == NULL) {
            continue;
        }
        for (idx = 0; idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; idx++) {
            ret = add_exception_to_event_proc(&g_dms_event_ns_stu[ns_id]->event_process[idx], exception_node);
            if (ret != 0) {
                fail_num++;
                dms_err("Add event code to process failed. "
                    "(dev_id=%u; event_code=0x%x; ns_id=%u; process_index=%u)\n",
                    exception_node->event.deviceid, exception_node->event.event_code, ns_id, idx);
            }
        }
    }

    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
    if (fail_num != 0) {
        dms_err("Distribute event code to process list failed. "
                "(dev_id=%u; event_code=0x%x; fail_num=%d)\n",
                exception_node->event.deviceid, exception_node->event.event_code, fail_num);
    }
    return fail_num;
}

static int get_process_index_range(enum cmd_source cmd_src, u32 *start_index, u32 *end_index)
{
    switch (cmd_src) {
        case FROM_DSMI:
            *start_index = DMS_EVENT_EXCEPTION_PROCESS_DSMI_START;
            *end_index = DMS_EVENT_EXCEPTION_PROCESS_DSMI_END;
            return 0;
        case FROM_HAL:
            *start_index = DMS_EVENT_EXCEPTION_PROCESS_HAL_START;
            *end_index = DMS_EVENT_EXCEPTION_PROCESS_HAL_END;
            return 0;
        case FROM_KERNEL:
            *start_index = DMS_EVENT_EXCEPTION_PROCESS_KERNEL_START;
            *end_index = DMS_EVENT_EXCEPTION_PROCESS_KERNEL_END;
            return 0;
        default:
            return DRV_ERROR_PARA_ERROR;
    }
}

static DMS_EVENT_PROCESS_STRU* alloc_new_event_proc(DMS_EVENT_STRU_T* event_ns_stu, struct pid_info_t pid_info,
    u32 start_index, u32 end_index)
{
    u32 i;

    for (i = start_index; i < end_index; i++) {
        ka_task_mutex_lock(&event_ns_stu->event_process[i].process_mutex);
        if (event_ns_stu->event_process[i].process_status != DMS_EVENT_PROCESS_STATUS_IDLE) {
            ka_task_mutex_unlock(&event_ns_stu->event_process[i].process_mutex);
            continue;
        }
        if (ka_base_kfifo_alloc(&event_ns_stu->event_process[i].event_fifo,
                        DMS_EXCEPTION_KFIFO_SIZE,   KA_GFP_KERNEL | __KA_GFP_ACCOUNT) != 0) {
            ka_task_mutex_unlock(&event_ns_stu->event_process[i].process_mutex);
            dms_err("kfifo_alloc failed. (ns_id=%u; tgid=%d; pid=%d; index=%u)\n",
                    event_ns_stu->ns_id, pid_info.tgid, pid_info.pid, i);
            return NULL;
        }
        ka_base_kfifo_reset(&event_ns_stu->event_process[i].event_fifo);
        event_ns_stu->event_process[i].exception_num = 0;
        event_ns_stu->event_process[i].process_pid = pid_info.tgid;
        event_ns_stu->event_process[i].process_tid = pid_info.pid;
        event_ns_stu->event_process[i].start_time = ka_task_get_current_starttime();
        event_ns_stu->event_process[i].process_status = DMS_EVENT_PROCESS_STATUS_WORK;
        ka_task_mutex_unlock(&event_ns_stu->event_process[i].process_mutex);
        dms_debug("Get a process success. (ns_id=%u; tgid=%d; pid=%d; index=%u)\n",
                  event_ns_stu->ns_id, pid_info.tgid, pid_info.pid, i);
        return &event_ns_stu->event_process[i];
    }
    return NULL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)) && defined(CFG_HOST_ENV)
STATIC bool get_pid_start_time(u32 pid, u64* start_time)
{
#if !defined(UT_VCAST) && !defined(DMS_UT)
    struct pid *pid_struct;
    struct task_struct *task = NULL;

    pid_struct = find_get_pid(pid);
    if (pid_struct == NULL) {
        return false;
    }

    rcu_read_lock();
    task = pid_task(pid_struct, PIDTYPE_PID);
    if (task == NULL) {
        rcu_read_unlock();
        put_pid(pid_struct);
        return false;
    }

    *start_time = task->start_time;
    rcu_read_unlock();
    put_pid(pid_struct);

#endif
    return true;
}
#endif

STATIC void check_and_release_event_procs(DMS_EVENT_STRU_T* event_ns_stu)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)) && defined(CFG_HOST_ENV)
    u32 i;
    pid_t pid, tgid;
    u64 record_start_time = 0, pid_start_time = 0, tgid_start_time = 0;
    u8 process_status;

    if (event_ns_stu == NULL) {
        return;
    }

    for (i = 0; i < DMS_EVENT_EXCEPTION_PROCESS_MAX; i++) {
        tgid = event_ns_stu->event_process[i].process_pid;
        pid = event_ns_stu->event_process[i].process_tid;
        record_start_time = event_ns_stu->event_process[i].start_time;
        process_status = event_ns_stu->event_process[i].process_status;

        if (process_status == DMS_EVENT_PROCESS_STATUS_IDLE) {
            continue;
        }

        if (get_pid_start_time(tgid, &tgid_start_time) && get_pid_start_time(pid, &pid_start_time) &&
            pid_start_time == record_start_time) {
            continue;
        } else {
            release_one_event_proc(event_ns_stu, i);
            dms_debug("Release one process. (ns_id=%d; tgid=%d; pid=%d; record_start_time=%llu; current_time=%llu; index=%u)\n",
                      event_ns_stu->ns_id, tgid, pid, record_start_time, pid_start_time, i);
        }
    }
#endif
    return;
}

static int get_old_event_proc(DMS_EVENT_STRU_T* event_ns_stu, struct pid_info_t pid_info,
                              u32 start_index, u32 end_index, DMS_EVENT_PROCESS_STRU** event_proc)
{
    u32 i;

    for (i = start_index; i < end_index; i++) {
        if (event_ns_stu->event_process[i].process_pid != pid_info.tgid ||
            event_ns_stu->event_process[i].process_tid != pid_info.pid) {
            continue;
        }

        event_ns_stu->event_process[i].process_status = DMS_EVENT_PROCESS_STATUS_WORK;
        *event_proc = &event_ns_stu->event_process[i];
        return 0;
    }
    *event_proc = NULL;
    return 0;
}

static int dms_get_cur_ns_id(u32* ns_id)
{
#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
    *ns_id = 0;
#else
    int ret;

    if (run_in_normal_docker()) {
 #ifdef CFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
        ret = smf_get_container_ns_id(ns_id);
        if (ret != 0) {
            return DRV_ERROR_INNER_ERR;
        }
 #else
        ret = uda_get_cur_ns_id(ns_id);
        if (ret != 0) {
            return DRV_ERROR_INNER_ERR;
        }
 #endif // CFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
    } else {
        *ns_id = UDA_NS_NUM;
    }

#endif // CFG_SOC_PLATFORM_MDC_V51 || CFG_SOC_PLATFORM_MDC_V11
    return 0;
}

static DMS_EVENT_STRU_T* dms_event_ns_stu_create(u32 ns_id)
{
    int i;

    g_dms_event_ns_stu[ns_id] = dbl_kzalloc(sizeof(DMS_EVENT_STRU_T), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if ( g_dms_event_ns_stu[ns_id] == NULL) {
        dms_err("Call kzalloc failed. (ns_id=%u)\n", ns_id);
        return NULL;
    }

    g_dms_event_ns_stu[ns_id]->process_num = 0;
    g_dms_event_ns_stu[ns_id]->ns_id = ns_id;
    for (i = 0; i < DMS_EVENT_EXCEPTION_PROCESS_MAX; i++) {
        ka_task_mutex_init(&g_dms_event_ns_stu[ns_id]->event_process[i].process_mutex);
        ka_task_mutex_lock(&g_dms_event_ns_stu[ns_id]->event_process[i].process_mutex);
        ka_task_init_waitqueue_head(&g_dms_event_ns_stu[ns_id]->event_process[i].event_wait);
        g_dms_event_ns_stu[ns_id]->event_process[i].exception_num = 0;
        g_dms_event_ns_stu[ns_id]->event_process[i].process_pid = DMS_INVALID_PID;
        g_dms_event_ns_stu[ns_id]->event_process[i].process_tid = DMS_INVALID_PID;
        g_dms_event_ns_stu[ns_id]->event_process[i].start_time = 0;
        g_dms_event_ns_stu[ns_id]->event_process[i].process_status = DMS_EVENT_PROCESS_STATUS_IDLE;
        ka_task_mutex_unlock(&g_dms_event_ns_stu[ns_id]->event_process[i].process_mutex);
    }

    return g_dms_event_ns_stu[ns_id];
}

static int get_or_create_event_ns_stu(DMS_EVENT_STRU_T** event_ns_stu)
{
    u32 ns_id = UDA_NS_NUM + 1;
    int ret;

    ret = dms_get_cur_ns_id(&ns_id);
    if (ret != 0 || ns_id > UDA_NS_NUM) {
        dms_err("Get ns id failed. (ns_id=%u; ret=%d)\n", ns_id, ret);
        return ret;
    }

    if (g_dms_event_ns_stu[ns_id] == NULL) {
        *event_ns_stu = dms_event_ns_stu_create(ns_id);
        if (*event_ns_stu == NULL) {
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    } else {
        check_and_release_event_procs(g_dms_event_ns_stu[ns_id]);
        *event_ns_stu = g_dms_event_ns_stu[ns_id];
    }

    return 0;
}

static int get_current_ns_event_proc(struct pid_info_t pid_info, enum cmd_source cmd_src, DMS_EVENT_PROCESS_STRU** event_proc)
{
    int ret;
    u32 start_index = 0;
    u32 end_index = 0;
    DMS_EVENT_STRU_T* event_ns_stu = NULL;

    *event_proc = NULL;
    ret = get_process_index_range(cmd_src, &start_index, &end_index);
    if (ret != 0 || start_index >= DMS_EVENT_EXCEPTION_PROCESS_MAX || end_index > DMS_EVENT_EXCEPTION_PROCESS_MAX) {
        dms_err("Invalid para. (pid=%d; cmd_src=%d; start_index=%u; end_index=%u; max_index=%d; ret=%d)\n",
            pid_info.tgid, cmd_src, start_index, end_index, DMS_EVENT_EXCEPTION_PROCESS_MAX, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    ret = get_or_create_event_ns_stu(&event_ns_stu);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        dms_err("Get current event ns stu failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = get_old_event_proc(event_ns_stu, pid_info, start_index, end_index, event_proc);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        /* There was another thread had been registered in this process, return error */
        return ret;
    }
    /* this thread has already registered before, return the old event proc directly */
    if (*event_proc != NULL) {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        return 0;
    }

    /* this is a new thread, try to alloc a new event_proc */
    *event_proc = alloc_new_event_proc(event_ns_stu, pid_info, start_index, end_index);
    if (*event_proc != NULL) {
        event_ns_stu->process_num++;
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        return 0;
    } else {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        dms_err("Process list is full. (tgid=%d)\n", pid_info.tgid);
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }
}

static int get_event_para_from_event_proc(DMS_EVENT_PROCESS_STRU *event_proc, struct dms_event_para *fault_event)
{
    DMS_EVENT_NODE_STRU exception_node = {{0}};
    int ret = DRV_ERROR_NONE;

    ka_task_mutex_lock(&event_proc->process_mutex);
    if (ka_base_kfifo_len(&event_proc->event_fifo) < DMS_EXCEPTION_KFIFO_CELL) {
        dms_warn("The event proc does not have event. (tgid=%d; pid=%d)\n",
            event_proc->process_pid, event_proc->process_tid);
        ret = -ETIMEDOUT;
        goto out;
    }

    if (!ka_base_kfifo_out(&event_proc->event_fifo, &exception_node, DMS_EXCEPTION_KFIFO_CELL)) {
        dms_err("kfifo_out failed. (tgid=%d; pid=%d)\n", event_proc->process_pid, event_proc->process_tid);
        ret = DRV_ERROR_INNER_ERR;
        goto out;
    }

    if (memcpy_s(fault_event, sizeof(struct dms_event_para),
                 &exception_node.event, sizeof(struct dms_event_para)) != 0) {
        dms_err("Call memcpy_s failed. (tgid=%d; pid=%d)\n", event_proc->process_pid, event_proc->process_tid);
        ret = DRV_ERROR_INNER_ERR;
        goto out;
    }

out:
    event_proc->exception_num = ka_base_kfifo_len(&event_proc->event_fifo) / DMS_EXCEPTION_KFIFO_CELL;
    ka_task_mutex_unlock(&event_proc->process_mutex);
    return ret;
}

STATIC int dms_event_clear_exception_by_devid(DMS_EVENT_PROCESS_STRU *event_proc, u32 devid)
{
    DMS_EVENT_NODE_STRU exception_node = {{0}};
    struct kfifo fifo_buf;

    ka_task_mutex_lock(&event_proc->process_mutex);
    if (event_proc->process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        return DRV_ERROR_NONE;
    }

    if (ka_base_kfifo_is_empty(&event_proc->event_fifo) != 0) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        return DRV_ERROR_NONE;
    }

    if (ka_base_kfifo_alloc(&fifo_buf, DMS_EXCEPTION_KFIFO_SIZE, KA_GFP_KERNEL) != 0) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        dms_err("kfifo_alloc failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    ka_base_kfifo_reset(&fifo_buf);

    while (ka_base_kfifo_len(&event_proc->event_fifo) >= DMS_EXCEPTION_KFIFO_CELL) {
        if (!ka_base_kfifo_out(&event_proc->event_fifo, &exception_node, DMS_EXCEPTION_KFIFO_CELL)) {
            dms_warn("kfifo_out from event_proc warn.\n");
            continue;
        }
        if (exception_node.event.deviceid != (unsigned short)devid) {
            ka_base_kfifo_in(&fifo_buf, &exception_node, DMS_EXCEPTION_KFIFO_CELL);
        }
    }
    ka_base_kfifo_reset(&event_proc->event_fifo);

    while (ka_base_kfifo_len(&fifo_buf) >= DMS_EXCEPTION_KFIFO_CELL) {
        if (!ka_base_kfifo_out(&fifo_buf, &exception_node, DMS_EXCEPTION_KFIFO_CELL)) {
            dms_warn("kfifo_out from fifo_buf warn.\n");
            continue;
        }
        ka_base_kfifo_in(&event_proc->event_fifo, &exception_node, DMS_EXCEPTION_KFIFO_CELL);
    }
    event_proc->exception_num = ka_base_kfifo_len(&event_proc->event_fifo) / DMS_EXCEPTION_KFIFO_CELL;
    ka_task_mutex_unlock(&event_proc->process_mutex);
    ka_base_kfifo_free(&fifo_buf);
    return DRV_ERROR_NONE;
}

/* return > 0 means success, return 0 means no event, return < 0 means error */
static int dms_event_wait_interrupt(int timeout, DMS_EVENT_PROCESS_STRU *event_proc)
{
    int ret;

    if (timeout == DMS_EVENT_NO_TIMEOUT_FLAG) {
        do {
            ret = ka_task_wait_event_interruptible_timeout(event_proc->event_wait,
                                                   (event_proc->exception_num > 0),
                                                   ka_system_msecs_to_jiffies(DMS_EVENT_WAIT_TIME));
        } while (ret == 0);
    } else if ((timeout >= 0) && (timeout <= DMS_EVENT_WAIT_TIME_MAX)) {
        ret = ka_task_wait_event_interruptible_timeout(event_proc->event_wait,
                                               (event_proc->exception_num > 0),
                                               ka_system_msecs_to_jiffies((unsigned int)timeout));
    } else {
        dms_err("Invalid parameter. (timeout=%dms)\n", timeout);
        return -EINVAL;
    }

    return ret;
}

int dms_event_get_exception(struct dms_event_para *fault_event, int timeout, enum cmd_source cmd_src)
{
    DMS_EVENT_PROCESS_STRU *event_proc = NULL;
    struct pid_info_t pid_info;
    int ret;

    if (fault_event == NULL) {
        dms_err("Invalid parameter, fault_event is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    pid_info.tgid = current->tgid;
    pid_info.pid = current->pid;
    ret = get_current_ns_event_proc(pid_info, cmd_src, &event_proc);
    if (ret != 0 || event_proc == NULL) {
        dms_err("Alloc an event_proc failed. (tgid=%d; pid=%d; cmd_src=%d)\n", current->tgid, current->pid, cmd_src);
        return ret;
    }

    ret = dms_event_wait_interrupt(timeout, event_proc);
    if (ret < 0) {
        if (ret == -EINVAL) {
            ret = DRV_ERROR_PARA_ERROR;
        }
        return ret;
    } else if (ret == 0) {
        return -ETIMEDOUT;
    }

    ret = get_event_para_from_event_proc(event_proc, fault_event);
    if (ret == -ETIMEDOUT) {
        return ret;
    } else if (ret != 0) {
        dms_err("Get exception by process index failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_event_get_exception);

int dms_event_clear_exception(u32 devid)
{
    u32 ns_id;
    u32 idx;
    int ret;

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (ns_id = 0; ns_id <= UDA_NS_NUM; ns_id++) {
        if (g_dms_event_ns_stu[ns_id] == NULL) {
            continue;
        }

        for (idx = 0; idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; idx++) {
            ret = dms_event_clear_exception_by_devid(&g_dms_event_ns_stu[ns_id]->event_process[idx], devid);
            if (ret != 0) {
                ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
                return ret;
            }
        }
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);

    return DRV_ERROR_NONE;
}

STATIC void release_one_event_proc(DMS_EVENT_STRU_T* event_ns_stu, u32 proc_idx)
{
    ka_task_mutex_lock(&event_ns_stu->event_process[proc_idx].process_mutex);
    if (event_ns_stu->event_process[proc_idx].process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
        ka_task_mutex_unlock(&event_ns_stu->event_process[proc_idx].process_mutex);
        return;
    }

    ka_base_kfifo_free(&event_ns_stu->event_process[proc_idx].event_fifo);
    event_ns_stu->event_process[proc_idx].exception_num = 0;
    event_ns_stu->event_process[proc_idx].process_pid = DMS_INVALID_PID;
    event_ns_stu->event_process[proc_idx].process_tid = DMS_INVALID_PID;
    event_ns_stu->event_process[proc_idx].start_time = 0;
    event_ns_stu->event_process[proc_idx].process_status = DMS_EVENT_PROCESS_STATUS_IDLE;
    ka_task_mutex_unlock(&event_ns_stu->event_process[proc_idx].process_mutex);

    event_ns_stu->process_num--;
    return;
}

void dms_event_release_proc(int tgid, int pid)
{
    int ret;
    u32 proc_idx;
    u32 ns_id = UDA_NS_NUM + 1;
    DMS_EVENT_STRU_T* event_ns_stu = NULL;

    ret = dms_get_cur_ns_id(&ns_id);
    if ((ret != 0) || (ns_id > UDA_NS_NUM)) {
        /* can not printf error log, because this function may be called by a no-ns_id thread */
        return;
    }

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    event_ns_stu = g_dms_event_ns_stu[ns_id];
    if (event_ns_stu == NULL) {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        return;
    }

    for (proc_idx = 0; proc_idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; proc_idx++) {
        if ((event_ns_stu->event_process[proc_idx].process_pid == tgid) &&
            (event_ns_stu->event_process[proc_idx].process_tid == pid)) {
            release_one_event_proc(event_ns_stu, proc_idx);
            if (event_ns_stu->process_num == 0) {
                dms_event_ns_stu_free(ns_id);
                event_ns_stu = NULL;
            }
            ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
            dms_debug("Release one process. (ns_id=%d; tgid=%d; pid=%d; index=%u)\n",
                      ns_id, current->tgid, current->pid, proc_idx);
            return;
        }
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
}
KA_EXPORT_SYMBOL(dms_event_release_proc);

int dms_event_subscribe_register(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func,
    DMS_DISTRIBUTE_PRIORITY priority)
{
    u32 i;

    if ((priority >= DMS_DISTRIBUTE_PRIORITY_MAX) ||
        (handle_func == NULL)) {
        dms_err("Invalid parameter. (priority=%u; handle_func=%s).\n",
                priority, (handle_func == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = DISTRIBUTE_FUNC_HEAD_INDEX(priority); i < DISTRIBUTE_FUNC_TAIL_INDEX(priority); i++) {
        if (g_dms_event_handle_list[i] == NULL) {
            g_dms_event_handle_list[i] = handle_func;
            ka_task_mutex_unlock(&g_dms_event_handle_mutex);
            return DRV_ERROR_NONE;
        }
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);

    dms_err("The distribute handle list is full. (priority=%u).\n", priority);
    return DRV_ERROR_NO_RESOURCES;
}
KA_EXPORT_SYMBOL(dms_event_subscribe_register);

void dms_event_subscribe_unregister(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func)
{
    u32 i;

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        if (g_dms_event_handle_list[i] == handle_func) {
            g_dms_event_handle_list[i] = NULL;
            break;
        }
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
}
KA_EXPORT_SYMBOL(dms_event_subscribe_unregister);

void dms_event_subscribe_unregister_all(void)
{
    u32 i;

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        g_dms_event_handle_list[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
}

int dms_event_distribute_handle(DMS_EVENT_NODE_STRU *exception_node, DMS_DISTRIBUTE_PRIORITY priority)
{
    int ret;
    u32 i;

    if ((priority >= DMS_DISTRIBUTE_PRIORITY_MAX) ||
        (exception_node == NULL)) {
        dms_err("Invalid parameter. (priority=%u; exception=%s).\n",
                priority, (exception_node == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = DISTRIBUTE_FUNC_HEAD_INDEX(priority); i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        if (g_dms_event_handle_list[i] != NULL) {
            ret = g_dms_event_handle_list[i](exception_node);
            if (ret != 0) {
                dms_err("Distribute event failed. (handle_index=%u; ret=%d)\n", i, ret);
                ka_task_mutex_unlock(&g_dms_event_handle_mutex);
                return ret;
            }
        }
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);

    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_event_distribute_handle);

ssize_t dms_event_print_subscribe_handle(char *str)
{
    int len, avl_len = EVENT_DFX_BUF_SIZE_MAX;
    char *refill_buf = str;
    u32 i;

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe handle function begin:\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
    str += len;
    avl_len -= len;

    ka_task_mutex_lock(&g_dms_event_handle_mutex);

    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        if (g_dms_event_handle_list[i] != NULL) {
            len = snprintf_s(str, avl_len, avl_len - 1, "event handle[%u]=ok\n", i);
            EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
            str += len;
            avl_len -= len;
        }
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe handle function end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
    return str - refill_buf;
out:
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
    dms_warn("snprintf_s warn.\n");
    return 0;
}

ssize_t dms_event_print_subscribe_process(char *str)
{
    int len, avl_len = EVENT_DFX_BUF_SIZE_MAX;
    char *refill_buf = str;
    u32 ns_id;
    u32 idx;
    DMS_EVENT_PROCESS_STRU *event_proc = NULL;

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe process begin:\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
    str += len;
    avl_len -= len;

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);

    for (ns_id = 0; ns_id <= UDA_NS_NUM; ns_id++) {
        if (g_dms_event_ns_stu[ns_id] == NULL) {
            continue;
        }
        for (idx = 0; idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; idx++) {
            event_proc = &g_dms_event_ns_stu[ns_id]->event_process[idx];
            ka_task_mutex_lock(&event_proc->process_mutex);
            if (event_proc->process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
                ka_task_mutex_unlock(&event_proc->process_mutex);
                continue;
            }
            len = snprintf_s(str, avl_len, avl_len - 1, "ns_id[%u]; proc_id[%u]: tgid=%d; pid=%d; event_num=%u\n",
                             ns_id, idx, event_proc->process_pid, event_proc->process_tid, event_proc->exception_num);
            ka_task_mutex_unlock(&event_proc->process_mutex);
            EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
            str += len;
            avl_len -= len;
        }
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe process end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
    return str - refill_buf;
out:
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
    dms_warn("snprintf_s warn.\n");
    return 0;
}
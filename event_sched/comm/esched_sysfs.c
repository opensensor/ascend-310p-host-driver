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


#include "securec.h"

#include "esched.h"
#include "esched_sysfs.h"
#include "ka_fs_pub.h"
#include "ka_driver_pub.h"
#include "ka_memory_pub.h"
#include "ka_errno_pub.h"

#define SCHED_FILE_PATH "/var/log/esched_trace"
#define SCHED_FILE_MODE 0640
#define SCHED_SYSFS_EVENT_NUM 1000
#define SCHED_SYSFS_CPU_USAGE_NUM 100

STATIC u32 g_node_id = 0;

u32 cpuid_in_node;
u32 cur_pid;
u32 cur_gid;

#ifdef CFG_FEATURE_VFIO
u32 g_cur_vf = 0;
#endif

STATIC void sched_fs_set_node_id(u32 id)
{
    g_node_id = id;
}

typedef void (*pidHandle)(char *buf, ssize_t *offset, struct sched_proc_ctx *proc_ctx);

STATIC void pidListForEachHandle(char *buf, ssize_t *offset, pidHandle handle)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct pid_entry *entry = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return;
    }

    ka_task_mutex_lock(&node->pid_list_mutex);
    ka_list_for_each_entry(entry, &node->pid_list, list) {
        proc_ctx = esched_proc_get(node, entry->pid);
        if (proc_ctx == NULL) {
            continue;
        }

        handle(buf, offset, proc_ctx);
        esched_proc_put(proc_ctx);
    }
    ka_task_mutex_unlock(&node->pid_list_mutex);

    esched_dev_put(node);
}

/*
* Due to engineering problems, there will be false alarm,
* here to shield until the problem is solved.
*/
/*lint -e144 -e666 -e102 -e1112 -e145 -e151 -e514*/
STATIC void sched_sysfs_write_file(ka_file_t*file, const void *buf, size_t count, loff_t *pos)
{
    ssize_t ret;
    ret = ka_fs_kernel_write(file, buf, count, pos);
#if !defined (EVENT_SCHED_UT) && !defined (EMU_ST)
    if (ret != (ssize_t)count) {
        sched_warn("Unable to invoke the kernel_write. (count=%lu; ret=%ld)\n", count, ret);
    }
#endif
}

STATIC int32_t sched_sysfs_clear_node_stat(struct sched_numa_node *node)
{
    int32_t ret;

    ret = memset_s(&node->event_trace, sizeof(struct sched_node_event_trace), 0,
        sizeof(struct sched_node_event_trace));
    if (ret != 0) {
        sched_err("Failed to clear the event_trace node.\n");
        return ret;
    }
    node->event_trace.enable_flag = SCHED_TRACE_ENABLE;

    return 0;
}
STATIC ssize_t sched_sysfs_node_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_numa_node *node = NULL;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "node id,cpu num,sched cpu num\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < SCHED_MAX_CHIP_NUM; i++) {
        node = esched_dev_get(i);
        if (node != NULL) {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u,%u,%u\n",
                i, node->cpu_num, node->sched_cpu_num);
            if (ret >= 0) {
                offset += ret;
            }
            esched_dev_put(node);
        }
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_id_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%u\n", g_node_id);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_id_write(ka_device_t *dev, ka_device_attribute_t *attr,
                                         const char *buf, size_t count)
{
    u32 val = 0;
    struct sched_numa_node *node = NULL;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        return count;
    }

    node = sched_get_numa_node(val);
    if (node == NULL) {
        sched_err("The node_id is invalid. (node_id=%u)\n", val);
        return count;
    }

    g_node_id = val;
    sched_info("Set node_id %u.\n", val);
    return (ssize_t)count;
}

STATIC ssize_t sched_sysfs_node_debug_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "0x%x\n", node->debug_flag);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "bit0: clear node event trace, bit1:"
        "record cpu usage and event num sample data, bit2: support trigger sched event trace, bit3: record all event"
        "trace (only valid when bit2 = 1)\n");
    if (ret >= 0) {
        offset += ret;
    }

    esched_dev_put(node);

    return offset;
}

void sched_sysfs_node_debug_uninit(struct sched_numa_node *node)
{
    if (node->node_event_sample != NULL) {
        sched_vfree(node->node_event_sample);
        node->node_event_sample = NULL;
    }

    if (node->proc_event_sample != NULL) {
        sched_vfree(node->proc_event_sample);
        node->proc_event_sample = NULL;
    }
}

STATIC int32_t sched_sysfs_node_debug_init(struct sched_numa_node *node)
{
    if (node->node_event_sample == NULL) {
        node->node_event_sample = (struct sched_event_sample *)sched_vzalloc(sizeof(struct sched_event_sample));
        if (node->node_event_sample == NULL) {
            sched_err("Failed to vzalloc memory. (size=0x%lx)\n", sizeof(struct sched_event_sample));
            return -ENOMEM;
        }
    }

    if (node->proc_event_sample == NULL) {
        node->proc_event_sample = (struct sched_event_sample *)sched_vzalloc(sizeof(struct sched_event_sample));
        if (node->proc_event_sample == NULL) {
            sched_err("Failed to vzalloc memory. (size=0x%lx)\n", sizeof(struct sched_event_sample));
            return -ENOMEM;
        }
    }

    return 0;
}

STATIC ssize_t sched_sysfs_node_debug_write(ka_device_t *dev, ka_device_attribute_t *attr,
                                            const char *buf, size_t count)
{
    u32 val = 0;
    int32_t ret;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        sched_err("Dev not valid. (devid=%u)\n", g_node_id);
        return count;
    }

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        esched_dev_put(node);
        return count;
    }

    if ((val == 0) && (node->node_event_sample == NULL) && (node->proc_event_sample == NULL)) {
#if !defined (EVENT_SCHED_UT) && !defined (EMU_ST)
        sched_info("Set node dbg %u.\n", val);
#endif
        esched_dev_put(node);
        return count;
    }

    ka_task_mutex_lock(&node->node_guard_work_mutex);
    if (val != 0) {
        ret = sched_sysfs_node_debug_init(node);
        if (ret != 0) {
            ka_task_mutex_unlock(&node->node_guard_work_mutex);
            sched_err("Failed to invoke the sched_sysfs_node_debug_init. (node_id=%u; ret=%d)\n",
                node->node_id, ret);
            esched_dev_put(node);
            return ret;
        }
    }

    if ((val & (0x1 << SCHED_DEBUG_EVENT_TRACE_BIT)) != 0) {
        (void)sched_sysfs_clear_node_stat(node);
    }

    if ((val & (0x1 << SCHED_DEBUG_SAMPLE_BIT)) != 0) {
        sched_sysfs_clear_sample_data(node);
    } else if ((val & (0x1 << SCHED_DEBUG_TRIGGER_SAMPLE_FILE_BIT)) != 0) {
        sched_sysfs_record_sample_data(node);
    }

    if ((val & (0x1 << SCHED_DEBUG_SCHED_TRACE_RECORD_BIT)) != 0) {
        node->trace_record.num = 0;
    }

    node->debug_flag = val;
    if (val == 0) {
        sched_sysfs_node_debug_uninit(node);
    }
    ka_task_mutex_unlock(&node->node_guard_work_mutex);
    sched_info("Set node dbg %u.\n", val);
    esched_dev_put(node);

    return count;
}

STATIC ssize_t sched_sysfs_node_cpu_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "cpuid,sched_mode,"
        "sched stat:sched_event_num,check_sched_num,proc_exit_drop_num,exclusive_sched_num,timeout_cnt\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        struct sched_cpu_ctx *cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        struct sched_cpu_stat *stat = &cpu_ctx->stat;
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "%u,sched,%llu,%llu,%llu,%llu,%llu\n", cpu_ctx->cpuid, stat->sched_event_num,
            stat->check_sched_num, stat->proc_exit_drop_num, stat->exclusive_sched_num, stat->timeout_cnt);
        if (ret >= 0) {
            offset += ret;
        }
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "cpuid,"
        "thread(pid,gid,task_id,tid,event_id,sub_event_id,cpu_sched_time,curr_timestamp)\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        struct sched_cpu_ctx *cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        struct esched_abnormal_thread_record *record = &cpu_ctx->cpu_abnormal_thread;
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "%u,(%d,%u,%u,%u,%u,%u,%llu,%llu)\n", cpu_ctx->cpuid,
            record->thread_info.pid, record->thread_info.gid, record->thread_info.task_id, record->thread_info.tid,
            record->event_id, record->sub_event_id, record->cpu_sched_time, record->curr_timestamp);
        if (ret >= 0) {
            offset += ret;
        }
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_cpu_mask(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    u32 i;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u\n", node->sched_cpuid[i]);
        if (ret >= 0) {
            offset += ret;
        }
    }

    esched_dev_put(node);

    return offset;
}

STATIC void sched_sysfs_node_get_proc_list(char *buf, ssize_t *offset, struct sched_proc_ctx *proc_ctx)
{
    int ret = snprintf_s(buf + *offset, KA_MM_PAGE_SIZE - *offset, KA_MM_PAGE_SIZE - *offset - 1, "%d,%s,%d,%d,%d\n",
        proc_ctx->pid, proc_ctx->name, proc_ctx->refcnt,
        ka_base_atomic_read(&proc_ctx->publish_event_num), ka_base_atomic_read(&proc_ctx->sched_event_num));
    if (ret >= 0) {
        *offset += ret;
    }
}

STATIC ssize_t sched_sysfs_node_proc_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;

    pidListForEachHandle(buf, &offset, sched_sysfs_node_get_proc_list);

    return offset;
}

STATIC ssize_t sched_sysfs_node_del_proc_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ka_task_mutex_lock(&node->proc_mng_mutex);

    if (!ka_list_empty_careful(&node->del_proc_head)) {
        ka_list_for_each_safe(pos, n, &node->del_proc_head) {
            proc_ctx = ka_list_entry(pos, struct sched_proc_ctx, list);

            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%d,%d\n",
                proc_ctx->pid, proc_ctx->refcnt);
            if (ret >= 0) {
                offset += ret;
            }
        }
    }

    ka_task_mutex_unlock(&node->proc_mng_mutex);

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_event_resource(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_event_que *res_que = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "resource total:use:que head:que tail,max use,enque_full,deque empty\n");
    if (ret >= 0) {
        offset += ret;
    }

    res_que = &node->event_res;
    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u:%u:%u:%u:%u:%llu:%llu\n",
        res_que->depth, res_que->depth - sched_que_element_num(res_que), res_que->head, res_que->tail,
        res_que->stat.max_use, res_que->stat.enque_full, res_que->stat.deque_empty);
    if (ret >= 0) {
        offset += ret;
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_event_que_trace(struct sched_event *event_base,
    struct sched_event_que *que, char *buf, ssize_t offset, const char *que_name, u32 que_id)
{
    u32 i, j, flag;
    int32_t ret;
    struct sched_event *event = NULL;

    if ((que->depth - sched_que_element_num(que)) == 0) {
        return offset;
    }

    for (i = 0; i < que->depth; i++) {
        event = &event_base[i];
        flag = SCHED_INVALID;
        for (j = que->head; j < que->tail; j++) {
            if (event == que->ring[j & (que->mask)]) {
                flag = SCHED_VALID;
                break;
            }
        }

        if (flag == SCHED_INVALID) {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%s(%u),%u,%u,%u,%u,%u\n",
                que_name, que_id, i, event->trace.type, event->trace.a, event->trace.b, event->trace.c);
            if (ret >= 0) {
                offset += ret;
            }
        }
    }

    return offset;
}

STATIC ssize_t sched_sysfs_event_trace(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "que name(que id),event index,sched type,dest_pid,dest_gid,event_pri\n");
    if (ret >= 0) {
        offset += ret;
    }

    offset = sched_sysfs_event_que_trace(node->event_base, &node->event_res, buf, offset, "node", 0);

    for (i = 0; i < node->sched_cpu_num; i++) {
        u32 cpuid = node->sched_cpuid[i];
        cpu_ctx = sched_get_cpu_ctx(node, cpuid);
        offset = sched_sysfs_event_que_trace(cpu_ctx->event_base, &cpu_ctx->event_res, buf, offset, "cpu", cpuid);
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_sample_period_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%u ms\n", node->sample_period);
    if (ret >= 0) {
        offset += ret;
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_sample_period_write(ka_device_t *dev,
                                               ka_device_attribute_t *attr,
                                               const char *buf,
                                               size_t count)
{
    u32 val = 0;
    struct sched_numa_node *node = NULL;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        return count;
    }

    if ((val < SCHED_SAMPLE_MIN_PERIOD) || (val > SCHED_SAMPLE_MAX_PERIOD)) {
        sched_err("The value of variable val is out of range. (val=%u)\n", val);
        return count;
    }

    val = val / SCHED_SAMPLE_MIN_PERIOD * SCHED_SAMPLE_MIN_PERIOD;

    node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return count;
    }

    node->sample_period = val;
    node->sample_interval = val / SCHED_GUARD_WORK_PERIOD;
    esched_dev_put(node);
    sched_info("Set sample_period %u.\n", val);
    return count;
}

STATIC ssize_t sched_sysfs_event_sample_format(const char *period, char *buf, u32 buf_len, u32 idx, u32 pre_index,
    struct sched_event_sample_data *data)
{
    int32_t i, ret, submit_speed, sched_speed;
    ssize_t offset = 0;
    u64 duration, total_event_num, cur_event_num, event_num;

    duration = tick_to_millisecond(data[idx].timestamp - data[pre_index].timestamp);
    total_event_num = data[idx].total_event_num - data[pre_index].total_event_num;
    cur_event_num = data[idx].cur_event_num - data[pre_index].cur_event_num;

    if (duration == 0) {
        return offset;
    }

    submit_speed = (int)((total_event_num * SCHED_SYSFS_EVENT_NUM) / duration);
    sched_speed = (int)(((total_event_num - cur_event_num) * SCHED_SYSFS_EVENT_NUM) / duration);

    if (ka_base_strlen(period) > 0) {
        ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1,
            "%s,%llu,%d,%d", period, data[idx].timestamp, submit_speed, sched_speed);
    } else {
        ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1,
            "%llu,%llu,%llu", data[idx].timestamp, total_event_num, data[idx].cur_event_num);
    }
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0 ; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        if (ka_base_strlen(period) > 0) {
            event_num = data[idx].publish_event_num[i] - data[pre_index].sched_event_num[i];
            submit_speed = (int)((event_num * SCHED_SYSFS_EVENT_NUM) / duration);
            ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1, ",%d", submit_speed);
        } else {
            ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1,
                ",(%d,%llu,%llu)", i, data[idx].publish_event_num[i] -
                data[pre_index].publish_event_num[i], data[idx].publish_event_num[i] -
                data[idx].sched_event_num[i]);
        }
        if (ret >= 0) {
            offset += ret;
        }
    }

    ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1, "\n");
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC int32_t sched_sysfs_record_event_sample_data(struct sched_numa_node *node, u64 timestamp,
    struct sched_event_sample *sample_data, SAMPLE_TYPE_VALUE type)
{
    struct sched_proc_ctx *proc_stat = NULL;
    ka_file_t *fp = NULL;
    char *buf = NULL;
    int32_t ret;
    loff_t offset_tmp = 0;
    u32 i;

    // check the input argument
    if (sample_data == NULL) {
        sched_err("The sample_data is NULL.\n");
        return DRV_ERROR_INNER_ERR;
    }

#if !defined (EVENT_SCHED_UT) && !defined (EMU_ST)
    buf = (char *)sched_kzalloc(KA_MM_PAGE_SIZE, KA_GFP_KERNEL);
    if (buf == NULL) {
        sched_err("Failed to alloc memory.(size=%lx)\n", KA_MM_PAGE_SIZE);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (type == NODE_SAMPLE_TYPE) {
        ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%s/event_sample_%llu", SCHED_FILE_PATH, timestamp);
    } else {
        proc_stat = esched_proc_get(node, node->sample_proc_id);
        if (proc_stat == NULL) {
            sched_kfree(buf);
            return 0;
        }
        ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%s/event_sample_pid_%d_%llu", SCHED_FILE_PATH,
            proc_stat->pid, timestamp);
        esched_proc_put(proc_stat);
    }

    if (ret < 0) {
        sched_err("Failed to invoke the snprintf_s.\n");
        sched_kfree(buf);
        return ret;
    }

    fp = ka_fs_filp_open(buf, KA_O_RDWR | KA_O_TRUNC | KA_O_CREAT, SCHED_FILE_MODE);
    if (KA_IS_ERR(fp)) {
        sched_err("Failed to invoke the filp_open. (buf=\"%s\")\n", buf);
        sched_kfree(buf);
        return DRV_ERROR_INNER_ERR;
    }

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "timestamp, publish_event_num, cur_event_num,"
        "(event_id, publish_event_num, cur_event_num).....\n");
    if (ret > 0) {
        sched_sysfs_write_file(fp, (const void *)buf, ka_base_strlen(buf), &offset_tmp);
    }

    for (i = 1; i < sample_data->record_num; i++) {
        ret = sched_sysfs_event_sample_format("", buf, KA_MM_PAGE_SIZE, i, i - 1, sample_data->data);
        if (ret > 0) {
            sched_sysfs_write_file(fp, (const void *)buf, ka_base_strlen(buf), &offset_tmp);
        }
    }

    sched_kfree(buf);
#endif
    (void)ka_fs_filp_close(fp, NULL);

    return 0;
}

STATIC ssize_t sched_sysfs_event_sample(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 idx, pre_index, period;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_event_sample *sample = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ka_task_mutex_lock(&node->node_guard_work_mutex);
    sample = node->node_event_sample;
    if (sample == NULL) {
        ka_task_mutex_unlock(&node->node_guard_work_mutex);
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "timestamp, publish_speed(num/s), sched_speed(num/s), each type\n");
    if (ret >= 0) {
        offset += ret;
    }

    if (sample->record_num == 0) {
        ka_task_mutex_unlock(&node->node_guard_work_mutex);
        esched_dev_put(node);
        return offset;
    }

    idx = sample->record_num - 1;

    period = node->sample_period;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_event_sample_format("single", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index,
            sample->data);
    }

    period = SYS_SCHED_PERIOD_TIME_1S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_event_sample_format("1s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index,
            sample->data);
    }

    period = SYS_SCHED_PERIOD_TIME_10S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_event_sample_format("10s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index,
            sample->data);
    }

    period = SYS_SCHED_PERIOD_TIME_60S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_event_sample_format("60s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index,
            sample->data);
    }

    period = SYS_SCHED_PERIOD_TIME_300S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_event_sample_format("300s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index,
            sample->data);
    }

    ka_task_mutex_unlock(&node->node_guard_work_mutex);
    esched_dev_put(node);
    return offset;
}

STATIC ssize_t sched_sysfs_cpu_usage_sample_format(struct sched_cpu_sample *sample_data,
    char *buf, u32 buf_len, u32 idx, u32 pre_index)
{
    int32_t usage;
    u64 duration, total_use_time;

    duration = sample_data->data[idx].timestamp - sample_data->data[pre_index].timestamp;
    if (duration == 0) {
        return 0;
    }

    total_use_time = sample_data->data[idx].total_use_time - sample_data->data[pre_index].total_use_time;

    usage = (int)((total_use_time * SCHED_SYSFS_CPU_USAGE_NUM) / duration);
    if (usage > SCHED_SYSFS_CPU_USAGE_NUM) {
        usage = SCHED_SYSFS_CPU_USAGE_NUM;
    }
    return snprintf_s(buf, buf_len, buf_len - 1, "%llu,%d\n", sample_data->data[idx].timestamp, usage);
}

STATIC int32_t sched_sysfs_record_cpu_sample_data(u64 timestamp, int32_t cpu_id, struct sched_cpu_sample *sample_data)
{
    ka_file_t *fp = NULL;
    char buf[MAX_LENTH];
    loff_t offset_tmp = 0;
    int32_t ret;
    u32 i;

    ret = snprintf_s(buf, MAX_LENTH, MAX_LENTH - 1, "%s/cpu_%d_sample_%llu", SCHED_FILE_PATH, cpu_id, timestamp);
    if (ret < 0) {
        sched_err("Failed to invoke the snprintf_s.\n");
        return ret;
    }

    fp = ka_fs_filp_open(buf, KA_O_RDWR | KA_O_TRUNC | KA_O_CREAT, SCHED_FILE_MODE);
    if (KA_IS_ERR(fp)) {
        sched_err("Failed to invoke the filp_open.\n");
        return DRV_ERROR_INNER_ERR;
    }

    ret = snprintf_s(buf, MAX_LENTH, MAX_LENTH - 1, "timestamp, cpu_usage(percent)\n");
    if (ret > 0) {
        sched_sysfs_write_file(fp, (const void *)buf, ka_base_strlen(buf), &offset_tmp);
    }

    for (i = 1; i < sample_data->record_num; i++) {
        ret = sched_sysfs_cpu_usage_sample_format(sample_data, buf, MAX_LENTH, i, i - 1);
        if (ret > 0) {
            sched_sysfs_write_file(fp, (const void *)buf, ka_base_strlen(buf), &offset_tmp);
        }
    }

    (void)ka_fs_filp_close(fp, NULL);

    return 0;
}

STATIC ssize_t sched_sysfs_cpu_usage_sample_period(const char *period, char *buf, u32 buf_len, u32 idx, u32 pre_index)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_cpu_sample *sample = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1, "%s\n", period);
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);

        sample = cpu_ctx->sample;
        if (sample == NULL) {
            continue;
        }

        ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1, "%d,", cpu_ctx->cpuid);
        if (ret >= 0) {
            offset += ret;
        }

        offset += sched_sysfs_cpu_usage_sample_format(sample, buf + offset, buf_len - offset, idx, pre_index);
    }

    ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1, "\n");
    if (ret >= 0) {
        offset += ret;
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_cpu_usage_sample(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 idx, pre_index, period;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_event_sample *sample = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ka_task_mutex_lock(&node->node_guard_work_mutex);
    sample = node->node_event_sample; /* use event sample record index */
    if (sample == NULL) {
        ka_task_mutex_unlock(&node->node_guard_work_mutex);
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "cpuid,timestamp,usage(%%)\n");
    if (ret >= 0) {
        offset += ret;
    }

    if (sample->record_num == 0) {
        ka_task_mutex_unlock(&node->node_guard_work_mutex);
        esched_dev_put(node);
        return offset;
    }

    idx = sample->record_num - 1;

    period = node->sample_period;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_cpu_usage_sample_period("single", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index);
    }

    period = SYS_SCHED_PERIOD_TIME_1S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_cpu_usage_sample_period("1s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index);
    }

    period = SYS_SCHED_PERIOD_TIME_10S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_cpu_usage_sample_period("10s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index);
    }

    period = SYS_SCHED_PERIOD_TIME_60S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_cpu_usage_sample_period("60s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index);
    }

    period = SYS_SCHED_PERIOD_TIME_300S;
    if (idx >= (period / node->sample_period)) {
        pre_index = idx - (period / node->sample_period);
        offset += sched_sysfs_cpu_usage_sample_period("300s", buf + offset, KA_MM_PAGE_SIZE - offset, idx, pre_index);
    }

    ka_task_mutex_unlock(&node->node_guard_work_mutex);
    esched_dev_put(node);
    return offset;
}

void sched_sysfs_record_sample_data(struct sched_numa_node *node)
{
    u64 timestamp = sched_get_cur_timestamp();
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_cpu_sample *sample = NULL;
    int32_t ret;
    u32 i;

    ret = sched_sysfs_record_event_sample_data(node, timestamp, node->node_event_sample, NODE_SAMPLE_TYPE);
    if (ret != 0) {
        sched_info("Invoke the sched_sysfs_record_event_sample_data. (ret=%d, type=%d)\n", ret, NODE_SAMPLE_TYPE);
    }

    ret = sched_sysfs_record_event_sample_data(node, timestamp, node->proc_event_sample, PID_SAMPLE_TYPE);
    if (ret != 0) {
        sched_info("Invoke the sched_sysfs_record_event_sample_data. (ret=%d, type=%d)\n", ret, PID_SAMPLE_TYPE);
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        sample = cpu_ctx->sample;
        if (sample != NULL) {
            (void)sched_sysfs_record_cpu_sample_data(timestamp, cpu_ctx->cpuid, sample);
        }
    }
}

void sched_sysfs_clear_sample_data(struct sched_numa_node *node)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_cpu_sample *sample = NULL;
    u32 i;

    node->node_event_sample->record_num = 0;
    node->proc_event_sample->record_num = 0;

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        sample = cpu_ctx->sample;
        if (sample != NULL) {
            sample->record_num = 0;
        }
    }

    sched_info("End of calling sched_sysfs_clear_sample_data. "
               "(cur_timestamp=%llu; frequence=%llu)\n", sched_get_cur_timestamp(), sched_get_sys_freq());
}

STATIC ssize_t sched_sysfs_node_sched_abnormal_time_read(char *buf, u64 timestamp_thres)
{
    int32_t ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%llu (us)\n", tick_to_microsecond(timestamp_thres));
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_abnormal_time_write(const char *buf,
    size_t count, u64 *timestamp_thres)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        return count;
    }

    *timestamp_thres = microsecond_to_tick((u64)val);
    sched_info("Set abnormal_time %u.\n", val);
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_proc_abnormal_time_read(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    size_t count = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_read(buf, node->abnormal_event.proc.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_proc_abnormal_time_write(ka_device_t *dev,
    ka_device_attribute_t *attr, const char *buf, size_t count)
{
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_write(buf, count, &node->abnormal_event.proc.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_publish_syscall_abnormal_time_read(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    size_t count = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_read(buf, node->abnormal_event.publish_syscall.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_publish_syscall_abnormal_time_write(ka_device_t *dev,
    ka_device_attribute_t *attr, const char *buf, size_t count)
{
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_write(buf, count,
            &node->abnormal_event.publish_syscall.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_publish_in_kernel_abnormal_time_read(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    size_t count = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_read(buf, node->abnormal_event.publish_in_kernel.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_publish_in_kernel_abnormal_time_write(ka_device_t *dev,
    ka_device_attribute_t *attr, const char *buf, size_t count)
{
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_write(buf, count,
            &node->abnormal_event.publish_in_kernel.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_wakeup_abnormal_time_read(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    size_t count = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_read(buf, node->abnormal_event.wakeup.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_wakeup_abnormal_time_write(ka_device_t *dev,
    ka_device_attribute_t *attr, const char *buf, size_t count)
{
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_write(buf, count, &node->abnormal_event.wakeup.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_publish_subscribe_abnormal_time_read(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    size_t count = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_read(buf, node->abnormal_event.publish_subscribe.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_publish_subscribe_abnormal_time_write(ka_device_t *dev,
    ka_device_attribute_t *attr, const char *buf, size_t count)
{
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_write(buf, count,
            &node->abnormal_event.publish_subscribe.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_thread_run_abnormal_time_read(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    size_t count = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_read(buf, node->abnormal_event.thread_run.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_thread_run_abnormal_time_write(ka_device_t *dev,
    ka_device_attribute_t *attr, const char *buf, size_t count)
{
    struct sched_numa_node *node = NULL;
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        return count;
    }

    if (val < (SCHED_GUARD_WORK_PERIOD * MILLISECOND_TO_MICROSECOND)) {
        sched_err("The value of unit us must be large than 10ms. (val=%u)\n", val);
        return count;
    }

    node = esched_dev_get(g_node_id);
    if (node != NULL) {
        count = sched_sysfs_node_sched_abnormal_time_write(buf, count,
            &node->abnormal_event.thread_run.timestamp_thres);
        esched_dev_put(node);
    }
    return count;
}

STATIC int32_t sched_sysfs_node_single_event_info(char *buf, struct sched_abnormal_event_item *event_info,
    struct sched_event_timestamp *timestamp, ssize_t *offset, u32 idx, ABNORMAL_EVENT_TYPE_VALUE type)
{
    u64 wakeup_to_waked;
    u64 wakeup_offset;
    s64 subscribe_in_kernel;
    int32_t ret;
    char *format_string[] = {
        "%d,%llu,%llu,%llu,%llu,%llu,%lld,%llu,%llu,%llu,(%llu),%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
        "%d,%llu,%llu,%llu,%llu,%llu,%lld,(%llu),%llu,%llu,%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
        "%d,%llu,%llu,%llu,%llu,%llu,%lld,%llu,%llu,(%llu),%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
        "%d,%llu,%llu,%llu,%llu,%llu,%lld,%llu,%llu,%llu,%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n"
    };

    wakeup_offset = 0;
    if (timestamp->publish_wakeup != 0) {
        wakeup_offset = tick_to_microsecond(timestamp->publish_wakeup - timestamp->publish_user);
    }

    wakeup_to_waked = 0;
    if (timestamp->publish_wakeup != 0) {
        wakeup_to_waked = tick_to_microsecond(timestamp->subscribe_waked - timestamp->publish_wakeup);
    }
    subscribe_in_kernel = (timestamp->subscribe_in_kernel >= timestamp->publish_user) ?
        tick_to_microsecond(timestamp->subscribe_in_kernel - timestamp->publish_user) :
        tick_to_microsecond((timestamp->publish_user - timestamp->subscribe_in_kernel) * (-1));

    ret = snprintf_s(buf + *offset, KA_MM_PAGE_SIZE - *offset, KA_MM_PAGE_SIZE - *offset - 1,
        format_string[type], idx, timestamp->publish_user, timestamp->publish_user_of_day,
        tick_to_microsecond(timestamp->publish_in_kernel - timestamp->publish_user), wakeup_offset,
        tick_to_microsecond(timestamp->publish_out_kernel - timestamp->publish_user), subscribe_in_kernel,
        wakeup_to_waked, tick_to_microsecond(timestamp->subscribe_waked - timestamp->publish_user),
        tick_to_microsecond(timestamp->subscribe_out_kernel - timestamp->publish_user),
        tick_to_microsecond(event_info->proc_time),
        event_info->event_id, event_info->publish_pid, event_info->publish_cpuid,
        event_info->wait_event_num_in_que, event_info->pid, event_info->gid, event_info->tid,
        event_info->bind_cpuid, event_info->kernel_tid, event_info->name);
    if (ret >= 0) {
        *offset += ret;
    }

    return ret;
}

STATIC ssize_t sched_sysfs_node_event_all_info(char *buf, struct sched_abnormal_event_item *events,
    int32_t cur_id, u32 event_num, ABNORMAL_EVENT_TYPE_VALUE type)
{
    static u32 i = 0; /* output msg is large than 1 page */
    int32_t ret;
    ssize_t offset = 0;

    struct sched_abnormal_event_item *event_info = NULL;
    struct sched_event_timestamp *timestamp = NULL;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "record index %d\n\n", cur_id);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "index,publish_user,publish_user_of_day,publish_syscall(+),publish_to_wakeup(+),publish_finish(+),"
        "publish_to_wait_start(+),wakeup_to_waked(+),publish_to_waked(+),publish_to_wait_end(+),proc time(us),event id,"
        "publish_pid,publish_cpuid,wait_event_num_in_que,pid,gid,tid,bind cpuid,kernel tid,thread name\n");
    if (ret >= 0) {
        offset += ret;
    }

    if (i == event_num) {
        i = 0;
    }

    for (; i < event_num; i++) {
        event_info = &events[i];
        timestamp = &event_info->timestamp;
        if (timestamp->publish_user == 0) {
            continue;
        }

        ret = sched_sysfs_node_single_event_info(buf, event_info, timestamp, &offset, i, type);
        if (ret < 0) {
            break;
        }
    }

    return offset;
}
STATIC ssize_t sched_sysfs_node_sched_abnormal_event_clear(ka_device_t *dev,
    ka_device_attribute_t *attr, const char *buf, size_t count)
{
    u32 val = 0;
    struct sched_node_abnormal_event *abnormal_event = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return count;
    }

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        esched_dev_put(node);
        return count;
    }

    if (val == 1) {
        abnormal_event = &node->abnormal_event;
        (void)memset_s(abnormal_event, sizeof(struct sched_node_abnormal_event), 0,
            sizeof(struct sched_node_abnormal_event));

        /* After the exception event information is cleared, the threshold is set to the default value */
        abnormal_event->publish_syscall.timestamp_thres =
            microsecond_to_tick(SCHED_DEFAULT_PUBLISH_SYSCALL_TIME_THRES);
        abnormal_event->publish_in_kernel.timestamp_thres =
            microsecond_to_tick(SCHED_DEFAULT_PUBLISH_IN_KERNEL_TIME_THRES);
        abnormal_event->wakeup.timestamp_thres =
            microsecond_to_tick(SCHED_DEFAULT_WAKEUP_TIME_THRES);
        abnormal_event->publish_subscribe.timestamp_thres =
            microsecond_to_tick(SCHED_DEFAULT_PUBLISH_SUBSCRIBE_TIME_THRES);
        abnormal_event->proc.timestamp_thres =
            microsecond_to_tick(SCHED_DEFAULT_PROC_TIME_THRES);
        abnormal_event->thread_run.timestamp_thres =
            microsecond_to_tick(SCHED_DEFAULT_PROC_TIME_THRES);
    }

    esched_dev_put(node);
    sched_info("Set abnormal_event %u.\n", val);
    return count;
}

STATIC ssize_t sched_sysfs_node_sched_proc_abnormal_event(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        offset = sched_sysfs_node_event_all_info(buf, node->abnormal_event.proc.event_info,
            ka_base_atomic_read(&node->abnormal_event.proc.cur_index), SCHED_ABNORMAL_EVENT_MAX_NUM, PROC_ABNORMAL_EVENT);
        esched_dev_put(node);
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_thread_run_abnormal_event(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        offset = sched_sysfs_node_event_all_info(buf, node->abnormal_event.thread_run.event_info,
            ka_base_atomic_read(&node->abnormal_event.thread_run.cur_index),
            SCHED_ABNORMAL_EVENT_MAX_NUM, PROC_ABNORMAL_EVENT);
        esched_dev_put(node);
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_publish_syscall_abnormal_event(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    static u32 i = 0; /* output msg is large than 1 page */
    int32_t ret;
    ssize_t offset = 0;
    struct sched_abnormal_event_item *event_info = NULL;
    struct sched_event_timestamp *timestamp = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "record index %d\n\n", ka_base_atomic_read(&node->abnormal_event.publish_syscall.cur_index));
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "index,publish_user,publish_syscall(+),event id,publish_pid,publish_cpuid\n");
    if (ret >= 0) {
        offset += ret;
    }

    if (i == SCHED_ABNORMAL_EVENT_MAX_NUM) {
        i = 0;
    }

    for (; i < SCHED_ABNORMAL_EVENT_MAX_NUM; i++) {
        event_info = &node->abnormal_event.publish_syscall.event_info[i];
        timestamp = &event_info->timestamp;
        if (timestamp->publish_user == 0) {
            continue;
        }

        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%d,%llu,%llu,%d,%d,%d\n", i,
            timestamp->publish_user, tick_to_microsecond(timestamp->publish_in_kernel - timestamp->publish_user),
            event_info->event_id, event_info->publish_pid, event_info->publish_cpuid);
        if (ret >= 0) {
            offset += ret;
        } else {
            break;
        }
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_publish_in_kernel_abnormal_event(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    static u32 i = 0; /* output msg is large than 1 page */
    int32_t ret;
    ssize_t offset = 0;
    struct sched_abnormal_event_item *event_info = NULL;
    struct sched_event_timestamp *timestamp = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "record index %d\n\n", ka_base_atomic_read(&node->abnormal_event.publish_in_kernel.cur_index));
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "index,publish_user,publish_syscall(+),publish_finish(+),event id,publish_pid,publish_cpuid\n");
    if (ret >= 0) {
        offset += ret;
    }

    if (i == SCHED_ABNORMAL_EVENT_MAX_NUM) {
        i = 0;
    }

    for (; i < SCHED_ABNORMAL_EVENT_MAX_NUM; i++) {
        event_info = &node->abnormal_event.publish_in_kernel.event_info[i];
        timestamp = &event_info->timestamp;
        if (timestamp->publish_user == 0) {
            continue;
        }

        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%d,%llu,%llu,%llu,%d,%d,%d\n", i,
            timestamp->publish_user, tick_to_microsecond(timestamp->publish_in_kernel - timestamp->publish_user),
            tick_to_microsecond(timestamp->publish_out_kernel - timestamp->publish_user),
            event_info->event_id, event_info->publish_pid, event_info->publish_cpuid);
        if (ret >= 0) {
            offset += ret;
        } else {
            break;
        }
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_wakeup_abnormal_event(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        offset = sched_sysfs_node_event_all_info(buf, node->abnormal_event.wakeup.event_info,
            ka_base_atomic_read(&node->abnormal_event.wakeup.cur_index), SCHED_ABNORMAL_EVENT_MAX_NUM, WAKEUP_ABNORMAL_EVENT);
        esched_dev_put(node);
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_publish_subscribe_abnormal_event(ka_device_t *dev,
    ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        offset = sched_sysfs_node_event_all_info(buf, node->abnormal_event.publish_subscribe.event_info,
            ka_base_atomic_read(&node->abnormal_event.publish_subscribe.cur_index), SCHED_ABNORMAL_EVENT_MAX_NUM,
            PUBLISH_SUBSCRIBE_ABNORMAL_EVENT);
        esched_dev_put(node);
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_sched_event_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        offset = sched_sysfs_node_event_all_info(buf, node->event_trace.event_info,
            ka_base_atomic_read(&node->event_trace.cur_index), SCHED_EVENT_TRACE_MAX_NUM, NORMAL_EVENT);
        esched_dev_put(node);
    }

    return offset;
}

STATIC void sched_sysfs_get_all_cpu_event_summary(struct sched_cpu_perf_stat *perf_stat)
{
    u32 i, j;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return;
    }

    for (i = 0; i < node->cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, i);
        if (cpu_ctx == NULL) {
            continue;
        }

        for (j = 0 ; j < SCHED_MAX_EVENT_TYPE_NUM; j++) {
            perf_stat->publish_event_num[j] += cpu_ctx->perf_stat.publish_event_num[j];
            perf_stat->publish_fail_event_num[j] += cpu_ctx->perf_stat.publish_fail_event_num[j];
            perf_stat->sched_event_num[j] += cpu_ctx->perf_stat.sched_event_num[j];
        }
        perf_stat->total_publish_event_num += cpu_ctx->perf_stat.total_publish_event_num;
        perf_stat->total_publish_fail_event_num += cpu_ctx->perf_stat.total_publish_fail_event_num;
        perf_stat->total_sched_event_num += cpu_ctx->perf_stat.total_sched_event_num;
        perf_stat->total_wakeup_event_num += cpu_ctx->perf_stat.total_wakeup_event_num;
        perf_stat->wakeup_total_time += cpu_ctx->perf_stat.wakeup_total_time;
        perf_stat->publish_in_kernel_total_time += cpu_ctx->perf_stat.publish_in_kernel_total_time;
        perf_stat->publish_subscribe_total_time += cpu_ctx->perf_stat.publish_subscribe_total_time;
        perf_stat->publish_syscall_total_time += cpu_ctx->perf_stat.publish_syscall_total_time;

        if (cpu_ctx->perf_stat.publish_in_kernel_max_time > perf_stat->publish_in_kernel_max_time) {
            perf_stat->publish_in_kernel_max_time = cpu_ctx->perf_stat.publish_in_kernel_max_time;
        }
        if (cpu_ctx->perf_stat.publish_subscribe_max_time > perf_stat->publish_subscribe_max_time) {
            perf_stat->publish_subscribe_max_time = cpu_ctx->perf_stat.publish_subscribe_max_time;
        }
        if (cpu_ctx->perf_stat.publish_syscall_max_time > perf_stat->publish_syscall_max_time) {
            perf_stat->publish_syscall_max_time = cpu_ctx->perf_stat.publish_syscall_max_time;
        }
        if (cpu_ctx->perf_stat.wakeup_max_time > perf_stat->wakeup_max_time) {
            perf_stat->wakeup_max_time = cpu_ctx->perf_stat.wakeup_max_time;
        }
    }

    esched_dev_put(node);
}

STATIC ssize_t sched_sysfs_node_event_summary(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_cpu_perf_stat *perf_stat = NULL;

    perf_stat = (struct sched_cpu_perf_stat *)sched_kzalloc(sizeof(struct sched_cpu_perf_stat), KA_GFP_KERNEL);
    if (perf_stat == NULL) {
        sched_err("Failed to alloc memory.(size=%lx)\n", sizeof(struct sched_cpu_perf_stat));
        return offset;
    }

    sched_sysfs_get_all_cpu_event_summary(perf_stat);

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "publish_event_num %llu \ntotal_publish_fail_event_num %llu\nsched_event_num %llu \nwakeup_event_num %llu\n\n",
        perf_stat->total_publish_event_num, perf_stat->total_publish_fail_event_num, perf_stat->total_sched_event_num, perf_stat->total_wakeup_event_num);
    if (ret >= 0) {
        offset += ret;
    }

    if (perf_stat->total_publish_event_num != 0) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "publish system call:\nave time(us) %llu \nmax time(us) %llu \n",
            tick_to_microsecond(perf_stat->publish_syscall_total_time / perf_stat->total_publish_event_num),
            tick_to_microsecond(perf_stat->publish_syscall_max_time));
        if (ret >= 0) {
            offset += ret;
        }

        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "publish in kernel:\nave time(us) %llu \nmax time(us) %llu \n",
            tick_to_microsecond(perf_stat->publish_in_kernel_total_time / perf_stat->total_publish_event_num),
            tick_to_microsecond(perf_stat->publish_in_kernel_max_time));
        if (ret >= 0) {
            offset += ret;
        }
    }

    if (perf_stat->total_wakeup_event_num != 0) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "wakeup:\nave time(us) %llu \nmax time(us) %llu \n",
            tick_to_microsecond(perf_stat->wakeup_total_time / perf_stat->total_wakeup_event_num),
            tick_to_microsecond(perf_stat->wakeup_max_time));
        if (ret >= 0) {
            offset += ret;
        }
    }

    if (perf_stat->total_sched_event_num != 0) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "publish subscribe:\nave time(us) %llu \nmax time(us) %llu \n",
            tick_to_microsecond(perf_stat->publish_subscribe_total_time / perf_stat->total_sched_event_num),
            tick_to_microsecond(perf_stat->publish_subscribe_max_time));
        if (ret >= 0) {
            offset += ret;
        }
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "\nevent id,publish event num,publish_fail_event_num,sched event num\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        if ((perf_stat->publish_event_num[i] != 0) || (perf_stat->sched_event_num[i] != 0)) {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                "%d, %llu, %llu, %llu\n",
                i, perf_stat->publish_event_num[i], perf_stat->publish_fail_event_num[i], perf_stat->sched_event_num[i]);
            if (ret >= 0) {
                offset += ret;
            }
        }
    }

    sched_kfree(perf_stat);
    return offset;
}

STATIC ssize_t sched_sysfs_node_cpuid_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%u\n", cpuid_in_node);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t sched_sysfs_node_cpuid_write(ka_device_t *dev, ka_device_attribute_t *attr,
                                            const char *buf, size_t count)
{
    u32 val = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return count;
    }

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        esched_dev_put(node);
        return count;
    }

    if ((val >= node->cpu_num) || (sched_get_cpu_ctx(node, val) == NULL)) {
        sched_err("The cpuid is invalid. (val=%u)\n", val);
        esched_dev_put(node);
        return count;
    }

    cpuid_in_node = val;
    esched_dev_put(node);
    sched_info("Set node_cpuid %u.\n", val);
    return count;
}

STATIC ssize_t sched_sysfs_node_cur_event_num(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
#ifdef CFG_FEATURE_VFIO
    int32_t k;
#endif
    int32_t ret, i, j;
    ssize_t offset = 0;
    struct sched_event_list *event_list = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "node_id %u current event num: %u\n\nnode_id proc_pri event_pri event_num (vf0~vf16)\n",
        node->node_id, ka_base_atomic_read(&node->cur_event_num));
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < SCHED_MAX_PROC_PRI_NUM; i++) {
        for (j = 0; j < SCHED_MAX_EVENT_PRI_NUM; j++) {
            event_list = sched_get_sched_event_list(node, i, j);
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                "%u,%d,%d,%u", node->node_id, i, j, event_list->cur_num);
            if (ret >= 0) {
                offset += ret;
            }

#ifdef CFG_FEATURE_VFIO
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, " (");
            if (ret >= 0) {
                offset += ret;
            }

            /* Count the number of events for each VF */
            for (k = 0; k < VMNG_VDEV_MAX_PER_PDEV; k++) {
                ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u ",
                    event_list->slice_cur_event_num[k]);
                if (ret >= 0) {
                    offset += ret;
                }
            }
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, ")\n");
            if (ret >= 0) {
                offset += ret;
            }
#else
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "\n");
            if (ret >= 0) {
                offset += ret;
            }
#endif
        }
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_cpu_cur_thread(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "cpuid,pid,gid,tid\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        thread_ctx = esched_cpu_cur_thread_get(cpu_ctx);
        if (thread_ctx != NULL) {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                "%u,pid:%d(%s),gid:%u,tid:%u\n",
                i, thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->proc_ctx->name,
                thread_ctx->grp_ctx->gid, thread_ctx->tid);
            if (ret >= 0) {
                offset += ret;
            }
            esched_cpu_cur_thread_put(thread_ctx);
        } else {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%d,idle\n", cpu_ctx->cpuid);
            if (ret >= 0) {
                offset += ret;
            }
        }
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_cpu_event_resource(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_event_que *res_que = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "cpuid,resouce total:use:que head:que tail,max use, enque_full cnt, deque empty cnt\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < node->cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, i);
        if (cpu_ctx == NULL) {
            continue;
        }

        res_que = &cpu_ctx->event_res;
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u,%u:%u:%u:%u:%u:%llu:%llu\n", i,
            res_que->depth, res_que->depth - sched_que_element_num(res_que), res_que->head, res_que->tail,
            res_que->stat.max_use, res_que->stat.enque_full, res_que->stat.deque_empty);
        if (ret >= 0) {
            offset += ret;
        }
    }

    esched_dev_put(node);

    return offset;
}

STATIC int32_t sched_sysfs_node_proc_single_thread_info(char *buf, struct sched_thread_ctx *thread_ctx,
    struct sched_grp_ctx *grp_ctx, ssize_t *offset, int32_t idx)
{
    int32_t ret;
    char *status = NULL;

    if (ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_IDLE) {
        status = "idle";
    } else if (ka_base_atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_RUN) {
        status = "running";
    } else {
        status = "ready";
    }

    if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
        ret = snprintf_s(buf + *offset, KA_MM_PAGE_SIZE - *offset, KA_MM_PAGE_SIZE - *offset - 1,
            "%u,%d,%d,%d,%s,%u,%s,%u,%u,%d,%d,%llu,%llu,%llu,%llu,%llu,%llx,%u,%u,%u\n",
            thread_ctx->bind_cpuid, grp_ctx->pid, grp_ctx->gid, idx,
            thread_ctx->name, thread_ctx->kernel_tid, status, thread_ctx->wait_flag, thread_ctx->timeout_flag,
            thread_ctx->pre_normal_wakeup_reason, thread_ctx->normal_wakeup_reason,
            tick_to_millisecond(thread_ctx->start_time), tick_to_microsecond(thread_ctx->max_proc_time),
            (thread_ctx->stat.sched_event != 0) ? (tick_to_microsecond(thread_ctx->total_proc_time) /
            thread_ctx->stat.sched_event) : 0, tick_to_microsecond(thread_ctx->max_sched_time),
            (thread_ctx->stat.sched_event != 0) ? (tick_to_microsecond(thread_ctx->total_sched_time) /
            thread_ctx->stat.sched_event) : 0, thread_ctx->subscribe_event_bitmap,
            thread_ctx->stat.sched_event, thread_ctx->stat.timeout_cnt,
            ka_base_atomic_read(&thread_ctx->stat.discard_event));
    } else {
        ret = snprintf_s(buf + *offset, KA_MM_PAGE_SIZE - *offset, KA_MM_PAGE_SIZE - *offset - 1,
            "%d,%s,%u,%s,%u,%llu,%llu,%llu,%llu,%llu,%llx,%u,%u\n",
            idx,
            thread_ctx->name, thread_ctx->kernel_tid, status, thread_ctx->wait_flag,
            tick_to_millisecond(thread_ctx->start_time), tick_to_microsecond(thread_ctx->max_proc_time),
            (thread_ctx->stat.sched_event != 0) ? (tick_to_microsecond(thread_ctx->total_proc_time) /
            thread_ctx->stat.sched_event) : 0, tick_to_microsecond(thread_ctx->max_sched_time),
            (thread_ctx->stat.sched_event != 0) ? (tick_to_microsecond(thread_ctx->total_sched_time) /
            thread_ctx->stat.sched_event) : 0, thread_ctx->subscribe_event_bitmap,
            ka_base_atomic_read(&grp_ctx->cur_event_num), thread_ctx->stat.sched_event);
    }
    if (ret >= 0) {
        *offset += ret;
    }

    return ret;
}

STATIC void sched_sysfs_node_grp_thread_info(char *buf, struct sched_grp_ctx *grp_ctx, ssize_t *offset)
{
    struct sched_thread_ctx *thread_ctx = NULL;
    u32 i;
    int32_t ret;

    for (i = 0; i < grp_ctx->thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, grp_ctx->thread[i]);
        if (thread_ctx->bind_cpuid == cpuid_in_node) {
            ret = sched_sysfs_node_proc_single_thread_info(buf, thread_ctx, grp_ctx, offset, i);
            if (ret < 0) {
                break;
            }
        }
    }
}

STATIC void sched_sysfs_node_proc_thread_info(char *buf, ssize_t *offset, struct sched_proc_ctx *proc_ctx)
{
    struct sched_grp_ctx *grp_ctx = NULL;
    u32 i;

    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, i);
        if (grp_ctx->sched_mode != SCHED_MODE_SCHED_CPU) {
            continue;
        }
        sched_sysfs_node_grp_thread_info(buf, grp_ctx, offset);
    }
}

STATIC ssize_t sched_sysfs_node_cpu_thread_info(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    if ((cpuid_in_node >= node->cpu_num) || (sched_get_cpu_ctx(node, cpuid_in_node) == NULL)) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "current cpu is not sched cpu\n");
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "bind_cpuid,pid,gid,tid,name,kernel_tid,status,wait flag,timeout flag,pre_wakeup reason,normal wakeup reason,"
        "start time(ms),max proc time(us),ave proc time(us),max sched time(us),ave sched time(us),"
        "subscribe event bitmap,fwd event,sched event,timeout cnt,discard cnt\n");
    if (ret >= 0) {
        offset += ret;
    }

    pidListForEachHandle(buf, &offset, sched_sysfs_node_proc_thread_info);
    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_cpu_sched_track_format(struct sched_cpu_sched_trace *sched_trace,
    char *buf, u32 buf_len, u32 start_index, u32 num, u64 *last_timestamp)
{
    u32 i, idx;
    int32_t ret;
    u64 timestamp, use_time;
    u64 callback_time = 0;
    int32_t record_finish_flag = SCHED_INVALID;
    ssize_t offset = 0;
    struct sched_cpu_sched_thread *sched_thread = NULL;

    timestamp = *last_timestamp;
    for (i = 0; i < num; i++) {
        idx = (u32)((start_index + i) & SCHED_SWICH_THREAD_NUM_MASK);
        sched_thread = &sched_trace->sched_thread_list[idx];

        if (sched_thread->sched_timestamp == 0) {
            continue;
        }

        if ((sched_thread->sched_timestamp - timestamp) != 0) {
            use_time = tick_to_microsecond(sched_thread->sched_timestamp - timestamp);
            /* If the value is greater than 5 us, the CPU is sleeping during thread switching.  */
            if (use_time > 5) {
                ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1, "idle,(%llu) -->\n", use_time);
                if (ret >= 0) {
                    offset += ret;
                }
            }
        }

        if (sched_thread->finish_timestamp == 0) {
            record_finish_flag = SCHED_VALID;
            use_time = tick_to_microsecond(sched_get_cur_timestamp() - sched_thread->sched_timestamp);
            callback_time = tick_to_microsecond(sched_get_cur_timestamp() - sched_thread->finish_timestamp);
        } else {
            use_time = tick_to_microsecond(sched_thread->finish_timestamp - sched_thread->sched_timestamp);
            callback_time = tick_to_microsecond(sched_thread->callback_timestamp - sched_thread->finish_timestamp);
        }

        ret = snprintf_s(buf + offset, buf_len - offset, buf_len - offset - 1,
            "(%u)(%llu,%llu,%llu): (%d %u %u %u),(%u %u),(%d),(%llu),(%u,%llu,%llu,%llu) -->\n",
            idx, sched_thread->sched_timestamp, sched_thread->publish_timestamp,
            sched_thread->add_queue_list_timestamp, sched_thread->pid, sched_thread->gid, sched_thread->tid,
            sched_thread->event_id, sched_thread->pid_pri, sched_thread->event_pri, sched_thread->normal_wakeup_reason,
            use_time, sched_thread->cur_event_num, tick_to_microsecond(sched_thread->wait_time),
            tick_to_microsecond(sched_thread->waked_to_wakeup_time), callback_time);
        if (ret >= 0) {
            offset += ret;
        }

        timestamp = sched_thread->finish_timestamp;

        /* record event trace finish, just break */
        if (record_finish_flag == SCHED_VALID) {
            break;
        }

        sched_thread->sched_timestamp = 0;
    }

    *last_timestamp = timestamp;

    return offset;
}

STATIC void sched_cpu_trace_record(struct sched_cpu_ctx *cpu_ctx,
    u64 trace_num, const struct sched_trace_record_info *trace_record)
{
    static char sched_sysfs_buf[4096];
    ka_file_t *fp = NULL;
    char buf[MAX_LENTH];
    loff_t offset_tmp = 0;
    int32_t ret;
    u32 i, start_index;
    u64 timestamp = 0;

    ret = snprintf_s(buf, MAX_LENTH, MAX_LENTH - 1, "%s/cpu_%d_%s_%s_%llu",
        SCHED_FILE_PATH, cpu_ctx->cpuid, trace_record->record_reason, trace_record->key, trace_record->timestamp);
    if (ret < 0) {
        sched_err("Failed to invoke the snprintf_s.\n");
        return;
    }

    fp = ka_fs_filp_open(buf, KA_O_RDWR | KA_O_TRUNC | KA_O_CREAT, SCHED_FILE_MODE);
    if (KA_IS_ERR(fp)) {
        sched_err("Failed to invoke the filp_open. (buf=\"%s\")\n", buf);
        return;
    }

    ret = snprintf_s(sched_sysfs_buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "cpuid %d thread schedule track\n"
        "(index)(sched timestamp,publish timestamp,add list timestamp): (pid gid tid event),(pid pri, event pri),"
        "(wakeup reason),(use time(us)),(cpu cur event num, sched wait time(us),waked to wakeup time(us),"
        "callback time(us)) -->\n",
        cpu_ctx->cpuid);
    if (ret > 0) {
        sched_sysfs_write_file(fp, (const void *)sched_sysfs_buf, ka_base_strlen(sched_sysfs_buf), &offset_tmp);
    }

    sched_debug("Show details. (cpuid=%u; trace_num=%llu)\n", cpu_ctx->cpuid, trace_num);

    for (i = 0; i < SCHED_SWICH_THREAD_RECORD_NUM; i += SCHED_SWICH_THREAD_SHOW_NUM) {
        start_index = (trace_num - SCHED_SWICH_THREAD_RECORD_NUM + i) & SCHED_SWICH_THREAD_NUM_MASK;

        ret = sched_sysfs_node_cpu_sched_track_format(cpu_ctx->sched_trace, sched_sysfs_buf, KA_MM_PAGE_SIZE,
            start_index, SCHED_SWICH_THREAD_SHOW_NUM, &timestamp);
        if (ret > 0) {
            sched_sysfs_write_file(fp, (const void *)sched_sysfs_buf, ka_base_strlen(sched_sysfs_buf), &offset_tmp);
        }
    }

    (void)ka_fs_filp_close(fp, NULL);
}

void sched_trace_record(struct sched_numa_node *node, struct sched_trace_record_info *trace_record)
{
    u32 i;
    struct sched_cpu_ctx *cpu_ctx = NULL;

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        if (cpu_ctx->sched_trace != NULL) {
            sched_cpu_trace_record(cpu_ctx, cpu_ctx->sched_trace->trace_num, trace_record);
        }
    }
}

STATIC ssize_t sched_sysfs_node_cpu_sched_track(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    struct sched_cpu_ctx *cpu_ctx = NULL;
    int32_t ret;
    u32 start_index;
    ssize_t offset = 0;
    u64 timestamp = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "cpuid %d thread schedule track\n"
        "(index)timestamp: (pid gid tid event),(use time(us)),(cpu cur event num, sched wait time(us),"
        "waked to wakeup time(us), callback time(us)) -->\n",
        cpuid_in_node);
    if (ret >= 0) {
        offset += ret;
    }

    cpu_ctx = sched_get_cpu_ctx(node, cpuid_in_node);
    if (((cpu_ctx == NULL) || (cpu_ctx->sched_trace == NULL))) {
        esched_dev_put(node);
        return offset;
    }

    start_index = (cpu_ctx->sched_trace->trace_num - SCHED_SWICH_THREAD_SHOW_NUM) & SCHED_SWICH_THREAD_NUM_MASK;

    offset += sched_sysfs_node_cpu_sched_track_format(cpu_ctx->sched_trace, buf + offset, KA_MM_PAGE_SIZE - offset,
        start_index, SCHED_SWICH_THREAD_SHOW_NUM, &timestamp);
    esched_dev_put(node);
    return offset;
}

STATIC ssize_t sched_sysfs_pid_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%u\n", cur_pid);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t sched_sysfs_pid_write(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf, size_t count)
{
    int32_t val = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        sched_err("Dev not valid. (devid=%u)\n", g_node_id);
        return count;
    }

    if (ka_base_kstrtou32(buf, 0, (u32 *)&val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        esched_dev_put(node);
        return count;
    }

    proc_ctx = esched_proc_get(node, val);
    if (proc_ctx == NULL) {
        sched_err("The pid is invalid. (pid=%d)\n", val);
        esched_dev_put(node);
        return count;
    }

    cur_pid = (u32)val;
    node->sample_proc_id = val;
    esched_proc_put(proc_ctx);
    esched_dev_put(node);
    sched_info("Set pid %d.\n", val);
    return count;
}

STATIC ssize_t sched_sysfs_proc_status(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    char *msg = "normal";
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    proc_ctx = esched_proc_get(node, cur_pid);
    if (proc_ctx == NULL) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d invalid. can not find in node %u.\n", cur_pid, node->node_id);
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    if (proc_ctx->status == SCHED_INVALID) {
        msg = "deleting";
    }

    esched_proc_put(proc_ctx);

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%s\n", msg);
    if (ret >= 0) {
        offset += ret;
    }

    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_proc_pri(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    proc_ctx = esched_proc_get(node, cur_pid);
    if (proc_ctx == NULL) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %u invalid. can not find in node %u.\n", cur_pid, node->node_id);
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%u\n", proc_ctx->pri);
    if (ret >= 0) {
        offset += ret;
    }

    esched_proc_put(proc_ctx);
    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_proc_event_num(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    proc_ctx = esched_proc_get(node, cur_pid);
    if (proc_ctx == NULL) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d invalid. can not find in node %u.\n", cur_pid, node->node_id);
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "publish,schedule\n");
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%d,%d\n",
        ka_base_atomic_read(&proc_ctx->publish_event_num), ka_base_atomic_read(&proc_ctx->sched_event_num));
    if (ret >= 0) {
        offset += ret;
    }

    esched_proc_put(proc_ctx);
    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_proc_event_pri(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    proc_ctx = esched_proc_get(node, cur_pid);
    if (proc_ctx == NULL) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d invalid. can not find in node %u.\n", cur_pid, node->node_id);
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "event id,pri\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u,%u\n",
            i, proc_ctx->event_pri[i]);
        if (ret >= 0) {
            offset += ret;
        }
    }

    esched_proc_put(proc_ctx);
    esched_dev_put(node);

    return offset;
}

STATIC void sched_sysfs_info_to_log(int32_t pid, char *buf, u32 mode, ssize_t offset, ssize_t size)
{
}

STATIC ssize_t sched_sysfs_node_proc_group_list_inner(struct sched_numa_node *node, int32_t pid, char *buf, u32 mode)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    char *sched_mode = NULL;

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d invalid. can not find in chip %d\n", pid, node->node_id);
        if (ret >= 0) {
            offset += ret;
        }

        sched_sysfs_info_to_log(pid, buf, mode, offset, ret);
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "group,sched type,is_exclusive,thread num,run status,wait cpu mask,tid, cur event num\n");
    if (ret >= 0) {
        offset += ret;
        sched_sysfs_info_to_log(pid, buf, mode, offset, ret);
    }

    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, i);
        if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
            continue;
        } else if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
            sched_mode = "sched";
        } else {
            sched_mode = "non sched";
        }
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u,%s,%u,%u,%d,0x%x,%u,%d\n",
                         i, sched_mode, grp_ctx->is_exclusive, grp_ctx->thread_num,
                         ka_base_atomic_read(&grp_ctx->run_status), ka_base_atomic_read(&grp_ctx->wait_cpu_mask),
                         grp_ctx->cur_tid, ka_base_atomic_read(&grp_ctx->cur_event_num));
        if (ret >= 0) {
            sched_sysfs_info_to_log(pid, buf + offset, mode, offset, ret);
            offset += ret;
        }
    }

    esched_proc_put(proc_ctx);

    return offset;
}


STATIC ssize_t sched_sysfs_node_proc_group_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node != NULL) {
        offset = sched_sysfs_node_proc_group_list_inner(node, cur_pid, buf, INFO_TO_SYSFS);
        esched_dev_put(node);
    }
    return offset;
}

STATIC ssize_t sched_sysfs_proc_group_id_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%u\n", cur_gid);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t sched_sysfs_proc_group_id_write(ka_device_t *dev, ka_device_attribute_t *attr,
    const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        return count;
    }

    if (val >= SCHED_MAX_GRP_NUM) {
        sched_err("The gid is out of range. (gid=%u; max=%d)\n", val, SCHED_MAX_GRP_NUM);
        return count;
    }

    cur_gid = val;
    sched_info("Set proc_group_id %u.\n", val);
    return count;
}

static struct sched_grp_ctx *sched_sysfs_cur_grp_get(struct sched_numa_node *node)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;

    proc_ctx = esched_proc_get(node, cur_pid);
    if (proc_ctx == NULL) {
        return NULL;
    }

    grp_ctx = sched_get_grp_ctx(proc_ctx, cur_gid);
    if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
        esched_proc_put(proc_ctx);
        return NULL;
    }

    return grp_ctx;
}

static void sched_sysfs_cur_grp_put(struct sched_grp_ctx *grp_ctx)
{
    esched_proc_put(grp_ctx->proc_ctx);
}

STATIC ssize_t sched_sysfs_node_proc_group_event_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    u32 i, j;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_event_thread_map *event_thread_map = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    grp_ctx = sched_sysfs_cur_grp_get(node);
    if (grp_ctx == NULL) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d gid %d invalid. can not find in node %u.\n", cur_pid, cur_gid, node->node_id);
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "event id,thread num(thread list),max_event_num,event_num,drop_event_num\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < SCHED_MAX_EVENT_TYPE_NUM; i++) {
        event_thread_map = &grp_ctx->event_thread_map[i];
        if (event_thread_map->thread_num == 0) {
            continue;
        }

        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u,%u(%u",
            i, event_thread_map->thread_num, event_thread_map->thread[0]);
        if (ret >= 0) {
            offset += ret;
        }

        for (j = 1; j < event_thread_map->thread_num; j++) {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, ",%u",
                event_thread_map->thread[j]);
            if (ret >= 0) {
                offset += ret;
            }
        }

        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "),%u,%u,%u\n",
            grp_ctx->max_event_num[i], ka_base_atomic_read(&grp_ctx->event_num[i]), ka_base_atomic_read(&grp_ctx->drop_event_num[i]));
        if (ret >= 0) {
            offset += ret;
        }
    }

    sched_sysfs_cur_grp_put(grp_ctx);
    esched_dev_put(node);

    return offset;
}

STATIC ssize_t sched_sysfs_node_proc_group_thread_inner(struct sched_grp_ctx *grp_ctx, char *buf, int32_t mode)
{
    u32 i;
    int32_t ret;
    ssize_t offset = 0;
    struct sched_thread_ctx *thread_ctx = NULL;

    if (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "bind_cpuid,pid,gid,tid,name,kernel_tid,status,wait flag,timeout flag,pre_wakeup reason,normal wakeup reason,"
            "start time(ms),max proc time(us),ave proc time(us),max sched time(us),ave sched time(us),"
            "subscribe event bitmap,fwd event,sched event,timeout cnt,discard cnt\n");
    } else {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "tid,name,kernel_tid,status,wait flag,start time(ms),max proc time(us),ave proc time(us),"
            "max sched time(us),ave sched time(us),subscribe event bitmap,queue remain event,sched event,"
            "publish enque full\n");
    }

    if (ret >= 0) {
        sched_sysfs_info_to_log(grp_ctx->pid, buf + offset, mode, offset, ret);
        offset += ret;
    }

    for (i = 0; i < grp_ctx->cfg_thread_num; i++) {
        thread_ctx = sched_get_thread_ctx(grp_ctx, i);
        if (thread_ctx->valid == SCHED_INVALID) {
            continue;
        }

        ret = sched_sysfs_node_proc_single_thread_info(buf, thread_ctx, grp_ctx, &offset, i);
        if (ret < 0) {
            break;
        }

        sched_sysfs_info_to_log(grp_ctx->pid, buf + offset - ret, mode, offset, ret);
    }

    return offset;
}


STATIC ssize_t sched_sysfs_node_proc_group_thread(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    struct sched_grp_ctx *grp_ctx = NULL;
    ssize_t offset = 0;
    ssize_t ret;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    grp_ctx = sched_sysfs_cur_grp_get(node);
    if (grp_ctx == NULL) {
        int count;
        count = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d gid %d invalid. can not find in node %u.\n", cur_pid, cur_gid, node->node_id);
        if (count >= 0) {
            offset += count;
        }
        esched_dev_put(node);
        return offset;
    }

    ret = sched_sysfs_node_proc_group_thread_inner(grp_ctx, buf, INFO_TO_SYSFS);
    sched_sysfs_cur_grp_put(grp_ctx);
    esched_dev_put(node);
    return ret;
}

void sched_sysfs_show_proc_info(u32 chip_id, int32_t pid)
{
    struct sched_proc_ctx *proc_ctx = NULL;
    struct sched_grp_ctx *grp_ctx = NULL;
    char *buf = NULL;
    int32_t i;
    struct sched_numa_node *node = NULL;

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        return;
    }

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        sched_warn("The variable node_id or pid is invalid. (node_id=%u; pid=%d)\n", node->node_id, pid);
        return;
    }

    buf = (char *)sched_kzalloc(sizeof(char) * KA_MM_PAGE_SIZE, KA_GFP_KERNEL);
    if (buf == NULL) {
        esched_proc_put(proc_ctx);
        return;
    }

    (void)sched_sysfs_node_proc_group_list_inner(node, pid, buf, INFO_TO_LOG);

    for (i = 0; i < SCHED_MAX_GRP_NUM; i++) {
        grp_ctx = sched_get_grp_ctx(proc_ctx, i);
        if (grp_ctx->sched_mode == SCHED_MODE_UNINIT) {
            continue;
        }

        (void)sched_sysfs_node_proc_group_thread_inner(grp_ctx, buf, INFO_TO_LOG);
    }

    esched_proc_put(proc_ctx);

    sched_kfree(buf);
    buf = NULL;
}

STATIC ssize_t sched_sysfs_group_cur_event_num(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret, i;
    ssize_t offset = 0;
    struct sched_grp_ctx *grp_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    grp_ctx = sched_sysfs_cur_grp_get(node);
    if (grp_ctx == NULL) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d gid %d invalid. can not find in node %u.\n", cur_pid, cur_gid, node->node_id);
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    if (grp_ctx->sched_mode != SCHED_MODE_NON_SCHED_CPU) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "pid %d gid %d mode is invalid, cur_mode %d.\n", cur_pid, cur_gid, grp_ctx->sched_mode);
        if (ret >= 0) {
            offset += ret;
        }

        sched_sysfs_cur_grp_put(grp_ctx);
        esched_dev_put(node);
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "node_id %d pid %d gid %d current event num: %u\n\npid gid event_pri event_num\n",
        node->node_id, cur_pid, cur_gid, ka_base_atomic_read(&grp_ctx->cur_event_num));
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < SCHED_MAX_EVENT_PRI_NUM; i++) {
        struct sched_event_list *event_list = NULL;
        struct sched_event *event = NULL;
        event_list = sched_get_non_sched_event_list(grp_ctx, i);
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "%d,%u,%d,%u\n", cur_pid, cur_gid, i, event_list->cur_num);
        if (ret >= 0) {
            offset += ret;
        }

        ka_task_spin_lock_bh(&event_list->lock);
        ka_list_for_each_entry(event, &event_list->head, list) {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                "   %u,%u\n", event->event_id, event->subevent_id);
            if (ret >= 0) {
                offset += ret;
            }
        }
        ka_task_spin_unlock_bh(&event_list->lock);
    }

    sched_sysfs_cur_grp_put(grp_ctx);
    esched_dev_put(node);
    return offset;
}

#ifdef CFG_FEATURE_VFIO
void sched_get_vf_proc_list(char *buf, ssize_t *offset, struct sched_proc_ctx *proc_ctx)
{
    int ret;

    if ((u32)proc_ctx->vfid == g_cur_vf) {
        ret = snprintf_s(buf + *offset, KA_MM_PAGE_SIZE - *offset, KA_MM_PAGE_SIZE - *offset - 1, "%ld, %s\n",
            proc_ctx->pid, proc_ctx->name);
        if (ret >= 0) {
            *offset += ret;
        }
    }
}

ssize_t sched_sysfs_node_vf_proc_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_vf_ctx *vf_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    if (g_cur_vf == SCHED_DEFAULT_VF_ID) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "vf_0 (used by physical machine) is not supported\n");
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    vf_ctx = sched_get_vf_ctx(node, g_cur_vf);

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "vf_%u (status: %u) have %u valid proc.\n", g_cur_vf, ka_base_atomic_read(&vf_ctx->status),
        ka_base_atomic_read(&vf_ctx->proc_num));
    if (ret >= 0) {
        offset += ret;
    }

    pidListForEachHandle(buf, &offset, sched_get_vf_proc_list);

    esched_dev_put(node);
    return offset;
}

ssize_t sched_sysfs_node_vf_list(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret, i;
    ssize_t offset = 0;
    struct sched_vf_ctx *vf_ctx = NULL;
    char *msg = "normal";
    int vf_status;
    u64 total_sched_cpu_mask = 0;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "vf_id, status, resource_packet_num.\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        vf_ctx = sched_get_vf_ctx(node, i);
        vf_status = ka_base_atomic_read(&vf_ctx->status);

        switch (vf_status) {
            case SCHED_VF_STATUS_UNCREATED:
                msg = "not_created";
                break;
            case SCHED_VF_STATUS_NORMAL:
                msg = "normal";
                total_sched_cpu_mask += vf_ctx->config_sched_cpu_mask;
                break;
            case SCHED_VF_STATUS_DELETING:
                msg = "deleting";
                break;
            default:
                ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                    "vf_%d unknown status: %d.\n", i, vf_status);
                break;
        }

        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%5d, %s, %llx.\n", i, msg,
            vf_ctx->config_sched_cpu_mask);
        if (ret >= 0) {
            offset += ret;
        }
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "total used resource packet num: %llx.\n", total_sched_cpu_mask);
    if (ret >= 0) {
        offset += ret;
    }

    esched_dev_put(node);
    return offset;
}

ssize_t sched_sysfs_vfid_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u\n", g_cur_vf);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

ssize_t sched_sysfs_vfid_write(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        return count;
    }

    if (val >= VMNG_VDEV_MAX_PER_PDEV) {
        sched_err("The vfid is out of range. (vfid=%u)\n", val);
        return count;
    }

    g_cur_vf = val;
    sched_info("Set vfid %u.\n", val);
    return count;
}

ssize_t sched_sysfs_vf_info_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;
    struct sched_vf_ctx *vf_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    if (g_cur_vf == SCHED_DEFAULT_VF_ID) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "vf_0 (used by physical machine) is not supported\n");
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    vf_ctx = sched_get_vf_ctx(node, g_cur_vf);

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "vf_ctx %u info show: \n"
        "vf_id: %u \nstatus: %u \ncreate timestamp: %llu \n"
        "proc num: %u \nque depth: %u \nconfig_sched_cpu_mask: 0x%llx \nsched_cpu_mask: 0x%llx",
        g_cur_vf, vf_ctx->vfid, ka_base_atomic_read(&vf_ctx->status), vf_ctx->create_timestamp,
        ka_base_atomic_read(&vf_ctx->proc_num), vf_ctx->que_depth, vf_ctx->config_sched_cpu_mask, vf_ctx->sched_cpu_mask);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "\n\nvf_ctx: %u stat show \n"
        "publish event num: %llu \nsched event num: %llu \ncur event num: %llu \n\n", g_cur_vf,
        ka_base_atomic64_read(&vf_ctx->stat.publish_event_num), ka_base_atomic64_read(&vf_ctx->stat.sched_event_num),
        ka_base_atomic64_read(&vf_ctx->stat.cur_event_num));
    if (ret >= 0) {
        offset += ret;
    }

    esched_dev_put(node);
    return offset;
}

ssize_t sched_sysfs_vf_occupy_cpu(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    u32 i;
    u64 config_sched_cpu_mask;
    ssize_t offset = 0;
    struct sched_vf_ctx *vf_ctx = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    struct sched_thread_ctx *thread_ctx = NULL;
    struct sched_numa_node *node = esched_dev_get(g_node_id);
    if (node == NULL) {
        return offset;
    }

    if (g_cur_vf == SCHED_DEFAULT_VF_ID) {
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
            "vf_0 (used by physical machine) is not supported\n");
        if (ret >= 0) {
            offset += ret;
        }
        esched_dev_put(node);
        return offset;
    }

    vf_ctx = sched_get_vf_ctx(node, g_cur_vf);

    config_sched_cpu_mask = vf_ctx->config_sched_cpu_mask;
    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "vf %u(status %u) max occupy aicpu\n"
        "config sched cpu mask: 0x%llx sched cpu mask: 0x%llx\n", g_cur_vf, ka_base_atomic_read(&vf_ctx->status),
        config_sched_cpu_mask, vf_ctx->sched_cpu_mask);
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < node->sched_cpu_num; i++) {
        cpu_ctx = sched_get_cpu_ctx(node, node->sched_cpuid[i]);
        thread_ctx = esched_cpu_cur_thread_get(cpu_ctx);
        if (thread_ctx == NULL) {
            continue;
        }

        if (config_sched_cpu_mask & (0x1ULL << node->sched_cpuid[i])) {
            ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
                "aicpu:%u, pid:%d(%s), gid:%u, tid:%u\n",
                cpu_ctx->cpuid, thread_ctx->grp_ctx->pid, thread_ctx->grp_ctx->proc_ctx->name,
                thread_ctx->grp_ctx->gid, thread_ctx->tid);
            if (ret >= 0) {
                offset += ret;
            }
        }
        esched_cpu_cur_thread_put(thread_ctx);
    }

    esched_dev_put(node);
    return offset;
}
#endif

#define SCHED_RECORD_DEFAULT_NUM 1000
#define SCHED_SYSFS_RECORD_MAX_NUM 65536

static uint32_t sched_sysfs_record_num = SCHED_RECORD_DEFAULT_NUM; // sched record deflaut num is 1000

STATIC ssize_t sched_sysfs_record_num_read(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    int32_t ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "%u\n", sched_sysfs_record_num);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t sched_sysfs_record_num_write(ka_device_t *dev, ka_device_attribute_t *attr,
                                            const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        sched_err("Failed to invoke the ka_base_kstrtou32.\n");
        return count;
    }

    if ((val > SCHED_SYSFS_RECORD_MAX_NUM) || (val == 0)) {
        sched_err("The value of variable record_num is out of range. "
            "(record_num=%u; max=%d)\n", val, SCHED_SYSFS_RECORD_MAX_NUM);
        return count;
    }

    sched_sysfs_record_num = val;
    sched_info("Set record_num %u.\n", val);
    return count;
}

uint32_t sched_sysfs_record_num_data(void)
{
    return sched_sysfs_record_num;
}

#define SCHED_ATTR_RD (KA_S_IRUSR | S_IRGRP | S_IROTH)
#define SCHED_ATTR_WR (S_IWUSR | S_IWGRP)
#define SCHED_ATTR_RW (SCHED_ATTR_RD | SCHED_ATTR_WR)

#define DRV_FS_ATTR KA_DRIVER_DEVICE_ATTR
#define DRV_FS_ATTR_NAME_POINTER(_name)  &dev_attr_##_name.attr

#define DECLEAR_ESCHED_FS_ATTR \
    static DRV_FS_ATTR(node_list, SCHED_ATTR_RD, sched_sysfs_node_list, NULL); \
    static DRV_FS_ATTR(node_id, SCHED_ATTR_RW, sched_sysfs_node_id_read, sched_sysfs_node_id_write); \
    static DRV_FS_ATTR(node_debug, SCHED_ATTR_RW, sched_sysfs_node_debug_read, sched_sysfs_node_debug_write); \
    static DRV_FS_ATTR(cpu_list, SCHED_ATTR_RD, sched_sysfs_node_cpu_list, NULL); \
    static DRV_FS_ATTR(sched_cpu_mask, SCHED_ATTR_RD, sched_sysfs_node_sched_cpu_mask, NULL); \
    static DRV_FS_ATTR(proc_list, SCHED_ATTR_RD, sched_sysfs_node_proc_list, NULL); \
    static DRV_FS_ATTR(proc_list_del, SCHED_ATTR_RD, sched_sysfs_node_del_proc_list, NULL); \
    static DRV_FS_ATTR(node_event_resource, SCHED_ATTR_RD, sched_sysfs_node_event_resource, NULL); \
    static DRV_FS_ATTR(event_trace, SCHED_ATTR_RD, sched_sysfs_event_trace, NULL); \
    static DRV_FS_ATTR(sample_period, SCHED_ATTR_RW, sched_sysfs_sample_period_read, sched_sysfs_sample_period_write); \
    static DRV_FS_ATTR(event_sample, SCHED_ATTR_RD, sched_sysfs_event_sample, NULL); \
    static DRV_FS_ATTR(cpu_usage_sample, SCHED_ATTR_RD, sched_sysfs_cpu_usage_sample, NULL); \
    static DRV_FS_ATTR(node_sched_proc_abnormal_time_in_microsecond, SCHED_ATTR_RW, \
        sched_sysfs_node_sched_proc_abnormal_time_read, sched_sysfs_node_sched_proc_abnormal_time_write); \
    static DRV_FS_ATTR(node_sched_abnormal_event_clear, SCHED_ATTR_WR, \
        NULL, sched_sysfs_node_sched_abnormal_event_clear); \
    static DRV_FS_ATTR(node_sched_proc_abnormal_event, SCHED_ATTR_RD, \
        sched_sysfs_node_sched_proc_abnormal_event, NULL); \
    static DRV_FS_ATTR(node_sched_publish_syscall_abnormal_time_in_microsecond, SCHED_ATTR_RW, \
        sched_sysfs_node_sched_publish_syscall_abnormal_time_read, \
        sched_sysfs_node_sched_publish_syscall_abnormal_time_write); \
    static DRV_FS_ATTR(node_sched_publish_syscall_abnormal_event, SCHED_ATTR_RD, \
        sched_sysfs_node_sched_publish_syscall_abnormal_event, NULL); \
    static DRV_FS_ATTR(node_sched_publish_in_kernel_abnormal_time_in_microsecond, SCHED_ATTR_RW, \
        sched_sysfs_node_sched_publish_in_kernel_abnormal_time_read, \
        sched_sysfs_node_sched_publish_in_kernel_abnormal_time_write); \
    static DRV_FS_ATTR(node_sched_publish_in_kernel_abnormal_event, SCHED_ATTR_RD, \
        sched_sysfs_node_sched_publish_in_kernel_abnormal_event, NULL); \
    static DRV_FS_ATTR(node_sched_wakeup_abnormal_time_in_microsecond, SCHED_ATTR_RW, \
        sched_sysfs_node_sched_wakeup_abnormal_time_read, sched_sysfs_node_sched_wakeup_abnormal_time_write); \
    static DRV_FS_ATTR(node_sched_wakeup_abnormal_event, SCHED_ATTR_RD, \
        sched_sysfs_node_sched_wakeup_abnormal_event, NULL); \
    static DRV_FS_ATTR(node_sched_publish_subscribe_abnormal_time_in_microsecond, SCHED_ATTR_RW, \
        sched_sysfs_node_sched_publish_subscribe_abnormal_time_read, \
        sched_sysfs_node_sched_publish_subscribe_abnormal_time_write); \
    static DRV_FS_ATTR(node_sched_publish_subscribe_abnormal_event, SCHED_ATTR_RD, \
        sched_sysfs_node_sched_publish_subscribe_abnormal_event, NULL); \
    static DRV_FS_ATTR(node_sched_thread_run_abnormal_time_in_microsecond, SCHED_ATTR_RW, \
        sched_sysfs_node_sched_thread_run_abnormal_time_read, \
        sched_sysfs_node_sched_thread_run_abnormal_time_write); \
    static DRV_FS_ATTR(node_sched_thread_run_abnormal_event, SCHED_ATTR_RD, \
        sched_sysfs_node_sched_thread_run_abnormal_event, NULL); \
    static DRV_FS_ATTR(node_event_summary, SCHED_ATTR_RD, sched_sysfs_node_event_summary, NULL); \
    static DRV_FS_ATTR(node_sched_event_list, SCHED_ATTR_RD, sched_sysfs_node_sched_event_list, NULL); \
    static DRV_FS_ATTR(node_cur_event_num, SCHED_ATTR_RD, sched_sysfs_node_cur_event_num, NULL); \
    static DRV_FS_ATTR(cpuid_in_node, SCHED_ATTR_RW, sched_sysfs_node_cpuid_read, sched_sysfs_node_cpuid_write); \
    static DRV_FS_ATTR(cpu_cur_thread, SCHED_ATTR_RD, sched_sysfs_node_cpu_cur_thread, NULL); \
    static DRV_FS_ATTR(cpu_event_resource, SCHED_ATTR_RD, sched_sysfs_node_cpu_event_resource, NULL); \
    static DRV_FS_ATTR(cpu_sched_track, SCHED_ATTR_RD, sched_sysfs_node_cpu_sched_track, NULL); \
    static DRV_FS_ATTR(cpu_thread_info, SCHED_ATTR_RD, sched_sysfs_node_cpu_thread_info, NULL); \
    static DRV_FS_ATTR(pid, SCHED_ATTR_RW, sched_sysfs_pid_read, sched_sysfs_pid_write); \
    static DRV_FS_ATTR(proc_status, SCHED_ATTR_RD, sched_sysfs_proc_status, NULL); \
    static DRV_FS_ATTR(proc_pri, SCHED_ATTR_RD, sched_sysfs_proc_pri, NULL); \
    static DRV_FS_ATTR(proc_event_num, SCHED_ATTR_RD, sched_sysfs_proc_event_num, NULL); \
    static DRV_FS_ATTR(proc_event_pri, SCHED_ATTR_RD, sched_sysfs_node_proc_event_pri, NULL); \
    static DRV_FS_ATTR(proc_group_list, SCHED_ATTR_RD, sched_sysfs_node_proc_group_list, NULL); \
    static DRV_FS_ATTR(proc_gid, SCHED_ATTR_RW, sched_sysfs_proc_group_id_read, sched_sysfs_proc_group_id_write); \
    static DRV_FS_ATTR(proc_group_event_list, SCHED_ATTR_RD, sched_sysfs_node_proc_group_event_list, NULL); \
    static DRV_FS_ATTR(proc_group_thread, SCHED_ATTR_RD, sched_sysfs_node_proc_group_thread, NULL); \
    static DRV_FS_ATTR(sched_record_num, SCHED_ATTR_RW, sched_sysfs_record_num_read, sched_sysfs_record_num_write); \
    static DRV_FS_ATTR(proc_group_cur_event_num, SCHED_ATTR_RD, sched_sysfs_group_cur_event_num, NULL)

#ifdef CFG_FEATURE_VFIO
#define DECLEAR_ESCHED_FS_VFIO_ATTR \
    static DRV_FS_ATTR(vfid, SCHED_ATTR_RW, sched_sysfs_vfid_read, sched_sysfs_vfid_write); \
    static DRV_FS_ATTR(vf_list, SCHED_ATTR_RD, sched_sysfs_node_vf_list, NULL); \
    static DRV_FS_ATTR(vf_proc_list, SCHED_ATTR_RD, sched_sysfs_node_vf_proc_list, NULL); \
    static DRV_FS_ATTR(vf_info_show, SCHED_ATTR_RD, sched_sysfs_vf_info_show, NULL); \
    static DRV_FS_ATTR(vf_occupy_cpu, SCHED_ATTR_RD, sched_sysfs_vf_occupy_cpu, NULL)
#endif

#define DECLEAR_ESCHED_FS_ATTR_NORMAL_LIST(_struct_type, _profix) \
    static struct _struct_type *g_sched_##_profix##_normal_list[] = { \
    DRV_FS_ATTR_NAME_POINTER(node_list), \
    DRV_FS_ATTR_NAME_POINTER(node_id), \
    DRV_FS_ATTR_NAME_POINTER(node_debug), \
    DRV_FS_ATTR_NAME_POINTER(cpu_list), \
    DRV_FS_ATTR_NAME_POINTER(sched_cpu_mask), \
    DRV_FS_ATTR_NAME_POINTER(proc_list), \
    DRV_FS_ATTR_NAME_POINTER(proc_list_del), \
    DRV_FS_ATTR_NAME_POINTER(node_event_resource), \
    DRV_FS_ATTR_NAME_POINTER(event_trace), \
    DRV_FS_ATTR_NAME_POINTER(sample_period), \
    DRV_FS_ATTR_NAME_POINTER(event_sample), \
    DRV_FS_ATTR_NAME_POINTER(cpu_usage_sample), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_abnormal_event_clear), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_proc_abnormal_time_in_microsecond), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_proc_abnormal_event), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_publish_syscall_abnormal_time_in_microsecond), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_publish_syscall_abnormal_event), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_publish_in_kernel_abnormal_time_in_microsecond), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_publish_in_kernel_abnormal_event), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_wakeup_abnormal_time_in_microsecond), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_wakeup_abnormal_event), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_publish_subscribe_abnormal_time_in_microsecond), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_publish_subscribe_abnormal_event), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_thread_run_abnormal_time_in_microsecond), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_thread_run_abnormal_event), \
    DRV_FS_ATTR_NAME_POINTER(node_event_summary), \
    DRV_FS_ATTR_NAME_POINTER(node_sched_event_list), \
    DRV_FS_ATTR_NAME_POINTER(node_cur_event_num), \
    DRV_FS_ATTR_NAME_POINTER(cpuid_in_node), \
    DRV_FS_ATTR_NAME_POINTER(cpu_cur_thread), \
    DRV_FS_ATTR_NAME_POINTER(cpu_event_resource), \
    DRV_FS_ATTR_NAME_POINTER(cpu_sched_track), \
    DRV_FS_ATTR_NAME_POINTER(cpu_thread_info), \
    DRV_FS_ATTR_NAME_POINTER(pid), \
    DRV_FS_ATTR_NAME_POINTER(proc_status), \
    DRV_FS_ATTR_NAME_POINTER(proc_pri), \
    DRV_FS_ATTR_NAME_POINTER(proc_event_num), \
    DRV_FS_ATTR_NAME_POINTER(proc_event_pri), \
    DRV_FS_ATTR_NAME_POINTER(proc_group_list), \
    DRV_FS_ATTR_NAME_POINTER(proc_gid), \
    DRV_FS_ATTR_NAME_POINTER(proc_group_event_list), \
    DRV_FS_ATTR_NAME_POINTER(proc_group_thread), \
    DRV_FS_ATTR_NAME_POINTER(sched_record_num), \
    DRV_FS_ATTR_NAME_POINTER(proc_group_cur_event_num), \
    NULL, \
}

#ifdef CFG_FEATURE_VFIO
#define DECLEAR_ESCHED_FS_VFIO_ATTR_LIST(_struct_type, _profix) \
static ka_attribute_t *g_sched_##_profix##_vfio_attrs[] = { \
    DRV_FS_ATTR_NAME_POINTER(vfid), \
    DRV_FS_ATTR_NAME_POINTER(vf_proc_list), \
    DRV_FS_ATTR_NAME_POINTER(vf_list), \
    DRV_FS_ATTR_NAME_POINTER(vf_info_show), \
    DRV_FS_ATTR_NAME_POINTER(vf_occupy_cpu), \
    NULL, \
}
#endif

DECLEAR_ESCHED_FS_ATTR;

DECLEAR_ESCHED_FS_ATTR_NORMAL_LIST(attribute, sysfs);

static const ka_attribute_group_t g_sched_sysfs_node_normal_group = {
    ka_fs_init_ag_attrs(g_sched_sysfs_normal_list)
    ka_fs_init_ag_name("node")
};

#ifdef CFG_FEATURE_VFIO
DECLEAR_ESCHED_FS_VFIO_ATTR;

DECLEAR_ESCHED_FS_VFIO_ATTR_LIST(attribute, sysfs);

static const ka_attribute_group_t g_sched_sysfs_node_vfio_group = {
    ka_fs_init_ag_attrs(g_sched_sysfs_vfio_attrs)
    ka_fs_init_ag_name("vfio")
};
#endif

void sched_sysfs_init(ka_device_t *dev)
{
    int32_t ret;
    int32_t node_id;
    struct sched_numa_node *numa_node = NULL;
    for (node_id = 0; node_id < SCHED_MAX_CHIP_NUM; node_id++) {
        numa_node = sched_get_numa_node(node_id);
        if (numa_node != NULL) {
            sched_info("Loop details. (node_id=%d)\n", node_id);
            sched_fs_set_node_id(node_id);
            break;
        }
    }

    ret = ka_sysfs_create_group(ka_base_get_device_kobj(dev), &g_sched_sysfs_node_normal_group);
    sched_debug("Show details. (ka_sysfs_create_group=\"%s\"; ret=%d)\n", g_sched_sysfs_node_normal_group.name, ret);
#ifdef CFG_FEATURE_VFIO
    ret = ka_sysfs_create_group(ka_base_get_device_kobj(dev), &g_sched_sysfs_node_vfio_group);
    sched_debug("Show details. (ka_sysfs_create_group=\"%s\"; ret=%d)\n", g_sched_sysfs_node_vfio_group.name, ret);
#endif

    return;
}

void sched_sysfs_uninit(ka_device_t *dev)
{
    ka_sysfs_remove_group(ka_base_get_device_kobj(dev), &g_sched_sysfs_node_normal_group);
#ifdef CFG_FEATURE_VFIO
    ka_sysfs_remove_group(ka_base_get_device_kobj(dev), &g_sched_sysfs_node_vfio_group);
#endif
}


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
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_driver_pub.h"
#include "ka_errno_pub.h"
#include "ka_compiler_pub.h"
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_pci_pub.h"

#ifdef CFG_FEATURE_EXTERNAL_CDEV
#include "davinci_api.h"
#endif
#include "dbl/feature_loader.h"
#include "dbl/runenv_config.h"
#include "esched.h"
#include "esched_sysfs.h"
#include "esched_fops.h"


#ifdef AOS_LLVM_BUILD
#include "aos_dlib.h"
#endif

struct sched_char_dev char_dev;

#ifdef ESCHED_HOST
#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
const ka_pci_device_id_t g_esched_driver_tbl[] =   {{ KA_PCI_VDEVICE(HUAWEI, 0xd100), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd105), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xa126), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd500), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd501), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd802), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd803), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd804), 0 },
                                                    { KA_PCI_VDEVICE(HUAWEI, 0xd805), 0 },
                                                    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500,
                                                      KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                    {}};
KA_MODULE_DEVICE_TABLE(pci, g_esched_driver_tbl);
#endif

#define ESCHED_HASH_TABLE_BIT 10
#define ESCHED_HASH_TABLE_MASK ((0x1 << ESCHED_HASH_TABLE_BIT) - 1)

STATIC KA_DEFINE_HASHTABLE(esched_task_table, ESCHED_HASH_TABLE_BIT);
STATIC KA_TASK_DEFINE_MUTEX(esched_task_mutex);

int32_t copy_from_user_safe(void *to, const void __ka_user *from, unsigned long n)
{
    if ((from == NULL) || (n == 0)) {
        sched_err("The variable from is NULL or n is zero.\n");
        return DRV_ERROR_INNER_ERR;
    }

    if (ka_base_copy_from_user(to, (void *)from, n) != 0) {
        sched_err("Failed to invoke the copy_from_user. (size=%lu)\n", n);
        return DRV_ERROR_INNER_ERR;
    }

    return 0;
}

int32_t copy_to_user_safe(void __ka_user *to, const void *from, unsigned long n)
{
    if ((to == NULL) || (n == 0)) {
        sched_err("The variable to is NULL or n is zero.\n");
        return DRV_ERROR_INNER_ERR;
    }

    if (ka_base_copy_to_user(to, (void *)from, n) != 0) {
        sched_err("Failed to invoke the copy from user. (size=%lu)\n", n);
        return DRV_ERROR_INNER_ERR;
    }

    return 0;
}

u32 sched_ioctl_devid(u32 open_devid, u32 cmd_devid)
{
#ifdef CFG_FEATURE_EXTERNAL_CDEV
    return open_devid;
#else
    return cmd_devid;
#endif
}

STATIC int32_t sched_fop_set_sched_cpu(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_cpu_info para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_cpu_info)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

#ifdef ESCHED_HOST
    return DRV_ERROR_INVALID_DEVICE;
#else
    return sched_set_sched_cpu(sched_ioctl_devid(devid, para.dev_id), &para.cpu_mask);
#endif
}

STATIC int32_t sched_fop_proc_add_grp(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_add_grp para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_add_grp)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return sched_proc_add_grp(sched_ioctl_devid(devid, para.dev_id), para.gid, para.grp_name,
        para.sched_mode, para.thread_num);
}

#ifndef AOS_LLVM_BUILD
STATIC int32_t sched_get_tgid_by_vpid(int32_t vpid, int32_t *tgid)
{
    ka_task_struct_t *task = NULL;

    ka_task_rcu_read_lock();
    task = ka_task_get_pid_task(ka_task_find_vpid(vpid), KA_PIDTYPE_PID);
    ka_task_rcu_read_unlock();
    if (task == NULL) {
#if !defined (CFG_SOC_PLATFORM_MDC_V51) && !defined (CFG_SOC_PLATFORM_MDC_V11)
        sched_err("Failed to get pid task. (vpid=%d)\n", vpid);
#endif
        return DRV_ERROR_NO_PROCESS;
    }

    *tgid = ka_task_get_pid_for_task(task);
    ka_task_put_task_struct(task);

    return DRV_ERROR_NONE;
}
#endif

static int sched_query_task_gid(u32 devid, ESCHED_QUERY_TYPE type, struct esched_input_info *input,
    struct esched_output_info *output)
{
    struct esched_query_gid_output output_info = {0};
    struct esched_query_gid_input input_info;
    int ret;

    if ((input->inLen != sizeof(struct esched_query_gid_input)) ||
        (output->outLen != sizeof(struct esched_query_gid_output))) {
        sched_err("Invalid para. (inLen=%u; outLen=%u)\n", input->inLen, output->outLen);
        return DRV_ERROR_PARA_ERROR;
    }

    if (copy_from_user_safe(&input_info, (void *)(uintptr_t)(input->inBuff), input->inLen) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    if (type == QUERY_TYPE_LOCAL_GRP_ID) {
#ifndef AOS_LLVM_BUILD
        ret = sched_get_tgid_by_vpid(input_info.pid, &input_info.pid);
        if (ret != 0) {
            return ret;
        }
#endif
        ret = sched_query_local_task_gid(devid, input_info.pid, input_info.grp_name, &output_info.grp_id);
    } else {
#ifdef CFG_FEATURE_REMOTE_SUBMIT
        ret = sched_query_remote_task_gid(devid, input_info.pid, input_info.grp_name, &output_info.grp_id);
#else
        ret = DRV_ERROR_NOT_SUPPORT;
#endif
    }

    if (ret != 0) {
        return ret;
    }

    if (copy_to_user_safe((void *)output->outBuff, &output_info, output->outLen) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return 0;
}

STATIC int32_t sched_fop_query_info(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_query_info para;
    int ret;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(para)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    if ((para.input.inBuff == NULL) || (para.output.outBuff == NULL)) {
        sched_err("Invalid para. (inBuff=%u; outBuff=%u)\n", para.input.inBuff == NULL ? 1 : 0,
            para.output.outBuff == NULL ? 1 : 0);
        return DRV_ERROR_PARA_ERROR;
    }

    switch (para.type) {
        case QUERY_TYPE_LOCAL_GRP_ID:
        case QUERY_TYPE_REMOTE_GRP_ID:
            ret = sched_query_task_gid(sched_ioctl_devid(devid, para.dev_id),
                para.type, &para.input, &para.output);
            break;
        default:
#ifndef EMU_ST
            sched_warn("Query info type is not valid. (type=%u)\n", para.type);
#endif
            return DRV_ERROR_NOT_SUPPORT;
    }

    return ret;
}

STATIC int32_t sched_fop_query_sync_msg_trace(u32 devid, unsigned long arg)
{
#ifndef EMU_ST
#if !defined (CFG_SOC_PLATFORM_MDC_V51) && !defined (CFG_SOC_PLATFORM_MDC_V11)
    int32_t ret;
    struct sched_ioctl_para_trace para_info = {{0}};
    struct sched_trace_input *para = &para_info.input;

    if (copy_from_user_safe(para, (void *)(uintptr_t)arg, sizeof(struct sched_trace_input)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    ret = sched_query_sync_event_trace(sched_ioctl_devid(devid, para->dev_id), para->dev_pid, para->gid, para->tid, &para_info.trace);
    if (ka_unlikely(ret != 0)) {
        return ret;
    }

    if (copy_to_user_safe((void *)((uintptr_t)arg + sizeof(struct sched_trace_input)),
        &para_info.trace, sizeof(struct sched_sync_event_trace)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }
#endif
#endif
    return 0;
}

STATIC int32_t sched_fop_set_event_priority(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_set_event_pri para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_set_event_pri)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return sched_set_event_priority(sched_ioctl_devid(devid, para.dev_id), SCHED_CURRENT_TGID, para.event_id, para.pri);
}

STATIC int32_t sched_fop_set_process_priority(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_set_proc_pri para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_set_proc_pri)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return sched_set_process_priority(sched_ioctl_devid(devid, para.dev_id), SCHED_CURRENT_TGID, para.pri);
}

STATIC int32_t sched_fop_thread_subscribe_event(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_subscribe para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_subscribe)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return sched_thread_subscribe_event(sched_ioctl_devid(devid, para.dev_id), SCHED_CURRENT_TGID, para.gid,
        para.tid, para.event_bitmap);
}

static int32_t sched_fop_grp_set_max_event_num(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_set_event_max_num para;
    int ret;

    ret = copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_set_event_max_num));
    if (ret != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return sched_grp_set_max_event_num(sched_ioctl_devid(devid, para.dev_id), SCHED_CURRENT_TGID, para.gid,
        para.event_id, para.max_num);
}

STATIC int32_t sched_fop_get_exact_event(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_get_event para_info = {{0}};
    struct sched_get_event_input *para = &para_info.input;
    int32_t ret;

    if (copy_from_user_safe(para, (void *)(uintptr_t)arg, sizeof(struct sched_get_event_input)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    if ((para->msg == NULL) || (para->msg_len > SCHED_MAX_EVENT_MSG_LEN)) {
        sched_err("Invalid para. (dev_id=%u; msg_len=%u)\n", para->dev_id, para->msg_len);
        return DRV_ERROR_PARA_ERROR;
    }

    para_info.event.msg = para->msg;
    para_info.event.msg_len = para->msg_len;

    ret = sched_get_exact_event(sched_ioctl_devid(devid, para->dev_id), SCHED_CURRENT_TGID, para->grp_id,
        para->thread_id, para->event_id, &para_info.event);
    if (ka_unlikely(ret != 0)) {
        return ret;
    }

    if (copy_to_user_safe((void *)((uintptr_t)arg + sizeof(struct sched_get_event_input)),
        &para_info.event, sizeof(struct sched_subscribed_event)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return 0;
}

STATIC int32_t sched_fop_ack_event(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_ack para_info;
    struct sched_ack_event_para ack_para;
    char msg[SCHED_MAX_EVENT_MSG_LEN];

    if (copy_from_user_safe(&para_info, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_ack)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    if (((para_info.msg == NULL) && (para_info.msg_len != 0)) ||
        ((para_info.msg != NULL) && ((para_info.msg_len == 0) || (para_info.msg_len > SCHED_MAX_EVENT_MSG_LEN)))) {
        sched_err("The member msg_len in para_info is out of range. (msg_len=%u; max=%d)\n",
                  para_info.msg_len, SCHED_MAX_EVENT_MSG_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((para_info.msg != NULL) &&
        (copy_from_user_safe(msg, (void *)(uintptr_t)para_info.msg, para_info.msg_len) != 0)) {
        sched_err("Failed to invoke the copy_from_user_safe. (dev_id=%u; event_id=%u; msg_len=%u)\n",
            para_info.dev_id, para_info.event_id, para_info.msg_len);
        return DRV_ERROR_COPY_USER_FAIL;
    }

    ack_para.event_id = para_info.event_id;
    ack_para.subevent_id = para_info.subevent_id;
    ack_para.msg = msg;
    ack_para.msg_len = para_info.msg_len;
    return sched_ack_event(sched_ioctl_devid(devid, para_info.dev_id), SCHED_CURRENT_TGID, &ack_para);
}

STATIC int32_t sched_fop_wait_event(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_wait para_info = {{0}};
    struct sched_wait_input *para = &para_info.input;
    int32_t ret;

    if (copy_from_user_safe(para, (void *)(uintptr_t)arg, sizeof(struct sched_wait_input)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    if (((para->msg == NULL) || (para->msg_len != SCHED_MAX_EVENT_MSG_LEN)) &&
        (para->timeout != SCHED_THREAD_SWAPOUT)) {
        sched_err("The msg is NULL or msg_len is invalid. (dev_id=%u; msg_len=%u; timeout=%dms)\n",
            para->dev_id, para->msg_len, para->timeout);
        return DRV_ERROR_PARA_ERROR;
    }

    para_info.event.msg = para->msg;
    para_info.event.msg_len = para->msg_len;

    ret = sched_wait_event(sched_ioctl_devid(devid, para->dev_id), SCHED_CURRENT_TGID, para->grp_id,
        para->thread_id, para->timeout, &para_info.event);
    if (ka_unlikely(ret != 0)) {
        return ret;
    }

    if (copy_to_user_safe((void *)((uintptr_t)arg + sizeof(struct sched_wait_input)),
        &para_info.event, sizeof(struct sched_subscribed_event)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return 0;
}

STATIC int32_t sched_fop_get_event_trace(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_get_event_trace para;
    int32_t ret;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_get_event_trace)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    if (para.buff == NULL) {
        sched_err("Invalid para. (dev_id=%u)\n", para.dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = sched_get_event_trace(sched_ioctl_devid(devid, para.dev_id), para.buff, para.buff_len, &para.data_len);
    if (ka_unlikely(ret != 0)) {
        return ret;
    }

    if (copy_to_user_safe((void *)(uintptr_t)arg, &para, sizeof(struct sched_ioctl_para_get_event_trace)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return 0;
}

#ifdef CFG_FEATURE_TRACE_RECOED
STATIC int32_t sched_fop_trigger_sched_trace_record(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_trigger_sched_trace_record para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg,
        sizeof(struct sched_ioctl_para_trigger_sched_trace_record)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    para.record_reason[SCHED_STR_MAX_LEN - 1] = '\0';
    para.key[SCHED_STR_MAX_LEN - 1] = '\0';

    return sched_trigger_sched_trace_record(sched_ioctl_devid(devid, para.dev_id), para.record_reason, para.key);
}
#endif

int32_t sched_publish_event_para_check(struct sched_published_event_info *event_info)
{
    if (event_info->gid >= SCHED_MAX_GRP_NUM) {
        sched_err("The variable gid is out of range. "
                  "(gid=%u; max=%d)\n", event_info->gid, SCHED_MAX_GRP_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    if (event_info->event_id >= SCHED_MAX_EVENT_TYPE_NUM) {
        sched_err("The variable event_id is out of range. (event_id=%u; max=%d)\n",
                  event_info->event_id, SCHED_MAX_EVENT_TYPE_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    if (((event_info->msg == NULL) && (event_info->msg_len != 0)) ||
        ((event_info->msg != NULL) && ((event_info->msg_len == 0) || (event_info->msg_len > EVENT_MAX_MSG_LEN)))) {
        sched_err("The variable msg_len is out of range. (msg_len=%u; max=%d)\n",
                  event_info->msg_len, SCHED_MAX_EVENT_MSG_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

#if defined (ESCHED_HOST) && !defined (AOS_LLVM_BUILD) && !defined (EVENT_SCHED_UT) && !defined (EMU_ST)
STATIC int32_t sched_proc_mnt_ns_check(u32 chip_id, int pid)
{
    ka_mnt_namespace_t *mnt_ns = NULL;

    mnt_ns = sched_get_proc_mnt_ns(chip_id, pid);
    if (mnt_ns != ka_task_get_current_mnt_ns()) {
        sched_err("Not in the same container. (chip_id=%u, pid=%d)\n", chip_id, pid);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}
#endif

#define PRPC_HANDLE_MAX_NUM 3
STATIC sched_event_pre_proc sched_event_pre_proc_handle[SCHED_MAX_EVENT_TYPE_NUM][PRPC_HANDLE_MAX_NUM] = {{NULL, }, };

#ifndef EMU_ST
int sched_register_event_pre_proc_handle(unsigned int event_id, sched_event_pre_proc handle)
{
    int i;

    if (event_id >= SCHED_MAX_EVENT_TYPE_NUM) {
        sched_err("Invalid para. (event_id=%u; max=%u)\n", event_id, SCHED_MAX_EVENT_TYPE_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < PRPC_HANDLE_MAX_NUM; i++) {
        if (sched_event_pre_proc_handle[event_id][i] == NULL) {
            sched_event_pre_proc_handle[event_id][i] = handle;
            sched_info("Event register pre handle. (event_id=%u; index=%d)\n", event_id, i);
            return 0;
        }
    }

    sched_err("Event has already been registered too many handle. (event_id=%u)\n", event_id);
    return DRV_ERROR_PARA_ERROR;
}
KA_EXPORT_SYMBOL_GPL(sched_register_event_pre_proc_handle);

void sched_unregister_event_pre_proc_handle(unsigned int event_id, sched_event_pre_proc handle)
{
    int i;

    if (event_id >= SCHED_MAX_EVENT_TYPE_NUM) {
        sched_err("Invalid para. (event_id=%u; max=%u)\n", event_id, SCHED_MAX_EVENT_TYPE_NUM);
        return;
    }

    for (i = 0; i < PRPC_HANDLE_MAX_NUM; i++) {
        if (sched_event_pre_proc_handle[event_id][i] == handle) {
            sched_event_pre_proc_handle[event_id][i] = NULL;
        }
    }
}
KA_EXPORT_SYMBOL_GPL(sched_unregister_event_pre_proc_handle);
#endif

STATIC int sched_submit_event_pre_proc(unsigned int dev_id, struct sched_published_event_info *event_info,
    struct sched_published_event_func *event_func)
{
    int i, ret;

    for (i = 0; i < PRPC_HANDLE_MAX_NUM; i++) {
        if (sched_event_pre_proc_handle[event_info->event_id][i] != NULL) {
#ifndef EMU_ST
            ret = sched_event_pre_proc_handle[event_info->event_id][i](dev_id, event_info, event_func);
            if (ret != 0) {
                sched_err("Pre proc failed. (dev_id=%u; event_id=%u; i=%d)\n", dev_id, event_info->event_id, i);
                return ret;
            }
#endif
        }
    }
    return 0;
}

STATIC int32_t sched_submit_single_event(struct sched_numa_node *node, u32 event_src,
    struct sched_published_event_info *event_info, struct sched_published_event_func *event_func)
{
    return node->ops.sumbit_event(node->node_id, event_src, event_info, event_func);
}

STATIC int32_t sched_submit_multi_events(struct sched_numa_node *node, u32 event_src,
    struct sched_published_event_info *event_info, struct sched_published_event_func *event_func, unsigned long arg)
{
#ifndef AOS_LLVM_BUILD
    struct sched_ioctl_para_submit *usr_arg = (struct sched_ioctl_para_submit __ka_user *)(uintptr_t)arg;
    u32 stamp = (u32)ka_jiffies;
    unsigned int succ_num;
    int ret;

    if (event_info->event_num > (unsigned int)SCHED_MAX_BATCH_EVENT_NUM) {
        sched_err("event_num exceed limit %u. (node_id=%u; event_num=%u)\n",
            (unsigned int)SCHED_MAX_BATCH_EVENT_NUM, node->node_id, event_info->event_num);
        return DRV_ERROR_PARA_ERROR;
    }

    for (succ_num = 0; succ_num < event_info->event_num; succ_num++) {
        ret = node->ops.sumbit_event(node->node_id, event_src, event_info, event_func);
        if (ret != 0) {
            break;
        }
        esched_try_cond_resched_by_time(&stamp, ESCHED_WAKEUP_TIMEINTERVAL);
    }

    if (event_info->event_num == succ_num) {
        return 0;
    } else if (succ_num != 0) {
        ka_base_put_user(succ_num, &usr_arg->event_info.event_num);
        return 0;
    } else {
        return ret;
    }
#else
    sched_info("Not support for MDC yet.\n");
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

STATIC int sched_check_cp2_dest_pid(struct sched_published_event_info *event_info)
{
#ifndef EMU_ST
#if !defined(ESCHED_HOST) && !defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(CFG_SOC_PLATFORM_MDC_V11)
    int pid = current->tgid;
    u32 dev_id, vfid, host_pid;
    enum devdrv_process_type cp_type;
    int ret;

    ret = devdrv_query_process_host_pid(pid, &dev_id, &vfid, &host_pid, &cp_type);
    if (ret != 0) {
        return 0;
    }

    if ((cp_type == DEVDRV_PROCESS_CP2) && (event_info->pid != pid)) {
        sched_warn("Custom process is not allowed. (pid=%u; cp2_pid=%d)\n", event_info->pid, pid);
        return DRV_ERROR_INNER_ERR;
    }
#endif
#endif
    return 0;
}

STATIC int32_t sched_fop_submit_event(u32 devid, unsigned long arg)
{
    struct sched_numa_node *node = NULL;
    struct sched_ioctl_para_submit para;
    struct sched_published_event_info *event_info = NULL;
    struct sched_published_event_func event_func;
    int32_t ret;
    u32 chip_id;
    char msg[EVENT_MAX_MSG_LEN];

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_submit)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    chip_id = sched_ioctl_devid(devid, para.dev_id);

    node = sched_get_numa_node(chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (devid=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    event_info = &para.event_info;
    ret = sched_publish_event_para_check(event_info);
    if (ret != 0) {
        return ret;
    }

    /* aicpu security problem: for sched event, cp2 only can send to itself */
    if (event_info->dst_engine == ACPU_DEVICE) {
        ret = sched_check_cp2_dest_pid(event_info);
        if (ret != 0) {
            return ret;
        }
    }
    if (event_info->msg_len > 0) {
        if (copy_from_user_safe((void *)&msg, (void *)(uintptr_t)event_info->msg, event_info->msg_len) != 0) {
            sched_err("Copy from user fail. (devid=%u; subevent_id=%u; msg_len=%u)\n",
                devid, event_info->subevent_id, event_info->msg_len);
            return DRV_ERROR_COPY_USER_FAIL;
        }
        event_info->msg = (char *)&msg;
    }
#ifndef AOS_LLVM_BUILD
    if (esched_dst_engine_is_local(event_info->dst_engine)) {
        ret = sched_get_tgid_by_vpid(event_info->pid, &event_info->pid);
        if (ret != 0) {
            return ret;
        }
    }
#endif

#if defined ESCHED_HOST && !defined (AOS_LLVM_BUILD) && !defined (EVENT_SCHED_UT) && !defined (EMU_ST)
    if ((event_info->dst_engine == CCPU_HOST) || (event_info->dst_engine == ACPU_HOST)) {
        ret = sched_proc_mnt_ns_check(chip_id, event_info->pid);
        if (ret != 0) {
            return ret;
        }
    }
#endif

    event_func.event_ack_func = NULL;
    event_func.event_finish_func = NULL;

    ret = sched_submit_event_pre_proc(chip_id, event_info, &event_func);
    if (ret != 0) {
        return ret;
    }

    if (event_info->event_num == 1) {
        return sched_submit_single_event(node, SCHED_PUBLISH_FORM_USER, event_info, &event_func);
    } else {
        return sched_submit_multi_events(node, SCHED_PUBLISH_FORM_USER, event_info, &event_func, arg);
    }
}

int32_t sched_submit_event(uint32_t chip_id, struct sched_published_event *event)
{
    struct sched_published_event_info *event_info = NULL;
    struct sched_numa_node *node = NULL;

    int32_t ret;

    if (event == NULL) {
        sched_err("The event is NULL. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    event->event_info.publish_timestamp = sched_get_cur_timestamp();
    event->event_info.publish_timestamp_of_day = sched_do_gettimeofday();

    event_info = &event->event_info;
    event_info->tid = SCHED_INVALID_TID;
    event_info->dst_devid = SCHED_INVALID_DEVID;
    ret = sched_publish_event_para_check(event_info);
    if (ret != 0) {
        return ret;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = node->ops.sumbit_event(chip_id, SCHED_PUBLISH_FORM_KERNEL, event_info, &event->event_func);
    esched_dev_put(node);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(sched_submit_event);

int32_t sched_submit_event_to_thread(uint32_t chip_id, struct sched_published_event *event)
{
    struct sched_published_event_info *event_info = NULL;
    struct sched_numa_node *node = NULL;
    int32_t ret;

    if (event == NULL) {
        sched_err("The event is NULL. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (event->event_info.tid == SCHED_INVALID_TID) {
        sched_err("User specific tid not support. (tid=%u)\n", SCHED_INVALID_TID);
        return DRV_ERROR_PARA_ERROR;
    }

    event->event_info.publish_timestamp = sched_get_cur_timestamp();
    event->event_info.dst_devid = SCHED_INVALID_DEVID;
    event->event_info.publish_timestamp_of_day = sched_do_gettimeofday();

    event_info = &event->event_info;
    ret = sched_publish_event_para_check(event_info);
    if (ret != 0) {
        return ret;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = node->ops.sumbit_event(chip_id, SCHED_PUBLISH_FORM_KERNEL, event_info, &event->event_func);
    esched_dev_put(node);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(sched_submit_event_to_thread);

STATIC int32_t sched_fop_attach_proc_to_chip(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_attach para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_attach)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return sched_add_process(sched_ioctl_devid(devid, para.dev_id), SCHED_CURRENT_TGID);
}

STATIC int32_t sched_fop_dettach_proc_from_chip(u32 devid, unsigned long arg)
{
    struct sched_ioctl_para_detach para;

    if (copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct sched_ioctl_para_detach)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    return sched_del_process(sched_ioctl_devid(devid, para.dev_id), SCHED_CURRENT_TGID);
}

STATIC int32_t (* sched_ioctl_handler[SCHED_CMD_MAX_NR])(u32 devid, unsigned long arg) = {
    [_KA_IOC_NR(SCHED_SET_SCHED_CPU_ID)] = sched_fop_set_sched_cpu,
    [_KA_IOC_NR(SCHED_PROC_ADD_GRP_ID)] = sched_fop_proc_add_grp,
    [_KA_IOC_NR(SCHED_SET_EVENT_PRIORITY_ID)] = sched_fop_set_event_priority,
    [_KA_IOC_NR(SCHED_SET_PROCESS_PRIORITY_ID)] = sched_fop_set_process_priority,
    [_KA_IOC_NR(SCHED_THREAD_SUBSCRIBE_EVENT_ID)] = sched_fop_thread_subscribe_event,
    [_KA_IOC_NR(SCHED_GET_EXACT_EVENT_ID)] = sched_fop_get_exact_event,
    [_KA_IOC_NR(SCHED_ACK_EVENT_ID)] = sched_fop_ack_event,
    [_KA_IOC_NR(SCHED_WAIT_EVENT_ID)] = sched_fop_wait_event,
    [_KA_IOC_NR(SCHED_SUBMIT_EVENT_ID)] = sched_fop_submit_event,
    [_KA_IOC_NR(SCHED_ATTACH_PROCESS_TO_CHIP_ID)] = sched_fop_attach_proc_to_chip,
    [_KA_IOC_NR(SCHED_DETTACH_PROCESS_FROM_CHIP_ID)] = sched_fop_dettach_proc_from_chip,
    [_KA_IOC_NR(SCHED_GRP_SET_EVENT_MAX_NUM)] = sched_fop_grp_set_max_event_num,
    [_KA_IOC_NR(SCHED_QUERY_INFO)] = sched_fop_query_info,
    [_KA_IOC_NR(SCHED_QUERY_SYNC_MSG_TRACE)] = sched_fop_query_sync_msg_trace,
    [_KA_IOC_NR(SCHED_GET_NODE_EVENT_TRACE)] = sched_fop_get_event_trace,
#ifdef CFG_FEATURE_TRACE_RECOED
    [_KA_IOC_NR(SCHED_TRIGGER_SCHED_TRACE_RECORD_VALUE)] = sched_fop_trigger_sched_trace_record,
#endif
};

void esched_register_ioctl_cmd_func(int nr, int32_t (*fn)(u32 devid, unsigned long arg))
{
    sched_ioctl_handler[nr] = fn;
}

STATIC struct sched_task *esched_task_hash_find(int pid, unsigned int devid)
{
    struct sched_task *task = NULL;
    int key = pid & ESCHED_HASH_TABLE_MASK;

    ka_hash_for_each_possible(esched_task_table, task, hnode, key)
        if ((task->pid == pid) && (task->devid == devid)) {
            return task;
        }

    return NULL;
}

STATIC void esched_task_hash_add(struct sched_task *task)
{
    int key = task->pid & ESCHED_HASH_TABLE_MASK;

    ka_hash_add(esched_task_table, &task->hnode, key);
}

STATIC void esched_task_hash_del(int pid, unsigned int devid)
{
    struct sched_task *task_tmp = NULL;
    int key = (pid) & ESCHED_HASH_TABLE_MASK;
    
    ka_hash_for_each_possible(esched_task_table, task_tmp, hnode, key)
        if ((task_tmp->pid == pid) && (task_tmp->devid == devid)) {
            ka_hash_del(&task_tmp->hnode);
        }
}

STATIC int32_t esched_wait_task_release_finish(unsigned int devid)
{
    u32 timeout_cnt = ESCHED_MAX_TIMEOUT_CNT;
    struct sched_task *task = NULL;

    while (timeout_cnt > 0) {
        ka_system_msleep(ESCHED_MSLEEP_TIME);
        ka_task_mutex_lock(&esched_task_mutex);
        task = esched_task_hash_find((int)SCHED_CURRENT_TGID, devid);
        ka_task_mutex_unlock(&esched_task_mutex);
        if (task == NULL) {
            sched_debug("Wait pre release finish success. (devid=%u; pid=%u; wait_cnt=%u)\n",
                devid, (unsigned int)SCHED_CURRENT_TGID, (ESCHED_MAX_TIMEOUT_CNT - timeout_cnt));
            return DRV_ERROR_NONE;
        }
        timeout_cnt--;
    }
    return DRV_ERROR_WAIT_TIMEOUT;
}

STATIC int32_t sched_fop_open(ka_inode_t *inode, ka_file_t *filep)
{
    struct sched_task *task = NULL;
    struct sched_numa_node *node;
    unsigned int devid = 0;
    int ret;

    ret = 0;
    node = NULL;
#ifdef CFG_FEATURE_EXTERNAL_CDEV
    devid = drv_davinci_get_device_id(filep);
#endif
    ka_task_mutex_lock(&esched_task_mutex);

    task = esched_task_hash_find((int)SCHED_CURRENT_TGID, devid);
    if ((task != NULL) && (task->status == TASK_RELEASE_INACTIVE)) {
        ka_task_mutex_unlock(&esched_task_mutex);
        sched_err("esched has been opened by task.(pid=%d)\n", SCHED_CURRENT_TGID);
        return DRV_ERROR_OPEN_FAILED;
    }

    if ((task != NULL) && (task->status == TASK_RELEASE_ACTIVE)) {
        ka_task_mutex_unlock(&esched_task_mutex);
        ret = esched_wait_task_release_finish(devid);
        if (ret != DRV_ERROR_NONE) {
            sched_err("Pre-esched task not be released.(pid=%d)\n", SCHED_CURRENT_TGID);
            return DRV_ERROR_OPEN_FAILED;
        }
        ka_task_mutex_lock(&esched_task_mutex);
    }

    task = sched_kzalloc(sizeof(*task), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(task == NULL)) {
        ka_task_mutex_unlock(&esched_task_mutex);
        sched_err("esched alloc task memory failed.(pid=%d)\n", SCHED_CURRENT_TGID);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    ret = devdrv_manager_container_check_devid_in_container_ns(devid, ka_task_get_current());
    if (ret != 0) {
        sched_kfree(task);
        ka_task_mutex_unlock(&esched_task_mutex);
        sched_err("Failed to check device in container. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    node = esched_dev_get(devid);
    if (node == NULL) {
        sched_kfree(task);
        ka_task_mutex_unlock(&esched_task_mutex);
        sched_err("Invalid device. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    esched_dev_put(node);
#endif

    task->devid = devid;
    task->pid = SCHED_CURRENT_TGID;
    task->status = TASK_RELEASE_INACTIVE;
    esched_task_hash_add(task);
    ka_fs_set_file_private_data(filep, (void *)task);

    ka_task_mutex_unlock(&esched_task_mutex);

    return 0;
}

#ifdef CFG_FEATURE_EXTERNAL_CDEV
STATIC int sched_pre_release(ka_file_t *filep, unsigned long mode)
{
    struct sched_task *task = ka_fs_get_file_private_data(filep);

    if (task == NULL) {
        return 0;
    }
    task->status = TASK_RELEASE_ACTIVE;
    sched_debug("sched pre_release active. (pid=%u; devid=%u)\n", (unsigned int)task->pid, task->devid);
    return 0;
}
#endif

STATIC int32_t sched_fop_release(ka_inode_t *inode, ka_file_t *filep)
{
    struct sched_task *task = ka_fs_get_file_private_data(filep);
    struct sched_numa_node *node;
    unsigned int devid;
    unsigned int i;
    int pid;

    if (task == NULL) {
        return 0;
    }
    pid = (int)task->pid;
    devid = task->devid;
    node = NULL;
    i = 0;

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    node = esched_dev_get(devid);
    if (node == NULL) {
        return 0;
    }

    (void)sched_del_process(devid, pid);

    esched_dev_put(node);
#else
    for (i = 0; i < SCHED_MAX_CHIP_NUM; i++) {
        node = esched_dev_get(i);
        if (node == NULL) {
            return 0;
        }
        (void)sched_del_process(i, pid);
        esched_dev_put(node);
    }
#endif

    ka_task_mutex_lock(&esched_task_mutex);
    esched_task_hash_del(pid, devid);
    ka_task_mutex_unlock(&esched_task_mutex);
 
    sched_kfree(task);
    module_feature_auto_uninit_task(pid);

    return 0;
}

STATIC long _sched_fop_ioctl(uint32_t devid, uint32_t cmd, unsigned long arg)
{
    if (_KA_IOC_NR(cmd) >= SCHED_CMD_MAX_NR) {
        sched_err("The command is invalid, which is out of range. (cmd=%u)\n", _KA_IOC_NR(cmd));
        return DRV_ERROR_INNER_ERR;
    }

    if (sched_ioctl_handler[_KA_IOC_NR(cmd)] == NULL) {
        sched_err("The command is invalid. (cmd=%u)\n", _KA_IOC_NR(cmd));
        return DRV_ERROR_INNER_ERR;
    }

    return sched_ioctl_handler[_KA_IOC_NR(cmd)](devid, arg);
}

STATIC long sched_fop_ioctl(ka_file_t *filep, uint32_t cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_EXTERNAL_CDEV
    struct sched_numa_node *node = NULL;
#endif
    uint32_t devid = SCHED_MAX_CHIP_NUM;
    int pid;
    long ret;
    struct sched_task *task = ka_fs_get_file_private_data(filep);
    if (task == NULL) {
        sched_err("Private_data is NULL.\n");
        return DRV_ERROR_FILE_OPS;
    }
    pid = (int)task->pid;

    if (pid != SCHED_CURRENT_TGID) { /* forked-process use father process fd, is not support */
        return DRV_ERROR_FILE_OPS;
    }

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    devid = drv_davinci_get_device_id(filep);
    node = esched_dev_get(devid); /* Ensure that subsequent access to device objects is secure. */
    if (node == NULL) {
        /* After the VM goes offline, the user process(eg:tsd) may still be running. */
        return DRV_ERROR_NO_DEVICE;
    }
#endif

    ret = _sched_fop_ioctl(devid, cmd, arg);
#ifdef CFG_FEATURE_EXTERNAL_CDEV
    esched_dev_put(node);
#endif

    return ret;
}

STATIC ka_file_operations_t event_sched_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(sched_fop_open)
    ka_fs_init_f_release(sched_fop_release)
    ka_fs_init_f_unlocked_ioctl(sched_fop_ioctl)
};

#ifdef CFG_FEATURE_EXTERNAL_CDEV
static const struct notifier_operations event_notifier_ops = {
    .notifier_call = sched_pre_release,
};
#endif

#if (!defined CFG_FEATURE_EXTERNAL_CDEV) && (!defined AOS_LLVM_BUILD)
STATIC char *sched_devnode(ka_device_t *dev, umode_t *mode)
{
    return NULL;
}

STATIC int32_t sched_register_driver(void)
{
    int32_t ret;

    ret = ka_fs_alloc_chrdev_region(&char_dev.devno, 0, MINOR_DEV_COUNT, SCHED_CHAR_DEV_NAME);
    if (ret < 0) {
        sched_err("Failed to invoke the alloc_chrdev_region. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    ka_fs_cdev_init(&char_dev.cdev, &event_sched_fops);
    ka_fs_set_cdev_owner(&char_dev.cdev, KA_THIS_MODULE);

    if (ka_fs_cdev_add(&char_dev.cdev, char_dev.devno, 1) != 0) {
        sched_err("Failed to invoke the cdev_add. (devno=%u)\n", char_dev.devno);
        goto unregister_chrdev_region;
    }

    char_dev.dev_class = ka_driver_class_create(KA_THIS_MODULE, SCHED_CHAR_DEV_NAME);

    if (KA_IS_ERR(char_dev.dev_class)) {
        sched_err("Failed to invoke the class_create.\n");
        goto cdev_del;
    }

#ifndef EMU_ST
    ka_driver_class_set_devnode(char_dev.dev_class, sched_devnode);
#endif
    char_dev.device = ka_driver_device_create(char_dev.dev_class, NULL, char_dev.devno, NULL, SCHED_CHAR_DEV_NAME);
    if (KA_IS_ERR(char_dev.device)) {
        sched_err("Failed to invoke the device_create.\n");
        goto class_destroy;
    }
    sched_debug("Called sched_register_cdev successfully.\n");

    return 0;

class_destroy:
    ka_driver_class_destroy(char_dev.dev_class);
cdev_del:
    ka_fs_cdev_del(&char_dev.cdev);
unregister_chrdev_region:
    ka_fs_unregister_chrdev_region(char_dev.devno, 1);
    return DRV_ERROR_INNER_ERR;
}

STATIC void sched_unregister_driver(void)
{
    ka_driver_device_destroy(char_dev.dev_class, char_dev.devno);
    ka_driver_class_destroy(char_dev.dev_class);
    ka_fs_cdev_del(&char_dev.cdev);
    ka_fs_unregister_chrdev_region(char_dev.devno, 1);
}
#endif

STATIC int32_t sched_register_cdev(void)
{
#ifdef CFG_FEATURE_EXTERNAL_CDEV /* dc */
    int ret;
    ret = drv_davinci_register_sub_module(DAVINCI_ESCHED_SUB_MODULE_NAME, &event_sched_fops);
    if (ret != 0) {
        sched_err("Module register fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = drv_ascend_register_notify(DAVINCI_ESCHED_SUB_MODULE_NAME, &event_notifier_ops);
    if (ret != 0) {
        (void)drv_ascend_unregister_sub_module(DAVINCI_ESCHED_SUB_MODULE_NAME);
        sched_err("Notifier register fail. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
#else /* helper host (Independent working mode) or mdc */
    return sched_register_driver();
#endif
}

STATIC void sched_unregister_cdev(void)
{
#ifdef CFG_FEATURE_EXTERNAL_CDEV
    (void)drv_ascend_unregister_sub_module(DAVINCI_ESCHED_SUB_MODULE_NAME);
#else
    sched_unregister_driver();
#endif
}

STATIC void sched_debugfs_init(void)
{
#ifdef CFG_FEATURE_EXTERNAL_CDEV
    ka_device_t *dev = NULL;

    dev = davinci_intf_get_owner_device();
    if (dev == NULL) {
        sched_err("Failed to invoke devdrv_device_intf_get_owner_device.\n");
        return;
    }

    sched_sysfs_init(dev);
#else
    sched_sysfs_init(char_dev.device);
#endif
}

STATIC void sched_debugfs_uninit(void)
{
#ifdef CFG_FEATURE_EXTERNAL_CDEV
    ka_device_t *dev = NULL;

    dev = davinci_intf_get_owner_device();
    if (dev == NULL) {
        sched_err("Failed to invoke devdrv_device_intf_get_owner_device.\n");
        return;
    }

    sched_sysfs_uninit(dev);
#else
    sched_sysfs_uninit(char_dev.device);
#endif
}

STATIC int32_t __init sched_init_module(void)
{
    int32_t ret;

    /* register cdev */
    ret = sched_register_cdev();
    if (ret != 0) {
        sched_err("Failed to invoke the sched_register_cdev. (ret=%d)\n", ret);
        return ret;
    }

    ret = esched_init();
    if (ret != 0) {
        sched_unregister_cdev();
        sched_err("Failed to invoke the esched_init. (ret=%d)\n", ret);
        return ret;
    }

    ret = module_feature_auto_init();
    if (ret != 0) {
        esched_uninit();
        sched_unregister_cdev();
        sched_err("Failed to invoke the esched_table_init. (ret=%d)\n", ret);
        return ret;
    }

    sched_debugfs_init();
    sched_debug("Called sched_init_module successfully.\n");
    return 0;
}

STATIC void __exit sched_exit_module(void)
{
    sched_debugfs_uninit();
    module_feature_auto_uninit();
    esched_uninit();
    sched_unregister_cdev();
    sched_debug("Called sched_exit_module successfully.\n");
}

ka_module_init(sched_init_module);
ka_module_exit(sched_exit_module);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("SCHEDULER DRIVER");
#if defined(CFG_FEATURE_HARDWARE_MIA) && defined(ESCHED_HOST)
KA_MODULE_SOFTDEP("pre: ascend_trs_sec_eh_adapt");
#endif

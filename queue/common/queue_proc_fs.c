/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-06-01
 */
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"
#include "ka_compiler_pub.h"

#include "securec.h"

#include "queue_module.h"
#include "queue_proc_fs.h"

#define PROC_FS_NAME_LEN 32
#define PROC_FS_R_MODE 0444
#define PROC_FS_RW_MODE 0644

static ka_proc_dir_entry_t *queue_top_entry = NULL;
static ka_proc_dir_entry_t *queue_process_entry = NULL;

static int queue_normal_status_show(ka_seq_file_t *seq, void *offset)
{
#ifndef EMU_ST
    struct queue_qid_status *status = (struct queue_qid_status *)seq->private;

    queue_show_one_qid_status(seq, status);
    return 0;
#endif
}

static int queue_normal_status_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, queue_normal_status_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t normal_status_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(queue_normal_status_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};

static int queue_abnormal_status_show(ka_seq_file_t *seq, void *offset)
{
    ka_fs_seq_printf(seq, "abnormal_queue_status_show\n");
    queue_show_all_qid_status(seq, RECORD_EXCEPT);

    return 0;
}

static int queue_abnormal_status_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, queue_abnormal_status_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t abnormal_status_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(queue_abnormal_status_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};

#ifndef EMU_ST
static int queue_perf_switch_show(ka_seq_file_t *seq, void *offset)
{
    ka_fs_seq_printf(seq, "perf_queue_switch_show\n");
    queue_show_perf_switch(seq);

    return 0;
}

static int queue_perf_switch_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, queue_perf_switch_show, ka_base_pde_data(inode));
}

#define PERF_SWITCH_MAX_LEN 10
static ssize_t queue_perf_switch_write(ka_file_t *file, const char __ka_user *buffer, size_t count, loff_t *ppos)
{
    char kstr[PERF_SWITCH_MAX_LEN] = {0};
    char *time_threshold_ptr = NULL;
    char *perf_switch_ptr = NULL;
    u32 time_threshold = 0;
    bool set_perf_switch = false;
    int ret;

    if (ka_task_get_mnt_ns(ka_task_get_current()) != ka_task_get_mnt_ns(ka_task_get_init_task())) {
        queue_err("No permission to set perf switch.\n");
        return -EINVAL;
    }

    if ((count == 0) || (count > PERF_SWITCH_MAX_LEN)) {
        queue_err("Input count out of range. (count=%lu; max=%u)\n", count, PERF_SWITCH_MAX_LEN);
        return -EINVAL;
    }

    if (ka_base_copy_from_user(kstr, (void *)buffer, count)) {
        queue_err("Copy from user fail.\n");
        return -EINVAL;
    }
    kstr[count - 1] = '\0';

    perf_switch_ptr = strtok_s(kstr, " ", &time_threshold_ptr);
    if (perf_switch_ptr != NULL) {
        ret = ka_base_kstrtobool(perf_switch_ptr, &set_perf_switch);
        if (ret) {
            queue_err("Kstr to bool failed. (ret=%d)\n", ret);
            return -EINVAL;
        }
        ret = ka_base_kstrtouint(time_threshold_ptr, 10, &time_threshold);
        if (ret != 0) {
            time_threshold = 0;
        }
    }

    if (set_perf_switch == false) {
        queue_free_one_type_qid_status(RECORD_PERF);
        time_threshold = 0;
    }
    queue_set_perf_switch(set_perf_switch, time_threshold);

    return (ssize_t)count;
}

static const ka_procfs_ops_t perf_switch_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(queue_perf_switch_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_write(queue_perf_switch_write)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)

};

static int queue_perf_status_show(ka_seq_file_t *seq, void *offset)
{
    ka_fs_seq_printf(seq, "perf_queue_status_show\n");
    queue_show_all_qid_status(seq, RECORD_PERF);

    return 0;
}

static int queue_perf_status_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, queue_perf_status_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t perf_status_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE)
    ka_fs_init_pf_open(queue_perf_status_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_single_release)
};
#endif

static void proc_fs_format_qid_dir_name(u32 qid, char *name, u32 len)
{
    if (sprintf_s(name, len, "qid-%u", qid) <= 0) {
        queue_info("Qid sprintf_s unsuccessful. (qid=%u)\n", qid);
    }
}

static ka_proc_dir_entry_t *proc_fs_mk_qid_dir(u32 qid, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    if (parent == NULL) {
        return NULL;
    }
    proc_fs_format_qid_dir_name(qid, name, PROC_FS_NAME_LEN);
    return ka_fs_proc_mkdir((const char *)name, parent);
}

void queue_proc_fs_add_qid(struct queue_qid_status *status, ka_proc_dir_entry_t *parent)
{
    ka_proc_dir_entry_t *entry = NULL;

    if (status == NULL) {
        return;
    }

    if (ka_base_atomic_cmpxchg(&status->qid_dir_exit, QID_DIR_NO_EXIT, QID_DIR_IS_EXIT) != QID_DIR_NO_EXIT) {
        return;
    }

    entry = proc_fs_mk_qid_dir(status->qid, parent);
    if (entry == NULL) {
        ka_base_atomic_set(&status->qid_dir_exit, QID_DIR_NO_EXIT);
        queue_info("Create qid entry dir unsuccessful. (qid=%u)\n", status->qid);
        return;
    }

    (void)ka_fs_proc_create_data("queue_status", PROC_FS_R_MODE, entry, &normal_status_ops, status);
}

#ifndef EMU_ST
static void proc_fs_rm_qid_dir(u32 qid, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    if (parent == NULL) {
        return;
    }

    proc_fs_format_qid_dir_name(qid, name, PROC_FS_NAME_LEN);
    (void)ka_fs_remove_proc_subtree((const char *)name, parent);
}

void queue_proc_fs_del_qid(struct queue_qid_status *status, ka_proc_dir_entry_t *parent)
{
    proc_fs_rm_qid_dir(status->qid, parent);
    ka_base_atomic_set(&status->qid_dir_exit, QID_DIR_NO_EXIT);
}
#endif
static void proc_fs_format_process_dir_name(ka_pid_t pid, char *name, u32 len)
{
    if (sprintf_s(name, len, "%d", pid) <= 0) {
        queue_info("Pid sprintf_s unsuccessful. (pid=%d)\n", pid);
    }
}

static ka_proc_dir_entry_t *proc_fs_mk_process_dir(ka_pid_t pid, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    if (parent == NULL) {
        return NULL;
    }
    proc_fs_format_process_dir_name(pid, name, PROC_FS_NAME_LEN);
    return ka_fs_proc_mkdir((const char *)name, parent);
}

static void proc_fs_rm_process_dir(ka_pid_t pid, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    if (parent == NULL) {
        return;
    }

    proc_fs_format_process_dir_name(pid, name, PROC_FS_NAME_LEN);
    (void)ka_fs_remove_proc_subtree((const char *)name, parent);
}

void queue_proc_fs_add_process(struct queue_context *ctx)
{
    ctx->entry = proc_fs_mk_process_dir(ctx->pid, queue_process_entry);
    if (ctx->entry == NULL) {
        queue_info("Create process entry dir unsuccessful. (pid=%u)\n", ctx->pid);
        return;
    }
}

void queue_proc_fs_del_process(struct queue_context *ctx)
{
    proc_fs_rm_process_dir(ctx->pid, queue_process_entry);
}

void queue_proc_fs_init(void)
{
    ka_proc_dir_entry_t *abnormal_entry = NULL;
    ka_proc_dir_entry_t *perf_entry = NULL;

    queue_status_record_mng_init();
    queue_top_entry = ka_fs_proc_mkdir("queue", NULL);
    if (queue_top_entry != NULL) {
        queue_process_entry = ka_fs_proc_mkdir("process", queue_top_entry);
        abnormal_entry = ka_fs_proc_mkdir("except_collect", queue_top_entry);
        if (abnormal_entry != NULL) {
            (void)ka_fs_proc_create_data("except_queue_status", PROC_FS_R_MODE, abnormal_entry,
                &abnormal_status_ops, NULL);
        }
        perf_entry = ka_fs_proc_mkdir("perf_collect", queue_top_entry);
        if (perf_entry != NULL) {
#ifndef EMU_ST
            (void)ka_fs_proc_create_data("perf_switch", PROC_FS_RW_MODE, perf_entry,
                &perf_switch_ops, NULL);
            (void)ka_fs_proc_create_data("perf_queue_status", PROC_FS_R_MODE, perf_entry,
                &perf_status_ops, NULL);
#endif
        }
    }
}

void queue_proc_fs_uninit(void)
{
    if (queue_top_entry != NULL) {
        (void)ka_fs_remove_proc_subtree("queue", NULL);
    }
    queue_free_all_type_qid_status();
}


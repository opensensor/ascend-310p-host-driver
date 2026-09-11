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

#ifndef SVM_PROC_MNG_H
#define SVM_PROC_MNG_H

#include "devmm_proc_info.h"
#include "svm_cmd.h"

/* proc_status */
#define DEVMM_SVM_THREAD_EXITING   0x2   /* svm proc exiting, above pcie msg */
#define DEVMM_SVM_THREAD_WAIT_EXIT 0x4   /* svm proc wait exit, wait pcie msg proc over */
#define DEVMM_RELEASE_WAIT_FOREVER   0xFFFFFFFF

#define DEVMM_SVM_PROC_ABORT_STATE (DEVMM_SVM_THREAD_EXITING | DEVMM_SVM_THREAD_WAIT_EXIT)

#define DEVMM_HOST_IS_VIRT 0
#define DEVMM_HOST_IS_PHYS 1
#define DEVMM_HOST_IS_UNKNOWN 2

struct devmm_svm_proc_list_node {
    ka_list_head_t list;
    struct devmm_svm_process *svm_proc;
};

typedef bool (*proc_mng_cmp_fun)(struct devmm_svm_process *svm_proc, u64 cmp_arg);
typedef bool (*proc_mng_cmp_fun_by_both_pid)(struct devmm_svm_process *svm_proc, ka_pid_t devpid, ka_pid_t hostpid);
typedef void (*srcu_mng_release_fun)(u64 arg);
typedef void (*proc_mng_callback_hander)(struct devmm_svm_process *, u64);
typedef void (*proc_mng_put_hander)(struct devmm_svm_process *);

//extern ka_task_t init_task;

int devmm_add_to_svm_proc_hashtable(struct devmm_svm_process *svm_proc);
void devmm_wait_exit_and_del_from_hashtable(struct devmm_svm_process *svm_proc);
void devmm_wait_exit_and_del_from_hashtable_lock(struct devmm_svm_process *svm_proc);
void devmm_del_from_svm_proc_hashtable_lock(struct devmm_svm_process *svm_proc);
void devmm_svm_proc_wait_exit(struct devmm_svm_process *svm_proc);

struct devmm_svm_process *devmm_get_svm_proc_by_mm(ka_mm_struct_t *mm);
struct devmm_svm_process *devmm_get_svm_proc_from_file(ka_file_t *file);
struct devmm_custom_process *devmm_get_svm_custom_proc_by_mm(ka_mm_struct_t *mm);
struct devmm_svm_process *devmm_get_svm_proc_by_custom_mm(ka_mm_struct_t *mm);

struct devmm_svm_process *devmm_svm_proc_get_by_custom_mm(ka_mm_struct_t *mm);
struct devmm_svm_process *devmm_svm_proc_get_by_mm(ka_mm_struct_t *mm);
struct devmm_svm_process *devmm_svm_proc_get_by_devpid(int dev_pid);
struct devmm_svm_process *devmm_svm_proc_get_by_both_pid(int dev_pid, int hostpid);
struct devmm_svm_process *devmm_svm_proc_get_by_process_id(struct devmm_svm_process_id *process_id);
struct devmm_svm_process *devmm_svm_proc_get_by_process_id_ex(struct devmm_svm_process_id *process_id);
void devmm_svm_proc_put(struct devmm_svm_process *svm_proc);

void devmm_svm_procs_get(struct devmm_svm_process *svm_procs[], u32 *num);
void devmm_svm_procs_put(struct devmm_svm_process *svm_procs[], u32 num);

void devmm_svm_proc_list_get_by_dev(u32 devid, ka_list_head_t *list);
void devmm_svm_proc_list_put(ka_list_head_t *list, proc_mng_put_hander handle);

bool devmm_test_and_set_init_flag(ka_file_t *file);
int devmm_alloc_svm_proc_set_to_file(ka_file_t *file);

struct devmm_svm_process *devmm_alloc_svm_proc(void);
void devmm_free_svm_proc(struct devmm_svm_process *svm_proc);

int devmm_add_svm_proc_pid(struct devmm_svm_process *svm_proc,
    struct devmm_svm_process_id *process_id, int dev_pid);
int devmm_add_svm_proc_pid_lock(struct devmm_svm_process *svm_proc,
    struct devmm_svm_process_id *process_id, int dev_pid);
void devmm_del_first_svm_proc_pid(struct devmm_svm_process *svm_proc);
void devmm_del_first_svm_proc_pid_lock(struct devmm_svm_process *svm_proc);
void devmm_set_svm_proc_state(struct devmm_svm_process *svm_proc, u32 state);

void devmm_modify_process_status(
    struct devmm_svm_process *svm_proc, u32 devid, u32 vfid, processStatus_t pid_status, bool new_state);
u32 devmm_get_process_status(struct devmm_svm_process *svm_proc, u32 devid, u32 vfid, processStatus_t status);
void devmm_oom_ref_inc(struct devmm_svm_process *svm_proc);
void devmm_set_oom_ref(struct devmm_svm_process *svm_proc, u64 value);
u64 devmm_get_oom_ref(struct devmm_svm_process *svm_proc);

void devmm_svm_mmu_notifier_unreg(struct devmm_svm_process *svm_proc);
void devmm_svm_release_proc(struct devmm_svm_process *svm_proc);

int devmm_get_hostpid_by_docker_id(u32 docker_id, u32 phy_devid, u32 vfid, int *pids, u32 cnt);

int devmm_srcu_add_release_node(u64 arg, srcu_mng_release_fun);
struct devmm_svm_process *devmm_sreach_svm_proc_each_get_and_del(proc_mng_cmp_fun cmp_fun, u64 cmp_arg);

int devmm_svm_proc_mng_init(void);
void devmm_svm_proc_mng_uinit(void);
int devmm_svm_proc_init_private(struct devmm_svm_process *svm_proc);
void devmm_svm_proc_uninit_private(struct devmm_svm_process *svm_proc);

int devmm_get_host_phy_mach_flag(u32 devid, u32 *host_flag);
int devmm_get_host_run_mode(u32 devid);
bool devmm_is_hccs_vm_scene(u32 dev_id, u32 host_mode);
void devmm_notify_ts_drv_to_release(u32 devid, ka_pid_t pid);
void devmm_free_proc_priv_data(struct devmm_svm_process *svm_proc);
bool devmm_thread_is_run_in_docker(void);

int devmm_get_real_phy_devid(u32 devid, u32 vfid, u32 *phy_devid);
void devmm_destroy_svm_proc_host_pin_heap(struct devmm_svm_process *svm_proc);

#endif /* __SVM_PROC_MNG_H__ */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-12-15
 */
#ifndef SVM_SHMEM_NODE_H
#define SVM_SHMEM_NODE_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/proc_fs.h>

#include "svm_cmd.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "svm_dev_res_mng.h"

#define DEVMM_IPC_MEM_NAME_SIZE 65

#define IPC_WLIST_NUM       32768U    /* Total wlist number 1024 * 32 */
#define IPC_WLIST_SET_NUM   (DEVMM_SECONDARY_PROCESS_NUM - 1)   /* Max pid number at one time */

#define IPC_RANDOM_LENGTH 18
#define IPC_NAME_SIZE 65

struct devmm_ipc_node_attr;

typedef int (*devmm_ipc_node_init_func_t)(struct devmm_ipc_node_attr *attr);
typedef void (*devmm_ipc_node_uninit_func_t)(struct devmm_ipc_node_attr *attr);

typedef int (*setpid_pod_msg_send_func_t)(const char *name, struct svm_id_inst *inst,
    u32 sdid, ka_pid_t pid[], u32 pid_num);

struct devmm_ipc_node_attr {
    char name[DEVMM_IPC_MEM_NAME_SIZE];
    struct svm_id_inst inst;

    u32 sdid; /* Which pod requested to create this ipc node */
    devmm_ipc_node_init_func_t init_fn;
    devmm_ipc_node_uninit_func_t uninit_fn;

    struct devmm_svm_process *svm_proc; /* Creator svm proc */
    ka_pid_t pid;  /* Creator pid */
    u64 vptr;   /* Creator vptr */
    size_t len;

    size_t page_size;
    bool is_huge;
    bool is_reserve_addr;
    bool need_set_wlist; /* True means that the user needs to actively call the interface to set pid */
    u64 mem_map_route;
};

struct ipc_node_wlist {
    u32 sdid; /* server id */
    struct devmm_svm_process *svm_proc;
    ka_pid_t pid;
    unsigned long set_time;
    u64 vptr;
    ka_list_head_t list;
};

struct devmm_ipc_node {
    struct devmm_ipc_node_attr attr;
    bool valid; /* Is ipc node destroyed */
    bool is_open_page_freed;
    u32 key;

    u32 wlist_num;
    ka_list_head_t wlist_head;
    ka_hlist_node_t link;
    ka_mutex_t mutex;
    ka_kref_t ref;
#ifdef CONFIG_PROC_FS
    ka_proc_dir_entry_t *entry;
#endif
    bool need_async_recycle;
    bool mem_repair_record;
};

struct devmm_ipc_node_ops {
    int (*update_node_attr)(struct devmm_ipc_node_attr *attr);
    void (*destory_node)(struct devmm_ipc_node *node);
    bool (*is_local_pod)(u32 devid, u32 sdid);
    void (*clear_fault_sdid)(ka_pid_t pid, bool async_recycle);
    int (*pod_mem_repair)(struct devmm_ipc_node *node);
};

struct devmm_ipc_proc_node {
    char name[DEVMM_IPC_MEM_NAME_SIZE];
    ka_pid_t pid;
    u64 vptr;
    size_t len;
    ka_list_head_t list;
    u32 devid;
};

struct devmm_ipc_setpid_attr {
    char *name;

    struct svm_id_inst inst;
    u32 sdid;
    ka_pid_t creator_pid;
    ka_pid_t *pid;
    u32 pid_num;

    setpid_pod_msg_send_func_t send;
};

void devmm_ipc_node_ops_register(struct devmm_ipc_node_ops *ops);
int devmm_ipc_node_create(struct devmm_ipc_node_attr *attr);
int devmm_ipc_node_destroy(const char *name, ka_pid_t pid, bool async_recycle);

int devmm_ipc_node_set_pids(struct devmm_ipc_setpid_attr *attr);
int devmm_ipc_node_set_pids_ex(struct devmm_ipc_node_attr *attr, u32 sdid,
    ka_pid_t creator_pid, ka_pid_t pid[], u32 pid_num);

int devmm_ipc_node_open(const char *name, struct devmm_svm_process *svm_proc, u64 vptr);
int devmm_ipc_node_close(const char *name, struct devmm_svm_process *svm_proc, u64 vptr);

int devmm_ipc_proc_node_add(struct devmm_svm_process *svm_proc, struct devmm_ipc_node_attr *attr, int op);
void devmm_ipc_proc_node_del(struct devmm_svm_process *svm_proc, u64 vptr, int op);

int devmm_ipc_query_node_attr(const char *name, struct devmm_ipc_node_attr *attr);
int devmm_ipc_proc_query_name_by_va(struct devmm_svm_process *svm_proc, u64 vptr, int op, char *name);

void devmm_ipc_proc_node_recycle(struct devmm_svm_process *svm_proc, u32 devid);

void devmm_ipc_proc_node_free_open_pages(struct devmm_svm_process *svm_proc, u64 vptr, u64 page_num, u32 page_size);

int devmm_ipc_proc_find_create_node(struct devmm_svm_process *svm_proc, u64 vptr, size_t len);

int devmm_ipc_node_get_inst_by_name(const char *name, struct svm_id_inst *inst);

/* Query attr by creator va */
int devmm_ipc_proc_query_attr_by_va(struct devmm_svm_process *svm_proc, u64 vptr, int op,
    struct devmm_ipc_node_attr *attr);

int devmm_ipc_create_mem_repair_post_process(struct devmm_svm_process *svm_proc, u64 addr, u64 len);
int devmm_ipc_node_mem_repair_by_name(const char *name, ka_pid_t pid);

void devmm_ipc_node_clean_all_by_dev_res_mng(struct devmm_dev_res_mng *res_mng);

void devmm_ipc_node_init(void);
void devmm_ipc_node_uninit(void);

int devmm_ipc_set_mem_map_attr(const char *name, u64 attr);
int devmm_ipc_get_mem_map_attr(const char *name, u64 *attr);

int devmm_ipc_set_no_wlist_in_server_attr(const char *name, u64 attr);
int devmm_ipc_get_no_wlist_in_server_attr(const char *name, u64 *attr);

#endif

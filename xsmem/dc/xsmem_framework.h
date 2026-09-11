/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#ifndef XSMEM_FRAMEWORK_H
#define XSMEM_FRAMEWORK_H

#include "xsmem_drv_alloc_interface.h"
#include "xsmem_cmd.h"
#include "ka_net_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"

#define XSMEM_DEVICE_NAME "xsmem_dev"

#define XSMEM_NOT_CONFIRM_USER_ID   0xffffffff
#define XSMEM_ROOT_USER_ID          0x0

#ifdef EMU_ST
#define STATIC
#define THREAD __thread
#else
#define STATIC static
#define THREAD
#endif

struct xsm_adding_task {
    int pid;
    GroupShareAttr attr;
    u64 start_time;
    ka_list_head_t    pool_node;
};

struct xsm_pool {
    int          pool_id;
    unsigned int priv_mbuf_flag;
    ka_atomic_t  refcnt;
    int          task_id;
    ka_mutex_t   mutex;

    ka_list_head_t    node_list_head;
    ka_list_head_t    register_task_list;
    ka_hlist_node_t   hnode;

    struct xsm_pool_algo   *algo;

    /* Used to serialize the alloc and free operation on the POOL */
    ka_mutex_t          xp_block_mutex;
    ka_rb_root_t        block_root; /* for all alloced block */
    void                *private;
    ka_proc_dir_entry_t *entry;

    unsigned long alloc_size;
    unsigned long real_alloc_size;
    unsigned long alloc_peak_size;
    unsigned long pool_size;
    int           create_pid;
    int           adding_id;
    unsigned int  cache_type;
    ka_wait_queue_head_t adding_task_wq;
    ka_list_head_t    adding_task_list_head;
    u64               adding_task_num;
    ka_list_head_t    prop_list_head;
    unsigned long          mnt_ns;
    unsigned int           key_len;
    /* MUST be the last element */
    char                   key[];
};

struct xsm_task {
    ka_pid_t           pid;
    ka_pid_t           vpid; /* pid in container */
    int             attached_pool_count;
    u64             uid;    /* uniqueue id for each process */
    ka_atomic_t        pool_num;
    ka_hlist_node_t   link; /* hash task link */
    ka_list_head_t    node_list_head;
    ka_list_head_t    register_xp_list_head;
    ka_mutex_t        mutex;
    ka_mm_struct_t *mm;
    ka_proc_dir_entry_t *entry;
};

struct xsm_exit_task {
    ka_pid_t           pid;
    u64             uid;
    ka_list_head_t    node;
};

struct xsm_task_pool_node {
    ka_list_head_t    task_node; /* list node in TASK */
    ka_list_head_t    pool_node; /* list node in POOL */
    struct xsm_task *task;
    struct xsm_pool *pool;

    ka_mutex_t        mutex;
    ka_list_head_t    exit_task_head;

    GroupShareAttr attr;
    int task_id;

    u64     alloc_size;
    u64     real_alloc_size;
    u64     alloc_peak_size;
    ka_list_head_t    task_blk_head;
    ka_list_head_t    task_prop_head;
};

struct xsm_block {
    long refcnt;
    int pid;
    int id;
    unsigned long       offset;
    unsigned long       flag;
    unsigned long       alloc_size;
    unsigned long       real_size;

    ka_rb_node_t      block_rb_node;
    void            *private;
    ka_list_head_t    task_blk_head;
};

struct xsm_task_block_node {
    int refcnt;
    ka_list_head_t    node_list; /* list node in TASK node */
    ka_list_head_t    blk_list;  /* list node in BLOCK */
    struct xsm_block    *blk;
    struct xsm_task_pool_node *node;
};

#define ALGO_NAME_MAX 20
struct xsm_pool_algo {
    int num;
    char name[ALGO_NAME_MAX];
    ka_list_head_t algo_node;
    int (*xsm_pool_init)(struct xsm_pool *xp, struct xsm_reg_arg *arg);
    int (*xsm_pool_free)(struct xsm_pool *xp);
    int (*xsm_pool_cache_create)(struct xsm_pool *xp, struct xsm_cache_create_arg *arg);
    int (*xsm_pool_cache_destroy)(struct xsm_pool *xp, struct xsm_cache_destroy_arg *arg);
    int (*xsm_pool_cache_query)(struct xsm_pool *xp, unsigned int devId,
        GrpQueryGroupAddrInfo *cache_buff, unsigned int *cache_cnt);
    int (*xsm_pool_perm_add)(struct xsm_pool *xp, int pid, unsigned long prop);
    int (*xsm_pool_va_check)(struct xsm_pool *xp, unsigned long va, int *result);
    void (*xsm_pool_show)(struct xsm_pool *xp, ka_seq_file_t *seq);
    int (*xsm_block_alloc)(struct xsm_pool *xp, struct xsm_block *blk);
    int (*xsm_block_free)(struct xsm_pool *xp, struct xsm_block *blk);
};

void xsmem_register_algo(struct xsm_pool_algo *algo);

struct xsm_pool *xsmem_pool_get(int pool_id);
void xsmem_pool_put(struct xsm_pool *xp);

struct xsm_task_pool_node *task_pool_node_find(struct xsm_task *task, const struct xsm_pool *xp);

int copy_from_user_safe(void *to, const void __ka_user *from, unsigned long n);
bool blk_is_alloced_from_os(struct xsm_block *blk);

#endif

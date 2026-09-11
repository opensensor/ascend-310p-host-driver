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

#define pr_fmt(fmt) "XSMEM: <%s:%d> " fmt, __func__, __LINE__

#include "securec.h"
#include "ascend_kernel_hal.h"

#ifdef CFG_FEATURE_EXTERNAL_CDEV
#include "davinci_api.h"
#endif

#include "xsmem_algo_vma.h"
#include "xsmem_algo_cache_vma.h"

#ifdef CFG_FEATURE_SUPPORT_SP
#include "xsmem_algo_sp.h"
#include "xsmem_algo_cache_sp.h"
#endif

#include "xsmem_prop.h"
#include "xsmem_proc_fs.h"
#include "xsmem_res_dispatch.h"
#include "xsmem_ns_adapt.h"
#include "xsmem_framework_log.h"
#include "xsmem_framework.h"
#include "ka_list_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_errno_pub.h"
#include "ka_memory_pub.h"
#include "ka_define.h"
#include "ka_ioctl_pub.h"
#include "ka_common_pub.h"
#include "ka_driver_pub.h"
#include "ka_fs_pub.h"
#include "ka_dfx_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_barrier_pub.h"

#define XSHM_HLIST_TABLE_BIT 13
#define XSHM_HLIST_TABLE_MASK ((0x1 << XSHM_HLIST_TABLE_BIT) - 1)
#define TASK_HASH_TABLE_BIT 10
#define TASK_HASH_TABLE_MASK ((0x1UL << TASK_HASH_TABLE_BIT) - 1)

#define MAX_XP_NUM_OF_TASK  32
#define XSMEM_MAX_ADDING_TASK_NUM  32ULL
#define XSHM_KEY_AND_NAMESPACE_SIZE (XSHM_KEY_SIZE + 64)

/* Protect the lifetime for all POOL */
KA_TASK_DEFINE_MUTEX(xsmem_mutex);
KA_TASK_DEFINE_MUTEX(task_mutex);

static KA_DEFINE_HASHTABLE(xsmem_key_list, XSHM_HLIST_TABLE_BIT);
static KA_DEFINE_HASHTABLE(task_table, TASK_HASH_TABLE_BIT);

static u64 g_uni_process_id = 0;   // uniqueue id for each process

/* FIXME: use lock to protect the list_head */
static KA_LIST_HEAD(registered_algo);

#ifdef CFG_FEATURE_SUPPORT_SP
extern void xsmem_recycle_callback(void);
#endif

/* Check user id */
unsigned int THREAD g_xsmem_authorized_user_id = XSMEM_NOT_CONFIRM_USER_ID;
ka_module_param(g_xsmem_authorized_user_id, int, 0644);
KA_MODULE_PARM_DESC(g_xsmem_authorized_user_id, "User ID of system user \"HwHiAiUser\"");
#ifdef EMU_ST
void xsmem_set_authorized_user_id(unsigned int value)
{
    g_xsmem_authorized_user_id = value;
}
#endif
int copy_from_user_safe(void *to, const void __ka_user *from, unsigned long n)
{
    if ((from == NULL) || (n == 0)) {
        xsmem_err("The variable from is NULL or n is zero.\n");
        return -EINVAL;
    }

    if (ka_base_copy_from_user(to, (void *)from, n) != 0) {
        xsmem_err("Failed to invoke the copy_from_user. (size=%lu)\n", n);
        return -EFAULT;
    }

    return 0;
}

static int copy_to_user_safe(void __ka_user *to, const void *from, unsigned long n)
{
    if ((to == NULL) || (n == 0)) {
        xsmem_err("The variable to is NULL or n is zero.\n");
        return -EINVAL;
    }

    if (ka_base_copy_to_user(to, (void *)from, n) != 0) {
        xsmem_err("Failed to invoke the copy_from_user. (size=%lu)\n", n);
        return -EFAULT;
    }

    return 0;
}

void xsmem_register_algo(struct xsm_pool_algo *algo)
{
    struct xsm_pool_algo *tmp = NULL;

    ka_list_for_each_entry(tmp, &registered_algo, algo_node)
        if (algo->num == tmp->num) {
            xsmem_info("Algorithm with the same id, has been registered. (id=%d; name=%s)\n",
                tmp->num, tmp->name);
            return;
        }

    ka_list_add_tail(&algo->algo_node, &registered_algo);

    xsmem_debug("Algo registered success. (num=%d; name=%s)\n", algo->num, algo->name);

    return;
}

static struct xsm_pool_algo *xsmem_find_algo(int algo)
{
    struct xsm_pool_algo *tmp = NULL;

    ka_list_for_each_entry(tmp, &registered_algo, algo_node)
        if (tmp->num == algo) {
            return tmp;
        }

    return NULL;
}

/* The caller must hold xp->xp_block_mutex */
static struct xsm_block *xsmem_find_block(struct xsm_pool *xp, unsigned long offset)
{
    ka_rb_node_t *block_rb_node = ka_base_get_rb_root_node(&xp->block_root);

    while (block_rb_node) {
        struct xsm_block *blk = ka_base_rb_entry(block_rb_node, struct xsm_block, block_rb_node);

        if (offset >= (blk->offset + blk->alloc_size)) {
            block_rb_node = ka_base_get_rb_node_right(block_rb_node);
        } else if (offset < blk->offset) {
            block_rb_node = ka_base_get_rb_node_left(block_rb_node);
        } else {
            return blk;
        }
    }

    return NULL;
}

// link: new node
// parent: parent
static void xsmem_add_block(struct xsm_pool *xp, struct xsm_block *blk)
{
    ka_rb_node_t *parent = NULL;
    ka_rb_node_t **link = ka_base_get_rb_root_node_addr(&xp->block_root);
    while (*link) {
        struct xsm_block *tmp = NULL;

        parent = *link;
        tmp = ka_base_rb_entry(*link, struct xsm_block, block_rb_node);

        if (blk->offset > tmp->offset) {
            link = ka_base_get_rb_node_right_addr(parent);
        } else if (blk->offset < tmp->offset) {
            link = ka_base_get_rb_node_left_addr(parent);
        } else {
            xsmem_warn("pool id %d alloc one block offset %pK more than once, the POOL's algo got broken\n",
                xp->pool_id, (void *)(uintptr_t)blk->offset);
            return;
        }
    }

    ka_base_rb_link_node(&blk->block_rb_node, parent, link);
    ka_base_rb_insert_color(&blk->block_rb_node, &xp->block_root);
}

static void xsmem_block_destroy(struct xsm_pool *xp, struct xsm_block *blk)
{
    xp->algo->xsm_block_free(xp, blk); // should not fail
    ka_base_rb_erase(&blk->block_rb_node, &xp->block_root);
    xsmem_drv_kfree(blk);
}

static void xsmem_clear_block(struct xsm_pool *xp)
{
    ka_rb_node_t *node = NULL;
    struct xsm_block *blk = NULL;
    int num = 0;

    ka_task_mutex_lock(&xp->xp_block_mutex);
    node = ka_base_rb_first(&xp->block_root);
    while (node != NULL) {
        blk = ka_base_rb_entry(node, struct xsm_block, block_rb_node);
        xsmem_blockid_put(xp->mnt_ns, blk->id);
        node = ka_base_rb_next(node);
        xsmem_block_destroy(xp, blk);
        num++;
    }
    ka_task_mutex_unlock(&xp->xp_block_mutex);

    if (num > 0) {
        xsmem_info("pool %s id %d recycle blk num %d\n", xp->key, xp->pool_id, num);
    }
}

static struct xsm_task_block_node *xsmem_find_task_block_node(const struct xsm_task_pool_node *node,
    struct xsm_block *blk)
{
    struct xsm_task_block_node *task_blk_node = NULL;

    ka_list_for_each_entry(task_blk_node, &blk->task_blk_head, blk_list) {
        if (task_blk_node->node == node) {
            return task_blk_node;
        }
    }

    return NULL;
}

static void xsmem_task_link_blk(struct xsm_task_pool_node *node, struct xsm_block *blk,
    struct xsm_task_block_node *task_blk_node)
{
    task_blk_node->blk = blk;
    task_blk_node->node = node;
    ka_list_add_tail(&task_blk_node->node_list, &node->task_blk_head);
    ka_list_add_tail(&task_blk_node->blk_list, &blk->task_blk_head);
}

static void xsmem_task_unlink_blk(struct xsm_task_block_node *task_blk_node)
{
    ka_list_del(&task_blk_node->node_list);
    ka_list_del(&task_blk_node->blk_list);
}

static void node_stat_add(struct xsm_task_pool_node *node, struct xsm_block *blk)
{
    node->alloc_size += blk->alloc_size;
    node->real_alloc_size += blk->real_size;
    node->alloc_peak_size = (node->real_alloc_size > node->alloc_peak_size) ?
        node->real_alloc_size : node->alloc_peak_size;
}

static void node_stat_sub(struct xsm_task_pool_node *node, struct xsm_block *blk)
{
    node->alloc_size -= blk->alloc_size;
    node->real_alloc_size -= blk->real_size;
}

static void xsmem_pool_stat_add(struct xsm_pool *xp, struct xsm_block *blk)
{
    xp->alloc_size += blk->alloc_size;
    xp->real_alloc_size += blk->real_size;
    xp->alloc_peak_size = (xp->real_alloc_size > xp->alloc_peak_size) ?
        xp->real_alloc_size : xp->alloc_peak_size;
}

static void xsmem_pool_stat_sub(struct xsm_pool *xp, struct xsm_block *blk)
{
    xp->alloc_size -= blk->alloc_size;
    xp->real_alloc_size -= blk->real_size;
}

static bool xsmem_is_blk_need_auto_recycle(const struct xsm_block *blk)
{
    return (blk->flag & XSMEM_BLK_NOT_AUTO_RECYCLE) != XSMEM_BLK_NOT_AUTO_RECYCLE;
}

static void xsmem_task_clear_block(struct xsm_pool *xp, struct xsm_task_pool_node *node)
{
    struct xsm_task_block_node *task_blk_node = NULL, *tmp = NULL;

    ka_task_mutex_lock(&xp->xp_block_mutex);
    ka_list_for_each_entry_safe(task_blk_node, tmp, &node->task_blk_head, node_list) {
        struct xsm_block *blk = task_blk_node->blk;

        xsmem_debug("pool %d task %d exit offset %pK alloced by task %d ref %d blk ref %ld flag %lx\n", xp->pool_id,
            node->task->pid, (void *)(uintptr_t)blk->offset, blk->pid, task_blk_node->refcnt, blk->refcnt, blk->flag);

        xsmem_task_unlink_blk(task_blk_node);
        blk->refcnt -= task_blk_node->refcnt;
        if ((blk->refcnt <= 0) && xsmem_is_blk_need_auto_recycle(blk)) {
            xsmem_pool_stat_sub(xp, blk);
            xsmem_blockid_put(xp->mnt_ns, blk->id);
            xsmem_block_destroy(xp, blk);
        }

        xsmem_drv_kfree(task_blk_node);
    }
    ka_task_mutex_unlock(&xp->xp_block_mutex);
}

static struct xsm_pool *xsmem_pool_hnode_find(const char *name, unsigned int name_len)
{
    struct xsm_pool *xp = NULL;
    unsigned int key = ka_base_jhash(name, name_len, 0) & XSHM_HLIST_TABLE_MASK;

    ka_hash_for_each_possible(xsmem_key_list, xp, hnode, key)
        if ((name_len == xp->key_len) && (ka_base_strncmp(name, xp->key, xp->key_len) == 0)) {
            return xp;
        }

    return NULL;
}

static void xsm_pool_hnode_add(struct xsm_pool *xp)
{
    unsigned int key = ka_base_jhash(xp->key, xp->key_len, 0) & XSHM_HLIST_TABLE_MASK;

    ka_hash_add(xsmem_key_list, &xp->hnode, key);
}

static void xsm_pool_hnode_del(struct xsm_pool *xp)
{
    ka_hash_del(&xp->hnode);
}

STATIC struct xsm_task *xsm_task_find(int pid)
{
    struct xsm_task *task = NULL;
    unsigned int key = (u32)pid & TASK_HASH_TABLE_MASK;

    ka_hash_for_each_possible(task_table, task, link, key)
        if (task->pid == pid) {
            return task;
        }

    return NULL;
}

static void xsm_task_add(struct xsm_task *task)
{
    int key = (int)((u32)task->pid & TASK_HASH_TABLE_MASK);

    ka_hash_add(task_table, &task->link, (u32)key);
}

static void xsm_task_del(struct xsm_task *task)
{
    ka_hash_del(&task->link);
}

static struct xsm_task_pool_node *xsmem_pool_task_node_find(struct xsm_pool *xp, int pid)
{
    struct xsm_task_pool_node *node = NULL;

    ka_list_for_each_entry(node, &xp->node_list_head, pool_node) {
        if (node->task->pid == pid) {
            return node;
        }
    }

    return NULL;
}

struct xsm_task_pool_node *task_pool_node_find(struct xsm_task *task, const struct xsm_pool *xp)
{
    struct xsm_task_pool_node *node = NULL;

    ka_list_for_each_entry(node, &task->node_list_head, task_node)
        if (node->pool == xp) {
            return node;
        }

    return NULL;
}

static struct xsm_task_pool_node *task_pool_node_add(struct xsm_task *task, struct xsm_pool *xp, GroupShareAttr attr)
{
    struct xsm_task_pool_node *node = NULL;

    node = task_pool_node_find(task, xp);
    if (node != NULL) {
        xsmem_err("The pool has already attach to task. (pool=%d, task=%d)\n", xp->pool_id, task->pid);
        return KA_ERR_PTR(-ESRCH);
    }

    node = xsmem_drv_kmalloc(sizeof(*node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(node == NULL)) {
        xsmem_err("alloc xsm_task_pool_node failed\n");
        return KA_ERR_PTR(-ENOMEM);
    }

    node->attr = attr;
    node->task = task;
    node->pool = xp;

    KA_INIT_LIST_HEAD(&node->exit_task_head);
    KA_INIT_LIST_HEAD(&node->task_blk_head);
    KA_INIT_LIST_HEAD(&node->task_prop_head);
    node->real_alloc_size = 0;
    node->alloc_size = 0;
    node->alloc_peak_size = 0;

    ka_list_add_tail(&node->task_node, &task->node_list_head);
    ka_task_mutex_init(&node->mutex);

    /* Do not add node to xp->list_head until all its elements initialized */
    ka_task_mutex_lock(&xp->mutex);
    ka_list_add_tail(&node->pool_node, &xp->node_list_head);
    node->task_id = xp->task_id;
    xp->task_id = ((xp->task_id + 1) < 0) ? 0 : xp->task_id + 1;
    ka_task_mutex_unlock(&xp->mutex);

    proc_fs_xsmem_pool_add_task(node);

    if (node->attr.alloc == 1) {
        xsmem_debug("pool %d task %d attr has alloc\n", xp->pool_id, node->task->pid);
    } else {
        xsmem_debug("pool %d task %d attr has no alloc\n", xp->pool_id, node->task->pid);
    }

    xsmem_debug("pool_id %d, node %pK, pid %d\n", xp->pool_id, (void *)(uintptr_t)node, node->task->pid);

    return node;
}

static void task_pool_node_del(struct xsm_task_pool_node *node)
{
    struct xsm_pool *xp = node->pool;
    struct xsm_exit_task *exit_task = NULL, *tmp = NULL;

    proc_fs_xsmem_pool_del_task(node);

    ka_task_mutex_lock(&xp->mutex);
    ka_list_del(&node->pool_node);
    ka_task_mutex_unlock(&xp->mutex);

    ka_task_mutex_lock(&node->mutex);
    ka_list_for_each_entry_safe(exit_task, tmp, &node->exit_task_head, node) {
        xsmem_debug("task %d id %d exit uid %llu\n", node->task->pid, node->task_id, exit_task->uid);
        ka_list_del(&exit_task->node);
        xsmem_drv_kfree(exit_task);
    }
    ka_task_mutex_unlock(&node->mutex);

    ka_list_del(&node->task_node);
    xsmem_drv_kfree(node);
}

static bool xsmem_is_task_has_admin(struct xsm_task *task, struct xsm_pool *xp)
{
    struct xsm_task_pool_node *node = NULL;

    node = task_pool_node_find(task, xp);
    if (node == NULL) {
        if (xp->create_pid == task->pid) {
            return true;
        }
    } else {
        if (node->attr.admin == 1) {
            return true;
        }
        xsmem_debug("admin %u, alloc %u, write %u, read %u", node->attr.admin,
            node->attr.alloc, node->attr.write, node->attr.read);
    }

    return false;
}

static void xsmem_pool_notice_other_task(struct xsm_pool *xp, struct xsm_task_pool_node *cur_node)
{
    struct xsm_task_pool_node *node = NULL;
    struct xsm_exit_task *exit_task = NULL;

    ka_task_mutex_lock(&xp->mutex);

    ka_list_for_each_entry(node, &xp->node_list_head, pool_node) {
        if (node == cur_node) {
            continue;
        }

        exit_task = xsmem_drv_kmalloc(sizeof(struct xsm_exit_task), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (exit_task == NULL) {
            ka_task_mutex_unlock(&xp->mutex);
            xsmem_warn("alloc mem %lx failed\n", sizeof(struct xsm_exit_task));
            return;
        }

        exit_task->pid = cur_node->task->pid;
        exit_task->uid = cur_node->task->uid;
        ka_task_mutex_lock(&node->mutex);
        ka_list_add_tail(&exit_task->node, &node->exit_task_head);
        ka_task_mutex_unlock(&node->mutex);
        xsmem_debug("pool %d task proc %d uid %llu exit, notice task %d\n",
            xp->pool_id, cur_node->task->pid, cur_node->task->uid, node->task->pid);
    }

    ka_task_mutex_unlock(&xp->mutex);
}

STATIC int xsm_get_task_start_time(int tgid, u64 *start_time)
{
#ifndef EMU_ST
    ka_struct_pid_t *pid_struct;
    ka_task_struct_t *task = NULL;

    ka_task_rcu_read_lock();
    pid_struct = ka_task_get_pid(ka_task_find_pid_ns(tgid, ka_task_get_init_pid_ns_addr()));
    ka_task_rcu_read_unlock();
    if (pid_struct == NULL) {
        return -EINVAL;
    }

    task = ka_task_get_pid_task(pid_struct, KA_PIDTYPE_PID);
    ka_task_put_pid(pid_struct);
    if (task == NULL) {
        return -ESRCH;
    }

    *start_time = ka_task_get_starttime(task);

    ka_task_put_task_struct(task);
#else
    *start_time = 0;
#endif
    return 0;
}

static struct xsm_adding_task *xsmem_pool_adding_task_find(struct xsm_pool *xp, int pid)
{
    struct xsm_adding_task *adding_task = NULL;
    u64 start_time;

    if (xsm_get_task_start_time(pid, &start_time) != 0) {
        return NULL;
    }

    ka_list_for_each_entry(adding_task, &xp->adding_task_list_head, pool_node)
        if ((adding_task->pid == pid) && (adding_task->start_time == start_time)) {
            return adding_task;
        }

    return NULL;
}

static int xsmem_pool_adding_task_add(struct xsm_pool *xp, int pid, GroupShareAttr attr)
{
    struct xsm_adding_task *adding_task = NULL;
    u64 start_time;

    if (xsm_get_task_start_time(pid, &start_time) != 0) {
        xsmem_err("adding task is not exist. (pid=%d)\n", pid);
        return -ESRCH;
    }

    adding_task = xsmem_drv_kmalloc(sizeof(*adding_task), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(adding_task == NULL)) {
        xsmem_err("alloc adding_task node failed\n");
        return -ENOMEM;
    }

    adding_task->pid = pid;
    adding_task->attr = attr;
    adding_task->start_time = start_time;
    ka_list_add_tail(&adding_task->pool_node, &xp->adding_task_list_head);
    xp->adding_task_num++;

    return 0;
}

static void xsmem_pool_adding_task_del(struct xsm_pool *xp, struct xsm_adding_task *adding_task)
{
    ka_list_del(&adding_task->pool_node);
    xsmem_drv_kfree(adding_task);
    xp->adding_task_num--;
}

static void xsmem_pool_adding_task_clear(struct xsm_pool *xp)
{
    struct xsm_adding_task *adding_task = NULL, *tmp = NULL;

    ka_list_for_each_entry_safe(adding_task, tmp, &xp->adding_task_list_head, pool_node) {
        xsmem_pool_adding_task_del(xp, adding_task);
    }
}

static void xsmem_exit_adding_task_find_and_del(struct xsm_pool *xp)
{
    struct xsm_adding_task *adding_task = NULL, *tmp = NULL;
    u64 start_time;

    // adding task num exceeding the threshold is most likely caused by the resources of exited processes is unreleased
    if (xp->adding_task_num <= XSMEM_MAX_ADDING_TASK_NUM) {
        return;
    }

    ka_list_for_each_entry_safe(adding_task, tmp, &xp->adding_task_list_head, pool_node) {
        if ((xsm_get_task_start_time(adding_task->pid, &start_time) != 0) || (adding_task->start_time != start_time)) {
            xsmem_pool_adding_task_del(xp, adding_task);
        }
    }
    return;
}

static int xsmem_pool_task_readd(const struct xsm_pool *xp, int pid, GroupShareAttr new_attr, GroupShareAttr cur_attr)
{
    if (*(unsigned int *)&new_attr == *(unsigned int *)&cur_attr) {
        return 0;
    } else {
        xsmem_warn("Don't add pid to pool repeatedly. (pool=%d, pid=%d)\n", xp->pool_id, pid);
        return -EEXIST;
    }
}

static int xsmem_pool_task_add(struct xsm_pool *xp, int pid, GroupShareAttr attr)
{
    int ret;
    struct xsm_task_pool_node *node = NULL;
    struct xsm_adding_task *adding_task = NULL;

    ka_task_mutex_lock(&xp->mutex);
    xsmem_exit_adding_task_find_and_del(xp);
    adding_task = xsmem_pool_adding_task_find(xp, pid);
    if (adding_task != NULL) {
        ret = xsmem_pool_task_readd(xp, pid, attr, adding_task->attr);
        goto out;
    }

    node = xsmem_pool_task_node_find(xp, pid);
    if (node != NULL) {
        ret = xsmem_pool_task_readd(xp, pid, attr, node->attr);
        goto out;
    }

    ret = xsmem_pool_adding_task_add(xp, pid, attr);
    if (ret != 0) {
        goto out;
    }

    xp->adding_id++;
    ka_task_wake_up_interruptible(&xp->adding_task_wq);

    xsmem_run_info("Xsmem pool task add. (comm=%s, tgid=%d, pid=%d, pool_id=%d, process=%d)\n",
        XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, xp->pool_id, pid);
out:
    ka_task_mutex_unlock(&xp->mutex);

    return ret;
}

static int xsmem_pool_task_del(struct xsm_pool *xp, int pid)
{
    struct xsm_adding_task *adding_task = NULL;

    ka_task_mutex_lock(&xp->mutex);

    adding_task = xsmem_pool_adding_task_find(xp, pid);
    if (adding_task == NULL) {
        ka_task_mutex_unlock(&xp->mutex);
        xsmem_err("pool_id %d has no task %d\n", xp->pool_id, pid);
        return -EINVAL;
    }

    xsmem_pool_adding_task_del(xp, adding_task);
    ka_task_mutex_unlock(&xp->mutex);

    xsmem_info("<%s:%d,%d> xsmem_pool_task_del, pool_id:%d, pid:%d\n",
        XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, xp->pool_id, pid);

    return 0;
}

static int xsmem_pool_task_attr_get(const struct xsm_task *task, struct xsm_pool *xp, int timeout,
    GroupShareAttr *attr)
{
    struct xsm_adding_task *adding_task = NULL;
    int ret, pid = XSMEM_CURRENT_TGID, adding_id = xp->adding_id;
    unsigned long begin = ka_jiffies, escaped, jf_timeout;
    unsigned long prop;
    long ret_wait_event;

    while (1) {
        ka_task_mutex_lock(&xp->mutex);
        adding_task = xsmem_pool_adding_task_find(xp, pid);
        if (adding_task != NULL) {
            *attr = adding_task->attr;

            prop = ((attr->alloc != 0) || (attr->write != 0)) ? (KA_PROT_WRITE | KA_PROT_READ) : KA_PROT_READ;
            if (xp->algo->xsm_pool_perm_add != NULL) {
                ret = xp->algo->xsm_pool_perm_add(xp, adding_task->pid, prop);
            } else {
                ret = 0;
            }

            xsmem_debug("Find adding task. "
                "(task_id=%d; pool_id=%d; timeout=%dms; admin=%u; alloc=%u; write=%u; adding_task=%d)\n",
                task->pid, xp->pool_id, timeout, attr->admin, attr->alloc, attr->write, adding_task->pid);

            xsmem_pool_adding_task_del(xp, adding_task);
            ka_task_mutex_unlock(&xp->mutex);
            return ret;
        }
        ka_task_mutex_unlock(&xp->mutex);

        if (timeout == 0) {
            xsmem_err("the pool %d get task %d attr failed no wait\n", xp->pool_id, task->pid);
            return -EINVAL;
        }

        escaped = ka_jiffies - begin;
        jf_timeout = ka_system_msecs_to_jiffies((unsigned int)timeout);
        jf_timeout = (escaped >= jf_timeout) ? 1 : jf_timeout - escaped;

        xsmem_debug("<%s:%d,%d> the pool %d task %d begin wait add event adding_id %d\n",
            XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, xp->pool_id, task->pid, adding_id);

        ret_wait_event =
            ka_task_wait_event_interruptible_timeout(xp->adding_task_wq, (adding_id != xp->adding_id), (long)jf_timeout);
        if (ret_wait_event <= 0) {
            if (ret_wait_event == 0) {
                ret = -EAGAIN;
            } else {
                ret = (int)ret_wait_event;
            }
            break;
        }
        adding_id = xp->adding_id;
        xsmem_debug("the pool %d task %d retry adding_id %d\n", xp->pool_id, task->pid, adding_id);
    };

    return ret;
}

static unsigned int xsmem_pool_get_cache_type(struct xsm_pool_algo *algo)
{
    if (algo->xsm_pool_cache_create != NULL) {
        return BUFF_CACHE;
    }
    return BUFF_NOCACHE;
}

/*
 * The caller must hold xsmem_mutex and this function would unlock xsmem_mutex on failure.
 */
static struct xsm_pool *xsmem_pool_create(struct xsm_pool_algo *algo, struct xsm_reg_arg *arg)
{
    unsigned long ns = (unsigned long)(uintptr_t)ka_task_get_mnt_ns(ka_task_get_current());
    unsigned int key_len = arg->key_len;
    struct xsm_pool *xp = NULL;
    char *key = arg->key;
    int id, ret;

    ret = xsmem_ns_pool_num_inc(ns, algo->num);
    if (ret != 0) {
        xsmem_err("Namespace pool num inc failed. (ns=%lu; ret=%d)\n", ns, ret);
        return KA_ERR_PTR(ret);
    }

    xp = xsmem_drv_kzalloc(sizeof(*xp) + key_len + 1, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(xp == NULL)) {
        xsmem_ns_pool_num_dec(ns, algo->num);
        xsmem_err("alloc pool memory failed\n");
        return KA_ERR_PTR(-ENOMEM);
    }

    xp->algo = algo;
    ret = algo->xsm_pool_init(xp, arg);
    if (ret < 0) {
        xsmem_drv_kfree(xp);
        xsmem_ns_pool_num_dec(ns, algo->num);
        xsmem_err("init hook failed\n");
        return ERR_PTR(ret);
    }

    xp->create_pid = XSMEM_CURRENT_TGID;
    xp->priv_mbuf_flag = arg->priv_flag;
    xp->mnt_ns = ns;
    xp->cache_type = xsmem_pool_get_cache_type(algo);
    ka_task_mutex_init(&xp->mutex);
    ka_base_atomic_set(&xp->refcnt, 0);
    KA_INIT_LIST_HEAD(&xp->node_list_head);
    KA_INIT_LIST_HEAD(&xp->adding_task_list_head);
    ka_task_init_waitqueue_head(&xp->adding_task_wq);
    KA_INIT_LIST_HEAD(&xp->prop_list_head);

    ka_task_mutex_init(&xp->xp_block_mutex);

    xp->block_root = KA_RB_ROOT;
    xp->key_len = key_len;
    ret = strncpy_s(xp->key, key_len + 1, key, key_len);
    if (ret != 0) {
        xsmem_warn("pool name strncpy_s not success\n");
    }
    xp->key[key_len] = '\0';

    ka_wmb();
    ret = xsmem_alloc_id(xp, &id);
    if (ret != 0) {
        algo->xsm_pool_free(xp);
        xsmem_drv_kfree(xp);
        xsmem_ns_pool_num_dec(ns, algo->num);
        return KA_ERR_PTR(ret);
    }
    xp->pool_id = id;

    xsm_pool_hnode_add(xp);

    proc_fs_add_xsmem_pool(xp);

    xsmem_info("create pool id %d\n", id);

    return xp;
}

static void xsmem_pool_delete(struct xsm_pool *xp)
{
    xsmem_info("delete pool name %s pool id %d\n", xp->key, xp->pool_id);

    proc_fs_del_xsmem_pool(xp);

    xsmem_clear_block(xp);
    xsmem_pool_adding_task_clear(xp);
    xsmem_pool_prop_clear(xp);
    xsm_pool_hnode_del(xp);

    xsmem_delete_id(xp->pool_id);

    xp->algo->xsm_pool_free(xp);
    xsmem_ns_pool_num_dec(xp->mnt_ns, xp->algo->num);

    xsmem_drv_kfree(xp);
}

struct xsm_pool *xsmem_pool_get(int pool_id)
{
    struct xsm_pool *xp = NULL;

    ka_task_mutex_lock(&xsmem_mutex);
    xp = xsmem_get_xp_by_id(pool_id);
    if (xp) {
        ka_base_atomic_inc(&xp->refcnt);
    }
    ka_task_mutex_unlock(&xsmem_mutex);

    return xp;
}

void xsmem_pool_put(struct xsm_pool *xp)
{
    ka_task_mutex_lock(&xsmem_mutex);
    if (!ka_base_atomic_dec_and_test(&xp->refcnt)) {
        ka_task_mutex_unlock(&xsmem_mutex);
        return;
    }

    xsmem_pool_delete(xp);
    ka_task_mutex_unlock(&xsmem_mutex);

    return;
}

/*
 * get the POOL specified by key, create one if it not exist.
 */
static struct xsm_pool *xsmem_pool_register(struct xsm_pool_algo *algo, struct xsm_reg_arg *arg)
{
    struct xsm_pool *xp = NULL;

    ka_task_mutex_lock(&xsmem_mutex);
    xp = xsmem_pool_hnode_find((const char *)arg->key, arg->key_len);
    if (xp != NULL) {
        int create_pid = xp->create_pid;
        ka_task_mutex_unlock(&xsmem_mutex);
        xsmem_err("pool name %s is exist create_pid %d\n", arg->key, create_pid);
        return KA_ERR_PTR(-EEXIST);
    }

    xp = xsmem_pool_create(algo, arg);
    if (KA_IS_ERR_OR_NULL(xp)) {
        ka_task_mutex_unlock(&xsmem_mutex);
        xsmem_err("pool name %s create failed\n", arg->key);
        return xp;
    }

    /*
     * Here increase the POOL's refcnt by one, one for the convenience of fallback in
     * error branch in ioctl_xsmem_pool_register(), another to promise that the POOL
     * cannot be delete in the later attach routine.
     */
    ka_base_atomic_inc(&xp->refcnt);
    ka_task_mutex_unlock(&xsmem_mutex);

    return xp;
}

static void xsmem_pool_unregister(struct xsm_pool *xp)
{
    xsmem_info("pool_id %d unregister, refcnt %d\n", xp->pool_id, ka_base_atomic_read(&xp->refcnt));
    xsmem_pool_put(xp);
}

/*
 * The refcnt of the xp should be increased by one on success.
 * We don't require the xsmem_mutex here since the xp->refcnt has been increased already.
 */
static int xsmem_pool_attach(struct xsm_task *task, struct xsm_pool *xp, int timeout)
{
    struct xsm_task_pool_node *node = NULL;
    GroupShareAttr attr;
    int ret;

    ka_task_mutex_lock(&task->mutex);
    node = task_pool_node_find(task, xp);
    ka_task_mutex_unlock(&task->mutex);
    if (node != NULL) {
        xsmem_err("the pool %d has already attach to task %d\n", xp->pool_id, task->pid);
        return -ESRCH;
    }

    ret = xsmem_pool_task_attr_get(task, xp, timeout, &attr);
    if (ret != 0) {
        xsmem_err("the pool %d add task %d get attr failed\n", xp->pool_id, task->pid);
        return ret;
    }

    if ((xp->create_pid != task->pid) && (ka_base_atomic_inc_return(&task->pool_num) > MAX_XP_NUM_OF_TASK)) {
        ka_base_atomic_dec(&task->pool_num);
        xsmem_err("Current task's pool_num has reached its limit. (task_pid=%d)\n", task->pid);
        return -ENOMEM;
    }

    ka_task_mutex_lock(&task->mutex);
    node = task_pool_node_add(task, xp, attr);
    if (KA_IS_ERR_OR_NULL(node)) {
        ka_task_mutex_unlock(&task->mutex);
        if (xp->create_pid != task->pid) {
            ka_base_atomic_dec(&task->pool_num);
        }
        xsmem_err("the pool %d add task %d node failed\n", xp->pool_id, task->pid);
        return (int)KA_PTR_ERR(node);
    }
    ka_base_atomic_inc(&xp->refcnt);
    task->attached_pool_count++;
    ka_task_mutex_unlock(&task->mutex);

    return 0;
}

static void xsmem_pool_task_detach(struct xsm_task_pool_node *node)
{
    struct xsm_pool *xp = node->pool;
    struct xsm_task *task = node->task;

    xsmem_proc_grp_prop_del(node);
    xsmem_pool_task_prop_clear(xp, task);
    xsmem_task_clear_block(xp, node);

    task->attached_pool_count--;
    task_pool_node_del(node);

    if (xp->create_pid != task->pid) {
        ka_base_atomic_dec(&task->pool_num);
    }

    xsmem_pool_put(xp);
}

static int xsmem_pool_detach(struct xsm_task *task, struct xsm_pool *xp)
{
    struct xsm_task_pool_node *node = NULL;

    node = task_pool_node_find(task, xp);
    if (node == NULL) {
        xsmem_err("the pool %d has not task %d\n", xp->pool_id, task->pid);
        return -ESRCH;
    }

    xsmem_pool_task_detach(node);

    return 0;
}

static int xsmem_check_pool_name(char *name, unsigned int name_len)
{
    char *next = NULL;

    if (ka_base_strnlen(name, XSHM_KEY_SIZE) != name_len) {
        return -EINVAL;
    }

    if ((name_len == 1) && (name[0] == '.')) {
        return -EINVAL;
    }
    if ((name_len == 2) && (name[0] == '.') && (name[1] == '.')) { /* 2: name can't be '..' */
        return -EINVAL;
    }

    /* name including '/' will cause proc_mkdir fail.
     * for example, name="aaa/bbb", aaa directory don't exist.
     */
    next = ka_base_strchr(name, '/');
    if (next != NULL) {
        return -EINVAL;
    }
    return 0;
}

static bool xsmem_confirm_user(void)
{
    const ka_cred_t *cred = ka_task_get_current_cred();

    /* userid not init, means don't need confirm */
    if (g_xsmem_authorized_user_id == XSMEM_NOT_CONFIRM_USER_ID) {
        return true;
    }

    if (cred != NULL && (ka_task_get_current_cred_uid() == g_xsmem_authorized_user_id ||
        ka_task_get_current_cred_uid() == XSMEM_ROOT_USER_ID)) {
        return true;
    }

    return false;
}

static int ioctl_xsmem_pool_register(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_pool *xp = NULL;
    unsigned int key_len;
    char key[XSHM_KEY_SIZE];
    char key_tmp[XSHM_KEY_AND_NAMESPACE_SIZE] = {0};
    struct xsm_reg_arg reg_arg;
    struct xsm_pool_algo *algo = NULL;
    int ret, pool_id;

    if (xsmem_confirm_user() == false) {
        xsmem_err("The user is not allowed to register pool.\n");
        return -EPERM;
    }

    if (copy_from_user_safe(&reg_arg, (struct xsm_reg_arg __ka_user *)arg, sizeof(reg_arg)) != 0) {
        xsmem_err("register: copy_from_user failed\n");
        return -EFAULT;
    }

    algo = xsmem_find_algo(reg_arg.algo);
    if (ka_unlikely(algo == NULL)) {
        xsmem_err("unsupported algorithm %d\n", reg_arg.algo);
        return -EINVAL;
    }

    key_len = reg_arg.key_len;
    if ((reg_arg.key == NULL) || (key_len == 0) || (key_len >= XSHM_KEY_SIZE)) {
        xsmem_err("Key or key_len is invalid. (key_len=%u)\n", key_len);
        return -EINVAL;
    }

    if (copy_from_user_safe(key, (char __ka_user *)reg_arg.key, key_len) != 0) {
        xsmem_err("copy key from user failed, key_len %d\n", key_len);
        return -EFAULT;
    }

    key[key_len] = '\0';
    ret = xsmem_check_pool_name(key, key_len);
    if (ret != 0) {
        xsmem_err("Pool name is invalid. (key_len=%u)\n", key_len);
        return ret;
    }

    if (ka_base_atomic_inc_return(&task->pool_num) > MAX_XP_NUM_OF_TASK) {
        ka_base_atomic_dec(&task->pool_num);
        xsmem_err("Current task's pool_num has reached its limit. (task_pid=%d)\n", task->pid);
        return -ENOMEM;
    }

    ret = xsmem_strcat_with_ns(key_tmp, XSHM_KEY_AND_NAMESPACE_SIZE, key);
    if (ret < 0) {
        ka_base_atomic_dec(&task->pool_num);
        xsmem_err("Sprintf_s error. (ret=%d)\n", ret);
        return -EINVAL;
    }

    reg_arg.key = key_tmp;
    reg_arg.key_len = (unsigned int)ka_base_strlen(key_tmp);
    xp = xsmem_pool_register(algo, &reg_arg);
    if (KA_IS_ERR_OR_NULL(xp)) {
        ka_base_atomic_dec(&task->pool_num);
        xsmem_err("register pool %s failed algo %d size %lx\n", key_tmp, reg_arg.algo, reg_arg.pool_size);
        return (int)KA_PTR_ERR(xp);
    }
    pool_id = xp->pool_id;
    ka_task_mutex_lock(&task->mutex);
    ka_list_add_tail(&xp->register_task_list, &task->register_xp_list_head);
    ka_task_mutex_unlock(&task->mutex);

    xsmem_info("Xsmem pool register success. (pool_name=%s; id=%d; algo=%d; size=%lx)\n",
        key_tmp, pool_id, reg_arg.algo, reg_arg.pool_size);

    return pool_id;
}

static void xsmem_task_unregister_pool(struct xsm_task *task, struct xsm_pool *xp)
{
    ka_base_atomic_dec(&task->pool_num);
    xsmem_pool_unregister(xp);
}

static int ioctl_xsmem_pool_unregister(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_task_pool_node *node = NULL;
    struct xsm_pool *xp = NULL, *xp_tmp = NULL;
    int pool_id = (int)arg;

    ka_task_mutex_lock(&task->mutex);
    ka_list_for_each_entry(xp_tmp, &task->register_xp_list_head, register_task_list) {
        if (xp_tmp->pool_id == pool_id) {
            xp = xp_tmp;
            break;
        }
    }

    if (xp == NULL) {
        ka_task_mutex_unlock(&task->mutex);
        xsmem_err("pool_id %d couldn't find the pool\n", pool_id);
        return -ENODEV;
    }

    node = task_pool_node_find(task, xp);
    if (node != NULL) {
        (void)xsmem_pool_detach(task, xp);
    }

    xsmem_info("Xsmem pool unregister success. (pool_name=%s; id=%d)\n", xp->key, xp->pool_id);

    ka_list_del(&xp->register_task_list);
    ka_task_mutex_unlock(&task->mutex);

    xsmem_task_unregister_pool(task, xp);

    return 0;
}

static int ioctl_xsmem_cache_create(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_cache_create_arg cache_arg;
    struct xsm_pool *xp = NULL;
    int ret;

    if (copy_from_user_safe(&cache_arg, (struct xsm_cache_create_arg __ka_user *)(uintptr_t)arg, sizeof(cache_arg)) != 0) {
        xsmem_err("Copy_from_user failed.\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(cache_arg.poolId);
    if (xp == NULL) {
        xsmem_err("Invalid pool id. (pool_id=%d)\n", cache_arg.poolId);
        return -EINVAL;
    }

    if (xp->create_pid != XSMEM_CURRENT_TGID) {
        xsmem_pool_put(xp);
        xsmem_err("Task has no cache alloc prop. (proc=%s; tgid=%d; pid=%d; pool_id=%d)\n",
            XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, cache_arg.poolId);
        return -EPERM;
    }

    if (xp->algo->xsm_pool_cache_create == NULL) {
        xsmem_err("Not support cache. (name=%s)\n", xp->algo->name);
        xsmem_pool_put(xp);
        return -EOPNOTSUPP;
    }

    ret = xp->algo->xsm_pool_cache_create(xp, &cache_arg);
    if (ret != 0) {
        xsmem_pool_put(xp);
        xsmem_err("Xsm_pool_cache_create fail. (pool_id=%d)\n", cache_arg.poolId);
        return ret;
    }
    xp->pool_size = cache_arg.memSize; /* record in cache scene */

    xsmem_pool_put(xp);
    return 0;
}

static unsigned int _xsmem_show_alloced_mem_info(struct xsm_block *blk,
    unsigned int max_show_cnt)
{
    struct xsm_task_block_node *task_blk_node = NULL;
    unsigned int show_alloced_mem_cnt = 0;

    ka_list_for_each_entry(task_blk_node, &blk->task_blk_head, blk_list) {
        if (show_alloced_mem_cnt < max_show_cnt) {
            show_alloced_mem_cnt++;
            xsmem_warn("va=0x%pK; flag=%lx; pid=%d, ref=%d\n",
                (void *)(uintptr_t)blk->offset, blk->flag, task_blk_node->node->task->pid, task_blk_node->refcnt);
        } else {
            break;
        }
    }

    return show_alloced_mem_cnt;
}

bool blk_is_alloced_from_os(struct xsm_block *blk)
{
    return ((blk->flag & XSMEM_BLK_ALLOC_FROM_OS) == XSMEM_BLK_ALLOC_FROM_OS);
}

#define XSMEM_SHOW_ALLOCED_MEM_MAX_CNT 20
static void xsmem_show_alloced_mem_info(struct xsm_pool *xp)
{
    unsigned int show_cnt = 0;
    unsigned int max_show_cnt = XSMEM_SHOW_ALLOCED_MEM_MAX_CNT;
    unsigned long os_mem = 0;
    unsigned long cache_mem = 0;
    ka_rb_node_t *node = NULL;
    struct xsm_block *blk = NULL;

    ka_task_mutex_lock(&xp->xp_block_mutex);
    xsmem_warn("Show alloced mem start. (pool_id=%d)\n", xp->pool_id);

    node = ka_base_rb_first(&xp->block_root);
    while (node != NULL) {
        blk = ka_base_rb_entry(node, struct xsm_block, block_rb_node);
        node = ka_base_rb_next(node);

        if (blk_is_alloced_from_os(blk)) {
            os_mem += blk->real_size;
        } else {
            show_cnt += _xsmem_show_alloced_mem_info(blk, max_show_cnt);
            max_show_cnt = XSMEM_SHOW_ALLOCED_MEM_MAX_CNT - show_cnt;
            cache_mem += blk->real_size;
        }
    }

    xsmem_warn("Show alloced mem end. (pool_id=%d; os_mem(Bytes)=%lu; cache_mem(Bytes)=%lu; show_cnt=%u)\n",
        xp->pool_id, os_mem, cache_mem, show_cnt);
    ka_task_mutex_unlock(&xp->xp_block_mutex);
}

static int ioctl_xsmem_cache_destroy(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct xsm_cache_destroy_arg cache_arg;
    struct xsm_pool *xp = NULL;

    if (copy_from_user_safe(&cache_arg, (struct xsm_cache_destroy_arg __ka_user *)(uintptr_t)arg,
            sizeof(cache_arg)) != 0) {
        xsmem_err("Copy_from_user failed.\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(cache_arg.poolId);
    if (xp == NULL) {
        xsmem_err("Invalid pool id. (pool_id=%d)\n", cache_arg.poolId);
        return -EINVAL;
    }

    if (xp->create_pid != XSMEM_CURRENT_TGID) {
        xsmem_pool_put(xp);
        xsmem_err("Task has no cache free prop. (proc=%s; tgid=%d; pid=%d; pool_id=%d)\n",
            XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, cache_arg.poolId);
        return -EPERM;
    }

    if (xp->algo->xsm_pool_cache_destroy == NULL) {
        xsmem_pool_put(xp);
        return -EOPNOTSUPP;
    }

    ret = xp->algo->xsm_pool_cache_destroy(xp, &cache_arg);
    if (ret != 0) {
        if (ret == -EBUSY) {
            xsmem_show_alloced_mem_info(xp);
        } else {
            xsmem_err("Xsm_pool_cache_destroy fail. (pool_id=%d; ret=%d)\n", cache_arg.poolId, ret);
        }
        xsmem_pool_put(xp);
        return ret;
    }

    xsmem_pool_put(xp);
    return 0;
}


static int ioctl_xsmem_cache_query(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_query_cache_arg *usr_arg = (struct xsm_query_cache_arg __ka_user *)(uintptr_t)arg;
    struct xsm_query_cache_arg query_arg;
    struct xsm_task_pool_node *node = NULL;
    GrpQueryGroupAddrInfo *cache_buff = NULL;
    unsigned int cache_cnt;
    struct xsm_pool *xp = NULL;
    int ret;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("Copy_from_user failed.\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(query_arg.poolId);
    if (xp == NULL) {
        xsmem_err("Invalid pool id. (pool_id=%d)\n", query_arg.poolId);
        return -EINVAL;
    }

    // for safety, va can't be queried when not in the same pool.
    ka_task_mutex_lock(&task->mutex);
    node = task_pool_node_find(task, xp);
    ka_task_mutex_unlock(&task->mutex);
    if (node == NULL) {
        xsmem_err("Task is not in the pool. (pool_id=%d; task_pid=%d)\n", xp->pool_id, task->pid);
        xsmem_pool_put(xp);
        return -ESRCH;
    }

    if (xp->algo->xsm_pool_cache_query == NULL) {
        xsmem_err("Not support cache. (name=%s)\n", xp->algo->name);
        xsmem_pool_put(xp);
        return -EOPNOTSUPP;
    }

    cache_cnt = query_arg.cache_cnt;
    if (cache_cnt == 0 || cache_cnt > BUFF_GROUP_ADDR_MAX_NUM) {
        cache_cnt = BUFF_GROUP_ADDR_MAX_NUM;
    }
    cache_buff = xsmem_drv_kmalloc(sizeof(GrpQueryGroupAddrInfo) * cache_cnt, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cache_buff == NULL) {
        xsmem_err("Alloc memory for GrpQueryCacheInfo failed.\n");
        xsmem_pool_put(xp);
        return -ENOMEM;
    }

    ret = xp->algo->xsm_pool_cache_query(xp, query_arg.devId, cache_buff, &cache_cnt);
    if (ret != 0) {
        xsmem_drv_kfree(cache_buff);
        xsmem_pool_put(xp);
        xsmem_err("Cache query fail. (pool_id=%d)\n", query_arg.poolId);
        return ret;
    }

    if (cache_cnt != 0) {
        ret = (int)copy_to_user_safe(query_arg.cache_buff, cache_buff,
            cache_cnt * sizeof(GrpQueryGroupAddrInfo));
    }
    ret += (int)ka_base_put_user(cache_cnt, &usr_arg->cache_cnt);

    xsmem_drv_kfree(cache_buff);
    xsmem_pool_put(xp);
    return ret;
}

static int ioctl_xsmem_priv_flag_query(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_query_pool_flag_arg *usr_arg = (struct xsm_query_pool_flag_arg __ka_user *)(uintptr_t)arg;
    struct xsm_query_pool_flag_arg query_arg;
    struct xsm_task_pool_node *node = NULL;
    struct xsm_pool *xp = NULL;
    int ret;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("Copy_from_user failed.\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(query_arg.pool_id);
    if (xp == NULL) {
        xsmem_err("Invalid pool id. (pool_id=%d)\n", query_arg.pool_id);
        return -EINVAL;
    }

    /* for safety, flag can't be queried when not in the same pool. */
    ka_task_mutex_lock(&task->mutex);
    node = task_pool_node_find(task, xp);
    ka_task_mutex_unlock(&task->mutex);
    if (node == NULL) {
        xsmem_err("Task is not in the pool. (pool_id=%d; task_pid=%d)\n", xp->pool_id, task->pid);
        xsmem_pool_put(xp);
        return -ESRCH;
    }

    ret = (int)ka_base_put_user(xp->priv_mbuf_flag, &usr_arg->priv_flag);

    xsmem_pool_put(xp);
    return ret;
}

static int ioctl_xsmem_pool_task_common(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_task_arg proc_arg;
    struct xsm_pool *xp = NULL;
    int ret, create_pid;

    if (copy_from_user_safe(&proc_arg, (struct xsm_task_arg __ka_user *)arg, sizeof(proc_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(proc_arg.pool_id);
    if (xp == NULL) {
        xsmem_err("invalid pool_id %d\n", proc_arg.pool_id);
        return -EINVAL;
    }
    create_pid = xp->create_pid;

    xsmem_debug("create_pid:%d pid:%d cmd %d op pid %d\n", create_pid, task->pid, cmd, proc_arg.pid);

    ka_task_mutex_lock(&task->mutex);
    if (xsmem_is_task_has_admin(task, xp) == false) {
        ka_task_mutex_unlock(&task->mutex);
        xsmem_pool_put(xp);
        xsmem_err("task %d has no admin attr in pool %d(creat_pid %d).\n",
            task->pid, proc_arg.pool_id, create_pid);
        return -EINVAL;
    }
    ka_task_mutex_unlock(&task->mutex);

    ret = xsmem_get_tgid_by_vpid(proc_arg.pid, &proc_arg.pid);
    if (ret != 0) {
        xsmem_pool_put(xp);
        return ret;
    }
    if (cmd == XSMEM_POOL_TASK_ADD) {
        ret = xsmem_pool_task_add(xp, proc_arg.pid, proc_arg.attr);
    } else {
        ret = xsmem_pool_task_del(xp, proc_arg.pid);
    }

    xsmem_pool_put(xp);

    return ret;
}

static int ioctl_xsmem_pool_attach(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_task_attach_arg *usr_arg = (struct xsm_task_attach_arg __ka_user *)arg;
    struct xsm_pool *xp = NULL;
    struct xsm_task_attach_arg attach;
    int pool_id, ret;

    if (copy_from_user_safe(&attach, usr_arg, sizeof(attach)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    pool_id = attach.pool_id;

    xp = xsmem_pool_get(pool_id);
    if (xp == NULL) {
        xsmem_err("pool_id %d couldn't find the pool\n", pool_id);
        return -ENODEV;
    }

    ret = xsmem_pool_attach(task, xp, attach.timeout);
    if (ret != 0) {
        xsmem_pool_put(xp);
        return ret;
    }

    attach.uid = task->uid;
    attach.cache_type = xp->cache_type;
    xsmem_run_info("<%s:%d,%d> pool_id %d tid %d uid %llu attach success, refcnt %d\n", XSMEM_CURRENT_COMM,
        XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, pool_id, XSMEM_CURRENT_TGID, 
        attach.uid, ka_base_atomic_read(&xp->refcnt));
    xsmem_pool_put(xp);

    ret = (int)ka_base_put_user(attach.uid, &usr_arg->uid);
    ret += (int)ka_base_put_user(attach.cache_type, &usr_arg->cache_type);
    return ret;
}

static int ioctl_xsmem_pool_detach(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_pool *xp = NULL;
    int pool_id = (int)arg;
    int ret;

    xp = xsmem_pool_get(pool_id);
    if (xp == NULL) {
        xsmem_err("pool_id %d couldn't find the pool\n", pool_id);
        return -ENODEV;
    }
    ka_task_mutex_lock(&task->mutex);
    ret = xsmem_pool_detach(task, xp);
    if (ret == 0) {
        xsmem_info("pool_id %d task %d detach success,  refcnt %d\n", pool_id, XSMEM_CURRENT_TGID, ka_base_atomic_read(&xp->refcnt));
    }
    ka_task_mutex_unlock(&task->mutex);

    xsmem_pool_put(xp);

    return ret;
}

static void xsmem_task_alloc_info_show(struct xsm_pool *xp)
{
    struct xsm_task_pool_node *node = NULL;

    ka_task_mutex_lock(&xp->mutex);
    ka_list_for_each_entry(node, &xp->node_list_head, pool_node) {
        ka_dfx_pr_notice("Task alloc info, size:Bytes. (pid=%d; alloc_peak_size=%llu; real_alloc_size=%llu)\n",
            node->task->pid, node->alloc_peak_size, node->real_alloc_size);
    }
    ka_task_mutex_unlock(&xp->mutex);
}

static int ioctl_xsmem_block_alloc(struct xsm_pool *xp, struct xsm_task_pool_node *node, struct xsm_block_arg *arg)
{
    struct xsm_task_block_node *task_blk_node = NULL;
    struct xsm_block *blk = NULL;
    int blkid;
    int ret;

    if (node->attr.alloc == 0) {
        xsmem_err("Task has no alloc prop. (proc=%s; tgid=%d; pid=%d; pool_id=%d)\n",
            XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, xp->pool_id);
        return -EINVAL;
    }

    blk = xsmem_drv_kmalloc(sizeof(*blk), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(blk == NULL)) {
        xsmem_err("Alloc xsm_block memory failed.\n");
        return -ENOMEM;
    }

    task_blk_node = xsmem_drv_kmalloc(sizeof(*task_blk_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(task_blk_node == NULL)) {
        xsmem_err("Alloc xshm_task_block_cnt memory failed.\n");
        xsmem_drv_kfree(blk);
        return -ENOMEM;
    }

    blkid = xsmem_blockid_get(xp->mnt_ns);
    if (blkid < 0) {
        xsmem_err("Alloc blkid fail. (blkid=%d)\n", blkid);
        xsmem_drv_kfree(blk);
        xsmem_drv_kfree(task_blk_node);
        return -ENOSPC;
    }

    KA_INIT_LIST_HEAD(&blk->task_blk_head);
    blk->alloc_size = arg->alloc.size;
    blk->flag = arg->alloc.flag;
    ret = xp->algo->xsm_block_alloc(xp, blk);
    if (ret < 0) {
        ka_dfx_pr_notice("Can not alloc block, size:Bytes. (pool_id=%d; pool_size=%lu; alloc_peak_size=%lu; alloced_size=%lu; "
            "current_alloc_size=%lu; flag=0x%lx; ret=%d)\n", xp->pool_id, xp->pool_size, xp->alloc_peak_size,
            xp->real_alloc_size, blk->alloc_size, blk->flag, ret);
        xsmem_task_alloc_info_show(xp);
        xsmem_blockid_put(xp->mnt_ns, blkid);
        xsmem_drv_kfree(blk);
        xsmem_drv_kfree(task_blk_node);
    } else {
        arg->alloc.offset = blk->offset;
        arg->alloc.blkid = (u32)blkid;
        blk->id = blkid;
        blk->pid = XSMEM_CURRENT_TGID;
        task_blk_node->refcnt = 1;
        blk->refcnt = 1;
        ka_task_mutex_lock(&xp->xp_block_mutex);
        xsmem_add_block(xp, blk);
        xsmem_task_link_blk(node, blk, task_blk_node);
        node_stat_add(node, blk);
        xsmem_pool_stat_add(xp, blk);

        xsmem_debug("Block alloc success, size:Bytes. (pool=%d; task=%d; alloc_size=%lu; offset=0x%pK; real_size=%lu; "
            "flag=0x%lx; pool_alloc_size=%lu; pool_real_size=%lu; alloc_peak_size=%lu; pool_size=%lu; "
            "task_alloc_size=%llu; task_real_size=%llu; task_peak_size=%llu)\n", xp->pool_id, node->task->pid,
            blk->alloc_size, (void *)(uintptr_t)blk->offset, blk->real_size, blk->flag, xp->alloc_size, xp->real_alloc_size,
            xp->alloc_peak_size, xp->pool_size, node->alloc_size, node->real_alloc_size, node->alloc_peak_size);
        ka_task_mutex_unlock(&xp->xp_block_mutex);
    }

    return ret;
}

static int ioctl_xsmem_block_free(struct xsm_pool *xp, struct xsm_task_pool_node *node, unsigned long offset)
{
    struct xsm_task_block_node *task_blk_node = NULL;
    struct xsm_block *blk = NULL;

    ka_task_mutex_lock(&xp->xp_block_mutex);
    blk = xsmem_find_block(xp, offset);
    if (blk == NULL) {
        ka_task_mutex_unlock(&xp->xp_block_mutex);
        xsmem_err("Free unalloced block. (pool_id=%d; offset=0x%pK)\n", xp->pool_id, (void *)(uintptr_t)offset);
        return -ENODEV;
    }

    task_blk_node = xsmem_find_task_block_node(node, blk);
    if (task_blk_node == NULL) {
        ka_task_mutex_unlock(&xp->xp_block_mutex);
        xsmem_err("Free other task block. (pool_id=%d; offset=0x%pK)\n", xp->pool_id, (void *)(uintptr_t)offset);
        return -ENODEV;
    }

    xsmem_debug("Block free. (pool=%d; task=%d; offset=0x%pK; alloced_task=%d; ref=%d; blk_ref=%ld)\n",
        xp->pool_id, node->task->pid, (void *)(uintptr_t)blk->offset, blk->pid, task_blk_node->refcnt, blk->refcnt);

    task_blk_node->refcnt--;
    if (task_blk_node->refcnt <= 0) {
        if (blk->pid == node->task->pid) {
            node_stat_sub(node, blk);
        }
        xsmem_task_unlink_blk(task_blk_node);
        xsmem_drv_kfree(task_blk_node);
    }

    blk->refcnt--;
    if (blk->refcnt <= 0) {
        xsmem_pool_stat_sub(xp, blk);
        xsmem_blockid_put(xp->mnt_ns, blk->id);
        xsmem_debug("Block free success. (pool=%d; task=%d; alloc_size=0x%lx; offset=0x%pK; real_size=0x%lx; pid=%d; "
            "pool_alloc_size=0x%lx; pool_real_size=0x%lx; task_alloc_size=0x%llx; task_real_size=0x%llx)\n",
            xp->pool_id, node->task->pid, blk->alloc_size, (void *)(uintptr_t)blk->offset, blk->real_size, blk->pid,
            xp->alloc_size, xp->real_alloc_size, node->alloc_size, node->real_alloc_size);
        xsmem_block_destroy(xp, blk);
    }

    ka_task_mutex_unlock(&xp->xp_block_mutex);

    return 0;
}

static int xsmem_copy_blk_alloc_info_to_user(struct xsm_block_arg *block_arg, struct xsm_block_arg *usr_arg)
{
    int ret;

    ret = (int)ka_base_put_user(block_arg->alloc.offset, &usr_arg->alloc.offset);
    if (ret != 0) {
        xsmem_err("Xsmem put alloc offset to user fail.\n");
        return ret;
    }

    ret = (int)ka_base_put_user(block_arg->alloc.blkid, &usr_arg->alloc.blkid);
    if (ret != 0) {
        xsmem_err("Xsmem put alloc blkid to user fail.\n");
        return ret;
    }

    return 0;
}

static int ioctl_xsmem_block_common(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_block_arg *usr_arg = (struct xsm_block_arg __ka_user *)arg;
    struct xsm_task_pool_node *node = NULL;
    struct xsm_block_arg block_arg;
    struct xsm_pool *xp = NULL;
    int ret = 0;

    if (copy_from_user_safe(&block_arg, usr_arg, sizeof(block_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(block_arg.pool_id);
    if (xp == NULL) {
        xsmem_err("invalid pool_id\n");
        return -EINVAL;
    }

    ka_task_mutex_lock(&task->mutex);
    node = task_pool_node_find(task, xp);
    if (node == NULL) {
        ka_task_mutex_unlock(&task->mutex);
        xsmem_pool_put(xp);
        xsmem_err("Task is not attached to pool. (proc=%s, tgid=%d, pid=%d, pool_id=%d)\n",
            XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, block_arg.pool_id);
        return -EINVAL;
    }

    if (cmd == XSMEM_BLOCK_ALLOC) {
        ret = ioctl_xsmem_block_alloc(xp, node, &block_arg);
        if (ret == 0) {
            ret = xsmem_copy_blk_alloc_info_to_user(&block_arg, usr_arg);
        }
    } else {
        ret = ioctl_xsmem_block_free(xp, node, block_arg.free.offset);
    }
    ka_task_mutex_unlock(&task->mutex);

    xsmem_pool_put(xp);

    return ret;
}

static int xsmem_pool_block_get(struct xsm_pool *xp, struct xsm_task_pool_node *node, struct xsm_block_arg *arg)
{
    struct xsm_task_block_node *task_blk_node = NULL;
    struct xsm_block *blk = NULL;
    unsigned long offset = arg->get.offset;

    ka_task_mutex_lock(&xp->xp_block_mutex);
    blk = xsmem_find_block(xp, offset);
    if (blk == NULL) {
        ka_task_mutex_unlock(&xp->xp_block_mutex);
        return -ENODEV;
    }

    task_blk_node = xsmem_find_task_block_node(node, blk);
    if (task_blk_node == NULL) {
        task_blk_node = xsmem_drv_kmalloc(sizeof(*task_blk_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (ka_unlikely(task_blk_node == NULL)) {
            ka_task_mutex_unlock(&xp->xp_block_mutex);
            xsmem_err("alloc task_blk_node memory failed\n");
            return -ENOMEM;
        }

        xsmem_task_link_blk(node, blk, task_blk_node);
        task_blk_node->refcnt = 0;
    } else {
        if (task_blk_node->refcnt >= TASK_BLK_MAX_GET_NUM) {
            ka_task_mutex_unlock(&xp->xp_block_mutex);
            xsmem_err("Task get blk too many times\n");
            return -EINVAL;
        }
    }

    blk->refcnt++;
    task_blk_node->refcnt++;

    arg->get.alloc_size = blk->alloc_size;
    arg->get.alloc_offset = blk->offset;
    arg->get.blkid = (u32)blk->id;

    xsmem_debug("pool %d task %d get offset %pK, blk offset %pK size %lx alloced by task %d ref %d blk ref %ld\n",
        xp->pool_id, node->task->pid, (void *)(uintptr_t)offset, (void *)(uintptr_t)blk->offset, blk->alloc_size,
        blk->pid, task_blk_node->refcnt, blk->refcnt);

    ka_task_mutex_unlock(&xp->xp_block_mutex);

    return 0;
}

static int ioctl_xsmem_block_get(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_block_arg *usr_arg = (struct xsm_block_arg __ka_user *)arg;
    struct xsm_task_pool_node *node = NULL;
    struct xsm_block_arg block_arg;

    if (copy_from_user_safe(&block_arg, usr_arg, sizeof(block_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }
    ka_task_mutex_lock(&task->mutex);
    ka_list_for_each_entry(node, &task->node_list_head, task_node) {
        int ret = xsmem_pool_block_get(node->pool, node, &block_arg);
        if (ret == 0) {
            int pool_id = node->pool->pool_id;
            ka_task_mutex_unlock(&task->mutex);
            ret = (int)ka_base_put_user(block_arg.get.alloc_offset, &usr_arg->get.alloc_offset);
            ret += (int)ka_base_put_user(block_arg.get.alloc_size, &usr_arg->get.alloc_size);
            ret += (int)ka_base_put_user(block_arg.get.blkid, &usr_arg->get.blkid);
            ret += (int)ka_base_put_user(pool_id, &usr_arg->pool_id);
            return ret;
        }
    }
    ka_task_mutex_unlock(&task->mutex);
    xsmem_warn("offset %pK not find\n", (void *)(uintptr_t)block_arg.get.offset);
    return -ENODEV;
}

static int ioctl_xsmem_pool_id_query(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_pool_query_arg *usr_arg = (struct xsm_pool_query_arg __ka_user *)arg;
    struct xsm_pool_query_arg query_arg;
    struct xsm_pool *xp = NULL;
    char key_tmp[XSHM_KEY_AND_NAMESPACE_SIZE] = {0};
    char key[XSHM_KEY_SIZE];
    int ret;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    if ((query_arg.key == NULL) || (query_arg.key_len == 0) || (query_arg.key_len >= XSHM_KEY_SIZE)) {
        xsmem_err("Key or key_len is invalid. (key_len=%u)\n", query_arg.key_len);
        return -EINVAL;
    }

    if (copy_from_user_safe(key, (char __ka_user *)query_arg.key, query_arg.key_len) != 0) {
        xsmem_err("copy key from user failed, key_len %d\n", query_arg.key_len);
        return -EFAULT;
    }
    key[query_arg.key_len] = '\0';

    ret = xsmem_strcat_with_ns(key_tmp, XSHM_KEY_AND_NAMESPACE_SIZE, key);
    if (ret < 0) {
        xsmem_err("Sprintf_s error. (ret=%d)\n", ret);
        return -EINVAL;
    }

    query_arg.key_len = (u32)ka_base_strlen(key_tmp);
    xsmem_debug("Attempt to find pool name. (pool name=%s)\n", key_tmp);
    ka_task_mutex_lock(&xsmem_mutex);
    xp = xsmem_pool_hnode_find((const char *)key_tmp, query_arg.key_len);
    if (xp == NULL) {
        ka_task_mutex_unlock(&xsmem_mutex);
        return -EINVAL;
    }

    query_arg.pool_id = xp->pool_id;
    ka_task_mutex_unlock(&xsmem_mutex);

    return (int)ka_base_put_user(query_arg.pool_id, &usr_arg->pool_id);
}

static unsigned int xsmem_get_user_pool_name_len(const char *name)
{
    char *name_tmp = NULL;
    unsigned int name_len;

    name_tmp = ka_base_strrchr(name, '_');
    if (name_tmp == NULL) {
        xsmem_err("Strrchr failed.\n");
        return 0;
    }

    name_len = (unsigned int)(ka_base_strlen(name) - ka_base_strlen(name_tmp));
    return name_len;
}

static int ioctl_xsmem_pool_name_query(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_pool_query_arg query_arg;
    struct xsm_pool *xp = NULL;
    unsigned int key_len;
    int ret = -EINVAL;

    if (copy_from_user_safe(&query_arg, (struct xsm_pool_query_arg __ka_user *)arg, sizeof(query_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(query_arg.pool_id);
    if (xp == NULL) {
        return -EINVAL;
    }

    key_len = xsmem_get_user_pool_name_len(xp->key);

    xsmem_debug("Key info. (key=%s; key_len=%u; query_key_len=%u)\n", xp->key, key_len, query_arg.key_len);
    if ((key_len > 0) && (key_len < query_arg.key_len)) {
        ret = (int)copy_to_user_safe(query_arg.key, xp->key, key_len);
    }

    xsmem_pool_put(xp);

    return ret;
}

static int ioctl_xsmem_pool_task_query(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_query_pool_task_arg *usr_arg = (struct xsm_query_pool_task_arg __ka_user *)arg;
    struct xsm_query_pool_task_arg query_arg;
    struct xsm_task_pool_node *node = NULL;
    struct xsm_pool *xp = NULL;
    int task_num = 0, ret = 0, task_pid;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(query_arg.pool_id);
    if (xp == NULL) {
        xsmem_err("invalid pool_id %d\n", query_arg.pool_id);
        return -EINVAL;
    }

    ka_task_mutex_lock(&xp->mutex);

    ka_list_for_each_entry(node, &xp->node_list_head, pool_node) {
        if (task_num < query_arg.pid_num) {
            task_pid = (int)node->task->vpid;
            ret += (int)ka_base_put_user(task_pid, query_arg.pid + task_num);
            task_num++;
        }
    }

    ka_task_mutex_unlock(&xp->mutex);

    xsmem_pool_put(xp);

    if (ret != 0) {
        xsmem_err("pool_id %d task num %d put_user failed ret %d\n", query_arg.pool_id, task_num, ret);
        return -EFAULT;
    }

    return (int)ka_base_put_user(task_num, &usr_arg->pid_num);
}

static int ioctl_xsmem_pool_task_attr_query(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_task_arg *usr_arg = (struct xsm_task_arg __ka_user *)arg;
    struct xsm_adding_task *adding_task = NULL;
    struct xsm_task_pool_node *node = NULL;
    struct xsm_task_arg query_arg;
    struct xsm_pool *xp = NULL;
    int ret;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(query_arg.pool_id);
    if (xp == NULL) {
        xsmem_err("pool_id %d couldn't find the pool\n", query_arg.pool_id);
        return -ENODEV;
    }

    ret = xsmem_get_tgid_by_vpid(query_arg.pid, &query_arg.pid);
    if (ret != 0) {
        xsmem_pool_put(xp);
        return ret;
    }

    ka_task_mutex_lock(&xp->mutex);
    node = xsmem_pool_task_node_find(xp, query_arg.pid);
    if (node != NULL) {
        query_arg.attr = node->attr;
        ret = 0;
    } else {
        adding_task = xsmem_pool_adding_task_find(xp, query_arg.pid);
        if (adding_task != NULL) {
            query_arg.attr = adding_task->attr;
            ret = 0;
        } else {
            ret = -EINVAL;
        }
    }

    ka_task_mutex_unlock(&xp->mutex);
    xsmem_pool_put(xp);

    if (ret == 0) {
        if (copy_to_user_safe(usr_arg, &query_arg, sizeof(query_arg)) != 0) {
            xsmem_err("copy_to_user failed\n");
            return -EFAULT;
        }
    }

    return ret;
}

static int xsmem_task_in_pool_query(struct xsm_query_task_pool_arg *query_arg)
{
    struct xsm_task_pool_node *node = NULL;
    struct xsm_task *task = NULL;
    int pool_num = 0, ret = 0;

    ka_task_mutex_lock(&task_mutex);
    task = xsm_task_find(query_arg->pid);
    if (task == NULL) {
        ka_task_mutex_unlock(&task_mutex);
        return -EFAULT;
    }

    ka_task_mutex_lock(&task->mutex);
    ka_list_for_each_entry(node, &task->node_list_head, task_node) {
        if (pool_num >= query_arg->pool_num) {
            xsmem_info("<%s:%d,%d> task in pool query, query pid: %d, pool num: %d\n",
                XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, query_arg->pid, query_arg->pool_num);
            break;
        }

        ret += (int)ka_base_put_user(node->pool->pool_id, query_arg->pool_id + pool_num);
        pool_num++;
    }
    ka_task_mutex_unlock(&task->mutex);

    ka_task_mutex_unlock(&task_mutex);

    if (ret != 0) {
        xsmem_err("task %d pool num %d put_user failed ret %d\n", query_arg->pid, pool_num, ret);
        return -EFAULT;
    }

    query_arg->query_num = pool_num;

    return 0;
}

static int xsmem_adding_task_search(int id, void *p, void *data)
{
    struct xsm_pool *xp = p;
    struct xsm_adding_task *adding_task = NULL;
    struct xsm_query_task_pool_arg *query_arg = data;
    int ret = 0;

    if (query_arg->query_num >= query_arg->pool_num) {
        xsmem_info("<%s:%d,%d> adding task search, query pid: %d, query num: %d, pool num: %d\n",
            XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, 
            query_arg->pid, query_arg->query_num, query_arg->pool_num);
        return ret;
    }

    ka_task_mutex_lock(&xp->mutex);
    adding_task = xsmem_pool_adding_task_find(xp, query_arg->pid);
    ka_task_mutex_unlock(&xp->mutex);

    if (adding_task != NULL) {
        ret = (int)ka_base_put_user(xp->pool_id, query_arg->pool_id + query_arg->query_num);
        query_arg->query_num++;
    }

    return ret;
}

static int xsmem_task_adding_to_pool_query(struct xsm_query_task_pool_arg *query_arg)
{
    int ret;

    query_arg->query_num = 0;

    ka_task_mutex_lock(&xsmem_mutex);
    ret = xsmem_id_for_each(xsmem_adding_task_search, query_arg);
    ka_task_mutex_unlock(&xsmem_mutex);

    return ret;
}

static int ioctl_xsmem_task_pool_query(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_query_task_pool_arg *usr_arg = (struct xsm_query_task_pool_arg __ka_user *)arg;
    struct xsm_query_task_pool_arg query_arg;
    int ret;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    ret = xsmem_get_tgid_by_vpid(query_arg.pid, &query_arg.pid);
    if (ret != 0) {
        return ret;
    }

    if (query_arg.type == XSMEM_TASK_IN_POOL) {
        ret = xsmem_task_in_pool_query(&query_arg);
    } else {
        ret = xsmem_task_adding_to_pool_query(&query_arg);
    }

    return (ret != 0) ? ret : (int)ka_base_put_user(query_arg.query_num, &usr_arg->pool_num);
}

static int ioctl_xsmem_poll_exit_task(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_poll_exit_task_arg *usr_arg = (struct xsm_poll_exit_task_arg __ka_user *)arg;
    struct xsm_poll_exit_task_arg query_arg;
    struct xsm_task_pool_node *node = NULL;
    struct xsm_pool *xp = NULL;
    struct xsm_exit_task *exit_task = NULL;
    int ret = -1;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(query_arg.pool_id);
    if (xp == NULL) {
        xsmem_err("pool_id %d couldn't find the pool\n", query_arg.pool_id);
        return -ENODEV;
    }
    ka_task_mutex_lock(&task->mutex);
    node = task_pool_node_find(task, xp);
    if (node != NULL) {
        ka_task_mutex_lock(&node->mutex);
        if ((ka_list_empty_careful(&node->exit_task_head)) == 0) {
            exit_task = ka_list_first_entry(&node->exit_task_head, struct xsm_exit_task, node);
            xsmem_info("pool %d task %d poll task pid %d uid %llu exit\n",
                xp->pool_id, task->pid, exit_task->pid, exit_task->uid);
            query_arg.uid = exit_task->uid;
            ret = (int)ka_base_put_user(query_arg.uid, &usr_arg->uid);
            ka_list_del(&exit_task->node);
            xsmem_drv_kfree(exit_task);
        }
        ka_task_mutex_unlock(&node->mutex);
    }
    ka_task_mutex_unlock(&task->mutex);

    xsmem_pool_put(xp);

    return ret;
}

static int ioctl_xsmem_pool_va_check(struct xsm_task *task, unsigned int cmd, unsigned long arg)
{
    struct xsm_check_va_arg *usr_arg = (struct xsm_check_va_arg __ka_user *)(uintptr_t)arg;
    struct xsm_check_va_arg query_arg;
    struct xsm_pool *xp = NULL;
    int ret;

    if (copy_from_user_safe(&query_arg, usr_arg, sizeof(query_arg)) != 0) {
        xsmem_err("copy_from_user failed\n");
        return -EFAULT;
    }

    xp = xsmem_pool_get(query_arg.pool_id);
    if (xp == NULL) {
        xsmem_err("Invalid pool id. (pool_id=%d)\n", query_arg.pool_id);
        return -EINVAL;
    }

    if (xp->algo->xsm_pool_va_check == NULL) {
        xsmem_err("Not support va check. (name=%s)\n", xp->algo->name);
        xsmem_pool_put(xp);
        return -EOPNOTSUPP;
    }

    ret = xp->algo->xsm_pool_va_check(xp, query_arg.va, &query_arg.result);
    if (ret != 0) {
        xsmem_err("Check va failed. (pool_id=%d)\n", query_arg.pool_id);
    }
    xsmem_pool_put(xp);

    return (ret != 0) ? ret : (int)ka_base_put_user(query_arg.result, &usr_arg->result);
}

static int (*const xsmem_ioctl_handles[XSMEM_MAX_CMD])(struct xsm_task *task, unsigned int cmd, unsigned long arg) = {
    [_KA_IOC_NR(XSMEM_POOL_REGISTER)] = ioctl_xsmem_pool_register,
    [_KA_IOC_NR(XSMEM_POOL_UNREGISTER)] = ioctl_xsmem_pool_unregister,
    [_KA_IOC_NR(XSMEM_POOL_TASK_ADD)] = ioctl_xsmem_pool_task_common,
    [_KA_IOC_NR(XSMEM_POOL_TASK_DEL)] = ioctl_xsmem_pool_task_common,
    [_KA_IOC_NR(XSMEM_POOL_ATTACH)] = ioctl_xsmem_pool_attach,
    [_KA_IOC_NR(XSMEM_POOL_DETACH)] = ioctl_xsmem_pool_detach,
    [_KA_IOC_NR(XSMEM_BLOCK_ALLOC)] = ioctl_xsmem_block_common,
    [_KA_IOC_NR(XSMEM_BLOCK_FREE)] = ioctl_xsmem_block_common,
    [_KA_IOC_NR(XSMEM_BLOCK_GET)] = ioctl_xsmem_block_get,
    [_KA_IOC_NR(XSMEM_BLOCK_PUT)] = ioctl_xsmem_block_common,
    [_KA_IOC_NR(XSMEM_POOL_ID_QUERY)] = ioctl_xsmem_pool_id_query,
    [_KA_IOC_NR(XSMEM_POOL_NAME_QUERY)] = ioctl_xsmem_pool_name_query,
    [_KA_IOC_NR(XSMEM_POOL_TASK_QUERY)] = ioctl_xsmem_pool_task_query,
    [_KA_IOC_NR(XSMEM_POOL_TASK_ATTR_QUERY)] = ioctl_xsmem_pool_task_attr_query,
    [_KA_IOC_NR(XSMEM_TASK_POOL_QUERY)] = ioctl_xsmem_task_pool_query,
    [_KA_IOC_NR(XSMEM_PROP_OP)] = ioctl_xsmem_pool_prop_op,
    [_KA_IOC_NR(XSMEM_POLL_EXIT_TASK)] = ioctl_xsmem_poll_exit_task,
    [_KA_IOC_NR(XSMEM_CACHE_CREATE)] = ioctl_xsmem_cache_create,
    [_KA_IOC_NR(XSMEM_CACHE_DESTROY)] = ioctl_xsmem_cache_destroy,
    [_KA_IOC_NR(XSMEM_CACHE_QUERY)] = ioctl_xsmem_cache_query,
    [_KA_IOC_NR(XSMEM_POOL_FLAG_QUERY)] = ioctl_xsmem_priv_flag_query,
    [_KA_IOC_NR(XSMEM_VADDR_CHECK)] = ioctl_xsmem_pool_va_check,
};

STATIC long xsmem_ioctl(ka_file_t *file, unsigned int cmd, unsigned long arg)
{
    struct xsm_task *task = ka_fs_get_file_private_data(file);
    int cmd_nr = _KA_IOC_NR(cmd);
    if ((cmd_nr < 0) || (cmd_nr >= XSMEM_MAX_CMD) || (xsmem_ioctl_handles[cmd_nr] == NULL)) {
        xsmem_err("unsupported command %x\n", cmd);
        return -EINVAL;
    }
    if (task == NULL) {
#ifndef EMU_ST
        xsmem_err("The filep private_data is NULL.\n");
        return -ESRCH;
#endif
    }

    return xsmem_ioctl_handles[cmd_nr](task, cmd, arg);
}

#ifndef EMU_ST
static ka_pid_t xsmem_get_vpid(ka_task_struct_t *tsk)
{
    return ka_task_task_pid_vnr(tsk);
}
#endif
STATIC int xsmem_open(ka_inode_t *inode, ka_file_t *file)
{
    struct xsm_task *task = NULL;
    ka_task_mutex_lock(&task_mutex);

    task = xsm_task_find(XSMEM_CURRENT_TGID);
    if (task != NULL) {
        xsmem_err("pid %d has been opened\n", task->pid);
        ka_task_mutex_unlock(&task_mutex);
        return -ENOMEM;
    }

    task = xsmem_drv_kmalloc(sizeof(*task), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ka_unlikely(task == NULL)) {
        ka_task_mutex_unlock(&task_mutex);
        xsmem_err("kmalloc failed\n");
        return -ENOMEM;
    }

    ka_fs_set_file_private_data(file, (void*)task);

    /* mmget in open avoid share pool group exit in mm_exit before fput release */
    task->mm = ka_task_get_current_mm();
    ka_mm_mmget(task->mm);
    task->pid = XSMEM_CURRENT_TGID;
    task->vpid = xsmem_get_vpid(ka_task_get_current());
    task->attached_pool_count = 0;
    task->uid = ++g_uni_process_id;
    ka_base_atomic_set(&task->pool_num, 0);
    KA_INIT_LIST_HEAD(&task->node_list_head);
    KA_INIT_LIST_HEAD(&task->register_xp_list_head);

    ka_task_mutex_init(&task->mutex);
    xsm_task_add(task);
    ka_task_mutex_unlock(&task_mutex);

    proc_fs_add_task(task);

    xsmem_run_info("Xsmem open. (task_name=%s; tgid=%d; pid=%d; task_vpid=%d)\n",
        XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, task->vpid);

    return 0;
}

STATIC int xsmem_release(ka_inode_t *inode, ka_file_t *file)
{
    struct xsm_task_pool_node *node = NULL, *node_tmp = NULL;
    struct xsm_pool *xp = NULL, *xp_tmp = NULL;
    struct xsm_task *task = ka_fs_get_file_private_data(file);

    if (task == NULL) {
        xsmem_err("The filep private_data is NULL.\n");
        return -ESRCH;
    }

    xsmem_info("<%s:%d,%d> task %d release\n",
        XSMEM_CURRENT_COMM, XSMEM_CURRENT_TGID, XSMEM_CURRENT_PID, task->pid);

    ka_task_mutex_lock(&task->mutex);
    ka_list_for_each_entry_safe(node, node_tmp, &task->node_list_head, task_node) {
        xp = node->pool;
        xsmem_info("Release. (pid=%d; pool_id=%d; task_id=%d; refcnt=%d; task_alloc_peak_size=%llu; "
            "pool_alloc_peak_size=%lu)\n", task->pid, xp->pool_id, node->task_id, ka_base_atomic_read(&xp->refcnt),
            node->alloc_peak_size, xp->alloc_peak_size);

        xsmem_pool_notice_other_task(xp, node);
        xsmem_pool_task_detach(node);
    }

    ka_list_for_each_entry_safe(xp, xp_tmp, &task->register_xp_list_head, register_task_list) {
        ka_list_del(&xp->register_task_list);
        xsmem_task_unregister_pool(task, xp);
    }

    ka_task_mutex_unlock(&task->mutex);

    proc_fs_del_task(task);
    xsmem_task_prop_del(task->pid);
    ka_mm_mmput(task->mm);

    ka_task_mutex_lock(&task_mutex);
    xsm_task_del(task);
    ka_task_mutex_unlock(&task_mutex);
    xsmem_drv_kfree(task);

    return 0;
}

static int xsmem_mmap_not_support(ka_file_t *file, ka_vm_area_struct_t *vma)
{
    return -ENOTSUPP;
}

static ka_file_operations_t xsmem_fops = {
    ka_fs_init_f_open(xsmem_open)
    ka_fs_init_f_release(xsmem_release)
    ka_fs_init_f_unlocked_ioctl(xsmem_ioctl)
    ka_fs_init_f_mmap(xsmem_mmap_not_support)
};

#ifndef CFG_FEATURE_EXTERNAL_CDEV
static ka_miscdevice_t xsmem_dev = {
    ka_driver_init_md_minor(KA_MISC_DYNAMIC_MINOR)
    ka_driver_init_md_name(XSMEM_DEVICE_NAME)
    ka_driver_init_md_fops(&xsmem_fops)
};

static int xsmem_dev_reg(void)
{
    return ka_driver_misc_register(&xsmem_dev);
}

static void xsmem_dev_unreg(void)
{
    ka_driver_misc_deregister(&xsmem_dev);
}
#else
static int xsmem_notifier_release(ka_file_t *file, unsigned long mode)
{
    return xsmem_release(NULL, file);
}

static const struct notifier_operations xsmem_notifier_ops = {
    .notifier_call =  xsmem_notifier_release,
};

static int xsmem_dev_reg(void)
{
    int ret;

    /* davinci_recycle can't call mg_sp_free, because davinci_recycle is kernel thread, which is not in the sp group */
    ka_fs_set_file_operations_release(&xsmem_fops);

    ret = drv_davinci_register_sub_module(DAVINCI_XSMEM_SUB_MODULE_NAME, &xsmem_fops);
    if (ret != 0) {
        xsmem_err("Register sub module fail. (ret=%d)\n", ret);
        return ret;
    }
    ret = drv_ascend_register_notify(DAVINCI_XSMEM_SUB_MODULE_NAME, &xsmem_notifier_ops);
    if (ret != 0) {
        (void)drv_ascend_unregister_sub_module(DAVINCI_XSMEM_SUB_MODULE_NAME);
        xsmem_err("Register notify fail. (ret=%d)\n", ret);
        return ret;
    }
    return ret;
}

static void xsmem_dev_unreg(void)
{
    (void)drv_ascend_unregister_sub_module(DAVINCI_XSMEM_SUB_MODULE_NAME);
}
#endif

static void xsmem_algo_register(void)
{
#ifdef CFG_FEATURE_SUPPORT_VMA
    xsmem_register_algo(xsm_get_vma_algo());
#endif
#ifdef CFG_FEATURE_SUPPORT_SP
    xsmem_register_algo(xsm_get_sp_algo());
#endif

#ifdef CFG_FEATURE_SUPPORT_CACHE
#ifdef CFG_FEATURE_SUPPORT_VMA
    xsmem_register_algo(xsm_get_cache_vma_algo());
#endif
#ifdef CFG_FEATURE_SUPPORT_SP
    xsmem_register_algo(xsm_get_cache_sp_algo());
    /* vma algo for cache memory assignment */
    xsmem_register_algo(xsm_get_vma_algo());
#endif
#endif
}

STATIC int __init xsmem_init(void)
{
    int ret;

    ret = xsmem_dev_reg();
    if (ret != 0) {
        xsmem_err("misc_register failed, %d\n", ret);
        return ret;
    }
    xsmem_algo_register();

    xsmem_id_switch_init();
    xsmem_proc_fs_init();

    xsmem_info("Module init success.\n");
    return 0;
};
ka_module_init(xsmem_init);

STATIC void __exit xsmem_exit(void)
{
    xsmem_proc_fs_uninit();
    xsmem_dev_unreg();
    xsmem_info("Module exit.\n");
}
ka_module_exit(xsmem_exit);

KA_MODULE_LICENSE("GPL v2");

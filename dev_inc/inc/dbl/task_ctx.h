/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2024-03-15
 */

#ifndef TASK_CTX_H
#define TASK_CTX_H

#include <linux/version.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/seq_file.h>

#include "kref_safe.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"
#include "ka_memory_pub.h"

/*
   usage : Process Context Management with reference counting
        1. call task_ctx_domain_create create a domain when module_init
        2. call task_ctx_create create a task context in the domain when process open a file
        3. call task_ctx_get and task_ctx_put in business
        4. call task_ctx_destroy when process exit or close a file
        5. call task_ctx_for_each_safe traverse a domain
        6. task_ctx_domain_show
        7. call task_ctx_domain_destroy destroy domain when module_exit
*/

#define APM_CTX_HASH_BIT    6
#define APM_CTX_HASH_MAX    (1 << APM_CTX_HASH_BIT)

struct task_ctx_domain {
    KA_DECLARE_HASHTABLE(ctx_htable, APM_CTX_HASH_BIT);
    ka_rwlock_t ctx_lock[APM_CTX_HASH_MAX];
    ka_mutex_t mutex;
    const char *name;
    ka_atomic_t task_num;
    u32 task_max_num;
};

struct task_ctx {
    struct kref_safe ref;
    int tgid;
    char name[TASK_COMM_LEN];
    ka_hlist_node_t link;
    ka_mutex_t mutex;
    struct task_ctx_domain *domain;
    void *priv;
    void (*release)(struct task_ctx *ctx);
};

/* task_max_num=0, Not restricted */
static inline void task_ctx_domain_init(struct task_ctx_domain *domain, const char *name, u32 task_max_num)
{
    u32 i;

    domain->name = name;
    domain->task_max_num = task_max_num;
    ka_hash_init(domain->ctx_htable);
    ka_task_mutex_init(&domain->mutex);
    for (i = 0; i < APM_CTX_HASH_MAX; i++) {
        ka_task_rwlock_init(&domain->ctx_lock[i]);
    }
}

/* interface */
static inline struct task_ctx_domain *task_ctx_domain_create(const char *name, u32 task_max_num)
{
    struct task_ctx_domain *domain = NULL;

    domain = ka_mm_vzalloc(sizeof(*domain));
    if (domain == NULL) {
        ka_dfx_pr_err("Vmalloc domain fail. (domain=%s; size=%lu)\n", name, sizeof(*domain));
        return NULL;
    }

    task_ctx_domain_init(domain, name, task_max_num);

    return domain;
}

/* interface */
static inline void task_ctx_domain_destroy(struct task_ctx_domain *domain)
{
    if (ka_base_atomic_read(&domain->task_num) > 0) {
        ka_dfx_pr_warn("Task leak. (domain=%s; task_num=%d)\n", domain->name, ka_base_atomic_read(&domain->task_num));
    }

    ka_task_mutex_destroy(&domain->mutex);
    ka_mm_vfree(domain);
}

static inline u32 task_get_bkt_from_tgid(int tgid)
{
    return ka_hash_min(tgid, APM_CTX_HASH_BIT);
}

static inline void task_ctx_add(struct task_ctx_domain *domain, struct task_ctx *ctx)
{
    u32 bkt = task_get_bkt_from_tgid(ctx->tgid);
    ka_hash_add(domain->ctx_htable, &ctx->link, bkt);
}

static inline void task_ctx_del(struct task_ctx *ctx)
{
    ka_hash_del(&ctx->link);
}

static inline struct task_ctx *_task_ctx_find(struct task_ctx_domain *domain, int tgid)
{
    u32 bkt = task_get_bkt_from_tgid(tgid);
    struct task_ctx *ctx = NULL;

    ka_hash_for_each_possible(domain->ctx_htable, ctx, link, bkt) {
        if (ctx->tgid == tgid) {
            return ctx;
        }
    }
    return NULL;
}

static inline int task_ctx_add_to_domain(struct task_ctx_domain *domain, struct task_ctx *ctx)
{
    u32 bkt = task_get_bkt_from_tgid(ctx->tgid);

    ka_task_write_lock_bh(&domain->ctx_lock[bkt]);
    if (_task_ctx_find(domain, ctx->tgid) != NULL) {
        ka_task_write_unlock_bh(&domain->ctx_lock[bkt]);
        return -EEXIST;
    }
    task_ctx_add(domain, ctx);
    ka_task_write_unlock_bh(&domain->ctx_lock[bkt]);

    ka_base_atomic_inc(&domain->task_num);

    return 0;
}

static inline void _task_ctx_release(struct task_ctx_domain *domain, struct task_ctx *ctx)
{
    u32 bkt = task_get_bkt_from_tgid(ctx->tgid);

    ka_base_atomic_dec(&domain->task_num);

    ka_task_write_lock_bh(&domain->ctx_lock[bkt]);
    task_ctx_del(ctx);
    ka_task_write_unlock_bh(&domain->ctx_lock[bkt]);
    if (ctx->release != NULL) {
        ctx->release(ctx);
    }
    ka_task_mutex_destroy(&ctx->mutex);
    ka_mm_vfree(ctx);
}

static inline void task_ctx_release(struct kref_safe *ref)
{
    struct task_ctx *ctx = ka_container_of(ref, struct task_ctx, ref);
    _task_ctx_release(ctx->domain, ctx);
}

static inline struct task_ctx *_task_ctx_get(struct task_ctx_domain *domain, int tgid)
{
    struct task_ctx *ctx = _task_ctx_find(domain, tgid);
    if (ctx != NULL) {
        if (kref_safe_get_unless_zero(&ctx->ref) == 0) {
            return NULL;
        }
    }

    return ctx;
}

/* interface */
static inline struct task_ctx *task_ctx_get(struct task_ctx_domain *domain, int tgid)
{
    u32 bkt = task_get_bkt_from_tgid(tgid);
    struct task_ctx *ctx = NULL;

    ka_task_read_lock_bh(&domain->ctx_lock[bkt]);
    ctx = _task_ctx_get(domain, tgid);
    ka_task_read_unlock_bh(&domain->ctx_lock[bkt]);

    return ctx;
}

/* interface */
static inline void task_ctx_put(struct task_ctx *ctx)
{
    kref_safe_put(&ctx->ref, task_ctx_release);
}

static inline int _task_ctx_init(struct task_ctx_domain *domain, struct task_ctx *ctx,
    int tgid, void *priv, void (*release)(struct task_ctx *ctx))
{
    (void)strncpy_s(ctx->name, TASK_COMM_LEN, ka_task_get_current()->comm, TASK_COMM_LEN);
    ctx->tgid = tgid;
    ctx->priv = priv;
    ka_task_mutex_init(&ctx->mutex);
    ctx->domain = domain;
    ctx->release = release;
    kref_safe_init(&ctx->ref);
    return task_ctx_add_to_domain(domain, ctx);
}

static inline int task_ctx_init(struct task_ctx_domain *domain, struct task_ctx *ctx,
    int tgid, void *priv, void (*release)(struct task_ctx *ctx))
{
    if ((domain->task_max_num > 0) && (ka_base_atomic_read(&domain->task_num) >= domain->task_max_num)) {
        ka_dfx_pr_warn("Task overflow. (domain=%s; task_max_num=%u)\n", domain->name, domain->task_max_num);
        return -EFAULT;
    }

    return _task_ctx_init(domain, ctx, tgid, priv, release);
}

/* interface */
static inline int task_ctx_create(struct task_ctx_domain *domain,
    int tgid, void *priv, void (*release)(struct task_ctx *ctx))
{
    int ret;
    struct task_ctx *ctx = ka_mm_vzalloc(sizeof(*ctx));
    if (ctx == NULL) {
        return -ENOMEM;
    }

    ret = task_ctx_init(domain, ctx, tgid, priv, release);
    if (ret != 0) {
        ka_mm_vfree(ctx);
    }

    return ret;
}

/* interface */
static inline void task_ctx_destroy(struct task_ctx *ctx)
{
    task_ctx_put(ctx);
}

static inline struct task_ctx *task_ctx_get_first(struct hlist_head *head, rwlock_t *lock)
{
    struct task_ctx *ctx = NULL;

    ka_task_read_lock_bh(lock);
    ctx = ka_hlist_entry_safe(head->first, struct task_ctx, link);
    if (ctx != NULL) {
        if (kref_safe_get_unless_zero(&ctx->ref) == 0) {
            ctx = NULL;
        }
    }
    ka_task_read_unlock_bh(lock);

    return ctx;
}

static inline struct task_ctx *task_ctx_get_next(struct task_ctx *ctx_from, rwlock_t *lock)
{
    struct task_ctx *ctx = NULL;

    ka_task_read_lock_bh(lock);
    ctx = ka_hlist_entry_safe(ctx_from->link.next, struct task_ctx, link);
    if (ctx != NULL) {
        if (kref_safe_get_unless_zero(&ctx->ref) == 0) {
            ctx = NULL;
        }
    }
    ka_task_read_unlock_bh(lock);
    return ctx;
}

static inline void _task_ctx_for_each_safe(struct hlist_head *head, rwlock_t *lock,
    void *priv, void (*func)(struct task_ctx *ctx, void *priv))
{
    struct task_ctx *ctx = NULL;

    ctx = task_ctx_get_first(head, lock);

    while (ctx != NULL) {
        /* search and then release to ensure that the ctx is not deleted before the search */
        struct task_ctx *next_ctx = task_ctx_get_next(ctx, lock);
        func(ctx, priv);
        task_ctx_put(ctx);
        ctx = next_ctx;
    }
}

/* interface */
static inline void task_ctx_for_each_safe(struct task_ctx_domain *domain,
    void *priv, void (*func)(struct task_ctx *ctx, void *priv))
{
    u32 bkt;

    for (bkt = 0; bkt < APM_CTX_HASH_MAX; bkt++) {
        _task_ctx_for_each_safe(&domain->ctx_htable[bkt], &domain->ctx_lock[bkt], priv, func);
    }
}

static inline void task_ctx_item_show(struct task_ctx *ctx, void *priv)
{
    struct seq_file *seq = (struct seq_file *)priv;
    seq_printf(seq, "    name: %s tgid %d ref %d\n", ctx->name, ctx->tgid, kref_safe_read(&ctx->ref));
}

/* interface */
static inline void task_ctx_domain_show(struct task_ctx_domain *domain, struct seq_file *seq)
{
    seq_printf(seq, "domain: %s task_num %d task_max_num %u\n",
        domain->name, ka_base_atomic_read(&domain->task_num), domain->task_max_num);

    task_ctx_for_each_safe(domain, seq, task_ctx_item_show);
    seq_printf(seq, "\n");
}

/* interface */
static inline int task_ctx_lock_call_func(struct task_ctx_domain *domain, int tgid,
    int (*func)(struct task_ctx *ctx, void *para), void *para)
{
    int ret = -EINVAL;
    struct task_ctx *ctx = task_ctx_get(domain, tgid);
    if (ctx != NULL) {
        ka_task_mutex_lock(&ctx->mutex);
        ret = func(ctx, para);
        ka_task_mutex_unlock(&ctx->mutex);
        task_ctx_put(ctx);
    }

    return ret;
}

static inline struct task_struct *_task_get_by_tgid(int tgid)
{
#ifndef EMU_ST
    struct pid *pid = NULL;
    struct task_struct *task = NULL;

    pid = ka_task_get_pid(find_pid_ns(tgid, &init_pid_ns));
    if (pid == NULL) {
        return NULL;
    }

    task = ka_task_get_pid_task(pid, PIDTYPE_PID);
    put_pid(pid);

    return task;
#else
    return ka_task_get_current();
#endif
}

static inline struct task_struct *task_get_by_tgid(int tgid)
{
    struct task_struct *task = NULL;

    ka_task_rcu_read_lock();
    task = _task_get_by_tgid(tgid);
    ka_task_rcu_read_unlock();

    return task;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
static inline bool task_start_time_equal(u64 task_start_time, u64 start_time)
{
    return (task_start_time == start_time);
}

static inline bool task_is_dead(int tgid, u64 start_time)
#else
static inline bool task_start_time_equal(struct timespec task_start_time, struct timespec start_time)
{
    return timespec_equal(&task_start_time, &start_time);
}

static inline bool task_is_dead(int tgid, struct timespec start_time)
#endif
{
    bool is_dead = true;
    struct task_struct *task = task_get_by_tgid(tgid);
    if (task != NULL) {
        is_dead = !(task_start_time_equal(task->start_time, start_time));
        put_task_struct(task);
    }

    return is_dead;
}

static inline int task_get_tgid_by_vpid(int vpid, int *tgid)
{
    struct task_struct *task = ka_task_get_pid_task(find_vpid(vpid), PIDTYPE_PID);
    if (task == NULL) {
        return -EINVAL;
    }

    *tgid = (int)task_tgid_nr(task);
    put_task_struct(task);
    return 0;
}

static inline int task_get_parent_tgid_by_vpid(int vpid, int *tgid)
{
    struct task_struct *task = ka_task_get_pid_task(find_vpid(vpid), PIDTYPE_PID);
    if (task == NULL) {
        return -EINVAL;
    }

    *tgid = (int)task->real_parent->tgid;
    put_task_struct(task);
    return 0;
}

#endif


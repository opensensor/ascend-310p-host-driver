/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-11-11
 */

#ifndef KA_TASK_PUB_H
#define KA_TASK_PUB_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/cred.h>
#include <linux/nsproxy.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#endif
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rwlock.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#include <linux/swait.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
#include <linux/wait_bit.h>
#else
#include <linux/wait.h>
#endif
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/pid.h>
#include <linux/cgroup.h>
#include <linux/cpumask.h>
#include <linux/poll.h>
#include <linux/rcupdate.h>
#include <linux/irq.h>

#include "ka_macro_pub.h"
#include "ka_common_pub.h"

#define KA_TASK_PF_KTHREAD               PF_KTHREAD
#define KA_TASK_WQ_FLAG_EXCLUSIVE        WQ_FLAG_EXCLUSIVE
#define KA_TASK_MAX_SCHEDULE_TIMEOUT     MAX_SCHEDULE_TIMEOUT
#define KA_TASK_NORMAL                   TASK_NORMAL
#define KA_TASK_INTERRUPTIBLE            TASK_INTERRUPTIBLE
#define KA_TASK_KILLABLE                 TASK_KILLABLE

typedef enum pid_type  ka_pid_type_t;          /* defined in include/linux/pid.h */
#define KA_PIDTYPE_PID    PIDTYPE_PID
#define KA_PIDTYPE_TGID   PIDTYPE_TGID
#define KA_PIDTYPE_PGID   PIDTYPE_PGID
#define KA_PIDTYPE_SID    PIDTYPE_SID
#define KA_PIDTYPE_MAX    PIDTYPE_MAX

#define ka_pid_t pid_t
typedef struct pid ka_struct_pid_t;     /* defined in include/linux/pid.h */
typedef struct nsproxy ka_nsproxy_t;
typedef struct mnt_namespace ka_mnt_namespace_t;
typedef struct cgroup_namespace ka_cgroup_namespace_t;
typedef struct pid_namespace ka_pid_namespace_t;
typedef struct cgroup_subsys_state ka_cgroup_subsys_state_t;
typedef struct cgroup ka_cgroup_t;
typedef struct workqueue_struct ka_workqueue_struct_t;
typedef struct work_struct ka_work_struct_t;
typedef struct delayed_work ka_delayed_work_t;
typedef struct completion ka_completion_t;
typedef struct wait_queue_entry ka_wait_queue_entry_t;
typedef struct mutex ka_mutex_t;
#define ka_rwlock_t rwlock_t
typedef struct raw_spinlock ka_raw_spinlock_t;
typedef struct spinlock ka_spinlock_t;
typedef struct semaphore ka_semaphore_t;
typedef struct cred ka_cred_t;
typedef struct mm_struct  ka_mm_struct_t;
typedef struct timer_list  ka_timer_list_t;
typedef struct hlist_head  ka_hlist_head_t;

#define ka_wait_queue_head_t    wait_queue_head_t
#define KA_TASK_INIT_DELAYED_WORK(_work, _func)       INIT_DELAYED_WORK(_work, _func)
#define KA_TASK_INIT_WORK(_work, _func)    INIT_WORK(_work, _func)
#define ka_task_kthread_run       kthread_run
#define ka_task_kthread_create    kthread_create
#define ka_task_kthread_should_stop    kthread_should_stop
#define ka_task_kthread_bind(p, cpu)    kthread_bind(p, cpu)
#define ka_task_kthread_stop(p)    kthread_stop(p)
#define ka_task_task_pid_nr_ns(task, ns)  task_pid_nr_ns(task, ns)
#define ka_task_lockdep_tasklist_lock_is_held() lockdep_tasklist_lock_is_held()
#define ka_task_task_pid_vnr(task) task_pid_vnr(task)
#define ka_task_get_task_struct(task)    get_task_struct(task)
#define ka_task_cancel_work_sync(work)   cancel_work_sync(work)
#define ka_task_alloc_workqueue    alloc_workqueue
#define ka_task_create_workqueue(name)    create_workqueue(name)
#define ka_task_destroy_workqueue(wq)    destroy_workqueue(wq)
#define ka_task_create_singlethread_workqueue(name)    create_singlethread_workqueue(name)
#define ka_task_schedule_work(work)    schedule_work(work)
#define ka_task_queue_work(wq, work)   queue_work(wq, work)
#define ka_task_queue_work_on(cpu, wq, work)    queue_work_on(cpu, wq, work)
#define ka_task_work_pending(work)    work_pending(work)
#define ka_task_queue_delayed_work_on(cpu, wq, dwork, delay)    queue_delayed_work_on(cpu, wq, dwork, delay)
#define ka_task_flush_workqueue(wq)    flush_workqueue(wq)
#define ka_task_flush_delayed_work(dwork)    flush_delayed_work(dwork)
#define ka_task_cancel_delayed_work(dwork)    cancel_delayed_work(dwork)
#define ka_task_cancel_delayed_work_sync(dwork)    cancel_delayed_work_sync(dwork)
#define ka_task_delayed_work_pending(work)    delayed_work_pending(work)
#define ka_task_complete(x)    complete(x)
#define ka_task_complete_all(x)    complete_all(x)
#define ka_task_wait_for_completion(x)    wait_for_completion(x)
#define ka_task_wait_for_completion_timeout(x, timeout)    wait_for_completion_timeout(x, timeout)
#define ka_task_wait_for_completion_io(x)    wait_for_completion_io(x)
#define ka_task_wait_for_completion_io_timeout(x, timeout)    wait_for_completion_io_timeout(x, timeout)
#define ka_task_wait_for_completion_interruptible(x)    wait_for_completion_interruptible(x)
#define ka_task_wait_for_completion_interruptible_timeout(x, timeout)    wait_for_completion_interruptible_timeout(x, timeout)
#define ka_task_wait_for_completion_killable(x)    wait_for_completion_killable(x)
#define ka_task_wait_for_completion_killable_timeout(x, timeout)    wait_for_completion_killable_timeout(x, timeout)
#define ka_task_completion_done(x)    completion_done(x)
#define ka_task_wake_up_process(tsk)    wake_up_process(tsk)
#define ka_task_schedule()               schedule()
#define ka_task_schedule_timeout(timeout)    schedule_timeout(timeout)
#define ka_task_cond_resched()                 cond_resched()
#define ka_task_schedule_delayed_work_on(cpu, dwork, delay)    schedule_delayed_work_on(cpu, dwork, delay)
#define ka_task_schedule_delayed_work(work, delay)    schedule_delayed_work(work, delay)
#define ka_task_init_waitqueue_head(wq_head)      init_waitqueue_head(wq_head)
#define ka_task_waitqueue_active(wq_head)    waitqueue_active(wq_head)
#define ka_task_init_wait_entry(wq_entry, flags)    init_wait_entry(wq_entry, flags)
#define ka_task_prepare_to_wait_event(wq_head, wq_entry, state)    prepare_to_wait_event(wq_head, wq_entry, state)
#define ka_task_finish_wait(wq_head, wq_entry)    finish_wait(wq_head, wq_entry)
#define ka_task_autoremove_wake_function(wq_entry, mode, sync, key)  \
            autoremove_wake_function(wq_entry, mode, sync, key)
#define ka_task_down_read(sem)    down_read(sem)
#define ka_task_down_read_killable(sem)    down_read_killable(sem)
#define ka_task_down_read_trylock(sem)    down_read_trylock(sem)
#define ka_task_down_write(sem)    down_write(sem)
#define ka_task_down_write_killable(sem)    down_write_killable(sem)
#define ka_task_down_write_trylock(sem)    down_write_trylock(sem)
#define ka_task_up_write(sem)    up_write(sem)
#define ka_task_up_read(sem)    up_read(sem)
#define ka_task_downgrade_write(sem)    downgrade_write(sem)
#define ka_task_down_read_nested(sem, subclass)    down_read_nested(sem, subclass)
#define ka_task_down_read_non_owner(sem)    down_read_non_owner(sem)
#define ka_task_down_write_nested(sem, subclass)    down_write_nested(sem, subclass)
#define ka_task_down_write_killable_nested(sem, subclass)    down_write_killable_nested(sem, subclass)
#define ka_task_up_read_non_owner(sem)    up_read_non_owner(sem)
#define KA_TASK_DEFINE_MUTEX(mutexname) DEFINE_MUTEX(mutexname)
#define KA_TASK_DEFINE_RWLOCK(rw_lock)  DEFINE_RWLOCK(rw_lock)
#define ka_task_mutex_init(mutex)    mutex_init(mutex)
#define ka_task_mutex_destroy(mutex)    mutex_destroy(mutex)
#define ka_task_mutex_lock(mutex)    mutex_lock(mutex)
#define ka_task_mutex_unlock(mutex)    mutex_unlock(mutex)
#define ka_task_mutex_lock_interruptible(mutex)    mutex_lock_interruptible(mutex)
#define ka_task_mutex_trylock(mutex)    mutex_trylock(mutex)
#define ka_task_mutex_lock_killable(mutex)    mutex_lock_killable(mutex)
#define ka_task_spinlock_t spinlock_t
#define KA_TASK_DEFINE_SPINLOCK(x) DEFINE_SPINLOCK(x)
#define ka_task_spin_lock(lock)    spin_lock(lock)
#define ka_task_spin_lock_bh(lock)    spin_lock_bh(lock)
#define ka_task_spin_lock_nested(lock, subclass)     spin_lock_nested(lock, subclass)
#define ka_task_spin_lock_nest_lock(lock, subclass)    spin_lock_nest_lock(lock, subclass)
#define ka_task_spin_lock_irq(lock)    spin_lock_irq(lock)
#define ka_task_spin_lock_irqsave(lock, flags)    spin_lock_irqsave(lock, flags)
#define ka_task_spin_lock_irqsave_nested(lock, flags, subclass)    spin_lock_irqsave_nested(lock, flags, subclass)
#define ka_task_spin_unlock(lock)    spin_unlock(lock)
#define ka_task_spin_unlock_bh(lock)    spin_unlock_bh(lock)
#define ka_task_spin_unlock_irq(lock)    spin_unlock_irq(lock)
#define ka_task_spin_unlock_irqrestore(lock, flags)    spin_unlock_irqrestore(lock, flags)
#define ka_task_spin_trylock(lock)         spin_trylock(lock)
#define ka_task_spin_trylock_bh(lock)      spin_trylock_bh(lock)
#define ka_task_spin_trylock_irq(lock)     spin_trylock_irq(lock)
#define ka_task_spin_trylock_irqsave(lock, flags)    spin_trylock_irqsave(lock, flags)
#define ka_task_sema_init(sem, val)    sema_init(sem, val)
#define ka_task_down(sem)    down(sem)
#define ka_task_down_interruptible(sem)    down_interruptible(sem)
#define ka_task_down_killable(sem)    down_killable(sem)
#define ka_task_down_trylock(sem)    down_trylock(sem)
#define ka_task_down_timeout(sem, jiffies)    down_timeout(sem, jiffies)
#define ka_task_up(sem)    up(sem)
#define ka_task_clk_get_sys(dev_id, con_id)    clk_get_sys(dev_id, con_id)
#define ka_task_clk_get(dev, id)    clk_get(dev, id)
#define ka_task_clk_put(clk)    clk_put(clk)
#define ka_task_get_task_pid(task)    get_task_pid(task)
ka_pid_namespace_t *ka_task_get_init_pid_ns_addr(void);
#define ka_task_find_pid_ns(nr, ns)    find_pid_ns(nr, ns)
#define ka_task_find_get_pid(nr)    find_get_pid(nr)
#define ka_task_find_vpid(nr)    find_vpid(nr)
#define ka_task_get_pid(pid)    get_pid(pid)
#define ka_task_put_pid(pid)    put_pid(pid)
#define ka_task_get_pid_task(pid, type)    get_pid_task(pid, type)
#define ka_task_put_task_struct(t)    put_task_struct(t)
#define ka_task_schedule_work_on(cpu, work)    schedule_work_on(cpu, work)
#define ka_task_next_task(p)    next_task(p)
#define ka_task_task_get_css(task, subsys_id)    task_get_css(task, subsys_id)
#define ka_task_css_put(css)    css_put(css)
#define ka_task_cgroup_path_ns(cgrp, buf, buflen, ns)    cgroup_path_ns(cgrp, buf, buflen, ns)
#define ka_task_poll_wait(filp, wait_address, p)    poll_wait(filp, wait_address, p)
#define ka_task_need_resched()    need_resched()
#define ka_task_sched_setscheduler(p, policy, param)    sched_setscheduler(p, policy, param)
#define ka_task_sched_set_fifo_low(p)    sched_set_fifo_low(p)
#define ka_task_rcu_read_lock()    rcu_read_lock()
#define ka_task_rcu_read_unlock()    rcu_read_unlock()
#define ka_task_rcu_read_lock_bh()    rcu_read_lock_bh()
#define ka_task_rcu_read_unlock_bh()    rcu_read_unlock_bh()
#define ka_task_signal_pending_state(state, p)    signal_pending_state(state, p)
#define ka_task_local_irq_save(flags)    local_irq_save(flags)
#define ka_task_local_irq_restore(flags)    local_irq_restore(flags)
#define ka_task_might_sleep()    might_sleep()
#define ka_task_get_task_mm(task)       get_task_mm(task)
#define ka_task_sched_setscheduler(p, policy, param)  sched_setscheduler(p, policy, param)
#define ka_task_task_get_css(task, subsys_id)       task_get_css(task, subsys_id)
#define ka_task_prepare_to_wait_exclusive(wq_head, wq_entry, state) prepare_to_wait_exclusive(wq_head, wq_entry, state)
#define ka_task_rcu_dereference_bh_check(p, c)    rcu_dereference_bh_check(p, c)
#define KA_TASK_DEFINE_WAIT(name)   DEFINE_WAIT(name)

static inline ka_workqueue_struct_t *ka_task_system_wq(void)
{
    return system_wq;
}

static inline ka_hlist_head_t *ka_task_get_pid_tasks(ka_struct_pid_t *pid, ka_pid_type_t type)
{
    return &(pid->tasks[type]);
}

static inline ka_mm_struct_t *ka_task_get_mm(ka_task_struct_t *task)
{
    return task->mm;
}

void ka_task_do_exit(long code);
ka_mnt_namespace_t *ka_task_get_mnt_ns(ka_task_struct_t *task);
ka_nsproxy_t *ka_task_get_nsproxy(ka_task_struct_t *task);

#define ka_task_wake_up(x)                        wake_up(x)
#define ka_task_wake_up_nr(x, nr)                 wake_up_nr(x, nr)
#define ka_task_wake_up_all(x)                    wake_up_all(x)

#define ka_task_wake_up_interruptible(x)          wake_up_interruptible(x)
#define ka_task_wake_up_interruptible_nr(x, nr)   wake_up_interruptible_nr(x, nr)
#define ka_task_wake_up_interruptible_all(x)      wake_up_interruptible_all(x)
#define ka_task_init_completion(x)                init_completion(x)
#define ka_task_init_rwsem(sem)                   init_rwsem(sem)
#define ka_task_spin_lock_init(lock) spin_lock_init(lock)

u64 ka_task_get_start_boottime(ka_task_struct_t *task);
ka_fs_struct_t *ka_task_get_task_fs(ka_task_struct_t *task);
int ka_task_get_tgid_for_task(ka_task_struct_t *task);
int ka_task_get_pid_for_task(ka_task_struct_t *task);
ka_mm_struct_t *ka_task_get_mm_for_task(ka_task_struct_t *task);
ka_pid_namespace_t *ka_task_get_pid_ns_for_task(ka_task_struct_t *task);
ka_task_struct_t *ka_task_get_current(void);
char *ka_task_get_current_comm(void);
int ka_task_get_current_tgid(void);
int ka_task_get_current_pid(void);
ka_mm_struct_t *ka_task_get_current_mm(void);
unsigned long ka_task_get_current_get_unmapped_area(ka_file_t *filep,
        unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags);
ka_mm_struct_t *ka_task_get_current_active_mm(void);

ka_mem_cgroup_t *ka_task_get_current_active_memcg(void);
void ka_task_set_current_active_memcg(ka_mem_cgroup_t *memcg);
u64 ka_task_get_current_starttime(void);
u64 ka_task_get_current_group_starttime(void);
const ka_cred_t *ka_task_get_current_cred(void);
int ka_task_get_current_cred_euid(void);
int ka_task_get_current_cred_uid(void);
u64 ka_task_get_current_parent_tgid(void);
u64 ka_task_get_current_start_boottime(void);
ka_nsproxy_t *ka_task_get_current_nsproxy(void);
ka_mnt_namespace_t *ka_task_get_current_mnt_ns(void);
ka_cgroup_namespace_t *ka_task_get_current_cgroup_ns(void);
ka_pid_namespace_t *ka_task_get_current_pid_ns(void);
ka_task_struct_t *ka_task_get_init_task(void);
unsigned int ka_task_get_current_flags(void);
u64 ka_task_get_starttime(ka_task_struct_t *task);
unsigned int ka_task_get_cred_uid_val(const ka_cred_t *cred);

#define ka_task_rwlock_init(lock)                       rwlock_init(lock)
#define ka_task_read_lock(lock)                         read_lock(lock)
#define ka_task_read_lock_bh(lock)                      read_lock_bh(lock)
#define ka_task_read_lock_irq(lock)                     read_lock_irq(lock)
#define ka_task_read_lock_irqsave(lock, flags)          read_lock_irqsave(lock, flags)
#define ka_task_read_unlock(lock)                       read_unlock(lock)
#define ka_task_read_unlock_bh(lock)                    read_unlock_bh(lock)
#define ka_task_read_unlock_irq(lock)                   read_unlock_irq(lock)
#define ka_task_read_unlock_irqrestore(lock, flags)     read_unlock_irqrestore(lock, flags)
#define ka_task_write_lock(lock)                        write_lock(lock)
#define ka_task_write_lock_bh(lock)                     write_lock_bh(lock)
#define ka_task_write_lock_irq(lock)                    write_lock_irq(lock)
#define ka_task_write_lock_irqsave(lock, flags)         write_lock_irqsave(lock, flags)
#define ka_task_write_unlock(lock)                      write_unlock(lock)
#define ka_task_write_unlock_bh(lock)                   write_unlock_bh(lock)
#define ka_task_write_unlock_irq(lock)                  write_unlock_irq(lock)
#define ka_task_write_unlock_irqrestore(lock, flags)    write_unlock_irqrestore(lock, flags)

#define ka_task_read_trylock(lock)                      read_trylock(lock)
#define ka_task_write_trylock(lock)                     write_trylock(lock)
#define ka_for_each_process(p)                          for_each_process(p)

#define ka_task_wait_event_interruptible_exclusive(wq, condition)   \
            wait_event_interruptible_exclusive(wq, condition)
#define ka_task_wait_event_interruptible_timeout(wq_head, condition, timeout)    \
            wait_event_interruptible_timeout(wq_head, condition, timeout)

#define ka_task_wait_event_timeout(wq_head, condition, timeout)   \
            wait_event_timeout(wq_head, condition, timeout)

#define ka_task_wait_event_interruptible(wq_head, condition)         \
            wait_event_interruptible(wq_head, condition)

#define ka_task_wait_event_interruptible_lock_irq(wq_head, condition, lock)  \
                wait_event_interruptible_lock_irq(wq_head, condition, lock)

#define ka_task_wait_event_interruptible_lock_irq_timeout(wq_head, condition, lock, timeout)  \
                wait_event_interruptible_lock_irq_timeout(wq_head, condition, lock, timeout)

#endif

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

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/nsproxy.h>
#include <linux/sched.h>
#include <linux/rbtree.h>
#include <linux/namei.h>
#include <linux/dcache.h>
#include <linux/cred.h>
#include <linux/pid_namespace.h>
#include <linux/cpuset.h>
#endif
#include "devdrv_manager.h"
#include "devdrv_manager_container.h"
#include "devdrv_manager_container_noplugin.h"
#include "devdrv_manager_comm.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_manager_dev_share.h"
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#endif

#define DEVMNG_CONTAINER_DEVICE_CONFLICT 1
#define DEVMNG_CONTAINER_DEVICE_NORMAL 0

struct devdrv_container_table {
    struct list_head item_list_head;
    spinlock_t spinlock;
};

struct devdrv_container_main_pid {
    pid_t tgid;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
    u64 start_time;
    u64 real_start_time;
#else
    struct timespec start_time;
    struct timespec real_start_time;
#endif
};

struct devdrv_container_item {
    struct list_head item_list_node;
    struct devdrv_container_main_pid main_pid;

    u32 id;
    u32 conflict_flag;
    u32 dev_num;
    u32 dev_id[DEVDRV_MAX_DAVINCI_NUM];
    u64 dev_list_map[DEVDRV_CONTIANER_NUM_OF_LONG];
    u64 container_id;

    u64 mnt_ns;
    struct pid_namespace *pid_ns;
    spinlock_t spinlock;
};

enum docker_status_type {
    DOCKER_STATUS_IDLE = 0,
    DOCKER_STATUS_USED
};

extern struct devdrv_manager_info *dev_manager_info;
STATIC int docker_status[MAX_DOCKER_NUM] = {DOCKER_STATUS_IDLE};

STATIC int devdrv_manager_container_init_docker_id(struct devdrv_container_item *item)
{
    u32 i;

    for (i = 0; i < MAX_DOCKER_NUM; i++) {
        if (docker_status[i] == DOCKER_STATUS_IDLE) {
            item->id = i;
            docker_status[i] = DOCKER_STATUS_USED;
            break;
        }
    }

    if (i >= MAX_DOCKER_NUM) {
        devdrv_drv_err_spinlock("no idle docker, i = %d\n", i);
        return -EBUSY;
    }
    return 0;
}

STATIC void devdrv_manager_container_uninit_docker_id(struct devdrv_container_item *item)
{
    if (item->id >= MAX_DOCKER_NUM) {
        devdrv_drv_err("item id is invalid, id = %u\n", item->id);
        return;
    }
    docker_status[item->id] = DOCKER_STATUS_IDLE;
    item->id = MAX_DOCKER_NUM;
}

int devdrv_manager_container_task_struct_check(struct task_struct *tsk)
{
    if (tsk == NULL) {
        devdrv_drv_err("tsk is NULL\n");
        return -EINVAL;
    }
#ifdef CONFIG_ARM64
    if (tsk->fs == NULL) {
        devdrv_drv_err("tsk->fs is NULL\n");
        return -EINVAL;
    }
#endif
    if (tsk->nsproxy == NULL) {
        devdrv_drv_err("tsk->nsproxy is NULL\n");
        return -ENODEV;
    }
    if (tsk->nsproxy->mnt_ns == NULL) {
        devdrv_drv_err("tsk->nsproxy->mnt_ns is NULL\n");
        return -EFAULT;
    }
    return 0;
}

int devdrv_manager_get_current_mnt_ns(u64 *current_mnt_ns)
{
    int ret;

    ret = devdrv_manager_container_task_struct_check(current);
    if (ret) {
        devdrv_drv_err("current is invalid, ret=%d\n", ret);
        return -EINVAL;
    }

    *current_mnt_ns = (u64)(uintptr_t)current->nsproxy->mnt_ns;
    return 0;
}

u64 devdrv_manager_get_host_mnt_ns(void)
{
    return (u64)(uintptr_t)init_task.nsproxy->mnt_ns;
}

u32 devmng_run_mode = DEVDRV_NORMAL_MODE;

int devdrv_is_in_assignment(void)
{
    if (devmng_run_mode == DEVDRV_NORMAL_MODE) {
        return 0;
    } else {
        return 1;
    }
}

STATIC bool devdrv_manager_container_is_admin_task(struct task_struct *tsk)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
    kernel_cap_t privileged = (kernel_cap_t){{ ~0, (CAP_TO_MASK(CAP_AUDIT_READ + 1) -1)}};
#else
    kernel_cap_t privileged = CAP_FULL_SET;
#endif
    const struct cred *cred = NULL;
    kernel_cap_t effective;
    unsigned int i, user_id;

    rcu_read_lock();
    cred = __task_cred(tsk); //lint !e1058 !e64 !e666
    effective = cred->cap_effective;
    user_id = cred->uid.val;
    rcu_read_unlock();

    CAP_FOR_EACH_U32(i)
    {
        if ((effective.cap[i] & privileged.cap[i]) != privileged.cap[i]) {
            return false;
        }
    }
    if (user_id == 0) {
        return true;
    } else {
        return false;
    }
}

bool devdrv_manager_container_is_admin(void)
{
    return devdrv_manager_container_is_admin_task(current);
}

int devdrv_manager_container_is_host_system(u64 mnt_ns)
{
    if ((mnt_ns == devdrv_manager_get_host_mnt_ns()) || devdrv_manager_container_is_admin()) {
        return true;
    }
    return false;
}
EXPORT_SYMBOL(devdrv_manager_container_is_host_system);

int devdrv_manager_container_is_host_system_ns(struct mnt_namespace *para_mnt_ns)
{
    u64 mnt_ns = 0;
    int ret = 0;
    ret = devdrv_manager_get_current_mnt_ns(&mnt_ns);
    if (ret < 0) {
        devdrv_drv_err("get mnt ns failed, ret=%d\n", ret);
        return -EINVAL;
    }

    if (mnt_ns == devdrv_manager_get_host_mnt_ns()) {
        return true;
    }

    return false;
}
EXPORT_SYMBOL(devdrv_manager_container_is_host_system_ns);

STATIC void devdrv_manager_container_print_list(struct devdrv_container_table *dev_container_table)
{
    struct devdrv_container_item *item = NULL;
    struct list_head *pos = NULL, *n = NULL;

    if (list_empty_careful(&dev_container_table->item_list_head)) {
        return;
    }

    list_for_each_safe(pos, n, &dev_container_table->item_list_head)
    {
        item = list_entry(pos, struct devdrv_container_item, item_list_node);
    }
}

STATIC inline struct pid_namespace *devdrv_manager_container_get_pid_ns(struct task_struct *tsk)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    return tsk->nsproxy->pid_ns_for_children;
#else
    return tsk->nsproxy->pid_ns;
#endif
}

STATIC struct task_struct *devdrv_manager_container_get_main_proc_by_first_pid(struct pid_namespace *pid_ns)
{
    struct task_struct *tsk = NULL;
    struct pid *pro_id = NULL;

    rcu_read_lock();
    pro_id = get_pid(find_pid_ns(1, pid_ns));
    rcu_read_unlock();
    if (pro_id == NULL) {
        return NULL;
    }

    tsk = get_pid_task(pro_id, PIDTYPE_PID);
    put_pid(pro_id);

    /* 1st pid equal to main pid */
    if (tsk == NULL) {
        return NULL;
    }

    if (tsk->nsproxy == NULL) {
        put_task_struct(tsk);
        return NULL;
    }

    if (tsk->nsproxy->mnt_ns != current->nsproxy->mnt_ns) {
        put_task_struct(tsk);
        return NULL;
    }

    return tsk;
}

STATIC struct task_struct *devdrv_manager_container_get_main_proc_by_task_list(void)
{
    struct task_struct *tsk = NULL;
    struct task_struct *p = NULL;

    rcu_read_lock();
    for_each_process(p)
    {
        if ((p != NULL) && (p->nsproxy != NULL) && (p->nsproxy->mnt_ns == current->nsproxy->mnt_ns)) {
            tsk = p;
            get_task_struct(tsk);
            break;
        }
    }
    rcu_read_unlock();

    return tsk;
}

STATIC int devdrv_manager_container_find_main_proc(pid_t *nr, pid_t *vnr)
{
    struct task_struct *main_tsk = NULL;
    struct pid_namespace *pid_ns = NULL;

    pid_ns = task_active_pid_ns(current);
    if (pid_ns == NULL) {
        devdrv_drv_err_spinlock("find current pid ns failed.\n");
        return -EINVAL;
    }

    main_tsk = devdrv_manager_container_get_main_proc_by_first_pid(pid_ns);
    if (main_tsk == NULL) {
        main_tsk = devdrv_manager_container_get_main_proc_by_task_list();
        if (main_tsk == NULL) {
            devdrv_drv_err_spinlock("find main pid failed, current mnt_ns(0x%pK),"
                " pid_ns(0x%pK), current pid_ns(0x%pK).\n",
                (void *)(uintptr_t)current->nsproxy->mnt_ns, (void *)(uintptr_t)pid_ns,
                (void *)(uintptr_t)devdrv_manager_container_get_pid_ns(current));
            return -EINVAL;
        }
    }

    *nr = task_tgid_nr(main_tsk);
    *vnr = task_pid_vnr(main_tsk);
    put_task_struct(main_tsk);

    return 0;
}

STATIC struct devdrv_container_item *devdrv_manager_container_item_init(
    struct devdrv_container_table *dev_container_table, struct devdrv_manager_container_para *para)
{
    struct devdrv_container_item *item = NULL;
    struct task_struct *task = NULL;
    pid_t main_proc, vnr;
    struct pid *pro_id = NULL;
    u32 i;

    if (devdrv_manager_container_find_main_proc(&main_proc, &vnr)) {
        devdrv_drv_err_spinlock("find_main_proc failed\n");
        return NULL;
    }

    rcu_read_lock();
    pro_id = get_pid(find_pid_ns(main_proc, &init_pid_ns));
    rcu_read_unlock();
    if (pro_id == NULL) {
        devdrv_drv_err_spinlock("find_pid_ns failed.\n");
        return NULL;
    }

    task = get_pid_task(pro_id, PIDTYPE_PID);
    put_pid(pro_id);
    if (task == NULL) {
        devdrv_drv_err_spinlock("get task is NULL, main_proc(%d), vnr(%d)"
            "init_pid_ns(0x%pK) mnt_ns(0x%pK) pid_ns(0x%pK)\n",
            main_proc, vnr, (void *)(uintptr_t)&init_pid_ns,
            (void *)(uintptr_t)para->mnt_ns, (void *)(uintptr_t)para->pid_ns);
        return NULL;
    }

    item = dbl_kzalloc(sizeof(struct devdrv_container_item), GFP_ATOMIC | __GFP_ACCOUNT);
    if (item == NULL) {
        put_task_struct(task);
        devdrv_drv_err_spinlock("item kzalloc failed\n");
        return NULL;
    }

    for (i = 0; i < DEVDRV_CONTIANER_NUM_OF_LONG; i++) {
        item->dev_list_map[i] = 0;
    }
    item->dev_num = 0;
    item->conflict_flag = DEVMNG_CONTAINER_DEVICE_NORMAL;
    item->mnt_ns = para->mnt_ns;
    item->pid_ns = para->pid_ns;
    item->main_pid.tgid = main_proc;
    item->main_pid.start_time = task->start_time;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    item->main_pid.real_start_time = task->start_boottime;
#else
    item->main_pid.real_start_time = task->real_start_time;
#endif

    put_task_struct(task);

    spin_lock_init(&item->spinlock);
    list_add_tail(&item->item_list_node, &dev_container_table->item_list_head);

    return item;
}

STATIC void devdrv_manager_container_item_free(struct devdrv_container_item *item)
{
    devdrv_manager_container_uninit_docker_id(item);

    list_del(&item->item_list_node);
    dbl_kfree(item);
    item = NULL;
}

STATIC bool devdrv_manager_container_check_main_proc_timestamp(struct devdrv_container_item *item)
{
    struct task_struct *tsk = NULL;
    struct pid *pro_id = NULL;

    rcu_read_lock();
    pro_id = get_pid(find_pid_ns(item->main_pid.tgid, &init_pid_ns));
    rcu_read_unlock();
    if (pro_id == NULL) {
        return true;
    }

    tsk = get_pid_task(pro_id, PIDTYPE_PID);
    put_pid(pro_id);
    if (tsk == NULL) {
        return true;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
    if (item->main_pid.start_time == tsk->start_time) {
        put_task_struct(tsk);
        return false;
    }
#else
    if (timespec_equal(&item->main_pid.start_time, &tsk->start_time)) {
        put_task_struct(tsk);
        return false;
    }
#endif
    put_task_struct(tsk);

    return true;
}
#else
struct devdrv_container_item {
    u32 id;
    u32 conflict_flag;
    u32 dev_num;
    u64 container_id;
    u64 mnt_ns;
};
#endif

STATIC bool devdrv_manager_container_check_and_free_item(struct devdrv_container_item *item)
{
    if (item->mnt_ns == (u64)init_task.nsproxy->mnt_ns) {
        /* Host mnt_ns not need check and remover here. */
        return false;
    }
    if (devdrv_manager_container_check_main_proc_timestamp(item)) {
        devdrv_manager_container_item_free(item);
        return true;
    }
    return false;
}

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
STATIC bool devdrv_manager_container_check_and_free_table(struct devdrv_container_table *dev_container_table)
{
    struct devdrv_container_item *item = NULL;
    struct list_head *pos = NULL, *n = NULL;

    if (!list_empty_careful(&dev_container_table->item_list_head)) {
        list_for_each_safe(pos, n, &dev_container_table->item_list_head) {
            item = list_entry(pos, struct devdrv_container_item, item_list_node);
            if (devdrv_manager_container_check_and_free_item(item)) {
                return true;
            }
        }
    }
    return false;
}


STATIC struct devdrv_container_item *devdrv_manager_container_find_item(
    struct devdrv_container_table *dev_container_table, u64 mnt_ns)
{
    struct devdrv_container_item *item = NULL;
    struct list_head *pos = NULL, *n = NULL;

    if (!list_empty_careful(&dev_container_table->item_list_head)) {
        list_for_each_safe(pos, n, &dev_container_table->item_list_head)
        {
            item = list_entry(pos, struct devdrv_container_item, item_list_node);
            if (item->mnt_ns == mnt_ns) {
                return item;
            }
        }
    }

    return NULL;
}

STATIC void devdrv_manager_container_set_item_conflict(
    struct devdrv_container_table *dev_container_table, u64 mnt_ns)
{
    struct devdrv_container_item *item = NULL;
    struct list_head *pos = NULL, *n = NULL;
    u32 i;

    if (list_empty_careful(&dev_container_table->item_list_head)) {
        return;
    }

    list_for_each_safe(pos, n, &dev_container_table->item_list_head) {
        item = list_entry(pos, struct devdrv_container_item, item_list_node);
        if (item->mnt_ns == mnt_ns) {
            item->conflict_flag = DEVMNG_CONTAINER_DEVICE_CONFLICT;
            item->dev_num = 0;
            for (i = 0; i <  DEVDRV_MAX_DAVINCI_NUM; i++) {
                item->dev_id[i] = DEVDRV_MAX_DAVINCI_NUM;
            }
            for (i = 0; i < DEVDRV_CONTIANER_NUM_OF_LONG; i++) {
                item->dev_list_map[i] = 0x0;
            }
            return;
        }
    }
}

STATIC bool devdrv_manager_container_devid_check(struct devdrv_container_table *dev_container_table,
    struct devdrv_manager_container_para para)
{
    struct devdrv_container_item *item = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    u32 i, j;

    if (para.mnt_ns == devdrv_manager_get_host_mnt_ns()) {
        return true;
    }

    if (list_empty_careful(&dev_container_table->item_list_head)) {
        return false;
    }

    list_for_each_safe(pos, n, &dev_container_table->item_list_head) {
        item = list_entry(pos, struct devdrv_container_item, item_list_node);
        if ((item->mnt_ns == devdrv_manager_get_host_mnt_ns()) ||
            (item->mnt_ns == para.mnt_ns)) {
            continue;
        }
        if (item->conflict_flag == DEVMNG_CONTAINER_DEVICE_CONFLICT) {
            continue;
        }
        for (i = 0; i < item->dev_num; i++) {
            for (j = 0; j < para.dev_num; j++) {
                if (item->dev_id[i] == para.dev_id[j]) {
                    if (get_device_share_flag(item->dev_id[i]) == DEVICE_UNSHARE) {
                        devdrv_manager_container_set_item_conflict(dev_container_table, para.mnt_ns);
                        devdrv_drv_err("device[%u] = %u is in used, "
                                       "item_mntns(0x%pK) current_mntns(0x%pK)\n",
                                       i, item->dev_id[i], (void *)(uintptr_t)item->mnt_ns,
                                       (void *)(uintptr_t)current->nsproxy->mnt_ns);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

STATIC int devdrv_manager_container_table_add_new_item(struct devdrv_container_table *dev_container_table,
    struct devdrv_manager_container_para *para)
{
    struct devdrv_container_item *item = NULL;
    int ret;

    item = devdrv_manager_container_item_init(dev_container_table, para);
    if (item == NULL) {
        devdrv_drv_err_spinlock("init item failed\n");
        return -ENOMEM;
    }

    ret = devdrv_manager_container_init_docker_id(item);
    if (ret) {
        devdrv_manager_container_item_free(item);
        devdrv_drv_err_spinlock("init docker id failed, ret = %d\n", ret);
        return ret;
    }

    return 0;
}

STATIC int devdrv_manager_container_table_add_exist_item(struct devdrv_container_item *item,
    struct devdrv_manager_container_para *para)
{
    u32 index, offset, i;
    u32 dev_num = 0;

    if (para->dev_num > DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_err("invalid dev_num = %u\n", para->dev_num);
        return -EINVAL;
    }

    for (i = 0; i < para->dev_num; i++) {
        if (para->dev_id[i] >= DEVDRV_MAX_DAVINCI_NUM) {
            devdrv_drv_err("invalid deviceid, dev_id[%u] = %u\n", i, para->dev_id[i]);
            return -EINVAL;
        }
        index = para->dev_id[i] / BIT_NUM_U64;
        offset = para->dev_id[i] % BIT_NUM_U64;
        item->dev_list_map[index] |= (0x01ULL << offset);
    }

    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        item->dev_id[i] = DEVDRV_MAX_DAVINCI_NUM;
    }

    for (index = 0; index < DEVDRV_CONTIANER_NUM_OF_LONG; index++) {
        for (offset = 0; offset < BIT_NUM_U64; offset++) {
            if ((item->dev_list_map[index] & (0x1ULL << offset)) &&
                (dev_num < DEVDRV_MAX_DAVINCI_NUM)) {
                item->dev_id[dev_num] = BIT_NUM_U64 * index + offset;
                dev_num++;
            }
        }
    }
    item->dev_num = dev_num;

    return 0;
}

STATIC int devdrv_manager_container_table_add_devlist_to_item(
    struct devdrv_container_table *dev_container_table, struct devdrv_manager_container_para *para)
{
    struct devdrv_container_item *item = NULL;

    item = devdrv_manager_container_find_item(dev_container_table, para->mnt_ns);
    if (item == NULL) {
        devdrv_drv_err_spinlock("find item failed, para->mnt_ns = 0x%pK\n", (void *)(uintptr_t)para->mnt_ns);
        return -EINVAL;
    }

    return devdrv_manager_container_table_add_exist_item(item, para);
}

/* 恢复当前item的冲突位 */
STATIC void devdrv_manager_container_recover_conflict(
    struct devdrv_container_table *dev_container_table, struct devdrv_manager_container_para *para)
{
    struct devdrv_container_item *item = NULL;
    struct devdrv_container_item *cur_item = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    u32 i;

    cur_item = devdrv_manager_container_find_item(dev_container_table, para->mnt_ns);
    if (cur_item == NULL) {
        devdrv_drv_err("devdrv manager container can't find item.\n");
        return;
    }
    if (cur_item->conflict_flag != DEVMNG_CONTAINER_DEVICE_CONFLICT) {
        return;
    }
    list_for_each_safe(pos, n, &dev_container_table->item_list_head) {
        item = list_entry(pos, struct devdrv_container_item, item_list_node);
        if ((item->mnt_ns == devdrv_manager_get_host_mnt_ns()) ||
            (item->conflict_flag == DEVMNG_CONTAINER_DEVICE_CONFLICT)) {
            continue;
        }
        /* 通过list_map判断, 当前item中的device是否被占用 */
        for (i = 0; i < DEVDRV_CONTIANER_NUM_OF_LONG; i++) {
            if ((cur_item->dev_list_map[i] & item->dev_list_map[i]) != 0) {
                return;
            }
        }
    }
    cur_item->conflict_flag = DEVMNG_CONTAINER_DEVICE_NORMAL;
    devdrv_drv_info_spinlock("Devices in docker is not conflict. (docker_id=%d)\n", cur_item->id);
    return;
}

/* 1.删除无效容器 2.找到当前容器对应的item，找不到则添加item节点 3.解当前item的冲突标志 */
STATIC int devdrv_manager_container_table_overlap_item(
    struct devdrv_container_table *dev_container_table, struct devdrv_manager_container_para *para)
{
    struct devdrv_container_item *item = NULL;

    (void)devdrv_manager_container_check_and_free_table(dev_container_table);
    item = devdrv_manager_container_find_item(dev_container_table, para->mnt_ns);
    if (item == NULL) {
        return devdrv_manager_container_table_add_new_item(dev_container_table, para);
    }

    devdrv_manager_container_recover_conflict(dev_container_table, para);

    return 0;
}

int devdrv_manager_container_table_devlist_add_ns(u32 *physical_devlist, u32 physical_dev_num,
    struct mnt_namespace *mnt_ns)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_manager_info *manager_info = NULL;
    struct devdrv_manager_container_para para;
    int ret, ret_add_devlist;
    u32 i;

    /* privileged container not need to create item */
    if (devdrv_manager_container_is_admin() &&
        (u64)(uintptr_t)mnt_ns != devdrv_manager_get_host_mnt_ns()) {
        return 0;
    }

    manager_info = devdrv_get_manager_info();
    if ((manager_info == NULL) || (physical_dev_num > DEVDRV_MAX_DAVINCI_NUM) ||
        (physical_devlist == NULL)) {
        devdrv_drv_err("manager_info = 0x%pK, physical_devlist = 0x%pK, physical_dev_num = %d\n",
            (void *)(uintptr_t)manager_info, (void *)(uintptr_t)physical_devlist, physical_dev_num);
        return -EINVAL;
    }
    for (i = 0; i < physical_dev_num; i++) {
        para.dev_id[i] = physical_devlist[i];
    }
    para.dev_num = physical_dev_num;
    para.mnt_ns = (u64)(uintptr_t)mnt_ns;
    para.start_time = current->start_time;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    para.real_start_time = current->start_boottime;
#else
    para.real_start_time = current->real_start_time;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    para.pid_ns = current->nsproxy->pid_ns_for_children;
#else
    para.pid_ns = current->nsproxy->pid_ns;
#endif

    dev_container_table = manager_info->container_table;
    if (dev_container_table == NULL) {
        devdrv_drv_err("container table is NULL\n");
        return -ENOMEM;
    }
    spin_lock(&dev_container_table->spinlock);
    ret = devdrv_manager_container_table_overlap_item(dev_container_table, &para);
    if (ret) {
        devdrv_manager_container_print_list(dev_container_table);
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("add devlist to item failed, ret = %d\n", ret);
        return -ENODEV;
    }
    if (!devdrv_manager_container_devid_check(dev_container_table, para)) {
        devdrv_drv_err_spinlock("device is in used\n");
        ret = -EBUSY;
    }
    ret_add_devlist = devdrv_manager_container_table_add_devlist_to_item(dev_container_table, &para);
    if (ret_add_devlist) {
        devdrv_manager_container_print_list(dev_container_table);
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("add devlist to item failed, ret = %d\n", ret);
        return -ENODEV;
    }
    spin_unlock(&dev_container_table->spinlock);

    return ret;
}
EXPORT_SYMBOL(devdrv_manager_container_table_devlist_add_ns);

STATIC int devdrv_manager_container_table_del_exist_item(struct devdrv_container_item *item,
    struct devdrv_manager_container_para *para)
{
    u32 index, offset;
    u32 i;

    for (i = 0; i < para->dev_num; i++) {
        if (para->dev_id[i] >= DEVDRV_MAX_DAVINCI_NUM) {
            devdrv_drv_err_spinlock("invalid deviceid, dev_id[%u] = %u\n", i, para->dev_id[i]);
            return -EINVAL;
        }
        index = para->dev_id[i] / BIT_NUM_U64;
        offset = para->dev_id[i] % BIT_NUM_U64;
        if ((item->dev_list_map[index] & (0x01ULL << offset)) == 0) {
            devdrv_drv_err_spinlock("add devid failed, dev_id = %u exist already\n", para->dev_id[i]);
            return -ENODEV;
        }
        item->dev_list_map[index] &= ~(0x01ULL << offset);
    }
    item->dev_num -= para->dev_num;

    return 0;
}

STATIC int devdrv_manager_container_table_del_devlist_to_item(
    struct devdrv_container_table *dev_container_table, struct devdrv_manager_container_para *para)
{
    struct devdrv_container_item *item = NULL;

    item = devdrv_manager_container_find_item(dev_container_table, para->mnt_ns);
    if (item == NULL) {
        return -EINVAL;
    }

    return devdrv_manager_container_table_del_exist_item(item, para);
}

int devdrv_manager_container_table_devlist_del_ns(u32 *physical_devlist, u32 physical_dev_num,
    struct mnt_namespace *mnt_ns)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_manager_info *manager_info = NULL;
    struct devdrv_manager_container_para para;
    int ret;
    u32 i;

    manager_info = devdrv_get_manager_info();
    if (manager_info == NULL || physical_dev_num > DEVDRV_MAX_DAVINCI_NUM || physical_devlist == NULL) {
        devdrv_drv_err("manager_info = %pK, physical_devlist = %pK, physical_dev_num = %d\n",
                       manager_info, physical_devlist, physical_dev_num);
        return -EINVAL;
    }
    for (i = 0; i < physical_dev_num; i++) {
        para.dev_id[i] = physical_devlist[i];
    }
    para.dev_num = physical_dev_num;
    para.mnt_ns = (u64)(uintptr_t)mnt_ns;
    para.start_time = current->start_time;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    para.real_start_time = current->start_boottime;
#else
    para.real_start_time = current->real_start_time;
#endif

    dev_container_table = manager_info->container_table;
    if (dev_container_table == NULL) {
        devdrv_drv_err("container table is NULL\n");
        return -ENOMEM;
    }
    spin_lock(&dev_container_table->spinlock);
    ret = devdrv_manager_container_table_del_devlist_to_item(dev_container_table, &para);
    if (ret) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("add devlist to item failed, ret = %d\n", ret);
        return -ENODEV;
    }
    spin_unlock(&dev_container_table->spinlock);

    return 0;
}

int devdrv_manager_container_table_init(struct devdrv_manager_info *manager_info)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_manager_container_para para;
    struct devdrv_container_item *item = NULL;

    if (manager_info == NULL) {
        devdrv_drv_err("manager info or tsk is null\n");
        return -ENOMEM;
    }
    dev_container_table = dbl_kzalloc(sizeof(struct devdrv_container_table), GFP_KERNEL | __GFP_ACCOUNT);
    if (dev_container_table == NULL) {
        devdrv_drv_err("container_table kzalloc failed\n");
        return -ENOMEM;
    }
    INIT_LIST_HEAD(&dev_container_table->item_list_head);
    spin_lock_init(&dev_container_table->spinlock);

    para.mnt_ns = devdrv_manager_get_host_mnt_ns();
    para.pid_ns = &init_pid_ns;
    devdrv_drv_info("container table init: host_mnt_ns(0x%pK) host_pid_ns(0x%pK)\n",
                    (void *)(uintptr_t)para.mnt_ns, (void *)(uintptr_t)&init_pid_ns);

    /* create host item */
    spin_lock(&dev_container_table->spinlock);
    item = devdrv_manager_container_item_init(dev_container_table, &para);
    if (item == NULL) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("host item failed\n");
        dbl_kfree(dev_container_table);
        dev_container_table = NULL;
        return -ENODEV;
    }
    spin_unlock(&dev_container_table->spinlock);

    manager_info->container_table = dev_container_table;
    return 0;
}

void devdrv_manager_container_table_exit(struct devdrv_manager_info *manager_info)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct list_head *pos_item = NULL, *n_item = NULL;
    struct devdrv_container_item *item = NULL;

    if (manager_info == NULL || manager_info->container_table == NULL) {
        devdrv_drv_err("invalid parameter, dev_manager_info = %pK\n", manager_info);
        return;
    }
    dev_container_table = manager_info->container_table;

    spin_lock(&dev_container_table->spinlock);
    if (!list_empty_careful(&dev_container_table->item_list_head)) {
        list_for_each_safe(pos_item, n_item, &dev_container_table->item_list_head)
        {
            item = list_entry(pos_item, struct devdrv_container_item, item_list_node);
            devdrv_manager_container_item_free(item);
        }
    }
    spin_unlock(&dev_container_table->spinlock);

    dbl_kfree(manager_info->container_table);
    manager_info->container_table = NULL;
}

int devdrv_manager_container_get_bare_pid(struct devdrv_container_para *cmd)
{
    pid_t pid;
    int ret;

    if (cmd->para.out == NULL) {
        devdrv_drv_err("invalid parameter\n");
        return -EINVAL;
    }
    pid = current->pid;
    ret = copy_to_user_safe(cmd->para.out, &pid, sizeof(pid_t));
    if (ret) {
        devdrv_drv_err("pid = %d, copy to user failed: %d.\n", pid, ret);
        return -ENOMEM;
    }

    return 0;
}

int devdrv_manager_container_get_bare_tgid(struct devdrv_container_para *cmd)
{
    pid_t tgid;
    int ret;

    if (cmd->para.out == NULL) {
        devdrv_drv_err("invalid parameter\n");
        return -EINVAL;
    }
    tgid = current->tgid;
    ret = copy_to_user_safe(cmd->para.out, &tgid, sizeof(pid_t));
    if (ret) {
        devdrv_drv_err("tgid = %d, copy to user failed: %d.\n", tgid, ret);
        return -ENOMEM;
    }

    return 0;
}

STATIC int devdrv_manager_container_get_devlist_ns(struct devdrv_container_alloc_para *para,
    u64 mnt_ns)
{
    struct devdrv_container_table *dev_container_table = dev_manager_info->container_table;
    struct devdrv_container_item *item = NULL;
    u32 i;

    if (dev_container_table == NULL) {
        devdrv_drv_err("container table is NULL.\n");
        return -ENOMEM;
    }
    if (devdrv_manager_container_is_admin()) {
        mnt_ns = devdrv_manager_get_host_mnt_ns();
    }

    para->num = 0;
    spin_lock(&dev_container_table->spinlock);
    item = devdrv_manager_container_find_item(dev_container_table, mnt_ns);
    if (item == NULL) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("find item failed, mnt_ns = 0x%pK\n", (void *)(uintptr_t)mnt_ns);
        return -EINVAL;
    }
    if (item->conflict_flag != DEVMNG_CONTAINER_DEVICE_NORMAL) {
        devdrv_manager_container_print_list(dev_container_table);
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("some devices are inused by other docker, "
                       "mnt_ns = 0x%pK\n", (void *)(uintptr_t)mnt_ns);
        return -EBUSY;
    }
    if (item->dev_num > DEVDRV_MAX_DAVINCI_NUM) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("too many devices, dev_num(%u), devlist_size(%u)\n",
                       item->dev_num, DEVDRV_MAX_DAVINCI_NUM);
        return -EINVAL;
    }
    for (i = 0; i < item->dev_num; i++) {
        para->npu_id[i] = item->dev_id[i];
    }
    para->num = item->dev_num;
    spin_unlock(&dev_container_table->spinlock);

    return 0;
}

int devdrv_manager_container_get_davinci_devlist(struct devdrv_container_para *cmd)
{
    struct devdrv_container_alloc_para para = { 0 };
    int ret;

    if (cmd == NULL || cmd->para.out == NULL || dev_manager_info == NULL) {
        devdrv_drv_err("cmd = 0x%pK, dev_manager_info = 0x%pK\n",
                       (void *)(uintptr_t)cmd, (void *)(uintptr_t)dev_manager_info);
        return -EINVAL;
    }

    (void)devdrv_manager_get_devnum();

    ret = devdrv_manager_container_get_devlist_ns(&para, (u64)(uintptr_t)current->nsproxy->mnt_ns);
    if (ret == -EBUSY) {
        devdrv_drv_err("device is busy. (ret=%d)\n", ret);
        return -EBUSY;
    }
    if (ret) {
        devdrv_drv_err("get davinci devlist failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = copy_to_user_safe(cmd->para.out, &para, sizeof(struct devdrv_container_alloc_para));
    if (ret) {
        devdrv_drv_err("copy_to_user failed, ret = %d\n", ret);
        return -ENOMEM;
    }

    return 0;
}

int devdrv_manager_container_table_overlap(
    struct devdrv_manager_context *dev_manager_context, u32 *docker_id)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_manager_container_para para;
    int ret;

    if (devdrv_manager_container_is_host_system(dev_manager_context->mnt_ns)) {
        *docker_id = MAX_DOCKER_NUM;
        return 0;
    }

    dev_container_table = dev_manager_info->container_table;
    if (dev_container_table == NULL) {
        devdrv_drv_err("container table is NULL\n");
        return -ENOMEM;
    }

    para.mnt_ns = dev_manager_context->mnt_ns;
    para.pid_ns = dev_manager_context->pid_ns;
    para.start_time = dev_manager_context->start_time;
    para.real_start_time = dev_manager_context->real_start_time;

    spin_lock(&dev_container_table->spinlock);
    ret = devdrv_manager_container_table_overlap_item(dev_container_table, &para);
    if (ret) {
        devdrv_manager_container_print_list(dev_container_table);
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("add devlist to item failed, ret = %d\n", ret);
        return -ENODEV;
    }
    spin_unlock(&dev_container_table->spinlock);

    ret = devdrv_manager_container_get_docker_id(docker_id);
    if (ret) {
        devdrv_drv_err("container get docker_id failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    return 0;
}

STATIC u32 devdrv_manager_container_get_cdev_id_by_logical_id(
    struct devdrv_container_table *dev_container_table,
    struct devdrv_manager_container_para *para, u32 logical_dev_id)
{
    u32 cdev_id = DEVDRV_MAX_DAVINCI_NUM;
    u64 mnt_ns = para->mnt_ns;
    struct devdrv_container_item *item = NULL;
    u32 index, offset;
    u32 dev_index = 0;

    spin_lock(&dev_container_table->spinlock);
    item = devdrv_manager_container_find_item(dev_container_table, mnt_ns);
    if (item == NULL) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("find item failed, mnt_ns = %llu\n", mnt_ns);
        return DEVDRV_MAX_DAVINCI_NUM;
    }
    for (index = 0; index < DEVDRV_CONTIANER_NUM_OF_LONG; index++) {
        for (offset = 0; offset < BIT_NUM_U64; offset++) {
            if ((item->dev_list_map[index] & (0x1ULL << offset)) != 0) {
                if (dev_index == logical_dev_id) {
                    cdev_id = index * BIT_NUM_U64 + offset;
                    spin_unlock(&dev_container_table->spinlock);
                    return cdev_id;
                }
                dev_index++;
            }
        }
    }
    spin_unlock(&dev_container_table->spinlock);

    return cdev_id;
}

STATIC int devdrv_manager_container_check_devlist_ns(u32 *devlist, u32 devlist_len, u32 *devnum, u64 mnt_ns)
{
    if (devlist == NULL || devlist_len == 0 || devlist_len > DEVDRV_MAX_DAVINCI_NUM || devnum == NULL ||
        dev_manager_info == NULL || dev_manager_info->container_table == NULL) {
        devdrv_drv_err("invalid parameter, dev_manager_info = %pK, "
            "devlist = %pK, devlist_len = %u, devnum = %pK, mnt_ns = %llu\n",
            dev_manager_info, devlist, devlist_len, devnum, mnt_ns);
        return -EINVAL;
    }
    return 0;
}

int devdrv_manager_container_get_devids(u32 *devlist, u32 devlist_len, u32 *devnum, u64 mnt_ns)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_container_item *item = NULL;
    u32 devnum_tmp;
    u32 i;
    int ret;

    ret = devdrv_manager_container_check_devlist_ns(devlist, devlist_len, devnum, mnt_ns);
    if (ret) {
        devdrv_drv_err("check devlist ns error, ret=%d\n", ret);
        return -EINVAL;
    }

    /* privilege container's devlist are the same as host */
    if (devdrv_manager_container_is_admin()) {
        mnt_ns = devdrv_manager_get_host_mnt_ns();
    }

    dev_container_table = dev_manager_info->container_table;
    spin_lock(&dev_container_table->spinlock);
    item = devdrv_manager_container_find_item(dev_container_table, mnt_ns);
    if (item == NULL) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("item find failed, mnt_ns = 0x%pK\n", (void *)(uintptr_t)mnt_ns);
        return -ENODEV;
    }
    if (devlist_len < item->dev_num) {
        devdrv_drv_warn("weird devlist length, devlist_len = %u, item->dev_num = %u\n",
                        devlist_len, item->dev_num);
    }
    devnum_tmp = devlist_len > item->dev_num ? item->dev_num : devlist_len;
    spin_unlock(&dev_container_table->spinlock);

    for (i = 0; i < devnum_tmp; i++) {
        devlist[i] = i;
    }

    *devnum = devnum_tmp;
    return 0;
}

STATIC inline u32 devdrv_manager_container_get_devnum_bare(void)
{
    u32 dev_num;
    unsigned long flags;

    spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    dev_num = dev_manager_info->num_dev;
    spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    return dev_num;
}

u32 devdrv_manager_container_get_devnum_ns(u64 mnt_ns)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_container_item *item = NULL;
    u32 dev_num;

    if (mnt_ns == 0 || dev_manager_info == NULL || dev_manager_info->container_table == NULL) {
        devdrv_drv_err("invalid parameter, mnt_ns = %llu, dev_manager_info = %pK\n", mnt_ns, dev_manager_info);
        return DEVDRV_MAX_DAVINCI_NUM + 1;
    }

    if (devdrv_manager_container_is_host_system(mnt_ns)) {
        mnt_ns = devdrv_manager_get_host_mnt_ns();
    }

    dev_container_table = dev_manager_info->container_table;
    spin_lock(&dev_container_table->spinlock);
    item = devdrv_manager_container_find_item(dev_container_table, mnt_ns);
    if (item == NULL) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("item find failed, mnt_ns = %llu\n", mnt_ns);
        return DEVDRV_MAX_DAVINCI_NUM + 1;
    }
    dev_num = item->dev_num;
    spin_unlock(&dev_container_table->spinlock);

    if (dev_num == 0 || dev_num > DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_info("weird device number, dev_num = %u\n", dev_num);
    }
    return dev_num;
}

int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_manager_container_para para;
    int ret;

    if ((physical_dev_id == NULL) || (logical_dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (vfid == NULL) ||
        (dev_manager_info == NULL) || (dev_manager_info->container_table == NULL)) {
        devdrv_drv_err("invalid device num, physical_dev_id = 0x%pK, logical_dev_id = %u,"
                       " dev_manager_info = 0x%pK.\n",
                       (void *)(uintptr_t)physical_dev_id, logical_dev_id,
                       (void *)(uintptr_t)dev_manager_info);
        return -EINVAL;
    }
    *vfid = 0;

    ret = devdrv_manager_container_task_struct_check(current);
    if (ret) {
        devdrv_drv_err("invalid current pointer, ret = %d\n", ret);
        return -EINVAL;
    }

    dev_container_table = dev_manager_info->container_table;
    ret = devdrv_manager_get_current_mnt_ns(&para.mnt_ns);
    if (ret) {
        devdrv_drv_err("get current mnt ns error, ret = %d\n", ret);
        return -EINVAL;
    }
    if (devdrv_manager_container_is_admin()) {
        para.mnt_ns = devdrv_manager_get_host_mnt_ns();
    }

    *physical_dev_id = devdrv_manager_container_get_cdev_id_by_logical_id(dev_container_table, &para, logical_dev_id);
    if (*physical_dev_id >= DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_err("get physical dev id failed, logical_id = %u, physic_id = %u\n",
                       logical_dev_id, *physical_dev_id);
        return -ENODEV;
    }
    return 0;
}
EXPORT_SYMBOL(devdrv_manager_container_logical_id_to_physical_id);

int devdrv_manager_container_is_in_admin_container(void)
{
    if (devdrv_manager_container_is_host_system_ns(current->nsproxy->mnt_ns)) {
        return false;
    }

    if (devdrv_manager_container_is_admin()) {
        return true;
    }

    return false;
}
EXPORT_SYMBOL(devdrv_manager_container_is_in_admin_container);

int devdrv_manager_container_get_docker_id(u32 *docker_id)
{
    struct devdrv_container_table *dev_container_table = NULL;
    struct devdrv_container_item *item = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    u64 current_mnt_ns;
    int ret;

    if (docker_id == NULL) {
        devdrv_drv_err("docker_id is NULL\n");
        return -EINVAL;
    }

    *docker_id = MAX_DOCKER_NUM;

    if (dev_manager_info == NULL) {
        devdrv_drv_err("dev_manager_info is NULL\n");
        return -EINVAL;
    }

    dev_container_table = dev_manager_info->container_table;
    if (dev_container_table == NULL) {
        devdrv_drv_err("container table is NULL\n");
        return -EINVAL;
    }

    ret = devdrv_manager_get_current_mnt_ns(&current_mnt_ns);
    if (ret) {
        devdrv_drv_err("get mnt_ns failed(%d)\n", ret);
        return -EINVAL;
    }

    spin_lock(&dev_container_table->spinlock);
    if (!list_empty_careful(&dev_container_table->item_list_head)) {
        list_for_each_safe(pos, n, &dev_container_table->item_list_head)
        {
            item = list_entry(pos, struct devdrv_container_item, item_list_node);
            if (item->mnt_ns == current_mnt_ns) {
                *docker_id = item->id;
                break;
            }
        }
    }
    spin_unlock(&dev_container_table->spinlock);

    if (*docker_id >= MAX_DOCKER_NUM) {
        devdrv_drv_err("can't get docker_id or the docker_id is invalid: docker_id = %u\n", *docker_id);
        return -EINVAL;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_manager_container_get_docker_id);

int devdrv_manager_container_check_devid_in_container(u32 devid, pid_t hostpid)
{
    struct mnt_namespace *mnt_ns = NULL;
    struct task_struct *tsk = NULL;
    struct pid *pgrp = NULL;
    int ret;

    pgrp = find_get_pid(hostpid);
    if (pgrp == NULL) {
        devdrv_drv_err("pgrp is NULL\n");
        return -EINVAL;
    }

    tsk = get_pid_task(pgrp, PIDTYPE_PID);
    ret = devdrv_manager_container_task_struct_check(tsk);
    if (ret) {
        devdrv_drv_err("task_struct_check failed, ret(%d), devid(%u), hostpid(%d)\n", ret, devid, hostpid);
        if (tsk != NULL) {
            put_task_struct(tsk);
        }
        put_pid(pgrp);
        return -EINVAL;
    }
    mnt_ns = tsk->nsproxy->mnt_ns;

    if (((u64)(uintptr_t)mnt_ns == devdrv_manager_get_host_mnt_ns()) ||
        devdrv_manager_container_is_admin_task(tsk)) {
        put_task_struct(tsk);
        put_pid(pgrp);
        return 0;
    }

    ret = devdrv_manager_container_check_devid_in_container_ns(devid, tsk);
    if (ret) {
        devdrv_drv_err("device phyid %u is not belong to current docker, ret(%d), hostpid(%d)\n", devid, ret, hostpid);
    }
    put_task_struct(tsk);
    put_pid(pgrp);

    return ret;
}
EXPORT_SYMBOL(devdrv_manager_container_check_devid_in_container);

int devdrv_manager_container_check_devid_in_container_ns(u32 devid, struct task_struct *tsk)
{
    struct devdrv_container_table *dev_container_table = NULL;
    u64 mnt_ns = (u64)(uintptr_t)tsk->nsproxy->mnt_ns;
    struct devdrv_container_item *item = NULL;
    u32 index, offset;

    dev_container_table = dev_manager_info->container_table;
    if (devdrv_manager_container_is_admin_task(tsk)) {
        mnt_ns = devdrv_manager_get_host_mnt_ns();
    }
    spin_lock(&dev_container_table->spinlock);
    item = devdrv_manager_container_find_item(dev_container_table, mnt_ns);
    if (item == NULL) {
        spin_unlock(&dev_container_table->spinlock);
        devdrv_drv_err("item is null:mnt_ns(0x%llx), devid(%u)\n", mnt_ns, devid);
        return -EINVAL;
    }

    index = devid / BIT_NUM_U64;
    offset = devid % BIT_NUM_U64;
    if ((index < DEVDRV_CONTIANER_NUM_OF_LONG) &&
        (item->dev_list_map[index] & (0x1ULL << offset))) {
        spin_unlock(&dev_container_table->spinlock);
        return 0;
    }
    spin_unlock(&dev_container_table->spinlock);
    devdrv_drv_err("devid check in container failed, mnt_ns(0x%llx), devid(%u)\n", mnt_ns, devid);
    return -EINVAL;
}
EXPORT_SYMBOL(devdrv_manager_container_check_devid_in_container_ns);

#endif

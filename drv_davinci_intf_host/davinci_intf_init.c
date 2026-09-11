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

#ifndef DAVINCI_INTF_UT

#include "ka_kernel_def_pub.h"
#include "ka_errno_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_fs_pub.h"
#include "ka_pci_pub.h"
#include <linux/sched/mm.h>

#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_driver_pub.h"
#include "ka_ioctl_pub.h"

#include "securec.h"
#include "davinci_interface.h"
#include "davinci_api.h"
#include "davinci_intf_init.h"
#include "davinci_intf_common.h"
#include "dbl_mem_alloc_interface.h"
#include "davinci_intf_process.h"
#ifdef CFG_HOST_ENV
#include "tsdrv_common.h"
#else
#include "drv_cpu_type.h"
#include "drv_cpu_mask.h"
#include "workqueue_affinity.h"
#endif

#ifndef AOS_LLVM_BUILD
/* Device struct */
static ka_class_t *g_davinci_class = NULL;
static dev_t g_davinci_intf_dev;
#endif
static struct davinci_intf_stru g_davinci_intf_cb = {{0}};
#ifdef CFG_FEATURE_MANAGE_EXTRA
static dev_t g_davinci_intf_extra_dev;
static ka_cdev_t g_cdev_extra;
#endif

/* Check group id */
unsigned int g_authorized_group_id[DAVINIC_CONFIRM_USER_NUM] = {DAVINIC_NOT_CONFIRM_USER_ID,
    DAVINIC_NOT_CONFIRM_USER_ID, DAVINIC_NOT_CONFIRM_USER_ID};
#ifndef AOS_LLVM_BUILD
static int  g_authorized_group_id_num = 3;
ka_module_param_array(g_authorized_group_id, int, &g_authorized_group_id_num, 0644);
KA_MODULE_PARM_DESC(g_authorized_group_id, "Group ID of system user \"HwHiAiUser\", \"HwDmUser\", \"HwBaseUser\"");

#endif
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF

#ifndef AOS_LLVM_BUILD
#define MAX_RECYCLE_WORKQUEUE 8U
STATIC ka_workqueue_struct_t *g_recycle_wq[MAX_RECYCLE_WORKQUEUE];
STATIC ka_atomic64_t g_recycle_wq_idx;
#endif

#ifdef AOS_LLVM_BUILD
#ifndef KA_PCI_ANY_ID
#define KA_PCI_ANY_ID (~0)
#endif
struct pci_device_id {
    __u32 vendor, device;       /* Vendor and device ID or PCI_ANY_ID */
    __u32 subvendor, subdevice; /* Subsystem ID's or PCI_ANY_ID */
    __u32 class, class_mask;    /* (class,subclass,prog-if) triplet */
    unsigned long driver_data; /* Data private to the driver */
};
#endif
static const ka_pci_device_id_t g_davinci_intf_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd100),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd105),           0 },
    { KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd801),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd501),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd802),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd803),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd804),           0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd805),           0 },
    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}
};
KA_MODULE_DEVICE_TABLE(pci, g_davinci_intf_tbl);

STATIC int drv_ascend_intf_ioctl_open_cmd(
    ka_file_t *filep,
    unsigned int cmd,
    unsigned long arg);
STATIC int drv_ascend_intf_ioctl_close_cmd(
    ka_file_t *filep,
    unsigned int cmd,
    unsigned long arg);
int drv_ascend_intf_ioctl_check_module_no_use(
    ka_file_t *filep,
    unsigned int cmd,
    unsigned long arg);
extern void svm_process_exit_clean_slots(int pid);

STATIC int (*const g_davinci_ioctl_handlers[DAVINCI_INTF_IOCTL_CMD_MAX_NR])(ka_file_t *filep, unsigned int cmd,
    unsigned long arg) = {
        [_KA_IOC_NR(DAVINCI_INTF_IOCTL_OPEN)] = drv_ascend_intf_ioctl_open_cmd,
        [_KA_IOC_NR(DAVINCI_INTF_IOCTL_CLOSE)] = drv_ascend_intf_ioctl_close_cmd,
        [_KA_IOC_NR(DAVINCI_INTF_IOCTL_GET_MODULE_STATUS)] = drv_ascend_intf_ioctl_check_module_no_use,
};

STATIC void drv_davinci_svm_process_exit_clean(int pid)
{
#ifdef CFG_SUPPORT_OS_SVM_CLEAN
    svm_process_exit_clean_slots(pid);
#endif
    return;
}

STATIC int check_proc_open_module_count(
    pid_t owner_pid, u64 start_time, const char *module_name, unsigned int max)
{
    struct davinci_intf_process_stru *proc = NULL;
    unsigned int cnt;

    if (max == 0) { /* no limit */
        return 0;
    }
    proc = get_process_entry(&g_davinci_intf_cb, owner_pid, start_time);
    if (proc == NULL) {
        log_intf_err("not found proc. (module_name=\"%s\"; pid=%d)\n", module_name, owner_pid);
        return -ESRCH;
    }

    cnt = get_file_module_cnt(proc, module_name);
    if (cnt >= max) {
        log_intf_err("process open module cnt exceeded. (module_name=\"%s\"; pid=%d; cnt=%u; max=%u)\n",
            module_name, owner_pid, cnt, max);
        return -EBADF;
    }
    return 0;
}

STATIC int drv_ascend_set_file_ops(const char *module_name,
    struct davinci_intf_private_stru *prvi)
{
    int ret = -1;
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_sub_module_stru *node_info_next = NULL;
    ka_file_operations_t *ops = NULL;

    if (module_name == NULL || prvi == NULL) {
        log_intf_err("Input parameter is null.\n");
        return -EINVAL;
    }
    ops = &prvi->fops;

    /* Module list empty */
    if (ka_list_empty(&g_davinci_intf_cb.module_list) != 0) {
        log_intf_err("Module list is empty. (module_name=\"%s\")\n", module_name);
        return -EINVAL;
    }

    ka_list_for_each_entry_safe(node_info, node_info_next, &g_davinci_intf_cb.module_list, list) {
        /* Find the module and save the ops */
        if (ka_base_strncmp(node_info->module_name, module_name, DAVINIC_MODULE_NAME_MAX) == 0) {
            ret = check_proc_open_module_count(prvi->owner_pid, prvi->start_time, module_name, node_info->open_module_max);
            if (ret != 0) {
                return -EINVAL;
            }
            ret = memcpy_s(ops,
                sizeof(struct file_operations),
                &node_info->ops,
                sizeof(struct file_operations));
            if (ret != 0) {
                log_intf_err("memcpy_s error. (module_name=\"%s\"; ret=%d)\n",
                    module_name,
                    ret);
                return -EINVAL;
            }
            prvi->notifier = node_info->notifier;
            prvi->free_type = node_info->free_type;
            return 0;
        }
    }
    log_intf_err("Module not init. (module_name=\"%s\")\n", module_name);
    return  -EINVAL;
}

STATIC void drv_davinci_unset_file_ops(struct davinci_intf_private_stru *file_private)
{
    file_private->fops.open = NULL;
    file_private->fops.release = NULL;
    file_private->fops.unlocked_ioctl = NULL;
#ifndef AOS_LLVM_BUILD
    file_private->fops.mmap = NULL;
#endif
}

#ifndef AOS_LLVM_BUILD
#ifdef CFG_FEATURE_MANAGE_EXTRA
/* To create /dev/davinci_manager_docker or npu_device_cust. */
#ifdef CFG_FEATURE_MANAGE_DOCKER
#define DAVINCI_MANAGER_EXTRA_NAME      "davinci_manager_docker"
#else
#define DAVINCI_MANAGER_EXTRA_NAME      "npu_device_cust"
#endif

STATIC signed int drv_ascend_intf_extra_setup_cdev(const ka_file_operations_t *fops)
{
    ka_device_t *dev = NULL;
    int ret;

    if (fops == NULL) {
        log_intf_err("Invalid parameter. (fops_is_null=%d)\n", (fops == NULL));
        return -EINVAL;
    }

    ret = ka_fs_alloc_chrdev_region(&g_davinci_intf_extra_dev, 0, 1, "hisi_davinci_intf");
    if (ret != 0) {
        log_intf_err("Failed to invoke alloc_chrdev_region for davinci_manager_extra. (ret=%d)\n", ret);
        return -EINVAL;
    }

    g_cdev_extra.owner = KA_THIS_MODULE;
    ka_fs_cdev_init(&g_cdev_extra, fops);
    ret = ka_fs_cdev_add(&g_cdev_extra, g_davinci_intf_extra_dev, 1);
    if (ret < 0) {
        log_intf_err("Failed to invoke cdev_add for davinci_manager_extra. (ret=%d)\n", ret);
        goto unregister_region;
    }

    dev = ka_driver_device_create(g_davinci_class, NULL, g_davinci_intf_extra_dev, NULL, DAVINCI_MANAGER_EXTRA_NAME);
    if (KA_IS_ERR(dev)) {
        ret = PTR_ERR(dev);
        log_intf_err("Failed to invoke device_create for davinci_manager_extra. (ret=%d)\n", ret);
        goto delete_cdev;
    }

    return 0;

delete_cdev:
    ka_fs_cdev_del(&g_cdev_extra);
unregister_region:
    ka_fs_unregister_chrdev_region(g_davinci_intf_extra_dev, 1);

    return ret;
}

STATIC void drv_ascend_intf_extra_cleanup(void)
{
    ka_fs_cdev_del(&g_cdev_extra);
    ka_driver_device_destroy(g_davinci_class, g_davinci_intf_extra_dev);
    ka_fs_unregister_chrdev_region(g_davinci_intf_extra_dev, 1);
    return;
}
#endif

/* Create davinci interface /dev/davinci_manager */
STATIC signed int drv_ascend_intf_setup_cdev(struct davinci_intf_stru *cb, const ka_file_operations_t *fops)
{
    signed int rc;
    ka_device_t *dev = NULL;

    if ((cb == NULL) || (fops == NULL)) {
        log_intf_err("Input parameter is null.\n");
        return -EINVAL;
    }

    g_davinci_class = ka_driver_class_create(KA_THIS_MODULE, "devdrv_manager");
    if (KA_IS_ERR(g_davinci_class)) {
        rc = KA_PTR_ERR(g_davinci_class);
        g_davinci_class = NULL;
        log_intf_err("class_create failed. (rc=%d)\n", rc);

        return rc;
    }

    rc = ka_fs_alloc_chrdev_region(&g_davinci_intf_dev, 0, 1, "hisi_davinci_intf");
    if (rc != 0) {
        log_intf_err("alloc_chrdev_region failed. (rc=%d)\n", rc);
        goto cls_destroy;
    }

    dev = ka_driver_device_create(g_davinci_class, NULL, g_davinci_intf_dev, NULL, DAVINCI_INTF_DEV_NAME);
    if (KA_IS_ERR(dev)) {
        rc = KA_PTR_ERR(dev);
        log_intf_err("device_create failed. (rc=%d)\n", rc);
        goto unregister_region;
    }
    cb->device = dev;

    ka_fs_cdev_init(&cb->cdev, fops);
    cb->cdev.owner = KA_THIS_MODULE;
    cb->cdev.ops = fops;
    rc = ka_fs_cdev_add(&cb->cdev, g_davinci_intf_dev, 1);
    if (rc < 0) {
        log_intf_err("cdev_add failed. (rc=%d)\n", rc);
        goto dev_destroy;
    }

#ifdef CFG_FEATURE_MANAGE_EXTRA
    rc = drv_ascend_intf_extra_setup_cdev(fops);
    if (rc != 0) {
        log_intf_err("Failed to invoke drv_ascend_intf_extra_setup_cdev. (rc=%d)\n", rc);
        goto clean_manager;
    }
#endif

    return 0;

#ifdef CFG_FEATURE_MANAGE_EXTRA
clean_manager:
    ka_fs_cdev_del(&cb->cdev);
#endif
dev_destroy:
    ka_driver_device_destroy(g_davinci_class, g_davinci_intf_dev);
unregister_region:
    ka_fs_unregister_chrdev_region(g_davinci_intf_dev, 1);
cls_destroy:
    ka_driver_class_destroy(g_davinci_class);
    g_davinci_class = NULL;

    return rc;
}

STATIC signed int drv_ascend_intf_cleanup_cdev(struct davinci_intf_stru *cb)
{
    if (g_davinci_class == NULL) {
        log_intf_err("Input parameter is null.\n");
        return -EFAULT;
    }

#ifdef CFG_FEATURE_MANAGE_EXTRA
    drv_ascend_intf_extra_cleanup();
#endif
    ka_fs_cdev_del(&cb->cdev);
    ka_driver_device_destroy(g_davinci_class, g_davinci_intf_dev);
    ka_fs_unregister_chrdev_region(g_davinci_intf_dev, 1);
    ka_driver_class_destroy(g_davinci_class);
    g_davinci_class = NULL;

    return 0;
}
#endif

STATIC long drv_ascend_wait_work_finish(struct davinci_intf_private_stru *file_private)
{
    int wait_count = 0;
    /* Check file not in used */
    while ((ka_base_atomic_read(&file_private->work_count) != 0) &&
        (wait_count < DAVINIC_CONFIRM_MAX_TIME)) {
        wait_count++;
        if ((wait_count % DAVINIC_CONFIRM_WARN_MASK) != 0) {
            log_intf_warn("Waiting ioctl finish. (module_name=\"%s\"; work_count=%d)\n",
                file_private->module_name,
                ka_base_atomic_read(&file_private->work_count));
        };
        ka_system_msleep(DAVINIC_CONFIRM_EACH_TIME);
    }
    if (wait_count >= DAVINIC_CONFIRM_MAX_TIME) {
        log_intf_err("Wait ioctl finish timeout. (module_name=\"%s\"; work_count=%d)\n",
            file_private->module_name,
            ka_base_atomic_read(&file_private->work_count));
        return -EBUSY;
    }

    return 0;
}
STATIC long drv_davinci_inc_work_count(struct davinci_intf_private_stru *file_private_data)
{
    ka_base_atomic_inc(&file_private_data->work_count);
    /* If release_status has been set,no more work are allowed. */
    if (file_private_data->release_status == TRUE) {
        ka_base_atomic_dec(&file_private_data->work_count);
        return -EBUSY;
    }

    return 0;
}

STATIC void drv_davinci_dec_work_count(struct davinci_intf_private_stru *file_private_data)
{
    ka_base_atomic_dec(&file_private_data->work_count);
    return;
}

/* Check whether the process exists.
If the process does not exist, release related resources. */
STATIC void drv_davinci_intf_check_process(void)
{
    struct davinci_intf_process_stru *proc_node = NULL;
    ka_list_head_t *process_list = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *next = NULL;

    process_list = &g_davinci_intf_cb.process_list;
    if (ka_list_empty_careful(process_list) != 0) {
        return;
    }

    ka_list_for_each_safe(pos, next, process_list)
    {
        proc_node = (struct davinci_intf_process_stru *)ka_list_entry(pos, struct davinci_intf_process_stru, list);
        if (ka_list_empty_careful(&proc_node->file_list) && (ka_base_atomic_read(&proc_node->work_count) <= 0)) {
            ka_list_del(pos);

            release_file_free_list(proc_node->free_list);
            if (proc_node->free_list != NULL) {
                dbl_kfree(proc_node->free_list);
                proc_node->free_list = NULL;
            }

            free_process_entry(proc_node);
            dbl_kfree(proc_node);
            proc_node = NULL;
        }
    }
    return;
}

STATIC int drv_ascend_intf_call_release(struct davinci_intf_private_stru **file_private_ptr)
{
    int ret = 0;
    struct davinci_intf_private_stru *file_private = *file_private_ptr;

    if (file_private == NULL) {
        log_intf_err("Invalid file_private_data.\n");
        return 0;
    }

    if (file_private->fops.release != NULL) {
        if (drv_ascend_wait_work_finish(file_private) != 0) {
            log_intf_err("Dev is busy, cannot close. (module_name=\"%s\"; owner_pid=%d; work_count=%d)\n",
                file_private->module_name, file_private->owner_pid,
                ka_base_atomic_read(&file_private->work_count));
            return -EBUSY;
        }
        /* Call file release */
        ret = file_private->fops.release(0, &file_private->priv_filep);
        if (ret != 0) {
            log_intf_err("Release failed. (module_name=\"%s\"; owner_pid=%d; ret=%d)\n",
                file_private->module_name, file_private->owner_pid, ret);
        }
    }

    if (file_private->fops.owner != NULL) {
        if (ka_system_module_refcount(file_private->fops.owner) > 0) {
            ka_system_module_put(file_private->fops.owner);
        }
    }
    if (file_private->notifier.owner != NULL) {
        if (ka_system_module_refcount(file_private->notifier.owner) > 0) {
            ka_system_module_put(file_private->notifier.owner);
        }
    }

    ka_task_mutex_lock(&file_private->fmutex);
    drv_davinci_unset_file_ops(file_private);
    ka_task_mutex_unlock(&file_private->fmutex);
    ka_task_mutex_destroy(&file_private->fmutex);
    dbl_kfree(file_private);
    *file_private_ptr = NULL;

    return ret;
}

/* Release sub module */
STATIC long drv_ascend_intf_sub_module_release(struct davinci_intf_free_file_stru *file_node)
{
    struct davinci_intf_private_stru *file_private = NULL;
    int ret = 0;

    if (file_node == NULL) {
        log_intf_err("Invalid file_node.\n");
        return 0;
    }

    file_private = file_node->file_private;
    if (file_private == NULL) {
        log_intf_err("Invalid file_private_data. (module_name=\"%s\"; owner_pid=%d;)\n",
            file_node->module_name, file_node->owner_pid);
        return 0;
    }

    ret = drv_ascend_intf_call_release(&file_private);
    log_intf_debug("Call release module. (module_name=\"%s\"; pid=%d; ret=%d)\n",
        file_node->module_name, file_node->owner_pid, ret);

    return ret;
}

STATIC void drv_davinci_run_release(struct work_struct *work)
{
    struct davinci_intf_free_list_stru *free_list = NULL;
    int all_flag = FALSE;
    int owner_pid = 0;

    free_list = ka_container_of(work, struct davinci_intf_free_list_stru, release_work);
    if (free_list != NULL) {
        all_flag = free_list->all_flag;
        owner_pid = free_list->owner_pid;
    }

    /* run release work, the "free_list" check int the function "drv_ascend_release_work" */
    drv_ascend_release_work(free_list);
    /* need call svm process exit clean */
    if (all_flag == TRUE) {
        drv_davinci_svm_process_exit_clean(owner_pid);
    }
}

/* set release task cpumask,ensure that run in the CTRLCPU */
#ifdef CFG_BIND_CPU_ENABLE
static inline void drv_bind_task_by_mask(ka_task_struct_t *release_task,
    cpumask_var_t cpu_mask)
{
#ifndef CFG_HOST_ENV
    if (IS_ERR(release_task) || (release_task == NULL)) {
        return;
    }
    if (cpumask_available(cpu_mask)) {
        /* set cpumask */
        (void)set_cpus_allowed_ptr(release_task, cpu_mask);
    }
#endif
    return;
}
#endif

void drv_davinci_bind_ctrl_cpu(void *release_task)
{
#ifdef CFG_BIND_CPU_ENABLE
    drv_bind_task_by_mask((ka_task_struct_t *)release_task, g_davinci_intf_cb.cpumask);
    return;
#endif
}
KA_EXPORT_SYMBOL_GPL(drv_davinci_bind_ctrl_cpu);

STATIC int drv_davinci_release_run(void *arg)
{
    long ret;
    struct davinci_intf_free_file_stru *file_node = arg;
    ret = drv_ascend_intf_sub_module_release(file_node);
    ka_base_atomic_dec(&file_node->owner_list->current_count);
    dbl_kfree(file_node);
    ka_task_do_exit(0);
    return ret;
}

static inline int drv_ascend_wait_finish(struct davinci_intf_free_list_stru *free_list)
{
    unsigned int count = 0;
    while ((ka_base_atomic_read(&free_list->current_count) > 0) &&
        (count < DAVINIC_FREE_WAIT_MAX_TIME)) {
        ka_system_msleep(DAVINIC_FREE_WAIT_EACH_TIME);
        count++;
    }
    if (count >= DAVINIC_FREE_WAIT_MAX_TIME) {
        log_intf_err("Waiting for the previous task to complete timed out. (owner_pid=%d; count=%u)\n",
            free_list->owner_pid, count);
        return -EINVAL;
    } else {
        return 0;
    }
}

static void drv_davinci_wait_free_finish(struct davinci_intf_free_file_stru *file_node)
{
    if ((file_node->owner_list->current_free_index == file_node->free_index) &&
        (file_node->free_type == DAVINIC_FREE_IN_PARALLEL)) {
        return;
    }

    (void)drv_ascend_wait_finish(file_node->owner_list);
    return;
}

STATIC long drv_ascend_intf_release_file(struct davinci_intf_free_file_stru *file_node)
{
    long ret;
    ka_task_struct_t *release_task = NULL;
    /* wait last free work finish */
    drv_davinci_wait_free_finish(file_node);
    file_node->owner_list->current_free_index = file_node->free_index;
    ka_base_atomic_inc(&file_node->owner_list->current_count);
    if (file_node->free_type == DAVINIC_FREE_IN_ORDER) {
        ret = drv_ascend_intf_sub_module_release(file_node);
        ka_base_atomic_dec(&file_node->owner_list->current_count);
        dbl_kfree(file_node);
        return ret;
    } else {
        /* create async free task */
        release_task = ka_task_kthread_create(drv_davinci_release_run, (void*)file_node, "davinci_sub_recycle");
        if (KA_IS_ERR(release_task) || (release_task == NULL)) {
            log_intf_warn("Kthread_create not up to expectations. (pid=%d, num=%ld)\n", file_node->owner_pid, PTR_ERR(release_task));
            ret = drv_ascend_intf_sub_module_release(file_node);
            ka_base_atomic_dec(&file_node->owner_list->current_count);
            dbl_kfree(file_node);
            return ret;
        }
        drv_davinci_bind_ctrl_cpu(release_task);
        (void)ka_task_wake_up_process(release_task);
    }
    return 0;
}

void drv_ascend_release_work(struct davinci_intf_free_list_stru *free_list)
{
    struct davinci_intf_free_file_stru *file_next = NULL;
    struct davinci_intf_free_file_stru *file_pos = NULL;
    ka_list_head_t *file_list = NULL;
    int ret = 0;
    if (free_list == NULL) {
        log_intf_err("Input parameter error,proc is NULL.\n");
        return;
    }

    /* If file list is empty, completion of the process */
    if (ka_list_empty(&free_list->list) != 0) {
        goto out;
    }

    ka_base_atomic_set(&free_list->current_count, 0);

    file_list = &free_list->list;
    /* for each free_node */
    ka_list_for_each_entry_safe(file_pos, file_next, file_list, list)
    {
        ka_list_del(&file_pos->list);
        /* release the sub module's file */
        ret = drv_ascend_intf_release_file(file_pos);
        if (ret != 0) {
            log_intf_err("Release failed. (ret=%d)\n", ret);
            continue;
        }
    }
    /* wait all free finish */
    ret = drv_ascend_wait_finish(free_list);
out:
    /* dec the work_count */
    if (free_list->owner_proc != NULL) {
        ka_base_atomic_dec(&free_list->owner_proc->work_count);
    }
    if (ret == 0) {
        dbl_kfree(free_list);
        free_list = NULL;
    }
    return;
}

STATIC struct davinci_intf_sub_module_stru *drv_davinci_get_module(const char *module_name)
{
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_sub_module_stru *node_info_next = NULL;

    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    if (ka_list_empty(&g_davinci_intf_cb.module_list) != 0) {
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return NULL;
    }

    ka_list_for_each_entry_safe(node_info, node_info_next, &g_davinci_intf_cb.module_list, list) {
        if (ka_base_strcmp(node_info->module_name, module_name) == 0) {
            ka_task_up_write(&g_davinci_intf_cb.cb_sem);
            return node_info;
        }
    }

    ka_task_up_write(&g_davinci_intf_cb.cb_sem);
    return NULL;
}

STATIC int check_notifier(struct davinci_intf_private_stru *file_private, struct notifier_operations *noti)
{
    struct davinci_intf_sub_module_stru *module_stru = NULL;
    int ret = 0;

    module_stru = drv_davinci_get_module(file_private->module_name);
    if (module_stru == NULL) {
        log_intf_err("not found module struct. (module_name=\"%s\";pid=%d)\n",
            file_private->module_name, file_private->owner_pid);
        return -ESRCH;
    }

    ret = drv_ascend_wait_work_finish(file_private);
    if (ret != 0) {
        return ret;
    }


    *noti = module_stru->notifier;
    if (noti->owner == file_private->notifier.owner && noti->notifier_call == file_private->notifier.notifier_call) {
        return 0;
    }

    log_intf_err("check notifier fail. (module_name=\"%s\"; pid=%d; diff owner=%d; fun=%d)\n",
        file_private->module_name, file_private->owner_pid, noti->owner != file_private->notifier.owner,
        noti->notifier_call != file_private->notifier.notifier_call);

    return -EBADF;
}

STATIC void drv_ascend_intf_call_notifier(
    struct davinci_intf_private_stru *file_private,
    unsigned long mode)
{
    int ret;
    struct notifier_operations verified_notifier = {0};

    if (file_private == NULL || file_private->notifier.notifier_call == NULL) {
        return;
    }

    ret = check_notifier(file_private, &verified_notifier);
    if (ret != 0) {
        return;
    }

    /* Call notifier_call */
    ret = verified_notifier.notifier_call(&file_private->priv_filep, mode);
    if (ret != 0) {
        log_intf_err("Notify sub module failed. (module_name=\"%s\"; mode=0x%lx; ret=%d)\n",
            file_private->module_name, mode, ret);
        return;
    }
}

STATIC void drv_davinci_notify_release_work(struct davinci_intf_free_list_stru *free_list)
{
    struct davinci_intf_free_file_stru *file_next = NULL;
    struct davinci_intf_free_file_stru *file_pos = NULL;
    ka_list_head_t *file_list = NULL;
    file_list = &free_list->list;
    ka_list_for_each_entry_safe(file_pos, file_next, file_list, list) {
        drv_ascend_intf_call_notifier(file_pos->file_private, NOTIFY_MODE_RELEASE_PREPARE);
    }
}

#if (!defined(CFG_HOST_ENV)) && (!defined(AOS_LLVM_BUILD))
int drv_ascend_get_ctrlcpu_mask(cpumask_t *ctrl_cpumask)
{
#ifdef CFG_FEATURE_SUPPORT_CPUDOMAIN
    return drv_get_ctrlcpu_mask(ctrl_cpumask);
#else
    return drv_get_ctrlcpu_mask_from_cpuset(ctrl_cpumask);
#endif
}

static void init_ctrl_cpumask(struct davinci_intf_stru *cb)
{
    int ret;
    cpumask_t ctrl_cpumask;

    cpumask_clear(&ctrl_cpumask);
    ret = drv_ascend_get_ctrlcpu_mask(&ctrl_cpumask);
    if (ret != 0) {
        log_intf_info("The cpumask configuration is not found. The CPU affinity will not be set.\n");
        return;
    }

    /* alloc cpumask var */
    if (!zalloc_cpumask_var(&cb->cpumask, KA_GFP_KERNEL)) {
        log_intf_info("cpumask variable not be alloced, The CPU affinity will not be set.\n");
        return;
    }
    cpumask_copy(cb->cpumask, &ctrl_cpumask);
    return;
}
#else
static void init_ctrl_cpumask(struct davinci_intf_stru *cb)
{
    log_intf_debug("no need to init cpu mask.\n");
}
#endif

#ifdef CFG_BIND_CPU_ENABLE
STATIC void intf_recycle_workqueue_affinity(struct workqueue_struct *wq)
{
    struct cpumask wq_cpumask;
    int ret;

    ret = drv_ascend_get_ctrlcpu_mask(&wq_cpumask);
    if (ret != 0) {
        log_intf_warn("get ctrl cpu mask warn.(ret=%d)\n", ret);
        return;
    }

    if (set_workqueue_affinity(wq, 0, &wq_cpumask) != 0) {
        log_intf_warn("bind workqueue dms_timer_common warn\n");
    }
}
#endif

STATIC void drv_ascend_run_release_work(struct davinci_intf_free_list_stru *free_list)
{
    uint64_t idx = 0;
    /* when the file list is null,means already released */
    if(free_list == NULL) {
        log_intf_err("free_list is NULL\n");
        return;
    }
    if (ka_list_empty(&free_list->list) != 0) {
        ka_base_atomic_dec(&free_list->owner_proc->work_count);
        dbl_kfree(free_list);
        free_list = NULL;
        return;
    }
    /* Notify sub mode prepare release first */
    drv_davinci_notify_release_work(free_list);
    /* init the queue_work, start the work task */
    idx = (uint64_t)ka_base_atomic64_inc_return(&g_recycle_wq_idx);
    KA_TASK_INIT_WORK(&free_list->release_work, drv_davinci_run_release);
    ka_task_queue_work(g_recycle_wq[idx % MAX_RECYCLE_WORKQUEUE], &free_list->release_work);
    return;
}

/* Release resources when module exit */
STATIC int drv_ascend_intf_release_process(void)
{
    struct davinci_intf_process_stru *proc_node = NULL;
    ka_list_head_t *process_list = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *next = NULL;
    process_list = &g_davinci_intf_cb.process_list;
    if (ka_list_empty(process_list) != 0) {
        return 0;
    }

    ka_list_for_each_safe(pos, next, process_list)
    {
        proc_node = (struct davinci_intf_process_stru *)ka_list_entry(pos, struct davinci_intf_process_stru, list);
        ka_list_del(pos);

        release_file_free_list(proc_node->free_list);
        if (proc_node->free_list != NULL) {
            dbl_kfree(proc_node->free_list);
            proc_node->free_list = NULL;
        }

        destory_file_proc_list(proc_node);
        dbl_kfree(proc_node);
    }
    return 0;
}

void drv_ascend_free_file_node(ka_file_t *file)
{
    struct davinci_intf_private_stru *file_private_data = NULL;
    struct davinci_intf_file_stru *file_node = NULL;

    file_private_data = file->private_data;
    if (file_private_data == NULL) {
        return;
    }
    /* already in release state */
    if (file_private_data->release_status == TRUE) {
        return;
    }
    file_node = file_private_data->file_stru_node;
    if (file_node == NULL) {
        return;
    }

    ka_list_del(&file_node->list);
    dbl_kfree(file_node);
    file_private_data->file_stru_node = NULL;

    return;
}

void drv_intf_trans_free_list_nodes(struct davinci_intf_process_stru *proc,
    ka_file_t *file, unsigned int free_index)
{
    struct davinci_intf_free_list_stru *proc_free_list = proc->free_list;
    struct davinci_intf_private_stru *file_private_data = NULL;
    struct davinci_intf_free_list_stru *file_free_list = NULL;

    struct davinci_intf_free_file_stru *file_next = NULL;
    struct davinci_intf_free_file_stru *file_pos = NULL;

    file_private_data = file->private_data;

    if (file_private_data == NULL) {
        log_intf_err("Invalid file_private_data.\n");
        return;
    }

    if (file_private_data->release_status == TRUE || file_private_data->free_list == NULL) {
        log_intf_err("The file free node has been released.\n");
        return;
    }

    file_free_list = file_private_data->free_list;
    if (ka_list_empty(&file_free_list->list) != 0) {
        log_intf_err("file free list is empty. (proc=\"%d\")\n", proc->owner_pid);
        dbl_kfree(file_free_list);
        file_private_data->free_list = NULL;
        return;
    }

    ka_list_for_each_entry_safe(file_pos, file_next, &file_free_list->list, list) {
        ka_list_del(&file_pos->list);

        file_pos->free_index = free_index;
        file_pos->owner_list = proc_free_list;
        file_pos->owner_pid = proc->owner_pid;
        ka_list_add_tail(&file_pos->list, &proc_free_list->list);
    }

    file->private_data = NULL;
    file_private_data->free_list = NULL;
    dbl_kfree(file_free_list);
    file_private_data->release_status = TRUE;
    return;
}

void free_uninit_file_pos(struct davinci_intf_process_stru *proc, ka_file_t *file)
{
    struct davinci_intf_private_stru *file_private_data = NULL;
    struct davinci_intf_free_list_stru *file_free_list = NULL;

    struct davinci_intf_free_file_stru *file_next = NULL;
    struct davinci_intf_free_file_stru *file_pos = NULL;

    file_private_data = file->private_data;

    if (file_private_data == NULL) {
        log_intf_err("Invalid file_private_data.\n");
        return;
    }

    if (file_private_data->release_status == TRUE || file_private_data->free_list == NULL) {
        log_intf_err("The file free node has been released.\n");
        return;
    }

    file_free_list = file_private_data->free_list;
    if (ka_list_empty(&file_free_list->list) != 0) {
        log_intf_err("file free list is empty. (proc=\"%d\")\n", proc->owner_pid);
        dbl_kfree(file_free_list);
        file_private_data->free_list = NULL;
        return;
    }

    ka_list_for_each_entry_safe(file_pos, file_next, &file_free_list->list, list) {
        ka_list_del(&file_pos->list);
        dbl_kfree(file_pos);
    }

    file->private_data = NULL;
    dbl_kfree(file_free_list);
    file_private_data->free_list = NULL;
    dbl_kfree(file_private_data);
    file_private_data = NULL;

    return;
}

int drv_ascend_add_release_list_all(struct davinci_intf_process_stru *proc, ka_file_t *file)
{
    struct davinci_intf_sub_module_stru *node_info_next = NULL;
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_file_stru *file_next = NULL;
    struct davinci_intf_file_stru *file_pos = NULL;
    ka_list_head_t *module_list = NULL;
    ka_list_head_t *file_list = NULL;
    unsigned int free_index = 0;
    int ret = 0;
    int ret_sprintf = 0;
    char buff[MODULE_NAME_MAX_LEN] = {0};
    unsigned int buff_index = 0;
    unsigned int buff_len = 0;
    /* already free or no file opened */
    if (ka_list_empty(&proc->file_list) != 0) {
        return 0;
    }

    module_list = &g_davinci_intf_cb.module_list;
    ka_list_for_each_entry_safe(node_info, node_info_next, module_list, list)
    {
        free_index++;
        ka_task_mutex_lock(&proc->res_lock);
        file_list = &proc->file_list;
        ka_list_for_each_entry_safe(file_pos, file_next, file_list, list)
        {
            if ((ka_base_strcmp(node_info->module_name, file_pos->module_name) != 0)) {
                if ((ka_base_strcmp(DAVINIC_UNINIT_FILE, file_pos->module_name) == 0)) {
                    log_intf_warn("free uninit file pos. (pid=%d; module_name=\"%s\")\n",
                        file_pos->owner_pid, file_pos->module_name);
                    ka_list_del(&file_pos->list);
                    free_uninit_file_pos(proc, file_pos->file_op);
                    dbl_kfree(file_pos);
                    file_pos = NULL;
                }
                continue;
            }

            /* Delete file list first */
            ka_list_del(&file_pos->list);
            drv_intf_trans_free_list_nodes(proc, file_pos->file_op, free_index);

            log_intf_debug("Add release list success. (pid=%d; module_name=\"%s\"; "
                "seq=%u; time=%u ms; cur=%u ms; trig=%d; mm=%d)\n",
                file_pos->owner_pid, file_pos->module_name, file_pos->seq,
                file_pos->open_time, jiffies_to_msecs(jiffies), file_pos->file_op == file, current->mm != NULL);

            buff_len = (MODULE_NAME_MAX_LEN > buff_index) ? (MODULE_NAME_MAX_LEN - buff_index) : 0;
            ret_sprintf = sprintf_s(buff + buff_index, buff_len, "%s[%d] ", file_pos->module_name, file_pos->owner_pid);
            if (ret_sprintf < 0) {
                dbl_kfree(file_pos);
                file_pos = NULL;
                continue;
            }
            buff_index += (unsigned int)ret_sprintf;

            dbl_kfree(file_pos);
            file_pos = NULL;
        }
        ka_task_mutex_unlock(&proc->res_lock);
    }
    log_intf_info("Add release list success. (%s)\n", buff);
    return ret;
}

STATIC struct davinci_intf_free_list_stru *drv_ascend_make_release_list(
    struct davinci_intf_process_stru *proc,
    ka_file_t *file, int all_flag)
{
    int ret;
    struct davinci_intf_private_stru *file_private_data = file->private_data;
    struct davinci_intf_free_list_stru *free_list = NULL;

    /* free single file description */
    if (all_flag == FALSE) {
        if (file_private_data == NULL) {
            log_intf_err("Invalid file_private_data.\n");
            return NULL;
        }

        if (file_private_data->free_list == NULL || file_private_data->release_status == TRUE) {
            log_intf_err("file already free.\n");
            return NULL;
        }

        ka_task_mutex_lock(&proc->res_lock);
        drv_ascend_free_file_node(file);

        free_list = file_private_data->free_list;

        file->private_data = NULL;
        file_private_data->free_list = NULL;
        file_private_data->release_status = TRUE;
        ka_task_mutex_unlock(&proc->res_lock);
    } else {
        ret = drv_ascend_add_release_list_all(proc, file);
        if (ret != 0) {
            log_intf_err("Add release list failed. (ret=%d)\n", ret);
            return NULL;
        }
        if (proc->free_list == NULL) {
            return NULL;
        }

        free_list = proc->free_list;
        proc->free_list = NULL;
    }

    free_list->all_flag = all_flag;
    return free_list;
}

void release_file_free_list(struct davinci_intf_free_list_stru *file_free_list)
{
    struct davinci_intf_free_file_stru *file_next = NULL;
    struct davinci_intf_free_file_stru *file_pos = NULL;

    if (file_free_list != NULL) {
        if (ka_list_empty(&file_free_list->list) != 0) {
            return;
        } else {
            ka_list_for_each_entry_safe(file_pos, file_next, &file_free_list->list, list) {
                ka_list_del(&file_pos->list);
                dbl_kfree(file_pos);
                file_pos = NULL;
            }
        }
    }

    return;
}

STATIC int drv_ascend_intf_open(struct inode *inode, ka_file_t *file)
{
    struct davinci_intf_private_stru *file_private_data = NULL;
    struct davinci_intf_stru *cb = &g_davinci_intf_cb;
    signed int ret = -EBUSY;
    ASSERT_RET((inode != NULL), (-EFAULT));
    ASSERT_RET((file != NULL), (-EFAULT));

    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        return ret;
    }

    file_private_data = (struct davinci_intf_private_stru *)dbl_kzalloc(
        sizeof(struct davinci_intf_private_stru), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (file_private_data == NULL) {
        log_intf_err("kzalloc failed. (size=%lu)\n", sizeof(struct davinci_intf_private_stru));
        ka_system_module_put(KA_THIS_MODULE);
        return -ENOMEM;
    }
    ka_task_mutex_init(&file_private_data->fmutex);
    file_private_data->device_cb = cb;
    file_private_data->device_id = DAVINIC_INTF_INVAILD_DEVICE_ID;
    file_private_data->owner_pid = current->tgid;
    file_private_data->start_time = ka_task_get_current_group_starttime();
    file_private_data->close_flag = DAVINIC_NOT_INIT_BY_OPENCMD;
    file_private_data->release_status = FALSE;
    ka_base_atomic_set(&file_private_data->work_count, 0);
    /* Initialize the vendor's private dispatch snapshot; it is never fput().
     * A struct assignment is unavailable once VFS makes f_path const.
     * The original file continues to own the borrowed VFS references.
     */
    memcpy(&file_private_data->priv_filep, file, sizeof(file_private_data->priv_filep));
    ret = strcpy_s(file_private_data->module_name, DAVINIC_MODULE_NAME_MAX,
        DAVINIC_UNINIT_FILE);
    if (ret != 0) {
        log_intf_err("strcpy_s failed. (module_name=\"%s\"; ret=%d)\n",
            file_private_data->module_name,
            ret);
        ret = -ENOSYS;
        goto out_err;
    }

    ka_task_down_write(&cb->cb_sem);
    /* Check process list */
    drv_davinci_intf_check_process();
    ret = add_file_to_list(cb, file, file_private_data);
    if (ret != 0) {
        log_intf_err("Add file to list failed. (ret=%d)\n", ret);
        goto add_file_err;
    }
    file->private_data = file_private_data;
    ka_task_up_write(&cb->cb_sem);
    ka_base_atomic_inc(&cb->count);

    return ret;


add_file_err:
    release_file_free_list(file_private_data->free_list);
    if (file_private_data->free_list != NULL) {
        dbl_kfree(file_private_data->free_list);
        file_private_data->free_list = NULL;
    }
    ka_task_up_write(&cb->cb_sem);

out_err:
    ka_system_module_put(KA_THIS_MODULE);
    file->private_data = NULL;
    dbl_kfree(file_private_data);
    file_private_data = NULL;
    return ret;
}

STATIC int drv_ascend_intf_release(struct inode *inode, ka_file_t *file)
{
    struct davinci_intf_private_stru *file_private_data = NULL;
    struct davinci_intf_free_list_stru *free_list = NULL;
    struct davinci_intf_process_stru *proc = NULL;
    struct davinci_intf_stru *cb = &g_davinci_intf_cb;
    int all_flag;
    ASSERT_RET((inode != NULL), (-EFAULT));
    ASSERT_RET((file != NULL), (-EFAULT));

    ka_system_module_put(KA_THIS_MODULE);
    ka_task_down_write(&cb->cb_sem);
    /* Check process list */
    drv_davinci_intf_check_process();

    file_private_data = file->private_data;
    if ((file_private_data == NULL) || (file_private_data->device_cb == NULL)) {
        ka_task_up_write(&cb->cb_sem);
        return -EBADFD;
    }

    ka_base_atomic_dec(&cb->count);
    proc = get_process_entry(cb, file_private_data->owner_pid, file_private_data->start_time);
    if (proc == NULL) {
        /* if proc is null, means already free by other fd */
        ka_task_up_write(&cb->cb_sem);
        return 0;
    }

    /* close_flag was FALSE, means free all file */
    all_flag = (file_private_data->close_flag == FALSE) ? TRUE : FALSE;
    free_list = drv_ascend_make_release_list(proc, file, all_flag);
    if (free_list == NULL) {
        ka_task_up_write(&cb->cb_sem);
        log_intf_err("Make free list failed. (module_name=\"%s\"; pid=%d)\n",
            file_private_data->module_name, file_private_data->owner_pid);
        return -EBADFD;
    }
    /* inc process work count */
    ka_base_atomic_inc(&proc->work_count);
    ka_task_up_write(&cb->cb_sem);
    drv_ascend_run_release_work(free_list);
    return 0;
}

STATIC int drv_ascend_intf_init_file_private(struct davinci_intf_private_stru *file_private,
    const char *module_name, int device_id)
{
    int ret;
    struct davinci_intf_free_list_stru *file_free_list = file_private->free_list;
    struct davinci_intf_free_file_stru *file_next = NULL;
    struct davinci_intf_free_file_stru *file_pos = NULL;

    ka_task_mutex_lock(&file_private->fmutex);
    if (file_private->close_flag != DAVINIC_NOT_INIT_BY_OPENCMD) {
        ka_task_mutex_unlock(&file_private->fmutex);
        log_intf_err("Already call open command. (module_name=\"%s\"; new_name=\"%s\")\n",
            file_private->module_name,
            module_name);
        return -ENOSYS;
    }

    ret = drv_ascend_set_file_ops(module_name, file_private);
    if (ret != 0) {
        ka_task_mutex_unlock(&file_private->fmutex);
        log_intf_err("set file ops failed. (module_name=\"%s\"; ret=%d)\n",
            module_name,
            ret);
        return ret;
    }

    ret = strcpy_s(file_private->module_name, DAVINIC_MODULE_NAME_MAX, module_name);
    if (ret != 0) {
        ka_task_mutex_unlock(&file_private->fmutex);
        log_intf_err("strcpy_s failed. (module_name=\"%s\"; ret=%d)\n", module_name, ret);
        return -ENOSYS;
    }

    ka_list_for_each_entry_safe(file_pos, file_next, &file_free_list->list, list) {
        ret = strcpy_s(file_pos->module_name, DAVINIC_MODULE_NAME_MAX, module_name);
        if (ret != 0) {
            ka_task_mutex_unlock(&file_private->fmutex);
            log_intf_err("strcpy_s failed. (module_name=\"%s\"; ret=%d)\n", module_name, ret);
            return -ENOSYS;
        }
    }

    file_private->device_id = device_id;
    ka_task_mutex_unlock(&file_private->fmutex);
    return 0;
}

STATIC int drv_ascend_intf_open_private(struct davinci_intf_private_stru *file_private)
{
    int ret;

    if (file_private->fops.owner != NULL) {
        if (!ka_system_try_module_get(file_private->fops.owner)) {
            log_intf_err("module is busy. (module_name=\"%s\"; owner_pid=%d)\n",
                 file_private->module_name, file_private->owner_pid);
            return -EBUSY;
        }
    }

    if (file_private->notifier.owner != NULL) {
        if (!ka_system_try_module_get(file_private->notifier.owner)) {
            log_intf_err("notify module is busy. (module_name=\"%s\"; owner_pid=%d)\n",
                 file_private->module_name, file_private->owner_pid);
            return -EBUSY;
        }
    }

    if (file_private->fops.open != NULL) {
        ret = file_private->fops.open(file_private->priv_filep.f_inode, &file_private->priv_filep);
        if (ret != 0) {
            log_intf_warn("open detail info. (module_name=\"%s\"; ret=%d)\n",
                file_private->module_name, ret);
        }
        return ret;
    }

    return -EINVAL;
}

int drv_ascend_intf_ioctl_open_cmd(
    ka_file_t *filep,
    unsigned int cmd,
    unsigned long arg)
{
    int ret;
    struct davinci_intf_open_arg module_para = {{0}};
    struct davinci_intf_private_stru *file_private = NULL;
    size_t name_len;
    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    file_private = filep->private_data;
    if (file_private == NULL) {
        log_intf_err("Invalid file_private_data.\n");
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return -EBADFD;
    }

    if ((void *)arg == NULL) {
        log_intf_err("Invalid arg from user.\n");
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return -EINVAL;
    }

    if (ka_base_copy_from_user(&module_para, (void *)(uintptr_t)arg,
        sizeof(struct davinci_intf_open_arg)) != 0) {
        log_intf_err("copy_from_user failed. (size=%lu)\n", sizeof(struct davinci_intf_open_arg));
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return -EFAULT;
    }
    module_para.module_name[DAVINIC_MODULE_NAME_MAX - 1] = '\0';
    name_len = strnlen(module_para.module_name, DAVINIC_MODULE_NAME_MAX);
    if ((name_len == 0) || (name_len >= DAVINIC_MODULE_NAME_MAX)) {
        log_intf_err("Length out of range. (name_len=%lu)\n", name_len);
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return -EINVAL;
    }

    ret = drv_ascend_intf_init_file_private(file_private,
        module_para.module_name,
        module_para.device_id);
    if (ret != 0) {
        log_intf_err("Init file_private failed. (module_name=\"%s\"; ret=%d)\n",
            module_para.module_name, ret);
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return ret;
    }

    ret = drv_ascend_intf_open_private(file_private);
    if (ret != 0) {
        log_intf_warn("Call open function detail info. (module_name=\"%s\"; ret=%d)\n",
            file_private->module_name, ret);
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return ret;
    }

    ret = add_module_to_list(&g_davinci_intf_cb, file_private,
        filep, (char *)module_para.module_name);
    if (ret != 0) {
        log_intf_err("add_module_to_list failed. (module_name=\"%s\"; ret=%d)\n",
            module_para.module_name, ret);
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return ret;
    }

    ka_task_mutex_lock(&file_private->fmutex);
    file_private->close_flag = FALSE;
    ka_task_mutex_unlock(&file_private->fmutex);
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);

    return 0;
}

int drv_ascend_intf_ioctl_close_cmd(ka_file_t *filep,
    unsigned int cmd,
    unsigned long arg)
{
    struct davinci_intf_private_stru *file_private = NULL;
    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    file_private = filep->private_data;
    if (file_private == NULL) {
        log_intf_err("Invalid file_private_data.\n");
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return -EBADFD;
    }

    /* child thread call will close file of father thread  */
    if (file_private->owner_pid != current->tgid) {
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return 0;
    }
    ka_task_mutex_lock(&file_private->fmutex);
    file_private->close_flag = TRUE;
    ka_task_mutex_unlock(&file_private->fmutex);
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);
    return 0;
}

int drv_ascend_intf_ioctl_check_module_no_use(
    ka_file_t *filep,
    unsigned int cmd,
    unsigned long arg)
{
    struct davinci_intf_check_no_use_arg module_para = {{0}};
    struct davinci_intf_private_stru *file_private = NULL;
    size_t name_len;

    file_private = filep->private_data;
    if (file_private == NULL) {
        log_intf_err("Invalid file_private_data.\n");
        return -EBADFD;
    }

    if ((void *)arg == NULL) {
        log_intf_err("Invalid arg from user.\n");
        return -EINVAL;
    }

    if (ka_base_copy_from_user(&module_para, (void *)(uintptr_t)arg,
        sizeof(struct davinci_intf_check_no_use_arg)) != 0) {
        log_intf_err("copy_from_user failed. (size=%lu)\n", sizeof(struct davinci_intf_check_no_use_arg));
        return -EFAULT;
    }
    name_len = strnlen(module_para.module_name, DAVINIC_MODULE_NAME_MAX);
    if ((name_len == 0) || (name_len >= DAVINIC_MODULE_NAME_MAX)) {
        log_intf_err("Length out of range. (name_len=%lu)\n", name_len);
        return -EINVAL;
    }
    ka_task_down_read(&g_davinci_intf_cb.cb_sem);
    module_para.status = check_module_file_close(&g_davinci_intf_cb, module_para.module_name);
    ka_task_up_read(&g_davinci_intf_cb.cb_sem);
    if (ka_base_copy_to_user((void *)(uintptr_t)arg, &module_para,
        sizeof(struct davinci_intf_check_no_use_arg)) != 0) {
        log_intf_err("copy_to_user failed. (size=%lu)\n", sizeof(struct davinci_intf_check_no_use_arg));
        return -EFAULT;
    }
    return 0;
}

STATIC long drv_ascend_intf_ioctl_local(ka_file_t *filep,
    unsigned int cmd,
    unsigned long arg)
{
    if (_KA_IOC_NR(cmd) >= DAVINCI_INTF_IOCTL_CMD_MAX_NR) {
        log_intf_err("invalid cmd,out of range. (cmd=%u)\n", _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    if (g_davinci_ioctl_handlers[_KA_IOC_NR(cmd)] == NULL) {
        log_intf_err("invalid cmd,function not defined. (cmd=%u)\n", _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    return g_davinci_ioctl_handlers[_KA_IOC_NR(cmd)](filep, cmd, arg);
}

STATIC long drv_ascend_intf_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct davinci_intf_private_stru *file_private = NULL;
    long ret;
    ASSERT_RET((filep != NULL), (-EFAULT));

    file_private = filep->private_data;
    if (file_private == NULL) {
        log_intf_err("Invalid file_private_data.\n");
        return -EBADFD;
    }

    switch (_KA_IOC_TYPE(cmd)) {
        case DAVINCI_INTF_IOC_MAGIC:
            return drv_ascend_intf_ioctl_local(filep, cmd, arg);
        default:
            if (file_private->fops.unlocked_ioctl == NULL) {
                log_intf_err("File not init. (cmd=%u)\n", _KA_IOC_NR(cmd));
                return -ENODEV;
            }
            ret = drv_davinci_inc_work_count(file_private);
            if (ret != 0) {
                log_intf_err("Call is not allowed in released state. (cmd=%u; ret=%ld)\n",
                    _KA_IOC_NR(cmd),
                    ret);
                return -ENODEV;
            }
            ret = file_private->fops.unlocked_ioctl(&file_private->priv_filep, cmd, arg);
            drv_davinci_dec_work_count(file_private);
            return ret;
    }
}

#ifndef AOS_LLVM_BUILD
STATIC int drv_ascend_intf_mmap(ka_file_t *filep, struct vm_area_struct *vma)
{
    struct davinci_intf_private_stru *file_private = NULL;
    long ret;
    ASSERT_RET((filep != NULL), (-EFAULT));
    ASSERT_RET((vma != NULL), (-EFAULT));

    file_private = filep->private_data;
    if ((file_private == NULL) || (file_private->fops.mmap == NULL)) {
        log_intf_err("Invalid file_private_data.\n");
        return -EBADFD;
    }

    ret = drv_davinci_inc_work_count(file_private);
    if (ret != 0) {
        log_intf_err("Call is not allowed in released state. (ret=%ld)\n",
            ret);
        return -ENODEV;
    }
    ret = file_private->fops.mmap(&file_private->priv_filep, vma);
    drv_davinci_dec_work_count(file_private);

    return ret;
}

STATIC unsigned long drv_ascend_intf_get_unmapped_area(ka_file_t *filep,
    unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags)
{
    if ((flags & KA_MAP_FIXED) != 0) {
        log_intf_err("Not support MAP_FIXED flag. (flags=0x%lx)\n", flags);
        return -EINVAL;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 19, 0)
    return mm_get_unmapped_area(filep, addr, len, pgoff, flags);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
    return mm_get_unmapped_area(current->mm, filep, addr, len, pgoff, flags);
#else
    return current->mm->get_unmapped_area(filep, addr, len, pgoff, flags);
#endif
}

STATIC unsigned int drv_ascend_intf_poll(
    ka_file_t *filep,
    struct poll_table_struct *wait)
{
    struct davinci_intf_private_stru *file_private = NULL;
    long ret;
    ASSERT_RET((filep != NULL), KA_POLLERR);
    ASSERT_RET((wait != NULL), KA_POLLERR);

    file_private = filep->private_data;
    if ((file_private == NULL) || (file_private->fops.poll == NULL)) {
        log_intf_err("Invalid file_private_data.\n");
        return KA_POLLERR;
    }

    ret = drv_davinci_inc_work_count(file_private);
    if (ret != 0) {
        log_intf_err("Call is not allowed in released state. (ret=%ld)\n",
            ret);
        return KA_POLLERR;
    }
    ret = file_private->fops.poll(&file_private->priv_filep, wait);
    drv_davinci_dec_work_count(file_private);
    return ret;
}
#endif

static const ka_file_operations_t g_davinci_intf_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(drv_ascend_intf_open)
    ka_fs_init_f_release(drv_ascend_intf_release)
#ifdef AOS_LLVM_BUILD
    .ioctl = drv_ascend_intf_ioctl,
#else
    ka_fs_init_f_unlocked_ioctl(drv_ascend_intf_ioctl)
    ka_fs_init_f_mmap(drv_ascend_intf_mmap)
    ka_fs_init_f_get_unmapped_area(drv_ascend_intf_get_unmapped_area)
    ka_fs_init_f_poll(drv_ascend_intf_poll)
#endif
};

STATIC int drv_davinci_check_module_init(const char *module_name)
{
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_sub_module_stru *node_info_next = NULL;

    if (ka_list_empty(&g_davinci_intf_cb.module_list) != 0) {
        return FALSE;
    }

    ka_list_for_each_entry_safe(node_info, node_info_next, &g_davinci_intf_cb.module_list, list) {
        if (ka_base_strcmp(node_info->module_name, module_name) == 0) {
            return TRUE;
        }
    }
    return  FALSE;
}

STATIC struct davinci_intf_sub_module_stru *alloc_module_node(
    const char *module_name,
    const ka_file_operations_t *ops)
{
    struct davinci_intf_sub_module_stru *node = NULL;
    int ret;

    node = (struct davinci_intf_sub_module_stru *)dbl_kzalloc(
        sizeof(struct davinci_intf_sub_module_stru),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        log_intf_err("kzalloc failed. (module_name=\"%s\"; size=%lu)\n",
            module_name,
            sizeof(struct davinci_intf_sub_module_stru));
        return NULL;
    }
    ret = strcpy_s(node->module_name, DAVINIC_MODULE_NAME_MAX, module_name);
    if (ret != 0) {
        log_intf_err("strcpy_s error. (module_name=\"%s\"; ret=%d)\n",
            module_name,
            ret);
        goto out_err;
    }
    ret = memcpy_s(&node->ops,
        sizeof(struct file_operations),
        ops,
        sizeof(struct file_operations));
    if (ret != 0) {
        log_intf_err("memcpy_s error. (module_name=\"%s\"; ret=%d)\n",
            module_name,
            ret);
        goto out_err;
    }
    node->valid = VALID;
    return node;
out_err:
    dbl_kfree(node);
    node = NULL;
    return NULL;
}
STATIC int drv_ascend_register_module(
    const char *module_name,
    const ka_file_operations_t *ops, unsigned int free_type, unsigned int open_module_max)
{
    struct davinci_intf_sub_module_stru *node = NULL;

    if (module_name == NULL || ops == NULL
        || ((free_type != DAVINIC_FREE_IN_PARALLEL) && (free_type != DAVINIC_FREE_IN_ORDER))) {
        log_intf_err("Input parameter is null.\n");
        return -EINVAL;
    }

    if (ka_base_strnlen(module_name, DAVINIC_MODULE_NAME_MAX) >= DAVINIC_MODULE_NAME_MAX) {
        log_intf_err("Length out of range. (length=%lu)\n",
            ka_base_strnlen(module_name, DAVINIC_MODULE_NAME_MAX));
        return -EINVAL;
    }

    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    if (drv_davinci_check_module_init(module_name) == TRUE) {
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        log_intf_err("Already init. (module_name=\"%s\")\n", module_name);
        return -EINVAL;
    }
    node = alloc_module_node(module_name, ops);
    if (node == NULL) {
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        log_intf_err("alloc_module_node failed. (module_name=\"%s\")\n",
            module_name);
        return -ENOMEM;
    }
    node->free_type = free_type;
    node->open_module_max = open_module_max;
    if (ka_base_strcmp(node->module_name, DAVINCI_INTF_MODULE_TSDRV) == 0) {
        ka_list_add(&node->list, &g_davinci_intf_cb.module_list);
    } else {
        ka_list_add_tail(&node->list, &g_davinci_intf_cb.module_list);
    }
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);
    return 0;
}
int drv_davinci_register_sub_module_cnt(
    const char *module_name,
    const ka_file_operations_t *ops,
    unsigned int open_module_max)
{
    return drv_ascend_register_module(module_name, ops, DAVINIC_FREE_IN_ORDER, open_module_max);
}
KA_EXPORT_SYMBOL_GPL(drv_davinci_register_sub_module_cnt);

int drv_davinci_register_sub_module(
    const char *module_name,
    const ka_file_operations_t *ops)
{
    return drv_ascend_register_module(module_name, ops, DAVINIC_FREE_IN_ORDER, 0);
}
KA_EXPORT_SYMBOL_GPL(drv_davinci_register_sub_module);

int drv_davinci_register_sub_parallel_module(
    const char *module_name,
    const ka_file_operations_t *ops)
{
    return drv_ascend_register_module(module_name, ops, DAVINIC_FREE_IN_PARALLEL, 0);
}
KA_EXPORT_SYMBOL_GPL(drv_davinci_register_sub_parallel_module);

int drv_ascend_unregister_sub_module(const char *module_name)
{
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_sub_module_stru *node_info_next = NULL;
    if (module_name == NULL) {
        log_intf_err("Input parameter is null.\n");
        return -EINVAL;
    }

    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    if (ka_list_empty(&g_davinci_intf_cb.module_list) != 0) {
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        log_intf_warn("There was no one registered. (module_name=\"%s\")\n", module_name);
        return 0;
    }

    /* Check file all close */
    if (check_module_file_close(&g_davinci_intf_cb, module_name) == FALSE) {
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        log_intf_warn("Some file not close. (module_name=\"%s\")\n", module_name);
        return -EINVAL;
    }
    /* Release module */
    ka_list_for_each_entry_safe(node_info, node_info_next, &g_davinci_intf_cb.module_list, list) {
        if (ka_base_strncmp(node_info->module_name, module_name, DAVINIC_MODULE_NAME_MAX) == 0) {
            ka_list_del(&node_info->list);
            ka_task_up_write(&g_davinci_intf_cb.cb_sem);
            dbl_kfree(node_info);
            node_info = NULL;
            return 0;
        }
    }
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);
    return  0;
}
KA_EXPORT_SYMBOL_GPL(drv_ascend_unregister_sub_module);

int drv_ascend_register_notify(
    const char *module_name,
    const struct notifier_operations *notifier)
{
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_sub_module_stru *node_info_next = NULL;
    int ret;

    if (module_name == NULL) {
        log_intf_err("Input parameter is null.\n");
        return -EINVAL;
    }

    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    /* replace module fops */
    ka_list_for_each_entry_safe(node_info, node_info_next, &g_davinci_intf_cb.module_list, list) {
        if (ka_base_strncmp(node_info->module_name, module_name, DAVINIC_MODULE_NAME_MAX) == 0) {
            if (node_info->notifier.notifier_call != NULL) {
                log_intf_warn("duplicate register notify. (module_name=\"%s\")\n", module_name);
            }
            ret = memcpy_s(&node_info->notifier, sizeof(struct notifier_operations),
                notifier, sizeof(struct notifier_operations));
            ka_task_up_write(&g_davinci_intf_cb.cb_sem);
            if (ret != 0) {
                log_intf_err("memcpy_s error. (module_name=\"%s\"; ret=%d)\n", module_name, ret);
                return -EINVAL;
            }
            return 0;
        }
    }
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);
    log_intf_err("cannot find %s module node, replace failed.\n", module_name);
    return -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(drv_ascend_register_notify);

int drv_ascend_unregister_notify(const char *module_name)
{
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_sub_module_stru *node_info_next = NULL;

    if (module_name == NULL) {
        log_intf_err("Input parameter is null.\n");
        return -EINVAL;
    }

    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    /* replace module fops */
    ka_list_for_each_entry_safe(node_info, node_info_next, &g_davinci_intf_cb.module_list, list) {
        if (ka_base_strncmp(node_info->module_name, module_name, DAVINIC_MODULE_NAME_MAX) == 0) {
            if (node_info->notifier.notifier_call == NULL) {
                log_intf_warn("duplicate unregister notify. (module_name=\"%s\")\n", module_name);
            }
            (void)memset_s(&node_info->notifier, sizeof(struct notifier_operations),
                0, sizeof(struct notifier_operations));
            ka_task_up_write(&g_davinci_intf_cb.cb_sem);
            return 0;
        }
    }
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);
    log_intf_err("cannot find module node, unregister failed. (module_name=\"%s\")\n", module_name);
    return -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(drv_ascend_unregister_notify);

int drv_ascend_replace_sub_module_fops(const char *module_name,
    const ka_file_operations_t *ops)
{
    struct davinci_intf_sub_module_stru *node_info = NULL;
    struct davinci_intf_sub_module_stru *node_info_next = NULL;
    int ret;

    if (module_name == NULL) {
        log_intf_err("Input parameter is null.\n");
        return -EINVAL;
    }

    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    /* replace module fops */
    ka_list_for_each_entry_safe(node_info, node_info_next, &g_davinci_intf_cb.module_list, list) {
        if (ka_base_strncmp(node_info->module_name, module_name, DAVINIC_MODULE_NAME_MAX) == 0) {
            ret = memcpy_s(&node_info->ops, sizeof(struct file_operations), ops,
                sizeof(struct file_operations));
            ka_task_up_write(&g_davinci_intf_cb.cb_sem);
            if (ret != 0) {
                log_intf_err("memcpy_s error. (module_name=\"%s\"; ret=%d)\n", module_name, ret);
                return -EINVAL;
            }
            return 0;
        }
    }
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);
    log_intf_err("cannot find %s module node, replace failed.\n", module_name);
    return -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(drv_ascend_replace_sub_module_fops);

u32 drv_davinci_get_device_id(const ka_file_t *filep)
{
    struct davinci_intf_private_stru *file_private = NULL;

    file_private = ka_container_of(filep, struct davinci_intf_private_stru, priv_filep);
    return file_private->device_id;
}
KA_EXPORT_SYMBOL_GPL(drv_davinci_get_device_id);

int ascend_intf_report_process_status(pid_t pid, unsigned int status)
{
    struct davinci_intf_process_stru *proc = NULL;

    ka_task_down_write(&g_davinci_intf_cb.cb_sem);
    proc = get_process_entry_latest(&g_davinci_intf_cb, pid);
    if (proc == NULL) {
        log_intf_err("Process not init,proc is NULL. (pid=%d)\n", pid);
        ka_task_up_write(&g_davinci_intf_cb.cb_sem);
        return -ESRCH;
    }
    /* Clear status */
    if ((status & DAVINCI_INTF_PROCESS_CLEAR_STATUS) != 0) {
        proc->status &= ~status;
    } else {
        proc->status |= status;
    }
    ka_task_up_write(&g_davinci_intf_cb.cb_sem);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(ascend_intf_report_process_status);

STATIC int ascend_intf_get_process_status(pid_t pid, unsigned int *status)
{
    struct davinci_intf_process_stru *proc = NULL;

    ka_task_down_read(&g_davinci_intf_cb.cb_sem);
    proc = get_process_entry_latest(&g_davinci_intf_cb, pid);
    if (proc == NULL) {
        log_intf_err("Process not init,proc is NULL. (pid=%d)\n", pid);
        ka_task_up_read(&g_davinci_intf_cb.cb_sem);
        return -ESRCH;
    }
    *status = proc->status;
    ka_task_up_read(&g_davinci_intf_cb.cb_sem);

    return 0;
}

bool davinci_intf_confirm_user(void)
{
#ifdef AOS_LLVM_BUILD
    return true;
#else
    unsigned int i = 0, valid_flag = false;
    const struct cred *cred = current_cred();

    for (i = 0; i < DAVINIC_CONFIRM_USER_NUM; i++) {
        /* group id not init,means don't need confirm */
        if (g_authorized_group_id[i] == DAVINIC_NOT_CONFIRM_USER_ID) {
            return true;
        }
        /* if in the group, it will return 1, otherwise return 0. */
        if (ka_system_in_group_p(*(kgid_t *)&g_authorized_group_id[i]) != 0) {
            valid_flag = true;
        }
    }
    if (valid_flag == true || cred->uid.val == DAVINIC_ROOT_USER_ID) {
        return true;
    }
    return false;
#endif
}
KA_EXPORT_SYMBOL_GPL(davinci_intf_confirm_user);

u32 davinci_intf_get_manage_group(void)
{
    return g_authorized_group_id[1]; /* 0:HwHiAiUser; 1:HwDmUser; 2:HwBaseUser. */
}
KA_EXPORT_SYMBOL_GPL(davinci_intf_get_manage_group);

ka_device_t *davinci_intf_get_owner_device(void)
{
    return g_davinci_intf_cb.device;
}
KA_EXPORT_SYMBOL_GPL(davinci_intf_get_owner_device);

int ascend_intf_report_device_status(unsigned int device_id, unsigned int status)
{
    if (device_id >= MAX_DEVICE_COUNT) {
        log_intf_err("Input parameter is error. (device_id=%u)\n", device_id);
        return -EINVAL;
    }
    /* Clear status */
    if ((status & DAVINCI_INTF_DEVICE_CLEAR_STATUS) != 0) {
        g_davinci_intf_cb.device_status[device_id] &= ~status;
    } else {
        g_davinci_intf_cb.device_status[device_id] |= status;
    }
    return 0;
}
KA_EXPORT_SYMBOL_GPL(ascend_intf_report_device_status);

int ascend_intf_get_status(struct ascend_intf_get_status_para para, unsigned int *status)
{
    if (((para.type == DAVINCI_STATUS_TYPE_DEVICE) && (para.para.device_id >= MAX_DEVICE_COUNT)) || (status == NULL)) {
        log_intf_err("Input parameter is error. (device_id=%u)\n", para.para.device_id);
        return -EINVAL;
    }

    switch (para.type) {
        case DAVINCI_STATUS_TYPE_PROCESS:
            return  ascend_intf_get_process_status(para.para.process_id, status);
        case DAVINCI_STATUS_TYPE_DEVICE:
            *status = g_davinci_intf_cb.device_status[para.para.device_id];
            return 0;
        default:
            log_intf_err("Input parameter is error. (type=%u)\n", para.type);
            return -EINVAL;
    }
}
KA_EXPORT_SYMBOL_GPL(ascend_intf_get_status);

int ascend_intf_is_pid_init(pid_t process_id, const char *module_name)
{
    struct davinci_intf_process_stru *proc = NULL;
    ka_task_down_read(&g_davinci_intf_cb.cb_sem);
    /* check process_pid open status */
    proc = get_process_entry_latest(&g_davinci_intf_cb, process_id);
    if (proc == NULL) {
        ka_task_up_read(&g_davinci_intf_cb.cb_sem);
        return false;
    }
    /* if module_name not null, check module open status */
    if ((module_name != NULL) && (check_module_file_close_in_process(proc, module_name) == TRUE)) {
        ka_task_up_read(&g_davinci_intf_cb.cb_sem);
        return false;
    }
    ka_task_up_read(&g_davinci_intf_cb.cb_sem);
    return true;
}
KA_EXPORT_SYMBOL_GPL(ascend_intf_is_pid_init);

bool ascend_intf_is_restrict_access(ka_file_t *filep)
{
    if ((filep == NULL) || (filep->f_path.dentry == NULL)) {
        log_intf_warn("Can't get dev name, restricte access.\n");
        return true;
    }

    if (ka_base_strcmp(filep->f_path.dentry->d_iname, DAVINCI_INTF_NPU_DEV_CUST_NAME) == 0) {
        return true;
    }

    return false;
}
KA_EXPORT_SYMBOL_GPL(ascend_intf_is_restrict_access);

STATIC int drv_davinci_intf_recycle_wq_init(void)
{
    int i;
    ka_base_atomic64_set(&g_recycle_wq_idx, 0);
    for (i = 0; i < MAX_RECYCLE_WORKQUEUE; i++) {
#ifdef CFG_BIND_CPU_ENABLE
        g_recycle_wq[i] = alloc_workqueue("intf_cb_recycle", WQ_UNBOUND, 1);
#else
        g_recycle_wq[i] = ka_task_create_workqueue("intf_cb_recycle");
#endif
        if (g_recycle_wq[i] == NULL) {
            log_intf_err("davinci recycle workqueue is NULL.\n");
            return -ENOMEM;
        }
#ifdef CFG_BIND_CPU_ENABLE
        intf_recycle_workqueue_affinity(g_recycle_wq[i]);
#endif
    }
    return 0;
}

STATIC void drv_davinci_intf_recycle_wq_uninit(void)
{
    int i;
    for (i = 0; i < MAX_RECYCLE_WORKQUEUE; i++) {
        if (g_recycle_wq[i] != NULL) {
            ka_task_flush_workqueue(g_recycle_wq[i]);
            ka_task_destroy_workqueue(g_recycle_wq[i]);
        }
        g_recycle_wq[i] = NULL;
    }
}

STATIC int drv_davinci_intf_cb_init(struct davinci_intf_stru *cb)
{
    int ret;
    ka_task_init_rwsem(&cb->cb_sem);
    ka_base_atomic_set(&cb->count, 0);
    cb->device = NULL;
    KA_INIT_LIST_HEAD(&cb->module_list);
    KA_INIT_LIST_HEAD(&cb->process_list);
    init_ctrl_cpumask(cb);
    ret = drv_davinci_intf_recycle_wq_init();
    if (ret != 0) {
        drv_davinci_intf_recycle_wq_uninit();
        return ret;
    }
    return 0;
}

STATIC int drv_davinci_intf_cb_destory(struct davinci_intf_stru *cb)
{
    ka_task_down_write(&cb->cb_sem);
#if !defined(CFG_HOST_ENV) && !defined(AOS_LLVM_BUILD)
    if (cpumask_available(cb->cpumask)) {
        free_cpumask_var(cb->cpumask);
    }

#endif
    ka_base_atomic_set(&cb->count, 0);
    cb->device = NULL;
    destroy_process_list(cb);
    ka_task_up_write(&cb->cb_sem);
    return 0;
}

STATIC int __init drv_ascend_intf_init(void)
{
    int ret;
    ret = drv_davinci_intf_cb_init(&g_davinci_intf_cb);
    if (ret != 0) {
        return ret;
    }
#ifdef AOS_LLVM_BUILD
    ret = register_driver(DAVINCI_INTF_FULL_DEV_NAME, &g_davinci_intf_fops, DAVINIC_NONE_ROOT_ACCESS, NULL);
#else
    ret = drv_ascend_intf_setup_cdev(&g_davinci_intf_cb, &g_davinci_intf_fops);
#endif
    if (ret != 0) {
        log_intf_err("drv_ascend_intf_setup_cdev failed. (ret=%d)\n", ret);
        goto cleanup1;
    }
    return 0;
cleanup1:
    drv_davinci_intf_cb_destory(&g_davinci_intf_cb);
    return ret;
}

STATIC void __exit drv_davinci_intf_exit(void)
{
#ifdef AOS_LLVM_BUILD
    unregister_driver(DAVINCI_INTF_FULL_DEV_NAME);
#else
    (void)drv_ascend_intf_cleanup_cdev(&g_davinci_intf_cb);
    drv_davinci_intf_recycle_wq_uninit();
#endif
    (void)drv_ascend_intf_release_process();
    ka_base_atomic_set(&g_davinci_intf_cb.count, 0);
#if (!defined(CFG_HOST_ENV)) && (!defined(AOS_LLVM_BUILD))
    if (cpumask_available(g_davinci_intf_cb.cpumask)) {
        free_cpumask_var(g_davinci_intf_cb.cpumask);
    }
#endif
    g_davinci_intf_cb.device = NULL;
}

ka_module_init(drv_ascend_intf_init);
ka_module_exit(drv_davinci_intf_exit);

MODULE_DESCRIPTION("Ascend host device interface");
KA_MODULE_LICENSE("GPL");

#else
int drv_davinci_unset_file_ops(void)
{
    return 0;
}

#endif

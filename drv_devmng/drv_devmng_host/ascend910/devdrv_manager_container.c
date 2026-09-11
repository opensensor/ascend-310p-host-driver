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

#include "ka_errno_pub.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"

#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_container.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_pcie.h"
#include "dbl/uda.h"
#include "dbl/runenv_config.h"
#ifdef CFG_FEATURE_DEVICE_SHARE
#include "devdrv_manager_dev_share.h"
#endif

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/cgroup.h>
#endif

#include "dbl/uda.h"

ka_mnt_namespace_t *devdrv_manager_get_host_mnt_ns(void)
{
    return init_task.nsproxy->mnt_ns;
}

int devdrv_devpid_container_convert(int *ipid)
{
#ifndef UT_VCAST
    ka_struct_pid_t *kpid = NULL;

    if (ipid == NULL) {
        devdrv_drv_err("Parameter invalid.\n");
        return -EINVAL;
    }

    if (!run_in_docker()) {
        /* Physical machines do not need to be converted. */
        return 0;
    }

    ka_task_rcu_read_lock_bh();
    kpid = ka_task_find_pid_ns(*ipid, ka_task_get_pid_ns_for_task(&init_task));
    if (kpid != NULL) {
        *ipid = kpid->numbers[kpid->level].nr;
        ka_task_rcu_read_unlock_bh();
        return 0;
    } else {
        ka_task_rcu_read_unlock_bh();
        return -ESRCH;
    }
#endif
}

#if defined(CFG_HOST_ENV) || defined(CFG_FEATURE_BIND_TGID)
int devdrv_get_tgid_by_pid(int pid, int *tgid)
{
    ka_task_struct_t *tsk = NULL;
    ka_struct_pid_t *pro_id = NULL;
    int ret;

    if (tgid == NULL) {
        devdrv_drv_err("The tgid is NULL.\n");
        return -EINVAL;
    }

    pro_id = ka_task_find_get_pid(pid);
    if (pro_id == NULL) {
        devdrv_drv_err("Failed to find get pid. (pid=%d)\n", pid);
        return -EINVAL;
    }

    /*
    * The value of tsk is NULL, which means the PID and current process are not in the same pid_ns.
    * Even if we find it, it doesn't mean it really is, because each container may has its own pid_ns.
    */
    tsk = ka_task_get_pid_task(pro_id, KA_PIDTYPE_PID);
    if (tsk == NULL) {
        devdrv_drv_err("Failed to find task struct in current process's pid_ns. (pid=%d)\n", pid);
        ret = -EINVAL;
        goto OUT_PUT_PID;
    }

    /* If not in container, it return tgid directly. */
    ret = devdrv_manager_container_is_in_container();
    if (ret < 0) {
        devdrv_drv_err("Failed to invoke devdrv_is_in_container. (ret=%d)\n", ret);
        goto OUT;
    } else if (ret != true) {
        *tgid = tsk->tgid;
        ret = 0;
        goto OUT;
    }

    /* If in container, the PID mnt_ns will be equal to the current mnt_ns. */
    if ((tsk->nsproxy != NULL) && (tsk->nsproxy->mnt_ns == current->nsproxy->mnt_ns)) {
        *tgid = tsk->tgid;
        ret = 0;
        goto OUT;
    }

    ret = -EINVAL;
    devdrv_drv_err("The PID does not exist in the container. (pid=%d)\n", pid);
OUT:
    ka_task_put_task_struct(tsk);
OUT_PUT_PID:
    ka_task_put_pid(pro_id);
    return ret;
}
#endif

bool devdrv_manager_container_is_admin(void)
{
    return dbl_current_is_admin();
}
#endif // DEVDRV_MANAGER_HOST_UT_TEST
/**
 * Check the process is running in the physical machine or in container
 */
bool devdrv_manager_container_is_host_system(ka_mnt_namespace_t *mnt_ns)
{
    if (mnt_ns == devdrv_manager_get_host_mnt_ns()) {
        return true;
    }

    return false;
}
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
#define DEVMNG_MAX_TEXT_LENGTH 34
#define DEVMNG_UNKNOWN_CONTAINER_ID 0xFFFFFFFFFFFFUL
#define HEXADECIMAL 16
#define CONTAINER_ID_LENGTH 12

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
STATIC int new_cpuset_read(char *buf, size_t count)
{
    struct cgroup_subsys_state *css = NULL;
    int retval;

    css = task_get_css(current, cpuset_cgrp_id);
    if (css == NULL) {
        devdrv_drv_err("task get css failed.\n");
        return -EINVAL;
    }

    retval = cgroup_path_ns(css->cgroup, buf,
        count, current->nsproxy->cgroup_ns);
    ka_task_css_put(css);
    if (retval <= 0) {
        devdrv_drv_err("read buf failed. (retval=%d)\n", retval);
        return -EINVAL;
    }

    return count;
}
#endif

static void devdrv_manager_get_container_id_from_hostname(unsigned long long *container_id)
{
    char hostname_file_text[DEVMNG_MAX_TEXT_LENGTH] = {0};
    ka_file_t *fp = NULL;
    loff_t pos = 0;
    char container_id_string[DEVMNG_MAX_TEXT_LENGTH] = {0};
    int ret;

    fp = ka_fs_filp_open("/etc/hostname", O_RDONLY, 0);
    if (KA_IS_ERR(fp)) {
        devdrv_drv_err("open hostname file failed. (err=%ld)\n", KA_PTR_ERR(fp));
        return;
    }

    ret = (int)ka_fs_kernel_read(fp, hostname_file_text, DEVMNG_MAX_TEXT_LENGTH, &pos);

    (void)ka_fs_filp_close(fp, NULL);
    if ((ret <= 0) || (ret > DEVMNG_MAX_TEXT_LENGTH)) {
        devdrv_drv_err("get hostname file context failed. (ret=%d)\n", ret);
        return;
    }

    hostname_file_text[DEVMNG_MAX_TEXT_LENGTH - 1] = '\0';
    ret = strncpy_s(container_id_string, DEVMNG_MAX_TEXT_LENGTH, hostname_file_text, CONTAINER_ID_LENGTH);
    if (ret != 0) {
        devdrv_drv_err("strncpy_s container id failed. (ret=%d)\n", ret);
        return;
    }

    ret = ka_base_kstrtoull(container_id_string, HEXADECIMAL, container_id);
    if (ret < 0) {
        devdrv_drv_debug("kstrtoul failed. (ret=%d)\n", ret);
        return;
    }

    return;
}

STATIC void devdrv_manager_get_container_id(unsigned long long *container_id)
{
    char cpuset_file_text[DEVMNG_MAX_TEXT_LENGTH] = {0};
    ka_file_t *fp = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
    loff_t pos = 0;
#endif
    int ret;
    char *res = NULL;
    char container_id_string[DEVMNG_MAX_TEXT_LENGTH] = {0};
    size_t container_id_len = 12;

    if (container_id == NULL) {
        devdrv_drv_err("input container id is NULL\n");
        return;
    }

    /* set default container id, which means we can't get real container id. */
    *container_id = DEVMNG_UNKNOWN_CONTAINER_ID;

    /**
     * /proc/[pid]/cpuset
     * inside docker the context of this file is
     * "/docker/<container_id>" or "/system.slice/docker-<container_id>"
     */
    fp = ka_fs_filp_open("/proc/self/cpuset", O_RDONLY, 0);
    if (KA_IS_ERR(fp)) {
        devdrv_drv_err("open cpuset file failed, err = %ld.\n", KA_PTR_ERR(fp));
        return;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    ret = new_cpuset_read(cpuset_file_text, DEVMNG_MAX_TEXT_LENGTH);
#else
    ret = kernel_read(fp, pos, cpuset_file_text, DEVMNG_MAX_TEXT_LENGTH);
#endif
    (void)ka_fs_filp_close(fp, NULL);
    if ((ret <= 0) || (ret > DEVMNG_MAX_TEXT_LENGTH)) {
        devdrv_drv_err("get cpuset file context failed, ret = %d.\n", ret);
        return;
    }

    cpuset_file_text[DEVMNG_MAX_TEXT_LENGTH - 1] = '\0';
    res = ka_base_strstr(cpuset_file_text, "docker");
    if (res == NULL) {
        devdrv_drv_debug("cannot find docker in /proc/self/cpuset.\n");
        (void)devdrv_manager_get_container_id_from_hostname(container_id);
        return;
    }

    ret = strncpy_s(container_id_string, DEVMNG_MAX_TEXT_LENGTH, res + ka_base_strlen("docker") + 1, container_id_len);
    if (ret != 0) {
        devdrv_drv_err("strncpy_s container id failed. (ret=%d)\n", ret);
        return;
    }

    ret = ka_base_kstrtoull(container_id_string, HEXADECIMAL, container_id);
    if (ret < 0) {
        devdrv_drv_debug("kstrtoul failed. (ret=%d)\n", ret);
        return;
    }

    return;
}

STATIC int devdrv_manager_container_check_current(void)
{
    /* current->nsproxy is NULL when the release function is called */
    if (current == NULL || current->nsproxy == NULL || current->nsproxy->mnt_ns == NULL) {
        devdrv_drv_warn("(current == NULL) is %d, (current->nsproxy == NULL) is %d\n",
            (current == NULL), ((current == NULL) ? (-EINVAL) : (current->nsproxy == NULL)));
        return -EINVAL;
    }

    return 0;
}

int devdrv_manager_container_get_docker_id(u32 *docker_id)
{
    int ret;

    if (devdrv_manager_container_is_in_container() == false) {
        return -EPERM;
    }

    ret = uda_get_cur_ns_id(docker_id);
    if (ret != 0) {
        devdrv_drv_err("Query ns id failed\n");
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_manager_container_get_docker_id);

#define DMS_MNG_NOTIFIER "dms_mng"
static int dms_mng_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (action != UDA_UNOCCUPY && devdrv_manager_container_is_in_container() == false) {
        return 0;
    }

    if (action == UDA_OCCUPY) {
        u64 container_id;
        devdrv_manager_get_container_id(&container_id);
        ret = uda_set_dev_ns_identify(udevid, container_id);
        if (!uda_is_phy_dev(udevid)) {
            (void)dev_mnt_vdevice_add_inform(udevid, VDEV_BIND, current->nsproxy->mnt_ns, container_id);
            dev_mnt_vdevice_inform();
        }
    } else if (action == UDA_UNOCCUPY) {
        if (!uda_is_phy_dev(udevid)) {
            (void)dev_mnt_vdevice_add_inform(udevid, VDEV_UNBIND, NULL, 0);
            dev_mnt_vdevice_inform();
        }
    }

    devdrv_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

static int dms_mng_init_uda_notifier(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_local_real_entity_type_pack(&type); /* for device */
    ret = uda_real_virtual_notifier_register(DMS_MNG_NOTIFIER, &type, UDA_PRI1, dms_mng_notifier_func);
    if (ret != 0) {
        devdrv_drv_err("uda_real_virtual_notifier_register failed\n");
        return ret;
    }

    uda_davinci_near_real_entity_type_pack(&type); /* for host */
    ret = uda_real_virtual_notifier_register(DMS_MNG_NOTIFIER, &type, UDA_PRI1, dms_mng_notifier_func);
    if (ret != 0) {
        devdrv_drv_err("uda_real_virtual_notifier_register failed\n");
        return ret;
    }

    return 0;
}

static void dms_mng_uninit_uda_notifier(void)
{
    struct uda_dev_type type;

    uda_davinci_local_real_entity_type_pack(&type);
    (void)uda_real_virtual_notifier_unregister(DMS_MNG_NOTIFIER, &type);
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_real_virtual_notifier_unregister(DMS_MNG_NOTIFIER, &type);
}

int devdrv_manager_container_table_init(struct devdrv_manager_info *manager_info)
{
    return dms_mng_init_uda_notifier();
}

void devdrv_manager_container_table_exit(struct devdrv_manager_info *manager_info)
{
    dms_mng_uninit_uda_notifier();
}

STATIC int devdrv_manager_container_get_bare_pid(struct devdrv_container_para *cmd)
{
    pid_t pid;
    int ret;

    if (cmd->para.out == NULL) {
        devdrv_drv_err("invalid parameter\n");
        return -EINVAL;
    }
    pid = current->pid;
    ret = copy_to_user_safe(cmd->para.out, &pid, sizeof(pid_t));
    if (ret != 0) {
        devdrv_drv_err("pid = %d, copy to user failed: %d.\n", pid, ret);
        return -ENOMEM;
    }

    return 0;
}

STATIC int devdrv_manager_container_get_bare_tgid(struct devdrv_container_para *cmd)
{
    pid_t tgid;
    int ret;

    if (cmd->para.out == NULL) {
        devdrv_drv_err("invalid parameter\n");
        return -EINVAL;
    }
    tgid = current->tgid;
    ret = copy_to_user_safe(cmd->para.out, &tgid, sizeof(pid_t));
    if (ret != 0) {
        devdrv_drv_err("tgid = %d, copy to user failed: %d.\n", tgid, ret);
        return -ENOMEM;
    }

    return 0;
}

/**
 * @brief Determine whether the current running environment is a normal container
 * @attention Not include admin container
 */
int devdrv_manager_container_is_in_container(void)
{
    ka_mnt_namespace_t *current_mnt = NULL;
    ka_mnt_namespace_t *host_mnt = NULL;
    int is_in;

    if (devdrv_manager_container_check_current() != 0) {
        return false;
    }

    current_mnt = current->nsproxy->mnt_ns;
    host_mnt = devdrv_manager_get_host_mnt_ns();
    if (host_mnt == NULL) {
        is_in = false;
    } else if ((current_mnt != host_mnt) && (!devdrv_manager_container_is_admin())) {
        is_in = true;
    } else {
        is_in = false;
    }

    return is_in;
}
EXPORT_SYMBOL_GPL(devdrv_manager_container_is_in_container);

int devdrv_manager_container_is_in_admin_container(void)
{
    if (devdrv_manager_container_check_current() != 0) {
        return false;
    }

    if (devdrv_manager_container_is_host_system(current->nsproxy->mnt_ns)) {
        return false;
    }

    if (devdrv_manager_container_is_admin()) {
        return true;
    }

    return false;
}
EXPORT_SYMBOL(devdrv_manager_container_is_in_admin_container);

int devdrv_manager_container_check_devid_in_container(u32 devid, pid_t hostpid)
{
    return uda_proc_can_access_udevid(hostpid, devid) ? 0 : -EINVAL;
}
EXPORT_SYMBOL(devdrv_manager_container_check_devid_in_container);
int devdrv_manager_container_check_devid_in_container_ns(u32 devid, ka_task_struct_t*task)
{
    bool ret = (task == current) ? uda_can_access_udevid(devid) : uda_proc_can_access_udevid(task->tgid, devid);
    return ret ? 0 : -EINVAL;
}
EXPORT_SYMBOL(devdrv_manager_container_check_devid_in_container_ns);

STATIC int (*CONST devdrv_manager_container_process_handler[DEVDRV_CONTAINER_MAX_CMD])(
    struct devdrv_container_para *cmd) = {
        [DEVDRV_CONTAINER_GET_BARE_PID] = devdrv_manager_container_get_bare_pid,
        [DEVDRV_CONTAINER_GET_BARE_TGID] = devdrv_manager_container_get_bare_tgid,
    };

int devdrv_manager_container_process(ka_file_t *filep, unsigned long arg)
{
    struct devdrv_container_para container_cmd;
    int ret;

    if (!arg || (filep == NULL) || (filep->private_data == NULL)) {
        devdrv_drv_err("arg = %lu, filep = %pK\n", arg, filep);
        return -EINVAL;
    }

    ret = copy_from_user_safe(&container_cmd, (void *)((uintptr_t)arg), sizeof(struct devdrv_container_para));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user return error: %d.\n", ret);
        return ret;
    }

    if (container_cmd.para.cmd >= DEVDRV_CONTAINER_MAX_CMD) {
        devdrv_drv_err("invalid input container process cmd: %u.\n", container_cmd.para.cmd);
        return -EINVAL;
    }

    if (devdrv_manager_container_process_handler[container_cmd.para.cmd] == NULL) {
        devdrv_drv_err("not supported cmd: %u.\n", container_cmd.para.cmd);
        return -EINVAL;
    }

    ret = devdrv_manager_container_process_handler[container_cmd.para.cmd](&container_cmd);

    return ret;
}
#endif // DEVDRV_MANAGER_HOST_UT_TEST

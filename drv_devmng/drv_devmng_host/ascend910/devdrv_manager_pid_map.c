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

#ifdef CFG_HOST_ENV
    #include <linux/pid_namespace.h>
#endif

#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "securec.h"
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    #ifndef CFG_HOST_ENV
    #include "drv_whitelist.h"
    #endif
#endif
#include "dbl/uda.h"
#include "dbl/runenv_config.h"
#include "devdrv_pcie.h"
#include "devdrv_manager.h"
#include "ascend_hal_error.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_manager_pid_map.h"
#include "devdrv_manager_container.h"
#include "devdrv_manager_common.h"
#include "devdrv_interface.h"
#include "davinci_api.h"

extern struct devdrv_manager_info *dev_manager_info;

#if (!defined (DEVMNG_UT)) && (!defined (DEVDRV_MANAGER_HOST_UT_TEST))
#if defined(CFG_HOST_ENV)
STATIC int get_pid_start_time(pid_t pid, u64* start_time)
{
    ka_struct_pid_t *pid_struct;
    ka_task_struct_t *task;

    pid_struct = ka_task_find_get_pid(pid);
    if (pid_struct == NULL) {
        return false;
    }

    ka_task_rcu_read_lock();
    task = pid_task(pid_struct, PIDTYPE_PID);
    if (task == NULL) {
        ka_task_rcu_read_unlock();
        ka_task_put_pid(pid_struct);
        return false;
    }
    *start_time = ka_task_get_starttime(task);
    ka_task_rcu_read_unlock();
    ka_task_put_pid(pid_struct);

    return true;
}
#endif

STATIC struct devdrv_process_sign *devdrv_find_process_sign(struct devdrv_manager_info *d_info,
    pid_t hostpid)
{
    struct devdrv_process_sign *proc_sign = NULL;
    u32 key;

    key = (u32)hostpid & DEVDRV_PROC_HASH_TABLE_MASK;
    /*lint -e666 */
    ka_hash_for_each_possible(d_info->proc_hash_table, proc_sign, link, key) {
        if (proc_sign->hostpid == hostpid) {
            return proc_sign;
        }
    }
    return NULL;
}

STATIC struct devdrv_process_sign *devdrv_create_process_sign(struct devdrv_manager_info *d_info,
    struct devdrv_ioctl_para_bind_host_pid para_info)
{
    struct devdrv_process_sign *d_sign = NULL;
    int ret;
    enum devdrv_process_type k;
    int i, j;

    d_sign = dbl_vmalloc(sizeof(struct devdrv_process_sign), GFP_KERNEL | __KA_GFP_ZERO | __GFP_ACCOUNT, PAGE_KERNEL);
    if (d_sign == NULL) {
        return NULL;
    }
    d_sign->hostpid = para_info.host_pid;
    d_sign->cp_count = 0;
    d_sign->sync_proc_cnt = 0;
    for (i = 0; i < PID_MAP_DEVNUM; i++) {
        for (j = 0; j < VFID_NUM_MAX; j++) {
            for (k = DEVDRV_PROCESS_CP1; k < DEVDRV_PROCESS_CPTYPE_MAX; k++) {
                d_sign->devpid[i][j][k].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[i][j][k].pid_start_time = 0;
            }
        }
    }

    for (i = 0; i < DEVMNG_USER_PROC_MAX; i++) {
        d_sign->user_proc_device[i].valid = 0;
        d_sign->user_proc_host[i].valid = 0;
        d_sign->user_proc_host[i].dev_num = 0;
        d_sign->user_proc_host[i].start_time = 0;
    }

    ret = strcpy_s(d_sign->sign, PROCESS_SIGN_LENGTH, para_info.sign);
    if (ret != 0) {
        dbl_vfree(d_sign);
        d_sign = NULL;
        return NULL;
    }

    devdrv_drv_debug("Create master pid ctx. (master_pid=%d)\n", para_info.host_pid);

    return d_sign;
}

static int devdrv_get_cur_run_side(void)
{
#ifdef CFG_HOST_ENV
    return HOST_SIDE;
#else
    return DEVICE_SIDE;
#endif
}

static struct devdrv_process_user_info *devdrv_get_user_proc(struct devdrv_process_sign *d_sign, int slave_side)
{
    if (slave_side == DEVICE_SIDE) {
        return d_sign->user_proc_device;
    } else {
        return d_sign->user_proc_host;
    }
}

/* side: host 1 device 0 */
STATIC struct devdrv_process_user_info *devdrv_query_user_proc(struct devdrv_process_sign *d_sign,
    int slave_side, pid_t devpid)
{
    struct devdrv_process_user_info *user_proc = devdrv_get_user_proc(d_sign, slave_side);
    int i;

    for (i = 0; i < DEVMNG_USER_PROC_MAX; i++) {
        if ((user_proc[i].valid == 1) && (user_proc[i].pid == devpid)) {
            return &user_proc[i];
        }
    }

    return NULL;
}

STATIC int set_udevid_to_d_info(u32 dev_num, u32* udevids, struct devdrv_process_user_info *user_proc, int idx)
{
    int j;

    if (dev_num > PID_MAP_DEVNUM || dev_num == 0) {
        return -EINVAL;
    }

    user_proc[idx].dev_num = dev_num;
    for (j = 0; j < dev_num; j++) {
        user_proc[idx].udevids[j] = udevids[j];
    }

    return 0;
}
#ifdef CFG_HOST_ENV
STATIC int get_udevid_and_dev_num(u32* dev_num, u32* udevids, pid_t slave_pid,
    struct devdrv_ioctl_para_bind_host_pid *para_info)
{
    struct devdrv_process_sign *d_sign = NULL;
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_user_info *host_user_proc = NULL;
    int j;

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    d_sign = devdrv_find_process_sign(d_info, para_info->host_pid);
    if (d_sign == NULL) {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_err("Can not find d_sign. (master_pid=%d)\n", para_info->host_pid);
        return -EINVAL;
    }

    host_user_proc = devdrv_query_user_proc(d_sign, HOST_SIDE, slave_pid);
    if (host_user_proc == NULL) {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_err("Can not find host_user_proc. (slave_pid=%d)\n", slave_pid);
        return -EINVAL;
    }

    if (host_user_proc->dev_num == 0 || host_user_proc->dev_num > PID_MAP_DEVNUM) {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_err("Invalid para. (dev_num=%u)\n", host_user_proc->dev_num);
        return -EINVAL;
    }

    *dev_num = host_user_proc->dev_num;
    for (j = 0; j < host_user_proc->dev_num; j++) {
        udevids[j] = host_user_proc->udevids[j];
    }

    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
    return 0;
}
#endif
static bool devdrv_is_update_slave_proc_num(int slave_side)
{
    return (devdrv_get_cur_run_side() == slave_side);
}

struct devdrv_curr_ns_dev_info {
    unsigned int dev_num;
    unsigned int udevid_list[PID_MAP_DEVNUM];
};

static int devdrv_get_curr_ns_dev_info(struct devdrv_curr_ns_dev_info *curr_dev_info)
{
    int ret;

    curr_dev_info->dev_num = uda_get_cur_ns_dev_num();
    if (curr_dev_info->dev_num == 0 || curr_dev_info->dev_num > PID_MAP_DEVNUM) {
        devdrv_drv_err("Get device number failed. (dev_num=%u)\n", curr_dev_info->dev_num);
        return -EINVAL;
    }

    ret = uda_get_cur_ns_udevids(curr_dev_info->udevid_list, curr_dev_info->dev_num);
    if (ret != 0) {
        devdrv_drv_err("Get device list failed. (dev_num=%u; ret=%d)\n", curr_dev_info->dev_num, ret);
        return ret;
    }
    return 0;
}

static int devdrv_bind_user_proc(struct devdrv_process_sign *d_sign,
    struct devdrv_ioctl_para_bind_host_pid para_info, pid_t devpid, u64 slave_start_time, int slave_side,
    struct devdrv_curr_ns_dev_info curr_dev_info)
{
    struct devdrv_process_user_info *user_proc = devdrv_get_user_proc(d_sign, slave_side);
    int i;
    int ret;

    for (i = 0; i < DEVMNG_USER_PROC_MAX; i++) {
        if ((user_proc[i].valid == 1) && (user_proc[i].pid == devpid) &&
            (user_proc[i].devid == para_info.chip_id) && (user_proc[i].vfid == para_info.vfid)) {
            return 0;
        }
    }

    for (i = 0; i < DEVMNG_USER_PROC_MAX; i++) {
        if (user_proc[i].valid == 0) {
            break;
        }
    }

    if (i >= DEVMNG_USER_PROC_MAX) {
        return -EINVAL;
    }

    user_proc[i].devid = para_info.chip_id;
    user_proc[i].vfid = para_info.vfid;
    user_proc[i].pid = devpid;
    user_proc[i].start_time = slave_start_time;
    user_proc[i].mode = para_info.mode;
    user_proc[i].valid = 1;

    if (devdrv_is_update_slave_proc_num(slave_side)) {
        d_sign->cp_count++;
    } else {
        d_sign->sync_proc_cnt++;
    }

    if (slave_side == HOST_SIDE && user_proc[i].devid == HAL_BIND_ALL_DEVICE) {
        ret = set_udevid_to_d_info(curr_dev_info.dev_num, curr_dev_info.udevid_list, user_proc, i);
        if (ret != 0) {
            user_proc[i].valid = 0;
            if (devdrv_is_update_slave_proc_num(slave_side)) {
                d_sign->cp_count--;
            } else {
                d_sign->sync_proc_cnt--;
            }
            return ret;
        }
    }

    return 0;
}

static int devdrv_unbind_user_proc(struct devdrv_process_sign *d_sign, u32 devid,
    u32 vfid, pid_t devpid, int slave_side)
{
    struct devdrv_process_user_info *user_proc = devdrv_get_user_proc(d_sign, slave_side);
    int i;

    for (i = 0; i < DEVMNG_USER_PROC_MAX; i++) {
        if ((user_proc[i].valid == 1) && (user_proc[i].pid == devpid)
            && (user_proc[i].devid == devid) && (user_proc[i].vfid == vfid)) {
            break;
        }
    }

    if (i >= DEVMNG_USER_PROC_MAX) {
        return -EINVAL;
    }

    user_proc[i].valid = 0;

    if (devdrv_is_update_slave_proc_num(slave_side)) {
        d_sign->cp_count--;
    } else {
        d_sign->sync_proc_cnt--;
    }

    return 0;
}

struct devdrv_bind_para {
    struct devdrv_ioctl_para_bind_host_pid para_info;
    pid_t devtgid;
    pid_t devpid;
    int slave_side;
    struct devdrv_curr_ns_dev_info curr_dev_info;
};

/*
 * Notice:Cannot print because it is called in Spinlock
 */
STATIC int devdrv_check_and_bind_hostpid(struct devdrv_process_sign *d_sign, struct devdrv_bind_para bind_para,
    pid_t *bound_devpid)
{
    struct devdrv_ioctl_para_bind_host_pid para_info = bind_para.para_info;
    pid_t devpid = bind_para.devtgid;
    int slave_side = bind_para.slave_side;
    u64 slave_start_time = 0;
    pid_t devpid_tmp = bind_para.devtgid;
    struct devdrv_curr_ns_dev_info curr_dev_info = bind_para.curr_dev_info;

#if defined(CFG_HOST_ENV)
    if (para_info.cp_type != DEVDRV_PROCESS_DEV_ONLY) {
        if (run_in_docker()) {
            devpid_tmp = bind_para.devpid;
        }

        if (!get_pid_start_time(devpid_tmp, &slave_start_time)) {
            return -ESRCH;
        }
    }
#else
    (void)devpid_tmp;
#endif

    if (para_info.cp_type == DEVDRV_PROCESS_USER) {
        *bound_devpid = 0;
        return devdrv_bind_user_proc(d_sign, para_info, devpid, slave_start_time, slave_side, curr_dev_info);
    }

    if (d_sign->devpid[para_info.chip_id][para_info.vfid][para_info.cp_type].slave_pid == DEVMNG_PID_INVALID ||
        d_sign->devpid[para_info.chip_id][para_info.vfid][para_info.cp_type].slave_pid == DEVMNG_PID_START_ONCE) {
        if ((para_info.cp_type == DEVDRV_PROCESS_CP2) &&
            (d_sign->devpid[para_info.chip_id][para_info.vfid][DEVDRV_PROCESS_CP1].slave_pid == DEVMNG_PID_INVALID)) {
            return -EPERM;
        }
        d_sign->devpid[para_info.chip_id][para_info.vfid][para_info.cp_type].slave_pid = devpid;
        d_sign->devpid[para_info.chip_id][para_info.vfid][para_info.cp_type].pid_start_time = slave_start_time;
        d_sign->devpid[para_info.chip_id][para_info.vfid][para_info.cp_type].mode = para_info.mode;

        if (devdrv_is_update_slave_proc_num(slave_side)) {
            d_sign->cp_count += (para_info.cp_type == DEVDRV_PROCESS_CP1 ? 1 : 0);
            d_sign->cp_count += (para_info.cp_type == DEVDRV_PROCESS_DEV_ONLY ? 1 : 0);
            d_sign->cp_count += (para_info.cp_type == DEVDRV_PROCESS_QS ? 1 : 0);
            d_sign->cp_count += (para_info.cp_type == DEVDRV_PROCESS_HCCP ? 1 : 0);
        } else {
            d_sign->sync_proc_cnt += (para_info.cp_type == DEVDRV_PROCESS_CP1 ? 1 : 0);
            d_sign->sync_proc_cnt += (para_info.cp_type == DEVDRV_PROCESS_DEV_ONLY ? 1 : 0);
        }
        d_sign->host_process_status = 0;
        return 0;
    }

    *bound_devpid = d_sign->devpid[para_info.chip_id][para_info.vfid][para_info.cp_type].slave_pid;
    d_sign->host_process_status = 0;

    if (d_sign->devpid[para_info.chip_id][para_info.vfid][para_info.cp_type].slave_pid == devpid) {
        return -EBUSY;
    }

    return -EINVAL;
}

STATIC int devdrv_verify_sign(char *sign, int mode, pid_t host_pid, pid_t *dev_pid, pid_t *dev_tgid)
{
    int ret;
    pid_t pid_tmp = -1;
#ifndef CFG_HOST_ENV
#ifndef CFG_BUILD_DEBUG
    const char *process_name[WHITE_LIST_PROCESS_NUM_FOR_BIND_PID] = {PROCESS_NAME_TSD};
#endif
#endif

    /* sign is used as dev_pid. */
    ret = memcpy_s(&pid_tmp, sizeof(pid_t), sign, sizeof(pid_t));
    if (ret != 0 || pid_tmp == -1) {
        devdrv_drv_err("Failed to get the dev_pid. (ret=%d; pid=%d)\n", ret, pid_tmp);
        return -EINVAL;
    }

    /* Compatible with early HIVA scenarios and RC mode */
    if ((mode == AICPUFW_OFFLINE_PLAT) && (host_pid == current->tgid)) {
        *dev_tgid = current->tgid;
        *dev_pid = current->tgid;
        ret = devdrv_devpid_container_convert(dev_pid);
        if (ret != 0) {
            devdrv_drv_err("Failed to convert pid. (ret=%d; dev_pid=%d)\n", ret, *dev_pid);
            return -EINVAL;
        }
        return 0;
    } else {
#ifndef CFG_HOST_ENV
#ifndef CFG_BUILD_DEBUG
        ret = whitelist_process_handler(process_name, WHITE_LIST_PROCESS_NUM_FOR_BIND_PID);
        if (ret != 0) {
            devdrv_drv_err("whitelist_process_handler error or Invalid parameter."
                " (ret=%d; dev_pid=%d)\n", ret, pid_tmp);
            return ret;
        }
#endif
#endif
        *dev_pid = pid_tmp;
        *dev_tgid = pid_tmp;
#if (defined CFG_FEATURE_BIND_TGID) || (defined CFG_HOST_ENV)
        if (devdrv_get_tgid_by_pid(pid_tmp, dev_tgid) != 0) {
            devdrv_drv_err("Failed to get tgid by pid. (pid=%d)\n", pid_tmp);
            return -EINVAL;
        }
#endif
    }

    return 0;
}

#ifndef CFG_FEATURE_RC_MODE
STATIC int devdrv_pid_map_sync_result_check(int ret, struct devdrv_manager_msg_info *dev_manager_msg_info, int out_len)
{
    if (ret != 0) {
        devdrv_drv_warn("Can't no send msg to remote. (ret=%d)\n", ret);
        return ret;
    }

    if (out_len != (sizeof(struct devdrv_pid_map_sync) + sizeof(struct devdrv_manager_msg_head))) {
        devdrv_drv_warn("receive length not equal expected. (out_len=%d; exp_len=%ld)\n", out_len,
            (sizeof(struct devdrv_ioctl_para_query_pid) + sizeof(struct devdrv_manager_msg_head)));
        return -EINVAL;
    }

    if (dev_manager_msg_info->header.result != 0) {
        devdrv_drv_warn("Can't sync or no need sync. (result=%d)\n", dev_manager_msg_info->header.result);
        return dev_manager_msg_info->header.result;
    }

    return 0;
}
#endif

#ifdef CFG_HOST_ENV
STATIC int devdrv_pid_map_sync_to_one_device(struct devdrv_ioctl_para_bind_host_pid *para_info, pid_t pid, int op)
{
    int ret;
    int out_len = 0;
    u32 remote_udevid;
    struct devdrv_info *dev_info = NULL;
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    struct devdrv_pid_map_sync *sync = (struct devdrv_pid_map_sync *)dev_manager_msg_info.payload;

    if (para_info->cp_type != DEVDRV_PROCESS_USER) {
        return 0;
    }

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_PID_MAP_SYNC;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    /* give a random value for checking result later */
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    /* inform corresponding devid to device side */

    ret = uda_dev_get_remote_udevid(para_info->chip_id, &remote_udevid);
    if (ret != 0) {
        devdrv_drv_err("get remote udevid failed. (udevid=%u)\n", para_info->chip_id);
        return ret;
    }

    sync->cp_type = para_info->cp_type;
    sync->host_pid = para_info->host_pid;
    sync->chip_id = remote_udevid;
    sync->vfid = para_info->vfid;
    sync->pid = pid;
    sync->op = op;

    dev_manager_msg_info.header.dev_id = para_info->chip_id;
    dev_info = devdrv_manager_get_devdrv_info(para_info->chip_id);
    if (dev_info == NULL) {
        devdrv_drv_err("dev_info is NULL. (dev_id=%u)\n", para_info->chip_id);
        return -EINVAL;
    }

    ret = devdrv_manager_send_msg(dev_info, &dev_manager_msg_info, &out_len);
    if (devdrv_pid_map_sync_result_check(ret, &dev_manager_msg_info, out_len)) {
        return -EINVAL;
    }

    return 0;
}

STATIC int devdrv_pid_map_sync_to_peer(struct devdrv_ioctl_para_bind_host_pid *para_info, pid_t pid, int op)
{
    u32 *udevids = NULL;
    u32 dev_num, devid, i;
    int ret;

    if (para_info->chip_id < DEVDRV_MAX_DAVINCI_NUM) {
        return devdrv_pid_map_sync_to_one_device(para_info, pid, op);
    }
    if (op == ADD_PID) {
        dev_num = uda_get_cur_ns_dev_num();
        if (dev_num <= 0 || dev_num > PID_MAP_DEVNUM) {
            devdrv_drv_err("Invalid device number. (slave_pid=%d; dev_num=%u)\n", pid, dev_num);
            return -EINVAL;
        }

        udevids = dbl_vmalloc(sizeof(u32) * dev_num, GFP_KERNEL|__KA_GFP_HIGHMEM|__GFP_ACCOUNT, PAGE_KERNEL);
        if (udevids == NULL) {
            devdrv_drv_err("Alloc udevids failed. (pid=%d; dev_num=%u)\n", pid, dev_num);
            return -ENOMEM;
        }

        ret = uda_get_cur_ns_udevids(udevids, dev_num);
        if (ret != 0) {
            dbl_vfree(udevids);
            devdrv_drv_err("Get udevids failed. (pid=%d; dev_num=%u)\n", pid, dev_num);
            return ret;
        }
    } else {
        udevids = dbl_vmalloc(sizeof(u32) * PID_MAP_DEVNUM, GFP_KERNEL|__KA_GFP_HIGHMEM|__GFP_ACCOUNT, PAGE_KERNEL);
        if (udevids == NULL) {
            devdrv_drv_err("Alloc udevids failed.\n");
            return -ENOMEM;
        }
        ret = get_udevid_and_dev_num(&dev_num, udevids, pid, para_info);
        if (ret != 0) {
            dbl_vfree(udevids);
            devdrv_drv_err("Get udevid from d_info failed. (ret=%u)\n", ret);
            return ret;
        }
    }

    devid = para_info->chip_id;
    for (i = 0; i < dev_num; i++) {
        para_info->chip_id = udevids[i];
        ret = devdrv_pid_map_sync_to_one_device(para_info, pid, op);
        if (ret != 0) {
            devdrv_drv_err("Sync failed. (pid=%d; udevid=%u)\n", pid, udevids[i]);
            dbl_vfree(udevids);
            para_info->chip_id = devid;
            return ret;
        }
    }

    dbl_vfree(udevids);

    para_info->chip_id = devid;

    return 0;
}
#else
STATIC int devdrv_pid_map_sync_to_peer(struct devdrv_ioctl_para_bind_host_pid *para_info, pid_t pid, int op)
{
#ifdef CFG_FEATURE_RC_MODE
    return 0;
#else
    int ret;
    int out_len = 0;
    u32 remote_udevid;
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    struct devdrv_pid_map_sync *sync = (struct devdrv_pid_map_sync *)dev_manager_msg_info.payload;

    if ((para_info->cp_type != DEVDRV_PROCESS_CP1) || (para_info->vfid != 0) ||
        (para_info->mode != AICPUFW_ONLINE_PLAT)) {
        return 0;
    }

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_PID_MAP_SYNC;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    /* give a random value for checking result later */
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    /* inform corresponding devid to device side */

    ret = uda_dev_get_remote_udevid(para_info->chip_id, &remote_udevid);
    if (ret != 0) {
        devdrv_drv_err("get remote udevid failed. (udevid=%u)\n", para_info->chip_id);
        return ret;
    }

    sync->cp_type = DEVDRV_PROCESS_DEV_ONLY; /* host has cp, store device cp in dev only */
    sync->host_pid = para_info->host_pid;
    sync->chip_id = remote_udevid;
    sync->vfid = para_info->vfid;
    sync->pid = pid;
    sync->op = op;

    devdrv_drv_info("Sync pid map. (op=%d; host_pid=%d; pid=%d; cp_type=%u; devid=%u)\n",
        sync->op, sync->host_pid, sync->pid, sync->cp_type, sync->chip_id);
    dev_manager_msg_info.header.dev_id = para_info->chip_id;

    ret = devdrv_manager_wait_host_ready_event(para_info->chip_id);
    if (ret != 0) {
        return ret;
    }

    ret = agentdrv_common_msg_send(para_info->chip_id, &dev_manager_msg_info, sizeof(struct devdrv_manager_msg_info),
        sizeof(struct devdrv_manager_msg_info), (u32 *)&out_len, AGENTDRV_COMMON_MSG_DEVDRV_MANAGER);

    return devdrv_pid_map_sync_result_check(ret, &dev_manager_msg_info, out_len);
#endif
}
#endif

#ifdef CFG_HOST_ENV
STATIC bool devdrv_is_master_pid(pid_t master_pid)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;

    ka_task_mutex_lock(&d_info->devdrv_sign_list_lock);
    ka_list_for_each_entry(d_sign, &d_info->hostpid_list_header, list) {
        if (d_sign->hostpid == master_pid) {
            ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);
            return true;
        }
    }
    ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);
    return false;
}
#endif
#define MAX_BIND_WAIT_TIMES  2800 /* max wait time 28s, equal to operator timeout interval  */
#define MAX_BIND_WAIT_ONCE   10

STATIC int devdrv_wait_pid_init(pid_t proc_pid, pid_t proc_tgid, int mode)
{
    u32 times = 0;
    ka_struct_pid_t *pid_proc = NULL;
#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
    if (mode == AICPUFW_OFFLINE_PLAT) {
        return 0;
    }
#endif
    do {
        times++;
        /* check process alive */
        pid_proc = find_get_pid(proc_pid);
        if (pid_proc == NULL) {
            return -ESRCH;
        }
        put_pid(pid_proc);

        /* check process init status */
        if (ascend_intf_is_pid_init(proc_tgid, DAVINCI_INTF_MODULE_DEVMNG) != 0) {
            return 0;
        }
        (void)msleep(MAX_BIND_WAIT_ONCE);
    } while (times < MAX_BIND_WAIT_TIMES);

    return -ETIME;
}


#ifdef CFG_HOST_ENV
static int check_parent_child_relationship(pid_t master_pid, char *sign)
{
    int ret;
    pid_t slave_pid = -1;
    pid_t cur_tgid = current->tgid;
    ka_task_struct_t *tsk = NULL;
    ka_struct_pid_t *pro_id = NULL;

    ret = memcpy_s(&slave_pid, sizeof(pid_t), sign, sizeof(pid_t));
    if (ret != 0 || slave_pid == -1) {
        devdrv_drv_err("Failed to get the dev_pid. (ret=%d; pid=%d)\n", ret, slave_pid);
        return -EINVAL;
    }

    pro_id = find_get_pid(slave_pid);
    if (pro_id == NULL) {
        devdrv_drv_err("Failed to find current process's pid. (pid=%d)\n", slave_pid);
        return -EINVAL;
    }

    tsk = get_pid_task(pro_id, PIDTYPE_PID);
    if (tsk == NULL) {
        devdrv_drv_err("Failed to find task struct in current process's pid_ns. (pid=%d)\n", slave_pid);
        put_pid(pro_id);
        return -EINVAL;
    }

    /* A process can bind itself, but can not help other process bind themselves. */
    if (master_pid == tsk->tgid && master_pid == cur_tgid) {
        ret = 0;
        goto OUT;
    }

    /*
    Currently, it is not allowed to help other process to bind slave pid.
    If it is allowed to help master process to bind current process's subprocess,
    it is necessary to check whether current process has bind master pid already.
    */
    if (master_pid != cur_tgid) {
        devdrv_drv_warn("master pid is not current tgid. (master_pid=%d; current tgid=%d)\n", master_pid, cur_tgid);
        ret = -EOPNOTSUPP;
        goto OUT;
    }

    /* the slave process must be a child of the current process */
    if (tsk->real_parent->tgid != cur_tgid) {
        devdrv_drv_err("slave pid's parent is not master pid. (master_pid=%d; slave_pid=%d, parent_pid=%d)\n",
            master_pid, slave_pid, tsk->real_parent->tgid);
        ret = -EINVAL;
        goto OUT;
    }

    ret = 0;
OUT:
    put_task_struct(tsk);
    put_pid(pro_id);
    return ret;
}
#endif

#ifndef CFG_SOC_PLATFORM_HELPER
/* The function operates protected resources [proc_hash_table],
   lock protection is required before invoking the function. */
STATIC int devdrv_query_master_pid_by_slave_pid(pid_t slave_pid, pid_t *master_pid)
{
    u32 bkt = 0;
    int i, j;
    struct devdrv_process_sign *d_sign = NULL;
    struct devdrv_process_user_info *user_proc = NULL;
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    int current_side = devdrv_get_cur_run_side();

    ka_hash_for_each(d_info->proc_hash_table, bkt, d_sign, link) {
        for (i = 0; i < PID_MAP_DEVNUM; i++) {
            for (j = 0; j < VFID_NUM_MAX; j++) {
                if ((d_sign->devpid[i][j][DEVDRV_PROCESS_CP1].slave_pid == slave_pid) ||
                (d_sign->devpid[i][j][DEVDRV_PROCESS_CP2].slave_pid == slave_pid) ||
                (d_sign->devpid[i][j][DEVDRV_PROCESS_QS].slave_pid == slave_pid) ||
                (d_sign->devpid[i][j][DEVDRV_PROCESS_HCCP].slave_pid == slave_pid) ||
                ((current_side == DEVICE_SIDE) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_DEV_ONLY].slave_pid == slave_pid))) {
                    *master_pid = d_sign->hostpid;
                    return 0;
                }
            }
        }

        user_proc = devdrv_query_user_proc(d_sign, current_side, slave_pid);
        if (user_proc != NULL) {
            *master_pid = d_sign->hostpid;
            return 0;
        }
    }
    return -ESRCH;
}
#endif

STATIC bool is_multi_bind_master(pid_t slave_pid, pid_t master_pid, pid_t *bound_masterpid)
{
#ifndef CFG_SOC_PLATFORM_HELPER
    int ret;

    ret = devdrv_query_master_pid_by_slave_pid(slave_pid, bound_masterpid);
    if ((ret == 0) && (*bound_masterpid != master_pid)) {
        return true;
    }
    return false;
#else
    return false;
#endif
}

static int devdrv_get_master_deployment_location(unsigned int mode)
{
    int current_side = devdrv_get_cur_run_side();
    if ((current_side == DEVICE_SIDE) && (mode == AICPUFW_OFFLINE_PLAT)) {
        return DEVDRV_DEVICE_MASTER;
    } else {
        return DEVDRV_HOST_MASTER;
    }
}

#define DEVDRV_PID_MAP_COST_TIME_MAX    1000    /* 1000 ms */
void bind_cost_print(struct bind_cost_statistics *cost_stat)
{
    if (ka_system_ktime_to_ms(ktime_sub(cost_stat->bind_end, cost_stat->bind_start)) <
        DEVDRV_PID_MAP_COST_TIME_MAX) {
        return;
    }

    devdrv_drv_warn("Binding time exceeds %d ms. (cost_time=%lldms; bind_start=%lldus; bind_end=%lldus; "
                    "check_master_start=%lldus; check_master_end=%lldus; "
                    "check_slave_start=%lldus; check_slave_end=%lldus; "
                    "sync_start=%lldus; sync_end=%lldus; "
                    "update_hash_start=%lldus; update_hash_end=%lldus)\n",
        DEVDRV_PID_MAP_COST_TIME_MAX,
        ka_system_ktime_to_ms(ktime_sub(cost_stat->bind_end, cost_stat->bind_start)),
        ka_system_ktime_to_us(cost_stat->bind_start), ka_system_ktime_to_us(cost_stat->bind_end),
        ka_system_ktime_to_us(cost_stat->check_master_start), ka_system_ktime_to_us(cost_stat->check_master_end),
        ka_system_ktime_to_us(cost_stat->check_slave_start), ka_system_ktime_to_us(cost_stat->check_slave_end),
        ka_system_ktime_to_us(cost_stat->sync_start), ka_system_ktime_to_us(cost_stat->sync_end),
        ka_system_ktime_to_us(cost_stat->update_hash_start), ka_system_ktime_to_us(cost_stat->update_hash_end));
}

int devdrv_bind_hostpid(struct devdrv_ioctl_para_bind_host_pid para_info, struct bind_cost_statistics *cost_stat)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    struct devdrv_process_sign *d_sign_create = NULL;
    u32 key;
    int ret, ret_sync;
    bool is_multi_bind = false;
    pid_t bound_devpid = -1;
    pid_t bound_masterpid = -1;
    pid_t dev_pid = -1;
    pid_t dev_tgid = -1;
    int master_location = DEVDRV_HOST_MASTER;
    int current_side = devdrv_get_cur_run_side();
    struct devdrv_bind_para bind_para = { {0}, 0, 0, 0, {0} };

#if (defined CFG_FEATURE_BIND_TGID) || (defined CFG_HOST_ENV)
    pid_t tgid = 0;

    if (devdrv_get_tgid_by_pid(para_info.host_pid, &tgid) != 0) {
        devdrv_drv_err("Failed to get tgid by pid. (pid=%d)\n", para_info.host_pid);
        return -EINVAL;
    }

    para_info.host_pid = tgid;
#endif

#ifdef CFG_HOST_ENV
    if (!devdrv_is_master_pid(para_info.host_pid)) {
        devdrv_drv_err("Master proc not create. (host_pid=%d)\n", para_info.host_pid);
        return -EINVAL;
    }

    ret = check_parent_child_relationship(para_info.host_pid, para_info.sign);
    if (ret != 0) {
        devdrv_drv_err("check parent child relationship failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

    ret = devdrv_verify_sign(para_info.sign, para_info.mode, para_info.host_pid, &dev_pid, &dev_tgid);
    if (ret != 0) {
        devdrv_drv_err("Failed to get the dev_tgid. (ret=%d; dev_tgid=%d)\n", ret, dev_tgid);
        return -EINVAL;
    }

    master_location = devdrv_get_master_deployment_location(para_info.mode);
#ifdef CFG_FEATURE_DEVICE_MASTER_INDEPENDENCE
    if (master_location == DEVDRV_DEVICE_MASTER) {
        /* dev_only type for tsdaemon save in host master table, other no save. */
        if (para_info.cp_type != DEVDRV_PROCESS_DEV_ONLY) {
            devdrv_drv_info("Bind pid success. "
                "(host_pid=%d; dev_tgid=%d; cp_type=%d; dev_id=%u; vfid=%u; mode=%d; master_location=%d)\n",
                para_info.host_pid, dev_tgid, para_info.cp_type, para_info.chip_id, para_info.vfid, para_info.mode,
                master_location);
            return 0;
        }
    }
#endif

    cost_stat->check_slave_start = ka_system_ktime_get();
    ret = devdrv_wait_pid_init(dev_pid, dev_tgid, para_info.mode);
    if (ret != 0) {
        devdrv_drv_err("wait process init failed. (ret=%d; dev_pid=%d; dev_tgid=%d)\n", ret, dev_pid, dev_tgid);
        return -EINVAL;
    }
    cost_stat->check_slave_end = ka_system_ktime_get();

    if ((para_info.cp_type == DEVDRV_PROCESS_USER) && (current_side == HOST_SIDE)
        && (para_info.chip_id == HAL_BIND_ALL_DEVICE)) {
        ret = devdrv_get_curr_ns_dev_info(&bind_para.curr_dev_info);
        if (ret != 0) {
            devdrv_drv_err("Get all device list failed. "
                "(ret=%d, host_pid=%d; dev_tgid=%d; cp_type=%d; dev_id=%u; vfid=%u; mode=%d)\n",
                ret, para_info.host_pid, dev_tgid, para_info.cp_type,
                para_info.chip_id, para_info.vfid, para_info.mode);
            return ret;
        }
    }

    key = (u32)para_info.host_pid & DEVDRV_PROC_HASH_TABLE_MASK;
    d_sign_create = devdrv_create_process_sign(d_info, para_info);
    if (d_sign_create == NULL) {
        devdrv_drv_err("Create sign failed or invalid sign. (chip_id=%u; hostpid=%d)\n",
            para_info.chip_id, para_info.host_pid);
        return -EINVAL;
    }

    cost_stat->sync_start = ka_system_ktime_get();
    ret_sync = devdrv_pid_map_sync_to_peer(&para_info, dev_tgid, ADD_PID);
    if (ret_sync != 0) {
        devdrv_drv_warn("Sync to peer unsuccessfully."
            "(host_pid=%d; dev_tgid=%d; cp_type=%d; dev_id=%u; vfid=%u; mode=%d)\n",
            para_info.host_pid, dev_tgid, para_info.cp_type, para_info.chip_id, para_info.vfid, para_info.mode);
    }
    cost_stat->sync_end = ka_system_ktime_get();

    bind_para.para_info = para_info;
    bind_para.devtgid = dev_tgid;
    bind_para.devpid = dev_pid;
    bind_para.slave_side = current_side;

    cost_stat->update_hash_start = ka_system_ktime_get();
    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    d_sign = devdrv_find_process_sign(d_info, para_info.host_pid);
    if (d_sign != NULL) {
        is_multi_bind = is_multi_bind_master(dev_tgid, para_info.host_pid, &bound_masterpid);
        ret = (is_multi_bind) ? -EINVAL : devdrv_check_and_bind_hostpid(d_sign, bind_para, &bound_devpid);
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_debug("Destroy master pid ctx when exist. (master_pid=%d)\n", para_info.host_pid);
        dbl_vfree(d_sign_create);
        d_sign_create = NULL;
        if (ret == -EBUSY) {
            devdrv_drv_warn("Hostpid has already bound current devpid. "
                "(hostpid=%d; devpid=%d; cp_type=%d; chip_id=%u; vfid=%u)\n",
                para_info.host_pid, dev_tgid, para_info.cp_type, para_info.chip_id, para_info.vfid);
        } else if (ret != 0) {
            devdrv_drv_err("Bind failed or already bound another m/s process. "
                "(ret=%d; host_pid=%d; cp_type=%d; bound_devpid=%d; bound_masterpid=%d; dev_tgid=%d;"
                " dev_id=%u; vfid=%u).\n",
                ret, para_info.host_pid, para_info.cp_type, bound_devpid, bound_masterpid, dev_tgid,
                    para_info.chip_id, para_info.vfid);
            goto BIND_FAILED;
        }
        goto bind_succ;
    }

    is_multi_bind = is_multi_bind_master(dev_tgid, para_info.host_pid, &bound_masterpid);
    ret = (is_multi_bind) ? -EINVAL : devdrv_check_and_bind_hostpid(d_sign_create, bind_para, &bound_devpid);
    if (ret != 0) {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_err("Bind failed or already bound another m/s process. "
            "(ret=%d; host_pid=%d; cp_type=%d; bound_devpid=%d; bound_masterpid=%d; dev_tgid=%d;"
            " dev_id=%u; vfid=%u).\n",
            ret, para_info.host_pid, para_info.cp_type, bound_devpid, bound_masterpid, dev_tgid,
                para_info.chip_id, para_info.vfid);
        dbl_vfree(d_sign_create);
        d_sign_create = NULL;
        goto BIND_FAILED;
    }
    ka_hash_add(d_info->proc_hash_table, &d_sign_create->link, key); //lint !e666
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
    cost_stat->update_hash_end = ka_system_ktime_get();

bind_succ:
    if (ret_sync != 0) {
        ret = devdrv_pid_map_sync_to_peer(&para_info, dev_tgid, ADD_PID);
        if (ret != 0) {
            devdrv_drv_warn("Sync to peer unsuccessfully."
                "(host_pid=%d; dev_tgid=%d; cp_type=%d; dev_id=%u; vfid=%u; mode=%d)\n",
                para_info.host_pid, dev_tgid, para_info.cp_type, para_info.chip_id, para_info.vfid, para_info.mode);
        }
    }
    cost_stat->bind_end = ka_system_ktime_get();
    bind_cost_print(cost_stat);

    devdrv_drv_info("Bind pid success. "
        "(host_pid=%d; dev_tgid=%d; cp_type=%d; dev_id=%u; vfid=%u; mode=%d; master_location=%d)\n",
        para_info.host_pid, dev_tgid, para_info.cp_type, para_info.chip_id, para_info.vfid, para_info.mode,
        master_location);
    return 0;

BIND_FAILED:
    if (ret_sync == 0) {
        ret_sync = devdrv_pid_map_sync_to_peer(&para_info, dev_tgid, DELETE_PID);
        if (ret_sync != 0) {
            devdrv_drv_warn("Sync to peer to unbind unsuccessfully."
                "(host_pid=%d; dev_tgid=%d; cp_type=%d; dev_id=%u; vfid=%u; mode=%d)\n",
                para_info.host_pid, dev_tgid, para_info.cp_type, para_info.chip_id, para_info.vfid, para_info.mode);
        }
    }
    return ret;
}

static int devdrv_release_single_devpid(pid_t host_pid, pid_t dev_pid,
    unsigned int dev_id, unsigned int vf_id, enum devdrv_process_type cp_type, int slave_side)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    struct devdrv_process_sign *d_sign_check = NULL;
    int ret, count;

    if (d_info == NULL) {
        devdrv_drv_err("Global variable dev_manager_info is NULL.\n");
        return -EINVAL;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    d_sign = devdrv_find_process_sign(d_info, host_pid);
    if (d_sign == NULL) {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_warn("Can't find host_pid in table, please check interface parameters. "
                       "(host_pid=%d; dev_pid=%d; dev_id=%u; vf_id=%u; cp_type=%d)\n",
                       host_pid, dev_pid, dev_id, vf_id, cp_type);
        return -EINVAL;
    }

    if (cp_type == DEVDRV_PROCESS_USER) {
        ret = devdrv_unbind_user_proc(d_sign, dev_id, vf_id, dev_pid, slave_side);
    } else {
        if ((d_sign->devpid[dev_id][vf_id][cp_type].slave_pid == dev_pid)) {
            if (cp_type == DEVDRV_PROCESS_CP1) {
                d_sign->devpid[dev_id][vf_id][DEVDRV_PROCESS_CP1].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[dev_id][vf_id][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_INVALID;
                d_sign->cp_count -= (devdrv_is_update_slave_proc_num(slave_side)) ? 1 : 0;
                d_sign->sync_proc_cnt -= (!devdrv_is_update_slave_proc_num(slave_side)) ? 1 : 0;
            } else if ((cp_type == DEVDRV_PROCESS_CP2) &&
                        (d_sign->devpid[dev_id][vf_id][DEVDRV_PROCESS_CP1].slave_pid != DEVMNG_PID_INVALID)) {
                d_sign->devpid[dev_id][vf_id][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_START_ONCE;
            } else if (cp_type == DEVDRV_PROCESS_CP2) {
                d_sign->devpid[dev_id][vf_id][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_INVALID;
            } else {
                d_sign->devpid[dev_id][vf_id][cp_type].slave_pid = DEVMNG_PID_INVALID;
                d_sign->cp_count -= (devdrv_is_update_slave_proc_num(slave_side)) ? 1 : 0;
                d_sign->sync_proc_cnt -= (!devdrv_is_update_slave_proc_num(slave_side)) ? 1 : 0;
            }
            ret = 0;
        } else {
            ret = -EINVAL;
        }
    }
    if (ret != 0) {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_err("Can't find dev_pid in table, please check interface parameters. "
                       "(host_pid=%d; dev_pid=%d; dev_id=%u; vf_id=%u; cp_type=%d)\n",
                       host_pid, dev_pid, dev_id, vf_id, cp_type);
        return ret;
    }

    count = d_sign->cp_count + d_sign->sync_proc_cnt;
    if (count == 0) {
        ka_hash_del(&d_sign->link);
    }

    d_sign_check = devdrv_find_process_sign(d_info, host_pid);
    if (d_sign_check == NULL || (d_sign_check->devpid[dev_id][vf_id][cp_type].slave_pid == DEVMNG_PID_INVALID)) {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_info("Release device process. (host_pid=%d; dev_pid=%d; dev_id=%u; vf_id=%u; cp_type=%d)\n",
                        host_pid, dev_pid, dev_id, vf_id, cp_type);
        ret = 0;
        goto release_exit;
    } else {
        ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
        devdrv_drv_info("Find process sign is abnormal. (dev_pid=%d)\n",
            d_sign_check->devpid[dev_id][vf_id][cp_type].slave_pid);
        ret = -EINVAL;
        goto release_exit;
    }

release_exit:
    if (count == 0) {
        devdrv_drv_info("Destroy master pid ctx when proc count is 0. (master_pid=%d)\n", d_sign->hostpid);
        dbl_vfree(d_sign);
        d_sign = NULL;
    }
    return ret;
}

#ifdef CFG_FEATURE_HOST_UNBIND
int devdrv_unbind_hostpid(struct devdrv_ioctl_para_bind_host_pid para_info)
{
    int ret;
    u32 dev_id;
    u32 phys_id = DEVDRV_MAX_DAVINCI_NUM, vf_id = 0;
    pid_t host_pid = -1;
    pid_t dev_pid = -1;
    pid_t dev_tgid = -1;

    if (para_info.chip_id < PID_MAP_DEVNUM) {
        ret = devdrv_manager_trans_and_check_id(para_info.chip_id, &phys_id, &vf_id, ALSO_DOES_SUPPORT_VF);
        if (ret != 0) {
            devdrv_drv_err("Failed to transfer logical ID to physical ID. (dev_id=%u; ret=%d)\n",
                para_info.chip_id, ret);
            return ret;
        }
    }

    dev_id = phys_id;
    vf_id = para_info.vfid;
#if (defined CFG_FEATURE_BIND_TGID) || (defined CFG_HOST_ENV)
    if (devdrv_get_tgid_by_pid(para_info.host_pid, &host_pid) != 0) {
        devdrv_drv_err("Failed to get tgid by pid. (pid=%d)\n", para_info.host_pid);
        return -EINVAL;
    }
#else
    host_pid = para_info.host_pid;
#endif

    ret = devdrv_verify_sign(para_info.sign, para_info.mode, host_pid, &dev_pid, &dev_tgid);
    if (ret != 0) {
        devdrv_drv_err("Failed to get the dev_tgid. (ret=%d; dev_tgid=%d)\n", ret, dev_tgid);
        return -EINVAL;
    }

    ret = devdrv_release_single_devpid(host_pid, dev_tgid, dev_id, vf_id, para_info.cp_type, devdrv_get_cur_run_side());
    if (ret == 0) {
        int sync_ret = devdrv_pid_map_sync_to_peer(&para_info, dev_tgid, DELETE_PID);
        if (sync_ret != 0) {
            devdrv_drv_warn("Sync failed. (host_pid=%d; dev_tgid=%d; cp_type=%d; dev_id=%u; vfid=%u; mode=%d)\n",
                para_info.host_pid, dev_tgid, para_info.cp_type, para_info.chip_id, para_info.vfid, para_info.mode);
        }
    }

    return ret;
}
#endif

static int devdrv_pid_map_sync_add(struct devdrv_pid_map_sync *sync)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_ioctl_para_bind_host_pid para_info = {0};
    struct devdrv_bind_para bind_para = { {0}, 0, 0, 0, {0} };
    struct devdrv_process_sign *d_sign = NULL, *d_sign_tmp = NULL;
    int ret = 0, create_flag = 0;
    int sync_side = (devdrv_get_cur_run_side() == DEVICE_SIDE) ? HOST_SIDE : DEVICE_SIDE;
    u32 bound_devpid;

    para_info.host_pid = sync->host_pid;
    para_info.chip_id = sync->chip_id;
    para_info.vfid = sync->vfid;
    para_info.cp_type = sync->cp_type;
    /* host sync to device, the master proc must be host_master */
    /* device sync to host, the master proc must be host_master */
    para_info.mode = AICPUFW_ONLINE_PLAT;

    /* The mutex lock for creation and destruction is missing, devdrv_create_process_sign use vmalloc,
       can not call in spin_lock_bh, so alloc before here */
    d_sign_tmp = devdrv_create_process_sign(d_info, para_info);
    if (d_sign_tmp == NULL) {
        return -ENOMEM;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    d_sign = devdrv_find_process_sign(d_info, sync->host_pid);
    if (d_sign == NULL) {
        create_flag = 1;
        d_sign = d_sign_tmp;
    }

    bind_para.para_info = para_info;
    bind_para.devtgid = sync->pid;
    bind_para.slave_side = sync_side;
    ret = devdrv_check_and_bind_hostpid(d_sign, bind_para, &bound_devpid);
    if ((ret == 0) && (create_flag == 1)) {
        ka_hash_add(d_info->proc_hash_table, &d_sign->link, sync->host_pid & DEVDRV_PROC_HASH_TABLE_MASK); //lint !e666
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    if ((d_sign_tmp != NULL) && (create_flag == 0)) {
        devdrv_drv_debug("Destroy master pid ctx when sync. (master_pid=%d)\n", d_sign_tmp->hostpid);
        dbl_vfree(d_sign_tmp);
    }

    devdrv_drv_info("Sync finish. (op=%d; host_pid=%d; pid=%d; cp_type=%u; devid=%u; ret=%d)\n",
        sync->op, sync->host_pid, sync->pid, sync->cp_type, sync->chip_id, ret);

    return ret;
}

static int devdrv_pid_map_sync_del(struct devdrv_pid_map_sync *sync)
{
    int sync_side = (devdrv_get_cur_run_side() == DEVICE_SIDE) ? HOST_SIDE : DEVICE_SIDE;
    int ret;

    ret = devdrv_release_single_devpid(sync->host_pid, sync->pid, sync->chip_id, sync->vfid,
        sync->cp_type, sync_side);
    devdrv_drv_info("Sync finish. (op=%d; host_pid=%d; pid=%d; cp_type=%u; devid=%u; ret=%d)\n",
        sync->op, sync->host_pid, sync->pid, sync->cp_type, sync->chip_id, ret);

    return ret;
}

#ifdef CFG_HOST_ENV
int devdrv_pid_map_sync_proc(void *msg, u32 *ack_len)
#else
int devdrv_pid_map_sync_proc(u32 devid, void *msg, u32 in_len, u32 *ack_len)
#endif
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    struct devdrv_pid_map_sync *sync = NULL;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;

    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("invalid message\n");
        return -EINVAL;
    }

#ifndef CFG_HOST_ENV
    if (in_len < sizeof(struct devdrv_manager_msg_info)) {
        devdrv_drv_err("Invalid message from host. (dev_id=%u; valid=%u; in_len=%u)\n",
                       devid, dev_manager_msg_info->header.valid, in_len);
        return -EINVAL;
    }
    sync = (struct devdrv_pid_map_sync *)dev_manager_msg_info->payload;
#else
    sync = (struct devdrv_pid_map_sync *)dev_manager_msg_info->payload;
    sync->chip_id = dev_manager_msg_info->header.dev_id;
#endif

    if (sync->chip_id >= PID_MAP_DEVNUM) {
        /* In vm split mode, dev_id(chip_id) is udevid of vm, support sync;
           In container split mode, dev_id(chip_id) is udevid of Host machine(such as 100), NOT support sync.
        */
        dev_manager_msg_info->header.result = 0;
        dev_manager_msg_info->header.valid = DEVDRV_MANAGER_MSG_VALID;
        *ack_len = sizeof(struct devdrv_pid_map_sync) + sizeof(struct devdrv_manager_msg_head);
        return 0;
    }

    if ((sync->vfid >= VFID_NUM_MAX) || (sync->cp_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        devdrv_drv_err("Invalid parameter. (host_pid=%d; pid=%d; cp_type=%u; devid=%u; vfid=%u)\n",
            sync->host_pid, sync->pid, sync->cp_type, sync->chip_id, sync->vfid);
        return -EINVAL;
    }

    if (sync->op == ADD_PID) {
        dev_manager_msg_info->header.result = devdrv_pid_map_sync_add(sync);
    } else {
        dev_manager_msg_info->header.result = devdrv_pid_map_sync_del(sync);
    }
    dev_manager_msg_info->header.valid = DEVDRV_MANAGER_MSG_VALID;
    *ack_len = sizeof(struct devdrv_pid_map_sync) + sizeof(struct devdrv_manager_msg_head);

    return 0;
}

/* Not user parameter: q_info->slave_pid */
int devdrv_query_slave_by_map_info(const devdrv_pid_map_info_t *q_info, pid_t *slave_pid, int *mode)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    u32 key;

    if ((q_info == NULL) || (slave_pid == NULL) || (mode == NULL) || (d_info == NULL)) {
        devdrv_drv_err("null ptr. (q_info_is_null=%d; pid_is_null=%d; mode_is_null=%d; d_info_is_null=%d)\n",
            (q_info == NULL), (slave_pid == NULL), (mode == NULL), (d_info == NULL));
        return -EINVAL;
    }

    if ((q_info->cp_type >= DEVDRV_PROCESS_CPTYPE_MAX) || (q_info->vf_id >= VFID_NUM_MAX)) {
        devdrv_drv_err("Invalid para. (dev_id=%u; vf_id=%u; cp_type=%u; master_pid=%d)\n",
                    q_info->dev_id, q_info->vf_id, q_info->cp_type, q_info->master_pid);
        return -EINVAL;
    }

    if ((q_info->dev_id >= PID_MAP_DEVNUM) || (q_info->cp_type == DEVDRV_PROCESS_USER)) {
        devdrv_drv_debug("Not surpport. (dev_id=%u, cp_type=%u)\n", q_info->dev_id, q_info->cp_type);
        return -EINVAL;
    }

    key = (u32)q_info->master_pid & DEVDRV_PROC_HASH_TABLE_MASK;
    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    /*lint -e666 */
    ka_hash_for_each_possible(d_info->proc_hash_table, d_sign, link, key) {
        if (d_sign->hostpid == q_info->master_pid) {
            *slave_pid = d_sign->devpid[q_info->dev_id][q_info->vf_id][q_info->cp_type].slave_pid;
            *mode = d_sign->devpid[q_info->dev_id][q_info->vf_id][q_info->cp_type].mode;
            if ((q_info->cp_type == DEVDRV_PROCESS_CP2) && (*slave_pid == DEVMNG_PID_INVALID)) {
                *slave_pid = d_sign->devpid[q_info->dev_id][q_info->vf_id][DEVDRV_PROCESS_CP1].slave_pid;
                *mode = d_sign->devpid[q_info->dev_id][q_info->vf_id][DEVDRV_PROCESS_CP1].mode;
            }
            ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
            if (*slave_pid == DEVMNG_PID_INVALID) {
                devdrv_drv_debug("The hostpid didn't bind this type device pid. "
                    "(master=%u; dev_id=%u; vf_id=%u; cp_type=%u)\n",
                    q_info->master_pid, q_info->dev_id, q_info->vf_id, q_info->cp_type);
                return DRV_ERROR_NO_PROCESS;
            }
            return 0;
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
    devdrv_drv_debug("Cannot find the host pid in hashtable. (master=%u; dev_id=%u; vf_id=%u; cp_type=%u)\n",
        q_info->master_pid, q_info->dev_id, q_info->vf_id, q_info->cp_type);

    return DRV_ERROR_NO_PROCESS;
}

int devdrv_query_process_by_host_pid_kernel(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid)
{
    devdrv_pid_map_info_t q_info = { 0 };
    pid_t slave_pid = -1;
    unsigned int mode = AICPUFW_MAX_PLAT;
    int ret = 0;

    if (pid == NULL) {
        devdrv_drv_err("null ptr.\n");
        return -EINVAL;
    }

    q_info.master_pid = host_pid;
    q_info.dev_id = chip_id;
    q_info.vf_id = vfid;
    q_info.cp_type = (unsigned int)cp_type;

    ret = devdrv_query_slave_by_map_info((const devdrv_pid_map_info_t *)&q_info, &slave_pid, &mode);
    if (ret == 0) {
        *pid = slave_pid;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(devdrv_query_process_by_host_pid_kernel);

int devdrv_query_process_by_host_pid_user(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid)
{
    int ret;

#ifdef CFG_FEATURE_BIND_TGID
    pid_t tgid = 0;
    if (devdrv_get_tgid_by_pid(host_pid, &tgid) != 0) {
        devdrv_drv_err("Failed to get tgid by pid. (pid=%d)\n", host_pid);
        return -EINVAL;
    }

    host_pid = tgid;
#endif

    ret = devdrv_query_process_by_host_pid_kernel(host_pid, chip_id, cp_type, vfid, pid);
    if (ret == 0) {
#if defined(CFG_HOST_ENV) || defined(CFG_FEATURE_DEVICE_CONTAINER)
        if (devdrv_devpid_container_convert(pid) != 0) {
            devdrv_drv_err("Failed to convert pid. (pid=%d)\n", *pid);
            return -EINVAL;
        }
#endif
        return 0;
    }

    if (ret == DRV_ERROR_NO_PROCESS) {
        return ret;
    } else {
        return -EINVAL;
    }
}

int devdrv_query_process_by_host_pid(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid)
{
    return devdrv_query_process_by_host_pid_kernel(host_pid, chip_id, cp_type, vfid, pid);
}
KA_EXPORT_SYMBOL_GPL(devdrv_query_process_by_host_pid);

int devdrv_query_process_host_pid(int pid, unsigned int *chip_id, unsigned int *vfid, unsigned int *host_pid,
    enum devdrv_process_type *cp_type)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    int current_side = devdrv_get_cur_run_side();
    u32 bkt = 0;
    int i, j;

    if ((chip_id == NULL) || (vfid == NULL) || (host_pid == NULL) || (cp_type == NULL)) {
        devdrv_drv_err("null ptr.\n");
        return -EINVAL;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);

    ka_hash_for_each(d_info->proc_hash_table, bkt, d_sign, link) {
        struct devdrv_process_user_info *user_proc = NULL;
        for (i = 0; i < PID_MAP_DEVNUM; i++) {
            for (j = 0; j < VFID_NUM_MAX; j++) {
                if (d_sign->devpid[i][j][DEVDRV_PROCESS_CP1].slave_pid == pid) {
                    *cp_type = DEVDRV_PROCESS_CP1;
                } else if (d_sign->devpid[i][j][DEVDRV_PROCESS_CP2].slave_pid == pid) {
                    *cp_type = DEVDRV_PROCESS_CP2;
                } else if (d_sign->devpid[i][j][DEVDRV_PROCESS_QS].slave_pid == pid) {
                    *cp_type = DEVDRV_PROCESS_QS;
                } else if ((d_sign->devpid[i][j][DEVDRV_PROCESS_DEV_ONLY].slave_pid == pid) && (current_side == DEVICE_SIDE)) {
                    *cp_type = DEVDRV_PROCESS_DEV_ONLY;
                } else if (d_sign->devpid[i][j][DEVDRV_PROCESS_HCCP].slave_pid == pid) {
                    *cp_type = DEVDRV_PROCESS_HCCP;
                } else {
                    continue;
                }
                *chip_id = i;
                *vfid = j;
                *host_pid = d_sign->hostpid;
                ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                return 0;
            }
        }

        user_proc = devdrv_query_user_proc(d_sign, current_side, pid);
        if (user_proc != NULL) {
            *chip_id = user_proc->devid;
            *vfid = user_proc->vfid;
            *host_pid = d_sign->hostpid;
            *cp_type = DEVDRV_PROCESS_USER;
            ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
            return 0;
        }
    }

    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    return -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(devdrv_query_process_host_pid);

/*  Only search the hash table on the invoking side.
    If the current side is host, search the host hash table, the master proc must be is DEVDRV_HOST_MASTER.
    (Not include device sync to host message)
    If the current side is device, search the device hash table, the proc location is determined by [mode].
*/
int devdrv_query_master_location(const devdrv_pid_map_info_t *q_info, unsigned int *location)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    struct devdrv_process_user_info *user_proc = NULL;
    u32 key, mode;
    int current_side = devdrv_get_cur_run_side();

    if ((location == NULL) || (q_info == NULL) || (d_info == NULL)) {
        devdrv_drv_err("null ptr. (location_is_null=%d; q_info_is_null=%d; d_info_is_null=%d)\n",
            (location == NULL), (q_info == NULL), (q_info == NULL));
        return -EINVAL;
    }

    if ((q_info->dev_id >= PID_MAP_DEVNUM) || (q_info->vf_id >= VFID_NUM_MAX) ||
        (q_info->cp_type >= DEVDRV_PROCESS_CPTYPE_MAX) || (q_info->slave_pid <= 0) || (q_info->master_pid <= 0)) {
        devdrv_drv_err("Invalid para. (dev_id=%u; vf_id=%u; cp_type=%u; slave_pid=%d; master_pid=%d)\n",
            q_info->dev_id, q_info->vf_id, q_info->cp_type, q_info->slave_pid, q_info->master_pid);
        return -EINVAL;
    }

    if ((current_side == HOST_SIDE) && (q_info->cp_type == DEVDRV_PROCESS_DEV_ONLY)) {
        return -EOPNOTSUPP;
    }

    key = (u32)q_info->master_pid & DEVDRV_PROC_HASH_TABLE_MASK;
    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each_possible(d_info->proc_hash_table, d_sign, link, key) {
        if (d_sign->hostpid != q_info->master_pid) {
            continue;
        }

        if (q_info->cp_type < DEVDRV_PROCESS_USER) {
            if (d_sign->devpid[q_info->dev_id][q_info->vf_id][q_info->cp_type].slave_pid == q_info->slave_pid) {
                mode = d_sign->devpid[q_info->dev_id][q_info->vf_id][q_info->cp_type].mode;
                *location = devdrv_get_master_deployment_location(mode);
                ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                return 0;
            }
        } else {
            user_proc = devdrv_query_user_proc(d_sign, current_side, q_info->slave_pid);
            if ((user_proc != NULL) &&
                    ((user_proc->devid == HAL_BIND_ALL_DEVICE) || (user_proc->devid == q_info->dev_id)) &&
                    (user_proc->vfid == q_info->vf_id)) {
                *location = devdrv_get_master_deployment_location(user_proc->mode);
                ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                return 0;
            }
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
    return -ESRCH;
}
EXPORT_SYMBOL(devdrv_query_master_location);

/* stub */
#include "ascend_hal_define.h"
int apm_query_master_info_by_slave(int slave_tgid, int *master_tgid, u32 *udevid, int *mode, u32 *proc_type_bitmap)
{
    enum devdrv_process_type cp_type;
    unsigned int vfid;
    int ret;

    ret = devdrv_query_process_host_pid(slave_tgid, udevid, &vfid, (unsigned int *)master_tgid, &cp_type);
    if (ret != 0) {
        return ret;
    }

    *mode = 1; /* online support online */
    *proc_type_bitmap = 0x1 << cp_type;

    return 0;
}
EXPORT_SYMBOL(apm_query_master_info_by_slave);

int apm_query_slave_tgid_by_master(int master_tgid, u32 udevid, processType_t proc_type, int *slave_tgid)
{
    return devdrv_query_process_by_host_pid(master_tgid, udevid, (enum devdrv_process_type)proc_type, 0, slave_tgid);
}
EXPORT_SYMBOL(apm_query_slave_tgid_by_master);

#ifdef CFG_HOST_ENV
/* only support cp proc */
int devdrv_query_master_pid_by_device_slave(u32 udevid, int slave_pid, u32 *master_pid)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    u32 bkt = 0, vfid = 0;

    if ((master_pid == NULL) || (udevid >= PID_MAP_DEVNUM)) {
        return -EINVAL;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);

    ka_hash_for_each(d_info->proc_hash_table, bkt, d_sign, link) {
        /* host has cp, store device cp in dev_only */
        if (d_sign->devpid[udevid][vfid][DEVDRV_PROCESS_DEV_ONLY].slave_pid == slave_pid) {
            *master_pid = d_sign->hostpid;
            ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
            return 0;
        }
    }

    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    return -EINVAL;
}

EXPORT_SYMBOL(devdrv_query_master_pid_by_device_slave);
#else
/* only support user proc */
int devdrv_query_master_pid_by_host_slave(int slave_pid, u32 *master_pid)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    u32 bkt = 0;

    if (master_pid == NULL) {
        return -EINVAL;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each(d_info->proc_hash_table, bkt, d_sign, link) {
        struct devdrv_process_user_info *user_proc = devdrv_get_user_proc(d_sign, HOST_SIDE);
        int i;

        for (i = 0; i < DEVMNG_USER_PROC_MAX; i++) {
            if ((user_proc[i].valid == 1) && (user_proc[i].pid == slave_pid)) {
                *master_pid = d_sign->hostpid;
                ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                return 0;
            }
        }
    }

    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    return -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(devdrv_query_master_pid_by_host_slave);
#endif

STATIC unsigned int devdrv_query_process_host_pids_detail_by_pid(struct devdrv_process_sign *d_sign, int pid,
    unsigned int node, unsigned int vfid, devdrv_host_pids_info_t *host_pids_info)
{
    unsigned int i;
    unsigned int query_count = 0;

    for (i = 0; i < DEVDRV_PROCESS_CPTYPE_MAX; i++) {
        if (d_sign->devpid[node][vfid][i].slave_pid == pid) {
            host_pids_info->cp_type[query_count] = i;
            host_pids_info->chip_id[query_count] = node;
            host_pids_info->vfid[query_count] = vfid;
            host_pids_info->host_pids[query_count] = d_sign->hostpid;
            query_count++;
        }
    }

    return query_count;
}

int devdrv_query_process_host_pids_by_pid(int pid, devdrv_host_pids_info_t *host_pids_info)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    struct devdrv_process_user_info *user_proc = NULL;
    unsigned int query_count = 0;
    unsigned int i, j;
    u32 bkt = 0;

    if (host_pids_info == NULL) {
        devdrv_drv_err("Input host pid info is null.\n");
        return -EINVAL;
    }

    host_pids_info->vaild_num = 0;
    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each(d_info->proc_hash_table, bkt, d_sign, link) {
        for (i = 0; i < PID_MAP_DEVNUM; i++) {
            for (j = 0; j < VFID_NUM_MAX; j++) {
                query_count = devdrv_query_process_host_pids_detail_by_pid(d_sign, pid, i, j, host_pids_info);
                if (query_count != 0) {
                    host_pids_info->vaild_num = query_count;
                    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                    return 0;
                }
            }
        }

        user_proc = devdrv_query_user_proc(d_sign, devdrv_get_cur_run_side(), pid);
        if (user_proc != NULL) {
            host_pids_info->cp_type[query_count] = DEVDRV_PROCESS_USER;
            host_pids_info->chip_id[query_count] = user_proc->devid;
            host_pids_info->vfid[query_count] = user_proc->vfid;
            host_pids_info->host_pids[query_count] = d_sign->hostpid;
            query_count++;
            host_pids_info->vaild_num = query_count;
            ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
            return 0;
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    if (query_count == 0) {
        devdrv_drv_warn("Can't find cp_pid in hashtable. (pid=%d)\n", pid);
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_query_process_host_pids_by_pid);

int devdrv_check_hostpid(pid_t hostpid, unsigned int chip_id, unsigned int vfid)
{
    struct devdrv_manager_info *d_info = NULL;
    struct devdrv_process_sign *d_sign = NULL;
    u32 key, i;

    if (vfid >= VFID_NUM_MAX || chip_id >= PID_MAP_DEVNUM) {
        devdrv_drv_err("vfid(%u) or chip_id(%u) is invalid\n", vfid, chip_id);
        return -EINVAL;
    }

    d_info = devdrv_get_manager_info();
    if (d_info == NULL) {
        devdrv_drv_err("d_info is null, hostpid:%d.\n", hostpid);
        return -EINVAL;
    }

    key = (u32)hostpid & DEVDRV_PROC_HASH_TABLE_MASK;
    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    /*lint -e666 */
    ka_hash_for_each_possible(d_info->proc_hash_table, d_sign, link, key) {
        if (d_sign->hostpid != hostpid) {
            continue;
        }
        for (i = DEVDRV_PROCESS_CP1; i < DEVDRV_PROCESS_CPTYPE_MAX; i++) {
            if (d_sign->devpid[chip_id][vfid][i].slave_pid == current->tgid) {
                ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                return 0;
            }
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
    devdrv_drv_err("hostpid(%d) or chip_id(%u) or vfid(%u) is error.\n", hostpid, chip_id, vfid);
    return -EINVAL;
}
EXPORT_SYMBOL(devdrv_check_hostpid);

int devdrv_check_sign(pid_t hostpid, const char *sign, u32 len)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    u32 key;

    if ((sign == NULL) || (len != PROCESS_SIGN_LENGTH) || (d_info == NULL)) {
        devdrv_drv_err("sign is NULL(%d) or invalid length(%u) or d_info is NULL(%d).\n",
            (sign == NULL), len, (d_info == NULL));
        return -EINVAL;
    }

    key = (u32)hostpid & DEVDRV_PROC_HASH_TABLE_MASK;
    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    /*lint -e666 */
    ka_hash_for_each_possible(d_info->proc_hash_table, d_sign, link, key) {
        if (d_sign->hostpid == hostpid) {
            /* The sign is not used, not need check. */
            ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
            return 0;
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
    return -EINVAL;
}
EXPORT_SYMBOL(devdrv_check_sign);

int devdrv_get_dev_process(pid_t devpid)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    u32 bkt = 0;
    int i, j;

    if (d_info == NULL) {
        return 0;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each(d_info->proc_hash_table, bkt, d_sign, link) {
        for (i = 0; i < PID_MAP_DEVNUM; i++) {
            for (j = 0; j < VFID_NUM_MAX; j++) {
                if ((d_sign->devpid[i][j][DEVDRV_PROCESS_CP1].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_CP2].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_QS].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_DEV_ONLY].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_HCCP].slave_pid != devpid)) {
                    continue;
                }

                if (d_sign->host_process_status != 0) {
                    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                    return -EOWNERDEAD;
                }
                d_sign->in_use_count++;
                ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                return 0;
            }
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    return -ESRCH;
}
EXPORT_SYMBOL(devdrv_get_dev_process);

void devdrv_put_dev_process(pid_t devpid)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    u32 bkt = 0;
    int i, j;

    if (d_info == NULL) {
        return;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each(d_info->proc_hash_table, bkt, d_sign, link) {
        for (i = 0; i < PID_MAP_DEVNUM; i++) {
            for (j = 0; j < VFID_NUM_MAX; j++) {
                if ((d_sign->devpid[i][j][DEVDRV_PROCESS_CP1].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_CP2].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_QS].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_DEV_ONLY].slave_pid != devpid) &&
                    (d_sign->devpid[i][j][DEVDRV_PROCESS_HCCP].slave_pid != devpid)) {
                    continue;
                }
                d_sign->in_use_count =
                    (d_sign->in_use_count == 0) ? 0 : d_sign->in_use_count - 1;
                ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);
                return;
            }
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    return;
}
EXPORT_SYMBOL(devdrv_put_dev_process);

static bool devdrv_find_and_set_master_exit(u32 dev_id, struct devdrv_process_sign *sign, int *proc_stat)
{
    int i, j;
    struct devdrv_process_user_info *user_proc = NULL;

    for (i = 0; i < VFID_NUM_MAX; i++) {
        for (j = 0; j < DEVDRV_PROCESS_CPTYPE_MAX; j++) {
            if ((sign->devpid[dev_id][i][j].slave_pid != DEVMNG_PID_INVALID) &&
                (sign->devpid[dev_id][i][j].slave_pid != DEVMNG_PID_START_ONCE) &&
                (sign->devpid[dev_id][i][j].mode != AICPUFW_OFFLINE_PLAT)) {
                *proc_stat = (sign->in_use_count == 0) ? 0 : -EBUSY;
                sign->host_process_status = EOWNERDEAD;
                return true;
            }
        }
    }

    user_proc = devdrv_get_user_proc(sign, DEVICE_SIDE);
    for (i = 0; i < DEVMNG_USER_PROC_MAX; i++) {
        if ((user_proc[i].valid == 1) && (user_proc[i].mode == AICPUFW_ONLINE_PLAT)) {
            *proc_stat = (sign->in_use_count == 0) ? 0 : -EBUSY;
            sign->host_process_status = EOWNERDEAD;
            return true;
        }
    }

    return false;
}

int devdrv_notice_process_exit(u32 dev_id, u32 host_pid)
{
    int proc_stat = 0;
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign = NULL;
    u32 key = (u32)host_pid & DEVDRV_PROC_HASH_TABLE_MASK;

    if ((d_info == NULL) || (dev_id >= PID_MAP_DEVNUM)) {
        return 0;
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each_possible(d_info->proc_hash_table, d_sign, link, key) {
        if (d_sign->hostpid != host_pid) {
            continue;
        }

        if (devdrv_find_and_set_master_exit(dev_id, d_sign, &proc_stat)) {
            spin_unlock_bh(&d_info->proc_hash_table_lock);
            return proc_stat;
        }
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    return 0;
}

int devdrv_fop_query_host_pid(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_ioctl_para_query_pid para_info = {0};
    int node_id = numa_node_id();
    unsigned int phy_id = 0;
    unsigned int vfid = 0;
    int ret;
    pid_t tgid = 0;

    ret = devdrv_manager_container_logical_id_to_physical_id(0, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Transfer logical id to physical id failed. (ret=%d)\n", ret);
        return ret;
    }

    if (!devdrv_manager_is_pf_device(phy_id) || (vfid > 0)) {
        devdrv_drv_debug("Not support split mode. (phy_id=%u; vfid=%u)\n", phy_id, vfid);
        return -EOPNOTSUPP;
    }

    if (copy_from_user_safe(&para_info, (void *)(uintptr_t)arg,
        sizeof(struct devdrv_ioctl_para_query_pid)) != 0) {
        devdrv_drv_err("copy_from_user error. dev_id:%d\n", node_id);
        return -EINVAL;
    }

    if (para_info.pid <= 0) {
        devdrv_drv_err("invalid para pid(%d).\n", para_info.pid);
        return -EINVAL;
    }

#if defined(CFG_HOST_ENV) || defined(CFG_FEATURE_BIND_TGID)
    if (devdrv_get_tgid_by_pid(para_info.pid, &tgid) != 0) {
        devdrv_drv_err("Failed to get tgid by pid. (pid=%d)\n", para_info.pid);
        return -EINVAL;
    }
#else
    tgid = para_info.pid;
#endif

    ret = devdrv_query_process_host_pid(tgid, &para_info.chip_id, &para_info.vfid, &para_info.host_pid,
        &para_info.cp_type);
    if (ret != 0) {
        return -ESRCH;
    }
    devdrv_drv_debug("Query host_pid information. "
        "(node_id=%d; dev_id=%u, dev_pid=%d, tgid=%d, vfid=%u, hostpid=%d)",
        node_id, para_info.chip_id, tgid, current->tgid, para_info.vfid, para_info.host_pid);
#if defined(CFG_HOST_ENV) || defined(CFG_FEATURE_DEVICE_CONTAINER)
    if (devdrv_devpid_container_convert(&para_info.host_pid) != 0) {
        devdrv_drv_err("Failed to convert devpid. (pid=%d)\n", para_info.host_pid);
        return -EINVAL;
    }
#endif

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &para_info, sizeof(struct devdrv_ioctl_para_query_pid));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret(%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

#ifdef UT_VCAST
STATIC void devdrv_manager_set_sign_print(struct devdrv_process_sign *d_sign, u32 cp_type,
    u32 chip_id, u32 vfid, struct devdrv_process_sign *sign_print)
#else
STATIC inline void devdrv_manager_set_sign_print(struct devdrv_process_sign *d_sign, u32 cp_type,
    u32 chip_id, u32 vfid, struct devdrv_process_sign *sign_print)
#endif
{
    if (sign_print == NULL) {
        return;
    }

    sign_print->hostpid = d_sign->hostpid;
    sign_print->devpid[chip_id][vfid][cp_type] = d_sign->devpid[chip_id][vfid][cp_type];
}

STATIC void devdrv_release_sync_to_peer(pid_t master_pid, pid_t slave_pid,
    unsigned int dev_id, unsigned int vf_id, enum devdrv_process_type cp_type)
{
    struct devdrv_ioctl_para_bind_host_pid para_info;
    para_info.chip_id = dev_id;
    para_info.vfid = vf_id;
    para_info.cp_type = cp_type;
    para_info.host_pid = master_pid;
    para_info.mode = AICPUFW_ONLINE_PLAT;

    devdrv_drv_info("Release sync. (master_pid=%d; slave_pid=%d; dev_id=%u; vf_id=%u; cp_type=%d)\n",
        master_pid, slave_pid, dev_id, vf_id, cp_type);

    (void)devdrv_pid_map_sync_to_peer(&para_info, slave_pid, DELETE_PID);
}

void devdrv_release_try_to_sync_to_peer(pid_t slave_pid)
{
    unsigned int chip_id, vfid, master_pid;
    enum devdrv_process_type cp_type;

    int ret = devdrv_query_process_host_pid(slave_pid, &chip_id, &vfid, &master_pid, &cp_type);
    if (ret != 0) {
        return;
    }

#ifdef CFG_FEATURE_RC_MODE
    if (slave_pid == master_pid) { /* thread mode bind self */
        return;
    }
#endif

#ifdef CFG_HOST_ENV
    if (cp_type == DEVDRV_PROCESS_USER) {
        devdrv_release_sync_to_peer(master_pid, slave_pid, chip_id, vfid, cp_type);
    }
#else
    if (cp_type == DEVDRV_PROCESS_CP1) {
        devdrv_release_sync_to_peer(master_pid, slave_pid, chip_id, vfid, cp_type);
    }
#endif
}

STATIC void devdrv_manager_release_devpid(struct devdrv_process_sign *d_sign, pid_t devpid,
    struct devdrv_process_sign *sign_print, u32 *out_chip_id, u32 *out_vfid, ka_list_head_t *free_list)
{
    int current_side = devdrv_get_cur_run_side();
    struct devdrv_process_user_info *user_proc = devdrv_query_user_proc(d_sign, current_side, devpid);
    u32 chip_id, vfid;

    if (user_proc != NULL) {
        d_sign->cp_count--;
        *out_chip_id = user_proc->devid;
        *out_vfid = user_proc->vfid;
        user_proc->valid = 0;
    }

    for (chip_id = 0; chip_id < PID_MAP_DEVNUM; chip_id++) {
        for (vfid = 0; vfid < VFID_NUM_MAX; vfid++) {
            if (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].slave_pid == devpid) {
                devdrv_manager_set_sign_print(d_sign, DEVDRV_PROCESS_CP1, chip_id, vfid, sign_print);
                devdrv_manager_set_sign_print(d_sign, DEVDRV_PROCESS_CP2, chip_id, vfid, sign_print);
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_INVALID;
                d_sign->cp_count--;
                *out_chip_id = chip_id;
                *out_vfid = vfid;
            } else if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid == devpid) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].slave_pid != DEVMNG_PID_INVALID)) {
                devdrv_manager_set_sign_print(d_sign, DEVDRV_PROCESS_CP2, chip_id, vfid, sign_print);
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_START_ONCE;
                *out_chip_id = chip_id;
                *out_vfid = vfid;
            } else if (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid == devpid) {
                devdrv_manager_set_sign_print(d_sign, DEVDRV_PROCESS_CP2, chip_id, vfid, sign_print);
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_INVALID;
                *out_chip_id = chip_id;
                *out_vfid = vfid;
            }
            if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_DEV_ONLY].slave_pid == devpid) &&
                (current_side == DEVICE_SIDE)) {
                devdrv_manager_set_sign_print(d_sign, DEVDRV_PROCESS_DEV_ONLY, chip_id, vfid, sign_print);
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_DEV_ONLY].slave_pid = DEVMNG_PID_INVALID;
                d_sign->cp_count--;
                *out_chip_id = chip_id;
                *out_vfid = vfid;
            }
            if (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_QS].slave_pid == devpid) {
                devdrv_manager_set_sign_print(d_sign, DEVDRV_PROCESS_QS, chip_id, vfid, sign_print);
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_QS].slave_pid = DEVMNG_PID_INVALID;
                d_sign->cp_count--;
                *out_chip_id = chip_id;
                *out_vfid = vfid;
            }
            if (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_HCCP].slave_pid == devpid) {
                devdrv_manager_set_sign_print(d_sign, DEVDRV_PROCESS_HCCP, chip_id, vfid, sign_print);
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_HCCP].slave_pid = DEVMNG_PID_INVALID;
                d_sign->cp_count--;
                *out_chip_id = chip_id;
                *out_vfid = vfid;
            }
        }
    }

    if ((d_sign->cp_count == 0) && (d_sign->sync_proc_cnt == 0)) {
        ka_hash_del(&d_sign->link);
        ka_list_add(&d_sign->list, free_list);
    }
}

/* The devdrv_release_pid_with_start_time function is used only by the host. */
void devdrv_release_pid_with_start_time(struct devdrv_process_sign *d_sign, pid_t devpid, u64 start_time,
    ka_list_head_t *free_list, int *release_flag)
{
    int current_side = devdrv_get_cur_run_side();
    struct devdrv_process_user_info *user_proc = devdrv_query_user_proc(d_sign, current_side, devpid);
    u32 chip_id, vfid;

    if (user_proc != NULL && user_proc->start_time != start_time) {
        devdrv_drv_debug("Release user slave pid. (usrpid=%d; cur_pid=%d; usr_proc_time=%llu; cur_start_time=%llu)\n",
            user_proc->pid, devpid, user_proc->start_time, start_time);
        d_sign->cp_count--;
        user_proc->valid = 0;
        *release_flag = 1;
    }

    for (chip_id = 0; chip_id < PID_MAP_DEVNUM; chip_id++) {
        for (vfid = 0; vfid < VFID_NUM_MAX; vfid++) {
            if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].slave_pid == devpid) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].pid_start_time != start_time)) {
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].pid_start_time = 0;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].pid_start_time = 0;
                d_sign->cp_count--;
            } else if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid == devpid) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].pid_start_time != start_time) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].slave_pid != DEVMNG_PID_INVALID)) {
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_START_ONCE;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].pid_start_time = 0;
            } else if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid == devpid) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].pid_start_time != start_time)) {
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].pid_start_time = 0;
            }
            if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_DEV_ONLY].slave_pid == devpid) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_DEV_ONLY].pid_start_time != start_time) &&
                (current_side == DEVICE_SIDE)) {
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_DEV_ONLY].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_DEV_ONLY].pid_start_time = 0;
                d_sign->cp_count--;
            }
            if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_QS].slave_pid == devpid) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_QS].pid_start_time != start_time)) {
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_QS].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_QS].pid_start_time = 0;
                d_sign->cp_count--;
            }
            if ((d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_HCCP].slave_pid == devpid) &&
                (d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_HCCP].pid_start_time != start_time)) {
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_HCCP].slave_pid = DEVMNG_PID_INVALID;
                d_sign->devpid[chip_id][vfid][DEVDRV_PROCESS_HCCP].pid_start_time = 0;
                d_sign->cp_count--;
            }
        }
    }

    if ((d_sign->cp_count == 0) && (d_sign->sync_proc_cnt == 0)) {
        ka_hash_del(&d_sign->link);
        ka_list_add(&d_sign->list, free_list);
    }
}


void devdrv_manager_process_sign_release(pid_t devpid)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign_hostpid = NULL, *d_sign_devpid = NULL;
    ka_hlist_node_t *local_sign = NULL;
    struct devdrv_process_sign *free_sign = NULL, *free_sign_tmp = NULL;
    struct devdrv_process_sign *sign_print = NULL;
    u32 bkt;
    u32 chip_id = 0;
    u32 vfid = 0;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    ka_list_head_t free_list_head;
    KA_INIT_LIST_HEAD(&free_list_head);

    if (d_info == NULL) {
        devdrv_drv_err("dev_manager_info is NULL, devpid %d.\n", devpid);
        return;
    }

    /* for host side */
    ka_task_mutex_lock(&d_info->devdrv_sign_list_lock);
    if (!ka_list_empty_careful(&d_info->hostpid_list_header)) {
        ka_list_for_each_safe(pos, n, &d_info->hostpid_list_header) {
            d_sign_hostpid = ka_list_entry(pos, struct devdrv_process_sign, list);
            if (d_sign_hostpid->hostpid == devpid) {
                devdrv_drv_info("Delete hostpid sign list node. (hostpid=%d)\n", d_sign_hostpid->hostpid);
                ka_list_del(&d_sign_hostpid->list);
                d_info->devdrv_sign_count[d_sign_hostpid->docker_id]--;
                dbl_vfree(d_sign_hostpid);
                d_sign_hostpid = NULL;
                break;
            }
        }
    }
    ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);

    devdrv_release_try_to_sync_to_peer(devpid);

    sign_print = dbl_vmalloc(sizeof(struct devdrv_process_sign), GFP_KERNEL | __KA_GFP_ZERO | __GFP_ACCOUNT, PAGE_KERNEL);
    if (sign_print == NULL) {
        devdrv_drv_warn("Can not malloc for print, continue to release.");
    }

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each_safe(d_info->proc_hash_table, bkt, local_sign, d_sign_devpid, link) {
        /* release devpid if match */
        devdrv_manager_release_devpid(d_sign_devpid, devpid, sign_print, &chip_id, &vfid, &free_list_head);
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    ka_list_for_each_entry_safe(free_sign, free_sign_tmp, &free_list_head, list) {
        ka_list_del(&free_sign->list);
        devdrv_drv_info("Destroy master pid ctx when proc exit. (hostpid=%d; devpid=%d)", free_sign->hostpid, devpid);
        dbl_vfree(free_sign);
        free_sign = NULL;
    }

    /* print information about hashnode when release devpid */
    if (sign_print != NULL && sign_print->hostpid != 0) {
        devdrv_drv_info("Release slave pid. (hostpid=%d; devpid=%d; cp1_pid=%d; cp2_pid=%d; "
                        "dev_only_pid=%d; qs_pid=%d; hccp_pid=%d; user=%d; chip_id=%u; vfid=%u).\n",
                        sign_print->hostpid, devpid,
                        sign_print->devpid[chip_id][vfid][DEVDRV_PROCESS_CP1].slave_pid,
                        sign_print->devpid[chip_id][vfid][DEVDRV_PROCESS_CP2].slave_pid,
                        sign_print->devpid[chip_id][vfid][DEVDRV_PROCESS_DEV_ONLY].slave_pid,
                        sign_print->devpid[chip_id][vfid][DEVDRV_PROCESS_QS].slave_pid,
                        sign_print->devpid[chip_id][vfid][DEVDRV_PROCESS_HCCP].slave_pid,
                        sign_print->devpid[chip_id][vfid][DEVDRV_PROCESS_USER].slave_pid,
                        chip_id, vfid);
    }

    if (sign_print != NULL) {
        dbl_vfree(sign_print);
        sign_print = NULL;
    }

    return;
}

void devdrv_manager_free_hashtable(void)
{
    struct devdrv_process_sign *d_sign = NULL;
    ka_hlist_node_t *local_sign = NULL;
    u32 bkt;

    if (dev_manager_info == NULL) {
        devdrv_drv_err("dev_manager_info is NULL\n");
        return;
    }

    ka_hash_for_each_safe(dev_manager_info->proc_hash_table, bkt, local_sign, d_sign, link) {
        ka_hash_del(&d_sign->link);
        dbl_vfree(d_sign);
        d_sign = NULL;
    }
    return;
}
#else
int devdrv_manager_pid_map(void)
{
    return 0;
}

int devdrv_query_process_by_host_pid(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid)
{
    return 0;
}
EXPORT_SYMBOL(devdrv_query_process_by_host_pid);
#endif

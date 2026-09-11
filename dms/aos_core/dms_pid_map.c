/*************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
 * Description: dms_pid_map.c
 * FileName: dms_pid_map.c
 * Author: huawei
 * Created Time: Tue 09 Aug 2022 10:21:16 AM CST
 ************************************************************************/

/**
 * brief description about this document.
 * points to focus on.
 */

#ifndef AOSCORE_TEST_UT
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/timecounter.h>

#include "ka_hashtable_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_dfx_pub.h"

#include "dms_pid_map.h"
#include "devdrv_user_common.h"
#include "dms_define.h"
#include "devdrv_common.h"
#include "devdrv_manager_time.h"
#include "tsdrv_status.h"
#include "urd_notifier.h"
#include "dbl_mem_alloc_interface.h"
#include "urd_init.h"

spinlock_t dms_pid_map_lock;
DECLARE_HASHTABLE(dms_pid_map, 10);
typedef bool (*tsdrv_status_func)(unsigned int, unsigned int);

STATIC int dms_urd_host_pid_notifier(struct notifier_block *nb, unsigned long event, void *data);

static struct notifier_block dms_host_pid_urd_notifier = {
    .notifier_call = dms_urd_host_pid_notifier,
};

void dms_pid_map_init(void)
{
    int ret;

    ka_hash_init(dms_pid_map);
    spin_lock_init(&dms_pid_map_lock);

    ret = urd_register_notifier(&dms_host_pid_urd_notifier);
    if (ret) {
        dms_err("register urd notifier failed.(ret=%d)\n", ret);
    }
    return;
}

int devdrv_get_devnum(u32 *num_dev)
{
    *num_dev = 1;
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_get_devnum);

void dms_pid_map_exit(void)
{
    unsigned int bkt;
    struct hlist_node *hlink_node;
    pid_map_t *pid_node = NULL;
    int ret;

    ret = urd_unregister_notifier(&dms_host_pid_urd_notifier);
    if (ret) {
        dms_err("register urd notifier failed.(ret=%d)\n", ret);
    }

    spin_lock(&dms_pid_map_lock);
    hash_for_each_safe(dms_pid_map, bkt, hlink_node, pid_node, node) {
        ka_hash_del(&pid_node->node);
        dbl_kfree(pid_node);
        pid_node = NULL;
    }
    spin_unlock(&dms_pid_map_lock);
    return;
}

STATIC int dms_pid_map_add(int host_pid, int cp_type, pid_t dev_pid)
{
    int i;
    pid_map_t *pid_node = NULL;

    pid_node = dbl_kzalloc(sizeof(pid_map_t), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (pid_node == NULL) {
        return -ENOMEM;
    }

    for (i = 0; i < DEVDRV_PROCESS_CPTYPE_MAX; i++) {
        pid_node->devpid[i] = DMS_PID_INVALID;
    }
    pid_node->host_pid = host_pid;
    pid_node->devpid[cp_type] = dev_pid;
    hash_add(dms_pid_map, &pid_node->node, pid_node->host_pid);

    return 0;
}

STATIC void dms_remove_empty_pid_map_node(pid_map_t *pid_node)
{
    int i;

    for (i = 0; i < DEVDRV_PROCESS_CPTYPE_MAX; i++) {
        if (pid_node->devpid[i] != DMS_PID_INVALID) {
            break;
        }
    }
    if (i == DEVDRV_PROCESS_CPTYPE_MAX) {
        ka_hash_del(&pid_node->node);
        dbl_kfree(pid_node);
        pid_node = NULL;
    }
    return;
}

STATIC int dms_pid_map_update(pid_map_t *pid_node, int cp_type, int update_type, pid_t dev_pid)
{
    if (update_type == DMS_PID_MAP_UPDATE_BIND) {
        if (pid_node->devpid[cp_type] == DMS_PID_INVALID) {
            pid_node->devpid[cp_type] = dev_pid;
            return 0;
        } else {
            return -EINVAL;
        }
    } else if (update_type == DMS_PID_MAP_UPDATE_UNBIND) {
        if (pid_node->devpid[cp_type] == dev_pid) {
            pid_node->devpid[cp_type] = DMS_PID_INVALID;
            dms_remove_empty_pid_map_node(pid_node);
            return 0;
        } else {
            return -EINVAL;
        }
    }

    return -EINVAL;
}

STATIC pid_map_t* dms_pid_map_find(int host_pid)
{
    pid_map_t *target_node = NULL;

    hash_for_each_possible(dms_pid_map, target_node, node, host_pid) {
        if ((target_node != NULL) && (host_pid == target_node->host_pid)) {
            return target_node;
        }
    }
    return NULL;
}

STATIC int dms_pid_map_para_check(struct devdrv_ioctl_para_bind_host_pid *ioctl_para, unsigned int in_len)
{
    if ((ioctl_para == NULL) || (in_len != sizeof(struct devdrv_ioctl_para_bind_host_pid))) {
        dms_err("invalid parameters. (ioctl_para=%s; in_len=%u; expect_in_len=%lu)\n",
            (ioctl_para == NULL) ? "NULL" : "OK", in_len, sizeof(struct devdrv_ioctl_para_bind_host_pid));
        return -EINVAL;
    }
    // In scenario MDC, the following input parameters are fixed.
    if ((ioctl_para->chip_id != 0) || (ioctl_para->vfid != 0) ||
        (ioctl_para->mode != DMS_PID_MAP_OFFLINE_MODE)) {
        dms_err("para error, chip_id(%u), vfid(%u), mode(%d).\n",
                ioctl_para->chip_id, ioctl_para->vfid, ioctl_para->mode);
        return -EINVAL;
    }
    if ((ioctl_para->cp_type < 0) || (ioctl_para->cp_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        dms_err("invalid cp_type. (cp_type=%d)\n", ioctl_para->cp_type);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_drv_verify_sign(char *sign, pid_t *dev_pid)
{
    int ret;
    pid_t pid_tmp = -1;

    /* sign is used as dev_pid. */
    ret = memcpy_s(&pid_tmp, sizeof(pid_t), sign, sizeof(pid_t));
    if (ret != 0 || pid_tmp == -1) {
        dms_err("Failed to get the dev_pid. (ret=%d; pid=%d)\n", ret, pid_tmp);
        return -EINVAL;
    }

    *dev_pid = pid_tmp;

    return 0;
}

int dms_drv_bind_host_pid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;
    pid_t dev_pid = -1;
    pid_map_t *target_node = NULL;

    if ((in == NULL) || (in_len != sizeof(struct devdrv_ioctl_para_bind_host_pid))) {
        dms_err("invalid para, (in=%s; in_len=%u; expect_in_len=%lu;)\n",
            (in == NULL) ? "NULL" : "OK", in_len, sizeof(struct devdrv_ioctl_para_bind_host_pid));
        return -EINVAL;
    }
    struct devdrv_ioctl_para_bind_host_pid *ioctl_para = (struct devdrv_ioctl_para_bind_host_pid *)in;

    ret = dms_pid_map_para_check(ioctl_para, in_len);
    if (ret != 0) {
        dms_err("para check failed, (ret=%d)\n", ret);
        return ret;
    }

    ret = dms_drv_verify_sign(ioctl_para->sign, &dev_pid);
    if (ret != 0) {
        dms_err("Failed to get the dev_pid. (ret=%d; dev_pid=%d)\n", ret, dev_pid);
        return -EINVAL;
    }

    spin_lock(&dms_pid_map_lock);
    target_node = dms_pid_map_find(ioctl_para->host_pid);
    if (target_node == NULL) {
        ret = dms_pid_map_add(ioctl_para->host_pid, ioctl_para->cp_type, dev_pid);
        if (ret != 0) {
            spin_unlock(&dms_pid_map_lock);
            dms_err("add pid map failed, ret(%d).\n", ret);
            return ret;
        }
        spin_unlock(&dms_pid_map_lock);
    } else {
        ret = dms_pid_map_update(target_node, ioctl_para->cp_type, DMS_PID_MAP_UPDATE_BIND, dev_pid);
        if (ret != 0) {
            spin_unlock(&dms_pid_map_lock);
            dms_err("The binding relationship already exists, ret(%d), host_pid(%d), devpid(%d), cp_type(%d).\n",
                    ret, target_node->host_pid, target_node->devpid[ioctl_para->cp_type], ioctl_para->cp_type);
            return ret;
        }
        spin_unlock(&dms_pid_map_lock);
    }
    dms_info("pid bind success, host_pid(%d), devpid(%d), cp_type(%d).\n",
             ioctl_para->host_pid, dev_pid, ioctl_para->cp_type);
    return 0;
}

int dms_drv_unbind_host_pid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;
    pid_t dev_pid = -1;
    pid_map_t *target_node = NULL;

    if ((in == NULL) || (in_len != sizeof(struct devdrv_ioctl_para_bind_host_pid))) {
        dms_err("invalid para, (in=%s; in_len=%u; expect_in_len=%lu;)\n",
            (in == NULL) ? "NULL" : "OK", in_len, sizeof(struct devdrv_ioctl_para_bind_host_pid));
        return -EINVAL;
    }
    struct devdrv_ioctl_para_bind_host_pid *ioctl_para = (struct devdrv_ioctl_para_bind_host_pid *)in;

    ret = dms_pid_map_para_check(ioctl_para, in_len);
    if (ret != 0) {
        dms_err("para check failed, (ret=%d)\n", ret);
        return ret;
    }

    ret = dms_drv_verify_sign(ioctl_para->sign, &dev_pid);
    if (ret != 0) {
        dms_err("Failed to get the dev_pid. (ret=%d; dev_pid=%d)\n", ret, dev_pid);
        return -EINVAL;
    }

    spin_lock(&dms_pid_map_lock);
    target_node = dms_pid_map_find(ioctl_para->host_pid);
    if (target_node == NULL) {
        spin_unlock(&dms_pid_map_lock);
        dms_err("The binding relationship between host_pid(%d) and devpid(%d) does not exist.\n",
                ioctl_para->host_pid, dev_pid);
        return -EINVAL;
    }

    ret = dms_pid_map_update(target_node, ioctl_para->cp_type, DMS_PID_MAP_UPDATE_UNBIND, dev_pid);
    if (ret != 0) {
        spin_unlock(&dms_pid_map_lock);
        dms_err("The binding relationship between host_pid(%d) and devpid(%d) does not exist,"
                " pid_node->devpid[%d] = %d, ret(%d).\n",
                target_node->host_pid, dev_pid, ioctl_para->cp_type,
                target_node->devpid[ioctl_para->cp_type], ret);
        return ret;
    }
    spin_unlock(&dms_pid_map_lock);
    dms_info("pid unbind success, host_pid(%d), devpid(%d), cp_type(%d).\n",
             ioctl_para->host_pid, dev_pid, ioctl_para->cp_type);
    return 0;
}

/*
* When a process exits, if its PID exists with pad_map, it needs to be cleared.
*/
void dms_pidmap_release_process_id(pid_t pid)
{
    int i;
    unsigned int bkt;
    pid_map_t *tmp_node = NULL;
    struct hlist_node *hlink_node;
    bool pid_unbind_flag = false;
    unsigned int host_pid = DMS_PID_INVALID;

    spin_lock(&dms_pid_map_lock);
    hash_for_each_safe(dms_pid_map, bkt, hlink_node, tmp_node, node) {
        if (tmp_node != NULL) {
            for (i = 0; i < DEVDRV_PROCESS_CPTYPE_MAX; i++) {
                if (tmp_node->devpid[i] == pid) { // pid is device_pid
                    tmp_node->devpid[i] = DMS_PID_INVALID;
                    pid_unbind_flag = true;
                    host_pid = tmp_node->host_pid;
                    dms_remove_empty_pid_map_node(tmp_node);
                }
            }
        }
    }
    spin_unlock(&dms_pid_map_lock);
    if (pid_unbind_flag == true) {
        dms_info("pid unbind success, host_pid(%d), devpid(%d), cp_type(%d).\n", host_pid, pid, i);
    }
    return;
}

STATIC int dms_urd_host_pid_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
    struct urd_file_private_stru *urd_priv = NULL;

    switch (event) {
        case URD_NOTIFIER_RELEASE:
            urd_priv = (struct urd_file_private_stru *)data;
            if (urd_priv == NULL) {
                dms_err("invalid notifier data.\n");
                return -EINVAL;
            }

            dms_pidmap_release_process_id(urd_priv->owner_pid);
            return 0;
        default:
            dms_err("invalid notifier event.(event=0x%lx)\n", event);
            break;
    }

    return KA_NOTIFY_BAD;
}

int devdrv_query_process_by_host_pid(unsigned int host_pid, unsigned int chip_id,
                                     enum devdrv_process_type cp_type, unsigned int vfid, int *pid)
{
    pid_map_t *target_node = NULL;

    if ((chip_id != 0) || (vfid != 0) || (cp_type < 0) || (cp_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        dms_err("para error, (chip_id=%u, vfid=%u, cp_type=%d).\n", chip_id, vfid, cp_type);
        return -EINVAL;
    }
    if (pid == NULL) {
        dms_err("invalid para, pid is NULL.\n");
        return -EINVAL;
    }

    spin_lock(&dms_pid_map_lock);
    target_node = dms_pid_map_find(host_pid);
    if (target_node == NULL) {
        spin_unlock(&dms_pid_map_lock);
        dms_err("The binding relationship host_pid(%d) does not exist.\n", host_pid);
        return -EINVAL;
    }

    if (target_node->devpid[cp_type] == DMS_PID_INVALID) {
        spin_unlock(&dms_pid_map_lock);
        dms_err("The binding relationship does not exist, host_pid(%d), cp_type(%d).\n",
                host_pid, cp_type);
        return -EINVAL;
    }

    *pid = target_node->devpid[cp_type];
    spin_unlock(&dms_pid_map_lock);
    return 0;
}

int dms_drv_query_devpid_by_hostpid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct devdrv_ioctl_para_query_pid* ioctl_para = (struct devdrv_ioctl_para_query_pid*)in;
    int *devpid = (int *)out;

    if ((in == NULL) || (in_len != sizeof(struct devdrv_ioctl_para_query_pid)) ||
        (out == NULL) || (out_len != sizeof(int))) {
        dms_err("invalid parameters. (in=%s; in_len=%u; expect_in_len=%lu; out=%s; out_len=%u; expect_out_len=%lu)\n",
            (in == NULL) ? "NULL" : "OK", in_len, sizeof(struct devdrv_ioctl_para_query_pid),
            (out == NULL) ? "NULL" : "OK", out_len, sizeof(int));
        return -EINVAL;
    }
    // In scenario MDC, the following input parameters are fixed.
    if ((ioctl_para->chip_id != 0) || (ioctl_para->vfid != 0)) {
        dms_err("para error, (chip_id=%u; vfid=%u).\n",
                ioctl_para->chip_id, ioctl_para->vfid);
        return -EINVAL;
    }
    if ((ioctl_para->cp_type < 0) || (ioctl_para->cp_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        dms_err("invalid cp_type. (cp_type=%d)\n", ioctl_para->cp_type);
        return -EINVAL;
    }

    return devdrv_query_process_by_host_pid(ioctl_para->host_pid, ioctl_para->chip_id,
                                            ioctl_para->cp_type, ioctl_para->vfid, devpid);
}

int devdrv_query_process_by_devpid(pid_t pid, unsigned int *chip_id, unsigned int *vfid, pid_t *host_pid,
    enum devdrv_process_type *cp_type)
{
    int i;
    unsigned int bkt;
    pid_map_t *tmp_node = NULL;
    struct hlist_node *hlink_node;

    if (chip_id == NULL || vfid == NULL || host_pid == NULL || cp_type == NULL) {
        dms_err("Invalid parameters. (chip_id=%s; vfid=%s; cp_type=%s; host_pid=%s)\n",
                (chip_id == NULL) ? "NULL" : "OK", (vfid == NULL) ? "NULL" : "OK",
                (cp_type == NULL) ? "NULL" : "OK", (host_pid == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    spin_lock(&dms_pid_map_lock);
    hash_for_each_safe(dms_pid_map, bkt, hlink_node, tmp_node, node) {
        if (tmp_node == NULL) {
            continue;
        }
        for (i = 0; i < DEVDRV_PROCESS_CPTYPE_MAX; i++) {
            if (tmp_node->devpid[i] == pid) {
                *cp_type = i;
                *chip_id = 0;
                *vfid = 0;
                *host_pid = tmp_node->host_pid;
                spin_unlock(&dms_pid_map_lock);
                return 0;
            }
        }
    }

    spin_unlock(&dms_pid_map_lock);
    return -ESRCH;
}

int dms_drv_query_hostpid_by_devpid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct devdrv_ioctl_para_query_pid* para = (struct devdrv_ioctl_para_query_pid*)in;
    struct devdrv_ioctl_para_query_pid* result = (struct devdrv_ioctl_para_query_pid*)out;

    if ((in == NULL) || (in_len != sizeof(struct devdrv_ioctl_para_query_pid)) ||
        (out == NULL) || (out_len != sizeof(struct devdrv_ioctl_para_query_pid))) {
        dms_err("Invalid parameters. (in=%s; in_len=%u; expect_in_len=%lu; out=%s; out_len=%u; expect_out_len=%lu)\n",
            (in == NULL) ? "NULL" : "OK", in_len, sizeof(struct devdrv_ioctl_para_query_pid),
            (out == NULL) ? "NULL" : "OK", out_len, sizeof(struct devdrv_ioctl_para_query_pid));
        return -EINVAL;
    }

    return devdrv_query_process_by_devpid(para->pid, &result->chip_id, &result->vfid,
                                          &result->host_pid, &result->cp_type);
}

int dms_drv_get_process_sign(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct dev_process_sign *dev_sign = (struct dev_process_sign *)out;

    if ((out == NULL) || (out_len != sizeof(struct dev_process_sign))) {
        dms_err("invalid parameters. (out=%s; out_len=%u; expect_out_len=%lu)\n",
            (out == NULL) ? "NULL" : "OK", out_len, sizeof(struct dev_process_sign));
        return -EINVAL;
    }

    dev_sign->tgid = current->tgid;
    dms_info("get process_tgid(%d).\n", dev_sign->tgid);

    return 0;
}

/**
 * get cpu tick.
 * @return cpu tick
 */
u64 devdrv_manager_get_cpu_tick(void)
{
    u64 cnt = 0;
    isb();
    READ_SYSTEM_COUNTER_KER(cnt);
    return cnt;
}

int dms_get_hccl_device_info(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    unsigned int dev_id;
    struct devdrv_manager_hccl_devinfo *hccl_dev_info = (struct devdrv_manager_hccl_devinfo *)out;

    if ((in == NULL) || (in_len != sizeof(unsigned int)) ||
        (out == NULL) || (out_len != sizeof(struct devdrv_manager_hccl_devinfo))) {
        dms_err("invalid para, (in=%s; in_len=%u; expect_in_len=%lu; out=%s; out_len=%u; expect_in_len=%lu)\n",
            (in == NULL) ? "NULL" : "OK", in_len, sizeof(unsigned int),
            (out == NULL) ? "NULL" : "OK", out_len, sizeof(struct devdrv_manager_hccl_devinfo));
        return -EINVAL;
    }
    dev_id = *(unsigned int *)in;
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    if (dev_info == NULL) {
        dms_err("Invalid dev_info.\n");
        return -ENODEV;
    }

    hccl_dev_info->hardware_version = dev_info->hardware_version;
    hccl_dev_info->env_type = dev_info->env_type;
    hccl_dev_info->monotonic_raw_time_ns = ka_system_ktime_to_ns(ka_system_ktime_get_raw());
    hccl_dev_info->cpu_system_count = devdrv_manager_get_cpu_tick();
    hccl_dev_info->ts_num = dev_info->pdata->ts_num;
    hccl_dev_info->ai_cpu_core_num = dev_info->ai_cpu_core_num;
    hccl_dev_info->ai_cpu_core_id = dev_info->ai_cpu_core_id;
    hccl_dev_info->ai_cpu_bitmap = dev_info->aicpu_occupy_bitmap;
    hccl_dev_info->ctrl_cpu_id = dev_info->ctrl_cpu_id;
    hccl_dev_info->ctrl_cpu_core_num = dev_info->ctrl_cpu_core_num;
    hccl_dev_info->ctrl_cpu_endian_little = dev_info->ctrl_cpu_endian_little;
    hccl_dev_info->ts_cpu_core_num = dev_info->pdata->ts_pdata[0].ts_cpu_core_num;
    hccl_dev_info->ai_core_id = dev_info->ai_core_id;
    hccl_dev_info->ai_core_num = dev_info->ai_core_num;
    hccl_dev_info->aicore_freq = dev_info->aicore_freq;
    hccl_dev_info->vector_core_num = dev_info->vector_core_num;

    return 0;
}

int dms_get_core_spec_info(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    unsigned int dev_id;
    struct devdrv_hardware_spec *spec = (struct devdrv_hardware_spec *)out;

    if ((in == NULL) || (in_len != sizeof(unsigned int)) ||
        (out == NULL) || (out_len != sizeof(struct devdrv_hardware_spec))) {
        dms_err("invalid para, (in=%s; in_len=%u; expect_in_len=%lu; out=%s; out_len=%u; expect_in_len=%lu)\n",
            (in == NULL) ? "NULL" : "OK", in_len, sizeof(unsigned int),
            (out == NULL) ? "NULL" : "OK", out_len, sizeof(struct devdrv_hardware_spec));
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    if (dev_info == NULL || dev_info->pdata == NULL) {
        dms_err("Invalid dev_info.\n");
        return -ENODEV;
    }

    spec->ai_core_num = dev_info->ai_core_num;
    spec->first_ai_core_id = dev_info->ai_core_id;
    spec->ai_cpu_num = dev_info->ai_cpu_core_num;
    spec->first_ai_cpu_id = dev_info->ai_cpu_core_id;
    spec->ai_core_num_level = dev_info->pdata->ai_core_num_level;
    spec->ai_core_freq_level = dev_info->pdata->ai_core_freq_level;

    return 0;
}

int dms_get_core_inuse_info(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    unsigned int dev_id;
    struct devdrv_hardware_inuse *inuse = (struct devdrv_hardware_inuse *)out;
    tsdrv_status_func tsdrv_is_ts_work_func = NULL;

    if ((in == NULL) || (in_len != sizeof(unsigned int)) ||
        (out == NULL) || (out_len != sizeof(struct devdrv_hardware_inuse))) {
        dms_err("invalid para, (in=%s; in_len=%u; expect_in_len=%lu; out=%s; out_len=%u; expect_in_len=%lu)\n",
            (in == NULL) ? "NULL" : "OK", in_len, sizeof(unsigned int),
            (out == NULL) ? "NULL" : "OK", out_len, sizeof(struct devdrv_hardware_inuse));
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    if (dev_info == NULL) {
        dms_err("Invalid dev_info.\n");
        return -ENODEV;
    }

    tsdrv_is_ts_work_func = (tsdrv_status_func)(uintptr_t)kallsyms_lookup_name("tsdrv_is_ts_work");
    if (tsdrv_is_ts_work_func == NULL) {
        dms_err("Can not find symbol tsdrv_is_ts_work, failed.\n");
        return -EINVAL;
    }

    if (tsdrv_is_ts_work_func(dev_id, 0) == false) {
        dms_err("Ts driver is not working.\n");
        return -EFAULT;
    }

    /*  inuse为可用核数目，非已用核数目  */
    inuse->ai_core_num = dev_info->inuse.ai_core_num;
    inuse->ai_core_error_bitmap = dev_info->inuse.ai_core_error_bitmap;
    inuse->ai_cpu_num = dev_info->inuse.ai_cpu_num;
    inuse->ai_cpu_error_bitmap = dev_info->inuse.ai_cpu_error_bitmap;

    return 0;
}

#else
void dms_pidmap_ut_stub(void)
{
    return 0;
}
#endif

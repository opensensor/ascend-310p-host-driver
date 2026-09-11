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


#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/time.h>
#include "devdrv_manager_comm.h"
#include "devdrv_interface.h"
#ifndef AOS_LLVM_BUILD
#include "virtmng_interface.h"
#endif
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_kernel_version_adapt.h"
#include "dms_time.h"
#include "dms_define.h"
#include "urd_feature.h"
#include "dms_template.h"
#include "urd_acc_ctrl.h"
#include "dms_cmd_def.h"
#include "dms_notifier.h"
#include "kernel_version_adapt.h"
#include "devdrv_pcie.h"
#include "dms_hotreset.h"

#include "ka_memory_pub.h"

struct devdrv_container_table {
    ka_list_head_t item_list_head;
    ka_spinlock_t spinlock;
};
struct devdrv_container_main_pid {
    ka_pid_t tgid;
    ka_timespec64_t start_time;
    ka_timespec64_t real_start_time;
};

struct devdrv_container_item {
    ka_list_head_t item_list_node;
    struct devdrv_container_main_pid main_pid;
    u32 id;
    u32 conflict_flag;
    u32 dev_num;
    u32 pf_num;
    u32 vf_num;
    u32 dev_id[DEVDRV_MAX_DAVINCI_NUM];
    u32 pf_devid[DEVDRV_PF_DEV_MAX_NUM];
    u32 vf_devid[MAX_VDEV_NUM];
    u64 dev_list_map[DEVDRV_CONTIANER_NUM_OF_LONG];
    u64 container_id;

    ka_mnt_namespace_t *mnt_ns;
    ka_pid_namespace_t *pid_ns;
    ka_spinlock_t spinlock;
};

STATIC struct hotreset_task_info *g_device_task_info[DEVDRV_MAX_DAVINCI_NUM] = {0};
STATIC bool g_hotreset_executing_flag[DEVDRV_MAX_DAVINCI_NUM] = {false};
static spinlock_t g_hotreset_executing_spinlock;

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BASIC_POWER_INFO)
BEGIN_FEATURE_COMMAND()
#ifndef AOS_LLVM_BUILD
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_POWER_PRE_HOTRESET,
    NULL,
    NULL,
    DMS_SUPPORT_ROOT_PHY | DMS_PHYSICAL_ONLY,
    dms_power_pre_hotreset)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_POWER_PCIE_PRE_RESET,
    NULL,
    NULL,
    DMS_SUPPORT_ROOT_PHY | DMS_PHYSICAL_ONLY,
    dms_power_pcie_pre_reset)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_POWER_PCIE_HOT_RESET,
    NULL,
    NULL,
#ifdef CFG_FEATURE_SRIOV
    DMS_ENV_NOT_NORMAL_DOCKER | DMS_ACC_ROOT_ONLY | DMS_VDEV_VIRTUAL,
#else
    DMS_ENV_ALL | DMS_ACC_ROOT_ONLY,
#endif
    dms_power_pcie_hot_reset)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

STATIC struct hotreset_task_info *dms_get_device_task_info(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;

    if (dev_id >= DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("Wrong device id. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    device_task_info = g_device_task_info[dev_id];

    return device_task_info;
}

STATIC void dms_set_device_task_info(unsigned int dev_id, struct hotreset_task_info *device_task_info)
{
    g_device_task_info[dev_id] = device_task_info;
}

STATIC struct hotreset_task_info *dms_task_info_alloc(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;
    device_task_info = dbl_kzalloc(sizeof(struct hotreset_task_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (device_task_info == NULL) {
        dms_err("Kzalloc failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    init_rwsem(&device_task_info->task_rw_sema);

    return device_task_info;
}

STATIC unsigned long get_dms_task_info_cnt(unsigned int dev_id)
{
    return g_device_task_info[dev_id]->task_ref_cnt;
}

STATIC void set_hotreset_task_flag(unsigned int dev_id)
{
    set_bit(HOTRESET_TASK_FLAG_BIT, &g_device_task_info[dev_id]->task_flag);
}

STATIC void clear_hotreset_task_flag(unsigned int dev_id)
{
    clear_bit(HOTRESET_TASK_FLAG_BIT, &g_device_task_info[dev_id]->task_flag);
}

STATIC int get_hotreset_task_flag(unsigned int dev_id)
{
    return test_bit(HOTRESET_TASK_FLAG_BIT, &g_device_task_info[dev_id]->task_flag);
}

STATIC int dms_power_hotreset_notifier_handle(struct notifier_block *self, unsigned long event, void *data)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = (struct devdrv_info *)data;
    if ((data == NULL) || (event <= DMS_DEVICE_NOTIFIER_MIN) ||
        (event >= DMS_DEVICE_NOTIFIER_MAX)) {
        dms_err("Invalid parameter. (event=0x%lx; data=\"%s\")\n",
            event, data == NULL ? "NULL" : "OK");
        return NOTIFY_BAD;
    }

    dms_debug("Notifier hotreset handle success. (event=0x%lx; dev_id=%u)\n", event, dev_info->dev_id);
    return NOTIFY_DONE;
}

static struct notifier_block dms_power_hotreset_notifier = {
    .notifier_call = dms_power_hotreset_notifier_handle,
};

int dms_power_check_phy_mach(unsigned int dev_id)
{
    unsigned int host_flag;
    int ret;

    ret = devdrv_get_host_phy_mach_flag(dev_id, &host_flag);
    if (ret != 0) {
        dms_err("Devdrv_get_host_phy_mach_flag return value error. (devid=%u; ret=0x%x)\n", dev_id, ret);
        return ret;
    }

    if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
        dms_err("Device not phy mach. (devid=%u; host_flag=0x%x)\n", dev_id, host_flag);
        return -EPERM;
    }

#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_VFIO)
    if (vmng_get_device_split_mode(dev_id) != VMNG_NORMAL_NONE_SPLIT_MODE) {
        dms_err("The device has been split. Hot reset is not allowed. (dev_id=%u)\n", dev_id);
        return -EPERM;
    }
#endif

    return 0;
}

int dms_power_check_not_vmng(unsigned int dev_id)
{
#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_VFIO)
    if (vmng_get_device_split_mode(dev_id) != VMNG_NORMAL_NONE_SPLIT_MODE) {
        dms_err("Vdevice is not allowed! (dev_id=%u)\n", dev_id);
        return -EPERM;
    }
#endif

    return 0;
}

STATIC bool devdrv_check_hotreset_flag(unsigned int dev_id)
{
    int flag;
    down_write(&g_device_task_info[dev_id]->task_rw_sema);
    flag = get_hotreset_task_flag(dev_id);
    if (flag != 0) {
        up_write(&g_device_task_info[dev_id]->task_rw_sema);
        dms_info("Pre-hotreset has been triggered. (dev_id=%u)\n", dev_id);
        return false;
    }
    up_write(&g_device_task_info[dev_id]->task_rw_sema);

    return true;
}

STATIC int dms_power_check_all_phy_mach(void)
{
    unsigned int dev_num;
    unsigned int phys_id = 0;
    unsigned int vfid = 0;
    int i, ret;

    dev_num = devdrv_manager_get_devnum();
    if (dev_num > DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("Get invalid device num. (dev_num=%u)\n", dev_num);
        return -EINVAL;
    }
    for (i = 0; i < dev_num; ++i) {
        ret = devdrv_manager_container_logical_id_to_physical_id(i, &phys_id, &vfid);
        if (ret) {
            dms_err("Can't transform virt id. (virt_id=%d; ret=%d)\n", i, ret);
            return ret;
        }
        ret = dms_power_check_phy_mach(phys_id);
        if (ret) {
            return ret;
        }
    }

    return 0;
}

static void devdrv_uda_all_dev_ctrl_hotreset_cancel(u32 max_dev)
{
    u32 i;

    for (i = 0; i < max_dev; i++) {
        if (uda_is_udevid_exist(i)) {
            (void)uda_dev_ctrl(i, UDA_CTRL_HOTRESET_CANCEL);
        }
    }
}

STATIC void devdrv_uda_all_pre_dev_ctrl_hotreset_cancel(u32 max_dev)
{
    u32 i;

    for (i = 0; i < max_dev; i++) {
        if (uda_is_udevid_exist(i)) {
            (void)uda_dev_ctrl(i, UDA_CTRL_PRE_HOTRESET_CANCEL);
        }
    }
}

STATIC int devdrv_uda_all_dev_ctrl_hotreset(u32 max_dev)
{
    int ret;
    u32 i;

    for (i = 0; i < max_dev; i++) {
        if (!uda_is_udevid_exist(i) || (dms_get_device_task_info(i) == NULL)) {
            continue;
        }

        if (devdrv_check_hotreset_flag(i) == true) {
            ret = uda_dev_ctrl(i, UDA_CTRL_HOTRESET);
            if (ret != 0) {
                devdrv_uda_all_dev_ctrl_hotreset_cancel(i);
                devdrv_drv_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", i, ret);
                return ret;
            }
        }
    }
    return 0;
}

STATIC int devdrv_get_brother_udevid(u32 udevid, u32 *bro_udevid)
{
    int ret;
    u32 i;
    u32 udevid_master_id, tmp_master_id;

    ret = devdrv_get_master_devid_in_the_same_os(udevid, &udevid_master_id);
    if (ret != 0) {
        dms_warn("udevid master id cannot be obtained. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    for (i = 0; i < DEVDRV_PF_DEV_MAX_NUM; i++) {
        if (i == udevid) {
            continue;
        }
        if (!uda_is_udevid_exist(i)) {
            continue;
        }

        ret = devdrv_get_master_devid_in_the_same_os(i, &tmp_master_id);
        if (ret != 0) {
            dms_warn("Enumerated udevid master id cannot be obtained. (udevid=%u; ret=%d)\n", i, ret);
            continue;
        }

        if (udevid_master_id == tmp_master_id) {
            devdrv_drv_info("get smp brother devid. (ori_devid=%u; bro_devid=%u; ori_masterid=%u; tmp_masterid=%u)\n",
                udevid, i, udevid_master_id, tmp_master_id);
            *bro_udevid = i;
            return 0;
        }
    }
    return -ESRCH;
}

int devdrv_uda_one_dev_ctrl_hotreset(u32 udevid)
{
    int ret;
    u32 bro_udevid;

    if (dms_get_device_task_info(udevid) == NULL) {
        return 0;
    }

    if (devdrv_check_hotreset_flag(udevid) == true) {
        ret = uda_dev_ctrl(udevid, UDA_CTRL_HOTRESET);
        if (ret != 0) {
            devdrv_drv_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
    }

    ret = devdrv_get_brother_udevid(udevid, &bro_udevid);
    if (ret != 0) {
        return 0;
    }

    if (dms_get_device_task_info(bro_udevid) == NULL) {
        return 0;
    }

    if (devdrv_check_hotreset_flag(bro_udevid) == true) {
        ret = uda_dev_ctrl(bro_udevid, UDA_CTRL_HOTRESET);
        if (ret != 0) {
            (void)uda_dev_ctrl(udevid, UDA_CTRL_HOTRESET_CANCEL);
            devdrv_drv_err("Uda ctrl hotreset failed. (bro_udevid=%u; ret=%d)\n", bro_udevid, ret);
            return ret;
        }
    }

    return 0;
}
void devdrv_uda_one_dev_ctrl_hotreset_cancel(u32 udevid)
{
    int ret;
    u32 bro_udevid;

    (void)uda_dev_ctrl(udevid, UDA_CTRL_HOTRESET_CANCEL);

    ret = devdrv_get_brother_udevid(udevid, &bro_udevid);
    if (ret != 0) {
        return;
    }
    (void)uda_dev_ctrl(bro_udevid, UDA_CTRL_HOTRESET_CANCEL);
}

STATIC int dms_all_device_hotreset_excuting_check_set(void)
{
    u32 i;

    spin_lock(&g_hotreset_executing_spinlock);
    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        if (g_hotreset_executing_flag[i]) {
            spin_unlock(&g_hotreset_executing_spinlock);
            dms_err("Hot reset is being performed on some devices. (dev_id=%u)\n", i);
            return -EBUSY;
        }
    }
    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        g_hotreset_executing_flag[i] = true;
    }
    spin_unlock(&g_hotreset_executing_spinlock);
    return 0;
}

STATIC void dms_set_all_device_hotreset_excuting_flag(bool flag)
{
    u32 i;
    spin_lock(&g_hotreset_executing_spinlock);
    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        g_hotreset_executing_flag[i] = flag;
    }
    spin_unlock(&g_hotreset_executing_spinlock);
}

STATIC int dms_power_set_all_hot_reset(void)
{
    int ret = 0;

#ifdef CFG_FEATURE_TIMESYNC
    int i;
#endif
    ret = dms_power_check_all_phy_mach();
    if (ret) {
        dms_err("Permission deny or devid is invalid; (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_TIMESYNC
    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        set_time_need_update(i);
    }
#endif

    ret = dms_all_device_hotreset_excuting_check_set();
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_uda_all_dev_ctrl_hotreset(DEVDRV_MAX_DAVINCI_NUM);
    if (ret != 0) {
        dms_err("Notify all uda device hotreset failed. (ret=%d)\n", ret);
        goto ALL_DEV_HORESET_END;
    }

#ifdef CFG_FEATURE_SRIOV
    ret = devdrv_manager_check_and_disable_sriov(DEVDRV_RESET_ALL_DEVICE_ID);
    if (ret != 0) {
        dms_err("Failed to check and disable for all devices. (ret=%d)\n", ret);
        devdrv_uda_all_dev_ctrl_hotreset_cancel(DEVDRV_MAX_DAVINCI_NUM);
        goto ALL_DEV_HORESET_END;
    }
#endif

    ret = devdrv_hot_reset_device(ALL_DEVICE_HOTRESET_FLAG);
    if (ret != 0) {
        dms_err("Hotreset all devices failed. (ret=%d)\n", ret);
        devdrv_uda_all_dev_ctrl_hotreset_cancel(DEVDRV_MAX_DAVINCI_NUM);
    }

ALL_DEV_HORESET_END:
    dms_set_all_device_hotreset_excuting_flag(false);
    return ret;
}

int dms_container_task_struct_check(ka_task_struct_t *tsk)
{
    if (tsk == NULL) {
        devdrv_drv_err("tsk is NULL\n");
        return -EINVAL;
    }
#ifdef CONFIG_ARM64
    if (ka_task_get_task_fs(tsk) == NULL) {
        devdrv_drv_err("tsk->fs is NULL\n");
        return -EINVAL;
    }
#endif
    if (ka_task_get_nsproxy(tsk) == NULL) {
        devdrv_drv_err("tsk->nsproxy is NULL\n");
        return -ENODEV;
    }
    if (ka_task_get_mnt_ns(tsk) == NULL) {
        devdrv_drv_err("tsk->nsproxy->mnt_ns is NULL\n");
        return -EFAULT;
    }
    return 0;
}

STATIC u64 dms_get_host_mnt_ns(void)
{
    return (u64)(uintptr_t)ka_task_get_mnt_ns(ka_task_get_init_task());
}
STATIC int dms_get_current_mnt_ns(u64 *current_mnt_ns)
{
    int ret;
    ret = dms_container_task_struct_check(ka_task_get_current());
    if (ret) {
        devdrv_drv_err("current is invalid, ret=%d\n", ret);
        return -EINVAL;
    }

    *current_mnt_ns = (u64)(uintptr_t)ka_task_get_current_mnt_ns();
    return 0;
}

STATIC bool dms_container_is_admin_task(ka_task_struct_t *tsk)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
    unsigned int i;
#endif
    const ka_cred_t *cred = NULL;
    unsigned int user_id;
    ka_kernel_cap_t effective;
    ka_kernel_cap_t privileged = ka_system_get_privileged_kernel_cap();
    
    ka_task_rcu_read_lock();
    cred = __task_cred(tsk); //lint !e1058 !e64 !e666
    effective = cred->cap_effective;
    user_id = ka_task_get_cred_uid_val(cred);
    ka_task_rcu_read_unlock();
 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0) // 后续适配，os adapter暂无接口
    if ((effective.val & privileged.val) != privileged.val) {
        return false;
    }
#else
    CAP_FOR_EACH_U32(i)
    {
        if ((effective.cap[i] & privileged.cap[i]) != privileged.cap[i]) {
            return false;
        }
    }
#endif
    if (user_id == 0) {
        return true;
    } else {
        return false;
    }
}

STATIC int dms_get_current_mnt_ns_device(unsigned int dev_id, unsigned long long *mnt_ns_device_bitmap)
{
    unsigned int current_mnt_devids[DEVDRV_MAX_DAVINCI_NUM] = {0};
    unsigned int current_mnt_dev_num = 0;
    unsigned long long device_bitmap = 0;
    int ret = 0;
    int i = 0;
    current_mnt_dev_num = devdrv_manager_get_devnum();
    if ((current_mnt_dev_num > DEVDRV_MAX_DAVINCI_NUM) || current_mnt_dev_num > sizeof(device_bitmap)) {
        dms_err("get invalid devnum %d.\n", current_mnt_dev_num);
        return -EINVAL;
    }
    ret = devdrv_get_devids(current_mnt_devids, DEVDRV_MAX_DAVINCI_NUM);
    if (ret) {
        dms_err("get container devlist failed, ret(%d)\n", ret);
        return -ENODEV;
    }
    for (i = 0; i < current_mnt_dev_num; i++) {
            device_bitmap |= 1ull << current_mnt_devids[i];
    }
    dms_info("get device_bitmap:%llx\n", device_bitmap);
    *mnt_ns_device_bitmap = device_bitmap;

    return 0;
}

STATIC int dms_get_smp_device(unsigned int phy_id, unsigned long long *smp_device_bitmap, unsigned int *smp_device_num)
{
    struct devdrv_manager_info *manager_info = NULL;
    unsigned int num_dev = 0;
    unsigned int smp_device = 0;
    int current_masterid;
    int device_masterid;
    int smp_dev_num = 0;
    int ret = 0;
    int i;

    manager_info = devdrv_get_manager_info();
    if (manager_info == NULL) {
        dms_err("manager_info = 0x%pk", (void *)(uintptr_t)manager_info);
        return -EINVAL;
    }

    num_dev = manager_info->prob_num;

    ret = devdrv_get_master_devid_in_the_same_os(phy_id, &current_masterid);
    if (ret != 0) {
        dms_err("get master id failed. (phy_id=%u)\n", phy_id);
       return ret;
    }
    for (i = 0; i < num_dev; i++) {
        ret = devdrv_get_master_devid_in_the_same_os(i, &device_masterid);
        if (ret != 0) {
            dms_err("get master id failed. (phy_id=%u)\n", i);
            return ret;
        }
        if (device_masterid == current_masterid) {
            smp_device |= 1 << i;
            smp_dev_num++;
        }
    }

    *smp_device_bitmap = smp_device;
    *smp_device_num = smp_dev_num;

    return 0;
}

STATIC int dms_reset_smp_device(unsigned long long smp_device_bitmap, unsigned int smp_device_num)
{
    int ret = 0;
    int i;

    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        if ((smp_device_bitmap >> i & 1) == 1) {
#ifdef CFG_FEATURE_TIMESYNC
        set_time_need_update(i);
#endif
            ret = dms_notify_single_device_pre_hotreset(i);
            if (ret != 0) {
                dms_err("Notify single device hotreset failed. (ret=%d)\n", ret);
                return ret;
            }
        }
     }

    ret = devdrv_hot_reset_smp_device(smp_device_bitmap, smp_device_num);
    if (ret != 0) {
        dms_err("Hotreset smp device failed. (ret=%d)\n", ret);
        for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
            if ((smp_device_bitmap >> i & 1) == 1) {
                dms_notify_single_device_cancel_hotreset(i);
            }
        }
    }
    return ret;
}

STATIC int dms_hot_reset_smp_device(unsigned int phy_id)
{
    unsigned long long current_mnt_ns;
    unsigned long long smp_device_bitmap = 0;
    unsigned long long mnt_ns_device_bitmap = 0;
    unsigned int smp_device_num = 0;
    int ret = 0;

    ret = dms_get_current_mnt_ns(&current_mnt_ns);
    if (ret != 0) {
        dms_err("get current mnt ns error, (ret = %d)\n", ret);
        return -EINVAL;
    }

    ret = dms_get_current_mnt_ns_device(phy_id, &mnt_ns_device_bitmap);
    if (ret != 0) {
        dms_err("get mnt_ns device info failed. (phy_id=%u)\n", phy_id);
        return ret;
    }

    ret = dms_get_smp_device(phy_id, &smp_device_bitmap, &smp_device_num);
    if (ret != 0) {
        dms_err("get smp device failed. (phy_id=%u)\n", phy_id);
        return ret;
     }

    if ((current_mnt_ns == dms_get_host_mnt_ns()) || (dms_container_is_admin_task(ka_task_get_current()) != 0) ||
         ((smp_device_bitmap & mnt_ns_device_bitmap) == smp_device_bitmap)) {
        ret = dms_reset_smp_device(smp_device_bitmap, smp_device_num);
        if (ret != 0){
            dms_err("hotreset all smp device failed. (ret=%u, smp_device_bitmap=%llu)\n", ret, smp_device_bitmap);
        }
        return ret;
    } else {
        dms_err("hotreset failed, not all smp device in this container.(smp_device_bitmap=%llu\n"
            , smp_device_bitmap);
        return -E2BIG;
    }

    return ret;
}

STATIC int dms_single_device_hotreset_excuting_check_set(unsigned int phy_id, unsigned int bro_phy_id);
STATIC void dms_set_single_device_hotreset_excuting_flag(unsigned int phy_id, unsigned int bro_phy_id, bool flag);
STATIC int dms_hot_reset_single_device(unsigned int phy_id)
{
    int ret = 0;
    u32 bro_phy_id;

    if (devdrv_get_brother_udevid(phy_id, &bro_phy_id) != 0) {
        bro_phy_id = phy_id;
    }
#ifdef CFG_FEATURE_TIMESYNC
    set_time_need_update(phy_id);
#endif
    ret = dms_single_device_hotreset_excuting_check_set(phy_id, bro_phy_id);
    if (ret != 0) {
        dms_err("Notify single device hotreset failed. (dev_id=%u; bro_id=%u; ret=%d)\n", phy_id, bro_phy_id, ret);
        return ret;
    }
    ret = devdrv_uda_one_dev_ctrl_hotreset(phy_id);
    if (ret != 0) {
        dms_notify_single_device_cancel_hotreset(phy_id);
        devdrv_drv_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }
    ret = devdrv_hot_reset_device(phy_id);
    if (ret != 0) {
        dms_err("Hotreset single device failed. (dev_id=%u; ret=%d)\n", phy_id, ret);
        devdrv_uda_one_dev_ctrl_hotreset_cancel(phy_id);
    }

    dms_set_single_device_hotreset_excuting_flag(phy_id, bro_phy_id, false);
    return ret;
}
STATIC int dms_power_set_single_hot_reset(unsigned int virt_id)
{
    struct devdrv_manager_info *manager_info = NULL;
    unsigned int phy_id;
    unsigned int vfid;
    int ret = 0;

    /* the single device hot reset */
    ret = devdrv_manager_container_logical_id_to_physical_id(virt_id, &phy_id, &vfid);
    if (ret) {
        dms_err("Trans logical_id to physical_id failed. (devid=%u)\n", virt_id);
        return ret;
    }

    ret = dms_power_check_not_vmng(phy_id);
    if (ret) {
        dms_err("Permission deny or devid is invalid. (dev_id=%u; ret = %d)\n", virt_id, ret);
        return ret;
    }

    manager_info = devdrv_get_manager_info();
    if (manager_info == NULL) {
        dms_err("manager_info = 0x%pk", (void *)(uintptr_t)manager_info);
        return -EINVAL;
    }

    /* the contained device hot reset */
    if (manager_info->amp_or_smp == DEVMNG_SMP_MODE) {
        ret = dms_hot_reset_smp_device(phy_id);
        if (ret != 0) {
            dms_err("hot reset container device failed. (devid=%u)\n", phy_id);
            return ret;
        }
    } else {
        ret = dms_hot_reset_single_device(phy_id);
        if (ret != 0) {
            dms_err("hot reset container device failed. (devid=%u)\n", phy_id);
            return ret;
        }
    }
    return ret;
}

STATIC int dms_single_device_hotreset_excuting_check_set(unsigned int phy_id, unsigned int bro_phy_id)
{
    if(phy_id >= DEVDRV_MAX_DAVINCI_NUM || bro_phy_id >= DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("invalid parameters. (dev_phy_id=%u; bro_phy_id=%u)\n", phy_id, bro_phy_id);
        return -EBUSY;
    }

    spin_lock(&g_hotreset_executing_spinlock);
    if(g_hotreset_executing_flag[phy_id] || g_hotreset_executing_flag[bro_phy_id]) {
        spin_unlock(&g_hotreset_executing_spinlock);
        dms_err("Hotreset is executing. (dev_phy_id=%u; bro_phy_id=%u)\n", phy_id, bro_phy_id);
        return -EBUSY;
    }

    g_hotreset_executing_flag[phy_id] = true;
    g_hotreset_executing_flag[bro_phy_id] = true;

    spin_unlock(&g_hotreset_executing_spinlock);
    return 0;
}

STATIC void dms_set_single_device_hotreset_excuting_flag(unsigned int phy_id, unsigned int bro_phy_id, bool flag)
{
    spin_lock(&g_hotreset_executing_spinlock);

    g_hotreset_executing_flag[phy_id] = flag;
    g_hotreset_executing_flag[bro_phy_id] = flag;

    spin_unlock(&g_hotreset_executing_spinlock);
}

int dms_power_pcie_pre_reset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    unsigned int virt_id;
    int ret;
    unsigned int phys_id = 0;
    unsigned int vfid = 0;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    virt_id = *(unsigned int *)(uintptr_t)in;

    ret = devdrv_manager_container_logical_id_to_physical_id(virt_id, &phys_id, &vfid);
    if (ret != 0) {
        dms_err("Failed to transfer dev_id. (dev_id=%u)\n", virt_id);
        return -EFAULT;
    }

    if ((vfid != 0) || (!devdrv_manager_is_pf_device(phys_id))) {
        return -EOPNOTSUPP;
    }

    ret = dms_power_check_phy_mach(phys_id);
    if (ret) {
        dms_err("Permission deny or devid is invalid. (dev_id=%u; ret=%d)\n", phys_id, ret);
        return -EINVAL;
    }

    ret = devdrv_uda_one_dev_ctrl_hotreset(phys_id);
    if (ret != 0) {
        dms_err("Call uda_dev_ctrl failed, (phys_id=%u; ret=%d).\n", phys_id, ret);
        return ret;
    }

    ret = devdrv_pcie_prereset(phys_id);
    if (ret != 0) {
        dms_err("Hotreset failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        devdrv_uda_one_dev_ctrl_hotreset_cancel(phys_id);
    }

    return ret;
}

int dms_power_pcie_hot_reset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    unsigned int virt_id;
    int ret = 0;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    virt_id = *(unsigned int *)(uintptr_t)in;

    /* hot reset all devices */
    if (virt_id == DEVDRV_RESET_ALL_DEVICE_ID) {
        ret = dms_power_set_all_hot_reset();
        if (ret) {
            dms_err("Permission deny or devid is invalid. (dev_id=%d; ret=%d)\n", virt_id, ret);
        }
        return ret;
    }

    /* the single device hot reset */
    ret = dms_power_set_single_hot_reset(virt_id);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Set single device hotreset failed. (dev_id=%u; ret=%d)\n", virt_id, ret);
    }

    return ret;
}

void dms_notify_single_device_cancel_hotreset(unsigned int dev_id)
{
    clear_hotreset_task_flag(dev_id);
    return;
}

int dms_hotreset_task_cnt_increase(unsigned int dev_id)
{
    int flag;

    if (dms_get_device_task_info(dev_id) == NULL) {
        dms_err("Device not exist. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    down_write(&g_device_task_info[dev_id]->task_rw_sema);
    flag = get_hotreset_task_flag(dev_id);
    if (flag != 0) {
        dms_err("Hotreset is running. (dev_id=%u; flag=%d)\n", dev_id, flag);
        up_write(&g_device_task_info[dev_id]->task_rw_sema);
        return -EBUSY;
    }
    g_device_task_info[dev_id]->task_ref_cnt++;
    up_write(&g_device_task_info[dev_id]->task_rw_sema);

    return 0;
}

void dms_hotreset_task_cnt_decrease(unsigned int dev_id)
{
    if (dms_get_device_task_info(dev_id) == NULL) {
        dms_err("Device not exist. (dev_id=%u)\n", dev_id);
        return;
    }

    down_write(&g_device_task_info[dev_id]->task_rw_sema);
    g_device_task_info[dev_id]->task_ref_cnt--;
    up_write(&g_device_task_info[dev_id]->task_rw_sema);
}

int dms_power_pre_hotreset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret = 0;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    ret = dms_notify_all_device_pre_hotreset();
    if (ret != 0) {
        dms_err("Upgradetool set all device hotreset flag failed. (ret=%d)\n", ret);
    }

    return ret;
}

int dms_notify_all_device_pre_hotreset(void)
{
    unsigned int i = 0;
    int ret = 0;

    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        if (!uda_is_udevid_exist(i)) {
            continue;
        }

        ret = uda_dev_ctrl(i, UDA_CTRL_PRE_HOTRESET);
        if (ret != 0) {
            devdrv_uda_all_pre_dev_ctrl_hotreset_cancel(i);
            dms_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", i, ret);
            return ret;
        }
    }

    dms_event("Pre-hotreset set all devices flag success.\n");
    return ret;
}

int dms_notify_single_device_pre_hotreset(unsigned int dev_id)
{
    int ret = 0;

    ret = dms_notify_device_hotreset(dev_id);
    if (ret != 0) {
        dms_err("Notify hotreset failed. (ret=%d)\n", ret);
        return ret;
    }

    dms_event("Pre-hotreset set device flag success. (dev_id=%u)\n", dev_id);
    return ret;
}

int dms_notify_pre_device_hotreset(unsigned int dev_id)
{
    int flag;
    unsigned long task_cnt;

    if (dms_get_device_task_info(dev_id) == NULL) {
        dms_err("Device not exist. (dev_id=%u).\n", dev_id);
        return -EINVAL;
    }

    down_write(&g_device_task_info[dev_id]->task_rw_sema);
    flag = get_hotreset_task_flag(dev_id);
    if (flag != 0) {
        up_write(&g_device_task_info[dev_id]->task_rw_sema);
        dms_info("Pre-hotreset has been triggered. (dev_id=%u)\n", dev_id);
        return 0;
    }
    set_hotreset_task_flag(dev_id);

    task_cnt = get_dms_task_info_cnt(dev_id);
    if (task_cnt != 0) {
        clear_hotreset_task_flag(dev_id);
        up_write(&g_device_task_info[dev_id]->task_rw_sema);
        dms_err("Dms task_cnt not zero. (task_cnt=%lu; dev_id=%u)\n", task_cnt, dev_id);
        return -EINVAL;
    }
    up_write(&g_device_task_info[dev_id]->task_rw_sema);

    return 0;
}

int dms_notify_device_hotreset(unsigned int dev_id)
{
    unsigned long task_cnt;

    if (dms_get_device_task_info(dev_id) == NULL) {
        dms_err("Device not exist. (dev_id=%u).\n", dev_id);
        return -EINVAL;
    }

    down_write(&g_device_task_info[dev_id]->task_rw_sema);

    set_hotreset_task_flag(dev_id);

    task_cnt = get_dms_task_info_cnt(dev_id);
    if (task_cnt != 0) {
        clear_hotreset_task_flag(dev_id);
        up_write(&g_device_task_info[dev_id]->task_rw_sema);
        dms_err("Dms task_cnt not zero. (task_cnt=%lu; dev_id=%u)\n", task_cnt, dev_id);
        return -EINVAL;
    }
    up_write(&g_device_task_info[dev_id]->task_rw_sema);

    return 0;
}

int dms_power_hotreset_init(void)
{
    int ret;

    ret = dms_register_notifier(&dms_power_hotreset_notifier);
    if (ret) {
        dms_err("Dms event register notifier failed. (ret=%d)\n", ret);
        return ret;
    }
    spin_lock_init(&g_hotreset_executing_spinlock);
    CALL_INIT_MODULE(DMS_MODULE_BASIC_POWER_INFO);
    dms_debug("Dms power hotreset init success.\n");
    return 0;
}

void dms_power_hotreset_exit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_BASIC_POWER_INFO);
    (void)dms_unregister_notifier(&dms_power_hotreset_notifier);
    dms_debug("Dms power hotreset exit success.\n");
}

int dms_hotreset_task_init(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;

    device_task_info = dms_get_device_task_info(dev_id);
    if (device_task_info != NULL) {
        dms_info("Repeat init stop flag instance. (dev_id=%u)\n", dev_id);
    } else {
        device_task_info = dms_task_info_alloc(dev_id);
        if (device_task_info == NULL) {
            dms_err("Alloc stop flag info mem fail. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }
    }

    dms_set_device_task_info(dev_id, device_task_info);
    clear_hotreset_task_flag(dev_id);
    g_device_task_info[dev_id]->task_ref_cnt = 0;
    dms_debug("Dms hotreset task init success. (dev_id=%u)\n", dev_id);
    return 0;
}

void dms_hotreset_task_exit(void)
{
    struct hotreset_task_info *device_task_info = NULL;
    unsigned int i;

    for (i = 0; i < DEVDRV_MAX_DAVINCI_NUM; i++) {
        device_task_info = dms_get_device_task_info(i);
        if (device_task_info == NULL) {
            continue;
        }

        dms_set_device_task_info(i, NULL);
        dbl_kfree(device_task_info);
        device_task_info = NULL;
    }

    dms_debug("Dms hotreset task exit success.\n");
}
#ifdef CFG_FEATURE_SRIOV
void dms_hotreset_vf_task_exit(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;

    device_task_info = dms_get_device_task_info(dev_id);
    if (device_task_info == NULL) {
        return;
    }

    dms_set_device_task_info(dev_id, NULL);
    dbl_kfree(device_task_info);
    device_task_info = NULL;

    dms_debug("Dms hotreset task exit success.\n");
}

#endif

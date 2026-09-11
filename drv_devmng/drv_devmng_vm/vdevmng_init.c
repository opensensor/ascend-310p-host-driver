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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_dfx_pub.h"
#include "ka_ioctl_pub.h"

#include "ascend_hal_error.h"
#include "devdrv_manager_common.h"
#include "devdrv_common.h"
#include "vdevmng_init.h"
#include "devdrv_manager.h"
#include "devdrv_platform_resource.h"
#include "devdrv_manager_container.h"
#include "virtmng_interface.h"
#include "dms_event_distribute.h"
#include "dbl_mem_alloc_interface.h"
#include "hvdevmng_common.h"
#include "vdevmng_agent.h"
#include "vdevmng_cmd_proc.h"
#include "devdrv_manager_pid_map.h"
#include "devdrv_manager_comm.h"
#include "davinci_interface.h"
#include "davinci_api.h"

STATIC struct devdrv_manager_info *vdev_manager_info;
STATIC int vdevdrv_manager_check_fd(const struct file *filep);

struct devdrv_manager_info *vdevdrv_get_manager_info(void)
{
    return vdev_manager_info;
}

u32 vdevdrv_manager_get_devnum(void)
{
    return vdev_manager_info->num_dev;
}

struct devdrv_info *vdevdrv_manager_get_devdrv_info(u32 dev_id)
{
    if ((vdev_manager_info == NULL) || (dev_id >= VMNG_PDEV_MAX)) {
        devdrv_drv_err("invalid dev_id(%u) or vdev_manager_info(%pK) is NULL.\n", dev_id, vdev_manager_info);
        return NULL;
    }

    return vdev_manager_info->dev_info[dev_id];
}

int vdevdrv_manager_set_devdrv_info(u32 dev_id, struct devdrv_info *vdev_info)
{
    if ((vdev_manager_info == NULL) || (dev_id >= VMNG_PDEV_MAX)) {
        devdrv_drv_err("invalid dev_id(%u) or vdev_manager_info(%pK) is NULL.\n", dev_id, vdev_manager_info);
        return -EINVAL;
    }

    ka_task_spin_lock(&vdev_manager_info->spinlock);
    vdev_manager_info->dev_info[dev_id] = vdev_info;
    ka_task_spin_unlock(&vdev_manager_info->spinlock);

    return 0;
}

int vdevmng_set_devinfo_inc_devnum(u32 dev_id, struct devdrv_info *vdev_info)
{
    if ((dev_id >= VMNG_PDEV_MAX) || (vdev_manager_info == NULL) || (vdev_info == NULL)) {
        devdrv_drv_err("invalid dev_id(%u) or vdev_manager_info(%pK) or vdev_info(%pK) is NULL.\n",
            dev_id, vdev_manager_info, vdev_info);
        return -EINVAL;
    }

    ka_task_spin_lock(&vdev_manager_info->spinlock);
    if (vdev_manager_info->dev_info[dev_id] != NULL) {
        ka_task_spin_unlock(&vdev_manager_info->spinlock);
        devdrv_drv_err("vdev_info is not NULL, dev_id(%u).\n", dev_id);
        return -ENODEV;
    }

    if (vdev_manager_info->num_dev >= VMNG_PDEV_MAX) {
        ka_task_spin_unlock(&vdev_manager_info->spinlock);
        devdrv_drv_err("wrong device num, num_dev(%u), dev_id(%u).\n", vdev_manager_info->num_dev, dev_id);
        return -EFAULT;
    }

    vdev_manager_info->dev_info[dev_id] = vdev_info;
    vdev_manager_info->num_dev++;
    ka_task_spin_unlock(&vdev_manager_info->spinlock);

    return 0;
}

int vdevdrv_manager_reset_devinfo_dec_devnum(u32 dev_id)
{
    if ((dev_id >= VMNG_PDEV_MAX) || (vdev_manager_info == NULL)) {
        devdrv_drv_err("invalid dev_id(%u), or vdev_manager_info(%pK) is NULL.\n", dev_id, vdev_manager_info);
        return -EINVAL;
    }

    ka_task_spin_lock(&vdev_manager_info->spinlock);
    if (vdev_manager_info->dev_info[dev_id] == NULL) {
        ka_task_spin_unlock(&vdev_manager_info->spinlock);
        devdrv_drv_err("dev_info is not NULL, dev_id(%u).\n", dev_id);
        return -ENODEV;
    }

    if ((vdev_manager_info->num_dev > VMNG_PDEV_MAX) || (vdev_manager_info->num_dev == 0)) {
        ka_task_spin_unlock(&vdev_manager_info->spinlock);
        devdrv_drv_err("wrong device num, num_dev(%u), dev_id(%u).\n", vdev_manager_info->num_dev, dev_id);
        return -EFAULT;
    }

    vdev_manager_info->dev_info[dev_id] = NULL;
    vdev_manager_info->num_dev--;
    ka_task_spin_unlock(&vdev_manager_info->spinlock);

    return 0;
}

STATIC int vdevdrv_manager_ioctl_not_support(struct file *filep, unsigned int cmd, unsigned long arg)
{
    (void)filep;
    (void)cmd;
    (void)arg;
    return -EOPNOTSUPP;
}

STATIC int (*const vdevmng_ioctl_handlers[DEVDRV_MANAGER_CMD_MAX_NR])(struct file *filep, unsigned int cmd,
    unsigned long arg) = {
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_PCIINFO)] = vdevdrv_manager_get_pci_info,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_PLATINFO)] = vdevdrv_manager_ioctl_get_plat_info,
        [_KA_IOC_NR(DEVDRV_MANAGER_DEVICE_STATUS)] = vdevdrv_manager_get_device_status,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CORE_SPEC)] = vdevdrv_get_core_spec,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CORE_INUSE)] = vdevdrv_get_core_inuse,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVINFO)] = vdevmng_get_devinfo_from_vdev_info,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVID_BY_LOCALDEVID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEV_INFO_BY_PHYID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_PCIE_ID_INFO)] = vdevdrv_get_pcie_id,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_VOLTAGE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_TEMPERATURE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_TSENSOR)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_AI_USE_RATE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_FREQUENCY)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_POWER)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_HEALTH_CODE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_ERROR_CODE)] = vdevdrv_get_error_code,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_DDR_CAPACITY)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_LPM3_SMOKE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_BLACK_BOX_GET_EXCEPTION)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_DEVICE_MEMORY_DUMP)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_DEVICE_RESET_INFORM)] = vdevdrv_manager_not_support,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_MODULE_STATUS)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_MINI_BOARD_ID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_PRE_RESET)] = vdevdrv_manager_not_support,
        [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_RESCAN)] = vdevdrv_manager_not_support,
        [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_HOT_RESET)] = vdevdrv_manager_not_support,
        [_KA_IOC_NR(DEVDRV_MANAGER_P2P_ATTR)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_ALLOC_HOST_DMA_ADDR)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_READ)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_SRAM_WRITE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_EMMC_VOLTAGE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVICE_BOOT_STATUS)] = vdrv_get_device_boot_status,
        [_KA_IOC_NR(DEVDRV_MANAGER_ENABLE_EFUSE_LDO)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_DISABLE_EFUSE_LDO)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_CONTAINER_CMD)] = vdevdrv_manager_container_cmd,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_HOST_PHY_MACH_FLAG)] = vdevdrv_manager_get_host_phy_mach_flag,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_LOCAL_DEVICEIDS)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_IMU_SMOKE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_SET_NEW_TIME)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CREATE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_OPEN)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CLOSE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_DESTROY)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_SET_PID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CPU_INFO)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_SEND_TO_IMU)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_RECV_FROM_IMU)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_IMU_INFO)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_CONFIG_ECC_ENABLE)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_PROBE_NUM)] = vdevdrv_manager_get_probe_num,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_PROBE_LIST)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_DEBUG_INFORM)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_COMPUTE_POWER)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_SYNC_MATRIX_DAEMON_READY)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_BBOX_ERRSTR)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_IMU_DDR_READ)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_SLOT_ID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_APPMON_BBOX_EXCEPTION_CMD)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CONTAINER_FLAG)] = vdevdrv_manager_get_container_flag,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_PROCESS_SIGN)] = vdevdrv_manager_get_process_sign,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_MASTER_DEV_IN_THE_SAME_OS)] = vdevdrv_manager_ioctl_not_support,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_LOCAL_DEV_ID_BY_HOST_DEV_ID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_BOOT_DEV_ID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_TSDRV_DEV_COM_INFO)] = vdevdrv_manager_get_tsdrv_dev_com_info,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CAPABILITY_GROUP_INFO)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_PASSTHRU_MCU)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_P2P_CAPABILITY)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_ETH_ID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_BIND_PID_ID)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_H2D_DEVINFO)] = vdevmng_get_devinfo_from_phy_machine,
        [_KA_IOC_NR(DEVDRV_MANAGER_CREATE_VDEV)] = vdevdrv_manager_not_permit,
        [_KA_IOC_NR(DEVDRV_MANAGER_DESTROY_VDEV)] = vdevdrv_manager_not_permit,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_VDEVINFO)] = vdevdrv_manager_not_permit,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CONSOLE_LOG_LEVEL)] = vdevdrv_manager_get_console_loglevel,
        [_KA_IOC_NR(DEVDRV_MANAGER_UPDATE_STARTUP_STATUS)] = NULL,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_STARTUP_STATUS)] = vdevdrv_manager_get_device_startup_status,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVICE_HEALTH_STATUS)] = vdevdrv_manager_get_device_health_status,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEV_RESOURCE_INFO)] = vdevdrv_manager_get_dev_resource_info,
        [_KA_IOC_NR(DEVDRV_MANAGER_SET_SVM_VDEVINFO)] = vdevdrv_manager_ioctl_not_support,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_SVM_VDEVINFO)] = vdevdrv_manager_ioctl_not_support,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CHIP_INFO)] = vdevdrv_manager_ioctl_get_chip_info,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_OSC_FREQ)] = vdevdrv_manager_get_osc_freq,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_CURRENT_AIC_FREQ)] = vdevdrv_manager_get_current_aic_freq,
        [_KA_IOC_NR(DEVDRV_MANAGER_GET_HOST_KERN_LOG)] = vdevdrv_get_host_kern_log,
};

STATIC long vdevdrv_manager_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    if ((vdev_manager_info == NULL) || (arg == 0)) {
        devdrv_drv_err("invalid parameter, "
                       "vdev_manager_info = %pK, "
                       "arg = 0x%lx\n",
                       vdev_manager_info, arg);
        return -EINVAL;
    }

    if (vdevdrv_manager_check_fd((const struct file *)filep) != 0) {
        return -EINVAL;
    }

    if (_KA_IOC_TYPE(cmd) != DEVDRV_MANAGER_MAGIC) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (_KA_IOC_NR(cmd) >= DEVDRV_MANAGER_CMD_MAX_NR) {
        devdrv_drv_err("cmd out of range, cmd = %u, max value = %u\n", _KA_IOC_NR(cmd), DEVDRV_MANAGER_CMD_MAX_NR);
        return -EINVAL;
    }

    if (vdevmng_ioctl_handlers[_KA_IOC_NR(cmd)] == NULL) {
        devdrv_drv_err("not support this cmd(%u).\n", _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    return vdevmng_ioctl_handlers[_KA_IOC_NR(cmd)](filep, cmd, arg);
}

STATIC ssize_t vdevdrv_manager_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    return 0;
}

STATIC struct devdrv_manager_context *vdevdrv_manager_context_init(void)
{
    struct devdrv_manager_context *dev_manager_context = NULL;

    dev_manager_context = dbl_vmalloc(sizeof(struct devdrv_manager_context),
        KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (dev_manager_context == NULL) {
        devdrv_drv_err("vmalloc dev_manager_context is NULL.\n");
        return NULL;
    }

    dev_manager_context->tgid = current->tgid;

    return dev_manager_context;
}

STATIC void vdevdrv_manager_context_uninit(struct devdrv_manager_context *dev_manager_context)
{
    if (dev_manager_context == NULL) {
        return;
    }

    dbl_vfree(dev_manager_context);
    dev_manager_context = NULL;
}

STATIC int vdevdrv_manager_check_fd(const struct file *filep)
{
    struct devdrv_manager_context *dev_manager_context = NULL;

    if (filep == NULL) {
        devdrv_drv_err("Invalid parameter. (tgid=%d)\n", current->tgid);
        return -EINVAL;
    }

    dev_manager_context = (struct devdrv_manager_context *)filep->private_data;
    if (dev_manager_context == NULL) {
        devdrv_drv_err("context is NULL. (tgid=%d)\n", current->tgid);
        return -ENOMEM;
    }

    if (dev_manager_context->tgid != current->tgid) {
        devdrv_drv_err("The fd does not match pid. (context_tgid=%d; tgid=%d)\n",
            dev_manager_context->tgid, current->tgid);
        return -EBADFD;
    }
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
STATIC void vdevdrv_check_pid_map_process_sign(pid_t tgid, u64 start_time)
{
    struct devdrv_manager_info *d_info = vdevdrv_get_manager_info();
    struct devdrv_process_sign *d_sign_hostpid = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    if (d_info == NULL) {
        devdrv_drv_err("dev_manager_info is NULL. (tgid=%d)\n", tgid);
        return;
    }

    mutex_lock(&d_info->devdrv_sign_list_lock);
    if (!list_empty_careful(&d_info->hostpid_list_header)) {
        list_for_each_safe(pos, n, &d_info->hostpid_list_header) {
            d_sign_hostpid = list_entry(pos, struct devdrv_process_sign, list);
            if (d_sign_hostpid->hostpid == tgid && d_sign_hostpid->hostpid_start_time != start_time) {
                devdrv_drv_debug("Delete sign list node. (master_pid=%d; tgid=%d; start_time=%llu; cur_time=%llu)\n",
                    d_sign_hostpid->hostpid, tgid, d_sign_hostpid->hostpid_start_time, start_time);
                list_del(&d_sign_hostpid->list);
                d_info->devdrv_sign_count[d_sign_hostpid->docker_id]--;
                dbl_kfree(d_sign_hostpid);
                d_sign_hostpid = NULL;
                break;
            }
        }
    }
    mutex_unlock(&d_info->devdrv_sign_list_lock);
}
#endif

STATIC int vdevdrv_manager_open(struct inode *inode, struct file *filep)
{
    struct devdrv_manager_context *dev_manager_context = NULL;
    u32 docker_id = MAX_DOCKER_NUM;

    dev_manager_context = vdevdrv_manager_context_init();
    if (dev_manager_context == NULL) {
        devdrv_drv_err("context init failed\n");
        return -ENOMEM;
    }

    dev_manager_context->docker_id = docker_id;
    filep->private_data = dev_manager_context;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    vdevdrv_check_pid_map_process_sign(dev_manager_context->tgid, dev_manager_context->start_time);
#endif

    return 0;
}

STATIC void vdevdrv_manager_process_sign_release(pid_t pid)
{
    struct devdrv_manager_info *d_info = NULL;
    struct devdrv_process_sign *d_sign = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    d_info = vdevdrv_get_manager_info();
    if (d_info == NULL) {
        devdrv_drv_err("d_info is NULL.\n");
        return;
    }

    ka_task_mutex_lock(&d_info->devdrv_sign_list_lock);
    if (!ka_list_empty_careful(&d_info->hostpid_list_header)) {
        ka_list_for_each_safe(pos, n, &d_info->hostpid_list_header) {
            d_sign = list_entry(pos, struct devdrv_process_sign, list);
            if (d_sign->hostpid == pid) {
                devdrv_drv_info("Delete hostpid sign list node. (pid=%d; start_time=%llu)\n",
                    d_sign->hostpid, d_sign->hostpid_start_time);
                ka_list_del(&d_sign->list);
                d_info->devdrv_sign_count[d_sign->docker_id]--;
                dbl_kfree(d_sign);
                d_sign = NULL;
                break;
            }
        }
    }
    ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);
}

int devdrv_manager_get_docker_id(u32 *docker_id)
{
    *docker_id = MAX_DOCKER_NUM;
    return 0;
}
EXPORT_SYMBOL(devdrv_manager_get_docker_id);

devmm_get_device_accounting_pids_ops get_device_pids_from_devmm = NULL;
static KA_TASK_DEFINE_MUTEX(devmm_get_pids_mutex);
int devdrv_manager_get_process_pids_register(devmm_get_device_accounting_pids_ops func)
{
    if (func == NULL) {
        devdrv_drv_err("Register pids operation function null.\n");
        return -EINVAL;
    }

    ka_task_mutex_lock(&devmm_get_pids_mutex);
    get_device_pids_from_devmm = func;
    ka_task_mutex_unlock(&devmm_get_pids_mutex);
    return 0;
}
EXPORT_SYMBOL(devdrv_manager_get_process_pids_register);

void devdrv_manager_get_process_pids_unregister(void)
{
    ka_task_mutex_lock(&devmm_get_pids_mutex);
    get_device_pids_from_devmm = NULL;
    ka_task_mutex_unlock(&devmm_get_pids_mutex);
}
EXPORT_SYMBOL(devdrv_manager_get_process_pids_unregister);

STATIC int vdevdrv_manager_get_container_pid(pid_t hostpid, pid_t *container_pid)
{
    ka_task_struct_t *tsk = NULL;
    ka_struct_pid_t *pgrp = NULL;

    ka_task_rcu_read_lock();
    ka_for_each_process(tsk) {
        if ((tsk != NULL) && (tsk->pid == hostpid)) {
            pgrp = task_pid(tsk);
            if (pgrp == NULL) {
                ka_task_rcu_read_unlock();
                devdrv_drv_err("The pgrp parameter is NULL.\n");
                return -EINVAL;
            }
            *container_pid = pgrp->numbers[pgrp->level].nr;
            ka_task_rcu_read_unlock();
            return 0;
        }
    }

    ka_task_rcu_read_unlock();
    devdrv_drv_err("Can not find the tsk.\n");
    return -EINVAL;
}

int vdevdrv_manager_get_accounting_pid(struct devdrv_resource_info *dinfo)
{
    int ret;
    pid_t pid = -1;
    u32 i;
    u32 docker_id = 0;
    int out_count = 0;

    if (dinfo->buf_len > DEVDRV_MAX_PAYLOAD_LEN) {
        dinfo->buf_len = DEVDRV_MAX_PAYLOAD_LEN;
    }

    ret = devdrv_manager_get_docker_id(&docker_id);
    if (ret) {
        devdrv_drv_err("The devdrv_manager_get_container_mnt_ns failed. (devid=%u; docker_id=%d; ret=%d)\n",
            dinfo->devid, docker_id, ret);
        return ret;
    }

    ka_task_mutex_lock(&devmm_get_pids_mutex);
    if (get_device_pids_from_devmm == NULL) {
        ka_task_mutex_unlock(&devmm_get_pids_mutex);
        devdrv_drv_err("The devmm_get_device_accounting_pids is NULL.\n");
        return -EINVAL;
    }

    out_count = get_device_pids_from_devmm(docker_id, dinfo->devid, 0,
        (pid_t *)dinfo->buf, (dinfo->buf_len / sizeof(pid_t)));
    if (out_count < 0) {
        ka_task_mutex_unlock(&devmm_get_pids_mutex);
        devdrv_drv_err("Failed to obtain the PID list of the process from SVM. (devid=%u; docker_id=%d; count=%d)\n",
            dinfo->devid, docker_id, out_count);
        return -EINVAL;
    }
    ka_task_mutex_unlock(&devmm_get_pids_mutex);

    if (out_count > DEVDRV_MAX_PAYLOAD_LEN / sizeof(pid_t)) {
        out_count = DEVDRV_MAX_PAYLOAD_LEN / sizeof(pid_t);
    }

    for (i = 0; i < out_count; i++) {
        ret = vdevdrv_manager_get_container_pid(((pid_t *)dinfo->buf)[i], &pid);
        if (ret != 0) {
            devdrv_drv_err("devdrv_manager_get_container_pid failed. (devid=%u; hostpid=%d; ret=%d)\n",
                dinfo->devid, ((pid_t *)dinfo->buf)[i], ret);
            return ret;
        }
        ((pid_t *)dinfo->buf)[i] = pid;
    }
    dinfo->buf_len = out_count * sizeof(pid_t);

    return 0;
}

STATIC int vdevdrv_manager_release(struct inode *inode, struct file *filep)
{
    struct devdrv_manager_context *dev_manager_context = NULL;

    dev_manager_context = (struct devdrv_manager_context *)filep->private_data;
    if (dev_manager_context == NULL) {
        devdrv_drv_err("context is NULL\n");
        return -ENOMEM;
    }

    vdevdrv_manager_process_sign_release(dev_manager_context->tgid);
    vdevdrv_manager_context_uninit(dev_manager_context);
    return 0;
}

static const struct pci_device_id g_vdevmng_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500), 0 },
    {}};
MODULE_DEVICE_TABLE(pci, g_vdevmng_tbl);


const struct file_operations vdevdrv_manager_file_operations = {
    .owner = THIS_MODULE,
    .read = vdevdrv_manager_read,
    .open = vdevdrv_manager_open,
    .release = vdevdrv_manager_release,
    .unlocked_ioctl = vdevdrv_manager_ioctl,
};

STATIC int vdevdrv_manager_process_exit(struct notifier_block *nb, unsigned long mode, void *data)
{
    struct task_struct *task = (struct task_struct *)data;
    (void)mode;
    (void)nb;

    if ((task != NULL) && (task->mm != NULL) && (task->tgid != 0) && (task->tgid == task->pid) &&
        ascend_intf_is_pid_init(task->tgid, DAVINCI_INTF_MODULE_DEVMNG)) {
        /* Only the process open davinci device is check. */
        vdevdrv_manager_process_sign_release(task->tgid);
    }
    return 0;
}

STATIC struct notifier_block g_process_sign_exit_nb = {
    .notifier_call = vdevdrv_manager_process_exit,
};

STATIC int vdevdrv_manager_register_notifier(void)
{
    int ret;

    ret = ka_dfx_profile_event_register(KA_PROFILE_TASK_EXIT, &g_process_sign_exit_nb);
    if (ret != 0) {
        devdrv_drv_err("Failed to register profile_event_register. (ret=%d).\n", ret);
        return -ENODEV;
    }
    return 0;
}

STATIC void vdevdrv_manager_unregister_notifier(void)
{
    (void)ka_dfx_profile_event_unregister(KA_PROFILE_TASK_EXIT, &g_process_sign_exit_nb);
}

STATIC int __init vdevdrv_manager_init(void)
{
    struct device *i_device = NULL;
    int ret = 0;
    int i;

    devdrv_drv_info("vdevdrv manager module init begin.\n");

    ret = drv_davinci_register_sub_module(DAVINCI_INTF_MODULE_DEVMNG, &vdevdrv_manager_file_operations);
    if (ret) {
        devdrv_drv_err("drv_davinci_register_sub_module failed! ret=%d\n", ret);
        return -EINVAL;
    }

    vdev_manager_info = dbl_kzalloc(sizeof(*vdev_manager_info), KA_GFP_KERNEL | __GFP_ACCOUNT);
    if (vdev_manager_info == NULL) {
        devdrv_drv_err("kzalloc return NULL, failed to alloc mem for manager struct.\n");
        ret = -ENOMEM;
        goto manager_info_alloc_fail;
    }

    KA_INIT_LIST_HEAD(&vdev_manager_info->hostpid_list_header);
    ka_task_mutex_init(&vdev_manager_info->devdrv_sign_list_lock);
    (void)memset_s(vdev_manager_info->devdrv_sign_count, MAX_DOCKER_NUM + 1, 0, MAX_DOCKER_NUM + 1);

    ret = vdevdrv_manager_register_notifier();
    if (ret != 0) {
        devdrv_drv_err("Failed to register dmanager notifier.\n");
        goto register_notifier_fail;
    }

    i_device = davinci_intf_get_owner_device();
    if (i_device == NULL) {
        devdrv_drv_err("device_create failed.\n");
        ret = -ENODEV;
        goto device_get_fail;
    }

    vdev_manager_info->prob_num = 0;
    vdev_manager_info->dev = i_device;
    vdev_manager_info->num_dev = 0;

    ka_task_spin_lock_init(&vdev_manager_info->spinlock);

    for (i = 0; i < VMNG_PDEV_MAX; i++) {
        vdevdrv_manager_set_devdrv_info(i, NULL);
    }
    vdev_manager_info->plat_info = DEVDRV_MANAGER_HOST_ENV;
    vdev_manager_info->dev_rdy_work = ka_task_create_singlethread_workqueue("mdev_ready_work");
    if (vdev_manager_info->dev_rdy_work == NULL) {
        devdrv_drv_err("create workqueue failed\n");
        ret = -ENODEV;
        goto devdrv_manager_workqueue_create_failed;
    }

    ret = vdevmng_register_client();
    if (ret) {
        devdrv_drv_err("vmnga client register failed, ret = %d", ret);
        goto vdevmng_register_client_failed;
    }

    ret = log_level_file_init();
    if (ret != 0) {
        devdrv_drv_err("log_level_file_init failed!!! ret(%d), default_log_level is ERROR.\n", ret);
    }

    devdrv_drv_info("vdevdrv manager module init succ.\n");
    return 0;

vdevmng_register_client_failed:
    ka_task_destroy_workqueue(vdev_manager_info->dev_rdy_work);
devdrv_manager_workqueue_create_failed:
device_get_fail:
    vdevdrv_manager_unregister_notifier();
register_notifier_fail:
    dbl_kfree(vdev_manager_info);
    vdev_manager_info = NULL;
manager_info_alloc_fail:
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_DEVMNG);

    return ret;
}
module_init(vdevdrv_manager_init);

STATIC void __exit vdevdrv_manager_exit(void)
{
    log_level_file_remove();
    vdevmng_unregister_client();
    ka_task_destroy_workqueue(vdev_manager_info->dev_rdy_work);
    vdevdrv_manager_unregister_notifier();

    dbl_kfree(vdev_manager_info);
    vdev_manager_info = NULL;
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_DEVMNG);
}
module_exit(vdevdrv_manager_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("DAVINCI driver");

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

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"

#include "devdrv_manager_common.h"
#include "devdrv_common.h"
#include "vdevmng_init.h"
#include "devdrv_manager.h"
#include "virtmng_interface.h"
#include "dms_event_distribute.h"
#include "hvdevmng_common.h"
#include "vdevmng_agent.h"
#include "devdrv_manager_pid_map.h"
#include "devdrv_manager_container.h"
#include "dbl_mem_alloc_interface.h"
#include "log_drv_agent.h"
#include "vdevmng_cmd_proc.h"

/* default log level */
#define CONSOLE_LOG_LEVEL 3

int copy_from_user_safe(void *to, const void __user *from, unsigned long n)
{
    if ((to == NULL) || (from == NULL) || (n == 0)) {
        devdrv_drv_err("user pointer is NULL.\n");
        return -EINVAL;
    }

    if (ka_base_copy_from_user(to, (void *)from, n)) {
        return -ENODEV;
    }
    return 0;
}
EXPORT_SYMBOL(copy_from_user_safe);

int copy_to_user_safe(void __user *to, const void *from, unsigned long n)
{
    if ((to == NULL) || (from == NULL) || (n == 0)) {
        devdrv_drv_err("user pointer is NULL.\n");
        return -EINVAL;
    }

    if (ka_base_copy_to_user(to, (void *)from, n)) {
        return -ENODEV;
    }
    return 0;
}
EXPORT_SYMBOL(copy_to_user_safe);

STATIC int vdevdrv_manager_container_get_bare_pid(struct devdrv_container_para *cmd)
{
    pid_t pid;
    int ret;

    if (cmd == NULL || cmd->para.out == NULL) {
        devdrv_drv_err("invalid cmd(%pK).\n", cmd);
        return -EINVAL;
    }

    pid = current->pid;
    ret = copy_to_user_safe(cmd->para.out, &pid, sizeof(pid_t));
    if (ret) {
        devdrv_drv_err("pid = %d, copy to user failed: %d.\n", pid, ret);
    }
    return ret;
}

STATIC int vdevdrv_manager_container_get_bare_tgid(struct devdrv_container_para *cmd)
{
    pid_t tgid;
    int ret;

    if (cmd == NULL || cmd->para.out == NULL) {
        devdrv_drv_err("invalid cmd(%pK).\n", cmd);
        return -EINVAL;
    }

    tgid = current->tgid;
    ret = copy_to_user_safe(cmd->para.out, &tgid, sizeof(pid_t));
    if (ret) {
        devdrv_drv_err("tgid = %d, copy to user failed: %d.\n", tgid, ret);
    }
    return ret;
}

STATIC int (*CONST vdevdrv_manager_container_process_handler[DEVDRV_CONTAINER_MAX_CMD])(
    struct devdrv_container_para *cmd) = {
        [DEVDRV_CONTAINER_GET_BARE_PID] = vdevdrv_manager_container_get_bare_pid,
        [DEVDRV_CONTAINER_GET_BARE_TGID] = vdevdrv_manager_container_get_bare_tgid,
    };

int vdevdrv_manager_container_cmd(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_container_para container_cmd = {{0}, 0};
    int ret;

    ret = copy_from_user_safe(&container_cmd, (void *)((uintptr_t)arg), sizeof(struct devdrv_container_para));
    if (ret) {
        devdrv_drv_err("copy_from_user return error: %d.\n", ret);
        return ret;
    }

    if (container_cmd.para.cmd >= DEVDRV_CONTAINER_MAX_CMD) {
        devdrv_drv_err("invalid input container process cmd: %u.\n", container_cmd.para.cmd);
        return -EINVAL;
    }

    if (vdevdrv_manager_container_process_handler[container_cmd.para.cmd] == NULL) {
        devdrv_drv_err("not supported cmd: %u.\n", container_cmd.para.cmd);
        return -EINVAL;
    }

    ret = vdevdrv_manager_container_process_handler[container_cmd.para.cmd](&container_cmd);

    return ret;
}

int vdrv_get_device_boot_status(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 phys_id;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&iomsg->cmd_data.boot_status, (void *)((uintptr_t)arg),
                              sizeof(struct devdrv_get_device_boot_status_para));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        goto free_iomsg;
    }

    phys_id = iomsg->cmd_data.boot_status.devId;

    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.boot_status);
    iomsg->output_len = sizeof(iomsg->cmd_data.boot_status);

    ret = vdevmng_vpc_msg_send(phys_id, iomsg);
    if (ret) {
        devdrv_drv_err("vdevmng vpc msg failed, ret(%d), dev_id(%u).\n", ret, phys_id);
        goto free_iomsg;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &iomsg->cmd_data.boot_status,
                            sizeof(struct devdrv_get_device_boot_status_para));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d), dev_id(%u).\n", ret, phys_id);
        goto free_iomsg;
    }

free_iomsg:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_manager_get_host_phy_mach_flag(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_get_host_phy_mach_flag_para para = {0};
    u32 phys_id = 0;
    u32 vfid = 0;
    int ret;

    ret = copy_from_user_safe(&para, (void *)((uintptr_t)arg), sizeof(struct devdrv_get_host_phy_mach_flag_para));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return ret;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(para.devId, &phys_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Can't transform virt id to phy id. (vir_id=%u; ret=%d)\n", para.devId, ret);
        return -EFAULT;
    }

    para.host_flag = DEVDRV_HOST_VM_MACH_FLAG;

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &para, sizeof(struct devdrv_get_host_phy_mach_flag_para));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d), dev_id(%u).\n", ret, para.devId);
    }
    return ret;
}

int vdevdrv_manager_get_probe_num(struct file *filep, unsigned int cmd, unsigned long arg)
{
    u32 devnum;
    int ret;

    devnum = vdevdrv_manager_get_devnum();
    if (devnum > VMNG_PDEV_MAX) {
        devdrv_drv_err("invalid dev_num(%u).\n", devnum);
        return -EINVAL;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &devnum, sizeof(u32));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d) dev_num(%u).\n", ret, devnum);
    }
    return ret;
}

int vdevdrv_manager_get_container_flag(struct file *filep, unsigned int cmd, unsigned long arg)
{
    unsigned int flag;
    int ret;

    flag = false;

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &flag, sizeof(unsigned int));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d).\n", ret);
    }
    return ret;
}

STATIC int vdevdrv_creat_random_sign(char *random_sign, u32 len)
{
    char random[RANDOM_SIZE] = {0};
    int offset = 0;
    int ret;
    int i;

    for (i = 0; i < RANDOM_SIZE; i++) {
        ret = snprintf_s(random_sign + offset, len - offset, len - 1 - offset, "%02x", (u8)random[i]);
        if (ret < 0) {
            devdrv_drv_err("snprintf failed, ret(%d).\n", ret);
            return -EINVAL;
        }
        offset += ret;
    }
    random_sign[len - 1] = '\0';

    return 0;
}

STATIC int vdevdrv_get_process_sign(struct devdrv_manager_info *d_info,
    char *sign, u32 len, u32 docker_id)
{
    struct devdrv_process_sign *d_sign = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    int ret;

    if (!ka_list_empty_careful(&d_info->hostpid_list_header)) {
        ka_list_for_each_safe(pos, n, &d_info->hostpid_list_header) {
            d_sign = ka_list_entry(pos, struct devdrv_process_sign, list);
            if (d_sign->hostpid == current->tgid) {
                ret = strcpy_s(sign, len, d_sign->sign);
                if (ret) {
                    devdrv_drv_err("strcpy_s failed, ret(%d).\n", ret);
                    return -EINVAL;
                }
                return 0;
            }
        }
    }

    if (d_info->devdrv_sign_count[docker_id] >= DEVDRV_MAX_SIGN_NUM) {
        devdrv_drv_err("Master process is full. (max_number=%d; docker_id=%u).\n", DEVDRV_MAX_SIGN_NUM, docker_id);
        return -EINVAL;
    }

    d_sign = dbl_kzalloc(sizeof(struct devdrv_process_sign), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (d_sign == NULL) {
        devdrv_drv_err("kzalloc failed, docker_id(%u).\n", docker_id);
        return -ENOMEM;
    }
    d_sign->hostpid = current->tgid;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
    d_sign->hostpid_start_time = current->group_leader->start_time;
#else
    d_sign->hostpid_start_time = current->group_leader->start_time.tv_sec * KA_NSEC_PER_SEC +
                                current->group_leader->start_time.tv_nsec;
#endif
    d_sign->docker_id = docker_id;

    ret = vdevdrv_creat_random_sign(d_sign->sign, PROCESS_SIGN_LENGTH);
    if (ret) {
        devdrv_drv_err("get sign failed, ret(%d), docker_id(%u).\n", ret, docker_id);
        dbl_kfree(d_sign);
        d_sign = NULL;
        return ret;
    }

    ret = strcpy_s(sign, len, d_sign->sign);
    if (ret) {
        devdrv_drv_err("strcpy_s failed, ret(%d), docker_id(%u).\n", ret, docker_id);
        dbl_kfree(d_sign);
        d_sign = NULL;
        return -EINVAL;
    }
    ka_list_add(&d_sign->list, &d_info->hostpid_list_header);
    d_info->devdrv_sign_count[d_sign->docker_id]++;

    return 0;
}

int vdevdrv_manager_get_process_sign(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_manager_info *d_info = vdevdrv_get_manager_info();
    u32 docker_id = MAX_DOCKER_NUM;
    struct process_sign dev_sign = {0};
    int ret;

    if (d_info == NULL) {
        devdrv_drv_err("d_info is null.\n");
        return -EINVAL;
    }

    ka_task_mutex_lock(&d_info->devdrv_sign_list_lock);
    ret = vdevdrv_get_process_sign(d_info, dev_sign.sign, PROCESS_SIGN_LENGTH, docker_id);
    if (ret) {
        ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);
        devdrv_drv_err("get process_sign failed, ret(%d).\n", ret);
        return ret;
    }
    ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);

    dev_sign.tgid = current->tgid;

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &dev_sign, sizeof(struct process_sign));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d).\n", ret);
        return ret;
    }
    (void)memset_s(dev_sign.sign, PROCESS_SIGN_LENGTH, 0, PROCESS_SIGN_LENGTH);

    return 0;
}

int vdevdrv_get_core_inuse(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_hardware_inuse inuse = {0};
    struct devdrv_info *vdev_info = NULL;
    int ret;

    ret = copy_from_user_safe(&inuse, (void *)((uintptr_t)arg), sizeof(struct devdrv_hardware_inuse));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe failed, ret(%d).\n", ret);
        return ret;
    }

    vdev_info = vdevdrv_manager_get_devdrv_info(inuse.devid);
    if (vdev_info == NULL) {
        devdrv_drv_err("vdev_info(%u) is NULL.\n", inuse.devid);
        return -EINVAL;
    }

    if (vdev_info->dev_ready != DEVDRV_DEV_READY_WORK) {
        devdrv_drv_err("vdev_info(%u) is not ready.\n", inuse.devid);
        return -EINVAL;
    }

    inuse.ai_core_num = vdev_info->inuse.ai_core_num;
    inuse.ai_core_error_bitmap = vdev_info->inuse.ai_core_error_bitmap;
    inuse.ai_cpu_num = vdev_info->inuse.ai_cpu_num;
    inuse.ai_cpu_error_bitmap = vdev_info->inuse.ai_cpu_error_bitmap;

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &inuse, sizeof(struct devdrv_hardware_inuse));
    if (ret) {
        devdrv_drv_err("copy_to_user_safe failed, ret(%d) dev_id(%u).\n", ret, inuse.devid);
    }
    return ret;
}

int vdevdrv_get_core_spec(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_hardware_spec spec = {0};
    struct devdrv_info *vdev_info = NULL;
    int ret;

    ret = copy_from_user_safe(&spec, (void *)((uintptr_t)arg), sizeof(struct devdrv_hardware_spec));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe failed, ret(%d).\n", ret);
        return ret;
    }

    vdev_info = vdevdrv_manager_get_devdrv_info(spec.devid);
    if (vdev_info == NULL) {
        devdrv_drv_err("vdev_info(%u) is NULL.\n", spec.devid);
        return -EINVAL;
    }

    if (vdev_info->dev_ready != DEVDRV_DEV_READY_WORK) {
        devdrv_drv_err("vdev_info(%u) is not ready.\n", spec.devid);
        return -EINVAL;
    }

    spec.ai_core_num = vdev_info->ai_core_num;
    spec.first_ai_core_id = vdev_info->ai_core_id;
    spec.ai_cpu_num = vdev_info->ai_cpu_core_num;
    spec.first_ai_cpu_id = vdev_info->ai_cpu_core_id;
    spec.ai_core_num_level = vdev_info->pdata->ai_core_num_level;
    spec.ai_core_freq_level = vdev_info->pdata->ai_core_freq_level;

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &spec, sizeof(struct devdrv_hardware_spec));
    if (ret) {
        devdrv_drv_err("copy_to_user_safe failed, ret(%d) dev_id(%u).\n", ret, spec.devid);
    }
    return ret;
}

int vdevdrv_manager_get_tsdrv_dev_com_info(struct file *filep,
    unsigned int cmd, unsigned long arg)
{
    struct tsdrv_dev_com_info dev_com_info = {0};
    int ret;

    dev_com_info.mach_type = VIR_MACHINE_TYPE;
    dev_com_info.ts_num = 1;

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &dev_com_info, sizeof(struct tsdrv_dev_com_info));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d).\n", ret);
    }
    return ret;
}

int vdevmng_get_devinfo_from_vdev_info(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_manager_hccl_devinfo hccl_devinfo = {0};
    struct devdrv_platform_data *pdata = NULL;
    struct devdrv_info *vdev_info = NULL;
    u32 dev_id;

    if (copy_from_user_safe(&hccl_devinfo, (void *)(uintptr_t)arg, sizeof(hccl_devinfo))) {
        devdrv_drv_err("copy from user failed.\n");
        return -EINVAL;
    }

    dev_id = hccl_devinfo.dev_id;
    vdev_info = vdevdrv_manager_get_devdrv_info(dev_id);
    if (vdev_info == NULL) {
        devdrv_drv_err("device(%u) is not initialized\n", dev_id);
        return -ENODEV;
    }

    if (vdev_info->dev_ready != DEVDRV_DEV_READY_WORK) {
        devdrv_drv_err("vdev_info(%u) is not ready.\n", dev_id);
        return -EINVAL;
    }

    pdata = vdev_info->pdata;
    hccl_devinfo.ai_core_num = vdev_info->ai_core_num;
    hccl_devinfo.aicore_freq = vdev_info->aicore_freq;
    hccl_devinfo.ai_cpu_core_num = vdev_info->ai_cpu_core_num;
    hccl_devinfo.ctrl_cpu_core_num = vdev_info->ctrl_cpu_core_num;
    hccl_devinfo.ctrl_cpu_occupy_bitmap = vdev_info->ctrl_cpu_occupy_bitmap;

    /* 1:little endian 0:big endian */
    hccl_devinfo.ctrl_cpu_endian_little = vdev_info->ctrl_cpu_endian_little;
    hccl_devinfo.ctrl_cpu_id = vdev_info->ctrl_cpu_id;
    hccl_devinfo.ctrl_cpu_ip = vdev_info->ctrl_cpu_ip;
    hccl_devinfo.ts_cpu_core_num = pdata->ts_pdata[0].ts_cpu_core_num;
    hccl_devinfo.env_type = vdev_info->env_type;
    hccl_devinfo.ai_core_id = vdev_info->ai_core_id;
    hccl_devinfo.ai_cpu_core_id = vdev_info->ai_cpu_core_id;
    hccl_devinfo.ai_cpu_bitmap = vdev_info->aicpu_occupy_bitmap;
    hccl_devinfo.hardware_version = vdev_info->hardware_version;
    hccl_devinfo.env_type = vdev_info->env_type;
    hccl_devinfo.ts_num = vdev_info->pdata->ts_num;
#ifdef CFG_SOC_PLATFORM_MINIV2
    hccl_devinfo.vector_core_num = vdev_info->vector_core_num;
    hccl_devinfo.vector_core_freq = vdev_info->vector_core_freq;
#endif
    devdrv_drv_debug("ctrl_cpu_ip(0x%x), ts_cpu_core_num(%d), dev_id(%u)\n", vdev_info->ctrl_cpu_ip,
                     hccl_devinfo.ts_cpu_core_num, dev_id);

    if (copy_to_user_safe((void *)(uintptr_t)arg, &hccl_devinfo, sizeof(hccl_devinfo))) {
        devdrv_drv_err("copy to user failed, dev_id(%u).\n", dev_id);
        return -EFAULT;
    }

    return 0;
}

int vdevmng_get_devinfo_from_phy_machine(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_manager_hccl_devinfo hccl_devinfo = {0};
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 dev_id;
    int ret;
    int i;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&hccl_devinfo, (void *)(uintptr_t)arg, sizeof(hccl_devinfo));
    if (ret) {
        devdrv_drv_err("copy from user failed.\n");
        goto free_iomsg;
    }

    dev_id = hccl_devinfo.dev_id;

    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.H2D_devinfo);
    iomsg->output_len = sizeof(iomsg->cmd_data.H2D_devinfo);

    ret = vdevmng_vpc_msg_send(dev_id, iomsg);
    if (ret) {
        devdrv_drv_err("vdevmng vpc msg failed, ret(%d). dev_id(%u)\n", ret, dev_id);
        goto free_iomsg;
    }

    hccl_devinfo.cpu_system_count = iomsg->cmd_data.H2D_devinfo.cpu_system_count;
    hccl_devinfo.monotonic_raw_time_ns = iomsg->cmd_data.H2D_devinfo.monotonic_raw_time_ns;
    for (i = 0; i < DEVDRV_MAX_COMPUTING_POWER_TYPE; i++) {
        hccl_devinfo.computing_power[i] = iomsg->cmd_data.H2D_devinfo.computing_power[i];
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &hccl_devinfo, sizeof(hccl_devinfo));
    if (ret) {
        devdrv_drv_err("copy to user failed. dev_id(%u)\n", dev_id);
        goto free_iomsg;
    }

free_iomsg:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_manager_get_pci_info(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vmnga_pci_dev_info vmnga_pci_info = { 0 };
    struct devdrv_pci_info dev_pci_info = { 0 };
    u32 dev_id;

    if (copy_from_user_safe(&dev_pci_info, (void *)(uintptr_t)arg, sizeof(struct devdrv_pci_info))) {
        devdrv_drv_err("copy from user failed.\n");
        return -EFAULT;
    }

    dev_id = dev_pci_info.dev_id;
    if (dev_id >= VMNG_PDEV_MAX) {
        devdrv_drv_err("invalid dev_id(%u)\n", dev_id);
        return -ENODEV;
    }

    if (vmnga_get_pci_dev_info(dev_id, &vmnga_pci_info)) {
        devdrv_drv_err("devdrv_get_pci_dev_info failed. dev_id(%u)\n", dev_id);
        return -EBUSY;
    }

    dev_pci_info.bus_number = vmnga_pci_info.bus_no;
    dev_pci_info.dev_number = vmnga_pci_info.device_no;
    dev_pci_info.function_number = vmnga_pci_info.function_no;

    if (copy_to_user_safe((void *)(uintptr_t)arg, &dev_pci_info, sizeof(struct devdrv_pci_info))) {
        devdrv_drv_err("copy to user failed. dev_id(%u)\n", dev_id);
        return -EFAULT;
    }

    return 0;
}

int vdevdrv_get_pcie_id(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vmnga_pcie_id_info vmnga_dev_info = {0};
    struct dmanage_pcie_id_info pcie_id_info = {0};
    unsigned int dev_id;
    int ret;
    ret = copy_from_user_safe(&pcie_id_info, (void *)((uintptr_t)arg), sizeof(struct dmanage_pcie_id_info));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe failed.\n");
        return ret;
    }

    dev_id = pcie_id_info.davinci_id;
    if (dev_id >= VMNG_PDEV_MAX) {
        devdrv_drv_err("invalid device id, id = %d.\n", dev_id);
        return -EINVAL;
    }

    ret = vmnga_get_pcie_id_info(dev_id, &vmnga_dev_info);
    if (ret) {
        devdrv_drv_err("devdrv_get_pcie_id failed.\n");
        return ret;
    }

    pcie_id_info.venderid = vmnga_dev_info.venderid;
    pcie_id_info.subvenderid = vmnga_dev_info.subvenderid;
    pcie_id_info.deviceid = vmnga_dev_info.deviceid;
    pcie_id_info.subdeviceid = vmnga_dev_info.subdeviceid;
    pcie_id_info.bus = vmnga_dev_info.bus;
    pcie_id_info.device = vmnga_dev_info.device;
    pcie_id_info.fn = vmnga_dev_info.fn;

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &pcie_id_info, sizeof(struct dmanage_pcie_id_info));
    if (ret) {
        devdrv_drv_err("copy_to_user_safe failed, dev(%u).\n", dev_id);
    }
    return ret;
}

int vdevdrv_manager_get_device_status(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 dev_id;
    u32 status;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&dev_id, (void *)(uintptr_t)arg, sizeof(u32));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        goto free_iomsg;
    }

    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.u32_para);
    iomsg->output_len = sizeof(iomsg->cmd_data.u32_para);

    ret = vdevmng_vpc_msg_send(dev_id, iomsg);
    if (ret) {
        devdrv_drv_err("vdevmng vpc msg failed, ret(%d), dev_id(%u).\n", ret, dev_id);
        goto free_iomsg;
    }

    status = iomsg->cmd_data.u32_para.para_out;
    ret = copy_to_user_safe((void *)(uintptr_t)arg, &status, sizeof(u32));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d) dev_id(%u).\n", ret, dev_id);
        goto free_iomsg;
    }

free_iomsg:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_manager_ioctl_get_plat_info(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_manager_info *vmanager_info = NULL;
    u32 plat_info;

    vmanager_info = vdevdrv_get_manager_info();
    if (vmanager_info == NULL) {
        devdrv_drv_err("vdev_manager_info is NULL.\n");
        return -EINVAL;
    }

    plat_info = vmanager_info->plat_info;

    if (copy_to_user_safe((void *)((uintptr_t)arg), &plat_info, sizeof(u32))) {
        devdrv_drv_err("copy to user failed.\n");
        return -EINVAL;
    }
    return 0;
}

int vdevdrv_manager_get_console_loglevel(struct file *filep, unsigned int cmd, unsigned long arg)
{
    int console_loglevle_value;

    console_loglevle_value = log_level_get();
    if (copy_to_user_safe((void *)((uintptr_t)arg), &console_loglevle_value, sizeof(int))) {
        devdrv_drv_err("copy to user failed.\n");
        return -EINVAL;
    }

    return 0;
}

int vdevdrv_manager_get_device_startup_status(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 phys_id;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&iomsg->cmd_data.work_status, (void *)(uintptr_t)arg,
                              sizeof(struct devdrv_device_work_status));
    if (ret != 0) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        goto free_iomsg;
    }

    phys_id = iomsg->cmd_data.work_status.device_id;

    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.work_status);
    iomsg->output_len = sizeof(iomsg->cmd_data.work_status);

    ret = vdevmng_vpc_msg_send(phys_id, iomsg);
    if (ret) {
        devdrv_drv_err("vdevmng vpc msg failed, ret(%d), dev_id(%u).\n", ret, phys_id);
        goto free_iomsg;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &iomsg->cmd_data.work_status,
                            sizeof(struct devdrv_device_work_status));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d)\n", ret);
        goto free_iomsg;
    }

free_iomsg:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_manager_get_device_health_status(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 phys_id;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&iomsg->cmd_data.health_status, (void *)(uintptr_t)arg,
                              sizeof(struct devdrv_device_health_status));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        goto free_iomsg;
    }

    phys_id = iomsg->cmd_data.health_status.device_id;

    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.health_status);
    iomsg->output_len = sizeof(iomsg->cmd_data.health_status);

    ret = vdevmng_vpc_msg_send(phys_id, iomsg);
    if (ret) {
        devdrv_drv_err("vdevmng vpc msg failed, ret(%d), dev_id(%u).\n", ret, phys_id);
        goto free_iomsg;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &iomsg->cmd_data.health_status,
                            sizeof(struct devdrv_device_health_status));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d)\n", ret);
        goto free_iomsg;
    }

free_iomsg:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

#define MINIV2_CHIP_NAME 6481
STATIC int resource_type_check(struct devdrv_resource_info *dinfo)
{
    struct devdrv_info *vdev_info = NULL;

    if ((dinfo->owner_type == DEVDRV_VDEV_RESOURCE)) {
        return -EOPNOTSUPP;
    }

    vdev_info = vdevdrv_manager_get_devdrv_info(dinfo->devid);
    if (vdev_info == NULL) {
        devdrv_drv_err("The device is not initialized. (devid=%u)\n", dinfo->devid);
        return -ENODEV;
    }

    if (vdev_info->dev_ready != DEVDRV_DEV_READY_WORK) {
        devdrv_drv_err("The device is not ready. (devid=%u)\n", dinfo->devid);
        return -EINVAL;
    }

    if (vdev_info->chip_name == MINIV2_CHIP_NAME &&
        (dinfo->resource_type == DEVDRV_DEV_HBM_TOTAL || dinfo->resource_type == DEVDRV_DEV_HBM_FREE)) {
        return -EOPNOTSUPP;
    }

    return 0;
}

STATIC int vdevdrv_manager_get_hostpid(pid_t *process_id)
{
    struct pid *pgrp = NULL;

    pgrp = find_get_pid(*process_id);
    if (pgrp == NULL) {
        devdrv_drv_err("The pgrp parameter is NULL.\n");
        return -EINVAL;
    }

    *process_id = pgrp->numbers[0].nr; /* 0:hostpid */
    ka_task_put_pid(pgrp);
    return 0;
}

int vdevdrv_manager_get_dev_resource_info(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_resource_info *dinfo = NULL;
    struct vdevmng_ioctl_msg *iomsg = NULL;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg failed.\n");
        return -ENOMEM;
    }

    dinfo = &iomsg->cmd_data.resource_info;
    ret = copy_from_user_safe(dinfo, (void *)(uintptr_t)arg, sizeof(struct devdrv_resource_info));
    if (ret) {
        devdrv_drv_err("Copy from user failed. (ret=%d)\n", ret);
        goto free_iomsg;
    }

    ret = resource_type_check(dinfo);
    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "Failed to invoke resource_type_check. (ret=%d)\n", ret);
        goto free_iomsg;
    }
    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.resource_info);
    iomsg->output_len = sizeof(iomsg->cmd_data.resource_info);

    if ((dinfo->owner_type == DEVDRV_DEV_RESOURCE) &&
        ((dinfo->resource_type != DEVDRV_DEV_PROCESS_PID) && (dinfo->resource_type != DEVDRV_DEV_PROCESS_MEM))) {
        ret = vdevmng_vpc_msg_send(dinfo->devid, iomsg);
    } else if (dinfo->owner_type == DEVDRV_PROCESS_RESOURCE && dinfo->resource_type == DEVDRV_DEV_PROCESS_PID) {
        ret = vdevdrv_manager_get_accounting_pid(dinfo);
    } else if (dinfo->owner_type == DEVDRV_PROCESS_RESOURCE && dinfo->resource_type == DEVDRV_DEV_PROCESS_MEM) {
        ret = vdevdrv_manager_get_hostpid(&dinfo->owner_id);
        if (ret != 0) {
            devdrv_drv_err("Get hostpid failed. (ret=%d, dev_id=%u, owner_type=%d.)\n",
                ret, dinfo->devid, dinfo->owner_type);
            goto free_iomsg;
        }
        ret = vdevmng_vpc_msg_send(dinfo->devid, iomsg);
    } else {
        ret = -EOPNOTSUPP;
    }

    if (ret != 0) {
        devdrv_drv_err("vdevmng vpc msg failed, ret(%d). dev_id(%u) owner_type(%d).\n",
            ret, dinfo->devid, dinfo->owner_type);
        goto free_iomsg;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, dinfo, sizeof(struct devdrv_resource_info));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d)\n", ret);
        goto free_iomsg;
    }

free_iomsg:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_get_error_code(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 phy_id;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&iomsg->cmd_data.error_code_para,
                              (void *)((uintptr_t)arg), sizeof(struct devdrv_error_code_para));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe failed.\n");
        goto free_iomsg;
    }

    phy_id = iomsg->cmd_data.error_code_para.dev_id;

    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.error_code_para);
    iomsg->output_len = sizeof(iomsg->cmd_data.error_code_para);

    ret = vdevmng_vpc_msg_send(phy_id, iomsg);
    if (ret) {
        devdrv_drv_err("vdevmng vpc msg failed, ret(%d), dev_id(%u).\n", ret, phy_id);
        goto free_iomsg;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg),
                            &iomsg->cmd_data.error_code_para, sizeof(struct devdrv_error_code_para));
    if (ret != 0) {
        devdrv_drv_err("copy_to_user_safe failed.\n");
        goto free_iomsg;
    }

free_iomsg:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_manager_get_osc_freq(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 phy_id;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&iomsg->cmd_data.osc_freq, (void *)((uintptr_t)arg), sizeof(struct vdevmng_osc_freq));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user_safe failed. (ret=%d)\n", ret);
        goto FREE_IOMSG;
    }

    phy_id = iomsg->cmd_data.osc_freq.devid;
    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.osc_freq);
    iomsg->output_len = sizeof(iomsg->cmd_data.osc_freq);

    ret = vdevmng_vpc_msg_send(phy_id, iomsg);
    if (ret != 0) {
        devdrv_drv_err("Failed to send the VPC message. (dev_id=%u; ret=%d)\n", phy_id, ret);
        goto FREE_IOMSG;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &iomsg->cmd_data.osc_freq, sizeof(struct vdevmng_osc_freq));
    if (ret != 0) {
        devdrv_drv_err("copy_to_user_safe failed. (dev_id=%u; ret=%d)\n", phy_id, ret);
        goto FREE_IOMSG;
    }

FREE_IOMSG:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_manager_get_current_aic_freq(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    u32 phy_id;
    int ret;

    iomsg = (struct vdevmng_ioctl_msg *)dbl_kzalloc(sizeof(struct vdevmng_ioctl_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (iomsg == NULL) {
        devdrv_drv_err("kzalloc iomsg fail.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(&iomsg->cmd_data.current_aic_freq, (void *)((uintptr_t)arg),
        sizeof(struct vdevmng_current_aic_freq));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user_safe failed. (ret=%d)\n", ret);
        goto FREE_IOMSG;
    }

    phy_id = iomsg->cmd_data.current_aic_freq.devid;
    iomsg->main_cmd = DMS_MAIN_CMD_BASIC;
    iomsg->sub_cmd = _IOC_NR(cmd);
    iomsg->filter_len = 0;
    iomsg->input_len = sizeof(iomsg->cmd_data.current_aic_freq);
    iomsg->output_len = sizeof(iomsg->cmd_data.current_aic_freq);

    ret = vdevmng_vpc_msg_send(phy_id, iomsg);
    if (ret != 0) {
        devdrv_drv_err("Failed to send the VPC message. (dev_id=%u; ret=%d)\n", phy_id, ret);
        goto FREE_IOMSG;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &iomsg->cmd_data.current_aic_freq,
        sizeof(struct vdevmng_current_aic_freq));
    if (ret != 0) {
        devdrv_drv_err("copy_to_user_safe failed. (dev_id=%u; ret=%d)\n", phy_id, ret);
        goto FREE_IOMSG;
    }

FREE_IOMSG:
    dbl_kfree(iomsg);
    iomsg = NULL;
    return ret;
}

int vdevdrv_get_host_kern_log(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct vdevmng_log_info log_info = { 0 };
    int ret;

    if ((u32)(current->cred->euid.val) != 0) {
        devdrv_drv_err("Only the root user can invoke the function.\n");
        return -EPERM;
    }

    ret = copy_from_user_safe(&log_info, (void *)((uintptr_t)arg), sizeof(struct vdevmng_log_info));
    if (ret != 0) {
        devdrv_drv_err("Copy_from_user_safe failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = log_get_ringbuffer(log_info.buf, log_info.length, &log_info.buf_len);
    if (ret != 0) {
        devdrv_drv_err("Log_get_ringbuffer failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &log_info, sizeof(struct vdevmng_log_info));
    if (ret != 0) {
        devdrv_drv_err("Copy_to_user_safe failed. (ret=%d)\n", ret);
        return ret;
    }

    return ret;
}

int vdevdrv_manager_not_support(struct file *filep, unsigned int cmd, unsigned long arg)
{
    return -EOPNOTSUPP;
}

int vdevdrv_manager_not_permit(struct file *filep, unsigned int cmd, unsigned long arg)
{
    return -EPERM;
}

typedef struct vdev_chip_core_info {
    unsigned int chip_version;
    unsigned int chip_name;
    unsigned int aicore_num;
    unsigned char template_name[TEMPLATE_NAME_LEN];
} vdev_chip_core_info_t;

typedef struct chip_value_name_map {
    unsigned int chip_value;
    char *chip_name;
} chip_value_name_map_t;

/* chip value 6416/6528/6481 means miniv1/cloud/miniv2 */
STATIC chip_value_name_map_t g_chip_value_name_map[] = {
    { 6416, "310" },
    { 6528, "910" },
    { 6481, "310P" },
    { 6417, "310B" },
    { 6529, "910B" },
};

STATIC int vdevdrv_manager_get_chip_core_info(unsigned int dev_id, vdev_chip_core_info_t *chip_core_info)
{
    int ret;
    struct devdrv_info *vdev_info = NULL;

    vdev_info = vdevdrv_manager_get_devdrv_info(dev_id);
    if (vdev_info == NULL) {
        devdrv_drv_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (vdev_info->dev_ready != DEVDRV_DEV_READY_WORK) {
        devdrv_drv_err("Vdevice info is not ready. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    chip_core_info->chip_version = vdev_info->chip_version;
    chip_core_info->chip_name = vdev_info->chip_name;
    chip_core_info->aicore_num = vdev_info->ai_core_num;
    ret = memcpy_s(chip_core_info->template_name, TEMPLATE_NAME_LEN,
        vdev_info->template_name, TEMPLATE_NAME_LEN);
    if (ret != 0) {
        devdrv_drv_err("Copy template name failed.(dev_id=%u; ret=%d)\n", dev_id, ret);
        return -ENOMEM;
    }
    return 0;
}

STATIC int vdevdrv_manager_set_chip_name_and_ver(devdrv_query_chip_info_t *chip_info,
    vdev_chip_core_info_t *chip_core_info)
{
    int ret, i;

    for (i = 0; i < sizeof(g_chip_value_name_map) / sizeof(chip_value_name_map_t); ++i) {
        if (chip_core_info->chip_name != g_chip_value_name_map[i].chip_value) {
            continue;
        }
#ifndef CFG_FEATURE_VFG
        ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%svir%02u",
            g_chip_value_name_map[i].chip_name, chip_core_info->aicore_num);
        if (ret < 0) {
            devdrv_drv_err("Translate chip name failed. (dev_id=%d; ret=%d)\n", chip_info->dev_id, ret);
            return ret;
        }
#else
        ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%s",
            g_chip_value_name_map[i].chip_name);
        if (ret < 0) {
            devdrv_drv_err("Translate chip name failed. (dev_id=%d; ret=%d)\n", chip_info->dev_id, ret);
            return ret;
        }

        chip_core_info->template_name[TEMPLATE_NAME_LEN - 1] = '\0';
        ret = strcat_s((char *)chip_info->info.name, MAX_CHIP_NAME, chip_core_info->template_name);
        if (ret != 0) {
            devdrv_drv_err("Set template name failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
            return ret;
        }
#endif
        break;
    }

#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2)
    ret = sprintf_s((char *)chip_info->info.version, MAX_CHIP_NAME, "V%01x", chip_core_info->chip_version);
#else
    ret = sprintf_s((char *)chip_info->info.version, MAX_CHIP_NAME, "V%03x", chip_core_info->chip_version);
#endif
    if (ret < 0) {
        devdrv_drv_err("Set chip version failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
        return ret;
    }

    return 0;
}

int vdevdrv_manager_ioctl_get_chip_info(struct file *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    devdrv_query_chip_info_t chip_info = {0};
    vdev_chip_core_info_t chip_core_info = {0};
    unsigned int phy_dev_id = 0;
    unsigned int vfid = 0;

    ret = copy_from_user_safe(&chip_info, (void *)((uintptr_t)arg), sizeof(devdrv_query_chip_info_t));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user_safe failed.\n");
        return ret;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(chip_info.dev_id, &phy_dev_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Device id check failed. (dev_id=%d; ret=%d)\n", chip_info.dev_id, ret);
        return ret;
    }

    ret = vdevdrv_manager_get_chip_core_info(phy_dev_id, &chip_core_info);
    if (ret != 0) {
        devdrv_drv_err("Get chip core info failed. (dev_id=%d; ret=%d)\n", chip_info.dev_id, ret);
        return ret;
    }

    ret = strcpy_s((char *)chip_info.info.type, MAX_CHIP_NAME, "Ascend");
    if (ret != 0) {
        devdrv_drv_err("Copy chip type failed. (dev_id=%u; ret=%d)\n", chip_info.dev_id, ret);
        return ret;
    }

    ret = sprintf_s((char *)chip_info.info.name, MAX_CHIP_NAME, "%s", "unknown");
    if (ret < 0) {
        devdrv_drv_err("Set initial chip name failed. (dev_id=%u; ret=%d)\n", chip_info.dev_id, ret);
        return ret;
    }

    ret = vdevdrv_manager_set_chip_name_and_ver(&chip_info, &chip_core_info);
    if (ret != 0) {
        devdrv_drv_err("Set chip name failed. (dev_id=%u; ret=%d)\n", chip_info.dev_id, ret);
        return ret;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &chip_info, sizeof(devdrv_query_chip_info_t));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed. (dev_id=%u; ret=%d)\n", chip_info.dev_id, ret);
        return ret;
    }

    return 0;
}


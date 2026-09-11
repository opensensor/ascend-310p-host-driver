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

#include <linux/init.h>

#include "svm_cmd.h"
#include "devmm_proc_info.h"
#include "devmm_proc_mem_copy.h"
#include "devmm_channel.h"
#include "devdrv_interface.h"
#include "devmm_common.h"
#include "devmm_dev.h"
#include "svm_proc_mng.h"
#include "svm_mem_mng.h"
#include "svm_heap_mng.h"
#include "svm_register_ops.h"
#include "svm_srcu_work.h"
#include "dbl/feature_loader.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_dynamic_addr.h"

#ifdef CFG_FEATURE_VFIO
#include "devmm_pm_vpc.h"
#include "devmm_pm_adapt.h"
#endif

struct devmm_svm_dev *devmm_svm = NULL;
static char *svm_vma_magic = "SVM";

STATIC int devmm_svm_open(ka_inode_t *inode, ka_file_t *file)
{
    struct devmm_private_data *priv = NULL;

    priv = devmm_kzalloc_ex(sizeof(struct devmm_private_data), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (priv == NULL) {
        devmm_drv_err("Kzalloc devmm_private_data fail.\n");
        return -ENOMEM;
    }

    ka_base_atomic_set(&priv->next_seg_id, 0);
    ka_fs_set_file_private_data(file, priv);
    return 0;
}

STATIC int _devmm_svm_mmap_config_svm_proc(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma)
{
    if ((svm_proc->vma_num >= devmm_svm->mmap_para.seg_num) ||
        (svm_proc->inited != DEVMM_SVM_PRE_INITING_FLAG)) {
        devmm_drv_err("Svm map get_svm_process error. "
            "(vm_start=0x%lx; vm_end=0x%lx; vm_pgoff=0x%lx; vm_flags=0x%lx)\n",
            ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma), vma->vm_pgoff, ka_mm_get_vm_flags(vma));
        return -ESRCH;
    }
    devmm_remove_vma_wirte_flag(vma);
    /* init vma */
    /* svm_proc->vma[] cannot be set here, because va maybe unmap */
#ifdef EMU_ST
    svm_proc->vma[svm_proc->vma_num] = vma;
#endif
    svm_proc->vma_num++;
    if (svm_proc->vma_num == devmm_svm->mmap_para.seg_num) {
        svm_proc->mm = ka_task_get_current_mm();
        svm_proc->tsk = ka_task_get_current();
        devmm_set_svm_proc_state(svm_proc, DEVMM_SVM_INITING_FLAG);
        devmm_drv_debug("Devmm_map success. (hostpid=%d; devid=%d; vfid=%d; devpid=%d; status=%u; proc_idx=%u)\n",
            svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid,
            svm_proc->devpid, svm_proc->notifier_reg_flag, svm_proc->proc_idx);
    }

    return 0;
}

STATIC int devmm_svm_mmap_config_svm_proc(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma)
{
    int ret;

    ka_task_mutex_lock(&svm_proc->proc_lock);
    ret = _devmm_svm_mmap_config_svm_proc(svm_proc, vma);
    ka_task_mutex_unlock(&svm_proc->proc_lock);

    return ret;
}

static int devmm_file_priv_check(struct devmm_private_data *priv)
{
    if (priv == NULL) {
        devmm_drv_err("Unexpected, file->private_data is NULL.\n");
        return -EINVAL;
    }

    if ((priv->custom_flag == 0) && (priv->process == NULL)) {
        devmm_drv_err("File's svm_proc is null, please call alloc_svm_proc first.\n");
        return -EINVAL;
    }

    return 0;
}

bool devmm_is_svm_vma_magic(void *check_magic)
{
    return (check_magic == (void *)svm_vma_magic);
}

static int devmm_mmap_vma_check(u32 seg_id, ka_vm_area_struct_t *vma)
{
    u64 mmap_va, mmap_size;

    if (seg_id >= devmm_svm->mmap_para.seg_num) {
        return 0;
    }

    mmap_va = devmm_svm->mmap_para.segs[seg_id].va;
    mmap_size = devmm_svm->mmap_para.segs[seg_id].size;

    if ((ka_mm_get_vm_start(vma) != mmap_va) || (ka_mm_get_vm_end(vma) != (mmap_va + mmap_size))) {
        devmm_drv_info("Svm map va not fixed. (vm_start=0x%lx; vm_end=0x%lx; vm_pgoff=0x%lx; vm_flags=0x%lx)\n",
            ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma), vma->vm_pgoff, ka_mm_get_vm_flags(vma));
        return -EINVAL;
    }

    return 0;
}

#ifndef EMU_ST
static void devmm_unset_svm_static_vma(ka_vm_area_struct_t **svm_vma, u32 seg_num)
{
    u32 i;

    for (i = 0; i < seg_num; i++) {
        svm_vma[i] = NULL;
    }
}

static int devmm_check_and_get_svm_static_reserve_vma(u32 seg_id, u64 va, ka_vm_area_struct_t **svm_vma)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = devmm_find_vma_from_mm(ka_task_get_current_mm(), va);
    if ((vma == NULL) || (devmm_is_svm_vma_magic(ka_mm_get_vm_private_data(vma)) == false) ||
        (devmm_mmap_vma_check(seg_id, vma) != 0)) {
        if (vma == NULL) {
            devmm_drv_info("Vma is NULL. (seg_id=%u; va=0x%llx)\n", seg_id, va);
        } else {
            devmm_drv_info("Vma is not svm vma. (is_svm_vma=%u; seg_id=%u; va=0x%llx)\n",
                devmm_is_svm_vma_magic(ka_mm_get_vm_private_data(vma)), seg_id, va);
        }
        return -EINVAL;
    }
    *svm_vma = vma;
    return 0;
}
#endif

int devmm_check_and_set_svm_static_reserve_vma(void *svm_proc, ka_vm_area_struct_t **svm_vma)
{
#ifndef EMU_ST
    ka_vm_area_struct_t *vma = NULL;
    u32 seg_id;
    int ret;

    for (seg_id = 0; seg_id < devmm_svm->mmap_para.seg_num; seg_id++) {
        if (devmm_svm->mmap_para.segs[seg_id].va == DEVMM_HOST_PIN_START) {
            ret = devmm_check_and_get_svm_static_reserve_vma(seg_id, devmm_svm->mmap_para.segs[seg_id].va, &vma);
            if (ret != 0) {
                devmm_destroy_svm_proc_host_pin_heap(svm_proc);
                continue;
            }
        }
        ret = devmm_check_and_get_svm_static_reserve_vma(seg_id, devmm_svm->mmap_para.segs[seg_id].va, &vma);
        if (ret != 0) {
            goto set_vma_fail;
        }
        svm_vma[seg_id] = vma;
    }
#endif
    return 0;

#ifndef EMU_ST
set_vma_fail:
    devmm_unset_svm_static_vma(svm_vma, seg_id);
    return -EINVAL;
#endif
}

#ifndef EMU_ST
static int _devmm_check_and_set_svm_dynamic_vma(struct devmm_svm_process *svm_proc, u64 va, u64 size, void *priv)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = devmm_find_vma_from_mm(ka_task_get_current_mm(), va);
    if ((vma == NULL) || (devmm_is_svm_vma_magic(ka_mm_get_vm_private_data(vma)) == false) ||
        (ka_mm_get_vm_start(vma) != va) || (ka_mm_get_vm_end(vma) != (va + size))) {
        return 0;
    }
    (void)svm_da_set_custom_vma_nolock(svm_proc, va, vma);

    return 0;
}

static int devmm_check_and_set_svm_dynamic_vma(struct devmm_svm_process *svm_proc)
{
    int ret;

    svm_use_da(svm_proc);
    ret = svm_da_for_each_addr(svm_proc, true, _devmm_check_and_set_svm_dynamic_vma, NULL);
    svm_unuse_da(svm_proc);

    return ret;
}

/* Protection mechanism based on exclusive use of 8T virtual addresses */
int devmm_check_and_set_custom_svm_vma(void *svm_proc, ka_vm_area_struct_t **svm_vma)
{
    struct devmm_svm_process *svm_process = (struct devmm_svm_process *)svm_proc;
    int ret;

    ret = devmm_check_and_set_svm_static_reserve_vma(svm_proc, svm_vma);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_check_and_set_svm_dynamic_vma(svm_process);
    if (ret != 0) {
         devmm_unset_svm_static_vma(svm_vma, devmm_svm->mmap_para.seg_num);
         return ret;
    }

    return 0;
}
#endif

static int devmm_mmap_para_check(struct devmm_private_data *priv, ka_vm_area_struct_t *vma)
{
    int ret;

    ret = devmm_file_priv_check(priv);
    if (ret != 0) {
        devmm_drv_err("Check file priv failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devmm_mmap_vma_check(ka_base_atomic_read(&priv->next_seg_id), vma);
    if (ret != 0) {
        devmm_drv_info("Check mmap vma. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

/* 1. Unbound scenario: Set vma when binding the custom operator process.
 * 2. Bound scenario: Set vma using the process obtained from the aipcu process.
 */
STATIC struct devmm_svm_process *devmm_svm_mmap_get_svm_process(struct devmm_private_data *priv)
{
    struct devmm_custom_process *custom_proc = NULL;

    if (priv->custom_flag == 0) {
        return (struct devmm_svm_process *)priv->process;
    }

    custom_proc = devmm_get_svm_custom_proc_by_mm(ka_task_get_current_mm());

    return (custom_proc != NULL) ? custom_proc->aicpu_proc : NULL;
}

STATIC int devmm_svm_mmap(ka_file_t *file, ka_vm_area_struct_t *vma)
{
    struct devmm_private_data *priv = (struct devmm_private_data *)ka_fs_get_file_private_data(file);
    struct devmm_svm_process *svm_proc = NULL;
    int ret;

    ret = devmm_mmap_para_check(priv, vma);
    if (ret != 0) {
        devmm_drv_info("Check mmap para. (ret=%d)\n", ret);
        return ret;
    }

    svm_proc = devmm_svm_mmap_get_svm_process(priv);
    if ((ka_base_atomic_read(&priv->next_seg_id) >= devmm_svm->mmap_para.seg_num) && (svm_proc != NULL)) {
        svm_occupy_da(svm_proc);
        if (priv->custom_flag == 0) {
            ret = svm_da_add_addr(svm_proc, ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma) - ka_mm_get_vm_start(vma), vma);
        } else {
            ret = svm_da_set_custom_vma(svm_proc, ka_mm_get_vm_start(vma), vma);
        }
        svm_release_da(svm_proc);
        if (ret != 0) {
            return ret;
        }
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    ka_mm_set_vm_flags(vma, KA_VM_DONTEXPAND | KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_PFNMAP | KA_VM_LOCKED | KA_VM_WRITE | KA_VM_IO);
#else
    ka_mm_set_vm_flags(vma, KA_VM_DONTEXPAND | KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_PFNMAP | KA_VM_LOCKED | KA_VM_WRITE | KA_VM_IO);
#endif
#ifndef EMU_ST
    ka_mm_set_vm_private_data(vma, (void *)svm_vma_magic);
#endif
        devmm_svm_setup_vma_ops(vma);

    if ((priv->custom_flag == 0) && (ka_base_atomic_read(&priv->next_seg_id) < devmm_svm->mmap_para.seg_num)) {
        ret = devmm_svm_mmap_config_svm_proc(svm_proc, vma);
        if (ret != 0) {
            devmm_drv_err("Svm map config_svm_proc error. (vm_start=0x%lx; vm_end=0x%lx)\n",
                ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma));
            return ret;
        }
    }

    ka_base_atomic_inc(&priv->next_seg_id);
    if (ka_base_atomic_read(&priv->next_seg_id) == 0) {
        ka_base_atomic_set(&priv->next_seg_id, devmm_svm->mmap_para.seg_num);
    }

    return 0;
}

STATIC int devmm_svm_release(ka_inode_t *inode, ka_file_t *filp)
{
    struct devmm_svm_process *svm_proc = NULL;

    if (ka_fs_get_file_private_data(filp) == NULL) {
        devmm_drv_run_info("Private_data is NULL.\n");
        return 0;
    }
    if (((struct devmm_private_data *)ka_fs_get_file_private_data(filp))->custom_flag != 0) {
        devmm_drv_run_info("Custom exit.\n");
        goto free;
    }
    if (((struct devmm_private_data *)ka_fs_get_file_private_data(filp))->process == NULL) {
        devmm_drv_run_info("Process is NULL.\n");
        goto free;
    }
    svm_proc = (struct devmm_svm_process *)(((struct devmm_private_data *)ka_fs_get_file_private_data(filp))->process);
    devmm_svm_mem_disable(svm_proc);
    devmm_svm_release_proc(svm_proc);
free:
    devmm_kfree_ex(ka_fs_get_file_private_data(filp));
    ka_fs_get_file_private_data(filp);

    return 0;
}

STATIC int devmm_ioctl_get_svm_proc_from_file(ka_file_t *file, u32 cmd, struct devmm_svm_process **svm_proc)
{
    if (_KA_IOC_NR(cmd) < DEVMM_SVM_CMD_USE_PRIVATE_MAX_CMD) {
        *svm_proc = devmm_get_svm_proc_from_file(file);
        if (*svm_proc == NULL ||
            (devmm_get_end_type() == DEVMM_END_HOST && cmd != DEVMM_SVM_INIT_PROCESS &&
            ((*svm_proc)->inited != DEVMM_SVM_INITED_FLAG ||
            (*svm_proc)->process_id.hostpid != devmm_get_current_pid()))) {
            devmm_drv_err("Invalid svm_proc states. (cmd=%u; end_type=%u; svm_proc_is_null=%d; inited=%u; hostpid=%u)\n",
                cmd, devmm_get_end_type(), (*svm_proc == NULL),
                ((*svm_proc == NULL) ? 0 : (*svm_proc)->inited),
                ((*svm_proc == NULL) ? 0 : (*svm_proc)->process_id.hostpid));
            return -EINVAL;
        }
    }

    return 0;
}

static int devmm_dispatch_ioctl_use_svm_proc(struct devmm_svm_process *svm_proc,
    u32 cmd, struct devmm_ioctl_arg *buffer)
{
    u32 cmd_id = _KA_IOC_NR(cmd);
    u32 cmd_flag;
    int ret = 0;

    if (devmm_ioctl_handlers[cmd_id].ioctl_handler == NULL) {
        devmm_drv_err("Cmd not support. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
        return -EOPNOTSUPP;
    }
    cmd_flag = devmm_ioctl_handlers[cmd_id].cmd_flag;
    ret = devmm_convert_id_from_vir_to_phy(svm_proc, buffer, cmd_flag);
    if (ret != 0) {
        devmm_drv_err("Virtual id to physical id failed. (cmd=0x%x; cmd_id= 0x%x; ret=%d)\n", cmd, cmd_id, ret);
        return ret;
    }
    ret = devmm_check_cmd_support(cmd_flag);
    if (ret != 0) {
        devmm_drv_err("Not support cmd. (cmd=0x%x; cmd_id=0x%x)\n", cmd_flag, cmd_id);
        return ret;
    }

    devmm_svm_ioctl_lock(svm_proc, cmd_flag);
    ret = devmm_ioctl_dispatch(svm_proc, cmd_id, cmd_flag, buffer);
    devmm_svm_ioctl_unlock(svm_proc, cmd_flag);
    return ret;
}

STATIC int devmm_dispatch_ioctl_normal(ka_file_t *file, u32 cmd, struct devmm_ioctl_arg *buffer)
{
    struct devmm_svm_process *svm_proc = NULL;
    int ret;

    ret = devmm_ioctl_get_svm_proc_from_file(file, cmd, &svm_proc);
    if (ret != 0) {
        devmm_drv_err("Get svm_proc failed. (cmd=0x%x; _KA_IOC_NR(cmd)=0x%x)\n", cmd, _KA_IOC_NR(cmd));
        return ret;
    }
    ret = devmm_dispatch_ioctl_use_svm_proc(svm_proc, cmd, buffer);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

STATIC int devmm_dispatch_ioctl_for_file_arg(ka_file_t *file, u32 cmd, struct devmm_ioctl_arg *buffer)
{
    return devmm_ioctl_file_arg_handlers[_KA_IOC_NR(cmd)](file, buffer);
}

STATIC bool devmm_ioctl_cmd_is_file_arg(u32 cmd)
{
    return (devmm_ioctl_file_arg_handlers[_KA_IOC_NR(cmd)] != NULL) ? true : false;
}

STATIC int devmm_dispatch_ioctl(ka_file_t *file, u32 cmd, struct devmm_ioctl_arg *buffer)
{
    if (devmm_ioctl_cmd_is_file_arg(cmd) == true) {
        return devmm_dispatch_ioctl_for_file_arg(file, cmd, buffer);
    } else {
        return devmm_dispatch_ioctl_normal(file, cmd, buffer);
    }
}

STATIC long devmm_svm_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    struct devmm_ioctl_arg buffer = {{0}};
    u32 cmd_id = _KA_IOC_NR(cmd);
    int ret;

    if ((file == NULL) || (ka_fs_get_file_private_data(file) == NULL) || (arg == 0)) {
        devmm_drv_err("File is NULL, check svm init. (cmd=0x%x; _IOC_NR(cmd)=0x%x)\n", cmd, _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    if ((_KA_IOC_TYPE(cmd) != DEVMM_SVM_MAGIC) || (cmd_id >= DEVMM_SVM_CMD_MAX_CMD)) {
        devmm_drv_err("Cmd not support. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
        return -EINVAL;
    }

    if ((_KA_IOC_DIR(cmd) & _KA_IOC_WRITE) != 0) {
        if (ka_base_copy_from_user(&buffer, (void __ka_user *)(uintptr_t)arg, sizeof(struct devmm_ioctl_arg)) != 0) {
            devmm_drv_err("Copy_from_user fail. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
            return -EINVAL;
        }
    }

    ret = devmm_dispatch_ioctl(file, cmd, &buffer);
    if (ret != 0) {
        return ret;
    }

    if ((_KA_IOC_DIR(cmd) & _KA_IOC_READ) != 0) {
        if (ka_base_copy_to_user((void __ka_user *)(uintptr_t)arg, &buffer, sizeof(struct devmm_ioctl_arg)) != 0) {
            devmm_drv_err("Copy_to_user fail. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
            return -EINVAL;
        }
    }

    return 0;
}

STATIC ka_file_operations_t devmm_svm_fops = {
    .owner = KA_THIS_MODULE,
    .open = devmm_svm_open,
    .release = devmm_svm_release,
    .mmap = devmm_svm_mmap,
    .unlocked_ioctl = devmm_svm_ioctl,
};

#ifndef EMU_ST
static int devmm_svm_fake_open(ka_inode_t *inode, ka_file_t *file)
{
    return -EACCES;
}
#endif

static ka_file_operations_t devmm_svm_fake_fops = {
    .owner = KA_THIS_MODULE,
#ifndef EMU_ST
    .open = devmm_svm_fake_open,
#endif
};

STATIC int devmm_alloc_svm_struct(void)
{
    u32 i;

    devmm_svm = (struct devmm_svm_dev *)devmm_vzalloc_ex(sizeof(struct devmm_svm_dev));
    if (devmm_svm == NULL) {
        devmm_drv_err("Vzalloc svm fail.\n");
        return -ENOMEM;
    }

    devmm_svm->dev_no = 0;
    devmm_svm->host_page_shift = 0;
    devmm_svm->device_page_shift = 0;
    devmm_svm->svm_page_size = KA_MM_PAGE_SIZE;
#ifdef HOST_AGENT
    devmm_svm->device_page_size = KA_MM_PAGE_SIZE;
#endif
    devmm_svm->page_size_inited = 0;
    devmm_svm->smmu_status = DEVMM_SMMU_STATUS_UNINIT;
#ifdef CFG_FEATURE_VFIO
    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        devmm_svm->total_convert_len[i] = DEVMM_VDEV_MAX_CONVERT_LEN_DEFAULT;
    }
#endif

    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        devmm_svm->device_info.cluster_id[i] = DEVMM_MAX_DEVICE_NUM;
    }
    ka_task_init_rwsem(&devmm_svm->convert_sem);
    ka_base_atomic64_set(&devmm_svm->device_info.total_ddr, 0);
    ka_base_atomic64_set(&devmm_svm->device_info.total_hbm, 0);
    for (i = 0; i < HOST_REGISTER_MAX_TPYE; i++) {
        KA_INIT_LIST_HEAD(&devmm_svm->shm_pro_head[i].head);
        ka_task_mutex_init(&devmm_svm->shm_pro_head[i].node_lock);
    }
    for (i = 0; i < DEVMM_MAX_AGENTMM_DEVICE_NUM; i++) {
        ka_base_idr_init(&devmm_svm->share_phy_addr_blk_mng[i].idr);
        devmm_svm->share_phy_addr_blk_mng[i].id_start = 0;
        devmm_svm->share_phy_addr_blk_mng[i].id_end = KA_INT_MAX;
        ka_task_init_rwsem(&devmm_svm->share_phy_addr_blk_mng[i].rw_sem);
    }

    ka_task_mutex_init(&devmm_svm->setup_lock);

    return 0;
}

STATIC void devmm_free_svm_struct(void)
{
    devmm_vfree_ex(devmm_svm);
    devmm_svm = NULL;
}

STATIC int devmm_init_devmm_struct(void)
{
    if (devmm_alloc_svm_struct() != 0) {
        devmm_drv_err("Svm struct alloc fail.\n");
        return -ENOMEM;
    }

    devmm_init_dev_private(devmm_svm, &devmm_svm_fops);

    devmm_default_srcu_work_init();

    return devmm_svm_proc_mng_init();
}

STATIC void devmm_unint_devmm_struct(void)
{
    devmm_svm_proc_mng_uinit();
    devmm_free_svm_struct();
}

#ifndef DEVMM_UT
#ifdef HOST_AGENT
STATIC void devmm_svm_set_host_agent_pgsf(void)
{
    devmm_svm_set_host_pgsf(KA_MM_PAGE_SHIFT);
    devmm_svm_set_host_hpgsf(KA_MM_HPAGE_SHIFT);

    devmm_svm_set_device_pgsf(KA_MM_PAGE_SHIFT);
    devmm_svm_set_device_hpgsf(KA_MM_HPAGE_SHIFT);
    devmm_chan_set_host_device_page_size();
}
#endif
#endif

STATIC int devmm_svm_init(void)
{
    int ret;

    devmm_drv_run_info("Devmm model init. (svm_dev_size=%lu)\n", sizeof(struct devmm_svm_dev));

    ret = devmm_init_devmm_struct();
    if (ret != 0) {
        devmm_drv_err("Devmm_init_devmm_struct fail.\n");
        return -ENOMEM;
    }

    ret = devmm_svm_davinci_module_init(&devmm_svm_fops);
    if (ret != 0) {
        devmm_drv_err("Devmm_init_devmm_struct fail.\n");
        goto register_davinci_fail;
    }

    ret = devmm_svm_dev_init(&devmm_svm_fake_fops);
    if (ret != 0) {
        devmm_drv_err("Alloc_chrdev_region error.\n");
        goto register_dev_fail;
    }

#ifdef CFG_FEATURE_VFIO
    ret = vdevmmh_init();
    if (ret != 0) {
        devmm_drv_err("Register vdevmmh client error. (ret=%d)\n", ret);
#ifndef EMU_ST
        goto vdevmmh_init_fail;
#endif
    }
#endif

    ret = devmm_register_ops_init();
    if (ret != 0) {
        devmm_drv_err("Register client error. (ret=%d)\n", ret);
        goto register_client_fail;
    }

    ret = module_feature_auto_init();
    if (ret != 0) {
        devmm_drv_err("Feature atuo init fail. (ret=%d)\n", ret);
        goto feature_init_fail;
    }

#ifdef HOST_AGENT
#ifndef DEVMM_UT
    devmm_svm_set_host_agent_pgsf();
#endif
#endif

    devmm_drv_run_info("Devmm model init success.\n");
    return 0;
feature_init_fail:
    devmm_unregister_ops_uninit();
register_client_fail:
#ifndef EMU_ST
#ifdef CFG_FEATURE_VFIO
    vdevmmh_uninit();
vdevmmh_init_fail:
#endif
#endif
    devmm_svm_dev_destory();
register_dev_fail:
    devmm_svm_davinci_module_uninit();
register_davinci_fail:
    devmm_unint_devmm_struct();

    return ret;
}

STATIC void devmm_svm_exit(void)
{
    module_feature_auto_uninit();
#ifdef CFG_FEATURE_VFIO
    /*
     * vm msg_chan init need use vpc chan to sync pm info
     * rmmod ko pm proc release need use msg chan
     * so init and destroy vdevmmh first
     */
    vdevmmh_uninit();
#endif

    devmm_unregister_ops_uninit();

    devmm_uninit_dev_private(devmm_svm);

    devmm_svm_dev_destory();
    devmm_svm_davinci_module_uninit();
    devmm_unint_devmm_struct();
}

ka_module_init(devmm_svm_init);
ka_module_exit(devmm_svm_exit);
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_LICENSE("GPL");
KA_MODULE_DESCRIPTION("devmm shared memory manager driver");

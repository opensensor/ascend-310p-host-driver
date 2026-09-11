/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-02-22
 */

#include "virtmng_public_def.h"
#include "virtmnghost_ctrl.h"
#include "vmng_mem_alloc_interface.h"
#include "virtmnghost_proc_fs.h"

#include "ka_compiler_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"

#define STATIC_PROCFS_FILE_FUNC_OPS(ops, open_func, write_func)    \
    static const ka_procfs_ops_t ops = {                           \
        ka_fs_init_pf_owner(KA_THIS_MODULE)                        \
        ka_fs_init_pf_open(open_func)                              \
        ka_fs_init_pf_read(ka_fs_seq_read)                         \
        ka_fs_init_pf_lseek(ka_fs_seq_lseek)                       \
        ka_fs_init_pf_release(ka_fs_single_release)                \
        ka_fs_init_pf_write(write_func)                            \
    }

// file directory : /proc/vmng_host
static ka_proc_dir_entry_t *vmng_host_entry = NULL;

static struct vmngh_user_input g_host_id = {
    .dev_id = 0,
    .vfid = 1,
};

static struct vmngh_procfs_entry g_procfs_entry = {
    .dev_id = NULL,
    .vf_id = NULL,
    .each_resource_info = NULL,
};

#define MSG_SIZE 3
#define KSTRTOL_BASE 10
STATIC ssize_t set_global_id(ka_file_t *filp, const char *buf, size_t count, loff_t *offp, u32 *output)
{
    char *msg;
    int ret = 0;
    long temp;

    if (count > MSG_SIZE) {
        ret = -EINVAL;
        vmng_err("Set_global_id count size err!\n");
        goto error;
    }

    msg = vmng_kzalloc(MSG_SIZE, KA_GFP_KERNEL);
    if (msg == NULL) {
        ret = -ENOMEM;
        vmng_err("Set_global_id kzalloc err!\n");
        goto error;
    }

    if (ka_base_copy_from_user(msg, buf, count)) {
        ret = -EFAULT;
        vmng_err("Set_global_id ka_base_copy_from_user err!\n");
        goto free;
    }
#ifndef DRV_UT
    msg[MSG_SIZE - 1] = '\0';
#endif
    ret = ka_base_kstrtol(msg, KSTRTOL_BASE, &temp);
    if ((ret != 0) || (temp < 0) || (temp >= KA_INT_MAX)) {
        ret = -EINVAL;
        vmng_err("Set_global_id ka_base_kstrtol err!\n");
        goto free;
    }

    KA_WRITE_ONCE(*output, (u32)temp);
    ret = count;

free:
    vmng_kfree(msg);
error:
    return ret;
}

STATIC ssize_t write_dev_id(ka_file_t *filp, const char *buf, size_t count, loff_t *offp)
{
    u32 dev_id = 0;
    int ret = 0;
    ret = set_global_id(filp, buf, count, offp, &dev_id);

    if (dev_id >= VMNG_PDEV_MAX) {
        ret = -ERANGE;
    } else {
        g_host_id.dev_id = dev_id;
    }

    return ret;
}

STATIC int read_dev_id_proc_show(ka_seq_file_t *m, void *v)
{
    ka_fs_seq_printf(m, "[ dev_id = %u ]\n", g_host_id.dev_id);
    return 0;
}

STATIC int read_dev_id_proc_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, read_dev_id_proc_show, NULL);
}

STATIC ssize_t write_vfid(ka_file_t *filp, const char *buf, size_t count, loff_t *offp)
{
    u32 vfid = 0;
    int ret = 0;
    ret = set_global_id(filp, buf, count, offp, &vfid);

    if (vfid >= VMNG_VDEV_MAX_PER_PDEV) {
        ret = -ERANGE;
    } else {
        g_host_id.vfid = vfid;
    }

    return ret;
}

STATIC int read_vfid_proc_show(ka_seq_file_t *m, void *v)
{
    ka_fs_seq_printf(m, "[ vfid = %u ]\n", g_host_id.vfid);
    return 0;
}

STATIC int read_vfid_proc_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, read_vfid_proc_show, NULL);
}

STATIC void vf_res_id_info_proc_show(ka_seq_file_t *m, void *v, vf_id_info_t *id)
{
    ka_fs_seq_printf(m, "[id_info]:\n");
    ka_fs_seq_printf(m, "\tvf_id =                %u\n", id->vf_id);
    ka_fs_seq_printf(m, "\tvfg_mode =             %u\n", id->vfg_mode);
    ka_fs_seq_printf(m, "\tvfg_id =               %u\n", id->vfg_id);
    ka_fs_seq_printf(m, "\tvip =                  %u\n", id->vip);
    ka_fs_seq_printf(m, "\ttoken =                %#llx\n", id->token);
    ka_fs_seq_printf(m, "\ttoken_max =            %#llx\n", id->token_max);
    ka_fs_seq_printf(m, "\ttask_timeout =         %#llx\n", id->task_timeout);
    return;
}

STATIC void vf_res_ac_info_proc_show(ka_seq_file_t *m, void *v, vf_ac_info_t *accelerator)
{
    ka_fs_seq_printf(m, "[accelerator]:\n");
    ka_fs_seq_printf(m, "\taiv_bitmap =           %#llx\n", accelerator->aiv_bitmap);
    ka_fs_seq_printf(m, "\taic_bitmap =           %#x\n", accelerator->aic_bitmap);
    ka_fs_seq_printf(m, "\tc_core_bitmap =        %#x\n", accelerator->c_core_bitmap);
    ka_fs_seq_printf(m, "\tdsa_bitmap =           %#x\n", accelerator->dsa_bitmap);
    ka_fs_seq_printf(m, "\tffts_bitmap =          %#x\n", accelerator->ffts_bitmap);
    ka_fs_seq_printf(m, "\tsdma_bitmap =          %#x\n", accelerator->sdma_bitmap);
    ka_fs_seq_printf(m, "\tpcie_dma_bitmap =      %#x\n", accelerator->pcie_dma_bitmap);
    ka_fs_seq_printf(m, "\tacsq_slice_bitmap =    %#x\n", accelerator->acsq_slice_bitmap);
    ka_fs_seq_printf(m, "\trtsq_slice_bitmap =    %#x\n", accelerator->rtsq_slice_bitmap);
    ka_fs_seq_printf(m, "\tevent_slice_bitmap =   %#x\n", accelerator->event_slice_bitmap);
    ka_fs_seq_printf(m, "\tnotify_slice_bitmap =  %#x\n", accelerator->notify_slice_bitmap);
    ka_fs_seq_printf(m, "\tcdq_slice_bitmap =     %#x\n", accelerator->cdq_slice_bitmap);
    ka_fs_seq_printf(m, "\tcmo_slice_bitmap =     %#x\n", accelerator->cmo_slice_bitmap);

    return;
}

STATIC void vf_res_cpu_info_proc_show(ka_seq_file_t *m, void *v, vf_cpu_info_t *cpu)
{
    ka_fs_seq_printf(m, "[cpu]:\n");
    ka_fs_seq_printf(m, "\ttopic_aicpu_slot =     %#x\n", cpu->topic_aicpu_slot_bitmap);
    ka_fs_seq_printf(m, "\ttopic_ctrl_cpu_slot =  %#x\n", cpu->topic_ctrl_cpu_slot_bitmap);
    ka_fs_seq_printf(m, "\thost_ctrl_cpu =        %#x\n", cpu->host_ctrl_cpu_bitmap);
    ka_fs_seq_printf(m, "\tdevice_aicpu =         %#x\n", cpu->device_aicpu_bitmap);
    ka_fs_seq_printf(m, "\thost_aicpu =           %#llx\n", cpu->host_aicpu_bitmap);

    return;
}

STATIC void vf_res_dvpp_info_proc_show(ka_seq_file_t *m, void *v, vf_dvpp_info_t *dvpp)
{
    ka_fs_seq_printf(m, "[dvpp]:\n");
    ka_fs_seq_printf(m, "\tjpegd_bitmap =         %#x\n", dvpp->jpegd_bitmap);
    ka_fs_seq_printf(m, "\tjpege_bitmap =         %#x\n", dvpp->jpege_bitmap);
    ka_fs_seq_printf(m, "\tvpc_bitmap =           %#x\n", dvpp->vpc_bitmap);
    ka_fs_seq_printf(m, "\tvdec_bitmap =          %#x\n", dvpp->vdec_bitmap);
    ka_fs_seq_printf(m, "\tpngd_bitmap =          %#x\n", dvpp->pngd_bitmap);
    ka_fs_seq_printf(m, "\tvenc_bitmap =          %#x\n", dvpp->venc_bitmap);
    return;
}

STATIC void vf_res_info_proc_show(ka_seq_file_t *m, void *v, struct vmngh_vdev_ctrl *ctrl)
{
    struct vmng_vdev_ctrl *vdev_ctrl = &ctrl->vdev_ctrl;
    vmng_vf_cfg_t *vf_cfg = &vdev_ctrl->vf_cfg;

    ka_fs_seq_printf(m, "\tstatus =               %u\n", vdev_ctrl->status);
    ka_fs_seq_printf(m, "\tdev_id =               %u\n", vdev_ctrl->dev_id);
    ka_fs_seq_printf(m, "\tvfid =                 %u\n", vdev_ctrl->vfid);
    ka_fs_seq_printf(m, "\tvm_devid =             %u\n", vdev_ctrl->vm_devid);
    ka_fs_seq_printf(m, "\tvm_id =                %u\n", vdev_ctrl->vm_id);
    ka_fs_seq_printf(m, "\tdtype =                %u\n", vdev_ctrl->dtype);
    ka_fs_seq_printf(m, "\tcore_num =             %u\n", vdev_ctrl->core_num);
    ka_fs_seq_printf(m, "\ttotal_core_num =       %u\n", vdev_ctrl->total_core_num);
    ka_fs_seq_printf(m, "\tddr_size =             %#llx\n", vdev_ctrl->ddr_size);
    ka_fs_seq_printf(m, "\thbm_size =             %#llx\n", vdev_ctrl->hbm_size);
    ka_fs_seq_printf(m, "\tbar0_size =            %#llx\n", vdev_ctrl->bar0_size);
    ka_fs_seq_printf(m, "\tbar2_size =            %#llx\n", vdev_ctrl->bar2_size);
    ka_fs_seq_printf(m, "\tbar4_size =            %#llx\n", vdev_ctrl->bar4_size);

    ka_fs_seq_printf(m, "\tcapbility =            %u\n", vf_cfg->capbility);
    ka_fs_seq_printf(m, "\tnuma_id.bitmap =       %#lx\n", ctrl->memory.numa_id.bitmap);
    vf_res_id_info_proc_show(m, v, &vf_cfg->id);
    vf_res_ac_info_proc_show(m, v, &vf_cfg->accelerator);
    vf_res_cpu_info_proc_show(m, v, &vf_cfg->cpu);
    vf_res_dvpp_info_proc_show(m, v, &vf_cfg->dvpp);

    return;
}

STATIC int each_resource_info_proc_show(ka_seq_file_t *m, void *v)
{
    struct vmngh_vdev_ctrl *ctrl = NULL;
    u32 dev_id = g_host_id.dev_id;
    u32 vfid = g_host_id.vfid;

    ctrl = vmngh_get_ctrl(dev_id, vfid);
    if (ctrl == NULL) {
        ka_fs_seq_printf(m, "Vmngh get ctrl failed, please input valid id.(dev_id = %d; vfid = %d)\n", dev_id, vfid);
        return 0;
    }

    ka_fs_seq_printf(m, "\n----------- dev_id = %d / vfid = %d -----------\n", dev_id, vfid);
    vf_res_info_proc_show(m, v, ctrl);

    return 0;
}

STATIC int each_resource_info_proc_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, each_resource_info_proc_show, NULL);
}

STATIC_PROCFS_FILE_FUNC_OPS(vmngh_dev_id_ops, read_dev_id_proc_open, write_dev_id);
STATIC_PROCFS_FILE_FUNC_OPS(vmngh_vfid_ops, read_vfid_proc_open, write_vfid);
STATIC_PROCFS_FILE_FUNC_OPS(vmngh_each_resource_ops, each_resource_info_proc_open, NULL);

STATIC int vmngh_dev_id_proc_fs(void)
{
    g_procfs_entry.dev_id = ka_fs_proc_create_data("dev_id", KA_S_IRUSR | KA_S_IWUSR, vmng_host_entry, &vmngh_dev_id_ops, NULL);

    if (g_procfs_entry.dev_id == NULL) {
        vmng_err("Create dev_id_entry dir failed.\n");
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_vf_id_proc_fs(void)
{
    g_procfs_entry.vf_id = ka_fs_proc_create_data("vf_id", KA_S_IRUSR | KA_S_IWUSR, vmng_host_entry, &vmngh_vfid_ops, NULL);

    if (g_procfs_entry.vf_id == NULL) {
        vmng_err("Create vf_id_entry dir failed.\n");
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_each_resource_info_proc_fs(void)
{
    g_procfs_entry.each_resource_info = ka_fs_proc_create_data("each_resource_info", KA_S_IRUSR,
        vmng_host_entry, &vmngh_each_resource_ops, NULL);

    if (g_procfs_entry.each_resource_info == NULL) {
        vmng_err("Create each_resource_info_entry dir failed.\n");
        return VMNG_ERR;
    }

    return VMNG_OK;
}

int vmngh_proc_fs_init(void)
{
    vmng_host_entry = ka_fs_proc_mkdir("vmng_host", NULL);
    if (vmng_host_entry == NULL) {
        vmng_err("Create vmng_host entry dir failed\n");
        return VMNG_ERR;
    }

    if ((vmngh_dev_id_proc_fs() != 0) ||
        (vmngh_vf_id_proc_fs() != 0) ||
        (vmngh_each_resource_info_proc_fs() != 0)) {
        (void)ka_fs_remove_proc_subtree("vmng_host", NULL);
        vmng_err("Vmng_host proc fs init failed.\n");
        return VMNG_ERR;
    }
    return VMNG_OK;
}

void vmngh_proc_fs_uninit(void)
{
    (void)ka_fs_remove_proc_subtree("vmng_host", NULL);
    return;
}

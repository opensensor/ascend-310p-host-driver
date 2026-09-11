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

#include <linux/bitmap.h>

#include "dbl/uda.h"
#include "soc_res.h"

#include "devmm_vpc_mng_msg_def.h"
#include "devmm_pm_adapt.h"
#include "svm_msg_client.h"
#include "svm_proc_mng.h"
#include "devmm_pm_vpc.h"
#include "devmm_common.h"
#include "svm_interface.h"
#include "svm_pm_version_adapt.h"
#include "svm_shmem_interprocess.h"
#include "svm_dev_res_mng.h"

#include "devdrv_manager_comm.h"

struct devmm_vm_devid {
    unsigned short devid;
    unsigned short vm_id;
    unsigned short vfid;
};

STATIC u32 g_pm_svm_proc_max_num = DEVMM_VM_MAX_SVM_PROC_NUM;
struct devmm_pm_dev_info svm_pm_dev_info[DEVMM_PM_DEV_MAX_NUM];
struct devmm_vm_info svm_vm_info[DEVMM_VM_MAX_NUM];

bool devmm_is_pm_dev_fid_valid(u32 dev_id, u32 fid)
{
    if (dev_id >= DEVMM_PM_DEV_MAX_NUM || fid >= DEVMM_PM_DEV_FID_MAX_NUM) {
        return false;
    }
    if (svm_pm_dev_info[dev_id].map[fid].valid == DEVMM_VALID) {
        return true;
    }

    return false;
}

bool devmm_is_vm_dev_valid(u32 vm_id, u32 vm_dev_id)
{
    if ((vm_id >= DEVMM_VM_MAX_NUM) || (vm_dev_id >= DEVMM_VM_DEV_MAX_NUM)) {
        devmm_drv_err("Vm_id or vm_dev_id is out of range. (vm_id=%d; vm_dev_id=%d; DEVMM_VM_DEV_MAX_NUM=%d)\n",
            vm_id, vm_dev_id, DEVMM_VM_DEV_MAX_NUM);
        return false;
    }

    if (svm_vm_info[vm_id].map[vm_dev_id].valid != 0) {
        return true;
    }

    return false;
}

u32 devmm_get_vm_dev_id(u32 dev_id, u32 fid)
{
    return svm_pm_dev_info[dev_id].map[fid].vm_dev_id;
}

u32 devmm_get_dev_id_from_vm_dev_id(u32 vm_id, u32 vm_dev_id)
{
    return svm_vm_info[vm_id].map[vm_dev_id].pm_dev_id;
}

u32 devmm_get_vm_id_by_process_vm_id(u32 process_vm_id)
{
    return process_vm_id - 1;
}

u32 devmm_get_process_vm_id_by_vm_id(u32 vm_id)
{
    return vm_id + 1;
}

int devmm_pm_vpc_msg_send(u32 dev_id, u32 fid, void *msg, u32 len, u32 out_len)
{
#ifndef EMU_ST
    struct vmng_tx_msg_proc_info tx_info;
    int vm_id, ret;

    vm_id = vmngh_ctrl_get_vm_id(dev_id, fid);
    if ((vm_id < 0) || (vm_id >= DEVMM_VM_MAX_NUM)) {
        devmm_drv_err("Vm_id is error. (dev_id=%d; fid=%d; vm_id=%d)\n", dev_id, fid, vm_id);
        return -EINVAL;
    }

    tx_info.data = msg;
    tx_info.in_data_len = len;
    tx_info.out_data_len = out_len;
    tx_info.real_out_len = 0;

    ret = devmm_pm_tx_version_adapt_pre_proc((u32)vm_id, tx_info.data);
    if (ret != 0) {
        return ret;
    }

    ret = vmngh_vpc_msg_send(dev_id, fid, VMNG_VPC_TYPE_DEVMM, &tx_info, VPC_DEFAULT_TIMEOUT);
    if (ret != 0) {
        devmm_drv_err("Vpc send failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devmm_pm_tx_version_adapt_pre_proc((u32)vm_id, tx_info.data);
    return ret;
#endif
}
EXPORT_SYMBOL_UNRELEASE(devmm_pm_vpc_msg_send);

int devmm_pm_vpc_mng_msg_send(u32 dev_id, u32 fid, void *msg, u32 len, u32 out_len)
{
#ifndef EMU_ST
    struct vmng_tx_msg_proc_info tx_info;
    int vm_id, ret;

    vm_id = vmngh_ctrl_get_vm_id(dev_id, fid);
    if ((vm_id < 0) || (vm_id >= DEVMM_VM_MAX_NUM)) {
        devmm_drv_err("Vm_id is error. (dev_id=%d; fid=%d; vm_id=%d)\n", dev_id, fid, vm_id);
        return -EINVAL;
    }

    tx_info.data = msg;
    tx_info.in_data_len = len;
    tx_info.out_data_len = out_len;
    tx_info.real_out_len = 0;

    ret = devmm_pm_mng_tx_version_adapt_pre_proc((u32)vm_id, tx_info.data);
    if (ret != 0) {
        return ret;
    }

    ret = vmngh_vpc_msg_send(dev_id, fid, VMNG_VPC_TYPE_DEVMM_MNG, &tx_info, VPC_DEFAULT_TIMEOUT);
    if (ret != 0) {
        devmm_drv_err("Vpc send failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devmm_pm_mng_tx_version_adapt_post_proc((u32)vm_id, tx_info.data);
    return ret;
#endif
}
EXPORT_SYMBOL_UNRELEASE(devmm_pm_vpc_mng_msg_send);

STATIC int devmm_pm_check_vpc_msg_para(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    if ((dev_id >= DEVMM_MAX_DEVICE_NUM) || ((fid == 0) || (fid >= VMNG_VDEV_MAX_PER_PDEV))) {
        devmm_drv_err("Device id or fid error. (dev_id=%d; fid=%d)\n", dev_id, fid);
        return -EINVAL;
    }

    if ((proc_info == NULL) || (proc_info->real_out_len == NULL) || (proc_info->data == NULL)) {
        devmm_drv_err("Proc_info is NULL. (dev_id=%d; fid=%d)\n", dev_id, fid);
        return -EINVAL;
    }

    return 0;
}

static int (*devmm_pm_send_msg_pre_processes[DEVMM_CHAN_MAX_ID])
    (struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info) = {
        [DEVMM_CHAN_EX_PGINFO_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* vm not surport */
        [DEVMM_CHAN_PAGE_QUERY_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* devmm mng vpc msg */
        [DEVMM_CHAN_PAGE_CREATE_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* devmm mng vpc msg */
        [DEVMM_CHAN_PAGE_CREATE_QUERY_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* devmm mng vpc msg */
        [DEVMM_CHAN_PAGE_P2P_CREATE_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* devmm mng vpc msg */
        [DEVMM_CHAN_PAGE_FAULT_P2P_ID] = devmm_pm_channel_msg_default_pre_proc, /* vm not surport */
        [DEVMM_CHAN_ADVISE_CACHE_PERSIST_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* vm not surport */
        [DEVMM_CHAN_MAP_DEV_RESERVE_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* vm not surport */
        [DEVMM_CHAN_PAGE_FAULT_H2D_ID] = devmm_pm_channel_msg_default_pre_proc, /* devmm mng vpc msg */
        [DEVMM_CHAN_REMOTE_MAP_ID] = devmm_pm_channel_msg_default_pre_proc, /* vm not surport */
        [DEVMM_CHAN_REMOTE_UNMAP_ID] = devmm_pm_channel_msg_default_pre_proc, /* vm not surport */
};

static int (*devmm_pm_send_msg_post_processes[DEVMM_CHAN_MAX_ID])
    (struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info) = {
        [DEVMM_CHAN_SETUP_DEVICE_H2D] = devmm_pm_enable_device,
        [DEVMM_CHAN_CLOSE_DEVICE_H2D] = devmm_pm_disable_device,
};

STATIC int devmm_pm_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct svm_ver_adapt_msg_info new_msg_info = {0};
    struct devmm_svm_process_id process_id = {0};
    struct devmm_svm_process *svm_proc = NULL;
    struct devmm_chan_msg_head *head = NULL;
    u64 old_msg_len, old_out_len;
    void *old_msg = NULL;
    int vm_id, ret;
    u32 vm_dev_id;
    u16 msg_id;

    ret = devmm_pm_check_vpc_msg_para(dev_id, fid, proc_info);
    if (ret != 0) {
        return ret;
    }

    if (proc_info->in_data_len < sizeof(struct devmm_chan_msg_head)) {
        devmm_drv_err("Invalid in_data_len. (in_data_len=%u)\n", proc_info->in_data_len);
        return -EINVAL;
    }

    head = (struct devmm_chan_msg_head *)proc_info->data;
    msg_id = head->msg_id;

    if (msg_id >= DEVMM_CHAN_MAX_ID) {
        devmm_drv_err("Message id is not surport. (dev_id=%d; fid=%d; pid=%d; msg_id=%d)\n", dev_id, fid,
            head->msg_id, head->process_id.hostpid);
        return -ENOMSG;
    }

    vm_id = vmngh_ctrl_get_vm_id(dev_id, fid);
    if ((vm_id < 0) || (vm_id >= DEVMM_VM_MAX_NUM)) {
        devmm_drv_err("Vm_id is error. (dev_id=%d; fid=%d; msg_id=%d; hostpid=%d; vm_id=%d)\n",
            dev_id, fid, msg_id, head->process_id.hostpid, vm_id);
        return -EINVAL;
    }

    if (head->dev_id >= DEVMM_VM_DEV_MAX_NUM) {
        devmm_drv_err("Device id is error. (vm=%d; dev_id=%d)\n", vm_id, head->dev_id);
        return -ENODEV;
    }

    process_id.hostpid = head->process_id.hostpid;
    process_id.vm_id = (u16)devmm_get_process_vm_id_by_vm_id((u32)vm_id);

    ka_task_down_read(&svm_vm_info[vm_id].channel_lock);
    ka_task_mutex_lock(&svm_vm_info[vm_id].proc_lock);
    svm_proc = devmm_svm_proc_get_by_process_id_ex(&process_id);
    if (svm_proc == NULL) {
        ka_task_mutex_unlock(&svm_vm_info[vm_id].proc_lock);
        ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
        devmm_drv_err("Process not exist. (dev_id=%d; fid=%d; pid=%d; msg_id=%d; vm_id=%d)\n",
            dev_id, fid, head->process_id.hostpid, msg_id, vm_id);
        return -ESRCH;
    }
    if (devmm_svm_other_proc_occupy_get_lock(svm_proc) != 0) {
        devmm_svm_proc_put(svm_proc);
        ka_task_mutex_unlock(&svm_vm_info[vm_id].proc_lock);
        ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
        devmm_drv_debug("Process is exiting. (hostpid=%d)\n", svm_proc->process_id.hostpid);
        return -EBUSY;
    }
    ka_task_mutex_unlock(&svm_vm_info[vm_id].proc_lock);
    if (devmm_is_vm_dev_valid((u32)vm_id, head->dev_id) == false) {
        devmm_svm_other_proc_occupy_put_lock(svm_proc);
        devmm_svm_proc_put(svm_proc);
        ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
        devmm_drv_err("Device id is invalid. (vm=%d; dev_id=%d)\n", vm_id, head->dev_id);
        return -EINVAL;
    }
#ifndef EMU_ST
    ret = devmm_pm_rx_version_adapt_pre_proc((u32)vm_id, proc_info->data, proc_info->in_data_len, &new_msg_info);
    if (ret != 0) {
        devmm_svm_other_proc_occupy_put_lock(svm_proc);
        devmm_svm_proc_put(svm_proc);
        ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
        return ret;
    }

    old_msg = proc_info->data;
    old_msg_len = proc_info->in_data_len;
    old_out_len = proc_info->out_data_len;
    if (new_msg_info.msg != NULL) {
        proc_info->data = new_msg_info.msg;
        proc_info->in_data_len = (u32)new_msg_info.msg_len;
        proc_info->out_data_len = (u32)new_msg_info.out_len;
        head = (struct devmm_chan_msg_head *)proc_info->data;
    }
#endif

    if (devmm_pm_send_msg_pre_processes[msg_id] != NULL) {
        ret = devmm_pm_send_msg_pre_processes[msg_id](svm_proc, dev_id, fid, proc_info);
        if (ret != 0) {
            goto send_fail;
        }
    }

    vm_dev_id = head->dev_id;
    head->process_id.vfid = (u16)fid;
    head->dev_id = (u16)dev_id;
    ret = devmm_chan_msg_send_inner(proc_info->data, proc_info->in_data_len, proc_info->out_data_len);
    head->dev_id = (u16)vm_dev_id;
    head->process_id.vfid = 0;
    if (ret != 0) {
        goto send_fail;
    }

    if (devmm_pm_send_msg_post_processes[msg_id] != NULL) {
        (void)devmm_pm_send_msg_post_processes[msg_id](svm_proc, dev_id, fid, proc_info);
    }
send_fail:
#ifndef EMU_ST
    proc_info->data = old_msg;
    proc_info->in_data_len = (u32)old_msg_len;
    proc_info->out_data_len = (u32)old_out_len;
    head = (struct devmm_chan_msg_head *)proc_info->data;
    (void)devmm_pm_rx_version_adapt_post_proc((u32)vm_id, proc_info->data, proc_info->in_data_len, &new_msg_info);
#endif
    devmm_svm_other_proc_occupy_put_lock(svm_proc);
    devmm_svm_proc_put(svm_proc);
    ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
    *proc_info->real_out_len = proc_info->out_data_len;

    /*
     * 1. if svm process is wrong, the error code is returned by head->result
     * 2. devmm_pm_vpc_msg_recv always return 0, beacuse another error code means vpc msg wrong
     */
#ifndef EMU_ST
    if (devmm_pm_is_need_return_head_result((u32)vm_id)) {
        head->result = (short)ret;
        ret = 0;
    }
#endif
    return ret;
}

STATIC int devmm_pm_sync(u32 dev_id, u32 fid, u32 vm_id, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_sync *sync = (struct devmm_vpc_mng_msg_sync *)proc_info->data;
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_sync);

    if (proc_info->in_data_len != len) {
        devmm_drv_err("In_len or out_len error. (dev_id=%d; in_len=%d; out_len=%d)\n",
            dev_id, proc_info->in_data_len, proc_info->out_data_len);
        return -EMSGSIZE;
    }

    svm_vm_info[vm_id].map[sync->head.dev_id].pm_dev_id = dev_id;
    svm_vm_info[vm_id].map[sync->head.dev_id].pm_fid = fid;
    svm_vm_info[vm_id].map[sync->head.dev_id].valid = 1;

    svm_pm_dev_info[dev_id].map[fid].vm_dev_id = sync->head.dev_id;

    sync->pm_fid = fid;
    sync->pm_dev_id = dev_id;
    sync->device_page_shift = devmm_svm_get_device_pgsf();
    sync->device_hpage_shift = devmm_svm_get_device_hpgsf();
    sync->dev_mem[DEVMM_EXCHANGE_DDR_SIZE] = devmm_svm->device_info.ddr_size[dev_id][0];
    sync->dev_mem_p2p[DEVMM_EXCHANGE_DDR_SIZE] = devmm_svm->device_info.p2p_ddr_size[dev_id][0];
    sync->dev_mem[DEVMM_EXCHANGE_HBM_SIZE] = devmm_svm->device_info.hbm_size[dev_id][0];
    sync->dev_mem_p2p[DEVMM_EXCHANGE_HBM_SIZE] = devmm_svm->device_info.p2p_hbm_size[dev_id][0];
    sync->dvpp_memsize = devmm_svm->dev_capability[dev_id].dvpp_memsize;
    sync->feature_phycial_address = (u8)devmm_svm->dev_capability[dev_id].feature_phycial_address;
    sync->double_pgtable_offset = devmm_svm->dev_capability[dev_id].double_pgtable_offset;

    devmm_drv_info("Synchronize vm. (dev_id=%d; fid=%d; dev_id=%d)\n", dev_id, fid, sync->head.dev_id);

    *proc_info->real_out_len = len;
    return 0;
}

static struct devmm_pm_vpc_handlers devmm_vpc_mng_msg_processes[DEVMM_VPC_MNG_MSG_MAX_ID] = {
    [DEVMM_VPC_INIT_PROC] =
        {
            devmm_pm_init_process,
            sizeof(struct devmm_vpc_mng_msg_init_process),
            0
        },
    [DEVMM_VPC_UNINIT_PROC] =
        {
            devmm_pm_release_process,
            sizeof(struct devmm_vpc_mng_msg_init_process),
            0
        },
    [DEVMM_VPC_MNG_CREATE_DEV_PAGE] =
        {
            devmm_pm_create_device_page,
            sizeof(struct devmm_vpc_mng_msg_create_dev_page),
            0
        },
    [DEVMM_VPC_MNG_DELETE_DEV_PAGE_CACHE] =
        {
            devmm_pm_del_device_page_cache,
            sizeof(struct devmm_vpc_mng_msg_del_dev_page_cache),
            0
        },
    [DEVMM_VPC_MNG_COPY_MEM] =
        {
            devmm_pm_memcpy_process_res,
            sizeof(struct devmm_vpc_mng_msg_mem_copy),
            sizeof(u64)
        },
    [DEVMM_VPC_MNG_CONVERT_ADDR] =
        {
            devmm_pm_convert_addr_proc,
            sizeof(struct devmm_vpc_mng_msg_convert_addr),
            sizeof(u64)
        },
    [DEVMM_VPC_MNG_CONVERT_DMA_ADDR] =
        {
            devmm_pm_convert_dma_addr_proc,
            sizeof(struct devmm_vpc_mng_msg_convert_dma_addr),
            0
        },
    [DEVMM_VPC_MNG_DESTROY_ADDR] =
        {
            devmm_pm_destroy_addr_proc,
            sizeof(struct devmm_vpc_mng_msg_destroy_addr),
            0
        },
    [DEVMM_VPC_MNG_TRANSLATE_ADDR] =
        {
            devmm_pm_translate_addr_proc,
            sizeof(struct devmm_vpc_mng_msg_translate_addr),
            0
        },
    [DEVMM_VPC_MNG_CLEAR_TRANSLATE_ADDR] =
        {
            devmm_pm_clear_translate_addr_proc,
            sizeof(struct devmm_vpc_mng_msg_clear_translate_addr),
            0
        },
    [DEVMM_VPC_MNG_HOST_PAGE_FAULT] =
        {
            devmm_pm_page_fault_h2d_sync,
            sizeof(struct devmm_vpc_mng_msg_page_fault),
            sizeof(u64)
        },
    [DEVMM_VPC_MNG_AGENT_MEM_MAP] =
        {
            devmm_pm_agent_mem_map,
            sizeof(struct devmm_vpc_mng_msg_agent_mem_map),
            0
        },
};

STATIC int devmm_vpc_mng_svm_proc_get(u32 msg_id,
    struct devmm_svm_process_id *process_id, struct devmm_svm_process **svm_proc)
{
    if (msg_id == DEVMM_VPC_INIT_PROC) { /* before DEVMM_VPC_INIT_PROC do not need svm_process */
        *svm_proc = devmm_svm_proc_get_by_process_id_ex(process_id);
        if (*svm_proc != NULL) {
            devmm_drv_err("Hostpid has been init. (vf=%d; vm_id=%d; hostpid=%d)\n",
                process_id->vfid, process_id->vm_id, process_id->hostpid);
            return -EALREADY;
        }
    } else if (msg_id > DEVMM_VPC_INIT_PROC) {
        *svm_proc = devmm_svm_proc_get_by_process_id_ex(process_id);
        if (*svm_proc == NULL) {
            devmm_drv_err("Hostpid has not been init. (vf=%d; vm_id=%d; hostpid=%d)\n",
                process_id->vfid, process_id->vm_id, process_id->hostpid);
            return -ESRCH;
        }
        if (msg_id == DEVMM_VPC_UNINIT_PROC) {
            devmm_del_from_svm_proc_hashtable_lock(*svm_proc);
            devmm_svm_proc_put(*svm_proc);
        }
    }

    return 0;
}

STATIC void devmm_vpc_mng_svm_proc_put(u32 msg_id, struct devmm_svm_process *svm_proc)
{
    if ((msg_id != DEVMM_VPC_UNINIT_PROC) && (svm_proc != NULL)) {
        devmm_svm_proc_put(svm_proc);
    }
}

int devmm_pm_svm_proc_test_and_inc(u32 process_vm_id)
{
    u32 vm_id = devmm_get_vm_id_by_process_vm_id(process_vm_id);
    if ((process_vm_id >= 1) && (vm_id < DEVMM_VM_MAX_NUM) &&
        (ka_base_atomic_inc_return(&svm_vm_info[vm_id].svm_proc_num) > g_pm_svm_proc_max_num)) {
        devmm_drv_err("Vmid out of range. (vm_id=%d)\n", vm_id);
        ka_base_atomic_dec(&svm_vm_info[vm_id].svm_proc_num);
        return -ESRCH;
    }

    return 0;
}

void devmm_pm_svm_proc_sub(u32 process_vm_id)
{
    u32 vm_id = devmm_get_vm_id_by_process_vm_id(process_vm_id);
    if ((process_vm_id >= 1) && (vm_id < DEVMM_VM_MAX_NUM)) {
        ka_base_atomic_dec(&svm_vm_info[vm_id].svm_proc_num);
    }
}

int devmm_pm_idle_convert_len_sub(u32 process_vm_id, u32 devid, u32 vfid, u64 len)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[devid];
    u32 vm_id = devmm_get_vm_id_by_process_vm_id(process_vm_id);

    ka_task_down_read(&devmm_svm->convert_sem);
    if ((process_vm_id >= 1) && (vm_id < DEVMM_VM_MAX_NUM)) {
        ka_task_mutex_lock(&convert_mng->mutex);
        if (len > convert_mng->vdev_free_convert_len[vfid]) {
            devmm_drv_err("Not enough convert_len. (devid=%u; vfid=%u; idle_convert_len=%llu; request_len=%llu)\n",
                devid, vfid, convert_mng->vdev_free_convert_len[vfid], len);
            ka_task_mutex_unlock(&convert_mng->mutex);
            ka_task_up_read(&devmm_svm->convert_sem);
            return -ENOSPC;
        }

        convert_mng->vdev_free_convert_len[vfid] -= len;
        ka_task_mutex_unlock(&convert_mng->mutex);
    }
    ka_task_up_read(&devmm_svm->convert_sem);
    devmm_drv_debug("Idle convert len sub. (vmid=%u; devid=%u; vfid=%u; idle_len=%llu; request_len=%llu)\n",
        vm_id, devid, vfid, convert_mng->vdev_free_convert_len[vfid], len);

    return 0;
}

void devmm_pm_idle_convert_len_add(u32 process_vm_id, u32 devid, u32 vfid, u64 len)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[devid];
    u32 vm_id = devmm_get_vm_id_by_process_vm_id(process_vm_id);

    ka_task_down_read(&devmm_svm->convert_sem);
    if ((process_vm_id >= 1) && (vm_id < DEVMM_VM_MAX_NUM)) {
        ka_task_mutex_lock(&convert_mng->mutex);
        if ((convert_mng->vdev_free_convert_len[vfid] + len) >= convert_mng->vdev_total_convert_len[vfid]) {
            convert_mng->vdev_free_convert_len[vfid] = convert_mng->vdev_total_convert_len[vfid];
        } else {
            convert_mng->vdev_free_convert_len[vfid] += len;
        }
        ka_task_mutex_unlock(&convert_mng->mutex);
    }
    ka_task_up_read(&devmm_svm->convert_sem);
    devmm_drv_debug ("Idle convert len add. (vmid=%u; devid=%u; vfid=%u; idle_len=%llu; release_len=%llu)\n",
        vm_id, devid, vfid, convert_mng->vdev_free_convert_len[vfid], len);
}

int devmm_pm_check_set_convert_len_para(u32 devid, const void *buf, u32 buf_size)
{
    if ((buf == NULL) || (buf_size < sizeof(struct devmm_set_convert_len_para))) {
        devmm_drv_err("Invalid buf. (buf_size=%u)\n", buf_size);
        return -EINVAL;
    }

    if ((devid >= DEVMM_MAX_DEVICE_NUM) && (devid != DEVMM_SET_ALL_DEV_CONVERT_LEN)) {
        devmm_drv_err("Invalid devid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    return 0;
}

STATIC bool devmm_pm_dev_convert_is_busy(u32 devid)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[devid];
    int i;

    for (i = 0; i < DEVMM_MAX_VF_NUM; i++) {
        /* Vdev has not been inited */
        if (convert_mng->total_core_num[i] == 0) {
            continue;
        }

        if (convert_mng->vdev_free_convert_len[i] != convert_mng->vdev_total_convert_len[i]) {
            return true;
        }
    }
    return false;
}

STATIC bool devmm_pm_convert_is_busy(void)
{
    u32 i;

    for (i = 0; i < (u32)DEVMM_MAX_DEVICE_NUM; i++) {
        if (devmm_pm_dev_convert_is_busy(i)) {
            return true;
        }
    }

    return false;
}

STATIC void devmm_pm_update_dev_total_convert_len(u32 devid, u64 set_len)
{
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[devid];
    int i;

    devmm_svm->total_convert_len[devid] = set_len;
    for (i = 0; i < DEVMM_MAX_VF_NUM; i++) {
        u64 len = 0;

        /* Vdev has not been inited */
        if (convert_mng->total_core_num[i] == 0) {
            continue;
        }
        if ((set_len * convert_mng->core_num[i]) < set_len) {
            continue;
        }
        len = set_len * convert_mng->core_num[i] / convert_mng->total_core_num[i];
        convert_mng->vdev_total_convert_len[i] = len;
        convert_mng->vdev_free_convert_len[i] = len;
    }
}

STATIC void devmm_pm_update_total_convert_len(u64 set_len)
{
    u32 i;

    for (i = 0; i < (u32)DEVMM_MAX_DEVICE_NUM; i++) {
        devmm_pm_update_dev_total_convert_len(i, set_len);
    }
}

STATIC int devmm_pm_set_all_dev_convert_len(u64 set_len)
{
    ka_task_down_write(&devmm_svm->convert_sem);
    if (devmm_pm_convert_is_busy()) {
        devmm_drv_err("Convert is busy.\n");
        ka_task_up_write(&devmm_svm->convert_sem);
        return -EBUSY;
    }

    devmm_pm_update_total_convert_len(set_len);
    ka_task_up_write(&devmm_svm->convert_sem);
    return 0;
}

STATIC int devmm_pm_set_dev_convert_len(u32 devid, u64 set_len)
{
    ka_task_down_write(&devmm_svm->convert_sem);
    if (devmm_pm_dev_convert_is_busy(devid)) {
        devmm_drv_err("Convert is busy. (devid=%u)\n", devid);
        ka_task_up_write(&devmm_svm->convert_sem);
        return -EBUSY;
    }

    devmm_pm_update_dev_total_convert_len(devid, set_len);
    ka_task_up_write(&devmm_svm->convert_sem);
    return 0;
}

int devmm_pm_set_convert_len(u32 devid, const void *buf, u32 buf_size)
{
    struct devmm_set_convert_len_para *set_para = (struct devmm_set_convert_len_para *)buf;
    bool set_all_dev;
    int ret = 0;

    ret = devmm_pm_check_set_convert_len_para(devid, buf, buf_size);
    if (ret != 0) {
        devmm_drv_err("Set convert len para err.\n");
        return ret;
    }

    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        devmm_drv_err("Get module fail.\n");
        return -EBUSY;
    }

    set_all_dev = (devid == DEVMM_SET_ALL_DEV_CONVERT_LEN) ? true : false;
    if (set_all_dev) {
        ret = devmm_pm_set_all_dev_convert_len(set_para->total_convert_len);
    } else {
        ret = devmm_pm_set_dev_convert_len(devid, set_para->total_convert_len);
    }

    if (ret != 0) {
        devmm_drv_err("Update vdev convert len err.");
        goto return_err;
    }

    devmm_drv_info("Set convert total len success. (devid=%u; set_len=%llu)\n",
        devid, set_para->total_convert_len);
return_err:
    ka_system_module_put(KA_THIS_MODULE);
    return ret;
}

STATIC int devmm_pm_check_get_convert_len_para(u32 devid, void *buf, u32 *buf_size)
{
    if ((buf == NULL) || (buf_size == NULL)) {
        devmm_drv_err("Invalid buf.\n");
        return -EINVAL;
    }

    if (*buf_size < sizeof(struct devmm_get_convert_len_para)) {
        devmm_drv_err("Invalid buf_size. (buf_size=%u)\n", *buf_size);
        *buf_size = 0;
        return -EINVAL;
    }

    if (devid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid devid. (devid=%u)\n", devid);
        *buf_size = 0;
        return -EINVAL;
    }

    return 0;
}

STATIC int devmm_pm_get_dev_convert_len(u32 devid, u64 *total_convert_len)
{
    ka_task_down_read(&devmm_svm->convert_sem);
    *total_convert_len = devmm_svm->total_convert_len[devid];
    ka_task_up_read(&devmm_svm->convert_sem);
    return 0;
}

STATIC int devmm_pm_get_convert_len(u32 devid, void *buf, u32 *buf_size)
{
    struct devmm_get_convert_len_para *get_para = (struct devmm_get_convert_len_para *)buf;
    int ret;

    ret = devmm_pm_check_get_convert_len_para(devid, buf, buf_size);
    if (ret != 0) {
        devmm_drv_err("Get convert len para err.\n");
        return ret;
    }

    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        devmm_drv_err("Get module fail.\n");
        *buf_size = 0;
        return -EBUSY;
    }

    ret = devmm_pm_get_dev_convert_len(devid, &get_para->total_convert_len);
    if (ret != 0) {
        devmm_drv_err("Get dev convert len fail.");
        *buf_size = 0;
        goto return_err;
    }

    *buf_size = sizeof(struct devmm_get_convert_len_para);
    devmm_drv_info("Get convert total len success. (devid=%u; get_len=%llu)\n",
        devid, get_para->total_convert_len);
return_err:
    ka_system_module_put(KA_THIS_MODULE);
    return ret;
}

void devmm_register_set_vdev_convert_len_fun(void)
{
    (void)dms_register_set_dev_info_handler(DEVMM_SET_VDEV_CONVERT_LEN, devmm_pm_set_convert_len);
}

void devmm_unregister_set_vdev_convert_len_fun(void)
{
    (void)dms_unregister_set_dev_info_handler(DEVMM_SET_VDEV_CONVERT_LEN);
}

static void devmm_register_get_vdev_convert_len_fun(void)
{
    (void)dms_register_get_svm_dev_info_handler(DEVMM_GET_VDEV_CONVERT_LEN, devmm_pm_get_convert_len);
}

static void devmm_unregister_get_vdev_convert_len_fun(void)
{
    (void)dms_unregister_get_svm_dev_info_handler(DEVMM_SET_VDEV_CONVERT_LEN);
}

STATIC int devmm_pm_svm_proc_get_lock(struct devmm_svm_process *svm_proc, u16 msg_id)
{
    if (msg_id == DEVMM_VPC_UNINIT_PROC) {
        return 0;
    }
    if (svm_proc != NULL) {
        return devmm_svm_other_proc_occupy_get_lock(svm_proc);
    }
    return 0;
}

STATIC void devmm_pm_svm_proc_put_lock(struct devmm_svm_process *svm_proc, u16 msg_id)
{
    if (msg_id == DEVMM_VPC_UNINIT_PROC) {
        return;
    }
    if (svm_proc != NULL) {
        devmm_svm_other_proc_occupy_put_lock(svm_proc);
    }
}

STATIC int devmm_pm_vpc_mng_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_head *head = NULL;
    struct devmm_svm_process *svm_proc = NULL;
    u32 vm_dev_id, proc_len;
    int vm_id, ret;
    u16 msg_id;

    ret = devmm_pm_check_vpc_msg_para(dev_id, fid, proc_info);
    if (ret != 0) {
        return ret;
    }

    if (proc_info->in_data_len < sizeof(struct devmm_vpc_mng_msg_head)) {
        devmm_drv_err("Invalid in_data_len. (in_data_len=%u)\n", proc_info->in_data_len);
        return -EINVAL;
    }

    head = (struct devmm_vpc_mng_msg_head *)proc_info->data;
    msg_id = (u16)head->msg_id;

    vm_id = vmngh_ctrl_get_vm_id(dev_id, fid);
    if ((vm_id < 0) || (vm_id >= DEVMM_VM_MAX_NUM)) {
        devmm_drv_err("Vm_id is error. (dev_id=%d; fid=%d; pid=%d; msg_id=%d; vm_id=%d)\n",
            dev_id, fid, msg_id, head->process_id.hostpid, vm_id);
        return -EINVAL;
    }

    if (head->dev_id >= DEVMM_VM_DEV_MAX_NUM) {
        devmm_drv_err("Device id error. (vm=%d; dev_id=%d)\n", vm_id, head->dev_id);
        return -EINVAL;
    }

    if (msg_id >= DEVMM_VPC_MNG_MSG_MAX_ID) {
        devmm_drv_err("Message id is out of range. (msg_id=%d)\n", msg_id);
        return -EINVAL;
    }

    if (msg_id == DEVMM_VPC_SYNC_VERSION) {
        return devmm_pm_sync_version((u32)vm_id, proc_info);
    }
#ifndef EMU_ST
    ret = devmm_pm_mng_rx_version_adapt_pre_proc((u32)vm_id, proc_info->data);
    if (ret != 0) {
        return ret;
    }
#endif
    /* Synchronization messages need to establish a mapping relationship, without going through the following process */
    if (msg_id == DEVMM_VPC_SYNC) {
        return devmm_pm_sync(dev_id, fid, (u32)vm_id, proc_info);
    }

    if (devmm_vpc_mng_msg_processes[msg_id].msg_process == NULL) {
        devmm_drv_err("Message id is not surport. (dev_id=%d; fid=%d; pid=%d; msg_id=%d)\n",
            dev_id, fid, head->process_id.hostpid, msg_id);
        return -EINVAL;
    }

    proc_len = devmm_vpc_mng_msg_processes[msg_id].msg_size +
        head->extend_num * devmm_vpc_mng_msg_processes[msg_id].extend_size;
    if (proc_info->in_data_len < proc_len) {
        devmm_drv_err("In_len or out_len error. (vm_id=%d; hostpid=%d; in_len=%d; out_len=%d)\n",
            vm_id, head->process_id.hostpid, proc_info->in_data_len, proc_info->out_data_len);
        return -EINVAL;
    }

    if (devmm_is_vm_dev_valid((u32)vm_id, head->dev_id) == false) {
        devmm_drv_err("Device id invalid. (vm=%d; dev_id=%d)\n", vm_id, head->dev_id);
        return -EINVAL;
    }

    head->process_id.vm_id = (u16)devmm_get_process_vm_id_by_vm_id((u32)vm_id);
    vm_dev_id = head->dev_id;
    head->dev_id = dev_id;
    ka_task_down_read(&svm_vm_info[vm_id].channel_lock);
    ka_task_mutex_lock(&svm_vm_info[vm_id].proc_lock);
    ret = devmm_vpc_mng_svm_proc_get(msg_id, &head->process_id, &svm_proc);
    if (ret != 0) {
        ka_task_mutex_unlock(&svm_vm_info[vm_id].proc_lock);
        ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
        devmm_drv_err("Get svm_pro failed.\n");
        return ret;
    }
    if (devmm_pm_svm_proc_get_lock(svm_proc, msg_id) != 0) {
        devmm_vpc_mng_svm_proc_put(msg_id, svm_proc);
        ka_task_mutex_unlock(&svm_vm_info[vm_id].proc_lock);
        ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
        devmm_drv_debug("Process is exiting. (hostpid=%d)\n", svm_proc->process_id.hostpid);
        return -EBUSY;
    }
    ka_task_mutex_unlock(&svm_vm_info[vm_id].proc_lock);

    ret = devmm_vpc_mng_msg_processes[msg_id].msg_process(svm_proc, fid, proc_info);
    if (ret != 0) {
        devmm_drv_err("Message processes error. (dev_id=%d; fid=%d; pid=%d; msg_id=%d; vm_id=%d; ret=%d)\n",
            dev_id, fid, head->process_id.hostpid, msg_id, vm_id, ret);
    }

    devmm_pm_svm_proc_put_lock(svm_proc, msg_id);
    devmm_vpc_mng_svm_proc_put(msg_id, svm_proc);
    ka_task_up_read(&svm_vm_info[vm_id].channel_lock);
#ifndef EMU_ST
    (void)devmm_pm_mng_rx_version_adapt_post_proc((u32)vm_id, proc_info->data);
#endif
    head->dev_id = vm_dev_id;
    head->process_id.vm_id = 0;

    return ret;
}

struct vmng_vpc_client vdevmmh_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_DEVMM,
    .init = NULL,
    .msg_recv = devmm_pm_vpc_msg_recv
};

struct vmng_vpc_client vdevmmh_vpc_mng_client = {
    .vpc_type = VMNG_VPC_TYPE_DEVMM_MNG,
    .init = NULL,
    .msg_recv = devmm_pm_vpc_mng_msg_recv
};

STATIC int vdevmmh_init_vm_proc(u32 vm_id, u32 dev_id, u32 vfid)
{
    int ret;

    if (devmm_is_pm_dev_fid_valid(dev_id, vfid)) {
        devmm_drv_err("Fid has been init. (dev_id=%d; fid=%d)\n", dev_id, vfid);
        return -EINVAL;
    }

    ret = vmngh_vpc_register_client_safety(dev_id, vfid, &vdevmmh_vpc_client);
    if (ret != 0) {
        devmm_drv_err("Vdevmm vpc client register failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = vmngh_vpc_register_client_safety(dev_id, vfid, &vdevmmh_vpc_mng_client);
    if (ret != 0) {
        (void)vmngh_vpc_unregister_client(dev_id, vfid, &vdevmmh_vpc_client);
        devmm_drv_err("Vdevmm vpc client register failed. (ret=%d)\n", ret);
        return ret;
    }

    svm_pm_dev_info[dev_id].map[vfid].valid = DEVMM_VALID;
    svm_vm_info[vm_id].init_cnt++;

    devmm_drv_info("Vm_id init. (vm_id=%d; init_cnt=%d)\n", vm_id, svm_vm_info[vm_id].init_cnt);
    return 0;
}

STATIC void vdevmmh_init_vdev_convert_resource(u32 dev_id, u32 vfid, u32 core_num, u32 total_core_num)
{
    struct devmm_pm_convert_ids *convert_ids = &devmm_svm->convert_ids[dev_id];
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];

    if (total_core_num == 0) {
        devmm_drv_warn("No available core. (dev_id=%u)\n", dev_id);
        return;
    }

    ka_task_down_write(&devmm_svm->convert_sem);
    convert_mng->vdev_total_data_num[vfid] = devmm_svm->dev_capability[dev_id].svm_offset_num *
        core_num / total_core_num;
    convert_mng->vdev_free_data_num[vfid] = convert_mng->vdev_total_data_num[vfid];
    ka_base_atomic_set(&convert_ids->vdev_free_id_num[vfid], convert_mng->vdev_total_data_num[vfid]);
    convert_mng->vdev_total_convert_len[vfid] = devmm_svm->total_convert_len[dev_id] * core_num / total_core_num;
    convert_mng->vdev_free_convert_len[vfid] = convert_mng->vdev_total_convert_len[vfid];
    convert_mng->core_num[vfid] = core_num;
    convert_mng->total_core_num[vfid] = total_core_num;
    ka_task_up_write(&devmm_svm->convert_sem);
    devmm_drv_info("Init vdev convert resource details. (vdev_total_data_num=%u; vdev_total_convert_len=%llu)\n",
        convert_mng->vdev_total_data_num[vfid], convert_mng->vdev_total_convert_len[vfid]);
}

STATIC int vdevmmh_init_instance(u32 dev_id, u32 vfid, u32 aicore_num, u32 total_aicore_num)
{
    struct svm_id_inst id_inst;
    int ret;

    svm_id_inst_pack(&id_inst, dev_id, vfid);
    ret = devmm_dev_res_mng_create(&id_inst, NULL);
    if (ret != 0) {
        return ret;
    }

    vdevmmh_init_vdev_convert_resource(dev_id, vfid, aicore_num, total_aicore_num);

    devmm_drv_info("Init. (devid=%d; fid=%d; core_num=%d; total_core_num=%d)\n",
        dev_id, vfid, aicore_num, total_aicore_num);
    return 0;
}

STATIC int vdevmmh_mia_agent_init_instance(u32 vm_id, u32 dev_id, u32 vfid)
{
    int ret;

    ret = vdevmmh_init_vm_proc(vm_id, dev_id, vfid);
    if (ret != 0) {
        devmm_drv_err("Init vm failed. (vm_id=%d)\n", vm_id);
        return ret;
    }
    return 0;
}

STATIC bool devmm_svm_proc_cmp_vmid(struct devmm_svm_process *svm_proc, u64 cmp_arg)
{
    struct devmm_vm_devid *vm_devid = (struct devmm_vm_devid *)(uintptr_t)cmp_arg;

    if (svm_proc->process_id.vm_id == devmm_get_process_vm_id_by_vm_id(vm_devid->vm_id)) {
        return true;
    }
    return false;
}

STATIC void devmm_destory_all_agent_proc(struct devmm_vm_devid *vm_devid)
{
    struct devmm_svm_process *svm_proc = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 vm_id;

    while (1) {
        svm_proc = devmm_sreach_svm_proc_each_get_and_del(devmm_svm_proc_cmp_vmid,
            (u64)(uintptr_t)vm_devid);
        if (svm_proc == NULL) {
            return;
        }
        vm_id = svm_proc->process_id.vm_id;
        devmm_notify_deviceprocess(svm_proc);
        devmm_svm_proc_wait_exit(svm_proc);
        devmm_pm_release_vm_agent_proc(svm_proc);
        devmm_pm_svm_proc_sub(vm_id);
        devmm_try_cond_resched(&stamp);
    }
}

STATIC void devmm_destory_vf_agent_proc(u32 vm_id, u32 dev_id, u32 vfid)
{
    u32 vm_dev_id = devmm_get_vm_dev_id(dev_id, vfid);
    struct devmm_vm_devid vm_devid = {0};

    svm_vm_info[vm_id].map[vm_dev_id].valid = DEVMM_INVALID;
    if (devmm_device_is_ready(dev_id) == false) {
        devmm_drv_info("Channel has close. (devid=%d)\n", dev_id);
        return;
    }

    vm_devid.vm_id = (u16)vm_id;
    vm_devid.devid = (u16)dev_id;
    vm_devid.vfid = (u16)vfid;
    ka_task_down_write(&svm_vm_info[vm_id].channel_lock);
    devmm_destory_all_agent_proc(&vm_devid);
    ka_task_up_write(&svm_vm_info[vm_id].channel_lock);
}

STATIC int vdevmmh_uninit_vm_proc(u32 vm_id, u32 dev_id, u32 vfid)
{
    if (!devmm_is_pm_dev_fid_valid(dev_id, vfid)) {
        devmm_drv_err("Fid has not been init. (dev_id=%d; fid=%d)\n", dev_id, vfid);
        return -EINVAL;
    }

    svm_pm_dev_info[dev_id].map[vfid].valid = DEVMM_INVALID;
    svm_vm_info[vm_id].init_cnt--;
    vmngh_vpc_unregister_client(dev_id, vfid, &vdevmmh_vpc_mng_client);
    vmngh_vpc_unregister_client(dev_id, vfid, &vdevmmh_vpc_client);

    devmm_destory_vf_agent_proc(vm_id, dev_id, vfid);

    return 0;
}

STATIC void vdevmmh_uninit_vdev_convert_resource(u32 dev_id, u32 vfid)
{
    struct devmm_pm_convert_ids *convert_ids = &devmm_svm->convert_ids[dev_id];
    struct devmm_share_memory_head *convert_mng = &devmm_svm->pa_info[dev_id];

    ka_task_down_write(&devmm_svm->convert_sem);
    convert_mng->vdev_total_data_num[vfid] = 0;
    convert_mng->vdev_free_data_num[vfid] = 0;
    ka_base_atomic_set(&convert_ids->vdev_free_id_num[vfid], 0);
    convert_mng->vdev_total_convert_len[vfid] = 0;
    convert_mng->vdev_free_convert_len[vfid] = 0;
    convert_mng->core_num[vfid] = 0;
    convert_mng->total_core_num[vfid] = 0;
    ka_task_up_write(&devmm_svm->convert_sem);

    return;
}

STATIC int vdevmmh_uninit_instance(u32 dev_id, u32 vfid)
{
    struct svm_id_inst id_inst;

    vdevmmh_uninit_vdev_convert_resource(dev_id, vfid);

    svm_id_inst_pack(&id_inst, dev_id, vfid);
    devmm_dev_res_mng_destroy(&id_inst);

    return 0;
}

STATIC int vdevmmh_mia_agent_uninit_instance(u32 vm_id, u32 dev_id, u32 vfid)
{
    int ret;

    ret = vdevmmh_uninit_vm_proc(vm_id, dev_id, vfid);
    if (ret != 0) {
        devmm_drv_err("Uninit vm failed. (vm_id=%d)\n", vm_id);
        return ret;
    }

    devmm_pm_clear_vm_version(vm_id);

    return 0;
}

static int vdevmmh_get_aicore_num(u32 udevid, u32 phy_devid, u32 *aicore_num, u32 *total_aicore_num)
{
    u64 bitmap;
    u32 unit_per_bit;
    int ret;

    ret = soc_resmng_dev_get_mia_res(udevid, MIA_AC_AIC, &bitmap, &unit_per_bit);
    if (ret != 0) {
        devmm_drv_err("Get aicore bitmap failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }
    *aicore_num = (u32)ka_base_bitmap_weight((const unsigned long *)&bitmap, 64); /* 64 u64 bitnum */

    ret = soc_resmng_dev_get_mia_res(phy_devid, MIA_AC_AIC, &bitmap, &unit_per_bit);
    if (ret != 0) {
        devmm_drv_err("Get aicore bitmap failed. (udevid=%u; ret=%d)\n", phy_devid, ret);
        return ret;
    }
    *total_aicore_num = (u32)ka_base_bitmap_weight((const unsigned long *)&bitmap, 64); /* 64 u64 bitnum */

    return 0;
}

#define SVM_HOST_MIA_NOTIFIER "svm_mia"
static int vdevmmh_mia_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
        devmm_drv_err("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
    }
    fid = mia_para.sub_devid + 1;

    if (action == UDA_INIT) {
        u32 aicore_num, total_aicore_num;
        ret = vdevmmh_get_aicore_num(udevid, mia_para.phy_devid, &aicore_num, &total_aicore_num);
        if (ret == 0) {
            ret = vdevmmh_init_instance(mia_para.phy_devid, fid, aicore_num, total_aicore_num);
        }
    } else if (action == UDA_UNINIT) {
        ret = vdevmmh_uninit_instance(mia_para.phy_devid, fid);
    }

    devmm_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

static int vdevmmh_mia_dev_agent_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    int vm_id;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
        devmm_drv_err("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
    }

    fid = mia_para.sub_devid + 1;
    vm_id = vmngh_ctrl_get_vm_id(mia_para.phy_devid, fid);
    if (vm_id == VMNGH_VM_ID_DEFAULT) {
        devmm_drv_err("vmngh_ctrl_get_vm_id failed. (dev_id=%u; vfid=%u)\n",
            mia_para.phy_devid, fid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = vdevmmh_mia_agent_init_instance(vm_id, mia_para.phy_devid, fid);
    } else if (action == UDA_UNINIT) {
        ret = vdevmmh_mia_agent_uninit_instance(vm_id, mia_para.phy_devid, fid);
    }

    devmm_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

static int vdevmmh_notifier_register(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_near_virtual_entity_type_pack(&type);
    ret = uda_notifier_register(SVM_HOST_MIA_NOTIFIER, &type, UDA_PRI2, vdevmmh_mia_dev_notifier_func);
    if (ret != 0) {
        devmm_drv_err("Register mia dev notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    uda_davinci_near_virtual_agent_type_pack(&type);
    ret = uda_notifier_register(SVM_HOST_MIA_NOTIFIER, &type, UDA_PRI2, vdevmmh_mia_dev_agent_notifier_func);
    if (ret != 0) {
        uda_davinci_near_virtual_entity_type_pack(&type);
        (void)uda_notifier_unregister(SVM_HOST_MIA_NOTIFIER, &type);
        devmm_drv_err("Register mia dev agent notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

static void vdevmmh_notifier_unregister(void)
{
    struct uda_dev_type type;

    uda_davinci_near_virtual_entity_type_pack(&type);
    (void)uda_notifier_unregister(SVM_HOST_MIA_NOTIFIER, &type);
    uda_davinci_near_virtual_agent_type_pack(&type);
    (void)uda_notifier_unregister(SVM_HOST_MIA_NOTIFIER, &type);
}

int vdevmmh_init(void)
{
    int ret, i, j;

    for (i = 0; i < DEVMM_PM_DEV_MAX_NUM; i++) {
        for (j = 0; j < DEVMM_PM_DEV_FID_MAX_NUM; j++) {
            svm_pm_dev_info[i].map[j].valid = DEVMM_INVALID;
        }
    }

    for (i = 0; i < DEVMM_VM_MAX_NUM; i++) {
        for (j = 0; j < DEVMM_VM_DEV_MAX_NUM; j++) {
            svm_vm_info[i].map[j].valid = DEVMM_INVALID;
        }
        svm_vm_info[i].init_cnt = 0;
        ka_task_init_rwsem(&svm_vm_info[i].channel_lock);
        ka_task_mutex_init(&svm_vm_info[i].proc_lock);
        ka_base_atomic_set(&svm_vm_info[i].svm_proc_num, 0);
    }

    ret = vdevmmh_notifier_register();
    if (ret != 0) {
        return ret;
    }

    devmm_register_set_vdev_convert_len_fun();
    devmm_register_get_vdev_convert_len_fun();
    devmm_drv_info("Vdevmm register vmnh client success.\n");
    return 0;
}

void vdevmmh_uninit(void)
{
    devmm_unregister_set_vdev_convert_len_fun();
    devmm_unregister_get_vdev_convert_len_fun();
    vdevmmh_notifier_unregister();

    devmm_drv_info("Vdevmm unregister vmnh client success.\n");
    return;
}

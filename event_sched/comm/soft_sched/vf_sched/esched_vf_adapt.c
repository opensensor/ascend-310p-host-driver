/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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

#include "esched.h"
#include "esched_vf.h"
#include "esched_chip_def.h"

int esched_get_pfvf_id_by_devid(u32 dev_id, u32 *chip_id, u32 *vfid)
{
    *chip_id = dev_id;
    return 0;
}

bool sched_vf_has_free_cpu(struct sched_vf_ctx *vf_ctx)
{
    /* Returns true:
        1. Physical machine, identified by the VF_id of the process
        2. In the VF scenario, determine the number of AICPUs occupied currently
    */
    if (vf_ctx->vfid == SCHED_DEFAULT_VF_ID) {
        return true;
    }

    if ((vf_ctx->sched_cpu_mask != vf_ctx->config_sched_cpu_mask) &&
        (vf_ctx->cur_use_cpu_num < vf_ctx->max_use_cpu_num)) {
        return true;
    }

    return false;
}

bool sched_is_cpu_belongs_to_vf(struct sched_numa_node *node, u32 vfid, struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_vf_ctx *vf_ctx = sched_get_vf_ctx(node, vfid);

    /* Returns true:
        1. Physical machine, identified by the VF_id of the process
        2. In the VF scenario, determine the number of AICPUs occupied currently
    */
    if (vfid == SCHED_DEFAULT_VF_ID) {
        return true;
    }

    if ((0x1ULL << cpu_ctx->cpuid) & vf_ctx->config_sched_cpu_mask) {
        return true;
    }

    return false;
}

bool sched_use_vf_cpu_handle_event(struct sched_numa_node *node, u32 vfid, struct sched_cpu_ctx *cpu_ctx)
{
    struct sched_vf_ctx *vf_ctx = NULL;
    vf_ctx = sched_get_vf_ctx(node, vfid);

    /* Returns true:
        1. Physical machine, identified by the VF_id of the process
        2. In the VF scenario, determine the number of AICPUs occupied currently
    */
    if (vfid == SCHED_DEFAULT_VF_ID) {
        return true;
    }

    ka_task_spin_lock_bh(&vf_ctx->vf_lock);
    if (((0x1ULL << cpu_ctx->cpuid) & vf_ctx->config_sched_cpu_mask) &&
        (vf_ctx->cur_use_cpu_num < vf_ctx->max_use_cpu_num)) {
        vf_ctx->sched_cpu_mask |= (0x1ULL << cpu_ctx->cpuid);
        vf_ctx->cur_use_cpu_num++;
        ka_task_spin_unlock_bh(&vf_ctx->vf_lock);
        return true;
    }

    ka_task_spin_unlock_bh(&vf_ctx->vf_lock);
    cpu_ctx->cannot_handle_event_reason = SCHED_CANNOT_HANDLE_NO_AVAIABLE_RESOURCE;

    return false;
}

void sched_release_cpu_res(struct sched_thread_ctx *thread_ctx)
{
    struct sched_proc_ctx *proc_ctx = thread_ctx->grp_ctx->proc_ctx;
    struct sched_vf_ctx *vf_ctx = NULL;
    u32 sched_cpuid;

    if ((proc_ctx->vfid == SCHED_DEFAULT_VF_ID) || (thread_ctx->grp_ctx->sched_mode == SCHED_MODE_NON_SCHED_CPU)) {
        return;
    }

    vf_ctx = sched_get_vf_ctx(proc_ctx->node, (u32)proc_ctx->vfid);

    ka_task_spin_lock_bh(&vf_ctx->vf_lock);
    sched_cpuid = thread_ctx->bind_cpuid;
    vf_ctx->sched_cpu_mask &= (~(0x1U << sched_cpuid));
    vf_ctx->cur_use_cpu_num--;
    ka_task_spin_unlock_bh(&vf_ctx->vf_lock);

    return;
}

void sched_event_list_vf_init(struct sched_event_list *event_list)
{
    int i;

    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        event_list->slice_cur_event_num[i] = 0;
    }
}

int esched_fill_vf_info(struct esched_vf_info *vf_info, struct vmngd_client_instance *instance)
{
    struct vmng_soc_res_info vfg_info = {0};

    int ret = vmngd_enquire_vfg_resource(instance->vdev_ctrl.dev_id, instance->vdev_ctrl.vfid, &vfg_info);
    if (ret != 0) {
        sched_err("Enquire vf resource info failed, (ret=%d).\n", ret);
        return ret;
    }

    vf_info->chip_id = instance->vdev_ctrl.dev_id;
    vf_info->vfgid = instance->vdev_ctrl.vf_cfg.id.vfg_id;
    vf_info->vfid = instance->vdev_ctrl.vfid;
    vf_info->dtype = instance->vdev_ctrl.dtype;
    vf_info->config_sched_cpu_mask = vfg_info.stars_refresh.device_aicpu;

    return 0;
}

STATIC void sched_update_vfg_other_vf_aicpu(u32 chip_id, u32 vfgid, u32 aicpu_bitmap)
{
    struct sched_numa_node *node = sched_get_numa_node(chip_id);
    struct sched_vf_ctx *vf_ctx = NULL;
    u32 i;

    for (i = 1; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        vf_ctx = sched_get_vf_ctx(node, i);
        if (ka_base_atomic_read(&vf_ctx->status) == SCHED_VF_STATUS_NORMAL && vf_ctx->vfgid == vfgid) {
            ka_task_spin_lock_bh(&vf_ctx->vf_lock);
            vf_ctx->config_sched_cpu_mask = (((u64)aicpu_bitmap) << sched_get_previous_cpu_num(chip_id));
            ka_task_spin_unlock_bh(&vf_ctx->vf_lock);
            sched_info("Update vf aicpu. (vfid=%u; aicpu_bitmap=%llx)\n", i, vf_ctx->config_sched_cpu_mask);
        }
    }
}

STATIC int sched_get_vf_resource_info(u32 devid, u32 *slice_num, u32 dtype)
{
    u32 aicore_num_total = 32; /* inited */
    u32 aicore_need;
    int ret;

    ret = hvdevmng_get_aicore_num(devid, SCHED_DEFAULT_VF_ID, &aicore_num_total);
    if ((ret != 0) || (aicore_num_total == 0)) {
        sched_err("Failed to invoke the hvdevmng_get_aicore_num. (chip_id=%u; aicore_num_total=%u; ret=%d)\n",
                  devid, aicore_num_total, ret);
        return ret;
    }

    aicore_need = 0x1ULL << dtype;
    if (dtype == VMNG_TYPE_C1) {
        *slice_num = aicore_need;
    } else {
        *slice_num = SCHED_MAX_RESOURCE_PACKET_NUM * aicore_need / aicore_num_total;
    }

    sched_info("Show details. (devid=%u; aicore_num_total=%u; dtype=%u; slice_num=%u)\n",
        devid, aicore_num_total, dtype, *slice_num);

    return 0;
}

int esched_init_vf_ctx(struct sched_vf_ctx *vf_ctx, struct esched_vf_info *vf_info)
{
    u32 resource_slice_num = 0;
    int ret;

    ret = sched_get_vf_resource_info(vf_info->chip_id, &resource_slice_num, vf_info->dtype);
    if ((ret != 0) || (resource_slice_num == 0)) {
        return DRV_ERROR_INNER_ERR;
    }

    vf_ctx->vfgid = vf_info->vfgid;
    vf_ctx->vfid = vf_info->vfid;
    vf_ctx->config_sched_cpu_mask = (((u64)vf_info->config_sched_cpu_mask) <<
        sched_get_previous_cpu_num(vf_info->chip_id));
    vf_ctx->sched_cpu_mask = 0;
    sched_update_vfg_other_vf_aicpu(vf_info->chip_id, vf_info->vfgid, vf_info->config_sched_cpu_mask);
    vf_ctx->que_depth = SCHED_PUBLISH_EVENT_QUE_DEPTH / SCHED_MAX_RESOURCE_PACKET_NUM * resource_slice_num;
    vf_ctx->max_use_cpu_num = resource_slice_num;
    vf_ctx->cur_use_cpu_num = 0;
    ka_base_atomic64_set(&vf_ctx->stat.cur_event_num, 0);
    ka_base_atomic64_set(&vf_ctx->stat.publish_event_num, 0);
    ka_base_atomic64_set(&vf_ctx->stat.sched_event_num, 0);

    return 0;
}

int esched_uninit_vf_ctx(struct sched_vf_ctx *vf_ctx, struct esched_vf_info *vf_info)
{
    sched_update_vfg_other_vf_aicpu(vf_info->chip_id, vf_info->vfgid, vf_info->config_sched_cpu_mask);
    return 0;
}


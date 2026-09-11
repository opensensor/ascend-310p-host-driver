/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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
#include "esched_vf_adapt.h"

struct vmngd_client g_client;

/* In virtualization scenarios, publishing events between different VFs is strictly prohibited */
int sched_publish_check_event_source(u32 event_src, struct sched_proc_ctx *dest_proc_ctx)
{
    struct sched_proc_ctx *cur_proc_ctx = NULL;
    struct sched_numa_node *cur_node = NULL;
    int cur_proc_pid = SCHED_CURRENT_TGID;
    enum devdrv_process_type cp_type;
    u32 dev_id;
    u32 host_pid;
    int32_t vfid;
    int32_t ret;

    /* Do not limit: publish event from kernel or without VF */
    if ((dest_proc_ctx->node->slice_ctx.enable_flag == SCHED_INVALID) ||
        (event_src == SCHED_PUBLISH_FORM_KERNEL)) {
        return 0;
    }

    cur_node = sched_get_numa_node((u32)ka_system_numa_node_id());
    if (cur_node == NULL) {
        sched_err("Failed to get current numa node. (cur_cpuid=%u)\n", sched_get_cur_processor_id());
        return DRV_ERROR_PARA_ERROR;
    }

    cur_proc_ctx = esched_proc_get(cur_node, cur_proc_pid);
    if (cur_proc_ctx != NULL) {
        vfid = cur_proc_ctx->vfid;
        esched_proc_put(cur_proc_ctx);
    } else {
        ret = devdrv_query_process_host_pid(cur_proc_pid, &dev_id, &vfid, &host_pid, &cp_type);
        if (ret != 0) {
            sched_err("Failed to invoke the devdrv_query_process_host_pid. (pid=%d; ret=%d)\n", cur_proc_pid, ret);
            return ret;
        }
    }

    /* vf_0 has highest authority, no limit */
    if ((vfid == SCHED_DEFAULT_VF_ID) || (dest_proc_ctx->vfid == SCHED_DEFAULT_VF_ID))  {
        return 0;
    }

    /* ordinary CP proc */
    if (vfid != dest_proc_ctx->vfid) {
        sched_err("Cannot publish events between different vfids. "
                  "(cur_proc_pid=%d; cur_proc_vfid=%d; dest_proc_pid=%d; dest_proc_vfid=%d)\n",
                  cur_proc_pid, vfid, dest_proc_ctx->pid, dest_proc_ctx->vfid);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

void sched_vf_proc_ctx_num_inc(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    struct sched_vf_ctx *vf_ctx = NULL;

    vf_ctx = sched_get_vf_ctx(node, (u32)proc_ctx->vfid);
    if (ka_base_atomic_read(&vf_ctx->status) != SCHED_VF_STATUS_NORMAL) {
        sched_err("It has not been created yet. (pid=%d; vfid=%d; status=%d)\n",
                  proc_ctx->pid, proc_ctx->vfid, ka_base_atomic_read(&vf_ctx->status));
        return;
    }

    ka_base_atomic_inc(&vf_ctx->proc_num);
}

void sched_vf_proc_ctx_num_dec(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    struct sched_vf_ctx *vf_ctx = NULL;

    vf_ctx = sched_get_vf_ctx(node, (u32)proc_ctx->vfid);
    if (ka_base_atomic_read(&vf_ctx->status) != SCHED_VF_STATUS_NORMAL) {
        sched_err("It has not been created yet. (pid=%d; vfid=%d; status=%d)\n",
                  proc_ctx->pid, proc_ctx->vfid, ka_base_atomic_read(&vf_ctx->status));
        return;
    }

    ka_base_atomic_dec(&vf_ctx->proc_num);
}

int sched_vf_proc_ctx_init(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    u32 host_pid = SCHED_DEFAULT_HOST_PID;
    u32 vfid = SCHED_DEFAULT_VF_ID;
    enum devdrv_process_type cp_type;
    struct sched_vf_ctx *vf_ctx = NULL;
    u32 dev_id, chip_id;
    int ret;

    if (node->slice_ctx.enable_flag == SCHED_INVALID) {
        return 0;
    }

    ret = devdrv_query_process_host_pid(proc_ctx->pid, &dev_id, &vfid, &host_pid, &cp_type);
    if (ret != 0) {
        sched_err("Failed to invoke the devdrv_query_process_host_pid. (pid=%d; ret=%d)\n", proc_ctx->pid, ret);
        return ret;
    }

    ret = esched_get_pfvf_id_by_devid(dev_id, &chip_id, &vfid);
    if (ret != 0) {
        sched_err("Fail to get pfvf id. (dev_id=%u)\n", dev_id);
        return ret;
    }

    if (((chip_id != node->node_id) && (cp_type != DEVDRV_PROCESS_DEV_ONLY)) ||
        (vfid >= VMNG_VDEV_MAX_PER_PDEV) || (cp_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        sched_err("The variable chip_id, vfid_tmp or cp_type is invalid. "
                  "(pid=%d; host_pid=%u; chip_id=%u; vfid=%u; cp_type=%d)\n",
                  proc_ctx->pid, host_pid, chip_id, vfid, (int)cp_type);
        return DRV_ERROR_PARA_ERROR;
    }

    vf_ctx = sched_get_vf_ctx(node, vfid);
    if (ka_base_atomic_read(&vf_ctx->status) != SCHED_VF_STATUS_NORMAL) {
        sched_err("It has not been created yet. (chip_id=%u; pid=%d; host_pid=%u; vfid=%u; status=%d)\n",
                  chip_id, proc_ctx->pid, host_pid, vfid, ka_base_atomic_read(&vf_ctx->status));
        return DRV_ERROR_INNER_ERR;
    }

    proc_ctx->vfid = vfid;
    proc_ctx->cp_type = cp_type;

    return 0;
}

STATIC int sched_del_vf_proc(struct sched_numa_node *node, struct sched_vf_ctx *vf_ctx)
{
    int proc_total_num = ka_base_atomic_read(&vf_ctx->proc_num);
    int i, num = 0;
    struct sched_proc_ctx *proc_ctx = NULL;
    struct pid_entry *entry = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    int *proc_in_vf = NULL;

    if ((proc_total_num == 0) || (ka_base_atomic_read(&vf_ctx->status) != SCHED_VF_STATUS_DELETING)) {
        return 0;
    }

    sched_info("Del vf proc start. (vfid=%u; proc_total_num=%d)\n", vf_ctx->vfid, proc_total_num);

    proc_in_vf = sched_kzalloc(proc_total_num * sizeof(int), KA_GFP_KERNEL);
    if (proc_in_vf == NULL) {
        sched_err("Failed to kzalloc memory for variable proc_in_vf. (vfid=%u)\n", vf_ctx->vfid);
        return DRV_ERROR_INNER_ERR;
    }

    ka_task_mutex_lock(&node->pid_list_mutex);
    ka_list_for_each_safe(pos, n, &node->pid_list) {
        entry = ka_list_entry(pos, struct pid_entry, list);
        proc_ctx = esched_proc_get(node, entry->pid);
        if (proc_ctx == NULL) {
            continue;
        }

        if (((u32)proc_ctx->vfid == vf_ctx->vfid) && (proc_ctx->status == SCHED_VALID)) {
            proc_in_vf[num] = proc_ctx->pid;
            num++;
        }

        esched_proc_put(proc_ctx);
    }
    ka_task_mutex_unlock(&node->pid_list_mutex);

    for (i = 0; i < proc_total_num; i++) {
        (void)sched_del_process(node->node_id, proc_in_vf[i]);
    }

    sched_kfree(proc_in_vf);
    sched_info("Del vf proc end. (vfid=%u; proc_total_num=%d; del_num=%d)\n", vf_ctx->vfid, proc_total_num, num);

    return 0;
}

int sched_create_vf(struct vmngd_client_instance *instance)
{
    struct sched_numa_node *node = NULL;
    struct sched_vf_ctx *vf_ctx = NULL;
    struct esched_vf_info vf_info;
    int ret;

    if (instance == NULL) {
        sched_err("The variable instance is NULL.\n");
        return DRV_ERROR_INNER_ERR;
    }

    ret = esched_fill_vf_info(&vf_info, instance);
    if (ret != 0) {
        sched_err("Fail to fill vf_info. (ret=%d)\n", ret);
        return ret;
    }

    if ((vf_info.chip_id >= SCHED_MAX_CHIP_NUM) || (vf_info.vfid >= VMNG_VDEV_MAX_PER_PDEV)
        || (vf_info.dtype >= VMNG_TYPE_MAX)) {
        sched_err("The variable chip_id, vfid or dtype is out of range. (chip_id=%u; vfid=%u; dtype=%u)\n",
                  vf_info.chip_id, vf_info.vfid, vf_info.dtype);
        return DRV_ERROR_INNER_ERR;
    }

    node = esched_dev_get(vf_info.chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", vf_info.chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    vf_ctx = sched_get_vf_ctx(node, vf_info.vfid);
    if (ka_base_atomic_read(&vf_ctx->status) != SCHED_VF_STATUS_UNCREATED) {
        sched_err("It has has been created. (vfid=%u; dtype=%u; config_sched_cpu_mask=%llx; status=%d)\n",
                  vf_info.vfid, vf_info.dtype, vf_ctx->config_sched_cpu_mask, ka_base_atomic_read(&vf_ctx->status));
        esched_dev_put(node);
        return DRV_ERROR_REPEATED_INIT;
    }

    ret = esched_init_vf_ctx(vf_ctx, &vf_info);
    if (ret != 0) {
        sched_err("Fail to init vf ctx. (chip_id=%u; vfid=%u; dtype=%u)\n",
            vf_info.chip_id, vf_info.vfid, vf_info.dtype);
        esched_dev_put(node);
        return ret;
    }

    if (node->slice_ctx.enable_flag == SCHED_INVALID) {
        node->slice_ctx.enable_flag = SCHED_VALID;
    }

    node->slice_ctx.num++;
    vf_ctx->create_timestamp = sched_get_cur_timestamp();
    ka_base_atomic_set(&vf_ctx->status, SCHED_VF_STATUS_NORMAL);

    sched_info("Called sched_create_vf successfully. (chip_id=%u; vfgid=%u; vfid=%u; "
               "config_sched_cpu_mask=%x; create_timestamp=%llu; num=%u)\n",
               vf_info.chip_id, vf_info.vfgid, vf_info.vfid, vf_info.config_sched_cpu_mask,
               vf_ctx->create_timestamp, node->slice_ctx.num);
    esched_dev_put(node);
    return 0;
}

int sched_del_vf(struct vmngd_client_instance *instance)
{
    struct sched_numa_node *node = NULL;
    struct sched_vf_ctx *vf_ctx = NULL;
    struct esched_vf_info vf_info;
    int ret;

    if (instance == NULL) {
        sched_err("The variable instance is NULL.\n");
        return DRV_ERROR_INNER_ERR;
    }

    ret = esched_fill_vf_info(&vf_info, instance);
    if (ret != 0) {
        sched_err("Fail to fill vf_info. (ret=%d)\n", ret);
        return ret;
    }

    if ((vf_info.chip_id >= SCHED_MAX_CHIP_NUM) || (vf_info.vfid >= VMNG_VDEV_MAX_PER_PDEV)) {
        sched_err("The variable chip_id or vfid is out of range. (chip_id=%u; vfid=%u; dtype=%u)\n",
                  vf_info.chip_id, vf_info.vfid, vf_info.dtype);
        return DRV_ERROR_INNER_ERR;
    }

    node = esched_dev_get(vf_info.chip_id);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", vf_info.chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    vf_ctx = sched_get_vf_ctx(node, vf_info.vfid);
    if (ka_base_atomic_read(&vf_ctx->status) != SCHED_VF_STATUS_NORMAL) {
        sched_warn("It has not been created. (vfid=%u)\n", vf_info.vfid);
        esched_dev_put(node);
        return 0;
    }

    ka_base_atomic_set(&vf_ctx->status, SCHED_VF_STATUS_DELETING);

    ret = sched_del_vf_proc(node, vf_ctx);
    if (ret != 0) {
        ka_base_atomic_set(&vf_ctx->status, SCHED_VF_STATUS_NORMAL);
        sched_err("Failed to invoke the sched_del_vf_proc. (vfid=%u; ret=%d)\n", vf_info.vfid, ret);
        esched_dev_put(node);
        return ret;
    }

    ret = esched_uninit_vf_ctx(vf_ctx, &vf_info);
    if (ret != 0) {
        ka_base_atomic_set(&vf_ctx->status, SCHED_VF_STATUS_NORMAL);
        sched_err("Fail to uninit vf ctx. (chip_id=%u; vfid=%u; dtype=%u)\n",
            vf_info.chip_id, vf_info.vfid, vf_info.dtype);
        esched_dev_put(node);
        return ret;
    }
    ka_base_atomic_set(&vf_ctx->status, SCHED_VF_STATUS_UNCREATED);
    node->slice_ctx.num--;
    if (node->slice_ctx.num == 0) {
        node->slice_ctx.enable_flag = SCHED_INVALID;
    }
    sched_info("Calling sched_del_vf succeeded. (chip_id=%u; vfgid=%u; vfid=%u; "
               "config_sched_cpu_mask=%x; cur_timestamp=%llu, num=%u)\n",
               vf_info.chip_id, vf_info.vfgid, vf_info.vfid, vf_info.config_sched_cpu_mask,
               sched_get_cur_timestamp(), node->slice_ctx.num);
    esched_dev_put(node);
    return 0;
}

struct sched_vf_ctx *sched_get_vf_ctx(struct sched_numa_node *node, u32 vfid)
{
    return &node->slice_ctx.vf_ctx[vfid];
}

void sched_node_vf_init(u32 devid)
{
    struct sched_numa_node *node = sched_get_numa_node(devid);
    struct sched_vf_ctx *vf_ctx = NULL;
    int i;

    /* Initialize vf_0 on the physical machine */
    vf_ctx = sched_get_vf_ctx(node, SCHED_DEFAULT_VF_ID);
    vf_ctx->vfid = SCHED_DEFAULT_VF_ID;
    vf_ctx->max_use_cpu_num = node->cpu_num;
    vf_ctx->config_sched_cpu_mask = node->cpu_num;
    vf_ctx->que_depth = SCHED_PUBLISH_EVENT_QUE_DEPTH;
    vf_ctx->create_timestamp = sched_get_cur_timestamp();
    ka_base_atomic_set(&vf_ctx->proc_num, 0);
    ka_base_atomic_set(&vf_ctx->status, SCHED_VF_STATUS_NORMAL);

    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        vf_ctx = sched_get_vf_ctx(node, i);
        ka_task_spin_lock_init(&vf_ctx->vf_lock);
    }
}

void sched_vf_init(void)
{
    int ret;

    g_client.type = VMNGD_CLIENT_TYPE_ESCHED;
    g_client.init_instance = sched_create_vf;
    g_client.uninit_instance = sched_del_vf;

    ret = vmngd_register_client(&g_client);
    if (ret != 0) {
        sched_err("Failed to invoke the vmngd_register_client. (ret=%d)\n", ret);
        return;
    }
}

void sched_vf_uninit(void)
{
    int ret;

    g_client.type = VMNGD_CLIENT_TYPE_ESCHED;
    g_client.init_instance = sched_create_vf;
    g_client.uninit_instance = sched_del_vf;

    ret = vmngd_unregister_client(&g_client);
    if (ret != 0) {
        sched_err("Failed to invoke the vmngd_unregister_client. (ret=%d)\n", ret);
        return;
    }
}

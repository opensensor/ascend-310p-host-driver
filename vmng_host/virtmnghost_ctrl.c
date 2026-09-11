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

#include "dbl/uda.h"

#include "vmng_mem_alloc_interface.h"
#include "devdrv_interface.h"
#include "virtmnghost_ctrl.h"
#include "virtmnghost_unit.h"
#include "hw_vdavinci.h"
#include "virtmng_interface.h"
#include "virtmng_public_def.h"
#include "virtmnghost_pci.h"
#include "virtmng_res_drv.h"

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_type.h"
#include "ka_system_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

struct vmngh_client *g_vmngh_clients[VMNG_CLIENT_TYPE_MAX];
struct vmngh_vascend_client *g_vmngh_vascend_clients[VMNG_CLIENT_TYPE_MAX];

ka_mutex_t g_vmngh_ctrl_mutex;

ka_mutex_t g_vmngh_vm_ctrl_mutex;
struct vmngh_vm_ctrl g_vmngh_vm_ctrl[VMNG_VM_MAX];

struct vmngh_clear_timer g_clear_timer;

struct vmngh_device_ctrl *g_vmngh_device_ctrl;

enum vmng_split_mode vmng_get_device_split_mode(u32 dev_id)
{
    unsigned pf_id, vf_id;
    int ret;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        ret = devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id);
        if (ret != 0) {
            vmng_err("Trans dev_id to phy_id and vf_id failed.(dev_id=%u,pf_id=%u,vf_id=%u)\n", dev_id, pf_id, vf_id);
            return -EINVAL;
        }
        return g_vmngh_device_ctrl[pf_id].split_mode;
    }

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    return g_vmngh_device_ctrl[dev_id].split_mode;
}
KA_EXPORT_SYMBOL(vmng_get_device_split_mode);

void vmng_set_device_split_mode(u32 dev_id, enum vmng_split_mode split_mode)
{
    g_vmngh_device_ctrl[dev_id].split_mode = split_mode;
}

struct vmngh_ctrl_ops *vmngh_get_ctrl_ops(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        return NULL;
    }

    return &g_vmngh_device_ctrl[dev_id].ctrl_ops;
}

struct vmngh_vdev_ctrl *vmngh_get_ctrl(u32 dev_id, u32 vfid)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return NULL;
    }
    if (vfid >= VMNG_VDEV_MAX_PER_PDEV) {
        vmng_err("Input parameter is error. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return NULL;
    }

    return &g_vmngh_device_ctrl[dev_id].vdev_ctrl[vfid];
}

STATIC struct vmngh_client_instance *vmngh_get_client_instance(u32 dev_id, u32 vfid, enum vmng_client_type client_type)
{
    return &g_vmngh_device_ctrl[dev_id].vdev_ctrl[vfid].client_instance[client_type];
}

ka_mutex_t *vmngh_get_ctrl_mutex(void)
{
    return &g_vmngh_ctrl_mutex;
}

STATIC int vmngh_alloc_vfid_dynamic(u32 dev_id, u32 *fid)
{
    u32 vfid = 0;

    if (fid == NULL) {
        vmng_err("Param NULL.\n");
        return VMNG_ERR;
    }

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    for (vfid = VMNG_VDEV_FIRST_VFID; vfid < VMNG_VDEV_MAX_PER_PDEV; vfid++) {
        if (vmngh_get_ctrl(dev_id, vfid)->vdev_ctrl.status == VMNG_VDEV_STATUS_FREE) {
            break;
        }
    }

    if (vmngh_dev_id_check(dev_id, vfid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    *fid = vfid;

    return VMNG_OK;
}

STATIC int vmngh_alloc_vfid_static(u32 dev_id, u32 fid)
{
    if (vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.status != VMNG_VDEV_STATUS_FREE) {
        vmng_err("fid already in used. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

int vmngh_alloc_vfid(u32 dev_id, u32 *fid)
{
    int ret;

    if (*fid >= VMNG_VDEV_MAX_PER_PDEV) {
        vmng_err("Invalid fid. (dev_id=%u;fid=%u)\n", dev_id, *fid);
        return VMNG_ERR;
    }

    if (*fid == 0) {
        ret = vmngh_alloc_vfid_dynamic(dev_id, fid);
    } else {
        ret = vmngh_alloc_vfid_static(dev_id, *fid);
    }

    if (ret != VMNG_OK) {
        vmng_err("Alloc_vfid err. (dev_id=%u;vfid=%u)\n", dev_id, *fid);
        return ret;
    }

    vmngh_get_ctrl(dev_id, *fid)->vdev_ctrl.status = VMNG_VDEV_STATUS_ALLOC;
    return 0;
}

void vmngh_free_vdev_ctrl(u32 dev_id, u32 vfid)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, vfid);
    ka_mutex_t *ctrl_mutex = vmngh_get_ctrl_mutex();

    ka_task_mutex_lock(ctrl_mutex);
    (void)memset_s(&ctrl->memory, sizeof(struct vmng_vf_memory_info), 0, sizeof(struct vmng_vf_memory_info));
    ctrl->vdev_ctrl.dev_id = 0;
    ctrl->vdev_ctrl.vfid = 0;
    ctrl->vdev_ctrl.dtype = 0;
    ctrl->vdev_ctrl.core_num = 0;
    ctrl->vdev_ctrl.total_core_num = 0;
    ctrl->vdev_ctrl.status = VMNG_VDEV_STATUS_FREE;
    ka_task_mutex_unlock(ctrl_mutex);
}

/* get unit by id */
struct vmngh_vd_dev *vmngh_get_top_half_vdev_by_id(u32 dev_id, u32 fid)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    struct vmngh_vdev_ctrl *ctrl = NULL;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return NULL;
    }

    ctrl = vmngh_get_ctrl(dev_id, fid);
    if (((ctrl->startup_flag == VMNG_STARTUP_TOP_HALF_OK) || (ctrl->startup_flag == VMNG_STARTUP_BOTTOM_HALF_OK)) &&
        (ctrl->vdev_ctrl.dev_id == dev_id) && (ctrl->vdev_ctrl.vfid == fid)) {
        vd_dev = (struct vmngh_vd_dev *)ctrl->vd_dev;
    }

    return vd_dev;
}

struct vmngh_vd_dev *vmngh_get_bottom_half_vdev_by_id(u32 dev_id, u32 fid)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    struct vmngh_vdev_ctrl *ctrl = NULL;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return NULL;
    }

    ctrl = vmngh_get_ctrl(dev_id, fid);
    if ((ctrl->startup_flag == VMNG_STARTUP_BOTTOM_HALF_OK) && (ctrl->vdev_ctrl.dev_id == dev_id) &&
        (ctrl->vdev_ctrl.vfid == fid)) {
        vd_dev = (struct vmngh_vd_dev *)ctrl->vd_dev;
    }

    return vd_dev;
}

STATIC int vmngh_bw_get_vfio_barspace(u32 dev_id, u32 vfid, struct vf_bandwidth_ctrl_remote **vfio_base)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;

    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vmngh_pdev == NULL) {
        vmng_err("Get pdev from unit failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (vmngh_pdev->bw_ctrl.io_base_bwctrl == NULL) {
        vmng_err("io_base_bwctrl is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *vfio_base = vmngh_pdev->bw_ctrl.io_base_bwctrl + (vfid - VMNG_VDEV_FIRST_VFID);

    return 0;
}

STATIC int vmngh_bw_get_local_data(u32 dev_id, u32 vfid, u64 *flow_limit, u64 *pack_limit,
    struct vf_bandwidth_ctrl_local **data_ptr)
{
    struct vmngh_pci_dev *vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);

    if (vmngh_pdev == NULL) {
        vmng_err("vmngh_pdev is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (vfid < VMNG_VDEV_FIRST_VFID) {
        vmng_err("vfid is illegal. (dev_id=%u; fid=%u)\n", dev_id, vfid);
        return -EINVAL;
    }

    *flow_limit = vmngh_pdev->bw_ctrl.flow_limit[vfid - VMNG_VDEV_FIRST_VFID];
    *pack_limit = vmngh_pdev->bw_ctrl.pack_limit[vfid - VMNG_VDEV_FIRST_VFID];
    *data_ptr = &vmngh_pdev->bw_ctrl.local_data[vfid - VMNG_VDEV_FIRST_VFID];

    return 0;
}

STATIC void vmngh_bw_clear_remote_hostcpu_data(u32 dev_id, u32 vfid)
{
    size_t local_size = sizeof(struct vf_bandwidth_ctrl_local);
    struct vf_bandwidth_ctrl_remote* vf_ptr = NULL;
    struct vf_bandwidth_ctrl_local* local = NULL;
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    unsigned long flags;

    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);
    if ((vmngh_pdev == NULL) || (vmngh_pdev->bw_ctrl.io_base_bwctrl == NULL)) {
        return;
    }

    vf_ptr = vmngh_pdev->bw_ctrl.io_base_bwctrl + (vfid - VMNG_VDEV_FIRST_VFID);
    local = &vmngh_pdev->bw_ctrl.local_data[vfid - VMNG_VDEV_FIRST_VFID];

    ka_task_spin_lock_irqsave(&g_clear_timer.bandwidth_update_lock, flags);
    vf_ptr->hostcpu_flow_cnt[VMNG_PCIE_FLOW_H2D] = 0;
    vf_ptr->hostcpu_flow_cnt[VMNG_PCIE_FLOW_D2H] = 0;
    vf_ptr->hostcpu_pack_cnt[VMNG_PCIE_FLOW_H2D] = 0;
    vf_ptr->hostcpu_pack_cnt[VMNG_PCIE_FLOW_D2H] = 0;
    (void)memset_s((void *)local, local_size, 0, local_size);
    ka_task_spin_unlock_irqrestore(&g_clear_timer.bandwidth_update_lock, flags);
}

STATIC ka_hrtimer_restart_t vmngh_bw_data_clear_event(ka_hrtimer_t *t)
{
    u32 dev_id, vfid;

    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        if (vmngh_get_device_status(dev_id) != VMNG_VALID) {
            continue;
        }
        for (vfid = VMNG_VDEV_FIRST_VFID; vfid < VMNG_VDEV_MAX_PER_PDEV; vfid++) {
            vmngh_bw_clear_remote_hostcpu_data(dev_id, vfid);
        }
    }

    ka_system_hrtimer_forward_now(&g_clear_timer.timer, g_clear_timer.kt);
    return KA_HRTIMER_RESTART;
}

void vmngh_bw_data_clear_timer_uninit(void)
{
    ka_task_mutex_lock(&g_vmngh_ctrl_mutex);
    if (g_clear_timer.vaild_dev <= 0) {
        ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);
        return;
    }

    g_clear_timer.vaild_dev--;
    if (g_clear_timer.vaild_dev > 0) {
        ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);
        return;
    }
    ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);

    ka_system_hrtimer_cancel(&g_clear_timer.timer);

    vmng_info("used bandwidth clear timer canceled.\n");
}

void vmngh_bw_data_clear_timer_init(u32 dev_id, u32 vfid)
{
    if (devdrv_is_sriov_support(dev_id)) {
        return;
    }
    vmngh_bw_clear_remote_hostcpu_data(dev_id, vfid);

    ka_task_mutex_lock(&g_vmngh_ctrl_mutex);
    g_clear_timer.vaild_dev++;
    if (g_clear_timer.vaild_dev > 1) {
        ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);
        return;
    }
    ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);

    ka_task_spin_lock_init(&g_clear_timer.bandwidth_update_lock);

    ka_system_hrtimer_init(&g_clear_timer.timer, KA_CLOCK_MONOTONIC, KA_HRTIMER_MODE_REL);
    ka_system_set_hrtimer_func(&g_clear_timer.timer, vmngh_bw_data_clear_event);
    g_clear_timer.kt = ka_system_ktime_set(1, 0);
    ka_system_hrtimer_start(&g_clear_timer.timer, g_clear_timer.kt, KA_HRTIMER_MODE_REL);

    vmng_info("used bandwidth clear timer init finish.\n");
}

void vmngh_bw_ctrl_info_init(u32 dev_id)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    u64 raw_bandwidth = 0;
    u64 raw_packspeed = 0;
    size_t size = 0;
    u32 bandwidth_ratio = 0;
    u32 packspeed_ratio = 0;
    u64 addr = 0;
    int ret;

    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vmngh_pdev == NULL) {
        vmng_err("Call vmngh_get_pdev_from_unit failed. (dev_id=%u)\n", dev_id);
        return;
    }

    ret = devdrv_get_theoretical_capability(dev_id, &raw_bandwidth, &raw_packspeed);
    if (ret != 0) {
        vmng_warn("Incorrect bandwidth match. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return;
    }

    ret = devdrv_get_real_capability_ratio(dev_id, &bandwidth_ratio, &packspeed_ratio);
    if (ret != 0) {
        vmng_warn("Incorrect ratio match. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return;
    }

    vmngh_pdev->bw_ctrl.bandwidth = raw_bandwidth * bandwidth_ratio / VMNG_BANDW_PERCENTAGE_BASE;
    vmngh_pdev->bw_ctrl.packspeed = raw_packspeed * packspeed_ratio / VMNG_BANDW_PERCENTAGE_BASE;

    ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_VF_BANDWIDTH_BASE, 0, &addr, &size);
    if ((ret != 0) || (addr == 0) || (size == 0)) {
        vmng_err("Get address information failed. (dev_id=%u; ret=%d).\n", dev_id, ret);
        return;
    }

    vmngh_pdev->bw_ctrl.io_base_bwctrl =
        (struct vf_bandwidth_ctrl_remote*)ka_base_devm_ioremap(vmngh_pdev->dev, addr, size);
    if (vmngh_pdev->bw_ctrl.io_base_bwctrl == NULL) {
        vmng_err("Bandwidth ctrl ka_mm_ioremap failed. (dev_id=%u)\n", dev_id);
        return;
    }
}

void vmngh_bw_ctrl_info_uninit(u32 dev_id)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;

    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vmngh_pdev == NULL) {
        vmng_err("Call vmngh_get_pdev_from_unit failed. (dev_id=%u)\n", dev_id);
        return;
    }

    if (vmngh_pdev->bw_ctrl.io_base_bwctrl != NULL) {
        ka_base_devm_iounmap(vmngh_pdev->dev, vmngh_pdev->bw_ctrl.io_base_bwctrl);
        vmngh_pdev->bw_ctrl.io_base_bwctrl = NULL;
    }

    vmngh_pdev->bw_ctrl.bandwidth = 0;
    vmngh_pdev->bw_ctrl.packspeed = 0;
}

STATIC int vmngh_get_map_info_handle(struct vmngh_client_instance *instance,
    const struct vmngh_vascend_client *client, struct vmngh_map_info *map_info)
{
    int ret;
    u32 dev_id = instance->dev_ctrl->dev_id;
    u32 fid = instance->dev_ctrl->vfid;

    /* if client have not been registered. */
    if ((client == NULL) || (client->get_map_info == NULL)) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    vmng_info("Get map info begin. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client->type);
    ret = client->get_map_info(instance, map_info);
    if (ret != 0) {
        vmng_err("Get map info failed. (dev_id=%u; fid=%u; client_type=%u; ret=%d)\n", dev_id, fid, client->type, ret);
        return ret;
    }

    vmng_info("Client Get map info success. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client->type);
    return 0;
}

STATIC int vmngh_put_map_info_handle(struct vmngh_client_instance *instance,
    const struct vmngh_vascend_client *client)
{
    int ret;
    u32 dev_id = instance->dev_ctrl->dev_id;
    u32 fid = instance->dev_ctrl->vfid;

    /* if client have not been registered. */
    if ((client == NULL) || (client->put_map_info == NULL)) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    vmng_info("Put map info begin. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client->type);
    ret = client->put_map_info(instance);
    if (ret != 0) {
        vmng_err("Put map info failed. (dev_id=%u; fid=%u; ret=%d; client_type=%u)\n", dev_id, fid, ret, client->type);
        return ret;
    }

    vmng_info("Client put map info success. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client->type);
    return 0;
}

STATIC int vmngh_init_instance_proc(u32 dev_id, u32 fid, struct vmngh_client_instance *instance,
    const struct vmngh_client *client)
{
    int ret;
    u32 vdev_type = instance->vdev_type;

    /* if client have not been registered. */
    if (client == NULL) {
        vmng_info("Input client is NULL. (dev_id=%u;fid=%u;vdev_type=%u)\n", dev_id, fid, vdev_type);
        return 0;
    }

    if ((vdev_type != VMNGH_VM) && (vdev_type != VMNGH_CONTAINER)) {
        return 0;
    }

    if ((client->init_instance == NULL) && (vdev_type == VMNGH_VM)) {
        vmng_info("init_instance is NULL. (dev_id=%u;fid=%u;client_type=%u)\n", dev_id, fid, client->type);
        return 0;
    }

    if ((client->init_container_instance == NULL) && (vdev_type == VMNGH_CONTAINER)) {
        vmng_info("init_container_instance is NULL. (dev_id=%u;fid=%u;client_type=%u)\n", dev_id, fid, client->type);
        return 0;
    }
    ka_task_mutex_lock(&instance->flag_mutex);
    if (instance->flag == VMNG_INSTANCE_FLAG_UNINIT) {
        instance->flag = VMNG_INSTANCE_FLAG_INIT;
        ka_task_mutex_unlock(&instance->flag_mutex);
        vmng_info("Init instance begin. (dev_id=%u;fid=%u;client_type=%u;vdev_type=%u;vdev_id=%u)\n", dev_id, fid,
            client->type, vdev_type, instance->dev_ctrl->dev_id);
        if (vdev_type == VMNGH_VM) {
            ret = client->init_instance(instance);
        } else {
            ret = client->init_container_instance(instance);
        }
        if (ret != 0) {
            ka_task_mutex_lock(&instance->flag_mutex);
            instance->flag = VMNG_INSTANCE_FLAG_UNINIT;
            ka_task_mutex_unlock(&instance->flag_mutex);
            vmng_err("Init failed. (dev_id=%u;fid=%u;client_type=%u;ret=%d;vdev_id=%u)\n", dev_id, fid,
                client->type, ret, instance->dev_ctrl->dev_id);
            return ret;
        }
        vmng_info("Client init success. (dev_id=%u;fid=%u;client_type=%u;vdev_id=%u)\n", dev_id, fid, client->type,
            instance->dev_ctrl->dev_id);
    } else {
        ka_task_mutex_unlock(&instance->flag_mutex);
    }

    return 0;
}

STATIC int vmngh_uninit_instance_proc(u32 dev_id, u32 fid, struct vmngh_client_instance *instance,
    const struct vmngh_client *client)
{
    int ret = 0;
    u32 vdev_type = instance->vdev_type;

    /* if client have not been registered. */
    if (client == NULL) {
        vmng_debug("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return 0;
    }

    if ((vdev_type != VMNGH_VM) && (vdev_type != VMNGH_CONTAINER)) {
        return 0;
    }

    if (instance->dev_ctrl != NULL) {
        if ((client->uninit_instance == NULL) && (vdev_type == VMNGH_VM)) {
            vmng_info("Client is no need to uninit instance. (dev_id=%u; fid=%u; client_type=%u)\n",
                      dev_id, fid, client->type);
            return 0;
        }

        if ((client->uninit_container_instance == NULL) && (vdev_type == VMNGH_CONTAINER)) {
            vmng_info("Client is no need to uninit container instance. (dev_id=%u; fid=%u; client_type=%u)\n",
                      dev_id, fid, client->type);
            return 0;
        }
        ka_task_mutex_lock(&instance->flag_mutex);
        instance->flag = VMNG_INSTANCE_FLAG_UNINIT;
        ka_task_mutex_unlock(&instance->flag_mutex);
        vmng_info("Uninit begin. (dev_id=%u; fid=%u; client_type=%u; vdev_type=%u)\n", dev_id, fid, client->type,
            instance->vdev_type);
        if (vdev_type == VMNGH_VM) {
            ret = client->uninit_instance(instance);
        } else {
            ret = client->uninit_container_instance(instance);
        }
        if (ret != 0) {
            vmng_err("Uninit failed. (dev_id=%u; fid=%u; client_type=%u; ret=%d) \n", dev_id, fid, client->type, ret);
        }
        instance->dev_ctrl = NULL;
        instance->priv = NULL;
        vmng_info("Uninit success. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client->type);
    }

    return ret;
}

STATIC int vmngh_suspend_instance_proc(u32 dev_id, u32 fid, u32 type, struct vmngh_client_instance *instance,
    const struct vmngh_client *client)
{
    int ret;

    if (client == NULL) {
        vmng_debug("Input parameter is error. (dev_id=%u; fid=%u; client=%u)\n", dev_id, fid, type);
        return 0;
    }
    if (instance->dev_ctrl != NULL) {
        if (client->suspend == NULL) {
            vmng_debug("Client is no need to suspend. (dev_id=%u; fid=%u; client_type=%u)\n",
                       dev_id, fid, client->type);
            return 0;
        }
        vmng_info("Client suspend begin. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client->type);
        ret = client->suspend(instance);
        if (ret != 0) {
            vmng_err("Client suspend failed. (dev_id=%u; fid=%u; client_type=%u; ret=%d)\n",
                     dev_id, fid, client->type, ret);
            return ret;
        }
        vmng_info("Client suspend success. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client->type);
    }

    return 0;
}

/* call before init instance. */
int vmngh_get_map_info_client(struct vmngh_client_instance *instance_para,
    enum vmng_client_type client_type, struct vmngh_map_info *client_map_info)
{
    u32 dev_id, fid;
    int ret;

    if ((instance_para == NULL) || (instance_para->dev_ctrl == NULL) || (client_map_info == NULL)) {
        vmng_err("Parameter check failed. (instance_para=%s; proc_data=%s)\n",
                 instance_para == NULL ? "NULL" : "OK", client_map_info == NULL ? "NULL" : "OK");
        return -EINVAL;
    }

    dev_id = instance_para->dev_ctrl->dev_id;
    fid = instance_para->dev_ctrl->vfid;
    if ((vmngh_dev_id_check(dev_id, fid) != 0) || (client_type < 0) ||
        (client_type >= VMNG_CLIENT_TYPE_MAX)) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client_type);
        return -EINVAL;
    }

    ret = vmngh_get_map_info_handle(instance_para, g_vmngh_vascend_clients[client_type], client_map_info);
    if (ret != 0) {
        vmng_err("Client get map info failed. (dev_id=%u; fid=%u; client_type=%u; ret=%d)\n",
                 dev_id, fid, client_type, ret);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

int vmngh_put_map_info_client(struct vmngh_client_instance *instance_para,
    enum vmng_client_type client_type)
{
    u32 dev_id, fid;
    int ret;

    if ((instance_para == NULL) || (instance_para->dev_ctrl == NULL)) {
        vmng_err("Parameter check failed. (instance_para=%s)\n",
                 instance_para == NULL ? "NULL" : "OK");
        return -EINVAL;
    }

    dev_id = instance_para->dev_ctrl->dev_id;
    fid = instance_para->dev_ctrl->vfid;
    if ((vmngh_dev_id_check(dev_id, fid) != 0) || (client_type < 0) ||
        (client_type >= VMNG_CLIENT_TYPE_MAX)) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; client_type=%u)\n", dev_id, fid, client_type);
        return -EINVAL;
    }

    ret = vmngh_put_map_info_handle(instance_para, g_vmngh_vascend_clients[client_type]);
    if (ret != 0) {
        vmng_err("Client put map info failed. (dev_id=%u; fid=%u; client_type=%u; ret=%d)\n",
                 dev_id, fid, client_type, ret);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

/* call after probe finish. */
int vmngh_init_instance_all_client(u32 dev_id, u32 fid, u32 vdev_type)
{
    struct vmngh_client_instance *instance = NULL;
    int type;
    int ret, i;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    for (type = 0; type < (int)VMNG_CLIENT_TYPE_MAX; type++) {
        instance = vmngh_get_client_instance(dev_id, fid, type);
        instance->dev_ctrl = &vmngh_get_ctrl(dev_id, fid)->vdev_ctrl;
        instance->vdev_type = vdev_type;
        ret = vmngh_init_instance_proc(dev_id, fid, instance, g_vmngh_clients[type]);
        if (ret != 0) {
            vmng_err("Init instance failed. (dev_id=%u; fid=%u; client_type=%u; vdev_type=%u; ret=%d)\n",
                     dev_id, fid, type, vdev_type, ret);
            goto OUT;
        }
    }

    vmng_info("Init all client ok. (dev_id=%u; fid=%u; vdev_type=%u)\n", dev_id, fid, vdev_type);

    return VMNG_OK;

OUT:
    for (i = 0; i < type; i++) {
        instance = vmngh_get_client_instance(dev_id, fid, i);
        if (vmngh_uninit_instance_proc(dev_id, fid, instance, g_vmngh_clients[i]) != 0) {
            vmng_err("Uninit instance failed. (dev_id=%u; fid=%u; client=%u; vdev_type=%u;)\n",
                dev_id, fid, i, instance->vdev_type);
            continue;
        }
    }

    return ret;
}

/* call when vd_dev remove. */
int vmngh_uninit_instance_all_client(u32 dev_id, u32 fid)
{
    struct vmngh_client_instance *instance = NULL;
    int type;
    int ret;

    for (type = 0; type < (int)VMNG_CLIENT_TYPE_MAX; type++) {
        instance = vmngh_get_client_instance(dev_id, fid, type);
        ret = vmngh_uninit_instance_proc(dev_id, fid, instance, g_vmngh_clients[type]);
        if (ret != 0) {
            vmng_err("Uninit instance failed. (dev_id=%u; fid=%u; client=%u; vdev_type=%u; ret=%d)\n", dev_id, fid,
                type, instance->vdev_type, ret);
            continue;
        }
    }
    vmng_info("Uninit all client ok. (dev_id=%u; fid=%u)\n", dev_id, fid);
    return 0;
}

STATIC int vmngh_suspend_instance_all_client(u32 dev_id, u32 fid)
{
    struct vmngh_client_instance *instance = NULL;
    u32 type;
    int ret;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    for (type = 0; type < (u32)VMNG_CLIENT_TYPE_MAX; type++) {
        instance = vmngh_get_client_instance(dev_id, fid, type);
        ret = vmngh_suspend_instance_proc(dev_id, fid, type, instance, g_vmngh_clients[type]);
        if (ret != 0) {
            vmng_warn("Client suspend incomplete. (dev_id=%u; fid=%u; client=%u; ret=%d)\n", dev_id, fid, type, ret);
            continue;
        }
    }
    vmng_info("Suspend all client ok. (dev_id=%u; fid=%u)\n", dev_id, fid);
    return 0;
}

/* when register call init if device is ok. */
STATIC int vmngh_init_instance_all_dev(u32 type, struct vmngh_client *client)
{
    int ret;
    struct vmngh_client_instance *instance = NULL;
    u32 dev_id;
    u32 fid;

    if (type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client=%u)\n", type);
        return -EINVAL;
    }
    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        for (fid = 0; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
            if (vmngh_get_ctrl(dev_id, fid)->startup_flag != VMNG_STARTUP_BOTTOM_HALF_OK) {
                continue;
            }
            instance = vmngh_get_client_instance(dev_id, fid, type);
            instance->dev_ctrl = &vmngh_get_ctrl(dev_id, fid)->vdev_ctrl;
            ret = vmngh_init_instance_proc(dev_id, fid, instance, client);
            if (ret != 0) {
                vmng_err("Init instance failed. (dev_id=%u; fid=%u; client=%u; ret=%d)\n", dev_id, fid, type, ret);
                continue;
            }
        }
    }
    return 0;
}

STATIC int vmngh_uninit_instance_all_dev(u32 type, struct vmngh_client *client)
{
    int ret;
    struct vmngh_client_instance *instance = NULL;
    u32 dev_id, fid;

    if (type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client=%u)\n", type);
        return -EINVAL;
    }
    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        for (fid = 0; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
            instance = vmngh_get_client_instance(dev_id, fid, type);
            ret = vmngh_uninit_instance_proc(dev_id, fid, instance, client);
            if (ret != 0) {
                vmng_err("Uninit instance failed. (dev_id=%u; fid=%u; client=%u; ret=%d)\n", dev_id, fid, type, ret);
                continue;
            }
        }
    }
    return 0;
}

int vmngh_init_instance_after_probe(u32 dev_id, u32 fid)
{
    int ret;

    ret = vmngh_init_instance_all_client(dev_id, fid, VMNGH_VM);
    if (ret != 0) {
        vmng_err("Init all client error. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        return ret;
    }
    return 0;
}

int vmngh_uninit_instance_remove_vdev(u32 dev_id, u32 fid)
{
    int ret;

    ret = vmngh_uninit_instance_all_client(dev_id, fid);
    if (ret != 0) {
        vmng_err("Uninit all client error. (dev_id=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        return ret;
    }

    return 0;
}

void vmngh_suspend_instance_remove_vdev(u32 dev_id, u32 fid)
{
    int ret;

    ret = vmngh_suspend_instance_all_client(dev_id, fid);
    if (ret != 0) {
        vmng_warn("Suspend all client incomplete. (dev=%u;fid=%u;ret=%d)\n", dev_id, fid, ret);
    }
}

void vmngh_suspend_instance_remove_pdev(u32 dev_id)
{
    u32 fid;
    int ret;
    u32 type;
    struct vmngh_client_instance *instance = NULL;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return;
    }
    for (fid = 1; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
        for (type = 0; type < (u32)VMNG_CLIENT_TYPE_MAX; type++) {
            instance = vmngh_get_client_instance(dev_id, fid, type);
            ret = vmngh_suspend_instance_proc(dev_id, fid, type, instance, g_vmngh_clients[type]);
            if (ret != 0) {
                vmng_warn("Client suspend incomplete. (dev_id=%u;fid=%u;client=%u;ret=%d)\n", dev_id, fid, type, ret);
                continue;
            }
        }
    }
    vmng_info("Suspend all vdev client ok. (dev_id=%u)\n", dev_id);
}

void vmngh_host_stop(u32 dev_id, u32 fid)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return;
    }

    if (vmngh_get_ctrl(dev_id, fid)->startup_flag == VMNG_STARTUP_BOTTOM_HALF_OK) {
        vmngh_suspend_instance_all_client(dev_id, fid);
    }

    vmngh_free_vdev_host_stop(dev_id, fid);

    vmng_info("Host stop. (dev_id=%u; fid=%u)", dev_id, fid);
}

int vmngh_register_client(struct vmngh_client *client)
{
    int ret;

    if (client == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (client->type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", client->type);
        return -EINVAL;
    }
    if (g_vmngh_clients[client->type] != NULL) {
        vmng_err("Client is already registered. (client_type=%u)\n", client->type);
        return -EALREADY;
    }

    ret = vmngh_init_instance_all_dev(client->type, client);
    if (ret != 0) {
        vmng_err("Init all device error. (client_type=%u; ret=%d)\n", client->type, ret);
        return ret;
    }
    g_vmngh_clients[client->type] = client;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_register_client);

int vmngh_unregister_client(struct vmngh_client *client)
{
    int ret;

    if (client == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (client->type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", client->type);
        return -EINVAL;
    }

    ret = vmngh_uninit_instance_all_dev(client->type, client);
    if (ret != 0) {
        vmng_err("Uinit all device error. (client_type=%u; ret=%d)\n", client->type, ret);
        return ret;
    }
    g_vmngh_clients[client->type] = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_unregister_client);

int vmngh_register_vascend_client(struct vmngh_vascend_client *client)
{
    if (client == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (client->type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", client->type);
        return -EINVAL;
    }
    if (g_vmngh_vascend_clients[client->type] != NULL) {
        vmng_err("Client is already registered. (client_type=%u)\n", client->type);
        return -EALREADY;
    }

    g_vmngh_vascend_clients[client->type] = client;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_register_vascend_client);

int vmngh_unregister_vascend_client(struct vmngh_vascend_client *client)
{
    if (client == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (client->type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", client->type);
        return -EINVAL;
    }

    g_vmngh_vascend_clients[client->type] = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_unregister_vascend_client);

void vmngh_ctrl_set_startup_flag(u32 dev_id, u32 fid, enum vmng_startup_flag_type flag)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return;
    }

    ka_task_mutex_lock(&g_vmngh_ctrl_mutex);
    vmngh_get_ctrl(dev_id, fid)->startup_flag = flag;
    ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);
}

int vmngh_get_ctrl_startup_flag(u32 dev_id, u32 fid)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    return vmngh_get_ctrl(dev_id, fid)->startup_flag;
}

void vmngh_ctrl_register_half(struct vmngh_vd_dev *vd_dev)
{
    vmngh_ctrl_set_startup_flag(vd_dev->dev_id, vd_dev->fid, VMNG_STARTUP_BOTTOM_HALF_OK);
}

void vmngh_ctrl_unregister_half(struct vmngh_vd_dev *vd_dev)
{
    vmngh_ctrl_set_startup_flag(vd_dev->dev_id, vd_dev->fid, VMNG_STARTUP_TOP_HALF_OK);
}

u32 vmngh_get_total_core_num(u32 dev_id)
{
    return g_vmngh_device_ctrl[dev_id].total_core_num;
}

int vmngh_register_ctrls(struct vmngh_vd_dev *vd_dev)
{
    struct vmngh_vdev_ctrl *ctrl = NULL;
    struct vmngh_pci_dev *vm_pdev = NULL;
    u32 dev_id;
    u32 fid;

    if (vd_dev == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    dev_id = vd_dev->dev_id;
    fid = vd_dev->fid;
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    ctrl = vmngh_get_ctrl(dev_id, fid);
    vm_pdev = (struct vmngh_pci_dev *)vd_dev->vm_pdev;

    ka_task_mutex_lock(&g_vmngh_ctrl_mutex);
    ctrl->vdavinci = vd_dev->vdavinci;
    ctrl->vd_dev = vd_dev;
    if (devdrv_is_sriov_support(vd_dev->dev_id)) {
        devdrv_get_devid_by_pfvf_id(vd_dev->dev_id, vd_dev->fid, &ctrl->vdev_ctrl.dev_id);
        ctrl->vdev_ctrl.vfid = 0;
    } else {
        ctrl->vdev_ctrl.dev_id = dev_id;
        ctrl->vdev_ctrl.vfid = fid;
    }
    ctrl->vdev_ctrl.dtype = (u32)vd_dev->dtype.type;
    if (vm_pdev->vm_full_spec_enable) {
        ctrl->vdev_ctrl.core_num = vm_pdev->dev_info.aicore_num;
    } else {
        ctrl->vdev_ctrl.core_num = vd_dev->dtype.aicore_num;
    }
    ctrl->vdev_ctrl.total_core_num = vmngh_get_total_core_num(dev_id);
    ctrl->vdev_ctrl.ddr_size = vd_dev->dtype.ddrmem_size;
    ctrl->vdev_ctrl.hbm_size = vd_dev->dtype.hbmmem_size;
    ctrl->vdev_ctrl.status = VMNG_VDEV_STATUS_ALLOC;
    ctrl->startup_flag = VMNG_STARTUP_PROBED;
    ka_task_mutex_init(&ctrl->reset_mutex);
    ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);

    return 0;
}

void vmngh_unregister_ctrls(u32 dev_id, u32 fid)
{
    struct vmngh_vdev_ctrl *ctrl = NULL;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return;
    }

    ctrl = vmngh_get_ctrl(dev_id, fid);
    ka_task_mutex_lock(&g_vmngh_ctrl_mutex);
    ka_task_mutex_destroy(&ctrl->reset_mutex);
    ctrl->vdavinci = NULL;
    ctrl->vd_dev = NULL;
    ctrl->vdev_ctrl.dev_id = 0;
    ctrl->vdev_ctrl.vfid = 0;
    ctrl->vdev_ctrl.dtype = 0;
    ctrl->vdev_ctrl.core_num = 0;
    ctrl->vdev_ctrl.total_core_num = 0;
    ctrl->vdev_ctrl.ddr_size = 0;
    ctrl->vdev_ctrl.hbm_size = 0;
    ctrl->vdev_ctrl.status = VMNG_VDEV_STATUS_FREE;
    ctrl->startup_flag = VMNG_STARTUP_UNPROBED;
    ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);
}

int vmngh_init_ctrl(void)
{
    enum vmng_client_type type;
    u32 dev_id;
    u32 fid;
    u32 vm_id;

    if ((memset_s(g_vmngh_clients, sizeof(g_vmngh_clients), 0, sizeof(g_vmngh_clients)) != EOK) ||
        (memset_s(&g_clear_timer, sizeof(g_clear_timer), 0, sizeof(g_clear_timer)) != EOK) ||
        (memset_s(g_vmngh_vascend_clients, sizeof(g_vmngh_vascend_clients), 0,
            sizeof(g_vmngh_vascend_clients)) != EOK)) {
        vmng_err("Call memset_s failed.\n");
        return -EINVAL;
    }

    g_vmngh_device_ctrl = vmng_kzalloc(sizeof(struct vmngh_device_ctrl) * VMNG_PDEV_MAX, KA_GFP_KERNEL);
    if (g_vmngh_device_ctrl == NULL) {
        vmng_err("kzalloc g_vmngh_device_ctrl failed.\n");
        return -ENOMEM;
    }

    for (type = VMNG_CLIENT_TYPE_DEVMNG; type < VMNG_CLIENT_TYPE_MAX; type++) {
        for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
            for (fid = 0; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
                vmngh_get_client_instance(dev_id, fid, type)->type = type;
                vmngh_get_client_instance(dev_id, fid, type)->flag = VMNG_INSTANCE_FLAG_UNINIT;
                ka_task_mutex_init(&vmngh_get_client_instance(dev_id, fid, type)->flag_mutex);
            }
        }
    }

    ka_task_mutex_init(&g_vmngh_ctrl_mutex);

    /* VM id relative var init, set vmid of ctrls to default. */
    if (memset_s(g_vmngh_vm_ctrl, sizeof(g_vmngh_vm_ctrl), 0, sizeof(g_vmngh_vm_ctrl)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        vmng_kfree(g_vmngh_device_ctrl);
        g_vmngh_device_ctrl = NULL;
        return -EINVAL;
    }
    ka_task_mutex_init(&g_vmngh_vm_ctrl_mutex);
    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        for (fid = 0; fid < VMNG_VDEV_MAX_PER_PDEV; fid++) {
            vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.vm_id = (u32)VMNGH_VM_ID_DEFAULT;
        }
    }
    for (vm_id = 0; vm_id < VMNG_VM_MAX; vm_id++) {
        g_vmngh_vm_ctrl[vm_id].vm_id = (u32)VMNGH_VM_ID_DEFAULT;
    }

    return 0;
}

void vmngh_uninit_ctrl(void)
{
    vmng_kfree(g_vmngh_device_ctrl);
    g_vmngh_device_ctrl = NULL;
}

int vmngh_init_ctrl_ops(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (vmngh_res_drv_init(dev_id, vmngh_get_ctrl_ops(dev_id)) != VMNG_OK) {
        vmng_err("Res_drv_init error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

int vmngh_uninit_ctrl_ops(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    vmngh_res_drv_uninit(vmngh_get_ctrl_ops(dev_id));

    return VMNG_OK;
}


int vmngh_get_virtual_addr_info(u32 dev_id, u32 fid, enum vmng_get_addr_type type, u64 *addr, u64 *size)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    u64 mem_base;

    if (addr == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; mdev_id=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (size == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; mdev_id=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("Device is not ready. (dev_id=%u; mdev_id=%u)\n", dev_id, fid);
        return -ENODEV;
    }
    mem_base = (u64)(uintptr_t)(vd_dev->mm_res.mem_base);

    switch (type) {
        case VMNG_GET_ADDR_TYPE_TSDRV:
            *addr = mem_base + VMNG_BAR4_TSDRV_BASE;
            *size = (u64)(unsigned int)vd_dev->mm_res.mem_size;
            break;
        default:
            vmng_err("Unknown memory type. (dev_id=%u; fid=%u; mem_type=%u)\n", dev_id, fid, type);
            return -EINVAL;
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_get_virtual_addr_info);

ka_dma_addr_t vmngh_dma_map_guest_page(u32 dev_id, u32 fid, unsigned long addr, unsigned long size,
    ka_sg_table_t **dma_sgt)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    const u32 PA_TO_GFN_BITS = KA_MM_PAGE_SHIFT;
    int ret;

    if (dma_sgt == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; mdev_id=%u)\n", dev_id, fid);
        return DMA_MAP_ERROR;
    }

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("Device is not ready. (dev_id=%u; mdev_id=%u)\n", dev_id, fid);
        return DMA_MAP_ERROR;
    }

    ret = hw_dvt_hypervisor_dma_map_guest_page(vd_dev->vdavinci, addr >> PA_TO_GFN_BITS, size, dma_sgt);
    if (ret != 0) {
        vmng_err("Hypervisor dma map failed. (dev_id=%u; mdev_id=%u; ret=%d)\n", dev_id, fid, ret);
        return DMA_MAP_ERROR;
    }

    /* Adding offset in PAGE, in case of PM KA_MM_PAGE_SIZE is larger than VM */
    return (ka_dma_addr_t)(ka_mm_sg_dma_address((*dma_sgt)->sgl) + (addr & (KA_MM_PAGE_SIZE - 1)));
}
KA_EXPORT_SYMBOL(vmngh_dma_map_guest_page);

void vmngh_dma_unmap_guest_page(u32 dev_id, u32 fid, ka_sg_table_t *dma_sgt)
{
    struct vmngh_vd_dev *vd_dev = NULL;

    if (dma_sgt == NULL) {
        return;
    }

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("Device is not ready. (dev_id=%u; mdev_id=%u)\n", dev_id, fid);
        return;
    }

    hw_dvt_hypervisor_dma_unmap_guest_page(vd_dev->vdavinci, dma_sgt);
}
KA_EXPORT_SYMBOL(vmngh_dma_unmap_guest_page);

bool vmngh_dma_pool_active(u32 dev_id, u32 fid)
{
    struct vmngh_vd_dev *vd_dev = NULL;

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("dev %u mdev %u is not ready\n", dev_id, fid);
        return false;
    }

    return hw_dvt_hypervisor_dma_pool_active(vd_dev->vdavinci);
}
KA_EXPORT_SYMBOL(vmngh_dma_pool_active);

int vmngh_dma_map_guest_page_batch(u32 dev_id, u32 fid, unsigned long *gfn,
    unsigned long *dma_addr, unsigned long count)
{
    struct vmngh_vd_dev *vd_dev = NULL;

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("dev %u mdev %u is not ready\n", dev_id, fid);
        return -ENODEV;
    }

    return hw_dvt_hypervisor_dma_map_guest_page_batch(vd_dev->vdavinci, gfn, dma_addr, count);
}
KA_EXPORT_SYMBOL(vmngh_dma_map_guest_page_batch);

void vmngh_dma_unmap_guest_page_batch(u32 dev_id, u32 fid,
    unsigned long *gfn, unsigned long *dma_addr, unsigned long count)
{
    struct vmngh_vd_dev *vd_dev = NULL;

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("dev %u mdev %u is not ready\n", dev_id, fid);
        return;
    }

    hw_dvt_hypervisor_dma_unmap_guest_page_batch(vd_dev->vdavinci, gfn, dma_addr, count);
}
KA_EXPORT_SYMBOL(vmngh_dma_unmap_guest_page_batch);

int vmngh_alloc_vm_id(u32 dev_id, u32 fid, u32 vm_pid, u32 vm_devid)
{
    int i;
    int vm_id = VMNGH_VM_ID_DEFAULT;
    if ((vmngh_dev_id_check(dev_id, fid) != 0) || (vm_pid == 0)) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u; vm_pid=%u vm_devid=%u)\n", dev_id, fid, vm_pid, vm_devid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&g_vmngh_vm_ctrl_mutex);

    /* find pid match */
    for (i = 0; i < VMNG_VM_MAX; i++) {
        if (g_vmngh_vm_ctrl[i].vm_pid == vm_pid) {
            vm_id = i;
            if (g_vmngh_vm_ctrl[vm_id].pdev_cnt < VMNG_VM_MAX) {
                g_vmngh_vm_ctrl[vm_id].pdev_cnt++;
            } else {
                vmng_err("vm_devid is exceed MAX. (dev_id=%u; fid=%u; vm_pid=%u; vm_devid=%u)",
                         dev_id, fid, vm_pid, vm_devid);
                ka_task_mutex_unlock(&g_vmngh_vm_ctrl_mutex);
                return -EINVAL;
            }
            vmng_info("Find pid match. (dev_id=%u; fid=%u; vm_pid=%u; vm_id=%u; vm_devid=%u; pdev_num=%u)\n",
                dev_id, fid, vm_pid, vm_id, vm_devid, g_vmngh_vm_ctrl[vm_id].pdev_cnt);
            break;
        }
    }

    /* alloc new */
    if (vm_id == VMNGH_VM_ID_DEFAULT) {
        for (i = 0; i < VMNG_VM_MAX; i++) {
            if (g_vmngh_vm_ctrl[i].pdev_cnt == 0) {
                vm_id = i;
                g_vmngh_vm_ctrl[vm_id].pdev_cnt++;
                g_vmngh_vm_ctrl[vm_id].vm_pid = vm_pid;
                vmng_info("Alloc new vm_pid. (dev_id=%u; fid=%u; vm_pid=%u; vm_devid=%u; vm_id=%u)\n",
                    dev_id, fid, vm_pid, vm_id, vm_devid);
                break;
            }
        }
    }

    /* fail */
    if (vm_id == VMNGH_VM_ID_DEFAULT) {
        vmng_err("Alloc vm_id failed. (dev_id=%u; fid=%u; vm_pid=%u; vm_devid=%u)", dev_id, fid, vm_pid, vm_devid);
        ka_task_mutex_unlock(&g_vmngh_vm_ctrl_mutex);
        return -1;
    }

    /* set */
    g_vmngh_vm_ctrl[vm_id].vm_id = (u32)vm_id;
    g_vmngh_vm_ctrl[vm_id].vm_info[vm_devid].devid = dev_id;
    g_vmngh_vm_ctrl[vm_id].vm_info[vm_devid].fid = fid;
    g_vmngh_vm_ctrl[vm_id].vm_info[vm_devid].valid = VMNGH_VM_DEV_VALID;
    vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.vm_id = (u32)vm_id;
    vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.vm_devid = vm_devid;

    ka_task_mutex_unlock(&g_vmngh_vm_ctrl_mutex);
    return 0;
}

void vmngh_ctrl_rm_vm_id(u32 dev_id, u32 fid, u32 vm_devid)
{
    u32 rm_vm_id;

    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return;
    }

    ka_task_mutex_lock(&g_vmngh_vm_ctrl_mutex);

    rm_vm_id = vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.vm_id;
    if (rm_vm_id >= VMNG_VM_MAX) {
        ka_task_mutex_unlock(&g_vmngh_vm_ctrl_mutex);
        vmng_err("rm_vm_id is invalid. (dev_id=%u; fid=%u; vm_id=%u)\n", dev_id, fid, rm_vm_id);
        return;
    }

    if (g_vmngh_vm_ctrl[rm_vm_id].pdev_cnt > 0) {
        g_vmngh_vm_ctrl[rm_vm_id].pdev_cnt--;
        g_vmngh_vm_ctrl[rm_vm_id].vm_info[vm_devid].valid = VMNGH_VM_DEV_INVALID;
        g_vmngh_vm_ctrl[rm_vm_id].vm_info[vm_devid].devid = 0;
        g_vmngh_vm_ctrl[rm_vm_id].vm_info[vm_devid].fid = 0;
        if (g_vmngh_vm_ctrl[rm_vm_id].pdev_cnt == 0) {
            vmng_info("vm_id is removed. (dev_id=%u; fid=%u; pid=%u; vm_id=%u)\n",
                      dev_id, fid, g_vmngh_vm_ctrl[rm_vm_id].vm_pid, rm_vm_id);
            g_vmngh_vm_ctrl[rm_vm_id].vm_pid = 0;
        }
    } else {
        vmng_err("vm_id cnt is zero, but call removed. (dev_id=%u; fid=%u; pid=%u; vm_id=%u)\n", dev_id, fid,
            g_vmngh_vm_ctrl[rm_vm_id].vm_pid, rm_vm_id);
    }

    vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.vm_id = (u32)VMNGH_VM_ID_DEFAULT;
    ka_task_mutex_unlock(&g_vmngh_vm_ctrl_mutex);
}

int vmngh_ctrl_get_vm_id(u32 dev_id, u32 fid)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return VMNGH_VM_ID_DEFAULT;
    }
    if (vmngh_get_ctrl(dev_id, fid)->startup_flag != VMNG_STARTUP_BOTTOM_HALF_OK) {
        vmng_info("startup_flag is not half ok. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return VMNGH_VM_ID_DEFAULT;
    }

    return (int)vmngh_get_ctrl(dev_id, fid)->vdev_ctrl.vm_id;
}
KA_EXPORT_SYMBOL(vmngh_ctrl_get_vm_id);

int vmngh_ctrl_get_devid_fid(u32 vm_id, u32 vm_devid, u32 *dev_id, u32 *fid)
{
    if (vmngh_vm_id_check(vm_id, vm_devid) != 0) {
        vmng_err("Parameter check failed. (vm_id=%u; vm_devid=%u)\n", vm_id, vm_devid);
        return -EINVAL;
    }
    if ((dev_id == NULL) || (fid == NULL)) {
        vmng_err("Input parameter is error. (vm_id=%u; vm_devid=%u)\n", vm_id, vm_devid);
        return -EINVAL;
    }
    if (g_vmngh_vm_ctrl[vm_id].vm_info[vm_devid].valid == VMNGH_VM_DEV_VALID) {
        *dev_id = g_vmngh_vm_ctrl[vm_id].vm_info[vm_devid].devid;
        *fid = g_vmngh_vm_ctrl[vm_id].vm_info[vm_devid].fid;
        return 0;
    }

    vmng_err("No devid fid. (vm_id=%u; vm_devid=%u)\n", vm_id, vm_devid);
    return -EINVAL;
}
KA_EXPORT_SYMBOL(vmngh_ctrl_get_devid_fid);

void vmngh_set_dev_info(u32 dev_id, enum vmngh_dev_info_type type, u64 val)
{
    struct vmngh_pci_dev *vm_pdev = NULL;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (devid=%u)\n", dev_id);
        return;
    }

    vm_pdev = vmngh_get_pdev_from_unit(dev_id);
    if (vm_pdev == NULL) {
        vmng_err("Call vmngh_get_pdev_from_unit failed. (dev_id=%u)\n", dev_id);
        return;
    }

    switch (type) {
        case VMNGH_DEV_CORE_NUM:
            vm_pdev->dev_info.aicore_num = (u32)val;
            vmng_info("Set aicore information. (dev_id=%u)\n", dev_id);
            break;
        case VMNGH_DEV_DDR_SIZE:
            vm_pdev->dev_info.ddrmem_size = val;
            vmng_info("Set ddrmem information. (dev_id=%u)\n", dev_id);
            break;
        case VMNGH_DEV_HBM_SIZE:
            vm_pdev->dev_info.hbmmem_size = val;
            vmng_info("Set hbmmem information. (dev_id=%u)\n", dev_id);
            break;
        default:
            vmng_err("Type is error. (dev_id=%u)\n", dev_id);
            break;
    }

    return;
}
KA_EXPORT_SYMBOL(vmngh_set_dev_info);

void vmngh_set_total_core_num(u32 dev_id, u32 total_core_num)
{
    u32 i;
    struct vmng_vdev_ctrl *ctrl = NULL;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return;
    }

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return;
    }

    ka_task_mutex_lock(&g_vmngh_ctrl_mutex);
    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        ctrl = &vmngh_get_ctrl(dev_id, i)->vdev_ctrl;
        ctrl->total_core_num = total_core_num;
    }

    vmngh_set_dev_info(dev_id, VMNGH_DEV_CORE_NUM, total_core_num);
    ka_task_mutex_unlock(&g_vmngh_ctrl_mutex);

    g_vmngh_device_ctrl[dev_id].total_core_num = total_core_num;
    vmng_info("Get total_core_num. (dev_id=%u; total_core_num=%u)\n", dev_id, total_core_num);
}
KA_EXPORT_SYMBOL(vmngh_set_total_core_num);

int vmngh_init_instance_client_device(u32 dev_id, u32 vfid)
{
    struct vmng_ctrl_msg_info info;
    struct vmngh_vdev_ctrl *ctrl;
    int ret;

    if (memset_s(&info, sizeof(struct vmng_ctrl_msg_info), 0, sizeof(struct vmng_ctrl_msg_info)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    ctrl = vmngh_get_ctrl(dev_id, vfid);
    info.dev_id = dev_id;
    info.vfid = vfid;
    info.dtype = ctrl->vdev_ctrl.dtype;
    info.core_num = ctrl->vdev_ctrl.core_num;
    info.total_core_num = ctrl->vdev_ctrl.total_core_num;

    ret = vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_INIT_CLIENT);
    if (ret != VMNG_OK) {
        vmng_err("Send msg failed. (dev_id=%d; vfid=%d)\n", info.dev_id, info.vfid);
        return ret;
    }

    return VMNG_OK;
}

int vmngh_uninit_instance_client_device(u32 dev_id, u32 vfid)
{
    struct vmng_ctrl_msg_info info;
    int ret;

    if (memset_s(&info, sizeof(struct vmng_ctrl_msg_info), 0, sizeof(struct vmng_ctrl_msg_info)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    info.dev_id = dev_id;
    info.vfid = vfid;
    ret = vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_UNINIT_CLIENT);
    if (ret != VMNG_OK) {
        vmng_err("Send msg failed. (dev_id=%d; vfid=%d)\n", dev_id, info.vfid);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

static int vmngh_check_create_container_vdev_parameter(u32 dev_id, u32 dtype, const u32 *vfid)
{
    if (vfid == NULL) {
        vmng_err("Input parameter is error.\n");
        return VMNG_ERR;
    }

    if ((dev_id >= VMNG_PDEV_MAX) || (dtype >= HW_VDAVINCI_TYPE_MAX)) {
        vmng_err("Input parameter is error. (dev_id=%u; dtype=%u)\n", dev_id, dtype);
        return VMNG_ERR;
    }

    if (vmngh_get_device_status(dev_id) != VMNG_VALID) {
        vmng_err("Device is not ready. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }
    return VMNG_OK;
}

static inline int vmng_ops_alloc_vf(struct vmngh_ctrl_ops *ops, u32 dev_id, u32 *fid, u32 dtype,
                                    struct vmng_vf_res_info *vf_resource)
{
    if (ops->alloc_vf == NULL) {
        return 0;
    }
    return ops->alloc_vf(dev_id, fid, dtype, vf_resource);
}

static inline int vmng_ops_free_vf(struct vmngh_ctrl_ops *ops, u32 dev_id, u32 vfid)
{
    if (ops->free_vf == NULL) {
        return 0;
    }
    return ops->free_vf(dev_id, vfid);
}

static inline int vmng_ops_alloc_vfid(struct vmngh_ctrl_ops *ops, u32 dev_id, u32 dtype, u32 *fid)
{
    if (ops->alloc_vfid == NULL) {
        return 0;
    }
    return ops->alloc_vfid(dev_id, dtype, fid);
}

static inline void vmng_ops_free_vfid(struct vmngh_ctrl_ops *ops, u32 dev_id, u32 vfid)
{
    if (ops->free_vfid == NULL) {
        return;
    }
    ops->free_vfid(dev_id, vfid);
}

static inline int vmng_ops_pci_online(struct vmngh_ctrl_ops *ops, u32 dev_id, u32 vfid)
{
    if (ops->container_client_online == NULL) {
        return 0;
    }
    return ops->container_client_online(dev_id, vfid);
}

static inline int vmng_ops_pci_offline(struct vmngh_ctrl_ops *ops, u32 dev_id, u32 vfid)
{
    if (ops->container_client_offline == NULL) {
        return 0;
    }
    return ops->container_client_offline(dev_id, vfid);
}

STATIC int _vmngh_create_container_vdev(u32 dev_id, u32 dtype, u32 *vfid, struct vmng_vf_res_info *vf_resource)
{
    struct vmngh_ctrl_ops *ops = NULL;
    int ret = 0;

    ops = vmngh_get_ctrl_ops(dev_id);
    ret = vmng_ops_alloc_vfid(ops, dev_id, dtype, vfid);
    if (ret != 0) {
        vmng_err("Alloc vfid err. (dev_id=%d, dtype=%d)\n", dev_id, dtype);
        return ret;
    }

    ret = vmng_ops_alloc_vf(ops, dev_id, vfid, dtype, vf_resource);
    if (ret != 0) {
        vmng_err("Alloc vf err. (dev_id=%d, dtype=%d)\n", dev_id, dtype);
        vmng_ops_free_vfid(ops, dev_id, *vfid);
        return ret;
    }

    ret = vmng_ops_pci_online(ops, dev_id, *vfid);
    if (ret != 0) {
        goto EXIT2;
    }

    ret = vmngh_bw_set_token_limit(dev_id, *vfid);
    if (ret != VMNG_OK) {
        vmng_err("Set bandwidth limit failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, *vfid, ret);
        goto EXIT;
    }

    vmngh_ctrl_set_startup_flag(dev_id, *vfid, VMNG_STARTUP_BOTTOM_HALF_OK);
    vmngh_bw_data_clear_timer_init(dev_id, *vfid);
    vmng_info("Get alloc vfid. (dev_id=%d; vfid=%d; dtype=%d, vdev_id=%d)\n", dev_id, *vfid, dtype,
        vmngh_get_ctrl(dev_id, *vfid)->vdev_ctrl.dev_id);
    return VMNG_OK;

EXIT:
    vmng_ops_pci_offline(ops, dev_id, *vfid);
EXIT2:
    vmng_ops_free_vf(ops, dev_id, *vfid);
    vmng_ops_free_vfid(ops, dev_id, *vfid);

    return ret;
}

int vmngh_create_container_vdev(u32 dev_id, u32 dtype, u32 *vfid, struct vmng_vf_res_info *vf_resource)
{
    struct vmngh_pci_dev *pdev = NULL;
    int ret;

    if (vmngh_check_create_container_vdev_parameter(dev_id, dtype, vfid) != VMNG_OK) {
        return VMNG_ERR;
    }

    pdev = vmngh_get_pdev_from_unit(dev_id);

    ka_task_mutex_lock(&pdev->vpdev_mutex);
    if (pdev->vdev_ref == 0) {
        ret = uda_dev_ctrl(dev_id, UDA_CTRL_TO_MIA);
        if (ret != 0) {
            ka_task_mutex_unlock(&pdev->vpdev_mutex);
            vmng_err("To mia mode failed. (dev_id=%u)\n", dev_id);
            return ret;
        }

        vmng_set_device_split_mode(dev_id, VMNG_CONTAINER_SPLIT_MODE);
    }

    pdev->vdev_ref++;

    ret = _vmngh_create_container_vdev(dev_id, dtype, vfid, vf_resource);
    if (ret != 0) {
        pdev->vdev_ref--;
        if (pdev->vdev_ref == 0) {
            vmng_set_device_split_mode(dev_id, VMNG_NORMAL_NONE_SPLIT_MODE);
            (void)uda_dev_ctrl(dev_id, UDA_CTRL_TO_SIA);
        }
    }

    ka_task_mutex_unlock(&pdev->vpdev_mutex);
    return ret;
}
KA_EXPORT_SYMBOL(vmngh_create_container_vdev);

STATIC int vmngh_destory_single_container_vdev(u32 dev_id, u32 vfid)
{
    struct vmngh_pci_dev *pdev = NULL;
    struct vmngh_ctrl_ops *ops = NULL;
    struct vmng_ctrl_msg_info info;

    info.dev_id = dev_id;
    info.vfid = vfid;

    ops = vmngh_get_ctrl_ops(dev_id);

    if ((dev_id >= VMNG_PDEV_MAX) || (vfid >= VMNG_VDEV_MAX_PER_PDEV)) {
        vmng_err("Input parameter is error. (dev_id=%u; vfid=%u)\n", info.dev_id, info.vfid);
        return VMNG_ERR;
    }

    if (vmngh_get_ctrl(info.dev_id, info.vfid)->vdev_ctrl.status == VMNG_VDEV_STATUS_FREE) {
        vmng_err("vfid is not alloc. (dev_id=%u; vfid=%u)\n", info.dev_id, info.vfid);
        return VMNG_ERR;
    }

    if (devdrv_is_sriov_support(dev_id) == false) {
        vmngh_bw_data_clear_timer_uninit();
    }
    vmngh_ctrl_set_startup_flag(info.dev_id, info.vfid, VMNG_STARTUP_PROBED);

    if (vmng_ops_pci_offline(ops, dev_id, vfid) != 0) {
        vmng_err("Uinit instance all client failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
    }

    if (vmng_ops_free_vf(ops, dev_id, vfid) != 0) {
        vmng_err("Send free vf msg to device failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
    }

    vmng_ops_free_vfid(ops, dev_id, vfid);

    pdev = vmngh_get_pdev_from_unit(dev_id);
    ka_task_mutex_lock(&pdev->vpdev_mutex);
    pdev->vdev_ref--;
    if (pdev->vdev_ref == 0) {
        vmng_set_device_split_mode(dev_id, VMNG_NORMAL_NONE_SPLIT_MODE);
        (void)uda_dev_ctrl(dev_id, UDA_CTRL_TO_SIA);
    }
    ka_task_mutex_unlock(&pdev->vpdev_mutex);
    vmng_info("Call vmngh_free_vfid success. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
    return VMNG_OK;
}

STATIC int vmngh_destory_all_container_vdev(u32 dev_id)
{
    struct vmng_vdev_ctrl *msg = NULL;
    int ret;
    u32 i;

    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        msg = &vmngh_get_ctrl(dev_id, i)->vdev_ctrl;
        if (msg->status == VMNG_VDEV_STATUS_FREE) {
            continue;
        }

        ret = vmngh_destory_single_container_vdev(dev_id, msg->vfid);
        if (ret != VMNG_OK) {
            vmng_err("Destroy vfid failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, msg->vfid, ret);
        }
    }

    vmng_info("Destroy all_container success. (dev_id=%u)\n", dev_id);
    return VMNG_OK;
}

int vmngh_destory_container_vdev(u32 dev_id, u32 vfid)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    if (vmngh_get_device_status(dev_id) != VMNG_VALID) {
        vmng_err("Device is not ready. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (vfid == VMNGH_DESTORY_ALL_VDEV) {
        return vmngh_destory_all_container_vdev(dev_id);
    } else {
        return vmngh_destory_single_container_vdev(dev_id, vfid);
    }
}
KA_EXPORT_SYMBOL(vmngh_destory_container_vdev);

STATIC int vmngh_notify_sriov_info(u32 dev_id, enum vmng_pf_sriov_status sriov_status)
{
    struct vmng_sriov_info sriov_info = {0};
    struct vmngh_client *client;
    int type;
    int ret;

    sriov_info.dev_id = dev_id;
    sriov_info.sriov_status = sriov_status;

    for (type = 0; type < VMNG_CLIENT_TYPE_MAX; type++) {
        client = g_vmngh_clients[type];
        if (client == NULL || client->sriov_instance == NULL) {
            vmng_info("Client not register sriov_instance function. (dev_id=%u;type=%d).\n", dev_id, type);
            continue;
        }

        vmng_info("Sriov instance begin. (dev_id=%u;type=%d;status=%d)\n", dev_id, type, sriov_status);
        ret = client->sriov_instance(&sriov_info);
        if (ret != 0) {
            vmng_err("Client sriov instance failed. (dev_id=%u;client_type=%d;ret=%d;status=%d)\n",
                     dev_id, type, ret, (int)sriov_info.sriov_status);
            goto FAILED;
        }
        vmng_info("Sriov instance end. (dev_id=%u;type=%d;status=%d)\n", dev_id, type, sriov_status);
    }

    vmng_info("Sriov inform success.(dev_id=%u;sriov_status=%d)\n", dev_id, (int)sriov_info.sriov_status);

    return VMNG_OK;

FAILED:
    sriov_info.sriov_status = (sriov_status == VMNGH_PF_SRIOV_DISABLE) ?
        VMNGH_PF_SRIOV_ENABLE : VMNGH_PF_SRIOV_DISABLE;
    for (type--; type >= 0; --type) {
        client = g_vmngh_clients[type];
        if ((client != NULL) && (client->sriov_instance != NULL)) {
            (void)client->sriov_instance(&sriov_info);
        }
    }
    return VMNG_ERR;
}

int vmngh_common_enable_sriov(u32 dev_id)
{
    struct vmngh_ctrl_ops *ops = NULL;
    int ret;

    ops = vmngh_get_ctrl_ops(dev_id);
    if (ops == NULL || ops->enable_sriov == NULL) {
        vmng_err("Not support enable_sriov. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (ops->enable_sriov(dev_id) != VMNG_OK) {
        vmng_err("enable_sriov fail. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    ret = vmngh_notify_sriov_info(dev_id, VMNGH_PF_SRIOV_ENABLE);
    if (ret != 0) {
        vmng_err("Inform sriov info failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        (void)ops->disable_sriov(dev_id);
        return ret;
    }

    return VMNG_OK;
}

STATIC int vmngh_common_disable_sriov(u32 dev_id)
{
    struct vmngh_ctrl_ops *ops = NULL;
    int ret;

    ops = vmngh_get_ctrl_ops(dev_id);
    if (ops == NULL || ops->disable_sriov == NULL) {
        vmng_err("Not support disable_sriov. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    ret = vmngh_notify_sriov_info(dev_id, VMNGH_PF_SRIOV_DISABLE);
    if (ret != 0) {
        vmng_err("Inform sriov info failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    if (ops->disable_sriov(dev_id) != VMNG_OK) {
        vmng_err("disable_sriov fail. (dev_id=%u)\n", dev_id);
        (void)vmngh_notify_sriov_info(dev_id, VMNGH_PF_SRIOV_ENABLE);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

bool is_sriov_enable(u32 dev_id)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, 0);

    return ((ctrl->sriov_status == VMNGH_PF_SRIOV_ENABLE) ? true : false);
}

static void set_sriov_mode(u32 dev_id, int sriov_mode)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, 0);
    if (ctrl == NULL) {
        vmng_err("Set sriov mode failed. (dev_id=%u)\n", dev_id);
        return;
    }
    ctrl->sriov_mode = sriov_mode;
}

int vmngh_enable_sriov(u32 dev_id)
{
    ka_mutex_t *ctrl_mutex = vmngh_get_ctrl_mutex();
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%d; MAX=%d)\n", dev_id, VMNG_PDEV_MAX);
        return VMNG_ERR;
    }

    ka_task_mutex_lock(ctrl_mutex);
    if (is_sriov_enable(dev_id)) {
        vmng_info("Already enable. (dev_id=%u)\n", dev_id);
        ka_task_mutex_unlock(ctrl_mutex);
        return VMNG_OK;
    }

    ret = devdrv_set_sriov_and_mdev_mode(dev_id, DEVDRV_BOOT_ONLY_SRIOV);
    if (ret != 0) {
        vmng_err("Set pcie container sriov mode failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        ka_task_mutex_unlock(ctrl_mutex);
        return ret;
    }

    ret = vmngh_common_enable_sriov(dev_id);
    if (ret != 0) {
        (void)devdrv_set_sriov_and_mdev_mode(dev_id, DEVDRV_BOOT_DEFAULT_MODE);
        vmng_err("Enable sriov failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        ka_task_mutex_unlock(ctrl_mutex);
        return ret;
    }
    set_sriov_mode(dev_id, DEVDRV_BOOT_ONLY_SRIOV);
    ka_task_mutex_unlock(ctrl_mutex);
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_enable_sriov);

int vmngh_disable_sriov(u32 dev_id)
{
    ka_mutex_t *ctrl_mutex = vmngh_get_ctrl_mutex();
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%d; MAX=%d)\n", dev_id, VMNG_PDEV_MAX);
        return VMNG_ERR;
    }

    ka_task_mutex_lock(ctrl_mutex);
    if (!is_sriov_enable(dev_id)) {
        vmng_info("Already disable. (dev_id=%u)\n", dev_id);
        ka_task_mutex_unlock(ctrl_mutex);
        return VMNG_OK;
    }

    ret = vmngh_common_disable_sriov(dev_id);
    if (ret != 0) {
        vmng_err("Disable sriov failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        ka_task_mutex_unlock(ctrl_mutex);
        return ret;
    }

    ret = devdrv_set_sriov_and_mdev_mode(dev_id, DEVDRV_BOOT_DEFAULT_MODE);
    if (ret != 0) {
        (void)vmngh_common_enable_sriov(dev_id);
        vmng_err("Set pcie sriov mode failed. (ret=%d;set_mode=%d)\n", dev_id, DEVDRV_BOOT_DEFAULT_MODE);
        ka_task_mutex_unlock(ctrl_mutex);
        return ret;
    }

    ka_task_mutex_unlock(ctrl_mutex);
    return VMNG_OK;
}
KA_EXPORT_SYMBOL(vmngh_disable_sriov);

int vmngh_enquire_soc_resource(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info)
{
    struct vmngh_ctrl_ops *ops = NULL;
    unsigned int pf_id = dev_id;
    unsigned int vf_id = vfid;
    int ret;

    if (info == NULL) {
        vmng_err("Info NULL.\n");
        return VMNG_ERR;
    }

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        if (vfid != 0) {
            vmng_err("vfid is invalid. (dev_id=%u, vfid=%u)\n", dev_id, vfid);
            return -EINVAL;
        }
        ret = devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id);
        if (ret != 0) {
            vmng_err("Get pfvf id failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    if ((pf_id >= VMNG_PDEV_MAX) || (vf_id >= VMNG_VDEV_MAX_PER_PDEV)) {
        vmng_err("Input parameter error. (dev_id=%u, vfid=%u, pf_id=%u, vf_id=%u)\n", dev_id, vfid, pf_id, vf_id);
        return VMNG_ERR;
    }

    ops = vmngh_get_ctrl_ops(pf_id);
    if (ops->enquire_vf == NULL) {
        vmng_warn("enquire_vf hasn't been initialzed. (dev_id=%u)\n", pf_id);
        return VMNG_OK;
    }

    if (ops->enquire_vf(pf_id, vf_id, info) != VMNG_OK) {
        vmng_err("enquire_vf fail. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    return VMNG_OK;
}
KA_EXPORT_SYMBOL(vmngh_enquire_soc_resource);

int vmngh_refresh_vdev_resource(u32 dev_id, u32 vfid, struct vmng_soc_resource_refresh *info)
{
    struct vmngh_ctrl_ops *ops = NULL;

    if (info == NULL) {
        vmng_err("Info NULL.\n");
        return VMNG_ERR;
    }

    if (dev_id >= VMNG_PDEV_MAX || vfid > VMNG_VF_MAX_PER_PF) {
        vmng_err("Input parameter error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    ops = vmngh_get_ctrl_ops(dev_id);
    if (ops == NULL || ops->refresh_vf == NULL) {
        vmng_err("Not support refresh_vf. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (ops->refresh_vf(dev_id, vfid, info) != VMNG_OK) {
        vmng_err("refresh_vf fail. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    return VMNG_OK;
}
KA_EXPORT_SYMBOL(vmngh_refresh_vdev_resource);

int vmngh_sriov_reset_vdev(u32 dev_id, u32 vfid)
{
    struct vmngh_ctrl_ops *ops = NULL;
    u32 pf_id, vf_id;

    if (devdrv_get_pfvf_type_by_devid(dev_id) != DEVDRV_SRIOV_TYPE_VF) {
        vmng_err("Input parameter error, is not vf. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id) != 0) {
        vmng_err("Get pf id and vf id failed.. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    ops = vmngh_get_ctrl_ops(pf_id);
    if (ops == NULL || ops->reset_vf == NULL) {
        vmng_err("Not support reset_vf. (dev_id=%u, pf_id=%u)\n", dev_id, pf_id);
        return VMNG_ERR;
    }

    if (ops->reset_vf(dev_id, vfid) != VMNG_OK) {
        vmng_err("reset vf fail. (dev_id=%u, vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    return VMNG_OK;
}
KA_EXPORT_SYMBOL(vmngh_sriov_reset_vdev);

STATIC int vmngh_get_dev_numa_info(unsigned int dev_id, struct vmng_vf_memory_info **mem_info)
{
    struct vmngh_vdev_ctrl *ctrl;
    unsigned int pf_id = dev_id;
    unsigned int vf_id = 0;
    int ret;

    ret = devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id);
    if ((ret != 0) || (pf_id >= VMNG_PDEV_MAX) || (vf_id >= VMNG_VDEV_MAX_PER_PDEV)) {
        vmng_err("Trans dev_id to phy_id and vf_id failed.(dev_id=%u,pf_id=%u,vf_id=%u)\n", dev_id, pf_id, vf_id);
        return -EINVAL;
    }
    ctrl = vmngh_get_ctrl(pf_id, vf_id);
    *mem_info = &ctrl->memory;

    return 0;
}

int vmngh_check_vdev_phy_address(unsigned int dev_id, u64 phy_address, u64 length)
{
    struct vmng_vf_memory_info *mem_info = NULL;
    u64 numa_start, numa_end;
    int ret;
    int i;

    ret = vmngh_get_dev_numa_info(dev_id, &mem_info);
    if (ret != 0 || mem_info == NULL) {
        vmng_err("Get device numa info failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    for (i = 0; i < mem_info->number; ++i) {
        numa_start = mem_info->address[i].start;
        numa_end = mem_info->address[i].end;
        if ((phy_address >= numa_start) && (phy_address + length <= numa_end)) {
            return 0;
        }
    }
    return -EINVAL;
}
KA_EXPORT_SYMBOL(vmngh_check_vdev_phy_address);

STATIC int vmngh_bw_calcu_token_limit(u32 dev_id, u32 vfid, u64 *flow_limit, u64 *pack_limit)
{
    struct vmngh_pci_dev *vmngh_pdev = NULL;
    u32 total_aicore, used_aicore;
    u64 bandwidth, packspeed;

    vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);
    bandwidth = vmngh_pdev->bw_ctrl.bandwidth;
    packspeed = vmngh_pdev->bw_ctrl.packspeed;
    used_aicore = vmngh_get_ctrl(dev_id, vfid)->vdev_ctrl.core_num;
    total_aicore = vmngh_get_total_core_num(dev_id);
    if (total_aicore == 0) {
        *flow_limit = 0;
        *pack_limit = 0;
    } else {
        *flow_limit = (bandwidth * used_aicore) / total_aicore;
        *pack_limit = (packspeed * used_aicore) / total_aicore;
    }

    return 0;
}

STATIC int vmngh_bw_set_remote_token_limit(u32 dev_id, u32 vfid, u64 flow_limit, u64 pack_limit)
{
    struct vf_bandwidth_ctrl_remote* vf_ptr = NULL;
    int ret;

    ret = vmngh_bw_get_vfio_barspace(dev_id, vfid, &vf_ptr);
    if (ret != 0) {
        vmng_err("Get vfio base failed. (dev_id=%u; fid=%u)\n", dev_id, vfid);
        return ret;
    }

    vf_ptr->flow_limit = flow_limit;
    vf_ptr->pack_limit = pack_limit;

    return 0;
}

STATIC int vmngh_bw_set_remote_hostcpu_data(u32 dev_id, u32 vfid, u32 dir, u64 data_len, u32 node_cnt)
{
    struct vf_bandwidth_ctrl_remote* vf_ptr = NULL;
    struct vf_bandwidth_ctrl_local* ptr_local = NULL;
    u64 flow_limit, pack_limit;
    unsigned long flags;
    int ret;

    ret = vmngh_bw_get_vfio_barspace(dev_id, vfid, &vf_ptr);
    if (ret != 0) {
        vmng_err("Get vfio base failed. (dev_id=%u; fid=%u)\n", dev_id, vfid);
        return ret;
    }

    ret = vmngh_bw_get_local_data(dev_id, vfid, &flow_limit, &pack_limit, &ptr_local);
    if (ret != 0) {
        vmng_err("Get local token info failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return ret;
    }

    ka_task_spin_lock_irqsave(&g_clear_timer.bandwidth_update_lock, flags);

    ptr_local->hostcpu_flow_cnt[dir] += data_len;
    ptr_local->hostcpu_pack_cnt[dir] += node_cnt;

    if (ptr_local->write_cnt == 0) {
        vf_ptr->hostcpu_flow_cnt[dir] = ptr_local->hostcpu_flow_cnt[dir];
        vf_ptr->hostcpu_pack_cnt[dir] = ptr_local->hostcpu_pack_cnt[dir];
    }
    ptr_local->write_cnt = (ptr_local->write_cnt + 1) % VMNG_BW_BANDWIDTH_RENEW_CNT;

    ka_task_spin_unlock_irqrestore(&g_clear_timer.bandwidth_update_lock, flags);

    return 0;
}

STATIC void vmngh_bw_init_local_token_limit(u32 dev_id, u32 vfid, u64 flow_limit, u64 pack_limit)
{
    struct vmngh_pci_dev *vmngh_pdev = vmngh_get_pdev_from_unit(dev_id);

    if (vfid < VMNG_VDEV_FIRST_VFID) {
        vmng_err("vfid is illegal. (dev_id=%u; fid=%u)\n", dev_id, vfid);
        return;
    }

    vmngh_pdev->bw_ctrl.flow_limit[vfid - VMNG_VDEV_FIRST_VFID] = flow_limit;
    vmngh_pdev->bw_ctrl.pack_limit[vfid - VMNG_VDEV_FIRST_VFID] = pack_limit;
}

int vmngh_bw_set_token_limit(u32 dev_id, u32 vfid)
{
    u64 flow_limit = 0;
    u64 pack_limit = 0;
    int ret;

    if (devdrv_is_sriov_support(dev_id)) {
        return 0;
    }

    ret = vmngh_bw_calcu_token_limit(dev_id, vfid, &flow_limit, &pack_limit);
    if (ret != 0) {
        vmng_err("Calcu token limit error. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return -EINVAL;
    }

    ret = vmngh_bw_set_remote_token_limit(dev_id, vfid, flow_limit, pack_limit);
    if (ret != 0) {
        vmng_err("Set limit information to remote failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return -EINVAL;
    }

    vmngh_bw_init_local_token_limit(dev_id, vfid, flow_limit, pack_limit);

    return 0;
}

STATIC int vmngh_bw_local_data_update(u32 dev_id, u32 vfid, struct vf_bandwidth_ctrl_local *ptr_local)
{
    struct vf_bandwidth_ctrl_remote* vf_ptr = NULL;
    int ret;

    ret = vmngh_bw_get_vfio_barspace(dev_id, vfid, &vf_ptr);
    if (ret != 0) {
        vmng_err("Get vfio base failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return ret;
    }

    if (ptr_local->read_cnt == 0) {
        ptr_local->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_H2D] = vf_ptr->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_H2D];
        ptr_local->tscpu_flow_cnt[VMNG_PCIE_FLOW_H2D] = vf_ptr->tscpu_flow_cnt[VMNG_PCIE_FLOW_H2D];
        ptr_local->ctrlcpu_pack_cnt[VMNG_PCIE_FLOW_H2D] = vf_ptr->ctrlcpu_pack_cnt[VMNG_PCIE_FLOW_H2D];
        ptr_local->tscpu_pack_cnt[VMNG_PCIE_FLOW_H2D] = vf_ptr->tscpu_pack_cnt[VMNG_PCIE_FLOW_H2D];
        ptr_local->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_D2H] = vf_ptr->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_D2H];
        ptr_local->tscpu_flow_cnt[VMNG_PCIE_FLOW_D2H] = vf_ptr->tscpu_flow_cnt[VMNG_PCIE_FLOW_D2H];
        ptr_local->ctrlcpu_pack_cnt[VMNG_PCIE_FLOW_D2H] = vf_ptr->ctrlcpu_pack_cnt[VMNG_PCIE_FLOW_D2H];
        ptr_local->tscpu_pack_cnt[VMNG_PCIE_FLOW_D2H] = vf_ptr->tscpu_pack_cnt[VMNG_PCIE_FLOW_D2H];
    }
    ptr_local->read_cnt = (ptr_local->read_cnt + 1) % VMNG_BW_BANDWIDTH_RENEW_CNT;

    return 0;
}

STATIC int vmngh_bw_excess_bandwidth_judge(u32 dev_id, u32 vfid, u32 dir, bool* bandwidth_full)
{
    struct vf_bandwidth_ctrl_local* ptr_local = NULL;
    u64 flow_limit, pack_limit, single_flow, total_flow, single_pack;
    int ret;

    ret = vmngh_bw_get_local_data(dev_id, vfid, &flow_limit, &pack_limit, &ptr_local);
    if (ret != 0) {
        vmng_err("Get local token info failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return ret;
    }

    ret = vmngh_bw_local_data_update(dev_id, vfid, ptr_local);
    if (ret != 0) {
        vmng_err("Update local data failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return ret;
    }

    if (dir == VMNG_PCIE_FLOW_H2D) {
        single_flow = ptr_local->hostcpu_flow_cnt[VMNG_PCIE_FLOW_H2D] +
                      ptr_local->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_H2D] +
                      ptr_local->tscpu_flow_cnt[VMNG_PCIE_FLOW_H2D];
        total_flow = ptr_local->hostcpu_flow_cnt[VMNG_PCIE_FLOW_D2H] +
                     ptr_local->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_D2H] +
                     ptr_local->tscpu_flow_cnt[VMNG_PCIE_FLOW_D2H] + single_flow;
        single_pack = ptr_local->hostcpu_pack_cnt[VMNG_PCIE_FLOW_H2D] +
                      ptr_local->ctrlcpu_pack_cnt[VMNG_PCIE_FLOW_H2D] +
                      ptr_local->tscpu_pack_cnt[VMNG_PCIE_FLOW_H2D];
    } else {
        single_flow = ptr_local->hostcpu_flow_cnt[VMNG_PCIE_FLOW_D2H] +
                      ptr_local->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_D2H] +
                      ptr_local->tscpu_flow_cnt[VMNG_PCIE_FLOW_D2H];
        total_flow = ptr_local->hostcpu_flow_cnt[VMNG_PCIE_FLOW_H2D] +
                     ptr_local->ctrlcpu_flow_cnt[VMNG_PCIE_FLOW_H2D] +
                     ptr_local->tscpu_flow_cnt[VMNG_PCIE_FLOW_H2D] + single_flow;
        single_pack = ptr_local->hostcpu_pack_cnt[VMNG_PCIE_FLOW_D2H] +
                      ptr_local->ctrlcpu_pack_cnt[VMNG_PCIE_FLOW_D2H] +
                      ptr_local->tscpu_pack_cnt[VMNG_PCIE_FLOW_D2H];
    }

    if ((single_flow >= flow_limit) || (single_pack >= pack_limit) ||
        (total_flow >= ((flow_limit * VMNG_BANDW_DUPLEX_PERCENTAGE) / VMNG_BANDW_PERCENTAGE_BASE))) {
        *bandwidth_full = true;
    } else {
        *bandwidth_full = false;
    }

    return 0;
}

int vmng_bandwidth_limit_check(struct vmng_bandwidth_check_info *info)
{
    bool bandwidth_full = false;
    int retry_cnt = 0;
    int ret;

    if (info == NULL) {
        vmng_err("Input para error, info is NULL\n");
        return -EINVAL;
    }

    if ((info->dev_id >= VMNG_PDEV_MAX) || (info->vfid >= VMNG_VDEV_MAX_PER_PDEV) ||
        (info->dir > VMNG_PCIE_FLOW_D2H)) {
        vmng_err("Input parameter error. (dev_id=%u; vfid=%u; dir=%u)\n", info->dev_id, info->vfid, info->dir);
        return -EINVAL;
    }

    /* no need to judge physical machine or length less than 1K */
    if ((info->vfid < VMNG_VDEV_FIRST_VFID) || (info->data_len < VMNG_BW_BANDWIDTH_CHECK_LEN)) {
        return 0;
    }

RETRY_CHECK:
    ret = vmngh_bw_excess_bandwidth_judge(info->dev_id, info->vfid, info->dir, &bandwidth_full);
    if (ret != 0) {
        vmng_err("Excess bandwidth judge error. (ret=%d)\n", ret);
        return ret;
    }

    if (bandwidth_full) {
        if ((retry_cnt > VMNG_BW_BANDWIDTH_CHECK_MAX_CNT) ||
            (info->handle_mode == VMNG_BW_BANDWIDTH_CHECK_NON_SLEEP)) {
            return -EBUSY;
        }
        ka_system_msleep(VMNG_BW_BANDWIDTH_CHECK_WAIT_TIME);
        retry_cnt++;
        goto RETRY_CHECK;
    } else {
        ret = vmngh_bw_set_remote_hostcpu_data(info->dev_id, info->vfid, info->dir, info->data_len, info->node_cnt);
        if (ret != 0) {
            vmng_err("Set remote hostcpu data error. (ret=%d)\n", ret);
            return ret;
        }
    }

    return 0;
}
KA_EXPORT_SYMBOL(vmng_bandwidth_limit_check);

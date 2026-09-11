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

#include "virtmnghost_pci.h"
#include "virtmng_interface.h"
#include "devdrv_interface.h"
#include "virtmnghost_unit.h"
#include "virtmng_res_cloud_v1.h"

#include "ka_base_pub.h"

STATIC int vmngh_alloc_vdev_ctrl_cloud_v1(u32 dev_id, u32 dtype, const u32 *fid)
{
    ka_mutex_t *ctrl_mutex = vmngh_get_ctrl_mutex();
    struct vmngh_vdev_ctrl *ctrl = NULL;
    u32 vfid;

    if (fid == NULL) {
        vmng_err("Invalid parameter.\n");
        return VMNG_ERR;
    }

    ctrl = vmngh_get_ctrl(dev_id, *fid);
    if (ctrl == NULL) {
        vmng_err("Get_ctrl error.\n");
        return VMNG_ERR;
    }

    vfid = *fid;
    ka_task_mutex_lock(ctrl_mutex);
    ctrl->vdev_ctrl.dev_id = dev_id;
    ctrl->vdev_ctrl.vfid = vfid;
    ctrl->vdev_ctrl.dtype = dtype;
    ctrl->vdev_ctrl.core_num = VMNGH_DTYPE_TO_AICORE_NUM(dtype);
    ctrl->vdev_ctrl.total_core_num = vmngh_get_total_core_num(dev_id);
    ctrl->vdev_ctrl.status = VMNG_VDEV_STATUS_ALLOC;
    ka_task_mutex_unlock(ctrl_mutex);

    return VMNG_OK;
}

STATIC int vmngh_alloc_vfid_cloud_v1(u32 dev_id, u32 dtype, u32 *fid)
{
    int ret;

    ret = vmngh_alloc_vfid(dev_id, fid);
    if (ret != VMNG_OK) {
        vmng_err("Alloc vfid failed. (ret=%d;dev_id=%u;vfid=%u)\n", ret, dev_id, *fid);
        return ret;
    }

    ret = vmngh_alloc_vdev_ctrl_cloud_v1(dev_id, dtype, fid);
    if (ret != VMNG_OK) {
        vmng_err("alloc_vdev_ctrl_cloud_v1 error.(ret=%d;dev_id=%d;vfid=%d;dtype=%d)\n", ret, dev_id, *fid, dtype);
        return ret;
    }

    return VMNG_OK;
}

STATIC void vmngh_free_vfid_cloud_v1(u32 dev_id, u32 vfid)
{
    if (vmngh_dev_id_check(dev_id, vfid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return;
    }

    vmngh_free_vdev_ctrl(dev_id, vfid);
}

STATIC int vmngh_container_client_online_cloud_v1(u32 dev_id, u32 vfid)
{
    int ret;

    ret = vmngh_init_instance_all_client(dev_id, vfid, VMNGH_CONTAINER);
    if (ret != VMNG_OK) {
        vmng_err("Init instance all client failed. (dev_id=%d; vfid=%d)\n", dev_id, vfid);
        return ret;
    }

    ret = vmngh_add_mia_dev(dev_id, vfid, 0);
    if (ret != 0) {
        (void)vmngh_uninit_instance_all_client(dev_id, vfid);
        return ret;
    }

    ret = vmngh_init_instance_client_device(dev_id, vfid);
    if (ret != VMNG_OK) {
        (void)vmngh_remove_mia_dev(dev_id, vfid, 0);
        (void)vmngh_uninit_instance_all_client(dev_id, vfid);
        vmng_err("Init instance all client device failed. (dev_id=%d; vfid=%d)\n", dev_id, vfid);
        return ret;
    }

    return VMNG_OK;
}

STATIC int vmngh_container_client_offline_cloud_v1(u32 dev_id, u32 vfid)
{
    int ret;

    ret = vmngh_uninit_instance_client_device(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmngh_ctrl_set_startup_flag(dev_id, (u32)vfid, VMNG_STARTUP_BOTTOM_HALF_OK);
        vmng_err("Uninit instance all client device failed. (dev_id=%d; vfid=%d)\n", dev_id, vfid);
        return ret;
    }

    (void)vmngh_remove_mia_dev(dev_id, vfid, 0);

    ret = vmngh_uninit_instance_all_client(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Uninit instance all client failed. (dev_id=%d; vfid=%d)\n", dev_id, vfid);
    }

    return VMNG_OK;
}

STATIC int vmngh_enquire_soc_resource_cloud_v1(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, vfid);
    int ret;

    if (info == NULL) {
        vmng_err("Info NULL.\n");
        return VMNG_ERR;
    }

    if (ctrl == NULL) {
        vmng_err("Get_ctrl error.\n");
        return VMNG_ERR;
    }

    ret = memset_s(info, sizeof(struct vmng_soc_resource_enquire), 0xff, sizeof(struct vmng_soc_resource_enquire));
    if (ret != EOK) {
        vmng_err("Call memcpy_s err.(dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    ret = memset_s(info->each.name, VMNG_VF_TEMP_NAME_LEN, 0, VMNG_VF_TEMP_NAME_LEN);
    if (ret != EOK) {
        vmng_err("Call memcpy_s err.(dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    info->each.stars_static.aic = ctrl->vdev_ctrl.core_num;

    return VMNG_OK;
}

int vmngh_res_init_cloud_v1(struct vmngh_ctrl_ops *ops)
{
    if (ops == NULL) {
        vmng_err("Invalid parameter.\n");
        return VMNG_ERR;
    }

    ops->alloc_vfid = vmngh_alloc_vfid_cloud_v1;
    ops->free_vfid = vmngh_free_vfid_cloud_v1;
    ops->enquire_vf = vmngh_enquire_soc_resource_cloud_v1;
    ops->container_client_online = vmngh_container_client_online_cloud_v1;
    ops->container_client_offline = vmngh_container_client_offline_cloud_v1;
    ops->valid = VMNG_VALID;

    return VMNG_OK;
}

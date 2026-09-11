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

#include "virtmnghost_ctrl.h"
#include "virtmnghost_pci.h"
#include "virtmng_interface.h"
#include "devdrv_interface.h"
#include "virtmnghost_unit.h"
#include "virtmng_res_mini_v2.h"

#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"

STATIC int vmngh_alloc_vdev_ctrl_mini_v2(u32 dev_id, u32 dtype, const u32 *fid)
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

STATIC int vmngh_alloc_vfid_mini_v2(u32 dev_id, u32 dtype, u32 *fid)
{
    int ret;

    ret = vmngh_alloc_vfid(dev_id, fid);
    if (ret != VMNG_OK) {
        vmng_err("alloc vfid failed. (dev_id=%u;vfid=%u)\n", dev_id, *fid);
        return ret;
    }

    ret = vmngh_alloc_vdev_ctrl_mini_v2(dev_id, dtype, fid);
    if (ret != VMNG_OK) {
        vmng_err("alloc_vdev_ctrl_mini_v2 error.(ret=%d;dev_id=%d;vfid=%d;dtype=%d)\n", ret, dev_id, *fid, dtype);
        return ret;
    }

    return VMNG_OK;
}

STATIC void vmngh_free_vfid_mini_v2(u32 dev_id, u32 vfid)
{
    if (vmngh_dev_id_check(dev_id, vfid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return;
    }

    vmngh_free_vdev_ctrl(dev_id, vfid);
}

STATIC int vmngh_alloc_vf_mini_v2(u32 dev_id, u32 *fid, u32 dtype, struct vmng_vf_res_info *vf_resource)
{
    struct vmng_ctrl_msg_info info;
    int ret;

    if ((vf_resource == NULL) || (fid == NULL)) {
        vmng_err("Invalid parameter.\n");
        return VMNG_ERR;
    }

    if (memset_s(&info, sizeof(info), 0, sizeof(info)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    info.dev_id = dev_id;
    info.vfid = *fid;
    info.dtype = dtype;
    info.core_num = VMNGH_DTYPE_TO_AICORE_NUM(dtype);
    info.total_core_num = vmngh_get_total_core_num(dev_id);
    ret = memcpy_s(&info.vf_cfg, sizeof(struct vmng_vf_res_info), vf_resource, sizeof(struct vmng_vf_res_info));
    if (ret != EOK) {
        vmng_err("memcpy_s failed, (ret = %d).\n", ret);
        return VMNG_ERR;
    }
    if (vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_ALLOC_VF) != VMNG_OK) {
        vmng_err("Send msg failed. (dev_id=%u;vfid=%u)\n", dev_id, *fid);
        return VMNG_ERR;
    }
    vf_resource->vfg.vfg_id = info.vf_cfg.vfg.vfg_id;

    return VMNG_OK;
}

STATIC int vmngh_free_vf_mini_v2(u32 dev_id, u32 vfid)
{
    struct vmng_ctrl_msg_info info;

    if (vmngh_dev_id_check(dev_id, vfid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    if (memset_s(&info, sizeof(info), 0, sizeof(info)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    info.dev_id = dev_id;
    info.vfid = vfid;
    if (vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_FREE_VF) != VMNG_OK) {
        vmng_err("Send msg failed. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_container_client_online_mini_v2(u32 dev_id, u32 vfid)
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

STATIC int vmngh_container_client_offline_mini_v2(u32 dev_id, u32 vfid)
{
    int ret;

    ret = vmngh_uninit_instance_client_device(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Uninit instance all client device failed. (dev_id=%d; vfid=%d)\n", dev_id, vfid);
    }

    (void)vmngh_remove_mia_dev(dev_id, vfid, 0);

    ret = vmngh_uninit_instance_all_client(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Uninit instance all client failed. (dev_id=%d; vfid=%d)\n", dev_id, vfid);
    }

    return VMNG_OK;
}

STATIC int vmngh_enquire_soc_resource_mini_v2(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info)
{
    struct vmng_vdev_ctrl *vdev_ctrl = NULL;
    struct vmng_ctrl_msg_info info_msg;
    struct vmngh_vdev_ctrl *ctrl = NULL;
    int ret;

    if (info == NULL) {
        vmng_err("Info NULL.\n");
        return VMNG_ERR;
    }

    if ((dev_id >= VMNG_PDEV_MAX) || (vfid >= VMNG_VDEV_MAX_PER_PDEV)) {
        vmng_err("Input parameter error. (dev_id=%u, vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    if (memset_s(&info_msg, sizeof(info_msg), 0, sizeof(info_msg)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    info_msg.dev_id = dev_id;
    info_msg.vfid = vfid;
    ret = vmngh_vdev_msg_send(&info_msg, VMNG_CTRL_MSG_TYPE_ENQUIRE_VF);
    if (ret != VMNG_OK) {
        vmng_err("Send msg failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return ret;
    }

    ret = memcpy_s(info, sizeof(struct vmng_soc_resource_enquire), &info_msg.enquire,
        sizeof(struct vmng_soc_resource_enquire));
    if (ret != EOK) {
        vmng_err("Call memcpy_s err.(dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }
    ctrl = vmngh_get_ctrl(dev_id, vfid);
    vdev_ctrl = &ctrl->vdev_ctrl;
    info->each.stars_static.aic = vdev_ctrl->core_num;

    return VMNG_OK;
}

int vmngh_res_init_mini_v2(struct vmngh_ctrl_ops *ops)
{
    if (ops == NULL) {
        vmng_err("Invalid parameter.\n");
        return VMNG_ERR;
    }

    ops->alloc_vf = vmngh_alloc_vf_mini_v2;
    ops->free_vf = vmngh_free_vf_mini_v2;
    ops->alloc_vfid = vmngh_alloc_vfid_mini_v2;
    ops->free_vfid = vmngh_free_vfid_mini_v2;
    ops->enquire_vf = vmngh_enquire_soc_resource_mini_v2;
    ops->container_client_online = vmngh_container_client_online_mini_v2;
    ops->container_client_offline = vmngh_container_client_offline_mini_v2;
    ops->valid = VMNG_VALID;

    return VMNG_OK;
}

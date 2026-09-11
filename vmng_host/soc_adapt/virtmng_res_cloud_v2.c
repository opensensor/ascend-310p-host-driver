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
#include "virtmng_res_cloud_v2.h"

#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"

STATIC int is_sriov_vf_cloud_v2(u32 dev_id)
{
    return ((devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) ? true : false);
}

static int get_sriov_mode(u32 dev_id)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, 0);
    return ctrl->sriov_mode;
}

// Only create container vf need to alloc vfid, vascend has assign vfid when creating vm mdev
STATIC int vmngh_alloc_vfid_cloud_v2(u32 dev_id, u32 dtype, u32 *fid)
{
    int sriov_mode;
    int ret;

    if (is_sriov_enable(dev_id) == false) {
        vmng_err("Sriov disabled. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    sriov_mode = get_sriov_mode(dev_id);
    if (sriov_mode != DEVDRV_BOOT_ONLY_SRIOV) {
        vmng_err("Not support sriov mode, check operation. (dev_id=%u;sriov_mode=%d)\n", dev_id, sriov_mode);
        return -EOPNOTSUPP;
    }

    ret = vmngh_alloc_vfid(dev_id, fid);
    if (ret != 0) {
        vmng_err("Alloc vfid failed.(ret=%d;dev_id=%u;vfid=%u)\n", ret, dev_id, *fid);
        return ret;
    }

    return VMNG_OK;
}

STATIC int vmngh_alloc_vdev_ctrl_cloud_v2(u32 dev_id, u32 vfid, struct vmng_ctrl_msg_info *info)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, vfid);
    ka_mutex_t *ctrl_mutex = vmngh_get_ctrl_mutex();
    struct vmng_vdev_ctrl *vdev_ctrl = &ctrl->vdev_ctrl;
    int ret;

    ka_task_mutex_lock(ctrl_mutex);
    (void)devdrv_get_devid_by_pfvf_id(dev_id, vfid, &vdev_ctrl->dev_id);
    vdev_ctrl->vfid = 0;
    vdev_ctrl->dtype = info->dtype;
    vdev_ctrl->core_num = info->core_num;
    vdev_ctrl->total_core_num = info->total_core_num;
    if (memcpy_s(&vdev_ctrl->vf_cfg, sizeof(vmng_vf_cfg_t), &info->vf_cfg, sizeof(vmng_vf_cfg_t)) != EOK) {
        vmng_err("Call memcpy_s failed.\n");
        ka_task_mutex_unlock(ctrl_mutex);
        return VMNG_ERR;
    }
    vdev_ctrl->status = VMNG_VDEV_STATUS_ALLOC;
    ka_task_mutex_unlock(ctrl_mutex);

    info->dev_id = dev_id;
    info->vfid = vfid;
    ret = vmngh_vdev_msg_send(info, VMNG_CTRL_MSG_TYPE_ENQUIRE_VF);
    if (ret != 0) {
        vmng_err("Get vf info failed. (dev_id=%u;vfid=%u;ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }
    ret = memcpy_s(&ctrl->memory, sizeof(struct vmng_vf_memory_info),
                   &info->enquire.each.memory, sizeof(struct vmng_vf_memory_info));
    if (ret != 0) {
        vmng_err("Memcpy failed. (ret=%d;dev_id=%u;vfid=%u)\n", ret, dev_id, vfid);
        return ret;
    }

    if (ctrl->memory.number > VMNG_NUMA_MAX_NUM) {
        vmng_err("Memory number is invalid. (number=%u;dev_id=%u;vfid=%u)\n", ctrl->memory.number, dev_id, vfid);
        return -EINVAL;
    }

    return VMNG_OK;
}

STATIC void vmngh_free_vdev_ctrl_cloud_v2(u32 dev_id, u32 vfid)
{
    vmngh_free_vdev_ctrl(dev_id, vfid);
}

STATIC int vmngh_alloc_vf_cloud_v2(u32 dev_id, u32 *fid, u32 dtype, struct vmng_vf_res_info *vf_resource)
{
    struct vmng_ctrl_msg_info info;

    if (fid == NULL) {
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
    if (memcpy_s(&info.vf_cfg, sizeof(struct vmng_vf_res_info), vf_resource, sizeof(struct vmng_vf_res_info)) != EOK) {
        vmng_err("Call memcpy_s failed.\n");
        return VMNG_ERR;
    }
    if (vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_ALLOC_VF) != VMNG_OK) {
        return VMNG_ERR;
    }
    vf_resource->vfg.vfg_id = info.vf_cfg.vfg.vfg_id;

    if (vmngh_alloc_vdev_ctrl_cloud_v2(dev_id, *fid, &info) != VMNG_OK) {
        vmng_err("alloc_vdev_ctrl_cloud_v2 error. (dev_id=%u, vfid=%u, dtype=%u)\n", dev_id, *fid, dtype);
        goto EXIT;
    }

    return VMNG_OK;

EXIT:
    info.dev_id = dev_id;
    info.vfid = *fid;
    (void)vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_FREE_VF);

    return VMNG_ERR;
}

STATIC int vmngh_free_vf_cloud_v2(u32 dev_id, u32 vfid)
{
    struct vmng_ctrl_msg_info info;

    if (vmngh_dev_id_check(dev_id, vfid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    if (memset_s(&info, sizeof(struct vmng_ctrl_msg_info), 0, sizeof(struct vmng_ctrl_msg_info)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    info.dev_id = dev_id;
    info.vfid = vfid;
    if (vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_FREE_VF) != VMNG_OK) {
        vmng_err("Send msg to device failed. (dev_id=%u;vfid=%u)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_container_client_online_cloud_v2(u32 dev_id, u32 vfid)
{
    unsigned int vdev_id;
    int ret;

    ret = vmngh_init_instance_client_device(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Init instance all client device failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return ret;
    }

    (void)devdrv_get_devid_by_pfvf_id(dev_id, vfid, &vdev_id);
    ret = devdrv_sriov_init_instance(vdev_id);
    if (ret != VMNG_OK) {
        (void)vmngh_uninit_instance_client_device(dev_id, vfid);
        vmng_err("pci init instance error. (dev_id=%u;vfid=%u;vdev_id=%u;ret=%d)\n", dev_id, vfid, vdev_id, ret);
        return ret;
    }

    ret = vmngh_init_instance_all_client(dev_id, vfid, VMNGH_CONTAINER);
    if (ret != VMNG_OK) {
        (void)devdrv_sriov_uninit_instance(vdev_id);
        (void)vmngh_uninit_instance_client_device(dev_id, vfid);
        vmng_err("Init instance all client failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return ret;
    }

    ret = vmngh_add_mia_dev(dev_id, vfid, 0);
    if (ret != 0) {
        (void)vmngh_uninit_instance_all_client(dev_id, vfid);
        (void)devdrv_sriov_uninit_instance(vdev_id);
        (void)vmngh_uninit_instance_client_device(dev_id, vfid);
        return ret;
    }

    return VMNG_OK;
}

STATIC int vmngh_container_client_offline_cloud_v2(u32 dev_id, u32 vfid)
{
    u32 vdev_id;
    int ret;

    (void)vmngh_remove_mia_dev(dev_id, vfid, 0);

    ret = vmngh_uninit_instance_all_client(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Uninit instance all client failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
    }

    (void)devdrv_get_devid_by_pfvf_id(dev_id, vfid, &vdev_id);
    if (devdrv_sriov_uninit_instance(vdev_id) != VMNG_OK) {
        (void)vmngh_init_instance_all_client(dev_id, vfid, VMNGH_CONTAINER);
        vmng_err("pci init instance error. (dev_id=%u, vfid=%u, vdev_id=%u)\n", dev_id, vfid, vdev_id);
        return VMNG_ERR;
    }

    ret = vmngh_uninit_instance_client_device(dev_id, vfid);
    if (ret != VMNG_OK) {
        vmng_err("Uninit instance all client device failed. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        (void)devdrv_sriov_init_instance(vdev_id);
        (void)vmngh_init_instance_all_client(dev_id, vfid, VMNGH_CONTAINER);
        return ret;
    }

    return VMNG_OK;
}

STATIC int vmngh_res_enable_sriov_cloud_v2(u32 dev_id)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, 0);
    struct vmng_ctrl_msg_info info;
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; MAX=%d)\n", dev_id, VMNG_PDEV_MAX);
        return VMNG_ERR;
    }

    if (is_sriov_vf_cloud_v2(dev_id) == true) {
        vmng_err("Not support vf enable sriov. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (devdrv_sriov_enable(dev_id) != VMNG_OK) {
        vmng_err("Sriov enable failed. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    ka_system_msleep(1000); // sleep 1000 ms
    info.dev_id = dev_id;
    info.vfid = 0;
    info.sriov_status = VMNGH_PF_SRIOV_ENABLE;
    ret = vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_SRIOV_INFO);
    if (ret != 0) {
        vmng_err("Send vdev msg failed, (dev_id=%u, ret=%d).\n", dev_id, ret);
        (void)devdrv_sriov_disable(dev_id);
        return ret;
    }

    ctrl->sriov_status = VMNGH_PF_SRIOV_ENABLE;
    return VMNG_OK;
}

STATIC bool is_has_vf_running(u32 dev_id)
{
    struct vmngh_vdev_ctrl *ctrl = NULL;
    u32 i;
    int cnt = 0;

    for (i = VMNG_VDEV_FIRST_VFID; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        ctrl = vmngh_get_ctrl(dev_id, i);
        if (ctrl->vdev_ctrl.status != VMNG_VDEV_STATUS_FREE) {
            cnt++;
        }
    }
    return ((cnt != 0) ? true : false);
}

STATIC int vmngh_res_disable_sriov_cloud_v2(u32 dev_id)
{
    struct vmngh_vdev_ctrl *ctrl = vmngh_get_ctrl(dev_id, 0);
    struct vmng_ctrl_msg_info info;
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; MAX=%d)\n", dev_id, VMNG_PDEV_MAX);
        return VMNG_ERR;
    }

    if (is_sriov_vf_cloud_v2(dev_id) == true) {
        vmng_err("Not support vf disable sriov. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (is_has_vf_running(dev_id)) {
        vmng_err("Has VF running. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    ctrl->sriov_status = VMNGH_PF_SRIOV_DISABLE;

    ret = devdrv_sriov_disable(dev_id);
    if (ret != VMNG_OK) {
        vmng_err("Sriov close failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    info.dev_id = dev_id;
    info.vfid = 0;
    info.sriov_status = ctrl->sriov_status;
    ret = vmngh_vdev_msg_send(&info, VMNG_CTRL_MSG_TYPE_SRIOV_INFO);
    if (ret != 0) {
        vmng_err("Send vdev msg failed, (dev_id=%u,ret=%d).\n", dev_id, ret);
        ctrl->sriov_status = VMNGH_PF_SRIOV_ENABLE;
        return ret;
    }

    return VMNG_OK;
}

STATIC int vmngh_enquire_soc_resource_cloud_v2(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info)
{
    int ret;
    u32 pf_id;
    u32 vf_id;
    struct vmng_ctrl_msg_info info_msg;

    if (info == NULL) {
        vmng_err("Info NULL.\n");
        return VMNG_ERR;
    }

    if (devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id) < 0) {
        vmng_err("Get_pfvf_id_by_devid error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (memset_s(&info_msg, sizeof(struct vmng_ctrl_msg_info), 0, sizeof(struct vmng_ctrl_msg_info)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    info_msg.dev_id = dev_id;
    info_msg.vfid = vfid;
    ret = vmngh_vdev_msg_send(&info_msg, VMNG_CTRL_MSG_TYPE_ENQUIRE_VF);
    if (ret != VMNG_OK) {
        vmng_err("Send msg failed. (dev_id=%u; vfid=%u)\n", info_msg.dev_id, info_msg.vfid);
        return VMNG_ERR;
    }

    ret = memcpy_s(info, sizeof(struct vmng_soc_resource_enquire), &info_msg.enquire,
        sizeof(struct vmng_soc_resource_enquire));
    if (ret != EOK) {
        vmng_err("Call memcpy_s err.(dev_id=%u; vfid=%u)\n", info_msg.dev_id, info_msg.vfid);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_refresh_vdev_resource_cloud_v2(u32 dev_id, u32 vfid, struct vmng_soc_resource_refresh *info)
{
    int ret;
    struct vmng_ctrl_msg_info info_msg;

    if (info == NULL) {
        vmng_err("Info NULL.\n");
        return VMNG_ERR;
    }

    if (dev_id >= VMNG_PDEV_MAX || vfid > VMNG_VF_MAX_PER_PF) {
        vmng_err("Input parameter error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (!is_sriov_enable(dev_id)) {
        vmng_err("Sriov not enable. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if (memset_s(&info_msg, sizeof(struct vmng_ctrl_msg_info), 0, sizeof(struct vmng_ctrl_msg_info)) != EOK) {
        vmng_err("Call memset_s failed.\n");
        return VMNG_ERR;
    }

    ret = memcpy_s(&info_msg.refresh, sizeof(struct vmng_soc_resource_refresh), info,
        sizeof(struct vmng_soc_resource_refresh));
    if (ret != EOK) {
        vmng_err("Call memcpy_s err.(dev_id=%u; vfid=%u; vdev_id=%u)\n", dev_id, vfid, dev_id);
        return VMNG_ERR;
    }

    info_msg.dev_id = dev_id;
    info_msg.vfid = vfid;
    ret = vmngh_vdev_msg_send(&info_msg, VMNG_CTRL_MSG_TYPE_REFRESH_VF);
    if (ret != VMNG_OK) {
        vmng_err("Send msg failed. (dev_id=%u; vfid=%u;)\n", dev_id, vfid);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_check_pci_reset_finish(u32 dev_id)
{
    int count = 0;

    while ((devdrv_check_half_probe_finish(dev_id) == false) && (count < VMNG_WAIT_REMOTE_ID_S)) {
        ka_system_msleep(1000); // sleep 1000 ms
        count++;
    }

    if (count >= VMNG_WAIT_REMOTE_ID_S) {
        vmng_err("flr_finish >= %ds (dev_id=%d)\n", VMNG_WAIT_REMOTE_ID_S, dev_id);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_sriov_reset_vdev_cloud_v2(u32 dev_id, u32 vfid)
{
    u32 pf_id;
    u32 vf_id;
    int ret = 0;

    vmng_info("vmngh_sriov_reset_vdev.(dev_id=%u, vfid=%u)\n", dev_id, vfid);

    if (devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id) < 0) {
        vmng_err("Get pfvf id by devid error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }

    if ((pf_id >= VMNG_PDEV_MAX) || (vf_id >= VMNG_VDEV_MAX_PER_PDEV)) {
        vmng_err("Get pf_id or vf_id invalid. (dev_id=%u;pf_id=%u;vf_id=%u)\n", dev_id, pf_id, vf_id);
        return -EINVAL;
    }

    if (!is_sriov_enable(pf_id)) {
        vmng_err("Sriov not enable. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }
    ka_task_mutex_lock(&vmngh_get_ctrl(pf_id, vf_id)->reset_mutex);

    ret = vmngh_container_client_offline_cloud_v2(pf_id, vf_id);
    if (ret != 0) {
        ka_task_mutex_unlock(&vmngh_get_ctrl(pf_id, vf_id)->reset_mutex);
        vmng_err("Vf device offline failed.(dev_id=%u;vfid=%u;ret=%d)\n", pf_id, vf_id, ret);
        return ret;
    }

    if (devdrv_hot_reset_device(dev_id) < 0) {
        vmng_err("devdrv_hot_reset_device error.(dev_id=%u)\n", dev_id);
        ret = VMNG_ERR;
        ka_system_ssleep(VMNG_WAIT_RESET_S);  // wait for device RM process finish
        goto vf_recover;
    }

    if (vmngh_check_pci_reset_finish(dev_id) != VMNG_OK) {
        vmng_err("Check pci reset finish timeout. (dev_id=%u)\n", dev_id);
    }
vf_recover:
    if (vmngh_init_instance_client_device(pf_id, vf_id) != VMNG_OK) {
        vmng_err("Init instance all client device failed. (dev_id=%u; vfid=%u)\n", pf_id, vf_id);
        ret = VMNG_ERR;
    }

    if (devdrv_sriov_init_instance(dev_id) != VMNG_OK) {
        vmng_err("pci init instance error. (dev_id=%u;vfid=%u;vdev_id=%u;ret=%d)\n", pf_id, vf_id, dev_id, ret);
        ret = VMNG_ERR;
    }

    if (vmngh_init_instance_all_client(pf_id, vf_id, VMNGH_CONTAINER) != VMNG_OK) {
        vmng_err("Init instance all client failed. (dev_id=%u; vfid=%u)\n", pf_id, vf_id);
        ret = VMNG_ERR;
    }

    if (vmngh_add_mia_dev(pf_id, vf_id, 0) != 0) {
        ret = VMNG_ERR;
        vmng_err("Call vmngh_add_mia_dev failed. (dev_id=%u; vfid=%u)\n", pf_id, vf_id);
    }

    ka_task_mutex_unlock(&vmngh_get_ctrl(pf_id, vf_id)->reset_mutex);
    ka_system_ssleep(VMNG_WAIT_RESET_S);

    return ret;
}

int vmngh_res_init_cloud_v2(struct vmngh_ctrl_ops *ops)
{
    if (ops == NULL) {
        vmng_err("Invalid parameter.\n");
        return VMNG_ERR;
    }

    ops->enable_sriov = vmngh_res_enable_sriov_cloud_v2;
    ops->disable_sriov = vmngh_res_disable_sriov_cloud_v2;
    ops->alloc_vf = vmngh_alloc_vf_cloud_v2;
    ops->free_vf = vmngh_free_vf_cloud_v2;
    ops->alloc_vfid = vmngh_alloc_vfid_cloud_v2;
    ops->free_vfid = vmngh_free_vdev_ctrl_cloud_v2;
    ops->refresh_vf = vmngh_refresh_vdev_resource_cloud_v2;
    ops->enquire_vf = vmngh_enquire_soc_resource_cloud_v2;
    ops->reset_vf = vmngh_sriov_reset_vdev_cloud_v2;
    ops->container_client_online = vmngh_container_client_online_cloud_v2;
    ops->container_client_offline = vmngh_container_client_offline_cloud_v2;
    ops->is_vf = is_sriov_vf_cloud_v2;
    ops->unify_devid_to_pfvf_id = devdrv_get_pfvf_id_by_devid;
    ops->valid = VMNG_VALID;

    return VMNG_OK;
}

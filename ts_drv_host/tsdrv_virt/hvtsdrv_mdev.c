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
#include "hvtsdrv_init.h"
#ifdef CFG_FEATURE_VFIO
#include <linux/vmalloc.h>

#include "securec.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "tsdrv_sync.h"
#include "tsdrv_nvme.h"
#include "devdrv_cb.h"
#include "devdrv_cqsq.h"

#include "devdrv_manager_common.h"
#include "devdrv_interface.h"
#include "virtmng_interface.h"
#include "davinci_interface.h"
#include "davinci_api.h"

#include "hvtsdrv_proc.h"
#include "hvtsdrv_tsagent.h"
#include "hvtsdrv_id.h"
#include "hvtsdrv_cqsq.h"
#include "hvtsdrv_cb.h"

int hvtsdrv_instance_check(struct vmngh_client_instance *instance)
{
    if (instance == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The instance is NULL.\n");
        return -EINVAL;
#endif
    }

    if (instance->dev_ctrl == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The instance dev_ctrl is NULL.\n");
        return -EINVAL;
#endif
    }

    if (instance->type != VMNG_CLIENT_TYPE_TSDRV) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The instance type is not correct. (type=%d)\n", instance->type);
        return -EINVAL;
#endif
    }

    if (instance->dev_ctrl->dev_id >= TSDRV_MAX_DAVINCI_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The devid is invalid. (devid=%u)\n", instance->dev_ctrl->dev_id);
        return -EINVAL;
#endif
    }

    if (instance->dev_ctrl->dtype >= INVALID_CAPACITY) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The dtype is invalid. (dtype=%d)\n", instance->dev_ctrl->dtype);
        return -EINVAL;
#endif
    }

    if ((instance->dev_ctrl->vfid >= TSDRV_MAX_FID_NUM) || (instance->dev_ctrl->vfid == TSDRV_PM_FID)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The fid is invalid. (fid=%u)\n", instance->dev_ctrl->vfid);
        return -EINVAL;
#endif
    }

    if (!tsdrv_pm_ctx_is_finished(instance->dev_ctrl->dev_id)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Device in pm has process running, not support. (devid=%u)\n", instance->dev_ctrl->dev_id);
        return -EINVAL;
#endif
    }
    TSDRV_PRINT_INFO("(devid=%u; fid=%u; dtype=%d)\n", instance->dev_ctrl->dev_id,
        instance->dev_ctrl->vfid, instance->dev_ctrl->dtype);

    return 0;
}

void hvtsdrv_vm_res_recycle_work(ka_work_struct_t *work)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    struct tsdrv_device *tsdrv_dev = NULL;
    u32 dev_id, fid;
    u32 fid_num = 0;
    int ret;

    dev_res = ka_container_of(work, struct tsdrv_dev_resource, vm_res_recycle_work);
    tsdrv_dev = ka_container_of(dev_res, struct tsdrv_device, dev_res[dev_res->fid]);
    dev_id = tsdrv_dev->devid;
    fid = dev_res->fid;
    /* If TS exception occurs, some resources fail to be reclaimed */
    tsdrv_wait_dev_ids_recycle_complete(dev_id, fid);

#ifndef TSDRV_UT
    tsdrv_notice_tsagent_dev_destroy(dev_id, fid);
#endif

    hvtsdrv_dev_logic_cq_uninit(dev_id, fid);
    if (tsdrv_is_in_container(dev_id)) {
        hvtsdrv_resource_uninit(dev_id, fid, TSDRV_CONTAINER_TYPE);
        fid_num = tsdrv_get_fid_num(dev_id);
        if (fid_num == 1) {
            ret = hvtsdrv_phy_cbcqsq_init(dev_id, 0);
            if (ret != 0) {
                TSDRV_PRINT_ERR("Failed to init pf cbsqcq. (devid=%u; ret=%d)\n", dev_id, ret);
                return;
            }
        }
    } else {
        u32 tsid;
        for (tsid = 0; tsid < tsdrv_get_dev_tsnum(dev_id); tsid++) {
            hvtsdrv_sq_mem_uninit(dev_id, fid, tsid, TSDRV_VIRTUAL_TYPE);
        }
    }

    TSDRV_PRINT_INFO("Success to uninit hvtsdrv. (devid=%u; fid=%u; fid_num=%d)\n", dev_id, fid, fid_num);
    return;
}

static int hvtsdrv_get_container_shm_mem(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    u32 chip_type = devdrv_get_dev_chip_type(devid);
    struct devdrv_ts_sq_info *sq_info = NULL;
    int ret, sqid, i, j;

    if (chip_type != HISI_MINI_V2) {
        return 0;
    }

    for (i = 0; i < TSRDV_SHM_SQ_NUM; i++) {
        sqid = tsdrv_kernel_alloc_sq(devid, fid, tsid, DEVDRV_SQ_SLOT_SIZE, DEVDRV_MAX_SQ_DEPTH, SHM_SQCQ_TYPE);
        if (sqid < 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Failed to alloc sq. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
            ret = -EFAULT;
            goto err_alloc_sq;
#endif
        }
        sq_info = tsdrv_get_sq_info(devid, fid, tsid, sqid);
        if (sq_info == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Failed to get sq_info. (devid=%u; fid=%u; tsid=%u; sqid=%d)\n", devid, fid, tsid, sqid);
            ret = -EINVAL;
            i++;
            goto err_alloc_sq;
#endif
        }
        ts_res->shm_phy_sq[i] = sqid;
    }
    ts_res->shm_map_addr = NULL;
    ts_res->shm_offset = 0;
    TSDRV_PRINT_INFO("The hvtsdrv_get_container_shm_mem success. (devid=%u; fid=%u; tsid=%u; sqid=%u)\n",
        devid, fid, tsid, sqid);
    return 0;

#ifndef TSDRV_UT
err_alloc_sq:
    for (j = 0; j < i; j++) {
        tsdrv_kernel_free_sq(devid, fid, tsid, ts_res->shm_phy_sq[j]);
    }
    return ret;
#endif
}

static void hvtsdrv_put_container_shm_mem(u32 devid, u32 fid, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(devid, fid, tsid);
    u32 chip_type = devdrv_get_dev_chip_type(devid);
    int i;

    if (chip_type != HISI_MINI_V2) {
        return;
    }

    for (i = 0; i < TSRDV_SHM_SQ_NUM; i++) {
        tsdrv_kernel_free_sq(devid, fid, tsid, ts_res->shm_phy_sq[i]);
    }
}

static int hvtsdrv_container_resource_init(u32 devid, u32 fid, enum TSDRV_CAPACITY tsdrv_capacity)
{
    u32 tsid, i;
    int ret;

    ret = hvtsdrv_resource_init(devid, fid, tsdrv_capacity, TSDRV_CONTAINER_TYPE);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to init resource. (devid=%u; fid=%u; dtype=%d)\n", devid, fid, tsdrv_capacity);
        return ret;
#endif
    }

    ret = hvtsdrv_dev_logic_cq_init(devid, fid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("hvtsdrv dev-%u fid-%u id logic cq init fail, ret-%d.\n", devid, fid, ret);
        goto logic_cq_init_err;
#endif
    }

    ret = callback_dev_init(devid, fid, tsdrv_get_dev_tsnum(devid));
    if (ret != 0) {
#ifndef TSDRV_UT
        goto err_callback_dev_init;
#endif
    }

    for (tsid = 0; tsid < DEVDRV_MAX_TS_NUM; tsid++) {
        ret = hvtsdrv_get_container_shm_mem(devid, fid, tsid);
        if (ret != 0) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Failed to get share memory. (devid=%u; fid=%u; tsid=%u)\n", devid, fid, tsid);
            goto get_container_shm_fail;
#endif
        }
    }

    return 0;

#ifndef TSDRV_UT
get_container_shm_fail:
    for (i = 0; i < tsid; i++) {
        hvtsdrv_put_container_shm_mem(devid, fid, i);
    }
    callback_dev_exit(devid, fid, tsdrv_get_dev_tsnum(devid));
err_callback_dev_init:
    hvtsdrv_dev_logic_cq_uninit(devid, fid);
logic_cq_init_err:
    hvtsdrv_resource_uninit(devid, fid, TSDRV_CONTAINER_TYPE);
    return ret;
#endif
}

#ifndef TSDRV_UT
static void hvtsdrv_container_resource_uninit(u32 dev_id, u32 fid)
{
    u32 ts_num, tsid;

    ts_num = tsdrv_get_dev_tsnum(dev_id);
    for (tsid = 0; tsid < ts_num; tsid++) {
        hvtsdrv_put_container_shm_mem(dev_id, fid, tsid);
    }

    callback_dev_exit(dev_id, fid, ts_num);
    hvtsdrv_dev_logic_cq_uninit(dev_id, fid);
    hvtsdrv_resource_uninit(dev_id, fid, TSDRV_CONTAINER_TYPE);
}
#endif

int hvtsdrv_init_mia_dev_instance(u32 dev_id, u32 fid, u32 dtype)
{
    int ret;

    if (tsdrv_is_in_vm(dev_id)) {
        TSDRV_PRINT_WARN("Not support vm. (devid=%u; fid=%u; host_flag=%d)\n",
            dev_id, fid, tsdrv_get_host_flag(dev_id));
        return -EINVAL;
    }

    ret = hvtsdrv_phy_cbcqsq_exit(dev_id, 0);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to exit callback sqcq. (devid=%u)\n", dev_id);
        return ret;
    }

    ret = hvtsdrv_container_resource_init(dev_id, fid, (enum TSDRV_CAPACITY)dtype);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to init container resource. (devid=%u; fid=%u)\n", dev_id, fid);
#ifndef TSDRV_UT
        goto resource_init_fail;
#endif
    }

    dev_mnt_vdev_register_client(dev_id, fid, hvtsdrv_get_vdavinci_fop());
    tsdrv_set_dev_status(dev_id, fid, TSDRV_DEV_ACTIVE, TSDRV_CONTAINER_TYPE);

#ifndef TSDRV_UT
    ret = tsdrv_notice_tsagent_dev_create(dev_id, fid);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to create vf. (devid=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto tsagent_ops_fail;
    }
#endif

    ret = hvtsdrv_dev_recycle_wq_init(dev_id, fid);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to init recycle workqueue. (devid=%u; fid=%u; ret=%d)\n", dev_id, fid, ret);
        goto tsagent_ops_fail;
    }

    TSDRV_PRINT_INFO("Init container success. (devid=%u; fid=%u)\n", dev_id, fid);
    return 0;

tsagent_ops_fail:
    dev_mnt_vdev_unregister_client(dev_id, fid);

#ifndef TSDRV_UT
    hvtsdrv_container_resource_uninit(dev_id, fid);
resource_init_fail:
    hvtsdrv_phy_cbcqsq_init(dev_id, 0);
#endif

    return ret;
}

int hvtsdrv_uninit_mia_dev_instance(u32 dev_id, u32 fid)
{
    struct tsdrv_dev_resource *dev_res = NULL;
    enum tsdrv_dev_status status;
    u32 ts_num, tsid;

    status = tsdrv_get_dev_status(dev_id, fid);
    if (status != TSDRV_DEV_ACTIVE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("The hvtsdrv status is not active. (devid=%u; fid=%u; status=%u)\n", dev_id, fid, status);
        return -EINVAL;
#endif
    }

    dev_res = tsdrv_get_dev_resource(dev_id, fid);
    if (dev_res == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to get device resource. (devid=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
#endif
    }

    ka_base_atomic_set(&dev_res->status, TSDRV_DEV_REBOOTING);

    dev_mnt_vdev_unregister_client(dev_id, fid);
    /* callback must exit before cqsq_uninit */
    ts_num = tsdrv_get_dev_tsnum(dev_id);

    callback_dev_exit_for_container(dev_id, fid, ts_num);

    for (tsid = 0; tsid < ts_num; tsid++) {
        hvtsdrv_put_container_shm_mem(dev_id, fid, tsid);
    }

    ka_task_mutex_lock(&dev_res->dev_res_lock);
    if (dev_res->vm_res_wq != NULL) {
        (void)ka_task_queue_work(dev_res->vm_res_wq, &dev_res->vm_res_recycle_work);
    }
    ka_task_mutex_unlock(&dev_res->dev_res_lock);

    return 0;
}
#endif


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

#include "virtmnghost_external.h"
#include "virtmnghost_unit.h"
#include "virtmnghost_ctrl.h"
#include "hw_vdavinci.h"
#include "virtmng_resource.h"
#include "virtmng_public_def.h"
#include "devdrv_interface.h"
#include "virtmng_interface.h"
#include "vmng_mem_alloc_interface.h"

#include "ka_kernel_def_pub.h"


/* doorbell for external module */
int vmngh_alloc_external_db_entries(struct vmngh_vd_dev *vd_dev)
{
    struct vmngh_db_mng *db_mng = NULL;
    struct vmngh_db_entry *db_info = NULL;
    u32 i;

    db_mng = &(vd_dev->db_mng);
    db_mng->db_num = VMNG_DB_NUM_EXTERNAL;
    db_mng->db_entries = (struct vmngh_db_entry *)vmng_kzalloc(sizeof(struct vmngh_db_entry) * db_mng->db_num, KA_GFP_KERNEL);
    if (db_mng->db_entries == NULL) {
        vmng_err("Alloc db entries failed. (dev_id=%u; fid=%u)\n", vd_dev->dev_id, vd_dev->fid);
        return -ENOMEM;
    }
    for (i = 0; i < db_mng->db_num; i++) {
        db_info = &(db_mng->db_entries[i]);
        db_info->index = VMNG_DB_BASE_EXTERNAL + i;
        db_info->handler = NULL;
        db_info->data = NULL;
        db_info->db_count = 0;
    }
    return 0;
}

void vmngh_free_external_db_entries(struct vmngh_vd_dev *vd_dev)
{
    if ((vd_dev != NULL) && (vd_dev->db_mng.db_entries != NULL)) {
        vmng_kfree(vd_dev->db_mng.db_entries);
        vd_dev->db_mng.db_entries = NULL;
    }
}

int vmngh_get_local_db(u32 dev_id, u32 fid, enum vmng_get_irq_type type, u32 *db_base, u32 *db_num)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (db_base == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (db_num == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    switch (type) {
        case VMNG_GET_IRQ_TYPE_TSDRV:
            *db_base = VMNG_DB_BASE_EXTERNAL_TSDRV;
            *db_num = VMNG_DB_NUM_EXTERNAL_TSDRV;
            break;
        default:
            vmng_err("type is overflow. (dev_id=%u; fid=%u; type=%u)\n", dev_id, fid, type);
            return -EINVAL;
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_get_local_db);

int vmngh_register_local_db(u32 dev_id, u32 fid, u32 db_index, db_handler_t handler, void *data)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    struct vmngh_db_mng *db_mng = NULL;

    if (handler == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if ((db_index >= VMNG_DB_BASE_EXTERNAL_MAX) || (db_index < VMNG_DB_BASE_EXTERNAL_MIN)) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; db_index=%u)\n", dev_id, fid, db_index);
        return -EINVAL;
    }
    db_index = db_index - VMNG_DB_BASE_EXTERNAL_MIN;

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("mdev is not ready. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    db_mng = &(vd_dev->db_mng);
    if (db_index >= db_mng->db_num) {
        vmng_err("db_index is error. (dev_id=%u; fid=%u; db_index=%u)\n",
                 dev_id, fid, db_index + VMNG_DB_BASE_EXTERNAL_MIN);
        return -EINVAL;
    }
    db_mng->db_entries[db_index].handler = handler;
    db_mng->db_entries[db_index].data = data; /* null data is allowed */

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_register_local_db);

/* when other module exit, must unregister db irq.
 * otherwise vpc will invoke unknow func pointer, as vm still send db to pm.
 */
int vmngh_unregister_local_db(u32 dev_id, u32 fid, u32 db_index, const void *data)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    struct vmngh_db_mng *db_mng = NULL;

    if ((db_index >= VMNG_DB_BASE_EXTERNAL_MAX) || (db_index < VMNG_DB_BASE_EXTERNAL_MIN)) {
        vmng_err("db_index is error. (dev_id=%u; fid=%u; db_index=%u)\n", dev_id, fid, db_index);
        return -EINVAL;
    }
    db_index = db_index - VMNG_DB_BASE_EXTERNAL_MIN;

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("mdev is not ready. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    db_mng = &(vd_dev->db_mng);
    if (db_index >= db_mng->db_num) {
        vmng_err("db_index is error. (dev_id=%u; fid=%u; db_index=%u)\n", dev_id, fid, db_index);
        return -EINVAL;
    }
    if (db_mng->db_entries[db_index].data != data) {
        vmng_err("Irq data is error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    db_mng->db_entries[db_index].handler = NULL;
    db_mng->db_entries[db_index].data = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmngh_unregister_local_db);

int vmngh_external_db_handler(struct vmngh_vd_dev *vd_dev, int db_index)
{
    struct vmngh_db_mng *db_mng = NULL;
    struct vmngh_db_entry *db_info = NULL;

    db_mng = &(vd_dev->db_mng);
    db_info = &(db_mng->db_entries[db_index - VMNG_DB_BASE_EXTERNAL_MIN]);

    db_info->db_count++;
    if (db_info->handler == NULL) {
        vmng_err("handler is NULL. (dev_id=%u; fid=%u; db_index=%u)\n", vd_dev->dev_id, vd_dev->fid, db_index);
        return -EINVAL;
    }

    return db_info->handler(db_index, db_info->data);
}

int vmngh_get_remote_msix(u32 dev_id, u32 fid, enum vmng_get_irq_type type, u32 *msix_base, u32 *msix_num)
{
    if (vmngh_dev_id_check(dev_id, fid) != 0) {
        vmng_err("Parameter check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }
    if (msix_base == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; type=%u)\n", dev_id, fid, type);
        return -EINVAL;
    }
    if (msix_num == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; type=%u)\n", dev_id, fid, type);
        return -EINVAL;
    }

    switch (type) {
        case VMNG_GET_IRQ_TYPE_TSDRV:
            *msix_base = VMNG_MSIX_BASE_EXTERNAL_TSDRV;
            *msix_num = VMNG_MSIX_NUM_EXTERNAL_TSDRV;
            break;
        default:
            vmng_err("Parameter type is overflow. (dev_id=%u; fid=%u; type=%u)\n", dev_id, fid, type);
            return -EINVAL;
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_get_remote_msix);

int vmngh_trigger_remote_msix(u32 dev_id, u32 fid, u32 msix_index)
{
    struct vmngh_vd_dev *vd_dev = NULL;
    u32 msix_offset;
    int ret;

    if ((msix_index >= VMNG_MSIX_BASE_EXTERNAL_MAX) || (msix_index < VMNG_DB_BASE_EXTERNAL_MIN)) {
        vmng_err("Input parameter is error. (dev_id=%u; fid=%u; msix_index=%u)\n", dev_id, fid, msix_index);
        return -EINVAL;
    }

    vd_dev = vmngh_get_bottom_half_vdev_by_id(dev_id, fid);
    if (vd_dev == NULL) {
        vmng_err("mdev is not ready. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return -EINVAL;
    }

    msix_offset = vd_dev->shr_para->msix_offset;
    ret = hw_dvt_hypervisor_inject_msix(vd_dev->vdavinci, msix_index + msix_offset);
    if (ret != 0) {
        vmng_err("Send msix to remote failed. (dev_id=%u; mdev_id=%u; msix=%u)\n", dev_id, fid, msix_index);
        return -ENODEV;
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_trigger_remote_msix);


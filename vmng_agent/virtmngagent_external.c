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

#include "virtmngagent_ctrl.h"
#include "virtmng_public_def.h"
#include "virtmng_interface.h"
#include "virtmng_resource.h"

#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"

int vmnga_get_remote_db(u32 dev_id, enum vmng_get_irq_type type, u32 *db_base, u32 *db_num)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (db_base == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; type=%u)\n", dev_id, type);
        return -EINVAL;
    }
    if (db_num == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; type=%u)\n", dev_id, type);
        return -EINVAL;
    }

    switch (type) {
        case VMNG_GET_IRQ_TYPE_TSDRV:
            *db_base = VMNG_DB_BASE_EXTERNAL_TSDRV;
            *db_num = VMNG_DB_NUM_EXTERNAL_TSDRV;
            break;
        default:
            vmng_err("Input parameter is error. (dev_id=%u; type=%u)\n", dev_id, type);
            return -EINVAL;
    }

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_get_remote_db);

int vmnga_trigger_remote_db(u32 dev_id, u32 db_index)
{
    struct vmnga_unit *unit = NULL;

    if ((db_index < VMNG_DB_BASE_EXTERNAL_MIN) || (db_index >= VMNG_DB_BASE_EXTERNAL_MAX)) {
        vmng_err("Input parameter is error. (dev_id=%u; db_index=%u)\n", dev_id, db_index);
        return -EINVAL;
    }

    unit = vmnga_get_bottom_half_unit_by_id(dev_id);
    if (unit == NULL) {
        vmng_err("Get unit failed. (dev_id=%u; db_index=%u)\n", dev_id, db_index);
        return -ENODEV;
    }
    vmnga_set_doorbell(unit->db_base, db_index, 1);

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_trigger_remote_db);

int vmnga_get_local_msix(u32 dev_id, enum vmng_get_irq_type type, u32 *msix_base, u32 *msix_num)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (msix_base == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; type=%u)\n", dev_id, type);
        return -EINVAL;
    }
    if (msix_num == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; type=%u)\n", dev_id, type);
        return -EINVAL;
    }

    switch (type) {
        case VMNG_GET_IRQ_TYPE_TSDRV:
            *msix_base = VMNG_MSIX_BASE_EXTERNAL_TSDRV;
            *msix_num = VMNG_MSIX_NUM_EXTERNAL_TSDRV;
            break;
        default:
            vmng_err("Input parameter is error. (dev_id=%u; type=%u)\n", dev_id, type);
            return -EINVAL;
    }

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_get_local_msix);

int vmnga_register_local_msix(u32 dev_id, u32 msix_index, ka_irq_handler_t handler, void *data, const char *name)
{
    struct vmnga_unit *unit = NULL;

    if ((msix_index < VMNG_MSIX_BASE_EXTERNAL_MIN) || (msix_index >= VMNG_MSIX_BASE_EXTERNAL_MAX)) {
        vmng_err("Input parameter is error. (dev_id=%u; msix_index=%u)\n", dev_id, msix_index);
        return -EINVAL;
    }

    unit = vmnga_get_bottom_half_unit_by_id(dev_id);
    if (unit == NULL) {
        vmng_err("Get unit failed. (dev_id=%u; msix_index=%u)\n", dev_id, msix_index);
        return -EINVAL;
    }
    return vmnga_register_irq_func((void *)unit, msix_index, handler, data, name);
}
KA_EXPORT_SYMBOL(vmnga_register_local_msix);

int vmnga_unregister_local_msix(u32 dev_id, u32 msix_index, void *data)
{
    struct vmnga_unit *unit = NULL;
    int ret;

    if ((msix_index < VMNG_MSIX_BASE_EXTERNAL_MIN) || (msix_index) >= VMNG_MSIX_BASE_EXTERNAL_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u; msix_index=%u)\n", dev_id, msix_index);
        return -EINVAL;
    }

    unit = vmnga_get_top_half_unit_by_id(dev_id);
    if (unit == NULL) {
        vmng_err("Get unit failed. (dev_id=%u; msix_index=%u)\n", dev_id, msix_index);
        return -ENODEV;
    }

    ret = (vmnga_unregister_irq_func((void *)unit, msix_index, data));
    if (ret != 0) {
        vmng_err("Unregister failed. (dev_id=%u; msix_index=%u)\n", dev_id, msix_index);
        return -EINVAL;
    }
    return 0;
}
KA_EXPORT_SYMBOL(vmnga_unregister_local_msix);

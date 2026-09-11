/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
* Create: 2025-07-11
*/

#include <linux/spinlock.h>
#include <linux/rwlock_types.h>
#include <linux/rwlock.h>

#include "ka_task_pub.h"

#include "securec.h"
#include "dbl/uda.h"
#include "dbl/feature_loader.h"
#include "dms_cmd_def.h"
#include "dms_define.h"
#include "ascend_platform.h"
#include "devdrv_user_common.h"

#include "dms_sdk_ex_version.h"

#ifndef STATIC_SKIP
#define STATIC static
#else
#define STATIC
#endif

#define DMS_SDK_EX_VER_NOTIFIER "sdk_ex_ver"

#define DMS_OP_GET_SDK_EX_VERSION       0
#define DMS_OP_SET_SDK_EX_VERSION       1
#define DMS_OP_CLEAR_SDK_EX_VERSION     2

#define ASCEND_VDEV_EX_VER_MAX_NUM      (ASCEND_DEVICE_DAVINCI_NUM_MAX * VDAVINCI_MAX_VFID_NUM)

static rwlock_t g_sdk_ex_ver_lock;
char sdk_pf_ex_version[ASCEND_DEVICE_DAVINCI_NUM_MAX][DMS_SDK_EX_VERSION_LEN_MAX + 1];
char sdk_vf_ex_version[ASCEND_VDEV_EX_VER_MAX_NUM][DMS_SDK_EX_VERSION_LEN_MAX + 1];

STATIC int dms_get_sdk_ex_ver_pointer(unsigned int udevid, char **p_ver)
{
    int offset = 0;

    if (uda_is_phy_dev(udevid)) {
        offset = udevid;
        if ((offset >= ASCEND_DEVICE_DAVINCI_NUM_MAX) || (offset < 0)) {
            dms_err("Invalid parameter. (udevid=%u; offset=%d)", udevid, offset);
            return -EINVAL;
        }
        *p_ver = sdk_pf_ex_version[offset];
    } else {
        offset = udevid - 32; /* 32: ASCEND_VDEV_ID_START in device side */
        if ((offset >= ASCEND_VDEV_EX_VER_MAX_NUM) || (offset < 0)) {
            dms_err("Invalid parameter. (udevid=%u; offset=%d)", udevid, offset);
            return -EINVAL;
        }
        *p_ver = sdk_vf_ex_version[offset];
    }
    return 0;
}

STATIC int dms_operate_sdk_ex_version(int opcode, unsigned int udevid, char *buff, unsigned int *len)
{
    int ret;
    char *p_ver = NULL;

    ret = dms_get_sdk_ex_ver_pointer(udevid, &p_ver);
    if ((ret != 0) || (p_ver == NULL)) {
        dms_err("Invalid parameter. (udevid=%u)", udevid);
        return -EINVAL;
    }

    if (opcode == DMS_OP_GET_SDK_EX_VERSION) {
        ka_task_read_lock(&g_sdk_ex_ver_lock);
        *len = (u32)strnlen(p_ver, DMS_SDK_EX_VERSION_LEN_MAX);
        if (*len != 0) {
            ret = memcpy_s(buff, DMS_SDK_EX_VERSION_LEN_MAX, p_ver, *len);
            if (ret != 0) {
                dms_err("Failed to invoke memcpy_s. (ret=%d)\n", ret);
                ret = -ENOMEM;
            }
        } else {
            /* no set version */
            ret = 0;
        }
        ka_task_read_unlock(&g_sdk_ex_ver_lock);
    } else if (opcode == DMS_OP_SET_SDK_EX_VERSION) {
        ka_task_write_lock(&g_sdk_ex_ver_lock);
        ret = memcpy_s(p_ver, DMS_SDK_EX_VERSION_LEN_MAX, buff, *len);
        if (ret != 0) {
            dms_err("Failed to invoke memcpy_s. (ret=%d)", ret);
            ret = -ENOMEM;
        } else {
            p_ver[*len] = '\0';
            dms_event("Set SDK Ex success. (udevid=%u; ver=%s)\n", udevid, p_ver);
        }
        ka_task_write_unlock(&g_sdk_ex_ver_lock);
    } else if (opcode == DMS_OP_CLEAR_SDK_EX_VERSION) {
        ka_task_write_lock(&g_sdk_ex_ver_lock);
        ret = memset_s(p_ver, DMS_SDK_EX_VERSION_LEN_MAX + 1, 0, DMS_SDK_EX_VERSION_LEN_MAX + 1);
        if (ret != 0) {
            dms_warn("Can not clear sdk ex version. (udevid=%u)", udevid);
        }
        ka_task_write_unlock(&g_sdk_ex_ver_lock);
        ret = 0;
    } else {
        ret = -EINVAL;
    }

    return ret;
}

int dms_get_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int udevid, vf_id, soc_type;
    struct dms_hal_device_info_stru *cfg_in = NULL;
    struct dms_hal_device_info_stru *cfg_out = NULL;

    if ((feature == NULL) ||
        (in == NULL) || (in_len != (DMS_HAL_DEV_INFO_HEAD_LEN + DMS_SDK_EX_VERSION_LEN_MAX)) ||
        (out == NULL) || (out_len != (DMS_HAL_DEV_INFO_HEAD_LEN + DMS_SDK_EX_VERSION_LEN_MAX))) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
            (feature == NULL) ? "NULL" : "OK",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }

    cfg_in = (struct dms_hal_device_info_stru *)in;
    if (cfg_in->buff_size != DMS_SDK_EX_VERSION_LEN_MAX) {
        dms_err("Invalid parameter. (buff_size=%u; buff_max_len=%u)\n", cfg_in->buff_size, DMS_SDK_EX_VERSION_LEN_MAX);
        return -EINVAL;
    }
    cfg_out = (struct dms_hal_device_info_stru *)out;

    ret = uda_devid_to_phy_devid(cfg_in->dev_id, &udevid, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id. (logical_id=%u; ret=%d)\n", cfg_in->dev_id, ret);
        return -EINVAL;
    }

    /* 310B & 310P & 910 not support vf */
    soc_type = uda_get_chip_type(udevid);
    if ((soc_type == HISI_CLOUD_V1) || (soc_type == HISI_MINI_V2) || (soc_type == HISI_MINI_V3)) {
        if ((vf_id != 0) || !uda_is_phy_dev(udevid)) {
            return -EOPNOTSUPP;
        }
    }

    ret = dms_operate_sdk_ex_version(DMS_OP_GET_SDK_EX_VERSION, udevid, cfg_out->payload, &cfg_out->buff_size);
    if (ret != 0) {
        dms_err("Failed to get SDK Ex version. (udevid=%u; ret=%d)\n", udevid, ret);
    }
    return ret;
}

int dms_set_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int udevid, vf_id, soc_type;
    struct dms_hal_device_info_stru *cfg_in = NULL;

    if ((feature == NULL) ||
        (in == NULL) || (in_len < DMS_HAL_DEV_INFO_HEAD_LEN) ||
        (out == NULL) || (out_len < DMS_HAL_DEV_INFO_HEAD_LEN)) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
            (feature == NULL) ? "NULL" : "OK",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }

    cfg_in = (struct dms_hal_device_info_stru *)in;
    if ((cfg_in->buff_size == 0) || (cfg_in->buff_size > DMS_SDK_EX_VERSION_LEN_MAX) ||
        (in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + cfg_in->buff_size))) {
        dms_err("Invalid parameter. (in_len=%u; buff_size=%u)\n", in_len, cfg_in->buff_size);
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(cfg_in->dev_id, &udevid, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id. (logical_id=%u; ret=%d)\n", cfg_in->dev_id, ret);
        return -EINVAL;
    }

    /* 310B & 310P & 910 not support vf */
    soc_type = uda_get_chip_type(udevid);
    if ((soc_type == HISI_CLOUD_V1) || (soc_type == HISI_MINI_V2) || (soc_type == HISI_MINI_V3)) {
        if ((vf_id != 0) || !uda_is_phy_dev(udevid)) {
            return -EOPNOTSUPP;
        }
    }

    ret = dms_operate_sdk_ex_version(DMS_OP_SET_SDK_EX_VERSION, udevid, cfg_in->payload, &cfg_in->buff_size);
    if (ret != 0) {
        dms_err("Failed to set SDK Ex version. (udevid=%u; ret=%d)\n", udevid, ret);
    }
    return ret;
}

STATIC int dms_sdk_ex_ver_uda_notifier_func(u32 udevid, enum uda_notified_action action)
{
    if ((action == UDA_INIT) || (action == UDA_UNINIT)) {
        dms_operate_sdk_ex_version(DMS_OP_CLEAR_SDK_EX_VERSION, udevid, NULL, NULL);
    }

    dms_info("notifier action. (udevid=%u; action=%d)\n", udevid, action);
    return 0;
}

int dms_sdK_ex_ver_init(void)
{
    int ret;
    struct uda_dev_type type = { 0 };

    ka_task_rwlock_init(&g_sdk_ex_ver_lock);

    uda_davinci_local_real_entity_type_pack(&type);
    ret = uda_real_virtual_notifier_register(DMS_SDK_EX_VER_NOTIFIER, &type, UDA_PRI2,
        dms_sdk_ex_ver_uda_notifier_func);
    if (ret != 0) {
        dms_err("Device register local real virtual type failed. (ret=%d)\n", ret);
        return ret;
    }

    dms_info("dms sdk ex ver init.\n");
    return ret;
}
DECLAER_FEATURE_AUTO_INIT(dms_sdK_ex_ver_init, FEATURE_LOADER_STAGE_5);

void dms_sdK_ex_ver_exit(void)
{
    struct uda_dev_type type = { 0 };

    uda_davinci_local_real_entity_type_pack(&type);
    (void)uda_real_virtual_notifier_unregister(DMS_SDK_EX_VER_NOTIFIER, &type);
    dms_info("dms sdk ex ver exit.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_sdK_ex_ver_exit, FEATURE_LOADER_STAGE_5);
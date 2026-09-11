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
#ifndef AOS_LLVM_BUILD
#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "virtmng_interface.h"
#include "dms_basic_info.h"
#include "dms_vdev.h"

#ifdef SUPPORT_VDEV_MEM
int dms_drv_get_vdevice_info(void *feature, char *in, u32 in_len,
    char *out, u32 out_len)
{
    struct dms_get_vdevice_info_out inf_out = {0};
    struct vmng_soc_resource_enquire info = {0};
    struct dms_get_vdevice_info_in *input = NULL;
    u32 total_core = 0;
    u32 core_count = 0;
    u64 mem_size = 0;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_vdevice_info_in))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_vdevice_info_in *)in;

    if ((out == NULL) || (out_len != sizeof(struct dms_get_vdevice_info_out))) {
        dms_err("output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    if ((input->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (input->vfid > VDAVINCI_MAX_VFID_NUM)) {
        dms_err("devid invalid=%u\r\n", input->dev_id);
        return -EINVAL;
    }

    ret = vmngd_get_device_vf_core_info(input->dev_id, input->vfid, &total_core, &core_count, &mem_size);
    if (ret != 0) {
        dms_err("get pcie id info failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = vmngd_enquire_soc_resource(input->dev_id, input->vfid, &info);
    if (ret != 0) {
        dms_err("get soc resource failed. (ret=%d)\n", ret);
        return ret;
    }

    inf_out.total_core = total_core;
    inf_out.core_num = core_count;
    inf_out.mem_size = mem_size;
    inf_out.bitmap = info.each.stars_refresh.device_aicpu;
    ret = memcpy_s((void *)out, out_len, (void *)&inf_out, sizeof(struct dms_get_vdevice_info_out));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
#endif

#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_SRIOV)
int dms_feature_set_sriov_switch(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_sriov_switch_in *sriov_switch_in = NULL;
    u32 phy_id = 0;
    u32 vfid = 0;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct dms_sriov_switch_in))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    sriov_switch_in = (struct dms_sriov_switch_in *)in;
    if (sriov_switch_in->dev_id >= DEVDRV_PF_DEV_MAX_NUM) {
        dms_err("Device id invalid. (dev_id=%u)\n", sriov_switch_in->dev_id);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(sriov_switch_in->dev_id, &phy_id, &vfid);
    if (ret != 0) {
        dms_err("Transfer logical id to physical id fail. (dev_id=%u; ret=%d)\n", sriov_switch_in->dev_id, ret);
        return ret;
    }

    /* 1:sriov open  0:sriov close */
    if (sriov_switch_in->sriov_switch == 1) {
        ret = vmngh_enable_sriov(phy_id);
        if (ret != 0) {
            dms_err("Enable sriov failed. (dev_id=%u; ret=%d)", sriov_switch_in->dev_id, ret);
            return -EINVAL;
        }
    } else if (sriov_switch_in->sriov_switch == 0) {
        ret = vmngh_disable_sriov(phy_id);
        if (ret != 0) {
            dms_err("Disable sriov failed. (dev_id=%u; ret=%d)", sriov_switch_in->dev_id, ret);
            return -EINVAL;
        }
    } else {
        return -EOPNOTSUPP;
    }

    return 0;
}
#endif
#endif


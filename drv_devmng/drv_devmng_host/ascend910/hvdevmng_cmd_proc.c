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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include <linux/types.h>

#include "ka_base_pub.h"

#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager.h"
#include "devdrv_manager_msg.h"
#include "devdrv_platform_resource.h"
#include "devdrv_common.h"
#include "devdrv_interface.h"
#include "dms_event_distribute.h"
#include "devmng_dms_adapt.h"
#include "hvdevmng_init.h"
#include "dms_osc_freq.h"
#include "dms_cmd_def.h"
#include "dms_feature.h"
#include "dms_urd_forward.h"
#include "hvdevmng_cmd_proc.h"

struct dms_lpm_info_in {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int core_id;
    unsigned int sub_cmd;
};

int hvdevmng_get_h2d_devinfo(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    struct devdrv_info *dev_info = NULL;
    int ret;
    int i;

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_err("device(%u) is not initialized\n", dev_id);
        return -ENODEV;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_warn("dev %d has been reset\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_manager_h2d_sync_get_devinfo(dev_info);
    if (ret) {
        devdrv_drv_err("device info get failed. dev_id(%u)\n", dev_id);
    } else {
        iomsg->cmd_data.H2D_devinfo.cpu_system_count = dev_info->cpu_system_count;
        iomsg->cmd_data.H2D_devinfo.monotonic_raw_time_ns = dev_info->monotonic_raw_time_ns;
        for (i = 0; i < DEVDRV_MAX_COMPUTING_POWER_TYPE; i++) {
            iomsg->cmd_data.H2D_devinfo.computing_power[i] = dev_info->computing_power[i];
        }
    }
    ka_base_atomic_dec(&dev_info->occupy_ref);

    return ret;
}

enum devdrv_ts_status tsdrv_get_ts_status(u32 devid, u32 tsid);

int hvdevmng_get_device_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    struct devdrv_manager_info *d_info = NULL;
    enum devdrv_ts_status ts_status;
    u32 status;

    d_info = devdrv_get_manager_info();

    ts_status = tsdrv_get_ts_status(dev_id, 0);

    if ((d_info == NULL) || (d_info->dev_info[dev_id] == NULL)) {
        status = DRV_STATUS_INITING;
    } else if (d_info->device_status[dev_id] == DRV_STATUS_COMMUNICATION_LOST) {
        status = DRV_STATUS_COMMUNICATION_LOST;
    } else if (ts_status == TS_DOWN) {
        status = DRV_STATUS_EXCEPTION;
    } else if (ts_status == TS_WORK) {
        status = DRV_STATUS_WORK;
    } else {
        status = DRV_STATUS_INITING;
    }

    iomsg->cmd_data.u32_para.para_out = status;

    return 0;
}

int hvdevmng_get_device_boot_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    int ret;

    ret = devdrv_get_device_boot_status(dev_id, &iomsg->cmd_data.boot_status.boot_status);
    if (ret) {
        devdrv_drv_err("drv_get_device_boot_status_common, ret(%d).\n", ret);
    }

    return ret;
}

int hvdevmng_get_device_startup_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if ((dev_info == NULL) || (dev_info->shm_head == NULL) || (dev_info->shm_status == NULL) ||
        (dev_info->shm_head->head_info.magic != DEVMNG_SHM_INFO_HEAD_MAGIC)) {
        iomsg->cmd_data.work_status.device_process_status = DSMI_BOOT_STATUS_UNINIT;
        iomsg->cmd_data.work_status.dmp_started = false;
    } else if (dev_info->shm_head->head_info.version != DEVMNG_SHM_INFO_HEAD_VERSION) {
        devdrv_drv_err("dev(%u) version of share memory in host is 0x%llx, "
                       "the version in device is 0x%llx, magic is 0x%x.\n",
                       dev_info->dev_id, DEVMNG_SHM_INFO_HEAD_VERSION,
                       dev_info->shm_head->head_info.version,
                       dev_info->shm_head->head_info.magic);
        return -EINVAL;
    } else {
        if (dev_info->dmp_started == false)
            dev_info->dmp_started = devdrv_manager_h2d_query_dmp_started(dev_id);
        iomsg->cmd_data.work_status.dmp_started = dev_info->dmp_started;
        iomsg->cmd_data.work_status.device_process_status = (unsigned int)dev_info->shm_status->os_status;
    }

    return 0;
}

int hvdevmng_get_device_health_status(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    struct devdrv_info *dev_info = NULL;
    unsigned int bbox_health, dms_health;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (devdrv_manager_shm_info_check(dev_info)) {
        devdrv_drv_err("devid(%u) shm info check fail.\n", dev_id);
        return -EFAULT;
    }

    bbox_health = (unsigned int)dev_info->shm_status->health_status;
    dms_health = (unsigned int)dev_info->shm_status->dms_health_status[0];
    iomsg->cmd_data.health_status.device_health_status = bbox_health > dms_health ? bbox_health : dms_health;
    return 0;
}

int hvdevmng_mdev_info_get(u32 dev_id, u32 fid, struct vdevdrv_info_msg *ready_info)
{
    struct devdrv_info *dev_info = NULL;
    u32 aicpu_occupy_bitmap = 0;
    int ret_core, ret_aicpu, ret_vecore, ret_template;
    u32 aicore_num = 0;
    u32 aicpu_num = 0;
    u32 vector_core_num = 0;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if ((dev_info == NULL) || (ready_info == NULL)) {
        devdrv_drv_err("dev(%u) dev_info(%pK) or ready_info(%pK) is NULL, .\n",
                       dev_id, dev_info, ready_info);
        return -EINVAL;
    }

    ret_core = hvdevmng_get_aicore_num(dev_id, fid, &aicore_num);
    ret_aicpu = hvdevmng_get_aicpu_num(dev_id, fid, &aicpu_num, &aicpu_occupy_bitmap);
    ret_vecore = hvdevmng_get_vector_core_num(dev_id, fid, &vector_core_num);
    ret_template = hvdevmng_get_template_name(dev_id, fid, ready_info->template_name, TEMPLATE_NAME_LEN);
    if ((ret_core != 0) || (ret_aicpu != 0) || (ret_vecore != 0) || (ret_template != 0)) {
        devdrv_drv_err("Get aicore or aicpu or vector or template name failed;\
            (dev_id=%u; ret_core=%d; ret_aicpu=%d; ret_vecore=%d; ret_template=%d).\n",
            dev_id, ret_core, ret_aicpu, ret_vecore, ret_template);
        return -EINVAL;
    }

    /* get device info from phy machine for vdev_info */
    ready_info->ctrl_cpu_ip = dev_info->ctrl_cpu_ip;
    ready_info->ctrl_cpu_id = dev_info->ctrl_cpu_id;
    ready_info->ctrl_cpu_core_num = dev_info->ctrl_cpu_core_num;
    ready_info->ctrl_cpu_occupy_bitmap = dev_info->ctrl_cpu_occupy_bitmap;
    ready_info->ctrl_cpu_endian_little = dev_info->ctrl_cpu_endian_little;
    ready_info->ai_core_num = aicore_num;
    ready_info->ai_core_id = dev_info->ai_core_id;
    ready_info->ai_cpu_core_num = aicpu_num;
    ready_info->ai_cpu_core_id = dev_info->ai_cpu_core_id;
    ready_info->aicpu_occupy_bitmap = aicpu_occupy_bitmap;
    ready_info->ai_subsys_ip_broken_map = dev_info->ai_subsys_ip_broken_map;
    ready_info->hardware_version = dev_info->hardware_version;
    ready_info->env_type = dev_info->env_type;
    ready_info->inuse_ai_core_num = aicore_num;
    ready_info->inuse_ai_core_error_bitmap = 0x0;
    ready_info->inuse_ai_cpu_num = aicpu_num;
    ready_info->inuse_ai_cpu_error_bitmap = 0x0;
    ready_info->ts_num = dev_info->ts_num;
    ready_info->aicore_freq = dev_info->aicore_freq;
    ready_info->vector_core_num = vector_core_num;
    ready_info->vector_core_bitmap = dev_info->vector_core_bitmap;
    ready_info->vector_core_freq = dev_info->vector_core_freq;
    ready_info->chip_name = dev_info->chip_name;
    ready_info->chip_version = dev_info->chip_version;
    return 0;
}

int hvdevmng_mdev_pdata_get(u32 dev_id, struct vdevdrv_info_pdata *pdata)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if ((dev_info == NULL) || (dev_info->pdata == NULL) || (pdata == NULL)) {
        devdrv_drv_err("dev(%u) dev_info(%pK) pdata(%pK).\n", dev_id, dev_info, pdata);
        return -EINVAL;
    }

    pdata->ts_cpu_core_num = dev_info->pdata->ts_pdata[0].ts_cpu_core_num;
    pdata->ts_num = devdrv_manager_get_ts_num(dev_info);
    pdata->ai_core_num_level = dev_info->pdata->ai_core_num_level;
    pdata->ai_core_freq_level = dev_info->pdata->ai_core_freq_level;

    return 0;
}

int hvdevmng_vpc_get_dev_resource_info(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    struct devdrv_manager_msg_resource_info info;
    u32 tsid;
    int ret;

    info.vfid = fid;
    info.info_type = iomsg->cmd_data.resource_info.resource_type;
    tsid = iomsg->cmd_data.resource_info.tsid;
    info.owner_id = iomsg->cmd_data.resource_info.owner_id;

    ret = hvdevmng_get_dev_resource(dev_id, tsid, &info);
    if (ret) {
        devdrv_drv_err("get dev resource info failed, devid(%u) vfid(%u) tsid(%u ret(%d).\n",
            dev_id, fid, tsid, ret);
        return ret;
    }

    ret = memcpy_s((void *)iomsg->cmd_data.resource_info.buf, DEVDRV_MAX_PAYLOAD_LEN,
                   &info.value, sizeof(u64));

    return ret;
}

int hvdevmng_get_error_code(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    struct devdrv_info *dev_info = NULL;
    unsigned int i, j, error_cnt, event_cnt;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (devdrv_manager_shm_info_check(dev_info)) {
        devdrv_drv_err("devid(%u) shm info check fail.\n", dev_id);
        return -EFAULT;
    }

    for (i = 0; i < DMANAGE_ERROR_ARRAY_NUM; i++) {
        iomsg->cmd_data.error_code_para.error_code[i] = 0;
    }

    error_cnt = dev_info->shm_status->error_cnt;
    error_cnt = error_cnt > DEVMNG_SHM_INFO_ERROR_CODE_LEN ? DEVMNG_SHM_INFO_ERROR_CODE_LEN : error_cnt;
    event_cnt = dev_info->shm_status->event_cnt;
    event_cnt = event_cnt > DEVMNG_SHM_INFO_EVENT_CODE_LEN ? DEVMNG_SHM_INFO_EVENT_CODE_LEN : event_cnt;
    for (i = 0; i < error_cnt; i++) {
        iomsg->cmd_data.error_code_para.error_code[i] = dev_info->shm_status->error_code[i];
    }
    for (i = error_cnt, j = 0; (i < DMANAGE_ERROR_ARRAY_NUM) && (j < event_cnt); i++, j++) {
        iomsg->cmd_data.error_code_para.error_code[i] = dev_info->shm_status->event_code[j].event_code;
    }

    iomsg->cmd_data.error_code_para.error_code_count = (error_cnt + event_cnt) > DMANAGE_ERROR_ARRAY_NUM ? \
                                                       DMANAGE_ERROR_ARRAY_NUM : (error_cnt + event_cnt);
    return 0;
}

int hvdevmng_get_osc_freq(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    int ret;
    u64 freq = 0;

    if (iomsg->cmd_data.osc_freq.sub_cmd == DMS_SUBCMD_GET_HOST_OSC_FREQ) {
        ret = dms_get_host_osc_freq(&freq);
        if (ret != 0) {
            devdrv_drv_ex_notsupport_err(ret, "Get host osc frequency failed. (ret=%d)\n", ret);
            return ret;
        }
        iomsg->cmd_data.osc_freq.value = freq;
    } else {
        ret = dms_get_device_osc_freq(dev_id, &freq);
        if (ret != 0) {
            devdrv_drv_err("Get device osc frequency failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }

        iomsg->cmd_data.osc_freq.value = freq;
    }

    return 0;
}

enum lpm_core_id {
    CLUSTER_ID = 0,
    PERI_ID = 1,
    TS_ID = 2,
    DDR_ID = 3,
    AICORE0_ID = 4,
    AICORE1_ID = 5,
    HBM_ID = 6,
    VECTOR_ID = 7,
    INVALID_ID,
};

int hvdevmng_get_current_aic_freq(u32 dev_id, u32 fid, struct vdevmng_ioctl_msg *iomsg)
{
    int ret;
    u32 freq = 0;
    DMS_FEATURE_S feature_cfg = {0};
    struct dms_lpm_info_in freq_in = {0};

    feature_cfg.main_cmd = DMS_MAIN_CMD_LPM;
    feature_cfg.sub_cmd = DMS_SUBCMD_GET_FREQUENCY;
    feature_cfg.filter = NULL;
    freq_in.dev_id = dev_id;
    freq_in.core_id = AICORE0_ID;
    ret = dms_send_msg_to_device_by_h2d(&feature_cfg, (void *)&freq_in, sizeof(struct dms_lpm_info_in),
        (void *)&freq, sizeof(u32));
    if (ret != 0) {
        devdrv_drv_ex_notsupport_err(ret, "Get current aicore frequency failed. (dev_id=%u; vfid=%u; ret=%d)\n",
            dev_id, fid, ret);
        return ret;
    }

    iomsg->cmd_data.current_aic_freq.freq = freq;
    return 0;
}
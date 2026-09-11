/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-04-16
*/

#include <linux/string.h>

#include "devdrv_common.h"
#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_feature.h"
#include "urd_acc_ctrl.h"
#include "dbl/feature_loader.h"
#include "dbl/uda.h"
#include "dms_hotreset.h"
#include "ascend_kernel_hal.h"
#include "dms_urd_forward.h"

#define MAX_PACKET_SIZE 300

STATIC int dms_send_msg_to_device_get_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len);

BEGIN_DMS_MODULE_DECLARATION(DMS_URD_FORWARD_CMD_NAME)
BEGIN_FEATURE_COMMAND()
#ifdef CFG_FEATURE_QUERY_FREQ_INFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_LPM, DMS_SUBCMD_GET_FREQUENCY, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
#endif
#ifdef CFG_FEATURE_QUERY_QOS_CFG_INFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_QOS, DMS_SUBCMD_GET_CONFIG_INFO, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
#endif
#ifdef CFG_FEATURE_QUERY_VA_INFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_HBM_GET_VA, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d_get_va)
#endif
#ifdef CFG_FEATURE_GET_QOS_MASTER_CFG
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x9,info=0x3a", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
#endif
#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_GET_FAULT_SYSCNT, NULL, NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT, urd_forward_get_memory_fault_syscnt)
#endif
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xc,info=0x24", NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT,
                    dms_send_msg_to_device_by_h2d) /* LP GET AIC CPM */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xc,info=0x25", NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT,
                    dms_send_msg_to_device_by_h2d) /* LP GET BUS CPM */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xc,info=0x26", NULL,
                    DMS_ACC_ROOT | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT,
                    dms_send_msg_to_device_lp_stress_set) /* LP SET STRESS TEST */

ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2d", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_get_sdk_ex_version)

ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2e", NULL,
                    DMS_ACC_ALL | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2e", NULL,
                    DMS_ACC_ROOT | DMS_ENV_PHYSICAL | DMS_VDEV_NOTSUPPORT, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x36", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_urd_forward_init(void)
{
    dms_info("dms urd_forward init.\n");
    CALL_INIT_MODULE(DMS_URD_FORWARD_CMD_NAME);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_urd_forward_init, FEATURE_LOADER_STAGE_5);

void dms_urd_forward_uninit(void)
{
    dms_info("dms urd_forward uninit.\n");
    CALL_EXIT_MODULE(DMS_URD_FORWARD_CMD_NAME);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_urd_forward_uninit, FEATURE_LOADER_STAGE_5);

STATIC int dms_urd_forward_para_check(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    if (feature == NULL) {
        dms_err("feature is NULL.");
        return -EINVAL;
    }
    if (in == NULL || in_len < sizeof(u32)) {
        dms_err("Input data is null or input data length is not enough. (in%s; in_len=%u)\n",
            in == NULL ? "=NULL" : "!=NULL", in_len);
        return -EINVAL;
    }

    if (out == NULL || out_len < sizeof(char)) {
        dms_err("Output data is null or output data length is not enough. (out%s; out_len=%u)\n",
            out == NULL ? "=NULL" : "!=NULL", out_len);
        return -EINVAL;
    }

    return 0;
}

int dms_set_urd_msg(DMS_FEATURE_S *feature_cfg, char *in, u32 in_len, u32 out_len, struct urd_forward_msg *urd_msg)
{
    int ret;

    urd_msg->main_cmd = feature_cfg->main_cmd;
    urd_msg->sub_cmd = feature_cfg->sub_cmd;
    if (feature_cfg->filter != NULL) {
        urd_msg->filter_len = strlen(feature_cfg->filter);
        if (urd_msg->filter_len != 0) {
            ret = strcpy_s(urd_msg->filter, FILTER_LEN, feature_cfg->filter);
            if (ret != 0) {
                dms_err("strcpy_s failed. (ret=%d)\n", ret);
                return ret;
            }
        }
    }

    if (in != NULL) {
        ret = memcpy_s((void*)&(urd_msg->payload[0]), PAYLOAD_LEN_MAX, (void*)in, in_len);
        if (ret != 0) {
            dms_err("memcpy_s failed. (size=%d, ret=%d)\n", in_len, ret);
            return ret;
        }
    }
    urd_msg->payload_len = in_len;
    urd_msg->output_len = out_len;
    return 0;
}

int dms_urd_forward_send_to_device(u32 phy_id, u32 vfid, struct urd_forward_msg *urd_msg, char *out, u32 out_len)
{
    int ret;

    ret = devdrv_manager_h2d_sync_urd_forward(phy_id, vfid, urd_msg);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "devdrv_manager_h2d_sync_urd_forward failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    if (urd_msg->payload_len > out_len) {
        dms_err("size of payload is bigger than out_buff. (phy_id=%u; payload_len=%u; out_len=%u)\n",
            phy_id, urd_msg->payload_len, out_len);
        return -EINVAL;
    }
    ret = memcpy_s((void*)out, out_len, (void*)&(urd_msg->payload[0]), urd_msg->payload_len);
    if (ret != 0) {
        dms_err("memcpy_s failed. (phy_id=%u; ret=%d; out_len=%u)\n", phy_id, ret, out_len);
    }

    return ret;
}

int dms_send_msg_to_device_by_h2d(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    DMS_FEATURE_S *feature_cfg = NULL;
    struct devdrv_info *dev_info = NULL;
    struct urd_forward_msg urd_msg = {0};
    u32 dev_id;
    u32 phys_id = 0, vfid = 0;
    int ret = 0;

    ret = dms_urd_forward_para_check(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_err("Para check failed.\n");
        return ret;
    }

    feature_cfg = (DMS_FEATURE_S *)feature;
    dev_id = *((u32 *)in);
    ret = uda_devid_to_phy_devid(dev_id, &phys_id, &vfid);
    if (ret != 0) {
        dms_err("Transform virt id failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return ret;
    }

    dev_info = devdrv_manager_get_devdrv_info(phys_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (phys_id=%u)\n", phys_id);
        return -ENODEV;
    }

    ret = dms_hotreset_task_cnt_increase(phys_id);
    if (ret != 0) {
        dms_err("Hotreset task cnt increase failed. (phys_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        dms_warn("Device has been reset. (phy_id=%u)\n", phys_id);
        ret = -EINVAL;
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

    ret = dms_set_urd_msg(feature_cfg, in, in_len, out_len, &urd_msg);
    if (ret != 0) {
        dms_err("dms_set_urd_msg failed. (phy_id=%u; ret=%d)\n", phys_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

    ret = dms_urd_forward_send_to_device(phys_id, vfid, &urd_msg, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "devdrv_manager_h2d_sync_urd_forward failed. (phy_id=%u; ret=%d)\n", phys_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

OCCUPY_AND_TASK_CNT_OUT:
    atomic_dec(&dev_info->occupy_ref);
    dms_hotreset_task_cnt_decrease(phys_id);
    return ret;
}

int dms_send_msg_to_device_by_h2d_get_va(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct hbm_pa_va_info *va_info = NULL;
    int ret = 0;

    if (in == NULL || in_len < sizeof(unsigned int) + sizeof(unsigned int)) {
        dms_err("Invalid parameter. (in_is_null=%d; in_len=%u)\n", (in == NULL), in_len);
        return -EINVAL;
    }

    va_info = (struct hbm_pa_va_info *)in;
    va_info->host_pid = current->tgid;

    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to send msg to device. (ret=%d)\n", ret);
    }
    return ret;
}

struct lpm_soc_stress_hal_cfg_in{
    unsigned int pid;
    unsigned int is_offline;
};
int dms_send_msg_to_device_lp_stress_set(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_hal_device_info_stru *forward_stru = NULL;
    struct lpm_soc_stress_hal_cfg_in *lp_stress_set = NULL;
    int ret = 0;

    if (feature == NULL || in == NULL ||
        in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + sizeof(struct lpm_soc_stress_hal_cfg_in))) {
        dms_err("param invalid.(feature=%d; in=%d; in_len=%u)\n", feature != NULL, in != NULL, in_len);
        return -EINVAL;
    }

    forward_stru = (struct dms_hal_device_info_stru *)in;
    lp_stress_set = (struct lpm_soc_stress_hal_cfg_in *)forward_stru->payload;
    lp_stress_set->pid = current->tgid;
    lp_stress_set->is_offline = 1;
    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "dms_send_msg_to_device_by_h2d failed. ret=%d\n", ret);
    }
    return ret;
}

#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
int urd_forward_get_memory_fault_syscnt(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct memory_fault_timestamp* mem_para = NULL;
    int ret;

    if (feature == NULL || in == NULL || out == NULL) {
        dms_err("NULL pointer. (feature=%d; in=%d; out=%d)\n", feature != NULL, in != NULL, out != NULL);
        return -EINVAL;
    }

    if (in_len != sizeof(struct memory_fault_timestamp) || out_len != sizeof(struct memory_fault_timestamp)) {
        dms_err("Invalid len. (in_len=%u; expected=%u; out_len=%u; expected=%u)\n",
                in_len, sizeof(struct memory_fault_timestamp), out_len, sizeof(struct memory_fault_timestamp));
        return -EINVAL;
    }

    mem_para = (struct memory_fault_timestamp *)in;
    mem_para->host_pid = current->tgid;

    ret = dms_send_msg_to_device_by_h2d(feature, (char *)in, in_len, (char *)out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to send msg to device. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
#endif

STATIC int dms_send_msg_to_device_get_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
#ifdef CFG_FEATURE_SDK_EX_VERSION
    int ret;
    unsigned int udevid, vf_id, soc_type;
    struct dms_hal_device_info_stru *cfg_in = NULL;

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

    return dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
#else
    (void)feature;
    (void)in;
    (void)in_len;
    (void)out;
    (void)out_len;
    return -EOPNOTSUPP;
#endif
}

int dms_send_msg_to_device_by_h2d_kernel(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    DMS_FEATURE_S *feature_cfg = NULL;
    struct devdrv_info *dev_info = NULL;
    struct urd_forward_msg urd_msg = {0};
    u32 dev_id;
    int ret = 0;
 
    ret = dms_urd_forward_para_check(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_err("Para check failed.\n");
        return ret;
    }
 
    feature_cfg = (DMS_FEATURE_S *)feature;
    dev_id = *((u32 *)in);
    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }
 
    ret = dms_hotreset_task_cnt_increase(dev_id);
    if (ret != 0) {
        dms_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
 
    atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        dms_warn("Device has been reset. (dev_id=%u)\n", dev_id);
        ret = -EINVAL;
        goto OCCUPY_AND_TASK_CNT_OUT;
    }
 
    ret = dms_set_urd_msg(feature_cfg, in, in_len, out_len, &urd_msg);
    if (ret != 0) {
        dms_err("dms_set_urd_msg failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }
 
    ret = dms_urd_forward_send_to_device(dev_id, 0, &urd_msg, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "devdrv_manager_h2d_sync_urd_forward failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }
 
OCCUPY_AND_TASK_CNT_OUT:
    atomic_dec(&dev_info->occupy_ref);
    dms_hotreset_task_cnt_decrease(dev_id);
    return ret;
}

int dms_send_msg_to_device_by_h2d_multi_packets(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int i = 0;
    int ret = 0;
    int start = 0;
    int len = 0;
    int total_packets;
    struct dms_set_device_info_in *cfg_in = NULL;
    struct dms_set_device_info_in_multi_packet cfg_in_temp;
 
    cfg_in = (struct dms_set_device_info_in *)in;
 
    total_packets = (cfg_in->buff_size + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    for (i = 0; i < total_packets; i++) {
        start = i * MAX_PACKET_SIZE;
        len = (start + MAX_PACKET_SIZE > cfg_in->buff_size) ? (cfg_in->buff_size - start) : MAX_PACKET_SIZE;
        dms_info("Set sign cert start =%d, len=%d.\n", start, len);
        cfg_in_temp.dev_id = cfg_in->dev_id;
        cfg_in_temp.sub_cmd = cfg_in->sub_cmd;
        cfg_in_temp.buff_size = len;
        cfg_in_temp.current_packet = i;
        cfg_in_temp.total_size = cfg_in->buff_size;
        (void)memset_s(cfg_in_temp.buff, MAX_PACKET_SIZE, 0, MAX_PACKET_SIZE);
        ret = copy_from_user(cfg_in_temp.buff, cfg_in->buff + start, len);
        if (ret != 0) {
            dms_err("Copy from user fail.(ret=%d;param_size=%u)\n", ret, len);
            return -EINVAL;
        }
        dms_info("Set sign cert. (current_packet=%u; buff_size=%u; total_size=%u)\n",
            cfg_in_temp.current_packet,
            cfg_in_temp.buff_size,
            cfg_in_temp.total_size);
 
        ret = dms_send_msg_to_device_by_h2d(feature,
            (char *)&cfg_in_temp,
            sizeof(cfg_in_temp),
            (char *)&cfg_in_temp,
            sizeof(cfg_in_temp));
        if (ret != 0) {
            return ret;
        }
    }
    return ret;
}
 
int dms_send_msg_to_device_by_h2d_multi_packets_kernel(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int i = 0;
    int ret = 0;
    int start = 0;
    int len = 0;
    int total_packets;
    struct dms_set_device_info_in *cfg_in = NULL;
    struct dms_set_device_info_in_multi_packet cfg_in_temp;
 
    cfg_in = (struct dms_set_device_info_in *)in;
 
    total_packets = (cfg_in->buff_size + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    for (i = 0; i < total_packets; i++) {
        start = i * MAX_PACKET_SIZE;
        len = (start + MAX_PACKET_SIZE > cfg_in->buff_size) ? (cfg_in->buff_size - start) : MAX_PACKET_SIZE;
        cfg_in_temp.dev_id = cfg_in->dev_id;
        cfg_in_temp.sub_cmd = cfg_in->sub_cmd;
        cfg_in_temp.buff_size = len;
        cfg_in_temp.current_packet = i;
        cfg_in_temp.total_size = cfg_in->buff_size;
        (void)memset_s(cfg_in_temp.buff, MAX_PACKET_SIZE, 0, MAX_PACKET_SIZE);
        ret = memcpy_s(cfg_in_temp.buff, MAX_PACKET_SIZE, cfg_in->buff + start, len);
        if (ret != 0) {
            dms_err("Memcpy fail.(ret=%d;param_size=%u)\n", ret, len);
            return -EINVAL;
        }
        ret = dms_send_msg_to_device_by_h2d_kernel(feature,
            (char *)&cfg_in_temp,
            sizeof(cfg_in_temp),
            (char *)&cfg_in_temp,
            sizeof(cfg_in_temp));
        if (ret != 0) {
            return ret;
        }
    }
    return ret;
}

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
 */

#include <linux/kernel.h>

#include "ascend_kernel_hal.h"
#include "urd_acc_ctrl.h"
#include "dbl/feature_loader.h"
#include "dbl/runenv_config.h"
#include "dbl/uda.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_urd_forward.h"
#include "dms_custom_common.h"

#include "ka_memory_pub.h"

int dms_host_set_sign_flag(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_host_get_sign_flag(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_host_set_sign_cert(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_host_get_sign_cert(void *feature, char *in, u32 in_len, char *out, u32 out_len);

struct mutex g_cert_sync_lock[MAX_DEVICE_NUM];

BEGIN_DMS_MODULE_DECLARATION(DMS_CUSTOM_FORWARD_CMD_NAME)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_CUSTOM_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "main_cmd=0x35,sub_cmd=0x2",
    NULL, DMS_ACC_ROOT | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_ALL, dms_host_set_sign_flag)
ADD_FEATURE_COMMAND(DMS_CUSTOM_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "main_cmd=0x35,sub_cmd=0x3",
    NULL, DMS_ACC_ROOT | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_ALL, dms_host_set_sign_cert)
ADD_FEATURE_COMMAND(DMS_CUSTOM_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "main_cmd=0x35,sub_cmd=0x2",
    NULL, DMS_SUPPORT_ALL_USER, dms_host_get_sign_flag)
ADD_FEATURE_COMMAND(DMS_CUSTOM_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "main_cmd=0x35,sub_cmd=0x3",
    NULL, DMS_ACC_ALL | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_ALL, dms_host_get_sign_cert)

END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_custom_forward_init(void)
{
    dms_info("dms custom_forward init.\n");
    CALL_INIT_MODULE(DMS_CUSTOM_FORWARD_CMD_NAME);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_custom_forward_init, FEATURE_LOADER_STAGE_5);

void dms_custom_forward_uninit(void)
{
    dms_info("dms custom_forward uninit.\n");
    CALL_EXIT_MODULE(DMS_CUSTOM_FORWARD_CMD_NAME);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_custom_forward_uninit, FEATURE_LOADER_STAGE_5);

int dms_host_set_sign_flag(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int phy_id, vf_id;
    unsigned int sign_flag = 0;
    long value_len = 0;
    struct dms_hal_device_info_stru *cfg_in = (struct dms_hal_device_info_stru *)in;
    char val[CUSTOM_SIGN_CONF_VALLUE_MAX_LEN] = {0};
    char file_path[CUSTOM_SIGN_CONF_FILE_PATH_MAX] = {0};
    int file_exist = 0;

    if ((feature == NULL) || (in == NULL) || (in_len < DMS_HAL_DEV_INFO_HEAD_LEN + cfg_in->buff_size)) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u)\n",
            (feature == NULL) ? "NULL" : "OK",
            (in == NULL) ? "NULL" : "OK",
            in_len);
        return -EINVAL;
    }

    if ((cfg_in->buff_size != sizeof(unsigned int))) {
        dms_err("Invalid parameter. (buff_size=%u; dev_id=%u)\n", cfg_in->buff_size, cfg_in->dev_id);
        return -EINVAL;
    }

    ret = memcpy_s(&sign_flag, sizeof(unsigned int), cfg_in->payload, cfg_in->buff_size);
    if (ret != 0) {
        dms_err("Failed to invoke memcpy. (ret=%d)\n", ret);
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(cfg_in->dev_id, &phy_id, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id. (logical_id=%u; ret=%d)\n", cfg_in->dev_id, ret);
        return -EINVAL;
    }

    ret = snprintf_s(file_path, CUSTOM_SIGN_CONF_FILE_PATH_MAX, CUSTOM_SIGN_CONF_FILE_PATH_MAX - 1, CUSTOM_SIGN_CONF_FLAG_PATH, phy_id);
    if (ret < 0) {
        dms_err("Failed to invoke snprintf_s, (phy_id=%u; ret=%d).\n", phy_id, ret);
        return -EINVAL;
    }

    ret = dms_custom_is_file_exist(file_path, &file_exist);
    if (ret != 0) {
        dms_err("Failed to dms_custom_is_file_exist. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    if (file_exist == 0) {
        dms_warn("File is not exist. (file_path=%s)\n", file_path);
        return -ENOENT;
    }

    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, in, in_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to send msg to device. (ret=%d)\n", ret);
        return ret;
    }

    value_len = snprintf_s(
        val, CUSTOM_SIGN_CONF_VALLUE_MAX_LEN, CUSTOM_SIGN_CONF_VALLUE_MAX_LEN - 1, "verify_flag=%u\n", sign_flag);
    if (value_len < 0) {
        dms_err("Failed to invoke snprintf_s, (device_id=%u).\n", phy_id);
        return -EINVAL;
    }

    ret = dms_save_sign_flag_to_file(phy_id, val, value_len);
    if (ret != 0) {
        dms_err("Failed to save sign flag to file failed. (dev_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }
    dms_event("Set host kernel custom sign flag success. (phy_id=%u; sign_flag=%u)\n", phy_id, sign_flag);
    return 0;
}

int dms_host_get_sign_flag(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to send msg to device. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}

int dms_host_set_sign_cert(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int phy_id, vf_id;
    char *buf = NULL;
    struct dms_set_device_info_in *cfg_in = NULL;
    char file_path[CUSTOM_SIGN_CONF_FILE_PATH_MAX] = {0};
    int file_exist = 0;

    if ((feature == NULL) || (in == NULL) || (in_len != sizeof(struct dms_set_device_info_in))) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u)\n",
            (feature == NULL) ? "NULL" : "OK",
            (in == NULL) ? "NULL" : "OK",
            in_len);
        return -EINVAL;
    }

    cfg_in = (struct dms_set_device_info_in *)in;

    if (cfg_in->buff == NULL) {
        dms_err("Invalid parameter buff is null.\n");
        return -EINVAL;
    }

    if ((cfg_in->buff_size == 0) || (cfg_in->buff_size > CUSTOM_SIGN_CONF_FILE_SIZE)) {
        dms_err("Invalid parameter buff_size. (buff_size=%u; dev_id=%u)\n", cfg_in->buff_size, cfg_in->dev_id);
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(cfg_in->dev_id, &phy_id, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id. (logical_id=%u; ret=%d)\n", cfg_in->dev_id, ret);
        return -EINVAL;
    }

    ret = dms_custom_get_file_name(phy_id, cfg_in->sub_cmd, file_path);
    if (ret < 0) {
        dms_err("Failed to invoke dms_custom_get_file_name, (phy_id=%u; ret=%d).\n", phy_id, ret);
        return -EINVAL;
    }

    ret = dms_custom_is_file_exist(file_path, &file_exist);
    if (ret != 0) {
        dms_err("Failed to dms_custom_is_file_exist. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    if (file_exist == 0) {
        dms_warn("File is not exist. (file_path=%s)\n", file_path);
        return -ENOENT;
    }

    if (phy_id >= MAX_DEVICE_NUM) {
        dms_err("Physical id is large than max device num. (phy_id=%u)\n", phy_id);
        return -EINVAL;
    }

    mutex_lock(&g_cert_sync_lock[phy_id]);

    ret = dms_send_msg_to_device_by_h2d_multi_packets(feature, in, in_len, in, in_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to send msg to device. (ret=%d)\n", ret);
        mutex_unlock(&g_cert_sync_lock[phy_id]);
        return ret;
    }

    buf = (char *)dbl_kzalloc(cfg_in->buff_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buf == NULL) {
        dms_err("Alloc memory for file failed. (phy_id=%u; buff_size=%u)\n", phy_id, cfg_in->buff_size);
        mutex_unlock(&g_cert_sync_lock[phy_id]);
        return -ENOMEM;
    }

    ret = copy_from_user(buf, cfg_in->buff, cfg_in->buff_size);
    if (ret != 0) {
        dms_err("Copy from user fail.(ret=%d;param_size=%u)\n", ret, cfg_in->buff_size);
        dbl_kfree(buf);
        buf = NULL;
        mutex_unlock(&g_cert_sync_lock[phy_id]);
        return -EINVAL;
    }

    ret = dms_save_sign_cert_to_file(phy_id, cfg_in->sub_cmd, buf, cfg_in->buff_size);
    if (ret != 0) {
        dms_err("Failed to save sign cert to file. (phy_id=%u; ret=%d)\n", phy_id, ret);
        dbl_kfree(buf);
        buf = NULL;
        mutex_unlock(&g_cert_sync_lock[phy_id]);
        return ret;
    }

    dbl_kfree(buf);
    buf = NULL;
    mutex_unlock(&g_cert_sync_lock[phy_id]);
    return 0;
}

int dms_host_get_sign_cert(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int phy_id, vf_id;
    unsigned long long file_size;
    char *buf = NULL;
    char dst_path[CUSTOM_SIGN_CONF_FILE_PATH_MAX] = {0};
    struct dms_get_device_info_in *cfg_in = NULL;
    struct dms_get_device_info_out *cfg_out = NULL;

    if ((feature == NULL) || (in == NULL) || (in_len != sizeof(struct dms_get_device_info_in)) || (out == NULL) ||
        (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
            (feature == NULL) ? "NULL" : "OK",
            (in == NULL) ? "NULL" : "OK",
            in_len,
            (out == NULL) ? "NULL" : "OK",
            out_len);
        return -EINVAL;
    }

    cfg_in = (struct dms_get_device_info_in *)in;
    cfg_out = (struct dms_get_device_info_out *)out;

    if (cfg_in->buff == NULL) {
        dms_err("Invalid parameter, cfg_in buff is null.\n");
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(cfg_in->dev_id, &phy_id, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id. (logical_id=%u; ret=%d)\n", cfg_in->dev_id, ret);
        return -EINVAL;
    }

    ret = dms_custom_get_file_name(phy_id, cfg_in->sub_cmd, dst_path);
    if (ret < 0) {
        dms_err("Failed to invoke dms_custom_get_file_name, (phy_id=%u; ret=%d).\n", phy_id, ret);
        return -EINVAL;
    }

    ret = dms_custom_get_file_size(dst_path, &file_size);
    if (ret != 0) {
        dms_err("Failed to invoke dms_custom_get_file_size, (phy_id=%u; ret=%d).\n", phy_id, ret);
        return ret;
    }

    if (file_size > CUSTOM_SIGN_CONF_FILE_SIZE) {
        dms_err("File size is invalid. (phy_id=%u; file_size=0x%llx)\n", phy_id, file_size);
        return -EINVAL;
    }

    if (file_size == 0) {
        dms_warn("File size is 0. (phy_id=%u; file_size=0x%llx)\n", phy_id, file_size);
        return -ENOENT;
    }

    if (file_size > cfg_in->buff_size) {
        dms_err("File size is large than buffer size. (phy_id=%u; file_size=0x%llx)\n", phy_id, file_size);
        return -EINVAL;
    }

    buf = (char *)dbl_kzalloc(file_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buf == NULL) {
        dms_err("Alloc memory for file failed. (phy_id=%u; file_size=0x%llx)\n", phy_id, file_size);
        return -ENOMEM;
    }

    ret = dms_custom_read_file(dst_path, buf, file_size);
    if (ret < 0) {
        dms_err("Failed to invoke dms_custom_read_file, (phy_id=%u; ret=%d).\n", phy_id, ret);
        dbl_kfree(buf);
        buf = NULL;
        return -EINVAL;
    }

    ret = copy_to_user(cfg_in->buff, buf, file_size);
    if (ret != 0) {
        dms_err("Copy from user fail.(ret=%d;param_size=%llx)\n", ret, file_size);
        dbl_kfree(buf);
        buf = NULL;
        return -EINVAL;
    }

    dbl_kfree(buf);
    buf = NULL;
    cfg_out->out_size = file_size;

    return 0;
}

int dms_custom_send_cert_file_to_device(u32 dev_id, CUSTOM_FILE_TYPE file_type, char *buf, int size)
{
    int ret;
    DMS_FEATURE_S feature_cfg = {0};
    struct dms_set_device_info_in send_data = {0};

    switch (file_type) {
        case CUSTOM_FILE_TYPE_USR_CERT:
            feature_cfg.filter = "main_cmd=0x35,sub_cmd=0x3,init";
            send_data.sub_cmd = 0x3; /* 0x3 indicate user cert */
            break;
        default:
            dms_err("File type is invalid. (dev_id=%u; file_type=%d)\n", dev_id, file_type);
            return -EINVAL;
    }

    feature_cfg.main_cmd = DMS_GET_SET_DEVICE_INFO_CMD;
    feature_cfg.sub_cmd = ZERO_CMD;
    feature_cfg.owner_name = DMS_URD_FORWARD_CMD_NAME;
    feature_cfg.privilege = DMS_ACC_ROOT | DMS_ENV_ALL | DMS_VDEV_ALL;
    feature_cfg.proc_ctrl_str = NULL;
    feature_cfg.handler = dms_send_msg_to_device_by_h2d_multi_packets_kernel;
    send_data.dev_id = dev_id;
    send_data.buff = buf;
    send_data.buff_size = size;

    ret = dms_send_msg_to_device_by_h2d_multi_packets_kernel(&feature_cfg,
        (char *)&send_data,
        sizeof(struct dms_set_device_info_in),
        (char *)&send_data,
        sizeof(struct dms_set_device_info_in));
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Send packets to device failed. (dev_id=%u; file_type=%d; ret=%d)\n", dev_id, file_type, ret);
        return ret;
    }

    return 0;
}

int dms_custom_send_cert_flag_to_device(u32 dev_id, char *buf, int size)
{
    int ret;
    DMS_FEATURE_S feature_cfg = {0};
    struct dms_hal_device_info_stru flag_data = {0};
    u32 len = DMS_HAL_DEV_INFO_HEAD_LEN + sizeof(unsigned int);
    unsigned int sign_flag = 0;

    ret = sscanf_s(buf, "verify_flag=%u", &sign_flag);
    if (ret != 0x1) {
        dms_err("Get dms cert flag failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    feature_cfg.main_cmd = DMS_GET_SET_DEVICE_INFO_CMD;
    feature_cfg.sub_cmd = ZERO_CMD;
    feature_cfg.owner_name = DMS_URD_FORWARD_CMD_NAME;
    feature_cfg.filter = "main_cmd=0x35,sub_cmd=0x2,init";
    feature_cfg.privilege = DMS_ACC_ROOT | DMS_ENV_ALL | DMS_VDEV_ALL;
    feature_cfg.proc_ctrl_str = NULL;
    feature_cfg.handler = dms_send_msg_to_device_by_h2d_kernel;
    flag_data.dev_id = dev_id;
    flag_data.module_type = 0x35;
    flag_data.info_type = 0x2;
    flag_data.buff_size = sizeof(unsigned int);
    ret = memcpy_s(flag_data.payload, sizeof(flag_data.payload), &sign_flag, sizeof(unsigned int));
    if (ret != 0) {
        dms_err("Memcpy fail. (ret=%d; size=%u; param_size=%u)\n",
            ret,
            (u32)sizeof(flag_data.payload),
            (u32)sizeof(unsigned int));
        return -ENOMEM;
    }

    ret = dms_send_msg_to_device_by_h2d_kernel((void *)&feature_cfg, (char *)&flag_data, len, (char *)&flag_data, len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Send cert flag to device failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return 0;
}

int dms_custom_init_device_cert_flag(u32 dev_id)
{
    int ret;
    char *buf = NULL;
    int file_exist = 0;
    unsigned long long file_size = 0;
    char file_path[CUSTOM_SIGN_CONF_FILE_PATH_MAX] = {0};

    ret = sprintf_s(file_path, CUSTOM_SIGN_CONF_FILE_PATH_MAX, CUSTOM_SIGN_CONF_FLAG_PATH, dev_id);
    if (ret < 0) {
        dms_err("Failed to invoke snprintf_s. (dev_id=%u; ret=%d).\n", dev_id, ret);
        return -EINVAL;
    }

    ret = dms_custom_is_file_exist(file_path, &file_exist);
    if (ret != 0) {
        dms_warn("Check file status unsuccessfully. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (file_exist == 0) {
        dms_warn("File is not exist. (dev_id=%u)\n", dev_id);
        return 0;
    }

    ret = dms_custom_get_file_size(file_path, &file_size);
    if (ret != 0) {
        dms_err("Get file size failed. (dev_id=%u; file_path=%s)\n", dev_id, file_path);
        return ret;
    }

    if ((file_size == 0) || (file_size > CUSTOM_SIGN_CONF_FILE_SIZE)) {
        dms_err("File size is invalid. (dev_id=%u; file_size=0x%llx)\n", dev_id, file_size);
        return -EINVAL;
    }

    buf = (char *)dbl_kzalloc((size_t)file_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buf == NULL) {
        dms_err("Alloc memory for file failed. (dev_id=%u; file_size=0x%llx)\n", dev_id, file_size);
        return -ENOMEM;
    }

    ret = dms_custom_read_file(file_path, buf, file_size);
    if (ret != 0) {
        dms_err("Read file data failed. (dev_id=%u; file_name=%s)\n", dev_id, file_path);
        dbl_kfree(buf);
        buf = NULL;
        return ret;
    }

    ret = dms_custom_send_cert_flag_to_device(dev_id, buf, file_size);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Send cert flag to device failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }

    dbl_kfree(buf);
    buf = NULL;
    return ret;
}

int dms_custom_init_device_cert_file(u32 dev_id, CUSTOM_FILE_TYPE file_type)
{
    int ret;
    char *buf = NULL;
    int file_exist = 0;
    unsigned long long file_size = 0;
    char file_path[CUSTOM_SIGN_CONF_FILE_PATH_MAX] = {0};

    switch (file_type) {
        case CUSTOM_FILE_TYPE_USR_CERT:
            ret = sprintf_s(file_path, CUSTOM_SIGN_CONF_FILE_PATH_MAX, CUSTOM_SIGN_CONF_USER_CERT_PATH, dev_id);
            break;
        default:
            dms_err("File type is invalid. (dev_id=%u; file_type=%d)\n", dev_id, file_type);
            return -EINVAL;
    }

    if (ret < 0) {
        dms_err("Failed to invoke snprintf_s. (dev_id=%u; file_type=%d; ret=%d).\n", dev_id, file_type, ret);
        return -EINVAL;
    }

    ret = dms_custom_is_file_exist(file_path, &file_exist);
    if (ret != 0) {
        dms_warn("Check file status unsuccessfully. (dev_id=%u; file_type=%d; ret=%d)\n", dev_id, file_type, ret);
        return ret;
    }

    if (file_exist == 0) {
        dms_warn("File is not exist. (dev_id=%u; file_type=%d; ret=%d)\n", dev_id, file_type, ret);
        return 0;
    }

    ret = dms_custom_get_file_size(file_path, &file_size);
    if (ret != 0) {
        dms_err("Get file size failed. (dev_id=%u; file_type=%d; file_path=%s)\n", dev_id, file_type, file_path);
        return ret;
    }

    if ((file_size == 0) || (file_size > CUSTOM_SIGN_CONF_FILE_SIZE)) {
        dms_warn("File size is invalid. (dev_id=%u; file_type=%d; file_size=0x%llx)\n", dev_id, file_type, file_size);
        return -EINVAL;
    }

    buf = (char *)dbl_kzalloc((size_t)file_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buf == NULL) {
        dms_err("Alloc memory for file failed. (dev_id=%u; file_type=%d; file_size=0x%llx)\n",
            dev_id,
            file_type,
            file_size);
        return -ENOMEM;
    }

    ret = dms_custom_read_file(file_path, buf, file_size);
    if (ret != 0) {
        dms_err("Read file data failed. (dev_id=%u; file_type=%d; file_name=%s)\n", dev_id, file_type, file_path);
        dbl_kfree(buf);
        buf = NULL;
        return ret;
    }

    ret = dms_custom_send_cert_file_to_device(dev_id, file_type, buf, file_size);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Send cert file to device failed. (dev_id=%u; file_type=%d; ret=%d)\n", dev_id, file_type, ret);
    }

    dbl_kfree(buf);
    buf = NULL;
    return ret;
}

STATIC int dms_host_clear_cert_info(u32 udev_id)
{
    int ret;
    const char *default_flag_str = "verify_flag=5";
    char file_default_data = '0';
 
    ret = dms_save_sign_flag_to_file(udev_id, (char *)default_flag_str, strlen(default_flag_str));
    if (ret != 0) {
        dms_err("Set sign flag to default value failed. (dev_id=%u; ret=%d)\n", udev_id, ret);
        return ret;
    }
 
    ret = dms_save_sign_cert_to_file(udev_id, DSMI_SEC_SUB_CMD_CUST_SIGN_USER_CERT, &file_default_data, 0);
    if (ret != 0) {
        dms_err("Set user cert to default file failed. (dev_id=%u; ret=%d)\n", udev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int dms_custom_dev_init(u32 udev_id)
{
    int ret;

    dms_info("dms_custom_dev_init start.\n");
    mutex_init(&g_cert_sync_lock[udev_id]);
    ret = dms_custom_init_device_cert_flag(udev_id);
    if (ret != 0) {
        dms_warn("Init device cert flag unsuccessfully. (udev_id=%u; ret=%d)\n", udev_id, ret);
        if (ret == -EOPNOTSUPP) {
            dms_warn("Clear host custom sign info. (udev_id=%u)\n", udev_id);
            dms_host_clear_cert_info(udev_id);
        }

        return ret;
    }

    ret = dms_custom_init_device_cert_file(udev_id, CUSTOM_FILE_TYPE_USR_CERT);
    if (ret != 0) {
        dms_warn("Init device user cert file . (udev_id=%u; ret=%d)\n", udev_id, ret);
        if (ret == -EOPNOTSUPP) {
            dms_warn("Clear host custom sign info. (udev_id=%u)\n", udev_id);
            dms_host_clear_cert_info(udev_id);
        }
    }

    dms_info("dms_custom_dev_init success.\n");

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(dms_custom_dev_init, FEATURE_LOADER_STAGE_5);

STATIC void dms_custom_dev_exit(u32 udev_id)
{
    dms_info("Dms custom dev exit. (udev_id=%u)\n", udev_id);
    mutex_destroy(&g_cert_sync_lock[udev_id]);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(dms_custom_dev_exit, FEATURE_LOADER_STAGE_5);
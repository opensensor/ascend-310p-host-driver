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
 
#ifndef DMS_CUSTOM_COMMON_H
#define DMS_CUSTOM_COMMON_H
 
#define DMS_CUSTOM_FORWARD_CMD_NAME "DMS_CUSTOM_FORWARD_CMD_NAME"
 
#define CUSTOM_SIGN_CONF_FLAG_PATH "/var/asdrv/dev%u/device-sw-plugin/verify/flag"
#define CUSTOM_SIGN_CONF_VENDOR_CERT_PATH "/var/asdrv/dev%u/device-sw-plugin/verify/vendor-certificate"
#define CUSTOM_SIGN_CONF_USER_CERT_PATH "/var/asdrv/dev%u/device-sw-plugin/verify/user-certificate"
#define CUSTOM_SIGN_CONF_CMTY_CERT_PATH "/var/asdrv/dev%u/device-sw-plugin/verify/community-certificate"
#define CUSTOM_SIGN_CONF_FILE_PATH_MAX 128
#define CUSTOM_SIGN_CONF_VALLUE_MAX_LEN 128
#define CUSTOM_SIGN_CONF_FILE_SIZE 8192
#define CUSTOM_SIGN_CONF_DIR_MODE 0755
#define MAX_PACKET_SIZE 300

#define DSMI_SEC_SUB_CMD_CUST_SIGN_USER_CERT 3
#define DSMI_SEC_SUB_CMD_CUST_SIGN_VENDOR_CERT 4
#define DSMI_SEC_SUB_CMD_CUST_SIGN_CMTY_CERT 5
#define MAX_DEVICE_NUM 1140

typedef enum {
    CUSTOM_FILE_TYPE_FLAG = 0,
    CUSTOM_FILE_TYPE_VENDOR_CERT,
    CUSTOM_FILE_TYPE_USR_CERT,
    CUSTOM_FILE_TYPE_MAX,
} CUSTOM_FILE_TYPE;

typedef enum {
    SIGN_FLAG_DISABLED = 0,
    SIGN_FLAG_ONLY_VENDOR = 1,
    SIGN_FLAG_ONLY_USER = 2,
    SIGN_FLAG_VENDOR_OR_USER = 3,
    SIGN_FLAG_ONLY_CMTY = 4,
    SIGN_FLAG_VENDOR_OR_CMTY = 5,
    SIGN_FLAG_USER_OR_CMTY = 6,
    SIGN_FLAG_ALL = 7,
    SIGN_FLAG_MAX
} SIGN_FLAG_TYPE;

int dms_custom_forward_init(void);
void dms_custom_forward_uninit(void);

int dms_save_sign_flag_to_file(unsigned int device_id, char *buffer, unsigned int buff_size);
int dms_save_sign_cert_to_file(unsigned int device_id, unsigned int sub_cmd, char *buffer, unsigned int buff_size);
int dms_custom_get_file_name(unsigned int dev_id, unsigned int sub_cmd, char *file_name);
int dms_custom_is_file_exist(const char *file_name, int *is_exist);
int dms_custom_get_file_size(const char *file_name,  unsigned long long *file_size);
int dms_custom_read_file(const char *file_name, char *buf,  unsigned int size);
int dms_sign_write_file(char *file_path, char *buffer, int size);
int create_dir_recursive(const char *path);
int search_key_and_get_value(char *buff, const char *split, const char *key, char *value, int len);

#endif
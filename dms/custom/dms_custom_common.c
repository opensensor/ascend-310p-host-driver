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

#include "securec.h"
#include "dms_custom_common.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_custom_common.h"

#include "ka_fs_pub.h"

int dms_save_sign_flag_to_file(unsigned int device_id, char *buffer, unsigned int buff_size)
{
    int ret;
    char dst_path[CUSTOM_SIGN_CONF_FILE_PATH_MAX] = {0};

    ret = snprintf_s(dst_path, CUSTOM_SIGN_CONF_FILE_PATH_MAX, CUSTOM_SIGN_CONF_FILE_PATH_MAX - 1, CUSTOM_SIGN_CONF_FLAG_PATH, device_id);
    if (ret < 0) {
        dms_err("Failed to invoke snprintf_s, (device_id=%u; ret=%d).\n", device_id, ret);
        return -EINVAL;
    }

    ret = dms_sign_write_file(dst_path, buffer, buff_size);
    if (ret != 0) {
        dms_err("Failed to write sign flag to file, (dst_path=%s).\n", dst_path);
        return -EINVAL;
    }
    return 0;
}

int dms_save_sign_cert_to_file(unsigned int device_id, unsigned int sub_cmd, char *buffer, unsigned int buff_size)
{
    int ret;
    char dst_path[CUSTOM_SIGN_CONF_FILE_PATH_MAX] = {0};

    ret = dms_custom_get_file_name(device_id, sub_cmd, dst_path);
    if (ret < 0) {
        dms_err("Failed to invoke dms_custom_get_file_name, (device_id=%u; ret=%d).\n", device_id, ret);
        return -EINVAL;
    }

    ret = dms_sign_write_file(dst_path, buffer, buff_size);
    if (ret != 0) {
        dms_err("Failed to write sign cert to file, (dst_path=%s).\n", dst_path);
        return -EINVAL;
    }
    return 0;
}

int dms_custom_get_file_name(unsigned int dev_id, unsigned int sub_cmd, char *file_name)
{
    int ret;
    switch(sub_cmd) {
        case DSMI_SEC_SUB_CMD_CUST_SIGN_VENDOR_CERT:
            ret = snprintf_s(file_name, CUSTOM_SIGN_CONF_FILE_PATH_MAX, CUSTOM_SIGN_CONF_FILE_PATH_MAX - 1, CUSTOM_SIGN_CONF_VENDOR_CERT_PATH, dev_id);
            break;
        case DSMI_SEC_SUB_CMD_CUST_SIGN_USER_CERT:
            ret = snprintf_s(file_name, CUSTOM_SIGN_CONF_FILE_PATH_MAX, CUSTOM_SIGN_CONF_FILE_PATH_MAX - 1, CUSTOM_SIGN_CONF_USER_CERT_PATH, dev_id);
            break; 
        case DSMI_SEC_SUB_CMD_CUST_SIGN_CMTY_CERT:
            ret = snprintf_s(file_name, CUSTOM_SIGN_CONF_FILE_PATH_MAX, CUSTOM_SIGN_CONF_FILE_PATH_MAX - 1, CUSTOM_SIGN_CONF_CMTY_CERT_PATH, dev_id);
            break;
        default:
            dms_err("File type invalid. (dev_id=%u; sub_cmd=%u)\n", dev_id, sub_cmd);
            return -EINVAL;
    }
    return ret;
}

int dms_custom_is_file_exist(const char *file_name, int *is_exist)
{
    int ret;
    struct file *filp = NULL;
 
    filp = filp_open(file_name, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL(filp)) {
        ret = (int)PTR_ERR(filp);
        if (ret == -ENOENT) {
            *is_exist = 0;
            return 0;
        }
        *is_exist = 1;
        return -EINVAL;
    }
 
    (void)filp_close(filp, NULL);
    filp = NULL;
    *is_exist = 1;
    return 0;
}
 
int dms_custom_get_file_size(const char *file_name,  unsigned long long *file_size)
{
    struct file *filp = NULL;
 
    /* the file may not exist, so print warning */
    filp = filp_open(file_name, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL(filp)) {
        dms_err("Unable to open file. (file_name=%s; errno=%ld)\n", file_name, PTR_ERR(filp));
        return (int)PTR_ERR(filp);
    }
 
    /* get file total length */
    if (filp->f_inode == NULL) {
        (void)filp_close(filp, NULL);
        filp = NULL;
        dms_err("File inode is NULL.\n");
        return -ENOENT;
    }
 
    *file_size = (unsigned long long)filp->f_inode->i_size;
    (void)filp_close(filp, NULL);
    filp = NULL;
    return 0;
}

int dms_custom_read_file(const char *file_name, char *buf,  unsigned int size)
{
    struct file *filp = NULL;
    loff_t offset_tmp = 0;
    ssize_t result;
 
    filp = filp_open(file_name, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL(filp)) {
        dms_err("Unable to open file. (file_name=%s; errno=%ld)\n", file_name, PTR_ERR(filp));
        return (int)PTR_ERR(filp);
    }
 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    result = kernel_read(filp, buf, size, &offset_tmp);
#else
    result = kernel_read(filp, offset_tmp, buf, (unsigned long)size);
#endif
    if (result != (ssize_t)size) {
        dms_err("Kernel read file error. (result=%ld; size=%u)\n", result, size);
        (void)filp_close(filp, NULL);
        filp = NULL;
        return -EINVAL;
    }
 
    (void)filp_close(filp, NULL);
    filp = NULL;
    return 0;
}

int dms_sign_write_file(char *file_path, char *buffer, int size)
{
    struct file *filp = NULL;
    loff_t offset_tmp = 0;
    int ret;
    int rw_len;

    filp = filp_open(file_path, O_WRONLY | KA_O_TRUNC, 0);
    if (IS_ERR_OR_NULL(filp)) {
        ret = (int)PTR_ERR(filp);
        if (ret != -ENOENT) {
            dms_err("Unable to open file. (file_name=%s; err=%ld)\n", file_path, PTR_ERR(filp));
        }   
        return ret;
    }
 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    rw_len = kernel_write(filp, buffer, size, &offset_tmp);
#else
    rw_len = kernel_write(filp, buffer, size, offset_tmp);
#endif
 
    (void)filp_close(filp, NULL);
    filp = NULL;
 
    if (rw_len != size) {
        dms_err("File write failed. (file_name=%s; rw_len=%d; w_len=%d)\n", file_path, rw_len, size);
        return -EINVAL;
    }
 
    return 0;
}

int search_key_and_get_value(char *buff, const char *split, const char *key, char *value, int len)
{
    char *left = NULL;
    char *right = NULL;
    char *p_save = NULL;

    if (len <= 0 || len > CUSTOM_SIGN_CONF_VALLUE_MAX_LEN) {
        dms_err("The variable len is invalid. (len=%d)\n", len);
        return -EINVAL;
    }

    if (strlen(buff) == 0) {
        return -EINVAL;
    }
    /* Check if the words is end of '\n' or not. */
    if (buff[strlen(buff) - 1] == '\n') {
        buff[strlen(buff) - 1] = '\0';
    }
    /* Check if it owns the key words or not. */
    if (strstr(buff, key) == NULL) {
        return -EINVAL;
    }
    /* Take the first part of the separator. */
    left  = strtok_s(buff, split, &p_save);
    if (left == NULL) {
        return -EINVAL;
    }
    /* Compare key-words. */
    if (strcmp(key, left) != 0) {
        return -EINVAL;
    }
    /* To get wanted-string. */
    if ((right = strtok_s(NULL, split, &p_save)) != NULL) {
        if (strcpy_s(value, len, right) != 0) {
            dms_err("Failed to invoke strcpy_s to copy the variable right.\n");
            return -EINVAL;
        }
        return 0;
    }
    return -EINVAL;
}
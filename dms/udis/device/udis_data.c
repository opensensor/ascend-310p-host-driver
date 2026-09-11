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
 * Create: 2025-07-14
 */
#include "securec.h"
#include "uda.h"
#include "urd_acc_ctrl.h"
#include "udis_log.h"
#include "udis_management.h"

#include "ka_kernel_def_pub.h"

STATIC int udis_check_input_valid(unsigned int udevid, UDIS_MODULE_TYPE module_type,
                                        const struct udis_dev_info *input_info)
{
    unsigned int name_len = 0;

    if (!uda_is_udevid_exist(udevid)) {
        udis_err("udis_info input udevid not exist. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    if (module_type < 0 || module_type >= UDIS_MODULE_MAX) {
        udis_err("udis_info input module_type invalid. (udevid=%u; module_type=%u; module_type_max=%d)\n",
                     udevid, module_type, UDIS_MODULE_MAX);
        return -EINVAL;
    }

    if (input_info == NULL) {
        udis_err("udis_info input input_info is NULL.(udevid=%u; module_type=%u; ret=%d)\n", udevid, module_type,
                    -EINVAL);
        return -EINVAL;
    }

    name_len = strnlen(input_info->name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        udis_err("name length is invalid. (udevid=%u; module_type=%u; name_len=%u; min_len=%u; max_len=%u)\n",
                udevid, module_type, name_len, 1, UDIS_MAX_NAME_LEN - 1);
        return -EINVAL;
    }

    if (input_info->data_len > UDIS_MAX_DATA_LEN) {
        udis_err("udis_info input Data length is too large. (udevid=%u; module_type=%u; name=%s; data_len=%d)\n",
                udevid, module_type, input_info->name, input_info->data_len);
        return -EINVAL;
    }
    return 0;
}

STATIC int udis_check_ucb(u32 udevid, const struct udis_ctrl_block *ucb)
{
    int ret = 0;
    if (ucb == NULL) {
        ret = -ENODEV;
        udis_err("udis get ctrl block failed, ucb = NULL. (udevid=%u)\n", udevid);
        return ret;
    }

    return ret;
}

STATIC struct udis_info_stu* udis_find_info_block(const struct udis_ctrl_block* ucb, UDIS_MODULE_TYPE module_type,
                                                    const char* name)
{
    struct udis_info_stu *module_block, *info_block_tmp;
    struct udis_info_stu *info_block = NULL;
    int i = 0;

    module_block = ucb->udis_info_buf + UDIS_NAME_NUM_MAX * module_type;
    if (module_block < ucb->udis_info_buf) {
        udis_err("Invalid memory access to module_block. (module_type=%u)\n", module_type);
        return NULL;
    }

    for (; i < UDIS_NAME_NUM_MAX; i++) {
        info_block_tmp = module_block + i;
        if (info_block_tmp < module_block) {
            udis_err("Invalid memory access to info_block_tmp. (module_type=%u; name_index=%d)\n", module_type, i);
            break;
        }

        if (strcmp(info_block_tmp->name, name) == 0) {
            info_block = info_block_tmp;
            break;
        }
    }
    return info_block;
}

STATIC struct udis_info_stu* udis_find_empty_block(const struct udis_ctrl_block* ucb, UDIS_MODULE_TYPE module_type)
{
    struct udis_info_stu *module_block, *empty_block_tmp;
    struct udis_info_stu *empty_block = NULL;
    int i = 0;
    char zeros_arr[UDIS_MAX_NAME_LEN] = {0};

    module_block = ucb->udis_info_buf + UDIS_NAME_NUM_MAX * module_type;
    if (module_block < ucb->udis_info_buf) {
        udis_err("Invalid memory access to module_block. (module_type=%u)\n", module_type);
        return NULL;
    }

    for (; i < UDIS_NAME_NUM_MAX; i++) {
        empty_block_tmp = module_block + i;
        if (empty_block_tmp < module_block) {
            udis_err("Invalid memory access to empty_block_tmp. (module_type=%u; i=%d)\n", module_type, i);
            break;
        }

        if (memcmp(empty_block_tmp->name, zeros_arr, UDIS_MAX_NAME_LEN) == 0) {
            empty_block = empty_block_tmp;
            break;
        }
    }

    return empty_block;
}

STATIC int udis_store_info(const struct udis_dev_info *src_info, struct udis_info_stu* dst_info_block)
{
    int ret = 0;
    struct udis_info_stu info_block_tmp = { 0 };

    if (src_info->update_type < 0 || src_info->update_type >= UPDATE_TYPE_MAX) {
        udis_err("udis update_type check invalid. (update_type=%u; update_type_max=%d)\n",
            src_info->update_type, UPDATE_TYPE_MAX);
        return -EINVAL;
    }
    info_block_tmp.update_type = src_info->update_type;

    if (src_info->name == NULL) {
        udis_err("udis src_info name is NULL.\n");
        return -EINVAL;
    }

    if (strlen(src_info->name) > UDIS_MAX_NAME_LEN - 1) {
        udis_err("udis src_info name len is invalid. (name=%s; max_name_len=%d)\n", src_info->name, UDIS_MAX_NAME_LEN);
        return -EINVAL;
    }

    ret = strcpy_s(info_block_tmp.name, UDIS_MAX_NAME_LEN, src_info->name);
    if (ret != 0) {
        udis_err("strcpy_s src_info->name failed. (name=%s)\n", src_info->name);
        return -EIO;
    }

    ret = memcpy_s(info_block_tmp.data, UDIS_MAX_DATA_LEN, src_info->data, src_info->data_len);
    if (ret != 0) {
        udis_err("memcpy_s src_info->data failed.(name=%s)\n", src_info->name);
        return -EIO;
    }

    info_block_tmp.acc_ctrl = src_info->acc_ctrl;
    info_block_tmp.data_len = src_info->data_len;
    info_block_tmp.last_update_time = ktime_get_raw_ns() / NSEC_PER_MSEC;

    ret = memcpy_s(dst_info_block, UDIS_NAME_OFFSET, &info_block_tmp, UDIS_NAME_OFFSET);
    if (ret != 0) {
        udis_err("memcpy_s info_block_tmp failed. (name=%s)\n", src_info->name);
        return -EIO;
    }

    return 0;
}

STATIC int udis_load_info_block(u32 udevid, UDIS_MODULE_TYPE module_type,
                                    const struct udis_info_stu *src_info_block, struct udis_dev_info *dst_info)
{
    int ret = 0;

    ret = memcpy_s(dst_info->data, UDIS_MAX_DATA_LEN, src_info_block->data, src_info_block->data_len);
    if (ret != 0) {
        ret = -EIO;
        udis_err("udis dst_info memcpy_s error. (udevid=%u; module_type=%u; name=%s)\n",
                    udevid, module_type, src_info_block->name);
        return ret;
    }
    dst_info->data_len = src_info_block->data_len;
    dst_info->acc_ctrl = src_info_block->acc_ctrl;
    dst_info->update_type = src_info_block->update_type;
    dst_info->last_update_time = src_info_block->last_update_time;
    return ret;
}

int hal_kernel_set_udis_info(unsigned int udevid, UDIS_MODULE_TYPE module_type,
                                const struct udis_dev_info *udis_set_info)
{
    int ret = 0;
    struct udis_ctrl_block* ucb = NULL;
    struct udis_info_stu* info_block = NULL;

#ifndef CFG_UDIS_SUPPORT_VF
    if (!uda_is_phy_dev(udevid)) {
        return -EOPNOTSUPP;
    }
#endif

    ret = udis_check_input_valid(udevid, module_type, udis_set_info);
    if (ret != 0) {
        return ret;
    }

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    ret = udis_check_ucb(udevid, ucb);
    if (ret != 0) {
        udis_cb_read_unlock(udevid);
        return ret;
    }

    down_write(&ucb->udis_info_lock);
    info_block = udis_find_info_block(ucb, module_type, udis_set_info->name);
    if (info_block == NULL) {
        info_block = udis_find_empty_block(ucb, module_type);
        if (info_block == NULL) {
            ret = -ENOSPC;
            udis_err("No empty block for new name. (udevid=%u; module_type=%u; name=%s)\n",
                        udevid, module_type, udis_set_info->name);
            goto udis_info_unlock;
        }
    }

    ret = udis_store_info(udis_set_info, info_block);
    if (ret != 0) {
        udis_err("udis_store_info failed. (udevid=%u; moduletype=%u; name=%s; ret=%d)\n",
                    udevid, module_type, udis_set_info->name, ret);
    }

udis_info_unlock:
    up_write(&ucb->udis_info_lock);
    udis_cb_read_unlock(udevid);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_set_udis_info);

int hal_kernel_get_udis_info(unsigned int udevid, UDIS_MODULE_TYPE module_type, struct udis_dev_info *info_request)
{
    int ret = 0;
    struct udis_ctrl_block* ucb = NULL;
    struct udis_info_stu* info_block = NULL;

#ifndef CFG_UDIS_SUPPORT_VF
    if (!uda_is_phy_dev(udevid)) {
        return -EOPNOTSUPP;
    }
#endif

    ret = udis_check_input_valid(udevid, module_type, info_request);
    if (ret != 0) {
        return ret;
    }

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    ret = udis_check_ucb(udevid, ucb);
    if (ret != 0) {
        goto unlock_ucb;
    }

    down_read(&(ucb->udis_info_lock));
    info_block = udis_find_info_block(ucb, module_type, info_request->name);
    if (info_block == NULL) {
        ret = -ENODATA;
        udis_debug("This module has no such name. (udevid=%u; type=%u; name=%s)\n",
            udevid, module_type, info_request->name);
        goto unlock_udis_info;
    }

    ret = dms_feature_access_identify(info_block->acc_ctrl, 0);
    if (ret != 0) {
        udis_ex_notsupport_err(ret, "Access identify failed. (udevid=%u; type=%u; name=%s; ret=%d; acc_ctrl=0x%x)\n",
            udevid, module_type, info_request->name, ret, info_block->acc_ctrl);
        goto unlock_udis_info;
    }

    ret = udis_load_info_block(udevid, module_type, info_block, info_request);
    if (ret != 0) {
        udis_err("udis_load_info_block failed. (udevid=%u; type=%u; name=%s; ret=%d)\n",
                    udevid, module_type, info_request->name, ret);
    }

unlock_udis_info:
    up_read(&ucb->udis_info_lock);
unlock_ucb:
    udis_cb_read_unlock(udevid);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_udis_info);

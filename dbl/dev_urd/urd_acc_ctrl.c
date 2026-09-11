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

#include <linux/slab.h>

#ifndef AOS_LLVM_BUILD
#  include <linux/dmi.h>
#else
#  include <linux/fs.h>
#endif

#include "securec.h"
#include "ascend_hal_error.h"
#include "urd_define.h"
#include "urd_init.h"
#include "davinci_api.h"
#include "urd_acc_ctrl.h"
#include "urd_container.h"

#include "ka_kernel_def_pub.h"
#include "ka_driver_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"

#ifndef CFG_HOST_ENV
#  include "drv_whitelist.h"
#endif

#define CHECK_PERI(user, profile, mask, ret_value)                                  \
    do {                                                                 \
        if ((((profile) & (mask)) == 0) && (((user) & (mask)) != 0)) { \
            return ret_value;                        \
        }                                                                \
    } while (0)

static inline s32 vdevice_access_check(u32 user_acc, u32 profile)
{
    /* not vdevice env, return OK */
    if ((user_acc & DMS_VDEV_MASK) == 0) {
        return 0;
    }

    CHECK_PERI(user_acc, profile, DMS_VDEV_PHYSICAL, DRV_ERROR_NOT_SUPPORT);
    CHECK_PERI(user_acc, profile, DMS_VDEV_VIRTUAL, DRV_ERROR_NOT_SUPPORT);
    CHECK_PERI(user_acc, profile, DMS_VDEV_DOCKER, DRV_ERROR_NOT_SUPPORT);
    return 0;
}

static inline s32 runtime_env_check(u32 user_acc, u32 profile)
{
    /* run env not set, para error */
    if ((user_acc & DMS_RUN_ENV_MASK) == 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    CHECK_PERI(user_acc, profile, DMS_ENV_PHYSICAL, DRV_ERROR_NOT_SUPPORT);
    CHECK_PERI(user_acc, profile, DMS_ENV_VIRTUAL, DRV_ERROR_NOT_SUPPORT);
    CHECK_PERI(user_acc, profile, DMS_ENV_ADMIN_DOCKER, DRV_ERROR_NOT_SUPPORT);
    CHECK_PERI(user_acc, profile, DMS_ENV_DOCKER, DRV_ERROR_NOT_SUPPORT);
    return 0;
}

static inline s32 user_group_identify(u32 user_acc, u32 profile)
{
    /* user right not set, para error */
    if ((user_acc & DMS_ACC_MASK) == 0) {
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    /* Use Kernel error code, the user can get accurate error code by DmsIoctl()/DmsIoctlConvertErrno().
       Use Drv error code, the user can get accurate error code by DmsIoctlConvertErrno(). */
    CHECK_PERI(user_acc, profile, DMS_ACC_LIMIT_USER, (-EPERM));
    CHECK_PERI(user_acc, profile, DMS_ACC_USER, DRV_ERROR_OPER_NOT_PERMITTED);
    CHECK_PERI(user_acc, profile, DMS_ACC_OPERATE, DRV_ERROR_OPER_NOT_PERMITTED);
    CHECK_PERI(user_acc, profile, DMS_ACC_ROOT, DRV_ERROR_OPER_NOT_PERMITTED);
    CHECK_PERI(user_acc, profile, DMS_ACC_DM_USER, DRV_ERROR_OPER_NOT_PERMITTED);
    return 0;
}

static inline bool dms_check_in_virtual_machine(void)
{
#ifndef AOS_LLVM_BUILD
    if (ka_driver_dmi_match(DMI_SYS_VENDOR, DMI_VIRTUAL_SYSVENDOR))  {
        return true;
    }
    return false;
#else
    return false;
#endif
}

static inline bool dms_check_in_container(void)
{
#ifndef AOS_LLVM_BUILD
    if (urd_container_is_in_container() == true) {
        return true;
    }
#endif
    return false;
}

static inline bool dms_check_in_admin_container(void)
{
#ifndef AOS_LLVM_BUILD
    if (urd_container_is_in_admin_container() == true) {
        return true;
    }
#endif
    return false;
}

STATIC u32 dms_get_run_env(void)
{
    u32 env;

    if (dms_check_in_container()) {
        env = DMS_ENV_DOCKER;
    } else if (dms_check_in_admin_container()) {
        env = DMS_ENV_ADMIN_DOCKER;
    } else if (dms_check_in_virtual_machine()) {
        env = DMS_ENV_VIRTUAL;
    } else {
        env = DMS_ENV_PHYSICAL;
    }
    return env;
}

STATIC u32 dms_get_user_role(void)
{
#ifndef AOS_LLVM_BUILD
    const ka_cred_t *cred = current_cred();
    u32 acc;

    /* check user role */
    if ((cred != NULL) && (cred->euid.val == 0)) {
        acc = DMS_ACC_ROOT;
    } else {
        if ((cred != NULL) && davinci_intf_confirm_user()) {
            if (davinci_intf_get_manage_group() == cred->egid.val) {
                acc = DMS_ACC_DM_USER;
            } else {
                acc = DMS_ACC_OPERATE;
            }
        } else {
            acc = DMS_ACC_USER;
        }
    }
    return acc;
#else
    return DMS_ACC_ROOT;
#endif
}

static bool dms_check_in_vdev_docker(void)
{
#ifndef AOS_LLVM_BUILD
    u32 phyid = 0, vfid = 0;

    if (!dms_check_in_container()) {
        return false;
    }

    if (urd_container_logical_id_to_physical_id(0, &phyid, &vfid) != 0) {
        return false;
    }

    if ((vfid != 0) || (!urd_is_pf_device(phyid))) {
        return true;
    }
#endif
    return false;
}

STATIC bool dms_check_in_vdev_virtual_machine(void)
{
#ifndef AOS_LLVM_BUILD
    u32 phyid = 0, vfid = 0;

    if (!dms_check_in_virtual_machine()) {
        return false;
    }

    if (urd_container_logical_id_to_physical_id(0, &phyid, &vfid) != 0) {
        return false;
    }

    if ((vfid != 0) || (!urd_is_pf_device(phyid))) {
        return true;
    }
#endif
    return false;
}

static inline u32 dms_get_vdev_env(void)
{
    u32 env = 0;

    if (dms_check_in_vdev_docker()) {
        env = DMS_VDEV_DOCKER;
    } else if (dms_check_in_vdev_virtual_machine()) {
        env = DMS_VDEV_VIRTUAL;
    }
    return env;
}

static bool dms_role_is_restrict_access(u32 acc, u32 msg_source)
{
    if ((acc == DMS_ACC_ROOT) || (acc == DMS_ACC_DM_USER)) {
        return false;
    }
    return ((msg_source == MSG_FROM_USER_REST_ACC) ? true : false);
}

STATIC u32 dms_feature_access_get_acc(u32 msg_source)
{
    u32 acc;
    acc = dms_get_user_role();
    acc = (dms_role_is_restrict_access(acc, msg_source) ? DMS_ACC_LIMIT_USER : acc);
    acc |= dms_get_run_env();
    acc |= dms_get_vdev_env();
    return acc;
}

u32 dms_feature_access_identify(u32 feature_prof, u32 msg_source)
{
    int ret;
    u32 user_acc;

    user_acc = dms_feature_access_get_acc(msg_source);
    ret = vdevice_access_check(user_acc, feature_prof);
    if (ret != 0) {
        return ret;
    }

    ret = runtime_env_check(user_acc, feature_prof);
    if (ret != 0) {
        return ret;
    }

    ret = user_group_identify(user_acc, feature_prof);
    if (ret != 0) {
        return ret;
    }
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dms_feature_access_identify);
#ifndef CFG_HOST_ENV
static int dms_parse_string(char **str, const char *delim, char **out,
    u32 *out_len)
{
    const char *sep = NULL;
    char *s = NULL;
    char *tok = NULL;
    char sc, c;
    u32 max_len, count;

    if ((str == NULL) || (*str == NULL) || (delim == NULL) || (out == NULL) ||
        (out_len == NULL) || (*out_len == 0)) {
        return -EINVAL;
    }
    max_len = *out_len;
    *out_len = 0;
    count = 0;
    s = *str;
    for (tok = s; ((tok != NULL) && (count < max_len));) {
        c = *s++;
        sep = delim;
        do {
            sc = *sep++;
            if (sc != c) {
                continue;
            }

            if (c == '\0') {
                s = NULL;
            } else {
                s[-1] = 0;
            }

            *out++ = tok;
            count++;
            tok = s;
            break;
        } while (sc != 0);
    }
    *out_len = count;
    return 0;
}

STATIC int dms_feature_alloc_and_save_proc_name(char **process_name, int process_num,  char ***proc_ctl)
{
    int i;
    char **tmp_proc = NULL;

    tmp_proc = (char **)kzalloc(process_num * sizeof(char *), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (tmp_proc == NULL) {
        dms_err("kzalloc failed. (len=%lu)\n", process_num * sizeof(char *));
        return -ENOMEM;
    }
    *proc_ctl = tmp_proc;
    for (i = 0; i < process_num; i++) {
        tmp_proc[i] = process_name[i];
    }

    return 0;
}
#endif

int dms_feature_parse_proc_ctrl(const char *proc_str, char **buf, char ***proc_ctl, u32 *proc_num)
{
#ifndef CFG_HOST_ENV
    char *process_name[WL_NAME_NUM_MAX + 1] = {NULL};
    int ret;
    unsigned int process_num;
    size_t len;

    /* if proc_ctrl was NULL,means not use whitelist control */
    if (proc_str == NULL) {
        return 0;
    }
    if ((buf == NULL) || (proc_ctl == NULL) || (proc_num == NULL)) {
        return -EINVAL;
    }
    len = strnlen(proc_str, DMS_ACCESS_PROC_STRING_MAX);
    if (len >= DMS_ACCESS_PROC_STRING_MAX) {
        dms_err("Parse proc failed. (len=%lu)\n", len);
        return -EINVAL;
    }
    *buf = (char *)kzalloc(len + 1, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (*buf == NULL) {
        return -ENOMEM;
    }
    ret = strcpy_s(*buf, len + 1, proc_str);
    if (ret != 0) {
        dms_err("strcpy_s failed. (len=%lu)\n", len);
        goto ERR_OUT;
    }
    process_num = WL_NAME_NUM_MAX;
    ret = dms_parse_string(buf, ",", process_name, &process_num);
    if ((ret != 0) || (process_num == 0)) {
        dms_err("dms_parse_string failed. (ret=%d)\n", ret);
        ret = -ENOEXEC;
        goto ERR_OUT;
    }

    /* malloc proc_ctl, save the  process_name */
    ret = dms_feature_alloc_and_save_proc_name(process_name, process_num, proc_ctl);
    if (ret != 0) {
        dms_err("dms_feature_alloc_and_save_proc_name failed. (len=%lu)\n", process_num * sizeof(char *));
        goto ERR_OUT;
    }

    *proc_num = process_num;
    return 0;

ERR_OUT:
    kfree(*buf);
    *buf = NULL;
    return ret;
#else
    return 0;
#endif
}

u32 dms_feature_whitelist_check(const char **proc_ctrl, u32 proc_num)
{
    int ret = 0;
#ifndef CFG_HOST_ENV
    /* means no verification is required. */
    if (proc_ctrl == NULL) {
        return 0;
    }

    if ((proc_num == 0) || (proc_num > WL_NAME_NUM_MAX)) {
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }
#ifndef AOS_LLVM_BUILD
    ret = whitelist_process_handler(proc_ctrl, proc_num);
    if (ret != 0) {
        dms_err("Permission denied! ret = %d.\n", ret);
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }
#endif
#endif

    return ret;
}

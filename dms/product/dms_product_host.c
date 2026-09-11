/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2023-10-10
 * File Name     : dms_product_host.c
 * Version       : Initial product
 * Author        :
 * Created       : 2023-10-10
 * Last Modified :
 * Description   :

 *  History       :
 * 1.Date        : 2023-10-10
 *    Author      :
 *    Modification: Created file
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_acc_ctrl.h"
#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "dms_product.h"
#include "dms_product_host.h"

typedef struct {
    unsigned int card_multi_die_policy;
    struct mutex policy_mutex;
} card_multi_die_control;
 
typedef enum {
    CARD_MUITI_DIE_UNION_POLICY = 0,  // A3 多die必须同时通入容器
    CARD_MUITI_DIE_INDEP_POLICY = 1,  // A3 支持单die单独通入容器
    CARD_MULTI_DIE_POLICY_MAX = 2
} card_multi_die_policy;

card_multi_die_control g_multi_die_control = {0};

int devdrv_get_work_mode(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int work_mode;
    int ret;
#ifndef CFG_SOC_PLATFORM_CLOUD
    struct devdrv_manager_info *manager_info = NULL;
#endif

    if (out == NULL || (out_len != sizeof(unsigned int))) {
        dms_err("Out char is NULL or in_len is wrong.");
        return -EINVAL;
    }

#ifdef CFG_SOC_PLATFORM_CLOUD
    ret = devdrv_manager_get_amp_smp_mode(&work_mode);
    if (ret) {
        dms_err("Get amp smp mode failed. (ret=%d)\n", ret);
        return -EFAULT;
    }
#else
    manager_info = devdrv_get_manager_info();
    if (manager_info == NULL) {
        dms_err("Manager_info is Null\n");
        return -EINVAL;
    }
    work_mode = manager_info->amp_or_smp;
#endif

    ret = memcpy_s((void *)out, out_len, (void *)&work_mode, sizeof(unsigned int));
    if (ret) {
        dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return ret;
}

int devdrv_get_card_multi_die_policy(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    if (out == NULL || (out_len != sizeof(unsigned int))) {
        dms_err("Out char is NULL or out_len is wrong. (out_is_null=%d; out_len=%u)\n", (out == NULL), out_len);
        return -EINVAL;
    }
    mutex_lock(&g_multi_die_control.policy_mutex);
    ret = memcpy_s((void *)out, out_len, (void *)&g_multi_die_control.card_multi_die_policy,
        sizeof(unsigned int));
    if (ret) {
        dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
        mutex_unlock(&g_multi_die_control.policy_mutex);
        return -ENOMEM;
    }
    mutex_unlock(&g_multi_die_control.policy_mutex);
    return 0;
}

int devdrv_set_card_multi_die_policy(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int policy;
    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input char is null or in_len is wrong. (in_is_null=%d; in_len=%u)\n", (in == NULL), in_len);
        return -EINVAL;
    }
 
    policy = *(unsigned int*)in;
    if (policy >= CARD_MULTI_DIE_POLICY_MAX) {
        dms_err("Policy is wrong, policy, policy=%u\n", policy);
        return -EINVAL;
    }
    mutex_lock(&g_multi_die_control.policy_mutex);
    g_multi_die_control.card_multi_die_policy = policy;
    mutex_unlock(&g_multi_die_control.policy_mutex);
    return 0;
}
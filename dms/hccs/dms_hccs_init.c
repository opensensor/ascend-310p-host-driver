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
* Create: 2024-12-19
*/
#include <asm/io.h>
#include <linux/uaccess.h>

#include "ka_dfx_pub.h"

#include "securec.h"
#include "devdrv_user_common.h"
#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms_basic_info.h"
#include "dbl/feature_loader.h"
#include "devdrv_common.h"
#include "dms_notifier.h"
#include "dms_hccs_init.h"

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_HCCS)
BEGIN_FEATURE_COMMAND()
#ifdef CFG_HOST_ENV
ADD_FEATURE_COMMAND(DMS_MODULE_HCCS,
    DMS_GET_GET_DEVICE_INFO_CMD,
    ZERO_CMD,
    DMS_FILTER_HCCS_CREDIT_INFO,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_hccs_credit_info)
#else
ADD_FEATURE_COMMAND(DMS_MODULE_HCCS,
    DMS_GET_GET_DEVICE_INFO_CMD,
    ZERO_CMD,
    DMS_FILTER_HCCS,
    "dmp_daemon",
    DMS_SUPPORT_ALL,
    dms_feature_get_hccs_info)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()
int __attribute__((weak)) dms_hccs_credit_info_task_register(u32 dev_id);
int __attribute__((weak)) dms_hccs_credit_info_task_register(u32 dev_id)
{
    return 0;
}
int __attribute__((weak)) dms_hccs_credit_info_task_unregister(u32 dev_id);
int __attribute__((weak)) dms_hccs_credit_info_task_unregister(u32 dev_id)
{
    return 0;
}

int __attribute__((weak)) dms_get_hccs_credit_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    return -ENOTSUPP;
}

STATIC int dms_hccs_task_register(struct devdrv_info *dev_info)
{
    int ret = 0;
    if (dev_info == NULL) {
        dms_err("Invalid parameter. dev_info = %s\n", dev_info == NULL ? "null" : "not null");
        return -EINVAL;
    }

    ret = dms_hccs_credit_info_task_register(dev_info->dev_id);
    if (ret != 0) {
        dms_err("Dms hcss credit info task register failed. (dev_id=%u; ret=%d).\n", dev_info->dev_id, ret);
        return ret;
    }

#ifndef CFG_HOST_ENV
    ret = dms_hccs_statistic_task_register(dev_info->dev_id);
    if (ret != 0) {
		dms_err("dms hccs statistic init failed. (ret=%d)\n", ret);
		return ret;
	}
#endif

    return ret;
}

STATIC int dms_hccs_task_unregister(struct devdrv_info *dev_info)
{
    int ret = 0;
    if (dev_info == NULL) {
        dms_err("Invalid parameter. dev_info = %s\n", dev_info == NULL ? "null" : "not null");
        return -EINVAL;
    }

    ret = dms_hccs_credit_info_task_unregister(dev_info->dev_id);
    if (ret != 0) {
        dms_err("Dms timer task unregister failed, (dev_id=%u; ret=%d).\n", dev_info->dev_id, ret);
        return ret;
    }

#ifndef CFG_HOST_ENV
    ret = dms_hccs_statistic_task_unregister(dev_info->dev_id);
    if (ret != 0) {
        dms_err("Dms timer task unregister failed, (dev_id=%u; ret=%d).\n", dev_info->dev_id, ret);
        return ret;
    }
#endif

    return ret;
}

static int (*const dms_hccs_notifier_handle_func[DMS_DEVICE_NOTIFIER_MAX]) \
    (struct devdrv_info *dev_info) = {
        [DMS_DEVICE_UP3] = dms_hccs_task_register,
        [DMS_DEVICE_DOWN3] = dms_hccs_task_unregister,
};

STATIC int dms_hccs_notifier_handle(struct notifier_block *nb, unsigned long mode, void *data)
{
    struct devdrv_info *dev_info = (struct devdrv_info *)data;
    int ret;

    if ((data == NULL) || (mode == DMS_DEVICE_NOTIFIER_MIN) ||
        (mode >= DMS_DEVICE_NOTIFIER_MAX)) {
        dms_err("Invalid parameter. (mode=0x%lx; data=\"%s\")\n",
                mode, data == NULL ? "NULL" : "OK");
        return KA_NOTIFY_BAD;
    }

    if (mode != DMS_DEVICE_UP3 && mode != DMS_DEVICE_DOWN3) {
        return KA_NOTIFY_DONE;
    }

    ret = dms_hccs_notifier_handle_func[mode](dev_info);
    if (ret != 0) {
        dms_err("Credit num qurey task handle failed. (dev_id=%u; mode=%ld; ret=%d)\n",
            dev_info->dev_id, mode, ret);
        return KA_NOTIFY_BAD;
    }

    return KA_NOTIFY_DONE;
}

STATIC struct notifier_block g_dms_hccs_notifier = {
    .notifier_call = dms_hccs_notifier_handle,
};


STATIC int dms_hccs_init(void)
{
    int ret;
    dms_info("dms hccs init.\n");

#ifndef CFG_HOST_ENV
    ret = dms_hccs_feature_init();
    if (ret) {
        dms_err("dms hccs feature init failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

    CALL_INIT_MODULE(DMS_MODULE_HCCS);
    ret = dms_register_notifier(&g_dms_hccs_notifier);
    if (ret) {
        dms_err("register dms notifier failed. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_hccs_init, FEATURE_LOADER_STAGE_5);

STATIC void dms_hccs_exit(void)
{
    dms_info("dms hccs exit.\n");
    CALL_EXIT_MODULE(DMS_MODULE_HCCS);
#ifndef CFG_HOST_ENV
    dms_hccs_feature_exit();
#endif
    (void)dms_unregister_notifier(&g_dms_hccs_notifier);
    return;
}
DECLAER_FEATURE_AUTO_UNINIT(dms_hccs_exit, FEATURE_LOADER_STAGE_5);

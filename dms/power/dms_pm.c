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

#include <linux/suspend.h>
#include <linux/time.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

#include "ka_system_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"

#include "dms_define.h"
#include "dms_template.h"
#include "urd_acc_ctrl.h"
#include "dms_pm.h"
#include "devdrv_user_common.h"
#include "devdrv_ipc.h"
#include "drv_ipc.h"
#include "dms_cmd_def.h"
#include "dms_kernel_version_adapt.h"
#include "kernel_version_adapt.h"
#include "devdrv_ipc_msg.h"
#include "icm_interface.h"
#include "devdrv_manager.h"
#include "devdrv_manager_comm.h"

STATIC KA_TASK_DEFINE_MUTEX(g_power_state_mutex);
STATIC struct devdrv_power_handle g_power_state_handle[POWER_STATE_MODULE_MAX];
int devdrv_manager_register_power_handle(unsigned int module, int (*func)(struct drvdev_power_state_info *power_info))
{
    if ((func == NULL) || module >= POWER_STATE_MODULE_MAX) {
        dms_err("Invalid parameter. (func=%d; module=%u)\n", (func != NULL), module);
        return -EINVAL;
    }
    mutex_lock(&g_power_state_mutex);
    g_power_state_handle[module].module = module;
    g_power_state_handle[module].func = func;
    mutex_unlock(&g_power_state_mutex);
    dms_info("Register power handle ok. (module=%u)\n", module);
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_manager_register_power_handle);

int devdrv_manager_unregister_power_handle(unsigned int module)
{
    if (module >= POWER_STATE_MODULE_MAX) {
        dms_err("Invalid input argument. (module=%u)\n", module);
        return -EINVAL;
    }
    mutex_lock(&g_power_state_mutex);
    g_power_state_handle[module].func = NULL;
    mutex_unlock(&g_power_state_mutex);
    dms_info("Unregister power handle ok. (module=%u)\n", module);
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_manager_unregister_power_handle);

STATIC int dms_power_state_exec(struct drvdev_power_state_info *info)
{
    int ret;
    unsigned int i;
    mutex_lock(&g_power_state_mutex);
    for (i = 0; i < POWER_STATE_MODULE_MAX; i++) {
        if (g_power_state_handle[i].func == NULL) {
            continue;
        }
        ret = (g_power_state_handle[i].func)(info);
        if ((ret != 0) && (ret != -EOPNOTSUPP)) {
            dms_err("power state handle exec fail. (i=%u; ret=%d)\n", i, ret);
            mutex_unlock(&g_power_state_mutex);
            return ret;
        }
        if (ret == 0) {
            dms_info("power state handle exec ok. (i=%u)\n", i);
        }
    }
    mutex_unlock(&g_power_state_mutex);
    return 0;
}

#ifdef CFG_SOC_PLATFORM_MDC_V51
#define SLEEP_POWER_DELAY_MS 2U
#define DEVDRV_SEND_IPC_MBOX_ID HISI_RPROC_SI_Q_TX_RPID15
#define SILS_IPC_MSGTYPE_HAERTBEAT 8
#define SILS_IPC_CMDTYPE_OFF 6
#define SILS_IPC_DATA 0xff
#endif

#ifdef CFG_FEATURE_POWER_STATE
#ifdef CFG_SOC_PLATFORM_MDC_V51
STATIC int dms_set_bist_pre_check(struct drvdev_power_state_info *info)
{
    int ret, chip_type;
    if (info->power_info.type != POWER_STATE_BIST) {
        return 0;
    }
    ret = devdrv_manager_get_chip_type(&chip_type);
    if (ret != 0) {
        dms_err("get chip type failed, (ret=%d)\n", ret);
        return ret;
    }
    if ((chip_type != CHIP_TYPE_ASCEND_V2) && (chip_type != CHIP_TYPE_ASCEND_V51_LITE)) {
        return -EOPNOTSUPP;
    }
    return 0;
}
#endif
STATIC int dms_set_power_state_pre_check(struct drvdev_power_state_info *info, u32 in_len)
{
    int ret = 0;
    if ((info == NULL) || (in_len != sizeof(struct drvdev_power_state_info))) {
        dms_err("Invalid parameter. (in_buff=%s; in_len=%u)\n", (info == NULL) ? "NULL" : "OK", in_len);
        return -EINVAL;
    }

    if (info->dev_id >= DEVDRV_MANGER_MAX_DEVICE_NUM) {
        dms_err("Invalid device id. (dev_id=%d)\n", info->dev_id);
        return -EINVAL;
    }
    if (info->power_info.type >= POWER_STATE_MAX) {
        dms_err("Invalid type. (type=%u)\n", info->power_info.type);
        return -EINVAL;
    }
#if !defined(CFG_SOC_PLATFORM_MDC_V51) && !defined(CFG_SOC_PLATFORM_MDC_V11)
    if (info->power_info.type != POWER_STATE_SUSPEND) {
        dms_err("Type is invalid. (device_id=%d; type=%u)\n", info->dev_id, info->power_info.type);
        return -EOPNOTSUPP;
    }
#endif
#ifdef CFG_SOC_PLATFORM_MDC_V51
    ret = dms_set_bist_pre_check(info);
#endif
    return ret;
}
#endif

#ifdef CFG_SOC_PLATFORM_MDC_V51
typedef int (*dms_hsm_notify_func)(uint32_t dev_id);
#define DMS_OPS_SET_HSM_RESET "hsm_notify_reset_action"
STATIC int dms_set_hsm_reset(uint32_t dev_id)
{
    int ret;
    dms_hsm_notify_func hsm_notify_handle = NULL;
    hsm_notify_handle = (dms_hsm_notify_func)(uintptr_t)__symbol_get(DMS_OPS_SET_HSM_RESET);
    if (hsm_notify_handle == NULL) {
        dms_err("Lookup symbol failed, (sym=%s).\n", DMS_OPS_SET_HSM_RESET);
        return -EINVAL;
    }

    ret = hsm_notify_handle(dev_id);
    __ka_system_symbol_put(DMS_OPS_SET_HSM_RESET);
    if (ret != 0) {
        dms_err("send power msg to hiss failed, (ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    dms_info("send power msg to hiss success.\n");
    return 0;
}
#endif

#ifdef CFG_FEATURE_POWER_STATE
s32 dms_set_power_state(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct drvdev_power_state_info *info = NULL;
    unsigned int type;
    int ret;

    info = (struct drvdev_power_state_info *)in;
    ret = dms_set_power_state_pre_check(info, in_len);
    if (ret != 0) {
        return ret;
    }

    type = info->power_info.type;
    dms_info("set power state, (type=%u)\n", type);
    ret = dms_power_state_exec(info);
    if (ret != 0) {
        return ret;
    }
    if ((type == POWER_STATE_POWEROFF) || (type == POWER_STATE_RESET)) {
#ifdef CFG_FEATURE_REBOOT_REASON
        devdrv_manager_record_reset_reason(RST_REASON_CUR_L3SRAM_OFFSET1, RST_REASON_BIT_DSMI_CMD);
#endif
#ifdef CFG_SOC_PLATFORM_MDC_V51
        ret = dms_set_hsm_reset(info->dev_id);
        if (ret != 0) {
            return ret;
        }
#endif
    }
    return 0;
}
#endif

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-3-2
 */
#ifdef CFG_FEATURE_HOTRESET
#include "devdrv_common.h"
#include "tsdrv_common.h"
#include "tsdrv_log.h"
#include "dms_notifier.h"
#include "tsdrv_hotreset.h"
#include "tsdrv_dms_notifier.h"

static int (*const dms_notifier_handle_func[DMS_DEVICE_NOTIFIER_MAX])(u32 devid) = {
    [DMS_DEVICE_PRE_HOTRESET] = tsdrv_device_pre_hotreset,
    [DMS_DEVICE_HOTRESET_CANCEL] = tsdrv_device_cancel_hotreset,
};

#ifndef TSDRV_UT
static int tsdrv_dms_notifier_handle(ka_notifier_block_t *self,
    unsigned long event, void *data)
{
    struct devdrv_info *dev_info = (struct devdrv_info *)data;
    int ret;

    if ((dev_info == NULL) ||
        (dev_info->dev_id >= TSDRV_MAX_DAVINCI_NUM) ||
        (event >= DMS_DEVICE_NOTIFIER_MAX)) {
        TSDRV_PRINT_ERR("Invalid parameter. (event=0x%lx)\n", event);
        return NOTIFY_BAD;
    }

    if (dms_notifier_handle_func[event] == NULL) {
        TSDRV_PRINT_WARN("Event handle not support. (event=0x%lx)\n", event);
        return KA_NOTIFY_DONE;
    }

    ret = dms_notifier_handle_func[event](dev_info->dev_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Notifier handle failed. (devid=%u; event=0x%lx)\n",
            dev_info->dev_id, event);
        return NOTIFY_BAD;
    }

    TSDRV_PRINT_INFO("Notifier handle success. (devid=%u; event=0x%lx)\n",
        dev_info->dev_id, event);
    return KA_NOTIFY_DONE;
}

static ka_notifier_block_t g_tsdrv_dms_notifier = {
    .notifier_call = tsdrv_dms_notifier_handle,
};
#endif
int tsdrv_dms_register_notifier(void)
{
    return dms_register_notifier(&g_tsdrv_dms_notifier);
}

void tsdrv_dms_unregister_notifier(void)
{
    (void)dms_unregister_notifier(&g_tsdrv_dms_notifier);
}
#endif

#ifdef TSDRV_UT
void tsdrv_dms_notifier_handle_test(void)
{
}
#endif

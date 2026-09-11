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
#ifndef __TSDRV_DMS_NOTIFIER_H
#define __TSDRV_DMS_NOTIFIER_H

#ifdef CFG_FEATURE_HOTRESET
int tsdrv_dms_register_notifier(void);
void tsdrv_dms_unregister_notifier(void);
#else
static inline int tsdrv_dms_register_notifier(void)
{
    return 0;
}

static inline void tsdrv_dms_unregister_notifier(void)
{
}
#endif

#endif
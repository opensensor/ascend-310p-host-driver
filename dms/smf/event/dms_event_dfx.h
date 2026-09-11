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

#ifndef __DMS_EVENT_DFX_H__
#define __DMS_EVENT_DFX_H__

#define EVENT_DFX_BUF_SIZE_MAX (1024 * 4)
#define EVENT_DFX_CHECK_DO_SOMETHING(condition, something) \
do {                                                       \
    if (condition) {                                       \
        something;                                         \
    }                                                      \
} while (0)

extern int sscanf_s(const char *buffer, const char *format, ...);

ssize_t dms_event_print_convergent_diagrams(u32 devid, char opt, char *str);
ssize_t dms_event_print_event_list(u32 devid, char *str);
ssize_t dms_event_print_mask_list(u32 devid, char *str);
ssize_t dms_event_print_subscribe_handle(char *str);
ssize_t dms_event_print_subscribe_process(char *str);

ssize_t dms_event_dfx_channel_flux_store(const char *buf, size_t count);
ssize_t dms_event_dfx_channel_flux_show(char *str);
ssize_t dms_event_dfx_convergent_diagrams_store(const char *buf, size_t count);
ssize_t dms_event_dfx_convergent_diagrams_show(char *str);
ssize_t dms_event_dfx_event_list_store(const char *buf, size_t count);
ssize_t dms_event_dfx_event_list_show(char *str);
ssize_t dms_event_dfx_mask_list_store(const char *buf, size_t count);
ssize_t dms_event_dfx_mask_list_show(char *str);
ssize_t dms_event_dfx_subscribe_handle_show(char *str);
ssize_t dms_event_dfx_subscribe_process_show(char *str);

#endif

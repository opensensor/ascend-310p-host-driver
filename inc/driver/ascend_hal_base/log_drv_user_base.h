/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */
#ifndef LOG_DRV_USER_BASE_H
#define LOG_DRV_USER_BASE_H

#include "ascend_hal_define.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_DEVICE_ID_MAX_BASE (64)
#define LOG_CHANNEL_NUM_MAX_BASE (64)

struct log_ioctl_para {
    int32_t device_id;
    uint32_t channel_type;
    const int8_t *slog_buf;
    int8_t *dest_buf;
    uint32_t buf_size;
    int32_t write_size;
    int32_t timeout;
    int32_t log_level;
    int32_t compress;
    int32_t byte_per_sec;
    int32_t device_id_set[LOG_DEVICE_ID_MAX_BASE];
    int32_t device_id_num;
    int32_t channel_type_set[LOG_CHANNEL_NUM_MAX_BASE];
    int32_t channel_type_num;
    uint32_t tsnum;
    uint32_t slog_type;
    int32_t ret; /* return value of command */
    uint64_t reserved[4];
};

int32_t log_ioctl_set_level(struct log_ioctl_para *arg);
int32_t log_ioctl_set_dfx_param(struct log_ioctl_para *arg);
int32_t log_ioctl_get_dfx_param(struct log_ioctl_para *arg);
int32_t log_ioctl_get_channel_type(struct log_ioctl_para *arg);
int32_t log_ioctl_get_device_id(struct log_ioctl_para *arg);
int32_t log_ioctl_get_data(struct log_ioctl_para *arg);
int32_t log_ioctl_get_data_by_type(struct log_ioctl_para *arg);
int32_t log_ioctl_alloc_mem_by_type(struct log_ioctl_para *arg);

#ifdef __cplusplus
}
#endif
#endif

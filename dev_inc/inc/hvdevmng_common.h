/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2023. All rights reserved.
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
 * Create: 2020-10-15
 */

#ifndef __HVDEVMNG_COMMON_H__
#define __HVDEVMNG_COMMON_H__

#include "devdrv_manager_common.h"

#define VDEVMNG_MSG_DATE_MAXLEN 1024

enum VDEVMNG_CTRL_MSG_TYPE {
    VDEVMNG_CTRL_MSG_TYPE_OPEN = 1,
    VDEVMNG_CTRL_MSG_TYPE_RELEASE,
    VDEVMNG_CTRL_MSG_TYPE_GET_DEVINFO,
    VDEVMNG_CTRL_MSG_TYPE_GET_PDATA,
    VDEVMNG_CTRL_MSG_TYPE_MAX,
};

struct vdevdrv_info_msg {
    u8 plat_type;
    u8 status;
    u32 env_type;
    u32 board_id;
    u32 slot_id;
    u32 dev_id;
    u32 pci_dev_id;
    u32 capability;
    u32 chip_name;
    u32 chip_version;

    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u32 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;

    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_core_id;
    u32 ai_core_id;
    u32 aicpu_occupy_bitmap;
    u32 ai_subsys_ip_broken_map;
    u32 hardware_version;
    u64 aicore_bitmap;

    u32 inuse_ai_core_num;
    u32 inuse_ai_core_error_bitmap;
    u32 inuse_ai_cpu_num;
    u32 inuse_ai_cpu_error_bitmap;

    u32 ts_num;
    u32 firmware_hardware_version;

    u64 aicore_freq;
    u64 cpu_system_count;
    u64 monotonic_raw_time_ns;

    u32 vector_core_num;
    u64 vector_core_bitmap;
    u64 vector_core_freq;

    #define TEMPLATE_NAME_LEN 32
    u8 template_name[TEMPLATE_NAME_LEN];
};

struct vdevdrv_info_pdata {
    u32 dev_id;
    u32 env_type;
    u32 ai_core_num;
    u32 ai_core_freq;
    u64 ai_core_bitmap;
    u32 ts_num;

    u8 ai_core_num_level;
    u8 ai_core_freq_level;

    u32 tsid;
    u32 ts_cpu_core_num;
};

union vdevmng_ctrl {
    struct vdevdrv_info_msg ready_info;
    struct vdevdrv_info_pdata ready_pdata;
    char recv[VDEVMNG_MSG_DATE_MAXLEN];
};

struct vdevmng_ctrl_msg {
    enum VDEVMNG_CTRL_MSG_TYPE type;
    int error_code;
    union vdevmng_ctrl ctrl_data;
};

struct vdevmng_u32_para {
    unsigned int para_in;
    unsigned int para_out;
};

struct vdemvng_H2D_devinfo {
    u64 cpu_system_count;
    u64 monotonic_raw_time_ns;
    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];
};

struct vdevmng_osc_freq {
    u32 devid;
    u32 sub_cmd;
    u64 value;
};

struct vdevmng_current_aic_freq {
    u32 devid;
    u32 freq;
};

struct vdevmng_log_info {
    void *buf;
    u32 length;  // size of buf
    u32 buf_len;  // actual len of host log ringbuffer
};

union vdevmng_cmd {
    struct vdevmng_u32_para u32_para;
    struct devdrv_get_device_boot_status_para boot_status;
    struct vdemvng_H2D_devinfo H2D_devinfo;
    struct devdrv_device_work_status work_status;
    struct devdrv_device_health_status health_status;
    struct devdrv_resource_info resource_info;
    struct devdrv_error_code_para error_code_para;
    struct vdevmng_osc_freq osc_freq;
    struct vdevmng_current_aic_freq current_aic_freq;
    char resv[VDEVMNG_MSG_DATE_MAXLEN];
};

#define FILTER_MAX_LEN 128
#define DMS_MAIN_CMD_BASIC 'E'

struct vdevmng_ioctl_msg {
    unsigned int main_cmd;
    unsigned int sub_cmd;
    int result;
    char filter[FILTER_MAX_LEN];
    unsigned int filter_len;
    unsigned int input_len;
    unsigned int output_len;
    union vdevmng_cmd cmd_data;
};

int hvdevmng_get_dev_resource(u32 devid, u32 tsid, struct devdrv_manager_msg_resource_info *resource_info);

#endif /* __HVDEVMNG_COMMON_H__ */

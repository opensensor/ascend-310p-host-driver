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
* Create: 2024-06-03
*/

#ifndef __DMS_FLASH_H
#define __DMS_FLASH_H

#define DMS_MODULE_FLASH "dms_module_flash"
#define FLASH_ERASE_COUNT_LEN   (4 * 1024 * sizeof(uint16_t))
#define DSMI_OUT_LEN_EACH_READ  (2 * 1024)
#define DSMI_READ_TIMES   4

int dms_flash_init(void);
void dms_flash_uninit(void);
extern int32_t sec_read_and_clear_erase_count(uint32_t dev_id, uint16_t *buf, uint32_t buf_size_byte);

#endif
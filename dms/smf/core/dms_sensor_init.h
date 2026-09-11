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


#ifndef DMS_SENSOR_INIT_H
#define DMS_SENSOR_INIT_H

int dms_smf_suspend(unsigned int devid);
int dms_smf_resume(unsigned int devid);
void dms_sensor_init(void);
unsigned int dms_sensor_exit(void);

#endif

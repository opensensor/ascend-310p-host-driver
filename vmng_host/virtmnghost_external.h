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

#ifndef VIRTMNGHOST_EXTERNAL_H
#define VIRTMNGHOST_EXTERNAL_H

#include "virtmnghost_unit.h"

int vmngh_alloc_external_db_entries(struct vmngh_vd_dev *vd_dev);
void vmngh_free_external_db_entries(struct vmngh_vd_dev *vd_dev);
int vmngh_external_db_handler(struct vmngh_vd_dev *vd_dev, int db_index);

#endif

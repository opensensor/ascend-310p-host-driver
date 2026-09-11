/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _DVT_SYSFS_H_
#define _DVT_SYSFS_H_

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
unsigned int available_instances_show(struct mdev_type *mtype);
ssize_t description_show(struct mdev_type *mtype, char *buf);
#else
struct attribute **get_hw_vdavinci_type_attrs(void);
#endif /* KERNEL_VERSION(6,1,0) */

const struct attribute_group **get_hw_vdavinci_groups(void);
unsigned int available_instances_ops(struct device *dev, const char *name);
ssize_t description_ops(struct device *dev, const char *name, char *buf);
ssize_t device_api_ops(char *buf);
ssize_t vfg_id_store_ops(struct device *dev, const char *name,
                         const char *buf, size_t count);
#endif

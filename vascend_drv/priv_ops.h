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
 
#ifndef _PRIV_OPS_H_
#define _PRIV_OPS_H_
 
#include "dvt.h"
 
int vdavinci_priv_vdev_create(struct hw_vdavinci *vdavinci,
                              struct vdavinci_type *tp, uuid_le uuid);
void vdavinci_priv_vdev_destroy(struct hw_vdavinci *vdavinci);
void vdavinci_priv_vdev_release(struct hw_vdavinci *vdavinci);
int vdavinci_priv_vdev_reset(struct hw_vdavinci *vdavinci);
int vdavinci_priv_vdev_flr(struct hw_vdavinci *vdavinci);
void vdavinci_priv_vdev_notify(struct hw_vdavinci *vdavinci, int db_index);
int vdavinci_priv_vdev_getmapinfo(struct hw_vdavinci *vdavinci,
                                  struct vdavinci_type *tp, u32 bar_id,
                                  struct vdavinci_mapinfo *map_info);
int vdavinci_priv_vdev_putmapinfo(struct hw_vdavinci *vdavinci);
unsigned int vdavinci_priv_vdev_getdevnum(struct vdavinci_priv *priv, struct device *dev);
int vdavinci_priv_vdev_getdevinfo(struct vdavinci_priv *priv,
                                  struct device *dev, u32 dev_index,
                                  struct dvt_devinfo *dev_info);
int vdavinci_priv_vdev_enable_sriov(struct vdavinci_priv *priv,
                                    struct pci_dev *pdev, int numvfs);
 
#endif /* _PRIV_OPS_H_ */

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
 
#include "hw_vdavinci.h"
#include "priv_ops.h"
 
static bool vdavinci_priv_check(struct vdavinci_priv *priv)
{
    if (priv == NULL || priv->ops == NULL) {
        return false;
    }
 
    return true;
}
 
int vdavinci_priv_vdev_create(struct hw_vdavinci *vdavinci,
                              struct vdavinci_type *tp, uuid_le uuid)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return -EINVAL;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_create == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's create ops is null\n");
        return -EINVAL;
    }
 
    return vdavinci->dvt->vdavinci_priv->ops->vdavinci_create(&vdavinci->dev, vdavinci,
                                                              tp, uuid);
}
 
void vdavinci_priv_vdev_destroy(struct hw_vdavinci *vdavinci)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_destroy == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's destroy ops is null\n");
        return;
    }
 
    vdavinci->dvt->vdavinci_priv->ops->vdavinci_destroy(&vdavinci->dev);
}
 
void vdavinci_priv_vdev_release(struct hw_vdavinci *vdavinci)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_release == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's release ops is null\n");
        return;
    }
 
    vdavinci->dvt->vdavinci_priv->ops->vdavinci_release(&vdavinci->dev);
}
 
int vdavinci_priv_vdev_reset(struct hw_vdavinci *vdavinci)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return -EINVAL;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_reset == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's reset ops is null\n");
        return -EINVAL;
    }
 
    return vdavinci->dvt->vdavinci_priv->ops->vdavinci_reset(&vdavinci->dev);
}
 
int vdavinci_priv_vdev_flr(struct hw_vdavinci *vdavinci)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return -EINVAL;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_flr == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's flr ops is null\n");
        return -EINVAL;
    }
 
    return vdavinci->dvt->vdavinci_priv->ops->vdavinci_flr(&vdavinci->dev);
}
 
void vdavinci_priv_vdev_notify(struct hw_vdavinci *vdavinci, int db_index)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_notify == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's notify ops is null\n");
        return;
    }
 
    vdavinci->dvt->vdavinci_priv->ops->vdavinci_notify(&vdavinci->dev, db_index);
}
 
int vdavinci_priv_vdev_getmapinfo(struct hw_vdavinci *vdavinci,
                                  struct vdavinci_type *tp, u32 bar_id,
                                  struct vdavinci_mapinfo *map_info)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return -EINVAL;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_getmapinfo == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's getmapinfo ops is null\n");
        return -EINVAL;
    }
 
    return vdavinci->dvt->vdavinci_priv->ops->vdavinci_getmapinfo(&vdavinci->dev, tp,
                                                                  bar_id, map_info);
}
 
int vdavinci_priv_vdev_putmapinfo(struct hw_vdavinci *vdavinci)
{
    if (!vdavinci_priv_check(vdavinci->dvt->vdavinci_priv)) {
        return -EINVAL;
    }
    if (vdavinci->dvt->vdavinci_priv->ops->vdavinci_putmapinfo == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci_priv's putmapinfo ops is null\n");
        return -EINVAL;
    }
 
    return vdavinci->dvt->vdavinci_priv->ops->vdavinci_putmapinfo(&vdavinci->dev);
}
 
unsigned int vdavinci_priv_vdev_getdevnum(struct vdavinci_priv *priv, struct device *dev)
{
    if (!vdavinci_priv_check(priv)) {
        return 0;
    }
    if (priv->ops->davinci_getdevnum == NULL) {
        vascend_err(dev, "priv's getdevnum ops is null\n");
        return 0;
    }
 
    return (unsigned int)priv->ops->davinci_getdevnum(dev);
}
 
int vdavinci_priv_vdev_getdevinfo(struct vdavinci_priv *priv,
                                  struct device *dev, u32 dev_index,
                                  struct dvt_devinfo *dev_info)
{
    if (!vdavinci_priv_check(priv)) {
        return -EINVAL;
    }
    if (priv->ops->davinci_getdevinfo == NULL) {
        vascend_err(dev, "vdavinci_priv's getdevinfo ops is null\n");
        return -EINVAL;
    }
 
    return priv->ops->davinci_getdevinfo(dev, dev_index, dev_info);
}
 
int vdavinci_priv_vdev_enable_sriov(struct vdavinci_priv *priv,
                                    struct pci_dev *pdev, int numvfs)
{
    if (!vdavinci_priv_check(priv)) {
        return -EINVAL;
    }
    if (priv->ops->vascend_enable_sriov == NULL) {
        vascend_err(&pdev->dev, "vdavinci_priv's vascend_enable_sriov ops is null\n");
        return -EINVAL;
    }
 
    return priv->ops->vascend_enable_sriov(pdev, numvfs);
}

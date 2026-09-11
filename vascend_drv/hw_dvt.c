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

#include "dvt.h"

int hw_dvt_init(void *vdavinci_priv)
{
    int ret;

    if (!hw_vdavinci_priv_callback_check(vdavinci_priv)) {
        return -EINVAL;
    }
    if (!IS_ENABLED(CONFIG_VFIO_MDEV)) {
        vascend_warn(((struct vdavinci_priv *)vdavinci_priv)->dev,
                     "mdev is not enable");
        return 0;
    }
    ret = hw_dvt_init_device((struct vdavinci_priv *)vdavinci_priv);
    if (ret == -ENOTSUPP) {
        vascend_warn(((struct vdavinci_priv *)vdavinci_priv)->dev,
                     "vdavinci is not support");
        return 0;
    }
    if (ret) {
        vascend_err(((struct vdavinci_priv *)vdavinci_priv)->dev,
                    "Fail to init DVT device, ret: %d\n", ret);
        return ret;
    }

    return 0;
}

int hw_dvt_uninit(void *vdavinci_priv)
{
    int ret;

    if (!vdavinci_priv) {
        return -EINVAL;
    }
    ret = hw_dvt_uninit_device((struct vdavinci_priv *)vdavinci_priv);
    if (ret) {
        vascend_err(((struct vdavinci_priv *)vdavinci_priv)->dev,
                    "Fail to uninit DVT device, ret: %d\n", ret);
        return ret;
    }

    return 0;
}

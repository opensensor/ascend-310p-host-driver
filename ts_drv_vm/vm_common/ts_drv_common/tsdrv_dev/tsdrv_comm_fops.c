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
#include "devdrv_devinit.h"
#include "tsdrv_device.h"

#ifndef CFG_SOC_PLATFORM_MDC_V51
#  include "hvtsdrv_proc.h"
#endif

#include "davinci_api.h"
#include "devdrv_manager.h"

#ifdef AOS_LLVM_BUILD
int tsdrv_get_vdavinci_info(u32 vdev_id, u32 *phy_id, u32 *vfid)
{
    if ((phy_id == NULL) || (vfid == NULL))
        return -EINVAL;

    *phy_id = vdev_id;
    *vfid = 0;
    return 0;
}
#endif

STATIC int tsdrv_comm_open(ka_inode_t *inode, ka_file_t *filep)
{
    u32 vdevid = drv_davinci_get_device_id(filep);
    enum tsdrv_host_flag host_flag;
    u32 devid = 0;
    u32 fid = 0;
    int ret;
#ifndef AOS_LLVM_BUILD
    ret = devmng_get_vdavinci_info(vdevid, &devid, &fid);
#else
    ret = tsdrv_get_vdavinci_info(vdevid, &devid, &fid);
#endif
    if ((ret != 0) || (devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("invalid para, vdevid(%u), devid(%u), fid(%u)\n", vdevid, devid, fid);
        return -EFAULT;
    }

    host_flag = tsdrv_get_host_flag(devid);
    switch (host_flag) {
        case TSDRV_PHYSICAL_TYPE:
            return devdrv_open(inode, filep);
#ifndef CFG_SOC_PLATFORM_MDC_V51
        case TSDRV_CONTAINER_TYPE:
            return hvtsdrv_open(inode, filep);
#endif
        default:
            TSDRV_PRINT_ERR("invalid host_flag(%d).\n", (int)host_flag);
            return -EINVAL;
    }
}

STATIC int tsdrv_comm_release(ka_inode_t *inode, ka_file_t *filep)
{
    u32 vdevid = drv_davinci_get_device_id(filep);
    enum tsdrv_host_flag host_flag;
    u32 devid = 0;
    u32 fid = 0;
    int ret;

#ifndef AOS_LLVM_BUILD
    ret = devmng_get_vdavinci_info(vdevid, &devid, &fid);
#else
    ret = tsdrv_get_vdavinci_info(vdevid, &devid, &fid);
#endif
    if ((ret != 0) || (devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("invalid para, vdevid(%u), devid(%u), fid(%u)\n", vdevid, devid, fid);
        return -EFAULT;
    }

    host_flag = tsdrv_get_host_flag(devid);
    switch (host_flag) {
        case TSDRV_PHYSICAL_TYPE:
            return devdrv_release(inode, filep);
#ifndef CFG_SOC_PLATFORM_MDC_V51
        case TSDRV_CONTAINER_TYPE:
            return hvtsdrv_release(inode, filep);
#endif
        default:
            TSDRV_PRINT_ERR("invalid host_flag(%d).\n", (int)host_flag);
            return -EINVAL;
    }
}
#ifndef AOS_LLVM_BUILD
STATIC int tsdrv_comm_mmap(ka_file_t *filep, struct vm_area_struct *vma)
{
    u32 vdevid = drv_davinci_get_device_id(filep);
    enum tsdrv_host_flag host_flag;
    u32 devid = 0;
    u32 fid = 0;
    int ret;

    ret = devmng_get_vdavinci_info(vdevid, &devid, &fid);
    if ((ret != 0) || (devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("invalid para, vdevid(%u), devid(%u), fid(%u)\n", vdevid, devid, fid);
        return -EFAULT;
    }

    host_flag = tsdrv_get_host_flag(devid);
    switch (host_flag) {
        case TSDRV_PHYSICAL_TYPE:
            return devdrv_mmap(filep, vma);
#ifndef CFG_SOC_PLATFORM_MDC_V51
        case TSDRV_CONTAINER_TYPE:
            return hvtsdrv_mmap(filep, vma);
#endif
        default:
            TSDRV_PRINT_ERR("invalid host_flag(%d).\n", (int)host_flag);
            return -EINVAL;
    }
}
#endif
STATIC long tsdrv_comm_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    u32 vdevid = drv_davinci_get_device_id(filep);
    enum tsdrv_host_flag host_flag;
    u32 devid = 0;
    u32 fid = 0;
    int ret;

#ifndef AOS_LLVM_BUILD
    ret = devmng_get_vdavinci_info(vdevid, &devid, &fid);
#else
    ret = tsdrv_get_vdavinci_info(vdevid, &devid, &fid);
#endif
    if ((ret != 0) || (devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("invalid para, vdevid(%u), devid(%u), fid(%u)\n", vdevid, devid, fid);
        return -EFAULT;
    }

    host_flag = tsdrv_get_host_flag(devid);
    switch (host_flag) {
        case TSDRV_PHYSICAL_TYPE:
            return devdrv_ioctl(filep, cmd, arg);
#ifndef CFG_SOC_PLATFORM_MDC_V51
        case TSDRV_CONTAINER_TYPE:
            return hvtsdrv_ioctl(filep, cmd, arg);
#endif
        default:
            TSDRV_PRINT_ERR("invalid host_flag(%d).\n", (int)host_flag);
            return -EINVAL;
    }
}

ka_file_operations_t tsdrv_comm_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(tsdrv_comm_open)
    ka_fs_init_f_release(tsdrv_comm_release)
    ka_fs_init_f_unlocked_ioctl(tsdrv_comm_ioctl)
#ifndef AOS_LLVM_BUILD
    ka_fs_init_f_mmap(tsdrv_comm_mmap)
#endif
};

ka_file_operations_t *tsdrv_get_comm_fops(void)
{
    return &tsdrv_comm_fops;
}


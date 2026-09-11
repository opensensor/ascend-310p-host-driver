/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description:ascend manager driver
 * Author: huawei
 * Create: 2021-06-30
 */
#include <devdrv_interface.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include "dbl/uda.h"
#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_dev_share.h"

#define DEVDRV_GET_DEVICE_SHARE 0
#define DEVDRV_SET_DEVICE_SHARE 1

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

struct device_share_para {
    unsigned int dev_id;
    unsigned int opcode;
    int dev_share_flag;
};

STATIC int g_device_share_flag[DEVDRV_MAX_DAVINCI_NUM];

#ifdef CFG_FEATURE_DEVICE_SHARE
int set_device_share_flag(unsigned int device_id, unsigned value)
#else
void set_device_share_flag(unsigned int device_id, unsigned value)
#endif
{
#ifdef CFG_FEATURE_DEVICE_SHARE
    u32 share_device_id;
    u8 share_flag;
    int ret = 0;

    share_device_id = device_id;
    share_flag = value;
    ret = uda_set_dev_share(share_device_id, share_flag);
    if (ret != 0) {
        devdrv_drv_err("uda set dev share failed. (ret=%d)\n", ret);
        return ret;
    }
    g_device_share_flag[device_id] = value;
    return 0;
#else
    g_device_share_flag[device_id] = value;
#endif
}

int get_device_share_flag(unsigned int device_id)
{
    return g_device_share_flag[device_id];
}

STATIC int devdrv_proc_dev_share_opcode(unsigned long arg, struct device_share_para *dev_share_arg)
{
    int ret = 0;
    int dev_share_flag;
    unsigned int device_id;

    device_id = dev_share_arg->dev_id;
    if (device_id >= DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_err("Invalid device id. (device_id=%u)\n", device_id);
        return -ENODEV;
    }

    switch (dev_share_arg->opcode) {
        case DEVDRV_GET_DEVICE_SHARE:
            dev_share_arg->dev_share_flag = get_device_share_flag(device_id);
            ret = copy_to_user_safe((void *)(uintptr_t)arg, dev_share_arg, sizeof(struct device_share_para));
            if (ret) {
                devdrv_drv_err("Copy to user failed. (ret=%d)\n", ret);
                return ret;
            }
            break;
        case DEVDRV_SET_DEVICE_SHARE:
            dev_share_flag = dev_share_arg->dev_share_flag;
            if ((dev_share_flag == DEVICE_SHARE) || (dev_share_flag == DEVICE_UNSHARE)) {
            #ifdef CFG_FEATURE_DEVICE_SHARE
                ret = set_device_share_flag(device_id, dev_share_flag);
            #else
                set_device_share_flag(device_id, dev_share_flag);
            #endif
            } else {
                devdrv_drv_err("Device share flag is invalid. (flag=%d)\n", dev_share_flag);
                return -EFAULT;
            }
            break;
        default:
            devdrv_drv_err("Ioctl opcode is invalid. (opcode=%u)\n", dev_share_arg->opcode);
            return -EINVAL;
    }

    return ret;
}

#ifndef CFG_FEATURE_DEVICE_SHARE
STATIC int check_device_share_by_board_type(void)
{
    unsigned int i;
    int board_type;
    const int device_share_white_list[] = {
        HOST_TYPE_ARM_3559,
        HOST_TYPE_ARM_3519
    };
    unsigned int array_size = sizeof(device_share_white_list) / sizeof(int);

    board_type = devdrv_manager_get_product_type();
    for (i = 0; i < array_size; i++) {
        if (board_type == device_share_white_list[i]) {
            return 0;
        }
    }
    devdrv_drv_err("Board not support device share. (board_type=%d)\n", board_type);
 
    return -EOPNOTSUPP;
}
#endif

STATIC int devdrv_manager_support_device_share(void)
{
#ifdef CFG_FEATURE_DEVICE_SHARE
    return 0;
#else
    return check_device_share_by_board_type();
#endif
}

int devdrv_manager_config_device_share(struct file *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    u32 info = DEVDRV_MANAGER_DEVICE_ENV;
    struct device_share_para dev_share_arg = {0};

    ret = devdrv_get_platformInfo(&info);
    if (ret) {
        devdrv_drv_err("Get platform info failed. (ret=%d)\n", ret);
        return ret;
    }
    if (info == DEVDRV_MANAGER_HOST_ENV) {
        ret = devdrv_manager_support_device_share();
        if (ret) {
            return ret;
        }
    }

    ret = copy_from_user_safe(&dev_share_arg, (void *)(uintptr_t)arg, sizeof(struct device_share_para));
    if (ret) {
        devdrv_drv_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_proc_dev_share_opcode(arg, &dev_share_arg);
    if (ret) {
        devdrv_drv_err("Proc device share opcode failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

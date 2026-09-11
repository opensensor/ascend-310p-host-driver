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
#ifndef QUEUE_UT

#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"

#include "queue_module.h"
#include "queue_fops.h"

#ifdef AOS_LLVM_BUILD
#include "aos_dlib.h"
#endif

STATIC int queue_open(ka_inode_t *inode, ka_file_t *file)
{
    return queue_drv_open(inode, file);
}

STATIC int queue_release(ka_inode_t *inode, ka_file_t *file)
{
    return queue_drv_release(inode, file);
}

STATIC long queue_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    if (filep->private_data == NULL) {
        queue_err("The filep private_data is NULL.\n");
        return -EINVAL;
    }

    if (_KA_IOC_TYPE(cmd) != QUEUE_IOC_MAGIC || (unsigned int)_KA_IOC_NR(cmd) >= (unsigned int)QUEUE_CMD_MAX) {
        queue_err("Cmd is invalid. (_IOC_TYPE=%u, _IOC_NR=%u)\n", _KA_IOC_TYPE(cmd), _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    if (arg == 0) {
        queue_err("Arg is null. (_IOC_NR=%u)\n", _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    if (drv_queue_ioctl_handlers[_KA_IOC_NR(cmd)] == NULL) {
        queue_err("invalid cmd, cmd = %u\n", _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    return drv_queue_ioctl_handlers[_KA_IOC_NR(cmd)](filep, cmd, arg);
}

static const ka_file_operations_t g_queue_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(queue_open)
    ka_fs_init_f_unlocked_ioctl(queue_ioctl)
    ka_fs_init_f_release(queue_release)
};

STATIC int __init queue_module_init(void)
{
    int ret;

    ret = queue_drv_module_init(&g_queue_fops);
    if (ret != 0) {
        queue_err("module init failed, ret=%d.\n", ret);
        return ret;
    }

    return ret;
}

STATIC void __exit queue_module_exit(void)
{
    queue_drv_module_exit();
}

ka_module_init(queue_module_init);
ka_module_exit(queue_module_exit);

KA_MODULE_LICENSE("GPL");
#else
void queue_module_ut(void)
{
    return;
}
#endif

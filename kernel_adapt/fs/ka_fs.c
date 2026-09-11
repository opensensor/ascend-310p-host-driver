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
 * Create: 2024-11-11
 */

#include <linux/fs.h>
#include "securec.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"

ssize_t ka_fs_kernel_read(ka_file_t *file, void *buf, size_t count, loff_t *pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    return kernel_read(file, buf, count, pos);
#else
    return kernel_read(file, *pos, (char*)buf, count);
#endif
}
EXPORT_SYMBOL_GPL(ka_fs_kernel_read);

ssize_t ka_fs_kernel_write(ka_file_t *file, const void *buf, size_t count, loff_t *pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    return kernel_write(file, buf, count, pos);
#else
    return kernel_write(file, (char*)buf, count, *pos);
#endif
}
EXPORT_SYMBOL_GPL(ka_fs_kernel_write);

int ka_fs_vfs_getattr(ka_path_t *path, ka_kstat_t *stat, u32 request_mask, unsigned int query_flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    return vfs_getattr(path, stat, request_mask, query_flags);
#else
    return vfs_getattr(path, stat);
#endif
}
EXPORT_SYMBOL_GPL(ka_fs_vfs_getattr);

ssize_t ka_fs_read_file(ka_file_t *file, loff_t *pos, char *addr, size_t count)
{
    ssize_t len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    len = kernel_read(file, addr, count, pos);
#else
    char __user *buf = (char __user *)addr;
    mm_segment_t old_fs;

    old_fs = get_fs();
    set_fs(get_ds()); /* lint !e501 */ /* kernel source */
    len = vfs_read(file, buf, count, pos);
    set_fs(old_fs);
#endif

    return len;
}
EXPORT_SYMBOL_GPL(ka_fs_read_file);
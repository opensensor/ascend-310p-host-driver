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

#ifndef KA_FS_PUB_H
#define KA_FS_PUB_H

#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/poll.h>
#include <linux/mount.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/posix_types.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>

#include "ka_macro_pub.h"
#include "ka_common_pub.h"

#define KA_POLLIN       POLLIN
#define KA_POLLERR      POLLERR
#define KA_POLLRDNORM   POLLRDNORM

#define KA_S_IRUGO      S_IRUGO
#define KA_S_IRUSR      S_IRUSR
#define KA_S_IRGRP      S_IRGRP
#define KA_S_IWUSR      S_IWUSR
#define KA_S_IWGRP      S_IWGRP
#define KA_S_IROTH      S_IROTH

#define KA_O_RDONLY     O_RDONLY
#define KA_O_CREAT      O_CREAT
#define KA_O_RDWR       O_RDWR
#define KA_O_TRUNC      O_TRUNC

#define KA_LOOKUP_FOLLOW    LOOKUP_FOLLOW

typedef struct vfsmount ka_vfsmount_t;
typedef struct attribute ka_attribute_t;
#define ka_fs_get_dev_attr(dev_attr) \
    &dev_attr.attr,

typedef struct attribute_group ka_attribute_group_t;
#define ka_fs_init_ag_name(ag_name) \
    .name = ag_name,
#define ka_fs_init_ag_attrs(ag_attrs) \
    .attrs = ag_attrs,

typedef struct file_operations ka_file_operations_t;
#define ka_fs_init_f_owner(f_owner) \
    .owner = f_owner,
#define ka_fs_init_f_llseek(f_llseek) \
    .llseek = f_llseek,
#define ka_fs_init_f_read(f_read) \
    .read = f_read,
#define ka_fs_init_f_write(f_write) \
    .write = f_write,
#define ka_fs_init_f_poll(f_poll) \
    .poll = f_poll,
#define ka_fs_init_f_unlocked_ioctl(f_unlocked_ioctl) \
    .unlocked_ioctl = f_unlocked_ioctl,
#define ka_fs_init_f_compat_ioctl(f_compat_ioctl) \
    .compat_ioctl = f_compat_ioctl,
#define ka_fs_init_f_mmap(f_mmap) \
    .mmap = f_mmap,
#define ka_fs_init_f_open(f_open) \
    .open = f_open,
#define ka_fs_init_f_release(f_release) \
    .release = f_release,
#define ka_fs_init_f_get_unmapped_area(f_get_unmapped_area) \
    .get_unmapped_area = f_get_unmapped_area,
#define ka_fs_init_f_check_flags(f_check_flags) \
    .check_flags = f_check_flags,

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
typedef struct proc_ops ka_procfs_ops_t;
#define ka_fs_init_pf_owner(pf_owner)
#define ka_fs_init_pf_open(pf_open) \
    .proc_open = pf_open,
#define ka_fs_init_pf_read(pf_read) \
    .proc_read = pf_read,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#define ka_fs_init_pf_read_iter(pf_read_iter) \
    .proc_read_iter = pf_read_iter,
#else
#define ka_fs_init_pf_read_iter(pf_read_iter)
#endif
#define ka_fs_init_pf_write(pf_write) \
    .proc_write = pf_write,
#define ka_fs_init_pf_lseek(pf_lseek) \
    .proc_lseek = pf_lseek,
#define ka_fs_init_pf_release(pf_release) \
    .proc_release = pf_release,
#else
typedef struct file_operations ka_procfs_ops_t;
#define ka_fs_init_pf_owner(pf_owner) \
    .owner = pf_owner,
#define ka_fs_init_pf_open(pf_open) \
    .open = pf_open,
#define ka_fs_init_pf_read(pf_read) \
    .read = pf_read,
#define ka_fs_init_pf_read_iter(pf_read_iter)
#define ka_fs_init_pf_write(pf_write) \
    .write = pf_write,
#define ka_fs_init_pf_lseek(pf_lseek) \
    .llseek = pf_lseek,
#define ka_fs_init_pf_release(pf_release) \
    .release = pf_release,
#endif

static inline void *ka_fs_get_file_private_data(ka_file_t *file)
{
    return file->private_data;
}
static inline void ka_fs_set_file_private_data(ka_file_t *file, void *private_data)
{
    file->private_data = private_data;
}
static inline ka_inode_t *ka_fs_get_file_f_inode(ka_file_t *file)
{
    return file->f_inode;
}
static inline const ka_path_t *ka_fs_get_file_f_path(ka_file_t *file)
{
    return &file->f_path;
}
static inline ka_dentry_t *ka_fs_get_path_dentry(ka_path_t *path)
{
    return path->dentry;
}
static inline unsigned char *ka_fs_get_dentry_d_iname(ka_dentry_t *dentry)
{
    return dentry->d_iname;
}

static inline void ka_fs_set_file_operations_release(ka_file_operations_t *file_operations)
{
    file_operations->release = NULL;
}

static inline void *ka_fs_get_seq_file_private(ka_seq_file_t *seq_file)
{
#ifndef __cplusplus
    return seq_file->private;
#else
    return nullptr;
#endif
}

#define ka_fs_single_open single_open
#define ka_fs_single_release single_release
#define ka_fs_seq_read seq_read
#define ka_fs_seq_lseek seq_lseek
#define ka_fs_seq_printf seq_printf
#define ka_fs_generic_file_llseek_size(file, offset, whence, maxsize, eof) generic_file_llseek_size(file, offset, whence, maxsize, eof)
#define ka_fs_generic_file_llseek(file, offset, whence) generic_file_llseek(file, offset, whence)
#define ka_fs_no_llseek(file, offset, whence) no_llseek(file, offset, whence)
#define ka_fs_vfs_llseek(file, offset, whence) vfs_llseek(file, offset, whence)
ssize_t ka_fs_kernel_read(ka_file_t *file, void *buf, size_t count, loff_t *pos);
ssize_t ka_fs_kernel_write(ka_file_t *file, const void *buf, size_t count, loff_t *pos);
#define ka_fs_vfs_read(file, buf, count, pos) vfs_read(file, buf, count, pos)


#define ka_fs_path_get(path) path_get(path)
#define ka_fs_path_put(path) path_put(path)

#define ka_fs_kern_path(name, flags, path) kern_path(name, flags, path)
#define ka_fs_kern_path_mountpoint(dfd, name, path, flags) kern_path_mountpoint(dfd, name, path, flags)
#define ka_fs_kern_path_create(dfd, pathname, path, lookup_flags) kern_path_create(dfd, pathname, path, lookup_flags)
#define ka_fs_d_path(path, buf, buflen) d_path(path, buf, buflen)
#define ka_fs_fput(file) fput(file)
#define ka_fs_vfs_statfs(path, buf) vfs_statfs(path, buf)
#define ka_fs_get_unused_fd_flags(flags) get_unused_fd_flags(flags)
#define ka_fs_put_unused_fd(fd) put_unused_fd(fd)
#define ka_fs_fd_install(fd, file) fd_install(fd, file)
#define ka_fs_fget(fd) fget(fd)
#define ka_fs_fget_raw(fd) fget_raw(fd)
#define __ka_fs_fdget(fd) __fdget(fd)
#define ka_fs_igrab(inode) igrab(inode)
#define ka_fs_iput(inode) iput(inode)
#define ka_fs_proc_mkdir_mode(name, mode, parent) proc_mkdir_mode(name, mode, parent)
#define ka_fs_proc_mkdir(name, parent) proc_mkdir(name, parent)
#define ka_fs_proc_create_mount_point(name) proc_create_mount_point(name)
#define ka_fs_proc_create_data(name, mode, parent, fops, data) proc_create_data(name, mode, parent, fops, data)
#define ka_fs_proc_create(name, mode, parent, fops) proc_create(name, mode, parent, fops)
#define ka_fs_proc_create_seq_private(name, mode, parent, ops, state_size, data) proc_create_seq_private(name, mode, parent, ops, state_size, data)
#define ka_fs_proc_create_single_data(name, mode, parent, show, data) proc_create_single_data(name, mode, parent, show, data)
#define ka_fs_remove_proc_entry(name, parent) remove_proc_entry(name, parent)
#define ka_fs_remove_proc_subtree(name, parent) remove_proc_subtree(name, parent)
#define ka_fs_proc_remove(de) proc_remove(de)

#define ka_fs_kobject_set_name  kobject_set_name
#define ka_fs_register_chrdev_region(from, count, name) register_chrdev_region(from, count, name)
#define ka_fs_unregister_chrdev_region(from, count) unregister_chrdev_region(from, count)
#define ka_fs_alloc_chrdev_region(dev, baseminor, count, name) alloc_chrdev_region(dev, baseminor, count, name)
#define ka_fs_cdev_init(cdev, fops) cdev_init(cdev, fops)
#define ka_fs_cdev_alloc() cdev_alloc()
#define ka_fs_cdev_del(p) cdev_del(p)
#define ka_fs_cdev_add(p, dev, count) cdev_add(p, dev, count)
#define ka_fs_device_add(dev) device_add(dev)
#define ka_fs_device_del(dev) device_del(dev)
static inline void ka_fs_set_cdev_owner(ka_cdev_t *cdev, ka_module_t *owner)
{
    cdev->owner = owner;
}
static inline void ka_fs_set_cdev_ops(ka_cdev_t *cdev, ka_file_operations_t *fops)
{
    cdev->ops = fops;
}
static inline ka_kobject_t *ka_fs_get_cdev_kobj(ka_cdev_t *cdev)
{
    return &cdev->kobj;
}
static inline long long ka_fs_get_kstat_size(ka_kstat_t *stat)
{
    return (long long)stat->size;
}
int ka_fs_vfs_getattr(ka_path_t *path, ka_kstat_t *stat, u32 request_mask, unsigned int query_flags);
ssize_t ka_fs_read_file(ka_file_t *file, loff_t *pos, char *addr, size_t count);
#define ka_fs_simple_read_from_buffer(to, count, ppos, from, available) simple_read_from_buffer(to, count, ppos, from, available)
#define ka_fs_simple_write_to_buffer(to, available, ppos, from, count) simple_write_to_buffer(to, available, ppos, from, count)
#define ka_fs_kfree_link(p) kfree_link(p)
#define ka_fs_filp_open(filename, flags, mode) filp_open(filename, flags, mode)
#define ka_fs_filp_close(filp, id) filp_close(filp, id)
#define ka_fs_stream_open(inode, filp) stream_open(inode, filp)

#define ka_sysfs_create_group(kobj, grp) sysfs_create_group(kobj, grp)
#define ka_sysfs_remove_group(kobj, grp) sysfs_remove_group(kobj, grp)
#define ka_fs_debugfs_create_dir(name, parent) debugfs_create_dir(name, parent)
#define ka_fs_debugfs_lookup(name, parent) debugfs_lookup(name, parent)
#define ka_fs_debugfs_remove(dentry) debugfs_remove(dentry)
#define ka_fs_debugfs_remove_recursive(dentry) debugfs_remove_recursive(dentry)
#define ka_fs_debugfs_create_file(name, mode, parent, data, fops) debugfs_create_file(name, mode, parent, data, fops)
#define ka_fs_debugfs_create_ulong(name, mode, parent, value) debugfs_create_ulong(name, mode, parent, value)

#define ka_fs_iminor(inode) iminor(inode)

static inline ka_kuid_t ka_fs_get_inode_i_uid(ka_inode_t *inode)
{
    return inode->i_uid;
}
static inline ka_kgid_t ka_fs_get_inode_i_gid(ka_inode_t *inode)
{
    return inode->i_gid;
}
static inline umode_t ka_fs_get_inode_i_mode(ka_inode_t *inode)
{
    return inode->i_mode;
}
static inline void *ka_fs_get_inode_i_private(ka_inode_t *inode)
{
    return inode->i_private;
}

#define __ka_fs_mnt_is_readonly(mnt) __mnt_is_readonly(mnt)

#define ka_fs_file_inode(f) file_inode(f)

#define ka_mm_segment_t mm_segment_t

#define ka_fs_set_fs(fs) set_fs(fs)
#define ka_fs_get_fs(fs) get_fs(fs)

#define __KA_FS_ATTR(_name, _mode, _show, _store) __ATTR(_name, _mode, _show, _store)
#define __KA_FS_ATTR_RO(_name) __ATTR_RO(_name)

#endif

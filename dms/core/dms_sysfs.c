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

#ifdef AOS_LLVM_BUILD
#include <linux/proc_fs.h>
#else
#include <linux/sysfs.h>
#endif
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#include "ka_driver_pub.h"
#include "ka_memory_pub.h"

#include "davinci_api.h"
#include "dms_sysfs.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_dev_node.h"
#include "urd_feature.h"
#include "dms_sensor.h"
#include "dms_event_dfx.h"

#define DMS_ATTR_RD (00400 | 00040)
#define DMS_ATTR_WR (00200 | 00020)
#define DMS_ATTR_RW (DMS_ATTR_RD | DMS_ATTR_WR)
#define MAX_CAT_BUFSIZE 2010
#define PRINTT_LINE 30

STATIC ssize_t dms_sysfs_feature_list_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (buf == NULL) {
        dms_err("buf is NULL.\n");
        return 0;
    }

    return dms_feature_print_feature_list(buf);
}

STATIC ssize_t dms_sysfs_node_list_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    if (buf == NULL) {
        dms_err("buf is NULL.\n");
        return 0;
    }

    return dms_devnode_print_node_list(buf);
}

STATIC ssize_t dms_sysfs_sensor_list_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return dms_sensor_print_sensor_list(buf);
}

STATIC ssize_t dms_sysfs_sensor_mask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return 0;
}

STATIC ssize_t dms_sysfs_sensor_mask_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return 0;
}

STATIC ssize_t dms_sysfs_channel_flux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return dms_event_dfx_channel_flux_show(buf);
}

STATIC ssize_t dms_sysfs_channel_flux_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return dms_event_dfx_channel_flux_store(buf, count);
}

STATIC ssize_t dms_sysfs_convergent_diagrams_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return dms_event_dfx_convergent_diagrams_show(buf);
}

STATIC ssize_t dms_sysfs_convergent_diagrams_store(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count)
{
    return dms_event_dfx_convergent_diagrams_store(buf, count);
}

STATIC ssize_t dms_sysfs_event_list_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return dms_event_dfx_event_list_show(buf);
}

STATIC ssize_t dms_sysfs_event_list_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return dms_event_dfx_event_list_store(buf, count);
}

STATIC ssize_t dms_sysfs_mask_list_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return dms_event_dfx_mask_list_show(buf);
}

STATIC ssize_t dms_sysfs_mask_list_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    return dms_event_dfx_mask_list_store(buf, count);
}

STATIC ssize_t dms_sysfs_subscribe_handle_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return dms_event_dfx_subscribe_handle_show(buf);
}

STATIC ssize_t dms_sysfs_subscribe_process_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return dms_event_dfx_subscribe_process_show(buf);
}

#ifdef AOS_LLVM_BUILD

typedef ssize_t (*proc_show_func)(struct device *dev, struct device_attribute *attr, char *buf);
typedef ssize_t (*proc_store_func)(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

struct dms_procfs_attr {
    struct proc_dir_entry *entry;
    const char *name;
    unsigned int mode;
    struct file_operations ops;
};

#define drv_procfs_func_write(file, buf, count, store)  do { \
    char *store_buf = NULL; \
    ssize_t offset; \
    proc_store_func store_func = store; \
    if (file == NULL || buf == NULL || store == NULL) { \
        return -1; \
    } \
    if (count == 0 || count >= KA_MM_PAGE_SIZE) { \
        return 0; \
    } \
    store_buf = dbl_vzalloc(count); \
    if (store_buf == NULL) { \
        return -1; \
    } \
    if (copy_from_user(store_buf, buf, count) != 0) { \
        dms_err("copy from user failed\n"); \
        dbl_vfree(store_buf); \
        return -1; \
    } \
    offset = store_func(NULL, NULL, store_buf, count); \
    dbl_vfree(store_buf); \
    return offset; \
}while (0)

#define drv_procfs_func_read(file, buf, count, show)  do { \
    char *store_buf = NULL; \
    ssize_t offset; \
    proc_show_func show_func = show; \
    if (file == NULL || buf == NULL || show == NULL) { \
        return -1; \
    } \
    if (count == 0) { \
        return 0; \
    } \
    store_buf = dbl_vzalloc(KA_MM_PAGE_SIZE); \
    if (store_buf == NULL) { \
        return -1; \
    } \
    offset = show_func(NULL, NULL, store_buf); \
    if (copy_to_user(buf, store_buf, offset) != 0) { \
        dms_err("sched copy to user err: %d\n"); \
        dbl_vfree(store_buf); \
        return -1; \
    } \
    dbl_vfree(store_buf); \
    return offset; \
}while (0)

#ifdef AOS_LLVM_BUILD
#define DRV_PROC_DEVICE_ATTR(_name, _mode, _show, _store) \
ssize_t dms_procfs_write_##_name(struct file *file, const char __user *buf, size_t count, loff_t *ppos) \
{ \
    drv_procfs_func_write(file, buf, count, _store); \
} \
ssize_t dms_procfs_read_##_name(struct file *file, char __user *buf, size_t count, loff_t *ppos) \
{ \
    drv_procfs_func_read(file, buf, count, _show); \
} \
struct dms_procfs_attr dms_procfs_attr_##_name = { \
    .name = __stringify(_name), \
    .mode = VERIFY_OCTAL_PERMISSIONS(_mode), \
    .ops.write = dms_procfs_write_##_name, \
    .ops.read = dms_procfs_read_##_name, \
}
#else
#define DRV_PROC_DEVICE_ATTR(_name, _mode, _show, _store) \
ssize_t dms_procfs_write_##_name(struct file *file, const char __user *buf, size_t count) \
{ \
    drv_procfs_func_write(file, buf, count, _store); \
} \
ssize_t dms_procfs_read_##_name(struct file *file, char __user *buf, size_t count) \
{ \
    drv_procfs_func_read(file, buf, count, _show); \
} \
struct dms_procfs_attr dms_procfs_attr_##_name = { \
    .name = __stringify(_name), \
    .mode = VERIFY_OCTAL_PERMISSIONS(_mode), \
    .ops.write = dms_procfs_write_##_name, \
    .ops.read = dms_procfs_read_##_name, \
}
#endif

#define DRV_DMS_FS_ATTR DRV_PROC_DEVICE_ATTR
#define DRV_FS_ATTR_NAME_POINTER(_name)  &dms_procfs_attr_##_name

#else

#define DRV_DMS_FS_ATTR KA_DRIVER_DEVICE_ATTR
#define DRV_FS_ATTR_NAME_POINTER(_name)  &dev_attr_##_name.attr

#endif

#define DECLARE_DMS_FS_ATTR \
    static DRV_DMS_FS_ATTR(feature_list, DMS_ATTR_RD, dms_sysfs_feature_list_show, NULL); \
    static DRV_DMS_FS_ATTR(node_list, DMS_ATTR_RD, dms_sysfs_node_list_show, NULL); \
    static DRV_DMS_FS_ATTR(sensor_list, DMS_ATTR_RD, dms_sysfs_sensor_list_show, NULL); \
    static DRV_DMS_FS_ATTR(sensor_trace, DMS_ATTR_RW, dms_sysfs_sensor_mask_show, dms_sysfs_sensor_mask_store); \
    static DRV_DMS_FS_ATTR(channel_flux, DMS_ATTR_RW, dms_sysfs_channel_flux_show, dms_sysfs_channel_flux_store); \
    static DRV_DMS_FS_ATTR(convergent_diagrams, DMS_ATTR_RW, dms_sysfs_convergent_diagrams_show, \
                                                             dms_sysfs_convergent_diagrams_store); \
    static DRV_DMS_FS_ATTR(event_list, DMS_ATTR_RW, dms_sysfs_event_list_show, dms_sysfs_event_list_store); \
    static DRV_DMS_FS_ATTR(mask_list, DMS_ATTR_RW, dms_sysfs_mask_list_show, dms_sysfs_mask_list_store); \
    static DRV_DMS_FS_ATTR(subscribe_handle, DMS_ATTR_RD, dms_sysfs_subscribe_handle_show, NULL); \
    static DRV_DMS_FS_ATTR(subscribe_process, DMS_ATTR_RD, dms_sysfs_subscribe_process_show, NULL)

#define DECLARE_DMS_FS_ATTR_LIST(_struct_type, _profix) \
    static struct _struct_type *g_dms_##_profix##_attr_list[] = { \
    DRV_FS_ATTR_NAME_POINTER(feature_list), \
    DRV_FS_ATTR_NAME_POINTER(node_list), \
    DRV_FS_ATTR_NAME_POINTER(sensor_list), \
    DRV_FS_ATTR_NAME_POINTER(sensor_trace), \
    DRV_FS_ATTR_NAME_POINTER(channel_flux), \
    DRV_FS_ATTR_NAME_POINTER(convergent_diagrams), \
    DRV_FS_ATTR_NAME_POINTER(event_list), \
    DRV_FS_ATTR_NAME_POINTER(mask_list), \
    DRV_FS_ATTR_NAME_POINTER(subscribe_handle), \
    DRV_FS_ATTR_NAME_POINTER(subscribe_process), \
    NULL, \
}

#ifdef AOS_LLVM_BUILD
static struct proc_dir_entry *g_dms_procfs_boot_entry;

DECLARE_DMS_FS_ATTR;

DECLARE_DMS_FS_ATTR_LIST(dms_procfs_attr, procfs);


void dms_procfs_boot_dir_create(void)
{
    g_dms_procfs_boot_entry = proc_mkdir("dms_node", NULL);
}

void dms_procfs_init(void)
{
    int i;
    int sum = sizeof(g_dms_procfs_attr_list) / sizeof(g_dms_procfs_attr_list[0]) - 1;
    struct dms_procfs_attr *attr = NULL;

    dms_procfs_boot_dir_create();
    for (i = 0; i < sum; i++) {
        attr = g_dms_procfs_attr_list[i];
        attr->entry = proc_create(attr->name, attr->mode, g_dms_procfs_boot_entry, &attr->ops);
    }
    return;
}

void dms_procfs_uninit(void)
{
    int i;
    int sum = sizeof(g_dms_procfs_attr_list) / sizeof(g_dms_procfs_attr_list[0]) - 1;
    struct dms_procfs_attr *attr = NULL;
    for (i = 0; i < sum; i++) {
        attr = g_dms_procfs_attr_list[i];
        proc_remove(attr->entry);
    }
    proc_remove(g_dms_procfs_boot_entry);
}

#else

DECLARE_DMS_FS_ATTR;

DECLARE_DMS_FS_ATTR_LIST(attribute, sysfs);

static const struct attribute_group g_dms_sysfs_group = {
    .attrs = g_dms_sysfs_attr_list,
    .name = "dms"
};

void dms_sysfs_init(void)
{
    int ret;
    struct device *dev = davinci_intf_get_owner_device();
    if (dev == NULL) {
        dms_err("dms dev is NULL, sysfs init failed.\n");
        return;
    }

    ret = sysfs_create_group(&dev->kobj, &g_dms_sysfs_group);
    if (ret != 0) {
        dms_err("dms sysfs create group failed.\n");
        return;
    }
    dms_info("dms sysfs init.\n");
    return;
}

void dms_sysfs_uninit(void)
{
    struct device *dev = davinci_intf_get_owner_device();
    if (dev == NULL) {
        return;
    }
    sysfs_remove_group(&dev->kobj, &g_dms_sysfs_group);
}

#endif



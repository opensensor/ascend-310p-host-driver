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
 *
 * Description:
 * Author: huawei
 * Create: 2025-02-22
 */

#define SWITCH_PERMISSION 0666

#include "securec.h"

#include "ka_dfx_pub.h"
#include "ka_base_pub.h"
#include "ka_errno_pub.h"
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_net_pub.h"

#define BUFFER_SIZE 50

#define TD_PRINT_ERR(fmt, ...) ka_dfx_printk(KERN_ERR "[ts_debug][ERROR]<%s:%d> " \
    fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define TD_PRINT_INFO(fmt, ...) ka_dfx_printk(KERN_INFO "[ts_debug][INFO]<%s:%d> " \
    fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

static int debug_mode = 0;
static ka_proc_dir_entry_t *ent;

static ssize_t debug_proc_write(ka_file_t *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
    char kernel_buf[BUFFER_SIZE] = { 0 };
    if (*ppos < 0) {
        TD_PRINT_ERR("ppos's initial value error,ppos's value can't be negative\n");
        return -EFAULT;
    } else if (*ppos > 0) {
        TD_PRINT_ERR("ppos's initial value error,ppos greater than zero\n");
        return -EFAULT;
    }
    if (ubuf == NULL) {
        TD_PRINT_ERR("ubuf is NULL\n");
        return -EFAULT;
    }
    if (count > BUFFER_SIZE) {
        TD_PRINT_ERR("size of write content must less than 50\n");
        return -EFAULT;
    }
    int ret = ka_base_copy_from_user(kernel_buf, ubuf, count);
    if (ret != 0) {
        TD_PRINT_ERR("kernel copy from user failed\n");
        return -EFAULT;
    }
    ret = sscanf_s(kernel_buf, "%d", &debug_mode);
    if (ret != 1) {
        TD_PRINT_ERR("kernel sscanf execute failed\n");
        return -EFAULT;
    }
    return count;
}
static ssize_t debug_proc_read(ka_file_t *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    char kernel_buf[BUFFER_SIZE] = { 0 };
    int len = 0;
    if (*ppos > 0) {
        return 0;
    }
    if (ubuf == NULL) {
        TD_PRINT_ERR("ubuf is NULL\n");
        return -EFAULT;
    }
    len += sprintf_s(kernel_buf, sizeof(kernel_buf), "debug_switch_status = %d\n", debug_mode);
    if (len < 0) {
        TD_PRINT_ERR("sprintf_s failed, ret = %d\n", len);
        return -EFAULT;
    }
    if (ka_base_copy_to_user(ubuf, kernel_buf, len)) {
        TD_PRINT_ERR("copy to user Failed\n");
        return -EFAULT;
    }
    *ppos += len;
    return len;
}

static ssize_t debug_proc_read_iter(ka_kiocb_t *iocb, ka_iov_iter_t *iter)
{
    char kernel_buf[BUFFER_SIZE] = { 0 };
    int len;
    if (iocb == NULL || iter == NULL) {
        TD_PRINT_ERR("iocb or iter is NULL\n");
        return -EFAULT;
    }
    if (ka_net_get_ki_pos(iocb) > 0) {
        return 0;
    }

    len = sprintf_s(kernel_buf, sizeof(kernel_buf), "debug_switch_status = %d\n", debug_mode);
    if (len < 0) {
        TD_PRINT_ERR("sprintf_s failed, ret = %d\n", len);
        return -EFAULT;
    }
    if (!ka_net_copy_to_iter(kernel_buf, len, iter)) {
        TD_PRINT_ERR("copy to user Failed\n");
        return -EFAULT;
    }
    ka_net_set_ki_pos(iocb, ka_net_get_ki_pos(iocb)+len);
    return len;
}

ka_procfs_ops_t debug_switch_ops = {
    ka_fs_init_pf_read(debug_proc_read)
    ka_fs_init_pf_write(debug_proc_write)
    ka_fs_init_pf_read_iter(debug_proc_read_iter)
};

static int debug_switch_init(void)
{
    ent = ka_fs_proc_create("debug_switch", SWITCH_PERMISSION, NULL, &debug_switch_ops);
    if (!ent) {
        TD_PRINT_ERR("Failed to create proc entry\n");
        return -ENOMEM;
    }
    TD_PRINT_INFO("debug_switch_created!\n");
    return 0;
}

static void debug_switch_exit(void)
{
    ka_fs_proc_remove(ent);
    TD_PRINT_INFO("debug_switch_destroyed!\n");
}

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#ifndef PCI_VENDOR_ID_HUAKUNZHENYU
#define PCI_VENDOR_ID_HUAKUNZHENYU 0x20c6
#endif
#ifndef PCI_VENDOR_ID_CHANGJIANG
#define PCI_VENDOR_ID_CHANGJIANG 0x20e9
#endif

static const struct pci_device_id g_debug_switch_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAKUNZHENYU, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAKUNZHENYU, 0xd500), 0},
    {KA_PCI_VDEVICE(CHANGJIANG, 0xd802), 0},
    {KA_PCI_VDEVICE(CHANGJIANG, 0xd500), 0},
    {}
};
KA_MODULE_DEVICE_TABLE(pci, g_debug_switch_tbl);

ka_module_init(debug_switch_init);
ka_module_exit(debug_switch_exit);

KA_MODULE_DESCRIPTION("ts debug channel switch");
KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., LTD.");

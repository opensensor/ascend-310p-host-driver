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

#include <linux/slab.h>

#include "ka_fs_pub.h"

#include "davinci_api.h"
#include "urd_define.h"
#include "urd_notifier.h"
#include "urd_feature.h"
#include "urd_kv.h"
#include "securec.h"
#include "urd_init.h"

#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"

#ifndef AOS_LLVM_BUILD
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
#include <linux/namei.h>
#endif
#else
#include <linux/namei.h>
#endif

STATIC int copy_from_user_local(void *to, unsigned long to_len, const void *from, unsigned long from_len)
{
    return (int)ka_base_copy_from_user(to, (void __user *)from, from_len);
}

STATIC int copy_to_user_local(void *to, unsigned long to_len, const void *from, unsigned long from_len)
{
    return (int)ka_base_copy_to_user((void __user *)to, from, from_len);
}

STATIC int memcpy_s_local(void *to, unsigned long to_len, const void *from, unsigned long from_len)
{
    return memcpy_s(to, to_len, from, from_len);
}

int (*copy_func_to[MSG_FROM_TYPE_MAX])(void *to, unsigned long to_len, const void *from, unsigned long from_len) = {
    copy_to_user_local,
    memcpy_s_local,
    copy_to_user_local,
};

int (*copy_func_from[MSG_FROM_TYPE_MAX])(void *to, unsigned long to_len, const void *from, unsigned long from_len) = {
    copy_from_user_local,
    memcpy_s_local,
    copy_from_user_local,
};

STATIC int dms_open(struct inode *inode, ka_file_t *filep)
{
    struct urd_file_private_stru *file_private = NULL;
    if (filep == NULL) {
        return -EINVAL;
    }

    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        return -EBUSY;
    }

    file_private =
        (struct urd_file_private_stru *)kzalloc(sizeof(struct urd_file_private_stru), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (file_private == NULL) {
        dms_err("kzalloc failed. (size=%lu)\n", sizeof(struct urd_file_private_stru));
        ka_system_module_put(KA_THIS_MODULE);
        return -ENOMEM;
    }
    file_private->owner_pid = ka_task_get_current()->tgid;
    filep->private_data = (void *)file_private;
    ka_base_atomic_set(&file_private->work_count, 0);
    return 0;
}

STATIC int dms_release(struct inode* inode, ka_file_t *filep)
{
    struct urd_file_private_stru* file_private = NULL;

    if (filep == NULL) {
        return -EINVAL;
    }
    ka_system_module_put(KA_THIS_MODULE);

    file_private = filep->private_data;
    if (file_private == NULL) {
        return -EBADFD;
    }
    urd_notifier_call(URD_NOTIFIER_RELEASE, file_private);

    kfree(file_private);
    file_private = NULL;
    return 0;
}

STATIC unsigned int dms_msg_poll(ka_file_t *filep, struct poll_table_struct *wait)
{
    return KA_POLLERR;
}

STATIC int dms_make_feature_key(struct dms_ioctl_arg *ctl_arg, DMS_FEATURE_ARG_S *arg)
{
    int ret;
    char* filter = NULL;

    if ((ctl_arg->filter_len > FILTER_MAX_LEN) || ((ctl_arg->filter_len == 0) && (ctl_arg->filter != NULL))) {
        dms_err("filter_len is error. (filter_len=%u)", ctl_arg->filter_len);
        return -EINVAL;
    }
    arg->key = (char*)kzalloc(KV_KEY_MAX_LEN + 1, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (arg->key == NULL) {
        dms_err("kzalloc failed. (size=%d)\n", KV_KEY_MAX_LEN);
        return -ENOMEM;
    }
    if (ctl_arg->filter != NULL) {
        filter = kzalloc(ctl_arg->filter_len + 1, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (filter == NULL) {
            dms_err("kzalloc failed. (size=%u)\n", ctl_arg->filter_len + 1);
            kfree(arg->key);
            arg->key = NULL;
            return -ENOMEM;
        }
        ret = copy_func_from[ctl_arg->msg_source]((void*)filter, (unsigned long)ctl_arg->filter_len + 1,
            (void*)ctl_arg->filter, (unsigned long)ctl_arg->filter_len);
        if (ret != 0) {
            dms_err("copy_func_from failed. (ret=%d; size=%u)\n", ret, ctl_arg->input_len);
            kfree(arg->key);
            arg->key = NULL;
            kfree(filter);
            filter = NULL;
            return -EFAULT;
        }
    }
    ret = dms_feature_make_key(ctl_arg->main_cmd, ctl_arg->sub_cmd, filter, arg->key, KV_KEY_MAX_LEN + 1);
    if (ret != 0) {
        dms_err(
            "make feature's key failed. (ret=%d; main_cmd=%u; sub_cmd=%u)\n", ret, ctl_arg->main_cmd, ctl_arg->sub_cmd);
        kfree(arg->key);
        arg->key = NULL;
    }
    if (filter != NULL) {
        kfree(filter);
        filter = NULL;
    }
    return ret;
}

STATIC int dms_make_feature_input(struct dms_ioctl_arg *ctl_arg, DMS_FEATURE_ARG_S *arg)
{
    int ret;
    arg->input = NULL;
    arg->input_len = 0;
    /* means no input data */
    if ((ctl_arg->input_len == 0) && (ctl_arg->input == NULL)) {
        return 0;
    }

    if ((ctl_arg->input_len != 0) && (ctl_arg->input != NULL) && (ctl_arg->input_len <= DMS_MAX_INPUT_LEN)) {
        arg->input = kmalloc(ctl_arg->input_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (arg->input == NULL) {
            dms_err("kzalloc failed. (size=%u)\n", ctl_arg->input_len);
            return -ENOMEM;
        }
        arg->input_len = ctl_arg->input_len;
        ret = copy_func_from[ctl_arg->msg_source]((void*)arg->input, (unsigned long)arg->input_len,
            (void*)ctl_arg->input, (unsigned long)ctl_arg->input_len);
        if (ret != 0) {
            dms_err("copy_func_from failed. (ret=%d; size=%u)\n", ret, ctl_arg->input_len);
            kfree(arg->input);
            arg->input = NULL;
            return -EFAULT;
        }
        return 0;
    } else {
        dms_err("Input para error. (input_len=%u)\n", ctl_arg->input_len);
        return -EINVAL;
    }
}

#define DMS_OUTPUT_2M_LIMIT (2 * 1024 * 1024)
// apply high-capacity memory for output
STATIC int dms_make_feature_max_output(struct dms_ioctl_arg *ctl_arg, DMS_FEATURE_ARG_S *arg)
{
    if (ctl_arg->output_len > DMS_OUTPUT_2M_LIMIT) {
        dms_err("Output len is larger than 2M limit. (output_len=%u)\n", ctl_arg->output_len);
        return -EINVAL;
    }

    arg->output = kzalloc(ctl_arg->output_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (arg->output == NULL) {
        dms_err("kzalloc failed. (size=%u)\n", ctl_arg->output_len);
        return -ENOMEM;
    }
    arg->output_len = ctl_arg->output_len;
    return 0;
}

// if need high-capacity memory(greater than DMS_MAX_OUTPUT_LEN) return true
static bool dms_is_need_high_capacity_memory(struct dms_ioctl_arg *ctl_arg)
{
    //  get history fault event info need high-capacity memory
    if ((ctl_arg->main_cmd == DMS_MAIN_CMD_BASIC && ctl_arg->sub_cmd == DMS_SUBCMD_GET_HISTORY_FAULT_EVENT) ||
        (ctl_arg->main_cmd == DMS_MAIN_CMD_BASIC && ctl_arg->sub_cmd == DMS_SUBCMD_GET_FAULT_EVENT_OBJ) ||
        (ctl_arg->main_cmd == DMS_MAIN_CMD_BASIC && ctl_arg->sub_cmd == DMS_SUBCMD_GET_DEVICE_STATE)) {
        return true;
    }
    return false;
}

STATIC int dms_make_feature_output(struct dms_ioctl_arg *ctl_arg, DMS_FEATURE_ARG_S *arg)
{
    arg->output = NULL;
    arg->output_len = 0;

    /* means no output data */
    if (ctl_arg->output_len == 0) {
        return 0;
    }

    // need high-capacity memory, maybe greater than DMS_MAX_OUTPUT_LEN
    if (dms_is_need_high_capacity_memory(ctl_arg)) {
        return dms_make_feature_max_output(ctl_arg, arg);
    }

    if (ctl_arg->output_len <= DMS_MAX_OUTPUT_LEN) {
        arg->output = kzalloc(ctl_arg->output_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (arg->output == NULL) {
            dms_err("kzalloc failed. (size=%u)\n", ctl_arg->output_len);
            return -ENOMEM;
        }
        arg->output_len = ctl_arg->output_len;
        return 0;
    } else {
        dms_err("Output len error. (output_len=%u)\n", ctl_arg->output_len);
        return -EINVAL;
    }
}

STATIC int dms_proc_feature_output(struct dms_ioctl_arg *ctl_arg, DMS_FEATURE_ARG_S *arg)
{
    int ret;

    if (ctl_arg->output != NULL) {
        ret = copy_func_to[ctl_arg->msg_source]((void*)ctl_arg->output, (unsigned long)ctl_arg->output_len,
            (void*)arg->output, (unsigned long)ctl_arg->output_len);
        if (ret != 0) {
            dms_err("copy_func_to failed. (size=%u)\n", ctl_arg->output_len);
            return -EFAULT;
        }
    }

    return 0;
}

STATIC void dms_free_msg(DMS_FEATURE_ARG_S *arg, struct dms_ioctl_arg *ctl_arg)
{
    if (arg->key != NULL) {
        kfree(arg->key);
        arg->key = NULL;
    }
    if (arg->input != NULL) {
        kfree(arg->input);
        arg->input = NULL;
    }
    if (arg->output != NULL) {
        kfree(arg->output);
        arg->output = NULL;
    }
    return;
}

STATIC int dms_make_up_msg_arg(struct dms_ioctl_arg *ctl_arg, DMS_FEATURE_ARG_S *arg)
{
    int ret;
    ret = dms_make_feature_key(ctl_arg, arg);
    if (ret != 0) {
        return ret;
    }

    ret = dms_make_feature_input(ctl_arg, arg);
    if (ret != 0) {
        dms_free_msg(arg, ctl_arg);
        return ret;
    }

    ret = dms_make_feature_output(ctl_arg, arg);
    if (ret != 0) {
        dms_free_msg(arg, ctl_arg);
        return ret;
    }
    return 0;
}

STATIC int dms_cmd_process(struct dms_ioctl_arg *ctl_arg)
{
    int ret;
    DMS_FEATURE_ARG_S arg = {0};

    if (ctl_arg->msg_source >= MSG_FROM_TYPE_MAX) {
        dms_err("msg_source is wrong. (msg_source=%d)\n", ctl_arg->msg_source);
        return -EINVAL;
    }
    arg.msg_source = ctl_arg->msg_source;
    /* make up call arg */
    ret = dms_make_up_msg_arg(ctl_arg, &arg);
    if (ret != 0) {
        dms_err("Make up message arg failed. (ret=%d)\n", ret);
        return ret;
    }

    /* call feature process */
    ret = dms_feature_process(&arg);
    if (ret != 0) {
        /* do not print err info */
        goto out;
    }

    /* copy out data to user */
    ret = dms_proc_feature_output(ctl_arg, &arg);
    if (ret != 0) {
        dms_err("Process output failed. (ret=%d; size=%u)\n", ret, ctl_arg->output_len);
    }

out:
    dms_free_msg(&arg, ctl_arg);
    return ret;
}

#define CMD_MASK (0x0000FFFFU)
STATIC bool check_cmd_arg_valid(unsigned int cmd, struct dms_ioctl_arg ctl_arg)
{
    if ((cmd & CMD_MASK) == ((ctl_arg.main_cmd << _KA_IOC_TYPESHIFT) + (ctl_arg.sub_cmd << _KA_IOC_NRSHIFT))) {
        return true;
    } else {
        return false;
    }
}

int dms_cmd_process_from_kernel(struct dms_ioctl_arg *ctl_arg)
{
    if (ctl_arg == NULL) {
        dms_err("Invalid file_private_data.\n");
        return -EINVAL;
    }

    ctl_arg->msg_source = MSG_FROM_KERNEL;
    return dms_cmd_process(ctl_arg);
}
KA_EXPORT_SYMBOL(dms_cmd_process_from_kernel);

STATIC long dms_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct dms_ioctl_arg ctl_arg = {0};
    if ((filep == NULL) || (arg == 0)) {
        dms_err("Invalid file_private_data.\n");
        return -EINVAL;
    }

    /* copy ioctl arg from user */
    ret = ka_base_copy_from_user(&ctl_arg, (void*)((uintptr_t)arg), sizeof(struct dms_ioctl_arg));
    if (ret != 0) {
        dms_err("copy_from_user_safe failed.\n");
        return ret;
    }
    if ((cmd == DMS_IOCTL_CMD) || (check_cmd_arg_valid(cmd, ctl_arg))) {
        ctl_arg.msg_source = (ascend_intf_is_restrict_access(filep) ? MSG_FROM_USER_REST_ACC : MSG_FROM_USER);
        ret = dms_cmd_process(&ctl_arg);
    } else {
        dms_err("Invalid command. (command=%u; main_cmd=%u; sub_cmd=%u)\n", cmd, ctl_arg.main_cmd, ctl_arg.sub_cmd);
        ret = -ENODEV;
    }
    return ret;
}

const ka_file_operations_t g_dms_file_operations = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(dms_open)
    ka_fs_init_f_release(dms_release)
    ka_fs_init_f_poll(dms_msg_poll)
    ka_fs_init_f_unlocked_ioctl(dms_ioctl)
};

STATIC int urd_release_prepare(ka_file_t *file_op, unsigned long mode)
{
    if (mode != NOTIFY_MODE_RELEASE_PREPARE) {
        dms_err("Invalid mode. (mode=%lu)\n", mode);
        return -EINVAL;
    }

    if ((file_op == NULL) || (file_op->private_data == NULL)) {
        return -EBADFD;
    }

    urd_notifier_call(URD_NOTIFIER_RELEASE_PREPARE, file_op->private_data);
    return 0;
}

static const struct notifier_operations urd_notifier_ops = {
    .owner = KA_THIS_MODULE,
    .notifier_call = urd_release_prepare,
};

STATIC int __init urd_init(void)
{
    int ret;

    dms_info("dms urd driver init start.\n");
    ret = drv_davinci_register_sub_module(DAVINCI_INTF_MODULE_DMS, &g_dms_file_operations);
    if (ret != 0) {
        dms_err("Failed to register DMS module. (ret=%d)\n", ret);
        return ret;
    }

    ret = drv_ascend_register_notify(DAVINCI_INTF_MODULE_DMS, &urd_notifier_ops);
    if (ret != 0) {
        dms_err("Failed to register davinci notify. (ret=%d)\n", ret);
        goto unregister_sub_module;
    }

    ret = dms_kv_init();
    if (ret) {
        dms_err("Failed to init dms kv. (ret=%d)\n", ret);
        goto unregister_davinci_notify;
    }

    dms_info("dms urd driver init success.\n");
    return 0;

unregister_davinci_notify:
    (void)drv_ascend_unregister_notify(DAVINCI_INTF_MODULE_DMS);
unregister_sub_module:
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_DMS);
    return ret;
}

STATIC void __exit urd_exit(void)
{
    dms_info("Dms urd exit start.\n");
    dms_kv_uninit();
    (void)drv_ascend_unregister_notify(DAVINCI_INTF_MODULE_DMS);
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_DMS);
    dms_info("Dms urd driver exit success.\n");
    return;
}

ka_module_init(urd_init);
ka_module_exit(urd_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");

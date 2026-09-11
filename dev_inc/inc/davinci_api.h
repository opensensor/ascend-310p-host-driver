/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2023. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2020-12-6
 * File Name     : davinci_api.h
 * Version       : Initial Draft
 * Author        :
 * Created       : 2020-05-06
 * Last Modified :
 * Description   : head file

 *  History       :
 * 1.Date        : 2020-12-06
 *    Author      :
 *    Modification: Created file
 */
#ifndef __DAVINCI_API_H__
#define __DAVINCI_API_H__

union status_para_un {
    u32 device_id;
    u32 process_id;
};

struct ascend_intf_get_status_para {
    u32 type;
    union status_para_un para;
};

#define NOTIFY_MODE_RELEASE_PREPARE      0x0001
#define NOTIFY_MODE_RELEASE_FINISH       0x0002
#define NOTIFY_MODE_REMOVE_PREPARE       0x0003

struct notifier_operations {
    struct module *owner;
    int (*notifier_call) (struct file *file_op, unsigned long mode);
};
int drv_ascend_register_notify(
    const char *module_name,
    const struct notifier_operations *notifier);
int drv_ascend_unregister_notify(const char *module_name);

void drv_davinci_bind_ctrl_cpu(void *release_task);
int drv_davinci_register_sub_parallel_module(const char *module_name, const struct file_operations *ops);
int drv_davinci_register_sub_module(const char *module_name, const struct file_operations *ops);
int drv_davinci_register_sub_module_cnt(const char *module_name, const struct file_operations *ops,
    unsigned int open_module_max);

int drv_ascend_unregister_sub_module(const char *module_name);
int drv_davinci_set_private_data(const struct file *filep, const void *priv_data);
void *drv_davinci_get_private_data(const struct file *filep);
u32 drv_davinci_get_device_id(const struct file *filep);
int ascend_intf_report_process_status(pid_t pid, unsigned int status);
bool davinci_intf_confirm_user(void);
u32 davinci_intf_get_manage_group(void);
int ascend_intf_report_device_status(unsigned int device_id, unsigned int status);
int ascend_intf_get_status(struct ascend_intf_get_status_para para, unsigned int *status);
struct device *davinci_intf_get_owner_device(void);
int drv_ascend_replace_sub_module_fops(const char *module_name, const struct file_operations *ops);
int ascend_intf_is_pid_init(pid_t process_id, const char *module_name);
bool ascend_intf_is_restrict_access(struct file *filep);
#endif

/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* Description:
* Author: huawei
* Create: 2024-3-11
*/

#ifndef KERNEL_ADAPT_INIT_H
#define KERNEL_ADAPT_INIT_H

#include <linux/sched.h>
#include <linux/printk.h>

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC  static
#endif

#define module_ka "kernel_adapt"

#define ka_err(fmt, ...)    printk(KERN_ERR "[ascend][ERROR][%s][%s %d]<%s:%d>" fmt, module_ka,     \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)
#define ka_warn(fmt, ...)   printk(KERN_WARNING "[ascend][WARN][%s][%s %d]<%s:%d>" fmt, module_ka,  \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)
#define ka_info(fmt, ...)   printk(KERN_INFO "[ascend][INFO][%s][%s %d]<%s:%d>" fmt, module_ka,     \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)
#define ka_event(fmt, ...)  printk(KERN_NOTICE "[ascend][EVENT][%s][%s %d]<%s:%d>" fmt, module_ka,  \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)
#define ka_debug(fmt, ...)  printk(KERN_DEBUG "[ascend][DBG][%s][%s %d]<%s:%d>" fmt, module_ka,     \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)

#endif

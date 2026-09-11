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

#ifndef TSDRV_LOG_H
#define TSDRV_LOG_H

#include <linux/sched.h>
#include <linux/version.h>
#include "ascend_hal_define.h"
#include "ka_memory.h"
#include "ka_dfx_pub.h"

void tsdrv_share_log_err_inner(const char *fmt, ...);

#ifdef TSDRV_UT
#ifndef STATIC
#define STATIC
#endif

#include <stdio.h>
#include <stdarg.h>

#ifdef CFG_FEATURE_SHARE_LOG_INNER
#define TSDRV_PRINT_ERR(fmt, ...) do {                                          \
        ka_dfx_printk(KA_KERN_ERR "[ascend][ERROR][%s][%s %d]<%s:%d>" fmt, "tsdrv",       \
            __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__);   \
        tsdrv_share_log_err_inner(fmt, ##__VA_ARGS__);                          \
} while(0)
#else
#define TSDRV_PRINT_ERR(fmt, ...)                                           \
    ka_dfx_printk(KA_KERN_ERR "[ascend][ERROR][%s][%s %d]<%s:%d>" fmt, "tsdrv",       \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)
#endif

#define TSDRV_PRINT_WARN(fmt, ...)                                          \
    ka_dfx_printk(KA_KERN_WARNING "[ascend][WARN][%s][%s %d]<%s:%d>" fmt, "tsdrv",    \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)

#define TSDRV_PRINT_INFO(fmt, ...)                                          \
    ka_dfx_printk(KA_KERN_INFO "[ascend][INFO][%s][%s %d]<%s:%d>" fmt, "tsdrv",       \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)

#define TSDRV_PRINT_EVENT(fmt, ...)                                         \
    ka_dfx_printk(KA_KERN_NOTICE "[ascend][EVENT][%s][%s %d]<%s:%d>" fmt, "tsdrv",    \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)

#define TSDRV_PRINT_DEBUG(fmt, ...)                                         \
    ka_dfx_printk(KA_KERN_DEBUG "[ascend][DBG][%s][%s %d]<%s:%d>" fmt, "tsdrv",       \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)

#define TSDRV_PRINT_WARN_LIMIT(fmt, ...)                                           \
    ka_dfx_printk(KA_KERN_WARNING "[ascend][WARN][%s][%s %d]<%s:%d>" fmt, "tsdrv",    \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)

#define TSDRV_PRINT_INFO_LIMIT(fmt, ...)                                           \
    ka_dfx_printk(KA_KERN_INFO "[ascend][INFO][%s][%s %d]<%s:%d>" fmt, "tsdrv",       \
        __func__, __LINE__, current->comm, current->tgid, ##__VA_ARGS__)

#define TSDRV_SPINLOCK_ERR(fmt...)    TSDRV_PRINT_ERR(fmt)
#define TSDRV_SPINLOCK_WARN(fmt...)   TSDRV_PRINT_WARN(fmt)
#define TSDRV_SPINLOCK_INFO(fmt...)   TSDRV_PRINT_INFO(fmt)
#define TSDRV_SPINLOCK_EVENT(fmt...)  TSDRV_PRINT_EVENT(fmt)
#define TSDRV_SPINLOCK_DEBUG(fmt...)  TSDRV_PRINT_DEBUG(fmt)

#define TSDRV_PRINT_FPGA(fmt...)      TSDRV_PRINT_DEBUG(fmt)
#define TSDRV_SAMPLE_BACK_DEBUG(fmt...)      TSDRV_PRINT_DEBUG(fmt)


#define TSDRV_KA_MODULE_ID_TYPE_0               ka_get_module_id(HAL_MODULE_TYPE_TS_DRIVER, KA_SUB_MODULE_TYPE_0)

#define tsdrv_kmalloc(size, flags)              ka_kmalloc(size, flags, TSDRV_KA_MODULE_ID_TYPE_0)
#define tsdrv_kzalloc(size, flags)              ka_kzalloc(size, flags, TSDRV_KA_MODULE_ID_TYPE_0)
#define tsdrv_kfree(addr)                       ka_kfree(addr, TSDRV_KA_MODULE_ID_TYPE_0)

#define tsdrv_vzalloc(size)                     ka_vzalloc(size, TSDRV_KA_MODULE_ID_TYPE_0)
#define tsdrv_vmalloc(size, gfp_mask, prot)     __ka_vmalloc(size, gfp_mask, prot, TSDRV_KA_MODULE_ID_TYPE_0)
#define tsdrv_vfree(addr)                       ka_vfree(addr, TSDRV_KA_MODULE_ID_TYPE_0)

#define tsdrv_kvzalloc(size, flags)             ka_kvzalloc(size, flags, TSDRV_KA_MODULE_ID_TYPE_0)
#define tsdrv_kvfree(addr)                      ka_kvfree(addr, TSDRV_KA_MODULE_ID_TYPE_0)


#else /* !TSDRV_UT */
#include "drv_log.h"
#include "drv_share_log.h"

#define module_tsdrv "tsdrv"

#ifndef STATIC
#define STATIC static
#endif

#ifdef CFG_FEATURE_SHARE_LOG_INNER
#define TSDRV_PRINT_ERR(fmt, ...) do { \
    drv_err(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__); \
    tsdrv_share_log_err_inner(fmt, ##__VA_ARGS__); \
} while (0)
#else
#define TSDRV_PRINT_ERR(fmt, ...) do { \
    drv_err(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__); \
    share_log_err(TSDRV_SHARE_LOG_START, fmt, ##__VA_ARGS__); \
} while (0)
#endif


#define TSDRV_PRINT_WARN(fmt, ...)  drv_warn(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define TSDRV_PRINT_INFO(fmt, ...)  drv_info(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define TSDRV_PRINT_EVENT(fmt, ...) drv_event(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define TSDRV_PRINT_DEBUG(fmt, ...) drv_pr_debug(module_tsdrv, "<%s:%d> " fmt,     \
        current->comm, current->tgid, ##__VA_ARGS__)

#define TSDRV_PRINT_WARN_LIMIT(fmt, ...) do {           \
        static int cnt = 0;                             \
        if (cnt++ < 10) {                               \
            TSDRV_PRINT_WARN(fmt, ##__VA_ARGS__);       \
        }                                               \
} while(0)

#define TSDRV_PRINT_INFO_LIMIT(fmt, ...) do {           \
        static int cnt = 0;                             \
        if (cnt++ < 10) {                               \
            TSDRV_PRINT_INFO(fmt, ##__VA_ARGS__);       \
        }                                               \
} while(0)

#define TSDRV_SPINLOCK_ERR(fmt, ...)    \
        drv_err_spinlock(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define TSDRV_SPINLOCK_WARN(fmt, ...)   \
        drv_warn_spinlock(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define TSDRV_SPINLOCK_INFO(fmt, ...)   \
        drv_info_spinlock(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define TSDRV_SPINLOCK_DEBUG(fmt, ...)  \
        drv_debug_spinlock(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)
#define TSDRV_SPINLOCK_EVENT(fmt, ...)  \
        drv_event_spinlock(module_tsdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__)

/* for fpga debug */
#define TSDRV_PRINT_FPGA(fmt, ...)

#ifdef CFG_SOC_SAMPLE_BACK_DEBUG
#  define TSDRV_SAMPLE_BACK_DEBUG TSDRV_PRINT_INFO
#else
#  define TSDRV_SAMPLE_BACK_DEBUG
#endif

#define TSDRV_KA_MODULE_ID_TYPE_0               ka_get_module_id(HAL_MODULE_TYPE_TS_DRIVER, KA_SUB_MODULE_TYPE_0)
#define TSDRV_MEM_PRINT_DEBUG(fmt, ...)

#define tsdrv_kmalloc(size, flags)              ka_kmalloc(size, flags, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_kmalloc. (size=%u)\n", size)
#define tsdrv_kzalloc(size, flags)              ka_kzalloc(size, flags, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_kzalloc. (size=%lu)\n", size)
#define tsdrv_kfree(addr)                       ka_kfree(addr, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_kfree.\n")

#define tsdrv_vzalloc(size)                     ka_vzalloc(size, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_vzalloc. (size=%lu)\n", size)
#define tsdrv_vmalloc(size, gfp_mask, prot)     __ka_vmalloc(size, gfp_mask, prot, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_vmalloc. (size=%u)\n", size)
#define tsdrv_vfree(addr)                       ka_vfree(addr, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_vfree.\n")

#define tsdrv_kvzalloc(size, flags)             ka_kvzalloc(size, flags, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_kvzalloc. (size=%u)\n", size)
#define tsdrv_kvfree(addr)                      ka_kvfree(addr, TSDRV_KA_MODULE_ID_TYPE_0); \
                                                    TSDRV_MEM_PRINT_DEBUG("tsdrv_kvfree.\n")

#endif /* TSDRV_UT */

#endif /* TSDRV_LOG_H */

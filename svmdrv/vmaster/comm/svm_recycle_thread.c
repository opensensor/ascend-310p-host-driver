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
 */
#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_errno_pub.h"
#include "feature_loader.h"

#include "svm_log.h"
#include "svm_recycle_thread.h"

#define SVM_MAX_RECYCLE_HANDLE_NUM  2U

static ka_task_struct_t *g_recycle_thread = NULL;
static void (* g_recycle_handle[SVM_MAX_RECYCLE_HANDLE_NUM])(void) = {NULL, };
static u32 g_recycle_thread_pause = 0U;
static u32 g_recycle_thread_quick_exit = 0U;

void svm_recycle_handle_register(void (*func)(void))
{
    u32 i;

    for (i = 0; i < SVM_MAX_RECYCLE_HANDLE_NUM; i++) {
        if (g_recycle_handle[i] == NULL) {
            g_recycle_handle[i] = func;
        }
    }
}

static void svm_call_recycle_handle(void)
{
    u32 i;

    for (i = 0; i < SVM_MAX_RECYCLE_HANDLE_NUM; i++) {
        if (g_recycle_handle[i] != NULL) {
            g_recycle_handle[i]();
        }
    }
}

void svm_recycle_thread_pause(void)
{
    g_recycle_thread_pause = 1U;
}

void svm_recycle_thread_continue(void)
{
    g_recycle_thread_pause = 0U;
}
#ifndef EMU_ST
static void ssleep_interruptible(u32 seconds)
{
    u32 i;

    for (i = 0; i < seconds; i++) {
        if (g_recycle_thread_quick_exit == 1U) {
            break;
        }
        ka_system_msleep_interruptible(1000U);
    }
}

static int svm_recycle_thread(void *data)
{
    g_recycle_thread_quick_exit = 0U;
    while (!ka_task_kthread_should_stop()) {
        if (g_recycle_thread_pause == 0U) {
            svm_call_recycle_handle();
        }
 
        ssleep_interruptible(60); /* 60s */
    }

    return 0;
}

static int svm_recycle_thread_init(void)
{
    g_recycle_thread = ka_task_kthread_run(svm_recycle_thread, NULL, "svm_recycle_thread");
    if (KA_IS_ERR(g_recycle_thread)) {
        g_recycle_thread = NULL;
        devmm_drv_err("Failed to create recycle thread\n");
        return -EINVAL;
    }
 
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_recycle_thread_init, FEATURE_LOADER_STAGE_2);

static void svm_recycle_thread_uninit(void)
{
    if (g_recycle_thread != NULL) {
        g_recycle_thread_quick_exit = 1U;
        ka_task_kthread_stop(g_recycle_thread);
        g_recycle_thread = NULL;
    }
}
DECLAER_FEATURE_AUTO_UNINIT(svm_recycle_thread_uninit, FEATURE_LOADER_STAGE_2);
#endif

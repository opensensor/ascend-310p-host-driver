/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-11-15
*/
#include <linux/vmalloc.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include "dms_timer.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dbl_mem_alloc_interface.h"
#include "ascend_platform.h"

#ifndef AOS_LLVM_BUILD
#include "ascend_kernel_hal.h"
#endif

#include "uda.h"

#include "dms_ts_info.h"
#include "dms_ts_npu_utilization.h"

#include "ka_system_pub.h"
#include "ka_memory_pub.h"

STATIC struct npu_util_ctrl_block* g_npu_util_cb[ASCEND_DIE_NUM_MAX] = {NULL};
STATIC spinlock_t g_npu_util_lock[ASCEND_DIE_NUM_MAX];
STATIC unsigned int g_npu_num = 0;
STATIC struct npu_util_hrtimer g_npu_util_timer;

STATIC void rb_write(u32 devid, unsigned char val)
{
    unsigned long flags;
    struct npu_util_ctrl_block *cb = NULL;

    spin_lock_irqsave(&(g_npu_util_lock[devid]), flags);
    cb = g_npu_util_cb[devid];
    if (cb == NULL) {
        spin_unlock_irqrestore(&(g_npu_util_lock[devid]), flags);
        return;
    }

    cb->buf[cb->head % NPU_UTILIZATION_SAMPLE_NUM] = val;  // 循环覆盖
    cb->head++;
    spin_unlock_irqrestore(&(g_npu_util_lock[devid]), flags);
}

int rb_read_all(u32 devid, u32 *occupy_count)
{
    unsigned long flags;
    int ret = 0;
    int i;
    struct npu_util_ctrl_block *cb = NULL;

    spin_lock_irqsave(&(g_npu_util_lock[devid]), flags);
    cb = g_npu_util_cb[devid];
    if (cb == NULL) {
        spin_unlock_irqrestore(&(g_npu_util_lock[devid]), flags);
        return -EINVAL;
    }

    for (i = 0; i < NPU_UTILIZATION_SAMPLE_NUM; i++) {
        *occupy_count += cb->buf[i];
    }
    spin_unlock_irqrestore(&(g_npu_util_lock[devid]), flags);

    return ret;
}

STATIC unsigned char npu_is_used_cal(u32 dev_id)
{
    u32 acsq_tmp;
    int i;
    void __iomem **remap_addr = g_npu_util_cb[dev_id]->remap_addr;

    for (i = 0; i < ACSQ_MAX_NUM; i++ ) {
        acsq_tmp = readl(remap_addr[i]);
        if (acsq_tmp & SQID_VALID_MASK) {
            return 1;
        }
    }
    return 0;
}

STATIC void dms_ts_record_npu_util(void)
{
    u32 dev_id;
    unsigned char occupy_status = 0;

    for (dev_id = 0; dev_id < g_npu_num; dev_id++) {
        occupy_status = npu_is_used_cal(dev_id);
        rb_write(dev_id, occupy_status);
    }
}

STATIC enum hrtimer_restart dms_npu_util_timer_callback(struct hrtimer *timer)
{
    (void)dms_ts_record_npu_util();
    hrtimer_forward(timer, hrtimer_cb_get_time(timer), g_npu_util_timer.period);
    return HRTIMER_RESTART; // 继续循环
}

STATIC int dms_ts_npu_util_timer_init(void)
{
    g_npu_util_timer.period = ms_to_ktime(NPU_UTILIZATION_CYCLE_MS);
    hrtimer_init(&g_npu_util_timer.timer, KA_CLOCK_MONOTONIC, KA_HRTIMER_MODE_REL);
    g_npu_util_timer.timer.function = dms_npu_util_timer_callback;
    hrtimer_start(&g_npu_util_timer.timer, g_npu_util_timer.period, KA_HRTIMER_MODE_REL);
    dms_info("Dms_ts_npu_util_timer_init success.\n");
    return 0;
}

STATIC void dms_ts_npu_util_timer_uninit(void)
{
    int ret = hrtimer_cancel(&g_npu_util_timer.timer);
    dms_info("Dms_ts_npu_util_timer_uninit success.(ret=%u)\n", ret);
}

STATIC int dms_ts_npu_util_cb_init(u32 devid)
{
    int i, ret;
    phys_addr_t phy_addr = 0, dev0_phy_addr = 0;

    if (devid >= ASCEND_DIE_NUM_MAX) {
        dms_err("Invalid devid.(devid=%u)\n", devid);
        return -EINVAL;
    }

    spin_lock_init(&(g_npu_util_lock[devid]));

    if (g_npu_util_cb[devid] != NULL) {
        dms_err("G_npu_util_cb already init.(devid=%u)\n", devid);
        return -EEXIST;
    }

    g_npu_util_cb[devid] = dbl_kzalloc(sizeof(struct npu_util_ctrl_block),  KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_npu_util_cb[devid] == NULL) {
        dms_err("Malloc for npu util cb failed. (devid=%u)\n", devid);
        return -ENOMEM;
    }

    for (i = 0; i < ACSQ_MAX_NUM; i++) {
        dev0_phy_addr = REG_STARS_SQ_MAP1_ADDR + i * (REG_STARS_SQ_MAP1_OFFSET);
        phy_addr = hal_kernel_get_dev_phy_base_addr(devid, dev0_phy_addr);
        if (phy_addr == 0) {
            dms_err("Get phy addr failed. (devid=%u; i=%d)\n", devid, i);
            ret = -EINVAL;
            goto addr_unmap;
        }
        g_npu_util_cb[devid]->remap_addr[i] = ioremap(phy_addr, sizeof(u32));
        if (g_npu_util_cb[devid]->remap_addr[i] == NULL) {
            dms_err("Ioremap for npu util cb failed. (devid=%u; i=%d)\n", devid, i);
            ret = -ENOMEM;
            goto addr_unmap;
        }
    }
    return 0;

addr_unmap:
    for (; i > 0; i--) {
        iounmap(g_npu_util_cb[devid]->remap_addr[i - 1]);
        g_npu_util_cb[devid]->remap_addr[i - 1] = NULL;
    }
    dbl_kfree(g_npu_util_cb[devid]);
    g_npu_util_cb[devid] = NULL;
    return ret;
}

STATIC void dms_ts_npu_util_cb_uninit(u32 devid)
{
    int i;
    unsigned long flags;
    if (devid >= ASCEND_DIE_NUM_MAX) {
        dms_err("Invalid devid.(devid=%u)\n", devid);
        return;
    }

    spin_lock_irqsave(&(g_npu_util_lock[devid]), flags);
    if (g_npu_util_cb[devid] == NULL) {
        spin_unlock_irqrestore(&(g_npu_util_lock[devid]), flags);
        return;
    }

    for (i = 0; i < ACSQ_MAX_NUM; i++) {
        iounmap(g_npu_util_cb[devid]->remap_addr[i]);
        g_npu_util_cb[devid]->remap_addr[i] = NULL;
    }

    dbl_kfree(g_npu_util_cb[devid]);
    g_npu_util_cb[devid] = NULL;
    spin_unlock_irqrestore(&(g_npu_util_lock[devid]), flags);
}

int dms_ts_npu_utilization_init(void)
{
    int ret, i;

    g_npu_num = uda_get_detected_phy_dev_num();
    if (g_npu_num == 0) {
        dms_err("Get npu num failed.\n");
        return -ENODEV;
    }

    for (i = 0; i < g_npu_num; i++) {
        ret = dms_ts_npu_util_cb_init(i);
        if (ret != 0) {
            dms_err("Npu util cb init failed. (devid=%u; ret=%d)\n", i, ret);
            goto cb_uninit;
        }
    }

    ret = dms_ts_npu_util_timer_init();
    if (ret != 0) {
        dms_err("Npu util timer init failed. (ret=%d)\n", ret);
        goto cb_uninit;
    }

    dms_info("Dms_ts_npu_utilization_init success. (npu_num=%u)\n", g_npu_num);
    return 0;

cb_uninit:
    for (i = 0; i < g_npu_num; i++) {
        dms_ts_npu_util_cb_uninit(i);
    }
    return ret;
}

void dms_ts_npu_utilization_exit(void)
{
    int i;

    dms_ts_npu_util_timer_uninit();

    for (i = 0; i < g_npu_num; i++) {
        dms_ts_npu_util_cb_uninit(i);
    }

    dms_info("Dms_ts_npu_utilization_exit success. (npu_num=%u)\n", g_npu_num);
    g_npu_num = 0;
}

int dms_get_npu_utilization(core_utilization_rate_t *core_status, int copy_flag)
{
    unsigned char utilization = 0;
    u32 occupy_count = 0;
    int ret;

    if (core_status->dev_id >= g_npu_num) {
        dms_err("Invalid devid. (devid=%u)\n", core_status->dev_id);
        return -EINVAL;
    }

    core_status->core_num = NPU_NUM_PER_DEV;
    ret = rb_read_all(core_status->dev_id, &occupy_count);
    if (ret != 0) {
        dms_err("Get npu occupy_count failed. (devid=%u; ret=%d)\n", core_status->dev_id, ret);
        return ret;
    }

    occupy_count = occupy_count * NPU_UTILIZATION_RATE_PERCENT / NPU_UTILIZATION_SAMPLE_NUM;
    utilization = (occupy_count < NPU_UTILIZATION_RATE_PERCENT)? occupy_count : NPU_UTILIZATION_RATE_PERCENT;

    ret = copy_to_user_safe(core_status->core_utilization_rate, &utilization, core_status->core_num);
    if (ret != 0) {
        dms_err("Copy utilization to user space failed. (devid=%u; core_num = %u; ret=%d)\n",
            core_status->dev_id, core_status->core_num, ret);
        return ret;
    }

    return 0;
}
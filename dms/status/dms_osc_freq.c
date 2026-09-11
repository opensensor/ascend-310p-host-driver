/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
* Create: 2023-5-29
*/

#ifndef AOS_LLVM_BUILD
#include <linux/time64.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/clock.h>
#else
#include <linux/sched.h>
#endif

#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "drv_type.h"
#include "dbl/uda.h"

#include "dms_define.h"
#include "dms_cmd_def.h"
#include "dms_template.h"
#include "dms_acc_ctrl.h"
#include "dms_notifier.h"
#include "dms_kernel_version_adapt.h"

#ifndef CFG_FEATURE_RC_MODE
#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_msg.h"
#endif
#include "devdrv_manager_container.h"
#include "devdrv_user_common.h"

#include "dms_osc_freq.h"

#ifndef CFG_DMS_TEST

#define SEC_TO_USEC 1000000ULL
#define FREQ_TO_KHZ 1000ULL
#define AVERAGE_2X 2
#define HOST_FREQ_INVALID 0xFFFFFFFFFFFFFFFFULL
static u64 g_host_osc_freq[DEVDRV_PF_DEV_MAX_NUM] = {0};
static u64 g_device_osc_freq[DEVDRV_PF_DEV_MAX_NUM] = {0};

static struct task_struct *calculate_osc_freq_task[DEVDRV_PF_DEV_MAX_NUM] = {NULL};

#if defined(__aarch64__)
STATIC u64 get_local_system_freq(void)
{
    u64 freq = 0;

    asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));
    return freq;
}
#endif
#ifndef CFG_FEATURE_RC_MODE
STATIC u64 get_host_osc_cycles(void)
{
    u64 cycles = 0;

#if defined(__aarch64__)
    asm volatile("mrs %0, cntvct_el0" : "=r" (cycles));
#elif defined(__x86_64__)
    const u32 uint32Bits = 32;
    u32 hi = 0;
    u32 lo = 0;
    __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
    cycles = (uint64_t)(lo) | ((uint64_t)(hi) << uint32Bits);
#endif

    return cycles;
}

STATIC int dms_h2d_get_device_osc_cycles(u32 devid, u64 *cycles)
{
    int ret;
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_manager_get_devdrv_info(devid);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (devid=%u)\n", devid);
        return -EINVAL;
    }

    ret = devdrv_manager_h2d_sync_get_devinfo(dev_info);
    if (ret != 0) {
        dms_err("H2D get device info failed. (devid=%u) \n",  devid);
        return ret;
    }

    *cycles = dev_info->cpu_system_count;

    return 0;
}

STATIC int dms_get_dev_nominal_osc_freq(u32 dev_id, u64 *freq)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *freq = dev_info->dev_nominal_osc_freq;
    return 0;
}

#define DEV_FREQ_DIFF_TIME_MIN 100
STATIC int dms_check_and_update_freq(u32 dev_id)
{
    int ret;
    u64 dev_nominal_osc_freq = 0;
    u64 diff_val;

    ret = dms_get_dev_nominal_osc_freq(dev_id, &dev_nominal_osc_freq);
    if (ret != 0) {
        dms_err("Get device nominal osc freq fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (g_device_osc_freq[dev_id] < dev_nominal_osc_freq) {
        diff_val = dev_nominal_osc_freq - g_device_osc_freq[dev_id];
    } else {
        diff_val = g_device_osc_freq[dev_id] - dev_nominal_osc_freq;
    }

    dms_info("Calculate osc frequency. (dev_id=%u; host_osc_freq=%llu; device_osc_freq=%llu; dev_nominal_freq=%llu)\n",
        dev_id, g_host_osc_freq[dev_id], g_device_osc_freq[dev_id], dev_nominal_osc_freq);
    /*
     * if device calculate freq and nominal_freq deviation exceeds 1%, host_freq return 0, dev freq return nominal val;
     * others, return calculate freq;
     */
    if ((diff_val != 0) && (dev_nominal_osc_freq / diff_val < DEV_FREQ_DIFF_TIME_MIN)) {
        g_host_osc_freq[dev_id] = HOST_FREQ_INVALID;
        g_device_osc_freq[dev_id] = dev_nominal_osc_freq / FREQ_TO_KHZ;
    } else {
        g_host_osc_freq[dev_id] = g_host_osc_freq[dev_id] / FREQ_TO_KHZ;
        g_device_osc_freq[dev_id] = g_device_osc_freq[dev_id] / FREQ_TO_KHZ;
    }

    dms_info("Final osc frequency. (devid=%u; host_osc_freq=%llu; device_osc_freq=%llu)\n",
        dev_id, g_host_osc_freq[dev_id], g_device_osc_freq[dev_id]);
    return 0;
}
#endif

STATIC int dms_osc_freq_calculate_task(void *arg)
{
    u32 devid;

#ifdef CFG_FEATURE_RC_MODE
    devid = *(u32 *)arg;
    g_device_osc_freq[devid] = get_local_system_freq() / FREQ_TO_KHZ;
    g_host_osc_freq[devid] = g_device_osc_freq[devid];
    dms_info("Final osc frequency. (devid=%u; host_osc_freq=%llu; device_osc_freq=%llu)\n",
        devid, g_host_osc_freq[devid], g_device_osc_freq[devid]);
#else
#if defined(__x86_64__)
    u64 host_tick_start1, host_tick_start2, host_tick_end1, host_tick_end2;
    u64 host_start_time, host_end_time;
    u64 current_time;
#endif
    int ret;
    u64 host_osc_cycles_1, host_osc_cycles_2, host_osc_cycles_3, host_osc_cycles_4;
    u64 device_osc_cycles_1 = 0;
    u64 device_osc_cycles_2 = 0;

    devid = *(u32 *)arg;

    if (!try_module_get(KA_THIS_MODULE)) {
        ka_task_do_exit(0);
        return -EBUSY;
    }

#if defined(__x86_64__)
    host_tick_start1 = get_host_osc_cycles();
    current_time = local_clock();
    host_tick_start2 = get_host_osc_cycles();
    host_start_time = current_time / NSEC_PER_USEC;
#endif

    /* the first h2d dropped */
    dms_h2d_get_device_osc_cycles(devid, &device_osc_cycles_1);
    /* the second h2d dropped */
    dms_h2d_get_device_osc_cycles(devid, &device_osc_cycles_1);

    host_osc_cycles_1 = get_host_osc_cycles();
    ret = dms_h2d_get_device_osc_cycles(devid, &device_osc_cycles_1);
    host_osc_cycles_2 = get_host_osc_cycles();

    /* sleep 10 seconds to expand the time spec to reduce the deviation */
    ka_system_ssleep(10);

    host_osc_cycles_3 = get_host_osc_cycles();
    ret = dms_h2d_get_device_osc_cycles(devid, &device_osc_cycles_2);
    host_osc_cycles_4 = get_host_osc_cycles();

#if defined(__x86_64__)
    host_tick_end1 = get_host_osc_cycles();
    current_time = local_clock();
    host_tick_end2 = get_host_osc_cycles();
    host_end_time = current_time / NSEC_PER_USEC;
#endif

#if defined(__aarch64__)
    g_host_osc_freq[devid] = get_local_system_freq();
#elif defined(__x86_64__)
    g_host_osc_freq[devid] = (((host_tick_end1 + host_tick_end2) - (host_tick_start1 + host_tick_start2)) *\
        SEC_TO_USEC) / (AVERAGE_2X * (host_end_time - host_start_time));
    dms_info("Host info. (devid=%u; start1=%llu; start2=%llu; end1=%llu; end2=%llu; t_start=%llu; t_end=%llu)\n",
        devid, host_tick_start1, host_tick_start2, host_tick_end1, host_tick_end2, host_start_time, host_end_time);
#endif

    g_device_osc_freq[devid] = (AVERAGE_2X * g_host_osc_freq[devid] * (device_osc_cycles_2 - device_osc_cycles_1)) /
        ((host_osc_cycles_4 + host_osc_cycles_3) - (host_osc_cycles_2 + host_osc_cycles_1));

    dms_info("Device info. (devid=%u; tick_1=%llu; tick_2=%llu; tick_3=%llu; tick_4=%llu; dev_t1=%llu; dev_t2=%llu)\n",
        devid, host_osc_cycles_1, host_osc_cycles_2, host_osc_cycles_3, host_osc_cycles_4,
        device_osc_cycles_1, device_osc_cycles_2);

    dms_check_and_update_freq(devid);
    module_put(KA_THIS_MODULE);
#endif

    ka_task_do_exit(0);
    return 0;
}

STATIC int osc_freq_notifier(struct notifier_block *nb, unsigned long mode, void *data)
{
    struct devdrv_info *dev = NULL;

    if (data == NULL) {
        dms_err("Data is null.\n");
        return -ENOMEM;
    }
    dev = (struct devdrv_info *)data;
    if (dev->dev_id >= (u32)DEVICE_NUM_MAX) {
        dms_err("Device id is invalid. (dev_id=%u).\n", dev->dev_id);
        return -EINVAL;
    }
    if (dev->dev_id >= DEVDRV_PF_DEV_MAX_NUM) {
        dms_debug("The VF does not need to be initialized. (dev_id=%u).\n", dev->dev_id);
        return 0;
    }

    switch (mode) {
        case DMS_DEVICE_UP0:
            calculate_osc_freq_task[dev->dev_id] = kthread_create(dms_osc_freq_calculate_task, &(dev->dev_id),
            "dms_osc_freq_calc_task_%u", dev->dev_id);
            if (IS_ERR_OR_NULL(calculate_osc_freq_task[dev->dev_id])) {
                dms_err("Create thread for cpu freq calculate failed.\n");
                return -EINVAL;
            }
            (void)wake_up_process(calculate_osc_freq_task[dev->dev_id]);
            break;
        case DMS_DEVICE_DOWN0:
            g_host_osc_freq[dev->dev_id] = 0;
            break;
        default:
            break;
    }

    return 0;
}

STATIC struct notifier_block g_osc_freq_notifier = {
    .notifier_call = osc_freq_notifier,
};

STATIC int get_device_osc_freq(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    u32 devid, phy_id, vfid;
    struct uda_mia_dev_para mia_dev;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(u64))) {
        dms_err("Output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    devid = *(u32 *)in;

    if (devid >= DEVICE_NUM_MAX) {
        dms_err("Device id is invalid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(devid, &phy_id, &vfid);
    if (ret != 0) {
        dms_err("Logical id to physical id failed or container env. (ret=%d; devid=%u; vfid=%u)\n",
            ret, devid, vfid);
        return -EINVAL;
    }

    if (!uda_is_phy_dev(phy_id)) {
        ret = uda_udevid_to_mia_devid(phy_id, &mia_dev);
        if (ret != 0) {
            dms_err("Udevid to mia devid failed. (ret=%d; phy_id=%u)\n", ret, phy_id);
            return -EINVAL;
        }
        phy_id = mia_dev.phy_devid;
    }

    if (phy_id >= DEVDRV_PF_DEV_MAX_NUM) {
        dms_err("Physic id is invalid. (phy_id=%u)\n", phy_id);
        return -EINVAL;
    }

    if (g_device_osc_freq[phy_id] == 0) {
        dms_warn("device is not ready. (phy_id=%u)\n", phy_id);
        return -EBUSY;
    }

    ret = memcpy_s(out, out_len, &g_device_osc_freq[phy_id], sizeof(u64));
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return -ENOMEM;
    }

    return 0;
}

int dms_get_device_osc_freq(u32 devid, u64 *freq)
{
    if (devid >= DEVDRV_PF_DEV_MAX_NUM) {
        dms_err("Device id is invalid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (freq == NULL) {
        dms_err("Input ptr is NULL.\n");
        return -EINVAL;
    }

    if (g_device_osc_freq[devid] == 0) {
        dms_warn("Device is not ready. (phy_id=%u)\n", devid);
        return -EBUSY;
    }

    *freq = g_device_osc_freq[devid];

    return 0;
}

int dms_get_host_osc_freq(u64 *freq)
{
    unsigned int i;
    unsigned int host_deviation_time = 0;

    if (freq == NULL) {
        dms_err("Input ptr is NULL. \n");
        return -EINVAL;
    }

    for (i = 0; i < DEVDRV_PF_DEV_MAX_NUM; i++) {
        if (g_host_osc_freq[i] == HOST_FREQ_INVALID) {
            host_deviation_time++;
            continue;
        }

        if (g_host_osc_freq[i] != 0) {
            *freq = g_host_osc_freq[i];
            return 0;
        }
    }

    if (host_deviation_time > 0) {
        return -EOPNOTSUPP;
    }

    return -EAGAIN;
}

STATIC int get_host_osc_freq(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    u64 freq = 0;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(u64))) {
        dms_err("Output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    ret = dms_get_host_osc_freq(&freq);
    if (ret != 0) {
        return ret;
    }

    *(u64 *)out = freq;
    return 0;
}

int osc_freq_init(void)
{
    int ret = 0;

    ret = dms_register_notifier(&g_osc_freq_notifier);
    if (ret != 0) {
        dms_err("register dms notifier failed. (ret=%d)\n", ret);
        return ret;
    }
    CALL_INIT_MODULE(DMS_MODULE_OSC_FREQ);

    dms_debug("OSC module is initialized successfully.\n");
    return ret;
}

void osc_freq_exit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_OSC_FREQ);
    (void)dms_unregister_notifier(&g_osc_freq_notifier);
    dms_debug("OSC module exits.\n");
}

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_OSC_FREQ)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_OSC_FREQ,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_HOST_OSC_FREQ,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    get_host_osc_freq)
ADD_FEATURE_COMMAND(DMS_MODULE_OSC_FREQ,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_DEV_OSC_FREQ,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    get_device_osc_freq)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()
#else

int osc_freq_init(void)
{
    return 0;
}
void osc_freq_exit(void)
{
    return;
}
#endif

#endif

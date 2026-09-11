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

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "dms_pm_chain.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_manager.h"

struct dms_pm_chain {
    struct list_head pm_list_header; /* for power manager */
    struct mutex pm_list_lock;         /* for power manager */
};

static struct dms_pm_chain *dms_get_pm_chain(void)
{
    static struct dms_pm_chain pm_chain;
    return &pm_chain;
}


#ifndef CFG_FEATURE_HEART_BEAT
/*
 *
 * devdrv_manager_register_pm
 * run_stage: 0 - 1
 */
struct devdrv_pm *devdrv_manager_register_pm(int run_stage, int (*suspend)(u32 devid), int (*resume)(u32 devid))
{
    struct dms_pm_chain *pm_chain = NULL;
    struct devdrv_pm *pm = NULL;

    if ((suspend == NULL) || (resume == NULL)) {
        devdrv_drv_err("callback func is null.\n");
        return NULL;
    }

    pm = dbl_kzalloc(sizeof(struct devdrv_pm), KAGFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pm == NULL) {
        devdrv_drv_err("kmalloc failed.\n");
        return NULL;
    }

    pm->suspend = suspend;
    pm->resume = resume;
    pm->run_stage = run_stage;

    pm_chain = dms_get_pm_chain();
    if (pm_chain == NULL) {
        devdrv_drv_err("pm_chain is null.\n");
        dbl_kfree(pm);
        pm = NULL;
        return NULL;
    }

    mutex_lock(&pm_chain->pm_list_lock);
    list_add(&pm->list, &pm_chain->pm_list_header);
    mutex_unlock(&pm_chain->pm_list_lock);

    return pm;
}

KA_EXPORT_SYMBOL(devdrv_manager_register_pm);
/*
 *
 * devdrv_manager_unregister_pm
 *
 */
void devdrv_manager_unregister_pm(struct devdrv_pm *pm)
{
    struct dms_pm_chain *pm_chain = NULL;

    if (pm == NULL) {
        devdrv_drv_err("invalid input argument.\n");
        return;
    }
    if ((pm->suspend == NULL) || (pm->resume == NULL)) {
        devdrv_drv_err("callback func is null.\n");
        return;
    }

    pm_chain = dms_get_pm_chain();
    if (pm_chain == NULL) {
        devdrv_drv_err("pm_chain is null.\n");
        return;
    }

    mutex_lock(&pm_chain->pm_list_lock);
    list_del(&pm->list);
    mutex_unlock(&pm_chain->pm_list_lock);

    dbl_kfree(pm);
    pm = NULL;
}
KA_EXPORT_SYMBOL(devdrv_manager_unregister_pm);
/*
 *
 * devdrv_manager_suspend_proc
 * suspend func
 */
int devdrv_manager_suspend_proc(int devid, int run_stage)
{
    int ret;
    struct list_head *pos = NULL, *n = NULL;
    struct list_head *stop = NULL;
    struct devdrv_pm *pm = NULL;
    struct dms_pm_chain *pm_chain = NULL;

    devdrv_drv_debug("call devdrv_manager_suspend_proc.\n");

    pm_chain = dms_get_pm_chain();
    if (pm_chain == NULL) {
        devdrv_drv_err("pm_chain is null.\n");
        return -EINVAL;
    }

    mutex_lock(&pm_chain->pm_list_lock);
    list_for_each_safe(pos, n, &pm_chain->pm_list_header) {
        stop = pos;
        pm = list_entry(pos, struct devdrv_pm, list);
        if ((pm->suspend != NULL) && (pm->run_stage == run_stage)) {
            ret = pm->suspend(devid);
            if (ret) {
                mutex_unlock(&pm_chain->pm_list_lock);
                devdrv_drv_err("one suspend func failed ret = %d.\n", ret);
                return -EINVAL;
            }
        }
    }
    mutex_unlock(&pm_chain->pm_list_lock);

    devdrv_drv_info("devdrv_manager_suspend_proc succ.\n");
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_manager_suspend_proc);
/*
 *
 * devdrv_manager_resume_proc
 * resume func
 */
int devdrv_manager_resume_proc(struct devdrv_info *info)
{
    struct dms_pm_chain *pm_chain = NULL;
    struct list_head *pos = NULL, *n = NULL;
    struct devdrv_pm *pm = NULL;

    devdrv_drv_debug("call devdrv_manager_resume.\n");

    pm_chain = dms_get_pm_chain();
    if (pm_chain == NULL) {
        devdrv_drv_err("pm_chain is null.\n");
        return -EINVAL;
    }

    /* resume all registered resume callback func */
    mutex_lock(&pm_chain->pm_list_lock);
    list_for_each_safe(pos, n, &pm_chain->pm_list_header) {
        pm = list_entry(pos, struct devdrv_pm, list);
        if (pm->resume != NULL) {
            (void)pm->resume(info->dev_id);
        }
    }
    mutex_unlock(&pm_chain->pm_list_lock);

    devdrv_drv_info("resume succ.\n");

    return 0;
}
KA_EXPORT_SYMBOL(devdrv_manager_resume_proc);
#endif
/*
 *
 * dms_pm_chain_init
 * init func
 */
int dms_pm_chain_init(void)
{
    struct dms_pm_chain *pm_chain = NULL;

    devdrv_drv_debug("dms pm chain init.\n");

    pm_chain = dms_get_pm_chain();
    mutex_init(&pm_chain->pm_list_lock);
    INIT_LIST_HEAD(&pm_chain->pm_list_header);

    return 0;
}

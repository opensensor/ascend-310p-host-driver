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

#include "ts_agent_vsq_worker.h"
#include "securec.h"
#include "ts_agent_common.h"
#include "ts_agent_log.h"
#include "ts_agent_vsq_proc.h"
#include "virtmng_interface.h"
#include "ts_agent_resource.h"

typedef struct ts_agent_vsq_work_ctx {
    vsq_base_info_t vsq_base_info;      // used for ka_work_struct_t to find vsq info.
    ka_work_struct_t proc_work;
    ka_workqueue_struct_t *wq;          // for vsq_receive_proc
} vsq_work_ctx_t;

typedef struct ts_agent_vf_work_ctx {
    u32 vsq_num;
    vsq_work_ctx_t *vsq_work_ctx_list;  // array, size is vsq_num.
} vf_work_ctx_t;

// vf_id is >= 1 <= 16
static vf_work_ctx_t g_all_vf_worker[TS_AGENT_MAX_DEVICE_NUM][TS_AGENT_WQ_VF_MAX + 1][TS_AGENT_MAX_TS_NUM];

#ifdef CFG_SOC_PLATFORM_STARS
#define VF_WORK(_dev_id, _vf_id, _ts_id)     \
    g_all_vf_worker[_dev_id][0][_ts_id]
#else
#define VF_WORK(_dev_id, _vf_id, _ts_id)     \
    g_all_vf_worker[_dev_id][_vf_id][_ts_id]
#endif

int init_all_vf_work_ctx(void)
{
    u32 dev_id;
    u32 vf_id;
    u32 ts_id;
    vf_work_ctx_t *vf_work_ctx = NULL;
    for (dev_id = 0; dev_id < TS_AGENT_MAX_DEVICE_NUM; ++dev_id) {
        for (vf_id = TS_AGENT_VF_ID_MIN; vf_id <= TS_AGENT_WQ_VF_MAX; ++vf_id) {
            for (ts_id = 0; ts_id < TS_AGENT_MAX_TS_NUM; ++ts_id) {
                vf_work_ctx = &VF_WORK(dev_id, vf_id, ts_id);
                vf_work_ctx->vsq_num = 0;
                vf_work_ctx->vsq_work_ctx_list = NULL;
            }
        }
    }
    return 0;
}

static void destroy_vf_work_ctx(vf_work_ctx_t *vf_work_ctx)
{
    vsq_work_ctx_t *vsq_work_ctx = NULL;
    u32 vsq_id;
    if (vf_work_ctx->vsq_num == 0 || vf_work_ctx->vsq_work_ctx_list == NULL) {
        return;
    }
    for (vsq_id = 0; vsq_id < vf_work_ctx->vsq_num; ++vsq_id) {
        vsq_work_ctx = &vf_work_ctx->vsq_work_ctx_list[vsq_id];
        ka_task_cancel_work_sync(&vsq_work_ctx->proc_work);
        if (vsq_work_ctx->wq != NULL) {
            ka_task_destroy_workqueue(vsq_work_ctx->wq);
            vsq_work_ctx->wq = NULL;
        }
    }
    ka_mm_kfree(vf_work_ctx->vsq_work_ctx_list);
    vf_work_ctx->vsq_work_ctx_list = NULL;
    vf_work_ctx->vsq_num = 0;
}

void destroy_all_vf_work_ctx(void)
{
    u32 dev_id;
    u32 vf_id;
    u32 ts_id;
    vf_work_ctx_t *vf_work_ctx = NULL;
    for (dev_id = 0; dev_id < TS_AGENT_MAX_DEVICE_NUM; ++dev_id) {
        for (vf_id = TS_AGENT_VF_ID_MIN; vf_id <= TS_AGENT_WQ_VF_MAX; ++vf_id) {
            for (ts_id = 0; ts_id < TS_AGENT_MAX_TS_NUM; ++ts_id) {
                vf_work_ctx = &VF_WORK(dev_id, vf_id, ts_id);
                destroy_vf_work_ctx(vf_work_ctx);
            }
        }
    }
}

#ifndef TS_AGENT_UT
STATIC void proc_vsq_work(ka_work_struct_t *work)
{
    vsq_work_ctx_t *work_ctx = NULL;
    work_ctx = container_of(work, vsq_work_ctx_t, proc_work);
    proc_vsq(&work_ctx->vsq_base_info);
}
#endif

static int create_vsq_work_ctx(vsq_work_ctx_t *vsq_work_ctx)
{
    int ret;
    vsq_base_info_t *vsq_base_info = &vsq_work_ctx->vsq_base_info;
    char work_queue_name[TS_AGENT_MAX_WQ_NAME_LEN] = {0};
    ret = sprintf_s(work_queue_name, TS_AGENT_MAX_WQ_NAME_LEN, "tsa_%u_%u_%u_%u",
                    vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
    if (ret == -1) {
        ts_agent_err("format work queue name failed, dev_id=%u, vf_id=%u, ts_id=%u, vsq_num=%u.",
                     vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return EFAULT;
    }

    vsq_work_ctx->wq = ka_task_create_singlethread_workqueue(work_queue_name);
    if (vsq_work_ctx->wq == NULL) {
#ifndef TS_AGENT_UT
        ts_agent_err("ka_task_create_singlethread_workqueue failed, name=%s.", work_queue_name);
#endif
        return ENOMEM;
    }

#ifndef TS_AGENT_UT
    KA_TASK_INIT_WORK(&vsq_work_ctx->proc_work, proc_vsq_work);
#endif
    ts_agent_debug("create vsq work ctx success, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u",
                   vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
    return 0;
}

int create_vf_worker(u32 dev_id, u32 vf_id, u32 ts_id, u32 vsq_num)
{
    vf_work_ctx_t *vf_work_ctx = NULL;
    vsq_work_ctx_t *vsq_work_ctx = NULL;
    u32 vsq_id;
    u32 size;

    vf_work_ctx = &VF_WORK(dev_id, vf_id, ts_id);
    if (vf_work_ctx->vsq_num > 0 || vf_work_ctx->vsq_work_ctx_list != NULL) {
        ts_agent_err("vf work ctx repeat create, dev_id=%u, vf_id=%u, ts_id=%u, vsq_num=%u, last vsq_num=%u",
                     dev_id, vf_id, ts_id, vsq_num, vf_work_ctx->vsq_num);
        return -EINVAL;
    }

    vf_work_ctx->vsq_num = vsq_num;
    size = vf_work_ctx->vsq_num * (u32)sizeof(vsq_work_ctx_t);
    vf_work_ctx->vsq_work_ctx_list = (vsq_work_ctx_t *)ka_mm_kzalloc(size, KA_GFP_KERNEL);
    if (vf_work_ctx->vsq_work_ctx_list == NULL) {
        ts_agent_err("ka_mm_kzalloc failed, dev_id=%u, vf_id=%u, vsq_num=%u, size=%u",
                     dev_id, vf_id, vf_work_ctx->vsq_num, size);
        vf_work_ctx->vsq_num = 0;
        return -ENOMEM;
    }
    for (vsq_id = 0; vsq_id < vsq_num; ++vsq_id) {
        int ret;
        vsq_work_ctx = &vf_work_ctx->vsq_work_ctx_list[vsq_id];
        vsq_work_ctx->vsq_base_info.dev_id = dev_id;
        vsq_work_ctx->vsq_base_info.vf_id = vf_id;
        vsq_work_ctx->vsq_base_info.ts_id = ts_id;
        vsq_work_ctx->vsq_base_info.vsq_id = vsq_id;

        #ifndef CFG_SOC_PLATFORM_STARS
        // stars fill vsq info in vsq proc
        ret = fill_vsq_info(&vsq_work_ctx->vsq_base_info);
        if (ret != 0) {
            ts_agent_err("file vsq info failed, dev_id=%u, vf_id=%u, vsq_num=%u, vsq_id=%u",
                         dev_id, vf_id, vf_work_ctx->vsq_num, vsq_id);
            // rollback
            destroy_vf_work_ctx(vf_work_ctx);
            return ret;
        }
        #endif

        ret = create_vsq_work_ctx(vsq_work_ctx);
        if (ret != 0) {
            ts_agent_err("create vsq work ctx failed, dev_id=%u, vf_id=%u, vsq_num=%u, vsq_id=%u",
                         dev_id, vf_id, vf_work_ctx->vsq_num, vsq_id);
            // rollback
            destroy_vf_work_ctx(vf_work_ctx);
            return ret;
        }
    }
    ts_agent_info("create vf worker end, dev_id=%u, vf_id=%u, ts_id=%u, vsq_num=%u",
                  dev_id, vf_id, ts_id, vsq_num);
    return 0;
}

void destroy_vf_worker(u32 dev_id, u32 vf_id, u32 ts_id)
{
    destroy_vf_work_ctx(&VF_WORK(dev_id, vf_id, ts_id));
    ts_agent_info("destroy vf worker end, dev_id=%u, vf_id=%u, ts_id=%u", dev_id, vf_id, ts_id);
}

int schedule_vsq_work(const struct tsdrv_id_inst * const id_inst, u32 vsq_id,
    enum vsqcq_type vsq_type, u32 cmd_num)
{
    int ret;
    u32 dev_id = id_inst->devid;
    u32 vf_id = id_inst->fid;
    u32 ts_id = id_inst->tsid;
    u16 wq_sq_id = (u16)vsq_id;
    vf_work_ctx_t *vf_work_ctx = NULL;
    vsq_work_ctx_t *vsq_work_ctx = NULL;

    vf_work_ctx = &VF_WORK(dev_id, vf_id, ts_id);
    if (wq_sq_id >= vf_work_ctx->vsq_num) {
        ts_agent_err("wq_sq_id or vf_id is invalid, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, vsq_num=%u",
                     dev_id, vf_id, ts_id, vsq_id, vf_work_ctx->vsq_num);
        return -ERANGE;
    }
    if (vf_work_ctx->vsq_work_ctx_list == NULL) {
        ts_agent_err("vsq_work_ctx is null, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, vsq_num=%u",
                     dev_id, vf_id, ts_id, vsq_id, vf_work_ctx->vsq_num);
        return -EINVAL;
    }
    vsq_work_ctx = &vf_work_ctx->vsq_work_ctx_list[wq_sq_id];
    vsq_work_ctx->vsq_base_info.dev_id = dev_id;
    vsq_work_ctx->vsq_base_info.vf_id = vf_id;
    vsq_work_ctx->vsq_base_info.vsq_id = vsq_id;
    vsq_work_ctx->vsq_base_info.ts_id = ts_id;
    vsq_work_ctx->vsq_base_info.vsq_type = vsq_type;

    ret = vsq_top_proc(&vsq_work_ctx->vsq_base_info, cmd_num);
    if (ret != 0) {
        ts_agent_err("vsq top proc failed, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, cmd_num=%u",
                     dev_id, vf_id, ts_id, vsq_id, cmd_num);
    }

    if (ka_task_queue_work(vsq_work_ctx->wq, &vsq_work_ctx->proc_work)) {
        ts_agent_debug("schedule work success, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u",
                       dev_id, vf_id, ts_id, vsq_id);
    } else {
        ts_agent_debug("task is in schedule already, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u",
                       dev_id, vf_id, ts_id, vsq_id);
    }
    return ret;
}

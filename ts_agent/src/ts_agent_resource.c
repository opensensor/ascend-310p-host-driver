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

#include "virtmng_interface.h"
#include "ts_agent_resource.h"
#include "hvtsdrv_tsagent.h"
#include "ts_agent_log.h"
#include "tsch/task_struct.h"

int get_vf_info(u32 dev_id, u32 vf_id, struct vmng_soc_resource_enquire *vf_res)
{
#ifndef CFG_SOC_PLATFORM_MINIV3 // delete it when 1911 support vmng
    int ret;
    ts_agent_debug("get vf info start, dev_id=%u, vf_id=%u.", dev_id, vf_id);
    ret = vmngh_enquire_soc_resource(dev_id, vf_id, vf_res);
    if (ret != 0) {
        ts_agent_err("get vf info failed, ret=%d, dev_id=%u, vf_id=%u.", ret, dev_id, vf_id);
        return ret;
    }
    ts_agent_info(
        "get vf info end, dev_id=%u, vf_id=%u, aicore_num=%u, vfg_id=%u, vf_aicpu_bitmap=%u, vfg_aicpu_bitmap=%u.",
        dev_id, vf_id, vf_res->each.stars_static.aic, vf_res->each.vfg.vfg_id,
        vf_res->each.stars_refresh.device_aicpu, vf_res->vfg.stars_refresh.device_aicpu);
#endif
    return 0;
}

int get_vf_vsq_num(u32 dev_id, u32 vf_id, u32 ts_id, u32 *vsq_num)
{
    int ret;
    ts_agent_debug("get vf vsq num start, dev_id=%u, vf_id=%u, ts_id=%u.", dev_id, vf_id, ts_id);
    ret = hvtsdrv_get_res_num(dev_id, vf_id, ts_id, TSDRV_SQ_ID, vsq_num);
    if (ret != 0) {
        ts_agent_err("get vf vsq num failed, ret=%d, dev_id=%u, vf_id=%u, ts_id=%u.", ret, dev_id, vf_id, ts_id);
        return ret;
    }
    if (*vsq_num > TS_AGENT_MAX_SQ_NUM) {
        ts_agent_err("Driver get vsq num=%u is out of range(0, %u], dev_id=%u, vf_id=%u, ts_id=%u.",
                     *vsq_num, TS_AGENT_MAX_SQ_NUM, dev_id, vf_id, ts_id);
        return -ERANGE;
    }

    ts_agent_debug("get vf vsq num end, dev_id=%u, vf_id=%u, ts_id=%u, vsq_num=%u.",
                   dev_id, vf_id, ts_id, *vsq_num);
    return 0;
}

static int convert_virt_to_phy(const vsq_base_info_t *vsq_base_info, enum tsdrv_id_type id_type, u16 v_id, u16 *id)
{
    int ret;
    struct hvtsdrv_id_v2p v2p;
    ts_agent_debug("convert id from virt to phy start, v_id=%u, id_type=%d.", v_id, id_type);
    v2p.id_type = id_type;
    v2p.virt_id = v_id;
    v2p.phy_id = 0;
    ret = hvtsdrv_resid_v2p(vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, &v2p);
    if (ret != 0) {
        ts_agent_err("convert virt id failed, ret=%d, dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u, id_type=%d.",
                     ret, vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, v_id, id_type);
        return ret;
    }
    // 0xFFFFU is max value of uint16
    if (v2p.phy_id > 0xFFFFU) {
        ts_agent_err("Driver convert result phy_id=%u is out of range[0, %u], dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
            v2p.phy_id, 0xFFFFU, vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, v_id);
        return -ERANGE;
    }
    *id = (u16) v2p.phy_id;
    ts_agent_debug("convert id from virt to phy end, v_id=%u, phy_id=%u, id_type=%d.", v_id, *id, id_type);
    return 0;
}

int convert_sq_id(const vsq_base_info_t *vsq_base_info, u16 v_sq_id, u16 *sq_id)
{
    return convert_virt_to_phy(vsq_base_info, TSDRV_SQ_ID, v_sq_id, sq_id);
}

int convert_stream_id(const vsq_base_info_t *vsq_base_info, u16 v_stream_id, u16 *stream_id)
{
    return convert_virt_to_phy(vsq_base_info, TSDRV_STREAM_ID, v_stream_id, stream_id);
}

int convert_event_id(const vsq_base_info_t *vsq_base_info, u16 v_event_id, u16 *event_id)
{
    return convert_virt_to_phy(vsq_base_info, TSDRV_EVENT_SW_ID, v_event_id, event_id);
}

int convert_notify_id(const vsq_base_info_t *vsq_base_info, u16 v_notify_id, u16 *notify_id)
{
    return convert_virt_to_phy(vsq_base_info, TSDRV_NOTIFY_ID, v_notify_id, notify_id);
}

int convert_model_id(const vsq_base_info_t *vsq_base_info, u16 v_model_id, u16 *model_id)
{
    return convert_virt_to_phy(vsq_base_info, TSDRV_MODEL_ID, v_model_id, model_id);
}

int fill_vsq_info(vsq_base_info_t *vsq_base_info)
{
    struct hvtsdrv_vsq_info vsq_info = {0};
    int ret;
    ret = hvtsdrv_get_vsq_info(vsq_base_info->dev_id, vsq_base_info->vf_id,
                               vsq_base_info->ts_id, vsq_base_info->vsq_id, &vsq_info);
    if (ret != 0) {
        ts_agent_err("Driver get vsq info failed, ret=%d, dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
                     ret, vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return ret;
    }
    ts_agent_debug("Driver get vsq info end, dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u,"
                   " vsq_base_addr=%pk, vsq_dep=%u, vsq_slot_size=%u.",
                   vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id,
                   vsq_info.vsq_base_addr, vsq_info.vsq_dep, vsq_info.vsq_slot_size);
    if (vsq_info.vsq_base_addr == NULL) {
        ts_agent_err("Driver get vsq info vsq_base_addr is null, dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
                     vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return EINVAL;
    }
    if (vsq_info.vsq_dep == 0) {
        ts_agent_err("Driver get vsq info vsq_dep is 0, dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
                     vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return EINVAL;
    }
    if (vsq_info.vsq_slot_size < TS_TASK_COMMAND_SIZE || vsq_info.vsq_slot_size > TS_AGENT_MAX_VSQ_SLOT_SIZE) {
        ts_agent_err("Driver get vsq info vsq_slot_size is out of range [%u, %u], "
                     "dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
                     TS_TASK_COMMAND_SIZE, TS_AGENT_MAX_VSQ_SLOT_SIZE,
                     vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return EINVAL;
    }
    vsq_base_info->vsq_base_addr = vsq_info.vsq_base_addr;
    vsq_base_info->vsq_dep = vsq_info.vsq_dep;
    vsq_base_info->vsq_slot_size = vsq_info.vsq_slot_size;
    vsq_base_info->vsq_type = NORMAL_VSQCQ_TYPE;     // default is normal sq.
    return 0;
}

int get_vsq_head_and_tail(const vsq_base_info_t *vsq_base_info, u32 *head, u32 *tail)
{
    struct hvtsdrv_vsq_head_tail head_tail = {0};
    int ret;
    ret = hvtsdrv_get_vsq_head_and_tail(vsq_base_info->dev_id, vsq_base_info->vf_id,
                                        vsq_base_info->ts_id, vsq_base_info->vsq_id, &head_tail);
    if (ret != 0) {
        ts_agent_err("Driver get vsq head and tail failed, ret=%d, dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
                     ret, vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return ret;
    }
    if (head_tail.head >= vsq_base_info->vsq_dep) {
        ts_agent_err("Driver get vsq head=%u is not less than vsq_dep=%u, "
                     "dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
                     head_tail.head, vsq_base_info->vsq_dep,
                     vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return EINVAL;
    }

    if (head_tail.tail >= vsq_base_info->vsq_dep) {
        ts_agent_err("Driver get vsq tail=%u is not less than vsq_dep=%u, "
                     "dev_id=%u, vf_id=%u, ts_id=%u, v_id=%u.",
                     head_tail.tail, vsq_base_info->vsq_dep,
                     vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
        return EINVAL;
    }
    *head = head_tail.head;
    *tail = head_tail.tail;
    ts_agent_debug("get vsq head and tail end, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, head=%u, tail=%u.",
                   vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id,
                   *head, *tail);
    return 0;
}
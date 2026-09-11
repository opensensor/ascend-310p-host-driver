/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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
#include <securec.h>

#include "devdrv_common.h"
#include "devdrv_id.h"
#include "devdrv_devinit.h"
#include "tsdrv_common.h"
#include "tsdrv_log.h"
#include "tsdrv_drvops.h"
#include "tsdrv_device.h"
#include "tsdrv_hwinfo.h"
#include "tsdrv_id.h"
#include "tsdrv_sync.h"
#include "devdrv_cqsq.h"
#include "tsdrv_get_ssid.h"
#include "devdrv_cb.h"
#include "hvtsdrv_id_event.h"
#include "devdrv_manager_comm.h"
STATIC int (*tsdrv_send_sync_msg)(u32 devid, void *tx, size_t tx_size) = NULL;

bool tsdrv_set_id_sync_non_idle(struct tsdrv_ts_resource *ts_res, enum tsdrv_id_type id_type, u32 status)
{
    u32 devid;

    devid = tsdrv_get_devid_by_res(ts_res);
    if (ka_base_atomic_cmpxchg(&ts_res->id_res[id_type].id_sync_msg_status,
        ID_SYNC_MSG_IDLE, status) != ID_SYNC_MSG_IDLE) {
        TSDRV_PRINT_ERR("Set id sync status failed. (devid=%u; id_type=%u; cur_status=%u; set_status=%u)\n",
            devid, id_type, ka_base_atomic_read(&ts_res->id_res[id_type].id_sync_msg_status), status);
        return false;
    }

    return true;
}

void tsdrv_set_id_sync_idle(struct tsdrv_ts_resource *ts_res, enum tsdrv_id_type id_type)
{
    ka_base_atomic_set(&ts_res->id_res[id_type].id_sync_msg_status, ID_SYNC_MSG_IDLE);
}

s32 tsdrv_sync_msg_send(u32 dev_id, struct tsdrv_msg_info *msg_info)
{
#ifdef CFG_FEATURE_RC_MODE
    msg_info->header.result = 0;
    msg_info->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    return 0;
#else
    if (tsdrv_send_sync_msg == NULL) {
        TSDRV_PRINT_ERR("tsdrv_send_sync_msg is null.\n");
        return -EFAULT;
    }

    return tsdrv_send_sync_msg(dev_id, msg_info, sizeof(struct tsdrv_msg_info));
#endif
}
EXPORT_SYMBOL_UNRELEASE(tsdrv_sync_msg_send);

STATIC void tsdrv_construct_sync_data(struct tsdrv_msg_info *msg_info,
    struct tsdrv_msg_resource_id *msg_resource, u32 tsid, u32 msg_id, enum tsdrv_id_type id_type, u32 req_id_num)
{
    msg_info->header.msg_id = msg_id;
    msg_info->header.valid = TSDRV_MSG_SYNC_MAGIC;
    msg_info->header.result = TSDRV_MSG_INVALID_RESULT;
    msg_info->header.tsid = tsid;
    msg_resource->id_type = id_type;
    msg_resource->sync_type = MSG_CMD_TYPE_SYNC;
    msg_resource->req_id_num = req_id_num;
    msg_resource->ret_id_num = 0;
}

#ifdef CFG_FEATURE_CMO
s32 tsdrv_msg_alloc_sync_cmo(u32 dev_id, u32 tsid)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_cmo_num;

    int ret = tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_CMO_ID, &cur_cmo_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_cmo_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (cur_cmo_num >= DEVDRV_MAX_CMO_ID - 1) {
        TSDRV_PRINT_ERR("cur_cmo_num(%u), don't try to get cmo from opposite side, dev_id(%u).\n",
            cur_cmo_num, dev_id);
        return -ENODEV;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_CMO, TSDRV_CMO_ID,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device(%u) msg chan alloc cmo d2h failed, ret = %d, result = %u, valid = 0x%x.\n",
            dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
#endif
    }

    ret = tsdrv_add_cmo_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("add_cmo_msg_chan failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    ret = tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_CMO_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("inc cmo_id_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    return 0;
}

s32 tsdrv_sync_cmo_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    u32 cur_cmo_num;
    u32 tsid;
    int ret;

    tsid = msg->header.tsid;
    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC) {
        TSDRV_PRINT_ERR("devid %u invalid message from opposite.\n", devid);
        return -EINVAL;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    dev_msg_resource_id->ret_id_num = 0;
    if ((dev_msg_resource_id->id_type != TSDRV_CMO_ID) || (dev_msg_resource_id->req_id_num == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid %u invalid request from opposite, id_type = 0x%x, req_id_num = %u.\n",
            devid, (u32)dev_msg_resource_id->id_type, dev_msg_resource_id->req_id_num);
        return -EINVAL;
#endif
    }
    ret = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_CMO_ID, &cur_cmo_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_cmo_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = tsdrv_alloc_cmo_msg_chan(devid, tsid, dev_msg_resource_id);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if (cur_cmo_num < dev_msg_resource_id->ret_id_num) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid %u cur_cmo_num = %u, ret_id_num = %u.\n", devid, cur_cmo_num,
            dev_msg_resource_id->ret_id_num);
        return -EINVAL;
#endif
    }

    ret = tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_CMO_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("dec cmo_id_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    return 0;
}
#endif

s32 tsdrv_msg_alloc_sync_stream(u32 dev_id, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_stream_num;
    int ret;

    ret = tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_STREAM_ID, &cur_stream_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_stream_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (cur_stream_num >= ts_res->id_res[TSDRV_STREAM_ID].id_capacity) {
        TSDRV_PRINT_DEBUG("Don't try sync stream id. (devid=%u; cur_stream_num=%u; cap=%u)\n",
            dev_id, cur_stream_num, ts_res->id_res[TSDRV_STREAM_ID].id_capacity);
        return -EBUSY;
    }

    if (!tsdrv_set_id_sync_non_idle(ts_res, TSDRV_STREAM_ID, ID_SYNC_MSG_LOCAL_TO_REMOTE)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_STREAM, TSDRV_STREAM_ID,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_STREAM_ID);
#endif
        TSDRV_PRINT_ERR("Msg chan sync res failed. (devid=%u; tsid=%u; ret=%d, result=%u; valid=0x%x)\n",
            dev_id, tsid, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
    }

    if (dev_msg_resource_id->ret_id_num == 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_STREAM_ID);
        TSDRV_PRINT_DEBUG("Peer res id is not enough.\n");
        return -EBUSY;
    }

    ret = devdrv_add_stream_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_STREAM_ID);
#endif
        TSDRV_PRINT_ERR("add_stream_msg_chan failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    ret = tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_STREAM_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_STREAM_ID);
#endif
        TSDRV_PRINT_ERR("inc stream_id_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    tsdrv_set_id_sync_idle(ts_res, TSDRV_STREAM_ID);

    return 0;
}

s32 tsdrv_msg_alloc_sync_event(u32 dev_id, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_hw_event_num, cur_sw_event_num, capacity;
    u32 tmp_hw_event_num = 0;
    u32 tmp_sw_event_num = 0;
    int ret;
    u32 i;

    if (tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_HW_ID, &cur_hw_event_num) != 0) {
        return -EINVAL;
    }

    if (tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_SW_ID, &cur_sw_event_num) != 0) {
        return -EINVAL;
    }

    capacity = ts_res->id_res[TSDRV_EVENT_HW_ID].id_capacity + ts_res->id_res[TSDRV_EVENT_SW_ID].id_capacity;
    if ((cur_hw_event_num + cur_sw_event_num) >= capacity) {
        TSDRV_PRINT_DEBUG("Don't try sync event id. (devid=%u; cur_hw_event_num=%u; cur_sw_event_num=%u; cap=%u)\n",
            dev_id, cur_hw_event_num, cur_sw_event_num, capacity);
        return -EBUSY;
    }

    if (!tsdrv_set_id_sync_non_idle(ts_res, TSDRV_EVENT_SW_ID, ID_SYNC_MSG_LOCAL_TO_REMOTE)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_EVENT, TSDRV_EVENT_SW_ID,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_EVENT_SW_ID);
#endif
        TSDRV_PRINT_ERR("Msg chan sync res failed. (devid=%u; tsid=%u; ret=%d, result=%u; valid=0x%x)\n",
            dev_id, tsid, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
    }

    if (dev_msg_resource_id->ret_id_num == 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_EVENT_SW_ID);
        TSDRV_PRINT_DEBUG("Peer res id is not enough.\n");
        return -EBUSY;
    }

    ret = devdrv_add_event_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_EVENT_SW_ID);
#endif
        TSDRV_PRINT_ERR("inc add_event_msg_chan failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    for (i = 0; i < dev_msg_resource_id->ret_id_num; i++) {
        if (tsdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_PF) {
            if (dev_msg_resource_id->id[i] >= DEVDRV_MAX_HW_EVENT_ID) {
                tmp_sw_event_num++;
            } else {
                tmp_hw_event_num++;
            }
        } else {
            tmp_sw_event_num++;
        }
    }

    if (tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_HW_ID, tmp_hw_event_num) != 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_EVENT_SW_ID);
#endif
        return -EINVAL;
    }
    if (tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_SW_ID, tmp_sw_event_num) != 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_EVENT_SW_ID);
#endif
        return -EINVAL;
    }

    tsdrv_set_id_sync_idle(ts_res, TSDRV_EVENT_SW_ID);

    return 0;
}

STATIC s32 tsdrv_msg_alloc_sync_id_l(u32 dev_id, u32 tsid, enum tsdrv_id_type id_type)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_dev_resource *pm_dev_res = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_hw_id_num = 0;
    u32 cur_sw_id_num = 0;
    u32 tmp_hw_id_num = 0;
    u32 tmp_sw_id_num = 0;
    u32 max_cnt;
    int ret;
    u32 i;

    pm_dev_res = tsdrv_get_dev_resource(dev_id, TSDRV_PM_FID);
    if (pm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", dev_id, TSDRV_PM_FID);
        return -EINVAL;
    }

    if (tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_HW_ID, &cur_hw_id_num) != 0) {
        TSDRV_PRINT_DEBUG("hw(%u)\n", cur_hw_id_num);
        return -EINVAL;
    }

    if (tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_SW_ID, &cur_sw_id_num) != 0) {
        TSDRV_PRINT_DEBUG("sw(%u)\n", cur_sw_id_num);
        return -EINVAL;
    }

    max_cnt = pm_dev_res->ts_resource[tsid].id_res[TSDRV_EVENT_HW_ID].id_capacity +
        pm_dev_res->ts_resource[tsid].id_res[TSDRV_EVENT_SW_ID].id_capacity;
    if ((cur_hw_id_num + cur_sw_id_num) >= max_cnt) {
        return -EAGAIN;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_RES_ID, id_type,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC) ||
        (dev_msg_resource_id->ret_id_num == 0)) {
        TSDRV_PRINT_ERR("device(%u) msg chan alloc failed, ret=%d, result=%u, valid=0x%x. id_type=%d ret_num=%u\n",
            dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid, (int)id_type,
            dev_msg_resource_id->ret_id_num);
        return -EFAULT;
    }

    ret = tsdrv_get_drv_ops()->add_id_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
        TSDRV_PRINT_ERR("inc add_id_msg_chan failed, ret(%d),id_type(%d).\n", ret, (int)id_type);
        return -EINVAL;
    }

    for (i = 0; i < dev_msg_resource_id->ret_id_num; i++) {
        if (dev_msg_resource_id->id[i] >= DEVDRV_MAX_HW_EVENT_ID) {
            tmp_sw_id_num++;
        } else {
            tmp_hw_id_num++;
        }
    }

    if (tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_HW_ID, tmp_hw_id_num) != 0) {
        return -EINVAL;
    }
    if (tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_EVENT_SW_ID, tmp_sw_id_num) != 0) {
        return -EINVAL;
    }

    return 0;
}
s32 tsdrv_msg_sync_id_map(u32 dev_id, struct tsdrv_id_map_sync_info id_map)
{
    struct tsdrv_msg_info msg_info = {{0}};
    s32 ret;

    msg_info.header.msg_id = TSDRV_CHAN_SYNC_ID_MAP;
    msg_info.header.valid = TSDRV_MSG_SYNC_MAGIC;
    msg_info.header.result = TSDRV_MSG_INVALID_RESULT;
    msg_info.header.tsid = id_map.event.tsid;
    ret = memcpy_s(msg_info.payload, TSDRV_MSG_INFO_PAYLOAD_LEN, &id_map, sizeof(struct tsdrv_id_map_sync_info));
#ifndef TSDRV_UT
    if (ret != 0) {
        TSDRV_PRINT_ERR("memcpy failed. (ret=%d)\n", ret);
        return -EFAULT;
    }
#endif
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device(%u) sync failed, ret=%d, result=%u, valid=0x%x. id_type=%d\n",
            dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid, (int)id_map.id_type);
        return -EFAULT;
#endif
    }

    return 0;
}

s32 tsdrv_msg_alloc_sync_id(u32 dev_id, u32 tsid, enum tsdrv_id_type id_type)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_dev_resource *pm_dev_res = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_id_num;
    int ret;

    pm_dev_res = tsdrv_get_dev_resource(dev_id, TSDRV_PM_FID);
    if (pm_dev_res == NULL) {
        TSDRV_PRINT_ERR("get dev resource failed, devid(%u) fid(%u).\n", dev_id, TSDRV_PM_FID);
        return -EINVAL;
    }

    if ((id_type == TSDRV_EVENT_HW_ID) || (id_type == TSDRV_EVENT_SW_ID)) {
        ret = tsdrv_msg_alloc_sync_id_l(dev_id, tsid, id_type);
        if (ret != 0) {
            TSDRV_PRINT_ERR("id sync failed, ret(%d),id_type(%d).\n", ret, (int)id_type);
            return -EINVAL;
        }
    } else {
        if (tsdrv_get_ts_id_cur_num(dev_id, tsid, id_type, &cur_id_num) != 0) {
            return -EINVAL;
        }
        if (cur_id_num >= pm_dev_res->ts_resource[tsid].id_res[id_type].id_capacity) {
            return -EAGAIN;
        }

        dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
        tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_RES_ID, id_type,
            TSDRV_MSG_GET_ID_NUM);
        ret = tsdrv_sync_msg_send(dev_id, &msg_info);
        if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC) ||
            (dev_msg_resource_id->ret_id_num == 0)) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("device(%u) sync failed, ret=%d, result=%u, valid=0x%x. id_type=%d ret_num=%u\n",
                dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid, (int)id_type,
                dev_msg_resource_id->ret_id_num);
            return -EFAULT;
#endif
        }

        ret = tsdrv_get_drv_ops()->add_id_msg_chan(dev_id, tsid, dev_msg_resource_id);
        if (ret != 0) {
            TSDRV_PRINT_ERR("inc add_id_msg_chan failed, ret(%d),id_type(%d).\n", ret, (int)id_type);
            return -EINVAL;
        }

        if (tsdrv_inc_ts_id_cur_num(dev_id, tsid, id_type, dev_msg_resource_id->ret_id_num) != 0) {
            return -EINVAL;
        }
    }

    return 0;
}

s32 tsdrv_msg_alloc_sync_sq(u32 dev_id, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_sq_num;
    int ret;

    ret = tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_SQ_ID, &cur_sq_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_sq_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (!tsdrv_set_id_sync_non_idle(ts_res, TSDRV_SQ_ID, ID_SYNC_MSG_LOCAL_TO_REMOTE)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_SQ, TSDRV_SQ_ID,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_SQ_ID);
        TSDRV_PRINT_ERR("device(%u) msg chan alloc sq failed, ret = %d, result = %u, valid = 0x%x.\n",
            dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
#endif
    }

    ret = devdrv_add_sq_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_SQ_ID);
        TSDRV_PRINT_ERR("add_sq_msg_chan failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    ret = tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_SQ_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_SQ_ID);
        TSDRV_PRINT_ERR("inc sq_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    tsdrv_set_id_sync_idle(ts_res, TSDRV_SQ_ID);

    return 0;
}

s32 tsdrv_msg_alloc_sync_cq(u32 dev_id, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_cq_num;

    int ret = tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_CQ_ID, &cur_cq_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_cq_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (cur_cq_num >= ts_res->id_res[TSDRV_CQ_ID].id_capacity) {
        TSDRV_PRINT_INFO("Don't try to get CQ from opposite side. (devid=%u; cur_cq_num=%u; capaicty=%u)\n",
            dev_id, cur_cq_num, ts_res->id_res[TSDRV_CQ_ID].id_capacity);
        return -ENODEV;
    }

    if (!tsdrv_set_id_sync_non_idle(ts_res, TSDRV_CQ_ID, ID_SYNC_MSG_LOCAL_TO_REMOTE)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_CQ, TSDRV_CQ_ID,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_CQ_ID);
        TSDRV_PRINT_ERR("device(%u) msg chan alloc cq failed, ret = %d, result = %u, valid = 0x%x.\n",
            dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
#endif
    }
    if (dev_msg_resource_id->ret_id_num == 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_CQ_ID);
        TSDRV_PRINT_ERR("Sync cq fail, (ret_cq_num=%u; cur_cq_num=%u)\n", dev_msg_resource_id->ret_id_num, cur_cq_num);
        return -ENODEV;
#endif
    }
    ret = devdrv_add_cq_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_CQ_ID);
        TSDRV_PRINT_ERR("add_cq_msg_chan failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    ret = tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_CQ_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_CQ_ID);
        TSDRV_PRINT_ERR("inc cq_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    tsdrv_set_id_sync_idle(ts_res, TSDRV_CQ_ID);
    return 0;
}

s32 tsdrv_msg_alloc_sync_model(u32 dev_id, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_model_num;
    int ret;

    ret = tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_MODEL_ID, &cur_model_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_model_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (cur_model_num >= ts_res->id_res[TSDRV_MODEL_ID].id_capacity) {
        TSDRV_PRINT_DEBUG("Don't try sync model id. (devid=%u; cur_model_num=%u; cap=%u)\n",
            dev_id, cur_model_num, ts_res->id_res[TSDRV_MODEL_ID].id_capacity);
        return -EBUSY;
    }

    if (!tsdrv_set_id_sync_non_idle(ts_res, TSDRV_MODEL_ID, ID_SYNC_MSG_LOCAL_TO_REMOTE)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_MODEL, TSDRV_MODEL_ID,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_MODEL_ID);
        TSDRV_PRINT_ERR("Msg chan sync res failed. (devid=%u; tsid=%u; ret=%d, result=%u; valid=0x%x)\n",
            dev_id, tsid, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
    }

    if (dev_msg_resource_id->ret_id_num == 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_MODEL_ID);
        TSDRV_PRINT_DEBUG("Peer res id is not enough.\n");
        return -EBUSY;
    }

    ret = devdrv_add_model_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_MODEL_ID);
        TSDRV_PRINT_ERR("add_model_msg_chan failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    ret = tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_MODEL_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_MODEL_ID);
        TSDRV_PRINT_ERR("inc model_id_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    tsdrv_set_id_sync_idle(ts_res, TSDRV_MODEL_ID);
    return 0;
}

s32 tsdrv_msg_alloc_sync_notify(u32 dev_id, u32 tsid)
{
    struct tsdrv_ts_resource *ts_res = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    struct tsdrv_msg_info msg_info = {{0}};
    u32 cur_notify_num;

    int ret = tsdrv_get_ts_id_cur_num(dev_id, tsid, TSDRV_NOTIFY_ID, &cur_notify_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_notify_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (cur_notify_num >= ts_res->id_res[TSDRV_NOTIFY_ID].id_capacity) {
        TSDRV_PRINT_DEBUG("Don't try sync notify id. (devid=%u; cur_notify_num=%u; cap=%u)\n",
            dev_id, cur_notify_num, ts_res->id_res[TSDRV_NOTIFY_ID].id_capacity);
        return -EBUSY;
    }

    if (!tsdrv_set_id_sync_non_idle(ts_res, TSDRV_NOTIFY_ID, ID_SYNC_MSG_LOCAL_TO_REMOTE)) {
#ifndef TSDRV_UT
        return -EPERM;
#endif
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg_info.payload;
    tsdrv_construct_sync_data(&msg_info, dev_msg_resource_id, tsid, TSDRV_CHAN_SYNC_NOTIFY, TSDRV_NOTIFY_ID,
        TSDRV_MSG_GET_ID_NUM);
    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_NOTIFY_ID);
#endif
        TSDRV_PRINT_ERR("Msg chan sync res failed. (devid=%u; tsid=%u; ret=%d, result=%u; valid=0x%x)\n",
            dev_id, tsid, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
    }

    if (dev_msg_resource_id->ret_id_num == 0) {
        tsdrv_set_id_sync_idle(ts_res, TSDRV_NOTIFY_ID);
        TSDRV_PRINT_DEBUG("Peer res id is not enough.\n");
        return -EBUSY;
    }

    ret = tsdrv_get_drv_ops()->add_notify_msg_chan(dev_id, tsid, dev_msg_resource_id);
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_NOTIFY_ID);
#endif
        TSDRV_PRINT_ERR("add_notify_msg_chan failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    ret = tsdrv_inc_ts_id_cur_num(dev_id, tsid, TSDRV_NOTIFY_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
#ifndef TSDRV_UT
        tsdrv_set_id_sync_idle(ts_res, TSDRV_NOTIFY_ID);
#endif
        TSDRV_PRINT_ERR("inc notify_id_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    tsdrv_set_id_sync_idle(ts_res, TSDRV_NOTIFY_ID);
    return 0;
}

int tsdrv_msg_phy_cbcqsq_init(u32 dev_id, u32 tsid)
{
    struct tsdrv_msg_info msg_head;
    int ret;

    msg_head.header.msg_id = TSDRV_CHAN_SYNC_PHY_CBCQSQ_INIT;
    msg_head.header.valid = TSDRV_MSG_SYNC_MAGIC;
    msg_head.header.result = TSDRV_MSG_INVALID_RESULT;
    msg_head.header.tsid = tsid;

    ret = tsdrv_sync_msg_send(dev_id, &msg_head);
    if ((ret != 0) || (msg_head.header.result != 0) || (msg_head.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
        TSDRV_PRINT_ERR("Failed to init offline physic callback sqcq. (devid=%u; ret=%d; result=%u; valid=0x%x)\n",
            dev_id, ret, (u32)msg_head.header.result, (u32)msg_head.header.valid);
        return -EFAULT;
    }

    return 0;
}

s32 tsdrv_msg_phy_cbcqsq_exit(u32 dev_id, u32 tsid)
{
    struct tsdrv_msg_info msg_head;
    int ret;

    msg_head.header.msg_id = TSDRV_CHAN_SYNC_PHY_CBCQSQ_EXIT;
    msg_head.header.valid = TSDRV_MSG_SYNC_MAGIC;
    msg_head.header.result = TSDRV_MSG_INVALID_RESULT;
    msg_head.header.tsid = tsid;

    ret = tsdrv_sync_msg_send(dev_id, &msg_head);
    if ((ret != 0) || (msg_head.header.result != 0) || (msg_head.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
        TSDRV_PRINT_ERR("device(%u) msg chan send failed, ret = %d, result = %u, valid = 0x%x.\n",
            dev_id, ret, (u32)msg_head.header.result, (u32)msg_head.header.valid);
        return -EFAULT;
    }

    return 0;
}

static int tsdrv_sync_data_check(u32 devid, u32 tsid, struct tsdrv_msg_info *msg)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;

    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC) {
        TSDRV_PRINT_ERR("Invalid message from opposite. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM) || (dev_msg_resource_id == NULL) ||
        (dev_msg_resource_id->req_id_num > DEVDRV_MANAGER_MSG_ID_NUM_MAX) || (dev_msg_resource_id->req_id_num == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid input parameter. (devid=%u; tsid=%u)\n", devid, tsid);
#endif
        return -EINVAL;
    }

    return 0;
}

s32 tsdrv_sync_stream_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    u32 tsid = msg->header.tsid;
    int ret;

    ret = tsdrv_sync_data_check(devid, tsid, msg);
    if (ret != 0) {
        return ret;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    if ((dev_msg_resource_id->id_type != TSDRV_STREAM_ID)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Unmatched id type from opposite. (devid=%u; id_type=%d;)\n",
            devid, dev_msg_resource_id->id_type);
#endif
        return -EINVAL;
    }

    ret = devdrv_alloc_stream_msg_chan(devid, tsid, dev_msg_resource_id);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("Sync no success. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return 0;
#endif
    }

    ret = tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_STREAM_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Dec cur stream id num failed. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return -EINVAL;
    }

    return 0;
}

s32 tsdrv_sync_sq_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    u32 cur_sq_num;
    u32 tsid = 0;
    int ret;

    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC) {
        TSDRV_PRINT_ERR("devid %u invalid message from opposite.\n", devid);
        return -EINVAL;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    dev_msg_resource_id->ret_id_num = 0;
    if ((dev_msg_resource_id->id_type != TSDRV_SQ_ID) || (dev_msg_resource_id->req_id_num == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid %u invalid request from opposite, id_type = 0x%x, req_id_num = %u\n",
            devid, (u32)dev_msg_resource_id->id_type, dev_msg_resource_id->req_id_num);
        return -EINVAL;
#endif
    }

    ret = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_SQ_ID, &cur_sq_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_sq_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = devdrv_alloc_sq_msg_chan(devid, tsid, dev_msg_resource_id);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if (cur_sq_num < dev_msg_resource_id->ret_id_num) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid %u cur_sq = %u, ret_id = %u.\n", devid, cur_sq_num, dev_msg_resource_id->ret_id_num);
        return -EINVAL;
#endif
    }
    ret = tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_SQ_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("dec event_id_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    return 0;
}

s32 tsdrv_sync_cq_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    u32 cur_cq_num;
    u32 tsid = 0;
    int ret;

    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC) {
        TSDRV_PRINT_ERR("devid %u invalid message from opposite.\n", devid);
        return -EINVAL;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    dev_msg_resource_id->ret_id_num = 0;
    if ((dev_msg_resource_id->id_type != TSDRV_CQ_ID) || (dev_msg_resource_id->req_id_num == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid %u invalid request from opposite, id_type = 0x%x, req_id_num = %u.\n",
            devid, (u32)dev_msg_resource_id->id_type, dev_msg_resource_id->req_id_num);
        return -EINVAL;
#endif
    }

    ret = tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_CQ_ID, &cur_cq_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("get cur_cq_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = devdrv_alloc_cq_msg_chan(devid, tsid, dev_msg_resource_id);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if (cur_cq_num < dev_msg_resource_id->ret_id_num) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("cur_cq_num = %u, ret_id_num = %u.\n", cur_cq_num, dev_msg_resource_id->ret_id_num);
        return -EINVAL;
#endif
    }

    ret = tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_CQ_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("dec cq_id_num failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    return 0;
}

s32 tsdrv_sync_event_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    u32 tsid = msg->header.tsid;
    u32 sync_hw_event_num, sync_sw_event_num;
    int ret;

    ret = tsdrv_sync_data_check(devid, tsid, msg);
    if (ret != 0) {
        return ret;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    if ((dev_msg_resource_id->id_type != TSDRV_EVENT_SW_ID) || (dev_msg_resource_id->req_id_num == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Unmatched id type from opposite. (devid=%u; id_type=%d)\n",
            devid, dev_msg_resource_id->id_type);
#endif
        return -EINVAL;
    }

    ret = devdrv_alloc_event_msg_chan(devid, tsid, dev_msg_resource_id, &sync_hw_event_num, &sync_sw_event_num);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("Sync no success. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return 0;
#endif
    }

    ret = tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_EVENT_HW_ID, sync_hw_event_num);
    ret |= tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_EVENT_SW_ID, sync_sw_event_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Dec cur event id num failed. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return -EINVAL;
    }

    return 0;
}

s32 tsdrv_sync_res_id_proc_l(u32 devid, u32 tsid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    u32 cur_id_hw_num, cur_id_sw_num;
    u32 tmp_id_hw_num = 0;
    u32 tmp_id_sw_num = 0;
    int ret;
    u32 i;

    if ((tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_SW_ID, &cur_id_sw_num) != 0) ||
        (tsdrv_get_ts_id_cur_num(devid, tsid, TSDRV_EVENT_HW_ID, &cur_id_hw_num) != 0)) {
        return -EINVAL;
    }

    ret = devdrv_alloc_id_msg_chan(devid, tsid, dev_msg_resource_id);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if ((cur_id_hw_num + cur_id_sw_num) < dev_msg_resource_id->ret_id_num) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid(%u) cur_event_num hw(%u) sw(%u),ret_id_num(%u)\n",
            devid, cur_id_hw_num, cur_id_sw_num, dev_msg_resource_id->ret_id_num);
        return -EINVAL;
#endif
    }
    for (i = 0; i < dev_msg_resource_id->ret_id_num; i++) {
        if (dev_msg_resource_id->id[i] >= DEVDRV_MAX_HW_EVENT_ID) {
            tmp_id_sw_num++;
        } else {
            tmp_id_hw_num++;
        }
    }
    if (tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_EVENT_HW_ID, tmp_id_hw_num) != 0) {
        return -EINVAL;
    }
    if (tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_EVENT_SW_ID, tmp_id_sw_num) != 0) {
        return -EINVAL;
    }
    return 0;
}

s32 tsdrv_sync_res_id_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    u32 cur_id_num;
    enum tsdrv_id_type id_type;
    u32 tsid;
    int ret;

    tsid = msg->header.tsid;
    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC) {
        TSDRV_PRINT_ERR("devid(%u) invalid message from opposite.\n", devid);
        return -EINVAL;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    dev_msg_resource_id->ret_id_num = 0;
    id_type = dev_msg_resource_id->id_type;
    if ((id_type >= TSDRV_MAX_ID) || (dev_msg_resource_id->req_id_num == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid(%u) invalid request from opposite, id_type(0x%x), req_id_num(%u).\n",
            devid, (u32)dev_msg_resource_id->id_type, dev_msg_resource_id->req_id_num);
        return -EINVAL;
#endif
    }

    if ((id_type == TSDRV_EVENT_HW_ID) || (id_type == TSDRV_EVENT_SW_ID)) {
        ret = tsdrv_sync_res_id_proc_l(devid, tsid, msg, ack_len);
        if (ret != 0) {
            TSDRV_PRINT_ERR("id sync failed, ret(%d) id_type(%d).\n", ret, (int)id_type);
            return -EINVAL;
        }
    } else {
        ret = tsdrv_get_ts_id_cur_num(devid, tsid, id_type, &cur_id_num);
        if (ret != 0) {
            TSDRV_PRINT_ERR("get cur_num failed, ret(%d) id_type(%d).\n", ret, (int)id_type);
            return -EINVAL;
        }

        ret = devdrv_alloc_id_msg_chan(devid, tsid, dev_msg_resource_id);
        msg->header.result = ret;
        msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
        *ack_len = sizeof(struct tsdrv_msg_info);
        if (cur_id_num < dev_msg_resource_id->ret_id_num) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("devid(%u) cur_num(%u) id_type(%d),ret_id_num(%u)\n",
                devid, cur_id_num, (int)id_type, dev_msg_resource_id->ret_id_num);
            return -EINVAL;
#endif
        }

        if (tsdrv_dec_ts_id_cur_num(devid, tsid, id_type, dev_msg_resource_id->ret_id_num) != 0) {
            return -EINVAL;
        }
    }
    return 0;
}

s32 tsdrv_sync_model_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    u32 tsid = msg->header.tsid;
    int ret;

    ret = tsdrv_sync_data_check(devid, tsid, msg);
    if (ret != 0) {
        return ret;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    if ((dev_msg_resource_id->id_type != TSDRV_MODEL_ID)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Unmatched id type from opposite. (devid=%u; id_type=%d;)\n",
            devid, dev_msg_resource_id->id_type);
#endif
        return -EINVAL;
    }

    ret = devdrv_alloc_model_msg_chan(devid, tsid, dev_msg_resource_id);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("Sync no success. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return 0;
#endif
    }

    ret = tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_MODEL_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Dec cur model id num failed. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return -EINVAL;
    }

    TSDRV_PRINT_DEBUG("Sync success. (devid=%u; tsid=%u)\n", devid, tsid);

    return 0;
}

s32 tsdrv_sync_notify_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_msg_resource_id *dev_msg_resource_id = NULL;
    u32 tsid = msg->header.tsid;
    int ret;

    ret = tsdrv_sync_data_check(devid, tsid, msg);
    if (ret != 0) {
        return ret;
    }

    dev_msg_resource_id = (struct tsdrv_msg_resource_id *)msg->payload;
    if ((dev_msg_resource_id->id_type != TSDRV_NOTIFY_ID)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Unmatched id type from opposite. (devid=%u; id_type=%d;)\n",
            devid, dev_msg_resource_id->id_type);
#endif
        return -EINVAL;
    }

    ret = devdrv_alloc_notify_msg_chan(devid, tsid, dev_msg_resource_id);
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_DEBUG("Sync no success. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return 0;
#endif
    }

    ret = tsdrv_dec_ts_id_cur_num(devid, tsid, TSDRV_NOTIFY_ID, dev_msg_resource_id->ret_id_num);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Dec cur notify id num failed. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return -EINVAL;
    }

    return 0;
}

s32 tsdrv_sync_stream_sqcq(u32 devid, struct tsdrv_msg_info *msg_head, u32 *ack_len)
{
    msg_head->header.result = 0;
    msg_head->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    return 0;
}

STATIC int tsdrv_sync_phy_cbcqsq_init(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    int ret;

    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC || msg->header.tsid >= DEVDRV_MAX_TS_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("devid %u invalid message from opposite, valid = 0x%x tsid = %u.\n",
            devid, (u32)msg->header.valid, msg->header.tsid);
        return -EFAULT;
#endif
    }

    if (tsdrv_get_env_type() != TSDRV_ENV_OFFLINE) {
#ifndef TSDRV_UT
        TSDRV_PRINT_WARN("D2H not support init phy cbcqsq. (devid=%u)\n", devid);
        return -EINVAL;
#endif
    }

    ret = callback_physic_sqcq_init(tsdrv_get_dev(devid), TSDRV_PM_FID, msg->header.tsid);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to init callback physic sqcq. (devid=%u; ret=%d)\n", devid, ret);
#endif
    }

    msg->header.result = (u16)ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    return 0;
}

s32 tsdrv_sync_phy_cbcqsq_exit(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC || msg->header.tsid >= DEVDRV_MAX_TS_NUM) {
        TSDRV_PRINT_ERR("devid %u invalid message from opposite, valid = 0x%x tsid = %u.\n",
            devid, (u32)msg->header.valid, msg->header.tsid);
        return -EFAULT;
    }

    if (tsdrv_get_env_type() != TSDRV_ENV_OFFLINE) {
        TSDRV_PRINT_WARN("d2h no support destroy phy cbcqsq, devid %u.\n", devid);
        return -EINVAL;
    }

    callback_physic_sqcq_exit(tsdrv_get_dev(devid), TSDRV_PM_FID, msg->header.tsid);

    msg->header.result = 0;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    return 0;
}

#ifdef CFG_FEATURE_ID_MAPING
s32 tsdrv_sync_id_map_proc(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
    struct tsdrv_id_map_sync_info *id_map = NULL;
    int ret;

    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC) {
        TSDRV_PRINT_ERR("devid %u invalid message from opposite.\n", devid);
        return -EINVAL;
    }

    id_map = (struct tsdrv_id_map_sync_info *)msg->payload;
    if ((id_map->id_type == TSDRV_EVENT_SW_ID)) {
        ret = tsdrv_event_id_maping(devid, &id_map->event);
    } else {
        TSDRV_PRINT_ERR("devid %u invalid request from opposite, id_type = 0x%x\n",
            devid, (u32)id_map->id_type);
        ret = -EINVAL;
    }
    msg->header.result = ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
    return 0;
}
#endif

s32 tsdrv_msg_check_res_id(u32 dev_id, u32 tsid, u32 id_type, u32 id)
{
#ifndef TSDRV_UT
    struct tsdrv_msg_info msg_info;
    struct tsdrv_msg_res_id_check *info = (struct tsdrv_msg_res_id_check *)msg_info.payload;
    int ret;

    msg_info.header.msg_id = TSDRV_CHAN_SYNC_ID_CHECK;
    msg_info.header.valid = TSDRV_MSG_SYNC_MAGIC;
    msg_info.header.result = TSDRV_MSG_INVALID_RESULT;
    msg_info.header.tsid = tsid;
    info->hpid = (u32)current->tgid;
    info->id_type = (int)id_type;
    info->res_id = id;

    ret = tsdrv_sync_msg_send(dev_id, &msg_info);
    if ((ret != 0) || (msg_info.header.result != 0) || (msg_info.header.valid != TSDRV_MSG_SYNC_RSP_MAGIC)) {
        TSDRV_PRINT_ERR("Msg chan alloc notify d2h failed. (device=%u; ret=%d, result=%u, valid=0x%x).\n",
            dev_id, ret, (u32)msg_info.header.result, (u32)msg_info.header.valid);
        return -EFAULT;
    }
#endif
    return 0;
}

static s32 tsdrv_sync_id_check(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len)
{
#ifndef TSDRV_UT
    struct tsdrv_msg_res_id_check *info = (struct tsdrv_msg_res_id_check *)msg->payload;
    struct tsdrv_drv_ops *ops = tsdrv_get_drv_ops();
    struct tsdrv_id_inst id_inst;
    u32 tsid;
    int ret, pid;

    tsid = msg->header.tsid;
    if (msg->header.valid != TSDRV_MSG_SYNC_MAGIC) {
        TSDRV_PRINT_ERR("devid(%u) invalid message from opposite.\n", devid);
        return -EINVAL;
    }

    if ((info->id_type != TSDRV_EVENT_HW_ID) && (info->id_type != TSDRV_NOTIFY_ID)) {
        TSDRV_PRINT_ERR("devid(%u) invalid request from opposite. (tsid=%u; id_type=%u; res_id=%u).\n",
            devid, tsid, info->id_type, info->res_id);
        return -EINVAL;
    }

    if (ops->cp_proc_query_by_hpid != NULL) {
        ret = ops->cp_proc_query_by_hpid(info->hpid, devid, DEVDRV_PROCESS_CP1, 0, &pid);
        if (ret != 0) {
            TSDRV_PRINT_ERR("Get pid fail. (hpid=%u; devid=%u; vfid=%u; ret=%d)\n", info->hpid, devid, 0, ret);
            return ret;
        }
    }

    tsdrv_pack_id_inst(devid, TSDRV_PM_FID, tsid, &id_inst);
    ret = tsdrv_id_is_belong_to_proc(&id_inst, pid, info->res_id, info->id_type);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Id is not belong to this proc. (devid=%u; tsid=%u; hpid=%u; pid=%d;"
            "id_type=%u; id=%u; ret=%d)\n", id_inst.devid, id_inst.tsid, info->hpid, pid,
            info->res_id, info->res_id, ret);
        return ret;
    }

    msg->header.result = (u16)ret;
    msg->header.valid = TSDRV_MSG_SYNC_RSP_MAGIC;
    *ack_len = sizeof(struct tsdrv_msg_info);
#endif
    return 0;
}

s32 (*const tsdrv_chan_msg_handlers[TSDRV_CHAN_SYNC_MAX])(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len) = {
    [TSDRV_CHAN_SYNC_SSID] = tsdrv_sync_ssid_proc,
    [TSDRV_CHAN_SYNC_RES_ID] = tsdrv_sync_res_id_proc,
    [TSDRV_CHAN_SYNC_STREAM] = tsdrv_sync_stream_proc,
    [TSDRV_CHAN_SYNC_SQ] = tsdrv_sync_sq_proc,
    [TSDRV_CHAN_SYNC_CQ] = tsdrv_sync_cq_proc,
    [TSDRV_CHAN_SYNC_EVENT] = tsdrv_sync_event_proc,
    [TSDRV_CHAN_SYNC_MODEL] = tsdrv_sync_model_proc,
#ifdef CFG_FEATURE_CMO
    [TSDRV_CHAN_SYNC_CMO] = tsdrv_sync_cmo_proc,
#endif
    [TSDRV_CHAN_SYNC_NOTIFY] = tsdrv_sync_notify_proc,
    [TSDRV_CHAN_SYNC_STREAM_SQ_CQ] = tsdrv_sync_stream_sqcq,
    [TSDRV_CHAN_SYNC_PHY_CBCQSQ_INIT] = tsdrv_sync_phy_cbcqsq_init,
    [TSDRV_CHAN_SYNC_PHY_CBCQSQ_EXIT] = tsdrv_sync_phy_cbcqsq_exit,
#ifdef CFG_FEATURE_ID_MAPING
    [TSDRV_CHAN_SYNC_ID_MAP] = tsdrv_sync_id_map_proc,
#endif
    [TSDRV_CHAN_SYNC_ID_CHECK] = tsdrv_sync_id_check,
};

s32 tsdrv_chan_msg_proc(u32 devid, void *msg, u32 msg_len, u32 *ack_len)
{
    struct tsdrv_msg_info *tsdrv_msg_info = NULL;
    u32 msg_id;

    if ((msg == NULL) || (ack_len == NULL) || (msg_len != sizeof(struct tsdrv_msg_info))) {
        TSDRV_PRINT_ERR("msg(%pK) or real_out_len(%pK) is NULL, msg_len(%u)\n", msg, ack_len, msg_len);
        return -EINVAL;
    }

    tsdrv_msg_info = (struct tsdrv_msg_info *)msg;
    msg_id = tsdrv_msg_info->header.msg_id;

    if (msg_id >= TSDRV_CHAN_SYNC_MAX) {
        TSDRV_PRINT_ERR("invalid msg_id=%u, devid=%u\n", msg_id, devid);
        return -EINVAL;
    }
    if (tsdrv_chan_msg_handlers[msg_id] == NULL) {
        TSDRV_PRINT_ERR("msg_id=%u handler is NULL, devid=%u\n", msg_id, devid);
        return -EFAULT;
    }
    return tsdrv_chan_msg_handlers[msg_id](devid, tsdrv_msg_info, ack_len);
}
KA_EXPORT_SYMBOL_GPL(tsdrv_chan_msg_proc);

void tsdrv_set_send_msg_proc(int (*sync_msg_send)(u32 devid, void *tx, size_t tx_size))
{
    tsdrv_send_sync_msg = sync_msg_send;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_set_send_msg_proc);

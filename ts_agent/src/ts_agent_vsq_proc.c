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

#include "ts_agent_vsq_proc.h"
#include "securec.h"
#include "tsch/task_struct.h"
#include "ts_agent_log.h"
#include "hvtsdrv_tsagent.h"
#include "virtmng_interface.h"
#include "ts_agent_resource.h"

typedef int (*task_convert_custom_fn_t)(const vsq_base_info_t *vsq_base_info, ts_task_t *task);

static task_convert_custom_fn_t g_task_convert_fn[TS_TASK_TYPE_RESERVED] = {NULL};

STATIC int convert_task_basic(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    int ret;
    uint16_t v_stream_id = task->streamID;
    ret = convert_stream_id(vsq_base_info, v_stream_id, &task->streamID);
    if (ret != 0) {
        ts_agent_err("convert stream id failed. v_stream_id=%u, ret=%d.", v_stream_id, ret);
        task->streamID = U16_MAX;
    } else {
        ts_agent_debug("convert v_stream_id=%u to stream_id=%u.", v_stream_id, task->streamID);
    }
    return ret;
}

STATIC int convert_event_record_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_event_record_task_t *event_record_task = &task->u.event_task;
    int ret;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_event_id = event_record_task->eventID;
    ret = convert_event_id(vsq_base_info, v_event_id, &event_record_task->eventID);
    if (ret != 0) {
        ts_agent_err("convert event id failed. v_stream_id=%u, ret=%d.", v_stream_id, ret);
        event_record_task->eventID = U16_MAX;
    } else {
        ts_agent_debug("convert v_event_id=%u to event_id=%u. v_stream_id=%u.",
                       v_event_id, event_record_task->eventID, v_stream_id);
    }
    return ret;
}

STATIC int convert_stream_wait_event_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_stream_wait_event_task_t *stream_wait_event_task = &task->u.stream_wait_event_task;
    int ret;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_event_id = stream_wait_event_task->eventID;
    ret = convert_event_id(vsq_base_info, v_event_id, &stream_wait_event_task->eventID);
    if (ret != 0) {
        ts_agent_err("convert event id failed. v_stream_id=%u, ret=%d.", v_stream_id, ret);
        stream_wait_event_task->eventID = U16_MAX;
    } else {
        ts_agent_debug("convert v_event_id=%u to event_id=%u. v_stream_id=%u.",
                       v_event_id, stream_wait_event_task->eventID, v_stream_id);
    }
    return ret;
}

STATIC int convert_maintenance_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_maintenance_task_t *maintenance_task = &task->u.maintenance_task;
    int ret;
    uint16_t v_stream_id = task->streamID;
    uint16_t target_id = maintenance_task->targetID;
    uint16_t goal = maintenance_task->goal;
    if ((goal == MAINTENANCE_GOAL_STREAM) || (goal == MAINTENANCE_GOAL_STREAM_TASK_RECYCLE)) {
        ret = convert_stream_id(vsq_base_info, target_id, &maintenance_task->targetID);
        if (ret != 0) {
            ts_agent_err("convert maintenance target (stream)id failed. v_stream_id=%u, target_id=%u, ret=%d.",
                         v_stream_id, target_id, ret);
            maintenance_task->targetID = U16_MAX;
        } else {
            maintenance_task->v_target_id = target_id;
            ts_agent_debug("convert v_stream_id=%u to event_id=%u. v_stream_id=%u.",
                target_id, maintenance_task->targetID, v_stream_id);
        }
    } else if (goal == MAINTENANCE_GOAL_EVENT) {
        ret = convert_event_id(vsq_base_info, target_id, &maintenance_task->targetID);
        if (ret != 0) {
            ts_agent_err("convert maintenance target (event)id failed. v_stream_id=%u, target_id=%u, ret=%d.",
                         v_stream_id, target_id, ret);
            maintenance_task->targetID = U16_MAX;
        } else {
            maintenance_task->v_target_id = target_id;
            ts_agent_debug("convert v_event_id=%u to event_id=%u. v_stream_id=%u.",
                target_id, maintenance_task->targetID, v_stream_id);
        }
    } else {
        ret = 0;
        // do nothing
        ts_agent_debug("task goal=%u no need convert. v_stream_id=%u.", goal, v_stream_id);
    }
    return ret;
}

static int convert_create_stream_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_create_stream_t *create_stream_task = &task->u.creat_stream;
    create_stream_task->vf_id = vsq_base_info->vf_id;
    create_stream_task->v_stream_id = task->streamID;
    ts_agent_debug("convert create stream task end. vf_id=%u, v_stream_id=%u.",
                   vsq_base_info->vf_id, task->streamID);
    return 0;
}

STATIC int convert_notify_wait_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_notify_wait_task_t *notify_wait_task = &task->u.notify_wait_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_notify_id = notify_wait_task->notify_id;
    int ret = convert_notify_id(vsq_base_info, v_notify_id, &notify_wait_task->notify_id);
    if (ret != 0) {
        ts_agent_err("convert notify id failed. v_stream_id=%u, v_notify_id=%u, ret=%d.",
                     v_stream_id, v_notify_id, ret);
        notify_wait_task->notify_id = U16_MAX;
    } else {
        ts_agent_debug("convert v_notify_id=%u to notify_id=%u. v_stream_id=%u.",
                       v_notify_id, notify_wait_task->notify_id, v_stream_id);
    }
    return ret;
}

STATIC int convert_model_maintaince_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_model_maintaince_task_t *model_maintaince_task = &task->u.model_maintaince_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_model_id = model_maintaince_task->model_id;
    uint16_t v_sub_stream_id = model_maintaince_task->stream_id;
    int convert_model_ret;
    int convert_stream_ret;
    convert_model_ret = convert_model_id(vsq_base_info, v_model_id, &model_maintaince_task->model_id);
    if (convert_model_ret != 0) {
        ts_agent_err("convert model id failed. v_stream_id=%u, v_model_id=%u, ret=%d.",
                     v_stream_id, v_model_id, convert_model_ret);
        model_maintaince_task->model_id = U16_MAX;
        // convert failed need convert sub stream_id also.
    } else {
        ts_agent_debug("convert v_model_id=%u to model_id=%u. v_stream_id=%u.",
                       v_model_id, model_maintaince_task->model_id, v_stream_id);
    }
    model_maintaince_task->v_model_id = v_model_id;

    convert_stream_ret = convert_stream_id(vsq_base_info, v_sub_stream_id, &model_maintaince_task->stream_id);
    if (convert_stream_ret != 0) {
        ts_agent_err("convert stream id failed. v_stream_id=%u, v_sub_stream_id=%u, ret=%d.",
                     v_stream_id, v_sub_stream_id, convert_stream_ret);
        model_maintaince_task->stream_id = U16_MAX;
    } else {
        ts_agent_debug("convert v_sub_stream_id=%u to sub_stream_id=%u. v_stream_id=%u.",
                       v_sub_stream_id, model_maintaince_task->stream_id, v_stream_id);
    }
    return convert_model_ret == 0 ? convert_stream_ret : convert_model_ret;
}

STATIC int convert_model_execute_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_model_execute_task_t *model_execute_task = &task->u.model_execute_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_model_id = model_execute_task->model_id;

    int convert_model_ret = convert_model_id(vsq_base_info, v_model_id, &model_execute_task->model_id);
    if (convert_model_ret != 0) {
        ts_agent_err("convert model id failed. v_stream_id=%u, v_model_id=%u, ret=%d.",
                     v_stream_id, v_model_id, convert_model_ret);
        model_execute_task->model_id = U16_MAX;
    } else {
        ts_agent_debug("convert v_model_id=%u to model_id=%u. v_stream_id=%u.",
                       v_model_id, model_execute_task->model_id, v_stream_id);
    }
    return convert_model_ret;
}

STATIC int convert_stream_switch_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_stream_switch_task_t *stream_switch_task = &task->u.stream_switch_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_true_stream_id = stream_switch_task->trueStreamId;
    int ret = convert_stream_id(vsq_base_info, v_true_stream_id, &stream_switch_task->trueStreamId);
    if (ret != 0) {
        ts_agent_err("convert true stream id failed. v_stream_id=%u, v_true_stream_id=%u, ret=%d.",
                     v_stream_id, v_true_stream_id, ret);
        stream_switch_task->trueStreamId = U16_MAX;
    } else {
        ts_agent_debug("convert v_true_stream_id=%u to true_stream_id=%u. v_stream_id=%u.",
                       v_true_stream_id, stream_switch_task->trueStreamId, v_stream_id);
    }
    return 0;
}

STATIC int convert_stream_active_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_stream_active_task_t *stream_active_task = &task->u.stream_active_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_active_stream_id = stream_active_task->activeStreamId;
    int ret = convert_stream_id(vsq_base_info, v_active_stream_id, &stream_active_task->activeStreamId);
    if (ret != 0) {
        ts_agent_err("convert active stream id failed. v_stream_id=%u, v_active_stream_id=%u, ret=%d.",
                     v_stream_id, v_active_stream_id, ret);
        stream_active_task->activeStreamId = U16_MAX;
    } else {
        ts_agent_debug("convert v_true_stream_id=%u to true_stream_id=%u. v_stream_id=%u.",
                       v_active_stream_id, stream_active_task->activeStreamId, v_stream_id);
    }
    return 0;
}

STATIC int convert_event_reset_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_event_reset_task_t *event_reset_task = &task->u.event_reset_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_event_id = event_reset_task->eventID;
    int ret = convert_event_id(vsq_base_info, v_event_id, &event_reset_task->eventID);
    if (ret != 0) {
        ts_agent_err("convert event id failed. v_stream_id=%u, v_event_id=%u, ret=%d.",
                     v_stream_id, v_event_id, ret);
        event_reset_task->eventID = U16_MAX;
    } else {
        ts_agent_debug("convert v_event_id=%u to event_id=%u. v_stream_id=%u.",
                       v_event_id, event_reset_task->eventID, v_stream_id);
    }
    return ret;
}

STATIC int convert_model_end_graph_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_end_graph_t *end_graph_task = &task->u.end_graph_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_model_id = end_graph_task->model_id;
    uint16_t model_id;

    int ret = convert_model_id(vsq_base_info, v_model_id, &model_id);
    if (ret != 0) {
        ts_agent_err("convert model id failed. v_stream_id=%u, v_model_id=%u, ret=%d.",
                     v_stream_id, v_model_id, ret);
        end_graph_task->model_id = U16_MAX;
    } else {
        end_graph_task->model_id = model_id;
        ts_agent_debug("convert v_model_id=%u to model_id=%u. v_stream_id=%u.",
                       v_model_id, end_graph_task->model_id, v_stream_id);
    }
    return ret;
}

STATIC int convert_model_exit_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_model_exit_t *model_exit_task = &task->u.model_exit_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_model_id = model_exit_task->model_id;
    uint16_t v_sub_stream_id = model_exit_task->stream_id;
    uint16_t model_id;
    uint16_t sub_stream_id;
    int convert_model_ret;
    int convert_stream_ret;
    convert_model_ret = convert_model_id(vsq_base_info, v_model_id, &model_id);
    if (convert_model_ret != 0) {
        ts_agent_err("convert model id failed. v_stream_id=%u, v_model_id=%u, ret=%d.",
                     v_stream_id, v_model_id, convert_model_ret);
        model_exit_task->model_id = U16_MAX;
        // convert failed need convert sub stream_id also.
    } else {
        model_exit_task->model_id = model_id;
        ts_agent_debug("convert v_model_id=%u to model_id=%u. v_stream_id=%u.",
                       v_model_id, model_exit_task->model_id, v_stream_id);
    }

    convert_stream_ret = convert_stream_id(vsq_base_info, v_sub_stream_id, &sub_stream_id);
    if (convert_stream_ret != 0) {
        ts_agent_err("convert stream id failed. v_stream_id=%u, v_sub_stream_id=%u, ret=%d.",
                     v_stream_id, v_sub_stream_id, convert_stream_ret);
        model_exit_task->stream_id = U16_MAX;
    } else {
        model_exit_task->stream_id = sub_stream_id;
        ts_agent_debug("convert v_sub_stream_id=%u to sub_stream_id=%u. v_stream_id=%u.",
                       v_sub_stream_id, model_exit_task->stream_id, v_stream_id);
    }
    return convert_model_ret == 0 ? convert_stream_ret : convert_model_ret;
}

static int convert_aicpu_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_aicpu_task_t *aicpu_task = &task->u.aicpu_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_model_id = aicpu_task->model_id;
    uint16_t model_id;

    int ret = convert_model_id(vsq_base_info, v_model_id, &model_id);
    if (ret != 0) {
        ts_agent_err("convert model id failed. v_stream_id=%u, v_model_id=%u, ret=%d.",
                     v_stream_id, v_model_id, ret);
        aicpu_task->model_id = U16_MAX;
    } else {
        aicpu_task->model_id = model_id;
        ts_agent_debug("convert v_model_id=%u to model_id=%u. v_stream_id=%u.",
                       v_model_id, aicpu_task->model_id, v_stream_id);
    }
    return ret;
}

static int convert_stream_label_goto_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_stream_label_goto_task_t *stream_label_goto_task = &task->u.stream_label_goto_task;
    uint16_t v_stream_id = task->streamID;
    uint16_t v_model_id = stream_label_goto_task->model_id;
    int ret = convert_model_id(vsq_base_info, v_model_id, &stream_label_goto_task->model_id);
    if (ret != 0) {
        ts_agent_err("convert model id failed. v_stream_id=%u, v_model_id=%u, ret=%d.",
                     v_stream_id, v_model_id, ret);
        stream_label_goto_task->model_id = U16_MAX;
    } else {
        ts_agent_debug("convert v_model_id=%u to model_id=%u. v_stream_id=%u.",
                       v_model_id, stream_label_goto_task->model_id, v_stream_id);
    }
    return ret;
}

static int convert_not_support_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_agent_err("task is not support. vsq_id=%u, v_stream_id=%u, task_id=%u, task_type=%u.",
                 vsq_base_info->vsq_id, task->streamID, task->taskID, task->type);
    return -EPERM;
}

static int convert_do_nothing(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    ts_agent_debug("task no need custom convert, vsq_id=%u, v_stream_id=%u, task_id=%u, task_type=%u.",
                   vsq_base_info->vsq_id, task->streamID, task->taskID, task->type);
    return 0;
}

// attention: must config all task type.
void init_task_convert_func(void)
{
    int32_t type;
    for (type = 0; type < TS_TASK_TYPE_RESERVED; type++) {
        g_task_convert_fn[type] = convert_do_nothing;
    }
    // ts_event_record_task_t
    g_task_convert_fn[TS_TASK_TYPE_EVENT_RECORD] = convert_event_record_task;
    // ts_stream_wait_event_task_t
    g_task_convert_fn[TS_TASK_TYPE_STREAM_WAIT_EVENT] = convert_stream_wait_event_task;
    // ts_maintenance_task_t
    g_task_convert_fn[TS_TASK_TYPE_MAINTENANCE] = convert_maintenance_task;
    // ts_create_stream_t
    g_task_convert_fn[TS_TASK_TYPE_CREATE_STREAM] = convert_create_stream_task;
    // ts_remote_event_wait_t not support
    g_task_convert_fn[TS_TASK_TYPE_REMOTE_EVENT_WAIT] = convert_not_support_task;
    // tsPCTrace_task_t is not support online
    g_task_convert_fn[TS_TASK_TYPE_PCTRACE_ENABLE] = convert_not_support_task;
    // ts_model_maintaince_task_t
    g_task_convert_fn[TS_TASK_TYPE_MODEL_MAINTAINCE] = convert_model_maintaince_task;
    // ts_model_execute_task_t
    g_task_convert_fn[TS_TASK_TYPE_MODEL_EXECUTE] = convert_model_execute_task;
    // ts_notify_wait_task_t
    g_task_convert_fn[TS_TASK_TYPE_NOTIFY_WAIT] = convert_notify_wait_task;
    // ts_notify_record_task_t not support
    g_task_convert_fn[TS_TASK_TYPE_NOTIFY_RECORD] = convert_not_support_task;
    // ts_rdma_send_task_t not support
    g_task_convert_fn[TS_TASK_TYPE_RDMA_SEND] = convert_not_support_task;
    // ts_stream_switch_task_t
    g_task_convert_fn[TS_TASK_TYPE_STREAM_SWITCH] = convert_stream_switch_task;
    // ts_stream_active_task_t
    g_task_convert_fn[TS_TASK_TYPE_STREAM_ACTIVE] = convert_stream_active_task;
    // ts_label_switch_task_t not support
    g_task_convert_fn[TS_TASK_TYPE_LABEL_SWITCH] = convert_not_support_task;
    // ts_label_goto_task_t not support
    g_task_convert_fn[TS_TASK_TYPE_LABEL_GOTO] = convert_not_support_task;
    // ts_event_reset_task_t
    g_task_convert_fn[TS_TASK_TYPE_EVENT_RESET] = convert_event_reset_task;
    // ts_end_graph_t
    g_task_convert_fn[TS_TASK_TYPE_MODEL_END_GRAPH] = convert_model_end_graph_task;
    // ts_aicpu_task_t
    g_task_convert_fn[TS_TASK_TYPE_MODEL_TO_AICPU] = convert_aicpu_task;
    // ts_stream_switchN_task_t not support
    g_task_convert_fn[TS_TASK_TYPE_STREAM_SWITCH_N] = convert_not_support_task;
    // ts_stream_label_goto_task_t
    g_task_convert_fn[TS_TASK_TYPE_STREAM_LABEL_GOTO] = convert_stream_label_goto_task;
    // ts_debug_register_task_t
    g_task_convert_fn[TS_TASK_TYPE_DEBUG_REGISTER] = convert_not_support_task;
    // ts_debug_unregister_task_t
    g_task_convert_fn[TS_TASK_TYPE_DEBUG_UNREGISTER] = convert_not_support_task;
    // ts_model_exit_t
    g_task_convert_fn[TS_TASK_TYPE_MODEL_EXIT] = convert_model_exit_task;
    // ts_mdcprof_t not support
    g_task_convert_fn[TS_TASK_TYPE_MDCPROF] = convert_not_support_task;
    // ts_debug_register_with_stream_task_t
    g_task_convert_fn[TS_TASK_TYPE_DEBUG_REGISTER_WITH_STREAM] = convert_not_support_task;
    // ts_debug_unregister_with_stream_task_t
    g_task_convert_fn[TS_TASK_TYPE_DEBUG_UNREGISTER_WITH_STREAM] = convert_not_support_task;
    // ts_reduce_async_v2_task_t
    g_task_convert_fn[TS_TASK_TYPE_REDUCE_ASYNC_V2] = convert_not_support_task;
}

static int convert_task(const vsq_base_info_t *vsq_base_info, ts_task_t *task)
{
    task_convert_custom_fn_t custom_fn = NULL;
    int ret;
    int custom_convert_ret;
    if (task->type >= TS_TASK_TYPE_RESERVED) {
        ts_agent_err("task type is invalid. task_type=%u, task_id=%u.",
                     task->type, task->taskID);
        return -EINVAL;
    }
    custom_fn = g_task_convert_fn[task->type];
    // no custom convert function is allowed.
    if (custom_fn == NULL) {
        ts_agent_err("task does not config convert function. task_type=%u, task_id=%u.",
                     task->type, task->taskID);
        custom_convert_ret = -EINVAL;
    } else {
        custom_convert_ret = custom_fn(vsq_base_info, task);
        if (custom_convert_ret != 0) {
            ts_agent_err("custom convert task failed. ret=%d, task_type=%u, task_id=%u.",
                         custom_convert_ret, task->type, task->taskID);
            // don't return, convert_task_basic must convert.
            task->task_info_flag |= TS_TASK_INVALID_FLAG; /* set invalid flag */
        }
    }

    // convert_task_basic must after custom_convert, as custom convert may use streamId.
    ret = convert_task_basic(vsq_base_info, task);
    if (ret != 0) {
        ts_agent_err("convert task basic failed. ret=%d, task_type=%u, task_id=%u.",
                     ret, task->type, task->taskID);
        task->task_info_flag |= TS_TASK_INVALID_FLAG; /* set invalid flag */
        return ret;
    }

    return custom_convert_ret;
}

static int proc_task(const vsq_base_info_t *vsq_base_info, char *task_buf, size_t buf_len)
{
    ts_task_t *task = NULL;
    ts_host_func_sq_send_msg_t *callback_msg = NULL;
    int ret;
    u16 v_stream_id;

    switch (vsq_base_info->vsq_type) {
        case NORMAL_VSQCQ_TYPE: {
            task = (ts_task_t *) task_buf;
            ret = convert_task(vsq_base_info, task);
            if (ret != 0) {
                ts_agent_err("convert task failed. ret=%d, dev_id=%u, vf_id=%u, ts_id=%u, "
                             "vsq_id=%u, stream_id=%u, task_type=%u, task_id=%u.",
                             ret, vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id,
                             vsq_base_info->vsq_id, task->streamID, task->type, task->taskID);
                // can not break, as error task is need send to ts also,
                // if not send, may lost cq report and can't update vsq_head.
            }
            break;
        }
        case CALLBACK_VSQCQ_TYPE: {
            // will be need convert later
            callback_msg = (ts_host_func_sq_send_msg_t *) task_buf;
            v_stream_id = callback_msg->stream_id;
            ret = convert_stream_id(vsq_base_info, v_stream_id, &callback_msg->stream_id);
            if (ret != 0) {
                ts_agent_err("convert stream id failed. v_stream_id=%u, ret=%d.", v_stream_id, ret);
                callback_msg->stream_id = U16_MAX;
            } else {
                ts_agent_debug("callback sq stream id is need convert now. dev_id=%u, vf_id=%u, ts_id=%u, "
                               "vsq_id=%u, stream_id=%u, task_id=%u, v_stream_id=%u.",
                               vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id,
                               vsq_base_info->vsq_id, callback_msg->stream_id, callback_msg->task_id, v_stream_id);
            }
            break;
        }
        default: {
            ts_agent_err("unknown vsq type. dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, vsq_type=%d.",
                         vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id,
                         vsq_base_info->vsq_id, vsq_base_info->vsq_type);
            ret = -EINVAL;
            break;
        }
    }
    return ret;
}

static int proc_vsq_by_range(const vsq_base_info_t *vsq_base, u32 head, u32 tail)
{
    const char *vsq_base_addr = (const char *)vsq_base->vsq_base_addr;
    u32 vsq_slot_size = vsq_base->vsq_slot_size;
    char task_buf[TS_TASK_COMMAND_SIZE];
    int ret = 0;
    u32 copy_size;
    u32 deal_count = 0;
    u32 curr_head = head;
    struct hvtsdrv_vsq_slot_info sq_slot;

    ts_agent_debug("proc vsq, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, head=%u, tail=%u.",
        vsq_base->dev_id, vsq_base->vf_id, vsq_base->ts_id, vsq_base->vsq_id, head, tail);

    copy_size = (vsq_slot_size > TS_TASK_COMMAND_SIZE) ? TS_TASK_COMMAND_SIZE : vsq_slot_size;

    sq_slot.vsq_id = vsq_base->vsq_id;
    sq_slot.vsq_slot_addr = task_buf;
    sq_slot.vsq_slot_size = TS_TASK_COMMAND_SIZE;

    while (curr_head != tail) {
        // copy task from vsq
        errno_t cp_ret = memcpy_s(task_buf, TS_TASK_COMMAND_SIZE, vsq_base_addr + curr_head * vsq_slot_size, copy_size);
        if (cp_ret != EOK) {
            ts_agent_err("memcpy task from vsq failed. cp_ret=%d, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, "
                         "vsq_slot_addr=%pk, vsq_slot_size=%u, head=%u.", cp_ret, vsq_base->dev_id, vsq_base->vf_id,
                vsq_base->ts_id, sq_slot.vsq_id, vsq_base_addr, vsq_slot_size, head);
            ret = -EINVAL;
            break;
        }
        ret = proc_task(vsq_base, task_buf, TS_TASK_COMMAND_SIZE);
        if (ret != 0) {
            ts_agent_err("proc_task failed. ret=%d, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, vsq_type=%d.",
                ret, vsq_base->dev_id, vsq_base->vf_id, vsq_base->ts_id, vsq_base->vsq_id, vsq_base->vsq_type);
            // can not break, as error task is need send to ts also,
            // if not send, may lost cq report and can't update vsq_head.
        }
        ret = hvtsdrv_sq_write(vsq_base->dev_id, vsq_base->vf_id, vsq_base->ts_id, &sq_slot);
        if (ret != 0) {
            ts_agent_err("Driver sq write failed. ret=%d, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, vsq_slot_addr=%pk,"
                         " vsq_slot_size=%u.", ret, vsq_base->dev_id, vsq_base->vf_id, vsq_base->ts_id, sq_slot.vsq_id,
                sq_slot.vsq_slot_addr, sq_slot.vsq_slot_size);
            break;
        }
        ++deal_count;
        ++curr_head;
        curr_head = curr_head % vsq_base->vsq_dep;
    }
    if (deal_count > 0) {
        hvtsdrv_sq_irq_trigger(vsq_base->dev_id, vsq_base->vf_id, vsq_base->ts_id, vsq_base->vsq_id);
    }
    ts_agent_debug(
        "proc vsq finish, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, head=%u, curr_head=%u, tail=%u, deal_count=%u.",
        vsq_base->dev_id, vsq_base->vf_id, vsq_base->ts_id, vsq_base->vsq_id, head, curr_head, tail, deal_count);
    return ret;
}

void proc_vsq(vsq_base_info_t *vsq_base_info)
{
    // retry once when proc end.
    int retry_times = 1;
    ts_agent_debug("proc vsq begin, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u.",
                   vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
    do {
        u32 head;
        u32 tail;
        int ret = get_vsq_head_and_tail(vsq_base_info, &head, &tail);
        if (ret != 0) {
            break;
        }
        if (head == tail) {
            ts_agent_debug("head is same as tail, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u, head=%u.",
                           vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id,
                           head);
            break;
        }
        ret = proc_vsq_by_range(vsq_base_info, head, tail);
        if (ret != 0) {
            break;
        }
        retry_times--;
    } while (retry_times >= 0);
    ts_agent_debug("proc vsq end, dev_id=%u, vf_id=%u, ts_id=%u, vsq_id=%u.",
                   vsq_base_info->dev_id, vsq_base_info->vf_id, vsq_base_info->ts_id, vsq_base_info->vsq_id);
}

int vsq_top_proc(vsq_base_info_t *vsq_base_info, u32 cmd_num)
{
    return EOK;
}

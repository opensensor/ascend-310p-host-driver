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
#ifndef QUEUE_UT
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_common_pub.h"

#include "dbl/uda.h"
#include "queue_module.h"
#include "queue_channel.h"
#include "queue_fops.h"
#include "queue_msg.h"
#include "hdcdrv_interface.h"
#include "queue_context.h"

STATIC ka_device_t *queue_devices[MAX_DEVICE] = {NULL};
STATIC ka_rw_semaphore_t queue_dev_sem[MAX_DEVICE];

ka_device_t *queue_get_device(u32 dev_id)
{
    ka_task_down_read(&queue_dev_sem[dev_id]);
    if (queue_devices[dev_id] == NULL) {
        ka_task_up_read(&queue_dev_sem[dev_id]);
        return NULL;
    }
    return queue_devices[dev_id];
}

void queue_put_device(u32 dev_id)
{
    if (queue_devices[dev_id] != NULL) {
        ka_task_up_read(&queue_dev_sem[dev_id]);
    }
}

STATIC void queue_init_instance(u32 devid, ka_device_t *dev)
{
    ka_task_down_write(&queue_dev_sem[devid]);
    queue_devices[devid] = dev;
    ka_task_up_write(&queue_dev_sem[devid]);

    queue_info("notifier init action success. (devid=%u).\n", devid);
    return;
}

STATIC void queue_uninit_instance(u32 devid)
{
    ka_task_down_write(&queue_dev_sem[devid]);
    queue_devices[devid] = NULL;
    ka_task_up_write(&queue_dev_sem[devid]);

    queue_info("notifier uninit action success. (devid=%u).\n", devid);
    return;
}

#define QUEUE_HOST_NOTIFIER "queue_host"
static int queue_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    if (udevid >= MAX_DEVICE) {
        queue_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        queue_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        queue_uninit_instance(udevid);
    }

    return 0;
}

int queue_drv_msg_chan_init(void)
{
    struct uda_dev_type type;
    u32 devid;

    for (devid = 0; devid < MAX_DEVICE; devid++) {
        ka_task_init_rwsem(&queue_dev_sem[devid]);
    }
    uda_davinci_near_real_entity_type_pack(&type);
    return uda_notifier_register(QUEUE_HOST_NOTIFIER, &type, UDA_PRI2, queue_host_notifier_func);
}

void queue_drv_msg_chan_uninit(void)
{
    struct uda_dev_type type;
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(QUEUE_HOST_NOTIFIER, &type);
}

STATIC int queue_data_in(int devid, int vfid, int local_pid, struct hdcdrv_data_info data)
{
    struct queue_reply_complete_msg *reply_msg = (struct queue_reply_complete_msg *)(uintptr_t)data.src_addr;
    struct queue_qid_status *qid_status = NULL;
    struct queue_context *ctx = NULL;
    int ret;

    if (reply_msg == NULL) {
        queue_err("head is NULL, invalid src_addr.\n");
        return HDCDRV_RX_FINISH;
    }

    if (data.len != sizeof(struct queue_reply_complete_msg)) {
        queue_err("invalid in_data_len(%u).\n", data.len);
        return HDCDRV_RX_FINISH;
    }

    ctx = queue_context_get(reply_msg->dma_info.hostpid);
    if (ctx == NULL) {
        queue_err("Pid is not exit. (pid=%d)\n", reply_msg->dma_info.hostpid);
        return HDCDRV_RX_FINISH;
    }

    qid_status = queue_get_qid_status(ctx, reply_msg->qid);
    if (qid_status != NULL) {
        queue_set_qid_status_timestamp(qid_status, HOST_HDC_RECV);
        queue_set_qid_status_dma_node_num(qid_status, reply_msg->dma_node_num);
        (void)memcpy_s(&qid_status->time_record[DEV_QUEUE_STATUS_RECORE_START], REPLY_MSG_TIME_RECORD_SIZE,
            reply_msg->time_record, REPLY_MSG_TIME_RECORD_SIZE);
    }

    ret = queue_wakeup_enqueue(ctx, reply_msg->dma_info.que_chan_addr);
    queue_set_qid_status_timestamp(qid_status, HOST_WAKE_UP);

    queue_context_put(ctx);
    if (ret != 0) {
        queue_err("queue_free_dma_node failed. (ret=%d, serial_num=%llu; que_chan_addr=0x%pK).\n",
            ret, reply_msg->dma_info.serial_num, (void *)(uintptr_t)reply_msg->dma_info.que_chan_addr);
    }

    return HDCDRV_RX_FINISH;
}

struct hdcdrv_session_notify g_queue_notify = {
    .data_in_notify = queue_data_in,
};

void queue_register_hdc_cb_func(void)
{
    queue_info("queue_register_hdc_cb_func succ.\n");
    hdcdrv_session_notify_register(QUEUE_HDC_SERVICE_TYPE, &g_queue_notify);
}

void queue_unregister_hdc_cb_func(void)
{
    queue_info("queue_unregister_hdc_cb_func succ.\n");
    hdcdrv_session_notify_unregister(QUEUE_HDC_SERVICE_TYPE);
}

#else
void queue_drv_msg_chan_uninit(void)
{
    return;
}
#endif

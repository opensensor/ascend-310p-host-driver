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
#include <securec.h>

#include "ka_barrier_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "devdrv_manager_comm.h"
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2)
#include "devdrv_manager_common.h"
#endif
#include "tsdrv_drvops.h"
#include "tsdrv_device.h"
#include "devdrv_interface.h"
#include "tsdrv_mailbox_dfx.h"
#include "tsdrv_osal_intr.h"
#include "devdrv_mailbox.h"

STATIC void devdrv_mailbox_ack_proc(struct devdrv_mailbox_message *message,
    struct devdrv_mailbox_message_header *header);
STATIC ka_irqreturn_t devdrv_mailbox_ack_irq(int irq, void *data);

static inline u64 devdrv_mailbox_get_us_timestamp(void)
{
#ifndef TSDRV_UT
    return (u64)ka_system_ktime_to_us(ka_system_ktime_get_boottime());
#else
    return 0;
#endif
}

/* trigger irq to inform TS */
STATIC void devdrv_mailbox_trigger_irq(u32 devid, u32 tsid)
{
    if (tsdrv_get_drv_ops()->irq_trigger == NULL) {
        TSDRV_PRINT_ERR("[dev_id = %u]:irq trigger is NULL\n", devid);
        return;
    }
    tsdrv_get_drv_ops()->irq_trigger(devid, tsid);
}

STATIC void devdrv_mailbox_exit(u32 devid, struct devdrv_mailbox *mailbox)
{
    if (mailbox == NULL) {
        TSDRV_PRINT_ERR("mailbox is null.\n");
        return;
    }
    (void)tsdrv_unrequest_irq(devid, mailbox->ack_irq, mailbox);
    mailbox->working = 0;
    ka_task_mutex_destroy(&mailbox->mlock);
}

STATIC int devdrv_mailbox_init(u32 devid, struct devdrv_mailbox *mailbox)
{
    int ret;
#ifdef CFG_SOC_PLATFORM_HELPER
    u32 first_ccpu, last_ccpu;
#endif

    DRV_PRINT_START();
    if (mailbox == NULL) {
        TSDRV_PRINT_ERR("input argument error.\n");
        return -EINVAL;
    }
    /* init mailbox */
    ka_base_atomic_set(&mailbox->status, DEVDRV_MAILBOX_FREE);
    ka_base_atomic_set(&mailbox->timeout, 0);
    mailbox->dfx_times = 0;
    mailbox->mailbox_type = DEVDRV_MAILBOX_SRAM;

    ret = tsdrv_request_irq(devid, mailbox->ack_irq, devdrv_mailbox_ack_irq, mailbox, "devdrv-ack");
    if (ret != 0) {
        TSDRV_PRINT_ERR("ka_system_request_irq ack irq failed ret 0x%x.\n", ret);
        return ret;
    }

#ifdef CFG_SOC_PLATFORM_HELPER
    ret = tsdrv_get_cpu_index_range(devid, &first_ccpu, &last_ccpu);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to get ctrl cpu range. (devid=%u; ret=%d)\n", devid, ret);
        (void)tsdrv_unrequest_irq(devid, mailbox->ack_irq, mailbox);
        return -EINVAL;
#endif
    }
    tsdrv_irq_set_affinity_hint(devid, mailbox->ack_irq, first_ccpu, last_ccpu);
#endif
    ka_task_mutex_init(&mailbox->mlock);
    mailbox->working = 1;
    DRV_PRINT_END();
    return 0;
}

int tsdrv_mailbox_kernel_sync_no_feedback(u32 devid, u32 tsid, const u8 *buf, u32 len, int *result)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct devdrv_mailbox *mailbox = NULL;

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    mailbox = &ts_resource->mailbox;

    return devdrv_mailbox_kernel_sync_no_feedback(mailbox, buf, len, result);
#endif
}

int tsdrv_mbox_init(u32 devid, u32 tsnum)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    u32 tsid;
    int err;
    int result = 0;
    u32 i;
    struct devdrv_mailbox_notice_ack_irq msg;

    msg.head.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    msg.head.cmd_type = DEVDRV_MAILBOX_NOTICE_ACK_IRQ_VALUE;
    msg.head.result = 0;

    for (tsid = 0; tsid < tsnum; tsid++) {
        struct tsdrv_mbox_hwinfo *mbox_hwinfo = tsdrv_get_ts_mbox_hwinfo(devid, tsid, TSDRV_TS_MBOX);
        struct devdrv_mailbox *mailbox = &tsdrv_dev->dev_res[TSDRV_PM_FID].ts_resource[tsid].mailbox;
        mailbox->ack_irq = mbox_hwinfo->mbox_ack_request_irq;
        mailbox->send_sram = mbox_hwinfo->mbox_tx;
        mailbox->receive_sram = mbox_hwinfo->mbox_rx;
        mailbox->data_ack_irq = mbox_hwinfo->mbox_rx_irq;
        err = devdrv_mailbox_init(devid, mailbox);
        if (err != 0) {
            TSDRV_PRINT_ERR("mbox init fail, devid(%u) tsid(%u)\n", devid, tsid);
            goto err;
        }

        err = tsdrv_create_mbox_dfx_addr(devid, tsid);
        if (err != 0) {
            devdrv_mailbox_exit(devid, mailbox);
            goto err;
        }

        if (tsdrv_get_env_type() == TSDRV_ENV_ONLINE) {
            msg.ack_irq = mbox_hwinfo->mbox_ack_irq;

            err = devdrv_mailbox_kernel_sync_no_feedback(mailbox, (const u8 *)&msg, sizeof(msg), &result);
            TSDRV_PRINT_INFO("notice ack irq devid(%u) tsid(%u) ret %d result %d\n", devid, tsid, err, result);
        }
    }
    return 0;
err:
    for (i = 0; i < tsid; i++) {
        tsdrv_destroy_mbox_dfx_addr(devid, i);
        devdrv_mailbox_exit(devid, &tsdrv_dev->dev_res[TSDRV_PM_FID].ts_resource[i].mailbox);
    }
    return -ENODEV;
}

void tsdrv_mbox_exit(u32 devid, u32 tsnum)
{
    struct tsdrv_device *tsdrv_dev = tsdrv_get_dev(devid);
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        struct tsdrv_mbox_hwinfo *mbox_hwinfo = tsdrv_get_ts_mbox_hwinfo(devid, tsid, TSDRV_TS_MBOX);
        struct devdrv_mailbox *mailbox = &tsdrv_dev->dev_res[TSDRV_PM_FID].ts_resource[tsid].mailbox;
        mailbox->ack_irq = mbox_hwinfo->mbox_ack_request_irq;
        mailbox->send_sram = mbox_hwinfo->mbox_tx;
        mailbox->receive_sram = mbox_hwinfo->mbox_rx;
        mailbox->data_ack_irq = mbox_hwinfo->mbox_rx_irq;
        tsdrv_destroy_mbox_dfx_addr(devid, tsid);
        devdrv_mailbox_exit(devid, &tsdrv_dev->dev_res[TSDRV_PM_FID].ts_resource[tsid].mailbox);
    }
}

STATIC void devdrv_mailbox_data_send(struct devdrv_mailbox *mailbox, struct devdrv_mailbox_message *message)
{
    struct tsdrv_ts_resource *ts_resource = message->ts_resource;
    struct tsdrv_device *ts_dev = NULL;
    enum tsdrv_env_type env_type;
    u32 tsid = ts_resource->tsid;
    u32 i;

    ts_dev = tsdrv_res_to_dev(ts_resource);
    env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_OFFLINE) {
        ka_mm_memcpy_toio(mailbox->send_sram, message->message_payload, message->message_length);

        for (i = message->message_length; i < DEVDRV_MAILBOX_PAYLOAD_LENGTH; i++) {
            mailbox->send_sram[i] = 0;
        }
        ka_wmb(); /* memory barrier */
        message->is_sent = 1;
        devdrv_mailbox_trigger_irq(ts_dev->devid, tsid);
    } else if (env_type == TSDRV_ENV_ONLINE) {
        ka_mm_memcpy_toio(mailbox->receive_sram, message->message_payload, message->message_length);

        for (i = message->message_length; i < DEVDRV_MAILBOX_PAYLOAD_LENGTH; i++) {
            mailbox->receive_sram[i] = 0;
        }
        ka_wmb(); /* memory barrier */
        message->is_sent = 1;
        devdrv_mailbox_trigger_irq(ts_dev->devid, tsid);
    }
#ifndef TSDRV_UT
    mailbox->send = devdrv_mailbox_get_us_timestamp();
#endif
}

/*
 * try to send message
 * if mailbox is free, add message into send queue, write message payload into sram,
 *	create doorbell interrupt to inform TS
 * else if mailbox is busy, add message into send queue, return
 *
 * when add message into send queue, must make the new message be the last valid message
 *	Fist In First Out
 *
 * return:
 * 0  for succ
 * negative for error
 */
STATIC void tsdrv_mbox_data_ready(struct devdrv_mailbox *mailbox, struct devdrv_mailbox_message *message)
{
    struct devdrv_mailbox_message_header *header = NULL;

    message->process_result = 0;
    message->is_sent = 0;

    header = (struct devdrv_mailbox_message_header *)message->message_payload;
    header->result = 0;
    header->valid = DEVDRV_MAILBOX_MESSAGE_VALID;

    ka_task_sema_init(&message->wait, 0);
#ifndef TSDRV_UT
    mailbox->irq_time = 0;
#endif
    message->message_type = DEVDRV_MAILBOX_SYNC_MESSAGE;
}

STATIC void devdrv_mailbox_try_to_send(struct devdrv_mailbox *mailbox, struct devdrv_mailbox_message *message_info,
    struct tsdrv_mailbox_dfx *mbox_dfx)
{
    tsdrv_mbox_data_ready(mailbox, message_info);
    devdrv_mailbox_data_send(mailbox, message_info);
    mbox_dfx->send_num++;
}

STATIC int devdrv_mailbox_add_message_check(struct devdrv_mailbox *mailbox, struct devdrv_mailbox_message *message_info)
{
    if (!mailbox->working) {
        TSDRV_PRINT_ERR("mailbox not working.\n");
        return -EINVAL;
    }

    if (message_info->message_length > DEVDRV_MAILBOX_PAYLOAD_LENGTH) {
        TSDRV_PRINT_ERR("invalid input argument.\n");
        return -EINVAL;
    }

    if (message_info->sync_type != DEVDRV_MAILBOX_SYNC) {
        TSDRV_PRINT_WARN("no support aync mailbox msg yet.\n");
        return -EBADTYPE;
    }

    return 0;
}

STATIC int tsdrv_mbox_wait(struct devdrv_mailbox *mbox, struct devdrv_mailbox_message *message,
    struct tsdrv_mailbox_dfx *mbox_dfx, int *result)
{
#ifndef TSDRV_UT
#define CUR_MESECS_TO_JIFFIES 1000

    struct devdrv_mailbox_message_header *header = NULL;
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    int ret, i;
    u64 jiffy;

    header = (tsdrv_get_env_type() == TSDRV_ENV_OFFLINE) ?
        (struct devdrv_mailbox_message_header *)message->mailbox->send_sram :
        (struct devdrv_mailbox_message_header *)message->mailbox->receive_sram;

    for (i = 0; i < DEVDRV_MAILBOX_DOWN_TRY_TIMES; i++) {
        jiffy = ka_system_msecs_to_jiffies(CUR_MESECS_TO_JIFFIES);
        ret = ka_task_down_timeout(&message->wait, jiffy);
        if (ret != 0) {
            mbox->down_timeout = devdrv_mailbox_get_us_timestamp();
            TSDRV_PRINT_WARN("mailbox down timeout, i(%d), start[%llu us], end[%llu us], irq[%llu us], "
                "ret(%d), valid(%u), result(%u), cmd_type(%u)\n",
                i, mbox->send, mbox->down_timeout, mbox->irq_time, ret, (u32)header->valid, header->result,
                (u32)header->cmd_type);
        } else {
            ka_base_atomic_set(&mbox->timeout, 0);
            mbox->dfx_times = 0;
            *result = message->process_result;
            mbox_dfx->receive_num++;
            return 0;
        }

        if ((env_type == TSDRV_ENV_ONLINE) && (header->valid == 0) && (header->result == 0)) {
            devdrv_mailbox_ack_proc(message, header);
            ka_base_atomic_set(&mbox->timeout, 0);
            mbox->dfx_times = 0;
            *result = message->process_result;
            mbox_dfx->receive_num++;

            TSDRV_PRINT_WARN("Data is valid, i(%d), start[%llu us], end[%llu us], irq[%llu us], "
                "ret(%d), valid(%u), result(%u), cmd_type(%u)\n",
                i, mbox->send, mbox->down_timeout, mbox->irq_time, ret, (u32)header->valid, header->result,
                (u32)header->cmd_type);
            return 0;
        }
    }

    if (ret == -ETIME) {
        ka_base_atomic_add(1, &mbox->timeout);
    } else {
        ka_base_atomic_set(&mbox->timeout, 0);
        mbox->dfx_times = 0;
    }
    mbox_dfx->lost_num++;
    *result = message->process_result;

    return ret;
#else
    *result = 0;
    return 0;
#endif
}
/*
 * block or nonblock depend on input message's field: sync_type
 * return: 0 -- ok
 *         1 -- error, caller must not release the message
 * -1 -- error, caller have to release the message
 */
STATIC int devdrv_mailbox_message_send(struct devdrv_mailbox *mbox, struct devdrv_mailbox_message *msg, int *result)
{
    struct tsdrv_mailbox_dfx *mbox_dfx = NULL;
    struct tsdrv_ts_resource *ts_res = NULL;
    u32 devid;
    int ret;

    ts_res = ka_container_of(mbox, struct tsdrv_ts_resource, mailbox);
    devid = tsdrv_get_devid_by_res(ts_res);
    mbox_dfx = tsdrv_get_mbox_dfx_addr(devid, ts_res->tsid);

    ret = devdrv_mailbox_add_message_check(mbox, msg);
    if (ret != 0) {
        return ret;
    }

    devdrv_mailbox_try_to_send(mbox, msg, mbox_dfx);

    return tsdrv_mbox_wait(mbox, msg, mbox_dfx, result);
}

int devdrv_mailbox_kernel_sync_no_feedback(struct devdrv_mailbox *mailbox, const u8 *buf, u32 len, int *result)
{
    struct devdrv_mailbox_message_header *header = NULL;
    struct devdrv_mailbox_message *message = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_device *ts_dev = NULL;
    u32 tsid;
    int ret;

    if ((mailbox == NULL) || (buf == NULL) || (len < sizeof(struct devdrv_mailbox_message_header)) ||
        (len > DEVDRV_MAILBOX_PAYLOAD_LENGTH) || (result == NULL)) {
        TSDRV_PRINT_ERR("input argument invalid.\n");
        return -EINVAL;
    }
    ts_resource = ka_container_of(mailbox, struct tsdrv_ts_resource, mailbox);
    ts_dev = tsdrv_res_to_dev(ts_resource);
    tsid = ts_resource->tsid;

    if (ka_base_atomic_read(&mailbox->timeout) >= TSDRV_CONT_TIMEOUT_CNT) {
#ifndef TSDRV_UT
        if (mailbox->dfx_times < TSDRV_DFX_MAX_TIMES) {
            mailbox->dfx_times++;
            TSDRV_PRINT_ERR("[dev_id=%u tsid=%u]too many continue timeout, please check ts status...\n",
                ts_dev->devid, tsid);
        }
#endif
        return -EAGAIN;
    }
    ka_task_mutex_lock(&mailbox->mlock);
    message = &mailbox->message;

    ret = memcpy_s(message->message_payload, DEVDRV_MAILBOX_PAYLOAD_LENGTH, buf, len);
    if (ret != 0) {
        TSDRV_PRINT_ERR("[dev_id=%u tsid=%u]:memcpy_s failed, ret = %d.\n", ts_dev->devid, tsid, ret);
        ka_task_mutex_unlock(&mailbox->mlock);
        return ret;
    }

    message->message_length = DEVDRV_MAILBOX_PAYLOAD_LENGTH;
    message->feedback = DEVDRV_MAILBOX_NO_FEEDBACK;
    message->feedback_num = 0;
    message->feedback_buffer = NULL;
    message->feedback_count = 0;
    message->process_result = 0;
    message->hwts_sqid = 0xffff;
    message->sync_type = DEVDRV_MAILBOX_SYNC;
    message->cmd_type = 0;
    message->message_index = 0;
    message->message_pid = 0;
    message->callback = NULL;
    message->mailbox = mailbox;
    message->ts_resource = ts_resource;
    message->abandon = DEVDRV_MAILBOX_VALID_MESSAGE;

    ka_base_atomic_set(&mailbox->status, DEVDRV_MAILBOX_BUSY);
    ret = devdrv_mailbox_message_send(mailbox, message, result);
    if (ret != 0) {
        TSDRV_PRINT_ERR("[dev_id=%u tsid=%u]:devdrv_mailbox_message_send failed. ret=%d\n", ts_dev->devid, tsid, ret);
        if (ret == -ETIME) {
#ifndef TSDRV_UT
            ka_base_atomic_set(&mailbox->status, DEVDRV_MAILBOX_FREE);
            ka_task_mutex_unlock(&mailbox->mlock);
            return -EAGAIN;
#endif
        }
    }

    header = (struct devdrv_mailbox_message_header *)buf;
    if (header->cmd_type == DEVDRV_MAILBOX_CREATE_CQSQ_CALC) {
        struct devdrv_normal_cqsq_mailbox *normal_sqcq_mbox = (struct devdrv_normal_cqsq_mailbox *)buf;
        normal_sqcq_mbox->sq_index = (u16)message->hwts_sqid;
    }

    ka_base_atomic_set(&mailbox->status, DEVDRV_MAILBOX_FREE);
    ka_task_mutex_unlock(&mailbox->mlock);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(devdrv_mailbox_kernel_sync_no_feedback);

STATIC int tsdrv_mailbox_send_para_check(u32 devid, u32 tsid,
    struct tsdrv_mbox_data *data)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= tsdrv_get_dev_tsnum(devid))) {
        TSDRV_PRINT_ERR("invalid para, devid(%u) tsid(%u).\n", devid, tsid);
        return -EINVAL;
    }

    if ((data == NULL) || (data->msg == NULL)) {
        TSDRV_PRINT_ERR("invalid para(%pK).\n", data);
        return -EINVAL;
    }
    if ((data->msg_len < sizeof(struct devdrv_mailbox_message_header)) ||
        (data->msg_len > DEVDRV_MAILBOX_PAYLOAD_LENGTH)) {
        TSDRV_PRINT_ERR("invalid para(%#x).\n", data->msg_len);
        return -EINVAL;
    }

    if ((data->out_len != 0) && (data->out_data == NULL)) {
        TSDRV_PRINT_ERR("invalid para(%pK %#x).\n", data->out_data, data->out_len);
        return -EINVAL;
    }
    if (data->out_len > DEVDRV_MAILBOX_PAYLOAD_LENGTH) {
        TSDRV_PRINT_ERR("invalid para(%#x).\n", data->out_len);
        return -EINVAL;
    }

    return 0;
}

int tsdrv_mailbox_send_sync(u32 devid, u32 tsid, struct tsdrv_mbox_data *data)
{
    struct devdrv_mailbox_message *message = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct devdrv_mailbox *mailbox = NULL;
    enum tsdrv_env_type env_type;
    u8 __iomem *src_addr = NULL;
    int result = 0;
    int ret;

    if (tsdrv_mailbox_send_para_check(devid, tsid, data) != 0) {
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    mailbox = &ts_resource->mailbox;
    if (ka_base_atomic_read(&mailbox->timeout) >= TSDRV_CONT_TIMEOUT_CNT) {
#ifndef TSDRV_UT
        if (mailbox->dfx_times < TSDRV_DFX_MAX_TIMES) {
            mailbox->dfx_times++;
            TSDRV_PRINT_ERR("[dev_id=%u tsid=%u]:too many continue timeout, please check ts status...\n", devid, tsid);
        }
#endif
        return -EAGAIN;
    }

    ka_task_mutex_lock(&mailbox->mlock);
    message = &mailbox->message;
    ret = memcpy_s(message->message_payload, DEVDRV_MAILBOX_PAYLOAD_LENGTH, data->msg, data->msg_len);
    if (ret != 0) {
        ka_task_mutex_unlock(&mailbox->mlock);
        TSDRV_PRINT_ERR("[dev_id=%u tsid=%u]:memcpy_s failed, ret = %d.\n", devid, tsid, ret);
        return ret;
    }

    message->message_length = DEVDRV_MAILBOX_PAYLOAD_LENGTH;
    message->feedback = DEVDRV_MAILBOX_NO_FEEDBACK;
    message->feedback_num = 0;
    message->feedback_buffer = NULL;
    message->feedback_count = 0;
    message->process_result = 0;
    message->sync_type = DEVDRV_MAILBOX_SYNC;
    message->cmd_type = 0;
    message->message_index = 0;
    message->message_pid = 0;
    message->callback = NULL;
    message->mailbox = mailbox;
    message->ts_resource = ts_resource;
    message->abandon = DEVDRV_MAILBOX_VALID_MESSAGE;

    ka_base_atomic_set(&mailbox->status, DEVDRV_MAILBOX_BUSY);
    ret = devdrv_mailbox_message_send(mailbox, message, &result);
    if (ret != 0) {
        TSDRV_PRINT_ERR("[dev_id=%u tsid=%u]:devdrv_mailbox_message_send failed.\n", devid, tsid);
        if (ret == -ETIME) {
            ret = -EAGAIN;
        }
    } else {
        ret = result;
    }
    ka_base_atomic_set(&mailbox->status, DEVDRV_MAILBOX_FREE);

    if (data->out_len != 0) {
        env_type = tsdrv_get_env_type();
        if (env_type ==  TSDRV_ENV_OFFLINE) {
            src_addr = mailbox->send_sram;
        } else {
            src_addr = mailbox->receive_sram;
        }
        ka_mm_memcpy_toio(data->out_data, src_addr, data->out_len);
    }
    ka_task_mutex_unlock(&mailbox->mlock);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_mailbox_send_sync);

#ifndef TSDRV_UT
STATIC void devdrv_mailbox_ack_proc(struct devdrv_mailbox_message *message,
    struct devdrv_mailbox_message_header *header)
{
    enum tsdrv_env_type env_type = tsdrv_get_env_type();
    /* In MC2 featrue, ts will write back hwts sqid. */
    if ((env_type == TSDRV_ENV_OFFLINE) && (header->cmd_type == DEVDRV_MAILBOX_CREATE_CQSQ_CALC)) {
        struct devdrv_normal_cqsq_mailbox *payload = (struct devdrv_normal_cqsq_mailbox *)message->mailbox->send_sram;
        message->hwts_sqid = payload->sq_index;
    }

    message->process_result = (int)header->result;
    return;
}
#endif

/* interrupt upper half for mailbox ack and feedback */
STATIC ka_irqreturn_t devdrv_mailbox_ack_irq(int irq, void *data)
{
#ifndef TSDRV_UT
    struct devdrv_mailbox_message_header *header = NULL;
    struct devdrv_mailbox_message *message = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_mailbox_dfx *mbox_dfx = NULL;
    struct devdrv_mailbox *mailbox = NULL;
    enum tsdrv_env_type env_type;
    unsigned long flags;
    u32 devid;

    ka_task_local_irq_save(flags);

    mailbox = (struct devdrv_mailbox *)data;
    ts_resource = ka_container_of(mailbox, struct tsdrv_ts_resource, mailbox);
    devid = tsdrv_get_devid_by_res(ts_resource);
    mbox_dfx = tsdrv_get_mbox_dfx_addr(devid, ts_resource->tsid);
    mbox_dfx->irq_num++;

    mailbox->irq_time = devdrv_mailbox_get_us_timestamp();
    env_type = tsdrv_get_env_type();
    message = &mailbox->message;

    if (env_type == TSDRV_ENV_OFFLINE) {
        header = (struct devdrv_mailbox_message_header *)message->mailbox->send_sram;
    } else {
        header = (struct devdrv_mailbox_message_header *)message->mailbox->receive_sram;
    }

    if (ka_base_atomic_read(&mailbox->status) == DEVDRV_MAILBOX_FREE) {
        /* record irq_num, time, cmd_type and valid when mailbox status is free */
        mbox_dfx->free_num++;
        mbox_dfx->free_time[mbox_dfx->free_num % MAILBOX_DFX_COUNT] = devdrv_mailbox_get_us_timestamp();
        mbox_dfx->free_type[mbox_dfx->free_num % MAILBOX_DFX_COUNT] = header->cmd_type;
        mbox_dfx->free_valid[mbox_dfx->free_num % MAILBOX_DFX_COUNT] = header->valid;
        ka_task_local_irq_restore(flags);
        return KA_IRQ_HANDLED;
    }

    devdrv_mailbox_ack_proc(message, header);

    ka_task_up(&message->wait);
    mbox_dfx->up_num++;

    ka_task_local_irq_restore(flags);
#endif
    return KA_IRQ_HANDLED;
}

int devdrv_send_hwts_addr_to_ts(u32 dev_id, u32 tsid, u32 local_devid, u32 host_id)
{
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct hwts_addr_info data;
    int result = 0;
    int ret;

    if ((dev_id >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM) ||
        (local_devid >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u) host_id(%u)\n", dev_id, tsid, host_id);
        return -ENODEV;
    }
    ts_resource = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);
    data.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    data.header.cmd_type = DEVDRV_MAILBOX_SEND_HWTS_INFO;
    data.header.result = 0;
    data.local_devid = local_devid;
    data.host_devid = host_id;

    ret = devdrv_mailbox_kernel_sync_no_feedback(&ts_resource->mailbox, (u8 *)&data,
                                                 sizeof(struct hwts_addr_info), &result);
    if ((ret != 0) || (result != 0)) {
        TSDRV_PRINT_ERR("send hwts addr info failed, devid(%u), tsid(%u), ret(%d), result(%d).\n",
            dev_id, tsid, ret, result);
        return -EINVAL;
    }
    return ret;
}

int devdrv_send_rdmainfo_to_ts(u32 dev_id, const u8 *buf, u32 len, int *result)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct rdma_info data;
    u32 tsid = 0;
    int ret;

    if ((len > MAX_RDMA_INFO_LEN) || (buf == NULL) || (result == NULL) || (dev_id >= TSDRV_MAX_DAVINCI_NUM)) {
        TSDRV_PRINT_ERR("[dev_id=%u]:the length(%d) of buf out of range(%d), buf = %pK, result = %pK.\n",
            dev_id, len, MAX_RDMA_INFO_LEN, buf, result);
        return -EINVAL;
    }
    ts_resource = tsdrv_get_ts_resoruce(dev_id, TSDRV_PM_FID, tsid);

    data.header.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    data.header.cmd_type = DEVDRV_MAILBOX_SEND_RDMA_INFO;
    data.header.result = 0;
    ret = memcpy_s(data.buf, sizeof(data.buf), buf, len);
    if (ret != 0) {
        TSDRV_PRINT_ERR("[dev_id=%u]:copy from buf failed, ret = %d. \n", dev_id, ret);
        return -EINVAL;
    }

    ret = devdrv_mailbox_kernel_sync_no_feedback(&ts_resource->mailbox, (u8 *)&data,
                                                 sizeof(struct devdrv_mailbox_message_header) + len, result);
    if ((ret != 0) || (*result != 0)) {
        TSDRV_PRINT_ERR("send rdma info failed,devid[%u], ret = %d, result = %d\n", dev_id, ret, *result);
        return -EINVAL;
    }
    return 0;
#endif
}
KA_EXPORT_SYMBOL_GPL(devdrv_send_rdmainfo_to_ts);

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
#ifndef EVENT_SCHED_UT

#include "dbl/uda.h"
#include "dbl/feature_loader.h"

#include "devdrv_interface.h"
#include "securec.h"
#include "esched.h"
#include "esched_msg_def.h"
#include "esched_fops.h"
#include "esched_host_msg.h"

#ifdef CFG_FEATURE_HARDWARE_SCHED
#include "esched_drv_adapt.h"
#endif

STATIC int esched_host_msg_send(u32 dev_id, struct esched_ctrl_msg *msg, u32 msg_len)
{
    int ret;
    u32 out_len;

    ret = devdrv_common_msg_send(dev_id, (void *)msg, msg_len, msg_len, &out_len, DEVDRV_COMMON_MSG_ESCHED);
    if ((ret != 0) || (msg->error_code != 0)) {
        sched_debug("Send message to device not success. (dev_id=%d; deal_code=%d; ret=%d)\n",
            dev_id, msg->error_code, ret);
        return ret != 0 ? ret : msg->error_code;
    }

    return 0;
}

int sched_query_remote_trace_msg_send(u32 chip_id,
    u32 pid, u32 gid, u32 tid, struct sched_sync_event_trace *sched_trace)
{
    struct esched_ctrl_msg msg;
    int ret;

    msg.type = ESCHED_MSG_TYPE_REMOTE_QUERY_TRACE;
    msg.error_code = 0;
    msg.trace_msg.pid = pid;
    msg.trace_msg.gid = gid;
    msg.trace_msg.tid = tid;

    ret = esched_host_msg_send(chip_id, (void *)&msg, sizeof(msg));
    if (ret == 0) {
        *sched_trace = msg.trace_msg.sched_trace;
    }

    return ret;
}

int sched_publish_event_to_remote(u32 chip_id, u32 event_src,
    const struct sched_published_event_info *event_info, struct sched_published_event_func *event_func)
{
    struct esched_ctrl_msg msg;
    int ret;

    if ((event_func->event_ack_func != NULL) || (event_func->event_finish_func != NULL)) {
#ifndef EMU_ST
        sched_warn("The callback function is not supported for remote submission.\n");
        return DRV_ERROR_NOT_SUPPORT;
#endif
    }

    msg.type = ESCHED_MSG_TYPE_REMOTE_SUBMIT;
    msg.error_code = 0;
    msg.submit_msg.event_info = *event_info;
    msg.submit_msg.pid = SCHED_CURRENT_TGID;

    if (event_info->msg_len > 0) {
        ret = memcpy_s(msg.submit_msg.msg, SCHED_MAX_EVENT_MSG_LEN, event_info->msg, event_info->msg_len);
        if (ret != 0) {
            sched_err("Failed to copy variable msg. (pid=%d; gid=%u; event_id=%u)\n",
                event_info->pid, event_info->gid, event_info->event_id);
            return ret;
        }
    }

    return esched_host_msg_send(chip_id, (void *)&msg, sizeof(msg));
}

STATIC int sched_publish_event_proxy(u32 devid, struct esched_ctrl_msg *msg)
{
    struct esched_remote_submit_msg *submit_msg = &msg->submit_msg;
    struct sched_published_event_func event_func;
    u32 chip_id = devid;
    int32_t ret;

    event_func.event_ack_func = NULL;
    event_func.event_finish_func = NULL;

    if (submit_msg->event_info.msg_len != 0) {
        submit_msg->event_info.msg = submit_msg->msg;
    } else {
        submit_msg->event_info.msg = NULL;
    }
    submit_msg->event_info.publish_timestamp = sched_get_cur_timestamp();

    ret = sched_publish_event_para_check(&submit_msg->event_info);
    if (ret != 0) {
        return ret;
    }

    return sched_publish_event(chip_id, SCHED_PUBLISH_FORM_KERNEL, &submit_msg->event_info, &event_func);
}

STATIC int sched_query_gid_proxy(u32 devid, struct esched_ctrl_msg *msg)
{
    struct esched_remote_query_gid_msg *query_msg = &msg->query_gid_msg;

    return sched_query_local_task_gid(devid, query_msg->pid, query_msg->grp_name, &query_msg->gid);
}

int sched_query_remote_task_gid_msg_send(u32 chip_id, int pid, const char *grp_name, u32 *gid)
{
    struct esched_ctrl_msg msg;
    int ret;

    msg.type = ESCHED_MSG_TYPE_REMOTE_QUERY_GID;
    msg.error_code = 0;
    msg.query_gid_msg.pid = pid;
    ret = strcpy_s(msg.query_gid_msg.grp_name, EVENT_MAX_GRP_NAME_LEN, grp_name);
    if (ret != 0) {
        sched_err("Failed to invoke strcpy_s. (chip_id=%u; ret=%d)\n", chip_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = esched_host_msg_send(chip_id, (void *)&msg, sizeof(msg));
    if (ret == 0) {
        *gid = msg.query_gid_msg.gid;
    }

    return ret;
}

#ifdef CFG_FEATURE_HARDWARE_SCHED
STATIC int esched_drv_remote_config_pid(u32 dev_id, u32 msg_type, u32 host_ctrl_pid, u32 pid_type, u32 pid)
{
    struct esched_ctrl_msg msg;
    int ret;

    msg.type = msg_type;
    msg.error_code = 0;
    msg.host_pid_msg.vfid = 0;
    msg.host_pid_msg.host_ctrl_pid = host_ctrl_pid;
    msg.host_pid_msg.pid_type = pid_type;
    msg.host_pid_msg.pid = pid;

    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to invoke the esched_host_msg_send. (dev_id=%u; pid=%u; error_code=%d; ret=%d)\n",
                  dev_id, pid, msg.error_code, ret);
        return -EFAULT;
    }

    return 0;
}

int esched_drv_remote_add_pid(u32 dev_id, u32 host_ctrl_pid, u32 pid_type, u32 pid)
{
    return esched_drv_remote_config_pid(dev_id, ESCHED_MSG_TYPE_ADD_HOST_PID, host_ctrl_pid, pid_type, pid);
}

int esched_drv_remote_del_pid(u32 dev_id, u32 host_ctrl_pid, u32 pid)
{
    return esched_drv_remote_config_pid(dev_id, ESCHED_MSG_TYPE_DEL_HOST_PID, host_ctrl_pid, 0, pid);
}

int esched_drv_remote_add_pool(u32 dev_id, u32 cpu_type)
{
    struct esched_ctrl_msg msg;
    int ret;

    msg.type = ESCHED_MSG_TYPE_ADD_POOL;
    msg.error_code = 0;
    msg.pool_msg.cpu_type = cpu_type;

    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to send msg. (dev_id=%u; error_code=%x; ret=%d)\n", dev_id, msg.error_code, ret);
        return -EFAULT;
    }

    return 0;
}

int esched_drv_remote_get_cpu_mbid(u32 dev_id, u32 cpu_type, u32 *mb_id, u32 *wait_mb_id)
{
    struct esched_ctrl_msg msg;
    int ret;

    msg.type = ESCHED_MSG_TYPE_GET_CPU_MBID;
    msg.error_code = 0;
    msg.mbid_msg.cpu_type = cpu_type;

    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to send msg. (dev_id=%u; error_code=%x; ret=%d)\n",
            dev_id, msg.error_code, ret);
        return -EFAULT;
    }

    *mb_id = msg.mbid_msg.mb_id;
    *wait_mb_id = msg.mbid_msg.wait_mb_id;

    return 0;
}

int esched_drv_remote_config_intr(u32 dev_id, u32 irq)
{
    struct esched_ctrl_msg msg;
    int ret;

    msg.type = ESCHED_MSG_TYPE_CONF_INTR;
    msg.error_code = 0;
    msg.intr_msg.irq = irq;

    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_debug("Invoke the esched_host_msg_send not success. "
            "(dev_id=%u; irq=%u; deal_code=%d; ret=%d)\n", dev_id, irq, msg.error_code, ret);
        return -EFAULT;
    }

    return 0;
}

int esched_drv_remote_add_mb(u32 dev_id, u32 vf_id)
{
    struct esched_ctrl_msg msg;
    int ret;

    msg.type = ESCHED_MSG_TYPE_ADD_MB;
    msg.error_code = 0;
    msg.mb_msg.vf_id = vf_id;

    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to invoke the esched_host_msg_send. "
                  "(dev_id=%u; vf_id=%u; error_code=%d; ret=%d)\n", dev_id, vf_id, msg.error_code, ret);
        return -EFAULT;
    }

    return 0;
}
#endif

STATIC int esched_ctrl_msg_recv(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct esched_ctrl_msg *msg = (struct esched_ctrl_msg *)data;
    int ret;
    struct sched_numa_node *node = NULL;

    if ((data == NULL) || (real_out_len == NULL) || (in_data_len != sizeof(struct esched_ctrl_msg))) {
        sched_err("The variable data or real_out_len is invalid. "
                  "(devid=%u; in_data_len=%u; out_data_len=%u)\n", devid, in_data_len, out_data_len);
        return -EINVAL;
    }
    
    node = esched_dev_get(devid);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }    
    *real_out_len = sizeof(*msg);

    switch (msg->type) {
        case ESCHED_MSG_TYPE_REMOTE_SUBMIT:
            ret = sched_publish_event_proxy(devid, msg);
            break;
        case ESCHED_MSG_TYPE_REMOTE_QUERY_GID:
            ret = sched_query_gid_proxy(devid, msg);
            break;
        default:
            ret = -EINVAL;
            break;
    }

    if ((ret != 0) && !((msg->type == ESCHED_MSG_TYPE_REMOTE_QUERY_GID) && (ret == DRV_ERROR_UNINIT))) {
        sched_err("Failed to invoke the sched_publish_event_proxy. (devid=%u; type=%d; ret=%d)\n",
            devid, msg->type, ret);
    }

    msg->error_code = ret;
    esched_dev_put(node);
    return 0;
}

struct devdrv_common_msg_client esched_host_msg_client = {
    .type = DEVDRV_COMMON_MSG_ESCHED,
    .common_msg_recv = esched_ctrl_msg_recv,
};

STATIC int esched_init_instance(u32 dev_id)
{
    struct sched_dev_ops ops;
    int ret;

#ifdef CFG_FEATURE_HARDWARE_SCHED
    esched_setup_dev_hw_ops(&ops);
#else
    esched_setup_dev_soft_ops(&ops);
#endif

    ret = esched_create_dev(dev_id, &ops);
    if (ret != 0) {
        sched_err("Create dev failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

    ret = module_feature_auto_init_dev(dev_id);
    if (ret != 0) {
#ifndef EMU_ST
        esched_destroy_dev(dev_id);
        sched_err("module feature init failed. (dev_id=%u)\n", dev_id);
        return ret;
#endif
    }

    return 0;
}

STATIC int esched_uninit_instance(u32 dev_id)
{
    module_feature_auto_uninit_dev(dev_id);
    esched_destroy_dev(dev_id);

    return 0;
}

#define ESCHED_HOST_NOTIFIER "esched_host"
static int esched_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= SCHED_MAX_CHIP_NUM) {
        sched_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    switch (action) {
        case UDA_INIT:
            ret = esched_init_instance(udevid);
            break;
        case UDA_UNINIT:
            ret = esched_uninit_instance(udevid);
            break;
#if defined(CFG_FEATURE_HARDWARE_MIA)
        case UDA_TO_MIA:
            esched_drv_reset_phy_dev(udevid);
            sched_info("Enable dev_id %u sriov\n", udevid);
            break;
        case UDA_TO_SIA:
            esched_drv_restore_phy_dev(udevid);
            sched_info("Disable dev_id %u sriov\n", udevid);
            break;
#endif
        default:
            /* Ignore other actions. */
            return 0;
    }

    sched_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

int esched_client_init(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(ESCHED_HOST_NOTIFIER, &type, UDA_PRI3, esched_host_notifier_func);
    if (ret != 0) {
        sched_err("Failed to register client. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_register_common_msg_client(&esched_host_msg_client);
    if (ret != 0) {
        (void)uda_notifier_unregister(ESCHED_HOST_NOTIFIER, &type);
        sched_err("Failed to invoke the devdrv_register_common_msg_client. (type=%d)\n", esched_host_msg_client.type);
        return ret;
    }

    return 0;
}

void esched_client_uninit(void)
{
    struct uda_dev_type type;
    (void)devdrv_unregister_common_msg_client(0, &esched_host_msg_client);
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(ESCHED_HOST_NOTIFIER, &type);
}

#else
int tmp_esched_host_msg()
{
    return 0;
}

int tmp_esched_mia_init()
{
    return 0;
}
#endif

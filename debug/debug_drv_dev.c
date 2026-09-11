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
 * Create: 2023-09-05
 */

#ifdef CFG_SOC_PLATFORM_CLOUD
#include "trs_chan.h"
#endif
#include "uda.h"
#include "devdrv_functional_cqsq.h"
#include "debug_dma.h"
#include "securec.h"
#include "trs_chan_kernel_interface.h"
#include "ascend_kernel_hal.h"

#include "ka_task_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_base_pub.h"
#include "ka_driver_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "ka_dfx_pub.h"
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_errno_pub.h"

#define DEBUG_SQCQ_DEPTH 4
#define DEBUG_SQ_BUF_LEN 64
#define DEBUG_CQ_BUF_LEN 64
#define DEBUG_NOTIFIER "debug"
#define DEBUG_DEFAULT_PID (-1)
#define DEBUG_TIMEOUT 10000
#define DEBUG_ENABLE 1
#define DEBUG_DISABLE 0
#define REQ_ENABLE_ID 0
#define REQ_DISABLE_ID 1
#define DEVICE_NUM_MAX 16
#define PARTITION_CQ_DST_OFFSET 8

#define CQ_QUEUE_DEPTH 10
#define HZ_TO_MS 1000

#define ERR_DEV_ID (-1)
#define ERR_QUEUE_FULL (-2)
#define ERR_WAIT_TIMEOUT (-3)
#define ERR_UNRECOGNIZED_CMD (-4)
#define ERR_INVALID_PARAM (-5)
#define ERR_SWITCH_CLOSE (-6)

#define TD_PRINT_ERR(fmt, ...) ka_dfx_printk(KERN_ERR "[ts_debug][ERROR]<%s:%d> " \
    fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define TD_PRINT_INFO(fmt, ...) ka_dfx_printk(KERN_INFO "[ts_debug][INFO]<%s:%d> " \
    fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

DECLARE_WAIT_QUEUE_HEAD(g_wq);

static u32 g_tsid = 0;
static int isUdaRegister = 0;

struct debug_cq_report {
    u8 phase;
    u8 reserved[3]; // 4字节对齐
    u16 sq_index;
    u16 sq_head;
    u32 cmd_type;
    u32 return_val;
    u32 msg_id;
    u8 payload[44]; // 填充到64字节
};

#ifndef CFG_SOC_PLATFORM_CLOUD
struct debug_cq_report_310P {
    u32 cmd_type;
    u32 return_val;
    u32 msg_id;
    u8 payload[44]; // 填充到64字节,与debug_cq_report保持一致
};
#endif

static struct debug_cq_report g_cq_queue[CQ_QUEUE_DEPTH];

static struct devdrv_func_sqcq_alloc_para_in para_in[DEVICE_NUM_MAX];
static struct devdrv_func_sqcq_alloc_para_out para_out[DEVICE_NUM_MAX];
static struct devdrv_func_sqcq_free_para para[DEVICE_NUM_MAX];

struct sqcq_chn_info {
    int enable_debug;
    int chn_id;
    int cq_head;
    int cq_tail;
    int pid;
    int debugger_pid;
    int isChannelCreate;
    ka_mutex_t mtx;
    struct debug_cq_report cq_queue[CQ_QUEUE_DEPTH];
};

static struct sqcq_chn_info g_sqcq_chn_info_list[DEVICE_NUM_MAX]; // phy_dev_id as index

static void cq_queue_info_init(void)
{
    int i;
    for (i = 0; i < DEVICE_NUM_MAX; ++i) {
        struct sqcq_chn_info *info = &g_sqcq_chn_info_list[i];
        info->enable_debug = 0;
        info->cq_head = 0;
        info->cq_tail = 0;
        info->pid = DEBUG_DEFAULT_PID;
        info->debugger_pid = DEBUG_DEFAULT_PID;
        info->isChannelCreate = 0;
        ka_task_mutex_init(&info->mtx);
    }
    isUdaRegister = 0;
}

struct ms_debug_info {
    u32 dev_id;
    int timeout;
    u32 data_len;
    int pid;
    u8 data[64]; // sq/cq payload为64字节
};

#define TS_DEBUG_MAGIC_WORD 'D'
#define CMD_SQ_SEND _KA_IOR(TS_DEBUG_MAGIC_WORD, 0, struct ms_debug_info*) // 发送SQ
#define CMD_CQ_RECV _KA_IOWR(TS_DEBUG_MAGIC_WORD, 1, struct ms_debug_info*) // 读取CQ
#define CMD_GM_COPY _KA_IOR(TS_DEBUG_MAGIC_WORD, 2, struct ms_debug_info*) // 读写GM
#define CMD_DEV_REGISTER _KA_IOR(TS_DEBUG_MAGIC_WORD, 3, struct ms_debug_info*) // 注册device id
#define CMD_QUIT _KA_IOR(TS_DEBUG_MAGIC_WORD, 4, struct ms_debug_info *)         // 退出debug模式

#define SQ_SEND_INFO_PAYLOAD_SIZE 48
struct debug_send_info {
    uint32_t req_id;
    uint8_t is_return;
    uint8_t reserved[3]; // 4字节对齐
    uint32_t msg_id;
    uint32_t data_len;
    uint8_t params[SQ_SEND_INFO_PAYLOAD_SIZE];
};

enum debug_error_code {
    DEVICE_ALREADY_OCCUPIED = 0x10000,
    DEVICE_ID_INVALID,
};

static bool debug_cqe_is_valid(void *cqe, u32 round)
{
    struct debug_cq_report *report;
    if (cqe == NULL) {
        TD_PRINT_ERR("cqe is NULL\n");
        return false;
    }
    report = (struct debug_cq_report *)cqe;
    TD_PRINT_INFO("enter debug_cqe_is_valid phase=%u, round=%u ret=%d\n", report->phase, round,
        (report->phase == ((round + 1) & 0x1)));

    return (report->phase == ((round + 1) & 0x1));
}

static void debug_get_sq_head_in_cqe(void *cqe, u32 *sq_head)
{
    struct debug_cq_report *report;
    if ((cqe == NULL) || (sq_head == NULL)) {
        TD_PRINT_ERR("cqe or sq_head is NULL\n");
        return;
    }
    report = (struct debug_cq_report *)cqe;
    TD_PRINT_INFO("enter debug_get_sq_head_in_cqe sq_head=%u cmd_type=%u return_val=%u\n", report->sq_head,
        report->cmd_type, report->return_val);
    *sq_head = report->sq_head;
}

static int debug_cq_0_recv(struct trs_id_inst *inst, u32 cqid, void *cqe)
{
    struct debug_cq_report *cq_report;
    struct debug_cq_report *cq_dst;
    struct sqcq_chn_info *chn_info;
    errno_t ret_s;

    if ((inst == NULL) || (cqe == NULL)) {
        TD_PRINT_ERR("inst or cqe is NULL\n");
        return ERR_INVALID_PARAM;
    }

    TD_PRINT_INFO("enter debug_cq_0_recv\n");

    if (inst->devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("devid is out of range\n");
        return ERR_DEV_ID;
    }

    chn_info = &g_sqcq_chn_info_list[inst->devid];

    if (((chn_info->cq_head + 1) % CQ_QUEUE_DEPTH) == chn_info->cq_tail) {
        TD_PRINT_ERR("g_cq_queue is full\n");
        return ERR_QUEUE_FULL;
    }
    chn_info->cq_head = (chn_info->cq_head + 1) % CQ_QUEUE_DEPTH;

    cq_report = (struct debug_cq_report *)cqe;
    cq_dst = &(chn_info->cq_queue[chn_info->cq_head]);

    ret_s = memcpy_s(cq_dst, sizeof(struct debug_cq_report), cq_report, sizeof(struct debug_cq_report));
    if (ret_s != EOK) {
        TD_PRINT_ERR("memcpy_s failed, ret = %d\n", ret_s);
        return ret_s;
    }
    TD_PRINT_INFO("copy to cq_queue cq_dst->cmd_type=%u\n", cq_dst->cmd_type);
    ka_task_wake_up_interruptible(&g_wq);
    TD_PRINT_INFO("notify devid=%u cq_head=%d cq_tail=%d \n",
        inst->devid, chn_info->cq_head, chn_info->cq_tail);
    return CQ_RECV_FINISH;
}

#ifndef CFG_SOC_PLATFORM_CLOUD
static int debug_cq_0_recv_for_310P(u32 devid, u32 ts_id, uint8_t *cqe, void *sqe)
{
    (void)ts_id;

    struct debug_cq_report_310P *cq_report;
    struct debug_cq_report *cq_dst;
    struct sqcq_chn_info *chn_info;
    errno_t ret_s;

    if (cqe == NULL) {
        TD_PRINT_ERR("cqe is NULL\n");
        return ERR_INVALID_PARAM;
    }

    TD_PRINT_INFO("enter debug_cq_0_recv\n");

    if (devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("devid %u is out of range\n", devid);
        return ERR_DEV_ID;
    }

    chn_info = &g_sqcq_chn_info_list[devid];

    if (((chn_info->cq_head + 1) % CQ_QUEUE_DEPTH) == chn_info->cq_tail) {
        TD_PRINT_ERR("g_cq_queue is full\n");
        return ERR_QUEUE_FULL;
    }
    chn_info->cq_head = (chn_info->cq_head + 1) % CQ_QUEUE_DEPTH;

    cq_report = (struct debug_cq_report_310P *)cqe;
    cq_dst = &(chn_info->cq_queue[chn_info->cq_head]);

    ret_s = memcpy_s(((u8 *)cq_dst) + PARTITION_CQ_DST_OFFSET, sizeof(struct debug_cq_report_310P), cq_report,
        sizeof(struct debug_cq_report_310P));
    if (ret_s != EOK) {
        TD_PRINT_ERR("memcpy_s failed, ret = %d\n", ret_s);
        return ret_s;
    }
    TD_PRINT_INFO("copy to cq_queue cq_dst->cmd_type=%u\n", cq_dst->cmd_type);
    ka_task_wake_up_interruptible(&g_wq);
    TD_PRINT_INFO("notify devid=%u, cq_head=%d cq_tail=%d \n",
        devid, chn_info->cq_head, chn_info->cq_tail);
    return CQ_RECV_FINISH;
}
#endif

static void set_chan_create_para(struct trs_chan_para *para)
{
    para->flag = (0x1 << CHAN_FLAG_ALLOC_CQ_BIT);
    para->types.type = CHAN_TYPE_MAINT;
    para->types.sub_type = CHAN_SUB_TYPE_MAINT_DBG;
    para->cq_para.cq_depth = DEBUG_SQCQ_DEPTH;
    para->ops.cqe_is_valid = debug_cqe_is_valid;
    para->ops.get_sq_head_in_cqe = debug_get_sq_head_in_cqe;

    para->flag |= (0x1 << CHAN_FLAG_ALLOC_SQ_BIT);
    para->flag |= (0x1 << CHAN_FLAG_AUTO_UPDATE_SQ_HEAD_BIT);
    para->flag |= (0x1 << CHAN_FLAG_NOTICE_TS_BIT);
    para->sq_para.sq_depth = DEBUG_SQCQ_DEPTH;
    para->sq_para.sqe_size = DEBUG_SQ_BUF_LEN;
    para->cq_para.cqe_size = DEBUG_SQ_BUF_LEN;
    para->ops.cq_recv = debug_cq_0_recv;
    para->ops.abnormal_proc = NULL;
}

static void save_debug_mode_enable_status(u32 phy_devid, u32 req_id)
{
    if (phy_devid < DEVICE_NUM_MAX) {
        if (req_id == REQ_ENABLE_ID) {
            // 使能debug模式命令字, 驱动保存使能状态, 异常退出时发送退出debug模式命令字
            g_sqcq_chn_info_list[phy_devid].enable_debug = DEBUG_ENABLE;
        } else if (req_id == REQ_DISABLE_ID) {
            g_sqcq_chn_info_list[phy_devid].enable_debug = DEBUG_DISABLE;
        }
    }
}

static int send_sq(u32 phy_devid, struct ms_debug_info *info)
{
    int ret, chn_id;
    struct trs_id_inst id_inst;
    struct trs_chan_send_para para;
    struct debug_send_info *send_info;

    if (phy_devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("phy_devid %u is out of range\n", phy_devid);
        return ERR_DEV_ID;
    }

    chn_id = g_sqcq_chn_info_list[phy_devid].chn_id;

    id_inst.devid = phy_devid;
    id_inst.tsid = g_tsid;

    send_info = (struct debug_send_info *)info->data;
    para.sqe = (u8 *)send_info;
    para.sqe_num = 1;
    para.timeout = info->timeout;

    TD_PRINT_INFO("send_sq devid=%u, tsid=%u, req_id=%u, is_return=%u, msg_id=%u data_len=%u\n", id_inst.devid,
        id_inst.tsid, send_info->req_id, send_info->is_return, send_info->msg_id, send_info->data_len);

    ret = hal_kernel_trs_chan_send(&id_inst, chn_id, &para);
    TD_PRINT_INFO("hal_kernel_trs_chan_send chnid=%d ret=%d\n", chn_id, ret);

    if ((ret == 0) && ((send_info->req_id == REQ_ENABLE_ID) || (send_info->req_id == REQ_DISABLE_ID))) {
        // enable/disable 命令字成功，更新通道状态，绑定pid与通道号
        save_debug_mode_enable_status(phy_devid, send_info->req_id);
    }

    return ret;
}

#ifndef CFG_SOC_PLATFORM_CLOUD
static int send_sq_for_310P(u32 phy_devid, struct ms_debug_info *info)
{
    int ret;
    struct debug_send_info *sqe;

    if (phy_devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("phy_devid %u is out of range\n", phy_devid);
        return ERR_DEV_ID;
    }

    if (info == NULL) {
        TD_PRINT_ERR("ms_debug_info is NULL\n");
        return ERR_INVALID_PARAM;
    }

    sqe = (struct debug_send_info *)info->data;

    TD_PRINT_INFO("send_sq devid=%u, tsid=%u, req_id=%u, is_return=%u, msg_id=%u data_len=%u\n", phy_devid, g_tsid,
        sqe->req_id, sqe->is_return, sqe->msg_id, sqe->data_len);

    ret = devdrv_functional_sq_send(phy_devid, g_tsid, para_out[phy_devid].sq_id, (void *)sqe, DEBUG_SQ_BUF_LEN);
    TD_PRINT_INFO("devdrv_functional_sq_send chnid=%d ret=%d\n", para_out[phy_devid].sq_id, ret);

    if ((ret == 0) && ((sqe->req_id == REQ_ENABLE_ID) || (sqe->req_id == REQ_DISABLE_ID))) {
        // enable命令字成功，绑定pid与通道号
        save_debug_mode_enable_status(phy_devid, sqe->req_id);
    }

    return ret;
}
#endif

static int copy_cq_out(u32 devid, struct debug_cq_report *recv_info)
{
    struct debug_cq_report *cq_src;
    struct sqcq_chn_info *chn_info;
    errno_t ret_s;
    TD_PRINT_INFO("enter copy_cq_out\n");

    if (devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("phy_devid %u is out of range\n", devid);
        return ERR_DEV_ID;
    }

    chn_info = &g_sqcq_chn_info_list[devid];
    if (chn_info->cq_tail == chn_info->cq_head) {
        TD_PRINT_ERR("cq_tail == cq_head, no new cq msg received phy_devid=%u\n", devid);
        return ERR_QUEUE_FULL;
    }

    chn_info->cq_tail = (chn_info->cq_tail + 1) % CQ_QUEUE_DEPTH;

    cq_src = &(g_sqcq_chn_info_list[devid].cq_queue[chn_info->cq_tail]);
    TD_PRINT_INFO("before memcpy cq_src->cmd_type=%u devid=%u, cq_tail=%d\n", cq_src->cmd_type, devid,
        chn_info->cq_tail);
    ret_s = memcpy_s(recv_info, sizeof(struct debug_cq_report), cq_src, sizeof(struct debug_cq_report));
    if (ret_s != EOK) {
        TD_PRINT_ERR("memcpy_s failed, ret = %d\n", ret_s);
        return ret_s;
    }

    TD_PRINT_INFO("cq already copied out cq_head=%d, cq_tail=%d recv_info.cmd_type=%u\n", chn_info->cq_head,
        chn_info->cq_tail, recv_info->cmd_type);

    return 0;
}

static int sqcq_chn_create(u32 devid, u32 tsid)
{
    TD_PRINT_INFO("enter sqcq_chn_create devid=%u, tsid=%u\n", devid, tsid);
    struct trs_id_inst id_inst = {
        .devid = devid,
        .tsid = tsid
    };
    struct trs_chan_para para = { 0 };
    int ret;

    if (devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("invalid devid=%u\n", devid);
        return ERR_DEV_ID;
    }

    set_chan_create_para(&para);
    ret = hal_kernel_trs_chan_create(&id_inst, &para, &g_sqcq_chn_info_list[devid].chn_id);
    if (ret != 0) {
        TD_PRINT_ERR("create chan 0 fail. (devid=%u; tsid=%u)\n", devid, tsid);
        return ret;
    }
    g_sqcq_chn_info_list[devid].isChannelCreate = 1;
    TD_PRINT_INFO("sqcq_chn_create done chnid=%d\n", g_sqcq_chn_info_list[devid].chn_id);

    return ret;
}

#ifndef CFG_SOC_PLATFORM_CLOUD
static int sqcq_chn_create_for_310P(u32 devid, u32 tsid)
{
    TD_PRINT_INFO("enter sqcq_chn_create devid=%u, tsid=%u\n", devid, tsid);
    int ret;

    if (devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("invalid devid=%u\n", devid);
        return ERR_DEV_ID;
    }

    para_in[devid].type = DEBUG_CQSQ_CREATE;
    para_in[devid].sqe_size = DEBUG_SQ_BUF_LEN;
    para_in[devid].cqe_size = DEBUG_CQ_BUF_LEN;
    para_in[devid].callback = (void (*)(u32, u32, const u8 *, u8 *))debug_cq_0_recv_for_310P;

    ret = devdrv_create_functional_sqcq(devid, tsid, &para_in[devid], &para_out[devid]);
    if (ret != 0) {
        TD_PRINT_ERR("create chan 0 fail. (devid=%u; tsid=%u)\n", devid, tsid);
        return ret;
    }
    g_sqcq_chn_info_list[devid].isChannelCreate = 1;
    g_sqcq_chn_info_list[devid].chn_id = para_out[devid].sq_id;
    TD_PRINT_INFO("sqcq_chn_create done chnid=%u\n", para_out[devid].sq_id);

    return ret;
}
#endif

static int chan_destroy(void)
{
    int ret = 0;
#ifndef AOS_LLVM_BUILD
    struct uda_dev_type type;
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_unregister(DEBUG_NOTIFIER, &type);
    if (ret != 0) {
        TD_PRINT_ERR("uda_notifier_unregister failed,ret = %d\n", ret);
        return ret;
    }
    TD_PRINT_INFO("uda_notifier_unregister done\n");
    isUdaRegister = 0;
#else
    ret = debug_notifier_func(0, UDA_UNINIT);
#endif
    return ret;
}

static void sqcq_chn_destroy(u32 devid, u32 tsid)
{
    TD_PRINT_INFO("enter sqcq_chn_destroy devid=%u, tsid=%u\n", devid, tsid);
    struct trs_id_inst id_inst = {
        .devid = devid,
        .tsid = tsid
    };

    if (devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("invalid devid=%u\n", devid);
        return;
    }
    if (g_sqcq_chn_info_list[devid].isChannelCreate == 1) {
        hal_kernel_trs_chan_destroy(&id_inst, g_sqcq_chn_info_list[devid].chn_id);
    }
    g_sqcq_chn_info_list[devid].isChannelCreate = 0;
    TD_PRINT_INFO("sqcq_chn_destroy done chnid=%d\n", g_sqcq_chn_info_list[devid].chn_id);
}

#ifndef CFG_SOC_PLATFORM_CLOUD
static void sqcq_chn_destroy_for_310P(u32 devid, u32 tsid)
{
    para[devid].type = DEBUG_CQSQ_RELEASE;
    para[devid].sq_id = para_out[devid].sq_id;
    para[devid].cq_id = para_out[devid].cq_id;

    if (devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("invalid devid=%u\n", devid);
        return;
    }

    TD_PRINT_INFO("enter sqcq_chn_destroy devid=%u, tsid=%u\n", devid, tsid);
    if (g_sqcq_chn_info_list[devid].isChannelCreate == 1) {
        devdrv_destroy_functional_sqcq(devid, tsid, &para[devid]);
    }
    g_sqcq_chn_info_list[devid].isChannelCreate = 0;
    TD_PRINT_INFO("sqcq_chn_destroy done sq_id=%u, cq_id=%u\n", para[devid].sq_id, para[devid].cq_id);
}
#endif
static int debug_each_phy_device_init(u32 device_id)
{
#ifndef CFG_SOC_PLATFORM_CLOUD
    return sqcq_chn_create_for_310P(device_id, g_tsid);
#else
    return sqcq_chn_create(device_id, g_tsid);
#endif
}

static void debug_each_phy_device_uninit(u32 device_id)
{
#ifndef CFG_SOC_PLATFORM_CLOUD
    sqcq_chn_destroy_for_310P(device_id, g_tsid);
#else
    sqcq_chn_destroy(device_id, g_tsid);
#endif
}

static int debug_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;
    TD_PRINT_INFO("enter debug_notifier_func, udevid=%u, action=%d\n", udevid, (int)action);

    if (udevid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = debug_each_phy_device_init(udevid);
    } else if (action == UDA_UNINIT) {
        debug_each_phy_device_uninit(udevid);
    }

    TD_PRINT_INFO("debug_notifier_func done\n");
    return ret;
}

void send_disable_debug_mode_cmd(u32 phy_devid, int chn_id)
{
    int ret;
    errno_t ret_s;
    struct ms_debug_info info;
    struct debug_send_info send_info;
    TD_PRINT_INFO("detect app exit while debug mode enabled. send disable cmd now\n");

    info.pid = ka_task_get_current_pid();
    info.dev_id = phy_devid;
    info.timeout = DEBUG_TIMEOUT;

    send_info.req_id = REQ_DISABLE_ID; // disable debug mode
    send_info.is_return = 0;
    send_info.msg_id = 0;

    ret_s = memcpy_s(&info.data[0], sizeof(info.data), (void *)&send_info, sizeof(struct debug_send_info));
    if (ret_s != EOK) {
        TD_PRINT_ERR("memcpy_s failed, ret = %d\n", ret_s);
        return;
    }
#ifndef CFG_SOC_PLATFORM_CLOUD
    ret = send_sq_for_310P(phy_devid, &info);
#else
    ret = send_sq(phy_devid, &info);
#endif
    if (ret == 0) {
        TD_PRINT_INFO("send disable cmd for phy_devid %u succeed\n", phy_devid);
    }
}

static int wait_for_cq_arrive(u32 phy_devid, int timeout)
{
    int ret;
    struct sqcq_chn_info *chn_info;

    if (phy_devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("invalid phy_devid=%u\n", phy_devid);
        return ERR_DEV_ID;
    }

    chn_info = &g_sqcq_chn_info_list[phy_devid];
    ret = ka_task_wait_event_interruptible_timeout(g_wq, (chn_info->cq_tail != chn_info->cq_head),
        timeout * KA_HZ / HZ_TO_MS);
    TD_PRINT_INFO("wake up phy_devid=%u ret=%d cq_head=%d cq_tail=%d\n", phy_devid, ret, chn_info->cq_head,
        chn_info->cq_tail);
    return ret;
}

static int chan_create(void)
{
    int ret = 0;
#ifndef AOS_LLVM_BUILD
    struct uda_dev_type type;
    uda_davinci_near_real_entity_type_pack(&type);
    TD_PRINT_INFO("uda_davinci_near_real_entity_type_pack done\n");
    ret = uda_notifier_register(DEBUG_NOTIFIER, &type, UDA_PRI3, debug_notifier_func);
    if (ret != 0) {
        TD_PRINT_ERR("uda_notifier_register failed ret=%d\n", ret);
        return ret;
    }
    TD_PRINT_INFO("uda_notifier_register done ret=%d\n", ret);
    isUdaRegister = 1;
#else
    TD_PRINT_INFO("skip uda_notifier_register\n");
    ret = debug_notifier_func(0, UDA_INIT);
#endif
    return ret;
}

static int register_device_id(u32 phy_devid, u32 ts_id, int pid)
{
    ka_struct_pid_t *pid_struct = NULL;
    if (phy_devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("invalid phy_devid=%u\n", phy_devid);
        return ERR_DEV_ID;
    }
    // 设备未注册pid
    if (g_sqcq_chn_info_list[phy_devid].debugger_pid == DEBUG_DEFAULT_PID) {
        g_sqcq_chn_info_list[phy_devid].debugger_pid = pid;
        TD_PRINT_INFO("devid %u is free. now register for pid %d\n", phy_devid, pid);
        return 0;
    }
    // 设备已注册pid，检查该pid是否还存在
    pid_struct = ka_task_find_get_pid(g_sqcq_chn_info_list[phy_devid].debugger_pid);
    if (pid_struct == NULL) {
        // 释放资源
        TD_PRINT_INFO("Device %d is free. pid=%d is not found.\n", phy_devid,
            g_sqcq_chn_info_list[phy_devid].debugger_pid);
        if (g_sqcq_chn_info_list[phy_devid].enable_debug == 1) {
            send_disable_debug_mode_cmd(phy_devid, g_sqcq_chn_info_list[phy_devid].chn_id);
            g_sqcq_chn_info_list[phy_devid].enable_debug = 0;
            chan_destroy();
        }
        g_sqcq_chn_info_list[phy_devid].cq_head = 0;
        g_sqcq_chn_info_list[phy_devid].cq_tail = 0;
        g_sqcq_chn_info_list[phy_devid].pid = DEBUG_DEFAULT_PID;
        TD_PRINT_INFO("phy_dev_id=%u released\n", phy_devid);
        // 注册新pid
        g_sqcq_chn_info_list[phy_devid].debugger_pid = pid;
        chan_create();
        TD_PRINT_INFO("devid %u is free. now register for pid %d\n", phy_devid, pid);
        return 0;
    }
    // 设备被占用，注册失败
    TD_PRINT_ERR("devid %u has been occupied by debugger pid %d,current pid %d\n", phy_devid,
        g_sqcq_chn_info_list[phy_devid].debugger_pid, pid);
    return DEVICE_ALREADY_OCCUPIED;
}

static int check_process(u32 phy_devid, int pid)
{
    if (phy_devid >= DEVICE_NUM_MAX) {
        TD_PRINT_ERR("invalid phy_devid=%u\n", phy_devid);
        return ERR_DEV_ID;
    }

    if (g_sqcq_chn_info_list[phy_devid].debugger_pid != pid) {
        TD_PRINT_ERR("devid %u has been occupied by pid %d\n", phy_devid, g_sqcq_chn_info_list[phy_devid].debugger_pid);
        return DEVICE_ALREADY_OCCUPIED;
    }
    return 0;
}

static int debug_drv_open(ka_inode_t *a, ka_file_t *fd)
{
    TD_PRINT_INFO("misc file open pid=%d\n", ka_task_get_current_pid());
    return 0;
}

static int debug_drv_release(ka_inode_t *a, ka_file_t *fd)
{
    int i;
    int ret;
    TD_PRINT_INFO("misc file release pid=%d\n", ka_task_get_current_pid());
    for (i = 0; i < DEVICE_NUM_MAX; ++i) {
        TD_PRINT_INFO("debugger_pid = %d\n", g_sqcq_chn_info_list[i].debugger_pid);
        if (g_sqcq_chn_info_list[i].debugger_pid == ka_task_get_current_pid()) {
            if (g_sqcq_chn_info_list[i].enable_debug == 1) {
                send_disable_debug_mode_cmd(i, g_sqcq_chn_info_list[i].chn_id);
                g_sqcq_chn_info_list[i].enable_debug = 0;
            }
            if (g_sqcq_chn_info_list[i].isChannelCreate == 1 || isUdaRegister == 1) {
                ret = chan_destroy();
            }
            g_sqcq_chn_info_list[i].cq_head = 0;
            g_sqcq_chn_info_list[i].cq_tail = 0;
            g_sqcq_chn_info_list[i].pid = DEBUG_DEFAULT_PID;
            g_sqcq_chn_info_list[i].debugger_pid = DEBUG_DEFAULT_PID;
            TD_PRINT_INFO("phy_dev_id=%d released\n", i);
            break;
        }
    }
    return ret;
}

static int parse_ioctl_cmd(u32 phy_devid, struct ms_debug_info *info, unsigned int cmd, unsigned long *arg)
{
    int ret;
    switch (cmd) {
        case CMD_SQ_SEND:
            TD_PRINT_INFO("CMD_SQ_SEND\n");
#ifndef CFG_SOC_PLATFORM_CLOUD
            ret = send_sq_for_310P(phy_devid, info);
#else
            ret = send_sq(phy_devid, info);
#endif
            if (ret != 0) {
                TD_PRINT_ERR("CMD_SQ_SEND send_sq failed. ret=%d\n", ret);
            }
            break;
        case CMD_CQ_RECV:
            TD_PRINT_INFO("CMD_CQ_RECV\n");
            ret = wait_for_cq_arrive(phy_devid, info->timeout);
            if (ret <= 0) {
                TD_PRINT_INFO("wait cq timed out or error\n");
                ret = ERR_WAIT_TIMEOUT;
                break;
            }
            copy_cq_out(phy_devid, (struct debug_cq_report *)&(info->data));
            ret = ka_base_copy_to_user((struct ms_debug_info *)(*arg), info, sizeof(struct ms_debug_info));
            if (ret != 0) {
                TD_PRINT_ERR("CMD_CQ_RECV ka_base_copy_to_user failed. ret=%d\n", ret);
            }
            break;
        case CMD_GM_COPY:
            TD_PRINT_INFO("CMD_GM_COPY\n");
            ret = dma_copy_sync(info->dev_id, phy_devid, g_tsid, info->pid, (struct dma_param *)&(info->data));
            break;
        case CMD_DEV_REGISTER:
            ret = 0;
            break;
        default:
            TD_PRINT_INFO("unrecognized cmd=%d\n", cmd);
            ret = ERR_UNRECOGNIZED_CMD;
            break;
    }
    return ret;
}

static int get_switch_status(void)
{
    ka_file_t *debug_switch;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
    mm_segment_t oldfs;
#endif
    loff_t pos = 0;
    int switch_status = -1;
    char readBuffer[50] = { 0 };
    int ret;

    debug_switch = ka_fs_filp_open("/proc/debug_switch", O_RDONLY, 0);
    if (KA_IS_ERR(debug_switch)) {
        TD_PRINT_ERR("/proc/debug_switch doesn't exist,please check if debug_switch has been loaded\n");
        return -ENOENT;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    ssize_t bytes_read = ka_fs_vfs_read(debug_switch, readBuffer, sizeof(readBuffer) - 1, &pos);
#else
    ssize_t bytes_read = ka_fs_kernel_read(debug_switch, readBuffer, sizeof(readBuffer) - 1, &pos);
#endif
    if (bytes_read < 0) {
        TD_PRINT_ERR("Failed to read /proc/debug_switch\n");
        ka_fs_filp_close(debug_switch, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
        set_fs(oldfs);
#endif
        return -EIO;
    }

    readBuffer[bytes_read] = '\0';
    TD_PRINT_INFO("%s", readBuffer);
    ret = sscanf_s(readBuffer, "debug_switch_status = %d", &switch_status);
    if (ret != 1) {
        TD_PRINT_INFO("sscanf error, sccanf ret = %d\n", ret);
        switch_status = -1;
    }
    if (switch_status != 0 && switch_status != 1) {
        TD_PRINT_ERR("get debug_switch_status error\n");
        switch_status = -1;
    }
    ka_fs_filp_close(debug_switch, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
    set_fs(oldfs);
#endif
    return switch_status;
}

static int check_dev_registered(u32 phy_devid)
{
    int ret = 0;
    if (g_sqcq_chn_info_list[phy_devid].debugger_pid == DEBUG_DEFAULT_PID) {
        TD_PRINT_INFO("CMD_DEV_REGISTER\n");
        ret = register_device_id(phy_devid, g_tsid, current->tgid);
    } else if (g_sqcq_chn_info_list[phy_devid].debugger_pid != current->tgid) {
        TD_PRINT_INFO("CMD_DEV_REGISTER\n");
        ret = register_device_id(phy_devid, g_tsid, current->tgid);
    } else {
        TD_PRINT_INFO("current pid %d equal to debugger_pid %d\n", current->tgid,
                      g_sqcq_chn_info_list[phy_devid].debugger_pid);
    }
    return ret;
}

static int check_and_create_chan(unsigned int cmd, u32 phy_devid)
{
    int ret = 0;
    if (g_sqcq_chn_info_list[phy_devid].isChannelCreate == 0) {
        int status = get_switch_status();
        if (status != 1) {
            TD_PRINT_ERR("debug_switch_status = %d, channel create is forbidden\n", status);
            return ERR_SWITCH_CLOSE;
        } else {
            ret = chan_create();
        }
    }
    return ret;
}

static long debug_drv_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    u32 phy_devid, vfid;
    struct ms_debug_info info;
    if ((fd == NULL) || ((struct ms_debug_info *)arg == NULL)) {
        TD_PRINT_ERR("fd or arg is NULL\n");
        return ERR_INVALID_PARAM;
    }
    if (_IOC_TYPE(cmd) != TS_DEBUG_MAGIC_WORD) {
        TD_PRINT_ERR("invalid cmd pattern.\n");
        return ERR_INVALID_PARAM;
    }

    ret = ka_base_copy_from_user(&info, (struct ms_debug_info *)arg, sizeof(struct ms_debug_info));
    if (ret != 0) {
        TD_PRINT_ERR("ka_base_copy_from_user failed. ret=%d\n", ret);
        return ret;
    }
    if (info.dev_id >= DEVICE_NUM_MAX || info.dev_id < 0) {
        TD_PRINT_ERR("ms_debug_info.devid=%u is out of range\n", info.dev_id);
        return ERR_DEV_ID;
    }

    if (info.timeout < 0 || info.pid < 0) {
        TD_PRINT_ERR("Parameter out of range (must be >= 0), ms_debug_info:timeout=%d, pid=%d.\n",
                     info.timeout, info.pid);
        return ERR_INVALID_PARAM;
    }

    TD_PRINT_INFO("ms_debug_info: dev_id=%u timeout=%d pid=%d\n", info.dev_id, info.timeout, info.pid);

    ret = uda_devid_to_phy_devid(info.dev_id, &phy_devid, &vfid);
    if (ret != 0) {
        TD_PRINT_ERR("uda_devid_to_phy_devid failed logic_devid=%u\n", info.dev_id);
        return ret;
    }
    if (phy_devid >= DEVICE_NUM_MAX || phy_devid < 0) {
        TD_PRINT_ERR("phy_devid=%u is out of range\n", phy_devid);
        return ERR_DEV_ID;
    }

    mutex_lock(&(g_sqcq_chn_info_list[0].mtx));
    ret = check_dev_registered(phy_devid);
    mutex_unlock(&(g_sqcq_chn_info_list[0].mtx));
    if (ret != 0) {
        return ret;
    }

    ret = check_and_create_chan(cmd, phy_devid);
    if (ret != 0) {
        return ret;
    }
    ka_task_mutex_lock((ka_mutex_t *)&(g_sqcq_chn_info_list[phy_devid].mtx)); // 防止多线程调用ioctl
    ret = parse_ioctl_cmd(phy_devid, &info, cmd, &arg);
    ka_task_mutex_unlock((ka_mutex_t *)&(g_sqcq_chn_info_list[phy_devid].mtx));

    return ret;
}

static ka_file_operations_t g_debug_drv_fops = {
    .owner = KA_THIS_MODULE,
    .open = debug_drv_open,
    .release = debug_drv_release,
    .unlocked_ioctl = debug_drv_ioctl
};

static ka_miscdevice_t g_debug_drv_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "drv_debug",
    .fops = &g_debug_drv_fops,
    .mode = 0666,
};

static int ts_drv_module_init(void)
{
    int ret;
    ret = ka_driver_misc_register(&g_debug_drv_dev);
    if (ret < 0) {
        TD_PRINT_ERR("ka_driver_misc_register error\n");
        return ret;
    }
    TD_PRINT_INFO("ka_driver_misc_register succeed\n");

    cq_queue_info_init();
    return 0;
}

static void ts_drv_module_exit(void)
{
    int i;
    errno_t ret_s;

    ka_driver_misc_deregister(&g_debug_drv_dev);
    TD_PRINT_INFO("ka_driver_misc_deregister!\n");

    // 清理缓存cq
    for (i = 0; i < DEVICE_NUM_MAX; ++i) {
        if (g_sqcq_chn_info_list[i].enable_debug == 1) {
            send_disable_debug_mode_cmd(i, g_sqcq_chn_info_list[i].chn_id);
            g_sqcq_chn_info_list[i].enable_debug = 0;
        }
        g_sqcq_chn_info_list[i].cq_head = 0;
        g_sqcq_chn_info_list[i].cq_tail = 0;
        g_sqcq_chn_info_list[i].pid = DEBUG_DEFAULT_PID;
        g_sqcq_chn_info_list[i].debugger_pid = DEBUG_DEFAULT_PID;
    }
    ret_s = memset_s(&g_cq_queue[0], sizeof(struct debug_cq_report) * CQ_QUEUE_DEPTH, 0,
        sizeof(struct debug_cq_report) * CQ_QUEUE_DEPTH);
    if (ret_s != EOK) {
        TD_PRINT_ERR("memset_s failed, ret = %d\n", ret_s);
        return;
    }
}

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#ifndef PCI_VENDOR_ID_HUAKUNZHENYU
#define PCI_VENDOR_ID_HUAKUNZHENYU 0x20c6
#endif
#ifndef PCI_VENDOR_ID_CHANGJIANG
#define PCI_VENDOR_ID_CHANGJIANG 0x20e9
#endif

static const struct pci_device_id g_ts_debug_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAKUNZHENYU, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAKUNZHENYU, 0xd500), 0},
    {KA_PCI_VDEVICE(CHANGJIANG, 0xd802), 0},
    {KA_PCI_VDEVICE(CHANGJIANG, 0xd500), 0},
    {}
};
KA_MODULE_DEVICE_TABLE(pci, g_ts_debug_tbl);

ka_module_init(ts_drv_module_init);
ka_module_exit(ts_drv_module_exit);

KA_MODULE_DESCRIPTION("ts debug driver");
KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");

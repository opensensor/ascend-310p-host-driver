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

#include "hdcdrv_cmd.h"
#include "hdcdrv_sysfs.h"
#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

#include "ka_fs_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_driver_pub.h"

u32 g_chan_id = 0;
u32 g_dev_id = 0;
u32 g_fid = 0;
u32 g_session_fd = 0;
u32 g_server_type = 0;

static const char *g_hdc_ctrl_msg_status_str[HDCDRV_LINK_CTRL_MSG_STATUS_MAX] = {
    "send_succ   ",
    "send_fail   ",
    "recv_succ   ",
    "recv_fail   ",
    "wait_succ   ",
    "wait_timeout"
};

STATIC void hdcdrv_add_stat(struct hdcdrv_stats *stat_all, const struct hdcdrv_stats *stat)
{
    stat_all->tx += stat->tx;
    stat_all->tx_bytes += stat->tx_bytes;
    stat_all->rx += stat->rx;
    stat_all->rx_bytes += stat->rx_bytes;
    stat_all->tx_finish += stat->tx_finish;
    stat_all->tx_full += stat->tx_full;
    stat_all->tx_fail += stat->tx_fail;
    stat_all->rx_fail += stat->rx_fail;
    stat_all->rx_full += stat->rx_full;
    stat_all->rx_total += stat->rx_total;
    stat_all->alloc_mem_err += stat->alloc_mem_err;
}

STATIC int hdcdrv_get_dev_check(const struct hdcdrv_cmd_get_stat *cmd)
{
    if ((cmd->dev_id >= hdcdrv_get_max_support_dev()) || (cmd->dev_id < 0)) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d)\n", cmd->dev_id);
        return HDCDRV_PARA_ERR;
    }

    if (cmd->outbuf == NULL) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d)\n", cmd->dev_id);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_get_mem_pool_stat(struct hdcdrv_dev *hdc_dev, struct hdcdrv_cmd_stat_dev_service *stat)
{
    struct hdcdrv_mem_info *mem_info = NULL;
    struct hdcdrv_mem_pool *mem_pool = NULL;
    int i;

    for (i = 0; i < HDCDRV_MEM_POOL_TYPE_NUM; i++) {
        if (i == HDCDRV_MEM_POOL_TYPE_TX) {
            mem_info = &stat->tx_mem_info;
        } else {
            mem_info = &stat->rx_mem_info;
        }

        mem_pool = &hdc_dev->huge_mem_pool[i];
#ifdef CFG_FEATURE_MIRROR
        mem_info->huge_pool_size = HDCDRV_HUGE_PAGE_NUM * HDCDRV_PAGE_BLOCK_NUM;
        mem_info->huge_pool_remain_size = (unsigned int)(mem_info->huge_pool_size - mem_pool->used_block_all);
#else
        mem_info->huge_pool_size = mem_pool->size;
        mem_info->huge_pool_remain_size = (unsigned int)(mem_pool->tail - mem_pool->head);
#endif

        mem_pool = &hdc_dev->small_mem_pool[i];
        mem_info->small_pool_size = mem_pool->size;
        mem_info->small_pool_remain_size = (unsigned int)(mem_pool->tail - mem_pool->head);
    }
}

/* Both vdev_stat and dev_stat use this func to collect statistics on session status */
STATIC void hdcdrv_fill_dev_stat(const struct hdcdrv_cmd_get_stat *cmd, struct hdcdrv_cmd_stat_dev_service *stat)
{
    int i, j;
    int status;
    struct hdcdrv_service *serv = NULL;
#ifdef CFG_FEATURE_VFIO
    struct vhdch_vdev *hdc_dev = &hdc_ctrl->vdev[cmd->dev_id][cmd->fid];

    stat->s_brief.cur_alloc_long_session = hdc_dev->cur_alloc_long_session;
    stat->s_brief.cur_alloc_short_session = hdc_dev->cur_alloc_short_session;
#else
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[cmd->dev_id];
#endif

    for (j = 0; j < HDCDRV_SUPPORT_MAX_SERVICE; j++) {
        if ((cmd->service_type != -1) && (cmd->service_type != j)) {
            continue;
        }

        serv = &hdc_dev->service[j];
        stat->s_brief.accept_num += (int)serv->service_stat.accept_session_num;
        stat->s_brief.connect_num += (int)serv->service_stat.connect_session_num;
        stat->s_brief.close_num += (int)serv->service_stat.close_session_num;
        hdcdrv_add_stat(&stat->stat, &serv->data_stat);
    }

    for (i = 0; i < HDCDRV_REAL_MAX_SESSION; i++) {
        status = hdcdrv_get_session_status(&hdc_ctrl->sessions[i]);
        /* devices in the same OS share sessions. Count the number of idle sessions before check devid. */
        if (status == HDCDRV_SESSION_STATUS_IDLE) {
            stat->s_brief.total_idle_session_num++;
        }

        if (hdc_ctrl->sessions[i].dev_id != cmd->dev_id) {
            continue;
        }
        // Use vf_flag to distinguish dev_stat from vdev_stat
        if ((cmd->vf_flag == 1) && (hdc_ctrl->sessions[i].local_fid != cmd->fid)) {
            continue;
        }

        if (status == HDCDRV_SESSION_STATUS_CONN) {
            stat->s_brief.active_list[stat->s_brief.active_num++] = i;
        } else if (status == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) {
            stat->s_brief.remote_close_list[stat->s_brief.remote_close_num++] = i;
        } else if (status == HDCDRV_SESSION_STATUS_IDLE) {
            stat->s_brief.idle_list[stat->s_brief.idle_num++] = i;
        }
    }
}

STATIC struct hdcdrv_cmd_stat_dev_service* hdcdrv_alloc_stat_list(void)
{
    struct hdcdrv_cmd_stat_dev_service *stat = NULL;
    stat = (struct hdcdrv_cmd_stat_dev_service *)hdcdrv_kzalloc(sizeof(struct hdcdrv_cmd_stat_dev_service),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_SUB_MODULE_TYPE_4);
    if (stat == NULL) {
        goto STAT_ALLOC_FAIL;
    }

    stat->s_brief.active_list = (int*)hdcdrv_kvmalloc(sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION, KA_SUB_MODULE_TYPE_4);
    if (stat->s_brief.active_list == NULL) {
        goto ACTIVE_ALLOC_FAIL;
    }
    (void)memset_s(stat->s_brief.active_list, sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION, 0,
        sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION);

    stat->s_brief.remote_close_list = (int*)hdcdrv_kvmalloc(sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION,
        KA_SUB_MODULE_TYPE_4);
    if (stat->s_brief.remote_close_list == NULL) {
        goto REMOTE_ALLOC_FAIL;
    }
    (void)memset_s(stat->s_brief.remote_close_list, sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION, 0,
        sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION);

    stat->s_brief.idle_list = (int*)hdcdrv_kvmalloc(sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION, KA_SUB_MODULE_TYPE_4);
    if (stat->s_brief.idle_list == NULL) {
        goto IDLE_ALLOC_FAIL;
    }
    (void)memset_s(stat->s_brief.idle_list, sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION, 0,
        sizeof(int) * (u64)HDCDRV_REAL_MAX_SESSION);

    return stat;

IDLE_ALLOC_FAIL:
    hdcdrv_kvfree(stat->s_brief.remote_close_list, KA_SUB_MODULE_TYPE_4);
    stat->s_brief.remote_close_list = NULL;
REMOTE_ALLOC_FAIL:
    hdcdrv_kvfree(stat->s_brief.active_list, KA_SUB_MODULE_TYPE_4);
    stat->s_brief.active_list = NULL;
ACTIVE_ALLOC_FAIL:
    hdcdrv_kfree(stat, KA_SUB_MODULE_TYPE_4);
    stat = NULL;
STAT_ALLOC_FAIL:
    return NULL;
}

STATIC void hdcdrv_free_stat_list(struct hdcdrv_cmd_stat_dev_service *stat)
{
    hdcdrv_kvfree(stat->s_brief.idle_list, KA_SUB_MODULE_TYPE_4);
    stat->s_brief.idle_list = NULL;
    hdcdrv_kvfree(stat->s_brief.remote_close_list, KA_SUB_MODULE_TYPE_4);
    stat->s_brief.remote_close_list = NULL;
    hdcdrv_kvfree(stat->s_brief.active_list, KA_SUB_MODULE_TYPE_4);
    stat->s_brief.active_list = NULL;
    hdcdrv_kfree(stat, KA_SUB_MODULE_TYPE_4);
}

STATIC long hdcdrv_get_dev_stat(struct hdcdrv_cmd_get_stat *cmd)
{
    struct hdcdrv_cmd_stat_dev_service *stat = NULL;
    struct hdcdrv_dev *hdc_dev = NULL;

    if (hdcdrv_get_dev_check(cmd) != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_get_dev_check failed. (dev_id=%d)\n", cmd->dev_id);
        return HDCDRV_PARA_ERR;
    }

    hdc_dev = &hdc_ctrl->devices[cmd->dev_id];

    if (hdc_dev->valid != HDCDRV_VALID) {
        hdcdrv_err("Device ID is invalid. (dev_id=%d)\n", cmd->dev_id);
        return HDCDRV_PARA_ERR;
    }

    stat = cmd->outbuf;

    hdcdrv_fill_dev_stat(cmd, stat);
    hdcdrv_get_mem_pool_stat(hdc_dev, stat);

    return HDCDRV_OK;
}

STATIC void hdcdrv_fill_service_stat(const struct hdcdrv_cmd_get_stat *cmd, struct hdcdrv_cmd_stat_dev_service *stat)
{
    int i, j;
    int status;
    struct hdcdrv_service *serv = NULL;

    for (j = 0; j < hdcdrv_get_max_support_dev(); j++) {
        if (hdc_ctrl->devices[j].valid != HDCDRV_VALID) {
            continue;
        }

        serv = &hdc_ctrl->devices[j].service[cmd->service_type];
        stat->s_brief.accept_num += (int)serv->service_stat.accept_session_num;
        stat->s_brief.connect_num += (int)serv->service_stat.connect_session_num;
        stat->s_brief.close_num += (int)serv->service_stat.close_session_num;
        hdcdrv_add_stat(&stat->stat, &serv->data_stat);
    }

    for (i = 0; i < HDCDRV_REAL_MAX_SESSION; i++) {
        if (cmd->service_type != hdc_ctrl->sessions[i].service_type) {
            continue;
        }
        status = hdcdrv_get_session_status(&hdc_ctrl->sessions[i]);
        if (status == HDCDRV_SESSION_STATUS_CONN) {
            stat->s_brief.active_list[stat->s_brief.active_num++] = i;
        } else if (status == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) {
            stat->s_brief.remote_close_list[stat->s_brief.remote_close_num++] = i;
        } else if ((status == HDCDRV_SESSION_STATUS_IDLE) && (cmd->service_type != 0)) {
            stat->s_brief.idle_list[stat->s_brief.idle_num++] = i;
        } else {
            continue;
        }
    }
}

STATIC long hdcdrv_get_service_stat(const struct hdcdrv_cmd_get_stat *cmd)
{
    struct hdcdrv_cmd_stat_dev_service *stat = NULL;

    if ((cmd->service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (cmd->service_type < 0) ||
        (cmd->outbuf == NULL)) {
        hdcdrv_err("Input pararmeter is error. (service_type=%d)\n", cmd->service_type);
        return HDCDRV_PARA_ERR;
    }

    stat = cmd->outbuf;

    hdcdrv_fill_service_stat(cmd, stat);

    return HDCDRV_OK;
}

STATIC long hdcdrv_get_chan_stat(struct hdcdrv_cmd_get_stat *cmd)
{
    struct hdcdrv_cmd_stat_chan stat;
    struct hdcdrv_dev *hdc_dev = NULL;
    struct hdcdrv_msg_chan *msg_chan = NULL;
    int chan_id = cmd->chan_id;
    int dev_id = cmd->dev_id;

    if (dev_id == HDCDRV_INVALID_VALUE) {
        dev_id = 0;
    }

    if ((dev_id >= hdcdrv_get_max_support_dev()) || (dev_id < 0) || (cmd->outbuf == NULL)) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d)\n", dev_id);
        return HDCDRV_PARA_ERR;
    }
    hdc_dev = &hdc_ctrl->devices[dev_id];

    if ((chan_id >= hdc_dev->msg_chan_cnt) || (chan_id < 0)) {
        hdcdrv_err("chan_id is illegal. (dev=%u; chan_id=%d; msg_chan_num=%d)\n",
            hdc_dev->dev_id, chan_id, hdc_dev->msg_chan_cnt);
        return HDCDRV_PARA_ERR;
    }

    msg_chan = hdc_dev->msg_chan[chan_id];
    if (msg_chan == NULL) {
        hdcdrv_err("Calling msg_chan is null.\n");
        return HDCDRV_SAFE_MEM_OP_FAIL;
    }

    if (memset_s((void *)&stat, sizeof(stat), 0, sizeof(stat)) != EOK) {
        hdcdrv_err("Calling memset_s failed.\n");
        return HDCDRV_SAFE_MEM_OP_FAIL;
    }

    stat.stat = msg_chan->stat;
    stat.dbg_stat = msg_chan->dbg_stat;
    stat.dma_head = msg_chan->dma_head;
    stat.rx_head = msg_chan->rx_head;
    stat.w_sq_head = msg_chan->sq_head;
    stat.submit_dma_head = msg_chan->submit_dma_head;
    (void)hdcdrv_get_w_sq_desc(msg_chan->chan, (u32 *)(&stat.w_sq_tail));
    (void)hdcdrv_get_r_sq_desc(msg_chan->chan, (u32 *)(&stat.r_sq_head));

    if (memcpy_s(cmd->outbuf, sizeof(struct hdcdrv_cmd_stat_chan), &stat,
        sizeof(struct hdcdrv_cmd_stat_chan)) != EOK) {
        hdcdrv_err("Calling memcpy_s failed.\n");
        return HDCDRV_ERR;
    }
    return HDCDRV_OK;
}
STATIC long hdcdrv_get_session_stat(const struct hdcdrv_cmd_get_stat *cmd)
{
    struct hdcdrv_cmd_stat_session stat;
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_session_fast_rx *fast_rx = NULL;
    int session_fd = cmd->session;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0) || (cmd->outbuf == NULL)) {
        hdcdrv_err("Input pararmeter is error. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];
    fast_rx = &hdc_ctrl->sessions[session_fd].fast_rx;

    if (memset_s((void *)&stat, sizeof(stat), 0, sizeof(stat)) != EOK) {
        hdcdrv_err("Calling memset_s failed.\n");
        return HDCDRV_SAFE_MEM_OP_FAIL;
    }

    stat.status = hdcdrv_get_session_status(session);
    stat.local_session = session->local_session_fd;
    stat.remote_session = session->remote_session_fd;
    stat.dev_id = session->dev_id;
    stat.service_type = session->service_type;
    stat.chan_id = (int)session->chan_id;
    stat.fast_chan_id = (int)session->fast_chan_id;
    if (fast_rx != NULL) {
        stat.pkts_in_fast_list = (fast_rx->tail + HDCDRV_BUF_MAX_CNT - fast_rx->head) % HDCDRV_BUF_MAX_CNT;
    }

    stat.pkts_in_list = (session->normal_rx.tail + HDCDRV_SESSION_RX_LIST_MAX_PKT - session->normal_rx.head) %
        HDCDRV_SESSION_RX_LIST_MAX_PKT;
    stat.stat = session->stat;
    stat.remote_close_state = session->remote_close_state;
    stat.local_close_state = session->local_close_state;
    stat.local_fid = session->local_fid;
    stat.container_id = session->container_id;
    stat.create_pid = session->create_pid;
    stat.peer_create_pid = session->peer_create_pid;
    stat.owner_pid = session->owner_pid;
    stat.timeout.send_timeout = ka_system_jiffies_to_msecs(session->timeout_jiffies.send_timeout);
    stat.timeout.fast_send_timeout = ka_system_jiffies_to_msecs(session->timeout_jiffies.fast_send_timeout);
    stat.dbg_stat = session->dbg_stat;

    if (memcpy_s(cmd->outbuf, sizeof(struct hdcdrv_cmd_stat_session), &stat,
        sizeof(struct hdcdrv_cmd_stat_session)) != EOK) {
        hdcdrv_err("Calling memcpy_s failed.\n");
        return HDCDRV_ERR;
    }

    return HDCDRV_OK;
}

STATIC ssize_t hdcdrv_fill_buf_mem_info(char *buf, u32 buf_len, const struct hdcdrv_mem_info* mem_info)
{
    int ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "\tHuge mem pool's total num: %u, remain num: %u\n"
        "\tSmall mem pool's total num: %u, remain num: %u\n\n", mem_info->huge_pool_size,
        mem_info->huge_pool_remain_size, mem_info->small_pool_size, mem_info->small_pool_remain_size);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_fill_buf_session_brief(char *buf, u32 buf_len,
    const struct hdcdrv_cmd_stat_session_brief *s_brief)
{
    int ret;
    ssize_t offset = 0;
    int i;

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "\tTotal session number: %d\t shared device num: %d\t total_idle_session_num: %d\n"
        "\tTotal active session number: %d\n"
        "\tactive session list: ", (int)HDCDRV_REAL_MAX_SESSION, (int)hdcdrv_dev_num,
        s_brief->total_idle_session_num, s_brief->active_num);
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < s_brief->active_num; i++) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "%d ", s_brief->active_list[i]);
        if (ret >= 0) {
            offset += ret;
        }
    }

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "\n\tTotal remote closed session number: %d\n"
        "\tremote closed session list: ", s_brief->remote_close_num);
    if (ret >= 0) {
        offset += ret;
    }

    for (i = 0; i < s_brief->remote_close_num; i++) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "%d ", s_brief->remote_close_list[i]);
        if (ret >= 0) {
            offset += ret;
        }
    }

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "\n\tTotal idle session number: %d\n", s_brief->idle_num);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "\n\tAccept session: %d, Connect session: %d, Close session: %d\n",
        s_brief->accept_num, s_brief->connect_num, s_brief->close_num);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "\tcurrent_alloc_long_session: %d, current_alloc_short_session: %d\n\n",
        s_brief->cur_alloc_long_session, s_brief->cur_alloc_short_session);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}


STATIC ssize_t hdcdrv_fill_buf_tx_rx(char *buf, u32 buf_len, const struct hdcdrv_stats *stat,
    enum hdc_dfx_print_type type)
{
    int ret = 0;
    ssize_t offset = 0;

    if (type == HDC_DFX_PRINT_IN_SYSFS) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "Tx  packets: %llu  Bytes: %llu\n"
            "Tx  finish: %llu\n"
            "Tx  full: %llu\n"
            "Tx  fail: %llu\n"
            "Tx alloc mem err: %llu\n\n"
            "Rx  packets: %llu  Bytes: %llu\n"
            "Rx  finish: %llu\n"
            "Rx  full: %llu\n"
            "Rx  fail: %llu\n"
            "Rx  user recv cnt: %llu\n\n",
            stat->tx, stat->tx_bytes, stat->tx_finish, stat->tx_full, stat->tx_fail, stat->alloc_mem_err,
            stat->rx, stat->rx_bytes, stat->rx_finish, stat->rx_full, stat->rx_fail, stat->rx_total);
    } else {
        hdcdrv_event("Tx packets:%llu; Bytes:%llu; finish:%llu; full:%llu; fail:%llu; alloc mem err:%llu\n",
            stat->tx, stat->tx_bytes, stat->tx_finish, stat->tx_full, stat->tx_fail, stat->alloc_mem_err);
        hdcdrv_event("Rx packets:%llu; Bytes:%llu; finish:%llu; full:%llu; fail:%llu; user recv cnt:%llu\n",
            stat->rx, stat->rx_bytes, stat->rx_finish, stat->rx_full, stat->rx_fail, stat->rx_total);
    }

    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_fill_buf_chan_head_info(char *buf, u32 buf_len, u32 dev_id, u32 chan_id,
    const struct hdcdrv_cmd_stat_chan *chan_stat)
{
    int ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "HDC dev %u chan %u statistics:\n"
        "\tTx      w_sq head : %d (send sq, remote read)\n"
        "\t        w_sq tail : %d (send sq, local write)\n"
        "\t        r_sq head : %d (recv sq, local read)\n"
        "\tRx       dma_head : %d (dma completed index)\n"
        "\t          rx_head : %d (dispatch task received index)\n"
        "\t  submit_dma_head : %d (dma request_index)\n\n",
        dev_id, chan_id, chan_stat->w_sq_head, chan_stat->w_sq_tail,
        chan_stat->r_sq_head, chan_stat->dma_head, chan_stat->rx_head, chan_stat->submit_dma_head);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_fill_buf_session_head_info(char *buf, u32 buf_len, const struct hdcdrv_cmd_stat_session *stat,
    enum hdc_dfx_print_type type)
{
    int ret = 0;
    ssize_t offset = 0;

    if (type == HDC_DFX_PRINT_IN_SYSFS) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "HDC session %d statistics:\n"
            "Device id: %d\n"
            "Service type: %d \n"
            "Trans chan: %d, Fast chan: %d\n"
            "Session status: %d\n"
            "Local close state: %d, remote close state: %d\n"
            "local_fid: %u, container_id:%u, create_pid:%llu, peer_create_pid:%llu, owner_pid:%llu\n"
            "Remote session: %d\n\n"
            "Session rx_list cnt: %d, fast rx_list cnt: %d, work_cancel_cnt: %d\n\n",
            stat->local_session, stat->dev_id, stat->service_type, stat->chan_id, stat->fast_chan_id,
            stat->status, stat->local_close_state, stat->remote_close_state,
            stat->local_fid, stat->container_id, stat->create_pid, stat->peer_create_pid, stat->owner_pid,
            stat->remote_session, stat->pkts_in_list, stat->pkts_in_fast_list, stat->work_cancel_cnt);
    } else {
        hdcdrv_event("HDC session:%d; devid:%d; server:%d; trans chan:%d; fast chan:%d; status:%d\n",
            stat->local_session, stat->dev_id, stat->service_type, stat->chan_id, stat->fast_chan_id, stat->status);
        hdcdrv_event("l_close:%d; r_close:%d; l_fid:%u; container_id:%u; c_pid:%llu; p_pid:%llu; o_pid:%llu\n",
            stat->local_close_state, stat->remote_close_state, stat->local_fid, stat->container_id,
            stat->create_pid, stat->peer_create_pid, stat->owner_pid);
        hdcdrv_event("remote session:%d; rx_list cnt:%d; fast rx_list cnt:%d; work_cancel_cnt:%d\n",
            stat->remote_session, stat->pkts_in_list, stat->pkts_in_fast_list, stat->work_cancel_cnt);
    }

    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_fill_buf_session_chan_info(char *buf, u32 buf_len, const struct hdcdrv_dbg_stats *stat,
    enum hdc_dfx_print_type type)
{
    int ret = 0;
    ssize_t offset = 0;

    if (type == HDC_DFX_PRINT_IN_SYSFS) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "msg_chan_send: %llu  \trx_msg_notify_task2: %llu\n"
            "rx_msg_notify_task4: %llu  \trx_msg_notify_task5: %llu\n"
            "rx_msg_notify_task6: %llu  \trx_msg_notify_task7: %llu\n"
            "rx_msg_notify_task8: %llu  \trx_msg_notify_task9: %llu\n"
            "normal_dma_copy: %llu  \trx_msg_callback1: %llu\n"
            "rx_msg_callback3: %llu  \tmsg_chan_recv_task5: %llu\n"
            "msg_chan_recv_task6: %llu  \tmsg_chan_recv_task7: %llu\n"
            "tx_finish_notify_task5: %llu  \tx_finish_notify_task7: %llu\n"
            "tx_finish_notify_task8: %llu  \trecv_data_times: %llu\n"
            "hdcdrv_wait_mem_normal: %llu  \thdcdrv_wait_mem_fifo_full: %llu\n",
            stat->hdcdrv_msg_chan_send1, stat->hdcdrv_rx_msg_notify_task2,
            stat->hdcdrv_rx_msg_notify_task4, stat->hdcdrv_rx_msg_notify_task5,
            stat->hdcdrv_rx_msg_notify_task6, stat->hdcdrv_rx_msg_notify_task7,
            stat->hdcdrv_rx_msg_notify_task8, stat->hdcdrv_rx_msg_notify_task9,
            stat->hdcdrv_normal_dma_copy1, stat->hdcdrv_rx_msg_callback1,
            stat->hdcdrv_rx_msg_callback3, stat->hdcdrv_msg_chan_recv_task5,
            stat->hdcdrv_msg_chan_recv_task6, stat->hdcdrv_msg_chan_recv_task7,
            stat->hdcdrv_tx_finish_notify_task5, stat->hdcdrv_tx_finish_notify_task7,
            stat->hdcdrv_tx_finish_notify_task8, stat->hdcdrv_recv_data_times,
            stat->hdcdrv_wait_mem_normal, stat->hdcdrv_wait_mem_fifo_full);
    } else {
        hdcdrv_event("send:%llu; RX task2:%llu; task4:%llu; task5:%llu; task6:%llu; task7:%llu; task8:%llu; task9:%llu\n",
            stat->hdcdrv_msg_chan_send1, stat->hdcdrv_rx_msg_notify_task2, stat->hdcdrv_rx_msg_notify_task4,
            stat->hdcdrv_rx_msg_notify_task5, stat->hdcdrv_rx_msg_notify_task6, stat->hdcdrv_rx_msg_notify_task7,
            stat->hdcdrv_rx_msg_notify_task8, stat->hdcdrv_rx_msg_notify_task9);
        hdcdrv_event("dma_copy:%llu; rx_cb1:%llu; rx_cb3:%llu; recv_task5:%llu; recv_task6:%llu; recv_task7:%llu\n",
            stat->hdcdrv_normal_dma_copy1, stat->hdcdrv_rx_msg_callback1, stat->hdcdrv_rx_msg_callback3,
            stat->hdcdrv_msg_chan_recv_task5, stat->hdcdrv_msg_chan_recv_task6, stat->hdcdrv_msg_chan_recv_task7);
        hdcdrv_event("tx_task5:%llu; tx_task7:%llu; tx_task8:%llu; recv_times:%llu; wait_normal:%llu; fifo_full:%llu\n",
            stat->hdcdrv_tx_finish_notify_task5, stat->hdcdrv_tx_finish_notify_task7, stat->hdcdrv_tx_finish_notify_task8,
            stat->hdcdrv_recv_data_times, stat->hdcdrv_wait_mem_normal, stat->hdcdrv_wait_mem_fifo_full);
    }

    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

ssize_t hdcdrv_fill_buf_chan_info(char *buf, u32 buf_len, const struct hdcdrv_dbg_stats *stat)
{
    int ret;
    ssize_t offset = 0;

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "msg_chan_send1           :%llu\n"
        "msg_chan_send2           :%llu\n"
        "rx_msg_notify            :%llu\n"
        "rx_msg_notify_task_check1:%llu\n"
        "rx_msg_notify_task_check2:%llu\n"
        "rx_msg_task_delay_over2ms:%llu\n"
        "rx_msg_task_delay_over4ms:%llu\n"
        "rx_msg_task_delay_new    :%llu\n"
        "rx_msg_task_delay_max    :%llu\n"
        "rx_msg_notify_task1      :%llu\n"
        "rx_msg_notify_task2      :%llu\n"
        "rx_msg_notify_task3      :%llu\n"
        "rx_msg_notify_task4      :%llu\n"
        "rx_msg_notify_task5      :%llu\n"
        "rx_msg_notify_task6      :%llu\n"
        "rx_msg_notify_task7      :%llu\n"
        "rx_msg_notify_task8      :%llu\n"
        "rx_msg_notify_task9      :%llu\n"
        "normal_dma_copy          :%llu\n"
        "rx_msg_callback1         :%llu\n"
        "rx_msg_callback2         :%llu\n"
        "rx_msg_callback3         :%llu\n"
        "rx_msg_task_check1       :%llu\n"
        "rx_msg_task_check2       :%llu\n"
        "msg_chan_recv_task1      :%llu\n"
        "msg_chan_recv_task2      :%llu\n"
        "msg_chan_recv_task3      :%llu\n"
        "msg_chan_recv_task4      :%llu\n"
        "msg_chan_recv_task5      :%llu\n"
        "msg_chan_recv_task6      :%llu\n"
        "msg_chan_recv_task7      :%llu\n"
        "msg_chan_recv_task8      :%llu\n"
        "tx_finish_notify         :%llu\n"
        "tx_finish_task_check1    :%llu\n"
        "tx_finish_task_check2    :%llu\n"
        "tx_finish_task_over2ms   :%llu\n"
        "tx_finish_task_over4ms   :%llu\n"
        "tx_finish_task_new       :%llu\n"
        "tx_finish_task_max       :%llu\n"
        "tx_finish_notify_task1   :%llu\n"
        "tx_finish_notify_task2   :%llu\n"
        "tx_finish_notify_task3   :%llu\n"
        "tx_finish_notify_task4   :%llu\n"
        "tx_finish_notify_task5   :%llu\n"
        "tx_finish_notify_task6   :%llu\n"
        "tx_finish_notify_task7   :%llu\n"
        "tx_finish_notify_task8   :%llu\n"
        "tx_finish_notify_session :%llu\n"
        "tx_finish_notify_sq_head :%llu\n"
        "mem_avail1               :%llu\n"
        "recv_data_times          :%llu\n",
        stat->hdcdrv_msg_chan_send1, stat->hdcdrv_msg_chan_send2,
        stat->hdcdrv_rx_msg_notify1, stat->hdcdrv_rx_msg_notify_task_check1,
        stat->hdcdrv_rx_msg_notify_task_check2, stat->hdcdrv_rx_msg_notify_task_delay_over2ms,
        stat->hdcdrv_rx_msg_notify_task_delay_over4ms, stat->hdcdrv_rx_msg_notify_task_delay_new,
        stat->hdcdrv_rx_msg_notify_task_delay_max, stat->hdcdrv_rx_msg_notify_task1, stat->hdcdrv_rx_msg_notify_task2,
        stat->hdcdrv_rx_msg_notify_task3, stat->hdcdrv_rx_msg_notify_task4, stat->hdcdrv_rx_msg_notify_task5,
        stat->hdcdrv_rx_msg_notify_task6, stat->hdcdrv_rx_msg_notify_task7, stat->hdcdrv_rx_msg_notify_task8,
        stat->hdcdrv_rx_msg_notify_task9, stat->hdcdrv_normal_dma_copy1, stat->hdcdrv_rx_msg_callback1,
        stat->hdcdrv_rx_msg_callback2, stat->hdcdrv_rx_msg_callback3, stat->hdcdrv_rx_msg_task_check1,
        stat->hdcdrv_rx_msg_task_check2, stat->hdcdrv_msg_chan_recv_task1, stat->hdcdrv_msg_chan_recv_task2,
        stat->hdcdrv_msg_chan_recv_task3, stat->hdcdrv_msg_chan_recv_task4, stat->hdcdrv_msg_chan_recv_task5,
        stat->hdcdrv_msg_chan_recv_task6, stat->hdcdrv_msg_chan_recv_task7, stat->hdcdrv_msg_chan_recv_task8,
        stat->hdcdrv_tx_finish_notify1, stat->hdcdrv_tx_finish_task_check1,
        stat->hdcdrv_tx_finish_task_check2, stat->hdcdrv_tx_finish_notify_task_delay_over2ms,
        stat->hdcdrv_tx_finish_notify_task_delay_over4ms, stat->hdcdrv_tx_finish_notify_task_delay_new,
        stat->hdcdrv_tx_finish_notify_task_delay_max, stat->hdcdrv_tx_finish_notify_task1,
        stat->hdcdrv_tx_finish_notify_task2, stat->hdcdrv_tx_finish_notify_task3, stat->hdcdrv_tx_finish_notify_task4,
        stat->hdcdrv_tx_finish_notify_task5, stat->hdcdrv_tx_finish_notify_task6, stat->hdcdrv_tx_finish_notify_task7,
        stat->hdcdrv_tx_finish_notify_task8, stat->hdcdrv_tx_finish_notify_session_no_update,
        stat->hdcdrv_tx_finish_notify_sq_head_no_update, stat->hdcdrv_mem_avail1, stat->hdcdrv_recv_data_times);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_chan_id(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    int ret;

    (void)attr;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "chan id %u\n", g_chan_id);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_session_fd(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    int ret;

    (void)attr;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "session id %u\n", g_session_fd);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_server_type(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    int ret;

    (void)attr;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "server type %u\n",
        g_server_type);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_dev_id(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    int ret;

    (void)attr;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "dev id %u\n", g_dev_id);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_set_chan_id(ka_device_t *dev, ka_device_attribute_t *attr,
    const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        hdcdrv_err("Calling ka_base_kstrtou32 failed.\n");
        return -EINVAL;
    }

    if (val >= HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN) {
        hdcdrv_err("Pararmeter is invalid. (msg_chan=%u)\n", val);
        return -EINVAL;
    }

    g_chan_id = val;

    return (ssize_t)count;
}

STATIC ssize_t hdcdrv_sysfs_set_session_fd(ka_device_t *dev, ka_device_attribute_t *attr,
    const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        hdcdrv_err("Calling ka_base_kstrtou32 failed.\n");
        return -EINVAL;
    }

    if (val >= (u32)HDCDRV_REAL_MAX_SESSION) {
        hdcdrv_err("Pararmeter is invalid. (session=%u)\n", val);
        return -EINVAL;
    }

    g_session_fd = val;

    return (ssize_t)count;
}

STATIC ssize_t hdcdrv_sysfs_set_server_type(ka_device_t *dev, ka_device_attribute_t *attr,
    const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        hdcdrv_err("Calling ka_base_kstrtou32 failed.\n");
        return -EINVAL;
    }

    if (val >= HDCDRV_SUPPORT_MAX_SERVICE) {
        hdcdrv_err("Pararmeter is invalid. (server_type=%u)\n", val);
        return -EINVAL;
    }

    g_server_type = val;

    return (ssize_t)count;
}

STATIC ssize_t hdcdrv_sysfs_set_dev_id(ka_device_t *dev, ka_device_attribute_t *attr,
    const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        hdcdrv_err("Calling ka_base_kstrtou32 failed.\n");
        return -EINVAL;
    }

    if (val >= (u32)hdcdrv_get_max_support_dev()) {
        hdcdrv_err("Pararmeter is invalid. (dev_id=%u)\n", val);
        return -EINVAL;
    }

    g_dev_id = val;

    return (ssize_t)count;
}

STATIC int hdcdrv_sysfs_get_chan_inner(char *buf, u32 buf_len, u32 dev_id, u32 chan_id, ssize_t *len)
{
    struct hdcdrv_cmd_stat_chan *chan_stat = NULL;
    struct hdcdrv_cmd_get_stat cmd;
    ssize_t offset = 0;

    cmd.chan_id = (int)chan_id;
    cmd.dev_id = (int)dev_id;
    cmd.outbuf = hdcdrv_kzalloc(sizeof(struct hdcdrv_cmd_stat_chan), KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_SUB_MODULE_TYPE_4);
    if (cmd.outbuf == NULL) {
        hdcdrv_err("Calling kzalloc failed. (chan_id=%u)\n", chan_id);
        return HDCDRV_MEM_ALLOC_FAIL;
    }

    if (hdcdrv_get_chan_stat(&cmd) != HDCDRV_OK) {
        hdcdrv_err("get chan stat failed. (chan_id=%u)\n", chan_id);
        hdcdrv_kfree(cmd.outbuf, KA_SUB_MODULE_TYPE_4);
        cmd.outbuf = NULL;
        return HDCDRV_ERR;
    }

    chan_stat = (struct hdcdrv_cmd_stat_chan *)cmd.outbuf;

    offset += hdcdrv_fill_buf_chan_head_info(buf + offset, (u32)(buf_len - offset), dev_id, chan_id, chan_stat);
    offset += hdcdrv_fill_buf_tx_rx(buf + offset, (u32)(buf_len - offset), &(chan_stat->stat), HDC_DFX_PRINT_IN_SYSFS);
    offset += hdcdrv_fill_buf_chan_info(buf + offset, (u32)(buf_len - offset), &(chan_stat->dbg_stat));

    *len = offset;
    hdcdrv_kfree(cmd.outbuf, KA_SUB_MODULE_TYPE_4);
    cmd.outbuf = NULL;

    return HDCDRV_OK;
}

STATIC ssize_t hdcdrv_sysfs_get_remote_chan_stat(char *buf, u32 buf_len, u32 dev_id, u32 chan_id)
{
    ssize_t offset = 0;
    int ret;
    u32 msg_len;
    u32 out_len;
    struct hdcdrv_sysfs_ctrl_msg *msg = NULL;

    msg_len = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg) + HDCDRV_SYSFS_DATA_MAX_LEN;
    msg = (struct hdcdrv_sysfs_ctrl_msg *)hdcdrv_kvmalloc(msg_len, KA_SUB_MODULE_TYPE_4);
    if (msg == NULL) {
        hdcdrv_err("Calling kzalloc failed.\n");
        return offset;
    }

    msg->head.type = HDCDRV_CTRL_MSG_TYPE_GET_DEV_CHAN_STAT;
    msg->head.error_code = HDCDRV_OK;
    msg->head.para = chan_id;
    msg->head.msg_len = msg_len;

    ret = (int)hdcdrv_ctrl_msg_send(dev_id, (void *)msg, msg_len, msg_len, &out_len);
    if ((ret == 0) && (msg->head.msg_len < buf_len - offset)) {
        msg->data[HDCDRV_SYSFS_DATA_MAX_LEN - 1] = '\0';
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1), "%s\n", msg->data);
        if (ret >= 0) {
            offset += ret;
        }
    }

    hdcdrv_kvfree(msg, KA_SUB_MODULE_TYPE_4);
    msg = NULL;

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_chan_stat(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    int ret;

    (void)attr;

    ret = hdcdrv_sysfs_get_chan_inner(buf, KA_MM_PAGE_SIZE, g_dev_id, g_chan_id, &offset);
    if (ret != HDCDRV_OK) {
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "\ndevice chan stat:\n");
    if (ret >= 0) {
        offset += ret;
    }

    offset += hdcdrv_sysfs_get_remote_chan_stat(buf + offset, KA_MM_PAGE_SIZE - offset, g_dev_id, g_chan_id);

    return offset;
}
int hdcdrv_sysfs_get_session_inner(char *buf, u32 buf_len, u32 session_fd, ssize_t *len, enum hdc_dfx_print_type type)
{
    struct hdcdrv_cmd_stat_session *stat = NULL;
    struct hdcdrv_cmd_get_stat cmd = {0};
    ssize_t offset = 0;

    cmd.session = (int)session_fd;
    cmd.outbuf = hdcdrv_kzalloc(sizeof(struct hdcdrv_cmd_stat_session), KA_GFP_KERNEL | __KA_GFP_ACCOUNT,
        KA_SUB_MODULE_TYPE_4);
    if (cmd.outbuf == NULL) {
        hdcdrv_err("Calling kzalloc failed. (g_session_fd=%u)\n", session_fd);
        return HDCDRV_MEM_ALLOC_FAIL;
    }

    if (hdcdrv_get_session_stat(&cmd) != HDCDRV_OK) {
        hdcdrv_err("Get session stat failed. (g_session_fd=%u)\n", session_fd);
        hdcdrv_kfree(cmd.outbuf, KA_SUB_MODULE_TYPE_4);
        cmd.outbuf = NULL;
        return HDCDRV_ERR;
    }

    stat = (struct hdcdrv_cmd_stat_session *)cmd.outbuf;

    offset += hdcdrv_fill_buf_session_head_info(buf + offset, (u32)(buf_len - offset), stat, type);
    offset += hdcdrv_fill_buf_tx_rx(buf + offset, (u32)(buf_len - offset), &stat->stat, type);
    offset += hdcdrv_fill_buf_session_chan_info(buf + offset, (u32)(buf_len - offset), &stat->dbg_stat, type);

    *len = offset;

    hdcdrv_kfree(cmd.outbuf, KA_SUB_MODULE_TYPE_4);
    cmd.outbuf = NULL;

    return HDCDRV_OK;
}

STATIC ssize_t hdcdrv_sysfs_get_session_stat(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    int ret;
    u32 msg_len;
    u32 out_len;
    struct hdcdrv_sysfs_ctrl_msg *msg = NULL;
    struct hdcdrv_session *session = NULL;

    (void)attr;

    ret = hdcdrv_sysfs_get_session_inner(buf, KA_MM_PAGE_SIZE, g_session_fd, &offset, HDC_DFX_PRINT_IN_SYSFS);
    if (ret != HDCDRV_OK) {
        return offset;
    }
    session = &hdc_ctrl->sessions[g_session_fd];

    msg_len = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg) + HDCDRV_SYSFS_DATA_MAX_LEN;
    msg = (struct hdcdrv_sysfs_ctrl_msg *)hdcdrv_kvmalloc(msg_len, KA_SUB_MODULE_TYPE_4);
    if (msg == NULL) {
        hdcdrv_err("Calling kzalloc failed.\n");
        return offset;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "\nRemote session stat:\n");
    if (ret >= 0) {
        offset += ret;
    }

    msg->head.type = HDCDRV_CTRL_MSG_TYPE_GET_DEV_SESSION_STAT;
    msg->head.error_code = HDCDRV_OK;
    msg->head.para = (u32)session->remote_session_fd;
    msg->head.msg_len = msg_len;
    msg->head.print_type = HDC_DFX_PRINT_IN_SYSFS;

    ret = (int)hdcdrv_ctrl_msg_send((u32)session->dev_id, (void *)msg, msg_len, msg_len, &out_len);
    if ((ret == 0) && (msg->head.msg_len < KA_MM_PAGE_SIZE - offset)) {
        msg->data[HDCDRV_SYSFS_DATA_MAX_LEN - 1] = '\0';
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%s\n", msg->data);
        if (ret >= 0) {
            offset += ret;
        }
    }

    hdcdrv_kvfree(msg, KA_SUB_MODULE_TYPE_4);
    msg = NULL;
    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_server_stat(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    long ret;
    struct hdcdrv_cmd_get_stat cmd;
    struct hdcdrv_cmd_stat_dev_service *server_stat = NULL;

    (void)attr;

    cmd.service_type = (int)g_server_type;
    cmd.dev_id = (int)g_dev_id;

    cmd.outbuf = hdcdrv_alloc_stat_list();
    if (cmd.outbuf == NULL) {
        hdcdrv_err("Calling kzalloc failed. (g_server_type=%u)\n", g_server_type);
        return offset;
    }

    ret = hdcdrv_get_service_stat(&cmd);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Get server stat failed. (g_server_type=%u)\n", g_server_type);
        hdcdrv_free_stat_list((struct hdcdrv_cmd_stat_dev_service *)cmd.outbuf);
        cmd.outbuf = NULL;
        return offset;
    }

    server_stat = (struct hdcdrv_cmd_stat_dev_service *)cmd.outbuf;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "HDC service %u statistics:\n", g_server_type);
    if (ret >= 0) {
        offset += ret;
    }

    offset += hdcdrv_fill_buf_session_brief(buf + offset, KA_MM_PAGE_SIZE - offset, &server_stat->s_brief);
    offset += hdcdrv_fill_buf_tx_rx(buf + offset, KA_MM_PAGE_SIZE - offset, &server_stat->stat, HDC_DFX_PRINT_IN_SYSFS);

    hdcdrv_free_stat_list((struct hdcdrv_cmd_stat_dev_service *)cmd.outbuf);
    cmd.outbuf = NULL;

    return offset;
}


STATIC ssize_t hdcdrv_sysfs_print_dev_stat(struct hdcdrv_cmd_get_stat cmd, char *buf)
{
    ssize_t offset = 0;
    long ret;
    struct hdcdrv_cmd_stat_dev_service *dev_stat = NULL;

    cmd.outbuf = hdcdrv_alloc_stat_list();
    if (cmd.outbuf == NULL) {
        hdcdrv_err("Calling kzalloc failed. (dev_id=%d)\n", cmd.dev_id);
        return offset;
    }

    ret = hdcdrv_get_dev_stat(&cmd);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Get dev stat failed. (dev_id=%d)\n", cmd.dev_id);
        hdcdrv_free_stat_list((struct hdcdrv_cmd_stat_dev_service *)cmd.outbuf);
        cmd.outbuf = NULL;
        return offset;
    }

    dev_stat = (struct hdcdrv_cmd_stat_dev_service *)cmd.outbuf;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "HDC device %u fid %u statistics:\n", g_dev_id, g_fid);
    if (ret >= 0) {
        offset += ret;
    }

    offset += hdcdrv_fill_buf_session_brief(buf + offset, KA_MM_PAGE_SIZE - offset, &dev_stat->s_brief);
    offset += hdcdrv_fill_buf_tx_rx(buf + offset, KA_MM_PAGE_SIZE - offset, &dev_stat->stat, HDC_DFX_PRINT_IN_SYSFS);

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "\ttx mem pool:\n");
    if (ret >= 0) {
        offset += ret;
    }

    offset += hdcdrv_fill_buf_mem_info(buf + offset, KA_MM_PAGE_SIZE - offset, &dev_stat->tx_mem_info);
    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "\trx mem pool:\n");
    if (ret >= 0) {
        offset += ret;
    }
    offset += hdcdrv_fill_buf_mem_info(buf + offset, KA_MM_PAGE_SIZE - offset, &dev_stat->rx_mem_info);

    hdcdrv_free_stat_list((struct hdcdrv_cmd_stat_dev_service *)cmd.outbuf);
    cmd.outbuf = NULL;

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_dev_stat(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    struct hdcdrv_cmd_get_stat cmd;

    cmd.service_type = -1;
    cmd.dev_id = (int)g_dev_id;
    cmd.fid = 0;
    cmd.vf_flag = 0;

    return hdcdrv_sysfs_print_dev_stat(cmd, buf);
}

int hdcdrv_sysfs_ctrl_msg_get_session_stat(u32 dev_id, void *data, u32 *real_out_len)
{
    struct hdcdrv_sysfs_ctrl_msg *msg = (struct hdcdrv_sysfs_ctrl_msg *)data;
    ssize_t offset = 0;
    int ret;

    ret = hdcdrv_sysfs_get_session_inner(msg->data, HDCDRV_SYSFS_DATA_MAX_LEN, msg->head.para, &offset,
        msg->head.print_type);

    if (msg->head.print_type == HDC_DFX_PRINT_IN_SYSFS) {
        *real_out_len = (u32)(offset + sizeof(struct hdcdrv_sysfs_ctrl_msg));
        msg->head.msg_len = (u32)offset;
    } else {
        *real_out_len = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg);
    }

    return ret;
}

int hdcdrv_sysfs_ctrl_msg_get_chan_stat(u32 dev_id, void *data, u32 *real_out_len)
{
    struct hdcdrv_sysfs_ctrl_msg *msg = (struct hdcdrv_sysfs_ctrl_msg *)data;
    ssize_t offset = 0;
    int ret;

    ret = hdcdrv_sysfs_get_chan_inner(msg->data, HDCDRV_SYSFS_DATA_MAX_LEN, dev_id, msg->head.para, &offset);

    *real_out_len = (u32)(offset + sizeof(struct hdcdrv_sysfs_ctrl_msg));
    msg->head.msg_len = (u32)offset;

    return ret;
}

STATIC int hdcdrv_sysfs_get_link_inner(char *buf, u32 buf_len, u32 dev_id, u32 server_type, ssize_t *len)
{
    struct hdcdrv_link_ctrl_msg_stats *connect_msg_stat, *reply_msg_stat, *close_msg_stat;
    struct hdcdrv_dev *dev = NULL;
    int status_type, ret;
    ssize_t offset = 0;

    if (hdcdrv_dev_para_check((int)dev_id, (int)server_type) != HDCDRV_OK) {
        return HDCDRV_ERR;
    }

    dev = &hdc_ctrl->devices[dev_id];
    connect_msg_stat = &dev->service[server_type].service_stat.connect_msg_stat;
    reply_msg_stat = &dev->service[server_type].service_stat.reply_msg_stat;
    close_msg_stat = &dev->service[server_type].service_stat.close_msg_stat;

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
        "Msg type\tMsg status\tlast_err\tcount\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (status_type = HDCDRV_LINK_CTRL_MSG_SEND_SUCC; status_type < HDCDRV_LINK_CTRL_MSG_STATUS_MAX;
        status_type++) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "connect       \t%s\t%d\t\t%u\n", g_hdc_ctrl_msg_status_str[status_type],
            connect_msg_stat->last_err[status_type], connect_msg_stat->count[status_type]);
        if (ret >= 0) {
            offset += ret;
        }
    }

    for (status_type = HDCDRV_LINK_CTRL_MSG_SEND_SUCC; status_type < HDCDRV_LINK_CTRL_MSG_STATUS_MAX;
        status_type++) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "connect_reply \t%s\t%d\t\t%u\n", g_hdc_ctrl_msg_status_str[status_type],
            reply_msg_stat->last_err[status_type], reply_msg_stat->count[status_type]);
        if (ret >= 0) {
            offset += ret;
        }
    }

    for (status_type = HDCDRV_LINK_CTRL_MSG_SEND_SUCC; status_type < HDCDRV_LINK_CTRL_MSG_STATUS_MAX;
        status_type++) {
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
            "close         \t%s\t%d\t\t%u\n", g_hdc_ctrl_msg_status_str[status_type],
            close_msg_stat->last_err[status_type], close_msg_stat->count[status_type]);
        if (ret >= 0) {
            offset += ret;
        }
    }

    *len = offset;
    return HDCDRV_OK;
}

int hdcdrv_sysfs_ctrl_msg_get_connect_stat(u32 dev_id, void *data, u32 *real_out_len)
{
    struct hdcdrv_sysfs_ctrl_msg *msg = (struct hdcdrv_sysfs_ctrl_msg *)data;
    ssize_t offset = 0;
    int ret;

    ret = hdcdrv_sysfs_get_link_inner(msg->data, HDCDRV_SYSFS_DATA_MAX_LEN, dev_id, msg->head.para, &offset);

    *real_out_len = (u32)(offset + sizeof(struct hdcdrv_sysfs_ctrl_msg));
    msg->head.msg_len = (u32)offset;

    return ret;
}

STATIC ssize_t hdcdrv_sysfs_get_remote_link_stat(char *buf, u32 buf_len, u32 dev_id, u32 server_type)
{
    ssize_t offset = 0;
    int ret;
    u32 msg_len;
    u32 out_len;
    struct hdcdrv_sysfs_ctrl_msg *msg = NULL;

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
                     "HDC device %u server_type %u remote statistics:\n", dev_id, server_type);
    if (ret >= 0) {
        offset += ret;
    }

    msg_len = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg) + HDCDRV_SYSFS_DATA_MAX_LEN;
    msg = (struct hdcdrv_sysfs_ctrl_msg *)hdcdrv_kvmalloc(msg_len, KA_SUB_MODULE_TYPE_4);
    if (msg == NULL) {
        hdcdrv_err("Calling kzalloc failed. (dev_id=%u; server_type=%u)\n", dev_id, server_type);
        return offset;
    }

    msg->head.type = HDCDRV_CTRL_MSG_TYPE_GET_DEV_LINK_STAT;
    msg->head.error_code = HDCDRV_OK;
    msg->head.para = server_type;
    msg->head.msg_len = msg_len;

    ret = (int)hdcdrv_ctrl_msg_send(dev_id, (void *)msg, msg_len, msg_len, &out_len);
    if ((ret == 0) && (msg->head.msg_len < buf_len - offset)) {
        msg->data[HDCDRV_SYSFS_DATA_MAX_LEN - 1] = '\0';
        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1), "%s\n", msg->data);
        if (ret >= 0) {
            offset += ret;
        }
    }

    hdcdrv_kvfree(msg, KA_SUB_MODULE_TYPE_4);
    msg = NULL;

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_link_stat(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t buf_size = KA_MM_PAGE_SIZE;
    ssize_t offset = 0;
    ssize_t len = 0;
    int ret;

    (void)attr;

    ret = snprintf_s(buf, (size_t)buf_size, (size_t)(buf_size - 1),
                     "HDC device %u server_type %u local statistics:\n", g_dev_id, g_server_type);
    if (ret >= 0) {
        offset += ret;
    }

    ret = hdcdrv_sysfs_get_link_inner(buf + offset, (u32)(buf_size - offset), g_dev_id, g_server_type, &len);
    if (ret != HDCDRV_OK) {
        return offset;
    }
    offset += len;

    offset += hdcdrv_sysfs_get_remote_link_stat(buf + offset, (u32)(buf_size - offset), g_dev_id, g_server_type);

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_get_fid(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    int ret;

    (void)attr;

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "fid %u\n", g_fid);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

STATIC ssize_t hdcdrv_sysfs_set_fid(ka_device_t *dev, ka_device_attribute_t *attr,
    const char *buf, size_t count)
{
    u32 val = 0;

    if (ka_base_kstrtou32(buf, 0, &val) < 0) {
        hdcdrv_err("Calling ka_base_kstrtou32 failed.\n");
        return -EINVAL;
    }

    if (val >= VMNG_VDEV_MAX_PER_PDEV) {
        hdcdrv_err("Pararmeter is invalid. (fid=%u)\n", val);
        return -EINVAL;
    }

    g_fid = val;

    return (ssize_t)count;
}

STATIC ssize_t hdcdrv_sysfs_get_vdev_stat(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
#ifdef CFG_FEATURE_VFIO
    struct hdcdrv_cmd_get_stat cmd;

    cmd.service_type = -1;
    cmd.dev_id = (int)g_dev_id;
    cmd.fid = (int)g_fid;
    cmd.vf_flag = 1;

    if ((cmd.dev_id >= hdcdrv_get_max_support_dev()) || (cmd.dev_id < 0) ||
        (cmd.dev_id >= VMNG_PDEV_MAX) || (cmd.fid < 0) || (cmd.fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d; fid=%d)\n", cmd.dev_id, cmd.fid);
        return offset;
    }

    offset = hdcdrv_sysfs_print_dev_stat(cmd, buf);
#endif
    return offset;
}

static KA_DRIVER_DEVICE_ATTR(chan, HDCDRV_ATTR_RW, hdcdrv_sysfs_get_chan_id, hdcdrv_sysfs_set_chan_id);
static KA_DRIVER_DEVICE_ATTR(session, HDCDRV_ATTR_RW, hdcdrv_sysfs_get_session_fd, hdcdrv_sysfs_set_session_fd);
static KA_DRIVER_DEVICE_ATTR(server, HDCDRV_ATTR_RW, hdcdrv_sysfs_get_server_type, hdcdrv_sysfs_set_server_type);
static KA_DRIVER_DEVICE_ATTR(dev, HDCDRV_ATTR_RW, hdcdrv_sysfs_get_dev_id, hdcdrv_sysfs_set_dev_id);
static KA_DRIVER_DEVICE_ATTR(fid, HDCDRV_ATTR_RW, hdcdrv_sysfs_get_fid, hdcdrv_sysfs_set_fid);
static KA_DRIVER_DEVICE_ATTR(chan_stat, HDCDRV_ATTR_RD, hdcdrv_sysfs_get_chan_stat, NULL);
static KA_DRIVER_DEVICE_ATTR(session_stat, HDCDRV_ATTR_RD, hdcdrv_sysfs_get_session_stat, NULL);
static KA_DRIVER_DEVICE_ATTR(server_stat, HDCDRV_ATTR_RD, hdcdrv_sysfs_get_server_stat, NULL);
static KA_DRIVER_DEVICE_ATTR(dev_stat, HDCDRV_ATTR_RD, hdcdrv_sysfs_get_dev_stat, NULL);
static KA_DRIVER_DEVICE_ATTR(link_stat, HDCDRV_ATTR_RD, hdcdrv_sysfs_get_link_stat, NULL);
static KA_DRIVER_DEVICE_ATTR(vdev_stat, HDCDRV_ATTR_RD, hdcdrv_sysfs_get_vdev_stat, NULL);

static ka_attribute_t *g_hdcdrv_sysfs_attrs[] = {
    ka_fs_get_dev_attr(dev_attr_chan)
    ka_fs_get_dev_attr(dev_attr_session)
    ka_fs_get_dev_attr(dev_attr_server)
    ka_fs_get_dev_attr(dev_attr_dev)
    ka_fs_get_dev_attr(dev_attr_fid)
    ka_fs_get_dev_attr(dev_attr_chan_stat)
    ka_fs_get_dev_attr(dev_attr_session_stat)
    ka_fs_get_dev_attr(dev_attr_server_stat)
    ka_fs_get_dev_attr(dev_attr_dev_stat)
    ka_fs_get_dev_attr(dev_attr_link_stat)
    ka_fs_get_dev_attr(dev_attr_vdev_stat)
    NULL,
};

static const ka_attribute_group_t g_hdcdrv_sysfs_group = {
    ka_fs_init_ag_attrs(g_hdcdrv_sysfs_attrs)
    ka_fs_init_ag_name("hdc")
};

void hdcdrv_sysfs_init(ka_device_t *dev)
{
    int ret;
;
    ret = ka_sysfs_create_group(ka_base_get_device_kobj(dev), &g_hdcdrv_sysfs_group);
    if (ret != 0) {
        hdcdrv_warn("Calling ka_sysfs_create_group not success. (ret=%d)\n", ret);
    }
}

void hdcdrv_sysfs_uninit(ka_device_t *dev)
{
    ka_sysfs_remove_group(ka_base_get_device_kobj(dev), &g_hdcdrv_sysfs_group);
}

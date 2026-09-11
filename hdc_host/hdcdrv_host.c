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

#include "dbl/uda.h"
#include "dbl/runenv_config.h"
#include "davinci_api.h"
#include "davinci_interface.h"

#include "kernel_version_adapt.h"
#include "hdcdrv_core.h"
#include "hdcdrv_adapter.h"
#include "hdcdrv_host.h"
#ifdef CFG_FEATURE_PFSTAT
#include "hdcdrv_pfstat.h"
#endif

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_driver_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_barrier_pub.h"
#include "ka_system_pub.h"
#include "ka_pci_pub.h"

STATIC struct hdc_hotreset_task_info g_hdc_hotreset_task_info[HDCDRV_SUPPORT_MAX_DEV] = {{0}};

static const ka_pci_device_id_t hdcdrv_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd100U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd105U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd801U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd501U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd802U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd803U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd804U), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd805U), 0 },
    { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
    {}};
KA_MODULE_DEVICE_TABLE(pci, hdcdrv_tbl);

STATIC int normal_chan_cnt[HDCDRV_SUPPORT_MAX_DEV] = {0};
STATIC int packet_segment = 0;
STATIC int running_env = 0;

int hdcdrv_force_link_down(void)
{
    int dev_id = 1;

    return devdrv_force_linkdown(dev_id);
}

int hdcdrv_get_link_status(struct devdrv_pcie_link_info_para *link_info)
{
    u32 link_status;
    int dev_id = 1;
    int ret;

    ret = devdrv_get_pcie_link_info((u32)dev_id, link_info);
    if (ret != 0) {
        hdcdrv_err("when query link status, get pcie status fail.(ret=%d, link_status=%d)\n",
            ret, link_info->link_status);
        return ret;
    }
    if (link_info->link_status != HDCDRV_LINK_NORMAL) {
        return HDCDRV_OK;
    }

    link_status = hdcdrv_get_device_status(dev_id);
    if (link_status == HDCDRV_VALID) {
        link_info->link_status = HDCDRV_LINK_NORMAL;
    } else {
        link_info->link_status = HDCDRV_HDC_DISCONNECT;
    }
    return HDCDRV_OK;
}

int hdcdrv_get_msgchan_refcnt(u32 dev_id)
{
    if (dev_id >= HDCDRV_SUPPORT_MAX_DEV) {
        hdcdrv_err("Input device id is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ka_task_spin_lock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    if ((g_hdc_hotreset_task_info[dev_id].hotreset_flag == HDCDRV_HOTRESET_FLAG_SET) &&
        (g_hdc_hotreset_task_info[dev_id].hdc_valid == HDCDRV_VALID)) {
        ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
        return -EBUSY;
    }

    g_hdc_hotreset_task_info[dev_id].msg_chan_refcnt++;
    ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    return 0;
}

int hdcdrv_put_msgchan_refcnt(u32 dev_id)
{
    if (dev_id >= HDCDRV_SUPPORT_MAX_DEV) {
        hdcdrv_err("Input device id is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ka_task_spin_lock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    g_hdc_hotreset_task_info[dev_id].msg_chan_refcnt--;
    ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    return 0;
}

int hdcdrv_get_running_env(void)
{
    return running_env;
}

/* driver mem config */
static int hdc_mempool_level = HDC_MEM_POOL_LEVEL_0;
ka_module_param(hdc_mempool_level, int, KA_S_IRUGO);

u32 hdcdrv_get_hdc_mempool_level(void)
{
    return (u32)hdc_mempool_level;
}

enum devdrv_dma_direction hdcdrv_get_dma_direction(void)
{
    return DEVDRV_DMA_DEVICE_TO_HOST;
}

int hdcdrv_set_msg_chan_priv(void *msg_chan, void *priv)
{
    return devdrv_set_msg_chan_priv(msg_chan, priv);
}

struct hdcdrv_msg_chan *hdcdrv_get_msg_chan_priv(void *msg_chan)
{
    return (struct hdcdrv_msg_chan *)devdrv_get_msg_chan_priv(msg_chan);
}

struct hdcdrv_sq_desc *hdcdrv_get_w_sq_desc(void *msg_chan, u32 *tail)
{
#ifdef CFG_FEATURE_SEC_COMM_L3
    return (struct hdcdrv_sq_desc *)devdrv_get_msg_chan_host_rsv_sq_tail(msg_chan, tail);
#else
    return (struct hdcdrv_sq_desc *)devdrv_get_msg_chan_slave_sq_tail(msg_chan, tail);
#endif
}

void hdcdrv_set_w_sq_desc_head(void *msg_chan, u32 head)
{
    devdrv_set_msg_chan_slave_sq_head(msg_chan, head);
}
#ifdef CFG_FEATURE_SEC_COMM_L3
STATIC void hdcdrv_sq_msg_callback(void *data, u32 trans_id, u32 status)
{
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)data;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, TX_SQ_DMA_LATENCY,
        msg_chan->tx[trans_id].latency_info.sq_dma_timestamp);
#endif
    if (status != 0) {
        hdcdrv_err("hdc trans send sq status fail. (dev_id=%u, status=%u)\n", msg_chan->chan_id, status);
        return;
    }

    msg_chan->dbg_stat.hdcdrv_msg_chan_send2++;
    /* trigger doorbell irq */
    ka_system_tasklet_schedule(&msg_chan->tx_sq_task);
}

STATIC void hdcdrv_cq_msg_callback(void *data, u32 trans_id, u32 status)
{
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)data;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, TX_CQ_DMA_LATENCY,
        msg_chan->rx[trans_id].latency_info.cq_dma_timestamp);
#endif
    if (status != 0) {
        hdcdrv_err("hdc trans send cq status fail. (dev_id=%u, status=%u)\n", msg_chan->chan_id, status);
        return;
    }

    msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task8++;
    /* trigger doorbell irq */
    ka_system_tasklet_schedule(&msg_chan->tx_cq_task);
}

int hdcdrv_copy_sq_desc_to_remote(struct hdcdrv_msg_chan *msg_dev, const struct hdcdrv_sq_desc *sq_desc,
    enum devdrv_dma_data_type data_type)
{
    struct devdrv_asyn_dma_para_info para = {0};
    void *msg_chan = msg_dev->chan;
    int ret;

    (void)data_type;

    para.interrupt_and_attr_flag = DEVDRV_REMOTE_IRQ_FLAG;
    para.priv = msg_dev;
    para.trans_id = sq_desc->trans_id;
    para.finish_notify = hdcdrv_sq_msg_callback;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&msg_dev->tx[sq_desc->trans_id].latency_info.sq_dma_timestamp);
#endif
    ret = devdrv_dma_copy_sq_desc_to_slave(msg_chan, &para, data_type, msg_dev->chan_id);
    if (ret != 0) {
        return ret;
    }
    devdrv_move_msg_chan_slave_sq_tail(msg_chan);

    return 0;
}
#else
int hdcdrv_copy_sq_desc_to_remote(struct hdcdrv_msg_chan *msg_dev, const struct hdcdrv_sq_desc *sq_desc,
    enum devdrv_dma_data_type data_type)
{
    void *msg_chan = msg_dev->chan;

    (void)sq_desc;
    (void)data_type;

    devdrv_move_msg_chan_slave_sq_tail(msg_chan);

    /* shared memory without copying */
    ka_wmb();

    /* trigger doorbell irq */
    devdrv_msg_ring_doorbell(msg_chan);
    return 0;
}
#endif

bool hdcdrv_w_sq_full_check(void *msg_chan)
{
    return devdrv_msg_chan_slave_sq_full_check(msg_chan);
}

struct hdcdrv_sq_desc *hdcdrv_get_r_sq_desc(void *msg_chan, u32 *head)
{
    return (struct hdcdrv_sq_desc *)devdrv_get_msg_chan_host_sq_head(msg_chan, head);
}

void hdcdrv_move_r_sq_desc(void *msg_chan)
{
    devdrv_move_msg_chan_host_sq_head(msg_chan);
}

struct hdcdrv_cq_desc *hdcdrv_get_w_cq_desc(void *msg_chan)
{
#ifdef CFG_FEATURE_SEC_COMM_L3
    return (struct hdcdrv_cq_desc *)devdrv_get_msg_chan_host_rsv_cq_tail(msg_chan);
#else
    return (struct hdcdrv_cq_desc *)devdrv_get_msg_chan_slave_cq_tail(msg_chan);
#endif
}

void hdcdrv_copy_cq_desc_to_remote(struct hdcdrv_msg_chan *msg_dev, const struct hdcdrv_cq_desc *cq_desc,
    enum devdrv_dma_data_type data_type)
{
#ifdef CFG_FEATURE_SEC_COMM_L3
    struct devdrv_asyn_dma_para_info para = {0};
#endif
    void *msg_chan = msg_dev->chan;

    (void)data_type;
#ifdef CFG_FEATURE_SEC_COMM_L3
    para.interrupt_and_attr_flag = DEVDRV_REMOTE_IRQ_FLAG;
    para.priv = msg_dev;
    para.trans_id = cq_desc->sq_head;
    para.finish_notify = hdcdrv_cq_msg_callback;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&msg_dev->rx[cq_desc->sq_head].latency_info.cq_dma_timestamp);
#endif
    (void)devdrv_dma_copy_cq_desc_to_slave(msg_chan, &para, data_type, msg_dev->chan_id);
    devdrv_move_msg_chan_slave_cq_tail(msg_chan);
    return;
#else
    devdrv_move_msg_chan_slave_cq_tail(msg_chan);

    /* shared memory without copying */
    ka_wmb();

    /* trigger doorbell irq */
    devdrv_msg_ring_cq_doorbell(msg_chan);
#endif
}

struct hdcdrv_cq_desc *hdcdrv_get_r_cq_desc(void *msg_chan)
{
    return (struct hdcdrv_cq_desc *)devdrv_get_msg_chan_host_cq_head(msg_chan);
}

void hdcdrv_move_r_cq_desc(void *msg_chan)
{
    devdrv_move_msg_chan_host_cq_head(msg_chan);
}

long hdcdrv_ctrl_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    long ret = 0;

    ret = hdcdrv_get_msgchan_refcnt(devid);
    if (ret != 0) {
        hdcdrv_err("Can not send ctrl message while hot reset flag is set. (dev_id=%u)\n", devid);
        return ret;
    }

    ret = (long)devdrv_common_msg_send(devid, data, in_data_len, out_data_len, real_out_len, DEVDRV_COMMON_MSG_HDC);
    (void)hdcdrv_put_msgchan_refcnt(devid);
    return ret;
}
EXPORT_SYMBOL_UNRELEASE(hdcdrv_ctrl_msg_send);

long hdcdrv_non_trans_ctrl_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    long ret = 0;
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[devid];

    ret = hdcdrv_get_msgchan_refcnt(devid);
    if (ret != 0) {
        hdcdrv_err("Can not send non trans ctrl message while hot reset flag is set. (dev_id=%u)\n", devid);
        return ret;
    }

    ret = (long)devdrv_sync_msg_send(hdc_dev->ctrl_msg_chan, data, in_data_len, out_data_len, real_out_len);
    (void)hdcdrv_put_msgchan_refcnt(devid);
    return ret;
}
EXPORT_SYMBOL_UNRELEASE(hdcdrv_non_trans_ctrl_msg_send);

STATIC int hdcdrv_non_trans_ctrl_msg_recv(void *msg_chan, void *data, u32 in_data_len,
    u32 out_data_len, u32 *real_out_len)
{
    u32 devid = (u32)devdrv_get_msg_chan_devid(msg_chan);

    return hdcdrv_ctrl_msg_recv(devid, data, in_data_len, out_data_len, real_out_len);
}

STATIC struct devdrv_common_msg_client hdcdrv_host_comm_msg_client = {
    .type = DEVDRV_COMMON_MSG_HDC,
    .common_msg_recv = hdcdrv_ctrl_msg_recv,
};

STATIC struct devdrv_non_trans_msg_chan_info hdcdrv_non_trans_msg_chan_info = {
    .msg_type = devdrv_msg_client_hdc,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_HIGH,
    .s_desc_size = HDCDRV_NON_TRANS_MSG_S_DESC_SIZE,
    .c_desc_size = HDCDRV_NON_TRANS_MSG_C_DESC_SIZE,
    .rx_msg_process = hdcdrv_non_trans_ctrl_msg_recv,
};

STATIC struct devdrv_trans_msg_chan_info hdcdrv_msg_chan_info = {
    .msg_type = devdrv_msg_client_hdc,
    .queue_depth = HDCDRV_DESC_QUEUE_DEPTH,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .sq_desc_size = sizeof(struct hdcdrv_sq_desc),
    .cq_desc_size = sizeof(struct hdcdrv_cq_desc),
    .rx_msg_notify = hdcdrv_rx_msg_notify,
    .tx_finish_notify = hdcdrv_tx_finish_notify,
};

STATIC void hdcdrv_msg_chan_level_init(u32 dev_id, int chan_id)
{
    if (chan_id < normal_chan_cnt[dev_id]) {
        hdcdrv_msg_chan_info.level = DEVDRV_MSG_CHAN_LEVEL_LOW;
        if (hdcdrv_get_service_level(chan_id) == HDCDRV_SERVICE_HIGH_LEVEL) {
            hdcdrv_msg_chan_info.level = DEVDRV_MSG_CHAN_LEVEL_HIGH;
        }
    } else {
        hdcdrv_msg_chan_info.level = DEVDRV_MSG_CHAN_LEVEL_HIGH;
    }
}

STATIC int hdcdrv_alloc_trans_queue(u32 dev_id, u32 chan_num, char *chan[], u32 *alloc_chan_num)
{
    int i;
    int ret = HDCDRV_OK;
    void *msg_chan = NULL;

    for (i = 0; i < chan_num; i++) {
        hdcdrv_msg_chan_level_init(dev_id, i);

        msg_chan = devdrv_pcimsg_alloc_trans_queue(dev_id, &hdcdrv_msg_chan_info);
        if (msg_chan == NULL) {
            hdcdrv_err("Calling devdrv_pcimsg_alloc_trans_queue failed. (dev_id=%d)\n", dev_id);
            ret = HDCDRV_ERR;
            goto OUT;
        }

        chan[i] = msg_chan;

        if (hdcdrv_add_msg_chan_to_dev(dev_id, msg_chan) != HDCDRV_OK) {
            i = i + 1;
            hdcdrv_err("Calling hdcdrv_add_msg_chan_to_dev failed. (dev_id=%d)\n", dev_id);
            ret = HDCDRV_ERR;
            goto OUT;
        }
    }

    hdcdrv_info("alloc chan. (dev_id=%u; chan_num=%u normal_chan_cnt=%d)\n", dev_id, chan_num, normal_chan_cnt[dev_id]);
OUT:
    *alloc_chan_num = (u32)i;
    return ret;
}

STATIC void hdcdrv_free_trans_queue(u32 dev_id, char *chan[], u32 chan_num)
{
    int i;
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];

    for (i = 0; i < chan_num; i++) {
        (void)devdrv_pcimsg_realease_trans_queue(chan[i]);
    }
    hdcdrv_free_msg_chan(hdc_dev);

    hdcdrv_info("free chan. (dev_id=%d; chan_num=%d)\n", dev_id, chan_num);
}

STATIC void hdcdrv_normal_chan_num_adapter(u32 dev_id, u32 msg_chan_cnt)
{
#ifdef CFG_FEATURE_VFIO
    if (msg_chan_cnt <= normal_chan_cnt[dev_id]) {
        normal_chan_cnt[dev_id] = HDCDRV_SUPPORT_MAX_SERVICE;
    }

    if (msg_chan_cnt <= HDCDRV_SUPPORT_MAX_SERVICE) {
        normal_chan_cnt[dev_id] = (int)(msg_chan_cnt / DEVDRV_TRANS_CHAN_TYPE);
    }
    hdcdrv_info("Record vfio msg_chan_cnt. (dev_id=%d; msg_chan=%d; normal_chan_cnt=%d)\n",
        dev_id, msg_chan_cnt, normal_chan_cnt[dev_id]);
#else
    /* Alloc suitable num for same fast and normal channel num */
    if (msg_chan_cnt / DEVDRV_TRANS_CHAN_TYPE <= normal_chan_cnt[dev_id]) {
        normal_chan_cnt[dev_id] = (int)(msg_chan_cnt / DEVDRV_TRANS_CHAN_TYPE);
    }

    if ((normal_chan_cnt[dev_id] < HDCDRV_SERVICE_TYPE_APPLY_MAX) &&
        (msg_chan_cnt > HDCDRV_SERVICE_TYPE_APPLY_MAX)) {
        normal_chan_cnt[dev_id] = HDCDRV_SERVICE_TYPE_APPLY_MAX;
    }

    hdcdrv_info("Record msg_chan_cnt. (dev_id=%d; msg_chan=%d; normal_chan_cnt=%d)\n",
        dev_id, msg_chan_cnt, normal_chan_cnt[dev_id]);
#endif
}

STATIC int hdcdrv_init_msg_chan(u32 dev_id)
{
    void *msg_chan = NULL;
    struct hdcdrv_ctrl_msg msg;
    u32 len = 0;
    u32 alloc_num = 0;
    int msg_chan_cnt = devdrv_get_support_msg_chan_cnt(dev_id, devdrv_msg_client_hdc);
    char *msgChanTmp[HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN] = {NULL};
    long ret;

    if ((msg_chan_cnt > HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN) || (msg_chan_cnt <= 0)) {
        hdcdrv_err("msg_chan_cnt is too large. (msg_chan_cnt=%d)\n", msg_chan_cnt);
        return HDCDRV_ERR;
    }

    hdcdrv_normal_chan_num_adapter(dev_id, (u32)msg_chan_cnt);

    hdcdrv_set_device_para(dev_id, normal_chan_cnt[dev_id]);
    msg.type = HDCDRV_CTRL_MSG_TYPE_CHAN_SET;
    msg.chan_set_msg.normal_chan_num = normal_chan_cnt[dev_id];
    msg.error_code = HDCDRV_OK;

    ret = hdcdrv_ctrl_msg_send(dev_id, (void *)&msg, (u32)sizeof(msg), (u32)sizeof(msg), &len);
    if ((ret != HDCDRV_OK) || (len != sizeof(msg)) || (msg.error_code != HDCDRV_OK)) {
        hdcdrv_err("Calling hdcdrv_ctrl_msg_send failed. (dev_id=%d)\n", dev_id);
        return HDCDRV_ERR;
    }

    ret = hdcdrv_alloc_trans_queue(dev_id, (u32)msg_chan_cnt, msgChanTmp, &alloc_num);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Alloc_trans_queue failed. (dev_id=%d; alloc_msg_chan_cnt=%d; alloc_num=%d)\n", dev_id,
            msg_chan_cnt, alloc_num);
        goto ALLOC_ERR;
    }

    msg_chan = devdrv_pcimsg_alloc_non_trans_queue(dev_id, &hdcdrv_non_trans_msg_chan_info);
    if (msg_chan == NULL) {
        hdcdrv_err("Calling devdrv_pcimsg_alloc_non_trans_queue failed. (dev_id=%d)\n", dev_id);
        goto ALLOC_ERR;
    }

    if (hdcdrv_add_ctrl_msg_chan_to_dev(dev_id, msg_chan) != HDCDRV_OK) {
        (void)devdrv_pcimsg_free_non_trans_queue(msg_chan);
        hdcdrv_err("Calling hdcdrv_add_ctrl_msg_chan_to_dev failed. (dev_id=%d)\n", dev_id);
        goto ALLOC_ERR;
    }

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_create_stats_handle(HDCDRV_PFSTATE_NON_TRANS_CHAN_ID);
#endif
    return HDCDRV_OK;

ALLOC_ERR:
    hdcdrv_free_trans_queue(dev_id, msgChanTmp, alloc_num);
    return HDCDRV_ERR;
}

STATIC void hdcdrv_uninit_msg_chan(struct hdcdrv_dev *hdc_dev)
{
    int i;

    if (hdc_dev->ctrl_msg_chan != NULL) {
        (void)devdrv_pcimsg_free_non_trans_queue(hdc_dev->ctrl_msg_chan);
        hdc_dev->ctrl_msg_chan = NULL;
    }

    for (i = 0; i < hdc_dev->msg_chan_cnt; i++) {
        (void)devdrv_set_msg_chan_priv(hdc_dev->msg_chan[i]->chan, NULL);
        (void)devdrv_pcimsg_realease_trans_queue(hdc_dev->msg_chan[i]->chan);
    }
}

STATIC void hdcdrv_init_host_phy_mach_flag(struct hdcdrv_dev *hdc_dev)
{
    u32 host_flag = DEVDRV_HOST_PHY_MACH_FLAG;
    int ret;

    ret = devdrv_get_host_phy_mach_flag(hdc_dev->dev_id, &host_flag);
    if (ret != 0) {
        hdcdrv_warn("Get_host_phy_mach_flag fail. (dev_id=%u)\n", hdc_dev->dev_id);
    }

    hdc_dev->host_pm_or_vm_flag = host_flag;
    return;
}

STATIC void hdcdrv_uninit_host_phy_mach_flag(struct hdcdrv_dev *hdc_dev)
{
    hdc_dev->host_pm_or_vm_flag = 0;
    return;
}

STATIC void hdcdrv_init_dev(struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_ctrl_msg msg;
    u32 len = 0;
    int ret;

    /* wait for device init */
    msg.type = HDCDRV_CTRL_MSG_TYPE_SYNC;
    msg.sync_msg.segment = packet_segment;
    msg.sync_msg.peer_dev_id = (int)hdc_dev->dev_id;
    msg.error_code = HDCDRV_OK;

    hdcdrv_init_host_phy_mach_flag(hdc_dev);

    ret = (int)hdcdrv_ctrl_msg_send(hdc_dev->dev_id, (void *)&msg, (u32)sizeof(msg), (u32)sizeof(msg), &len);
    if ((ret != HDCDRV_OK) || (len != sizeof(msg)) || (msg.error_code != HDCDRV_OK)) {
#ifndef DRV_UT
        hdcdrv_info_limit("Wait for device startup. (dev_id=%d; ret=%d; code=%d; msg_len=%d; sizeof_msg=%ld)\n",
            hdc_dev->dev_id, ret, msg.error_code, len, sizeof(msg));
        ka_task_schedule_delayed_work(&hdc_dev->init, 1 * KA_HZ);
        return;
#endif
    }
    hdcdrv_set_peer_dev_id((int)hdc_dev->dev_id, msg.sync_msg.peer_dev_id);

    /* init msg channel */
    ret = hdcdrv_init_msg_chan(hdc_dev->dev_id);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_init_msg_chan failed. (dev_id=%d)\n", hdc_dev->dev_id);
#ifndef DRV_UT
        return;
#endif
    }

    ret = devdrv_register_common_msg_client(&hdcdrv_host_comm_msg_client);
    if (ret != 0) {
        hdcdrv_err("Calling devdrv_register_common_msg_client failed.\n");
        hdcdrv_uninit_msg_chan(hdc_dev);
#ifndef DRV_UT
        return;
#endif
    }

    if (hdc_dev->msg_chan_cnt > 0) {
        hdcdrv_set_device_status((int)hdc_dev->dev_id, HDCDRV_VALID);
    }

    (void)devdrv_set_module_init_finish((int)hdc_dev->dev_id, DEVDRV_HOST_MODULE_HDC);

    if (hdcdrv_get_running_status() == HDCDRV_RUNNING_RESUME) {
        hdcdrv_set_running_status(HDCDRV_RUNNING_NORMAL);
    }
    ka_task_up(&hdc_dev->hdc_instance_sem);

    hdcdrv_info("Device enable work. (dev_id=%u; peer_dev_id=%d; msg_chan_count=%d; normal_chan_cnt=%u)\n",
        hdc_dev->dev_id, hdc_dev->peer_dev_id, hdc_dev->msg_chan_cnt, hdc_dev->normal_chan_num);
    return;
}

STATIC bool hdcdrv_is_in_phy_machine(u32 dev_id)
{
    unsigned int split_mode = VMNG_NORMAL_NONE_SPLIT_MODE;
    struct hdcdrv_dev *hdc_dev = NULL;
    bool is_in = false;

    hdc_dev = &hdc_ctrl->devices[dev_id];
#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_SRIOV)
    split_mode = vmng_get_device_split_mode(dev_id);
#endif
    if ((hdc_dev->host_pm_or_vm_flag == DEVDRV_HOST_PHY_MACH_FLAG) &&
        (split_mode == VMNG_NORMAL_NONE_SPLIT_MODE)) {
        is_in = true;
    }

    return is_in;
}

int hdcdrv_get_session_run_env(u32 dev_id, u32 fid)
{
    int ret;
    int run_env = HDCDRV_SESSION_RUN_ENV_UNKNOW;
    bool is_in_container = false;

    if (hdcdrv_is_vf_device(dev_id, fid)) {
        return HDCDRV_SESSION_RUN_ENV_VIRTUAL;
    }

    ret = hdcdrv_check_in_container();
    if (ret != HDCDRV_ERR) {
        is_in_container = (bool)ret;
    }

    if (hdcdrv_is_in_phy_machine(dev_id)) {
        if (is_in_container) {
            run_env = HDCDRV_SESSION_RUN_ENV_PHYSICAL_CONTAINER;
        } else {
            run_env = HDCDRV_SESSION_RUN_ENV_PHYSICAL;
        }
    } else {
        if (is_in_container) {
            run_env = HDCDRV_SESSION_RUN_ENV_VIRTUAL_CONTAINER;
        } else {
            run_env = HDCDRV_SESSION_RUN_ENV_VIRTUAL;
        }
    }

    return run_env;
}

int hdcdrv_container_vir_to_phs_devid(u32 virtual_devid, u32 *physical_devid, u32 *vfid)
{
    return devdrv_manager_container_logical_id_to_physical_id(virtual_devid, physical_devid, vfid);
}

int hdcdrv_check_in_container(void)
{
    return (int)run_in_normal_docker();
}

u32 hdcdrv_get_container_id(void)
{
    u32 docker_id = HDCDRV_PHY_HOST_ID;
    int ret;
#ifndef CFG_FEATURE_NOT_SUPPORT_UDA
    if (hdcdrv_check_in_container() != 1) {
        return docker_id;
    }

    ret = uda_get_cur_ns_id(&docker_id);
#else
    if (devdrv_manager_container_is_in_container() != 1) {
        return docker_id;
    }
    ret = devdrv_manager_container_get_docker_id(&docker_id);
#endif
    if (ret != 0) {
        docker_id = HDCDRV_DOCKER_MAX_NUM;
    }

    return docker_id;
}

u32 hdcdrv_get_vmid_from_pid(u64 pid)
{
    u32 vm_id;

    vm_id = (u32)((pid >> HDCDRV_VMID_OFFSET) & HDCDRV_VMID_MASK);
    return vm_id;
}

int hdcdrv_check_hostpid(int hostpid, u32 devid, u32 vfid)
{
    return HDCDRV_OK;
}

u32 hdcdrv_get_fid(u64 pid)
{
    return HDCDRV_DEFAULT_PM_FID;
}

int hdcdrv_get_localpid(u32 hostpid, u32 chip_id, int cp_type, u32 vfid, int *pid)
{
    *pid = (int)hostpid;
    return HDCDRV_OK;
}

u64 hdcdrv_get_peer_pid(u32 devid, u64 host_pid, u32 fid, u64 peer_pid, int service_type)
{
    (void)devid;
    (void)host_pid;
    (void)fid;
    (void)service_type;
    return peer_pid;
}

void hdcdrv_alloc_session_chan(int dev_id, int fid, int service_type, u32 *normal_chan_id, u32 *fast_chan_id)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdcdrv_alloc_msg_chan(dev_id, service_type, normal_chan_id, fast_chan_id);
    return;
#endif

#ifdef CFG_FEATURE_VFIO
    if (fid == HDCDRV_DEFAULT_PM_FID) {
        *normal_chan_id = (u32)service_type % hdc_ctrl->devices[dev_id].normal_chan_num;
        *fast_chan_id = hdcdrv_alloc_fast_msg_chan(dev_id, service_type,
            hdc_ctrl->devices[dev_id].normal_chan_num, hdc_ctrl->devices[dev_id].msg_chan_cnt);
    } else {
        *fast_chan_id = vdhch_alloc_fast_msg_chan((u32)dev_id, (u32)fid, service_type);
        *normal_chan_id = vdhch_alloc_normal_msg_chan((u32)dev_id, (u32)fid, service_type);
    }
#else
    *normal_chan_id = (u32)service_type % hdc_ctrl->devices[dev_id].normal_chan_num;
    *fast_chan_id = hdcdrv_alloc_fast_msg_chan(dev_id, service_type,
        hdc_ctrl->devices[dev_id].normal_chan_num, (u32)hdc_ctrl->devices[dev_id].msg_chan_cnt);
#endif
}

u64 hdcdrv_rebuild_pid(u32 devid, u32 fid, u64 pid)
{
    u64 pid_new;
    u64 vm_id;

#ifdef CFG_FEATURE_VFIO
        if (fid == HDCDRV_DEFAULT_PM_FID) {
            vm_id = HDCDRV_DEFAULT_VM_ID;
        } else {
            vm_id = (u64)vmngh_ctrl_get_vm_id(devid, fid) + 1;
        }
#else
        vm_id = HDCDRV_DEFAULT_VM_ID;
#endif

    pid_new = (vm_id << HDCDRV_VMID_OFFSET) | (pid & HDCDRV_RAW_PID_MASK);

    return pid_new;
}

STATIC void hdcdrv_register_hotreset_refcnt(u32 dev_id)
{
    ka_task_spin_lock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    if (g_hdc_hotreset_task_info[dev_id].hdc_valid == HDCDRV_INVALID) {
        g_hdc_hotreset_task_info[dev_id].hdc_valid = HDCDRV_VALID;
        g_hdc_hotreset_task_info[dev_id].hotreset_flag = HDCDRV_HOTRESET_FLAG_UNSET;
        g_hdc_hotreset_task_info[dev_id].msg_chan_refcnt = 0;
    }
    ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    hdcdrv_info("Hdc device hotreset refcnt register success. (dev_id=%u)\n", dev_id);
}

STATIC void hdcdrv_init_dev_work(ka_work_struct_t *p_work)
{
    struct hdcdrv_dev *hdc_dev = ka_container_of(p_work, struct hdcdrv_dev, init.work);
#ifndef DRV_UT
    hdcdrv_init_dev(hdc_dev);
#endif
}

int hdcdrv_init_instance(u32 dev_id, ka_device_t *dev)
{
    struct hdcdrv_dev *hdc_dev = NULL;

    hdc_dev = hdcdrv_add_dev(dev, dev_id);
    if (hdc_dev == NULL) {
        hdcdrv_err("Calling hdcdrv_add_dev failed. (dev_driver=\"%s\")\n", ka_driver_dev_driver_string(dev));
        return HDCDRV_ERR;
    }

    hdcdrv_register_hotreset_refcnt(dev_id);

    ka_task_sema_init(&hdc_dev->hdc_instance_sem, 0);
    KA_TASK_INIT_DELAYED_WORK(&hdc_dev->init, hdcdrv_init_dev_work);
    hdcdrv_init_dev(hdc_dev);

    return HDCDRV_OK;
}

int hdcdrv_service_scope_init(int service_type)
{
    if ((service_type == HDCDRV_SERVICE_TYPE_PROFILING) || (service_type == HDCDRV_SERVICE_TYPE_LOG) ||
        (service_type == HDCDRV_SERVICE_TYPE_DUMP)) {
        return HDCDRV_SERVICE_SCOPE_PROCESS;
    }

    return HDCDRV_SERVICE_SCOPE_GLOBAL;
}

int hdcdrv_service_log_limit_init(int service_type)
{
    if ((service_type == HDCDRV_SERVICE_TYPE_PROFILING) || (service_type == HDCDRV_SERVICE_TYPE_DUMP) ||
        (service_type == HDCDRV_SERVICE_TYPE_IDE1)) {
        return HDCDRV_SERVICE_LOG_LIMIT;
    }

    return HDCDRV_SERVICE_NO_LOG_LIMIT;
}

STATIC void hdcdrv_hotreset_stop_business(u32 dev_id)
{
    int loop_cnt = 0;
    int ret;
#ifdef CFG_FEATURE_HDC_REG_MEM
    struct devdrv_pcie_link_info_para link_info = {0};
#else
    unsigned int status = 0;
    struct ascend_intf_get_status_para para = {0};

    para.type = DAVINCI_STATUS_TYPE_DEVICE;
    para.para.device_id = dev_id;
#endif
    ka_task_spin_lock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    g_hdc_hotreset_task_info[dev_id].hotreset_flag = HDCDRV_HOTRESET_FLAG_SET;

    while ((g_hdc_hotreset_task_info[dev_id].msg_chan_refcnt != 0) && (loop_cnt <= HDCDRV_HOTRESET_CHECK_MAX_CNT)) {
        ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
#ifdef CFG_FEATURE_HDC_REG_MEM
        if (loop_cnt >= HDCDRV_HOTRESET_CHECK_PCIE_STAT_CNT) {
            ret = devdrv_get_pcie_link_info(dev_id, &link_info);
            if ((ret != 0) || (link_info.link_status != HDCDRV_LINK_NORMAL)) {
                return;
            }
        }
#else
#ifndef DRV_UT
        ret = ascend_intf_get_status(para, &status);
#else
        status = DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST;
#endif
        if (ret != 0) {
            hdcdrv_warn("Get device status warn. (dev_id=%u; ret=%d))\n", dev_id, ret);
        }

        if (((status & DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST) == DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST) ||
            ((status & DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL) == DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL)) {
            return;
        }
#endif
        ka_system_msleep(HDCDRV_HOTRESET_CHECK_DELAY_MS);
        loop_cnt++;
        ka_task_spin_lock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    }
    ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    return;
}

STATIC void hdcdrv_hotreset_refcnt_unregister(u32 dev_id)
{
    ka_task_spin_lock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    g_hdc_hotreset_task_info[dev_id].hdc_valid = HDCDRV_INVALID;
    ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    hdcdrv_info("Hdc drv hotreset refcnt unregister success. (dev_id=%u)\n", dev_id);
    return;
}

int hdcdrv_uninit_instance(u32 dev_id)
{
    struct hdcdrv_dev *hdc_dev = NULL;
    struct hdcdrv_ctrl_msg msg;
    int server_type;
    u32 len = 0;
    long ret;

    if (hdcdrv_get_running_status() == HDCDRV_RUNNING_SUSPEND) {
        return 0;
    }
    hdc_dev = hdcdrv_get_dev(dev_id);
    if (hdc_dev == NULL) {
        hdcdrv_err("Not valid dev. (dev_id=%d)\n", dev_id);
        return HDCDRV_ERR;
    }

    ret = ka_task_down_timeout(&hdc_dev->hdc_instance_sem, HDCDRV_INIT_INSTANCE_TIMEOUT);
    if (ret != 0) {
        hdcdrv_warn("Device instance timeout. (Devid=%d; ret=%ld)\n", hdc_dev->dev_id, ret);
    }

    /* when suspend, no need notify to ep, only free resource self */
    if (hdcdrv_get_running_status() == HDCDRV_RUNNING_NORMAL) {
        msg.type = HDCDRV_CTRL_MSG_TYPE_RESET;
        msg.error_code = HDCDRV_OK;
        ret = hdcdrv_ctrl_msg_send(hdc_dev->dev_id, (void *)&msg, (u32)sizeof(msg), (u32)sizeof(msg), &len);
        if ((ret != HDCDRV_OK) || (len != sizeof(msg)) || (msg.error_code != HDCDRV_OK)) {
            hdcdrv_info("Driver reset abnormal. (dev_driver=\"%s\")\n", ka_driver_dev_driver_string(hdc_dev->dev));
        }
    }
    hdcdrv_hotreset_stop_business(dev_id);
    ka_task_cancel_delayed_work_sync(&hdc_dev->init);
    (void)devdrv_unregister_common_msg_client(hdc_dev->dev_id, &hdcdrv_host_comm_msg_client);
    hdcdrv_hotreset_refcnt_unregister(dev_id);

    hdcdrv_reset_dev(hdc_dev);
    ka_system_msleep(500);  // sleep 500 ms
    hdcdrv_stop_work(hdc_dev);
    hdcdrv_uninit_msg_chan(hdc_dev);
    hdcdrv_remove_dev(hdc_dev);
    hdcdrv_free_dev_mem(dev_id);
    if (hdcdrv_get_running_status() == HDCDRV_RUNNING_NORMAL) {
        ka_task_mutex_lock(&hdc_dev->mutex);
        for (server_type = 0; server_type < HDCDRV_SUPPORT_MAX_SERVICE; server_type++) {
            hdcdrv_service_res_uninit(&hdc_dev->service[server_type], server_type);
        }
        ka_task_mutex_unlock(&hdc_dev->mutex);
    }
    hdcdrv_uninit_host_phy_mach_flag(hdc_dev);

    return HDCDRV_OK;
}

#define HDCDRV_HOST_NOTIFIER "hdc_host"
STATIC int hdcdrv_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= HDCDRV_SUPPORT_MAX_DEV) {
        hdcdrv_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = hdcdrv_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = hdcdrv_uninit_instance(udevid);
    } else if (action == UDA_SUSPEND) {
        ret = hdcdrv_suspend(udevid);
    } else if (action == UDA_RESUME) {
        ret = hdcdrv_resume(udevid, uda_get_device(udevid));
    } else {
#ifndef DRV_UT
        return 0;
#endif
    }

    hdcdrv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, (u32)action, ret);

    return ret;
}

static void hdcdrv_module_param_init(void)
{
    int chan_cnt;
    u32 i;

    if (devdrv_get_host_type() == HOST_TYPE_NORMAL) {
        chan_cnt = HDCDRV_SUPPORT_MAX_DEV_NORMAL_MSG_CHAN;
        packet_segment = HDCDRV_HUGE_PACKET_SEGMENT;
        running_env = HDCDRV_RUNNING_ENV_X86_NORMAL;
    } else {
        chan_cnt = HDCDRV_NORMAL_MSG_CHAN_CNT;
        packet_segment = HDCDRV_PACKET_SEGMENT;
        running_env = HDCDRV_RUNNING_ENV_ARM_3559;
    }

    for (i = 0; i < HDCDRV_SUPPORT_MAX_DEV; i++) {
        if (devdrv_get_pfvf_type_by_devid(i) == DEVDRV_SRIOV_TYPE_VF) {
            chan_cnt = HDCDRV_SRIOV_VF_SUPPORT_MAX_NORMAL_MSG_CHAN;
        }
        normal_chan_cnt[i] = chan_cnt;
    }

    if (hdc_mempool_level < 0 || hdc_mempool_level >= HDC_MEM_POOL_LEVEL_INVALID) {
        hdc_mempool_level = HDC_MEM_POOL_LEVEL_0;
    }

    hdcdrv_info("Set hdc mem pool level. (level=%d, type=%d).\n", hdc_mempool_level, devdrv_get_host_type());
}

STATIC void hdcdrv_init_hotreset_param(void)
{
    int dev_id;
    for (dev_id = 0; dev_id < HDCDRV_SUPPORT_MAX_DEV; dev_id++) {
        g_hdc_hotreset_task_info[dev_id].dev_id = dev_id;
        g_hdc_hotreset_task_info[dev_id].hdc_valid = HDCDRV_INVALID;
        g_hdc_hotreset_task_info[dev_id].hotreset_flag = HDCDRV_HOTRESET_FLAG_UNSET;
        g_hdc_hotreset_task_info[dev_id].msg_chan_refcnt = 0;
        ka_task_spin_lock_init(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    }
}

STATIC void hdcdrv_uninit_hotreset_param(void)
{
    int dev_id;
    for (dev_id = 0; dev_id < HDCDRV_SUPPORT_MAX_DEV; dev_id++) {
        ka_task_spin_lock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
        g_hdc_hotreset_task_info[dev_id].hdc_valid = HDCDRV_INVALID;
        g_hdc_hotreset_task_info[dev_id].hotreset_flag = HDCDRV_HOTRESET_FLAG_UNSET;
        g_hdc_hotreset_task_info[dev_id].msg_chan_refcnt = 0;
        ka_task_spin_unlock_bh(&g_hdc_hotreset_task_info[dev_id].task_rw_lock);
    }
}

STATIC int __init hdcdrv_init_module(void)
{
    struct uda_dev_type type;
    int ret;

    hdcdrv_module_param_init();
    hdcdrv_init_hotreset_param();

    ret = hdcdrv_init();
    if (ret != 0) {
        hdcdrv_err("Calling hdcdrv_init failed.\n");
        return ret;
    }

    hdcdrv_set_segment(packet_segment);

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(HDCDRV_HOST_NOTIFIER, &type, UDA_PRI1, hdcdrv_host_notifier_func);
    if (ret != HDCDRV_OK) {
        hdcdrv_uninit();
        hdcdrv_err("Calling uda_notifier_register failed.\n");
        return ret;
    }

#ifdef CFG_FEATURE_VFIO
    ret = vhdch_init();
    if (ret != HDCDRV_OK) {
        (void)uda_notifier_unregister(HDCDRV_HOST_NOTIFIER, &type);
        hdcdrv_uninit();
        hdcdrv_err("Calling vhdch_vmngh_init failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

#ifdef CFG_FEATURE_PFSTAT
    (void)hdcdrv_pfstat_init();
#endif

#ifdef CFG_FEATURE_HDC_REG_MEM
    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        hdcdrv_warn("can not get module.\n");
    }
#endif
    return HDCDRV_OK;
}

STATIC void __exit hdcdrv_exit_module(void)
{
    struct uda_dev_type type;
#ifdef CFG_FEATURE_PFSTAT
	hdcdrv_pfstat_exit();
#endif
    uda_davinci_near_real_entity_type_pack(&type);

#ifdef CFG_FEATURE_VFIO
    vhdch_uninit();
#endif
    (void)devdrv_unregister_common_msg_client(0, &hdcdrv_host_comm_msg_client);
    (void)uda_notifier_unregister(HDCDRV_HOST_NOTIFIER, &type);
    hdcdrv_uninit_hotreset_param();
    hdcdrv_uninit();
}

ka_module_init(hdcdrv_init_module);
ka_module_exit(hdcdrv_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("hdcdrv host driver");
KA_MODULE_LICENSE("GPL");

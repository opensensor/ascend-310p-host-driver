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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#ifdef VNIC_ENV_DEVICE
#include "dbl/spod_info.h"
#endif

#include "pcivnic_mem_alloc.h"
#include "pcivnic_main.h"

#include "ka_barrier_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_common_pub.h"
#include "ka_errno_pub.h"
#include "ka_driver_pub.h"
#include "ka_compiler_pub.h"
#include "ka_net_pub.h"

#define PCIVNIC_COUNT 64
#define CHAR_ARRAY_MAX_LEN 20

const char g_pcivnic_name[CHAR_ARRAY_MAX_LEN] = "pcivnic_driver";

static struct {
    const char str[KA_ETH_GSTRING_LEN];
} pcivnic_ethtool_stats[] = {
    {"pcivnic_rx_packets"}, {"pcivnic_tx_packets"}, {"pcivnic_rx_bytes"}, {"pcivnic_tx_bytes"}, {"pcivnic_rx_errors"},
    {"pcivnic_tx_errors"},  {"pcivnic_rx_dropped"}, {"pcivnic_tx_dropped"}, {"pcivnic_multicast"},
    {"pcivnic_collisions"}, {"pcivnic_rx_length_errors"}, {"pcivnic_rx_over_errors"}, {"pcivnic_rx_crc_errors"},
    {"pcivnic_rx_frame_errors"}, {"pcivnic_rx_fifo_errors"}, {"pcivnic_rx_missed_errors"},
    {"pcivnic_tx_aborted_errors"}, {"pcivnic_tx_carrier_errors"}, {"pcivnic_tx_fifo_errors"},
    {"pcivnic_tx_heartbeat_errors"}, {"pcivnic_tx_window_errors"}, {"pcivnic_rx_compressed"}, {"pcivnic_tx_compressed"}
};

STATIC void pcivnic_net_get_drvinfo(ka_net_device_t *net_dev, ka_ethtool_drvinfo_t *info)
{
    char version[CHAR_ARRAY_MAX_LEN] = "pcivnic v1.0";
    char bus_info[CHAR_ARRAY_MAX_LEN] = "pcivnic";

    if (strncpy_s(info->driver, CHAR_ARRAY_MAX_LEN, g_pcivnic_name, CHAR_ARRAY_MAX_LEN - 1) == EOK &&
        strncpy_s(info->version, CHAR_ARRAY_MAX_LEN, version, CHAR_ARRAY_MAX_LEN - 1) == EOK &&
        strncpy_s(info->bus_info, CHAR_ARRAY_MAX_LEN, bus_info, CHAR_ARRAY_MAX_LEN - 1) == EOK) {
        return;
    }

    devdrv_err("strncpy_s fail\n");
    return;
}

STATIC unsigned int pcivnic_net_get_link(ka_net_device_t *net_dev)
{
    struct pcivnic_netdev *vnic_dev = ka_net_netdev_priv(net_dev);
    return (vnic_dev->status & BIT_STATUS_LINK);
}

STATIC int pcivnic_get_sset_count(ka_net_device_t *net_dev, int sset)
{
    switch (sset) {
        case KA_ETH_SS_STATS:
            return KA_BASE_ARRAY_SIZE(pcivnic_ethtool_stats);
        default:
            return -EOPNOTSUPP;
    }
}

STATIC void pcivnic_get_strings(ka_net_device_t *net_dev, u32 strset, u8 *buf)
{
    if (strset == KA_ETH_SS_TEST) {
        devdrv_warn("strset equal with KA_ETH_SS_TEST\n");
        return;
    }
    if (memcpy_s(buf, sizeof(pcivnic_ethtool_stats), pcivnic_ethtool_stats, sizeof(pcivnic_ethtool_stats)) != 0) {
        devdrv_err("memcpy_s fail\n");
    }
    return;
}

STATIC void pcivnic_get_ethtool_stats(ka_net_device_t *net_dev, ka_ethtool_stats_t *estats, u64 *stats)
{
    struct pcivnic_netdev *vnic_dev = ka_net_netdev_priv(net_dev);
    int i = 0;

    devdrv_info("%s: pcivnic_get_ethtool_stats\n", vnic_dev->ndev->name);
    stats[i++] = net_dev->stats.rx_packets;
    stats[i++] = net_dev->stats.tx_packets;
    stats[i++] = net_dev->stats.rx_bytes;
    stats[i++] = net_dev->stats.tx_bytes;
    stats[i++] = net_dev->stats.rx_errors;
    stats[i++] = net_dev->stats.tx_errors;
    stats[i++] = net_dev->stats.rx_dropped;
    stats[i++] = net_dev->stats.tx_dropped;
    stats[i++] = net_dev->stats.multicast;
    stats[i++] = net_dev->stats.collisions;
    stats[i++] = net_dev->stats.rx_length_errors;
    stats[i++] = net_dev->stats.rx_over_errors;
    stats[i++] = net_dev->stats.rx_crc_errors;
    stats[i++] = net_dev->stats.rx_frame_errors;
    stats[i++] = net_dev->stats.rx_fifo_errors;
    stats[i++] = net_dev->stats.rx_missed_errors;
    stats[i++] = net_dev->stats.tx_aborted_errors;
    stats[i++] = net_dev->stats.tx_carrier_errors;
    stats[i++] = net_dev->stats.tx_fifo_errors;
    stats[i++] = net_dev->stats.tx_heartbeat_errors;
    stats[i++] = net_dev->stats.tx_window_errors;
    stats[i++] = net_dev->stats.rx_compressed;
    stats[i++] = net_dev->stats.tx_compressed;
}

STATIC void pcivnic_irqs_enable(struct pcivnic_netdev *vnic_dev)
{
    ka_net_skb_queue_head_init(&vnic_dev->skbq);
    ka_net_napi_enable(&vnic_dev->napi);
}

STATIC void pcivnic_irqs_disable(struct pcivnic_netdev *vnic_dev)
{
    ka_net_napi_disable(&vnic_dev->napi);
    ka_net_skb_queue_purge(&vnic_dev->skbq);
}

STATIC int pcivnic_net_open(ka_net_device_t *dev)
{
    struct pcivnic_netdev *vnic_dev = ka_net_netdev_priv(dev);

    ka_net_netif_carrier_off(dev);
    pcivnic_irqs_enable(vnic_dev);
    ka_net_netif_carrier_on(dev);

    ka_net_netif_start_queue(dev);

    vnic_dev->status |= BIT_STATUS_LINK;

    devdrv_info("%s: pcivnic_net_open\n", vnic_dev->ndev->name);

    /* Notify the other side that we're open will be next iteration */
    return 0;
}

STATIC int pcivnic_net_close(ka_net_device_t *dev)
{
    struct pcivnic_netdev *vnic_dev = ka_net_netdev_priv(dev);

    pcivnic_irqs_disable(vnic_dev);
    ka_net_netif_carrier_off(dev);
    ka_net_netif_stop_queue(dev);
    vnic_dev->status &= ~BIT_STATUS_LINK;

    devdrv_info("%s: pcivnic_net_close\n", vnic_dev->ndev->name);

    /* Notify the other side that we're closed will be next iteration */
    return 0;
}

STATIC int pcivnic_get_next_valid_pcidev(struct pcivnic_netdev *vnic_dev, int begin)
{
    struct pcivnic_pcidev *pcidev = NULL;
    int i;

    for (i = begin; i < NETDEV_PCIDEV_NUM; i++) {
        pcidev = vnic_dev->pcidev[i];
        if (pcidev == NULL) {
            continue;
        }
        if ((pcidev->status & BIT_STATUS_LINK) != 0) {
            if (pcidev->msg_chan != NULL) {
                return i;
            }
        }
    }

    return -EINVAL;
}

STATIC void pcivnic_netdev_completed_queue(struct pcivnic_pcidev *pcidev, u32 *bytes_compl, u32 *pkts_compl)
{
    struct pcivnic_netdev *vnic_dev = (struct pcivnic_netdev *)pcidev->netdev;

    ka_task_spin_lock_bh(&vnic_dev->lock);
    if ((*pkts_compl) || (*bytes_compl)) {
        ka_net_netdev_completed_queue(vnic_dev->ndev, *pkts_compl, *bytes_compl);
    }

    if (ka_unlikely(ka_net_netif_queue_stopped(vnic_dev->ndev)) && (*pkts_compl)) {
        ka_net_netif_wake_queue(vnic_dev->ndev);
    }
    ka_task_spin_unlock_bh(&vnic_dev->lock);
}

#ifdef CFG_FEATURE_S2S
STATIC int pcivnic_s2s_sk_buff_enqueue(struct vnic_s2s_queue *s2s_send_queue, ka_sk_buff_t *val)
{
    ka_task_spin_lock_bh(&s2s_send_queue->s2s_queue_lock);
    if ((s2s_send_queue->rear + 1) % VNIC_S2S_MAX_BUFF_DEPTH == s2s_send_queue->front) {
        ka_task_spin_unlock_bh(&s2s_send_queue->s2s_queue_lock);
        return -ENOMEM;
    }

    s2s_send_queue->skb[s2s_send_queue->rear] = val;
    s2s_send_queue->rear = (s2s_send_queue->rear + 1) % VNIC_S2S_MAX_BUFF_DEPTH;
    ka_task_spin_unlock_bh(&s2s_send_queue->s2s_queue_lock);
    return 0;
}

STATIC ka_sk_buff_t *pcivnic_s2s_sk_buff_dequeue(struct vnic_s2s_queue *s2s_send_queue)
{
    ka_sk_buff_t *val = NULL;

    ka_task_spin_lock_bh(&s2s_send_queue->s2s_queue_lock);
    if (s2s_send_queue->front == s2s_send_queue->rear) {
        ka_task_spin_unlock_bh(&s2s_send_queue->s2s_queue_lock);
        return NULL;
    }

    val = s2s_send_queue->skb[s2s_send_queue->front];
    s2s_send_queue->skb[s2s_send_queue->front] = NULL;
    s2s_send_queue->front = (s2s_send_queue->front + 1) % VNIC_S2S_MAX_BUFF_DEPTH;

    ka_task_spin_unlock_bh(&s2s_send_queue->s2s_queue_lock);
    return val;
}

STATIC void pcivnic_sdid_info_init(unsigned char *dmac, struct sdid_parse_info *sdid_info)
{
    u32 host_devid;
    u32 chip_id, die_id;

    host_devid = dmac[PCIVNIC_MAC_5];
    chip_id = host_devid / PCIVNIC_DIE_NUM_ONE_CHIP;
    die_id = host_devid % PCIVNIC_DIE_NUM_ONE_CHIP;

    sdid_info->server_id = dmac[PCIVNIC_MAC_4];
    sdid_info->chip_id = chip_id;
    sdid_info->die_id = die_id;
    sdid_info->udevid = host_devid;
}

STATIC int pcivnic_data_info_init(struct pcivnic_s2s_data_info *s2s_data_info, ka_sk_buff_t *skb,
    struct data_input_info *data_info)
{
    int ret;

    ret = memcpy_s(&s2s_data_info->data, skb->len, skb->data, skb->len);
    if (ret != 0) {
        devdrv_err("Memcpy skb data fail.(ret=%d)\n", ret);
        return ret;
    }
    s2s_data_info->msg_len = skb->len;

    data_info->data = s2s_data_info;
    data_info->data_len = sizeof(struct pcivnic_s2s_data_info) + skb->len;
    data_info->in_len = sizeof(struct pcivnic_s2s_data_info) + skb->len;
    data_info->out_len = 0;
    data_info->msg_mode = DEVDRV_S2S_SYNC_MODE;

    return 0;
}

STATIC int pcivnic_s2s_send_proc(ka_sk_buff_t *skb, struct pcivnic_pcidev *pcidev, u32 *sdid_list, u32 sdid_len)
{
    struct data_input_info data_info = {0};
    struct sdid_parse_info sdid_info = {0};
    struct pcivnic_s2s_data_info *s2s_data_info = NULL;
    u32 sdid = 0;
    int ret;
    u32 i;

    if ((sdid_list == NULL) || (sdid_len > VNIC_S2S_QUEUE_BUDGET)) {
        devdrv_err("Sdid list is null, or len is invaild.(ret=%d, devid=%u, sdid_len=%u)\n",
            ret, pcidev->dev_id, sdid_len);
        return -EINVAL;
    }

    pcivnic_sdid_info_init((unsigned char *)ka_net_skb_mac_header(skb), &sdid_info);
    ret = dbl_make_sdid(&sdid_info, &sdid);
    if (ret != 0) {
        devdrv_err("Get sdid fail.(ret=%d, devid=%u)\n", ret, pcidev->dev_id);
        return -EINVAL;
    }

    for (i = 0; i < sdid_len; i++) {
        /* no s2s abnormal, break to improve performance */
        if (sdid_list[i] == KA_U32_MAX) {
            break;
        }

        /* dst sdid may abnormal, please check or retry, must return -ENODEV */
        if (sdid_list[i] == sdid) {
            return -ENODEV;
        }
    }

    s2s_data_info = (struct pcivnic_s2s_data_info *)pcivnic_kvzalloc(skb->len + sizeof(struct pcivnic_s2s_data_info),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (s2s_data_info == NULL) {
        devdrv_err("Alloc skb data buf fail.(devid=%u)\n", pcidev->dev_id);
        return -ENOMEM;
    }

    ret = pcivnic_data_info_init(s2s_data_info, skb, &data_info);
    if (ret != 0) {
        pcivnic_kvfree(s2s_data_info);
        devdrv_err("s2s msg data init fail.(ret=%d, devid=%u)\n", ret, pcidev->dev_id);
        return -EINVAL;
    }

    ret = agentdrv_s2s_msg_send(pcidev->dev_id, sdid, AGENTDRV_S2S_MSG_PCIVNIC, AGENTDRV_S2S_TO_DEVICE, &data_info);
    if (ret != 0) {
        /* Record abnormal sdid, direct return next time */
        for (i = 0; i < sdid_len; i++) {
            if (sdid_list[i] == sdid) {
                break;
            }

            if (sdid_list[i] == KA_U32_MAX) {
                sdid_list[i] = sdid;
                break;
            }
        }
        pcivnic_kvfree(s2s_data_info);
        devdrv_err("s2s msg send fail.(ret=%d, devid=%u, dst_sdid=%u)\n", ret, pcidev->dev_id, sdid);
        return -EINVAL;
    }

    pcivnic_kvfree(s2s_data_info);
    return 0;
}

STATIC void pcivnic_s2s_send_work(ka_work_struct_t *p_work)
{
    struct vnic_s2s_queue *s2s_send_queue = ka_container_of(p_work, struct vnic_s2s_queue, s2s_send_work);
    struct pcivnic_pcidev *pcidev = s2s_send_queue->pcidev;
    struct pcivnic_netdev *vnic_dev = NULL;
    u32 sdid_list[VNIC_S2S_QUEUE_BUDGET];
    ka_sk_buff_t *skb = NULL;
    u32 bytes_compl = 0;
    u32 pkts_compl = 0;
    int cnt = 0;
    int ret;

    (void)memset_s(sdid_list, VNIC_S2S_QUEUE_BUDGET * sizeof(u32), KA_U32_MAX, VNIC_S2S_QUEUE_BUDGET * sizeof(u32));

    if (pcidev == NULL) {
        return;
    }
    vnic_dev = pcivnic_get_netdev(pcidev->dev_id);
    if (vnic_dev == NULL || vnic_dev->ndev_register != PCIVNIC_VALID) {
        return;
    }

    while (1) {
        if ((pcidev == NULL) || ((pcidev->status & BIT_STATUS_LINK) == 0)) {
            return;
        }

        if ((vnic_dev == NULL) || ((vnic_dev->status & BIT_STATUS_LINK) == 0)) {
            return;
        }

        /* Reach the threshold and schedule out */
        if ((cnt >= VNIC_S2S_QUEUE_BUDGET) && (s2s_send_queue->s2s_send_workqueue != NULL)) {
            ka_task_queue_work(s2s_send_queue->s2s_send_workqueue, &s2s_send_queue->s2s_send_work);
            break;
        }
        skb = pcivnic_s2s_sk_buff_dequeue(s2s_send_queue);
        if (skb == NULL) {
            break;
        }

        if (skb->len > PCIVNIC_MAX_PKT_SIZE) {
            ka_net_dev_kfree_skb_any(skb);
            devdrv_err("Len is too big.(len=%u, devid=%u)\n", skb->len, pcidev->dev_id);
            continue;
        }

        ret = pcivnic_s2s_send_proc(skb, pcidev, sdid_list, VNIC_S2S_QUEUE_BUDGET);
        if (ret != 0) {
            ka_net_dev_kfree_skb_any(skb);
            ka_task_spin_lock_bh(&vnic_dev->lock);
            vnic_dev->ndev->stats.tx_dropped++;
            vnic_dev->ndev->stats.tx_fifo_errors++;
            ka_task_spin_unlock_bh(&vnic_dev->lock);
            continue;
        }

        /* update netdev */
        if (vnic_dev->ndev != NULL) {
            ka_task_spin_lock_bh(&vnic_dev->lock);
            vnic_dev->ndev->stats.tx_packets++;
            vnic_dev->ndev->stats.tx_bytes += skb->len;
            ka_net_netdev_sent_queue(vnic_dev->ndev, skb->len);
            ka_task_spin_unlock_bh(&vnic_dev->lock);
        }

        bytes_compl += skb->len;
        pkts_compl++;
        ka_net_dev_consume_skb_any(skb);

        cnt++;
    }
    pcivnic_netdev_completed_queue(pcidev, &bytes_compl, &pkts_compl);
}

STATIC int pcivnic_pciedev_s2s_send(ka_sk_buff_t *skb, struct pcivnic_pcidev *pcidev, ka_net_device_t *dev)
{
    struct sdid_parse_info sdid_info = {0};
    u32 chan_idx;
    int ret;

    if ((pcidev == NULL) || ((pcidev->status & BIT_STATUS_LINK) == 0)) {
        devdrv_warn("pcidev is not useable\n");
        return KA_NETDEV_TX_OK;
    }

    if ((skb == NULL) || (dev == NULL)) {
        devdrv_warn("skb or dev is null.(devid=%u)\n", pcidev->dev_id);
        return KA_NETDEV_TX_OK;
    }

    pcivnic_sdid_info_init((unsigned char *)ka_net_skb_mac_header(skb), &sdid_info);
    if ((sdid_info.server_id >= PCIVNIC_S2S_SERVER_NUM) || (sdid_info.chip_id >= PCIVNIC_S2S_CHIP_NUM) ||
        (sdid_info.die_id >= PCIVNIC_S2S_DIE_NUM) || (sdid_info.udevid >= PCIVNIC_S2S_ONE_SERVER_DEV_NUM )) {
        devdrv_warn("skb mac is invaild. (devid=%u, server_id=%u, chip_id=%u, die_id=%u)\n", pcidev->dev_id,
            sdid_info.server_id, sdid_info.chip_id, sdid_info.die_id);
        return KA_NETDEV_TX_OK;
    }

    chan_idx = sdid_info.server_id * PCIVNIC_S2S_ONE_SERVER_DEV_NUM + sdid_info.udevid;
    ret = pcivnic_s2s_sk_buff_enqueue(&pcidev->s2s_send_queue[chan_idx], skb);
    if (ret != 0) {
        ka_net_netdev_tx_dropped_add(dev);
        ka_net_netdev_tx_fifo_errors_add(dev);
        return KA_NETDEV_TX_BUSY;
    }

    if (pcidev->s2s_send_queue[chan_idx].s2s_send_workqueue != NULL) {
        ka_task_queue_work(pcidev->s2s_send_queue[chan_idx].s2s_send_workqueue,
            &pcidev->s2s_send_queue[chan_idx].s2s_send_work);
    }

    return KA_NETDEV_TX_OK;
}

STATIC void pcidev_s2s_send_queue_uninit(struct pcivnic_pcidev *pcidev)
{
    u32 i, j;

    if (pcivnic_get_addr_mode() != AGENTDRV_ADMODE_FULL_MATCH) {
        return;
    }

    for (i = 0; i < PCIVNIC_S2S_MAX_CHAN_NUM; i++) {
        if (pcidev->s2s_send_queue[i].s2s_send_workqueue != NULL) {
            ka_task_destroy_workqueue(pcidev->s2s_send_queue[i].s2s_send_workqueue);
        }
        pcidev->s2s_send_queue[i].pcidev = NULL;
        pcidev->s2s_send_queue[i].queue_index = 0;

        for (j = 0; j < VNIC_S2S_MAX_BUFF_DEPTH; j++) {
            pcidev->s2s_send_queue[i].skb[j] = NULL;
        }
        pcidev->s2s_send_queue[i].front = 0;
        pcidev->s2s_send_queue[i].rear = 0;
    }
}

STATIC int pcidev_s2s_send_queue_init(struct pcivnic_pcidev *pcidev)
{
    u32 i, j;

    if (pcivnic_get_addr_mode() != AGENTDRV_ADMODE_FULL_MATCH) {
        return 0;
    }

    for (i = 0; i < PCIVNIC_S2S_MAX_CHAN_NUM; i++) {
        pcidev->s2s_send_queue[i].pcidev = pcidev;
        pcidev->s2s_send_queue[i].queue_index = i;
        ka_task_spin_lock_init(&(pcidev->s2s_send_queue[i].s2s_queue_lock));
        for (j = 0; j < VNIC_S2S_MAX_BUFF_DEPTH; j++) {
            pcidev->s2s_send_queue[i].skb[j] = NULL;
        }
        pcidev->s2s_send_queue[i].front = 0;
        pcidev->s2s_send_queue[i].rear = 0;

        pcidev->s2s_send_queue[i].s2s_send_workqueue = ka_task_create_singlethread_workqueue("s2s-send-work");
        if (pcidev->s2s_send_queue[i].s2s_send_workqueue == NULL) {
            return -EINVAL;
        }
        KA_TASK_INIT_WORK(&pcidev->s2s_send_queue[i].s2s_send_work, pcivnic_s2s_send_work);
    }

    return 0;
}
#endif

STATIC int pcivnic_pciedev_send(ka_sk_buff_t *skb, struct pcivnic_pcidev *pcidev, ka_net_device_t *dev)
{
    struct pcivnic_netdev *vnic_dev = (struct pcivnic_netdev *)pcidev->netdev;
    struct pcivnic_sq_desc *sq_desc = NULL;
    u64 addr;
    u32 tail = 0;
    int ret;

    if (pcivnic_device_status_abnormal(pcidev->msg_chan) != 0) {
        devdrv_warn("device %d maybe offline\n", pcidev->dev_id);
        ret = KA_NETDEV_TX_OK;
        goto ERROR;
    }

    ka_task_spin_lock_bh(&pcidev->lock);
    if ((pcidev->status & BIT_STATUS_LINK) == 0) {
        ka_task_spin_unlock_bh(&pcidev->lock);
        devdrv_warn("device %d is unlink\n", pcidev->dev_id);
        ret = KA_NETDEV_TX_OK;
        goto ERROR;
    }

    /* tx queue full */
    if (ka_unlikely(pcidev->status & BIT_STATUS_TQ_FULL)) {
        if (dev != NULL) {
            ka_net_netif_stop_queue(dev);
            ka_net_netdev_tx_dropped_add(dev);
            ka_net_netdev_tx_fifo_errors_add(dev);
        } else {
            ka_net_dev_kfree_skb_any(skb);
            skb = NULL;
        }
        ka_task_spin_unlock_bh(&pcidev->lock);
        if (pcidev->stat.tx_full % 1000 == 0) { // Print once every 1000 cycles
            devdrv_info("device %d tx queue full %llu, ndev status %d, stop tx queue\n",
                pcidev->dev_id, pcidev->stat.tx_full, (dev == NULL) ? 0 : 1);
        }
        pcidev->stat.tx_full++;
        return KA_NETDEV_TX_BUSY;
    }

    /* update netdev */
    if (dev != NULL) {
        ka_task_spin_lock_bh(&vnic_dev->lock);
        ka_net_netdev_set_trans_start(dev);
        ka_net_netdev_tx_packets_add(dev);
        ka_net_netdev_tx_bytes_add(dev, skb->len);
        ka_net_netdev_sent_queue(dev, skb->len);
        ka_task_spin_unlock_bh(&vnic_dev->lock);
    }

    sq_desc = pcivnic_get_w_sq_desc(pcidev->msg_chan, &tail);
    if (sq_desc == NULL || tail >= PCIVNIC_DESC_QUEUE_DEPTH) {
        ka_task_spin_unlock_bh(&pcidev->lock);
        devdrv_err("sq_desc is NULL\n");
        ret = KA_NETDEV_TX_OK;
        goto ERROR;
    }

#ifdef USE_DMA_ADDR
    addr = pcivnic_dma_map_single(pcidev, skb, PCIVNIC_DESC_QUEUE_TX, tail, skb->len);
    if (addr == (~(ka_dma_addr_t)0)) {
        ka_task_spin_unlock_bh(&pcidev->lock);
        devdrv_err("device %d dma map is error\n", pcidev->dev_id);
        ret = KA_NETDEV_TX_OK;
        goto ERROR;
    }
#else
    addr = ka_mm_virt_to_phys(skb->data);
#endif

    sq_desc->data_buf_addr = addr;
    sq_desc->data_len = skb->len;
    ka_wmb();
    sq_desc->valid = PCIVNIC_VALID;

    /* stored locally for release after subsequent sending */
    ka_task_spin_lock_bh(&pcidev->tx[tail].skb_lock);
    pcidev->tx[tail].addr = addr;
    pcidev->tx[tail].skb = skb;
    pcidev->tx[tail].len = (int)skb->len;
    pcidev->tx[tail].netdev = (void *)dev;
    if (dev != NULL) {
        ka_task_spin_lock_bh(&vnic_dev->lock);
        pcidev->tx[tail].tx_seq = ka_net_netdev_get_stats_tx_packets(dev);
        pcidev->tx[tail].timestamp = (unsigned long)(ka_jiffies / KA_HZ);
        ka_task_spin_unlock_bh(&vnic_dev->lock);
    }
    ka_task_spin_unlock_bh(&pcidev->tx[tail].skb_lock);

    pcidev->stat.tx_pkt++;
    pcidev->stat.tx_bytes += skb->len;

    pcivnic_copy_sq_desc_to_remote(pcidev, sq_desc);

    if (pcivnic_w_sq_full_check(pcidev->msg_chan)) {
        pcidev->status |= BIT_STATUS_TQ_FULL;
    }

    ka_task_spin_unlock_bh(&pcidev->lock);

    return KA_NETDEV_TX_OK;

ERROR:
    if (dev != NULL) {
        ka_task_spin_lock_bh(&vnic_dev->lock);
        ka_net_netdev_tx_dropped_add(dev);
        ka_net_netdev_tx_fifo_errors_add(dev);
        ka_task_spin_unlock_bh(&vnic_dev->lock);
    }
    ka_net_dev_kfree_skb_any(skb);

    return ret;
}

STATIC ka_netdev_tx_t pcivnic_net_xmit(ka_sk_buff_t *skb, ka_net_device_t *dev)
{
    struct pcivnic_netdev *vnic_dev = ka_net_netdev_priv(dev);
    ka_sk_buff_t *skb_cp = NULL;
    int ret = 0;
    int next_hop;
    int begin = 0;
    vnic_dev->stat.send_pkt++;

    /* bad pkt */
    if ((skb->len < KA_ETH_HLEN) || (skb->len > PCIVNIC_MAX_PKT_SIZE)) {
        goto free_skb;
    }

    next_hop = pcivnic_down_get_next_hop((unsigned char *)ka_net_skb_mac_header(skb));
    if (next_hop == PCIVNIC_NEXT_HOP_S2S) {
#ifdef CFG_FEATURE_S2S
        ret = pcivnic_pciedev_s2s_send(skb, vnic_dev->pcidev[0], dev);
            if (ret != 0) {
            goto free_skb;
        }
#endif
    } else if (next_hop != PCIVNIC_NEXT_HOP_BROADCAST) {
        if (vnic_dev->pcidev[next_hop] == NULL) {
            devdrv_info("next_hop %d dmac: %pM no pciedev\n", next_hop, ka_net_skb_mac_header(skb));
            goto free_skb;
        }

        if (vnic_dev->pcidev[next_hop]->msg_chan == NULL) {
            devdrv_info("next_hop %d dmac: %pM no msg chan\n", next_hop, ka_net_skb_mac_header(skb));
            goto free_skb;
        }

        ret = pcivnic_pciedev_send(skb, vnic_dev->pcidev[next_hop], dev);
    } else {
        next_hop = pcivnic_get_next_valid_pcidev(vnic_dev, begin);
        if (next_hop < 0) {
            goto free_skb;
        }

        begin = next_hop + 1;

        /* Broadcast transmission only counts once */
        skb_cp = ka_net_skb_copy(skb, KA_GFP_ATOMIC);
        ret = pcivnic_pciedev_send(skb, vnic_dev->pcidev[next_hop], dev);
        while (skb_cp != NULL) {
            next_hop = pcivnic_get_next_valid_pcidev(vnic_dev, begin);
            if (next_hop < 0) {
                break;
            }
            begin = next_hop + 1;

            skb = ka_net_skb_copy(skb_cp, KA_GFP_ATOMIC);
            if (skb != NULL) {
                (void)pcivnic_pciedev_send(skb, vnic_dev->pcidev[next_hop], NULL);
            }
        }

        if (skb_cp != NULL) {
            ka_net_dev_kfree_skb_any(skb_cp);
            skb_cp = NULL;
        }
    }

    return (ka_netdev_tx_t)ret;

free_skb:
    ka_task_spin_lock_bh(&vnic_dev->lock);
    ka_net_netdev_tx_errors_add(dev);
    ka_net_netdev_tx_dropped_add(dev);
    ka_task_spin_unlock_bh(&vnic_dev->lock);
    ka_net_dev_kfree_skb_any(skb);
    return KA_NETDEV_TX_OK;
}

STATIC int pcivnic_tx_finish_para_check(const struct pcivnic_cq_desc *cq_desc)
{
    if ((cq_desc == NULL) || (cq_desc->valid != PCIVNIC_VALID) ||
        (cq_desc->sq_head >= PCIVNIC_DESC_QUEUE_DEPTH)) {
        return -EINVAL;
    }
    return 0;
}

STATIC void pcivnic_tx_free_and_cqsq_update(struct pcivnic_pcidev *pcidev, struct pcivnic_cq_desc *cq_desc,
    u16 tx_head, u32 *bytes_compl, u32 *pkts_compl)
{
    struct pcivnic_netdev *vnic_dev = (struct pcivnic_netdev *)pcidev->netdev;
    ka_net_device_t *ndev = vnic_dev->ndev;
    void *msg_chan = pcidev->msg_chan;
    ka_sk_buff_t *skb = NULL;
    bool skb_been_freed = false;

    ka_task_spin_lock_bh(&pcidev->tx[tx_head].skb_lock);
    skb = pcidev->tx[tx_head].skb;
    if (skb == NULL) {
        skb_been_freed = true;
        goto next;
    }

    if ((cq_desc != NULL) && (cq_desc->status != 0)) {
        if (pcidev->tx[tx_head].netdev) {
            ka_task_spin_lock_bh(&vnic_dev->lock);
            ndev->stats.tx_errors++;
            ndev->stats.tx_carrier_errors++;
            ka_task_spin_unlock_bh(&vnic_dev->lock);
        }
    }

    if (pcidev->tx[tx_head].netdev != NULL) {
        (*pkts_compl)++;
        (*bytes_compl) += (u32)pcidev->tx[tx_head].len;
    }

#ifdef USE_DMA_ADDR
    pcivnic_dma_unmap_single(pcidev, skb, PCIVNIC_DESC_QUEUE_TX, tx_head, pcidev->tx[tx_head].len);
#endif
    ka_net_dev_consume_skb_any(skb);

next:
    pcidev->tx[tx_head].skb = NULL;
    pcidev->tx[tx_head].addr = (~(ka_dma_addr_t)0);
    pcidev->tx[tx_head].len = 0;
    pcidev->tx[tx_head].netdev = NULL;
    ka_task_spin_unlock_bh(&pcidev->tx[tx_head].skb_lock);

    ka_task_spin_lock_bh(&pcidev->lock);
    /* cq_desc != NULL is normal free flow, cq_desc == NULL is timeout flow, timeout flow not move cq */
    if (cq_desc != NULL) {
        cq_desc->valid = PCIVNIC_INVALID;
        pcivnic_move_r_cq_desc(msg_chan);
    }

    /* if skb_been_freed == true, means skb has been freed, no need double update the sq head */
    if (skb_been_freed != true) {
        /* update the sq head pointer to continue send packet */
        tx_head = (tx_head + 1) % PCIVNIC_DESC_QUEUE_DEPTH;
        pcivnic_set_w_sq_desc_head(msg_chan, tx_head);
    }

    if (ka_unlikely(pcidev->status & BIT_STATUS_TQ_FULL)) {
        pcidev->status &= ~BIT_STATUS_TQ_FULL;
    }
    ka_task_spin_unlock_bh(&pcidev->lock);
}

STATIC void pcivnic_tx_finish_notify_task(unsigned long data)
{
    struct pcivnic_pcidev *pcidev = (struct pcivnic_pcidev *)((uintptr_t)data);
    void *msg_chan = pcidev->msg_chan;
    struct pcivnic_cq_desc *cq_desc = NULL;
    u32 bytes_compl = 0;
    u32 pkts_compl = 0;
    u16 tx_head;
    int cnt = 0;
    int ret;

    pcidev->tx_finish_sched_stat.in++;

    do {
        cq_desc = pcivnic_get_r_cq_desc(msg_chan);
        ret = pcivnic_tx_finish_para_check(cq_desc);
        if (ret != 0) {
            break;
        }

        ka_rmb();

        /* Reach the threshold and schedule out */
        if (cnt >= PCIVNIC_TX_BUDGET) {
            ka_system_tasklet_schedule(&pcidev->tx_finish_task);
            break;
        }

        tx_head = (u16)cq_desc->sq_head;
        pcivnic_tx_free_and_cqsq_update(pcidev, cq_desc, tx_head, &bytes_compl, &pkts_compl);
        cnt++;
    } while (1);

    pcivnic_netdev_completed_queue(pcidev, &bytes_compl, &pkts_compl);
    pcidev->tx_finish_sched_stat.out++;
}

STATIC void pcivnic_timeout_tx_recycle_proc(struct pcivnic_pcidev *pcidev, u16 tx_head)
{
    u32 bytes_compl = 0;
    u32 pkts_compl = 0;

    pcivnic_tx_free_and_cqsq_update(pcidev, NULL, tx_head, &bytes_compl, &pkts_compl);
    pcivnic_netdev_completed_queue(pcidev, &bytes_compl, &pkts_compl);
}

void pcivnic_tx_finish_notify(void *msg_chan)
{
    struct pcivnic_pcidev *pcidev = pcivnic_get_pcidev(msg_chan);
    if (pcidev != NULL) {
        ka_system_tasklet_schedule(&pcidev->tx_finish_task);
    }
}

STATIC void pcivnic_response_cq(struct pcivnic_pcidev *pcidev, u32 sq_head, u32 status)
{
    struct pcivnic_cq_desc *cq_desc = NULL;

    /* response cq */
    cq_desc = pcivnic_get_w_cq_desc(pcidev->msg_chan);
    if (cq_desc == NULL) {
        devdrv_err("devid %d cq_desc is NULL, sq_head 0x%x, status 0x%x.\n", pcidev->dev_id, sq_head, status);
        return;
    }
    cq_desc->sq_head = sq_head;
    cq_desc->status = status;
    ka_wmb();
    cq_desc->valid = PCIVNIC_VALID;
    pcivnic_copy_cq_desc_to_remote(pcidev, cq_desc);
}

/* napi receive polling callback function */
static int pcivnic_napi(ka_napi_struct_t *napi, int budget)
{
    struct pcivnic_netdev *vnic_dev = ka_container_of(napi, struct pcivnic_netdev, napi);
    int work_done = 0;
    ka_sk_buff_t *skb = NULL;

    while (1) {
        if (work_done >= budget) {
            break;
        }

        skb = ka_net_skb_dequeue(&vnic_dev->skbq);
        if (ka_unlikely(skb == NULL)) {
            break;
        }

        (void)ka_net_napi_gro_receive(&vnic_dev->napi, skb);
        work_done++;
    }

    if (work_done < budget) {
        (void)ka_net_napi_complete(napi);
    }
    return work_done;
}

void pcivnic_rx_packet(ka_sk_buff_t *skb, struct pcivnic_netdev *vnic_dev, u32 dev_id)
{
    ka_net_device_t *ndev = vnic_dev->ndev;

    ka_task_spin_lock_bh(&vnic_dev->lock);
    if ((vnic_dev->status & BIT_STATUS_LINK) == 0) {
        ndev->stats.rx_dropped++;
        ka_task_spin_unlock_bh(&vnic_dev->lock);
        if ((ndev->stats.rx_dropped % PCIVNIC_LINKDOWN_NUM == 0) && (pcivnic_is_register_netdev(dev_id) == true)) {
            devdrv_info("rx drop packet.(len=%d, dev_id=%u)\n", skb->len, dev_id);
        }
        ka_net_dev_consume_skb_any(skb);
        return;
    }

    /* update stats */
    ndev->stats.rx_packets++;
    ndev->stats.rx_bytes += skb->len;

    ka_task_spin_unlock_bh(&vnic_dev->lock);

    /* set skb */
    skb->protocol = ka_net_eth_type_trans(skb, ndev);
    skb->ip_summed = KA_CHECKSUM_NONE;
    skb->dev = ndev;

    /* submit upper level process */
    ka_rmb();

    ka_net_skb_queue_tail(&vnic_dev->skbq, skb);
    vnic_dev->stat.recv_pkt++;
    if (ka_net_napi_schedule_prep(&vnic_dev->napi)) {
        __ka_net_napi_schedule(&vnic_dev->napi);
    }
    ka_net_netdev_set_last_rx(ndev);
}

STATIC int pcivnic_forward_flow_ctrl(struct pcivnic_netdev *vnic_dev, int forward_dev, int next_hop)
{
    struct pcivnic_flow_ctrl *flow_ctrl = NULL;
    struct pcivnic_fwd_stat *fwd_stat = NULL;
    unsigned long timestamp;

    fwd_stat = &vnic_dev->pcidev[forward_dev]->fwd_stat[next_hop];

    fwd_stat->fwd_all++;

    if (pcivnic_is_p2p_enabled((u32)forward_dev, (u32)next_hop) == false) {
        fwd_stat->disable_drop++;
        return PCIVNIC_VALID;
    }

    flow_ctrl = &vnic_dev->pcidev[forward_dev]->flow_ctrl[next_hop];

    flow_ctrl->pkt++;

    /* Threshold not reached */
    if (flow_ctrl->pkt <= flow_ctrl->threshold) {
        fwd_stat->fwd_success++;
        return PCIVNIC_INVALID;
    }

    /* Threshold reached */
    timestamp = (unsigned long)ka_jiffies;
    if (ka_system_jiffies_to_msecs(timestamp - flow_ctrl->timestamp) < PCIVNIC_FLOW_CTRL_PERIOD) {
        fwd_stat->flow_ctrl_drop++;
        return PCIVNIC_VALID;
    }

    /* new period */
    flow_ctrl->timestamp = timestamp;
    flow_ctrl->pkt = 0;

    fwd_stat->fwd_success++;
    return PCIVNIC_INVALID;
}

STATIC void pcivnic_forward_packet(ka_sk_buff_t *skb, struct pcivnic_netdev *vnic_dev, int forward_dev)
{
    ka_sk_buff_t *skb_cp = NULL;
    int next_hop;
    int begin = 0;

    next_hop = pcivnic_up_get_next_hop((unsigned char *)skb->data);
    if (next_hop == PCIVNIC_NEXT_HOP_LOCAL_NETDEV) {
        pcivnic_rx_packet(skb, vnic_dev, (u32)forward_dev);
    } else if (next_hop != PCIVNIC_NEXT_HOP_BROADCAST) {
        if ((forward_dev == next_hop) || (vnic_dev->pcidev[next_hop] == NULL)) {
            ka_net_dev_consume_skb_any(skb);
        } else {
            if (pcivnic_forward_flow_ctrl(vnic_dev, forward_dev, next_hop) == PCIVNIC_VALID) {
                ka_net_dev_consume_skb_any(skb);
                return;
            }

            (void)pcivnic_pciedev_send(skb, vnic_dev->pcidev[next_hop], NULL);
        }
    } else {
        while (1) {
            next_hop = pcivnic_get_next_valid_pcidev(vnic_dev, begin);
            if (next_hop < 0) {
                break;
            }
            begin = next_hop + 1;
            if (forward_dev == next_hop) {
                continue;
            }

            if (pcivnic_forward_flow_ctrl(vnic_dev, forward_dev, next_hop) == PCIVNIC_VALID) {
                continue;
            }

            skb_cp = ka_net_skb_copy(skb, KA_GFP_ATOMIC);
            if (skb_cp != NULL) {
                (void)pcivnic_pciedev_send(skb_cp, vnic_dev->pcidev[next_hop], NULL);
            }
        }

        pcivnic_rx_packet(skb, vnic_dev, (u32)forward_dev);
    }
}

STATIC void pcivnic_rx_msg_callback(void *data, u32 trans_id, u32 status)
{
    struct pcivnic_pcidev *pcidev = (struct pcivnic_pcidev *)data;
    struct pcivnic_netdev *vnic_dev = (struct pcivnic_netdev *)pcidev->netdev;
    ka_sk_buff_t *skb = NULL;

    if (trans_id >= PCIVNIC_DESC_QUEUE_DEPTH) {
        devdrv_err("dev %d pcivnic rx callback trans id %d error.\n", pcidev->dev_id, trans_id);
        return;
    }

    ka_task_spin_lock_bh(&pcidev->rx[trans_id].skb_lock);
    skb = pcidev->rx[trans_id].skb;
    if (skb == NULL) {
        ka_task_spin_unlock_bh(&pcidev->rx[trans_id].skb_lock);
        devdrv_err("dev %d pcivnic rx head %d skb is null.\n", pcidev->dev_id, trans_id);
        return;
    }

#ifdef USE_DMA_ADDR
    pcivnic_dma_unmap_single(pcidev, skb, PCIVNIC_DESC_QUEUE_RX, trans_id, pcidev->rx[trans_id].len);
#endif

    pcidev->stat.rx_pkt++;
    pcidev->stat.rx_bytes += (u64)pcidev->rx[trans_id].len;

    (void)ka_net_skb_put(skb, (unsigned int)pcidev->rx[trans_id].len);
    ka_task_spin_unlock_bh(&pcidev->rx[trans_id].skb_lock);
    if (status != 0) {
        devdrv_warn("dev %d rx callback unintended, rx_head=%d, status=%d.\n", pcidev->dev_id, trans_id, status);
        ka_net_dev_consume_skb_any(skb);
        pcidev->stat.rx_dma_err++;
    } else {
        pcivnic_forward_packet(skb, vnic_dev, (int)pcidev->dev_id);
        pcivnic_response_cq(pcidev, trans_id, status);
    }

    ka_task_spin_lock_bh(&pcidev->rx[trans_id].skb_lock);
    pcidev->rx[trans_id].skb = NULL;
    pcidev->rx[trans_id].addr = (~(ka_dma_addr_t)0);
    pcidev->rx[trans_id].len = 0;
    ka_task_spin_unlock_bh(&pcidev->rx[trans_id].skb_lock);
}

/* only [vfio] and [mdev+sriov's vm] use workqueue, others use tasklet */
STATIC void pcivnic_rx_msg_schedule_task(struct pcivnic_pcidev *pcidev)
{
    if (devdrv_is_mdev_vm_boot_mode(pcidev->dev_id) == true) {
        ka_task_queue_work(pcidev->rx_workqueue, &pcidev->rx_notify_work);
    } else {
        ka_system_tasklet_schedule(&pcidev->rx_notify_task);
    }
}

STATIC void pcivnic_rx_msg_notify_handle(struct pcivnic_pcidev *pcidev)
{
    void *msg_chan = pcidev->msg_chan;
    ka_sk_buff_t *skb = NULL;
    struct pcivnic_sq_desc *sq_desc = NULL;
    struct devdrv_asyn_dma_para_info dma_para_info;
    u32 head = PCIVNIC_DESC_QUEUE_DEPTH;
    static u32 log_printk_limit = 0;
    u64 addr;
    int cnt = 0;
    int ret = 0;

    dma_para_info.interrupt_and_attr_flag = DEVDRV_LOCAL_IRQ_FLAG;
    dma_para_info.priv = (void *)pcidev;
    dma_para_info.finish_notify = pcivnic_rx_msg_callback;

    do {
        sq_desc = pcivnic_get_r_sq_desc(msg_chan, &head);
        if ((sq_desc == NULL) || (sq_desc->valid != PCIVNIC_VALID) ||
            (sq_desc->data_len > PCIVNIC_MAX_PKT_SIZE) || (head >= PCIVNIC_DESC_QUEUE_DEPTH)) {
            break;
        }

        ka_rmb();

        /* Reach the threshold and schedule out */
        if (cnt >= PCIVNIC_RX_BUDGET) {
            pcivnic_rx_msg_schedule_task(pcidev);
            break;
        }

        dma_para_info.trans_id = head;

        /* the receiving end cannot apply for memory, too many receive
         * buffers, and waits for the upper layer to receive packets.
         * Block live send.
         */
        skb = ka_net_dev_alloc_skb(sq_desc->data_len);
        if (ka_unlikely(skb == NULL)) {
            if (log_printk_limit % 100 == 0) { // Print once every 100 cycles
                devdrv_err("dev %d rx alloc skb failed!\n", pcidev->dev_id);
            }
            log_printk_limit++;
            break;
        }

#ifdef USE_DMA_ADDR
        addr = pcivnic_dma_map_single(pcidev, skb, PCIVNIC_DESC_QUEUE_RX, head, sq_desc->data_len);
        if (addr == (~(ka_dma_addr_t)0)) {
            ka_net_dev_kfree_skb_any(skb);
            if (log_printk_limit % 100 == 0) { // Print once every 100 cycles
                devdrv_err("dev %d dma mapping error!\n", pcidev->dev_id);
            }
            log_printk_limit++;
            break;
        }
#else
        addr = ka_mm_virt_to_phys(skb->data);
#endif

        ka_task_spin_lock_bh(&pcidev->rx[head].skb_lock);
        pcidev->rx[head].skb = skb;
        pcidev->rx[head].addr = addr;
        pcidev->rx[head].len = (int)sq_desc->data_len;
        ka_task_spin_unlock_bh(&pcidev->rx[head].skb_lock);

        /* copy the packet */
        ret = pcivnic_dma_copy(pcidev, sq_desc->data_buf_addr, addr, sq_desc->data_len, &dma_para_info);
        if (ret != 0) {
            devdrv_warn("devid %d pcivnic_dma_copy is fail\n", pcidev->dev_id);
            pcidev->stat.rx_dma_fail++;
        }
        sq_desc->valid = PCIVNIC_INVALID;

        pcivnic_move_r_sq_desc(msg_chan);
        cnt++;
    } while (1);
}

STATIC void pcivnic_rx_msg_notify_work(ka_work_struct_t *p_work)
{
    struct pcivnic_pcidev *pcidev = ka_container_of(p_work, struct pcivnic_pcidev, rx_notify_work);
    pcivnic_rx_msg_notify_handle(pcidev);
}

STATIC void pcivnic_rx_msg_notify_task(unsigned long data)
{
    struct pcivnic_pcidev *pcidev = (struct pcivnic_pcidev *)((uintptr_t)data);
    pcivnic_rx_msg_notify_handle(pcidev);
}

void pcivnic_rx_msg_notify(void *msg_chan)
{
    struct pcivnic_pcidev *pcidev = pcivnic_get_pcidev(msg_chan);
    if ((pcidev != NULL) && (pcidev->status & BIT_STATUS_LINK)) {
        pcivnic_rx_msg_schedule_task(pcidev);
    }
}

STATIC void pcivnic_msg_chan_guard_work_sched(struct pcivnic_pcidev *pcidev)
{
    if (pcivnic_device_status_abnormal(pcidev->msg_chan) != 0) {
        return;
    }

    pcivnic_rx_msg_notify(pcidev->msg_chan);
}

STATIC void pcivnic_guard_work_sched(ka_work_struct_t *p_work)
{
    struct pcivnic_pcidev *pcidev =
        ka_container_of(p_work, struct pcivnic_pcidev, guard_work.work);

    pcivnic_msg_chan_guard_work_sched(pcidev);
    ka_task_schedule_delayed_work(&pcidev->guard_work, ka_system_msecs_to_jiffies(PCIVNIC_GUARD_WORK_DELAY_TIME));
}

void pcivnic_guard_work_init(struct pcivnic_pcidev *pcidev)
{
    /* msg guard work */
    KA_TASK_INIT_DELAYED_WORK(&pcidev->guard_work, pcivnic_guard_work_sched);
    ka_task_schedule_delayed_work(&pcidev->guard_work, 0);
}

void pcivnic_guard_work_uninit(struct pcivnic_pcidev *pcidev)
{
    ka_task_cancel_delayed_work_sync(&pcidev->guard_work);
}

STATIC unsigned long pcivnic_findout_smallest_tx_seq(struct pcivnic_netdev *vnic_dev, int *dev_id, u16 *tx_head)
{
    struct pcivnic_pcidev *pcidev = NULL;
    int i, j;
    unsigned long tx_seq = (unsigned long)-1;

    /* Search for the pkt with the smallest tx seq */
    for (i = 0; i < NETDEV_PCIDEV_NUM; i++) {
        pcidev = vnic_dev->pcidev[i];
        if (pcidev == NULL) {
            continue;
        }

        ka_task_spin_lock_bh(&pcidev->lock);
        if ((pcidev->status & BIT_STATUS_LINK) == 0) {
                ka_task_spin_unlock_bh(&pcidev->lock);
                continue;
        }
        for (j = 0; j < (int)(pcidev->queue_depth); j++) {
            ka_task_spin_lock_bh(&pcidev->tx[j].skb_lock);
            if (pcidev->tx[j].netdev == NULL) {
                ka_task_spin_unlock_bh(&pcidev->tx[j].skb_lock);
                continue;
            }
            if (pcidev->tx[j].tx_seq < tx_seq) {
                tx_seq = pcidev->tx[j].tx_seq;
                *dev_id = i;
                *tx_head = (u16)j;
            }
            ka_task_spin_unlock_bh(&pcidev->tx[j].skb_lock);
        }
        ka_task_spin_unlock_bh(&pcidev->lock);
    }

    return tx_seq;
}

STATIC void pcivnic_update_timeout_stat(struct pcivnic_pcidev *pcidev)
{
    if (pcidev->tx_finish_sched_stat.last_in != pcidev->tx_finish_sched_stat.in) {
        pcidev->timeout_cnt++;
    }
    pcidev->tx_finish_sched_stat.triger++;
    pcidev->tx_finish_sched_stat.last_in = pcidev->tx_finish_sched_stat.in;
}

STATIC void pcivnic_pause_free_queue(struct pcivnic_netdev *vnic_dev, struct pcivnic_pcidev *pcidev)
{
    u32 sq_id;

    for (sq_id = 0; sq_id < pcidev->queue_depth; sq_id++) {
        ka_task_spin_lock_bh(&pcidev->tx[sq_id].skb_lock);
        if (pcidev->tx[sq_id].skb == NULL) {
            ka_task_spin_unlock_bh(&pcidev->tx[sq_id].skb_lock);
            continue;
        }

        if (pcidev->tx[sq_id].netdev) {
            ka_task_spin_lock_bh(&vnic_dev->lock);
            ka_net_netdev_completed_queue(vnic_dev->ndev, 1, pcidev->tx[sq_id].skb->len);
            ka_task_spin_unlock_bh(&vnic_dev->lock);
        }

#ifdef USE_DMA_ADDR
        devdrv_dma_unmap_single(pcidev->dev, pcidev->tx[sq_id].addr, pcidev->tx[sq_id].skb->len, KA_DMA_TO_DEVICE);
#endif
        ka_net_dev_consume_skb_any(pcidev->tx[sq_id].skb);

        pcidev->tx[sq_id].skb = NULL;
        pcidev->tx[sq_id].addr = (~(ka_dma_addr_t)0);
        pcidev->tx[sq_id].len = 0;
        pcidev->tx[sq_id].netdev = NULL;
        ka_task_spin_unlock_bh(&pcidev->tx[sq_id].skb_lock);
    }
}

STATIC void pcivnic_net_timeout_work(ka_work_struct_t *p_work)
{
    ka_delayed_work_t *delayed_work = ka_container_of(p_work, ka_delayed_work_t, work);
    struct pcivnic_netdev *vnic_dev = ka_container_of(delayed_work, struct pcivnic_netdev, timeout);
    ka_net_device_t *dev = vnic_dev->ndev;
    struct pcivnic_pcidev *pcidev = NULL;
    u16 tx_head = 0;
    int dev_id = 0;
    unsigned long tx_seq;
    unsigned long cur_timestamp = (unsigned long)(ka_jiffies / KA_HZ);
    unsigned long timestamp;
    int timeout_num = 0;

find_next:

    /* Search for the pkt with the smallest tx seq */
    tx_seq = pcivnic_findout_smallest_tx_seq(vnic_dev, &dev_id, &tx_head);

    /* find out */
    if (tx_seq == (unsigned long)-1) {
        devdrv_info("net dev %s watchdog timeout num %d, cur_timestamp %lu, tx finish!\n", dev->name, timeout_num,
                    cur_timestamp);
        return;
    }

    pcidev = vnic_dev->pcidev[dev_id];
    ka_task_spin_lock_bh(&pcidev->tx[tx_head].skb_lock);
    timestamp = pcidev->tx[tx_head].timestamp;
    ka_task_spin_unlock_bh(&pcidev->tx[tx_head].skb_lock);
    ka_task_spin_lock_bh(&pcidev->lock);
    if ((pcidev->status & BIT_STATUS_LINK) == 0) {
        ka_task_spin_unlock_bh(&pcidev->lock);
        devdrv_info("net dev %s watchdog, pcidev->status=%d.\n", dev->name, pcidev->status);
        return;
    }
    ka_task_spin_unlock_bh(&pcidev->lock);

    if (cur_timestamp - timestamp > PCIVNIC_TIMESTAMP_OUT) {
        /* set the head cq desc status valid */
        pcivnic_timeout_tx_recycle_proc(pcidev, tx_head);

        /* only host has multi pcie dev in one netdev, clear linkup status */
        if ((pcidev->timeout_cnt > PCIVNIC_TIMEOUT_CNT) && (vnic_dev->pciedev_num > 1)) {
            ka_task_spin_lock_bh(&pcidev->lock);
            pcidev->status &= ~BIT_STATUS_LINK;
            ka_task_spin_unlock_bh(&pcidev->lock);
            pcivnic_pause_free_queue(vnic_dev, pcidev);
            devdrv_err("Set device pause, free queue. (netdev=%s; dev=%d; cur_timestamp=%lu)\n",
                dev->name, dev_id, cur_timestamp);
            return;
        }

        timeout_num++;
        pcivnic_update_timeout_stat(pcidev);

        ka_system_msleep(PCIVNIC_SLEEP_CNT);
        goto find_next;
    }

    devdrv_info("net dev %s watchdog, timeout_num=%d, timeout_cnt=%u, cur_timestamp=%lu, tx_head=%d.\n",
        dev->name, timeout_num, pcidev->timeout_cnt, cur_timestamp, tx_head);
}

STATIC void pcivnic_net_timeout(ka_net_device_t *dev)
{
    struct pcivnic_netdev *vnic_dev = ka_net_netdev_priv(dev);
    struct pcivnic_pcidev *pcidev = NULL;
    struct pcivnic_cq_desc *cq_desc = NULL;
    u16 tx_head = 0;
    int dev_id = 0;
    unsigned long tx_seq;
    unsigned long cur_timestamp = (unsigned long)(ka_jiffies / KA_HZ);
    unsigned long timestamp = 0;

    tx_seq = pcivnic_findout_smallest_tx_seq(vnic_dev, &dev_id, &tx_head);
    /* find out */
    if (tx_seq == (unsigned long)-1) {
        goto RESET_QUEUE;
    }

    pcidev = vnic_dev->pcidev[dev_id];
    ka_task_spin_lock_bh(&pcidev->tx[tx_head].skb_lock);
    timestamp = pcidev->tx[tx_head].timestamp;
    ka_task_spin_unlock_bh(&pcidev->tx[tx_head].skb_lock);
    ka_task_spin_lock_bh(&pcidev->lock);
    if ((pcidev->status & BIT_STATUS_LINK) != 0) {
        if (cur_timestamp - timestamp > PCIVNIC_TIMESTAMP_OUT) {
            cq_desc = pcivnic_get_r_cq_desc(pcidev->msg_chan);
            if (cq_desc != NULL) {
                devdrv_info_spinlock("dev %d cq id %d valid %d tx_head %d tx_seq %ld cur_timestamp %lu timestamp %lu"
                                     "sched in %llu out %llu triger %llu last sched %llu\n",
                                     dev_id, cq_desc->cq_id, cq_desc->valid, tx_head, tx_seq, cur_timestamp, timestamp,
                                     pcidev->tx_finish_sched_stat.in, pcidev->tx_finish_sched_stat.out,
                                     pcidev->tx_finish_sched_stat.triger, pcidev->tx_finish_sched_stat.last_in);
            }
            /* wakeup tx finish  */
            ka_system_tasklet_schedule(&pcidev->tx_finish_task);
            pcidev->tx_finish_sched_stat.triger++;
        }
    }
    ka_task_spin_unlock_bh(&pcidev->lock);

    /* perhaps tx finish tasklet is block, resycle the pkts later */
    (void)ka_task_schedule_delayed_work(&vnic_dev->timeout, PCIVNIC_DELAYWORK_TIME * KA_HZ);

RESET_QUEUE:
    if (ka_net_netif_queue_stopped(dev)) {
        ka_net_netif_wake_queue(dev);
    }

    devdrv_warn("net dev %s tx_seq=%ld, cur_timestamp=%lu, timestamp=%lu\n", dev->name, tx_seq, cur_timestamp, timestamp);
}

#ifndef RHEL_RELEASE_CODE
STATIC int pcivnic_net_change_mtu(ka_net_device_t *dev, int new_mtu)
{
    if ((new_mtu < PCIVNIC_MTU_LOW) || (new_mtu > PCIVNIC_MTU_HIGH)) {
        devdrv_err("mtu value is invalid!\n");
        return -EINVAL;
    }
    ka_net_netdev_set_mtu(dev, (unsigned int)new_mtu);
    return 0;
}
#endif

STATIC ka_net_device_stats_t *pcivnic_net_get_stats(ka_net_device_t *dev)
{
    return ka_net_netdev_get_stats(dev);
}

void pcivnic_net_timeout_new(ka_net_device_t *dev, unsigned int txqueue)
{
    pcivnic_net_timeout(dev);
}

ka_ethtool_ops_t g_pcivnic_ethtools_ops = {
    ka_ethtool_n_get_drvinfo(pcivnic_net_get_drvinfo)
    ka_ethtool_n_get_link(pcivnic_net_get_link)
    ka_ethtool_n_get_strings(pcivnic_get_strings)
    ka_ethtool_n_get_sset_count(pcivnic_get_sset_count)
    ka_ethtool_n_get_ethtool_stats(pcivnic_get_ethtool_stats)
};

ka_net_device_ops_t g_pcivnic_netdev_ops = {
    ka_net_n_ndo_open(pcivnic_net_open)
    ka_net_n_ndo_stop(pcivnic_net_close)
    ka_net_n_ndo_start_xmit(pcivnic_net_xmit)
    ka_net_n_ndo_change_mtu(pcivnic_net_change_mtu)
    ka_net_n_ndo_tx_timeout(pcivnic_net_timeout_new, pcivnic_net_timeout)
    ka_net_n_ndo_get_stats(pcivnic_net_get_stats)
};

STATIC ssize_t pcivnic_read_file(ka_file_t *file, loff_t *pos, char *addr, size_t count)
{
    return ka_fs_read_file(file, pos, addr, count);
}

STATIC char *pcivnic_skip_blank_space(const char *ptr)
{
    while ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n')) {
        ptr++;
    }

    return (char *)ptr;
}

STATIC char *pcivnic_skip_line(const char *ptr)
{
    while ((*ptr != '\n') && (*ptr != '\0')) {
        ptr++;
    }

    if (*ptr == '\n') {
        ptr++;
    }

    return pcivnic_skip_blank_space(ptr);
}

STATIC char *pcivnic_skip_notes(const char *ptr)
{
    /* skip blank space  */
    ptr = pcivnic_skip_blank_space(ptr);

    /* skip notes with # */
    while (*ptr == '#') {
        ptr = pcivnic_skip_line(ptr);
    }

    return (char *)ptr;
}

STATIC int pcivnic_is_valid_mac(const unsigned char *mac, int len)
{
    (void)len;

    return ka_net_is_valid_ether_addr(mac);
}

void pcivnic_get_mac(unsigned char last_byte, unsigned char *mac)
{
    unsigned int tmpmac[KA_ETH_ALEN];
    ka_file_t *file = NULL;
    char *tmpbuf = NULL;
    char *pmac = NULL;
    loff_t offset = 0;
    int tmp_id = -1;
    ssize_t len;
    const int dev_id = 0;
    int ret = 0;

    file = ka_fs_filp_open(PCIVNIC_MAC_FILE, KA_O_RDONLY, 0);
    if (KA_IS_ERR(file)) {
        devdrv_warn("pcivnic mac config file(%s) not existed\n", PCIVNIC_MAC_FILE);
        goto _random_mac;
    }

    tmpbuf = (char *)pcivnic_kvzalloc(PCIVNIC_CONF_FILE_SIZE, KA_GFP_KERNEL);
    if (tmpbuf == NULL) {
        devdrv_err("pcivnic mac buffer malloc failed, size %d\n", PCIVNIC_CONF_FILE_SIZE);
        goto _close_file;
    }

    len = pcivnic_read_file(file, &offset, tmpbuf, PCIVNIC_CONF_FILE_SIZE - 1);
    if (len <= 0) {
        devdrv_warn("read file(%s) failed\n", PCIVNIC_MAC_FILE);
        goto _free_buf;
    }

    tmpbuf[len] = '\0';
    pmac = &tmpbuf[0];
    pmac = pcivnic_skip_notes(pmac);

    /* find the mac address by device id */
    while (*pmac != '\0') {
        tmp_id = -1;
        ret = sscanf_s(pmac, "%d %x:%x:%x:%x:%x:%x", &tmp_id, &tmpmac[PCIVNIC_MAC_0], &tmpmac[PCIVNIC_MAC_1],
                       &tmpmac[PCIVNIC_MAC_2], &tmpmac[PCIVNIC_MAC_3], &tmpmac[PCIVNIC_MAC_4], &tmpmac[PCIVNIC_MAC_5]);
        if (ret != (KA_ETH_ALEN + 1)) {
                devdrv_warn("sscanf_s failed! ret = %d\n", ret);
                goto _free_buf;
        }

        if ((ret == PCIVNIC_CONF_SSCANF_OK) && (tmp_id == dev_id)) {
            break;
        }

        pmac = pcivnic_skip_line(pmac);
    }

    if (tmp_id != dev_id) {
        devdrv_warn("can't find device %d fixed mac address\n", dev_id);
        goto _free_buf;
    }

    /* mac[4] and mac[5] will be changed in devcie */
    mac[PCIVNIC_MAC_0] = tmpmac[PCIVNIC_MAC_0] & 0xFF;
    mac[PCIVNIC_MAC_1] = tmpmac[PCIVNIC_MAC_1] & 0xFF;
    mac[PCIVNIC_MAC_2] = tmpmac[PCIVNIC_MAC_2] & 0xFF;
    mac[PCIVNIC_MAC_3] = last_byte;
    mac[PCIVNIC_MAC_4] = tmpmac[PCIVNIC_MAC_4] & 0xFF;
    mac[PCIVNIC_MAC_5] = last_byte;

    if (pcivnic_is_valid_mac(mac, KA_ETH_ALEN) == 0) {
        devdrv_warn("The MAC is invalid form file %s\n", PCIVNIC_MAC_FILE);
        goto _free_buf;
    }

    pcivnic_kvfree(tmpbuf);
    tmpbuf = NULL;
    (void)ka_fs_filp_close(file, NULL);
    file = NULL;

    devdrv_info("pcivnic device %d, get mac success\n", dev_id);
    return;

_free_buf:
    pcivnic_kvfree(tmpbuf);
    tmpbuf = NULL;
_close_file:
    (void)ka_fs_filp_close(file, NULL);
    file = NULL;
_random_mac:
    mac[PCIVNIC_MAC_0] = PCIVNIC_MAC_VAL_0;
    mac[PCIVNIC_MAC_1] = PCIVNIC_MAC_VAL_1;
    mac[PCIVNIC_MAC_2] = PCIVNIC_MAC_VAL_2;
    mac[PCIVNIC_MAC_3] = PCIVNIC_MAC_VAL_3;
    mac[PCIVNIC_MAC_4] = PCIVNIC_MAC_VAL_4;
    mac[PCIVNIC_MAC_5] = last_byte;
    devdrv_info("pcivnic using default MAC. (dev_id=%d)\n", dev_id);

    return;
}

void pcivnic_set_netdev_mac(struct pcivnic_netdev *vnic_dev, const unsigned char *mac)
{
    ka_net_ether_addr_copy(vnic_dev->ndev, mac);
}

bool pcivnic_is_register_netdev(u32 dev_id)
{
#ifdef VNIC_ENV_DEVICE
    return true;
#else
    u32 chip_type = HISI_CHIP_NUM;

    chip_type = devdrv_get_dev_chip_type(dev_id);
    if (chip_type == HISI_CHIP_UNKNOWN) {
        return false;
    }

    /* CLOUD_V1 and CLOUD_V2's host not support vnic in release run pkg */
    if ((chip_type == HISI_CLOUD_V1) || (chip_type == HISI_CLOUD_V2)) {
        return false;
    } else {
        return true;
    }
#endif
}

int pcivnic_register_netdev(struct pcivnic_netdev *vnic_dev)
{
    int ret = -1;

    if (vnic_dev->ndev_register == PCIVNIC_INVALID) {
        ret = ka_net_register_netdev(vnic_dev->ndev);
        if (ret != 0) {
            devdrv_err("%s, dma register_ndev failed!\n", vnic_dev->ndev->name);
            return ret;
        }

        vnic_dev->ndev_register = PCIVNIC_VALID;
    }

    return ret;
}

void pcivnic_init_msgchan_cq_desc(void *msg_chan)
{
    struct pcivnic_cq_desc *cq_desc = NULL;
    struct pcivnic_cq_desc *w_cq_desc = NULL;
    u32 i;

    cq_desc = pcivnic_get_r_cq_desc(msg_chan);
    w_cq_desc = pcivnic_get_w_cq_desc(msg_chan);
    if ((cq_desc == NULL) || (w_cq_desc == NULL)) {
        devdrv_err("cq_desc is NULL\n");
        return;
    }
    for (i = 0; i < (u32)PCIVNIC_DESC_QUEUE_DEPTH; i++) {
        cq_desc->cq_id = i;
        cq_desc++;
        w_cq_desc->cq_id = i;
        w_cq_desc++;
    }
}

ssize_t pcivnic_get_dev_stat_inner(ka_device_t *dev, char *buf)
{
    struct pcivnic_pcidev *pcidev;
    struct pcivnic_netdev *vnic_dev = NULL;
    ssize_t offset = 0;
    int ret;

    pcidev = pcivnic_get_pciedev(dev);
    if (pcidev == NULL) {
        devdrv_err("not find pcidev\n");
        return offset;
    }

    vnic_dev = (struct pcivnic_netdev *)pcidev->netdev;

    ret = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1, "dev_id: %u\n", pcidev->dev_id);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "net dev status: %x, pci dev status: %x\n", vnic_dev->status, pcidev->status);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "pci dev timeout cnt: %x\n", pcidev->timeout_cnt);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "\npci dev stat:\n    tx_full: %llu\n    fwd_pkt: %llu\n    flow_ctrl_drop: %llu\n    rx_dma_err: %llu\n"
        "    rx_dma_fail: %llu\n    tx_pkt: %llu\n    tx_bytes: %llu\n    rx_pkt: %llu\n    rx_bytes: %llu\n",
        pcidev->stat.tx_full, pcidev->stat.fwd_pkt, pcidev->stat.flow_ctrl_drop,
        pcidev->stat.rx_dma_err, pcidev->stat.rx_dma_fail,
        pcidev->stat.tx_pkt, pcidev->stat.tx_bytes, pcidev->stat.rx_pkt, pcidev->stat.rx_bytes);
    if (ret >= 0) {
        offset += ret;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1,
        "\nnet dev stat:\n    send_pkt: %llu\n    recv_pkt: %llu\n    tx_packets: %llu\n    tx_dropped: %llu\n"
        "    tx_fifo_errors: %llu\n    tx_errors: %llu\n    tx_carrier_errors: %llu\n    rx_dropped: %llu\n"
        "    rx_packets: %llu\n",
        vnic_dev->stat.send_pkt, vnic_dev->stat.recv_pkt,  vnic_dev->ndev->stats.tx_packets,
        vnic_dev->ndev->stats.tx_dropped, vnic_dev->ndev->stats.tx_fifo_errors, vnic_dev->ndev->stats.tx_errors,
        vnic_dev->ndev->stats.tx_carrier_errors, vnic_dev->ndev->stats.rx_dropped, vnic_dev->ndev->stats.rx_packets);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}

#ifdef CFG_FEATURE_S2S
ssize_t pcivnic_sys_get_server_id(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    ssize_t offset = 0;
    u32 server_id;
    int ret;

    (void)attr;

    server_id = (u32)pcivnic_get_server_id();
    if ((server_id >= VNIC_MAX_SERVER_NUM) || (pcivnic_get_addr_mode() != AGENTDRV_ADMODE_FULL_MATCH)) {
        server_id = VNIC_DEFAULT_IP;
    }

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%u\n", server_id);
    if (ret >= 0) {
        offset += ret;
    }

    return offset;
}
static KA_DRIVER_DEVICE_ATTR(get_server_id, KA_S_IRUSR | KA_S_IRGRP, pcivnic_sys_get_server_id, NULL);
#endif
static KA_DRIVER_DEVICE_ATTR(stat, KA_S_IRUSR | KA_S_IRGRP, pcivnic_get_dev_stat, NULL);

static ka_attribute_t *g_pcivnic_sysfs_attrs[] = {
    ka_fs_get_dev_attr(dev_attr_stat)
#ifdef CFG_FEATURE_S2S
    ka_fs_get_dev_attr(dev_attr_get_server_id)
#endif
    NULL,
};

static const ka_attribute_group_t g_pcivnic_sysfs_group = {
    .attrs = g_pcivnic_sysfs_attrs,
    .name = "vnic",
};

static void pcivnic_init_pcidev(struct pcivnic_pcidev *pcidev)
{
    pcidev->timeout_cnt = 0;
    if (memset_s((void *)&pcidev->stat, sizeof(struct pcivnic_dev_stat), 0, sizeof(struct pcivnic_dev_stat)) != 0) {
        devdrv_warn("pcidev clear stat failed!\n");
    }

    if (memset_s((void *)&pcidev->tx_finish_sched_stat, sizeof(struct pcivnic_sched_stat), 0,
        sizeof(struct pcivnic_sched_stat)) != 0) {
        devdrv_warn("pcidev clear tx_finish_sched_stat failed!\n");
    }

    if (memset_s((void *)&pcidev->fwd_stat[0], sizeof(struct pcivnic_fwd_stat) * NETDEV_PCIDEV_NUM, 0,
        sizeof(struct pcivnic_fwd_stat) * NETDEV_PCIDEV_NUM) != 0) {
        devdrv_warn("pcidev clear fwd_stat failed!\n");
    }

    if (memset_s((void *)&pcidev->tx[0], sizeof(struct pcivnic_skb_addr) * PCIVNIC_DESC_QUEUE_DEPTH, 0,
        sizeof(struct pcivnic_skb_addr) * PCIVNIC_DESC_QUEUE_DEPTH) != 0) {
        devdrv_warn("pcidev tx buffer clear failed!\n");
    }

    if (memset_s((void *)&pcidev->rx[0], sizeof(struct pcivnic_skb_addr) * PCIVNIC_DESC_QUEUE_DEPTH, 0,
        sizeof(struct pcivnic_skb_addr) * PCIVNIC_DESC_QUEUE_DEPTH) != 0) {
        devdrv_warn("pcidev rx buffer clear failed!\n");
    }
}

struct pcivnic_pcidev *pcivnic_add_dev(struct pcivnic_netdev *vnic_dev, ka_device_t *dev, u32 queue_depth,
                                       int net_dev_id)
{
    struct pcivnic_pcidev *pcidev = NULL;
    int ret;
    u32 i;

    pcidev = vnic_dev->pcidev[net_dev_id];

    if (pcidev == NULL) {
        devdrv_err("dev id %d alloc err.", net_dev_id);
        return NULL;
    }

    pcidev->dev = dev;
    pcidev->netdev = (void *)vnic_dev;
    pcidev->dev_id = (u32)net_dev_id;

    ret = pcivnic_skb_data_buff_init(pcidev);
    if (ret != 0) {
        devdrv_err("dev_id %d skb_data_buff_init failed!\n", net_dev_id);
        return NULL;
    }

    pcivnic_init_pcidev(pcidev);

    /* init skb queue */
    pcidev->queue_depth = queue_depth;

    for (i = 0; i < queue_depth; i++) {
        pcidev->rx[i].skb = NULL;
        pcidev->tx[i].skb = NULL;
    }

    for (i = 0; i < NETDEV_PCIDEV_NUM; i++) {
        pcidev->flow_ctrl[i].threshold = PCIVNIC_FLOW_CTRL_THRESHOLD;
        pcidev->flow_ctrl[i].timestamp = (unsigned long)ka_jiffies;
        pcidev->flow_ctrl[i].pkt = 0;
    }

    ka_system_tasklet_init(&pcidev->tx_finish_task, pcivnic_tx_finish_notify_task, (uintptr_t)pcidev);

    /* only [vfio] and [mdev+sriov's vm] use workqueue, others use tasklet */
    if (devdrv_is_mdev_vm_boot_mode(net_dev_id) == true) {
        pcidev->rx_workqueue = ka_task_create_singlethread_workqueue(PCIVNIC_RX_MSG_NORIFY_WORK_NAME);
        if (pcidev->rx_workqueue == NULL) {
            pcivnic_skb_data_buff_uninit(pcidev);
            devdrv_err("Create_singlethread_workqueue failed. (name=\"%s\")\n", PCIVNIC_RX_MSG_NORIFY_WORK_NAME);
            return NULL;
        }
        KA_TASK_INIT_WORK(&pcidev->rx_notify_work, pcivnic_rx_msg_notify_work);
    } else {
        ka_system_tasklet_init(&pcidev->rx_notify_task, pcivnic_rx_msg_notify_task, (uintptr_t)pcidev);
    }

#ifdef CFG_FEATURE_S2S
    ret = pcidev_s2s_send_queue_init(pcidev);
    if (ret != 0) {
        pcivnic_skb_data_buff_uninit(pcidev);
        pcidev_s2s_send_queue_uninit(pcidev);
        devdrv_err("S2s queue init failed. (dev_id=%d; name=\"%s\")\n", net_dev_id, PCIVNIC_RX_MSG_NORIFY_WORK_NAME);
        return NULL;
    }
    KA_TASK_INIT_DELAYED_WORK(&pcidev->s2s_recycle, pcivnic_s2s_send_guard_work);
    (void)ka_task_schedule_delayed_work(&pcidev->s2s_recycle, VNIC_S2S_RECYCLE_DELAY_TIME * KA_HZ);
#endif

    vnic_dev->pciedev_num++;
    if (pcivnic_get_sysfs_creat_group_capbility(dev, net_dev_id) == true) {
        if (ka_sysfs_create_group(ka_base_get_device_kobj(dev), &g_pcivnic_sysfs_group) != 0) {
            devdrv_warn("pcivnic ka_sysfs_create_group abnormal.(dev_id=%d )\n", net_dev_id);
            pcidev->sysfs_create_flag = 0;
        } else {
            pcidev->sysfs_create_flag = PCIVNIC_SYSFS_BEEN_CREATE;
        }
    }

    devdrv_info("pcivnic add dev success. (dev_id=%d)\n", net_dev_id);

    return pcidev;
}

void pcivnic_del_dev(struct pcivnic_netdev *vnic_dev, int dev_id)
{
    struct pcivnic_pcidev *pcidev = NULL;
    u32 i;

    pcidev = vnic_dev->pcidev[dev_id];
    ka_task_spin_lock_bh(&pcidev->lock);
    pcidev->status &= ~BIT_STATUS_LINK;
    ka_task_spin_unlock_bh(&pcidev->lock);

    if (pcivnic_get_sysfs_creat_group_capbility(pcidev->dev, dev_id) == true) {
        if (pcidev->sysfs_create_flag == PCIVNIC_SYSFS_BEEN_CREATE) {
            ka_sysfs_remove_group(ka_base_get_device_kobj(pcidev->dev), &g_pcivnic_sysfs_group);
            pcidev->sysfs_create_flag = 0;
        }
    }

    vnic_dev->pciedev_num--;
    ka_system_msleep(PCIVNIC_SLEEP_CNT);
#ifdef CFG_FEATURE_S2S
    (void)ka_task_cancel_delayed_work_sync(&pcidev->s2s_recycle);
    pcidev_s2s_send_queue_uninit(pcidev);
#endif
    ka_system_tasklet_kill(&pcidev->tx_finish_task);
    if (devdrv_is_mdev_vm_boot_mode(pcidev->dev_id) == true) {
        ka_task_destroy_workqueue(pcidev->rx_workqueue);
    } else {
        ka_system_tasklet_kill(&pcidev->rx_notify_task);
    }

    for (i = 0; i < pcidev->queue_depth; i++) {
        if (pcidev->tx[i].skb) {
            if (pcidev->tx[i].netdev) {
                ka_task_spin_lock_bh(&vnic_dev->lock);
                ka_net_netdev_completed_queue(vnic_dev->ndev, 1, pcidev->tx[i].skb->len);
                ka_task_spin_unlock_bh(&vnic_dev->lock);
            }
#ifdef USE_DMA_ADDR
            devdrv_dma_unmap_single(pcidev->dev, pcidev->tx[i].addr, pcidev->tx[i].skb->len, KA_DMA_TO_DEVICE);
#endif
            ka_net_dev_consume_skb_any(pcidev->tx[i].skb);
        }

        if (pcidev->rx[i].skb) {
#ifdef USE_DMA_ADDR
            devdrv_dma_unmap_single(pcidev->dev, pcidev->rx[i].addr, PCIVNIC_MAX_PKT_SIZE, KA_DMA_FROM_DEVICE);
#endif
            ka_net_dev_consume_skb_any(pcidev->rx[i].skb);
        }
    }
    pcivnic_skb_data_buff_uninit(pcidev);
    devdrv_info("pcivnic del dev success.(dev_id=%d)\n", dev_id);
}

STATIC struct pcivnic_pcidev *pcivnic_alloc_pcidevs(u32 dev_num)
{
    struct pcivnic_pcidev *pcidev;

    pcidev = (struct pcivnic_pcidev *)pcivnic_vzalloc((unsigned long)sizeof(struct pcivnic_pcidev) * dev_num);
    if (pcidev == NULL) {
        devdrv_err("alloc pcidevs err\n");
        return NULL;
    }

    return pcidev;
}

STATIC void pcivnic_free_pcidevs(const struct pcivnic_pcidev *pcidev)
{
    if (pcidev != NULL) {
        pcivnic_vfree(pcidev);
        pcidev = NULL;
    }
}

struct pcivnic_netdev *pcivnic_alloc_netdev(const char *ndev_name, int ndev_name_len)
{
    struct pcivnic_netdev *vnic_dev = NULL;
    struct pcivnic_pcidev *pcidev = NULL;
    ka_net_device_t *ndev = NULL;
    u32 i, j;

    (void)ndev_name_len;
    pcidev = pcivnic_alloc_pcidevs(NETDEV_PCIDEV_NUM);
    if (pcidev == NULL) {
        devdrv_err("alloc pcidev fail\n");
        return NULL;
    }

    /* init netdev */
    ndev = ka_net_alloc_netdev(sizeof(struct pcivnic_netdev), ndev_name);
    if (ndev == NULL) {
        pcivnic_free_pcidevs(pcidev);
        devdrv_err("alloc netdev fail\n");
        return NULL;
    }

    vnic_dev = ka_net_netdev_priv(ndev);
    vnic_dev->ndev_register = PCIVNIC_INVALID;
    vnic_dev->ndev = ndev;

    ndev->netdev_ops = &g_pcivnic_netdev_ops;
    ndev->ethtool_ops = &g_pcivnic_ethtools_ops;

    ndev->watchdog_timeo = PCIVNIC_WATCHDOG_TIME * KA_HZ;
    ndev->hw_features |= KA_NETIF_F_GRO;
    ndev->features |= KA_NETIF_F_HIGHDMA | KA_NETIF_F_GSO;
#ifdef CFG_FEATURE_S2S
    if (pcivnic_get_addr_mode() == AGENTDRV_ADMODE_FULL_MATCH) {
        ndev->max_mtu = PCIVNIC_MTU_HIGH;
    }
#endif
    vnic_dev->status &= ~BIT_STATUS_LINK;
    vnic_dev->status &= ~BIT_STATUS_TQ_FULL;
    vnic_dev->status &= ~BIT_STATUS_RQ_FULL;

    ka_task_spin_lock_init(&vnic_dev->lock);
    ka_task_spin_lock_init(&vnic_dev->rx_lock);

    vnic_dev->pciedev_num = 0;

    KA_TASK_INIT_DELAYED_WORK(&vnic_dev->timeout, pcivnic_net_timeout_work);

    for (i = 0; i < NETDEV_PCIDEV_NUM; i++) {
        vnic_dev->pcidev[i] = pcidev + i;
        ka_task_spin_lock_init(&(vnic_dev->pcidev[i]->lock));
        for (j = 0; j < PCIVNIC_DESC_QUEUE_DEPTH; j++) {
            ka_task_spin_lock_init(&(vnic_dev->pcidev[i]->tx[j].skb_lock));
            ka_task_spin_lock_init(&(vnic_dev->pcidev[i]->rx[j].skb_lock));
        }
    }

    /* init napi */
    ka_net_netif_napi_add(vnic_dev->ndev, &vnic_dev->napi, pcivnic_napi, PCIVNIC_NAPI_POLL_WEIGHT);
    return vnic_dev;
}

void pcivnic_free_netdev(struct pcivnic_netdev *vnic_dev)
{
    int i;

    ka_net_netif_napi_del(&vnic_dev->napi);
#ifndef DRV_UT
    if (vnic_dev->ndev_register == PCIVNIC_VALID) {
        ka_net_unregister_netdev(vnic_dev->ndev);
        vnic_dev->ndev_register = PCIVNIC_INVALID;
    }
#endif
    (void)ka_task_cancel_delayed_work_sync(&vnic_dev->timeout);

    if (vnic_dev->pcidev[0] != NULL) {
        pcivnic_free_pcidevs(vnic_dev->pcidev[0]);
    }

    for (i = 0; i < NETDEV_PCIDEV_NUM; i++) {
        vnic_dev->pcidev[i] = NULL;
    }

    ka_net_free_netdev(vnic_dev->ndev);
}

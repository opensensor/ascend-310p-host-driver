/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#include <linux/delay.h>
#include <asm/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/pci.h>

#include "devdrv_ctrl.h"
#include "devdrv_dma.h"
#include "devdrv_msg.h"
#include "devdrv_common_msg.h"
#include "devdrv_msg_def.h"
#include "devdrv_util.h"

int (*global_common_fun[DEVDRV_COMMON_MSG_TYPE_MAX])(u32 devid, void *data, u32 in_data_len, u32 out_data_len,
                                                     u32 *real_out_len);
struct rw_semaphore g_common_rw_sem[DEVDRV_COMMON_MSG_TYPE_MAX];

struct rw_semaphore *devdrv_get_common_msg_rw_sem(void)
{
    return g_common_rw_sem;
}

STATIC int rx_msg_common_msg_process(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len,
    u32 *real_out_len)
{
    struct devdrv_non_trans_msg_desc *msg_desc = NULL;
    struct devdrv_msg_chan *chan = devdrv_find_msg_chan(msg_chan);
    int ret;
    u32 cost_time;

    if (chan == NULL) {
        if (devdrv_is_dev_hot_reset() != true) {
            devdrv_err("Input parameter msg_chan is null.\n");
        }
        return -EINVAL;
    }
    if (data == NULL) {
        devdrv_err("Input parameter data is null. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
        return -EINVAL;
    }
    if (real_out_len == NULL) {
        devdrv_err("Input parameter real_out_len is null. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
        return -EINVAL;
    }

    msg_desc = container_of(data, struct devdrv_non_trans_msg_desc, data);
    if (msg_desc->msg_type >= (u32)DEVDRV_COMMON_MSG_TYPE_MAX) {
        devdrv_err("msg_type is invalid. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
        return -EINVAL;
    }

    down_read(&g_common_rw_sem[msg_desc->msg_type]);
    if ((chan->msg_dev->common_msg.common_fun[msg_desc->msg_type] == NULL) &&
        (global_common_fun[msg_desc->msg_type] == NULL)) {
        up_read(&g_common_rw_sem[msg_desc->msg_type]);
        devdrv_warn("Rx common callback func is null. (dev_id=%u; common_type=%d)\n", chan->msg_dev->pci_ctrl->dev_id,
                    msg_desc->msg_type);
        return -EUNATCH;
    }

    cost_time = jiffies_to_msecs(jiffies - chan->stamp);
    if (cost_time > chan->msg_dev->common_msg.com_msg_stat[msg_desc->msg_type].rx_work_max_time) {
        chan->msg_dev->common_msg.com_msg_stat[msg_desc->msg_type].rx_work_max_time = cost_time;
    }
    if (cost_time > DEVDRV_COMMON_WORK_RESQ_TIME) {
        chan->msg_dev->common_msg.com_msg_stat[msg_desc->msg_type].rx_work_delay_cnt++;
        devdrv_info("(dev_id=%u; msg_type=%d; cost_time=%ums)\n",
                    chan->msg_dev->pci_ctrl->dev_id, msg_desc->msg_type, cost_time);
    }

    chan->msg_dev->common_msg.com_msg_stat[msg_desc->msg_type].rx_total_cnt++;
    if (chan->msg_dev->common_msg.common_fun[msg_desc->msg_type] != NULL) {
        ret = chan->msg_dev->common_msg.common_fun[msg_desc->msg_type](devdrv_get_devid_by_dev(chan->msg_dev), data,
                                                                       in_data_len, out_data_len, real_out_len);
    } else {
        ret = global_common_fun[msg_desc->msg_type](devdrv_get_devid_by_dev(chan->msg_dev), data, in_data_len,
                                                    out_data_len, real_out_len);
    }
    up_read(&g_common_rw_sem[msg_desc->msg_type]);

    if (ret == 0) {
        chan->msg_dev->common_msg.com_msg_stat[msg_desc->msg_type].rx_success_cnt++;
    }

    return ret;
}

int devdrv_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                           enum devdrv_common_msg_type msg_type)
{
    struct devdrv_msg_chan *msg_chan = NULL;
    struct devdrv_common_msg_stat *com_msg_stat = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int msg_type_tmp;
    int ret;

    msg_type_tmp = (int)msg_type;
    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Invalid dev_id. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (data == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (real_out_len == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if ((msg_type_tmp < DEVDRV_COMMON_MSG_PCIVNIC) || (msg_type_tmp >= DEVDRV_COMMON_MSG_TYPE_MAX)) {
        devdrv_err("msg_type is invalid. (dev_id=%u; msg_type=%d)\n", devid, msg_type_tmp);
        return -EINVAL;
    }

    pci_ctrl = devdrv_pci_ctrl_get(devid);
    if (pci_ctrl == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("Get pci_ctrl unsuccess. (dev_id=%u)\n", devid);
        } else {
            devdrv_err_limit("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        }
        return -EINVAL;
    }
    if (pci_ctrl->msg_dev == NULL) {
        devdrv_pci_ctrl_put(pci_ctrl);
        devdrv_err("pcie msg dev is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    msg_chan = pci_ctrl->msg_dev->common_msg.msg_chan;
    if ((msg_chan == NULL) || (msg_chan->status == DEVDRV_DISABLE)) {
        devdrv_pci_ctrl_put(pci_ctrl);
        devdrv_err("msg chan is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    com_msg_stat = &(msg_chan->msg_dev->common_msg.com_msg_stat[msg_type]);
    com_msg_stat->tx_total_cnt++;

    ret = devdrv_sync_non_trans_msg_send(msg_chan, data, in_data_len, out_data_len, real_out_len, msg_type);
    if (ret == 0) {
        com_msg_stat->tx_success_cnt++;
    } else if (ret == -EINVAL) {
        com_msg_stat->tx_einval_err++;
    } else if (ret == -ENODEV) {
        com_msg_stat->tx_enodev_err++;
    } else if (ret == -ENOSYS) {
        com_msg_stat->tx_enosys_err++;
    } else if (ret == -ETIMEDOUT) {
        com_msg_stat->tx_etimedout_err++;
    } else {
        com_msg_stat->tx_default_err++;
    }

    devdrv_pci_ctrl_put(pci_ctrl);
    return ret;
}
EXPORT_SYMBOL(devdrv_common_msg_send);

int devdrv_register_common_msg_client(struct devdrv_common_msg_client *msg_client)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 i;

    if (msg_client == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }

    if (msg_client->type >= DEVDRV_COMMON_MSG_TYPE_MAX) {
        devdrv_err("Input parameter is invalid. (msg_client_type=%d)\n", (int)msg_client->type);
        return -EINVAL;
    }

    down_write(&g_common_rw_sem[msg_client->type]);
    global_common_fun[msg_client->type] = msg_client->common_msg_recv;
    up_write(&g_common_rw_sem[msg_client->type]);

    for (i = 0; i < MAX_DEV_CNT; i++) {
        ctrl = devdrv_get_devctrl_by_id(i);
        if (ctrl == NULL) {
            continue;
        }
        if (ctrl->startup_flg != DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK) {
            continue;
        }
        pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;
        if (pci_ctrl == NULL) {
            continue;
        }
        if (pci_ctrl->msg_dev == NULL) {
            devdrv_info("msg_dev is NULL.\n");
            continue;
        }
        down_write(&g_common_rw_sem[msg_client->type]);
        pci_ctrl->msg_dev->common_msg.common_fun[msg_client->type] = msg_client->common_msg_recv;
        up_write(&g_common_rw_sem[msg_client->type]);
        if (msg_client->init_notify != NULL) {
            msg_client->init_notify(pci_ctrl->dev_id, 0);
        }
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_register_common_msg_client);

int devdrv_unregister_common_msg_client(u32 devid, const struct devdrv_common_msg_client *msg_client)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (msg_client == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (msg_client->type >= DEVDRV_COMMON_MSG_TYPE_MAX) {
        devdrv_err("Input parameter is invalid. (dev_id=%u; msg_client_type=%d)\n",
            devid, (int)msg_client->type);
        return -EINVAL;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if (ctrl == NULL) {
        devdrv_info("Device is offline. (dev_id=%u; msg_client_type=%d)\n", devid, (int)msg_client->type);
        return -EINVAL;
    }
    pci_ctrl = ctrl->priv;
    down_write(&g_common_rw_sem[msg_client->type]);
    pci_ctrl->msg_dev->common_msg.common_fun[msg_client->type] = NULL;
    global_common_fun[msg_client->type] = NULL;
    up_write(&g_common_rw_sem[msg_client->type]);
    devdrv_debug("Unregister common msg_client success. (dev_id=%u; msg_client_type=%d)\n", devid, msg_client->type);

    return 0;
}
EXPORT_SYMBOL(devdrv_unregister_common_msg_client);

STATIC struct devdrv_non_trans_msg_chan_info g_common_msg_chan_info = {
    .msg_type = devdrv_msg_client_common,
    .flag = DEVDRV_MSG_SYNC,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .s_desc_size = DEVDRV_NON_TRANS_MSG_DEFAULT_DESC_SIZE,
    .c_desc_size = DEVDRV_NON_TRANS_MSG_DEFAULT_DESC_SIZE,
    .rx_msg_process = rx_msg_common_msg_process,
};

int devdrv_alloc_common_msg_queue(struct devdrv_pci_ctrl *pci_ctrl)
{
    void *msg_chan = NULL;

    msg_chan = devdrv_pcimsg_alloc_non_trans_queue_inner(pci_ctrl->dev_id, &g_common_msg_chan_info);
    if (msg_chan == NULL) {
        devdrv_err("Alloc common msg_queue failed, msg_chan is null. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }

    /* save common msg_chan to msg_dev */
    pci_ctrl->msg_dev->common_msg.msg_chan = (struct devdrv_msg_chan *)msg_chan;

    return 0;
}

void devdrv_free_common_msg_queue(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;
    if ((pci_ctrl->msg_dev == NULL) || (pci_ctrl->msg_dev->common_msg.msg_chan == NULL)) {
        devdrv_info("Input parameter is invalid. (dev_id=%u)\n", pci_ctrl->dev_id);
        return;
    }
    ret = devdrv_pcimsg_free_non_trans_queue_inner((void *)(pci_ctrl->msg_dev->common_msg.msg_chan));
    if (ret != 0) {
        devdrv_info("No need to free common msg_queue. (dev_id=%u; ret=%d)\n", pci_ctrl->dev_id, ret);
    }
    devdrv_info("Free common msg_queue success. (dev_id=%u)\n", pci_ctrl->dev_id);
    pci_ctrl->msg_dev->common_msg.msg_chan = NULL;

    return;
}

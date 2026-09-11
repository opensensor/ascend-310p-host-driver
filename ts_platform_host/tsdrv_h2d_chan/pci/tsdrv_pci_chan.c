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
#include <linux/types.h>
#include <linux/delay.h>

#include "ka_system_pub.h"
#include "ka_base_pub.h"
#include "ka_barrier_pub.h"
#include "tsdrv_pci_chan.h"
#include "tsdrv_h2d_chan_ops.h"
#include "tsdrv_sync.h"
#include "tsdrv_log.h"

static struct tsdrv_h2d_rx_handler pci_slow_rx_handler[TSDRV_MAX_DAVINCI_NUM];
static struct tsdrv_h2d_rx_handler pci_fast_rx_handler[TSDRV_MAX_DAVINCI_NUM];


STATIC int tsdrv_pci_slow_rx_handler_register(u32 devid, u32 cmd_type,
    int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len))
{
#ifndef TSDRV_UT
    if (devid >= TSDRV_MAX_DAVINCI_NUM || cmd_type >= MAX_D2H_RX_CMD_NUM) {
        TSDRV_PRINT_ERR("invalid cmd_type=%u or devid=%u\n", cmd_type, devid);
        return -ENODEV;
    }
    pci_slow_rx_handler[devid].rx_handler[cmd_type] = rx_handler;
#endif
    return 0;
}

int tsdrv_pci_fast_rx_handler_register(u32 devid, u32 cmd_type,
    int (*rx_handler)(u32 devid, struct tsdrv_msg_info *msg, u32 *ack_len))
{
#ifndef TSDRV_UT
    if (devid >= TSDRV_MAX_DAVINCI_NUM || cmd_type >= MAX_D2H_RX_CMD_NUM) {
        TSDRV_PRINT_ERR("invalid cmd_type=%u or devid=%u\n", cmd_type, devid);
        return -ENODEV;
    }
    pci_fast_rx_handler[devid].rx_handler[cmd_type] = rx_handler;
#endif
    return 0;
}

enum pci_chan_status {
    CHAN_SETUP = 0,
    CHAN_INIT,
    CHAN_RELEASE
};

struct pci_chan {
    ka_atomic_t status;
    void *msg_chan;
};

enum pci_chan_type {
    PCI_CHAN_SLOW,
    PCI_CHAN_FAST
};

static struct pci_chan g_pci_fast_chan[TSDRV_MAX_DAVINCI_NUM];
static struct pci_chan g_pci_slow_chan[TSDRV_MAX_DAVINCI_NUM];

static struct pci_chan *get_pci_chan(u32 devid, enum pci_chan_type type)
{
    struct pci_chan *chan = NULL;

    switch (type) {
        case PCI_CHAN_SLOW:
            chan = &g_pci_slow_chan[devid];
            break;
        case PCI_CHAN_FAST:
            chan = &g_pci_fast_chan[devid];
            break;
    }
    return chan;
}

STATIC bool tsdrv_pci_slow_chan_ready(u32 devid)
{
    struct pci_chan *chan = NULL;
    enum pci_chan_status status;
#ifndef TSDRV_UT
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        return false;
    }
    chan = get_pci_chan(devid, PCI_CHAN_SLOW);
    if (chan == NULL)
        return false;
    status = (enum pci_chan_status)ka_base_atomic_read(&chan->status);
#endif
    return status == CHAN_INIT;
}

static void set_pci_chan_init(u32 devid, enum pci_chan_type type, void *msg_chan)
{
    struct pci_chan *chan = NULL;

    chan = get_pci_chan(devid, type);
    chan->msg_chan = msg_chan;
    ka_wmb();

    ka_base_atomic_set(&chan->status, CHAN_INIT);
}

static int set_pci_chan_exit(u32 devid, enum pci_chan_type type)
{
    struct pci_chan *chan = NULL;
    int loop = 100; // wait for 10 sec maximum

    chan = get_pci_chan(devid, type);
    while (loop > 0) {
        enum pci_chan_status status = ka_base_atomic_cmpxchg(&chan->status, CHAN_INIT, CHAN_RELEASE);
        if (status == CHAN_INIT || status == CHAN_SETUP) {
            chan->msg_chan = NULL;
            ka_wmb();
            break;
        }
        loop--;
        ka_system_msleep(100); // wait for 100 msec for each loop
    }
    if (loop == 0) {
        TSDRV_PRINT_ERR("pci chan exit fail, devid=%u type=%d\n", devid, (int)type);
        return -ENODEV;
    }
    ka_base_atomic_set(&chan->status, CHAN_SETUP);
    return 0;
}

static int set_pci_chan_setup(enum pci_chan_type type)
{
    u32 devid;

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        struct pci_chan *chan = get_pci_chan(devid, type);
        chan->msg_chan = NULL;
        ka_base_atomic_set(&chan->status, CHAN_SETUP);
    }
    return 0;
}

STATIC int set_pci_chan_cleanup(enum pci_chan_type type)
{
    int err = 0;
    u32 devid;

    for (devid = 0; devid < TSDRV_MAX_DAVINCI_NUM; devid++) {
        if (set_pci_chan_exit(devid, type) != 0) {
            TSDRV_PRINT_ERR("pci chan exit fail, devid=%u type=%d\n", devid, (int)type);
            err = -ENODEV;
        }
    }
    return err;
}

static void pci_slow_chan_init_callback(u32 devid, int status)
{
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid=%u\n", devid);
        return;
    }
    set_pci_chan_init(devid, PCI_CHAN_SLOW, NULL);
}

static int pci_slow_chan_rx(u32 devid, void *data, u32 in_data_len, u32 out_data_len,
    u32 *real_out_len)
{
    if (devid >= TSDRV_MAX_DAVINCI_NUM) {
        TSDRV_PRINT_ERR("invalid devid=%u\n", devid);
        return -EFAULT;
    }
    return tsdrv_chan_msg_proc(devid, data, in_data_len, real_out_len);
}

STATIC struct devdrv_common_msg_client pci_slow_chan_client = {
    .type = DEVDRV_COMMON_MSG_DEVDRV_TSDRV,
    .common_msg_recv = pci_slow_chan_rx,
    .init_notify = pci_slow_chan_init_callback,
};

static int pci_slow_chan_setup(void)
{
    int err;

    err = set_pci_chan_setup(PCI_CHAN_SLOW);
    if (err != 0) {
        return err;
    }
    return err;
}

static int pci_slow_chan_cleanup(void)
{
    int err;

    err = set_pci_chan_cleanup(PCI_CHAN_SLOW);
    if (err != 0) {
#ifndef TSDRV_UT
        return err;
#endif
    }
    return err;
}

static int pci_fast_chan_setup(void)
{
    return set_pci_chan_setup(PCI_CHAN_FAST);
}

static int pci_fast_chan_cleanup(void)
{
    return set_pci_chan_cleanup(PCI_CHAN_FAST);
}

STATIC int tsdrv_pci_slow_msg_send(u32 devid, void *tx, size_t tx_size)
{
    u32 out_len;

    return devdrv_common_msg_send(devid, tx, tx_size, tx_size, &out_len, DEVDRV_COMMON_MSG_DEVDRV_TSDRV);
}

STATIC int tsdrv_pci_fast_msg_send(u32 devid, void *tx, size_t tx_size)
{
    struct pci_chan *chan = NULL;
    enum pci_chan_status status;
    u32 out_len;
#ifndef TSDRV_UT
    chan = get_pci_chan(devid, PCI_CHAN_FAST);
    status = ka_base_atomic_read(&chan->status);
    if (unlikely(status != CHAN_INIT)) {
        TSDRV_PRINT_ERR("invalid chan status, devid=%u status=%d\n", devid, (int)status);
        return -EFAULT;
    }
#endif
    return devdrv_sync_msg_send(chan->msg_chan, tx, tx_size, tx_size, &out_len);
}

STATIC int tsdrv_pci_slow_chan_init(u32 devid)
{
    int err;

    TSDRV_PRINT_INFO("register common msg, type=%d\n", pci_slow_chan_client.type);
    err = devdrv_register_common_msg_client(&pci_slow_chan_client);
    if (err != 0) {
        TSDRV_PRINT_ERR("common msg client fail, err=%d\n", err);
        return err;
    }
    return 0;
}

STATIC int tsdrv_pci_slow_chan_exit(u32 devid)
{
    int err = devdrv_unregister_common_msg_client(devid, &pci_slow_chan_client);
    if (err != 0) {
        TSDRV_PRINT_ERR("pci slow chan exit fail, err=%d\n", err);
        return err;
    }
    return err;
}

STATIC int tsdrv_pci_fast_chan_init(u32 devid)
{
    void *fast_chan = NULL;

    set_pci_chan_init(devid, PCI_CHAN_FAST, fast_chan);
    return 0;
}

STATIC int tsdrv_pci_fast_chan_exit(u32 devid)
{
    return 0;
}

int tsdrv_pci_chan_setup(void)
{
    int err = pci_fast_chan_setup();
    if (err != 0) {
        TSDRV_PRINT_ERR("pci fast chan init fail, err=%d\n", err);
        return err;
    }
    err = pci_slow_chan_setup();
    if (err != 0) {
        TSDRV_PRINT_ERR("pci slow chan init fail, err=%d\n", err);
        goto err_pci_slow_chan_init;
    }
    return 0;
err_pci_slow_chan_init:
    (void)pci_fast_chan_cleanup();
    return err;
}

int tsdrv_pci_chan_cleanup(void)
{
    int err = pci_slow_chan_cleanup();
    if (err != 0) {
#ifndef TSDRV_UT
        return err;
#endif
    }
    err = pci_fast_chan_cleanup();
    if (err != 0) {
#ifndef TSDRV_UT
        return err;
#endif
    }
    return 0;
}

static const struct tsdrv_h2d_chan_ops g_pci_chan_ops = {
    .h2d_chan_setup = tsdrv_pci_chan_setup,
    .h2d_chan_cleanup = tsdrv_pci_chan_cleanup,

    .h2d_fast_chan_init = tsdrv_pci_fast_chan_init,
    .h2d_fast_chan_exit = tsdrv_pci_fast_chan_exit,

    .h2d_slow_chan_ready = tsdrv_pci_slow_chan_ready,

    .h2d_slow_chan_init = tsdrv_pci_slow_chan_init,
    .h2d_slow_chan_exit = tsdrv_pci_slow_chan_exit,

    .h2d_slow_msg_send = tsdrv_pci_slow_msg_send,
    .h2d_fast_msg_send = tsdrv_pci_fast_msg_send,

    .fast_rx_handler_register = tsdrv_pci_fast_rx_handler_register,
    .slow_rx_handler_register = tsdrv_pci_slow_rx_handler_register,
};

const struct tsdrv_h2d_chan_ops *tsdrv_get_h2d_chan_ops(void)
{
    return &g_pci_chan_ops;
}

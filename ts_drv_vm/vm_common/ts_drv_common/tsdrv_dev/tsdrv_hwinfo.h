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
#ifndef TSDRV_HWINFO_H
#define TSDRV_HWINFO_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#include "devdrv_platform_resource.h"
#include "devdrv_common.h"
#include "tsdrv_kernel_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DOORBELL is assigned like below
 */
#ifdef CFG_SOC_PLATFORM_MINIV3
#define TSDRV_SQ_DB_NUM                 0U
#else /* !CFG_SOC_PLATFORM_MINIV3 */
#define TSDRV_SQ_DB_NUM                 512U    /* 0 ~ 511 */
#endif /* CFG_SOC_PLATFORM_MINIV3 */
#define TSDRV_CQ_DB_NUM                 352U    /* 512 ~ 863 */
#define TSDRV_HCCL_DB_NUM               128U    /* 864 ~ 991 */ /* reserved for hccl */
#define TSDRV_ONLINE_DFX_SQ_DB_NUM      4U      /* 992 ~ 995 */
#define TSDRV_ONLINE_DFX_CQ_DB_NUM      10U     /* 996 ~ 1005 */
#define TSDRV_ONLINE_MBOX_SEND_DB_NUM   1U      /* 1006 */
#define TSDRV_ONLINE_MBOX_READ_DB_NUM   1U      /* 1007 */
#define TSDRV_OFFLINE_DFX_SQ_DB_NUM     4U      /* 1008 ~ 1011 */
#define TSDRV_OFFLINE_DFX_CQ_DB_NUM     10U     /* 1012 ~ 1021 */
#define TSDRV_OFFLINE_MBOX_SEND_DB_NUM  1U      /* 1022 */
#define TSDRV_OFFLINE_MBOX_READ_DB_NUM  1U      /* 1023 */

#ifdef CFG_SOC_PLATFORM_MINIV3
#define TSDRV_DB_NUM                    512U
#else /* !CFG_SOC_PLATFORM_MINIV3 */
#define TSDRV_DB_NUM                    1024U
#endif /* CFG_SOC_PLATFORM_MINIV3 */
#define TSDRV_SQ_DB_OFFSET                  0U
#define TSDRV_CQ_DB_OFFSET                  (TSDRV_SQ_DB_OFFSET + TSDRV_SQ_DB_NUM)
#define TSDRV_HCCL_DB_OFFSET                (TSDRV_CQ_DB_OFFSET + TSDRV_CQ_DB_NUM)
#define TSDRV_ONLINE_DFX_SQ_DB_OFFSET       (TSDRV_HCCL_DB_OFFSET + TSDRV_HCCL_DB_NUM)
#define TSDRV_ONLINE_DFX_CQ_DB_OFFSET       (TSDRV_ONLINE_DFX_SQ_DB_OFFSET + TSDRV_ONLINE_DFX_SQ_DB_NUM)
#define TSDRV_ONLINE_MBOX_SEND_DB_OFFSET    (TSDRV_ONLINE_DFX_CQ_DB_OFFSET + TSDRV_ONLINE_DFX_CQ_DB_NUM)
#define TSDRV_ONLINE_MBOX_READ_DB_OFFSET    (TSDRV_ONLINE_MBOX_SEND_DB_OFFSET + TSDRV_ONLINE_MBOX_SEND_DB_NUM)
#define TSDRV_OFFLINE_DFX_SQ_DB_OFFSET      (TSDRV_ONLINE_MBOX_READ_DB_OFFSET + TSDRV_ONLINE_MBOX_READ_DB_NUM)
#define TSDRV_OFFLINE_DFX_CQ_DB_OFFSET      (TSDRV_OFFLINE_DFX_SQ_DB_OFFSET + TSDRV_OFFLINE_DFX_SQ_DB_NUM)
#define TSDRV_OFFLINE_MBOX_SEND_DB_OFFSET   (TSDRV_OFFLINE_DFX_CQ_DB_OFFSET + TSDRV_OFFLINE_DFX_CQ_DB_NUM)
#define TSDRV_OFFLINE_MBOX_READ_DB_OFFSET   (TSDRV_OFFLINE_MBOX_SEND_DB_OFFSET + TSDRV_OFFLINE_MBOX_SEND_DB_NUM)

#ifdef CFG_SOC_PLATFORM_MINIV3
#define TSDRV_STARS_DB_NUM                  512U
#define TSDRV_STARS_SQ_DB_NUM               512U
#define TSDRV_STARS_CQ_DB_NUM               512U
#define TSDRV_STARS_SQ_DB_OFFSET            0U
#define TSDRV_STARS_CQ_DB_OFFSET            0U
#else /* !CFG_SOC_PLATFORM_MINIV3 */
#define TSDRV_STARS_DB_NUM                  2048U
#define TSDRV_STARS_SQ_DB_NUM               2048U
#define TSDRV_STARS_CQ_DB_NUM               2048U
#define TSDRV_STARS_SQ_DB_OFFSET            0U
#define TSDRV_STARS_CQ_DB_OFFSET            0U
#endif /* CFG_SOC_PLATFORM_MINIV3 */

#define NORMAL_IRQ_MIN_NUM 16
#define TS_DB_NUM_EACH_VF 64U

/* 内部枚举 */
enum tsdrv_db_type {
    TSDRV_SQ_DB = 0,
    TSDRV_CQ_DB = 1,
    TSDRV_HCCL_DB = 2,
    TSDRV_ONLINE_DFX_SQ_DB = 3,
    TSDRV_ONLINE_DFX_CQ_DB = 4,
    TSDRV_ONLINE_MBOX_SEND_DB = 5,
    TSDRV_ONLINE_MBOX_READ_DB = 6,
    TSDRV_OFFLINE_DFX_SQ_DB = 7,
    TSDRV_OFFLINE_DFX_CQ_DB = 8,
    TSDRV_OFFLINE_MBOX_SEND_DB = 9,
    TSDRV_OFFLINE_MBOX_READ_DB = 10,
    TSDRV_MAX_DB = 11
};

/* 对外结构体 */
enum tsdrv_db_type_l {
    TSDRV_SQ_DB_L = 0,
    TSDRV_CQ_DB_L = 1,
    TSDRV_HCCL_DB_L = 2,
    TSDRV_DFX_SQ_DB_L = 3,
    TSDRV_DFX_CQ_DB_L = 4,
    TSDRV_MBOX_SEND_DB_L = 5,
    TSDRV_MBOX_READ_DB_L = 6,
    TSDRV_MAX_DB_L = 7
};

enum tsdrv_mbox_chan {
    TSDRV_TS_MBOX,
    TSDRV_MAX_MBOX_CHANN
};

struct tsdrv_mbox_hwinfo {
    u32 chann_id;
    void __iomem *mbox_tx;          /* ts_mbox_send_vaddr */
    void __iomem *mbox_rx;          /* ts_mbox_rcv_vaddr */
    u32 mbox_size;                  /* sram_size */
    int mbox_rx_irq;                /* irq rx irq index for sending to ts */
    int mbox_rx_request_irq;        /* irq_mailbox_data_ack */
    int mbox_ack_irq;               /* irq index for sending to ts */
    int mbox_ack_request_irq;           /* irq for request_irq */
};

struct tsdrv_cq_hwinfo {
    int dfx_cq_irq;                 /* irq_functional_cq; */
    int dfx_cq_request_irq;
    u32 cq_irq_num;                 /* new */
    u32 thread_bind_irq_num;
    u16 cq_irq[DEVDRV_CQ_IRQ_NUM];  /* Interrupt ID of the ts view */
    u32 cq_request_irq[DEVDRV_CQ_IRQ_NUM];  /* Interrupt ID of the host/device view  */
    phys_addr_t bar_addr;   /* pcie bar addres for host */
};

/* total size for device */
struct tsdrv_db_info {
    phys_addr_t db_paddr;
    size_t db_size;
    void __iomem *dbs;
    size_t db_stride;
    u32 num;
};

/* tsdrv db info for each type */
struct tsdrv_db_hwinfo_t {
    enum tsdrv_db_type db_type;
    void __iomem *dbs;              /* doorbell_vaddr */
    size_t db_stride;               /* size for each doorbell */
    u32 num;
};

struct tsdrv_db_hwinfo {
    /* whole doorbell physical address and size */
    phys_addr_t db_paddr; /* temporary */
    size_t db_size;
    /* whole ts doorbell physical address and size, just for cloudv2 vf mailbox */
    void __iomem *dbs;    /* db base vaddr */
    phys_addr_t ts_db_paddr; /* temporary */
    size_t ts_db_size;
    struct tsdrv_db_hwinfo_t db_hwinfo_t[TSDRV_MAX_DB];
};

struct tsdrv_sq_hwinfo {
    phys_addr_t bar_addr;   /* pcie bar address assigned from */
    phys_addr_t paddr;      /* reserved sq static memory base physical addr */
    size_t size;            /* ts_sq_static_size */
};

struct tsdrv_stars_sqcq_intr_hwinfo {
    phys_addr_t pa;
    void __iomem *va;
    size_t size;
};

struct tsdrv_tsfw_hwinfo {
    phys_addr_t paddr;
    void *vaddr; /* tsfw load address, used for task synk */
    size_t size;
};

// struct devdrv_ts_pdata
struct tsdrv_hwinfo {
    u32 tsid;
    struct tsdrv_tsfw_hwinfo tsfw_hwinfo;
    struct tsdrv_mbox_hwinfo mbox_hwinfo[TSDRV_MAX_MBOX_CHANN];
    struct tsdrv_cq_hwinfo cq_hwinfo;
    struct tsdrv_sq_hwinfo sq_hwinfo;
    struct tsdrv_db_hwinfo db_hwinfo;
    struct tsdrv_stars_sqcq_intr_hwinfo stars_sqcq_intr_hwinfo;
};

struct tsdrv_dev_hwinfo {
    u32 devid;
    u32 chip_id;
    u32 die_id;
    struct tsdrv_hwinfo hwinfo[DEVDRV_MAX_TS_NUM];
};

int tsdrv_dev_hwinfo_init(u32 devid);
void tsdrv_dev_hwinfo_exit(u32 devid);

int tsdrv_set_tsfw_hwinfo(u32 devid, u32 tsid, struct tsdrv_tsfw_hwinfo *tsfw_hwinfo);
int tsdrv_set_ts_sq_hwinfo(u32 devid, u32 tsid, struct tsdrv_sq_hwinfo *sq_hwinfo);
int tsdrv_set_ts_cq_hwinfo(u32 devid, u32 tsid, struct tsdrv_cq_hwinfo *cq_hwinfo);
int tsdrv_set_ts_db_info(u32 devid, u32 tsid, struct tsdrv_db_info *db_info);
int tsdrv_set_stars_db_info(u32 devid, u32 tsid, struct tsdrv_db_info *db_info);
int tsdrv_set_ts_mbox_hwinfo(u32 devid, u32 tsid, struct tsdrv_mbox_hwinfo *mbox_hwinfo);
void tsdrv_set_mbox_db_info(u32 devid, u32 tsid, struct tsdrv_db_info *db_info);
struct tsdrv_cq_hwinfo *tsdrv_get_cq_hwinfo(u32 devid, u32 tsid);

struct tsdrv_tsfw_hwinfo *tsdrv_get_tsfw_hwinfo(u32 devid, u32 tsid);
struct tsdrv_db_hwinfo *tsdrv_get_db_hwinfo(u32 devid, u32 tsid);
struct tsdrv_db_hwinfo_t *tsdrv_get_db_hwinfo_t(u32 devid, u32 tsid, enum tsdrv_db_type db_type);
struct tsdrv_mbox_hwinfo *tsdrv_get_ts_mbox_hwinfo(u32 devid, u32 tsid, u32 chann_id);
struct tsdrv_sq_hwinfo *tsdrv_get_ts_sq_hwinfo(u32 devid, u32 tsid);
void tsdrv_get_db_phy_info(u32 devid, u32 tsid, phys_addr_t *paddr, size_t *size);
int tsdrv_get_dfx_cq_irq_vector(u32 devid, u32 tsid);

#ifdef __cplusplus
}
#endif

#endif /* TSDRV_HWINFO_H */

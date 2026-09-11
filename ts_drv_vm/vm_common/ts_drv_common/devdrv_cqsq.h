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
#ifndef DEVDRV_CQSQ_H
#define DEVDRV_CQSQ_H

#ifdef AOS_LLVM_BUILD
#include <aos_dma.h>
#endif
#include "devdrv_common.h"
#include "tsdrv_kernel_common.h"
#include "tsdrv_device.h"
#include "tsdrv_ioctl.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"

#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3)
#define DEVDRV_SQ_TAIL_OFFSET 0x8
#define DEVDRV_CQ_HEAD_OFFSET 0x888
#define DEVDRV_CQ_TAIL_OFFSET 0x890
#define DEVDRV_SQ_HEAD_OFFSET 0x10
#define DEVDRV_SQ_STATUS_OFFSET 0x14
#else
#define DEVDRV_SQ_HEAD_OFFSET 0
#define DEVDRV_SQ_TAIL_OFFSET 0
#define DEVDRV_CQ_HEAD_OFFSET 0
#define DEVDRV_CQ_TAIL_OFFSET 0
#define DEVDRV_SQ_STATUS_OFFSET 0
#endif

#define DEVDRV_NO_NEED_TO_INFORM 0
#define DEVDRV_HAVE_TO_INFORM 1

struct phy_cq_alloc_para {
    enum phy_sqcq_type type;    // input
    u32 cq_size;                // input
    u32 cq_depth;               // input
    u32 cq_id;                  // output
    u16 cq_irq;                 // output
    phys_addr_t cq_paddr;       // output
    void *cq_vaddr;             // output
};

struct phy_cq_free_para {
    u32 cq_id;
};

struct phy_sq_alloc_para {
    enum phy_sqcq_type type;    // input
    u32 sq_size;                // input
    u32 sq_depth;               // input
    u32 sq_id;                  // output
    phys_addr_t sq_paddr;       // output
    phys_addr_t sq_bar_addr;    // output
    void *sq_vaddr;             // output
};

struct phy_sq_free_para {
    u32 sq_id;
};

struct tsdrv_task_submit_chan {
    u32 devid;
    u32 vfid;
    u32 tsid;
    u32 sqid;
    u32 cqid;
    u64 submit_num;
    u64 complete_num;
    void (*report_handle)(void *report, u32 report_count);
    ka_task_spinlock_t lock; /* Callers may submit tasks in software interrupts.  */
};

struct tsdrv_phy_addr_get {
    u32 offset;
    u32 len;
    phys_addr_t paddr;
    u32 paddr_len;
};

static inline struct devdrv_sq_sub_info *tsdrv_get_sq_sub_info(struct tsdrv_ts_resource *ts_res, u32 sqid)
{
    struct devdrv_ts_sq_info *sq_info = NULL;
    struct devdrv_sq_sub_info *sq_sub = NULL;
    phys_addr_t info_mem;

    info_mem = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    sq_info = devdrv_calc_sq_info(info_mem, sqid);
    sq_sub = (struct devdrv_sq_sub_info *)sq_info->sq_sub;

    return sq_sub;
}

static inline struct devdrv_cq_sub_info *tsdrv_get_cq_sub_info(struct tsdrv_ts_resource *ts_res, u32 cqid)
{
    struct devdrv_ts_cq_info *cq_info = NULL;
    struct devdrv_cq_sub_info *cq_sub = NULL;
    phys_addr_t info_mem;

    info_mem = ts_res->mem_info[DEVDRV_INFO_MEM].virt_addr;
    cq_info = devdrv_calc_cq_info(info_mem, cqid);
    cq_sub = (struct devdrv_cq_sub_info *)cq_info->cq_sub;

    return cq_sub;
}

void devdrv_cqsq_destroy(u32 devid, u32 tsid);
int devdrv_cqsq_init(u32 devid, u32 tsid, u32 num_sq, u32 num_cq);
struct devdrv_ts_cq_info *devdrv_get_cq_exist(struct tsdrv_ts_resource *ts_res,
    struct tsdrv_ts_ctx *ts_ctx);
void devdrv_free_cq(struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx, struct devdrv_ts_cq_info *cq_info);
void tsdrv_free_sq(struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx, struct devdrv_ts_sq_info *sq_info);
void devdrv_destroy_functional_cqsq(struct devdrv_info *info, u32 tsid);
int devdrv_alloc_sq_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_alloc_cq_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_add_sq_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int devdrv_add_cq_msg_chan(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
int tsdrv_alloc_ts_sqcq(u32 devid, u32 vfid, u32 tsid);
void tsdrv_free_ts_sqcq(u32 devid, u32 vfid, u32 tsid);
int tsdrv_ioctl_enable_stream(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_ioctl_disable_stream(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);

int tsdrv_dfx_cqsq_init(u32 devid, u32 tsnum);
void tsdrv_dfx_cqsq_exit(u32 devid, u32 tsnum);
int devdrv_ioctl_sqcq_alloc(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int devdrv_ioctl_sqcq_free(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_ioctl_sq_msg_send(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_ioctl_sqcq_set(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_ioctl_sqcq_query(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_ioctl_cq_report_release(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
int tsdrv_ioctl_get_sq_head(struct tsdrv_ctx *ctx, struct devdrv_ioctl_arg *arg);
void devdrv_calc_irq_cq_range(u32 irq_id, u32 irq_num, u32 cq_num, u32 *first_cq_index, u32 *last_cq_index);
void devdrv_calc_cq_irq_id(u32 cq_index, u32 irq_num, u32 cq_num, u16 *irq_id);
int tsdrv_phy_sq_alloc(struct tsdrv_ctx *ctx, u32 tsid, struct phy_sq_alloc_para *para);
int tsdrv_phy_sq_free(struct tsdrv_ctx *ctx, u32 tsid, struct phy_sq_free_para *para);
void tsdrv_phy_sq_alloc_restore(struct tsdrv_ctx *ctx, u32 tsid, u32 sqid);
int tsdrv_phy_cqid_exist_chk(struct tsdrv_ctx *ctx, u32 tsid, u32 cqid, enum phy_sqcq_type type);
int tsdrv_get_sq_mem_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 sqId, u32 sq_type, struct tsdrv_phy_addr_get *info);
int tsdrv_get_cq_mem_phy_addr(struct tsdrv_ctx *ctx, u32 tsId, u32 cqId, u32 cq_type, struct tsdrv_phy_addr_get *info);
int tsdrv_get_sq_mem_map_prot(struct tsdrv_ctx *ctx, u32 tsId, u32 sq_type, ka_pgprot_t vm_page_prot, ka_pgprot_t *prot);
int tsdrv_get_cq_mem_map_prot(struct tsdrv_ctx *ctx, u32 tsId, struct tsdrv_mem_map_para *map_para,
    ka_pgprot_t vm_page_prot, ka_pgprot_t *prot);

int tsdrv_phy_cq_alloc(struct tsdrv_ctx *ctx, u32 tsid, struct phy_cq_alloc_para *para);
int tsdrv_phy_cq_free(struct tsdrv_ctx *ctx, u32 tsid, struct phy_cq_free_para *para);
int tsdrv_sq_type_check(u32 devid, u32 fid, u32 tsid, u32 sqid, enum phy_sqcq_type type);
int tsdrv_phy_sqid_exist_chk(struct tsdrv_ctx *ctx, u32 tsid, u32 sqid, enum phy_sqcq_type type);

int tsdrv_phy_sq_head_update(u32 devid, u32 fid, u32 tsid, u32 sq_id, u32 sq_head);

int tsdrv_ring_phy_cq_doorbell(u32 devid, u32 tsid, u32 cq_id, u32 cq_head);

struct devdrv_ts_cq_info *tsdrv_get_cq_info(u32 devid, u32 fid, u32 tsid, u32 cq_id);
struct devdrv_ts_sq_info *tsdrv_get_sq_info(u32 devid, u32 fid, u32 tsid, u32 sq_id);
int tsdrv_sq_exist_check(struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx, u32 sqId);
int tsdrv_cq_exist_check(struct tsdrv_ts_resource *ts_res, struct tsdrv_ctx *ctx, u32 cqId);
bool tsdrv_is_sq_belong_to_proc(struct tsdrv_ctx *ctx, u32 tsId, u32 sqId);
bool tsdrv_is_cq_belong_to_proc(struct tsdrv_ctx *ctx, u32 tsId, u32 cqId);
int tsdrv_phy_cq_recycle(struct tsdrv_ctx *ctx, u32 tsid, struct phy_cq_free_para *para);
void tsdrv_set_cq_doorbell(u32 devid, u32 tsid, u32 cqid, u32 cq_head);
void tsdrv_update_cq_head(struct tsdrv_ts_resource *ts_res, u32 cqid, u32 cq_head);
void tsdrv_dma_sync_cpu(u32 devid, dma_addr_t dma_addr, size_t size, ka_dma_data_direction_t dir);

int tsdrv_kernel_alloc_sq(u32 devid, u32 vfid, u32 tsid, u32 sqe_size, u32 depth, enum phy_sqcq_type type);
void tsdrv_kernel_free_sq(u32 devid, u32 vfid, u32 tsid, u32 sqid);
int tsdrv_kernel_alloc_cq(u32 devid, u32 vfid, u32 tsid, u32 cqe_size, u32 depth, enum phy_sqcq_type type);
void tsdrv_kernel_free_cq(u32 devid, u32 vfid, u32 tsid, u32 cqid);
int tsdrv_sync_id_maping_mailbox(struct devdrv_mailbox *mailbox,
    struct tsdrv_ctx *ctx,
    struct resource_mapping_notice_mailbox_t *msg);

void tsdrv_trigger_phy_cq_scan(u32 devid, u32 tsid, u32 phy_cqid);
void tsdrv_reserved_sqcq_restore(u32 devid, u32 vfid, u32 tsid);

void tsdrv_sq_db_unmap(struct tsdrv_ctx *ctx, struct devdrv_ts_sq_info *sq_info);
void tsdrv_sq_head_tail_unmap(struct tsdrv_ctx *ctx, struct devdrv_ts_sq_info *sq_info);

#endif

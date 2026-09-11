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
#include "devdrv_dma.h"
#include "devdrv_util.h"

#define DMA_QUEUE_SQ_TAIL 0xc
#define DMA_QUEUE_CQ_HEAD 0x1c
#define DMA_QUEUE_CQ_TAIL 0x3c
#define DMA_QUEUE_SQ_READ_ERR_PTR 0x68
#define DMA_QUEUE_INT_MASK 0x44
#define DMA_CHAN_LOCAL_USED_START_INDEX 0
#define DMA_CHAN_REMOTE_USED_START_INDEX 25
#define DMA_QUEUE_CQ_HEAD_VALID_BIT 0xFFFF
#define DMA_QUEUE_CQ_TAIL_VALID_BIT 0xFFFF
#define DMA_QUEUE_SQ_HEAD_VALID_BIT 0xFFFF0000
#define DMA_QUEUE_SQ_HEAD_OFFSET 16

void devdrv_dma_chan_ptr_show(struct devdrv_dma_channel *dma_chan, int wait_status)
{
    u32 sq_tail, cq_tail, cq_head, sq_head;
    void __iomem *io_base = dma_chan->io_base;

    sq_tail = readl(io_base + DMA_QUEUE_SQ_TAIL);
    cq_head = readl(io_base + DMA_QUEUE_CQ_HEAD);
    cq_tail = readl(io_base + DMA_QUEUE_CQ_TAIL) & DMA_QUEUE_CQ_TAIL_VALID_BIT;
    sq_head = (readl(io_base + DMA_QUEUE_CQ_TAIL) & DMA_QUEUE_SQ_HEAD_VALID_BIT) >> DMA_QUEUE_SQ_HEAD_OFFSET;

    if (is_need_dma_copy_retry(dma_chan->dma_dev->dev_id, wait_status) == true) {
        devdrv_warn("dma_chan ptr show. (hardware_sq_tail=0x%x; cq_head=0x%x; cq_tail=0x%x; sq_head=0x%x; "
                   "software_sq_tail=0x%x; sq_head=0x%x; cq_head=0x%x)\n",
                   sq_tail, cq_head, cq_tail, sq_head, dma_chan->sq_tail, dma_chan->sq_head, dma_chan->cq_head);
    } else {
        devdrv_err("dma_chan ptr show. (hardware_sq_tail=0x%x; cq_head=0x%x; cq_tail=0x%x; sq_head=0x%x; "
                   "software_sq_tail=0x%x; sq_head=0x%x; cq_head=0x%x)\n",
                   sq_tail, cq_head, cq_tail, sq_head, dma_chan->sq_tail, dma_chan->sq_head, dma_chan->cq_head);
    }
}

void devdrv_set_dma_sq_tail(void __iomem *io_base, u32 val)
{
    writel(val, io_base + DMA_QUEUE_SQ_TAIL);
}

void devdrv_set_dma_cq_head(void __iomem *io_base, u32 val)
{
    writel(val, io_base + DMA_QUEUE_CQ_HEAD);
}

u32 devdrv_get_dma_cq_head(void __iomem *io_base)
{
    return (readl(io_base + DMA_QUEUE_CQ_HEAD) & DMA_QUEUE_CQ_HEAD_VALID_BIT);
}

u32 devdrv_get_dma_cq_tail(void __iomem *io_base)
{
    return (readl(io_base + DMA_QUEUE_CQ_TAIL) & DMA_QUEUE_CQ_TAIL_VALID_BIT);
}

u32 devdrv_get_sq_err_ptr(const void __iomem *io_base)
{
    return readl(io_base + DMA_QUEUE_SQ_READ_ERR_PTR);
}

void devdrv_dma_set_sq_addr_info(struct devdrv_dma_sq_node *sq_desc, u64 src_addr, u64 dst_addr, u32 length)
{
    sq_desc->src_addr_l = (u32)src_addr;
    sq_desc->src_addr_h = (u32)(src_addr >> DEVDRV_ADDR_SHIFT_32);

    sq_desc->dst_addr_l = (u32)dst_addr;
    sq_desc->dst_addr_h = (u32)(dst_addr >> DEVDRV_ADDR_SHIFT_32);

    sq_desc->length = length;
}

void devdrv_dma_set_sq_attr(struct devdrv_dma_sq_node *sq_desc, u32 opcode, u32 attr,
    struct devdrv_dma_dev *dma_dev, u32 wd_barrier, u32 rd_barrier)
{
    sq_desc->opcode = opcode;
    /* RO.remote flag for RD.remote np and WD.remote p */
    sq_desc->attr = attr;
    /* RO.local flag for RD.local p and WD.local np */
    sq_desc->attr_d = attr;
    sq_desc->pf = dma_dev->dma_pf_num;
    sq_desc->vfen = dma_dev->dma_vf_en;
    sq_desc->vf = dma_dev->dma_vf_num;
    sq_desc->wd_barrier = wd_barrier;
    sq_desc->rd_barrier = rd_barrier;

    devdrv_debug_spinlock("Set sq attr ok. (opcode=%x; attr=%x; pf=%x; vf=%x; wb_barrier=%x; rd_barrier=%x)\n",
        opcode, attr, dma_dev->dma_pf_num, dma_dev->dma_vf_num, wd_barrier, rd_barrier);
}

void devdrv_dma_set_sq_irq(struct devdrv_dma_sq_node *sq_desc, u32 rdie, u32 ldie, u32 msi)
{
    sq_desc->rdie = rdie;
    sq_desc->ldie = ldie;
    sq_desc->msi_l = msi & DMA_MSI_L_MASK;
    sq_desc->msi_h = (msi >> DMA_MSI_H_BIT_OFFSET) & DMA_MSI_H_MASK;
    devdrv_debug_spinlock("Set sq_irq. (rdie=%x; ldie=%x; msi=%x)\n", rdie, ldie, msi);
}

void devdrv_dma_set_passid(struct devdrv_dma_sq_node *sq_desc, u32 loc_passid, int direction,
                           int pava_flag, int connect_type)
{
    if (pava_flag == DEVDRV_DMA_PA_COPY) {
        sq_desc->pa_loc = DEVDRV_DMA_DES_PA_LOC_PA;
        sq_desc->addrt_d = DEVDRV_DMA_DES_AT_LOC_PA;
        return;
    }

    if (direction == DEVDRV_DMA_SYS_TO_SYS) {
        sq_desc->pa_rmt = DEVDRV_DMA_DES_PA_RMT_PA;
        sq_desc->pa_loc = DEVDRV_DMA_DES_PA_LOC_PA;
        sq_desc->addrt_d = DEVDRV_DMA_DES_AT_LOC_PA;
        return;
    }

    if ((connect_type == CONNECT_PROTOCOL_PCIE) || (direction == DEVDRV_DMA_HOST_TO_DEVICE)) {
        sq_desc->pa_rmt = DEVDRV_DMA_DES_PA_RMT_PA;
        sq_desc->pa_loc = DEVDRV_DMA_DES_PA_LOC_VA;
        sq_desc->addrt_d = DEVDRV_DMA_DES_AT_LOC_VA;
        sq_desc->flow_id_rmt = loc_passid & DEVDRV_DMA_DES_FLOW_ID_RMT_MASK;
        sq_desc->flow_id_loc_l = (loc_passid >> DEVDRV_DMA_DES_FLOW_ID_LOC_L_SHIFT) & DEVDRV_DMA_DES_FLOW_ID_LOC_L_MASK;
        sq_desc->flow_id_loc_h = (loc_passid >> DEVDRV_DMA_DES_FLOW_ID_LOC_H_SHIFT) & DEVDRV_DMA_DES_FLOW_ID_LOC_H_MASK;
    } else {
        sq_desc->pa_rmt = DEVDRV_DMA_DES_PA_RMT_VA;
        sq_desc->pa_loc = DEVDRV_DMA_DES_PA_LOC_PA;
        sq_desc->addrt_d = DEVDRV_DMA_DES_AT_LOC_PA;
        if (loc_passid != DEVDRV_DMA_PASSID_DEFAULT) {
            sq_desc->pasid = loc_passid;
            sq_desc->prfen = DEVDRV_DMA_DES_PA_RMT_VA;
        }
    }
}

STATIC bool devdrv_dma_get_cq_valid_flip(struct devdrv_dma_cq_node *cq_desc, u32 rounds)
{
    /* In order to avoid the software clearing the valid flag,
    the hardware will change from 1 to 0 after the cq is used up,
    and so on. 1 is valid for the first time */
    if ((rounds & 0x1) != 0) {
        return (cq_desc->vld == 0);
    } else {
        return (cq_desc->vld == 1);
    }
}

STATIC void devdrv_dma_set_cq_invalid_flip(struct devdrv_dma_cq_node *cq_desc)
{
    /* no need to set */
    (void)cq_desc;
}

STATIC bool devdrv_dma_get_cq_valid_normal(struct devdrv_dma_cq_node *cq_desc, u32 rounds)
{
    (void)rounds;
    return (cq_desc->vld == 1);
}

STATIC void devdrv_dma_set_cq_invalid_normal(struct devdrv_dma_cq_node *cq_desc)
{
    cq_desc->vld = 0;
}

u32 devdrv_dma_get_cq_sqhd(struct devdrv_dma_cq_node *cq_desc)
{
    return (u32)cq_desc->sqhd;
}

u32 devdrv_dma_get_cq_status(struct devdrv_dma_cq_node *cq_desc)
{
    return (u32)cq_desc->status;
}

void devdrv_dma_ops_init(struct devdrv_dma_dev *dma_dev, u32 chip_type)
{
    if (chip_type == HISI_MINI_V1) {
        dma_dev->ops.devdrv_dma_get_cq_valid = devdrv_dma_get_cq_valid_normal;
        dma_dev->ops.devdrv_dma_set_cq_invalid = devdrv_dma_set_cq_invalid_normal;
    } else {
        dma_dev->ops.devdrv_dma_get_cq_valid = devdrv_dma_get_cq_valid_flip;
        dma_dev->ops.devdrv_dma_set_cq_invalid = devdrv_dma_set_cq_invalid_flip;
    }
}

STATIC void devdrv_set_pf_dma_queue_chan_pause(struct devdrv_dma_dev *dma_dev,
    bool pause_flag, u32 chan_id)
{
    void __iomem *reg_addr = NULL;
    u32 regval;

    reg_addr = dma_dev->dma_chan_base + DEVDRV_DMA_CHAN_OFFSET * chan_id + DEVDRV_DMA_QUEUE_CTRL0;
    regval = readl(reg_addr);
    /* bit4 dma_queue_pause */
    if (pause_flag) {
        regval |= (u32)BIT(4);
    } else {
        regval &= (u32)~BIT(4);
    }

    writel(regval, reg_addr);
}

void devdrv_set_pf_dma_queue_pause(struct devdrv_dma_dev *dma_dev, bool pause_flag)
{
    devdrv_set_pf_dma_queue_chan_pause(dma_dev, pause_flag, DMA_CHAN_LOCAL_USED_START_INDEX);
    devdrv_set_pf_dma_queue_chan_pause(dma_dev, pause_flag, DMA_CHAN_REMOTE_USED_START_INDEX);
}

// mask dma done and all err interrupts int suspend
void devdrv_dma_set_interrupt_mask(struct devdrv_dma_channel *dma_chan)
{
    writel(0xFFFFFFFFU, dma_chan->io_base + DMA_QUEUE_INT_MASK);
}

// unmask dma done and all err interrupts int suspend
void devdrv_dma_set_interrupt_unmask(struct devdrv_dma_channel *dma_chan)
{
    writel(0x0, dma_chan->io_base + DMA_QUEUE_INT_MASK);
}
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

#ifndef _DMA_ADAPT_H_
#define _DMA_ADAPT_H_

#include <asm/io.h>

#define DEVDRV_EACH_DMA_IRQ_NUM 2

#define DEVDRV_DMA_CHAN_OFFSET 0x100

#define DEVDRV_DMA_SML_PKT_DATA_SIZE 32
#define DEVDRV_DMA_SML_PKT_SQ_DESC_NUM 2

#define DEVDRV_DMA_SQ_ATTR_SO (1U << 0)
#define DEVDRV_DMA_SQ_ATTR_RO (1U << 1)

#define DEVDRV_DMA_REG_ALIGN_SIZE 0x40 /* 64B */

#define DEVDRV_ADDR_SHIFT_32 32
#define DEVDRV_DMA_QUEUE_CTRL0 0x20

/* the opcode of SQ descriptor */
enum devdrv_dma_opcode {
    DEVDRV_DMA_SMALL_PACKET = 0x1,
    DEVDRV_DMA_READ = 0x2,
    DEVDRV_DMA_WRITE = 0x3,
    DEVDRV_DMA_LOOP = 0x4,
};

#define DEVDRV_DMA_DES_PA_RMT_PA 0
#define DEVDRV_DMA_DES_PA_RMT_VA 1
#define DEVDRV_DMA_DES_PA_LOC_PA 0
#define DEVDRV_DMA_DES_PA_LOC_VA 1
#define DEVDRV_DMA_DES_AT_LOC_PA 0
#define DEVDRV_DMA_DES_AT_LOC_VA 2

#define DEVDRV_DMA_DES_FLOW_ID_RMT_MASK    0xFF
#define DEVDRV_DMA_DES_FLOW_ID_LOC_L_SHIFT 8
#define DEVDRV_DMA_DES_FLOW_ID_LOC_L_MASK  0x1F
#define DEVDRV_DMA_DES_FLOW_ID_LOC_H_SHIFT 13
#define DEVDRV_DMA_DES_FLOW_ID_LOC_H_MASK  0x7

struct devdrv_dma_sq_node {
    u32 opcode : 4;
    u32 drop : 1;
    u32 nw : 1;
    u32 wd_barrier : 1;
    u32 rd_barrier : 1;
    u32 ldie : 1;
    u32 rdie : 1;
    u32 loop_barrier : 1;
    u32 spd_barrier : 1;
    u32 attr : 3;
    u32 reserved3 : 1;
    u32 addrt : 2;
    u32 reserved4 : 2;
    u32 pf : 3;
    u32 vfen : 1;
    u32 vf : 8;
    u32 pasid : 20;
    u32 er : 1;
    u32 pmr : 1;
    u32 prfen : 1;
    u32 reserved5 : 1;
    u32 msi_l : 8;
    u32 flow_id_rmt : 8;
    u32 msi_h : 3;
    u32 flow_id_loc_l : 5;
    u32 th : 1;
    u32 ph : 2;
    u32 flow_id_loc_h : 3;
    u32 pa_rmt : 1;
    u32 pa_loc : 1;
    u32 attr_d : 3;
    u32 addrt_d : 2;
    u32 th_d : 1;
    u32 ph_d : 2;
    u32 length;
    u32 src_addr_l;
    u32 src_addr_h;
    u32 dst_addr_l;
    u32 dst_addr_h;
};

#define DMA_MSI_L_MASK 0xff
#define DMA_MSI_H_BIT_OFFSET 8
#define DMA_MSI_H_MASK 0x7

struct devdrv_dma_cq_node {
    u32 reserved1;
    u32 reserved2;
    u32 sqhd : 16;
    u32 reserved3 : 16;
    u32 reserved4 : 16;
    u32 vld : 1;
    u32 status : 15;
};

struct devdrv_dma_channel;
struct devdrv_dma_dev;

void devdrv_dma_chan_ptr_show(struct devdrv_dma_channel *dma_chan, int wait_status);
u32 devdrv_get_sq_err_ptr(const void __iomem *io_base);

void devdrv_set_dma_sq_tail(void __iomem *io_base, u32 val);
void devdrv_set_dma_cq_head(void __iomem *io_base, u32 val);
u32 devdrv_get_dma_cq_head(void __iomem *io_base);
u32 devdrv_get_dma_cq_tail(void __iomem *io_base);

void devdrv_dma_set_sq_addr_info(struct devdrv_dma_sq_node *sq_desc, u64 src_addr, u64 dst_addr, u32 length);
void devdrv_dma_set_sq_attr(struct devdrv_dma_sq_node *sq_desc, u32 opcode, u32 attr,
    struct devdrv_dma_dev *dma_dev, u32 wd_barrier, u32 rd_barrier);
void devdrv_dma_set_sq_irq(struct devdrv_dma_sq_node *sq_desc, u32 rdie, u32 ldie, u32 msi);
void devdrv_dma_set_passid(struct devdrv_dma_sq_node *sq_desc, u32 loc_passid, int direction,
                           int pava_flag, int connect_type);
u32 devdrv_dma_get_cq_sqhd(struct devdrv_dma_cq_node *cq_desc);
u32 devdrv_dma_get_cq_status(struct devdrv_dma_cq_node *cq_desc);
void devdrv_set_pf_dma_queue_pause(struct devdrv_dma_dev *dma_dev, bool pause_flag);
void devdrv_dma_set_interrupt_mask(struct devdrv_dma_channel *dma_chan);
void devdrv_dma_set_interrupt_unmask(struct devdrv_dma_channel *dma_chan);
#endif

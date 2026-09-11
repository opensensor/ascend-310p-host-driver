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
#ifndef EVENT_SCHED_UT
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"

#include "icm_interface.h"
#include "tsdrv_interface.h"
#include "devdrv_interface.h"

#include "ascend_hal_define.h"
#include "event_sched_inner.h"

#include "esched_fault_report.h"
#include "esched.h"
#include "esched_host_msg.h"
#include "esched_drv_adapt.h"

/* HOST reg offset */
#define SCHED_HOST_TOPIC_ADDR_OFFSET                                 (0x400000)

/* wait/get status and report reg */
#define STARS_TOPIC_VF_HOST_AICPU_STATUS_REPORT_NS(cpu_id, vf_id)    (0x000000 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_AICPU_WAIT_TOPIC_NS(cpu_id, vf_id)       (0x000004 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_AICPU_INT_EN_NS(cpu_id, vf_id)           (0x000008 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_AICPU_ERRCODE_REPORT_NS(cpu_id, vf_id)   (0x00000C + (cpu_id) * 0x20 + (vf_id) * 0x10000)

#define STARS_TOPIC_VF_HOST_CTRLCPU_STATUS_REPORT_NS(cpu_id, vf_id)  (0x000010 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_CTRLCPU_ERRCODE_REPORT_NS(cpu_id, vf_id) (0x00001C + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_CTRLCPU_WAIT_TOPIC_NS(cpu_id, vf_id)     (0x000014 + (cpu_id) * 0x20 + (vf_id) * 0x10000)
#define STARS_TOPIC_VF_HOST_CTRLCPU_INT_EN_NS(cpu_id, vf_id)         (0x000018 + (cpu_id) * 0x20 + (vf_id) * 0x10000)

/* host intr reg */
#define STARS_TOPIC_HCPU_INT_REG_NUM                                2

#define STARS_TOPIC_HOST_AICPU_INT_MASK_NUM                         2
#define STARS_TOPIC_HOST_AICPU_INT_STS0_NS(index, vf_id)            (0x000A00 + (vf_id) * 0x10000 + (index) * 0x20)
#define STARS_TOPIC_HOST_AICPU_INT_CLR0_NS(index, vf_id)            (0x000A08 + (vf_id) * 0x10000 + (index) * 0x20)
#define STARS_TOPIC_HOST_AICPU_INT_MASK0_NS(index, vf_id)           (0x000A0C + (vf_id) * 0x10000 + (index) * 0x20)
#define STARS_TOPIC_HOST_AICPU_INT_STS_NS(vf_id)                    (0x000A40 + (vf_id) * 0x10000)
#define STARS_TOPIC_HOST_AICPU_INT_CLR_NS(vf_id)                    (0x000A48 + (vf_id) * 0x10000)

#define STARS_TOPIC_HOST_CTRLCPU_INT_STS_NS(vf_id)                  (0x000A60 + (vf_id) * 0x10000)
#define STARS_TOPIC_HOST_CTRLCPU_INT_CLR_NS(vf_id)                  (0x000A68 + (vf_id) * 0x10000)
#define STARS_TOPIC_HOST_CTRLCPU_INT_MASK_NS(vf_id)                 (0x000A6C + (vf_id) * 0x10000)

#define STARS_INT_HTIC_HCPU_INT_CLR_NS   0x0CD8

#define SCHED_INSTANCE_INIT_WAIT_TIME     300     /* unit s */
#define SCHED_HOST_CONF_INTR_FREQ         3

#define SCHED_HOST_STARS_TOPIC_INTR       0
#define SCHED_HOST_STARS_INT_INTR         1

#define SCHED_HOST_TOPIC_IRQ_NUM          1
#define SCHED_HOST_MB_COUNT              (sizeof(u32) * KA_BITS_PER_BYTE)

#define ESCHED_DRV_REASSIGN_VFID(vf_id)  (((vf_id) == 0) ? (vf_id) : ((vf_id) - 1))

#define SOFT_FAULT_REPORT_THR         60
struct esched_drv_dev_attr {
    u32 valid;
    u32 chip_id;
    u32 vf_id;
    int irq;
};

STATIC struct esched_drv_dev_attr *esched_drv_get_host_dev_attr(u32 dev_id)
{
    struct sched_hard_res *res = NULL;
    struct esched_drv_dev_attr *attr = NULL;

    res = esched_get_hard_res(dev_id);
    if (res == NULL) {
        sched_err("Failed to get hard res. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    attr = (struct esched_drv_dev_attr *)res->priv;

    return (attr->valid == 1) ? attr : NULL;
}

STATIC int esched_drv_host_init_priv(struct sched_hard_res *res)
{
    int ret;
    struct esched_drv_dev_attr *attr = NULL;

    if (res->priv == NULL) {
        res->priv = sched_vzalloc(sizeof(struct esched_drv_dev_attr) * SCHED_MAX_CHIP_NUM);
        if (res->priv == NULL) {
            return DRV_ERROR_INNER_ERR;
        }
    }

    attr = (struct esched_drv_dev_attr *)res->priv;

    ret = devdrv_get_pfvf_id_by_devid(res->dev_id, &attr->chip_id, &attr->vf_id);
    if (ret != 0) {
        sched_err("Failed to get pfvf id. (dev_id=%u)\n", res->dev_id);
        sched_vfree(res->priv);
        res->priv = NULL;
        return DRV_ERROR_INNER_ERR;
    }

    ret = devdrv_get_irq_vector(res->dev_id, res->irq[0], &attr->irq);
    if (ret != 0) {
        sched_err("Get irq vector failed. (irq=%u; dev_id=%u; ret=%d)\n", res->irq[0], res->dev_id, ret);
        sched_vfree(res->priv);
        res->priv = NULL;
        return ret;
    }

    attr->valid = 1;

    return DRV_ERROR_NONE;
}

STATIC void esched_drv_host_uninit_priv(struct sched_hard_res *res)
{
    if (res->priv == NULL) {
        return;
    }

    sched_vfree(res->priv);
    res->priv = NULL;
}

STATIC void esched_drv_reg_wr(void __iomem *io_base, u32 offset, u32 val)
{
    ka_mm_writel(val, io_base + offset);
    sched_debug("Show details. (offset=%x; data=%x)\n", offset, val);
}

STATIC void esched_drv_reg_rd(const void __iomem *io_base, u32 offset, u32 *val)
{
    *val = ka_mm_readl(io_base + offset);

    if (*val != 0) {
        sched_debug("Show details. (offset=%x; data=%x)\n", offset, *val);
    }
}

/* vf_id: range from 0 to 15 */
STATIC void topic_sched_host_ccpu_status_report(void __iomem *io_base, u32 mb_id, u32 vf_id, u32 status)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_STATUS_REPORT_NS(mb_id, vf_id_tmp), status);
}

STATIC void topic_sched_host_ccpu_errcode_report(void __iomem *io_base, u32 mb_id, u32 vf_id, u32 error_code)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_ERRCODE_REPORT_NS(mb_id, vf_id_tmp), error_code);
}

STATIC void topic_sched_host_aicpu_intr_mask_set(void __iomem *io_base, u32 mask_index, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_AICPU_INT_MASK0_NS(mask_index, vf_id_tmp), val);
}

STATIC void topic_sched_host_ctrlcpu_intr_mask_set(void __iomem *io_base, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_CTRLCPU_INT_MASK_NS(vf_id_tmp), val);
}

STATIC bool topic_sched_host_ccpu_is_mb_valid(const void __iomem *io_base, u32 mb_id, u32 vf_id)
{
    u32 val;

    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_WAIT_TOPIC_NS(mb_id, vf_id_tmp), &val);

    return ((val & 0x1) == 1);
}

STATIC void topic_sched_host_aicpu_status_report(void __iomem *io_base, u32 mb_id, u32 vf_id, u32 status)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_AICPU_STATUS_REPORT_NS(mb_id, vf_id_tmp), status);
}

STATIC void topic_sched_host_aicpu_errcode_report(void __iomem *io_base, u32 mb_id, u32 vf_id, u32 error_code)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_AICPU_ERRCODE_REPORT_NS(mb_id, vf_id_tmp), error_code);
}

STATIC bool topic_sched_host_aicpu_is_mb_valid(const void __iomem *io_base, u32 mb_id, u32 vf_id)
{
    u32 val;

    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_VF_HOST_AICPU_WAIT_TOPIC_NS(mb_id, vf_id_tmp), &val);

    return ((val & 0x1) == 1);
}

STATIC void topic_sched_host_aicpu_intr_clr(void __iomem *io_base, u32 intr_index, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_AICPU_INT_CLR0_NS(intr_index, vf_id_tmp), val);
}

STATIC void topic_sched_host_ccpu_intr_clr(void __iomem *io_base, u32 vf_id, u32 val)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_CTRLCPU_INT_CLR_NS(vf_id_tmp), val);
}

STATIC void topic_sched_host_aicpu_intr_enable(void __iomem *io_base, u32 cpu_index, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_AICPU_INT_EN_NS(cpu_index, vf_id_tmp), 0x1);
}

STATIC void topic_sched_host_ctrlcpu_intr_enable(void __iomem *io_base, u32 cpu_index, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_VF_HOST_CTRLCPU_INT_EN_NS(cpu_index, vf_id_tmp), 0x1);
}

STATIC void topic_sched_host_aicpu_int_all_status(const void __iomem *io_base, u32 *val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_HOST_AICPU_INT_STS_NS(vf_id_tmp), val);
}

STATIC void topic_sched_host_aicpu_intr_all_clr(void __iomem *io_base, u32 val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_wr(io_base, STARS_TOPIC_HOST_AICPU_INT_CLR_NS(vf_id_tmp), val);
}

STATIC void topic_sched_host_aicpu_int_status(const void __iomem *io_base, u32 intr_index, u32 *val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_HOST_AICPU_INT_STS0_NS(intr_index, vf_id_tmp), val);
}

STATIC void topic_sched_host_ccpu_int_status(const void __iomem *io_base, u32 *val, u32 vf_id)
{
    u32 vf_id_tmp = ESCHED_DRV_REASSIGN_VFID(vf_id);
    esched_drv_reg_rd(io_base, STARS_TOPIC_HOST_CTRLCPU_INT_STS_NS(vf_id_tmp), val);
}

u32 esched_get_devid_from_hw_vfid(u32 chip_id, u32 vfid, u32 sub_dev_num)
{
    return chip_id;
}

u32 esched_get_hw_vfid_from_devid(u32 dev_id)
{
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(dev_id);

    return ESCHED_DRV_REASSIGN_VFID(dev_attr->vf_id);
}

bool esched_is_phy_dev(u32 dev_id)
{
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(dev_id);

    return (dev_attr->vf_id > 0) ? false : true;
}

int esched_drv_config_pid(struct sched_proc_ctx *proc_ctx, u32 identity, devdrv_host_pids_info_t *pids_info)
{
    if (pids_info->cp_type[0] == (unsigned int)DEVDRV_PROCESS_USER) {
        return 0;
    }

    return esched_drv_remote_add_pid(proc_ctx->node->node_id, proc_ctx->host_pid,
        pids_info->cp_type[0], proc_ctx->pid);
}

void esched_drv_del_pid(struct sched_proc_ctx *proc_ctx, u32 identity)
{
    (void)esched_drv_remote_del_pid(proc_ctx->node->node_id, proc_ctx->host_pid, proc_ctx->pid);
}

static void esched_drv_status_report(struct topic_data_chan *topic_chan, u32 vf_id, u32 status)
{
#ifdef CFG_SOC_PLATFORM_ESL
    status = 1; /* ESL not surport abnormal report sched, The ESL will suspend. */
#endif

    /* For the CDQM event, the topic requires that no exception be replied. */
    if (topic_chan->wait_mb->topic_id == EVENT_CDQ_MSG) {
        status = 1;
    }

    if (topic_chan->mb_type == ACPU_HOST) {
        topic_sched_host_aicpu_status_report(topic_chan->hard_res->io_base, topic_chan->mb_id, vf_id, status);
    } else {
        topic_sched_host_ccpu_status_report(topic_chan->hard_res->io_base, topic_chan->mb_id, vf_id, status);
    }
}

static void esched_drv_errcode_report(struct topic_data_chan *topic_chan, u32 vf_id, u32 error_code)
{
    /* For the CDQM event, the topic requires that no exception be replied. */
    if (topic_chan->wait_mb->topic_id == EVENT_CDQ_MSG) {
        error_code = 0;
    }

    if (topic_chan->mb_type == ACPU_HOST) {
        topic_sched_host_aicpu_errcode_report(topic_chan->hard_res->io_base,
            topic_chan->mb_id, vf_id, error_code);
    } else {
        topic_sched_host_ccpu_errcode_report(topic_chan->hard_res->io_base,
            topic_chan->mb_id, vf_id, error_code);
    }
}

void esched_drv_cpu_report(struct topic_data_chan *topic_chan, u32 error_code, u32 status)
{
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(topic_chan->hard_res->dev_id);

    if (dev_attr == NULL) {
#ifndef EMU_ST
        sched_err("Invalid dev attr. (dev_id=%u)\n", topic_chan->hard_res->dev_id);
        return;
#endif
    }

    esched_drv_errcode_report(topic_chan, dev_attr->vf_id, error_code);
    esched_drv_status_report(topic_chan, dev_attr->vf_id, status);
    if (status == TOPIC_FINISH_STATUS_WARNING) { // topic constraint
#ifndef EMU_ST
        esched_drv_status_report(topic_chan, dev_attr->vf_id, TOPIC_FINISH_STATUS_NORMAL);
#endif
    }
}

void esched_drv_get_status_report(struct topic_data_chan *topic_chan, u32 status)
{
}

bool esched_drv_is_mb_valid(struct topic_data_chan *topic_chan)
{
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(topic_chan->hard_res->dev_id);

    if (dev_attr == NULL) {
        sched_err("Invalid dev attr. (dev_id=%u)\n", topic_chan->hard_res->dev_id);
        return 0;
    }

    if (topic_chan->mb_type == ACPU_HOST) {
        return topic_sched_host_aicpu_is_mb_valid(topic_chan->hard_res->io_base, topic_chan->mb_id, dev_attr->vf_id);
    } else {
        return topic_sched_host_ccpu_is_mb_valid(topic_chan->hard_res->io_base, topic_chan->mb_id, dev_attr->vf_id);
    }
}

bool esched_drv_is_get_mb_valid(struct topic_data_chan *topic_chan)
{
    return false;
}

int esched_cpu_port_submit_task(struct topic_data_chan *topic_chan, void *sqe, u32 timeout)
{
    return -EFAULT;
}

STATIC void esched_drv_host_mb_intr_clr(struct topic_data_chan *topic_chan, u32 intr_index, u32 val, u32 vf_id)
{
    if (topic_chan->mb_type == ACPU_HOST) {
        topic_sched_host_aicpu_intr_clr(topic_chan->hard_res->io_base, intr_index, vf_id, val);
    }

    if (topic_chan->mb_type == CCPU_HOST) {
        topic_sched_host_ccpu_intr_clr(topic_chan->hard_res->io_base, vf_id, val);
    }
}

static void esched_drv_host_init_node_aicpu_chan(struct sched_numa_node *node)
{
    if (devdrv_get_dev_chip_type(node->node_id) == HISI_CLOUD_V2) {
        node->hard_res.aicpu_chan_num = TOPIC_SCHED_HOST_AICPU_CHAN_NUM;
    } else {
        node->hard_res.aicpu_chan_num = 0;
    }

    node->hard_res.aicpu_chan_start_id = 0;
}

STATIC struct esched_drv_dev_attr *esched_drv_interrupt_get_host_dev_attr(u32 dev_id)
{
    struct sched_hard_res *res = NULL;
    struct esched_drv_dev_attr *attr = NULL;
    struct sched_numa_node *node = sched_get_numa_node(dev_id);
    if (node == NULL) {
        return NULL;
    }
 
    res = &node->hard_res;
    attr = (struct esched_drv_dev_attr *)res->priv;
 
    return (attr->valid == 1) ? attr : NULL;
}
 
STATIC struct topic_data_chan *esched_drv_interrupt_get_topic_chan(u32 dev_id, u32 chan_id)
{
    struct sched_numa_node *node = sched_get_numa_node(dev_id);
 
    if (chan_id >= TOPIC_SCHED_MAX_CHAN_NUM) {
        return NULL;
    }
 
    return node->hard_res.topic_chan[chan_id];
}

STATIC ka_irqreturn_t esched_drv_host_task_interrupt(int irq, void *data)
{
    struct sched_hard_res *res = (struct sched_hard_res *)data;
    struct topic_data_chan *topic_chan = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 offset, mb_id, i, val = 0;
    struct esched_drv_dev_attr *dev_attr = esched_drv_interrupt_get_host_dev_attr(res->dev_id);

    if (dev_attr == NULL) {
        return KA_IRQ_HANDLED;
    }

    for (i = 0; i < STARS_TOPIC_HCPU_INT_REG_NUM; i++) {
        topic_sched_host_aicpu_int_status(res->io_base, i, &val, dev_attr->vf_id);

        if (val == 0) {
            continue;
        }

        for (offset = 0; offset < SCHED_HOST_MB_COUNT; offset++) {
            if ((val >> offset) & 0x1) {
                mb_id = SCHED_HOST_MB_COUNT * i + offset;
                topic_chan = esched_drv_interrupt_get_topic_chan(res->dev_id, mb_id);
                esched_drv_host_mb_intr_clr(topic_chan, i, val, dev_attr->vf_id);
                ka_system_tasklet_schedule(&topic_chan->sched_task);
            }
        }
    }

    /* Clear merge interrupt status */
    topic_sched_host_aicpu_int_all_status(res->io_base, &val, dev_attr->vf_id);
    topic_sched_host_aicpu_intr_all_clr(res->io_base, val, dev_attr->vf_id);

    topic_sched_host_ccpu_int_status(res->io_base, &val, dev_attr->vf_id);
    if (val == 0) {
        return KA_IRQ_HANDLED;
    }

    cpu_ctx = sched_get_cpu_ctx(sched_get_numa_node(res->dev_id), NON_SCHED_DEFAULT_CPUID);
    esched_drv_host_mb_intr_clr(cpu_ctx->topic_chan, 0, val, dev_attr->vf_id);
    ka_system_tasklet_schedule(&cpu_ctx->topic_chan->sched_task);

    return KA_IRQ_HANDLED;
}

void esched_drv_mb_intr_enable(struct topic_data_chan *topic_chan)
{
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(topic_chan->hard_res->dev_id);

    if (dev_attr == NULL) {
        sched_err("Invalid dev attr. (dev_id=%u)\n", topic_chan->hard_res->dev_id);
        return;
    }

    if (topic_chan->mb_type == ACPU_HOST) {
        topic_sched_host_aicpu_intr_enable(topic_chan->hard_res->io_base, topic_chan->mb_id, dev_attr->vf_id);
    } else {
        topic_sched_host_ctrlcpu_intr_enable(topic_chan->hard_res->io_base, topic_chan->mb_id, dev_attr->vf_id);
    }

    sched_debug("Show details. (mb_id=%u; mb_type=%u; irq=%d)\n",
        topic_chan->mb_id, topic_chan->mb_type, topic_chan->irq);
}

STATIC void esched_drv_host_init_cpu_mb(u32 chip_id, u32 mb_index, u32 wait_mb_id)
{
    struct topic_data_chan *topic_chan = esched_drv_get_topic_chan(chip_id, wait_mb_id);
    u32 offset = TOPIC_SCHED_MB_SIZE * wait_mb_id;

    topic_chan->mb_id = mb_index;

    /* host rsv_mem_va only use for mailbox */
    topic_chan->wait_mb = (struct topic_sched_mailbox *)(topic_chan->hard_res->rsv_mem_va + offset);

    sched_debug("Show details. (chip_id=%u; mb_type=%u; mb_index=%u; wait_mb_id=%u; offset=0x%x)\n",
        chip_id, topic_chan->mb_type, mb_index, wait_mb_id, offset);

    return;
}

STATIC int esched_drv_host_config_intr(struct sched_hard_res *res)
{
    int ret;

    /* hard sched just need to handle one irq in host */
    ret = esched_drv_remote_config_intr(res->dev_id, res->irq[0]);
    if (ret != 0) {
        sched_debug("Remote config irq_base not success. (dev_id=%u; irq_base=%d; ret=%d)\n",
            res->dev_id, res->irq[0], ret);
        return ret;
    }

    return 0;
}

STATIC int esched_drv_init_aicpu_chan(struct sched_numa_node *node)
{
    u32 chip_id = node->node_id;
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(chip_id);
    struct sched_hard_res *res = esched_get_hard_res(chip_id);
    struct topic_data_chan *topic_chan = NULL;
    u32 i, chan_id;

    if (dev_attr == NULL) {
        sched_err("Invalid dev attr. (chip_id=%u)\n", chip_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    esched_drv_remote_add_mb(chip_id, dev_attr->vf_id);

    for (i = 0; i < res->aicpu_chan_num; i++) {
        chan_id = res->aicpu_chan_start_id + i;
        topic_chan = esched_drv_get_topic_chan(chip_id, chan_id);
        if (topic_chan == NULL) {
            return DRV_ERROR_INNER_ERR;
        }

        topic_chan->irq = res->irq[0];
        topic_chan->mb_type = ACPU_HOST;
        topic_chan->hard_res = res;
        topic_chan->cpu_ctx = NULL;
        esched_drv_host_init_cpu_mb(chip_id, chan_id, chan_id);
        ka_system_tasklet_init(&topic_chan->sched_task, esched_aicpu_sched_task, (uintptr_t)topic_chan);
        topic_chan->valid = SCHED_VALID;
    }

    return 0;
}

STATIC void esched_drv_uninit_aicpu_chan(struct sched_numa_node *node)
{
    struct topic_data_chan *topic_chan = NULL;
    struct sched_hard_res *res = &node->hard_res;
    u32 i, chan_id;

    for (i = 0; i < res->aicpu_chan_num; i++) {
#ifndef EMU_ST
        chan_id = res->aicpu_chan_start_id + i;
        topic_chan = esched_drv_get_topic_chan(node->node_id, chan_id);
        if (topic_chan == NULL) {
            continue;
        }
        if (topic_chan->valid == SCHED_VALID) {
            ka_system_tasklet_kill(&topic_chan->sched_task);
            topic_chan->valid = SCHED_INVALID;
        }
#endif
    }
}

STATIC int esched_drv_init_aicpu_topic_chan(struct sched_numa_node *node)
{
    int ret;

    ret = esched_drv_create_topic_chans(node->node_id, 0, node->hard_res.aicpu_chan_num);
    if (ret != 0) {
        return ret;
    }

    ret = esched_drv_init_aicpu_chan(node);
    if (ret != 0) {
        esched_drv_destroy_topic_chans(node->node_id, 0, node->hard_res.aicpu_chan_num);
        return ret;
    }

    return 0;
}

STATIC void esched_drv_uninit_aicpu_topic_chan(struct sched_numa_node *node)
{
    esched_drv_uninit_aicpu_chan(node);
    esched_drv_destroy_topic_chans(node->node_id, 0, node->hard_res.aicpu_chan_num);
}

STATIC int esched_drv_init_ccpu_chan(struct sched_numa_node *node, u32 mb_id)
{
    u32 dev_id = node->node_id;
    struct topic_data_chan *topic_chan = NULL;
    struct sched_hard_res *res = esched_get_hard_res(dev_id);
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(dev_id);
    u32 chan_id = res->ccpu_chan_id;

    if (dev_attr == NULL) {
        sched_err("Invalid dev attr. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    topic_chan = esched_drv_get_topic_chan(dev_id, chan_id);
    if (topic_chan == NULL) {
        return DRV_ERROR_INNER_ERR;
    }

    topic_chan->irq = res->irq[0];   /* Not used in host side */
    topic_chan->mb_type = CCPU_HOST;
    topic_chan->hard_res = res;
    topic_chan->serial_no = 0;
    topic_chan->cpu_ctx = sched_get_cpu_ctx(node, NON_SCHED_DEFAULT_CPUID);
    topic_chan->cpu_ctx->topic_chan = topic_chan;

    esched_drv_host_init_cpu_mb(dev_id, mb_id, chan_id);
    ka_system_tasklet_init(&topic_chan->sched_task, esched_ccpu_sched_task, (uintptr_t)topic_chan);
    topic_chan->valid = SCHED_VALID;

    esched_drv_remote_add_mb(dev_id, dev_attr->vf_id);

    return 0;
}

STATIC void esched_drv_uninit_ccpu_chan(struct sched_numa_node *node)
{
    struct topic_data_chan *topic_chan = esched_drv_get_topic_chan(node->node_id, node->hard_res.ccpu_chan_id);

    if (topic_chan == NULL) {
        return;
    }

    if (topic_chan->valid == SCHED_VALID) {
        ka_system_tasklet_kill(&topic_chan->sched_task);
        topic_chan->valid = SCHED_INVALID;
    }
}

STATIC void esched_drv_uninit_ccpu_topic_chan(struct sched_numa_node *node)
{
    esched_drv_uninit_ccpu_chan(node);
    esched_drv_destroy_one_topic_chan(node->node_id, node->hard_res.ccpu_chan_id);
}

STATIC int esched_drv_init_ccpu_topic_chan(struct sched_numa_node *node)
{
    struct topic_data_chan *topic_chan = NULL;
    int ret;
    u32 mb_id, chan_id;

    ret = esched_drv_remote_get_cpu_mbid(node->node_id, CCPU_HOST, &mb_id, &chan_id);
    if (ret != 0) {
        return ret;
    }

    node->hard_res.ccpu_chan_id = chan_id;

    topic_chan = esched_drv_create_one_topic_chan(node->node_id, chan_id);
    if (topic_chan == NULL) {
        return DRV_ERROR_INNER_ERR;
    }

    ret = esched_drv_init_ccpu_chan(node, mb_id);
    if (ret != 0) {
        esched_drv_destroy_one_topic_chan(node->node_id, chan_id);
        return ret;
    }

    return 0;
}

STATIC int esched_drv_host_init_irq(struct sched_hard_res *res)
{
    u32 dev_id = res->dev_id;
    int ret;
    struct esched_drv_dev_attr *attr = (struct esched_drv_dev_attr *)res->priv;

    if (attr == NULL) {
        sched_err("Invalid attr. (dev_id=%u\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = devdrv_register_irq_by_vector_index(dev_id, (int)res->irq[0], esched_drv_host_task_interrupt,
        (void *)res, "topic_sched_aicpu");
    if (ret != 0) {
        sched_err("Request irq failed. (dev_id=%u; irq=%u; ret=%d)\n", dev_id, attr->irq, ret);
        return ret;
    }

    /* all cpu in host can handle the same irq */
    (void)ka_base_irq_set_affinity_hint(attr->irq, NULL);
    res->irq_reg_flag = 1;

    sched_debug("Request irq success. (dev_id=%u; irq=%u)\n", dev_id, attr->irq);

    return 0;
}

STATIC void esched_drv_host_uninit_irq(struct sched_hard_res *res)
{
    struct esched_drv_dev_attr *attr = (struct esched_drv_dev_attr *)res->priv;

    if (attr == NULL) {
        sched_err("Invalid attr. (dev_id=%u\n", res->dev_id);
        return;
    }

    if (res->irq_reg_flag == 1) {
        (void)ka_base_irq_set_affinity_hint((u32)attr->irq, NULL);
        (void)devdrv_unregister_irq_by_vector_index(res->dev_id, res->irq[0], (void *)res);
        res->irq_reg_flag = 0;
    }
}

STATIC int esched_drv_host_init_aicpu_pool(struct sched_numa_node *node)
{
    int ret;

    ret = esched_drv_remote_add_pool(node->node_id, ACPU_HOST);
    if (ret != 0) {
        sched_err("Add aicpu pool failed. (devid=%u)\n", node->node_id);
        return ret;
    }

    sched_debug("Add aicpu pool success. (devid=%u)\n", node->node_id);

    return 0;
}

STATIC int esched_drv_init_ccpu_pool(struct sched_numa_node *node)
{
    int ret;

    ret = esched_drv_remote_add_pool(node->node_id, CCPU_HOST);
    if (ret != 0) {
        sched_err("Add ccpu pool failed. (devid=%u)\n", node->node_id);
        return ret;
    }

    sched_debug("Add ccpu pool success. (devid=%u)\n", node->node_id);
    return 0;
}

STATIC int esched_drv_host_ccpu_chan_init(struct sched_numa_node *node)
{
    int ret;

    ret = esched_drv_init_ccpu_pool(node);
    if (ret != 0) {
        return ret;
    }

    ret = esched_drv_init_ccpu_topic_chan(node);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

STATIC void esched_drv_host_ccpu_chan_uninit(struct sched_numa_node *node)
{
    esched_drv_uninit_ccpu_topic_chan(node);
}

STATIC int esched_drv_host_aicpu_chan_init(struct sched_numa_node *node)
{
    int ret;

    esched_drv_host_init_node_aicpu_chan(node);

    ret = esched_drv_host_init_aicpu_pool(node);
    if (ret != 0) {
        return ret;
    }

    ret = esched_drv_init_aicpu_topic_chan(node);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

STATIC void esched_drv_host_aicpu_chan_uninit(struct sched_numa_node *node)
{
    esched_drv_uninit_aicpu_topic_chan(node);
}

STATIC int esched_drv_host_map_addr(u32 dev_id, struct sched_hard_res *res)
{
    int ret;
    u64 reg_base;
    size_t size;
    u64 rsv_mem_pa;

    /* topic sched base */
    ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_STARS_TOPIC_SCHED_BASE, 0, &reg_base, &size);
    if (ret != 0) {
        sched_err("Failed to invoke the devdrv_get_addr_info. "
            "(dev_id=%u; type=%d)\n", dev_id, DEVDRV_ADDR_STARS_TOPIC_SCHED_BASE);
        return ret;
    }

    /* rsv_mem_pa are different between PF and VF */
    ret = devdrv_get_pfvf_type_by_devid(dev_id);
    if (ret == DEVDRV_SRIOV_TYPE_PF) {
        reg_base += SCHED_HOST_TOPIC_ADDR_OFFSET;
    }

    res->io_base = ka_mm_ioremap(reg_base, size);
    if (res->io_base == NULL) {
        sched_err("Failed to invoke the ioremap. (dev_id=%u; size=0x%x)\n", res->dev_id, (u32)size);
        return -ENOMEM;
    }

    res->int_io_base = NULL;

    ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_STARS_TOPIC_SCHED_RES_MEM_BASE, 0, &rsv_mem_pa, &size);
    if (ret != 0) {
        sched_err("Failed to invoke the devdrv_get_addr_info. (dev_id=%u; type=%d; size=0x%x; ret=%d)\n",
            dev_id, DEVDRV_ADDR_STARS_TOPIC_SCHED_RES_MEM_BASE, (u32)size, ret);
        goto iounmap_io_base;
    }

    res->rsv_mem_pa = rsv_mem_pa;
    res->rsv_mem_va = ka_mm_ioremap(res->rsv_mem_pa, size);
    if (res->rsv_mem_va == NULL) {
        sched_err("Failed to invoke the ioremap. (dev_id=%u; size=0x%x)\n", res->dev_id, (u32)size);
        ret = -ENOMEM;
        goto iounmap_io_base;
    }

    sched_debug("Show details. (dev_id=%u; io_base=%pK; int_io_base=%pK; rsv_mem_pa=0x%pK; size=0x%x)\n",
        dev_id, res->io_base, res->int_io_base, (void *)res->rsv_mem_pa, (u32)size);

    return 0;

iounmap_io_base:
    ka_mm_iounmap(res->io_base);
    res->io_base = NULL;
    return ret;
}

STATIC void esched_drv_init_host_cpu_intr_mask(struct sched_hard_res *res)
{
    int i;
    struct esched_drv_dev_attr *dev_attr = esched_drv_get_host_dev_attr(res->dev_id);

    if (dev_attr == NULL) {
        sched_err("Invalid dev attr. (dev_id=%u)\n", res->dev_id);
        return;
    }

    /* init host aicpu intr mask */
    for (i = 0; i < STARS_TOPIC_HOST_AICPU_INT_MASK_NUM; i++) {
        topic_sched_host_aicpu_intr_mask_set(res->io_base, i, dev_attr->vf_id, 0);
    }

    /* init host ctrlcpu intr mask */
    topic_sched_host_ctrlcpu_intr_mask_set(res->io_base, dev_attr->vf_id, 0);
}

STATIC void esched_drv_host_mb_intr_all_reset(struct sched_hard_res *res)
{
    struct topic_data_chan *topic_chan = NULL;
    struct sched_cpu_ctx *cpu_ctx = NULL;
    u32 offset, mb_id, i, val = 0;
    u32 vf_id = ((struct esched_drv_dev_attr *)res->priv)->vf_id;

    for (i = 0; i < STARS_TOPIC_HCPU_INT_REG_NUM; i++) {
        topic_sched_host_aicpu_int_status(res->io_base, i, &val, vf_id);
        for (offset = 0; offset < SCHED_HOST_MB_COUNT; offset++) {
            mb_id = SCHED_HOST_MB_COUNT * i + offset;
            topic_chan = esched_drv_get_topic_chan(res->dev_id, mb_id);
            if ((topic_chan == NULL) || (topic_chan->valid != SCHED_VALID)) {
                continue;
            }
            if ((val >> offset) & 0x1) {
                esched_drv_host_mb_intr_clr(topic_chan, i, val, vf_id);
            }
            esched_drv_mb_intr_enable(topic_chan);
        }
    }

    topic_sched_host_aicpu_int_all_status(res->io_base, &val, vf_id);
    topic_sched_host_aicpu_intr_all_clr(res->io_base, val, vf_id);

    cpu_ctx = sched_get_cpu_ctx(sched_get_numa_node(res->dev_id), NON_SCHED_DEFAULT_CPUID);
    topic_sched_host_ccpu_int_status(res->io_base, &val, vf_id);
    if (val != 0) {
        esched_drv_host_mb_intr_clr(cpu_ctx->topic_chan, 0, val, vf_id);
    }
    esched_drv_mb_intr_enable(cpu_ctx->topic_chan);
}

STATIC int esched_drv_init_msg_chan_main_proc(struct sched_hard_res *res)
{
    struct sched_numa_node *node = sched_get_numa_node(res->dev_id);
    int ret;

    ret = esched_drv_init_non_sched_task_submit_chan(res->dev_id, TOPIC_SCHED_HOST_POOL_ID);
    if (ret != 0) {
        return DRV_ERROR_INNER_ERR;
    }

    ret = esched_drv_init_sched_task_submit_chan(res->dev_id, TOPIC_SCHED_HOST_POOL_ID, 1, 1);
    if (ret != 0) {
        sched_err("Sched task chan init failed. (dev_id=%u)\n", res->dev_id);
        esched_drv_uninit_non_sched_task_submit_chan(res->dev_id);
        return 0;
    }

    ret = esched_drv_host_ccpu_chan_init(node);
    if (ret != 0) {
        sched_err("Host ccpu init failed. (dev_id=%u)\n", res->dev_id);
        esched_drv_uninit_sched_task_submit_chan(res->dev_id);
        esched_drv_uninit_non_sched_task_submit_chan(res->dev_id);
        return 0;
    }

    ret = esched_drv_host_aicpu_chan_init(node);
    if (ret != 0) {
        sched_err("Host aicpu init failed. (dev_id=%u)\n", res->dev_id);
        esched_drv_host_ccpu_chan_uninit(node);
        esched_drv_uninit_sched_task_submit_chan(res->dev_id);
        esched_drv_uninit_non_sched_task_submit_chan(res->dev_id);
        return 0;
    }

    ret = esched_drv_host_init_irq(res);
    if (ret != 0) {
        sched_err("Host irq init failed. (dev_id=%u)\n", res->dev_id);
        esched_drv_host_aicpu_chan_uninit(node);
        esched_drv_host_ccpu_chan_uninit(node);
        esched_drv_uninit_sched_task_submit_chan(res->dev_id);
        esched_drv_uninit_non_sched_task_submit_chan(res->dev_id);
        return 0;
    }

    esched_drv_host_mb_intr_all_reset(res);

    res->init_flag = SCHED_VALID;
    sched_debug("Hw dev init success. (dev_id=%u)\n", res->dev_id);
    return 0;
}

STATIC void esched_drv_init_msg_chan_work(ka_work_struct_t *p_work)
{
    struct sched_hard_res *res = ka_container_of(p_work, struct sched_hard_res, init.work);
    int ret;

    if (res->intr_config_flag == SCHED_INVALID) {
        ret = esched_drv_host_config_intr(res);
        if (ret != 0) {
            res->retry_times++;
            sched_debug("Retry. (dev_id=%u; retry_times=%u; ret=%d)\n", res->dev_id, res->retry_times, ret);
            (void)ka_task_schedule_delayed_work(&res->init, SCHED_HOST_CONF_INTR_FREQ * KA_HZ);
            return;
        }
        res->intr_config_flag = SCHED_VALID;
    }

    ret = esched_drv_init_msg_chan_main_proc(res);
    if (ret != 0) {
        res->retry_times++;
        if ((res->retry_times > SOFT_FAULT_REPORT_THR) && (res->report_fault_flag == 0)) {
            res->report_fault_flag = 1;
            esched_kernel_soft_fault_report(res->dev_id);
        }
        sched_debug("Retry. (dev_id=%u; retry_times=%u; ret=%d)\n", res->dev_id, res->retry_times, ret);
        (void)ka_task_schedule_delayed_work(&res->init, 1 * KA_HZ);
        return;
    }

    return;
}

STATIC int esched_drv_init_msg_chan(struct sched_hard_res *res)
{
    int ret;

    if (res->intr_config_flag == SCHED_INVALID) {
        ret = esched_drv_host_config_intr(res);
        if (ret != 0) {
            res->retry_times++;
            return DRV_ERROR_INNER_ERR;
        }
        res->intr_config_flag = SCHED_VALID;
    }

    ret = esched_drv_init_msg_chan_main_proc(res);
    if (ret != 0) {
        res->retry_times++;
        sched_debug("Retry. (dev_id=%u; retry_times=%u; ret=%d)\n", res->dev_id, res->retry_times, ret);
        return DRV_ERROR_INNER_ERR;
    }
    res->delay_work_enable = 0;

    return 0;
}

STATIC void esched_host_iounmap(struct sched_hard_res *res)
{
    if (res->io_base != NULL) {
        ka_mm_iounmap(res->io_base);
        res->io_base = NULL;
    }

    if (res->int_io_base != NULL) {
        ka_mm_iounmap(res->int_io_base);
        res->int_io_base = NULL;
    }

    if (res->rsv_mem_va != NULL) {
        ka_mm_iounmap(res->rsv_mem_va);
        res->rsv_mem_va = NULL;
    }
}

void esched_drv_reset_phy_dev(u32 devid)
{
    esched_drv_uninit_sched_task_submit_chan(devid);
    esched_drv_uninit_non_sched_task_submit_chan(devid);
}

void esched_drv_restore_phy_dev(u32 devid)
{
    (void)esched_drv_host_config_intr(esched_get_hard_res(devid));
    (void)esched_drv_init_non_sched_task_submit_chan(devid, 0);
    (void)esched_drv_init_sched_task_submit_chan(devid, TOPIC_SCHED_HOST_POOL_ID, 1, 1);
    (void)esched_drv_host_init_priv(esched_get_hard_res(devid));
    esched_drv_host_mb_intr_all_reset(esched_get_hard_res(devid));
}

int esched_hw_dev_init(struct sched_numa_node *node)
{
    struct sched_hard_res *res = &node->hard_res;
    u32 chip_id = node->node_id;
    u32 index = 0;
    int ret;

    esched_init_topic_types();

    res->dev_id = chip_id;

    res->irq_num = SCHED_HOST_TOPIC_IRQ_NUM;
    res->irq = (int *)sched_vzalloc(sizeof(int) * res->irq_num);
    if (res->irq == NULL) {
#ifndef EMU_ST
        sched_err("Failed to vzalloc memory for res irq array. (size=0x%lx)\n", sizeof(int) * res->irq_num);
        return DRV_ERROR_OUT_OF_MEMORY;
#endif
    }

    ret = devdrv_get_topic_sched_irq_vector_id(chip_id, index, &res->irq[0]);
    if (ret != 0) {
#ifndef EMU_ST
        sched_vfree(res->irq);
        res->irq = NULL;
#endif
        sched_err("Get irq failed. (index=%u; chip_id=%u; ret=%d)\n", index, chip_id, ret);
        return ret;
    }

    ret = esched_drv_host_init_priv(res);
    if (ret != 0) {
#ifndef EMU_ST
        sched_vfree(res->irq);
        res->irq = NULL;
#endif
        sched_err("Host init priv failed. (chip_id=%u; ret=%d)\n", chip_id, ret);
        return ret;
    }

    ret = esched_drv_host_map_addr(chip_id, res);
    if (ret != 0) {
#ifndef EMU_ST
        esched_drv_host_uninit_priv(res);
        sched_vfree(res->irq);
        res->irq = NULL;
#endif
        sched_err("Failed to map addr. (chip_id=%u; ret=%d)\n", chip_id, ret);
        return ret;
    }

    esched_drv_init_host_cpu_intr_mask(res);
    ret = esched_drv_init_msg_chan(res);
    if (ret != 0) {
        sched_warn("device not ready, need start delay work to retry. (chip_id=%u)\n", chip_id);
        KA_TASK_INIT_DELAYED_WORK(&res->init,  esched_drv_init_msg_chan_work);
        (void)ka_task_schedule_delayed_work(&res->init, 0);
        res->delay_work_enable = 1;
    }

    sched_debug("Show details. (chip_id=%u)\n", chip_id);

    return 0;
}

void esched_hw_dev_uninit(struct sched_numa_node *node)
{
    struct sched_hard_res *res = &node->hard_res;
    u32 chip_id = node->node_id;

    res->init_flag = SCHED_INVALID;
    if (res->delay_work_enable != 0) {
        (void)ka_task_cancel_delayed_work_sync(&res->init);
    }
    res->delay_work_enable = 0;

    esched_drv_uninit_non_sched_task_submit_chan(chip_id);
    esched_drv_uninit_sched_task_submit_chan(chip_id);
    esched_drv_host_uninit_irq(res);
    esched_drv_host_ccpu_chan_uninit(node);
    esched_drv_host_aicpu_chan_uninit(node);
    esched_host_iounmap(res);

    sched_info("Show details. (chip_id=%u)\n", chip_id);

    esched_drv_host_uninit_priv(res);
    sched_vfree(res->irq);
    res->irq = NULL;
}
#else
void esched_drv_adapt_ut(void)
{
}
#endif

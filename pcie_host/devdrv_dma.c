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
#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif

#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <asm/ptrace.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/version.h>

#include "devdrv_dma.h"
#include "devdrv_util.h"
#include "devdrv_pci.h"
#include "devdrv_ctrl.h"
#include "vpc_interface.h"
#include "devdrv_vpc.h"
#include "devdrv_smmu.h"
#include "devdrv_mem_alloc.h"
#ifdef CFG_FEATURE_DUMP_PCIE_DFX
#include "devdrv_pcie_dump_dfx.h"
#endif

#ifdef CFG_HOST
#include "securec.h"
#else
#include <linux/securec.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static inline void *dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
    void *ret = devdrv_ka_dma_alloc_coherent(dev, size, dma_handle, flag | __GFP_ZERO);
    return ret;
}
#endif

/* dma channel sq submit interface */
STATIC void devdrv_dma_ch_sq_submit(struct devdrv_dma_channel *dma_chan)
{
    devdrv_set_dma_sq_tail(dma_chan->io_base, dma_chan->sq_tail);
}

STATIC u32 devdrv_get_dma_sqcq_side(const struct devdrv_dma_channel *dma_chan)
{
    return (dma_chan->flag >> DEVDRV_DMA_SQCQ_SIDE_BIT) & 1U;
}

void devdrv_set_dma_status(struct devdrv_dma_dev *dma_dev, u32 status)
{
    if (dma_dev != NULL) {
        dma_dev->dev_status = status;
    }
}

void devdrv_set_dma_chan_status(struct devdrv_dma_channel *dma_chan, u32 status)
{
    if (dma_chan != NULL) {
        spin_lock_bh(&dma_chan->lock);
        dma_chan->chan_status = status;
        spin_unlock_bh(&dma_chan->lock);
    }
}

STATIC void devdrv_dma_cb_record_resq_time(struct devdrv_dma_channel *dma_chan)
{
    u32 resq_time;

    if (jiffies < dma_chan->status.callback_time_stamp) {
        resq_time = 0;
    } else {
        resq_time = jiffies_to_msecs(jiffies - dma_chan->status.callback_time_stamp);
    }
    dma_chan->status.new_callback_time = resq_time;

    if (resq_time > DEVDRV_CB_TIME_OVER_10MS) {
        dma_chan->status.callback_time_over10ms++;
    }
    if (resq_time > DEVDRV_CB_TIME_OVER_10S) {
        dma_chan->status.callback_time_over10s++;
    }
    if (resq_time > dma_chan->status.max_callback_time) {
        dma_chan->status.max_callback_time = resq_time;
    }
}

STATIC void devdrv_dma_done_proc(struct devdrv_dma_soft_bd *soft_bd, struct devdrv_dma_channel *dma_chan)
{
    struct devdrv_dma_soft_bd_wait_status *wait_status = NULL;

    if (soft_bd->copy_type == DEVDRV_DMA_SYNC) {
        wait_status = (struct devdrv_dma_soft_bd_wait_status *)soft_bd->priv;
        if (wait_status != NULL) {
            wait_status->status = soft_bd->status;
            wmb();
            wait_status->valid = DEVDRV_DISABLE;
            soft_bd->priv = NULL;
        }
        wmb();
        /* synchronous mode release semaphore wake-up waiting task */
        if (soft_bd->wait_type == DEVDRV_DMA_WAIT_INTR) {
            up(&soft_bd->sync_sem);
            dma_chan->status.sync_sem_up_cnt++;
        }
        atomic_set(&soft_bd->process_flag, DEVDRV_DMA_PROCESS_INIT);
    } else {
        /* asynchronous mode callback completion function */
        if (soft_bd->callback_func != NULL) {
            dma_chan->status.callback_time_stamp = jiffies;
            wmb(); /* ensure get jiffies before callback */

            soft_bd->callback_func(soft_bd->priv, soft_bd->trans_id, (u32)soft_bd->status);

            devdrv_dma_cb_record_resq_time(dma_chan);
            dma_chan->status.async_proc_cnt++;
        }
    }
}

/* not all sq bds will respond to cq, and multiple sq may be merged. Need to consider the merged sq */
STATIC void devdrv_dma_done_task_proc(struct devdrv_dma_channel *dma_chan, u32 cq_sqhd, u32 status)
{
    struct devdrv_dma_soft_bd *owner_soft_bd = NULL;
    struct devdrv_dma_soft_bd *soft_bd = NULL;
    u32 process_flag;
    u32 sq_index;
    u32 cur_status;
    u32 sq_cnt = (cq_sqhd + dma_chan->sq_depth - dma_chan->sq_head + 1) % dma_chan->sq_depth;

    for (sq_index = 0; sq_index < sq_cnt; sq_index++) {
        /* merged cq status is ok */
        cur_status = (dma_chan->sq_head == cq_sqhd) ? status : 0;
        soft_bd = dma_chan->dma_soft_bd + dma_chan->sq_head;

        /*
         * do not pay attention to soft bd, like the second bd sent by small packet;
         * do not pay attention to timeout bd
         */
        process_flag = (u32)atomic_cmpxchg(&soft_bd->process_flag, DEVDRV_DMA_PROCESS_INIT,
            DEVDRV_DMA_PROCESS_HANDLING);
        if ((soft_bd->valid == DEVDRV_DISABLE) || (process_flag == DEVDRV_DMA_PROCESS_WAIT_TIMEOUT)) {
            dma_chan->sq_head = (dma_chan->sq_head + 1) % dma_chan->sq_depth;
            continue;
        }
        rmb();
        /* the front bd in chain copy */
        if (soft_bd->owner_bd >= 0) {
            /* status error needs to be set to the last bd */
            if (cur_status != 0) {
                owner_soft_bd = dma_chan->dma_soft_bd + soft_bd->owner_bd;
                owner_soft_bd->status = (int)cur_status;
            }
        } else {
            /* if there is no error in front of bd, assign the status of the last sq. */
            if (soft_bd->status == -1) {
                soft_bd->status = (int)cur_status;
            }

            wmb();
            devdrv_dma_done_proc(soft_bd, dma_chan);
        }

        soft_bd->valid = DEVDRV_DISABLE;
        dma_chan->sq_head = (dma_chan->sq_head + 1) % dma_chan->sq_depth;
    }
}

STATIC int devdrv_dma_done_lock(struct devdrv_dma_channel *dma_chan, int is_mdev_vm)
{
    if (is_mdev_vm == 1) {
        mutex_lock(&dma_chan->vm_cq_lock);
    } else {
        if (spin_trylock_bh(&dma_chan->cq_lock) == 0) {
            return -EINVAL;
        }
    }
    return 0;
}

STATIC void devdrv_dma_done_unlock(struct devdrv_dma_channel *dma_chan, int is_mdev_vm)
{
    if (is_mdev_vm == 1) {
        mutex_unlock(&dma_chan->vm_cq_lock);
    } else {
        spin_unlock_bh(&dma_chan->cq_lock);
    }
}

STATIC void devdrv_mdev_dma_done_cqsq_update(struct devdrv_dma_channel *dma_chan)
{
    struct devdrv_vpc_msg vpc_msg = {0};
    int ret;

    vpc_msg.cmd_data.update_cmd.dev_id = dma_chan->dma_dev->dev_id;
    vpc_msg.cmd_data.update_cmd.chan_id = dma_chan->chan_id;
    vpc_msg.cmd_data.update_cmd.cq_head = dma_chan->cq_head;
    vpc_msg.cmd_data.update_cmd.sq_head = dma_chan->sq_head;
    ret = devdrv_vpc_msg_send(dma_chan->dma_dev->dev_id, DEVDRV_VPC_MSG_TYPE_SQCQ_HEAD_UPDATE, &vpc_msg,
        (u32)sizeof(struct devdrv_vpc_msg), DEVDRV_VPC_MSG_DEFAULT_TIMEOUT);
    if ((ret != 0) || (vpc_msg.error_code != 0)) {
        devdrv_err("Vpc send fail. (dev_id=%u; chan_id=%u; error_code=%d; ret=%d)\n",
            dma_chan->dma_dev->dev_id, dma_chan->chan_id, vpc_msg.error_code, ret);
    }
}

STATIC void devdrv_dma_done_sched_work(struct devdrv_dma_channel *dma_chan)
{
    if (devdrv_is_mdev_vm_boot_mode(dma_chan->dma_dev->dev_id) == true) {
        queue_work(dma_chan->dma_done_workqueue, &dma_chan->dma_done_work);
    } else {
        tasklet_schedule(&dma_chan->dma_done_task);
    }
}

STATIC void devdrv_dma_done_handle(struct devdrv_dma_channel *dma_chan)
{
    int is_mdev_vm = (devdrv_is_mdev_vm_boot_mode(dma_chan->dma_dev->dev_id) == true) ? 1 : 0;
    struct devdrv_pci_ctrl *pci_ctrl = dma_chan->dma_dev->pci_ctrl;
    struct devdrv_dma_cq_node *p_cur_last_cq = NULL;
    u32 cq_sqhd[DMA_DONE_BUDGET] = {0};
    u32 status[DMA_DONE_BUDGET] = {0};
    int cnt = 0, i;
    u32 head;
    u64 ivl;

    if (devdrv_dma_done_lock(dma_chan, is_mdev_vm) != 0) {
        return;
    }

    if ((dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED) || (pci_ctrl->device_status == DEVDRV_DEVICE_UDA_RM)) {
        devdrv_dma_done_unlock(dma_chan, is_mdev_vm);
        return;
    }

    dma_chan->status.done_tasklet_in_cnt++;
    dma_chan->status.done_tasklet_in_time = jiffies;

    while (1) {
        head = (dma_chan->cq_head + 1) % (dma_chan->cq_depth);
        p_cur_last_cq = dma_chan->cq_desc_base + head;

        if (pci_ctrl->ops.flush_cache != NULL) {
            pci_ctrl->ops.flush_cache((u64)p_cur_last_cq, sizeof(struct devdrv_dma_cq_node), CACHE_INVALID);
        }

        /* invalid cq, break */
        if (!dma_chan->dma_dev->ops.devdrv_dma_get_cq_valid(p_cur_last_cq, dma_chan->rounds)) {
            break;
        }
        rmb();
        /* Reach the threshold and schedule out */
        if (cnt >= DMA_DONE_BUDGET) {
            devdrv_dma_done_sched_work(dma_chan);
            dma_chan->status.re_schedule_cnt++;
            break;
        }

        cq_sqhd[cnt] = devdrv_dma_get_cq_sqhd(p_cur_last_cq);
        status[cnt] = devdrv_dma_get_cq_status(p_cur_last_cq);
        devdrv_dma_done_task_proc(dma_chan, cq_sqhd[cnt], status[cnt]);

        dma_chan->dma_dev->ops.devdrv_dma_set_cq_invalid(p_cur_last_cq);
        dma_chan->cq_head = head;

        if (dma_chan->cq_head == (dma_chan->cq_depth - 1)) {
            dma_chan->rounds++;
        }

        cnt++;
    }

    dma_chan->status.done_tasklet_out_time = jiffies;
    ivl = jiffies_to_msecs(dma_chan->status.done_tasklet_out_time - dma_chan->status.done_tasklet_in_time);
    if (ivl > dma_chan->status.max_task_op_time) {
        dma_chan->status.max_task_op_time = ivl;
    }

    mb();
    if (cnt > 0) {
        if (is_mdev_vm == 1) {
            devdrv_mdev_dma_done_cqsq_update(dma_chan);
        } else {
            devdrv_set_dma_cq_head(dma_chan->io_base, dma_chan->cq_head);
        }
    }

    devdrv_dma_done_unlock(dma_chan, is_mdev_vm);

    for (i = 0; i < cnt; ++i) {
        if (status[i] != 0) {
            devdrv_warn("Dma copy info. (local_id=%u; sq=%d; cq_status=0x%x)\n",
                       dma_chan->chan_id, cq_sqhd[i], status[i]);
        }
    }
    return;
}

void devdrv_dma_done_task(unsigned long data)
{
    struct devdrv_dma_channel *dma_chan = (struct devdrv_dma_channel *)((uintptr_t)data);
    if (((dma_chan->dma_dev->pci_ctrl->device_status == DEVDRV_DEVICE_DEAD) ||
        (dma_chan->dma_dev->pci_ctrl->device_status == DEVDRV_DEVICE_UDA_RM)) &&
        (devdrv_get_product() == HOST_PRODUCT_MDC)) {
        devdrv_info("DMA channel has entered into dead status\n");
    } else {
        devdrv_dma_done_handle(dma_chan);
    }
}

STATIC void devdrv_dma_done_work(struct work_struct *p_work)
{
    struct devdrv_dma_channel *dma_chan = container_of(p_work, struct devdrv_dma_channel, dma_done_work);

    devdrv_dma_done_handle(dma_chan);
}

STATIC irqreturn_t devdrv_dma_done_interrupt(int irq, void *data)
{
    struct devdrv_dma_channel *dma_chan = (struct devdrv_dma_channel *)data;

    if ((dma_chan->dma_dev->dev_status != DEVDRV_DMA_ALIVE) || (dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED)) {
        devdrv_err_spinlock("Dma channel disable. (chan_id=%d)\n", dma_chan->chan_id);
        return IRQ_HANDLED;
    }

    rmb();

    dma_chan->status.done_int_cnt++;
    dma_chan->status.done_int_in_time = jiffies;

    devdrv_dma_done_sched_work(dma_chan);

    return IRQ_HANDLED;
}

STATIC void devdrv_dma_done_guard_work_sched(struct devdrv_dma_dev *dma_dev)
{
    struct devdrv_dma_channel *dma_chan = NULL;
    u32 cq_head;
    u32 cq_tail;
    u32 i;

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        dma_chan = &dma_dev->dma_chan[i];
        if (dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED) {
            continue;
        }
        if ((devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_PHY_BOOT) ||
            (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_SRIOV_VF_BOOT)) {
            cq_head = devdrv_get_dma_cq_head(dma_chan->io_base);
            cq_tail = devdrv_get_dma_cq_tail(dma_chan->io_base);
            if ((((cq_head + 1) % dma_chan->cq_depth) != cq_tail) &&
                (cq_head != cq_tail)) {
                devdrv_dma_done_sched_work(dma_chan);
            }
        } else if ((devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_MDEV_VF_VM_BOOT) ||
            (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
            if (dma_chan->sq_head != dma_chan->sq_tail) {
                devdrv_dma_done_sched_work(dma_chan);
            }
        }
    }
}

STATIC void devdrv_show_soft_sqe(struct devdrv_dma_channel *dma_chan)
{
    u32 *sq_desc = NULL;
    u32 sq_index = devdrv_get_sq_err_ptr(dma_chan->io_base);
    int i, num;

    devdrv_warn("Squeue index. (sq_index=%d)\n", sq_index);
    if (sq_index < dma_chan->sq_depth) {
        sq_desc = (u32 *)(dma_chan->sq_desc_base + sq_index);
        num = (int)(sizeof(struct devdrv_dma_sq_node) / sizeof(u32));

        for (i = 0; i < num; i++) {
            devdrv_warn("Get current descriptor reg_val. (num=%dst; reg_val=0x%x)\n", i, sq_desc[i]);
        }
    }
}

STATIC int devdrv_dma_err_suppress(struct devdrv_dma_suppression *supp, u32 period, u32 max_log_cnt)
{
    u64 cur = jiffies;

    if (devdrv_get_product() != HOST_PRODUCT_MDC) {
        return DEVDRV_DMA_NO_NEED_SUPPRESSION;
    }

    if (jiffies_to_msecs(cur - supp->start_time) > period) { /* out of time window, reset */
        supp->start_time = cur;
        supp->log_cnt = 1;
        if (supp->suppress_cnt != 0) {
            devdrv_warn_limit("dma err suppress recover. (suppress_cnt=%u)\n", supp->suppress_cnt);
            supp->suppress_cnt = 0;
        }
        return DEVDRV_DMA_NO_NEED_SUPPRESSION;
    }

    if (supp->log_cnt < max_log_cnt) {
        supp->log_cnt++;
        return DEVDRV_DMA_NO_NEED_SUPPRESSION;
    }

    supp->suppress_cnt++;
    return DEVDRV_DMA_NEED_SUPPRESSION;
}

void devdrv_dma_err_proc(struct devdrv_dma_channel *dma_chan)
{
    devdrv_dma_done_task((unsigned long)(uintptr_t)dma_chan);
    if (devdrv_dma_err_suppress(&dma_chan->dma_dev->suppression,
                                DEVDRV_DMA_ERR_SUPPRESSION_PERIOD_MS,
                                DEVDRV_DMA_ERR_SUPPRESSION_MAX_CNT) == DEVDRV_DMA_NO_NEED_SUPPRESSION) {
        (void)devdrv_dma_chan_err_proc(dma_chan);
    }

    devdrv_show_soft_sqe(dma_chan);
    devdrv_warn("Get dma err chan id. (chan_id=%d)\n", dma_chan->chan_id);
    devdrv_warn("Get sq vir base addr. (sq_desc_base=0x%pK)\n", dma_chan->sq_desc_base);
    devdrv_warn("Get cq vir base addr. (cq_desc_base 0x%pK)\n", dma_chan->cq_desc_base);
    devdrv_warn("Get software sq head. (sq_head=%d)\n", dma_chan->sq_head);
    devdrv_warn("Get software sq tail. (sq_tail=%d)\n", dma_chan->sq_tail);
    devdrv_warn("Get software cq head. (cq_head=%d)\n", dma_chan->cq_head);
}

void devdrv_dma_err_task(struct work_struct *p_work)
{
    struct devdrv_dma_channel *dma_chan = NULL;
    bool dfx_dump_flag;
    dma_chan = container_of(p_work, struct devdrv_dma_channel, err_work);
    if (((dma_chan->dma_dev->pci_ctrl->device_status == DEVDRV_DEVICE_DEAD) ||
        (dma_chan->dma_dev->pci_ctrl->device_status == DEVDRV_DEVICE_UDA_RM)) &&
        (devdrv_get_product() == HOST_PRODUCT_MDC)) {
        devdrv_debug("DMA channel has entered into dead status\n");
    } else {
        dma_chan->status.err_work_cnt++;
        devdrv_dma_err_proc(dma_chan);
        dfx_dump_flag = false;
#ifdef CFG_FEATURE_DUMP_PCIE_DFX
        soc_misc_update_dma_err_dfx_info(dma_chan, &dfx_dump_flag);
        if (dfx_dump_flag) {
            devdrv_pcie_dump_msix_info();
            soc_misc_pcie_dump_dfx_info();
        }
#endif
    }
}

STATIC irqreturn_t devdrv_dma_err_interrupt(int irq, void *data)
{
    struct devdrv_dma_channel *dma_chan = (struct devdrv_dma_channel *)data;

    if ((dma_chan->dma_dev->dev_status != DEVDRV_DMA_ALIVE) || (dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED)) {
        devdrv_err_spinlock("Dma channel disable. (chan_id=%d)\n", dma_chan->chan_id);
        return IRQ_HANDLED;
    }

    rmb();

    dma_chan->status.err_int_cnt++;

    /* start work queue */
    schedule_work(&dma_chan->err_work);

    return IRQ_HANDLED;
}

STATIC void devdrv_dma_parse_sq_interrupt_info(struct devdrv_dma_channel *dma_chan,
    struct devdrv_asyn_dma_para_info *para_info, u32 *ldie, u32 *rdie, u32 *msi)
{
    if (devdrv_get_dma_sqcq_side(dma_chan) == DEVDRV_DMA_REMOTE_SIDE) {
        *rdie = 1;
        *msi = (u32)dma_chan->done_irq;
        dma_chan->status.trigger_remot_int_cnt++;
        return;
    }
    if (para_info != NULL) {
        if ((para_info->interrupt_and_attr_flag & DEVDRV_REMOTE_IRQ_FLAG) != 0) {
            *rdie = 1;
            *msi = para_info->remote_msi_vector;
            dma_chan->remote_irq_cnt++;
            dma_chan->status.trigger_remot_int_cnt++;

            /* add a local irq to update local SQ head and tail */
            if (dma_chan->remote_irq_cnt == DEVDRV_DMA_MAX_REMOTE_IRQ) {
                *ldie = 1;
                dma_chan->remote_irq_cnt = 0;
                dma_chan->status.trigger_local_128++;
            }
        }
        if ((para_info->interrupt_and_attr_flag & DEVDRV_LOCAL_IRQ_FLAG) != 0) {
            *ldie = 1;
        }
    } else {
        *ldie = 1;
    }
}

/* Avoid cq merge cause int loss when the previous sq's ldie==1, and current sq's ldie==0 & rdie==1, set ldie=1. */
STATIC u32 devdrv_dma_get_amended_ldie(struct devdrv_dma_channel *dma_chan, u32 ldie, u32 rdie)
{
    u32 amended_ldie = ldie;
    u32 irq_type = dma_chan->last_irq_type;

    if ((irq_type == DEVDRV_DMA_SQ_LDIE_ENABEL) && (rdie == 1) && (ldie == 0)) {
        amended_ldie = 1;
    }
    dma_chan->last_irq_type = ldie + (rdie << 1);

    return amended_ldie;
}

STATIC void devdrv_dma_fill_sq_desc(struct devdrv_dma_channel *dma_chan, struct devdrv_dma_sq_node *sq_desc,
    struct devdrv_dma_node *dma_node, struct devdrv_asyn_dma_para_info *para_info, int intr_flag, int pava_flag)
{
    struct devdrv_pci_ctrl *pci_ctrl = dma_chan->dma_dev->pci_ctrl;
    u32 rdie = 0;
    u32 ldie = 0;
    u32 msi = 0;
    u32 attr = 0;
    u32 wd_barrier = 0;
    u32 rd_barrier = 0;
    u32 chip_type = HISI_CHIP_NUM;
    u32 opcode;
    int connect_type;

    connect_type = devdrv_get_connect_protocol_by_dev(dma_chan->dev);
    if (connect_type == CONNECT_PROTOCOL_HCCS) {
        opcode = DEVDRV_DMA_LOOP;
    } else {
        if (dma_node->direction == DEVDRV_DMA_DEVICE_TO_HOST) {
            opcode = DEVDRV_DMA_WRITE;
        } else {
            opcode = DEVDRV_DMA_READ;
        }
    }

    chip_type = devdrv_get_dev_chip_type(dma_chan->dma_dev->dev_id);
    if (chip_type == HISI_CHIP_UNKNOWN) {
        devdrv_err_spinlock("Get chip type failed. (dev_id=%u)\n", dma_chan->dma_dev->dev_id);
    }

    if ((chip_type == HISI_CLOUD_V2) || (chip_type == HISI_MINI_V3)) {
        attr = DEVDRV_DMA_RO_RELEX_ORDER;
    }

    if (para_info != NULL) {
        if ((para_info->interrupt_and_attr_flag & DEVDRV_ATTR_FLAG) == 0) {
            attr = DEVDRV_DMA_RO_RELEX_ORDER;
        }
        if ((para_info->interrupt_and_attr_flag & DEVDRV_WD_BARRIER_FLAG) != 0) {
            wd_barrier = 1;
        }
        if ((para_info->interrupt_and_attr_flag & DEVDRV_RD_BARRIER_FLAG) != 0) {
            rd_barrier = 1;
        }
    }

    if (intr_flag == 1) {
        devdrv_dma_parse_sq_interrupt_info(dma_chan, para_info, &ldie, &rdie, &msi);
    }

    if (chip_type == HISI_MINI_V1) {
        ldie = devdrv_dma_get_amended_ldie(dma_chan, ldie, rdie);
    }

    /* fill addr */
    devdrv_dma_set_sq_addr_info(sq_desc, dma_node->src_addr, dma_node->dst_addr, dma_node->size);

    /* fill attr */
    devdrv_dma_set_sq_attr(sq_desc, opcode, attr, dma_chan->dma_dev, wd_barrier, rd_barrier);

    /* fill interrupt info */
    devdrv_dma_set_sq_irq(sq_desc, rdie, ldie, msi);

    /* fill passid info */
    devdrv_dma_set_passid(sq_desc, dma_node->loc_passid, dma_node->direction, pava_flag, connect_type);

    if (pci_ctrl->ops.flush_cache != NULL) {
        pci_ctrl->ops.flush_cache((u64)sq_desc, sizeof(struct devdrv_dma_sq_node), CACHE_CLEAN);
    }
}

STATIC void devdrv_dma_fill_soft_bd(int wait_type, int copy_type, struct devdrv_dma_soft_bd *soft_bd,
    struct devdrv_asyn_dma_para_info *para_info)
{
    if (para_info != NULL) {
        soft_bd->priv = para_info->priv;
        soft_bd->trans_id = para_info->trans_id;
        soft_bd->callback_func = para_info->finish_notify;
    } else {
        soft_bd->priv = NULL;
        soft_bd->trans_id = 0;
        soft_bd->callback_func = NULL;
    }

    soft_bd->copy_type = copy_type;
    soft_bd->wait_type = wait_type;
    soft_bd->owner_bd = -1;
    soft_bd->status = -1;
    atomic_set(&soft_bd->process_flag, DEVDRV_DMA_PROCESS_INIT);
    sema_init(&soft_bd->sync_sem, 0);
    soft_bd->valid = DEVDRV_ENABLE;
}

int devdrv_dma_para_check(u32 dev_id, enum devdrv_dma_data_type type, int copy_type,
    const struct devdrv_asyn_dma_para_info *para_info)
{
    int type_tmp;

    type_tmp = (int)type;

    if ((type_tmp >= DEVDRV_DMA_DATA_TYPE_MAX) || (type_tmp < DEVDRV_DMA_DATA_COMMON)) {
        devdrv_err_spinlock("Input parameter is invalid. (dev_id=%u; type_tmp=%u)\n", dev_id, type_tmp);
        return -DEVDRV_DMA_TYPE_ERR;
    }

    if (copy_type == DEVDRV_DMA_ASYNC) {
        if (para_info == NULL) {
            devdrv_err_spinlock("Device async mode para_info is null. (dev_id=%u)\n", dev_id);
            return -DEVDRV_DMA_NO_PARA;
        }

        if ((para_info->interrupt_and_attr_flag & DEVDRV_LOCAL_REMOTE_IRQ_FLAG) == 0) {
            if (para_info->finish_notify != NULL) {
                devdrv_info_spinlock("para_info is invalid. (dev_id=%u)\n", dev_id);
                return -DEVDRV_DMA_NO_NOTIFY;
            }
        }
    }
    return 0;
}

int devdrv_dma_node_check(u32 dev_id, const struct devdrv_dma_node *dma_node, u32 node_cnt,
    struct devdrv_dma_dev *dma_dev)
{
    u32 i;
    struct devdrv_dma_res *dma_res = NULL;

    if (dma_dev == NULL) {
        devdrv_err_spinlock("Dma_dev is null. (dev_id=%u)\n", dev_id);
        return -DEVDRV_DMA_NO_DEV;
    }

    dma_res = &((struct devdrv_pci_ctrl *)(dma_dev->drvdata))->res.dma_res;
    if ((node_cnt == 0) || (node_cnt > (dma_res->sq_depth - dma_res->sq_rsv_num))) {
        devdrv_err_spinlock("node_cnt is illegal. (dev_id=%u; node_cnt=%u)\n", dev_id, node_cnt);
        return -DEVDRV_DMA_CNT_ERR;
    }

    if (dma_node == NULL) {
        devdrv_err_spinlock("dma_node is null. (dev_id=%u)\n", dev_id);
        return -DEVDRV_DMA_NO_NODE;
    }

    for (i = 0; i < node_cnt; i++) {
        if (dma_node[i].size == 0) {
            devdrv_err_spinlock("Size is error. (dma_node=%d; size=%x)\n", i, dma_node[i].size);
            return -DEVDRV_DMA_SIZE_ERR;
        }

        if ((dma_node[i].direction != DEVDRV_DMA_DEVICE_TO_HOST) &&
            (dma_node[i].direction != DEVDRV_DMA_HOST_TO_DEVICE) && (dma_node[i].direction != DEVDRV_DMA_SYS_TO_SYS)) {
            devdrv_err_spinlock("Direction is error. (dma_node=%d; direction=%d)\n", i, dma_node[i].direction);
            return -DEVDRV_DMA_DIR_ERR;
        }
    }
    return 0;
}

struct devdrv_dma_channel *devdrv_dma_get_chan(u32 dev_id, enum devdrv_dma_data_type type)
{
    struct devdrv_dma_dev *dma_dev = NULL;
    struct data_type_chan *data_chan = NULL;
    u32 chan_id;

    if (type >= DEVDRV_DMA_DATA_TYPE_MAX) {
        return NULL;
    }

    dma_dev = devdrv_get_dma_dev(dev_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    data_chan = &dma_dev->data_chan[type];

    chan_id = data_chan->chan_start_id + ((data_chan->last_use_chan + 1) % data_chan->chan_num);
    data_chan->last_use_chan = chan_id;

    return &dma_dev->dma_chan[chan_id];
}

int devdrv_dma_get_sq_idle_bd_cnt(struct devdrv_dma_channel *dma_chan)
{
    u32 sq_tail, sq_head, sq_depth, sq_access;
    struct devdrv_dma_res *dma_res = &((struct devdrv_pci_ctrl *)(dma_chan->dma_dev->drvdata))->res.dma_res;

    sq_tail = dma_chan->sq_tail;
    sq_head = dma_chan->sq_head;
    sq_depth = dma_chan->sq_depth;

    sq_access = sq_depth - ((sq_tail + sq_depth - sq_head) % sq_depth) - (dma_res->sq_rsv_num);
    dma_chan->status.sq_idle_bd_cnt = sq_access;
    return (int)sq_access;
}

STATIC struct devdrv_dma_soft_bd *devdrv_get_soft_bd(struct devdrv_dma_channel *dma_chan)
{
    struct devdrv_dma_soft_bd *soft_bd = NULL;

    soft_bd = dma_chan->dma_soft_bd + dma_chan->sq_tail;

    return soft_bd;
}

STATIC struct devdrv_dma_sq_node *devdrv_get_sq_desc(struct devdrv_dma_channel *dma_chan)
{
    struct devdrv_dma_sq_node *sq_desc = NULL;

    sq_desc = dma_chan->sq_desc_base + dma_chan->sq_tail;

    return sq_desc;
}

STATIC void devdrv_updata_sq_tail(struct devdrv_dma_channel *dma_chan, int cnt)
{
    dma_chan->sq_tail = (dma_chan->sq_depth + dma_chan->sq_tail + cnt) % dma_chan->sq_depth;
}

STATIC int devdrv_dma_chan_sync_wait_intr(u32 dev_id, struct devdrv_dma_channel *dma_chan,
    struct devdrv_dma_soft_bd *soft_bd, const struct devdrv_dma_soft_bd_wait_status *wait_status)
{
    u32 dma_copy_timeout;
    u32 process_flag;
    int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    struct timespec64 start_time;
    struct timespec64 end_time;
#else
    struct timeval start_time;
    struct timeval end_time;
#endif

    if (dma_chan->dma_data_type == DEVDRV_DMA_DATA_TRAFFIC) {
        dma_copy_timeout = DEVDRV_DMA_COPY_MAX_TIMEOUT;
    } else {
        dma_copy_timeout = DEVDRV_DMA_COPY_TIMEOUT;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    ktime_get_real_ts64(&(start_time));
    ret = down_timeout(&soft_bd->sync_sem, dma_copy_timeout);
    ktime_get_real_ts64(&(end_time));
#else
    do_gettimeofday(&(start_time));
    ret = down_timeout(&soft_bd->sync_sem, dma_copy_timeout);
    do_gettimeofday(&(end_time));
#endif

    if (ret == 0) {
        return 0;
    }
    /* call done task if timeout */
    ret = 0;
RETRY_SYNC_WAIT:
    if (dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED) {
        return -ENODEV;
    }
    devdrv_dma_done_task((unsigned long)(uintptr_t)dma_chan);
    /* check soft_bd_wait_status valid */
    if (wait_status->valid == DEVDRV_ENABLE) {
        process_flag = (u32)atomic_cmpxchg(&soft_bd->process_flag, DEVDRV_DMA_PROCESS_INIT,
            DEVDRV_DMA_PROCESS_WAIT_TIMEOUT);
        if (process_flag == DEVDRV_DMA_PROCESS_HANDLING) {
            goto RETRY_SYNC_WAIT;
        }
        ret = -ETIMEDOUT;
        devdrv_dma_chan_ptr_show(dma_chan, wait_status->status);
        if (is_need_dma_copy_retry(dev_id, wait_status->status) == true) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
            devdrv_warn("Time out. (dev_id=%u; chan_id=%u; ret=%d; wait_time=%lld(us))\n",
                dev_id, dma_chan->chan_id, ret,
                (((u64)(end_time.tv_sec)) * DEVDRV_SECOND_TO_MICROSECOND +
                end_time.tv_nsec / DEVDRV_MICROSECOND_TO_NANOSECOND) -
                (((u64)(start_time.tv_sec)) * DEVDRV_SECOND_TO_MICROSECOND +
                start_time.tv_nsec / DEVDRV_MICROSECOND_TO_NANOSECOND));
#endif
        } else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
            devdrv_err("Time out. (dev_id=%u; chan_id=%u; ret=%d; wait_time=%lld(us))\n",
                dev_id, dma_chan->chan_id, ret,
                (((u64)(end_time.tv_sec)) * DEVDRV_SECOND_TO_MICROSECOND +
                end_time.tv_nsec / DEVDRV_MICROSECOND_TO_NANOSECOND) -
                (((u64)(start_time.tv_sec)) * DEVDRV_SECOND_TO_MICROSECOND +
                start_time.tv_nsec / DEVDRV_MICROSECOND_TO_NANOSECOND));
#else
            devdrv_err("Time out. (dev_id=%u; chan_id=%u; ret=%d; wait_time=%lld(us))\n",
                dev_id, dma_chan->chan_id, ret,
                (((u64)(end_time.tv_sec)) * DEVDRV_SECOND_TO_MICROSECOND + end_time.tv_usec) -
                (((u64)(start_time.tv_sec)) * DEVDRV_SECOND_TO_MICROSECOND + start_time.tv_usec));
#endif
        }
    }

    return ret;
}

STATIC int devdrv_dma_chan_sync_wait_query(u32 dev_id, struct devdrv_dma_channel *dma_chan,
    struct devdrv_dma_soft_bd *soft_bd, const struct devdrv_dma_soft_bd_wait_status *wait_status)
{
    int wait_cnt = 0;
    u32 process_flag;
    int ret = 0;

    do {
        if (dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED) {
            break;
        }
        /* check cq status,update soft_bd and soft_bd_wait_status */
        devdrv_dma_done_task((unsigned long)(uintptr_t)dma_chan);
        /* check soft_bd_wait_status valid */
        if (wait_status->valid == DEVDRV_DISABLE) {
            break;
        }

        rmb();

        if (wait_cnt++ > DEVDRV_DMA_QUERY_MAX_WAIT_LONG_TIME) {
            process_flag = (u32)atomic_cmpxchg(&soft_bd->process_flag, DEVDRV_DMA_PROCESS_INIT,
                DEVDRV_DMA_PROCESS_WAIT_TIMEOUT);
            if (process_flag == DEVDRV_DMA_PROCESS_HANDLING) {
                continue;
            }
            ret = -EINVAL;
            if (is_need_dma_copy_retry(dev_id, wait_status->status) == true) {
                devdrv_warn("Wait timeout. (dev_id=%u; chan_id=%u)\n", dev_id, dma_chan->chan_id);
            } else {
                devdrv_err("Wait timeout. (dev_id=%u; chan_id=%u)\n", dev_id, dma_chan->chan_id);
            }
            break;
        }
        usleep_range(DEVDRV_MSG_WAIT_MIN_TIME, DEVDRV_MSG_WAIT_MAX_TIME);
    } while (1);

    return ret;
}

STATIC int devdrv_dma_chan_sync_wait(u32 dev_id, struct devdrv_dma_channel *dma_chan,
    struct devdrv_dma_soft_bd *soft_bd, struct devdrv_dma_soft_bd_wait_status *wait_status)
{
    int ret;

    /* interrupt mode */
    if (soft_bd->wait_type == DEVDRV_DMA_WAIT_INTR) {
        ret = devdrv_dma_chan_sync_wait_intr(dev_id, dma_chan, soft_bd, wait_status);
    } else {
    /* query mode */
        ret = devdrv_dma_chan_sync_wait_query(dev_id, dma_chan, soft_bd, wait_status);
    }

    mb();
    if (wait_status->status == 0) {
        return ret;
    }
    devdrv_dma_chan_ptr_show(dma_chan, wait_status->status);
    if (is_need_dma_copy_retry(dev_id, wait_status->status) == true) {
        devdrv_warn("Dma copy failed. (dev_id=%u; chan_id=%u; valid=%d; status=%x)\n",
            dev_id, dma_chan->chan_id, wait_status->valid, wait_status->status);
    } else {
        devdrv_err("Dma copy failed. (dev_id=%u; chan_id=%u; valid=%d; status=%x)\n",
            dev_id, dma_chan->chan_id, wait_status->valid, wait_status->status);
    }
#ifdef CFG_BUILD_DEBUG
    dump_stack();
#endif
    return -EINVAL;
}

/* hccs peh adaptive, need check addr range */
int devdrv_peh_dma_node_addr_check(struct devdrv_dma_node *dma_node)
{
    if ((dma_node->direction == DEVDRV_DMA_HOST_TO_DEVICE) &&
        ((dma_node->src_addr + dma_node->size > DEVDRV_PEH_PHY_ADDR_MAX_VALUE) ||
        (dma_node->src_addr + dma_node->size <= dma_node->src_addr) ||
        (dma_node->src_addr >= DEVDRV_PEH_PHY_ADDR_MAX_VALUE))) {
        return -EINVAL;
    }

    if ((dma_node->direction == DEVDRV_DMA_DEVICE_TO_HOST) &&
        ((dma_node->dst_addr + dma_node->size > DEVDRV_PEH_PHY_ADDR_MAX_VALUE) ||
        (dma_node->dst_addr + dma_node->size <= dma_node->dst_addr) ||
        (dma_node->dst_addr >= DEVDRV_PEH_PHY_ADDR_MAX_VALUE))) {
        return -EINVAL;
    }

    return 0;
}

STATIC struct devdrv_dma_soft_bd *devdrv_dma_fill_sq_desc_and_soft_bd(struct devdrv_dma_channel *dma_chan,
    struct devdrv_dma_node *dma_node, u32 node_cnt, struct devdrv_dma_copy_para *para)
{
    struct devdrv_dma_sq_node *sq_desc = NULL;
    struct devdrv_dma_soft_bd *soft_bd = NULL;
    int intr_flag = (para->wait_type == DEVDRV_DMA_WAIT_INTR) ? 1 : 0;
    int connect_protocol = devdrv_get_connect_protocol_by_dev(dma_chan->dev);
    u32 last_sq_id, sq_index;
    int ret;

    last_sq_id = (dma_chan->sq_tail + node_cnt - 1U) % dma_chan->sq_depth;
    for (sq_index = 0; sq_index < node_cnt; sq_index++) {
        if (connect_protocol == CONNECT_PROTOCOL_HCCS) {
            ret = devdrv_peh_dma_node_addr_check(&dma_node[sq_index]);
            if (ret != 0) {
                devdrv_err_spinlock("Peh dma addr range check.(dev_id=%u)\n", dma_chan->dma_dev->dev_id);
                return NULL;
            }
        }

        sq_desc = devdrv_get_sq_desc(dma_chan);
        soft_bd = devdrv_get_soft_bd(dma_chan);

        if (memset_s((void *)sq_desc, DEVDRV_DMA_SQ_DESC_SIZE, 0, DEVDRV_DMA_SQ_DESC_SIZE) != 0) {
            devdrv_err_spinlock("memset_s failed.\n");
            return NULL;
        }

        if (sq_index < node_cnt - 1) {
            devdrv_dma_fill_sq_desc(dma_chan, sq_desc, &dma_node[sq_index], para->asyn_info, 0, para->pa_va_flag);
            soft_bd->owner_bd = (int)last_sq_id;
            soft_bd->valid = DEVDRV_ENABLE;
        } else {
            devdrv_dma_fill_sq_desc(dma_chan, sq_desc, &dma_node[sq_index], para->asyn_info,
                intr_flag, para->pa_va_flag);
            devdrv_dma_fill_soft_bd(para->wait_type, para->copy_type, soft_bd, para->asyn_info);
        }
        ret = devdrv_vpc_dma_iova_addr_check(dma_chan->dma_dev->pci_ctrl, sq_desc, dma_node[sq_index].direction);
        if (ret != 0) {
            devdrv_err_spinlock("Vm's dma_iova_addr_check check failed.(dev_id=%u)\n", dma_chan->dma_dev->dev_id);
            soft_bd->valid = DEVDRV_DISABLE;
            return NULL;
        }
        devdrv_updata_sq_tail(dma_chan, 1);
    }

    return soft_bd;
}

STATIC void devdrv_vpc_dma_sq_desc_info_init(u32 dev_id, u32 chan_id, u32 node_cnt, struct devdrv_dma_copy_para *para,
    struct devdrv_vpc_msg *sq_submit)
{
    sq_submit->error_code = 0;
    sq_submit->cmd_data.sq_cmd.dev_id = dev_id;
    sq_submit->cmd_data.sq_cmd.chan_id = chan_id;
    sq_submit->cmd_data.sq_cmd.node_cnt = node_cnt;
    sq_submit->cmd_data.sq_cmd.instance = para->instance;
    sq_submit->cmd_data.sq_cmd.type = para->type;
    sq_submit->cmd_data.sq_cmd.wait_type = para->wait_type;
    sq_submit->cmd_data.sq_cmd.pa_va_flag = para->pa_va_flag;
    if (para->asyn_info != NULL) {
        sq_submit->cmd_data.sq_cmd.asyn_info.trans_id = para->asyn_info->trans_id;
        sq_submit->cmd_data.sq_cmd.asyn_info.remote_msi_vector = para->asyn_info->remote_msi_vector;
        sq_submit->cmd_data.sq_cmd.asyn_info.interrupt_and_attr_flag = para->asyn_info->interrupt_and_attr_flag;
        sq_submit->cmd_data.sq_cmd.asyn_info.priv = para->asyn_info->priv;
        sq_submit->cmd_data.sq_cmd.asyn_info.finish_notify = para->asyn_info->finish_notify;
        sq_submit->cmd_data.sq_cmd.asyn_info_flag = DEVDRV_VPC_DMA_ASYN_INFO_NOT_NULL;
    } else {
        sq_submit->cmd_data.sq_cmd.asyn_info_flag = DEVDRV_VPC_DMA_ASYN_INFO_IS_NULL;
    }
}

STATIC void devdrv_vpc_dma_sq_desc_init(struct devdrv_dma_node *dma_node, u32 node_cnt,
    struct devdrv_vpc_msg *sq_submit)
{
    u32 sq_index;

    for (sq_index = 0; sq_index < node_cnt; sq_index++) {
        sq_submit->cmd_data.sq_cmd.dma_node[sq_index].src_addr = dma_node[sq_index].src_addr;
        sq_submit->cmd_data.sq_cmd.dma_node[sq_index].dst_addr = dma_node[sq_index].dst_addr;
        sq_submit->cmd_data.sq_cmd.dma_node[sq_index].size = dma_node[sq_index].size;
        sq_submit->cmd_data.sq_cmd.dma_node[sq_index].direction = dma_node[sq_index].direction;
        sq_submit->cmd_data.sq_cmd.dma_node[sq_index].loc_passid = dma_node[sq_index].loc_passid;
    }
}

STATIC struct devdrv_vpc_msg *devdrv_vpc_dma_sq_desc_addr_alloc(struct devdrv_dma_channel *dma_chan)
{
    u32 sq_submit_buf_len;

    sq_submit_buf_len = (u32)(sizeof(struct devdrv_dma_node) * DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT +
        sizeof(struct devdrv_vpc_msg));
    dma_chan->sq_submit = (struct devdrv_vpc_msg *)devdrv_kzalloc(sq_submit_buf_len, GFP_KERNEL);
    if (dma_chan->sq_submit == NULL) {
        devdrv_err("Alloc sq_submit fail.\n");
        return NULL;
    }

    return dma_chan->sq_submit;
}

int devdrv_dma_chan_copy_by_vpc(u32 dev_id, struct devdrv_dma_channel *dma_chan, struct devdrv_dma_node *dma_node,
    u32 node_cnt, struct devdrv_dma_copy_para *para)
{
    struct devdrv_dma_soft_bd_wait_status wait_status;
    struct devdrv_dma_soft_bd *soft_bd = NULL;
    int ret = 0, err_code;
    int sq_idle_bd_cnt;
    u32 chan_id;

    if (node_cnt > DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT) {
        devdrv_err("Dma node_cnt[%u] is too big.\n", node_cnt);
        return -EINVAL;
    }

    mutex_lock(&dma_chan->vm_sq_lock);

    /* sq_submit only alloc once, will free in devdrv_dma_exit */
    if (dma_chan->sq_submit == NULL) {
        dma_chan->sq_submit = devdrv_vpc_dma_sq_desc_addr_alloc(dma_chan);
        if (dma_chan->sq_submit == NULL) {
            mutex_unlock(&dma_chan->vm_sq_lock);
            return -EINVAL;
        }
    }

    /* wait till chan space enough */
    chan_id = dma_chan->chan_id;
    dma_chan->status.dma_chan_copy_cnt++;

    sq_idle_bd_cnt = devdrv_dma_get_sq_idle_bd_cnt(dma_chan);
    if ((sq_idle_bd_cnt < 0) || ((u32)sq_idle_bd_cnt < node_cnt)) {
        mutex_unlock(&dma_chan->vm_sq_lock);
        devdrv_warn("No space. (dev_id=%u; chan_id=%u; idle_bd=%d; need=%u)\n",
            dev_id, chan_id, sq_idle_bd_cnt, node_cnt);
        return -ENOSPC;
    }

    devdrv_vpc_dma_sq_desc_info_init(dev_id, chan_id, node_cnt, para, dma_chan->sq_submit);
    devdrv_vpc_dma_sq_desc_init(dma_node, node_cnt, dma_chan->sq_submit);
    soft_bd = devdrv_dma_fill_sq_desc_and_soft_bd(dma_chan, dma_node, node_cnt, para);
    if (soft_bd == NULL) {
        mutex_unlock(&dma_chan->vm_sq_lock);
        devdrv_warn("Fill sq_desc and soft_bd fail. (dev_id=%u; chan_id=%u; need=%d)\n",
            dev_id, chan_id, node_cnt);
        return -EINVAL;
    }

    if (para->copy_type == DEVDRV_DMA_SYNC) {
        wait_status.status = -1;
        wait_status.valid = DEVDRV_ENABLE;
        soft_bd->priv = &wait_status;
        dma_chan->status.sync_submit_cnt++;
    } else {
        dma_chan->status.async_submit_cnt++;
    }

    wmb();

    ret = devdrv_vpc_msg_send(dev_id, DEVDRV_VPC_MSG_TYPE_SQ_SUBMIT, dma_chan->sq_submit,
        (u32)sizeof(struct devdrv_dma_node) * node_cnt + sizeof(struct devdrv_vpc_msg),
        DEVDRV_VPC_MSG_DEFAULT_TIMEOUT);
    err_code = dma_chan->sq_submit->error_code;
    if ((ret != 0) || (err_code != 0)) {
        soft_bd->valid = DEVDRV_DISABLE;
        if ((ret != 0) || (err_code == -ENOSPC)) {
            devdrv_updata_sq_tail(dma_chan, -(node_cnt));
            if (para->copy_type == DEVDRV_DMA_SYNC) {
                dma_chan->status.sync_submit_cnt--;
            } else {
                dma_chan->status.async_submit_cnt--;
            }
        }
        mutex_unlock(&dma_chan->vm_sq_lock);
        devdrv_err("Vpc send fail. (dev_id=%u; chan_id=%u; error_code=%d; ret=%d)\n",
            dev_id, chan_id, err_code, ret);
        if (ret == 0) {
            return err_code;
        } else {
            return ret;
        }
    }
    mutex_unlock(&dma_chan->vm_sq_lock);

    if ((para->copy_type == DEVDRV_DMA_SYNC) && (soft_bd != NULL)) {
        ret = devdrv_dma_chan_sync_wait(dev_id, dma_chan, soft_bd, &wait_status);
    }

    return ret;
}

bool is_need_dma_copy_retry(u32 dev_id, int wait_status)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_pci_ctrl_get_no_ref(dev_id);
    if (pci_ctrl == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        } else {
            devdrv_err_limit("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        }
        return false;
    }

    if (pci_ctrl->device_status != DEVDRV_DEVICE_ALIVE) {
        return false;
    }

    if ((pci_ctrl->addr_mode == DEVDRV_ADMODE_FULL_MATCH) &&
        (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) &&
        ((wait_status == DEVDRV_DMA_READ_RESPONSE_ERROR) ||
        (wait_status == DEVDRV_DMA_WRITE_RESPONSE_ERROR) ||
        (wait_status == DEVDRV_DMA_DATA_POISON_RECEIVED))) {
        return true;
    } else {
        return false;
    }
}

#ifdef CFG_FEATURE_AGENT_S2S
STATIC bool devdrv_dma_chan_copy_retry_judge(u32 dev_id, struct devdrv_dma_soft_bd_wait_status wait_status,
    u8 *retry_cnt, int timeout)
{
    int timeout_tmp;

    if ((is_need_dma_copy_retry(dev_id, wait_status.status) == true) && ((*retry_cnt) < DEVDRV_S2S_MSG_RETRY_LIMIT)) {
        (*retry_cnt)++;
        devdrv_info("devdrv_dma_chan_copy retry. (dev_id=%u, status=0x%x, retry_cnt=%u)\n",
            dev_id, wait_status.status, *retry_cnt);
        timeout_tmp = timeout;
        /* wait 3s */
        while (timeout_tmp > 0) {
            rmb();
            usleep_range(DEVDRV_MSG_WAIT_MIN_TIME, DEVDRV_MSG_WAIT_MAX_TIME);
            timeout_tmp -= DEVDRV_MSG_WAIT_MIN_TIME;
        }
        mb();
        return true;
    }

    return false;
}
#endif

int devdrv_dma_chan_copy(u32 dev_id, struct devdrv_dma_channel *dma_chan, struct devdrv_dma_node *dma_node,
    u32 node_cnt, struct devdrv_dma_copy_para *para)
{
    struct devdrv_dma_soft_bd_wait_status wait_status;
    struct devdrv_dma_soft_bd *soft_bd = NULL;
    u32 chan_id;
    int sq_idle_bd_cnt;
    int ret = 0;
#ifdef CFG_FEATURE_AGENT_S2S
    u8 retry_cnt = 0;

RETRY:
#endif
    if (devdrv_is_mdev_vm_boot_mode(dev_id) == true) {
        ret = devdrv_dma_chan_copy_by_vpc(dev_id, dma_chan, dma_node, node_cnt, para);
        return ret;
    }

    /* wait till chan space enough */
    spin_lock_bh(&dma_chan->lock);

    if (dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED) {
        spin_unlock_bh(&dma_chan->lock);
        return -ENODEV;
    }

    chan_id = dma_chan->chan_id;
    dma_chan->status.dma_chan_copy_cnt++;

    sq_idle_bd_cnt = devdrv_dma_get_sq_idle_bd_cnt(dma_chan);
    if ((sq_idle_bd_cnt < 0) || ((u32)sq_idle_bd_cnt < node_cnt)) {
        spin_unlock_bh(&dma_chan->lock);
        devdrv_warn_spinlock("No space. (dev_id=%u; chan_id=%u; idle_bd=%d; need=%u)\n",
            dev_id, chan_id, sq_idle_bd_cnt, node_cnt);
        return -ENOSPC;
    }

    soft_bd = devdrv_dma_fill_sq_desc_and_soft_bd(dma_chan, dma_node, node_cnt, para);
    if (soft_bd == NULL) {
        spin_unlock_bh(&dma_chan->lock);
#ifdef CFG_BUILD_DEBUG
        dump_stack();
#endif
        devdrv_warn_spinlock("Fill sq_desc and soft_bd fail. (dev_id=%u; chan_id=%u; idle_bd=%d; need=%d)\n",
            dev_id, chan_id, sq_idle_bd_cnt, node_cnt);
        return -EINVAL;
    }

    if (para->copy_type == DEVDRV_DMA_SYNC) {
        wait_status.status = -1;
        wait_status.valid = DEVDRV_ENABLE;
        soft_bd->priv = &wait_status;
        dma_chan->status.sync_submit_cnt++;
    } else {
        dma_chan->status.async_submit_cnt++;
    }

    wmb();
    devdrv_dma_ch_sq_submit(dma_chan);

    spin_unlock_bh(&dma_chan->lock);

    if ((para->copy_type == DEVDRV_DMA_SYNC) && (soft_bd != NULL)) {
        ret = devdrv_dma_chan_sync_wait(dev_id, dma_chan, soft_bd, &wait_status);
#ifdef CFG_FEATURE_AGENT_S2S
        if (devdrv_dma_chan_copy_retry_judge(dev_id, wait_status, &retry_cnt, DEVDRV_DMA_COPY_RETRY_DELAY)) {
            goto RETRY;
        }
#endif
    }

    return ret;
}

void devdrv_dma_copy_type_info_init(struct devdrv_dma_copy_para *para, enum devdrv_dma_data_type type,
    int wait_type, int copy_type)
{
    para->type = type;
    para->wait_type = wait_type;
    para->copy_type = copy_type;
}

void devdrv_dma_copy_para_info_init(struct devdrv_dma_copy_para *para, int pava_flag, int instance,
    struct devdrv_asyn_dma_para_info *asyn_info)
{
    para->pa_va_flag = pava_flag;
    para->instance = instance;
    para->asyn_info = asyn_info;
}

int devdrv_dma_copy(struct devdrv_dma_dev *dma_dev, struct devdrv_dma_node *dma_node, u32 node_cnt,
    struct devdrv_dma_copy_para *para)
{
    struct devdrv_dma_channel *dma_chan = NULL;
    struct data_type_chan *data_chan = NULL;
    u32 entry, i;
    int dev_id;
    int ret = 0;
    u32 device_status;

    if (dma_dev == NULL) {
        devdrv_err_spinlock("dma_dev is null.\n");
        return -EINVAL;
    }
    dev_id = (int)dma_dev->dev_id;

    device_status = dma_dev->pci_ctrl->device_status;
    if ((device_status != DEVDRV_DEVICE_ALIVE) && (device_status != DEVDRV_DEVICE_SUSPEND) &&
        (dma_node->direction == DEVDRV_DMA_HOST_TO_DEVICE)) {
        devdrv_warn_spinlock("Device is abnormal, can't dma copy to device. (dev_id=%d)\n", dev_id);
        return -ENODEV;
    }

    if (dma_dev->dev_status != DEVDRV_DMA_ALIVE) {
        devdrv_warn_spinlock("Dma dev disable. (dev_id=%d)\n", dev_id);
        return -ENODEV;
    }

    devdrv_debug_spinlock("Get copy_type. (type=%x; instance=%d; node_cnt=%x; copy_type=%d)\n",
                          para->type, para->instance, node_cnt, para->copy_type);
    data_chan = &dma_dev->data_chan[para->type];

    /* If wait spinlock in the tasklet, the cq interrupt that updates the sq head also be processed in the tasklet,
        which will form a deadlock. So let the caller waits */
    if (para->instance == DEVDRV_INVALID_INSTANCE) {
        for (i = 0; i < data_chan->chan_num; i++) {
            entry = data_chan->chan_start_id + ((i + data_chan->last_use_chan + 1) % data_chan->chan_num);
            dma_chan = &dma_dev->dma_chan[entry];
            ret = devdrv_dma_chan_copy((u32)dev_id, dma_chan, dma_node, node_cnt, para);
            if (ret != -ENOSPC) {
                data_chan->last_use_chan = entry;
                break;
            }
        }
    } else {
        entry = data_chan->chan_start_id + ((u32)para->instance % (u32)data_chan->chan_num);
        dma_chan = &dma_dev->dma_chan[entry];
        ret = devdrv_dma_chan_copy((u32)dev_id, dma_chan, dma_node, node_cnt, para);
    }

    return ret;
}

STATIC struct devdrv_dma_channel *devdrv_dma_get_chan_by_type(struct devdrv_dma_dev *dma_dev,
                                                              enum devdrv_dma_data_type type)
{
    struct data_type_chan *data_chan = NULL;
    u32 chan_id;

    if (type >= DEVDRV_DMA_DATA_TYPE_MAX) {
        devdrv_err("type is out of range. (type=%d)\n", (int)type);
        return NULL;
    }
    if (dma_dev == NULL) {
        devdrv_err("Parameter dma_dev is null.\n");
        return NULL;
    }

    data_chan = &dma_dev->data_chan[type];
    chan_id = data_chan->chan_start_id + ((data_chan->last_use_chan + 1) % data_chan->chan_num);
    data_chan->last_use_chan = chan_id;

    return &dma_dev->dma_chan[chan_id];
}

STATIC int devdrv_dma_chan_copy_sml_pkt(int dev_id, struct devdrv_dma_channel *dma_chan, dma_addr_t dst,
    const void *data, u32 size)
{
    struct devdrv_dma_soft_bd_wait_status wait_status;
    struct devdrv_dma_sq_node *sq_desc = NULL;
    struct devdrv_dma_soft_bd *soft_bd = NULL;
    int connect_type = devdrv_get_connect_protocol((u32)dev_id);

    spin_lock_bh(&dma_chan->lock);

    if (devdrv_dma_get_sq_idle_bd_cnt(dma_chan) < DEVDRV_DMA_SML_PKT_SQ_DESC_NUM) {
        spin_unlock_bh(&dma_chan->lock);
        devdrv_warn("Sq space not enough in small pkt. (dev_id=%d; chan_id=%d; sq_tail=%d; sq_head=%d; sq_depth=%d)\n",
            dev_id, dma_chan->chan_id, dma_chan->sq_tail, dma_chan->sq_head, dma_chan->sq_depth);
        return -ENOSPC;
    }

    sq_desc = dma_chan->sq_desc_base + dma_chan->sq_tail;
    if (memset_s((void *)sq_desc, DEVDRV_DMA_SQ_DESC_SIZE, 0, DEVDRV_DMA_SQ_DESC_SIZE) != 0) {
        spin_unlock_bh(&dma_chan->lock);
        devdrv_err("memset_s failed. (dev_id=%d)\n", dev_id);
        return -ENOMEM;
    }

    /* fill addr */
    devdrv_dma_set_sq_addr_info(sq_desc, 0, dst, size);

    /* fill attr */
    devdrv_dma_set_sq_attr(sq_desc, DEVDRV_DMA_SMALL_PACKET, 0, dma_chan->dma_dev, 1, 1);

    /* fill interrupt info */
    devdrv_dma_set_sq_irq(sq_desc, 0, 1, 0);

    /* fill passid info */
    devdrv_dma_set_passid(sq_desc, DEVDRV_DMA_PASSID_DEFAULT,
        DEVDRV_DMA_HOST_TO_DEVICE, DEVDRV_DMA_VA_COPY, connect_type);

    soft_bd = dma_chan->dma_soft_bd + dma_chan->sq_tail;
    devdrv_dma_fill_soft_bd(DEVDRV_DMA_WAIT_QUREY, DEVDRV_DMA_SYNC, soft_bd, NULL);

    dma_chan->sq_tail = (dma_chan->sq_tail + 1) % dma_chan->sq_depth;
    sq_desc = dma_chan->sq_desc_base + dma_chan->sq_tail;

    if (memcpy_s((void *)sq_desc, sizeof(struct devdrv_dma_sq_node), data, size) != 0) {
        spin_unlock_bh(&dma_chan->lock);
        devdrv_err("memcpy_s failed. (dev_id=%d)\n", dev_id);
        return -ENOMEM;
    }
    dma_chan->sq_tail = (dma_chan->sq_tail + 1) % dma_chan->sq_depth;

    wait_status.status = -1;
    wait_status.valid = DEVDRV_ENABLE;
    soft_bd->priv = &wait_status;

    wmb();
    devdrv_dma_ch_sq_submit(dma_chan);

    dma_chan->status.sml_submit_cnt++;
    spin_unlock_bh(&dma_chan->lock);

    return devdrv_dma_chan_sync_wait((u32)dev_id, dma_chan, soft_bd, &wait_status);
}

int devdrv_dma_copy_sml_pkt(struct devdrv_dma_dev *dma_dev, enum devdrv_dma_data_type type, dma_addr_t dst,
    const void *data, u32 size)
{
    struct devdrv_dma_channel *dma_chan = NULL;
    int dev_id = -1;
    int ret;

    dma_chan = devdrv_dma_get_chan_by_type(dma_dev, type);
    if (dma_chan == NULL) {
        devdrv_err("call devdrv_dma_get_chan failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    dev_id = (int)dma_dev->dev_id;

    if (((dma_chan->flag >> DEVDRV_DMA_SML_PKT_BIT) & 1U) == DEVDRV_DISABLE) {
        devdrv_err("This channel not support small packet. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (size > DEVDRV_DMA_SML_PKT_DATA_SIZE) {
        devdrv_err("Size is too big. (dev_id=%u; size=%d)\n", dev_id, size);
        return -EINVAL;
    }

    ret = devdrv_dma_chan_copy_sml_pkt(dev_id, dma_chan, dst, data, size);

    return ret;
}

void devdrv_free_dma_sq_cq(struct devdrv_dma_channel *dma_chan)
{
    u64 sq_size, cq_size;

    devdrv_set_dma_chan_status(dma_chan, DEVDRV_DMA_CHAN_DISABLED);
    if (dma_chan->sq_desc_base != NULL) {
        sq_size = ((u64)sizeof(struct devdrv_dma_sq_node)) * dma_chan->sq_depth;

        devdrv_dma_free_coherent(dma_chan->dev, sq_size, dma_chan->sq_desc_base, dma_chan->sq_desc_dma);

        dma_chan->sq_desc_base = NULL;
    }

    if (dma_chan->cq_desc_base != NULL) {
        cq_size = ((u64)sizeof(struct devdrv_dma_cq_node)) * dma_chan->cq_depth;

        devdrv_dma_free_coherent(dma_chan->dev, cq_size, dma_chan->cq_desc_base, dma_chan->cq_desc_dma);

        dma_chan->cq_desc_base = NULL;
    }

    if (dma_chan->dma_soft_bd != NULL) {
        devdrv_vfree(dma_chan->dma_soft_bd);
        dma_chan->dma_soft_bd = NULL;
    }
}

int devdrv_alloc_dma_sq_cq(struct devdrv_dma_channel *dma_chan)
{
    struct devdrv_dma_soft_bd *soft_virt_addr = NULL;
    void *sq_virt_addr = NULL;
    void *cq_virt_addr = NULL;
    struct device *dev = NULL;
    u64 soft_size, sq_size, cq_size;
    struct devdrv_dma_res *dma_res = &((struct devdrv_pci_ctrl *)(dma_chan->dma_dev->drvdata))->res.dma_res;
    int i;

    dev = dma_chan->dev;
    sq_size = DEVDRV_DMA_SQ_DESC_SIZE * (dma_res->sq_depth);
    cq_size = DEVDRV_DMA_CQ_DESC_SIZE * (dma_res->cq_depth);
    soft_size = sizeof(struct devdrv_dma_soft_bd) * (dma_res->sq_depth);

    sq_virt_addr = devdrv_dma_zalloc_coherent(dev, sq_size, &dma_chan->sq_desc_dma, GFP_KERNEL);
    if (sq_virt_addr == NULL) {
        devdrv_err("Sq alloc failed. (chan_id=%d)\n", dma_chan->chan_id);
        return -ENOMEM;
    }
    dma_chan->sq_desc_base = (struct devdrv_dma_sq_node *)sq_virt_addr;
    dma_chan->sq_depth = dma_res->sq_depth;

    cq_virt_addr = devdrv_dma_zalloc_coherent(dev, cq_size, &dma_chan->cq_desc_dma, GFP_KERNEL);
    if (cq_virt_addr == NULL) {
        devdrv_err("Cq alloc failed. (chan_id=%d)\n", dma_chan->chan_id);
        devdrv_free_dma_sq_cq(dma_chan);
        return -ENOMEM;
    }
    dma_chan->cq_desc_base = (struct devdrv_dma_cq_node *)cq_virt_addr;
    dma_chan->cq_depth = dma_res->cq_depth;

    /* DMA_QUEUE_SQ_BASE/DMA_QUEUE_CQ_BASE Note:the address must be 64Bytes aligned. */
    if (((dma_chan->sq_desc_dma % DEVDRV_DMA_REG_ALIGN_SIZE) != 0) ||
        ((dma_chan->cq_desc_dma % DEVDRV_DMA_REG_ALIGN_SIZE) != 0)) {
        devdrv_err("Address dont aligned with 64B. (driver_name=\"%s\"; chan_id=%d)\n",
            dev_driver_string(dev), dma_chan->chan_id);
        devdrv_free_dma_sq_cq(dma_chan);
        return -EFAULT;
    }

    devdrv_debug("Get dma channel. (chan_id=%d)\n", dma_chan->chan_id);
    soft_virt_addr = (struct devdrv_dma_soft_bd *)devdrv_vzalloc(soft_size);
    if (soft_virt_addr == NULL) {
        devdrv_err("Cq alloc failed. (chan_id=%d)\n", dma_chan->chan_id);
        devdrv_free_dma_sq_cq(dma_chan);
        return -ENOMEM;
    }
    dma_chan->dma_soft_bd = soft_virt_addr;

    for (i = 0; i < dma_res->sq_depth; i++) {
        soft_virt_addr[i].valid = DEVDRV_DISABLE;
    }

    return 0;
}

STATIC int devdrv_vm_dma_init_and_alloc_sq_cq(struct devdrv_dma_channel *dma_chan)
{
    struct devdrv_dma_dev *dma_dev = dma_chan->dma_dev;
    struct devdrv_vpc_msg vpc_msg = {0};
    int ret;

    if (devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == false) {
        return 0;
    }

    vpc_msg.cmd_data.dma_init.dev_id = dma_dev->dev_id;
    vpc_msg.cmd_data.dma_init.chan_id = dma_chan->chan_id;
    vpc_msg.error_code = 0;
    ret = devdrv_vpc_msg_send(dma_dev->dev_id, DEVDRV_VPC_MSG_TYPE_DMA_INIT_AND_ALLOC_SQCQ, &vpc_msg,
        (u32)sizeof(struct devdrv_vpc_msg), DEVDRV_VPC_MSG_MAX_TIMEOUT);
    if ((ret != 0) || (vpc_msg.error_code != 0)) {
        devdrv_err("Vpc send fail. (dev_id=%u; chan_id=%u; error_code=%d; ret=%d)\n",
            dma_dev->dev_id, dma_chan->chan_id, vpc_msg.error_code, ret);
        return -ENOSPC;
    }

    return 0;
}

STATIC void devdrv_vm_free_dma_sq_cq(struct devdrv_dma_channel *dma_chan)
{
    struct devdrv_dma_dev *dma_dev = dma_chan->dma_dev;
    struct devdrv_vpc_msg vpc_msg = {0};
    int ret;

    if (devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == false) {
        return;
    }

    vpc_msg.cmd_data.dma_init.dev_id = dma_dev->dev_id;
    vpc_msg.cmd_data.dma_init.chan_id = dma_chan->chan_id;
    vpc_msg.error_code = 0;
    ret = devdrv_vpc_msg_send(dma_dev->dev_id, DEVDRV_VPC_MSG_TYPE_FREE_DMA_SQCQ, &vpc_msg,
        (u32)sizeof(struct devdrv_vpc_msg), DEVDRV_VPC_MSG_MAX_TIMEOUT);
    if ((ret != 0) || (vpc_msg.error_code != 0)) {
        devdrv_err("Vpc send fail. (dev_id=%u; chan_id=%u; error_code=%d; ret=%d)\n",
            dma_dev->dev_id, dma_chan->chan_id, vpc_msg.error_code, ret);
        return;
    }

    return;
}

STATIC void devdrv_dma_interrupt_init_chan(struct devdrv_dma_dev *dma_dev, u32 entry_id)
{
    struct devdrv_dma_channel *dma_chan = &dma_dev->dma_chan[entry_id];
    u32 dma_chan_id = dma_chan->chan_id;
    int done_irq_register = dma_chan->done_irq;
    int ret = 0;

    devdrv_dma_update_msix_entry_offset(dma_dev->drvdata, &dma_chan->done_irq, 1);

    if (devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == true) {
        dma_chan->dma_done_workqueue = create_singlethread_workqueue("dma-done-work");
        if (dma_chan->dma_done_workqueue == NULL) {
            devdrv_err("Create dma done workqueue fail. (dev_id=%u)\n", dma_dev->dev_id);
            return;
        }
        INIT_WORK(&dma_chan->dma_done_work, devdrv_dma_done_work);
    }
    tasklet_init(&dma_chan->dma_done_task, devdrv_dma_done_task, (uintptr_t)dma_chan);

    ret = devdrv_register_irq_by_vector_index(dma_dev->dev_id,
        done_irq_register,
        devdrv_dma_done_interrupt,
        dma_chan,
        "dma_done_irq");
    if (ret != 0) {
        devdrv_warn("dma_done_irq register abnormal. (ret=%d, dev_id=%u, irq_index=%d)\n",
            ret, dma_dev->dev_id, done_irq_register);
        dma_chan->done_irq_state = DEVDRV_IRQ_IS_UNINIT;
    } else {
        dma_chan->done_irq_state = DEVDRV_IRQ_IS_INIT;
    }

    if (dma_chan->err_irq_flag != 0) {
        ret = devdrv_register_irq_by_vector_index(dma_dev->dev_id, dma_chan->err_irq, devdrv_dma_err_interrupt,
            dma_chan, "dma_err_irq");
        if (ret != 0) {
            dma_chan->err_irq_state = DEVDRV_IRQ_IS_UNINIT;
            devdrv_err("dma_err_irq register fail. (ret=%d, dev_id=%u, irq_index=%d)\n",
                ret, dma_dev->dev_id, dma_chan->err_irq);
        } else {
            /* err interrupt we do some dfx words, so use wordqueue which can sleep */
            INIT_WORK(&dma_chan->err_work, devdrv_dma_err_task);
            dma_chan->err_irq_state = DEVDRV_IRQ_IS_INIT;
        }
    } else {
        dma_chan->err_irq = -1;
    }

    if (dma_dev->sq_cq_side == DEVDRV_DMA_REMOTE_SIDE) {
        ret = devdrv_notify_dma_err_irq(dma_dev->drvdata, dma_chan_id, dma_chan->err_irq);
        if (ret != 0) {
            devdrv_err("Notify err_irq failed. (dev_id=%u; entry_id=%u; dma_chan_id=%u; ret=%d)\n",
                       dma_dev->dev_id, entry_id, dma_chan_id, ret);
        }
    }
    return;
}

void devdrv_dma_chan_info_init(struct devdrv_dma_dev *dma_dev, u32 entry_id, u32 dma_chan_id)
{
    struct devdrv_dma_channel *dma_chan = &dma_dev->dma_chan[entry_id];
    u32 bar_chan_id;

    dma_chan->dma_dev = dma_dev;
    dma_chan->dev = dma_dev->dev;
    bar_chan_id = dma_chan_id - dma_dev->remote_chan_begin + dma_dev->remote_bar_begin;
    dma_chan->io_base = dma_dev->dma_chan_base + (u64)bar_chan_id * DEVDRV_DMA_CHAN_OFFSET; //lint !e571
    dma_chan->chan_id = dma_chan_id;
    dma_chan->sq_tail = 0;
    dma_chan->sq_head = 0;
    dma_chan->cq_head = dma_chan->cq_depth - 1;
    dma_chan->rounds = 0;
    dma_chan->func_id = dma_dev->func_id;
    dma_chan->chan_status = DEVDRV_DMA_CHAN_ENABLED;

    if (dma_dev->sq_cq_side == DEVDRV_DMA_REMOTE_SIDE) {
        /* flags of DMA chan used in host */
        dma_chan->flag =
            (DEVDRV_DMA_REMOTE_SIDE << DEVDRV_DMA_SQCQ_SIDE_BIT) | (DEVDRV_DISABLE << DEVDRV_DMA_SML_PKT_BIT);
    } else {
        /* flags of DMA chan used in device */
        dma_chan->flag =
            ((u32)DEVDRV_DMA_LOCAL_SIDE << DEVDRV_DMA_SQCQ_SIDE_BIT) | (DEVDRV_ENABLE << DEVDRV_DMA_SML_PKT_BIT);
    }
}

int devdrv_dma_init_chan(struct devdrv_dma_dev *dma_dev, u32 entry_id, u32 dma_chan_id, u32 sriov_flag)
{
    int ret;
    struct devdrv_dma_channel *dma_chan = &dma_dev->dma_chan[entry_id];

    devdrv_dma_chan_info_init(dma_dev, entry_id, dma_chan_id);

    if (devdrv_is_mdev_pm_boot_mode(dma_dev->dev_id) == true) {
        return 0;
    }

    if (devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == true) {
        /* mdev + sriov, vm need reset dma chan, so set sriov_flag is enable */
        sriov_flag = DEVDRV_SRIOV_ENABLE;
    }

    /* reset DMA channel before init */
    ret = devdrv_dma_chan_reset(dma_chan, sriov_flag);
    if (ret != 0) {
        devdrv_err("dma_ch_cfg_reset failed. (dev_id=%u; chan=%u; ret=%d)\n", dma_dev->dev_id, entry_id, ret);
        dma_chan->dma_dev = NULL;
        dma_chan->dev = NULL;
        return ret;
    }

    ret = devdrv_vm_dma_init_and_alloc_sq_cq(dma_chan);
    if (ret != 0) {
        devdrv_err("Alloc vm dma sq cq failed. (dev_id=%u; chan=%u)\n", dma_dev->dev_id, entry_id);
        dma_chan->dma_dev = NULL;
        dma_chan->dev = NULL;
        return ret;
    }

    ret = devdrv_alloc_dma_sq_cq(dma_chan);
    if (ret != 0) {
        devdrv_err("Alloc dma sq cq failed. (dev_id=%u; chan=%u)\n", dma_dev->dev_id, entry_id);
        devdrv_vm_free_dma_sq_cq(dma_chan);
        dma_chan->dma_dev = NULL;
        dma_chan->dev = NULL;
        return ret;
    }

    ret = devdrv_dma_chan_init(dma_chan, (u32)devdrv_get_env_boot_type(dma_dev->dev_id));
    if (ret != 0) {
        devdrv_err("Remote init failed. (dev_id=%u; chan=%u; ret=%d)\n", dma_dev->dev_id, entry_id, ret);
        devdrv_vm_free_dma_sq_cq(dma_chan);
        devdrv_free_dma_sq_cq(dma_chan);
        dma_chan->dma_dev = NULL;
        dma_chan->dev = NULL;
        return ret;
    }

    devdrv_dma_interrupt_init_chan(dma_dev, entry_id);

    return ret;
}

int devdrv_sriov_dma_init_chan(struct devdrv_dma_dev *dma_dev)
{
    struct devdrv_dma_channel *dma_chan = NULL;
    int ret;
    u32 i;

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        if ((dma_dev->func_id == 0) && (i == 0)) {
            continue;
        }
        dma_chan = &dma_dev->dma_chan[i];

        ret = devdrv_dma_init_chan(dma_dev, i, dma_chan->chan_id, DEVDRV_SRIOV_ENABLE);
        if (ret != 0) {
            devdrv_err("Dma chan init failed. (dev_id=%u; chan_id=%u)\n", dma_dev->dev_id, i);
            return ret;
        }
    }

    return 0;
}

STATIC void devdrv_dma_chan_data_type_init(struct devdrv_dma_dev *dma_dev)
{
    u32 i = 0;

    for (i = 0; i < dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_num; i++) {
        dma_dev->dma_chan[i + dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_start_id].dma_data_type =
            DEVDRV_DMA_DATA_COMMON;
    }

    for (i = 0; i < dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_num; i++) {
        dma_dev->dma_chan[i + dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_start_id].dma_data_type =
            DEVDRV_DMA_DATA_PCIE_MSG;
    }

    /* if all data type use one dma chan, set dma_data_type to traffic type */
    for (i = 0; i < dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num; i++) {
        dma_dev->dma_chan[i + dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id].dma_data_type =
            DEVDRV_DMA_DATA_TRAFFIC;
    }
}

void devdrv_traffic_and_manage_dma_chan_config(struct devdrv_dma_dev *dma_dev)
{
    u32 left_num;
    u32 traffic_chan_num = 3;
    u32 manage_chan_num = 1;

    /* data dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id =
        dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_start_id +
        dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_num;
    if (dma_dev->remote_chan_num <= dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id) {
        dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id = 0;
    }
    left_num = dma_dev->remote_chan_num - dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id;
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num =
        left_num < traffic_chan_num ? left_num : traffic_chan_num;
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].last_use_chan =
        dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id;

    /* manage dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_MANAGE].chan_start_id =
        dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id +
        dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num;
    dma_dev->data_chan[DEVDRV_DMA_DATA_MANAGE].chan_num = manage_chan_num;
    if (dma_dev->remote_chan_num <= dma_dev->data_chan[DEVDRV_DMA_DATA_MANAGE].chan_start_id) {
        dma_dev->data_chan[DEVDRV_DMA_DATA_MANAGE].chan_start_id =
            dma_dev->remote_chan_num - manage_chan_num;
    }
    dma_dev->data_chan[DEVDRV_DMA_DATA_MANAGE].last_use_chan =
        dma_dev->data_chan[DEVDRV_DMA_DATA_MANAGE].chan_start_id;
}

void devdrv_res_dma_traffic(struct devdrv_dma_dev *dma_dev)
{
    /* init data type to dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_start_id = 0;
    dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_num = DEVDRV_DMA_DATA_COMM_CHAN_NUM;
    dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].last_use_chan = dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_start_id;

    /* msg dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_start_id =
        dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_start_id + dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_num;
    dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_num = DEVDRV_DMA_DATA_PCIE_MSG_CHAN_NUM;
    if (dma_dev->remote_chan_num <= DEVDRV_DMA_DATA_COMM_CHAN_NUM + DEVDRV_DMA_DATA_PCIE_MSG_CHAN_NUM) {
        dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_start_id = 0;
    }
    dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].last_use_chan =
        dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_start_id;

#if defined(CFG_SOC_PLATFORM_MDC_V11) || defined(CFG_SOC_PLATFORM_MDC_V51_LITE)
    devdrv_traffic_and_manage_dma_chan_config(dma_dev);
#else
    /* traffic dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id =
        dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_start_id +
        dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_num;
    if (dma_dev->remote_chan_num <= dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id) {
        dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id = 0;
    }
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num =
        dma_dev->remote_chan_num - dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id;
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].last_use_chan =
        dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id;
#endif
    devdrv_dma_chan_data_type_init(dma_dev);
}

void devdrv_sriov_pf_dma_traffic(struct devdrv_dma_dev *dma_dev)
{
    /* init data type to dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_start_id = 0;
    dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].chan_num = DEVDRV_DMA_DATA_COMM_CHAN_NUM;
    dma_dev->data_chan[DEVDRV_DMA_DATA_COMMON].last_use_chan = 0;

    /* msg dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_start_id = 0;
    dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_num = DEVDRV_DMA_DATA_PCIE_MSG_CHAN_NUM;
    dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].last_use_chan = 0;

    /* traffic dma chan map */
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id = 0;
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num = 1;
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].last_use_chan = 0;

    devdrv_dma_chan_data_type_init(dma_dev);
}

STATIC void devdrv_dma_irq_clear(struct devdrv_dma_dev *dma_dev)
{
    u32 i;
    struct devdrv_dma_channel *dma_chan = NULL;

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        dma_chan = &dma_dev->dma_chan[i];
        dma_chan->done_irq = -1;
        dma_chan->err_irq = -1;
        dma_chan->done_irq_state = DEVDRV_IRQ_IS_UNINIT;
        dma_chan->err_irq_state = DEVDRV_IRQ_IS_UNINIT;
    }
    return;
}

STATIC void devdrv_dma_guard_work_sched(struct work_struct *p_work)
{
    struct devdrv_dma_guard_work *guard_work = container_of(p_work, struct devdrv_dma_guard_work, dma_guard_work.work);
    struct devdrv_dma_dev *dma_dev = guard_work->dma_dev;
    u32 device_status;

    if (dma_dev->pci_ctrl == NULL) {
        devdrv_warn("pci_ctrl is null. (dev_id=%u)\n", dma_dev->dev_id);
        return;
    }

    device_status = dma_dev->pci_ctrl->device_status;
    if ((device_status != DEVDRV_DEVICE_ALIVE) && (device_status != DEVDRV_DEVICE_SUSPEND)) {
        devdrv_warn("Device is abnormal, can't dma copy. (dev_id=%u)\n", dma_dev->dev_id);
        return;
    }

    if (dma_dev->dev_status != DEVDRV_DMA_ALIVE) {
        devdrv_warn("Dma dev disable. (dev_id=%u)\n", dma_dev->dev_id);
        return;
    }

    devdrv_dma_done_guard_work_sched(dma_dev);
    if (dma_dev->guard_work.work_magic == DEVDRV_DMA_GUARD_WORK_MAGIC) {
        schedule_delayed_work(&dma_dev->guard_work.dma_guard_work, msecs_to_jiffies(DEVDRV_DMA_DONE_GUARD_WORK_DELAY));
    }
}

struct devdrv_dma_dev *devdrv_dma_init(struct devdrv_dma_func_para *para_in, u32 sq_cq_side, u32 func_id)
{
    struct devdrv_dma_dev *dma_dev = NULL;
    u32 chan_id;
    u32 i;

    /* 1 check dlcmsm */
    if (devdrv_check_dl_dlcmsm_state(para_in->drvdata) != 0) {
        devdrv_err("Check dlcmsm state failed.\n");
        return NULL;
    }

    /* 2 create dma dev */
    dma_dev = (struct devdrv_dma_dev *)devdrv_kzalloc(sizeof(struct devdrv_dma_dev), GFP_KERNEL);
    if (dma_dev == NULL) {
        devdrv_err("dma_dev alloc failed. (dev_id=%u)\n", para_in->dev_id);
        return NULL;
    }

    dma_dev->dma_chan = devdrv_kzalloc(sizeof(struct devdrv_dma_channel) * para_in->chan_num, GFP_KERNEL);
    if (dma_dev->dma_chan == NULL) {
        devdrv_err("dma_chan alloc failed. (dev_id=%u; chan_num=%d)\n", para_in->dev_id, para_in->chan_num);
        devdrv_kfree(dma_dev);
        dma_dev = NULL;
        return NULL;
    }

    /* 3 init dma dev */
    dma_dev->dev = para_in->dev;
    dma_dev->io_base = para_in->io_base;
    dma_dev->dma_chan_base = para_in->dma_chan_base;
    dma_dev->drvdata = para_in->drvdata;
    dma_dev->sq_cq_side = sq_cq_side;
    dma_dev->remote_chan_num = para_in->chan_num;
    dma_dev->remote_chan_begin = para_in->dma_chan_begin;
    dma_dev->remote_bar_begin = para_in->chan_begin;
    dma_dev->dev_id = para_in->dev_id;
    dma_dev->func_id = func_id;
    dma_dev->dma_pf_num = para_in->dma_pf_num;
    dma_dev->dma_vf_en = para_in->dma_vf_en;
    dma_dev->dma_vf_num = para_in->dma_vf_num;
    dma_dev->dev_status = DEVDRV_DMA_ALIVE;

    devdrv_dma_irq_clear(dma_dev);
    devdrv_dma_ops_init(dma_dev, para_in->chip_type);

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        chan_id = para_in->use_chan[i];
        devdrv_debug("Get chan_id. (dev_id=%d; fun=%d; dma_chan=%d)\n",
                     dma_dev->dev_id, func_id, chan_id);
        dma_dev->dma_chan[i].done_irq = (int)(para_in->done_irq_base + i); /* host pf/vf use own msi-x */
        dma_dev->dma_chan[i].err_irq = (int)(para_in->err_irq_base + i);   /* host pf/vf use own msi-x */
        dma_dev->dma_chan[i].err_irq_flag = (int)para_in->err_flag;
        spin_lock_init(&dma_dev->dma_chan[i].lock);
        spin_lock_init(&dma_dev->dma_chan[i].cq_lock);
        mutex_init(&dma_dev->dma_chan[i].vm_sq_lock);
        mutex_init(&dma_dev->dma_chan[i].vm_cq_lock);
        if (devdrv_dma_init_chan(dma_dev, i, (chan_id + dma_dev->remote_chan_begin), DEVDRV_SRIOV_DISABLE) != 0) {
            devdrv_err("Dma chan init failed. (dev_id=%u; chan_id=%u)\n", para_in->dev_id, i);
            devdrv_dma_exit(dma_dev, DEVDRV_SRIOV_DISABLE);
            return NULL;
        }
    }

    devdrv_res_dma_traffic(dma_dev);
    /* DMA guard work */
    if ((devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_PHY_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_SRIOV_VF_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_MDEV_VF_VM_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        INIT_DELAYED_WORK(&dma_dev->guard_work.dma_guard_work, devdrv_dma_guard_work_sched);
        dma_dev->guard_work.work_magic = DEVDRV_DMA_GUARD_WORK_MAGIC;
        dma_dev->guard_work.dma_dev = dma_dev;
        schedule_delayed_work(&dma_dev->guard_work.dma_guard_work, 0);
    }

    tasklet_init(&dma_dev->single_fault_task, devdrv_dma_stop_business, (uintptr_t)dma_dev);
    return dma_dev;
}

void devdrv_dma_exit(struct devdrv_dma_dev *dma_dev, u32 sriov_flag)
{
    u32 i;
    struct devdrv_dma_channel *dma_chan = NULL;

    if (dma_dev == NULL) {
        return;
    }

    tasklet_kill(&dma_dev->single_fault_task);
    if ((devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_PHY_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_SRIOV_VF_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_MDEV_VF_VM_BOOT) ||
        (devdrv_get_env_boot_type(dma_dev->dev_id) == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        if ((dma_dev->guard_work.dma_guard_work.work.func != NULL) &&
            (dma_dev->guard_work.work_magic == DEVDRV_DMA_GUARD_WORK_MAGIC)) {
            cancel_delayed_work_sync(&dma_dev->guard_work.dma_guard_work);
        }
        dma_dev->guard_work.dma_dev = NULL;
        dma_dev->guard_work.work_magic = 0;
    }

    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        if ((sriov_flag == DEVDRV_SRIOV_ENABLE) && (dma_dev->func_id == 0) && (i == 0)) {
            continue;
        }
        dma_chan = &dma_dev->dma_chan[i];

        /* chan has not init */
        if (dma_chan->dev == NULL) {
            continue;
        }

        if (dma_chan->err_irq_state == DEVDRV_IRQ_IS_INIT) {
            dma_chan->err_irq_state = DEVDRV_IRQ_IS_UNINIT;
            (void)devdrv_unregister_irq_by_vector_index(dma_dev->dev_id, dma_chan->err_irq, dma_chan);
            (void)cancel_work_sync(&dma_chan->err_work);
        }

        if (dma_chan->done_irq_state == DEVDRV_IRQ_IS_INIT) {
            dma_chan->done_irq_state = DEVDRV_IRQ_IS_UNINIT;
            devdrv_dma_update_msix_entry_offset(dma_dev->drvdata, &dma_chan->done_irq, 0);
            (void)devdrv_unregister_irq_by_vector_index(dma_dev->dev_id, dma_chan->done_irq, dma_chan);
            if ((devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == true) && (dma_chan->dma_done_workqueue != NULL)) {
                destroy_workqueue(dma_chan->dma_done_workqueue);
            }
            tasklet_kill(&dma_chan->dma_done_task);
        }

        if (devdrv_is_mdev_pm_boot_mode(dma_dev->dev_id) == false) {
            (void)devdrv_dma_chan_reset(dma_chan, sriov_flag);
        }

        devdrv_vm_free_dma_sq_cq(dma_chan);
        devdrv_free_dma_sq_cq(dma_chan);

        if ((dma_chan->sq_submit != NULL) && (devdrv_is_mdev_vm_boot_mode(dma_dev->dev_id) == true)) {
            devdrv_kfree(dma_chan->sq_submit);
            dma_chan->sq_submit = NULL;
        }
    }

    if (sriov_flag == DEVDRV_SRIOV_DISABLE) {
        devdrv_kfree(dma_dev->dma_chan);
        dma_dev->dma_chan = NULL;
        devdrv_kfree(dma_dev);
        dma_dev = NULL;
    }
}

void devdrv_dma_stop_business(unsigned long data)
{
    struct devdrv_dma_dev *dma_dev = (struct devdrv_dma_dev *)((uintptr_t)data);
    struct devdrv_dma_channel *dma_chan = NULL;
    u32 i;

    if (dma_dev == NULL) {
        return;
    }

    // stop guard work
    if ((dma_dev->guard_work.dma_guard_work.work.func != NULL) &&
        (dma_dev->guard_work.work_magic == DEVDRV_DMA_GUARD_WORK_MAGIC)) {
        cancel_delayed_work(&dma_dev->guard_work.dma_guard_work);
    }
    dma_dev->guard_work.work_magic = 0;

    // stop processing & in-coming dma work
    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        dma_chan = &dma_dev->dma_chan[i];

        /* chan has not init */
        if (dma_chan->dev == NULL) {
            continue;
        }
        devdrv_set_dma_chan_status(dma_chan, DEVDRV_DMA_CHAN_DISABLED);
    }
}

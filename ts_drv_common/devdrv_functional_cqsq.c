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

#include <securec.h>
#include "ka_common_pub.h"
#include "ka_barrier_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "devdrv_driver_pm.h"
#ifndef CFG_MANAGER_HOST_ENV
#include "devdrv_parse_pdata.h"
#endif /* CFG_MANAGER_HOST_ENV */
#include "tsdrv_drvops.h"
#include "tsdrv_device.h"
#include "devdrv_manager_comm.h"
#include "tsdrv_osal_intr.h"
#include "kernel_cgroup_mem_adapt.h"
#include "ascend_kernel_hal.h"

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2)
#include "devdrv_manager_common.h"
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0) && !defined(AOS_LLVM_BUILD)
static inline void getnstimeofday64(ka_timespec64_t *tv)
{
    ka_system_ktime_get_real_ts64(tv);
}

#endif
#ifdef  CFG_SOC_PLATFORM_MINIV2
#define DEVDRV_CQSQ_RESERVED_MEM_ADDR (0x2C381000) /* reserve mem: start addr + offset */
#define DEVDRV_CQSQ_RESERVED_MEM_END (0x2C981000 - 0x20000) /* 0x20000 reserved for callback cq buffer */
#define DEVDRV_CQSQ_MAX_SIZE (128 * 1024)

STATIC int devdrv_get_cqsq_reserved_mem(u32 devid, u32 size, u64 *rsv_addr)
{
    static u64 malloc_addr[TSDRV_MAX_DAVINCI_NUM] = {DEVDRV_CQSQ_RESERVED_MEM_ADDR, DEVDRV_CQSQ_RESERVED_MEM_ADDR};
    u64 rsv_malloc_addr;
    struct tsdrv_device *tsdrv_dev = NULL;

    tsdrv_dev = tsdrv_get_dev(devid);
    if (tsdrv_dev == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Invalid tsdrv dev. (devid=%u)\n", devid);
        return -ENOMEM;
#endif
    }
    if ((rsv_addr == NULL) || (size > DEVDRV_CQSQ_MAX_SIZE)) {
        TSDRV_PRINT_ERR("invalid input para.\n");
        return -EINVAL;
    }
    /* The heartbeat and log modules are concurrent in the function sqcq allocation */
    ka_task_mutex_lock(&tsdrv_dev->cqsq_mem_lock);
    rsv_malloc_addr = malloc_addr[devid] + size;
    if (rsv_malloc_addr < DEVDRV_CQSQ_RESERVED_MEM_END) {
        *rsv_addr = malloc_addr[devid] + ((u64)devid * CHIP_BASEADDR_PA_OFFSET);
        malloc_addr[devid] += size;
        ka_task_mutex_unlock(&tsdrv_dev->cqsq_mem_lock);
    } else {
#ifndef TSDRV_UT
        ka_task_mutex_unlock(&tsdrv_dev->cqsq_mem_lock);
        TSDRV_PRINT_ERR("devdrv cqsq reserved memory have no space!\n");
#endif
        return -ENOMEM;
    }

    return 0;
}
#endif

static inline void devdrv_functional_free_sqcq_memory(u8 *addr, unsigned long size)
{
    if (addr != NULL) {
#ifndef CFG_SOC_PLATFORM_MINIV2
        ka_free_pages_ex((unsigned long)addr, (u32)ka_mm_get_order(size));
#else
        ka_mm_iounmap(addr);
#endif
    }
}

STATIC void devdrv_functional_sq_dfx_show(struct devdrv_functional_sq_dfx_info *sq_dfx_info)
{
    u32 i;
    for (i = 0; i < SQ_DFX_RECORD_COUNT; i++) {
        TSDRV_PRINT_INFO("time[%llu.%llu]: head[%u]\n", (u64)sq_dfx_info->head_timestamp[i].tv_sec,
            (u64)sq_dfx_info->head_timestamp[i].tv_nsec, sq_dfx_info->head[i]);
    }

    for (i = 0; i < SQ_DFX_RECORD_COUNT; i++) {
        TSDRV_PRINT_INFO("time[%llu.%llu]: tail[%u]\n", (u64)sq_dfx_info->tail_timestamp[i].tv_sec,
            (u64)sq_dfx_info->tail_timestamp[i].tv_nsec, sq_dfx_info->tail[i]);
    }
}

STATIC void devdrv_functional_sq_dfx_record_head(struct devdrv_functional_sq_info *sq_info)
{
    u32 sq_dfx_head_count;

    sq_dfx_head_count = sq_info->sq_dfx_info.head_count;
    sq_info->sq_dfx_info.head[sq_dfx_head_count] = sq_info->head;
    getnstimeofday64(&sq_info->sq_dfx_info.head_timestamp[sq_dfx_head_count]);
    sq_dfx_head_count = (sq_dfx_head_count + 1) % SQ_DFX_RECORD_COUNT;
    sq_info->sq_dfx_info.head_count = sq_dfx_head_count;
}

STATIC void devdrv_functional_sq_dfx_record_tail(struct devdrv_functional_sq_info *sq_info)
{
    u32 sq_dfx_tail_count;

    sq_dfx_tail_count = sq_info->sq_dfx_info.tail_count;
    sq_info->sq_dfx_info.tail[sq_dfx_tail_count] = sq_info->tail;
    getnstimeofday64(&sq_info->sq_dfx_info.tail_timestamp[sq_dfx_tail_count]);
    sq_dfx_tail_count = (sq_dfx_tail_count + 1) % SQ_DFX_RECORD_COUNT;
    sq_info->sq_dfx_info.tail_count = sq_dfx_tail_count;
}

STATIC void devdrv_flush_cache_check_null(u64 base, u32 len)
{
    if (tsdrv_get_drv_ops()->flush_cache != NULL) {
        tsdrv_get_drv_ops()->flush_cache(base, len);
    }
}

#define DEVDRV_FUNC_WORK_CONSUME_MAX 1000
STATIC void devdrv_functional_work(ka_work_struct_t *work)
{
#ifndef TSDRV_UT
    struct devdrv_functional_cq_report *report = NULL;
    struct devdrv_functional_sq_info *sq_info = NULL;
    struct devdrv_functional_cq_info *cq_info = NULL;
    struct devdrv_functional_cqsq *cqsq = NULL;
    u8 *sq_slot = NULL;
    u8 *cq_slot = NULL;
    u8 *cq_data = NULL;
    u32 sq_slot_index;
    u32 delay_time;
    u32 sq_index;
    u32 len;

    cq_info = ka_container_of(work, struct devdrv_functional_cq_info, work);
    cqsq = &cq_info->ts_resource->functional_cqsq;

    delay_time = (u32)ka_system_jiffies_to_msecs((u64)jiffies - cq_info->start_timestamp);
    if (delay_time >= DEVDRV_FUNC_WORK_CONSUME_MAX) {
        TSDRV_PRINT_WARN("dev_id=%u, consume too much time, cq_id=%u, time(%u).\n", cq_info->devid, cq_info->index,
            delay_time);
    }

    ka_task_mutex_lock(&cq_info->lock);
    if (cq_info->addr == NULL) {
        ka_task_mutex_unlock(&cq_info->lock);
        return;
    }
    cq_slot = cq_info->addr + ((unsigned long)cq_info->tail * cq_info->slot_len);
    len = cq_info->slot_len * DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH;

    if (tsdrv_get_drv_ops()->flush_cache != NULL) {
        tsdrv_get_drv_ops()->flush_cache((u64)(uintptr_t)cq_info->addr, len);
    }
    while (cq_slot[0] == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        if (cq_info->type == DEVDRV_FUNCTIONAL_DETAILED_CQ) {
            report = (struct devdrv_functional_cq_report *)cq_slot;
            sq_index = report->sq_index;
            if ((sq_index >= DEVDRV_MAX_FUNCTIONAL_SQ_NUM) || (report->sq_head >= DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH)) {
                ka_task_mutex_unlock(&cq_info->lock);
                TSDRV_PRINT_ERR("index(%u) sq_head(%u).\n", sq_index, report->sq_head);
                return;
            }
            sq_info = &cqsq->sq_info[sq_index];

            sq_slot_index = (report->sq_head == 0) ? (DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH - 1) : (report->sq_head - 1);

            sq_slot = sq_info->addr + ((unsigned long)sq_slot_index * sq_info->slot_len);
            cq_data = cq_slot + DEVDRV_FUNCTIONAL_DETAILED_CQ_OFFSET;
            TSDRV_PRINT_DEBUG("call cq callback, cq id: %u.\n", cq_info->index);

            if (cq_info->callback != NULL) {
                cq_info->callback(cq_info->devid, cq_info->tsid, cq_data, sq_slot);
            }
            sq_info->head = report->sq_head;
#if FUNCTIONAL_SQ_DFX_SUPPORT
            devdrv_functional_sq_dfx_record_head(sq_info);
#endif
        } else {
            cq_data = cq_slot + DEVDRV_FUNCTIONAL_BRIEF_CQ_OFFSET;
            TSDRV_PRINT_DEBUG("call cq callback, cq id: %u.\n", cq_info->index);

            if (cq_info->callback != NULL) {
                cq_info->callback(cq_info->devid, cq_info->tsid, cq_data, NULL);
            }
        }

        if (cq_info->tail >= DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH - 1) {
            cq_info->phase = (cq_info->phase == DEVDRV_FUNCTIONAL_PHASE_ONE) ? DEVDRV_FUNCTIONAL_PHASE_ZERO :
                                                                               DEVDRV_FUNCTIONAL_PHASE_ONE;
            cq_info->tail = 0;
        } else {
            cq_info->tail++;
        }
        cq_slot = cq_info->addr + (unsigned long)cq_info->tail * cq_info->slot_len;
    }

    cq_info->head = cq_info->tail;
    ka_mm_writel(cq_info->tail, (void __iomem *)cq_info->doorbell);
    ka_task_mutex_unlock(&cq_info->lock);
#endif
}

STATIC void devdrv_heart_beat_cq_callback(struct devdrv_functional_cq_info *cq_info)
{
#ifndef TSDRV_UT
    struct devdrv_functional_cq_report *report = NULL;
    struct devdrv_functional_sq_info *sq_info = NULL;
    struct devdrv_functional_cqsq *cqsq = NULL;
    u32 sq_slot_index;
    u32 sq_index;
    u8 *sq_slot = NULL;
    u8 *cq_slot = NULL;
    u8 *cq_data = NULL;
    TSDRV_PRINT_DEBUG("heart beat start .\n");
    cqsq = &cq_info->ts_resource->functional_cqsq;
    cq_slot = cq_info->addr + (unsigned long)cq_info->tail * cq_info->slot_len;

    while (cq_slot[0] == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        report = (struct devdrv_functional_cq_report *)cq_slot;

        sq_index = report->sq_index;
        if (sq_index >= DEVDRV_MAX_FUNCTIONAL_SQ_NUM) {
            return;
        }
        sq_info = &cqsq->sq_info[sq_index];

        sq_slot_index = (report->sq_head == 0) ? (DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH - 1) : (report->sq_head - 1);

        sq_slot = sq_info->addr + (unsigned long)sq_slot_index * sq_info->slot_len;

        cq_data = cq_slot + DEVDRV_FUNCTIONAL_DETAILED_CQ_OFFSET;

        if (cq_info->callback != NULL) {
            cq_info->callback(cq_info->devid, cq_info->tsid, cq_data, sq_slot);
        }
        sq_info->head = report->sq_head;

#if FUNCTIONAL_SQ_DFX_SUPPORT
        devdrv_functional_sq_dfx_record_head(sq_info);
#endif

        if (cq_info->tail >= (DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH - 1)) {
            cq_info->phase = (cq_info->phase == DEVDRV_FUNCTIONAL_PHASE_ONE) ? DEVDRV_FUNCTIONAL_PHASE_ZERO :
                                                                               DEVDRV_FUNCTIONAL_PHASE_ONE;
            cq_info->tail = 0;
        } else {
            cq_info->tail++;
        }

        cq_slot = cq_info->addr + ((unsigned long)cq_info->tail * cq_info->slot_len);
        if (tsdrv_get_drv_ops()->flush_cache != NULL) {
            tsdrv_get_drv_ops()->flush_cache((u64)(uintptr_t)cq_slot, cq_info->slot_len);
        }
    }

    cq_info->head = cq_info->tail;
    ka_mm_writel(cq_info->tail, (void __iomem *)cq_info->doorbell);
#endif /* TSDRV_UT */
}

STATIC void devdrv_cq_callback_handle(struct devdrv_functional_cq_info *cq_info)
{
#ifndef TSDRV_UT
    u8 *cq_data = NULL;
    u8 *cq_slot = NULL;

    cq_slot = cq_info->addr + ((unsigned long)cq_info->tail * cq_info->slot_len);
    while (cq_slot[0] == cq_info->phase) {
        /**
         * In the ARM environment:
         * 1.Out-of-order may cause the TS driver to read other members of the report structure (read instruction 1)
         *   to be executed before the phase comparison instruction (read instruction 2);
         * 2.The time when TS writes report is between [read instruction 1] and [read instruction 2];
         * This error timing may cause [Read instruction 1] to read the old value.
         */
        ka_rmb();
        cq_data = cq_slot + DEVDRV_FUNCTIONAL_BRIEF_CQ_OFFSET;

        if (cq_info->callback != NULL) {
            cq_info->callback(cq_info->devid, cq_info->tsid, cq_data, NULL);
        }
        if (cq_info->tail >= (DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH - 1)) {
            cq_info->phase = (cq_info->phase == DEVDRV_FUNCTIONAL_PHASE_ONE) ? DEVDRV_FUNCTIONAL_PHASE_ZERO :
                                                                               DEVDRV_FUNCTIONAL_PHASE_ONE;
            cq_info->tail = 0;
        } else {
            cq_info->tail++;
        }

        cq_slot = cq_info->addr + ((unsigned long)cq_info->tail * cq_info->slot_len);
        if (tsdrv_get_drv_ops()->flush_cache != NULL) {
            tsdrv_get_drv_ops()->flush_cache((u64)(uintptr_t)cq_slot, cq_info->slot_len);
        }
    }
    cq_info->head = cq_info->tail;
    ka_mm_writel(cq_info->tail, (void __iomem *)cq_info->doorbell);
#endif
}

#define REPORT_HS_CQ devdrv_heart_beat_cq_callback
STATIC void devdrv_get_functional_cq_report(struct devdrv_functional_cq_info *cq_info, const u8 *addr)
{
#ifndef TSDRV_UT
    if (cq_info->function != DEVDRV_CQSQ_HEART_BEAT) {
        TSDRV_PRINT_DEBUG("receive irq find one report, cq id: %d, addr: %pK, cq_info->type=%u.\n",
            cq_info->index, (void *)addr, cq_info->type);
        if (cq_info->type == DEVDRV_FUNCTIONAL_CALLBACK_HS_CQ) {
            /* used for profiling log cq-sq channel callback */
            devdrv_cq_callback_handle(cq_info);
            return;
        } else if (cq_info->type == DEVDRV_FUNCTIONAL_REPORT_HS_CQ) {
            /* used for profiling log cq-sq channel report */
            REPORT_HS_CQ(cq_info);
            return;
        } else {
            cq_info->start_timestamp = (u64)jiffies;
            (void)ka_task_queue_work(cq_info->wq, &cq_info->work);
            return;
        }
    }

    devdrv_heart_beat_cq_callback(cq_info);
#endif /* TSDRV_UT */
}

STATIC void devdrv_functional_cqsq_irq_handler(unsigned long data)
{
#ifndef TSDRV_UT
    struct devdrv_functional_int_context *int_context = NULL;
    struct devdrv_functional_cq_info *cq_info = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    unsigned long flags;
    u8 *addr = NULL;
    u32 get;

    TSDRV_PRINT_DEBUG("receive functional cq irq .\n");
    int_context = (struct devdrv_functional_int_context *)(uintptr_t)data;
    ka_task_spin_lock_irqsave(&int_context->spinlock, flags);
    get = 0;
    ka_list_for_each_safe(pos, n, &int_context->int_list_header) {
        cq_info = ka_list_entry(pos, struct devdrv_functional_cq_info, int_list_node);
        addr = cq_info->addr + ((unsigned long)cq_info->tail * cq_info->slot_len);
        if (tsdrv_get_env_type() == TSDRV_ENV_OFFLINE) {
            if (tsdrv_get_drv_ops()->flush_cache != NULL) {
                tsdrv_get_drv_ops()->flush_cache((u64)(uintptr_t)addr, cq_info->slot_len);
            }
        } else {
            ka_mm_dma_sync_single_for_cpu(cq_info->dev, cq_info->phy_addr, cq_info->slot_len * cq_info->depth,
                KA_DMA_FROM_DEVICE);
        }

        if (addr[0] == cq_info->phase) {
            devdrv_get_functional_cq_report(cq_info, addr);
            get++;
        }
    }
    ka_task_spin_unlock_irqrestore(&int_context->spinlock, flags);

    if (get == 0) {
        TSDRV_PRINT_DEBUG("receive irq but no report found.\n");
    }
#endif /* TSDRV_UT */
}

STATIC ka_irqreturn_t devdrv_function_cqsq_hwirq_proc(int irq, void *data)
{
    struct devdrv_functional_int_context *int_context = NULL;
    unsigned long flags;

    if (data == NULL) {
        TSDRV_PRINT_WARN("param is null .irq = %d.\n", irq);
        return KA_IRQ_NONE;
    }

    ka_task_local_irq_save(flags);
    int_context = (struct devdrv_functional_int_context *)data;
    ka_system_tasklet_schedule(&int_context->cqsq_tasklet);
    ka_task_local_irq_restore(flags);
    return KA_IRQ_HANDLED;
}

STATIC int devdrv_init_functional_cqsq(u32 devid, u32 tsid)
{
    struct devdrv_functional_sq_info *sq_info = NULL;
    struct devdrv_functional_cq_info *cq_info = NULL;
    struct devdrv_functional_cqsq *cqsq = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_cq_hwinfo *cq_hwinfo = NULL;
    struct tsdrv_db_hwinfo_t *cq_db_hwinfo = NULL;
    struct tsdrv_db_hwinfo_t *sq_bd_hwinfo = NULL;
    enum tsdrv_env_type env_type;
    int ret;
    int i, j;
#ifdef CFG_SOC_PLATFORM_HELPER
    u32 first_ccpu, last_ccpu;
#endif

    env_type = tsdrv_get_env_type();
    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);

    if (env_type == TSDRV_ENV_OFFLINE) {
        sq_bd_hwinfo = tsdrv_get_db_hwinfo_t(devid, tsid, TSDRV_OFFLINE_DFX_SQ_DB);
        cq_db_hwinfo = tsdrv_get_db_hwinfo_t(devid, tsid, TSDRV_OFFLINE_DFX_CQ_DB);
    } else {
        sq_bd_hwinfo = tsdrv_get_db_hwinfo_t(devid, tsid, TSDRV_ONLINE_DFX_SQ_DB);
        cq_db_hwinfo = tsdrv_get_db_hwinfo_t(devid, tsid, TSDRV_ONLINE_DFX_CQ_DB);
    }
    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);

    cqsq = &ts_resource->functional_cqsq;
    KA_INIT_LIST_HEAD(&cqsq->int_context.int_list_header);
    ka_task_spin_lock_init(&cqsq->int_context.spinlock);

    sq_info = tsdrv_kzalloc(sizeof(struct devdrv_functional_sq_info) * DEVDRV_MAX_FUNCTIONAL_SQ_NUM, KA_GFP_KERNEL);
    if (sq_info == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id = %u]:kmalloc failed.\n", devid);
        return -ENOMEM;
#endif
    }
    cq_info = tsdrv_kzalloc(sizeof(struct devdrv_functional_cq_info) * DEVDRV_MAX_FUNCTIONAL_CQ_NUM, KA_GFP_KERNEL);
    if (cq_info == NULL) {
#ifndef TSDRV_UT
        tsdrv_kfree(sq_info);
        sq_info = NULL;
        TSDRV_PRINT_ERR("[dev_id = %u]:kmalloc failed.\n", devid);
        return -ENOMEM;
#endif
    }

    for (i = 0; i < DEVDRV_MAX_FUNCTIONAL_SQ_NUM; i++) {
        sq_info[i].devid = devid;
        sq_info[i].index = i;
        sq_info[i].depth = DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH;
        sq_info[i].slot_len = 0;
        sq_info[i].addr = NULL;
        sq_info[i].head = 0;
        sq_info[i].tail = 0;
        sq_info[i].credit = DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH;
        sq_info[i].host_dma_addr = 0;
        sq_info[i].phy_addr = 0;
        sq_info[i].host_dma_buffer = NULL;

        /* use cq head's doorbell for functional sq */
        sq_info[i].doorbell = sq_bd_hwinfo->dbs + ((long)(unsigned)i * sq_bd_hwinfo->db_stride);
        sq_info[i].function = DEVDRV_MAX_CQSQ_FUNC;
    }

    for (i = 0; i < DEVDRV_MAX_FUNCTIONAL_CQ_NUM; i++) {
        cq_info[i].devid = devid;
        cq_info[i].index = i;
        cq_info[i].depth = DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH;
        cq_info[i].slot_len = 0;
        cq_info[i].type = 0;
        cq_info[i].ts_resource = cqsq->int_context.ts_resource;
        cq_info[i].wq = ka_task_create_workqueue("devdrv-cqsq-work");

        if (cq_info[i].wq == NULL) {
            for (j = 0; j < i; j++) {
                ka_task_destroy_workqueue(cq_info[j].wq);
            }
            tsdrv_kfree(sq_info);
            sq_info = NULL;
            tsdrv_kfree(cq_info);
            cq_info = NULL;
            TSDRV_PRINT_ERR("[dev_id = %u]:create_workqueue error.\n", devid);
            return -ENOMEM;
        }

        KA_TASK_INIT_WORK(&cq_info[i].work, devdrv_functional_work);
        ka_task_mutex_init(&cq_info[i].lock);

        cq_info[i].addr = NULL;
        cq_info[i].head = 0;
        cq_info[i].tail = 0;
        cq_info[i].phase = DEVDRV_FUNCTIONAL_PHASE_ONE;
        cq_info[i].doorbell = cq_db_hwinfo->dbs + ((long)(unsigned)i * cq_db_hwinfo->db_stride);
        cq_info[i].callback = NULL;
        cq_info[i].function = DEVDRV_MAX_CQSQ_FUNC;
        cq_info[i].start_timestamp = 0;
        cq_info[i].dev = tsdrv_get_dev_p(devid);
    }

    cqsq->sq_info = sq_info;
    cqsq->sq_num = DEVDRV_MAX_FUNCTIONAL_SQ_NUM;
    cqsq->cq_info = cq_info;
    cqsq->cq_num = DEVDRV_MAX_FUNCTIONAL_CQ_NUM;

    ka_system_tasklet_init(&cqsq->int_context.cqsq_tasklet, devdrv_functional_cqsq_irq_handler,
        (unsigned long)(uintptr_t)&cqsq->int_context);
    TSDRV_PRINT_DEBUG("devid(%u) tsid(%u) dfx_irq(%u)\n", devid, tsid, cq_hwinfo->dfx_cq_irq);

    /* The Modification is to adapt A500 */
    ret = tsdrv_request_irq(devid, cq_hwinfo->dfx_cq_request_irq, devdrv_function_cqsq_hwirq_proc,
        &cqsq->int_context, "devdrv-functional_cq");
    if (ret != 0) {
        cqsq->sq_info = NULL;
        cqsq->sq_num = 0;
        cqsq->cq_info = NULL;
        cqsq->cq_num = 0;
        ka_system_tasklet_kill(&cqsq->int_context.cqsq_tasklet);
        for (i = 0; i < DEVDRV_MAX_FUNCTIONAL_CQ_NUM; i++) {
            ka_task_destroy_workqueue(cq_info[i].wq);
        }
        tsdrv_kfree(sq_info);
        sq_info = NULL;
        tsdrv_kfree(cq_info);
        cq_info = NULL;
        TSDRV_PRINT_ERR("[dev_id = %u]:request_irq failed,ret:%d\n", devid, ret);
        return -EFAULT;
    }

#ifdef CFG_SOC_PLATFORM_HELPER
    ret = tsdrv_get_cpu_index_range(devid, &first_ccpu, &last_ccpu);
    if (ret != 0) {
#ifndef TSDRV_UT
        (void)tsdrv_unrequest_irq(devid, cq_hwinfo->dfx_cq_request_irq, &cqsq->int_context);
        cqsq->sq_info = NULL;
        cqsq->sq_num = 0;
        cqsq->cq_info = NULL;
        cqsq->cq_num = 0;
        ka_system_tasklet_kill(&cqsq->int_context.cqsq_tasklet);
        for (i = 0; i < DEVDRV_MAX_FUNCTIONAL_CQ_NUM; i++) {
            ka_task_destroy_workqueue(cq_info[i].wq);
        }
        tsdrv_kfree(sq_info);
        sq_info = NULL;
        tsdrv_kfree(cq_info);
        cq_info = NULL;
        TSDRV_PRINT_ERR("Failed to get ctrl cpu range. (devid=%u; ret=%d)\n", devid, ret);
        return -EINVAL;
#endif
    }
    tsdrv_irq_set_affinity_hint(devid, cq_hwinfo->dfx_cq_request_irq, first_ccpu, last_ccpu);
#endif

    ka_task_spin_lock_init(&cqsq->spinlock);
    return 0;
}

void devdrv_destroy_functional_cqsq(u32 devid, u32 tsid)
{
    struct tsdrv_cq_hwinfo *cq_hwinfo = NULL;
    struct devdrv_functional_cqsq *cqsq = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    int i;

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("device not exist");
        return;
#endif
    }

    cqsq = &ts_resource->functional_cqsq;

    if ((cqsq->sq_num < DEVDRV_MAX_FUNCTIONAL_SQ_NUM) || (cqsq->cq_num < DEVDRV_MAX_FUNCTIONAL_CQ_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id = %u]:some cq or sq still working, free first.\n", devid);
        return;
#endif
    }
    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);
    (void)ka_base_irq_set_affinity_hint((u32)cq_hwinfo->dfx_cq_irq, NULL);
    (void)tsdrv_unrequest_irq(devid, cq_hwinfo->dfx_cq_request_irq, &cqsq->int_context);
    ka_system_tasklet_kill(&cqsq->int_context.cqsq_tasklet);
    for (i = 0; i < DEVDRV_MAX_FUNCTIONAL_CQ_NUM; i++) {
        ka_task_mutex_destroy(&cqsq->cq_info[i].lock);
        ka_task_destroy_workqueue(cqsq->cq_info[i].wq);
    }

    tsdrv_kfree(cqsq->sq_info);
    tsdrv_kfree(cqsq->cq_info);

    cqsq->sq_info = NULL;
    cqsq->sq_num = 0;
    cqsq->cq_info = NULL;
    cqsq->cq_num = 0;
}

int tsdrv_dfx_cqsq_init(u32 devid, u32 tsnum)
{
    u32 tsid;
    int err;
    u32 i;

    for (tsid = 0; tsid < tsnum; tsid++) {
        err = devdrv_init_functional_cqsq(devid, tsid);
        if (err != 0) {
#ifndef TSDRV_UT
            goto err_dfx_cqsq_init;
#endif
        }
    }
    return 0;
#ifndef TSDRV_UT
err_dfx_cqsq_init:
    for (i = 0; i < tsid; i++) {
        devdrv_destroy_functional_cqsq(devid, tsid);
    }
    return -ENODEV;
#endif
}

void tsdrv_dfx_cqsq_exit(u32 devid, u32 tsnum)
{
    u32 tsid;

    for (tsid = 0; tsid < tsnum; tsid++) {
        devdrv_destroy_functional_cqsq(devid, tsid);
    }
}

STATIC int devdrv_get_functional_sq_memory(u32 devid, struct devdrv_functional_sq_info *sq_info, u32 size)
{
    enum tsdrv_env_type env_type;
#ifndef CFG_SOC_PLATFORM_MINIV2
    struct page *page = NULL;
#else
    struct tsdrv_sq_hwinfo *sq_hwinfo = NULL;
    u64 rsv_sq_addr;
    u32 tsid = 0;
    int ret;
#endif

    env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_OFFLINE) {
#ifndef CFG_SOC_PLATFORM_MINIV2
        page = ka_alloc_pages_node_ex(devdrv_manager_devid_to_nid(devid, (u32)DEVDRV_TS_NODE_DDR_MEM),
            __KA_GFP_ZERO | __KA_GFP_THISNODE | KA_GFP_HIGHUSER_MOVABLE, (u32)ka_mm_get_order(size));
        if (page == NULL) {
            TSDRV_PRINT_ERR("kzalloc failed.\n");
            return -ENOMEM;
        }
        sq_info->addr = (u8 *)ka_mm_page_to_virt(page);
        sq_info->phy_addr = (phys_addr_t)ka_mm_virt_to_phys((void *)sq_info->addr);
#else
        ret = devdrv_get_cqsq_reserved_mem(devid, size, &rsv_sq_addr);
        if (ret != 0) {
            TSDRV_PRINT_ERR("devdrv get sq reserved memory error!\n");
            return -ENOMEM;
        }

        sq_info->phy_addr = rsv_sq_addr;

        sq_info->addr = (void *)ka_mm_ioremap_wc(sq_info->phy_addr, size);
        if ((void *)sq_info->addr == NULL) {
            TSDRV_PRINT_ERR("ioremap sq addr failed\n");
            return -ENOMEM;
        }

        ret = memset_s(sq_info->addr, size, 0, size);
        if (ret != 0) {
#ifndef TSDRV_UT
            ka_mm_iounmap(sq_info->addr);
            TSDRV_PRINT_ERR("memset sq memory failed.\n");
#endif
            return -ENOMEM;
        }
#endif
    } else {
#ifdef CFG_SOC_PLATFORM_MINIV2
#define DFX_SQCQ_SQE_SIZE (128 * 1024)
        /* host sq reserve mem distribution: ts0 sq resv mem(32M) + ts1 sq resv mem(32M) */
        /* dc on host, ts1 sq resv mem not used for normal sqcq, it is use for host functional sqcq mem */
        sq_hwinfo = tsdrv_get_ts_sq_hwinfo(devid, tsid);
        sq_info->phy_addr = sq_hwinfo->paddr + DEVDRV_RESERVE_MEM_SIZE + sq_info->index * DFX_SQCQ_SQE_SIZE;
        sq_info->bar_addr = sq_hwinfo->bar_addr + DEVDRV_RESERVE_MEM_SIZE + sq_info->index * DFX_SQCQ_SQE_SIZE;
        sq_info->addr = (void *)ka_mm_ioremap(sq_info->bar_addr, size);
        if (sq_info->addr == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Ioremap fail. (devid=%u; sqid=%u)\n", devid, sq_info->index);
            return -ENOMEM;
#endif
        }
#else
        TSDRV_PRINT_ERR("devid(%u):invalid plat type(%d)\n", devid, (int)env_type);
        return -ENOMEM;
#endif
    }

    return 0;
}

int devdrv_create_functional_sq(u32 devid, u32 tsid, u32 slot_len, u32 *sq_index, u64 *addr)
{
    struct devdrv_functional_sq_info *sq_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u32 len;
    int ret;
    int i;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (sq_index == NULL) || (addr == NULL) || (tsid >= DEVDRV_MAX_TS_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid input argument,tsid :%u.\n", devid, tsid);
        return -EINVAL;
#endif
    }
    if ((slot_len == 0) || (slot_len > DEVDRV_FUNCTIONAl_MAX_SQ_SLOT_LEN)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid input argument.\n", devid);
        return -EINVAL;
#endif
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return -ENODEV;
#endif
    }

    if (ts_resource->functional_cqsq.sq_num <= 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id=%u]:no available sq num=%d.\n", devid, ts_resource->functional_cqsq.sq_num);
        return -ENOMEM;
#endif
    }

    sq_info = ts_resource->functional_cqsq.sq_info;

    ka_task_spin_lock(&ts_resource->functional_cqsq.spinlock);
    for (i = 0; i < DEVDRV_MAX_FUNCTIONAL_SQ_NUM; i++) {
        if (sq_info[i].addr != 0) {
            continue;
        }
        sq_info[i].addr = (void *)1;
        ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);

        len = DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH * slot_len;
        ret = devdrv_get_functional_sq_memory(devid, &sq_info[i], len);
        if (ret != 0) {
            sq_info[i].addr = NULL;
            return -ENOMEM;
        }

        ka_task_spin_lock(&ts_resource->functional_cqsq.spinlock);
        sq_info[i].depth = DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH;
        sq_info[i].slot_len = slot_len;
        // set sq doorbell zero ? *sq_info[i].doorbell
        ts_resource->functional_cqsq.sq_num--;

        *sq_index = sq_info[i].index;
        *addr = (unsigned long)sq_info[i].phy_addr;

        ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);

        TSDRV_PRINT_DEBUG("dev[%d] functional sq is created, sq id: %d.\n", devid, *sq_index);
        return 0;
    }
    ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);
    return -ENOMEM;
}
KA_EXPORT_SYMBOL_GPL(devdrv_create_functional_sq);

void devdrv_destroy_functional_sq(u32 devid, u32 tsid, u32 sq_index)
{
    struct devdrv_functional_sq_info *sq_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    u8 *sq_addr = NULL;
    unsigned long size;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (sq_index >= DEVDRV_MAX_FUNCTIONAL_SQ_NUM) ||
        (tsid >= DEVDRV_MAX_TS_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid argument sq_index:%u tsid:%u.\n", devid, sq_index, tsid);
        return;
#endif
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return;
#endif
    }

    sq_info = ts_resource->functional_cqsq.sq_info;
    ka_task_spin_lock(&ts_resource->functional_cqsq.spinlock);
    sq_addr = sq_info[sq_index].addr;
    if (sq_info[sq_index].addr != 0) {
        sq_info[sq_index].addr = NULL;
        size = sq_info[sq_index].slot_len * sq_info[sq_index].depth;
        sq_info[sq_index].slot_len = 0;
        sq_info[sq_index].head = 0;
        sq_info[sq_index].tail = 0;
        sq_info[sq_index].credit = DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH;
        sq_info[sq_index].function = DEVDRV_MAX_CQSQ_FUNC;
        sq_info[sq_index].phy_addr = 0;
        ts_resource->functional_cqsq.sq_num++;
    }
    ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);

    devdrv_functional_free_sqcq_memory(sq_addr, size);
}
KA_EXPORT_SYMBOL_GPL(devdrv_destroy_functional_sq);

int devdrv_functional_set_sq_func(u32 devid, u32 tsid, u32 sq_index, enum devdrv_cqsq_func function)
{
    struct devdrv_functional_sq_info *sq_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (sq_index >= DEVDRV_MAX_FUNCTIONAL_SQ_NUM) ||
        (function >= DEVDRV_MAX_CQSQ_FUNC) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid input argument.sq_index:%u, function:%d, tsid:%u\n",
            devid, sq_index, (int)function, tsid);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return -ENODEV;
    }
    sq_info = &ts_resource->functional_cqsq.sq_info[sq_index];
    if (sq_info->addr == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:sq is in the source pool, not use.\n", devid);
        return -EINVAL;
    }
    sq_info->function = function;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_functional_set_sq_func);

int devdrv_functional_set_cq_func(u32 devid, u32 tsid, u32 cq_index, enum devdrv_cqsq_func function)
{
    struct devdrv_functional_cq_info *cq_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (cq_index >= DEVDRV_MAX_FUNCTIONAL_CQ_NUM) ||
        (function >= DEVDRV_MAX_CQSQ_FUNC) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid input argument. cq_index:%u, funcion:%d, tsid:%u\n",
            devid, cq_index, (int)function, tsid);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return -ENODEV;
    }
    cq_info = &ts_resource->functional_cqsq.cq_info[cq_index];
    if (cq_info->addr == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:cq is in the source pool, not use.\n", devid);
        return -EINVAL;
    }
    cq_info->function = function;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_functional_set_cq_func);

STATIC int devdrv_get_functional_cq_memory(u32 devid, struct devdrv_functional_cq_info *cq_info, u32 size)
{
    struct tsdrv_device *tsdrv_dev = NULL;
    enum tsdrv_env_type env_type;
#ifndef CFG_SOC_PLATFORM_MINIV2
    struct page *page = NULL;
#else
    u64 rsv_cq_addr;
    int ret;
#endif

    tsdrv_dev = tsdrv_get_dev(devid);
    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_ERR("[dev_id = %u]:invalid tsdrv dev.\n", devid);
        return -ENOMEM;
    }

    env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_OFFLINE) {
#ifndef CFG_SOC_PLATFORM_MINIV2
        page = ka_alloc_pages_node_ex(devdrv_manager_devid_to_nid(devid, (u32)DEVDRV_TS_NODE_DDR_MEM),
            __KA_GFP_ZERO | __KA_GFP_THISNODE | KA_GFP_HIGHUSER_MOVABLE, (u32)ka_mm_get_order(size));
        if (page == NULL) {
            TSDRV_PRINT_ERR("[dev_id = %u]:kzalloc failed.\n", devid);
            return -ENOMEM;
        }
        cq_info->addr = (u8 *)(uintptr_t)ka_mm_page_to_virt(page);
        cq_info->phy_addr = (u64)ka_mm_virt_to_phys((void *)cq_info->addr);
#else
        ret = devdrv_get_cqsq_reserved_mem(devid, size, &rsv_cq_addr);
        if (ret != 0) {
            TSDRV_PRINT_ERR("devdrv get cqsq reserved memory error!\n");
            return -ENOMEM;
        }

        cq_info->phy_addr = rsv_cq_addr;
        cq_info->addr = (void *)ka_mm_ioremap_wc(cq_info->phy_addr, size);
        if ((void *)cq_info->addr == NULL) {
            TSDRV_PRINT_ERR("ioremap cq addr failed\n");
            return -ENOMEM;
        }

        ret = memset_s(cq_info->addr, size, 0, size);
        if (ret != 0) {
            TSDRV_PRINT_ERR("memset sq memory failed.\n");
            return -ENOMEM;
        }

#endif
    } else {
#ifndef TSDRV_UT
        cq_info->host_dma_buffer = dma_alloc_coherent(tsdrv_dev->dev, size, &cq_info->host_dma_addr, KA_GFP_KERNEL);
        if (cq_info->host_dma_buffer == NULL) {
            TSDRV_PRINT_ERR("dma_alloc_coherent failed. (devid=%u)\n", devid);
            cq_info->addr = NULL;
            cq_info->host_dma_addr = 0;
            cq_info->host_dma_buffer = NULL;
            return -ENOMEM;
        }
        cq_info->phy_addr = cq_info->host_dma_addr;
        cq_info->addr = (u8 *)(uintptr_t)cq_info->host_dma_buffer;
#endif
    }

    devdrv_flush_cache_check_null((u64)((uintptr_t)cq_info->addr), size);

    return 0;
}

int devdrv_create_functional_cq(u32 devid, u32 tsid, u32 slot_len, u32 cq_type,
    void (*callback)(u32 device_id, u32 tsid, const u8 *cq_slot, u8 *sq_slot), u32 *cq_index, u64 *addr)
{
    struct devdrv_functional_int_context *int_context = NULL;
    struct devdrv_functional_cq_info *cq_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    unsigned long flags;
    int ret;
    int len;
    int i;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (callback == NULL) || (cq_type > DEVDRV_FUNCTIONAL_REPORT_HS_CQ) ||
        (cq_index == NULL) || (addr == NULL) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid argument tsid:%u cq_type:%u\n", devid, tsid, cq_type);
        return -EINVAL;
    }
    if ((slot_len == 0) || (slot_len > DEVDRV_FUNCTIONAL_MAX_CQ_SLOT_LEN)) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid input argument.\n", devid);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return -ENODEV;
    }

    if (ts_resource->functional_cqsq.cq_num <= 0) {
        TSDRV_PRINT_ERR("[dev_id=%u]:no available cq num=%d.\n", devid, ts_resource->functional_cqsq.cq_num);
        return -ENOMEM;
    }

    cq_info = ts_resource->functional_cqsq.cq_info;

    ka_task_spin_lock(&ts_resource->functional_cqsq.spinlock);
    for (i = 0; i < DEVDRV_MAX_FUNCTIONAL_CQ_NUM; i++) {
        if (cq_info[i].addr != 0) {
            continue;
        }

        cq_info[i].addr = (void *)1;
        ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);
        len = DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH * slot_len;

        ret = devdrv_get_functional_cq_memory(devid, &cq_info[i], len);
        if (ret != 0) {
            return -ENOMEM;
        }

        ka_task_spin_lock(&ts_resource->functional_cqsq.spinlock);
        cq_info[i].depth = DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH;
        cq_info[i].slot_len = slot_len;
        cq_info[i].type = cq_type;
        cq_info[i].phase = DEVDRV_FUNCTIONAL_PHASE_ONE;
        cq_info[i].callback = callback;
        *cq_info[i].doorbell = 0;
        cq_info[i].tsid = tsid;

        ts_resource->functional_cqsq.cq_num--;
        *cq_index = cq_info[i].index;

        *addr = (unsigned long)cq_info[i].phy_addr;

        ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);
        int_context = &ts_resource->functional_cqsq.int_context;
        ka_task_spin_lock_irqsave(&int_context->spinlock, flags);
        ka_list_add(&cq_info[i].int_list_node, &int_context->int_list_header);
        ka_task_spin_unlock_irqrestore(&int_context->spinlock, flags);

        TSDRV_PRINT_DEBUG("dev[%d] functional cq is created, cq id: %d.\n", devid, *cq_index);
        return 0;
    }
#ifndef TSDRV_UT
    ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);
    return -ENOMEM;
#endif
}
KA_EXPORT_SYMBOL_GPL(devdrv_create_functional_cq);

void devdrv_destroy_functional_cq(u32 devid, u32 tsid, u32 cq_index)
{
    struct devdrv_functional_int_context *int_context = NULL;
    struct devdrv_functional_cq_info *cq_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct tsdrv_device *tsdrv_dev = NULL;
    enum tsdrv_env_type env_type;
    dma_addr_t host_dma_addr = 0;
    void *cq_addr = NULL;
    unsigned long flags, size;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (cq_index >= DEVDRV_MAX_FUNCTIONAL_CQ_NUM) ||
        (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid input argument tsid:%u.cq_index:%u\n", devid, tsid, cq_index);
        return;
    }

    tsdrv_dev = tsdrv_get_dev(devid);
    if (tsdrv_dev == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return;
    }
    cq_info = ts_resource->functional_cqsq.cq_info;
    ka_task_mutex_lock(&cq_info[cq_index].lock);
    cq_addr = cq_info[cq_index].addr;
    if (cq_info[cq_index].addr != 0) {
        int_context = &ts_resource->functional_cqsq.int_context;

        ka_task_spin_lock_irqsave(&int_context->spinlock, flags);
        ka_list_del(&cq_info[cq_index].int_list_node);
        ka_task_spin_unlock_irqrestore(&int_context->spinlock, flags);
        ka_task_spin_lock(&ts_resource->functional_cqsq.spinlock);

        env_type = tsdrv_get_env_type();
        if (env_type == TSDRV_ENV_OFFLINE) {
            cq_info[cq_index].addr = NULL;
        } else if (env_type == TSDRV_ENV_ONLINE) {
#ifndef TSDRV_UT
            host_dma_addr = cq_info[cq_index].host_dma_addr;
            cq_addr = cq_info[cq_index].host_dma_buffer;
            cq_info[cq_index].addr = NULL;
            cq_info[cq_index].host_dma_buffer = NULL;
            cq_info[cq_index].host_dma_addr = 0;
#endif
        } else {
#ifndef TSDRV_UT
            ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);
            ka_task_mutex_unlock(&cq_info[cq_index].lock);
            TSDRV_PRINT_ERR("[dev_id=%u]:invalid device type.\n", devid);
            return;
#endif
        }
        size = cq_info[cq_index].depth * cq_info[cq_index].slot_len;
        cq_info[cq_index].head = 0;
        cq_info[cq_index].tail = 0;
        cq_info[cq_index].depth = DEVDRV_MAX_FUNCTIONAL_CQ_DEPTH;
        cq_info[cq_index].slot_len = 0;
        cq_info[cq_index].type = 0;
        cq_info[cq_index].phase = DEVDRV_FUNCTIONAL_PHASE_ONE;
        cq_info[cq_index].callback = NULL;
        cq_info[cq_index].function = DEVDRV_MAX_CQSQ_FUNC;
        ts_resource->functional_cqsq.cq_num++;
        ka_task_spin_unlock(&ts_resource->functional_cqsq.spinlock);

        if (env_type == TSDRV_ENV_OFFLINE) {
            devdrv_functional_free_sqcq_memory(cq_addr, size);
        } else if (env_type == TSDRV_ENV_ONLINE) {
            dma_free_coherent(tsdrv_dev->dev, size, cq_addr, host_dma_addr);
        }
    }
    ka_task_mutex_unlock(&cq_info[cq_index].lock);
}
KA_EXPORT_SYMBOL_GPL(devdrv_destroy_functional_cq);

STATIC int devdrv_functional_send_sq_check(u32 devid, u32 tsid, u32 sq_index, const u8 *buffer, u32 buf_len)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (sq_index >= DEVDRV_MAX_FUNCTIONAL_SQ_NUM) || (buffer == NULL) ||
        (buf_len == 0) || (buf_len > DEVDRV_FUNCTIONAl_MAX_SQ_SLOT_LEN) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid argument.sq_index:%u, (buffer == NULL):%d, buf_len:%u, tsid:%u\n", devid,
            sq_index, (buffer == NULL), buf_len, tsid);
        return -EINVAL;
    }

    return 0;
}

int devdrv_functional_send_sq(u32 devid, u32 tsid, u32 sq_index, const u8 *buffer, u32 buf_len)
{
    struct devdrv_functional_sq_info *sq_info = NULL;
    struct tsdrv_ts_resource *ts_resource = NULL;
    enum tsdrv_env_type env_type;
    int credit;
    u8 *addr = NULL;
    u32 tail;
    int ret;

    ret = devdrv_functional_send_sq_check(devid, tsid, sq_index, buffer, buf_len);
    if (ret != 0) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid input argument.\n", devid);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    if (ts_resource == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:device not exist.\n", devid);
        return -ENODEV;
    }
    sq_info = ts_resource->functional_cqsq.sq_info;
    if (sq_info[sq_index].addr == NULL) {
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid sq.\n", devid);
        return -ENOMEM;
    }

    tail = sq_info[sq_index].tail;
    credit = (sq_info[sq_index].tail >= sq_info[sq_index].head) ?
        (DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH - (sq_info[sq_index].tail - sq_info[sq_index].head + 1)) :
        (sq_info[sq_index].head - sq_info[sq_index].tail - 1);
    if (credit <= 0) {
        TSDRV_PRINT_ERR("no available sq slot. devid(%u), tsid(%u), sq_index(%u), head(%u), tail(%u)\n",
            devid, tsid, sq_index, sq_info[sq_index].head, sq_info[sq_index].tail);
#if FUNCTIONAL_SQ_DFX_SUPPORT
        devdrv_functional_sq_dfx_show(&sq_info[sq_index].sq_dfx_info);
#endif
        return -ENOMEM;
    }

    addr = sq_info[sq_index].addr + ((unsigned long)tail * sq_info[sq_index].slot_len);

    env_type = tsdrv_get_env_type();
    if (env_type == TSDRV_ENV_OFFLINE) {
        ret = memcpy_s(addr, sq_info[sq_index].slot_len, buffer, buf_len);
        if (ret != 0) {
            TSDRV_PRINT_ERR("[dev_id=%u]:DEVICE: copy to addr failed, ret = %d.\n", devid, ret);
            return -EINVAL;
        }
    } else {
#ifdef CFG_SOC_PLATFORM_MINIV2
        ka_mm_memcpy_toio(addr, buffer, sq_info[sq_index].slot_len);
#else
        TSDRV_PRINT_ERR("[dev_id=%u]:invalid plat type.\n", devid);
        return -EINVAL;
#endif
    }

    devdrv_flush_cache_check_null((u64)(uintptr_t)addr, sq_info[sq_index].slot_len);

    tail = (tail >= (DEVDRV_MAX_FUNCTIONAL_SQ_DEPTH - 1)) ? (0) : (tail + 1);

    sq_info[sq_index].tail = tail;

#if FUNCTIONAL_SQ_DFX_SUPPORT
    devdrv_functional_sq_dfx_record_tail(&sq_info[sq_index]);
#endif

    ka_mm_writel((u32)tail, (void __iomem *)sq_info[sq_index].doorbell);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_functional_send_sq);

int devdrv_functional_sq_send(u32 devid, u32 tsid, u32 sqid, const u8 *sqe, u32 sqe_size)
{
    return devdrv_functional_send_sq(devid, tsid, sqid, sqe, sqe_size);
}
KA_EXPORT_SYMBOL_GPL(devdrv_functional_sq_send);

int devdrv_mailbox_send_cqsq(u32 devid, u32 tsid, struct devdrv_mailbox_cqsq *cqsq)
{
#ifndef TSDRV_UT
    struct tsdrv_ts_resource *ts_resource = NULL;
    struct devdrv_mailbox *mailbox = NULL;
    int result = -1;
    u32 len;
    int ret;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (cqsq == NULL) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("devid(%u), cqsq(%pK), tsid(%u), "
            "invalid input argument.\n",
            devid, cqsq, tsid);
        return -EINVAL;
    }

    ts_resource = tsdrv_get_ts_resoruce(devid, TSDRV_PM_FID, tsid);
    mailbox = &ts_resource->mailbox;
    len = sizeof(struct devdrv_mailbox_cqsq);

    TSDRV_PRINT_DEBUG("send functional cqsq to TS, cmdType: 0x%x, "
        "sq id: %d, sq addr: %pK,"
        "cq0 id: %d, cq0 addr: %pK,"
        "cq1 id: %d, cq1 addr: %pK,"
        "cq2 id: %d, cq2 addr: %pK.\n",
        cqsq->cmd_type, cqsq->sq_index, (void *)(uintptr_t)cqsq->sq_addr, cqsq->cq0_index,
        (void *)(uintptr_t)cqsq->cq0_addr, cqsq->cq1_index, (void *)(uintptr_t)cqsq->cq1_addr,
        cqsq->cq2_index, (void *)(uintptr_t)cqsq->cq2_addr);

    cqsq->plat_type = (u8)tsdrv_get_env_type();
    ret = devdrv_mailbox_kernel_sync_no_feedback(mailbox, (u8 *)cqsq, len, &result);
    if (ret == 0) {
        ret = result;
    }
    return ret;
#endif
}
KA_EXPORT_SYMBOL_GPL(devdrv_mailbox_send_cqsq);

static int devdrv_create_functional_sqcq_para_check(u32 devid, u32 tsid,
    struct devdrv_func_sqcq_alloc_para_in *para_in, struct devdrv_func_sqcq_alloc_para_out *para_out)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM) || (para_in == NULL) ||
        (para_out == NULL) || (para_in->callback == NULL)) {
        TSDRV_PRINT_ERR("Invalid para. (devid=%u; tsid=%u)\n", devid, tsid);
        return -EINVAL;
    }

    if ((para_in->sqe_size == 0) || (para_in->sqe_size > DEVDRV_MAX_FUNCTIONAL_SQE_SIZE) ||
        (para_in->cqe_size == 0) || (para_in->cqe_size > DEVDRV_MAX_FUNCTIONAL_CQE_SIZE) ||
        (para_in->type == 0) || (para_in->type >= FUNC_MBOX_CMD_TYPE_MAX)) {
        TSDRV_PRINT_ERR("Invalid para. (devid=%u; sqe_size=%u; cqe_size=%u; type=%u)\n", devid, para_in->sqe_size,
            para_in->cqe_size, para_in->type);
        return -EINVAL;
    }

    return 0;
}

int devdrv_create_functional_sqcq(u32 devid, u32 tsid, struct devdrv_func_sqcq_alloc_para_in *para_in,
    struct devdrv_func_sqcq_alloc_para_out *para_out)
{
    struct devdrv_mailbox_cqsq mailbox_cqsq = {0};
    u32 sq_0_index, cq_0_index;
    u64 sq_0_addr = 0;
    u64 cq_0_addr = 0;
    int ret;

    ret = devdrv_create_functional_sqcq_para_check(devid, tsid, para_in, para_out);
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_create_functional_sq(devid, tsid, para_in->sqe_size, &sq_0_index, (u64 *)&sq_0_addr);
    if (ret != 0) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("Failed to alloc func sq. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
#endif
    }

    ret = devdrv_create_functional_cq(devid, tsid, para_in->cqe_size, DEVDRV_FUNCTIONAL_DETAILED_CQ,
        para_in->callback, &cq_0_index, (u64 *)&cq_0_addr);
    if (ret != 0) {
        devdrv_destroy_functional_sq(devid, tsid, sq_0_index);
        TSDRV_PRINT_ERR("Failed to alloc func cq. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    mailbox_cqsq.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    mailbox_cqsq.cmd_type = para_in->type;
    mailbox_cqsq.result = 0;
    mailbox_cqsq.sq_index = sq_0_index;
    mailbox_cqsq.sq_addr = sq_0_addr;
    mailbox_cqsq.cq0_index = cq_0_index;
    mailbox_cqsq.cq0_addr = cq_0_addr;
    mailbox_cqsq.cq1_index = 0;
    mailbox_cqsq.cq1_addr = 0;
    mailbox_cqsq.cq2_index = 0;
    mailbox_cqsq.cq2_addr = 0;
    mailbox_cqsq.cq_irq = (u16)tsdrv_get_dfx_cq_irq_vector(devid, tsid);
    mailbox_cqsq.cq_slot_size = para_in->sqe_size;
    ret = devdrv_mailbox_send_cqsq(devid, tsid, &mailbox_cqsq);
    if (ret != 0) {
        devdrv_destroy_functional_cq(devid, tsid, cq_0_index);
        devdrv_destroy_functional_sq(devid, tsid, sq_0_index);
        TSDRV_PRINT_ERR("Failed to send mb. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    para_out->sq_id = sq_0_index;
    para_out->cq_id = cq_0_index;

    TSDRV_PRINT_INFO("Alloc func sqcq success. (devid=%u; sqid=%u; cqid=%u)\n", devid, sq_0_index, cq_0_index);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_create_functional_sqcq);

int devdrv_destroy_functional_sqcq(u32 devid, u32 tsid, struct devdrv_func_sqcq_free_para *para)
{
    struct devdrv_mailbox_cqsq mailbox_cqsq = {0};
    int ret;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM) || (para == NULL) ||
        (para->type == 0) || (para->type >= FUNC_MBOX_CMD_TYPE_MAX) ||
        (para->sq_id >= DEVDRV_MAX_FUNCTIONAL_SQ_NUM) || (para->cq_id >= DEVDRV_MAX_FUNCTIONAL_CQ_NUM)) {
        TSDRV_PRINT_ERR("Invalid para. (devid=%u; tsid=%u)\n", devid, tsid);
        return -EINVAL;
    }

    mailbox_cqsq.valid = DEVDRV_MAILBOX_MESSAGE_VALID;
    mailbox_cqsq.cmd_type = para->type;
    mailbox_cqsq.result = 0;
    mailbox_cqsq.sq_index = (u16)para->sq_id;
    mailbox_cqsq.cq0_index = (u16)para->cq_id;
    mailbox_cqsq.cq1_index = 0;
    mailbox_cqsq.cq2_index = 0;
    ret = devdrv_mailbox_send_cqsq(devid, tsid, &mailbox_cqsq);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Failed to send mailbox. (devid=%u; sqid=%u; cqid=%u; ret=%d)\n", devid, para->sq_id,
            para->cq_id, ret);
        return ret;
    }

    devdrv_destroy_functional_sq(devid, tsid, para->sq_id);
    devdrv_destroy_functional_cq(devid, tsid, para->cq_id);

    TSDRV_PRINT_INFO("Free func sqcq success. (devid=%u; sqid=%u; cqid=%u)\n", devid, para->sq_id, para->cq_id);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_destroy_functional_sqcq);


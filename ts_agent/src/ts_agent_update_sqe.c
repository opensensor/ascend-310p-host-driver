/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-09-23
 */

#include "securec.h"
#include "ts_agent_common.h"
#include "tsch/task_struct.h"
#include "tsch/mb_struct.h"
#include "ts_agent_log.h"
#include "ts_agent_dvpp.h"
#ifndef TS_AGENT_UT
#include "trs_adapt.h"
#include "devdrv_interface.h"
#endif
#include "ts_agent_update_sqe.h"
#include "dbl/uda.h"
#ifndef TS_AGENT_UT
#include "dbl/runenv_config.h"
#include "trs_shr_id_spod.h"
#include "ascend_kernel_hal.h"
#endif
typedef int (*sqe_hook_proc_t)(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe);
static sqe_hook_proc_t g_sqe_proc_fn[TS_STARS_SQE_TYPE_END] = {NULL};
static ts_agent_dvpp_ops_t g_dvpp_ops = {NULL};
long dma_cnt[TSAGENT_MAX_DEV_ID] = {0};
long dma_cnt_max[TSAGENT_MAX_DEV_ID] = {0};
long dma_cnt_max_last[TSAGENT_MAX_DEV_ID] = {0};
#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
void __iomem *g_warning_bit_addr[TSAGENT_MAX_DEV_ID] = {NULL};
#endif

const uint16_t TS_MASK_BIT0_BIT1 = 0x3U;
const uint16_t TS_MASK_BIT0_BIT11 = 0x0FFFU;
const uint16_t TS_MASK_BIT12 = 0x1000U; // the 12th bit indicate whether task_id is update.
const uint16_t TS_MASK_BIT13_BIT15 = 0xE000U;
const uint16_t TS_MASK_BIT12_BIT15 = 0xF000U;
const uint16_t TS_UPDATE_FOR_ALL = 0b10;
const uint16_t TS_UPDATE_FLAG_BIT = 12U;
const uint16_t TS_MASK_BIT14_BIT15 = 0xC000U;
const uint16_t TS_MASK_BIT0_BIT14 = 0x7FFFU;
const uint16_t TS_MASK_BIT15 = 0x8000U;
const uint16_t TS_UPDATE_FOR_STREAM_EXTEND = 0b1;
const uint16_t TS_UPDATE_FOR_STREAM_EXTEND_FLAG_BIT = 15U;

const uint64_t TS_AGENT_ASCEND910B1_DIE_ADDR_OFFSET =   0x10000000000ULL;
const uint64_t TS_AGENT_ASCEND910B1_CHIP_ADDR_OFFSET =  0x80000000000ULL;
const uint64_t TS_AGENT_CHIP_BASE_ADDR        =  0x200000000000ULL;
const uint64_t TS_AGENT_ASCEND910B1_HCCS_CHIP_ADDR_OFFSET =  0x20000000000ULL;
const uint16_t TS_AGENT_SQE_SIZE =  0x40U;
const uint16_t TS_CPU_KERNEL_TYPE_CUSTOM_KFC = 6U;

static uint16_t *g_stream_id_to_sq_id_map[TSAGENT_MAX_DEV_ID] = {NULL};

#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
struct tsagent_sq_base_info g_sq_base_info[TSAGENT_MAX_DEV_ID][TS_AGENT_MAX_SQ_NUM];
#endif

#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES) && (defined CFG_HOST_ENV)

struct tsagent_dev_base_info g_dev_base_info[TSAGENT_MAX_DEV_ID] = {{U32_MAX, U32_MAX, U32_MAX, U32_MAX}};

void tsagent_dev_base_info_init(void)
{
    int i;

    for (i = 0; i < TSAGENT_MAX_DEV_ID; i++) {
        g_dev_base_info[i].chip_id = U32_MAX;
        g_dev_base_info[i].die_id = U32_MAX;
        g_dev_base_info[i].addr_mode = U32_MAX;
        g_dev_base_info[i].soc_type = U32_MAX;
    }
}

struct tsagent_dev_base_info* tsagent_get_device_base_info(u32 dev_id)
{
    u32 chip_id = U32_MAX;
    u32 die_id = U32_MAX;
    HAL_KERNEL_ADDR_MODE addr_mode = ADDR_MODE_MAX;
    unsigned int soc_type = SOC_TYPE_MAX;
    int ret;
    u32 phy_dev_id = dev_id;
    struct uda_mia_dev_para mia_para;

    if (dev_id >= TSAGENT_MAX_DEV_ID) {
        return NULL;
    }

    if ((g_dev_base_info[dev_id].chip_id != U32_MAX) &&
        (g_dev_base_info[dev_id].die_id != U32_MAX) &&
        (g_dev_base_info[dev_id].addr_mode != U32_MAX) &&
        (g_dev_base_info[dev_id].soc_type != U32_MAX)) {
        return &g_dev_base_info[dev_id];     
    }

    if (uda_is_phy_dev(dev_id) == false) {
        ret = uda_udevid_to_mia_devid(dev_id, &mia_para);
        if (ret != EOK) {
            ts_agent_err("convert devid failed, devid=%u, ret=%d.", dev_id, ret);
            return NULL;
        }

        phy_dev_id = mia_para.phy_devid;
    }

    ret = hal_kernel_get_device_chip_die_id(phy_dev_id, &chip_id, &die_id);
    if (ret != EOK) {
        ts_agent_err("get chip and die id failed, phy_dev_id=%u, devid=%u, ret=%d.", phy_dev_id, dev_id, ret);
        return NULL;
    }

    ret = hal_kernel_get_device_addr_mode(phy_dev_id, &addr_mode);
    if (ret != EOK) {
        ts_agent_err("get addr mode failed, phy_dev_id=%u, devid=%u, ret=%d.", phy_dev_id, dev_id, ret);
        return NULL;
    }

    ret = hal_kernel_get_soc_type((unsigned int)phy_dev_id, &soc_type);
    if (ret != EOK) {
        ts_agent_err("get soc type failed, phy_dev_id=%u, devid=%u, ret=%d.", phy_dev_id, dev_id, ret);
        return NULL;
    }

    g_dev_base_info[dev_id].chip_id = chip_id;
    g_dev_base_info[dev_id].die_id = die_id;
    g_dev_base_info[dev_id].addr_mode = addr_mode;
    g_dev_base_info[dev_id].soc_type = soc_type;

    ts_agent_debug("get dev base info success, devid=%u, chip_id=%u, die_id=%u, addr_mode=%d, soc_type=%u.",
                   dev_id, chip_id, die_id, addr_mode, soc_type);

    return &g_dev_base_info[dev_id];
}

#else

void tsagent_dev_base_info_init(void)
{
    return;
}
#endif

#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
void tsagent_sq_base_info_init(void)
{
    int index_i;
    int index_j;

    for (index_i = 0; index_i < TSAGENT_MAX_DEV_ID; index_i++) {
        for (index_j = 0; index_j < TS_AGENT_MAX_SQ_NUM; index_j++) {
            g_sq_base_info[index_i][index_j].swsq_flag = false;
            g_sq_base_info[index_i][index_j].rsv = 0U;
        }
    }
}

void tsagent_sq_info_set(u32 devid, u32 stream_id, u16 sqid, bool swsq_flag)
{
    if ((devid >= TSAGENT_MAX_DEV_ID) || (sqid >= TS_AGENT_MAX_SQ_NUM)) {
        ts_agent_err("devid=%u, sqid=%u.", devid, sqid);
        return;
    }

    g_sq_base_info[devid][sqid].swsq_flag = swsq_flag;
    ts_agent_debug("devid=%u, stream_id=%u, sqid=%u, swsq_flag=%d.", devid, stream_id, sqid,
        swsq_flag);
}

void tsagent_sq_info_reset(u32 devid, u32 stream_id, u16 sqid)
{
    if ((devid >= TSAGENT_MAX_DEV_ID) || (sqid >= TS_AGENT_MAX_SQ_NUM)) {
        ts_agent_err("devid=%u, sqid=%u.", devid, sqid);
        return;
    }

    g_sq_base_info[devid][sqid].swsq_flag = false;
    g_sq_base_info[devid][sqid].rsv = 0U;
    ts_agent_debug("devid=%u, stream_id=%u, sqid=%u.", devid, stream_id, sqid);
}

bool tsagent_is_software_sq_version(u32 devid, u16 sqid)
{
    if ((devid >= TSAGENT_MAX_DEV_ID) || (sqid >= TS_AGENT_MAX_SQ_NUM)) {
        ts_agent_err("devid=%u, sqid=%u.", devid, sqid);
        return false;
    }

    return g_sq_base_info[devid][sqid].swsq_flag;
}
#else
void tsagent_sq_base_info_init(void)
{
    return;
}

void tsagent_sq_info_set(u32 devid, u32 stream_id, u16 sqid, bool swsq_flag)
{
    (void)devid;
    (void)stream_id;
    (void)sqid;
    (void)swsq_flag;
    return;
}

void tsagent_sq_info_reset(u32 devid, u32 stream_id, u16 sqid)
{
    (void)devid;
    (void)stream_id;
    (void)sqid;
    return;
}

bool tsagent_is_software_sq_version(u32 devid, u16 sqid)
{
    (void)devid;
    (void)sqid;
    return false;
}
#endif

int tsagent_stream_id_to_sq_id_init(void)
{
    int index_i;
    int index_j;
    int free_idx;
    const size_t size = TS_AGENT_MAX_STREAM_NUM * sizeof(uint16_t);

    for (index_i = 0; index_i < TSAGENT_MAX_DEV_ID; index_i++) {
        g_stream_id_to_sq_id_map[index_i] = NULL;
    }

    for (index_i = 0; index_i < TSAGENT_MAX_DEV_ID; index_i++) {
        g_stream_id_to_sq_id_map[index_i] = (uint16_t *)ka_mm_kzalloc(size, KA_GFP_KERNEL);
        if (g_stream_id_to_sq_id_map[index_i] != NULL) {
            for (index_j = 0; index_j < TS_AGENT_MAX_STREAM_NUM; index_j++) {
                g_stream_id_to_sq_id_map[index_i][index_j] = U16_MAX;
            }
        } else {
            for (free_idx = 0; free_idx < index_i; free_idx++) {
                ka_mm_kfree(g_stream_id_to_sq_id_map[free_idx]);
                g_stream_id_to_sq_id_map[free_idx] = NULL;
            }
            ts_agent_err("ka_mm_kzalloc failed, index_i=%d.", index_i);
            return -ENOMEM;
        }
    }

    return EOK;
}

void tsagent_stream_id_to_sq_id_uninit(void)
{
    int index_i;
 
    for (index_i = 0; index_i < TSAGENT_MAX_DEV_ID; index_i++) {
        if (g_stream_id_to_sq_id_map[index_i] != NULL) {
            ka_mm_kfree(g_stream_id_to_sq_id_map[index_i]);
            g_stream_id_to_sq_id_map[index_i] = NULL;
        }
    }
}

void tsagent_stream_id_to_sq_id_add(u32 devid, u32 stream_id, u16 sqid)
{
    if (stream_id == U32_MAX) return;

    if ((devid >= TSAGENT_MAX_DEV_ID) || (stream_id >= TS_AGENT_MAX_STREAM_NUM)) {
        ts_agent_err("devid=%u, stream_id=%u is invalid, sqid=%u.", devid, stream_id, sqid);
        return;
    }

    g_stream_id_to_sq_id_map[devid][stream_id] = sqid;
}

void tsagent_stream_id_to_sq_id_del(u32 devid, u32 stream_id, u16 sqid)
{
    if (stream_id == U32_MAX) return;

    if ((devid >= TSAGENT_MAX_DEV_ID) || (stream_id >= TS_AGENT_MAX_STREAM_NUM)) {
        ts_agent_err("devid=%u, stream_id=%u is invalid, sqid=%u.", devid, stream_id, sqid);
        return;
    }

    g_stream_id_to_sq_id_map[devid][stream_id] = U16_MAX;
}

bool tsagent_sq_is_belong_to_stream(u32 devid, u16 stream_id, u32 sqid)
{
    if ((devid >= TSAGENT_MAX_DEV_ID) || (stream_id >= TS_AGENT_MAX_STREAM_NUM)) {
        ts_agent_err("devid=%u, stream_id=%u is invalid, sqid=%u.", devid, stream_id, sqid);
        return false;
    }

    if (sqid == U32_MAX) {
        return true;
    }

    return (g_stream_id_to_sq_id_map[devid][stream_id] == sqid);
}

void tsagent_dvpp_register(ts_agent_dvpp_ops_t *ops)
{
    if (ops != NULL) {
        g_dvpp_ops.dvpp_sqe_update = ops->dvpp_sqe_update;
        ts_agent_info("dvpp reg successful.");
    }
}
#ifndef TS_AGENT_UT
EXPORT_SYMBOL(tsagent_dvpp_register);
#endif

void tsagent_dvpp_unregister(void)
{
    g_dvpp_ops.dvpp_sqe_update = NULL;
    ts_agent_info("dvpp unreg successful.");
}
#ifndef TS_AGENT_UT
EXPORT_SYMBOL(tsagent_dvpp_unregister);
#endif

static void sqe_error_dump(u32 devid, const ts_stars_sqe_t *sqe)
{
    size_t i;
    ts_agent_err("dump error sqe, dev_id=%u, stream_id=%u, task_id=%u, sqe_type=%u",
        devid, sqe->stream_id, sqe->task_id, sqe->type);

    for (i = 0U; i < (sizeof(ts_stars_sqe_t) / sizeof(u32)); i += 8U) {
        ts_agent_err("word[%zu-%zu]: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x.", i, i + 7U,
        ((const u32 *)sqe)[i + 0U], ((const u32 *)sqe)[i + 1U], ((const u32 *)sqe)[i + 2U], ((const u32 *)sqe)[i + 3U],
        ((const u32 *)sqe)[i + 4U], ((const u32 *)sqe)[i + 5U], ((const u32 *)sqe)[i + 6U], ((const u32 *)sqe)[i + 7U]);
    }
}

static int sqe_proc_dvpp(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    int ret;
    if (g_dvpp_ops.dvpp_sqe_update == NULL) {
        ts_agent_warn("dvpp ops is not register!");
        return EOK;
    }

    ret = g_dvpp_ops.dvpp_sqe_update(devid, tsid, pid, sqe);
    if (ret != EOK) {
        ts_agent_err("dvpp update is error, ret=%d, devid=%u, stream_id=%u, task_id=%u, pid=%d, sqe_type=%u",
            ret, devid, sqe->stream_id, sqe->task_id, pid, sqe->type);
    }
    return ret;
}

static int sqe_proc_event(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    bool is_match;
    ts_stars_event_sqe_t *evt_sqe = (ts_stars_event_sqe_t *)sqe;

#ifndef TS_AGENT_UT
    is_match = trs_is_proc_has_res(devid, tsid, pid, TRS_EVENT, evt_sqe->event_id);
    if (is_match == false) {
        ts_agent_err("event sqe check failed, ret=%d, devid=%u, stream_id=%u, task_id=%u, pid=%d, event_id=%u",
            is_match, devid, sqe->stream_id, sqe->task_id, pid, evt_sqe->event_id);
        return -EINVAL;
    }
#endif
    return EOK;
}

static int sqe_proc_notify(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    bool is_match;
    ts_stars_notify_sqe_t *nty_sqe = (ts_stars_notify_sqe_t *)sqe;

#ifndef TS_AGENT_UT
    is_match = trs_is_proc_has_res(devid, tsid, pid, TRS_NOTIFY, nty_sqe->notify_id);
    if (is_match == false) {
        ts_agent_err("notify sqe check failed, ret=%d, devid=%u, stream_id=%u, task_id=%u, pid=%d, notify_id=%u",
            is_match, devid, sqe->stream_id, sqe->task_id, pid, nty_sqe->notify_id);
        return -EINVAL;
    }
#endif
    return EOK;
}

static int sqe_proc_cdqm(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    bool is_match;
    ts_stars_cdqm_sqe_t *cdq_sqe = (ts_stars_cdqm_sqe_t *)sqe;

#ifndef TS_AGENT_UT
    is_match = trs_is_proc_has_res(devid, tsid, pid, TRS_CDQ, cdq_sqe->cdq_id);
    if (is_match == false) {
        ts_agent_err("cdq sqe check failed, ret=%d, devid=%u, stream_id=%u, task_id=%u, pid=%d, cdq_id=%u",
            is_match, devid, sqe->stream_id, sqe->task_id, pid, cdq_sqe->cdq_id);
        return -EINVAL;
    }

    if ((cdq_sqe->ptr_mode != 0U) && (cdq_sqe->va == 0U)) {
        ts_agent_err("cdq sqe check failed, devid=%u, stream_id=%u, task_id=%u, pid=%d, cdq_id=%u, ptr_mode=%u, va=%u",
            devid, sqe->stream_id, sqe->task_id, pid, cdq_sqe->cdq_id, cdq_sqe->ptr_mode, cdq_sqe->va);
        return -EINVAL;
    }
#endif
    return EOK;
}

#ifndef CFG_DEVICE_ENV
// vm vpc agent should check dma desc after converted, while pf will not get here
static int pcie_dma_desc_check(u32 devid, ts_stars_pciedma_sqe_t *sqe)
{
#if (!defined CFG_HOST_VIRTUAL_MACHINES) && (!defined CFG_DEVICE_ENV) && (!defined TS_AGENT_UT)
    int ret;
    struct devdrv_dma_desc_info check_dma = {
        .sq_dma_addr = ((u64)sqe->sq_addr_high << 32U) | (u64)sqe->sq_addr_low,
        .sq_size = (u64)sqe->sq_tail_ptr,
    };
    ret = devdrv_dma_sqcq_desc_check(devid, &check_dma);
    if (ret != 0) {
        ts_agent_err("pciedma desc check failed, ret=%d, devid=%u, stream_id=%u, task_id=%u, src=%#llx, dst=%#llx, "
            "len=%#llx, dma sq addr=%#llx, dma sq tail=%llu.", ret, devid, sqe->header.rt_stream_id,
            sqe->header.task_id, sqe->src, sqe->dst, sqe->length, check_dma.sq_dma_addr, check_dma.sq_size);
        return ret;
    }
#endif
    return EOK;
}
#endif

#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined TS_AGENT_UT) && (!defined CFG_DEVICE_ENV)
static int sqe_proc_vm_pciedma_for_update_sqe(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_pciedma_sqe_t *pcie_sqe)
{
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (defined CFG_HOST_VIRTUAL_MACHINES)
    int ret;
    u32 passid;
    u32 phy_sq_id;

    ret = hal_kernel_trs_get_ssid(devid, tsid, pid, &passid);
    if (ret != 0) {
        ts_agent_err("get ssid failed, ret=%d, devid=%u, pid=%d, tsid=%u, sq_id=%u",
            ret, devid, pid, tsid, sqid);
        return -EINVAL;
    }
    pcie_sqe->is_converted = 1U;
    pcie_sqe->passid = passid;

    ts_agent_info("get ssid success passid =%u", passid);
    ret = trs_res_trans_v2p(devid, tsid, TRS_HW_SQ, sqid, &phy_sq_id);
    if (ret != 0) {
        ts_agent_err("trans v2p failed, ret=%d, devid=%u, tsid=%d, sq_id=%u",
            ret, devid, tsid, sqid);
        return -EINVAL;
    }
    ts_agent_info("physical sq_id=%u", phy_sq_id);
    pcie_sqe->dst = ((u64)phy_sq_id << 32U) + (pcie_sqe->dst & 0x00000000FFFFFFFFULL);
#endif
    return EOK;
}

#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES)

#define UINT32_BIT_NUM (32U)

static int sqe_proc_convert_pciedma_to_sdma(u32 devid, u32 tsid, int pid, u64 sdma_dst_addr,
                                            ts_stars_pciedma_sqe_t *pcie_sqe)
{
    ts_stars_memcpy_async_sqe_t sdma_sqe = {};
    int ret;

    sdma_sqe.type = TS_STARS_SQE_TYPE_SDMA;
    sdma_sqe.ie = 0U;
    sdma_sqe.pre_p = 0U;
    sdma_sqe.post_p = 0U;
    sdma_sqe.wr_cqe = 0U;
    sdma_sqe.rt_stream_id = pcie_sqe->header.rt_stream_id;
    sdma_sqe.task_id = pcie_sqe->header.task_id;
    sdma_sqe.kernel_credit = pcie_sqe->kernel_credit;
    sdma_sqe.ptr_mode = 0U;
    sdma_sqe.opcode = 0U;
    sdma_sqe.src_streamid = 0U;
    sdma_sqe.dst_streamid = 0U;
    sdma_sqe.src_sub_streamid = 0U;
    sdma_sqe.dst_sub_streamid = 0U;
    sdma_sqe.length = pcie_sqe->length;
    sdma_sqe.src_addr_low = (uint32_t)((pcie_sqe->src) & 0x00000000ffffffffU);
    sdma_sqe.src_addr_high = (uint32_t)(((pcie_sqe->src) & 0xffffffff00000000U) >> UINT32_BIT_NUM);
    sdma_sqe.dst_addr_low = (uint32_t)(sdma_dst_addr & 0x00000000ffffffffU);
    sdma_sqe.dst_addr_high = (uint32_t)((sdma_dst_addr & 0xffffffff00000000U) >> UINT32_BIT_NUM);
    sdma_sqe.ie2  = 0U;
    sdma_sqe.sssv = 1U;
    sdma_sqe.dssv = 1U;
    sdma_sqe.sns  = 1U;
    sdma_sqe.dns  = 1U;
    sdma_sqe.qos  = 6U;
    sdma_sqe.sro  = 0U;
    sdma_sqe.dro  = 0U;
    sdma_sqe.partid = 0U;
    sdma_sqe.mpam = 0U;
    sdma_sqe.res3 = 0U;
    sdma_sqe.res4 = 0U;
    sdma_sqe.res5 = 0U;
    sdma_sqe.res6 = 0U;
    sdma_sqe.d2d_offset_flag = 0U;
    sdma_sqe.src_offset_low = 0U;
    sdma_sqe.dst_offset_low = 0U;
    sdma_sqe.src_offset_high = 0U;
    sdma_sqe.dst_offset_high = 0U;

    ret = memcpy_s((void *)pcie_sqe, sizeof(ts_stars_pciedma_sqe_t),
                   (void *)&sdma_sqe, sizeof(ts_stars_memcpy_async_sqe_t));
    if (ret != EOK) {
        ts_agent_err("memcpy_s failed, ret=%d, devid=%u, pid=%d, tsid=%u", ret, devid, pid, tsid);
    } else {
        ts_agent_info("convert pciedma to sdma success, devid=%u, pid=%d, tsid=%u", devid, pid, tsid);
    }

    return ret;
}
#endif

#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES)
static void sqe_proc_pm_pciedma_fill_addr_info(int pid, u32 sqid, u32 passid,
    ts_stars_pciedma_sqe_t *pcie_sqe, struct trs_dma_desc_addr_info *addr_info)
{
    u32 pos = (u32)(pcie_sqe->dst & 0x00000000FFFFFFFFULL);
    u16 cnt = (u16)(pcie_sqe->length);
    u16 offset;

    if (pcie_sqe->is_dsa_update == 1U) {
        offset = 16U;
    } else {
        offset = (u16)(pcie_sqe->offset);
    }

    addr_info->src_va = pcie_sqe->src;
    addr_info->passid = passid;
    addr_info->sqid = sqid;
    addr_info->sqeid = pos;
    addr_info->offset = offset;
    addr_info->size = cnt;
    addr_info->pid = pid;
}
#endif

static int sqe_proc_pm_pciedma_for_update_sqe(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_pciedma_sqe_t *pcie_sqe)
{
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES)
    int ret;
    u32 passid;
    u32 pos;
    struct trs_dma_desc_addr_info addr_info = {};
    struct trs_dma_desc dma = {};
    // if vm kernel, vm kernel has been convert to 1
    if (pcie_sqe->is_converted == 1U) {
        ts_agent_info("get passid from pcie sqe");
        passid = pcie_sqe->passid;
    } else if (pcie_sqe->is_converted == 0U) {
        ret = hal_kernel_trs_get_ssid(devid, tsid, pid, &passid);
        if (ret != 0) {
            ts_agent_err("get ssid failed, ret=%d, devid=%u, pid=%d, tsid=%u, sq_id=%u",
                ret, devid, pid, tsid, sqid);
            return -EINVAL;
        }
        ts_agent_info("get passid success, passid = %u", passid);
    }

    pos = (u32)(pcie_sqe->dst & 0x00000000FFFFFFFFULL);

    sqe_proc_pm_pciedma_fill_addr_info(pid, sqid, passid, pcie_sqe, &addr_info);

    ret = hal_kernel_sqe_update_desc_create(devid, tsid, &addr_info, &dma);
    if (ret != 0) {
        ts_agent_err("pciedma addr create failed, ret=%d, devid=%u, pid=%d, sq_id=%u, passid=%u",
            ret, devid, pid, sqid, passid);
        return ret;
    }

    if (dma.dma_type == TRS_DMA_TYPE_SDMA) {
        u64 sdma_dst_addr = (u64)(uintptr_t)dma.sdma_desc.dst_addr;
        return sqe_proc_convert_pciedma_to_sdma(devid, tsid, pid, sdma_dst_addr, pcie_sqe);
    }

    pcie_sqe->sq_addr_low = (uintptr_t)dma.pciedma_desc.sq_addr & U32_MAX;
    pcie_sqe->sq_addr_high = (u32)((uintptr_t)dma.pciedma_desc.sq_addr >> 32U);
    pcie_sqe->sq_tail_ptr = (u16)dma.pciedma_desc.sq_tail;
    ts_agent_info("pciedma addr create success, devid=%u, sq_id=%u, pos=%u, pid=%d, src=%#llx, cnt=%llu, "
        "dma sq addr=%#lx, dma sq tail=%u, type=%u, ssid=%u", devid, sqid, pos, pid, pcie_sqe->src, pcie_sqe->length,
        (uintptr_t)dma.pciedma_desc.sq_addr, dma.pciedma_desc.sq_tail, pcie_sqe->header.type, passid);
#endif
    return EOK;
}
#endif

#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined TS_AGENT_UT) && (!defined CFG_DEVICE_ENV)
static int sqe_proc_pciedma_for_update_sqe(u32 devid, u32 tsid, int pid, ts_stars_sqe_t *sqe)
{
    bool is_match;
    int ret;
#ifdef CFG_DEVICE_ENV
    /* device rc has no pcie dma sqe task */
    ts_agent_err("task is invaild, devid=%u, stream_id=%u, task_id=%u, pid=%d, tsid=%u, type=%u",
        devid, sqe->stream_id, sqe->task_id, pid, tsid, sqe->type);
    return -EINVAL;
#endif
    ts_stars_pciedma_sqe_t *pcie_sqe = (ts_stars_pciedma_sqe_t *)sqe;
    u32 sqid = (u32)((pcie_sqe->dst & 0xFFFFFFFF00000000ULL) >> 32U);
    u64 dsa_cp_size = 40ULL;
    ts_agent_info("sq_id=%u", sqid);

    if ((pcie_sqe->is_sqe_update == 1U) && ((pcie_sqe->length == 0ULL) ||
        (pcie_sqe->length > TS_AGENT_SQE_SIZE) ||
        ((pcie_sqe->length + pcie_sqe->offset) > TS_AGENT_SQE_SIZE))) {
        ts_agent_err("update cp size is invalid, invalid_size=%llu, invalid_offset=%u",
            pcie_sqe->length, pcie_sqe->offset);
        return -EINVAL;
    }

    if ((pcie_sqe->is_dsa_update == 1U) && (pcie_sqe->length != dsa_cp_size)) {
        ts_agent_err("dsa cp size is invalid, invalid_size=%llu, correct_size=%llu",
            pcie_sqe->length, dsa_cp_size);
        return -EINVAL;
    }

#ifndef TS_AGENT_UT
    is_match = trs_is_proc_has_res(devid, tsid, pid, TRS_HW_SQ, sqid);
    if (is_match == false) {
        ts_agent_err("sq_id check failed, devid=%u, pid=%d, sq_id=%u",
            devid, pid, sqid);
        return -EINVAL;
    }
#endif

    ret = sqe_proc_vm_pciedma_for_update_sqe(devid, tsid, pid, sqid, pcie_sqe);
    if (ret != 0) {
        ts_agent_err("dsa update failed, ret=%d, sqe_type=%u, devid=%u, sq_id=%u, pid=%d.",
            ret, sqe->type, devid, sqid, pid);
        sqe_error_dump(devid, sqe);
        return  ret;
    }
    ts_agent_info("dsa pm_pciedma in");
    return sqe_proc_pm_pciedma_for_update_sqe(devid, tsid, pid, sqid, pcie_sqe);
}
#endif

#ifndef CFG_DEVICE_ENV
#ifndef TS_AGENT_UT
static void ts_agent_pciedma_dfx(u32 devid)
{
    if (devid < TSAGENT_MAX_DEV_ID) {
        dma_cnt[devid] = dma_cnt[devid] + 1;
        if (dma_cnt_max[devid] < dma_cnt[devid]) {
            dma_cnt_max[devid] = dma_cnt[devid];
        }
    }
}
#endif

static void ts_agent_pciedma_update_sqe(ts_stars_pciedma_sqe_t *pcie_sqe, struct svm_dma_desc *dma)
{
    pcie_sqe->sq_addr_low = (uintptr_t)dma->sq_addr & U32_MAX;
    pcie_sqe->sq_addr_high = (u32)((uintptr_t)dma->sq_addr >> 32U);
    pcie_sqe->sq_tail_ptr = (u16)dma->sq_tail;
    pcie_sqe->is_converted = 1UL;
}
#endif

static int sqe_proc_pciedma(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
#ifdef CFG_DEVICE_ENV
    /* device rc has no pcie dma sqe task */
    ts_agent_err("task is invalid, devid=%u, stream_id=%u, task_id=%u, pid=%d, tsid=%u, type=%u",
        devid, sqe->stream_id, sqe->task_id, pid, tsid, sqe->type);
    return -EINVAL;
#else
    int ret;
    ts_stars_pciedma_sqe_t *pcie_sqe = (ts_stars_pciedma_sqe_t *)sqe;
    struct svm_dma_desc_addr_info addr = {
        .src_va = pcie_sqe->src, .dst_va = pcie_sqe->dst, .size = pcie_sqe->length,};
    struct svm_dma_desc_handle handle = {
        .pid = pid, .key = (devid << 16U) | (u32)sqe->stream_id, .subkey = (u32)sqe->task_id,};
    struct svm_dma_desc dma = {};

#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined TS_AGENT_UT)
    if ((pcie_sqe->is_dsa_update == 1U) || (pcie_sqe->is_sqe_update == 1U)) {
        if (sqid == U32_MAX) {
            ts_agent_err("not support sqe_proc_pciedma for decoupled stream_id and sq_id scenario: sqid=%u, stream_id=%u", 
                sqid, sqe->stream_id);
            return EOK;
        }

        return sqe_proc_pciedma_for_update_sqe(devid, tsid, pid, sqe);
    }
#endif

    if (pcie_sqe->is_converted == 0U) {
    #ifndef TS_AGENT_UT
        ret = hal_kernel_svm_dma_desc_create(&addr, &handle, &dma);
        if (ret != 0) {
            ts_agent_err("pciedma addr convert failed, ret=%d, devid=%u, stream_id=%u, task_id=%u, pid=%d, "
                    "src=%#llx, dst=%#llx, len=%#llx, is_not_delete=%u.", ret, devid, sqe->stream_id, sqe->task_id, pid,
                    pcie_sqe->src, pcie_sqe->dst, pcie_sqe->length, (ret != -ESRCH) ? 1U: 0U);
            return ret;
        }

        ts_agent_pciedma_dfx(devid);

        if (ret != 0) {
            if (ret != -ESRCH) {
                ts_agent_err("pciedma addr convert failed, ret=%d, devid=%u, stream_id=%u, task_id=%u, pid=%d, "
                    "src=%#llx, dst=%#llx, len=%#llx.", ret, devid, sqe->stream_id, sqe->task_id, pid,
                    pcie_sqe->src, pcie_sqe->dst, pcie_sqe->length);
            }
            return ret;
        }
    #endif

        ts_agent_pciedma_update_sqe(pcie_sqe, &dma);

        ts_agent_debug("devid=%u, stream_id=%u, task_id=%u, pid=%d, src=%#llx, dst=%#llx, len=%#llx, dma sq addr=%#lx, "
            "dma sq tail=%u.", devid, sqe->stream_id, sqe->task_id, pid, pcie_sqe->src, pcie_sqe->dst, pcie_sqe->length,
            (uintptr_t)dma.sq_addr, dma.sq_tail);
        return EOK;
    }
    return pcie_dma_desc_check(devid, pcie_sqe);
#endif
}

static int sqe_proc_sdma(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    ts_stars_memcpy_async_sqe_t *sdma_sqe = (ts_stars_memcpy_async_sqe_t *)sqe;
    if (sdma_sqe->ptr_mode == 0U) {
        if ((sdma_sqe->sssv == 1U) && (sdma_sqe->dssv == 1U)) {
            return EOK;
        }
        ts_agent_err("sdma sqe check failed, devid=%u, stream_id=%u, task_id=%u, pid=%d, sssv=%u, dssv=%u",
            devid, sqe->stream_id, sqe->task_id, pid, sdma_sqe->sssv, sdma_sqe->dssv);
        return -EINVAL;
    } else {
        ts_stars_memcpy_ptr_async_sqe_t *sdma_ptr_sqe = (ts_stars_memcpy_ptr_async_sqe_t *)sqe;
        if (sdma_ptr_sqe->va == 0ULL) {
            ts_agent_err("sdma sqe check failed, devid=%u, stream_id=%u, task_id=%u, pid=%d, ptr_mode=%u, va=%u",
                devid, sqe->stream_id, sqe->task_id, pid, sdma_ptr_sqe->ptr_mode, sdma_ptr_sqe->va);
            return -EINVAL;
        }
    }

    return EOK;
}

static int sqe_proc_write_value_check_event_addr(u32 devid, u32 tsid, int pid, uint64_t reg_addr)
{
    bool is_match;
    u64 event_table_id;
    u64 event_num;
    u32 event_id;

    if ((reg_addr & TS_STARS_SINGLE_DEV_ADDR_MASK & (~(TS_STARS_EVENT_TABLE_MASK | TS_STARS_EVENT_MASK))) !=
        (TS_STARS_BASE_ADDR + TS_STARS_EVENT_BASE_ADDR)) {
        ts_agent_err("stars addr base check failed, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
        return -EINVAL;
    }

    if (((reg_addr & TS_STARS_EVENT_MASK) % TS_STARS_EVENT_OFFSET) != 0U) {
        ts_agent_err("event offset check failed, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
        return -EINVAL;
    }

    event_num = (reg_addr & TS_STARS_EVENT_MASK) / TS_STARS_EVENT_OFFSET;
    event_table_id = (reg_addr & TS_STARS_EVENT_TABLE_MASK) / TS_STARS_EVENT_TABLE_OFFSET;
    event_id = (u32)((event_table_id * TS_STARS_EVENT_NUM_OF_SINGLE_TABLE) + event_num);

#ifndef TS_AGENT_UT
    is_match = trs_is_proc_has_res(devid, tsid, pid, (int)TRS_EVENT, (int)event_id);
    if (is_match == false) {
        ts_agent_err("write value event check failed, ret=%d, devid=%u, pid=%d, event_id=%u",
            is_match, devid, pid, event_id);
        return -EINVAL;
    }
#endif

    return EOK;
}

static int sqe_proc_write_value_check_notify_addr_no_pcie(
    u32 devid, u32 tsid, int pid, uint64_t reg_addr, uint32_t isPod)
{
    bool is_match;
    uint64_t notify_table_id;
    uint64_t notify_num;
    uint64_t notify_id;

    if (isPod == 0) {
        if ((reg_addr & TS_STARS_SINGLE_DEV_ADDR_MASK & (~(TS_STARS_NOTIFY_TABLE_MASK | TS_STARS_NOTIFY_MASK))) !=
            (TS_STARS_BASE_ADDR + TS_STARS_NOTIFY_BASE_ADDR)) {
            ts_agent_err("stars addr base check failed, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
            return -EINVAL;
        }
    } else {
#ifndef TS_AGENT_UT
        // is hccs
        // addr is 48T
        // 用地址反算 remote chip and die id is valid
        if ((reg_addr & TS_STARS_SINGLE_DEV_ADDR_MASK & TS_STARS_NOTIFY_POD_TABLE_OFFSET) !=
            TS_STARS_NOTIFY_POD_TABLE_OFFSET) {
            ts_agent_err("stars addr base check failed, pod, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
            return -EINVAL;
        }
#endif
    }

    if (((reg_addr & TS_STARS_NOTIFY_MASK) % TS_STARS_NOTIFY_OFFSET) != 0ULL) {
        ts_agent_err("notify offset check failed, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
        return -EINVAL;
    }

    notify_num = (reg_addr & TS_STARS_NOTIFY_MASK) / TS_STARS_NOTIFY_OFFSET;
    notify_table_id = (reg_addr & TS_STARS_NOTIFY_TABLE_MASK) / TS_STARS_NOTIFY_TABLE_OFFSET;
    notify_id = (notify_table_id * TS_STARS_NOTIFY_NUM_OF_SINGLE_TABLE) + notify_num;
#ifndef TS_AGENT_UT
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES) && (defined CFG_HOST_ENV)
    if (isPod != 0) {
        is_match = hal_kernel_trs_is_belong_to_pod_proc(devid, tsid, pid, TRS_NOTIFY, (int)notify_id);
    } else {
        is_match = trs_is_proc_has_res(devid, tsid, pid, TRS_NOTIFY, (int)notify_id);
    }
#else
    is_match = trs_is_proc_has_res(devid, tsid, pid, TRS_NOTIFY, (int)notify_id);
#endif
    if (is_match == false) {
        ts_agent_err("notifyid check failed,pid=%d,notify_id=%#llx,tsid=%u,addr=%#llx,devid=%u",
            pid, notify_id, tsid, reg_addr, devid);
        return -EINVAL;
    }
#endif

    ts_agent_debug("notify_id=%#llx check success!pid=%d,devid=%u", notify_id, pid, devid);
    return EOK;
}

static int sqe_proc_write_value_check_notify_addr_pcie(u32 devid, u32 tsid, int pid, uint64_t reg_addr)
{
    bool is_match;
    uint32_t notify_table_id;
    uint32_t notify_num;
    uint64_t notify_id;

    if ((reg_addr & TS_STARS_PCIE_BASE_MASK & (~(TS_STARS_NOTIFY_TABLE_MASK | TS_STARS_NOTIFY_MASK))) !=
        (TS_STARS_PCIE_BASE_ADDR + TS_STARS_NOTIFY_BASE_ADDR)) {
        ts_agent_err("notify pcie addr base check failed, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
        return -EINVAL;
    }

    if (((reg_addr & TS_STARS_NOTIFY_MASK) % TS_STARS_NOTIFY_OFFSET) != 0ULL) {
        ts_agent_err("notify offset check failed, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
        return -EINVAL;
    }

    notify_num = (uint32_t)((reg_addr & TS_STARS_NOTIFY_MASK) / TS_STARS_NOTIFY_OFFSET);
    notify_table_id = (uint32_t)((reg_addr & TS_STARS_NOTIFY_TABLE_MASK) / TS_STARS_NOTIFY_TABLE_OFFSET);
    notify_id = (notify_table_id * TS_STARS_NOTIFY_NUM_OF_SINGLE_TABLE) + notify_num;
#ifndef TS_AGENT_UT
    is_match = trs_is_proc_has_res(devid, tsid, pid, TRS_NOTIFY, (int)notify_id);
    if (is_match == false) {
        ts_agent_err("notifyid check failed,pid=%d,notify_id=%#llx,tsid=%u,addr=%#llx,devid=%u",
            pid, notify_id, tsid, reg_addr, devid);
        return -EINVAL;
    }
#endif

    ts_agent_debug("notify_id=%#llx check success!pid=%d,devid=%u", notify_id, pid, devid);
    return EOK;
}

static int sqe_proc_write_value_check_rdma_addr(u32 devid, u32 tsid, int pid, uint64_t reg_addr)
{
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES) && (defined CFG_HOST_ENV)
    struct tsagent_dev_base_info *dev_base_info = NULL;
    uint64_t dbAddr;

    dev_base_info = tsagent_get_device_base_info(devid);
    if (dev_base_info == NULL) {
        return -EINVAL;
    }

    if ((dev_base_info->soc_type == SOC_TYPE_CLOUD_V2) || (dev_base_info->soc_type == SOC_TYPE_CLOUD_V3)) {
        uint64_t dieOffset = TS_AGENT_ASCEND910B1_DIE_ADDR_OFFSET;
        uint64_t chipOffset = TS_AGENT_ASCEND910B1_CHIP_ADDR_OFFSET;
        uint64_t chipBaseAddr = 0;

        if (dev_base_info->addr_mode == ADDR_UNIFIED)  {
            chipBaseAddr = TS_AGENT_CHIP_BASE_ADDR;
            chipOffset = TS_AGENT_ASCEND910B1_HCCS_CHIP_ADDR_OFFSET;
        }

        dbAddr = TS_ROCEE_BASE_ADDR + TS_ROCEE_VF_DB_CFG0_REG +
            chipOffset * dev_base_info->chip_id + dieOffset * dev_base_info->die_id + chipBaseAddr;
        if (reg_addr != dbAddr) {
            ts_agent_err("rdma addr base check failed, devid=%u, pid=%d, addr=%#llx, except dbAddr=%#llx, "
                "chip_id=%u, die_id=%u, addr_mode=%u, soc_type=%u",
                devid, pid, reg_addr, dbAddr, dev_base_info->chip_id, dev_base_info->die_id,
                dev_base_info->addr_mode, dev_base_info->soc_type);
            return -EINVAL;
        }
    } else {
        ts_agent_err("rdma addr base check failed, devid=%u, pid=%d, addr=%#llx, "
            "chip_id=%u, die_id=%u, addr_mode=%u, soc_type=%u",
            devid, pid, reg_addr, dev_base_info->chip_id, dev_base_info->die_id,
            dev_base_info->addr_mode, dev_base_info->soc_type);
        return -EINVAL;
    }

    ts_agent_debug("rdma addr base check success, devid=%u, pid=%d, addr=%#llx, "
        "chip_id=%u, die_id=%u, addr_mode=%u, soc_type=%u",
        devid, pid, reg_addr, dev_base_info->chip_id, dev_base_info->die_id,
        dev_base_info->addr_mode, dev_base_info->soc_type);
#else
    if ((reg_addr & TS_STARS_SINGLE_DEV_ADDR_MASK) != (TS_ROCEE_BASE_ADDR + TS_ROCEE_VF_DB_CFG0_REG)) {
        ts_agent_err("rdma addr base check failed, devid=%u, pid=%d, addr=%#llx", devid, pid, reg_addr);
        return -EINVAL;
    }
#endif

    return EOK;
}

static int tsagent_check_write_value_sub_type(u32 devid, u32 remote_dev_id, int pid, u32 sub_type)
{
#if (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES) && (defined CFG_HOST_ENV)    
    HAL_KERNEL_TOPOLOGY_TYPE topology_type = TOPOLOGY_TYPE_MAX;
    u32 phy_dev_id = devid;
    struct uda_mia_dev_para mia_para;
    int ret;

    if (uda_is_phy_dev(devid) == false) {
        ret = uda_udevid_to_mia_devid(devid, &mia_para);
        if (ret != EOK) {
            ts_agent_err("convert devid failed, devid=%u, ret=%d.", devid, ret);
            return -EINVAL;
        }

        phy_dev_id = mia_para.phy_devid;
    }

    ret = hal_kernel_get_d2d_topology_type(phy_dev_id, remote_dev_id, &topology_type);
    if (ret != EOK) {
        ts_agent_err("sub type check failed, phy_dev_id=%u, devid=%u, remote_dev_id=%u, pid=%d, sub_type=%u, ret=%d.",
            phy_dev_id, devid, remote_dev_id, pid, sub_type, ret);
        return -EINVAL;
    }

    if (sub_type == TS_STARS_WRITE_VALUE_SUB_TYPE_NOTIFY_RECORD_IPC_PCIE) {
        /* Below are all PCIe types in kernel space, all return as PIX type in user space */
        if (topology_type == TOPOLOGY_TYPE_PIX || topology_type == TOPOLOGY_TYPE_PIB ||
            topology_type == TOPOLOGY_TYPE_PHB || topology_type == TOPOLOGY_TYPE_SYS) {
            ts_agent_debug("sub type check success, phy_dev_id=%u, devid=%u, remote_dev_id=%u, "
                "pid=%d, sub_type=%u, topology_type=%u.",
                phy_dev_id, devid, remote_dev_id, pid, sub_type, topology_type);
            return EOK;
        }
    } else {
        if ((topology_type == TOPOLOGY_TYPE_HCCS) || (topology_type == TOPOLOGY_TYPE_HCCS_SW) ||
            (topology_type == TOPOLOGY_TYPE_SIO)) {
            ts_agent_debug("sub type check success, phy_dev_id=%u, devid=%u, remote_dev_id=%u, "
                "pid=%d, sub_type=%u, topology_type=%u.",
                phy_dev_id, devid, remote_dev_id, pid, sub_type, topology_type);
            return EOK;
        }
    }

    ts_agent_err("sub type check failed, phy_dev_id=%u, devid=%u, remote_dev_id=%u, "
        "pid=%d, sub_type=%u, topology_type=%u.",
        phy_dev_id, devid, remote_dev_id, pid, sub_type, topology_type);

    return -EINVAL;
#else

    return EOK;
#endif
}


static int sqe_proc_write_value(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    int ret = EINVAL;
    uint64_t reg_addr;
    ts_stars_write_value_sqe_t *wv_sqe = (ts_stars_write_value_sqe_t *)sqe;
    uint32_t remote_dev_id = wv_sqe->res3;
    uint32_t isPod = wv_sqe->res4;

    if (wv_sqe->ptr_mode == 1U) {
        return EOK;
    }

    if (wv_sqe->awsize > 5U) { // 5: awsize max value
        ts_agent_err("awsize check failed, awsize=%u", wv_sqe->awsize);
        goto ERR_PROC;
    }

    if (!IS_ALIGNED((wv_sqe->write_addr_low), (1U << wv_sqe->awsize))) {
        ts_agent_err("addr aligned check failed, awsize=%u, write_addr_low=%#x",
            wv_sqe->awsize, wv_sqe->write_addr_low);
        goto ERR_PROC;
    }

    // 1: virtual address
    if (wv_sqe->va == 1U) {
        return EOK;
    }

    if ((isPod == 0) && (wv_sqe->sub_type == (uint32_t)TS_STARS_WRITE_VALUE_SUB_TYPE_NOTIFY_RECORD_IPC_NO_PCIE ||
        wv_sqe->sub_type == (uint32_t)TS_STARS_WRITE_VALUE_SUB_TYPE_NOTIFY_RECORD_IPC_PCIE)) {
        if (tsagent_check_write_value_sub_type(devid, remote_dev_id, pid, wv_sqe->sub_type) != EOK) {
#ifndef TS_AGENT_UT
            goto ERR_PROC;
#endif
        }
    }

    reg_addr = ((uint64_t)wv_sqe->write_addr_high << 32U) | wv_sqe->write_addr_low;
    if (wv_sqe->sub_type == TS_STARS_WRITE_VALUE_SUB_TYPE_EVENT_RESET) {
        ret = sqe_proc_write_value_check_event_addr(devid, tsid, pid, reg_addr);
    } else if (wv_sqe->sub_type == TS_STARS_WRITE_VALUE_SUB_TYPE_NOTIFY_RECORD_IPC_NO_PCIE) {
        ret = sqe_proc_write_value_check_notify_addr_no_pcie(remote_dev_id, tsid, pid, reg_addr, isPod);
    } else if (wv_sqe->sub_type == TS_STARS_WRITE_VALUE_SUB_TYPE_NOTIFY_RECORD_IPC_PCIE) {
        ret = sqe_proc_write_value_check_notify_addr_pcie(remote_dev_id, tsid, pid, reg_addr);
    } else if (wv_sqe->sub_type == TS_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND) {
        ret = sqe_proc_write_value_check_rdma_addr(devid, tsid, pid, reg_addr);
    } else {
        // reserve
    }

    if (ret == EOK) {
        return EOK;
    }

ERR_PROC:
    ts_agent_err("write value check failed, devid=%u, stream_id=%u, task_id=%u, pid=%d, addr_h=%#x, addr_l=%#x, "
        "sub_type=%u", devid, sqe->stream_id, sqe->task_id, pid, wv_sqe->write_addr_high, wv_sqe->write_addr_low,
        wv_sqe->sub_type);
    return -EINVAL;
}

static int cmo_sqe_check(u32 devid, u32 tsid, int pid, int cmo_id)
{
#ifndef TS_AGENT_UT
    bool is_match = trs_is_proc_has_res(devid, tsid, pid, (int)TRS_CMO, (int)cmo_id);
    if (!is_match) {
        ts_agent_err("CMO sqe check failed, devid = %u, tsid = %u, pid = %d, cmo_id = %u", devid, tsid, pid, cmo_id);
        return -EINVAL;
    }
#endif
    return EOK;
}

static int sqe_proc_barrier(u32 devid, u32 tsid, int pid, ts_stars_sqe_t *sqe)
{
    ts_stars_barrier_sqe_t *barrier_sqe = (ts_stars_barrier_sqe_t *)sqe;
    uint32_t cmo_bitmap = (uint32_t)(barrier_sqe->cmo_bitmap);
    uint32_t index;
    for (index = 0U; index < MAX_CMO_INFO_NUM; index++) { // barrier task contains 6 cmoinfo
        if (((cmo_bitmap >> index) & 1U) != 0U) { // get the index bit of com_bitmap
            int res = cmo_sqe_check(devid, tsid, pid, barrier_sqe->cmo_info[index].cmo_id);
            if (res == -EINVAL) {
                return -EINVAL;
            }
        }
    }
    return EOK;
}

static int sqe_proc_cmo(u32 devid, u32 tsid, int pid, ts_stars_sqe_t *sqe)
{
    ts_stars_cmo_sqe_t *cmo_sqe = (ts_stars_cmo_sqe_t *)sqe;
    if (cmo_sqe->cmo_type != 0U) { // 0 is barrier
        return cmo_sqe_check(devid, tsid, pid, cmo_sqe->cmo_id);
    }
    return sqe_proc_barrier(devid, tsid, pid, sqe);
}

/* update taskid used by plan A/B/C */
static int sqe_update_for_all_ffts_plus(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
#ifndef TS_AGENT_UT    
    ts_ffts_plus_sqe_t *ffts_sqe = (ts_ffts_plus_sqe_t *)sqe;
    uint16_t tmp_stream_id = ffts_sqe->header.rt_stream_id;
    uint16_t real_stream_id = ffts_sqe->header.rt_stream_id;
    uint16_t real_task_id = ffts_sqe->header.task_id;
    bool is_match = true;

    /* plan C is used */
    if ((tmp_stream_id >> TS_UPDATE_FOR_STREAM_EXTEND_FLAG_BIT) == TS_UPDATE_FOR_STREAM_EXTEND) {
        return EOK;
    }

    /* plan A or plan B is used */
    if ((((tmp_stream_id >> TS_UPDATE_FLAG_BIT) & TS_MASK_BIT0_BIT1) == TS_UPDATE_FOR_ALL) ||
        ((tmp_stream_id & TS_MASK_BIT12) != 0U)) {
        return EOK;
    }

    /* check stream id */
    is_match = tsagent_sq_is_belong_to_stream(devid, tmp_stream_id & TS_MASK_BIT0_BIT14, sqid);
    if (is_match == false) {
        ts_agent_err("sqe check failed, devid=%u, tsid=%u, stream_id=%u, task_id=%u, pid=%d, sqid=%u",
            devid, tsid, tmp_stream_id, ffts_sqe->header.task_id, pid, sqid);
        return -EINVAL;
    }

    /* plan C will be used when plan A or plan B or plan C does not take action */
    // task_id[0:14] + updateflag bit15:1 = stream_id
    ffts_sqe->header.rt_stream_id = (real_task_id & TS_MASK_BIT0_BIT14) |
        (TS_UPDATE_FOR_STREAM_EXTEND << TS_UPDATE_FOR_STREAM_EXTEND_FLAG_BIT);
    // stream_id[0:14] + task_id[15] = task_id
    ffts_sqe->header.task_id = (real_stream_id & TS_MASK_BIT0_BIT14) | (real_task_id & TS_MASK_BIT15);

    ts_agent_debug("update fftsplus sqe for all, devid=%u, tsid=%u, pid=%d, sqid=%u, "
        "real_stream_id=%hu, real_task_id=%hu, new_stream_id=%hu, new_task_id=%hu",
        devid, tsid, pid, sqid, real_stream_id, real_task_id,
        ffts_sqe->header.rt_stream_id, ffts_sqe->header.task_id);
#endif
    return EOK;    
}

static int sqe_proc_ffts(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    ts_ffts_plus_sqe_t *ffts_sqe = (ts_ffts_plus_sqe_t *)sqe;

    // CMO SQE reuses FFTS SQE, bit3 of word2 identifies whether task type is CMO.
    if (ffts_sqe->cmo == 0x1U) {
        return sqe_proc_cmo(devid, tsid, pid, sqe);
    }

    if (ffts_sqe->ffts_type != TS_FFTS_TYPE_FFTS_PLUS) {
        return -EOK;
    }

    if (sqe_update_for_all_ffts_plus(devid, tsid, pid, sqid, sqe) != EOK) {
        return -EINVAL;
    }

    if (!IS_ALIGNED((ffts_sqe->context_address_base_l), 128U)) { // 128 aligned
        ts_agent_err("addr aligned check failed, devid=%u, stream_id=%u, task_id=%u, pid=%d, address_base_l=%#x",
            devid, sqe->stream_id, sqe->task_id, pid, ffts_sqe->context_address_base_l);
        return -EINVAL;
    }
#ifndef TS_AGENT_UT
    if (uda_is_phy_dev(devid) == false) {
        ts_agent_debug("sqe pid update success, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            devid, sqe->stream_id, sqe->task_id, pid);
        ffts_sqe->pid = (u32)pid;
    }
#endif
    return EOK;
}

#if (!defined TS_AGENT_UT)
static int get_mnt_ns(int input_pid, u64 *mnt_ns)
{
    ka_task_struct_t *task = NULL;
    ka_struct_pid_t *pgrp = NULL;

    pgrp = ka_task_find_get_pid(input_pid);
    if (pgrp == NULL) {
        ts_agent_err("pid check failed, could not find pid, pid=%d.", input_pid);
        return -EINVAL;
    }

    task = ka_task_get_pid_task(pgrp, KA_PIDTYPE_PID);
    if (task == NULL) {
        ka_task_put_pid(pgrp);
        ts_agent_err("Failed to get pid task, pid=%d.", input_pid);
        return -EINVAL;
    }

    if (ka_task_get_nsproxy(task) != NULL) {
        if (ka_task_get_mnt_ns(task) != NULL) {
            *mnt_ns = (u64)(uintptr_t)(ka_task_get_mnt_ns(task));
        } else {
            ts_agent_err("mnt_ns is NULL, pid=%d.", input_pid);
        }
    } else {
        ts_agent_err("nsproxy is NULL, pid=%d.", input_pid);
    }

    ka_task_put_task_struct(task);
    ka_task_put_pid(pgrp);
    return EOK;
}

static int check_mnt_ns(int pid, ts_stars_sqe_t *sqe)
{
    ts_stars_topic_sched_sqe_t *tp_sqe = (ts_stars_topic_sched_sqe_t *)sqe;
    if (tp_sqe->dest_pid_flag != 0U) {
        u64 src_mnt_ns = 0ULL;
        u64 dest_mnt_ns = 0ULL;
        u32 dest_pid = 0U;
        int ret = EOK;
        if (tp_sqe->usr_data_len < sizeof(dest_pid)) {
            ts_agent_err("topic_sched sqe usr_data_len should >= 4 Bytes, when dest_pid_flag was set.");
            return -EINVAL;
        }
        dest_pid = *(u32*)(&(tp_sqe->user_data[tp_sqe->usr_data_len - sizeof(dest_pid)]));
        ret = get_mnt_ns(pid, &src_mnt_ns);
        if ((ret != EOK) || (src_mnt_ns == 0ULL)) {
            return -EINVAL;
        }
        ret = get_mnt_ns((int)(dest_pid), &dest_mnt_ns);
        if ((ret != EOK) || (dest_mnt_ns == 0ULL)) {
            return -EINVAL;
        }
        if (src_mnt_ns != dest_mnt_ns) {
            ts_agent_err("src_pid and dest_pid are not in the same mnt namespace, src_pid=%d, dest_pid=%d.",
                pid, (int)(dest_pid));
            return -EINVAL;
        }
    }
    return EOK;
}
#endif


static int sqe_proc_topic(u32 devid, u32 tsid, int pid, u32 sqid, ts_stars_sqe_t *sqe)
{
    ts_stars_aicpu_sqe_t *topic_sqe = (ts_stars_aicpu_sqe_t *)sqe;

    #ifdef CFG_SOC_PLATFORM_MINIV3_EP
    if ((topic_sqe->topic_type == (uint16_t)TOPIC_TYPE_HOST_AICPU_ONLY) ||
        (topic_sqe->topic_type == (uint16_t)TOPIC_TYPE_HOST_AICPU_FIRST)) {
        ts_agent_err("Chip type not support, return error, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            topic_sqe->topic_type, devid, sqe->stream_id, sqe->task_id, pid);
        sqe_error_dump(devid, sqe);
        topic_sqe->type = TS_STARS_SQE_TYPE_INVALID; /* set invalid flag */
        /* 1911 ep do not support host aicpu topic message, ts_agent will still sends this sqe to stars,
           when stars finds invalid flag, it will report error info to runtime by cqe.
        */
        return EOK;
    }
    #endif

    #ifdef CFG_SOC_PLATFORM_MINIV3_RC
    if ((topic_sqe->topic_type == (uint16_t)TOPIC_TYPE_HOST_AICPU_ONLY) ||
        (topic_sqe->topic_type == (uint16_t)TOPIC_TYPE_HOST_AICPU_FIRST) ||
        (topic_sqe->topic_type == (uint16_t)TOPIC_TYPE_HOST_CTRL_CPU)) {
        ts_agent_err("Chip type not support, return error, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            topic_sqe->topic_type, devid, sqe->stream_id, sqe->task_id, pid);
        sqe_error_dump(devid, sqe);
        topic_sqe->type = TS_STARS_SQE_TYPE_INVALID; /* set invalid flag */
        /* 1911 rc do not support host aicpu and host ctrl topic message, ts_agent will still sends this sqe to stars,
           when stars finds invalid flag, it will report error info to runtime by cqe.
        */
        return EOK;
    }
    #endif

    // custom kfc cpu kernel needs to pass host pid
    topic_sqe->p_l2ctrl_low = (topic_sqe->kernel_type == TS_CPU_KERNEL_TYPE_CUSTOM_KFC) ? (u32)pid : topic_sqe->p_l2ctrl_low;
    if (uda_is_phy_dev(devid) == false) {
        ts_stars_callback_sqe_t *callback_sqe = (ts_stars_callback_sqe_t *)sqe;

        if (callback_sqe->topic_id == 26U) {
            callback_sqe->res5 = (u32)pid;
            ts_agent_debug("sqe pid update success, topic_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
                callback_sqe->topic_type, devid, sqe->stream_id, sqe->task_id, pid);
        } else if (topic_sqe->topic_type <= TOPIC_TYPE_HOST_AICPU_FIRST) {
            topic_sqe->p_l2ctrl_low = (u32)pid;
            ts_agent_debug("sqe pid update success, topic_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
                topic_sqe->topic_type, devid, sqe->stream_id, sqe->task_id, pid);
        } else {
            // reserve
        }
    }

    #if (!defined TS_AGENT_UT)
    if (topic_sqe->topic_type == (uint16_t)TOPIC_TYPE_HOST_CTRL_CPU) {
        int ret = check_mnt_ns(pid, sqe);
        if (ret != EOK) {
            return -EINVAL;
        }
    }
    #endif

    return EOK;
}

#ifndef TS_AGENT_UT
int tsagent_sqe_update_check_sqe_type(u32 devid, int pid, ts_stars_sqe_t *sqe)
{
    if ((sqe->type != (uint8_t)TS_STARS_SQE_TYPE_NOTIFY_WAIT) &&
        (sqe->type != (uint8_t)TS_STARS_SQE_TYPE_EVENT_WAIT) &&
        (sqe->type != (uint8_t)TS_STARS_SQE_TYPE_AICPU) &&
        (sqe->type != (uint8_t)TS_STARS_SQE_TYPE_FFTS)) {
        ts_agent_err("kernel credit can't be 0xFF, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            sqe->type, devid, sqe->stream_id, sqe->task_id, pid);
        return -EINVAL;
    } else {
        // reserve
    }

    return EOK;
}
#endif

int tsagent_sqe_update(u32 devid, u32 tsid, int pid, u32 sqid, void *sqe_buf)
{
    int ret = EOK;
    ts_stars_sqe_t *sqe = (ts_stars_sqe_t *)sqe_buf;
    sqe_hook_proc_t proc_fn = NULL;
#ifndef TS_AGENT_UT
    if (sqid == U32_MAX) {
        ts_agent_debug("tsagent_sqe_update: stream_id=%u, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            sqe->stream_id, sqe->type, devid, sqe->stream_id, sqe->task_id, pid);
    }
#endif
    if (sqe->type >= TS_STARS_SQE_TYPE_END) {
        ts_agent_err("sqe type is invalid, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            sqe->type, devid, sqe->stream_id, sqe->task_id, pid);
        sqe_error_dump(devid, sqe);
        sqe->type = TS_STARS_SQE_TYPE_INVALID; /* set invalid flag */
        /* if sqe is invalid, ts_agent will still sends this sqe to stars,
           when stars finds invalid flag, it will report error info to runtime by cqe.
        */
        return EOK;
    }

#ifndef TS_AGENT_UT
    if (sqe->kernel_credit == 0xFFU) {
        ret = tsagent_sqe_update_check_sqe_type(devid, pid, sqe);
        if (ret != EOK) {
            sqe_error_dump(devid, sqe);
            sqe->type = TS_STARS_SQE_TYPE_INVALID;
            return -EINVAL;
        }
    }
#endif

    proc_fn = g_sqe_proc_fn[sqe->type];
    if (proc_fn == NULL) {
        ts_agent_debug("sqe no need update, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            sqe->type, devid, sqe->stream_id, sqe->task_id, pid);
        return EOK;
    }
    ret = proc_fn(devid, tsid, pid, sqid, sqe);
    if (ret != 0) {
        ts_agent_err("sqe update failed, ret=%d, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
            ret, sqe->type, devid, sqe->stream_id, sqe->task_id, pid);
        if (!((sqe->type == (uint8_t)TS_STARS_SQE_TYPE_PCIE_DMA) && (ret == -ESRCH))) {
            sqe_error_dump(devid, sqe);
        }
        sqe->type = TS_STARS_SQE_TYPE_INVALID; /* set invalid flag */
        /* if sqe is invalid, ts_agent will still sends this sqe to stars,
           when stars finds invalid flag, it will report error info to runtime by cqe.
        */
        return EOK;
    }
    ts_agent_debug("sqe update success, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.",
        sqe->type, devid, sqe->stream_id, sqe->task_id, pid);
    return EOK;
}

void init_task_convert_func(void)
{
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_EVENT_RECORD] = sqe_proc_event;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_NOTIFY_RECORD] = sqe_proc_notify;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_CDQM] = sqe_proc_cdqm;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_VPC] = sqe_proc_dvpp;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_JPEGE] = sqe_proc_dvpp;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_JPEGD] = sqe_proc_dvpp;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_PCIE_DMA] = sqe_proc_pciedma;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_WRITE_VALUE] = sqe_proc_write_value;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_SDMA] = sqe_proc_sdma;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_FFTS] = sqe_proc_ffts;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_AICPU] = sqe_proc_topic;
#if defined(CFG_SOC_PLATFORM_KPSTARS)
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_EVENT_RECORD] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_SDMA] = kp_sqe_proc_sdma;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_ROCCE] = sqe_proc_rdma;
#endif
}

#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
#if (!defined TS_AGENT_UT)
void tsagent_set_sram_overflow_bit(u32 devid, u32 bit_offset, u32 reg_offset, u32 sram_overflow_offset)
{
    u32 unit = TSAGENT_WARNING_BIT_UNIT;
    u32 value = 0U;
    u32 read_value = 0U;
    /* read sram addr + offset */
    ka_mm_memcpy_fromio(&value, (g_warning_bit_addr[devid] + sram_overflow_offset + reg_offset * unit), unit);
    ka_rmb();

    value |= 1U << bit_offset;
    /* write value to addr */
    ka_mm_memcpy_toio((g_warning_bit_addr[devid] + sram_overflow_offset + reg_offset * unit), &value, unit);
    ka_wmb();

    /* check value is write sucess */
    ka_mm_memcpy_fromio(&read_value, (g_warning_bit_addr[devid] + sram_overflow_offset + reg_offset * unit), unit);
    ka_rmb();
    ts_agent_debug("set_warning_bit, writeValue=0x%x, readValue=0x%x.", value, read_value);
}
#endif


static void tsagent_set_stream_warning(u32 devid, u16 sqid)
{
    u32 bit_offset = sqid % 32U;  /* 32U: Bits/unit */
    u32 reg_offset = sqid / 32U;  /* 32U: Bits/unit */
#ifndef TS_AGENT_UT
    if (sqid >= TS_AGENT_MAX_SQ_NUM) {
        ts_agent_err("invalid sq, sqid=%u.", sqid);
        return;
    }

    if (devid >= TSAGENT_MAX_DEV_ID) {
        ts_agent_err("device id check failed, devid=%u.", devid);
        return;
    }

    if (g_warning_bit_addr[devid] == NULL) {
        ts_agent_err("g_warning_bit_addr[%u] is null.", devid);
        return;
    }
    tsagent_set_sram_overflow_bit(devid, bit_offset, reg_offset, 0U);
    tsagent_set_sram_overflow_bit(devid, bit_offset, reg_offset, 256U);
#endif
    return;
}

int tsagent_device_init(u32 devid, u32 tsid, struct trs_sqcq_agent_para *para)
{
    u64 ts_sram_rsv;
    ts_agent_info("tsagent device init, devid=%u, tsid=%u, agent_para_size=0x%llx.", devid, tsid, (u64)(para->rsv_size));
    if (devid >= TSAGENT_MAX_DEV_ID) {
        ts_agent_warn("device id check failed, devid=%u.", devid);
        return EOK;
    }
    // 16个vf和1个物理机，每个2k sram的空间，其中前512B是mailbox，vfinfo等，然后512B是host
    // stream的溢出检测，然后是device stream的溢出检测用了512B
    ts_sram_rsv = para->rsv_phy_addr + TSAGENT_WARNING_BIT_SRAM_OFFSET;
#ifndef TS_AGENT_UT
    if (dbl_get_deployment_mode() == DBL_DEVICE_DEPLOYMENT) {
        ts_agent_info("now is in device.");
        ts_sram_rsv += TSAGENT_WARNING_BIT_SRAM_OFFSET;
    }
    g_warning_bit_addr[devid] = ka_mm_ioremap(ts_sram_rsv, TSAGENT_WARNING_BIT_SIZE);
    if (g_warning_bit_addr[devid] == NULL) {
        ts_agent_err("ioremap sram fail, sram addr=0x%llx, size=%u.", ts_sram_rsv, TSAGENT_WARNING_BIT_SIZE);
        return -ENOMEM;
    }
#endif
    ts_agent_info("tsagent device init, ka_mm_ioremap_addr=0x%llx", (u64)g_warning_bit_addr[devid]);
    return EOK;
}

int tsagent_device_uninit(u32 devid, u32 tsid)
{
    if (devid >= TSAGENT_MAX_DEV_ID) {
        ts_agent_err("device id check failed, devid=%u.", devid);
        return EOK;
    }
#ifndef TS_AGENT_UT
    if (g_warning_bit_addr[devid] != NULL) {
        ka_mm_iounmap(g_warning_bit_addr[devid]);
        g_warning_bit_addr[devid] = NULL;
    }
#endif
    ts_agent_info("tsagent device uninit success, devid=%u.", devid);
    return EOK;
}
#endif

#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3_EP)
static bool is_cqe_status_syscnt(ts_stars_cqe_t *cqe)
{
    if ((cqe->evt || cqe->place_hold) && (cqe->error_bit == 0U)) {
        return true;
    }
    return false;
}

static void cqe_set_drop_flag(ts_stars_cqe_t *cqe)
{
    if (is_cqe_status_syscnt(cqe)) {
        /* syscnt cqe need to always dispatch to logic cq */
        return;
    }

    if (cqe->error_bit) {
        /* no drop, runtime need to proc error cqe */
        cqe->drop_flag = 0U;
        return;
    }

    /* dvpp or soft dvpp need check cqe in runtime */
    if ((cqe->sqe_type == TS_STARS_SQE_TYPE_VPC) || (cqe->sqe_type == TS_STARS_SQE_TYPE_JPEGE)
        || (cqe->sqe_type == TS_STARS_SQE_TYPE_JPEGD) || (cqe->sqe_type == TS_STARS_SQE_TYPE_AICPU)) {
        cqe->drop_flag = 0U;
        return;
    }

    if (cqe->warn || (cqe->sqe_type == TS_STARS_SQE_TYPE_PCIE_DMA)) {
        /* cqe has been processed in ts_agent, no need to send to runtime */
        cqe->drop_flag = 1U;
        return;
    }
    cqe->drop_flag = 0U;
    return;
}

#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
#ifndef TS_AGENT_UT
static void ts_agent_rollback_stream_task_id(bool swsq_flag, uint16_t *stream_id, uint16_t *task_id)
{
    /* 这里是Ts agent的处理逻辑, 任务类型的过滤条件, sqe_type == 0 && ffts_type = 4
     *  如果是新3包, runtime一定走的plan C, 比如普通aic报错，stream_id=6128, 匹配不上plan c, 直接返回
     *  如果是老3包, runtime一定走的plan B/A
     *      如果stream_id >= 2k, runtime业务跑不起来, 不会走到这个流程
     *      如果stream_id < 2k, 没有问题
     */
    if (swsq_flag) {
        /* Plan C */
        if (((*stream_id) >> TS_UPDATE_FOR_STREAM_EXTEND_FLAG_BIT) == TS_UPDATE_FOR_STREAM_EXTEND) {
            uint16_t tmp_task_id = (*task_id);
            (*task_id) = ((*stream_id) & TS_MASK_BIT0_BIT14) | ((*task_id) & TS_MASK_BIT15);
            (*stream_id) = tmp_task_id & TS_MASK_BIT0_BIT14;
        }
        return;
    }

    /* Plan B and Plan A will be used for old cann package */
    /* Plan B */
    if ((((*stream_id) >> TS_UPDATE_FLAG_BIT) & TS_MASK_BIT0_BIT1) == TS_UPDATE_FOR_ALL) {
        uint16_t tmp_task_id = (*task_id);
        (*task_id) = ((*stream_id) & TS_MASK_BIT0_BIT11) | ((*task_id) & TS_MASK_BIT12_BIT15);
        (*stream_id) = tmp_task_id & TS_MASK_BIT0_BIT11;
        return;
    }

    /* Plan A */
    if (((*stream_id) & TS_MASK_BIT12) != 0U) {
        (*task_id) = (((*task_id) & (TS_MASK_BIT0_BIT11 | TS_MASK_BIT12)) | ((*stream_id) & TS_MASK_BIT13_BIT15));
        (*stream_id) &= TS_MASK_BIT0_BIT11;
    }
}
#endif
#endif

int tsagent_cqe_update(u32 devid, u32 tsid, int pid, u32 cqid, void *cqe)
{
    ts_stars_cqe_t *stars_cqe = (ts_stars_cqe_t *)cqe;
#ifndef TS_AGENT_UT
#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
    uint16_t stream_id = stars_cqe->stream_id;
    uint16_t task_id = stars_cqe->task_id;
    if ((!is_cqe_status_syscnt(stars_cqe)) && (stars_cqe->sqe_type == TS_STARS_SQE_TYPE_FFTS)) {
        bool swsq_flag = tsagent_is_software_sq_version(devid, stars_cqe->sq_id);
        ts_agent_rollback_stream_task_id(swsq_flag, &(stars_cqe->stream_id), &(stars_cqe->task_id));
    }
    ts_agent_debug("stars cqe pid=%d, old stream_id=%hu, task_id=%hu, new stream_id=%hu, task_id=%hu",
                   pid, stream_id, task_id, stars_cqe->stream_id, stars_cqe->task_id);
#endif
#endif
    cqe_set_drop_flag(stars_cqe);

#ifndef CFG_SOC_PLATFORM_MINIV3_EP
    if (stars_cqe->warn != 0U) {
        ts_agent_debug("stars warning happened, devid=%u, pid=%d, stream_id=%u, task_id=%u, sqid=%u.", devid, pid,
            stars_cqe->stream_id, stars_cqe->task_id, stars_cqe->sq_id);
        tsagent_set_stream_warning(devid, stars_cqe->sq_id);
    }
#endif
#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
    ts_agent_debug("cqe debug, devid=%u, pid=%d, stream_id=%u, task_id=%u, sqid=%u, sqetype=%u, sq_head=%u, "
        "place_hold=%d, evt=%d, error_bit=%d.", devid, pid, stars_cqe->stream_id, stars_cqe->task_id,
        stars_cqe->sq_id, stars_cqe->sqe_type, stars_cqe->sq_head, stars_cqe->place_hold, stars_cqe->evt,
        stars_cqe->error_bit);
#endif

    if (stars_cqe->place_hold || stars_cqe->evt) {
        return EOK;
    }

    if (stars_cqe->sqe_type == TS_STARS_SQE_TYPE_PCIE_DMA) {
        struct svm_dma_desc_handle handle = {
            .pid = pid,
            .key = (devid << 16U) | (u32)stars_cqe->stream_id,
            .subkey = (u32)stars_cqe->task_id,
        };
#if (!defined CFG_DEVICE_ENV) && (!defined TS_AGENT_UT)
        hal_kernel_svm_dma_desc_destroy(&handle);
        if (devid < TSAGENT_MAX_DEV_ID) {
            dma_cnt[devid] = dma_cnt[devid] - 1;
        }
#endif
        ts_agent_debug("destroy dma desc, devid=%u, pid=%d, stream_id=%u, task_id=%u, sqid=%u, key=%u.", devid, pid,
            stars_cqe->stream_id, stars_cqe->task_id, stars_cqe->sq_id, handle.key);
    }
    return EOK;
}

#if ((!defined TS_AGENT_UT) && (!defined CFG_DEVICE_ENV))
static void dma_des_destroy_by_stream_id(u32 devid, int pid, u32 stream_id)
{
    if (stream_id != U32_MAX) {
        struct svm_dma_desc_handle handle = {
            .pid = pid,
            .key = (devid << 16U) | stream_id,
            .subkey = U32_MAX,
        };
        hal_kernel_svm_dma_desc_destroy(&handle);
        if (dma_cnt_max_last[devid] < dma_cnt_max[devid]) {
            dma_cnt_max_last[devid] = dma_cnt_max[devid];
            ts_agent_debug("peak value of dma desc is: dma_cnt[%u]=%ld.", devid, dma_cnt_max_last[devid]);
        }
    }
    ts_agent_debug("try to destroy stream all dma desc, devid=%u, pid=%d, stream_id=%u", devid, pid, stream_id);
}
#endif

int tsagent_mailbox_update(u32 devid, u32 tsid, int pid, void *data, u32 size)
{
    ts_mailbox_t *mb = (ts_mailbox_t *)data;
    ts_agent_debug("mb_valid=%u, mb_cmd_type=%u", mb->valid, mb->cmd_type);
    if ((mb->valid == TS_DRV_MAILBOX_VALID_VALUE) && (mb->cmd_type == RELEASE_TASK_CMD_SQCQ)) {
        u32 stream_id = mb->u.cmd_sqcq_info.info[0];
#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
        u16 sqid = mb->u.cmd_sqcq_info.sq_idx;
        tsagent_stream_id_to_sq_id_del(devid, stream_id, sqid);
        tsagent_sq_info_reset(devid, stream_id, sqid);
#endif
#if ((!defined TS_AGENT_UT) && (!defined CFG_DEVICE_ENV))
        dma_des_destroy_by_stream_id(devid, pid, stream_id);
#endif
#if ((!defined TS_AGENT_UT) && (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_HOST_VIRTUAL_MACHINES)  \
    && (!defined CFG_DEVICE_ENV))
        hal_kernel_sqe_update_desc_destroy(devid, tsid, mb->u.cmd_sqcq_info.sq_idx);
#endif
        ts_agent_debug("try to destroy stream all dma desc, devid=%u, pid=%d, stream_id=%u, sqid=%u",
            devid, pid, stream_id, mb->u.cmd_sqcq_info.sq_idx);
    }

#if ((!defined TS_AGENT_UT) && (defined CFG_SOC_PLATFORM_CLOUD_V2) && (!defined CFG_DEVICE_ENV))
    if ((mb->valid == TS_DRV_MAILBOX_VALID_VALUE) && (mb->cmd_type == FREE_RUNTIME_STREAM_ID)) {
        u32 stream_id = mb->u.free_runtime_stream_id.stream_id;
        dma_des_destroy_by_stream_id(devid, pid, stream_id);
    }
#endif

#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
    if ((mb->valid == TS_DRV_MAILBOX_VALID_VALUE) && (mb->cmd_type == CREATE_TASK_CMD_SQCQ)) {
        u32 stream_id = mb->u.cmd_sqcq_info.info[0];
        u16 sqid = mb->u.cmd_sqcq_info.sq_idx;
        bool swsq_flag = ((ts_stream_alloc_info_t *)mb->u.cmd_sqcq_info.info)->swsq_flag;
        tsagent_stream_id_to_sq_id_add(devid, stream_id, sqid);
        tsagent_sq_info_set(devid, stream_id, sqid, swsq_flag);
    }
#endif
    return EOK;
}
#endif


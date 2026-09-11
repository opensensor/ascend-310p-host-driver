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
 * Create: 2022-09-20
 */

#ifndef TS_AGENT_UPDATE_SQE_H
#define TS_AGENT_UPDATE_SQE_H

#include <linux/types.h>
#if defined(CFG_SOC_PLATFORM_KPSTARS)
#include "ts_agent_kp_update_sqe.h"
#endif

#ifndef TS_AGENT_UT
#include "trs_adapt.h"
#else
struct trs_sqcq_agent_para {
    phys_addr_t rsv_phy_addr;
    size_t      rsv_size;
};
#endif

struct tsagent_dev_base_info {
    u32 chip_id;
    u32 die_id;
    u32 addr_mode;
    u32 soc_type;
};

struct tsagent_sq_base_info {
    u8 swsq_flag : 1;
    u8 rsv : 7;
};

#define TSAGENT_WARNING_BIT_SRAM_OFFSET    0x200
#define TSAGENT_WARNING_BIT_SIZE           512
#define TSAGENT_WARNING_BIT_UNIT           4

#if defined(CFG_HOST_ENV) || defined (TS_AGENT_UT)
#define TSAGENT_MAX_DEV_ID                 1124 /* devid: 100 + 64*16 */
#endif

#ifdef CFG_DEVICE_ENV
#define TSAGENT_MAX_DEV_ID                 64 /* devid: 0 : 63 */
#endif

int tsagent_device_init(u32 devid, u32 tsid, struct trs_sqcq_agent_para *para);
int tsagent_device_uninit(u32 devid, u32 tsid);
int tsagent_sqe_update(u32 devid, u32 tsid, int pid, u32 sqid, void *sqe_buf);
int tsagent_cqe_update(u32 devid, u32 tsid, int pid, u32 cqid, void *cqe);
int tsagent_mailbox_update(u32 devid, u32 tsid, int pid, void *data, u32 size);
int tsagent_stream_id_to_sq_id_init(void);
void tsagent_stream_id_to_sq_id_uninit(void);
void tsagent_dev_base_info_init(void);
void tsagent_sq_base_info_init(void);
void tsagent_sq_info_set(u32 devid, u32 stream_id, u16 sqid, bool swsq_flag);
void tsagent_sq_info_reset(u32 devid, u32 stream_id, u16 sqid);
bool tsagent_is_software_sq_version(u32 devid, u16 sqid);
void tsagent_stream_id_to_sq_id_add(u32 devid, u32 stream_id, u16 sqid);
void tsagent_stream_id_to_sq_id_del(u32 devid, u32 stream_id, u16 sqid);
bool tsagent_sq_is_belong_to_stream(u32 devid, u16 stream_id, u32 sqid);
int tsagent_sqe_update_check_sqe_type(u32 devid, int pid, ts_stars_sqe_t *sqe);
void init_task_convert_func(void);
void tsagent_set_sram_overflow_bit(u32 devid, u32 bit_offset, u32 reg_offset, u32 sram_overflow_offset);
struct tsagent_dev_base_info* tsagent_get_device_base_info(u32 dev_id);

#endif // TS_AGENT_DVPP_H

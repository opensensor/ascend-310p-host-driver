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

#ifndef ESCHED_DRV_ADAPT_H
#define ESCHED_DRV_ADAPT_H

#include "esched.h"

#define HOST_VF_DEVID_START 100
#define DEVICE_VF_DEVID_START 32U
#define MAX_VF_NUM_PER_DEVICE 16U

#define PID_MAP_MASK 0x80000000U
#define MB_PID_MASK 0x7FFFFFFFU
#define TOPIC_EVENT_QUEUE_LIMIT 32U

u32 esched_drv_get_node_aicpu_chan_mask(u32 start_id, u32 aicpu_chan_num);
u32 esched_get_cpuid_in_node(u32 cpuid);
u32 esched_get_cpuid_in_os(u32 chip_id, u32 cpuid_in_node);

int esched_hw_dev_init(struct sched_numa_node *node);
void esched_hw_dev_uninit(struct sched_numa_node *node);

int esched_drv_init_comm_pid_mapping(u32 node_id);
void esched_drv_uninit_comm_pid_mapping(u32 node_id);

int esched_drv_device_aicpu_chan_init(u32 chip_id);
void esched_drv_device_aicpu_chan_uninit(u32 chip_id);

int esched_drv_conf_sched_cpu(struct sched_numa_node *node, u32 sched_cpu_num);
u32 esched_get_devid_from_hw_vfid(u32 chip_id, u32 vfid, u32 sub_dev_num);
u32 esched_get_hw_vfid_from_devid(u32 dev_id);
u32 esched_get_chipid_from_devid(u32 dev_id);
bool esched_is_phy_dev(u32 dev_id);
int esched_drv_config_pid(struct sched_proc_ctx *proc_ctx, u32 identity, devdrv_host_pids_info_t *pids_info);
void esched_drv_del_pid(struct sched_proc_ctx *proc_ctx, u32 identity);

void esched_drv_cpu_report(struct topic_data_chan *topic_chan, u32 error_code, u32 status);
bool esched_drv_is_mb_valid(struct topic_data_chan *topic_chan);
void esched_drv_get_status_report(struct topic_data_chan *topic_chan, u32 status);
bool esched_drv_is_get_mb_valid(struct topic_data_chan *topic_chan);
int esched_cpu_port_submit_task(struct topic_data_chan *topic_chan, void *sqe, u32 timeout);
void esched_drv_init_aicpu_pool(struct sched_numa_node *node, u32 start_id, u32 aicpu_chan_num);
void esched_drv_uninit_aicpu_pool(struct sched_hard_res *res);
void esched_drv_reset_pool(struct sched_hard_res *res);
void esched_drv_init_non_aicpu_pool(struct sched_hard_res *res);

/* use for esched_drv_mia.c */
void esched_drv_reset_phy_dev(u32 devid);
void esched_drv_restore_phy_dev(u32 devid);
int esched_drv_init_topic_table(u32 chip_id, u32 identity);
void esched_drv_uninit_topic_table(u32 chip_id, u32 identity);

void esched_drv_mb_intr_enable(struct topic_data_chan *topic_chan);

/* use for esched_mia_msg.c esched_sia_msg.c */
u64 esched_drv_get_host_ccpu_mask(u32 pool_id);
#endif

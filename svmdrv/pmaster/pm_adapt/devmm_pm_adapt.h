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

#ifndef DEVMM_PM_ADAPT_H
#define DEVMM_PM_ADAPT_H

#include "devmm_proc_info.h"
#include "svm_cmd.h"
#include "virtmng_interface.h"

/* vm pid map pm host pid as (vmid + pid), map device host pid as (devid + fid + pid)
   each device and vm, pm have different host pid */
#define DEV_HOSTPID_DEVID_OFFSET 24
#define DEV_HOSTPID_DEVID_MASK 0xff
#define DEV_HOSTPID_FID_OFFSET 16
#define DEV_HOSTPID_FID_MASK 0xff
#define DEV_HOSTPID_PID_OFFSET 0
#define DEV_HOSTPID_PID_MASK 0xffff

/* for vm safety check */
struct devmm_pm_copy_res {
    ka_list_head_t list;
    struct devmm_copy_res *res; /* in VM, for link all the res in one ioctl */
};

struct devmm_pm_convert_para {
    ka_list_head_t list_head;
    u64 convert_id;
    u32 dev_id;     /* for destroy all convert */
    u32 fid;        /* for destroy all convert */
    u64 height;
    u64 len;
    u32 dma_node_num;
    struct devmm_pm_copy_res *pm_res;
};

struct devmm_pm_convert_node {
    ka_hlist_node_t res_hlist;
    struct devmm_pm_convert_para pm_convrt;
};

static inline u32 devmm_get_dev_host_pid(u32 dev_id, u32 fid, u32 vm_pid)
{
    u32 host_pid = (vm_pid & DEV_HOSTPID_PID_MASK) << DEV_HOSTPID_PID_OFFSET;
    host_pid |= (fid & DEV_HOSTPID_FID_MASK) << DEV_HOSTPID_FID_OFFSET;
    host_pid |= (dev_id & DEV_HOSTPID_DEVID_MASK) << DEV_HOSTPID_DEVID_OFFSET;
    return host_pid;
}

static inline u32 devmm_get_devid_from_dev_host_pid(u32 host_pid)
{
    return (host_pid >> DEV_HOSTPID_DEVID_OFFSET) & DEV_HOSTPID_DEVID_MASK;
}

static inline u32 devmm_get_fid_from_dev_host_pid(u32 host_pid)
{
    return (host_pid >> DEV_HOSTPID_FID_OFFSET) & DEV_HOSTPID_FID_MASK;
}

static inline u32 devmm_get_pid_from_dev_host_pid(u32 host_pid)
{
    return (host_pid >> DEV_HOSTPID_PID_OFFSET) & DEV_HOSTPID_PID_MASK;
}

#define PM_HOSTPID_VMID_OFFSET 16
#define PM_HOSTPID_VMID_MASK 0xffff
#define PM_HOSTPID_PID_OFFSET 0
#define PM_HOSTPID_PID_MASK 0xffff

static inline u32 devmm_get_pm_host_pid(u32 vmid, u32 vm_pid)
{
    u32 host_pid = (vm_pid & PM_HOSTPID_PID_MASK) << PM_HOSTPID_PID_OFFSET;
    host_pid |= (vmid & PM_HOSTPID_VMID_MASK) << PM_HOSTPID_VMID_OFFSET;
    return host_pid;
}

static inline u32 devmm_get_vmid_from_pm_host_pid(u32 host_pid)
{
    return (host_pid >> PM_HOSTPID_VMID_OFFSET) & PM_HOSTPID_VMID_MASK;
}

static inline u32 devmm_get_pid_from_pm_host_pid(u32 host_pid)
{
    return (host_pid >> PM_HOSTPID_PID_OFFSET) & PM_HOSTPID_PID_MASK;
}

void devmm_clear_pm_host_addr(struct devmm_copy_res *res);

int devmm_pm_enable_device(struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_disable_device(struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_check_memcpy_d2d_para(struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_channel_msg_default_pre_proc(struct devmm_svm_process *svm_proc, u32 dev_id, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info);
void devmm_pm_release_vm_agent_proc(struct devmm_svm_process *svm_proc);
void devmm_pm_release_work_init(struct devmm_svm_process *svm_proc);

int devmm_pm_init_process(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_release_process(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_create_device_page(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_del_device_page_cache(struct devmm_svm_process *svm_proc,
    u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_memcpy_process_res(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_convert_addr_proc(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_convert_dma_addr_proc(struct devmm_svm_process *svm_proc, u32 fid,
    struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_destroy_addr_proc(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_translate_addr_proc(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_clear_translate_addr_proc(struct devmm_svm_process *svm_proc,
    u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_page_fault_h2d_sync(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int devmm_pm_agent_mem_map(struct devmm_svm_process *svm_proc, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
bool devmm_pm_is_vm_scene(void *msg);
int devmm_pm_chan_msg_dispatch(void *msg, u32 in_data_len, u32 out_data_len, u32 *ack_len);
void devmm_pm_increase_convert_free_id(u32 dev_id, u32 fid, u64 height);

#endif


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

#ifndef DEVMM_PROC_MEM_COPY_H
#define DEVMM_PROC_MEM_COPY_H

#include "ka_hashtable_pub.h"
#include "devmm_proc_info.h"
#include "svm_cmd.h"
#include "virtmng_interface.h"
#include "devdrv_interface.h"
#include "devmm_addr_mng.h"

#define CONVERT_INFO_TO_DEVICE 0
#define CONVERT_INFO_FROM_DEVICE 1
#define DEVMM_WRITED_MAGIC_WORD 0xAA55AA55
#define DEVMM_READED_MAGIC_WORD 0xBB33BB33
#define DEVMM_FIN_MAGIC_WORD 0x55FF55FF

/* devmm_memcpy_proc api dma mode */
enum {
    DEVMM_CPY_SYNC_MODE = 0,  /* last task is sync, other task is async */
    DEVMM_CPY_ASYNC_MODE = 1, /* all task is async */
    DEVMM_CPY_ASYNC_API_MODE = 2, /* just convert addr, caller add status dma node add submit dma task */
    DEVMM_CPY_CONVERT_MODE = 3 /* all task is convert addr */
};

enum {
    DEVMM_DMA_SYNC_MODE = 0,  /* last task is sync, other task is async */
    DEVMM_DMA_ASYNC_MODE = 1 /* all task is async */
};

enum {
    DEVMM_DMA_ASYNC_MODE_CHANNEL = 1, /* all task is async use channel 1 */
    DEVMM_DMA_CONVERT_MODE_CHANNEL = 2 /* all task is convert use channel 2 */
};

#define DMA_COPY_TASK_WAIT_SLEEP_MIN_TIME 1000 /* us */
#define DMA_COPY_TASK_WAIT_SLEEP_MAX_TIME 2000 /* us */
#define DMA_COPY_TASK_WAIT_TIMES 120000

#define DMA_LINK_PREPARE_TRY_TIMES 8

#define DEVMM_DMA_COPY_TASK_TIMEOUT 60000ul /* max 60 second */

struct translate_offset_data {
    u64 pa;
    u64 continuty_len; /* Continuous available physical address length after pa, used by TS to verify */
};

bool devmm_is_same_dev(u32 src_devid, u32 dst_devid);
u32 devmm_get_vfid_from_dev_id(struct devmm_memory_attributes *attr);
int devmm_ioctl_memcpy_process_res(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr);

int devmm_ioctl_convert_addr_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr);
int devmm_convert_addr_process(struct devmm_svm_process *svm_process, struct devmm_mem_convrt_addr_para *convrt_para,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr, struct devmm_copy_res *res);
int devmm_ioctl_destroy_addr_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_ioctl_destroy_addr_proc_inner(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
void devmm_destroy_all_convert_dma_addr(struct devmm_svm_process *svm_proc);
void devmm_destroy_all_convert_dma_addr_inner(struct devmm_svm_process *svm_proc);
struct devmm_copy_res *devmm_alloc_copy_res(u64 byte_count,
    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr, bool is_need_clear);
void devmm_free_copy_mem(struct devmm_copy_res *res);
int devmm_destroy_dma_addr(struct DMA_ADDR *dma_addr);
int devmm_ioctl_memcpy_process(struct devmm_svm_process *svm_proc,
                               struct devmm_mem_copy_convrt_para *para,
                               struct devmm_memory_attributes *src_attr,
                               struct devmm_memory_attributes *dst_attr);
int devmm_set_translate_pa_addr_to_device_inner(struct devmm_svm_process *svm_proc,
    struct devmm_translate_info trans, u64 *pa_offset);
int devmm_set_translate_pa_addr_to_device(struct devmm_svm_process *svm_process,
    struct devmm_translate_info trans, u64 *pa_offset);
int devmm_clear_translate_pa_addr_inner(u32 dev_id, u32 vfid, u64 va, u64 len, u32 host_pid);
int devmm_clear_translate_pa_addr(u32 dev_id, u32 vfid, u64 va, u64 len, u32 host_pid);
void devmm_wait_task_finish(u32 dev_id, ka_atomic_t *finish_num, int submit_num);
void devmm_task_del(struct devmm_dma_copy_task *copy_task);
int devmm_convert2d_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para);
int devmm_convert2d_proc_inner(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg,
    struct devmm_mem_convrt_addr_para *convrt_para);
int devmm_cpy_result_refresh(struct devmm_svm_process *svm_proc,
    struct devmm_mem_async_copy_para *async_copy_para);
void devmm_async_cpy_inc_addr_ref(struct devmm_svm_process *svm_proc, u64 src, u64 dst, u64 size);
int devmm_mem_dma_cpy_process(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_convrt_para *para);
int devmm_sumbit_convert_dma_proc(struct devmm_svm_process *svm_proc, struct DMA_ADDR *dma_addr, int sync_flag);
int devmm_wait_convert_dma_result(struct devmm_svm_process *svm_proc, struct DMA_ADDR *dma_addr);
int devmm_copy_one_convert_addr_info(u32 dev_id, u32 index, u32 dir, struct devmm_share_memory_data *data);
void devmm_clear_one_convert_dma_addr(u32 dev_id, u32 vfid, u32 index);
void devmm_clear_host_pa_node_list(int pin_flg, struct devmm_copy_side *side);
void devmm_pa_node_list_dma_unmap(u32 dev_id, struct devmm_copy_side *side);
int devmm_svm_addr_pa_list_get(struct devmm_svm_process *svm_proc, u64 va, u64 size,
    struct devmm_copy_side *side);
int devmm_get_non_svm_addr_pa_list(struct devmm_svm_process *svm_proc, u64 va, u64 size,
    struct devmm_copy_side *side, int write);
int devmm_pa_node_list_dma_map(u32 dev_id, struct devmm_copy_side *side);

#ifdef EMU_ST
int devmm_get_bar_addr_info(u32 devid, u64 device_phy_addr, u64 *base_addr, size_t *size);
#endif

void devmm_free_raw_dmanode_list(struct devmm_copy_res *res);
int devmm_vm_pa_to_pm_pa(u32 devid, u64 *paddr, u64 num, u64 *out_paddr, u64 out_num);
int devmm_vm_pa_blks_to_pm_pa_blks(u32 devid, struct devmm_dma_block *blks, u64 num, struct devmm_dma_block *out_blks);

#endif

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

#ifndef _VHDC_CORE_H_
#define _VHDC_CORE_H_

#include "virtmng_interface.h"
#include "hdcdrv_core_com.h"

#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"

#define VHDC_MSG_SEND_NOBLOCK 0
#define VHDC_MSG_SEND_BLOCK 1

#define VHDC_INVALID_DEV_ID (unsigned int)(-1)
#define VHDC_KERNEL_WITHOUT_CTX (struct vhdca_vctx *)-4

struct vhdca_vctx {
    ka_file_t *file;
    u32 dev_id;
    long long pid;
    unsigned long long hash;
    struct hdcdrv_ctx_fmem ctx_fmem;
    struct hdcdrv_ctx_fmem abnormal_ctx_fmem;
};

long vhdca_ioctl(ka_file_t *file, unsigned int cmd, unsigned long arg);
int vhdca_open(ka_inode_t *node, ka_file_t *file);
int vhdca_release(ka_inode_t *node, ka_file_t *file);
void vhdca_com_msg_init(u32 dev_id, u32 fid, int status);
int vhdca_com_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
int vhdca_vpc_init(void);
int vhdca_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info);
long vhdca_cmd_operation(struct vhdca_vctx *vctx, u32 cmd, struct vhdc_ioctl_msg *iomsg);
bool vhdca_cmd_block_flag(unsigned int cmd);
u64 vhdca_hash_gen(u64 va, u64 pid);
extern long vhdca_fast_alloc_mem(struct vhdca_vctx *vctx, struct hdcdrv_cmd_alloc_mem *cmd);

extern long hdccom_fast_alloc_mem(void *ctx, struct hdcdrv_cmd_alloc_mem *cmd,
    struct hdcdrv_fast_node **f_node_ret);

extern long hdcdrv_fast_dma_map(const struct hdcdrv_cmd_dma_map *cmd);
extern long hdcdrv_fast_dma_unmap(const struct hdcdrv_cmd_dma_unmap *cmd);
extern long hdcdrv_fast_dma_remap(const struct hdcdrv_cmd_dma_remap *cmd);
extern long hdcdrv_dev_para_check(int dev_id, int service_type);
extern void hdcdrv_fast_mem_uninit(ka_task_spinlock_t *lock, ka_rb_root_t *root, int reset, int flag);
extern void hdcdrv_fast_mem_arry_uninit(void);
struct hdcdrv_fast_node *hdcdrv_fast_node_search_timeout(ka_task_spinlock_t *lock, ka_rb_root_t *root,
    u64 hash_va, int timeout);
extern int hdcdrv_fast_node_insert(ka_task_spinlock_t *lock,
    ka_rb_root_t *root, struct hdcdrv_fast_node *fast_node, u64 search_type);
struct hdcdrv_fast_node *hdcdrv_fast_node_search_from_new_tree(u32 rb_side,
    int timeout, struct hdcdrv_fast_node_msg_info *node_info);
extern int hdcdrv_fast_node_insert_new_tree(int devid,
    u64 pid, u32 fid, u32 rb_side, struct hdcdrv_fast_node *new_node);
extern void hdcdrv_fast_node_erase_from_new_tree(u64 pid,
    u32 fid, int devid, u32 rb_side, struct hdcdrv_fast_node *fast_node);
extern void hdcdrv_fast_node_erase(ka_task_spinlock_t *lock,
    ka_rb_root_t *root, struct hdcdrv_fast_node *fast_node);
extern void hdcdrv_fast_node_free(const struct hdcdrv_fast_node *fast_node);

long vhdca_cmd_deliver(struct vhdca_vctx *vctx, unsigned int cmd, struct vhdc_ioctl_msg *iomsg);


#endif /* _VHDC_CORE_H_ */

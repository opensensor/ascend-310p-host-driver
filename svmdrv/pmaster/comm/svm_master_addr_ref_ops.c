/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-08-24
 */

#include "svm_master_addr_ref_ops.h"

static void devmm_get_alloc_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_alloc_para *para = &arg->data.alloc_svm_para;

    info->num = 1;
    info->va[0] = para->p;
    info->size[0] = para->size;
}

static void devmm_get_free_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_free_pages_para *para = &arg->data.free_pages_para;

    info->num = 1;
    info->va[0] = para->va;
    info->size[0] = 1;
}

static void devmm_get_memcpy_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_copy_para *para = &arg->data.copy_para;

    info->num = 2;  /* 2 addr */
    info->va[0] = para->dst;
    info->size[0] = para->ByteCount;
    info->va[1] = para->src;
    info->size[1] = para->ByteCount;
}

static void devmm_get_memcpy2d_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_copy2d_para *para = &arg->data.copy2d_para;

    info->num = 2;  /* 2 addr */
    info->va[0] = para->dst;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
    info->va[1] = para->src;
    info->size[1] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
}

static void devmm_get_async_cpy_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_async_copy_para *para = &arg->data.async_copy_para;

    info->num = 2;  /* 2 addr */
    info->va[0] = para->dst;
    info->size[0] = para->byte_count;
    info->va[1] = para->src;
    info->size[1] = para->byte_count;
}

static void devmm_get_convert_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_convrt_addr_para *para = &arg->data.convrt_para;

    /* If 1d's len out of limit size, will goto 2d, 2d's destroy will vmma dec by height. */
    info->num = 2;  /* 2 addr */
    info->va[0] = para->pSrc;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
    info->va[1] = para->pDst;
    info->size[1] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
}

static void devmm_get_advise_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_advise_para *para = &arg->data.advise_para;

    info->num = 1;
    info->va[0] = para->ptr;
    info->size[0] = para->count;
}

static void devmm_get_prefetch_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_advise_para *para = &arg->data.prefetch_para;

    info->num = 1;
    info->va[0] = para->ptr;
    info->size[0] = para->count;
}

static void devmm_get_memset_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_memset_para *para = &arg->data.memset_para;

    info->num = 1;
    info->va[0] = para->dst;
    info->size[0] = para->count;
}

static void devmm_get_translate_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_translate_para *para = &arg->data.translate_para;

    info->num = 1;
    info->va[0] = para->vptr;
    info->size[0] = 1;      /* MEM_RESERVE_VAL not support continuty phy addr, other attr only set fist page ref */
}

static void devmm_get_ipc_open_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_ipc_open_para *para = &arg->data.ipc_open_para;

    info->num = 1;
    info->va[0] = para->vptr;
    info->size[0] = 1;      /* vptr is MEM_SVM_VAL, only set first page ref */
}

static void devmm_get_ipc_close_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_ipc_close_para *para = &arg->data.ipc_close_para;

    info->num = 1;
    info->va[0] = para->vptr;
    info->size[0] = 1;      /* vptr is MEM_SVM_VAL, only set first page ref */
}

static void devmm_get_ipc_create_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_ipc_create_para *para = &arg->data.ipc_create_para;

    /*
     * Ipc create set ADD_SUB_REF, halMemFree will check bitmap if ipc_create.
     * For reserve addr, if ipc create, should ipc destroy before halMemUnmap.
     */
    info->num = 1;
    info->va[0] = para->vptr;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
}

static void devmm_get_remote_map_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_remote_map_para *para = &arg->data.map_para;

    info->num = 1;
    info->va[0] = para->src_va;
    info->size[0] = para->size;
}

static void devmm_get_remote_unmap_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_remote_unmap_para *para = &arg->data.unmap_para;

    info->num = 1;
    info->va[0] = para->src_va;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
}

static void devmm_get_mem_map_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_map_para *para = &arg->data.mem_map_para;

    info->num = 1;
    info->va[0] = para->va;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;   /* Will create vmma, shouldn't vmma_occupy_inc */
}

static void devmm_get_mem_unmap_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_unmap_para *para = &arg->data.mem_unmap_para;

    info->num = 1;
    info->va[0] = para->va;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;  /* Will destroy vmma, shouldn't vmma_occupy_inc */
}

static void devmm_get_register_dma_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_register_dma_para *para = &arg->data.register_dma_para;

    info->num = 1;
    info->va[0] = para->vaddr;
    info->size[0] = para->size;
}

static void devmm_get_unregister_dma_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_unregister_dma_para *para = &arg->data.unregister_dma_para;

    info->num = 1;
    info->va[0] = para->vaddr;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
}

static void devmm_get_mem_repair_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_mem_repair_para *para = &arg->data.mem_repair_para;
    u32 i;

    info->num = para->count < DEVMM_COMMON_PARA_VA_NUM ? para->count : DEVMM_COMMON_PARA_VA_NUM;
    for (i = 0; i < info->num; i++) {
        info->va[i] = para->repair_addrs[i].ptr;
        info->size[i] = para->repair_addrs[i].len;
    }
}

static void devmm_get_reserve_addr_info(struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info)
{
    struct devmm_resv_addr_info_query_para *para = &arg->data.resv_addr_info_query_para;

    info->num = 1;
    info->va[0] = para->va;
    info->size[0] = SVM_ADDR_REF_OPS_UNKNOWN_SIZE;
}

static void (*get_ioctl_addr_info[DEVMM_SVM_CMD_MAX_CMD])
    (struct devmm_ioctl_arg *arg, struct devmm_ioctl_addr_info *info) = {
        [_KA_IOC_NR(DEVMM_SVM_ALLOC)] = devmm_get_alloc_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_FREE_PAGES)] = devmm_get_free_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEMCPY)] = devmm_get_memcpy_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEMCPY2D)] = devmm_get_memcpy2d_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_ASYNC_MEMCPY)] = devmm_get_async_cpy_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_CONVERT_ADDR)] = devmm_get_convert_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_ADVISE)] = devmm_get_advise_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_PREFETCH)] = devmm_get_prefetch_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEMSET8)] = devmm_get_memset_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_TRANSLATE)] = devmm_get_translate_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_OPEN)] = devmm_get_ipc_open_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_CLOSE)] = devmm_get_ipc_close_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_IPC_MEM_CREATE)] = devmm_get_ipc_create_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEM_REMOTE_MAP)] = devmm_get_remote_map_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEM_REMOTE_UNMAP)] = devmm_get_remote_unmap_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEM_MAP)] = devmm_get_mem_map_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEM_UNMAP)] = devmm_get_mem_unmap_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_REGISTER_DMA)] = devmm_get_register_dma_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_UNREGISTER_DMA)] = devmm_get_unregister_dma_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_MEM_REPLAIR)] = devmm_get_mem_repair_addr_info,
        [_KA_IOC_NR(DEVMM_SVM_RESERVE_ADDR_INFO_QUERY)] = devmm_get_reserve_addr_info,
};

int devmm_get_ioctl_addr_info(struct devmm_ioctl_arg *arg, u32 cmd_id, struct devmm_ioctl_addr_info *info)
{
    if (get_ioctl_addr_info[cmd_id] != NULL) {
        get_ioctl_addr_info[cmd_id](arg, info);
        info->cmd_id = cmd_id;
        return 0;
    } else {
        devmm_drv_err("Cmd need to adapt get_addr_info func. (cmd_id=%u)\n", cmd_id);
        return -EINVAL;
    }
}


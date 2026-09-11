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
 * Create: 2023-08-21
 */
#include "svm_vmma_mng.h"
#include "devmm_channel.h"
#include "devmm_page_cache.h"
#include "svm_msg_client.h"
#include "svm_master_mem_map.h"

static void devmm_msg_to_agent_mem_map_fail_proc(struct devmm_svm_process *svm_proc,
    struct devmm_vmma_info *info, u64 to_free_pg_num)
{
    u64 num = info->pg_num;
    u64 size = info->size;

    info->size = to_free_pg_num * info->pg_size;
    info->pg_num = to_free_pg_num;
    (void)devmm_msg_to_agent_mem_unmap(svm_proc, info);
    info->pg_num = num;
    info->size = size;
}

int devmm_msg_to_agent_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    struct devmm_pages_cache_info cache_info = {0};
    struct devmm_chan_mem_map *msg = NULL;
    u64 mapped_num, num, pg_num_per_msg, msg_len;
    int ret;

    pg_num_per_msg = ka_base_min(info->pg_num, DEVMM_MEM_MAP_MAX_PAGE_NUM_PER_MSG);
    msg_len = sizeof(struct devmm_chan_mem_map) + sizeof(struct devmm_chan_query_phy_blk) * pg_num_per_msg;

    msg = devmm_kvzalloc_ex(msg_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (msg == NULL) {
        devmm_drv_err("Kvzalloc failed. (msg_len=%llu)\n", msg_len);
        return -ENOMEM;
    }

    msg->head.msg_id = DEVMM_CHAN_MEM_MAP_H2D_ID;
    msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg->head.process_id.vfid = (u16)info->vfid;
    msg->head.dev_id = (u16)info->devid;
    msg->module_id = info->module_id;
    msg->pg_type = info->pg_type;
    msg->phy_addr_blk_id = info->phy_addr_blk_id;
    msg->phy_addr_blk_pg_num = info->phy_addr_blk_pg_num;
    msg->dma_blk_id = 0;
    msg->dma_blk_pg_id = 0;
    for (mapped_num = 0; mapped_num < info->pg_num; mapped_num += num) {
        /* Attention, msg->head.extend_num * page_size should be multiples of DEVMM_VMMA_GRANULARITY_SIZE */
        /* Every agent map msg will create vmma, so map per page num should equal with unmap */
        num = ka_base_min(info->pg_num - mapped_num, DEVMM_MEM_MAP_MAX_PAGE_NUM_PER_MSG);

        msg->head.extend_num = (u16)num; // max DEVMM_MEM_MAP_MAX_PAGE_NUM_PER_MSG
        msg->va = info->va + mapped_num * info->pg_size;
        msg->size = num * info->pg_size;
        msg->offset_pg_num = mapped_num;
        msg->get_next_dma_blk_pg_id = ((mapped_num + num) < info->pg_num) ? 1 : 0;
        ret = devmm_chan_msg_send(msg, (u32)sizeof(struct devmm_chan_mem_map), (u32)msg_len);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_no_err_if((ret == -ENOMEM), "Mem map msg send failed. (ret=%d; va=0x%llx; size=%llu)\n",
                ret, msg->va, msg->size);
#endif
            devmm_msg_to_agent_mem_map_fail_proc(svm_proc, info, mapped_num);
            devmm_kvfree_ex(msg);
            return ret;
        }

        cache_info.va = msg->va;
        cache_info.pg_num = num;
        cache_info.pg_size = info->pg_size;
        cache_info.blks = msg->blks;
        devmm_pages_cache_set(svm_proc, info->page_insert_dev_id, &cache_info);
    }

    devmm_kvfree_ex(msg);
    return 0;
}

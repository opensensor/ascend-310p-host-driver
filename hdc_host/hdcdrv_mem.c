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

#include "hdcdrv_core.h"
#include "hdcdrv_mem.h"

#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_compiler_pub.h"

ka_delayed_work_t *hdcdrv_get_recycle_mem(void)
{
    return &(hdc_ctrl->recycle_mem);
}

ka_device_t* hdcdrv_get_pdev_dev(int dev_id)
{
    if (hdc_ctrl->devices[dev_id].valid != HDCDRV_VALID) {
        hdcdrv_err("Device is not ready. (dev_id=%d)\n", dev_id);
        return NULL;
    }
    return hdc_ctrl->devices[dev_id].dev;
}

/* 1 fast alloc/free/map        -> uni -> ctrl.lo
*  2 add/del                    -> sep -> ctrl.dev[id].lo/re
*  3 send/rx src & pm(fid == 0) -> uni -> ctrl.lo
*  4 send/rx/dma-dst            -> sep -> ctrl.dev[id].lo/re
*/
struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_uni(void)
{
    return &(hdc_ctrl->fmem);
}

struct hdcdrv_node_tree_ctrl *hdcdrv_get_node_tree(void)
{
    return hdc_node_tree;
}

struct hdcdrv_node_tree_info *hdcdrv_get_node_tree_info(int idx, u32 rb_side)
{
    if (rb_side == HDCDRV_RBTREE_SIDE_LOCAL) {
        return &(hdc_node_tree->node_tree[idx].local_tree.tree_info);
    } else {
        return &(hdc_node_tree->node_tree[idx].re_tree.tree_info);
    }
}

struct hdcdrv_dev_fmem *hdcdrv_get_ctrl_arry_uni_ex(int idx, int devid, u32 side)
{
    if (side == HDCDRV_RBTREE_SIDE_LOCAL) {
        return &(hdc_node_tree->node_tree[idx].local_tree.fmem);
    } else {
        return &(hdc_node_tree->node_tree[idx].re_tree.devices[devid]);
    }
}

struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_sep(int devid)
{
    return &(hdc_ctrl->devices[devid].fmem);
}

struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_ex(int devid, u32 fid, u32 side)
{
#ifdef HDC_ENV_DEVICE
    if (side == HDCDRV_RBTREE_SIDE_LOCAL) {
#else
    if ((side == HDCDRV_RBTREE_SIDE_LOCAL) && (fid == 0)) {
#endif
        return &(hdc_ctrl->fmem);
    } else {
        return &(hdc_ctrl->devices[devid].fmem);
    }
}

void hdcdrv_iova_fmem_unmap(u32 dev_id, u32 fid, struct hdcdrv_fast_mem* f_mem, u32 num)
{
#ifdef CFG_FEATURE_VFIO
    u32 i;

    for (i = 0; i < num; i++) {
        if (f_mem->mem[i].dma_sgt != NULL) {
            vmngh_dma_unmap_guest_page(dev_id, fid, f_mem->mem[i].dma_sgt);
        }
    }
#endif
}

#ifdef CFG_FEATURE_VFIO
static void hdcdrv_iova_new_mem(u32 dev_id, u32 fid, u32 old_num, struct hdcdrv_mem_f old_mem[],
    struct hdcdrv_mem_f new_mem[])
{
    u32 i, j;
    u32 num_new = 0;
    ka_scatterlist_t *sgl = NULL;
    u32 len_per_sgt;

    for (i = 0; i < old_num; i++) {
        sgl = old_mem[i].dma_sgt->sgl;
        len_per_sgt = 0;

        for (j = 0; j < old_mem[i].dma_sgt->nents; j++) {
            if (sgl == NULL) {
                break;
            }
            /*lint -e679 */
            /* Adding offset in PAGE, in case of PM KA_MM_PAGE_SIZE is larger than VM */
            new_mem[num_new + j].addr = ka_mm_sg_dma_address(sgl) + (old_mem[i].addr & (KA_MM_PAGE_SIZE - 1));
            new_mem[num_new + j].len = ka_mm_sg_dma_len(sgl);
            new_mem[num_new + j].dma_sgt = (j == 0) ? (old_mem[i].dma_sgt) : (NULL);
            len_per_sgt = len_per_sgt + new_mem[num_new + j].len;
            /*lint +e679 */
            sgl = ka_base_sg_next(sgl);
        }

        num_new = num_new + old_mem[i].dma_sgt->nents;

        if (len_per_sgt != old_mem[i].len) {
            hdcdrv_warn("sgt_len not match. (dev_id=%u; fid=%u; mem=%u; len=%u; sgt=%u)\n",
                dev_id, fid, i, old_mem[i].len, len_per_sgt);
        }
    }
}
#endif

int hdcdrv_iova_fmem_map(u32 dev_id, u32 fid, struct hdcdrv_fast_mem* f_mem)
{
#ifdef CFG_FEATURE_VFIO
    ka_dma_addr_t dma_addr;
    u32 i, unmap_num;
    int num_new;
    struct hdcdrv_mem_f *new_mem = NULL;
    u32 new_mem_size;
    ka_sg_table_t *dma_sgt = NULL;

    num_new = 0;
    for (i = 0; i < (u32)f_mem->phy_addr_num; i++) {
        dma_addr = vmngh_dma_map_guest_page(dev_id, fid, f_mem->mem[i].addr, f_mem->mem[i].len, &dma_sgt);
        if ((dma_sgt == NULL) || (dma_addr == DMA_MAP_ERROR)) {
            hdcdrv_err("Calling vmngh_dma_map_guest_page failed. (dev=%u; fid=%u)\n", dev_id, fid);
            unmap_num = i;
            goto MAP_FAIL;
        }

        if ((num_new + dma_sgt->nents) > HDCDRV_MEM_MAX_PHY_NUM) {
            hdcdrv_err("New physic number too big. (dev=%u; fid=%u)\n", dev_id, fid);
            unmap_num = i;
            goto MAP_FAIL;
        }

        num_new = num_new + (int)dma_sgt->nents;
        f_mem->mem[i].dma_sgt = dma_sgt;
    }

    new_mem_size = (u32)num_new * sizeof(struct hdcdrv_mem_f);
    new_mem = hdcdrv_kvmalloc(new_mem_size, KA_SUB_MODULE_TYPE_2);
    if (new_mem == NULL) {
        unmap_num = (u32)f_mem->phy_addr_num;
        hdcdrv_err("Calling alloc failed. (dev=%u; fid=%u)\n", dev_id, fid);
        goto MAP_FAIL;
    }

    hdcdrv_iova_new_mem(dev_id, fid, (u32)f_mem->phy_addr_num, f_mem->mem, new_mem);

    hdcdrv_kvfree(f_mem->mem, KA_SUB_MODULE_TYPE_2);
    f_mem->mem = new_mem;
    f_mem->phy_addr_num = num_new;
    f_mem->dma_map = 1;

    return HDCDRV_OK;

MAP_FAIL:
    hdcdrv_iova_fmem_unmap(dev_id, fid, f_mem, unmap_num);
    return HDCDRV_ERR;
#else
    return HDCDRV_OK;
#endif
}

STATIC struct hdcdrv_mem_pool *get_pool(struct hdcdrv_dev *dev, int pool_type, u32 data_len)
{
    struct hdcdrv_mem_pool *pool = NULL;

    if (data_len <= (u32)(HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE)) {
        pool = &dev->small_mem_pool[pool_type];
        return pool;
    }

    if (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) {
        pool = &dev->reserve_huge_mem_pool[HDCDRV_MEM_POOL_TYPE_TX];
    } else if (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX) {
        pool = &dev->reserve_huge_mem_pool[HDCDRV_MEM_POOL_TYPE_RX];
    } else {
        pool = &dev->huge_mem_pool[pool_type];
    }
    return pool;
}

STATIC struct hdcdrv_mem_pool *get_pool_by_segment(struct hdcdrv_dev *dev, int pool_type, u32 seg_len)
{
    struct hdcdrv_mem_pool *pool = NULL;

    if (seg_len <= (u32)HDCDRV_SMALL_PACKET_SEGMENT) {
        pool = &dev->small_mem_pool[pool_type];
        return pool;
    }

    if (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) {
        pool = &dev->reserve_huge_mem_pool[HDCDRV_MEM_POOL_TYPE_TX];
    } else if (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX) {
        pool = &dev->reserve_huge_mem_pool[HDCDRV_MEM_POOL_TYPE_RX];
    } else {
        pool = &dev->huge_mem_pool[pool_type];
    }
    return pool;
}

STATIC int alloc_mem_later(struct hdcdrv_mem_pool *pool, ka_list_head_t *wait_head)
{
    int ret = HDCDRV_OK;

    ka_task_spin_lock_bh(&pool->mem_lock);
    if (pool->head == pool->tail) {
        if (wait_head != NULL) {
            if (wait_head->next == NULL) {
                ka_list_add_tail(wait_head, &pool->wait_list);
            }
        }

        ret = HDCDRV_DMA_MEM_ALLOC_FAIL;
    }
    ka_task_spin_unlock_bh(&pool->mem_lock);

    return ret;
}

int alloc_mem(int pool_type, int dev_id, int len, void **buf, ka_dma_addr_t *addr, ka_list_head_t *wait_head)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[dev_id];
    struct hdcdrv_mem_pool *pool = get_pool(dev, pool_type, (u32)len);
    int ret;

#ifndef CFG_FEATURE_MIRROR
    if (ka_unlikely((len > hdcdrv_mem_block_capacity()) || (len <= 0))) {
            hdcdrv_err_limit("data len invalid. (len=%d, capacity=%d)\n", len, hdcdrv_mem_block_capacity());
            return HDCDRV_ERR;
        }
#endif

    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d)\n", dev_id);
        return HDCDRV_DEVICE_NOT_READY;
    }

    ret = alloc_mem_later(pool, wait_head);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    ret = hdccom_alloc_mem(pool, buf, addr);
    if (ret != HDCDRV_OK) {
        if (ret != HDCDRV_DMA_MEM_ALLOC_FAIL) {
            hdcdrv_err("Alloc memory failed. (pool_type=%d; device=%d; ret=%d)\n", pool_type, dev_id, ret);
        }
        return ret;
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_mem_avail(int pool_type, ka_list_head_t *target)
{
    struct hdcdrv_msg_chan *msg_chan = NULL;

    if (pool_type == HDCDRV_MEM_POOL_TYPE_RX) {
        msg_chan = ka_list_entry(target, struct hdcdrv_msg_chan, wait_mem_list);
        msg_chan->dbg_stat.hdcdrv_mem_avail1++;
        hdcdrv_rx_msg_schedule_task(msg_chan);
    }
}

STATIC void free_mem_notify(struct hdcdrv_mem_pool *pool, u32 type)
{
    ka_list_head_t *list = NULL;

    ka_task_spin_lock_bh(&pool->mem_lock);
    if (!ka_list_empty_careful(&pool->wait_list)) {
        list = pool->wait_list.next;
        ka_list_del(list);
        list->next = NULL;
        list->prev = NULL;
        hdcdrv_mem_avail((int)type, list);
    }
    ka_task_spin_unlock_bh(&pool->mem_lock);
}

void free_mem(void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    struct hdcdrv_mem_pool *pool = NULL;
    int ret;

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err_spinlock("Block head check failed.\n");
        return;
    }

    block_head = (struct hdcdrv_mem_block_head *)buf;

    pool = get_pool(&hdc_ctrl->devices[block_head->devid], (int)block_head->type, block_head->size);
    if (pool->ring == NULL) {
        hdcdrv_warn_spinlock("Pool ring has freed.\n");
        return;
    }

    ret = hdccom_free_mem(pool, buf);
    if (ret != HDCDRV_OK) {
        hdcdrv_err_spinlock("Calling free_mem failed. (pool_type=%d; device=%d; ret=%d)\n",
            block_head->type, block_head->devid, ret);
        return;
    }

    free_mem_notify(pool, block_head->type);
    return;
}

int alloc_mem_pool(int pool_type, int dev_id, u32 segment, u32 num)
{
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];
    struct hdcdrv_mem_pool *pool;
    struct hdccom_mem_init init_mem;
    int ret;

    if (num == 0) {
        hdcdrv_warn("hdccom_init_mem_pool num is 0. (dev_id=%d; pool_type=%d)\n", dev_id, pool_type);
        return HDCDRV_OK;
    }
    pool = get_pool_by_segment(hdc_dev, pool_type, segment);

    init_mem.dev = hdc_dev->dev;
    init_mem.pool_type = pool_type;
    init_mem.dev_id = dev_id;
    init_mem.segment = segment;
    init_mem.num = num;

#ifdef CFG_FEATURE_MIRROR
    if (segment <= HDCDRV_SMALL_PACKET_SEGMENT) {
        ret = hdccom_init_mem_pool(pool, &init_mem);
    } else {
        if ((init_mem.pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) ||
            (init_mem.pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
            ret = hdccom_init_mem_pool(pool, &init_mem);
        } else {
            ret = hdccom_init_page_pool(pool, &init_mem);
        }
    }
#else
    ret = hdccom_init_mem_pool(pool, &init_mem);
#endif
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_init_mem_pool failed. (dev_id=%d; pool_type=%d)\n", dev_id, pool_type);
        return ret;
    }

    return HDCDRV_OK;
}

void free_mem_pool(int pool_type, int dev_id, u32 segment)
{
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];
    struct hdcdrv_mem_pool *pool = NULL;
    int ret;

    pool = get_pool_by_segment(hdc_dev, pool_type, segment);
#ifdef CFG_FEATURE_MIRROR
    if (segment > HDCDRV_SMALL_PACKET_SEGMENT) {
        if ((pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) ||
            (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
            ret = hdccom_free_mem_pool(pool, hdc_dev->dev, segment);
        } else {
            ret = hdccom_free_page_pool(pool);
        }
    } else {
        ret = hdccom_free_mem_pool(pool, hdc_dev->dev, segment);
    }
#else
    ret = hdccom_free_mem_pool(pool, hdc_dev->dev, segment);
#endif
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_free_mem_pool failed. (dev_id=%d; pool_type=%d; ret=%d)\n",
            dev_id, pool_type, ret);
        return;
    }

    return;
}

int hdcdrv_mem_block_capacity(void)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    return HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE;
#else
    return hdc_ctrl->segment - HDCDRV_MEM_BLOCK_HEAD_SIZE;
#endif
}

#ifdef CFG_FEATURE_MIRROR
int get_pool_type (int data_len)
{
    int pool_type;
    if (data_len <= (HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE)) {
        pool_type = HDCDRV_USE_POOL;
    } else {
        pool_type = HDCDRV_USE_PAGE;
    }
    return pool_type;
}

STATIC int alloc_mem_later_page(struct hdcdrv_mem_pool *pool, ka_list_head_t *wait_head)
{
    int ret = HDCDRV_OK;
    ka_task_spin_lock_bh(&pool->mem_lock);

    // when rx pool is full, add to wait_list to avoid rx_desc lost
    if (pool->used_block_all == HDCDRV_PAGE_BLOCK_NUM_MAX) {
        if (wait_head != NULL) {
            if (wait_head->next == NULL) {
                ka_list_add_tail(wait_head, &pool->wait_list);
            }
        }

        ret = HDCDRV_DMA_MEM_ALLOC_FAIL;
    }
    ka_task_spin_unlock_bh(&pool->mem_lock);

    return ret;
}

STATIC u32 g_alloc_mem_page_print_cnt = 0;
STATIC u64 g_alloc_mem_page_jiffies = 0;
int alloc_mem_page(int pool_type, struct mirror_alloc_para para, void **buf, ka_dma_addr_t *addr, int *mem_id)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[para.dev_id];
    struct hdcdrv_mem_pool *pool = &dev->huge_mem_pool[pool_type];
    int ret, retry_time = 0;

    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d)\n", para.dev_id);
        return HDCDRV_DEVICE_NOT_READY;
    }

ALLOC_MEM_RETRY:
    ret = alloc_mem_later_page(pool, para.wait_head);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    ret = hdccom_alloc_mem_page(pool, buf, addr, mem_id);
    if (ret != HDCDRV_OK) {
        if ((ret != HDCDRV_ALLOC_AGAIN) && (ret != HDCDRV_DMA_MEM_ALLOC_FAIL)
            && (ret != HDCDRV_ALLOC_LATER_WITH_INDEX)) {
            HDC_LOG_ERR_LIMIT(&g_alloc_mem_page_print_cnt, &g_alloc_mem_page_jiffies,
                "Alloc memory failed. (service_type=%d; pool_type=%d; device=%d; ret=%d)\n",
                para.service_type, pool_type, para.dev_id, ret);
        } else if (ret == HDCDRV_ALLOC_LATER_WITH_INDEX) {
            goto ALLOC_MEM_RETRY;
        } else if ((ret == HDCDRV_ALLOC_AGAIN) && (retry_time < HDCDRV_SEND_ALLOC_MEM_RETRY_TIME) &&
            (pool_type == HDCDRV_MEM_POOL_TYPE_RX)) {
            retry_time++;   // we don't want the rx desc to be lost, so only provide retry in rx pool
            ka_system_msleep(HDCDRV_RETRY_SLEEP_TIME);
            goto ALLOC_MEM_RETRY;
        } else {
            ret = HDCDRV_DMA_MEM_ALLOC_FAIL;
        }
        return ret;
    }

    return HDCDRV_OK;
}

void free_mem_page(void *buf, int mem_id)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    struct hdcdrv_mem_pool *pool = NULL;
    u32 pool_type;
    int ret;

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err_spinlock("Block head check failed.\n");
        return;
    }

    block_head = HDCDRV_BLOCK_HEAD(buf);

    pool_type = block_head->type;
    pool = &hdc_ctrl->devices[block_head->devid].huge_mem_pool[pool_type];

    ret = hdccom_free_mem_page(pool, buf, mem_id);
    if (ret != HDCDRV_OK) {
        hdcdrv_err_spinlock("Calling free_mem_page failed. (pool_type=%d; device=%d; ret=%d)\n",
            pool_type, block_head->devid, ret);
        return;
    }

    free_mem_notify(pool, pool_type);
    return;
}

int hdcdrv_init_reserve_mem_pool(u32 dev_id)
{
    u32 reserve_huge_packet_num = HDCDRV_RESERVE_HUGE_PACKET_NUM;
    int ret;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        reserve_huge_packet_num = HDCDRV_VF_RESERVE_HUGE_PACKET_NUM;
    }

    ret = alloc_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment, reserve_huge_packet_num);
    if (ret != HDCDRV_OK) {
        goto FREE_RESERVE_HUGE_TX;
    }

    ret = alloc_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment, reserve_huge_packet_num);
    if (ret != HDCDRV_OK) {
        goto FREE_RESERVE_HUGE_RX;
    }

    return 0;

FREE_RESERVE_HUGE_RX:
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment);
FREE_RESERVE_HUGE_TX:
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment);
    return ret;
}
#endif

int hdcdrv_init_mem_pool(u32 dev_id)
{
    int ret;
    u32 small_packet_num = HDCDRV_SMALL_PACKET_NUM;
    u32 huge_packet_num = HDCDRV_HUGE_PACKET_NUM;

    /* only uninit free, suspend status not free */
    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return HDCDRV_OK;
    }

    hdcdrv_get_mempool_size(&small_packet_num, &huge_packet_num);
    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT, small_packet_num);
    if (ret != HDCDRV_OK) {
        goto FREE_SMALL_TX;
    }

    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT, small_packet_num);
    if (ret != HDCDRV_OK) {
        goto FREE_SMALL_RX;
    }

    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment, huge_packet_num);
    if (ret != HDCDRV_OK) {
        goto FREE_HUGE_TX;
    }

    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment, huge_packet_num);
    if (ret != HDCDRV_OK) {
        goto FREE_HUGE_RX;
    }

#ifdef CFG_FEATURE_MIRROR
    ret = hdcdrv_init_reserve_mem_pool(dev_id);
    if (ret != HDCDRV_OK) {
        goto FREE_HUGE_RX;
    }
#endif
    hdcdrv_info("hdcdrv_init_mem_pool success.\n");

    return HDCDRV_OK;

FREE_HUGE_RX:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment);
FREE_HUGE_TX:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment);
FREE_SMALL_RX:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
FREE_SMALL_TX:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);

    return HDCDRV_ERR;
}

void hdcdrv_uninit_mem_pool(u32 dev_id)
{
    /* only uninit free, suspend status not free */
    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return;
    }

#ifdef CFG_FEATURE_MIRROR
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment);
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment);
#endif
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment);
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment);
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);\
    hdcdrv_info("hdcdrv_uninit_mem_pool success.\n");
}

STATIC int hdcdrv_list_node_mem_alloc(struct hdcdrv_ctx *ctx, struct hdcdrv_mem_fd_list **new_node)
{
    if ((ctx != NULL) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
        *new_node = (struct hdcdrv_mem_fd_list *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_mem_fd_list), KA_SUB_MODULE_TYPE_1);
        if (*new_node == NULL) {
            hdcdrv_err("Calling kzalloc failed.\n");
            return HDCDRV_MEM_ALLOC_FAIL;
        }
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_list_node_mem_free(void *ctx, struct hdcdrv_mem_fd_list *new_node)
{
    if ((ctx != NULL) && ((struct hdcdrv_ctx *)ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
        hdcdrv_kvfree(new_node, KA_SUB_MODULE_TYPE_1);
    }
}

long hdcdrv_fast_alloc_mem(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_alloc_mem *cmd)
{
    long ret;
    struct hdcdrv_fast_node *f_node = NULL;
    struct hdcdrv_mem_fd_list *new_node = NULL;

    ret = hdcdrv_list_node_mem_alloc(ctx, &new_node);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    ret = hdccom_fast_alloc_mem((void *)ctx, cmd, &f_node);
    if ((ret != HDCDRV_OK) || (f_node == NULL)) {
        if (ret == HDCDRV_VA_UNMAP_FAILED) {
            hdcdrv_bind_mem_ctx(&ctx->abnormal_ctx_fmem, f_node, new_node);
        } else {
            hdcdrv_list_node_mem_free((void *)ctx, new_node);
        }
        hdcdrv_err("Calling alloc memory error. (dev=%d)\n", cmd->dev_id);
        return ret;
    }

    if ((ctx != NULL) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
       hdcdrv_bind_mem_ctx(&ctx->ctx_fmem, f_node, new_node);
    }

    f_node->ctx = (void *)ctx;

    return HDCDRV_OK;
}

bool hdcdrv_mem_is_notify(const struct hdcdrv_fast_mem *f_mem)
{
    if ((f_mem->mem_type == HDCDRV_FAST_MEM_TYPE_RX_DATA) || (f_mem->mem_type == HDCDRV_FAST_MEM_TYPE_RX_CTRL)) {
        return false;
    }

    return true;
}

#ifdef CFG_FEATURE_HDC_REG_MEM
long hdcdrv_fast_register_mem(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_register_mem *cmd)
{
    long ret;
    struct hdcdrv_fast_node *f_node = NULL;
    struct hdcdrv_mem_fd_list *new_node = NULL;

    ret = hdcdrv_list_node_mem_alloc(ctx, &new_node);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    ret = hdccom_fast_register_mem(cmd, &f_node);
    if ((ret != HDCDRV_OK) || (f_node == NULL)) {
        hdcdrv_warn_limit("Calling alloc memory not success. (dev=%d)\n", cmd->dev_id);
        hdcdrv_list_node_mem_free((void *)ctx, new_node);
        return ret;
    }

    if ((ctx != NULL) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
        hdcdrv_bind_mem_ctx(&ctx->ctx_fmem, f_node, new_node);
        f_node->mem_fd_node->async_ctx = &ctx->async_ctx;
    }

    f_node->ctx = (void *)ctx;

    return HDCDRV_OK;
}

// create session，Create a kfifo queue for each session and associate it with the session.
int hdcdrv_init_session_release_mem_event(struct hdcdrv_session *session)
{
    int ret;
    if (session == NULL) {
        hdcdrv_err("error init session mem fin event not exist\n");
        return HDCDRV_ERR;
    }
    ka_task_spin_lock_init(&session->mem_release_lock);
    ka_task_init_waitqueue_head(&session->wq_mem_release_event);
    ret = ka_base_kfifo_alloc(&session->mem_release_fifo, HDC_WAIT_MEM_FIFO_SIZE, KA_GFP_KERNEL);
    if (ret != 0) {
        hdcdrv_err("error init session mem fin event (devid=%d)\n", session->dev_id);
        return HDCDRV_ERR;
    }
    return HDCDRV_OK;
}

// kfifo queue for each session and associate it with the session.
void hdcdrv_free_session_release_mem_event(struct hdcdrv_session *session)
{
    unsigned long flags;

    if (session == NULL) {
        hdcdrv_err("error init session mem fin event not exist\n");
        return;
    }
    ka_task_spin_lock_irqsave(&session->mem_release_lock, flags);
    if (session->mem_release_fifo.buf != NULL) {
        ka_base_kfifo_free(&session->mem_release_fifo);
    }
    ka_task_spin_unlock_irqrestore(&session->mem_release_lock, flags);
    return;
}

// fast send
void hdcdrv_update_session_release_mem_event(struct hdcdrv_session *session,
                                             const struct hdcdrv_wait_mem_fin_msg *in_msg)
{
    unsigned long flags;
    unsigned int ret;

    if ((session == NULL) || (in_msg == NULL)) {
        hdcdrv_err("when update mem event, session and msg not exist\n");
        return;
    }
    if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) {
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x108, "when update mem event, session have closed."
            "(session=%d)\n", session->local_session_fd);
        return;
    }

    if (ka_base_kfifo_is_full(&session->mem_release_fifo) == 0) {
        ka_task_spin_lock_irqsave(&session->mem_release_lock, flags);
        ret = ka_base_kfifo_in(&session->mem_release_fifo, (const void *)in_msg, sizeof(struct hdcdrv_wait_mem_fin_msg));
        ka_task_spin_unlock_irqrestore(&session->mem_release_lock, flags);
        if (ret != sizeof(struct hdcdrv_wait_mem_fin_msg)) {
            hdcdrv_warn_limit("update mem fifo in event no success.\n");
            return;
        }
        session->dbg_stat.hdcdrv_wait_mem_normal++;
        if (ka_task_waitqueue_active(&session->wq_mem_release_event) != 0) {
            session->mem_release_wait_flag = 1;
            ka_task_wake_up_interruptible(&session->wq_mem_release_event);
        }
    } else {
        session->dbg_stat.hdcdrv_wait_mem_fifo_full++;
        return;
    }
    return;
}

// fast wait
int hdcdrv_wait_session_release_mem_event(struct hdcdrv_session *session,
    int time_out, unsigned int result_type, struct hdcdrv_wait_mem_fin_msg *out_msg)
{
    unsigned long flags;
    unsigned int ret;
    int session_status;

    if ((session == NULL) || (out_msg == NULL)) {
        hdcdrv_err("session illegal or out_msg is null\n");
        return HDCDRV_ERR;
    }

    if ((time_out < 0) && (time_out != -1)) {
        hdcdrv_err("HDC wait mem timeout para err(%d)\n", time_out);
        return HDCDRV_PARA_ERR;
    }

    while (ka_base_kfifo_is_empty(&session->mem_release_fifo) != 0) {
        if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) {
            hdcdrv_warn("session closed fd=%d.\n", session->local_session_fd);
            return HDCDRV_SESSION_HAS_CLOSED;
        }
        if (time_out == 0) {
            hdcdrv_warn("HDC wait mem release no timeout, fd=%d.\n", session->local_session_fd);
            return HDCDRV_NO_WAIT_MEM_INFO;
        }
        if (time_out == -1) {
            ka_task_wait_event_interruptible(session->wq_mem_release_event,
                                     (session->mem_release_wait_flag == 1) ||
                                     (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
                                     (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
                                     (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL));
        } else {
            if (ka_task_wait_event_interruptible_timeout(session->wq_mem_release_event,
                                                 (session->mem_release_wait_flag == 1) ||
                                                 (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
                                                 (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE),
                                                 ka_system_msecs_to_jiffies((unsigned int)time_out)) == 0) {
                hdcdrv_warn("HDC wait mem release timeout, fd=%d.\n", session->local_session_fd);
                return HDCDRV_NO_WAIT_MEM_TIMEOUT;
            }
        }

        if (session->mem_release_wait_flag != 1) {
            session->mem_release_wait_flag = 0;
#ifndef DRV_UT
            session_status = hdcdrv_get_session_status(session);
            if ((session_status != HDCDRV_SESSION_STATUS_IDLE) && (session_status != HDCDRV_SESSION_STATUS_CLOSING)) {
                hdcdrv_err("HDC wait mem release bad wakeup.(fd=%d; session_status=%d)\n", session->local_session_fd, session_status);
                return HDCDRV_ERR;
            } else {
                return HDCDRV_SESSION_HAS_CLOSED;
            }
#endif
        }
        session->mem_release_wait_flag = 0;
    }

    ka_task_spin_lock_irqsave(&session->mem_release_lock, flags);
    while (ka_base_kfifo_is_empty(&session->mem_release_fifo) == 0) {
        ret = ka_base_kfifo_out(&session->mem_release_fifo, (void *)out_msg, (u32)sizeof(struct hdcdrv_wait_mem_fin_msg));
        if ((result_type == (u32)HDC_WAIT_ALL) ||
            ((out_msg->status != 0) && (result_type == (u32)HDC_WAIT_ONLY_EXCEPTION)) ||
            ((out_msg->status == 0) && (result_type == HDC_WAIT_ONLY_SUCCESS))) {
            break;
        }
    }
    ka_task_spin_unlock_irqrestore(&session->mem_release_lock, flags);
    if (ret != sizeof(struct hdcdrv_wait_mem_fin_msg)) {
        hdcdrv_err("session mem fin event bad size (ret=%u)\n", ret);
        return HDCDRV_ERR;
    } else {
        return HDCDRV_OK;
    }
}

// destroy session
void hdcdrv_destroy_session_del_mem_release_event(struct hdcdrv_session *session)
{
    unsigned long flags;
    if (session != NULL) {
        if ((ka_task_waitqueue_active(&session->wq_mem_release_event) != 0)) {
            session->mem_release_wait_flag = 1;
            ka_task_spin_lock_irqsave(&session->mem_release_lock, flags);
            ka_base_kfifo_reset(&session->mem_release_fifo); // only the kfifo memory is set to empty.
            ka_task_spin_unlock_irqrestore(&session->mem_release_lock, flags);
            hdcdrv_info("Clear mem release fifo data.\n");
            ka_task_wake_up_interruptible(&session->wq_mem_release_event);
        }
    } else {
        hdcdrv_err("no session in mem release event\n");
    }
}

// when peer fault happen, set peer_status and wake up mem_release_event
void hdcdrv_peer_fault_del_mem_release_event(struct hdcdrv_session *session)
{
    unsigned long flags;
    if (session != NULL) {
        if ((ka_task_waitqueue_active(&session->wq_mem_release_event) != 0)) {
            ka_task_spin_lock_irqsave(&session->mem_release_lock, flags);
            ka_base_kfifo_reset(&session->mem_release_fifo); // only the kfifo memory is set to empty.
            ka_task_spin_unlock_irqrestore(&session->mem_release_lock, flags);
            hdcdrv_info("Clear mem release fifo data.\n");
            ka_task_wake_up_interruptible(&session->wq_mem_release_event);
        }
    }
}

long hdcdrv_fast_wait_mem(struct hdcdrv_cmd_wait_mem *cmd)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_wait_mem_fin_msg out_msg;
    long ret;

    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    session = &hdc_ctrl->sessions[cmd->session];
    session->result_type = cmd->result_type;
    ret = hdcdrv_wait_session_release_mem_event(session, cmd->timeout, cmd->result_type, &out_msg);
    if (ret != HDCDRV_OK) {
        cmd->data_addr = 0UL;
        cmd->ctrl_addr = 0UL;
        cmd->data_len = 0U;
        cmd->ctrl_len = 0U;
        cmd->result = 0U;
    } else {
        cmd->data_addr = (u64)out_msg.dataAddr;
        cmd->ctrl_addr = (u64)out_msg.ctrlAddr;
        cmd->data_len = (int)out_msg.dataLen;
        cmd->ctrl_len = (int)out_msg.ctrlLen;
        cmd->result = (int)out_msg.status;
    }
    return ret;
}
#endif

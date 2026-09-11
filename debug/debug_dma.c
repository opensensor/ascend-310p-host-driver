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
 * Create: 2023-09-07
 */

#ifdef CFG_SOC_PLATFORM_CLOUD
#include "trs_sqe_update.h"
#endif

#include "uda.h"
#include "devdrv_interface.h"
#include "kernel_version_adapt.h"
#include "trs_chan_kernel_interface.h"
#include "ascend_kernel_hal.h"
#include "debug_dma.h"

#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_dfx_pub.h"
#include "ka_errno_pub.h"
#include "ka_base_pub.h"

#define DEBUG_GET_2M_PAGE_NUM 512
#define ERR_DMA_COPY_FAILED   (-1)
#define ALIGNED_NUMBER        0xFFF
#define ALIGNED_OFFSET        12

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif
#endif

#define TD_PRINT_ERR(fmt, ...) ka_dfx_printk(KERN_ERR "[ts_debug][ERROR]<%s:%d> " \
    fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define TD_PRINT_INFO(fmt, ...) ka_dfx_printk(KERN_INFO "[ts_debug][INFO]<%s:%d> " \
    fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

static u64 get_page_num(u64 addr, u64 addr_len)
{
    u64 align_addr_len, page_num;

    align_addr_len = ((addr & (KA_MM_PAGE_SIZE - 1)) + addr_len);
    if (align_addr_len < addr_len || align_addr_len == 0) {
        TD_PRINT_INFO("get_page_num addr=0x%llx addr_len=%llu overflow!\n", addr, addr_len);
        return -EOVERFLOW;
    }
    
    page_num = align_addr_len / KA_MM_PAGE_SIZE;
    if ((align_addr_len & (KA_MM_PAGE_SIZE - 1)) != 0) {
        page_num++;
    }
    TD_PRINT_INFO("get_page_num addr=0x%llx addr_len=%llu align_addr_len=%llu page_num=%llu\n",
        addr, addr_len, align_addr_len, page_num);
    return page_num;
}

void debug_put_user_pages(ka_page_t **pages, u64 page_num, u64 got_num)
{
    u64 i;

    if ((got_num == 0) || (got_num > page_num)) {
        return;
    }

    for (i = 0; i < got_num; i++) {
        if (pages[i] != NULL) {
            ka_mm_put_page(pages[i]);
            pages[i] = NULL;
        }
    }
}

int debug_get_user_pages(u64 va, u64 page_num, ka_page_t **pages)
{
    u64 got_num, remained_num, tmp_va;
    int expected_num, tmp_num;

    for (got_num = 0; got_num < page_num;) {
        tmp_va = va + got_num * KA_MM_PAGE_SIZE;
        remained_num = page_num - got_num;
        expected_num = (int)((remained_num > DEBUG_GET_2M_PAGE_NUM) ? DEBUG_GET_2M_PAGE_NUM : remained_num);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
        tmp_num = get_user_pages_fast(tmp_va, expected_num, 1, &pages[got_num]);
#else
        tmp_num = get_user_pages_fast(tmp_va, expected_num, FOLL_WRITE, &pages[got_num]);
#endif
        got_num += (u64)((tmp_num > 0) ? (u32)tmp_num : 0);
        if (tmp_num != expected_num) {
            TD_PRINT_ERR("Get_user_pages_fast fail. (va=0x%llx; already_got_num=%llu; tmp_va=0x%llx; "
                "expected_num=%d; get_num_or_ret=%d)\n",
                va, got_num, tmp_va, expected_num, tmp_num);
            debug_put_user_pages(pages, page_num, got_num);
            return -EFBIG;
        }
    }
    TD_PRINT_INFO("debug_get_user_pages done\n");
    return 0;
}

void *kvalloc(u64 size, ka_gfp_t flags)
{
    void *ptr = ka_mm_kmalloc(size, KA_GFP_ATOMIC | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT | flags);
    if (ptr == NULL) {
        ptr = __ka_mm_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT | flags, KA_PAGE_KERNEL);
    }
    return ptr;
}

void kvfree(const void *ptr)
{
    if (ka_mm_is_vmalloc_addr(ptr)) {
        ka_mm_vfree(ptr);
    } else {
        ka_mm_kfree(ptr);
    }
}

void debug_make_single_dma_node(struct devdrv_dma_node *dma_node, struct dma_param *param,
    u64 dma_addr, u64 dma_addr_dev, u32 passid)
{
    dma_node->loc_passid = passid;
    dma_node->size = param->size;
    if (param->direction == 0) {
        // host --> device
        dma_node->direction = DEVDRV_DMA_HOST_TO_DEVICE;
        dma_node->src_addr = dma_addr;
        dma_node->dst_addr = dma_addr_dev;
    } else {
        // device --> host
        dma_node->direction = DEVDRV_DMA_DEVICE_TO_HOST;
        dma_node->src_addr = dma_addr_dev;
        dma_node->dst_addr = dma_addr;
    }
}

void debug_get_dma_addr_dev(struct dma_param *param, int pid, u32 logical_devid, u64 *dma_addr_dev, u32 *passid)
{
    u64 tmp, dev_addr_aligned;
    int ret;

    // 地址对齐
    tmp = param->device_addr & ALIGNED_NUMBER;
    dev_addr_aligned = param->device_addr >> ALIGNED_OFFSET;
    dev_addr_aligned = dev_addr_aligned << ALIGNED_OFFSET;
    TD_PRINT_INFO("tmp=[0x%llx], dev_addr_aligned=[0x%llx]\n", tmp, dev_addr_aligned);

    // 先使用dma地址转换接口,若遇到非只读地址导致该接口失败，则改用va拷贝
    ret = hal_kernel_svm_dev_va_to_dma_addr(pid, logical_devid, dev_addr_aligned, dma_addr_dev);
    if ((ret != 0) || (*dma_addr_dev == 0)) { // dma地址为0当作接口失败进行处理，走回va地址拷贝
        *dma_addr_dev = dev_addr_aligned;
        TD_PRINT_INFO("Do va copy. (ret=[%d])", ret);
    } else {
       *passid = 0;
        TD_PRINT_INFO("Do dma copy. (ret=[%d])", ret);
    }
    TD_PRINT_INFO("pid=[%d] logical_devid=[%u] dev_addr_aligned=[0x%llx] dma_addr_dev=[0x%llx]\n",
        pid, logical_devid, dev_addr_aligned, *dma_addr_dev);
    *dma_addr_dev += tmp;
}

int debug_get_passid(u32 devid, u32 tsid, int pid, u32 *passid)
{
    #ifndef CFG_SOC_PLATFORM_CLOUD
        return devdrv_get_ssid(devid, tsid, pid, passid);
    #else
        return hal_kernel_trs_get_ssid(devid, tsid, pid, passid);
    #endif
}

int dma_copy_sync(u32 logical_devid, u32 devid, u32 tsid, int pid, struct dma_param *param)
{
    u32 passid;
    u64 page_num, offset, dma_addr, dma_addr_dev;
    int ret;
    ka_device_t *dev;
    ka_page_t **pages;
    struct devdrv_dma_node dma_node = {0};

    TD_PRINT_INFO("enter dma_copy_sync host_addr=0x%llx device_addr=0x%llx size=%llu pid=%d direction=%d\n",
        param->host_addr, param->device_addr, param->size, pid, (int)param->direction);

    // 获取passid
    ret = debug_get_passid(devid, tsid, pid, &passid);
    if (ret != 0) {
            TD_PRINT_ERR("get_ssid failed ret=%d devid=%u tsid=%u pid=%d\n", ret, devid, tsid, pid);
            return ERR_DMA_COPY_FAILED;
    }
    // 获取page页数
    page_num = get_page_num(param->host_addr, param->size);
    TD_PRINT_INFO("page_num=%llu\n", page_num);
    if (page_num != 1) {
        TD_PRINT_ERR("0 page or more than 1 page is not supported.\n");
        return ERR_DMA_COPY_FAILED;
    }

    // 分配pages空间
    pages = (ka_page_t **)kvalloc(page_num * sizeof(ka_page_t *), 0);
    if (pages == NULL) {
        TD_PRINT_ERR("pages kvalloc failed\n");
        return ERR_DMA_COPY_FAILED;
    }

    // 获取设备
    dev = uda_get_device(devid);
    // 获取page指针
    ret = debug_get_user_pages(param->host_addr, page_num, pages);
    if (ret != 0) {
        // 释放pages
        kvfree(pages);
        TD_PRINT_ERR("debug_get_user_pages failed\n");
        return ret;
    }

    // 获取dma地址
    offset = (param->host_addr) % KA_MM_PAGE_SIZE;
    dma_addr = devdrv_dma_map_page(dev, pages[0], offset, param->size, KA_DMA_BIDIRECTIONAL);
    TD_PRINT_INFO("devdrv_dma_map_page done dma_addr=0x%llx, offset=%llu\n", dma_addr, offset);

    debug_get_dma_addr_dev(param, pid, logical_devid, &dma_addr_dev, &passid);

    debug_make_single_dma_node(&dma_node, param, dma_addr, dma_addr_dev, passid);

    // 执行dma拷贝
    ret = devdrv_dma_sync_link_copy(devid, DEVDRV_DMA_DATA_TRAFFIC, DEVDRV_DMA_WAIT_INTR, &dma_node, 1);

    TD_PRINT_INFO("dma_node src_addr=0x%llx dst_addr=0x%llx size=%llu loc_passid=%u direction=%d\n",
        dma_node.src_addr, dma_node.dst_addr, dma_node.size, dma_node.loc_passid, (int)dma_node.direction);

    if (ret != 0) {
        TD_PRINT_ERR("devdrv_dma_sync_link_copy fail. (devid=%u; node_cnt=%d; ret=%d)\n", devid, 1, ret);
        kvfree(pages);
        return ret;
    }

    // 释放page
    debug_put_user_pages(pages, page_num, page_num);

    // 释放pages
    kvfree(pages);

    TD_PRINT_INFO("dma_copy_sync done\n");
    return 0;
}

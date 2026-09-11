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
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "svm_cmd.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "devmm_page_cache.h"
#include "devmm_channel.h"
#include "devdrv_interface.h"
#include "svm_msg_client.h"
#include "svm_heap_mng.h"
#include "svm_proc_mng.h"
#include "svm_dma.h"
#include "devmm_proc_mem_copy.h"
#include "devmm_channel.h"
#ifdef CFG_FEATURE_VFIO
#include "devmm_pm_adapt.h"
#include "devmm_pm_vpc.h"
#endif

int devmm_page_create_query_msg(struct devmm_svm_process *svm_pro, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num)
{
    unsigned long total_size, ack_msg_len, aligned_va, aligned_size;
    struct devmm_chan_page_query_ack *page_query = NULL;
    u32 merg_num, num_pre_msg, i, per_max_num, flag;
    u32 total_num = 0;
    int ret = 0;

    devmm_drv_debug("Page create query message. (va=0x%llx; size=0x%llx; page_size=%u; addr_type=%d; "
        "msg_id=%d; dev_id=%d; num=%d)\n", query_arg.va, query_arg.size,
        query_arg.page_size, query_arg.addr_type, query_arg.msg_id, query_arg.dev_id, *num);

    per_max_num = devmm_get_max_page_num_of_per_msg(&query_arg.bitmap);
    num_pre_msg = *num;
    num_pre_msg = ka_base_min(num_pre_msg, per_max_num);
    ack_msg_len = sizeof(struct devmm_chan_page_query_ack) +
                  sizeof(struct devmm_chan_query_phy_blk) * (unsigned long)num_pre_msg;
    page_query = (struct devmm_chan_page_query_ack *)devmm_kvzalloc(ack_msg_len);
    if (page_query == NULL) {
        devmm_drv_err("Page query devmm_kvzalloc_ex fail. (va=0x%llx; size=0x%llx; msg_id=%d; dev_id=%d; num=%d)\n",
                      query_arg.va, query_arg.size, query_arg.msg_id, query_arg.dev_id, *num);
        return -ENOMEM;
    }

    /* query dst addr list* */
    page_query->bitmap = query_arg.bitmap;
    page_query->is_giant_page = query_arg.is_giant_page;
    page_query->head.msg_id = (u16)query_arg.msg_id;
    page_query->head.process_id.vfid = query_arg.process_id.vfid;
    page_query->head.process_id.hostpid = query_arg.process_id.hostpid;
    page_query->head.extend_num = (u16)num_pre_msg;
    page_query->page_size = query_arg.page_size;
    page_query->p2p_owner_sdid = query_arg.p2p_owner_sdid;
    page_query->mem_map_route = query_arg.mem_map_route;

    aligned_va = ka_base_round_down(query_arg.va, query_arg.page_size);
    aligned_size = ka_base_round_up((query_arg.size + query_arg.offset), query_arg.page_size);
    for (total_size = 0, merg_num = 0, total_num = 0; ((total_size < aligned_size) && (total_num < *num));) {
        page_query->p2p_owner_process_id = query_arg.p2p_owner_process_id;
        page_query->head.dev_id = (u16)query_arg.dev_id;
        page_query->va = aligned_va + total_size;
        page_query->p2p_owner_va = (query_arg.p2p_owner_va != 0) ? query_arg.p2p_owner_va + total_size : 0;
        page_query->size = aligned_size - total_size;
        page_query->addr_type = query_arg.addr_type;
        page_query->num = ka_base_min((u32)num_pre_msg, (*num - total_num));
        /*
         * for 4G+continuty mem, first msg is CREAT to alloc all continuty papges, other msgs while not receive all
         * pages, then send QUERY msgs to query mapped pages.
         */
        flag = devmm_page_bitmap_is_advise_ts(&query_arg.bitmap) &&
               devmm_page_bitmap_is_advise_continuty(&query_arg.bitmap) &&
               (total_num > 0) && (total_num < *num);
        if (flag != 0) {
            page_query->head.msg_id = DEVMM_CHAN_PAGE_QUERY_H2D_ID;
        }
        ret = devmm_chan_msg_send(page_query, (u32)sizeof(struct devmm_chan_page_query_ack), (u32)ack_msg_len);
        if (ret != 0) {
            devmm_drv_info("Can not page alloc message. (ret=%d; ack_len=%lu; va=0x%llx; "
                "size=%llu; msg_id=%u; dev_id=%u; vfid=%u; num=%u; total_num=%u)\n",
                ret, ack_msg_len, query_arg.va, query_arg.size, query_arg.msg_id, query_arg.dev_id,
                query_arg.process_id.vfid, *num, total_num);
            goto query_page_bymsg_free;
        }

        /* check rest pages* */
        flag = (page_query->num <= 0) || (page_query->num > num_pre_msg) || ((total_num + page_query->num) > *num);
        if (flag != 0) {
            /* over max size */
            ret = -EINVAL;
            devmm_drv_err("Page query pa num null or too long. (page_query_num=%u; "
                          "num_pre_msg=%u; va=0x%llx; size=%llu; msg_id=%u; dev_id=%u; num=%u; total_num=%u)\n",
                          page_query->num, num_pre_msg, page_query->va, page_query->size,
                          query_arg.msg_id, query_arg.dev_id, *num, total_num);
            goto query_page_bymsg_free;
        }

        if (query_arg.msg_id != DEVMM_CHAN_PAGE_P2P_CREATE_H2D_ID) {
            devmm_insert_pages_cache(svm_pro, page_query, query_arg.page_insert_dev_id);
        }

        for (i = 0; i < page_query->num; i++) {
            total_size += page_query->page_size;
            /* create do not need save pa to blks... */
            if (blks != NULL) {
                blks[total_num].pa = (query_arg.addr_type == DEVMM_ADDR_TYPE_DMA) ?
                    page_query->blks[i].dma_addr : page_query->blks[i].phy_addr;
                blks[total_num].sz = page_query->page_size;
                blks[total_num].ssid = 0;
                devmm_merg_blk(blks, total_num, &merg_num);
            }
            total_num++;
        }
    };
    *num = (blks != NULL) ? merg_num : total_num;

query_page_bymsg_free:
    devmm_kvfree(page_query);
    page_query = NULL;

    return ret;
}

int devmm_query_page_by_msg(struct devmm_svm_process *svm_proc, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num)
{
    /* just proc query */
    if ((query_arg.msg_id == DEVMM_CHAN_PAGE_QUERY_H2D_ID) &&
        (devmm_find_pages_cache(svm_proc, query_arg, blks, num))) {
        return 0;
    }

    if (query_arg.msg_id == DEVMM_CHAN_PAGE_QUERY_H2D_ID &&
        devmm_page_bitmap_is_advise_readonly(&query_arg.bitmap)) {
        devmm_drv_err("Readonly mem, not allowed memcpy. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx)\n",
            query_arg.process_id.hostpid, query_arg.dev_id, query_arg.process_id.vfid, query_arg.va);
        return -EADDRNOTAVAIL;
    }

    return devmm_page_create_query_msg(svm_proc, query_arg, blks, num);
}

int devmm_p2p_page_create_msg(struct devmm_svm_process *svm_pro, struct devmm_page_query_arg query_arg,
    struct devmm_dma_block *blks, u32 *num)
{
    return devmm_query_page_by_msg(svm_pro, query_arg, blks, num);
}

STATIC void devmm_fill_page_fault_msg(struct devmm_devid svm_id, unsigned long va, u32 adjust_order, int msg_id,
    struct devmm_chan_page_fault *fault_msg)
{
    fault_msg->head.dev_id = (u16)svm_id.devid;
    fault_msg->head.process_id.vfid = (u16)svm_id.vfid;
    fault_msg->head.msg_id = DEVMM_CHAN_PAGE_FAULT_H2D_ID;
    fault_msg->head.process_id.hostpid = devmm_get_current_pid();
    fault_msg->va = va;
    fault_msg->num = (1ul << adjust_order);
}

/* h2d fault, inv device pagetable: (the max dma unit size is PAGESIZE ?)
 * 1. host host query host page (pin page and map dma)
 * 2. host send page-msg to device
 * 3. device recv and prs devic pagetable
 * 4. device query device page
 * 5. device copy to host
 * 6. device inv device page,return.
 * 7. host (unpin page and unmap dma) if non full size, return to 1.
 */
int devmm_page_fault_h2d_sync(struct devmm_devid svm_id, ka_page_t **pages, unsigned long va, u32 adjust_order,
                              const struct devmm_svm_heap *heap)
{
    struct devmm_chan_page_fault *fault_msg = NULL;
    struct devmm_chan_phy_block *blks = NULL;
    ka_device_t *dev = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 j, idx;
    int ret;

    devmm_drv_debug("Synchronize details. (dev_id=%u; va=0x%lx; adj_order=%u)\n", svm_id.devid, va, adjust_order);

    fault_msg = (struct devmm_chan_page_fault *)devmm_kzalloc_ex(sizeof(struct devmm_chan_page_fault), KA_GFP_KERNEL);
    if (fault_msg == NULL) {
        devmm_drv_err("Kzalloc error. (dev_id=%u; va=0x%lx; adjust_order=%u)\n", svm_id.devid, va, adjust_order);
        return -ENOMEM;
    }
    blks = fault_msg->blks;
    devmm_fill_page_fault_msg(svm_id, va, adjust_order, DEVMM_CHAN_PAGE_FAULT_H2D_ID, fault_msg);

    if (fault_msg->num > DEVMM_PAGE_NUM_PER_FAULT) {
        devmm_drv_err("Fault message num is invalid. (num=%u)\n", fault_msg->num);
        devmm_kfree_ex(fault_msg);
        return -EINVAL;
    }

    dev = devmm_device_get_by_devid(svm_id.devid);
    if (dev == NULL) {
        devmm_drv_err("Device is NULL. (dev_id=%d)\n", svm_id.devid);
        devmm_kfree_ex(fault_msg);
        return -ENODEV;
    }

    for (idx = 0; idx < fault_msg->num; idx++) {
        blks[idx].pa = ka_mm_page_to_phys(pages[idx]);
        blks[idx].sz = KA_MM_PAGE_SIZE;
    }

    stamp = (u32)ka_jiffies;
    for (idx = 0; idx < fault_msg->num; idx++) {
        blks[idx].pa = devdrv_dma_map_page(dev, devmm_pa_to_page(blks[idx].pa), 0, blks[idx].sz, KA_DMA_BIDIRECTIONAL);
        if (ka_mm_dma_mapping_error(dev, blks[idx].pa) != 0) {
            devmm_drv_err("Host page fault dma map page failed. (dev_id=%u; va=0x%lx; adjust_order=%u)\n",
                          svm_id.devid, va, adjust_order);
            ret = -EIO;
            goto host_page_fault_dma_free;
        }
        devmm_try_cond_resched(&stamp);
    }

    /* sync send msg:device todo copy data process */
    ret = devmm_chan_msg_send(fault_msg, sizeof(*fault_msg), 0);
    if (ret != 0) {
        devmm_drv_err("Device copy data process failed. (ret=%d; dev_id=%u; va=0x%lx; adj_order=%u)\n",
                      ret, svm_id.devid, va, adjust_order);
    }

host_page_fault_dma_free:
    stamp = (u32)ka_jiffies;
    for (j = 0; j < idx; j++) {
        devdrv_dma_unmap_page(dev, blks[j].pa, blks[j].sz, KA_DMA_BIDIRECTIONAL);
        devmm_try_cond_resched(&stamp);
    }
    devmm_device_put_by_devid(svm_id.devid);

    devmm_kfree_ex(fault_msg);

    return ret;
}

/* d2h fault, inv host pagetable:
 * 1. device query device page
 * 2. device send page-msg to host
 * 3. host recv and prs devic pages
 * 4. host query host page (pin page and map dma)
 * 5. host copy to device
 * 6. host inv host page (unpin page and unmap dma), return.
 * 7. device if nonfull size return to 1.
 */
int devmm_chan_page_fault_d2h_process_dma_copy(struct devmm_chan_page_fault *fault_msg, u64 *pas,
    u32 *szs, u32 num)
{
    struct devmm_svm_process_id *process_id = &fault_msg->head.process_id;
    struct devdrv_dma_node *dma_nodes = NULL;
    u32 off, max_num, i;
    int ret;

    dma_nodes = (struct devdrv_dma_node *)devmm_kzalloc_ex(sizeof(struct devdrv_dma_node) * DEVMM_PAGE_NUM_PER_FAULT,
                                                  KA_GFP_KERNEL);
    if (dma_nodes == NULL) {
        devmm_drv_err("Kzalloc error. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num);
        return -ENOMEM;
    }

    /*
     * Create dma node list*, num: host page num; fault_msg->num: device page num
     * 1. device hugepage fault: num >= fault_msg->num (the value of num depended on merged result)
     * 2. device chunkpage fault: 1) num < fault_msg->num when host page_size is 64K
     *                            2) num = fault_msg->num when host page_size is 4K
     */
    if (num >= fault_msg->num) {
        for (i = 0, off = 0; i < num; i++) {
            dma_nodes[i].src_addr = pas[i];
            dma_nodes[i].dst_addr = fault_msg->blks[0].pa + off;
            dma_nodes[i].size = szs[i];
            dma_nodes[i].direction = DEVDRV_DMA_HOST_TO_DEVICE;
            off += dma_nodes[i].size;
        }
        max_num = num;
    } else {
        if (fault_msg->num > DEVMM_PAGE_NUM_PER_FAULT) {
            devmm_drv_err("Fault page num too large. (num=%u)\n", fault_msg->num);
            devmm_kfree_ex(dma_nodes);
            dma_nodes = NULL;
            return -EINVAL;
        }
        for (i = 0, off = 0; i < fault_msg->num; i++) {
            dma_nodes[i].src_addr = pas[0] + off;
            dma_nodes[i].dst_addr = fault_msg->blks[i].pa;
            dma_nodes[i].size = fault_msg->blks[i].sz;
            dma_nodes[i].direction = DEVDRV_DMA_HOST_TO_DEVICE;
            off += dma_nodes[i].size;
            /* check if off exceed host page size in the case of host page size is 64k while device page size is 4k */
            if (off > KA_MM_PAGE_SIZE) {
                devmm_drv_err("Over host page size. (off=%u)\n", off);
                devmm_kfree_ex(dma_nodes);
                dma_nodes = NULL;
                return -EINVAL;
            }
        }
        max_num = fault_msg->num;
    }

    ret = devmm_dma_sync_link_copy(fault_msg->head.dev_id, fault_msg->head.vfid, dma_nodes, max_num);
    if (ret != 0) {
        devmm_drv_err("Dma sync link copy failed. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; num=%d; ret=%d)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, fault_msg->va, fault_msg->num, ret);
    }

    devmm_kfree_ex(dma_nodes);
    dma_nodes = NULL;

    return ret;
}

/* stub */
int devmm_init_process_notice_pm(struct devmm_svm_process *svm_proc)
{
    return 0;
}

int devmm_release_process_notice_pm(struct devmm_svm_process *svm_proc)
{
    return 0;
}

u32 devmm_get_vfid_from_dev_id(struct devmm_memory_attributes *attr)
{
    return attr->vfid;
}

bool devmm_is_same_dev(u32 src_devid, u32 dst_devid)
{
    if (src_devid == dst_devid) {
        return true;
    }
    return false;
}

bool devmm_current_is_vdev(void)
{
    return false;
}

int devmm_get_host_phy_mach_flag(u32 devid, u32 *host_flag)
{
    static bool get_flag = false;
    static u32 get_host_flag;
    int ret;

    if (get_flag == false) {
        ret = devdrv_get_host_phy_mach_flag(devid, &get_host_flag);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Get host physics flag failed.(devid=%u;ret=%d).\n", devid, ret);
            return ret;
#endif
        }
        get_flag = true;
    }
    *host_flag = get_host_flag;
    return 0;
}

int devmm_get_host_run_mode(u32 devid)
{
    u32 phy_flag;
    int ret;

    ret = devmm_get_host_phy_mach_flag(devid, &phy_flag);
    if (ret != 0) {
        return DEVMM_HOST_IS_UNKNOWN;
    }

    return (phy_flag == 0) ? DEVMM_HOST_IS_VIRT : DEVMM_HOST_IS_PHYS;
}

void devmm_notify_ts_drv_to_release(u32 devid, ka_pid_t pid)
{
    return;
}

bool devmm_is_mdev_vm_boot_mode(u32 devid)
{
#ifndef EMU_ST
    return devdrv_is_mdev_vm_boot_mode(devid);
#else
    return false;
#endif
}

#ifndef EMU_ST
/* include vm pass through and vm mdev calculation grp */
bool devmm_is_hccs_vm_scene(u32 dev_id, u32 host_mode)
{
    if (((host_mode == DEVDRV_VIRT_PASS_THROUGH_MACH_FLAG) || devmm_is_mdev_vm_boot_mode(dev_id))
        && devmm_is_hccs_connect(dev_id)) {
        return true;
    }
    return false;
}
#endif

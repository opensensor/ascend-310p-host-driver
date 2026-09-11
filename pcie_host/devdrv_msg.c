/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/io.h>

#include "devdrv_dma.h"
#include "devdrv_ctrl.h"
#include "devdrv_common_msg.h"
#include "devdrv_msg.h"
#include "devdrv_pci.h"
#include "devdrv_msg_def.h"
#include "devdrv_util.h"
#include "nvme_adapt.h"
#include "devdrv_pcie_link_info.h"
#include "devdrv_mem_alloc.h"

static struct {
    char str[DEVDRV_STR_NAME_LEN];
} devdrv_msg_client_type_str[devdrv_msg_client_max + 1] = {
    { "msg_client_vnic" },                /* devdrv_msg_client_pcivnic */
    { "msg_client_smmu" },                /* devdrv_msg_client_smmu */
    { "msg_client_devmm" },               /* devdrv_msg_client_devmm */
    { "msg_client_common" },              /* devdrv_msg_client_common */
    { "msg_client_devmanager" },          /* devdrv_msg_client_devmanager */
    { "msg_client_tsdrv" },               /* devdrv_msg_client_tsdrv */
    { "msg_client_hdc" },                 /* devdrv_msg_client_hdc */
    { "msg_client_queue" },               /* devdrv_msg_client_queue */
    { "msg_client_s2s" },                 /* devdrv_msg_client_s2s */
};

static struct {
    char str[DEVDRV_STR_NAME_LEN];
} devdrv_common_msg_type_str[DEVDRV_COMMON_MSG_TYPE_MAX + 1] = {
    { "common_msg_vnic" },                /* DEVDRV_COMMON_MSG_PCIVNIC */
    { "common_msg_smmu" },                /* DEVDRV_COMMON_MSG_SMMU */
    { "common_msg_devmm" },               /* DEVDRV_COMMON_MSG_DEVMM */
    { "common_msg_vmng" },                /* DEVDRV_COMMON_MSG_VMNG */
    { "common_msg_prof" },                /* DEVDRV_COMMON_MSG_PROFILE */
    { "common_msg_devmanager" },          /* DEVDRV_COMMON_MSG_DEVDRV_MANAGER */
    { "common_msg_tsdrv" },               /* DEVDRV_COMMON_MSG_DEVDRV_TSDRV */
    { "common_msg_hdc" },                 /* DEVDRV_COMMON_MSG_HDC */
    { "common_msg_sysfs" },               /* DEVDRV_COMMON_MSG_SYSFS */
    { "common_msg_esched" },              /* DEVDRV_COMMON_MSG_ESCHED */
    { "common_msg_dpmng" },               /* DEVDRV_COMMON_MSG_DP_PROC_MNG */
    { "common_msg_udis" },                /* DEVDRV_COMMON_MSG_UDIS */
    { "common_msg" },                     /* DEVDRV_COMMON_MSG_TYPE_MAX; must be last */
};

STATIC const char *devdrv_msg_type_str(u32 client_type, u32 common_msg_type)
{
    if (client_type >= devdrv_msg_client_max) {
        return "client_type_invaild";
    }

    if ((client_type == devdrv_msg_client_common) && (common_msg_type > DEVDRV_COMMON_MSG_TYPE_MAX)) {
        return "common_type_invaild";
    }

    if (client_type == devdrv_msg_client_common) {
        return devdrv_common_msg_type_str[common_msg_type].str;
    } else {
        return devdrv_msg_client_type_str[client_type].str;
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static inline void *dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
    void *ret = devdrv_ka_dma_alloc_coherent(dev, size, dma_handle, flag | __GFP_ZERO);
    return ret;
}
#endif

void devdrv_set_device_status(struct devdrv_pci_ctrl *pci_ctrl, u32 status)
{
    pci_ctrl->device_status = status;
}

STATIC u32 devdrv_get_device_status(struct devdrv_msg_chan *msg_chan)
{
    return msg_chan->msg_dev->pci_ctrl->device_status;
}

STATIC struct devdrv_msg_chan *devdrv_get_msg_chan_by_id(u32 dev_id, u32 chan_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_msg_dev *msg_dev = NULL;

    pci_ctrl = devdrv_pci_ctrl_get(dev_id);
    if (pci_ctrl == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_spinlock("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        } else {
            devdrv_err_spinlock("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        }
        return NULL;
    }

    msg_dev = pci_ctrl->msg_dev;
    if (msg_dev == NULL) {
        devdrv_pci_ctrl_put(pci_ctrl);
        devdrv_err_spinlock("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    if (chan_id >= msg_dev->chan_cnt) {
        devdrv_pci_ctrl_put(pci_ctrl);
        devdrv_err_spinlock("chan_id is invalid. (dev_id=%u; chan_id=%u)\n", dev_id, chan_id);
        return NULL;
    }

    return &msg_dev->msg_chan[chan_id];
}

struct devdrv_msg_chan *devdrv_get_msg_chan(const void *chan_handle)
{
    const DEVDRV_MSG_HANDLE *handle = (const DEVDRV_MSG_HANDLE *)&chan_handle;

    if (handle->bits.magic != DEVDRV_MSG_MAGIC) {
        devdrv_err_spinlock("magic is invalid. (magic=0x%x)\n", (u32)handle->bits.magic);
        return NULL;
    }

    return devdrv_get_msg_chan_by_id((u32)handle->bits.dev_id, (u32)handle->bits.chan_id);
}

void devdrv_put_msg_chan(const struct devdrv_msg_chan *chan)
{
    if ((chan != NULL) && (chan->msg_dev != NULL) && (chan->msg_dev->pci_ctrl != NULL)) {
        devdrv_pci_ctrl_put(chan->msg_dev->pci_ctrl);
    }
}

struct devdrv_msg_chan *devdrv_find_msg_chan(const void *chan_handle)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(chan_handle);
    if (chan != NULL) {
        devdrv_put_msg_chan(chan); /* free chan or msg rx no need to add ref_cnt */
    }
    return chan;
}

void *devdrv_generate_msg_handle(const struct devdrv_msg_chan *chan)
{
    DEVDRV_MSG_HANDLE msg_handle;

    msg_handle.bits.dev_id = chan->msg_dev->pci_ctrl->dev_id;
    msg_handle.bits.chan_id = chan->chan_id;
    msg_handle.bits.magic = DEVDRV_MSG_MAGIC;
    msg_handle.bits.reserved = 0;

    return (void *)msg_handle.value;
}

STATIC u32 devdrv_msg_alloc_slave_mem(struct devdrv_msg_dev *msg_dev, u32 len)
{
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct devdrv_msg_slave_mem_node *node = NULL;
    u32 offset = 0;

    len = roundup(len, DEVDRV_MSG_QUEUE_MEM_ALIGN);

    mutex_lock(&msg_dev->mutex);

    /* alloc memory from the allocated queue first */
    if (list_empty_careful(&msg_dev->slave_mem_list) == 0) {
        list_for_each_safe(pos, n, &msg_dev->slave_mem_list)
        {
            node = list_entry(pos, struct devdrv_msg_slave_mem_node, list);
            if (node->mem.len == len) {
                devdrv_debug("Get reuse len. (dev_id=%d; len=0x%x)\n", msg_dev->pci_ctrl->dev_id, len);
                offset = node->mem.offset;
                list_del(&node->list);
                devdrv_kfree(node);
                node = NULL;
                break;
            }
        }
    }

    /* alloc memory for the first time */
    if (offset == 0) {
        if (len <= msg_dev->slave_mem.len) {
            offset = msg_dev->slave_mem.offset;
            msg_dev->slave_mem.offset += len;
            msg_dev->slave_mem.len -= len;
            devdrv_debug("Slave memory alloc. (dev_id=%d; len=0x%x; remain=0x%x)\n",
                msg_dev->pci_ctrl->dev_id, len, msg_dev->slave_mem.len);
        } else {
            devdrv_err("Slave memory is used up. dev_id=%d; len=0x%x; remain=0x%x)\n",
                msg_dev->pci_ctrl->dev_id, len, msg_dev->slave_mem.len);
        }
    }

    mutex_unlock(&msg_dev->mutex);

    return offset;
}

STATIC void devdrv_msg_free_slave_mem(struct devdrv_msg_dev *msg_dev, u32 offset, u32 len)
{
    struct devdrv_msg_slave_mem_node *node = NULL;

    len = roundup(len, DEVDRV_MSG_QUEUE_MEM_ALIGN);

    node = (struct devdrv_msg_slave_mem_node *)devdrv_kzalloc(sizeof(struct devdrv_msg_slave_mem_node), GFP_KERNEL);
    if (node == NULL) {
        devdrv_err("Alloc node failed, free slave mem offset. (dev_id=%d; offset=%d; len=%d)\n",
                   msg_dev->pci_ctrl->dev_id, offset, len);
        return;
    }

    node->mem.offset = offset;
    node->mem.len = len;

    /* Repeat alloc the msg chan after release is a debugging function. It is simple to implement here.
        Join the linked list. Re-take directly from here next time. not support length changes in next time */
    mutex_lock(&msg_dev->mutex);
    list_add(&node->list, &msg_dev->slave_mem_list);
    mutex_unlock(&msg_dev->mutex);
}

STATIC int devdrv_admin_msg_chan_alloc_host_sq(struct devdrv_msg_chan *chan, u32 size)
{
    u32 align_size = round_up(size, PAGE_SIZE);

    if (chan->msg_dev->pci_ctrl->connect_protocol == CONNECT_PROTOCOL_PCIE) {
        chan->sq_info.desc_h = dma_zalloc_coherent(chan->msg_dev->dev, size, &chan->sq_info.dma_handle,
                                                   GFP_KERNEL | __GFP_DMA);
        if (chan->sq_info.desc_h == NULL) {
            devdrv_err("msg_sq alloc  failed. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
            return -ENOMEM;
        }
    } else {
        /* hccs peh, use phy addr, if host smmu enabled, need convert by agent-smmu */
        chan->sq_info.desc_h = devdrv_kzalloc(align_size, GFP_KERNEL);
        if (chan->sq_info.desc_h == NULL) {
            devdrv_err("devdrv_kzalloc failed. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
            return -ENOMEM;
        }

        chan->sq_info.dma_handle = dma_map_single(chan->msg_dev->dev, chan->sq_info.desc_h, align_size, DMA_BIDIRECTIONAL);
        if (dma_mapping_error(chan->msg_dev->dev, chan->sq_info.dma_handle) != 0) {
            devdrv_kfree(chan->sq_info.desc_h);
            devdrv_err("Admin dma_mapping failed. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
            return -ENOMEM;
        }
    }

    return 0;
}

STATIC void devdrv_admin_msg_chan_free_host_sq(struct devdrv_msg_chan *chan, u32 size)
{
    u32 align_size = round_up(size, PAGE_SIZE);

    if (chan->sq_info.desc_h == NULL) {
        return;
    }

    if (chan->msg_dev->pci_ctrl->connect_protocol == CONNECT_PROTOCOL_PCIE) {
        devdrv_ka_dma_free_coherent(chan->msg_dev->dev, size, chan->sq_info.desc_h, chan->sq_info.dma_handle);
    } else {
        dma_unmap_single(chan->msg_dev->dev, chan->sq_info.dma_handle, align_size, DMA_BIDIRECTIONAL);
        devdrv_kfree(chan->sq_info.desc_h);
    }
    chan->sq_info.desc_h = NULL;
    chan->sq_info.dma_handle = (~(dma_addr_t)0);
}

STATIC int devdrv_msg_alloc_host_sq(struct devdrv_msg_chan *chan, u32 depth, u32 bd_size)
{
    u32 size = depth * bd_size;

    if (chan->chan_id != 0) {
        chan->sq_info.desc_h = devdrv_dma_zalloc_coherent(chan->msg_dev->dev, size, &chan->sq_info.dma_handle,
            GFP_KERNEL | __GFP_DMA);
        if (chan->sq_info.desc_h == NULL) {
            devdrv_err("msg_alloc_sq failed. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
            return -ENOMEM;
        }
    } else {
        if (devdrv_admin_msg_chan_alloc_host_sq(chan, size) != 0) {
            return -ENOMEM;
        }
    }

    chan->sq_info.desc_size = bd_size;
    chan->sq_info.irq_vector = -1;
    chan->sq_info.depth = depth;
    chan->sq_info.head_h = 0;
    chan->sq_info.tail_h = 0;

    return 0;
}

STATIC int devdrv_msg_free_host_sq(struct devdrv_msg_chan *msg_chan)
{
    u32 free_size;

    if (msg_chan->sq_info.desc_h != NULL) {
        free_size = msg_chan->sq_info.desc_size * msg_chan->sq_info.depth;
        if (msg_chan->chan_id != 0) {
            devdrv_dma_free_coherent(msg_chan->msg_dev->dev, free_size,
                msg_chan->sq_info.desc_h, msg_chan->sq_info.dma_handle);
        } else {
            devdrv_admin_msg_chan_free_host_sq(msg_chan, free_size);
        }
        msg_chan->sq_info.desc_h = NULL;
    }

    return 0;
}

STATIC int devdrv_msg_alloc_host_cq(struct devdrv_msg_chan *chan, int depth, int bd_size)
{
    u32 alloc_size = (u32)(depth * bd_size);

    chan->cq_info.desc_h = devdrv_dma_zalloc_coherent(chan->msg_dev->dev, alloc_size, &chan->cq_info.dma_handle,
                                               GFP_KERNEL | __GFP_DMA);

    if (chan->cq_info.desc_h == NULL) {
        devdrv_err("msg_alloc_cq failed. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
        return -ENOMEM;
    }

    chan->cq_info.depth = (u32)depth;
    chan->cq_info.desc_size = (u32)bd_size;
    chan->cq_info.irq_vector = -1;
    chan->cq_info.head_h = 0;
    chan->cq_info.tail_h = 0;

    return 0;
}

STATIC int devdrv_msg_free_host_cq(struct devdrv_msg_chan *msg_chan)
{
    u32 free_size;

    if (msg_chan->cq_info.desc_h != NULL) {
        free_size = msg_chan->cq_info.desc_size * msg_chan->cq_info.depth;
        devdrv_dma_free_coherent(msg_chan->msg_dev->dev, free_size, msg_chan->cq_info.desc_h,
            msg_chan->cq_info.dma_handle);
        msg_chan->cq_info.desc_h = NULL;
    }

    return 0;
}

STATIC int devdrv_msg_alloc_slave_sq(struct devdrv_msg_chan *chan, u32 depth, u32 bd_size)
{
    struct devdrv_msg_dev *msg_dev = NULL;
    u32 queue_size;
    u32 offset;

    msg_dev = chan->msg_dev;

    queue_size = depth * bd_size;
    offset = devdrv_msg_alloc_slave_mem(msg_dev, queue_size);
    if (offset == 0) {
        devdrv_err("Queue alloc failed. (dev_id=%u; size=%u)\n", chan->msg_dev->pci_ctrl->dev_id, queue_size);
        return -ENOMEM;
    }

    chan->sq_info.desc_d = msg_dev->reserve_mem_base + offset;
    chan->sq_info.head_d = 0;
    chan->sq_info.tail_d = 0;
    chan->sq_info.slave_mem_offset = offset;

    return 0;
}

static int devdrv_msg_free_slave_sq(struct devdrv_msg_chan *msg_chan)
{
    u32 queue_size = msg_chan->sq_info.depth * msg_chan->sq_info.desc_size;

    msg_chan->sq_info.desc_d = NULL;

    devdrv_msg_free_slave_mem(msg_chan->msg_dev, msg_chan->sq_info.slave_mem_offset, queue_size);

    return 0;
}

STATIC int devdrv_msg_alloc_slave_cq(struct devdrv_msg_chan *chan, u32 depth, u32 bd_size)
{
    struct devdrv_msg_dev *msg_dev = NULL;
    u32 queue_size;
    u32 offset;

    msg_dev = chan->msg_dev;

    queue_size = depth * bd_size;
    offset = devdrv_msg_alloc_slave_mem(msg_dev, queue_size);
    if (offset == 0) {
        devdrv_err("Queue alloc failed. (dev_id=%u; size=%u)\n", msg_dev->pci_ctrl->dev_id, queue_size);
        return -ENOMEM;
    }

    chan->cq_info.desc_d = msg_dev->reserve_mem_base + offset;
    chan->cq_info.head_d = 0;
    chan->cq_info.tail_d = 0;
    chan->cq_info.slave_mem_offset = offset;

    return 0;
}

static int devdrv_msg_free_slave_cq(struct devdrv_msg_chan *msg_chan)
{
    u32 queue_size = msg_chan->cq_info.depth * msg_chan->cq_info.desc_size;
    msg_chan->cq_info.desc_d = NULL;

    devdrv_msg_free_slave_mem(msg_chan->msg_dev, msg_chan->cq_info.slave_mem_offset, queue_size);

    return 0;
}

#ifdef CFG_FEATURE_SEC_COMM_L3
STATIC int devdrv_msg_alloc_host_rsv_sq(struct devdrv_msg_chan *chan, u32 depth, u32 bd_size)
{
    u32 rsv_offset = chan->sq_info.slave_mem_offset;
    struct devdrv_msg_dev *msg_dev = chan->msg_dev;
    phys_addr_t rsv_phys_addr;
    struct page *page = NULL;
    dma_addr_t dma_addr_rsv;
    u32 queue_size;
    int ret;

    queue_size = depth * bd_size;
    chan->sq_info.base_reserve_h = msg_dev->local_reserve_mem_base + rsv_offset;
    ret = memset_s(chan->sq_info.base_reserve_h, queue_size, 0, queue_size);
    if (ret != 0) {
        devdrv_err("Call memset_s failed. (ret=%d)\n", ret);
        return ret;
    }

    rsv_phys_addr = msg_dev->pci_ctrl->res.rsv_phy_addr + rsv_offset;
    page = phys_to_page(rsv_phys_addr);
    dma_addr_rsv = dma_map_page(msg_dev->dev, page, rsv_phys_addr % PAGE_SIZE, (size_t)queue_size, DMA_BIDIRECTIONAL);
    if (dma_mapping_error(msg_dev->dev, dma_addr_rsv)) {
        devdrv_err("DMA mapping error.\n");
        return -ENOMEM;
    }
    chan->sq_info.dma_reserve_h = dma_addr_rsv;

    return 0;
}
#endif

STATIC int devdrv_msg_free_host_rsv_sq(struct devdrv_msg_chan *msg_chan)
{
    u32 queue_size = msg_chan->sq_info.depth * msg_chan->sq_info.desc_size;

    if (msg_chan->sq_info.dma_reserve_h != 0) {
        dma_unmap_page(msg_chan->msg_dev->dev, msg_chan->sq_info.dma_reserve_h,
                       (size_t)queue_size, DMA_BIDIRECTIONAL);
        msg_chan->sq_info.dma_reserve_h = 0;
    }
    msg_chan->sq_info.base_reserve_h = NULL;

    return 0;
}

#ifdef CFG_FEATURE_SEC_COMM_L3
STATIC int devdrv_msg_alloc_host_rsv_cq(struct devdrv_msg_chan *chan, u32 depth, u32 bd_size)
{
    u32 rsv_offset = chan->cq_info.slave_mem_offset;
    struct devdrv_msg_dev *msg_dev = chan->msg_dev;
    phys_addr_t rsv_phys_addr;
    struct page *page = NULL;
    dma_addr_t dma_addr_rsv;
    u32 queue_size;
    int ret;

    queue_size = depth * bd_size;
    chan->cq_info.base_reserve_h = msg_dev->local_reserve_mem_base + rsv_offset;
    ret = memset_s(chan->cq_info.base_reserve_h, queue_size, 0, queue_size);
    if (ret != 0) {
        devdrv_err("Call memset_s failed. (ret=%d)\n", ret);
        return ret;
    }

    rsv_phys_addr = msg_dev->pci_ctrl->res.rsv_phy_addr + rsv_offset;
    page = phys_to_page(rsv_phys_addr);
    dma_addr_rsv = dma_map_page(msg_dev->dev, page, rsv_phys_addr % PAGE_SIZE, (size_t)queue_size, DMA_BIDIRECTIONAL);
    if (dma_mapping_error(msg_dev->dev, dma_addr_rsv)) {
        devdrv_err("DMA mapping error.\n");
        return -ENOMEM;
    }
    chan->cq_info.dma_reserve_h = dma_addr_rsv;

    return 0;
}
#endif

STATIC int devdrv_msg_free_host_rsv_cq(struct devdrv_msg_chan *msg_chan)
{
    u32 queue_size = msg_chan->cq_info.depth * msg_chan->cq_info.desc_size;

    if (msg_chan->cq_info.dma_reserve_h != 0) {
        dma_unmap_page(msg_chan->msg_dev->dev, msg_chan->cq_info.dma_reserve_h,
                       (size_t)queue_size, DMA_BIDIRECTIONAL);
        msg_chan->cq_info.dma_reserve_h = 0;
    }
    msg_chan->cq_info.base_reserve_h = NULL;

    return 0;
}

#ifdef CFG_FEATURE_SEC_COMM_L3
STATIC int devdrv_msg_save_slave_sqcq_dma(struct devdrv_msg_chan *msg_chan,
                                    dma_addr_t slave_sq_dma, dma_addr_t slave_cq_dma)
{
    msg_chan->sq_info.dma_reserve_d = slave_sq_dma;
    msg_chan->cq_info.dma_reserve_d = slave_cq_dma;

    return 0;
}
#endif

STATIC int devdrv_msg_alloc_s_queue(struct devdrv_msg_chan *msg_chan, u32 depth, u32 size)
{
    int ret;

    ret = devdrv_msg_alloc_host_sq(msg_chan, depth, size);
    if (ret != 0) {
        devdrv_err("devdrv_msg_alloc_host_sq failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_msg_alloc_slave_sq(msg_chan, depth, size);
    if (ret != 0) {
        devdrv_err("devdrv_msg_alloc_slave_sq failed. (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_SEC_COMM_L3
    ret = devdrv_msg_alloc_host_rsv_sq(msg_chan, depth, size);
#endif
    return ret;
}

STATIC int devdrv_msg_alloc_c_queue(struct devdrv_msg_chan *msg_chan, u32 depth, u32 size)
{
    int ret;

    ret = devdrv_msg_alloc_host_cq(msg_chan, (int)depth, (int)size);
    if (ret != 0) {
        devdrv_err("devdrv_msg_alloc_c_queue failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_msg_alloc_slave_cq(msg_chan, depth, size);
    if (ret != 0) {
        devdrv_err("devdrv_msg_alloc_slave_cq failed. (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_SEC_COMM_L3
    ret = devdrv_msg_alloc_host_rsv_cq(msg_chan, depth, size);
#endif
    return ret;
}

STATIC int devdrv_msg_free_sq(struct devdrv_msg_chan *msg_chan)
{
    (void)devdrv_msg_free_host_sq(msg_chan);

    (void)devdrv_msg_free_slave_sq(msg_chan);

    (void)devdrv_msg_free_host_rsv_sq(msg_chan);

    msg_chan->sq_info.dma_reserve_d = 0;
    return 0;
}

STATIC int devdrv_msg_free_cq(struct devdrv_msg_chan *msg_chan)
{
    (void)devdrv_msg_free_host_cq(msg_chan);

    (void)devdrv_msg_free_slave_cq(msg_chan);

    (void)devdrv_msg_free_host_rsv_cq(msg_chan);

    msg_chan->cq_info.dma_reserve_d = 0;
    return 0;
}

/* write doorbell to notify dev */
STATIC void devdrv_msg_ring_doorbell_inner(void *msg_chan)
{
    struct devdrv_msg_chan *chan = (struct devdrv_msg_chan *)msg_chan;
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    devdrv_set_sq_doorbell(chan->io_base, 0x1);
}

void devdrv_msg_ring_doorbell(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    devdrv_msg_ring_doorbell_inner(chan);
    devdrv_put_msg_chan(chan);
}
EXPORT_SYMBOL(devdrv_msg_ring_doorbell);

void devdrv_msg_ring_cq_doorbell(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    devdrv_set_cq_doorbell(chan->io_base, 0x1);
    devdrv_put_msg_chan(chan);
}
EXPORT_SYMBOL(devdrv_msg_ring_cq_doorbell);

/* devid */
int devdrv_get_msg_chan_devid(void *msg_chan)
{
    int dev_id;
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("msg_chan is null.\n");
        } else {
            devdrv_err_limit("msg_chan is null.\n");
        }
        return -1;
    }
    dev_id = (int)chan->msg_dev->pci_ctrl->dev_id;
    devdrv_put_msg_chan(chan);
    return dev_id;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_devid);

/* priv */
int devdrv_set_msg_chan_priv(void *msg_chan, void *priv)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("msg_chan is null.\n");
        } else {
            devdrv_err_limit("msg_chan is null.\n");
        }
        return -EINVAL;
    }
    chan->priv = priv;
    devdrv_put_msg_chan(chan);
    return 0;
}
EXPORT_SYMBOL(devdrv_set_msg_chan_priv);

void *devdrv_get_msg_chan_priv(void *msg_chan)
{
    void *priv = NULL;
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("msg_chan is null.\n");
        } else {
            devdrv_err_limit("msg_chan is null.\n");
        }
        return NULL;
    }
    priv = chan->priv;
    devdrv_put_msg_chan(chan);
    return priv;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_priv);

/* host sq */
void *devdrv_get_msg_chan_host_sq_head(void *msg_chan, u32 *head)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    void *sq_head = NULL;
    u64 offset;

    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return NULL;
    }

    if (head == NULL) {
        devdrv_put_msg_chan(chan);
        devdrv_err_spinlock("head is null.\n");
        return NULL;
    }

    *head = chan->sq_info.head_h;
    offset = (u64)chan->sq_info.head_h * chan->sq_info.desc_size;
    sq_head = (void *)((char *)chan->sq_info.desc_h + offset);

    devdrv_put_msg_chan(chan);
    return sq_head;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_host_sq_head);

void devdrv_move_msg_chan_host_sq_head(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    if (chan->sq_info.depth != 0) {
        chan->sq_info.head_h = (chan->sq_info.head_h + 1) % chan->sq_info.depth;
    }
    devdrv_put_msg_chan(chan);
}
EXPORT_SYMBOL(devdrv_move_msg_chan_host_sq_head);

/* host cq */
void *devdrv_get_msg_chan_host_cq_head(void *msg_chan)
{
    void *cq_head = NULL;
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    u64 offset;

    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return NULL;
    }
    offset = (u64)chan->cq_info.head_h * chan->cq_info.desc_size;
    cq_head = (void *)((char *)chan->cq_info.desc_h + offset);

    devdrv_put_msg_chan(chan);
    return cq_head;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_host_cq_head);

void devdrv_move_msg_chan_host_cq_head(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    if (chan->cq_info.depth != 0) {
        chan->cq_info.head_h = (chan->cq_info.head_h + 1) % chan->cq_info.depth;
    }
    devdrv_put_msg_chan(chan);
}
EXPORT_SYMBOL(devdrv_move_msg_chan_host_cq_head);

/* slave sq */
void devdrv_set_msg_chan_slave_sq_head(void *msg_chan, u32 head)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    chan->sq_info.head_d = head;
    devdrv_put_msg_chan(chan);
}
EXPORT_SYMBOL(devdrv_set_msg_chan_slave_sq_head);

void *devdrv_get_msg_chan_slave_sq_tail(void *msg_chan, u32 *tail)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    void *sq_tail = NULL;
    u64 offset;

    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return NULL;
    }

    if (tail == NULL) {
        devdrv_put_msg_chan(chan);
        devdrv_err_spinlock("tail is null.\n");
        return NULL;
    }

    *tail = chan->sq_info.tail_d;
    offset = (u64)chan->sq_info.tail_d * chan->sq_info.desc_size;
    sq_tail = (void *)((char *)chan->sq_info.desc_d + offset);

    devdrv_put_msg_chan(chan);
    return sq_tail;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_slave_sq_tail);

#ifdef CFG_FEATURE_SEC_COMM_L3
void *devdrv_get_msg_chan_host_rsv_sq_tail(void *msg_chan, u32 *tail)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    void *sq_tail = NULL;
    u64 offset;

    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return NULL;
    }

    if (tail == NULL) {
        devdrv_put_msg_chan(chan);
        devdrv_err_spinlock("tail is null.\n");
        return NULL;
    }

    *tail = chan->sq_info.tail_d;
    offset = (u64)chan->sq_info.tail_d * chan->sq_info.desc_size;
    sq_tail = (void *)((char *)chan->sq_info.base_reserve_h + offset);

    devdrv_put_msg_chan(chan);
    return sq_tail;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_host_rsv_sq_tail);

void *devdrv_get_msg_chan_host_rsv_cq_tail(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    void *cq_tail = NULL;
    u64 offset;

    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return NULL;
    }
    offset = (u64)chan->cq_info.tail_d * chan->cq_info.desc_size;
    cq_tail = (void *)((char *)chan->cq_info.base_reserve_h + offset);

    devdrv_put_msg_chan(chan);
    return cq_tail;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_host_rsv_cq_tail);

int devdrv_dma_copy_sq_desc_to_slave(void *msg_chan, struct devdrv_asyn_dma_para_info *para,
                                     enum devdrv_dma_data_type data_type, int instance)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    dma_addr_t src, dst;
    int ret;
    u32 len;

    if (chan == NULL) {
        devdrv_err("msg_chan is null.\n");
        return -EINVAL;
    }

    src = chan->sq_info.dma_reserve_h + chan->sq_info.tail_d * chan->sq_info.desc_size;
    dst = chan->sq_info.dma_reserve_d + chan->sq_info.tail_d * chan->sq_info.desc_size;
    len = chan->sq_info.desc_size;
    dma_sync_single_for_device(chan->msg_dev->dev, src, len, DMA_TO_DEVICE);
    ret = devdrv_dma_async_copy_plus(chan->msg_dev->pci_ctrl->dev_id, data_type, instance, src, dst,
                                     len, DEVDRV_DMA_HOST_TO_DEVICE, para);
    if (ret != 0) {
        devdrv_err("dma copy fail. (ret=%d)\n", ret);
    }

    devdrv_put_msg_chan(chan);
    return ret;
}
EXPORT_SYMBOL(devdrv_dma_copy_sq_desc_to_slave);

int devdrv_dma_copy_cq_desc_to_slave(void *msg_chan, struct devdrv_asyn_dma_para_info *para,
                                     enum devdrv_dma_data_type data_type, int instance)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    dma_addr_t src, dst;
    int ret;
    u32 len;

    if (chan == NULL) {
        devdrv_err("msg_chan is null.\n");
        return -EINVAL;
    }

    src = chan->cq_info.dma_reserve_h + chan->cq_info.tail_d * chan->cq_info.desc_size;
    dst = chan->cq_info.dma_reserve_d + chan->cq_info.tail_d * chan->cq_info.desc_size;
    len = chan->cq_info.desc_size;
    dma_sync_single_for_device(chan->msg_dev->dev, src, len, DMA_TO_DEVICE);
    ret = devdrv_dma_async_copy_plus(chan->msg_dev->pci_ctrl->dev_id, data_type, instance, src, dst,
                                     len, DEVDRV_DMA_HOST_TO_DEVICE, para);
    if (ret != 0) {
        devdrv_err("dma copy fail. (ret=%d)\n", ret);
    }

    devdrv_put_msg_chan(chan);
    return ret;
}
EXPORT_SYMBOL(devdrv_dma_copy_cq_desc_to_slave);
#endif
void devdrv_move_msg_chan_slave_sq_tail(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    if (chan->sq_info.depth != 0) {
        chan->sq_info.tail_d = (chan->sq_info.tail_d + 1) % chan->sq_info.depth;
    }
    devdrv_put_msg_chan(chan);
}
EXPORT_SYMBOL(devdrv_move_msg_chan_slave_sq_tail);

bool devdrv_msg_chan_slave_sq_full_check(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return true;
    }
    if ((chan->sq_info.depth != 0) && (((chan->sq_info.tail_d + 1) % chan->sq_info.depth) == chan->sq_info.head_d)) {
        devdrv_put_msg_chan(chan);
        return true;
    } else {
        devdrv_put_msg_chan(chan);
        return false;
    }
}
EXPORT_SYMBOL(devdrv_msg_chan_slave_sq_full_check);

/* slave cq */
void *devdrv_get_msg_chan_slave_cq_tail(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    void *cq_tail = NULL;
    u64 offset;

    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return NULL;
    }
    offset = (u64)chan->cq_info.tail_d * chan->cq_info.desc_size;
    cq_tail = (void *)((char *)chan->cq_info.desc_d + offset);

    devdrv_put_msg_chan(chan);
    return cq_tail;
}
EXPORT_SYMBOL(devdrv_get_msg_chan_slave_cq_tail);

void devdrv_move_msg_chan_slave_cq_tail(void *msg_chan)
{
    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        devdrv_err_spinlock("msg_chan is null.\n");
        return;
    }
    if (chan->cq_info.depth != 0) {
        chan->cq_info.tail_d = (chan->cq_info.tail_d + 1) % chan->cq_info.depth;
    }
    devdrv_put_msg_chan(chan);
}
EXPORT_SYMBOL(devdrv_move_msg_chan_slave_cq_tail);

STATIC bool devdrv_judge_chan_invalid_by_level(u32 level, u32 chan_id)
{
    if (level == DEVDRV_MSG_CHAN_LEVEL_LOW) {
        if ((chan_id % AGENTDRV_NVME_DB_IRQ_STRDE) >= AGENTDRV_NVME_LOW_LEVEL_DB_IRQ_NUM) {
            return true;
        }
    } else {
        if ((chan_id % AGENTDRV_NVME_DB_IRQ_STRDE) < AGENTDRV_NVME_LOW_LEVEL_DB_IRQ_NUM) {
            return true;
        }
    }
    return false;
}

/* alloc a msg_chan node from msg_dev struct */
STATIC struct devdrv_msg_chan *devdrv_alloc_msg_chan(struct devdrv_msg_dev *msg_dev, u32 level)
{
    u32 i;
    int level_check = DEVDRV_ENABLE;
    struct devdrv_msg_chan *msg_chan = NULL;

retry:

    mutex_lock(&msg_dev->mutex);
    for (i = 0; i < msg_dev->chan_cnt; i++) {
        if (level_check == DEVDRV_ENABLE) {
            if (devdrv_judge_chan_invalid_by_level(level, i) == true) {
                continue;
            }
        }
        if (msg_dev->msg_chan[i].status == DEVDRV_DISABLE) {
            msg_dev->msg_chan[i].status = DEVDRV_ENABLE;
            atomic_set(&msg_dev->msg_chan[i].sched_status.state,
                DEVDRV_MSG_HANDLE_STATE_INIT);
            msg_chan = &msg_dev->msg_chan[i];
            break;
        }
    }
    mutex_unlock(&msg_dev->mutex);
    if (msg_chan == NULL) {
        if (level_check == DEVDRV_ENABLE) {
            level_check = DEVDRV_DISABLE;
            goto retry;
        }

        devdrv_err("Alloc msg_chan failed. (dev_id=%d; level=%d)\n", msg_dev->pci_ctrl->dev_id, level);
    } else {
        devdrv_debug("Alloc msg_chan success. (dev_id=%d; level=%d; chan_id=%d)\n",
                     msg_dev->pci_ctrl->dev_id, level, msg_chan->chan_id);
    }

    return msg_chan;
}

STATIC int devdrv_device_status_abnormal_check_inner(const void *msg_chan)
{
    struct devdrv_msg_chan *msg_chan_tmp = (struct devdrv_msg_chan *)msg_chan;

    if (msg_chan_tmp == NULL) {
        devdrv_err_spinlock("Check failed, msg_chan_tmp is NULL.\n");
        return -EINVAL;
    }

    if (msg_chan_tmp->msg_dev == NULL) {
        devdrv_err_spinlock("Check failed, msg_dev is NULL.\n");
        return -EINVAL;
    }

    if (msg_chan_tmp->msg_dev->pci_ctrl == NULL) {
        devdrv_err_spinlock("Check failed, pci_ctrl is NULL.\n");
        return -EINVAL;
    }

    if ((msg_chan_tmp->msg_dev->pci_ctrl->device_status == DEVDRV_DEVICE_DEAD) ||
        (msg_chan_tmp->msg_dev->pci_ctrl->device_status == DEVDRV_DEVICE_UDA_RM)) {
        devdrv_err_spinlock("Check failed, device_status is dead.\n");
        return -EINVAL;
    }

    if (devdrv_get_pcie_channel_status() == DEVDRV_PCIE_COMMON_CHANNEL_LINKDOWN) {
        return -ENODEV;
    }

    return 0;
}

int devdrv_device_status_abnormal_check(const void *msg_chan)
{
    const struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    int ret;

    ret = devdrv_device_status_abnormal_check_inner(chan);

    devdrv_put_msg_chan(chan);
    return ret;
}
EXPORT_SYMBOL(devdrv_device_status_abnormal_check);

STATIC void devdrv_device_mutex_lock(struct devdrv_msg_chan *msg_chan)
{
    if ((msg_chan != NULL) && (msg_chan->msg_dev != NULL)) {
        mutex_lock(&msg_chan->mutex);
    }
}

STATIC void devdrv_device_mutex_unlock(struct devdrv_msg_chan *msg_chan)
{
    if ((msg_chan != NULL) && (msg_chan->msg_dev != NULL)) {
        mutex_unlock(&msg_chan->mutex);
    }
}

STATIC void devdrv_non_trans_rx_msg_task_resq_record(struct devdrv_msg_chan *msg_chan)
{
    u32 cost_time;

    cost_time = jiffies_to_msecs(jiffies - msg_chan->stamp);
    if (cost_time > msg_chan->chan_stat.rx_work_max_time) {
        msg_chan->chan_stat.rx_work_max_time = cost_time;
    }
    if (cost_time > DEVDRV_MSG_TIME_VOERFLOW) {
        msg_chan->chan_stat.rx_work_delay_cnt++;
        devdrv_info("Get host schedule msg work time. (msg_type=\"%s\"; cost_time=%ums)\n",
                    devdrv_msg_type_str(msg_chan->msg_type, DEVDRV_COMMON_MSG_TYPE_MAX), cost_time);
    }
}

#ifdef CFG_FEATURE_SEC_COMM_L3
STATIC int devdrv_non_trans_msg_dma_copy_cq(struct devdrv_msg_chan *msg_chan,
                                            struct devdrv_non_trans_msg_desc *bd_desc)
{
    enum devdrv_dma_data_type data_type = DEVDRV_DMA_DATA_PCIE_MSG;
    dma_addr_t src, dst;
    int ret;

    if ((bd_desc->status == DEVDRV_MSG_CMD_FINISH_SUCCESS) && (bd_desc->real_out_len > 0)) {
        src = msg_chan->cq_info.dma_handle + DEVDRV_NON_TRANS_MSG_HEAD_LEN;
        dst = msg_chan->cq_info.dma_reserve_d + DEVDRV_NON_TRANS_MSG_HEAD_LEN;
        ret = devdrv_dma_sync_copy(msg_chan->msg_dev->pci_ctrl->dev_id, data_type, src,
                                   dst, bd_desc->real_out_len, DEVDRV_DMA_HOST_TO_DEVICE);
        if (ret != 0) {
            devdrv_err("non_trans channel send msg reply failed. (len=%u, ret=%d)\n", bd_desc->real_out_len, ret);
            bd_desc->status = DEVDRV_MSG_CMD_FINISH_FAILED;
        }
    }
    atomic_set(&msg_chan->sched_status.state, DEVDRV_MSG_HANDLE_STATE_INIT);

    isb(); /* ensure dma submit order */

    src = msg_chan->cq_info.dma_handle;
    dst = msg_chan->cq_info.dma_reserve_d;
    ret = devdrv_dma_sync_copy(msg_chan->msg_dev->pci_ctrl->dev_id, data_type, src,
                               dst, DEVDRV_NON_TRANS_MSG_HEAD_LEN, DEVDRV_DMA_HOST_TO_DEVICE);
    if (ret != 0) {
        devdrv_err("non_trans channel send msg status failed. (ret=%d)\n", ret);
    }

    return ret;
}
#endif
STATIC void devdrv_non_trans_rx_msg_handle(struct devdrv_msg_chan *msg_chan)
{
#ifndef CFG_FEATURE_SEC_COMM_L3
    struct devdrv_non_trans_msg_desc *bd_desc_d = (struct devdrv_non_trans_msg_desc *)msg_chan->cq_info.desc_d;
#endif
    struct devdrv_non_trans_msg_desc *bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->cq_info.desc_h;
    enum devdrv_msg_client_type msg_type_tmp = msg_chan->msg_type;
    void *handle = NULL;
    u64 call_start;
    u32 resq_time;
    u64 seq_num;
    int dev_id;
    int ret;

    msg_chan->sched_status.schedule_in++;

    dev_id = (int)msg_chan->msg_dev->pci_ctrl->dev_id;
    seq_num = bd_desc->seq_num;
    if (seq_num == msg_chan->seq_num) {
        devdrv_warn("seq_num is no change. (dev_id=%d; msg_type=\"%s\"; seq_num=%lld)\n",
            dev_id, devdrv_msg_type_str(msg_type_tmp, bd_desc->msg_type), seq_num);
    }
    msg_chan->seq_num = seq_num;
    /* the dma moves the command data and the msix interrupt takes the same pcie path,
       which guarantees that the data has arrived when the interrupt arrives. */
    if ((bd_desc->status == DEVDRV_MSG_CMD_BEGIN) && (msg_chan->rx_msg_process != NULL)) {
        msg_chan->chan_stat.rx_total_cnt++;
        handle = devdrv_generate_msg_handle(msg_chan);
        call_start = jiffies;
        ret = msg_chan->rx_msg_process(handle, bd_desc->data, bd_desc->in_data_len, bd_desc->out_data_len,
                                       &bd_desc->real_out_len);
        resq_time = jiffies_to_msecs(jiffies - call_start);
        if (resq_time > DEVDRV_NON_TRANS_CB_TIME) {
            devdrv_info("Get resq_time. (dev_id=%u; msg_type=\"%s\"; resq_time=%ums; cpu=%d)\n",
                dev_id, devdrv_msg_type_str(msg_type_tmp, bd_desc->msg_type), resq_time,
                raw_smp_processor_id());
        }

        if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
            devdrv_info("Device is abnormal.(msg_type=\"%s\")\n", devdrv_msg_type_str(msg_type_tmp, bd_desc->msg_type));
            atomic_set(&msg_chan->sched_status.state, DEVDRV_MSG_HANDLE_STATE_INIT);
            return;
        }

        if ((ret == 0) && (bd_desc->real_out_len <= bd_desc->out_data_len)) {
            bd_desc->status = DEVDRV_MSG_CMD_FINISH_SUCCESS;
            msg_chan->chan_stat.rx_success_cnt++;
        } else if (ret == -EINVAL) {
            msg_chan->chan_stat.rx_para_err++;
            devdrv_warn("Unexpect rx msg process result.(dev_id=%d; msg_type=\"%s\"; ret=%d; out_buf=%d; out_len=%d)\n",
                        dev_id, devdrv_msg_type_str(msg_type_tmp, bd_desc->msg_type),
                        ret, bd_desc->out_data_len, bd_desc->real_out_len);
            bd_desc->status = DEVDRV_MSG_CMD_INVALID_PARA;
        } else if (ret == -EUNATCH) {
            devdrv_warn("Rx msg process is null. (dev_id=%d; msg_type=\"%s\"; ret=%d)\n",
                        dev_id, devdrv_msg_type_str(msg_type_tmp, bd_desc->msg_type), ret);
            bd_desc->status = DEVDRV_MSG_CMD_NULL_PROCESS_CB;
        } else {
            devdrv_err("Rx msg process fail. (dev_id=%d; msg_type=\"%s\"; ret=%d; out_buf=%d; out_len=%d)\n",
                       dev_id, devdrv_msg_type_str(msg_type_tmp, bd_desc->msg_type),
                       ret, bd_desc->out_data_len, bd_desc->real_out_len);
            bd_desc->status = DEVDRV_MSG_CMD_FINISH_FAILED;
        }
    } else {
        devdrv_err("No cmd to handle. (dev_id=%d; msg_type=\"%s\"; desc_status=%u)\n",
                   dev_id, devdrv_msg_type_str(msg_type_tmp, bd_desc->msg_type), bd_desc->status);
        atomic_set(&msg_chan->sched_status.state, DEVDRV_MSG_HANDLE_STATE_INIT);
        return;
    }
#ifndef CFG_FEATURE_SEC_COMM_L3
    /* copy the execution result to shared reserved memory pointed to by cq desc_h */
    if ((bd_desc->status == DEVDRV_MSG_CMD_FINISH_SUCCESS) && (bd_desc->real_out_len > 0)) {
        /* copy data */
        memcpy_toio((void __iomem *)bd_desc_d->data, (void *)bd_desc->data, bd_desc->real_out_len);
        wmb();
    }

    /* copy msg head, the status field must be placed at the end of the
       structure so that other data is already written when it is in effect */
    bd_desc_d->in_data_len = bd_desc->in_data_len;
    bd_desc_d->out_data_len = bd_desc->out_data_len;
    bd_desc_d->real_out_len = bd_desc->real_out_len;
    bd_desc_d->seq_num = seq_num;
    bd_desc_d->msg_type = bd_desc->msg_type;
    atomic_set(&msg_chan->sched_status.state, DEVDRV_MSG_HANDLE_STATE_INIT);

    wmb();
    bd_desc_d->status = bd_desc->status;
#else
    (void)devdrv_non_trans_msg_dma_copy_cq(msg_chan, bd_desc);
#endif
    return;
}

/* the large lock control of the sending function at the device side only
   has one command executed at the same time, no lock is needed here */
STATIC void devdrv_non_trans_rx_msg_task(struct work_struct *p_work)
{
    struct devdrv_msg_chan *msg_chan = container_of(p_work, struct devdrv_msg_chan, rx_work);
    enum devdrv_msg_client_type msg_type_tmp = msg_chan->msg_type;
    int dev_id;

    mutex_lock(&msg_chan->rx_mutex);

    devdrv_non_trans_rx_msg_task_resq_record(msg_chan);

    if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
        devdrv_info("Device is abnormal. (msg_type=\"%s\")\n",
                    devdrv_msg_type_str(msg_type_tmp, DEVDRV_COMMON_MSG_TYPE_MAX));
        mutex_unlock(&msg_chan->rx_mutex);
        atomic_set(&msg_chan->sched_status.state, DEVDRV_MSG_HANDLE_STATE_INIT);
        return;
    }

    dev_id = (int)msg_chan->msg_dev->pci_ctrl->dev_id;

    if (msg_chan->status == DEVDRV_DISABLE) {
        devdrv_err("msg_chan is disable. (dev_id=%d; msg_type=\"%s\"; msg_chan=%d)\n",
                   dev_id, devdrv_msg_type_str(msg_type_tmp, DEVDRV_COMMON_MSG_TYPE_MAX), msg_chan->chan_id);
        mutex_unlock(&msg_chan->rx_mutex);
        atomic_set(&msg_chan->sched_status.state, DEVDRV_MSG_HANDLE_STATE_INIT);
        return;
    }

    devdrv_non_trans_rx_msg_handle(msg_chan);
    mutex_unlock(&msg_chan->rx_mutex);

    return;
}

STATIC irqreturn_t devdrv_rx_msg_notify_handler(int irq, void *data)
{
    struct devdrv_msg_chan *msg_chan = (struct devdrv_msg_chan *)data;
    void *handle = NULL;

    if (msg_chan->status == DEVDRV_DISABLE) {
        devdrv_err_spinlock("msg_chan is disable. (dev_id=%u; chan_id=%u; msg_type=\"%s\"; queue_type=%u)\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, msg_chan->chan_id,
            devdrv_msg_type_str(msg_chan->msg_type, DEVDRV_COMMON_MSG_TYPE_MAX), (u32)msg_chan->queue_type);
        return IRQ_HANDLED;
    }

    if ((devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_ALIVE) &&
        (devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_SUSPEND)) {
        devdrv_err_spinlock("Device is not alive. (dev_id=%u; chan_id=%u; msg_type=\"%s\"; queue_type=%u)\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, msg_chan->chan_id,
            devdrv_msg_type_str(msg_chan->msg_type, DEVDRV_COMMON_MSG_TYPE_MAX), (u32)msg_chan->queue_type);
        return IRQ_HANDLED;
    }

    handle = devdrv_generate_msg_handle(msg_chan);

    rmb();

    if (msg_chan->rx_msg_notify != NULL) {
        msg_chan->rx_msg_notify(handle);
    }

    return IRQ_HANDLED;
}

STATIC irqreturn_t devdrv_tx_fnsh_notify_handler(int irq, void *data)
{
    struct devdrv_msg_chan *msg_chan = (struct devdrv_msg_chan *)data;
    void *handle = NULL;

    if (msg_chan->status == DEVDRV_DISABLE) {
        devdrv_err_spinlock("msg_chan is disable. (chan_id=%d)\n", msg_chan->chan_id);
        return IRQ_HANDLED;
    }

    if ((devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_ALIVE) &&
        (devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_SUSPEND)) {
        devdrv_err_spinlock("Device is not alive. (chan_id=%d)\n", msg_chan->chan_id);
        return IRQ_HANDLED;
    }

    handle = devdrv_generate_msg_handle(msg_chan);

    rmb();

    if (msg_chan->tx_finish_notify != NULL) {
        msg_chan->tx_finish_notify(handle);
    }

    return IRQ_HANDLED;
}

STATIC void devdrv_msg_chan_do_queue_work(struct devdrv_msg_chan *msg_chan)
{
    if (DEVDRV_MSG_HANDLE_STATE_INIT == atomic_cmpxchg(&msg_chan->sched_status.state,
        DEVDRV_MSG_HANDLE_STATE_INIT, DEVDRV_MSG_HANDLE_STATE_SCHEDING)) {
        msg_chan->stamp = (u32)jiffies;
        queue_work(msg_chan->msg_dev->work_queue[msg_chan->chan_id], &msg_chan->rx_work);
    }
}

STATIC irqreturn_t devdrv_wakeup_rx_work(int irq, void *data)
{
    struct devdrv_msg_chan *msg_chan = (struct devdrv_msg_chan *)data;
    struct devdrv_non_trans_msg_desc *bd_desc = NULL;

    if (msg_chan->status == DEVDRV_DISABLE) {
        return IRQ_HANDLED;
    }

    if ((devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_ALIVE) &&
        (devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_SUSPEND)) {
        return IRQ_HANDLED;
    }

    rmb();
    bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->cq_info.desc_h;
    if (bd_desc->status == DEVDRV_MSG_CMD_BEGIN) {
        devdrv_msg_chan_do_queue_work(msg_chan);
    } else {
        devdrv_guard_work_sched_immediate(msg_chan->msg_dev->pci_ctrl);
    }

    return IRQ_HANDLED;
}

STATIC bool devdrv_msg_chan_sched_check(struct devdrv_msg_chan *msg_chan)
{
    struct devdrv_msg_chan_sched_status *sched_status = &msg_chan->sched_status;

    if (sched_status->schedule_in_last != sched_status->schedule_in) {
        sched_status->schedule_in_last = sched_status->schedule_in;
        sched_status->no_schedule_cnt = 0;
        return true;
    }

    sched_status->no_schedule_cnt++;

    if (sched_status->no_schedule_cnt <= DEVDRV_MSG_SCHED_STATUS_CHECK_TIME) {
        return true;
    }

    sched_status->no_schedule_cnt = 0;

    return false;
}

void devdrv_msg_chan_guard_work_sched(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_non_trans_msg_desc *bd_desc = NULL;
    struct devdrv_msg_chan *msg_chan = NULL;
    int i;

    for (i = 0; i < pci_ctrl->msg_dev->chan_cnt; i++) {
        msg_chan = &(pci_ctrl->msg_dev->msg_chan[i]);

        if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
            continue;
        }

        if (msg_chan->status == DEVDRV_DISABLE) {
            continue;
        }

        if ((devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_ALIVE) &&
            (devdrv_get_device_status(msg_chan) != DEVDRV_DEVICE_SUSPEND)) {
            continue;
        }

        /* non trans msg chan */
        if (msg_chan->rx_work_flag == 1) {
            if (devdrv_msg_chan_sched_check(msg_chan) == true) {
                continue;
            }
            /* the host side cq buf is used as a buf to receive the command */
            bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->cq_info.desc_h;
            if (bd_desc->status == DEVDRV_MSG_CMD_BEGIN) {
                devdrv_msg_chan_do_queue_work(msg_chan);
            }
        }
    }
}

STATIC int devdrv_sync_non_trans_status_handle(struct devdrv_msg_chan *msg_chan, int status,
    enum devdrv_common_msg_type msg_type)
{
    struct devdrv_msg_chan_stat *chan_stat = &(msg_chan->chan_stat);
    int ret;

    if (status == DEVDRV_MSG_CMD_FINISH_SUCCESS) {
        ret = 0;
    } else if (status == DEVDRV_MSG_CMD_BEGIN) {
        ret = -ENOSYS;
        chan_stat->tx_timeout_err++;
        devdrv_err("Message send finish, no resp. (dev_id=%u; msg_type=\"%s\"; status=%d; ret=%d)\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_chan->msg_type, msg_type), status, ret);
    } else if (status == DEVDRV_MSG_CMD_FINISH_FAILED) {
        ret = -ETIMEDOUT;
        chan_stat->tx_process_err++;
        devdrv_err("Message send finish, process failed. (dev_id=%u; msg_type=\"%s\"; status=%d; ret=%d)\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_chan->msg_type, msg_type), status, ret);
    } else if (status == DEVDRV_MSG_CMD_NULL_PROCESS_CB) {
        ret = -EUNATCH;
        chan_stat->tx_no_callback++;
        devdrv_warn("Message send finish, no process cb. (dev_id=%u; msg_type=\"%s\"; status=%d; ret=%d)\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_chan->msg_type, msg_type), status, ret);
    } else {
        ret = -ETIMEDOUT;
        chan_stat->tx_invalid_para_err++;
        devdrv_warn("Message send finish, invalid para. (dev_id=%u; msg_type=\"%s\"; status=%d; ret=%d)\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_chan->msg_type, msg_type), status, ret);
    }

    return ret;
}

STATIC int devdrv_sync_non_trans_msg_chan_send(struct devdrv_msg_chan *msg_chan, enum devdrv_common_msg_type msg_type,
    struct devdrv_non_trans_msg_send_data_para *data_para)
{
    struct devdrv_non_trans_msg_desc *bd_desc = NULL;
    int timeout = DEVDRV_MSG_IRQ_TIMEOUT;
    u32 status = 0;
    int retry_cnt = 0;
    int ret;
    struct devdrv_msg_chan_stat *chan_stat = &(msg_chan->chan_stat);
    u64 seq_num = chan_stat->tx_total_cnt;
    enum devdrv_msg_client_type msg_type_tmp = msg_chan->msg_type;
    u32 *real_out_len = data_para->real_out_len;
    u32 msg_timeout = ((msg_type_tmp == devdrv_msg_client_devmm) &&
        (msg_chan->msg_dev->pci_ctrl->addr_mode == DEVDRV_ADMODE_FULL_MATCH)) ?
        DEVDRV_MAX_MSG_TIMEOUT : DEVDRV_MSG_TIMEOUT;

msg_retry:
    if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
        chan_stat->tx_status_abnormal_err++;
        devdrv_info("Device is abnormal. (msg_type=\"%s\")\n", devdrv_msg_type_str(msg_type_tmp, msg_type));
        return -ENODEV;
    }

    /* result is in the sq buf(desc_h) on the host side */
    bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->sq_info.desc_h;
    bd_desc->status = DEVDRV_MSG_CMD_BEGIN;

    wmb();
    /* inform device */
    devdrv_msg_ring_doorbell_inner((void *)msg_chan);

#ifndef CFG_FEATURE_SEC_COMM_L3
    /* wait for the device irq set status, when device dead, timeout quickly */
    bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->sq_info.desc_d;
    timeout = DEVDRV_MSG_IRQ_TIMEOUT;
    while (timeout > 0) {
        if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
            chan_stat->tx_status_abnormal_err++;
            devdrv_info("Device is abnormal. (msg_type=\"%s\")\n", devdrv_msg_type_str(msg_type_tmp, msg_type));
            return -ENODEV;
        }

        status = bd_desc->status;
        if (status == DEVDRV_MSG_CMD_IRQ_BEGIN) {
            break;
        }
        if (status == DEVDRV_MSG_CMD_IRQ_FINISH) {
            *real_out_len = 0;
            chan_stat->tx_success_cnt++;
            return 0;
        }
        rmb();
        usleep_range(DEVDRV_MSG_WAIT_MIN_TIME, DEVDRV_MSG_WAIT_MAX_TIME);
        timeout -= DEVDRV_MSG_WAIT_MIN_TIME;
    }

    if (status == DEVDRV_MSG_CMD_IRQ_BUSY) {
        if (retry_cnt < DEVDRV_MSG_RETRY_LIMIT) {
            retry_cnt++;
            goto msg_retry;
        }
        timeout = 0;
    }

    if (timeout <= 0) {
        chan_stat->tx_irq_timeout_err++;
        devdrv_err("Device irq not resp. (dev_id=%u; msg_type=\"%s\"; status=%d; retry_cnt=%d)\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_type_tmp, msg_type), status, retry_cnt);
        return -ENOSYS;
    }
#endif

    /* wait for the device to use the dma to move result to the sq buf on the host side */
    bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->sq_info.desc_h;
    timeout = msg_timeout - (DEVDRV_MSG_IRQ_TIMEOUT - timeout);
    while (timeout > 0) {
        if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
            chan_stat->tx_status_abnormal_err++;
            devdrv_info("Device is abnormal. (msg_type=\"%s\")\n", devdrv_msg_type_str(msg_type_tmp, msg_type));
            return -ENODEV;
        }

        status = bd_desc->status;
        if ((status != DEVDRV_MSG_CMD_BEGIN) && (bd_desc->seq_num == seq_num)) {
            break;
        }
        rmb();
        usleep_range(DEVDRV_MSG_WAIT_MIN_TIME, DEVDRV_MSG_WAIT_MAX_TIME);
        timeout -= DEVDRV_MSG_WAIT_MIN_TIME;
    }
    mb();
    if ((status != DEVDRV_MSG_CMD_BEGIN) && (bd_desc->seq_num != seq_num)) {
        devdrv_warn("Parameter is invalid. (dev_id=%d; msg_type=\"%s\"; "
            "num=%lld; reply_num=%lld; status=%d; timeout=%d(us))\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_type_tmp, msg_type),
            seq_num, bd_desc->seq_num, status, timeout);
        if (retry_cnt < DEVDRV_MSG_RETRY_LIMIT) {
            retry_cnt++;
            goto msg_retry;
        }
    }
    ret = devdrv_sync_non_trans_status_handle(msg_chan, (int)status, msg_type);
    if (ret == 0) {
        *real_out_len = bd_desc->real_out_len;
        if (*real_out_len > data_para->out_data_len) {
            chan_stat->tx_reply_len_check_err++;
            devdrv_err("real_out_len is error. (dev_id=%d; msg_type=\"%s\"; real_out_len=%d; out_len=%d)\n",
                msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_type_tmp, msg_type),
                *real_out_len, data_para->out_data_len);
            return -EINVAL;
        }
        /* real_out_len could be zero */
        if (*real_out_len > 0) {
            ret = memcpy_s(data_para->data, data_para->out_data_len, (void *)bd_desc->data, *real_out_len);
            if (ret != 0) {
                devdrv_err("memcpy_s failed. (ret=%d)\n", ret);
                ret = -EINVAL;
            }
        }
        chan_stat->tx_success_cnt++;
    }
    return ret;
}

#ifndef CFG_FEATURE_SEC_COMM_L3
STATIC int devdrv_sync_non_trans_msg_copy_bd(struct devdrv_msg_chan *msg_chan,
    void *data, u32 in_data_len, u32 out_data_len, enum devdrv_common_msg_type msg_type)
{
    struct devdrv_non_trans_msg_desc *bd_desc = NULL;

    bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->sq_info.desc_d;
    if (bd_desc == NULL) {
        devdrv_warn("Msg bd_desc is null, may be reset flow, retry later. (dev_id=%d; msg_type=\"%s\")\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_chan->msg_type, msg_type));
        return -EINVAL;
    }
    bd_desc->in_data_len = in_data_len;
    bd_desc->out_data_len = out_data_len;
    bd_desc->real_out_len = 0;
    bd_desc->msg_type = (u32)msg_type;
    bd_desc->seq_num = msg_chan->chan_stat.tx_total_cnt;
    bd_desc->status = DEVDRV_MSG_CMD_BEGIN;

    memcpy_toio((void __iomem *)bd_desc->data, (void *)data, in_data_len);

    return 0;
}
#else
STATIC int devdrv_sync_non_trans_msg_dma_copy_sq(struct devdrv_msg_chan *msg_chan,
    void *data, u32 in_data_len, u32 out_data_len, enum devdrv_common_msg_type msg_type)
{
    enum devdrv_dma_data_type data_type = DEVDRV_DMA_DATA_PCIE_MSG;
    struct devdrv_non_trans_msg_desc *bd_desc = NULL;
    u32 total_len;
    int ret;

    bd_desc = (struct devdrv_non_trans_msg_desc *)msg_chan->sq_info.base_reserve_h;
    if (bd_desc == NULL) {
        devdrv_warn("Msg bd_desc is null, may be reset flow, retry later. (dev_id=%d; msg_type=\"%s\")\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_chan->msg_type, msg_type));
        return -EINVAL;
    }
    bd_desc->in_data_len = in_data_len;
    bd_desc->out_data_len = out_data_len;
    bd_desc->real_out_len = 0;
    bd_desc->msg_type = (u32)msg_type;
    bd_desc->seq_num = msg_chan->chan_stat.tx_total_cnt;
    bd_desc->status = DEVDRV_MSG_CMD_BEGIN;

    ret = memcpy_s((void *)bd_desc->data, msg_chan->sq_info.desc_size - DEVDRV_NON_TRANS_MSG_HEAD_LEN,
                   data, in_data_len);
    if (ret != 0) {
        devdrv_err("memcpy fail. (ret=%d)\n", ret);
        return ret;
    }

    total_len = (u32)sizeof(struct devdrv_non_trans_msg_desc) + bd_desc->in_data_len;
    ret = devdrv_dma_sync_copy(msg_chan->msg_dev->pci_ctrl->dev_id, data_type, msg_chan->sq_info.dma_reserve_h,
                               msg_chan->sq_info.dma_reserve_d, total_len, DEVDRV_DMA_HOST_TO_DEVICE);
    if (ret != 0) {
        devdrv_err("dma copy fail. (len=%u, ret=%d)\n", total_len, ret);
    }
    return ret;
}
#endif
int devdrv_sync_non_trans_msg_send(struct devdrv_msg_chan *msg_chan, void *data, u32 in_data_len, u32 out_data_len,
                                   u32 *real_out_len, enum devdrv_common_msg_type msg_type)
{
    struct devdrv_non_trans_msg_send_data_para data_para;
    int ret;
    struct devdrv_msg_chan_stat *chan_stat = NULL;
    enum devdrv_msg_client_type msg_type_tmp = msg_chan->msg_type;
    u32 max_data_len = msg_chan->sq_info.desc_size - (u32)DEVDRV_NON_TRANS_MSG_HEAD_LEN;

    devdrv_device_mutex_lock(msg_chan);
    if (devdrv_get_pcie_channel_status() == DEVDRV_PCIE_COMMON_CHANNEL_LINKDOWN) {
        devdrv_device_mutex_unlock(msg_chan);
        return -ENODEV;
    }
    if (msg_chan->status == DEVDRV_DISABLE) {
        devdrv_err("msg chan is invalid. (chan_id=%u)\n", msg_chan->chan_id);
        devdrv_device_mutex_unlock(msg_chan);
        return -EINVAL;
    }

    chan_stat = &(msg_chan->chan_stat);

    chan_stat->tx_total_cnt++;
    if ((in_data_len > max_data_len) || (out_data_len > max_data_len)) {
        chan_stat->tx_len_check_err++;
        devdrv_device_mutex_unlock(msg_chan);
        devdrv_err("Input parameter is invalid. (dev_id=%d; msg_type=\"%s\"; in_data_len=%d; "
                   "out_data_len=%d; desc_len=%d)\n",
                   msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_type_tmp, msg_type),
                   in_data_len, out_data_len, max_data_len);
        return -EINVAL;
    }

    devdrv_debug("Get data length. (msg_type=\"%s\"; in_data_len=%d; out_data_len=%d\n",
                 devdrv_msg_type_str(msg_type_tmp, msg_type), in_data_len, out_data_len);

    if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
        chan_stat->tx_status_abnormal_err++;
        devdrv_device_mutex_unlock(msg_chan);
        devdrv_info("Device is abnormal. (dev_id=%u; msg_type=\"%s\")\n",
                    msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_type_tmp, msg_type));
        return -ENODEV;
    }
    data_para.data = data;
    data_para.in_data_len = in_data_len;
    data_para.out_data_len = out_data_len;
    data_para.real_out_len = real_out_len;

#ifdef CFG_FEATURE_SEC_COMM_L3
    /* dma copy messages to device shared reserved memory pointed to by sq dma_reserve_d */
    ret = devdrv_sync_non_trans_msg_dma_copy_sq(msg_chan, data, in_data_len, out_data_len, msg_type);
#else
    /* put messages to send to shared reserved memory pointed to by sq desc_d */
    ret = devdrv_sync_non_trans_msg_copy_bd(msg_chan, data, in_data_len, out_data_len, msg_type);
#endif
    if (ret != 0) {
        devdrv_device_mutex_unlock(msg_chan);
        devdrv_err("Msg sq copy failed. (dev_id=%u; msg_type=\"%s\")\n",
            msg_chan->msg_dev->pci_ctrl->dev_id, devdrv_msg_type_str(msg_type_tmp, msg_type));
        return -EINVAL;
    }

    ret = devdrv_sync_non_trans_msg_chan_send(msg_chan, msg_type, &data_para);

    devdrv_device_mutex_unlock(msg_chan);

    return ret;
}

int devdrv_sync_msg_send(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    int ret;

    struct devdrv_msg_chan *chan = devdrv_get_msg_chan(msg_chan);
    if (chan == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("msg_chan is null.\n");
        } else {
            devdrv_err_limit("msg_chan is null.\n");
        }
        return -EINVAL;
    }
    if ((chan->msg_dev == NULL) || (chan->msg_dev->dev == NULL)) {
        devdrv_err("chan->msg_dev or chan->msg_dev->dev is null.\n");
        ret = -EINVAL;
        goto exit;
    }
    if (data == NULL) {
        devdrv_err("data is null. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
        ret = -EINVAL;
        goto exit;
    }
    if (real_out_len == NULL) {
        devdrv_err("real_out_len is null. (dev_id=%u)\n", chan->msg_dev->pci_ctrl->dev_id);
        ret = -EINVAL;
        goto exit;
    }

    ret = devdrv_sync_non_trans_msg_send(chan, data, in_data_len, out_data_len, real_out_len,
                                         DEVDRV_COMMON_MSG_TYPE_MAX);
    if (ret == -ENODEV) {
        devdrv_info("Device is abnormal. (dev_id=%u; chan_id=%d; in_data_len=%d; ret=%d)\n",
                    chan->msg_dev->pci_ctrl->dev_id, chan->chan_id, in_data_len, ret);
    } else if (ret != 0) {
        devdrv_err("Send failed. (dev_id=%u; chan_id=%d; in_data_len=%d; ret=%d)\n",
                   chan->msg_dev->pci_ctrl->dev_id, chan->chan_id, in_data_len, ret);
    }

exit:
    devdrv_put_msg_chan(chan);
    return ret;
}
EXPORT_SYMBOL(devdrv_sync_msg_send);

#ifdef CFG_FEATURE_SEC_COMM_L3
STATIC bool devdrv_is_need_secure_comm(const void *cmd, enum devdrv_admin_msg_opcode opcode)
{
    const struct devdrv_dma_chan_remote_op *cmd_data = (struct devdrv_dma_chan_remote_op *)cmd;
    return (opcode == DEVDRV_DMA_CHAN_REMOTE_OP) && (cmd_data->op == DMA_CHAN_REMOTE_OP_ERR_PROC);
}
#endif
STATIC void devdrv_admin_record_wait_time(int dev_id, u32 opcode, u32 time_use, u32 time_log)
{
    if (time_use > time_log) {
        devdrv_info("Record wait time. (dev_id=%d; opcode=%u; time_use=%dus; time_log=%dus)\n",
                    dev_id, opcode, time_use, time_log);
    }
}

STATIC int devdrv_admin_wait_recv_resp(struct devdrv_msg_chan *msg_chan, int *timeout, int dev_id)
{
    struct devdrv_admin_msg_command *msg_head = (struct devdrv_admin_msg_command *)msg_chan->sq_info.desc_h;
    u32 status = 0;

    while (*timeout > 0) {
        status = msg_chan->msg_dev->pci_ctrl->shr_para->admin_msg_status;
        if (status == DEVDRV_MSG_CMD_IRQ_BEGIN) {
            break;
        }

        rmb();
        usleep_range(DEVDRV_ADMIN_MSG_WAIT_MIN_TIME, DEVDRV_ADMIN_MSG_WAIT_MAX_TIME);

        if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
            devdrv_info("Device is abnormal. (dev_id=%u; msg_type=%d)\n", dev_id, (int)msg_chan->msg_type);
            return -ENODEV;
        }

        (*timeout) -= DEVDRV_ADMIN_MSG_WAIT_MIN_TIME;
    }

    devdrv_admin_record_wait_time(dev_id, msg_head->opcode, (u32)(DEVDRV_ADMIN_MSG_IRQ_TIMEOUT - *timeout),
                                  DEVDRV_MSG_IRQ_TIMEOUT_LOG);

    if (*timeout <= 0) {
#ifdef CFG_FEATURE_SEC_COMM_L3
        if (!devdrv_is_need_secure_comm((const void *)msg_head->data, msg_head->opcode)) {
            msg_chan->msg_dev->pci_ctrl->shr_para->admin_msg_status = DEVDRV_MSG_CMD_FINISH_FAILED;
        }
#else
        msg_chan->msg_dev->pci_ctrl->shr_para->admin_msg_status = DEVDRV_MSG_CMD_FINISH_FAILED;
#endif
        devdrv_err("Device not resp. (dev_id=%d; opcode=%u; status=%u)\n", dev_id, msg_head->opcode, status);
        return -ENOSYS;
    }

    return 0;
}

STATIC int devdrv_admin_send_wait_resq(struct devdrv_msg_chan *msg_chan, int *time)
{
    struct devdrv_admin_msg_command *msg_head = (struct devdrv_admin_msg_command *)msg_chan->sq_info.desc_h;
    int dev_id = (int)msg_chan->msg_dev->pci_ctrl->dev_id;
    u32 status = 0;
    int timeout;
    int ret = 0;

    /* wait for the device irq set status, when device dead, timeout quickly */
    timeout = DEVDRV_ADMIN_MSG_IRQ_TIMEOUT;
#ifdef CFG_FEATURE_SEC_COMM_L3
    if (!devdrv_is_need_secure_comm((const void *)msg_head->data, msg_head->opcode)) {
        ret = devdrv_admin_wait_recv_resp(msg_chan, &timeout, dev_id);
    }
#else
    ret = devdrv_admin_wait_recv_resp(msg_chan, &timeout, dev_id);
#endif
    if (ret != 0) {
        return ret;
    }

    timeout = DEVDRV_ADMIN_MSG_TIMEOUT - (DEVDRV_ADMIN_MSG_IRQ_TIMEOUT - timeout);
    while (timeout > 0) {
        if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
            devdrv_info("Device is abnormal. (dev_id=%u; msg_type=%d)\n", dev_id, (int)msg_chan->msg_type);
            return -ENODEV;
        }

        status = msg_head->status;
        if (status != DEVDRV_MSG_CMD_BEGIN) {
            break;
        }

        rmb();
        usleep_range(DEVDRV_ADMIN_MSG_WAIT_MIN_TIME, DEVDRV_ADMIN_MSG_WAIT_MAX_TIME);
        timeout -= DEVDRV_ADMIN_MSG_WAIT_MIN_TIME;
    }

    mb();
    devdrv_admin_record_wait_time(dev_id, msg_head->opcode,
        (u32)(DEVDRV_ADMIN_MSG_TIMEOUT - timeout), DEVDRV_MSG_TIMEOUT_LOG);

    if ((timeout <= 0) && (status == DEVDRV_MSG_CMD_BEGIN)) {
        msg_head->status = DEVDRV_MSG_CMD_FINISH_FAILED;
    }

    if (status != DEVDRV_MSG_CMD_FINISH_SUCCESS) {
        devdrv_err("Status is error. (dev_id=%d; opcode=%u; wait_time=%dus; status=%u)\n",
                   dev_id, msg_head->opcode, DEVDRV_ADMIN_MSG_TIMEOUT - timeout, status);
        return -ENOSYS;
    }

    *time = timeout;

    return 0;
}


/* admin msg chan submit a command and wait reply */
int devdrv_admin_msg_chan_send(struct devdrv_msg_dev *msg_dev, enum devdrv_admin_msg_opcode opcode, const void *cmd,
                               size_t size, void *reply, size_t reply_size)
{
    struct devdrv_msg_chan *msg_chan = msg_dev->admin_msg_chan;
    struct devdrv_admin_msg_command *msg_head = NULL;
    struct devdrv_admin_msg_reply *msg_reply = NULL;
    int ret;
    int timeout;
    u32 dev_id = msg_dev->pci_ctrl->dev_id;
    enum devdrv_msg_client_type msg_type_tmp = msg_chan->msg_type;

    if (size > DEVDRV_ADMIN_MSG_DATA_LEN) {
        devdrv_err("Input parameter is invalid. (dev_id=%u; size=%lu)\n", dev_id, size);
        return -EINVAL;
    }

    if ((msg_dev->pci_ctrl->device_status == DEVDRV_DEVICE_DEAD) ||
        (msg_dev->pci_ctrl->device_status == DEVDRV_DEVICE_UDA_RM)) {
        if (devdrv_get_product() == HOST_PRODUCT_MDC) {
            return 0; // MDC suspend scene doesn't need log
        }
        devdrv_info("Device is reset, opcode needn't send. (dev_id=%u; opcode=%d)\n", dev_id, (int)opcode);
        return 0;
    }
    devdrv_debug("Opcode start. (dev_id=%u; opcode=%d)\n", dev_id, opcode);

    devdrv_device_mutex_lock(msg_chan);

    if (devdrv_device_status_abnormal_check_inner(msg_chan) != 0) {
        devdrv_device_mutex_unlock(msg_chan);
        devdrv_info("Device is abnormal. (msg_type=%d)\n", (int)msg_type_tmp);
        return -ENODEV;
    }

    msg_head = (struct devdrv_admin_msg_command *)msg_chan->sq_info.desc_h;
    msg_head->opcode = (u32)opcode;
    msg_head->status = DEVDRV_MSG_CMD_BEGIN;

    if ((cmd != NULL) && (memcpy_s(msg_head->data, DEVDRV_ADMIN_MSG_DATA_LEN, cmd, size) != 0)) {
        devdrv_device_mutex_unlock(msg_chan);
        devdrv_err("memcpy failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    wmb();

#ifdef CFG_FEATURE_SEC_COMM_L3
    if (!devdrv_is_need_secure_comm(cmd, opcode)) {
        msg_dev->pci_ctrl->shr_para->admin_msg_status = DEVDRV_MSG_CMD_BEGIN;
    }
#else
    msg_dev->pci_ctrl->shr_para->admin_msg_status = DEVDRV_MSG_CMD_BEGIN;
#endif
    wmb();

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
    msleep(500);
#endif
    devdrv_msg_ring_doorbell_inner((void *)msg_chan);

    ret = devdrv_admin_send_wait_resq(msg_chan, &timeout);
    if (ret != 0) {
        devdrv_device_mutex_unlock(msg_chan);
        return ret;
    }

    mb();
    ret = -1;
    if (reply == NULL) { /* not need response message */
        ret = 0;
    } else {
        msg_reply = (struct devdrv_admin_msg_reply *)msg_head->data;
        if (msg_reply->len - sizeof(msg_reply->len) > reply_size) {
            devdrv_info("msg_reply_len is invalid. (dev_id=%u; opcode=%d; msg_reply_len=%u; buf_size=%lu)\n",
                        dev_id, (int)opcode, msg_reply->len, reply_size);
        } else if (memcpy_s(reply, reply_size, msg_reply->data,
                            msg_reply->len - sizeof(msg_reply->len)) == 0) {
            ret = 0;
        }
    }

    devdrv_device_mutex_unlock(msg_chan);

    devdrv_debug("Message send finish. (dev_id=%u; opcode=%d; wait_time=%dus; status=%d)\n",
                 dev_id, opcode, DEVDRV_ADMIN_MSG_TIMEOUT - timeout, msg_head->status);

    return ret;
}

void devdrv_free_msg_queue_res(struct devdrv_msg_chan *msg_chan)
{
    msg_chan->status = DEVDRV_DISABLE;
    if (msg_chan->cq_info.irq_vector >= 0) {
        (void)devdrv_unregister_irq_by_vector_index(msg_chan->msg_dev->pci_ctrl->dev_id,
            msg_chan->cq_info.irq_vector, msg_chan);
        msg_chan->cq_info.irq_vector = -1;
    }

    if (msg_chan->sq_info.irq_vector >= 0) {
        (void)devdrv_unregister_irq_by_vector_index(msg_chan->msg_dev->pci_ctrl->dev_id,
            msg_chan->sq_info.irq_vector, msg_chan);
        msg_chan->sq_info.irq_vector = -1;
    }

    if (msg_chan->rx_work_flag != 0) {
        cancel_work_sync(&msg_chan->rx_work);
        msg_chan->rx_work_flag = 0;
    }

    (void)devdrv_msg_free_cq(msg_chan);
    (void)devdrv_msg_free_sq(msg_chan);
}

STATIC void devdrv_alloc_msg_queue_register_irq(void *priv,
    const struct devdrv_msg_chan_info *chan_info,
    struct devdrv_msg_chan *msg_chan,
    struct devdrv_create_queue_command *cmd_data)
{
    int ret;
    u32 dev_id = msg_chan->msg_dev->pci_ctrl->dev_id;

    if (chan_info->queue_type == TRANSPARENT_MSG_QUEUE) {
        if (chan_info->rx_msg_notify != NULL) {
            ret = devdrv_register_irq_by_vector_index(dev_id, msg_chan->irq_rx_msg_notify,
                devdrv_rx_msg_notify_handler, msg_chan, "trans msg_chan_rx_msg_notify");
            if (ret != 0) {
                devdrv_err("PCIe trans msg_chan_rx_msg_notify register failed.(ret=%d, devid=%u, irq_index=%d)\n",
                    ret, dev_id, msg_chan->irq_rx_msg_notify);
                cmd_data->irq_rx_msg_notify = -1;
            } else {
                cmd_data->irq_rx_msg_notify = msg_chan->irq_rx_msg_notify;
            }
        } else {
            cmd_data->irq_rx_msg_notify = -1;
        }

        if (chan_info->tx_finish_notify != NULL) {
            ret = devdrv_register_irq_by_vector_index(dev_id, msg_chan->irq_tx_finish_notity,
                devdrv_tx_fnsh_notify_handler, msg_chan, "trans msg_chan_tx_finish_notify");
            if (ret != 0) {
                devdrv_err("PCIe trans msg_chan_tx_finish_notify register failed.(ret=%d, devid=%u, irq_index=%d)\n",
                    ret, dev_id, msg_chan->irq_tx_finish_notity);
                cmd_data->irq_tx_finish_notify = -1;
            } else {
                cmd_data->irq_tx_finish_notify = msg_chan->irq_tx_finish_notity;
            }
        } else {
            cmd_data->irq_tx_finish_notify = -1;
        }
        msg_chan->rx_work_flag = 0;
    } else {
        ret = devdrv_register_irq_by_vector_index(dev_id, msg_chan->irq_rx_msg_notify, devdrv_wakeup_rx_work,
            msg_chan, "non-trans msg_chan_rx_msg_notify");
        if (ret != 0) {
            devdrv_err("PCIe non-trans msg_chan_rx_msg_notify register failed.(ret=%d, devid=%u, irq_index=%d)\n",
                ret, dev_id, msg_chan->irq_rx_msg_notify);
            cmd_data->irq_rx_msg_notify = -1;
            cmd_data->irq_tx_finish_notify = -1;
        } else {
            cmd_data->irq_rx_msg_notify = msg_chan->irq_rx_msg_notify;
            cmd_data->irq_tx_finish_notify = -1;
        }
        INIT_WORK(&msg_chan->rx_work, devdrv_non_trans_rx_msg_task);
        msg_chan->rx_work_flag = 1;
    }

    msg_chan->cq_info.irq_vector = cmd_data->irq_rx_msg_notify;
    msg_chan->sq_info.irq_vector = cmd_data->irq_tx_finish_notify;
}

STATIC struct devdrv_msg_chan *devdrv_alloc_msg_queue(void *priv, struct devdrv_msg_chan_info *chan_info)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)priv;
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;
    struct devdrv_msg_chan *msg_chan = NULL;
    struct devdrv_create_queue_command cmd_data;
    struct devdrv_alloc_msg_chan_reply reply;
    int ret;

    /* alloc msg_chan */
    msg_chan = devdrv_alloc_msg_chan(msg_dev, chan_info->level);
    if (msg_chan == NULL) {
        devdrv_err("Alloc msg_chan failed.\n");
        return NULL;
    }

    cmd_data.msg_type = (u32)chan_info->msg_type;
    cmd_data.queue_type = (u32)chan_info->queue_type;
    cmd_data.queue_id = msg_chan->chan_id;
    cmd_data.sq_dma_base_host = 0;
    cmd_data.cq_dma_base_host = 0;
    cmd_data.sq_desc_size = 0;
    cmd_data.cq_desc_size = 0;
    cmd_data.sq_depth = 0;
    cmd_data.cq_depth = 0;
    cmd_data.sq_slave_mem_offset = 0;
    cmd_data.cq_slave_mem_offset = 0;
    /* alloc sq queue */
    if ((chan_info->sq_desc_size != 0) && (chan_info->queue_depth != 0)) {
        ret = devdrv_msg_alloc_s_queue(msg_chan, chan_info->queue_depth, chan_info->sq_desc_size);
        if (ret != 0) {
            devdrv_err("Alloc s_queue failed. (dev_id=%d; ret=%d)\n", msg_dev->pci_ctrl->dev_id, ret);
            (void)devdrv_msg_free_host_sq(msg_chan);
            msg_chan->status = DEVDRV_DISABLE;
            return NULL;
        }

        cmd_data.sq_desc_size = chan_info->sq_desc_size;
        cmd_data.sq_depth = (u16)chan_info->queue_depth;
        cmd_data.sq_dma_base_host = msg_chan->sq_info.dma_handle;
        cmd_data.sq_slave_mem_offset = msg_chan->sq_info.slave_mem_offset;
    }

    /* alloc cq queue */
    if ((chan_info->cq_desc_size != 0) && (chan_info->queue_depth != 0)) {
        ret = devdrv_msg_alloc_c_queue(msg_chan, chan_info->queue_depth, chan_info->cq_desc_size);
        if (ret != 0) {
            devdrv_err("Alloc c_queue failed. (dev_id=%d; ret=%d)\n", msg_dev->pci_ctrl->dev_id, ret);
            devdrv_free_msg_queue_res(msg_chan);
            return NULL;
        }

        cmd_data.cq_desc_size = chan_info->cq_desc_size;
        cmd_data.cq_depth = (u16)chan_info->queue_depth;
        cmd_data.cq_dma_base_host = msg_chan->cq_info.dma_handle;
        cmd_data.cq_slave_mem_offset = msg_chan->cq_info.slave_mem_offset;
    }

    /* request irqs */
    devdrv_alloc_msg_queue_register_irq(priv, chan_info, msg_chan, &cmd_data);

    msg_chan->msg_type = chan_info->msg_type;
    msg_chan->flag = chan_info->flag;
    msg_chan->rx_msg_process = chan_info->rx_msg_process;
    msg_chan->rx_msg_notify = chan_info->rx_msg_notify;
    msg_chan->tx_finish_notify = chan_info->tx_finish_notify;
    msg_chan->queue_type = chan_info->queue_type;

    devdrv_debug("Alloc msg_chan. (dev_id=%d;  msg_type=%d; queue_id=%d)\n",
                 msg_dev->pci_ctrl->dev_id, cmd_data.msg_type, cmd_data.queue_id);

    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_CREATE_MSG_QUEUE,
                                     &cmd_data, sizeof(cmd_data), &reply, sizeof(reply));
    if (ret != 0) {
        devdrv_err("Message send failed. (dev_id=%d; msg_type=%d; queue_type=%d, queue_id=%d; "
                   "sq_size=%d; cq_size=%d; sq_depth=%d; cq_depth=%d; tx_irq=%d; rx_irq=%d; ret=%d)\n",
                   msg_dev->pci_ctrl->dev_id, cmd_data.msg_type, cmd_data.queue_type, cmd_data.queue_id,
                   cmd_data.sq_desc_size, cmd_data.cq_desc_size, cmd_data.sq_depth, cmd_data.cq_depth,
                   cmd_data.irq_tx_finish_notify, cmd_data.irq_rx_msg_notify, ret);

        devdrv_free_msg_queue_res(msg_chan);
        return NULL;
    }
#ifdef CFG_FEATURE_SEC_COMM_L3
    devdrv_msg_save_slave_sqcq_dma(msg_chan, reply.sq_rsv_dma_addr_d, reply.cq_rsv_dma_addr_d);
#endif
    return msg_chan;
}

STATIC int devdrv_free_msg_queue(struct devdrv_msg_chan *msg_chan, enum msg_queue_type queue_type)
{
    struct devdrv_free_queue_cmd cmd_data;
    struct devdrv_msg_dev *msg_dev = NULL;
    int ret;

    if (msg_chan == NULL) {
        devdrv_err("msg_chan is null.\n");
        return -EINVAL;
    }

    msg_dev = msg_chan->msg_dev;

    /* fill the queue free cmd */
    cmd_data.queue_id = msg_chan->chan_id;

    /* send the cmd */
    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_FREE_MSG_QUEUE, &cmd_data, sizeof(cmd_data), NULL, 0);
    if (ret != 0) {
        devdrv_err("Message send failed. (dev_id=%u; cmd=%d; ret=%d)\n",
                   msg_dev->pci_ctrl->dev_id, DEVDRV_FREE_MSG_QUEUE, ret);
    }

    devdrv_free_msg_queue_res(msg_chan);

    return ret;
}

struct devdrv_msg_chan *devdrv_alloc_trans_queue(void *priv, struct devdrv_trans_msg_chan_info *chan_info)
{
    struct devdrv_msg_chan_info msg_chan_info;
    struct devdrv_msg_chan *msg_chan = NULL;

    if (chan_info->msg_type >= devdrv_msg_client_max) {
        devdrv_err("msg_type is error. (msg_type=%d)\n", (int)chan_info->msg_type);
        return NULL;
    }

    if (memset_s((void *)&msg_chan_info, sizeof(struct devdrv_msg_chan_info), 0, sizeof(struct devdrv_msg_chan_info)) !=
        0) {
        devdrv_err("memset_s failed.\n");
        return NULL;
    }

    msg_chan_info.msg_type = chan_info->msg_type;
    msg_chan_info.queue_depth = chan_info->queue_depth;
    msg_chan_info.sq_desc_size = chan_info->sq_desc_size;
    msg_chan_info.cq_desc_size = chan_info->cq_desc_size;
    msg_chan_info.rx_msg_notify = chan_info->rx_msg_notify;
    msg_chan_info.tx_finish_notify = chan_info->tx_finish_notify;
    msg_chan_info.queue_type = TRANSPARENT_MSG_QUEUE;
    msg_chan_info.level = chan_info->level;

    msg_chan = devdrv_alloc_msg_queue(priv, &msg_chan_info);
    if (msg_chan == NULL) {
        devdrv_err("Alloc msg_queue failed.\n");
        return NULL;
    }

    return msg_chan;
}

int devdrv_free_trans_queue(struct devdrv_msg_chan *msg_chan)
{
    return devdrv_free_msg_queue(msg_chan, TRANSPARENT_MSG_QUEUE);
}

struct devdrv_msg_chan *devdrv_alloc_non_trans_queue(void *priv, struct devdrv_non_trans_msg_chan_info *chan_info)
{
    struct devdrv_msg_chan_info msg_chan_info;
    struct devdrv_msg_chan *msg_chan = NULL;

    if (chan_info->msg_type >= devdrv_msg_client_max) {
        devdrv_err("msg_type is error. (msg_type=%d)\n", (int)chan_info->msg_type);
        return NULL;
    }

    if (chan_info->rx_msg_process == NULL) {
        devdrv_err("rx_msg_process must set.\n");
        return NULL;
    }

    msg_chan_info.msg_type = chan_info->msg_type;
    msg_chan_info.flag = chan_info->flag;

    /* synchronization message mechanism, using a block of memory */
    msg_chan_info.sq_desc_size = chan_info->s_desc_size;
    msg_chan_info.cq_desc_size = chan_info->c_desc_size;
    msg_chan_info.queue_depth = 1;

    msg_chan_info.rx_msg_process = chan_info->rx_msg_process;
    msg_chan_info.queue_type = NON_TRANSPARENT_MSG_QUEUE;
    msg_chan_info.level = chan_info->level;
    msg_chan_info.rx_msg_notify = NULL;
    msg_chan_info.tx_finish_notify = NULL;

    msg_chan = devdrv_alloc_msg_queue(priv, &msg_chan_info);
    if (msg_chan == NULL) {
        devdrv_err("Alloc msg_queue failed.\n");
        return NULL;
    }

    return msg_chan;
}

int devdrv_free_non_trans_queue(struct devdrv_msg_chan *msg_chan)
{
    return devdrv_free_msg_queue(msg_chan, NON_TRANSPARENT_MSG_QUEUE);
}

/*
 * p1 use msix after p0 in 1pf2p:
 * p0:0~255; p1:256~511
 * flag=1:add; 0:sub
 */
void devdrv_dma_update_msix_entry_offset(void *drvdata, u32 *irq, int flag)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)drvdata;

    if (flag == 1) {
        *irq += pci_ctrl->msix_offset;
    } else {
        *irq -= pci_ctrl->msix_offset;
    }
}

int devdrv_notify_dma_err_irq(void *drvdata, u32 dma_chan_id, int err_irq)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)drvdata;
    struct devdrv_notify_dma_err_irq_cmd cmd_data;
    int ret;

    /* fill the cmd */
    cmd_data.dma_chan_id = dma_chan_id;
    cmd_data.err_irq = err_irq;

    /* send the cmd */
    ret = devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_NOTIFY_DMA_ERR_IRQ, &cmd_data, sizeof(cmd_data), NULL,
                                     0);
    if (ret != 0) {
        devdrv_err("Message send failed. (dev_id=%u; cmd=%d; ret=%d)\n", dma_chan_id, DEVDRV_NOTIFY_DMA_ERR_IRQ,
                   ret);
    }

    return ret;
}

int devdrv_get_general_interrupt_db_info(struct devdrv_msg_dev *msg_dev, u32 *db_start, u32 *db_num)
{
    struct devdrv_general_interrupt_db_info reply_data = {0};
    int ret;

    /* send the cmd */
    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_GET_GELNERAL_INTERRUPT_DB_INFO, NULL, 0, &reply_data,
                                     sizeof(reply_data));
    if (ret != 0) {
        devdrv_err("Message send failed. (cmd=%d; ret=%d)\n", DEVDRV_GET_GELNERAL_INTERRUPT_DB_INFO, ret);
    } else {
        *db_start = reply_data.db_start;
        *db_num = reply_data.db_num;
    }

    return ret;
}

int devdrv_get_rx_atu_info(struct devdrv_pci_ctrl *pci_ctrl, u32 bar_num)
{
    struct devdrv_get_rx_atu_cmd cmd_data;
    struct devdrv_iob_atu io_rsv_atu[DEVDRV_MAX_RX_ATU_NUM];
    struct devdrv_iob_atu *reply = NULL;
    int ret;

    cmd_data.devid = pci_ctrl->dev_id;
    cmd_data.bar_num = bar_num;

    if (bar_num == pci_ctrl->mem_bar_id) {
        reply = pci_ctrl->mem_rx_atu;
    } else {
        reply = io_rsv_atu;
    }

    ret = devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_GET_RX_ATU, &cmd_data, sizeof(cmd_data),
                                     reply, sizeof(struct devdrv_iob_atu) * DEVDRV_MAX_RX_ATU_NUM);
    if (ret != 0) {
        devdrv_err("Message send failed. (cmd=%d; ret=%d)\n", DEVDRV_GET_RX_ATU, ret);
        return -ENOMEM;
    }

    return 0;
}

int devdrv_notify_dev_online(struct devdrv_msg_dev *msg_dev, u32 devid, u32 status)
{
    struct devdrv_notify_dev_online_cmd cmd_data;
    int ret;

    /* fill the cmd */
    cmd_data.devid = devid;
    cmd_data.status = status;

    /* send the cmd */
    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_NOTIFY_DEV_ONLINE, &cmd_data, sizeof(cmd_data), NULL, 0);
    if (ret != 0) {
        devdrv_err("Notify online cmd failed. (dev_id=%u; dst_dev=%d; ret=%d)\n",
                   msg_dev->pci_ctrl->dev_id, devid, ret);
    }

    return ret;
}

int devdrv_get_support_msg_chan_cnt(u32 devid, enum devdrv_msg_client_type module_type)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;
    int chan_cnt = -1;

    if (((int)module_type < 0) || (module_type >= devdrv_msg_client_max)) {
        devdrv_err("invalid module type. (module_type=%d)\n", module_type);
        return -EINVAL;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if ((ctrl != NULL) && (ctrl->priv != NULL)) {
        pci_ctrl = ctrl->priv;
        chan_cnt = pci_ctrl->res.msg_chan_cnt[module_type] - DEVDRV_MSG_CHAN_NUM_FOR_NON_HDC;
        if ((devdrv_get_dev_chip_type(devid) == HISI_MINI_V2)
            && (pci_ctrl->msix_irq_num < pci_ctrl->res.intr.max_vector)) {
            chan_cnt = DEVDRV_DEV_HDC_LITE_MSG_CHAN_CNT_MAX - DEVDRV_MSG_CHAN_NUM_FOR_NON_HDC;
        }
    }

    return chan_cnt;
}
EXPORT_SYMBOL(devdrv_get_support_msg_chan_cnt);

STATIC void devdrv_set_admin_sq_base(struct devdrv_pci_ctrl *pci_ctrl, u64 sq_base)
{
    pci_ctrl->shr_para->admin_chan_sq_base = sq_base;
}

int devdrv_init_admin_msg_chan(struct devdrv_msg_dev *msg_dev)
{
    /* The first application in the initialization, the fixed application is channel 0 */
    struct devdrv_msg_chan *msg_chan = NULL;
    u64 sq_base;
    int ret = -1;

    msg_chan = devdrv_alloc_msg_chan(msg_dev, DEVDRV_MSG_CHAN_LEVEL_LOW);
    if (msg_chan == NULL) {
        devdrv_err("msg_chan alloc failed. (dev_id=%u)\n", msg_dev->pci_ctrl->dev_id);
        return ret;
    }
    /* admin queue communication only needs a sq buf  */
    ret = devdrv_msg_alloc_host_sq(msg_chan, DEVDRV_ADMIN_MSG_QUEUE_DEPTH, DEVDRV_ADMIN_MSG_QUEUE_BD_SIZE);
    if (ret != 0) {
        devdrv_err("Message queue alloc failed. (dev_id=%u; ret=%d)\n", msg_dev->pci_ctrl->dev_id, ret);
        return ret;
    }

    msg_chan->cq_info.irq_vector = -1;
    msg_chan->rx_work_flag = 0;

    sq_base = (u64)msg_chan->sq_info.dma_handle;

    /* notice base addr of SQ */
    devdrv_set_admin_sq_base(msg_dev->pci_ctrl, sq_base);

    msg_dev->admin_msg_chan = msg_chan;

    return 0;
}

int devdrv_msg_chan_init(struct devdrv_msg_dev *msg_dev, int chan_start, int chan_end, int irq_base)
{
    int i;
    struct devdrv_msg_chan *msg_chan = NULL;

    for (i = chan_start; i < chan_end; i++) {
        msg_chan = &msg_dev->msg_chan[i];
        msg_chan->chan_id = (u32)i;
        msg_chan->status = DEVDRV_DISABLE;
        msg_chan->msg_dev = msg_dev;
        msg_chan->io_base = msg_dev->db_io_base + (long)i * DEVDRV_DB_QUEUE_TYPE * DEVDRV_MSG_CHAN_DB_OFFSET;

        msg_chan->irq_rx_msg_notify = irq_base + (i - chan_start) * DEVDRV_MSG_CHAN_IRQ_NUM;
        msg_chan->irq_tx_finish_notity = irq_base + (i - chan_start) * DEVDRV_MSG_CHAN_IRQ_NUM + 1;

        devdrv_debug("msg_chan init. (dev_id=%u; db_io_base=%pK; rx_msg_notify=%d; tx_finish_notity=%d)\n",
                     msg_dev->pci_ctrl->dev_id, msg_chan->io_base, msg_chan->irq_rx_msg_notify,
                     msg_chan->irq_tx_finish_notity);

        mutex_init(&msg_chan->mutex);
        mutex_init(&msg_chan->rx_mutex);
    }
    return 0;
}

STATIC void devdrv_msg_work_queue_uninit(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_msg_dev *msg_dev)
{
    int max_msg_chan_cnt;
    int i;

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        max_msg_chan_cnt = pci_ctrl->ops.get_vf_max_msg_chan_cnt();
    } else {
        max_msg_chan_cnt = pci_ctrl->ops.get_pf_max_msg_chan_cnt();
    }

    if (max_msg_chan_cnt > DEVDRV_MAX_MSG_CHAN_NUM) {
        devdrv_warn("Real cnt greater than max num. (dev_id=%u, chan_cnt=%d)\n", pci_ctrl->dev_id, max_msg_chan_cnt);
        return;
    }

    for (i = 0; i < max_msg_chan_cnt; i++) {
        msg_dev->work_queue[i] = NULL;
    }
}

/* not each msg chan create one workqueue, host will create too much; so each msg_client create one workqueue */
STATIC int devdrv_msg_work_queue_init(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_msg_dev *msg_dev)
{
    int max_msg_chan_cnt;
    int i;

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        max_msg_chan_cnt = pci_ctrl->ops.get_vf_max_msg_chan_cnt();
    } else {
        max_msg_chan_cnt = pci_ctrl->ops.get_pf_max_msg_chan_cnt();
    }

    if (max_msg_chan_cnt > DEVDRV_MAX_MSG_CHAN_NUM) {
        devdrv_err("Real cnt greater than max num. (dev_id=%u, chan_cnt=%d)\n", pci_ctrl->dev_id, max_msg_chan_cnt);
        return -EINVAL;
    }

    for (i = 0; i < max_msg_chan_cnt; i++) {
        if (pci_ctrl->work_queue[i] == NULL) {
            msg_dev->work_queue[i] = create_workqueue("pcie_msg_workqueue");
            if (msg_dev->work_queue[i] == NULL) {
                devdrv_err("Create msg work_queue[%d] failed. (dev_id=%u)\n", i, pci_ctrl->dev_id);
                devdrv_msg_work_queue_uninit(pci_ctrl, msg_dev);
                return -ENOMEM;
            }
            pci_ctrl->work_queue[i] = msg_dev->work_queue[i];
        } else {
            msg_dev->work_queue[i] = pci_ctrl->work_queue[i];
        }
    }

    return 0;
}

int devdrv_msg_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret, irq_num, irq2_num, chan_cnt;
    struct device *dev = &pci_ctrl->pdev->dev;
    struct devdrv_msg_dev *msg_dev = NULL;
    int devdrv_pf_max_msg_chan_cnt;
    int devdrv_vf_max_msg_chan_cnt;

    irq_num = pci_ctrl->res.intr.msg_irq_num;
    irq2_num = (pci_ctrl->msix_irq_num > pci_ctrl->res.intr.msg_irq_vector2_base) ?
        pci_ctrl->msix_irq_num - pci_ctrl->res.intr.msg_irq_vector2_base : 0;
    irq2_num = (irq2_num > pci_ctrl->res.intr.msg_irq_vector2_num) ?
        pci_ctrl->res.intr.msg_irq_vector2_num : irq2_num;

    devdrv_pf_max_msg_chan_cnt = pci_ctrl->ops.get_pf_max_msg_chan_cnt();
    devdrv_vf_max_msg_chan_cnt = pci_ctrl->ops.get_vf_max_msg_chan_cnt();
    chan_cnt = irq_num / DEVDRV_MSG_CHAN_IRQ_NUM + irq2_num / DEVDRV_MSG_CHAN_IRQ_NUM;
    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        chan_cnt = (chan_cnt > devdrv_vf_max_msg_chan_cnt) ? devdrv_vf_max_msg_chan_cnt : chan_cnt;
    } else {
        chan_cnt = (chan_cnt > devdrv_pf_max_msg_chan_cnt) ? devdrv_pf_max_msg_chan_cnt : chan_cnt;
    }

    devdrv_info("Message init statr. (msix_irq_num=%d; irq_num=%d; irq2_num=%d; chan_cnt=%d)\r\n",
                pci_ctrl->msix_irq_num, irq_num, irq2_num, chan_cnt);

    msg_dev = devdrv_kzalloc(sizeof(struct devdrv_msg_dev), GFP_KERNEL);
    if (msg_dev == NULL) {
        devdrv_err("msg_dev devdrv_kzalloc failed. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -ENOMEM;
    }

    msg_dev->msg_chan = devdrv_kzalloc(sizeof(struct devdrv_msg_chan) * chan_cnt, GFP_KERNEL);
    if (msg_dev->msg_chan == NULL) {
        devdrv_err("msg_chan devdrv_kzalloc failed. (dev_id=%u; chan_cnt=%d)\n", pci_ctrl->dev_id,
            chan_cnt);
        devdrv_kfree(msg_dev);
        pci_ctrl->msg_dev = NULL;
        return -ENOMEM;
    }

    pci_ctrl->msg_dev = msg_dev;
    msg_dev->pci_ctrl = pci_ctrl;
    msg_dev->db_io_base = pci_ctrl->res.nvme_db_base;
    msg_dev->ctrl_io_base = pci_ctrl->res.nvme_pf_ctrl_base;
    msg_dev->reserve_mem_base = pci_ctrl->mem_base;
    msg_dev->local_reserve_mem_base = pci_ctrl->local_reserve_mem_base;

    msg_dev->chan_cnt = (u32)chan_cnt;
    msg_dev->dev = dev;
    mutex_init(&msg_dev->mutex);
    msg_dev->func_id = pci_ctrl->func_id;

    INIT_LIST_HEAD(&msg_dev->slave_mem_list);
    msg_dev->slave_mem.offset = DEVDRV_MSG_QUEUE_MEM_BASE;
    msg_dev->slave_mem.len = (u32)(pci_ctrl->res.msg_mem.size - msg_dev->slave_mem.offset);

    if (devdrv_msg_work_queue_init(pci_ctrl, msg_dev) != 0) {
        devdrv_kfree(msg_dev->msg_chan);
        msg_dev->msg_chan = NULL;
        devdrv_kfree(msg_dev);
        pci_ctrl->msg_dev = NULL;
        return -ENOMEM;
    }

    (void)devdrv_msg_chan_init(msg_dev, 0, irq_num / DEVDRV_MSG_CHAN_IRQ_NUM, pci_ctrl->res.intr.msg_irq_base);
    if (irq2_num / DEVDRV_MSG_CHAN_IRQ_NUM > 0) {
        (void)devdrv_msg_chan_init(msg_dev, irq_num / DEVDRV_MSG_CHAN_IRQ_NUM, chan_cnt,
                                   pci_ctrl->res.intr.msg_irq_vector2_base);
    }

    ret = devdrv_init_admin_msg_chan(msg_dev);
    if (ret != 0) {
        devdrv_err("Admin queue init failed. (dev_id=%u; ret=%d)\n", pci_ctrl->dev_id, ret);
        devdrv_msg_work_queue_uninit(pci_ctrl, msg_dev);
        devdrv_kfree(msg_dev->msg_chan);
        msg_dev->msg_chan = NULL;
        devdrv_kfree(msg_dev);
        pci_ctrl->msg_dev = NULL;
    }

    return ret;
}

void devdrv_msg_exit(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;
    u32 i;
    struct devdrv_msg_chan *msg_chan = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct devdrv_msg_slave_mem_node *node = NULL;

    if (msg_dev == NULL) {
        devdrv_info("msg_dev has been free. (dev_id=%u)\n", pci_ctrl->dev_id);
        return;
    }

    for (i = 0; i < msg_dev->chan_cnt; i++) {
        msg_chan = &msg_dev->msg_chan[i];
        if (msg_chan->status == DEVDRV_ENABLE) {
            devdrv_free_msg_queue_res(msg_chan);
        }
    }

    devdrv_msg_work_queue_uninit(pci_ctrl, msg_dev);

    mutex_lock(&msg_dev->mutex);
    if (list_empty_careful(&msg_dev->slave_mem_list) == 0) {
        list_for_each_safe(pos, n, &msg_dev->slave_mem_list)
        {
            node = list_entry(pos, struct devdrv_msg_slave_mem_node, list);
            list_del(&node->list);
            devdrv_kfree(node);
            node = NULL;
        }
    }
    mutex_unlock(&msg_dev->mutex);
    devdrv_kfree(msg_dev->msg_chan);
    msg_dev->msg_chan = NULL;
    devdrv_kfree(msg_dev);
    pci_ctrl->msg_dev = NULL;
    devdrv_info("Call devdrv_msg_exit success. (dev_id=%u)\n", pci_ctrl->dev_id);
}

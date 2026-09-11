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

#include "ascend_kernel_hal.h"
#include "ka_base_pub.h"

#include "devmm_proc_info.h"
#include "devmm_common.h"
#include "devmm_addr_mng.h"

static struct devmm_mem_node *devmm_search_mem_node_no_lock(struct devmm_addr_mng *addr_mng, u64 va, u64 len)
{
    struct devmm_mem_node *mem_node = NULL;
    ka_rb_node_t *node = NULL;
    u64 tmp_va, tmp_len;

    tmp_len = ka_base_round_up((va - ka_base_round_down(va, DEVMM_MEM_NODE_VA_ALIGN)) + len, DEVMM_MEM_NODE_VA_ALIGN);
    tmp_va = ka_base_round_down(va, DEVMM_MEM_NODE_VA_ALIGN);

    node = ka_base_get_rb_root_node(&addr_mng->rbtree);
    while (node != NULL) {
        mem_node = (struct devmm_mem_node *)ka_base_rb_entry(node, struct devmm_mem_node, node);

        if ((tmp_va + tmp_len) <= mem_node->va) {
            node = ka_base_get_rb_node_left(node);
        } else if (tmp_va >= (mem_node->va + mem_node->len)) {
            node = ka_base_get_rb_node_right(node);
        } else {
            break;
        }
    }

    if (mem_node != NULL) {
        if ((tmp_va < mem_node->va) || ((tmp_va + tmp_len) > (mem_node->va + mem_node->len))) {
            mem_node = NULL;
        }
    }

    return mem_node;
}

struct devmm_mem_node *devmm_search_mem_node(struct devmm_addr_mng *addr_mng, u64 va, u64 len)
{
    struct devmm_mem_node *mem_node = NULL;

    ka_task_down_read(&addr_mng->rbtree_mutex);
    mem_node = devmm_search_mem_node_no_lock(addr_mng, va, len);
    ka_task_up_read(&addr_mng->rbtree_mutex);

    return mem_node;
}

STATIC int devmm_insert_mem_node(struct devmm_addr_mng *addr_mng, struct devmm_mem_node *mem_node)
{
    ka_rb_node_t *parent = NULL;
    ka_rb_node_t **new_node = NULL;

    ka_task_down_write(&addr_mng->rbtree_mutex);

    new_node = ka_base_get_rb_root_node_addr(&addr_mng->rbtree);

    /* Figure out where to put new node */
    while (*new_node) {
        struct devmm_mem_node *this =
            (struct devmm_mem_node *)ka_base_rb_entry(*new_node, struct devmm_mem_node, node);

        parent = *new_node;
        if ((mem_node->va + mem_node->len) <= this->va) {
            new_node = ka_base_get_rb_node_left_addr(*new_node);
        } else if (mem_node->va >= (this->va + this->len)) {
            new_node = ka_base_get_rb_node_right_addr(*new_node);
        } else {
            ka_task_up_write(&addr_mng->rbtree_mutex);
            return -EFAULT;
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(&mem_node->node, parent, new_node);
    ka_base_rb_insert_color(&mem_node->node, &addr_mng->rbtree);

    ka_task_up_write(&addr_mng->rbtree_mutex);

    return 0;
}

STATIC void devmm_erase_mem_node(struct devmm_addr_mng *addr_mng, struct devmm_mem_node *mem_node)
{
    ka_base_rb_erase(&mem_node->node, &addr_mng->rbtree);
}

/* Returning true means that the corresponding add_info is saved, including the page structure and dma address. */
bool devmm_mem_node_is_store_addr_info(struct devmm_mem_node *mem_node)
{
    return (mem_node->addr_info != NULL);
}

static struct devmm_mem_node *devmm_alloc_mem_node(u64 va, u64 len, u32 page_size, bool is_need_dma_map)
{
    struct devmm_mem_node *mem_node = NULL;
    u64 tmp_va, tmp_len;
    size_t size;

    tmp_len = ka_base_round_up((va - ka_base_round_down(va, DEVMM_MEM_NODE_VA_ALIGN)) + len, DEVMM_MEM_NODE_VA_ALIGN);
    tmp_va = ka_base_round_down(va, DEVMM_MEM_NODE_VA_ALIGN);

    if (tmp_len < page_size) {
        return NULL;
    }

    if (is_need_dma_map) {
        size = sizeof(struct devmm_mem_node) + sizeof(struct devmm_addr_info) * (tmp_len / page_size);
    } else {
        size = sizeof(struct devmm_mem_node);
    }

    mem_node = (struct devmm_mem_node *)devmm_kvzalloc(size);
    if (mem_node == NULL) {
        devmm_drv_err("Kvzalloc failed. (size=%lx)\n", (unsigned long)size);
        return NULL;
    }

    mem_node->va = tmp_va;
    mem_node->len = tmp_len;
    mem_node->page_size = page_size;
    mem_node->addr_info = is_need_dma_map ? (struct devmm_addr_info *)(mem_node + 1) : NULL;

    devmm_drv_debug("Vaddress and len alloc memory node. (is_need_dma_map=%u; va=%llx; len=%llx)\n",
        is_need_dma_map, tmp_va, tmp_len);

    return mem_node;
}

STATIC void devmm_free_mem_node(struct devmm_mem_node *mem_node)
{
    devmm_drv_debug("Vaddress and len free memory node. (va=%llx; len=%llx).\n", mem_node->va, mem_node->len);
    devmm_kvfree(mem_node);
}

static inline bool devmm_is_need_dma_map(struct devmm_addr_mng *addr_mng, u64 va, u64 len)
{
    bool is_dev_readonly = (va >= DEVMM_DEV_READ_ONLY_ADDR_START) && (va + len <= DEVMM_DEV_READ_ONLY_ADDR_END);
    return (addr_mng->is_need_dma_map || is_dev_readonly);
}

struct devmm_mem_node *devmm_get_mem_node(struct devmm_addr_mng *addr_mng, ka_pid_t devpid, u64 va, u64 len, u32 page_size)
{
    struct devmm_mem_node *mem_node = devmm_search_mem_node(addr_mng, va, len);
    if (mem_node != NULL) {
        return mem_node;
    }

    mem_node = devmm_alloc_mem_node(va, len, page_size, devmm_is_need_dma_map(addr_mng, va, len));
    if (mem_node != NULL) {
        if (devmm_insert_mem_node(addr_mng, mem_node) != 0) {
            devmm_free_mem_node(mem_node);
            /* Other threads may have inserted the same node  */
            mem_node = devmm_search_mem_node(addr_mng, va, len);
        }
    }

    return mem_node;
}

int devmm_try_free_mem_node(struct devmm_addr_mng *addr_mng, struct devmm_mem_node *mem_node)
{
    if ((mem_node != NULL) && ka_base_atomic_read(&mem_node->valid_page_num) == 0) {
        ka_task_down_write(&addr_mng->rbtree_mutex);
        devmm_erase_mem_node(addr_mng, mem_node);
        ka_task_up_write(&addr_mng->rbtree_mutex);
        devmm_free_mem_node(mem_node);
        return 0;
    }
    return -EFAULT;
}

struct devmm_addr_info *devmm_get_addr_info(struct devmm_mem_node *mem_node, u64 va)
{
    u32 offset;

    if ((mem_node == NULL) || (devmm_mem_node_is_store_addr_info(mem_node) == false) ||
        (va < mem_node->va) || (va >= (mem_node->va + mem_node->len))) {
        return NULL;
    }

    offset = (u32)((va - mem_node->va) / mem_node->page_size);

    return &mem_node->addr_info[offset];
}

void devmm_set_ka_dma_addr_to_addr_info(const struct devmm_addr_info *in_addr_info,
    struct devmm_mem_node *mem_node, struct devmm_addr_info *addr_info)
{
    *addr_info = *in_addr_info;
}

int devmm_dma_map_page(u32 dev_id, ka_page_t *page, u32 len,
    struct devmm_mem_node *mem_node, struct devmm_addr_info *addr_info)
{
    ka_device_t *dev = NULL;
    ka_dma_addr_t dma_addr = 0;
    int ret;

    if (addr_info == NULL) {
        devmm_drv_err("Get device failed. (dev_id=%u)\n", dev_id);
        return -EFAULT;
    }

    /* null of host agent dev is normal */
    dev = devmm_device_get_by_devid(dev_id);
    if (dev != NULL) {
        dma_addr = devdrv_dma_map_page(dev, page, 0, len, KA_DMA_BIDIRECTIONAL);
        ret = ka_mm_dma_mapping_error(dev, dma_addr);
        devmm_device_put_by_devid(dev_id);
        if (ret != 0) {
            devmm_drv_err("Dma map page failed. (dev_id=%u; len=%x; ret=%d)\n", dev_id, len, ret);
            return -EFAULT;
        }
    } else {
        dma_addr = 0;
    }

    addr_info->dev_id = dev_id;
    addr_info->page = page;
    addr_info->len = len;
    addr_info->addr = dma_addr;
    if (mem_node != NULL) {
        ka_base_atomic_inc(&mem_node->valid_page_num);
    }
    devmm_drv_debug("Dma map page details. (dev_id=%u; len=%x; addr=0x%llx)\n",
        dev_id, len, (u64)addr_info->addr);
    return 0;
}

void devmm_dma_unmap_page(struct devmm_mem_node *mem_node, struct devmm_addr_info *addr_info)
{
    if (addr_info != NULL) {
        if ((addr_info->addr != 0) && (addr_info->len != 0)) {
            ka_device_t *dev = devmm_device_get_by_devid(addr_info->dev_id);
            devmm_drv_debug("Dma unmap page details. (addr=%llx; len=%x)\n", (u64)addr_info->addr, addr_info->len);
            if (dev != NULL) {
                devdrv_dma_unmap_page(dev, addr_info->addr, addr_info->len, KA_DMA_BIDIRECTIONAL);
                devmm_device_put_by_devid(addr_info->dev_id);
            }
        }
        addr_info->page = NULL;
        addr_info->addr = 0;
        addr_info->len = 0;
    }

    if (mem_node != NULL) {
        ka_base_atomic_dec(&mem_node->valid_page_num);
    }
}

STATIC void devmm_dma_unmap_all_page(struct devmm_mem_node *mem_node)
{
    int num = (int)(mem_node->len / mem_node->page_size);

    if (devmm_mem_node_is_store_addr_info(mem_node)) {
        u32 stamp = (u32)ka_jiffies;
        int i;
        for (i = 0; i < num; i++) {
            devmm_dma_unmap_page(NULL, &mem_node->addr_info[i]);
            devmm_try_cond_resched(&stamp);
        }
    }
    atomic_sub(num, &mem_node->valid_page_num);
}

void devmm_addr_mng_free_res(struct devmm_addr_mng *addr_mng)
{
    struct devmm_mem_node *mem_node = NULL;
    ka_rb_node_t *node = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_task_down_write(&addr_mng->rbtree_mutex);
    node = ka_base_rb_first(&addr_mng->rbtree);
    while (node != NULL) {
        mem_node = (struct devmm_mem_node *)ka_base_rb_entry(node, struct devmm_mem_node, node);
        node = ka_base_rb_next(node);

        devmm_dma_unmap_all_page(mem_node);
        if (ka_base_atomic_read(&mem_node->valid_page_num) != 0) {
            devmm_drv_warn("Memnode details. (va=0x%llx; len=%llu; valid_page_num=%d)\n",
                mem_node->va, mem_node->len, ka_base_atomic_read(&mem_node->valid_page_num));
        }

        devmm_erase_mem_node(addr_mng, mem_node);
        devmm_free_mem_node(mem_node);
        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_write(&addr_mng->rbtree_mutex);
}

void devmm_addr_mng_free_res_by_addr(struct devmm_addr_mng *addr_mng, u64 start, u64 end)
{
    struct devmm_mem_node *mem_node = NULL;
    ka_rb_node_t *node = NULL;
    u32 stamp = (u32)ka_jiffies;

    ka_task_down_write(&addr_mng->rbtree_mutex);
    node = ka_base_rb_first(&addr_mng->rbtree);
    while (node != NULL) {
        mem_node = (struct devmm_mem_node *)ka_base_rb_entry(node, struct devmm_mem_node, node);
        node = ka_base_rb_next(node);
        if ((mem_node->va >= start) && (mem_node->va < end)) {
            devmm_dma_unmap_all_page(mem_node);
            if (ka_base_atomic_read(&mem_node->valid_page_num) != 0) {
                devmm_drv_warn("Memnode details. (va=0x%llx; len=%llu; valid_page_num=%d)\n",
                    mem_node->va, mem_node->len, ka_base_atomic_read(&mem_node->valid_page_num));
            }
            devmm_erase_mem_node(addr_mng, mem_node);
            devmm_free_mem_node(mem_node);
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_write(&addr_mng->rbtree_mutex);
}

struct devmm_mem_node *devmm_get_addr_mem_node(struct devmm_addr_mng *addr_mng, u64 va, u64 len)
{
    struct devmm_mem_node *mem_node = NULL;

    ka_task_down_read(&addr_mng->rbtree_mutex);
    mem_node = devmm_search_mem_node_no_lock(addr_mng, va, len);
    if (mem_node == NULL) {
        ka_task_up_read(&addr_mng->rbtree_mutex);
        return NULL;
    }
    ka_base_atomic_inc(&mem_node->user_cnt);
    ka_task_up_read(&addr_mng->rbtree_mutex);

    return mem_node;
}

void devmm_put_addr_mem_node(struct devmm_addr_mng *addr_mng, u64 va, u64 len)
{
    struct devmm_mem_node *mem_node = NULL;

    ka_task_down_read(&addr_mng->rbtree_mutex);
    mem_node = devmm_search_mem_node_no_lock(addr_mng, va, len);
    if (mem_node == NULL) {
        ka_task_up_read(&addr_mng->rbtree_mutex);
        return;
    }
    (void)atomic_dec_if_positive(&mem_node->user_cnt);
    ka_task_up_read(&addr_mng->rbtree_mutex);

    return;
}

bool devmm_mem_node_is_in_use(struct devmm_mem_node *mem_node)
{
    u32 use_cnt;

    if (mem_node == NULL) {
        return false;
    }
    use_cnt = (u32)ka_base_atomic_read(&mem_node->user_cnt);
    if (use_cnt != 0) {
        return true;
    }
    return false;
}

typedef bool (*devmm_mem_attr_check_func)
(struct devmm_svm_process *svm_proc, struct devmm_mem_node *mem_node, u64 vaddr, u64 size, u32 page_size);

static bool devmm_mem_is_alloced(struct devmm_svm_process *svm_proc, struct devmm_mem_node *mem_node,
    u64 vaddr, u64 size, u32 page_size)
{
    struct devmm_addr_info *addr_info = NULL;
    ka_vm_area_struct_t *vma = NULL;
    u64 end_addr = vaddr + size;
    u64 tmp_vaddr = vaddr;
    u64 pfn, kpg_size;

    for (; tmp_vaddr < end_addr; tmp_vaddr += page_size) {
        if (devmm_mem_node_is_store_addr_info(mem_node) == false) {
            vma = devmm_find_vma(svm_proc, tmp_vaddr);
            if (vma == NULL) {
                return false;
            }
            if (devmm_va_to_pfn(vma, vaddr, &pfn, &kpg_size) != 0) {
                return false;
            }
        } else {
            addr_info = devmm_get_addr_info(mem_node, vaddr);
            if (addr_info == NULL || addr_info->page == NULL) {
                return false;
            }
        }
    }
    return true;
}

static bool devmm_mem_is_readonly(struct devmm_svm_process *svm_proc, struct devmm_mem_node *mem_node,
    u64 vaddr, u64 size, u32 page_size)
{
    if (devmm_is_readonly_mem(mem_node->page_prot) == false) {
        return false;
    }

    return devmm_mem_is_alloced(svm_proc, mem_node, vaddr, size, page_size);
}

bool devmm_mem_attr_is_match(struct devmm_addr_mng *addr_mng, u64 va, u64 size, u32 page_size, u32 mem_attr)
{
    devmm_mem_attr_check_func mem_attr_check_func[DEVMM_MEM_ATTR_TYPE_MAX] = {
        [DEVMM_MEM_ATTR_READONLY] = devmm_mem_is_readonly,
        [DEVMM_MEM_ATTR_DVPP] = devmm_mem_is_alloced,
        [DEVMM_MEM_ATTR_DEV] = devmm_mem_is_alloced,
    };
    struct devmm_svm_process *svm_proc = ka_container_of(addr_mng, struct devmm_svm_process, addr_mng);
    struct devmm_mem_node *mem_node = NULL;
    u64 vaddr, end_addr, check_size;
    u32 stamp = (u32)ka_jiffies;
    bool is_match = false;

    end_addr = ka_base_round_up(va + size, page_size);
    vaddr = ka_base_round_down(va, page_size);
    check_size = end_addr - vaddr;

    ka_task_mutex_lock(&svm_proc->mem_node_mutex);
    mem_node = devmm_search_mem_node(addr_mng, vaddr, check_size);
    if (mem_node != NULL) {
        is_match = (mem_attr_check_func[mem_attr](svm_proc, mem_node, vaddr, check_size, page_size));
        ka_task_mutex_unlock(&svm_proc->mem_node_mutex);
        return is_match;
    }
    ka_task_mutex_unlock(&svm_proc->mem_node_mutex);

    for (; vaddr < end_addr; vaddr += page_size) {
        ka_task_mutex_lock(&svm_proc->mem_node_mutex);
        mem_node = devmm_search_mem_node(addr_mng, vaddr, page_size);
        if (mem_node == NULL) {
            ka_task_mutex_unlock(&svm_proc->mem_node_mutex);
            return false;
        }
        if (mem_attr_check_func[mem_attr](svm_proc, mem_node, vaddr, page_size, page_size) == false) {
#ifndef EMU_ST
            ka_task_mutex_unlock(&svm_proc->mem_node_mutex);
#endif
            return false;
        }
        ka_task_mutex_unlock(&svm_proc->mem_node_mutex);
        devmm_try_cond_resched(&stamp);
    }
    return true;
}

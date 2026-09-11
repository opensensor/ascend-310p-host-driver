/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)) || (defined(DRV_UT)))
#include <linux/vfio.h>
#endif

#include <linux/kvm_host.h>
#include <linux/vmalloc.h>
#include <linux/iova.h>
#include <linux/iommu.h>
#include "dvt.h"
#include "vfio_ops.h"
#include "kvmdt.h"
#include "dma_pool.h"

/**
 * try to get rwsem down lock or read lock,
 * the time will last forever if wait_mins is 0
 */
STATIC int hw_vdavinci_rwsem_trylock(struct hw_vdavinci *vdavinci,
                                     struct rw_semaphore *sem,
                                     unsigned long wait_mins,
                                     bool is_write)
{
#define VDAVINCI_LOCK_INTERVAL      500
#define VDAVINCI_LOCK_WARN_INTERVAL (1 * 60 * 1000)
    unsigned long wait_time = 0;

    while (true) {
        if (is_write && down_write_trylock(sem) != 0) {
            return 0;
        }
        if (!is_write && down_read_trylock(sem) != 0) {
            return 0;
        }
        if (wait_mins != 0 && wait_time >= wait_mins * VDAVINCI_LOCK_WARN_INTERVAL) {
            return -EAGAIN;
        }
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout((long)msecs_to_jiffies(VDAVINCI_LOCK_INTERVAL));
        wait_time += VDAVINCI_LOCK_INTERVAL;
        if (wait_time % VDAVINCI_LOCK_WARN_INTERVAL == 0) {
            vascend_warn(vdavinci_get_device(vdavinci),
                         "the time of getting down %s lock is more than %lu minutes\n",
                         is_write ? "write" : "read",
                         wait_time / VDAVINCI_LOCK_WARN_INTERVAL);
        }
    }
}

STATIC void hw_vdavinci_rwsem_unlock(struct hw_vdavinci *vdavinci,
                                     struct rw_semaphore *sem,
                                     bool is_write)
{
    if (is_write) {
        up_write(sem);
    } else {
        up_read(sem);
    }
}

STATIC bool hw_vdavinci_changed_cpu(struct task_struct *p,
                                    const struct cpumask *next_mask)
{
    if (cpumask_empty(next_mask)) {
        return false;
    }

    if (cpumask_test_cpu(smp_processor_id(), next_mask)) {
        return false;
    }

    if (set_cpus_allowed_ptr(p, next_mask) != 0) {
        return false;
    }

    return true;
}

bool hw_vdavinci_scheduled(struct hw_vdavinci *vdavinci,
                           unsigned long current_pages,
                           unsigned long max_pages,
                           unsigned int timeout,
                           struct page *page)
{
    cpumask_var_t next_mask;
    bool ret;

    if (page != NULL) {
        if (!zalloc_cpumask_var(&next_mask, GFP_KERNEL)) {
            return false;
        }
        ret = get_node_cpu_by_page(vdavinci, smp_processor_id(), page, next_mask);
        if (ret && hw_vdavinci_changed_cpu(current, next_mask)) {
            free_cpumask_var(next_mask);
            return true;
        }
        free_cpumask_var(next_mask);
    }
    if (current_pages >= max_pages) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout((long)msecs_to_jiffies(timeout));
        return true;
    }

    return false;
}

STATIC void dev_pin_scheduled(struct hw_vdavinci *vdavinci,
                              unsigned long *count,
                              struct page *page)
{
    (*count)++;
    if (hw_vdavinci_scheduled(vdavinci,
                              (*count) * VFIO_PIN_PAGES_MAX_ENTRIES,
                              VDAVINCI_PIN_PAGES_OF_SCHEDULE,
                              VDAVINCI_PIN_TIME_OF_SCHEDULE,
                              page)) {
        (*count) = 0;
    }
}

STATIC int hw_vdavinci_vfio_pin_pages(struct hw_vdavinci *vdavinci,
                                      struct vdavinci_pin_info *pin_info)
{
    int ret = 0, i = 0;

    if (pin_info->npage <= 0 || pin_info->npage > (int)VFIO_PIN_PAGES_MAX_ENTRIES) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci error npage or gfn: %d, %lx\n",
                    pin_info->npage, pin_info->gfn);
        return -EINVAL;
    }
    if (pin_info->pages == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci's pages is null");
        return -EINVAL;
    }
    ret = vdavinci_pin_pages(vdavinci, pin_info);
    if (ret < 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci pin pages failed, ret: %d\n", ret);
        return ret;
    }
    for (i = 0; i < pin_info->npage; i++) {
        if (!pfn_valid(page_to_pfn(pin_info->pages[i]))) {
            vascend_warn(vdavinci_to_dev(vdavinci), "vid: %u pfn 0x%lx is not mem backed\n",
                         vdavinci->id, page_to_pfn(pin_info->pages[i]));
            ret = -EFAULT;
            goto unpin;
        }
    }

    return 0;

unpin:
    vdavinci_unpin_pages(vdavinci, pin_info);
    return ret;
}

STATIC void hw_vdavinci_vfio_unpin_pages(struct hw_vdavinci *vdavinci,
                                         struct vdavinci_pin_info *pin_info)
{
    if (pin_info->npage <= 0 || pin_info->npage > (int)VFIO_PIN_PAGES_MAX_ENTRIES) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci error npage: %d\n", pin_info->npage);
        return;
    }
    vdavinci_unpin_pages(vdavinci, pin_info);
}

STATIC int add_dma_page_list(struct page_info_list *dma_page_list, unsigned long gfn,
    unsigned int size, struct page *page)
{
    struct page_info_entry *dma_page_info = NULL;

    dma_page_info = list_empty(&(dma_page_list->head)) == true ?
        NULL : list_last_entry(&(dma_page_list->head), struct page_info_entry, list);
    if (dma_page_info) {
        if (page_to_pfn(dma_page_info->page) == page_to_pfn(page)) {
            dma_page_info->length = size;
            return 0;
        }
    }

    dma_page_info = kzalloc(sizeof(struct page_info_entry), GFP_KERNEL);
    if (dma_page_info == NULL) {
        return -ENOMEM;
    }

    dma_page_info->gfn = gfn;
    dma_page_info->length = size;
    dma_page_info->page = page;
    list_add_tail(&(dma_page_info->list), &(dma_page_list->head));
    dma_page_list->elem_num++;

    return 0;
}

/**
 * When dealing with pfn, it is necessary to judge whether it is
 * continuous with the last area in dma_page_list. When a discontinuous
 * pfn or the last pfn is found, this area will be added to the
 * dma_page_list. Return success only when all pfns has added to dma_page_list.
 * NOTICE: the dma_page_list must be and should be empty at first.
 */
STATIC int hw_vdavinci_add_pfn_to_dma_list(struct hw_vdavinci *vdavinci,
                                           struct page_info_list *dma_page_list,
                                           struct vdavinci_pin_info *pin_info)
{
    int i, ret, last_end;
    unsigned int length = 0;
    unsigned long gfn = 0;
    struct page **pages = pin_info->pages;
    struct page_info_entry *dma_page_info, *tmp;

    if (dma_page_list->elem_num != 0) {
        return -EINVAL;
    }

    last_end = -1;
    for (i = 0; i < pin_info->npage; i++) {
         /* if launch the last pfn or find the pfn and the next pfn are
          * discontinuous, add this region into list.
          */
        if (i + 1 == pin_info->npage || page_to_pfn(pages[i]) + 1 != page_to_pfn(pages[i + 1])) {
            length = (i - last_end) * PAGE_SIZE;
            gfn = pin_info->gfn + last_end + 1;
            ret = add_dma_page_list(dma_page_list, gfn,
                                    length, pages[last_end + 1]);
            if (ret != 0) {
                goto clean_dma_page_list;
            }

            last_end = i;
        }
    }
    return 0;

clean_dma_page_list:
    list_for_each_entry_safe(dma_page_info, tmp, &dma_page_list->head, list) {
        list_del(&dma_page_info->list);
        kfree(dma_page_info);
    }
    dma_page_list->elem_num = 0;
    return ret;
}

/**
 * In order to DMA, we need to get the physical page according to gfn.
 * dma_page_list saves a series of continous physical pages. Doing pin pages
 * during DMA will reduce performance. The solution of using memory pool can
 * solve this problem. User will pin the physical memory corresponding to
 * a large section of gfn before DMA
 *
 * Improve performance by reducing the number of calls to vfio_pin_pages.
 * And the step size of the processing page is the same as vfio_pin_pages
 * and both are VFIO_PIN_PAGES_MAX_ENTRIES
 *
 * The length entered by the user is not necessarily aligned with the page
 * and needs to be modified
 */
STATIC int hw_vdavinci_pin_page_2m(struct hw_vdavinci *vdavinci,
                                   struct vdavinci_pin_info *pin_info,
                                   struct page_info_list *dma_page_list)
{
    int ret = 0;

    ret = hw_vdavinci_vfio_pin_pages(vdavinci, pin_info);
    if (ret != 0) {
        return ret;
    }
    ret = hw_vdavinci_add_pfn_to_dma_list(vdavinci, dma_page_list, pin_info);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci add pfns to dma list failed");
        goto unpin;
    }

    return 0;

unpin:
    hw_vdavinci_vfio_unpin_pages(vdavinci, pin_info);
    return ret;
}

STATIC int hw_vdavinci_init_ram_dma_array(struct ram_range_info *ram_info)
{
    unsigned long total_steps = 0, npages = ram_info->npages;
 
    total_steps = DIV_ROUND_UP(npages, VFIO_PIN_PAGES_MAX_ENTRIES);
    ram_info->dma_array = kzalloc(sizeof(struct dma_info_2m*) * total_steps, GFP_KERNEL);
    if (ram_info->dma_array == NULL) {
        return -ENOMEM;
    }
    ram_info->dma_array_len = 0;
 
    return 0;
}
 
STATIC void hw_vdavinci_clean_ram_dma_array(struct ram_range_info *ram_info)
{
    if (ram_info->dma_array != NULL) {
        kfree(ram_info->dma_array);
    }
    ram_info->dma_array_len = 0;
    ram_info->dma_array = NULL;
}
 
STATIC int hw_vdavinci_init_pin_info(struct vdavinci_pin_info *pin_info,
                                     unsigned long gfn, int npage, bool is_alloc)
{
    if (npage <= 0 || npage > (int)VFIO_PIN_PAGES_MAX_ENTRIES) {
        return -EINVAL;
    }
    pin_info->gfn = gfn;
    pin_info->npage = npage;
    if (!is_alloc) {
        return 0;
    }
    if (pin_info->pages != NULL) {
        return 0;
    }
    pin_info->pages = kzalloc(sizeof(struct page *) * VFIO_PIN_PAGES_MAX_ENTRIES, GFP_KERNEL);
    if (pin_info->pages == NULL) {
        return -ENOMEM;
    }
 
    return 0;
}
 
STATIC void hw_vdavinci_clean_pin_info(struct vdavinci_pin_info *pin_info)
{
    if (pin_info->pages != NULL) {
        kfree(pin_info->pages);
        pin_info->pages = NULL;
    }
    pin_info->gfn = 0;
    pin_info->npage = 0;
}

STATIC void hw_vdavinci_unpin_page_2m(struct hw_vdavinci *vdavinci,
                                      struct vdavinci_pin_info *pin_info,
                                      struct page_info_list *dma_page_list)
{
    int npage = 0, ret = 0;
    struct page_info_entry *dma_page_info = NULL;
    struct list_head *pos = NULL, *next = NULL;

    list_for_each_safe(pos, next, &(dma_page_list->head)) {
        dma_page_info = list_entry(pos, struct page_info_entry, list);
        npage = (int)DIV_ROUND_UP(dma_page_info->length, PAGE_SIZE);

        ret = hw_vdavinci_init_pin_info(pin_info, dma_page_info->gfn, npage, false);
        if (ret != 0) {
            vascend_err(vdavinci_to_dev(vdavinci), "init pin info failed\n");
        }
        hw_vdavinci_vfio_unpin_pages(vdavinci, pin_info);

        list_del(pos);
        kfree(dma_page_info);
    }
    hw_vdavinci_clean_pin_info(pin_info);
}

STATIC void hw_vdavinci_unpin_page_range(struct hw_vdavinci *vdavinci,
                                         struct ram_range_info *ram_info)
{
    int j;
    unsigned long count = 0;
    unsigned int dma_start_cpu;
    struct dma_info_2m *dma_array_node = NULL;
    struct page_info_entry *dma_page_info = NULL;
    struct vdavinci_pin_info pin_info = { 0 };

    dma_start_cpu = smp_processor_id();
    for (j = 0; j < ram_info->dma_array_len; j++) {
        dma_array_node = ram_info->dma_array[j];
        dma_page_info = list_first_entry(&(dma_array_node->dma_page_list.head),
                                         struct page_info_entry, list);
        dev_pin_scheduled(vdavinci, &count, dma_page_info->page);
        hw_vdavinci_unpin_page_2m(vdavinci, &pin_info, &(dma_array_node->dma_page_list));
        kfree(dma_array_node);
    }

    (void)hw_vdavinci_changed_cpu(current, cpumask_of(dma_start_cpu));
    hw_vdavinci_clean_ram_dma_array(ram_info);
}

STATIC int hw_vdavinci_pin_page_range(struct hw_vdavinci *vdavinci,
                                      struct ram_range_info *ram_info)
{
    int ret = 0;
    unsigned int dma_start_cpu;
    gfn_t base_gfn = ram_info->base_gfn;
    unsigned long npages_step = 0, count = 0, npages = ram_info->npages;
    struct dma_info_2m *new = NULL, **dma_array_temp = NULL;
    struct vdavinci_pin_info pin_info = { 0 };

    ret = hw_vdavinci_init_ram_dma_array(ram_info);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "init ram dma array error: %d", ret);
        return ret;

    }
    dma_array_temp = ram_info->dma_array;
    dma_start_cpu = smp_processor_id();

    while (npages) {
        npages_step = npages > VFIO_PIN_PAGES_MAX_ENTRIES ?
                        VFIO_PIN_PAGES_MAX_ENTRIES : npages;
        ret = hw_vdavinci_init_pin_info(&pin_info, base_gfn, (int)npages_step, true);
        if (ret != 0) {
            goto out;
        }

        new = (struct dma_info_2m *)kzalloc(sizeof(struct dma_info_2m), GFP_KERNEL);
        if (!new) {
            ret = -ENOMEM;
            goto out;
        }

        INIT_LIST_HEAD(&(new->dma_page_list.head));
        ret = hw_vdavinci_pin_page_2m(vdavinci, &pin_info, &(new->dma_page_list));
        if (ret != 0) {
            kfree(new);
            goto out;
        }

        new->gfn = base_gfn;
        new->size = npages_step * PAGE_SIZE;
        *dma_array_temp = new;

        npages -= npages_step;
        base_gfn += npages_step;
        dma_array_temp++;
        ram_info->dma_array_len++;
        dev_pin_scheduled(vdavinci, &count, pin_info.pages[0]);
    }

    (void)hw_vdavinci_changed_cpu(current, cpumask_of(dma_start_cpu));
    hw_vdavinci_clean_pin_info(&pin_info);
    return 0;
out:
    hw_vdavinci_unpin_page_range(vdavinci, ram_info);
    hw_vdavinci_clean_pin_info(&pin_info);
    hw_vdavinci_clean_ram_dma_array(ram_info);
    return ret;
}

STATIC void dma_dom_pool_unpin(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    struct list_head *pos = NULL, *next = NULL;
    struct ram_range_info *ram_info = NULL;

    if (!vm_dom || !vm_dom->ram_info_list) {
        return;
    }

    list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head)) {
        ram_info = list_entry(pos, struct ram_range_info, list);
        hw_vdavinci_unpin_page_range(vdavinci, ram_info);
        list_del(pos);
        kfree(ram_info);
    }

    vm_dom->status = DOMAIN_PIN_STATUS_INVALID;
}

/**
 * do unpin pages alone
 */
void hw_vdavinci_unpin_pages(struct hw_vdavinci *vdavinci)
{
    struct vm_dom_info *vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    struct device *dev = vdavinci_get_device(vdavinci);

    mutex_lock(&vdavinci->vdev.cache_lock);
    if (vm_dom && vm_dom->status != DOMAIN_PIN_STATUS_INVALID) {
        vascend_info(dev, "dma pool unpin pages start\n");
        down_write(&vm_dom->sem);
        dma_dom_pool_unpin(vdavinci, vm_dom);
        up_write(&vm_dom->sem);
        vascend_info(dev, "dma pool unpin pages end\n");
    }
    mutex_unlock(&vdavinci->vdev.cache_lock);
}

void hw_vdavinci_dma_pool_uninit(struct hw_vdavinci *vdavinci)
{
    struct vm_dom_info *vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    struct device *dev = vdavinci_get_device(vdavinci);
    struct dev_dom_info *dev_dom = NULL;

    if (!vm_dom) {
        vascend_err(dev, "dma pool uninit failed\n");
        return;
    }
    vascend_info(dev, "dma pool uninit start\n");
    mutex_lock(&vdavinci->vdev.cache_lock);
    down_write(&vm_dom->sem);
    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (!dev_dom) {
        vascend_info(dev, "dma pool had already uninited\n");
        up_write(&vm_dom->sem);
        mutex_unlock(&vdavinci->vdev.cache_lock);
        return;
    }
    up_write(&vm_dom->sem);

    dev_dom_info_put(dev_dom, vdavinci);

    mutex_unlock(&vdavinci->vdev.cache_lock);
    vascend_info(dev, "dma pool uninit success\n");
}

STATIC int raminfo_init(struct ram_range_info **ram, struct kvm_memory_slot *memslot)
{
    struct ram_range_info *ram_info;

    if (memslot->npages >= U64_MAX - memslot->base_gfn) {
        return -EINVAL;
    }

    ram_info = kzalloc(sizeof(struct ram_range_info), GFP_KERNEL);
    if (!ram_info) {
        return -ENOMEM;
    }

    ram_info->base_gfn = memslot->base_gfn;
    ram_info->userspace_addr = memslot->userspace_addr;
    ram_info->npages = memslot->npages;
    atomic_set(&ram_info->map_count, 0);

    *ram = ram_info;

    return 0;
}

#define ASCEND_RESERVE_IOVA_LENGTH    0x10000000    /* 256M */
STATIC int get_reserve_iova(struct device *dev, dma_addr_t *iova_addr, size_t *size)
{
    if (dev->coherent_dma_mask < ASCEND_RESERVE_IOVA_LENGTH) {
        return -EINVAL;
    }

    *iova_addr = dev->coherent_dma_mask - (ASCEND_RESERVE_IOVA_LENGTH - 1);
    *size = ASCEND_RESERVE_IOVA_LENGTH;

    return 0;
}

int get_reserve_iova_for_check(struct device *dev, dma_addr_t *iova_addr, size_t *size)
{
    struct hw_vdavinci *vdavinci = find_vdavinci(dev);

    if (vdavinci == NULL || !vdavinci->is_passthrough) {
        return -EINVAL;
    }

    return get_reserve_iova(dev, iova_addr, size);
}

STATIC int vm_reserve_iova(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    int ret;
    dma_addr_t iova_addr;
    size_t size;
    struct ram_range_info *ram_info;
    struct list_head *pos = NULL, *next = NULL;
    struct iova *iova_re;
    struct device *dev = vdavinci_get_device(vdavinci);

    if (vdavinci->is_passthrough) {
        ret = get_reserve_iova(dev, &iova_addr, &size);
        if (ret != 0) {
            return ret;
        }

        list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head)) {
            ram_info = list_entry(pos, struct ram_range_info, list);
            if (ram_info->base_gfn > (iova_addr >> PAGE_SHIFT) ||
                    ram_info->base_gfn + ram_info->npages > (iova_addr >> PAGE_SHIFT)) {
                vascend_err(dev, "reserve iova failed, ram base : 0x%llx, len : %ld\n",
                            ram_info->base_gfn, ram_info->npages);
                return -EINVAL;
            }
        }

        iova_re = reserve_iova(&vm_dom->iovad, 0,
                               (iova_addr >> PAGE_SHIFT) - 1);
        if (iova_re == NULL) {
            vascend_debug("dev iova reserve failed\n");
            return -EINVAL;
        }
    }
    return 0;
}

STATIC void raminfo_list_cleanup(struct list_head *slot_ram_list)
{
    struct ram_range_info *ram_info = NULL;
    struct list_head *pos = NULL, *next = NULL;

    list_for_each_safe(pos, next, slot_ram_list) {
        ram_info = list_entry(pos, struct ram_range_info, list);
        list_del(pos);
        kfree(ram_info);
    }
}

STATIC void raminfo_list_destroy(struct hw_vdavinci *vdavinci,
                                 struct list_head *slot_ram_list)
{
    raminfo_list_cleanup(slot_ram_list);
}

STATIC int raminfo_list_init(struct hw_vdavinci *vdavinci,
                             struct list_head *slot_ram_list)
{
    int ret = -1;
    struct ram_range_info *ram_info = NULL;
    struct kvm_memslots *slots = NULL;
    struct kvm_memory_slot *slot = NULL;
    int __maybe_unused iter = 0;

    mutex_lock(&(vdavinci->vdev.kvm->slots_lock));
    slots = kvm_memslots(vdavinci->vdev.kvm);

    vdavinci_for_each_memslot(slot, slots, iter) {
        if (slot->flags & KVM_MEM_READONLY) {
            continue;
        }
        ret = raminfo_init(&ram_info, slot);
        if (ret != 0) {
            vascend_err(vdavinci_to_dev(vdavinci), "vdavinci ram init failed, ret: %d\n", ret);
            mutex_unlock(&(vdavinci->vdev.kvm->slots_lock));
            goto out;
        }

        vascend_debug("pin add ram gfn 0x%llx\n", ram_info->base_gfn);
        list_add_tail(&(ram_info->list), slot_ram_list);
    }
    mutex_unlock(&(vdavinci->vdev.kvm->slots_lock));

    return 0;
out:
    raminfo_list_cleanup(slot_ram_list);

    return ret;
}

STATIC int dma_dom_pool_pin(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    int ret_t = 0, ret = 0;
    struct ram_range_info *ram_info = NULL;
    struct list_head slot_ram_list;
    unsigned long pfn = 0;
    struct list_head *pos = NULL, *next = NULL;

    INIT_LIST_HEAD(&(slot_ram_list));

    ret = raminfo_list_init(vdavinci, &slot_ram_list);
    if (ret != 0) {
        return ret;
    }

    list_for_each_safe(pos, next, &(slot_ram_list)) {
        ram_info = list_entry(pos, struct ram_range_info, list);
        pfn = g_hw_kvmdt_ops.gfn_to_mfn(vdavinci->handle, ram_info->base_gfn);
        if (!page_is_ram(pfn)) {
            vascend_warn(vdavinci_to_dev(vdavinci),
                         "page is not ram, npage: %lu", ram_info->npages);
            continue;
        }

        ret_t = hw_vdavinci_pin_page_range(vdavinci, ram_info);
        if (ret_t) {
            vascend_warn(vdavinci_to_dev(vdavinci),
                         "page may not be ram : %lu, ret : %d\n",
                         ram_info->npages, ret_t);
            continue;
        }

        list_del(pos);
        list_add_tail(&(ram_info->list), &(vm_dom->ram_info_list->head));
    }

    raminfo_list_destroy(vdavinci, &slot_ram_list);

    if (list_empty(&(vm_dom->ram_info_list->head))) {
        return -EFAULT;
    }

    vm_dom->status = DOMAIN_PIN_STATUS_READY;
    return 0;
}

STATIC void hw_vdavinci_set_dev_dom_ops(struct dev_dom_info *dom,
                                        bool is_passthrough)
{
    dom->is_passthrough = is_passthrough;
    vascend_debug("set dom is vf passthrough %d\n", is_passthrough);

    if (is_passthrough) {
        dom->ops.dev_dma_map_ram_range = vf_map_ram_range;
        dom->ops.dev_dma_unmap_ram_range = vf_unmap_ram_range;
        dom->ops.hw_vdavinci_get_iova_sg = vf_get_iova_sg;
        dom->ops.hw_vdavinci_get_iova_array = vf_get_iova_array;
    } else {
        dom->ops.dev_dma_map_ram_range = dev_dma_map_ram_range;
        dom->ops.dev_dma_unmap_ram_range = dev_dma_unmap_ram_range;
        dom->ops.hw_vdavinci_get_iova_sg = hw_vdavinci_get_iova_sg;
        dom->ops.hw_vdavinci_get_iova_array = hw_vdavinci_get_iova_array;
    }
}

STATIC int hw_vdavinci_dma_pool_init_locked(struct hw_vdavinci *vdavinci)
{
    int ret = 0;
    struct ram_range_info *ram_info = NULL;
    struct vm_dom_info *vm_dom = vdavinci->vdev.domain;
    struct dev_dom_info *dev_dom = NULL;
    struct list_head *pos = NULL, *next = NULL;
    struct device *dev = vdavinci_get_device(vdavinci);

    vascend_info(dev, "dma pool init start\n");

    ret = hw_vdavinci_rwsem_trylock(vdavinci, &vm_dom->sem, 0, true);
    if (ret) {
        vascend_err(dev, "get down write lock failed, ret: %d", ret);
        return ret;
    }

    if (vm_dom->status == DOMAIN_PIN_STATUS_INVALID) {
        ret = dma_dom_pool_pin(vdavinci, vm_dom);
        if (ret) {
            vascend_err(dev, "vm pin page failed, ret : %d", ret);
            goto unpin;
        }
    }

    dev_dom = dev_dom_info_get(vm_dom, vdavinci);
    if (!dev_dom) {
        ret = -ENOMEM;
        vascend_err(dev, "dev get dma domian failed, ret : %d", ret);
        goto unpin;
    }

    ret = vm_reserve_iova(vdavinci, vm_dom);
    if (ret) {
        vascend_err(dev, "reserve iova failed, ret : %d", ret);
        goto dom;
    }
    hw_vdavinci_rwsem_unlock(vdavinci, &vm_dom->sem, true);

    hw_vdavinci_set_dev_dom_ops(dev_dom, vdavinci->is_passthrough);
    if (dev_dom->status == DOMAIN_MAP_STATUS_INVALID) {
        if (vdavinci->is_passthrough) {
            ret = hw_vdavinci_iommu_attach_group(vdavinci);
            if (ret) {
                vascend_err(dev, "dev attach new group failed, ret : %d", ret);
                goto unmap;
            }
        }
        INIT_LIST_HEAD(&(vdavinci->vdev.dev_dma_info_list_head));
        list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head)) {
            ram_info = list_entry(pos, struct ram_range_info, list);
            ret = dev_dom->ops.dev_dma_map_ram_range(vdavinci, ram_info);
            if (ret) {
                vascend_err(dev, "dev dma map failed, ret : %d", ret);
                goto unmap;
            }
        }

        dev_dom->status = DOMAIN_MAP_STATUS_READY;
    }

    vascend_info(dev, "dma pool init success\n");
    return 0;

unmap:
    (void)hw_vdavinci_rwsem_trylock(vdavinci, &vm_dom->sem, 0, true);
dom:
    dev_dom_info_put(dev_dom, vdavinci);
unpin:
    dma_dom_pool_unpin(vdavinci, vm_dom);
    hw_vdavinci_rwsem_unlock(vdavinci, &vm_dom->sem, true);
    return ret;
}

int hw_vdavinci_dma_pool_init(struct hw_vdavinci *vdavinci)
{
    int ret = -EINVAL;

    mutex_lock(&vdavinci->vdev.cache_lock);
    if (vdavinci->vdev.domain == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vnpu domain is null\n");
        goto out;
    }
    ret = hw_vdavinci_dma_pool_init_locked(vdavinci);

out:
    mutex_unlock(&vdavinci->vdev.cache_lock);
    return ret;
}

STATIC void get_ram_range_by_gfn(struct vm_dom_info *vm_dom,
                                 unsigned long gfn, unsigned long size,
                                 struct ram_range_info **ram_info)
{
    struct list_head *pos = NULL, *next = NULL;
    struct ram_range_info *ram_info_temp = NULL;

    *ram_info = NULL;
    list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head)) {
        ram_info_temp = list_entry(pos, struct ram_range_info, list);
        if (gfn >= ram_info_temp->base_gfn &&
            gfn < (ram_info_temp->base_gfn + ram_info_temp->npages) &&
            DIV_ROUND_UP(size, PAGE_SIZE) <= (ram_info_temp->base_gfn +
                ram_info_temp->npages - gfn)) {
            *ram_info = ram_info_temp;
            break;
        }
    }
}

void hw_vdavinci_unplug_ram(struct hw_vdavinci *vdavinci,
                            unsigned long start_gfn, unsigned long size)
{
    struct ram_range_info *ram_info = NULL;
    struct dev_dom_info *dev_dom = NULL;
    struct vm_dom_info *vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;

    down_write(&vm_dom->sem);
    get_ram_range_by_gfn(vm_dom, start_gfn, size, &ram_info);
    if (!ram_info) {
        vascend_warn(vdavinci_to_dev(vdavinci),
                     "ram range has already been unpluged, size %lu\n", size);
        up_write(&vm_dom->sem);
        return;
    }

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    /* first unmap */
    if (dev_dom != NULL) {
        dev_dom->ops.dev_dma_unmap_ram_range(vdavinci, ram_info);
    }
    if (atomic_read(&ram_info->map_count) == 0) {
        hw_vdavinci_unpin_page_range(vdavinci, ram_info);
        list_del(&(ram_info->list));
        kfree(ram_info);
    }

    up_write(&vm_dom->sem);
    vascend_info(vdavinci_to_dev(vdavinci), "unplug ram success");
    return;
}

void hw_vdavinci_put_iova(struct sg_table *dma_sgt)
{
    if (dma_sgt) {
        sg_free_table(dma_sgt);
        kfree(dma_sgt);
    }
}

int hw_vdavinci_get_iova(struct hw_vdavinci *vdavinci,
                         unsigned long gfn, unsigned long size,
                         struct sg_table **dma_sgt)
{
    int ret;
    struct vm_dom_info *vm_dom = NULL;
    struct dev_dom_info *dev_dom = NULL;
    struct device *dev = NULL;

    mutex_lock(&vdavinci->vdev.cache_lock);
    if (!vdavinci->vdev.domain) {
        ret = -EINVAL;
        goto unlock;
    }

    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    down_read(&vm_dom->sem);

    /* unlock to support concurrent read */
    mutex_unlock(&vdavinci->vdev.cache_lock);
    dev = vdavinci_get_device(vdavinci);

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (!dev_dom || dev_dom->status != DOMAIN_MAP_STATUS_READY) {
        vascend_err(dev,
            "dma pool not ready\n");
        ret = -ENODEV;
        goto up_read;
    }

    ret = dev_dom->ops.hw_vdavinci_get_iova_sg(vdavinci, vm_dom, gfn,
                                               size, dma_sgt);
up_read:
    up_read(&vm_dom->sem);
    return ret;
unlock:
    mutex_unlock(&vdavinci->vdev.cache_lock);
    return ret;
}

int hw_vdavinci_get_iova_batch(struct hw_vdavinci *vdavinci,
                               unsigned long *gfn, unsigned long *dma_addr,
                               unsigned long count)
{
    int ret;
    struct dev_dom_info *dev_dom = NULL;
    struct vm_dom_info *vm_dom = NULL;

    mutex_lock(&vdavinci->vdev.cache_lock);
    if (!vdavinci->vdev.domain) {
        ret = -EINVAL;
        goto unlock;
    }

    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    down_read(&vm_dom->sem);

    /* unlock to support concurrent read */
    mutex_unlock(&vdavinci->vdev.cache_lock);

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (!dev_dom || dev_dom->status != DOMAIN_MAP_STATUS_READY) {
        vascend_err(vdavinci_to_dev(vdavinci), "dma pool not ready\n");
        ret = -ENODEV;
        goto up_read;
    }

    ret = dev_dom->ops.hw_vdavinci_get_iova_array(vdavinci, vm_dom,
                                                  gfn, dma_addr, count);
up_read:
    up_read(&vm_dom->sem);
    return ret;
unlock:
    mutex_unlock(&vdavinci->vdev.cache_lock);
    return ret;
}

/**
 * The 2M area can be composed of multiple consecutive sgs, to determine
 * whether the gfn is in these sgs
 */
STATIC bool check_gfn_in_dma_sg(unsigned long gfn,
                                unsigned long sg_gfn_base,
                                unsigned long sg_gfn_len)
{
    if (gfn >= sg_gfn_base && gfn < (sg_gfn_base + sg_gfn_len)) {
        return true;
    }
    return false;
}

STATIC unsigned long get_gfn_in_sg_offset(unsigned long gfn,
                                          unsigned long sg_gfn_base)
{
    return (gfn - sg_gfn_base) * PAGE_SIZE;
}

STATIC void set_gfn_sgl(struct scatterlist *new,
                        struct scatterlist *ogn,
                        unsigned int gfn_sg_offset)
{
    if (new == NULL) {
        return;
    }
    sg_dma_address(new) = sg_dma_address(ogn) + gfn_sg_offset;
    sg_dma_len(new) = sg_dma_len(ogn) - gfn_sg_offset;
}

/**
 * if iova_info->dma_sgt is not null, return sg length and sg table
 * otherwise return sg length
 */
STATIC int hw_vdavinci_get_gfn_sg(struct hw_vdavinci *vdavinci,
                                  struct vdavinci_iova_info *iova_info)
{
    unsigned int i;
    unsigned int sg_len = 0;
    unsigned long sg_gfn_len = 0, dma_length = 0, gfn_sg_offset = 0;
    struct dev_dma_sgt **sgt_array = iova_info->sgt_array;
    unsigned long sgl_gfn_base = (*sgt_array)->gfn;
    struct scatterlist *temp_sgl = NULL, *out_sgl = NULL;
    struct sg_table *sg_table_2m = NULL;

    if (iova_info->dma_sgt != NULL && *iova_info->dma_sgt != NULL) {
        out_sgl = (*iova_info->dma_sgt)->sgl;
    }

    while (dma_length < iova_info->size) {
        sg_table_2m = (*sgt_array)->dma_sgt;

        for_each_sg(sg_table_2m->sgl, temp_sgl, sg_table_2m->nents, i) {
            /* find the first sgl which gfn is in it, gfn_sg_offset is the
               intervel between the start of temp_sgl and gfn,
               so the gfn_sg_offset is caculated for only once */
            gfn_sg_offset = 0;
            if (sg_len == 0) {
                sg_gfn_len = roundup(temp_sgl->length, PAGE_SIZE) / PAGE_SIZE;
                if (!check_gfn_in_dma_sg(iova_info->gfn, sgl_gfn_base, sg_gfn_len)) {
                    /* sgl before the start_sgl, skip */
                    sgl_gfn_base += sg_gfn_len;
                    continue;
                }
                /* the start_sgl */
                gfn_sg_offset = get_gfn_in_sg_offset(iova_info->gfn, sgl_gfn_base);
            }
            sg_len++;

            set_gfn_sgl(out_sgl, temp_sgl, gfn_sg_offset);
            dma_length += (sg_dma_len(temp_sgl) - gfn_sg_offset);
            if (dma_length >= iova_info->size) {
                if (out_sgl != NULL) { /* the last sgl */
                    sg_dma_len(out_sgl) = iova_info->size -
                                          (dma_length - (sg_dma_len(temp_sgl) - gfn_sg_offset));
                }
                break;
            }
            if (out_sgl != NULL) {
                out_sgl = sg_next(out_sgl);
            }
        }
        sgt_array++;
    }
    iova_info->sg_len = sg_len;

    return 0;
}

STATIC struct dev_dma_info *get_dma_info_by_ram(struct ram_range_info *ram_info,
                                                struct hw_vdavinci *vdavinci)
{
    struct dev_dma_info *dma_info = NULL;
    struct list_head *pos = NULL, *next = NULL;

    list_for_each_safe(pos, next, &(vdavinci->vdev.dev_dma_info_list_head)) {
        dma_info = list_entry(pos, struct dev_dma_info, list);
        if (dma_info->ram_info == ram_info) {
            return dma_info;
        }
    }

    return NULL;
}

STATIC int get_iova_sgt_info(struct hw_vdavinci *vdavinci,
                             struct vm_dom_info *vm_dom,
                             struct vdavinci_iova_info *iova_info)
{
    int ret = 0;
    unsigned long array_base = 0;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_info *dma_info = NULL;

    get_ram_range_by_gfn(vm_dom, iova_info->gfn, iova_info->size, &ram_info);
    if (!ram_info) {
        vascend_err(vdavinci_to_dev(vdavinci), "dvt_get_dma_map_page invalid gfn %llx\n",
                    (unsigned long long)iova_info->gfn);
        return -EINVAL;
    }

    dma_info = get_dma_info_by_ram(ram_info, vdavinci);
    if (dma_info == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "get dma info failed\n");
        return -EINVAL;
    }

    /* base should be rounddown */
    array_base = (iova_info->gfn - ram_info->base_gfn) / VFIO_PIN_PAGES_MAX_ENTRIES;
    iova_info->sgt_array = dma_info->sgt_array + array_base;

    ret = hw_vdavinci_get_gfn_sg(vdavinci, iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "get sg length error, ret: %d", ret);
        return ret;
    }
    return 0;
}

STATIC int set_iova_sgt_info(struct hw_vdavinci *vdavinci,
                             struct sg_table **dma_sgt,
                             struct vdavinci_iova_info *iova_info)
{
    int ret;

    *dma_sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
    if (*dma_sgt == NULL) {
        return -ENOMEM;
    }

    ret = sg_alloc_table(*dma_sgt, iova_info->sg_len, GFP_KERNEL);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "sg_alloc_table return error result, ret: %d, sg_len: %u",
                    ret, iova_info->sg_len);
        ret = -ENOMEM;
        goto sgt_free;
    }

    iova_info->dma_sgt = dma_sgt;
    ret = hw_vdavinci_get_gfn_sg(vdavinci, iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "get sg list failed, ret: %d, gfn: 0x%lx, size: 0x%lx",
                    ret, iova_info->gfn, iova_info->size);
        ret = -ENOMEM;
        goto table_free;
    }
    return ret;

table_free:
    sg_free_table(*dma_sgt);
    iova_info->dma_sgt = NULL;
sgt_free:
    kfree(*dma_sgt);
    return ret;
}

int hw_vdavinci_get_iova_sg(struct hw_vdavinci *vdavinci,
                            struct vm_dom_info *vm_dom,
                            unsigned long gfn, unsigned long size,
                            struct sg_table **dma_sgt)
{
    int ret;
    struct vdavinci_iova_info iova_info;

    iova_info.gfn = gfn;
    iova_info.size = size;
    iova_info.dma_sgt = NULL;
    iova_info.sg_len = 0;

    ret = get_iova_sgt_info(vdavinci, vm_dom, &iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "get iova sg table info failed, ret: %d, gfn: 0x%lx, size: 0x%lx",
                    ret, iova_info.gfn, iova_info.size);
        return ret;
    }
    ret = set_iova_sgt_info(vdavinci, dma_sgt, &iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "set iova sg table info failed, ret: %d", ret);
        return ret;
    }

    return ret;
}

STATIC int get_iova_by_sg(struct hw_vdavinci *vdavinci,
                          struct dev_dma_sgt **sgt_array_base,
                          unsigned long gfn, unsigned long *dma_addr)
{
    unsigned int i;
    unsigned long sg_gfn_len;
    unsigned long gfn_sg_offset = 0;
    unsigned long sgl_gfn_base = 0;
    struct scatterlist *temp_sgl = NULL;
    struct sg_table *sg_table_2m = NULL;

    if (!(*sgt_array_base)) {
        return -EINVAL;
    }

    sgl_gfn_base = (*sgt_array_base)->gfn;
    sg_table_2m = (*sgt_array_base)->dma_sgt;

    for_each_sg(sg_table_2m->sgl, temp_sgl, sg_table_2m->nents, i) {
        sg_gfn_len = roundup(temp_sgl->length, PAGE_SIZE) / PAGE_SIZE;
        if (!check_gfn_in_dma_sg(gfn, sgl_gfn_base, sg_gfn_len)) {
            sgl_gfn_base += sg_gfn_len;
            continue;
        }

        gfn_sg_offset = get_gfn_in_sg_offset(gfn, sgl_gfn_base);
        *dma_addr = sg_dma_address(temp_sgl) + gfn_sg_offset;
        return 0;
    }

    vascend_err(vdavinci_to_dev(vdavinci),
                "can not find gfn in sg list, gfn: 0x%lx, the base gfn sgl: 0x%llx",
                gfn, (*sgt_array_base)->gfn);
    return -ENODEV;
}

int hw_vdavinci_get_iova_array(struct hw_vdavinci *vdavinci,
                               struct vm_dom_info *vm_dom,
                               unsigned long *gfn,
                               unsigned long *dma_addr,
                               unsigned long count)
{
    int ret = 0;
    unsigned long index = 0, array_base = 0;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_sgt **sgt_array_base = NULL;
    struct dev_dma_info *dma_info = NULL;

    while (index != count) {
        if (ram_info == NULL ||
            gfn[index] < ram_info->base_gfn ||
            gfn[index] >= ram_info->base_gfn + ram_info->npages) {
            get_ram_range_by_gfn(vm_dom, gfn[index], PAGE_SIZE, &ram_info);
            if (!ram_info) {
                vascend_err(vdavinci_to_dev(vdavinci),
                            "get iova batch failed, invalid gfn %lx\n", gfn[index]);
                return -EINVAL;
            }

            dma_info = get_dma_info_by_ram(ram_info, vdavinci);
            if (dma_info == NULL) {
                return -ENODEV;
            }
        }

        /* base should be rounddown */
        array_base = (gfn[index] - ram_info->base_gfn) / VFIO_PIN_PAGES_MAX_ENTRIES;
        sgt_array_base = dma_info->sgt_array + array_base;

        ret = get_iova_by_sg(vdavinci, sgt_array_base, gfn[index], &dma_addr[index]);
        if (ret) {
            vascend_err(vdavinci_to_dev(vdavinci),
                        "get iova batch failed, invalid gfn %lx\n", gfn[index]);
            return ret;
        }
        index++;
    }

    return ret;
}

int vf_get_iova_sg(struct hw_vdavinci *vdavinci,
                   struct vm_dom_info *vm_dom,
                   unsigned long gfn, unsigned long size,
                   struct sg_table **dma_sgt)
{
    int ret;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_info *dma_info = NULL;

    get_ram_range_by_gfn(vm_dom, gfn, size, &ram_info);
    if (!ram_info) {
        vascend_err(vdavinci_to_dev(vdavinci),
            "get iova failed, invalid gfn %llx\n", (unsigned long long)gfn);
        return -EINVAL;
    }

    *dma_sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
    if (*dma_sgt == NULL) {
        return -ENOMEM;
    }

    ret = sg_alloc_table(*dma_sgt, 1, GFP_KERNEL);
    if (ret) {
        goto free_sgt;
    }

    dma_info = get_dma_info_by_ram(ram_info, vdavinci);
    if (dma_info == NULL) {
        ret = -ENODEV;
        goto free_table;
    }

    sg_dma_address((*dma_sgt)->sgl) = dma_info->base_iova +
                (gfn - ram_info->base_gfn) * PAGE_SIZE;
    sg_dma_len((*dma_sgt)->sgl) = size;

    return 0;

free_table:
    sg_free_table(*dma_sgt);
free_sgt:
    kfree(*dma_sgt);
    *dma_sgt = NULL;

    return ret;
}

int vf_get_iova_array(struct hw_vdavinci *vdavinci,
                      struct vm_dom_info *vm_dom,
                      unsigned long *gfn, unsigned long *dma_addr,
                      unsigned long count)
{
    unsigned long index = 0;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_info *dma_info = NULL;

    while (index != count) {
        if (ram_info == NULL ||
            gfn[index] < ram_info->base_gfn ||
            gfn[index] >= ram_info->base_gfn + ram_info->npages) {
            get_ram_range_by_gfn(vm_dom, gfn[index], PAGE_SIZE, &ram_info);
            if (!ram_info) {
                vascend_err(vdavinci_to_dev(vdavinci),
                    "get iova array failed, invalid gfn %lx\n", gfn[index]);
                return -EINVAL;
            }

            dma_info = get_dma_info_by_ram(ram_info, vdavinci);
            if (dma_info == NULL) {
                return -ENODEV;
            }
        }

        dma_addr[index] = dma_info->base_iova +
            (gfn[index] - ram_info->base_gfn) * PAGE_SIZE;
        index++;
    }

    return 0;
}

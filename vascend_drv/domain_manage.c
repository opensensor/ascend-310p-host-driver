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
#include <linux/kvm_host.h>
#include <linux/vmalloc.h>
#include "dvt.h"
#include "dma_pool.h"
#include "domain_manage.h"
#include "vfio_ops.h"

LIST_HEAD(g_vm_domains);
DEFINE_MUTEX(g_vm_domains_lock);

struct list_head *get_vm_domains_list(void)
{
    return &g_vm_domains;
}

struct mutex *get_vm_domains_lock(void)
{
    return &g_vm_domains_lock;
}

void dev_dom_release(struct kref *ref)
{
    struct dev_dom_info *dev_dom = container_of(ref, typeof(*dev_dom), ref);
    struct ram_range_info *ram_info = NULL;

    list_del(&dev_dom->list);

    list_for_each_entry(ram_info, &dev_dom->vm_dom->ram_info_list->head, list) {
        dev_dom->ops.dev_dma_unmap_ram_range(dev_dom->vdavinci, ram_info);
    }
    kfree(dev_dom);
}

struct dev_dom_info *dev_dom_info_find(struct vm_dom_info *vm_dom,
                                       struct hw_vdavinci *vdavinci)
{
    struct dev_dom_info *dev_dom = NULL;

    list_for_each_entry(dev_dom, &vm_dom->dev_dom_list_head, list) {
        if (dev_dom->vdavinci == vdavinci) {
            return dev_dom;
        }
    }

    return NULL;
}

struct dev_dom_info *dev_dom_info_new(struct vm_dom_info *vm_dom,
                                      struct hw_vdavinci *vdavinci)
{
    struct dev_dom_info *dev_dom =
        (struct dev_dom_info *)kzalloc(sizeof(struct dev_dom_info), GFP_KERNEL);
    if (!dev_dom) {
        return NULL;
    }

    dev_dom->vm_dom = vm_dom;
    dev_dom->vdavinci = vdavinci;
    dev_dom->status = DOMAIN_MAP_STATUS_INVALID;
    kref_init(&dev_dom->ref);
    list_add_tail(&(dev_dom->list), &(vm_dom->dev_dom_list_head));

    return dev_dom;
}

/* hold vm_dom_info->rw_semaphore before call this function */
struct dev_dom_info *dev_dom_info_get(struct vm_dom_info *vm_dom,
                                      struct hw_vdavinci *vdavinci)
{
    struct dev_dom_info *dev_dom = NULL;

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (dev_dom) {
        kref_get(&dev_dom->ref);
        return dev_dom;
    }

    return dev_dom_info_new(vm_dom, vdavinci);
}

/* hold vm_dom_info->rw_semaphore before call this function */
void dev_dom_info_put(struct dev_dom_info *dev_dom,
                      struct hw_vdavinci *vdavinci)
{
    if (kref_put(&dev_dom->ref, dev_dom_release)) {
        if (vdavinci->is_passthrough) {
            hw_vdavinci_iommu_detach_group(vdavinci);
        }
    }
}

/* hold dma_domains_lock before call this function */
void vm_dom_info_release(struct kref *ref)
{
    struct vm_dom_info *vm_dom = container_of(ref, typeof(*vm_dom), ref);

    put_iova_domain(&vm_dom->iovad);

    list_del(&vm_dom->node);
    kfree(vm_dom->ram_info_list);
    kfree(vm_dom);
}

struct vm_dom_info *vm_dom_info_find(const struct kvm *kvm)
{
    struct vm_dom_info *vm_dom = NULL;

    list_for_each_entry(vm_dom, &g_vm_domains, node) {
        if (vm_dom->kvm == kvm) {
            return vm_dom;
        }
    }

    return NULL;
}

struct vm_dom_info *vm_dom_info_new(struct kvm *kvm)
{
    struct vm_dom_info *vm_dom =
        (struct vm_dom_info *)kzalloc(sizeof(struct vm_dom_info), GFP_KERNEL);
    if (!vm_dom) {
        return NULL;
    }

    vm_dom->ram_info_list = (struct ram_range_info_list *)
        kzalloc(sizeof(struct ram_range_info_list), GFP_KERNEL);
    if (!vm_dom->ram_info_list) {
        kfree(vm_dom);
        return NULL;
    }

    vm_dom->kvm = kvm;
    vm_dom->status = DOMAIN_PIN_STATUS_INVALID;
    init_rwsem(&vm_dom->sem);
    kref_init(&vm_dom->ref);
    INIT_LIST_HEAD(&(vm_dom->ram_info_list->head));
    INIT_LIST_HEAD(&(vm_dom->dev_dom_list_head));
    list_add_tail(&(vm_dom->node), &g_vm_domains);

    vdavinci_init_iova_domain(&vm_dom->iovad);

    return vm_dom;
}

struct vm_dom_info *vm_dom_info_get(struct kvm *kvm)
{
    struct vm_dom_info *vm_dom = NULL;

    vm_dom = vm_dom_info_find(kvm);
    if (vm_dom) {
        kref_get(&vm_dom->ref);
        return vm_dom;
    }

    return vm_dom_info_new(kvm);
}

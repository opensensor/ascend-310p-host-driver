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

#ifndef _DVT_DOMAIN_MANAGE_H_
#define _DVT_DOMAIN_MANAGE_H_

#include <linux/iova.h>
#include "dma_pool_map.h"

#define DOMAIN_PIN_STATUS_INVALID    0x0
#define DOMAIN_PIN_STATUS_READY        0x1
#define DOMAIN_MAP_STATUS_INVALID    0x2
#define DOMAIN_MAP_STATUS_READY        0x4
#define DOMAIN_DMA_BIT_MASK_32       32

struct dev_dom_info {
    struct hw_vdavinci *vdavinci;
    int status;
    bool is_passthrough;
    struct kref ref;
    struct vm_dom_info *vm_dom;
    struct list_head list;
    struct {
        int (*dev_dma_map_ram_range)(struct hw_vdavinci *vdavinci,
                                     struct ram_range_info *ram_info);
        void (*dev_dma_unmap_ram_range)(struct hw_vdavinci *vdavinci,
                                        struct ram_range_info *ram_info);
        int (*hw_vdavinci_get_iova_sg)(struct hw_vdavinci *vdavinci,
                                       struct vm_dom_info *vm_dom,
                                       unsigned long gfn, unsigned long size,
                                       struct sg_table **dma_sgt);
        int (*hw_vdavinci_get_iova_array)(struct hw_vdavinci *vdavinci,
                                          struct vm_dom_info *vm_dom,
                                          unsigned long *gfn,
                                          unsigned long *dma_addr,
                                          unsigned long count);
    } ops;
};

struct reserve_mem {
    struct list_head node;
    unsigned long pfn_lo;
    unsigned long pfn_hi;
};

struct vm_dom_info {
    struct list_head node;
    struct kvm *kvm;
    int status;
    struct rw_semaphore sem;
    struct kref ref;
    struct list_head dev_dom_list_head;
    struct ram_range_info_list *ram_info_list;
    struct iova_domain iovad;
};

struct list_head *get_vm_domains_list(void);
struct mutex *get_vm_domains_lock(void);

void dev_dom_release(struct kref *ref);
struct dev_dom_info *dev_dom_info_find(struct vm_dom_info *vm_dom,
                                       struct hw_vdavinci *vdavinci);
struct dev_dom_info *dev_dom_info_new(struct vm_dom_info *vm_dom,
                                      struct hw_vdavinci *vdavinci);
struct dev_dom_info *dev_dom_info_get(struct vm_dom_info *vm_dom,
                                      struct hw_vdavinci *vdavinci);
void dev_dom_info_put(struct dev_dom_info *dev_dom,
                      struct hw_vdavinci *vdavinci);
int dma_domain_add_reserve_mem(struct vm_dom_info *vm_dom,
                               unsigned long pfn_lo, unsigned long pfn_hi);
void vm_dom_info_release(struct kref *ref);
struct vm_dom_info *vm_dom_info_find(const struct kvm *kvm);
struct vm_dom_info *vm_dom_info_new(struct kvm *kvm);
struct vm_dom_info *vm_dom_info_get(struct kvm *kvm);

#endif

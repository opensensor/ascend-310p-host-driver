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

#ifndef _VFIO_OPS_H_
#define _VFIO_OPS_H_

#include "dvt.h"
#include "kvmdt.h"

struct vdavinci_pin_info {
    unsigned long gfn;
    int npage;
    struct page **pages;
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0))
#define IS_VDAVINCI_KERNEL_VERSION_SUPPORT      1
#else
#define IS_VDAVINCI_KERNEL_VERSION_SUPPORT      0
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0))
#define vdavinci_for_each_memslot(slot, slots, iter)  \
    kvm_for_each_memslot(slot, iter, slots)
#else
#define vdavinci_for_each_memslot(slot, slots, iter)  \
    for ((iter) = 0; (iter) < (slots)->used_slots && ((slot) = &(slots)->memslots[(iter)]); (iter)++)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0))
#define DMA_MAPPING_ERROR       (~(dma_addr_t)0)
#endif

extern struct mdev_driver hw_vdavinci_mdev_driver;

struct device *get_mdev_parent(struct mdev_device *mdev);
bool device_is_mdev(struct device *dev);
struct mdev_device *get_mdev_device(struct device *dev);
void *get_mdev_drvdata(struct device *dev);
void set_mdev_drvdata(struct device *dev, void *data);
void vdavinci_iommu_unmap(struct device *dev, unsigned long iova, size_t size);
int vdavinci_iommu_map(struct device *dev, unsigned long iova,
                       phys_addr_t paddr, size_t size, int prot);
void vdavinci_unpin_pages(struct hw_vdavinci *vdavinci, struct vdavinci_pin_info *pin_info);
int vdavinci_pin_pages(struct hw_vdavinci *vdavinci, struct vdavinci_pin_info *pin_info);
int vdavinci_register_device(struct device *dev,
                             struct hw_dvt *dvt,
                             const char *name);
void vdavinci_unregister_device(struct device *dev, struct hw_dvt *dvt);
int vdavinci_register_driver(struct mdev_driver *drv);
void vdavinci_unregister_driver(struct mdev_driver *drv);
void vdavinci_init_iova_domain(struct iova_domain *iovad);
bool is_dev_dma_coherent(struct device *dev);
uuid_le vdavinci_get_uuid(struct mdev_device *mdev);
void vdavinci_use_mm(struct mm_struct *mm);
void vdavinci_unuse_mm(struct mm_struct *mm);
int vdavinci_register_vfio_group(struct hw_vdavinci *vdavinci);
void vdavinci_unregister_vfio_group(struct hw_vdavinci *vdavinci);
__u64 vdavinci_eventfd_signal(struct eventfd_ctx *ctx, __u64 n);
bool vdavinci_refcount_mutex_lock(refcount_t *r, struct mutex *lock);
struct cpumask *vdavinci_get_cpumask(struct task_struct *task);
int vdavinci_rw_gpa(struct kvmdt_guest_info *info, unsigned long gpa,
                    void *buf, unsigned long len, bool write);
void vdavinci_copy_reserved_iova(struct iova_domain *from, struct iova_domain *to);
#endif

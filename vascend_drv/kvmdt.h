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

#ifndef _KVMDT_H_
#define _KVMDT_H_

#include "dvt.h"
/*
 * Specific MPT modules function collections.
 */
struct hw_kvmdt_ops {
    int (*register_mdev)(struct device *dev, struct hw_dvt *dvt);
    void (*unregister_mdev)(struct device *dev, struct hw_dvt *dvt);
    int (*attach_vdavinci)(void *vdavinci, uintptr_t handle);
    void (*detach_vdavinci)(void *vdavinci);
    int (*inject_msix)(uintptr_t handle, u32 vector);
    unsigned long (*from_virt_to_mfn)(void *addr);
    int (*read_gpa)(uintptr_t handle, unsigned long gpa, void *buf,
                    unsigned long len);
    int (*write_gpa)(uintptr_t handle, unsigned long gpa, void *buf,
                     unsigned long len);
    unsigned long (*gfn_to_mfn)(uintptr_t handle, unsigned long gfn);
    bool (*is_valid_gfn)(uintptr_t handle, unsigned long gfn);
    int  (*mmio_get)(void **dst, int *size, void *_vdavinci, int bar);
    int (*dma_pool_init)(struct hw_vdavinci *vdavinci);
    void (*dma_pool_uninit)(struct hw_vdavinci *vdavinci);
    int (*dma_get_iova)(struct hw_vdavinci *vdavinci, unsigned long gfn,
                        unsigned long size, struct sg_table **dma_sgt);
    void (*dma_put_iova)(struct sg_table *dma_sgt);
    int (*dma_get_iova_batch)(struct hw_vdavinci *vdavinci, unsigned long *gfn,
                              unsigned long *dma_addr, unsigned long count);
};

struct kvmdt_guest_info {
    struct kvm *kvm;
    struct hw_vdavinci *vdavinci;
    struct dentry *debugfs_cache_entries;
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
struct hw_vfio_vdavinci {
    struct vfio_device vfio_dev;
    struct hw_vdavinci *vdavinci;
};
#endif

struct dvt_dma {
    struct hw_vdavinci *vdavinci;
    struct rb_node gfn_node;
    struct rb_node dma_node;
    gfn_t gfn;
    dma_addr_t dma_addr;
    unsigned long size;
    struct sg_table *dma_sgt;
    struct page_info_list *dma_page_list;
    struct kref ref;
};

void kvmdt_dma_unmap_guest_page(uintptr_t handle, struct sg_table *dma_sgt);
int kvmdt_dma_map_guest_page(uintptr_t handle, unsigned long gfn,
                             unsigned long size, struct sg_table **dma_sgt);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
void dvt_cache_remove_ram(struct hw_vdavinci *vdavinci,
                          unsigned long start_gfn, unsigned long size);
#endif

#if ((LINUX_VERSION_CODE == KERNEL_VERSION(4,4,0)) && (!defined(DRV_UT)))
static inline bool mmget_not_zero(struct mm_struct *mm)
{
    return atomic_inc_not_zero(&mm->mm_users);
}
#endif

bool get_node_cpu_by_page(struct hw_vdavinci *vdavinci,
                          unsigned int current_cpu,
                          struct page *page,
                          struct cpumask *cpumask);
struct hw_vdavinci *hw_vdavinci_create(struct kobject *kobj, struct mdev_device *mdev);
int hw_vdavinci_remove(struct mdev_device *mdev);
int hw_vdavinci_open(struct mdev_device *mdev);
void hw_vdavinci_release(struct mdev_device *mdev);
ssize_t hw_vdavinci_read(struct mdev_device *mdev, char __user *buf,
                         size_t count, loff_t *ppos);
size_t hw_vdavinci_write(struct mdev_device *mdev,
                         const char __user *buf,
                         size_t count, loff_t *ppos);
int hw_vdavinci_mmap(struct mdev_device *mdev, struct vm_area_struct *vma);
long hw_vdavinci_ioctl(struct mdev_device *mdev, unsigned int cmd,
                       unsigned long arg);
#endif

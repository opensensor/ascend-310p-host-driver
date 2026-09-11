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
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/mmu_context.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/uuid.h>

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)) || (defined(DRV_UT)))
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)) || (defined(DRV_UT)))
#include <linux/sched/mm.h>
#endif
#include <linux/kvm_host.h>
#include <linux/vfio.h>
#endif

#include <linux/kvm_host.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sched/signal.h>

#include "dma_pool.h"
#include "guest_memory.h"
#include "guest_slots.h"
#include "vfio_ops.h"
#include "dvt_sysfs.h"
#include "dvt.h"
#include "kvmdt.h"

#define HW_VDAVINCI_READ_SUPPORT_TYPES 3
#define HW_VDAVINCI_WRITE_SUPPORT_TYPES 2

enum {
    IO_REGION_INDEX,
    MEM_REGION_INDEX,
    NUM_REGION_INDEX
};

static unsigned int g_vdavinci_rw_support_size[] = {1, 2, 4, 8};

STATIC void kvmdt_guest_exit(struct kvmdt_guest_info *info);
STATIC void hw_vdavinci_release_work(struct work_struct *work);
STATIC int kvmdt_guest_init(struct mdev_device *mdev);
STATIC int hw_vdavinci_get_irq_count(struct hw_vdavinci *vdavinci, unsigned int type);

bool handle_valid(uintptr_t handle)
{
    return !!(((unsigned long)handle) & ~0xff);
}

struct vdavinci_priv *kdev_to_davinci(struct device *kdev)
{
    struct pci_dev *pdev;
    pdev = container_of(kdev, struct pci_dev, dev);
    return (struct vdavinci_priv *)pci_get_drvdata(pdev);
}

STATIC struct hw_vdavinci_type *
hw_vdavinci_find_available_type(struct device *dev, const char *name)
{
    struct hw_vdavinci_type *type = NULL;
    struct hw_dvt *dvt = kdev_to_davinci(dev)->dvt;

    type = g_hw_vdavinci_ops.dvt_find_vdavinci_type(dvt, name);
    if (type == NULL) {
        vascend_err(dev, "failed to find type: %s to create\n", name);
        return NULL;
    }

    if (type->avail_instance == 0) {
        vascend_err(dev, "insufficient instance of type: %s\n", name);
        return NULL;
    }

    return type;
}

/**
 * check if the driver is in vm mode
 */
STATIC bool hw_vdavinci_check_is_vm_mode(void)
{
    int ret;
    int mode;

    ret = hw_dvt_get_mode(&mode);
    if (ret != 0) {
        pr_err("hw_dvt_get_mode fail, ret: %d\n", ret);
        return false;
    }

    if (mode == VDAVINCI_VM) {
        return true;
    }

    return false;
}

bool hw_vdavinci_is_enabled(struct hw_dvt *dvt)
{
    if (!hw_vdavinci_check_is_vm_mode()) {
        return false;
    }

    if (hw_vdavinci_sriov_support(dvt) && !dvt->is_sriov_enabled) {
        return false;
    }

    return true;
}

bool hw_vdavinci_priv_callback_check(struct vdavinci_priv *vdavinci_priv)
{
    struct vdavinci_priv_ops *ops = NULL;
 
    if (vdavinci_priv == NULL) {
        return false;
    }
    if (vdavinci_priv->ops == NULL || vdavinci_priv->dev == NULL) {
        return false;
    }
    ops = vdavinci_priv->ops;
    if (ops->vdavinci_create == NULL ||
        ops->vdavinci_destroy == NULL ||
        ops->vdavinci_release == NULL ||
        ops->vdavinci_reset == NULL ||
        ops->vdavinci_notify == NULL ||
        ops->davinci_getdevnum == NULL ||
        ops->davinci_getdevinfo == NULL) {
        return false;
    }
 
    return true;
}

struct hw_vdavinci *hw_vdavinci_create(struct kobject *kobj, struct mdev_device *mdev)
{
    int ret = 0;
    struct hw_vdavinci *vdavinci = NULL;
    struct hw_vdavinci_type *type = NULL;
    struct device *pdev;
    struct hw_dvt *dvt;
    struct hw_pf_info *pf_info;
    uuid_le uuid = vdavinci_get_uuid(mdev);

    pdev = get_mdev_parent(mdev);
    dvt = kdev_to_davinci(pdev)->dvt;
    if (!hw_vdavinci_is_enabled(dvt)) {
        vascend_err(pdev, "driver is not in vm mode or device's sriov is not enabled\n");
        return NULL;
    }

    mutex_lock(&dvt->lock);
    type = hw_vdavinci_find_available_type(pdev, kobject_name(kobj));
    if (type == NULL) {
        goto unlock;
    }

    vascend_info(pdev, "enter create vdavinci, type: %s\n", kobject_name(kobj));
    vdavinci = g_hw_vdavinci_ops.vdavinci_create(dvt, type, uuid);
    if (IS_ERR_OR_NULL(vdavinci)) {
        ret = vdavinci == NULL ? -EFAULT : PTR_ERR(vdavinci);
        vascend_err(pdev, "failed to create vdavinci: %d\n", ret);
        goto unlock;
    }

    type->vf_num++;

    pf_info = &dvt->pf[vdavinci->dev.dev_index];

    pf_info->reserved_aicpu_num = hw_dvt_get_used_aicpu_num(dvt, pf_info->dev_index);

    pf_info->reserved_aicore_num -= type->aicore_num;
    pf_info->reserved_jpegd_num -= type->jpegd_num;
    pf_info->reserved_mem_size -= type->mem_size;
    pf_info->instance_num++;
    hw_dvt_update_vdavinci_types(dvt, pf_info->dev_index);
    mutex_unlock(&dvt->lock);

    /* VFIO may invalidate DMA before open_device and after close_device. */
    mutex_init(&vdavinci->vdev.cache_lock);
    INIT_WORK(&vdavinci->vdev.release_work, hw_vdavinci_release_work);

    vdavinci->vfg_id = type->vfg_id;
    vdavinci->vdev.mdev = mdev;
    set_mdev_drvdata(mdev_dev(mdev), vdavinci);
    vascend_info(pdev, "leave create vdavinci, vid: %u\n", vdavinci->id);
    return vdavinci;

unlock:
    mutex_unlock(&dvt->lock);
    return NULL;
}

int hw_vdavinci_remove(struct mdev_device *mdev)
{
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));
    struct hw_dvt *dvt = vdavinci->dvt;
    struct hw_vdavinci_type *type = vdavinci->type;
    u32 vid = vdavinci->id;
    struct device *dev = vdavinci->dvt->vdavinci_priv->dev;
    struct hw_pf_info *pf_info = &dvt->pf[vdavinci->dev.dev_index];

    vascend_info(dev, "enter remove vdavinci, vid: %u\n", vid);
    if (handle_valid(vdavinci->handle)) {
        return -EBUSY;
    }

    mutex_lock(&dvt->lock);
    mutex_destroy(&vdavinci->vdev.cache_lock);
    g_hw_vdavinci_ops.vdavinci_destroy(vdavinci);

    type->vf_num--;
    pf_info->reserved_aicpu_num = hw_dvt_get_used_aicpu_num(dvt, pf_info->dev_index);

    pf_info->reserved_aicore_num += type->aicore_num;
    pf_info->reserved_jpegd_num += type->jpegd_num;
    pf_info->reserved_mem_size += type->mem_size;
    pf_info->instance_num--;
    hw_dvt_update_vdavinci_types(dvt, pf_info->dev_index);
    mutex_unlock(&dvt->lock);
    vascend_info(dev, "leave remove vdavinci, vid: %u\n", vid);

    return 0;
}

int hw_vdavinci_open(struct mdev_device *mdev)
{
    int ret = 0;
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));

    vascend_info(vdavinci_to_dev(vdavinci), "enter open vdavinci, vid: %u\n", vdavinci->id);

    if (!hw_vdavinci_priv_callback_check(vdavinci->dvt->vdavinci_priv)) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci's priv callback is null\n");
        return -EINVAL;
    }

    vdavinci->qemu_task = current->group_leader;
    cpumask_clear(&vdavinci->vm_cpus_mask);

    ret = vdavinci_register_vfio_group(vdavinci);
    if (ret != 0) {
        return ret;
    }
    if (!try_module_get(THIS_MODULE)) {
        ret = -EBUSY;
        goto undo_group;
    }

    ret = kvmdt_guest_init(mdev);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "kvmdt_guest_init failed, "
                    "vid: %u, ret: %d\n", vdavinci->id, ret);
        goto out_module;
    }

    g_hw_vdavinci_ops.vdavinci_activate(vdavinci);

    atomic_set(&vdavinci->vdev.released, 0);
    vascend_info(vdavinci_to_dev(vdavinci), "leave open vdavinci, vid: %u\n",
        vdavinci->id);
    return ret;

out_module:
    module_put(THIS_MODULE);
undo_group:
    vdavinci_unregister_vfio_group(vdavinci);
    return ret;
}

STATIC void hw_vdavinci_release_msix_eventfd_ctx(struct hw_vdavinci *vdavinci)
{
    int i, nvec;
    struct eventfd_ctx *trigger = NULL;

    if (!vdavinci->vdev.msix_triggers) {
        return;
    }

    nvec = hw_vdavinci_get_irq_count(vdavinci, VFIO_PCI_MSIX_IRQ_INDEX);

    for (i = 0; i < nvec; i++) {
        trigger = vdavinci->vdev.msix_triggers[i];
        if (trigger != NULL) {
            eventfd_ctx_put(trigger);
            vdavinci->vdev.msix_triggers[i] = NULL;
        }
    }

    kfree(vdavinci->vdev.msix_triggers);
    vdavinci->vdev.msix_triggers = NULL;
}

STATIC void __hw_vdavinci_release(struct hw_vdavinci *vdavinci)
{
    struct kvmdt_guest_info *info = NULL;
    struct vdavinci_ioeventfd *ioeventfd = NULL;
    struct vdavinci_ioeventfd *ioeventfd_tmp = NULL;

    vascend_info(vdavinci_to_dev(vdavinci), "enter release vdavinci, vid: %u\n",
                 vdavinci->id);

    if (!handle_valid(vdavinci->handle)) {
        return;
    }

    if (atomic_cmpxchg(&vdavinci->vdev.released, 0, 1)) {
        return;
    }

    g_hw_vdavinci_ops.vdavinci_release(vdavinci);

    vdavinci_unregister_vfio_group(vdavinci);

    module_put(THIS_MODULE);

    info  = (struct kvmdt_guest_info *)vdavinci->handle;
    kvmdt_guest_exit(info);

    hw_vdavinci_release_msix_eventfd_ctx(vdavinci);

    vdavinci->vdev.kvm = NULL;
    vdavinci->handle = 0;

    list_for_each_entry_safe(ioeventfd, ioeventfd_tmp, &vdavinci->ioeventfds_list, next) {
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
        hw_vdavinci_ioeventfd_deactive(vdavinci, ioeventfd);
#endif
    }

    vascend_info(vdavinci_to_dev(vdavinci), "leave release vdavinci, vid: %u\n",
                 vdavinci->id);
}

void hw_vdavinci_release(struct mdev_device *mdev)
{
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));

    __hw_vdavinci_release(vdavinci);
}

STATIC void hw_vdavinci_release_work(struct work_struct *work)
{
    struct hw_vdavinci *vdavinci = container_of(work, struct hw_vdavinci,
                    vdev.release_work);

    __hw_vdavinci_release(vdavinci);
}

ssize_t hw_vdavinci_read(struct mdev_device *mdev, char __user *buf,
                         size_t count, loff_t *ppos)
{
    int i = 0;
    unsigned int done = 0;
    unsigned int size = 0;
    size_t filled;
    u64 val;
    char *pos;
    size_t count_left;
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));

    if (!buf || !ppos) {
        return -EINVAL;
    }

    pos = buf;
    count_left = count;

    while (count_left > 0) {
        filled = 0;
        val = 0;

        for (i = HW_VDAVINCI_READ_SUPPORT_TYPES; i >= 0; i--) {
            size = g_vdavinci_rw_support_size[i];
            if (count_left >= size && (*ppos % size) == 0) {
                if (hw_vdavinci_rw(vdavinci, (char *)&val, size, ppos, false) < 0) {
                    return -EFAULT;
                }

                if (copy_to_user(pos, &val, size) != 0) {
                    return -EFAULT;
                }

                filled = size;
                break;
            }
        }

        count_left -= filled;
        done += filled;
        *ppos += filled;
        pos += filled;
    }

    return done;
}

size_t hw_vdavinci_write(struct mdev_device *mdev,
                         const char __user *buf,
                         size_t count, loff_t *ppos)
{
    int i = 0;
    unsigned int done = 0;
    unsigned int size = 0;
    size_t filled;
    u64 val;
    const char *pos;
    size_t count_left;
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));

    if (!buf || !ppos) {
        return -EINVAL;
    }
    pos = buf;
    count_left = count;

    while (count_left > 0) {
        filled = 0;
        val = 0;

        for (i = HW_VDAVINCI_WRITE_SUPPORT_TYPES; i >= 0; i--) {
            size = g_vdavinci_rw_support_size[i];
            if (count_left >= size && (*ppos % size) == 0) {
                if (copy_from_user(&val, pos, size) != 0) {
                    return -EFAULT;
                }

                if (hw_vdavinci_rw(vdavinci, (char *)&val, size, ppos, true) <= 0) {
                    return -EFAULT;
                }

                filled = size;
                break;
            }
        }

        count_left -= filled;
        done += filled;
        *ppos += filled;
        pos += filled;
    }

    return done;
}

STATIC struct vdavinci_bar_map *
hw_vdavinci_find_bar_map(struct vdavinci_mapinfo *mmio_map_info,
                         unsigned long offset)
{
    u64 i = 0;
    struct vdavinci_bar_map *map = NULL;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->offset <= offset && offset < map->offset + map->size) {
            return map;
        }
    }

    return NULL;
}

STATIC int hw_vdavinci_mmap_check(const struct vm_area_struct *vma)
{
    unsigned long index;

    index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT);
    if (index >= VFIO_PCI_ROM_REGION_INDEX) {
        return -EINVAL;
    }

    if (vma->vm_end < vma->vm_start) {
        return -EINVAL;
    }

    if ((vma->vm_flags & VM_SHARED) == 0) {
        return -EINVAL;
    }

    return 0;
}

STATIC struct vdavinci_mapinfo *hw_vdavinci_get_bar_sparse(struct hw_vdavinci *vdavinci, unsigned long index)
{
    switch (index) {
        case VFIO_PCI_BAR0_REGION_INDEX:
            return &vdavinci->mmio.bar0_sparse;
        case VFIO_PCI_BAR2_REGION_INDEX:
            return &vdavinci->mmio.bar2_sparse;
        case VFIO_PCI_BAR4_REGION_INDEX:
            return &vdavinci->mmio.bar4_sparse;
        default:
            return NULL;
    }
}

int hw_vdavinci_mmap(struct mdev_device *mdev, struct vm_area_struct *vma)
{
    int ret;
    unsigned long pgoff = 0;
    unsigned long index;
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));
    struct vdavinci_bar_map *map;
    struct vdavinci_mapinfo *mmio_map_info = NULL;

    ret = hw_vdavinci_mmap_check(vma);
    if (ret) {
        return ret;
    }

    index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT);

    pgoff = vma->vm_pgoff &
        ((1U << (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT)) - 1);

    mmio_map_info = hw_vdavinci_get_bar_sparse(vdavinci, index);
    if (!mmio_map_info) {
        return -EINVAL;
    }

    map = hw_vdavinci_find_bar_map(mmio_map_info, pgoff << PAGE_SHIFT);
    if (!map) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "find no bar map for pgoff:0x%lx\n", pgoff);
        return -EINVAL;
    }

    if (map->size == 0 || vma->vm_end - vma->vm_start != map->size) {
        vascend_err(vdavinci_to_dev(vdavinci), "mmap unreasonable length\n");
        return -EINVAL;
    }
    if (map->map_type == MAP_TYPE_BACKEND) {
        ret = remap_vmalloc_range(vma, map->vaddr, 0);
    } else if (map->map_type == MAP_TYPE_PASSTHROUGH) {
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
        vma->vm_pgoff = map->paddr >> PAGE_SHIFT;

        ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
                              vma->vm_end - vma->vm_start, vma->vm_page_prot);
    } else {
        ret = -EINVAL;
    }
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "vdavinci mmap error, index: %lu, map_type: %d, ret: %d",
                    index, map->map_type, ret);
    }
    return ret;
}

STATIC int hw_vdavinci_get_irq_count(struct hw_vdavinci *vdavinci, unsigned int type)
{
    u16 flags;
    const unsigned int byte_count = 2;

    if (type == VFIO_PCI_INTX_IRQ_INDEX) {
        return 1;
    } else if (type == VFIO_PCI_MSIX_IRQ_INDEX) {
        g_hw_vdavinci_ops.emulate_cfg_read(vdavinci, DAVINCI_PCI_MSIX_FLAGS,
                                           &flags, byte_count);
        return (flags & PCI_MSIX_FLAGS_QSIZE) + 1;
    } else if (type == VFIO_PCI_MSI_IRQ_INDEX ||
               type == VFIO_PCI_ERR_IRQ_INDEX ||
               type == VFIO_PCI_REQ_IRQ_INDEX) {
        return 0;
    }

    return 0;
}

STATIC int hw_vdavinci_set_intx_mask(struct hw_vdavinci *vdavinci,
                                     const struct vfio_irq_set *hdr,
                                     void *data)
{
    return 0;
}

STATIC int hw_vdavinci_set_intx_unmask(struct hw_vdavinci *vdavinci,
                                       const struct vfio_irq_set *hdr,
                                       void *data)
{
    return 0;
}

STATIC int hw_vdavinci_set_intx_trigger(struct hw_vdavinci *vdavinci,
                                        const struct vfio_irq_set *hdr,
                                        void *data)
{
    return 0;
}

static void hw_vdavinci_put_msix_trigger(struct hw_vdavinci *vdavinci,
                                         unsigned int end, unsigned int start)
{
    struct eventfd_ctx *trigger = NULL;
    unsigned int i;

    for (i = start; i < end; i++) {
        trigger = vdavinci->vdev.msix_triggers[i];
        if (trigger != NULL) {
            eventfd_ctx_put(trigger);
            vdavinci->vdev.msix_triggers[i] = NULL;
        }
    }
}

STATIC int hw_vdavinci_check_msix(struct hw_vdavinci *vdavinci,
    unsigned int start, unsigned int count)
{
    unsigned int nnvec;
    int nvec;

    nvec = hw_vdavinci_get_irq_count(vdavinci, VFIO_PCI_MSIX_IRQ_INDEX);
    nnvec = (unsigned int)nvec;

    if (!vdavinci->vdev.msix_triggers) {
        vdavinci->vdev.msix_triggers = kcalloc(nvec, sizeof(struct eventfd_ctx*), GFP_KERNEL);
        if (!vdavinci->vdev.msix_triggers) {
            return -ENOMEM;
        }
    }

    if (!vdavinci->debugfs.msix_count) {
        vdavinci->debugfs.msix_count = kcalloc(nvec, sizeof(unsigned long long), GFP_KERNEL);
        if (!vdavinci->debugfs.msix_count) {
            kfree(vdavinci->vdev.msix_triggers);
            vdavinci->vdev.msix_triggers = NULL;
            return -ENOMEM;
        }
        vdavinci->debugfs.nvec = nvec;
    }

    if (start >= nnvec || start + count > nnvec) {
        return -EINVAL;
    }

    return 0;
}

STATIC int hw_vdavinci_set_msix_trigger(struct hw_vdavinci *vdavinci,
                                        const struct vfio_irq_set *hdr,
                                        void *data)
{
    unsigned int i, j;
    int fd, ret;
    unsigned int start = hdr->start;
    unsigned int count = hdr->count;
    u32 flags = hdr->flags;

    ret = hw_vdavinci_check_msix(vdavinci, start, count);
    if (ret) {
        return ret;
    }

    if ((flags & VFIO_IRQ_SET_DATA_EVENTFD) != 0) {
        u_int32_t *fds = data;
        struct eventfd_ctx *trigger = NULL;

        for (i = start, j = 0; i < start + count; i++, j++) {
            if (vdavinci->vdev.msix_triggers[i]) {
                eventfd_ctx_put(vdavinci->vdev.msix_triggers[i]);
                vdavinci->vdev.msix_triggers[i] = NULL;
            }

            fd = fds ? (int)fds[j] : -1;
            if (fd < 0) {
                continue;
            }

            trigger = eventfd_ctx_fdget(fd);
            if (IS_ERR(trigger)) {
                vascend_err(vdavinci_to_dev(vdavinci), "eventfd_ctx_fdget_failed, "
                    "vid: %u, vector: %u 's eventfd can't be %d\n", vdavinci->id, i, fd);
                ret = PTR_ERR(trigger);
                goto release_eventfd;
            }
            vdavinci->vdev.msix_triggers[i] = trigger;
        }
    } else if ((flags & VFIO_IRQ_SET_DATA_NONE) != 0 && count == 0) {
        hw_vdavinci_release_msix_eventfd_ctx(vdavinci);
    }

    return 0;

release_eventfd:
    hw_vdavinci_put_msix_trigger(vdavinci, i, start);

    return ret;
}

STATIC int hw_vdavinci_set_irqs(struct hw_vdavinci *vdavinci,
                                struct vfio_irq_set *hdr,
                                void *data)
{
    int (*func)(struct hw_vdavinci *vdavinci, const struct vfio_irq_set *hdr,
                void *data) = NULL;

    switch (hdr->index) {
        case VFIO_PCI_INTX_IRQ_INDEX:
            switch (hdr->flags & VFIO_IRQ_SET_ACTION_TYPE_MASK) {
                case VFIO_IRQ_SET_ACTION_MASK:
                    func = hw_vdavinci_set_intx_mask;
                    break;
                case VFIO_IRQ_SET_ACTION_UNMASK:
                    func = hw_vdavinci_set_intx_unmask;
                    break;
                case VFIO_IRQ_SET_ACTION_TRIGGER:
                    func = hw_vdavinci_set_intx_trigger;
                    break;
                default:
                    return -ENOTTY;
            }
            break;
        case VFIO_PCI_MSIX_IRQ_INDEX:
            switch (hdr->flags & VFIO_IRQ_SET_ACTION_TYPE_MASK) {
                case VFIO_IRQ_SET_ACTION_MASK:
                case VFIO_IRQ_SET_ACTION_UNMASK:
                    /* XXX Need masking support exported */
                    break;
                case VFIO_IRQ_SET_ACTION_TRIGGER:
                    func = hw_vdavinci_set_msix_trigger;
                    break;
                default:
                    return -ENOTTY;
            }
            break;
        default:
            return -ENOTTY;
    }

    if (func == NULL) {
        return -ENOTTY;
    }

    return func(vdavinci, hdr, data);
}

STATIC long _hw_vdavinci_device_get_info(uintptr_t arg,
                                         struct hw_vdavinci* vdavinci)
{
    struct vfio_device_info info;
    unsigned long minsz = offsetofend(struct vfio_device_info, num_irqs);

    if (!arg) {
        return -EINVAL;
    }

    if (copy_from_user(&info, (void __user *)arg, minsz) != 0) {
        return -EFAULT;
    }

    if (info.argsz < minsz) {
        return -EINVAL;
    }

    info.flags = VFIO_DEVICE_FLAGS_PCI;
    info.flags |= VFIO_DEVICE_FLAGS_RESET;
    info.num_regions = VFIO_PCI_NUM_REGIONS + vdavinci->vdev.num_regions;
    info.num_irqs = VFIO_PCI_NUM_IRQS;

    return copy_to_user((void __user *)arg, &info, minsz) != 0 ?    -EFAULT : 0;
}

STATIC int _hw_vdavinci_device_get_cap(int cap_type_id,
                                       struct vfio_region_info_cap_sparse_mmap *sparse,
                                       struct vfio_region_info *info, uintptr_t arg)
{
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
    int ret;
    struct vfio_info_cap caps = { .buf = NULL, .size = 0 };

    switch (cap_type_id) {
        case VFIO_REGION_INFO_CAP_SPARSE_MMAP:
            if (!sparse) {
                return -EINVAL;
            }
            ret = vfio_info_add_capability(&caps, &sparse->header,
                                           sizeof(*sparse) + (sparse->nr_areas * sizeof(*sparse->areas)));
            if (ret != 0) {
                return ret;
            }
            break;
        default:
            return -EINVAL;
    }

    if (caps.size != 0) {
        info->flags |= VFIO_REGION_INFO_FLAG_CAPS;
        if (info->argsz < sizeof(*info) + caps.size) {
            info->argsz = sizeof(*info) + caps.size;
            info->cap_offset = 0;
        } else {
            vfio_info_cap_shift(&caps, sizeof(*info));
            if (copy_to_user((void __user *)(arg + sizeof(*info)), caps.buf,
                    caps.size) != 0) {
                kfree(caps.buf);
                return -EFAULT;
            }
            info->cap_offset = sizeof(*info);
        }
        kfree(caps.buf);
    }
#endif
    return 0;
}

STATIC struct vfio_region_info_cap_sparse_mmap *
hw_vdavinci_device_get_sparse_info(struct vdavinci_mapinfo *mmio_map_info, unsigned int map_num)
{
    struct vfio_region_info_cap_sparse_mmap *sparse = NULL;
    u64 i = 0, j = 0;
    unsigned int nr_areas = map_num;
    struct vdavinci_bar_map *map;

    if (nr_areas == 0) {
        return NULL;
    }

    sparse = kzalloc(sizeof(*sparse) + sizeof(sparse->areas[0]) * nr_areas,
                     GFP_KERNEL);
    if (!sparse) {
        return NULL;
    }

    sparse->header.id = VFIO_REGION_INFO_CAP_SPARSE_MMAP;
    sparse->header.version = 1;
    sparse->nr_areas = nr_areas;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->map_type == MAP_TYPE_BACKEND || map->map_type == MAP_TYPE_PASSTHROUGH) {
            sparse->areas[j].offset = map->offset;
            sparse->areas[j].size = map->size;
            j++;
        }
    }

    return sparse;
}

STATIC long hw_vdavinci_device_get_bar_info(struct hw_vdavinci *vdavinci,
                                            struct vfio_region_info *info)
{
    switch (info->index) {
        case VFIO_PCI_CONFIG_REGION_INDEX:
            info->offset = VFIO_PCI_INDEX_TO_OFFSET(info->index);
            info->size = vdavinci->dvt->device_info.cfg_space_size;
            info->flags = VFIO_REGION_INFO_FLAG_READ |
                    VFIO_REGION_INFO_FLAG_WRITE;
            break;
        case VFIO_PCI_BAR0_REGION_INDEX:
        case VFIO_PCI_BAR2_REGION_INDEX:
        case VFIO_PCI_BAR4_REGION_INDEX:
            info->offset = VFIO_PCI_INDEX_TO_OFFSET(info->index);
            info->size = vdavinci->cfg_space.bar[info->index].size;
            info->flags = VFIO_REGION_INFO_FLAG_READ |
                    VFIO_REGION_INFO_FLAG_WRITE;
            break;
        case VFIO_PCI_BAR1_REGION_INDEX:
        case VFIO_PCI_BAR3_REGION_INDEX:
        case VFIO_PCI_BAR5_REGION_INDEX:
        case VFIO_PCI_ROM_REGION_INDEX:
        case VFIO_PCI_VGA_REGION_INDEX:
            info->offset = VFIO_PCI_INDEX_TO_OFFSET(info->index);
            info->size = 0;
            info->flags = 0;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

STATIC int hw_vdavinci_get_vfio_region_info(uintptr_t arg, struct vfio_region_info *info,
                                            struct vdavinci_mapinfo *mmio_map_info)
{
    u64 i = 0;
    int ret = 0, cap_type_id = 0;
    unsigned int map_num = 0;
    struct vdavinci_bar_map *map = NULL;
    struct vfio_region_info_cap_sparse_mmap *sparse = NULL;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->map_type == MAP_TYPE_BACKEND || map->map_type == MAP_TYPE_PASSTHROUGH) {
            map_num++;
        }
    }

    if (map_num > 0) {
        info->flags = info->flags | VFIO_REGION_INFO_FLAG_MMAP;
        sparse = hw_vdavinci_device_get_sparse_info(mmio_map_info, map_num);
        if (sparse) {
            cap_type_id = VFIO_REGION_INFO_CAP_SPARSE_MMAP;
            info->flags = info->flags | VFIO_REGION_INFO_FLAG_CAPS;
        }
    }

    if ((info->flags & VFIO_REGION_INFO_FLAG_CAPS) != 0) {
        ret = _hw_vdavinci_device_get_cap(cap_type_id, sparse, info, arg);
    }

    if (sparse) {
        kfree(sparse);
    }

    return ret;
}

STATIC long _hw_vdavinci_device_get_region_info(uintptr_t arg,
                                                struct hw_vdavinci* vdavinci)
{
    int ret = 0;
    struct vfio_region_info info;
    struct vdavinci_mapinfo *mmio_map_info = NULL;
    unsigned long minsz = offsetofend(struct vfio_region_info, offset);

    if (!arg) {
        return -EINVAL;
    }

    if (copy_from_user(&info, (void __user *)arg, minsz) != 0) {
        return -EFAULT;
    }

    if (info.argsz < minsz) {
        return -EINVAL;
    }

    if (hw_vdavinci_device_get_bar_info(vdavinci, &info)) {
        return -EINVAL;
    }

    mmio_map_info = hw_vdavinci_get_bar_sparse(vdavinci, info.index);
    if (!mmio_map_info) {
        return copy_to_user((void __user *)arg, &info, minsz) != 0 ? -EFAULT : 0;
    }

    ret = hw_vdavinci_get_vfio_region_info(arg, &info, mmio_map_info);
    if (ret) {
        return ret;
    }

    return copy_to_user((void __user *)arg, &info, minsz) ? -EFAULT : 0;
}

STATIC long _hw_vdavinci_device_get_irq_info(uintptr_t arg,
                                             struct hw_vdavinci* vdavinci)
{
    struct vfio_irq_info info;
    unsigned long minsz = offsetofend(struct vfio_irq_info, count);

    if (!arg) {
        return -EINVAL;
    }

    if (copy_from_user(&info, (void __user *)arg, minsz) != 0) {
        return -EFAULT;
    }

    if (info.argsz < minsz || info.index >= VFIO_PCI_NUM_IRQS) {
        return -EINVAL;
    }

    switch (info.index) {
        case VFIO_PCI_INTX_IRQ_INDEX:
        case VFIO_PCI_MSIX_IRQ_INDEX:
            break;
        default:
            return -EINVAL;
    }

    info.flags = VFIO_IRQ_INFO_EVENTFD;

    info.count = (unsigned int)hw_vdavinci_get_irq_count(vdavinci, info.index);

    if (info.index == VFIO_PCI_INTX_IRQ_INDEX) {
        info.flags |= (VFIO_IRQ_INFO_MASKABLE | VFIO_IRQ_INFO_AUTOMASKED);
    } else {
        info.flags |= VFIO_IRQ_INFO_NORESIZE;
    }

    return copy_to_user((void __user *)arg, &info, minsz) != 0 ? -EFAULT : 0;
}

#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
STATIC long _hw_vdavinci_device_set_ioeventfd(uintptr_t arg,
                                              struct hw_vdavinci *vdavinci)
{
    struct vfio_device_ioeventfd efd;
    unsigned long minsz = offsetofend(struct vfio_device_ioeventfd, fd);
    int count = 0;

    if (!arg) {
        return -EINVAL;
    }

    if (copy_from_user(&efd, (void __user *)arg, minsz)) {
        return -EFAULT;
    }

    if (efd.argsz < minsz || (efd.flags & ~VFIO_DEVICE_IOEVENTFD_SIZE_MASK)) {
        return -EINVAL;
    }

    count = efd.flags & VFIO_DEVICE_IOEVENTFD_SIZE_MASK;

    if (hweight8(count) != 1 || efd.fd < -1) {
        return -EINVAL;
    }

    return hw_vdavinci_set_ioeventfd(vdavinci, efd.offset, efd.data,
                                     count, efd.fd);
}
#endif

STATIC long _hw_vdavinci_device_set_irqs(uintptr_t arg,
                                         struct hw_vdavinci* vdavinci)
{
    struct vfio_irq_set hdr;
    unsigned long minsz = offsetofend(struct vfio_irq_set, count);
    size_t data_size = 0;
    u8 *data = NULL;
    int ret = 0;

    if (!arg) {
        return -EINVAL;
    }

    if (copy_from_user(&hdr, (void __user *)arg, minsz) != 0) {
        return -EFAULT;
    }

    if ((hdr.flags & VFIO_IRQ_SET_DATA_NONE) == 0) {
        int max = hw_vdavinci_get_irq_count(vdavinci, hdr.index);

        ret = vfio_set_irqs_validate_and_prepare(&hdr, max,
                                                 VFIO_PCI_NUM_IRQS, &data_size);
        if (ret) {
            return -EINVAL;
        }
        if (data_size != 0) {
            data = memdup_user((void __user *)(arg + minsz), data_size);
            if (IS_ERR(data)) {
                return PTR_ERR(data);
            }
        }
    }

    ret = hw_vdavinci_set_irqs(vdavinci, &hdr, data);
    kfree(data);

    return ret;
}

STATIC long _hw_vdavinci_device_reset(struct hw_vdavinci* vdavinci)
{
    return g_hw_vdavinci_ops.vdavinci_reset(vdavinci);
}

long hw_vdavinci_ioctl(struct mdev_device *mdev, unsigned int cmd,
                       unsigned long arg)
{
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));
    uintptr_t arg_uptr = arg;

    if (!vdavinci) {
        return -EINVAL;
    }

    switch (cmd) {
        case VFIO_DEVICE_GET_INFO:
            return _hw_vdavinci_device_get_info(arg_uptr, vdavinci);
        case VFIO_DEVICE_GET_REGION_INFO:
            return _hw_vdavinci_device_get_region_info(arg_uptr, vdavinci);
        case VFIO_DEVICE_GET_IRQ_INFO:
            return _hw_vdavinci_device_get_irq_info(arg_uptr, vdavinci);
        case VFIO_DEVICE_SET_IRQS:
            return _hw_vdavinci_device_set_irqs(arg_uptr, vdavinci);
        case VFIO_DEVICE_RESET:
            return _hw_vdavinci_device_reset(vdavinci);
        #if ((IS_VDAVINCI_KERNEL_VERSION_SUPPORT) || (defined(DRV_UT)))
        case VFIO_DEVICE_IOEVENTFD:
            return _hw_vdavinci_device_set_ioeventfd(arg_uptr, vdavinci);
        #endif
        default:
            return -ENOTTY;
    }
}

STATIC int kvmdt_register_mdev(struct device *dev, struct hw_dvt *dvt)
{
    dvt->drv = &hw_vdavinci_mdev_driver;
    return vdavinci_register_device(dev, dvt, VDAVINCI_NAME);
}

STATIC void kvmdt_unregister_mdev(struct device *dev, struct hw_dvt *dvt)
{
    vdavinci_unregister_device(dev, dvt);
}

STATIC int kvmdt_inject_msix(uintptr_t handle, u32 vector)
{
    struct kvmdt_guest_info *info = NULL;
    struct hw_vdavinci *vdavinci = NULL;
    int nvec;
    u32 nnvec;

    if (!handle_valid(handle)) {
        return -ESRCH;
    }

    info  = (struct kvmdt_guest_info *)handle;
    vdavinci = info->vdavinci;

    nvec = hw_vdavinci_get_irq_count(vdavinci, VFIO_PCI_MSIX_IRQ_INDEX);
    nnvec = (u32)nvec;

    if (vector >= nnvec) {
        vascend_err(vdavinci_to_dev(vdavinci), "inject msix failed, "
            "wrong msix data: %d, vid: %u\n", vector, vdavinci->id);
        return -EINVAL;
    }

    if (vdavinci->vdev.msix_triggers == NULL ||
        vdavinci->vdev.msix_triggers[vector] == NULL) {
        return 0;
    }

    if (vdavinci_eventfd_signal(vdavinci->vdev.msix_triggers[vector], 1) != 0) {
        vdavinci->debugfs.msix_count[vector]++;
        return 0;
    }

    return 0;
}

STATIC int kvmdt_rw_gpa_common(uintptr_t handle, unsigned long gpa,
                               void *buf, unsigned long len, bool write)
{
    struct kvmdt_guest_info *info = NULL;

    if (!handle_valid(handle)) {
        return -ESRCH;
    }
    info = (struct kvmdt_guest_info *)handle;
    return vdavinci_rw_gpa(info, gpa, buf, len, write);
}

STATIC int kvmdt_read_gpa(uintptr_t handle, unsigned long gpa,
                          void *buf, unsigned long len)
{
    return kvmdt_rw_gpa_common(handle, gpa, buf, len, false);
}

STATIC int kvmdt_write_gpa(uintptr_t handle, unsigned long gpa,
                           void *buf, unsigned long len)
{
    return kvmdt_rw_gpa_common(handle, gpa, buf, len, true);
}

STATIC unsigned long kvmdt_gfn_to_mfn(uintptr_t handle, unsigned long gfn)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
    struct kvmdt_guest_info *info = (struct kvmdt_guest_info *)handle;
    unsigned long pfn;

    if (!handle_valid(handle)) {
        return ~0UL;
    }
    /* An invalidation callback must wait for this temporary pin to drain. */
    mutex_lock(&info->vdavinci->vdev.cache_lock);
    pfn = info->vdavinci->vdev.domain ?
        vdavinci_lookup_guest_pfn(info->vdavinci->vdev.vfio_device, gfn) : ~0UL;
    mutex_unlock(&info->vdavinci->vdev.cache_lock);
    return pfn;
#else
    unsigned long pfn = 0;
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    struct kvmdt_guest_info *info = (struct kvmdt_guest_info *)handle;
    struct kvm *kvm = NULL;
    bool kthread = (current->mm == NULL);

    if (!handle_valid(handle)) {
        return ~0;
    }

    kvm = info->kvm;
    if (kthread) {
        if (!mmget_not_zero(kvm->mm)) {
            return ~0;
        }
        vdavinci_use_mm(kvm->mm);
    }

    pfn = gfn_to_pfn(info->kvm, gfn);

    if (kthread) {
        vdavinci_unuse_mm(kvm->mm);
        mmput(kvm->mm);
    }

    if (is_error_noslot_pfn(pfn)) {
        return ~0;
    }
    kvm_release_pfn_clean(pfn);
#endif
    return pfn;
#endif
}

STATIC int kvmdt_guest_init(struct mdev_device *mdev)
{
    struct kvmdt_guest_info *info = NULL;
    struct hw_vdavinci *vdavinci = NULL;
    struct kvm *kvm = NULL;

    vdavinci = get_mdev_drvdata(mdev_dev(mdev));
    if (handle_valid(vdavinci->handle)) {
        return -EEXIST;
    }

    kvm = vdavinci->vdev.kvm;
    if (kvm == NULL || kvm->mm != current->mm) {
        vascend_err(vdavinci_to_dev(vdavinci), "KVM is required to use huawei vdavinci, "
            "vid: %u\n", vdavinci->id);
        return -ESRCH;
    }

    info = vzalloc(sizeof(struct kvmdt_guest_info));
    if (info == NULL) {
        return -ENOMEM;
    }
    vdavinci->handle = (uintptr_t)info;
    info->vdavinci = vdavinci;
    info->kvm = kvm;
    kvm_get_kvm(info->kvm);

    return 0;
}

/* Serialize domain detachment with DMA queries and VFIO invalidations. */
void hw_vdavinci_release_vm_domain(struct hw_vdavinci *vdavinci)
{
    struct mutex *g_vm_domains_lock = get_vm_domains_lock();
    struct vm_dom_info *vm_dom;

    mutex_lock(&vdavinci->vdev.cache_lock);
    vm_dom = vdavinci->vdev.domain;
    vdavinci->vdev.domain = NULL;
    if (vm_dom != NULL &&
        vdavinci_refcount_mutex_lock(&vm_dom->ref.refcount, g_vm_domains_lock)) {
        /* Do not let another open find a domain whose last reference expired. */
        list_del_init(&vm_dom->node);
        mutex_unlock(g_vm_domains_lock);

        hw_vdavinci_unpin_pages(vdavinci, vm_dom);

        mutex_lock(g_vm_domains_lock);
        vm_dom_info_release(&vm_dom->ref);
        mutex_unlock(g_vm_domains_lock);
    }
    mutex_unlock(&vdavinci->vdev.cache_lock);
}

STATIC void kvmdt_guest_exit(struct kvmdt_guest_info *info)
{
    debugfs_remove(info->debugfs_cache_entries);
    hw_vdavinci_dma_pool_uninit(info->vdavinci);
    hw_vdavinci_release_vm_domain(info->vdavinci);
    kvm_put_kvm(info->kvm);
    vfree(info);
}

STATIC bool kvmdt_is_valid_gfn(uintptr_t handle, unsigned long gfn)
{
    struct kvmdt_guest_info *info = (struct kvmdt_guest_info *)handle;

    if (!handle_valid(handle)) {
        return false;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
    return vdavinci_guest_gfn_visible(info->kvm, gfn);
#else
    {
        bool ret;
        int idx = srcu_read_lock(&info->kvm->srcu);

        ret = kvm_is_visible_gfn(info->kvm, gfn);
        srcu_read_unlock(&info->kvm->srcu, idx);
        return ret;
    }
#endif
}

STATIC int kvmdt_get_mmio_info(void **dst, int *size, void *_vdavinci, int bar)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)_vdavinci;
    struct hw_dvt *dvt = vdavinci->dvt;

    if (bar == IO_REGION_INDEX) {
        *dst = vdavinci->mmio.io_base;
        *size = vdavinci->type->bar2_size;
        if (hw_vdavinci_sriov_support(dvt)) {
            *size = VF_BAR0_VPC_SIZE;
        }
    } else if (bar == MEM_REGION_INDEX) {
        *dst = vdavinci->mmio.mem_base;
        *size = vdavinci->type->bar4_size;
        if (hw_vdavinci_sriov_support(dvt)) {
            *size = VF_BAR0_DVPP_SIZE;
        }
    } else {
        return -EINVAL;
    }

    return 0;
}

STATIC bool is_vcpu_thread(struct task_struct *task)
{
    return strstarts(task->comm, "CPU ");
}

bool get_node_cpu_by_page(struct hw_vdavinci *vdavinci,
                          unsigned int current_cpu,
                          struct page *page,
                          struct cpumask *cpumask)
{
#ifdef CONFIG_NUMA
    unsigned int cpu = 0;
    struct task_struct *me = NULL;
    struct task_struct *thread = NULL;
    int page_node = page_to_nid(page);
    struct task_struct *qemu_task = vdavinci->qemu_task;
    struct cpumask *thread_mask;

    if (page_node == cpu_to_node(current_cpu)) {
        return false;
    }
    me = qemu_task;
    thread = me;
    do {
        if (!is_vcpu_thread(thread)) {
            continue;
        }
        cpu = task_cpu(thread);
        if (cpu_to_node(cpu) == page_node) {
            cpumask_set_cpu(cpu, cpumask);
        }
        thread_mask = vdavinci_get_cpumask(thread);
        cpumask_or(&(vdavinci->vm_cpus_mask), &(vdavinci->vm_cpus_mask), thread_mask);
    } while_each_thread(me, thread);
    for_each_cpu(cpu, &(vdavinci->vm_cpus_mask)) {
        if (cpu_to_node(cpu) == page_node) {
            cpumask_set_cpu(cpu, cpumask);
        }
    }
    thread_mask = vdavinci_get_cpumask(qemu_task);
    for_each_cpu(cpu, thread_mask) {
        if (cpu_to_node(cpu) == page_node) {
            cpumask_set_cpu(cpu, cpumask);
        }
    }
#endif /* CONFIG_NUMA */
    return true;
}

struct hw_kvmdt_ops g_hw_kvmdt_ops = {
    .register_mdev = kvmdt_register_mdev,
    .unregister_mdev = kvmdt_unregister_mdev,
    .inject_msix = kvmdt_inject_msix,
    .read_gpa = kvmdt_read_gpa,
    .write_gpa = kvmdt_write_gpa,
    .gfn_to_mfn = kvmdt_gfn_to_mfn,
    .is_valid_gfn = kvmdt_is_valid_gfn,
    .mmio_get = kvmdt_get_mmio_info,
    .dma_pool_init = hw_vdavinci_dma_pool_init,
    .dma_pool_uninit = hw_vdavinci_dma_pool_uninit,
    .dma_get_iova = hw_vdavinci_get_iova,
    .dma_put_iova = hw_vdavinci_put_iova,
    .dma_get_iova_batch = hw_vdavinci_get_iova_batch,
};

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
#include <linux/device.h>
#include <linux/uuid.h>
#include <linux/mdev.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0))
#include <linux/dma-map-ops.h>
#include <linux/vfio.h>
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,36)) || (defined(DRV_UT)))
#include <linux/dma-noncoherent.h>
#include <linux/vfio.h>
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)) || (defined(DRV_UT)))
#include <linux/vfio.h>
#endif

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)) || (defined(DRV_UT)))
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)) || (defined(DRV_UT)))
#include <linux/sched/mm.h>
#endif
#include <linux/kvm_host.h>
#endif

#include <linux/mm.h>
#include <linux/mmu_context.h>
#include <linux/kthread.h>
#include "dvt_sysfs.h"
#include "domain_manage.h"
#include "dma_pool.h"
#include "kvmdt.h"
#include "vfio_ops.h"

STATIC DEFINE_MUTEX(mdev_register_lock);

#ifndef __ASM_DEVICE_H
bool is_dev_dma_coherent(struct device *dev)
{
    return true;
}
#else
#if ((LINUX_VERSION_CODE == KERNEL_VERSION(4,19,36)) || (LINUX_VERSION_CODE == KERNEL_VERSION(4,19,90)))
bool is_dev_dma_coherent(struct device *dev)
{
    return dev->archdata.dma_coherent;
}
#else
bool is_dev_dma_coherent(struct device *dev)
{
        return true;
}
#endif
#endif /* __ASM_DEVICE_H */

void vdavinci_iommu_unmap(struct device *dev, unsigned long iova, size_t size)
{
    size_t unmapped;
    struct iommu_domain *domain = NULL;

    dma_sync_single_for_cpu(dev, iova, size, DMA_BIDIRECTIONAL);
    domain = iommu_get_domain_for_dev(dev);
    unmapped = iommu_unmap(domain, iova, size);
    WARN_ON(unmapped != size);
}

int vdavinci_iommu_map(struct device *dev, unsigned long iova,
                       phys_addr_t paddr, size_t size, int prot)
{
    struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0))
    return iommu_map(domain, iova, paddr, size, prot, GFP_KERNEL);
#else
    return iommu_map(domain, iova, paddr, size, prot);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,36))
    if (!is_dev_dma_coherent(dev)) {
        dma_sync_single_for_device(dev, iova, size, DMA_BIDIRECTIONAL);
    }
#endif
}

void vdavinci_unpin_pages(struct hw_vdavinci *vdavinci, struct vdavinci_pin_info *pin_info)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
    unsigned long *gfns = NULL;
    int i = 0;
#endif
    int ret = pin_info->npage;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
    gfns = kzalloc(sizeof(unsigned long) * pin_info->npage, GFP_KERNEL);
    if (IS_ERR_OR_NULL(gfns)) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci malloc mem error\n");
        return;
    }
    for (i = 0; i < pin_info->npage; i++) {
        gfns[i] = pin_info->gfn + i;
    }
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0))
    vfio_unpin_pages(vdavinci->vdev.vfio_device, pin_info->gfn << PAGE_SHIFT,
                     pin_info->npage);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0))
    ret = vfio_unpin_pages(vdavinci->vdev.vfio_device, gfns, pin_info->npage);
#else
    ret = vfio_unpin_pages(mdev_dev(vdavinci->vdev.mdev), gfns, pin_info->npage);
#endif
    WARN_ON(ret != pin_info->npage);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
    kfree(gfns);
#endif
}

int vdavinci_pin_pages(struct hw_vdavinci *vdavinci, struct vdavinci_pin_info *pin_info)
{
    int ret = -1;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
    int i = 0;
    unsigned long *gfns = NULL, *pfns = NULL;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
    gfns = kzalloc(sizeof(unsigned long) * pin_info->npage, GFP_KERNEL);
    if (IS_ERR_OR_NULL(gfns)) {
        return -ENOMEM;
    }
    for (i = 0; i < pin_info->npage; i++) {
        gfns[i] = pin_info->gfn + i;
    }
    pfns = kzalloc(sizeof(unsigned long) * pin_info->npage, GFP_KERNEL);
    if (IS_ERR_OR_NULL(pfns)) {
        ret = -ENOMEM;
        goto free_gfns;
    }
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0))
    ret = vfio_pin_pages(vdavinci->vdev.vfio_device, pin_info->gfn << PAGE_SHIFT,
                         pin_info->npage, IOMMU_READ | IOMMU_WRITE, pin_info->pages);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0))
    ret = vfio_pin_pages(vdavinci->vdev.vfio_device, gfns,
                         pin_info->npage, IOMMU_READ | IOMMU_WRITE, pfns);
#else
    ret = vfio_pin_pages(mdev_dev(vdavinci->vdev.mdev), gfns,
                         pin_info->npage, IOMMU_READ | IOMMU_WRITE, pfns);
#endif
    if (ret != pin_info->npage) {
        vascend_warn(vdavinci_to_dev(vdavinci), "pin page's quantities are not equal, %d, %d\n",
                     ret, pin_info->npage);
        ret = (ret < 0) ? ret : -EINVAL;
    }
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
    for (i = 0; ret > 0 && i < pin_info->npage; i++) {
        pin_info->pages[i] = pfn_to_page(pfns[i]);
    }

    kfree(pfns);
free_gfns:
    kfree(gfns);
#endif
    return ret;
}

struct device *get_mdev_parent(struct mdev_device *mdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
    return mdev->type->parent->dev;
#else
    return mdev_parent_dev(mdev);
#endif
}

bool device_is_mdev(struct device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0)
    if (dev == NULL) {
        return false;
    }
    return !strcmp(dev->bus->name, "mdev");
#else
    return dev->bus == &mdev_bus_type;
#endif
}

struct mdev_device *get_mdev_device(struct device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
    return to_mdev_device(dev);
#else
    return mdev_from_dev(dev);
#endif
}

void *get_mdev_drvdata(struct device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)
    return dev_get_drvdata(dev);
#else
    struct mdev_device *mdev = mdev_from_dev(dev);

    return mdev_get_drvdata(mdev);
#endif
}

void set_mdev_drvdata(struct device *dev, void *data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)
    dev_set_drvdata(dev, data);
#else
    struct mdev_device *mdev = mdev_from_dev(dev);

    mdev_set_drvdata(mdev, data);
#endif
}

void vdavinci_unregister_driver(struct mdev_driver *drv)
{
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
    mdev_unregister_driver(drv);
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* CONFIG_VFIO_MDEV */
}

int vdavinci_register_driver(struct mdev_driver *drv)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
    ret = mdev_register_driver(drv);
    if (ret != 0) {
        return ret;
    }
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* CONFIG_VFIO_MDEV */

    return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
STATIC inline struct hw_vfio_vdavinci *vfio_dev_to_vfio_vnpu(struct vfio_device *vfio_dev)
{
    return container_of(vfio_dev, struct hw_vfio_vdavinci, vfio_dev);
}

STATIC inline struct hw_vdavinci *vfio_dev_to_vnpu(struct vfio_device *vfio_dev)
{
    struct hw_vfio_vdavinci *vfio_vdavinci = NULL;

    vfio_vdavinci = vfio_dev_to_vfio_vnpu(vfio_dev);
    if (vfio_vdavinci == NULL) {
        return NULL;
    }

    return vfio_vdavinci->vdavinci;
}

STATIC int hw_vdavinci_vfio_init(struct vfio_device *vfio_dev)
{
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);
    struct hw_vfio_vdavinci *vfio_vdavinci = vfio_dev_to_vfio_vnpu(vfio_dev);

    vfio_vdavinci->vdavinci = hw_vdavinci_create(&mdev->type->kobj, mdev);
    if (vfio_vdavinci->vdavinci == NULL) {
        return -EINVAL;
    }

    return 0;
}

STATIC void hw_vdavinci_vfio_release(struct vfio_device *vfio_dev)
{
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);

    (void)hw_vdavinci_remove(mdev);
}

STATIC int hw_vdavinci_vfio_open(struct vfio_device *vfio_dev)
{
    struct vm_dom_info *vm_dom = NULL;
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);
    struct hw_vdavinci *vdavinci = vfio_dev_to_vnpu(vfio_dev);
    struct mutex *g_vm_domains_lock = get_vm_domains_lock();

    if (vdavinci == NULL || vfio_dev->kvm == NULL) {
        return -EINVAL;
    }
    vdavinci->vdev.kvm = vfio_dev->kvm;
    mutex_lock(g_vm_domains_lock);
    vm_dom = vm_dom_info_get(vdavinci->vdev.kvm);
    mutex_unlock(g_vm_domains_lock);
    if (vm_dom == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vnpu init domain failed\n");
        return -EINVAL;
    }
    vdavinci->vdev.domain = vm_dom;

    return hw_vdavinci_open(mdev);
}

STATIC void hw_vdavinci_vfio_close(struct vfio_device *vfio_dev)
{
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);
    struct hw_vdavinci *vdavinci = vfio_dev_to_vnpu(vfio_dev);

    if (vdavinci == NULL) {
        return;
    }
    hw_vdavinci_release(mdev);
    vdavinci->vdev.kvm = NULL;
    vdavinci->vdev.domain = NULL;
}

STATIC ssize_t hw_vdavinci_vfio_read(struct vfio_device *vfio_dev, char __user *buf,
                                     size_t count, loff_t *ppos)
{
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);

    return hw_vdavinci_read(mdev, buf, count, ppos);
}

STATIC ssize_t hw_vdavinci_vfio_write(struct vfio_device *vfio_dev,
                                      const char __user *buf,
                                      size_t count, loff_t *ppos)
{
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);

    return hw_vdavinci_write(mdev, buf, count, ppos);
}

STATIC int hw_vdavinci_vfio_mmap(struct vfio_device *vfio_dev, struct vm_area_struct *vma)
{
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);

    return hw_vdavinci_mmap(mdev, vma);
}

STATIC long hw_vdavinci_vfio_ioctl(struct vfio_device *vfio_dev,
                                   unsigned int cmd, unsigned long arg)
{
    struct mdev_device *mdev = get_mdev_device(vfio_dev->dev);

    return hw_vdavinci_ioctl(mdev, cmd, arg);
}

STATIC void hw_vdavinci_vfio_dma_unmap(struct vfio_device *vfio_dev,
                                       u64 iova, u64 length)
{
    unsigned long start_gfn = iova >> PAGE_SHIFT;
    struct hw_vdavinci *vdavinci = vfio_dev_to_vnpu(vfio_dev);

    if (vdavinci == NULL) {
        return;
    }
    mutex_lock(&vdavinci->vdev.cache_lock);
    hw_vdavinci_unplug_ram(vdavinci, start_gfn, length);
    mutex_unlock(&vdavinci->vdev.cache_lock);
}

STATIC const struct vfio_device_ops hw_vnpu_vfio_dev_ops = {
    .init = hw_vdavinci_vfio_init,
    .release = hw_vdavinci_vfio_release,
    .open_device = hw_vdavinci_vfio_open,
    .close_device = hw_vdavinci_vfio_close,
    .read = hw_vdavinci_vfio_read,
    .write = hw_vdavinci_vfio_write,
    .mmap = hw_vdavinci_vfio_mmap,
    .dma_unmap = hw_vdavinci_vfio_dma_unmap,
    .ioctl = hw_vdavinci_vfio_ioctl,
    .bind_iommufd	= vfio_iommufd_emulated_bind,
    .unbind_iommufd = vfio_iommufd_emulated_unbind,
    .attach_ioas	= vfio_iommufd_emulated_attach_ioas,
    .detach_ioas	= vfio_iommufd_emulated_detach_ioas,
};

STATIC int hw_vdavinci_vfio_probe(struct mdev_device *mdev)
{
    int ret = 0;
    struct device *pdev;
    struct hw_dvt *dvt;
    struct hw_vfio_vdavinci *vfio_vdavinci = NULL;

    pdev = get_mdev_parent(mdev);
    dvt = kdev_to_davinci(pdev)->dvt;
    if (!hw_vdavinci_is_enabled(dvt)) {
        vascend_err(pdev, "driver is not in vm mode or device's sriov is not enabled\n");
        return -EINVAL;
    }

    vfio_vdavinci = vfio_alloc_device(hw_vfio_vdavinci, vfio_dev, &mdev->dev, &hw_vnpu_vfio_dev_ops);
    if (IS_ERR(vfio_vdavinci)) {
        vascend_err(pdev, "vdavinci probe failed\n");
        return -EINVAL;
    }
    vfio_vdavinci->vdavinci->vdev.vfio_device = &vfio_vdavinci->vfio_dev;
    ret = vfio_register_emulated_iommu_dev(&vfio_vdavinci->vfio_dev);
    if (ret != 0) {
        vascend_err(pdev, "register iommu error\n");
        goto out_put_vdev;
    }

    return 0;

out_put_vdev:
    vfio_put_device(&vfio_vdavinci->vfio_dev);
    return ret;
}

STATIC void hw_vdavinci_vfio_remove(struct mdev_device *mdev)
{
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));
    struct hw_vfio_vdavinci *vfio_vdavinci = NULL;

    if (vdavinci == NULL) {
        return;
    }
    vfio_vdavinci = container_of(vdavinci->vdev.vfio_device,
                                 struct hw_vfio_vdavinci, vfio_dev);
    if (vfio_vdavinci == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "can not get vfio vdavinci\n");
        return;
    }

    vfio_unregister_group_dev(&vfio_vdavinci->vfio_dev);
    vfio_put_device(&vfio_vdavinci->vfio_dev);
}
#else
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5,12,0))
STATIC int hw_vdavinci_create_ops(struct kobject *kobj, struct mdev_device *mdev)
{
    struct hw_vdavinci *vdavinci = NULL;

    vdavinci = hw_vdavinci_create(kobj, mdev);
    if (vdavinci == NULL) {
        return -EINVAL;
    }

    return 0;
}
#else
STATIC int hw_vdavinci_create_ops(struct mdev_device *mdev)
{
    struct hw_vdavinci *vdavinci = NULL;
    struct kobject *kobj = NULL;

    if (mdev == NULL || mdev->type == NULL) {
        return -EINVAL;
    }
    kobj = (struct kobject *)mdev->type;
    if (kobj == NULL || kobj->name == NULL) {
        return -EINVAL;
    }
    vdavinci = hw_vdavinci_create(kobj, mdev);
    if (vdavinci == NULL) {
        return -EINVAL;
    }

    return 0;
}
#endif /* KERNEL_VERSION(5,12,0) */

STATIC int hw_vdavinci_remove_ops(struct mdev_device *mdev)
{
    return hw_vdavinci_remove(mdev);
}

STATIC int hw_vdavinci_open_ops(struct mdev_device *mdev)
{
    return hw_vdavinci_open(mdev);
}

STATIC void hw_vdavinci_release_ops(struct mdev_device *mdev)
{
    hw_vdavinci_release(mdev);
}

STATIC ssize_t hw_vdavinci_read_ops(struct mdev_device *mdev,
                                    char __user *buf,
                                    size_t count, loff_t *ppos)
{
    return hw_vdavinci_read(mdev, buf, count, ppos);
}

STATIC ssize_t hw_vdavinci_write_ops(struct mdev_device *mdev,
                                     const char __user *buf,
                                     size_t count, loff_t *ppos)
{
    return hw_vdavinci_write(mdev, buf, count, ppos);
}

STATIC int hw_vdavinci_mmap_ops(struct mdev_device *mdev,
                                struct vm_area_struct *vma)
{
    return hw_vdavinci_mmap(mdev, vma);
}

STATIC long hw_vdavinci_ioctl_ops(struct mdev_device *mdev,
                                  unsigned int cmd,
                                  unsigned long arg)
{
    return hw_vdavinci_ioctl(mdev, cmd, arg);
}
#endif /* KERNEL_VERSION(6,1,0) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
struct mdev_driver hw_vdavinci_mdev_driver = {
    .device_api = VFIO_DEVICE_API_PCI_STRING,
    .driver = {
        .name = "vnpu_mdev",
        .owner = THIS_MODULE,
    },
    .probe = hw_vdavinci_vfio_probe,
    .remove = hw_vdavinci_vfio_remove,
    .get_available = available_instances_show,
    .show_description = description_show
};
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0))
struct mdev_driver hw_vdavinci_mdev_driver = {
    .driver = {
        .name = "vnpu_mdev",
        .owner = THIS_MODULE,
    },
    .probe = hw_vdavinci_vfio_probe,
    .remove = hw_vdavinci_vfio_remove,
};
#else
struct mdev_driver hw_vdavinci_mdev_driver = {
};
#endif /* KERNEL_VERSION(6,1,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
STATIC int vdavinci_init_type_groups(struct hw_dvt *dvt)
{
    unsigned int i;
    struct hw_vdavinci_type *type = NULL;

    dvt->mdev_types = kcalloc(dvt->vdavinci_type_num * dvt->dev_num,
                              sizeof(struct mdev_type *), GFP_KERNEL);
    if (dvt->mdev_types == NULL) {
        return -ENOMEM;
    }

    for (i = 0; i < dvt->vdavinci_type_num * dvt->dev_num; i++) {
        type = &dvt->types[i];

        dvt->mdev_types[i] = &type->mtype;
        dvt->mdev_types[i]->sysfs_name = type->name;
    }

    return 0;
}

STATIC void vdavinci_cleanup_type_groups(struct hw_dvt *dvt)
{
    kfree(dvt->mdev_types);
    dvt->mdev_types = NULL;
}
#else
STATIC int vdavinci_init_type_groups(struct hw_dvt *dvt)
{
    unsigned int i, j;
    struct hw_vdavinci_type *type = NULL;
    struct attribute_group *group = NULL;

    /* we need put a NULL pointer at the end of supported_type_groups
     * array, vfio-mdev module use the NULL pointer as the arrary end.
     */
    dvt->groups = kcalloc(dvt->vdavinci_type_num * dvt->dev_num + 1,
                          sizeof(struct attribute_group *), GFP_KERNEL);
    if (dvt->groups == NULL) {
        return -ENOMEM;
    }

    for (i = 0; i < dvt->vdavinci_type_num * dvt->dev_num; i++) {
        type = &dvt->types[i];

        group = kzalloc(sizeof(struct attribute_group), GFP_KERNEL);
        if (WARN_ON(!group)) {
            goto unwind;
        }

        group->name = type->name;
        group->attrs = get_hw_vdavinci_type_attrs();
        dvt->groups[i] = group;
    }
    return 0;

unwind:
    for (j = 0; j < i; j++) {
        kfree(dvt->groups[j]);
        dvt->groups[j] = NULL;
    }

    kfree(dvt->groups);
    dvt->groups = NULL;
    return -ENOMEM;
}

STATIC void vdavinci_cleanup_type_groups(struct hw_dvt *dvt)
{
    int i;

    for (i = 0; i < dvt->vdavinci_type_num * dvt->dev_num; i++) {
        kfree(dvt->groups[i]);
        dvt->groups[i] = NULL;
    }

    kfree(dvt->groups);
    dvt->groups = NULL;
}
#endif

STATIC int vdavinci_set_device_ops(struct hw_dvt *dvt)
{
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
    dvt->drv->driver.dev_groups = get_hw_vdavinci_groups();
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0))
    dvt->drv->driver.dev_groups = get_hw_vdavinci_groups();
    dvt->supported_type_groups = dvt->groups;
#else
    struct mdev_parent_ops *vdavinci_mdev_ops = NULL;

    vdavinci_mdev_ops = kzalloc(sizeof(struct mdev_parent_ops), GFP_KERNEL);
    if (vdavinci_mdev_ops == NULL) {
        return -ENOMEM;
    }
    vdavinci_mdev_ops->create = hw_vdavinci_create_ops;
    vdavinci_mdev_ops->remove = hw_vdavinci_remove_ops;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0))
    vdavinci_mdev_ops->open = hw_vdavinci_open_ops;
    vdavinci_mdev_ops->release = hw_vdavinci_release_ops;
#else
    vdavinci_mdev_ops->open_device = hw_vdavinci_open_ops;
    vdavinci_mdev_ops->close_device = hw_vdavinci_release_ops;
#endif /* KERNEL_VERSION(5,15,0) */
    vdavinci_mdev_ops->read = hw_vdavinci_read_ops;
    vdavinci_mdev_ops->write = hw_vdavinci_write_ops;
    vdavinci_mdev_ops->mmap = hw_vdavinci_mmap_ops;
    vdavinci_mdev_ops->ioctl = hw_vdavinci_ioctl_ops;
    vdavinci_mdev_ops->supported_type_groups = dvt->groups;
    vdavinci_mdev_ops->mdev_attr_groups = get_hw_vdavinci_groups();
    dvt->vdavinci_mdev_ops = vdavinci_mdev_ops;
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* CONFIG_VFIO_MDEV */

    return 0;
}

STATIC void vdavinci_clean_device_ops(struct hw_dvt *dvt)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,19,0))
    if (IS_ERR_OR_NULL(dvt->vdavinci_mdev_ops)) {
        return;
    }
    kfree(dvt->vdavinci_mdev_ops);
    dvt->vdavinci_mdev_ops = NULL;
#endif
}

STATIC int vdavinci_register_mdev_device(struct device *dev,
                                         struct hw_dvt *dvt,
                                         const char *name)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_VFIO_MDEV)
    const char *saved_name;

    mutex_lock(&mdev_register_lock);
    saved_name = dev->driver->name;
    dev->driver->name = name;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
    ret = mdev_register_parent(&dvt->parent, dev, dvt->drv,
                               dvt->mdev_types, dvt->vdavinci_type_num * dvt->dev_num);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0))
    ret = mdev_register_device(dev, dvt->drv);
#else
    ret = mdev_register_device(dev, dvt->vdavinci_mdev_ops);
#endif /* KERNEL_VERSION(6,1,0) */
    dev->driver->name = saved_name;
    mutex_unlock(&mdev_register_lock);
#endif /* CONFIG_VFIO_MDEV */

    return ret;
}

STATIC void vdavinci_unregister_mdev_device(struct device *dev,
                                            struct hw_dvt *dvt)
{
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
    mdev_unregister_parent(&dvt->parent);
#else
    mdev_unregister_device(dev);
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* CONFIG_VFIO_MDEV */
}

int vdavinci_register_device(struct device *dev,
                             struct hw_dvt *dvt,
                             const char *name)
{
    int ret = 0;

    ret = vdavinci_init_type_groups(dvt);
    if (ret != 0) {
        vascend_err(dev, "vdavinci init type groups error: %d\n", ret);
        return ret;
    }
    ret = vdavinci_set_device_ops(dvt);
    if (ret != 0) {
        vascend_err(dev, "vdavinci set device's ops error: %d\n", ret);
        goto clean_group;
    }
    ret = vdavinci_register_mdev_device(dev, dvt, name);
    if (ret != 0) {
        vascend_err(dev, "vdavinci register mdev device error: %d\n", ret);
        goto clean_ops;
    }

    return 0;

clean_ops:
    vdavinci_clean_device_ops(dvt);
clean_group:
    vdavinci_cleanup_type_groups(dvt);
    return ret;
}

void vdavinci_unregister_device(struct device *dev, struct hw_dvt *dvt)
{
    vdavinci_unregister_mdev_device(dev, dvt);
    vdavinci_clean_device_ops(dvt);
    vdavinci_cleanup_type_groups(dvt);
}

void vdavinci_init_iova_domain(struct iova_domain *iovad)
{
    if (iovad == NULL) {
        return;
    }
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
    init_iova_domain(iovad, PAGE_SIZE, 1);
#else
    init_iova_domain(iovad, PAGE_SIZE, 1,
                     DMA_BIT_MASK(DOMAIN_DMA_BIT_MASK_32) >> PAGE_SHIFT);
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,1,0)
STATIC void guid_to_uuid(struct device *dev, uuid_le *dst, const guid_t *src)
{
    if (UUID_SIZE != sizeof(guid_t) || UUID_SIZE != sizeof(uuid_le)) {
        vascend_err(dev, "uuid size error\n");
        return;
    }
    memcpy_s(dst, UUID_SIZE, src, UUID_SIZE);
}
#endif

uuid_le vdavinci_get_uuid(struct mdev_device *mdev)
{
    uuid_le uuid;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)
    struct device *pdev = get_mdev_parent(mdev);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,1,0)
    struct device *pdev = get_mdev_parent(mdev);
    const guid_t *m_uuid = mdev_uuid(mdev);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)
    guid_to_uuid(pdev, &uuid, &mdev->uuid);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,1,0)
    guid_to_uuid(pdev, &uuid, m_uuid);
#else
    uuid = mdev_uuid(mdev);
#endif

    return uuid;
}

void vdavinci_use_mm(struct mm_struct *mm)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0))
    kthread_use_mm(mm);
#else
    use_mm(mm);
#endif
}

void vdavinci_unuse_mm(struct mm_struct *mm)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0))
    kthread_unuse_mm(mm);
#else
    unuse_mm(mm);
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0))
STATIC int hw_vdavinci_iommu_notifier(struct notifier_block *nb,
                                      unsigned long action, void *data)
{
    struct hw_vdavinci *vdavinci = container_of(nb,
            struct hw_vdavinci, vdev.iommu_notifier);

    if (action == VFIO_IOMMU_NOTIFY_DMA_UNMAP) {
        unsigned long start_gfn;
        struct vfio_iommu_type1_dma_unmap *unmap = data;

        if (unmap == NULL) {
            return NOTIFY_BAD;
        }
        start_gfn = unmap->iova >> PAGE_SHIFT;
        mutex_lock(&vdavinci->vdev.cache_lock);
        hw_vdavinci_unplug_ram(vdavinci, start_gfn, unmap->size);
        mutex_unlock(&vdavinci->vdev.cache_lock);
    }

    return NOTIFY_OK;
}

STATIC int hw_vdavinci_group_notifier(struct notifier_block *nb,
                                      unsigned long action, void *data)
{
    struct vm_dom_info *vm_dom = NULL;
    struct hw_vdavinci *vdavinci = container_of(nb,
            struct hw_vdavinci, vdev.group_notifier);
    struct mutex *g_vm_domains_lock = get_vm_domains_lock();

    if (action == VFIO_GROUP_NOTIFY_SET_KVM) {
        vdavinci->vdev.kvm = data;

        if (data == NULL) {
            schedule_work(&vdavinci->vdev.release_work);
        } else {
            mutex_lock(g_vm_domains_lock);
            vm_dom = vm_dom_info_get(vdavinci->vdev.kvm);
            mutex_unlock(g_vm_domains_lock);
            if (!vm_dom) {
                vascend_err(vdavinci_to_dev(vdavinci), "vnpu init domain failed.\n");
            }

            vdavinci->vdev.domain = vm_dom;
        }
    }

    return NOTIFY_OK;
}

STATIC int hw_get_vfio_group(struct mdev_device *mdev)
{
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)))
    int ret = 0;
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));
    struct vfio_group *vfio_group = NULL;

    vfio_group = vfio_group_get_external_user_from_dev(mdev_dev(mdev));
    if (IS_ERR_OR_NULL(vfio_group)) {
        ret = !vfio_group ? -EFAULT : (int)PTR_ERR(vfio_group);
        vascend_err(vdavinci_to_dev(vdavinci),
                    "vfio_group_get_external_user_from_dev failed, ret: %d\n", ret);
        return ret;
    }
    vdavinci->vdev.vfio_group = vfio_group;
#endif
    return 0;
}

STATIC void hw_put_vfio_group(struct mdev_device *mdev)
{
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)))
    struct hw_vdavinci *vdavinci = get_mdev_drvdata(mdev_dev(mdev));

    if (!vdavinci->vdev.vfio_group) {
        return;
    }
    vfio_group_put_external_user(vdavinci->vdev.vfio_group);
    vdavinci->vdev.vfio_group = NULL;
#endif
}
#endif /* KERNEL_VERSION(6,0,0) */

int vdavinci_register_vfio_group(struct hw_vdavinci *vdavinci)
{
    int ret = 0;
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0)))
    unsigned long events = VFIO_IOMMU_NOTIFY_DMA_UNMAP;

    vdavinci->vdev.iommu_notifier.notifier_call = hw_vdavinci_iommu_notifier;
    vdavinci->vdev.group_notifier.notifier_call = hw_vdavinci_group_notifier;
    ret = vfio_register_notifier(mdev_dev(vdavinci->vdev.mdev), VFIO_IOMMU_NOTIFY,
                                 &events, &vdavinci->vdev.iommu_notifier);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vfio register iommu notifier failed, "
                    "vid: %u, ret: %d\n", vdavinci->id, ret);
        goto out;
    }
    events = VFIO_GROUP_NOTIFY_SET_KVM;
    ret = vfio_register_notifier(mdev_dev(vdavinci->vdev.mdev), VFIO_GROUP_NOTIFY,
                                 &events, &vdavinci->vdev.group_notifier);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vfio register group notifier failed, "
                    "vid: %u, ret: %d\n", vdavinci->id, ret);
        goto unregister_iommu;
    }

    ret = hw_get_vfio_group(vdavinci->vdev.mdev);
    if (ret != 0) {
        goto unregister_group;
    }

    return 0;

unregister_group:
    vfio_unregister_notifier(mdev_dev(vdavinci->vdev.mdev), VFIO_GROUP_NOTIFY,
                             &vdavinci->vdev.group_notifier);
unregister_iommu:
    vfio_unregister_notifier(mdev_dev(vdavinci->vdev.mdev), VFIO_IOMMU_NOTIFY,
                             &vdavinci->vdev.iommu_notifier);
    vdavinci->vdev.iommu_notifier.notifier_call = NULL;
    vdavinci->vdev.group_notifier.notifier_call = NULL;
out:
#endif
    return ret;
}

void vdavinci_unregister_vfio_group(struct hw_vdavinci *vdavinci)
{
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0)))
    int ret = 0;

    hw_put_vfio_group(vdavinci->vdev.mdev);
    ret = vfio_unregister_notifier(mdev_dev(vdavinci->vdev.mdev), VFIO_GROUP_NOTIFY,
                                   &vdavinci->vdev.group_notifier);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "Failed to unregister vfio group notifier, "
            "vid: %u, ret: %d\n", vdavinci->id, ret);
    }

    ret = vfio_unregister_notifier(mdev_dev(vdavinci->vdev.mdev), VFIO_IOMMU_NOTIFY,
                                   &vdavinci->vdev.iommu_notifier);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "Failed to unregister vfio iommu notifier, "
            "vid: %u, ret: %d\n", vdavinci->id, ret);
    }
    vdavinci->vdev.iommu_notifier.notifier_call = NULL;
    vdavinci->vdev.group_notifier.notifier_call = NULL;
#endif
}

__u64 vdavinci_eventfd_signal(struct eventfd_ctx *ctx, __u64 n)
{
    __u64 ret = 1;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0))
    eventfd_signal(ctx);
#else
    ret = eventfd_signal(ctx, n);
#endif

    return ret;
}

bool vdavinci_refcount_mutex_lock(refcount_t *r, struct mutex *lock)
{
    bool ret = false;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
    ret = refcount_dec_and_mutex_lock(r, lock);
#else
    mutex_lock(lock);
    if (atomic_dec_and_test(r) <= 0) {
        mutex_unlock(lock);
        return false;
    }
    ret = true;
#endif

    return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0))
unsigned int available_instances_show(struct mdev_type *mtype)
{
    if (mtype == NULL || mtype->parent == NULL) {
        return 0;
    }

    return available_instances_ops(mtype->parent->dev, kobject_name(&mtype->kobj));
}

ssize_t description_show(struct mdev_type *mtype, char *buf)
{
    if (mtype == NULL || mtype->parent == NULL) {
        return 0;
    }

    return description_ops(mtype->parent->dev, kobject_name(&mtype->kobj), buf);
}
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0))
STATIC ssize_t device_api_show(struct mdev_type *mtype, struct mdev_type_attribute *attr,
                               char *buf)
{
    if (buf == NULL) {
        return 0;
    }

    return device_api_ops(buf);
}

STATIC ssize_t available_instances_show(struct mdev_type *mtype,
                                        struct mdev_type_attribute *attr,
                                        char *buf)
{
    unsigned int num = 0;
    struct kobject *kobj = NULL;

    if (buf == NULL) {
        return 0;
    }
    if (mtype == NULL) {
        goto out;
    }
    kobj = (struct kobject *)mtype;
    if (kobj->name == NULL) {
        goto out;
    }

    num = available_instances_ops(mtype_get_parent_dev(mtype),
                                  kobject_name(kobj));
out:
	return snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%u\n", num);
}

STATIC ssize_t description_show(struct mdev_type *mtype,
				                struct mdev_type_attribute *attr,
                                char *buf)
{
    struct kobject *kobj = NULL;

    if (mtype == NULL || buf == NULL) {
        return 0;
    }
    kobj = (struct kobject *)mtype;
    if (kobj->name == NULL) {
        return 0;
    }

    return description_ops(mtype_get_parent_dev(mtype),
                           kobject_name(kobj), buf);
}

STATIC ssize_t vfg_id_store(struct mdev_type *mtype,
                            struct mdev_type_attribute *attr,
                            const char *buf, size_t count)
{
    struct kobject *kobj = NULL;

    if (mtype == NULL || buf == NULL) {
        return 0;
    }
    kobj = (struct kobject *)mtype;
    if (kobj->name == NULL) {
        return 0;
    }

    return vfg_id_store_ops(mtype_get_parent_dev(mtype),
                            kobject_name(kobj), buf, count);
}
#else
STATIC ssize_t device_api_show(struct kobject *kobj, struct device *dev,
                               char *buf)
{
    if (buf == NULL) {
        return 0;
    }

    return device_api_ops(buf);
}

STATIC ssize_t available_instances_show(struct kobject *kobj, struct device *dev,
                                        char *buf)
{
    unsigned int num = 0;
    int ret = -1;

    if (buf == NULL) {
        return 0;
    }

    if (kobj == NULL || dev == NULL) {
        goto out;
    }

    num = available_instances_ops(dev, kobject_name(kobj));

out:
    ret = snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%u\n", num);
    return ret == -1 ? 0 : ret;
}

STATIC ssize_t description_show(struct kobject *kobj, struct device *dev,
                                char *buf)
{
    if (kobj == NULL || dev == NULL || buf == NULL) {
        return 0;
    }

    return description_ops(dev, kobject_name(kobj), buf);
}

STATIC ssize_t vfg_id_store(struct kobject *kobj, struct device *dev,
                            const char *buf, size_t count)
{
    if (kobj == NULL || dev == NULL || buf == NULL) {
        return -EINVAL;
    }

    return vfg_id_store_ops(dev, kobject_name(kobj), buf, count);
}
#endif /* KERNEL_VERSION(5,13,0) */
static MDEV_TYPE_ATTR_RO(device_api);
static MDEV_TYPE_ATTR_RO(available_instances);
static MDEV_TYPE_ATTR_RO(description);
static MDEV_TYPE_ATTR_WO(vfg_id);

struct attribute *hw_vdavinci_type_attrs[] = {
    &mdev_type_attr_device_api.attr,
    &mdev_type_attr_available_instances.attr,
    &mdev_type_attr_description.attr,
    &mdev_type_attr_vfg_id.attr,
    NULL
};

struct attribute **get_hw_vdavinci_type_attrs(void)
{
    return hw_vdavinci_type_attrs;
}
#endif /* KERNEL_VERSION(6,1,0) */

struct cpumask *vdavinci_get_cpumask(struct task_struct *task)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,3,0))
    return &(task->cpus_mask);
#else
    return &(task->cpus_allowed);
#endif
}

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)))
int vdavinci_rw_gpa(struct kvmdt_guest_info *info, unsigned long gpa,
                    void *buf, unsigned long len, bool write)
{
    int ret;
    int (*vfio_dma_rw_fn)(struct vfio_device *device, dma_addr_t iova,
                          void *data, size_t len, bool write);

    vfio_dma_rw_fn = symbol_get(vfio_dma_rw);
    if (!vfio_dma_rw_fn) {
        return -EINVAL;
    }
    ret = vfio_dma_rw_fn(info->vdavinci->vdev.vfio_device, gpa, buf, len, write);
    symbol_put(vfio_dma_rw);

    return ret;
}
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)))
int vdavinci_rw_gpa(struct kvmdt_guest_info *info, unsigned long gpa,
                    void *buf, unsigned long len, bool write)
{
    int ret;
    int (*vfio_dma_rw_fn)(struct vfio_group *group, dma_addr_t user_iova,
                          void *data, size_t len, bool write);

    vfio_dma_rw_fn = symbol_get(vfio_dma_rw);
    if (!vfio_dma_rw_fn) {
        return -EINVAL;
    }
    ret = vfio_dma_rw_fn(info->vdavinci->vdev.vfio_group, gpa, buf, len, write);
    symbol_put(vfio_dma_rw);
    return ret;
}
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)) || (defined(DRV_UT)))
int vdavinci_rw_gpa(struct kvmdt_guest_info *info, unsigned long gpa,
                    void *buf, unsigned long len, bool write)
{
    struct kvm *kvm = NULL;
    int idx, ret;
    bool kthread = (current->mm == NULL);
    mm_segment_t old_fs = get_fs();

    kvm = info->kvm;
    if (kthread) {
        if (!mmget_not_zero(kvm->mm)) {
            return -EFAULT;
        }
        vdavinci_use_mm(kvm->mm);
    }

    idx = srcu_read_lock(&kvm->srcu);
    set_fs(USER_DS);
    if (write) {
        ret = kvm_write_guest(kvm, gpa, buf, len);
    } else {
        ret = kvm_read_guest(kvm, gpa, buf, len);
    }
    set_fs(old_fs);
    srcu_read_unlock(&kvm->srcu, idx);

    if (kthread) {
        vdavinci_unuse_mm(kvm->mm);
        mmput(kvm->mm);
    }
    return ret;
}
#else
int vdavinci_rw_gpa(struct kvmdt_guest_info *info, unsigned long gpa,
                    void *buf, unsigned long len, bool write)
{
    return 0;
}
#endif

/*
 * copy_reserved_iova - copies the reserved between domains
 * @from: - source doamin from where to copy
 * @to: - destination domin where to copy
 * This function copies reserved iova's from one doamin to
 * other.
 */
void vdavinci_copy_reserved_iova(struct iova_domain *from, struct iova_domain *to)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,12,0))
#define IOVA_ANCHOR     ~0UL
    unsigned long flags;
    struct rb_node *node;
    struct iova *iova;
    struct iova *new_iova;

    spin_lock_irqsave(&from->iova_rbtree_lock, flags);
    for (node = rb_first(&from->rbroot); node; node = rb_next(node)) {
        iova = rb_entry(node, struct iova, node);

        if (iova->pfn_lo == IOVA_ANCHOR) {
            continue;
        }

        new_iova = reserve_iova(to, iova->pfn_lo, iova->pfn_hi);
        if (IS_ERR_OR_NULL(new_iova)) {
            printk(KERN_ERR "Reserve iova range %lx@%lx failed\n",
                   iova->pfn_lo, iova->pfn_hi);
        }
    }
    spin_unlock_irqrestore(&from->iova_rbtree_lock, flags);
#else
    copy_reserved_iova(from, to);
#endif
}
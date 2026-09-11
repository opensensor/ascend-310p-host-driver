/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <linux/version.h>
#define IS_VDAVINCI_KERNEL_VERSION_SUPPORT 1
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define PTR_ERR(p) ((long)(p))
#define ERR_PTR(e) ((void *)(intptr_t)(e))
#define vascend_err(...) ((void)0)
typedef uint64_t phys_addr_t;
struct device { int id; };
struct iommu_domain { int live; };
struct iommu_group { int refs, attached; };
struct iova_domain { int initialized; };
struct hw_vf_info { struct iommu_domain *domain; struct iova_domain iovad; };
struct hw_vdavinci { struct hw_vf_info vf; };
static struct device device;
static struct iommu_domain domain;
static struct iommu_group group;
static int alloc_error, attach_error, cookie_error, missing_group, sw_msi;
static struct device *vdavinci_resource_dev(struct hw_vdavinci *dev) { (void)dev; return &device; }
static struct iommu_domain *iommu_paging_domain_alloc(struct device *dev)
{
    assert(dev == &device);
    if (alloc_error) return ERR_PTR(alloc_error);
    assert(!domain.live); domain.live = 1; return &domain;
}
static void iommu_domain_free(struct iommu_domain *dom)
{
    assert(dom == &domain && dom->live && !group.attached);
    dom->live = 0;
}
static struct iommu_group *iommu_group_get(struct device *dev)
{
    assert(dev == &device);
    if (missing_group) return NULL;
    group.refs++; return &group;
}
static void iommu_group_put(struct iommu_group *grp) { assert(grp == &group && grp->refs-- == 1); }
static int iommu_attach_group(struct iommu_domain *dom, struct iommu_group *grp)
{
    assert(dom->live && grp->refs && !grp->attached);
    if (attach_error) return attach_error;
    grp->attached = 1; return 0;
}
static void iommu_detach_group(struct iommu_domain *dom, struct iommu_group *grp)
{
    assert(dom->live && grp->refs && grp->attached);
    grp->attached = 0;
}
static bool vfio_iommu_has_sw_msi(struct iommu_group *grp, phys_addr_t *base)
{
    assert(grp->refs && grp->attached); *base = 0x8000; return sw_msi;
}
static int iommu_get_msi_cookie(struct iommu_domain *dom, phys_addr_t base)
{
    assert(dom->live && base == 0x8000); return cookie_error;
}
static void init_vf_iovad(struct hw_vdavinci *dev) { assert(!dev->vf.iovad.initialized++); }
static void put_iova_domain(struct iova_domain *iovad) { assert(iovad->initialized-- == 1); }
#include "iommu_lifecycle_test.inc"

int main(void)
{
    struct hw_vdavinci dev = { 0 };
    hw_vdavinci_iommu_detach_group(&dev); /* Cleanup after failed allocation. */
    alloc_error = -EOPNOTSUPP;
    assert(hw_vdavinci_iommu_attach_group(&dev) == -EOPNOTSUPP);
    assert(!dev.vf.domain && !domain.live && !group.refs);
    alloc_error = 0; missing_group = 1;
    assert(hw_vdavinci_iommu_attach_group(&dev) == -ENODEV);
    assert(!dev.vf.domain && !domain.live && !group.refs);
    missing_group = 0; attach_error = -EBUSY;
    assert(hw_vdavinci_iommu_attach_group(&dev) == -EBUSY);
    assert(!dev.vf.domain && !domain.live && !group.refs && !group.attached);
    hw_vdavinci_iommu_detach_group(&dev);
    attach_error = 0; sw_msi = 1; cookie_error = -ENOMEM;
    assert(hw_vdavinci_iommu_attach_group(&dev) == -ENOMEM);
    assert(!dev.vf.domain && !domain.live && !group.refs && !group.attached);
    cookie_error = 0;
    assert(hw_vdavinci_iommu_attach_group(&dev) == 0);
    assert(domain.live && group.attached && !group.refs && dev.vf.iovad.initialized);
    assert(hw_vdavinci_iommu_attach_group(&dev) == -EEXIST);
    hw_vdavinci_iommu_detach_group(&dev);
    assert(!dev.vf.domain && !domain.live && !group.refs && !group.attached);
    assert(!dev.vf.iovad.initialized);
    hw_vdavinci_iommu_detach_group(&dev);
    return 0;
}

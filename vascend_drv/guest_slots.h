/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef VDAVINCI_GUEST_SLOTS_H
#define VDAVINCI_GUEST_SLOTS_H

#include <linux/kvm_host.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
/* Preserve KVM visibility semantics without calling a KVM-private export.
 * The header-defined slot lookup requires SRCU across lookup and inspection.
 * Visibility alone does not authorize DMA; VFIO validates and pins its IOVA.
 */
static inline bool vdavinci_guest_gfn_visible(struct kvm *kvm, unsigned long gfn)
{
    struct kvm_memory_slot *slot;
    bool visible;
    int idx;

    if (!kvm)
        return false;
    idx = srcu_read_lock(&kvm->srcu);
    slot = __gfn_to_memslot(kvm_memslots(kvm), gfn);
    visible = kvm_is_visible_memslot(slot);
    srcu_read_unlock(&kvm->srcu, idx);
    return visible;
}
#endif

#endif /* VDAVINCI_GUEST_SLOTS_H */

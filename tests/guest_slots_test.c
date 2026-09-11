/* SPDX-License-Identifier: GPL-2.0-only */
#include "guest_slots.h"

int main(void)
{
    struct kvm_memory_slot slots[] = {
        { .base_gfn = 10, .npages = 2, .id = 0 },
        { .base_gfn = 20, .npages = 2, .id = 1, .flags = KVM_MEMSLOT_INVALID },
        { .base_gfn = 30, .npages = 2, .id = KVM_USER_MEM_SLOTS },
        { .base_gfn = 40, .npages = 2, .id = 2, .flags = KVM_MEM_READONLY },
    };
    struct kvm kvm = { .memslots = { .slots = slots, .count = 4 } };
    kvm.memslots.srcu = &kvm.srcu;
    assert(!vdavinci_guest_gfn_visible(NULL, 10));
    assert(!vdavinci_guest_gfn_visible(&kvm, 9));
    assert(vdavinci_guest_gfn_visible(&kvm, 10));
    assert(vdavinci_guest_gfn_visible(&kvm, 11));
    assert(!vdavinci_guest_gfn_visible(&kvm, 12));
    assert(!vdavinci_guest_gfn_visible(&kvm, 20));
    assert(!vdavinci_guest_gfn_visible(&kvm, 30));
    assert(vdavinci_guest_gfn_visible(&kvm, 40));
    assert(kvm.srcu.entries == 7 && kvm.srcu.exits == 7 && kvm.srcu.depth == 0);
    kvm.memslots.count = 0;
    assert(!vdavinci_guest_gfn_visible(&kvm, 10));
    assert(kvm.srcu.entries == 8 && kvm.srcu.exits == 8 && kvm.srcu.depth == 0);
    return 0;
}

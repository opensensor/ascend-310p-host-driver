/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TEST_LINUX_KVM_HOST_H
#define TEST_LINUX_KVM_HOST_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#define KVM_USER_MEM_SLOTS 512
#define KVM_MEMSLOT_INVALID 1
#define KVM_MEM_READONLY 2
struct srcu_struct { int depth, entries, exits; };
struct kvm_memory_slot { unsigned long base_gfn, npages; int id, flags; };
struct kvm_memslots {
    struct kvm_memory_slot *slots;
    size_t count;
    struct srcu_struct *srcu;
};
struct kvm { struct srcu_struct srcu; struct kvm_memslots memslots; };

static inline int srcu_read_lock(struct srcu_struct *srcu)
{
    assert(srcu->depth++ == 0);
    srcu->entries++;
    return 17;
}

static inline void srcu_read_unlock(struct srcu_struct *srcu, int idx)
{
    assert(idx == 17 && srcu->depth-- == 1);
    srcu->exits++;
}

static inline struct kvm_memslots *kvm_memslots(struct kvm *kvm)
{
    assert(kvm->srcu.depth == 1);
    return &kvm->memslots;
}

static inline struct kvm_memory_slot *__gfn_to_memslot(struct kvm_memslots *slots,
                                                       unsigned long gfn)
{
    assert(slots->srcu->depth == 1);
    for (size_t i = 0; i < slots->count; i++) {
        struct kvm_memory_slot *slot = &slots->slots[i];
        if (gfn >= slot->base_gfn && gfn - slot->base_gfn < slot->npages)
            return slot;
    }
    return NULL;
}

static inline bool kvm_is_visible_memslot(struct kvm_memory_slot *slot)
{
    return slot && slot->id < KVM_USER_MEM_SLOTS && !(slot->flags & KVM_MEMSLOT_INVALID);
}
#endif

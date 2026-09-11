/* SPDX-License-Identifier: GPL-2.0-only */
/* Kernel operations are mocked; the functions under test are compiled verbatim
 * from the driver sources by test_port.py, including their cleanup branches.
 */
#include <assert.h>
#include <stdlib.h>
#include "guest_dma_range.h"

#define STATIC
#define typeof __typeof__
#define GFP_KERNEL 0
#define DOMAIN_PIN_STATUS_INVALID 0
#define DOMAIN_PIN_STATUS_READY 1
#define vascend_info(dev, ...) ((void)(dev))
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
struct list_head { struct list_head *next, *prev; };
#define INIT_LIST_HEAD(h) ((h)->next = (h)->prev = (h))
#define list_entry(p, type, member) container_of(p, type, member)
#define list_for_each_entry(p, h, member) \
    for (p = list_entry((h)->next, __typeof__(*p), member); &p->member != (h); \
         p = list_entry(p->member.next, __typeof__(*p), member))
#define list_for_each_entry_safe(p, n, h, member) \
    for (p = list_entry((h)->next, __typeof__(*p), member), \
         n = list_entry(p->member.next, __typeof__(*p), member); &p->member != (h); \
         p = n, n = list_entry(n->member.next, __typeof__(*n), member))
static void list_add_tail(struct list_head *node, struct list_head *head)
{
    node->next = head; node->prev = head->prev;
    head->prev->next = node; head->prev = node;
}
static void list_del(struct list_head *node)
{
    node->prev->next = node->next; node->next->prev = node->prev;
}
static void list_del_init(struct list_head *node) { list_del(node); INIT_LIST_HEAD(node); }
struct mutex { int initialized, depth; };
struct rw_semaphore { int depth; };
static int sem_write_locks;
static void mutex_lock(struct mutex *m) { assert(m->initialized && m->depth++ == 0); }
static void mutex_unlock(struct mutex *m) { assert(m->depth-- == 1); }
static void init_rwsem(struct rw_semaphore *s) { s->depth = 0; }
static void down_write(struct rw_semaphore *s) { assert(s->depth++ == 0); sem_write_locks++; }
static void up_write(struct rw_semaphore *s) { assert(s->depth-- == 1); }
struct kref { int refcount; };
static void kref_init(struct kref *r) { r->refcount = 1; }
static void kref_get(struct kref *r) { assert(r->refcount > 0); r->refcount++; }
static bool vdavinci_refcount_mutex_lock(int *r, struct mutex *m)
{
    assert(*r > 0);
    if (--*r) return false;
    mutex_lock(m);
    return true;
}
struct iova_domain { int initialized; };
static void vdavinci_init_iova_domain(struct iova_domain *d) { d->initialized = 1; }
static void put_iova_domain(struct iova_domain *d) { assert(d->initialized-- == 1); }
static int allocations, fail_allocation, unpins, unmaps, open_result;
static void *kzalloc(size_t size, int flags)
{
    (void)flags;
    if (fail_allocation && !--fail_allocation) return NULL;
    void *p = calloc(1, size); assert(p); allocations++; return p;
}
static void kfree(void *p) { if (p) { allocations--; free(p); } }
static int atomic_read(const int *v) { return *v; }
struct kvm { int id; };
struct hw_vdavinci;
struct mdev_device { struct hw_vdavinci *vdavinci; };
struct vfio_device { struct kvm *kvm; struct mdev_device *dev; };
struct ram_range_info {
    unsigned long base_gfn, npages;
    int map_count, pinned;
    struct list_head list;
};
struct ram_range_info_list { struct list_head head; };
struct dev_dom_info {
    struct { void (*dev_dma_unmap_ram_range)(struct hw_vdavinci *, struct ram_range_info *); } ops;
};
struct vm_dom_info {
    struct list_head node;
    struct kvm *kvm;
    struct vfio_device *pin_owner;
    int status;
    struct rw_semaphore sem;
    struct kref ref;
    struct list_head dev_dom_list_head;
    struct ram_range_info_list *ram_info_list;
    struct iova_domain iovad;
    struct dev_dom_info *mock_dev;
};
struct hw_vdavinci {
    struct { struct kvm *kvm; struct vm_dom_info *domain; struct mutex cache_lock; } vdev;
};
struct device { int id; };
static struct device *vdavinci_get_device(struct hw_vdavinci *dev) { (void)dev; return NULL; }
static struct list_head g_vm_domains;
static struct mutex g_vm_domains_lock = { .initialized = 1 };
static struct mutex *get_vm_domains_lock(void) { return &g_vm_domains_lock; }
static struct mdev_device *get_mdev_device(struct mdev_device *dev) { return dev; }
static struct hw_vdavinci *vfio_dev_to_vnpu(struct vfio_device *dev) { return dev->dev->vdavinci; }
static int hw_vdavinci_open(struct mdev_device *mdev) { (void)mdev; return open_result; }
static struct dev_dom_info *dev_dom_info_find(struct vm_dom_info *dom, struct hw_vdavinci *dev)
{
    assert(dev->vdev.cache_lock.depth == 1 && dom->sem.depth == 1);
    return dom->mock_dev;
}
static void hw_vdavinci_unpin_page_range(struct hw_vdavinci *, struct ram_range_info *);
static void dma_dom_pool_unpin(struct hw_vdavinci *, struct vm_dom_info *);

#include "virtualization_lifecycle_test.inc"

static void unmap_range(struct hw_vdavinci *dev, struct ram_range_info *ram)
{
    assert(dev->vdev.cache_lock.depth == 1 && ram->map_count == 1 && ram->pinned);
    ram->map_count--;
    unmaps++;
}
static void hw_vdavinci_unpin_page_range(struct hw_vdavinci *dev, struct ram_range_info *ram)
{
    assert(dev->vdev.cache_lock.depth == 1 && ram->map_count == 0 && ram->pinned);
    ram->pinned = 0;
    unpins++;
}
static void dma_dom_pool_unpin(struct hw_vdavinci *dev, struct vm_dom_info *dom)
{
    struct ram_range_info *ram, *next;
    assert(dev->vdev.cache_lock.depth == 1 && !dev->vdev.domain);
    assert(dom->sem.depth == 1);
    assert(!g_vm_domains_lock.depth);
    assert(!vm_dom_info_find(dom->kvm, dom->pin_owner)); /* No resurrection. */
    list_for_each_entry_safe(ram, next, &dom->ram_info_list->head, list) {
        hw_vdavinci_unpin_page_range(dev, ram);
        list_del(&ram->list);
        kfree(ram);
    }
}
static void add_ram(struct vm_dom_info *dom, unsigned long base, int mapped)
{
    struct ram_range_info *ram = kzalloc(sizeof(*ram), GFP_KERNEL);
    ram->base_gfn = base; ram->npages = 2;
    ram->map_count = mapped; ram->pinned = 1;
    dom->status = DOMAIN_PIN_STATUS_READY;
    list_add_tail(&ram->list, &dom->ram_info_list->head);
}

int main(void)
{
    struct kvm kvm = { .id = 1 };
    struct hw_vdavinci a = { .vdev.cache_lock.initialized = 1 };
    struct hw_vdavinci b = { .vdev.cache_lock.initialized = 1 };
    struct mdev_device ma = { &a }, mb = { &b };
    struct vfio_device fa = { &kvm, &ma }, fb = { &kvm, &mb };
    struct dev_dom_info mapper = { .ops.dev_dma_unmap_ram_range = unmap_range };
    INIT_LIST_HEAD(&g_vm_domains);

    /* Callbacks before open and after failed open must be harmless. */
    hw_vdavinci_vfio_dma_unmap(&fa, 0, ~(u64)0);
    for (int failure = 1; failure <= 2; failure++) {
        fail_allocation = failure;
        assert(hw_vdavinci_vfio_open(&fa) == -ENOMEM);
        assert(!a.vdev.domain && !a.vdev.kvm && allocations == 0);
    }
    open_result = -EBUSY;
    assert(hw_vdavinci_vfio_open(&fa) == -EBUSY);
    assert(!a.vdev.domain && !a.vdev.kvm && allocations == 0);
    assert(sem_write_locks == 1); /* Failed-open teardown still drains readers. */
    hw_vdavinci_vfio_dma_unmap(&fa, 0, ~(u64)0);
    open_result = 0;

    assert(hw_vdavinci_vfio_open(&fa) == 0);
    assert(hw_vdavinci_vfio_open(&fb) == 0);
    assert(a.vdev.domain != b.vdev.domain); /* Same KVM, different VFIO pin owners. */
    assert(a.vdev.domain->pin_owner == &fa && b.vdev.domain->pin_owner == &fb);
    a.vdev.domain->mock_dev = b.vdev.domain->mock_dev = &mapper;
    add_ram(a.vdev.domain, 10, 1); add_ram(a.vdev.domain, 20, 1);
    add_ram(a.vdev.domain, 30, 1); add_ram(b.vdev.domain, 10, 1);
    hw_vdavinci_vfio_dma_unmap(&fa, 11 * PAGE_SIZE, 11 * PAGE_SIZE);
    assert(unpins == 2 && unmaps == 2); /* Spans two cached RAM slots. */
    assert(allocations == 6); /* Two domains, two lists, two remaining slots. */
    hw_vdavinci_vfio_dma_unmap(&fa, 0, ~(u64)0);
    assert(unpins == 3 && unmaps == 3); /* Does not release B's pins. */
    hw_vdavinci_vfio_dma_unmap(&fa, 0, ~(u64)0); /* Repeated callback. */
    hw_vdavinci_vfio_dma_unmap(&fb, 10 * PAGE_SIZE + 1, 1); /* Retire whole slot. */
    assert(unpins == 4 && unmaps == 4);
    add_ram(a.vdev.domain, 40, 0); /* Pinned but never DMA-mapped, e.g. open error. */
    hw_vdavinci_release_vm_domain(&a);
    hw_vdavinci_vfio_dma_unmap(&fa, 0, ~(u64)0);
    hw_vdavinci_release_vm_domain(&a); /* Idempotent teardown. */
    hw_vdavinci_release_vm_domain(&b);
    assert(unpins == 5 && allocations == 0 && g_vm_domains.next == &g_vm_domains);

    /* Legacy NULL owners still share references; non-last release detaches A. */
    a.vdev.domain = vm_dom_info_get(&kvm, NULL);
    b.vdev.domain = vm_dom_info_get(&kvm, NULL);
    assert(a.vdev.domain == b.vdev.domain && a.vdev.domain->ref.refcount == 2);
    hw_vdavinci_release_vm_domain(&a);
    assert(!a.vdev.domain && b.vdev.domain->ref.refcount == 1);
    hw_vdavinci_release_vm_domain(&b);
    assert(allocations == 0 && g_vm_domains.next == &g_vm_domains);
    assert(!a.vdev.cache_lock.depth && !b.vdev.cache_lock.depth && !g_vm_domains_lock.depth);
    return 0;
}

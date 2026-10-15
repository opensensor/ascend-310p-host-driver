// SPDX-License-Identifier: GPL-2.0-only
/* Execute the driver's pool planner and dispatch functions with no hardware. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "ascend_device_dma.h"

typedef uint32_t u32;
typedef uint64_t u64;
#define READ_ONCE(x) (x)
#define WRITE_ONCE(x, value) ((x) = (value))
#define DEVDRV_DMA_CHAN_ENABLED 1
static void lock(int *held) { assert(!*held); *held = 1; }
static void unlock(int *held) { assert(*held); *held = 0; }
#define spin_lock_bh lock
#define spin_unlock_bh unlock
#define spin_lock lock
#define spin_unlock unlock
#define STATIC static
#define HISI_MINI_V2 2
#define DEVDRV_PHY_BOOT 0
#define DEVDRV_DMA_REMOTE_SIDE 1
#define DEVDRV_DMA_DATA_COMM_CHAN_NUM 1
#define DEVDRV_DMA_DATA_PCIE_MSG_CHAN_NUM 1
#define DEVDRV_INVALID_INSTANCE (-1)
#define DEVDRV_DEVICE_ALIVE 1
#define DEVDRV_DEVICE_SUSPEND 2
#define DEVDRV_DMA_ALIVE 1
#define DEVDRV_DMA_HOST_TO_DEVICE 1
#define devdrv_err(...) ((void)0)
#define devdrv_err_spinlock(...) ((void)0)
#define devdrv_warn_spinlock(...) ((void)0)
#define devdrv_debug_spinlock(...) ((void)0)

enum devdrv_dma_data_type {
    DEVDRV_DMA_DATA_COMMON, DEVDRV_DMA_DATA_PCIE_MSG,
    DEVDRV_DMA_DATA_TRAFFIC, DEVDRV_DMA_DATA_MANAGE, DEVDRV_DMA_DATA_TYPE_MAX
};
struct data_type_chan { u32 chan_start_id, chan_num, last_use_chan; };
struct devdrv_dma_channel {
    struct device *dev;
    bool device_dma_reserved;
    struct devdrv_dma_channel *host_dma_redirect;
    u32 dma_data_type, sq_head, sq_tail, chan_status, chan_id, sq_depth, cq_depth;
    int cq_lock, lock;
};
struct devdrv_device_dma_file {
    struct devdrv_dma_dev *dma_dev;
    u64 generation;
    u32 entry;
    struct devdrv_device_dma_source *source;
    u64 source_sequence;
};
struct devdrv_pci_ctrl { u32 device_status, chip_type, is_sriov_enabled; };
struct list_head { struct list_head *next, *prev; };
static void list_add_tail(struct list_head *node, struct list_head *head) {
    node->prev = head->prev; node->next = head;
    head->prev->next = node; head->prev = node;
}
static void list_del_init(struct list_head *node) {
    node->prev->next = node->next; node->next->prev = node->prev;
    node->next = node; node->prev = node;
}
#define container_of(p, type, member) ((type *)((char *)(p) - offsetof(type, member)))
#define list_for_each_entry(pos, head, member) \
    for (struct list_head *cursor = (head)->next; \
         cursor != (head) && ((pos) = container_of(cursor, struct devdrv_dma_dev, member), 1); \
         cursor = cursor->next)
struct devdrv_dma_dev {
    u32 dev_id, remote_chan_num, sq_cq_side, dev_status;
    struct data_type_chan data_chan[DEVDRV_DMA_DATA_TYPE_MAX];
    struct devdrv_dma_channel *dma_chan;
    struct devdrv_pci_ctrl *pci_ctrl;
    struct devdrv_device_dma_file *device_dma_owner;
    struct list_head device_dma_registry;
    bool device_dma_registered;
};
struct devdrv_dma_node { u32 direction; };
struct devdrv_dma_copy_para { int type, instance, copy_type; };
static struct devdrv_dma_dev *active_dev;
static unsigned int dispatch_count;
static bool dispatch_full;
static struct devdrv_dma_channel *last_dispatch;
static void (*before_dispatch)(struct devdrv_dma_channel *);
static void reserve_before_dispatch(struct devdrv_dma_channel *chan);
static struct devdrv_dma_dev *devdrv_get_dma_dev(u32 dev_id)
{
    assert(dev_id == active_dev->dev_id);
    return active_dev;
}
static int devdrv_dma_chan_copy(u32 id, struct devdrv_dma_channel *chan,
    struct devdrv_dma_node *node, u32 count, struct devdrv_dma_copy_para *para)
{
    (void)id; (void)node; (void)count; (void)para;
    if (before_dispatch) before_dispatch(chan);
    if (chan->device_dma_reserved || chan->host_dma_redirect) return -EBUSY;
    last_dispatch = chan;
    if (dispatch_full) return -ENOSPC;
    dispatch_count++;
    return 0;
}
#define __user
#define GFP_KERNEL 0
#define U64_MAX UINT64_MAX
#define DEVDRV_SRIOV_DISABLE 0
#define mutex_lock lock
#define mutex_unlock unlock
static int device_dma_mutex;
static struct list_head device_dma_devices = {&device_dma_devices, &device_dma_devices};
static u64 device_dma_generation;
static int fail_read, fail_write, fail_alloc, fail_open, allocations;
struct file { void *private_data; };
struct inode { int unused; };
static unsigned long copy_from_user(void *out, const void *in, size_t size) {
    if (fail_read) return size;
    memcpy(out, in, size); return 0;
}
static unsigned long copy_to_user(void *out, const void *in, size_t size) {
    if (fail_write) return size;
    memcpy(out, in, size); return 0;
}
static void *kzalloc(size_t size, int flags) {
    (void)flags;
    if (fail_alloc) return NULL;
    void *result = calloc(1, size); assert(result); allocations++; return result;
}
static void kfree(void *ptr) { if (ptr) allocations--; free(ptr); }
static int nonseekable_open(struct inode *inode, struct file *file) {
    (void)inode; (void)file; return fail_open ? -ENODEV : 0;
}
static u32 devdrv_get_env_boot_type(u32 id) { (void)id; return DEVDRV_PHY_BOOT; }
struct devdrv_device_dma_source { struct ascend_device_dma_source info; };
static int source_failure;
static int source_live;
static int devdrv_device_dma_source_create(struct device *dev,
    const struct ascend_device_dma_source *req, struct devdrv_device_dma_source **out)
{
    (void)dev;
    if (source_failure) return source_failure;
    *out = kzalloc(sizeof(**out), GFP_KERNEL);
    if (!*out) return -ENOMEM;
    (*out)->info = *req;
    (*out)->info.page_count = 2;
    (*out)->info.segment_count = 1;
    source_live++;
    return 0;
}
static void devdrv_device_dma_source_destroy(struct devdrv_device_dma_source *source)
{
    if (source) { source_live--; kfree(source); }
}
static void devdrv_device_dma_source_describe(const struct devdrv_device_dma_source *source,
    struct ascend_device_dma_source *reply) { *reply = source->info; }
static void test_source_lifecycle(struct file *file, u64 generation);
static void test_ioctl_lifecycle(void);
_Static_assert(ASCEND_DEVICE_DMA_ACQUIRE == 0xc04044d0U, "acquire ioctl ABI");
_Static_assert(ASCEND_DEVICE_DMA_QUERY == 0xc04044d1U, "query ioctl ABI");
_Static_assert(ASCEND_DEVICE_DMA_RELEASE == 0x404044d2U, "release ioctl ABI");
_Static_assert(sizeof(struct ascend_device_dma_lease) == 64, "stable 64-byte ABI");
_Static_assert(offsetof(struct ascend_device_dma_lease, generation) == 16, "aligned generation");
_Static_assert(_IOC_SIZE(ASCEND_DEVICE_DMA_ACQUIRE) == 64, "ioctl ABI size");
#include "device_dma_reservation_test.inc"

static struct devdrv_dma_dev fresh(struct devdrv_dma_channel *channels, u32 n,
    struct devdrv_pci_ctrl *pci)
{
    struct devdrv_dma_dev dev = {
        .remote_chan_num = n, .sq_cq_side = DEVDRV_DMA_REMOTE_SIDE,
        .dev_status = DEVDRV_DMA_ALIVE, .dma_chan = channels, .pci_ctrl = pci
    };
    memset(channels, 0, sizeof(*channels) * n);
    for (u32 i = 0; i < n; ++i) {
        channels[i].chan_status = DEVDRV_DMA_CHAN_ENABLED;
        channels[i].chan_id = i + 16;
        channels[i].sq_depth = 64;
        channels[i].cq_depth = 64;
    }
    devdrv_res_dma_traffic(&dev);
    return dev;
}
static void rejected_unchanged(struct devdrv_dma_dev *dev, int error)
{
    struct devdrv_dma_dev before = *dev;
    struct devdrv_dma_channel channels[32];
    u32 entry = UINT32_MAX;
    memcpy(channels, dev->dma_chan, dev->remote_chan_num * sizeof(*channels));
    assert(devdrv_device_dma_choose_channel(dev, &entry) == error);
    assert(entry == UINT32_MAX);
    assert(memcmp(&before, dev, sizeof(before)) == 0);
    assert(memcmp(channels, dev->dma_chan, dev->remote_chan_num * sizeof(*channels)) == 0);
}
int main(void)
{
    struct devdrv_dma_channel channels[32];
    struct devdrv_pci_ctrl pci = { .device_status = DEVDRV_DEVICE_ALIVE };
    struct devdrv_dma_node node = { .direction = DEVDRV_DMA_HOST_TO_DEVICE };
    unsigned int n, type, step;

    for (n = 1; n <= 32; n++) {
        struct devdrv_dma_dev dev = fresh(channels, n, &pci);
        struct devdrv_device_dma_file owner = {0};
        u32 traffic_before = dev.data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num;
        u32 entry;
        active_dev = &dev;
        /* Default startup preserves every channel and every fixed instance. */
        for (step = 0; step < traffic_before; ++step) {
            struct devdrv_dma_copy_para para = {.type = DEVDRV_DMA_DATA_TRAFFIC,
                .instance = (int)step};
            assert(devdrv_dma_copy(&dev, &node, 1, &para) == 0);
        }
        if (n < 4) {
            rejected_unchanged(&dev, -ENOSPC);
            continue;
        }
        dev.data_chan[DEVDRV_DMA_DATA_MANAGE] = (struct data_type_chan){n - 1, 1, 0};
        rejected_unchanged(&dev, -EBUSY);
        dev.data_chan[DEVDRV_DMA_DATA_MANAGE] = (struct data_type_chan){UINT32_MAX, 2, 0};
        rejected_unchanged(&dev, -EINVAL);
        dev.data_chan[DEVDRV_DMA_DATA_MANAGE] = (struct data_type_chan){0};
        assert(devdrv_device_dma_choose_channel(&dev, &entry) == 0);
        assert(entry == n - 1);
        channels[entry].sq_tail = 1;
        assert(devdrv_device_dma_try_claim(&channels[entry], &channels[dev.data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id]) == -EBUSY);
        assert(!channels[entry].device_dma_reserved && !channels[entry].host_dma_redirect);
        assert(!channels[entry].cq_lock && !channels[entry].lock);
        channels[entry].sq_tail = 0;
        channels[entry].chan_status = 0;
        assert(devdrv_device_dma_try_claim(&channels[entry], &channels[dev.data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id]) == -ENODEV);
        channels[entry].chan_status = DEVDRV_DMA_CHAN_ENABLED;
        assert(devdrv_device_dma_try_claim(&channels[entry], &channels[dev.data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id]) == 0);
        assert(devdrv_device_dma_try_claim(&channels[entry], &channels[dev.data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id]) == -EBUSY);
        owner.dma_dev = &dev;
        owner.entry = entry;
        dev.device_dma_owner = &owner;
        assert(dev.data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num == traffic_before);
        rejected_unchanged(&dev, -EBUSY);
        for (type = 0; type < DEVDRV_DMA_DATA_TYPE_MAX; type++) {
            struct data_type_chan *pool = &dev.data_chan[type];
            if (!pool->chan_num) continue;
            for (step = 0; step < 1000; step++) {
                struct devdrv_dma_copy_para para = {.type = (int)type,
                    .instance = step % 2 ? DEVDRV_INVALID_INSTANCE : (int)(step * 100003)};
                assert(!devdrv_dma_get_chan(0, type)->device_dma_reserved);
                assert(!devdrv_dma_get_chan_by_type(&dev, type)->device_dma_reserved);
                assert(devdrv_dma_copy(&dev, &node, 1, &para) == 0);
            }
        }
        {
            struct devdrv_dma_copy_para para = {.type = DEVDRV_DMA_DATA_TRAFFIC,
                .instance = DEVDRV_INVALID_INSTANCE};
            dispatch_full = true;
            assert(devdrv_dma_copy(&dev, &node, 1, &para) == -ENOSPC);
            dispatch_full = false;
        }
        channels[entry].sq_tail = 1;
        assert(devdrv_device_dma_drop(&owner) == -EBUSY);
        assert(channels[entry].device_dma_reserved);
        assert(owner.dma_dev == &dev);
        channels[entry].sq_tail = 0;
        assert(devdrv_device_dma_drop(&owner) == 0);
        assert(!channels[entry].device_dma_reserved && owner.dma_dev == NULL);
        assert(dev.device_dma_owner == NULL);
        assert(devdrv_device_dma_drop(&owner) == -ENODEV);
        for (step = 0; step < traffic_before; ++step) {
            struct devdrv_dma_copy_para para = {.type = DEVDRV_DMA_DATA_TRAFFIC,
                .instance = (int)step};
            assert(devdrv_dma_copy(&dev, &node, 1, &para) == 0);
        }
    }
    {
        struct devdrv_dma_dev dev = fresh(channels, 5, &pci);
        dev.data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num = UINT32_MAX;
        rejected_unchanged(&dev, -ENOSPC);
    }
    {
        struct devdrv_dma_dev dev = fresh(channels, 5, &pci);
        struct devdrv_dma_copy_para para = {.type = DEVDRV_DMA_DATA_TRAFFIC, .instance = 2};
        struct devdrv_device_dma_file owner = {.dma_dev = &dev, .entry = 4};
        active_dev = &dev;
        /* Force acquire between the unlocked route read and SQ submission. */
        before_dispatch = reserve_before_dispatch;
        assert(devdrv_dma_copy(&dev, &node, 1, &para) == 0);
        assert(last_dispatch == &channels[2]);
        assert(devdrv_device_dma_drop(&owner) == 0);
        assert(channels[4].host_dma_redirect == &channels[2]);
        /* Release must not migrate back while the successor has work. */
        channels[2].sq_tail = 1;
        assert(devdrv_dma_copy(&dev, &node, 1, &para) == 0);
        assert(last_dispatch == &channels[2]);
        dispatch_full = true;
        assert(devdrv_dma_copy(&dev, &node, 1, &para) == -ENOSPC);
        dispatch_full = false;
        assert(devdrv_dma_chan_copy(0, &channels[4], &node, 1, &para) == -EBUSY);
        for (step = 0; step < 30; step++) {
            assert(devdrv_dma_get_chan(0, DEVDRV_DMA_DATA_TRAFFIC) != &channels[4]);
            assert(devdrv_dma_get_chan_by_type(&dev, DEVDRV_DMA_DATA_TRAFFIC) != &channels[4]);
        }
        assert(devdrv_device_dma_try_claim(&channels[4], &channels[2]) == 0);
        assert(devdrv_dma_copy(&dev, &node, 1, &para) == 0);
        assert(last_dispatch == &channels[2]);
    }
    test_ioctl_lifecycle();
    printf("dynamic lease lifecycle and %u successful host dispatches passed\n", dispatch_count);
    return 0;
}

static void reserve_before_dispatch(struct devdrv_dma_channel *chan)
{
    before_dispatch = NULL;
    assert(chan == &active_dev->dma_chan[4]);
    assert(devdrv_device_dma_try_claim(chan, &active_dev->dma_chan[2]) == 0);
}

static long invoke(struct file *file, unsigned int command, struct ascend_device_dma_lease *request)
{
    long ret = devdrv_device_dma_ioctl(file, command, (unsigned long)request);
    assert(!device_dma_mutex);
    return ret;
}
static struct ascend_device_dma_lease acquire_request(u32 id)
{
    return (struct ascend_device_dma_lease){.version = ASCEND_DEVICE_DMA_ABI_VERSION, .dev_id = id};
}
static void test_ioctl_lifecycle(void)
{
    struct devdrv_dma_channel channels[5], replacement_channels[5];
    struct devdrv_pci_ctrl pci = {.device_status = DEVDRV_DEVICE_ALIVE, .chip_type = HISI_MINI_V2};
    struct devdrv_dma_dev dev = fresh(channels, 5, &pci), replacement;
    struct file first = {0}, second = {0};
    struct inode inode = {0};
    struct ascend_device_dma_lease req, saved, stale;
    struct devdrv_device_dma_file *owner;
    const unsigned last = 4;

    devdrv_device_dma_attach(&dev);
    assert(dev.device_dma_registered && !channels[last].device_dma_reserved);
    fail_alloc = 1;
    assert(devdrv_device_dma_open(&inode, &first) == -ENOMEM);
    fail_alloc = 0; fail_open = 1;
    assert(devdrv_device_dma_open(&inode, &first) == -ENODEV && !allocations);
    fail_open = 0;
    assert(devdrv_device_dma_open(&inode, &first) == 0);
    assert(devdrv_device_dma_open(&inode, &second) == 0);
    owner = first.private_data;
    req = acquire_request(0);
    assert(invoke(&first, ASCEND_DEVICE_DMA_QUERY, &req) == -ENODEV);
    assert(invoke(&first, 0, &req) == -ENOTTY);
    fail_read = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EFAULT);
    fail_read = 0;
    req.version++;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EINVAL);
    req = acquire_request(0); req.flags = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EINVAL);
    req = acquire_request(0); req.reserved[2] = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EINVAL);
    req = acquire_request(0); req.generation = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EINVAL);
    req = acquire_request(1);
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -ENODEV);
    req = acquire_request(0);
    device_dma_generation = UINT64_MAX;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EOVERFLOW);
    device_dma_generation = 0;
    pci.device_status = 0;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -ENODEV);
    pci.device_status = DEVDRV_DEVICE_ALIVE; pci.is_sriov_enabled = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -ENODEV);
    pci.is_sriov_enabled = 0;
    channels[last].sq_tail = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EBUSY);
    assert(!channels[last].device_dma_reserved && !owner->dma_dev);
    channels[last].sq_tail = 0;
    fail_write = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EFAULT);
    assert(!channels[last].device_dma_reserved && !owner->dma_dev);
    fail_write = 0;
    assert(channels[last].host_dma_redirect == &channels[2]);
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == 0);
    assert(req.generation && req.capabilities == (ASCEND_DEVICE_DMA_CAP_HOST_ROUTING_ISOLATED |
        ASCEND_DEVICE_DMA_CAP_SOURCE_REGISTRATION) && channels[last].device_dma_reserved);
    test_source_lifecycle(&first, req.generation);
    saved = req;
    req = acquire_request(0);
    assert(invoke(&second, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EBUSY);
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -EBUSY);
    assert(devdrv_device_dma_suspend(&dev) == -EBUSY && dev.device_dma_registered);
    req = saved; req.generation++;
    assert(invoke(&first, ASCEND_DEVICE_DMA_RELEASE, &req) == -ESTALE);
    req = saved; req.channel++;
    assert(invoke(&first, ASCEND_DEVICE_DMA_RELEASE, &req) == -ESTALE);
    req = saved; req.dev_id++;
    assert(invoke(&first, ASCEND_DEVICE_DMA_RELEASE, &req) == -ESTALE);
    req = acquire_request(0);
    assert(invoke(&first, ASCEND_DEVICE_DMA_QUERY, &req) == 0);
    assert(memcmp(&req, &saved, sizeof(req)) == 0);
    fail_write = 1;
    assert(invoke(&first, ASCEND_DEVICE_DMA_QUERY, &req) == -EFAULT);
    assert(owner->dma_dev == &dev);
    fail_write = 0;
    assert(invoke(&first, ASCEND_DEVICE_DMA_RELEASE, &saved) == 0);
    assert(source_live == 0);
    assert(!channels[last].device_dma_reserved && !owner->dma_dev);
    stale = saved;
    req = acquire_request(0);
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == 0);
    assert(req.generation > stale.generation);
    assert(invoke(&first, ASCEND_DEVICE_DMA_RELEASE, &stale) == -ESTALE);
    assert(invoke(&first, ASCEND_DEVICE_DMA_RELEASE, &req) == 0);
    assert(devdrv_device_dma_suspend(&dev) == 0 && !dev.device_dma_registered);
    req = acquire_request(0);
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == -ENODEV);
    devdrv_device_dma_attach(&dev);
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == 0);
    {
        struct ascend_device_dma_source source = {.version = 1, .generation = req.generation,
            .address = 4090, .length = 320};
        assert(devdrv_device_dma_ioctl(&first, ASCEND_DEVICE_DMA_SOURCE_REGISTER,
            (unsigned long)&source) == 0 && source_live == 1);
    }
    assert(devdrv_device_dma_close(&inode, &first) == 0);
    assert(source_live == 0);
    assert(!channels[last].device_dma_reserved);
    assert(devdrv_device_dma_open(&inode, &first) == 0);
    req = acquire_request(0);
    assert(invoke(&first, ASCEND_DEVICE_DMA_ACQUIRE, &req) == 0);
    saved = req;
    /* Reset removes the registry entry and invalidates the old FD before free. */
    test_source_lifecycle(&first, req.generation);
    devdrv_device_dma_detach(&dev);
    assert(source_live == 0);
    assert(!dev.device_dma_registered && !dev.device_dma_owner);
    assert(invoke(&first, ASCEND_DEVICE_DMA_QUERY, &req) == -ENODEV);
    replacement = fresh(replacement_channels, 5, &pci);
    devdrv_device_dma_attach(&replacement);
    req = acquire_request(0);
    assert(invoke(&second, ASCEND_DEVICE_DMA_ACQUIRE, &req) == 0);
    assert(req.generation > saved.generation);
    assert(invoke(&first, ASCEND_DEVICE_DMA_RELEASE, &saved) == -ENODEV);
    assert(invoke(&second, ASCEND_DEVICE_DMA_RELEASE, &saved) == -ESTALE);
    assert(devdrv_device_dma_close(&inode, &first) == 0);
    assert(replacement_channels[last].device_dma_reserved);
    /* Unexpected outstanding work on close is quarantined until reset. */
    replacement_channels[last].sq_tail = 1;
    assert(devdrv_device_dma_close(&inode, &second) == 0);
    assert(replacement_channels[last].device_dma_reserved && !replacement.device_dma_owner);
    assert(devdrv_device_dma_suspend(&replacement) == -EBUSY);
    devdrv_device_dma_detach(&replacement);
    assert(device_dma_devices.next == &device_dma_devices && !allocations);
    assert(!device_dma_mutex);
}

static long source_invoke(struct file *file, unsigned int cmd, struct ascend_device_dma_source *req)
{
    long result = devdrv_device_dma_ioctl(file, cmd, (unsigned long)req);
    assert(!device_dma_mutex);
    return result;
}

static void test_source_lifecycle(struct file *file, u64 generation)
{
    struct devdrv_device_dma_file *owner = file->private_data;
    struct ascend_device_dma_source request = {.version = 1, .generation = generation,
        .address = 4090, .length = 320}, reply, query;
    _Static_assert(sizeof(request) == 64, "source ABI size");
    _Static_assert(ASCEND_DEVICE_DMA_SOURCE_REGISTER == 0xc04044d3U, "register ABI");
    _Static_assert(ASCEND_DEVICE_DMA_SOURCE_QUERY == 0xc04044d4U, "query ABI");
    _Static_assert(ASCEND_DEVICE_DMA_SOURCE_RELEASE == 0x404044d5U, "unregister ABI");
    reply = request; reply.generation++;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &reply) == -ESTALE);
    reply = request; reply.reserved[1] = 1;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &reply) == -EINVAL);
    reply = request; reply.page_count = 1;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &reply) == -EINVAL);
    reply = request; fail_read = 1;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &reply) == -EFAULT);
    fail_read = 0; source_failure = -ENOMEM;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &reply) == -ENOMEM);
    source_failure = 0; fail_write = 1;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &reply) == -EFAULT);
    assert(!source_live && !owner->source);
    fail_write = 0; reply = request;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &reply) == 0);
    assert(reply.region_id == 2 && reply.page_count == 2 && source_live == 1);
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &request) == -EBUSY);
    query = (struct ascend_device_dma_source){.version = 1, .generation = generation,
        .region_id = reply.region_id};
    query.region_id++;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_RELEASE, &query) == -ESTALE);
    query.region_id--;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_QUERY, &query) == 0);
    assert(memcmp(&query, &reply, sizeof(query)) == 0);
    query = (struct ascend_device_dma_source){.version = 1, .generation = generation,
        .region_id = reply.region_id};
    fail_write = 1;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_QUERY, &query) == -EFAULT);
    assert(source_live == 1);
    fail_write = 0;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_RELEASE, &query) == 0);
    assert(!source_live);
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_RELEASE, &query) == -ESTALE);
    owner->source_sequence = U64_MAX;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &request) == -EOVERFLOW);
    owner->source_sequence = 2;
    assert(source_invoke(file, ASCEND_DEVICE_DMA_SOURCE_REGISTER, &request) == 0);
    /* Leave a live region for the enclosing lease-release cleanup test. */
    assert(source_live == 1);
}

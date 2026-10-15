/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

typedef uint64_t u64;
typedef uint32_t u32;
static u64 now;
static unsigned clock_calls;
static u64 ktime_get_ns(void) { clock_calls++; return ++now; }
#include "dma_profile_header.inc"

#define DEVDRV_DMA_SYNC 0
#define DEVDRV_DMA_WAIT_QUREY 2
#include "../pcie_host/devdrv_dma_poll.h"
#define DEVDRV_ENABLE 1
#define DEVDRV_DISABLE 0
#define DEVDRV_DMA_CHAN_ENABLED 1
#define READ_ONCE(x) (x)
#define wmb() ((void)0)
#define spin_lock_bh(x) ((void)(x))
#define spin_unlock_bh(x) ((void)(x))
static void ignore_log(const char *format, ...) { (void)format; }
#define devdrv_warn_spinlock(...) ignore_log(__VA_ARGS__)
#define devdrv_warn(...) ignore_log(__VA_ARGS__)

struct devdrv_dma_soft_bd_wait_status {
    int status, valid;
    struct devdrv_dma_profile *profile;
    u32 spin_us;
};
struct devdrv_dma_soft_bd { void *priv; };
struct devdrv_dma_channel {
    bool device_dma_reserved, host_dma_redirect;
    int lock, chan_status;
    u32 chan_id;
    struct { unsigned dma_chan_copy_cnt, sync_submit_cnt, async_submit_cnt; } status;
};
struct devdrv_dma_node { u32 size; int direction; };
struct devdrv_dma_copy_para { int copy_type, wait_type; };

static bool enabled, virtual_mode, fill_failed, immediate;
static unsigned int dma_query_spin_us;
static int free_slots = 8, wait_error;
static unsigned submitted, waited, events;
static struct devdrv_dma_soft_bd descriptor;
static struct devdrv_dma_profile saved;
static int saved_status, saved_wait_mode;
static bool devdrv_is_mdev_vm_boot_mode(u32 id) { (void)id; return virtual_mode; }
static bool trace_ascend_dma_sync_enabled(void) { return enabled; }
static int devdrv_dma_get_sq_idle_bd_cnt(struct devdrv_dma_channel *c) { (void)c; return free_slots; }
static struct devdrv_dma_soft_bd *devdrv_dma_fill_sq_desc_and_soft_bd(
    struct devdrv_dma_channel *c, struct devdrv_dma_node *n, u32 count, struct devdrv_dma_copy_para *p)
{
    (void)c; (void)n; (void)count; (void)p;
    return fill_failed ? NULL : &descriptor;
}
static void complete(void)
{
    struct devdrv_dma_soft_bd_wait_status *w = descriptor.priv;
    assert(w != NULL);
    devdrv_dma_profile_complete(w->profile);
    w->status = 0;
    w->valid = DEVDRV_DISABLE;
}
static void devdrv_dma_ch_sq_submit(struct devdrv_dma_channel *c)
{
    (void)c;
    submitted++;
    if (immediate) complete();
}
static int devdrv_dma_chan_sync_wait(u32 id, struct devdrv_dma_channel *c,
    struct devdrv_dma_soft_bd *bd, struct devdrv_dma_soft_bd_wait_status *w)
{
    (void)id; (void)c; (void)bd;
    waited++;
    if (!wait_error && w->valid) complete();
    return wait_error;
}
static int devdrv_dma_chan_copy_by_vpc(u32 id, struct devdrv_dma_channel *c,
    struct devdrv_dma_node *n, u32 count, struct devdrv_dma_copy_para *p)
{
    (void)id; (void)c; (void)n; (void)count; (void)p;
    return 0;
}
static void trace_ascend_dma_sync(u32 id, u32 channel, u32 nodes, int direction,
    int mode, int status, const struct devdrv_dma_profile *p)
{
    assert(id == 0 && channel == 3 && nodes == 2 && direction == 1);
    events++;
    saved = *p;
    saved_status = status;
    saved_wait_mode = mode;
}
#include "dma_profile_path.inc"

int main(void)
{
    struct devdrv_dma_channel channel = {.chan_id = 3, .chan_status = DEVDRV_DMA_CHAN_ENABLED};
    struct devdrv_dma_node nodes[] = {{320, 1}, {320, 1}};
    struct devdrv_dma_copy_para para = {.copy_type = DEVDRV_DMA_SYNC, .wait_type = 2};
    unsigned before;

    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(clock_calls == 0 && events == 0 && submitted == 1 && waited == 1);
    enabled = true;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(events == 1 && saved.bytes == 640 && saved_status == 0 && saved_wait_mode == 2);
    assert(saved.start_ns < saved.submit_ns && saved.submit_ns < saved.wait_ns);
    assert(saved.wait_ns < saved.complete_ns && saved.complete_ns < saved.end_ns);

    immediate = true;
    para.wait_type = 1;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(saved.submit_ns < saved.complete_ns && saved.complete_ns < saved.wait_ns);
    assert(saved_wait_mode == 1);
    immediate = false;

    dma_query_spin_us = 5;
    para.wait_type = DEVDRV_DMA_WAIT_QUREY;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(saved.spin_budget_us == 5);
    nodes[0].size = 8193;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(saved.spin_budget_us == 0);
    nodes[0].size = 320;
    para.wait_type = 1;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(saved.spin_budget_us == 0);
    dma_query_spin_us = 0;

    wait_error = -ETIMEDOUT;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == -ETIMEDOUT);
    assert(saved_status == -ETIMEDOUT && saved.complete_ns == 0);
    wait_error = 0;
    before = submitted;
    free_slots = 0;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == -ENOSPC);
    assert(saved_status == -ENOSPC && saved.submit_ns == 0 && submitted == before);
    free_slots = 8;
    channel.device_dma_reserved = true;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == -EBUSY);
    assert(saved_status == -EBUSY && saved.submit_ns == 0);
    channel.device_dma_reserved = false;
    fill_failed = true;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == -EINVAL);
    assert(saved_status == -EINVAL && submitted == before);
    fill_failed = false;
    channel.chan_status = 0;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == -ENODEV);
    assert(saved_status == -ENODEV);
    channel.chan_status = DEVDRV_DMA_CHAN_ENABLED;

    before = clock_calls;
    para.copy_type = 1;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(clock_calls == before);
    virtual_mode = true;
    para.copy_type = DEVDRV_DMA_SYNC;
    assert(devdrv_dma_chan_copy(0, &channel, nodes, 2, &para) == 0);
    assert(clock_calls == before);
    return 0;
}

/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

typedef uint64_t u64;
typedef uint32_t u32;
#define STATIC static
#define DEVDRV_DMA_SYNC 0
#define DEVDRV_DMA_WAIT_QUREY 2
#define DEVDRV_DMA_CHAN_ENABLED 1
#define DEVDRV_DISABLE 0
#define DEVDRV_DMA_PROCESS_INIT 0
#define DEVDRV_DMA_PROCESS_HANDLING 1
#define DEVDRV_DMA_PROCESS_WAIT_TIMEOUT 2
#define DEVDRV_DMA_QUERY_MAX_WAIT_LONG_TIME 3
#define DEVDRV_MSG_WAIT_MIN_TIME 1
#define DEVDRV_MSG_WAIT_MAX_TIME 2
#define NSEC_PER_USEC 1000
#define READ_ONCE(x) (x)
#define WRITE_ONCE(x, value) ((x) = (value))
#define rmb() ((void)0)
static void ignore_log(const char *format, ...) { (void)format; }
#define devdrv_warn(...) ignore_log(__VA_ARGS__)
#define devdrv_err(...) ignore_log(__VA_ARGS__)

static u64 now;
static unsigned clocks, checked, slept;
static unsigned complete_at, reschedule_at;
static bool resched;
static u64 ktime_get_ns(void) { clocks++; now += 250; return now; }
static bool need_resched(void) { return resched; }
static void cpu_relax(void) {}
#include "spin_profile_header.inc"
#include "../pcie_host/devdrv_dma_poll.h"
struct devdrv_dma_soft_bd_wait_status {
    int status, valid;
    struct devdrv_dma_profile *profile;
    u32 spin_us;
};
struct devdrv_dma_channel { int chan_status; u32 chan_id; };
struct devdrv_dma_soft_bd { int process_flag; };
static struct devdrv_dma_soft_bd_wait_status *current_wait;
static void devdrv_dma_done_task(unsigned long channel)
{
    (void)channel;
    checked++;
    if (checked == complete_at) {
        current_wait->status = 0;
        devdrv_dma_profile_complete(current_wait->profile);
        current_wait->valid = DEVDRV_DISABLE;
    }
    if (checked == reschedule_at) resched = true;
}
static void usleep_range(unsigned min, unsigned max)
{
    assert(min == 1 && max == 2);
    slept++;
    now += 6000;
}
static u32 atomic_cmpxchg(int *state, int old, int value)
{
    int result = *state;
    if (result == old) *state = value;
    return result;
}
static bool is_need_dma_copy_retry(u32 device, int status)
{
    (void)device; (void)status;
    return false;
}
#include "spin_query_functions.inc"

int main(void)
{
    struct devdrv_dma_channel channel = {DEVDRV_DMA_CHAN_ENABLED, 3};
    struct devdrv_dma_soft_bd bd = {0};
    struct devdrv_dma_soft_bd_wait_status wait = {.status = -1, .valid = 1};
    struct devdrv_dma_profile profile = {0};
    current_wait = &wait;

    assert(devdrv_dma_query_spin_budget(5, 0, 2, 8192) == 5);
    assert(devdrv_dma_query_spin_budget(5, 0, 2, 8193) == 0);
    assert(devdrv_dma_query_spin_budget(5, 0, 2, 0) == 0);
    assert(devdrv_dma_query_spin_budget(5, 0, 1, 320) == 0);
    assert(devdrv_dma_query_spin_budget(5, 1, 2, 320) == 0);
    assert(devdrv_dma_query_spin_budget(0, 0, 2, 320) == 0);

    assert(!devdrv_dma_chan_spin_query(&channel, &wait));
    assert(clocks == 0 && checked == 0);
    complete_at = 2;
    assert(devdrv_dma_chan_sync_wait_query(0, &channel, &bd, &wait) == 0);
    assert(slept == 1 && checked == 2 && clocks == 0); /* Original path. */

    wait.status = -1; wait.valid = 1; wait.spin_us = 5; wait.profile = &profile;
    checked = slept = 0; complete_at = 3;
    assert(devdrv_dma_chan_sync_wait_query(0, &channel, &bd, &wait) == 0);
    assert(checked == 3 && slept == 0 && profile.spin_completed == 1);
    assert(profile.spin_queries == 3 && profile.queries == 0 && profile.sleeps == 0);

    memset(&profile, 0, sizeof(profile));
    wait.status = -1; wait.valid = 1; wait.spin_us = 1;
    checked = slept = 0; complete_at = 6;
    assert(devdrv_dma_chan_sync_wait_query(0, &channel, &bd, &wait) == 0);
    assert(profile.spin_completed == 0 && profile.spin_queries == 4);
    assert(slept == 1 && profile.sleeps == 1 && profile.queries == 2);

    memset(&profile, 0, sizeof(profile));
    wait.status = -1; wait.valid = 1; wait.spin_us = 20;
    checked = slept = 0; complete_at = 3; reschedule_at = 1;
    assert(devdrv_dma_chan_sync_wait_query(0, &channel, &bd, &wait) == 0);
    assert(profile.spin_queries == 1 && profile.spin_completed == 0 && slept == 1);

    memset(&profile, 0, sizeof(profile));
    wait.status = -1; wait.valid = 1;
    checked = slept = 0; complete_at = 0; reschedule_at = 0; resched = false;
    channel.chan_status = 0;
    assert(devdrv_dma_chan_sync_wait_query(0, &channel, &bd, &wait) == 0);
    assert(wait.status == -1 && checked == 0 && slept == 0); /* Caller handles error. */

    channel.chan_status = DEVDRV_DMA_CHAN_ENABLED;
    wait.spin_us = 1;
    assert(devdrv_dma_chan_sync_wait_query(0, &channel, &bd, &wait) == -EINVAL);
    assert(bd.process_flag == DEVDRV_DMA_PROCESS_WAIT_TIMEOUT);
    assert(slept == DEVDRV_DMA_QUERY_MAX_WAIT_LONG_TIME + 1); /* Original timeout preserved. */
    return 0;
}

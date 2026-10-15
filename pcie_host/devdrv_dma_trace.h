/* SPDX-License-Identifier: GPL-2.0-only */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ascend_dma

#if !defined(DEVDRV_DMA_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define DEVDRV_DMA_TRACE_H

#include <linux/tracepoint.h>
#include "devdrv_dma_profile.h"

/* One record per synchronous physical-channel attempt, including failures. */
TRACE_EVENT(ascend_dma_sync,
    TP_PROTO(u32 device, u32 channel, u32 nodes, int direction, int wait_mode,
             int status, const struct devdrv_dma_profile *profile),
    TP_ARGS(device, channel, nodes, direction, wait_mode, status, profile),
    TP_STRUCT__entry(
        __field(u32, device)
        __field(u32, channel)
        __field(u32, nodes)
        __field(int, direction)
        __field(int, wait_mode)
        __field(int, status)
        __field(u64, bytes)
        __field(u64, start_ns)
        __field(u64, submit_ns)
        __field(u64, wait_ns)
        __field(u64, complete_ns)
        __field(u64, end_ns)
        __field(u64, sleep_ns)
        __field(u32, sleeps)
        __field(u32, queries)
        __field(u32, spin_budget_us)
        __field(u32, spin_queries)
        __field(u32, spin_completed)
        __field(u64, spin_ns)
    ),
    TP_fast_assign(
        __entry->device = device;
        __entry->channel = channel;
        __entry->nodes = nodes;
        __entry->direction = direction;
        __entry->wait_mode = wait_mode;
        __entry->status = status;
        __entry->bytes = profile->bytes;
        __entry->start_ns = profile->start_ns;
        __entry->submit_ns = profile->submit_ns;
        __entry->wait_ns = profile->wait_ns;
        __entry->complete_ns = profile->complete_ns;
        __entry->end_ns = profile->end_ns;
        __entry->sleep_ns = profile->sleep_ns;
        __entry->sleeps = profile->sleeps;
        __entry->queries = profile->queries;
        __entry->spin_budget_us = profile->spin_budget_us;
        __entry->spin_queries = profile->spin_queries;
        __entry->spin_completed = profile->spin_completed;
        __entry->spin_ns = profile->spin_ns;
    ),
    TP_printk("device=%u channel=%u nodes=%u direction=%d wait_mode=%d status=%d bytes=%llu start_ns=%llu submit_ns=%llu wait_ns=%llu complete_ns=%llu end_ns=%llu sleep_ns=%llu sleeps=%u queries=%u spin_budget_us=%u spin_queries=%u spin_completed=%u spin_ns=%llu",
        __entry->device, __entry->channel, __entry->nodes, __entry->direction,
        __entry->wait_mode, __entry->status, __entry->bytes, __entry->start_ns,
        __entry->submit_ns, __entry->wait_ns, __entry->complete_ns, __entry->end_ns,
        __entry->sleep_ns, __entry->sleeps, __entry->queries, __entry->spin_budget_us,
        __entry->spin_queries, __entry->spin_completed, __entry->spin_ns)
);

#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE devdrv_dma_trace
#include <trace/define_trace.h>

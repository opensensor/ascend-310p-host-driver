#ifndef _DEBUG_DRV_DEV_H_
#define _DEBUG_DRV_DEV_H_

#include "drv_log.h"

#define CHAR_DRIVER_NAME  "dbg_drv"
#define DEBUG_DRV_VERSION "DAVINCI 1.10"

#define TIME_UNIT            (HZ)
#define TS2DRV_TIMEOUT       (5 * (HZ))  // 5 second
#define DEBUG_SYNC_TIME      1
#define PCI_VENDOR_ID_HUAWEI 0x19e5

#define LOG_CACHE_LINE  64
#define LOG_WAIT_TS_NUM 50

#define MODULE_DEBUG "drv_debug"
/*lint -e652*/
#define debug_err(fmt...)   drv_err(MODULE_DEBUG, fmt)
#define debug_warn(fmt...)  drv_warn(MODULE_DEBUG, fmt)
#define debug_info(fmt...)  drv_info(MODULE_DEBUG, fmt)
#define debug_debug(fmt...) drv_debug(MODULE_DEBUG, fmt)
/*lint +e652*/

struct devdrv_device_info {
    u8 env_type;

    u8 ai_cpu_ready_num;
    u8 ai_cpu_broken_map;
    u8 ai_core_ready_num;
    u8 ai_core_broken_map;
    u8 ai_subsys_ip_map;

    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u32 ctrl_cpu_endian_little;
    u32 ts_cpu_core_num;
    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_core_id;
    u32 ai_core_id;
    u32 aicpu_occupy_bitmap;

    u32 ts_load_fail;

    u32 min_sq_id;
    u32 max_sq_id;
    u32 min_cq_id;
    u32 max_cq_id;
    u32 min_stream_id;
    u32 max_stream_id;
    u32 min_event_id;
    u32 max_event_id;

    u32 res[5];
};

#ifdef __aarch64__
void log_flush_cache_dbg(u64 base, u32 len);
#endif

#endif

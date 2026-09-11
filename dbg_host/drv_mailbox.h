#ifndef _DRV_MAILBOX_H_
#define _DRV_MAILBOX_H_

#include <linux/time64.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#define DEV_UNUSED 0
#define DEV_USED   1

#define IDE_DRV_OK      (0)
#define IDE_DRV_ERROR   (-1)
#define IDE_DRV_TIMEOUT (-2)

/* for debug */
#define DEBUG_MAILBOX_LEN          128
#define DEBUG_CALLBACK_MAILBOX_LEN 32

#define DEV_NUM          64
#define DEBUG_BUF_LEN    32
#define DEBUG_VALUE_LEN  4
#define DEBUG_VALUE_SIZE 32
#define BROKEN_TASK_NUM  28

#define LOG_CHANNEL_MAX_NAME (32)

enum cq_info_type {
    callback_cq = 0,
    report_cq
};

enum prof_pm_type {
    PM_DISABLE,
    PM_ENABLE
};

enum mailbox_cmd_type {
    CALC_CQSQ_CREATE = 0x1,
    CALC_CQSQ_RELEASE,
    LOG_CQSQ_CREATE,
    LOG_CQSQ_RELEASE,
    DEBUG_CQSQ_CREATE,
    DEBUG_CQSQ_RELEASE,
    PROFILE_CQSQ_CREATE,
    PROFILE_CQSQ_RELEASE
};

enum ts_cmd_type {
    DEBUG_SUSPEND_DEVICE = 0x10,
    DEBUG_RESUME_DEVICE = 0x11,
    DEBUG_RESUME_PC = 0x12,
    DEBUG_SINGLE_STEP = 0x13,
    DEBUG_SET_BP = 0x14,
    DEBUG_UNSET_BP = 0x15,
    DEBUG_READ_TASK = 0x16,
    DEBUG_READ_CORE = 0x18,
    DEBUG_EXCEPTION_AICORES = 0X1B,
    DEBUG_EXCEPTION_REASON = 0X1C,
    DEBUG_READ_REG = 0x20,
    DEBUG_READ_LOCAL = 0x22,
    DEBUG_READ_SHARED = 0x23,
    DEBUG_READ_GLOBAL = 0x21,
    DEBUG_WRITE_REG = 0x30,
    DEBUG_WRITE_LOCAL = 0x32,
    DEBUG_WRITE_SHARED = 0x33,
    DEBUG_WRITE_GLOBAL = 0x31,
    DEBUG_SET_DEV_STATUS = 0x40, /* to debug status for HDC/TS */
    DEBUG_CALLBACK = 0x50,
    DEBUG_SET_TIME = 0x09,   // mailbox∂®“Â;driver's ioctl no use;
    DEBUG_GET_INFO = 0x52,   // internal use for driver
    DEBUG_GET_DEVNUM = 0x53  // internal use for driver
};

enum drv_type {
    DRV_LOG,
    DRV_DEBUG,
    DRV_PROFILE,
    DRV_TYPE_MAX
};

enum debug_mode {
    DEBUG_MODE_DISABLE,
    DEBUG_MODE_ENABLE
};

enum debug_cmd_type {
    COMMAND_IOCTL,
    COMMAND_RELEASE
};

enum debug_cmd_type_t {
    DEBUG_PLATFORM = 201,
    DEBUG_DEVNUM,
    DEBUG_DEVIDS,
    DEBUG_DEVINFO,
    DEBUG_DEV_START,
    DEBUG_TS_START,
    DEBUG_STOP,
    DEBUG_READ,
    DEBUG_CMD_MAX
};

struct devdrv_cqsq {
    struct mutex pm_mutex;
    struct mutex send_mutex; /* reserved, multi thread for ioctl and sync time */
    struct semaphore sync_wait_sema;
    struct semaphore callback_wait_sema;
    u32 used; /* the cqsq is used or not */
    u32 flag; /* for log/debug/profile */
    u32 sq_handle;
    u32 cq_handle;
    u32 ts_handle;
    u32 aicpu_handle;
    u32 pm_flag;
};

// #pragma pack(1)

struct time_info {
    struct timespec64 system_time;
    struct timespec64 wall_time;
};

/* ts's command info,  user to driver interface; size: 102 Bytes */
struct ts_cmd {
    u8 cmd_verify;
    u8 cmd_type;
    u8 ishwbp;
    u8 hw_bp_id;
    u8 type;
    u8 core_id;
    u16 dev_id;
    u16 debug_id;
    u16 reg_id;
    u16 n_steps;
    u16 smmu_substream_id;
    u16 stream_id;
    u16 task_id;
    u16 local_memory_addr;
    u32 bitmap;
    enum debug_mode dev_flag;
    u64 pid;
    u64 virt_pc;
    u64 virt_addr;
    u64 phy_addr;
    struct ts_cmd_report *ts_report; /* only use by debug's driver, for check */
    union cmd_datas {
        u8 buf[DEBUG_BUF_LEN]; /* buffer and value is union */
        struct time_info liteos_time;
    } data;
};

/* device info */
struct device_info {
    u32 dev_num;       // all device numbers
    u32 aicore_num;    // for single device
    u32 aicpu_num;     // for single device
    u32 aicpu_bitmap;  // for single device
};

/* broken_task information, size: 116 Bytes */
struct broken_task_part {
    u16 task_ids[BROKEN_TASK_NUM];
    u16 stream_ids[BROKEN_TASK_NUM];
};

/* ts's report information, size: 96 Bytes */
struct ts_cmd_info {
    u32 bitmap;
    uint64_t exception_reason;
    u8 buf[DEBUG_BUF_LEN];
    u64 val[DEBUG_VALUE_LEN];
    struct device_info dev_info;
};

/* ts's report command, size: 120 Bytes; already no other free space */
struct ts_cmd_report {
    u8 cmd_type;   /* require ts to fill */
    s8 cmd_result; /* require ts to fill, the command's result */
    u8 cmd_verify; /* require ts to fill, check the command order */
    u8 dev_id;     /* require ts to fill for debug use */
    u32 task_num;  /* require ts to fill for the command read_broken_tasks */
    union datas {
        struct ts_cmd_info ts_info;
        struct broken_task_part task;
    } data;
};

/* ts's callback command, size: 4 Bytes */
struct ts2driver_info {
    u16 stream_id;
    u16 task_id;
};

/* driver info, size: 226 Bytes (102+4+120) ; used by driver, not limited */
struct debug_drv_info {
    struct ts_cmd cmd_info;
    struct ts2driver_info ts_cb_info;
    struct ts_cmd_report ts_report;
};

// #pragma pack()

#define DEBUG_MAILBOX         "mailbox"
#define mailbox_err(fmt...)   drv_err(DEBUG_MAILBOX, fmt)
#define mailbox_warn(fmt...)  drv_warn(DEBUG_MAILBOX, fmt)
#define mailbox_info(fmt...)  drv_info(DEBUG_MAILBOX, fmt)
#define mailbox_debug(fmt...) drv_debug(DEBUG_MAILBOX, fmt)

int devdrv_dbg_init(void);
s32 devdrv_cqsq_init_dbg(u32 devid, enum drv_type flag);
void devdrv_cqsq_exit(enum drv_type flag);
s32 devdrv_cqsq_send(u32 devid, enum drv_type flag, u8 *buffer);
s32 devdrv_cqsq_send_pm(u32 devid, enum drv_type flag, u8 *buffer);
s32 devdrv_check_cqsq(u32 devid, enum drv_type flag);
s32 devdrv_init_dev(void);
enum prof_pm_type devdrv_get_pm(u32 devid, enum drv_type flag);

extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);

#endif


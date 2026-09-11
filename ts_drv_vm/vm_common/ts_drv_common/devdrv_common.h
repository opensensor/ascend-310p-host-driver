/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */

#ifndef DEVDRV_COMMON_H
#define DEVDRV_COMMON_H

#include <linux/types.h>
#ifndef AOS_LLVM_BUILD
#include <linux/uio_driver.h>
#else
#endif
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/rbtree.h>
#include <linux/cpumask.h>
#include <linux/hrtimer.h>

#include <linux/device.h>
#include <linux/version.h>
#if defined(CFG_SOC_PLATFORM_MINI)
#include <linux/pm_wakeup.h>
#endif

#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_dfx_pub.h"
#include "devdrv_functional_cqsq.h"
#include "devdrv_user_common.h"
#include "devdrv_platform_resource.h"
#include "tsdrv_interface.h"
#include "drv_log.h"
#include "drv_share_log.h"
#include "tsdrv_kernel_common.h"
#include "hdcdrv_interface.h"
#include "virtmng_interface.h"
#include "devmng_shm_info.h"
#include "tsdrv_interface.h"

#define module_devdrv "devdrv"

#ifdef UT_VCAST
#define devdrv_drv_err(fmt, ...) drv_err(module_devdrv, fmt, ##__VA_ARGS__)
#define devdrv_drv_warn(fmt, ...) drv_warn(module_devdrv, fmt, ##__VA_ARGS__)
#define devdrv_drv_info(fmt, ...) drv_info(module_devdrv, fmt, ##__VA_ARGS__)
#define devdrv_drv_event(fmt, ...) drv_event(module_devdrv, fmt, ##__VA_ARGS__)
#define devdrv_drv_debug(fmt, ...) drv_pr_debug(module_devdrv, fmt, ##__VA_ARGS__)
#define devdrv_drv_err_extend(fmt, ...) drv_err(module_devdrv, fmt, ##__VA_ARGS__)
#else
#ifdef CFG_FEATURE_SHARE_LOG
#define devdrv_drv_err(fmt, ...) do { \
    drv_err(module_devdrv, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
    share_log_err(DEVMNG_SHARE_LOG_START, fmt, ##__VA_ARGS__); \
} while (0)
#else
#define devdrv_drv_err(fmt, ...) do { \
    drv_err(module_devdrv, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0)
#endif

#define devdrv_drv_warn(fmt, ...)                                                                                  \
    drv_warn(module_devdrv, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define devdrv_drv_info(fmt, ...)                                                                                  \
    drv_info(module_devdrv, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define devdrv_drv_event(fmt, ...)                                                                                 \
    drv_event(module_devdrv, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define devdrv_drv_debug(fmt, ...)                                                                                 \
    drv_pr_debug(module_devdrv, "<%s:%d,%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)

#define devdrv_drv_err_extend(ret, no_err_code, fmt, ...) do {  \
    if ((ret) != (no_err_code)) {                               \
        devdrv_drv_err(fmt, ##__VA_ARGS__);                     \
    } else {                                                    \
        devdrv_drv_warn(fmt, ##__VA_ARGS__);                    \
    }                                                           \
} while (0)
#endif /* UT_VCAST */

#define devdrv_drv_err_spinlock(fmt, ...)
#define devdrv_drv_warn_spinlock(fmt, ...)
#define devdrv_drv_info_spinlock(fmt, ...)
#define devdrv_drv_event_spinlock(fmt, ...)
#define devdrv_drv_debug_spinlock(fmt, ...)

#define devdrv_log_adust(times, fmt, ...) do { \
                if (times >= IPC_RETRY_TIME) \
                    drv_err(module_devdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__);      \
                else      \
                    drv_warn(module_devdrv, "<%s:%d> " fmt, current->comm, current->tgid, ##__VA_ARGS__);      \
} while (0)

#define DEVDRV_SQ_VF_FLOOR_MIN  8 // less than or equal to 1/16 computing power 8 proc
#define DEVDRV_SQ_VF_FLOOR  32  // greater or equal to 1/8 computing power up to 32 proc，
#define DEVDRV_SQ_FLOOR     64 // 64 物理机最大64个进程
#define DEVDRV_CQSQ_INVALID_INDEX 0xFEFE

#define DEVDRV_CORE_MULTIPLE_CAPACITY 1000  /* To calculate the multiplier that needs to be expanded */
#define DEVDRV_CORE_VF_FLOOR_CAPACITY 125   /* (cap_num * multiple) / total_num */

#define CQ_HEAD_UPDATE_FLAG 0x1
#define CQ_HEAD_WAIT_FLAG 0x0

#define DEVDRV_REPORT_PHASE 0x8000
#define DEVDRV_REPORT_SQ_ID 0x01FF
#define DEVDRV_REPORT_SQ_HEAD 0x3FFF0000

#define DEVDRV_INVALID_FD_OR_NUM (-1)

#define DEVDRV_INVALID_CB_SQ_ID 0xFFFF

#ifdef CFG_SOC_PLATFORM_MINIV3
#define DEVDRV_TS_DOORBELL_STRIDE 4096 /* stride 4KB */
#define DEVDRV_TS_DOORBELL_NUM 512
#elif defined(CFG_SOC_PLATFORM_CLOUD_V2)
#define DEVDRV_TS_DOORBELL_STRIDE 4096 /* stride 4KB */
#define DEVDRV_TS_DOORBELL_NUM 2048
#else
#define DEVDRV_TS_DOORBELL_STRIDE 4096 /* stride 4KB */
#define DEVDRV_TS_DOORBELL_NUM 1024
#endif

#define DEVDRV_TS_DOORBELL_SQ_NUM 512

#define DEVDRV_MANAGER_DEVICE_ENV 0
#define DEVDRV_MANAGER_HOST_ENV 1

#define DEVDRV_STATUS_HDC_CLOSE_FLAG  0xEEEEDDDDU

#define TS_MEM_RESTRICT_VALID 0x1 /* ts restricted to access ts_node memory */

#if defined(CFG_SOC_PLATFORM_MINIV3)
#define DEVDRV_RESERVE_MEM_BASE 0x24240000
#define DEVDRV_RESERVE_MEM_SIZE (72 * 1024 * 1024)
#define DEVDRV_RESERVE_CQ_MEM_OFFSET (64 * 1024 *1024)
/* reserved memory for sq slot */
#elif defined(CFG_SOC_PLATFORM_MINIV2)
#define DEVDRV_RESERVE_MEM_BASE 0xBB00000
#define DEVDRV_RESERVE_MEM_SIZE (32 * 1024 * 1024) /* sqmem size is 32M per ts */
#define DEVDRV_RESERVE_CQ_MEM_OFFSET 0
#elif defined(CFG_SOC_PLATFORM_CLOUD_V2)
#define DEVDRV_RESERVE_MEM_BASE 0x1042C00000ULL
#define DEVDRV_RESERVE_MEM_SIZE (128 * 1024 * 1024) /* sqcq total reserve mem size is 128M, sq is 64M, cq is 64M */
#define DEVDRV_RESERVE_CQ_MEM_OFFSET (64 * 1024 * 1024)
#else
#define DEVDRV_RESERVE_MEM_BASE 0x60000000
#define DEVDRV_RESERVE_MEM_SIZE (32 * 1024 * 1024)
#define DEVDRV_RESERVE_CQ_MEM_OFFSET 0
#endif

#define CQ_RESERVE_MEM_BASE (0x2B981000 + 0x200000) /* reserve mem: start addr + offset */
#define CQ_RESERVE_MEM_SIZE (16 * 1024 * 256)
#define CQ_RESERVE_MEM_CQ_OFFSET (16 * 1024UL) /* cq size must not larger than this offset */

/* test, using spi or doorbel for mailbox */

#define DEVDRV_MANAGER_MSG_ID_NUM_MAX 32

#define WAIT_PROCESS_EXIT_TIME 5000

#ifndef STATIC
#if defined(TSDRV_UT) || defined(DEVDRV_MANAGER_HOST_UT_TEST)
#define STATIC
#else
#define STATIC static
#endif
#endif

#ifndef STATIC_INLINE
#if defined(TSDRV_UT) || defined(DEVDRV_MANAGER_HOST_UT_TEST)
#define STATIC_INLINE
#else
#define STATIC_INLINE static inline
#endif
#endif

#define CONST

/* for acpu and tscpu power down synchronization on lite plat */
#define TS_PD_DONE_FLG 0x7B7B
#define ACPU_PD_TS_DONE_FLG 0x5D5D

#ifdef CFG_SOC_PLATFORM_CLOUD
#define LOAD_DEVICE_TIME 500
#else
#ifdef CFG_SOC_PLATFORM_MINIV2
#define LOAD_DEVICE_TIME 800
#else
#define LOAD_DEVICE_TIME 360
#endif
#endif

#ifdef CFG_SOC_PLATFORM_MDC_V51
#define DEVDRV_CBCQ_MAX_GID     128
#else
#define DEVDRV_CBCQ_MAX_GID     1024
#endif

#define ARMv8_Cortex_A55 0x41d05

enum devdrv_power_mode {
    DEVDRV_LOW_POWER = 0x0,
    DEVDRV_NORMAL_POWER,
    DEVDRV_MAX_MODE
};

enum devdrv_device_status {
    DRV_STATUS_INITING = 0x0,
    DRV_STATUS_WORK,
    DRV_STATUS_EXCEPTION,
    DRV_STATUS_SLEEP,
    DRV_STATUS_COMMUNICATION_LOST,
    DRV_STATUS_RESERVED
};

enum hccl_trans_way {
    DRV_SDMA = 0x0,
    DRV_PCIE_DMA
};

#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3)
struct devdrv_report {
    u16 phase : 1;
    u16 warn : 1; /* process warning */
    u16 event_record : 1; /* event record flag */
    u16 place_hold : 1;
    u16 sq_id : 11;
    u16 error_bit : 1;
    u16 sq_head;

    u16 stream_id;
    u16 task_id;

    u32 status_low;
    u32 status_high;
};

#else

/* for get report phase byte */
struct devdrv_report {
    u32 a;
    u32 b;
    u32 c;
};

#endif

struct devdrv_ctrl_report {
    u16 phase : 1;
    u16 sq_id : 11;
    u16 match_flag : 1;
    u16 resv0 : 3;
    u16 sq_head;
    u16 stream_id;
    u16 task_id;
    u32 err_code;
    u32 resv1;
};

struct devdrv_parameter {
    ka_list_head_t list;
    ka_pid_t pid;
    u32 cq_slot_size;

    u16 disable_wakelock;
};

#define P2P_ADDR_INFO_SIZE 48
struct dev_online_notify {
    phys_addr_t ts_db_addr;
    u32 ts_db_size;
    phys_addr_t ts_sram_addr;
    u32 ts_sram_size;
    phys_addr_t hwts_addr;
    u32 hwts_size;
};
struct host_cfg_notify {
    phys_addr_t host_mem_addr;
    u32 host_mem_size;
};
struct p2p_addr_info {
    struct devdrv_mailbox_message_header header;
    u8 remote_devid;
    u8 status;
    u8 notify_type;
    u8 local_devid;
    u32 reserved2;
    union notify_info {
        u8 buf[P2P_ADDR_INFO_SIZE];
        struct dev_online_notify dev_online_notify_data;
        struct host_cfg_notify host_cfg_notify_data;
    } data;
};

struct devdrv_cm_info {
    dma_addr_t dma_handle;
    void *cpu_addr;
    u64 size;
    u32 valid;
};

struct devdrv_cm_msg {
    u32 share_num;
    u64 size;
};

#define MAX_MEM_INFO_NUM 4
#define MEM_INFO_VALID 0xa5a5a5a5
#define MEM_TOTAL_SIZE 0x800000

struct devdrv_contiguous_mem {
    struct devdrv_cm_info mem_info[MAX_MEM_INFO_NUM];
    u64 total_size;
};

struct devdrv_cce_ops {
    ka_cdev_t devdrv_cdev;
    ka_device_t *cce_dev;
};

/* memory types managed by devdrv */
enum {
    DEVDRV_SQ_MEM = 0,
    DEVDRV_CQ_MEM,
    DEVDRV_INFO_MEM,
    DEVDRV_VSQ_INFO_MEM,
    DEVDRV_VCQ_INFO_MEM,
    DEVDRV_DOORBELL_MEM,
    DEVDRV_MAX_MEM
};


enum {
    DEVINFO_STATUS_WORKING = 0x00,
    DEVINFO_STATUS_REMOVED = 0x01,
    DEVINFO_STATUS_SUSPEND = 0x02,
    DEVINFO_STATUS_SHUTDOWN = 0xF1
};

#define TS_FW_VERIFY_FAILED    100001
#define TS_STARTUP_FAILED      100002

struct devdrv_mem_info {
    phys_addr_t phy_addr;
    phys_addr_t bar_addr;
    phys_addr_t virt_addr;
    size_t size;

    ka_task_spinlock_t spinlock;
};

struct devdrv_int_context {
    u32 index;                        /* irq index, eg: when irq num is 16, the index is 0 ~ 15 */
    int first_cq_index;
    int last_cq_index;
    struct tsdrv_ts_resource *ts_resource;
    struct tasklet_struct find_cq_task;
};

struct tsdrv_id_info {
    int id;          /* when fid:0,the id is phy_id,else is virt_id */
    int virt_id;
    int phy_id;
    u32 devid;
    ka_list_head_t list;
    void *ctx;
    ka_pid_t tgid;

    ka_task_spinlock_t spinlock;
    ka_atomic_t ref;
    u8 fid;
    u32 valid;
};

struct devdrv_heart_beat {
    ka_list_head_t queue;
    ka_task_spinlock_t spinlock;
    struct timer_list timer;
    u32 sq;
    u32 cq;
    volatile u32 cmd_inc_counter; /* increment counter for sendind heart beat cmd */
    ka_work_struct_t work;
    const void *exception_info;
    volatile u32 stop;     /* use in host manager heart beat to device,
                        * avoid access null ptr to heart beat node
                        * when heart beat is stop */
    volatile u32 too_much; /* flag if too much heart beat waiting in queue to be sent */
    volatile u32 broken;
    volatile u32 working;

    /* new heart solution */
    struct hrtimer hrtimer;
    ka_ktime_t kt;
    volatile u32 lost_count;
    volatile u64 old_count;
    volatile u64 total_lost_count;
    volatile u64 total_15s_count;
    volatile u64 total_10s_count;
    struct timespec old_time;
    volatile int debug_count;

    u32 tsid;
    u32 reserve;
};

#define DEVDRV_DEV_READY_EXIST 1
#define DEVDRV_DEV_READY_WORK 2

struct aicpu_dts_config {
    u32 flag;
    u32 fw_cpu_id_base;        /* firmware cpu start id */
    u32 fw_cpu_num;            /* firmware cpu num */
    u32 system_cnt_freq;       /* timer frequency */
    u32 ts_int_start_id;       /* interrupt start id, send int to ts when task finish runing or log */
    u32 ctrl_cpu_int_start_id; /* interrupt start id, send int to control cpu when page missing */
    u32 ipc_cpu_int_start_id;  /* ipc interrupt start id, send int to control cpu when page missing */
    u32 ipc_mbx_int_start_id;
};

struct firmware_info {
    /* 内存信息 */
    void *aicpu_fw_mem;
    u32 aicpu_fw_mem_size;
    void *alg_mem[AICPU_MAX_NUM];
    u32 alg_mem_size;

    /* 启动地址 */
    u64 aicpu_boot_addr; /* 此地址是对齐后的虚拟地址 */
    u64 ts_boot_addr;    /* 此地址是对齐后的物理地址 */
    u64 ts_boot_addr_virt;

    u64 ts_blackbox_base;
    u64 ts_blackbox_size;

    /* TS 信息 */
    u64 ts_start_log_base; /* TS log 物理地址 */
    u64 ts_start_log_size; /* TS log 大小 */

    u8 enable_bbox;
};

#define DEVDRV_UUID_BUFF_MAX_SIZE 16

struct devdrv_tflop_allocation {
    u8 uuid[DEVDRV_UUID_BUFF_MAX_SIZE];
};

struct devdrv_container_info {
    u32 tflop_mode;
    u32 total_tflop;
    u32 alloc_unit;
    u32 tflop_num;
    ka_mutex_t lock; /* used for container process */
    void *alloc_table;
};

struct devdrv_ts_mng {
    /* TS working Flag */
    volatile u32 *ts_work_status;
    volatile u32 ts_work_status_shadow;

    /* spinlock for read write ts status */
    ka_task_spinlock_t ts_spinlock;
};

/* common shared data structures define */
#define SOC_VERSION_LEN 32
#define RESERVED_SPACE 8

typedef struct {
    u32 magic;  // fix value：“modu”aisc�?0x606F6475
    u32 modLen; // module python tool fill, frame use, modHdr + modData
    u32 modID; // module fill，frame use，module may use
    u32 modVer; // module fill, frame not use
    u32 modHdrSz; // module tool auto fill，module use,
    u32 checkSum; // module python tool fill, frame not use,modHdr(except checksum) + modData
} devdrv_fix_mod_hdr;

#pragma pack(1)
typedef struct {
    u8 flag; /* 0:No related configuration, 1:configuration exists, 2: bitMap updated */
    u8 totalNum; /* physical core total num, 0:Do not follow this resource */
    u8 minNum; /* Minimum available quantity required */
    u8 reserved;
    u32 freq; /* core working frequency */
    u64 bitMap; /* 1:good, 0:bad */
} devdrv_common_pg_info;

typedef struct {
    devdrv_common_pg_info cpuPara;
    devdrv_common_pg_info aicPara;
    devdrv_common_pg_info aivPara;
    devdrv_common_pg_info hbmPara;
    devdrv_common_pg_info vpcPara;
    devdrv_common_pg_info jpegdPara;
    devdrv_common_pg_info dvppPara;
    devdrv_common_pg_info sioPara;
    devdrv_common_pg_info hccsPara;
    devdrv_common_pg_info mataPara;
    devdrv_common_pg_info l2Para;
    devdrv_common_pg_info gpuPara;
#ifdef CFG_FEATURE_PG_V2
    devdrv_common_pg_info ispPara;
    devdrv_common_pg_info vencPara;
    devdrv_common_pg_info vdecPara;
    devdrv_common_pg_info dmcPara;
#endif
} devdrv_common_pg;

typedef struct {
    u8 socVersion[SOC_VERSION_LEN];
    u32 ringFreq;
    u32 l2Freq;
    u32 mataFreq;
    u32 reserved[RESERVED_SPACE];
} devdrv_special_pg;

struct devdrv_pg_info {
    devdrv_fix_mod_hdr pgHdr;
    devdrv_special_pg spePgInfo;
    devdrv_common_pg comPgInfo;
};
#pragma pack()

#define DEVMNG_SHM_INFO_RANDOM_SIZE 24
struct devdrv_info {
    u8 plat_type;
    u8 status;
    ka_atomic_t occupy_ref;
    u32 env_type;
    u32 board_id;
    u32 slot_id;
    u32 dev_id; /* device id assigned by local device driver */
    u32 chip_id; /* chip id */
    u32 die_id; /* die id */
    u32 ts_mem_restrict_valid;

    /*
     * device id assigned by pcie driver
     * not used in device side
     */
    u32 pci_dev_id;

    /*
     * indicates whether the device supports
     * the corresponding feature.
     */
    u32 capability;

    ka_semaphore_t sem;

    /* devdrv_drv_register is called successfully */
    ka_mutex_t lock;
    volatile u32 driver_flag;
    ka_device_t *dev;

    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u32 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;

    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_core_id;
    u32 ai_core_id;
    u32 aicpu_occupy_bitmap;
    u32 ai_subsys_ip_broken_map;
    u32 hardware_version;
    u64 aicore_bitmap;
    ka_cpumask_t ccpumask;

    struct devdrv_hardware_inuse inuse;

    ka_work_struct_t work;
    /* devdrv_manager_register is called successfully */
    volatile int dev_ready;
    ka_task_spinlock_t spinlock;

    u32 ts_num;
    struct devdrv_ts_mng ts_mng[DEVDRV_MAX_TS_NUM];
    struct devdrv_cce_ops cce_ops;

    /* independent ops and information for each device */
    struct tsdrv_drv_ops *drv_ops;
    struct devdrv_platform_data *pdata;

    /*
     * Host Subscript choose 0
     * Device Subscript choose 0 or 1
     */
    struct devdrv_heart_beat heart_beat[DEVDRV_MAX_TS_NUM];

    /*
     * HOST manager use this workqueue to
     * send heart beat msg to device.
     */
    ka_workqueue_struct_t *heart_beat_wq;

    /* for nfe irq, devdrv_device_manager start a tasklet */
    ka_tasklet_struct_t nfe_task;

    /* aicpu & ts upload related parameters */
    struct firmware_info fw_info;
    struct aicpu_dts_config dts_cfg;
    dma_addr_t aicpu_dma_handle;
    u32 firmware_hardware_version;

    u32 fw_wq_retry;
    ka_delayed_work_t fw_load_wq;

    struct devdrv_container_info container;

    struct rb_root rb_root;
    ka_semaphore_t no_trans_chan_wait_sema;
    unsigned int user_core_distribution;

    u32 dump_ddr_size;
    u64 dump_ddr_dma_addr;
    u64 aicore_freq;
    u64 cpu_system_count;
    u64 dev_nominal_osc_freq;
    u64 monotonic_raw_time_ns;

    u32 fw_verify;
    u32 sec_head_size;
    u8 *fw_src_addr;
    u32 pos;
    u32 fw_len;
    u32 vector_core_num;
    u64 vector_core_bitmap;
    u64 vector_core_freq;
    u32 localtime_sync_state;

    u64 reg_ddr_dma_addr;
    u32 reg_ddr_size;

    u8 run_mode; /*
                 * normal mode: online or offline without container
                 * container mode
                 */

    /* share memory info */
    void __iomem *shm_vaddr;
    U_SHM_INFO_HEAD __iomem *shm_head;
    U_SHM_INFO_SOC __iomem *shm_soc;
    U_SHM_INFO_BOARD __iomem *shm_board;
    U_SHM_INFO_STATUS __iomem *shm_status;

    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE]; /* computing power level info */
    u32 dmp_started;
    u32 ffts_type;

    u32 ts_irq_init;  /* for destroy ts irq. */

    u32 chip_name;
    u32 chip_version;
    u32 chip_info;
    u32 mainboard_id;
    u8 multi_chip;
    u8 multi_die;
    u16 connect_type;
    u16 server_id;
    u16 scale_type;
    u32 super_pod_id;
    u16 addr_mode;

#ifdef CFG_FEATURE_PG
    struct devdrv_pg_info pg_info;
#endif
    #define TEMPLATE_NAME_LEN 32
    u8 template_name[TEMPLATE_NAME_LEN];

    u8 version;
    u16 hccs_hpcs_bitmap;
    char random_number[DEVMNG_SHM_INFO_RANDOM_SIZE];
};

#define TSDRV_MSG_GET_TASKID_NUM 15
#define TSDRV_MSG_GET_ID_NUM 16

#define TSDRV_MSG_INVALID_RESULT 0x1A
#define TSDRV_MSG_H2D_MAGIC 0x5A5A
#define TSDRV_MSG_D2H_MAGIC 0xA5A5

struct tsdrv_msg_resource_id {
    /* how many ids request and returned */
    enum tsdrv_id_type id_type;
    u8 sync_type;
    u32 req_id_num;
    u32 ret_id_num;
    u16 id[DEVDRV_MANAGER_MSG_ID_NUM_MAX];
};

struct tsdrv_msg_res_id_check {
    u32 hpid;
    int id_type;
    u32 res_id;
};

enum tsdrv_res_query_cmd {
    TSDRV_RES_QUERY_ADDR,
    TSDRV_SQ_QUERY_HEAD_ADDR,
    TSDRV_SQ_QUERY_TAIL_ADDR,
    TSDRV_RES_QUERY_MAX,
};

struct tsdrv_res_query {
    u32 cmd;
    u32 id_type;
    u32 id;
    u64 value;
};

struct tsdrv_drv_ops {
    int (*irq_trigger)(u32 dev_id, u32 tsid);
    void (*flush_cache)(u64 base, u32 len);

    /* get stream, event, sq, cq ids from opposite side */
    int (*add_notify_msg_chan)(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
    int (*add_task_msg_chan)(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);
    int (*add_id_msg_chan)(u32 devid, u32 tsid, struct tsdrv_msg_resource_id *dev_msg_resource_id);

    /* functional sqcq */
    int (*create_functional_sq)(u32 devid, u32 tsid, u32 slot_len, u32 *sq_index, u64 *addr);
    int (*functional_set_sq_func)(u32 devid, u32 tsid, u32 sq_index, enum devdrv_cqsq_func function);
    int (*functional_send_sq)(u32 devid, u32 tsid, u32 sq_index, const u8 *buffer, u32 buf_len);
    void (*destroy_functional_sq)(u32 devid, u32 tsid, u32 sq_index);

    int (*create_functional_cq)(u32 devid, u32 tsid, u32 slot_len, u32 cq_type,
        void (*callback)(u32 device_id, u32 tsid, const u8 *cq_slot, u8 *sq_slot), u32 *cq_index, u64 *addr);
    int (*functional_set_cq_func)(u32 devid, u32 tsid, u32 cq_index, enum devdrv_cqsq_func function);
    void (*destroy_functional_cq)(u32 devid, u32 tsid, u32 cq_index);

    /* mailbox */
    int (*mailbox_kernel_sync_no_feedback)(u32 devid, u32 tsid, const u8 *buf, u32 len, int *result);

    int (*memcpy_to_device_sq)(u32 dev_id, u64 dst, u64 src, u32 buf_len);

    int (*ipc_notify_create)(void *context, unsigned long arg, void *notify_ioctl_info);
    int (*ipc_notify_open)(void *context, unsigned long arg, void *notify_ioctl_info);
    int (*ipc_notify_close)(void *context, void *notify_ioctl_info);
    int (*ipc_notify_destroy)(void *context, void *notify_ioctl_info);
    int (*ipc_notify_set_pid)(void *context, void *notify_ioctl_info);
    int (*ipc_notify_set_attr)(void *context, void *notify_ioctl_info);
    int (*ipc_notify_get_info)(void *context, unsigned long arg, void *notify_ioctl_info);
    int (*ipc_notify_get_attr)(void *context, unsigned long arg, void *notify_ioctl_info);
    int (*ipc_notify_record)(void *context, void *notify_ioctl_info);
    void (*ipc_notify_release_recycle)(void *ctx);

    void (* wakeup_all_ctx)(u32 devid, u32 tsid);

    /* ts2ccpu heart beat */
    void (*tsdrv_heart_beat_set_work_state)(u32 devid, u32 tsid, u8 state);
    int (*tsdrv_firmware_load)(struct devdrv_info *dev_info);
    void (*tsdrv_ts_plugin_load)(u32 devid);

    /* ipc api */
    int(*ipc_msg_recv_async)(struct devdrv_info *dev_info, unsigned long arg);
    int(*ipc_msg_send_async)(struct devdrv_info *dev_info, unsigned long arg);

    /* ascend310 host */
    int(*alloc_mem)(struct devdrv_info *dev_info, phys_addr_t *dev_phy_addr, phys_addr_t *host_phy_addr, size_t size);
    int(*free_mem)(struct devdrv_info *info, phys_addr_t phy_addr);
    u32(*svm_va_to_devid)(unsigned long va);
    int(*get_dev_info)(struct devdrv_info *dev_info);
    void (*hdc_register_symbol)(struct hdcdrv_register_symbol *module_symbol);
    void (*hdc_unregister_symbol)(void);
    int (*resid_query)(u32 devid, u32 tsid, struct tsdrv_res_query *info);
    int (*cp_proc_query_by_hpid)(u32 hpid, u32 devid, int cp_type, u32 vfid, int *pid);
    int (*cp_proc_query_hpid)(int pid, u32 *chip_id, u32 *vfid, u32 *host_pid, int *cp_type);
    int (*cp_proc_query_master_location)(u32 host_pid, int pid, u32 chip_id, u32 vfid, int cp_type, u32 *location);
};

struct tsdrv_msg_head {
    u32 dev_id;
    u32 msg_id;
    u16 valid;  /* validity judgement, 0x5A5A is valide */
    u16 result; /* process result from rp, zero for succ, non zero for fail */
    u32 tsid;
};

#define TSDRV_MSG_INFO_LEN 128
#define TSDRV_MSG_INFO_PAYLOAD_LEN (TSDRV_MSG_INFO_LEN - sizeof(struct tsdrv_msg_head))

struct tsdrv_msg_info {
    struct tsdrv_msg_head header;
    u8 payload[TSDRV_MSG_INFO_PAYLOAD_LEN];
};

/* bit 0 sq side, bit 1 cq side */
enum tsdrv_sqcq_addr_side_type {
    TSDRV_SQCQ_SIDE_SQ_BIT = 0,
    TSDRV_SQCQ_SIDE_CQ_BIT = 1
};

/* device: 0 host: 1 */
enum tsdrv_mem_type {
    TSDRV_MEM_ON_DEVICE_SIDE = 0,
    TSDRV_MEM_ON_HOST_SIDE = 1
};

struct devdrv_sq_sub_info {
    u32 index;
    phys_addr_t bar_addr;
    phys_addr_t phy_addr;

    size_t size;
    u32 depth;

    phys_addr_t vaddr;         /* slot base vaddr */
    u32 addr_side;             /* slot memory, device: 0 host: 1 */

    unsigned long map_va;
    u32 queue_size;
};

struct devdrv_cq_sub_info {
    u32 index;
    ka_list_head_t list_sq;
    struct tsdrv_ctx *ctx; /* should mofity to void * */
    ka_task_spinlock_t spinlock; /*
                          * use for avoid the problem:
                          * tasklet(devdrv_find_cq_index) may access cq's uio mem,
                          * there is a delay time, between set cq's uio invalid and accessing cq's uio mem by tasklet.
                          */
    phys_addr_t virt_addr;
    phys_addr_t phy_addr;
    phys_addr_t bar_addr;
    u32 size; // queue size,
    u32 addr_side;        /* slot memory, device: 0 host: 1 */

    size_t slot_size; //
    u32 depth;

    ka_device_t *dev;

    u32 callback_sq_index;
    phys_addr_t callback_sq_phy_addr;

    unsigned long map_va;
    void *chan;
    void (*complete_handle)(struct devdrv_ts_cq_info *cq_info);
};

struct devdrv_client_info {
    u32 init;        /* upper layer driver is initialized or not */
    u32 client_init; /* devdrv_manager_register is called or not */
    void *priv;
};

#define DEVDRV_SQ_CQ_MAP 0
#define DEVDRV_INFO_MAP 0
#define DEVDRV_DOORBELL_MAP 1

#define devdrv_calc_sq_info(addr, index)                                                                            \
    ({                                                                                                              \
        struct devdrv_ts_sq_info *sq;                                                                               \
        sq = (struct devdrv_ts_sq_info *)((uintptr_t)((addr) +                                                        \
                                                      (unsigned long)sizeof(struct devdrv_ts_sq_info) * (index))); \
        sq;                                                                                                         \
    })

#define devdrv_calc_cq_info(addr, index)                                                                            \
    ({                                                                                                              \
        struct devdrv_ts_cq_info *cq;                                                                               \
        cq = (struct devdrv_ts_cq_info *)((uintptr_t)((addr) + DEVDRV_SQ_INFO_OCCUPY_SIZE +                           \
                                                      (unsigned long)sizeof(struct devdrv_ts_cq_info) * (index))); \
        cq;                                                                                                         \
    })

#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3)
#define devdrv_report_get_phase(report) ((report)->phase)
#define tsdrv_report_get_sq_id(report) ((report)->sq_id)
#define tsdrv_report_get_sq_head(report) ((report)->sq_head)

#define devdrv_report_get_sqcq_index(report) ((report)->sq_id)
#define devdrv_report_get_stream_index(report) ((report)->stream_id)
#define devdrv_report_get_type(report) 0 /* no type */
#define devdrv_report_get_task_index(report) ((report)->task_id)

#else

#define devdrv_report_get_phase(report) (((report)->c & DEVDRV_REPORT_PHASE) >> 15)
#define tsdrv_report_get_sq_id(report) ((report)->c & DEVDRV_REPORT_SQ_ID)
#define tsdrv_report_get_sq_head(report) (((report)->c & DEVDRV_REPORT_SQ_HEAD) >> 16)

#define DEVDRV_REPORT_TYPE 0x038
#define DEVDRV_REPORT_SQCQ_ID 0x1FF
#define devdrv_report_get_sqcq_index(report) ((report)->c & DEVDRV_REPORT_SQCQ_ID)
#define DEVDRV_REPORT_STREAM_ID 0XFFC0
#define DEVDRV_REPORT_STREAM_ID_EX 0x40000000
#define devdrv_report_get_stream_index(report) ((((report)->a & DEVDRV_REPORT_STREAM_ID) >> 6) | \
    (((report)->c & DEVDRV_REPORT_STREAM_ID_EX) >> 20))
#define devdrv_report_get_type(report) (((report)->a & DEVDRV_REPORT_TYPE) >> 3)
#define DEVDRV_REPORT_TASK_ID 0xFFFF0000
#define devdrv_report_get_task_index(report) (((report)->a & DEVDRV_REPORT_TASK_ID) >> 16)

#endif

int copy_from_user_safe(void *to, const void __user *from, unsigned long n);
int copy_to_user_safe(void __user *to, const void *from, unsigned long n);

#define DRV_PRINT_START(args...)    \
        devdrv_drv_debug("enter %s: %.4d.\n", __func__, __LINE__)

#define DRV_PRINT_END(args...)      \
        devdrv_drv_debug("exit %s: %.4d.\n", __func__, __LINE__)

#define DRV_CHECK_RET(expr, fmt, ...) do {      \
    if (expr) {                                 \
        devdrv_drv_err(fmt, ##__VA_ARGS__);     \
    }                                           \
} while (0)

#define DRV_CHECK_EXP_ACT(expr, action, fmt, ...) do {  \
    if (expr) {                                         \
        devdrv_drv_err(fmt, ##__VA_ARGS__);             \
        action;                                         \
    }                                                   \
} while (0)

#define DRV_CHECK_EXP_ACT_WARN(expr, action, fmt, ...) do {  \
    if (expr) {                                              \
        devdrv_drv_warn(fmt, ##__VA_ARGS__);                 \
        action;                                              \
    }                                                        \
} while (0)

#define DRV_CHECK_EXP_ACT_DBG(expr, action, fmt, ...) do {  \
    if (expr) {                                             \
        devdrv_drv_debug(fmt, ##__VA_ARGS__);               \
        action;                                             \
    }                                                       \
} while (0)

#define DRV_CHECK_PTR(p, action, fmt, ...) do { \
    if ((p) == NULL) {                          \
        devdrv_drv_err(fmt, ##__VA_ARGS__);     \
        action;                                 \
    }                                           \
} while (0)

#define ERROR_NOT_SUPPORT 65534
#define devdrv_drv_ex_notsupport_err(ret, fmt, ...) do {           \
    if (((ret) != ERROR_NOT_SUPPORT) && ((ret) != -EOPNOTSUPP)) {  \
        devdrv_drv_err(fmt, ##__VA_ARGS__);                        \
    }                                                              \
} while (0)

#endif /* __DEVDRV_ID_COMMON_H */

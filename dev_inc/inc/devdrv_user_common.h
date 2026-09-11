/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */
#ifndef __DEVDRV_USER_COMMON_H
#define __DEVDRV_USER_COMMON_H

#ifndef AOS_LLVM_BUILD
#ifndef pid_t
typedef int pid_t;
#endif
#endif

#define DEVDRV_TS_GROUP_NUM 4

typedef enum {
    DEVDRV_TS_AICORE = 0,
    DEVDRV_TS_AIVECTOR,
    DEVDRV_TS_MAX
} DEVDRV_TS_ID;

#define MAX_DAVINCI_NUM_OF_ONE_CHIP     (4)

#define DEVDRV_SRAM_TS_SHM_SIZE         (0x1000)

#define DEVDRV_CACHELINE_OFFSET         (6)
#define DEVDRV_CACHELINE_SIZE           (64)
#define DEVDRV_CACHELINE_MASK           (DEVDRV_CACHELINE_SIZE - 1)

#ifndef DEVDRV_IMU_CMD_LEN
#define DEVDRV_IMU_CMD_LEN              (32U)
#endif
#define BIT_NUM_U64                     (64)


/*
 *  |     feature(chip)             | side (host or device) | DEVDRV_MAX_DAVINCI_NUM  |  DEVDRV_PF_DEV_MAX_NUM
 *     SRIOV support(910B)               host                    1124                     64
 *     same solution(910B/310Brc)        device                    64                     32
 *     others                            host                      64                     64
 *     others                            device                    64                      4
 */
#ifndef DEVDRV_MAX_DAVINCI_NUM
    #if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_SRIOV)
        #define DEVDRV_MAX_DAVINCI_NUM          1124
        #define DEVDRV_PF_DEV_MAX_NUM           64
    #else
        #define DEVDRV_MAX_DAVINCI_NUM          64

        #ifdef CFG_HOST_ENV
            #define DEVDRV_PF_DEV_MAX_NUM       64
        #else
            #ifdef CFG_FEATURE_VF_USE_DEVID
                #define DEVDRV_PF_DEV_MAX_NUM   32
            #else
                #define DEVDRV_PF_DEV_MAX_NUM   4
            #endif
        #endif
    #endif
#endif

#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_SRIOV)
#define MAX_VDEV_NUM                    (1024)
#define VDAVINCI_MAX_VFID_NUM           (16)
#define VDAVINCI_MAX_DEVICE_COUNT       (64)
#elif (defined CFG_FEATURE_VFIO) && (defined CFG_FEATURE_RC_MODE)
#define MAX_VDEV_NUM                    (4)
#define VDAVINCI_MAX_VFID_NUM           (4)
#define VDAVINCI_MAX_DEVICE_COUNT       (1)
#else
#define MAX_VDEV_NUM                    (1024)
#define VDAVINCI_MAX_VFID_NUM           (16)
#define VDAVINCI_MAX_DEVICE_COUNT       (64)
#endif

#define DEVDRV_PF_TYPE 0
#define DEVDRV_VF_TYPE 1
#define DEVDRV_ALL_TYPE 2

#define DEVDRV_MAILBOX_MAX_FEEDBACK         (16)
#define DEVDRV_MAILBOX_STOP_THREAD          (0x0FFFFFFF)
#define DEVDRV_BUS_DOWN                     (0x0F00FFFF)
#define DEVDRV_HDC_CONNECT_DOWN             (0x0F0FFFFF)
#define DEVDRV_IPC_NAME_SIZE               (65)
#define DEVDRV_PID_MAX_NUM                  (16)

#define DEVDRV_MAX_INTERRUPT_NUM            (32)
#define DEVDRV_MAX_MEMORY_DUMP_SIZE         (4 * 1024 * 1024)

#define DEVDRV_USER_CONFIG_NAME_LEN         (32)
#define DEVDRV_USER_CONFIG_NAME_MAX         (32)
#define DEVDRV_USER_CONFIG_VALUE_LEN        (128)

#define DEVDRV_BB_DEVICE_ID_INFORM          (0x66020004)
#define DEVDRV_BB_DEVICE_STATE_INFORM       (0x66020008)

#ifdef CFG_SOC_PLATFORM_MINIV2
#define CHIP_BASEADDR_PA_OFFSET             (0x8000000000ULL)
#define DIE_BASEADDR_PA_OFFSET              (0x0ULL)
#define DEVDRV_MAX_REG_DDR_READ_SIZE         (5 * 1024 * 1024)
#elif defined (CFG_SOC_PLATFORM_CLOUD_V2)
#define HCCS_BASEADDR_PA_OFFSET             (0x200000000000ULL)
#define HCCS_CHIP_BASEADDR_PA_OFFSET        (0x20000000000ULL)
#define CHIP_BASEADDR_PA_OFFSET             (0x80000000000ULL)
#define DIE_BASEADDR_PA_OFFSET              (0x10000000000ULL)
/* if phycical address is inside 1G and die id is larger than 0, need to add high offset */
#define DIE_BASEADDR_HIGH_ADDR_OFFSET       (0x1000000000ULL)
#define DEVDRV_MAX_REG_DDR_READ_SIZE         (10 * 1024 * 1024)
#else
#define CHIP_BASEADDR_PA_OFFSET             (0x200000000000ULL)
#define DIE_BASEADDR_PA_OFFSET              (0x0ULL)
#define DEVDRV_MAX_REG_DDR_READ_SIZE         (5 * 1024 * 1024)
#endif /* CFG_SOC_PLATFORM_MINIV2 */

#define DEVDRV_LOG_DUMP_TYPE_MAX 64

#define DEVDRV_MAILBOX_PAYLOAD 64

#define DMS_SDK_EX_VERSION_LEN_MAX      128

struct devdrv_mailbox_user_message {
    u8 message_payload[DEVDRV_MAILBOX_PAYLOAD];
    int message_length;
    int feedback_num;
    u8 *feedback_buffer; /*
                          * if a sync message need feedback, must alloc buffer for feedback data.
                          * if a async message need feedback, set this to null,
                          * because driver will send a callback parameter to callback func,
                          * app has no need to free callback parameter in callback func.
                          */
    int sync_type;
    int cmd_type;
    int message_index;
    int message_pid;
    void (*callback)(void *data);
};

struct devdrv_mailbox_feedback {
    void (*callback)(void *data);
    u8 *buffer;
    int feedback_num;
    int process_result;
};

struct devdrv_user_parameter {
    u32 devid;
    u32 cq_slot_size;
    u16 disable_wakelock;
};

struct devdrv_pci_info {
    u32 dev_id;
    u8 bus_number;
    u8 dev_number;
    u8 function_number;
};

struct devdrv_svm_to_devid {
    u32 src_devid;
    u32 dest_devid;
    unsigned long src_addr;
    unsigned long dest_addr;
};

struct devdrv_channel_info_devid {
    char name[DEVDRV_IPC_NAME_SIZE];
    u32 handle;

    u32 event_id;
    u32 src_devid;
    u32 dest_devid;

    /* for ipc event query */
    u32 status;
    u64 timestamp;
};

#define DEVDRV_SHRID_OP_CONFIG    (1 << 0)
#define DEVDRV_SHRID_OP_DECONFIG  (1 << 1)
struct devdrv_notify_ioctl_info {
    u32 dev_id;
    u32 tsid;
    u32 notify_id;
    u32 id_type;
    u64 dev_addr;
    u64 host_addr;
    char name[DEVDRV_IPC_NAME_SIZE];
    pid_t pid[DEVDRV_PID_MAX_NUM];
    u32 flag;
    u32 enableFlag;
};

struct devdrv_hardware_spec {
    u32 devid;
    u32 ai_core_num;
    u32 first_ai_core_id;
    u32 ai_cpu_num;
    u32 first_ai_cpu_id;
    u8 ai_core_num_level; /* 0 invalid */
    u8 ai_core_freq_level; /* 0 invalid */
};

struct devdrv_hardware_inuse {
    u32 devid;
    u32 ai_core_num;
    u32 ai_core_error_bitmap;
    u32 ai_cpu_num;
    u32 ai_cpu_error_bitmap;
};

struct devdrv_board_info {
    u32 dev_id;
    u32 board_id;
    u32 pcb_id;
    u32 bom_id;
    u32 slot_id;
};

#define DEVDRV_MAX_COMPUTING_POWER_TYPE 1
#define DEVDRV_COMPUTING_VALUE_ERROR    0xFFFFFFFFFFFFFFFEull
#define DEVDRV_COMPUTING_VALUE_INVALID  0xFFFFFFFFFFFFFFFFull
#define SOC_VERSION_LENGTH 32U
struct devdrv_manager_hccl_devinfo {
    u8 env_type;
    u32 dev_id;
    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u32 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;
    u32 ts_cpu_core_num;
    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_bitmap;
    u32 ai_core_id;
    u32 ai_cpu_core_id;
    u32 hardware_version; /* mini, cloud, lite, etc. */

    u32 num_dev;
    u32 devids[DEVDRV_MAX_DAVINCI_NUM];
    u32 ts_num;
    u64 aicore_freq;
    u64 cpu_system_count;
    u64 monotonic_raw_time_ns;
    u32 vector_core_num;
    u64 vector_core_freq;
    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];
    u32 ffts_type;
    u32 chip_id;
    u32 die_id;
    u32 addr_mode;
    u32 mainboard_id;
    char soc_version[SOC_VERSION_LENGTH];
    u64 aicore_bitmap[2]; /* support max 128 aicore */
};

#define DEVDRV_MAX_SPEC_RESERVE 8

struct devdrv_vdev_spec_info {
    unsigned char core_num;        /* aicore num for virt device */
    unsigned char reservesd[DEVDRV_MAX_SPEC_RESERVE]; /**< reserved */
};

struct devdrv_vdev_create_info {
    u32 devid;
    u32 vdev_num;    /* number of vdavinci the devid spilt */
    struct devdrv_vdev_spec_info spec[VDAVINCI_MAX_VFID_NUM]; /* specification of vdavinci */
    u32 vdevid[VDAVINCI_MAX_VFID_NUM];        /* id number of vdavinci, equel the 'N' in /dev/vdavincN */
};

#define DSMI_VDEV_RES_NAME_LEN 16
#define DSMI_VDEV_FOR_RESERVE 32
#define DSMI_SOC_SPLIT_MAX 32
#define DSMI_UINT_SIZE sizeof(unsigned int)

struct devdrv_base_resource_vdev {
    unsigned long long token;
    unsigned long long token_max;
    unsigned long long task_timeout;
    unsigned int vfg_id;
    unsigned char vip_mode;
    unsigned char reserved[DSMI_VDEV_FOR_RESERVE - 1];  /* bytes aligned */
};

/* total types of computing resource */
struct devdrv_computing_resource_vdev {
    /* accelator resource */
    float aic;
    float aiv;
    unsigned short dsa;
    unsigned short rtsq;
    unsigned short acsq;
    unsigned short cdqm;
    unsigned short c_core;
    unsigned short ffts;
    unsigned short sdma;
    unsigned short pcie_dma;

    /* memory resource, MB as unit */
    unsigned long long memory_size;

    /* id resource */
    unsigned int event_id;
    unsigned int notify_id;
    unsigned int stream_id;
    unsigned int model_id;

    /* cpu resource */
    unsigned short topic_schedule_aicpu;
    unsigned short host_ctrl_cpu;
    unsigned short host_aicpu;
    unsigned short device_aicpu;
    unsigned short topic_ctrl_cpu_slot;

    unsigned int vdev_aicore_utilization;

    unsigned char reserved[DSMI_VDEV_FOR_RESERVE - DSMI_UINT_SIZE];
};

/* configurable computing resource */
struct devdrv_computing_configurable_vdev {
    /* memory resource, MB as unit */
    unsigned long long memory_size;

    /* accelator resource */
    float aic;
    float aiv;
    unsigned short dsa;
    unsigned short rtsq;
    unsigned short cdqm;

    /* cpu resource */
    unsigned short topic_schedule_aicpu;
    unsigned short host_ctrl_cpu;
    unsigned short host_aicpu;
    unsigned short device_aicpu;

    unsigned char reserved[DSMI_VDEV_FOR_RESERVE];
};

struct devdrv_media_resource_vdev {
    /* dvpp resource */
    unsigned int jpegd;
    unsigned int jpege;
    unsigned int vpc;
    unsigned int vdec;
    unsigned int pngd;
    unsigned int venc;
    unsigned char reserved[DSMI_VDEV_FOR_RESERVE];
};

struct devdrv_create_vdev_res_stru_vdev {
    char name[DSMI_VDEV_RES_NAME_LEN];
    struct devdrv_base_resource_vdev base;
    struct devdrv_computing_configurable_vdev computing;
    struct devdrv_media_resource_vdev media;
};

struct vdev_create_info {
    unsigned int devid;
    unsigned int vdevid;
    struct devdrv_vdev_spec_info spec; /* specification of vdavinci */
    struct devdrv_create_vdev_res_stru_vdev vdev_info;
};

struct devdrv_vdev_id_info {
    u32 devid;
    u32 vdevid;
};

struct vdev_info_st {
    u32 status;       /* whether the vdavinci used by container */
    u32 id;           /* id number of vdavinci, equel the 'N' in /dev/vdavincN */
    u32 vfid;
    u64 cid;          /* container id, equel current->nsproxy->uts_ns->name.nodename */
    struct devdrv_vdev_spec_info spec;
};

struct devdrv_vdev_info {
    u32 devid;
    u32 vdev_num;         /* number of vdavinci the devid had created */
    struct devdrv_vdev_spec_info spec_unused;
    struct vdev_info_st vdev[VDAVINCI_MAX_VFID_NUM];
};

enum {
    DEV_VMNG_SUB_CMD_GET_VDEV_RESOURCE = 0,
    DEV_VMNG_SUB_CMD_GET_TOTAL_RESOURCE,
    DEV_VMNG_SUB_CMD_GET_FREE_RESOURCE,
    DEV_VMNG_SUB_CMD_GET_VDEV_TOPS_PERCENTAGE = 4,
    DEV_VMNG_SUB_CMD_GET_VDEV_ACTIVITY,
    DEV_VMNG_SUB_CMD_MAX
};

struct vdev_query_info {
    char name[DSMI_VDEV_RES_NAME_LEN];
    unsigned int cmd_type;
    unsigned int aic_total;
    unsigned int aic_unused;
    unsigned int devid;
    unsigned int status;
    unsigned int vfid;
    unsigned int vfg_id;
    unsigned int vfg_num;
    unsigned int vfg_bitmap;
    unsigned int vdev_num;         /* number of vdavinci the devid had created */
    unsigned int vdev_id_single;
    unsigned int is_container_used;
    unsigned long long container_id;
    unsigned int vdev_id[DSMI_SOC_SPLIT_MAX];
    struct vdev_info_st vdev[VDAVINCI_MAX_VFID_NUM];
    struct devdrv_base_resource_vdev base;
    struct devdrv_computing_resource_vdev computing;
    struct devdrv_media_resource_vdev media;
};

#define DEVDRV_SVM_VDEV_LEN  8
struct devdrv_svm_vdev_info {
    u32 devid;
    u32 type;
    u32 buf_size;
    char buf[DEVDRV_SVM_VDEV_LEN];
};

struct devdrv_sysrdy_info {
    u32 probe_dev_num;
    u32 rdy_dev_num;
};

enum devdrv_arch_type {
    ARCH_BEGIN = 0,
    ARCH_V100 = ARCH_BEGIN, /* Ascend310，Ascend910，Ascend910B Ascend910_93 used value */
    ARCH_V200,     /* Ascend310P */
    ARCH_V300,     /* Ascend310B */
    ARCH_C100 = 3, /* Ascend910 real value */
    ARCH_C220 = 4, /* Ascend910B & Ascend910_93 real value */
    ARCH_M100 = 5, /* Ascend310 real value */
    ARCH_M200 = 6, /* Ascend610 */
    ARCH_M201 = 7, /* BS9SX1A */
    ARCH_T300 = 8, /* Tiny */
    ARCH_N350 = 9, /* Nano */
    ARCH_M300 = 10, /* AS31XM1X */
    ARCH_M310 = 11, /* Ascend610Lite */
    ARCH_S200 = 12, /* Hi3796CV300ES & TsnsE */
    ARCH_S202 = 13, /* Hi3796CV300CS & OPTG & SD3403 &TsnsC */
    ARCH_END
};

enum devdrv_chip_type {
    CHIP_BEGIN = 0,
    CHIP_MINI = CHIP_BEGIN, /* Ascend310 */
    CHIP_CLOUD,  /* Ascend910 */
    CHIP_MDC,    /* Ascend610 */
    CHIP_LHISI,  /* Hi3796CV300ES & TsnsE, Hi3796CV300CS & OPTG & SD3403 &TsnsC */
    CHIP_DC,     /* Ascend310P */
    CHIP_CLOUD_V2 = 5,     /* Ascend910B & Ascend910_93 */
    CHIP_RESERVED = 6,     /* Reserved */
    CHIP_MINI_V3 = 7,  /* Ascend310B */
    CHIP_TINY_V1 = 8,  /* Tiny */
    CHIP_NANO_V1 = 9,  /* Nano */
    CHIP_KUNPENG_V1 = 10, /* KUNPENG */
    CHIP_AS31XM1 = 11, /* AS31XM1X */
    CHIP_610LITE = 12, /* 610lite */
    CHIP_CLOUD_V3 = 13,  /* Ascend910_93 */
    CHIP_BS9SX1A = 14,  /* BS9SX1A */
    CHIP_END
};

enum devdrv_version {
    VER_BEGIN = 0,
    VER_NA = VER_BEGIN,
    VER_ES,
    VER_CS,
    VER_END
};

enum devdrv_pg_version {
    VER_BIN1 = 1,
    VER_BIN2,
    VER_BIN3,
    VER_BIN4,
    VER_BIN5,
    VER_BIN6,
    VER_BIN7,
    VER_BIN8,
    VER_BIN9,
    VER_BIN10,
    VER_BIN11,
};

#define PLAT_COMBINE(arch, chip, ver) (((arch) << 16) | ((chip) << 8) | (ver))
#define PLAT_GET_ARCH(type) ((type >> 16) & 0xffff)
#define PLAT_GET_CHIP(type) ((type >> 8) & 0xff)
#define PLAT_GET_VER(type) (type & 0xff)

enum devdrv_hardware_version {
    DEVDRV_PLATFORM_MINI_V1 = PLAT_COMBINE(ARCH_V100, CHIP_MINI, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V1 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD, VER_NA),
    DEVDRV_PLATFORM_ASCEND610 = PLAT_COMBINE(ARCH_M200, CHIP_MDC, VER_NA),
    DEVDRV_PLATFORM_ASCEND610LITE = PLAT_COMBINE(ARCH_M310, CHIP_610LITE, VER_NA),
    DEVDRV_PLATFORM_LHISI_ES = PLAT_COMBINE(ARCH_S200, CHIP_LHISI, VER_ES),
    DEVDRV_PLATFORM_LHISI_CS = PLAT_COMBINE(ARCH_S202, CHIP_LHISI, VER_CS),
    DEVDRV_PLATFORM_ASCEND310P = PLAT_COMBINE(ARCH_V200, CHIP_DC, VER_NA),
    DEVDRV_PLATFORM_BS9SX1A = PLAT_COMBINE(ARCH_M201, CHIP_MDC, VER_CS),
    DEVDRV_PLATFORM_AS31XM1 = PLAT_COMBINE(ARCH_M300, CHIP_AS31XM1, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V2 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V2_B1 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN1),
    DEVDRV_PLATFORM_CLOUD_V2_B2 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN2),
    DEVDRV_PLATFORM_CLOUD_V2_B3 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN3),
    DEVDRV_PLATFORM_CLOUD_V2_B4 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V2_B4_1 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN10),
    DEVDRV_PLATFORM_CLOUD_V2_B2C = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN8),
    DEVDRV_PLATFORM_CLOUD_V3 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_NA),
    DEVDRV_PLATFORM_ASCEND310B = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_NA),
    DEVDRV_PLATFORM_ASCEND310B_B1 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN1),
    DEVDRV_PLATFORM_ASCEND310B_B2 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN2),
    DEVDRV_PLATFORM_ASCEND310B_B3 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN3),
    DEVDRV_PLATFORM_ASCEND310B_B4 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN4),
    DEVDRV_PLATFORM_END
};

struct devdrv_black_box_devids {
    u32 dev_num;
    u32 devids[DEVDRV_MAX_DAVINCI_NUM];
};

enum devdrv_device_state {
    STATE_TO_SO = 0,
    STATE_TO_SUSPEND,
    STATE_TO_S3,
    STATE_TO_S4,
    STATE_TO_D0,
    STATE_TO_D3,
    STATE_TO_DISABLE_DEV,
    STATE_TO_ENABLE_DEV,
    STATE_TO_MAX
};

struct devdrv_black_box_state_info {
    u32 state;
    u32 devId;
};

struct devdrv_black_box_user {
    u32 devid;
    u32 size;
    union {
        u64 addr_offset;   /* for ascend910/dc */
        u64 phy_addr;      /* for ascend310 */
    };
    void *dst_buffer;
    u32 thread_should_stop;
    u32 exception_code;
    u64 tv_sec;
    u64 tv_nsec;

    union {
        struct devdrv_black_box_devids bbox_devids;
        struct devdrv_black_box_state_info bbox_state;
    } priv_data;
};

enum devdrv_dev_log_dump_type {
    DEVDRV_DEV_LOG_DUMP_DEBUG_OS = 0,
    DEVDRV_DEV_LOG_DUMP_SEC,
    DEVDRV_DEV_LOG_DUMP_RUN_OS,
    DEVDRV_DEV_LOG_DUMP_RUN_EVENT,
    DEVDRV_DEV_LOG_DUMP_DEBUG_DEV,
    DEVDRV_DEV_LOG_DUMP_TYPE_MAX
};

struct devdrv_bbox_logdump {
    struct devdrv_black_box_user *bbox_user;
    enum devdrv_dev_log_dump_type log_type;
};

struct devdrv_module_status {
    u32 devid;
    u32 ai_core_error_bitmap;
    u8 lpm3_start_fail;
    u8 lpm3_lost_heart_beat;
    u8 ts_start_fail;
    u8 ts_lost_heart_beat;
    u8 ts_sram_broken;
    u8 ts_sdma_broken;
    u8 ts_bs_broken;
    u8 ts_l2_buf0_broken;
    u8 ts_l2_buf1_broken;
    u8 ts_spcie_broken;
    u8 ts_ai_core_broken;
    u8 ts_hwts_broken;
    u8 ts_doorbell_broken;
};

struct devdrv_pcie_pre_reset {
    u32 dev_id;
};
struct devdrv_pcie_rescan {
    u32 dev_id;
};
struct devdrv_pcie_hot_reset {
    u32 dev_id;
};

enum devdrv_p2p_attr_op {
    DEVDRV_P2P_ADD = 0,
    DEVDRV_P2P_DEL,
    DEVDRV_P2P_QUERY,
    DEVDRV_P2P_CAPABILITY_QUERY,
    DEVDRV_P2P_MAX
};

struct devdrv_p2p_attr {
    u32 op;
    u32 dev_id;
    u32 peer_dev_id;
    int status;
};

struct devdrv_p2p_capability {
    u32 dev_id;
    u64 capability;
};

struct devdrv_alloc_host_dma_addr_para {
    unsigned int devId;
    unsigned int size;
    unsigned long long phyAddr;
    unsigned long long virAddr;
};
struct devdrv_free_host_dma_addr_para {
    unsigned int devId;
    unsigned int size;
    unsigned long long phyAddr;
    unsigned long long virAddr;
};

struct devdrv_pcie_sram_read_para {
    unsigned int devId;
    unsigned int offset;
    unsigned char value[512];
    unsigned int len;
};

struct devdrv_pcie_ddr_read_para {
    unsigned int devId;
    unsigned int offset;
    unsigned char value[512];
    unsigned int len;
};

struct devdrv_pcie_hdr_read_para {
    unsigned int devId;
    unsigned int offset;
    unsigned char value[512];
    unsigned int len;
};


#define DEVDRV_VALUE_SIZE 512

enum devdrv_pcie_read_type {
    DEVDRV_PCIE_READ_TYPE_SRAM = 0x0,
    DEVDRV_PCIE_READ_TYPE_DDR,
    DEVDRV_PCIE_READ_TYPE_HDR,
    DEVDRV_PCIE_READ_TYPE_REG_SRAM,
    DEVDRV_PCIE_READ_TYPE_HBOOT_SRAM,
    DEVDRV_PCIE_READ_TYPE_VMCORE_FILE,
    DEVDRV_PCIE_READ_TYPE_VMCORE_STAT,
    DEVDRV_PCIE_READ_TYPE_CHIP_DFX_LOG,
    DEVDRV_PCIE_READ_TYPE_TS_LOG,
    DEVDRV_PCIE_READ_TYPE_BBOX_DDR_LOG,
    DEVDRV_MAX_PCIE_READ_TYPE,
};

struct devdrv_bbox_pcie_logdump {
    unsigned int devid;
    unsigned int offset;
    unsigned int len;
    void *buff;
    enum devdrv_pcie_read_type type;
};

enum devdrv_pcie_write_type {
    DEVDRV_PCIE_WRITE_TYPE_KDUMP = 0x0,
    DEVDRV_MAX_PCIE_WRITE_TYPE,
};

struct devdrv_pcie_read_para {
    unsigned int devId;
    unsigned int offset;
    unsigned char value[DEVDRV_VALUE_SIZE];
    unsigned int len;
    enum devdrv_pcie_read_type type;
};

struct devdrv_pcie_write_para {
    unsigned int devId;
    unsigned int offset;
    unsigned char value[DEVDRV_VALUE_SIZE];
    unsigned int len;
    enum devdrv_pcie_write_type type;
};

struct devdrv_get_device_boot_status_para {
    unsigned int devId;
    u32 boot_status;
};
struct devdrv_get_host_phy_mach_flag_para {
    unsigned int devId;
    unsigned int host_flag;
};
struct devdrv_pcie_imu_ddr_read_para {
    unsigned int devId;
    unsigned int offset;
    unsigned char value[DEVDRV_VALUE_SIZE];
    unsigned int len;
};

struct devdrv_get_device_os_para {
    unsigned int dev_id;
    unsigned int master_dev_id;
};

struct devdrv_get_local_devid_para {
    u32 host_dev_id;
    u32 local_dev_id;
};

/* get dev info by phy id */
struct devdrv_phy_get_devinfo_para {
    u32 phy_id;
    u32 chip_type;
};

#define DEVDRV_SQ_INFO_OCCUPY_SIZE \
    (sizeof(struct devdrv_ts_sq_info) * DEVDRV_MAX_SQ_NUM)
#define DEVDRV_CQ_INFO_OCCUPY_SIZE \
    (sizeof(struct devdrv_ts_cq_info) * DEVDRV_MAX_CQ_NUM)

#define DEVDRV_MAX_INFO_SIZE (DEVDRV_SQ_INFO_OCCUPY_SIZE + DEVDRV_CQ_INFO_OCCUPY_SIZE)
#define DEVDRV_STATUS_INFO_SIZE (sizeof(u32)) /* ts status */

#define DEVDRV_MAX_INFO_ORDER ((u32)get_order(DEVDRV_MAX_INFO_SIZE))
#define DEVDRV_STATUS_INFO_ORDER (get_order(DEVDRV_STATUS_INFO_SIZE))

#define PMU_EMMC_VCC_CHANNEL  (7)
#define PMU_EMMC_VCCQ_CHANNEL (14)
#define ADCIN7_SLOT0          (7)
#define ADCIN8_SLOT1          (8)

struct devdrv_emmc_voltage_para {
    int emmc_vcc;   // should be 2950 mv
    int emmc_vccq;  // should be 1800 mv
};

struct devdrv_emmc_info_para {
    unsigned int dev_id;
    unsigned int info_type;
};

#define DMANAGE_ERROR_ARRAY_NUM (128)
struct devdrv_error_code_para {
    int error_code_count;
    unsigned int error_code[DMANAGE_ERROR_ARRAY_NUM];
    unsigned int dev_id;
};

struct devdrv_lp_status {
    unsigned int status;
    unsigned long long status_info;
};
struct devdrv_dev_info {
    unsigned int info_type;
    unsigned int data1;
    int data2;
    unsigned int data3;
};

#define RESULT_SIZE 4

struct tsensor_ioctl_arg {
    u32 dev_id;
    u32 coreid;
    u32 result_size;
    u32 result[RESULT_SIZE];
};

enum devdrv_dfx_sec_cmd {
    DEVMNG_DFX_TS_WORKING = 0,
    DEVMNG_DFX_HEART_BEAT,
    DEVMNG_DFX_CQSQ,
    DEVMNG_DFX_ALL_STATUS,
    DEVMNG_DFX_MAX_DFX_CMD
};

struct devdrv_cq_dfx_info {
    u32 flag;
    u32 a;
    u32 b;
    u32 c;
    u32 a1;
    u32 b1;
    u32 c1;
};

/* ioctl parameter */
struct devdrv_dfx_para {
    u32 devid;
    u32 cmd;
    void *in;
    void *out;
};

enum devdrv_container_cmd {
#if ((defined CFG_SOC_PLATFORM_MINI) && (!defined CFG_SOC_PLATFORM_MINIV2))
    /* container tflops mode cmd begin */
    DEVDRV_CONTAINER_NOTIFY,
    DEVDRV_CONTAINER_ALLOCATE_TFLOPS,
    DEVDRV_CONTAINER_IS_CONTAINER,
    DEVDRV_CONTAINER_DOCKER_EXIT,
    DEVDRV_CONTAINER_DOCKER_CREATE,
    /* container tflops mode cmd end */

    /* container device assignment mode cmd begin */
    DEVDRV_CONTAINER_ASSIGN_NOTIFY,
    DEVDRV_CONTAINER_ASSIGN_ALLOCATE_DEVICES,
    DEVDRV_CONTAINER_ASSIGN_IS_ASSIGN_MODE,
    DEVDRV_CONTAINER_ASSIGN_SET_UUID,
    /* container device assignment mode cmd end */

    DEVDRV_CONTAINER_IS_IN_CONTAINER,

    /* user for no plusgin container */
    DEVDRV_CONTAINER_GET_DAVINCI_DEVLIST, /* read davinci devlist from /dev directory */
    DEVDRV_CONTAINER_LOGICID_TO_PHYSICID,
    DEVDRV_CONTAINER_GET_BARE_PID,
    DEVDRV_CONTAINER_GET_BARE_TGID,
#else
    /* user for no plusgin container */
    DEVDRV_CONTAINER_GET_DAVINCI_DEVLIST, /* get davinci devlist from item */
    DEVDRV_CONTAINER_GET_BARE_PID,
    DEVDRV_CONTAINER_GET_BARE_TGID,
#endif
    DEVDRV_CONTAINER_MAX_CMD
};

struct devdrv_container_para {
    struct devdrv_dfx_para para;
    u32 admin;
};

#define DEVDRV_MINI_TOTAL_TFLOP 16
#define DEVDRV_MINI_FP16_UNIT 1
#define DEVDRV_MINI_INT8_UNIT 2

#if (defined CFG_FEATURE_VFIO) && (defined CFG_FEATURE_RC_MODE)
#define VDAVINCI_VDEV_OFFSET 32
#define VDAVINCI_VDEV_OFFSET_PLUS(x) ((x) + VDAVINCI_VDEV_OFFSET)
#define VDAVINCI_VDEV_OFFSET_SUB(x) ((x) - VDAVINCI_VDEV_OFFSET)
#define VDAVINCI_IS_VDEV(x) ((x) >= VDAVINCI_VDEV_OFFSET)
#define VDAVINCI_MAX_VDEV_ID (VDAVINCI_VDEV_OFFSET + MAX_VDEV_NUM - 1)
#define VDAVINCI_MAX_VDEV_NUM (VDAVINCI_MAX_VDEV_ID + 1)
#else
#define VDAVINCI_VDEV_OFFSET 100
#define VDAVINCI_VDEV_OFFSET_PLUS(x) ((x) + VDAVINCI_VDEV_OFFSET)
#define VDAVINCI_VDEV_OFFSET_SUB(x) ((x) - VDAVINCI_VDEV_OFFSET)
#define VDAVINCI_IS_VDEV(x) ((x) >= VDAVINCI_VDEV_OFFSET)
#define VDAVINCI_MAX_VDEV_ID (VDAVINCI_VDEV_OFFSET + MAX_VDEV_NUM - 1)
#define VDAVINCI_MAX_VDEV_NUM (VDAVINCI_MAX_VDEV_ID + 1)
#endif

#define VDAVINCI_GET_PFID(x) (VDAVINCI_VDEV_OFFSET_SUB(x) / VDAVINCI_MAX_VFID_NUM)

struct devdrv_container_alloc_para {
    u32 is_in_container;
    u32 num;
    u32 phy_dev_num;
    u32 npu_id[DEVDRV_MAX_DAVINCI_NUM];
    u32 vdev_id[DEVDRV_MAX_DAVINCI_NUM];
    u32 phy_dev_id[DEVDRV_MAX_DAVINCI_NUM];
};

struct devdrv_container_tflop_config {
    u32 tflop_mode;
    u32 total_tflop;
    u32 alloc_unit;
    u32 tflop_num;
};

enum devdrv_run_mode {
    DEVDRV_NORMAL_MODE = 0,
    DEVDRV_CONTAINER_MODE,
    DEVDRV_MAX_RUN_MODE
};

enum devdrv_running_mode {
    DEVDRV_NORMAL_RUNNING_MODE = 0,
    DEVDRV_CONTAINER_RUNNING_MODE,
    DEVDRV_CONTAINER_DEVICE_RUNNING_MODE,
    DEVDRV_MAX_RUNNING_MODE
};

enum devdrv_container_tflop_mode {
    DEVDRV_FP16 = 0,
    DEVDRV_INT8,
    DEVDRV_MAX_TFLOP_MODE
};

enum devdrv_flash_config_cmd {
    DEVDRV_FLASH_CONFIG_READ_CMD = 0,
    DEVDRV_FLASH_CONFIG_WRITE_CMD,
    DEVDRV_FLASH_CONFIG_CLEAR_CMD,
    DEVDRV_FLASH_CONFIG_MAX_CMD
};

struct devdrv_flash_config_ioctl_para {
    u32 dev_id;
    int cmd;
    int cfg_index;
    u32 buf_size;
    void *buf;
};

#define DEVDRV_MAX_LIB_LENGTH 128
#define DEVDRV_SHA256_SIZE 32

struct devdrv_load_kernel {
    unsigned int devid;
    unsigned int share;
    char libname[DEVDRV_MAX_LIB_LENGTH];
    unsigned char sha256[DEVDRV_SHA256_SIZE];
    int pid;
    void *binary;
    unsigned int size;
};

struct devdrv_load_kernel_serve {
    struct devdrv_load_kernel load_kernel;
    u8 thread_should_exit;
    u32 save_state;  // succ: 0, fail: 1
};

#define REMAP_ALIGN_SIZE (64 * 1024)
#define REMAP_ALIGN_MASK (~(REMAP_ALIGN_SIZE - 1))
#define REMAP_ALIGN(x) (((x) + REMAP_ALIGN_SIZE - 1) & REMAP_ALIGN_MASK)
#define DEVDRV_VM_SYSFS_DFX_SIZE (1024 * 1024)

#define DEVDRV_VM_SQ_SLOT_SIZE      REMAP_ALIGN(DEVDRV_MAX_SQ_DEPTH * DEVDRV_SQ_SLOT_SIZE)
#define DEVDRV_VM_INFO_MEM_SIZE     REMAP_ALIGN(DEVDRV_MAX_INFO_SIZE)
#define DEVDRV_VM_CQ_SLOT_SIZE      REMAP_ALIGN(DEVDRV_MAX_CQ_DEPTH * DEVDRV_CQ_SLOT_SIZE)

/* ****custom ioctrl****** */
typedef enum {
    DEVDRV_IOC_VA_TO_PA,              // current only use in lite
    DEVDRV_IOC_GET_SVM_SSID,          // current only use in lite
    DEVDRV_IOC_GET_CHIP_INFO,         // current only use in lite
    DEVDRV_IOC_ALLOC_CONTIGUOUS_MEM,  // current only use in lite
    DEVDRV_IOC_FREE_CONTIGUOUS_MEM    // current only use in lite
} devdrv_custom_ioc_t;

typedef struct {
    u32 version;
    u32 cmd;
    u32 result;
    u32 reserved;
    u64 arg;
} devdrv_custom_para_t;

typedef enum {
    GET_GROUP_COUNT_RES_INDEX = 0,
    GET_GROUP_TS_ID_RES_INDEX,
    GET_GROUP_ID_RES_INDEX
}GET_GROUP_RESVER_INDEX;

#define COMMON_STATUS_RES_LEN 8
typedef struct _common_status_info {
    unsigned int dev_id;
    unsigned int name_len;
    unsigned char *name;
    void *commoninfo;
    unsigned int commoninfo_len;
    int reserver[COMMON_STATUS_RES_LEN];
} common_status_info_t;

typedef struct _last_bootstate_info {
    unsigned int dev_id;
    unsigned int boot_type;
} last_bootstate_info_t;

#define DDR_MANUFACTURE_CODE_LEN 24
struct devdrv_ddr_base_info {
    unsigned int ddr_type;
    unsigned int ddr_capacity;
    unsigned int ddr_channel;
    unsigned int ddr_rank_num;
    unsigned int ddr_ecc_enable;
};

enum tsdrv_dev_machine_type {
    PHY_MACHINE_TYPE = 0,
    VIR_MACHINE_TYPE = 1,
    MACHINE_MAX_TYPE
};

struct tsdrv_dev_com_info {
    enum tsdrv_dev_machine_type mach_type;    /* physical machine: 0, virtual machine: 1 */
    u32 ts_num;
};

struct delete_ts_group_info {
    int ts_id;
    int group_id;
    unsigned int result;
};

struct get_ts_group_para {
    unsigned int device_id;
    int ts_id;
    int group_id;
    int group_count;
};

#define DEVDRV_IOCTL_RES_LEN 8
struct devdrv_ioctl_info {
    int dev_id;
    void *input_buf;
    unsigned int input_len;
    char *out_buf;
    unsigned int out_len;
    int res[DEVDRV_IOCTL_RES_LEN];
};

struct ts_group_info {
    unsigned char  group_id;
    unsigned char  state;
    unsigned char  extend_attribute; // bit 0=1??ʾ??Ĭ??group
    unsigned char  aicore_number; // 0~9
    unsigned char  aivector_number; // 0~7
    unsigned char  sdma_number; // 0~15
    unsigned char  aicpu_number; // 0~15
    unsigned char  active_sq_number; // 0~31
    unsigned int   aicore_mask; // as output in dsmi_get_capability_group_info/halGetCapabilityGroupInfo
    unsigned int   result;
};

typedef enum {
    SET_CAN_CONFIG_FILTER = 0,
    SET_CAN_CONFIG_RAM,
    SET_CAN_CONFIG_MAX
}SET_CAN_CONFIG_INDEX;

/* DSMI sub commond for can module */
#define CAN_ID_INDEX_OFFSET 24
#define CAN_CFG_ITEM_CFG_BIT 0xff
#define CAN_MAX_CFG_LEN 2048
#define GET_CAN_ID_FROM_SUB_CMD(can_sub_cmd) ((can_sub_cmd) >> CAN_ID_INDEX_OFFSET)
#define GET_CAN_CFG_ITEM_FROM_SUB_CMD(can_sub_cmd) ((can_sub_cmd) & CAN_CFG_ITEM_CFG_BIT)

#define DEVDRV_SIGN_LEN   49
struct devdrv_ioctl_para_bind_host_pid {
    pid_t host_pid;
    unsigned int vfid;
    unsigned int chip_id;
    int mode;
    int cp_type;
    unsigned int len;
    char sign[DEVDRV_SIGN_LEN];
};

struct devdrv_ioctl_para_query_pid {
    pid_t pid;
    unsigned int chip_id;
    unsigned int vfid;
    pid_t host_pid;
    unsigned int cp_type;
};

enum ipcMainCmdType {
    IPC_MAIN_CMD_QUERY = 1,
    MAX_IPC_MAIN_CMD_NUM
};

enum ipcSubCmdType {
    IPC_SUB_CMD_SOC_TEMP = 1,
    IPC_SUB_CMD_HBM_TEMP,
    IPC_SUB_CMD_NDIE_TEMP,
    IPC_SUB_CMD_VRD_TEMP,
    IPC_SUB_CMD_AIC0_FREQ,
    IPC_SUB_CMD_AIC1_FREQ,
    IPC_SUB_CMD_AIV_FREQ,
    IPC_SUB_CMD_CCPU_FREQ,
    IPC_SUB_CMD_DDR_FREQ,
    IPC_SUB_CMD_HBM_FREQ,
    IPC_SUB_CMD_SOC_POWER,
    MAX_IPC_SUB_CMD_NUM
};

enum ipcTargetId {
    TARGET_ID_LP = 0,        /**< 310p is lp, 310 is lpm3, 910 have no lp */
    TARGET_ID_IMU,           /**< only in 910 */
    TARGET_ID_TSC,
    TARGET_ID_TSV,           /**< only in 610 */
    TARGET_ID_SAFETYISLAND,  /**< only in 610 */
    MAX_TARGET_ID
};

struct devdrv_device_work_status {
    unsigned int device_id;
    unsigned int device_process_status;
    unsigned int dmp_started;
};

struct devdrv_device_health_status {
    unsigned int device_id;
    unsigned int device_health_status;
};

#define DEVDRV_MAX_PAYLOAD_LEN 256
enum devdrv_owner_type {
    DEVDRV_DEV_RESOURCE = 0,
    DEVDRV_VDEV_RESOURCE,
    DEVDRV_PROCESS_RESOURCE,
    DEVDRV_MAX_OWNER_TYPE
};

enum devdrv_dev_resource_type {
    DEVDRV_DEV_STREAM_ID = 0,
    DEVDRV_DEV_EVENT_ID,
    DEVDRV_DEV_NOTIFY_ID,
    DEVDRV_DEV_MODEL_ID,
    DEVDRV_MAX_TS_INFO_TYPE = DEVDRV_DEV_MODEL_ID,
    DEVDRV_DEV_HBM_TOTAL,
    DEVDRV_DEV_HBM_FREE,
    DEVDRV_DEV_DDR_TOTAL,
    DEVDRV_DEV_DDR_FREE,
    DEVDRV_MAX_MEM_INFO_TYPE = DEVDRV_DEV_DDR_FREE,
    DEVDRV_DEV_PROCESS_PID,
    DEVDRV_DEV_PROCESS_MEM,
    DEVDRV_DEV_PROCESS_CONTAINER_PID,
    DEVDRV_DEV_INFO_TYPE_MAX
};

struct devdrv_resource_info {
    u32 devid;
    u32 owner_type;
    u32 owner_id;
    u32 resource_type;
    u32 tsid;
    u32 buf_len;
    char buf[DEVDRV_MAX_PAYLOAD_LEN];
};

#define DEVDRV_MAX_CHIP_NUM 64
#define DEVDRV_RANDOM_SIZE 24
struct devdrv_chip_list {
    int count;
    int chip_list[VDAVINCI_VDEV_OFFSET];
};

struct devdrv_chip_dev_list {
    int chip_id;
    int count;
    int dev_list[DEVDRV_MAX_DAVINCI_NUM];
};

struct devdrv_get_dev_chip_id {
    int dev_id;
    int chip_id;
};

struct virtmng_vf_list {
    u32 dev_id;
    u32 vf_num;
    u32 vf_list[VDAVINCI_MAX_VFID_NUM];
};

#define MAX_CHIP_NAME 32
typedef struct devdrv_chip_info {
    u8 type[MAX_CHIP_NAME];
    u8 name[MAX_CHIP_NAME];
    u8 version[MAX_CHIP_NAME];
} devdrv_chip_info_t;

typedef struct devdrv_query_chip_info {
    u32 dev_id;
    u32 reg_val;
    devdrv_chip_info_t info;
} devdrv_query_chip_info_t;

struct devdrv_sign_verification_info {
    unsigned int dev_id;
    unsigned char sign_buf;
};

#endif /* __DEVDRV_USER_COMMON_H */

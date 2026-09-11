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
#ifndef DRVFAULT_USER_COMMON_H
#define DRVFAULT_USER_COMMON_H
#if (defined AOS_LLVM_BUILD) && (!defined DSMI_USER_API)
#include <linux/compiler_types.h>
#endif
#include <linux/ioctl.h>
#include <linux/types.h>

#if (defined AOS_LLVM_BUILD) && (defined DSMI_USER_API)
#  define DRVFAULT_DEV_PATH "/local/dev/"
#else
#  define DRVFAULT_DEV_PATH "/dev/"
#endif
#define DRVFAULT_DEV_NAME "drvfault"

#define DRVFAULT_MSG_DATA_LEN   22

#define EVENT_TYPE_EMU 0
#define EVENT_TYPE_SOFTWARE 1

#if (defined LINUX) && (LINUX == 0)
#  ifndef __iomem
#    define __iomem
#  endif
#endif

/* miniv2 cpuid for ipc */
enum ipcmsg_cpuid {
    CPUID_TAISHAN0 = 0,
    CPUID_TAISHAN1 = 1,
    CPUID_TAISHAN0_AOS = 2,
    CPUID_TAISHAN1_AOS = 3,
    CPUID_LP_R52 = 4,
    CPUID_TSCPU0 = 5,
    CPUID_TSCPU1 = 6,
    CPUID_SAFETYISLAND = 7,
    CPUID_FM_TAISHAN = 22,
    MAX_CPUID
};

enum ipcmsg_msgtype {
    MSGTYPE_SMOKE = 1,
    MSGTYPE_TEMPERATURE = 2,
    MSGTYPE_SLEEP = 3,
    MSGTYPE_RESUME = 4,
    MSGTYPE_FREQ = 5,
    MSGTYPE_HEALTH_STATE = 6,
    MSGTYPE_HEALTH_EVENT = 7,
    MSGTYPE_HAERTBEAT = 8,
    MSGTYPE_VERSION = 9,
    MSGTYPE_TIME = 10,
    MSGTYPE_INFO = 11,
    MSGTYPE_LOG_BBOX = 13,
    MSGTYPE_MS_STATUS = 14,
    MSGTYPE_HARD_TEST = 15,
};

enum ipcmsg_cmdtype {
    CMDTYPE_INQUERY = 1,
    CMDTYPE_SETTING = 2,
    CMDTYPE_NOTIFY = 3,
    CMDTYPE_TEST = 4,
    CMDTYPE_ON = 5,
    CMDTYPE_OFF = 6,
};

enum drvfault_event_type {
    FAULT_EVENT_TYPE_EMU_EVENT = 0,
    FAULT_EVENT_TYPE_SOFTWARE = 1,
    FAULT_EVENT_TYPE_MAX
};

enum ipcmsg_type {
    REQUEST_DATA = 0, /* send request data from taishan */
    RESPONSE_DATA = 1, /* response data from others to taishan */
};

enum drvfault_level {
    FAULT_LEVEL_NORMAL = 0,
    FAULT_LEVEL_MINOR = 1,
    FAULT_LEVEL_MAJOR = 2,
    FAULT_LEVEL_CRITICAL = 3,
    FAULT_LEVEL_MAX
};

enum drvfault_state {
    FAULT_STATE_OCCUR = 0,
    FAULT_STATE_RESUME = 1,
    FAULT_STATE_ONE_TIME = 2,
    FAULT_STATE_NOT_REPORT = 3,
    FAULT_STATE_CHANNEL_TEST = 4,
    FAULT_STATE_MAX
};

enum drvfault_emu_id {
    SYS_EMU_ID = 0,          /* 0 */
    SILS_EMU_ID = 1,         /* 1 */
    SILS_SUB_EMU_ID = 2,     /* 2 */
    PERI_SUB_EMU_ID = 3,     /* 3 */
    AO_SUB_EMU_ID = 4,       /* 4 */
    HAC_SUB_EMU_ID = 5,      /* 5 */
    GPU_SUB_EMU_ID = 6,      /* 6 */
    ISP0_SUB_EMU_ID = 7,     /* 7 */
    ISP1_SUB_EMU_ID = 8,     /* 8 */
    DVPP0_SUB_EMU_ID = 9,    /* 9 */
    DVPP1_SUB_EMU_ID = 10,    /* 10 */
    DVPP2_SUB_EMU_ID = 11,    /* 11 */
    IO_SUB_EMU_ID = 12,       /* 12 */
    TS_SUB_EMU_ID = 13,       /* 13 */
    CPU_CLUSTER0_EMU_ID = 14, /* 14 */
    CPU_CLUSTER1_EMU_ID = 15, /* 15 */
    CPU_CLUSTER2_EMU_ID = 16, /* 16 */
    CPU_CLUSTER3_EMU_ID = 17, /* 17 */
    AICORE0_EMU_ID = 18,      /* 18 */
    AICORE1_EMU_ID = 19,      /* 19 */
    AICORE2_EMU_ID = 20,      /* 20 */
    AICORE3_EMU_ID = 21,      /* 21 */
    AICORE4_EMU_ID = 22,      /* 22 */
    AICORE5_EMU_ID = 23,      /* 23 */
    AICORE6_EMU_ID = 24,      /* 24 */
    AICORE7_EMU_ID = 25,      /* 25 */
    AICORE8_EMU_ID = 26,      /* 26 */
    AICORE9_EMU_ID = 27,      /* 27 */
    AIVECTOR0_EMU_ID = 28,    /* 28 */
    AIVECTOR1_EMU_ID = 29,    /* 29 */
    AIVECTOR2_EMU_ID = 30,    /* 30 */
    AIVECTOR3_EMU_ID = 31,    /* 31 */
    AIVECTOR4_EMU_ID = 32,    /* 32 */
    AIVECTOR5_EMU_ID = 33,    /* 33 */
    AIVECTOR6_EMU_ID = 34,    /* 34 */
    AIVECTOR7_EMU_ID = 35,    /* 35 */
    MEDIA_SUB_EMU_ID = 36,    /* 36 */
    LP_SUB_EMU_ID = 37,       /* 37 */
    TSV_SUB_EMU_ID = 38,      /* 38 */
    TSC_SUB_EMU_ID = 39,      /* 39 */
    EMU_SUB_ID_MAX       /* 40 */
};

struct IpcMsgHead {
    unsigned char msg_type;      /* refer to enum ipcmsg_msgtype */
    unsigned char cmd_type;      /* refer to enum ipcmsg_cmdtype */
    unsigned char cmdDest;       /* refer to enum ipcmsg_cpuid */
    unsigned char cmdSrc;        /* refer to enum ipcmsg_cpuid */
    unsigned short sequence;     /* ipc msg order */
    unsigned char type : 1;      /* request data or respond data */
    unsigned char sync_type : 1; /* 0: async 0: sync */
    unsigned char reserve : 6;   /* reserved for future use */
    unsigned char msg_length;    /* payload msg length */
} __attribute__((packed));

struct IpcMessage {
    struct IpcMsgHead head;
    unsigned char payload[DRVFAULT_MSG_DATA_LEN];
    unsigned short crc;
} __attribute__((packed));

enum DrvFaultIoctlType {
    USERMSG_SEND,
    USER_GET_REPORT,
    IOCTL_TYPE_MAX,
};

#define FAULTMNG_MAX_FAULT_REPORT   128 /* 128 msg max */
#define FAULTMNG_TIMEOUT_MAX_VALUE  3000

#define FAULTMNG_DID_INFO_SIZE_MAX   1 /* 1 did_info max */

 /* 128 bytes max - kfifo need 2^n aligned: reference struct drvfault_msg_info_iner */
#define FAULTMNG_DID_VALUE_SIZE_MAX (128UL - 4 * sizeof(unsigned short) - 2 * sizeof(unsigned int))

/* Data Identifier */
struct did_info {
    unsigned short did_id;
    unsigned short did_value_size;
    unsigned char *did_value;
};

/* Unified Diagnostic Services */
struct uds_info {
    unsigned short total_size;
    unsigned short did_info_size;
    struct did_info *did_record;
};

struct drvfault_msg_info {
    union { /* eventCode */
        unsigned int val;
        struct {
            unsigned char bit_id;
            unsigned char src_id;
            unsigned char emu_id;
            unsigned char report_type : 2;   /* 0:safety   1:error */
            unsigned char intclear_flag : 1; /* 0:cleard  1:need clear */
            unsigned char event_type : 1;    /* 0:emu event  1:others */
            unsigned char level : 3;
            unsigned char direc : 1;         /* 0:assert  1:deassert */
        } bit;
        struct {
            unsigned short error_num;
            unsigned char id;
            unsigned char reserve : 3;
            unsigned char event_type : 1;    /* 0:emu event  1:software */
            unsigned char level : 3;
            unsigned char direc : 1;         /* 0:assert  1:deassert */
        } bit_s;                             /* software fault */
    } event_code;
    unsigned int fault_state;

    struct uds_info fault_uds;
};

struct devdrv_drvfault_msg_arg {
    unsigned int time_out;
    unsigned int report_num;
    struct drvfault_msg_info msg_info[FAULTMNG_MAX_FAULT_REPORT];
};

struct DrvFaultIocArg {
    unsigned int devId;
    enum DrvFaultIoctlType iocType;
    union {
        struct IpcMessage ipcMsg;
        struct devdrv_drvfault_msg_arg arg;
    };
};

#pragma pack(1)
struct safety_emu_info {
    union {
        unsigned int val;
        struct {
            unsigned char bit_id;
            unsigned char src_id;
            unsigned char emu_id;
            unsigned char report_type : 2;    /* 1:safety   2:error */
            unsigned char int_clear_flag : 1; /* 0:cleard  1:need clear */
            unsigned char event_type : 1;     /* 0:emu event  1:others */
            unsigned char level : 3;
            unsigned char direc : 1; /* 0:assert  1:deassert */
        } emu_bit;
    };
};

#define SAFETY_EVENT_RSV_NUM 5
struct safety_fault_event {
    unsigned int event_id;          /* Module ID + SensorType + EventType */
    unsigned char device_id;        /* chip id */
    unsigned char node_type;
    unsigned char node_id;
    unsigned char sub_node_type;
    unsigned char sub_node_id;
    unsigned char event_severity;
    unsigned char event_assertion;  /* 0:RESUME 1:OCCUR 2:ONE_TIME */
    unsigned char event_serial_num;
    unsigned char reserve[SAFETY_EVENT_RSV_NUM];
};

enum safety_fault_type {
    SAFETY_HW_FAULT = 0,  /* hardware fault */
    SAFETY_SW_FAULT = 1,  /* software fault */
    SAFETY_FAULT_NUM
};

struct safety_ipc_msg_payload {
    struct {
        unsigned char version : 4;    /* set the default value to 0 */
        unsigned char event_type : 2; /* 0: hardware fault;1: software fault */
        unsigned char reserve : 2;
    } msg_head;
    struct safety_emu_info hw_fault_info;  /* hardware fault need to set */
    struct safety_fault_event fault_event; /* hardware and software fault need to set */
};

#define SAFETY_IPC_MSG_LEN  22
struct safety_ipc_msg {
    unsigned char cmd_type;
    unsigned char cmd;
    unsigned char cmd_dest;
    unsigned char cmd_src;
    unsigned short seq;
    unsigned char data_type : 1; /* 0:request  1:response */
    unsigned char reserved : 3;
    unsigned char chan_type : 4; /* 1:uart  2:s_ipc  3:ns_ipc */
    unsigned char len;
    unsigned char data[SAFETY_IPC_MSG_LEN];
    unsigned short crc;
};
#pragma pack()

struct drvfault_irq_info {
    unsigned int dev_id;
    unsigned int emu_id;
    unsigned int src_id;
    unsigned int bit_id;
    unsigned int fault_status;
};


/**
* fault event severity
* refer to @DMS_SEVERITY_T
*/
typedef enum {
    DRV_FAULT_OK,
    DRV_FAULT_MINOR,
    DRV_FAULT_MAJOR,
    DRV_FAULT_CRITICAL
} drvfault_event_severity;

#define DMS_SENSOR_DESCRIPT_LENGTH 20
#define DMS_MAX_EVENT_DATA_LENGTH 32

/**
* fault event struct
* refer to dms_event_obj
*/
struct drvfault_event {
    unsigned int node_type;
    unsigned int node_id;
    struct {
        drvfault_event_severity severity;
        unsigned char sensor_type;   /* @see DMS_SENSOR_TYPE_T defined in dms_sensor_type.h */
        char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH]; /* max 20 bytes */

        unsigned char assertion; /* Fault generation or clearance flag.
                                  *  0-resume,
                                  *  1-occur,
                                  *  2-one time
                                  */
        unsigned char event_type; /* the current fault type,
                                   * @see to dms_sensor_event.event_status
                                   */
        char desc[DMS_MAX_EVENT_DATA_LENGTH];  /* max 32 bytes */
    } event;
    unsigned int alarm_serial_num;
};

/************************************************************************
 * region begin:
 * Safety Fault manager
 ************************************************************************/
struct safety_fault_status {
    unsigned int emu_id;
    unsigned int src_id;
    unsigned int bit_id;

    unsigned long long base_paddr; /* interrupt reg base addr */
    void __iomem *base_vaddr;      /* interrupt reg base vaddr */
    unsigned int fault_status_offset; /* interrupt status reg offset addr */
    unsigned int fault_status;
};

struct safety_fault_info {
    unsigned int dev_id;
    unsigned int fault_reg_num;
    struct safety_fault_status *fault_status_list;
};

struct safety_event {
    unsigned int emu_id;  /* set value from @safety_fault_status */
    unsigned int src_id;  /* set value from @safety_fault_status */
    unsigned int bit_id;  /* set value from @safety_fault_status */

    unsigned char node_type;
    unsigned char sensor_type;
    unsigned short event_type;
    unsigned char extend;
    unsigned char node_id;
    unsigned char sub_node_type;
    unsigned char sub_node_id;
    unsigned char event_severity;
    unsigned char event_assertion;  /* 0:RESUME 1:OCCUR 2:ONE_TIME */
    unsigned char event_serial_num;
    unsigned char reserve[SAFETY_EVENT_RSV_NUM];
};

enum ras_system_type {
    RAS_SYS_AO,
    RAS_SYS_GPU,
    RAS_SYS_IO,
    RAS_SYS_INVALID
};

enum safety_intterrupt_type {
    SAFETY_IRQ_TYPE_LPI = 0,          /* LPI IRQ */
    SAFETY_IRQ_TYPE_SPI = 1,          /* SPI IRQ */
    SAFETY_IRQ_TYPE_ARM_RAS = 2,      /* ARM_RAS IRQ */
	RAS_IRQ_TYPE_SPI = 3,             /* interrupt for RAS */
    RAS_IRQ_TYPE_LPI = 4,             /* interrupt for RAS */
};

enum ras_fault_type {
    RAS_FAULT_FHI,   /* for SAFETY_IRQ_TYPE_ARM_RAS */
    RAS_FAULT_ERI,   /* for SAFETY_IRQ_TYPE_ARM_RAS */
};

struct safety_reg {
    unsigned int offset; /* interrupt status reg offset addr */
    unsigned int mask;
};

#define DRVFAULT_SAFETY_INIT_REGS_NUM  (4u)
#define DRVFAULT_FILED_VALID   (1)
#define DRVFAULT_FILED_INVALID (0)

enum drvfault_enable_type {
    DRVFAULT_INTERRUPT_MASK = 0,
    DRVFAULT_INTERRUPT_ENABLE = 1
};

#define TAISHAN_INTR_ENABLE_BIT    BIT(1)
#define SILS_INTR_ENABLE_BIT       BIT(2)

struct fault_suppression_bit {
    unsigned int bit_trigger_cnt;
    int is_suppression;
    unsigned long long start_suppression_time;
};

struct safety_mask_reg {  /* handle_policy's bit 2 is 0b */
    unsigned int offset;
    unsigned int mask;
};

struct safety_enable_reg { /* handle_policy's bit 2 is 1b */
    unsigned int offset;
    unsigned int value;
};

struct safety_check_mask_regs {
    unsigned char valid_flag;   /* 0 - invalid  1 - valid */
    unsigned int offset;        /* interrupt status reg offset addr */
    unsigned int val;           /* interrupt status reg offset addr */
};
struct safety_intr_enable_regs {
    unsigned char valid_flag;   /* 0 - invalid  1 - valid */
    unsigned int offset;        /* interrupt status reg offset addr */
    unsigned int val;           /* interrupt status reg offset addr */
};

enum drvfault_custom_op_type {
    DRVFAULT_CUSTUM_CONFIG_DISABLE = 0,
    DRVFAULT_CUSTUM_CONFIG_CLEAR = 1,
    DRVFAULT_CUSTUM_CONFIG_ENABLE = 2,
};

struct drvfault_custom_op {
    enum drvfault_custom_op_type custom_config_type;
    unsigned int reserved;
};

typedef int(*safety_irq_handler)(struct safety_fault_info *fault_info,
    unsigned int *event_num, struct safety_event **event_list);
typedef void(*drvfault_resume)(void);

/**
* struct drvfault_base_info - base data of interrupt
* @irq_name:  irq name, same as compatible in dtsi
* @handle_policy: Refer to fault_handle_policy
*      Bit 0: Interrupt enable policy.
*         0b= Enable the IRQ in behind half
*         1b= Enable the IRQ in top half
*      Bit 1: Interrupt enable Flag in TaiShan
*         0b= Use Mask Mode:   0-enable 1-masked
*         1b= Use Enable Mode: 0-disable 1-enable
*      Bit 2: Interrupt enable Flag in SafetyIsland
*         0b= Use Mask Mode:   0-enable 1-masked
*         1b= Use Enable Mode: 0-disable 1-enable
* @safety_irq_func: callback function when interrupt occur
* @drvfault_resume_func: callback function when resume occur
*       background: smmu single bit ecc fault init
* @drvfault_custom_config_func: callback function after clear/mask/checkmast regs be configured
*       background: crg module need custom config regs
*       caution：The parameter struct safety_irq_info * safety_irq cannot be changed.
*       There should be no operations that interfere with the interrupt-proccess in the func.
*/

#define DRVFAULT_IRQ_ENTRY                      \
    struct {                                    \
        unsigned int dev_id;                    \
        enum safety_intterrupt_type irq_type;   \
        char *irq_name;                         \
        unsigned int handle_policy;             \
        safety_irq_handler safety_irq_func;     \
        unsigned int irq_hwinfo_num;            \
        drvfault_resume drvfault_resume_func;   \
        int(*drvfault_custom_config_func)(struct drvfault_custom_op *custom_op, struct safety_irq_info *safety_irq);  \
    }

/**
 * @drvfault_irq_hwinfo_base - hardware base inform,
        must in head of the hwinfo sturct
 *
 * @emu_id: reference to drvfault_emu_id
 * @src_id: value equal to:1 + emu_irq_number / 32
 * @bit_id: value equal to:emu_irq_number % 32
 *
 * @base_paddr: interrupt reg base addr
 * @base_size: interrupt reg base size
 * @base_vaddr: the virtual addr of the @base_paddr,
 *      set NULL when register to faultmng
 * @dfx_reg_num: DFX register numbers
 * @dfx_reg_list: DFX register offset list
 */
#define DRVFAULT_HARDWARE_ENTRY struct { \
        unsigned int emu_id;              \
        unsigned int src_id;              \
        unsigned int bit_id;              \
        unsigned long long base_paddr;    \
        unsigned long long base_size;     \
        void __iomem *base_vaddr;         \
        unsigned int dfx_reg_num;         \
        unsigned int *dfx_reg_list;       \
    }

/************************************************************************/
/* Data structure For Safety Fault Interrupt.
 * Note: The mask val must be the same of following struct:
 *       struct safety_reg status
 *       struct safety_reg clear
 *       struct safety_mask_reg mask
 */
/************************************************************************/
struct safety_irq_hw_info {
    DRVFAULT_HARDWARE_ENTRY;

    struct safety_reg status;      /* interrupt status reg */
    struct safety_reg clear;       /* interrupt clear reg */

    /* enable safety irq in emu */
    union {
        struct safety_mask_reg mask;     /* handle_policy's bit 2 is 0b */
        struct safety_enable_reg enable; /* handle_policy's bit 2 is 1b */
    };

    union {
        /* handle_policy's 1'b is 0b */
        struct safety_check_mask_regs check_mask[DRVFAULT_SAFETY_INIT_REGS_NUM];

        /* handle_policy's 1'b is 1b */
        struct safety_intr_enable_regs check_enable[DRVFAULT_SAFETY_INIT_REGS_NUM];
    };

    struct fault_suppression_bit suppression_bit[32];  /* safety irq internal data */
};

enum fault_handle_policy {
    FHP_IRQ_MASK_IN_TOP = 0x1,      /* default in behind half */
    FHP_IRQ_ENABLE_FLAG_IN_TAISHAN = 0x2,
    FHP_IRQ_ENABLE_FLAG_IN_SILS = 0x3,
};

struct safety_irq_info {
    DRVFAULT_IRQ_ENTRY;
    unsigned int irq_hwinfo_size;  /* each safety irq hw info size */
    struct safety_irq_hw_info *irq_hwinfo_list;
};

/************************************************************************/
/* Data structure For RAS Fault Interrupt                               */
/************************************************************************/
#define DRVFAULT_RAS_MAX_REGS  4

typedef struct {
    unsigned int offset;    /* 0~0xFFFF, This item will be ignored if @len=0 */
    unsigned int len;       /* 0-invalid, Bytes: 4/8 */
    unsigned long long mask;
    unsigned long long val;    /* value */
} ras_reg_t;

struct ras_irq_hw_info {
    DRVFAULT_HARDWARE_ENTRY;

    ras_reg_t status[DRVFAULT_RAS_MAX_REGS];  /* interrupt status reg */
    ras_reg_t clear[DRVFAULT_RAS_MAX_REGS];   /* interrupt clear reg */
    ras_reg_t check_mask[DRVFAULT_SAFETY_INIT_REGS_NUM];   /* ras interrupt mask in TaiShan */

    struct fault_suppression_bit suppression_bit[1];  /* safety irq internal data */
};

struct safety_irq_emu_info {
    unsigned int emu_id;
    unsigned int src_id;
    unsigned int bit_id;
};

struct ras_irq_info {
    DRVFAULT_IRQ_ENTRY;
    unsigned int irq_hwinfo_size;  /* each safety irq hw info size */
    struct ras_irq_hw_info *irq_hwinfo_list;
};

int drvfault_register_safety_irq_info(struct safety_irq_info *irq_info);
int drvfault_unregister_safety_irq_info(unsigned int dev_id, char *irq_name);
int drvfault_ras_irq_notify(const struct safety_irq_info *irq_info);

int drvfault_register_ras_irq_info(struct ras_irq_info *irq_info);
int drvfault_unregister_ras_irq_info(unsigned int dev_id, char *irq_name);

/* safety irq storm exception handler */
typedef int (*drvfault_irq_storm_exception_func)(void);

/*
 * drvfault_irq_storm_exception_register
 *     - register safety irq storm exception func to the fault management framework
 * @irq_storm_handler: callback function when irq storm exception occur
 */
int drvfault_irq_storm_exception_register(drvfault_irq_storm_exception_func irq_storm_handler);

/*
 * drvfault_irq_storm_exception_unregister
 *     - unregister safety irq storm exception func to the fault management framework
 */
int drvfault_irq_storm_exception_unregister(void);

/*
 * dfm_register_safety_irq_hwinfo
 *     - register safety interrupt hardware info to fault manager
 * @devid: device id
 * @safety_irq_infos: interrupt hardware informations, can't be free before module removed
 * @hwinfo_num: numbers of interrupt hardware informations
 *
 * Register the safety/no-arm RAS fault interrupt to the fault management
 * framework. The name of the fault interrupt registered in this function
 * must be the same as the interrupt configuration in the DTS. For details,
 * see the @drvfault_register_safety_irq_info function.
 */
int dfm_register_safety_irq_hwinfo(unsigned int devid,
    struct safety_irq_info *safety_irq_infos, unsigned int hwinfo_num);

/*
 * dfm_unregister_safety_irq_hwinfo
 *     - unregister safety interrupt hardware info from fault manager
 * @devid: device id
 * @safety_irq_infos: interrupt hardware informations
 * @hwinfo_num: numbers of interrupt hardware informations
 *
 * refer to @dfm_register_safety_irq_hwinfo
 */
void dfm_unregister_safety_irq_hwinfo(unsigned int devid,
    struct safety_irq_info *safety_irq_infos, unsigned int hwinfo_num);

/*
 * dfm_register_ras_irq_hwinfo
 *     - register ras interrupt hardware info to fault manager
 * @devid: device id
 * @ras_irq_infos: interrupt hardware informations, can't be free before module removed
 * @hwinfo_num: numbers of interrupt hardware informations
 *
 * Register the RAS fault interrupt to the fault management
 * framework. The name of the fault interrupt registered in this function
 * must be the same as the interrupt configuration in the DTS. For details,
 * see the @drvfault_register_ras_irq_info function.
 */
int dfm_register_ras_irq_hwinfo(unsigned int devid,
    struct ras_irq_info *ras_irq_infos, unsigned int hwinfo_num);

/*
 * dfm_unregister_ras_irq_hwinfo
 *     - unregister ras interrupt hardware info from fault manager
 * @devid: device id
 * @ras_irq_infos: interrupt hardware informations
 * @hwinfo_num: numbers of interrupt hardware informations
 *
 * refer to @dfm_register_ras_irq_hwinfo
 */
void dfm_unregister_ras_irq_hwinfo(unsigned int devid,
    struct ras_irq_info *ras_irq_infos, unsigned int hwinfo_num);

/**
 * drvfault_find_irq_in_dts - find irq name in DTS
 * @irq_name: irq compatible name
 *
 * Search for the @irq_name node in the DTS and try to obtain the irq ID.
 * If the irq ID is obtained successfully, 0 is returned. Otherwise,
 * EEXIST is returned.
 */
int drvfault_find_irq_in_dts(const char *irq_name);

/*
 * drvfault_update_safety_irq_info
 *     - update safety interrupt hardware info to fault manager
 * @irq_info: interrupt hardware informations for update
 * caution: The parameter struct safety_irq_info *irq_info cannot use the memory space which
 * used for interrupt registration before, to avoid modify the original information table.
 * The mask will be updated in the order of registration.
 */
int drvfault_update_safety_irq_info(struct safety_irq_info *irq_info);

/************************************************************************
 * region end:
 * Safety Fault manager
 ************************************************************************/

#define DRVFAULT_IOC_MAGIC      'F'
#define DRVFAULT_MSG_SEND       _IOW(DRVFAULT_IOC_MAGIC, 1, struct DrvFaultIocArg)
#define DRVFAULT_MSG_GET_FAULT_INFO       _IOWR(DRVFAULT_IOC_MAGIC, 2, struct DrvFaultIocArg)
#define DRVFAULT_IOC_MAX        3

#define  DRVFAULT_REPORT_NAME  "drvfault_msg_send_report"
typedef int (*drvfault_report_func)(unsigned int dev_id, struct drvfault_msg_info *msg_info);
typedef int (*safetyirq_timeout_callback)(unsigned int emu_id, unsigned int src_id, unsigned int bit_id,
    unsigned int node_type);

/**
 * report fault event
 * @deprecated
 *
 * This interface will not be used later. advised to use the
 * new API @drvfault_report_dms_event instead.
 */
int drvfault_msg_send_report(unsigned int dev_id, struct drvfault_msg_info *msg_info);
int drvfault_report_dms_event(unsigned int dev_id, const struct drvfault_event *fault_event);

/**
 * drvfault_send_safety_info_to_sils - send safety event to safeyisland
 * @event_num: the number of safety event
 * @events: the safety events
 *
 * This interface sends fault reporting or fault recovery events
 * to the safety island. This interface only supports the syn-
 * chronization of hardware fault information to the EMUs on the
 * safety island.
 *
 * If sending a software fault, use the interface @drvfault_report_to_sils.
 */
void drvfault_send_safety_info_to_sils(unsigned int event_num, struct safety_event *events);

/**
 * drvfault_report_to_sils - send ipc msg to safety island
 * @fault_msg: safety fault message data
 *
 * This interface can send an event to the SafetyIsland at a time.
 * It supports flexible message content filling. Hardware faults
 * or software faults can be sent through this interface.
 */
int drvfault_report_to_sils(struct safety_ipc_msg_payload *fault_msg);

/**
 * drvfault_register_safety_timeout_callback - taishan cann't recieve safety interrupt, safetyisland send ipc.
 * @node_type: Indicates the node type corresponding to the fault.
 * @func: Callback processing function corresponding to node_type
 *
 * The service core cannot receive the interrupt,
 * but the safetyisland core receives the interrupt and sends a notification message to the service core for processing.
 * Each subsystem can register callback through this interface to enhance the maintenance
 * and test capability of the interrupt.
 * This callback is triggered by the IPC receiving function. The IPC receiving function is a software interrupt.
 * Therefore, the registered callback function cannot conflict with the soft interrupt processing.
 * For example, you cannot call ioremap
 */
int drvfault_register_safety_timeout_callback(unsigned int node_type, safetyirq_timeout_callback func,
    struct safety_irq_emu_info *irq_info, unsigned int size);
int drvfault_unregister_safety_timeout_callback(unsigned int node_type);

/**
 * drvfault_safety_timeout_handle - Processing function after the interrupt IPC message is received from the sils.
 * @emu_id: emu id of this fault interrupt
 * @src_id: src id of this fault interrupt
 * @bit_id: bit id of this fault interrupt
 *
 * This processing function is called after the interrupt ipc message is received from the safetyisland.
 */
int drvfault_safety_timeout_handle(unsigned int emu_id, unsigned int src_id, unsigned int bit_id);

#endif

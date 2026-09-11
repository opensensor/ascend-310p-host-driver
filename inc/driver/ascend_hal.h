/**
 * @file ascend_hal.h
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description:
 * Author: huawei
 * Create: 2020-01-21
 * @brief driver interface.
 * @version 1.0
 *
 */


#ifndef __ASCEND_HAL_H__
#define __ASCEND_HAL_H__

#include "ascend_hal_external.h"
#include "ascend_hal_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup driver
 * @brief API major version.
 * @attention major version range form 0x00 to 0xff.
 * when delete API, modify API name, should add major version.
 */
#define __HAL_API_VER_MAJOR 0x07
/**
 * @ingroup driver
 * @brief API minor version.
 * @attention minor version range form 0x00 to 0xff.
 * when add new API, should add minor version.
 */
#define __HAL_API_VER_MINOR 0x24
/**
 * @ingroup driver
 * @brief API patch version,
 * @attention patch version range form 0x00 to 0xff.
 * when modify enum para, struct para add patch version.
 * this means when new API compatible with old API, change patch version
 */
#define __HAL_API_VER_PATCH 0x18

/**
 * @ingroup driver
 * @brief API VERSION NUMBER combines major version, minor version and patch version,
 * @brief example : 020103 means version 0x020103, major 0x02, minor 0x01, patch 0x03
 */
#define __HAL_API_VERSION ((__HAL_API_VER_MAJOR << 16) | (__HAL_API_VER_MINOR << 8) | (__HAL_API_VER_PATCH))

/**
 * @ingroup driver
 * @brief driver unified error numbers.
 * @brief new code must return error numbers based on unified error numbers.
 */
#define HAL_ERROR_CODE_BASE  0x90020000

/**
 * @ingroup driver
 * @brief each error code use definition "HAI_ERROR_CODE(MODULE, ERROR_CODE)"
 * @brief which MODULE is the module and ERROR_CODE is the error number.
 */
#define HAI_ERROR_CODE(MODULE, ERROR_CODE) (HAL_ERROR_CODE_BASE + (ERROR_CODE) + ((MODULE) << 12))
#define HAI_ERROR_CODE_NO_MODULE(ERROR_CODE) ((ERROR_CODE) & 0x00000FFF)
/**
 * @ingroup driver
 * @brief turn deviceID to nodeID
 */
#define DEVICE_TO_NODE(x) x
#define NODE_TO_DEVICE(x) x

/**
 * @ingroup driver
 * @brief memory type
 */
typedef enum tagDrvMemType {
    DRV_MEMORY_HBM, /**< HBM memory on device */
    DRV_MEMORY_DDR, /**< DDR memory on device */
} drvMemType_t;

/**
 * @ingroup driver
 * @brief memcpy kind.
 */
typedef enum tagDrvMemcpyKind {
    DRV_MEMCPY_HOST_TO_HOST,     /**< host to host */
    DRV_MEMCPY_HOST_TO_DEVICE,   /**< host to device */
    DRV_MEMCPY_DEVICE_TO_HOST,   /**< device to host */
    DRV_MEMCPY_DEVICE_TO_DEVICE, /**< device to device */
} drvMemcpyKind_t;

/**
 * @ingroup driver
 * @brief Async memcpy parameter.
 */
typedef struct tagDrvDmaAddr {
    void *dst;   /**< destination address */
    void *src;   /**< source address */
    int32_t len; /**< the number of byte to copy */
    int8_t flag; /**< mulitycopy flag */
} drvDmaAddr_t;

/**
 * @ingroup driver
 * @brief interrupt number that task scheduler set to driver.
 */
typedef enum tagDrvInterruptNum {
    DRV_INTERRUPT_QOS_READY = 0, /**< QoS queue almost empty*/
    DRV_INTERRUPT_REPORT_READY,  /**< Return queue almost full*/
    DRV_INTERRUPT_RESERVED,
} drvInterruptNum_t;

/**
 * @ingroup driver
 * @brief driver command handle.
 */
typedef void *drvCommand_t;

/**
 * @ingroup driver
 * @brief driver task report handle.
 */
typedef void *drvReport_t;

typedef enum tagDrvStatus {
    DRV_STATUS_INITING = 0x0,
    DRV_STATUS_WORK,
    DRV_STATUS_EXCEPTION,
    DRV_STATUS_SLEEP,
    DRV_STATUS_COMMUNICATION_LOST,
    DRV_STATUS_RESERVED,
} drvStatus_t;

typedef enum {
    MODULE_TYPE_SYSTEM = 0,         /**< system info */
    MODULE_TYPE_AICPU,              /**< aicpu info */
    MODULE_TYPE_CCPU,               /**< ccpu_info */
    MODULE_TYPE_DCPU,               /**< dcpu info */
    MODULE_TYPE_AICORE,             /**< AI CORE info */
    MODULE_TYPE_TSCPU = 5,          /**< tscpu info */
    MODULE_TYPE_PCIE,               /**< PCIE info */
    MODULE_TYPE_VECTOR_CORE,        /**< VECTOR CORE info */
    MODULE_TYPE_HOST_AICPU,         /**< Host Aicpu info */
    MODULE_TYPE_QOS,                /**< qos info */
    MODULE_TYPE_MEMORY = 10,        /**< memory info */
    MODULE_TYPE_LOG,                /**< log info */
    MODULE_TYPE_LP,                 /**< lp info */
    MODULE_TYPE_L2BUFF,             /**< l2buff info */
    MODULE_TYPE_CC,                 /**< confidential computing info */
    MODULE_TYPE_UB,                 /**< ub info */

    /* The following types are used by the product */
    MODULE_TYPE_COMPUTING = 0x8000, /**< computing power info */
} DEV_MODULE_TYPE;

typedef enum {
    INFO_TYPE_ENV = 0,
    INFO_TYPE_VERSION,
    INFO_TYPE_MASTERID,
    INFO_TYPE_CORE_NUM,
    INFO_TYPE_FREQUE,
    INFO_TYPE_OS_SCHED = 5,
    INFO_TYPE_IN_USED,
    INFO_TYPE_ERROR_MAP,
    INFO_TYPE_OCCUPY,
    INFO_TYPE_ID,
    INFO_TYPE_IP = 10,
    INFO_TYPE_ENDIAN,
    INFO_TYPE_P2P_CAPABILITY,
    INFO_TYPE_SYS_COUNT,
    INFO_TYPE_MONOTONIC_RAW,
    INFO_TYPE_CORE_NUM_LEVEL = 15,
    INFO_TYPE_FREQUE_LEVEL,
    INFO_TYPE_FFTS_TYPE,
    INFO_TYPE_PHY_CHIP_ID,
    INFO_TYPE_PHY_DIE_ID,
    INFO_TYPE_PF_CORE_NUM = 20,
    INFO_TYPE_PF_OCCUPY,
    INFO_TYPE_WORK_MODE,
    INFO_TYPE_UTILIZATION,
    INFO_TYPE_HOST_OSC_FREQUE,
    INFO_TYPE_DEV_OSC_FREQUE = 25,
    INFO_TYPE_SDID,
    INFO_TYPE_SERVER_ID,
    INFO_TYPE_SCALE_TYPE,
    INFO_TYPE_SUPER_POD_ID,
    INFO_TYPE_ADDR_MODE = 30,
    INFO_TYPE_RUN_MACH,
    INFO_TYPE_CURRENT_FREQ,
    INFO_TYPE_CONFIG,
    INFO_TYPE_UCE_VA,
    INFO_TYPE_HOST_KERN_LOG = 35,
    INFO_TYPE_LP_AIC,
    INFO_TYPE_LP_BUS,
    INFO_TYPE_LP_FREQ_VOLT,
    INFO_TYPE_MAINBOARD_ID,
    INFO_TYPE_HD_CONNECT_TYPE = 40,
    INFO_TYPE_DIE_NUM,
    INFO_TYPE_CC,
    INFO_TYPE_L2BUFF_RESUME,
    INFO_TYPE_L2BUFF_RESUME_CNT,
    INFO_TYPE_SDK_EX_VERSION = 45,
    INFO_TYPE_CUST_OP_ENHANCE,
    INFO_TYPE_PRODUCT_TYPE,
    INFO_TYPE_CHASSIS_ID,
    INFO_TYPE_SUPER_POD_TYPE,
    INFO_TYPE_BOARD_ID = 50,
    INFO_TYPE_VNIC_IP,
    INFO_TYPE_SPOD_VNIC_IP,
    INFO_TYPE_UB_STATUS,
    INFO_TYPE_UUID,
    INFO_TYPE_UB_PACKET_STATISTICS = 55,
    INFO_TYPE_UB_QOS_INFO,
    INFO_TYPE_UB_CONFIG_INFO,
    INFO_TYPE_QOS_MASTER_CONFIG,
} DEV_INFO_TYPE;

/**
 * @ingroup driver
 * @brief Get computing power value parameter.
 */
typedef enum {
    INFO_TYPE_COMPUTING_TOKEN = 0,
    INFO_TYPE_MAX_TOKEN,
} INFO_TYPE_COMPUTING;

typedef enum {
    PHY_INFO_TYPE_CHIPTYPE = 0,
    PHY_INFO_TYPE_MASTER_ID,
    PHY_INFO_TYPE_PHY_CHIP_ID,
 	PHY_INFO_TYPE_PHY_DIE_ID,
} PHY_DEV_INFO_TYPE;

typedef enum {
    DEVS_INFO_TYPE_TOPOLOGY = 0,
} PAIR_DEVS_INFO_TYPE;

#define TOPOLOGY_HCCS       0
#define TOPOLOGY_PIX        1
#define TOPOLOGY_PIB        2
#define TOPOLOGY_PHB        3
#define TOPOLOGY_SYS        4
#define TOPOLOGY_SIO        5
#define TOPOLOGY_HCCS_SW    6

typedef enum {
    ADDR_MODE_INDEPENDENT = 0,
    ADDR_MODE_UNIFIED,
} ADDR_MODE_TYPE;

typedef enum {
    RUN_MACHINE_PHYCICAL = 0,
    RUN_MACHINE_VIRTUAL,
} RUN_MACHINE_TYPE;

#define PROCESS_SIGN_LENGTH  49
#define PROCESS_RESV_LENGTH  4

#define COMPUTING_TOKEN_TYPE_INVALID 0xFF
#define COMPUTING_TOKEN_LAD0TKEN01 1
#define COMPUTING_TOKEN_LAD0TKEN02 2
#define COMPUTING_POWER_MAX_VALUE  65535
#define COMPUTING_POWER_MIN_VALUE  0

struct computing_token {
    float value;
    unsigned char type;
    unsigned char reserve_c;
    unsigned short reserve_s;
};

struct process_sign {
    pid_t tgid;
    char sign[PROCESS_SIGN_LENGTH];
    char resv[PROCESS_RESV_LENGTH];
};

enum devdrv_process_type {
    DEVDRV_PROCESS_CP1 = 0,   /* aicpu_scheduler */
    DEVDRV_PROCESS_CP2,       /* custom_process */
    DEVDRV_PROCESS_DEV_ONLY,  /* TDT */
    DEVDRV_PROCESS_QS,        /* queue_scheduler */
    DEVDRV_PROCESS_HCCP,      /* hccp server */
    DEVDRV_PROCESS_USER,      /* user proc, can bind many on host or device. not surport quert from host pid */
    DEVDRV_PROCESS_CPTYPE_MAX,
};

#define HAL_BIND_ALL_DEVICE 0xffffffff
#define HAL_QUERY_RESV_LENGTH 8
struct halQueryDevpidInfo {
    pid_t hostpid;
    uint32_t devid;
    uint32_t vfid;
    enum devdrv_process_type proc_type;
    char resv[HAL_QUERY_RESV_LENGTH];
};

/**
 * @ingroup driver
 * @brief  get device info when open device
 */
struct drvDevInfo {
#ifndef __linux
    mmProcess fd;
#else
    int fd;
#endif
};

typedef enum {
    CMD_TYPE_POWERON,
    CMD_TYPE_POWEROFF,
    CMD_TYPE_CM_ALLOC,
    CMD_TYPE_CM_FREE,
    CMD_TYPE_SC_FREE,
    CMD_TYPE_MAX,
} devdrv_cmd_type_t;

typedef enum {
    MEM_TYPE_PCIE_SRAM = 0,
    MEM_TYPE_PCIE_DDR,
    MEM_TYPE_IMU_DDR,
    MEM_TYPE_BBOX_DDR,
    MEM_TYPE_BBOX_HDR,
    MEM_TYPE_REG_SRAM,
    MEM_TYPE_REG_DDR,
    MEM_TYPE_TS_LOG,
    MEM_TYPE_HBOOT_SRAM,
    MEM_TYPE_DEBUG_OS_LOG,
    MEM_TYPE_SEC_LOG,
    MEM_TYPE_RUN_OS_LOG,
    MEM_TYPE_RUN_EVENT_LOG,
    MEM_TYPE_DEBUG_DEV_LOG,
    MEM_TYPE_KDUMP_MAGIC,
    MEM_TYPE_VMCORE_FILE,
    MEM_TYPE_VMCORE_STAT,
    MEM_TYPE_CHIP_LOG_PCIE_BAR,
    MEM_TYPE_TS_LOG_PCIE_BAR,
    MEM_TYPE_BBOX_PCIE_BAR,
    MEM_CTRL_TYPE_MAX,
} MEM_CTRL_TYPE;

typedef struct tag_alloc_cm_para {
    void **ptr;
    uint64_t size;
} devdrv_alloc_cm_para_t;

typedef struct tag_free_cm_para {
    void *ptr;
} devdrv_free_cm_para_t;

typedef enum {
    DRVDEV_CALL_BACK_SUCCESS = 0,
    DRVDEV_CALL_BACK_FAILED,
} devdrv_callback_state_t;

typedef enum {
    GO_TO_SO = 0,
    GO_TO_SUSPEND,
    GO_TO_S3,
    GO_TO_S4,
    GO_TO_D0,
    GO_TO_D3,
    GO_TO_DISABLE_DEV,
    GO_TO_ENABLE_DEV,
    GO_TO_STATE_MAX,
} devdrv_state_t;

typedef struct tag_state_info {
    devdrv_state_t state;
    uint32_t devId;
} devdrv_state_info_t;

struct drvNotifyInfo {
    uint32_t tsId;
    uint32_t notifyId;
    uint64_t devAddrOffset;
};

struct drvIpcNotifyInfo {
    uint32_t tsId;
    uint32_t devId;
    uint32_t notifyId;
};

struct drvTsExceptionInfo {
    uint32_t tsId;
    uint64_t exception_code;
};

#define HAL_MAX_EVENT_NAME_LENGTH 256
#define HAL_MAX_EVENT_DATA_LENGTH 32
#define HAL_MAX_EVENT_RESV_LENGTH 32

#define HAL_EVENT_FILTER_FLAG_EVENT_ID  (1UL << 0)
#define HAL_EVENT_FILTER_FLAG_SERVERITY (1UL << 1)
#define HAL_EVENT_FILTER_FLAG_NODE_TYPE (1UL << 2)
#define HAL_EVENT_FILTER_FLAG_HOST_PID  (1UL << 3)

struct halEventFilter {
    unsigned long long filter_flag; /* bit0: event_id; bit1: severity; bit2: node_type; bit3: current tgid */
    unsigned int event_id;
    unsigned char severity;
    unsigned short node_type;

    unsigned char resv[HAL_MAX_EVENT_RESV_LENGTH]; /* reserve 32byte */
};

struct halFaultEventInfo {
    unsigned long long alarm_raised_time;
    unsigned int event_id;
    int tgid;
    int event_serial_num;
    int notify_serial_num;
    unsigned short deviceid;
    unsigned short node_type;
    unsigned short sub_node_type;
    unsigned char node_id;
    unsigned char sub_node_id;
    unsigned char severity;
    unsigned char assertion;
    char event_name[HAL_MAX_EVENT_NAME_LENGTH];
    char additional_info[HAL_MAX_EVENT_DATA_LENGTH];
    unsigned char os_id;
    unsigned char resv[HAL_MAX_EVENT_RESV_LENGTH]; /* reserve 32byte */
};

typedef void (*halfaulteventcb)(struct halFaultEventInfo *halFaultEvent);

#define CAP_RESERVE_SIZE 30

#define CAP_MEM_SUPPORT_HBM      (1)          /**< mem support  for HBM */
#define CAP_MEM_SUPPORT_L2BUFFER (1 << 1)     /**< mem support  for L2BUFFER */

#define CAP_SDMA_REDUCE_FP32   (1)         /**< sdma_reduce support for FP32 */
#define CAP_SDMA_REDUCE_FP16   (1 << 1)    /**< sdma_reduce support for FP16 */
#define CAP_SDMA_REDUCE_INT16  (1 << 2)    /**< sdma_reduce support for INT16 */

#define CAP_SDMA_REDUCE_INT4   (1 << 3)    /**< sdma_reduce support for INT4 */
#define CAP_SDMA_REDUCE_INT8   (1 << 4)    /**< sdma_reduce support for INT8 */
#define CAP_SDMA_REDUCE_INT32  (1 << 5)    /**< sdma_reduce support for INT32 */
#define CAP_SDMA_REDUCE_BFP16  (1 << 6)    /**< sdma_reduce support for BFP16 */
#define CAP_SDMA_REDUCE_BFP32  (1 << 7)    /**< sdma_reduce support for BFP32 */
#define CAP_SDMA_REDUCE_UINT8  (1 << 8)    /**< sdma_reduce support for UINT8 */
#define CAP_SDMA_REDUCE_UINT16 (1 << 9)    /**< sdma_reduce support for UINT16 */
#define CAP_SDMA_REDUCE_UINT32 (1 << 10)   /**< sdma_reduce support for UINT32 */

#define CAP_SDMA_REDUCE_KIND_ADD   (1)           /**< sdma_reduce support for ADD */
#define CAP_SDMA_REDUCE_KIND_MAX   (1 << 1)      /**< sdma_reduce support for MAX */
#define CAP_SDMA_REDUCE_KIND_MIN   (1 << 2)      /**< sdma_reduce support for MIN */
#define CAP_SDMA_REDUCE_KIND_EQUAL (1 << 3)      /**< sdma_reduce support for EQUAL */

struct halCapabilityInfo {
    uint32_t sdma_reduce_support; /**< bit for CAP_SDMA_REDUCE_* */
    uint32_t memory_support;      /**< bit for CAP_MEM_SUPPORT_* */
    uint32_t ts_group_number;
    uint32_t sdma_reduce_kind;    /**< bit for CAP_SDMA_REDUCE_KIND_* */
    uint32_t res[CAP_RESERVE_SIZE];
};

#define COMPUTE_GROUP_INFO_RES_NUM 5
#define AICORE_MASK_NUM            2

/* devdrv ts identifier for get ts group info */
typedef enum {
    TS_AICORE = 0,
    TS_AIVECTOR,
}DRV_TS_ID;

struct capability_group_info {
    unsigned int  group_id;
    unsigned int  state; // 0: not create, 1: created
    unsigned int  extend_attribute; // 0: default group attribute
    unsigned int  aicore_number; // 0~9
    unsigned int  aivector_number; // 0~7
    unsigned int  sdma_number; // 0~15
    unsigned int  aicpu_number; // 0~15
    unsigned int  active_sq_number; // 0~31
    unsigned int  aicore_mask[AICORE_MASK_NUM]; // as output in dsmi_get_capability_group_info/halGetCapabilityGroupInfo
    unsigned int  vfid;
    unsigned int  poolid;
    unsigned int  poolid_max;
    unsigned int  res[COMPUTE_GROUP_INFO_RES_NUM - AICORE_MASK_NUM];
};

#define MAX_CHIP_NAME 32
typedef struct hal_chip_info {
    unsigned char type[MAX_CHIP_NAME];
    unsigned char name[MAX_CHIP_NAME];
    unsigned char version[MAX_CHIP_NAME];
} halChipInfo;

typedef devdrv_callback_state_t (*drvDeviceStateNotify)(devdrv_state_info_t *state);

typedef int (*drvDeviceExceptionReporFunc)(uint32_t devId, uint32_t exceptionId, struct timespec timeStamp);
typedef int (*drvDeviceStartupNotify)(uint32_t num, uint32_t *devId);

typedef struct hal_dev_open_in {
    uint64_t reserve[8];   // reserved parameter
} halDevOpenIn;

typedef struct hal_dev_open_out {
    uint64_t reserve[8];   // reserved parameter
} halDevOpenOut;

typedef struct hal_dev_close_in {
    uint32_t close_type;   /* Reference DEV_CLOSE_TYPE */
    uint32_t reserve0;
    uint64_t reserve[7];   // reserved parameter
} halDevCloseIn;

typedef enum {
    DEV_CLOSE_HOST_DEVICE = 0,
    DEV_CLOSE_ONLY_HOST, /* only used when backup device switches */
    DEV_CLOSE_HOST_USER, /* only used when process snapshot restore */
    DEV_CLOSE_TYPE_MAX,
} DEV_CLOSE_TYPE;

/**
* @ingroup driver
* @brief This interface is used to invoke the unified device open interfaces of driver components.
* @attention
*   1) This interface cannot be invoked repeatedly.
*   2) This interface cannot be used together with the open or close interface of an independent module.
* @param [in]  devId device id
* @param [in]  in reserved parameter, user must set 0
* @param [out]  out reserved parameter
* @return 0 success, others for fail
*/
DLLEXPORT drvError_t halDeviceOpen(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out);
/**
* @ingroup driver
* @brief This interface is used to invoke the unified device close interfaces of driver components.
* @attention
*   1) This interface cannot be invoked repeatedly.
*   2) This interface cannot be used together with the open or close interface of an independent module.
*   3) The halDeviceOpen interface must be invoked before this interface is used.
* @param [in]  devId device id
* @param [in]  in reserved parameter, user must set 0
* @return 0 success, others for fail
*/
DLLEXPORT drvError_t halDeviceClose(uint32_t devid, halDevCloseIn *in);

typedef struct hal_proc_res_backup_info {
    uint64_t reserve[8];   // reserved parameter
} halProcResBackupInfo;

typedef struct hal_proc_res_restore_info {
    uint64_t reserve[8];   // reserved parameter
} halProcResRestoreInfo;

/**
* @ingroup driver
* @brief Back up the resources requested by the process
* @attention Support repeated calls
* @param [in] halProcResBackupInfo info
* @return 0 success, others for fail
*/
DLLEXPORT drvError_t halProcessResBackup(halProcResBackupInfo *info);

/**
* @ingroup driver
* @brief Restore the resources requested by the process
* @attention null
* @param [in] halProcResRestoreInfo info
* @return 0 success, others for fail
*/
DLLEXPORT drvError_t halProcessResRestore(halProcResRestoreInfo *info);

/**
* @ingroup driver
* @brief Black box status notification callback function registration Interface
* @attention null
* @param [in] drvDeviceStateNotify state_callback
* @return 0 success, others for fail
*/
DLLEXPORT drvError_t drvDeviceStateNotifierRegister(drvDeviceStateNotify state_callback);
/**
* @ingroup driver
* @brief Black box status Start up callback function registration
* @attention null
* @param [in] startup_callback  callback function poiniter
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceStartupRegister(drvDeviceStartupNotify startup_callback);
/**
* @ingroup driver
* @brief get chip capbility information
* @attention null
* @param [in]  devId device id
* @param [out]  info chip capbility information
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t halGetChipCapability(uint32_t devId, struct halCapabilityInfo *info);

/**
* @ingroup driver
* @brief get ts group info
* @attention null
* @param [in]  devId device id
* @param [in]  ts_id ts id 0 : TS_AICORE, 1 : TS_AIVECTOR
* @param [in]  group_id group id
* @param [in]  group_count group count
* @param [out]  info ts group info
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t halGetCapabilityGroupInfo(int device_id, int ts_id, int group_id,
    struct capability_group_info *group_info, int group_count);

/**
* @ingroup driver
* @brief get hal API Version
* @attention null
* @param [out]  halAPIVersion version of hal API
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t halGetAPIVersion(int *halAPIVersion);

/**
* @ingroup driver
* @brief set runtime API Version
* @attention null
* @param [in] must just be __HAL_API_VERSION
* @return
*/
DLLEXPORT void halSetRuntimeApiVer(int Version);

/**
* @ingroup driver
* @brief get device availability information
* @attention null
* @param [in] devId  device id
* @param [out] status  device status
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceStatus(uint32_t devId, drvStatus_t *status);
/**
* @ingroup driver
* @brief open device
* @attention:
*   1)it will return error when reopen device
*   2)assure invoked TsdOpen successfully, before invoke this api
* @param [in] devId  device id
* @param [out] devInfo  device information
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceOpen(void **devInfo, uint32_t devId);
/**
* @ingroup driver
* @brief close device
* @attention it will return error when reclose device
* @param [in] devid  device id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceClose(uint32_t devId);
/**
* @ingroup driver
* @brief Get the dma handling method of the device
* @attention The transport method based on the source and destination addresses should be implemented
* by the runtime layer. However, since the mini and cloud implementation methods are different,
* the runtime does not have a corresponding macro partition, so DRV sinks to the kernel state and adds
* the macro partition
* @param [in] src  unused
* @param [in] dest unused
* @param [out] trans_type trans type which has two types:
*              DRV_SDMA = 0x0;  SDMA mode move
*              DRV_PCIE_DMA = 0x1;  PCIE mode move
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceGetTransWay(void *src, void *dest, uint8_t *trans_type);
/**
* @ingroup driver
* @brief Get current platform information
* @attention null
* @param [out] *info  0 Means currently on the Device side, 1/Means currently on the host side
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetPlatformInfo(uint32_t *info);
/**
* @ingroup driver
* @brief Get the current number of devices
* @attention null
* @param [out] num_dev  Number of current devices
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetDevNum(uint32_t *num_dev);
/**
* @ingroup driver
* @brief Convert device-side devId to host-side devId
* @attention null
* @param [in] localDevId  chip ID
* @param [out] devId  host side devId
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetDevIDByLocalDevID(uint32_t localDevId, uint32_t *devId);
/**
* @ingroup driver
* @brief Get the probe device list
* @attention null
* @param [in] len  device list length
* @param [out] *devices  device phyical id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetDevProbeList(uint32_t *devices, uint32_t len);
/**
* @ingroup driver
* @brief The device side and the host side both obtain the host IDs of all the current devices.
* If called in a container, get the host IDs of all devices in the current container.
* @attention null
* @param [in]  len  Array length
* @param [out] devices  device id Array
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetDevIDs(uint32_t *devices, uint32_t len);
/**
* @ingroup driver
* @brief Get the chip IDs of all current devices
* @attention null
* @param [in]  len  Array length
* @param [out] devices  device id Array
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetDeviceLocalIDs(uint32_t *devices, uint32_t len);
/**
* @ingroup driver
* @brief Get device id via host-side device physical id , only called in device side.
* @attention null
* @param [in]  host_dev_id  host-side device physical id
* @param [out] local_dev_id  device id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetLocalDevIDByHostDevID(uint32_t host_dev_id, uint32_t *local_dev_id);

typedef enum {
    HAL_DMS_DEV_TYPE_BASE_SERVCIE = 0x600,
    HAL_DMS_DEV_TYPE_PROC_MGR = 0x601,
    HAL_DMS_DEV_TYPE_IAMMGR = 0x602,
    HAL_DMS_DEV_TYPE_PROC_LAUNCHER = 0x603,
    HAL_DMS_DEV_TYPE_ADDA = 0x604,
    HAL_DMS_DEV_TYPE_DMP_DAEMON = 0x605,
    HAL_DMS_DEV_TYPE_SKLOGD = 0x606,
    HAL_DMS_DEV_TYPE_SLOGD = 0x607,
    HAL_DMS_DEV_TYPE_LOG_DAEMON = 0x608,
    HAL_DMS_DEV_TYPE_HDCD = 0x609,
    HAL_DMS_DEV_TYPE_AICPU_SCH = 0x60B,
    HAL_DMS_DEV_TYPE_QUEUE_SCH = 0x60C,
    HAL_DMS_DEV_TYPE_AICPU_CUST_SCH = 0x60E,
    HAL_DMS_DEV_TYPE_HCCP = 0x60F,
    HAL_DMS_DEV_TYPE_TSD_DAEMON = 0x610,
    HAL_DMS_DEV_TYPE_TIMER_SERVER = 0x616,
    HAL_DMS_DEV_TYPE_OS_LINUX = 0x617,
    HAL_DMS_DEV_TYPE_DATA_MASTER = 0x619,
    HAL_DMS_DEV_TYPE_CFG_MGR = 0x61A,
    HAL_DMS_DEV_TYPE_DATA_GW = 0x61D,
    HAL_DMS_DEV_TYPE_RESMGR = 0x623,
    HAL_DMS_DEV_TYPE_DRV_KERNEL = 0x625,
    HAL_DMS_DEV_TYPE_MAX
} HAL_DMS_DEVICE_NODE_TYPE;

typedef enum {
    HAL_DMS_SEN_TYPE_HEARTBEAT = 0x27,
    HAL_DMS_SEN_TYPE_GENERAL_SOFTWARE_FAULT = 0xD0,
} HAL_DMS_SENSOR_TYPE_T;

typedef enum {
    HAL_HEARTBEAT_ERROR_TYPE_LOST = 0x00,
    HAL_HEARTBEAT_ERROR_TYPE_LOST2 = 0x01,
    HAL_HEARTBEAT_ERROR_TYPE_RECOVER = 0x02,
    HAL_HEARTBEAT_ERROR_TYPE_ERR_TYPE_MAX
} HAL_HEARTBEAT_ERROR_T;

typedef enum {
    HAL_GENERAL_SOFTWARE_FAULT_PROCESS_START_FAILED_OR_EXIT = 0x00,
    HAL_GENERAL_SOFTWARE_FAULT_MEMORY_OVER_LIMIT = 0x01,
    HAL_GENERAL_SOFTWARE_FAULT_NORMAL_RESOURCE_RECYCLE_FAILED = 0x05,
    HAL_GENERAL_SOFTWARE_FAULT_CRITICAL_RESOURCE_RECYCLE_FAILED = 0x06,
    HAL_GENERAL_SOFTWARE_FAULT_ERR_TYPE_MAX
} HAL_GENERAL_SOFTWARE_FAULT_ERR_TYPE_T;

struct halSensorNodeCfg {
    char name[20]; /* 20: max name len */
    unsigned short NodeType; /* HAL_DMS_DEVICE_NODE_TYPE */
    unsigned char SensorType;
    unsigned char Resv; /* used for byte alignment */
    unsigned int AssertEventMask;    /* bit position is 1:event enable; bit position is 0:event disable */
    unsigned int DeassertEventMask;  /* bit position is 1:fault event;  bit position is 0:notify event(one time) */
    unsigned char Reserve[32]; /* 32: reserve bytes */
};

typedef enum {
    GENERAL_EVENT_TYPE_RESUME   = 0,
    GENERAL_EVENT_TYPE_OCCUR    = 1,
    GENERAL_EVENT_TYPE_ONE_TIME = 2,
    GENERAL_EVENT_TYPE_MAX
} halGeneralEventType_t;

/**
* @ingroup driver
* @brief Register device and sensor node by device id and user node cfg.
* @attention null
* @param [in]  devId  Device ID
* @param [in]  cfg    user cfg
* @param [out] handle return user handle
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSensorNodeRegister(uint32_t devId, struct halSensorNodeCfg *cfg, uint64_t *handle);

/**
* @ingroup driver
* @brief Unregister device and sensor node by device id and user Handle.
* @attention null
* @param [in] devId  Device ID
* @param [in] handle user sensor handle
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSensorNodeUnregister(uint32_t devId, uint64_t handle);

/**
* @ingroup driver
* @brief Set sensor val by device id and user Handle.
* @attention null
* @param [in] devId          Device ID
* @param [in] handle         user sensor handle
* @param [in] val            user event value
* @param [in] assertion      user event type
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSensorNodeUpdateState(uint32_t devId, uint64_t handle, int val,
    halGeneralEventType_t assertion);

/**
* @ingroup driver
* @brief Get Soc Version
* @attention null
* @param [in] devId          Device ID
* @param [out] socVersion    soc version
* @param [in] len            soc version length
* @return   0 for success, others for fail
*/
drvError_t halGetSocVersion(uint32_t devId, char *socVersion, uint32_t len);

/**
* @ingroup driver
* @brief Get device information, CPU information and PCIe bus information.
* @attention each  moduleType  and infoType will get a different info value.
* if the type you input is not compatitable with the table below, then will return fail
* aicpu/ctrlcpu bitmap: the value indicates the total number of CPUs on device side,
*                       and which bit in the map corresponds to which kind of CPU.
* --------------------------------------------------------------------------------------------------------
* moduleType                |        infoType             |    value                    |   attention    |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_ENV              |   env type                  |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_VERSION          |   hardware_version          |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_MASTERID         |   masterId                  | used in host   |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_CORE_NUM         |   ts_num                    |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SYS_COUNT        |   system count              |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_MONOTONIC_RAW    |   MONOTONIC_RAW time        |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_PHY_CHIP_ID      |   physical chip id          |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_PHY_DIE_ID       |   physical die id           |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_HOST_OSC_FREQUE  |   host OSC Frequency        |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_DEV_OSC_FREQUE   |   device OSC Frequency      |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SDID             |   super pod SDID            |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SERVER_ID        |   super pod server ID       |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SCALE_TYPE       |   super pod scale type      |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SUPER_POD_ID     |   super pod ID              |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_ADDR_MODE        |   address mode              |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_RUN_MACH         |   phycical or virtul machine|                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_MAINBOARD_ID     |   mainboard id              |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_CUST_OP_ENHANCE  |   customer get OP enhance   |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_BOARD_ID         |   board id                  | used in device |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_VNIC_IP          |   device vnic ip            | used in device |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SPOD_VNIC_IP     |   device vnic ip by sdid    | used in device |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_AICPU         |  INFO_TYPE_CORE_NUM         |   ai cpu number(vcpu in vf) |                |
* MODULE_TYPE_AICPU         |  INFO_TYPE_OS_SCHED         |   ai cpu in os sched        | used in device |
* MODULE_TYPE_AICPU         |  INFO_TYPE_IN_USED          |   ai cpu in used            |                |
* MODULE_TYPE_AICPU         |  INFO_TYPE_ERROR_MAP        |   ai cpu error map          |                |
* MODULE_TYPE_AICPU         |  INFO_TYPE_ID               |   ai cpu id                 |                |
* MODULE_TYPE_AICPU         |  INFO_TYPE_OCCUPY           |   ai cpu occupy bitmap      |                |
* MODULE_TYPE_AICPU         |  INFO_TYPE_PF_CORE_NUM      |   PF ai cpu core num        | used in device |
* MODULE_TYPE_AICPU         |  INFO_TYPE_PF_OCCUPY        |   PF ai cpu occupy bitmap   | used in device |
* MODULE_TYPE_AICPU         |  INFO_TYPE_UTILIZATION      |   ai cpu utilization        |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_CCPU          |  INFO_TYPE_CORE_NUM         |   ctrl cpu number           |                |
* MODULE_TYPE_CCPU          |  INFO_TYPE_ID               |   ctrl cpu id               |                |
* MODULE_TYPE_CCPU          |  INFO_TYPE_OCCUPY           |   ctrl cpu occupy bitmap    |                |
* MODULE_TYPE_CCPU          |  INFO_TYPE_IP               |   ctrl cpu ip               |                |
* MODULE_TYPE_CCPU          |  INFO_TYPE_ENDIAN           |   ctrl cpu ENDIAN           |                |
* MODULE_TYPE_CCPU          |  INFO_TYPE_OS_SCHED         |   ctrl cpu in os sched      | used in device |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_DCPU          |  INFO_TYPE_CORE_NUM         |   data cpu number           | used in device |
* MODULE_TYPE_DCPU          |  INFO_TYPE_OS_SCHED         |   data cpu in os sched      | used in device |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_AICORE        |  INFO_TYPE_CORE_NUM         |   ai core number            |                |
* MODULE_TYPE_AICORE        |  INFO_TYPE_CORE_NUM_LEVEL   |   ai core number level      |                |
* MODULE_TYPE_AICORE        |  INFO_TYPE_IN_USED          |   ai core in used           |                |
* MODULE_TYPE_AICORE        |  INFO_TYPE_ERROR_MAP        |   ai core error map         |                |
* MODULE_TYPE_AICORE        |  INFO_TYPE_ID               |   ai core id                |                |
* MODULE_TYPE_AICORE        |  INFO_TYPE_FREQUE           |   ai core rated frequence   |                |
* MODULE_TYPE_AICORE        |  INFO_TYPE_FREQUE_LEVEL     |   ai core frequence level   |                |
* MODULE_TYPE_AICORE        |  INFO_TYPE_UTILIZATION      |   ai core utilization       |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_VECTOR_CORE   |   INFO_TYPE_CORE_NUM        |   vector core number        |                |
* MODULE_TYPE_VECTOR_CORE   |   INFO_TYPE_FREQUE          |   vector core frequence     |                |
* MODULE_TYPE_VECTOR_CORE   |   INFO_TYPE_UTILIZATION     |   vector core utilization   |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_TSCPU         |  INFO_TYPE_CORE_NUM         |   ts cpu number             |                |
* MODULE_TYPE_TSCPU         |  INFO_TYPE_OS_SCHED         |   ts cpu in os sched        | used in device |
* MODULE_TYPE_TSCPU         |  INFO_TYPE_FFTS_TYPE        |   ts cpu ffts type          |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_PCIE          |  INFO_TYPE_ID               |   pcie bdf                  | used in host   |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_HOST_AICPU    |  INFO_TYPE_CORE_NUM         |   host aicpu num            | used in host   |
* MODULE_TYPE_HOST_AICPU    |  INFO_TYPE_OCCUPY           |   host aicpu bitmap(64byte) | used in host   |
* MODULE_TYPE_HOST_AICPU    |  INFO_TYPE_WORK_MODE        |   host aicpu work mode      | used in host   |
* MODULE_TYPE_HOST_AICPU    |  INFO_TYPE_FREQUE           |   host aicpu frequency      | used in host   |
* --------------------------------------------------------------------------------------------------------
* @param [in] devId  Device ID, when parameter infoType is set to INFO_TYPE_MASTERID, need to use physical device ID.
*             In other cases, need to use logical device ID.
*             Note: The physical ID used by INFO_TYPE_MASTERID is a known issue.
*                   Currently, the log and black box modules use this function.
* @param [in] moduleType  See enum DEV_MODULE_TYPE
* @param [in] infoType  See enum DEV_INFO_TYPE
* @param [out] value  device info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);

#define HAL_CPM_DATA_SIZE 256
typedef struct {
    unsigned short voltage;
    unsigned char max_volt_fall;
    unsigned char core_num;
    unsigned char cpm_data[HAL_CPM_DATA_SIZE];
} HAL_LP_GET_CPM_STRU;

// lp stress cfg type
enum {
    HAL_STRESS_ADJ_AIC,
    HAL_STRESS_ADJ_BUS,
    HAL_STRESS_ADJ_L2CACHE,
    HAL_STRESS_ADJ_MATA,
    HAL_STRESS_ADJ_CPU,
    HAL_STRESS_ADJ_HBM,
    HAL_STRESS_ADJ_MAX
};

// lp stress set restore
enum {
    HAL_STRESS_VOLT_SET,
    HAL_STRESS_VOLT_RESTORE,
    HAL_STRESS_FREQ_SET,
    HAL_STRESS_FREQ_RESTORE,
    HAL_STRESS_FUNC_OPEN,
    HAL_STRESS_FUNC_CLOSE,
    HAL_STRESS_SET_RESTORE_MAX
};

struct hal_soc_stress_cfg {
    unsigned char type;
    unsigned char set_restore;
    unsigned short value;
};

#define HAL_LPM_SOC_STRESS_RESV_LEN 24
#define COMPONENT_CFG_MAGIC   0x636F6D70U  /* comp */
struct hal_component_id_cfg {
    unsigned int magic;
    unsigned int component_id;
};

typedef struct {
    struct hal_soc_stress_cfg cfg;
    struct hal_component_id_cfg component_cfg;
    unsigned char resv[HAL_LPM_SOC_STRESS_RESV_LEN]; /* reserve, write 0 */
} HAL_LP_SET_STRESS_TEST_STRU;

typedef struct hal_fault_occurr_syscnt {
    unsigned int reserved; /* for byte alignment */
    unsigned int event_id;
    unsigned long long sys_cnt;
} HAL_FAULT_OCCUR_SYSCNT_STRU;

/**
* @ingroup driver
* @brief Get device information, CPU information and PCIe bus information.
* @attention each moduleType and infoType will get a different
* if the type you input is not compatitable with the table below, then will return fail
* --------------------------------------------------------------------------------------------------------
* moduleType                |        infoType             |    value                    |   attention    |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_QOS           |  INFO_TYPE_CONFIG           |   qos config information    |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_QOS           |  INFO_TYPE_QOS_MASTER_CONFIG|  qos master cfg information |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_AICORE        |  INFO_TYPE_CURRENT_FREQ     |   ai core current frequence |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_MEMORY        |  INFO_TYPE_UCE_VA           |   UCE VA num and VA info    |                |
* MODULE_TYPE_MEMORY        |  INFO_TYPE_SYS_COUNT        | HAL_FAULT_OCCUR_SYSCNT_STRU |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_LOG           |  INFO_TYPE_HOST_KERN_LOG    |   host kern log information |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_LP            |  INFO_TYPE_LP_AIC           |   HAL_LP_GET_CPM_STRU       |                |
* MODULE_TYPE_LP            |  INFO_TYPE_LP_BUS           |   HAL_LP_GET_CPM_STRU       |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_CC            |  INFO_TYPE_CC               |   HAL_CC_INFO               | used in device |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_L2BUFF        | INFO_TYPE_L2BUFF_RESUME_CNT |   L2buff fault resume cnt   |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SDK_EX_VERSION   |    SDK extend version       |                |
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_UUID             |      device uuid            |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_AICORE        |  INFO_TYPE_OCCUPY           |    Aicore bitmap,           |
                                                             max bit number is 128      |  buff：u64 aicore_bitmap[2]
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_UB            |  INFO_TYPE_UB_STATUS        |    UB connection status     |                |
* MODULE_TYPE_UB            |INFO_TYPE_UB_PACKET_STATISTICS|   UB packet statistics     |                |
* MODULE_TYPE_UB            |  INFO_TYPE_UB_QOS_INFO      |    UB QOS info              |                |
* MODULE_TYPE_UB            |  INFO_TYPE_UB_CONFIG_INFO   |    UB config info           |                |
* --------------------------------------------------------------------------------------------------------
* @param [in] devId  Device ID, when parameter infoType is set to INFO_TYPE_MASTERID, need to use physical device ID.
*             In other cases, need to use logical device ID.
*             Note: The physical ID used by INFO_TYPE_MASTERID is a known issue.
*                   Currently, the log and black box modules use this function.
* @param [in] moduleType  See enum DEV_MODULE_TYPE
* @param [in] infoType  See enum DEV_INFO_TYPE
* @param [in/out] buf input and output buffer
* @param [in/out] size input buffer size and output data size
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetDeviceInfoByBuff(uint32_t devId, int32_t moduleType,
                                            int32_t infoType, void *buf, int32_t *size);

/**
* @ingroup driver
* @brief Setting the device configuration
* @attention Each moduleType and infoType will have a different configuration.
* if the type you input is not compatitable with the table below, then will return fail
* --------------------------------------------------------------------------------------------------------
* moduleType                |        infoType             |             buf             |   attention    |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_LP            |  INFO_TYPE_LP_FREQ_VOLT     | HAL_LP_SET_STRESS_TEST_STRU |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_L2BUFF        |  INFO_TYPE_L2BUFF_RESUME    |   L2buff fault resume       |                |
* --------------------------------------------------------------------------------------------------------
* MODULE_TYPE_SYSTEM        |  INFO_TYPE_SDK_EX_VERSION   |    SDK extend version       | used in device |
* --------------------------------------------------------------------------------------------------------
* @param [in] devId  Device ID, when parameter infoType is set to INFO_TYPE_MASTERID, need to use physical device ID.
*             In other cases, need to use logical device ID.
*             Note: The physical ID used by INFO_TYPE_MASTERID is a known issue.
*                   Currently, the log and black box modules use this function.
* @param [in] moduleType  See enum DEV_MODULE_TYPE
* @param [in] infoType  See enum DEV_INFO_TYPE
* @param [in] buf input buffer
* @param [in] size input buffer size
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSetDeviceInfoByBuff(uint32_t devId, int32_t moduleType,
                                            int32_t infoType, void *buf, int32_t size);

/**
* @ingroup driver
* @brief Get device info using physical device id
* @attention each  moduleType  and infoType will get a different
* if the type you input is not compatitable with the table below, then will return fail
* ------------------------------------------------------------------------------------------
* moduleType            |        infoType           |    value                |   attention    |
* ------------------------------------------------------------------------------------------
* MODULE_TYPE_SYSTEM    | PHY_INFO_TYPE_CHIPTYPE    |   chip type             | used in host   |
* MODULE_TYPE_SYSTEM    | PHY_INFO_TYPE_MASTER_ID   |   masterId              | used in host   |
* MODULE_TYPE_SYSTEM    | PHY_INFO_TYPE_PHY_CHIP_ID |   physical chip id      |                |
* MODULE_TYPE_SYSTEM    | PHY_INFO_TYPE_PHY_DIE_ID  |   physical die id       |                |
* ------------------------------------------------------------------------------------------
* @param [in] phyId  Device physical ID
* @param [in] moduleType  See enum DEV_MODULE_TYPE
* @param [in] infoType  See enum PHY_DEV_INFO_TYPE
* @param [out] value  device info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetPhyDeviceInfo(uint32_t phyId, int32_t moduleType, int32_t infoType, int64_t *value);

/**
* @ingroup driver
* @brief Get devices relationship, etc
* @attention This interface can be invoked only on the host side.
* @param [in] devId  Device ID
* @param [in] otherDevId  other device id compared
* @param [in] infoType  See enum PAIR_DEVS_INFO_TYPE
* @param [out] value   type of relationship
* *value == TOPOLOGY_HCCS, means relationship is hccs
* *value == TOPOLOGY_PIX,  means relationship is pix
* *value == TOPOLOGY_SIO,  means relationship is sio
* *value == TOPOLOGY_HCCS_SW,  means relationship is hccs_sw
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetPairDevicesInfo(uint32_t devId, uint32_t otherDevId, int32_t infoType, int64_t *value);

/**
* @ingroup driver
* @brief Get devices relationship, etc
* @attention This interface can be invoked only on the host side.
* @param [in] devId  Physical Device ID
* @param [in] otherDevId  other physical device id compared
* @param [in] infoType  See enum PAIR_DEVS_INFO_TYPE
* @param [out] value   relationship type between the devices
* *value == TOPOLOGY_HCCS, means relationship is hccs
* *value == TOPOLOGY_PIX,  means relationship is pix
* *value == TOPOLOGY_PIB, means relationship is pib
* *value == TOPOLOGY_PHB,  means relationship is phb
* *value == TOPOLOGY_SYS, means relationship is sys
* *value == TOPOLOGY_SIO,  means relationship is sio
* *value == TOPOLOGY_HCCS_SW,  means relationship is hccs_sw
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetPairPhyDevicesInfo(uint32_t devId, uint32_t otherDevId, int32_t infoType, int64_t *value);

/**
* @ingroup driver
* @brief Used to define the unique interface of the product. The cmd command word remains unified, compatible,
* and functions are implemented independently
* @attention only support lite
* @param [in] devId  Device ID
* @param [in] cmd  cmd command word
* @param [in] para parameter for cmd
* @param [out] None, can be passed in para
* @return    0 for success, others for fail
*/
DLLEXPORT drvError_t drvCustomCall(uint32_t devId, uint32_t cmd, void *para);
/**
* @ingroup driver
* @brief The black box daemon on the host side calls the interface registration exception reporting function
* @attention null
* @param [in] exception_callback_func  Exception reporting function
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceExceptionHookRegister(drvDeviceExceptionReporFunc exception_callback_func);
/**
* @ingroup driver
* @brief Flash cache interface
* @attention
* 1.Virtual address is the virtual address of this process; 2.Note whether the length passed in meets the requirements
* @param [in] base  Virtual address base address
* @param [in] len  cache length
* @return   0 for success, others for fail
*/
DLLEXPORT void drvFlushCache(uint64_t base, uint32_t len);
/**
* @ingroup driver
* @brief Get physical ID (phyId) using logical ID (devIndex)
* @attention null
* @param [in] devIndex  Logical ID
* @param [out] phyId  Physical ID
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceGetPhyIdByIndex(uint32_t devIndex, uint32_t *phyId);
/**
* @ingroup driver
* @brief Get logical ID (devIndex) using physical ID (phyId)
* @attention null
* @param [in] phyId   Physical ID
* @param [out] devIndex  Logical ID
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex);
/**
* @ingroup driver
* @brief host process random flags get interface
* @attention null
* @param [out] sign  host process random flag
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetProcessSign(struct process_sign *sign);
/**
* @ingroup driver
* @brief query devpid by info
* @attention null
* @param [in] info: See struct halQueryDevpidInfo
* @param [out] dev_pid: device pid correspond to info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueryDevpid(struct halQueryDevpidInfo info, pid_t *dev_pid) ASCEND_HAL_WEAK;

struct drvBindHostpidInfo {
    pid_t host_pid;
    uint32_t vfid;
    uint32_t chip_id;
    int32_t mode;
    enum devdrv_process_type cp_type;
    uint32_t len;
    char sign[PROCESS_SIGN_LENGTH];
};

/**
* @ingroup driver
* @brief Bind Device custom-process to aicpu-process
* @attention Must have a paired hostpid and a paired aicpupid
* @param [in] info  See struct drvBindHostpidInfo
              host_pid: hops pid
              chip_id:  chip id
              mode:     mode, online:0, offline:1
              cp_type:  type of custom-process
              len:      lenth of sign
              sign:     sign of hostpid
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_XXX : bind fail
*/
DLLEXPORT drvError_t drvBindHostPid(struct drvBindHostpidInfo info);
/**
* @ingroup driver
* @brief Unbind Device custom-process to aicpu-process
* @attention The hostpid and aicpuid must be bound through the drvBindHostPid interface.
* @param [in] info  See struct drvBindHostpidInfo
              host_pid: hops pid
              chip_id:  chip id
              mode:     mode, online:0, offline:1
              cp_type:  type of custom-process
              len:      lenth of sign
              sign:     sign of hostpid
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_XXX : unbind fail
*/
DLLEXPORT drvError_t drvUnbindHostPid(struct drvBindHostpidInfo info);
/**
* @ingroup driver
* @brief Query the binding information of the devpid
* @attention Must have a paired hostpid and a paired aicpupid
* If a process binds more than one type, the cp_type with the smaller value is returned.
* @param [in] pid: dev pid
              chip_id:  chip id
              vfid:  vf id
              host_pid: host pid
              cp_type:  type of custom-process
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_XXX : query fail
*/
DLLEXPORT drvError_t drvQueryProcessHostPid(int pid, unsigned int *chip_id, unsigned int *vfid,
    unsigned int *host_pid, unsigned int *cp_type);

/**
* @ingroup driver
* @brief  soc res addr map for slave process.
* @param [in] devId: logic devid
* @param [in] res_info: see struct res_addr_info
* @param [out] addr: visual addr in target process for resouse reg addr
* @param [out] len: resouse addr len
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResAddrMap(unsigned int devId, struct res_addr_info *res_info,
    unsigned long *va, unsigned int *len);

/**
* @ingroup driver
* @brief  soc res addr unmap for slave process.
* @param [in] devId: logic devid
* @param [in] res_info: see struct res_addr_info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResAddrUnmap(unsigned int devId, struct res_addr_info *res_info);

/**
* @ingroup driver
* @brief set process into aicpu tasks
* @attention null
* @param [in] bindType: bind cgroup type
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halBindCgroup(BIND_CGROUP_TYPE bindType);

/**
* @ingroup driver
* @brief Sharing memory to specific object
* @attention null
* @param [in] para: see struct drvMemSharingPara
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSetMemSharing(struct drvMemSharingPara *para);

/**
* @ingroup driver
* @brief Get non-container internal Tgid number
* @attention null
* @return Tgid number (non-container Tgid)
*/
DLLEXPORT pid_t drvDeviceGetBareTgid(void);
/**
* @ingroup driver
* @brief HP/DELL/LENOVO PC send I2C reset cmd to Device
* @attention only support HP/DELL/LENOVO PC + EVB VB
* @param [in] devId  : Device ID
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvResetDevice(uint32_t devId);
/**
* @ingroup driver
* @brief map kernel space for ddrdump
* @attention null
* @param [in] devId  : Device ID
* @param [in] virAddr : user space addr
* @param [out] size : kernel space size
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDmaMmap(uint32_t devId, uint64_t virAddr, uint32_t *size);
/**
* @ingroup driver
* @brief read value from bbox hdr addr
* @attention offset + len <= bbox hdr len(512KB)
* @param [in]  devId  : device physical id
* @param [in]  memType: MEM_CTRL_TYPE
* @param [in]  offset : bbox hdr offset
* @param [in]  len : length of read content
* @param [out] value : read value
* @return   0 for success, others for fail
* ---------------------------------------------------------------------------------------------------------------------
*     MEM_CTRL_TYPE           | log dump mode |     log location        |                   attention                  |
* ---------------------------------------------------------------------------------------------------------------------
* MEM_TYPE_PCIE_SRAM          | PCIe bar dump | /mntn/hbm.txt           | IMU startup log                              |
*                             |               | /mntn/snapshot.txt      | Startup Snapshot Log Before DDR Init Success |
*                             |               | /mntn/bios_hiss-r52.txt | Checkpoint logs before BIOS startup Success  |
* MEM_TYPE_PCIE_DDR           | PCIe bar dump | /log/kernel.log         | OS kernel log                                |
* MEM_TYPE_IMU_DDR            |    DMA dump   | /log/imu_boot.log       | IMU startup log                              |
*                             |               | /log/imu_run.log        | IMU run logs                                 |
* MEM_TYPE_BBOX_DDR           |    DMA dump   | /bbox/                  | Bbox reserved space maintenance and test log |
* MEM_TYPE_BBOX_HDR           | PCIe bar dump | /snapshot/hdr.log       | snapshot info log                            |
* MEM_TYPE_REG_SRAM           | PCIe bar dump | /mntn/chip_dfx_min.txt  | Log of Minimal Set of Chip DFX Registers     |
* MEM_TYPE_REG_DDR            |    DMA dump   | /mntn/chip_dfx_full.txt | Full Set of Chip DFX Registers               |
* MEM_TYPE_TS_LOG             |    DMA dump   | /log/ts.log             | TS info log                                  |
* MEM_TYPE_HBOOT_SRAM         | PCIe bar dump | /mntn/hboot.txt         | Hboot Log Before HBM Init Success            |
* MEM_TYPE_DEBUG_OS_LOG       |    DMA dump   | /slog/debug/device-os   | device ccpu system process debug log         |
* MEM_TYPE_SEC_LOG            |    DMA dump   | /slog/security          | device ccpu system process security log      |
* MEM_TYPE_RUN_OS_LOG         |    DMA dump   | /slog/run/device-os     | device ccpu system process run log           |
* MEM_TYPE_RUN_EVENT_LOG      |    DMA dump   | /slog/run/event         | device ccpu system process event log         |
* MEM_TYPE_DEBUG_DEV_LOG      |    DMA dump   | /slog/debug/device-id   | device other than ccpu sys process debug log |
* MEM_TYPE_KDUMP_MAGIC        |               |                         |                                              |
* MEM_TYPE_VMCORE_FILE        |               |                         |                                              |
* MEM_TYPE_VMCORE_STAT        |               |                         |                                              |
* MEM_TYPE_CHIP_LOG_PCIE_BAR  | PCIe bar dump | /mntn/chip_dfx_full.txt | Full Set of Chip DFX Registers               |
* MEM_TYPE_TS_LOG_PCIE_BAR    | PCIe bar dump | /log/ts.log             | TS info log                                  |
* MEM_TYPE_BBOX_PCIE_BAR      | PCIe bar dump | /bbox/                  | Bbox reserved space maintenance and test log |
* ---------------------------------------------------------------------------------------------------------------------
* In SMP scenario, devId can transfer only master id when drvMemRead is invoked to obtain OS-level log information.
*/
DLLEXPORT drvError_t drvMemRead(uint32_t devId, MEM_CTRL_TYPE memType, uint32_t offset, uint8_t *value, uint32_t len);
/**
* @ingroup driver
* @brief write value to bbox address
* @attention null
* @param [in]  devId  : device physical id
* @param [in]  memType: MEM_CTRL_TYPE
* @param [in]  offset : bbox offset
* @param [in]  len : length of write content
* @param [out] value : write value
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvMemWrite(uint32_t devId, MEM_CTRL_TYPE memType, uint32_t offset, uint8_t *value, uint32_t len);

/**
* @ingroup driver
* @brief pcie pre-reset, release pcie related resources applied by each module on the host side
* @attention All functions of pcie are invalid after calling
* @param [in]  devId  : Device ID
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvPciePreReset(uint32_t devId);
/**
* @ingroup driver
* @brief pcie rescan, re-apply the pcie related resources required by each module on the host side
* @attention All functions of pcie are invalid after calling
* @param [in]  devId  : Device ID
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvPcieRescan(uint32_t devId);
#define DRV_P2P_STATUS_ENABLE 1
#define DRV_P2P_STATUS_DISABLE 0
/**
* @ingroup driver
* @brief p2p Enable interface
* @attention
* 1. Both directions must be set to take effect, and support sdma and vnic interworking.
* 2. When using device access host memory, the MAX enable number will be reduced.
* 3. The two devices of Ascend310P Duo share 8 MAX enable number.
* 4. dev and peer_dev cannot be equal or share the same PCIE device.
* 5. VF devices are not supported.
* -----------------------------------------------------------
* Chip Type        |   Support phy_id   | MAX enable number |
* ------------------ ----------------------------------------
* Ascend910A1      |        0~15        |        8          |
* Ascend910A2      |        0~15        |        8          |
* Ascend910A3      |        0~15        |        8          |
* Ascend310P       |  0/2/4...34/36/38  |        8          |
* Ascend310P Duo   |        0~39        |        8(shared)  |
* Ascend310B       |     Not Support    |    Not Support    |
* -----------------------------------------------------------
* @param [in]  dev : Logical device id
* @param [in]  peer_dev : Physical device id
* @param [in]  flag : reserve para fill 0
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halDeviceEnableP2P(uint32_t dev, uint32_t peer_dev, uint32_t flag);
/**
* @ingroup driver
* @brief p2p Disable interface
* @attention
* 1. Both directions must be set to take effect, and support sdma and vnic interworking.
* 2. When using device access host memory, the MAX enable number will be reduced.
* 3. The two devices of Ascend310P Duo share 8 MAX enable number.
* 4. dev and peer_dev cannot be equal or share the same PCIE device.
* 5. VF devices are not supported.
* -----------------------------------------------------------
* Chip Type        |   Support phy_id   | MAX enable number |
* ------------------ ----------------------------------------
* Ascend910A1      |        0~15        |        8          |
* Ascend910A2      |        0~15        |        8          |
* Ascend910A3      |        0~15        |        8          |
* Ascend310P       |  0/2/4...34/36/38  |        8          |
* Ascend310P Duo   |        0~39        |        8(shared)  |
* Ascend310B       |     Not Support    |    Not Support    |
* -----------------------------------------------------------
* @param [in]  dev : Logical device id
* @param [in]  peer_dev : Physical device id
* @param [in]  flag : reserve para fill 0
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halDeviceDisableP2P(uint32_t dev, uint32_t peer_dev, uint32_t flag);
/**
* @ingroup driver
* @brief p2p Status interface
* @attention Both directions must be set to take effect, and support sdma and vnic interworking
* @param [in]  dev : Logical device id
* @param [in]  peer_dev : Physical device id
* @param [out]  0 for disable, 1 for enable
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetP2PStatus(uint32_t dev, uint32_t peer_dev, uint32_t *status);
/**
* @ingroup driver
* @brief p2p if can access peer interface
* @attention null
* @param [out]  canAccessPeer : 0 for disable, 1 for enable
* @param [in]  dev : Logical device id
* @param [in]  peer_dev : Physical device id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halDeviceCanAccessPeer(int *canAccessPeer, uint32_t dev, uint32_t peer_dev);
/**
* @ingroup driver
* @brief host get device boot status
* @attention null
* @param [in]  phy_id : Physical device id
* @param [out] boot_status : See dsmi_boot_status definition
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvGetDeviceBootStatus(int phy_id, uint32_t *boot_status);
/**
* @ingroup driver
* @brief Get offset address of notify id
* @attention null
* @param [in]  devId  Device number
* @param [in] info  See struct drvNotifyInfo which includes:
*               tsId: ts id,  ascend310:0, ascend910 :0
*              notifyId, the range of values for notify id [0,1023]
* @param [out] *info: devAddrOffset:  Physical address offset
* @return    0 for success, others for fail
*/
DLLEXPORT drvError_t drvNotifyIdAddrOffset(uint32_t devId, struct drvNotifyInfo *info);
/**
* @ingroup driver
* @brief drvCreateIpcNotify
* @attention null
* @param [in] name  Ipc notify name to be created
* @param [in] len  name lenth, length should be at least 65
* @param [in] info  See struct drvIpcNotifyInfo
*              tsId: ts id,  ascend310:0, ascend910 :0
*              devId: device id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvCreateIpcNotify(struct drvIpcNotifyInfo *info, char *name, unsigned int len);
/**
* @ingroup driver
* @brief Destroy ipc notify
* @attention null
* @param [in] *name  Ipc notify name to be destroyed
* @param [in] *info  See struct drvIpcNotifyInfo
*             tsId: ts id,  ascend310:0, ascend910 :0
*             devId: device id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvDestroyIpcNotify(const char *name, struct drvIpcNotifyInfo *info);
/**
* @ingroup driver
* @brief Open ipc notify
* @attention null
* @param [in] name  Ipc notify name to open
* @param [in] info  See struct drvIpcNotifyInfo
*             tsId: ts id,  ascend310:0, ascend910 :0
*             devId: device id
* @param [out] *info  *notifyId  Return opened notification id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvOpenIpcNotify(const char *name, struct drvIpcNotifyInfo *info);
/**
* @ingroup driver
* @brief Close ipc notify
* @attention null
* @param [in] name  Ipc notify name to close
* @param [in] info  See struct drvIpcNotifyInfo
*              tsId: ts id,  ascend310:0, ascend910 :0
*              devId: device id
*              notifyId : notification id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvCloseIpcNotify(const char *name, struct drvIpcNotifyInfo *info);
/**
* @ingroup driver
* @brief Set the notification pid whitelist
* @attention null
* @param [in] name  Ipc notify name to be set
* @param [in] pid[]  Array of whitelisted processes
* @param [in] num  number of whitelisted processes
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvSetIpcNotifyPid(const char *name, pid_t pid[], int num);
/**
* @ingroup driver
* @brief record notify register
* @attention null
* @param [in] name: Ipc notify name
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvRecordIpcNotify(const char *name);
/**
* @ingroup driver
* @brief create shared id
* @attention null
* @param [in] name  share id name to be created
* @param [in] len  name lenth
* @param [in] info  See struct drvShrIdInfo
*              devid: device id
*              tsid: ts id
*              id_type: resource id type
*              shrid: shared resource id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdCreate(struct drvShrIdInfo *info, char *name, uint32_t name_len);
/**
* @ingroup driver
* @brief destroy shared id
* @attention null
* @param [in] name  share id name to be destroyed
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdDestroy(const char *name);
/**
* @ingroup driver
* @brief open shared id
* @attention null
* @param [in] name  share id name to be opened
* @param [out] info  shrid  Return opened shared id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdOpen(const char *name, struct drvShrIdInfo *info);
/**
* @ingroup driver
* @brief close shared id handle
* @attention null
* @param [in] name  share id name to be closed
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdClose(const char *name);
/**
* @ingroup driver
* @brief Set the attribute of shared id by name
* @attention
   Type is SHR_ID_ATTR_NO_WLIST_IN_SERVER:
   1.If the set pid related API has been called, it returns DRV_ERROR_OPER_NOT_PERMITTED;
   2.If it is not called by the ipc notify create process, it returns DRV_ERROR_OPER_NOT_PERMITTED;
   3.Currently attr.enableFlag is not supported SHRID_WLIST_ENABLE.
* @param [in] name used for sharing between processes, get by halShrIdCreate
* @param [in] type Type of shared id attribute settings
* @param [in] attr  corresponding to type to set shared id priority
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdSetAttribute(const char *name, enum shrIdAttrType type, struct shrIdAttr attr);
/**
* @ingroup driver
* @brief get the attribute of shared id by name
* @attention Available online, not offline.
* @param [in] name Name used for sharing between processes
* @param [in] type Type of shared id attribute settings
* @param [out] attr Related attribute information about name
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdGetAttribute(const char *name, enum shrIdAttrType type, struct shrIdAttr *attr);
/**
* @ingroup driver
* @brief get the info of of shared id by name
* @attention Available online, not offline.
* @param [in] name Name used for sharing between processes
* @param [out] info Related information about name
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halShrIdInfoGet(const char *name, struct shrIdGetInfo *info);
/**
* @ingroup driver
* @brief set the share id pid whitelist
* @attention mutually exclusive with halShrIdSetAttribute interface
* @param [in] name  share id name to be set
* @param [in] pid[]  array of whitelisted processes
* @param [in] pid_num  number of whitelisted processes
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdSetPid(const char *name, pid_t pid[], uint32_t pid_num);
/**
* @ingroup driver
* @brief set the share id pid whitelist with sdid
* @attention mutually exclusive with halShrIdSetAttribute interface
* @param [in] name  share id name to be set
* @param [in] sdid whitelisted sdid
* @param [in] pid  whitelisted process
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdSetPodPid(const char *name, uint32_t sdid, pid_t pid);
/**
* @ingroup driver
* @brief record shared id
* @attention null
* @param [in] name  share id name to be recorded
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halShrIdRecord(const char *name);
/**
* @ingroup driver
* @brief cqsq channel positioning information
* @attention null
* @param [in] devId  Device ID
* @return void
*/
DLLEXPORT void drvDfxShowReport(uint32_t devId);
/**
* @ingroup driver
* @brief send IPC msg to safetyIsland
* @attention null
* @param [in]   devId  Device ID
*               msg : message contents
                msgSize : message size
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSafeIslandTimeSyncMsgSend(uint32_t devId, void *msg, size_t msgSize);

#define DV_OFFLINE
#define DV_ONLINE
#define DV_OFF_ONLINE
#define DV_LITE

#define ADVISE_TYPE (1UL)       /**< 0: DDR memory 1: HBM memory */
#define ADVISE_EXE (1UL << 1)   /**< setting executable permissions */
#define ADVISE_THP (1UL << 2)   /**< using huge page memory */
#define ADVISE_PLE (1UL << 3)   /**< memory prefetching */
#define ADVISE_PIN (1UL << 4)   /**< pin ddr memory */
#define ADVISE_UNPIN (1UL << 5) /**< unpin ddr memory */

/* mem register flag */
#define MEM_PROC_TYPE_BIT 16
#define MEM_REGISTER_HCCP_PROC_TYPE (DEVDRV_PROCESS_HCCP << MEM_PROC_TYPE_BIT)

typedef struct drv_mem_handle drv_mem_handle_t;

struct drv_mem_prop {
    uint32_t side;       /* enum drv_mem_side */
    uint32_t devid;      /* devid is numa id, if side == MEM_HOST_NUMA_SIDE; otherwise devid is device id */
    uint32_t module_id;  /* module id defines in ascend_hal_define.h */

    uint32_t pg_type;    /* enum drv_mem_pg_type */
    uint32_t mem_type;   /* enum drv_mem_type */
    uint64_t reserve;
};

struct memcpy_info {
    drvMemcpyKind_t dir;
    uint32_t devid;
    uint64_t reserve[2];   // should be zero
};

struct drv_process_status_output {
    uint32_t result;
    uint32_t reserve[8];   // should be zero
};

typedef enum {
    MEM_ALLOC_GRANULARITY_MINIMUM = 0x0,
    MEM_ALLOC_GRANULARITY_RECOMMENDED,
    MEM_ALLOC_GRANULARITY_INVALID,
} drv_mem_granularity_options;

typedef UINT32 DVmem_advise;
typedef UINT32 DVdevice;
typedef UINT64 DVdeviceptr;
typedef drvError_t DVresult;

#define DV_MEM_LOCK_HOST        0x0008
#define DV_MEM_LOCK_DEV         0x0010
#define DV_MEM_LOCK_DEV_DVPP    0x0020
#define DV_MEM_LOCK_HOST_AGENT  0x0040
#define DV_MEM_USER_MALLOC      0x0080
#define DV_MEM_USER_REGISTER    0x0100

#define DV_MEM_RESV 8
struct DVattribute {
    /**< DV_MEM_SVM_DEVICE : svm memory & mapped device */
    /**< DV_MEM_SVM_HOST   : svm memory & mapped host */
    /**< DV_MEM_SVM        : svm memory & no mapped */
    /**< DV_MEM_LOCK_HOST  : host mapped memory & lock host */
    /**< DV_MEM_LOCK_DEV   : dev mapped memory & lock dev */
    /**< DV_MEM_LOCK_DEV_DVPP   : dev_dvpp mapped memory & lock dev */
    /**< DV_MEM_LOCK_HOST_AGENT : host agent mapped memory & lock host */
    /**< DV_MEM_USER_MALLOC     : not svm addr range, default user malloc */
    /**< DV_MEM_USER_REGISTER   :
     * not svm addr range, need to call halSupportFeature(devid, FEATURE_SVM_MEM_REGISTER_QUERY_AND_GET_ATTR) firstly,
     * then call halHostRegister. only in this way can get this mem type.
     */
    UINT32 memType;
    UINT32 resv1;
    UINT32 resv2;

    UINT32 devId;
    UINT32 pageSize;
    UINT32 resv[DV_MEM_RESV];
};

#define DV_LOCK_HOST 0x0001
#define DV_LOCK_DEVICE 0x0002
#define DV_UNLOCK 0

#define SVM_AGENT_DEVICE 0U
#define SVM_AGENT_HOST 1U
/**
* @ingroup driver
* @brief Set memory allocation strategy for memory range segments
* @attention
* 1. Ensure that the device id corresponding to the execution CPU where the thread calling the interface is consistent
* with the dev_id in the parameter (ids all start from 0), that is, the interface only supports setting the allocation
* strategy of the memory range segment on the device where the current execution thread is located;
* 2. Currently only offline scenarios are supported;
* @param [in] devPtr  Unsigned long, Start address of memory range segment
* @param [in] len  Unsigned long, the size of the memory range segment
* @param [in] type  Signed shaping, the type of memory range segment, currently only supports three: 0 (Local DDR),
* 1 (Local HBM), 2 (Cross HBM)
* @param [in] dev_id  device id
* @return DRV_ERROR_INVALID_VALUE : parameter error, unsupported type or board node number error
* @return DRV_ERROR_INVALID_DEVICE : device id error
* @return DRV_ERROR_MBIND_FAIL : internel memory bind fail
* @return DRV_ERROR_NONE : success
 */
DLLEXPORT DV_OFFLINE DVresult drvMbindHbm(DVdeviceptr devPtr, size_t len, uint32_t type, uint32_t dev_id);
/**
* @ingroup driver
* @brief Load a certain length of data to the specified position of L2BUF
* @attention: offline mode:
* 1. User guarantees *vPtr points to the correct L2BUFF mapped virtual space starting address
* 2. The current interface only takes effect the first time it is successfully invoked throughout the OS life cycle
* @param [in] deviceId  Unsigned shaping, device id, this value is not used in offline scenarios
* @param [in] program  void pointer, a program to be loaded
* @param [in] offset  Offset value of the position to be loaded from the L2BUF starting address, in bytes
* @param [in] ByteCount  The length of the program to be loaded
* @param [out] vPtr  The start address of L2BUF is used as the input. After the load is completed, the address of
* the start position of the load is used as the output
* @return DRV_ERROR_INVALID_VALUE : Parameter error, null pointer, offset exceeds l2buf size,
* copy data exceeds l2buf range, etc
* @return DRV_ERROR_FILE_OPS : Internal error, file operation failed;
* @return DRV_ERROR_IOCRL_FAIL : Internal error, IOCTL operation failed;
* @return DRV_ERROR_INVALID_HANDLE : Internal error, loading program error
* @return DRV_ERROR_NONE : success
*/
DLLEXPORT DV_OFF_ONLINE DVresult drvLoadProgram(DVdevice deviceId, void *program, unsigned int offset,
    size_t ByteCount, void **vPtr);
/**
* @ingroup driver
* @brief Get the corresponding physical address based on the entered virtual address
* @attention
* 1. After applying for memory, you need to call the advise interface to allocate physical memory, and then
* call this interface. That is, the user should ensure that the page table has been established in the space where
* the virtual address is located to ensure that the corresponding physical address is correctly obtained
* @param [in] vptr  Unsigned 64-bit integer, the device memory address must be the shared memory requested
* @param [out] pptr Unsigned 64-bit integer. The corresponding physical address is returned. The value is valid
* when the return is successful
* @return DRV_ERROR_INVALID_HANDLE : parameter error, pointer is empty, addr is zero.
* @return DRV_ERROR_FILE_OPS : internel error, file operation failed.
* @return DRV_ERROR_IOCRL_FAIL : Internal error, IOCTL operation failed
* @return DRV_ERROR_NONE : success
*/
DLLEXPORT DV_OFF_ONLINE DVresult drvMemAddressTranslate(DVdeviceptr vptr, UINT64 *pptr);
/**
* @ingroup driver
* @brief Get the TTBR and substreamid of the current process
* @attention Can be called multiple times, it is recommended that Runtime be called once; the result record can be
* saved and can be used next time in this process
* @param [in] device  Unsigned shaping, device id, this value is not used in offline scenarios
* @param [out] SSID  Returns the SubStreamid of the current process
* @return DRV_ERROR_INVALID_VALUE : Parameter error, pointer is empty
* @return DRV_ERROR_FILE_OPS : Internal error, file operation failed
* @return DRV_ERROR_IOCRL_FAIL : Internal error, IOCTL operation failed
* @return DRV_ERROR_NONE : success
*/
DLLEXPORT DV_OFF_ONLINE DVresult drvMemSmmuQuery(DVdevice device, UINT32 *SSID);
/**
* @ingroup driver
* @brief Map the L2buff to the process address space, establish page table, and obtain the starting virtual address
* of the current process L2buff and the corresponding PTE
* @attention 1. It can only be called once during initialization, and a page will be created internally, and multiple
* calls are prohibited; it is released when the process exits.
* @param [in] device  Unsigned shaping, device id, this value is not used in offline scenarios
* @param [out] l2buff  Double pointer, returns a pointer to the starting virtual address of the L2buff
* @param [out] pte  Reserved param
* @return DRV_ERROR_INVALID_HANDLE :  Parameter error, pointer is empty
* @return DRV_ERROR_FILE_OPS :  Internal error, file operation failed
* @return DRV_ERROR_IOCRL_FAIL :  Internal error, IOCTL operation failed
* @return DRV_ERROR_NONE : success
*/
DLLEXPORT DV_OFF_ONLINE DVresult drvMemAllocL2buffAddr(DVdevice device, void **l2buff, UINT64 *pte);
/**
* @ingroup driver
* @brief Release L2buff address space, should be used in conjunction with drvMemAllocL2buffAddr
* @attention null
* @param [in] device  Unsigned shaping, device id, this value is not used in offline scenarios
* @param [in] l2buff  Pointer to the starting virtual address space of L2buff
* @return DRV_ERROR_INVALID_HANDLE : Parameter error, pointer is empty
* @return DRV_ERROR_NONE : success
*/
DLLEXPORT DV_OFF_ONLINE DVresult drvMemReleaseL2buffAddr(DVdevice device, void *l2buff);
/**
* @ingroup driver
* @brief Set the initial memory value according to 8bits (device physical address, unified virtual address
* are supported)
* @attention
*  1. Make sure that the destination buffer can store at least num characters.
*  2. The interface supports processing data larger than 2G
* online:
*  1. The memory to be initialized needs to be on the Host or both on the Device side
*  2. The memory management module is not responsible for the length check of ByteCount. Users need to ensure
*  that the length is legal.
* @param [in] dst  Unsigned 64-bit integer, memory address to be initialized
* @param [in] destMax  The maximum number of valid initial memory values that can be set
* @param [in] value  8-bit unsigned, initial value set
* @param [in] num  Set the initial length of the memory space in bytes
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_INVALID_VALUE : The destination address is 0 and the number of values is 0
* @return DRV_ERROR_INVALID_HANDLE : Internal error, setting failed
*/
DLLEXPORT DV_OFF_ONLINE DVresult drvMemsetD8(DVdeviceptr dst, size_t destMax, UINT8 value, size_t num);
/**
* @ingroup driver
* @brief Copy the data in the source buffer to the destination buffer synchronously
* @attention
* 1. The destination buffer must have enough space to store the contents of the source buffer to be copied.
* 2. (offline) This interface cannot process data larger than 2G
* 3. Share mem addr not support D2D copy, including ipc open and mem export, use may result in unexpected behavior.
* @param [in] dst  Unsigned 64-bit integer, memory address to be initialized
* @param [in] destMax  The maximum number of valid initial memory values that can be set
* @param [in] value  16-bit unsigned, initial value set
* @param [in] num  Set the number of memory space initial values
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_INVALID_HANDLE : Internal error, copy failed
*/
DLLEXPORT DV_OFF_ONLINE DVresult drvMemcpy(DVdeviceptr dst, size_t destMax, DVdeviceptr src, size_t ByteCount);

/**
* @ingroup driver
* @brief Copy the data in the source buffer to the destination buffer synchronously
* @attention
* 1. The destination buffer must have enough space to store the contents of the source buffer to be copied.
* 2. Only support d2h copy.
* @param [in] dst destination address
* @param [in] dstSize destination memory region size
* @param [in] src source address
* @param [in] count size of the buffer to be copy
* @param [in] info copy information
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_INVALID_HANDLE : Internal error, copy failed
*/
DLLEXPORT drvError_t halMemcpy(void *dst, size_t dstSize, void *src, size_t count, struct memcpy_info *info);

/**
* @ingroup driver
* @brief Copy the data in the source buffer to the destination buffer asynchronously
* @attention
* 1. The destination buffer must have enough space to store the contents of the source buffer to be copied.
* 2. (offline) (virtual machine logical grouping) not support
* 3. The max num of async copy tasks being processed simultaneously is 65535.
* @param [in] dst  Unsigned 64-bit integer, memory address to be initialized
* @param [in] destMax  The maximum number of valid initial memory values that can be set
* @param [in] value  16-bit unsigned, initial value set
* @param [in] num  Set the number of memory space initial values
* @param [out] copyFd  Asynchronously copy Fd
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_INVALID_HANDLE : Internal error, copy failed
*/
DLLEXPORT DV_OFF_ONLINE DVresult halMemCpyAsync(DVdeviceptr dst, size_t destMax, DVdeviceptr src,
    size_t byteCount, uint64_t *copyFd) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Copy the data in the source buffer to the destination buffer asynchronously wait finish
* @attention
* 1. The destination buffer must have enough space to store the contents of the source buffer to be copied.
* 2. (offline) (virtual machine logical grouping) not support
* 3. The copyFd will be free after wait finish, so the same copyFd can only be wait finish once.
* @param [in] copyFd  get from halMemCpyAsync, Asynchronously copy Fd
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_INVALID_HANDLE : Internal error, copy failed
*/
DLLEXPORT DV_OFF_ONLINE DVresult halMemCpyAsyncWaitFinish(uint64_t copyFd) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Copy the 2D data in the source buffer to the destination buffer
* @attention The destination buffer must have enough space to store the contents of the source buffer to be copied.
* @param [in] *pCopy  see struct MEMCPY2D
* @param [out] *pCopy  see struct MEMCPY2D
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_XXX  : copy failed
*/
DLLEXPORT DV_ONLINE drvError_t halMemcpy2D(struct MEMCPY2D *pCopy) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Copy the addr array data in the source buffer to the destination buffer synchronously
* @attention
* 1. The destination buffer must have enough space to store the contents of the source buffer to be copied.
* 2. Only support d2h/h2d copy.
* 3. Array max size is 4096.
* @param [in] dst destination address array
              src source address array
              size copy size array
              count len of array
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_NOT_SUPPORT : (virtual machine of 51/80) || (count > DEVMM_MEMCPY_BATCH_MAX_COUNT)
* || (va from ipc_open/shmem import)
* @return DRV_ERROR_XXX : copy failed
*/
DLLEXPORT DV_ONLINE DVresult halMemcpyBatch(uint64_t dst[], uint64_t src[], size_t size[], size_t count);

/**
 * @halSdmaCopy
 * @brief Use sdma device to accelerate memcpy
 * @attention This function is suitable for large size of memcpy. It fallback to normal
 * memcpy_s if the sdma version of memcpy failed. This copy interface can not be used
 * in p2p scenario.
 * @param [in] dst: destination address
 * @param [in] dst_size: destination memory region size
 * @param [in] src: source address
 * @param [in] len: size of the buffer to be copy
 * @return zero on success otherwise -errno
 */
DLLEXPORT DV_OFFLINE drvError_t halSdmaCopy(
    DVdeviceptr dst, size_t dst_size, DVdeviceptr src, size_t len) ASCEND_HAL_WEAK;

/**
 * @halSdmaBatchCopy
 * @brief Use sdma device to accelerate memcpy
 * @attention This is used for large size of memcpy, and it is the batch one of
 * halSdmaCopy. This copy interface can not be used in p2p scenario.
 * @param [in] dst[]: destination address array
 * @param [in] src[]: source address arrary
 * @param [in] size[]: size arrary for copy
 * @param [in] count: the length of size[], dst[] and src[]
 * @return zero on success otherwise -errno
 */
DLLEXPORT DV_OFFLINE drvError_t halSdmaBatchCopy(
    void *dst[], void *src[], size_t size[], int count) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Converts the address to the physical address for DMA copy, including H2D, D2H, and D2D asynchronous
* copy interfaces.
* @attention
* 1. The memory management module does not verify the length of ByteCount. You need to ensure that the length is valid
* 2. D2D convert don't support same device.
* @param [in] pSrc Source address (virtual address)
* @param [in] pDst Destination address (virtual address)
* @param [in] len length
* @param [in] dmaAddr->offsetAddr.devid
* @param [out] dmaAddr see struct DMA_ADDR.
* 1. Flag= 0: non-chain, SRC and DST are physical addresses, can directly conduct DMA copy operation
* 2. Flag= 1: chain, SRC is the address of dma chain list, can directly conduct dma copy operation;
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : convert fail
*/
DLLEXPORT DV_ONLINE DVresult drvMemConvertAddr(DVdeviceptr pSrc, DVdeviceptr pDst, UINT32 len,
    struct DMA_ADDR *dmaAddr);

/**
* @ingroup driver
* @brief Releases the physical address information of the DMA copy
* @attention Available online, not offline. This interface is used with drvMemConvertAddr.
* @param [in] ptr : Information to be released
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : destroy fail
*/
DLLEXPORT DV_ONLINE DVresult drvMemDestroyAddr(struct DMA_ADDR *ptr);

/**
* @ingroup driver
* @brief Async releases the physical address batch information of the DMA copy
* @attention Available online, not offline. This interface is used with drvMemConvertAddr.
* @param [in] ptr[] : Address array to be released
* @param [in] num : num of ptr
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : destroy fail
*/
DLLEXPORT DV_ONLINE DVresult halMemDestroyAddrBatch(struct DMA_ADDR *ptr[], uint32_t num);

/**
* @ingroup driver
* @brief Sumbit DMA the physical address information of the DMA copy
* @attention Available online, not offline. This interface is used with drvMemConvertAddr.
* @param [in] ptr : Information to be DMA copy
* @param [in] Flag: Sumbit DMA copy use synchronize or asynchronous mode, use enum MEMCPY_SUMBIT_TYPE
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT DV_ONLINE DVresult halMemcpySumbit(struct DMA_ADDR *dmaAddr, int flag);
/**
* @ingroup driver
* @brief Wait the physical address information of the DMA copy asynchronously finish
* @attention Available online, not offline. This interface is used with halMemcpySumbit.
* @param [in] ptr : Information to be wait dma finish
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : DMA copy fail
*/
DLLEXPORT DV_ONLINE DVresult halMemcpyWait(struct DMA_ADDR *dmaAddr);

/**
* @ingroup driver
* @brief Prefetch data to the memory of the specified device (uniformly shared virtual address)
* @attention Available online, not offline.
* 1. First apply for svm memory, then fill the data, and then prefetch to the target device side.
* 2. The output buffer scenario uses advice to allocate physical memory to the device side.
* 3. If the host does not create a page table, this interface fails.
* 4. The memory management module is not responsible for the length check of ByteCount,
* 5. users need to ensure that the length and devPtr is legal.
* 6. devPtr must be aligned by page size.
* @param [in] devPtr Memory to prefetch
* @param [in] len Prefetch size
* @param [in] device Destination device for prefetching data
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : prefetch fail
*/
DLLEXPORT DV_ONLINE DVresult drvMemPrefetchToDevice(DVdeviceptr devPtr, size_t len, DVdevice device);
/**
* @ingroup driver
* @brief Create a share corresponding to vptr based on name
* @attention Available online, not offline.
* vptr must be device memory, and must be directly allocated for calling the memory management interface, without offset
* The length of the name array and name_len must be greater than 64
* @param [in] vptr Virtual memory to be shared
* @param [in] byte_count User-defined length to be shared
* @param [in] name_len The maximum length of the name array
* @param [out] name  Name used for sharing between processes
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : create mem handle fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemCreateHandle(DVdeviceptr vptr, size_t byte_count, char *name, uint32_t name_len);
/**
* @ingroup driver
* @brief Destroy shared memory created by halShmemCreateHandle
* @attention Available online, not offline.
* @param [in] name Name used for sharing between processes
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : destroy mem handle fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemDestroyHandle(const char *name);
/**
* @ingroup driver
* @brief Configure the whitelist of nodes with ipc mem shared memory
* @attention Available online, not offline. mutually exclusive with halShmemSetAttribute interface.
* @param [in] name Name used for sharing between processes
* @param [in] pid host pid whitelist array
* @param [in] num number of pid arrays
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemSetPidHandle(const char *name, int pid[], int num);

/**
* @ingroup driver
* @brief Configure the whitelist of nodes with ipc mem shared memory with sdid
* @attention Available online, not offline. mutually exclusive with halShmemSetAttribute interface.
* @param [in] name Name used for sharing between processes
* @param [in] pid host pid whitelist array
* @param [in] sdid which sdid that the white list pids belong to
* @param [in] num number of pid arrays
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemSetPodPid(const char *name, uint32_t sdid, int pid[], int num);

/**
* @ingroup driver
* @brief Open the shared memory corresponding to name, vptr returns the virtual address that can access shared memory
* @attention
* 1、Available online, not offline.
* 2、Ipc not support access double pgtable offset addr.
* @param [in] name Name used for sharing between processes
* @param [out] vptr Virtual address with access to shared memory
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : open fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemOpenHandle(const char *name, DVdeviceptr *vptr);
/**
* @ingroup driver
* @brief Open the shared memory corresponding to name, vptr returns the virtual address that can access shared memory
* @attention Available online, not offline.
* @param [in] name Name used for sharing between processes
* @param [in] devId logic devid
* @param [out] vptr Virtual address with access to shared memory
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : open fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemOpenHandleByDevId(DVdevice devId, const char *name, DVdeviceptr *vptr);
/**
* @ingroup driver
* @brief Close the shared memory corresponding to name
* @attention Available online, not offline.
* @param [in] vptr The virtual address that halShmemOpenHandle can access to shared memory
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : close fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemCloseHandle(DVdeviceptr vptr);

/**
* @ingroup driver
* @brief Set the attribute of shared memory by name
* @attention Available online, not offline.
   Type is SHMEM_ATTR_TYPE_NO_WLIST_IN_SERVER:
   1.If the set pid related API has been called, it returns DRV_ERROR_OPER_NOT_PERMITTED;
   2.If it is not called by the shmem create process, it returns DRV_ERROR_OPER_NOT_PERMITTED;
   3.Currently attr is not supported SHMEM_WLIST_ENABLE.
* @param [in] name Name used for sharing between processes, get by halShmemCreateHandle
* @param [in] type Type of shared memory attribute settings
* @param [in] attr Attr corresponding to type to set shared memory attribute
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemSetAttribute(const char *name, uint32_t type, uint64_t attr);

/**
* @ingroup driver
* @brief get the attribute of shared memory by name
* @attention Available online, not offline.
* @param [in] name Name used for sharing between processes
* @param [in] type Type of shared memory attribute settings
* @param [out] attr Related attribute information about name
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemGetAttribute(const char *name, enum ShmemAttrType type, uint64_t *attr);

/**
* @ingroup driver
* @brief get the info of shared memory by name
* @attention Available online, not offline.
* @param [in] name Name used for sharing between processes
* @param [out] info Related information about name
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT DV_ONLINE DVresult halShmemInfoGet(const char *name, struct ShmemGetInfo *info);

/**
* @ingroup driver
* @brief Get the properties of the virtual memory, if it is device memory, get the deviceID at the same time
* @attention
* 1. Only by calling interface halSupportFeature(devId, FEATURE_SVM_GET_USER_MALLOC_ATTR) first
* can support getting the user malloc addr attribute.
* 2. To improve query performance, this interface only query vptr property information but not judge alloced or not,
* due to internal address management, it is possible to get the attributes of vptr that have not been alloced or have been released.
* 3. The devId is only valid after vptr has been mapped on the device side.
* 4. Vptr from halMemAddressReserve function must be mapped before attribute query
* @param [in] vptr  Virtual address to be queried
* @param [out] attr  vptr property information corresponding to the page
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : get fail
*/
DLLEXPORT DV_ONLINE DVresult drvMemGetAttribute(DVdeviceptr vptr, struct DVattribute *attr);
/**
* @ingroup driver
* @brief Device mounts memory daemon background thread
* @attention Called by matrix after device os starts
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : setup fail
*/
DLLEXPORT DV_ONLINE int devmm_daemon_setup(void);
/**
* @ingroup driver
* @brief Open the memory management module interface and initialize related information
* @attention null
* @param [in] devid  Device id
* @param [in] flag   Open flag(SVM_AGENT_DEVICE/SVM_AGENT_HOST)
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : open fail
*/
DLLEXPORT DV_ONLINE drvError_t halMemAgentOpen(uint32_t devid, uint32_t flag);
/**
* @ingroup driver
* @brief Close the memory management module interface
* @attention Used with halMemAgentOpen.
* @param [in] devid  Device id
* @param [in] flag   Close flag(SVM_AGENT_DEVICE/SVM_AGENT_HOST)
* @return DRV_ERROR_NONE  success
* @return DV_ERROR_XXX  close fail
*/
DLLEXPORT DV_ONLINE drvError_t halMemAgentClose(uint32_t devid, uint32_t flag);
/**
* @ingroup driver
* @brief Open the memory management module interface and initialize related information
* @attention null
* @param [in] devid  Device id
* @param [in] devfd  Device file handle
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : open fail
*/
DLLEXPORT DV_ONLINE int drvMemDeviceOpen(uint32_t devid, int devfd);
/**
* @ingroup driver
* @brief Close the memory management module interface
* @attention Used with drvMemDeviceOpen.
* @param [in] devid  Device id
* @return DRV_ERROR_NONE  success
* @return DV_ERROR_XXX  close fail
*/
DLLEXPORT DV_ONLINE int drvMemDeviceClose(uint32_t devid);
/**
* @ingroup driver
* @brief Applying for the Memory with the Execute Permission
* @attention Currently, this interface can be used only for the memory applied by the ion
* @param [in] bytesize Requested byte size
* @param [out] pp  Level-2 pointer that stores the address of the allocated memory pointer
* @return DRV_ERROR_NONE  success
* @return DV_ERROR_XXX  alloc fail
*/
DLLEXPORT DV_ONLINE DVresult drvMemAllocProgram(void **pp, size_t bytesize);

/**
* @ingroup driver
* @brief This command is used to register src share memory.
* @attention
* 1. To munmap the registered memory, you should unregister it before.
* 2. HOST_MEM_MAP_DMA don't support read-only memory.
* 3. To improve hccs vm scene's performance, halMemAlloc already register host_pin mem to dma, can't register again.
* 4. For user malloc memory, should unregister first then free memory, otherwise it'll lead to unpredictable behavior.
* 5. Due to lower versions of Linux, pin pages(os malloc) may still be swapped, so register os malloc va to dma is not support in Linux versions below 5.19.
* 6. HOST_SVM_MAP_DEV don't support in virt machine.
* @param [in] srcPtr Requested the src share memory pointer, srcPtr must be page aligned.
* @param [in] size Requested byte size.
* @param [in] flag  Requested memory parameter, the flag is made by map type and proc type.
* @param [in] devid  Requested input device id when map_type is't DEV_SVM_MAP_HOST.
* @param [out] dstPtr Level-2 pointer that stores the address of the allocated dst memory pointer.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : register fail
*/
DLLEXPORT drvError_t halHostRegister(void *srcPtr, UINT64 size, UINT32 flag, UINT32 devid, void **dstPtr);

/**
* @ingroup driver
* @brief This command is used to unregister src share memory.
* @attention null
* @param [in] srcPtr Requested the src share memory pointer.
* @param [in] devid  Requested input device id when srcPtr isn't in svm range.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : unregister fail
*/
DLLEXPORT drvError_t halHostUnregister(void *srcPtr, UINT32 devid);

/**
* @ingroup driver
* @brief This command is used to unregister src share memory.
* @attention null
* @param [in] srcPtr Requested the src share memory pointer.
* @param [in] devid  Requested input device id when srcPtr isn't in svm range.
* @param [in] flag   Made by map_type and proc_type. Only DEV_MEM_MAP_HOST request map_type input, others judge type by va_attr.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : unregister fail
*/
DLLEXPORT drvError_t halHostUnregisterEx(void *srcPtr, UINT32 devid, UINT32 flag);

/**
* @ingroup driver
* @brief This command is used to get device pointer that had been registered.
* @attention
* 1. Need to call halSupportFeature(devid, FEATURE_SVM_MEM_REGISTER_QUERY_AND_GET_ATTR) firstly, then call halHostRegister.
*    Only in this way can get device pointer.
* @param [in] srcPtr Requested the src share memory pointer, srcPtr must be page aligned.
* @param [in] devid  Requested input device id that had been registered.
* @param [out] dstPtr Level-2 pointer that stores the address of the device pointer.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : get device pointer fail
*/
DLLEXPORT drvError_t halMemHostGetDevPointer(void *srcPtr, uint32_t devid, void **dstPtr);

/**
 * @ingroup driver
 * @brief This command is used to query accelerators to access host memory
 * @attention null
 * @param [in] devid:  device id.
 * @param [in] acc_module_type: the accelerators type.  The value only supports enum drvAccModuleType.
 * @param [out] mem_map_cap: The pointer that stores the accelerators' capability to access host memory.
     The value only includes enum drvMemMapCapability.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : query fail
 */
DLLEXPORT drvError_t halHostRegisterCapabilities(uint32_t devid, uint32_t acc_module_type, uint32_t *mem_map_cap);

/**
* @ingroup driver
* @brief This command is used to alloc memory.
* @attention
* 1. When the application phy_mem_type is HBM and no HBM is available on the device side, this command allocates
*    memory from the DDR.
* 2. When advise_4G is true, user needs to set adivse_continuty true at the same time. Besides, alloc continuty
*    physics memory may fail if the system is fragmented seriously. User needs to handle the failure scenario.
     advise_4G only support Ascend310B,Ascend310P.
* 3. When the virt_mem_type is DVPP, ignore the advise_4G and adivse_continuty flags, and will return DVPP memory
*    directly.
* 4. Source address marked with MEM_HOST_RW_DEV_RO is only support drvMemcpy.
* 5. Source address marked with MEM_READONLY is not support drvMemcpy/drvMemConvertAddr.
* 6. The maximum virtual memory(no page) applied for at a time is 128GB.
* @param [in] size Requested byte size.
* @param [in] flag  Requested memory parameter.
* @param [out] **pp  Level-2 pointer that stores the address of the allocated memory pointer.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halMemAlloc(void **pp, unsigned long long size, unsigned long long flag);

/**
* @ingroup driver
* @brief This command is used to release memory resources.
* @attention The memory may not be reclaimed because of the memory caching mechanism.
* @param [in] pp Pointer to the memory space to be released.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halMemFree(void *pp);

/**
* @ingroup driver
* @brief This command is used to advise memory.
* @attention When advise continuty virtual memory to different devices, only support the devices in same os.
* @param [in] ptr Requested the svm memory pointer, ptr must be page aligned.
* @param [in] count Requested byte size.
* @param [in] type Requested advise type parameter.
* @param [in] device Requested input device id.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halMemAdvise(DVdeviceptr ptr, size_t count, unsigned int type, DVdevice device);

/**
* @ingroup driver
* @brief This command is used to check device process status.
* @attention Only support ONLINE scene. Don't support virtual machine and host agent.
* @param [in] device Requested input device id.
* @param [in] processType Requested device process type parameter.
* @param [in] status Used to check the status of device process.
* @param [out] isMatched Used to indicate whether the deivce process status is the same as the given status.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT DV_ONLINE drvError_t halCheckProcessStatus(
    DVdevice device, processType_t processType, processStatus_t status, bool *isMatched);

/**
* @ingroup driver
* @brief This command is used to check device process status.
* @attention Only support ONLINE scene. Don't support virtual machine and host agent.
* @param [in] device Requested input device id.
* @param [in] processType Requested device process type parameter.
* @param [in] status Used to check the status of device process.
* @param [out] out The output corresponding to the process status.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT DV_ONLINE drvError_t halCheckProcessStatusEx(
    DVdevice device, processType_t processType, processStatus_t status, struct drv_process_status_output *out);

/**
* @ingroup driver
* @brief This command is used to reserve a virtual address range.
* @attention
* 1. Only support ONLINE scene: in scenarios where hccl is not used for a single process,
*    cross-chip access is not supported, only can use aclrtMemcpyAsync to copy data across chips.
* 2. Spcified addrress reserve must comply with the following rules:
     1) If the size is greater than 512 MB, the address must be 1 GB aligned.
     2) If the size is less than or equal to 512 MB, the address must be aligned by power(2, ceil(log2(size))).
* 3. The halMemFree interface has a cache mechanism. The address is actually cached after be released.
     As a result, the specified cached address fails to be applied for.
* 4. The maximum virtual memory(no page) applied for at a time is 128GB.
* 5. Flag=MEM_RSV_TYPE_REMOTE_MAP or MEM_RSV_TYPE_DEVICE_SHARE only support specified address reserve.
* @param [in] size Size of the reserved virtual address range requested.
* @param [in] alignment Currently unused, must be zero.
* @param [in] addr addr==NULL, normal address reserve.
 *                 addr!=NULL, spcified addrress reserve, should in specified address alloc range, and be 2M aligned.
* @param [in] flag Flag of the address to create, current only support pg_type.
* @param [out] pp Resulting pointer to start of virtual address range allocated.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemAddressReserve(void **ptr, size_t size,
    size_t alignment, void *addr, uint64_t flag);

/**
* @ingroup driver
* @brief This command is used to free a virtual address range reserved by halMemAddressReserve.
* @attention Only support ONLINE scene.
* @param [in] pp Starting address of the virtual address range to free.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemAddressFree(void *ptr);

/**
* @ingroup driver
* @brief This command is used to alloc physical memory.
* @attention Only support ONLINE scene.
* @param [in] size Size of the allocation requested, must be aligned by 2M.
* @param [in] prop Properties of the allocation to create.
* @param [in] flag Currently unused, must be zero.
* @param [out] handle Value of handle returned, all operations on this allocation are to be performed using this handle.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemCreate(drv_mem_handle_t **handle, size_t size,
    const struct drv_mem_prop *prop, uint64_t flag);

/**
* @ingroup driver
* @brief This command is used to free physical memory.
* @attention Only support ONLINE scene.
* @param [in] handle Value of handle which was returned previously by halMemCreate.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemRelease (drv_mem_handle_t *handle);

/**
 * @ingroup driver
 * @brief This command is used to given an address ptr, returns the allocation handle.
 * @attention Only support ONLINE scene. must call halMemRelease to destroy the handle.
 *            If the address requested is not mapped, the function will fail.
 * @param [in] ptr the address, can be any address in a range previously mapped by halMemMap,
 *             and not necessarily the start address.
 * @param [out] handle value of handle returned.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : fail
 */
DLLEXPORT drvError_t halMemRetainAllocationHandle(drv_mem_handle_t **handle, void *ptr);

/**
 * @ingroup driver
* @brief This command is used to map an allocation handle to a reserved virtual address range.
* @attention
* 1. Only support ONLINE scene.
* 2. If page type is MEM_GIANT_PAGE_TYPE, size and ptr must be aligned by 1G.
* @param [in] ptr Address where memory will be mapped, must be aligned by 2M.
* @param [in] size Size of the memory mapping, must be aligned by 2M.
* @param [in] offset Currently unused, must be zero.
* @param [in] handle Value of handle which was returned previously by halMemCreate.
* @param [in] flag Currently unused, must be zero.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemMap(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag);

/**
* @ingroup driver
* @brief This command is used to unmap the backing memory of a given address range.
* @attention Only support ONLINE scene.
* @param [in] ptr Starting address for the virtual address range to unmap.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemUnmap(void *ptr);

/**
 * @ingroup driver
 * @brief This command is used to set access to a reserved virtual address range for the other device.
 * @attention
 * 1. Only support ONLINE scene.
 * 2. Support va->pa:
 *    D2H,
 *    D2D(sigle device, diffrent device with same host, diffrent device with diffrent host),
 *    H2H(same host, diffrent host(support latter))
 * 3. halMemSetAccess: ptr and size must be same with halMemMap, halMemGetAccess: ptr and size is in range of set
 * 4. after halMemMap, if handle has owner(witch location pa is created or use witch device pa handle is imported)
 *    the owner location has readwrite prop automatic, not need to set again
 * 5. repeat set ptr to same location with diffrent type will return DRV_ERROR_BUSY
 * @param [in] ptr: mapped address.
 * @param [in] size: mapped size.
 * @param [in] desc: va location and access type, when location is device, id is devid;
                     when location is host, id must be 0.
 * @param [in] count: desc num.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : fail
 */
DLLEXPORT drvError_t halMemSetAccess(void *ptr, size_t size, struct drv_mem_access_desc *desc, size_t count);

/**
 * @ingroup driver
 * @brief This command is used to get access to a reserved virtual address range for the other device.
 * @attention if location not halMemSetAccess, will return success with flags as MEM_ACCESS_TYPE_NONE
 * @param [in] ptr: mapped address.
 * @param [in] location: va location, when location is device, id is devid; when location is host, id must be 0.
 * @param [out] flags: low 2 bit is access type.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : fail
 */
DLLEXPORT drvError_t halMemGetAccess(void *ptr, struct drv_mem_location *location, uint64_t *flags);

/**
 * @ingroup driver
* @brief This command is used to export an allocation to a shareable handle.
* @attention Only support ONLINE scene. Not support compute group.
* @param [in] handle Handle for the memory allocation.
* @param [in] handleType Currently unused, must be MEM_HANDLE_TYPE_NONE.
* @param [in] flags Currently unused, must be zero.
* @param [out] shareableHandle Export a shareable handle.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemExportToShareableHandle(drv_mem_handle_t *handle, drv_mem_handle_type handleType,
    uint64_t flags, uint64_t *shareableHandle);

/**
 * @ingroup driver
 * @brief This command is used to export an allocation to a shareable handle.
 * @attention Only support ONLINE scene. Not support compute group.
 * @param [in] handle: Handle for the memory allocation.
 * @param [in] handle_type: MEM_HANDLE_TYPE_NONE or MEM_HANDLE_TYPE_FABRIC.
 * @param [in] flags: Currently unused, must be zero.
 * @param [out] share_handle: Export a shareable handle.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : fail
 */
DLLEXPORT drvError_t halMemExportToShareableHandleV2(
    drv_mem_handle_t *handle, drv_mem_handle_type handle_type, uint64_t flags, struct MemShareHandle *share_handle);

/**
 * @ingroup driver
* @brief This command is used to import an allocation from a shareable handle.
* @attention Only support ONLINE scene. Not support compute group.
* @param [in] shareableHandle Import a shareable handle.
* @param [in] devid Device id.
* @param [out] handle Value of handle returned, all operations on this allocation are to be performed using this handle.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemImportFromShareableHandle(uint64_t shareableHandle,
    uint32_t devid, drv_mem_handle_t **handle);

/**
 * @ingroup driver
 * @brief This command is used to import an allocation from a shareable handle.
 * @attention Only support ONLINE scene. Not support compute group.
 * @param [in] handle_type: MEM_HANDLE_TYPE_NONE or MEM_HANDLE_TYPE_FABRIC.
 * @param [in] share_handle: Import a shareable handle.
 * @param [in] devid: Device id. Since server-to-server communication requires the capabilities of the devices,
              the device ID is retained in the halMemImportFromShareableHandleEx interface.
 * @param [out] handle: Value of handle returned, all operations on this allocation are to be performed using this handle
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : fail
 */
DLLEXPORT drvError_t halMemImportFromShareableHandleV2(
    drv_mem_handle_type handle_type, struct MemShareHandle *share_handle, uint32_t devid, drv_mem_handle_t **handle);

/**
 * @ingroup driver
 * @brief This command is used to trans struct MemShareHandle to u64 shareable_handle(devid + share id),
         to set/get attribute in local.
 * @attention Only support ONLINE scene. Not support compute group.
 * @param [in] handle_type: MEM_HANDLE_TYPE_NONE or MEM_HANDLE_TYPE_FABRIC.
 * @param [in] share_handle: export by halMemExportToShareableHandleEx.
 * @param [out] server_id: witch server share_handle from.
 * @param [out] shareable_handle: Export a shareable handle.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : fail
 */
DLLEXPORT drvError_t halMemTransShareableHandle(drv_mem_handle_type handle_type, struct MemShareHandle *share_handle,
    uint32_t *server_id, uint64_t *shareable_handle);

/**
 * @ingroup driver
* @brief This command is used to configure the process whitelist which can use shareable handle.
* @attention Only support ONLINE scene.
  1.Not support compute group.
  2.Mutually exclusive with halMemShareHandleSetAttribute
* @param [in] shareableHandle A shareable handle.
* @param [in] pid Host pid whitelist array.
* @param [in] pid_num Number of pid arrays.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemSetPidToShareableHandle(uint64_t shareableHandle,
    int pid[], uint32_t pid_num);

/**
* @ingroup driver
* @brief Set the attribute of shared memory by shareableHandle
* @attention Available online, not offline. Not support compute group.
   Type is SHR_HANDLE_ATTR_NO_WLIST_IN_SERVER:
   1.If the set pid related API has been called, it returns DRV_ERROR_OPER_NOT_PERMITTED;
   2.If it is not called by the mem export process, it returns DRV_ERROR_OPER_NOT_PERMITTED;
   3.Currently attr.enableFlag is not supported SHR_HANDLE_WLIST_ENABLE.
* @param [in] shareableHandle shareableHandle used for sharing between processes, get by halMemExportToShareableHandled
* @param [in] type Type of shared memory attribute settings
* @param [in] attr Attr corresponding to type to set shared memory attribute
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halMemShareHandleSetAttribute(uint64_t shareableHandle, enum ShareHandleAttrType type,
    struct ShareHandleAttr attr);
/**
* @ingroup driver
* @brief Get the attribute of shared memory by shareableHandle
* @attention Available online, not offline. Not support compute group.
* @param [in] shareableHandle shareableHandle used for sharing between processes
* @param [in] type Type of shared memory attribute settings
* @param [out] attr Related attribute information about shareableHandle
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halMemShareHandleGetAttribute(uint64_t shareableHandle, enum ShareHandleAttrType type,
    struct ShareHandleAttr *attr);

/**
* @ingroup driver
* @brief get the info of shared memory by shareableHandle
* @attention Available online, not offline. Not support compute group.
* @param [in] shareableHandle shareableHandle used for sharing between processes
* @param [out] info Related information about shareableHandle
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halMemShareHandleInfoGet(uint64_t shareableHandle, struct ShareHandleGetInfo *info);

/**
* @ingroup driver
* @brief This command is used to calculate either the minimal or recommended granularity.
* @attention Only support ONLINE scene.
* @param [in] prop Properties of the allocation.
* @param [in] option Determines which granularity to return.
* @param [out] granularity Returned granularity.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemGetAllocationGranularity(const struct drv_mem_prop *prop,
    drv_mem_granularity_options option, size_t *granularity);

/**
* @ingroup driver
* @brief Return the base address in *pbase and size in *psize of the allocation by halMemAlloc or
   halMemAddressReserve(shoud be mapped by halMemMap) that contains the input pointer ptr. Both
   parameters pbase and psize are optional.if one of them is NULL, it is ignored.
* @attention Only support ONLINE scene.
* @param [in] ptr -Device poniter to query.
* @param [out] pbase -Return base address.
* @param [out] psize -Return size of device memory allocation.
   Note: The returned size is the actual allocated memory after aligning to the page size, which
   is larger than the user's initial requested memory size. For example, if the user wishes to
   allocate 1024 bytes, the actual allocated size aligned to a 4K page size is 4K.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemGetAddressRange(DVdeviceptr ptr, DVdeviceptr *pbase, size_t *psize);

/**
* @ingroup driver
* @brief Get memory allocation properties from handle.
* @param [in] handle - memory handle.
* @param [out] prop - memory allocation properties.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : fail
*/
DLLEXPORT drvError_t halMemGetAllocationPropertiesFromHandle(struct drv_mem_prop *prop, drv_mem_handle_t *handle);

struct MemPhyInfo {
#ifndef __linux
    unsigned long long total;
    unsigned long long free;
    unsigned long long huge_total;
    unsigned long long huge_free;
    unsigned long long giant_total;
    unsigned long long giant_free;
#else
    unsigned long total;        /* normal page total size, only 4K pages */
    unsigned long free;         /* normal page free size */
    unsigned long huge_total;   /* huge page total size, 2M pages + 1G pages */
    unsigned long huge_free;    /* huge page free size */
    unsigned long giant_total;  /* giant page total size, only 1G pages */
    unsigned long giant_free;   /* giant page free size */
#endif
};

struct MemAddrInfo {
    DVdeviceptr** addr;
    unsigned int cnt;
    unsigned int mem_type;
    unsigned int flag;
};

#define MAX_NUMA_NUM_OF_PER_DEV 64
struct MemNumaInfo {
    unsigned int node_cnt;
    int node_id[MAX_NUMA_NUM_OF_PER_DEV];
};


#define SVM_GRP_NAME_LEN    BUFF_GRP_NAME_LEN
struct MemSvmGrpInfo {
    char name[SVM_GRP_NAME_LEN];
};

struct MemUbTokenInfo {
    uint64_t va;                /* Input para */
    uint64_t size;              /* Input para */

    uint32_t token_id;          /* Output para */
    uint32_t token_value;       /* Output para */
};

struct MemInfo {
    union {
        struct MemPhyInfo phy_info;
        struct MemAddrInfo addr_info;
        struct MemNumaInfo numa_info;
        struct MemSvmGrpInfo grp_info;
        struct MemUbTokenInfo ub_token_info;
    };
};

typedef drvError_t hdcError_t;
typedef void *HDC_CLIENT;
typedef void *HDC_SESSION;
typedef void *HDC_SERVER;

/**
 * @ingroup driver
 * @brief get device memory info
 * @attention For offline scenarios, return success.
 * If type == MEM_INFO_TYPE_ADDR_CHECK, to check whether the address is accessible, not support svm/host/host_agent memtype.
 * @param [in] device: device id
 * @param [in] type: command type
 * @param [in/out] info: memory info
 * @return  0 for success, others for fail
 */
DLLEXPORT DVresult halMemGetInfoEx(DVdevice device, unsigned int type, struct MemInfo *info);

/**
* @ingroup driver
* @brief This command is used to get memory information.
* @attention If type == MEM_INFO_TYPE_ADDR_CHECK, to check whether the address is accessible, not support svm/host/host_agent memtype.
* @param [in] device Requested input device id.
* @param [in] type Requested input memory type.
* @param [out] info memory information.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT DVresult halMemGetInfo(DVdevice device, unsigned int type, struct MemInfo *info);

/**
* @ingroup driver
* @brief This command is used to control memory.
* @attention null
* @param [in] type Requested input memory type.
* @param [in] param_value Requested input param value pointer.
* @param [in] param_value_size Requested input param value size.
* @param [out] out_value the pointer of output value.
* @param [out] out_size_ret the pointer of output value size.
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : set fail
*/
DLLEXPORT drvError_t halMemCtl(int type, void *param_value, size_t param_value_size, void *out_value,
    size_t *out_size_ret) ASCEND_HAL_WEAK;

struct mem_module_usage {
    char name[32]; /* module name Length 32 */
    uint64_t cur_mem_size;   /* Current memory size occupied by the module */
    uint64_t mem_peak_size;   /* Peak memory size occupied by the module */
    size_t reserved[8];  /* reserved field 8 */
};
/**
 * @ingroup driver
 * @brief This command is used to query the memory usage of each module.
 * @attention Only memory allocated through halMemAlloc and halMemCreate can be queried
 * @param [in] dev_id:  calls the aclrtGetDeviceCount API to obtain the number of available devices,
 * the value range of the device ID is [0, (number of available devices - 1)].
 * Host pin memory information is queried using devid halGetHostId() (ASAN version all return zero).
 * @param [out] mem_usage: An array used to store the memory size information occupied by each module.
 * It is user-defined, and the number of modules to be passed must be 128 (a macro definition is provided).
 * The specific size of the array is passed through in_num..
 * @param [in] in_num: Actual length of the array.
 * @param [out] out_num: The number of modules that occupy non-zero memory size,
 * consistent with the valid data length of memUsageInfo.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : fail
 */
DLLEXPORT drvError_t halGetMemUsageInfo(uint32_t dev_id, struct mem_module_usage *mem_usage, size_t in_num, size_t *out_num);

typedef void *HDC_EPOLL;

#define HDC_EPOLL_CTL_ADD 0
#define HDC_EPOLL_CTL_DEL 1

#define HDC_EPOLL_CONN_IN (0x1 << 0)
#define HDC_EPOLL_DATA_IN (0x1 << 1)
#define HDC_EPOLL_FAST_DATA_IN (0x1 << 2)
#define HDC_EPOLL_SESSION_CLOSE (0x1 << 3)

struct drvHdcEvent {
    unsigned int events;
    uintptr_t data;
};

#define RUN_ENV_UNKNOW 0
#define RUN_ENV_PHYSICAL 1
#define RUN_ENV_PHYSICAL_CONTAINER 2
#define RUN_ENV_VIRTUAL 3
#define RUN_ENV_VIRTUAL_CONTAINER 4

/**< The HDC interface is dead and blocked by default. Set HDC_FLAG_NOWAIT to be non-blocked */
/**< Set HDC_FLAG_WAIT_TIMEOUT to timeout after blocking for a period of time. HDC_FLAG_WAIT_TIMEOUT */
/**< takes precedence over HDC_FLAG_NOWAIT */
#define HDC_FLAG_NOWAIT (0x1 << 0)        /**< Occupy bit0 */
#define HDC_FLAG_WAIT_TIMEOUT (0x1 << 1)  /**< Occupy bit1 */
#define HDC_FLAG_MAP_VA32BIT (0x1 << 1)   /**< Use low 32bit memory */
#define HDC_FLAG_MAP_HUGE (0x1 << 2)      /**< Using large pages */

/* 通信类型 */
enum halHdcTransType {
    HDC_TRANS_USE_SOCKET = 0,
    HDC_TRANS_USE_PCIE = 1
};

#define HDC_MDC_RC_DEVID (1)    /**< add for as31xm1 */
#define HDC_MDC_EP_DEVID (0)    /**< add for as31xm1 */

enum drvHdcServiceType {
    HDC_SERVICE_TYPE_DMP = 0,
    HDC_SERVICE_TYPE_PROFILING = 1, /**< used by profiling tool */
    HDC_SERVICE_TYPE_IDE1 = 2,
    HDC_SERVICE_TYPE_FILE_TRANS = 3,
    HDC_SERVICE_TYPE_IDE2 = 4,
    HDC_SERVICE_TYPE_LOG = 5,
    HDC_SERVICE_TYPE_RDMA = 6,
    HDC_SERVICE_TYPE_BBOX = 7,
    HDC_SERVICE_TYPE_FRAMEWORK = 8,
    HDC_SERVICE_TYPE_TSD = 9,
    HDC_SERVICE_TYPE_TDT = 10,
    HDC_SERVICE_TYPE_PROF = 11, /* used by drv prof */
    HDC_SERVICE_TYPE_IDE_FILE_TRANS = 12,
    HDC_SERVICE_TYPE_DUMP = 13,
    HDC_SERVICE_TYPE_USER3 = 14, /* used by user */
    HDC_SERVICE_TYPE_DVPP = 15, /* support multiple processes */
    HDC_SERVICE_TYPE_QUEUE = 16, /* support multiple processes */
    HDC_SERVICE_TYPE_UPGRADE = 17,
    HDC_SERVICE_TYPE_RDMA_V2 = 18, /* support multiple processes */
    HDC_SERVICE_TYPE_TEST = 19, /* support multiple processes */
    HDC_SERVICE_TYPE_KMS = 20,
    HDC_SERVICE_TYPE_USER_START = 64,
    HDC_SERVICE_TYPE_USER_END = 127,
    HDC_SERVICE_TYPE_MAX
};

enum drvHdcSessionAttr {
    HDC_SESSION_ATTR_DEV_ID = 0,
    HDC_SESSION_ATTR_UID = 1,
    HDC_SESSION_ATTR_RUN_ENV = 2,
    HDC_SESSION_ATTR_VFID = 3,
    HDC_SESSION_ATTR_LOCAL_CREATE_PID = 4,
    HDC_SESSION_ATTR_PEER_CREATE_PID = 5,
    HDC_SESSION_ATTR_STATUS = 6,
    HDC_SESSION_ATTR_DFX = 7,
    HDC_SESSION_ATTR_MAX
};

enum drvHdcServerAttr {
    HDC_SERVER_ATTR_DEV_ID = 0,
    HDC_SERVER_ATTR_MAX
};

enum drvHdcChanType {
    HDC_CHAN_TYPE_SOCKET = 0,
    HDC_CHAN_TYPE_PCIE,
    HDC_CHAN_TYPE_MAX
};

enum drvHdcMemType {
    HDC_MEM_TYPE_TX_DATA = 0,
    HDC_MEM_TYPE_TX_CTRL = 1,
    HDC_MEM_TYPE_RX_DATA = 2,
    HDC_MEM_TYPE_RX_CTRL = 3,
    HDC_MEM_TYPE_DVPP = 4,
    HDC_MEM_TYPE_ANY = 5,
    HDC_MEM_TYPE_MAX
};

#define HDC_SESSION_MEM_MAX_NUM 100

struct drvHdcFastSendMsg {
    unsigned long long srcDataAddr;
    unsigned long long dstDataAddr;
    unsigned long long srcCtrlAddr;
    unsigned long long dstCtrlAddr;
    unsigned int dataLen;
    unsigned int ctrlLen;
};

struct drvHdcFastRecvMsg {
    unsigned long long dataAddr;
    unsigned long long ctrlAddr;
    unsigned int dataLen;
    unsigned int ctrlLen;
};

struct drvHdcFastSendFinishMsg {
    unsigned long long dataAddr;
    unsigned long long ctrlAddr;
    unsigned int dataLen;
    unsigned int ctrlLen;

    unsigned int result; /* 0-send success, other- send fail */
    unsigned int rsv1;
    unsigned int rsv2;
};

struct drvHdcWaitMsgInput {
    int time_out;
    unsigned int result_type;
    unsigned int rsv1;
    unsigned int rsv2;
};

struct drvHdcCapacity {
    enum drvHdcChanType chanType;
    unsigned int maxSegment;
};

struct drvHdcMsgBuf {
    char *pBuf;
    int len;
};

struct drvHdcMsg {
    int count;
    struct drvHdcMsgBuf bufList[0];
};

struct drvHdcRecvConfig {
    UINT64 wait_flag;
    UINT32 timeout;
    int group_flag;
    int reserved_params1;
    int reserved_params2;
    int reserved_params3;
    int reserved_params4;
};

struct drvHdcProgInfo {
    char name[256];
    int progress;
    long long int send_bytes;
    long long int rate;
    int remain_time;
};

#define HDC_SESSION_INFO_RES_CNT 8

struct drvHdcSessionInfo {
    unsigned int devid;
    unsigned int fid;
    unsigned int res[HDC_SESSION_INFO_RES_CNT];
};

/**
* @ingroup driver
* @brief Before the HDC sends messages, you need to know the size of the sent packet and
* the channel type through this API.
* @attention null
* @param [out] capacity : get the packet size and channel type currently supported by HDC
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t drvHdcGetCapacity(struct drvHdcCapacity *capacity);
/**
* @ingroup driver
* @brief Create an HDC client and initialize it based on the maximum number of sessions and service type.
* @attention null
* @param [in]  maxSessionNum : The maximum number of sessions currently required by Client
* @param [in]  serviceType : select service type
* @param [in]  flag : Reserved parameters, [bit0 - bit7] session connect timeout, other fixed to 0
* @param [out] HDC_CLIENT *client : Created a good HDC Client pointer
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcClientCreate(HDC_CLIENT *client, int maxSessionNum, int serviceType, int flag);
/**
* @ingroup driver
* @brief Release HDC Client
* @attention null
* @param [in]  client : HDC Client to be released
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcClientDestroy(HDC_CLIENT client);
/**
* @ingroup driver
* @brief Wake up client connect wait
* @attention null
* @param [in]  client : HDC Client handle
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcClientWakeUp(HDC_CLIENT client);
/**
* @ingroup driver
* @brief Create HDC Session for Host and Device communication
* @attention null
* @param [in]  peer_node : The node number of the node where the Device is located. Currently only 1 node is supported.
* Remote nodes are not supported. You need to pass a fixed value of 0
* @param [in]  peer_devid : Device's uniform ID in the host (number in each node)
* @param [in]  client : HDC Client handle corresponding to the newly created Session
* @param [out] session : Created session
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcSessionConnect(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session);
/**
* @ingroup driver
* @brief Create HDC Session for Host and Device communication
* @attention null
* @param [in]  peer_node : The node number of the node where the Device is located. Currently only 1 node is supported.
* Remote nodes are not supported. You need to pass a fixed value of 0
* @param [in]  peer_devid : Device's uniform ID in the host (number in each node)
* @param [in]  peer_pid : server's pid which you want to connect
* @param [in]  client : HDC Client handle corresponding to the newly created Session
* @param [out] session : Created session
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcSessionConnectEx(int peer_node, int peer_devid, int peer_pid, HDC_CLIENT client,
    HDC_SESSION *pSession);

/**
* @ingroup driver
* @brief Create and initialize HDC Server
* @attention null
* @param [in]  devid : only support [0, 64)
* @param [in]  serviceType : select server type
* @param [out] server : Created HDC server
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcServerCreate(int devid, int serviceType, HDC_SERVER *pServer);
/**
* @ingroup driver
* @brief Release HDC Server
* @attention null
* @param [in]  server : HDC server to be released
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcServerDestroy(HDC_SERVER server);
/**
* @ingroup driver
* @brief Wake up accept wait
* @attention null
* @param [in]  server : HDC server handle
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcServerWakeUp(HDC_SERVER server);
/**
* @ingroup driver
* @brief Open HDC Session for communication between Host and Device
* @attention null
* @param [in]  server     : HDC server to which the newly created session belongs
* @param [out] session  : Created session
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcSessionAccept(HDC_SERVER server, HDC_SESSION *session);
/**
* @ingroup driver
* @brief Close HDC Session for communication between Host and Device
* @attention null
* @param [in]  session : Specify in which session to receive data
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcSessionClose(HDC_SESSION session);
/**
* @ingroup driver
* @brief Apply for MSG descriptor for sending and receiving
* @attention The user applies for a message descriptor before sending and receiving data, and then releases the
* message descriptor after using it.
* @param [in]  session : Specify in which session to receive data
* @param [in]  count : Number of buffers in the message descriptor. Currently only one is supported
* @param [out] ppMsg : Message descriptor pointer, used to store the send and receive buffer
* address and length
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcAllocMsg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
/**
* @ingroup driver
* @brief Release MSG descriptor for sending and receiving
* @attention The user applies for a message descriptor before sending and receiving data, and then releases
* the message descriptor after using it.
* @param [in]  pMsg   :  Pointer to message descriptor to be released
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcFreeMsg(struct drvHdcMsg *msg);
/**
* @ingroup driver
* @brief Reuse MSG descriptor
* @attention This interface will clear the Buffer pointer in the message descriptor. For offline scenarios, Reuse
* will release the original Buffer. For online scenarios, Reuse will not release the original Buffer (the upper
* layer calls the device memory management interface on the Host to release it).
* @param [in]  pMsg : The pointer of message need to Reuse
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcReuseMsg(struct drvHdcMsg *msg);
/**
* @ingroup driver
* @brief Add the receiving and sending buffer to the MSG descriptor
* @attention User applies for a message descriptor before sending and receiving data, and then releases the
* message descriptor after using it.
* @param [in]  pMsg : The pointer of the message need to be operated
* @param [in]  pBuf : Buffer pointer to be added
* @param [in]  len : The length of the effective data to be added
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcAddMsgBuffer(struct drvHdcMsg *msg, char *pBuf, int len);
/**
* @ingroup driver
* @brief Add MSG descriptor to send buffer
* @attention null
* @param [in]  pMsg : Pointer to the message descriptor to be manipulated
* @param [in]  index              : The first several buffers need to be obtained, but currently only supports one,
* be fixed to 0
* @param [out] ppBuf           : Obtained Buffer pointer
* @param [out] pLen              : Length of valid data that can be obtained from the Buffer
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcGetMsgBuffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen);
/**
* @ingroup driver
* @brief Block and wait before sending data from the peer end and receive the length of the sent packet
* @attention null
* @param [in]  session : session
* @param [in]  msgLen         : Data length
* @param [in]  flag            : Flag, 0 wait always, HDC_FLAG_NOWAIT non-blocking, HDC_FLAG_WAIT_TIMEOUT
* blocking timeout
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcRecvPeek(HDC_SESSION session, int *msgLen, int flag);
/**
* @ingroup driver
* @brief Receive data over normal channel, Save the received data to the upper layer buffer pBuf
* @attention null
* @param [in]  session : session
* @param [in]  pBuf     : Receive data buf
* @param [in]  bufLen     : Received data buf length
* @param [out] msgLen    : Received data buf length
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcRecvBuf(HDC_SESSION session, char *pBuf, int bufLen, int *msgLen);
/**
* @ingroup driver
* @brief Set session and process affinity
* @attention If the interface is not called after the session is created, and an exception occurs in the process,
* HDC will not detect and release the corresponding
* session resources.
* @param [in]  session    :    Specified session
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcSetSessionReference(HDC_SESSION session);
/**
* @ingroup driver
* @brief Get the base trusted path sent to the specified node device, get trusted path, used to combine dst_path
* parameters of drvHdcSendFile
* @attention host call is valid, used to obtain the basic trusted path sent to the device side using
* the drvHdcSendFile interface
* @param [in]  peer_node         	:	Node number of the node where the Device is located
* @param [in]  peer_devid         :	Device's unified ID in the host
* @param [in]  path_len	:	base_path space size
* @param [out] base_path		:	Obtained trusted path
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcGetTrustedBasePath(int peer_node, int peer_devid, char *base_path, unsigned int path_len);
/**
* @ingroup driver
* @brief Send file to the specified path on the specified device
* @attention Only files in the trustlist can be sent using this interface.
* @param [in]  peer_node        :	Node number of the node where the Device is located
* @param [in]  peer_devid       :	Device's unified ID in the host
* @param [in]  file		:	Specify the file name of the sent file
* @param [in]  dst_path	:	Specifies the path to send the file to the receiver. If the path is directory,
* the file name remains unchanged after it is sent to the peer; otherwise, the file name is changed to the part of the
* path after the file is sent to the receiver.
* @param [out] (*progress_notifier)(struct drvHdcProgInfo *) :	  Specify the user's callback handler function;
* when progress of the file transfer increases by at least one percent,file transfer protocol will call this interface.
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcSendFile(int peer_node, int peer_devid, const char *file, const char *dst_path,
    void (*progress_notifier)(struct drvHdcProgInfo *));
/**
* @ingroup driver
* @brief New interfaces provided based on compatibility do not perform whitelist validation for file names,
* Get the base trusted path sent to the specified node device, get trusted path, used to combine dst_path parameters
* of drvHdcSendFileV2
* @attention host call is valid, used to obtain the basic trusted path sent to the device side using
* the drvHdcSendFileV2 interface
* @param [in]  peer_node         	:	Node number of the node where the Device is located
* @param [in]  peer_devid         :	Device's unified ID in the host
* @param [in]  path_len	:	base_path space size
* @param [out] base_path		:	Obtained trusted path
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcGetTrustedBasePathV2(int peer_node, int peer_devid, char *base_path, unsigned int path_len);
/**
* @ingroup driver
* @brief New interfaces provided based on compatibility do not perform whitelist validation for file names,
* Send file to the specified path on the specified device
* @attention Only files in the trustlist can be sent using this interface.
* @param [in]  peer_node        :	Node number of the node where the Device is located
* @param [in]  peer_devid       :	Device's unified ID in the host
* @param [in]  file		:	Specify the file name of the sent file
* @param [in]  dst_path	:	Specifies the path to send the file to the receiver. If the path is directory,
* the file name remains unchanged after it is sent to the peer; otherwise, the file name is changed to the part of the
* path after the file is sent to the receiver.
* @param [out] (*progress_notifier)(struct drvHdcProgInfo *) :	  Specify the user's callback handler function;
* when progress of the file transfer increases by at least one percent,file transfer protocol will call this interface.
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcSendFileV2(int peer_node, int peer_devid, const char *file, const char *dst_path,
    void (*progress_notifier)(struct drvHdcProgInfo *));
/**
* @ingroup driver
* @brief Request to allocate memory
* @attention Call the kernel function to apply for physical memory. If the continuous physical memory is insufficient,
* it will fail. when HDC is used by DVPP, it can only use low 32-bit memory.
* @param [in]  mem_type  : Memory type, default is 0
* @param [in]  addr : Specifies the start address of the application, default is NULL
* @param [in]  len : length
* @param [in]  align  : The address returned by the application is aligned by align. Currently,
* only 4k is a common multiple
* @param [in]  flag : Memory application flag. low 32-bit memory / hugepage / normal, only valid on the
* device side
* @param [in]  devid : Device id
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT void *drvHdcMallocEx(enum drvHdcMemType mem_type, void *addr, unsigned int align, unsigned int len, int devid,
    unsigned int flag);
/**
* @ingroup driver
* @brief Release memory
* @attention null
* @param [in]  mem_type  : Memory type
* @param [in]  buf : Applied memory address
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcFreeEx(enum drvHdcMemType mem_type, void *buf);

/**
* @ingroup driver
* @brief register va
* @attention null
* @param [in]  signed int devid               dev id
* @param [in]  enum drvHdcMemType mem_type    内存type
* @param [in]  void *va                       内存虚拟地址 (来源为mbuf, 需要支持sp_walk_page_range可翻译)
* @param [in]  unsigned int len               内存的长度 len size 需要满足 4k/2M 对齐
* @param [in]  unsigned int flag              原有flag标志, 保留字段, 传0
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcRegisterMem(signed int devid, enum drvHdcMemType mem_type,
                                       void *va, unsigned int len, unsigned int flag);
/**
* @ingroup driver
* @brief unregister va
* @attention null
* @param [in]  mem_type memory type
* @param [in]  va pointer of memory
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcUnregisterMem(enum drvHdcMemType mem_type, void *va);
/**
* @ingroup driver
* @brief get hdc config
* @attention null
* @param [out]  transType socket or pcie
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
hdcError_t halHdcGetTransType(enum halHdcTransType *transType);
/**
* @ingroup driver
* @brief set hdc trans type
* @attention null
* @param [in]  transType  [HDC_TRANS_USE_SOCKET=0, HDC_TRANS_USE_PCIE=1]
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
hdcError_t halHdcSetTransType(enum halHdcTransType transType);
/**
* @ingroup driver
* @brief wait mem finish release va
* @attention null
* @param [in]  session
* @param [in]  time_out
* @param [out] msg
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
hdcError_t halHdcWaitMemRelease(HDC_SESSION session, int time_out, struct drvHdcFastRecvMsg *msg);
/**
* @ingroup driver
* @brief wait mem finish release va
* @attention null
* @param [in]  session
* @param [in]  input
* @param [out] msg
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
hdcError_t halHdcWaitMemReleaseEx(HDC_SESSION session,
    struct drvHdcWaitMsgInput *input, struct drvHdcFastSendFinishMsg *msg);
/**
* @ingroup driver
* @brief Map DMA address
* @attention null
* @param [in]  mem_type   : Memory type
* @param [in]  buf : Applied memory address
* @param [in]  devid : Device id
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcDmaMap(enum drvHdcMemType mem_type, void *buf, int devid);
/**
* @ingroup driver
* @brief UnMap DMA address
* @attention null
* @param [in]  mem_type   : Memory type
* @param [in]  buf : Applied memory address
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcDmaUnMap(enum drvHdcMemType mem_type, void *buf);
/**
* @ingroup driver
* @brief ReMap DMA address
* @attention null
* @param [in]  mem_type   : Memory type
* @param [in]  buf : Applied memory address
* @param [in]  devid : Device id
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcDmaReMap(enum drvHdcMemType mem_type, void *buf, int devid);

/* hdc epoll func */
/**
* @ingroup driver
* @brief HDC epoll create interface
* @attention null
* @param [in]  size    : Specify the number of file handles to listen on
* @param [out]  epoll : Returns the supervised epoll handle
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcEpollCreate(int size, HDC_EPOLL *epoll);
/**
* @ingroup driver
* @brief close HDC epoll interface
* @attention null
* @param [in]  epoll : Returns the supervised epoll handle
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcEpollClose(HDC_EPOLL epoll);
/**
* @ingroup driver
* @brief HDC epoll control interface
* @attention null
* @param [in]  epoll : Specify the created epoll handle
* @param [in]  op : Listen event operation type
* @param [in]  target : Specify to add / remove resource topics
* @param [in]  event : Used with target, HDC_EPOLL_CONN_IN Used with HDC_SERVER to monitor whether
* there is a new connection; HDC_EPOLL_DATA_IN Cooperate with HDC_SESSION to monitor data entry of normal channels
; HDC_EPOLL_FAST_DATA_IN Cooperate with HDC_SESSION to monitor fast channel data entry
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcEpollCtl(HDC_EPOLL epoll, int op, void *target, struct drvHdcEvent *event);
/**
* @ingroup driver
* @brief wait HDC epoll interface
* @attention null
* @param [in]  epoll : Specify the created epoll handle
* @param [in]  maxevents : Specify the maximum number of events returned
* @param [in]  timeout : Set timeout
* @param [out]  events : Returns the triggered event
* @param [out]  eventnum : Returns the number of valid events
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t drvHdcEpollWait(HDC_EPOLL epoll, struct drvHdcEvent *events, int maxevents, int timeout,
                                     int *eventnum);
/**
* @ingroup driver
* @brief Get the information of the session.
* @attention null
* @param [in]  session : Specify in which session
* @param [out] info  : session info
* @param [out] info->devid  : session devid
* @param [out] info->fid  : session fid
* @param [out] info->res  : reserved
* @return DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t halHdcGetSessionInfo(HDC_SESSION session, struct drvHdcSessionInfo *info);
/**
* @ingroup driver
* @brief Send data based on HDC Session
* @attention This interface sends the message encapsulated with the buffer address and length to the peer end.
* @param [in]  session    : Specify in which session to send data
* @param [in]  msg : Descriptor pointer for sending messages. The maximum sending length
* must be obtained through the drvHdcGetCapacity function
* @param [in]  flag               : Reserved parameter, currently fixed 0
* @param [in]  timeout   : Allow time for send timeout determined by user mode
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcSend(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag, UINT32 timeout);
/**
* @ingroup driver
* @brief Session zero-copy fast sending interface, applications need to apply for memory through "drvHdcMallocEx"
* in advance
* @attention After send function returns,src address cannot be reused directly. It must wait for peer to receive it.
* @param [in]  HDC_SESSION session    : Specify in which session
* @param [in]  msg : Send and receive information
* @param [in]  int flag : Fill in 0 default blocking, HDC_FLAG_NOWAIT set non-blocking
* @param [in]  unsigned int timeout   : Allow time for sending timeout determined by user mode
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcFastSend(HDC_SESSION session, struct drvHdcFastSendMsg msg, UINT64 flag, UINT32 timeout);
/**
* @ingroup driver
* @brief Receive data based on HDC Session
* @attention The interface will parse the message sent by the peer, obtain the data buffer address and length,
* save it in the message descriptor, and return it to the upper layer.
* @param [in]  HDC_SESSION session   : Specify in which session to receive data
* @param [in]  int bufLen            : The length of each receive buffer in bytes
* @param [in]  u64 flag              : Fixed 0
* @param [in]  unsigned int timeout   : Allow time for sending timeout determined by user mode
* @param [out] struct drvHdcMsg *msg : Descriptor pointer for receiving messages
* @param [out] int *recvBufCount      : The number of buffers that actually received the data
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcRecv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen,
    UINT64 flag, int *recvBufCount, UINT32 timeout);
/**
* @ingroup driver
* @brief Receive data based on HDC Session
* @attention The interface will parse the message sent by the peer, obtain the data buffer address and length,
* save it in the message descriptor, and return it to the upper layer.
* @param [in]  session   : Specify in which session to receive data
* @param [in]  bufLen            : The length of each receive buffer in bytes
* @param [in]  userConfig : Record the parameters set by the user
* @param [out] msg : Descriptor pointer for receiving messages
* @param [out] recvBufCount      : The number of buffers that actually received the data
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcRecvEx(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen,
    int *recvBufCount, struct drvHdcRecvConfig *userConfig);
/**
* @ingroup driver
* @brief Session copy-free fast sending interface, applications need to apply for memory through hdc in advance
* @attention Need to apply for memory through hdc in advance. And after the send function returns, the src address
* cannot be reused directly. It must wait for the peer to receive it.
* @param [in]  session    : Specify in which session
* @param [in]  msg : Send and receive information
* @param [in]  flag : Fill in 0 default blocking, HDC_FLAG_NOWAIT set non-blocking
* @param [in]  timeout   : Allow time for sending timeout determined by user mode
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcFastRecv(HDC_SESSION session, struct drvHdcFastRecvMsg *msg, UINT64 flag, UINT32 timeout);
/**
* @ingroup driver
* @brief Get the information of session
* @attention null
* @param [in]  session : Specify the session need to query
* @param [in]  attr : Fill in information type
* @param [out] value : Returns information
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t halHdcGetSessionAttr(HDC_SESSION session, int attr, int *value);
/**
* @ingroup driver
* @brief Get the information of server
* @attention null
* @param [in]  server : Specify the server need to query
* @param [in]  attr : Fill in information type
* @param [out] value : Returns information
* @return   DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT hdcError_t halHdcGetServerAttr(HDC_SERVER server, int attr, int *value);

/**
* @ingroup driver
* @brief get device gateway address command proc.
* @attention null
* @return 0 success
*/
DLLEXPORT int dsmi_cmd_get_network_device_info(int device_id, const char *inbuf, unsigned int size_in, char *outbuf,
                                               unsigned int *size_out);
enum log_error_code {
    LOG_OK = 0,
    LOG_ERROR = -1,
    LOG_NOT_READY = -2,
    LOG_NOT_SUPPORT = -5,
};

#define LOG_CHANNEL_TYPE_AICPU (10)
#define LOG_DEVICE_ID_MAX (64)
#define LOG_CHANNEL_NUM_MAX (64)
#define LOG_SLOG_BUF_MAX_SIZE (2 * 1024 * 1024)
#define LOG_DRIVER_NAME "log_drv"
enum log_channel_type {
    /* range 0-9 is for ts */
    LOG_CHANNEL_TYPE_TS = 0,
    LOG_CHANNEL_TYPE_TS_PROC = 1,

    LOG_CHANNEL_TYPE_MCU_DUMP = 10,

    LOG_CHANNEL_TYPE_LPM3 = 30,
    LOG_CHANNEL_TYPE_IMP = 31,
    LOG_CHANNEL_TYPE_IMU = 32,

    LOG_CHANNEL_TYPE_ISP = 33,

    LOG_CHANNEL_TYPE_SIS = 37,
    LOG_CHANNEL_TYPE_HSM = 38,
    LOG_CHANNEL_TYPE_SIS_BIST = 39,
    LOG_CHANNEL_TYPE_BIOS_ATF = 40,
    LOG_CHANNEL_TYPE_RTC = 41,
    LOG_CHANNEL_TYPE_EVENT = 42,

    LOG_CHANNEL_TYPE_MAX
};

/**
* @ingroup driver
* @brief get log information by device id and channel type.
* @attention null
* @param [in] device_id   device ID
* @param [out] buf Store log information
* @param [out] size size of log information store in buf,As a input parameter means max size of buf.
* @param [in] timeout   timeout to read log infomation
* @param [in] channel_type   which channel to read
* @return  0 for success, others for fail
*/
int log_read_by_type(int device_id, char *buf, unsigned int *size, int timeout, enum log_channel_type channel_type);

/**
* @ingroup driver
* @brief set log level by device id and channel type.
* @attention null
* @param [in] device_id   device ID
* @param [in] channel_type   which channel to set
* @param [in] log_level   which level to set
* @return  0 for success, others for fail
*/
int log_set_level(int device_id, int channel_type, unsigned int log_level);

/**
* @ingroup driver
* @brief get channel type from device
* @attention null
* @param [in] device_id   device ID
* @param [out] channel_type_set   set of channel_type
* @param [out] channel_type_num   number of channel_type
* @param [in] set_size   total number of channel_type_set
* @return  0 for success, others for fail
*/
int log_get_channel_type(int device_id, int *channel_type_set, int *channel_type_num, int set_size);

/**
* @ingroup driver
* @brief get set of device
* @attention null
* @param [out] device_id_set   set of device ID
* @param [out] device_id_num   number of device ID
* @param [in] set_size   total number of device_id_set
* @return  0 for success, others for fail
*/
int log_get_device_id(int *device_id_set, int *device_id_num, int set_size);

/**
* @ingroup driver
* @brief get log information by device id.
* @attention null
* @param [in] device_id   device ID
* @param [out] buf Store log information
* @param [out] size size of log information store in buf,As a input parameter means max size of buf.
* @param [in] timeout   timeout to read log infomation
* @return  0 for success, others for fail
*/
int log_read(int device_id, char *buf, unsigned int *size, int timeout);

/**
* @ingroup driver
* @brief set parameters to channel type
* @attention null
* @param [in] devid : device id
* @param [in] chan_type : which channel to set
* @param [in] data : parameters to be delivered to the ts
* @param [in] data_len : size of data
* @return  0 for success, others for fail
*/
int log_set_dfx_param(uint32_t devid, uint32_t chan_type, void *data, uint32_t data_len);

/**
* @ingroup driver
* @brief get parameters from channel type
* @attention null
* @param [in] devid : device id
* @param [in] chan_type : which channel to set
* @param [in/out] data : parameters to be delivered to the ts and information obtained from the ts
* @param [in] data_len : size of data
* @return  0 for success, others for fail
*/
int log_get_dfx_param(uint32_t devid, uint32_t chan_type, void *data, uint32_t data_len);

/**
* @ingroup driver
* @brief alloc mem from kernel and map to user for slogd by type
* @attention null
* @param [in] devid   device ID
* @param [in] type   the type of mem
* @param [in/out] size   the size of mem firstly allocated
* @return void *buff  buff mmaped for user space
*/
void* log_type_alloc_mem(uint32_t device_id, uint32_t type, uint32_t *size);

#ifndef dma_addr_t
typedef unsigned long long dma_addr_t;
#endif

/**< profile drv user */
#define PROF_DRIVER_NAME "prof_drv"
#define PROF_OK (0)
#define PROF_ERROR (-1)
#define PROF_TIMEOUT (-2)
#define PROF_STARTED_ALREADY (-3)
#define PROF_STOPPED_ALREADY (-4)
#define PROF_ERESTARTSYS (-5)
#define CHANNEL_NUM 160

#define CHANNEL_HBM (1)
#define CHANNEL_BUS (2)
#define CHANNEL_PCIE (3)
#define CHANNEL_NIC (4)
#define CHANNEL_DMA (5)
#define CHANNEL_DVPP (6)
#define CHANNEL_DDR (7)
#define CHANNEL_LLC (8)
#define CHANNEL_HCCS (9)
#define CHANNEL_TSCPU (10)

#define CHANNEL_BIU_GROUP0_AIC (11)
#define CHANNEL_BIU_GROUP0_AIV0 (12)
#define CHANNEL_BIU_GROUP0_AIV1 (13)
#define CHANNEL_BIU_GROUP1_AIC (14)
#define CHANNEL_BIU_GROUP1_AIV0 (15)
#define CHANNEL_BIU_GROUP1_AIV1 (16)
#define CHANNEL_BIU_GROUP2_AIC (17)
#define CHANNEL_BIU_GROUP2_AIV0 (18)
#define CHANNEL_BIU_GROUP2_AIV1 (19)
#define CHANNEL_BIU_GROUP3_AIC (20)
#define CHANNEL_BIU_GROUP3_AIV0 (21)
#define CHANNEL_BIU_GROUP3_AIV1 (22)
#define CHANNEL_BIU_GROUP4_AIC (23)
#define CHANNEL_BIU_GROUP4_AIV0 (24)
#define CHANNEL_BIU_GROUP4_AIV1 (25)
#define CHANNEL_BIU_GROUP5_AIC (26)
#define CHANNEL_BIU_GROUP5_AIV0 (27)
#define CHANNEL_BIU_GROUP5_AIV1 (28)
#define CHANNEL_BIU_GROUP6_AIC (29)
#define CHANNEL_BIU_GROUP6_AIV0 (30)
#define CHANNEL_BIU_GROUP6_AIV1 (31)
#define CHANNEL_BIU_GROUP7_AIC (32)
#define CHANNEL_BIU_GROUP7_AIV0 (33)
#define CHANNEL_BIU_GROUP7_AIV1 (34)
#define CHANNEL_BIU_GROUP8_AIC (35)
#define CHANNEL_BIU_GROUP8_AIV0 (36)
#define CHANNEL_BIU_GROUP8_AIV1 (37)
#define CHANNEL_BIU_GROUP9_AIC (38)
#define CHANNEL_BIU_GROUP9_AIV0 (39)
#define CHANNEL_BIU_GROUP9_AIV1 (40)
#define CHANNEL_BIU_GROUP10_AIC (41)
#define CHANNEL_BIU_GROUP10_AIV0 (42)

#define CHANNEL_AICORE (43)

#define CHANNEL_TSFW (44)      // add for ts0 as tsfw channel
#define CHANNEL_HWTS_LOG (45)  // add for ts0 as hwts channel
#define CHANNEL_KEY_POINT (46)
#define CHANNEL_TSFW_L2 (47)   /* add for ascend910 and ascend610 */
#define CHANNEL_HWTS_LOG1 (48) // add for ts1 as hwts channel
#define CHANNEL_TSFW1 (49)     // add for ts1 as tsfw channel
#define CHANNEL_STARS_SOC_LOG_BUFFER (50)
#define CHANNEL_STARS_BLOCK_LOG_BUFFER (51)
#define CHANNEL_STARS_SOC_PROFILE_BUFFER (52)
#define CHANNEL_FFTS_PROFILE_BUFFER_TASK (53)
#define CHANNEL_FFTS_PROFILE_BUFFER_SAMPLE (54)

#define CHANNEL_BIU_GROUP10_AIV1 (55)
#define CHANNEL_BIU_GROUP11_AIC (56)
#define CHANNEL_BIU_GROUP11_AIV0 (57)
#define CHANNEL_BIU_GROUP11_AIV1 (58)
#define CHANNEL_BIU_GROUP12_AIC (59)
#define CHANNEL_BIU_GROUP12_AIV0 (60)
#define CHANNEL_BIU_GROUP12_AIV1 (61)
#define CHANNEL_BIU_GROUP13_AIC (62)
#define CHANNEL_BIU_GROUP13_AIV0 (63)
#define CHANNEL_BIU_GROUP13_AIV1 (64)
#define CHANNEL_BIU_GROUP14_AIC (65)
#define CHANNEL_BIU_GROUP14_AIV0 (66)
#define CHANNEL_BIU_GROUP14_AIV1 (67)
#define CHANNEL_BIU_GROUP15_AIC (68)
#define CHANNEL_BIU_GROUP15_AIV0 (69)
#define CHANNEL_BIU_GROUP15_AIV1 (70)
#define CHANNEL_BIU_GROUP16_AIC (71)
#define CHANNEL_BIU_GROUP16_AIV0 (72)
#define CHANNEL_BIU_GROUP16_AIV1 (73)
#define CHANNEL_BIU_GROUP17_AIC (74)
#define CHANNEL_BIU_GROUP17_AIV0 (75)
#define CHANNEL_BIU_GROUP17_AIV1 (76)
#define CHANNEL_BIU_GROUP18_AIC (77)
#define CHANNEL_BIU_GROUP18_AIV0 (78)
#define CHANNEL_BIU_GROUP18_AIV1 (79)
#define CHANNEL_BIU_GROUP19_AIC (80)
#define CHANNEL_BIU_GROUP19_AIV0 (81)
#define CHANNEL_BIU_GROUP19_AIV1 (82)
#define CHANNEL_BIU_GROUP20_AIC (83)
#define CHANNEL_BIU_GROUP20_AIV0 (84)

#define CHANNEL_AIV (85)

#define CHANNEL_BIU_GROUP20_AIV1 (86)
#define CHANNEL_BIU_GROUP21_AIC (87)
#define CHANNEL_BIU_GROUP21_AIV0 (88)
#define CHANNEL_BIU_GROUP21_AIV1 (89)
#define CHANNEL_BIU_GROUP22_AIC (90)
#define CHANNEL_BIU_GROUP22_AIV0 (91)
#define CHANNEL_BIU_GROUP22_AIV1 (92)
#define CHANNEL_BIU_GROUP23_AIC (93)
#define CHANNEL_BIU_GROUP23_AIV0 (94)
#define CHANNEL_BIU_GROUP23_AIV1 (95)
#define CHANNEL_BIU_GROUP24_AIC (96)
#define CHANNEL_BIU_GROUP24_AIV0 (97)
#define CHANNEL_BIU_GROUP24_AIV1 (98)

#define CHANNEL_TSCPU_MAX (128)
#define CHANNEL_ROCE (129)
#define CHANNEL_NPU_APP_MEM (130) /* HBM and DDR used on app level */
#define CHANNEL_NPU_MEM (131)     /* HBM and DDR used on device level */
#define CHANNEL_LP        (132)
#define CHANNEL_QOS       (133)
#define CHANNEL_SOC_PMU   (134)
#define CHANNEL_DVPP_VENC (135)
#define CHANNEL_DVPP_JPEGE (136)
#define CHANNEL_DVPP_VDEC (137)
#define CHANNEL_DVPP_JPEGD (138)
#define CHANNEL_DVPP_VPC (139)
#define CHANNEL_DVPP_PNG (140)
#define CHANNEL_DVPP_SCD (141)
#define CHANNEL_NPU_MODULE_MEM (142)
#define CHANNEL_AICPU (143)
#define CHANNEL_CUS_AICPU (144)
#define CHANNEL_ADPROF (145)
#define CHANNEL_STARS_NANO_PROFILE (150) /* add for ascend035 */
#define CHANNEL_IDS_MAX CHANNEL_NUM

#define PROF_NON_REAL 0
#define PROF_REAL 1
#define DEV_NUM 64

/* this struct = the one in "prof_drv_dev.h" */
typedef struct prof_poll_info {
    unsigned int device_id;
    unsigned int channel_id;
} prof_poll_info_t;

/* add for get prof channel list */
#define PROF_CHANNEL_NAME_LEN 32
#define PROF_CHANNEL_NUM_MAX 160
struct channel_info {
    char channel_name[PROF_CHANNEL_NAME_LEN];
    unsigned int channel_type; /* system / APP */
    unsigned int channel_id;
};

typedef struct channel_list {
    unsigned int chip_type;
    unsigned int channel_num;
    struct channel_info channel[PROF_CHANNEL_NUM_MAX];
} channel_list_t;

/**
* @ingroup driver
* @brief Trigger to get enable channels
* @attention null
* @param [in] device_id : Device ID
* @param [in/out] channels : Channels list
* @return  0 for success, others for fail
*/
DLLEXPORT int prof_drv_get_channels(unsigned int device_id, channel_list_t *channels);

typedef enum prof_channel_type {
    PROF_TS_TYPE,
    PROF_PERIPHERAL_TYPE,
    PROF_CHANNEL_TYPE_MAX,
} PROF_CHANNEL_TYPE;

typedef struct prof_start_para {
    PROF_CHANNEL_TYPE channel_type;     /* for ts and other device */
    unsigned int sample_period;
    unsigned int real_time;             /* real mode */
    void *user_data;                    /* ts data's pointer */
    unsigned int user_data_size;        /* user data's size */
} prof_start_para_t;

/**
* @ingroup driver
* @brief Trigger prof sampling start
* @attention null
* @param [in] device_id : Device ID
* @param [in] channel_id : Channel ID(1--(CHANNEL_NUM - 1))
* @param [in] channel_type : Sampling channel type
* @param [in] sample_period : Sampling period
* @param [in] real_time : Real-time mode or non-real-time mode
* @param [in] *user_data : Sampling private configuration
* @param [in] user_data_size : Sampling private configuration length
* @return  0 for success, others for fail
*/
DLLEXPORT int prof_drv_start(unsigned int device_id, unsigned int channel_id, struct prof_start_para *start_para);
/**
* @ingroup driver
* @brief Trigger prof sampling end
* @attention nul
* @param [in] device_id : Device ID
* @param [in] channel_id : Channel ID(1--(CHANNEL_NUM - 1))
* @return   0 for success, others for fail
*/
DLLEXPORT int prof_stop(unsigned int device_id, unsigned int channel_id);
/**
* @ingroup driver
* @brief Read and collect profile information
* @attention null
* @param [in] device_id : Device ID
* @param [in] channel_id : Channel ID(1--(CHANNEL_NUM - 1))
* @param [in/out] *out_buf : Store read profile information
* @param [in] buf_size : Store the length of the profile to be read
* @return   >= 0   success
* @return positive number for readable buffer length
* @return  < 0 for fail
*/
DLLEXPORT int prof_channel_read(unsigned int device_id, unsigned int channel_id, char *out_buf, unsigned int buf_size);
/**
* @ingroup driver
* @brief Querying valid channel information
* @attention null
* @param [in/out] *out_buf : Readable channel list
* @param [in] num : Number of channels to monitor
* @param [in] timeout : Timeout in seconds
* @return 0  No channels available
* @return positive number for channels Number
* @return < 0 for fail
*/
DLLEXPORT int prof_channel_poll(struct prof_poll_info *out_buf, int num, int timeout);

/**
* @ingroup driver
* @brief flush data of a specified channel
* @attention null
* @param [in] device_id : Device ID
* @param [in] channel_id : Channel ID(1--(CHANNEL_NUM - 1))
* @param [in/out] *data_len : Store the length of the profile to be read
* @return PROF_OK flush ok
* @return PROF_STOPPED_ALREADY means channel is stopped
* @return DRV_ERROR_NOT_SUPPORT for not support
*/
DLLEXPORT int halProfDataFlush(unsigned int device_id, unsigned int channel_id, unsigned int *data_len);

/**
* @ingroup driver
* @brief get phy addr of virtual addr buf
* @attention null
* @param [in] void *buf: virtual addr
* @param [out] unsigned long long *phyAddr: physical addr
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffGetPhyAddr(void *buf, unsigned long long *phyAddr);

/**
* @ingroup driver
* @brief alloc buff
* @attention null
* @param [in] unsigned int size: The amount of memory space requested
* @param [out] void **buff: buff addr alloced
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffAlloc(uint64_t size, void **buff);
/**
* @ingroup driver
* @brief alloc buff from pool
* @attention null
* @param [in] poolHandle pHandle: pool handle
* @param [out] void **buff: buff addr alloced
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffAllocByPool(poolHandle pHandle, void **buff);
/**
* @ingroup driver
* @brief buff alloc interface
* @attention null
* @param [in] unsigned int size: size of buff to alloc
* @param [in] unsigned long flag: flag of buff to alloc(bit0~31: mem type, bit32~bit39: devid, bit40~63: resv)
* @param [in] int grp_id: group id num
* @param [out] buff **buff: buff alloced
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffAllocEx(uint64_t size, unsigned long flag, int grp_id, void **buff);

/**
* @ingroup driver
* @brief buff alloc interface
* @attention null
* @param [in] unsigned int size: size of buff to alloc
* @param [in] unsigned int align: align of buff to alloc
* @param [in] unsigned long flag: flag of buff to alloc(bit0~31: mem type, bit32~bit39: devid, bit40~63: resv)
* @param [in] int grp_id: group id
* @param [out] buff **buff: buff alloced
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffAllocAlignEx(uint64_t size, unsigned int align, unsigned long flag, int grp_id, void **buff);

/**
* @ingroup driver
* @brief buff memory get interface
* @attention only take effect in the process which is not the memory alloced process
* @param [in] Mbuf *mbuf : the mbuf addr that data is buf
* @param [in] void *buf: start addr of buff that need to check
* @param [in] unsigned int size: size of buff that need to check
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halBuffGet(Mbuf *mbuf, void *buf, unsigned long size);

/**
* @ingroup driver
* @brief buff memory put interface
* @attention null
* @param [in] Mbuf *mbuf: the mbuf addr that data is buf
* @param [in] void *buf: start addr of buff that has been get
* @return null
*/
DLLEXPORT void halBuffPut(Mbuf *mbuf, void *buf);

/**
* @ingroup driver
* @brief buff memory pool get interface
* @attention only take effect in the process which is not the memory alloced process
* @param [in] void* poolStart : start addr of pool
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffPoolGet(void* poolStart);

/**
* @ingroup driver
* @brief buff memory pool put interface
* @attention must call after buff memory pool get interface
* @param [in] void* poolStart : start addr of pool
* @return  0 for success, others for fail
*/
DLLEXPORT int halBuffPoolPut(void* poolStart);

/**
 * @ingroup driver
 * @brief Mbuf alloc interface
 * @attention null
 * @param [out] Mbuf **mbuf: Mbuf alloced
 * @param [in] unsigned int size: size of Mbuf to alloc
 * @param [in] unsigned int align: align of Mbuf to alloc(32~4096)
 * @param [in] unsigned long flag: huge page flag(bit0~31: mem type, bit32~bit39: devid, bit40~63: resv)
 * @param [in] int grp_id: group id
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT int halMbufAllocEx(uint64_t size, unsigned int align, unsigned long flag, int grp_id, Mbuf **mbuf);

/**
* @ingroup driver
* @brief verify mbuf is valid
* @attention null
* @param [in]  Mbuf *mbuf: mbuf need to verify
* @param [in]  int type: value 0 check single mbuf, value 1 check mbuf chain
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufVerify(Mbuf *mbuf, unsigned int type);
/**
* @ingroup driver
* @brief create a Mbuf using a normal buff
* @attention null
* @param [in] void *buff: buff addr
* @param [in] unsigned int len: buff len
* @param [out] Mbuf **mbuf: new Mbuf addr
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufBuild(void *buff, uint64_t len, Mbuf **mbuf);

/**
* @ingroup driver
* @brief release mbuf head and return the nomal buff
* @attention buff must be referenced only by this mbuf
* @param [in] Mbuf *mbuf: mbuf handle
* @param [out] void **buff: buff addr
* @param [out] uint64_t *len: buff len that decided by halBuffAlloc
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufUnBuild(Mbuf *mbuf, void **buff, uint64_t *len);

/**
* @ingroup driver
* @brief Subscribe event for memory pool size changes
* @attention the maximum subscription amount supported is 4
* @param [in] grpName, the share grp name of buff
* @param [in] threadGrpId, the subscription group to which the event belongs
* @param [in] event_id, 0~EVENT_MAX_NUM
* @param [in] devid, device id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halBufEventSubscribe(
    const char *grpName, unsigned int threadGrpId, unsigned int event_id, unsigned int devid) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Event notification when memory pool size changes
* @attention null
* @param [in] grpName, the share grp name of buff
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halBufEventReport(const char *grpName);

/**
* @ingroup driver
* @brief add one process to a group, There are two forms, one is to generate the group ID,
* and the other is to specify the group ID.
* @attention null
* @param [in] type: GROUP_ID_CREATE or GROUP_ID_ADD
* @param [in] pid: pid of the process
* @param [in/out] grp_id: As an input parameter when type is GROUP_ID_ADD and must be
* within [1,99999], as an output parameter when the type is GROUP_ID_CREATE.
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffGroupConfig(GROUP_ID_TYPE type, int pid, int *grp_id);


/**
* @ingroup driver
* @brief cache memory alloc
* @attention halGrpCreate must enable cacheAllocFlag first
* @param [in] name, grp name
* @param [in] devId, device id
* @param [in] para, cache alloc parameter
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGrpCacheAlloc(const char *name, unsigned int devId, GrpCacheAllocPara *para);

/**
* @ingroup driver
* @brief cache memory free
* @attention all process in the buff group must call halBuffProcCacheFree first
* @param [in] name, grp name
* @param [in] devId, device id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGrpCacheFree(const char *name, unsigned int devId);

/**
* @ingroup driver
* @brief buff grp query
* @attention null
* @param [in] cmd, cmd type
* @param [in] inBuff, query input buff
* @param [in] inLen, query input buff len
* @param [out] outBuff, query output buff
* @param [out] outLen, query output buff len
* @return   0 for success, others for fail
*/
DLLEXPORT int halGrpQuery(GroupQueryCmdType cmd,
    void *inBuff, unsigned int inLen, void *outBuff, unsigned int *outLen) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief buff cache free in current process
* @attention ensure to free buff memory first
* @param [in] flag, (bit0~30: resv, bit31: free all dev, bit32~bit39: devid, bit40~63: resv)
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halBuffProcCacheFree(unsigned long flag);


/**
* @ingroup driver
* @brief buff pub pool init
* @attention null
* @param [in] attr, pub pool init attr
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffInitPubPool(PubPoolAttr *attr) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief buff pub pool attach
* @attention null
* @param [in] timeout, pub pool attach timeout in (ms)
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffAttachPubPool(int timeout) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief buff pub pool alloc
* @attention null
* @param [in] size, buf size
* @param [out] mbuf, needed mbuf
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufAllocByPubPool(uint64_t size, Mbuf **mbuf) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Get series of information from the specified Mbuf
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [out] MbufInfoConverge *mbufInfo: mbuf information
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufGetMbufInfo(Mbuf *mbuf, MbufInfoConverge *mbufInfo);

/*=========================== Tsdrv ===========================*/
/*============================add from aicpufw_drv_msg.h"==========================================*/
struct hwts_ts_kernel {
    pid_t pid;
    unsigned short kernel_type : 8;
    unsigned short batchMode : 1; // default 0
    unsigned short satMode : 1;
    unsigned short rspMode : 1;
    unsigned short resv : 5;
    unsigned short streamID;
    unsigned long long kernelName;
    unsigned long long kernelSo;
    unsigned long long paramBase;
    unsigned long long l2VaddrBase;
    unsigned long long l2Ctrl;
    unsigned short blockId;
    unsigned short blockNum;
    unsigned int l2InMain;
    unsigned long long taskID;
};

#define RESERVED_ARRAY_SIZE         11
typedef struct drv_hwts_task_response {
    volatile unsigned int valid;
    volatile unsigned int state;
    volatile unsigned long long serial_no;
    volatile unsigned int reserved[RESERVED_ARRAY_SIZE];
} drv_aicpufw_task_response_t;

/*=========================== New add below===========================*/
/* Not allow hwts_ts_task to be parameter passing in aicpufw and drv */
struct hwts_ts_task {
    unsigned int mailbox_id;
    volatile unsigned long long serial_no;
    struct hwts_ts_kernel kernel_info;
};

typedef enum hwts_task_status {
    TASK_SUCC = 0,
    TASK_FAIL = 1,
    TASK_OVERFLOW = 2,
    TASK_STATUS_MAX,
} HWTS_TASK_STATUS;


#define HWTS_RESPONSE_RSV   3
typedef struct hwts_response {
    unsigned int result;        /* RESPONSE_RESULE_E */
    unsigned int mailbox_id;
    unsigned long long serial_no;
    unsigned int status;
    int rsv[HWTS_RESPONSE_RSV];
    char* msg;
    int len;
} hwts_response_t;

typedef enum tagDrvIdType {
    DRV_STREAM_ID = 0,
    DRV_EVENT_ID,
    DRV_MODEL_ID,
    DRV_NOTIFY_ID,
    DRV_CMO_ID,
    DRV_CNT_NOTIFY_ID,    /* add start ascend910_95 */
    DRV_INVALID_ID,
} drvIdType_t;

#define RESOURCEID_RESV_STREAM_PRIORITY 0
#define RESOURCEID_RESV_FLAG    1

#define RESOURCEID_RESV_LENGTH  8

struct halResourceIdInputInfo {
    drvIdType_t type;   // Resource Id Type
    uint32_t tsId;
    uint32_t resourceId;    // the id that will be freed, halResourceIdAlloc does not care about this variable
    uint32_t res[RESOURCEID_RESV_LENGTH];    // 0:stream pri, 1:flag
};

struct halResourceIdOutputInfo {
    uint32_t resourceId;
    uint32_t res[RESOURCEID_RESV_LENGTH];
};

/**
* @ingroup driver
* @brief  resource id alloc interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halResourceIdInputInfo
*              type: Resource Id Type
*              tsId: ts id,  ascend310:0, ascend910 :0
*              resourceId: not used
* @param [out] *out  See struct halResourceIdOutputInfo
*              resourceId: applied resource id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceIdAlloc(uint32_t devId, struct halResourceIdInputInfo *in,
    struct halResourceIdOutputInfo *out);

/**
* @ingroup driver
* @brief  resource id free interface
* @attention:
*   1)assure free normalcqsq before free stream_id on 910B&310B
* @param [in] devId: logic devid
* @param [in]  *in   See struct halResourceIdInputInfo
*              type: Resource Id Type
*              tsId: ts id,  ascend310:0, ascend910 :0
*              resourceId:  resource id will be freed
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceIdFree(uint32_t devId, struct halResourceIdInputInfo *in);

/**
* @ingroup driver
* @brief  resource enable interface, DRV_STREAM_ID surport
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halResourceIdInputInfo
*              type: Resource Id Type
*              tsId: ts id,  ascend310:0, ascend910 :0
*              resourceId:  resource id will be freed
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceEnable(uint32_t devId, struct halResourceIdInputInfo *in) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  resource disable interface, DRV_STREAM_ID surport
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halResourceIdInputInfo
*              type: Resource Id Type
*              tsId: ts id,  ascend310:0, ascend910 :0
*              resourceId:  resource id will be freed
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceDisable(uint32_t devId, struct halResourceIdInputInfo *in) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  resource config interface, DRV_STREAM_ID surport
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halResourceIdInputInfo
*              type: Resource Id Type
*              tsId: ts id,  ascend310:0, ascend910 :0
*              resourceId:  resource id will be freed
* @param [in] para: config parameter
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceConfig(uint32_t devId, struct halResourceIdInputInfo *in,
    struct halResourceConfigInfo *para);
/**
* @ingroup driver
* @brief  resource detail query interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halResourceIdInputInfo
*              type: Resource Id Type
*              tsId: ts id,  ascend310:0, ascend910 :0
*              resourceId:  resource id will be queried
* @param [in]  *info See struct halResourceDetailInfo
*              type: query type
* @param [out] *info See struct halResourceDetailInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceDetailQuery(uint32_t devId, struct halResourceIdInputInfo *in,
    struct halResourceDetailInfo *info);

typedef enum tagDrvResourceType {
    DRV_RESOURCE_STREAM_ID = 0,
    DRV_RESOURCE_EVENT_ID,
    DRV_RESOURCE_MODEL_ID,
    DRV_RESOURCE_NOTIFY_ID,
    DRV_RESOURCE_CMO_ID,
    DRV_RESOURCE_SQ_ID,
    DRV_RESOURCE_CQ_ID,
    DRV_RESOURCE_CNT_NOTIFY_ID,    /* add start ascend910_95 */
    DRV_RESOURCE_INVALID_ID,
} drvResourceType_t;

struct halResourceInfo {
    uint32_t capacity;
    uint32_t usedNum; /* rts and dvpp used. just for host or device */
    uint32_t reserve[2];
};

/**
* @ingroup driver
* @brief  resource capacity and used number query interface
* @attention null
* @param [in] devId: logic devid
* @param [in] tsId: ts id,  ascend310:0, ascend910 :0
* @param [in] type: query type,  stream:0, event:1, model:2, notify:3
* @param [out] *info See struct halResourceInfo
*              capacity: the real capacity of id resource
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceInfoQuery(uint32_t devId, uint32_t tsId, drvResourceType_t type,
    struct halResourceInfo *info);

typedef enum tagDrvResIdType {
    TRS_RES_ID_ADDR,
    TRS_RES_INVALID_ID,
} drvResIdProcType;

struct drvResIdKey {
    uint32_t ruDevId;   /* ruDevId is viewed from host. */
    uint32_t tsId;
    drvIdType_t resType;
    uint32_t resId;
    uint32_t flag;  /* flag is used for distingush whether ruDevid is sdid. */
    uint32_t rsv[3]; /* 4 is rsv */
};

/**
* @ingroup driver
* @brief  check resource info whether valid
* @attention null
* @param [in]  *info  See struct drvResIdInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceIdCheck(struct drvResIdKey *info);

/**
* @ingroup driver
* @brief  get resource id info
* @attention null
* @param [in]  *info  See struct drvResIdKey
* @param [in]  type  See drvResIdProcType
* @param [out] *value addr or others
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceIdInfoGet(struct drvResIdKey *key,  drvResIdProcType type,  uint64_t *value);

/**
* @ingroup driver
* @brief  resource id restore
* @attention null
* @param [in]  *info  See struct drvResIdKey
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halResourceIdRestore(struct drvResIdKey *info);

/**
* @ingroup driver
* @brief  tsdrv IO contrl interface
* @attention null
* @param [in] devId: logic devid
* @param [in] type: IO contrl type
              *param: parameter
              paramSize: parameter size
* @param [out] *out: out data
               *outSize: out data size
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halTsdrvCtl(uint32_t devId, int cmd, void *param, size_t paramSize, void *out, size_t *outSize);

/**
* @ingroup driver
* @brief  SqCq alloc interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halSqCqInputInfo
* @param [out] *out  See struct halSqCqOutputInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSqCqAllocate(uint32_t devId, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out);

/**
* @ingroup driver
* @brief  SqCq alloc interface
* @attention:
*   1)assure free normalcqsq before free stream_id on 910B&310B
* @param [in] devId: logic devid
* @param [in]  *info   See struct halSqCqFreeInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSqCqFree(uint32_t devId, struct halSqCqFreeInfo *info);

/**
* @ingroup driver
* @brief  SqCq alloc interface
* @attention null
* @param [in] devId: logic devid
* @param [in] info   See struct halSqCqFreeInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSqCqQuery(uint32_t devId, struct halSqCqQueryInfo *info);

/**
* @ingroup driver
* @brief  SqCq config interface
* @attention null
* @param [in] devId: logic devid
* @param [in] info   See struct halSqCqConfigInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSqCqConfig(uint32_t devId, struct halSqCqConfigInfo *info) ASCEND_HAL_WEAK;

struct halSqMemGetInput {
    drvSqCqType_t type;   // 0-normal, 1-callback
    uint32_t tsId;        // ts id, ascend310 : 0, ascend910 : 0
    uint32_t sqId;
    uint32_t cmdCount;    // number of slot[1,1023] which will be alloced
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halSqMemGetOutput {
    uint32_t cmdCount;     // sq cmd slot number alloced actually
    volatile void *cmdPtr; // the first available cmd slot address
    uint32_t pos;          // sqe position
    uint32_t res[SQCQ_RESV_LENGTH - 1];
};

struct halSqMsgInfo {
    drvSqCqType_t type;    // 0-normal, 1-callback
    uint32_t tsId;         // ts id, ascend310 : 0, ascend910 : 0
    uint32_t sqId;
    uint32_t cmdCount;     // sq cmd slot number alloced actually
    uint32_t reportCount;  // cq report count
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halReportInfoInput {
    drvSqCqType_t type;   // 0-normal, 1-callback
    uint32_t grpId;       // runtime thread identifier, normal : 0
    uint32_t tsId;
    int32_t timeout;      // report irq wait time
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halReportInfoOutput {
    uint64_t *cqIdBitmap;    // output of callback module
    uint32_t cqIdBitmapSize; // output of callback module
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halReportGetInput {
    drvSqCqType_t type;  // 0-normal, 1-callback
    uint32_t tsId;
    uint32_t cqId;
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halReportGetOutput {
    uint32_t count;     // cq report count
    void *reportPtr;    // the first available report slot address
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halReportReleaseInfo {
    drvSqCqType_t type;  // 0-normal, 1-callback
    uint32_t tsId;
    uint32_t cqId;
    uint32_t count;
    uint32_t res[SQCQ_RESV_LENGTH];
};

/**
* @ingroup driver
* @brief  sq mem get interface
* @attention null
* @param [in] devId: logic devid
* @param [in] *in See struct halSqMemGetInput
* @param [out] *out See struct halSqMemGetOutput
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSqMemGet(uint32_t devId, struct halSqMemGetInput *in, struct halSqMemGetOutput *out);
/**
* @ingroup driver
* @brief  sq mem send interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *info   See struct halSqMsgInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSqMsgSend(uint32_t devId, struct halSqMsgInfo *info);
/**
* @ingroup driver
* @brief  cq report irq wait interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halReportInfoInput
* @param [out]  *out  See struct halReportInfoInput
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halCqReportIrqWait(uint32_t devId, struct halReportInfoInput *in, struct halReportInfoOutput *out);
/**
* @ingroup driver
* @brief  cq report get interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *in   See struct halReportGetInput
* @param [out]  *out  See struct halReportGetOutput
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halCqReportGet(uint32_t devId, struct halReportGetInput *in, struct halReportGetOutput *out);
/**
* @ingroup driver
* @brief  report release interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *info   See struct halReportReleaseInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halReportRelease(uint32_t devId, struct halReportReleaseInfo *info);

/**
* @ingroup driver
* @brief  sq mem send interface
* @attention null
* @param [in] devId: logic devid
* @param [in]  *info   See struct halTaskSendInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halSqTaskSend(uint32_t devId, struct halTaskSendInfo *info);

/**
* @ingroup driver
* @brief  recv cq report, Replace halCqReportIrqWait + halCqReportGet + halReportRelease.
* @attention null
* @param [in] devId: logic devid
* @param [in]  *info   See struct halReportRecvInfo
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halCqReportRecv(uint32_t devId, struct halReportRecvInfo *info);

/**
 * @ingroup driver
 * @brief  stream task fill interface
 * @attention null
 * @param [in] dev_id:       logic devid
 * @param [in] stream_id:    stream id
 * @param [in]  *stream_mem   stream memory
 * @param [in]  *task_info    task content
 * @param [in]  *task_cnt   task number
 * @return   0 for success, others for fail
 */
drvError_t halStreamTaskFill(uint32_t dev_id, uint32_t stream_id, void *stream_mem, void *task_info, uint32_t task_cnt);

/**
 * @ingroup driver
 * @brief  sq bind stream interface
 * @attention null
 * @param [in] dev_id:       logic devid
 * @param [in] info:    see struct sq_switch_stream_info
 * @param [in] num:     sq and stream number
 * @return   0 for success, others for fail
 */
drvError_t halSqSwitchStreamBatch(uint32_t dev_id, struct sq_switch_stream_info *info, uint32_t num);

/**
* @ingroup driver
* @brief  ACL IO contrl interface
* @attention null
* @param [in] cmd: command
* @param [in]  param_value   param_value addr
* @param [in]  param_value_size   param_value size
* @param [out]  out_value   out_value addr
* @param [out]  out_value_size   out_value size
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halCtl(int cmd, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret);

#define CDQ_NAME_LEN  64
#define CDQ_RESV_LEN 4
struct halCdqPara {
    char queName[CDQ_NAME_LEN];    /* name of cdq, length 32 */
    unsigned int batchNum;              /* number of batch in one CDQ */
    unsigned int batchSize;             /* number of CDE in each batch */
    DVdeviceptr queMemAddr;            /* Memory for create cdq */
    int resv[CDQ_RESV_LEN];
};

/**
* @ingroup driver
* @create circle data queue
* @param [in] devId  create on which device
* @param [in] tsId  tsId
* @param [in] cdqPara  parameter of Circle Data Queue
* @param [out] queId  cdq_Id created successed
* @return   0 for success, DRV_ERROR_NO_CDQ_RESOURCES means device full.
*/
DLLEXPORT drvError_t halCdqCreate(unsigned int devId, unsigned int tsId, struct halCdqPara *cdqPara,
    unsigned int *queId) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @destroy circle data queue
* @param [in] devId  create on which device
* @param [in] tsId  tsId
* @param [in] queId  cdq_Id created successed
* @return   0 for success, DRV_ERROR_CDQ_NOT_EXIST means no such cdq.
*/
DLLEXPORT drvError_t halCdqDestroy(unsigned int devId, unsigned int tsId, unsigned int queId) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @alloc free batch in data queue
* @param [in] devId  create on which device
* @param [in] tsId  tsId
* @param [in] queId  cdq_Id created successed
* @return   0 for success, DRV_ERROR_CDQ_ABNORMAL means cdq has batch timeout, DRV_ERROR_WAIT_TIMEOUT for wait timeout.
*/
DLLEXPORT drvError_t halCdqAllocBatch(unsigned int devId, unsigned int tsId, unsigned int queId, unsigned int timeout,
    unsigned int *batchId) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @get ready batch in a circle data queue
* @param [in] queName name of cdq
* @param [out] ready batchAddr
* @param [out] Size of batch
* @return   0 for success, DRV_ERROR_NO_CDQ_RESOURCES means cdq not ready.
*/
DLLEXPORT drvError_t halCdqGetReadyBatch(
    const char *queName, DVdeviceptr *batchAddr, unsigned int *batchSize) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @after get Ready batch and use data, free used batch in circle data queue
* @param [in] queName name of cdq
* @param [in] Addr of batch to be free
* @return 0 for success, others means fail.
*/
DLLEXPORT drvError_t halCdqFreeBatch(const char *queName, DVdeviceptr batchAddr) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @when CDQ is abnormal, such as can't ready for a long time, set CDQ abnormal status to notify another side
* @param [in] name of timeout error cdq
* @return 0 for success, others means fail.
*/
DLLEXPORT drvError_t halCdqSetAbnormal(const char *queName) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @query CDQ instance by cdqname, instance is globally unique to describe a cdq.
* @param [in] queName name of cdq
* @param [out] instance, qid in low 16 bits, tsid in 16~23bits, devid in 24~31bits
* @return 0 for success, others means fail.
*/
DLLEXPORT drvError_t halCdqGetInstance(const char *queName, unsigned int *instance) ASCEND_HAL_WEAK;

/*=========================== Event Sched ===========================*/
/**
* @ingroup driver
* @brief  dettach one process from a device
* @attention null
* @param [in] devId: logic devid
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedDettachDevice(unsigned int devId) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  create a group for process with specific thread num, and allocate a gid to user.
* @attention A process can create up to 32 groups
* @param [in] devId: logic devid
* @param [in] grpPara: group para info
* @param [out] grpId: group id [32, 63]
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedCreateGrpEx(uint32_t devId, struct esched_grp_para *grpPara, unsigned int *grpId);

/**
* @ingroup driver
* @brief  query esched info, such as grpid.
* @attention null
* @param [in] devId: logic devid
* @param [in] type: query info type
* @param [in] inPut: Input the corresponding data structure based on the type.
* @param [out] outPut: OutPut the corresponding data structure based on the type.
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedQueryInfo(unsigned int devId, ESCHED_QUERY_TYPE type,
    struct esched_input_info *inPut, struct esched_output_info *outPut);

/**
* @ingroup driver
* @brief  Sets the maximum number of events in a group, essentially setting the queue depth of events in a group.
* @attention null
* @param [in] devId: logic devid
* @param [in] grpId: group id
* @param [in] eventId: event id
* @param [in] maxNum: max event num
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedSetGrpEventQos(unsigned int devId, unsigned int grpId,
    EVENT_ID eventId, struct event_sched_grp_qos *qos);

/**
* @ingroup driver
* @brief  set the priority of event
* @attention null
* @param [in] devId: logic devid
* @param [in] priority: event priority
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedSetEventPriority(unsigned int devId, EVENT_ID eventId, SCHEDULE_PRIORITY priority);

/**
* @ingroup driver
* @brief  set the event finish callback func
* @attention null
* @param [in] grpId: group id
* @param [in] eventId: event id
* @param [in] finishFunc: finish callback func
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedRegisterFinishFunc(unsigned int grpId, unsigned int event_id,
    void (*finishFunc)(unsigned int devId, unsigned int grpId, unsigned int event_id, unsigned int subevent_id));

/**
* @ingroup driver
* @brief  Export the latest scheduling trace
* @attention null
* @param [in] devId: logic devid
* @param [in] buff: input buff to store scheduling trace
* @param [in] buffLen: len of the input buff
* @param [out] *dataLen: real length of the scheduling trace
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedDumpEventTrace(unsigned int devId, char *buff,
    unsigned int buffLen, unsigned int *dataLen);

/**
* @ingroup driver
* @brief  trigger to record the latest scheduling trace
* @attention null
* @param [in] devId: logic devid
* @param [in] recordReason: reason to recore the event trace
* @param [in] key: Identifies the uniqueness of the track file
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedTraceRecord(unsigned int devId, const char *recordReason, const char *key);

/**
* @ingroup driver
* @brief  Commit the event to a specific process
* @attention null
* @param [in] devId: logic devid
* @param [in] event: event summary info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedSubmitEvent(unsigned int devId, struct event_summary *event) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  Commit the event to a spcecific thread of a specific process
* @attention null
* @param [in] devId: logic devid
* @param [in] event: event summary info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedSubmitEventToThread(uint32_t devId, struct event_summary *event);

/**
* @ingroup driver
* @brief  Commit the event to a specific process
* @attention null
* @param [in] devId: logic devid
* @param [in] flag: submit flag
* @param [in] events: event summary info
* @param [in] event_num: num of events, max 128 * 1024
* @param [out] succ_event_num: succ num of events
* @return   0 for success with succ event num, others for fail
*/
DLLEXPORT drvError_t halEschedSubmitEventBatch(unsigned int devId, SUBMIT_FLAG flag,
    struct event_summary *events, unsigned int event_num, unsigned int *succ_event_num);

/**
* @ingroup driver
* @brief  Commit and wait the event to a specific process
* @attention null
* @param [in] devId: logic devid
* @param [in] event: event summary info
* @param [in] timeout: timeout value
* @param [out] reply: event reply info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedSubmitEventSync(unsigned int devId,
    struct event_summary *event, int timeout, struct event_reply *reply) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  Swap the thread out from running cpu
* @attention null
* @param [in] devId: logic devid
* @param [in] grpId: group id
* @param [in] threadId: thread id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedThreadSwapout(unsigned int devId, unsigned int grpId, unsigned int threadId);

/**
* @ingroup driver
* @brief  sched thread give up possession of AICPU
* @attention null
* @param [in] devId: logic devid
* @param [in] grpId: group id
* @param [in] threadId: thread id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedThreadGiveup(unsigned int devId, unsigned int grpId, unsigned int threadId);

/**
* @ingroup driver
* @brief  Actively retrieve an event
* @attention null
* @param [in] devId: logic devid
* @param [in] grpId: group id
* @param [in] threadId: thread id
* @param [in] eventId: event id
* @param [out] event: The event that is scheduled
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedGetEvent(unsigned int devId, unsigned int grpId, unsigned int threadId,
    EVENT_ID eventId, struct event_info *event);

/**
* @ingroup driver
* @brief  Respond to events
* @attention null
* @param [in] devId: logic devid
* @param [in] eventId: event id
* @param [in] subeventId: sub event id
* @param [in] msg: message info, it has a specific data format in ascend910B version, please refer to
* the structure hwts_response
* @param [in] msgLen: len of message
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedAckEvent(unsigned int devId, EVENT_ID eventId, unsigned int subeventId,
    char *msg, unsigned int msgLen) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  set the ack event callback func
* @attention null
* @param [in] grpId: group id
* @param [in] eventId: event id
* @param [in] ackFunc: ack event callback func
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedRegisterAckFunc(unsigned int grpId, EVENT_ID eventId,
    void (*ackFunc)(unsigned int devId, unsigned int subevent_id, char *msg, unsigned int msgLen));

/**
* @ingroup driver
* @brief  add a table entry
* @attention null
* @param [in] devId: logic devid
* @param [in] tableId: table id
* @param [in] key: entry key
* @param [in] entry: entry item
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedTableAddEntry(unsigned int devId, unsigned int tableId,
    struct esched_table_key *key, struct esched_table_entry *entry);

/**
* @ingroup driver
* @brief  del a table entry
* @attention null
* @param [in] devId: logic devid
* @param [in] tableId: table id
* @param [in] key: entry key
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedTableDelEntry(unsigned int devId, unsigned int tableId, struct esched_table_key *key);

/**
* @ingroup driver
* @brief  query table entry stat
* @attention null
* @param [in] devId: logic devid
* @param [in] tableId: table id
* @param [in] key: entry key
* @param [out] stat: entry key stat
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedTableQueryEntryStat(unsigned int devId, unsigned int tableId,
    struct esched_table_key *key, struct esched_table_key_entry_stat *stat);

/*=========================== Queue Manage ===========================*/
/**
* @ingroup driver
* @brief  queue subscribe event
* @attention null
* @param [in] subPara: subscribe event structure
* @return   0 for success, DRV_ERROR_REPEATED_SUBSCRIBED for Repeating subscription, others for fail
*/
drvError_t halQueueSubEvent(struct QueueSubPara *subPara);

/**
* @ingroup driver
* @brief  queue unsubscribe event
* @attention null
* @param [in] unsubPara: unsubscribe event structure
* @return   0 for success, others for fail
*/
drvError_t halQueueUnsubEvent(struct QueueUnsubPara *unsubPara);

/**
* @ingroup driver
* @brief  create queue
* @attention For a queue, the producer (enqueue) or consumer (dequeue) only supports single-threaded operations.
* @param [in] devId: logic devid
* @param [in] queAttr: queue attribute
* @param [out] qid: queue id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueCreate(unsigned int devId, const QueueAttr *queAttr, unsigned int *qid) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  grant queue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] pid: pid
* @param [in] attr: queue share attr
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueGrant(unsigned int devId, int qid, int pid, QueueShareAttr attr) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  attach queue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] timeOut: timeOut
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueAttach(unsigned int devId, unsigned int qid, int timeOut) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  enqueue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] mbuf: enqueue mbuf
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueEnQueue(unsigned int devId, unsigned int qid, void *mbuf) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  dequeue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [out] mbuf: dequeue to mbuf
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueDeQueue(unsigned int devId, unsigned int qid, void **mbuf) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  subscribe queue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] groupId: queue group id
* @param [in] type: single or group
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueSubscribe(
    unsigned int devId, unsigned int qid, unsigned int groupId, int type) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  unsubscribe queue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueUnsubscribe(unsigned int devId, unsigned int qid) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  sub full to not full event
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] groupId: queue group id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueSubF2NFEvent(unsigned int devId, unsigned int qid, unsigned int groupId) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  unsub full to not full event
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueUnsubF2NFEvent(unsigned int devId, unsigned int qid) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  get qid by name
* @attention null
* @param [in] devId: logic devid
* @param [in] name: queue name
* @param [out] qid: queue id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueGetQidbyName(unsigned int devId, const char *name, unsigned int *qid) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @Pause or recover Enqueue event in same queue-group
* @param [in] struct QueueSubscriber subscriber: info of pause event subscriber
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueCtrlEvent(struct QueueSubscriber *subscriber, QUE_EVENT_CMD cmdType) ASCEND_HAL_WEAK;
/**
* @ingroup driver
* @brief get len of queue tail data
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] timeout: timeout value
* @param [out] buf_len: len of queue tail data
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueuePeek(
    unsigned int devId, unsigned int qid, uint64_t *buf_len, int timeout) ASCEND_HAL_WEAK;
/**
* @ingroup driver
* @brief get a mbuf that has the same data as the index mbuf in the queue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] flag: reserved parameters, user must set 0
* @param [in] type: the type of queue peek data, if type is QUEUE_PEEK_DATA_COPY_REF, user should actively free mbuf
* @param [out] mbuf: output mbuf has the same data as the index mbuf in the queue
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueuePeekData(unsigned int devId, unsigned int qid, unsigned int flag,
    QueuePeekDataType type, void **mbuf);
/**
* @ingroup driver
* @brief  enqueue buff vector
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] vector: see struct buff_iovec
* @param [in] timeout: timeout value
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueEnQueueBuff(unsigned int devId, unsigned int qid,
    struct buff_iovec *vector, int timeout) ASCEND_HAL_WEAK;
/**
* @ingroup driver
* @brief  dequeue buff vector
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] timeout: timeout value
* @param [out] vector: see struct buff_iovec
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueDeQueueBuff(unsigned int devId, unsigned int qid,
    struct buff_iovec *vector, int timeout) ASCEND_HAL_WEAK;
/**
* @ingroup driver
* @brief  event handle api, aicpu get the EVENT_DRV_MSG event call this function.
* @param [in] devId: logic devid
* @param [in] event: see struct event_info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEventProc(unsigned int devId, struct event_info *event) ASCEND_HAL_WEAK;
/**
* @ingroup driver
* @brief  drv event thread init api, The driver starts a thread on the invoked CPU to wait for the
          EVENT_DRV_MSG event and call halEventProc to process.
* @param [in] devId: logic devid
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halDrvEventThreadInit(unsigned int devId);

/**
* @ingroup driver
* @brief  drv event thread uninit api, see halDrvEventThreadInit.
* @param [in] devId: logic devid
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halDrvEventThreadUninit(unsigned int devId);
/**
* @ingroup driver
* @brief  query queue status
* @Pause or recover Enqueue event in same queue-group
* @param [in] devId: logic devid
* @param [in] cmd: query cmd
* @param [in] inBuff: query cmd
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueQuery(unsigned int devId, QueueQueryCmdType cmd,
    QueueQueryInputPara *inPut, QueueQueryOutputPara *outPut) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  set queue workmode
* @attention null
* @param [in] devId: logic devid
* @param [in] cmd: set cmd type
* @param [in] ipPut: cmd related parameters
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueSet(unsigned int devId, QueueSetCmdType cmd, QueueSetInputPara *input);

/**
* @ingroup driver
* @brief  notificate buffer mode
* @attention null
* @param [in] status: work status, 0:work, 1:no work
* @param [in] rsv: Extended configuration parameter, reserved item, configured according to the scene.
                   This parameter is valid only when work status is no work.
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halBufferModeNotify(PSM_STATUS status, void *rsv) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  notificate queue mode
* @attention null
* @param [in] status: work status, 0:work, 1:no work
* @param [in] rsv: Extended configuration parameter, reserved item, configured according to the scene.
                   This parameter is valid only when work status is no work.
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueModeNotify(PSM_STATUS status, void *rsv) ASCEND_HAL_WEAK;

#define DRV_NOTIFY_TYPE_TOTAL_SIZE 1
#define DRV_NOTIFY_TYPE_NUM 2
/**
 * @ingroup driver
 * @brief get notify num or memory size
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function. Furthermore, this function
 *  can only be called in training scenario by physical machine.
 * @param [in] devId logic_id
 * @param [in] tsId: ascend910 tsid is 0
 * @param [in] type: get notify info type
 * @param [out] val: return corresponding value according to type
 * @return  0  success if chip type is ascend910 , return others fail
 * @return  0xfffe means not supported
 */
drvError_t halNotifyGetInfo(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val);

/**
* @ingroup driver
* @brief Get the number of chips
* @attention NULL
* @param [out] chip_count  The space requested by the user is used to store the number of returned chips
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t halGetChipCount(int *chip_count) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Get the id of all chips
* @attention NULL
* @param [out] count The space requested by the user is used to store the id of all returned chips
* @param [in] count Number of chip equipment
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t  halGetChipList(int chip_list[], int count) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Get the device number of one chip
* @attention NULL
* @param [in] chip_id  The chip id
* @param [out] device_count  The space requested by the user is used to store the number of returned devices
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t  halGetDeviceCountFromChip(int chip_id, int *device_count) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Get the id of all devices of one chip
* @attention NULL
* @param [in] chip_id  The chip id
* @param [out] count The space requested by the user is used to store the id of all returned devices of one chip
* @param [in] count Number of equipment
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t  halGetDeviceFromChip(int chip_id, int device_list[], int count) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Get the id of chip of one device
* @attention NULL
* @param [in] device_id  the device id
* @param [out] the chip id
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t  halGetChipFromDevice(int device_id, int *chip_id) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Get the id of chip of one device
* @attention NULL
* @param [in] devID  the device id
* @param [out] chipInfo chip name/type/version information
* @return  0 for success, others for fail
*/
DLLEXPORT drvError_t halGetChipInfo(unsigned int devId, halChipInfo *chipInfo);

/**
* @ingroup driver
* @brief Converting error code to error message
* @attention NULL
* @param [in] code
* @return 1-8999 user error message
* @return 9000-9999 Undefine error message or inner error message
*/
DLLEXPORT int32_t halMapErrorCode(drvError_t code);

/**
* @ingroup driver
* @brief Get the current number of VF
* @attention null
* @param [out] *num_dev  Number of current VF devices
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetVdevNum(uint32_t *num_dev);

/**
* @ingroup driver
* @brief Get the current number of devices of assigned type
* @attention null
* @param [in]  hw_type 0: davinci, 1: kunpeng
* @param [out] *num_dev  Number of current devices
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetDevNumEx(uint32_t hw_type, uint32_t *devNum);

/**
* @ingroup driver
* @brief The device side and the host side both obtain the host IDs of all the VF.
* @attention null
* @param [in]  len  Array length
* @param [out] devices  device id Array
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetVdevIDs(uint32_t *devices, uint32_t len);

/**
* @ingroup driver
* @brief The device side and the host side both obtain the host IDs of all the current devices.
* If called in a container, get the host IDs of all devices in the current container.
* @attention null
* @param [in]  hw_type 0: davinci, 1: kunpeng
* @param [in]  len  Array length
* @param [out] devices  device id Array
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetDevIDsEx(uint32_t hw_type, uint32_t *devices, uint32_t len);

/**
* @ingroup driver
* @brief Support calling on device and host sides. Currently, not supported called in split mode.
* If filter->filter_flag set to 0, it means do not filter events and get all events.
* @attention If len equal to eventCount, some events may have been abandoned.
* @param [in]  devId the device id
* @param [in]  filter  Filter conditions
* filter->filter_flag; bit0: event_id; bit1: severity; bit2: node_type; bit3: current tgid
* @param [in]  len  the number of eventInfo
* @param [out] eventInfo  Event information
* @param [out] eventCount  Number of events obtained. Max 128
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halGetFaultEvent(uint32_t devId, struct halEventFilter* filter,
    struct halFaultEventInfo* eventInfo, uint32_t len, uint32_t *eventCount);

/**
* @ingroup driver
* @brief Support calling host sides. Currently, not supported called in split mode.
* If filter->filter_flag set to 0, it means do not filter events and get all events.
* @attention A maximum of 64 processes can be invoked. Invoked by multiple threads under a process is not supported.
* @param [in]  devId The logical device id
* @param [in]  timeout Waiting time, unit:ms. Value Range: 0-30000, 0 means no waiting, -1 means never time out
* @param [in]  filter  Filter conditions
* @param [out] eventInfo  Event information
* @return   0 for success, 73 for no event occurred, 0xfffe for not support, others for fail
*/
DLLEXPORT drvError_t halReadFaultEvent(int32_t devId, int timeout,
    struct halEventFilter* filter, struct halFaultEventInfo* eventInfo);


/**
* @ingroup driver
* @brief Subscribe the fault event of device
* @attention NULL
* @param [in]  device_id The logical device id
* @param [in]  filter  Filter conditions
* @param [out] handler  fault event callback func.
* @return   0 for success, 0xfffe for not support, others for fail
*/
DLLEXPORT drvError_t halSubscribeFaultEvent(int device_id, struct halEventFilter filter, halfaulteventcb handler);

struct halSDIDParseInfo {
    unsigned int server_id;
    unsigned int chip_id;
    unsigned int die_id;
    unsigned int udevid;
    unsigned int reserve[8];
};

/**
* @ingroup driver
* @brief get the parsed SDID infomation
* @attention Not supported called in split mode, do not check validity for sdid;
* @param [in]  sdid SDID
* @param [out] sdid_parse  Parsed SDID infomation
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halParseSDID(uint32_t sdid, struct halSDIDParseInfo *sdid_parse);

/**
 * @ingroup driver
 * @brief uadk crypto param
 */
typedef enum {
    CRYPTO_CIPHER_SM4_CBC = 0,     /* SM4_CBC */
    CRYPTO_CIPHER_AES_128_CBC,     /* AES_128_CBC */
    CRYPTO_CIPHER_AES_256_CBC,     /* AES_256_CBC */
    CRYPTO_AEAD_AES_128_GCM = 10,  /* AES_128_GCM */
    CRYPTO_AEAD_AES_256_GCM,       /* AES_256_GCM */
    CIPHER_ALG_BUTT,
} uadk_crypto_algorithm;

typedef struct {
    uadk_crypto_algorithm alg;
    int rsv[4];  /**< rsv[0]: task mode, 0:block mode, 1:stream mode;
                      rsv[1]: wait result mode, 0:loop query, 1:intrrupt nofity; */
} uadk_crypto_param;

/**
 * @ingroup driver
 * @brief uadk key handle
 */
typedef struct {
    int len;
    unsigned char *buff;
} uadk_key_handle;

typedef struct uadk_mem_info {
    unsigned char *src;
    unsigned int src_len;
    unsigned char *dst;
    unsigned int dst_len;
    unsigned char *auth;
    unsigned int auth_len;
    unsigned char *key;
    unsigned int key_len;
    unsigned char *aiv;
    unsigned int aiv_len;
} uadk_mem_info;

/**
 * @ingroup driver
 * @brief uadk crypto init
 * @attention only support ciphertext key, don't support plaintext key
 * @param [out] ctx   crypto context
 * @param [in] param    crypto parameter, including crypto algorithm
 * @param [in] enc  0:encrypt,1:decrypt
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT int uadk_crypto_init(void **ctx, uadk_crypto_param *param, int enc);

/**
 * @ingroup driver
 * @brief alloc mem for ctx.
 * @param [in] ctx   crypto context
 * @param [inout] mem_info    alloc parameter
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT int uadk_crypto_alloc (void *ctx_, uadk_mem_info *mem_info);

/**
 * @ingroup driver
 * @brief update ctx information
 * @param [in] ctx   crypto context
 * @param [in] src_len    srouce length
 * @param [out] dst_len    destination length
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT int uadk_crypto_update(void *ctx_, size_t src_len, size_t *dst_len);

/**
 * @ingroup driver
 * @brief free ctx.
 * @param [in] ctx   crypto context
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT void uadk_crypto_free (void *ctx_);

/**
 * @ingroup driver
 * @brief uadk crypto uninit
 * @param [in] ctx   crypto context
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT void uadk_crypto_ctx_deinit(void *ctx);

/**
* @ingroup driver
* @brief This command is used to get a feature is support or not.
* @attention null
* @param [in] devId Requested input device id.
* @param [in] type  feature type
* @return true : support this feature
* @return false : Don't support this feature
*/
DLLEXPORT bool halSupportFeature(uint32_t devId, drvFeature_t type);

/**
 * @ingroup driver
 * @brief This command is used to get host id.
 * @attention null
 * @param [out] host_id: requested output host id.
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT drvError_t halGetHostID(uint32_t *host_id);

/**
* @ingroup driver
* @brief   interface for obtaining the information about the user configuration area
* @param [in] devId Device ID
* @param [in] name configuration item name
* @param [out] buf Buffer for storing information
* @param [out] bufSize Obtain the information length
* @return   0 for success, others for fail
*/
DLLEXPORT int halGetUserConfig(unsigned int devId, const char *name, unsigned char *buf, unsigned int *bufSize);

/**
* @ingroup driver
* @brief   This interface is used to set the information about the user configuration area.
*          Currently, this interface can be invoked only by the DMP process. In other cases,
*          the permission fails to be returned
* @param [in] devId Device ID
* @param [in] name configuration item name
* @param [in] buf Buffer for storing information
* @param [in] bufSize Obtain the information length
*              Due to the storage space limit, when the configuration area information is set,
*              The length of the setting information needs to be limited.
*              The current length range is as follows: For cloud-related forms,
*              the maximum value of bufSize is 0x8000, that is, 32 KB.
*              For mini-related forms, the maximum value of bufSize is 0x800, that is, 2 KB.
*              If the length is greater than the value of this parameter, a message is displayed,
*              indicating that the setting fails.
* @return   0 for success, others for fail
*/
DLLEXPORT int halSetUserConfig(unsigned int devId, const char *name, unsigned char *buf, unsigned int bufSize);

/**
* @ingroup driver
* @brief   This interface is used to clear the configuration items in the user configuration area.
*          Currently, this interface can be invoked only by the DMP process.
*          In other cases, a permission failure is returned.
* @param [in] devId Device ID
* @param [in] name configuration item name
* @return   0 Success, others for fail
*/
DLLEXPORT int halClearUserConfig(unsigned int devId, const char *name);

/**
* @ingroup driver
* @brief Get the max number of virtual device
* @attention null
* @param [in] devId device id
* @param [out] vf_max_num maximum number of segmentation
* @return 0  success, return others fail
*/
DLLEXPORT int halGetDeviceVfMax(unsigned int devId, unsigned int *vf_max_num);

/**
* @ingroup driver
* @brief Get the list of virtual device IDs
* @attention null
* @param [in] devId device id
* @param [out] vf_list list of vfid
* @param [in] list_len length of vf_list
* @param [out] vf_num number of vf
* @return 0  success, return others fail
*/
DLLEXPORT int halGetDeviceVfList(unsigned int devId, unsigned int *vf_list,
    unsigned int list_len, unsigned int *vf_num);

/**
* @ingroup driver
* @brief record wait event or notify
* @attention only called by cp process
* @param [in] devId device id
* @param [in] tsId tsid
* @param [in] record_type event or notify
* @param [in] record_Id
* @return 0  success, return others fail
*/
DLLEXPORT int halTsDevRecord(unsigned int devId, unsigned int tsId, unsigned int record_type, unsigned int record_Id);

/**
* @ingroup driver
* @brief Initialize Device Memory
* @attention Must have a paired hostpid
* @param [in] hostpid Paired host side pid
* @param [in] vfid Paired device virtual function id
* @param [in] dev_id device id
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : init fail
*/
DLLEXPORT DV_ONLINE DVresult halMemInitSvmDevice(int hostpid, unsigned int vfid, unsigned int dev_id);

#define SVM_MEM_BIND_SVM_GRP           (1U << 0) /* sync svm mem page table */
#define SVM_MEM_BIND_SP_GRP            (1U << 1) /* add to aicpu process share pool group, default with alloc attr */
#define SVM_MEM_BIND_SP_GRP_NO_ALLOC   (1U << 2) /* add to aicpu process share pool group, without alloc attr */

/**
* @ingroup driver
* @brief Bind Device custom-process to aicpu-process
* @attention Must have a paired hostpid and a paired aicpupid
* @param [in] hostpid Paired host side pid
* @param [in] aicpupid Paired aicpu process pid
* @param [in] vfid Paired device virtual function id
* @param [in] dev_id device id
* @param [in] flag mem bind flag
* @return DRV_ERROR_NONE : success
* @return DRV_ERROR_XXX : bind fail
*/
DLLEXPORT DV_ONLINE DVresult halMemBindSibling(int hostPid, int aicpuPid, unsigned int vfid, unsigned int dev_id, unsigned int flag);

/**
 * @ingroup driver
 * @brief backup stream info
 * @attention null
 * @param [in]  dev_id: logic devid id
 * @param [in]  in    : info needs to be saved
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t halStreamBackup(uint32_t dev_id, struct stream_backup_info *in);

/**
 * @ingroup driver
 * @brief restore stream info
 * @attention null
 * @param [in]  dev_id: logic devid id
 * @param [in]  in    : info needs to be restored
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t halStreamRestore(uint32_t dev_id, struct stream_backup_info *in);

enum vmng_split_mode {
    VMNG_NORMAL_NONE_SPLIT_MODE = 0,
    VMNG_VIRTUAL_SPLIT_MODE,
    VMNG_CONTAINER_SPLIT_MODE,
    VMNG_INVALID_SPLIT_MODE
};

/**
 * @ingroup driver
 * @brief Get device split mode
 * @attention null
 * @param [in] dev_id: logic devid id
 * @param [out] mode: split mode
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t halGetDeviceSplitMode(unsigned int dev_id, unsigned int *mode);

#ifdef __cplusplus
}
#endif
#endif

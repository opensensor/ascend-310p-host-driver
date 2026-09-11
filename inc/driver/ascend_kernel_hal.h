/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */
#ifndef __ASCEND_KERNEL_HAL_H__
#define __ASCEND_KERNEL_HAL_H__
#include <linux/types.h>
#include "ascend_hal_define.h"
#define LPM3_IDLE_CMD 9
#define IPC_CMD_DVPP_MIN    241
#define IPC_CMD_DVPP_MAX    249
#define IPC_CMD_RETR_MIN    250
#define IPC_CMD_RETR_MAX    255

#ifndef u64
typedef unsigned long long u64;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u16
typedef unsigned short u16;
#endif

#ifndef u8
typedef unsigned char u8;
#endif
#include "trs_kernel_interface.h"

/**
* @ingroup driver-stub
* @brief interface for dvpp notify LP to adjust ddr frequency
* @param [in]  unsigned int dev_id: device ID
* @param [in]  unsigned char cmd_type0: message target
* @param [in]  unsigned char cmd_type1: module target
* @param [in]  unsigned char *data: message
* @param [in]  unsigned int data_len: message length
* @return   0 for success, others for fail
*/
int hal_kernel_send_ipc_to_lp_async(unsigned int devid, unsigned char cmd_type0,
    unsigned char cmd_type1, unsigned char *data, unsigned int data_len);

/**
* @ingroup driver-stub
* @brief  interface for reporting the in-position information of the optical module
* @param [in]  unsigned int qsfp_index Optical port index
* @param [out] unsigned int *val presence information
* @return   0 for success, others for fail
*/
int drv_cpld_qsfp_present_query(unsigned int qsfp_index, unsigned int *val);
/**
* @ingroup driver-stub
* @brief  obtain the MAC address of the user configuration area.
* @param [in]  unsigned int dev_id Device ID
* @param [in]  unsigned char *buf Buffer for storing information
* @param [in]  unsigned int buf_size Size of the buffer for storing information
* @param [out] unsigned int *info_size Data length of the returned MAC information
* @return   0 for success, others for fail
* @note Support:Ascend910,Ascend910B
*/
int devdrv_config_get_mac_info(unsigned int dev_id,
                               unsigned char *buf,
                               unsigned int buf_size,
                               unsigned int *info_size);
/**
* @ingroup driver-stub
* @brief   interface for obtaining the information about the user configuration area
* @param [in]  unsigned int dev_id Device ID
* @param [in]  const char *name: configuration item name
* @param [in]  unsigned char *buf Buffer for storing information
* @param [out] unsigned int *buf_size Obtain the information length
* @return   0 for success, others for fail
* @note Support:Ascend310,Ascend910,Ascend610,Ascend310P,Ascend910B,Ascend310B
*/

int devdrv_get_user_config(unsigned int dev_id, const char *name, unsigned char *buf, unsigned int *buf_size);
/**
* @ingroup driver-stub
* @brief   This interface is used to set the information about the user configuration area.
*          Currently, this interface can be invoked only by the DMP process. In other cases, the permission fails to be returned
* @param [in]  unsigned int dev_id Device ID
* @param [in]  const char *name: configuration item name
* @param [in]  unsigned char *buf Buffer for storing information
* @param [out] unsigned int *buf_size Obtain the information length
*              Due to the storage space limit, when the configuration area information is set,
*              The length of the setting information needs to be limited.
*              The current length range is as follows: For cloud-related forms,
*              the maximum value of buf_size is 0x8000, that is, 32 KB.
*              For mini-related forms, the maximum value of buf_size is 0x800, that is, 2 KB.
*              If the length is greater than the value of this parameter, a message is displayed,
*              indicating that the setting fails.
* @return   0 for success, others for fail
* @note Support:Ascend310,Ascend910,Ascend610,Ascend310P,Ascend910B,Ascend310B
*/
int devdrv_set_user_config(unsigned int dev_id, const char *name, unsigned char *buf, unsigned int buf_size);
/**
* @ingroup driver-stub
* @brief   This interface is used to clear the configuration items in the user configuration area.
*          Currently, this interface can be invoked only by the DMP process.
*          In other cases, a permission failure is returned.
* @param [in]  unsigned int dev_id Device ID
* @param [in]  const char *name: configuration item name
* @param [in]  unsigned char *buf Buffer for storing information
* @return   0 Success, others for fail
* @note Support:Ascend310,Ascend910,Ascend610,Ascend310P,Ascend910B,Ascend310B
*/
int devdrv_clear_user_config(unsigned int devid, const char *name);

/**
* @ingroup driver-stub
* @brief   This interface is used to get flash item in the user configuration area.
* @param [in]  unsigned int: dev_id Device ID
* @param [in]  const char *: configuration name
* @param [in]  unsigned char *:data buffer for storing information
* @param [in]  unsigned int: data buffer length
* @param [out] unsigned int: data buffer out length
* @return   0 Success, others for fail
* @        -2 item not set
* @note Support:Ascend910,Ascend610,Ascend310P,Ascend910B,Ascend310B
*/
int hal_kernel_get_user_config(unsigned int dev_id, const char *name, unsigned char *data,
    unsigned int in_len, unsigned int *out_len);

/**
* @ingroup driver-stub
* @brief   This interface is used to set flash item in the user configuration area.
* @param [in]  unsigned int: dev_id Device ID
* @param [in]  const char *: configuration name
* @param [in]  unsigned char *:data buffer for storing information
* @param [in]  unsigned int: data buffer length
* @return   0 Success, others for fail
* @        -2 item not set
* @note Support:Ascend910,Ascend610,Ascend310P,Ascend910B,Ascend310B
*/
int hal_kernel_set_user_config(unsigned int dev_id, const char *name, u8 *data, u32 in_len);

typedef enum dms_pg_info_type {
    PG_INFO_TYPE_AIC = 0,
    PG_INFO_TYPE_CPU,
    PG_INFO_TYPE_HBM,
    PG_INFO_TYPE_MATA,
    PG_INFO_TYPE_MAX,
} HAL_DMS_PG_INFO_TYPE;

typedef struct {
    u32 num;
    u32 freq;       /* core working frequency */
    u64 bitmap;     /* 1:good, 0:bad */
    u64 reserved;
} hal_dms_common_pg_info_t;

/**
* @ingroup driver-stub
* @brief   This interface is used to get pg info from dms module.
* @param [in]  unsigned int: dev_id Device ID
* @param [in]  HAL_DMS_PG_INFO_TYPE: pg info type
* @param [out]  unsigned char *: pg info data pointer
* @param [in]  unsigned int : input data size
* @param [out]  unsigned int *: return info size
* @return   0 Success, others for fail
*/
int hal_kernel_dms_get_pg_info(unsigned int dev_id, HAL_DMS_PG_INFO_TYPE info_type,
    char* data, unsigned int size, unsigned int *ret_size);

#define BUFF_SP_NORMAL 0
#define BUFF_SP_HUGEPAGE_PRIOR (1 << 0)
#define BUFF_SP_HUGEPAGE_ONLY (1 << 1)
#define BUFF_SP_DVPP (1 << 2)

/**
* @ingroup driver-stub
* @brief   This interface is used to switch chip id to numa id.
* @param [in]  unsigned int device_id: device id
* @param [in]  unsigned int type: memory type
* @return   success return numa id, fail return fail code
*/
int hal_kernel_numa_get_nid(unsigned int device_id, unsigned int type);

struct buff_proc_free {
    int pid;
} ;

typedef void (*buff_free_ops)(struct buff_proc_free *arg);

/**
* @ingroup driver
* @brief   This interface is used to register func, which call before buff drv free share pool mem
* @param [in]  func: need to call before recycle
* @param [out]  module_idx: to unreg func module idx
* @return   0 Success, others for fail
*/
int hal_kernel_buff_register_proc_free_notifier(buff_free_ops func, unsigned int *module_idx);

/**
* @ingroup driver-stub
* @brief   This interface is used to unregister the func reg by hal_kernel_buff_register_proc_free_notifier
* @param [in] module_idx: module idx, which is returned in reg func
* @return   0 Success, others for fail
*/
int hal_kernel_buff_unregister_proc_free_notifier(unsigned int module_idx);

enum sensorhub_pps_source_type {
    PPS_FROM_XGMAC = 0x0,
    PPS_FROM_CHIP  = 0x1
};

enum sensorhub_ssu_ctrl_type {
    SSU_SW      = 0x0,
    PATH_SW     = 0x1,
    SSU_BUT
};

enum sensorhub_intr_ctrl_type {
    NORMAL1_MASK_CFG = 0x0,
    NORMAL2_MASK_CFG = 0x1,
    ERROR1_MASK_CFG  = 0x2,
    ERROR2_MASK_CFG  = 0x3,
    NORMAL_INT_CLR   = 0x4,
    INT_CTR_BUT
};

enum sensorhub_fsin_cfg_type {
    PPS_THRESHOLD       = 0x0,
    PPS_PW              = 0x1,
    PPS_BIAS_THRESH     = 0x2,
    EXPO_INIT_PRE       = 0x3,
    FSIN_FRAME_RATE     = 0x4,
    FSIN_CLR_TS         = 0x5,
    FSIN_CAM_MAP        = 0x6,
    FSIN_COMP_NUM       = 0x7,
    FSIN_CTRL_BUT
};

enum sensorhub_polarity_cfg_type {
    POLARITY_PPS_CONFIG = 0x0,
    POLARITY_CFG_BUT
};

enum sensorhub_module_type {
    SSU_CTRL_MODULE         = 0x0,
    INTERRUPT_CTRL_MODULE   = 0x1,
    FSIN_CTRL_MODULE        = 0x2,
    IMU_CTRL_MODULE         = 0x3,
    PERI_SUBCTRL_MODULE     = 0x4,
    GPIO_CTRL_MODULE        = 0x5,
    FAULT_CHECK_CFG         = 0x6,
    SSU_STROBE_SET_CFG      = 0x7,
    SSU_CAMERA_CHECK_READ   = 0x8,
    POLARITY_CONFIG_MODULE  = 0x9,
    MODULE_BUT
};

enum sensorhub_fault_check_type {
    EXPO_CHECK_PRE_THRESHOLD    = 0x0,
    EXPO_CHECK_POST_THRESHOLD   = 0x1
};

struct sensorhub_sub_cmd_value_stru {
    unsigned int value;
};

struct sensorhub_fsync_info_stru {
    unsigned int value;
    unsigned int sub_mod_id;
};

struct sensorhub_sub_cmd_info_stru {
    unsigned int value_0;
    unsigned int value_1;
};

struct sensorhub_msg_head_stru {
    enum sensorhub_module_type cmd;
    unsigned int sub_cmd;  /* fsin_cfg_type or ssu_ctrl_type or intr_ctrl_type */
    unsigned int len;
    void *param; /* sub_cmd_value or fsync_info or sub_cmd_info */
};

/**
* @ingroup driver-stub
* @brief   This interface is used to configure sensorhub module..
* @param [in]  hal_kernel_buff_notify_handle handle: notify handle
* @return   0 Success, others for fail
*/
int hal_kernel_sensorhub_set_module(struct sensorhub_msg_head_stru *para_cfg);

#define QOS_CFG_RESERVED_LEN 8
#define QOS_MASTER_BITMAP_LEN 4
#define MAX_QOS_MASTER_NODE_NAME_LEN 256

enum qos_master_type {
    MASTER_DVPP_VENC    = 0,
    MASTER_DVPP_VDEC    = 1,
    MASTER_DVPP_VPC     = 2,
    MASTER_DVPP_JPEGE   = 3,
    MASTER_DVPP_JPEGD   = 4,
    MASTER_ROCE         = 5,
    MASTER_NIC          = 6,
    MASTER_PCIE         = 7,
    MASTER_AICPU        = 8,
    MASTER_AIC_DAT      = 9,
    MASTER_AIC_INS      = 10,
    MASTER_AIV_DAT      = 11,
    MASTER_AIV_INS      = 12,
    MASTER_SDMA         = 13,
    MASTER_STARS        = 14,
    MASTER_ISP_VICAP    = 15,
    MASTER_ISP_VIPROC   = 16,
    MASTER_ISP_VIPE     = 17,
    MASTER_ISP_GDC      = 18,
    MASTER_ISP_VGS      = 19,
    MASTER_USB          = 20,
    MASTER_MATA         = 21,
    MASTER_DMC          = 22,
    MASTER_VDP          = 23,
    MASTER_GPU          = 24,
    MASTER_AUDIO        = 26,
    MASTER_SATA         = 27,
    MASTER_TPU          = 28,
    MASTER_RPU          = 29,
    MASTER_XGMAC        = 30,
    MASTER_ISP_R8       = 31,
    MASTER_ISP_RT       = 32,
    MASTER_UB_MEM       = 33,
    MASTER_UB_IO        = 34,
    MASTER_UB_CCUA      = 35,
    MASTER_UB_CCUM      = 36,
    MASTER_UB_SKYQ      = 37,
    MASTER_SIO_D2D      = 38,
    MASTER_SIO_D2U      = 39,
    MASTER_INVALID
};

struct qos_master_config_type {
    enum qos_master_type type; /* master type */
    unsigned int mpamid; /* mpam id */
    unsigned int qos; /* qos */
    unsigned int pmg; /* pmg */
    unsigned long long bitmap[QOS_MASTER_BITMAP_LEN]; /* max support 64 * 4  */
    unsigned int mode; /* 0 -- regs vaild, 1 -- smmu vaild, 2 -- sqe vaild */
    unsigned int reserved[QOS_CFG_RESERVED_LEN - 1];
};

#define MAX_OTSD_LEVEL 2
struct qos_otsd_config_type {
    enum qos_master_type master;
    unsigned int otsd_mode; /* 0 -- disable otsd limit, 1 -- read & write merge, 2 -- read & write not merge */
    unsigned int otsd_lvl[MAX_OTSD_LEVEL]; /* otsd level */
    int reserved[QOS_CFG_RESERVED_LEN];
    unsigned long long bitmap[4]; /* max support 64 * 4 */
};

#define MAX_QOS_ALLOW_LEVEL 3
struct qos_allow_config_type {
    enum qos_master_type master;
    unsigned int qos_allow_mode;        /* 0 -- disable bp, 1 -- produce bp, 2 -- response bp */
    unsigned int qos_allow_ctrl;        /* 0 -- all, 1 -- read, 2 -- write */
    unsigned int qos_allow_threshold;   /* for media, threshold for generating a count(unit: ns) */
    unsigned int qos_allow_windows;     /* width of the statistics window, (unit: ns) */
    unsigned int qos_allow_saturation;  /* allowable error value of bandwidth limit, (unit: GB/s) */
    unsigned int qos_allow_lvl[MAX_QOS_ALLOW_LEVEL]; /* qos allow level */
    int reserved[QOS_CFG_RESERVED_LEN];
    unsigned long long bitmap[4];       /* max support 64 * 4 */
};

typedef int (*set_qos_cfg)(int dev_id, const struct qos_master_config_type *cfg);
typedef int (*get_qos_cfg)(int dev_id, struct qos_master_config_type *cfg);
typedef int (*set_allow_cfg)(int dev_id, const struct qos_allow_config_type *cfg);
typedef int (*get_allow_cfg)(int dev_id, struct qos_allow_config_type *cfg);
typedef int (*set_otsd_cfg)(int dev_id, const struct qos_otsd_config_type *cfg);
typedef int (*get_otsd_cfg)(int dev_id, struct qos_otsd_config_type *cfg);

struct qos_master_node {
    char name[MAX_QOS_MASTER_NODE_NAME_LEN]; /* master name */
    struct qos_master_config_type cfg; /* master qos config */
    set_qos_cfg set;            /* set qos config handler */
    get_qos_cfg get;            /* get qos config handler */
    set_allow_cfg set_allow;    /* if support cfg qos allow, can't be null */
    get_allow_cfg get_allow;    /* if support cfg qos allow, can't be null */
    set_otsd_cfg set_otsd;      /* if support cfg otsd, can't be null */
    get_otsd_cfg get_otsd;      /* if support cfg otsd, can't be null */
};

#define MAX_QOS_MASTER_NAME_LEN 256
#define MAX_PROFILE_INFO_NUM    20
/*
streamNum:index of mpamId
mode: 0-get list of mpamid with streamName 1-get streamName of given mpamid 2-get mpamid of given streamName
*/
struct QosProfileInfo {
    unsigned int dev_id;
    unsigned short streamNum;
    unsigned short mode;
    unsigned char mpamId[MAX_PROFILE_INFO_NUM];
    char streamName[MAX_QOS_MASTER_NAME_LEN];
};

struct qos_bw_result_t {
    uint32_t mpamid;                    /* target mpamid */
    uint32_t bw_data_new;               /* newest bandwidth */
    uint32_t bw_data_max;               /* max bandwidth in history */
    uint32_t bw_data_min;               /* min bandwidth in history */
    uint32_t bw_data_mean;              /* mean bandwidth */
    int32_t curr_method;                /* bandwidth data source:   0--DHA/MATA monitoring mpamid bandwidth */
                                        /*                          1--DDRC monitoring master id bandwidth */
                                        /*                         -1--bandwidth monitoring is disabled. */
    uint32_t reserved[QOS_CFG_RESERVED_LEN - 1];
};

/**
* @brief register qos master node to ascend qos hal
* @param [in] const struct qos_master_node *master: qos master node info
* @return 0: success, else: fail
*/
int hal_kernel_qos_node_register(const struct qos_master_node *master);

/**
* @brief unregister qos master node from ascend qos hal
* @param [in] const struct qos_master_node *master: qos master node info
* @return 0: success, else: fail
*/
int hal_kernel_qos_node_unregister(const struct qos_master_node *master);

/**
* @ingroup driver
* @brief Set qos config to PCIe registers
* @param [in]  int dev_id: device id
* @param [in]  const struct qos_master_config *cfg: qos config
* @return 0: success, else: fail
*/
int hal_kernel_agentdrv_set_qos_config(int dev_id, const struct qos_master_config_type *cfg);

/**
* @brief notify QoS driver that module has gone online
* @param [in] master: master type
* @return 0: success, else: fail
*/
int hal_kernel_qos_notify_module_online(int dev_id, enum qos_master_type master);

/**
* @brief notify QoS driver that module has gone offline
* @param [in] master: master type
* @return 0: success, else: fail
*/
int hal_kernel_qos_notify_module_offline(int dev_id, enum qos_master_type master);

/**
* @brief get all bandwidth infomation
* @param [in] uint32_t devid: device id
* @param [out] struct qos_bw_result_t *res: bandwidth info of each master
* @param [in] uint32_t in_size: size of res
* @param [out] uint32_t *out_size: num of monitoring mpamid
* @return 0: success, else: fail
*/
int32_t hal_kernel_qos_get_all_bandwidth(uint32_t devid, struct qos_bw_result_t *res, \
                                         uint32_t in_size, uint32_t *out_size);

/**
* @brief get master name of specific mpamid
* @param [in] uint32_t devid: device id
* @param [out] struct QosProfileInfo *res: names of masters which use specific mpamid
* @return 0: success, else: fail
*/
int32_t hal_kernel_qos_get_master_name(uint32_t devid, struct QosProfileInfo *res);

/**
* @ingroup driver
* @brief Get qos config form PCIe registers
* @param [in]  int dev_id: device id
* @param [out] struct qos_master_config_type *cfg: qos config
* @return 0: success, else: fail
*/
int hal_kernel_agentdrv_get_qos_config(int dev_id, struct qos_master_config_type *cfg);

/**************************** event sched table intf start ***********************************/
#define ESCHED_MAX_KEY_LEN 128
#define ESCHED_MAX_ENTRY_NUM (1024 * 1024)

#define ESCHED_MAX_CQE_SIZE ESCHED_MAX_KEY_LEN
#define ESCHED_CQE_SIZE_ALIGN 4
#define ESCHED_MAX_CQ_DEPTH (64 * 1024)

#define ESCHED_ADDR_TYPE_PHY 0
#define ESCHED_ADDR_TYPE_VIR 1
#define ESCHED_ADDR_WIDTH_32_BITS 0
#define ESCHED_ADDR_WIDTH_64_BITS 1
#define ESCHED_ADDR_LITTLE_ENDIAN_TYPE 0
#define ESCHED_ADDR_BIG_ENDIAN_TYPE 1


enum esched_cq_type {
    ESCHED_CQ_TYPE_PHASE,
    ESCHED_CQ_TYPE_PTR,
    ESCHED_CQ_TYPE_MAX
};

struct esched_addr_desc {
    u32 offset; /* start bit of the offset of the element in the register */
    u64 reg_addr;
    u64 mask; /* mask of the element in the register */
};

struct esched_cq_phase_head {
    u32 addr_width; /* 0: 32 bit, 1: 64 bit */
    struct esched_addr_desc head;
    u64 overlay_value; /* when a register is written, the value must be superimposed. */
    u64 phase_mask; /* when the software notifies the hardware, the value of bits in the mask needs to be
                       incremented to notify the hardware when the queue flips. surport 1bit */
    u8 init_value; /* phase value of the first round when the software notifies the hardware */
    u8 ring_step;
};

struct esched_cq_phase {
    u8 *mask; /* len is cqe_size */
    u8 init_value;
    u8 ring_step;
    struct esched_cq_phase_head phase_head;
};

struct esched_cq_ptr {
    u32 addr_width; /* 0: 32 bit, 1: 64 bit */
    struct esched_addr_desc head;
    struct esched_addr_desc tail;
};

struct esched_raw_data_cq {
    u32 type; /* ESCHED_CQ_TYPE_: phase bit check, tail reg check */
    u32 addr_type; /* 0: phy, 1: vir(stream id, substream id) */
    u16 stream_id;
    u16 substream_id;
    u64 cq_addr;
    u32 cq_depth;
    u32 cqe_size;
    union {
        struct esched_cq_phase cq_phase;
        struct esched_cq_ptr cq_ptr;
    };
};

enum esched_raw_data_type {
    RAW_DATA_TYPE_CQ,
    RAW_DATA_TYPE_MAX
};

struct esched_table_raw_data {
    u32 raw_data_type;
    u32 max_entry_num;
    char *name; /* for debug, table name */
    u8 *raw_data_key_mask; /* len is cqe_size */
    u32 raw_data_key_mask_len;
    u32 endian_type; /* 0: little endian, 1: big endian */
    union {
        struct esched_raw_data_cq cq_data;
    };
};

struct esched_hw_info {
    char *name; /* for debug, hw name */
    int irq;
    u32 addr_type; /* 0: phy, 1: vir(stream id, substream id) */
    u32 addr_width; /* 0: 32 bit, 1: 64 bit */
    u32 endian_type; /* 0: little endian, 1: big endian */
    u64 irq_clr_reg; /* If this function is unavailable, set this parameter to 0. */
    u64 val;
};

int hal_esched_alloc_table(u32 dev_id, struct esched_table_raw_data *raw_data, u32 *table_id);
int hal_esched_free_table(u32 dev_id, u32 table_id);
int hal_esched_hw_bind_table(u32 dev_id, struct esched_hw_info *hw_info, u32 table_id);
int hal_esched_hw_unbind_table(u32 dev_id, struct esched_hw_info *hw_info);
/**************************** event sched table intf end ***********************************/
/********************************* svm kernel_api start ***********************************************/
struct svm_dma_desc_addr_info {
    u64 src_va;
    u64 dst_va;
    u64 size;
};

struct svm_dma_desc_handle {
    int pid;
    u32 key;
    u32 subkey;
};

struct svm_dma_desc {
    void *sq_addr;
    u32 sq_tail;    /* means the cnt of sq_addr */
};

/* if subkey==SVM_DMA_DESC_INVALID_SUB_KEY, means destroy all nodes related to key */
#define SVM_DMA_DESC_INVALID_SUB_KEY 0xFFFFFFFFu

/* create might sleep, do not call in irq or spinlock */
int hal_kernel_svm_dma_desc_create(struct svm_dma_desc_addr_info *addr_info,
    struct svm_dma_desc_handle *handle, struct svm_dma_desc *dma_desc);
void hal_kernel_svm_dma_desc_destroy(struct svm_dma_desc_handle *handle);

#ifndef CFG_ENABLE_ASAN
/* orther addr not test, dev mmap fail because program segments loading random */
#define SVM_READ_ONLY_ADDR_START (0x100000000000ULL + 0x20000000000Ul)
#define SVM_READ_ONLY_ADDR_END (0x100000000000ULL + 0x20000000000U + 0x4000000000Ul - 1)
#else
/* device asan 0x100000000000ULL mmap fail */
#define SVM_READ_ONLY_ADDR_START (0x210000000000ULL + 0x20000000000Ul)
#define SVM_READ_ONLY_ADDR_END (0x210000000000ULL + 0x20000000000U + 0x4000000000Ul - 1)
#endif

static inline int hal_kernel_svm_addr_is_read_only(u64 addr, u64 len)
{
    return (int)((addr >= SVM_READ_ONLY_ADDR_START) && (addr <= SVM_READ_ONLY_ADDR_END) &&
        ((addr + len) <= SVM_READ_ONLY_ADDR_END) &&
        (len <= (SVM_READ_ONLY_ADDR_END - SVM_READ_ONLY_ADDR_START)));
}

#define DEVMM_MEM_ATTR_READONLY 0U
#define DEVMM_MEM_ATTR_DVPP     1U
#define DEVMM_MEM_ATTR_DEV      2U
#define DEVMM_MEM_ATTR_TYPE_MAX 3U

struct p2p_page_info {
    u64 pa;
    u64 reserved[4];
};

#define P2P_GET_PAGE_VERSION 0x1
struct p2p_page_table {
    u32 version;
    u64 page_size; /* For giant pages, returns HPAGE_SIZE */
    struct p2p_page_info *pages_info;
    u64 page_num;
    u64 reserved[4];
};

/**
* @ingroup driver
* @brief   This interface is used to query the memory address of npu
* @attention
* 1. The calling interface process needs to have the same context as the va process, might sleep.
* 2. Only supports va corresponding to real device physical memory, So memory with special attributes is not supported.
     For example, ipc open va, vmm import va, read-only va, remote map va...
* 3. Must be matched with the calling interface hal_kernel_p2p_put_pages, otherwise page_table memory leaks will occur.
* 4. Once the free_callback call is completed, must be ensured that the memory corresponding to va is no longer used.
     Otherwise, the caller is responsible for problems such as using the memory UAF and stepping on the memory.
* 5. The caller needs to avoid user space being able to control the interface to be called without restrictions.
* 6. hal_kernel_p2p_put_pages cannot be called in free_callback or free_callback cannot wait for hal_kernel_p2p_put_pages
     to complete before returning, otherwise there is a risk of deadlock.
* @param [in]  va: svm addr range, need page_size aligned
* @param [in]  len: va length, need page_size aligned
* @param [in]  free_callback: Called when the queried memory is released and free_callback cannot be empty.
* @param [in]  data: parameters of free_callback
* @param [out]  page_table: Memory related query results, include page_size, pa, page_num...
* @return   0 Success, others for fail
* @note Support: ascend310P/ascend910 computing power grouping scenarios is not guaranteed to be supported.
*/
int hal_kernel_p2p_get_pages(u64 va, u64 len, void (*free_callback)(void *data), void *data,
    struct p2p_page_table **page_table);
/**
* @ingroup driver
* @brief   This interface is used to release related resources in the hal_kernel_p2p_get_pages interface
* @attention must be matched with the calling interface hal_kernel_p2p_get_pages, might sleep.
* @param [in]  page_table: The pointer returned by the hal_kernel_p2p_get_pages interface
* @return   0 Success, others for fail
* @note Support: Same as hal_kernel_p2p_get_pages
*/
int hal_kernel_p2p_put_pages(struct p2p_page_table *page_table);

int hal_kernel_svm_check_mem_attribute(int devpid, u64 va, u64 size, u32 attribute);

/********************************* svm kernel_api end ***********************************************/

/**
* @ingroup driver
* @brief   This interface is used to init bootdot block
* @param [in]  block_id: block id
* @param [in]  magic: magic
* @param [in]  execption_id: execption id
* @param [in]  expect_status: expect status
* @return   0 Success, others for fail
* @note Support: Ascend310Brc
*/
int bootdot_init_blk(u32 block_id, u32 magic, u32 execption_id, u32 expect_status);

/**
* @ingroup driver
* @brief   This interface is used to set bootdot block
* @param [in]  block_id: block id
* @param [in]  magic: magic
* @param [in]  current_status: current status
* @return   0 Success, others for fail
* @note Support: Ascend310Brc
*/
int bootdot_set_blk(u32 block_id, u32 magic, u32 current_status);

#define DRV_HW_INFO_RESERVED_LEN 4

typedef struct devdrv_base_hw_info {
    u8 chip_id;
    u8 multi_chip;  /* multi-chip or single-chip */
    u8 multi_die;   /* multi-die or single-die */
    u8 mainboard_id;
    u16 addr_mode;  /* host and device addressing mode. 0: independent, 1: unified */
    u16 board_id;

    u8 version; /* data version */
    u8 inter_connect_type;  /* connect mode. 0: pcie, 1: hccs */
    u16 hccs_hpcs_bitmap;   /* hccs lane info */

    u16 server_id;  /* super pod server ID */
    u16 scale_type; /* super pod scale type */
    u32 super_pod_id;   /* super pod ID */

    u8 reserved2[DRV_HW_INFO_RESERVED_LEN];
} devdrv_base_hw_info_t;

typedef struct devdrv_hardware_info {
    unsigned long long phy_addr_offset;
    devdrv_base_hw_info_t base_hw_info;
} devdrv_hardware_info_t;

/**
* @ingroup driver
* @brief   This interface is used to get hardware information, only called in device side.
* @param [in]  phy_id : Physical device id
* @param [out]  hardware_info: Hardware information
* @return   0 Success, others for fail
*/
int hal_kernel_get_hardware_info(unsigned int phy_id, devdrv_hardware_info_t *hardware_info);

/**
* @ingroup driver
* @brief   This interface is used to get physical base address, only called in device side.
* @param [in]  phy_id : Physical device id
* @param [in]  offset : address
* @return   address containing the offset of the physical base address, ULLONG_MAX if fail
*/
unsigned long long hal_kernel_get_dev_phy_base_addr(unsigned int phy_id, unsigned long long offset);

/**
* @ingroup driver
* @brief   This interface is used to get soc type
* @param [in]  dev_id : device id
* @param [out]  soc_type: soc type, use value in enum HAL_KERNEL_SOC_TYPE
* @return   0 Success, others for fail
*/
typedef enum {
    SOC_TYPE_MINI = 0,          /* Ascend310 */
    SOC_TYPE_CLOUD,             /* Ascend910 */
    SOC_TYPE_MDC,               /* Ascend610 */
    SOC_TYPE_LHISI,             /* Hi3796CV300ES & TsnsE, Hi3796CV300CS & OPTG & SD3403 &TsnsC */
    SOC_TYPE_DC,                /* Ascend310P */
    SOC_TYPE_CLOUD_V2,          /* Ascend910A2 & Ascend910A3 */
    SOC_TYPE_RESERVED,          /* Reserved */
    SOC_TYPE_MINI_V3,           /* Ascend310B */
    SOC_TYPE_TINY_V1,           /* Tiny */
    SOC_TYPE_NANO_V1,           /* Nano */
    SOC_TYPE_KUNPENG_V1,        /* KUNPENG */
    SOC_TYPE_AS31XM1,           /* AS31XM1X */
    SOC_TYPE_610LITE,           /* 610lite */
    SOC_TYPE_CLOUD_V3,          /* Ascend910A3 */
    SOC_TYPE_BS9SX1A,           /* BS9SX1A */
    SOC_TYPE_MAX
} HAL_KERNEL_SOC_TYPE;
int hal_kernel_get_soc_type(unsigned int dev_id, unsigned int *soc_type);

struct prof_kernel_sample_start_para {
    unsigned int dev_id;
    unsigned int chan_id;
    unsigned int sub_chan_id;
    int target_pid;
    void *user_data;                /* sample 配置信息 */
    unsigned int user_data_len;     /* sample 配置信息数据长度 */
};

enum prof_kernel_sample_data_mode {
    SAMPLE_PURE_DATA_MODE,            /* 非首次采集，只需上报数据 */
    SAMPLE_DATA_WITH_HEADER_MODE      /* 首次采集，部分通道需要装填数据描述头 */
};

struct prof_kernel_sample_para {
    unsigned int dev_id;
    unsigned int chan_id;
    unsigned int sub_chan_id;
    enum prof_kernel_sample_data_mode sample_mode;
    void *buff;                     /* sample buff地址 */
    unsigned int buff_len;          /* sample buff总长度 */
    unsigned int report_len;        /* 返回值：实际上报数据量 */
};

struct prof_kernel_sample_flush_para {
    unsigned int dev_id;
    unsigned int chan_id;
    unsigned int sub_chan_id;
};

struct prof_kernel_sample_stop_para {
    unsigned int dev_id;
    unsigned int chan_id;
    unsigned int sub_chan_id;
};

struct prof_kernel_sample_ops {
    int (*start_func)(struct prof_kernel_sample_start_para *para);
    int (*sample_func)(struct prof_kernel_sample_para *para);            /* NULL: sampler_period must equals to 0 */
    int (*flush_func)(struct prof_kernel_sample_flush_para *para);       /* not must */
    int (*stop_func)(struct prof_kernel_sample_stop_para *para);
};

struct prof_kernel_sample_register_para {
    struct module *owner;                   /* THIS_MODULE */
    unsigned int sub_chan_num;              /* 多实例 */
    struct prof_kernel_sample_ops ops;
};

#define MAX_RECORD_PA_NUM_PER_DEV    20U

struct va_info {
    unsigned short size;
    unsigned char reserved[6];
    unsigned long long va_addr;
};

struct hbm_pa_va_info {
    unsigned int dev_id;
    unsigned int host_pid;
    unsigned int va_num;
    struct va_info va_info[MAX_RECORD_PA_NUM_PER_DEV];
};

struct memory_fault_timestamp {
    unsigned int dev_id;
    unsigned int host_pid;
    unsigned int event_id;
    unsigned int reserved; /* for byte alignment */
    unsigned long long syscnt; /* event occurr syscnt*/
};

/**
 * @ingroup driver
 * @brief register prof channel smaple handle
 * @attention null
 * @param [in] dev_id chan_id para
 * @return   DRV_ERROR_NONE   success
 * @return   other  fail
 */
int hal_kernel_prof_sample_register(unsigned int dev_id, unsigned int chan_id,
    struct prof_kernel_sample_register_para *para);

/**
 * @ingroup driver
 * @brief unregister prof channel smaple handle
 * @attention null
 * @param [in] dev_id chan_id
 * @return: no ret_val
 */
void hal_kernel_prof_sample_unregister(unsigned int dev_id, unsigned int chan_id);

struct prof_kernel_data_report_para {
    void *data;
    unsigned int data_len;
};

int hal_kernel_prof_sample_data_report(unsigned int dev_id, unsigned int chan_id, unsigned int sub_chan_id,
    struct prof_kernel_data_report_para *para);

enum devdrv_func_mbox_cmd_type {
    CALC_CQSQ_CREATE = 0x1,
    CALC_CQSQ_RELEASE,
    LOG_CQSQ_CREATE,
    LOG_CQSQ_RELEASE,
    DEBUG_CQSQ_CREATE,
    DEBUG_CQSQ_RELEASE,
    PROFILE_CQSQ_CREATE,
    PROFILE_CQSQ_RELEASE,
    FUNC_MBOX_CMD_TYPE_MAX
};

struct devdrv_func_sqcq_alloc_para_in {
    enum devdrv_func_mbox_cmd_type type;
    u32 sqe_size;
    u32 cqe_size;
    void (*callback)(u32 devid, u32 tsid, const u8 *cqe, u8 *sqe);
};

struct devdrv_func_sqcq_alloc_para_out {
    u32 sq_id;
    u32 cq_id;
};

struct devdrv_func_sqcq_free_para {
    enum devdrv_func_mbox_cmd_type type;
    u32 sq_id;
    u32 cq_id;
};

/**
* @devdrv create functional sqcq
* @param [in]     devid: device id
* @param [in]      tsid: tsid
* @param [in]   para_in: See struct devdrv_func_sqcq_alloc_para_in
* @param [out] para_out: See struct devdrv_func_sqcq_alloc_para_out
* @return 0: success, else: fail
*/
int devdrv_create_functional_sqcq(u32 devid, u32 tsid, struct devdrv_func_sqcq_alloc_para_in *para_in,
    struct devdrv_func_sqcq_alloc_para_out *para_out);
/**
* @devdrv destroy functional sqcq
* @param [in] devid: device id
* @param [in]  tsid: tsid
* @param [in]  para: See struct devdrv_func_sqcq_free_para
* @return 0: success, else: fail
*/
int devdrv_destroy_functional_sqcq(u32 devid, u32 tsid, struct devdrv_func_sqcq_free_para *para);
/**
* @devdrv functional sq send
* @param [in]    devid: device id
* @param [in]     tsid: tsid
* @param [in]     sqid: sqid
* @param [in]      sqe: the ptr of sqe
* @param [in] sqe_size: the size of sqe
* @return 0: success, else: fail
*/
int devdrv_functional_sq_send(u32 devid, u32 tsid, u32 sqid, const u8 *sqe, u32 sqe_size);

/**
* @devdrv get ssid
* @param [in] devid: device id
* @param [in]  tsid: tsid
* @param [in]   pid: the pid of process
* @param [out] ssid: the ptr of ssid
* @return 0: success, else: fail
*/
int devdrv_get_ssid(u32 devid, u32 tsid, int pid, u32 *ssid);

/**
* @devdrv Get device chip id and die id
* @param [in] dev_id: device id
* @param [out] chip_id: device chip id
* @param [out] die_id: device die id
* @return 0: success, else: fail
*/
int hal_kernel_get_device_chip_die_id(u32 dev_id, u32 *chip_id, u32 *die_id);

/**
* @devdrv Get device memory address mode, unified or ununified
*  Indicates whether the physical addresses of devices are orchestrated in a unified manner.
* @param [in] dev_id: device id
* @param [out] addr_mode: device memory address mode
* @return 0: success, else: fail
*/
typedef enum {
    ADDR_INDEPENDENT = 0,
    ADDR_UNIFIED,
    ADDR_MODE_MAX
} HAL_KERNEL_ADDR_MODE;
int hal_kernel_get_device_addr_mode(u32 dev_id, HAL_KERNEL_ADDR_MODE *addr_mode);

/**
* @devdrv Get device to device topology type
* @param [in] dev_id1: device id 1 for get topology
* @param [in] dev_id2: device id 2 for get topology
* @param [out] topology_type: topology_type
* @return 0: success, else: fail
* @note 1. dev_id1 and dev_id2 not support to input virtual device id
* @note 2. if dev_id1 and dev_id2 are valid and the same, return TOPOLOGY_TYPE_HCCS_SW for Ascend910A3,
*          others return TOPOLOGY_TYPE_HCCS.
*/
typedef enum {
    TOPOLOGY_TYPE_HCCS = 0,
    TOPOLOGY_TYPE_PIX,
    TOPOLOGY_TYPE_PIB,
    TOPOLOGY_TYPE_PHB,
    TOPOLOGY_TYPE_SYS,
    TOPOLOGY_TYPE_SIO,
    TOPOLOGY_TYPE_HCCS_SW,
    TOPOLOGY_TYPE_MAX
} HAL_KERNEL_TOPOLOGY_TYPE;
int hal_kernel_get_d2d_topology_type(u32 dev_id1, u32 dev_id2, HAL_KERNEL_TOPOLOGY_TYPE *topology_type);

/**
* @devdrv Get the state of the remote device as seen by the local device
* @param [in] dev_id: local device id
* @param [in] sdid: remote device's sdid
* @param [out] status: status of remote device
* @return 0: success
* @return -EINVAL: devid is invalid
* @return -EFAULT: status is NULL
* @return -ERANGE: sdid is invalid
* @return -ENODEV: get local device's soc type failed
* @return -EOPNOTSUPP: local device's soc type is 1971 or local device is virtual device
* @return -ENODATA: remote device status array is not initialized
* @note 1. Regardless of whether the remote device exists, as long as the sdid is valid, its status can be gotten. The status defaults to NORMAL.
* @note 2. After the local device is reset, the status information of the remote device will be lost and revert to the default value NORMAL.
*/
typedef enum {
    DMS_SPOD_NODE_STATUS_NORMAL = 0,
    DMS_SPOD_NODE_STATUS_ABNORMAL,
    DMS_SPOD_NODE_STATUS_MAX
} HAL_KERNEL_SPOD_NODE_STATUS;
int hal_kernel_get_spod_node_status(u32 dev_id, u32 sdid, u32 *status);

/**
 * @ingroup driver
 * @brief report driver kernel software failure.
 * @attention This interface will report important alarms, please call it with caution and confirm clearly before calling.
 * @param [in] dev_id
 * @return   no ret_val
 */
#define DRV_SOFT_FAULT_REPORT_ALL_DEV 0xFFFF
void hal_kernel_drv_soft_fault_report(u32 devid);

#endif

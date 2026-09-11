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

#ifndef __DEVDRV_MANAGER_COMM_H
#define __DEVDRV_MANAGER_COMM_H
#include <linux/export.h>

#ifndef AOS_LLVM_BUILD
#include <linux/nsproxy.h>
#ifndef pid_t
typedef int pid_t;
#endif
#endif

#if ((defined CFG_BUILD_DEBUG) && (!defined EXPORT_SYMBOL_UNRELEASE))
#define EXPORT_SYMBOL_UNRELEASE(symbol) EXPORT_SYMBOL(symbol)
#elif (!defined EXPORT_SYMBOL_UNRELEASE)
#define EXPORT_SYMBOL_UNRELEASE(symbol)
#endif

#define MAX_DOCKER_NUM 128U /* equal to max device num */
#define MAX_AICPU_CORE_NUM 32U

enum devdrv_process_type {
    DEVDRV_PROCESS_CP1 = 0,   /* aicpu_scheduler */
    DEVDRV_PROCESS_CP2,       /* custom_process */
    DEVDRV_PROCESS_DEV_ONLY,  /* TDT */
    DEVDRV_PROCESS_QS,        /* queue_scheduler */
    DEVDRV_PROCESS_HCCP,      /* hccp server */
    DEVDRV_PROCESS_USER,      /* user proc, can bind many on host or device. */
    DEVDRV_PROCESS_CPTYPE_MAX
};

typedef struct {
    unsigned int vaild_num;
    unsigned int chip_id[DEVDRV_PROCESS_CPTYPE_MAX];
    unsigned int vfid[DEVDRV_PROCESS_CPTYPE_MAX];
    pid_t host_pids[DEVDRV_PROCESS_CPTYPE_MAX];
    unsigned int cp_type[DEVDRV_PROCESS_CPTYPE_MAX];
} devdrv_host_pids_info_t;


struct host_pid_info {
    pid_t host_pid;
    unsigned int dev_id;
    unsigned int vfid;
    enum devdrv_process_type cp_type;
};

struct dev_pid_info {
    unsigned int dev_id;
    unsigned int vfid;
    enum devdrv_process_type cp_type;
};

enum devdrv_ts_access_mem_type {
    DEVDRV_TS_NODE_DDR_MEM = 0,
    DEVDRV_DDR_MEM,
    DEVDRV_HBM_MEM,
    DEVDRV_P2P_HBM_MEM,
    DEVDRV_MEM_TYPE_MAX
};

struct dmanage_pcie_id_info {
    unsigned int venderid;    /* vendor id */
    unsigned int subvenderid; /* sub vendor id */
    unsigned int deviceid;
    unsigned int subdeviceid;
    unsigned int bus;         /* bus id */
    unsigned int device;      /* physical id of device */
    unsigned int fn;          /* function id of device */
    unsigned int davinci_id;  /* logical id */
};

typedef enum {
    POWER_STATE_SUSPEND,
    POWER_STATE_POWEROFF,
    POWER_STATE_RESET,
    POWER_STATE_BIST,
    POWER_STATE_MAX
} DSMI_POWER_STATE;

typedef enum {
    POWER_RESUME_MODE_BUTTON,   /* resume by button */
    POWER_RESUME_MODE_TIME,     /* resume by time */
    POWER_RESUME_MODE_TIME_POWEROFF, /* poweroff by time */
    POWER_RESUME_MODE_MAX,
} DSMI_LP_RESUME_MODE;

#define POWER_INFO_RESERVE_LEN 8
struct drvdev_power_state_info_stru {
    DSMI_POWER_STATE type;
    DSMI_LP_RESUME_MODE mode;
    unsigned int value;
    unsigned int reserve[POWER_INFO_RESERVE_LEN];
};

struct drvdev_power_state_info {
    unsigned int dev_id;
    struct drvdev_power_state_info_stru power_info;
};

typedef enum {
    POWER_STATE_MODULE_SILS,
    POWER_STATE_MODULE_LPM,
    POWER_STATE_MODULE_MAX
} POWER_STATE_MODULE_TYPE;
struct devdrv_power_handle {
    POWER_STATE_MODULE_TYPE module;
    int (*func)(struct drvdev_power_state_info *info);
};

struct dsmi_dtm_node_s {
    int node_type;
    int node_id;
    unsigned int reserve[POWER_INFO_RESERVE_LEN];
};

typedef enum {
    FREQSCALING = 0,
    POWERGATINGEN,
    CLOCKGATINGEN,
    RESETEN,
    DEVICE_STATUS,
    MAXDEVOPSIDNUMS = 0xff,
} DEV_POWER_OP_TYPE;
 
typedef union {
    unsigned int level;
    int flag;
} DEV_POWER_OP_VAL;
 
typedef struct {
    DEV_POWER_OP_TYPE op_type;
    DEV_POWER_OP_VAL op_val;
} DEVPOWER_OP;

typedef enum {
    DEVMM_SET_VDEV_CONVERT_LEN = 0,
    DEVMM_GET_VDEV_CONVERT_LEN,
    DMS_DEV_INFO_TYPE_MAX
} DMS_DEV_INFO_TYPE;

typedef enum {
    RST_REASON_COLD = 0x1,
    RST_REASON_DSMI_CMD = 0x2,
    RST_REASON_REBOOT_CMD = 0x3,
    RST_REASON_PANIC = 0x4,
    RST_REASON_UTIMATE_WDG = 0x5, /* SafetyIsland ultimate wdog. (int_wsx_wdog[x]) */
    RST_REASON_BIOS_EXCEPTION = 0x6,
    RST_REASON_WDG = 0x7, /* sils_wdg lp_wdg ddr_wdg */
    RST_REASON_EXTERNAL_PIN = 0x8, /* pulling down external pins. (M2_PERST_N and RSTIN_N) */
    RST_REASON_PCIE_RST = 0x9, /* PCIE reset */
    RST_REASON_SOFT_RST = 0xa, /* soft reset except SafetyIsland wdog */
    RST_REASON_SUSPEND_FAIL = 0xb, /* suspend fail reset */
    RST_REASON_RESUME_FAIL = 0xc, /* resume fail reset */
    RST_REASON_NO_COMMIT = 0xd, /* no commit exception reset */
    RST_REASON_NOCMT_TIMEOUT = 0xf, /* safetyisland no commit timeout reset */
    RST_REASON_UNKNOWN = 0xff
} DMS_RST_REASON_VAL;

typedef enum {
    HW_RST_REASON_COLD = 0x1,
    HW_RST_REASON_SOFT,
    HW_RST_REASON_LP_WDOG,
    HW_RST_REASON_DDR_WDOG,
    HW_RST_REASON_PCIE_HOT,
    HW_RST_REASON_RSTN_IN,
    HW_RST_REASON_PCIE_PAD,
    HW_RST_REASON_BUTT,
} HW_RST_REASON_INFO;

typedef enum {
    HW_RST_REASON = 0,
    SW_HIS_RST_REASON_1 = 1,
    SW_HIS_RST_REASON_2 = 2,
    RST_REASON_BUTT,
} DMS_RST_REASON_TYPE;

struct dms_reboot_reason {
    unsigned int reason;
    unsigned int sub_reason; /* reserve */
};

struct dms_reason_priority {
    unsigned int type  :16;  /* 0:(SC_RST_SRC 0x6700); 1:L3SRAM_ADDR1; 2:L3SRAM_ADDR2 */
    unsigned int reason :16;
    unsigned int mask   :16;
    unsigned int val   :16;
};

typedef struct devdrv_pid_map_info {
    pid_t master_pid;
    pid_t slave_pid;
    unsigned int dev_id;
    unsigned int vf_id;
    unsigned int cp_type;
} devdrv_pid_map_info_t;

#ifdef CFG_SOC_PLATFORM_MDC_V11

#define HW_RST_REASON_MASK_V11  0x7
/* L3SRAM base address */
#define DMS_RESET_REASON_L3SRAM_ADDR_BASE (0x90317600U)
/* L3SRAM address for recording hw reset reason register.(SC_RST_SRC 0x6700) */
#define RST_REASON_HW_L3SRAM_OFFSET       (0x0U)
/* L3SRAM address (current area) for recording the soc reset reason */
#define RST_REASON_CUR_L3SRAM_OFFSET1     (0x4U)
#define RST_REASON_CUR_L3SRAM_OFFSET2     (0x8U)
/* L3SRAM address (history area) for recording the soc reset reason */
#define RST_REASON_HIS_L3SRAM_OFFSET1     (0x14U)
#define RST_REASON_HIS_L3SRAM_OFFSET2     (0x18U)

#elif defined(CFG_SOC_MDC_V51_LITE)
/* L3SRAM base address */
#define DMS_RESET_REASON_L3SRAM_ADDR_BASE (0x87237000U)
/* L3SRAM address for recording hw reset reason register.(SC_RST_SRC 0x6700) */
#define RST_REASON_HW_L3SRAM_OFFSET       (0x24U)
/* L3SRAM address (current area) for recording the soc reset reason */
#define RST_REASON_CUR_L3SRAM_OFFSET1     (0x2CU)
#define RST_REASON_CUR_L3SRAM_OFFSET2     (0x30U)
/* L3SRAM address (history area) for recording the soc reset reason */
#define RST_REASON_HIS_L3SRAM_OFFSET1     (0x3CU)
#define RST_REASON_HIS_L3SRAM_OFFSET2     (0x40U)
#else
/* L3SRAM base address */
#define DMS_RESET_REASON_L3SRAM_ADDR_BASE (0xC6F37000U)
/* L3SRAM address for recording hw reset reason register.(SC_RST_SRC 0x6700) */
#define RST_REASON_HW_L3SRAM_OFFSET       (0x24U)
/* L3SRAM address (current area) for recording the soc reset reason */
#define RST_REASON_CUR_L3SRAM_OFFSET1     (0x30U)
#define RST_REASON_CUR_L3SRAM_OFFSET2     (0x34U)
/* L3SRAM address (history area) for recording the soc reset reason */
#define RST_REASON_HIS_L3SRAM_OFFSET1     (0x40U)
#define RST_REASON_HIS_L3SRAM_OFFSET2     (0x44U)
#endif

#define DMS_RESET_REASON_SIZE             (128U)
/* reset reason hw reset reason register */
#define RST_REASON_SOFT_RST_BIT         (0x1U)         /* soft reset except SafetyIsland ultimate wdog */
#define RST_DDR_R_WDG_BIT               (0x1U << 1U)   /* DDR right UCE wdog */
#define RST_DDR_L_WDG_BIT               (0x1U << 2U)   /* DDR lift UCE wdog */
#define RST_LP_WDG_BIT                  (0x1U << 3U)   /* LP wdog */
#define RST_SILS_WDG_BIT                (0x1U << 4U)   /* SafetyIsland wdog */
#define RST_REASON_WDG_BIT   (RST_DDR_R_WDG_BIT | RST_DDR_L_WDG_BIT | RST_LP_WDG_BIT | RST_SILS_WDG_BIT) /* wdog */
#define RST_REASON_PERST_N_BIT          (0x1U << 5U)   /* M2_PERST_N */
#define RST_REASON_RSTIN_N_BIT          (0x1U << 6U)   /* RSTIN_N */
#define RST_REASON_PCIE_BIT             (0x1U << 7U)   /* PCIE reset */
#define RST_REASON_COLD_RST_BIT         (0x1U << 8U)   /* power off */

/* reset reason l3sram1 */
#define RST_REASON_BIT_BIOS             (0x1U)         /* BIOS exception */
#define RST_REASON_BIT_DSMI_CMD         (0x1U << 1U)   /* dsmi command */
#define RST_REASON_BIT_REBOOT_CMD       (0x1U << 2U)   /* reboot command */
#define RST_REASON_BIT_PANIC            (0x1U << 3U)   /* os panic */
#define RST_REASON_BIT_NO_COMMIT        (0x1U << 4U)   /* no commit exception */

/* reset reason l3sram2 */
#define RST_REASON_BIT_UTIMATE_WDG      (0x1U)         /* SafetyIsland ultimate wdog (int_wsx_wdog[x]) */
#define RST_REASON_BIT_SUSPEND_FAIL     (0x1U << 1U)   /* suspend fail */
#define RST_REASON_BIT_RESUME_FAIL      (0x1U << 2U)   /* resume fail */
#define RST_REASON_BIT_NOCMT_TIMEOUT    (0x1U << 4U)   /* SafetyIsland no commit timeout reset */

#define CHIP_TYPE_ASCEND_V1 1
#define CHIP_TYPE_ASCEND_V2 2
#define CHIP_TYPE_ASCEND_V3 3
#define CHIP_TYPE_ASCEND_V51_LITE (0x19513U)
#define CENTRE_NOFITY_CHIP_INDEX 8

#define DEVDRV_HOST_MASTER              0
#define DEVDRV_DEVICE_MASTER            1

typedef int (*dms_set_dev_info_ops)(u32 devid, const void *buf, u32 buf_size);
typedef int (*dms_get_dev_info_ops)(u32 devid, void *buf, u32 *buf_size);
typedef int (*devmm_get_device_accounting_pids_ops)(u32, u32, u32, int *, u32);
typedef int (*devmm_get_device_process_memory_ops)(u32, u32, int, u64 *);

int devdrv_creat_ipc_name(char *ipc_name, unsigned int len);
int devdrv_check_hostpid(pid_t hostpid, unsigned int chip_id, unsigned int vfid);
int devdrv_check_sign(pid_t hostpid, const char *sign, u32 len);
int devdrv_query_process_by_host_pid(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid);
int devdrv_query_process_by_host_pid_user(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid);
int devdrv_query_process_by_host_pid_kernel(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid);
int devdrv_query_process_host_pid(int pid, unsigned int *chip_id, unsigned int *vfid, unsigned int *host_pid,
    enum devdrv_process_type *cp_type);
int devdrv_query_process_host_pids_by_pid(int pid, devdrv_host_pids_info_t *host_pids_info);
/* call on device side, only surport user proc */
int devdrv_query_master_pid_by_host_slave(int slave_pid, u32 *master_pid);
/* call on host side, only surport cp proc */
int devdrv_query_master_pid_by_device_slave(u32 udevid, int slave_pid, u32 *master_pid);
int devdrv_query_master_location(const devdrv_pid_map_info_t *q_info, unsigned int *location);

extern struct devdrv_info *devdrv_manager_get_devdrv_info(u32 dev_id);
int devdrv_inquire_aicore_task(unsigned int dev_id, unsigned int fid, unsigned int tgid,
    unsigned int *result);
int tsdrv_mirror_ctx_status_set(pid_t pid, u32 dev_id, u32 status);
#ifndef AOS_LLVM_BUILD
int devdrv_manager_container_table_devlist_add_ns(u32 *physical_devlist, u32 physical_dev_num,
    struct mnt_namespace *mnt_ns);
int devdrv_manager_container_check_devid_in_container_ns(u32 devid, struct task_struct *tsk);
int devdrv_manager_container_check_devid_in_container(u32 devid, pid_t hostpid);
#else
struct devdrv_info *dms_get_devinfo(u32 devid);
#endif
int dev_mnt_vdevice_logical_id_to_phy_id(u32 logical_id, u32 *phy_id, u32 *vfid);
int dev_mnt_vdevice_phy_id_to_logical_id(u32 phy_id, u32 vfid, u32 *logical_id);
#ifndef AOS_LLVM_BUILD
int dev_mnt_vdev_register_client(u32 phy_id, u32 vfid, const struct file_operations *ops);
#endif
int dev_mnt_vdev_unregister_client(u32 phy_id, u32 vfid);
int devdrv_get_devnum(u32 *num_dev);
int devdrv_get_vdevnum(u32 *num_dev);
int devdrv_manager_devid_to_nid(u32 devid, u32 mem_type);
int devdrv_manager_get_docker_id(u32 *docker_id);
int devdrv_manager_get_process_pids_register(devmm_get_device_accounting_pids_ops func);
void devdrv_manager_get_process_pids_unregister(void);
int devdrv_manager_get_process_memory_register(devmm_get_device_process_memory_ops func);
void devdrv_manager_get_process_memory_unregister(void);
int dms_register_set_dev_info_handler(DMS_DEV_INFO_TYPE type, dms_set_dev_info_ops func);
int dms_unregister_set_dev_info_handler(DMS_DEV_INFO_TYPE type);
int dms_register_get_svm_dev_info_handler(DMS_DEV_INFO_TYPE type, dms_get_dev_info_ops func);
int dms_unregister_get_svm_dev_info_handler(DMS_DEV_INFO_TYPE type);
int devdrv_manager_get_chip_type(int *chip_type);
bool devdrv_manager_ts_is_enable(void);
bool devdrv_manager_is_pf_device(unsigned int dev_id);
int devdrv_get_chip_die_id(u32 dev_id, u32 *chip_id, u32 *die_id);
int devdrv_manager_get_bootstrap(unsigned int *bootstrap);
int devdrv_get_devids(u32 *devices, u32 device_num);
int devdrv_manager_register_power_handle(unsigned int module, int (*func)(struct drvdev_power_state_info *power_info));
int devdrv_manager_unregister_power_handle(unsigned int module);

#endif /* __DEVDRV_MANAGER_COMM_H */


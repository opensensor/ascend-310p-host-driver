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

#ifndef _DEV_UPGRADE_PUBLIC_H_
#define _DEV_UPGRADE_PUBLIC_H_

#include <linux/limits.h>

#ifdef CFG_SOC_PLATFORM_CLOUD
#define DEVICE_NUM_MAX 4
#elif (defined CFG_SOC_PLATFORM_MINIV2) || (defined CFG_SOC_PLATFORM_CLOUD_V2)
#define DEVICE_NUM_MAX 2
#else /* mini & mdc & mini v3 */
#define DEVICE_NUM_MAX 1
#endif

#define BITS_OF_INT       32
#define BITS_OF_SHORT     16
#define BITS_OF_CHAR      8
#define BITS_OF_HALF_CHAR 4

#define DRV_ENABLE        1
#define DRV_DISABLE       0
#define DEVICE_ID_INVALID 0xFF

#define DEV_UPGRADE_STOP_FLAG 0x4b4b4b4b
#define UPGRADE_FILE_SIZE_MAX (512 * 1024 * 1024)

#ifndef OK
#define OK 0
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#define DEV_MID_UPGRADE 0x01

/* 升级模块错误码生成
   通用格式 [15:8]模块 | [7:0]具体错误码 */
#define dev_errno_make(mode, errno) \
    ((((unsigned int)(mode)&0xff) << 8) | ((unsigned int)(errno)&0xff))

#define upgrade_errno_make(errno) \
    dev_errno_make(DEV_MID_UPGRADE, errno)

/* [0x00 , 0x3f]通用错误码定义 */
#define ERRNO_NULL_POINT             0x01 /* 空指针 */
#define ERRNO_INVALID_PARAMS         0x02 /* 无效参数 */
#define ERRNO_INAVLID_ADDR           0x03 /* 无效地址 */
#define ERRNO_NONSUPPORT_ITEM        0x04 /* 不支持的选项 */
#define ERRNO_CRC                    0x05 /* CRC计算出错 */
#define ERRNO_INVALID_LENGTH         0x06 /* 参数长度错误 */
#define ERRNO_INVALID_INDEX          0x07 /* 非法的索引、数组下标等 */
#define ERRNO_MEM_MALLOC             0x08 /* 申请块内存 */
#define ERRNO_MEM_FREE               0x09 /* 释放块内存 */
#define ERRNO_NO_INIT                0x0a /* 没有初始化 */
#define ERRNO_REPEAT_INIT            0x0b /* 重复初始化 */
#define ERRNO_SOURCE_NO              0x0c /* 没有资源 */
#define ERRNO_SAME_VERSION           0x0d /* 版本相同 */
#define ERRNO_HASH_CHECK             0x0e /* HASH校验出错 */
#define ERRNO_HASH                   0x0f /* HASH计算出错 */
#define ERRNO_NOT_ALIGN              0x10 /* 非对齐错误 */
#define ERRNO_NOT_HAVE_VALID_PARTION 0x11 /* PART1的四个分区中无有效nve */
#define ERRNO_COPY_FROM_USER         0x12 /* copy_from_user错误 */
#define ERRNO_COPY_TO_USER           0x13 /* copy_to_user错误 */
#define ERRNO_FILE_TYPE              0x14 /* 文件类型不匹配 */
#define ERRNO_MEM_INIT               0x15 /* 内存初始化错误 */
#define ERRNO_PERMISSION_DENIED      0x16 /* 权限被拒绝 */
#define ERRNO_VERIFY_OR_REVOCATION   0x17 /* 认证或者吊销过程错误 */
#define ERRNO_DFM_REGISTER           0x18 /* 黑匣子注册错误 */
#define ERRNO_VERSION_NOT_MATCH      0x19 /* 版本回滚驱动拦截 */

/* [0x40 , 0x5f]操作系统任务相关错误码定义 */
#define ERRNO_TASK_CREATE       0x40 /* 任务创建 */
#define ERRNO_TASK_INIT         0x41 /* 任务初始化 */
#define ERRNO_TASK_DELETE       0x42 /* 任务删除 */
#define ERRNO_TASK_DELETE_FORCE 0x43 /* 任务强制删除 */
#define ERRNO_TASK_SUSPEND      0x44 /* 任务挂起 */
#define ERRNO_TASK_RESUME       0x45 /* 任务恢复 */
#define ERRNO_TASK_RESTART      0x46 /* 任务重启 */
#define ERRNO_TASK_PRIORITY_SET 0x47 /* 任务优先级设置 */
#define ERRNO_TASK_PRIORITY_GET 0x48 /* 任务优先级获取 */
#define ERRNO_TASK_LOCK         0x49 /* 任务上锁 */
#define ERRNO_TASK_UNLOCK       0x4a /* 任务解锁 */
#define ERRNO_TASK_DELAY        0x4b /* 任务切换 */

/* [0x60 , 0x7f]操作系统信号量相关错误码定义 */
#define ERRNO_SEM_GIVE         0x60 /* 信号量 释放 */
#define ERRNO_SEM_TRY          0x61 /* 信号量 尝试获取 */
#define ERRNO_SEM_TAKE         0x62 /* 信号量 获取 */
#define ERRNO_SEM_FLUSH        0x63 /* 信号量 解锁阻塞任务 */
#define ERRNO_SEM_DELETE       0x64 /* 信号量 删除 */
#define ERRNO_SEM_B_CREATE     0x65 /* 创建二进制信号量 */
#define ERRNO_SEM_C_CREATE     0x66 /* 创建计数信号量 */
#define ERRNO_SEM_M_CREATE     0x67 /* 创建互斥信号量 */
#define ERRNO_SEM_M_GIVE_FORCE 0x68 /* 释放互斥信号量 */
#define ERRNO_SEM_B_SM_CREATE  0x69 /* 创建共享内存二进制信号量 */
#define ERRNO_SEM_C_SM_CREATE  0x6a /* 创建共享内存计数信号量 */
#define ERRNO_SEM_SHOW         0x6b /* 信号量 显示 */

/* [0x80 , 0x9f] 文件系统操作类 通用错误码定义 */
#define ERRNO_FS_NOT_FOUND 0x80 /* 文件没有找到 */
#define ERRNO_FS_WRITE     0x81 /* 文件写入失败 */
#define ERRNO_FS_READ      0x82 /* 文件读取失败 */
#define ERRNO_FS_CONTENT   0x83 /* 内容无效 */
#define ERRNO_FS_FILEID    0x84 /* 文件ID无效 */
#define ERRNO_FS_MKDIR     0x85 /* 创建文件夹失败 */
#define ERRNO_FS_OPEN      0x86 /* 打开失败 */
#define ERRNO_FS_CLOSE     0x87 /* 关闭失败 */
#define ERRNO_FS_SEEK      0x88 /* 关闭失败 */
#define ERRNO_FS_ERASE     0x89 /* 擦除失败 */
#define ERRNO_FS_DELETE    0x90 /* 删除文件失败 */
#define ERRNO_FS_REALPATH  0x91 /* realpath失败 */
#define ERRNO_FS_IOCTL     0x92 /* ioctl失败 */

// DSMI_COMPONENT_TYPE 枚举类型修改时需要与对外头文件中的同一个定义对齐
typedef enum {
    DSMI_COMPONENT_TYPE_NVE,
    DSMI_COMPONENT_TYPE_XLOADER,
    DSMI_COMPONENT_TYPE_M3FW,
    DSMI_COMPONENT_TYPE_UEFI,
    DSMI_COMPONENT_TYPE_TEE,
    DSMI_COMPONENT_TYPE_KERNEL,
    DSMI_COMPONENT_TYPE_DTB,
    DSMI_COMPONENT_TYPE_ROOTFS,
    DSMI_COMPONENT_TYPE_IMU,
    DSMI_COMPONENT_TYPE_IMP,
    DSMI_COMPONENT_TYPE_AICPU,
    DSMI_COMPONENT_TYPE_HBOOT1_A,
    DSMI_COMPONENT_TYPE_HBOOT1_B,
    DSMI_COMPONENT_TYPE_HBOOT2,
    DSMI_COMPONENT_TYPE_DDR,
    DSMI_COMPONENT_TYPE_LP,
    DSMI_COMPONENT_TYPE_HSM,
    DSMI_COMPONENT_TYPE_SAFETY_ISLAND,
    DSMI_COMPONENT_TYPE_HILINK,
    DSMI_COMPONENT_TYPE_RAWDATA,
    DSMI_COMPONENT_TYPE_SYSDRV,
    DSMI_COMPONENT_TYPE_ADSAPP,
    DSMI_COMPONENT_TYPE_COMISOLATOR,
    DSMI_COMPONENT_TYPE_CLUSTER,
    DSMI_COMPONENT_TYPE_CUSTOMIZED,
    DSMI_COMPONENT_TYPE_SYS_BASE_CONFIG,
    DSMI_COMPONENT_TYPE_RECOVERY,
    DSMI_COMPONENT_TYPE_HILINK2,
    DSMI_COMPONENT_TYPE_LOGIC_BIST,
    DSMI_COMPONENT_TYPE_MEMORY_BIST,
    DSMI_COMPONENT_TYPE_ATF,
    DSMI_COMPONENT_TYPE_USER_BASE_CONFIG,
    DSMI_COMPONENT_TYPE_RTC_CORE,
    DSMI_COMPONENT_TYPE_RTC_CONFIG,
    DSMI_COMPONENT_TYPE_BOOTROM,
    DSMI_COMPONENT_TYPE_MAX,
    UPGRADE_AND_RESET_ALL_COMPONENT = 0xFFFFFFF7U,
    UPGRADE_STATE_FLAG = 0xFFFFFFFCU,
    UPGRADE_ALL_IMAGE_COMPONENT = 0xFFFFFFFDU,
    UPGRADE_ALL_FIRMWARE_COMPONENT = 0xFFFFFFFEU,
    UPGRADE_ALL_COMPONENT = 0xFFFFFFFFU
} DSMI_COMPONENT_TYPE;

/* DSMI sub commond for RECOVERY module */
typedef enum {
    DSMI_RCVR_SUB_CMD_SET_FLAG = 0,
    DSMI_RCVR_SUB_CMD_GET_FLAG,
    DSMI_RCVR_SUB_CMD_CLEAN_FLAG,
    DSMI_RCVR_SUB_CMD_RESET_BOOT_CNT,
    DSMI_RCVR_SUB_CMD_SET_STATUS, // set recovery upgrade status.
    DSMI_RCVR_SUB_CMD_RELEASE_DATA,
} DSMI_RECOVERY_SUB_CMD;

typedef enum {
    INDEX_0,
    INDEX_1,
    INDEX_2,
    INDEX_3,
    INDEX_4,
    INDEX_5,
    INDEX_6,
    INDEX_7
} COMMON_INDEX;


#define DAVINCI_LENGTH_FILE_PATH (PATH_MAX - NAME_MAX)

/* Use 'C' as magic number */
#define UPGRADE_CTL_IOC_MAGIC          'C'
#define UPGRADE_FIRMWARE_START         _IOR(UPGRADE_CTL_IOC_MAGIC, 1U, int)
#define UPGRADE_READ_SCHEDULE_CTL      _IOR(UPGRADE_CTL_IOC_MAGIC, 2U, int)
#define UPGRADE_GET_FIREWARE_VER       _IOR(UPGRADE_CTL_IOC_MAGIC, 3U, int)
#define UPGRADE_GET_COMPONENT_LIST     _IOR(UPGRADE_CTL_IOC_MAGIC, 4U, int)
#define UPGRADE_SYNC_IMAGE_COPY        _IOR(UPGRADE_CTL_IOC_MAGIC, 5U, int)
#define UPGRADE_FIRMWARE_STOP          _IOR(UPGRADE_CTL_IOC_MAGIC, 6U, int)
#define UPGRADE_CLR_LOCALCFG           _IOR(UPGRADE_CTL_IOC_MAGIC, 7U, int)
#define UPGRADE_SEC_REVOCATION         _IOR(UPGRADE_CTL_IOC_MAGIC, 8U, int)
#define UPGRADE_SYNC_LOCAL_COMP        _IOR(UPGRADE_CTL_IOC_MAGIC, 9U, int)
#define UPGRADE_IMAGE_CHECK            _IOR(UPGRADE_CTL_IOC_MAGIC, 10U, int)
#define UPGRADE_GET_STATE_FLAG         _IOR(UPGRADE_CTL_IOC_MAGIC, 11U, int)
#define UPGRADE_GET_DEV_INFO           _IOR(UPGRADE_CTL_IOC_MAGIC, 12U, int)
#define UPGRADE_VERIFY_IMG             _IOR(UPGRADE_CTL_IOC_MAGIC, 13U, int)
#define UPGRADE_SET_RCVR_FLAG          _IOR(UPGRADE_CTL_IOC_MAGIC, 14U, int)
#define UPGRADE_GET_RCVR_FLAG          _IOR(UPGRADE_CTL_IOC_MAGIC, 15U, int)
#define UPGRADE_CLEAN_RCVR_FLAG        _IOR(UPGRADE_CTL_IOC_MAGIC, 16U, int)
#define UPGRADE_RESET_RCVR_BOOT_CNT    _IOR(UPGRADE_CTL_IOC_MAGIC, 17U, int)
#define UPGRADE_SET_RCVR_STATUS        _IOR(UPGRADE_CTL_IOC_MAGIC, 18U, int)
#define UPGRADE_SET_DEV_INFO           _IOR(UPGRADE_CTL_IOC_MAGIC, 19U, int)
#define UPGRADE_GET_CHECK_INFO         _IOR(UPGRADE_CTL_IOC_MAGIC, 20U, int)
#define UPGRADE_GET_REPARTITION_FLAG   _IOR(UPGRADE_CTL_IOC_MAGIC, 21U, int)
#define UPGRADE_RELEASE_DATA           _IOR(UPGRADE_CTL_IOC_MAGIC, 22U, int)
#define UPGRADE_VERIFY_PATCH           _IOR(UPGRADE_CTL_IOC_MAGIC, 23U, int)

#define HISFC_PART_NUM_MAX 128

/* RootBubK related */
#define FIRMWARE_HEAD_LEN     0x477
#define ROOT_BUBK_LEN_OFFSET  0x58
#define ROOT_PUBK_OFFSET      0x5C
#define ROOT_PUBK_LEN_VAL     512
#define ROOT_PUBK_TOTAL_SIZE  1024
#define REVOCATE_FILE_LEN     544

/* revocation type */
#define REVOCATION_TYPE_SOC 0
#define REVOCATION_TYPE_CMS_CRL 1
#define REVOCATION_TYPE_CMS_CRL_EXT 2

/* master and standby area */
#define UPGRADE_MASTER_AREA 0
#define UPGRADE_STANDBY_AREA 1

/* system type for mdc */
#define SYSTEM_TYPE_SINGLE 0
#define SYSTEM_TYPE_MULTI 1
#define SYSTEM_TYPE_INVALID 0xFF

/* recovery_type is used to set old recovery upgrade master area, set new recovery upgrade standby area */
#define RECOVERY_TYPE_OLD 0
#define RECOVERY_TYPE_NEW 1

#ifdef CFG_SOC_PLATFORM_MDC_V51
#define MAX_MDC_PKT_NUM       32
#define MAX_NAME_LEN          32
#define MAX_PATH_LEN          64
#define MAX_TYPE_LEN          6
#define SUB_FILE_MAX_PATH_LEN (MAX_NAME_LEN + DAVINCI_LENGTH_FILE_PATH)

/**
 * struct mdc_file_info - MDC upgraded component information
 * @path: upgraded file full path
 * @size: upgraded file size
 * @type: file system format. such as ext4/f2fs/nat
 *
 * When MDC upgrading, read mdc configuration and fill in this struct.
 */
struct mdc_file_info {
    char   path[MAX_PATH_LEN];    /* file path in mdc package */
    char   type[MAX_TYPE_LEN];
};

/**
* struct mdc_file_storage - MDC UFS storage information
* @start: Start position of the file to be burnt on the device
* @len: Actual size of the partition
* @sign_ltart: Location of the file signature information in the file
* @sign_len: Length of the file signature information
* @back: Start position of the backup file on the storage device.
*        The value is 0 if there is no backup file.
* @lun: Main logical partition ID saved in the file
* @back_lun: Number of the standby logical partition for which the file is saved
*
* During the MDC upgrade, the upgrade package of each component needs to be
* saved to the UFS. This structure stores the component storage information,
* such as the logical unit number, storage offset, and occupied space.
*/
struct mdc_file_storage {
    unsigned long long    start;
    unsigned long long    len;
    unsigned long long    back;
    unsigned int    lun;
    unsigned int    back_lun;
};

struct  mdc_package {
    char   name[MAX_NAME_LEN];  /* file name: from mdcpack.xml */
    char   file_name[SUB_FILE_MAX_PATH_LEN];
    struct mdc_file_info file_info;
    struct mdc_file_storage storage;
};
typedef struct mdc_package mdc_package_t;

typedef struct tag_upgrade_sub_file_st {
    unsigned int sub_file_count;
    mdc_package_t *sub_file_info[MAX_MDC_PKT_NUM];
} upgrade_sub_file;

struct component_file_info {
    unsigned int component_type;
    unsigned int file_size;
    unsigned char *file_content;
    upgrade_sub_file sub_pkt;
};

typedef struct tag_upgrade_ufs_component_info_st {
    unsigned int component_type;
    unsigned int valid;       // 0:invalid 1:valid
    unsigned int area_flag;   // 0:single 1:double
    unsigned int area_check;  // 0:ok 1:err
    unsigned int cur_flag;    // 0:master 1:backup
    unsigned int system_type; // 0:singleOS 1:multiOS
    unsigned int recovery_type; // 0:old 1:new
    char component_name[MAX_NAME_LEN];
    upgrade_sub_file sub_pkt_m;
    upgrade_sub_file sub_pkt_b;
} upgrade_ufs_component_info;

typedef struct tag_upgrade_ufs_single_area_info_st {
    unsigned int component_type;
    unsigned int valid;       // 0:invalid 1:valid
    unsigned int area_flag;   // 0:single 1:double
    unsigned int master_flag; // 0:master 1:backup
    unsigned int cur_flag;    // 0:master 1:backup
    upgrade_sub_file sub_pkt;
} upgrade_ufs_single_area_info;

struct upgrade_ufs_local_component_start_in {
    int dev_id;
    unsigned int component_cnt;
    upgrade_ufs_single_area_info component_info[DSMI_COMPONENT_TYPE_MAX];
};

struct upgrade_ufs_local_component_start_out {
    int result;
};

#else
struct component_file_info {
    unsigned int component_type;
    unsigned int file_size;
    unsigned char *file_content;
};
#endif

struct upgrade_start_in {
    int dev_id;
    unsigned int count;
    unsigned int system_type;
    struct component_file_info file_list[DSMI_COMPONENT_TYPE_MAX];
};

struct upgrade_start_out {
    int result;
};

struct upgrade_read_schedule_in {
    int dev_id;
};

struct upgrade_read_schedule_out {
    unsigned int schedule;
};

struct upgrade_get_component_list_in {
    int dev_id;
};

struct upgrade_get_component_list_out {
    unsigned long long bitmap;
};

struct upgrade_sync_image_in {
    int dev_id;
    unsigned int component_type;
};

struct upgrade_sync_image_out {
    int result;
};

struct upgrade_check_image_in {
    int dev_id;
    unsigned int flash_chk;
    unsigned int ufs_chk;
};

struct upgrade_check_image_out {
    int result;
    unsigned int check_result;
};

struct upgrade_get_state_flag_in {
    int dev_id;
};

struct upgrade_get_state_flag_out {
    int result;
    unsigned int state_flag;
};

struct upgrade_stop_in {
    int dev_id;
};

struct upgrade_stop_out {
    int result;
};

struct upgrade_clr_localcfg_in {
    int dev_id;
};

struct upgrade_clr_localcfg_out {
    int result;
};

struct upgrade_get_version_in {
    int dev_id;
    unsigned int component_type;
    unsigned int buffLen;
    void *outBuff;
};

struct upgrade_get_version_out {
    int result;
};

struct upgrade_recovery_status_in {
    unsigned int op;
    unsigned int part;
};

typedef enum {
    DSMI_UPGRADE_MAIN_TYPE_VERSION = 0,
    DSMI_UPGRADE_MAIN_TYPE_PARTITION,
    DSMI_UPGRADE_MAIN_TYPE_REG_PROG_PKG_INFO, // register program package infomation
    DSMI_UPGRADE_SUB_TYPE_FW_VERIFY,
    DSMI_UPGRADE_MAIN_TYPE_CRL,
    /* Basic version information of the BOM, which is written in the fixed area of
       the flash memory during production. */
    DSMI_UPGRADE_MAIN_TYPE_BOM_INIT_VER,
    DSMI_UPGRADE_MAIN_TYPE_MAX,
} DSMI_UPGRADE_MAIN_TYPE;

typedef enum {
    DSMI_UPGRADE_GET_PUBLIC_AREA_CRL,
    DSMI_UPGRADE_GET_LOCAL_IMAGE_CRL,
    DSMI_UPGRADE_SUB_TYPE_CRL_MAX,
} DSMI_UPGRADE_SUB_TYPE_CRL;

/* partition type */
typedef enum {
    PARTITION_A = 0,
    PARTITION_B,
} PARTITION_TYPE;

/* DSMI sub commond for UPGRADE module */
typedef enum {
    DSMI_UPGRADE_SUB_TYPE_GET_PARTITION = 0,
    DSMI_UPGRADE_SUB_TYPE_SET_PARTITION = 1,
    DSMI_UPGRADE_SET_COLD_BOOT_AREA = 2,
    DSMI_UPGRADE_SAVE_ORIGIN_AREA = 3,
    DSMI_UPGRADE_GET_UPGRADE_INFO = 4,
    DSMI_UPGRADE_SET_WARM_BOOT_AREA = 5,
    DSMI_UPGRADE_GET_WARM_BOOT_AREA = 6,
    DSMI_UPGRADE_GET_RUN_IMG_LOCATION_INFO = 7,
    DSMI_UPGRADE_SUB_TYPE_PARTITION_MAX,
} DSMI_UPGRADE_SUB_TYPE_PARTITION;

/* package type, Security information is at the head or tail of the file. */
typedef enum {
    SEC_INFO_AT_HEAD = 0,
    SEC_INFO_AT_TAIL,
} PACKAGE_TYPE;

#define UPGRADE_RESERVED_LEN 10

typedef struct upgrade_flag {
    unsigned int upgrade_sys; // 升级标志
    unsigned int boot_area;   // 启动分区， boot_area=BOOT_NONE时，TEE清除升级状态、启动分区、原版本分区
    unsigned int reserved[UPGRADE_RESERVED_LEN];
} upgrade_flag_st;

typedef struct upgrade_info {
    unsigned int upgrade_sys; // 升级标志
    unsigned int boot_area_firmware; // 启动分区
    unsigned int boot_area_image;    // C20升级存在2套升级标志，需要感知ufs升级区信息
    unsigned int origin_area; // 主/备/NONE三种状态（以UFS启动区为准）
    unsigned int reserved[UPGRADE_RESERVED_LEN];
} upgrade_info_st;

typedef struct upgrade_bom_init_ver {
    int driver;    /* 驱动包的初始版本号 */
    int firmware;  /* 固件包的初始版本号 */
} upgrade_bom_init_ver_st;

typedef struct runimglocation_info {
    unsigned int valid;            /* 指示以下提供的信息是否有效可用，0代表无效，1表示有效 */
    unsigned int os_location;      /* 指示OS当前启动分区，0代表主区启动，1代表备区启动 */
    unsigned int flash_location;   /* 指示Flash首选启动分区，0代表主区启动，1代表备区启动 */
    unsigned int disk_type;        /* 指示磁盘介质类型，0为UFS，1为SSD，2为EMMC */
    unsigned int reserved[UPGRADE_RESERVED_LEN];     /* 保留字段，默认不处理 */
} runimglocation_info_st;

typedef enum {
    FLAG_UPGRADE_NONE = 0,
    FLAG_UPGRADING = 1,
    UPGRADE_SYS_MAX_VAL = 2,
} UPGRADE_SYS;

typedef enum {
    BOOT_AREA_NONE = 0,
    BOOT_AREA_MAIN = 1,
    BOOT_AREA_BAK = 2,
    BOOT_AREA_MAX_VAL = 3,
} BOOT_AREA;

#define UPGRADE_NONE_MAGIC 0
#define UPGRADING_MAGIC 0xD6C55BC1

#define BOOT_NONE_MAGIC 0
#define BOOT_MAIN_MAGIC 0xC11CB55BU
#define BOOT_BAK_MAGIC  0x3443DAAD

typedef enum {
    SEC_SET_BOOT_AREA_CMD = 0,
    SEC_SET_ORIGIN_AREA_CMD = 1,
    SEC_SET_UPGRADE_STATUS = 2,
    SEC_SET_RECOV_FLAG = 3,
    SEC_SET_VESION = 4,
    SEC_CLEAR_BOOT_AREA = 5,
} SEC_SET_UPGRADE_FLAG_CMD;

#define PROG_PKG_INFO_RESERVE_LEN 9
/* Format of the input parameter of the DSMI_UPGRADE_MAIN_TYPE_REG_PROG_PKG_INFO */
typedef struct program_pkg_info {
    unsigned int component_id;
    PARTITION_TYPE partition;
    unsigned int lun_id;
    unsigned long long lun_offset;
    PACKAGE_TYPE pkg_type;
    unsigned int flag;
    unsigned int reserve[PROG_PKG_INFO_RESERVE_LEN];
} PROG_PKG_INFO;

typedef struct program_all_pkg_info {
    unsigned int flag;
    PROG_PKG_INFO pkg_info_a[DSMI_COMPONENT_TYPE_MAX];
    PROG_PKG_INFO pkg_info_b[DSMI_COMPONENT_TYPE_MAX];
} PROG_ALL_PKG_INFO;

struct upgrade_dev_info_in {
    int dev_id;
    unsigned int cmd;
    unsigned int size;
    void *buf;
};

struct upgrade_dev_info_out {
    unsigned int size;
    int result;
};

struct upgrade_revocation_in {
    int dev_id;
    unsigned int type;
    unsigned int file_size;
    void *file_data;
};

struct upgrade_revocation_out {
    int result;
};

struct upgrade_verify_img_in {
    int dev_id;
    char *path;
    unsigned int size;
    int mode;
};

struct upgrade_verify_img_out {
    int result;
};

struct upgrade_ioctl_msg {
    int dev_id;
    signed int in_size;
    signed int out_size;
    void *in;
    void *out;
};

struct upgrade_get_check_info_in {
    int dev_id;
    unsigned int buffLen;
    void *outBuff;
};

struct upgrade_get_check_info_out {
    int result;
};

struct upgrade_verify_patch {
    unsigned int dev_id;
};

#endif /* _DEV_UPGRADE_PUBLIC_H_ */

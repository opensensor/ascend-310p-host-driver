/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2022-3-24
 * File Name     : dms_product.c
 * Version       : Initial product
 * Author        :
 * Created       : 2022-3-24
 * Last Modified :
 * Description   :

 *  History       :
 * 1.Date        : 2022-3-24
 *    Author      :
 *    Modification: Created file
 */
#ifndef __DMS_PRODUCT_H__
#define __DMS_PRODUCT_H__

#define DMS_PRODUCT_CMD_NAME "DMS_PRODUCT_CMD_NAME"
#define DMS_SUBCMD_GET_PCIE_ID_INFO_ALL 0
#define DMS_SUBCMD_GET_WORK_MODE 1
// DMS_SUBCMD_GET_VRD_INFO 2 in turing
#define DMS_SUBCMD_GET_PCIE_BANDWIDTH_INFO 3
#define DMS_SUBCMD_GET_HBM_MANUFACTURER_ID 4
#define DMS_SUBCMD_GET_MAINBOARD_ID 5
#define DMS_SUBCMD_GET_HCCS_STATUS 6
#define DMS_SUBCMD_GET_HCCS_BANDWIDTH_INFO 7
#define DMS_SUBCMD_GET_HCCS_LINK_ERROR_INFO 8
#define DMS_SUBCMD_GET_PCIE_LINK_ERROR_INFO 9
#define DMS_SUBCMD_GET_CARD_MULTI_DIE_POLICY 11
#define DMS_SUBCMD_SET_CARD_MULTI_DIE_POLICY 12
#include "ascend_kernel_hal.h"
#include "devdrv_interface.h"
#ifdef CONFIG_LLT
static inline void printf_stub(char *format, ...) {
}
#define dms_err          printf_stub
#endif
struct dmanage_pcie_id_info_all {
    unsigned int venderid;    /* 厂商id */
    unsigned int subvenderid; /* 厂商子id */
    unsigned int deviceid;    /* 设备id */
    unsigned int subdeviceid; /* 设备子id */
    int domain;               /* pcie域 */
    unsigned int bus;         /* 总线号 */
    unsigned int device;      /* 设备物理号 */
    unsigned int fn;          /* 设备功能号 */
    unsigned int davinci_id;  /* device id */
    unsigned char reserve[28];
};

#define A2_CARD_BOARD_TYPE_MIN          0x01
#define A2_CARD_BOARD_TYPE_MAX          0x02
#define A2_BOARD_TYPE_SHIFT             4
#define A2_CARD_DEFAULT_MAINBOARD_ID    0xFF

#ifdef CFG_FEATURE_HCCS_BANDWIDTH
#define HCCS_MAX_PCS_NUM                16
#define MS_TO_NS                        1000000
#define PROFILING_TIME_HCCS_MAX         1000
#define PROFILING_TIME_HCCS_MIN         1
#define OVERFLOWADD                     1
#define DRV_INFO_RESERVED_LEN           4
#define LINK_NUM                        8
#define HDLC_LINK_HCCS_NUM              2
#define HCCS_HDLC_REG_SIZE              4
#define HCCS_HDLC0_BASE_ADDR            0x000601910000
#define HCCS_HDLC1_BASE_ADDR            0x000601920000
#define HCCS_HDLC2_BASE_ADDR            0x000601930000
#define HCCS_HDLC3_BASE_ADDR            0x000601940000
#define HCCS_HDLC_REG_MAP_SIZE          0x1100
#define HCCS_HDLC_RETRY_CNT_ADDR        0x1078
#define HCCS_HDLC_CRC_ERR_CNT_ADDR      0x1090
#define HCCS_HDLC_TX_CNT_ADDR           0x10B0
#define HCCS_HDLC_RX_CNT_ADDR           0x10F0

typedef struct {
    int profiling_time;
    unsigned int tx_bandwidth[HCCS_MAX_PCS_NUM];
    unsigned int rx_bandwidth[HCCS_MAX_PCS_NUM];
} hccs_link_bandwidth_t;

typedef struct {
    unsigned int tx_cnt[HCCS_MAX_PCS_NUM];
    unsigned int rx_cnt[HCCS_MAX_PCS_NUM];
    unsigned int crc_err_cnt[HCCS_MAX_PCS_NUM];
    unsigned int retry_cnt[HCCS_MAX_PCS_NUM];
    unsigned int reserve[HCCS_MAX_PCS_NUM * 3];
} hccs_statistic_info;

typedef enum {
    INFO_TRUE = 1,
    INFO_FALSE,
} get_hccs_status;

extern int hal_kernel_get_hardware_info(unsigned int phy_id, devdrv_hardware_info_t *hardware_info);
extern unsigned int hccs_reg_read(unsigned long vir_addr);
int dms_feature_get_hccs_bandwidth_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif

#ifdef CFG_FEATURE_PCIE_HCCS_BANDWIDTH
#include "agentdrv_profiling.h"
#include "drv_profile.h"
#define PROFILING_BYTE_TRANS_TO_MBYTE   (1024 * 1024)  /* byte to Mbyte */
#define PROFILING_US_TRANS_TO_MS        1000
#define PROFILING_TIME_MAX              2000

#define AGENTDRV_PROF_DATA_NUM 3
typedef struct pcie_link_bandwidth_struct {
    int profiling_time;
    u32 tx_p_bw[AGENTDRV_PROF_DATA_NUM];
    u32 tx_np_bw[AGENTDRV_PROF_DATA_NUM];
    u32 tx_cpl_bw[AGENTDRV_PROF_DATA_NUM];
    u32 tx_np_lantency[AGENTDRV_PROF_DATA_NUM];

    u32 rx_p_bw[AGENTDRV_PROF_DATA_NUM];
    u32 rx_np_bw[AGENTDRV_PROF_DATA_NUM];
    u32 rx_cpl_bw[AGENTDRV_PROF_DATA_NUM];
} pcie_link_bandwidth_t;

extern int agentdrv_pcie_profiling_open(struct prof_peri_para para);
extern int agentdrv_pcie_profiling_sampling(struct prof_peri_para para);
extern int agentdrv_pcie_profiling_close(struct prof_peri_para para);

int dms_feature_get_pcie_bandwidth_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif

#if (defined(CFG_FEATURE_HBM_MANUFACTURER_ID) && !defined(DCFG_FEATURE_DMS_PRODUCT_HOST))
#define CONNECT_PROTOCOL_PCIE       0
#define CONNECT_PROTOCOL_HCCS       1
#define CONNECT_PROTOCOL_UNKNOWN    2

#define FULLMESH_CHIP_BASE_ADDR         0x0
#define FULLMESH_CHIP_OFFSET            0x80000000000ULL
#define FULLMESH_DIE_OFFSET             0x10000000000ULL

#define HCCS_CHIP_BASE_ADDR             0x200000000000ULL
#define HCCS_CHIP_OFFSET                0x20000000000ULL
#define HCCS_DIE_OFFSET                 0x10000000000ULL

#define CHIP_BASE_ADDR                  0x0
#define CHIP_OFFSET                     0x200000000000
#define DIE_OFFSET                      0x0
#define DEVDRV_LOAD_SRAM_ADDR           0x402000000ULL
#define DEVDRV_LOAD_SRAM_SIZE           0x4ULL
#define DEVDRV_MANUFACTURER_ID_OFFSET   0x39980ULL

#define MANUFACTURER_ID_ERROR           0xFFFFFFFFU

int dms_feature_get_hbm_manufacturer_id(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif

#ifdef CFG_FEATURE_HCCS_LINK_ERROR_INFO
#define HDLC0_BASE_ADDR        0x812B1000
#define HDLC_RETRY_CNT_ADDR         0x4A0
#define HDLC_CRC_ERR_CNT_ADDR       0x7C0
#define HDLC_REG_MAP_SIZE          0x1000
#define HCCS_MAX_PCS_NUM               16

struct hccs_statistic_info {
    unsigned int tx_cnt[HCCS_MAX_PCS_NUM];
    unsigned int rx_cnt[HCCS_MAX_PCS_NUM];
    unsigned int crc_err_cnt[HCCS_MAX_PCS_NUM];
    unsigned int retry_cnt[HCCS_MAX_PCS_NUM];
    unsigned int reserve[HCCS_MAX_PCS_NUM * 3];
};

struct hccs_link_err_info_t {
    unsigned int crc_err_cnt;
    unsigned int retry_cnt;
};

extern unsigned int hccs_reg_read(unsigned long vir_addr);
extern int hal_kernel_get_hardware_info(unsigned int phy_id, devdrv_hardware_info_t *hardware_info);
int dms_feature_get_hccs_link_error_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif

#ifdef CFG_FEATURE_PCIE_LINK_ERROR_INFO
#define SOC_MISC_DEVICE_NUM_MAX 4
#define RAS_PCIE_DFX_LCRC_ERR_NUM   0xA2906050
#define RAS_PCIE_DFX_RETRY_CNT      0xA29060B8
#define PCIE_RAS_REG_SIZE 0x4

struct soc_misc_pcie_link_err_info {
    u32 lcrc_err_cnt;
    u32 retry_cnt;
};

int dms_feature_get_pcie_link_error_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif

int dms_product_init(void);
void dms_product_exit(void);
int devdrv_get_pcie_id_all(void *feature, char *in, u32 in_len, char *out, u32 out_len);
extern int devdrv_get_pcie_id_info(u32 devid, struct devdrv_pcie_id_info *pcie_id_info);
extern int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
int devdrv_get_hccs_link_status_and_group_id(u32 devid, u32 *hccs_status, u32 hccs_group_id[], u32 group_id_num);
int devdrv_get_hccs_status(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int devdrv_get_mainboard_id(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#endif
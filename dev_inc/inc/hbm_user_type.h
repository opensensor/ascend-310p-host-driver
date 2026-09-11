/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description:
 * Author: huawei
 * Create: Fri Feb 5 13:53:18 2021
 * Notes:
 * History:
 *
 */
#ifndef __HBM_USER_TYPE_H__
#define __HBM_USER_TYPE_H__

#define MAX_RECORD_ECC_ADDR_COUNT   64
#pragma pack(1)
/* Force single byte alignment to return to out of band fetch */
struct multi_ecc_time_data_s {
    uint32_t multi_record_count;
    uint32_t multi_ecc_times[MAX_RECORD_ECC_ADDR_COUNT];
};
#pragma pack()

struct single_ecc_data_s {
    /* HBMC no. */
    uint32_t hbmc_id;
    uint32_t single_bit_count;
    uint32_t single_bit_low_addr;
    uint32_t single_bit_high_addr;
    uint32_t last_appear_time_stamp;
};

struct multi_ecc_data_s {
    uint64_t physical_addr;
    uint8_t resv[4];  /* 4 is resv length */
    uint16_t rank;
    uint16_t module_id;
    uint8_t type;
    uint8_t module;
    uint16_t bank;
    uint16_t row;
    uint16_t column;
    int timer_stamp;
};

struct ecc_address_count_s {
    uint32_t single_ecc_addr_cnt;
    uint32_t multi_ecc_addr_cnt;
};

#define MAX_USER_DATA_LEN 65
struct ecc_config_udata_s {
    uint32_t dev_id;
    uint32_t op_type;
    uint32_t data_index;
    union {
        uint32_t data[MAX_USER_DATA_LEN];
        struct multi_ecc_time_data_s multi_ecc_time_data;
        struct multi_ecc_data_s multi_ecc_data;
        struct single_ecc_data_s single_ecc_data;
        struct ecc_address_count_s ecc_address_count;
        uint8_t pending;
    };
};

enum {
    MULTI_ECC_TIMES_READ = 0,
    SINGLE_ECC_INFO_READ,
    MULTI_ECC_INFO_READ,
    ECC_ADDRESS_COUNT_READ,
    ECC_PENDING_READ_CMD,
    ECC_MAX_READ_CMD,
};

#endif

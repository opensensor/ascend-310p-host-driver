/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-6-27
 */
#ifndef DBL_BOARD_CONFIG_H
#define DBL_BOARD_CONFIG_H

#include "ascend_kernel_hal.h"

#define MAX_CHIP_SERDES_LAN_NUM 8

#define PCIE_CONNECT_TYPE               0
#define HCCS_CONNECT_TYPE               1

enum dbl_serdes_type {
    SERDES_TYPE_NULL = 0,
    SERDES_TYPE_PCIE,
    SERDES_TYPE_SATA,
    SERDES_TYPE_USB,
    SERDES_TYPE_GE
};
enum dbl_frequency_level {
    SERDES_FREQ_NULL,
    SERDES_FREQ_1G,
};

#define DBL_INVALID_CONFIG_VALUE 0xFF
struct dbl_serdes_lan_info {
    unsigned short lan_index;
    /* multiplexing type e.g. usbpcie */
    enum dbl_serdes_type type;
    /* Exhibition frequency  disable0  enable 1 */
    unsigned char ssc_enable;
    /* polarity:0   reversal:1 */
    unsigned char polarity_tx;
    /* polarity:0   reversal:1 */
    unsigned char polarity_rx;
    /* lan order, positive sequence: 0, inverted sequence:1 */
    unsigned char lan_order;
    /* bandwidth size e.g. If the type is PCIE the value is 2 or 4,
    meaning is x2 or x4, Invalid value is 0xff */
    unsigned char bandwidth;
    /* 0 : master,  1: salver,  Invalid value is 0xff */
    unsigned char align_mode;
    /* frequency level */
    enum dbl_frequency_level frequency;
};

typedef enum {
    ADDR_MODE_INDEPENDENT = 0,
    ADDR_MODE_UNIFIED,
} ADDR_MODE_TYPE;

struct dbl_board_serdes_info {
    unsigned short lan_count;
    struct dbl_serdes_lan_info lan_info[MAX_CHIP_SERDES_LAN_NUM];
};

struct dbl_board_serdes_info *dbl_get_board_serdes_info(void);

#endif /* DBL_BOARD_CONFIG_H */

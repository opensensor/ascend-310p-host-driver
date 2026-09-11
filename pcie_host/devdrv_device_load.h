/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#ifndef _DEVDRV_DEVICE_LOAD_H_
#define _DEVDRV_DEVICE_LOAD_H_

#include <linux/workqueue.h>
#include <linux/irqreturn.h>

#include "devdrv_interface.h"

extern struct devdrv_black_callback g_black_box;
#define DEVDRV_HOST_FILE_PATH "/home/bios"

#define DEVDRV_BLOCKS_NUM 16

#define DEVDRV_BLOCKS_COUNT 2

#define DEVDRV_BLOCKS_NAME_SIZE 32
#define DEVDRV_BLOCKS_ADDR_PAIR_NUM 3000
#define DEVDRV_BLOCKS_STATIC_NUM 48

#define DEVDRV_BLOCKS_SIZE (1024UL * 1024)
#define DEVDRV_LOAD_FILE_MAX_SIZE (DEVDRV_BLOCKS_SIZE * DEVDRV_BLOCKS_ADDR_PAIR_NUM)
#define DEVDRV_BLOCKS_STATIC_SIZE (DEVDRV_BLOCKS_SIZE * DEVDRV_BLOCKS_STATIC_NUM)

#define DEVDRV_DMA_CACHE_NUM 128
#define DEVDRV_DMA_ALLOC_DEPTH 8

#define DEVDRV_LOAD_FILE_BEGIN 0x1111111111111111UL
#define DEVDRV_NORMAL_BOOT_MODE 0x2222222222222222UL
#define DEVDRV_ABNORMAL_BOOT_MODE 0xFFFFFFFFFFFFFFFFUL
#define DEVDRV_SLOW_BOOT_MODE 0x3333333333333333UL
#define DEVDRV_SLOW_TO_NORMAL_MODE 0x4444444444444444UL
#define DEVDRV_LOAD_SUCCESS 0x55555555u
#define DEVDRV_LOAD_NOTICE 0x5555555555555555UL
#define DEVDRV_LOAD_FINISH 0
#define DEVDRV_SEND_FINISH 0x6666666666666666UL
#define DEVDRV_SEND_PATT_FINISH 0x7777777777777777UL
#define DEVDRV_RECV_FINISH 0x8888888888888888UL
#define DEVDRV_NO_FILE 0X9999999999999999UL
#define DEVDRV_TEE_CHECK_FAIL 0x5A5AA5A55A5AA5A0ULL
#define DEVDRV_IMAGE_CHECK_FAIL 0x5A5AA5A55A5AA5A1ULL
#define DEVDRV_FILESYSTEM_CHECK_FAIL 0x5A5AA5A55A5AA5A2ULL

#define DEVDRV_BIOS_VERSION_SUPPORT_REG 0x3dc4c
#define DEVDRV_BIOS_VERSION_SUPPORT_FLAG 0x5aa5dcbau
#define DEVDRV_BIOS_VERSION_SUPPORT_CLEAR 0x5aa5dc02u
#define DEVDRV_BIOS_BOOTROM_VERSION_REG 0x3dc50
#define DEVDRV_BIOS_XLOADER_VERSION_REG 0x3DC60
#define DEVDRV_BIOS_NVE_VERSION_REG 0x3dc70

#define DEVDRV_LOAD_TIMEOUT 200000ul /* 2s */
#define DEVDRV_LOAD_DELAY 100        /* 100ms */
#define DEVDRV_LOAD_TIMES (DEVDRV_LOAD_TIMEOUT / DEVDRV_LOAD_DELAY)
#define DEVDRV_LOAD_ABORT 1
#define DEVDRV_ADDR_ALIGN 64
#define DEVDRV_GET_FLAG_COUNT 100
#define DEVDRV_DELAY_TIME 20
#define DEVDRV_WAIT_LOAD_FILE_TIME 600

#define DEVDRV_MAX_FILE_SIZE (1024 * 100)
#define DEVDRV_STR_MAX_LEN 128
#define DEVDRV_CONFIG_OK 0
#define DEVDRV_CONFIG_FAIL 1
#define DEVDRV_CONFIG_NO_MATCH 1

#define DEVDRV_NON_CRITICAL_FILE 0
#define DEVDRV_CRITICAL_FILE 1
#define DEVDRV_NON_NOTICE 0
#define DEVDRV_NOTICE_BIOS 1

#ifdef DRV_UT
#define DEVDRV_LOAD_FILE_WAIT_TIME (10 * 1000) /* ms */
#else
#define DEVDRV_LOAD_FILE_WAIT_TIME (600 * 1000) /* ms */
#endif
#define DEVDRV_LOAD_FILE_CHECK_TIME 20
#define DEVDRV_LOAD_FILE_CHECK_CNT (DEVDRV_LOAD_FILE_WAIT_TIME / DEVDRV_LOAD_FILE_CHECK_TIME)

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define DEVDRV_TIMER_SCHEDULE_TIMES 1800 /* fpga:1800s */
#else
#define DEVDRV_TIMER_SCHEDULE_TIMES 300 /* asic:300s */
#endif
#define DEVDRV_TIMER_EXPIRES (1 * HZ)

#define DEVDRV_OPEN_CFG_FILE_TIME_MS  100
#define DEVDRV_OPEN_CFG_FILE_COUNT    50

#define DEVDRV_ALIGN(addr, size) (((addr) + ((size) - 1)) & (~((typeof(addr))(size) - 1)))

#define DEVDRV_PCIE_CFG_CMDSTS_REG 0X04
#define DEVDRV_PCIE_BAR0_CFG_REG 0X10
#define DEVDRV_PCIE_BAR1_CFG_REG 0X14
#define DEVDRV_PCIE_BAR2_CFG_REG 0X18
#define DEVDRV_PCIE_BAR3_CFG_REG 0X1C
#define DEVDRV_PCIE_BAR4_CFG_REG 0X20
#define DEVDRV_PCIE_BAR5_CFG_REG 0X24
#define DEVDRV_BAR_CFG_OFFSET 32

enum devdrv_load_wait_mode {
    DEVDRV_LOAD_WAIT_INTERVAL = 0x0,
    DEVDRV_LOAD_WAIT_FOREVER,
    DEVDRV_LOAD_WAIT_UNKNOWN
};

struct devdrv_load_addr_pair {
    void *addr;
    dma_addr_t dma_addr;
    u64 size;      /* block size */
    u64 data_size; /* data length is this block */
};

struct devdrv_load_blocks {
    char name[DEVDRV_BLOCKS_NAME_SIZE];
    u64 blocks_num;
    u64 blocks_valid_num;
    struct devdrv_load_addr_pair blocks_addr[DEVDRV_BLOCKS_ADDR_PAIR_NUM];
};

struct devdrv_load_work {
    struct devdrv_pci_ctrl *ctrl;
    struct work_struct work;
};

struct devdrv_agent_load {
    struct device *dev;
    u32 dev_id;
    void __iomem *mem_sram_base;
    struct devdrv_load_blocks *blocks;

    struct timer_list load_timer; /* device os load time out timer */
    int timer_remain;             /* timer_remain <= 0 means time out */
    int timer_expires;

    struct devdrv_load_work load_work;
    atomic_t load_flag;

    int load_wait_mode;
    int load_abort;
};

struct devdrv_load_file_cfg {
    char *file_name;
    u8 file_type;
    u8 fail_mode;
};

struct devdrv_load_file {
    char file_name[DEVDRV_STR_MAX_LEN];
    u8 file_type;
    u8 fail_mode;
};

int devdrv_load_device(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_load_exit(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_notify_blackbox_err(u32 devid, u32 code);
void devdrv_set_load_abort(struct devdrv_agent_load *agent_loader);
irqreturn_t devdrv_load_irq(int irq, void *data);

char *devdrv_get_config_file(void);
u32 devdrv_get_env_value_from_file(char *file, const char *env_name, char *env_val, u32 env_val_len);

#endif

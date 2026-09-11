/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-12-19
*/
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>

#include "ka_system_pub.h"
#include "ka_base_pub.h"
#include "ka_memory_pub.h"

#include "securec.h"
#include "devdrv_user_common.h"
#include "dms_define.h"
#include "ascend_kernel_hal.h"
#include "dms_timer.h"
#include "devdrv_interface.h"
#include "dms_hccs_init.h"
#include "devdrv_manager_comm.h"
#include "dms_kernel_version_adapt.h"
#include "dms_cmd_def.h"
#include "devdrv_common.h"
#ifndef CFG_HOST_ENV
#include "devdrv_cache_flush.h"
#include "ascend_platform.h"
#include "hsm_norflash.h"
#endif
#include "dms_hccs_credit.h"

#ifdef CFG_HOST_ENV
#define CREDIT_NUM_NO_UPDATE_WARN_THD 24
#define CREDIT_NUM_NO_UPDATE_WARN_THD_93 48
static hccs_credit_update_info g_hccs_credit_update_info[DEVDRV_PF_DEV_MAX_NUM];

STATIC int get_hccs_credit_update_expire_times(u32 devid, int *expire_times)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_manager_get_devdrv_info(devid);
    if (dev_info == NULL) {
        dms_err("Failed to get dev_info. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (dev_info->multi_die == 1) {
        *expire_times = CREDIT_NUM_NO_UPDATE_WARN_THD_93;
    } else {
        *expire_times = CREDIT_NUM_NO_UPDATE_WARN_THD;
    }

    return 0;
}

STATIC int dms_hccs_credit_num_update_check(u32 dev_id, u64 cur_cnt)
{
    int ret;
    int expire_times = 0;
    unsigned long long cur_timestamp = 0;
    unsigned long long interval_time_s = 0;

    ret = get_hccs_credit_update_expire_times(dev_id, &expire_times);
    if (ret != 0) {
        dms_err("Get hccs credit update expire times failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EINVAL;
    }

    cur_timestamp = ka_system_ktime_get_raw_ns();
    if (cur_timestamp < g_hccs_credit_update_info[dev_id].old_update_timestamp) {
        g_hccs_credit_update_info[dev_id].old_update_timestamp = cur_timestamp;
        return 0;
    }
    interval_time_s = (cur_timestamp - g_hccs_credit_update_info[dev_id].old_update_timestamp) / KA_NSEC_PER_SEC;

    if (cur_cnt == g_hccs_credit_update_info[dev_id].old_cnt &&
        interval_time_s > expire_times) {
        if(cur_cnt != g_hccs_credit_update_info[dev_id].log_cnt) {
            dms_err("The number of credit has not been updated for more than %d seconds. "
                "(dev_id=%u; old_cnt=%llu; old_timestamp=%llus; cur_timestamp=%llus)\n",
                expire_times, dev_id, g_hccs_credit_update_info[dev_id].old_cnt,
                g_hccs_credit_update_info[dev_id].old_update_timestamp / KA_NSEC_PER_SEC, cur_timestamp / KA_NSEC_PER_SEC);
            g_hccs_credit_update_info[dev_id].log_cnt = cur_cnt;
        }
        return -EBUSY;
    }

    if (cur_cnt != g_hccs_credit_update_info[dev_id].old_cnt) {
        g_hccs_credit_update_info[dev_id].old_update_timestamp = cur_timestamp;
        g_hccs_credit_update_info[dev_id].old_cnt = cur_cnt;
    }

    return 0;
}

STATIC int dms_read_hccs_credit_num_from_shm(u32 dev_id, hccs_credit_num *credit_num_info)
{
    int ret = 0;
    u64 shm_addr;
    size_t shm_size;
    void __iomem *credit_info_addr = NULL;

    ret = devdrv_get_addr_info(dev_id, DEVDRV_ADDR_DEVMNG_INFO_MEM_BASE, 0, &shm_addr, &shm_size);
    if (ret != 0) {
        dms_err("Failed to get the device credit num info address. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    credit_info_addr = ka_mm_ioremap(shm_addr, KA_MM_PAGE_SIZE);
    if (credit_info_addr == NULL) {
        dms_err("Remap addr for credit info failed. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }

    ret = memcpy_s(credit_num_info, sizeof(hccs_credit_num), credit_info_addr, sizeof(hccs_credit_num));
    if (ret != 0) {
        dms_err("Failed to read credit num and query cnt. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = -ENOMEM;
    }

    iounmap(credit_info_addr);
    credit_info_addr = NULL;
    return ret;
}

STATIC int dms_get_hccs_credit_num(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret = 0;
    u32 dev_id = in->dev_id;
    hccs_credit_num credit_num_info = {
        .pa_credit_num = {0},
        .query_cnt = 0,
    };
    hccs_credit_info_t info = {
        .credit_num = {0},
        .reserve = {0},
    };

    if (!devdrv_manager_is_pf_device(dev_id)) {
        return -EOPNOTSUPP;
    }

    if (dev_id >= DEVDRV_PF_DEV_MAX_NUM || in->buff_size < sizeof(hccs_credit_info_t)) {
        dms_err("Invalid parameter. (dev_id=%u; buff_size=%u; min_size=%zu)\n",
            dev_id, in->buff_size, sizeof(hccs_credit_info_t));
        return -EINVAL;
    }

    ret = dms_read_hccs_credit_num_from_shm(dev_id, &credit_num_info);
    if (ret != 0) {
        dms_err("Failed to read credit num from shm. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = dms_hccs_credit_num_update_check(dev_id, credit_num_info.query_cnt);
    if (ret != 0) {
        return ret;
    }

    ret = memcpy_s(info.credit_num, sizeof(info.credit_num), credit_num_info.pa_credit_num,
        sizeof(credit_num_info.pa_credit_num));
    if (ret != 0) {
        dms_err("memcpy_s credit_num failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -ENOMEM;
    }

    ret = copy_to_user((void *)(uintptr_t)in->buff, &info, sizeof(hccs_credit_info_t));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    *out_size = sizeof(hccs_credit_info_t);
    return 0;
}

int dms_get_hccs_credit_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    u32 physical_dev_id = UDA_INVALID_UDEVID, vfid = 0;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = {0};
    unsigned int out_size = 0;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if (input->buff == NULL) {
        dms_err("Input buffer is null or buffer size is not valid. (buff_is_null=%d; buff_size=%u)\n",
            (input->buff != NULL), input->buff_size);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }
    output = (struct dms_get_device_info_out *)out;

    ret = uda_devid_to_phy_devid(input->dev_id, &physical_dev_id, &vfid);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id (logical_id=%u; physical_id=%u; ret=%d)\n",
            input->dev_id, physical_dev_id, ret);
        return -EINVAL;
    }
    input->dev_id = physical_dev_id;

    ret = dms_get_hccs_credit_num(input, &out_size);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to get hccs credit num. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }
    output->out_size = out_size;
    return 0;
}

#else

u32 g_credit_num_query_task_id[ASCEND_DIE_NUM_MAX];

#define PA_CREDIT_NUM_LEN 12
#define PA_CREDIT_NUM_MASK 0x0FFF
#define PA_CREDIT_NUM_HIGH_FOUT_BIT_OFFSET 4
#define PA_CREDIT_NUM_HIGH_FOUR_BIT_MASK 0x0F
#define PA_CREDIT_NUM_0_TO_11_BIT 0
#define PA_CREDIT_NUM_12_TO_23_BIT 1
#define PA_CREDIT_NUM_24_TO_35_BIT 2
#define PA_CREDIT_NUM_36_TO_47_BIT 3
STATIC void convert_reg_val_to_credit_num(unsigned int *credit_num, const u32 reg_val_h, const u32 reg_val_l)
{
    u16 reg_val_h_low_four_bit = 0;
    u16 reg_val_l_high_eight_bit = 0;
    credit_num[PA_CREDIT_NUM_0_TO_11_BIT] = reg_val_l & PA_CREDIT_NUM_MASK;
    credit_num[PA_CREDIT_NUM_12_TO_23_BIT] = (reg_val_l >> PA_CREDIT_NUM_LEN) & PA_CREDIT_NUM_MASK;

    reg_val_h_low_four_bit = reg_val_h & PA_CREDIT_NUM_HIGH_FOUR_BIT_MASK;
    reg_val_h_low_four_bit <<= (PA_CREDIT_NUM_LEN - PA_CREDIT_NUM_HIGH_FOUT_BIT_OFFSET);
    reg_val_l_high_eight_bit = (reg_val_l >> (PA_CREDIT_NUM_LEN + PA_CREDIT_NUM_LEN));

    credit_num[PA_CREDIT_NUM_24_TO_35_BIT] = reg_val_h_low_four_bit + reg_val_l_high_eight_bit;
    credit_num[PA_CREDIT_NUM_36_TO_47_BIT] = (reg_val_h >> PA_CREDIT_NUM_HIGH_FOUT_BIT_OFFSET) & PA_CREDIT_NUM_MASK;
}

/*One PA corresponds to four macro ports.
* PA0: macro4~7
* PA1: macro0~3
*/
#define PA_TO_MACRO_NUM 4
STATIC int g_pa0_macro_map[PA_TO_MACRO_NUM] = {4, 5, 6, 7};
STATIC int g_pa1_macro_map[PA_TO_MACRO_NUM] = {0, 1, 2, 3};

STATIC int read_credit_num_by_sec_io_read(u32 dev_id, hccs_credit_num *info)
{
    u32 reg_val_h = 0;
    u32 reg_val_l = 0;
    u32 pa0_credit_num[PA_TO_MACRO_NUM]  = {0};
    u32 pa1_credit_num[PA_TO_MACRO_NUM] = {0};
    int i;
    int ret;
    if (info == NULL || dev_id >= ASCEND_DIE_NUM_MAX ) {
        dms_err("Invalid input. (dev_id=%u; info=%s)\n", dev_id, info == NULL ? "null" : "not null");
        return -EINVAL;
    }

    ret = sec_io_read32(dev_id, HCCS_PA0_BWAY_TX_VNA_H, &reg_val_h);
    if (ret != 0) {
        dms_err("sec_io_read32 for HCCS_PA0_BWAY_TX_VNA_H fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = sec_io_read32(dev_id, HCCS_PA0_BWAY_TX_VNA_L, &reg_val_l);
    if (ret != 0) {
        dms_err("sec_io_read32 for HCCS_PA0_BWAY_TX_VNA_L fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    convert_reg_val_to_credit_num(pa0_credit_num, reg_val_h, reg_val_l);

    ret = sec_io_read32(dev_id, HCCS_PA1_BWAY_TX_VNA_H, &reg_val_h);
    if (ret != 0) {
        dms_err("sec_io_read32 for HCCS_PA1_BWAY_TX_VNA_H fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = sec_io_read32(dev_id, HCCS_PA1_BWAY_TX_VNA_L, &reg_val_l);
    if (ret != 0) {
        dms_err("sec_io_read32 for HCCS_PA1_BWAY_TX_VNA_L fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    convert_reg_val_to_credit_num(pa1_credit_num, reg_val_h, reg_val_l);

    for (i = 0; i < PA_TO_MACRO_NUM; ++i) {
        info->pa_credit_num[g_pa0_macro_map[i]] = pa0_credit_num[i];
        info->pa_credit_num[g_pa1_macro_map[i]] = pa1_credit_num[i];
    }
    return 0;
}

STATIC int write_credit_num_to_shm(u32 dev_id, hccs_credit_num *info)
{
    int ret = 0;
    unsigned long long base_addr = 0;
    void __iomem *credit_num_addr = NULL;
    if (info == NULL || dev_id >= ASCEND_DIE_NUM_MAX) {
        dms_err("Invalid input. (dev_id=%u; info=%s)\n", dev_id, info == NULL ? "null" : "not null");
        return -EINVAL;
    }

    base_addr = hal_kernel_get_dev_phy_base_addr(dev_id, BASE_DEVMNG_INFO_MEM_ADDR);
    if (base_addr == KA_ULLONG_MAX) {
        dms_err("Failed to get sharemem base addr. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    credit_num_addr = ioremap_cache(base_addr, sizeof(hccs_credit_num));
    if (credit_num_addr == NULL) {
        dms_err("Remap addr for credit info failed. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }

    ret = memcpy_s(credit_num_addr, sizeof(hccs_credit_num), info, sizeof(hccs_credit_num));
    if (ret != 0) {
        dms_err("memcpy_s failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = -ENOMEM;
    }

    devdrv_flush_cache((u64)credit_num_addr, sizeof(hccs_credit_num));
    iounmap(credit_num_addr);
    credit_num_addr = NULL;
    return ret;
}

#define CREDIT_NUM_QUERY_TIMER_EXPIRE_MS 12000UL
#define CREDIT_NUM_QUERY_TIMER_EXPIRE_NS (CREDIT_NUM_QUERY_TIMER_EXPIRE_MS * 1000 * 1000)
#define DEFUALT_CREDIT_NUM_L 0xFEFFEFFE
#define DEFUALT_CREDIT_NUM_H 0x0000FFEF
#define TASK_REGISTE_FALIED KA_UINT_MAX
STATIC bool is_need_update_credit_num(void)
{
    static u64 last_update_time = 0;
    u64 new_time;

    new_time = ka_system_ktime_get_raw_ns();
    if ((new_time > last_update_time) && ((new_time - last_update_time) < CREDIT_NUM_QUERY_TIMER_EXPIRE_NS)) {
        return false;
    } else {
        last_update_time = new_time;
        return true;
    }
}
STATIC int credit_num_query_task(u64 user_data)
{
    int ret;
    hccs_credit_num info = {0};
    u32 dev_id = (u32)user_data;
    static u64 g_query_cnt[ASCEND_DIE_NUM_MAX] = {0};

    if (dev_id >= ASCEND_DIE_NUM_MAX) {
        dms_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (!is_need_update_credit_num()) {
        return 0;
    }

    ret = read_credit_num_by_sec_io_read(dev_id, &info);
    if (ret != 0) {
        dms_err("read_credit_num_by_sec_io fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    info.query_cnt = g_query_cnt[dev_id];
    ret = write_credit_num_to_shm(dev_id, &info);
    if (ret != 0) {
        dms_err("write_credit_num_to_shm fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    g_query_cnt[dev_id]++;
    return 0;
}

int dms_hccs_credit_info_task_register(u32 dev_id)
{
    int ret = 0;
    hccs_credit_num info = {0};
    struct dms_timer_task credit_num_query_task_property = {0};

    /* not support vdevice env, return OK. Avoid impacting vdevice register */
    if (!uda_is_phy_dev(dev_id)) {
        return 0;
    }

    if (dev_id >= ASCEND_DIE_NUM_MAX) {
        dms_err("Invalid input. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    convert_reg_val_to_credit_num(info.pa_credit_num, DEFUALT_CREDIT_NUM_H, DEFUALT_CREDIT_NUM_L);
    convert_reg_val_to_credit_num(info.pa_credit_num + PA_TO_MACRO_NUM, DEFUALT_CREDIT_NUM_H, DEFUALT_CREDIT_NUM_L);
    /* Write 0xFFE for each macro to shared memory before the first query. */
    ret = write_credit_num_to_shm(dev_id, &info);
    if (ret != 0) {
        dms_err("write_credit_num_to_shm failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    credit_num_query_task_property.expire_ms = CREDIT_NUM_QUERY_TIMER_EXPIRE_MS;
    credit_num_query_task_property.count_ms = 0;
    credit_num_query_task_property.user_data = dev_id;
    credit_num_query_task_property.handler_mode = COMMON_WORK;
    credit_num_query_task_property.exec_task = credit_num_query_task;
    ret = dms_timer_task_register(&credit_num_query_task_property, &g_credit_num_query_task_id[dev_id]);
    if (ret != 0) {
        dms_err("Dms timer task register failed, (dev_id=%u; ret=%d).\n", dev_id, ret);
        g_credit_num_query_task_id[dev_id] = TASK_REGISTE_FALIED;
        return ret;
    }

    return ret;
}

int dms_hccs_credit_info_task_unregister(u32 dev_id)
{
    int ret = 0;

    /* not support vdevice env, return OK. Avoid impacting vdevice register */
    if (!uda_is_phy_dev(dev_id)) {
        return 0;
    }

    if (dev_id >= ASCEND_DIE_NUM_MAX) {
        dms_err("Invalid input. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (g_credit_num_query_task_id[dev_id] == TASK_REGISTE_FALIED) {
        return 0;
    }

    ret = dms_timer_task_unregister(g_credit_num_query_task_id[dev_id]);
    if (ret != 0) {
        dms_err("Dms timer task unregister failed, (dev_id=%u; ret=%d).\n", dev_id, ret);
        return ret;
    }

    return ret;
}
#endif


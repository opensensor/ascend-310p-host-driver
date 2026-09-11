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
* Create: 2022-10-26
*/

#ifdef CFG_FEATURE_PARTIAL_GOOD

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "dms_define.h"
#include "dms_pg_info.h"
#include "dms_xpc_common.h"
#include "dbl_mem_alloc_interface.h"
#include "drv_systime.h"
#ifdef CFG_SOC_PLATFORM_MDC_V51
#include "kthread_affinity.h"
#endif

#ifdef AOS_LLVM_BUILD
#define READ_CNT_MAX   15 // 1000ms * 15 = 15s
#else
#define SEND_CNT_MAX   (15000) // 1ms * 15000 = 15s
#endif
#define WAITTING_TIME_MIN       1000U     // 最小等待间隔1ms
#define WAITTING_TIME_MAX       1100U     // 最大等待间隔1ms
#define PG_INFO_RETRY_COUNT_MAX     10000       // 1ms * 10000 = 10000ms

static int g_xpc_chl_id = -1;
static struct task_struct *get_pg_info_task = NULL;
static bool g_pg_info_task_run_flag = false;
STATIC all_pg_info g_pg_info = {0};

STATIC int dms_pg_dtsi_check(void)
{
    int ret;
    struct cpu_domain_info info = {0};
    u32 bit_map = (u32)g_pg_info.pg_info[0].com_pg_info[0].bitMapIdx;  // bitMapIdx indicates which cpu cores are good
    u32 valid_cpu_num = hweight32(bit_map);

    ret = get_cpudomain_info(&info);
    if (ret != 0) {
        dms_err("Dms get dtsi config from get_cpudomain_info failed. (ret=%d)\n", ret);
        return -1;
    }

    if (valid_cpu_num < info.aicpu_num + info.ctrlcpu_num + info.datacpu_num) {
        dms_err("valid_cpu_num is less than aicpu_num + ctrlcpu_num + datacpu_num, failed."
                "(valid_cpu_num=%u; aicpu_num=%u; ctrlcpu_num=%u; datacpu_num=%u)\n",
            valid_cpu_num, info.aicpu_num, info.ctrlcpu_num, info.datacpu_num);
        return -1;
    }
    dms_info("PG config can support DTSi config. (valid_cpu_num=%u; aicpu_num=%u; ctrlcpu_num=%u; datacpu_num=%u)\n",
        valid_cpu_num, info.aicpu_num, info.ctrlcpu_num, info.datacpu_num);
    return 0;
}

#ifdef AOS_LLVM_BUILD
STATIC int dms_xpc_recv_pg_info(char *arr, u32 length)
{
    int ret;
    xpc_msg msg;

    ret = memcpy_s(&msg, sizeof(msg), arr, length);
    if (ret != 0) {
        dms_err("Memcpy_s arr to msg failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    ret = memcpy_s(&g_pg_info, sizeof(all_pg_info), &msg.a_pg_info, sizeof(all_pg_info));
    if (ret != 0) {
        dms_err("Memcpy_s msg to g_pg_info failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}
#endif

#ifndef AOS_LLVM_BUILD
STATIC int dms_get_pg_info_from_tee(u32 dev_id, pg_cmd_data cmd, u64 *out_arg)
{
    int ret;

    ret = sec_read_pg_info(dev_id, cmd, out_arg);
    if (ret != 0) {
        dms_err("Call sec_read_pg_info failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
#endif

STATIC int dms_get_pg_info_from_global(u32 dev_id, pg_cmd_data cmd, u64 *out_arg)
{
    unsigned int i;

    for (i = 0; i < PG_MODULE_TYPE_ALL; i++) {
        if (cmd.module == i) {
            if (cmd.data == PG_DATA_TYPE_FREQ) {
                *out_arg = g_pg_info.pg_info[dev_id].com_pg_info[i].freq;
                return 0;
            } else if (cmd.data == PG_DATA_TYPE_CORE_MAP) {
                *out_arg = g_pg_info.pg_info[dev_id].com_pg_info[i].bitMapIdx;
                return 0;
            } else if (cmd.data == PG_DATA_TYPE_TOTAL_NUM) {
                *out_arg = g_pg_info.pg_info[dev_id].com_pg_info[i].totalNum;
                return 0;
            }
        }
    }
    return -1;
}

STATIC int dms_get_pg_info_para_check(u32 dev_id, pg_cmd_data cmd, u64 *out_arg)
{
    if (dev_id != 0) {
        dms_err("Dev_id is invalid. (dev_id=%u)\n", dev_id);
        return -ENXIO;
    }
    if (cmd.data > PG_DATA_TYPE_MAX || cmd.module >= PG_MODULE_TYPE_ALL) {
        dms_err("Dms get pg info input cmd is invalid. (module_type=%u; data_type=%u)\n", cmd.module, cmd.data);
        return -EINVAL;
    }
    if (out_arg == NULL) {
        dms_err("Dms get pg info out_arg is NULL.\n");
        return -EINVAL;
    }
    return 0;
}

int dms_get_pg_info(u32 dev_id, pg_cmd_data cmd, u64 *out_arg)
{
    int ret;
#ifdef AOS_LLVM_BUILD
    u32 retry_cnt = 0;
#endif

    ret = dms_get_pg_info_para_check(dev_id, cmd, out_arg);
    if (ret != 0) {
        dms_err("Dms get pg_info para check failed. (ret=%d)\n", ret);
        return ret;
    }

    while (true) {
        if (g_pg_info.pg_info[dev_id].magic == PG_INFO_MAGIC) {
            ret = dms_get_pg_info_from_global(dev_id, cmd, out_arg);
            if (ret != 0) {
                dms_err("Get pg_info in g_pg_info failed. (ret=%d; dev_id=%u)\n", ret, dev_id);
                return ret;
            }
            return 0;
        } else {
#ifndef AOS_LLVM_BUILD
            ret = dms_get_pg_info_from_tee(dev_id, cmd, out_arg);
            if (ret != 0) {
                dms_err("Dms get pg_info from tee failed. (ret=%d)\n", ret);
                return ret;
            }
            dms_info("Get pg_info from tee success. (cmd.module=%u; cmd.data=%u; syscnt=%llu)\n",
                cmd.module, cmd.data, get_syscnt());
            return 0;
#else
            usleep_range(WAITTING_TIME_MIN, WAITTING_TIME_MAX);
            if (retry_cnt++ >= PG_INFO_RETRY_COUNT_MAX) {
                dms_err("Retry_count reached the upper limit, g_pg_info init failed. (cmd.module=%u; cmd.data=%u; "
                        "retry_cnt=%u)\n", cmd.module, cmd.data, retry_cnt);
                return -1;
            }
#endif
        }
    }
}
KA_EXPORT_SYMBOL(dms_get_pg_info);

#ifndef AOS_LLVM_BUILD
STATIC int dms_xpc_kernel_send_pg_info(int chl_id)
{
    int ret;
    u32 send_cnt = 0;
    xpc_msg *send_data = NULL;

    send_data = (xpc_msg *)dbl_kzalloc(sizeof(xpc_msg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (send_data == NULL) {
        dms_err("Kzalloc msg.data_arr failed. (size=%ld)\n", sizeof(xpc_msg));
        return -ENOMEM;
    }

    send_data->serv_id = 0;
    send_data->msg_type = 0;
    send_data->length = sizeof(all_pg_info);
    send_data->check_sum = 0; // reserve

    ret = memcpy_s(&send_data->a_pg_info, sizeof(all_pg_info), &g_pg_info, sizeof(all_pg_info));
    if (ret != 0) {
        dms_err("Memcpy_s g_pg_info to send_data failed. (ret=%d)\n", ret);
        dbl_kfree(send_data);
        return -EINVAL;
    }

    ret = dms_xpc_send(chl_id, (const char *)send_data, sizeof(xpc_msg));
    while ((ret != 0) && (ret != -EINVAL)) {
        usleep_range(WAITTING_TIME_MIN, WAITTING_TIME_MAX);
        ret = dms_xpc_send(chl_id, (const char *)send_data, sizeof(xpc_msg));
        if (send_cnt++ == SEND_CNT_MAX) {
            dbl_kfree(send_data);
            dms_err("Dms dms_xpc_send failed. (ret=%d; send_cnt=%u)\n", ret, send_cnt);
            return -1;
        }
    }

    dbl_kfree(send_data);
    return ret;
}

STATIC int dms_read_pg_info_from_tee(u32 dev_id, module_type module, common_pg_info *com_pg_info)
{
    int ret;
    pg_cmd_data cmd;
    u64 query_cmd_result = 0;

    cmd.module = module;
    cmd.data = PG_DATA_TYPE_FREQ;
    ret = sec_read_pg_info(dev_id, cmd, &query_cmd_result);
    if (ret != 0) {
        dms_err("Call sec_read_pg_info failed. (ret=%d; cmd.module=%u; cmd.data=%u)\n", ret, cmd.module, cmd.data);
        return ret;
    }
    com_pg_info->freq = query_cmd_result;

    cmd.data = PG_DATA_TYPE_TOTAL_NUM;
    ret = sec_read_pg_info(dev_id, cmd, &query_cmd_result);
    if (ret != 0) {
        dms_err("Call sec_read_pg_info failed. (ret=%d; cmd.module=%u; cmd.data=%u)\n", ret, cmd.module, cmd.data);
        return ret;
    }
    com_pg_info->totalNum = query_cmd_result;

    cmd.data = PG_DATA_TYPE_CORE_MAP;
    ret = sec_read_pg_info(dev_id, cmd, &query_cmd_result);
    if (ret != 0) {
        dms_err("Call sec_read_pg_info failed. (ret=%d; cmd.module=%u; cmd.data=%u)\n", ret, cmd.module, cmd.data);
        return ret;
    }
    com_pg_info->bitMapIdx = query_cmd_result;

    return 0;
}

STATIC int dms_get_all_pg_info_from_tee(void)
{
    int ret;
    int i;
    int j;
    int dev_num = 1;

    for (i = 0; i < dev_num; i++) {
        for (j = 0; j < PG_MODULE_TYPE_ALL; j++) {
            ret = dms_read_pg_info_from_tee(i, j, &g_pg_info.pg_info[i].com_pg_info[j]);
            if (ret != 0) {
                dms_err("Get g_pg_info.pg_info[%d].common_pg_info[%d] failed.\n", i, j);
                return ret;
            }
        }
        g_pg_info.pg_info[i].magic = PG_INFO_MAGIC;
    }

    return 0;
}
#endif

STATIC int dms_pg_info_thread(void *arg)
{
    int ret;
    g_pg_info.pg_info[0].start_time = get_syscnt();

#ifndef AOS_LLVM_BUILD
    ret = dms_get_all_pg_info_from_tee();
    if (ret != 0) {
        dms_err("Dms get pg_info from tee failed. (ret=%d)\n", ret);
        g_pg_info_task_run_flag = false;
        return ret;
    }
    (void)dms_pg_dtsi_check();
#endif
    ret = dms_xpc_open_channel(&g_xpc_chl_id);
    if (ret == -EOPNOTSUPP) {
        g_pg_info_task_run_flag = false;
        return 0;
    } else if (ret != 0) {
        dms_err("Dms open xpc channel for pg_info failed. (ret=%d)\n", ret);
        g_pg_info_task_run_flag = false;
        return -1;
    }

#ifndef AOS_LLVM_BUILD
    ret = dms_xpc_kernel_send_pg_info(g_xpc_chl_id);
    if (ret != 0) {
        dms_err("Dms xpc kernel send pg info failed. (ret=%d)\n", ret);
        g_pg_info_task_run_flag = false;
        return ret;
    }
    dms_info("Dms xpc kernel send pg_info success. (channel_id=%d; sys_cnt=%llu)\n", g_xpc_chl_id, get_syscnt());
#else
    struct chl_poll_ret pollret[1];
    u32 read_cnt = 0;

    while (!kthread_should_stop()) {
        ret = dms_xpc_read_msg(g_xpc_chl_id, pollret, dms_xpc_recv_pg_info);
        if (ret == 0) {
            break;
        } else if ((ret != XPC_ERR_POLL_TIMEOUT) && (ret != XPC_ERR_POLL_CANCELED_BY_OTHERS)) {
            dms_err("Dms xpc poll pg_info failed. (ret=%u)\n", ret);
            g_pg_info_task_run_flag = false;
            return -1;
        } else if (read_cnt++ >= READ_CNT_MAX) {
            dms_err("Dms read pg_info failed. (ret=%d; read_cnt=%u)\n", ret, read_cnt);
            g_pg_info_task_run_flag = false;
            return -1;
        }
    }
    dms_info("Dms core recv pg_info success. (channel_id=%d; read_cnt=%u; sys_cnt=%llu)\n",
        g_xpc_chl_id, read_cnt, get_syscnt());
    dms_pg_dtsi_check();
#endif
    dms_info("Dms run pg_info_thread success. (start_syscnt=%llu)\n", g_pg_info.pg_info[0].start_time);
    g_pg_info_task_run_flag = false;
    return 0;
}

int init_pg_info_thread(void)
{
    if (get_pg_info_task != NULL) {
        dms_warn("Thread for get pg info has created.\n");
        return 0;
    }

    get_pg_info_task = kthread_create(dms_pg_info_thread, (void *)NULL, "pg_info_thread");
    if (IS_ERR(get_pg_info_task)) {
        dms_err("Create thread for get pg info failed.\n");
        return -EINVAL;
    }

#ifdef CFG_SOC_PLATFORM_MDC_V51
    kthread_bind_to_ctrl_cpu(get_pg_info_task);
#endif
    g_pg_info_task_run_flag = true;
    (void)wake_up_process(get_pg_info_task);
    dms_info("Dms init dms_pg_info_thread success.\n");

    return 0;
}

void uninit_pg_info_thread(void)
{
    if ((!IS_ERR(get_pg_info_task)) && (g_pg_info_task_run_flag == true)) {
        (void)kthread_stop(get_pg_info_task);
        get_pg_info_task = NULL;
    }
    return;
}

#endif


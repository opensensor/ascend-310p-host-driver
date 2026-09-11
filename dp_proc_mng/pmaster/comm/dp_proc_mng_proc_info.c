/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#include <securec.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/mutex.h>

#include "davinci_api.h"
#include "dp_proc_mng_cmd.h"
#include "dp_proc_mng_log.h"
#include "dp_proc_mng_channel.h"
#include "dp_proc_mng_msg_client.h"
#include "dp_proc_mng_alloc_interface.h"
#include "dp_proc_mng_mem_sharing.h"
#include "dp_proc_mng_proc_info.h"

/*
 * dp_proc_mng module init
 */
STATIC struct dp_proc_mng_info *dp_proc_mng = NULL;

struct dp_proc_mng_info *dp_proc_get_manager_info(void)
{
    return dp_proc_mng;
}

int dp_proc_mng_info_init(void)
{
    dp_proc_mng = dp_proc_kzalloc(sizeof(struct dp_proc_mng_info), GFP_KERNEL);
    if (dp_proc_mng == NULL) {
        dp_proc_mng_drv_err("dp_proc_kzalloc return NULL, failed to alloc mem for manager struct.\n");
        return -ENOMEM;
    }

    return 0;
}

void dp_proc_mng_info_unint(void)
{
    dp_proc_kfree(dp_proc_mng);
    dp_proc_mng = NULL;
}

int dp_proc_mng_davinci_module_init(const struct file_operations *ops)
{
    int ret;

    ret = drv_davinci_register_sub_module(DAVINCI_DP_PROC_MNG_SUB_MODULE_NAME, ops);
    if (ret != 0) {
        dp_proc_mng_drv_err("Register sub module failed. (ret=%d)\n", ret);
        return -ENODEV;
    }
    return 0;
}

void dp_proc_mng_davinci_module_uninit(void)
{
    int ret;

    ret = drv_ascend_unregister_sub_module(DAVINCI_DP_PROC_MNG_SUB_MODULE_NAME);
    if (ret != 0) {
        dp_proc_mng_drv_err("Unregister sub module failed. (ret=%d)\n", ret);
        return;
    }
    return;
}

static void dp_proc_mng_mem_stats_info_pack(struct dp_proc_mng_get_mem_stats *para,
    struct dp_proc_mng_chan_mem_stats *msg)
{
    para->mbuff_used_size = msg->mbuff_used_size;
    para->aicpu_used_size = msg->aicpu_used_size;
    para->custom_used_size = msg->custom_used_size;
    para->hccp_used_size = msg->hccp_used_size;
}

static int dp_proc_mng_ioctl_get_mem_stats(struct file *file, struct dp_proc_mng_ioctl_arg *arg)
{
    struct dp_proc_mng_chan_mem_stats mem_stats_msg = {{{0}}};
    int ret;

    mem_stats_msg.head.process_id.hostpid = current->tgid;
    mem_stats_msg.head.process_id.devid = arg->head.devid;
    mem_stats_msg.head.process_id.vfid = arg->head.vfid;
    mem_stats_msg.head.msg_id = DP_PROC_MNG_CHAN_QUERY_MEM_STATS_H2D_ID;
    ret = dp_proc_mng_common_msg_send(&mem_stats_msg, sizeof(struct dp_proc_mng_chan_mem_stats),
        sizeof(struct dp_proc_mng_chan_mem_stats));
    if (ret != 0) {
        dp_proc_mng_drv_err("Dp_proc_mng_query_mem_stats msg failed! (ret=%d)\n", ret);
        return ret;
    }

    dp_proc_mng_mem_stats_info_pack(&arg->data.get_mem_stats_para, &mem_stats_msg);
    return 0;
}

int (* const dp_proc_mng_exit_handlers)(struct dp_proc_mng_ctx *proc_ctx) = dp_proc_mng_exit_mem_sharing;

int (* const dp_proc_mng_ioctl_handlers[DP_PROC_MNG_CMD_MAX_CMD])(struct file *file,
    struct dp_proc_mng_ioctl_arg *arg) = {
        [_KA_IOC_NR(DP_PROC_MNG_GET_MEM_STATS)] = dp_proc_mng_ioctl_get_mem_stats,
        [_KA_IOC_NR(DP_PROC_MNG_MEM_SHARING)] = dp_proc_mng_ioctl_mem_sharing,
};

int dp_proc_mng_create_work(void)
{
    return 0;
}

void dp_proc_mng_destroy_work(void)
{
}


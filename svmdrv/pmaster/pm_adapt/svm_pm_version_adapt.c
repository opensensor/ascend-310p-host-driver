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
 * Create: 2022-09-13
 */

#include "devmm_pm_vpc.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "devmm_vpc_mng_msg_def.h"
#include "svm_version_adapt.h"

#include "virtmng_interface.h"
#include "svm_pm_version_adapt.h"

u32 g_vm_version[DEVMM_VM_MAX_NUM] = {0};

int devmm_pm_sync_version(u32 vm_id, struct vmng_rx_msg_proc_info *proc_info)
{
    struct devmm_vpc_mng_msg_sync_version *info = (struct devmm_vpc_mng_msg_sync_version *)proc_info->data;

    if (proc_info->in_data_len < sizeof(struct devmm_vpc_mng_msg_sync_version)) {
        devmm_drv_err("Invalid in_data_len. (in_data_len=%u)\n", proc_info->in_data_len);
#ifndef EMU_ST
        return -EINVAL;
#endif
    }

    g_vm_version[vm_id] = info->vm_version;
    info->pm_version = SVM_VERSION;
    *proc_info->real_out_len = proc_info->in_data_len;
    return 0;
}

void devmm_pm_clear_vm_version(u32 vm_id)
{
    g_vm_version[vm_id] = 0;
}
#ifndef EMU_ST
bool devmm_pm_is_need_version_adapt(u32 vm_id)
{
    return (g_vm_version[vm_id] < SVM_VERSION);
}

bool devmm_pm_is_need_return_head_result(u32 vm_id)
{
    return (g_vm_version[vm_id] >= SVM_VERSION_0003);
}

static int devmm_pm_ver_adapt_create_dev_pre_proc(u32 vm_id, void *msg)
{
    struct devmm_vpc_mng_msg_create_dev_page *create_msg = (struct devmm_vpc_mng_msg_create_dev_page *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&create_msg->bitmap, g_vm_version[vm_id], &dst_bitmap, SVM_VERSION);
    create_msg->bitmap = dst_bitmap;
    return 0;
}

static int devmm_pm_ver_adapt_copy_mem_pre_proc(u32 vm_id, void *msg)
{
    struct devmm_vpc_mng_msg_mem_copy *copy_msg = (struct devmm_vpc_mng_msg_mem_copy *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&copy_msg->src_attr.bitmap, g_vm_version[vm_id], &dst_bitmap, SVM_VERSION);
    copy_msg->src_attr.bitmap = dst_bitmap;
    devmm_page_bitmap_version_adapt(&copy_msg->dst_attr.bitmap, g_vm_version[vm_id], &dst_bitmap, SVM_VERSION);
    copy_msg->dst_attr.bitmap = dst_bitmap;
    return 0;
}

static int devmm_pm_ver_adapt_convert_addr_pre_proc(u32 vm_id, void *msg)
{
    struct devmm_vpc_mng_msg_convert_addr *convert_msg = (struct devmm_vpc_mng_msg_convert_addr *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&convert_msg->src_attr.bitmap, g_vm_version[vm_id], &dst_bitmap, SVM_VERSION);
    convert_msg->src_attr.bitmap = dst_bitmap;
    devmm_page_bitmap_version_adapt(&convert_msg->dst_attr.bitmap, g_vm_version[vm_id], &dst_bitmap, SVM_VERSION);
    convert_msg->dst_attr.bitmap = dst_bitmap;
    return 0;
}

static int (*pm_mng_rx_version_adapt_pre_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (u32 vm_id, void *msg) = {
    [DEVMM_VPC_MNG_CREATE_DEV_PAGE] = devmm_pm_ver_adapt_create_dev_pre_proc,
    [DEVMM_VPC_MNG_COPY_MEM] = devmm_pm_ver_adapt_copy_mem_pre_proc,
    [DEVMM_VPC_MNG_CONVERT_ADDR] = devmm_pm_ver_adapt_convert_addr_pre_proc,
};

static int (*pm_mng_rx_version_adapt_post_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (u32 vm_id, void *msg) = {

};

int devmm_pm_mng_rx_version_adapt_pre_proc(u32 vm_id, void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_mng_rx_version_adapt_pre_proc[msg_id] != NULL) {
        return pm_mng_rx_version_adapt_pre_proc[msg_id](vm_id, msg);
    }

    return 0;
}

int devmm_pm_mng_rx_version_adapt_post_proc(u32 vm_id, void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_mng_rx_version_adapt_post_proc[msg_id] != NULL) {
        return pm_mng_rx_version_adapt_post_proc[msg_id](vm_id, msg);
    }

    return 0;
}

static int devmm_pm_ver_adapt_setup_dev_pre_proc(u32 vm_id, void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)msg;

    /* For safety, if vm_version isn't match with msg_len, r/w msg will out of range */
    if (msg_len < devmm_get_setup_dev_msg_len(g_vm_version[vm_id], head->extend_num)) {
        devmm_drv_err("Unexpected, msg_len is smaller. (msg_len=%llu; expected_len=%llu)\n",
            msg_len, devmm_get_setup_dev_msg_len(g_vm_version[vm_id], head->extend_num));
        return -EINVAL;
    }

    new_msg_info->out_len = devmm_get_setup_dev_msg_len(SVM_VERSION, 0);
    new_msg_info->msg_len = devmm_get_setup_dev_msg_len(SVM_VERSION, head->extend_num);
    new_msg_info->msg = devmm_kvzalloc(new_msg_info->msg_len);
    if (new_msg_info->msg == NULL) {
        devmm_drv_err("Kmalloc failed.\n");
        return -ENOMEM;
    }

    devmm_setup_dev_msg_version_adapt(msg, g_vm_version[vm_id], new_msg_info->msg, SVM_VERSION);
    return 0;
}

static int devmm_pm_ver_adapt_setup_dev_post_proc(u32 vm_id, void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)msg;

    /* For safety, if vm_version isn't match with msg_len, r/w msg will out of range */
    if (msg_len < devmm_get_setup_dev_msg_len(g_vm_version[vm_id], head->extend_num)) {
        devmm_drv_err("Unexpected, msg_len is smaller. (msg_len=%llu; expected_len=%llu)\n",
            msg_len, devmm_get_setup_dev_msg_len(g_vm_version[vm_id], head->extend_num));
        return -EINVAL;
    }

    devmm_setup_dev_msg_version_adapt(new_msg_info->msg, SVM_VERSION, msg, g_vm_version[vm_id]);
    devmm_kvfree(new_msg_info->msg);
    new_msg_info->msg = NULL;
    return 0;
}

static int (*pm_rx_version_adapt_pre_proc[DEVMM_CHAN_MAX_ID])
    (u32 vm_id, void *msg, u64 msg_len, struct svm_ver_adapt_msg_info *new_msg_info) = {
    [DEVMM_CHAN_SETUP_DEVICE_H2D] = devmm_pm_ver_adapt_setup_dev_pre_proc,
};

static int (*pm_rx_version_adapt_post_proc[DEVMM_CHAN_MAX_ID])
    (u32 vm_id, void *msg, u64 msg_len, struct svm_ver_adapt_msg_info *new_msg_info) = {
    [DEVMM_CHAN_SETUP_DEVICE_H2D] = devmm_pm_ver_adapt_setup_dev_post_proc,
};

int devmm_pm_rx_version_adapt_pre_proc(u32 vm_id, void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_rx_version_adapt_pre_proc[msg_id] != NULL) {
        return pm_rx_version_adapt_pre_proc[msg_id](vm_id, msg, msg_len, new_msg_info);
    }

    return 0;
}

int devmm_pm_rx_version_adapt_post_proc(u32 vm_id, void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_rx_version_adapt_post_proc[msg_id] != NULL) {
        return pm_rx_version_adapt_post_proc[msg_id](vm_id, msg, msg_len, new_msg_info);
    }

    return 0;
}

static int (*pm_mng_tx_version_adapt_pre_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (u32 vm_id, void *msg) = {
};

static int (*pm_mng_tx_version_adapt_post_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (u32 vm_id, void *msg) = {
};

int devmm_pm_mng_tx_version_adapt_pre_proc(u32 vm_id, void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_mng_tx_version_adapt_pre_proc[msg_id] != NULL) {
        return pm_mng_tx_version_adapt_pre_proc[msg_id](vm_id, msg);
    }

    return 0;
}

int devmm_pm_mng_tx_version_adapt_post_proc(u32 vm_id, void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_mng_tx_version_adapt_post_proc[msg_id] != NULL) {
        return pm_mng_tx_version_adapt_post_proc[msg_id](vm_id, msg);
    }

    return 0;
}

static int devmm_pm_ver_adapt_query_vaflags_post_proc(u32 vm_id, void *msg)
{
    struct devmm_chan_page_query *query_msg = (struct devmm_chan_page_query *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&query_msg->bitmap, g_vm_version[vm_id], &dst_bitmap, SVM_VERSION);
    query_msg->bitmap = dst_bitmap;
    return 0;
}

static int devmm_pm_ver_adapt_check_va_post_proc(u32 vm_id, void *msg)
{
    struct devmm_chan_check_va *check_va_msg = (struct devmm_chan_check_va *)msg;
    u32 src_bitmap = (u32)check_va_msg->bitmap;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&src_bitmap, g_vm_version[vm_id], &dst_bitmap, SVM_VERSION);
    check_va_msg->bitmap = dst_bitmap;
    return 0;
}

static int (*pm_tx_version_adapt_pre_proc[DEVMM_CHAN_MAX_ID])
    (u32 vm_id, void *msg) = {
};

static int (*pm_tx_version_adapt_post_proc[DEVMM_CHAN_MAX_ID])
    (u32 vm_id, void *msg) = {
    [DEVMM_CHAN_QUERY_VAFLGS_D2H_ID] = devmm_pm_ver_adapt_query_vaflags_post_proc,
    [DEVMM_CHAN_CHECK_VA_D2H_ID] = devmm_pm_ver_adapt_check_va_post_proc,
};

int devmm_pm_tx_version_adapt_pre_proc(u32 vm_id, void *msg)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_tx_version_adapt_pre_proc[msg_id] != NULL) {
        return pm_tx_version_adapt_pre_proc[msg_id](vm_id, msg);
    }

    return 0;
}

int devmm_pm_tx_version_adapt_post_proc(u32 vm_id, void *msg)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_pm_is_need_version_adapt(vm_id) == false) {
        return 0;
    }

    if (pm_tx_version_adapt_post_proc[msg_id] != NULL) {
        return pm_tx_version_adapt_post_proc[msg_id](vm_id, msg);
    }

    return 0;
}
#endif


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

#include "devmm_vm_vpc.h"
#include "devmm_vpc_mng_msg_def.h"
#include "devmm_channel.h"
#include "devmm_common.h"

#include "svm_version_adapt.h"
#include "svm_vm_version_adapt.h"

u32 g_pm_version = 0;

void devmm_vm_sync_version(u32 devid)
{
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_sync_version);
    struct devmm_vpc_mng_msg_sync_version info = {{{0}}};
    static bool flag = false;
    int ret;

    if (flag == true) {
        return;
    }

    info.head.dev_id = devid;
    info.head.msg_id = DEVMM_VPC_SYNC_VERSION;
    info.vm_version = SVM_VERSION;
    ret = devmm_vm_vpc_mng_msg_send(&info, len, len);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_info("Check vpc or pm is orginal version. (ret=%d)\n", ret);
        flag = true;
        return;
#endif
    }

    flag = true;
    g_pm_version = info.pm_version;
}

#ifndef EMU_ST
bool devmm_vm_is_need_version_adapt(void)
{
    return (g_pm_version < SVM_VERSION);
}

bool devmm_vm_is_need_return_head_result(void)
{
    return (g_pm_version >= SVM_VERSION_0003);
}

static int (*vm_mng_rx_version_adapt_pre_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (void *msg) = {
};

static int (*vm_mng_rx_version_adapt_post_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (void *msg) = {
};

int devmm_vm_mng_rx_version_adapt_pre_proc(void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_mng_rx_version_adapt_pre_proc[msg_id] != NULL) {
        return vm_mng_rx_version_adapt_pre_proc[msg_id](msg);
    }

    return 0;
}

int devmm_vm_mng_rx_version_adapt_post_proc(void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_mng_rx_version_adapt_post_proc[msg_id] != NULL) {
        return vm_mng_rx_version_adapt_post_proc[msg_id](msg);
    }

    return 0;
}

static int devmm_vm_ver_adapt_query_vaflags_post_proc(void *msg)
{
    struct devmm_chan_page_query *query_msg = (struct devmm_chan_page_query *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&query_msg->bitmap, SVM_VERSION, &dst_bitmap, g_pm_version);
    query_msg->bitmap = dst_bitmap;
    return 0;
}

static int devmm_vm_ver_adapt_check_va_post_proc(void *msg)
{
    struct devmm_chan_check_va *check_va_msg = (struct devmm_chan_check_va *)msg;
    u32 src_bitmap = (u32)check_va_msg->bitmap;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&src_bitmap, SVM_VERSION, &dst_bitmap, g_pm_version);
    check_va_msg->bitmap = dst_bitmap;
    return 0;
}

static int (*vm_rx_version_adapt_pre_proc[DEVMM_CHAN_MAX_ID])
    (void *msg) = {
};

static int (*vm_rx_version_adapt_post_proc[DEVMM_CHAN_MAX_ID])
    (void *msg) = {
        [DEVMM_CHAN_QUERY_VAFLGS_D2H_ID] = devmm_vm_ver_adapt_query_vaflags_post_proc,
        [DEVMM_CHAN_CHECK_VA_D2H_ID] = devmm_vm_ver_adapt_check_va_post_proc,
};

int devmm_vm_rx_version_adapt_pre_proc(void *msg)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_rx_version_adapt_pre_proc[msg_id] != NULL) {
        return vm_rx_version_adapt_pre_proc[msg_id](msg);
    }

    return 0;
}

int devmm_vm_rx_version_adapt_post_proc(void *msg)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_rx_version_adapt_post_proc[msg_id] != NULL) {
        return vm_rx_version_adapt_post_proc[msg_id](msg);
    }

    return 0;
}

static int devmm_vm_ver_adapt_create_dev_pre_proc(void *msg)
{
    struct devmm_vpc_mng_msg_create_dev_page *create_msg = (struct devmm_vpc_mng_msg_create_dev_page *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&create_msg->bitmap, SVM_VERSION, &dst_bitmap, g_pm_version);
    create_msg->bitmap = dst_bitmap;
    return 0;
}

static int devmm_vm_ver_adapt_copy_mem_pre_proc(void *msg)
{
    struct devmm_vpc_mng_msg_mem_copy *copy_msg = (struct devmm_vpc_mng_msg_mem_copy *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&copy_msg->src_attr.bitmap, SVM_VERSION, &dst_bitmap, g_pm_version);
    copy_msg->src_attr.bitmap = dst_bitmap;
    devmm_page_bitmap_version_adapt(&copy_msg->dst_attr.bitmap, SVM_VERSION, &dst_bitmap, g_pm_version);
    copy_msg->dst_attr.bitmap = dst_bitmap;
    return 0;
}

static int devmm_vm_ver_adapt_convert_addr_pre_proc(void *msg)
{
    struct devmm_vpc_mng_msg_convert_addr *convert_msg = (struct devmm_vpc_mng_msg_convert_addr *)msg;
    u32 dst_bitmap = 0;

    devmm_page_bitmap_version_adapt(&convert_msg->src_attr.bitmap, SVM_VERSION, &dst_bitmap, g_pm_version);
    convert_msg->src_attr.bitmap = dst_bitmap;
    devmm_page_bitmap_version_adapt(&convert_msg->dst_attr.bitmap, SVM_VERSION, &dst_bitmap, g_pm_version);
    convert_msg->dst_attr.bitmap = dst_bitmap;
    return 0;
}

static int (*vm_mng_tx_version_adapt_pre_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (void *msg) = {
    [DEVMM_VPC_MNG_CREATE_DEV_PAGE] = devmm_vm_ver_adapt_create_dev_pre_proc,
    [DEVMM_VPC_MNG_COPY_MEM] = devmm_vm_ver_adapt_copy_mem_pre_proc,
    [DEVMM_VPC_MNG_CONVERT_ADDR] = devmm_vm_ver_adapt_convert_addr_pre_proc,
};

static int (*vm_mng_tx_version_adapt_post_proc[DEVMM_VPC_MNG_MSG_MAX_ID])
    (void *msg) = {
};

int devmm_vm_mng_tx_version_adapt_pre_proc(void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_mng_tx_version_adapt_pre_proc[msg_id] != NULL) {
        return vm_mng_tx_version_adapt_pre_proc[msg_id](msg);
    }

    return 0;
}

int devmm_vm_mng_tx_version_adapt_post_proc(void *msg)
{
    struct devmm_vpc_mng_msg_head *msg_head = (struct devmm_vpc_mng_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_mng_tx_version_adapt_post_proc[msg_id] != NULL) {
        return vm_mng_tx_version_adapt_post_proc[msg_id](msg);
    }

    return 0;
}

static int devmm_vm_ver_adapt_setup_dev_pre_proc(void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)msg;

    /* For reliability, if pm_version isn't match with msg_len, r/w msg will out of range */
    if (msg_len < devmm_get_setup_dev_msg_len(g_pm_version, head->extend_num)) {
        devmm_drv_err("Unexpected, msg_len is smaller. (msg_len=%llu; expected_len=%llu)\n",
            msg_len, devmm_get_setup_dev_msg_len(g_pm_version, head->extend_num));
        return -EINVAL;
    }

    new_msg_info->out_len = devmm_get_setup_dev_msg_len(g_pm_version, 0);
    new_msg_info->msg_len = devmm_get_setup_dev_msg_len(g_pm_version, head->extend_num);
    new_msg_info->msg = devmm_kvzalloc(new_msg_info->msg_len);
    if (new_msg_info->msg == NULL) {
        devmm_drv_err("Kmalloc failed.\n");
        return -ENOMEM;
    }

    devmm_setup_dev_msg_version_adapt(msg, SVM_VERSION, new_msg_info->msg, g_pm_version);
    return 0;
}

static int devmm_vm_ver_adapt_setup_dev_post_proc(void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *head = (struct devmm_chan_msg_head *)msg;

    /* For reliability, if pm_version isn't match with msg_len, r/w msg will out of range */
    if (msg_len < devmm_get_setup_dev_msg_len(g_pm_version, head->extend_num)) {
        devmm_drv_err("Unexpected, msg_len is smaller. (msg_len=%llu; expected_len=%llu)\n",
            msg_len, devmm_get_setup_dev_msg_len(g_pm_version, head->extend_num));
        return -EINVAL;
    }

    devmm_setup_dev_msg_version_adapt(new_msg_info->msg, g_pm_version, msg, SVM_VERSION);
    devmm_kvfree(new_msg_info->msg);
    new_msg_info->msg = NULL;
    return 0;
}

static int (*vm_tx_version_adapt_pre_proc[DEVMM_CHAN_MAX_ID])
    (void *msg, u64 msg_len, struct svm_ver_adapt_msg_info *new_msg_info) = {
    [DEVMM_CHAN_SETUP_DEVICE_H2D] = devmm_vm_ver_adapt_setup_dev_pre_proc,
};

static int (*vm_tx_version_adapt_post_proc[DEVMM_CHAN_MAX_ID])
    (void *msg, u64 msg_len, struct svm_ver_adapt_msg_info *new_msg_info) = {
    [DEVMM_CHAN_SETUP_DEVICE_H2D] = devmm_vm_ver_adapt_setup_dev_post_proc,
};

int devmm_vm_tx_version_adapt_pre_proc(void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_tx_version_adapt_pre_proc[msg_id] != NULL) {
        return vm_tx_version_adapt_pre_proc[msg_id](msg, msg_len, new_msg_info);
    }

    return 0;
}

int devmm_vm_tx_version_adapt_post_proc(void *msg, u64 msg_len,
    struct svm_ver_adapt_msg_info *new_msg_info)
{
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    u32 msg_id = msg_head->msg_id;

    if (devmm_vm_is_need_version_adapt() == false) {
        return 0;
    }

    if (vm_tx_version_adapt_post_proc[msg_id] != NULL) {
        return vm_tx_version_adapt_post_proc[msg_id](msg, msg_len, new_msg_info);
    }

    return 0;
}
#endif


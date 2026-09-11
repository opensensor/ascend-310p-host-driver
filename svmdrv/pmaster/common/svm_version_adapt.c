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

#include <linux/types.h>

#include "devmm_adapt.h"
#include "devmm_channel.h"
#include "svm_heap_mng.h"
#include "svm_version_adapt.h"

#define MSG_FORMAT_INVALID_OFFSET (-1)

struct devmm_setup_dev_msg_format {
    int cmd_offset;
    int devpid_offset;
    int ssid_offset;
    int logic_devid_offset;
    int heap_cnt_offset;
    int heap_info_offset;
};

struct devmm_page_bitmap_format {
    u32 ipc_mem_open_bit;
    u32 ipc_mem_create_bit;
    u32 remote_mapped_bit;
    u32 dev_mapped_bit;
    u32 host_mapped_bit;
    u32 is_first_page_bit;
    u32 advise_ddr_bit;
    u32 advise_populate_bit;
    u32 is_translate_bit;
    u32 alloced_bit;
    u32 locked_host_bit;
    u32 locked_device_bit;
    u32 advise_memory_shared_bit;
    u32 advise_p2p_hbm_bit;
    u32 advise_ts_bit;
    u32 readonly_bit;
    u32 remote_mapped_first_bit;
    u32 bitmap_locked_bit;
    u32 advise_continuty_bit;
    u32 advise_4g_bit;
    u32 advise_p2p_ddr_bit;
    u32 devid_shift;
    u32 devid_wid;
};

#define page_bitmap_bit_version_adapt(src_bitmap, src_bit, dst_bitmap, dst_bit) do { \
    if (((src_bit) != DEVMM_PAGE_INVAILD_BIT) && ((dst_bit) != DEVMM_PAGE_INVAILD_BIT)) { \
        u32 val = devmm_page_bitmap_get_value(src_bitmap, src_bit, 1); \
        devmm_page_bitmap_set_value(dst_bitmap, dst_bit, 1, val); \
    } \
} while (0)

static inline void msg_field_ver_adapt(void *src_msg, int src_field_offset,
    void *dst_msg, int dst_field_offset, u64 field_size)
{
    if (((src_field_offset) != MSG_FORMAT_INVALID_OFFSET) && ((dst_field_offset) != MSG_FORMAT_INVALID_OFFSET)) {
        void *src = (void *)((u64)(uintptr_t)src_msg + src_field_offset);
        void *dst = (void *)((u64)(uintptr_t)dst_msg + dst_field_offset);
        (void)memcpy_s(dst, field_size, src, field_size);
    }
}

static struct devmm_page_bitmap_format *devmm_get_svm_version_000_page_bitmap_format(void)
{
    static struct devmm_page_bitmap_format svm_version_000_page_bitmap_format = {
        .ipc_mem_open_bit = 0,
        .ipc_mem_create_bit = 1,
        .remote_mapped_bit = 2,
        .dev_mapped_bit = 3,
        .host_mapped_bit = 4,
        .is_first_page_bit = 5,
        .advise_ddr_bit = 6,
        .advise_populate_bit = 7,
        .is_translate_bit = 9,
        .alloced_bit = 10,
        .locked_host_bit = 11,
        .locked_device_bit = 12,
        .advise_memory_shared_bit = 13,
        .advise_p2p_hbm_bit = 14,
        .advise_ts_bit = 15,
        .readonly_bit = 16,
        .remote_mapped_first_bit = 17,
        .bitmap_locked_bit = 21,
        .advise_continuty_bit = 22,
        .advise_4g_bit = 23,
        .advise_p2p_ddr_bit = 24,
        .devid_shift = 25,
        .devid_wid = 7
    };
    return &svm_version_000_page_bitmap_format;
}

static struct devmm_page_bitmap_format *devmm_get_svm_version_001_page_bitmap_format(void)
{
    static struct devmm_page_bitmap_format svm_version_001_page_bitmap_format = {
        .ipc_mem_open_bit = DEVMM_PAGE_IPC_MEM_OPEN_BIT,
        .ipc_mem_create_bit = DEVMM_PAGE_IPC_MEM_CREATE_BIT,
        .remote_mapped_bit = DEVMM_PAGE_REMOTE_MAPPED_BIT,
        .dev_mapped_bit = DEVMM_PAGE_DEV_MAPPED_BIT,
        .host_mapped_bit = DEVMM_PAGE_HOST_MAPPED_BIT,
        .is_first_page_bit = DEVMM_PAGE_IS_FIRST_PAGE_BIT,
        .advise_ddr_bit = DEVMM_PAGE_ADVISE_DDR_BIT,
        .advise_populate_bit = DEVMM_PAGE_ADVISE_POPULATE_BIT,
        .is_translate_bit = DEVMM_PAGE_IS_TRANSLATE_BIT,
        .alloced_bit = DEVMM_PAGE_ALLOCED_BIT,
        .locked_host_bit = DEVMM_PAGE_LOCKED_HOST_BIT,
        .locked_device_bit = DEVMM_PAGE_LOCKED_DEVICE_BIT,
        .advise_memory_shared_bit = DEVMM_PAGE_ADVISE_MEMORY_SHARED_BIT,
        .advise_p2p_hbm_bit = DEVMM_PAGE_ADVISE_P2P_HBM_BIT,
        .advise_ts_bit = DEVMM_PAGE_ADVISE_TS_BIT,
        .readonly_bit = DEVMM_PAGE_READONLY_BIT,
        .remote_mapped_first_bit = DEVMM_PAGE_REMOTE_MAPPED_FIRST_BIT,
        .bitmap_locked_bit = DEVMM_PAGE_BITMAP_LOCKED_BIT,
        .advise_continuty_bit = DEVMM_PAGE_ADVISE_CONTINUTY_BIT,
        .advise_4g_bit = DEVMM_PAGE_INVAILD_BIT,
        .advise_p2p_ddr_bit = DEVMM_PAGE_ADVISE_P2P_DDR_BIT,
        .devid_shift = DEVMM_PAGE_DEVID_SHIT,
        .devid_wid = DEVMM_PAGE_DEVID_WID
    };
    return &svm_version_001_page_bitmap_format;
}
#ifndef EMU_ST
static struct devmm_page_bitmap_format *devmm_get_page_bitmap_format(u32 version)
{
    if (version >= SVM_VERSION_0001) {
        return devmm_get_svm_version_001_page_bitmap_format();
    } else {
        return devmm_get_svm_version_000_page_bitmap_format();
    }
}

void devmm_page_bitmap_version_adapt(u32 *src_bitmap, u32 src_version, u32 *dst_bitmap, u32 dst_version)
{
    struct devmm_page_bitmap_format *src = devmm_get_page_bitmap_format(src_version);
    struct devmm_page_bitmap_format *dst = devmm_get_page_bitmap_format(dst_version);

    if ((src->devid_shift != DEVMM_PAGE_INVAILD_BIT) && (src->devid_wid != DEVMM_PAGE_INVAILD_BIT) &&
        (dst->devid_shift != DEVMM_PAGE_INVAILD_BIT) && (dst->devid_wid != DEVMM_PAGE_INVAILD_BIT)) {
        u32 devid = devmm_page_bitmap_get_value(src_bitmap, src->devid_shift, src->devid_wid);
        devmm_page_bitmap_set_value(dst_bitmap, dst->devid_shift, dst->devid_wid, devid);
    }

    page_bitmap_bit_version_adapt(src_bitmap, src->ipc_mem_open_bit, dst_bitmap, dst->ipc_mem_open_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->ipc_mem_create_bit, dst_bitmap, dst->ipc_mem_create_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->remote_mapped_bit, dst_bitmap, dst->remote_mapped_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->dev_mapped_bit, dst_bitmap, dst->dev_mapped_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->host_mapped_bit, dst_bitmap, dst->host_mapped_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->is_first_page_bit, dst_bitmap, dst->is_first_page_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_ddr_bit, dst_bitmap, dst->advise_ddr_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_populate_bit, dst_bitmap, dst->advise_populate_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->is_translate_bit, dst_bitmap, dst->is_translate_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->alloced_bit, dst_bitmap, dst->alloced_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->locked_host_bit, dst_bitmap, dst->locked_host_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->locked_device_bit, dst_bitmap, dst->locked_device_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_memory_shared_bit, dst_bitmap, dst->advise_memory_shared_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_p2p_hbm_bit, dst_bitmap, dst->advise_p2p_hbm_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_ts_bit, dst_bitmap, dst->advise_ts_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->readonly_bit, dst_bitmap, dst->readonly_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->remote_mapped_first_bit, dst_bitmap, dst->remote_mapped_first_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->bitmap_locked_bit, dst_bitmap, dst->bitmap_locked_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_continuty_bit, dst_bitmap, dst->advise_continuty_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_4g_bit, dst_bitmap, dst->advise_4g_bit);
    page_bitmap_bit_version_adapt(src_bitmap, src->advise_p2p_ddr_bit, dst_bitmap, dst->advise_p2p_ddr_bit);
}

static struct devmm_setup_dev_msg_format *devmm_get_svm_version_001_setup_dev_msg_format(void)
{
    static struct devmm_setup_dev_msg_format svm_version_001_setup_dev_msg_format = {
        .cmd_offset = sizeof(struct devmm_chan_msg_head),
        .devpid_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32),
        .ssid_offset = MSG_FORMAT_INVALID_OFFSET,
        .logic_devid_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32) + sizeof(int),
        .heap_cnt_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32) + sizeof(int) + sizeof(u32),
        .heap_info_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32) + sizeof(int) + sizeof(u32) + sizeof(u32),
    };
    return &svm_version_001_setup_dev_msg_format;
}

static struct devmm_setup_dev_msg_format *devmm_get_svm_version_002_setup_dev_msg_format(void)
{
    static struct devmm_setup_dev_msg_format svm_version_002_setup_dev_msg_format = {
        .cmd_offset = sizeof(struct devmm_chan_msg_head),
        .devpid_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32),
        .ssid_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32) + sizeof(int),
        .logic_devid_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32) + sizeof(int) + sizeof(int),
        .heap_cnt_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32) + sizeof(int) + sizeof(int) + sizeof(u32),
        .heap_info_offset = sizeof(struct devmm_chan_msg_head) + sizeof(u32) + sizeof(int) + sizeof(int) + sizeof(u32) +
                            sizeof(u32),
    };
    return &svm_version_002_setup_dev_msg_format;
}

u64 devmm_get_setup_dev_msg_len(u32 version, u64 extend_num)
{
    u64 cur_ver_len = sizeof(struct devmm_chan_setup_device) + extend_num * sizeof(struct devmm_chan_heap_info);

    if (version >= SVM_VERSION_0002) {
        return cur_ver_len;
    } else {
        return (cur_ver_len - sizeof(int));
    }
}

static struct devmm_setup_dev_msg_format *devmm_get_setup_dev_msg_format(u32 version)
{
    if (version >= SVM_VERSION_0002) {
        return devmm_get_svm_version_002_setup_dev_msg_format();
    } else {
        return devmm_get_svm_version_001_setup_dev_msg_format();
    }
}

void devmm_setup_dev_msg_version_adapt(void *src_msg, u32 src_version, void *dst_msg, u32 dst_version)
{
    struct devmm_setup_dev_msg_format *src_format = devmm_get_setup_dev_msg_format(src_version);
    struct devmm_setup_dev_msg_format *dst_format = devmm_get_setup_dev_msg_format(dst_version);

    memcpy_s(dst_msg, sizeof(struct devmm_chan_msg_head), src_msg, sizeof(struct devmm_chan_msg_head));

    msg_field_ver_adapt(src_msg, src_format->cmd_offset, dst_msg, dst_format->cmd_offset, sizeof(u32));
    msg_field_ver_adapt(src_msg, src_format->devpid_offset, dst_msg, dst_format->devpid_offset, sizeof(int));
    msg_field_ver_adapt(src_msg, src_format->ssid_offset, dst_msg, dst_format->ssid_offset, sizeof(int));
    msg_field_ver_adapt(src_msg, src_format->logic_devid_offset, dst_msg, dst_format->logic_devid_offset, sizeof(u32));
    msg_field_ver_adapt(src_msg, src_format->heap_cnt_offset, dst_msg, dst_format->heap_cnt_offset, sizeof(u32));

    /* adapt extend msg */
    if ((src_format->heap_info_offset != MSG_FORMAT_INVALID_OFFSET) &&
        (dst_format->heap_info_offset != MSG_FORMAT_INVALID_OFFSET)) {
        struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)src_msg;
        u64 field_size = (u64)msg_head->extend_num * sizeof(struct devmm_chan_heap_info);
        void *src = (void *)((u64)(uintptr_t)src_msg + src_format->heap_info_offset);
        void *dst = (void *)((u64)(uintptr_t)dst_msg + dst_format->heap_info_offset);

        (void)memcpy_s(dst, field_size, src, field_size);
    }
}
#endif

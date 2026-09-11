/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-14
 */

#include <linux/ktime.h>

#include "ka_task_pub.h"
#include "ka_common_pub.h"
#include "ka_kernel_def_pub.h"

#include "securec.h"
#include "dbl_mem_alloc_interface.h"
#include "urd_acc_ctrl.h"
#include "udis_log.h"
#include "udis_timer.h"
#include "udis_management.h"

#define UDIS_INVALID_NAME_ID (~0)
#define UDIS_LINK_DMA_TASK_PERIOD_MS (100)
#define UDIS_LINK_DMA_TASK_NAME "udis_dma_%u"
STATIC unsigned long long g_link_dma_timestamp[UDIS_DEVICE_UDEVID_MAX] = {0};

STATIC int udis_check_input_valid(unsigned int udevid, UDIS_MODULE_TYPE module_type,
                                        const struct udis_dev_info *input_info)
{
    unsigned int name_len = 0;

    if (!uda_is_udevid_exist(udevid)) {
        udis_err("udis_info input udevid not exist. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    if (module_type < 0 || module_type >= UDIS_MODULE_MAX) {
        udis_err("udis_info module_type invalid.(udevid=%u; module_type=%u; module_type_max=%d)\n",
                     udevid, module_type, UDIS_MODULE_MAX);
        return -EINVAL;
    }

    if (input_info == NULL) {
        udis_err("udis_info input input_info is NULL.(udevid=%u; module_type=%u)\n", udevid, module_type);
        return -EINVAL;
    }

    name_len = strnlen(input_info->name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        udis_err("name length is invalid. (udevid=%u; module_type=%u; name_len=%u; min_len=%u; max_len=%u)\n",
                udevid, module_type, name_len, 1, UDIS_MAX_NAME_LEN - 1);
        return -EINVAL;
    }

    if (input_info->data_len > UDIS_MAX_DATA_LEN) {
        udis_err("udis_info data length is too large. (udevid=%u; module_type=%u; name=%s; data_len=%u)\n",
                udevid, module_type, input_info->name, input_info->data_len);
        return -EINVAL;
    }
    return 0;
}

STATIC int udis_check_ucb(u32 udevid, const struct udis_ctrl_block *ucb)
{
    if (ucb == NULL) {
        udis_err("udis ctrl block is NULL. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    if (ucb->state != UDIS_DEV_READY) {
        udis_err("udis device state is not ready. (udevid=%u; state=%d)", udevid, ucb->state);
        return -ENODEV;
    }

    return 0;
}

STATIC struct udis_info_stu* udis_find_info_block(const struct udis_ctrl_block* ucb, UDIS_MODULE_TYPE module_type,
                                                    const char* name, enum udis_search_scope uss)
{
    struct udis_info_stu *module_block, *info_block_tmp;
    struct udis_info_stu *info_block = NULL;
    int i = (uss == UDIS_INFO_DISCRE_SPACE) ? UDIS_UNIFIED_NAME_MAX : 0;

    module_block = ucb->udis_info_buf + UDIS_NAME_NUM_MAX * module_type;
    if (module_block <  ucb->udis_info_buf ) {
        udis_err("Invalid memory access to module_block. (module_type=%u)\n", module_type);
        return NULL;
    }

    for (; i < UDIS_NAME_NUM_MAX; i++) {
        info_block_tmp = module_block + i;
        if (info_block_tmp < module_block) {
            udis_err("Invalid memory access to info_block_tmp. (module_type=%u; name_index=%d)\n", module_type, i);
            break;
        }

        if (strcmp(info_block_tmp->name, name) == 0) {
            info_block = info_block_tmp;
            break;
        }
    }
    return info_block;
}

STATIC struct udis_info_stu* udis_find_empty_block(const struct udis_ctrl_block* ucb, UDIS_MODULE_TYPE module_type)
{
    struct udis_info_stu *module_block, *empty_block_tmp;
    struct udis_info_stu *empty_block = NULL;
    int i = UDIS_UNIFIED_NAME_MAX;
    char zeros_arr[UDIS_MAX_NAME_LEN] = {0};

    module_block = ucb->udis_info_buf + UDIS_NAME_NUM_MAX * module_type;
    if (module_block < ucb->udis_info_buf) {
        udis_err("Invalid memory access to module_block. (module_type=%u)\n", module_type);
        return NULL;
    }

    for (; i < UDIS_NAME_NUM_MAX; i++) {
        empty_block_tmp = module_block + i;
        if (empty_block_tmp < module_block) {
            udis_err("Invalid memory access to empty_block_tmp. (module_type=%u; name_index=%d)\n", module_type, i);
            break;
        }

        if (memcmp(empty_block_tmp->name, zeros_arr, UDIS_MAX_NAME_LEN) == 0) {
            empty_block = empty_block_tmp;
            break;
        }
    }

    return empty_block;
}

STATIC int udis_check_info_block_valid(u32 udevid, const struct udis_info_stu* info_block)
{
    int ret = UDIS_DATA_VALID;
    u64 cur_time;

    switch (info_block->update_type) {
        case UPDATE_ONLY_ONCE:
            if (info_block->data_len == 0) {
                ret = UDIS_DATA_NEEDS_UPDATE;
            }
            break;
        case UPDATE_IMMEDIATELY:
            ret = UDIS_DATA_UPDATE_IMMEDIATELY;
            if (info_block->data_len == 0) {
                ret = UDIS_DATA_NEEDS_UPDATE;
            }
            break;
        case UPDATE_PERIOD_LEVEL_1:
            cur_time = ktime_get_raw_ns() / NSEC_PER_MSEC;
            if (cur_time > (udis_get_link_dma_timestamp(udevid) + UDIS_MAX_NO_UPDATE_MSEC)) {
                ret = UDIS_DATA_NEEDS_UPDATE;
            }
            break;
        default:
            ret = UDIS_DATA_INVALID;
            udis_err("udis update_type check invalid. (udevid=%u; update_type=%u; ret=%d)\n",
                udevid, info_block->update_type, ret);
    }

    return ret;
}

STATIC int udis_store_info(const struct udis_dev_info *src_info, struct udis_info_stu* dst_info_block)
{
    int ret = 0;
    struct udis_info_stu info_block_tmp = {{0}, 0};

    if ((src_info->update_type < 0) || (src_info->update_type >= UPDATE_TYPE_MAX)) {
        udis_err("udis update_type check invalid, should be in [0, %u). (update_type=%u; ret=%d)\n", UPDATE_TYPE_MAX,
            src_info->update_type, -EINVAL);
        return -EINVAL;
    }
    info_block_tmp.update_type = src_info->update_type;

    if (src_info->name == NULL) {
        udis_err("udis src_info name is NULL.\n");
        return -EINVAL;
    }

    if (strlen(src_info->name) > UDIS_MAX_NAME_LEN - 1) {
        udis_err("udis src_info name len is invalid. (name=%s; max_name_len=%d)\n", src_info->name, UDIS_MAX_NAME_LEN);
        return -EINVAL;
    }

    ret = strcpy_s(info_block_tmp.name, UDIS_MAX_NAME_LEN, src_info->name);
    if (ret != 0) {
        udis_err("strcpy_s name failed. (name=%s)\n", src_info->name);
        return -EIO;
    }

    ret = memcpy_s(info_block_tmp.data, UDIS_MAX_DATA_LEN, src_info->data, src_info->data_len);
    if (ret != 0) {
        udis_err("memcpy_s data failed.(name=%s)\n", src_info->name);
        return -EIO;
    }

    info_block_tmp.acc_ctrl = src_info->acc_ctrl;
    info_block_tmp.data_len = src_info->data_len;
    info_block_tmp.last_update_time = ktime_get_raw_ns() / NSEC_PER_MSEC;

    ret = memcpy_s(dst_info_block, UDIS_NAME_OFFSET, &info_block_tmp, UDIS_NAME_OFFSET);
    if (ret != 0) {
        udis_err("memcpy_s info_block_tmp failed. (name=%s)\n", src_info->name);
        return -EIO;
    }

    return 0;
}

STATIC int udis_load_info_block(u32 udevid, UDIS_MODULE_TYPE module_type,
                                    const struct udis_info_stu *src_info_block, struct udis_dev_info *dst_info)
{
    int ret = 0;

    ret = memcpy_s(dst_info->data, UDIS_MAX_DATA_LEN, src_info_block->data, src_info_block->data_len);
    if (ret != 0) {
        ret = -EIO;
        udis_err("udis dst_info memcpy_s error. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
                    udevid, module_type, src_info_block->name, ret);
        return ret;
    }
    dst_info->data_len = src_info_block->data_len;
    dst_info->acc_ctrl = src_info_block->acc_ctrl;
    dst_info->update_type = src_info_block->update_type;
    dst_info->last_update_time = src_info_block->last_update_time;
    return ret;
}

STATIC int udis_update_load_info_block(struct udis_ctrl_block* ucb, u32 udevid, UDIS_MODULE_TYPE module_type,
                                    struct udis_dev_info *info_request)
{
    int ret = 0;
    struct udis_info_stu* info_block = NULL;

    ret = udis_update_info_by_dma(udevid, module_type, info_request->name);
    if (ret != 0) {
        udis_err("udis_update_info_by_dma failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
            udevid, module_type, info_request->name, ret);
        return ret;
    }

    ka_task_down_read(&ucb->udis_info_lock);
    info_block = udis_find_info_block(ucb, module_type, info_request->name, UDIS_INFO_ALL_SPACE);
    if (info_block == NULL) {
        ret = -ENODATA;
        udis_err("This module has no such name. (udevid=%u; module_type=%u; name=%s)\n",
            udevid, module_type, info_request->name);
        goto out;
    }

    ret = udis_check_info_block_valid(udevid, info_block);
    if (ret != UDIS_DATA_VALID && ret != UDIS_DATA_UPDATE_IMMEDIATELY) {
        ret = -EINVAL;
        udis_err("udis info_block's data is invalid. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
                    udevid, module_type, info_block->name, ret);
        goto out;
    }

    ret = udis_load_info_block(udevid, module_type, info_block, info_request);
    if (ret != 0) {
        udis_err("udis_load_info_block failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
            udevid, module_type, info_request->name, ret);
        goto out;
    }
out:
    ka_task_up_read(&(ucb->udis_info_lock));
    return ret;
}

int hal_kernel_set_udis_info(unsigned int udevid, UDIS_MODULE_TYPE module_type,
                                const struct udis_dev_info *udis_set_info)
{
    int ret = 0;
    struct udis_ctrl_block* ucb = NULL;
    struct udis_info_stu* info_block = NULL;

    ret = udis_check_input_valid(udevid, module_type, udis_set_info);
    if (ret != 0) {
        return ret;
    }

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    ret = udis_check_ucb(udevid, ucb);
    if (ret != 0) {
        udis_cb_read_unlock(udevid);
        return ret;
    }

    ka_task_down_write(&ucb->udis_info_lock);
    info_block = udis_find_info_block(ucb, module_type, udis_set_info->name, UDIS_INFO_DISCRE_SPACE);
    if (info_block == NULL) {
        info_block = udis_find_empty_block(ucb, module_type);
        if (info_block == NULL) {
            ret = -ENOSPC;
            udis_err("No empty block for new name. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
                        udevid, module_type, udis_set_info->name, ret);
            goto udis_info_unlock;
        }
    }

    ret = udis_store_info(udis_set_info, info_block);
    if (ret != 0) {
        udis_err("udis_store_info failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
                    udevid, module_type, udis_set_info->name, ret);
    }

udis_info_unlock:
    ka_task_up_write(&ucb->udis_info_lock);
    udis_cb_read_unlock(udevid);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_set_udis_info);

int hal_kernel_get_udis_info(unsigned int udevid, UDIS_MODULE_TYPE module_type, struct udis_dev_info *info_request)
{
    int ret = 0;
    struct udis_ctrl_block* ucb = NULL;
    struct udis_info_stu* info_block = NULL;

    ret = udis_check_input_valid(udevid, module_type, info_request);
    if (ret != 0) {
        return ret;
    }

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    ret = udis_check_ucb(udevid, ucb);
    if (ret != 0) {
        goto unlock_ucb;
    }

    ka_task_down_read(&(ucb->udis_info_lock));
    info_block = udis_find_info_block(ucb, module_type, info_request->name, UDIS_INFO_ALL_SPACE);
    if (info_block == NULL) {
        ret = -ENODATA;
        udis_debug("This module has no such name. (udevid=%u; type=%u; name=%s; ret=%d)\n",
            udevid, module_type, info_request->name, ret);
        goto unlock_udis_info;
    }

    ret = dms_feature_access_identify(info_block->acc_ctrl, 0);
    if (ret != 0) {
        udis_ex_notsupport_err(ret, "Access identify failed. (udevid=%u; type=%u; name=%s; ret=%d; acc_ctrl=0x%x)\n",
            udevid, module_type, info_request->name, ret, info_block->acc_ctrl);
        goto unlock_udis_info;
    }

    ret = udis_check_info_block_valid(udevid, info_block);
    switch(ret) {
        case UDIS_DATA_VALID:
            ret = udis_load_info_block(udevid, module_type, info_block, info_request);
            goto unlock_udis_info;
        case UDIS_DATA_NEEDS_UPDATE:
        case UDIS_DATA_UPDATE_IMMEDIATELY:
            ka_task_up_read(&(ucb->udis_info_lock));
            ret = udis_update_load_info_block(ucb, udevid, module_type, info_request);
            goto unlock_ucb;
        default:
            ret = -EINVAL;
    }

unlock_udis_info:
    ka_task_up_read(&ucb->udis_info_lock);
unlock_ucb:
    udis_cb_read_unlock(udevid);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_udis_info);

STATIC int udis_get_host_unified_info(unsigned int udevid, UDIS_MODULE_TYPE module_type, dma_addr_t *host_dma_addr)
{
    struct udis_ctrl_block *udis_cb = NULL;
    dma_addr_t dma_addr = UDIS_BAD_DMA_ADDR;
    dma_addr_t dma_base_addr = UDIS_BAD_DMA_ADDR;

    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    dma_base_addr = udis_cb->udis_info_buf_dma;
    dma_addr = dma_base_addr + module_type * UDIS_MODULE_OFFSET;
    if (dma_addr < dma_base_addr) {
        udis_err("dma addr overflow. (udevid=%u; module_type=%u)\n", udevid, module_type);
        return -EOVERFLOW;
    }

    *host_dma_addr = dma_addr;

    return 0;
}

STATIC int udis_fill_info_block(unsigned int udevid, const struct udis_dma_node *addr_node, struct udis_info_stu *info_block)
{
    int ret;

    ret = strcpy_s(info_block->name, UDIS_MAX_NAME_LEN, addr_node->name);
    if (ret != 0) {
        udis_err("strncpy_s failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
            udevid, addr_node->module_type, addr_node->name, ret);
        return -ENOMEM;
    }

    info_block->update_type = addr_node->update_type;
    info_block->acc_ctrl = addr_node->acc_ctrl;
    info_block->data_len = addr_node->data_len;
    return 0;
}

STATIC dma_addr_t udis_dma_map_info_block(unsigned int udevid, const struct udis_ctrl_block *udis_cb,
    UDIS_MODULE_TYPE module_type, const struct udis_info_stu *info_block)
{
    unsigned long offset = 0;
    dma_addr_t info_dma_addr = UDIS_BAD_DMA_ADDR;
    dma_addr_t udis_dma_base_addr = 0;

    offset = (unsigned long)(void *)info_block - (unsigned long)udis_cb->udis_info_buf;
    if (offset >= (unsigned long)(void *)info_block) {
        udis_err("Offset overflow. (udevid=%u; module_type=%u; name=%s; offset=%lu)\n", udevid,
            module_type, info_block->name, offset);
        return UDIS_BAD_DMA_ADDR;
    }

    /* map host virtual address to host DMA address */
    udis_dma_base_addr = udis_cb->udis_info_buf_dma;
    info_dma_addr = udis_dma_base_addr + offset + ka_offsetof(struct udis_info_stu, data);
    if (info_dma_addr <= udis_dma_base_addr) {
        udis_err("Info host dma addr overflow. (udevid=%u; module_type=%u; name=%s)\n", udevid,
            module_type, info_block->name);
        return UDIS_BAD_DMA_ADDR;
    }
    return info_dma_addr;
}

STATIC struct udis_info_stu *udis_dma_remap_info_block(unsigned int udevid, const struct udis_ctrl_block *udis_cb,
    dma_addr_t host_dma_addr)
{
    unsigned long offset = 0;
    unsigned long udis_dma_base_addr = 0;
    /* The function converts info block's host dma addr to the host virtual addr
     * The offset between the struct udis_info_stu corresponding to host_dma_addr and udis_cb->udis_info_buf is
     * equal to the offset between host host_dma_addr and udis_cb->udis_info_buf_dma minus
     * the offset of the data member in the struct udis_info_stu structure.
     */
    udis_dma_base_addr = udis_cb->udis_info_buf_dma;
    offset = host_dma_addr - udis_dma_base_addr - ka_offsetof(struct udis_info_stu, data);
    if (offset >= host_dma_addr) {
        udis_err("Offset overflow. (udevid=%u; offset=%lu)\n", udevid, offset);
        return NULL;
    }

    if (offset % sizeof(struct udis_info_stu) != 0) {
        udis_err("Offset is not an integer multiple of UDIS_NAME_OFFSET. (udevid=%u; offset=%lu)\n", udevid, offset);
        return NULL;
    }
    /* Convert the offset to an array index. */
    offset = offset / sizeof(struct udis_info_stu);
    if (offset >= (UDIS_NAME_NUM_MAX * UDIS_MODULE_MAX)) {
        udis_err("Offset is invalid. (udevid=%u; offset=%lu)\n", udevid, offset);
        return NULL;
    }
    return udis_cb->udis_info_buf + offset;
}

STATIC int udis_alloc_discrete_info_block(unsigned int udevid, const struct udis_dma_node *addr_node,
    dma_addr_t *host_dma_addr)
{
    int ret;
    dma_addr_t info_dma_addr = UDIS_BAD_DMA_ADDR;
    struct udis_ctrl_block *udis_cb = NULL;
    struct udis_info_stu *info_block = NULL;

    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    ka_task_down_write(&udis_cb->udis_info_lock);
    info_block = udis_find_empty_block(udis_cb, addr_node->module_type);
    if (info_block == NULL) {
        ka_task_up_write(&udis_cb->udis_info_lock);
        udis_err("Get unused info block failed. (udevid=%u; module_type=%u; name=%s)\n", udevid,
            addr_node->module_type, addr_node->name);
        return -ENOSPC;
    }

    info_dma_addr = udis_dma_map_info_block(udevid, udis_cb, addr_node->module_type, info_block);
    if (info_dma_addr == UDIS_BAD_DMA_ADDR) {
        ka_task_up_write(&udis_cb->udis_info_lock);
        udis_err("Failed to map udis info block's va to dma addr. (udevid=%u; module_type=%u; name=%s)\n",
            udevid, addr_node->module_type, addr_node->name);
        return -EFAULT;
    }

    *host_dma_addr = info_dma_addr;

    ret = udis_fill_info_block(udevid, addr_node, info_block);
    if (ret != 0) {
        ka_task_up_write(&udis_cb->udis_info_lock);
        udis_err("Failed to fill info block's name. (udevid=%u; module_type=%u; name=%s, ret=%d)\n",
            udevid, addr_node->module_type, addr_node->name);
        return ret;
    }

    ka_task_up_write(&udis_cb->udis_info_lock);

    return 0;
}

STATIC int udis_clear_unified_info(unsigned int udevid, struct udis_ctrl_block *udis_cb, UDIS_MODULE_TYPE module_type)
{
    int ret;
    struct udis_info_stu *module_base_addr = NULL;

    module_base_addr = udis_cb->udis_info_buf +  module_type * UDIS_NAME_NUM_MAX;
    ret = memset_s(module_base_addr, UDIS_UNIFIED_MODULE_OFFSET, 0, UDIS_UNIFIED_MODULE_OFFSET);
    if (ret != 0) {
        udis_err("Call memset_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -EIO;
    }
    return 0;
}

STATIC void udis_clear_unified_info_data_len(unsigned int udevid, struct udis_ctrl_block *udis_cb, UDIS_MODULE_TYPE module_type)
{
    int i;
    struct udis_info_stu *module_base_addr = NULL;
    struct udis_info_stu *info_block = NULL;

    module_base_addr = udis_cb->udis_info_buf +  module_type * UDIS_NAME_NUM_MAX;
    for (i = 0; i < UDIS_UNIFIED_NAME_MAX; ++i) {
        info_block = module_base_addr + i;
        info_block->data_len = 0;
    }

    return;
}

STATIC int udis_clear_info_block_name(unsigned int udevid, struct udis_ctrl_block *udis_cb, dma_addr_t host_dma_addr)
{
    int ret;
    struct udis_info_stu *info_block = NULL;

    info_block = udis_dma_remap_info_block(udevid, udis_cb, host_dma_addr);
    if (info_block == NULL) {
        udis_err("Remap host dma addr to host virtual addr failed. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    ret = memset_s(info_block->name, UDIS_MAX_NAME_LEN, 0, UDIS_MAX_NAME_LEN);
    if (ret != 0) {
        udis_err("Call memset_s failed. (udevid=%u; ret=%d)", udevid, ret);
        return -EIO;
    }
    return 0;
}

STATIC int udis_clear_info_block_len(unsigned int udevid, struct udis_ctrl_block *udis_cb, dma_addr_t host_dma_addr)
{
    struct udis_info_stu *info_block = NULL;

    info_block = udis_dma_remap_info_block(udevid, udis_cb, host_dma_addr);
    if (info_block == NULL) {
        udis_err("Remap host dma addr to host virtual addr failed. (udevid=%u)\n", udevid);
        return -EFAULT;
    }
    info_block->data_len = 0;
    return 0;
}

int udis_alloc_info_block(unsigned int udevid, const struct udis_dma_node *addr_node, dma_addr_t *host_dma_addr)
{
    if (host_dma_addr == NULL) {
        udis_err("Invalid param. (udevid=%u; module_type=%u; name=%s)\n",
            udevid, addr_node->module_type, addr_node->name);
        return -EINVAL;
    }

    if (strcmp(addr_node->name, UDIS_UNIFIED_MODULE_INFO) == 0) {
        return udis_get_host_unified_info(udevid, addr_node->module_type, host_dma_addr);
    }
    return udis_alloc_discrete_info_block(udevid, addr_node, host_dma_addr);
}

void udis_free_info_block(unsigned int udevid, UDIS_MODULE_TYPE module_type, const char *name,
    dma_addr_t host_dma_addr)
{
    int ret;
    struct udis_ctrl_block *udis_cb = NULL;

    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return;
    }

    if ((host_dma_addr < udis_cb->udis_info_buf_dma + UDIS_MODULE_OFFSET * module_type) ||
        (host_dma_addr >= udis_cb->udis_info_buf_dma + UDIS_MODULE_OFFSET * module_type + UDIS_MODULE_OFFSET)) {
        udis_err("Invalid host_dma_addr. (udevid=%u; module_type=%u; name=%s)\n",
            udevid, module_type, name);
        return;
    }

    ka_task_down_write(&udis_cb->udis_info_lock);
    if (strcmp(name, UDIS_UNIFIED_MODULE_INFO) == 0) {
        ret = udis_clear_unified_info(udevid, udis_cb, module_type);
        if (ret != 0) {
            udis_err("Clear host unified info failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
                udevid, module_type, name, ret);
        }
    } else {
        ret = udis_clear_info_block_name(udevid, udis_cb, host_dma_addr);
        if (ret != 0) {
            udis_err("Clear block name failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
                udevid, module_type, name,ret);
        }
    }

    ka_task_up_write(&udis_cb->udis_info_lock);
    return;
}

STATIC void udis_dma_fail_process(unsigned int udevid, struct udis_ctrl_block *udis_cb, const struct udis_dma_node *node)
{
    int ret;

    if(node->module_type >= UDIS_MODULE_MAX) {
        udis_err("Module id is invalid. (udevid=%u; module_type=%u)\n", udevid, node->module_type);
        return;
    }

    if (strcmp(node->name, UDIS_UNIFIED_MODULE_INFO) == 0) {
        udis_clear_unified_info_data_len(udevid, udis_cb, node->module_type);
    } else {
        ret = udis_clear_info_block_len(udevid, udis_cb, node->host_dma_addr);
        if (ret != 0) {
            udis_err("Clear block len failed. (udevid=%u; module=%u; name=%s; ret=%d)",
                udevid, node->module_type, node->name, ret);
        }
    }

    return;
}

int udis_dma_sync_copy(unsigned int udevid, struct udis_ctrl_block *udis_cb, const struct udis_dma_node *node)
{
    int ret;

    if (node == NULL) {
        udis_err("Invalid param, node is NULL. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if ((node->host_dma_addr == UDIS_BAD_DMA_ADDR) || (node->dev_dma_addr == UDIS_BAD_DMA_ADDR) ||
        (node->data_len == 0)) {
        udis_err("Invalid param. (udevid=%u; data_len=%u)\n", udevid, node->data_len);
        return -EINVAL;
    }

    if (udis_cb->state != UDIS_DEV_READY) {
        udis_err("udis ctrl block's state is not ready. (udevid=%d; state=%d)\n", udevid, udis_cb->state);
        return -ENODEV;
    }

    ka_task_down_write(&udis_cb->udis_info_lock);

    ret = devdrv_dma_sync_copy(udevid, DEVDRV_DMA_DATA_COMMON, node->dev_dma_addr, node->host_dma_addr,
        node->data_len, DEVDRV_DMA_DEVICE_TO_HOST);
    if (ret != 0) {
        udis_dma_fail_process(udevid, udis_cb, node);
        ka_task_up_write(&udis_cb-> udis_info_lock);
        udis_err("Dma copy failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n",
            udevid, node->module_type, node->name, ret);
        return ret;
    }
    ka_task_up_write(&udis_cb->udis_info_lock);

    return 0;
}

struct udis_dma_node *udis_find_dma_node(struct udis_ctrl_block *udis_cb, UDIS_MODULE_TYPE module_type,
    const char *name)
{
    struct udis_dma_node *addr_node = NULL;

    addr_node = udis_addr_list_find_node(udis_cb, module_type, name);
    if  (addr_node != NULL) {
        return addr_node;
    }

    /*If no node matching {module_type, name} is found, it indicates information in the unified space.*/
    list_for_each_entry(addr_node, &udis_cb->addr_list[UPDATE_PERIOD_LEVEL_1], list) {
        if ((addr_node->module_type == module_type) &&
            (strcmp(addr_node->name, UDIS_UNIFIED_MODULE_INFO) == 0)) {
            return addr_node;
        }
    }

    udis_err("No match addr node found. (module_type=%u; name=%s)\n", module_type, name);
    return NULL;
}

/*You need to udis_cb_read_lock(udevid) before call udis_update_info_by_dma*/
int udis_update_info_by_dma(unsigned int udevid, UDIS_MODULE_TYPE module_type, const char *name)
{
    int ret;
    struct udis_ctrl_block *udis_cb = NULL;
    struct udis_dma_node *addr_node = NULL;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (module_type >= UDIS_MODULE_MAX) || (name == NULL)) {
        udis_err("Invalid param. (udevid=%u, module_type=%u; name_is_null=%d)\n", udevid, module_type,
            name == NULL);
        return -EINVAL;
    }

    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    ka_task_down_read(&udis_cb->addr_list_lock);
    addr_node = udis_find_dma_node(udis_cb, module_type, name);
    if (addr_node == NULL) {
        udis_err("Can not find addr node. (udevid=%u; module_type=%u; name=%s)\n", udevid, module_type, name);
        ret = -ENOMEM;
        goto out;
    }
    /* To avoid the device DMA address becoming unavailable due to responding unregister D2H message
     * during the copying process, it is necessary to hold the addr_list_lock throughout the copying process.
     */
    ret = udis_dma_sync_copy(udevid, udis_cb, addr_node);
    if (ret != 0) {
        udis_err("Dma copy failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid, module_type, name,ret);
        goto out;
    }
out:
    ka_task_up_read(&udis_cb->addr_list_lock);
    return ret;
}

STATIC int udis_link_dma_copy(unsigned int udevid, struct udis_ctrl_block *udis_cb,
    UDIS_UPDATE_TYPE addr_list_type)
{
    int ret;
    unsigned int node_num = 0;
    struct devdrv_dma_node *dma_nodes = NULL;
    struct udis_link_dma_nodes *link_dma_nodes = NULL;

    ka_task_down_read(&udis_cb->addr_list_lock);
    /* To avoid the device DMA address becoming unavailable due to responding unregister D2H message
     * during the copying process, it is necessary to hold the addr_list_lock throughout the copying process.
     * And the link_dma_nodes array may be modified in response to register and unregister messages.
     */
    link_dma_nodes = udis_get_link_dma_nodes(udevid);
    if ((link_dma_nodes == NULL) || (link_dma_nodes->dma_nodes == NULL)) {
        ka_task_up_read(&udis_cb->addr_list_lock);
        return -EFAULT;
    }

    dma_nodes = link_dma_nodes->dma_nodes;
    node_num = link_dma_nodes->node_num;
    if (node_num == 0) {
        ka_task_up_read(&udis_cb->addr_list_lock);
        /* link_dma_timestamp does not require lock protection.
         * Because the work item is guaranteed to be executed by at most one worker system-wide at any given time
         */
        udis_set_link_dma_timestamp(udevid, ktime_get_raw_ns() / NSEC_PER_MSEC);
        return 0;
    }

    ka_task_down_write(&udis_cb->udis_info_lock);
    ret = devdrv_dma_sync_link_copy(udevid, DEVDRV_DMA_DATA_COMMON, DEVDRV_DMA_WAIT_INTR,
        dma_nodes, node_num);
    if (ret != 0) {
        if (ret != -ENODEV) {
            udis_err("Link dma copy failed. (udevid=%u; addr_list_type=%llu; ret=%d)\n", udevid, addr_list_type, ret);
        }
        goto out;
    }

    udis_set_link_dma_timestamp(udevid, ktime_get_raw_ns() / NSEC_PER_MSEC);
out:
    ka_task_up_write(&udis_cb->udis_info_lock);
    ka_task_up_read(&udis_cb->addr_list_lock);
    return ret;
}

STATIC int udis_period_link_dma_task(unsigned int udevid, unsigned long privilege_data)
{
    int ret;
    unsigned long addr_list_type = privilege_data;
    struct udis_ctrl_block *udis_cb = NULL;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (addr_list_type >= UPDATE_TYPE_MAX)) {
        udis_err("Invalid param. (udevid=%u; addr_list_type=%lu)\n", udevid, addr_list_type);
        return -EINVAL;
    }

    udis_cb_read_lock(udevid);
    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_cb_read_unlock(udevid);
        udis_err("Get udis ctrl block failed. (udevid=%u; addr_list_type=%llu)\n", udevid, addr_list_type);
        return -ENODEV;
    }

    if (udis_cb->state != UDIS_DEV_READY) {
        udis_cb_read_unlock(udevid);
        return -EBUSY;
    }

    ret = udis_link_dma_copy(udevid, udis_cb, (UDIS_UPDATE_TYPE)addr_list_type);
    if (ret != 0) {
        udis_cb_read_unlock(udevid);
        return ret;
    }

    udis_cb_read_unlock(udevid);
    return 0;
}

int period_link_dma_task_init(unsigned int udevid)
{
    int ret;
    struct udis_timer_task task = {0};

    task.period_ms = UDIS_LINK_DMA_TASK_PERIOD_MS;
    task.cur_ms = 0;
    task.work_type = INDEPENDENCE_WORK;
    task.privilege_data = UPDATE_PERIOD_LEVEL_1;
    task.period_task_func = udis_period_link_dma_task;
    ret = sprintf_s(task.task_name, TASK_NAME_MAX_LEN, UDIS_LINK_DMA_TASK_NAME, udevid);
    if (ret < 0) {
        udis_err("sprintf_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -ENOMEM;
    }

    ret = hal_kernel_register_period_task(udevid, &task);
    if ((ret != 0) && (ret != -EEXIST)) {
        udis_err("Failed to register period link dma task. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    return 0;
}

void period_link_dma_task_uninit(unsigned int udevid)
{
    int ret;
    char task_name[TASK_NAME_MAX_LEN] = {0};

    ret = sprintf_s(task_name, TASK_NAME_MAX_LEN, UDIS_LINK_DMA_TASK_NAME, udevid);
    if (ret < 0) {
        udis_err("sprintf_s failed. (udevid=%u; ret=%d; name=%s)\n", udevid, ret, UDIS_LINK_DMA_TASK_NAME);
        return;
    }

    ret = hal_kernel_unregister_period_task(udevid, task_name);
    if ((ret != 0) && (ret != -ENODATA)) {
        udis_err("Failed to unregister period link dma task. (udevid=%u; task_name=%s; ret=%d)\n",
            udevid, task_name, ret);
        return;
    }

    return;
}

unsigned long long udis_get_link_dma_timestamp(unsigned int udevid)
{
    return g_link_dma_timestamp[udevid];
}

void udis_set_link_dma_timestamp(unsigned int udevid, unsigned long long timestamp)
{
    g_link_dma_timestamp[udevid] = timestamp;
}
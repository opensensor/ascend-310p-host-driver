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

#ifndef SVM_VERSION_ADAPT_H
#define SVM_VERSION_ADAPT_H

#include "ascend_kernel_hal.h"

/*
 * 5a5a is magic
 * For example, add a new version number
 * #define SVM_VERSION_0002 0x5a5a0002u
 * #define SVM_VERSION SVM_VERSION_0002
 */
#define SVM_VERSION_0000 0
#define SVM_VERSION_0001 0x5a5a0001u
#define SVM_VERSION_0002 0x5a5a0002u
#define SVM_VERSION_0003 0x5a5a0003u
#define SVM_VERSION SVM_VERSION_0003

struct svm_ver_adapt_msg_info {
    void *msg;
    u64 msg_len;
    u64 out_len;
};

void devmm_page_bitmap_version_adapt(u32 *src_bitmap, u32 src_version, u32 *dst_bitmap, u32 dst_version);
void devmm_setup_dev_msg_version_adapt(void *src_msg, u32 src_version, void *dst_msg, u32 dst_version);

u64 devmm_get_setup_dev_msg_len(u32 version, u64 extend_num);

#endif /* SVM_VERSION_ADAPT_H */

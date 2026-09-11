/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2019-10-15
 */

#ifndef _DRV_SRAM_COMM_H_
#define _DRV_SRAM_COMM_H_

#define MAP_SIZE 4096U
#define MAP_MASK (MAP_SIZE - 1)

#define BASE_SRAM_ADDR_V51_LITE         0x87200000U
#define TAISHAN_DRV_MAP_OFFSET_V51_LITE 0x35000U

#define BASE_SRAM_ADDR_V11              0x90300000U
#define TAISHAN_DRV_MAP_OFFSET_V11      0x17A00U

#define BASE_SRAM_ADDR_V51              0xC6F00000U
#define TAISHAN_DRV_MAP_OFFSET_V51      0x35000U

#define TAISHAN_DRV_MAP_SRAM_SIZE   4096U

#define SRAM_START_MAGIC    0x994c6473U
#define SRAM_END_MAGIC      0x19fd1c2fU
#define WORD(a, b, c, d)    ((a<<24) | (b<<16) | (c<<8) | d)
/* 0~32Byte used for dm-flag-tool */
struct dm_flag_data_struct {
    unsigned int start_magic;
    unsigned int mode;
    unsigned int rsvd1;
    unsigned int rsvd2;
    unsigned int end_magic;
};
#define DM_FLAG_SRAM_OFFSET   0x0
#define DM_FLAG_SRAM_SIZE   sizeof(struct dm_flag_data_struct)   // 20Byte
#define NORMAL_MODE         WORD('n', 'o', 'r', 'm')
#define DM_LOGGING_MODE     WORD('d', 'l', 'o', 'g')
#define DM_EIO_MODE         WORD('d', 'e', 'i', 'o')

/* 33~64Byte used for repartition scenes upgrade */
typedef struct repartition_flag_struct {
    unsigned int start_magic;
    unsigned int mode;
    unsigned int rsvd1;
    unsigned int rsvd2;
    unsigned int end_magic;
} repartition_flag_st;
#define REPART_FLAG_SRAM_OFFSET  0x20
#define REPART_FLAG_SRAM_SIZE sizeof(repartition_flag_st)   // 20Byte
#define REPART_FLAG_MAGIC   WORD('r', 'e', 'p', 'a')

#endif  /* _DRV_SRAM_COMM_H_ */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
#ifndef __DRV_TYPE_H__
#define __DRV_TYPE_H__
#ifndef AOS_LLVM_BUILD
typedef unsigned int mbox_msg_t;
typedef unsigned int mbox_msg_len_t;
typedef unsigned long virt_addr_t;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed int INT32;
typedef unsigned int UINT32;
typedef unsigned short UINT16;
typedef unsigned char UINT8;
typedef unsigned long long UINT64;
typedef signed long long INT64;

typedef int pid_t;
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned int uint32_t;

typedef unsigned long size_t;//lint !e410 !e452
#endif
#endif

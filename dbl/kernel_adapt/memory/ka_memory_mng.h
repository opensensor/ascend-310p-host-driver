/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-7-20
 */

#ifndef KA_MEMORY_MNG_H
#define KA_MEMORY_MNG_H

#include <linux/seq_file.h>

#ifndef __GFP_ACCOUNT

#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif

#endif

void ka_mem_mng_init(void);
void ka_mem_mng_uninit(void);
int ka_mem_stats_show(struct seq_file *seq, void *offset);
bool ka_is_enable_mem_record(void);
void ka_mem_record_status_reset(bool is_enable);
void ka_mem_alloc_stat_add(unsigned int module_id, size_t size, unsigned long va);
void ka_mem_alloc_stat_del(unsigned long va, unsigned int module_id);

#endif


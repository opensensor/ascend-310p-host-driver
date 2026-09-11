/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
* Create: 2019-10-15
*/

#ifndef __DAVINCI_INTF_PROCESS_H__
#define __DAVINCI_INTF_PROCESS_H__

struct davinci_intf_file_stru *create_file_proc_entry(
    struct davinci_intf_process_stru *proc,
    ka_file_t *file);
struct davinci_intf_file_stru *get_file_proc_entry(
    struct davinci_intf_process_stru *proc,
    ka_file_t *file);
unsigned int get_file_module_cnt(struct davinci_intf_process_stru *proc, const char *module_name);

void destory_file_proc_list(struct davinci_intf_process_stru *proc);
struct davinci_intf_process_stru *create_process_entry(
    struct davinci_intf_stru *cb,
    pid_t proc_pid,
    u64 start_time);
struct davinci_intf_process_stru *get_process_entry(
    struct davinci_intf_stru *cb,
    pid_t proc_pid,
    u64 start_time);
struct davinci_intf_process_stru *get_process_entry_latest(
    struct davinci_intf_stru *cb,
    pid_t proc_pid);
void free_process_entry(struct davinci_intf_process_stru *proc);
void destroy_process_list(struct davinci_intf_stru *cb);
int add_file_to_list(struct davinci_intf_stru *cb, ka_file_t *file,
    struct davinci_intf_private_stru *private_data);
int add_module_to_list(struct davinci_intf_stru *cb,
    struct davinci_intf_private_stru *file_private,
    ka_file_t *file,
    const char *module_name);
void remove_file_from_list(struct davinci_intf_stru *cb,
    pid_t owner_pid,
    ka_file_t *file);
int check_module_file_close(struct davinci_intf_stru *cb,
    const char *module_name);
int check_module_file_close_in_process(
    struct davinci_intf_process_stru *proc,
    const char *module_name);
void destroy_free_node_and_list(struct davinci_intf_process_stru *proc,
    struct davinci_intf_private_stru *private_data);

#endif

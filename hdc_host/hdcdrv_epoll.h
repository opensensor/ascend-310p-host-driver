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

#ifndef _HDCDRV_EPOLL_H_
#define _HDCDRV_EPOLL_H_

#include "devdrv_manager_comm.h"
#include "hdcdrv_cmd.h"

#include "ka_list_pub.h"
#include "ka_task_pub.h"

#define HDCDRV_INV_DIVISOR 0x1999999A /* this is (2^32)/10 */

#if defined (CFG_FEATURE_SRIOV) && defined (HDC_ENV_DEVICE)
/* device not support docker */
#define HDCDRV_DOCKER_MAX_NUM 1U
#define HDCDRV_PHY_HOST_ID 0
#else
#define HDCDRV_DOCKER_MAX_NUM (MAX_DOCKER_NUM + 1U) /* 1(host)+69(MAX_DOCKER_NUM) */
#define HDCDRV_PHY_HOST_ID (MAX_DOCKER_NUM) /* host id */
#endif

struct hdcdrv_ctx;

struct hdcdrv_epoll_list_node {
    struct hdcdrv_event events;
    void *instance;
    char session_id[HDCDRV_SID_LEN];
    ka_list_head_t list;
};

struct hdcdrv_epoll_fd {
    int valid;
    int fd;
    int docker_id;
    int vm_id;
    u64 pid;
    int size;
    int event_num;
    int wait_flag;
    ka_mutex_t mutex;
    ka_wait_queue_head_t wq;
    ka_list_head_t service_list;
    ka_list_head_t session_list;
    struct hdcdrv_event *events;
    struct hdcdrv_ctx *ctx;
    u64 task_start_time;
};

struct hdcdrv_epoll_docker {
    struct hdcdrv_epoll_fd *epfds;
};

struct hdcdrv_epoll {
    ka_mutex_t mutex;
    struct hdcdrv_epoll_docker epoll_docks[HDCDRV_DOCKER_MAX_NUM];
    int *vm_alloc_cnt;
};

long hdcdrv_kernel_epoll_alloc_fd(int size, int *epfd, const int *magic_num);
long hdcdrv_kernel_epoll_ctl(int epfd, int magic_num, int op, 
    unsigned int event, int para1, const char *para2, unsigned int para2_len);
long hdcdrv_kernel_epoll_wait(int epfd, int magic_num, int timeout, int *event_num,
    unsigned int event[], unsigned int event_len, int para1[],
    unsigned int para1_len, int para2[], unsigned int para2_len);
long hdcdrv_kernel_epoll_free_fd(int epfd, int magic_num);
extern void hdcdrv_epoll_recycle_fd(struct hdcdrv_ctx *ctx);
extern void hdcdrv_epoll_wake_up(struct hdcdrv_epoll_fd *epfd);
extern int hdcdrv_epoll_init(struct hdcdrv_epoll *epolls);
extern void hdcdrv_epoll_uninit(struct hdcdrv_epoll *epolls);
extern int *hdcdrv_epoll_get_dev_id_ptr(union hdcdrv_cmd *cmd_data);
extern long hdcdrv_epoll_wait(struct hdcdrv_cmd_epoll_wait *cmd, int mode);

#endif  // _HDCDRV_EPOLL_H_

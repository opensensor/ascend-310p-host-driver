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

#ifndef _HDCDRV_CMD_H_
#define _HDCDRV_CMD_H_

#include "hdcdrv_cmd_enum.h"

#define HDCDRV_CHAR_DRIVER_NAME "hisi_hdc"

static inline int hdcdrv_get_max_support_dev(void)
{
#ifdef HDC_ENV_DEVICE
#ifdef CFG_FEATURE_SRIOV
    return (int)HDCDRV_SUPPORT_MAX_DEV;
#else
    return (int)hdcdrv_dev_num;
#endif
#else
    return (int)HDCDRV_SUPPORT_MAX_DEV;
#endif
}

struct hdcdrv_timeout {
    unsigned int send_timeout;
    unsigned int recv_timeout;
    unsigned int fast_send_timeout;
    unsigned int fast_recv_timeout;
};

struct hdcdrv_event {
    unsigned int events;
    int sub_data;       /* trans to user in epoll wait return */
    unsigned long long data; /* trans to user in epoll wait return */
};

struct hdcdrv_mem_stat {
    int mem_nums[HDCDRV_FAST_MEM_TYPE_MAX];
    unsigned long long mem_size[HDCDRV_FAST_MEM_TYPE_MAX];
};

struct hdcdrv_cmd_common {
    int ret;
    int dev_id;
    unsigned long long pid;
};

struct hdcdrv_cmd_get_peer_dev_id {
    int ret;
    int dev_id;         /* input */
    unsigned long long pid;
    int peer_dev_id;    /* output */
};

struct hdcdrv_cmd_config {
    int ret;
    int dev_id;
    unsigned long long pid;     /* input */
    int segment; /* input,output */
};

struct hdcdrv_cmd_set_service_level {
    int ret;
    int dev_id;
    unsigned long long pid;
    int service_type; /* input */
    int level;        /* input */
};

struct hdcdrv_cmd_server_create {
    int ret;
    int dev_id;       /* input */
    unsigned long long pid;          /* input */
    int service_type; /* input */
};

struct hdcdrv_cmd_server_destroy {
    int ret;
    int dev_id;       /* input */
    unsigned long long pid;
    int service_type; /* input */
};

struct hdcdrv_cmd_client_destroy {
    int ret;
    int dev_id;       /* input */
    unsigned long long pid;
    int service_type; /* input */
};

struct hdcdrv_cmd_accept {
    int ret;
    int dev_id;                      /* input */
    unsigned long long pid;
    int service_type;                /* input */
    int session;                     /* output */
    unsigned int session_cur_alloc_idx; /* output */
};

struct hdcdrv_cmd_connect {
    int peer_pid;                    /* input */
    int dev_id;                      /* input */
    unsigned long long pid;
    int service_type;                /* input */
    int session;                     /* output */
    unsigned int timeout;            /* output */
    unsigned int session_cur_alloc_idx; /* output */
};

struct hdcdrv_cmd_close {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                     /* input */
    unsigned int unique_val;
    unsigned long long task_start_time;
    unsigned int session_cur_alloc_idx; /* output */
};

struct hdcdrv_cmd_send {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                     /* input */
    void *src_buf;                   /* input */
    int len;                         /* input */
    void *pool_buf;                  /* input */
    unsigned long long pool_addr;    /* input */
    int wait_flag;                   /* input */
    unsigned int timeout;            /* input */
};

struct hdcdrv_cmd_recv_peek {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                     /* input */
    int len;                         /* output, if remote session close, this is 0 */
    int wait_flag;                   /* input */
    unsigned int timeout;            /* input */
    int group_flag;                  /* input */
    int count;                       /* output, if remote session close, this is 0 */
    unsigned int session_cur_alloc_idx;
};

struct hdcdrv_cmd_recv {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                     /* input */
    void *dst_buf;                   /* output */
    int len;                         /* input */
    void *pool_buf;                  /* output */
    int out_len;                     /* output */
    int buf_count;                   /* output, for vhdc used */
    int group_flag;                  /* input */
    void *buf_list[HDCDRV_SESSION_RX_LIST_MAX_PKT];         /* output, for vhdc used in VM */
    unsigned int buf_len[HDCDRV_SESSION_RX_LIST_MAX_PKT];   /* output, for vhdc used in VM */
};

struct hdcdrv_cmd_set_session_owner {
    int ret;
    int dev_id;
    unsigned long long pid;                         /* owner pid */
    unsigned long long ppid;
    int session;                     /* input */
};

struct hdcdrv_cmd_get_session_attr {
    int ret;
    int dev_id;
    unsigned long long pid;
    int cmd_type;                    /* input */
    int session;                     /* input */
    int output;                      /* output */
    unsigned int session_cur_alloc_idx;
};

struct hdcdrv_cmd_set_session_timeout {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                     /* input */
    struct hdcdrv_timeout timeout;   /* input */
};

struct hdcdrv_cmd_get_uid_stat {
    int ret;
    int dev_id;             /* input, -1 not care */
    unsigned long long pid;
    int session;            /* input, -1 not care */
    unsigned int euid;
    unsigned int uid;
    int root_privilege;
};

struct hdcdrv_cmd_alloc_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int type;      /* input */
    unsigned int len;       /* input */
    unsigned long long va;  /* input */
    unsigned int page_type; /* input */
    int map;                /* input */
};

struct hdcdrv_cmd_free_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int type;          /* input */
    unsigned int len;           /* output */
    unsigned int page_type;     /* output */
    unsigned long long va;      /* input */
};

struct hdcdrv_cmd_fast_send {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                      /* input */
    int wait_flag;                    /* input */
    unsigned long long src_data_addr; /* input */
    unsigned long long dst_data_addr; /* input */
    unsigned long long src_ctrl_addr; /* input */
    unsigned long long dst_ctrl_addr; /* input */
    int data_len;                     /* input */
    int ctrl_len;                     /* input */
    unsigned int timeout;             /* input */
};

struct hdcdrv_cmd_fast_recv {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                     /* input */
    int wait_flag;                   /* input */
    unsigned int timeout;            /* input */
    unsigned long long data_addr;    /* output */
    unsigned long long ctrl_addr;    /* output */
    int data_len;                    /* output */
    int ctrl_len;                    /* output */
};

struct hdcdrv_cmd_dma_map {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int type;     /* input */
    unsigned long long va; /* input */
};

struct hdcdrv_cmd_dma_unmap {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int type;     /* input */
    unsigned long long va; /* input */
};

struct hdcdrv_cmd_dma_remap {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int type;     /* input */
    unsigned long long va; /* input */
};

struct hdcdrv_cmd_register_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int type;      /* input */
    unsigned int len;       /* input */
    unsigned long long va;  /* input */
    unsigned int flag; /* input */
};

struct hdcdrv_cmd_unregister_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int type;          /* input */
    unsigned int len;           /* output */
    unsigned int page_type;     /* output */
    unsigned long long va;      /* input */
};

struct hdcdrv_cmd_wait_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    int session;                     /* input */
    int timeout;                     /* input */
    unsigned int result_type;        /* input */
    unsigned long long data_addr;    /* output */
    unsigned long long ctrl_addr;    /* output */
    int data_len;                    /* output */
    int ctrl_len;                    /* output */
    int result;                      /* output */
};

struct hdcdrv_cmd_epoll_alloc_fd {
    int ret;
    int dev_id;
    unsigned long long pid;
    int size;               /* input */
    int epfd;               /* output */
};

struct hdcdrv_cmd_epoll_free_fd {
    int ret;
    int dev_id;
    unsigned long long pid;
    int epfd;               /* input */
};

struct hdcdrv_cmd_epoll_ctl {
    int ret;
    int dev_id;
    unsigned long long pid;
    int epfd;                   /* input */
    int op;                     /* input */
    int para1;                  /* input, service:dev_id, session:session_fd */
    int para2;                  /* input, service:service_type */
    struct hdcdrv_event event;  /* input */
};

struct hdcdrv_cmd_epoll_wait {
    int ret;
    int dev_id;
    unsigned long long pid;
    int epfd;                   /* input */
    int timeout;                /* input */
    int maxevents;              /* input */
    int ready_event;            /* output */
    struct hdcdrv_event *event;  /* output */
    struct hdcdrv_event vevent[HDCDRV_VEPOLL_EVENT_MAX];
};

struct hdcdrv_cmd_get_page_size {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int page_size;
    unsigned int hpage_size;
    unsigned int page_bit;
};

struct hdcdrv_cmd_get_session_info {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned int fid;
    int session_fd;
};

struct hdcdrv_cmd_client_wakeup_wait {
    int ret;
    int dev_id;       /* input */
    unsigned long long pid;
    int service_type; /* input */
};

struct hdcdrv_cmd_server_wakeup_wait {
    int ret;
    int dev_id;       /* input */
    unsigned long long pid;
    int service_type; /* input */
};

union hdcdrv_cmd {
    struct hdcdrv_cmd_common cmd_com;
    struct hdcdrv_cmd_get_peer_dev_id get_peer_dev_id;
    struct hdcdrv_cmd_config config;
    struct hdcdrv_cmd_set_service_level set_level;
    struct hdcdrv_cmd_client_destroy client_destroy;
    struct hdcdrv_cmd_server_create server_create;
    struct hdcdrv_cmd_server_destroy server_destroy;
    struct hdcdrv_cmd_accept accept;
    struct hdcdrv_cmd_connect conncet;
    struct hdcdrv_cmd_close close;
    struct hdcdrv_cmd_send send;
    struct hdcdrv_cmd_recv_peek recv_peek;
    struct hdcdrv_cmd_recv recv;
    struct hdcdrv_cmd_set_session_owner set_owner;
    struct hdcdrv_cmd_get_session_attr get_session_attr;
    struct hdcdrv_cmd_set_session_timeout set_session_timeout;
    struct hdcdrv_cmd_get_uid_stat get_uid_stat;
    struct hdcdrv_cmd_alloc_mem alloc_mem;
    struct hdcdrv_cmd_free_mem free_mem;
    struct hdcdrv_cmd_fast_send fast_send;
    struct hdcdrv_cmd_fast_recv fast_recv;
    struct hdcdrv_cmd_dma_map dma_map;
    struct hdcdrv_cmd_dma_unmap dma_unmap;
    struct hdcdrv_cmd_dma_remap dma_remap;
    struct hdcdrv_cmd_register_mem register_mem;
    struct hdcdrv_cmd_unregister_mem unregister_mem;
    struct hdcdrv_cmd_wait_mem wait_mem;
    struct hdcdrv_cmd_epoll_alloc_fd epoll_alloc_fd;
    struct hdcdrv_cmd_epoll_free_fd epoll_free_fd;
    struct hdcdrv_cmd_epoll_ctl epoll_ctl;
    struct hdcdrv_cmd_epoll_wait epoll_wait;
    struct hdcdrv_cmd_get_page_size get_page_size;
    struct hdcdrv_cmd_get_session_info get_session_info;
    struct hdcdrv_cmd_client_wakeup_wait client_wakeup_wait;
    struct hdcdrv_cmd_server_wakeup_wait server_wakeup_wait;
};

#endif

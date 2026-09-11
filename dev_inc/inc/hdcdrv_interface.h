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

#ifndef _HDCDRV_INTERFACE_H_
#define _HDCDRV_INTERFACE_H_

#define HDCDRV_RX_CONTINUE 0
#define HDCDRV_RX_FINISH 1

#define HDCDRV_TX_TIMEOUT (-28)

struct hdcdrv_data_info {
    int session_fd;
    int data_type;
    u64 src_addr;
    u32 len;
};

typedef int (*hdcdrv_sessoin_connect_notify)(int dev_id, int vfid, int peer_pid, int local_pid);
typedef int (*hdcdrv_sessoin_close_notify)(int dev_id, int vfid, int peer_pid, int local_pid);
typedef int (*hdcdrv_sessoin_data_in_notify)(int dev_id, int vfid, int local_pid,
    struct hdcdrv_data_info data);

struct hdcdrv_session_notify {
    hdcdrv_sessoin_connect_notify connect_notify;
    hdcdrv_sessoin_close_notify close_notify;
    hdcdrv_sessoin_data_in_notify data_in_notify;
};

void hdcdrv_session_notify_register(int service_type, struct hdcdrv_session_notify *notify);
void hdcdrv_session_notify_unregister(int service_type);
/* value 0 means rx list is not full, value 1 means rx list is full */
int hdcdrv_get_session_rx_list_status(int session_fd, int *value);

long hdcdrv_kernel_epoll_alloc_fd(int size, int *epfd, const int *magic_num);
long hdcdrv_kernel_epoll_free_fd(int epfd, int magic_num);
long hdcdrv_kernel_epoll_ctl(int epfd, int magic_num, int op,
    unsigned int event, int para1, const char *para2, unsigned int para2_len);
long hdcdrv_kernel_epoll_wait(int epfd, int magic_num, int timeout, int *event_num,
    unsigned int event[], unsigned int event_len, int para1[],
    unsigned int para1_len, int para2[], unsigned int para2_len);
long hdcdrv_kernel_server_create(int dev_id, int service_type);
long hdcdrv_kernel_accept(int dev_id, int service_type, int *session, const char *session_id);
long hdcdrv_kernel_send_timeout(int session, const char *session_id, void *buf, int len, int timeout);
long hdcdrv_kernel_recv_peek(int session, const char *session_id, int *len);
long hdcdrv_kernel_recv(int session, const char *session_id, void *buf, int len, int *out_len);
int hdcdrv_get_segment(void);
long hdcdrv_kernel_close(int session, const char *session_id);
long hdcdrv_kernel_server_destroy(int dev_id, int service_type);
long hdcdrv_kernel_get_session_vfid(int session, int *value);
long hdcdrv_kernel_get_session_peer_create_pid(int session, int *value);

struct hdcdrv_register_symbol {
    struct module *module_ptr;
    int (*wake_up_context_status)(pid_t pid, u32 devid, u32 status);
};

void hdcdrv_register_symbol_from_tsdrv(struct hdcdrv_register_symbol *module_symbol);
void hdcdrv_unregister_symbol_from_tsdrv(void);

#endif /* _HDCDRV_INTERFACE_H_ */

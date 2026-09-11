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

#include "hdcdrv_cmd.h"
#include "hdcdrv_core.h"
#include "hdcdrv_epoll.h"

#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_barrier_pub.h"
#include "ka_kernel_def_pub.h"

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif


struct hdcdrv_epoll *hdc_epoll = NULL;

STATIC int hdcdrv_epoll_vm_resource_check(u32 vm_id)
{
#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_VFIO_DEVICE)
    if ((vm_id != HDCDRV_DEFAULT_VM_ID) &&
        (hdc_epoll->vm_alloc_cnt[vm_id] >= (int)(HDCDRV_EPOLL_FD_NUM / HDCDRV_VM_NUM))) {
        return HDCDRV_NO_EPOLL_FD;
    }
#endif
    return HDCDRV_OK;
}

STATIC long hdcdrv_epoll_fd_check(int fd, u32 docker_id, u64 check_pid)
{
    struct hdcdrv_epoll_fd *epfd = NULL;
    u64 task_start_time = hdcdrv_get_task_start_time();

    if ((fd >= (int)HDCDRV_EPOLL_FD_NUM) || (fd < 0)) {
        hdcdrv_err_limit("Input pararmeter is error. (fd=%d)\n", fd);
        return HDCDRV_PARA_ERR;
    }

    if (docker_id >= HDCDRV_DOCKER_MAX_NUM) {
        hdcdrv_err("Input pararmeter is error. (docker_id=%d)\n", docker_id);
        return HDCDRV_PARA_ERR;
    }

    epfd = &hdc_epoll->epoll_docks[docker_id].epfds[fd];

    if (epfd->valid == HDCDRV_INVALID) {
        hdcdrv_err_limit("epfd is invalid. (epfd=%d)\n", fd);
        return HDCDRV_PARA_ERR;
    }

    if (epfd->pid != check_pid) {
        hdcdrv_err("Current pid is invalid for epfd. (pid=%llu; fd=%d)\n", hdcdrv_get_pid(), fd);
        return HDCDRV_PARA_ERR;
    }

    if (hdcdrv_session_task_start_time_compare(epfd->task_start_time, task_start_time)) {
        hdcdrv_err("Current task start time is invalid for epfd.(docker_id=%d)\n", docker_id);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_epoll_alloc_fd_init(struct hdcdrv_ctx *ctx, struct hdcdrv_epoll_fd *epfd,
    struct hdcdrv_event *event, const struct hdcdrv_cmd_epoll_alloc_fd *cmd)
{
    if (epfd != NULL) { /* just for clear fortify warning */
        epfd->pid = cmd->pid;
        epfd->task_start_time = hdcdrv_get_task_start_time();
        epfd->size = cmd->size;
        epfd->event_num = 0;
        epfd->wait_flag = 0;
        epfd->events = event;
        epfd->ctx = ctx;
        if ((ctx != NULL) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
            ctx->epfd = epfd;
        }
    }
}

STATIC long hdcdrv_epoll_alloc_fd(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_epoll_alloc_fd *cmd)
{
    struct hdcdrv_event *event = NULL;
    struct hdcdrv_epoll_fd *epfd = NULL;
    int i;
    int fd = -1;
    u32 docker_id;
    u32 vm_id;

    if (((cmd->size > HDCDRV_EPOLL_FD_EVENT_NUM) || (cmd->size <= 0))) {
        hdcdrv_err("Input pararmeter is error. (size=%d)\n", cmd->size);
        return HDCDRV_PARA_ERR;
    }

    if ((ctx != NULL) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX) && (ctx->epfd != NULL)) {
        hdcdrv_err("ctx_epfd has been polled. (dev_id=%d)\n", cmd->dev_id);
        return HDCDRV_PARA_ERR;
    }

    docker_id = hdcdrv_get_container_id();
    if (docker_id >= HDCDRV_DOCKER_MAX_NUM) {
        hdcdrv_err("docker_id is illegal. (docker_id=%u)\n", docker_id);
        return HDCDRV_ERR;
    }

    vm_id = hdcdrv_get_vmid_from_pid(cmd->pid);
    if (vm_id >= HDCDRV_MAX_VM_NUM) {
        hdcdrv_err("vm_id is illegal. (vm_id=%u)\n", vm_id);
        return HDCDRV_ERR;
    }

    /*lint -e647 */
    event = (struct hdcdrv_event *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_event) * (u64)cmd->size, KA_SUB_MODULE_TYPE_3);
    /*lint +e647 */
    if (event == NULL) {
        hdcdrv_err("Calling malloc failed.\n");
        return HDCDRV_ERR;
    }
    (void)memset_s(event, sizeof(struct hdcdrv_event) * cmd->size, 0, sizeof(struct hdcdrv_event) * cmd->size);
    ka_task_mutex_lock(&hdc_epoll->mutex);

    if (hdcdrv_epoll_vm_resource_check(vm_id) != HDCDRV_OK) {
        ka_task_mutex_unlock(&hdc_epoll->mutex);
        goto free_event;
    }

    /* First find the session in the idle state */
    for (i = 0; i < (int)HDCDRV_EPOLL_FD_NUM; i++) {
        epfd = &hdc_epoll->epoll_docks[docker_id].epfds[i];
        if (epfd->valid == HDCDRV_INVALID) {
            fd = i;
            epfd->valid = HDCDRV_VALID;
            hdc_epoll->vm_alloc_cnt[vm_id]++;
            break;
        }
    }

    ka_task_mutex_unlock(&hdc_epoll->mutex);

    if (fd < 0) {
        goto free_event;
    }

    cmd->epfd = fd;
    epfd->vm_id = (int)vm_id;
    hdcdrv_epoll_alloc_fd_init(ctx, epfd, event, cmd);

    return HDCDRV_OK;

free_event:
    hdcdrv_kvfree(event, KA_SUB_MODULE_TYPE_3);
    event = NULL;
    hdcdrv_err_limit("VM no more epfd. (vm_id=%u)\n", vm_id);
    return HDCDRV_NO_EPOLL_FD;
}

STATIC void hdcdrv_epoll_clear_service(struct hdcdrv_epoll_fd *epfd)
{
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_epoll_list_node *node = NULL;
    struct hdcdrv_service *service = NULL;

    if (!ka_list_empty_careful(&epfd->service_list)) {
        ka_list_for_each_safe(pos, n, &epfd->service_list) {
            node = ka_list_entry(pos, struct hdcdrv_epoll_list_node, list);
            service = (struct hdcdrv_service *)node->instance;
            service->epfd = NULL;
            ka_list_del(&node->list);
            hdcdrv_kvfree(node, KA_SUB_MODULE_TYPE_3);
            node = NULL;
        }
    }
}

STATIC void hdcdrv_epoll_clear_session(struct hdcdrv_epoll_fd *epfd)
{
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_epoll_list_node *node = NULL;
    struct hdcdrv_session *session = NULL;

    if (!ka_list_empty_careful(&epfd->session_list)) {
        ka_list_for_each_safe(pos, n, &epfd->session_list)
        {
            node = ka_list_entry(pos, struct hdcdrv_epoll_list_node, list);
            session = (struct hdcdrv_session *)node->instance;
            session->epfd = NULL;
            ka_list_del(&node->list);
            hdcdrv_kvfree(node, KA_SUB_MODULE_TYPE_3);
            node = NULL;
        }
    }
}

STATIC void hdcdrv_epoll_free_fd_handle(struct hdcdrv_epoll_fd *epfd)
{
    ka_task_mutex_lock(&epfd->mutex);
    hdcdrv_epoll_clear_service(epfd);
    hdcdrv_epoll_clear_session(epfd);
    ka_task_mutex_unlock(&epfd->mutex);

    ka_task_mutex_lock(&hdc_epoll->mutex);
    if (epfd->valid == HDCDRV_VALID) {
        ka_task_mutex_lock(&epfd->mutex);
        epfd->valid = HDCDRV_INVALID;
        hdcdrv_kvfree(epfd->events, KA_SUB_MODULE_TYPE_3);
        epfd->events = NULL;
        hdc_epoll->vm_alloc_cnt[epfd->vm_id]--;
        ka_wmb();
        ka_task_wake_up_interruptible(&epfd->wq);
        if (epfd->ctx != NULL) {
            epfd->ctx->epfd = NULL;
            epfd->ctx = NULL;
        }
        ka_task_mutex_unlock(&epfd->mutex);
    }
    ka_task_mutex_unlock(&hdc_epoll->mutex);
}

STATIC long hdcdrv_epoll_free_fd(const struct hdcdrv_ctx *ctx, const struct hdcdrv_cmd_epoll_free_fd *cmd)
{
    struct hdcdrv_epoll_fd *epfd = NULL;
    long ret;
    u32 docker_id;

    docker_id = hdcdrv_get_container_id();

    ret = hdcdrv_epoll_fd_check(cmd->epfd, docker_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_epoll_fd_check error.\n");
        return ret;
    }

    epfd = &hdc_epoll->epoll_docks[docker_id].epfds[cmd->epfd];
    if (epfd->ctx != ctx) {
        hdcdrv_err("epfd_ctx not match. (epfd=%d)\n", cmd->epfd);
        return HDCDRV_PARA_ERR;
    }

    hdcdrv_epoll_free_fd_handle(epfd);
    return HDCDRV_OK;
}

void hdcdrv_epoll_recycle_fd(struct hdcdrv_ctx *ctx)
{
    if (ctx->epfd != NULL) {
        hdcdrv_info("Recycle epoll. (fd=%d)\n", ctx->epfd->fd);
        hdcdrv_epoll_free_fd_handle(ctx->epfd);
    }
}

STATIC long hdcdrv_epoll_add_service(struct hdcdrv_epoll_fd *epfd, struct hdcdrv_epoll_list_node *node,
    int dev_id, int service_type, u32 fid)
{
    struct hdcdrv_service *service = NULL;
    long ret;

    ret = hdcdrv_dev_para_check(dev_id, service_type);
    if (ret != 0) {
        hdcdrv_err("Calling hdcdrv_dev_para_check failed. (epfd=%d; dev_id=%d; service_type=%d)\n",
                   epfd->fd, dev_id, service_type);
        return ret;
    }

#ifdef CFG_FEATURE_VFIO_DEVICE
    if (service_type == HDCDRV_SERVICE_TYPE_TDT) {
        fid = hdcdrv_get_fid(epfd->pid);
    }
#endif

    service = hdcdrv_search_service((u32)dev_id, fid, service_type, epfd->pid);
    if (service->listen_status == HDCDRV_INVALID) {
        hdcdrv_err("listen_status is invalid. (epfd=%d; dev_id=%d; fid=%u; service_type=%d)\n",
            epfd->fd, dev_id, fid, service_type);
        return HDCDRV_ERR;
    }

    if (service->listen_pid != epfd->pid) {
        hdcdrv_err("Current pid is invalid. (pid=%llu; epfd=%d; dev_id=%d; service_type=%d; "
            "epfd_pid=%llu; listen_pid=%llu)\n",
            hdcdrv_get_pid(), epfd->fd, dev_id, service_type, epfd->pid, service->listen_pid);
        return HDCDRV_NO_PERMISSION;
    }

    if (service->epfd != NULL) {
        hdcdrv_err("Service has been polled. (epfd=%d; dev_id=%d; fid=%u; service_type=%d; pid=%llu)\n",
            epfd->fd, dev_id, fid, service_type, epfd->pid);
        return HDCDRV_ERR;
    }

    service->epfd = epfd;
    node->instance = (void *)service;
    ka_list_add(&node->list, &epfd->service_list);

    return HDCDRV_OK;
}

STATIC long hdcdrv_epoll_add_session(struct hdcdrv_epoll_fd *epfd, struct hdcdrv_epoll_list_node *node,
    int session_fd)
{
    struct hdcdrv_session *session = NULL;
    long ret;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0)) {
        hdcdrv_err("Input pararmeter is error. (epfd=%d; session_fd=%d)\n", epfd->fd, session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];
    if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) {
        hdcdrv_warn("Session has already closed. (epfd=%d; session=%d)\n",  epfd->fd, session_fd);
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    ret = hdcdrv_check_session_owner(session, epfd->pid);
    if (ret != 0) {
        hdcdrv_err("Current pid is invalid. (pid=%llu; epfd=%d; session_fd=%d)\n",
            hdcdrv_get_pid(), epfd->fd, session_fd);
        return ret;
    }

    if (session->epfd != NULL) {
        hdcdrv_err("Service has been polled. (epfd=%d; session_fd=%d)\n", epfd->fd, session_fd);
        return HDCDRV_ERR;
    }

    session->epfd = epfd;
    node->instance = (void *)session;
    ka_list_add(&node->list, &epfd->session_list);

    return HDCDRV_OK;
}

STATIC long hdcdrv_epoll_add_event(struct hdcdrv_epoll_fd *epfd,
    const struct hdcdrv_event *event, int para1, int para2, u32 fid)
{
    struct hdcdrv_epoll_list_node *node = NULL;
    long ret;

    node = (struct hdcdrv_epoll_list_node *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_epoll_list_node), KA_SUB_MODULE_TYPE_3);
    if (node == NULL) {
        hdcdrv_err("Calling malloc failed.\n");
        return HDCDRV_ERR;
    }

    node->events = *event;

    ka_task_mutex_lock(&epfd->mutex);

    if (epfd->event_num >= epfd->size) {
        hdcdrv_err("epfd event full. (epfd=%d)\n", epfd->fd);
        ret = HDCDRV_ERR;
        goto ERROR;
    }

    if ((event->events & HDCDRV_EPOLL_CONN_IN) != 0) {
        ret = hdcdrv_epoll_add_service(epfd, node, para1, para2, fid);
        if (ret != 0) {
            goto ERROR;
        }
    } else {
        ret = hdcdrv_epoll_add_session(epfd, node, para1);
        if (ret != 0) {
            goto ERROR;
        }
    }

    epfd->event_num++;

    ka_task_mutex_unlock(&epfd->mutex);

    return HDCDRV_OK;

ERROR:
    ka_task_mutex_unlock(&epfd->mutex);
    hdcdrv_kvfree(node, KA_SUB_MODULE_TYPE_3);
    node = NULL;
    return ret;
}

STATIC long hdcdrv_epoll_del_service(struct hdcdrv_epoll_fd *epfd, int dev_id, u32 fid, int service_type)
{
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_epoll_list_node *node = NULL;
    struct hdcdrv_service *service = NULL;
    long ret = HDCDRV_ERR;

    if ((dev_id >= hdcdrv_get_max_support_dev()) || (dev_id < 0)) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d)\n", dev_id);
        return HDCDRV_PARA_ERR;
    }

    if ((service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (service_type < 0)) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d; service_type=%d)\n", dev_id, service_type);
        return HDCDRV_PARA_ERR;
    }

#ifdef CFG_FEATURE_VFIO_DEVICE
    if (service_type == HDCDRV_SERVICE_TYPE_TDT) {
        fid = hdcdrv_get_fid(epfd->pid);
    }
#endif

    service = hdcdrv_search_service((u32)dev_id, fid, service_type, epfd->pid);
    if (!ka_list_empty_careful(&epfd->service_list)) {
        ka_list_for_each_safe(pos, n, &epfd->service_list)
        {
            node = ka_list_entry(pos, struct hdcdrv_epoll_list_node, list);
            if ((struct hdcdrv_service *)node->instance == service) {
                service->epfd = NULL;
                ka_list_del(&node->list);
                hdcdrv_kvfree(node, KA_SUB_MODULE_TYPE_3);
                node = NULL;
                ret = HDCDRV_OK;
                break;
            }
        }
    }

    return ret;
}

STATIC long hdcdrv_epoll_del_session(struct hdcdrv_epoll_fd *epfd, int session_fd)
{
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_epoll_list_node *node = NULL;
    struct hdcdrv_session *session = NULL;
    long ret = HDCDRV_ERR;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0)) {
        hdcdrv_err("Input pararmeter is error. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];
    if (!ka_list_empty_careful(&epfd->session_list)) {
        ka_list_for_each_safe(pos, n, &epfd->session_list)
        {
            node = ka_list_entry(pos, struct hdcdrv_epoll_list_node, list);
            if ((struct hdcdrv_session *)node->instance == session) {
                session->epfd = NULL;
                ka_list_del(&node->list);
                hdcdrv_kvfree(node, KA_SUB_MODULE_TYPE_3);
                node = NULL;
                ret = HDCDRV_OK;
                break;
            }
        }
    }

    return ret;
}

STATIC long hdcdrv_epoll_del_event(struct hdcdrv_epoll_fd *epfd,
    const struct hdcdrv_event *event, int para1, int para2, u32 fid)
{
    long ret;

    ka_task_mutex_lock(&epfd->mutex);

    if ((event->events & HDCDRV_EPOLL_CONN_IN) != 0) {
        ret = hdcdrv_epoll_del_service(epfd, para1, fid, para2);
    } else {
        ret = hdcdrv_epoll_del_session(epfd, para1);
    }

    if (ret == HDCDRV_OK) {
        epfd->event_num--;
    }

    ka_task_mutex_unlock(&epfd->mutex);

    return ret;
}

STATIC long hdcdrv_epoll_ctl(struct hdcdrv_cmd_epoll_ctl *cmd, u32 fid)
{
    struct hdcdrv_epoll_fd *epfd = NULL;
    long ret;
    u32 docker_id;

    docker_id = hdcdrv_get_container_id();

    ret = hdcdrv_epoll_fd_check(cmd->epfd, docker_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        hdcdrv_err_limit("Calling hdcdrv_epoll_fd_check error.\n");
        return ret;
    }

    if ((cmd->op != HDCDRV_EPOLL_OP_ADD) && (cmd->op != HDCDRV_EPOLL_OP_DEL)) {
        hdcdrv_err("Parameter cmd is invalid, (epfd=%d; op=%d)\n", cmd->epfd, cmd->op);
        return HDCDRV_PARA_ERR;
    }

    epfd = &hdc_epoll->epoll_docks[docker_id].epfds[cmd->epfd];

    if (cmd->op == HDCDRV_EPOLL_OP_ADD) {
        ret = hdcdrv_epoll_add_event(epfd, &cmd->event, cmd->para1, cmd->para2, fid);
    } else {
        ret = hdcdrv_epoll_del_event(epfd, &cmd->event, cmd->para1, cmd->para2, fid);
    }

    return ret;
}

void hdcdrv_epoll_wake_up(struct hdcdrv_epoll_fd *epfd)
{
    if (epfd != NULL) {
        epfd->wait_flag = 1;
        ka_wmb();
        ka_task_wake_up_interruptible(&epfd->wq);
    }
}

STATIC int hdcdrv_epoll_service_event_num(struct hdcdrv_epoll_fd *epfd, int num)
{
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_epoll_list_node *node = NULL;
    struct hdcdrv_service *service = NULL;
    int event_num = num;

    if (!ka_list_empty_careful(&epfd->service_list)) {
        ka_list_for_each_safe(pos, n, &epfd->service_list)
        {
            node = ka_list_entry(pos, struct hdcdrv_epoll_list_node, list);
            service = (struct hdcdrv_service *)node->instance;
            if (service->conn_list_head != NULL) {
                epfd->events[event_num].events = HDCDRV_EPOLL_CONN_IN;
                epfd->events[event_num].data = node->events.data;
                epfd->events[event_num].sub_data = node->events.sub_data;
                event_num++;
            }
        }
    }

    return event_num;
}

STATIC int hdcdrv_epoll_session_event_num(struct hdcdrv_epoll_fd *epfd, int num)
{
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_epoll_list_node *node = NULL;
    struct hdcdrv_session *session = NULL;
    int data_in_flag = 0;
    int event_num = num;

    if (!ka_list_empty_careful(&epfd->session_list)) {
        ka_list_for_each_safe(pos, n, &epfd->session_list)
        {
            node = ka_list_entry(pos, struct hdcdrv_epoll_list_node, list);

            data_in_flag = 0;
            session = (struct hdcdrv_session *)node->instance;

            epfd->events[event_num].events = 0;

            if ((hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
                (epfd->pid != session->owner_pid)) {
                epfd->events[event_num].events = HDCDRV_EPOLL_SESSION_CLOSE;
                epfd->events[event_num].data = node->events.data;
                epfd->events[event_num].sub_data = node->events.sub_data;
                event_num++;
                continue;
            }

            if ((node->events.events & HDCDRV_EPOLL_DATA_IN) && (session->normal_rx.head != session->normal_rx.tail) &&
                (epfd->pid == session->owner_pid)) {
                epfd->events[event_num].events |= HDCDRV_EPOLL_DATA_IN;
                epfd->events[event_num].data = node->events.data;
                epfd->events[event_num].sub_data = node->events.sub_data;
                data_in_flag = 1;
            }

            if ((node->events.events & HDCDRV_EPOLL_FAST_DATA_IN) && (session->fast_rx.head != session->fast_rx.tail) &&
                (epfd->pid == session->owner_pid)) {
                epfd->events[event_num].events |= HDCDRV_EPOLL_FAST_DATA_IN;
                epfd->events[event_num].data = node->events.data;
                epfd->events[event_num].sub_data = node->events.sub_data;
                data_in_flag = 1;
            }

            if (data_in_flag == 1) {
                event_num++;
            }
        }
    }

    return event_num;
}

STATIC int hdcdrv_epoll_event_num(struct hdcdrv_epoll_fd *epfd)
{
    int event_num = 0;

    event_num = hdcdrv_epoll_service_event_num(epfd, event_num);
    event_num = hdcdrv_epoll_session_event_num(epfd, event_num);

    return event_num;
}

STATIC int hdcdrv_copy_event_to_user(const struct hdcdrv_epoll_fd *epfd, int event_num,
    struct hdcdrv_cmd_epoll_wait *cmd, int mode)
{
    u64 copy_size;

    if (event_num > cmd->maxevents) {
        cmd->ready_event = cmd->maxevents;
    } else {
        cmd->ready_event = event_num;
    }

    /* when event num = 0, no need to copy data to cmd->event */
    if (event_num == 0) {
        return HDCDRV_OK;
    }

    copy_size = (u64)sizeof(struct hdcdrv_event) * (unsigned int)cmd->ready_event;
    if (copy_size > ((u64)sizeof(struct hdcdrv_event) * (unsigned int)epfd->size)) {
        hdcdrv_err("ka_base_copy_to_user is invalid. (ready_event=%d; size=%d; mode=%d)\n", cmd->ready_event, epfd->size, mode);
        return HDCDRV_PARA_ERR;
    }

#ifdef CFG_FEATURE_VFIO
    if (epfd->vm_id != HDCDRV_DEFAULT_VM_ID) {
        if (memcpy_s(cmd->vevent, HDCDRV_VEPOLL_EVENT_MAX * sizeof(struct hdcdrv_event), epfd->events,
            copy_size) != EOK) {
            hdcdrv_err("Calling memcpy_s failed. (epfd=%d)\n", epfd->fd);
            return HDCDRV_ERR;
        }
        return HDCDRV_CMD_CONTINUE;
    }
#endif

    if (mode == HDCDRV_MODE_USER) {
        if ((cmd->event != NULL) && ka_base_copy_to_user((void __ka_user *)cmd->event, epfd->events, (unsigned long)copy_size)) {
            hdcdrv_err("Calling ka_base_copy_to_user failed. (epfd=%d)\n", epfd->fd);
            return HDCDRV_COPY_FROM_USER_FAIL;
        }
    } else {
        if (memcpy_s(cmd->event, (unsigned long)cmd->maxevents * sizeof(struct hdcdrv_event),
            epfd->events, copy_size) != EOK) {
            hdcdrv_err("Calling memcpy_s failed. (epfd=%d)\n", epfd->fd);
            return HDCDRV_ERR;
        }
    }

    return HDCDRV_OK;
}

STATIC u32 g_epoll_wait_print_cnt = 0;
STATIC u64 g_epoll_wait_jiffies = 0;
long hdcdrv_epoll_wait(struct hdcdrv_cmd_epoll_wait *cmd, int mode)
{
    struct hdcdrv_epoll_fd *epfd = NULL;
    int fd = cmd->epfd;
    long ret;
    int event_num;
    u64 timeout;
    u32 docker_id;

    docker_id = hdcdrv_get_container_id();

    ret = hdcdrv_epoll_fd_check(fd, docker_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        hdcdrv_err_limit("Calling hdcdrv_epoll_fd_check failed.\n");
        return ret;
    }

    if (cmd->event == NULL) {
        hdcdrv_err("Input pararmeter is error.\n");
        return HDCDRV_PARA_ERR;
    }

    if (cmd->timeout < 0) {
        hdcdrv_err("timeout is invalid. (timeout=%d)\n", cmd->timeout);
        return HDCDRV_PARA_ERR;
    }

    if (cmd->maxevents < 0) {
        hdcdrv_err("maxevents is invalid. (maxevents=%d)\n", cmd->maxevents);
        return HDCDRV_PARA_ERR;
    }

    cmd->ready_event = 0;
    epfd = &hdc_epoll->epoll_docks[docker_id].epfds[fd];

    ka_task_mutex_lock(&epfd->mutex);
    event_num = hdcdrv_epoll_event_num(epfd);
    if (event_num == 0) {
        timeout = ka_system_msecs_to_jiffies((unsigned int)cmd->timeout);
retry:
        ka_task_mutex_unlock(&epfd->mutex);
        ret = ka_task_wait_event_interruptible_timeout(epfd->wq, ((epfd->wait_flag != 0) || (epfd->valid != HDCDRV_VALID)),
            (long)timeout);
        if (ret < 0) {
#ifndef DRV_UT
            HDC_LOG_WARN_LIMIT(&g_epoll_wait_print_cnt, &g_epoll_wait_jiffies,
                "Calling ka_task_wait_event_interruptible_timeout.(epfd %d; ret=%ld)\n", fd, ret);
#endif
            return ret;
        }

        ka_task_mutex_lock(&epfd->mutex);
        /* the epoll is closed */
        if (epfd->valid != HDCDRV_VALID) {
            ka_task_mutex_unlock(&epfd->mutex);
            return HDCDRV_EPOLL_CLOSE;
        }

        epfd->wait_flag = 0;
        ka_wmb();
        event_num = hdcdrv_epoll_event_num(epfd);
        if ((event_num == 0) && (ret > 0)) {
            /* wait_flag is set to 1 by another thread. As a result,
            the number of events obtained in the next wait is 0 */
            goto retry;
        }
    }

    ret = hdcdrv_copy_event_to_user(epfd, event_num, cmd, mode);

    ka_task_mutex_unlock(&epfd->mutex);
    return ret;
}

int *hdcdrv_epoll_get_dev_id_ptr(union hdcdrv_cmd *cmd_data)
{
    if ((cmd_data->epoll_ctl.event.events & HDCDRV_EPOLL_CONN_IN) != 0) {
        return &cmd_data->epoll_ctl.para1;
    } else {
        return NULL;
    }
}
long hdcdrv_epoll_operation(struct hdcdrv_ctx *ctx, u32 drv_cmd, union hdcdrv_cmd *cmd_data,
    bool *copy_to_user_flag, u32 fid)
{
    long ret = HDCDRV_OK;

    switch (drv_cmd) {
#ifndef CFG_FEATURE_HDC_REG_MEM
        case HDCDRV_CMD_EPOLL_ALLOC_FD:
            ret = hdcdrv_epoll_alloc_fd(ctx, &cmd_data->epoll_alloc_fd);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_EPOLL_FREE_FD:
            ret = hdcdrv_epoll_free_fd(ctx, &cmd_data->epoll_free_fd);
            break;
        case HDCDRV_CMD_EPOLL_CTL:
            ret = hdcdrv_epoll_ctl(&cmd_data->epoll_ctl, fid);
            break;
        case HDCDRV_CMD_EPOLL_WAIT:
            ret = hdcdrv_epoll_wait(&cmd_data->epoll_wait, HDCDRV_MODE_USER);
            *copy_to_user_flag = true;
            break;
#endif
        default:
            hdcdrv_err_limit("Parameter cmd is illegal. (drv_cmd=%d)\n", drv_cmd);
            ret = HDCDRV_PARA_ERR;
            break;
    }

    return ret;
}

long hdcdrv_kernel_epoll_alloc_fd(int size, int *epfd, const int *magic_num)
{
    struct hdcdrv_cmd_epoll_alloc_fd cmd;
    long ret;

    (void)magic_num;
    if (epfd == NULL) {
        return HDCDRV_ERR;
    }

    cmd.size = size;
    cmd.pid = hdcdrv_get_pid();

    ret = hdcdrv_epoll_alloc_fd(NULL, &cmd);
    if (ret == HDCDRV_OK) {
        *epfd = cmd.epfd;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_epoll_alloc_fd);

long hdcdrv_kernel_epoll_free_fd(int epfd, int magic_num)
{
    struct hdcdrv_cmd_epoll_free_fd cmd;

    (void)magic_num;
    cmd.epfd = epfd;
    cmd.pid = hdcdrv_get_pid();

    return hdcdrv_epoll_free_fd(NULL, &cmd);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_epoll_free_fd);

/*
para1; input, service:dev_id, session:session_fd
para2; input, service:service_type, session:magic_num
*/
long hdcdrv_kernel_epoll_ctl(int epfd, int magic_num, int op,
    unsigned int event, int para1, const char *para2, unsigned int para2_len)
{
    struct hdcdrv_cmd_epoll_ctl cmd;

    /* no place call this interface, first give a param judgement */
    if (para2 == NULL || para2_len != HDCDRV_EPOLL_CTL_PARA_NUM) {
        hdcdrv_err("Input pararmeter is error.\n");
        return HDCDRV_PARA_ERR;
    }

    (void)magic_num;
    cmd.epfd = epfd;
    cmd.pid = hdcdrv_get_pid();
    cmd.op = op;
    cmd.para1 = para1;
    cmd.para2 = *(int*)para2;
    cmd.event.events = event;
    /*lint -e571 */
    cmd.event.data = (u64)para1;
    /*lint +e571 */
    cmd.event.sub_data = *(int *)para2;

    return hdcdrv_epoll_ctl(&cmd, HDCDRV_DEFAULT_PM_FID);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_epoll_ctl);

long hdcdrv_kernel_epoll_wait(int epfd, int magic_num, int timeout, int *event_num,
    unsigned int event[], unsigned int event_len, int para1[],
    unsigned int para1_len, int para2[], unsigned int para2_len)
{
    struct hdcdrv_cmd_epoll_wait cmd;
    long ret;
    int i;
    struct hdcdrv_event *events = NULL;

    (void)magic_num;
    if ((event_num == NULL) || (event == NULL) || (para1 == NULL) || (para2 == NULL)) {
        return HDCDRV_PARA_ERR;
    }

    if (*event_num <= 0) {
        return HDCDRV_PARA_ERR;
    }

    cmd.epfd = epfd;
    cmd.pid = hdcdrv_get_pid();
    cmd.timeout = timeout;
    cmd.maxevents = *event_num;
    cmd.event = hdcdrv_kzalloc((u64)(sizeof(struct hdcdrv_event) * (u64)(unsigned int)cmd.maxevents),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_SUB_MODULE_TYPE_3);
    if (cmd.event == NULL) {
        hdcdrv_err("Calling malloc failed.\n");
        return HDCDRV_ERR;
    }

    ret = hdcdrv_epoll_wait(&cmd, HDCDRV_MODE_KERNEL);
    if (ret == HDCDRV_OK) {
        *event_num = cmd.ready_event;
        events = (struct hdcdrv_event *)cmd.event;
        for (i = 0; i < cmd.ready_event; i++) {
            if ((unsigned int)i >= event_len || (unsigned int)i >= para1_len ||
                (unsigned int)i >= para2_len) {
                hdcdrv_kfree(cmd.event, KA_SUB_MODULE_TYPE_3);
                cmd.event = NULL;
                hdcdrv_err("ready_event length is invalid.\n");
                return HDCDRV_ERR;
            }

            event[i] = events[i].events;
            para1[i] = (int)(events[i].data);
            para2[i] = events[i].sub_data;
        }
    }

    hdcdrv_kfree(cmd.event, KA_SUB_MODULE_TYPE_3);
    cmd.event = NULL;

    return ret;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_epoll_wait);

int hdcdrv_epoll_init(struct hdcdrv_epoll *epolls)
{
    int i = 0;
    int j = 0;
    hdc_epoll = epolls;

    for (i = 0; i < (int)HDCDRV_DOCKER_MAX_NUM; i++) {
        hdc_epoll->epoll_docks[i].epfds =
            (struct hdcdrv_epoll_fd *)hdcdrv_vzalloc(sizeof(struct hdcdrv_epoll_fd) * HDCDRV_EPOLL_FD_NUM,
                KA_SUB_MODULE_TYPE_0);
        if (hdc_epoll->epoll_docks[i].epfds == NULL) {
            hdcdrv_err("Calling alloc epfds failed. (dock num=%d)\n", i);
            goto EPOLL_FD_ALLOC_FAIL;
        }
    }

    hdc_epoll->vm_alloc_cnt = (int *)hdcdrv_vzalloc(sizeof(int) * (HDCDRV_MAX_VM_NUM + 1U), KA_SUB_MODULE_TYPE_0);
    if (hdc_epoll->vm_alloc_cnt == NULL) {
        hdcdrv_err("Alloc vm_alloc_cnt fail\n");
        goto EPOLL_FD_ALLOC_FAIL;
    }

    ka_task_mutex_init(&hdc_epoll->mutex);

    for (i = 0; i < (int)HDCDRV_DOCKER_MAX_NUM; i++) {
        for (j = 0; j < (int)HDCDRV_EPOLL_FD_NUM; j++) {
            hdc_epoll->epoll_docks[i].epfds[j].fd = j;
            hdc_epoll->epoll_docks[i].epfds[j].docker_id = i;
            ka_task_init_waitqueue_head(&hdc_epoll->epoll_docks[i].epfds[j].wq);
            ka_task_mutex_init(&hdc_epoll->epoll_docks[i].epfds[j].mutex);
            KA_INIT_LIST_HEAD(&hdc_epoll->epoll_docks[i].epfds[j].service_list);
            KA_INIT_LIST_HEAD(&hdc_epoll->epoll_docks[i].epfds[j].session_list);
            hdc_epoll->epoll_docks[i].epfds[j].valid = HDCDRV_INVALID;
        }
    }

    return 0;

EPOLL_FD_ALLOC_FAIL:
    for (j = 0; j < i; j++) {
        hdcdrv_vfree(epolls->epoll_docks[j].epfds, KA_SUB_MODULE_TYPE_0);
        epolls->epoll_docks[j].epfds = NULL;
    }
    return HDCDRV_MEM_ALLOC_FAIL;
}

void hdcdrv_epoll_uninit(struct hdcdrv_epoll *epolls)
{
    int i;

    for (i = 0; i < (int)HDCDRV_DOCKER_MAX_NUM; i++) {
        hdcdrv_vfree(epolls->epoll_docks[i].epfds, KA_SUB_MODULE_TYPE_0);
        epolls->epoll_docks[i].epfds = NULL;
    }

    hdcdrv_vfree(epolls->vm_alloc_cnt, KA_SUB_MODULE_TYPE_0);
    epolls->vm_alloc_cnt = NULL;
}
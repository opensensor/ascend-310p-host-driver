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

#include "dbl/uda.h"
#include "soc_res.h"

#include "kernel_version_adapt.h"
#include "virtmng_interface.h"
#include "hdcdrv_core.h"
#include "hdcdrv_mem_com.h"
#include "hdcdrv_adapter.h"

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_compiler_pub.h"

STATIC int vhdch_dev_id_check(u32 dev_id, u32 fid)
{
    if ((dev_id >= VMNG_PDEV_MAX) || (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        hdcdrv_err_spinlock("Input pararmeters error. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC u64 vhdch_rebuild_pid(u32 vm_id, u64 pid)
{
    u64 new_pid;

    new_pid = ((vm_id & HDCDRV_VMID_MASK) << HDCDRV_VMID_OFFSET) | (pid & HDCDRV_RAW_PID_MASK);

    return new_pid;
}

STATIC void vhdch_rb_mem_erase(struct vhdch_vdev *vdev, struct vhdch_fast_node *fast_node)
{
    ka_task_spin_lock_bh(&vdev->mem_lock);
    ka_base_rb_erase(&fast_node->mem_node, &vdev->rb_mem);
    ka_task_spin_unlock_bh(&vdev->mem_lock);
}

STATIC int vhdch_rb_mem_insert(struct vhdch_vdev *vdev, struct vhdch_fast_node *fast_node)
{
    ka_rb_node_t *parent = NULL;
    ka_rb_node_t **link = ka_base_get_rb_root_node_addr(&vdev->rb_mem);

    ka_task_spin_lock_bh(&vdev->mem_lock);
    while (*link != NULL) {
        struct vhdch_fast_node *this = ka_base_rb_entry(*link, struct vhdch_fast_node, mem_node);
        parent = *link;
        if (fast_node->hash_va < this->hash_va) {
            link = ka_base_get_rb_node_left_addr(*link);
        } else if (fast_node->hash_va > this->hash_va) {
            link = ka_base_get_rb_node_right_addr(*link);
        } else {
            ka_task_spin_unlock_bh(&vdev->mem_lock);
            return HDCDRV_F_NODE_SEARCH_FAIL;
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(&fast_node->mem_node, parent, link);
    ka_base_rb_insert_color(&fast_node->mem_node, &vdev->rb_mem);

    ka_task_spin_unlock_bh(&vdev->mem_lock);
    return HDCDRV_OK;
}

STATIC struct vhdch_fast_node *vhdch_rb_mem_search(struct vhdch_vdev *vdev, u64 hash)
{
    ka_rb_node_t *node = ka_base_get_rb_root_node(&vdev->rb_mem);
    struct vhdch_fast_node *fast_node = NULL;

    ka_task_spin_lock_bh(&vdev->mem_lock);
    while (node != NULL) {
        fast_node = ka_base_rb_entry(node, struct vhdch_fast_node, mem_node);
        if (hash < fast_node->hash_va) {
            node = ka_base_get_rb_node_left(node);
        } else if (hash > fast_node->hash_va) {
            node = ka_base_get_rb_node_right(node);
        } else {
            ka_task_spin_unlock_bh(&vdev->mem_lock);
            return fast_node;
        }
    }
    ka_task_spin_unlock_bh(&vdev->mem_lock);

    return NULL;
}

STATIC int vhdch_update_mem_tree(u32 dev_id, u32 fid, int flag, struct hdcdrv_ctrl_msg_sync_mem_info *mem_info)
{
    struct vhdch_fast_node *fast_node = NULL;
    struct vhdch_vdev *vdev = NULL;
    int ret = HDCDRV_OK;

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    ka_task_mutex_lock(&vdev->mutex);

    if ((flag == HDCDRV_ADD_FLAG) && (vdev->fast_node_num_avaliable > 0) &&
        (vdev->fnode_phy_num_avaliable - mem_info->phy_addr_num >= 0)) {
        fast_node = hdcdrv_kvmalloc(sizeof(struct vhdch_fast_node), KA_SUB_MODULE_TYPE_2);
        if (fast_node == NULL) {
            ka_task_mutex_unlock(&vdev->mutex);
            return HDCDRV_MEM_ALLOC_FAIL;
        }

        fast_node->hash_va = mem_info->hash_va;
        ret = vhdch_rb_mem_insert(vdev, fast_node);
        if (ret != HDCDRV_OK) {
            hdcdrv_kvfree(fast_node, KA_SUB_MODULE_TYPE_2);
            hdcdrv_warn("hash_va insert failed. (dev_id=%u; fid=%u; hash_va=%llu)\n", dev_id, fid, mem_info->hash_va);
            ka_task_mutex_unlock(&vdev->mutex);
            return ret;
        }

        vdev->fast_node_num_avaliable--;
        vdev->fnode_phy_num_avaliable -= mem_info->phy_addr_num;
    } else if (flag == HDCDRV_DEL_FLAG) {
        fast_node = vhdch_rb_mem_search(vdev, mem_info->hash_va);
        if (fast_node != NULL) {
            vhdch_rb_mem_erase(vdev, fast_node);
            hdcdrv_kvfree(fast_node, KA_SUB_MODULE_TYPE_2);
            vdev->fast_node_num_avaliable++;
            vdev->fnode_phy_num_avaliable += mem_info->phy_addr_num;
        } else {
            hdcdrv_warn("hash_va search failed. (dev_id=%u; fid=%u; hash_va=%llu)\n", dev_id, fid, mem_info->hash_va);
            ret = HDCDRV_F_NODE_SEARCH_FAIL;
        }
    } else {
        ret = HDCDRV_PARA_ERR;
        hdcdrv_warn("flag is invalid. (dev_id=%u; fid=%u; flag=%d; node_num=%d; phy_num=%d)\n",
            dev_id, fid, flag, vdev->fast_node_num_avaliable, vdev->fnode_phy_num_avaliable);
    }

    ka_task_mutex_unlock(&vdev->mutex);
    return ret;
}

STATIC int vhdch_set_session_owner_pre_handle(u32 dev_id, u32 fid, union hdcdrv_cmd *cmd)
{
    struct hdcdrv_cmd_set_session_owner *owner_cmd;
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];

    owner_cmd = &cmd->set_owner;
    owner_cmd->ppid = vhdch_rebuild_pid(vdev->vm_id, owner_cmd->ppid);
    return HDCDRV_OK;
}

STATIC int vhdch_epoll_ctrl_pre_handle(u32 dev_id, u32 fid, union hdcdrv_cmd *cmd)
{
    u32 pm_devid;
    u32 pm_fid;
    struct hdcdrv_event *event;
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];

    event = &cmd->epoll_ctl.event;

    if (event->events == HDCDRV_EPOLL_CONN_IN) {
        if (vmngh_ctrl_get_devid_fid(vdev->vm_id - 1, (u32)cmd->epoll_ctl.para1, &pm_devid, &pm_fid) != 0) {
            hdcdrv_err("Failed to get pm_devid pm_fid. (vm_id=%u; vm_devid=%d)\n", vdev->vm_id, cmd->epoll_ctl.para1);
            return HDCDRV_PARA_ERR;
        }

        cmd->epoll_ctl.para1 = (int)pm_devid;
    }

    return HDCDRV_OK;
}

STATIC int vhdch_cmd_default_pre_handle(u32 dev_id, u32 fid, union hdcdrv_cmd *cmd)
{
    (void)cmd;
    hdcdrv_warn("Command not support. (dev_id=%u; fid=%u)\n", dev_id, fid);
    return HDCDRV_NOT_SUPPORT;
}

int (*vhdch_vm_cmd_pre_handle[HDCDRV_CMD_MAX])(u32 dev_id, u32 fid, union hdcdrv_cmd *cmd) = {
    [HDCDRV_CMD_CONFIG] = vhdch_cmd_default_pre_handle,
    [HDCDRV_CMD_GET_PAGE_SIZE] = vhdch_cmd_default_pre_handle,
    [HDCDRV_CMD_SET_SESSION_OWNER] = vhdch_set_session_owner_pre_handle,
    [HDCDRV_CMD_EPOLL_CTL] = vhdch_epoll_ctrl_pre_handle,
    [HDCDRV_CMD_ALLOC_MEM] = vhdch_cmd_default_pre_handle,
    [HDCDRV_CMD_FREE_MEM] = vhdch_cmd_default_pre_handle,
    [HDCDRV_CMD_DMA_MAP] = vhdch_cmd_default_pre_handle,
    [HDCDRV_CMD_DMA_UNMAP] = vhdch_cmd_default_pre_handle,
    [HDCDRV_CMD_DMA_REMAP] = vhdch_cmd_default_pre_handle,
};

STATIC int vhdch_vm_cmd_pre_proc(u32 dev_id, u32 fid, u32 cmd, union hdcdrv_cmd *cmd_data)
{
    int ret = HDCDRV_OK;
    u32 drv_cmd = _KA_IOC_NR(cmd);
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];

    if (drv_cmd >= HDCDRV_CMD_MAX) {
        hdcdrv_err_limit("Command is illegal. (dev_id=%u; fid=%u; cmd=%u)\n", dev_id, fid, drv_cmd);
        return HDCDRV_PARA_ERR;
    }

    if (vdev->valid == HDCDRV_INVALID) {
        hdcdrv_warn("vhdch is invalid. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return HDCDRV_DEVICE_NOT_READY;
    }

    if (vhdch_vm_cmd_pre_handle[drv_cmd] != NULL) {
        ret = vhdch_vm_cmd_pre_handle[drv_cmd](dev_id, fid, cmd_data);
    }

    cmd_data->cmd_com.dev_id = (int)dev_id;
    /* rebuild pid for vm */
    cmd_data->cmd_com.pid = vhdch_rebuild_pid(vdev->vm_id, cmd_data->cmd_com.pid);

    return ret;
}

STATIC void vhdch_set_vdev_status(u32 dev_id, u32 fid, u32 valid)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];

    vdev->valid = valid;
    hdcdrv_info("Set vdev status finished. (dev_id=%u; fid=%u; status=%d)\n", dev_id, fid, valid);
}

STATIC int vhdch_check_vdev_ready(u32 dev_id, u32 fid)
{
    struct vhdch_vdev *vdev = NULL;
    int ret;

    ret = vhdch_dev_id_check(dev_id, fid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    if (vdev->valid == HDCDRV_VALID) {
        return HDCDRV_OK;
    }

    hdcdrv_warn_spinlock("Device not ready. (dev_id=%u; fid=%u)\n", dev_id, fid);

    return HDCDRV_DEVICE_NOT_READY;
}
int vhdch_alloc_mem_vm(struct hdccom_alloc_mem_para *para, struct hdcdrv_buf_desc *desc)
{
    struct vmng_tx_msg_proc_info tx_info;
    struct vhdc_ctrl_msg msg;
    int ret;

    /* notify PM to free resource */
    msg.type = VHDC_CTRL_MSG_TYPE_ALLOC_MEM;
    msg.error_code = HDCDRV_ERR;
    msg.alloc_mempool.buf = NULL;
    msg.alloc_mempool.addr = 0;
    msg.alloc_mempool.mem_para.dev_id = para->dev_id;
    msg.alloc_mempool.mem_para.pool_type = para->pool_type;
    msg.alloc_mempool.mem_para.len = para->len;
    msg.alloc_mempool.mem_para.fid = para->fid;

    tx_info.data = &msg;
    tx_info.in_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.out_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.real_out_len = 0;

    ret = vmngh_common_msg_send((u32)para->dev_id, para->fid,  VMNG_MSG_COMMON_TYPE_HDC, &tx_info);
    if ((ret != 0) || (msg.error_code != HDCDRV_OK)) {
        hdcdrv_err("Calling vmngh_common_msg_send failed. (dev_id_%u; fid=%u; ret=%d; error_code=%d)\n",
            para->dev_id, para->fid, ret, msg.error_code);
        return HDCDRV_ERR;
    }

    ret = hdcdrv_dma_map_guest_page((u32)para->dev_id, para->fid,
                                    msg.alloc_mempool.addr, (unsigned long)para->len, desc);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_dma_map_guest_page failed. (ret=%d)\n", ret);
        return ret;
    }
    desc->buf = msg.alloc_mempool.buf;

    return ret;
}

void vhdch_free_mem_vm(u32 dev_id, u32 fid, void *buf)
{
    struct vmng_tx_msg_proc_info tx_info;
    struct vhdc_ctrl_msg msg;
    int ret;

    /* notify VM to free resource */
    msg.type = VHDC_CTRL_MSG_TYPE_FREE_MEM;
    msg.error_code = HDCDRV_ERR;
    msg.free_mempool.buf = buf;

    tx_info.data = &msg;
    tx_info.in_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.out_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.real_out_len = 0;

    ret = vmngh_common_msg_send(dev_id, fid,  VMNG_MSG_COMMON_TYPE_HDC, &tx_info);
    if ((ret != 0) || (msg.error_code != HDCDRV_OK)) {
        hdcdrv_err("Calling vmngh_common_msg_send failed. (dev_id=%u; fid=%u; ret=%d; error_code=%d)\n",
            dev_id, fid, ret, msg.error_code);
        return;
    }

    return;
}

STATIC int vhdch_get_mem_pool_type(int pm_mem_pool_type, u32 len)
{
    if (pm_mem_pool_type == HDCDRV_MEM_POOL_TYPE_RX) {
        if (len <= (HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE)) {
            return HDCDRV_VDEV_MEM_POOL_TYPE_RX_SMALL;
        } else {
            return HDCDRV_VDEV_MEM_POOL_TYPE_RX_HUGE;
        }
    }

    if (len <= (HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE)) {
        return HDCDRV_VDEV_MEM_POOL_TYPE_TX_SMALL;
    } else {
        return HDCDRV_VDEV_MEM_POOL_TYPE_TX_HUGE;
    }
}

int vhdch_alloc_mem_container(struct hdccom_alloc_mem_para *para, struct hdcdrv_buf_desc *desc)
{
    struct vhdch_vdev *vdev = NULL;
    int pool_type;
    int ret;

    ret = vhdch_check_vdev_ready((u32)para->dev_id, para->fid);
    if (ret != HDCDRV_OK) {
        return ret;
    }
    vdev = &hdc_ctrl->vdev[para->dev_id][para->fid];

    pool_type = vhdch_get_mem_pool_type(para->pool_type, (u32)para->len);
    ka_task_spin_lock_bh(&vdev->mem_lock);
    if (vdev->mem_cnt[pool_type] == 0) {
        if (pool_type < HDCDRV_VDEV_RX_MEM_POOL_TYPE_MAX) {
            vdev->rx_wait_sche[pool_type] = 1;
        }
        ka_task_spin_unlock_bh(&vdev->mem_lock);
        return HDCDRV_DMA_MEM_ALLOC_FAIL;
    }

    vdev->mem_cnt[pool_type]--;
    ka_task_spin_unlock_bh(&vdev->mem_lock);

    ret = alloc_mem(para->pool_type, para->dev_id, para->len, &desc->buf, &desc->addr, para->wait_head);
    if (ret != HDCDRV_OK) {
        ka_task_spin_lock_bh(&vdev->mem_lock);
        vdev->mem_cnt[pool_type]++;
        ka_task_spin_unlock_bh(&vdev->mem_lock);
    }

    return ret;
}

void vhdch_free_mem_container(u32 dev_id, u32 fid, u32 chan_id, void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    int pool_type;
    struct vhdch_vdev *vdev = NULL;
    struct hdcdrv_msg_chan *msg_chan = NULL;
    bool rx_work_sche_flag = false;
    int ret;

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err_spinlock("Calling hdcdrv_mem_block_head_check failed.\n");
        return;
    }

    block_head = (struct hdcdrv_mem_block_head *)buf;
    pool_type = vhdch_get_mem_pool_type((int)block_head->type, block_head->size);

    free_mem(buf);

    ret = vhdch_check_vdev_ready(dev_id, fid);
    if (ret != HDCDRV_OK) {
        return;
    }

    vdev = &hdc_ctrl->vdev[dev_id][fid];

    ka_task_spin_lock_bh(&vdev->mem_lock);
    vdev->mem_cnt[pool_type]++;
    if ((pool_type < HDCDRV_VDEV_RX_MEM_POOL_TYPE_MAX) && (vdev->rx_wait_sche[pool_type] == 1)) {
        vdev->rx_wait_sche[pool_type] = 0;
        rx_work_sche_flag = 1;
    }
    ka_task_spin_unlock_bh(&vdev->mem_lock);

    msg_chan = hdc_ctrl->devices[dev_id].msg_chan[chan_id];
    if (rx_work_sche_flag) {
        ka_task_queue_work(msg_chan->rx_workqueue, &msg_chan->rx_notify_work);
    }
}

STATIC void vhdch_service_res_uninit(struct hdcdrv_service *service, int server_type)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;

    if (!ka_list_empty_careful(&service->serv_list)) {
        ka_list_for_each_safe(pos, n, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            ka_list_del(&node->list);
            hdcdrv_kvfree(node, KA_SUB_MODULE_TYPE_0);
            node = NULL;
        }
    }
    return;
}

STATIC int vhdch_service_res_init(struct hdcdrv_service *service, int server_type)
{
    struct hdcdrv_serv_list_node *node = NULL;
    int i = 0;

    if (ka_list_empty_careful(&service->serv_list) != 0) {
        for (i = 0; i < HDCDRV_SERVER_PROCESS_MAX_NUM; i++) {
            node = (struct hdcdrv_serv_list_node *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_serv_list_node), KA_SUB_MODULE_TYPE_0);
            if (ka_unlikely(node == NULL)) {
                hdcdrv_err("Calling alloc failed. (i=%d; server_type=%d)\n", i, server_type);
                return HDCDRV_ERR;
            }

            hdcdrv_service_init(&node->service);
            ka_list_add(&node->list, &service->serv_list);
        }
    }
    return HDCDRV_OK;
}

STATIC int vhdch_init_service(u32 dev_id, u32 fid)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];
    int i;

    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        hdcdrv_service_init(&vdev->service[i]);
        vdev->service_attr[i].level = hdcdrv_service_level_init(i);
        vdev->service_attr[i].conn_feature = hdcdrv_service_conn_feature_init(i);
        vdev->service_attr[i].service_scope = hdcdrv_service_scope_init(i);
    }

    ka_task_mutex_lock(&vdev->mutex);
    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        if (vdev->service_attr[i].service_scope == HDCDRV_SERVICE_SCOPE_PROCESS) {
            if (vhdch_service_res_init(&vdev->service[i], i) != 0) {
                hdcdrv_err("Resource init failed. (dev_id=%u; fid=%u; server=%d)\n", dev_id, fid, i);
                goto out;
            }
        }
    }
    ka_task_mutex_unlock(&vdev->mutex);

    return HDCDRV_OK;

out:
    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        if (vdev->service_attr[i].service_scope == HDCDRV_SERVICE_SCOPE_PROCESS) {
            vhdch_service_res_uninit(&vdev->service[i], i);
        }
    }
    ka_task_mutex_unlock(&vdev->mutex);
    return HDCDRV_ERR;
}

STATIC void vhdch_uninit_service(u32 dev_id, u32 fid)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];
    int i;

    ka_task_mutex_lock(&vdev->mutex);

    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        if (vdev->service_attr[i].service_scope == HDCDRV_SERVICE_SCOPE_PROCESS) {
            vhdch_service_res_uninit(&vdev->service[i], i);
        }
    }

    ka_task_mutex_unlock(&vdev->mutex);
}

struct hdcdrv_service *vhdch_search_service(u32 devid, u32 fid, int service_type, u64 host_pid)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_service *service = NULL;
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[devid][fid];

    service = &vdev->service[service_type];

    if (vdev->service_attr[service_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return service;
    }

    ka_task_mutex_lock(&vdev->mutex);
    if (!ka_list_empty_careful(&service->serv_list)) {
        ka_list_for_each_safe(pos, n, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_pid == host_pid) {
                ka_task_mutex_unlock(&vdev->mutex);
                return &node->service;
            }
        }
    }
    ka_task_mutex_unlock(&vdev->mutex);
    return service;
}

struct hdcdrv_service *vhdch_alloc_service(u32 devid, u32 fid, int service_type, u64 host_pid)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_service *service = NULL;
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[devid][fid];

    service = &vdev->service[service_type];

    if (vdev->service_attr[service_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return service;
    }
    ka_task_mutex_lock(&vdev->mutex);

    /* Checking whether the process server with the Same hostpid exists */
    if (!ka_list_empty_careful(&service->serv_list)) {
        ka_list_for_each_safe(pos, n, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_pid == host_pid) {
                ka_task_mutex_unlock(&vdev->mutex);
                return &node->service;
            }
        }
    }

    /* Searching for an idle server */
    if (!ka_list_empty_careful(&service->serv_list)) {
        ka_list_for_each_safe(pos, n, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_pid == HDCDRV_INVALID) {
                node->service.listen_pid = host_pid;
                ka_task_mutex_unlock(&vdev->mutex);
                return &node->service;
            }
        }
    }
    ka_task_mutex_unlock(&vdev->mutex);

    return NULL;
}

STATIC void vhdch_reset_process_server(struct vhdch_vdev *vdev, int service_type)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_service *service = NULL;

    service = &vdev->service[service_type];

    if (vdev->service_attr[service_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return;
    }

    ka_task_mutex_lock(&vdev->mutex);
    if (!ka_list_empty_careful(&service->serv_list)) {
        ka_list_for_each_safe(pos, n, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_status == HDCDRV_VALID) {
                (void)hdcdrv_server_free(&node->service, (int)vdev->dev_id, service_type);
            }
        }
    }
    ka_task_mutex_unlock(&vdev->mutex);

    return;
}

STATIC void vhdch_reset_service(struct vhdch_vdev *vdev)
{
    struct hdcdrv_service *service = NULL;
    int i;
    long ret = 0;

    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        service = &vdev->service[i];
        if (service->listen_status == HDCDRV_VALID) {
            hdcdrv_info("Service wakeup accept. (dev_id=%u; fid=%u; service=%d)\n", vdev->dev_id, vdev->fid, i);
            ret = hdcdrv_server_free(service, (int)vdev->dev_id, i);
            if (ret != HDCDRV_OK) {
                hdcdrv_warn("Reset failed, service wakeup accept. (dev_id=%u; fid=%u; service=%d)\n",
                    vdev->dev_id, vdev->fid, i);
            }
        }

        vhdch_reset_process_server(vdev, i);
    }
}

STATIC void vhdch_uninit_msgchan_pool(u32 dev_id, u32 fid)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[dev_id];
    int i;

    ka_task_mutex_lock(&dev->mutex);
    for (i = 0; i < vdev->msg_chan_cnt; i++) {
        dev->msg_chan[vdev->msgchan_map[i]]->is_allocated = HDCDRV_MSG_CHAN_FLAG_NOT_ALLOCED;
        vdev->msgchan_map[i] = HDCDRV_INVALID_CHAN_ID;
        dev->alloced_chan_cnt--;
    }
    vdev->msg_chan_cnt = 0;
    ka_task_mutex_unlock(&dev->mutex);
}

STATIC int vhdch_alloc_chan_from_dev(struct vhdch_vdev *vdev, struct hdcdrv_dev *dev, int vdev_fast_cnt)
{
    u32 i;

    /* vdev alloc normal chan from dev */
    for (i = 0; i < dev->normal_chan_num; i++) {
        if (dev->msg_chan[i]->is_allocated == HDCDRV_MSG_CHAN_FLAG_NOT_ALLOCED) {
            dev->msg_chan[i]->is_allocated = HDCDRV_MSG_CHAN_FLAG_ALLOCED;
            vdev->msgchan_map[vdev->msg_chan_cnt] = i;
            dev->alloced_chan_cnt++;
            vdev->msg_chan_cnt++;
            if (vdev->msg_chan_cnt == HDCDRV_VDEV_NORMAL_CHAN_CNT) {
                break;
            }
        }
    }

    if (vdev->msg_chan_cnt < HDCDRV_VDEV_NORMAL_CHAN_CNT) {
        return HDCDRV_ERR;
    }

    /* vdev alloc fast chan from dev */
    for (i = dev->normal_chan_num; i < (u32)dev->msg_chan_cnt; i++) {
        if (dev->msg_chan[i]->is_allocated == HDCDRV_MSG_CHAN_FLAG_NOT_ALLOCED) {
            dev->msg_chan[i]->is_allocated = HDCDRV_MSG_CHAN_FLAG_ALLOCED;
            vdev->msgchan_map[vdev->msg_chan_cnt] = i;
            dev->alloced_chan_cnt++;
            vdev->msg_chan_cnt++;
            if (vdev->msg_chan_cnt == (HDCDRV_VDEV_NORMAL_CHAN_CNT + vdev_fast_cnt)) {
                break;
            }
        }
    }

    if (vdev->msg_chan_cnt < (HDCDRV_VDEV_NORMAL_CHAN_CNT + vdev_fast_cnt)) {
        return HDCDRV_ERR;
    }

    return HDCDRV_OK;
}

STATIC int vhdch_init_msgchan_pool(u32 dev_id, u32 fid, u32 alloc_core_num, u32 total_core_num)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[dev_id];
    int dev_fast_cnt, vdev_fast_cnt;

    hdcdrv_info("Get the parameter information. (dev_id=%u; fid=%u; total_core_num=%u; alloc_core_num=%u; "
        "msg_chan_cnt=%d; normal_chan_num=%u)\n", dev_id, fid, total_core_num, alloc_core_num,
        dev->msg_chan_cnt, dev->normal_chan_num);
    dev_fast_cnt = dev->msg_chan_cnt - (int)dev->normal_chan_num;
    if (dev_fast_cnt <= 0) {
        hdcdrv_err("No enough fast message channel resource. (dev_id=%u; fid=%u; msg_chan_cnt=%d; "
            "normal_chan_num=%u; dev_fast_cnt=%d)\n", dev_id, fid, dev->msg_chan_cnt,
            dev->normal_chan_num, dev_fast_cnt);
        return HDCDRV_ERR;
    }
    vdev_fast_cnt = dev_fast_cnt * (int)alloc_core_num / (int)total_core_num;

    ka_task_mutex_lock(&dev->mutex);
    if (vhdch_alloc_chan_from_dev(vdev, dev, vdev_fast_cnt) != HDCDRV_OK) {
        ka_task_mutex_unlock(&dev->mutex);
        vhdch_uninit_msgchan_pool(dev_id, fid);
        hdcdrv_err("No enough message channel resource. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return HDCDRV_ERR;
    }

    ka_task_mutex_unlock(&dev->mutex);
    return HDCDRV_OK;
}

STATIC void vhdch_init_mem_pool(u32 dev_id, u32 fid, u32 alloc_core_num, u32 total_core_num)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];

    ka_task_spin_lock_bh(&vdev->mem_lock);
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_RX_SMALL] =
        (int)(HDCDRV_SMALL_PACKET_NUM * alloc_core_num / total_core_num);
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_RX_HUGE] = (int)(HDCDRV_HUGE_PACKET_NUM * alloc_core_num / total_core_num);
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_TX_SMALL] =
        (int)(HDCDRV_SMALL_PACKET_NUM * alloc_core_num / total_core_num);
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_TX_HUGE] = (int)(HDCDRV_HUGE_PACKET_NUM * alloc_core_num / total_core_num);
    vdev->rx_wait_sche[HDCDRV_VDEV_MEM_POOL_TYPE_RX_SMALL] = 0;
    vdev->rx_wait_sche[HDCDRV_VDEV_MEM_POOL_TYPE_RX_HUGE] = 0;
    ka_task_spin_unlock_bh(&vdev->mem_lock);
}

STATIC void vhdch_uninit_mem_pool(u32 dev_id, u32 fid)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];

    ka_task_spin_lock_bh(&vdev->mem_lock);
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_RX_SMALL] = 0;
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_RX_HUGE] = 0;
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_TX_SMALL] = 0;
    vdev->mem_cnt[HDCDRV_VDEV_MEM_POOL_TYPE_TX_HUGE] = 0;
    vdev->rx_wait_sche[HDCDRV_VDEV_MEM_POOL_TYPE_RX_SMALL] = 0;
    vdev->rx_wait_sche[HDCDRV_VDEV_MEM_POOL_TYPE_RX_HUGE] = 0;
    ka_task_spin_unlock_bh(&vdev->mem_lock);
}

int vhdch_session_pre_alloc(u32 dev_id, u32 fid, int service_type)
{
    struct vhdch_vdev *vdev = NULL;
    int ret;
    int connect_type = hdcdrv_get_service_conn_feature(service_type);

    ret = vhdch_check_vdev_ready(dev_id, fid);
    if (ret != HDCDRV_OK) {
        return ret;
    }
    vdev = &hdc_ctrl->vdev[dev_id][fid];

    ka_task_mutex_lock(&vdev->mutex);

    if ((connect_type == HDCDRV_SERVICE_LONG_CONN) &&
        (vdev->cur_alloc_long_session < HDCDRV_SUPPORT_MAX_LONG_SESSION_PER_VDEV)) {
        vdev->cur_alloc_long_session++;
        ret = HDCDRV_OK;
    } else if ((connect_type == HDCDRV_SERVICE_SHORT_CONN) &&
        (vdev->cur_alloc_short_session < HDCDRV_SUPPORT_MAX_SHORT_SESSION_PER_VDEV)) {
        vdev->cur_alloc_short_session++;
        ret = HDCDRV_OK;
    } else {
        ret = HDCDRV_NO_SESSION;
    }

    ka_task_mutex_unlock(&vdev->mutex);

    return ret;
}

void vhdch_session_free(u32 dev_id, u32 fid, int service_type)
{
    struct vhdch_vdev *vdev = NULL;
    int ret;
    int connect_type = hdcdrv_get_service_conn_feature(service_type);

    ret = vhdch_check_vdev_ready(dev_id, fid);
    if (ret != HDCDRV_OK) {
        return;
    }

    vdev = &hdc_ctrl->vdev[dev_id][fid];

    ka_task_mutex_lock(&vdev->mutex);
    if (connect_type == HDCDRV_SERVICE_LONG_CONN) {
        vdev->cur_alloc_long_session--;
    }

    if (connect_type == HDCDRV_SERVICE_SHORT_CONN) {
        vdev->cur_alloc_short_session--;
    }

    ka_task_mutex_unlock(&vdev->mutex);
}


u32 vdhch_alloc_normal_msg_chan(u32 dev_id, u32 fid, int service_type)
{
    struct vhdch_vdev *vdev = NULL;
    u32 chan_id;

    if (vhdch_check_vdev_ready(dev_id, fid) != HDCDRV_OK) {
        return HDCDRV_INVALID_CHAN_ID;
    }

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    if ((service_type == HDCDRV_SERVICE_TYPE_PROF) || (service_type == HDCDRV_SERVICE_TYPE_LOG)) {
        chan_id = vdev->msgchan_map[0];
    } else {
        chan_id = vdev->msgchan_map[1];
    }

    return chan_id;
}

u32 vdhch_alloc_fast_msg_chan(u32 dev_id, u32 fid, int service_type)
{
    struct vhdch_vdev *vdev = NULL;
    struct hdcdrv_dev *dev = NULL;
    int i;
    int vdev_chan_id = HDCDRV_VDEV_FAST_MSG_CHAN_START;
    u32 chan_id;
    int session_cnt;

    if (vhdch_check_vdev_ready(dev_id, fid) != HDCDRV_OK) {
        return HDCDRV_INVALID_CHAN_ID;
    }

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    dev = &hdc_ctrl->devices[dev_id];

    ka_task_mutex_lock(&vdev->mutex);

    /* Find the msg chan with the least number of sessions */
    if (vdev->msgchan_map[vdev_chan_id] >= (u32)dev->msg_chan_cnt) {
        ka_task_mutex_unlock(&vdev->mutex);
        hdcdrv_err("Channel ID invalid. (devid=%u; fid=%u; service_type=%d; chan_id=%u)",
            dev_id, fid, service_type, vdev->msgchan_map[vdev_chan_id]);
        chan_id = HDCDRV_INVALID_CHAN_ID;
        return chan_id;
    }

    session_cnt = dev->msg_chan[vdev->msgchan_map[vdev_chan_id]]->session_cnt;
    for (i = HDCDRV_VDEV_FAST_MSG_CHAN_START; i < (int)vdev->msg_chan_cnt; i++) {
        if (session_cnt > dev->msg_chan[vdev->msgchan_map[i]]->session_cnt) {
            session_cnt = dev->msg_chan[vdev->msgchan_map[i]]->session_cnt;
            vdev_chan_id = i;
        }
    }
    chan_id = vdev->msgchan_map[vdev_chan_id];

    ka_task_mutex_unlock(&vdev->mutex);

    return chan_id;
}

int hdcdrv_dma_map_guest_page(u32 dev_id, u32 fid, unsigned long in_addr,
    unsigned long size, struct hdcdrv_buf_desc *desc)
{
    ka_sg_table_t *dma_sgt = NULL;
    unsigned long align_addr;
    unsigned long align_size;
    ka_dma_addr_t dma_addr;

    align_addr = HDCDRV_BLOCK_DMA_HEAD(in_addr);
    align_size = KA_MM_PAGE_ALIGN(size + HDCDRV_MEM_BLOCK_HEAD_SIZE);

    dma_addr = vmngh_dma_map_guest_page(dev_id, fid, align_addr, align_size, &dma_sgt);
    if (dma_sgt == NULL) {
        hdcdrv_err("Calling vmngh_dma_map_guest_page failed. dma_sgt is null.\n");
        return HDCDRV_ERR;
    }
    if ((dma_addr == DMA_MAP_ERROR) || (dma_sgt->nents > 1)) {
        hdcdrv_err("Calling vmngh_dma_map_guest_page failed. (dev_id=%u; fid=%u; nents=%d)\n",
            dev_id, fid, dma_sgt->nents);
        return HDCDRV_ERR;
    }

    desc->addr = HDCDRV_BLOCK_DMA_BUFFER(dma_addr);
    desc->dma_sgt = dma_sgt;
    desc->dev_id = dev_id;
    desc->fid = fid;

    return HDCDRV_OK;
}

void hdcdrv_dma_unmap_guest_page(u32 dev_id, u32 fid, ka_sg_table_t *dma_sgt)
{
    if (dma_sgt == NULL) {
        hdcdrv_err_spinlock("Input pararmeter is error.\n");
#ifdef CFG_BUILD_DEBUG
	ka_base_dump_stack();
#endif
        return;
    }

    vmngh_dma_unmap_guest_page(dev_id, fid, dma_sgt);
}

STATIC int vhdch_get_segment(struct vhdc_ctrl_msg_get_segment *vhdc_segment)
{
    vhdc_segment->segment = hdc_ctrl->segment;
    return HDCDRV_OK;
}

STATIC int vhdch_get_hdc_version(u32 dev_id, u32 fid, struct vhdc_ctrl_msg_hdc_version *vhdc_version)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];

    vhdc_version->pm_version = hdc_ctrl->pm_version;
    vdev->vm_version = vhdc_version->vm_version;
    return HDCDRV_OK;
}

STATIC void vhdch_rb_ctx_erase(struct vhdch_vdev *vdev, struct hdcdrv_ctx *ctx)
{
    ka_task_spin_lock_bh(&vdev->lock);
    ka_base_rb_erase(&ctx->ctx_node, &vdev->rb_ctx);
    ctx->refcnt--;
    ka_task_spin_unlock_bh(&vdev->lock);
}

STATIC int vhdch_rb_ctx_insert(struct vhdch_vdev *vdev, struct hdcdrv_ctx *ctx)
{
    ka_rb_node_t *parent = NULL;
    ka_rb_node_t **link = ka_base_get_rb_root_node_addr(&vdev->rb_ctx);

    ka_task_spin_lock_bh(&vdev->lock);
    while (*link != NULL) {
        struct hdcdrv_ctx *this = ka_base_rb_entry(*link, struct hdcdrv_ctx, ctx_node);
        parent = *link;
        if (ctx->node_hash < this->node_hash) {
            link = ka_base_get_rb_node_left_addr(*link);
        } else if (ctx->node_hash > this->node_hash) {
            link = ka_base_get_rb_node_right_addr(*link);
        } else {
            ka_task_spin_unlock_bh(&vdev->lock);
            return HDCDRV_F_NODE_SEARCH_FAIL;
        }
    }

    /* Add new node and rebalance tree. */
    ctx->refcnt++;
    ka_base_rb_link_node(&ctx->ctx_node, parent, link);
    ka_base_rb_insert_color(&ctx->ctx_node, &vdev->rb_ctx);

    ka_task_spin_unlock_bh(&vdev->lock);
    return HDCDRV_OK;
}

STATIC struct hdcdrv_ctx *vhdch_rb_ctx_search(struct vhdch_vdev *vdev, u64 hash)
{
    ka_rb_node_t *node = ka_base_get_rb_root_node(&vdev->rb_ctx);
    struct hdcdrv_ctx *ctx = NULL;

    ka_task_spin_lock_bh(&vdev->lock);
    while (node != NULL) {
        ctx = ka_base_rb_entry(node, struct hdcdrv_ctx, ctx_node);
        if (hash < ctx->node_hash) {
            node = ka_base_get_rb_node_left(node);
        } else if (hash > ctx->node_hash) {
            node = ka_base_get_rb_node_right(node);
        } else {
            ka_task_spin_unlock_bh(&vdev->lock);
            return ctx;
        }
    }
    ka_task_spin_unlock_bh(&vdev->lock);

    return NULL;
}

STATIC struct hdcdrv_ctx *vhdch_search_create_ctx(struct vhdch_vdev *vdev, u64 hash, u32 cmd)
{
    struct hdcdrv_ctx *ctx = NULL;
    u32 drv_cmd = _KA_IOC_NR(cmd);
    if (!((drv_cmd == HDCDRV_CMD_SERVER_CREATE) || (drv_cmd == HDCDRV_CMD_SET_SESSION_OWNER) ||
        (drv_cmd == HDCDRV_CMD_EPOLL_ALLOC_FD) || (drv_cmd == HDCDRV_CMD_EPOLL_FREE_FD) ||
        (drv_cmd == HDCDRV_CMD_SERVER_DESTROY))) {
        return HDCDRV_KERNEL_WITHOUT_CTX;
    }

    ka_task_mutex_lock(&vdev->release_mutex);
    ctx = vhdch_rb_ctx_search(vdev, hash);
    if (ctx != NULL) {
        ka_task_mutex_unlock(&vdev->release_mutex);
        return ctx;
    }

    if ((drv_cmd == HDCDRV_CMD_EPOLL_FREE_FD) || (drv_cmd == HDCDRV_CMD_SERVER_DESTROY) ||
        (vdev->ctx_num >= HDCDRV_VDEV_MAX_CTX_NUM)) {
        ka_task_mutex_unlock(&vdev->release_mutex);
        hdcdrv_info("vhdch has created ctx. (ctx_num=%d)\n", vdev->ctx_num);
        return ctx;
    }

    ctx = hdcdrv_alloc_ctx();
    if (ctx == NULL) {
        ka_task_mutex_unlock(&vdev->release_mutex);
        hdcdrv_err("Calling kzalloc failed.\n");
        return NULL;
    }

    ctx->node_hash = hash;
    ctx->fid = vdev->fid;
    ctx->service_type = HDCDRV_INVALID_VALUE;
    ctx->refcnt = 0;
    if (vhdch_rb_ctx_insert(vdev, ctx) != HDCDRV_OK) {
        ka_task_mutex_unlock(&vdev->release_mutex);
        hdcdrv_err("vhdch rbtree insert ctx failed.\n");
        hdcdrv_free_ctx(ctx);
        return NULL;
    }

    vdev->ctx_num++;
    ka_task_mutex_unlock(&vdev->release_mutex);
    return ctx;
}

STATIC struct hdcdrv_ctx *vhdch_ctx_get(struct vhdch_vdev *vdev, u64 hash)
{
    struct hdcdrv_ctx *ctx = NULL;

    ctx = vhdch_rb_ctx_search(vdev, hash);
    if (ctx != NULL) {
        ctx->refcnt++;
    }

    return ctx;
}

STATIC struct hdcdrv_ctx *vhdch_ctx_get_by_cmd(struct vhdch_vdev *vdev, u64 hash, u32 cmd)
{
    struct hdcdrv_ctx *ctx = NULL;
    u32 drv_cmd = _KA_IOC_NR(cmd);
    if (!((drv_cmd == HDCDRV_CMD_SERVER_CREATE) || (drv_cmd == HDCDRV_CMD_SET_SESSION_OWNER) ||
        (drv_cmd == HDCDRV_CMD_EPOLL_ALLOC_FD) || (drv_cmd == HDCDRV_CMD_EPOLL_FREE_FD) ||
        (drv_cmd == HDCDRV_CMD_SERVER_DESTROY))) {
        return HDCDRV_KERNEL_WITHOUT_CTX;
    }

    ka_task_mutex_lock(&vdev->release_mutex);
    ctx = vhdch_ctx_get(vdev, hash);
    ka_task_mutex_unlock(&vdev->release_mutex);

    return ctx;
}

STATIC void vhdch_ctx_put(struct vhdch_vdev *vdev, struct hdcdrv_ctx *ctx)
{
    if ((ctx == HDCDRV_KERNEL_WITHOUT_CTX) || (ctx == NULL)) {
        return;
    }

    ctx->refcnt--;
    if (ctx->refcnt <= 0) {
        hdcdrv_release_by_ctx(ctx);
        hdcdrv_free_ctx(ctx);
    }
}

STATIC void vhdch_ctx_put_by_cmd(struct vhdch_vdev *vdev, struct hdcdrv_ctx *ctx, u32 cmd)
{
    u32 drv_cmd = _KA_IOC_NR(cmd);
    if (!((drv_cmd == HDCDRV_CMD_SERVER_CREATE) || (drv_cmd == HDCDRV_CMD_SET_SESSION_OWNER) ||
        (drv_cmd == HDCDRV_CMD_EPOLL_ALLOC_FD) || (drv_cmd == HDCDRV_CMD_EPOLL_FREE_FD) ||
        (drv_cmd == HDCDRV_CMD_SERVER_DESTROY))) {
        return;
    }

    ka_task_mutex_lock(&vdev->release_mutex);
    vhdch_ctx_put(vdev, ctx);
    ka_task_mutex_unlock(&vdev->release_mutex);
}

STATIC int vhdch_mem_pool_sg_check(u32 dev_id, u32 fid, struct vhdc_ctrl_msg_pool_check *pool_check)
{
    ka_sg_table_t *dma_sgt = NULL;
    ka_dma_addr_t dma_addr;
    u32 sg_cnt = 0;
    u32 i;

    if (pool_check->size > HDCDRV_HUGE_PACKET_NUM) {
        hdcdrv_err("Input pararmeter is error. (pool_check_size=%u)\n", pool_check->size);
        return HDCDRV_ERR;
    }

    for (i = 0; i < pool_check->size; i++) {
        dma_addr = vmngh_dma_map_guest_page(dev_id, fid,
            (unsigned long)pool_check->addr[i], pool_check->segment, &dma_sgt);
        if (dma_addr == DMA_MAP_ERROR) {
            hdcdrv_err("Calling vmngh_dma_map_guest_page failed. (dev_id=%u; fid=%u; segment=%u)\n",
                dev_id, fid, pool_check->segment);
            return HDCDRV_ERR;
        }

        if (dma_sgt->nents > 1) {
            pool_check->map[i] = VHDC_MEM_POOL_SG_FLAG;
            sg_cnt++;
        }

        vmngh_dma_unmap_guest_page(dev_id, fid, dma_sgt);
    }

    pool_check->sg_cnt = sg_cnt;
    return HDCDRV_OK;
}

STATIC int vhdch_release_proxy(u32 dev_id, u32 fid, struct vhdc_ctrl_msg_release *vhdc_release)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[dev_id][fid];
    struct hdcdrv_ctx *ctx = NULL;

    ka_task_mutex_lock(&vdev->release_mutex);
    ctx = vhdch_ctx_get(vdev, vhdc_release->hash);
    if (ctx == NULL) {
        ka_task_mutex_unlock(&vdev->release_mutex);
        hdcdrv_warn("Failed to found vhdch ctx. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return HDCDRV_PARA_ERR;
    }

    vhdch_rb_ctx_erase(vdev, ctx);
    vdev->ctx_num--;
    vhdch_ctx_put(vdev, ctx);
    ka_task_mutex_unlock(&vdev->release_mutex);

    return HDCDRV_OK;
}

STATIC int vhdch_com_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct vhdc_ctrl_msg *msg = NULL;
    int ret = HDCDRV_OK;

    if (hdccom_rx_comm_msg_para_check(dev_id, fid, proc_info) != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_rx_comm_msg_para_check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return HDCDRV_PARA_ERR;
    }

    msg = (struct vhdc_ctrl_msg *)proc_info->data;
    switch (msg->type) {
        case VHDC_CTRL_MSG_TYPE_SEGMENT:
            if (hdccom_rx_comm_msg_type_check(sizeof(struct vhdc_ctrl_msg_get_segment), proc_info) != HDCDRV_OK) {
                hdcdrv_err("Calling hdccom_rx_comm_msg_type_check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
                return HDCDRV_PARA_ERR;
            }
            ret = vhdch_get_segment(&msg->vhdc_segment);
            break;
        case VHDC_CTRL_MSG_TYPE_OPEN:
            break;
        case VHDC_CTRL_MSG_TYPE_RELEASE:
            if (hdccom_rx_comm_msg_type_check(sizeof(struct vhdc_ctrl_msg_release), proc_info) != HDCDRV_OK) {
                hdcdrv_err("Calling hdccom_rx_comm_msg_type_check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
                return HDCDRV_PARA_ERR;
            }
            ret = vhdch_release_proxy(dev_id, fid, &msg->vhdc_release);
            break;
        case VHDC_CTRL_MSG_TYPE_POOL_CHECK:
            if (hdccom_rx_comm_msg_type_check(sizeof(struct vhdc_ctrl_msg_pool_check), proc_info) != HDCDRV_OK) {
                hdcdrv_err("Calling hdccom_rx_comm_msg_type_check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
                return HDCDRV_PARA_ERR;
            }
            ret = vhdch_mem_pool_sg_check(dev_id, fid, &msg->pool_check);
            break;
        case VHDC_CTRL_MSG_TYPE_HDC_VERSION:
            if (hdccom_rx_comm_msg_type_check(sizeof(struct vhdc_ctrl_msg_hdc_version), proc_info) != HDCDRV_OK) {
                hdcdrv_err("Calling hdccom_rx_comm_msg_type_check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
                return HDCDRV_PARA_ERR;
            }
            ret = vhdch_get_hdc_version(dev_id, fid, &msg->hdc_version);
            break;
        default:
            hdcdrv_err("vhdc common ctrl msg_type error. (msg_type=%u)\n", msg->type);
            ret = HDCDRV_PARA_ERR;
            break;
    }

    *(proc_info->real_out_len) = proc_info->out_data_len;
    msg->error_code = ret;

    return HDCDRV_OK;
}

struct vmng_common_msg_client vhdch_common_msg_client = {
    .type = VMNG_MSG_COMMON_TYPE_HDC,
    .init = NULL,
    .common_msg_recv = vhdch_com_msg_recv,
};

STATIC int vhdch_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct hdcdrv_cmd_common *cmd_com = NULL;
    struct vhdc_ioctl_msg *msg = NULL;
    struct vhdch_vdev *vdev = NULL;
    struct hdcdrv_ctx *ctx = NULL;
    bool copy_flag = false;
    int agent_devid;
    int ret;

    if (hdccom_rx_vpc_msg_para_check(dev_id, fid, proc_info) != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_rx_vpc_msg_para_check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return HDCDRV_PARA_ERR;
    }

    msg = (struct vhdc_ioctl_msg *)proc_info->data;
    if (hdccom_rx_vpc_cmd_type_check(msg->cmd, proc_info) != HDCDRV_OK) {
        return HDCDRV_PARA_ERR;
    }

    cmd_com = &msg->cmd_data.cmd_com;
    agent_devid = cmd_com->dev_id;

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    if (vhdch_search_create_ctx(vdev, msg->hash, msg->cmd) == NULL) {
        hdcdrv_err("Calling vhdch_search_create_ctx failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        ret = HDCDRV_F_NODE_SEARCH_FAIL;
        goto VPC_OUT;
    }

    ret = vhdch_vm_cmd_pre_proc(dev_id, fid, msg->cmd, &msg->cmd_data);
    if (ret != 0) {
        hdcdrv_err("Calling vhdch_vm_cmd_pre_proc failed. (vhdch_cmd=%u; ret=%d; dev_id=%d; fid=%d)\n",
            msg->cmd, ret, dev_id, fid);
        goto VPC_OUT;
    }

    ctx = vhdch_ctx_get_by_cmd(vdev, msg->hash, msg->cmd);
    if (ctx == NULL) {
        hdcdrv_err("Calling vhdch_ctx_ref_get failed. (vhdch_cmd=%u; ret=%d; dev_id=%d; fid=%d)\n",
            msg->cmd, ret, dev_id, fid);
        ret = HDCDRV_F_NODE_SEARCH_FAIL;
        goto VPC_OUT;
    }

    ret = (int)hdcdrv_ioctl_com(ctx, msg->cmd, &msg->cmd_data, &copy_flag, fid);
    vhdch_ctx_put_by_cmd(vdev, ctx, msg->cmd);
    if ((ret != HDCDRV_OK) && (ret != HDCDRV_CMD_CONTINUE) &&
        (ret != HDCDRV_NO_BLOCK) && (ret != HDCDRV_RX_TIMEOUT)) {
        hdcdrv_warn_limit("Calling hdcdrv_ioctl_com failed. (dev_id=%u; fid=%u; cmd=0x%x; ret=%d)\n",
            dev_id, fid, _KA_IOC_NR(msg->cmd), ret);
        /* full through */
    }

    cmd_com->dev_id = agent_devid;

VPC_OUT:
    *(proc_info->real_out_len) = proc_info->out_data_len;
    msg->copy_flag = (int)copy_flag;
    msg->error_code = ret;

    return HDCDRV_OK;
}

struct vmng_vpc_client vhdch_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_HDC,
    .init = NULL,
    .msg_recv = vhdch_vpc_msg_recv,
};

STATIC int vhdch_traffic_msg_para_check(u32 dev_id, u32 fid, const struct vmng_rx_msg_proc_info *proc_info)
{
    struct hdcdrv_ctrl_msg_sync_mem_info *mem_info = NULL;
    u32 in_len_min;
    u32 out_len_min;

    if ((proc_info == NULL) || (proc_info->real_out_len == NULL) || (proc_info->data == NULL) ||
        (proc_info->in_data_len < sizeof(struct hdcdrv_ctrl_msg_sync_mem_info)) ||
        (dev_id >= VMNG_PDEV_MAX) || (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%u; fid=%u)n", dev_id, fid);
        return HDCDRV_PARA_ERR;
    }

    mem_info = (struct hdcdrv_ctrl_msg_sync_mem_info *)proc_info->data;
    if ((u32)mem_info->phy_addr_num > HDCDRV_MEM_MAX_PHY_NUM) {
        hdcdrv_err("phy_addr_num is biger than expected. (dev_id=%u; fid=%u; phy_addr_num=%d)\n",
            dev_id, fid, mem_info->phy_addr_num);
        return HDCDRV_PARA_ERR;
    }

    in_len_min = sizeof(struct hdcdrv_ctrl_msg_sync_mem_info) + mem_info->phy_addr_num * sizeof(struct hdcdrv_dma_mem);
    out_len_min = sizeof(struct hdcdrv_ctrl_msg_sync_mem_info);

    if ((proc_info->in_data_len < in_len_min) || (proc_info->out_data_len < out_len_min)) {
        hdcdrv_err("Input pararmeter check failed. (in_data_len=%u; out_data_len=%u; in_len_size=%u; out_len_min=%u)\n",
            proc_info->in_data_len, proc_info->out_data_len, in_len_min, out_len_min);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC int vhdch_traffic_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct hdcdrv_ctrl_msg_sync_mem_info *mem_info = NULL;
    int ret;
    int flag;

    if (vhdch_traffic_msg_para_check(dev_id, fid, proc_info) != HDCDRV_OK) {
        hdcdrv_err("Calling vhdch_traffic_msg_para_check failed. (dev_id=%u; fid=%u)\n", dev_id, fid);
        return HDCDRV_PARA_ERR;
    }

    mem_info = (struct hdcdrv_ctrl_msg_sync_mem_info *)proc_info->data;

    ret = vhdch_check_vdev_ready(dev_id, fid);
    if (ret != HDCDRV_OK) {
        goto traffic_out;
    }

    ret = vhdch_update_mem_tree(dev_id, fid, mem_info->flag, mem_info);
    if (ret == HDCDRV_OK) {
        ret = hdcdrv_set_mem_info((int)dev_id, fid, HDCDRV_RBTREE_SIDE_LOCAL, mem_info);
        if (ret != HDCDRV_OK) {
            flag = mem_info->flag == HDCDRV_ADD_FLAG ? HDCDRV_DEL_FLAG : HDCDRV_ADD_FLAG;
            (void)vhdch_update_mem_tree(dev_id, fid, flag, mem_info);
        }
    }

traffic_out:
    mem_info->error_code = ret;
    *(proc_info->real_out_len) = sizeof(struct hdcdrv_ctrl_msg_sync_mem_info);

    return HDCDRV_OK;
}

struct vmng_vpc_client vhdch_traffic_msg_client = {
    .vpc_type = VMNG_VPC_TYPE_HDC_CTRL,
    .init = NULL,
    .msg_recv = vhdch_traffic_msg_recv,
};

STATIC void vhdch_vdev_wait_for_idle(struct vhdch_vdev *vdev)
{
    return;
}

STATIC void vhdch_mem_tree_uninit(u32 devid, u32 fid)
{
    struct vhdch_vdev *vdev = &hdc_ctrl->vdev[devid][fid];
    ka_rb_root_t *rbtree = &vdev->rb_mem;
    ka_rb_node_t *node = NULL;
    struct vhdch_fast_node *fast_node = NULL;
    struct hdcdrv_ctrl_msg_sync_mem_info msg = {0};
    int ret;

    node = ka_base_rb_first(rbtree);
    while (node != NULL) {
        fast_node = ka_base_rb_entry(node, struct vhdch_fast_node, mem_node);
        node = ka_base_rb_next(node);
        msg.flag = HDCDRV_DEL_FLAG;
        msg.hash_va = fast_node->hash_va;
        ret = hdcdrv_set_mem_info((int)devid, fid, HDCDRV_RBTREE_SIDE_LOCAL, &msg);
        vhdch_rb_mem_erase(vdev, fast_node);
        hdcdrv_kvfree(fast_node, KA_SUB_MODULE_TYPE_2);
    }
}

STATIC void vhdch_stop_work(struct vhdch_vdev *vdev)
{
    struct hdcdrv_ctx *ctx = NULL;
    ka_rb_node_t *node = NULL;

    if (vdev->type == VMNGH_VM) {
        ka_task_mutex_lock(&vdev->release_mutex);
        node = ka_base_rb_first(&vdev->rb_ctx);
        while (node != NULL) {
            ctx = ka_base_rb_entry(node, struct hdcdrv_ctx, ctx_node);
            node = ka_base_rb_next(node);
            hdcdrv_release_by_ctx(ctx);
            vhdch_rb_ctx_erase(vdev, ctx);
            hdcdrv_free_ctx(ctx);
        }
        ka_task_mutex_unlock(&vdev->release_mutex);
    }

    vhdch_reset_service(vdev);
}

STATIC void vhdch_remove_vdev(struct  vhdch_vdev *vdev)
{
    vhdch_stop_work(vdev);
    vhdch_uninit_msgchan_pool(vdev->dev_id, vdev->fid);
    vhdch_uninit_service(vdev->dev_id, vdev->fid);
}

STATIC int vhdch_init_instance(u32 dev_id, u32 fid, u32 aicore_num, u32 total_aicore_num)
{
    struct vhdch_vdev *vdev = NULL;
    int ret;

    if (hdc_ctrl->devices[dev_id].valid != HDCDRV_VALID) {
        hdcdrv_err("Device is not ready. (dev_id=%u)\n", dev_id);
        return HDCDRV_DEVICE_NOT_READY;
    }

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    vdev->fid = fid;
    vdev->dev_id = dev_id;
    /* vpc vm_id start from 0 */
    vdev->vm_id = (u32)vmngh_ctrl_get_vm_id(dev_id, fid) + 1;
    vdev->type = VMNGH_VM;
    vdev->msg_chan_cnt = 0;
    vdev->cur_alloc_long_session = 0;
    vdev->cur_alloc_short_session = 0;
    vdev->ctx_num = 0;
    vdev->fast_node_num_avaliable = HDCDRV_VDEV_MAX_FAST_NODE_NUM;
    vdev->fnode_phy_num_avaliable = HDCDRV_VDEV_MAX_FNODE_PHY_NUM;
    vdev->rb_ctx = KA_RB_ROOT;
    vdev->rb_mem = KA_RB_ROOT;
    vdev->vm_version = HDCDRV_INVALID_HDC_VERSION;
    ka_task_spin_lock_init(&vdev->lock);
    ka_task_spin_lock_init(&vdev->mem_lock);
    ka_task_mutex_init(&vdev->mutex);
    ka_task_mutex_init(&vdev->release_mutex);
    ka_base_atomic64_set(&vdev->busy, 0);
    vdev->valid = HDCDRV_INVALID;

    hdcdrv_info("Init. (vm_id=%u; dev_id=%u; fid=%u; aicore_num=%u; total_aicore_num=%u)",
        vdev->vm_id, dev_id, fid, aicore_num, total_aicore_num);

    ret = vhdch_init_service(dev_id, fid);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdch_init_service failed. (ret=%d)\n", ret);
        return ret;
    }

    if (total_aicore_num == 0) {
        hdcdrv_err("total_aicore_num error.\n");
        return HDCDRV_ERR;
    }

    ret = vhdch_init_msgchan_pool(dev_id, fid, aicore_num, total_aicore_num);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdch_init_msgchan_pool failed. (ret=%d)\n", ret);
        goto uninit_service;
    }

    ret = vmngh_register_common_msg_client(dev_id, fid, &vhdch_common_msg_client);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vmngh_register_common_msg_client failed. (ret=%d)\n", ret);
        goto uninit_msgchan_pool;
    }

    ret = vmngh_vpc_register_client_safety(dev_id, fid, &vhdch_vpc_client);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vmngh_vpc_register_client failed. (ret=%d)\n", ret);
        goto unregister_common_msg_client;
    }

    ret = vmngh_vpc_register_client_safety(dev_id, fid, &vhdch_traffic_msg_client);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vmngh_vpc_register_client failed. (ret=%d; dev_id=%u; fid=%u)\n",
            ret, dev_id, fid);
        goto unregister_vpc_client;
    }

    vdev->valid = HDCDRV_VALID;
    return HDCDRV_OK;

unregister_vpc_client:
    vmngh_vpc_unregister_client(dev_id, fid, &vhdch_vpc_client);
unregister_common_msg_client:
    vmngh_unregister_common_msg_client(dev_id, fid, &vhdch_common_msg_client);
uninit_msgchan_pool:
    vhdch_uninit_msgchan_pool(dev_id, fid);
uninit_service:
    vhdch_uninit_service(dev_id, fid);

    return ret;
}

STATIC int vhdch_uninit_instance(u32 dev_id, u32 fid)
{
    struct vhdch_vdev *vdev = NULL;

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    vhdch_set_vdev_status(dev_id, fid, HDCDRV_INVALID);
    vhdch_vdev_wait_for_idle(vdev);

    vmngh_vpc_unregister_client(dev_id, fid, &vhdch_traffic_msg_client);
    vmngh_vpc_unregister_client(dev_id, fid, &vhdch_vpc_client);
    vmngh_unregister_common_msg_client(dev_id, fid, &vhdch_common_msg_client);

    vhdch_remove_vdev(vdev);
    vhdch_mem_tree_uninit(dev_id, fid);
    vdev->dev_id = 0;
    vdev->fid = 0;
    vdev->vm_id = 0;
    vdev->msg_chan_cnt = 0;
    vdev->cur_alloc_long_session = 0;
    vdev->cur_alloc_short_session = 0;
    ka_base_atomic64_set(&vdev->busy, 0);

    return HDCDRV_OK;
}

STATIC int vhdch_init_container_instance(u32 dev_id, u32 fid, u32 aicore_num, u32 total_aicore_num)
{
    struct vhdch_vdev *vdev = NULL;
    int ret;

    if (hdc_ctrl->devices[dev_id].valid != HDCDRV_VALID) {
        hdcdrv_err("Device is not ready. (dev_id=%u)\n", dev_id);
        return HDCDRV_DEVICE_NOT_READY;
    }

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    vdev->fid = fid;
    vdev->dev_id = dev_id;
    vdev->vm_id = 0; /* set vim_id to be zero in container */
    vdev->type = VMNGH_CONTAINER;
    vdev->msg_chan_cnt = 0;
    vdev->cur_alloc_long_session = 0;
    vdev->cur_alloc_short_session = 0;
    ka_task_mutex_init(&vdev->mutex);
    ka_task_spin_lock_init(&vdev->mem_lock);
    ka_base_atomic64_set(&vdev->busy, 0);
    vdev->valid = HDCDRV_INVALID;

    hdcdrv_info("Init. (dev_id=%u; fid=%u; aicore_num=%u; total_aicore_num=%u)",
        dev_id, fid, aicore_num, total_aicore_num);

    if (vhdch_init_service(dev_id, fid) != HDCDRV_OK) {
        hdcdrv_err("Calling vhdch_init_service failed.\n");
        return HDCDRV_ERR;
    }

    if (total_aicore_num == 0) {
        hdcdrv_err("total_aicore_num error.\n");
        return HDCDRV_ERR;
    }

    ret = vhdch_init_msgchan_pool(dev_id, fid, aicore_num, total_aicore_num);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdch_init_msgchan_pool failed. (ret=%d)\n", ret);
        goto uninit_service;
    }

    vhdch_init_mem_pool(dev_id, fid, aicore_num, total_aicore_num);

    vdev->valid = HDCDRV_VALID;
    return HDCDRV_OK;

uninit_service:
    vhdch_uninit_service(dev_id, fid);

    return ret;
}

STATIC int vhdch_uninit_container_instance(u32 dev_id, u32 fid)
{
    struct vhdch_vdev *vdev = NULL;

    vdev = &hdc_ctrl->vdev[dev_id][fid];
    vhdch_set_vdev_status(dev_id, fid, HDCDRV_INVALID);
    vhdch_vdev_wait_for_idle(vdev);
    vhdch_remove_vdev(vdev);
    vhdch_uninit_mem_pool(dev_id, fid);
    vdev->dev_id = 0;
    vdev->fid = 0;
    vdev->vm_id = 0;
    vdev->msg_chan_cnt = 0;
    vdev->cur_alloc_long_session = 0;
    vdev->cur_alloc_short_session = 0;
    ka_base_atomic64_set(&vdev->busy, 0);

    return HDCDRV_OK;
}

static int vhdcd_get_aicore_num(u32 udevid, u32 phy_devid, u32 *aicore_num, u32 *total_aicore_num)
{
    u64 bitmap;
    u32 unit_per_bit;
    int ret;

    ret = soc_resmng_dev_get_mia_res(udevid, MIA_AC_AIC, &bitmap, &unit_per_bit);
    if (ret != 0) {
#ifndef DRV_UT
        hdcdrv_err("Get aicore bitmap failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
#endif
    }
    *aicore_num = (u32)ka_base_bitmap_weight((const unsigned long *)&bitmap, 64); /* 64 u64 bitnum */

    ret = soc_resmng_dev_get_mia_res(phy_devid, MIA_AC_AIC, &bitmap, &unit_per_bit);
    if (ret != 0) {
#ifndef DRV_UT
        hdcdrv_err("Get aicore bitmap failed. (udevid=%u; ret=%d)\n", phy_devid, ret);
        return ret;
#endif
    }
    *total_aicore_num = (u32)ka_base_bitmap_weight((const unsigned long *)&bitmap, 64); /* 64 u64 bitnum */

    return 0;
}

#define HDC_HOST_MIA_NOTIFIER "hdc_mia"
static int vhdcd_mia_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
#ifndef DRV_UT
        hdcdrv_err("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
#endif
    }
    fid = mia_para.sub_devid + 1;

    if (vmng_get_device_split_mode(mia_para.phy_devid) != VMNG_CONTAINER_SPLIT_MODE) {
        hdcdrv_info("Not container split. (udevid=%u; action=%d)\n", udevid, action);
        return 0;
    }

#ifndef DRV_UT
    if (action == UDA_INIT) {
        u32 aicore_num, total_aicore_num;
        ret = vhdcd_get_aicore_num(udevid, mia_para.phy_devid, &aicore_num, &total_aicore_num);
        if (ret == 0) {
            ret = vhdch_init_container_instance(mia_para.phy_devid, fid, aicore_num, total_aicore_num);
        }
    } else if (action == UDA_UNINIT) {
        ret = vhdch_uninit_container_instance(mia_para.phy_devid, fid);
    }

    hdcdrv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
#endif

    return ret;
}

static int vhdcd_mia_dev_agent_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
#ifndef DRV_UT
        hdcdrv_err("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
#endif
    }
    fid = mia_para.sub_devid + 1;

    if (action == UDA_INIT) {
        u32 aicore_num, total_aicore_num;
        ret = vhdcd_get_aicore_num(udevid, mia_para.phy_devid, &aicore_num, &total_aicore_num);
        if (ret == 0) {
            ret = vhdch_init_instance(mia_para.phy_devid, fid, aicore_num, total_aicore_num);
        }
    } else if (action == UDA_UNINIT) {
        ret = vhdch_uninit_instance(mia_para.phy_devid, fid);
    }

    hdcdrv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

STATIC int vhdch_notifier_register(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_near_virtual_entity_type_pack(&type);
    ret = uda_notifier_register(HDC_HOST_MIA_NOTIFIER, &type, UDA_PRI1, vhdcd_mia_dev_notifier_func);
    if (ret != 0) {
#ifndef DRV_UT
        hdcdrv_err("Register mia dev notifier failed. (ret=%d)\n", ret);
        return ret;
#endif
    }

    uda_davinci_near_virtual_agent_type_pack(&type);
    ret = uda_notifier_register(HDC_HOST_MIA_NOTIFIER, &type, UDA_PRI1, vhdcd_mia_dev_agent_notifier_func);
    if (ret != 0) {
#ifndef DRV_UT
        uda_davinci_near_virtual_entity_type_pack(&type);
        (void)uda_notifier_unregister(HDC_HOST_MIA_NOTIFIER, &type);
        hdcdrv_err("Register mia dev agent notifier failed. (ret=%d)\n", ret);
        return ret;
#endif
    }

    return HDCDRV_OK;
}

STATIC void vhdch_notifier_unregister(void)
{
    struct uda_dev_type type;

    uda_davinci_near_virtual_agent_type_pack(&type);
    (void)uda_notifier_unregister(HDC_HOST_MIA_NOTIFIER, &type);
    uda_davinci_near_virtual_entity_type_pack(&type);
    (void)uda_notifier_unregister(HDC_HOST_MIA_NOTIFIER, &type);
}

int vhdch_init(void)
{
    int ret;

    hdccom_fill_cmd_size_table();

    ret = vhdch_notifier_register();
    if (ret != 0) {
        return ret;
    }

    hdcdrv_info("vhdch_inits success.\n");
    return HDCDRV_OK;
}

int vhdch_uninit(void)
{
    vhdch_notifier_unregister();

    hdcdrv_info("vhdch_uninit success.\n");
    return HDCDRV_OK;
}

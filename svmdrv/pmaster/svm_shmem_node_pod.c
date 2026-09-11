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
#include <linux/types.h>
#include "securec.h"

#include "ascend_hal_define.h"
#include "ascend_kernel_hal.h"
#include "dbl/feature_loader.h"
#include "dbl/spod_info.h"
#include "devdrv_interface.h"
#include "svm_log.h"
#include "devmm_proc_info.h"
#include "svm_shmem_node.h"
#include "svm_shmem_node_pod.h"

#define FAULT_POD_HASH_LIST_NUM_SHIFT 6
#define FAULT_POD_HASH_LIST_NUM (1 << FAULT_POD_HASH_LIST_NUM_SHIFT)  /* 64 */

KA_DECLARE_HASHTABLE(g_fault_pod_htable, FAULT_POD_HASH_LIST_NUM_SHIFT);
static KA_TASK_DEFINE_RWLOCK(g_fault_pod_rwlock);

struct devmm_fault_pod_node {
    ka_hlist_node_t link;
    ka_pid_t creator_pid;
    u32 fault_sdid;
};

struct devmm_ipc_pod_msg_set_pid {
    char name[DEVMM_IPC_MEM_NAME_SIZE];
    struct svm_id_inst inst;
    u32 sdid;

    u64 vptr;   /* Creator vptr */
    size_t len;
    size_t page_size;
    bool is_huge;
    bool is_reserve_addr;

    int creator_pid;
    ka_pid_t pid[IPC_WLIST_SET_NUM];
    u32 pid_num;
};

struct devmm_ipc_pod_msg_destroy {
    char name[DEVMM_IPC_MEM_NAME_SIZE];
    int creator_pid;
};

struct devmm_ipc_pod_msg_mem_repair {
    char name[DEVMM_IPC_MEM_NAME_SIZE];
    ka_pid_t creator_pid;
};

typedef int (*devmm_ipc_pod_msg_rcv_func_t)(u32 devid, struct devmm_ipc_pod_msg_data *msg);

static int devmm_ipc_pod_msg_sync_send(u32 devid, u32 sdid, void *msg, size_t size)
{
    struct data_input_info data = {.data = msg, .data_len = size, .in_len = size, .out_len = 0, .msg_mode = DEVDRV_S2S_SYNC_MODE};
    int ret;

    ret = devdrv_s2s_msg_send(devid, sdid, DEVDRV_S2S_MSG_DEVMM, DEVDRV_S2S_TO_HOST, &data);
    if (ret != 0) {
        devmm_drv_err("Pod sync send fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        return ret;
    }

    return 0;
}

int devmm_s2s_msg_sync_send(u32 devid, u32 sdid, void *msg, size_t size)
{
    u32 tmp_devid = devid;
    if (devid == uda_get_host_id()) {
        /* use logic id 0 */
        int ret = uda_devid_to_udevid(0, &tmp_devid);
        if (ret != 0) {
            devmm_drv_err("Get udevid fail. (ret=%d)\n", ret);
            return ret;
        }
    }
    return devmm_ipc_pod_msg_sync_send(tmp_devid, sdid, msg, size);
}

static int devmm_ipc_pod_msg_async_send(u32 devid, u32 sdid, void *msg, size_t size)
{
    struct data_input_info data = {.data = msg, .data_len = size, .in_len = size, .out_len = 0, .msg_mode = DEVDRV_S2S_ASYNC_MODE};
    int ret;

    ret = devdrv_s2s_msg_send(devid, sdid, DEVDRV_S2S_MSG_DEVMM, DEVDRV_S2S_TO_HOST, &data);
    if (ret != 0 && ret != -EBUSY) {
        devmm_drv_err("Pod async send fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        return ret;
    }

    return ret;
}

static int devmm_ipc_pod_msg_async_recv(u32 devid, u32 sdid, void *msg, size_t size, bool last_recv)
{
    struct data_recv_info data = {.data = msg, .data_len = size, .out_len = 0, .flag = DEVDRV_S2S_KEEP_RECV};
    int ret;

    if (last_recv) {
        data.flag = DEVDRV_S2S_END_RECV;
    }
    ret = devdrv_s2s_async_msg_recv(devid, sdid, DEVDRV_S2S_MSG_DEVMM, &data);
    if (ret != 0) {
        if ((ret != -EAGAIN) || last_recv) {
            devmm_drv_err("Pod async recv fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        }
        return ret;
    }

    return 0;
}

static bool devmm_ipc_pod_is_local_pod(u32 devid, u32 sdid)
{
    struct sdid_parse_info parse = {0};
    struct spod_info spod = {0};
    int ret;

    ret = dbl_get_spod_info(devid, &spod);
    if (ret != 0) {
        devmm_drv_err("Get spod info fail. (ret=%d; devid=%u)\n", ret, devid);
        return true;
    }

    ret = dbl_parse_sdid(sdid, &parse);
    if (ret != 0) {
        devmm_drv_err("Parse sdid fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        return true;
    }

    /* 0x3FF server id only have 10 bits */
    return ((spod.server_id & 0x3FF) == (parse.server_id & 0x3FF)) ? true : false;
}

bool svm_is_sdid_in_local_server(u32 devid, u32 sdid)
{
    u32 tmp_devid = devid;
    if (devid == uda_get_host_id()) {
        /* use logic id 0 */
        int ret = uda_devid_to_udevid(0, &tmp_devid);
        if (ret != 0) {
            devmm_drv_err("Get udevid fail. (ret=%d)\n", ret);
            return true;
        }
    }

    return devmm_ipc_pod_is_local_pod(tmp_devid, sdid);
}

static int devmm_ipc_pod_setpid_msg_send(const char *name, struct svm_id_inst *inst,
    u32 sdid, ka_pid_t pid[], u32 pid_num)
{
    struct devmm_ipc_pod_msg_set_pid *set_pid = NULL;
    struct devmm_ipc_pod_msg_data msg;
    struct devmm_ipc_node_attr attr;
    int ret, i;

    /* When set pid. sdid is KA_UINT_MAX means pid is on the currnet server */
    if (sdid == KA_UINT_MAX) {
        return 0;
    }

    ret = devmm_ipc_query_node_attr(name, &attr);
    if (ret != 0) {
        devmm_drv_err("Query ipc node attr fail. (ret=%d; name=%s)\n", ret, name);
        return ret;
    }
    set_pid = (struct devmm_ipc_pod_msg_set_pid *)msg.payload;
    for (i = 0; i < DEVMM_IPC_MEM_NAME_SIZE; i++) {
        set_pid->name[i] = name[i];
    }

    for (i = 0; i < IPC_WLIST_SET_NUM; i++) {
        set_pid->pid[i] = pid[i];
    }

    set_pid->vptr = attr.vptr;   /* Creator vptr */
    set_pid->len = attr.len;
    set_pid->page_size = attr.page_size;
    set_pid->is_huge = attr.is_huge;
    set_pid->is_reserve_addr = attr.is_reserve_addr;
    set_pid->sdid = attr.sdid;
    set_pid->creator_pid = ka_task_get_current_tgid();
    set_pid->pid_num = pid_num;
    set_pid->inst = *inst;

    msg.header.devid = set_pid->inst.devid;
    msg.header.cmdtype = DEVMM_IPC_POD_MSG_SET_PID;
    msg.header.result = 0;
    msg.header.valid = DEVMM_IPC_POD_MSG_SEND_MAGIC;

    ret = devmm_ipc_pod_msg_sync_send(set_pid->inst.devid, sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data));
    if ((ret != 0) || (msg.header.result != 0) || (msg.header.valid != DEVMM_IPC_POD_MSG_RCV_MAGIC)) {
        devmm_drv_err("Ipc pod msg send fail. (ret=%d; result=%d; valid=0x%x; devid=%u; sdid=%u)\n",
            ret, msg.header.result, msg.header.valid, set_pid->inst.devid, sdid);
        return -EFAULT;
    }

    return 0;
}

static int devmm_ipc_pod_get_devid_and_sdid(const char *name, struct svm_id_inst *inst, u32 *sdid)
{
    int ret;

    ret = devmm_ipc_node_get_inst_by_name(name, inst);
    if (ka_likely(ret == 0)) {
        /* Update sdid */
        if (devmm_ipc_pod_is_local_pod(inst->devid, *sdid)) {
            /* When set wlist, pids in current server, sdid will be set to KA_UINT_MAX */
            *sdid = KA_UINT_MAX;
        }
    }
    return ret;
}

static int devmm_ioctl_ipc_pod_set_pid(struct devmm_svm_process *svm_pro, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_ipc_set_pid_para *karg = &arg->data.ipc_set_pid_para;
    struct devmm_ipc_setpid_attr attr;
    int ret;

    karg->name[DEVMM_MAX_NAME_SIZE - 1] = '\0';
    ret = devmm_ipc_pod_get_devid_and_sdid(karg->name, &attr.inst, &karg->sdid);
    if (ret != 0) {
        devmm_drv_err("Update sdid fail. (ret=%d; name=%s)\n", ret, karg->name);
        return ret;
    }

    attr.name = karg->name;
    attr.sdid = karg->sdid;
    attr.pid = karg->set_pid;
    attr.pid_num = karg->num;
    attr.creator_pid = ka_task_get_current_tgid();
    attr.send = devmm_ipc_pod_setpid_msg_send;

    ret = devmm_ipc_node_set_pids(&attr);
    if (ret != 0) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Sent sdpid msg fail. (ret=%d; name=%s; sdid=%u)\n", ret, karg->name, karg->sdid);
        return ret;
    }

    return 0;
}

static int devmm_ipc_pod_msg_check(u32 devid, u32 sdid, struct data_input_info *data)
{
    struct devmm_ipc_pod_msg_data *msg = NULL;

    if (ka_unlikely(devid >= DEVMM_MAX_DEVICE_NUM)) {
        devmm_drv_err("Invalid devid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (ka_unlikely((data == NULL) || (data->data == NULL))) {
        devmm_drv_err("Invalid data. (devid=%u; sdid=%u)\n", devid, sdid);
        return -EINVAL;
    }

    if (ka_unlikely(data->in_len != sizeof(struct devmm_ipc_pod_msg_data))) {
        devmm_drv_err("Invalid data len. (in_len=%u; data_size=%ld; devid=%u; sdid=%u)\n",
            data->in_len, sizeof(struct devmm_ipc_pod_msg_data), devid, sdid);
        return -EINVAL;
    }
    msg = (struct devmm_ipc_pod_msg_data *)data->data;
    if (ka_unlikely(msg->header.valid != DEVMM_IPC_POD_MSG_SEND_MAGIC)) {
        devmm_drv_err("Invalid magic. (magic=0x%x; devid=%u; sdid=%u; cmdtype=%d)\n",
            msg->header.valid, devid, sdid, msg->header.cmdtype);
        return -EINVAL;
    }

    if (ka_unlikely(msg->header.cmdtype >= (u32)DEVMM_IPC_POD_MSG_MAX)) {
        devmm_drv_err("Invalid cmd. (cmd=%u; devid=%u; sdid=%u)\n", msg->header.cmdtype, devid, sdid);
        return -EINVAL;
    }
    return 0;
}

static void devmm_ipc_pod_node_attr_pack(struct devmm_ipc_pod_msg_set_pid *set_pid_msg,
    struct devmm_ipc_node_attr *attr)
{
    int i;

    for (i = 0; i < DEVMM_IPC_MEM_NAME_SIZE; i++) {
        attr->name[i] = set_pid_msg->name[i];
    }
    attr->name[DEVMM_IPC_MEM_NAME_SIZE - 1] = '\0';
    attr->inst = set_pid_msg->inst;
    attr->init_fn = NULL;
    attr->uninit_fn = NULL;
    attr->svm_proc = NULL;
    attr->pid = set_pid_msg->creator_pid;
    attr->sdid = set_pid_msg->sdid;
    attr->vptr = set_pid_msg->vptr;
    attr->len = set_pid_msg->len;
    attr->page_size = set_pid_msg->page_size;
    attr->is_huge = set_pid_msg->is_huge;
    attr->is_reserve_addr = set_pid_msg->is_reserve_addr;
    attr->need_set_wlist = true;
}

static int devmm_ipc_pod_node_attr_check(struct devmm_ipc_node_attr *attr)
{
#ifndef EMU_ST
    if ((attr->inst.devid >= SVM_MAX_AGENT_NUM) || (attr->inst.vfid >= DEVMM_MAX_VF_NUM)) {
        devmm_drv_err("Invalid devid or vfid. (devid=%u; vfid=%u)\n", attr->inst.devid, attr->inst.vfid);
        return -EINVAL;
    }
#endif
    return 0;
}

static int devmm_ipc_pod_shadow_set_pid(u32 devid, struct devmm_ipc_pod_msg_data *msg)
{
    struct devmm_ipc_pod_msg_set_pid *set_pid_msg = (struct devmm_ipc_pod_msg_set_pid *)msg->payload;
    struct devmm_ipc_node_attr attr;
    int ret;

    devmm_ipc_pod_node_attr_pack(set_pid_msg, &attr);
    ret = devmm_ipc_pod_node_attr_check(&attr);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Invalid ipc node attr. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }
    ret = devmm_ipc_node_set_pids_ex(&attr, KA_UINT_MAX, set_pid_msg->creator_pid,
        set_pid_msg->pid, set_pid_msg->pid_num);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Set pid fail. (ret=%d; name=%s; devid=%u; sdid=%u; pid=%d)\n",
            ret, attr.name, attr.inst.devid, set_pid_msg->sdid, attr.pid);
        return ret;
    }

    return 0;
}

static int devmm_ipc_pod_shadow_destroy(u32 devid, struct devmm_ipc_pod_msg_data *msg)
{
#ifndef EMU_ST
    struct devmm_ipc_pod_msg_destroy *destroy_msg = (struct devmm_ipc_pod_msg_destroy *)msg->payload;
    int ret;

    ret = devmm_ipc_node_destroy(destroy_msg->name, destroy_msg->creator_pid, false);
    if (ret != 0) {
        /*
         * Do not add any log here:
         * If a remote server has set many current server's PID in its whitelist,
         * it will send multiple destroy messages.
         */
        return ret;
    }
#endif
    return 0;
}

static int devmm_ipc_pod_shadow_mem_repair(u32 devid, struct devmm_ipc_pod_msg_data *msg)
{
    struct devmm_ipc_pod_msg_mem_repair *mem_repair_msg = (struct devmm_ipc_pod_msg_mem_repair *)msg->payload;

    return devmm_ipc_node_mem_repair_by_name(mem_repair_msg->name, mem_repair_msg->creator_pid);
}

static const devmm_ipc_pod_msg_rcv_func_t rcv_ops[DEVMM_IPC_POD_MSG_MAX] = {
    [DEVMM_IPC_POD_MSG_SET_PID] = devmm_ipc_pod_shadow_set_pid,
    [DEVMM_IPC_POD_MSG_DESTROY] = devmm_ipc_pod_shadow_destroy,
    [DEVMM_IPC_POD_MSG_MEM_REPAIR] = devmm_ipc_pod_shadow_mem_repair,
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    [DEVMM_GET_SHARE_INFO] = devmm_get_remote_share_mem_info_process,
    [DEVMM_PUT_SHARE_INFO] = devmm_put_remote_share_mem_info_process,
    [DEVMM_GET_BLK_INFO] = devmm_get_cs_host_chan_target_blk_info_process,
#endif
};

int devmm_ipc_pod_msg_recv(u32 devid, u32 sdid, struct data_input_info *data)
{
    struct devmm_ipc_pod_msg_data *msg = NULL;
    int ret;

    ret = devmm_ipc_pod_msg_check(devid, sdid, data);
    if (ret != 0) {
        devmm_drv_err("Ipc pod msg check fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        return ret;
    }
    msg = data->data;
    if (rcv_ops[msg->header.cmdtype] != NULL) {
        ret = rcv_ops[msg->header.cmdtype](devid, msg);
        data->out_len = (u32)sizeof(struct devmm_ipc_pod_msg_data);
        msg->header.valid = DEVMM_IPC_POD_MSG_RCV_MAGIC;
        msg->header.result = ret;
    }
    return 0;
}

static bool devmm_is_need_ipc_pod_destroy_msg(u32 devid, u32 sdid)
{
    int ret, status;

    ret = hal_kernel_get_spod_node_status(devid, sdid, &status);
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            devmm_drv_debug("Can't get sdid status. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        }
        return true;
    }

    if (status != DMS_SPOD_NODE_STATUS_NORMAL) {
        devmm_drv_debug("Sdid status is abnormal. (status=%d; devid=%u; sdid=%u)\n", status, devid, sdid);
        return false;
    }

    return true;
}

static int devmm_ipc_pod_destroy_sync_msg_send(const char *name, ka_pid_t pid, u32 devid, u32 sdid)
{
    struct devmm_ipc_pod_msg_destroy *destroy_msg = NULL;
    struct devmm_ipc_pod_msg_data msg;
    int ret;

    if (devmm_is_need_ipc_pod_destroy_msg(devid, sdid) == false) {
        return -EPERM;
    }

    destroy_msg = (struct devmm_ipc_pod_msg_destroy *)msg.payload;
    ret = memcpy_s(destroy_msg->name, DEVMM_IPC_MEM_NAME_SIZE, name, DEVMM_IPC_MEM_NAME_SIZE);
    if (ret != EOK) {
        return -ENOMEM;
    }
    destroy_msg->creator_pid = pid;
    msg.header.devid = devid;
    msg.header.cmdtype = DEVMM_IPC_POD_MSG_DESTROY;
    msg.header.result = 0;
    msg.header.valid = DEVMM_IPC_POD_MSG_SEND_MAGIC;

    ret = devmm_ipc_pod_msg_sync_send(devid, sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data));
    if (ret != 0) {
        devmm_drv_err("Send msg fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        return ret;
    }

    /*
     * (msg.header.result == -EFAULT) means destroy msg has been sent and remote node is processing this.
     * So do not print log here
     */
    if ((msg.header.result != 0) && (msg.header.result != -EFAULT)) {
        devmm_drv_err("Msg handle fail. (result=%d; devid=%u; sdid=%u)\n", msg.header.result, devid, sdid);
        return -EFAULT;
    }

    return 0;
}

static int devmm_ipc_pod_destroy_msg_async_send(const char *name, ka_pid_t pid, u32 devid, u32 sdid)
{
    struct devmm_ipc_pod_msg_destroy *destroy_msg = NULL;
    struct devmm_ipc_pod_msg_data msg;
    int ret;

    if (devmm_is_need_ipc_pod_destroy_msg(devid, sdid) == false) {
        return -EPERM;
    }

    destroy_msg = (struct devmm_ipc_pod_msg_destroy *)msg.payload;
    ret = memcpy_s(destroy_msg->name, DEVMM_IPC_MEM_NAME_SIZE, name, DEVMM_IPC_MEM_NAME_SIZE);
    if (ret != EOK) {
        return -ENOMEM;
    }
    destroy_msg->creator_pid = pid;
    msg.header.devid = devid;
    msg.header.cmdtype = DEVMM_IPC_POD_MSG_DESTROY;
    msg.header.result = 0;
    msg.header.valid = DEVMM_IPC_POD_MSG_SEND_MAGIC;

    ret = devmm_ipc_pod_msg_async_send(devid, sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data));
    if (ret != 0) {
        devmm_drv_err_if((ret != -EBUSY), "Send msg fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        return ret;
    }

    return 0;
}

static int devmm_ipc_pod_destroy_msg_async_recv(u32 devid, u32 sdid, bool last_recv)
{
    struct devmm_ipc_pod_msg_data msg;
    int ret;

    msg.header.result = 0;
    ret = devmm_ipc_pod_msg_async_recv(devid, sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data), last_recv);
    if (ret != 0) {
        devmm_drv_err_if(((ret != -EAGAIN) || last_recv), "Recv msg fail. (ret=%d; devid=%u; sdid=%u; last_recv=%u)\n",
            ret, devid, sdid, last_recv);
        return ret;
    }
    /*
      * (msg.header.result == -EFAULT) means destroy msg has been sent and remote node is processing this.
      * So do not print log here
      */
     if ((msg.header.result != 0) && (msg.header.result != -EFAULT)) {
         devmm_drv_err("Msg handle fail. (result=%d; devid=%u; sdid=%u)\n", msg.header.result, devid, sdid);
         return -EFAULT;
     }

    return 0;
}

#define IPC_POD_DESTROY_S2S_SEND_FAIL 0x1U
#define IPC_POD_DESTROY_S2S_SEND_BUSY 0x2U
#define IPC_POD_DESTROY_S2S_SEND_SUCC 0x3U
#define IPC_POD_DESTROY_S2S_RECV_FAIL 0x4U
#define IPC_POD_DESTROY_S2S_RECV_BUSY 0x5U
#define IPC_POD_DESTROY_S2S_RECV_SUCC 0x6U

#define IPC_POD_DESTROY_MAX_TIME 30000000000ULL /* 30s */

struct ipc_node_sdid_list {
    u32 sdid;
    u16 send_status;
    u16 recv_status;
};

struct ipc_node_destroy_stats {
    u32 send_fail_cnt;
    u32 send_succ_cnt;
    u32 recv_fail_cnt;
    u32 recv_succ_cnt;
    u32 should_recv_cnt;
};

static bool devmm_ipc_pod_check_fault_sdid_status(u32 send_status, u32 recv_status)
{
    return ((send_status == IPC_POD_DESTROY_S2S_SEND_FAIL || send_status == IPC_POD_DESTROY_S2S_SEND_BUSY) ||
        (recv_status == IPC_POD_DESTROY_S2S_RECV_FAIL || recv_status == IPC_POD_DESTROY_S2S_RECV_BUSY));
}

static void devmm_ipc_pod_update_fault_sdid(ka_pid_t pid, struct ipc_node_sdid_list*sdid_list, u32 sdid_num)
{
    struct devmm_fault_pod_node *node = NULL;
    u32 key = pid % FAULT_POD_HASH_LIST_NUM;
    u32 i;

    ka_task_write_lock(&g_fault_pod_rwlock);
    for (i = 0; i < sdid_num; i++) {
        if (devmm_ipc_pod_check_fault_sdid_status(sdid_list[i].send_status, sdid_list[i].recv_status)) {
            node = devmm_kzalloc_ex(sizeof(struct devmm_fault_pod_node), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
            if (node != NULL) {
                node->creator_pid = pid;
                node->fault_sdid = sdid_list[i].sdid;
                ka_hash_add(g_fault_pod_htable, &node->link, key);
            }
        }
    }
    ka_task_write_unlock(&g_fault_pod_rwlock);
}

static bool devmm_ipc_pod_check_sdid_is_fault(ka_pid_t pid, u32 sdid)
{
    struct devmm_fault_pod_node *node = NULL;
    u32 key = pid % FAULT_POD_HASH_LIST_NUM;

    ka_task_read_lock(&g_fault_pod_rwlock);
    ka_hash_for_each_possible(g_fault_pod_htable, node, link, key) {
        if ((node->creator_pid == pid) && (node->fault_sdid == sdid)) {
            ka_task_read_unlock(&g_fault_pod_rwlock);
            return true;
        }
    }
    ka_task_read_unlock(&g_fault_pod_rwlock);
    return false;
}

static void devmm_ipc_pod_clear_fault_sdid(ka_pid_t pid, bool async_recycle)
{
    struct devmm_fault_pod_node *node = NULL;
    u32 key = pid % FAULT_POD_HASH_LIST_NUM;
    ka_hlist_node_t *tmp = NULL;

    if (async_recycle == false) {
        return;
    }
    ka_task_write_lock(&g_fault_pod_rwlock);
    ka_hash_for_each_possible_safe(g_fault_pod_htable, node, tmp, link, key) {
        if (node->creator_pid == pid) {
            ka_hash_del(&node->link);
            devmm_kfree_ex(node);
        }
    }

    ka_task_write_unlock(&g_fault_pod_rwlock);
}

static struct ipc_node_sdid_list* devmm_create_ipc_node_sdid_list(struct devmm_ipc_node *node, u32 *sdid_num)
{
    struct ipc_node_sdid_list *sdid_list = NULL;
    struct ipc_node_wlist *wlist = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    if (ka_list_empty_careful(&node->wlist_head)) {
        return NULL;
    }

    sdid_list = devmm_kvzalloc(sizeof(struct ipc_node_sdid_list) * node->wlist_num);
    if (sdid_list == NULL) {
        devmm_drv_err("Kvzalloc sdid_list failed. (wlist_num=%u)\n", node->wlist_num);
        return NULL;
    }

    *sdid_num = 0;
    ka_list_for_each_entry(wlist, &node->wlist_head, list) {
        if (wlist->sdid == KA_UINT_MAX) {
            /* wlist.sdid == UNIX_MAX means wlist.pid is in current server */
            continue;
        }
        for (i = 0; i < *sdid_num; i++) {
            if (sdid_list[i].sdid == wlist->sdid) {
                break;
            }
        }
        /* Indicates that duplicate sdid cannot be found and needs to be added to sdid_list */
        if (i == *sdid_num) {
            if (!devmm_ipc_pod_check_sdid_is_fault(node->attr.pid, wlist->sdid)) {
                sdid_list[i].sdid = wlist->sdid;
                *sdid_num = *sdid_num + 1;
                devmm_drv_debug("Valid sdid. (sdid[%u]=%u; sdid_num=%u)\n", i, sdid_list[i].sdid, *sdid_num);
            };
        }
        devmm_try_cond_resched(&stamp);
    }
    if (*sdid_num == 0) {
        devmm_kvfree(sdid_list);
        sdid_list = NULL;
    }
    return sdid_list;
}

static void devmm_destroy_ipc_node_sdid_list(struct ipc_node_sdid_list* sdid_list)
{
    if (sdid_list != NULL) {
        devmm_kvfree(sdid_list);
    }
}

static int devmm_ipc_pod_mem_repair_sync_msg_send(const char *name, ka_pid_t pid, u32 devid, u32 sdid)
{
    struct devmm_ipc_pod_msg_mem_repair *mem_repair_msg = NULL;
    struct devmm_ipc_pod_msg_data msg;
    int ret;

    mem_repair_msg = (struct devmm_ipc_pod_msg_mem_repair *)msg.payload;
    ret = memcpy_s(mem_repair_msg->name, DEVMM_IPC_MEM_NAME_SIZE, name, DEVMM_IPC_MEM_NAME_SIZE);
    if (ret != EOK) {
        return -ENOMEM;
    }
    mem_repair_msg->creator_pid = pid;
    msg.header.devid = devid;
    msg.header.cmdtype = DEVMM_IPC_POD_MSG_MEM_REPAIR;
    msg.header.result = 0;
    msg.header.valid = DEVMM_IPC_POD_MSG_SEND_MAGIC;

    ret = devmm_ipc_pod_msg_sync_send(devid, sdid, &msg, sizeof(struct devmm_ipc_pod_msg_data));
    if (ret != 0) {
        devmm_drv_err("Send msg fail. (ret=%d; devid=%u; sdid=%u)\n", ret, devid, sdid);
        return ret;
    }

    if (msg.header.result != 0) {
        devmm_drv_err("Msg handle fail. (result=%d; devid=%u; sdid=%u)\n", msg.header.result, devid, sdid);
        return -EFAULT;
    }

    return 0;
}

static int devmm_ipc_pod_mem_repair(struct devmm_ipc_node *node)
{
    struct ipc_node_sdid_list *sdid_list = NULL;
    u32 sdid_num, i;
    int ret = 0;

    sdid_list = devmm_create_ipc_node_sdid_list(node, &sdid_num);
    if (sdid_list == NULL) {
        return 0;
    }

    for (i = 0; i < sdid_num; i++) {
        ret = devmm_ipc_pod_mem_repair_sync_msg_send(node->attr.name, node->attr.pid, node->attr.inst.devid,
            sdid_list[i].sdid);
        if (ret != 0) {
            devmm_drv_err("Pod mem repair fail. (name=%s; pid=%u; devid=%u; va=0x%llx; len=%lu; i=%u; sdid=%u)\n",
                node->attr.name, node->attr.pid, node->attr.inst.devid, node->attr.vptr, node->attr.len,
                i, sdid_list[i].sdid);
            break;
        }
        devmm_drv_debug("Ipc pod mem repair succ. (name=%s; pid=%u; devid=%u; va=0x%llx; len=%lu; i=%u; sdid=%u)\n",
            node->attr.name, node->attr.pid, node->attr.inst.devid, node->attr.vptr, node->attr.len,
            i, sdid_list[i].sdid);
    }
    devmm_destroy_ipc_node_sdid_list(sdid_list);
    return ret;
}

static void devmm_ipc_pod_sync_destory_node(struct devmm_ipc_node *node)
{
    struct ipc_node_sdid_list *sdid_list = NULL;
    u32 sdid_num, i;
    int ret;

    sdid_list = devmm_create_ipc_node_sdid_list(node, &sdid_num);
    if (sdid_list == NULL) {
        return;
    }

    for (i = 0; i < sdid_num; i++) {
        ret = devmm_ipc_pod_destroy_sync_msg_send(node->attr.name, node->attr.pid, node->attr.inst.devid, sdid_list[i].sdid);
        if (ret != 0) {
            /* The log cannot be modified, because in the failure mode library. */
            devmm_drv_err_if((ret != -EPERM), "Ipc node destroy msg send fail. (ret=%d; name=%s)\n",
                ret, node->attr.name);
        }
    }
    devmm_destroy_ipc_node_sdid_list(sdid_list);
}

static u32 devmm_ipc_pod_destroy_msg_try_async_send(struct devmm_ipc_node *node, struct ipc_node_sdid_list*sdid_list,
    u32 sdid_num, struct ipc_node_destroy_stats *stats, u64 start_time)
{
    u32 i, send_busy_cnt, send_succ_cnt;
    u64 current_time;
    int ret;

    send_succ_cnt = 0;
    send_busy_cnt = 0;
try_send:
    for (i = 0; i < sdid_num; i++) {
        if ((sdid_list[i].send_status == IPC_POD_DESTROY_S2S_SEND_FAIL) ||
            (sdid_list[i].send_status == IPC_POD_DESTROY_S2S_SEND_SUCC)) {
            continue;
        }

        ret = devmm_ipc_pod_destroy_msg_async_send(node->attr.name, node->attr.pid, node->attr.inst.devid, sdid_list[i].sdid);
        if (ret != 0) {
            if (ret != -EBUSY) {
                /* The log cannot be modified, because in the failure mode library. */
                devmm_drv_err_if((ret != -EPERM), "Ipc node destroy msg send fail. (ret=%d; name=%s)\n",
                    ret, node->attr.name);
                sdid_list[i].send_status = IPC_POD_DESTROY_S2S_SEND_FAIL;
                stats->send_fail_cnt++;
            } else {
                sdid_list[i].send_status = IPC_POD_DESTROY_S2S_SEND_BUSY;
                send_busy_cnt++;
            }
        } else {
            sdid_list[i].send_status = IPC_POD_DESTROY_S2S_SEND_SUCC;
            stats->send_succ_cnt++;
            send_succ_cnt++;
        }
    }
    /* recv async send msg first */
    if (send_succ_cnt != 0) {
        return send_succ_cnt;
    }
    ka_mb();
    current_time = ka_system_ktime_get_ns();
    if ((send_busy_cnt != 0) && (current_time - start_time <= IPC_POD_DESTROY_MAX_TIME)) {
        ka_system_usleep_range(1000, 2000); /* 1000us to 2000us */
        devmm_drv_debug("Try send. (name=%s; pid=%u; devid=%u; send_busy_cnt=%u)\n",
            node->attr.name, node->attr.pid, node->attr.inst.devid, send_busy_cnt);
        send_busy_cnt = 0;
        goto try_send;
    }
    return 0;
}

static bool devmm_ipc_pod_destory_node_try_async_recv(struct devmm_ipc_node *node, struct ipc_node_sdid_list*sdid_list,
    u32 sdid_num, struct ipc_node_destroy_stats *stats, u64 start_time)
{
    u32 i, recv_busy_cnt, recv_finish_cnt;
    bool last_recv = false;
    u64 current_time;
    int ret;

    recv_finish_cnt = 0;
try_recv:
    recv_busy_cnt = 0;
    for (i = 0; i < sdid_num; i++) {
        if (sdid_list[i].send_status != IPC_POD_DESTROY_S2S_SEND_SUCC ||
            (sdid_list[i].recv_status == IPC_POD_DESTROY_S2S_RECV_FAIL ||
            sdid_list[i].recv_status == IPC_POD_DESTROY_S2S_RECV_SUCC)) {
            continue;
        }
        ret = devmm_ipc_pod_destroy_msg_async_recv(node->attr.inst.devid, sdid_list[i].sdid, last_recv);
        if (ret != 0) {
            if (ret != -EAGAIN) {
                sdid_list[i].recv_status = IPC_POD_DESTROY_S2S_RECV_FAIL;
                stats->recv_fail_cnt++;
                recv_finish_cnt++;
            } else {
                sdid_list[i].recv_status = IPC_POD_DESTROY_S2S_RECV_BUSY;
                recv_busy_cnt++;
            }
        } else {
            sdid_list[i].recv_status = IPC_POD_DESTROY_S2S_RECV_SUCC;
            stats->recv_succ_cnt++;
            recv_finish_cnt++;
        }
    }
    /* had recv all async send msg */
    if (recv_finish_cnt == stats->should_recv_cnt) {
        return true;
    }

    /* last_recv is busy */
    if (last_recv) {
        return false;
    }

    if (recv_busy_cnt != 0) {
        ka_system_usleep_range(1000, 2000); /* 1000us to 2000us */
        current_time = ka_system_ktime_get_ns();
        last_recv = (current_time - start_time <= IPC_POD_DESTROY_MAX_TIME) ? false : true;
        devmm_drv_debug("Try recv. (name=%s; pid=%u; devid=%u; recv_busy_cnt=%u; last_recv=%u)\n",
            node->attr.name, node->attr.pid, node->attr.inst.devid, recv_busy_cnt, last_recv);
        goto try_recv;
    }
    return false;
}

static void devmm_ipc_pod_destory_node_result_show(struct devmm_ipc_node *node, struct ipc_node_sdid_list*sdid_list,
    u32 sdid_num, struct ipc_node_destroy_stats *stats)
{
    u32 i;
    devmm_drv_debug("node info. (name=%s; pid=%u; devid=%u; sdid_num=%u)\n",
        node->attr.name, node->attr.pid, node->attr.inst.devid, sdid_num);
    devmm_drv_debug("destroy result show. (send_succ=%u; send_fail=%u; recv_succ=%u; recv_fail=%u)\n",
        stats->send_succ_cnt, stats->send_fail_cnt, stats->recv_succ_cnt, stats->recv_fail_cnt);
    for (i = 0; i < sdid_num; i++) {
        devmm_drv_debug("sdid destroy result. (sdid[%u]=%u; send_status=%u; recv_status=%u)\n",
            i, sdid_list[i].sdid, sdid_list[i].send_status, sdid_list[i].recv_status);
    }
}

void devmm_ipc_pod_async_destory_node(struct devmm_ipc_node *node)
{
    struct ipc_node_sdid_list *sdid_list = NULL;
    struct ipc_node_destroy_stats stats  = {0};
    u32 sdid_num, send_succ_cnt;
    u64 start_time;

    sdid_list = devmm_create_ipc_node_sdid_list(node, &sdid_num);
    if (sdid_list == NULL) {
        return;
    }
    start_time = ka_system_ktime_get_ns();
RETRY:
    send_succ_cnt = devmm_ipc_pod_destroy_msg_try_async_send(node, sdid_list, sdid_num, &stats, start_time);
    if (send_succ_cnt != 0) {
        stats.should_recv_cnt = send_succ_cnt;
        if (devmm_ipc_pod_destory_node_try_async_recv(node, sdid_list, sdid_num, &stats, start_time)) {
            goto RETRY;
        }
    }

    devmm_ipc_pod_destory_node_result_show(node, sdid_list, sdid_num, &stats);
    devmm_ipc_pod_update_fault_sdid(node->attr.pid, sdid_list, sdid_num);
    devmm_destroy_ipc_node_sdid_list(sdid_list);
}

static void devmm_ipc_pod_destory_node(struct devmm_ipc_node *node)
{
    if (node->need_async_recycle) {
        devmm_ipc_pod_async_destory_node(node);
    } else {
        devmm_ipc_pod_sync_destory_node(node);
    }
}

static int devmm_ipc_pod_update_node_attr(struct devmm_ipc_node_attr *attr)
{
    char name[DEVMM_IPC_MEM_NAME_SIZE] = {0};
    struct spod_info spod = {0};
    int ret, offset;

    ret = dbl_get_spod_info(attr->inst.devid, &spod);
    if (ka_unlikely(ret != 0)) {
        devmm_drv_err("Get pod fail. (ret=%d; devid=%u)\n", ret, attr->inst.devid);
        return ret;
    }

    attr->sdid = spod.sdid;
    offset = snprintf_s(name, DEVMM_IPC_MEM_NAME_SIZE, DEVMM_IPC_MEM_NAME_SIZE - 1, "%08x", spod.sdid);
    if (ka_unlikely(offset < 0)) {
        devmm_drv_err("Name create fail. (offset=%d; devid=%u; name=%s)\n", offset, attr->inst.devid, attr->name);
        return -ENOMEM;
    }
    name[offset] = '\0';
    ret = strcat_s(attr->name, DEVMM_IPC_MEM_NAME_SIZE, name);
    if (ka_unlikely(ret != EOK)) {
        devmm_drv_err("Update name fail. (ret=%d; attr->name=%s; name=%s)\n", ret, attr->name, name);
        return -ENOMEM;
    }

    devmm_drv_debug("Ipc node update attr succeed. (name=%s; sdid=%u)\n", name, attr->sdid);

    return 0;
}

struct devmm_ioctl_handlers_st hander = {
    .ioctl_handler = devmm_ioctl_ipc_pod_set_pid,
    .cmd_flag = DEVMM_CMD_NOT_SURPORT_VDEV | DEVMM_CMD_NOT_SURPORT_HOST_AGENT
};

int devmm_ipc_pod_init(void)
{
    struct devmm_ipc_node_ops node_ops;
    int ret;

    ret = devmm_ioctl_handler_register(_KA_IOC_NR(DEVMM_SVM_IPC_MEM_SET_PID_POD), hander);
    if (ret != 0) {
        devmm_drv_err("Ioctl handler register fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_register_s2s_msg_proc_func(DEVDRV_S2S_MSG_DEVMM, devmm_ipc_pod_msg_recv);
    if (ret != 0) {
        devmm_drv_err("Register s2s msg func fail. (ret=%d)\n", ret);
        return ret;
    }

    node_ops.destory_node = devmm_ipc_pod_destory_node;
    node_ops.update_node_attr = devmm_ipc_pod_update_node_attr;
    node_ops.is_local_pod = devmm_ipc_pod_is_local_pod;
    node_ops.clear_fault_sdid = devmm_ipc_pod_clear_fault_sdid;
    node_ops.pod_mem_repair = devmm_ipc_pod_mem_repair;

    devmm_ipc_node_ops_register(&node_ops);
    devmm_drv_info("Ipc pod Init\n");
    return DRV_ERROR_NONE;
}
DECLAER_FEATURE_AUTO_INIT(devmm_ipc_pod_init, FEATURE_LOADER_STAGE_5);

void devmm_ipc_pod_uninit(void)
{
    (void)devdrv_unregister_s2s_msg_proc_func(DEVDRV_S2S_MSG_DEVMM);
    devmm_drv_info("Ipc pod Uninit\n");
}
DECLAER_FEATURE_AUTO_UNINIT(devmm_ipc_pod_uninit, FEATURE_LOADER_STAGE_5);

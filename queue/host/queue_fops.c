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
#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_compiler_pub.h"
#include "ka_pci_pub.h"

#include "event_sched_inner.h"
#include "ascend_kernel_hal.h"
#include "ascend_hal_error.h"
#include "ascend_hal_define.h"
#include "davinci_api.h"

#include "queue_module.h"
#include "queue_fops.h"
#include "queue_msg.h"
#include "queue_channel.h"
#include "queue_dma.h"
#include "queue_context.h"
#include "queue_ctx_private.h"
#include "queue_proc_fs.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
STATIC const ka_pci_device_id_t g_queue_driver_tbl[] =   {{ KA_PCI_VDEVICE(HUAWEI, 0xd100), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd105), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xa126), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd500), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd501), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd802), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd803), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd804), 0 },
                                                          { KA_PCI_VDEVICE(HUAWEI, 0xd805), 0 },
                                                          { DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500,
                                                            KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                          { 0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                          { 0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                          { 0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                          { 0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                          { 0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                          { 0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0 },
                                                          {}};
KA_MODULE_DEVICE_TABLE(pci, g_queue_driver_tbl);

STATIC ka_atomic64_t queue_serial_num = KA_BASE_ATOMIC64_INIT(0);

int queue_drv_open(ka_inode_t *inode, ka_file_t *file)
{
    struct queue_context *ctx = NULL;

    ctx = queue_context_init(QUEUE_CURRENT_TGID);
    if (ctx == NULL) {
        queue_err("Queue context init failed.\n");
        return -EEXIST;
    }

    queue_proc_fs_add_process(ctx);

    ka_fs_set_file_private_data(file, ctx);

    queue_run_info("Open success. (pid=%d; thread pid=%d)\n", QUEUE_CURRENT_TGID, QUEUE_CURRENT_PID);

    return 0;
}

int queue_drv_release(ka_inode_t *inode, ka_file_t *file)
{
    struct queue_context *context = (struct queue_context *)ka_fs_get_file_private_data(file);
    if (context == NULL) {
        queue_err("context is NULL.\n");
        return -EINVAL;
    }

    queue_proc_fs_del_process(context);
    queue_context_uninit(context);

    ka_fs_set_file_private_data(file, NULL);

    queue_info("Release success. (pid=%d; thread_pid=%d)\n", QUEUE_CURRENT_TGID, QUEUE_CURRENT_PID);
    return 0;
}

STATIC long queue_drv_host_init(ka_file_t *filep, struct queue_ioctl_host_common_op *para)
{
    struct queue_context *context = filep->private_data;
    struct context_private_data *ctx_private = NULL;
    int hdc_session;
    long ret;

    ctx_private = (struct context_private_data *)context->private_data;
    if (ctx_private->hdc_session[para->devid] >= 0) {
        queue_err("Repeat hdc connect. (devid=%u)\n", para->devid);
        return -EEXIST;
    }
    /* hdc init */
    ret = hdcdrv_kernel_connect((int)para->devid, QUEUE_HDC_SERVICE_TYPE, &hdc_session, NULL);
    if (ret != 0) {
        queue_err("Hdcdrv_kernel_connect failed. (ret=%ld; devid=%u)\n", ret, para->devid);
        return ret;
    }

    ctx_private->hdc_session[para->devid] = hdc_session;
    queue_run_info("Hdc channel init succ. (devid=%u; hdc_session=%d)\n", para->devid, hdc_session);

    return 0;
}

STATIC long queue_drv_host_uninit(ka_file_t *filep, struct queue_ioctl_host_common_op *para)
{
#ifndef EMU_ST
    struct queue_context *context = filep->private_data;
    struct context_private_data *ctx_private = NULL;
    long ret;

    ctx_private = (struct context_private_data *)context->private_data;
    if (ctx_private->hdc_session[para->devid] < 0) {
        queue_run_info("Hdc session is not exist. (devid=%u)\n", para->devid);
        return 0;
    }

    /* hdc uninit */
    ret = hdcdrv_kernel_close(ctx_private->hdc_session[para->devid], NULL);
    if (ret != 0) {
        queue_err("Hdcdrv_kernel_close failed. (ret=%ld; devid=%u)\n", ret, para->devid);
    }
    queue_run_info("Hdcdrv_kernel_close. (devid=%u; session=%d)\n", para->devid, ctx_private->hdc_session[para->devid]);
    ctx_private->hdc_session[para->devid] = -1;

    return ret;
#else
    return 0;
#endif
}

STATIC int queue_check_vector(struct buff_iovec *vector, unsigned int count)
{
    unsigned long stamp = ka_jiffies;
    u32 i;

    if ((vector->context_base == NULL && vector->context_len != 0) ||
        (vector->context_base != NULL && vector->context_len == 0)) {
        queue_err("ctx_addr(0x%pK) not match ctx_addr_len(%llu).\n",
            (void *)(uintptr_t)vector->context_base, vector->context_len);
        return -EINVAL;
    }

    if (vector->count != count) {
        queue_err("count is not equal vector_count(%u), count(%u).\n",
            vector->count, count);
        return -EINVAL;
    }

    for (i = 0; i < vector->count; i++) {
        if ((vector->ptr[i].iovec_base == NULL) || (vector->ptr[i].len == 0)) {
            queue_err("addr(0x%pK) is NULL or len(%llu) is zero, index=%d, cnt=%u.\n",
                (void *)(uintptr_t)vector->ptr[i].iovec_base, vector->ptr[i].len, i, vector->count);
            return -EINVAL;
        }
        queue_try_cond_resched(&stamp);
    }

    return 0;
}

STATIC struct buff_iovec *queue_get_vector(struct queue_ioctl_enqueue *para)
{
    struct buff_iovec *vector = NULL;
    u64 vector_len;
    int ret;

    vector_len = (u64)sizeof(struct buff_iovec) + (u64)para->iovec_count * sizeof(struct iovec_info);
    vector = (struct buff_iovec *)queue_kvalloc(vector_len, 0);
    if (vector == NULL) {
        queue_err("kmalloc %llu failed.\n", vector_len);
        return NULL;
    }

    if (ka_base_copy_from_user(vector, para->vector, vector_len) != 0) {
        queue_err("copy_from_user failed.\n");
        queue_kvfree(vector);
        return NULL;
    }

    ret = queue_check_vector(vector, para->iovec_count);
    if (ret != 0) {
        queue_err("check vector failed, ret=%d.", ret);
        queue_kvfree(vector);
        return NULL;
    }

    return vector;
}

STATIC void queue_put_vector(struct buff_iovec *vector)
{
    queue_kvfree(vector);
}

int queue_wakeup_enqueue(struct queue_context *context, u64 que_chan_addr)
{
    struct context_private_data *ctx_private = NULL;
    struct queue_chan *que_chan = NULL;

    ctx_private = (struct context_private_data *)context->private_data;
    ka_task_spin_lock_bh(&ctx_private->lock);
    ka_list_for_each_entry(que_chan, &ctx_private->node_list_head, list) {
        if ((u64)(uintptr_t)que_chan == que_chan_addr) {
            /* wake up queue enqueue ioctl */
            queue_chan_wake_up(que_chan);
            ka_task_spin_unlock_bh(&ctx_private->lock);
            return 0;
        }
    }
    ka_task_spin_unlock_bh(&ctx_private->lock);

    return DRV_ERROR_NOT_EXIST;
}

static inline void queue_add_que_chan(struct context_private_data *ctx_private, struct queue_chan *que_chan)
{
    ka_task_spin_lock_bh(&ctx_private->lock);
    ka_list_add_tail(&que_chan->list, &ctx_private->node_list_head);
    ka_task_spin_unlock_bh(&ctx_private->lock);
}

static inline void queue_del_que_chan(struct context_private_data *ctx_private, struct queue_chan *que_chan)
{
    ka_task_spin_lock_bh(&ctx_private->lock);
    ka_list_del(&que_chan->list);
    ka_task_spin_unlock_bh(&ctx_private->lock);
}

STATIC void queue_init_host_qid_status(struct queue_context *context, struct queue_qid_status *qid_status,
    struct queue_ioctl_enqueue *para, u64 serial_num, u64 mem_size)
{
    queue_set_qid_status_serial_num(qid_status, serial_num);
    queue_proc_fs_add_qid(qid_status, context->entry);
    queue_set_qid_status_mem_size(qid_status, mem_size);
}

static int queue_para_check(struct queue_ioctl_enqueue *para)
{
    if (para->event_info.msg == NULL || para->event_info.msg_len == 0 ||
        para->event_info.msg_len > EVENT_MAX_MSG_LEN) {
        queue_err("Event msg invalid. (msg_len=%u)\n", para->event_info.msg_len);
        return -EINVAL;
    }

    if ((para->vector == NULL) || (para->iovec_count > QUEUE_MAX_IOVEC_NUM)) {
        queue_err("Vector invalid. (iovec_count=%u)\n", para->iovec_count);
        return -EINVAL;
    }

    if (para->type >= QUEUE_TYPE_MAX) {
        queue_err("Invalid memory type. (type=%u)\n", para->type);
        return -EINVAL;
    }

    if (para->qid >= MAX_SURPORT_QUEUE_NUM) {
        queue_err("Invalid qid. (qid=%u)\n", para->qid);
        return -EINVAL;
    }
    return 0;
}

static int queue_hdc_msg_send(void *data, size_t size, void *priv, int time_out)
{
    int hdc_session = (int)(uintptr_t)priv;
    int ret;

    ret = (int)hdcdrv_kernel_send_timeout(hdc_session, NULL, data, (int)size, time_out);
    if (ret != 0) {
        queue_err("Hdc msg send fail. (hdc_session=%d; size=%ld)\n", hdc_session, size);
    }
    return ret;
}

#ifndef EMU_ST
STATIC int queue_get_host_phy_mach_flag(u32 devid, u32 *host_flag)
{
    static bool get_flag = false;
    static u32 get_host_flag;
    int ret;

    if (get_flag == false) {
        ret = devdrv_get_host_phy_mach_flag(devid, &get_host_flag);
        if (ret != 0) {
            queue_err("Get host physics flag failed.(devid=%u;ret=%d).\n", devid, ret);
            return ret;
        }
        get_flag = true;
    }
    *host_flag = get_host_flag;
    return 0;
}

bool queue_is_hccs_vm_through_scene(u32 dev_id)
{
    u32 host_flag;
    int ret, conn_protocol;

    ret = queue_get_host_phy_mach_flag(dev_id, &host_flag);
    if (ret != 0) {
        queue_err("get host type fail. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return false;
    }
    conn_protocol = devdrv_get_connect_protocol(dev_id);
    if (((host_flag == DEVDRV_VIRT_PASS_THROUGH_MACH_FLAG) || devdrv_is_mdev_vm_boot_mode(dev_id)) &&
        (conn_protocol == CONNECT_PROTOCOL_HCCS)) {
        return true;
    }
    return false;
}
#endif

static void queue_chan_attr_pack(struct queue_ioctl_enqueue *para, int hdc_session, u64 serial_num, ka_device_t *dev,
    struct queue_chan_attr *attr)
{
    attr->msg_type = QUEUE_DATA_MSG;
    attr->memory_type = para->type;
    attr->devid = para->devid;
    attr->host_pid = QUEUE_CURRENT_TGID;
    attr->dev = dev;
    attr->serial_num = serial_num;
    attr->qid = para->qid;
    attr->event_info = para->event_info;
    attr->msg = para->event_info.msg;
    attr->msg_len = (size_t)para->event_info.msg_len;
    attr->priv = (void *)(uintptr_t)hdc_session;
    attr->send = queue_hdc_msg_send;
    attr->loc_passid = 0;
    attr->hccs_vm_flag = queue_is_hccs_vm_through_scene(para->devid);
}

static int queue_drv_vector_add(struct queue_chan *que_chan, struct queue_ioctl_enqueue *para,
    struct buff_iovec *vector)
{
    bool dma_flag = (para->type == QUEUE_BUFF) ? true : false;
    struct queue_chan_iovec iovec;
    unsigned long stamp = ka_jiffies;
    int ret;
    u32 i;

    iovec.va = (u64)(uintptr_t)vector->context_base;
    iovec.len = vector->context_len;
    iovec.dma_flag = true;
    ret = queue_chan_iovec_add(que_chan, &iovec);
    if (ka_unlikely(ret != 0)) {
        queue_err("Add ctx iovec fail. (ret=%d)\n", ret);
        return ret;
    }
    for (i = 0; i < vector->count; i++) {
        iovec.va = (u64)(uintptr_t)vector->ptr[i].iovec_base;
        iovec.len = vector->ptr[i].len;
        iovec.dma_flag = dma_flag;
        ret = queue_chan_iovec_add(que_chan, &iovec);
        if (ka_unlikely(ret != 0)) {
            queue_err("Add data iovec fail. (ret=%d; i=%u; count=%u)\n", ret, i, vector->count);
            return ret;
        }
        queue_try_cond_resched(&stamp);
    }
    return 0;
}

static struct queue_chan *queue_drv_que_chan_create(struct context_private_data *ctx_private,
    struct queue_ioctl_enqueue *para, u64 serial_num, ka_device_t *dev)
{
    struct queue_chan *que_chan = NULL;
    struct queue_chan_attr attr = {0};
    int hdc_session;
    int ret;

    hdc_session = ctx_private->hdc_session[para->devid];
    queue_chan_attr_pack(para, hdc_session, serial_num, dev, &attr);
    que_chan = queue_chan_create(&attr);
    if (que_chan == NULL) {
        queue_err("Que chan inst create fail. (devid=%u; qid=%u)\n", para->devid, para->qid);
        return NULL;
    }
    ret = queue_chan_dma_create(que_chan, para->iovec_count + 1);
    if (ret != 0) {
        queue_chan_destroy(que_chan);
        queue_err("Que chan dma create fail. (ret=%d; devid=%u; qid=%u)\n", ret, para->devid, para->qid);
        return NULL;
    }
    return que_chan;
}

STATIC long queue_drv_enqueue(ka_file_t *filep, struct queue_ioctl_enqueue *para, ka_device_t *dev)
{
    struct queue_context *context = filep->private_data;
    struct context_private_data *ctx_private = NULL;
    struct queue_qid_status *qid_status = NULL;
    struct queue_chan *que_chan = NULL;
    struct buff_iovec *vector = NULL;
    u64 mem_size = 0;
    int hdc_session;
    u64 serial_num;
    int ret;

    ctx_private = (struct context_private_data *)context->private_data;
    hdc_session = ctx_private->hdc_session[para->devid];
    if (hdc_session < 0) {
        queue_err("The hdc_session is not initialized, enqueue or dequeue is not allowed. (session=%d, devid=%u)\n",
            hdc_session, para->devid);
        return -EINVAL;
    }
    ret = queue_para_check(para);
    if (ret != 0) {
        return ret;
    }
    serial_num = (u64)ka_base_atomic64_inc_return(&queue_serial_num);
    /* qid_status may be NULL , If you want to use it directly, please judge the null pointer */
    qid_status = queue_create_or_get_exit_qid_status(context, para->qid);
    que_chan = queue_drv_que_chan_create(ctx_private, para, serial_num, dev);
    if (que_chan == NULL) {
        queue_err("Que chan inst create fail.\n");
        return -ENOMEM;
    }

    vector = queue_get_vector(para);
    if (vector == NULL) {
        queue_chan_destroy(que_chan);
        queue_err("Get vector failed.\n");
        return -EFAULT;
    }
    queue_set_qid_status_subevent_id(qid_status, para->event_info.subevent_id);
    queue_set_qid_status_timestamp(qid_status, HOST_START_MAKE_DMA_LIST);
    ret = queue_drv_vector_add(que_chan, para, vector);
    if (ret != 0) {
        queue_err("Vector add fail. (ret=%d; devid=%u)\n", ret, para->devid);
        goto err_que_chan_vec_add;
    }
    queue_set_qid_status_timestamp(qid_status, HOST_END_MAKE_DMA_LIST);

    /* Get total mem_size for recording */
    ret = queue_chan_get_iovec_size(que_chan, QUEUE_DMA_LOCAL, &mem_size);
    if (ret == 0) {
        queue_init_host_qid_status(context, qid_status, para, serial_num, mem_size);
    }
    queue_add_que_chan(ctx_private, que_chan);
    ret = queue_chan_send(que_chan, para->time_out);
    if (ret != 0) {
        /*
         * error code DRV_ERROR_SEND_MESG will transmit to user when hdc send failed,
         * so if you want to modify the return value, please pay attention to check.
         */
        queue_err("Que chan send fail. (ret=%d)\n", ret);
        goto err_que_chan_send;
    }
    queue_set_qid_status_timestamp(qid_status, HOST_END_HDC_SNED);

    ret = queue_chan_wait(que_chan, QUEUE_HOST_WAIT_MAX_TIME);
    /*
    * error code DRV_ERROR_WAIT_TIMEOUT will transmit to user when remote reply timeout,
    * it may be that the event was not successfully published to the cp process on the device side.
    * if you want to modify the return value, please pay attention to check.
    */
    ret = (ret != 0) ? DRV_ERROR_WAIT_TIMEOUT : 0;
    queue_set_qid_status_timestamp(qid_status, HOST_END_WAIT_REPLY);
err_que_chan_send:
    queue_del_que_chan(ctx_private, que_chan);
err_que_chan_vec_add:
    queue_put_vector(vector);
    queue_chan_destroy(que_chan);
    if (ret == 0) {
        queue_set_qid_status_timestamp(qid_status, HOST_FINISH_QUEUE_MSG);
    }

    return ret;
}

STATIC long (*g_queue_host_common_op[QUEUE_OP_MAX])
    (ka_file_t *filep, struct queue_ioctl_host_common_op *para) = {
        [QUEUE_INIT] = queue_drv_host_init,
        [QUEUE_UNINIT] = queue_drv_host_uninit,
};

STATIC long queue_fop_host_common_op(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct queue_ioctl_host_common_op para;
    u32 phy_devid, vfid;
    int ret;

    if (_KA_IOC_SIZE(cmd) != sizeof(struct queue_ioctl_host_common_op)) {
        queue_err("cmd(0x%x) size not valid.\n", cmd);
        return -ENOTTY;
    }

    if (ka_base_copy_from_user(&para, (void *)(uintptr_t)arg, _KA_IOC_SIZE(cmd)) != 0) {
        queue_err("copy_from_user fail cmd(0x%x)\n", cmd);
        return -EFAULT;
    }

    /* convert devid */
    ret = devdrv_manager_container_logical_id_to_physical_id(para.devid, &phy_devid, &vfid);
    if (ret != 0) {
        queue_err("convert devid(%u) failed, ret=%d.\n", para.devid, ret);
        return ret;
    }
    if ((phy_devid >= MAX_DEVICE) || (vfid != 0) || (para.op_flag >= QUEUE_OP_MAX) ||
        (g_queue_host_common_op[para.op_flag] == NULL)) {
        queue_err("invalid devid(%u).\n", phy_devid);
        return -EINVAL;
    }
    para.devid = phy_devid;

    return g_queue_host_common_op[para.op_flag](filep, &para);
}

STATIC long queue_fop_enqueue(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct queue_ioctl_enqueue para;
    ka_device_t *dev = NULL;
    u32 phy_devid, vfid;
    long ret;

    if (_KA_IOC_SIZE(cmd) != sizeof(struct queue_ioctl_enqueue)) {
        queue_err("cmd(0x%x) size not valid.\n", cmd);
        return -ENOTTY;
    }

    if (ka_base_copy_from_user(&para, (void *)(uintptr_t)arg, _KA_IOC_SIZE(cmd)) != 0) {
        queue_err("copy_from_user fail cmd(0x%x)\n", cmd);
        return -EFAULT;
    }

    /* convert devid */
    ret = devdrv_manager_container_logical_id_to_physical_id(para.devid, &phy_devid, &vfid);
    if (ret != 0) {
        queue_err("convert devid(%u) failed, ret=%ld.\n", para.devid, ret);
        return -EINVAL;
    }
    if ((phy_devid >= MAX_DEVICE)  || (vfid != 0) || (para.qid >= MAX_SURPORT_QUEUE_NUM)) {
        queue_err("Invalid devid, vfid or qid. (phy_devid=%u; vfid=%u; qid=%u)\n", phy_devid, vfid, para.qid);
        return -EINVAL;
    }
    para.devid = phy_devid;

    dev = queue_get_device(phy_devid);
    if (dev == NULL) {
        queue_err("Get dev failed. (devid=%u)\n", phy_devid);
        return -EINVAL;
    }
    ret = queue_drv_enqueue(filep, &para, dev);
    queue_put_device(phy_devid);

    return ret;
}

STATIC struct queue_chan_ctrl_msg_mng *queue_ctrl_msg_mng_create(struct queue_ioctl_ctrl_msg_send *para)
{
    struct queue_chan_ctrl_msg_mng *ctrl_msg_mng = NULL;
    long ret;

    ctrl_msg_mng = (struct queue_chan_ctrl_msg_mng *)queue_kvalloc(sizeof(struct queue_chan_ctrl_msg_mng), 0);
    if (ctrl_msg_mng == NULL) {
        queue_err("Kvalloc failed. (devid=%u)\n", para->devid);
        return NULL;
    }

    ret = (long)ka_base_copy_from_user(ctrl_msg_mng->head.msg, para->event_info.msg, para->event_info.msg_len);
    if (ret != 0) {
        queue_err("Msg copy_from_user failed. (ret=%ld)\n", ret);
        queue_kvfree(ctrl_msg_mng);
        return NULL;
    }

    ret = (long)ka_base_copy_from_user(ctrl_msg_mng->ctrl_data, para->ctrl_data_addr, para->ctrl_data_len);
    if (ret != 0) {
        queue_err("Ctrl data copy_from_user failed. (ret=%ld)\n", ret);
        queue_kvfree(ctrl_msg_mng);
        return NULL;
    }

    ctrl_msg_mng->head.msg_type = QUEUE_CTRL_MSG;
    ctrl_msg_mng->head.devid = para->devid;
    ctrl_msg_mng->head.hostpid = QUEUE_CURRENT_TGID;
    ctrl_msg_mng->head.event_info = para->event_info;
    ctrl_msg_mng->ctrl_data_len = para->ctrl_data_len;
    ctrl_msg_mng->host_timestamp = para->host_timestamp;

    return ctrl_msg_mng;
}

STATIC void queue_ctrl_msg_mng_destroy(struct queue_chan_ctrl_msg_mng *ctrl_msg_mng)
{
    if (ctrl_msg_mng != NULL) {
        queue_kvfree(ctrl_msg_mng);
    }
}

STATIC long queue_ctrl_msg_send_by_hdc(ka_file_t *filep, struct queue_ioctl_ctrl_msg_send *para)
{
    struct queue_context *context = filep->private_data;
    struct context_private_data *ctx_private = (struct context_private_data *)context->private_data;
    struct queue_chan_ctrl_msg_mng *ctrl_msg_mng = NULL;
    int hdc_session;
    long ret;

    hdc_session = ctx_private->hdc_session[para->devid];
    if (hdc_session < 0) {
        queue_err("The hdc_session is not initialized, ctrl_msg_send is not allowed. (session=%d; devid=%u)\n",
            hdc_session, para->devid);
        return -EINVAL;
    }

    ctrl_msg_mng = queue_ctrl_msg_mng_create(para);
    if (ctrl_msg_mng == NULL) {
        queue_err("Ctrl msg mng create failed. (session=%d; devid=%u)\n", hdc_session, para->devid);
        return -EFAULT;
    }

    ret = hdcdrv_kernel_send(hdc_session, NULL, (void *)ctrl_msg_mng, sizeof(struct queue_chan_ctrl_msg_mng));
    queue_ctrl_msg_mng_destroy(ctrl_msg_mng);

    return ret;
}

STATIC long queue_ctrl_msg_send(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct queue_ioctl_ctrl_msg_send para;
    u32 phy_devid, vfid;
    long ret;

    if (_KA_IOC_SIZE(cmd) != sizeof(struct queue_ioctl_ctrl_msg_send)) {
        queue_err("Cmd size not valid. (cmd=0x%x)\n", cmd);
        return -ENOTTY;
    }

    if (ka_base_copy_from_user(&para, (void *)(uintptr_t)arg, _KA_IOC_SIZE(cmd)) != 0) {
        queue_err("Copy from user failed. (cmd=0x%x)\n", cmd);
        return -EFAULT;
    }

    /* convert devid */
    ret = devdrv_manager_container_logical_id_to_physical_id(para.devid, &phy_devid, &vfid);
    if (ret != 0) {
        queue_err("Convert devid failed. (devid=%u; ret=%ld).\n", para.devid, ret);
        return -EINVAL;
    }

    if ((phy_devid >= MAX_DEVICE) || (vfid != 0)) {
        queue_err("Invalid devid or vfid. (phy_devid=%u; vfid=%u)\n", phy_devid, vfid);
        return -EINVAL;
    }
    para.devid = phy_devid;

    if ((para.event_info.msg == NULL) || (para.event_info.msg_len == 0) ||
        (para.event_info.msg_len > EVENT_MAX_MSG_LEN)) {
        queue_err("Msg is NULL or len is invalid. (msg_is_null=%d; msg_len=%u)\n",
            para.event_info.msg == NULL, para.event_info.msg_len);
        return -EINVAL;
    }
    if ((para.ctrl_data_addr == NULL) || (para.ctrl_data_len == 0) || (para.ctrl_data_len > QUEUE_CTRL_MSG_DATA_LEN)) {
        queue_err("Ctrl_data addr or len is invalid. (ctrl_data_len=%u; max_len=%u)\n",
            para.ctrl_data_len, QUEUE_CTRL_MSG_DATA_LEN);
        return -EPERM;
    }

    return queue_ctrl_msg_send_by_hdc(filep, &para);
}

long (*const drv_queue_ioctl_handlers[QUEUE_CMD_MAX])
    (ka_file_t *filep, unsigned int cmd, unsigned long arg) = {
        [_KA_IOC_NR(QUEUE_HOST_COMMON_OP_CMD)] = queue_fop_host_common_op,
        [_KA_IOC_NR(QUEUE_ENQUEUE_CMD)] = queue_fop_enqueue,
        [_KA_IOC_NR(QUEUE_CTRL_MSG_SEND_CMD)] = queue_ctrl_msg_send,
};

static int queue_event_try_mcast(unsigned int devid, struct sched_published_event_info *event_info,
    struct sched_published_event_func *event_func)
{
    char msg[MSG_MAX_LEN];
    struct queue_event_msg_head *msg_head = (struct queue_event_msg_head *)&msg;
    struct sched_published_event event;
    int ret;

    if (!queue_is_mcast_event(event_info->subevent_id)) {
        return 0;
    }

    if ((event_info->msg == NULL) || (event_info->msg_len < sizeof(struct queue_event_msg_head))
        || (event_info->msg_len > MSG_MAX_LEN)) {
        queue_err("Msg is invalid. (devid=%u; subevent_id=%u; msg_len=%u)\n",
            devid, event_info->subevent_id, event_info->msg_len);
        return -EINVAL;
    }

    if (memcpy_s((void *)&msg, MSG_MAX_LEN, (void *)(uintptr_t)event_info->msg, event_info->msg_len) != 0) {
        queue_err("Copy from user fail. (devid=%u; subevent_id=%u; msg_len=%u)\n",
            devid, event_info->subevent_id, event_info->msg_len);
        return -EFAULT;
    }

    if (msg_head->comm.mcast_para.mcast_flag == 0) {
        return 0;
    }

    event.event_info = *event_info;
    event.event_func = *event_func;
    event.event_info.msg = msg;
    event.event_info.dst_engine = CCPU_DEVICE;
    event.event_info.gid = msg_head->comm.mcast_para.gid;
    ret = sched_submit_event(devid, &event);
    if (ret != 0) {
        /* Multicast failure does not affect the original process. */
        queue_warn("Mcast event failed. (ret=%d; devid=%u; subevent_id=%u; qid=%u; gid=%u; event_sn=%u)\n",
            ret, devid, event_info->subevent_id, msg_head->comm.qid,
            msg_head->comm.mcast_para.gid, msg_head->comm.mcast_para.event_sn);
    }

    return 0;
}

int queue_drv_module_init(const ka_file_operations_t *ops)
{
    int ret;

    /* register queue fops to davinci */
    ret = drv_davinci_register_sub_module(DAVINCI_QUEUE_SUB_MODULE_NAME, (void *)ops);
    if (ret != 0) {
        queue_err("register sub module fail, err(%d)\n", ret);
        return -ENODEV;
    }

    ret = queue_drv_msg_chan_init();
    if (ret != 0) {
        (void)drv_ascend_unregister_sub_module(DAVINCI_QUEUE_SUB_MODULE_NAME);
        queue_err("msg chan init failed, ret=%d.\n", ret);
        return ret;
    }

    ret = sched_register_event_pre_proc_handle(EVENT_DRV_MSG, queue_event_try_mcast);
    if (ret != 0) {
        queue_drv_msg_chan_uninit();
        (void)drv_ascend_unregister_sub_module(DAVINCI_QUEUE_SUB_MODULE_NAME);
        queue_err("msg chan init failed, ret=%d.\n", ret);
        return ret;
    }

    queue_proc_fs_init();
    queue_register_hdc_cb_func();

    queue_info("host queue module init success.\n");
    return 0;
}

void queue_drv_module_exit(void)
{
    int ret;

    queue_proc_fs_uninit();

    queue_unregister_hdc_cb_func();
    sched_unregister_event_pre_proc_handle(EVENT_DRV_MSG, queue_event_try_mcast);
    queue_drv_msg_chan_uninit();

    /* davinci unregister queue fops */
    ret = drv_ascend_unregister_sub_module(DAVINCI_QUEUE_SUB_MODULE_NAME);
    if (ret != 0) {
        queue_err("unregister sub module fail, err(%d)\n", ret);
        return;
    }

    queue_info("host queue module exit success.\n");
    return;
}


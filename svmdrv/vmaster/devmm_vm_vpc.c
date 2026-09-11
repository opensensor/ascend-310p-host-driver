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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "dbl/uda.h"
#include "devmm_vm_adapt.h"
#include "devmm_proc_info.h"
#include "devmm_channel.h"
#include "devmm_vpc_mng_msg_def.h"
#include "svm_vm_version_adapt.h"
#include "devmm_common.h"
#include "svm_dev_res_mng.h"
#include "svm_msg_client.h"
#include "devmm_vm_vpc.h"

struct devmm_vm_dev_info svm_vm_dev_info[DEVMM_MAX_DEVICE_NUM];
#define VPC_MSG_SEND_RETRY_TIME 100

static int devmm_vpc_msg_send(u32 devid, u32 vpc_type, struct vmng_tx_msg_proc_info *tx_info)
{
    u32 retry_time = VPC_MSG_SEND_RETRY_TIME;
    int ret;

    do {
        ret = vpc_msg_send(devid, VPC_VM_FID, vpc_type, tx_info, VPC_DEFAULT_TIMEOUT);
        if (ret != -ENOSPC) {
            break;
        }
        ka_system_usleep_range(100, 200);  /* 100us ~ 200us */
        retry_time--;
    } while (retry_time != 0);

    return ret;
}

bool devmm_device_is_ready(u32 dev_id)
{
    if (dev_id < DEVMM_MAX_DEVICE_NUM) {
        return (svm_vm_dev_info[dev_id].valid == DEVMM_VALID);
    } else {
        return 0;
    }
}

ka_device_t *devmm_device_get_by_devid(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        devmm_drv_err("Dev_id is invalid. (dev_id=%d)\n", dev_id);
        return NULL;
    }

    if (svm_vm_dev_info[dev_id].valid == DEVMM_INVALID) {
        devmm_drv_err("Dev_id has not been inited. (dev_id=%d)\n", dev_id);
        return NULL;
    }

    return svm_vm_dev_info[dev_id].dev;
}

void devmm_device_put_by_devid(u32 dev_id)
{
    return;
}

STATIC int devmm_vm_pm_sync(u32 dev_id)
{
    u32 len = (u32)sizeof(struct devmm_vpc_mng_msg_sync);
    struct devmm_vpc_mng_msg_sync sync = {{{0}}};
    int ret;

    sync.head.msg_id = DEVMM_VPC_SYNC;
    sync.head.dev_id = dev_id;
    ret = devmm_vm_vpc_mng_msg_send(&sync, len, len);
    if (ret != 0) {
        devmm_drv_err("Synchronize failed. (dev_id=%d; ret=%d)\n", dev_id, ret);
        return ret;
    }

    svm_vm_dev_info[dev_id].pm_dev_id = sync.pm_dev_id;
    svm_vm_dev_info[dev_id].pm_fid = sync.pm_fid;

    if (devmm_is_page_size_inited() == false) {
        devmm_svm_set_host_pgsf(KA_MM_PAGE_SHIFT);
        devmm_svm_set_host_hpgsf(KA_MM_HPAGE_SHIFT);
        devmm_svm_set_device_pgsf(sync.device_page_shift);
        devmm_svm_set_device_hpgsf(sync.device_hpage_shift);

        devmm_chan_set_host_device_page_size();
        devmm_drv_info("Init information. (page_shift_host=%d; device_page_shift=%d; host_huge=%d; device_huge=%d)\n",
            KA_MM_PAGE_SHIFT, sync.device_page_shift, KA_MM_HPAGE_SHIFT, sync.device_hpage_shift);
    }

    devmm_svm->device_info.ddr_size[dev_id][0] = sync.dev_mem[DEVMM_EXCHANGE_DDR_SIZE];
    devmm_svm->device_info.p2p_ddr_size[dev_id][0] = sync.dev_mem_p2p[DEVMM_EXCHANGE_DDR_SIZE];
    devmm_svm->device_info.hbm_size[dev_id][0] = sync.dev_mem[DEVMM_EXCHANGE_HBM_SIZE];
    devmm_svm->device_info.p2p_hbm_size[dev_id][0] = sync.dev_mem_p2p[DEVMM_EXCHANGE_HBM_SIZE];
    ka_base_atomic64_add(devmm_svm->device_info.ddr_size[dev_id][0], &devmm_svm->device_info.total_ddr);
    ka_base_atomic64_add(devmm_svm->device_info.hbm_size[dev_id][0], &devmm_svm->device_info.total_hbm);
    devmm_svm->dev_capability[dev_id].dvpp_memsize =
        sync.dvpp_memsize != 0 ? sync.dvpp_memsize : DEVMM_MAX_HEAP_MEM_FOR_DVPP_4G;
    devmm_svm->dev_capability[dev_id].feature_phycial_address = (u32)sync.feature_phycial_address;
    devmm_svm->dev_capability[dev_id].feature_dev_mem_map_host = 0;
    devmm_svm->dev_capability[dev_id].feature_bar_mem = 0;
    devmm_svm->dev_capability[dev_id].feature_bar_huge_mem = 0;
    devmm_svm->dev_capability[dev_id].feature_dev_read_only = 0;
    devmm_svm->dev_capability[dev_id].double_pgtable_offset = sync.double_pgtable_offset;
    devmm_svm->dev_capability[dev_id].feature_giant_page = 0;
    devmm_svm->dev_capability[dev_id].feature_remote_mmap = 0;
    devmm_svm->dev_capability[dev_id].feature_shmem_repair = 0;
    devmm_svm->dev_capability[dev_id].feature_mem_host_uva = 0;

    devmm_drv_info("Information. (dev_id=%u; ddr_size=%llu; p2p_ddr_size=%llu; hbm_size=%llu; p2p_hbm_size=%llu; "
        "host_ddr=%llu; total_ddr=%llu; total_hbm=%llu; dvpp_size=%llu; feature_phycial_address=%u; "
        "double_pgtable_offset=0x%llx)\n",
        dev_id, devmm_svm->device_info.ddr_size[dev_id][0], devmm_svm->device_info.p2p_ddr_size[dev_id][0],
        devmm_svm->device_info.hbm_size[dev_id][0],
        devmm_svm->device_info.p2p_hbm_size[dev_id][0], devmm_svm->device_info.host_ddr,
        (u64)ka_base_atomic64_read(&devmm_svm->device_info.total_ddr),
        (u64)ka_base_atomic64_read(&devmm_svm->device_info.total_hbm),
        devmm_svm->dev_capability[dev_id].dvpp_memsize,
        devmm_svm->dev_capability[dev_id].feature_phycial_address,
        devmm_svm->dev_capability[dev_id].double_pgtable_offset);

    devmm_drv_info("Synchronize succeeded. (dev_id=%d; pm_dev_id=%d; fid=%d)\n", dev_id, sync.pm_dev_id, sync.pm_fid);

    return 0;
}

int devmm_chan_msg_send(void *msg, unsigned int len, unsigned int out_len)
{
    struct vmng_tx_msg_proc_info info = {.data = msg, .in_data_len = len, .out_data_len = out_len, .real_out_len = 0};
    struct devmm_chan_msg_head *msg_head = (struct devmm_chan_msg_head *)msg;
    struct svm_ver_adapt_msg_info new_msg_info = {0};
    void *old_msg = NULL;
    u32 old_msg_len, old_out_len;
    u32 did = msg_head->dev_id;
    int ret;

    if ((did >= DEVMM_MAX_DEVICE_NUM) || (devmm_device_is_ready(did) == false)) {
        devmm_drv_err("Dev_id has not been init. (dev_id=%u)\n", did);
        return -EINVAL;
    }
#ifndef EMU_ST
    ret = devmm_vm_tx_version_adapt_pre_proc(info.data, info.in_data_len, &new_msg_info);
    if (ret != 0) {
        return ret;
    }

    old_msg = info.data;
    old_msg_len = info.in_data_len;
    old_out_len = info.out_data_len;
    if (new_msg_info.msg != NULL) {
        info.data = new_msg_info.msg;
        info.in_data_len = (u32)new_msg_info.msg_len;
        info.out_data_len = (u32)new_msg_info.out_len;
    }
#endif

    ret = devmm_vpc_msg_send(did, VMNG_VPC_TYPE_DEVMM, &info);
    if (ret != 0) {
        devmm_drv_err("Vpc send failed. (ret=%d)\n", ret);
    } else {
#ifndef EMU_ST
        if (devmm_vm_is_need_return_head_result() && (((struct devmm_chan_msg_head *)info.data)->result != 0)) {
            ret = ((struct devmm_chan_msg_head *)info.data)->result;
            devmm_drv_no_err_if((ret == -ENOMEM), "Msg process return failed. (msg_id=%d; ret=%d)\n", msg_head->msg_id, ret);
        }
#endif
    }

#ifndef EMU_ST
    info.data = old_msg;
    info.in_data_len = old_msg_len;
    info.out_data_len = old_out_len;
    (void)devmm_vm_tx_version_adapt_post_proc(info.data, info.in_data_len, &new_msg_info);
#endif
    return ret;
}
EXPORT_SYMBOL_UNRELEASE(devmm_chan_msg_send);

int devmm_vm_vpc_mng_msg_send(void *msg, unsigned int len, unsigned int out_len)
{
    struct devmm_vpc_mng_msg_head *send_msg = (struct devmm_vpc_mng_msg_head *)msg;
    struct vmng_tx_msg_proc_info tx_info;
    u32 dev_id = send_msg->dev_id;
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        devmm_drv_err("Dev_id is invalid. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (svm_vm_dev_info[dev_id].valid == DEVMM_INVALID) {
        devmm_drv_err("Dev_id has not been init. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    tx_info.data = msg;
    tx_info.in_data_len = len;
    tx_info.out_data_len = out_len;
    tx_info.real_out_len = 0;
#ifndef EMU_ST
    ret = devmm_vm_mng_tx_version_adapt_pre_proc(tx_info.data);
    if (ret != 0) {
        return ret;
    }
#endif
    ret = devmm_vpc_msg_send(dev_id, VMNG_VPC_TYPE_DEVMM_MNG, &tx_info);
    if (ret != 0) {
        devmm_drv_err("Vpc send failed. (ret=%d)\n", ret);
        return ret;
    }
#ifndef EMU_ST
    ret = devmm_vm_mng_tx_version_adapt_post_proc(tx_info.data);
#endif
    return ret;
}
EXPORT_SYMBOL_UNRELEASE(devmm_vm_vpc_mng_msg_send);

#ifndef EMU_ST
STATIC int vdevmm_rx_vpc_msg_para_check(u32 dev_id, struct vmng_rx_msg_proc_info *proc_info)
{
    if ((proc_info == NULL) || (proc_info->real_out_len == NULL) || (proc_info->data == NULL)) {
        devmm_drv_err("Parameter is NULL. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    if (dev_id >= VMNG_PDEV_MAX) {
        devmm_drv_err("Dev_id is error. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    if (svm_vm_dev_info[dev_id].valid == DEVMM_INVALID) {
        devmm_drv_err("Dev_id has not been inited. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    return 0;
}
#endif

STATIC int vdevmma_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
#ifndef EMU_ST
    struct devmm_chan_msg_head *head = NULL;
    int ret;

    if (vdevmm_rx_vpc_msg_para_check(dev_id, proc_info) != 0) {
        devmm_drv_err("Vdevmmh vpc message parameter check failed.\n");
        return -EINVAL;
    }

    if (proc_info->in_data_len < sizeof(struct devmm_chan_msg_head)) {
        devmm_drv_err("Invalid in_data_len. (in_data_len=%u)\n", proc_info->in_data_len);
        return -EMSGSIZE;
    }

    head = (struct devmm_chan_msg_head *)proc_info->data;
    if (head->msg_id >= DEVMM_CHAN_MAX_ID) {
        devmm_drv_err("Msg_id is not surport. (dev_id=%d; fid=%d; pid=%d; msg_id=%d)\n", dev_id, fid,
            head->process_id.hostpid, head->msg_id);
        return -ENOMSG;
    }

    ret = devmm_vm_rx_version_adapt_pre_proc(proc_info->data);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_chan_msg_dispatch(proc_info->data, proc_info->in_data_len, proc_info->out_data_len,
        proc_info->real_out_len, &devmm_channel_msg_processes[0]);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_vm_rx_version_adapt_post_proc(proc_info->data);
    return ret;
#endif
}

static int (*devmm_vm_vpc_mng_msg_processes[DEVMM_VPC_MNG_MSG_MAX_ID])(struct vmng_rx_msg_proc_info *proc_info) = {
    [DEVMM_VPC_MNG_DEVICE_PAGE_FAULT] = devmm_vm_chan_page_fault_d2h_process,
};

STATIC int vdevmma_vpc_mng_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
#ifndef EMU_ST
    struct devmm_vpc_mng_msg_head *head = NULL;
    int ret;

    if (vdevmm_rx_vpc_msg_para_check(dev_id, proc_info) != 0) {
        devmm_drv_err("Vdevmmh vpc mng message parameter check failed.\n");
        return -EINVAL;
    }

    if (proc_info->in_data_len < sizeof(struct devmm_vpc_mng_msg_head)) {
        devmm_drv_err("Invalid in_data_len. (in_data_len=%u)\n", proc_info->in_data_len);
        return -EMSGSIZE;
    }

    head = (struct devmm_vpc_mng_msg_head *)proc_info->data;
    if ((head->msg_id >= DEVMM_VPC_MNG_MSG_MAX_ID) || (devmm_vm_vpc_mng_msg_processes[head->msg_id] == NULL)) {
        devmm_drv_err("Msg_id  is not surport. (dev_id=%d; fid=%d; pid=%d; msg_id=%d)\n", dev_id, fid,
            head->process_id.hostpid, head->msg_id);
        return -ENOMSG;
    }

    if (head->dev_id != dev_id) {
        devmm_drv_err("Dev_id is not match message dev_id. (msg_id=%d; dev_id=%d; head_dev_id=%d)\n",
            head->msg_id, dev_id, head->dev_id);
        return -EINVAL;
    }

    ret = devmm_vm_mng_rx_version_adapt_pre_proc(proc_info->data);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_vm_vpc_mng_msg_processes[head->msg_id](proc_info);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_vm_mng_rx_version_adapt_post_proc(proc_info->data);
    return ret;
#endif
}

struct vmng_vpc_client vdevmma_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_DEVMM,
    .init = NULL,
    .msg_recv = vdevmma_vpc_msg_recv,
};

struct vmng_vpc_client vdevmma_vpc_mng_client = {
    .vpc_type = VMNG_VPC_TYPE_DEVMM_MNG,
    .init = NULL,
    .msg_recv = vdevmma_vpc_mng_msg_recv,
};

STATIC int vdevmma_init_instance(u32 dev_id, ka_device_t *dev)
{
    struct svm_id_inst id_inst;
    int ret;

    if (svm_vm_dev_info[dev_id].valid == DEVMM_VALID) {
        devmm_drv_err("Dev_id has been inited. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    svm_id_inst_pack(&id_inst, dev_id, 0);
    ret = devmm_dev_res_mng_create(&id_inst, NULL);
    if (ret != 0) {
        return ret;
    }

    ret = vmnga_vpc_register_client(dev_id, &vdevmma_vpc_client);
    if (ret != 0) {
        devmm_drv_err("Register vpc message failed. (dev_id=%d; ret=%d)\n", dev_id, ret);
        devmm_dev_res_mng_destroy(&id_inst);
        return ret;
    }

    ret = vmnga_vpc_register_client(dev_id, &vdevmma_vpc_mng_client);
    if (ret != 0) {
        devmm_drv_err("Register vpc mng message failed. (dev_id=%d; ret=%d)\n", dev_id, ret);
        vmnga_vpc_unregister_client(dev_id, &vdevmma_vpc_client);
        devmm_dev_res_mng_destroy(&id_inst);
        return ret;
    }

    svm_vm_dev_info[dev_id].dev = dev;
    svm_vm_dev_info[dev_id].valid = DEVMM_VALID;

    devmm_vm_sync_version(dev_id);
    ret = devmm_vm_pm_sync(dev_id);
    if (ret != 0) {
        vmnga_vpc_unregister_client(dev_id, &vdevmma_vpc_mng_client);
        vmnga_vpc_unregister_client(dev_id, &vdevmma_vpc_client);
        devmm_dev_res_mng_destroy(&id_inst);
        svm_vm_dev_info[dev_id].dev = NULL;
        svm_vm_dev_info[dev_id].valid = DEVMM_INVALID;
        return ret;
    }

    devmm_drv_info("Vdevmma dev_id init instance succeeded. (dev_id=%u)\n", dev_id);

    return 0;
}

STATIC int vdevmma_uninit_instance(u32 dev_id)
{
    struct svm_id_inst id_inst;

    if (svm_vm_dev_info[dev_id].valid == DEVMM_INVALID) {
        devmm_drv_err("Dev_id has not been inited. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    vmnga_vpc_unregister_client(dev_id, &vdevmma_vpc_mng_client);
    vmnga_vpc_unregister_client(dev_id, &vdevmma_vpc_client);

    svm_vm_dev_info[dev_id].dev = NULL;
    svm_vm_dev_info[dev_id].valid = DEVMM_INVALID;

    svm_id_inst_pack(&id_inst, dev_id, 0);
    devmm_dev_res_mng_destroy(&id_inst);

    devmm_drv_info("Vdevmma dev_id uninit instance succeeded. (dev_id=%u)\n", dev_id);
    return 0;
}

#define SVM_REMOTE_NOTIFIER "svm_remote"

static int svm_remote_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = vdevmma_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = vdevmma_uninit_instance(udevid);
    }

    return ret;
}

int devmm_msg_chan_init(void)
{
    struct uda_dev_type type;
    int ret, i;

    for (i = 0; i < DEVMM_MAX_DEVICE_NUM; i++) {
        svm_vm_dev_info[i].valid = DEVMM_INVALID;
    }

    uda_davinci_remote_real_entity_type_pack(&type);
    ret = uda_notifier_register(SVM_REMOTE_NOTIFIER, &type, UDA_PRI2, svm_remote_notifier_func);
    if (ret != 0) {
        devmm_drv_err("Register notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    devmm_drv_info("Vdevmma init success.\n");

    return 0;
}

void devmm_msg_chan_uninit(void)
{
    struct uda_dev_type type;

    uda_davinci_remote_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(SVM_REMOTE_NOTIFIER, &type);
    devmm_drv_info("Vdevmma uninit succeeded.\n");
}

int devmm_register_ops_init(void)
{
    int ret;

    ret = devmm_msg_chan_init();
    if (ret != 0) {
        devmm_drv_err("Vdevmm register failed. (ret=%d)\n", ret);
        return ret;
    }

    devmm_drv_info("Vdevmm register success.\n");
    return 0;
}

void devmm_unregister_ops_uninit(void)
{
    devmm_msg_chan_uninit();
    devmm_drv_info("Vdevmm register uninit succeeded.\n");
}

// stub for hotreset
int devmm_add_pid_into_business(uint32_t devid, ka_pid_t pid)
{
    return 0;
}
// stub for hotreset
void devmm_remove_pid_from_business(uint32_t devid, ka_pid_t pid)
{
    return;
}

// stub for hotreset
void devmm_remove_pid_from_all_business(ka_pid_t pid)
{
    return;
}

// stub for device process status check
int devmm_chan_report_process_status_d2h(
    struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap, void *msg, u32 *ack_len)
{
    return 0;
}


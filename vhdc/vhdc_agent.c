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

#include "vhdc_agent.h"
#include "vhdc_core.h"
#include "vhdc_mem.h"

#include "dbl/uda.h"
#include "devdrv_manager_comm.h"

#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_pci_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"

struct vhdca_ctrl *g_vhdca_ctrl = NULL;

static const ka_pci_device_id_t g_vhdca_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd500), 0 },
    {}};
KA_MODULE_DEVICE_TABLE(pci, g_vhdca_tbl);

ka_vm_operations_struct_t vhdca_vm_ops_managed;

STATIC int vhdca_vm_mremap(ka_vm_area_struct_t *vma)
{
    hdcdrv_err("mremap not supported\n");
    return -EINVAL;
}

STATIC int vhdca_mmap(ka_file_t *filep, ka_vm_area_struct_t *vma)
{
    int ret;

    ret = hdcdrv_mmap_param_check(filep, vma);
    if (ret != 0) {
        hdcdrv_err("mmap param check failed, ret = %d\n", ret);
        return -EINVAL;
    }

    ka_mm_set_vm_mremap(&vhdca_vm_ops_managed, vhdca_vm_mremap);
    ka_mm_set_vm_ops(vma, &vhdca_vm_ops_managed);
    ka_mm_set_vm_flags(vma, KA_VM_DONTEXPAND| KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_IO | KA_VM_PFNMAP);
    ka_mm_set_vm_private_data(vma, filep->private_data);

    return HDCDRV_OK;
}

STATIC unsigned long vhdca_get_unmapped_area(ka_file_t *filp, unsigned long addr, unsigned long len,
    unsigned long pgoff, unsigned long flags)
{
    if ((flags & KA_MAP_FIXED) != 0) {
        hdcdrv_err("hdc not support KA_MAP_FIXED\n");
        return -EINVAL;
    }

    if (filp == NULL) {
        hdcdrv_err("filp is NULL in hdcdrv_get_unmapped_area\n");
        return -ENODEV;
    }

    return ka_task_get_current_get_unmapped_area(filp, addr, len, pgoff, flags);
}

const ka_file_operations_t vhdca_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_unlocked_ioctl(vhdca_ioctl)
    ka_fs_init_f_open(vhdca_open)
    ka_fs_init_f_release(vhdca_release)
    ka_fs_init_f_mmap(vhdca_mmap)
    ka_fs_init_f_get_unmapped_area(vhdca_get_unmapped_area)
};

struct vmng_common_msg_client vhdca_common_msg_client = {
    .type = VMNG_MSG_COMMON_TYPE_HDC,
    .init = NULL,
    .common_msg_recv = vhdca_com_msg_recv,
};

struct vmng_vpc_client vhdca_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_HDC,
    .init = NULL,
    .msg_recv = vhdca_vpc_msg_recv,
};


void* hdcdrv_get_sync_mem_buf(int dev_id)
{
    return g_vhdca_ctrl->pdev[dev_id].sync_mem_buf;
}

ka_mutex_t *hdcdrv_get_sync_mem_lock(int dev_id)
{
    return &(g_vhdca_ctrl->pdev[dev_id].sync_mem_mutex);
}

int vhdca_vpc_msg_send(u32 dev_id, enum vmng_vpc_type vpc_type, struct vmng_tx_msg_proc_info *tx_info, u32 timeout)
{
    u32 retry_time;
    int ret;

    if (timeout == VPC_BLK_MODE_TIMEOUT) {
        retry_time = VHDC_BLK_VPC_MSG_SEND_RETRY_TIME;
    } else {
        retry_time = VHDC_VPC_MSG_SEND_RETRY_TIME;
    }

    do {
        ret = vpc_msg_send(dev_id, VPC_VM_FID, vpc_type, tx_info, timeout);
        if (ret != -ENOSPC) {
            break;
        }
        if (timeout == VPC_BLK_MODE_TIMEOUT) {
            ka_system_usleep_range(1000, 2000);  /* 1000us ~ 2000us */
        } else {
            ka_system_usleep_range(100, 200);  /* 100us ~ 200us */
        }
        retry_time--;
    } while (retry_time != 0);

    return ret;
}

long hdcdrv_non_trans_ctrl_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct vmng_tx_msg_proc_info tx_info;
    long ret;

    tx_info.data = data;
    tx_info.in_data_len = in_data_len;
    tx_info.out_data_len = out_data_len;
    tx_info.real_out_len = 0;

    ret = (long)vhdca_vpc_msg_send(devid, VMNG_VPC_TYPE_HDC_CTRL, &tx_info, VPC_DEFAULT_TIMEOUT);
    *real_out_len = tx_info.real_out_len;
    return ret;
}
EXPORT_SYMBOL_UNRELEASE(hdcdrv_non_trans_ctrl_msg_send);

STATIC int vhdca_register_cdev(void)
{
    int ret;

    ret = hdccom_register_cdev(&g_vhdca_ctrl->vhdca_cdev, &vhdca_fops);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_register_cdev failed. (ret=%d)\n", ret);
        return ret;
    }

    return HDCDRV_OK;
}

STATIC void vhdca_free_cdev(void)
{
    hdccom_free_cdev(&g_vhdca_ctrl->vhdca_cdev);
}

STATIC int vhdca_init_segment(struct vhdca_pdev *pdev)
{
    struct vmng_tx_msg_proc_info tx_info;
    struct vhdc_ctrl_msg msg;
    int ret;

    if (g_vhdca_ctrl->segment != HDCDRV_INVALID_PACKET_SEGMENT) {
        pdev->segment = g_vhdca_ctrl->segment;
        return HDCDRV_OK;
    }

    msg.type = VHDC_CTRL_MSG_TYPE_SEGMENT;
    msg.error_code = HDCDRV_ERR;
    msg.vhdc_segment.segment = 0;

    tx_info.data = &msg;
    tx_info.in_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.out_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.real_out_len = 0;
    ret = vmnga_common_msg_send(pdev->dev_id, VMNG_MSG_COMMON_TYPE_HDC, &tx_info);
    if ((ret != 0) || (msg.error_code != HDCDRV_OK)) {
        hdcdrv_err("Calling vmnga_common_msg_send failed. (dev_id=%u; ret=%d; error_code=%d)\n",
            pdev->dev_id, ret, msg.error_code);
        return HDCDRV_SEND_CTRL_MSG_FAIL;
    }

    if ((msg.vhdc_segment.segment > HDCDRV_HUGE_PACKET_SEGMENT) ||
        (msg.vhdc_segment.segment < HDCDRV_SMALL_PACKET_SEGMENT)) {
        hdcdrv_err("segment is invalid. (segment=%d)\n", msg.vhdc_segment.segment);
        return HDCDRV_ERR;
    }

    g_vhdca_ctrl->segment = msg.vhdc_segment.segment;
    pdev->segment = msg.vhdc_segment.segment;
    return HDCDRV_OK;
}

STATIC int vhdca_init_pm_version(struct vhdca_pdev *pdev)
{
    struct vmng_tx_msg_proc_info tx_info;
    struct vhdc_ctrl_msg msg;
    int ret;

    msg.type = VHDC_CTRL_MSG_TYPE_HDC_VERSION;
    msg.error_code = HDCDRV_ERR;
    msg.hdc_version.pm_version = HDCDRV_INVALID_HDC_VERSION;
    msg.hdc_version.vm_version = pdev->vm_version;

    tx_info.data = &msg;
    tx_info.in_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.out_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.real_out_len = 0;
    ret = vmnga_common_msg_send(pdev->dev_id, VMNG_MSG_COMMON_TYPE_HDC, &tx_info);
    if ((ret != 0) || (msg.error_code != HDCDRV_OK)) {
        hdcdrv_err("Calling vmnga_common_msg_send failed. (dev_id=%u; ret=%d; error_code=%d)\n",
            pdev->dev_id, ret, msg.error_code);
        return HDCDRV_SEND_CTRL_MSG_FAIL;
    }

    if (g_vhdca_ctrl->pm_version == HDCDRV_INVALID_HDC_VERSION) {
        g_vhdca_ctrl->pm_version = msg.hdc_version.pm_version;
    }
    hdcdrv_info("HDC Version in PM is %d, in VM is %d.\n", g_vhdca_ctrl->pm_version, pdev->vm_version);
    return HDCDRV_OK;
}

STATIC void vhdca_init_delayed_work(ka_work_struct_t *pwork)
{
    struct vhdca_pdev *pdev = ka_container_of(pwork, struct vhdca_pdev, init.work);
    int ret;

    ret = vmnga_register_common_msg_client(pdev->dev_id, &vhdca_common_msg_client);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vmnga_register_common_msg_client failed. (ret=%d)\n", ret);
        return;
    }

    ret = vmnga_vpc_register_client(pdev->dev_id, &vhdca_vpc_client);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vmnga_vpc_register_client failed. (ret=%d)\n", ret);
        goto VHDC_VPC_CLIENT_FAIL;
    }

    ret = vhdca_init_pm_version(pdev);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Version not support or get version failed. (ret=%d)\n", ret);
    }

    ret = vhdca_init_segment(pdev);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_init_segment failed. (ret=%d)\n", ret);
        goto VHDC_MEMPOOL_FAIL;
    }

    ret = vhdca_init_mempool(pdev);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_init_mempool failed. (ret=%d)\n", ret);
        goto VHDC_MEMPOOL_FAIL;
    }

    pdev->valid = HDCDRV_VALID;
    hdcdrv_info("Device init success. (fid=%u)\n", pdev->dev_id);
    return;

VHDC_MEMPOOL_FAIL:
    vmnga_vpc_unregister_client(pdev->dev_id, &vhdca_vpc_client);
VHDC_VPC_CLIENT_FAIL:
    vmnga_unregister_common_msg_client(pdev->dev_id, &vhdca_common_msg_client);

    return;
}

STATIC void vhdca_vdev_wait_for_idle(struct vhdca_pdev *pdev)
{
    int wait_times = VHDC_VDEV_BUSY_TIMEOUT;

    while (vhdca_vdev_is_busy(pdev) && (wait_times > 0)) {
        hdcdrv_info_limit("vdevice is busy. (user_count=%llu)\n", (u64)ka_base_atomic64_read(&pdev->busy));
        ka_system_msleep(1);  // sleep 1 ms
        wait_times--;
    }

    if (wait_times <= 0) {
        hdcdrv_warn("Wait vdev busy time out. (user_count=%llu)\n", (u64)ka_base_atomic64_read(&pdev->busy));
    }
}

#ifndef DRV_UT
STATIC int vhdca_init_instance(u32 dev_id, ka_device_t *dev)
{
    struct vhdca_pdev *pdev = NULL;

    pdev = &g_vhdca_ctrl->pdev[dev_id];
    pdev->dev_id = dev_id;
    pdev->dev = dev;
    pdev->valid = HDCDRV_INVALID;
    pdev->segment = HDCDRV_INVALID_PACKET_SEGMENT;
    pdev->vm_version = HDC_VERSION;
    ka_base_atomic64_set(&pdev->busy, 0);

    KA_TASK_INIT_DELAYED_WORK(&pdev->init, vhdca_init_delayed_work);
    ka_task_schedule_delayed_work(&pdev->init, 0);

    if (pdev->sync_mem_buf == NULL) {
        pdev->sync_mem_buf = (void *)hdcdrv_vzalloc(HDCDRV_NON_TRANS_MSG_S_DESC_SIZE, KA_SUB_MODULE_TYPE_0);
        if (pdev->sync_mem_buf == NULL) {
            hdcdrv_err("Calling vzalloc failed. (dev_id=%d)\n", dev_id);
            return HDCDRV_ERR;
        }
    }
    ka_task_mutex_init(&pdev->sync_mem_mutex);

    g_vhdca_ctrl->vdev_count++;
    return HDCDRV_OK;
}

STATIC int vhdca_uninit_instance(u32 dev_id)
{
    struct vhdca_pdev *pdev = NULL;

    pdev = &g_vhdca_ctrl->pdev[dev_id];
    pdev->valid = HDCDRV_INVALID;

    vhdca_vdev_wait_for_idle(pdev);

    ka_task_cancel_delayed_work_sync(&pdev->init);
    vmnga_vpc_unregister_client(dev_id, &vhdca_vpc_client);
    vmnga_unregister_common_msg_client(dev_id, &vhdca_common_msg_client);

    vhdca_uninit_mempool(pdev);

    if (pdev->sync_mem_buf != NULL) {
        hdcdrv_vfree(pdev->sync_mem_buf, KA_SUB_MODULE_TYPE_0);
        pdev->sync_mem_buf = NULL;
    }

    pdev->dev_id = 0;
    pdev->dev = NULL;
    ka_base_atomic64_set(&pdev->busy, 0);

    g_vhdca_ctrl->vdev_count--;
    return HDCDRV_OK;
}


#define HDC_REMOTE_NOTIFIER "hdc_remote"
static int vhdca_remote_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= VMNG_PDEV_MAX) {
        hdcdrv_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = vhdca_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = vhdca_uninit_instance(udevid);
    }

    return ret;
}
#endif

int g_running_env;

int hdcdrv_get_running_env(void)
{
    return g_running_env;
}

STATIC int vhdca_init(void)
{
    g_running_env = HDCDRV_RUNNING_ENV_X86_NORMAL;
    hdccom_fill_cmd_size_table();

    g_vhdca_ctrl = (struct vhdca_ctrl *)hdcdrv_vzalloc(sizeof(struct vhdca_ctrl), KA_SUB_MODULE_TYPE_0);
    if (g_vhdca_ctrl == NULL) {
        hdcdrv_err("Calling alloc failed. (size=%ld)\n", sizeof(struct vhdca_ctrl));
        return HDCDRV_MEM_ALLOC_FAIL;
    }

    if (vhdca_register_cdev() != HDCDRV_OK) {
        hdcdrv_vfree(g_vhdca_ctrl, KA_SUB_MODULE_TYPE_0);
        g_vhdca_ctrl = NULL;
        hdcdrv_err("Calling vhdca_register_cdev failed.\n");
        return HDCDRV_CHAR_DEV_CREAT_FAIL;
    }

    KA_TASK_INIT_DELAYED_WORK(&g_vhdca_ctrl->recycle_mem, hdcdrv_recycle_mem_work);

    g_vhdca_ctrl->fmem.rbtree = KA_RB_ROOT;
    g_vhdca_ctrl->fmem.rbtree_re = KA_RB_ROOT;
    ka_task_spin_lock_init(&g_vhdca_ctrl->fmem.rb_lock);
    ka_task_spin_lock_init(&g_vhdca_ctrl->fmem.mem_dfx_stat.lock);

    g_vhdca_ctrl->segment = HDCDRV_INVALID_PACKET_SEGMENT;
    g_vhdca_ctrl->vdev_count = 0;

    g_vhdca_ctrl->pm_version = HDCDRV_INVALID_HDC_VERSION;

    return HDCDRV_OK;
}

STATIC void vhdca_uninit(void)
{
    hdcdrv_fast_mem_uninit(&g_vhdca_ctrl->fmem.rb_lock, &g_vhdca_ctrl->fmem.rbtree, HDCDRV_TRUE_FLAG, HDCDRV_DEL_FLAG);
    vhdca_free_cdev();

    hdcdrv_vfree(g_vhdca_ctrl, KA_SUB_MODULE_TYPE_0);
    g_vhdca_ctrl = NULL;
}

STATIC int __init vhdca_init_module(void)
{
    struct uda_dev_type type;
    int ret;

    ret = vhdca_init();
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_init failed. (ret=%d)\n", ret);
        return ret;
    }
#ifndef DRV_UT
    uda_davinci_remote_real_entity_type_pack(&type);
    ret = uda_notifier_register(HDC_REMOTE_NOTIFIER, &type, UDA_PRI1, vhdca_remote_notifier_func);
    if (ret != 0) {
        vhdca_uninit();
        hdcdrv_err("Register notifier failed. (ret=%d)\n", ret);
        return ret;
    }
#endif
    hdcdrv_info("Calling vhdca init success.\n");
    return HDCDRV_OK;
}

STATIC void __exit vhdca_exit_module(void)
{
#ifndef DRV_UT
    struct uda_dev_type type;

    uda_davinci_remote_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(HDC_REMOTE_NOTIFIER, &type);
#endif
    vhdca_uninit();
    hdcdrv_info("Calling vhdca uninit success.\n");
}

ka_module_init(vhdca_init_module);
ka_module_exit(vhdca_exit_module);

KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("vhdca agent driver");
KA_MODULE_LICENSE("GPL");

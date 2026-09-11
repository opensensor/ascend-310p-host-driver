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

#include <linux/pci.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "svm_cmd.h"
#include "devmm_adapt.h"
#include "devmm_proc_info.h"
#include "devmm_channel.h"
#include "svm_master_remote_map.h"
#include "devdrv_interface.h"
#include "svm_hot_reset.h"
#include "svm_proc_mng.h"
#include "svm_master_dev_capability.h"
#include "svm_dev_res_mng.h"
#include "svm_device_msg_client.h"

#define DEVMM_MSG_INIT_SEND_TRYTIMES 1000

#define DEVMM_DEV_HPAGE_SHIIFT 21
#define DEVMM_DEV_PAGE_SHIIFT 12

bool devmm_device_agent_is_ready(u32 devid)
{
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;
    bool is_ready;

    svm_id_inst_pack(&id_inst, devid, 0);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        return false;
    }

    is_ready = (dev_res_mng->dev_msg_client.msg_chan == NULL) ? false : true;
    devmm_dev_res_mng_put(dev_res_mng);
    return is_ready;
}

/*lint -e629*/
STATIC int devmm_rx_msg_process(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    u32 did = (u32)devdrv_get_msg_chan_devid(msg_chan);
    u32 dev_id;
    int ret;

    if ((data == NULL) || (real_out_len == NULL)) {
        devmm_drv_err("Data, out_len, client or dev_ctrl is null. data=%pK; out_len=%pK)\n", data, real_out_len);
        return -EINVAL;
    }

    if (in_data_len < sizeof(struct devmm_chan_msg_head)) {
        devmm_drv_err("In_data_len invalid. (in_data_len=%u)\n", in_data_len);
        return -EMSGSIZE;
    }

    devmm_svm_stat_recv_inc();
    if (did >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Deivice id must less than DEVMM_MAX_DEVICE_NUM. "
                      "(did=%d; DEVMM_MAX_DEVICE_NUM=%d)\n", did, DEVMM_MAX_DEVICE_NUM);
        return -ENODEV;
    }
    dev_id = ((struct devmm_chan_msg_head *)data)->dev_id;
    ((struct devmm_chan_msg_head *)data)->dev_id = (u16)did;

    ret = devmm_chan_msg_dispatch(data, in_data_len, out_data_len, real_out_len,
        &devmm_channel_msg_processes[0]);
    ((struct devmm_chan_msg_head *)data)->dev_id = (u16)dev_id;

    return ret;
}

#define DEVMM_NON_TRANS_MSG_DESC_SIZE 0x10000 /* 64k */
static struct devdrv_non_trans_msg_chan_info devmm_msg_chan_info = {
    .msg_type = devdrv_msg_client_devmm,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .s_desc_size = DEVMM_NON_TRANS_MSG_DESC_SIZE,
    .c_desc_size = DEVMM_NON_TRANS_MSG_DESC_SIZE,
    .rx_msg_process = devmm_rx_msg_process,
};

STATIC int devmm_alloc_msg_chan(struct devmm_dev_msg_client *dev_msg_client, u32 dev_id)
{
    if (dev_msg_client->msg_chan != NULL) {
        devmm_drv_err("Message chan has been alloced.\n");
        return -EINVAL;
    }

    dev_msg_client->msg_chan = devdrv_pcimsg_alloc_non_trans_queue(dev_id, &devmm_msg_chan_info);
    if (dev_msg_client->msg_chan == NULL) {
        devmm_drv_err("Devmm malloc message tran fail.\n");
        return -EINVAL;
    }

    devmm_drv_debug("Host non-tran message chan init success.\n");
    return 0;
}

static u64 g_devmm_host_uva_start = DEVMM_DEFAULT_HOST_UVA_START;
KA_TASK_DEFINE_MUTEX(g_devmm_host_uva_lock);
u64 devmm_get_host_uva_start(void)
{
    return g_devmm_host_uva_start;
}

static void devmm_set_host_uva_range(u32 devid)
{
    static u32 inited_flag = 0;

    ka_task_mutex_lock(&g_devmm_host_uva_lock);
    if (inited_flag != 0) {
        ka_task_mutex_unlock(&g_devmm_host_uva_lock);
        return;
    }

    if (devmm_is_hccs_connect(devid)) {
        g_devmm_host_uva_start = DEVMM_HCCS_HOST_UVA_START;
    } else {
        g_devmm_host_uva_start = DEVMM_DEFAULT_HOST_UVA_START;
    }
    devmm_svm->mmap_para.segs[devmm_svm->mmap_para.seg_num].va = DEVMM_HOST_PIN_START;
    devmm_svm->mmap_para.segs[devmm_svm->mmap_para.seg_num].size = DEVMM_HOST_PIN_SIZE;
    devmm_svm->mmap_para.seg_num++;
    inited_flag = 1;
    ka_task_mutex_unlock(&g_devmm_host_uva_lock);
}

static int devmm_get_dev_info(u32 dev_id, struct devmm_chan_exchange_pginfo *info)
{
    u32 msg_len = sizeof(struct devmm_chan_exchange_pginfo);

    devmm_set_host_uva_range(dev_id);
    info->head.msg_id = DEVMM_CHAN_EX_PGINFO_H2D_ID;
    info->host_page_shift = KA_MM_PAGE_SHIFT;
    info->host_hpage_shift = KA_MM_HPAGE_SHIFT;
    info->head.dev_id = (u16)dev_id;
    info->host_uva_start = DEVMM_HOST_PIN_START;

    return devmm_common_msg_send(info, msg_len, msg_len);
}

static int devmm_set_dev_pg_size_info(struct devmm_chan_exchange_pginfo *info)
{
    if ((info->device_page_shift != DEVMM_DEV_PAGE_SHIIFT) || (info->device_hpage_shift != DEVMM_DEV_HPAGE_SHIIFT)) {
        devmm_drv_err("Dev_page_shift or dev_hpage_shift is invalid. (dev_page_shift=%u; dev_hpage_shift=%u)\n",
            info->device_page_shift, info->device_hpage_shift);
        return -EINVAL;
    }

    devmm_svm_set_device_pgsf(info->device_page_shift);
    devmm_svm_set_device_hpgsf(info->device_hpage_shift);
    if (devmm_svm->page_size_inited == 0) {
        devmm_chan_set_host_device_page_size();
    }

    return 0;
}

#define DEVMM_GET_DEV_INFO_MSG_TRY_TIMES 3
int devmm_host_dev_init(u32 dev_id, u32 vfid)
{
    struct devmm_chan_exchange_pginfo info = {{{0}}};
    u32 i = 0;
    int ret;

    devmm_svm_set_host_pgsf(KA_MM_PAGE_SHIFT);
    devmm_svm_set_host_hpgsf(KA_MM_HPAGE_SHIFT);

    ret = devdrv_get_master_devid_in_the_same_os(dev_id, &devmm_svm->device_info.cluster_id[dev_id]);
    if (ret != 0) {
        devmm_drv_err("Get master_devid failed. (devid=%u)\n", dev_id);
        return ret;
    }

    ret = -EINVAL;
    while ((i < DEVMM_GET_DEV_INFO_MSG_TRY_TIMES) && (ret != 0)) {
        i++;
        ret = devmm_get_dev_info(dev_id, &info);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_warn("Get dev capability info failed, device may not ka_task_up yet."
                " (ret=%d; dev_id=%u; vfid=%u; ret=%d)\n", ret, dev_id, vfid, ret);
            continue;
#endif
        }

        ret = devmm_set_dev_pg_size_info(&info);
        if (ret != 0) {
            continue;
        }

        ret = devmm_set_dev_capability(dev_id, vfid, &info);
        if (ret != 0) {
            continue;
        }

        if (devmm_dev_capability_support_offset_security(dev_id)) {
            ret = devmm_init_convert_addr_mng(dev_id, &info);
            if (ret != 0) {
                devmm_drv_err("Synchronize convert addr failed. (ret=%d)\n", ret);
                continue;
            }
        }
        devmm_set_dev_mem_size_info(dev_id, &info);
        devmm_drv_info("Host_dev_init. (try_times=%u; dev_id=%u; vfid=%u)\n", i, dev_id, vfid);
    }

    return ret;
}

void devmm_host_dev_uninit(u32 dev_id)
{
    devmm_uninit_shm_by_devid(dev_id);

    devmm_clear_dev_capability(dev_id);
    devmm_clear_dev_mem_size_info(dev_id);
}

int devmm_dev_res_init(struct devmm_dev_res_mng *dev_res_mng)
{
    struct devmm_dev_msg_client *dev_msg_client = &dev_res_mng->dev_msg_client;
    u32 devid = dev_res_mng->id_inst.devid;
    u32 vfid = dev_res_mng->id_inst.vfid;
    int ret;

    ret = devmm_alloc_msg_chan(dev_msg_client, devid);
    if (ret != 0) {
        devmm_drv_err("Alloc msg chan failed. (devid=%u)\n", devid);
        return ret;
    }

    /* send pagesize msg to device. */
    ret = devmm_host_dev_init(devid, vfid);
    if (ret != 0) {
        devdrv_pcimsg_free_non_trans_queue(dev_msg_client->msg_chan);
        devmm_drv_err("Devmm device message channel init failed. (devid=%u)\n", devid);
        return ret;
    }

    devmm_drv_debug("Devmm device message channel is ready. (devid=%u)\n", devid);
    (void)devdrv_set_module_init_finish((int)devid, DEVDRV_HOST_MODULE_DEVMM);

    /* clear hot reset flag */
    devmm_svm_business_info_init(devid);
    return 0;
}

#ifndef DEVMM_UT
STATIC int devmm_get_smmu_status(ka_device_t *dev, u32 *smmu_status)
{
    u32 flag = devmm_get_alloc_mask(false);
    ka_page_t *page = NULL;
    u64 dma_addr, pa;
    int ret = 0;

    page = ka_mm_alloc_pages(flag, 0);
    if (page == NULL) {
        devmm_drv_err("OOM: Alloc page fail, page_num = 1.\n");
        return -ENOMEM;
    }

    pa = (u64)ka_mm_page_to_phys(page);
    dma_addr = devdrv_dma_map_page(dev, page, 0, KA_MM_PAGE_SIZE, KA_DMA_BIDIRECTIONAL);
    if (ka_mm_dma_mapping_error(dev, dma_addr) != 0) {
        devmm_drv_err("Dma map page failed.\n");
        __ka_mm_free_page(page);
        page = NULL;
        return -ENOMEM;
    }

    *smmu_status = DEVMM_SMMU_STATUS_OPENING;
    if (dma_addr == pa) {
        *smmu_status = DEVMM_SMMU_STATUS_CLOSEING;
    }

    devdrv_dma_unmap_page(dev, dma_addr, KA_MM_PAGE_SIZE, KA_DMA_BIDIRECTIONAL);
    __ka_mm_free_page(page);
    page = NULL;
    return ret;
}
#endif

int devmm_query_smmu_status(ka_device_t *dev)
{
    if (devmm_svm->smmu_status != DEVMM_SMMU_STATUS_UNINIT) {
        return 0;
    }

    return devmm_get_smmu_status(dev, &devmm_svm->smmu_status);
}

int devmm_device_chan_msg_send(u32 dev_id, void *msg, u32 len, u32 out_len)
{
    struct devmm_dev_res_mng *dev_res_mng = NULL;
    struct svm_id_inst id_inst;
    void *msg_chan = NULL;
    int ret;

    svm_id_inst_pack(&id_inst, dev_id, 0);
    dev_res_mng = devmm_dev_res_mng_get(&id_inst);
    if (dev_res_mng == NULL) {
        devmm_drv_err("Get dev_res_mng failed. (devid=%u)\n", dev_id);
        return -ENODEV;
    }

    msg_chan = dev_res_mng->dev_msg_client.msg_chan;
    if (msg_chan == NULL) {
        devmm_drv_err("Device_id not ready. (dev_id=%d)\n", dev_id);
        devmm_dev_res_mng_put(dev_res_mng);
        return -EINVAL;
    }

    ret = devdrv_sync_msg_send(msg_chan, msg, len, out_len, &out_len);
    devmm_dev_res_mng_put(dev_res_mng);

    return ret;
}

int devmm_common_msg_send(void *msg, unsigned int len, unsigned int out_len)
{
    struct devmm_chan_msg_head *tmp_msg = NULL;
    u32 head_len = sizeof(struct devmm_chan_msg_head);
    u32 real_out_len = (out_len >= head_len) ? out_len : head_len;
    u32 device_id;
    int ret;

    if (msg == NULL) {
        devmm_drv_err("Message is NULL.\n");
        return -EINVAL;
    }

    tmp_msg = (struct devmm_chan_msg_head *)msg;
    tmp_msg->result = 0;
    device_id = tmp_msg->dev_id;

    if (device_id >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Host send device error. (device_id=%d)\n", device_id);
        return -ENODEV;
    }
    devmm_svm_stat_send_inc();
    ret = devdrv_common_msg_send(device_id, msg, len, real_out_len, &real_out_len, DEVDRV_COMMON_MSG_DEVMM);
    if (ret != 0) {
        devmm_drv_warn("Device is not ka_task_up yet, wait for a while and try again check. (ret=%d)\n", ret);
        return -ECOMM;
    }

    return tmp_msg->result;
}
EXPORT_SYMBOL_UNRELEASE(devmm_common_msg_send);
/*lint +e629*/

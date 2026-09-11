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

#include "svm_hot_reset.h"
#include "devmm_channel.h"
#include "svm_msg_client.h"
#include "svm_host_msg_client.h"
#include "svm_device_msg_client.h"
#include "svm_register_ops.h"
#include "svm_shmem_interprocess.h"
#include "svm_dev_res_mng.h"
#include "svm_dma_prepare_pool.h"
#include "svm_mem_query.h"
#include "devmm_common.h"
#include "svm_recycle_thread.h"

static void devmm_host_res_mng_create(void)
{
    struct svm_id_inst id_inst;
    svm_id_inst_pack(&id_inst, uda_get_host_id(), 0);
    (void)devmm_dev_res_mng_create(&id_inst, NULL);
}

static void devmm_host_res_mng_destroy(void)
{
    struct svm_id_inst id_inst;
    svm_id_inst_pack(&id_inst, uda_get_host_id(), 0);
    (void)devmm_dev_res_mng_destroy(&id_inst);
}

STATIC int devmm_init_instance(u32 devid, ka_device_t *dev)
{
    struct svm_id_inst id_inst;
    int ret;

    ret = devmm_query_smmu_status(dev);
    if (ret != 0) {
        devmm_drv_err("Query smmu_status fail. (dev=%pK)\n", dev);
        return ret;
    }

    svm_id_inst_pack(&id_inst, devid, 0);
    devmm_dma_prepare_pool_init(devid);
    ret = devmm_dev_res_mng_create(&id_inst, dev);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_dma_prepare_pool_uninit(devid);
#endif
    }
    return ret;
}

STATIC int devmm_uninit_instance(u32 devid)
{
    struct svm_id_inst id_inst;

    if (devmm_wait_business_finish(devid) == false) {
        return -EBUSY;
    }

    /*
     * Recycle thread will call dev_res_mng_get,
     * pause thread to prevent hold it too long, result in hot reset timeout.
     */
    svm_recycle_thread_pause();

    svm_id_inst_pack(&id_inst, devid, 0);
    devmm_dev_res_mng_destroy(&id_inst);
    devmm_dma_prepare_pool_uninit(devid);

    svm_recycle_thread_continue();
    devmm_drv_info("Devmm uninit instance. (devid=%u)\n", devid);
    return 0;
}

#define SVM_HOST_NOTIFIER "svm_host"
STATIC int devmm_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = devmm_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = devmm_uninit_instance(udevid);
    } else if (action == UDA_HOTRESET) {
        ret = devmm_hotreset_pre_handle(udevid);
    } else if (action == UDA_HOTRESET_CANCEL) {
        ret = devmm_hotreset_cancel_handle(udevid);
    } else if (action == UDA_PRE_HOTRESET) {
        ret = devmm_hotreset_pre_handle(udevid);
    } else if (action == UDA_PRE_HOTRESET_CANCEL) {
        ret = devmm_hotreset_cancel_handle(udevid);
    }

    devmm_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

int devmm_register_ops_init(void)
{
    struct uda_dev_type type;
    int ret;

    ret = devmm_svm_mem_query_ops_register();
    if (ret != 0) {
        return ret;
    }

    /* alloc business mem for hot reset */
    ret = devmm_alloc_business_info();
    if (ret != 0) {
        goto alloc_business_info_fail;
    }

    /* register a callback instance to PCIe, the callback instance include "msg_chan_init hot_reset_handle" */
    devmm_init_msg();
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(SVM_HOST_NOTIFIER, &type, UDA_PRI2, devmm_host_notifier_func);
    if (ret != 0) {
        devmm_drv_err("Register devmm_host_instance client error. (ret=%d)\n", ret);
        goto register_client_fail;
    }

    ret = devmm_host_msg_chan_init();
    if (ret != 0) {
        goto msg_chan_init_fail;
    }

    ret = devmm_register_reboot_notifier();
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Register reboot notifier fail. (ret=%d)\n", ret);
        goto register_reboot_fail;
#endif
    }

    devmm_host_res_mng_create();

    return 0;
#ifndef EMU_ST
register_reboot_fail:
devmm_host_msg_chan_uninit();
#endif
msg_chan_init_fail:
    (void)uda_notifier_unregister(SVM_HOST_NOTIFIER, &type);
register_client_fail:
    devmm_free_business_info();
alloc_business_info_fail:
    devmm_svm_mem_query_ops_unregister();
    return ret;
}

void devmm_unregister_ops_uninit(void)
{
    struct uda_dev_type type;

    devmm_host_res_mng_destroy();

    devmm_unregister_reboot_notifier();
    /* unregister a callback instance to PCIe, the callback instance include "msg_chan_init hot_reset_handle" */
    devmm_host_msg_chan_uninit();
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(SVM_HOST_NOTIFIER, &type);

    devmm_free_business_info();
    devmm_svm_mem_query_ops_unregister();
}

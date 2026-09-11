/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-11-29
 */

#include "ka_task_pub.h"
#include "svm_msg_client.h"
#include "svm_hot_reset.h"
#include "svm_shmem_interprocess.h"
#include "svm_task_dev_res_mng.h"
#include "devmm_channel.h"
#include "svm_master_mem_share.h"
#include "svm_shmem_node.h"
#include "svm_dev_res_mng.h"

static KA_TASK_DEFINE_RWLOCK(mng_rwlock);
static struct devmm_dev_res_mng *dev_res_mng[SVM_DEV_INST_MAX_NUM];

void devmm_init_task_dev_res_info(struct devmm_task_dev_res_info *info)
{
    KA_INIT_LIST_HEAD(&info->head);
    ka_task_init_rwsem(&info->rw_sem);
}

static void devmm_init_ipc_mem_node_info(struct devmm_ipc_mem_node_info *info)
{
    ka_hash_init(info->node_htable);
    ka_task_rwlock_init(&info->rwlock);
    ka_task_mutex_init(&info->mutex);
}

static void devmm_uninit_ipc_mem_node_info(struct devmm_ipc_mem_node_info *info)
{
    ka_task_mutex_destroy(&info->mutex);
}

static void devmm_init_share_agent_blk_mng(struct devmm_share_phy_addr_agent_blk_mng *share_agent_blk_mng)
{
    share_agent_blk_mng->rbtree = KA_RB_ROOT;
    ka_task_init_rwsem(&share_agent_blk_mng->rw_sem);
}

static int devmm_init_dev_msg_client(struct devmm_dev_msg_client *msg_client, ka_device_t *dev)
{
    struct devmm_dev_res_mng *mng = ka_container_of(msg_client, struct devmm_dev_res_mng, dev_msg_client);

    if ((mng->id_inst.vfid != 0) || (mng->id_inst.devid == SVM_HOST_AGENT_ID) || (dev == NULL)) {
        return 0;
    }

    devmm_drv_debug("Device message client will init. (devid=%u)\n", mng->id_inst.devid);

    msg_client->dev = dev;
    return devmm_dev_res_init(mng);
}

static void devmm_uninit_dev_msg_client(struct devmm_dev_msg_client *msg_client)
{
    struct devmm_dev_res_mng *mng = ka_container_of(msg_client, struct devmm_dev_res_mng, dev_msg_client);

    if ((mng->id_inst.vfid != 0) || (mng->id_inst.devid == SVM_HOST_AGENT_ID) || (msg_client->dev == NULL)) {
        return;
    }

    devmm_drv_info("Device message client will uninit. (did=%d)\n", mng->id_inst.devid);
    devmm_host_dev_uninit(mng->id_inst.devid);
    if (msg_client->msg_chan != NULL) {
        devdrv_set_msg_chan_priv(msg_client->msg_chan, NULL);
        devdrv_pcimsg_free_non_trans_queue(msg_client->msg_chan);
    }

    msg_client->dev = NULL;
    msg_client->msg_chan = NULL;
}

static int devmm_dev_res_mng_res_init(struct devmm_dev_res_mng *mng,
    struct svm_id_inst *id_inst, ka_device_t *dev)
{
    mng->id_inst = *id_inst;
    mng->dev = dev;
    ka_base_kref_init(&mng->ref);

    devmm_init_ipc_mem_node_info(&mng->ipc_mem_node_info);
    devmm_init_task_dev_res_info(&mng->task_dev_res_info);
    devmm_init_share_agent_blk_mng(&mng->share_agent_blk_mng);
    if (dev != NULL) {
        mng->is_mdev_vm = devmm_is_mdev_vm_boot_mode(id_inst->devid);
        return devmm_init_dev_msg_client(&mng->dev_msg_client, dev);
    }
    return 0;
}

static void devmm_dev_res_mng_priv_res_uninit(struct devmm_dev_res_mng *mng)
{
    if (mng->dev != NULL) {
        devmm_uninit_dev_msg_client(&mng->dev_msg_client);
    }
    devmm_uninit_ipc_mem_node_info(&mng->ipc_mem_node_info);
}

static void devmm_dev_res_mng_pub_res_uninit(struct devmm_dev_res_mng *mng)
{
    if ((mng->id_inst.vfid != 0) || (mng->id_inst.devid == SVM_HOST_AGENT_ID)) {
        return;
    }

    devmm_share_agent_blk_destroy_all(&mng->share_agent_blk_mng);
    devmm_uninit_convert_addr_mng(mng->id_inst.devid);
}

/*
 * Class's resources can be classified into public and private,
 * the public resource should be protected and released when the ref is 0.
 */
static void devmm_dev_res_mng_res_uninit(struct devmm_dev_res_mng *mng)
{
    devmm_dev_res_mng_priv_res_uninit(mng);
    devmm_dev_res_mng_pub_res_uninit(mng);
}

static int devmm_dev_res_mng_insert(struct devmm_dev_res_mng *mng, struct svm_id_inst *id_inst)
{
    u32 dev_inst_id = svm_id_inst_to_dev_inst(id_inst);

    ka_task_write_lock_bh(&mng_rwlock);
    if (dev_res_mng[dev_inst_id] != NULL) {
        ka_task_write_unlock_bh(&mng_rwlock);
        devmm_drv_err("Already exist. (devid=%u; vfid=%u)\n", id_inst->devid, id_inst->vfid);
        return -EEXIST;
    }

    dev_res_mng[dev_inst_id] = mng;
    ka_task_write_unlock_bh(&mng_rwlock);
    return 0;
}

static struct devmm_dev_res_mng *devmm_dev_res_mng_erase(struct svm_id_inst *id_inst)
{
    u32 dev_inst_id = svm_id_inst_to_dev_inst(id_inst);
    struct devmm_dev_res_mng *mng = NULL;
    u32 erase_try_time = 600; /* Try 600s, 600 times, 1000ms each time */
    u32 i;

    for (i = 0; i < erase_try_time; i++) {
        if (devmm_is_active_reboot_status()) {
#ifndef EMU_ST
            devmm_drv_info("Is active reboot status, no wait. (devid=%u)\n", dev_inst_id);
            return NULL;
#endif
        }

        ka_task_write_lock_bh(&mng_rwlock);
        mng = dev_res_mng[dev_inst_id];
        if (mng == NULL) {
            ka_task_write_unlock_bh(&mng_rwlock);
            devmm_drv_err("Already erase. (devid=%u; vfid=%u)\n", id_inst->devid, id_inst->vfid);
            return NULL;
        }
        if (ka_base_kref_read(&mng->ref) == 1) {
            dev_res_mng[dev_inst_id] = NULL;
            ka_task_write_unlock_bh(&mng_rwlock);
            return mng;
        }
        ka_task_write_unlock_bh(&mng_rwlock);
        devmm_drv_debug("Try erase. (devid=%u; vfid=%u; i=%u)\n", id_inst->devid, id_inst->vfid, i);
        ka_system_msleep(1000); /* 1000ms */
    }

    devmm_drv_err("Dev res_mng is in use, can not erase. (devid=%u; vfid=%u)\n", id_inst->devid, id_inst->vfid);
    return NULL;
}

int devmm_dev_res_mng_create(struct svm_id_inst *id_inst, ka_device_t *dev)
{
    struct devmm_dev_res_mng *mng = NULL;
    int ret;

    mng = (struct devmm_dev_res_mng *)devmm_kvzalloc(sizeof(struct devmm_dev_res_mng));
    if (mng == NULL) {
        devmm_drv_err("Repeate create. (devid=%u; vfid=%u)\n", id_inst->devid, id_inst->vfid);
        return -ENOMEM;
    }

    ret = devmm_dev_res_mng_res_init(mng, id_inst, dev);
    if (ret != 0) {
        devmm_kvfree(mng);
        return ret;
    }

    ret = devmm_dev_res_mng_insert(mng, id_inst);
    if (ret != 0) {
        devmm_dev_res_mng_res_uninit(mng);
        devmm_kvfree(mng);
        return ret;
    }

    devmm_drv_debug("Dev res mng create success. (devid=%u; vfid=%u)\n", id_inst->devid, id_inst->vfid);
    return 0;
}

static void devmm_dev_res_mng_release(ka_kref_t *kref)
{
    struct devmm_dev_res_mng *mng = ka_container_of(kref, struct devmm_dev_res_mng, ref);

    devmm_dev_res_mng_pub_res_uninit(mng);
    devmm_kvfree(mng);
}

void devmm_dev_res_mng_destroy(struct svm_id_inst *id_inst)
{
    struct devmm_dev_res_mng *mng = NULL;

    mng = devmm_dev_res_mng_erase(id_inst);
    if (mng == NULL) {
        return;
    }
    devmm_ipc_node_clean_all_by_dev_res_mng(mng);
    devmm_dev_res_mng_priv_res_uninit(mng);
    ka_base_kref_put(&mng->ref, devmm_dev_res_mng_release);
    devmm_drv_info("Dev res mng destroy. (devid=%u; vfid=%u)\n", id_inst->devid, id_inst->vfid);
}

struct devmm_dev_res_mng *devmm_dev_res_mng_get(struct svm_id_inst *id_inst)
{
    u32 dev_inst_id = svm_id_inst_to_dev_inst(id_inst);
    struct devmm_dev_res_mng *mng = NULL;

    ka_task_read_lock_bh(&mng_rwlock);
    mng = dev_res_mng[dev_inst_id];
    if (mng != NULL) {
        ka_base_kref_get(&mng->ref);
    }
    ka_task_read_unlock_bh(&mng_rwlock);

    return mng;
}

void devmm_dev_res_mng_put(struct devmm_dev_res_mng *res_mng)
{
    ka_base_kref_put(&res_mng->ref, devmm_dev_res_mng_release);
}

void devmm_dev_res_mng_destroy_all(void)
{
    u32 devid, vfid;
    u32 stamp = (u32)ka_jiffies;

    for (devid = 0; devid < SVM_MAX_AGENT_NUM; devid++) {
        for (vfid = 0; vfid < DEVMM_MAX_VF_NUM; vfid++) {
            struct devmm_dev_res_mng *mng = NULL;
            struct svm_id_inst inst;

            svm_id_inst_pack(&inst, devid, vfid);
            mng = devmm_dev_res_mng_get(&inst);
            if (mng != NULL) {
                devmm_dev_res_mng_destroy(&inst);
                devmm_dev_res_mng_put(mng);
            }
            devmm_try_cond_resched(&stamp);
        }
    }
}

bool devmm_is_mdev_vm(u32 devid, u32 vfid)
{
    struct devmm_dev_res_mng *mng = NULL;
    struct svm_id_inst inst;
    bool is_mdev_vm = false;

    svm_id_inst_pack(&inst, devid, vfid);
    mng = devmm_dev_res_mng_get(&inst);
    if (mng == NULL) {
        return false;
    }
    is_mdev_vm = mng->is_mdev_vm;
    devmm_dev_res_mng_put(mng);
    devmm_drv_debug("Mdev info. (devid=%u; vfid=%u; is_mdev_vm=%u)\n", devid, vfid, is_mdev_vm);
    return is_mdev_vm;
}
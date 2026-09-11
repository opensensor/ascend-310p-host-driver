/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-08-11
 */

#include "svm_phy_addr_blk_mng.h"
#include "svm_proc_gfp.h"
#include "svm_mem_create.h"
#include "svm_msg_client.h"
#include "devmm_channel.h"
#include "svm_mem_stats.h"
#include "svm_master_advise.h"
#include "svm_master_mem_share.h"
#include "svm_master_mem_create.h"
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#include "svm_shmem_node_pod.h"
#endif

int devmm_master_mem_release(struct devmm_svm_process *svm_proc, u64 pg_num, int id, u32 free_type)
{
    return devmm_mem_release(svm_proc, id, pg_num, free_type);
}

static int devmm_agent_mem_release_without_pages(struct devmm_chan_mem_release *msg)
{
    int ret;

    ret = devmm_chan_msg_send(msg, sizeof(struct devmm_chan_mem_release), sizeof(struct devmm_chan_mem_release));
    if (ret != 0) {
        devmm_drv_err("Mem release without pages msg send failed. (ret=%d; devid=%u; vfid=%u; host_pid=%d; id=%d)\n",
            ret, msg->head.dev_id, msg->head.process_id.vfid, msg->head.process_id.hostpid, msg->id);
    }
    return ret;
}

static int devmm_agent_normal_mem_release(struct devmm_chan_mem_release *msg)
{
    u64 total_pg_num = msg->to_free_pg_num;
    u64 freed_num;
    int ret;

    for (freed_num = 0; freed_num < total_pg_num;) {
        u64 tmp_num = ka_base_min((u64)DEVMM_PAGE_NUM_PER_MSG, (total_pg_num - freed_num));

        msg->to_free_pg_num = tmp_num;
        ret = devmm_chan_msg_send(msg, sizeof(struct devmm_chan_mem_release), sizeof(struct devmm_chan_mem_release));
        if (ret != 0) {
            devmm_drv_err("Normal mem release msg send failed. (ret=%d; devid=%u; vfid=%u; host_pid=%d; id=%d)\n",
                ret, msg->head.dev_id, msg->head.process_id.vfid, msg->head.process_id.hostpid, msg->id);
            return ret;
        }

        freed_num += tmp_num;
    }
    return 0;
}

int devmm_agent_mem_release_public(struct devmm_chan_mem_release *msg)
{
    if (msg->free_type == SVM_PYH_ADDR_BLK_NORMAL_FREE) {
        return devmm_agent_normal_mem_release(msg);
    } else {
        return devmm_agent_mem_release_without_pages(msg);
    }
}

int devmm_agent_mem_release(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    u64 pg_num, int id, u32 free_type)
{
    struct devmm_chan_mem_release msg = {{{0}}};

    msg.head.msg_id = DEVMM_CHAN_MEM_RELEASE_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = (u16)devids->vfid;
    msg.head.dev_id = (u16)devids->devid;
    msg.id = id;
    msg.free_type = free_type;
    msg.to_free_pg_num = pg_num;
    return devmm_agent_mem_release_public(&msg);
}

static int devmm_mem_release_proc(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_release_para *para)
{
    struct devmm_share_id_map_node *map_node = NULL;
    u32 share_sdid, share_devid, blk_type, free_tpye;
    int ret, share_id, hostpid;
    u64 pg_num = para->pg_num;
    int id = para->id;

    map_node = devmm_share_id_map_node_get(svm_proc, devids->devid, id);
    if (map_node == NULL) {
        free_tpye = SVM_PYH_ADDR_BLK_NORMAL_FREE;
        para->handle_type = SVM_MEM_HANDLE_NORMAL_TYPE;
    } else {
        free_tpye = SVM_PYH_ADDR_BLK_FREE_NO_PAGE;
    }

    devmm_drv_debug("Agent mem release. (devid=%u; vfid=%u; id=%d; free_tpye=%u)\n",
        devids->devid, devids->vfid, id, free_tpye);
    if (para->side == DEVMM_SIDE_MASTER) {
        ret = devmm_master_mem_release(svm_proc, pg_num, id, free_tpye);
    } else {
        ret = devmm_agent_mem_release(svm_proc, devids, pg_num, id, free_tpye);
    }
    if (ret != 0) {
        if (map_node != NULL) {
            devmm_share_id_map_node_put(map_node);
        }
        devmm_drv_err("Agent mem release fail. (ret=%d; devid=%u; vfid=%u; host_pid=%d; id=%d; free_tpye=%u)\n",
            ret, devids->devid, devids->vfid, svm_proc->process_id.hostpid, id, free_tpye);
        return ret;
    }

    if (map_node != NULL) {
        bool is_local_blk = true;
        share_sdid = map_node->shid_map_node_info.share_sdid;
        share_devid = map_node->shid_map_node_info.share_devid;
        share_id = map_node->shid_map_node_info.share_id;
        blk_type = map_node->blk_type;
        hostpid = map_node->hostpid;
        devmm_share_id_map_node_destroy(svm_proc, devids->devid, map_node);
        devmm_share_id_map_node_put(map_node);

        devmm_drv_debug("Agent mem release. (devid=%u; vfid=%u; id=%d; free_tpye=%u; blk_type=%u; share_id=%d)\n",
            devids->devid, devids->vfid, id, free_tpye, blk_type, share_id);
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
        if ((blk_type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) && (!svm_is_sdid_in_local_server(devids->devid, share_sdid))) {
            is_local_blk = false;
        }
#endif

        if (is_local_blk) {
            ret = devmm_share_agent_blk_put_with_share_id(share_devid, share_id, hostpid,
                devids->devid, false, SVM_INVALID_SDID);
            if (ret != 0) {
                return ret;
            }
        } else {
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
            ret = devmm_put_remote_share_mem_info(devids->devid, share_id, share_sdid, share_devid);
            if (ret != 0) {
                devmm_drv_err("Put share id failed. (devid=%u; share_devid=%u; share_id=%u; share_sdid=%u)\n",
                    devids->devid, share_id, share_devid, share_sdid);
                return ret;
            }
#endif
        }
        if (blk_type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) {
            para->handle_type = SVM_MEM_HANDLE_IMPORT_TYPE;
        } else {
            para->handle_type = SVM_MEM_HANDLE_NORMAL_TYPE;
        }
    }
    return 0;
}

int devmm_ioctl_mem_release(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_release_para *para = &arg->data.mem_release_para;

    if ((para->side != MEM_HOST_SIDE) && (para->side != MEM_DEV_SIDE)) {
        devmm_drv_err("Invalid side type. (side=%u)\n", para->side);
        return -EINVAL;
    }

    if ((para->side == MEM_HOST_SIDE) && (arg->head.devid != uda_get_host_id())) {
        devmm_drv_err("Invalid side devid. (side=%u; devid=%u)\n", para->side, arg->head.devid);
        return -EINVAL;
    }

    return devmm_mem_release_proc(svm_proc, &arg->head, para);
}

static int devmm_master_mem_create(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 size, u64 *pg_num, int *id)
{
    u32 page_size = (attr->pg_type == MEM_NORMAL_PAGE_TYPE) ? KA_MM_PAGE_SIZE : SVM_MASTER_HUGE_PAGE_SIZE;
    u64 tmp_pg_num;
    int ret;

    if (attr->is_giant_page) {
        page_size = SVM_MASTER_GIANT_PAGE_SIZE;
    }

    if (KA_DRIVER_IS_ALIGNED(size, page_size) == false) {
        devmm_drv_err("Size should aligned by page_size. (size=%llu; page_size=%u)\n", size, page_size);
        return -EINVAL;
    }

    tmp_pg_num = size / page_size;
    ret = devmm_mem_create_to_new_blk(svm_proc, attr, tmp_pg_num, tmp_pg_num, id);
    *pg_num = (ret == 0) ? tmp_pg_num : (*pg_num);
    return ret;
}

static void devmm_mem_create_msg_pack(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 pg_num, struct devmm_chan_mem_create *msg)
{
    msg->head.msg_id = DEVMM_CHAN_MEM_CREATE_H2D_ID;
    msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg->head.process_id.vfid = (u16)attr->vfid;
    msg->head.dev_id = (u16)attr->devid;
    msg->module_id = attr->module_id;
    msg->pg_type = attr->is_giant_page ? MEM_GIANT_PAGE_TYPE : attr->pg_type;
    msg->mem_type = attr->mem_type;
    msg->total_pg_num = (u32)pg_num;  /* size won't out of 4G*4k=16T */
}

static int _devmm_agent_mem_create(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 pg_num, int *id)
{
    struct devmm_chan_mem_create msg = {{{0}}};
    u64 created_num;
    int ret;

    devmm_mem_create_msg_pack(svm_proc, attr, pg_num, &msg);
    for (created_num = 0; created_num < pg_num;) {
        u64 tmp_num = ka_base_min((u64)DEVMM_PAGE_NUM_PER_MSG, (pg_num - created_num));

        msg.to_create_pg_num = (u32)tmp_num;
        msg.is_create_to_new_blk = (created_num == 0) ? 1 : 0;
        ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_mem_create), sizeof(struct devmm_chan_mem_create));
        if (ret != 0) {
            devmm_drv_no_err_if((ret == -ENOMEM), "Msg send failed. (ret=%d; devid=%u; vfid=%u; host_pid=%d)\n",
                ret, attr->devid, attr->vfid, svm_proc->process_id.hostpid);
            goto agent_mem_release;
        }

        created_num += tmp_num;
        *id = msg.id;
    }

    return 0;
agent_mem_release:
    if (created_num != 0) {
        struct devmm_devid devids = {
            .logical_devid = 0, .devid = attr->devid, .vfid = attr->vfid};  /* Logical devid no use */
        /* Ignore failure, will release when proc exiting. */
        (void)devmm_agent_mem_release(svm_proc, &devids, created_num, *id, SVM_PYH_ADDR_BLK_NORMAL_FREE);
    }
    return ret;
}

static int devmm_agent_mem_create(struct devmm_svm_process *svm_proc,
    struct devmm_phy_addr_attr *attr, u64 size, u64 *pg_num, int *id)
{
    u64 pg_size = (attr->pg_type == DEVMM_HUGE_PAGE_TYPE) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;
    u64 aligned_size = (attr->is_giant_page) ? ka_base_round_up(size, DEVMM_GIANT_PAGE_SIZE) : ka_base_round_up(size, pg_size);
    u64 tmp_pg_num = aligned_size / pg_size;
    int ret;

    ret =  _devmm_agent_mem_create(svm_proc, attr, tmp_pg_num, id);
    *pg_num = (ret == 0) ? tmp_pg_num : (*pg_num);
    return ret;
}

static void devmm_mem_create_phy_addr_attr_pack(struct devmm_ioctl_arg *arg, struct devmm_phy_addr_attr *attr)
{
    struct devmm_mem_create_para *para = &arg->data.mem_create_para;

    attr->side = (para->side == MEM_DEV_SIDE) ? MEM_DEV_SIDE : MEM_HOST_SIDE;
    attr->devid = arg->head.devid;
    attr->vfid = arg->head.vfid;
    attr->module_id = para->module_id;

    // host pg_type support MEM_GIANT_PAGE_TYPE
    attr->pg_type = (para->pg_type == MEM_GIANT_PAGE_TYPE && attr->side != DEVMM_SIDE_MASTER)
        ? MEM_HUGE_PAGE_TYPE : para->pg_type;
    attr->mem_type = para->mem_type;
    attr->is_continuous = false;
    attr->is_compound_page = ((DEVMM_SIDE_TYPE == DEVMM_SIDE_MASTER) || (DEVMM_SIDE_TYPE == DEVMM_SIDE_HOST_AGENT)) ?
        true : false;
    attr->is_giant_page = (para->pg_type == MEM_GIANT_PAGE_TYPE) ? true : false;

    if (para->side == MEM_HOST_NUMA_SIDE) {
        attr->numa_id = (para->host_numa_id == -1) ? -1 : (para->host_numa_id + 1);
    } else {
        attr->numa_id = 0;
    }
}

static int devmm_host_numa_id_check(u32 side, u32 host_numa_id)
{
    if (side != MEM_HOST_NUMA_SIDE) {
        return 0;
    }

    if ((host_numa_id != -1) && ((host_numa_id >= SVM_MASTER_NUMA_MAX) || (!node_online(host_numa_id)))) {
        devmm_drv_err("Invalid numa id. (side=%u; numa=%u; numa_max=%u)\n",
            side, host_numa_id, SVM_MASTER_NUMA_MAX);
        return -EINVAL;
    }
    return 0;
}

static int devmm_ioctl_mem_create_para_check(struct devmm_mem_create_para *para, u32 devid)
{
    if (para->size == 0) {
        devmm_drv_err("Size is zero.\n");
        return -EINVAL;
    }
    if ((para->pg_type == MEM_NORMAL_PAGE_TYPE) && KA_DRIVER_IS_ALIGNED(para->size, devmm_svm->host_page_size) == false) {
           /* The log cannot be modified, because in the failure mode library. */
           devmm_drv_err("Size should aligned by granularity_size. (size=%llu; granularity_size=%u)\n",
               para->size, devmm_svm->host_page_size);
           return -EINVAL;
    }

    if ((para->pg_type != MEM_NORMAL_PAGE_TYPE) && KA_DRIVER_IS_ALIGNED(para->size, devmm_svm->device_hpage_size) == false) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Size should aligned by granularity_size. (size=%llu; granularity_size=%u)\n",
            para->size, devmm_svm->device_hpage_size);
        return -EINVAL;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (para->flag != 0) {
        devmm_drv_err("Flag shoule be zero.\n");
        return -EINVAL;
    }

    if (para->side >= MEM_MAX_SIDE) {
        devmm_drv_err("Invalid side type. (side=%u)\n", para->side);
        return -EINVAL;
    }

    if (devmm_host_numa_id_check(para->side, para->host_numa_id) != 0) {
        return -EINVAL;
    }

    if ((para->side == MEM_HOST_SIDE || para->side == MEM_HOST_NUMA_SIDE) && (devid != uda_get_host_id())) {
        devmm_drv_err("Invalid side devid. (side=%u; devid=%u)\n", para->side, devid);
        return -EINVAL;
    }

    if (para->pg_type >= MEM_MAX_PAGE_TYPE) {
        devmm_drv_err("Invalid page type. (page_type=%u)\n", para->pg_type);
        return -EINVAL;
    }

    if (para->mem_type >= MEM_MAX_TYPE) {
        devmm_drv_err("Invalid mem type. (mem_type=%u)\n", para->mem_type);
        return -EINVAL;
    }

    /* Support later */
    if (devid == SVM_HOST_AGENT_ID) {
#ifndef EMU_ST
        devmm_drv_run_info("Not support host agent.\n");
#endif
        return -EOPNOTSUPP;
    }

    return 0;
}

int devmm_ioctl_mem_create(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_create_para *para = &arg->data.mem_create_para;
    struct devmm_phy_addr_attr attr = {0};
    int ret;

    ret = devmm_ioctl_mem_create_para_check(para, arg->head.devid);
    if (ret != 0) {
        return ret;
    }

    devmm_mem_create_phy_addr_attr_pack(arg, &attr);
    if (para->side == MEM_HOST_SIDE || para->side == MEM_HOST_NUMA_SIDE) {
        return devmm_master_mem_create(svm_proc, &attr, para->size, &para->pg_num, &para->id);
    } else {
        ret = devmm_agent_mem_create(svm_proc, &attr, para->size, &para->pg_num, &para->id);
        if (ret != 0) {
            u32 mem_type = ((para->mem_type == MEM_HBM_TYPE) || (para->mem_type == MEM_P2P_HBM_TYPE)) ?
                MEM_INFO_TYPE_HBM_SIZE : MEM_INFO_TYPE_DDR_SIZE;

            devmm_get_dev_mem_dfx(svm_proc, mem_type, arg);
            devmm_dev_mem_stats_log_show(arg->head.logical_devid);
        }
        return ret;
    }
}

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
 * Create: 2023-12-4
 */
#include <linux/uaccess.h>

#include "ascend_hal_define.h"
#include "dbl/feature_loader.h"

#include "event_sched_inner.h"
#include "dms_interface.h"
#include "devdrv_manager_comm.h"
#ifndef TSDRV_UT
#include "devdrv_common.h"
#endif

#include "tsdrv_log.h"
#include "devdrv_nsm.h"
#include "devdrv_interface.h"

/* for shrid pa addr manage */
#define SHRID_HWTS_BASE_ADDR  0xBA700000ULL
#define SHRID_ASCEND310P_CHIP_ADDR_OFFSET   0x8000000000ULL
#define SHRID_HWTS_EVENT_BASE_ADDR   0x90000ULL
#define SHRID_HWTS_NOTIFY_BASE_ADDR  0x94000ULL

#ifndef TSDRV_UT
static int shr_id_get_notify_base_addr(u32 devid, u32 remotePhyId, u32 id_type, u64 *baseAddr)
{
    int topology_type, ret;
    size_t size;

    ret = dms_get_dev_topology(devid, remotePhyId, &topology_type);
    if (ret != 0) {
        TSDRV_PRINT_ERR("Get topology_type failed. (devid=%u; remote_devid=%u; ret=%d)\n", devid, remotePhyId, ret);
        return ret;
    }

    if ((topology_type == TOPOLOGY_HCCS) || (topology_type == TOPOLOGY_SIO) || (topology_type == TOPOLOGY_HCCS_SW)) {
        struct devdrv_info *info;

        info = devdrv_manager_get_devdrv_info(remotePhyId);
        if (info == NULL) {
            TSDRV_PRINT_ERR("Get chipid failed. (remotePhyId=%u)\n", remotePhyId);
            return -EINVAL;
        }

        *baseAddr = SHRID_HWTS_BASE_ADDR + SHRID_ASCEND310P_CHIP_ADDR_OFFSET * info->chip_id;
        TSDRV_PRINT_DEBUG("Notify info. (devid=%u; remote_devid=%u; chip=%u; topology_type=%d)\n",
            devid, remotePhyId, info->chip_id, topology_type);
    } else {
        ret = devdrv_get_p2p_addr(devid, remotePhyId, DEVDRV_P2P_IO_HWTS, baseAddr, &size);
        if (ret != 0) {
            TSDRV_PRINT_ERR("Call devdrv_get_p2p_addr failed. (devid=%u; remotePhyId=%u; ret=%d)\n",
                devid, remotePhyId, ret);
            return ret;
        }

        TSDRV_PRINT_DEBUG("Call devdrv_get_p2p_addr ok. (devid=%u; remote_devid=%u; size=0x%lx)\n",
            devid, remotePhyId, size);
    }

    *baseAddr += (id_type == IPC_EVENT_TYPE) ? SHRID_HWTS_EVENT_BASE_ADDR : SHRID_HWTS_NOTIFY_BASE_ADDR;
    TSDRV_PRINT_DEBUG("type id base info. (devid=%u; remote_devid=%u)\n", devid, remotePhyId);
    return 0;
}

static int shr_id_type_trans[SHR_ID_TYPE_MAX] = {
    [SHR_ID_NOTIFY_TYPE] = NOTIFY_ID_TYPE,
    [SHR_ID_EVENT_TYPE] = IPC_EVENT_TYPE,
};

int shr_id_event_update(unsigned int devid, struct sched_published_event_info *event_info,
    struct sched_published_event_func *event_func)
{
    struct drvShrIdInfo *info = NULL;
    u64 baseAddr, notify_addr;
    u32 id_type, shr_id;
    int ret;

    if ((event_info->subevent_id != DRV_SUBEVENT_TRS_SHR_ID_CONFIG_MSG) &&
        (event_info->subevent_id != DRV_SUBEVENT_TRS_SHR_ID_DECONFIG_MSG)) {
        return 0;
    }

    if (event_info->msg == NULL) {
        TSDRV_PRINT_ERR("Msg is NULL.\n");
        return -EINVAL;
    }

    info = (struct drvShrIdInfo *)(event_info->msg + sizeof(struct event_sync_msg));
    if ((info->id_type != SHR_ID_EVENT_TYPE) && (info->id_type != SHR_ID_NOTIFY_TYPE)) {
        TSDRV_PRINT_ERR("Type is invalid. (type=%u)\n", info->id_type);
        return -EINVAL;
    }

    shr_id = (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) ? (info->shrid & (~IPC_EVENT_MASK)) : info->shrid;
    id_type = (u32)shr_id_type_trans[info->id_type];
    TSDRV_PRINT_DEBUG("recv event msg. (devid=%u; target_devid=%u; type=%u; id=%u; sub_eventid=%u)\n", devid,
        info->devid, info->id_type, shr_id, event_info->subevent_id);

    if (event_info->subevent_id == DRV_SUBEVENT_TRS_SHR_ID_DECONFIG_MSG) {
        return 0;
    }

    if (!devdrv_ipc_notify_is_belong_to_proc(info->devid, info->tsid, current->tgid, (int)id_type, shr_id)) {
        TSDRV_PRINT_ERR("Id invalid. (devid=%u; rudevid=%u; tsid=%u; pid=%d; type=%u; shrid=%u)\n",
            devid, info->devid, info->tsid, current->tgid, id_type, info->shrid);
        return -EACCES;
    }

    ret = shr_id_get_notify_base_addr(devid, info->devid, id_type, &baseAddr);
    if (ret != 0) {
        return ret;
    }

    notify_addr = baseAddr + 0x8 * shr_id;             /* 0x8: offset */
    info->rsv[0] = (u32)(notify_addr & 0xffffffffULL); /* 0xffffffffULL is low 32 bit */
    info->rsv[1] = (u32)(notify_addr >> 32);           /* 32 is high bit */

    TSDRV_PRINT_DEBUG("Notify info. (devid=%u; remote_devid=%u; type=%u; shrid=%u; sub_id=%u)\n",
        devid, info->devid, id_type, info->shrid, event_info->subevent_id);

    return 0;
}

int shr_id_event_init(void)
{
    TSDRV_PRINT_INFO("shr_id_event_update register\n");
    return sched_register_event_pre_proc_handle(EVENT_DRV_MSG, shr_id_event_update);
}
DECLAER_FEATURE_AUTO_INIT(shr_id_event_init, FEATURE_LOADER_STAGE_5);

void shr_id_event_uninit(void)
{
    sched_unregister_event_pre_proc_handle(EVENT_DRV_MSG, shr_id_event_update);
}
DECLAER_FEATURE_AUTO_UNINIT(shr_id_event_uninit, FEATURE_LOADER_STAGE_5);
#else
int shr_id_event_update(unsigned int devid, struct sched_published_event_info *event_info,
    struct sched_published_event_func *event_func)
{
    return 0;
}
#endif

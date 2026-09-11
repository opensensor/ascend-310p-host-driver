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
* Create: 2022-11-15
*/
#include <linux/fs.h>
#include <linux/kallsyms.h>

#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms_mem_info.h"
#include "dms_mem_common.h"
#include "kernel_version_adapt.h"
#include "devdrv_common.h"
#include "chip_config.h"

/* | CHIP  |    SYSTEM NODE ID   |
 * | devid |   0   |   0      1  |
 * | 910   |          X          |
 * | 910B  |  0 31 |  0 31  1 63 |
 */
int g_mem_system_nid[][MEM_SYSTEM_HBM_NUMA_NUM] = {
#ifdef CFG_MEMORY_NODE_CONFIG_3
    {0, 31},
    {1, 63},
#endif
};

static int mem_system_nid_check(u32 dev_id)
{
#ifdef CFG_MEMORY_NODE_CONFIG_3
    u32 nid_size = sizeof(g_mem_system_nid) / sizeof(g_mem_system_nid[0]);

    if (dev_id >= nid_size) {
        dms_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
#endif

    return 0;
}

int mem_get_hbm_node_info(u32 dev_id, u32 type, struct mem_info *info)
{
    int hbm_node_num = 0, node_id = 0, ret = 0;
    u64 total_size_tmp = 0, free_size_tmp = 0, hpage_2M_num = 0;
    struct mem_hpage_1G hpage_1G_info = {0};
    int service_nid[MEM_SERVICE_HBM_NUMA_NUM] = {0};
    int i;

    if (type == MEM_SERVICE) {
        hbm_node_num = dbl_nid_get_nid(dev_id, DBL_MEMTYPE_HBM, DBL_SUB_MEMTYPE_AI,
            service_nid, MEM_SERVICE_HBM_NUMA_NUM);
        if (hbm_node_num < 0) {
            dms_err("Failed to obtain the NID from the dbl. (dev_id=%u; ret=%d)\n", dev_id, hbm_node_num);
            return hbm_node_num;
        } else if (hbm_node_num > MEM_SERVICE_HBM_NUMA_NUM) {
            hbm_node_num = MEM_SERVICE_HBM_NUMA_NUM;
        }
    } else if (type == MEM_SYSTEM) {
        hbm_node_num = MEM_SYSTEM_HBM_NUMA_NUM;

        ret = mem_system_nid_check(dev_id);
        if (ret != 0) {
            return ret;
        }
    } else {
        return -EOPNOTSUPP;
    }

    for (i = 0; i < hbm_node_num; i++) {
        node_id = (type == MEM_SYSTEM) ? g_mem_system_nid[dev_id][i] : service_nid[i];
        if (node_online(node_id) == 0) {
            continue;
        }
        ret = mem_get_info_single(node_id, &total_size_tmp, &free_size_tmp);
        if (ret != 0) {
            dms_err("Get mem info failed. (dev_id=%u; node_id=%d; ret=%d)\n", dev_id, node_id, ret);
            return -EINVAL;
        }

#ifdef CFG_FEATURE_GET_HPAGE_2M_INFO
        ret = mem_get_hpage_info(node_id, &hpage_2M_num, &hpage_1G_info);
        if (ret != 0) {
            dms_err("Get hpage info failed. (dev_id=%u; node_id=%d; ret=%d)\n", dev_id, node_id, ret);
            return -EINVAL;
        }
#endif

        info->total_size += total_size_tmp;
        info->free_size += (free_size_tmp + hpage_1G_info.huge_1G_free * HPAGE_1G_SIZE +
                            hpage_1G_info.huge_2M_free * HPAGE_2M_SIZE);
        info->hpage_2M_num += hpage_2M_num;
        info->hpage_1G_num += hpage_1G_info.huge_1G_total;
    }

    info->use_size = info->total_size - info->free_size;
    if (info->total_size != 0) {
        info->mem_util = info->use_size * DIGITAL_NUM_TO_PER / info->total_size;
    } else {
        info->mem_util = 0;
    }

    return ret;
}

devmng_get_vdev_hbm_info g_devmng_get_vdev_hbm_info = NULL;
STATIC int mem_get_vdev_hbm_info_from_devmm(
    u32 devid, u32 vfid, u32 huge_flag, u64 *free_size, u64 *total_size)
{
    if (g_devmng_get_vdev_hbm_info == NULL) {
        g_devmng_get_vdev_hbm_info = (devmng_get_vdev_hbm_info)(
            uintptr_t)__kallsyms_lookup_name("devmm_get_vdev_hbm_info");
        if (g_devmng_get_vdev_hbm_info == NULL) {
            return -EFAULT;
        }
    }
    return g_devmng_get_vdev_hbm_info(devid, vfid, huge_flag, free_size, total_size);
}

int mem_get_vdev_hbm_info(u32 dev_id, u32 vfid, struct mem_info *info)
{
    u64 total_size_tmp = 0;
    u64 free_size_tmp = 0;
    int ret;

    /* read small page size */
    ret = mem_get_vdev_hbm_info_from_devmm(dev_id, vfid, 0, &free_size_tmp, &total_size_tmp);
    if (ret != 0) {
        dms_err("Read small page from devmm failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }
    info->total_size += total_size_tmp;
    info->free_size += free_size_tmp;

    /* read huge page size */
    ret = mem_get_vdev_hbm_info_from_devmm(dev_id, vfid, 1, &free_size_tmp, &total_size_tmp);
    if (ret != 0) {
        dms_err("Read huge page from devmm failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }
    info->total_size += total_size_tmp;
    info->free_size += free_size_tmp;

    info->total_size = BYTES_TO_KBYTE(info->total_size);
    info->free_size = BYTES_TO_KBYTE(info->free_size);
    info->use_size = info->total_size - info->free_size;
    if (info->total_size != 0) {
        info->mem_util = info->use_size * DIGITAL_NUM_TO_PER / info->total_size; // 100 change to percentage
    } else {
        info->mem_util = 0;
    }

    return 0;
}

int mem_get_hbm_medium_info(u32 dev_id, struct mem_info *hbm_info)
{
    int ret;
    struct mem_info service_mem_info = {0};
    struct mem_info system_mem_info = {0};
#ifdef CFG_FEATURE_PG
    struct devdrv_info *dev_info = NULL;
#endif

    ret = mem_get_hbm_node_info(dev_id, MEM_SERVICE, &service_mem_info);
    if (ret != 0) {
        dms_err("Failed to obtain service memory info. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = mem_get_hbm_node_info(dev_id, MEM_SYSTEM, &system_mem_info);
    if (ret != 0) {
        dms_err("Failed to obtain system memory info. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

#ifdef CFG_FEATURE_PG
    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("Failed to obtain dev_info data. (dev_id=%u)\n", dev_id);
        return -EFAULT;
    }

    dms_debug("Obtains the number of hbm granules. (dev_id=%u; num=%u)\n",
        dev_id, dev_info->pg_info.comPgInfo.hbmPara.minNum);
    /* One is 16 GB, 1024*1024:GB to KB */
    hbm_info->total_size = dev_info->pg_info.comPgInfo.hbmPara.minNum * 16 * 1024 * 1024;
#else
    hbm_info->total_size = service_mem_info.total_size + system_mem_info.total_size;
#endif
    hbm_info->free_size = service_mem_info.free_size;
    hbm_info->use_size = hbm_info->total_size - hbm_info->free_size;
    hbm_info->hpage_2M_num = service_mem_info.hpage_2M_num + system_mem_info.hpage_2M_num;
    if (hbm_info->total_size != 0) {
        hbm_info->mem_util = hbm_info->use_size * DIGITAL_NUM_TO_PER / hbm_info->total_size;
    } else {
        hbm_info->mem_util = 0;
    }

    return 0;
}

int mem_get_dev_hbm_info(u32 dev_id, u32 type, struct mem_info *hbm_info)
{
    int ret;

    switch (type) {
        case MEM_SERVICE:
        case MEM_SYSTEM:
            ret = mem_get_hbm_node_info(dev_id, type, hbm_info);
            break;
        case MEM_MEDIUM:
            ret = mem_get_hbm_medium_info(dev_id, hbm_info);
            break;
        default:
            return -EOPNOTSUPP;
    }
    if (ret != 0) {
        dms_err("Failed to obtain dev hbm info. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return 0;
}

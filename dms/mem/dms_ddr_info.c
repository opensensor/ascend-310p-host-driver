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
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include "ka_memory_pub.h"
#include "ka_fs_pub.h"

#include "securec.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms_mem_info.h"
#include "dms_mem_common.h"
#include "devdrv_user_common.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_manager_container.h"
#include "kernel_version_adapt.h"
#include "chip_config.h"

#define MEM_CGROUP_SCANF_NUM 1
#define MEM_DEV_CGROUP_LIMIT_PATH       "/sys/fs/cgroup/memory/usermemory/memory.limit_in_bytes"
#define MEM_DEV_CGROUP_USAGE_PATH       "/sys/fs/cgroup/memory/usermemory/memory.usage_in_bytes"
#define MEM_DEV_CGROUP_MAX_USAGE_PATH   "/sys/fs/cgroup/memory/usermemory/memory.max_usage_in_bytes"
#define MEM_VDEV_CGROUP_LIMIT_PATH      "/sys/fs/cgroup/memory/usermemory/dev%u/vf%u/memory.limit_in_bytes"
#define MEM_VDEV_CGROUP_USAGE_PATH      "/sys/fs/cgroup/memory/usermemory/dev%u/vf%u/memory.usage_in_bytes"
#define MEM_VDEV_CGROUP_MAX_USAGE_PATH  "/sys/fs/cgroup/memory/usermemory/dev%u/vf%u/memory.max_usage_in_bytes"
#define MEM_VF_CGROUP_LIMIT_PATH        "/sys/fs/cgroup/memory/%smemory.limit_in_bytes"
#define MEM_VF_CGROUP_USAGE_PATH        "/sys/fs/cgroup/memory/%smemory.usage_in_bytes"
#define MEM_VF_CGROUP_MAX_USAGE_PATH    "/sys/fs/cgroup/memory/%smemory.max_usage_in_bytes"
#define MEM_VF_CGROUP_FORMAT_PATH       "usermemory/dev%u/"

#ifdef CFG_FEATURE_VFIO_SOC
    #define KBYTE_TO_BYTES(b) ((b) << 10ULL)
    STATIC int mem_get_data_from_meminfo(const char *path, const char *key, u64 *data);
#endif

STATIC int mem_get_vf_cgroup_path_name(u32 dev_id, u32 vfid, char *limit_path, char *usage_path, char *max_usage_path)
{
    int ret;
    char concat[MEMINFO_PATH_LEN] = "";

#ifndef CFG_FEATURE_DEVICE_CONTAINER
    ret = sprintf_s(concat, MEMINFO_PATH_LEN, MEM_VF_CGROUP_FORMAT_PATH, dev_id);
    if (ret <= 0) {
        goto out;
    }
#endif
    ret = sprintf_s(limit_path, MEMINFO_PATH_LEN, MEM_VF_CGROUP_LIMIT_PATH, concat);
    if (ret <= 0) {
        goto out;
    }
    ret = sprintf_s(usage_path, MEMINFO_PATH_LEN, MEM_VF_CGROUP_USAGE_PATH, concat);
    if (ret <= 0) {
        goto out;
    }
    ret = sprintf_s(max_usage_path, MEMINFO_PATH_LEN, MEM_VF_CGROUP_MAX_USAGE_PATH, concat);
    if (ret <= 0) {
        goto out;
    }
    return 0;

out:
    dms_err("sprintf_s error. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
    return -EINVAL;
}

STATIC int mem_get_pf_cgroup_path_name(u32 dev_id, u32 vfid, char *limit_path, char *usage_path, char *max_usage_path)
{
    int ret;

#ifdef CFG_FEATURE_DEVICE_CONTAINER
    if (devdrv_manager_container_is_in_container()) {
        ret = mem_get_vf_cgroup_path_name(dev_id, vfid, limit_path, usage_path, max_usage_path);
        if (ret != 0) {
            goto out;
        }
        return 0;
    }
#endif

    ret = sprintf_s(limit_path, MEMINFO_PATH_LEN, MEM_DEV_CGROUP_LIMIT_PATH);
    if (ret <= 0) {
        goto out;
    }
    ret = sprintf_s(usage_path, MEMINFO_PATH_LEN, MEM_DEV_CGROUP_USAGE_PATH);
    if (ret <= 0) {
        goto out;
    }
    ret = sprintf_s(max_usage_path, MEMINFO_PATH_LEN, MEM_DEV_CGROUP_MAX_USAGE_PATH);
    if (ret <= 0) {
        goto out;
    }
    return 0;

out:
    dms_err("sprintf_s error. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
    return -EINVAL;
}

STATIC int mem_get_cgroup_path_name(u32 dev_id, u32 vfid, char *limit_path, char *usage_path, char *max_usage_path)
{
    int ret;

    if (vfid == 0) {
        if (devdrv_manager_is_pf_device(dev_id)) {
            ret = mem_get_pf_cgroup_path_name(dev_id, vfid, limit_path, usage_path, max_usage_path);
            if (ret != 0) {
                goto out;
            }
        } else {
            ret = mem_get_vf_cgroup_path_name(dev_id, vfid, limit_path, usage_path, max_usage_path);
            if (ret != 0) {
                goto out;
            }
        }
    } else {
        ret = sprintf_s(limit_path, MEMINFO_PATH_LEN, MEM_VDEV_CGROUP_LIMIT_PATH, dev_id, vfid);
        if (ret <= 0) {
            goto out;
        }
        ret = sprintf_s(usage_path, MEMINFO_PATH_LEN, MEM_VDEV_CGROUP_USAGE_PATH, dev_id, vfid);
        if (ret <= 0) {
            goto out;
        }
        ret = sprintf_s(max_usage_path, MEMINFO_PATH_LEN, MEM_VDEV_CGROUP_MAX_USAGE_PATH, dev_id, vfid);
        if (ret <= 0) {
            goto out;
        }
    }

    return 0;
out:
    dms_err("sprintf_s error. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
    return -EINVAL;
}

STATIC int mem_get_cgroup_ddr_size(char *cgroup_path, u64 *ddr_size)
{
    char file_string[MEMINFO_PATH_LEN] = {0};
    struct file *fp = NULL;
    loff_t pos = 0;
    int ret;

    fp = filp_open(cgroup_path, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL((void const *)fp)) {
        dms_err("File open failed. (file=%s; errno=%ld)\n", cgroup_path, PTR_ERR((void const *)fp));
        return -EINVAL;
    }

    if (mem_read_file(fp, file_string, MEMINFO_PATH_LEN - 1, &pos) < 0) {
        dms_err("File filestring not right. (file=%s; pos=%lld)\n", cgroup_path, pos);
        (void)filp_close(fp, NULL);
        return -EINVAL;
    }

    ret = sscanf_s(file_string, "%llu", ddr_size);
    if (ret != MEM_CGROUP_SCANF_NUM) {
        dms_err("File sscanf not right. (file=%s; ret=%d)\n", cgroup_path, ret);
        (void)filp_close(fp, NULL);
        return -EINVAL;
    }
    (void)filp_close(fp, NULL);
    return 0;
}

int mem_get_cgroup_info(u32 dev_id, u32 vfid, struct cgroup_mem_info *cg_info)
{
    char limit_path[MEMINFO_PATH_LEN] = {0};
    char usage_path[MEMINFO_PATH_LEN] = {0};
    char max_usage_path[MEMINFO_PATH_LEN] = {0};
    int ret;
#ifdef CFG_FEATURE_VFIO_SOC
    u64 mem_total = 0;
#endif

    ret = mem_get_cgroup_path_name(dev_id, vfid, limit_path, usage_path, max_usage_path);
    if (ret != 0) {
        dms_err("Get cgroup path failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }
    ret = mem_get_cgroup_ddr_size(limit_path, &cg_info->limit_in_bytes);
    if (ret != 0) {
        dms_err("Get cgroup ddr size failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }
    ret = mem_get_cgroup_ddr_size(usage_path, &cg_info->usage_in_bytes);
    if (ret != 0) {
        dms_err("Get cgroup ddr size failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }
    ret = mem_get_cgroup_ddr_size(max_usage_path, &cg_info->max_usage_in_bytes);
    if (ret != 0) {
        dms_err("Get cgroup ddr size failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

#ifdef CFG_FEATURE_VFIO_SOC
    /*
    * If a container is started with the -m parameter, the value of memory.limit_in_bytes will be valid.
    * If a container is started with no -m parameter, the value of memory.limit_in_bytes will be invalid,
    * which is a big number.
    **/
    ret = mem_get_data_from_meminfo("/proc/meminfo", "MemTotal", &mem_total);
    if (ret != 0) {
        dms_err("Failed to invoke mem_get_data_from_meminfo. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (cg_info->limit_in_bytes > KBYTE_TO_BYTES(mem_total)) {
        cg_info->limit_in_bytes = KBYTE_TO_BYTES(mem_total);
    }
#endif
    return 0;
}

#ifdef CFG_FEATURE_RC_MODE
STATIC int mem_get_tsnode_cdm_from_numa(u32 dev_id, u64 *total_size, u64 *free_size)
{
    int ret, i, j, nid_num;
    int nid_id[DBL_NUMA_ID_MAX_NUM] = { 0 };
    u32 query_type[] = {DBL_SUB_MEMTYPE_TS, DBL_SUB_MEMTYPE_P2P};
    u64 total_size_tmp, free_size_tmp;

    for (i = 0; i < sizeof(query_type) / sizeof(query_type[0]); i++) {
        nid_num = dbl_nid_get_nid(dev_id, DBL_MEMTYPE_ALL, query_type[i], nid_id, DBL_NUMA_ID_MAX_NUM);
        for (j = 0; j < nid_num; j++) {
            total_size_tmp = 0;
            free_size_tmp = 0;
            ret = mem_get_info_single(nid_id[j], &total_size_tmp, &free_size_tmp);
            if (ret != 0) {
                dms_err("Failed to obtain the memory info. (dev_id=%u; ret=%d)\n", dev_id, ret);
                return -EINVAL;
            }
            /* The memory has been allocated and is unavailable to the user. */
            *total_size += free_size_tmp;
            *free_size += 0;
        }
    }
    return 0;
}
#else
devmng_get_vdev_ddr_info g_devmng_get_vdev_ddr_info = NULL;
STATIC int mem_get_vdev_ddr_info_from_devmm(
    u32 dev_id, u32 vfid, u32 huge_flag, u64 *free_size, u64 *total_size)
{
    if (g_devmng_get_vdev_ddr_info == NULL) {
        g_devmng_get_vdev_ddr_info = (devmng_get_vdev_ddr_info)(
            uintptr_t)__kallsyms_lookup_name("devmm_get_vdev_ddr_info");
        if (g_devmng_get_vdev_ddr_info == NULL) {
            return -EFAULT;
        }
    }
    return g_devmng_get_vdev_ddr_info(dev_id, vfid, huge_flag, free_size, total_size);
}
#endif

int mem_get_tsnode_cdm(u32 dev_id, u32 vfid, u64 *total_size, u64 *free_size)
{
    int ret;
#ifdef CFG_FEATURE_RC_MODE
    *total_size = 0;
    *free_size = 0;
    if (!devdrv_manager_container_is_in_container()) {
        ret = mem_get_tsnode_cdm_from_numa(dev_id, total_size, free_size);
        if (ret != 0) {
            dms_err("Read cdm memory from numa failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
    }
#else
    u64 total_size_devmm_small = 0;
    u64 free_size_devmm_small = 0;
    u64 total_size_devmm_huge = 0;
    u64 free_size_devmm_huge = 0;

    if (devdrv_manager_is_pf_device(dev_id)) {
        /* get dev ddr from devmm reserve */
        ret = mem_get_vdev_ddr_info_from_devmm(dev_id, vfid, 0, &free_size_devmm_small, &total_size_devmm_small);
        if (ret != 0) {
            dms_err("Read small page from devmm failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
            return ret;
        }

        ret = mem_get_vdev_ddr_info_from_devmm(dev_id, vfid, 1, &free_size_devmm_huge, &total_size_devmm_huge);
        if (ret != 0) {
            dms_err("Read huge page from devmm failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
            return ret;
        }
    }

    *total_size = total_size_devmm_small + total_size_devmm_huge;
    *free_size = free_size_devmm_small + free_size_devmm_huge;
    *total_size = BYTES_TO_KBYTE(*total_size);
    *free_size = BYTES_TO_KBYTE(*free_size);
#endif
    return 0;
}

int mem_get_cgroup_and_cdm_memory(u32 dev_id, u32 vfid, u64 *total_size, u64 *free_size)
{
    int ret;
    u64 total_cdm = 0;
    u64 free_cdm = 0;
    struct cgroup_mem_info cg_info = {0};

    if ((vfid > VDAVINCI_MAX_VFID_NUM) || (total_size == NULL) || (free_size == NULL)) {
        dms_err("Invalid input para. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return -EINVAL;
    }

    /* get dev ddr total and free size by reading files */
    ret = mem_get_cgroup_info(dev_id, vfid, &cg_info);
    if (ret != 0) {
        dms_err("Get cgroup info failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    ret = mem_get_tsnode_cdm(dev_id, vfid, &total_cdm, &free_cdm);
    if (ret != 0) {
        dms_err("Get tsnode cdm memory failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    dms_debug("Get memory info success."
        "(dev_id=%u; vfid=%u; cg_total=%lluKB; cg_free=%lluKB; cdm_total=%lluKB; cdm_free=%lluKB)",
        dev_id, vfid, BYTES_TO_KBYTE(cg_info.limit_in_bytes),
        BYTES_TO_KBYTE(cg_info.limit_in_bytes) - BYTES_TO_KBYTE(cg_info.usage_in_bytes), total_cdm, free_cdm);

    *total_size = BYTES_TO_KBYTE(cg_info.limit_in_bytes) + total_cdm;
    *free_size = BYTES_TO_KBYTE(cg_info.limit_in_bytes) - BYTES_TO_KBYTE(cg_info.usage_in_bytes) + free_cdm;
    return 0;
}

static int mem_calculation_system_memory(u32 dev_id, u32 dev_num, u64 *system_memory)
{
#ifdef CFG_FEATURE_ADD_SYSTEM_MEMORY
    struct cgroup_mem_info cg_info = {0};
    struct mem_info node = {0};
    u64 total_size_tmp = 0;
    u64 free_size_tmp = 0;
    u32 i;
    int ret;

    for (i = 0; i < dev_num; i++) {
        ret = mem_get_info_single(i, &total_size_tmp, &free_size_tmp);
        if (ret != 0) {
            dms_err("Get mem info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return -EINVAL;
        }
        node.total_size += total_size_tmp;
        node.free_size += free_size_tmp;
    }
    node.use_size = node.total_size - node.free_size;

    ret = mem_get_cgroup_info(dev_id, 0, &cg_info);
    if (ret != 0) {
        dms_err("Get cgroup info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    /* In the SMP scenario, node is the sum of nodes 0 and 1.
       node_total = system_memory + reserved_memory + cgroup_limit
       node_use = reserved_memory + cproup_use */
    *system_memory = (node.total_size - BYTES_TO_KBYTE(cg_info.limit_in_bytes) -
        node.use_size + BYTES_TO_KBYTE(cg_info.usage_in_bytes)) / dev_num;
    return 0;
#else
    (void)dev_id;
    (void)dev_num;
    *system_memory = 0;
    return 0;
#endif
}

#define MEM_UTIL_MAX    100
int mem_get_dev_ddr_info(u32 dev_id, struct mem_info *info)
{
    int ret, i;
    u32 nid;
    u64 total_size_tmp = 0, free_size_tmp = 0, system_memory = 0, hpage_2M_num = 0;
    u32 dev_num = 0;
    struct mem_hpage_1G hpage_1G_info = {0};

    ret = devdrv_get_devnum(&dev_num);
    if (ret != 0 || dev_num == 0) {
        dms_err("Get devnum failed. (devid=%u; dev_num=%u; ret=%d)\n", dev_id, dev_num, ret);
        return -EINVAL;
    }

    for (i = 0; i < MEM_DDR_NUMA_NUM; i++) {
        nid = MEM_GET_NUMA_ID(dev_id, i, dev_num);
        if (node_online(nid) == 0) {
            continue;
        }

#ifdef CFG_FEATURE_GET_HPAGE_2M_INFO
        ret = mem_get_hpage_info(nid, &hpage_2M_num, &hpage_1G_info);
        if (ret != 0) {
            dms_err("Get hpage info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return -EINVAL;
        }
#endif

        ret = mem_get_info_single(nid, &total_size_tmp, &free_size_tmp);
        if (ret != 0) {
            dms_err("Get mem info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return -EINVAL;
        }

        info->hpage_2M_num += hpage_2M_num;
        info->hpage_1G_num += hpage_1G_info.huge_1G_total;
        info->total_size += total_size_tmp;
        info->free_size += (free_size_tmp + hpage_1G_info.huge_1G_free * HPAGE_1G_SIZE +
                            hpage_1G_info.huge_2M_free * HPAGE_2M_SIZE);
    }

    ret = mem_calculation_system_memory(dev_id, dev_num, &system_memory);
    if (ret != 0) {
        dms_err("Failed to calculate the system memory. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    info->use_size = info->total_size - info->free_size + system_memory;
    if (info->total_size != 0) {
        info->mem_util = (info->use_size * DIGITAL_NUM_TO_PER / info->total_size);
    } else {
        info->mem_util = 0;
    }

    if (info->mem_util > MEM_UTIL_MAX) {
        dms_warn("mem_util abnormal, device abnormal or not initialized. (dev_id=%u; mem_util=%d)\n",
            dev_id, info->mem_util);
        info->mem_util = MEM_UTIL_MAX;
    }

    return 0;
}

#ifdef CFG_FEATURE_VFIO_SOC
#define DEVMNG_MEMNFO_MAX_LEN   2048
STATIC int mem_get_data_from_meminfo(const char *path, const char *key, u64 *data)
{
    int ret;
    char *buffer = NULL;
    char *find_str = NULL;
    struct file *fp = NULL;
    loff_t pos = 0;

    fp = filp_open(path, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL(fp)) {
        dms_err("File open fail. (file=\"%s\", errno=%ld)\n", path, PTR_ERR(fp));
        return -ENOENT;
    }

    buffer = (char *)dbl_kzalloc(DEVMNG_MEMNFO_MAX_LEN, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buffer == NULL) {
        dms_err("Failed to invoke kzalloc.\n");
        ret = -ENOMEM;
        goto out_close;
    }

    ret = mem_read_file(fp, buffer, DEVMNG_MEMNFO_MAX_LEN - 1, &pos);
    if (ret < 0) {
        dms_err("Failed to invoke mem_read_file. (file=\"%s\")\n", path);
        goto out_kfree;
    }

    find_str = strstr(buffer, key);
    if (find_str == NULL) {
        dms_err("Can not find MemTotal. (key=\"%s\")\n", key);
        ret = -EINVAL;
        goto out_kfree;
    }

    /* The key words are just like "MemTotal:       65545752 kB". */
    ret = sscanf_s(find_str + strlen(key) + 1, " %llu", data);
    if (ret <= 0) {
        dms_err("Sscanf_s value not find. (key=\"%s\")\n", key);
        ret = -EINVAL;
        goto out_kfree;
    }
    ret = 0;

out_kfree:
    dbl_kfree(buffer);
    buffer = NULL;
out_close:
    filp_close(fp, NULL);
    return ret;
}

int mem_get_container_mem_info(u32 dev_id, struct mem_info *info)
{
    int ret;
    struct cgroup_mem_info container_cg_info = {0};

    ret = mem_get_cgroup_info(dev_id, 0, &container_cg_info);
    if (ret != 0) {
        dms_err("Get cgroup info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    info->use_size = BYTES_TO_KBYTE(container_cg_info.usage_in_bytes);
    info->total_size = BYTES_TO_KBYTE(container_cg_info.limit_in_bytes);
    if (info->total_size != 0) {
        info->mem_util = (info->use_size * DIGITAL_NUM_TO_PER / info->total_size);
    } else {
        info->mem_util = 0;
    }
    info->free_size = info->total_size - info->use_size;

    return 0;
}
#endif

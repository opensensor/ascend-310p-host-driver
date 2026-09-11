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
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "ka_memory_pub.h"

#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms_mem_info.h"
#include "dms_mem_common.h"
#include "securec.h"
#include "devdrv_user_common.h"
#include "uda.h"
#include "udis.h"
#include "dms_timer.h"
#include "devdrv_interface.h"
#include "dbl_mem_alloc_interface.h"
#include "chip_config.h"
#include "virtmng_interface.h"

#ifdef CFG_FEATURE_DEVICE_DEVID_CONVERT
#include "devdrv_manager_container.h"
#endif

#ifdef CFG_FEATURE_UDIS
STATIC u32 g_mem_info_timer[DEVDRV_MAX_DAVINCI_NUM];
STATIC struct udis_mem_info *g_hbm_mem_info[DEVDRV_MAX_DAVINCI_NUM];
STATIC dma_addr_t g_hbm_dma_addr[DEVDRV_MAX_DAVINCI_NUM];
#define DMS_MEM_NOTIFIER "dms_mem"
#define HBM_INFO_UDIS_TIMER_EXPIRE_MS 200
#endif

BEGIN_DMS_MODULE_DECLARATION(DMS_MEM_NAME)
BEGIN_FEATURE_COMMAND()
#ifdef SUPPORT_VDEV_MEM
ADD_FEATURE_COMMAND(DMS_MEM_NAME, DMS_MAIN_CMD_BASIC, DMS_GET_AI_DDR_INFO, NULL,
    "dmp_daemon", DMS_SUPPORT_ALL, dms_drv_get_ai_ddr_info)
#endif
ADD_FEATURE_COMMAND(DMS_MEM_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_DDR_MEM_INFO, NULL, NULL, DMS_SUPPORT_ALL,
    dms_get_ddr_mem_info)
ADD_FEATURE_COMMAND(DMS_MEM_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_HBM_MEM_INFO, NULL, NULL, DMS_SUPPORT_ALL,
    dms_get_hbm_mem_info)
ADD_FEATURE_COMMAND(DMS_MEM_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_CGROUP_MEM_INFO, NULL, NULL, DMS_SUPPORT_ALL,
    dms_get_cgroup_mem_info)
ADD_FEATURE_COMMAND(DMS_MEM_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_SERVICE_MEM_INFO, NULL, NULL, DMS_SUPPORT_ALL,
    dms_get_service_mem_info)
ADD_FEATURE_COMMAND(DMS_MEM_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_SYSTEM_MEM_INFO, NULL, NULL, DMS_SUPPORT_ALL,
    dms_get_system_mem_info)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

#ifdef CFG_FEATURE_UDIS
STATIC int dms_mem_scan_task(u64 user_data)
{
    int ret = 0;
    unsigned int udevid = user_data;
    struct mem_info mediem_info = {0};

    ret = mem_get_hbm_info(udevid, 0, MEM_MEDIUM, &mediem_info);
    if (ret != 0) {
        dms_err("Get hbm info failed, ret = %d.\n", ret);
        return ret;
    }
    g_hbm_mem_info[udevid]->medium_mem_info.total_size = mediem_info.total_size;
    g_hbm_mem_info[udevid]->medium_mem_info.free_size = mediem_info.free_size;

    return 0;
}

STATIC int dms_mem_info_task_unregister(unsigned int udevid)
{
    int ret = 0;
    struct device *dev = NULL;

    dev = uda_get_agent_device(udevid);
    if (dev == NULL) {
        dms_err("uda get agent device info failed. (dev_id=%u)\n", udevid);
        return -ENODEV;
    }

    ret = dms_timer_task_unregister(g_mem_info_timer[udevid]);
    if (ret != 0) {
        dms_err("Dms mem timer task unregister failed. (dev_id=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    ret = hal_kernel_unregister_udis_addr(udevid, UDIS_MODULE_DEVMNG, "hbm_mem_info");
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Dms mem udis unregister from udis failed. (dev_id=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    devdrv_dma_unmap_single(dev, g_hbm_dma_addr[udevid], sizeof(struct mem_info), DMA_BIDIRECTIONAL);

    if (g_hbm_mem_info[udevid] != NULL) {
        dbl_kfree(g_hbm_mem_info[udevid]);
        g_hbm_mem_info[udevid] = NULL;
    }
    dms_info("Dms udis mem unregister from timer. (dev_id=%u) \n", udevid);

    return 0;
}

STATIC int udis_mem_info_register(unsigned int udevid)
{
    int ret = 0;
    struct udis_addr_info addr_info;
    struct device *dev = NULL;

    dev = uda_get_agent_device(udevid);
    if (dev == NULL) {
        dms_err("uda get agent device info failed. (dev_id=%u)\n", udevid);
        return -ENODEV;
    }

    (void)sprintf_s(addr_info.name, UDIS_MAX_NAME_LEN, "hbm_mem_info");
    addr_info.update_type = UPDATE_PERIOD_LEVEL_1;
    addr_info.acc_ctrl = DMS_SUPPORT_ALL;
    addr_info.data_len = sizeof(struct mem_info);

    g_hbm_dma_addr[udevid] = devdrv_dma_map_single(dev, g_hbm_mem_info[udevid],
                                                    PAGE_SIZE, DMA_BIDIRECTIONAL);
    if (dma_mapping_error(dev, g_hbm_dma_addr[udevid]) != 0) {
        dms_err("dma map single failed. (dev_id=%u)\n", udevid);
        return -ENOMEM;
    }

    addr_info.dma_addr = g_hbm_dma_addr[udevid];

    ret = hal_kernel_register_udis_addr(udevid, UDIS_MODULE_DEVMNG, &addr_info);
    if (ret != 0) {
        devdrv_dma_unmap_single(dev, g_hbm_dma_addr[udevid], PAGE_SIZE, DMA_BIDIRECTIONAL);
        return ret;
    }

    return 0;
}

STATIC int dms_mem_info_task_register(unsigned int udevid)
{
    int ret;
    struct dms_timer_task task_property = {0};
    struct udis_mem_info *udis_mem_info = NULL;

    udis_mem_info = (struct udis_mem_info *)dbl_kmalloc(PAGE_SIZE, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (udis_mem_info == NULL) {
        dms_err("Kzalloc failed. (dev_id=%u)\n", udevid);
        return -ENOMEM;
    }
    g_hbm_mem_info[udevid] = udis_mem_info;

    task_property.expire_ms = HBM_INFO_UDIS_TIMER_EXPIRE_MS;
    task_property.count_ms = 0;
    task_property.user_data = udevid;
    task_property.handler_mode = COMMON_WORK;
    task_property.exec_task = dms_mem_scan_task;
    ret = dms_timer_task_register(&task_property, &g_mem_info_timer[udevid]);
    if (ret != 0) {
        dms_err("Dms mem info timer task register failed. (udevid=%u; ret=%d)\n", udevid, ret);
        goto free_hbm;
    }
    ret = udis_mem_info_register(udevid);
    if (ret != 0 && ret != -EOPNOTSUPP) {
        dms_ex_notsupport_err(ret, "udis mem info register failed. (udevid=%u; ret=%d)\n", udevid, ret);
        goto unreg_timer;
    }
    dms_info("Dms udis mem info register timer success. (dev_id=%u)\n", udevid);
    return 0;
unreg_timer:
    (void)dms_timer_task_unregister(g_mem_info_timer[udevid]);
free_hbm:
    dbl_kfree(g_hbm_mem_info[udevid]);
    g_hbm_mem_info[udevid] = NULL;
    return ret;
}

STATIC int dms_mem_notifier_func(unsigned int udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = dms_mem_info_task_register(udevid);
    } else if (action == UDA_UNINIT) {
        dms_mem_info_task_unregister(udevid);
    } else {
        return 0;
    }

    return ret;
}
#endif

int dms_mem_init(void)
{
#ifdef CFG_FEATURE_UDIS
    int ret = 0;
    struct uda_dev_type type = {0};
    uda_davinci_local_real_agent_type_pack(&type);
    ret = uda_notifier_register(DMS_MEM_NOTIFIER, &type, UDA_PRI3, dms_mem_notifier_func);
    if (ret != 0) {
        dms_err("register uda notifier for pf failed. (ret=%d)\n", ret);
        return ret;
    }

    uda_davinci_local_virtual_agent_type_pack(&type);
    ret = uda_notifier_register(DMS_MEM_NOTIFIER, &type, UDA_PRI3, dms_mem_notifier_func);
    if (ret != 0) {
        dms_err("register uda notifier for vf failed. (ret=%d)\n", ret);
        goto uda_phy_unregister;
    }
#endif
    CALL_INIT_MODULE(DMS_MEM_NAME);
    return 0;

#ifdef CFG_FEATURE_UDIS
uda_phy_unregister:
    uda_davinci_local_real_agent_type_pack(&type);
    (void)uda_notifier_unregister(DMS_MEM_NOTIFIER, &type);
    return ret;
#endif
}

void dms_mem_uninit(void)
{
    struct uda_dev_type type = {0};

    CALL_EXIT_MODULE(DMS_MEM_NAME);
#ifdef CFG_FEATURE_UDIS
    uda_davinci_local_virtual_agent_type_pack(&type);
    (void)uda_notifier_unregister(DMS_MEM_NOTIFIER, &type);

    uda_davinci_local_real_agent_type_pack(&type);
    (void)uda_notifier_unregister(DMS_MEM_NOTIFIER, &type);
#else
    (void)type;
#endif
}

STATIC int dms_mem_para_check(char *in, u32 in_len, u32 correct_in_len,
    char *out, u32 out_len, u32 correct_out_len)
{
    if ((in == NULL) || (in_len != correct_in_len)) {
        dms_err("Input char is NULL or in_len is wrong. (in_len=%u; correct_in_len=%u)\n",
            in_len, correct_in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != correct_out_len)) {
        dms_err("Output char is NULL or out_len is wrong. (out_len=%u; correct_out_len=%u)\n",
            out_len, correct_out_len);
        return -EINVAL;
    }

    return 0;
}

#ifndef DRV_SOFT_UT
STATIC int dms_trans_and_check_id(u32 logical_dev_id, u32 *pf_id, u32 *vfid)
{
#ifdef CFG_FEATURE_DEVICE_DEVID_CONVERT
    if (devdrv_manager_container_logical_id_to_physical_id(logical_dev_id, pf_id, vfid) != 0) {
        dms_err("Failed to transfer logical ID to physical ID. (dev_id=%u)\n", logical_dev_id);
        return -EINVAL;
    }
#else
    *pf_id = logical_dev_id;
#endif
    return 0;
}
#endif

#ifndef AOS_LLVM_BUILD
#ifdef SUPPORT_VDEV_MEM
int dms_drv_get_ai_ddr_info(void *feature, char *in, u32 in_len,
    char *out, u32 out_len)
{
    struct dms_get_ddr_info_in *input = NULL;
    struct dms_get_ddr_info_out info_out = {0};
    u64 total_size = 0;
    u64 free_size = 0;
    int ret;
    u32 phys_id = DEVDRV_MAX_DAVINCI_NUM + 1;

    ret = dms_mem_para_check(in, in_len, sizeof(struct dms_get_ddr_info_in),
        out, out_len, sizeof(struct dms_get_ddr_info_out));
    if (ret != 0) {
        dms_err("Input parameter check failed.\n");
        return ret;
    }

    input = (struct dms_get_ddr_info_in *)in;
    if ((input->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (input->vfid > VDAVINCI_MAX_VFID_NUM)) {
        dms_err("Device id or vf id invalid. (dev_id=%u; vfid=%u)\n", input->dev_id, input->vfid);
        return -EINVAL;
    }

    ret = dms_trans_and_check_id(input->dev_id, &phys_id, &input->vfid);
    if (ret != 0) {
        dms_err("Failed to transfer logical ID to physical ID. (dev_id=%u; ret=%d)\n",
            input->dev_id, ret);
        return ret;
    }

    ret = mem_get_tsnode_cdm(phys_id, input->vfid, &total_size, &free_size);
    if (ret != 0) {
        dms_err("Get dev cgroup ddr info failed. (dev_id=%u; vfid=%u; ret=%d)\n", phys_id, input->vfid, ret);
        return ret;
    }
    info_out.total = total_size;
    info_out.free = free_size;

    ret = memcpy_s(out, out_len, &info_out, sizeof(struct dms_get_ddr_info_out));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        return -EINVAL;
    }

    return 0;
}
#endif
#endif

STATIC int mem_get_ddr_info(u32 dev_id, struct mem_info *info)
{
    int ret = 0;

    if (devdrv_manager_is_pf_device(dev_id)) {
        ret = mem_get_dev_ddr_info(dev_id, info);
    } else {
#ifdef CFG_FEATURE_VFIO_SOC
        ret = mem_get_container_mem_info(dev_id, info);
#else
        return -EOPNOTSUPP;
#endif
    }
    if (ret != 0) {
        dms_err("Get ddr mem info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }

    return ret;
}

int dms_get_ddr_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    struct dms_mem_info_in *mem_in = NULL;
    struct mem_info info = {0};
    u32 phys_id = DEVDRV_MAX_DAVINCI_NUM + 1;

    ret = dms_mem_para_check(in, in_len, sizeof(struct dms_mem_info_in), out, out_len, sizeof(struct dms_mem_info_out));
    if (ret != 0) {
        dms_err("Input parameter check failed.\n");
        return ret;
    }

    mem_in = (struct dms_mem_info_in *)in;
    if ((mem_in->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (mem_in->vfid > VDAVINCI_MAX_VFID_NUM)) {
        dms_err("Device id or vf id invalid. (dev_id=%u; vfid=%u)\n", mem_in->dev_id, mem_in->vfid);
        return -EINVAL;
    }

    ret = dms_trans_and_check_id(mem_in->dev_id, &phys_id, &mem_in->vfid);
    if (ret != 0) {
        dms_err("Failed to transfer logical ID to physical ID. (dev_id=%u; ret=%d)\n",
            mem_in->dev_id, ret);
        return ret;
    }

    ret = mem_get_ddr_info(phys_id, &info);
    if (ret != 0) {
        dms_err("Get ddr memory info failed. (dev_id=%u; vfid=%u; ret=%d)\n", mem_in->dev_id, mem_in->vfid, ret);
        return ret;
    }

    ret = memcpy_s(out, out_len, &info, sizeof(struct mem_info));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

STATIC int mem_get_vf_mem_info(u32 dev_id, struct mem_info *info)
{
    u64 total_size_tmp = 0;
    u64 free_size_tmp = 0;
    u64 hpage_2M_num = 0;
    struct mem_hpage_1G hpage_1G_info = {0};
    int nid_id[DBL_NUMA_ID_MAX_NUM] = {-1};
    int nid_num = 0;
    int i;
    int ret;

    nid_num = dbl_nid_get_nid(dev_id, DBL_MEMTYPE_ALL, DBL_SUB_MEMTYPE_ALL, nid_id, DBL_NUMA_ID_MAX_NUM);
    if (nid_num < 0) {
        dms_err("Invalid number of nids. (drv_id=%u; nid_num=%d)\n", dev_id, nid_num);
        return -EINVAL;
    }

    for (i = 0; i < nid_num; i++) {
        ret = mem_get_info_single(nid_id[i], &total_size_tmp, &free_size_tmp);
        if (ret != 0) {
            dms_err("Failed to obtain the memory size. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }

#ifdef CFG_FEATURE_GET_HPAGE_2M_INFO
        ret = mem_get_hpage_info(nid_id[i], &hpage_2M_num, &hpage_1G_info);
        if (ret != 0) {
            dms_err("Get hpage info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return -EINVAL;
        }
#endif

        info->hpage_2M_num += hpage_2M_num;
        info->hpage_1G_num += hpage_1G_info.huge_1G_total;
        info->total_size += total_size_tmp;
        info->free_size += (free_size_tmp + hpage_1G_info.huge_1G_free * HPAGE_1G_SIZE +
                            hpage_1G_info.huge_2M_free * HPAGE_2M_SIZE);
    }

    info->use_size = info->total_size - info->free_size;
    if (info->total_size != 0) {
        info->mem_util = info->use_size * DIGITAL_NUM_TO_PER / info->total_size;
    } else {
        info->mem_util = 0;
    }

    return 0;
}

int mem_get_hbm_info(u32 dev_id, u32 vfid, u32 type, struct mem_info *info)
{
#ifdef CFG_FEATURE_HBM
    int ret = 0;

    if (vfid != 0) {
        ret = mem_get_vdev_hbm_info(dev_id, vfid, info);
    } else if (devdrv_manager_is_pf_device(dev_id)) {
        ret = mem_get_dev_hbm_info(dev_id, type, info);
    } else {
        ret = mem_get_vf_mem_info(dev_id, info);
    }
    if (ret != 0) {
        dms_err("Get hbm mem info failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
    }

    return ret;
#else
    return -EOPNOTSUPP;
#endif
}

int dms_get_hbm_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    struct dms_mem_info_in *mem_in = NULL;
    struct mem_info info = {0};

    ret = dms_mem_para_check(in, in_len, sizeof(struct dms_mem_info_in), out, out_len, sizeof(struct dms_mem_info_out));
    if (ret != 0) {
        dms_err("Input parameter check failed.\n");
        return ret;
    }

    mem_in = (struct dms_mem_info_in *)in;
    if ((mem_in->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (mem_in->vfid > VDAVINCI_MAX_VFID_NUM)) {
        dms_err("Device id or vf id invalid. (dev_id=%u; vfid=%u)\n", mem_in->dev_id, mem_in->vfid);
        return -EINVAL;
    }
    ret = mem_get_hbm_info(mem_in->dev_id, mem_in->vfid, MEM_MEDIUM, &info);
    if (ret != 0) {
        dms_err("Get hbm memory info failed. (dev_id=%u; vfid=%u; ret=%d)\n", mem_in->dev_id, mem_in->vfid, ret);
        return ret;
    }

    ret = memcpy_s(out, out_len, &info, sizeof(struct mem_info));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

int dms_get_cgroup_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    struct dms_mem_info_in *mem_in = NULL;
    struct cgroup_mem_info info = {0};
    u32 phys_id = DEVDRV_MAX_DAVINCI_NUM + 1;

    ret = dms_mem_para_check(in, in_len, sizeof(struct dms_mem_info_in),
        out, out_len, sizeof(struct dms_cgroup_mem_info_out));
    if (ret != 0) {
        dms_err("Input parameter check failed.\n");
        return ret;
    }

    mem_in = (struct dms_mem_info_in *)in;
    if ((mem_in->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (mem_in->vfid > VDAVINCI_MAX_VFID_NUM)) {
        dms_err("Device id or vf id invalid. (dev_id=%u; vfid=%u)\n", mem_in->dev_id, mem_in->vfid);
        return -EINVAL;
    }

    ret = dms_trans_and_check_id(mem_in->dev_id, &phys_id, &mem_in->vfid);
    if (ret != 0) {
        dms_err("Failed to transfer logical ID to physical ID. (dev_id=%u; ret=%d)\n",
            mem_in->dev_id, ret);
        return ret;
    }

    ret = mem_get_cgroup_info(phys_id, mem_in->vfid, &info);
    if (ret != 0) {
        dms_err("Get cgroup memory info failed. (dev_id=%u; vfid=%u; ret=%d)\n", mem_in->dev_id, mem_in->vfid, ret);
        return ret;
    }

    ret = memcpy_s(out, out_len, &info, sizeof(struct cgroup_mem_info));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

STATIC int mem_get_node_memory_info(u32 dev_id, u32 vfid, u32 type, struct mem_info *info)
{
    int ret;

    if (!devdrv_manager_is_pf_device(dev_id)) {
        ret = mem_get_vf_mem_info(dev_id, info);
        return ret;
    }

#ifdef CFG_FEATURE_HBM
    ret = mem_get_hbm_node_info(dev_id, type, info);
    if (ret != 0) {
        dms_err("Failed to obtain hbm node info. (dev_id=%u; type=%u; ret=%d)\n", dev_id, type, ret);
        return ret;
    }
#endif

    return 0;
}

int dms_get_service_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    struct dms_mem_info_in *mem_in = NULL;
    struct mem_info info = {0};

    ret = dms_mem_para_check(in, in_len, sizeof(struct dms_mem_info_in), out, out_len, sizeof(struct dms_mem_info_out));
    if (ret != 0) {
        dms_err("Input parameter check failed.\n");
        return ret;
    }

    mem_in = (struct dms_mem_info_in *)in;
    if ((mem_in->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (mem_in->vfid > VDAVINCI_MAX_VFID_NUM)) {
        dms_err("Device id or vf id invalid. (dev_id=%u; vfid=%u)\n", mem_in->dev_id, mem_in->vfid);
        return -EINVAL;
    }
    ret = mem_get_node_memory_info(mem_in->dev_id, mem_in->vfid, MEM_SERVICE, &info);
    if (ret != 0) {
        dms_err("Failed to obtain the service memory. (dev_id=%u; vfid=%u; ret=%d)\n",
            mem_in->dev_id, mem_in->vfid, ret);
        return ret;
    }

    ret = memcpy_s(out, out_len, &info, sizeof(struct mem_info));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

int dms_get_system_mem_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    struct dms_mem_info_in *mem_in = NULL;
    struct mem_info info = {0};

    ret = dms_mem_para_check(in, in_len, sizeof(struct dms_mem_info_in), out, out_len, sizeof(struct dms_mem_info_out));
    if (ret != 0) {
        dms_err("Input parameter check failed.\n");
        return ret;
    }

    mem_in = (struct dms_mem_info_in *)in;
    if ((mem_in->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (mem_in->vfid > VDAVINCI_MAX_VFID_NUM)) {
        dms_err("Device id or vf id invalid. (dev_id=%u; vfid=%u)\n", mem_in->dev_id, mem_in->vfid);
        return -EINVAL;
    }

    if ((!devdrv_manager_is_pf_device(mem_in->dev_id)) || (mem_in->vfid != 0)) {
        return -EOPNOTSUPP;
    }

    ret = mem_get_node_memory_info(mem_in->dev_id, mem_in->vfid, MEM_SYSTEM, &info);
    if (ret != 0) {
        dms_err("Failed to obtain the system memory. (dev_id=%u; ret=%d)\n", mem_in->dev_id, ret);
        return ret;
    }

    ret = memcpy_s(out, out_len, &info, sizeof(struct mem_info));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

int mem_get_dev_sliceable_memory(u32 dev_id, struct mem_info *info)
{
#ifdef CFG_FEATURE_GET_MEM_FROM_VIRMNG
    u64 total_size_tmp = 0;
    u64 free_size_tmp = 0;
    int nid_id[DBL_NUMA_ID_MAX_NUM] = {-1};
    int nid_num = 0;
    int i;
    int ret;
    struct vmng_soc_resource_enquire *soc_info = NULL;

    soc_info = dbl_kzalloc(sizeof(struct vmng_soc_resource_enquire), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (soc_info == NULL) {
        dms_err("Kzalloc failed. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }

    ret = vmngd_enquire_soc_resource(dev_id, 0, soc_info);
    if (ret != 0) {
        dms_err("Failed to obtain VM management soc resource. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto SOC_INFO_KFREE;
    }

    nid_num = dbl_nid_get_nid(dev_id, DBL_MEMTYPE_ALL, DBL_SUB_MEMTYPE_ALL, nid_id, DBL_NUMA_ID_MAX_NUM);
    if (nid_num < 0) {
        dms_err("Invalid number of nids. (drv_id=%u; nid_num=%d)\n", dev_id, nid_num);
        ret = -EINVAL;
        goto SOC_INFO_KFREE;
    }

    for (i = 0; i < nid_num; i++) {
        if (soc_info->total.base.numa_bitmap & (0x01ULL << i)) {
            ret = mem_get_info_single(nid_id[i], &total_size_tmp, &free_size_tmp);
            if (ret != 0) {
                dms_err("Failed to obtain the memory size. (dev_id=%u; ret=%d)\n", dev_id, ret);
                goto SOC_INFO_KFREE;
            }
            info->total_size += total_size_tmp;
        }
    }

SOC_INFO_KFREE:
    dbl_kfree(soc_info);
    soc_info = NULL;
    return ret;
#else
    return 0;
#endif
}

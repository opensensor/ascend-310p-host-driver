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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include <linux/types.h>

#include "ka_memory_pub.h"
#include "ka_base_pub.h"

#include "dbl/uda.h"
#include "soc_res.h"

#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager.h"
#include "devdrv_manager_msg.h"
#include "devdrv_platform_resource.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_pcie.h"
#include "devdrv_common.h"
#include "devdrv_interface.h"
#include "dms_event_distribute.h"
#include "devmng_dms_adapt.h"
#include "hvdevmng_common.h"
#include "hvdevmng_cmd_proc.h"
#include "virtmng_interface.h"
#include "dev_mnt_vdevice.h"

/* follow event_sched macro definition */
#ifdef CFG_SOC_PLATFORM_MINIV2
#define SCHED_MAX_RESOURCE_PACKET_NUM 8
#else
#define SCHED_MAX_RESOURCE_PACKET_NUM 16
#endif

#define HVDEVMNG_DTYPE_TO_AICORE_NUM(dtype) (0x01 << (dtype))
#define HVDEVMNG_DTYPE_TO_AICPU_NUM(dtype, aicore_total)                        \
    (((dtype) == VMNG_TYPE_C1) ?                                                \
    (0x01 << (dtype)) :                                                         \
    (SCHED_MAX_RESOURCE_PACKET_NUM * (0x01 << (dtype)) / (aicore_total)))       \

#define HVDEVMNG_AICPU_BITMAP_CALCULATE 15

struct dev_resource {
    u32 aicore_num;
    u32 aicpu_num;
    struct tsdrv_id_capacity ts_id_capa[DEVDRV_MAX_TS_NUM];
    u32 vector_core_num;
};

STATIC struct dev_resource **g_hvdevmng_resource = NULL;

STATIC void hvdevmng_mem_free(void)
{
    unsigned int dev_id;

    if (g_hvdevmng_resource != NULL) {
        for (dev_id = 0; dev_id < DEVDRV_MAX_DAVINCI_NUM; dev_id++) {
            if (g_hvdevmng_resource[dev_id] != NULL) {
                dbl_kfree(g_hvdevmng_resource[dev_id]);
                g_hvdevmng_resource[dev_id] = NULL;
            }
        }
        dbl_kfree(g_hvdevmng_resource);
        g_hvdevmng_resource = NULL;
    }
}

STATIC int hvdevmng_mem_alloc(void)
{
    unsigned int dev_id;

    g_hvdevmng_resource = dbl_kzalloc(sizeof(struct dev_resource *) * DEVDRV_MAX_DAVINCI_NUM,
                                      KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_hvdevmng_resource == NULL) {
        devdrv_drv_err("Memory alloc for g_hvdevmng_resource failed.\n");
        return -ENOMEM;
    }
    for (dev_id = 0; dev_id < DEVDRV_MAX_DAVINCI_NUM; dev_id++) {
        g_hvdevmng_resource[dev_id] = dbl_kzalloc(sizeof(struct dev_resource) * VMNG_VDEV_MAX_PER_PDEV,
            KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (g_hvdevmng_resource[dev_id] == NULL) {
            devdrv_drv_err("Memory alloc for g_hvdevmng_resource[dev_id] failed. (dev_id=%u)\n", dev_id);
            hvdevmng_mem_free();
            return -ENOMEM;
        }
    }

    return 0;
}

STATIC inline int hvdevmng_dtype_to_vector_core_num(u32 dtype)
{
    return dtype == VMNG_TYPE_C1 ? (0x01 << dtype) : ((0x01 << dtype) - 1);
}

STATIC inline struct dev_resource *hvdevmng_get_dev_res_instance(u32 devid, u32 fid)
{
    return &g_hvdevmng_resource[devid][fid];
}

int hvdevmng_get_aicore_num(u32 devid, u32 fid, u32 *aicore_num)
{
    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (aicore_num == NULL) ||
        (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        devdrv_drv_err("invalid para, devid(%u) fid(%u) aicore_num(%pK)\n", devid, fid, aicore_num);
        return -EINVAL;
    }

    *aicore_num = g_hvdevmng_resource[devid][fid].aicore_num;
    return 0;
}
EXPORT_SYMBOL(hvdevmng_get_aicore_num);

int hvdevmng_get_template_name(u32 devid, u32 fid, u8 *name, u32 name_len)
{
#ifdef CFG_FEATURE_VFG
    int ret;
    struct vmng_soc_resource_enquire info;

    if (name_len < VMNG_VF_TEMP_NAME_LEN) {
        devdrv_drv_err("Input name length is invalid. (devid=%u; fid=%u; name_len=%u)\n", devid, fid, name_len);
        return -EINVAL;
    }

    (void)memset_s(&info, sizeof(struct vmng_soc_resource_enquire), 0, sizeof(struct vmng_soc_resource_enquire));
    ret = vmngh_enquire_soc_resource(devid, fid, &info);
    if (ret != 0) {
        devdrv_drv_err("Failed to invoke vmngh_enquire_soc_resource. (devid=%u; fid=%u; ret=%d)\n", devid, fid, ret);
        return -EINVAL;
    }

    ret = memcpy_s(name, name_len, info.each.name, VMNG_VF_TEMP_NAME_LEN);
    if (ret != 0) {
        devdrv_drv_err("Copy name length failed.(devid=%u; fid=%u; ret=%d)\n", devid, fid, ret);
        return -ENOMEM;
    }
#endif

    return 0;
}

#ifndef CFG_FEATURE_VFG
int hvdevmng_get_aicpu_num(u32 devid, u32 fid, u32 *aicpu_num, u32 *aicpu_bitmap)
{
    u32 bitmap = 0;
    u32 i;

    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (aicpu_num == NULL) ||
        (aicpu_bitmap == NULL) || (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        devdrv_drv_err("invalid para, devid(%u) fid(%u) aicore_num(%pK)\n",
                       devid, fid, aicpu_num);
        return -EINVAL;
    }

    *aicpu_num = g_hvdevmng_resource[devid][fid].aicpu_num;

    for (i = 0; i < *aicpu_num; i++) {
        if (HVDEVMNG_AICPU_BITMAP_CALCULATE >= i)
            bitmap |= (0x01 << (HVDEVMNG_AICPU_BITMAP_CALCULATE - i));
    }
    *aicpu_bitmap = bitmap;

    return 0;
}
#else
int hvdevmng_get_aicpu_num(u32 devid, u32 fid, u32 *aicpu_num, u32 *aicpu_bitmap)
{
    u32 i;
    u32 bitmap = 0;
    int ret;
    unsigned long aicpu_bitmap_tmp = 0;
    struct vmng_soc_resource_enquire info;

    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (aicpu_num == NULL) ||
        (aicpu_bitmap == NULL) || (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        devdrv_drv_err("invalid para, devid(%u) fid(%u) aicore_num(%pK)\n",
                       devid, fid, aicpu_num);
        return -EINVAL;
    }

    if (fid == 0) {
        *aicpu_num = g_hvdevmng_resource[devid][fid].aicpu_num;

        for (i = 0; i < *aicpu_num; i++) {
            if (HVDEVMNG_AICPU_BITMAP_CALCULATE >= i)
                bitmap |= (0x01 << (HVDEVMNG_AICPU_BITMAP_CALCULATE - i));
        }
        *aicpu_bitmap = bitmap;
    } else {
        (void)memset_s(&info, sizeof(struct vmng_soc_resource_enquire), 0, sizeof(struct vmng_soc_resource_enquire));
        ret = vmngh_enquire_soc_resource(devid, fid, &info);
        if (ret) {
            devdrv_drv_err("Failed to invoke vmngh_enquire_soc_resource. (devid=%u; fid=%u)",
                           devid, fid);
            return -EINVAL;
        }
        *aicpu_bitmap = info.each.stars_refresh.device_aicpu;
        aicpu_bitmap_tmp = (u64)(*aicpu_bitmap);
        *aicpu_num = ka_base_bitmap_weight(&aicpu_bitmap_tmp, HVDEVMNG_AICPU_BITMAP_CALCULATE + 1);
        g_hvdevmng_resource[devid][fid].vector_core_num = *aicpu_num;
    }

    return 0;
}
#endif

int hvdevmng_get_vector_core_num(u32 devid, u32 fid, u32 *vector_core_num)
{
    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (vector_core_num == NULL) ||
        (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        devdrv_drv_err("invalid para, devid(%u) fid(%u) vector_core_num(%pK)\n", devid, fid, vector_core_num);
        return -EINVAL;
    }

    *vector_core_num = g_hvdevmng_resource[devid][fid].vector_core_num;
    return 0;
}

int hvdevmng_set_core_num(u32 devid, u32 fid, u32 dtype)
{
    struct devdrv_info *dev_info = NULL;
    u32 aicore_total;

    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) ||
        (fid >= VMNG_VDEV_MAX_PER_PDEV)) {
        devdrv_drv_err("invalid para, devid(%u) fid(%u) dtype(%u).\n", devid, fid, dtype);
        return -EINVAL;
    }

    dev_info = devdrv_get_devdrv_info_array(devid);
    if (dev_info == NULL) {
        devdrv_drv_err("device(%u) is not initialized.\n", devid);
        return -ENODEV;
    }

    if (fid == 0) {
        g_hvdevmng_resource[devid][fid].aicore_num = dev_info->ai_core_num;
        g_hvdevmng_resource[devid][fid].aicpu_num = dev_info->ai_cpu_core_num;
        g_hvdevmng_resource[devid][fid].vector_core_num = dev_info->vector_core_num;
        devdrv_drv_info("hvdevmng_set_aicpu_num succ: dev(%u) fid(%u) aicore(%u) aicpu(%u) vector_core(%u).\n",
                        devid, fid, g_hvdevmng_resource[devid][fid].aicore_num,
                        g_hvdevmng_resource[devid][fid].aicpu_num, g_hvdevmng_resource[devid][fid].vector_core_num);
        return 0;
    }

    aicore_total = dev_info->ai_core_num;
    if (aicore_total == 0) {
        devdrv_drv_err("device(%u) invalid aicore num.\n", devid);
        return -EINVAL;
    }

    g_hvdevmng_resource[devid][fid].aicore_num = HVDEVMNG_DTYPE_TO_AICORE_NUM(dtype);
    g_hvdevmng_resource[devid][fid].aicpu_num = HVDEVMNG_DTYPE_TO_AICPU_NUM(dtype, aicore_total);
    g_hvdevmng_resource[devid][fid].vector_core_num = g_hvdevmng_resource[devid][fid].aicpu_num;
    devdrv_drv_info("hvdevmng_set_aicpu_num succ: dev(%u) fid(%u) aicore(%u) aicpu(%u) vector_core(%u).\n",
                    devid, fid, g_hvdevmng_resource[devid][fid].aicore_num,
                    g_hvdevmng_resource[devid][fid].aicpu_num, g_hvdevmng_resource[devid][fid].vector_core_num);
    return 0;
}

int hvdevmng_get_chip_type(u32 devid, u32 *chip_type)
{
    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (chip_type == NULL)) {
        devdrv_drv_err("invalid para, devid(%u) chip_type(%pK).\n",
                       devid, chip_type);
        return -EINVAL;
    }

    *chip_type = devdrv_get_dev_chip_type(devid);
    if (*chip_type == HISI_CHIP_UNKNOWN) {
        devdrv_drv_err("[devid=%u]:devdrv_get_dev_chip_type failed, unknown.\n", devid);
        return -EINVAL;
    }
    if (*chip_type >= HISI_CHIP_NUM) {
        devdrv_drv_err("[devid=%u]:chip_type invalid(%d).\n", devid, *chip_type);
        return -ENODEV;
    }
    return 0;
}
EXPORT_SYMBOL(hvdevmng_get_chip_type);

STATIC int hvdevmng_get_dev_tsdrv_resource(u32 devid, u32 fid, u32 tsid, u32 info_type, u64 *result_data)
{
    struct dev_resource *resource = NULL;

    resource = hvdevmng_get_dev_res_instance(devid, fid);
    if (resource == NULL) {
        devdrv_drv_err("get hvdevmng global resource failed.\n");
        return -EINVAL;
    }

    switch (info_type) {
        case DEVDRV_DEV_STREAM_ID:
            *result_data = resource->ts_id_capa[tsid].stream_capacity;
            break;
        case DEVDRV_DEV_EVENT_ID:
            *result_data = resource->ts_id_capa[tsid].event_capacity;
            break;
        case DEVDRV_DEV_NOTIFY_ID:
            *result_data = resource->ts_id_capa[tsid].notify_capacity;
            break;
        case DEVDRV_DEV_MODEL_ID:
            *result_data = resource->ts_id_capa[tsid].model_capacity;
            break;
        default:
            devdrv_drv_err("invalid dev resource type info_type (%u).\n", info_type);
            return -EINVAL;
    }

    return 0;
}

STATIC int hvdevmng_check_chip_type(u32 devid, const struct devdrv_manager_msg_resource_info *resource_info)
{
    u32 chip_type;
    int ret;

    ret = hvdevmng_get_chip_type(devid, &chip_type);
    if (ret != 0) {
        devdrv_drv_err("Get chip type info failed. (ret=%d)\n", ret);
        return ret;
    }

    if (chip_type < HISI_CLOUD_V1 || chip_type > HISI_MINI_V3) {
        return -EOPNOTSUPP;
    }

    if (resource_info->info_type == DEVDRV_DEV_HBM_TOTAL || resource_info->info_type == DEVDRV_DEV_HBM_FREE) {
        if (chip_type == HISI_MINI_V2 || chip_type == HISI_MINI_V3) {
            return -EOPNOTSUPP;
        }
    }

    return 0;
}

int hvdevmng_get_dev_resource(u32 devid, u32 tsid, struct devdrv_manager_msg_resource_info *resource_info)
{
    int ret;

    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (resource_info->vfid >= VMNG_VDEV_MAX_PER_PDEV) ||
        (tsid >= DEVDRV_MAX_TS_NUM) || (resource_info->info_type >= DEVDRV_DEV_INFO_TYPE_MAX)) {
        devdrv_drv_err("Invalid para. (devid=%u; fid=%u; info_type=%u).\n",
            devid, resource_info->vfid, resource_info->info_type);
        return -EINVAL;
    }

    ret = hvdevmng_check_chip_type(devid, resource_info);
    if (ret) {
        devdrv_drv_err("Check chip type info failed. (ret = %d)\n", ret);
        return ret;
    }

    if (resource_info->info_type <= DEVDRV_MAX_TS_INFO_TYPE) {
        ret = hvdevmng_get_dev_tsdrv_resource(devid, resource_info->vfid, tsid,
            resource_info->info_type, &resource_info->value);
    } else if (resource_info->info_type <= DEVDRV_MAX_MEM_INFO_TYPE ||
                   resource_info->info_type == DEVDRV_DEV_PROCESS_MEM) {
        ret = devdrv_manager_h2d_query_resource_info(devid, resource_info);
    } else {
        return -EINVAL;
    }

    return ret;
}

void hvdevmng_set_dev_ts_resource(u32 devid, u32 fid, u32 tsid, void *data)
{
    struct tsdrv_id_capacity *ts_res_info = NULL;
    struct dev_resource *dev_resource = NULL;

    if ((devid >= DEVDRV_MAX_DAVINCI_NUM) || (fid >= VMNG_VDEV_MAX_PER_PDEV) ||
        (tsid >= DEVDRV_MAX_TS_NUM) || (data == NULL)) {
        devdrv_drv_err("invalid para, devid(%u) fid(%u) tsid(%u) or input data is NULL.\n",
            devid, fid, tsid);
        return;
    }

    ts_res_info = (struct tsdrv_id_capacity *)data;

    dev_resource = hvdevmng_get_dev_res_instance(devid, fid);
    if (dev_resource == NULL) {
        devdrv_drv_err("get hvdevmng global resource failed.\n");
        return;
    }

    dev_resource->ts_id_capa[tsid] = *ts_res_info;
    return;
}
EXPORT_SYMBOL(hvdevmng_set_dev_ts_resource);

#if defined CFG_FEATURE_VFIO && !defined CFG_FEATURE_SRIOV

STATIC int (*const hvdevmng_ioctl_handlers[DEVDRV_MANAGER_CMD_MAX_NR])(u32 dev_id,
    u32 fid, struct vdevmng_ioctl_msg *iomsg) = {
        [_IOC_NR(DEVDRV_MANAGER_GET_PCIINFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEVNUM)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_PLATINFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_DEVICE_STATUS)] = hvdevmng_get_device_status,
        [_IOC_NR(DEVDRV_MANAGER_GET_CORE_SPEC)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_CORE_INUSE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEVIDS)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_CONTAINER_DEVIDS)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEVINFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEVID_BY_LOCALDEVID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEV_INFO_BY_PHYID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_PCIE_ID_INFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_VOLTAGE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_TEMPERATURE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_TSENSOR)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_AI_USE_RATE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_FREQUENCY)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_POWER)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_HEALTH_CODE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_ERROR_CODE)] = hvdevmng_get_error_code,
        [_IOC_NR(DEVDRV_MANAGER_GET_DDR_CAPACITY)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_LPM3_SMOKE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_BLACK_BOX_GET_EXCEPTION)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_DEVICE_MEMORY_DUMP)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_DEVICE_RESET_INFORM)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_MODULE_STATUS)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_MINI_BOARD_ID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_PCIE_PRE_RESET)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_PCIE_RESCAN)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_PCIE_HOT_RESET)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_P2P_ATTR)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_ALLOC_HOST_DMA_ADDR)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_PCIE_READ)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_PCIE_SRAM_WRITE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_EMMC_VOLTAGE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEVICE_BOOT_STATUS)] = hvdevmng_get_device_boot_status,
        [_IOC_NR(DEVDRV_MANAGER_ENABLE_EFUSE_LDO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_DISABLE_EFUSE_LDO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_CONTAINER_CMD)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_HOST_PHY_MACH_FLAG)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_LOCAL_DEVICEIDS)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_IMU_SMOKE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_SET_NEW_TIME)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CREATE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_OPEN)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CLOSE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_DESTROY)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_SET_PID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_CPU_INFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_SEND_TO_IMU)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_RECV_FROM_IMU)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_IMU_INFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_CONFIG_ECC_ENABLE)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_PROBE_NUM)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_PROBE_LIST)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_DEBUG_INFORM)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_COMPUTE_POWER)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_SYNC_MATRIX_DAEMON_READY)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_BBOX_ERRSTR)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_PCIE_IMU_DDR_READ)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_SLOT_ID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_APPMON_BBOX_EXCEPTION_CMD)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_CONTAINER_FLAG)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_PROCESS_SIGN)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_MASTER_DEV_IN_THE_SAME_OS)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_LOCAL_DEV_ID_BY_HOST_DEV_ID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_BOOT_DEV_ID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_TSDRV_DEV_COM_INFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_CAPABILITY_GROUP_INFO)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_PASSTHRU_MCU)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_P2P_CAPABILITY)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_ETH_ID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_BIND_PID_ID)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_H2D_DEVINFO)] = hvdevmng_get_h2d_devinfo,
        [_IOC_NR(DEVDRV_MANAGER_GET_CONSOLE_LOG_LEVEL)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_UPDATE_STARTUP_STATUS)] = NULL,
        [_IOC_NR(DEVDRV_MANAGER_GET_STARTUP_STATUS)] = hvdevmng_get_device_startup_status,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEVICE_HEALTH_STATUS)] = hvdevmng_get_device_health_status,
        [_IOC_NR(DEVDRV_MANAGER_GET_DEV_RESOURCE_INFO)] = hvdevmng_vpc_get_dev_resource_info,
        [_IOC_NR(DEVDRV_MANAGER_GET_OSC_FREQ)] = hvdevmng_get_osc_freq,
        [_IOC_NR(DEVDRV_MANAGER_GET_CURRENT_AIC_FREQ)] = hvdevmng_get_current_aic_freq,
};

STATIC int hvdevmng_rx_vpc_msg_para_check(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    if ((proc_info == NULL) || (proc_info->data == NULL) ||
        (dev_id >= VMNG_PDEV_MAX) || (fid >= VMNG_VDEV_MAX_PER_PDEV) ||
        (proc_info->real_out_len == NULL)) {
        devdrv_drv_err("para error. dev_id %u, fid %u\n", dev_id, fid);
        return -EINVAL;
    }

    if ((proc_info->in_data_len != sizeof(struct vdevmng_ioctl_msg)) ||
        (proc_info->out_data_len != sizeof(struct vdevmng_ioctl_msg))) {
        devdrv_drv_err("in_data_len %u or out_data_len %u error, vdevmng_ioctl_msg %lu\n",
            proc_info->in_data_len, proc_info->out_data_len, sizeof(struct vdevmng_ioctl_msg));
        return -EINVAL;
    }

    return 0;
}

int hvdevmng_vpc_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct vdevmng_ioctl_msg *iomsg = NULL;
    unsigned int cmd;
    int ret = 0;

    ret = hvdevmng_rx_vpc_msg_para_check(dev_id, fid, proc_info);
    if (ret) {
        devdrv_drv_err("hvdevmng dev(%u) fid(%u) vpc msg para check failed.\n", dev_id, fid);
        return -EINVAL;
    }

    iomsg = (struct vdevmng_ioctl_msg *)proc_info->data;
    cmd = iomsg->sub_cmd;

    if (cmd >= DEVDRV_MANAGER_CMD_MAX_NR) {
        devdrv_drv_err("cmd out of range:cmd(%u) max value(%u) dev(%u) fid(%u).\n",
                       cmd, DEVDRV_MANAGER_CMD_MAX_NR, dev_id, fid);
        ret = -EINVAL;
        goto VPC_OUT;
    }

    if (hvdevmng_ioctl_handlers[cmd] == NULL) {
        devdrv_drv_err("not support cmd:cmd(%u) dev(%u) fid(%u).\n",
                       cmd, dev_id, fid);
        ret = -EINVAL;
        goto VPC_OUT;
    }

    ret = hvdevmng_ioctl_handlers[cmd](dev_id, fid, iomsg);

VPC_OUT:
    *(proc_info->real_out_len) = sizeof(struct vdevmng_ioctl_msg);
    iomsg->result = ret;

    return ret;
}
EXPORT_SYMBOL_UNRELEASE(hvdevmng_vpc_msg_recv);

struct vmng_vpc_client hvdevmng_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_DEVMNG,
    .init = NULL,
    .msg_recv = hvdevmng_vpc_msg_recv,
};

STATIC int hvdevmng_rx_comm_msg_para_check(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    if ((proc_info == NULL) || (proc_info->data == NULL) ||
        (dev_id >= VMNG_PDEV_MAX) || (fid >= VMNG_VDEV_MAX_PER_PDEV) ||
        (proc_info->real_out_len == NULL)) {
        devdrv_drv_err("para error. dev_id %u, fid %u\n", dev_id, fid);
        return -EINVAL;
    }

    if ((proc_info->in_data_len != sizeof(struct vdevmng_ctrl_msg)) ||
        (proc_info->out_data_len != sizeof(struct vdevmng_ctrl_msg))) {
        devdrv_drv_err("in_data_len %u or out_data_len %u error, vdevmng_ctrl_msg %lu\n",
            proc_info->in_data_len, proc_info->out_data_len, sizeof(struct vdevmng_ctrl_msg));
        return -EINVAL;
    }

    return 0;
}

int hvdevmng_com_msg_recv(u32 dev_id, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct vdevmng_ctrl_msg *ctrl_msg = NULL;
    int ret;

    ret = hvdevmng_rx_comm_msg_para_check(dev_id, fid, proc_info);
    if (ret) {
        devdrv_drv_err("hvdevmng common msg para check failed, devid %u, fid %u\n", dev_id, fid);
        return -EINVAL;
    }

    ctrl_msg = (struct vdevmng_ctrl_msg *)proc_info->data;
    switch (ctrl_msg->type) {
        case VDEVMNG_CTRL_MSG_TYPE_OPEN:
            break;
        case VDEVMNG_CTRL_MSG_TYPE_RELEASE:
            break;
        case VDEVMNG_CTRL_MSG_TYPE_GET_DEVINFO:
            ret = hvdevmng_mdev_info_get(dev_id, fid, &ctrl_msg->ctrl_data.ready_info);
            if (ret) {
                devdrv_drv_err("get mdev info fail, dev(%u).\n", dev_id);
            }
            break;
        case VDEVMNG_CTRL_MSG_TYPE_GET_PDATA:
            ret = hvdevmng_mdev_pdata_get(dev_id, &ctrl_msg->ctrl_data.ready_pdata);
            if (ret) {
                devdrv_drv_err("get mdev pdata fail, dev(%u).\n", dev_id);
            }
            break;
        default:
            devdrv_drv_err("hvdevmng common ctrl msg type %u error, dev(%u), fid(%u).\n", ctrl_msg->type, dev_id, fid);
            ret = -EINVAL;
            break;
    }

    *(proc_info->real_out_len) = sizeof(struct vdevmng_ctrl_msg);
    ctrl_msg->error_code = ret;

    return ret;
}
EXPORT_SYMBOL_UNRELEASE(hvdevmng_com_msg_recv);

struct vmng_common_msg_client hvdevmng_common_msg_client = {
    .type = VMNG_MSG_COMMON_TYPE_DEVMNG,
    .init = NULL,
    .common_msg_recv = hvdevmng_com_msg_recv,
};

STATIC int hvdevmng_init_instance(u32 dev_id, u32 fid)
{
    int ret;

    ret = vmngh_register_common_msg_client(dev_id, fid, &hvdevmng_common_msg_client);
    if (ret != 0) {
        devdrv_drv_err("hvdevmng dev-%u fid-%u common msg client register failed, ret %d.\n", dev_id, fid, ret);
        goto common_register_fail;
    }

    ret = vmngh_vpc_register_client_safety(dev_id, fid, &hvdevmng_vpc_client);
    if (ret != 0) {
        devdrv_drv_err("hvdevmng dev-%u fid-%u vpc client register failed, ret %d.\n", dev_id, fid, ret);
        goto vpc_register_fail;
    }

    ret = dev_mnt_create_one_vm_vdevice(dev_id, fid);
    if (ret) {
        devdrv_drv_err("hvdevmng dev-%u fid-%u create vdavinci failed, ret %d.\n", dev_id, fid, ret);
        goto create_vdevice_fail;
    }

    devdrv_drv_info("hvdevmng dev-%u fid-%u aicorenum-%u aicpunum-%u init success.\n",
                    dev_id, fid, g_hvdevmng_resource[dev_id][fid].aicore_num,
                    g_hvdevmng_resource[dev_id][fid].aicpu_num);
    return 0;

create_vdevice_fail:
    vmngh_vpc_unregister_client(dev_id, fid, &hvdevmng_vpc_client);
vpc_register_fail:
    vmngh_unregister_common_msg_client(dev_id, fid, &hvdevmng_common_msg_client);
common_register_fail:
    return ret;
}

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
STATIC int hvdevmng_uninit_instance(u32 dev_id, u32 fid)
{
    vmngh_vpc_unregister_client(dev_id, fid, &hvdevmng_vpc_client);
    vmngh_unregister_common_msg_client(dev_id, fid, &hvdevmng_common_msg_client);

    dev_mnt_released_one_vm_vdevice(dev_id, fid);

    devdrv_drv_info("hvdevmng dev-%u fid-%u uninit success.\n", dev_id, fid);
    return 0;
}
#endif

STATIC int hvdevmng_container_init_instance(u32 dev_id, u32 fid, u32 aicore_num)
{
    u32 dtype = 0;
    int ret;

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    u64 num = (u64)aicore_num;
    dtype = ka_base_find_first_bit((const unsigned long *)&num, 32); /* 32 u32 bitnum */
#endif

    ret = hvdevmng_set_core_num(dev_id, fid, dtype);
    if (ret != 0) {
        devdrv_drv_err("hvdevmng dev-%u fid-%u set aicpu num failed, ret %d.\n", dev_id, fid, ret);
        return ret;
    }

    devdrv_drv_info("Init success.(devid=%u; fid=%u; aicore_num=%u; dtype=%u)\n", dev_id, fid, aicore_num, dtype);
    return 0;
}

STATIC int hvdevmng_container_uninit_instance(u32 dev_id, u32 fid)
{
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    g_hvdevmng_resource[dev_id][fid].aicore_num = 0;
    g_hvdevmng_resource[dev_id][fid].aicpu_num = 0;

    devdrv_drv_info("hvdevmng container dev-%u fid-%u uninit success.\n", dev_id, fid);
#endif
    return 0;
}

static int hvdevmng_get_mia_dev_aicore_num(u32 udevid, u32 *aicore_num)
{
    u64 bitmap;
    u32 unit_per_bit;
    int ret;

    ret = soc_resmng_dev_get_mia_res(udevid, MIA_AC_AIC, &bitmap, &unit_per_bit);
    if (ret != 0) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
        devdrv_drv_err("Get aicore bitmap failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
#endif
    }
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    *aicore_num = ka_base_bitmap_weight((const unsigned long *)&bitmap, 64); /* 64 u64 bitnum */
#endif
    return 0;
}

#define DEVMNG_MIA_NOTIFIER "dms_mng_mia"
static int hvdevmng_mia_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
        devdrv_drv_err("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
#endif
    }
    fid = mia_para.sub_devid + 1;

    if (action == UDA_INIT) {
        u32 aicore_num;
        ret = hvdevmng_get_mia_dev_aicore_num(udevid, &aicore_num);
        if (ret == 0) {
            ret = hvdevmng_container_init_instance(mia_para.phy_devid, fid, aicore_num);
        }
    } else if (action == UDA_UNINIT) {
        ret = hvdevmng_container_uninit_instance(mia_para.phy_devid, fid);
    }

    devdrv_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
static int hvdevmng_mia_dev_agent_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct uda_mia_dev_para mia_para;
    u32 fid;
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
        devdrv_drv_err("Invalid para. (udevid=%u; action=%d)\n", udevid, action);
        return ret;
    }
    fid = mia_para.sub_devid + 1;

    if (action == UDA_INIT) {
        ret = hvdevmng_init_instance(mia_para.phy_devid, fid);
    } else if (action == UDA_UNINIT) {
        ret = hvdevmng_uninit_instance(mia_para.phy_devid, fid);
    }

    devdrv_drv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}
#endif

int hvdevmng_notifier_register(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_near_virtual_entity_type_pack(&type);
    ret = uda_notifier_register(DEVMNG_MIA_NOTIFIER, &type, UDA_PRI1, hvdevmng_mia_dev_notifier_func);
    if (ret != 0) {
        devdrv_drv_err("Register mia dev notifier failed. (ret=%d)\n", ret);
        return ret;
    }

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    uda_davinci_near_virtual_agent_type_pack(&type);
    ret = uda_notifier_register(DEVMNG_MIA_NOTIFIER, &type, UDA_PRI1, hvdevmng_mia_dev_agent_notifier_func);
    if (ret != 0) {
        uda_davinci_near_virtual_entity_type_pack(&type);
        (void)uda_notifier_unregister(DEVMNG_MIA_NOTIFIER, &type);
        devdrv_drv_err("Register mia dev agent notifier failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

    return 0;
}

void hvdevmng_notifier_unregister(void)
{
    struct uda_dev_type type;

    uda_davinci_near_virtual_entity_type_pack(&type);
    (void)uda_notifier_unregister(DEVMNG_MIA_NOTIFIER, &type);
    uda_davinci_near_virtual_agent_type_pack(&type);
    (void)uda_notifier_unregister(DEVMNG_MIA_NOTIFIER, &type);
}

int hvdevmng_init(void)
{
    int ret;

    ret = hvdevmng_notifier_register();
    if (ret != 0) {
        return -ENODEV;
    }

    ret =  hvdevmng_mem_alloc();
    if (ret != 0) {
        devdrv_drv_err("Alloc memory failed. (ret=%d)\n", ret);
        hvdevmng_notifier_unregister();
        return -ENOMEM;
    }

    devdrv_drv_info("vdevmng register vmnh client success\n");
    return 0;
}

void hvdevmng_uninit(void)
{
    hvdevmng_notifier_unregister();
    hvdevmng_mem_free();
}
#else

int hvdevmng_init(void)
{
    int ret;

    ret =  hvdevmng_mem_alloc();
    if (ret != 0) {
        devdrv_drv_err("Alloc memory failed. (ret=%d)\n", ret);
        return -ENOMEM;
    }

    return 0;
}

void hvdevmng_uninit(void)
{
    hvdevmng_mem_free();
    return;
}

#endif

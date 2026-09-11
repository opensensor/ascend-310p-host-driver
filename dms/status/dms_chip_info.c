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

#ifndef AOS_LLVM_BUILD
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/kthread.h>

#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#include "uda.h"
#include "dbl/feature_loader.h"
#include "urd_acc_ctrl.h"
#include "user_cfg_interface.h"
#include "dms_template.h"

#include "securec.h"
#include "dms_define.h"
#include "dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "hvtsdrv_tsagent.h"
#include "devdrv_common.h"
#include "devdrv_manager_container.h"
#include "devdrv_pcie.h"
#include "ascend_kernel_hal.h"
#include "dms_chip_info.h"

#ifdef CFG_SOC_PLATFORM_MDC_V51
#include "devdrv_manager_comm.h"
#include "devmng_dms_adapt.h"
#endif

#ifdef CFG_FEATURE_GET_DEV_UUID
#define UUID_MAX_LEN 16
#define MAX_CHIP_NUM 2
STATIC char g_mac_info[MAX_CHIP_NUM][UUID_MAX_LEN];
STATIC atomic_t g_mac_init_flag[MAX_CHIP_NUM] = {KA_BASE_ATOMIC_INIT(0)};
#define UUID_VENDOR_ID 0xCC08
#endif

typedef enum {
    CORE_NUM_A_INDEX = 1,
    CORE_NUM_B_INDEX,
    CORE_NUM_C_INDEX,
    CORE_NUM_LEVEL_MAX
} AICORE_NUM_LEVEL;

typedef enum {
    FREQ_LITE_INDEX = 1,
    FREQ_PRO_INDEX,
    FREQ_PRE_INDEX,
    CORE_FREQ_LEVEL_MAX,
} AICORE_FREQ_LEVEL;

/* add for ai computing power */
typedef enum {
    AI_COMPUTING_LEVEL1 = 0,
    AI_COMPUTING_LEVEL2,
    AI_COMPUTING_LEVEL3,
    AI_COMPUTING_LEVEL4,
} AI_COMPUTING_LEVEL;

#define MINIV2_PRO_CORE_NUM 10
#define MINIV2_CORE_NUM 8

#define DEV_AICORE_FREQ_LEVEL_CLOUD 1
#define DEV_AICORE_FREQ_LEVEL_MINIV2 2

/* add for partial good */
#define CLOUD_FULL_GOOD_CORE_NUM 32
#define AICORE_FREQ_LEVEL_PRO 1100
#define AICORE_FREQ_LEVEL_PRE 1200

#define AICORE_LEVEL_LIMIT(NumLevel, FreqLevel) (((NumLevel) > 0) &&    \
    ((NumLevel) < CORE_NUM_LEVEL_MAX) && ((FreqLevel) > 0) &&          \
    ((FreqLevel) < CORE_FREQ_LEVEL_MAX))

STATIC char *g_aicore_freq_level[] = {"NULL", "", "Pro", "Premium"};
STATIC char *g_aicore_num_level[] = {"NULL", "A", "B", "C"};

#ifndef CFG_SOC_PLATFORM_MDC_V51
STATIC char *g_ai_computing_level[] = {"1", "2", "3", "4"};
#endif

typedef struct chip_value_name_map {
    unsigned int chip_value;
    char *chip_name;
} chip_value_name_map_t;

#define MINI_CHIP_NAME_VALUE 6416

/* chip value 6416/6528/6481 means miniv1/cloud/miniv2 */
STATIC chip_value_name_map_t g_chip_value_name_map[] = {
    { 6416, "310" },
    { 6528, "910" },
#ifdef CFG_SOC_PLATFORM_MDC_V51
    { 6481, "610" },
#else
    { 6481, "310P" },
#endif
    { 6417, "310B" },
    { 6529, "910B" },
};

typedef struct dms_chip_aicore_info {
    unsigned int chip_ver;
    unsigned int chip_name;
    unsigned int num;
    unsigned int num_level;
    unsigned int freq;
    unsigned int freq_level;
} dms_chip_aicore_info_t;

STATIC int dms_get_chip_aicore_info(unsigned int dev_id, unsigned int vfid, bool is_container_split,
    dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret;
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct devdrv_platform_data *pdata = NULL;

    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get device ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (dev_cb->dev_info == NULL) {
        dms_err("Device ctrl dev_info is null. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    pdata = dev_info->pdata;
    if (pdata == NULL) {
        return -ENODEV;
    }
    chip_aicore_info->freq_level = pdata->ai_core_freq_level;
    chip_aicore_info->num_level = pdata->ai_core_num_level;
    chip_aicore_info->freq = dev_info->aicore_freq;
    chip_aicore_info->chip_name = dev_info->chip_name;
    chip_aicore_info->chip_ver = dev_info->chip_version;
    if (is_container_split == true) {
        ret = hvdevmng_get_aicore_num(dev_id, vfid, &chip_aicore_info->num);
        if (ret != 0) {
            dms_err("Get container aicore num failed. (dev_id=%u; vfid=%u; ret=%d)", dev_id, vfid, ret);
            return ret;
        }
    } else {
        chip_aicore_info->num = dev_info->ai_core_num;
    }

    dms_debug("(Freq level=%u; num_level=%u; num=%u; freq=%u; chip_name=0x%x; chip_ver=%u)\n",
        chip_aicore_info->freq_level, chip_aicore_info->num_level, chip_aicore_info->num, chip_aicore_info->freq,
        chip_aicore_info->chip_name, chip_aicore_info->chip_ver);
    return 0;
}

static int dms_set_chip_name_by_aicore_num_and_freq(devdrv_query_chip_info_t *chip_info,
    dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret;
    unsigned char *chip_name = chip_info->info.name;

    if (AICORE_LEVEL_LIMIT(chip_aicore_info->num_level, chip_aicore_info->freq_level)) {
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[chip_aicore_info->freq_level]);
        if (ret < 0) {
            dms_err("Strcat chip name by aicore freq level failed. (ret=%d)\n", ret);
            return ret;
        }

        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_num_level[chip_aicore_info->num_level]);
        if (ret < 0) {
            dms_err("Strcat chip name by aicore num level failed. (ret=%d)\n", ret);
            return ret;
        }
    } else {
        if (chip_aicore_info->freq < AICORE_FREQ_LEVEL_PRO) {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[FREQ_LITE_INDEX]);
        } else if (chip_aicore_info->freq > AICORE_FREQ_LEVEL_PRE) {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[FREQ_PRE_INDEX]);
        } else {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[FREQ_PRO_INDEX]);
        }

        if (ret < 0) {
            dms_err("Strcat chip name by aicore freq fail. (dev_id=%u; frequency=%u; ret=%d)\n",
                chip_info->dev_id, chip_aicore_info->freq, ret);
            return ret;
        }

        if (chip_aicore_info->num < CLOUD_FULL_GOOD_CORE_NUM) {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_num_level[CORE_NUM_B_INDEX]);
        } else {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_num_level[CORE_NUM_A_INDEX]);
        }

        if (ret < 0) {
            dms_err("Strcat chip name by aicore num fail. (dev_id=%u; num=%u; ret=%d)\n",
                chip_info->dev_id, chip_aicore_info->num, ret);
            return ret;
        }
    }

    return 0;
}

STATIC int dms_set_chip_name_only_by_aicore_num(devdrv_query_chip_info_t *chip_info,
    dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret;
    unsigned int ai_core_num = chip_aicore_info->num;
    unsigned char *chip_name = chip_info->info.name;

    if (ai_core_num == MINIV2_PRO_CORE_NUM) {
#ifdef CFG_SOC_PLATFORM_MDC_V51
        /* "1, 2" is array index, not magic num */
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[2]);
#else
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_ai_computing_level[AI_COMPUTING_LEVEL1]);
#endif
        if (ret < 0) {
            dms_err("Strcat by aicore freq failed. (dev_id=%u; aicore_num=%u; ret=%d)\n",
                chip_info->dev_id, ai_core_num, ret);
            return ret;
        }
    } else if (ai_core_num == MINIV2_CORE_NUM) {
#ifdef CFG_SOC_PLATFORM_MDC_V51
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[1]);
#else
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_ai_computing_level[AI_COMPUTING_LEVEL3]);
#endif
        if (ret < 0) {
            dms_err("Strcat by aicore freq failed. (dev_id=%u; aicore_num=%u; ret=%d)\n",
                chip_info->dev_id, ai_core_num, ret);
            return ret;
        }
    } else {
#ifdef CFG_FEATURE_BIN_DOWN
        dms_event("aicore_num = %d, please check whether mode BinDown is enabled.\n", ai_core_num);
        ret = sprintf_s((char *)chip_name, MAX_CHIP_NAME, "%s", "");
        if (ret < 0) {
            dms_err("Strcat by aicore freq failed. (dev_id=%u; aicore_num=%u; ret=%d)\n",
                chip_info->dev_id, ai_core_num, ret);
            return ret;
        }
#else
        dms_err("Aicore number is invalid. (dev_id=%u; aicore_num=%u)\n", chip_info->dev_id, ai_core_num);
        return DRV_ERROR_PARA_ERROR;
#endif
    }

    return 0;
}

#if defined (CFG_FEATURE_PG) && (!defined (CFG_SOC_PLATFORM_MDC_V11))
static int dms_set_chip_name_by_soc_version(u32 dev_id, u8 *chip_name)
{
    int ret;
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("dev_info is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (strlen(dev_info->pg_info.spePgInfo.socVersion) <= strlen("Ascend")) {
        dms_err("soc version is too short. (ver=\"%s\")\n", dev_info->pg_info.spePgInfo.socVersion);
        return DRV_ERROR_INVALID_VALUE;
    }
    /* the soc_version read from hsm is like Ascend910xx, Ascend is not wanted */
    ret = strcpy_s(chip_name, MAX_CHIP_NAME, (u8 *)dev_info->pg_info.spePgInfo.socVersion + strlen("Ascend"));
    if (ret) {
        dms_err("Call strcpy_s failed. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}
#endif

#ifdef CFG_CHIP_INFO_FROM_DEVINFO
STATIC int dms_set_chip_template_name_from_devinfo(u32 dev_id, u32 vfid, devdrv_query_chip_info_t *chip_info)
{
    int ret;
    struct devdrv_info *dev_info = NULL;

    if (devdrv_manager_is_pf_device(dev_id) && (vfid == 0)) {
        return 0;
    }

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("dev_info is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = strcat_s((char *)chip_info->info.name, MAX_CHIP_NAME, dev_info->template_name);
    if (ret != 0) {
        dms_err("Set template name failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return -EINVAL;
    }

    return 0;
}
#define dms_set_chip_template_name dms_set_chip_template_name_from_devinfo
#elif defined(CFG_CHIP_INFO_FROM_VMNG)
STATIC int dms_set_chip_template_name_from_vmng(u32 dev_id, u32 vfid, devdrv_query_chip_info_t *chip_info)
{
    int ret;
    struct vmng_soc_resource_enquire info;

    if (devdrv_manager_is_pf_device(dev_id) && (vfid == 0)) {
        return 0;
    }

#ifdef CFG_HOST_ENV
    ret = vmngh_enquire_soc_resource(dev_id, vfid, &info);
#else
    ret = vmngd_enquire_soc_resource(dev_id, vfid, &info);
#endif
    if (ret != 0) {
        dms_err("Enquire soc resource failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    ret = strcat_s((char *)chip_info->info.name, MAX_CHIP_NAME, info.each.name);
    if (ret != 0) {
        dms_err("Set template name failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return -EINVAL;
    }

    return 0;
}
#define dms_set_chip_template_name dms_set_chip_template_name_from_vmng
#else
STATIC int dms_set_chip_template_name(u32 dev_id, u32 vfid, devdrv_query_chip_info_t *chip_info)
{
    return 0;
}
#endif

STATIC int dms_set_chip_type(u32 dev_id, devdrv_query_chip_info_t *chip_info, unsigned int virt_id)
{
    int ret = 0;
#if defined (CFG_SOC_MDC_V51_LITE)
    ret = strcpy_s((char *)chip_info->info.type, MAX_CHIP_NAME, "P");
    if (ret != 0) {
        dms_err("Copy chip type failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }
#elif defined (CFG_FEATURE_PG) && defined (CFG_SOC_PLATFORM_MDC_V11)
    struct devdrv_info *dev_info = NULL;
    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("dev_info is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    /* The AS31XM1X information is obtained from the sys_base_config.xml file in the ts_platform table. */
    ret = strcpy_s((char *)chip_info->info.type, MAX_CHIP_NAME, dev_info->pg_info.spePgInfo.socVersion);
    if (ret != 0) {
        dms_err("Copy as31xm1 chip type failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
        return ret;
    }
#else
    /* Initialize the chip_type to Ascend. */
    ret = strcpy_s((char *)chip_info->info.type, MAX_CHIP_NAME, "Ascend");
    if (ret != 0) {
        dms_err("Copy chip type failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }
#endif
    return 0;
}

static int dms_set_chip_name(devdrv_query_chip_info_t *chip_info, bool is_container_split,
    dms_chip_aicore_info_t *chip_aicore_info, unsigned int virt_id)
{
    int ret = 0;
    unsigned int i;
#ifdef CFG_SOC_MDC_V51_LITE
    struct devdrv_info *dev_info = NULL;
#endif

    ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%s", "unknown");
    if (ret < 0) {
        dms_err("Set initial chip name failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

#if defined (CFG_FEATURE_PG) && (!defined (CFG_SOC_PLATFORM_MDC_V11))
    ret = dms_set_chip_name_by_soc_version(chip_info->dev_id, (u8 *)chip_info->info.name);
    if (ret) {
        dms_err("Set chip name by soc version failed. (dev_id=%u)\n", chip_info->dev_id);
        return ret;
    }
    return 0;
#elif defined (CFG_SOC_MDC_V51_LITE)
    dev_info = devdrv_manager_get_devdrv_info(chip_info->dev_id);
    if (dev_info == NULL) {
        dms_err("dev_info is NULL. (dev_id=%u)\n", chip_info->dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = strncpy_s((char *)chip_info->info.name, MAX_CHIP_NAME, dev_info->soc_version, MAX_CHIP_NAME - 1);
    if (ret != 0) {
        dms_err("Call strncpy_s failed. (dev_id=%u)\n", chip_info->dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
#elif defined (CFG_FEATURE_PG) && (defined (CFG_SOC_PLATFORM_MDC_V11))
    ret = strcpy_s((char *)chip_info->info.name, MAX_CHIP_NAME, g_aicore_freq_level[1]);
    if (ret != 0) {
        dms_err("Set AS31XM1 name err. (dev_id=%u  ret=%d)\n", chip_info->dev_id, ret);
        return ret;
    }
    return 0;
#endif

    for (i = 0; i < sizeof(g_chip_value_name_map) / sizeof(chip_value_name_map_t); ++i) {
        if (chip_aicore_info->chip_name != g_chip_value_name_map[i].chip_value) {
            continue;
        }

        if (is_container_split == true) {
#ifndef CFG_FEATURE_VFG
            ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%svir%02u",
                g_chip_value_name_map[i].chip_name, chip_aicore_info->num);
#else
            ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%s",
                g_chip_value_name_map[i].chip_name);
#endif
            if (ret < 0) {
                dms_err("Translate chip name failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
                return ret;
            }

            return 0;
        }

        ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%s", g_chip_value_name_map[i].chip_name);
        if (ret < 0) {
            dms_err("Translate chip name failed. (ret=%d)\n", ret);
            return ret;
        }

        if (chip_aicore_info->chip_name == g_chip_value_name_map[DEV_AICORE_FREQ_LEVEL_CLOUD].chip_value) {
            ret = dms_set_chip_name_by_aicore_num_and_freq(chip_info, chip_aicore_info);
            if (ret != 0) {
                dms_err("Set chip name by aicore number and freq fail. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
                return ret;
            }
        }

        if (chip_aicore_info->chip_name == g_chip_value_name_map[DEV_AICORE_FREQ_LEVEL_MINIV2].chip_value) {
            ret = dms_set_chip_name_only_by_aicore_num(chip_info, chip_aicore_info);
            if (ret != 0) {
                dms_err("Set chip name only by aicore number fail. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
                return ret;
            }
        }

        break;
    }

    return 0;
}

STATIC int dms_update_chip_info(devdrv_query_chip_info_t *chip_info, dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret = DRV_ERROR_NONE;
    int tmp;

#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2)
    tmp = sprintf_s((char *)chip_info->info.version, MAX_CHIP_NAME, "V%01x", chip_aicore_info->chip_ver);
#else
    /* mini chip info register, chip version is 0-11 bit, 3bytes, so need V%03x */
    tmp = sprintf_s((char *)chip_info->info.version, MAX_CHIP_NAME, "V%03x", chip_aicore_info->chip_ver);
#endif
    if (tmp < 0) {
        dms_err("get version err. (dev=%u chip_ver=%u ret=%d)\n", chip_info->dev_id, chip_aicore_info->chip_ver, tmp);
        return DRV_ERROR_INNER_ERR;
    }

#ifdef CFG_SOC_PLATFORM_MDC_V51
    switch (chip_aicore_info->chip_ver) {
        case CHIP_TYPE_ASCEND_V1:
            break;
        case CHIP_TYPE_ASCEND_V2:
            ret = strcpy_s((char *)chip_info->info.type, MAX_CHIP_NAME, "BS9SX1A");
            if (ret != 0) {
                dms_err("get type 'BS9SX1A' err. (dev_id=%u chip_ver=%u ret=%d)\n",
                    chip_info->dev_id, chip_aicore_info->chip_ver, ret);
                break;
            }

            ret = strcpy_s((char *)chip_info->info.name, MAX_CHIP_NAME, "A");
            if (ret != 0) {
                dms_err("get chip name 'A' err. (dev_id=%u chip_ver=%u ret=%d)\n",
                    chip_info->dev_id, chip_aicore_info->chip_ver, ret);
            }

            break;
        case CHIP_TYPE_ASCEND_V3:
            break;
        default:
            ret = DRV_ERROR_INVALID_VALUE;
            dms_err("chip_ver is not support! (dev_id=%u chip_ver=%u ret=%d)\n",
                chip_info->dev_id, chip_aicore_info->chip_ver, ret);
            return ret;
    }

    dms_info("dms_update_chip_info success. (dev_id=%u chip_ver=%u)\n", chip_info->dev_id, chip_aicore_info->chip_ver);
#endif

    return ret;
}

static int dms_get_chip_info(unsigned int virt_id, devdrv_query_chip_info_t *chip_info)
{
    int ret;
    u32 vfid = 0;
    bool is_in_container;
    bool is_container_split = false;
    dms_chip_aicore_info_t chip_aicore_info = {0};

    ret = dms_trans_and_check_id(virt_id, &chip_info->dev_id, &vfid);
    if (ret != 0) {
        dms_err("Transfer device id failed. (virt_id=%u; ret=%d)", virt_id, ret);
        return ret;
    }

    is_in_container = devdrv_manager_container_is_in_container();
    if ((is_in_container == true) && (vfid > 0)) {
        is_container_split = true;
    }

    ret = dms_get_chip_aicore_info(chip_info->dev_id, vfid, is_container_split, &chip_aicore_info);
    if (ret != 0) {
        dms_err("Get chip version register info fail. (virt_id=%u; dev_id=%u; ret=%d)\n",
            virt_id, chip_info->dev_id, ret);
        return ret;
    }

    ret = dms_set_chip_type(chip_info->dev_id, chip_info, virt_id);
    if (ret != 0) {
        dms_err("Copy chip type failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    ret = dms_set_chip_name(chip_info, is_container_split, &chip_aicore_info, virt_id);
    if (ret != 0) {
        dms_err("Set chip name failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    ret = dms_set_chip_template_name(chip_info->dev_id, vfid, chip_info);
    if (ret != 0) {
        dms_err("Set chip template name failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    ret = dms_update_chip_info(chip_info, &chip_aicore_info);
    if (ret != 0) {
        dms_err("update chip info failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    return 0;
}

int dms_feature_get_chip_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int virt_id;
    devdrv_query_chip_info_t chip_info = {0};

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input data is NULL or input data length is wrong. (in_len=%u; correct_in_len=%lu)\n",
            in_len, sizeof(unsigned int));
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(devdrv_query_chip_info_t))) {
        dms_err("Output data is NULL or output data length is wrong. (out_len=%u; correct_out_len=%lu)\n",
            out_len, sizeof(devdrv_query_chip_info_t));
        return -EINVAL;
    }

    virt_id = *(unsigned int *)in;
    ret = dms_get_chip_info(virt_id, &chip_info);
    if (ret != 0) {
        dms_err("Get chip info failed. (virt_id=%u; ret=%d)\n", virt_id, ret);
        return ret;
    }

    *(devdrv_query_chip_info_t *)out = chip_info;
    return 0;
}

#ifdef CFG_FEATURE_REBOOT_REASON
STATIC void dms_parse_reboot_reason(struct dms_reboot_reason *reboot_reason)
{
    void __iomem *vaddr = NULL;
    unsigned int i, type;
#define DMS_RESET_REG_NUM  3
    unsigned int reason_val[DMS_RESET_REG_NUM] = {0};
    unsigned int reason_sram_addr[DMS_RESET_REG_NUM] = {
        DMS_RESET_REASON_L3SRAM_ADDR_BASE + RST_REASON_HW_L3SRAM_OFFSET,
        DMS_RESET_REASON_L3SRAM_ADDR_BASE + RST_REASON_HIS_L3SRAM_OFFSET1,
        DMS_RESET_REASON_L3SRAM_ADDR_BASE + RST_REASON_HIS_L3SRAM_OFFSET2 };

    struct dms_reason_priority reason_priority[] = {
#if defined (CFG_SOC_PLATFORM_MDC_V51) || defined (CFG_SOC_MDC_V51_LITE)
        { HW_RST_REASON,       RST_REASON_COLD,           RST_REASON_COLD_RST_BIT,     RST_REASON_COLD_RST_BIT },
        { SW_HIS_RST_REASON_1, RST_REASON_DSMI_CMD,       RST_REASON_BIT_DSMI_CMD,     RST_REASON_BIT_DSMI_CMD },
        { SW_HIS_RST_REASON_1, RST_REASON_REBOOT_CMD,     RST_REASON_BIT_REBOOT_CMD,   RST_REASON_BIT_REBOOT_CMD },
        { SW_HIS_RST_REASON_1, RST_REASON_NO_COMMIT,      RST_REASON_BIT_NO_COMMIT,    RST_REASON_BIT_NO_COMMIT },
        { SW_HIS_RST_REASON_1, RST_REASON_PANIC,          RST_REASON_BIT_PANIC,        RST_REASON_BIT_PANIC },
        { SW_HIS_RST_REASON_1, RST_REASON_BIOS_EXCEPTION, RST_REASON_BIT_BIOS,         RST_REASON_BIT_BIOS },
        { SW_HIS_RST_REASON_2, RST_REASON_UTIMATE_WDG,    RST_REASON_BIT_UTIMATE_WDG,  RST_REASON_BIT_UTIMATE_WDG },
        { SW_HIS_RST_REASON_2, RST_REASON_SUSPEND_FAIL,   RST_REASON_BIT_SUSPEND_FAIL, RST_REASON_BIT_SUSPEND_FAIL },
        { SW_HIS_RST_REASON_2, RST_REASON_RESUME_FAIL,    RST_REASON_BIT_RESUME_FAIL,  RST_REASON_BIT_RESUME_FAIL },
        { SW_HIS_RST_REASON_2, RST_REASON_NOCMT_TIMEOUT,  RST_REASON_BIT_NOCMT_TIMEOUT,RST_REASON_BIT_NOCMT_TIMEOUT },
        { HW_RST_REASON,       RST_REASON_WDG,            RST_DDR_R_WDG_BIT,           RST_DDR_R_WDG_BIT },
        { HW_RST_REASON,       RST_REASON_WDG,            RST_DDR_L_WDG_BIT,           RST_DDR_L_WDG_BIT },
        { HW_RST_REASON,       RST_REASON_WDG,            RST_LP_WDG_BIT,              RST_LP_WDG_BIT },
        { HW_RST_REASON,       RST_REASON_WDG,            RST_SILS_WDG_BIT,            RST_SILS_WDG_BIT },
        { HW_RST_REASON,       RST_REASON_EXTERNAL_PIN,   RST_REASON_PERST_N_BIT,      RST_REASON_PERST_N_BIT },
        { HW_RST_REASON,       RST_REASON_EXTERNAL_PIN,   RST_REASON_RSTIN_N_BIT,      RST_REASON_RSTIN_N_BIT },
        { HW_RST_REASON,       RST_REASON_PCIE_RST,       RST_REASON_PCIE_BIT,         RST_REASON_PCIE_BIT },
        { HW_RST_REASON,       RST_REASON_SOFT_RST,       RST_REASON_SOFT_RST_BIT,     RST_REASON_SOFT_RST_BIT}
#else
        { HW_RST_REASON,       RST_REASON_COLD,           HW_RST_REASON_MASK_V11,      HW_RST_REASON_COLD },
        { SW_HIS_RST_REASON_1, RST_REASON_DSMI_CMD,       RST_REASON_BIT_DSMI_CMD,     RST_REASON_BIT_DSMI_CMD },
        { SW_HIS_RST_REASON_1, RST_REASON_REBOOT_CMD,     RST_REASON_BIT_REBOOT_CMD,   RST_REASON_BIT_REBOOT_CMD },
        { SW_HIS_RST_REASON_1, RST_REASON_NO_COMMIT,      RST_REASON_BIT_NO_COMMIT,    RST_REASON_BIT_NO_COMMIT },
        { SW_HIS_RST_REASON_1, RST_REASON_PANIC,          RST_REASON_BIT_PANIC,        RST_REASON_BIT_PANIC },
        { SW_HIS_RST_REASON_1, RST_REASON_BIOS_EXCEPTION, RST_REASON_BIT_BIOS,         RST_REASON_BIT_BIOS },
        { SW_HIS_RST_REASON_2, RST_REASON_UTIMATE_WDG,    RST_REASON_BIT_UTIMATE_WDG,  RST_REASON_BIT_UTIMATE_WDG },
        { SW_HIS_RST_REASON_2, RST_REASON_SUSPEND_FAIL,   RST_REASON_BIT_SUSPEND_FAIL, RST_REASON_BIT_SUSPEND_FAIL },
        { SW_HIS_RST_REASON_2, RST_REASON_RESUME_FAIL,    RST_REASON_BIT_RESUME_FAIL,  RST_REASON_BIT_RESUME_FAIL },
        { HW_RST_REASON,       RST_REASON_WDG,            HW_RST_REASON_MASK_V11,      HW_RST_REASON_LP_WDOG },
        { HW_RST_REASON,       RST_REASON_WDG,            HW_RST_REASON_MASK_V11,      HW_RST_REASON_DDR_WDOG },
        { HW_RST_REASON,       RST_REASON_EXTERNAL_PIN,   HW_RST_REASON_MASK_V11,      HW_RST_REASON_RSTN_IN },
        { HW_RST_REASON,       RST_REASON_EXTERNAL_PIN,   HW_RST_REASON_MASK_V11,      HW_RST_REASON_PCIE_PAD },
        { HW_RST_REASON,       RST_REASON_PCIE_RST,       HW_RST_REASON_MASK_V11,      HW_RST_REASON_PCIE_HOT },
        { HW_RST_REASON,       RST_REASON_SOFT_RST,       HW_RST_REASON_MASK_V11,      HW_RST_REASON_SOFT }
#endif
};

    reboot_reason->reason = RST_REASON_UNKNOWN;
    reboot_reason->sub_reason = 0;
    for (i = 0; i < DMS_RESET_REG_NUM; i++) {
        vaddr = ioremap(reason_sram_addr[i], DMS_RESET_REASON_SIZE);
        if (vaddr == NULL) {
            devdrv_drv_err("ioremap reason_sram_addr[%u] fail.\n", i);
            return;
        }
        reason_val[i] = ka_mm_readl_relaxed(vaddr);
        iounmap(vaddr);
        vaddr = NULL;
        devdrv_drv_info("reason_val[%u] = 0x%x.\n", i, reason_val[i]);
    }

    for (i = 0; i < sizeof(reason_priority) / sizeof(struct dms_reason_priority); i++) {
        type = reason_priority[i].type;
        if ((reason_val[type] & reason_priority[i].mask) == reason_priority[i].val) {
            reboot_reason->reason = reason_priority[i].reason;
            return;
        }
    }
    return;
}

int dms_get_reboot_reason(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input data is NULL or input data length is wrong. (in_len=%u; correct_in_len=%lu)\n",
            in_len, sizeof(unsigned int));
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dms_reboot_reason))) {
        dms_err("Output data is NULL or output data length is wrong. (out_len=%u; correct_out_len=%lu)\n",
            out_len, sizeof(struct dms_reboot_reason));
        return -EINVAL;
    }

    dms_parse_reboot_reason((struct dms_reboot_reason*)out);
    return 0;
}

#endif

#ifdef CFG_HOST_ENV
int hal_kernel_get_device_chip_die_id(u32 dev_id, u32 *chip_id, u32 *die_id)
{
    struct devdrv_info *dev_info = NULL;

    if ((chip_id == NULL) ||
        (die_id == NULL) ||
        (!uda_is_udevid_exist(dev_id))) {
        dms_err("Invalid parameter. (dev_id=%u; chip_id=%s; die_id=%s)\n",
            dev_id, (chip_id == NULL) ? "NULL" : "OK", (die_id == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *chip_id = dev_info->chip_id;
    *die_id = dev_info->die_id;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_device_chip_die_id);

int hal_kernel_get_soc_type(u32 dev_id, u32 *soc_type)
{
    u32 chip_type;
    struct devdrv_info *dev_info = NULL;
    /*
     * soc type map from uda definition to hal kernel
     ****   uda chip type ******* hal soc type ******
     * HISI_MINI_V1  (0)          SOC_TYPE_MINI
     * HISI_CLOUD_V1 (1)          SOC_TYPE_CLOUD
     * HISI_MINI_V2  (2)          SOC_TYPE_DC
     * HISI_CLOUD_V2 (3)          SOC_TYPE_CLOUD_V2 or SOC_TYPE_CLOUD_V3
     * HISI_MINI_V3  (4)          SOC_TYPE_MINI_V3
     * HISI_MDC_V2   (5)          SOC_TYPE_610LITE
     * HISI_CHIP_NUM (6)          SOC_TYPE_MAX
     * HISI_CHIP_UNKNOWN (7)      SOC_TYPE_MAX
     */
    u32 chip_type_map[HISI_CHIP_NUM] = {SOC_TYPE_MINI, SOC_TYPE_CLOUD, SOC_TYPE_DC, 
        SOC_TYPE_CLOUD_V2, SOC_TYPE_MINI_V3, SOC_TYPE_610LITE};

    if ((soc_type == NULL) ||
        (!uda_is_udevid_exist(dev_id))) {
        dms_err("Invalid parameter. (dev_id=%u; soc_type=%s)\n",
            dev_id, (soc_type == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    chip_type = uda_get_chip_type(dev_id);
    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (chip_type >= HISI_CHIP_NUM) {
        *soc_type = SOC_TYPE_MAX;
    } else if (chip_type == HISI_CLOUD_V2) {
        *soc_type = (dev_info->multi_die == 1) ? SOC_TYPE_CLOUD_V3 : CHIP_CLOUD_V2;
    } else {
        *soc_type = chip_type_map[chip_type];
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_soc_type);
#else
int hal_kernel_get_device_chip_die_id(u32 dev_id, u32 *chip_id, u32 *die_id)
{
    if ((chip_id == NULL) ||
        (die_id == NULL) ||
        (!uda_is_udevid_exist(dev_id))) {
        dms_err("Invalid parameter. (dev_id=%u; chip_id=%s; die_id=%s)\n",
            dev_id, (chip_id == NULL) ? "NULL" : "OK", (die_id == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    return devdrv_get_chip_die_id(dev_id, chip_id, die_id);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_device_chip_die_id);
#endif

int hal_kernel_get_device_addr_mode(u32 dev_id, HAL_KERNEL_ADDR_MODE *addr_mode)
{
    struct devdrv_info *dev_info = NULL;

    if ((addr_mode == NULL) ||
        (!uda_is_udevid_exist(dev_id))) {
        dms_err("Invalid parameter. (dev_id=%u; addr_mode=%s)\n",
            dev_id, (addr_mode == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *addr_mode = (HAL_KERNEL_ADDR_MODE)dev_info->addr_mode;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_device_addr_mode);

int __attribute__((weak)) hal_kernel_get_d2d_topology_type(u32 dev_id1, u32 dev_id2, HAL_KERNEL_TOPOLOGY_TYPE *topology_type)
{
    return -EOPNOTSUPP;
}

int __attribute__((weak)) hal_kernel_get_spod_node_status(u32 dev_id, u32 sdid, u32 *status)
{
    (void) dev_id;
    (void) sdid;
    (void) status;
    return -EOPNOTSUPP;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_spod_node_status);

#ifdef CFG_FEATURE_GET_DEV_UUID
/**********************************************  UUID  ***************************************************
*  UUID format: |---0--|--1--|--2--|-------3--------|--4--|--5--|-6-|-7--|--8---|-- -9------|-----10-----|
*     type:     |  Rsv | Vendor ID | Rsv1 | version | Device id |  Rsv2  | Vnpu id |  Rsv3  |     SN     |
*     size:     | 8bit |   16bit   | 4bit |  4bit   |   16bit   |  16bit |  6bit   |  10bit |   48bit    |
----------------|------|-----------|------|---------|-----------|--------|---------|--------|------------|
* value(910B):  |   0  |  0xCC08   |  0   |    0    |  0xD000   |   0    |  vfid   |   0    |  mac_addr  |
* value(910_93):|   0  |  0xCC08   |  0   |    0    |  0xD001   |   0    |  vfid   |   0    |  mac_addr  |
---------------------------------------------------------------------------------------------------------|
*********************************************************************************************************/
#define DEVICE_VERSION 0
#define VENDOR_ID 0xCC08
STATIC int dms_make_up_uuid(unsigned int phy_id, unsigned int vfid, unsigned int soc_type,
                            struct dms_hal_device_info_stru *output)
{
    char uuid_info[UUID_MAX_LEN];
    unsigned int device_id = 0;
    int i = 0;
    int ret = 0;

    if (atomic_read(&g_mac_init_flag[phy_id]) == 0) {
        dms_err("Mac info init failed. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id, vfid, ret);
        return -EINVAL;
    }

    if (soc_type == SOC_TYPE_CLOUD_V3) {
        device_id = 0xD001;
    } else {
        device_id = 0xD000;
    }

    ret = memset_s(uuid_info, UUID_MAX_LEN, 0, UUID_MAX_LEN);
    if (ret != 0) {
        dms_err("memset_s failed. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id, vfid, ret);
        return -EINVAL;
    }

    uuid_info[1] = (VENDOR_ID >> 8) & 0xFF;
    uuid_info[2] = VENDOR_ID & 0xFF;
    uuid_info[3] = (0 << 4) | DEVICE_VERSION;
    uuid_info[4] = (device_id >> 8) & 0xFF;
    uuid_info[5] = device_id & 0xFF;
    uuid_info[8] = (((uint8_t)vfid) & 0x3F) << 2;

    /* The UUID starts with SN at index 10, and the MAC address starts at index 5, 48 bits in total.*/
    for(i = 0; i < 6; i ++) {
        uuid_info[10 + i] = g_mac_info[phy_id][5 + i];
    }

    ret = memcpy_s(output->payload, sizeof(output->payload), uuid_info, UUID_MAX_LEN);
    if (ret != 0) {
        dms_err("Failed to invoke memcpy_s to copy uuid info. (phy_id=%u; vfid=%u; ret=%d)\n",
            phy_id, vfid, ret);
        return -EINVAL;
    }

    output->buff_size = UUID_MAX_LEN;
    return 0;
}

STATIC int dms_get_device_uuid(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    unsigned int phy_id, vfid;
    struct uda_mia_dev_para mia_para = {0};
    unsigned int soc_type = CHIP_END;
    struct dms_hal_device_info_stru *input = (struct dms_hal_device_info_stru *)in;
    struct dms_hal_device_info_stru *output = (struct dms_hal_device_info_stru *)out;

    if ((feature == NULL) ||
        (in == NULL) || (in_len < DMS_HAL_DEV_INFO_HEAD_LEN) ||
        (out == NULL) || (out_len < DMS_HAL_DEV_INFO_HEAD_LEN)) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
            (feature == NULL) ? "NULL" : "OK",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }

    if ((input->buff_size == 0) || (input->buff_size < UUID_MAX_LEN) ||
        (in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + input->buff_size))) {
        dms_err("Invalid parameter. (in_len=%u; buff_size=%u)\n", in_len, input->buff_size);
        return -EINVAL;
    }

    if (!uda_is_phy_dev(input->dev_id)) {
        ret = uda_udevid_to_mia_devid(input->dev_id, &mia_para);
        if (ret != 0) {
            dms_err("The udevid to mia devid failed. (udev_id=%u; ret=%d)", input->dev_id, ret);
            return ret;
        }
        phy_id = mia_para.phy_devid;
        vfid = mia_para.sub_devid + 1;
    } else {
        phy_id = input->dev_id;
        vfid = 0;
    }

    ret = hal_kernel_get_soc_type(0, &soc_type);
    if ((ret != 0) || (soc_type == (uint32_t)CHIP_END)) {
        dms_err("Get soc type failed. (ret=%d; soc_type=%u)\n", ret, soc_type);
        return -EINVAL;
    }

    ret = dms_make_up_uuid(phy_id, vfid, soc_type, output);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to get uuid. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int dms_get_uuid_thread(void *arg)
{
    int ret = 0;
    unsigned int out_len = 0;
    u32 dev_id = (u32)(uintptr_t)arg;

    ret = devdrv_config_get_mac_info(dev_id, g_mac_info[dev_id], UUID_MAX_LEN, &out_len);
    if (ret != 0) {
        dms_err("Get mac info failed. (devid=0; ret=%d)\n", ret);
        return ret;
    }
    atomic_set(&g_mac_init_flag[dev_id], 1);
    dms_info("Get mac info success from flash.\n");

    return 0;
}

STATIC void dms_mac_info_init(unsigned int dev_id)
{
    struct task_struct *release_task = NULL;

    if (!uda_is_phy_dev(dev_id)) {
        dms_info("Only support pf device. (dev_id=%d)\n", dev_id);
        return;
    }

    release_task = kthread_create(dms_get_uuid_thread, (void *)(uintptr_t)dev_id, "dms_get_uuid");
    if (IS_ERR(release_task) || (release_task == NULL)) {
        dms_err("get uuid Kthread not up to expectations.\n");
        return;
    }

    (void)wake_up_process(release_task);
    return;
}

STATIC void dms_mac_info_uninit(unsigned int dev_id)
{
    (void)dev_id;
    return;
}
#endif

STATIC int dms_get_phy_device_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int phy_id;
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct dms_get_phy_dev_info_out *output = (struct dms_get_phy_dev_info_out *)out;

    if ((feature == NULL) ||
        (in == NULL) || (in_len < sizeof(unsigned int)) ||
        (out == NULL) || (out_len < sizeof(struct dms_get_phy_dev_info_out))) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
            (feature == NULL) ? "NULL" : "OK",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }

    phy_id = *(u32 *)in;
    if (!uda_is_udevid_exist(phy_id)) {
        dms_err("Physical id is invalid. (phy_id=%u)\n", phy_id);
        return -EINVAL;
    }

    dev_cb = dms_get_dev_cb(phy_id);
    if (dev_cb == NULL) {
        dms_err("Get device ctrl block failed. (phy_id=%u)\n", phy_id);
        return -ENODEV;
    }

    if (dev_cb->dev_info == NULL) {
        dms_err("Device ctrl dev_info is null. (phy_id=%u)\n", phy_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    output->chip_id = dev_info->chip_id;
    output->die_id = dev_info->die_id;
    return 0;
}

#define DMS_CHIP_INFO_CMD_NAME "DMS_CHIPINFO"
BEGIN_DMS_MODULE_DECLARATION(DMS_CHIP_INFO_CMD_NAME)
BEGIN_FEATURE_COMMAND()
#ifdef CFG_FEATURE_GET_DEV_UUID
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME,
    DMS_GET_GET_DEVICE_INFO_CMD,
    ZERO_CMD,
    "module=0x0,info=0x36",
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_device_uuid)
#endif
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_PHY_DEVICE_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_phy_device_info)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_chip_info_dev_init(u32 dev_id)
{
#ifdef CFG_FEATURE_GET_DEV_UUID
    dms_mac_info_init(dev_id);
#endif

    dms_info("dms chip info dev init success. (dev_id=%u)\n", dev_id);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(dms_chip_info_dev_init, FEATURE_LOADER_STAGE_5);

void dms_chip_info_dev_uninit(u32 dev_id)
{
#ifdef CFG_FEATURE_GET_DEV_UUID
    dms_mac_info_uninit(dev_id);
#endif

    dms_info("dms chip info dev uninit success. (dev_id=%u)\n", dev_id);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(dms_chip_info_dev_uninit, FEATURE_LOADER_STAGE_5);

int dms_chip_info_init(void)
{
    CALL_INIT_MODULE(DMS_CHIP_INFO_CMD_NAME);

    dms_info("dms chip info init success.\n");
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_chip_info_init, FEATURE_LOADER_STAGE_5);
void dms_chip_info_uninit(void)
{
    CALL_EXIT_MODULE(DMS_CHIP_INFO_CMD_NAME);

    dms_info("dms chip info uninit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_chip_info_uninit, FEATURE_LOADER_STAGE_5);
#endif
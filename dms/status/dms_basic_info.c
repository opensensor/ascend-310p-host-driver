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

#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "ka_base_pub.h"
#include "ka_memory_pub.h"

#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#ifndef CFG_HOST_ENV
#include "dms_device_time_zone.h"
#endif
#ifdef AOS_LLVM_BUILD
#include "dms_pid_map.h"
#endif
#include "dms_pm.h"
#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_chip_info.h"
#include "dms_vdev.h"
#ifndef AOS_LLVM_BUILD
#include "ascend_kernel_hal.h"
#endif
#include "dms_basic_info.h"
#ifdef CFG_FEATURE_DEV_TOPOLOGY
#include "dms_dev_topology.h"
#endif
#ifdef CFG_FEATURE_MEM
#include "dms_mem_info.h"
#endif
#define BUFFER_MAX_SIZE 128
#define FLASH_CONTENT_BUFFER_MAX_SIZE 1024
#define RTC_FLASH_ADDR 0xDE0000
#define RTC_FLASH_SIZE (128 * 1024)
#ifdef CFG_SOC_PLATFORM_MDC_V51
#include "hiss/hsm_info.h"
#endif
#ifndef AOS_LLVM_BUILD
#include "virtmng_interface.h"

#include "dms_sdk_ex_version.h"

int dms_trans_and_check_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid)
{
    int ret;

    if (logical_dev_id >= DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("Wrong logic id. (logic_id=%u)\n", logical_dev_id);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(logical_dev_id, physical_dev_id, vfid);
    if (ret != 0) {
        dms_err("Transfer logic id to phy id failed. (logic_id=%u; ret=%d)\n", logical_dev_id, ret);
        return ret;
    }

    if (*physical_dev_id >= DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("wrong phy id. (logic_id=%u; phy_id=%u)\n", logical_dev_id, *physical_dev_id);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_get_device_split_mode(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int dev_id, phy_id, vfid;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(u32))) {
        dms_err("output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;

    ret = dms_trans_and_check_id(dev_id, &phy_id, &vfid);
    if (ret != 0) {
        dms_err("can't transform virt id. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_VFIO)
    *(unsigned int *)out = vmng_get_device_split_mode(phy_id);
#else
    #ifdef CFG_FEATURE_VFIO_SOC
    *(unsigned int *)out = vmng_get_device_split_mode(phy_id);
    #else
    *(unsigned int *)out = VMNG_NORMAL_NONE_SPLIT_MODE;
    #endif
#endif
    return 0;
}

STATIC int dms_soc_get_pcie_info(void *feature, char *in, u32 in_len,
    char *out, u32 out_len)
{
    struct dmanage_pcie_id_info pcie_id_info = {0};
    struct devdrv_pcie_id_info id_info = {0};
    unsigned int dev_id, virt_id, vfid;
    int ret;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dmanage_pcie_id_info))) {
        dms_err("output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    ret = memcpy_s((void *)&virt_id, sizeof(u32), (void *)in, in_len);
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = dms_trans_and_check_id(virt_id, &dev_id, &vfid);
    if (ret != 0) {
        dms_err("can't transform virt id. (virt_id=%u; ret=%d)\n", virt_id, ret);
        return ret;
    }

    ret = devdrv_get_pcie_id_info(dev_id, &id_info);
    if (ret != 0) {
        dms_err("get pcie id info failed. (ret=%d)\n", ret);
        return ret;
    }

    pcie_id_info.venderid = id_info.venderid;
    pcie_id_info.subvenderid = id_info.subvenderid;
    pcie_id_info.deviceid = id_info.deviceid;
    pcie_id_info.subdeviceid = id_info.subdeviceid;
    pcie_id_info.bus = id_info.bus;
    pcie_id_info.device = id_info.device;
    pcie_id_info.fn = id_info.fn;

    ret = memcpy_s((void *)out, out_len, (void *)&pcie_id_info, sizeof(struct dmanage_pcie_id_info));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
#endif

STATIC s32 dms_get_gpio_status(void *feature, char *in,
    u32 in_len, char *out, u32 out_len)
{
    struct dms_get_gpio *arg = (struct dms_get_gpio *)in;
    unsigned int *status = (unsigned int *)out;
    unsigned int gpio_val;
    unsigned int gpio_num;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_gpio)) ||
        (out == NULL) || (out_len != sizeof(unsigned int))) {
        dms_err("Invalid para. (in=%s; in_len=%u; out=%s; out_len=%u)\n",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }
    if (arg->dev_id >= DEVICE_NUM_MAX) {
        dms_err("Invalid device id. (dev_id=%u)\n", arg->dev_id);
        return -EINVAL;
    }
    gpio_num = arg->gpio_num;
    if (!gpio_is_valid(gpio_num)) {
        dms_err("Invalid GPIO. (num=%u)\n", gpio_num);
        return -EINVAL;
    }

    ret = gpio_request(gpio_num, DEVDRV_GPIO_NAME);
    if (ret != 0) {
        dms_err("GPIO request failed. (num=%u)\n", gpio_num);
        return ret;
    }

    gpio_val = gpio_get_value(gpio_num);

    (void)gpio_free(gpio_num);

    *status = gpio_val;
    return 0;
}

#ifdef CFG_TRS_HB_REFACTOR_FEATURE
STATIC int dms_get_device_init_status(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    u32 logic_id, ret;
    u32 init_status = DRV_STATUS_INITING;
    u32 phys_id = DEVDRV_MAX_DAVINCI_NUM;
    u32 vf_id = 0;

    if ((in == NULL) || (in_len != sizeof(u32)) || (out == NULL) || (out_len != sizeof(u32))) {
        dms_err("Invalid parameter. (in=%s; in_len=%u; out=%s; out_len=%u)\n",
            (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }

    logic_id = *(u32 *)in;
    ret = devdrv_manager_container_logical_id_to_physical_id(logic_id, &phys_id, &vf_id);
    if (ret != 0) {
        devdrv_drv_err("Failed to transfer logical ID to physical ID. (logic_id=%u; ret=%d)\n", logic_id, ret);
        return ret;
    }

    if (manager_info == NULL || manager_info->dev_info[phys_id] == NULL) {
        init_status = DRV_STATUS_INITING;
#ifdef CFG_HOST_ENV
    } else if (manager_info->device_status[phys_id] == DRV_STATUS_COMMUNICATION_LOST) {
        init_status = DRV_STATUS_COMMUNICATION_LOST;
#endif
    } else {
        /* TS heartbeat needs to be checked to determine if the device is normal. */
        init_status = DRV_STATUS_WORK;
    }

    *(u32 *)out = init_status;

    return 0;
}
#endif

#ifdef CFG_SOC_PLATFORM_MDC_V51
STATIC int dms_device_node_para_check_in(char *in, u32 in_len, u32 correct_in_len)
{
    if ((in == NULL) || (in_len != correct_in_len)) {
        dms_err("Input char is NULL or in_len is wrong. (in_len=%u; correct_in_len=%u)\n",
            in_len, correct_in_len);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_device_node_para_check_all(char *in, u32 in_len, u32 correct_in_len,
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

int dms_ctrl_device_node(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    u32 opcode;
    struct dms_ctrl_device_node_in *ctrl_device_node_in;
    int ret = 0;
    void *buf = NULL;
    unsigned int soc_type;

    ret = hal_kernel_get_soc_type(0, &soc_type);
    if (ret != 0 || soc_type == CHIP_END) {
        dms_err("Failed to get soc type. (ret=%d)\n", ret);
        return ret;
    }

    if ((soc_type == CHIP_BS9SX1A) || (soc_type == CHIP_MDC)) {
        return -EOPNOTSUPP;
    }

    // don't care out and out len
    ret = dms_device_node_para_check_in(in, in_len, sizeof(struct dms_ctrl_device_node_in));
    if (ret != 0) {
        return ret;
    }

    ctrl_device_node_in = (struct dms_ctrl_device_node_in *)in;
    opcode = ctrl_device_node_in->sub_cmd;

    if ((ctrl_device_node_in->in_buf == NULL) || (ctrl_device_node_in->in_size == 0) ||
             (ctrl_device_node_in->in_size > BUFFER_MAX_SIZE)) {
        dms_err("Invalid para. (in_buf=%s,in_size=%u).\n",ctrl_device_node_in->in_buf == NULL ? "NULL" : "OK",
                 ctrl_device_node_in->in_size);
        return -EINVAL;
    }

    buf = (void *)dbl_kmalloc(ctrl_device_node_in->in_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buf == NULL) {
        dms_err("kmalloc buf failed.\n");
        return -ENOMEM;
    }

    if (ka_base_copy_from_user(buf, (void *)ctrl_device_node_in->in_buf, ctrl_device_node_in->in_size) != 0) {
        dms_err("copy_from_user failed\n");
        dbl_kfree(buf);
        return -EFAULT;
    }

    switch (opcode) {
        case DMS_DTM_OPCODE_ENABLE_DEVICE:
            ret = dms_devnode_enable_device(ctrl_device_node_in->dev_id,
                ctrl_device_node_in->node_type, ctrl_device_node_in->node_id);
            break;
        case DMS_DTM_OPCODE_DISABLE_DEVICE:
            ret = dms_devnode_disable_device(ctrl_device_node_in->dev_id,
                ctrl_device_node_in->node_type, ctrl_device_node_in->node_id);
            break;
        case DMS_DTM_OPCODE_SET_POWER_INFO:
            ret = dms_devnode_set_power_info(ctrl_device_node_in->dev_id, ctrl_device_node_in->node_type,
                ctrl_device_node_in->node_id, buf, ctrl_device_node_in->in_size);
            break;
        case DMS_DTM_OPCODE_GET_POWER_INFO:
            ret = dms_devnode_get_power_info(ctrl_device_node_in->dev_id, ctrl_device_node_in->node_type,
                ctrl_device_node_in->node_id, buf, ctrl_device_node_in->in_size);
            break;
        default:
            dbl_kfree(buf);
            return -EOPNOTSUPP;
    }

    if (ret != 0) {
        dms_err("opcode=%u err. (devid=%u, node_type=%d, node_id=%d, ret=%d).\n", opcode,
            ctrl_device_node_in->dev_id, ctrl_device_node_in->node_type, ctrl_device_node_in->node_id, ret);
        dbl_kfree(buf);
        return ret;
    }

    if (opcode == DMS_DTM_OPCODE_GET_POWER_INFO) {
        if (ka_base_copy_to_user(ctrl_device_node_in->in_buf, (void *)buf, ctrl_device_node_in->in_size) != 0) {
            dms_err("copy to user fail.\n");
            dbl_kfree(buf);
            return -EFAULT;
        }
    }

    (void)feature;
    dbl_kfree(buf);
    return ret;
}

int dms_get_all_device_node(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int capability;
    struct dms_get_all_device_node_in get_all_device_node_in;
    int ret = 0;
    struct dsmi_dtm_node_s *node_info;
    unsigned int soc_type;

    ret = hal_kernel_get_soc_type(0, &soc_type);
    if (ret != 0 || soc_type == CHIP_END) {
        dms_err("Failed to get soc type. (ret=%d)\n", ret);
        return ret;
    }

    if ((soc_type == CHIP_BS9SX1A) || (soc_type == CHIP_MDC)) {
        return -EOPNOTSUPP;
    }

    ret = dms_device_node_para_check_all(in, in_len, sizeof(struct dms_get_all_device_node_in),
        out, out_len, sizeof(struct dms_get_all_device_node_out));
    if (ret != 0) {
        return ret;
    }
    get_all_device_node_in = *(struct dms_get_all_device_node_in *)in;

    if ((get_all_device_node_in.size == 0) || (get_all_device_node_in.size > BUFFER_MAX_SIZE)) {
        dms_err("Input buffer size is invalid. (buff_size=%u)\n", get_all_device_node_in.size);
        return -EINVAL;
    }

    node_info = (struct dsmi_dtm_node_s *)dbl_kmalloc(get_all_device_node_in.size * sizeof(struct dsmi_dtm_node_s),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node_info == NULL) {
        dms_err("kmalloc node_info failed.\n");
        return -ENOMEM;
    }

    if (ka_base_copy_from_user(node_info, (void *)get_all_device_node_in.node_info,
        get_all_device_node_in.size * sizeof(struct dsmi_dtm_node_s)) != 0) {
        dms_err("copy_from_user failed\n");
        ret = -EFAULT;
        goto free_buff;
    }
    capability = get_all_device_node_in.capability;

    switch (capability) {
        case DMS_DTM_POWER_CAPABILITY:
            ret = dms_traverse_devnode_get_capacity(get_all_device_node_in.dev_id,
                node_info, get_all_device_node_in.size, (unsigned int *)out);
            break;
        default:
            dbl_kfree(node_info);
            return -EOPNOTSUPP;
    }

    if (ret != 0) {
        dms_err("dms_get_all_device_node failed. (capability=%u)\n", capability);
        goto free_buff;
    }

    if (ka_base_copy_to_user(get_all_device_node_in.node_info, (void *)node_info,
        get_all_device_node_in.size * sizeof(struct dsmi_dtm_node_s)) != 0) {
        dms_err("copy to user fail.\n");
        ret = -EFAULT;
    }

free_buff:
    dbl_kfree(node_info);
    (void)feature;
    return ret;
}

int dms_get_flash_content(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_flash_content_in flash_content;
    int ret;
    unsigned char *buf;
    unsigned int soc_type = CHIP_END;

    if ((in == NULL) || (in_len != sizeof(struct dms_flash_content_in))) {
        dms_err("Input char is NULL or in_len is wrong. (in_len=%u; correct_in_len=%u)\n",
            in_len, sizeof(struct dms_flash_content_in));
        return -EINVAL;
    }

    flash_content = *(struct dms_flash_content_in *)in;
    switch (flash_content.type) {
        case DMS_CONTENT_RTC_CONFIG:
            if (flash_content.dev_id >= DEVICE_NUM_MAX || flash_content.dev_id < 0) {
                dms_err("Invalid device id. (dev_id=%d)\n", flash_content.dev_id);
                return -EINVAL;
            }

            ret = hal_kernel_get_soc_type(flash_content.dev_id, &soc_type);
            if (ret != 0 || soc_type == CHIP_END) {
                dms_err("Failed to get soc type. (ret=%d)\n", ret);
                return ret;
            }

            if (soc_type == CHIP_MDC || soc_type == CHIP_BS9SX1A) {
                return -EOPNOTSUPP;
            }

            if (flash_content.size > FLASH_CONTENT_BUFFER_MAX_SIZE || flash_content.size == 0 ||
                flash_content.size + flash_content.offset >= RTC_FLASH_SIZE ||
                flash_content.offset >= RTC_FLASH_SIZE) {
                dms_err("flash_content size or offset is invalid. (size=%u, offset=%u)\n",
                    flash_content.size, flash_content.offset);
                return -EINVAL;
            }

            buf = (unsigned char *)dbl_kmalloc(flash_content.size * sizeof(unsigned char), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
            if (buf == NULL) {
                dms_err("kmalloc buf failed.\n");
                return -ENOMEM;
            }

            if (ka_base_copy_from_user(buf, (void *)flash_content.buf, flash_content.size * sizeof(unsigned char)) != 0) {
                dms_err("copy_from_user failed\n");
                dbl_kfree(buf);
                return -EFAULT;
            }

            ret = sec_flash_read((unsigned int)flash_content.dev_id, RTC_FLASH_ADDR + flash_content.offset,
                buf, flash_content.size);
            if (ret != 0) {
                dms_err("The sec_flash_read failed. (device_id=%d; offset=%u; size=%u; ret=%d)\n",
                    flash_content.dev_id, flash_content.offset, flash_content.size, ret);
                dbl_kfree(buf);
                return -EINVAL;
            }

            if (ka_base_copy_to_user(flash_content.buf, (void *)buf, flash_content.size * sizeof(unsigned char)) != 0) {
                dms_err("copy to user fail.\n");
                dbl_kfree(buf);
                return -EFAULT;
            }
            dbl_kfree(buf);
            break;
        default:
            return -EOPNOTSUPP;
    }

    (void)feature;
    return 0;
}

int dms_set_flash_content(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_flash_content_in flash_content;
    int ret;
    unsigned char *buf;
    unsigned int soc_type = CHIP_END;

    if ((in == NULL) || (in_len != sizeof(struct dms_flash_content_in))) {
        dms_err("Input char is NULL or in_len is wrong. (in_len=%u; correct_in_len=%u)\n",
            in_len, sizeof(struct dms_flash_content_in));
        return -EINVAL;
    }

    flash_content = *(struct dms_flash_content_in *)in;
    switch (flash_content.type) {
        case DMS_CONTENT_RTC_CONFIG:
            if (flash_content.dev_id >= DEVICE_NUM_MAX || flash_content.dev_id < 0) {
                dms_err("Invalid device id. (dev_id=%d)\n", flash_content.dev_id);
                return -EINVAL;
            }

            ret = hal_kernel_get_soc_type(flash_content.dev_id, &soc_type);
            if (ret != 0 || soc_type == CHIP_END) {
                dms_err("Failed to get soc type. (ret=%d)\n", ret);
                return ret;
            }
        
            if (soc_type == CHIP_MDC || soc_type == CHIP_BS9SX1A) {
                return -EOPNOTSUPP;
            }

            if (flash_content.size > FLASH_CONTENT_BUFFER_MAX_SIZE || flash_content.size == 0 ||
                flash_content.size + flash_content.offset >= RTC_FLASH_SIZE ||
                flash_content.offset >= RTC_FLASH_SIZE) {
                dms_err("flash_content.size is invalid. (size=%u, offset=%u)\n", flash_content.size, flash_content.offset);
                return -EINVAL;
            }

            buf = (unsigned char *)dbl_kmalloc(flash_content.size * sizeof(unsigned char), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
            if (buf == NULL) {
                dms_err("kmalloc buf failed.\n");
                return -ENOMEM;
            }

            if (ka_base_copy_from_user(buf, (void *)flash_content.buf, flash_content.size * sizeof(unsigned char)) != 0) {
                dms_err("copy_from_user failed\n");
                dbl_kfree(buf);
                return -EFAULT;
            }

            ret = sec_flash_write((unsigned int)flash_content.dev_id, RTC_FLASH_ADDR + flash_content.offset,
                buf, flash_content.size);
            if (ret != 0) {
                dms_err("The sec_flash_write failed. (device_id=%d; offset=%u; size=%u; ret=%d)\n",
                    flash_content.dev_id, flash_content.offset, flash_content.size, ret);
                dbl_kfree(buf);
                return -EINVAL;
            }

            (void)feature;
            dbl_kfree(buf);
            dms_info("dms_set_flash_content success\n");
            break;
        default:
            return -EOPNOTSUPP;
    }
    return 0;
}
#endif

#if (defined(CFG_HOST_ENV)) && (!defined(CFG_FEATURE_UNSUPPORT_BASIC_INFO))
STATIC int dms_get_basic_info(unsigned int dev_id, int sub_cmd, unsigned int *basic_buffer)
{
    int ret;
    u32 phys_id;
    int vfid = 0;
    struct devdrv_board_info_cache *basic_info_host = NULL;

    if (dev_id >= DEVDRV_MAX_DAVINCI_NUM || basic_buffer == NULL) {
        dms_err("Invalid parameter. (dev_id=%u; dev_maxnum=%d; basic_buffer=%s)\n",
             dev_id, DEVDRV_MAX_DAVINCI_NUM, (basic_buffer == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(dev_id, &phys_id, &vfid);
    if (ret != 0) {
        dms_err("Transform virt id failed. (phys_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    basic_info_host = devdrv_get_board_info_host(phys_id);
    if (basic_info_host == NULL) {
        return -EOPNOTSUPP;
    }

    if (sub_cmd == DMS_SUBCMD_GET_BOARD_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->board_id), sizeof(unsigned int));
    } else if (sub_cmd == DMS_SUBCMD_GET_SLOT_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->slot_id), sizeof(unsigned int));
    } else if (sub_cmd == DMS_SUBCMD_GET_BOM_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->bom_id), sizeof(unsigned int));
    } else if (sub_cmd == DMS_SUBCMD_GET_PCB_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->pcb_id), sizeof(unsigned int));
    } else {
        dms_err("Invalid subcmd. (phys_id=%u; sub_cmd=%u).\n", phys_id, sub_cmd);
        return -EINVAL;
    }
    if (ret != 0) {
        dms_err("basic info memcpy fail. (phys_id=%u; sub_cmd=%u).\n", phys_id, sub_cmd);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_get_basic_info_op(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;
    int sub_cmd;
    unsigned int dev_id;

    if (in == NULL || out == NULL || feature == NULL) {
        dms_err("Invalid parameter. (in=%s; out=%s; feature=%s)\n",
            (in == NULL) ? "NULL" : "OK", (out == NULL) ? "NULL" : "OK", (feature == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    if (in_len != sizeof(unsigned int) || out_len != sizeof(unsigned int)) {
        dms_err("Error parameter. (in_len=%u; correct in_len=%lu; out_len=%u; correct out_len=%lu)\n",
            in_len, sizeof(unsigned int), out_len, sizeof(unsigned int));
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;
    sub_cmd = ((DMS_FEATURE_S*)feature)->sub_cmd;
    ret = dms_get_basic_info(dev_id, sub_cmd, (unsigned int *)out);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Get basic info failed. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}
#endif

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BASIC_INFO)
BEGIN_FEATURE_COMMAND()
#if (defined(CFG_HOST_ENV)) && (!defined(CFG_FEATURE_UNSUPPORT_BASIC_INFO))
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_BOARD_ID_HOST,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_basic_info_op)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_SLOT_ID_HOST,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_basic_info_op)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_BOM_ID_HOST,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_basic_info_op)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_PCB_ID_HOST,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_basic_info_op)
#endif
#ifndef AOS_LLVM_BUILD
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_PCIE_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_soc_get_pcie_info)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_CHIP_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL_USER,
    dms_feature_get_chip_info)
#ifdef CFG_FEATURE_REBOOT_REASON
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_GET_REBOOT_REASON_CMD,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_reboot_reason)
#endif

#ifdef CFG_SOC_PLATFORM_MDC_V51
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_CTRL_DEVICE_NODE_CMD,
    ZERO_CMD,
    NULL,
    "dmp_daemon",
    DMS_SUPPORT_ALL,
    dms_ctrl_device_node)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_ALL_DEVICE_NODE_CMD,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_all_device_node)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_FLASH_CONTENT,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_flash_content)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_SET_FLASH_CONTENT,
    ZERO_CMD,
    NULL,
    "dmp_daemon",
    DMS_SUPPORT_ALL,
    dms_set_flash_content)
#endif
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_DEV_SPLIT_MODE,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_device_split_mode)
#endif
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_GET_GPIO_STATUS_CMD,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ROOT_ONLY,
    dms_get_gpio_status)
#ifdef AOS_LLVM_BUILD
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_BIND_HOST_PID_CMD,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_drv_bind_host_pid)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_UNBIND_HOST_PID_CMD,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_drv_unbind_host_pid)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_QUERY_DEV_PID_CMD,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_drv_query_devpid_by_hostpid)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_QUERY_HOST_PID_CMD,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_drv_query_hostpid_by_devpid)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_PROCESS_SIGN,
    ZERO_CMD,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_drv_get_process_sign)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_HCCL_DEVICE_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_hccl_device_info)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_CORE_SPEC_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_core_spec_info)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_CORE_INUSE_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_core_inuse_info)
#endif
#ifdef CFG_FEATURE_POWER_STATE
#if defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_SET_POWER_STATE_CMD,
    ZERO_CMD,
    NULL,
    "dmp_daemon",
    DMS_SUPPORT_ALL,
    dms_set_power_state)
#else
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_SET_POWER_STATE_CMD,
    ZERO_CMD,
    NULL,
    "dmp_daemon",
    DMS_SUPPORT_MANAGE_PHY,
    dms_set_power_state)
#endif
#endif
#ifdef SUPPORT_VDEV_MEM
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_GET_VDEVICE_INFO,
    NULL,
    "dmp_daemon",
    DMS_SUPPORT_ALL,
    dms_drv_get_vdevice_info)
#endif
#ifdef CFG_FEATURE_DEV_TOPOLOGY
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_DEV_TOPOLOGY,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_feature_get_dev_topology)

ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_DEVICES_TOPOLOGY,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_feature_get_phy_devices_topology)

#endif
#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_SRIOV)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_SRIOV_SWITCH,
    NULL,
    NULL,
    DMS_SUPPORT_ROOT_PHY | DMS_ENV_ADMIN_DOCKER,
    dms_feature_set_sriov_switch)
#endif
#ifndef CFG_HOST_ENV
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SET_TIME_ZONE_SYNC,
    NULL,
    "dmp_daemon",
    DMS_SUPPORT_ALL,
    dms_process_time_sync)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_GET_DEVICE_INFO_CMD,
    ZERO_CMD,
    "module=0x0,info=0x2d",
    NULL,
    DMS_SUPPORT_ALL,
    dms_get_sdk_ex_version)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_GET_SET_DEVICE_INFO_CMD,
    ZERO_CMD,
    "module=0x0,info=0x2d",
#ifdef CFG_BUILD_DEBUG
    "tsdaemon,drv_tsd_daemon",
#else
    "tsdaemon",
#endif
    DMS_SUPPORT_ALL,
    dms_set_sdk_ex_version)
#endif
#ifdef CFG_TRS_HB_REFACTOR_FEATURE
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO,
    DMS_MAIN_CMD_BASIC,
    DMS_SUBCMD_GET_DEV_INIT_STATUS,
    NULL,
    NULL,
    DMS_SUPPORT_ALL_USER,
    dms_get_device_init_status)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

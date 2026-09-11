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
* Create: 2022-08-15
*/
#include "dbl/uda.h"

#include "ka_kernel_def_pub.h"

#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_basic_info.h"
#include "devdrv_user_common.h"
#include "devdrv_common.h"
#include "devdrv_interface.h"
#include "ascend_kernel_hal.h"
#include "dms_dev_topology.h"
#include "devdrv_manager_common.h"

/* NOTICE：
    When the mainboard ID is 0x11, the address is unified addressing(same as hccs switch type),
    but connected via PCIe.
 */
#define HCCS_SWITCH_RANGE_START         0x10 /* bitmap: 0001_0000 */
#define HCCS_SWITCH_RANGE_END           0x1F /* bitmap: 0001_1111 */

#ifdef CFG_HOST_ENV
STATIC int dms_topology_check_in_the_same_os(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    int ret;
    unsigned int master_id_1 = 0;
    unsigned int master_id_2 = 0;

    ret = devdrv_get_master_devid_in_the_same_os(dev_id1, &master_id_1);
    if (ret != 0) {
        dms_err("Get master id failed. (ret=%d; dev_id1=%u)\n", ret, dev_id1);
        return ret;
    }

    ret = devdrv_get_master_devid_in_the_same_os(dev_id2, &master_id_2);
    if (ret != 0) {
        dms_err("Get master id failed. (ret=%d; dev_id2=%u)\n", ret, dev_id2);
        return ret;
    }

    if (master_id_1 == master_id_2) {
        *result = true;
    } else {
        *result = false;
    }

    return 0;
}
#else
STATIC int dms_topology_check_in_the_same_os(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    int ret;
    unsigned int i;
    unsigned int dev_num = 0;
    unsigned int devids[DEVDRV_PF_DEV_MAX_NUM] = {0};

    ret = devdrv_get_devnum(&dev_num);
    if ((ret != 0) || (dev_num > DEVDRV_PF_DEV_MAX_NUM)) {
        dms_err("Failed to obtain the number of devices. (dev_id=%u; ret=%d)\n", dev_id1, ret);
        return ret;
    }

    ret = devdrv_get_devids(devids, dev_num);
    if (ret != 0) {
        dms_err("Failed to obtain the device IDs. (dev_id=%u; ret=%d)\n", dev_id1, ret);
        return ret;
    }

    for (i = 0; i < dev_num; i++) {
        u32 remote_udevid;
        ret = uda_dev_get_remote_udevid(devids[i], &remote_udevid);
        if ((ret == 0) && (remote_udevid == dev_id2)) {
            *result = true;
            return 0;
        }
    }
    *result = false;

    return 0;
}
#endif

#ifdef CFG_FEATURE_CHIP_DIE
STATIC int dms_topology_check_sio(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    int ret;
    struct devdrv_manager_info *dev_manager_info = NULL;

    dev_manager_info = devdrv_get_manager_info();
    if (dev_manager_info == NULL) {
        dms_err("Get devdrv_manager_info failed. (dev_id=%u)\n", dev_id1);
        return -EINVAL;
    }

    if (dev_manager_info->dev_id[dev_id1] == dev_id2) {
        *result = false;
        return 0;
    }

    ret = dms_topology_check_in_the_same_os(dev_id1, dev_id2, result);
    if (ret != 0) {
        dms_err("Failed to check whether they are in the same OS. (dev_id1=%u; dev_id2=%u; ret=%d)\n",
            dev_id1, dev_id2, ret);
        return ret;
    }

    return 0;
}

/* In order to distinguish between uniform and non-uniform address，return HCCS_SW when dev_id1=devid2.
   Runtime component will converts it back to HCCS.
 */
STATIC int dms_topology_check_hccs_sw(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_manager_get_devdrv_info(dev_id1);
    if (dev_info == NULL) {
        dms_warn("Can't get device info. (devid1=%u; devid2=%u)\n", dev_id1, dev_id2);
        *result = false;
        return 0;
    }

    if ((dev_info->mainboard_id >= HCCS_SWITCH_RANGE_START) && (dev_info->mainboard_id <= HCCS_SWITCH_RANGE_END)) {
        *result = true;
    } else {
        *result = false;
    }

    return 0;
}
#endif

#ifdef CFG_FEATURE_TOPOLOGY_BY_SMP
STATIC int dms_topology_check_hccs_by_smp(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    int ret;

    ret = dms_topology_check_in_the_same_os(dev_id1, dev_id2, result);
    if (ret != 0) {
        dms_err("Failed to check whether they are in the same OS. (dev_id1=%u; dev_id2=%u; ret=%d)\n",
            dev_id1, dev_id2, ret);
        return ret;
    }

    return 0;
}
#endif

#ifdef CFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
STATIC int dms_topology_check_hccs_by_hccs_link_status(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    int ret;
    unsigned int i;
    unsigned int chip_id1 = 0;
    unsigned int chip_id2 = 0;
    unsigned int host_devid1 = 0;
    unsigned int host_devid2 = dev_id2;
    unsigned int hccs_link_status = 0;
    unsigned int hccs_group_id[HCCS_GROUP_SUPPORT_MAX_CHIPNUM] = {0};
    struct devdrv_manager_info *dev_manager_info = NULL;
    unsigned int evb_device_num = 1;
    unsigned long flags;
    unsigned int board_id;
    unsigned int mainboard_id;

    if (dev_id1 >= HCCS_GROUP_SUPPORT_MAX_CHIPNUM || dev_id2 >= HCCS_GROUP_SUPPORT_MAX_CHIPNUM) {
        dms_err("Invalid parameter. (dev_id1=%u; dev_id2=%u; max_dev_id=%u)\n",
            dev_id1, dev_id2, HCCS_GROUP_SUPPORT_MAX_CHIPNUM - 1);
        return -EINVAL;
    }

    dev_manager_info = devdrv_get_manager_info();
    if (dev_manager_info == NULL) {
        dms_err("Get devdrv_manager_info failed. (dev_id=%u)\n", dev_id1);
        return -EINVAL;
    }

    spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    if (dev_manager_info->dev_info[dev_id1] == NULL) {
        spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
        dms_err("Get dev_info failed. (dev_id=%u)\n", dev_id1);
        return -EINVAL;
    }
    host_devid1 = dev_manager_info->dev_id[dev_id1];
    chip_id1 = dev_manager_info->dev_info[dev_id1]->chip_id;
    board_id = dev_manager_info->dev_info[dev_id1]->board_id;
    mainboard_id = dev_manager_info->dev_info[dev_id1]->mainboard_id;
    spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
    if (host_devid1 == host_devid2) {
        *result = true;
        return 0;
    }

    /* hccs_group_id: Check whether they are in the same group. */
    /* hccs_link_status: The bit corresponding to chipid is 1, which is HCCS. */
    ret = devdrv_get_hccs_link_status_and_group_id(dev_id1, &hccs_link_status,
        hccs_group_id, HCCS_GROUP_SUPPORT_MAX_CHIPNUM);
    if (ret != 0) {
        dms_err("Get hccs link status and group id failed. (dev_id=%u; ret=%d)\n", dev_id1, ret);
        return ret;
    }

    /* board_id bit7~bit4: Board Type */
    /* PCIE card type */
    if ((board_id & 0xF0) == DMS_PCIE_CARD) {
        *result = false;
        return 0;
    }

    /* EVB card type */
    if (((board_id & 0xF0) == DMS_EVB_CARD)) {
        if (mainboard_id == 0) {
            evb_device_num = 2; /* EVB 2P */
        } else if (mainboard_id == 0x1) {
            evb_device_num = 4; /* EVB 4P */
        }
        for (i = 0; i < evb_device_num; i++) {
            if ((i == chip_id1) || ((hccs_group_id[chip_id1] == hccs_group_id[i]) &&
                (hccs_link_status & (0x1 << i)))) {
                continue;
            } else {
                *result = false;
                return 0;
            }
        }
        *result = true;
        return 0;
    }

    /* MODULE care type */
    if (((host_devid1 < MODULE_CARD_DEVICE_NUM) && (host_devid2 < MODULE_CARD_DEVICE_NUM)) ||
        ((host_devid1 >= MODULE_CARD_DEVICE_NUM) && (host_devid2 >= MODULE_CARD_DEVICE_NUM))) {
        chip_id2 = (host_devid2 < MODULE_CARD_DEVICE_NUM) ? host_devid2 : (host_devid2 - MODULE_CARD_DEVICE_NUM);
        if ((hccs_group_id[chip_id1] == hccs_group_id[chip_id2]) && (hccs_link_status & (0x1 << chip_id2))) {
            *result = true;
        } else {
            *result = false;
        }
    }

    return 0;
}
#endif

static dms_topology_check_t g_topology_check_info[] = {
#ifdef CFG_FEATURE_CHIP_DIE
    {TOPOLOGY_SIO, DMS_TOPOLOGY_CHECK_SIO},
    {TOPOLOGY_HCCS_SW, DMS_TOPOLOGY_CHECK_HCCS_SW},
#endif
    {TOPOLOGY_HCCS, DMS_TOPOLOGY_CHECK_HCCS},
};

/* In device side, dev_id2 is phy id on host, so, the max dev_id in device is also 64 */
#define DMS_MAX_DEV_NUM_IN_HOST 64
/*
  In host side invoke, dev_id1 and dev_id2 is physic id on the host.
  In device side invoke, dev_id1 is physic id on the device, dev_id2 is physic id on the host.
*/
int dms_get_dev_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type)
{
    int ret, i;
    u32 check_num;
    bool result = false;

    if ((dev_id1 >= DEVDRV_PF_DEV_MAX_NUM) || (dev_id2 >= DMS_MAX_DEV_NUM_IN_HOST) || (topology_type == NULL)) {
        dms_err("Invalid parameter. (dev_id1=%u; dev_id2=%u; topology_type=%d)\n",
            dev_id1, dev_id2, (topology_type != NULL));
        return -EINVAL;
    }

    *topology_type = TOPOLOGY_PIX;
    check_num = sizeof(g_topology_check_info) / sizeof(dms_topology_check_t);
    for (i = 0; i < check_num; i++) {
        ret = g_topology_check_info[i].topology_check_handler(dev_id1, dev_id2, &result);
        if (ret != 0) {
            dms_err("Check topology failed. (ret=%d; i=%d)\n", ret, i);
            return ret;
        }

        if (result) {
            *topology_type = g_topology_check_info[i].topology_type;
            return ret;
        }
    }

#ifdef CFG_HOST_ENV
    ret = devdrv_get_dev_topology(dev_id1, dev_id2, topology_type);
    if (ret != 0) {
        dms_err("Get devices topology from pcie failed. (ret=%d; dev_id1=%u; dev_id2=%u)\n", ret, dev_id1, dev_id2);
        return ret;
    }
#endif
    dms_debug("Get topology type success. (dev_id1=%u; dev_id2=%u; topology_type=%d)\n",
        dev_id1, dev_id2, *topology_type);
    return 0;
}
KA_EXPORT_SYMBOL(dms_get_dev_topology);

int dms_feature_get_dev_topology(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    int topology_type = 0;
    struct dms_get_dev_topology_in *input = NULL;
    u32 phy_id_1 = 0, phy_id_2 = 0, vfid_1 = 0, vfid_2 = 0;

    input = (struct dms_get_dev_topology_in *)in;
    if ((in == NULL) || (in_len != sizeof(struct dms_get_dev_topology_in))) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(int))) {
        dms_err("Output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    if ((input->dev_id1 >= DEVDRV_MAX_DAVINCI_NUM) || (input->dev_id2 >= DEVDRV_MAX_DAVINCI_NUM)) {
        dms_err("Input device id invalid. (dev_id1=%u; dev_id2=%u)\n", input->dev_id1, input->dev_id2);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(input->dev_id1, &phy_id_1, &vfid_1);
    if ((ret != 0) || (vfid_1 != 0)) {
        dms_err("Logical id to physical id failed or container env. (ret=%d; dev_id1=%u; vfid1=%u)\n",
            ret, input->dev_id1, vfid_1);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(input->dev_id2, &phy_id_2, &vfid_2);
    if ((ret != 0) || (vfid_2 != 0)) {
        dms_err("Logical id to physical id failed or container env. (ret=%d; dev_id2=%u; vfid2=%u)\n",
            ret, input->dev_id2, vfid_2);
        return -EINVAL;
    }

    if ((!devdrv_manager_is_pf_device(phy_id_1)) || (!devdrv_manager_is_pf_device(phy_id_2))) {
        return -EOPNOTSUPP;
    }

    ret = dms_get_dev_topology(phy_id_1, phy_id_2, &topology_type);
    if (ret != 0) {
        dms_err("Get device topology failed. (ret=%d)\n", ret);
        return ret;
    }

    if (topology_type == TOPOLOGY_PIX || topology_type == TOPOLOGY_PIB || topology_type == TOPOLOGY_PHB ||
       topology_type == TOPOLOGY_SYS) {
        /* Above are all PCIe types, without further distinction, all return as PIX types. */
        topology_type = TOPOLOGY_PIX;
    }

    ret = memcpy_s((void *)out, out_len, (void *)&topology_type, sizeof(int));
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
        return -ENOMEM;
    }

    return 0;
}

int dms_feature_get_phy_devices_topology(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    u32 phy_id;
    u32 vfid;
    u32 devid = 0;
    int topology_type = 0;
    struct dms_get_dev_topology_in *input = NULL;

    input = (struct dms_get_dev_topology_in *)in;
    if ((in == NULL) || (in_len != sizeof(struct dms_get_dev_topology_in))) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(int))) {
        dms_err("Output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(devid, &phy_id, &vfid);
    if (ret != 0) {
        dms_err("Logical id to physical id failed. (ret=%d)\n", ret);
        return ret;
    }

    if (vfid != 0) {
        return -EOPNOTSUPP;
    }

    if ((!devdrv_manager_is_pf_device(input->dev_id1)) || (!devdrv_manager_is_pf_device(input->dev_id2))) {
        return -EOPNOTSUPP;
    }

    ret = dms_get_dev_topology(input->dev_id1, input->dev_id2, &topology_type);
    if (ret != 0) {
        dms_err("Get device topology failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = memcpy_s(out, out_len, &topology_type, sizeof(int));
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
        return -ENOMEM;
    }

    return 0;
}

int hal_kernel_get_d2d_topology_type(u32 dev_id1, u32 dev_id2, HAL_KERNEL_TOPOLOGY_TYPE *topology_type)
{
    if ((topology_type == NULL) || 
        !uda_is_udevid_exist(dev_id1) || 
        !uda_is_udevid_exist(dev_id2)) {
        dms_err("Invalid parameter. (dev_id1=%u; dev_id2=%u; topology_type=%s)\n",
            dev_id1, dev_id2, (topology_type == NULL ? "NULL" : "OK"));
        return -EINVAL;
    }

    if ((!devdrv_manager_is_pf_device(dev_id1)) || (!devdrv_manager_is_pf_device(dev_id2))) {
        return -EOPNOTSUPP;
    }

    return dms_get_dev_topology(dev_id1, dev_id2, (int *)topology_type);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_d2d_topology_type);

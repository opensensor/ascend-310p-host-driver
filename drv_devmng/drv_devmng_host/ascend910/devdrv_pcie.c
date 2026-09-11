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

#include <linux/types.h>
#include <linux/kernel.h>

#include "ka_memory_pub.h"
#include "ka_base_pub.h"

#include "devdrv_pcie.h"

#ifndef CFG_FEATURE_RC_MODE
#include "dbl/uda.h"
#include "devdrv_manager.h"
#include "devdrv_interface.h"
#include "dms_time.h"
#include "dms_hotreset.h"
#include "dbl_mem_alloc_interface.h"
#include "virtmng_interface.h"
#endif

int devdrv_manager_check_permission(void)
{
    u32 root;

    root = (u32)(current->cred->euid.val);
    if (root != 0) {
        devdrv_drv_err("Only the root user can invoke the function.\n");
#ifdef CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
        return -EPERM;
#else
        return -EACCES;
#endif
    }

    if ((devdrv_manager_container_is_in_container() == true) || !capable(CAP_SYS_ADMIN)) {
#ifdef CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
        return -EOPNOTSUPP;
#else
        return -EACCES;
#endif
    }

    return 0;
}

#ifndef CFG_FEATURE_RC_MODE
STATIC bool devdrv_manager_check_is_vf(unsigned int dev_id, unsigned int vfid)
{
    if ((vfid != 0) || (!devdrv_manager_is_pf_device(dev_id))) {
        return true;
    }

    return false;
}

int devdrv_manager_pcie_pre_reset(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    u32 phys_id = 0;
    u32 vfid = 0;
    struct devdrv_pcie_pre_reset para;

    ret = copy_from_user_safe(&para, (void *)((uintptr_t)arg), sizeof(struct devdrv_pcie_pre_reset));
    if (ret) {
        devdrv_drv_err("Failed to invoke copy_from_user_safe. (ret=%d)\n", ret);
        return -EINVAL;
    }

    if (devdrv_manager_container_logical_id_to_physical_id(para.dev_id, &phys_id, &vfid) != 0) {
        devdrv_drv_err("Failed to transfer dev_id. (dev_id=%u)\n", para.dev_id);
        return -EFAULT;
    }

    if (devdrv_manager_check_is_vf(phys_id, vfid)) {
        return -EOPNOTSUPP;
    }
#ifdef CFG_SOC_PLATFORM_CLOUD
    return 0;
#else
    ret = devdrv_manager_check_permission();
    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "devdrv_manager_check_permission failed, ret(%d).\n", ret);
        return ret;
    }

    ret = dms_power_check_phy_mach(phys_id);
    if (ret != 0) {
#ifdef CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
        return -EOPNOTSUPP;
#else
        return -EINVAL;
#endif
    }

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    ret = devdrv_uda_one_dev_ctrl_hotreset(phys_id);
    if (ret != 0) {
        devdrv_drv_err("Call uda_dev_ctrl failed, (phys_id=%u; ret=%d).\n", phys_id, ret);
        return ret;
    }
#endif

    ret = devdrv_pcie_prereset(phys_id);
    if (ret != 0) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
        devdrv_uda_one_dev_ctrl_hotreset_cancel(phys_id);
#endif
    }

    return ret;
#endif
}

int devdrv_manager_pcie_rescan(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    u32 udevid = 0;
    struct devdrv_pcie_rescan para;

    ret = copy_from_user_safe(&para, (void *)((uintptr_t)arg), sizeof(struct devdrv_pcie_rescan));
    if (ret) {
        devdrv_drv_err("Failed to invoke copy_from_user_safe. (ret=%d)\n", ret);
        return -EINVAL;
    }
    ret = uda_ns_node_devid_to_udevid(para.dev_id, &udevid);
    if (ret != 0) {
        devdrv_drv_err("Failed to transfer dev_id to udevid. (dev_id=%u)\n", para.dev_id);
        return -EFAULT;
    }
    if (!uda_is_phy_dev(udevid)) {
        return -EOPNOTSUPP;
    }

#ifdef CFG_SOC_PLATFORM_CLOUD
    return 0;
#else
    ret = devdrv_manager_check_permission();
    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "devdrv_manager_check_permission failed, ret(%d).\n", ret);
        return ret;
    }

    ret = devdrv_pcie_reinit(udevid);

    return ret;
#endif
}

#define ALL_DEVICE_RESET_FLAG 0xff

#ifdef CFG_FEATURE_SRIOV
STATIC int devdrv_manager_disable_sriov(unsigned int dev_id)
{
    int ret;
    int vf_num = 0;

    ret = devdrv_get_pci_enabled_vf_num(dev_id, &vf_num);
    if (ret != 0) {
        devdrv_drv_err("Get enable vf number failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -ENODEV;
    }

    devdrv_drv_info("vf_num. (dev_id=%u; vf_num=%d)\n", dev_id, vf_num);
    if (vf_num > 0) {
        devdrv_drv_info("Disable sriov. (dev_id=%u)\n", dev_id);
        ret = vmngh_disable_sriov(dev_id);
        if (ret != 0) {
            devdrv_drv_err("Disable sriov failed. (dev_id=%u; ret=%d)", dev_id, ret);
            return -ENODEV;
        }
    }

    return 0;
}

int devdrv_manager_check_and_disable_sriov(unsigned int dev_id)
{
    int ret;
    int dev_num, i;
    u32 phy_id = 0;
    u32 vfid = 0;

    /*
     *  1. if device id is DEVDRV_RESET_ALL_DEVICE_ID, check all device's vf num,
     *     if one device sriov is enable, disable it
     *  2. if device id is single device's id, check and disable sriov when it's enable.
     */
    if (dev_id == DEVDRV_RESET_ALL_DEVICE_ID) {
        dev_num = devdrv_get_dev_num();
        if (dev_num <= 0) {
            devdrv_drv_err("There is no pcie device. (dev_id=%u)\n", dev_id);
            return -ENODEV;
        }

        devdrv_drv_info("Device number. (dev_num=%d)\n", dev_num);
        for (i = 0; i < dev_num; i++) {
            ret = devdrv_manager_container_logical_id_to_physical_id(i, &phy_id, &vfid);
            if (ret) {
                devdrv_drv_err("Transform logical id to physical id failed. (logical_id=%d; ret=%d)\n", i, ret);
                return ret;
            }

            ret = devdrv_manager_disable_sriov(phy_id);
            if (ret != 0) {
                devdrv_drv_err("Disable sriov failed. (dev_id=%d; ret=%d)\n", i, ret);
                return ret;
            }
        }
    } else {
        ret = devdrv_manager_disable_sriov(dev_id);
        if (ret != 0) {
            devdrv_drv_err("Disable sriov failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
    }

    return 0;
}
#endif

// UEFI SRAM FLAG
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define DEVDRV_SRAM_BBOX_UEFI_DUMP_OFFSET 0x29004
#else
#define DEVDRV_SRAM_BBOX_UEFI_DUMP_OFFSET 0x0800
#endif
#ifndef CFG_SOC_PLATFORM_CLOUD
#define DEVDRV_SRAM_BBOX_UEFI_DUMP_LEN 20
#else
#define DEVDRV_SRAM_BBOX_UEFI_DUMP_LEN 28
#endif

// BIOS SRAM log (include POINT FLAG)
#ifdef CFG_SOC_PLATFORM_MINIV2
#define DEVDRV_SRAM_BBOX_BIOS_OFFSET 0x14000UL
#else
#define DEVDRV_SRAM_BBOX_BIOS_OFFSET 0x3DC00UL
#endif
#define DEVDRV_SRAM_BBOX_BIOS_LEN 0x400UL

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define DEVDRV_SRAM_BBOX_HBOOT_LEN 0x200000UL
#else
#define DEVDRV_SRAM_BBOX_HBOOT_LEN 0 /* not support */
#endif

// VMCORE
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define DEVDRV_HBM_BBOX_VMCORE_STAT_LEN (2 * sizeof(unsigned int))
#define DEVDRV_HBM_BBOX_VMCORE_LEN 0x80000000UL
#define DEVDRV_HBM_BBOX_KDUMP_LEN (3 * sizeof(unsigned int))
#define DEVDRV_BBOX_CHIP_DFX_FULL_LEN 0x800000UL
#define DEVDRV_BBOX_TS_LOG_LEN 0x100000UL
#define DEVDRV_BBOX_DDR_DUMP_LEN 0x900000UL
#else
#define DEVDRV_HBM_BBOX_VMCORE_STAT_LEN 0 /* not support */
#define DEVDRV_HBM_BBOX_VMCORE_LEN 0 /* not support */
#define DEVDRV_HBM_BBOX_KDUMP_LEN 0 /* not support */
#define DEVDRV_BBOX_CHIP_DFX_FULL_LEN 0 /* not support */
#define DEVDRV_BBOX_TS_LOG_LEN 0 /* not support */
#define DEVDRV_BBOX_DDR_DUMP_LEN 0 /* not support */
#endif

STATIC int drv_pcie_para_check(u32 phys_id, struct devdrv_pcie_read_para *pcie_read_para)
{
    /* The address that can be read on the 910B platform does not involve security information. */
#ifndef CFG_SOC_PLATFORM_CLOUD_V2
    if (pcie_read_para->type == DEVDRV_PCIE_READ_TYPE_SRAM &&
        !(((pcie_read_para->offset >= DEVDRV_SRAM_BBOX_UEFI_DUMP_OFFSET) &&
           (((u64)pcie_read_para->offset + (u64)pcie_read_para->len) <=
            DEVDRV_SRAM_BBOX_UEFI_DUMP_OFFSET + DEVDRV_SRAM_BBOX_UEFI_DUMP_LEN)) ||
          ((pcie_read_para->offset >= DEVDRV_SRAM_BBOX_BIOS_OFFSET) &&
           (((u64)pcie_read_para->offset + (u64)pcie_read_para->len) <=
            DEVDRV_SRAM_BBOX_BIOS_OFFSET + DEVDRV_SRAM_BBOX_BIOS_LEN)))) {
        devdrv_drv_err("can not access sram, dev_id:%u, offset:%u, len:%u.\n", phys_id, pcie_read_para->offset,
                       pcie_read_para->len);
        return -EINVAL;
    }
#endif

    if ((pcie_read_para->type == DEVDRV_PCIE_READ_TYPE_REG_SRAM) &&
        devdrv_manager_check_capability(phys_id, DEVDRV_CAP_IMU_REG_EXPORT)) {
        devdrv_drv_err("do not support read reg sram, dev_id:%u, read type:%u.\n",
            phys_id, pcie_read_para->type);
        return -EINVAL;
    }

    if ((pcie_read_para->type == DEVDRV_PCIE_READ_TYPE_HBOOT_SRAM) &&
        !((pcie_read_para->offset <= DEVDRV_SRAM_BBOX_HBOOT_LEN) &&
        ((u64)pcie_read_para->offset + (u64)pcie_read_para->len) <= DEVDRV_SRAM_BBOX_HBOOT_LEN)) {
        devdrv_drv_err("Can not access sram. (dev_id=%u; offset=%u; len=%u)\n", phys_id, pcie_read_para->offset,
            pcie_read_para->len);
        return -EINVAL;
    }

    if ((pcie_read_para->type == DEVDRV_PCIE_READ_TYPE_VMCORE_STAT) &&
        !((pcie_read_para->offset <= DEVDRV_HBM_BBOX_VMCORE_STAT_LEN) &&
        ((u64)pcie_read_para->offset + (u64)pcie_read_para->len) <= DEVDRV_HBM_BBOX_VMCORE_STAT_LEN)) {
        devdrv_drv_err("Can not access hbm to read vmcore stat. (dev_id=%u; offset=%u; len=%u)\n", phys_id, pcie_read_para->offset,
            pcie_read_para->len);
        return -EINVAL;
    }

    if ((pcie_read_para->type == DEVDRV_PCIE_READ_TYPE_VMCORE_FILE) &&
        !((pcie_read_para->offset <= DEVDRV_HBM_BBOX_VMCORE_LEN) &&
        ((u64)pcie_read_para->offset + (u64)pcie_read_para->len) <= DEVDRV_HBM_BBOX_VMCORE_LEN)) {
        devdrv_drv_err("Can not access hbm to read vmcore. (dev_id=%u; offset=%u; len=%u)\n", phys_id, pcie_read_para->offset,
            pcie_read_para->len);
        return -EINVAL;
    }

    return 0;
}

STATIC int drv_get_data_addr_info(u32 phys_id, enum devdrv_pcie_read_type type, u32 *addr_type)
{
    if (type == DEVDRV_PCIE_READ_TYPE_SRAM) {
        *addr_type = DEVDRV_ADDR_LOAD_RAM;
    } else if (type == DEVDRV_PCIE_READ_TYPE_DDR) {
#ifdef CFG_FEATURE_PCIE_BBOX_ADDR
        *addr_type = DEVDRV_ADDR_BBOX_BASE;
#else
        *addr_type = DEVDRV_ADDR_TEST_BASE;
#endif
    } else if (type == DEVDRV_PCIE_READ_TYPE_REG_SRAM) {
        *addr_type = DEVDRV_ADDR_REG_SRAM_BASE;
    } else {
        *addr_type = DEVDRV_ADDR_HDR_BASE;
    }

#if defined(CFG_HOST_ENV) && defined (CFG_SOC_PLATFORM_CLOUD_V2)
    if (type == DEVDRV_PCIE_READ_TYPE_HBOOT_SRAM) {
        *addr_type = DEVDRV_ADDR_HBOOT_SRAM_MEM;
    }
#endif

#if defined (CFG_SOC_PLATFORM_CLOUD_V2)
    if (type == DEVDRV_PCIE_READ_TYPE_VMCORE_STAT) {
        *addr_type = DEVDRV_ADDR_VMCORE_STAT_HBM_MEM;
    } else if (type == DEVDRV_PCIE_READ_TYPE_CHIP_DFX_LOG) {
        *addr_type = DEVDRV_ADDR_CHIP_DFX_FULL_MEM;
    } else if (type == DEVDRV_PCIE_READ_TYPE_TS_LOG) {
        *addr_type = DEVDRV_ADDR_TS_LOG_MEM;
    } else if (type == DEVDRV_PCIE_READ_TYPE_BBOX_DDR_LOG) {
        *addr_type = DEVDRV_ADDR_BBOX_DDR_DUMP_MEM;
    }
#endif

    return 0;
}

STATIC int drv_pcie_read_proc(struct devdrv_pcie_read_para* para)
{
    u32 type;
    int ret;
    u32 phys_id = para->devId;

    ret = drv_get_data_addr_info(phys_id, para->type, &type);
    if (ret) {
        devdrv_drv_err("get addr by dev_id(%u) failed, type %d, ret(%d).\n", phys_id, para->type, ret);
        return ret;
    }

    ret = devdrv_pcie_read_proc(para->devId, (enum devdrv_addr_type)type, para->offset, para->value, para->len);
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            devdrv_drv_err("Get address failed. (dev_id=%u; type=%d; ret=%d)\n", phys_id, para->type, ret);
        }
        return ret;
    }

    return 0;
}

int drv_pcie_read(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_pcie_read_para pcie_read_para = {0};
    u32 phys_id;
    int ret;

    /* bbox is not support container */
    if (devdrv_manager_container_is_in_container()) {
        devdrv_drv_err("bbox read interface is not support container\n");
        return -EPERM;
    }
    ret = copy_from_user_safe(&pcie_read_para, (void *)((uintptr_t)arg),
                              sizeof(struct devdrv_pcie_read_para));
    if (ret) {
        devdrv_drv_err("copy pcie from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    phys_id = pcie_read_para.devId;

    /* only can read within region for bbox */
    if (drv_pcie_para_check(phys_id, &pcie_read_para)) {
        devdrv_drv_err("dev_id:%u can not access sram.\n", phys_id);
        return -EINVAL;
    }

    ret = drv_pcie_read_proc(&pcie_read_para);
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            devdrv_drv_err("Read proc failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        }
        return ret;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &pcie_read_para, sizeof(struct devdrv_pcie_read_para));
    return ret;
}

STATIC int drv_pcir_log_size_check(struct devdrv_bbox_pcie_logdump *in, unsigned int len_ev)
{
    if ((U32_MAX - in->offset <= in->len) || (in->offset + in->len > len_ev)) {
        devdrv_drv_err("Failed to verify the log length. (dev_id=%u; offset=%u; len=%u; log_type=%u)\n",
            in->devid, in->offset, in->len, len_ev);
        return -EINVAL;
    }

    return 0;
}

STATIC int drv_pcie_log_dump_check(struct devdrv_bbox_pcie_logdump *in)
{
    int ret = 0;

    if (in->devid >= DEVDRV_PF_DEV_MAX_NUM || in->type >= DEVDRV_MAX_PCIE_READ_TYPE || in->len == 0) {
        devdrv_drv_err("Invalid parameter. (dev_id=%u; dev_maxnum=%d; log_type=%u; logtype_maxnum=%d; len=%u)\n",
            in->devid, DEVDRV_PF_DEV_MAX_NUM, in->type, DEVDRV_MAX_PCIE_READ_TYPE, in->len);
        return -EINVAL;
    }

    if (in->buff == NULL) {
        devdrv_drv_err("Invalid parameter, in buff is NULL. (dev_id=%u)\n", in->devid);
        return -EINVAL;
    }

    if (in->type == DEVDRV_PCIE_READ_TYPE_CHIP_DFX_LOG) {
        ret = drv_pcir_log_size_check(in, DEVDRV_BBOX_CHIP_DFX_FULL_LEN);
    } else if (in->type == DEVDRV_PCIE_READ_TYPE_TS_LOG) {
        ret = drv_pcir_log_size_check(in, DEVDRV_BBOX_TS_LOG_LEN);
    } else if (in->type == DEVDRV_PCIE_READ_TYPE_BBOX_DDR_LOG) {
        ret = drv_pcir_log_size_check(in, DEVDRV_BBOX_DDR_DUMP_LEN);
    } else {
        devdrv_drv_err("Invalid log type. (dev_id=%u; log_type=%d)\n", in->devid, in->type);
        return -EINVAL;
    }

    return ret;
}

int devdrv_pcie_devlog_dump(struct devdrv_bbox_pcie_logdump *in)
{
    int ret;
    u32 addr_type;
    u64 phy_addr;
    size_t phy_addr_size;
    void __iomem *vir_addr = NULL;

    ret = drv_pcie_log_dump_check(in);
    if (ret != 0) {
        return ret;
    }

    ret = drv_get_data_addr_info(in->devid, in->type, &addr_type);
    if (ret != 0) {
        devdrv_drv_err("Get addr info type failed. (dev_id=%u; type=%d; ret=%d)\n",
            in->devid, in->type, ret);
        return ret;
    }

    ret = devdrv_get_addr_info(in->devid, addr_type, 0, &phy_addr, &phy_addr_size);
    if (ret != 0) {
        devdrv_drv_err("Get pcie log addr failed. (dev_id=%u; ret=%d)\n", in->devid, ret);
        return ret;
    }

    if ((U32_MAX - in->offset <= in->len) || (in->offset + in->len > phy_addr_size)) {
        devdrv_drv_err("Para offset len check failed. (dev_id=%u; log_type=%d; offset=%u; len=%u; max_offset=%lu)\n",
            in->devid, in->type, in->offset, in->len, phy_addr_size);
        return -EINVAL;
    }

    vir_addr = ka_mm_ioremap(phy_addr + in->offset, in->len);
    if (vir_addr == NULL) {
        devdrv_drv_err("Failed to invoke the ioremap. (dev_id=%u)\n", in->devid);
        return -ENOMEM;
    }

    ret = copy_to_user_safe(in->buff, vir_addr, in->len);
    if (ret != 0) {
        devdrv_drv_err("copy_to_user_safe failed. (dev_id=%u; ret=%d)\n", in->devid, ret);
    }

    ka_mm_iounmap(vir_addr);
    vir_addr = NULL;
    return ret;
}

int drv_pcie_write_para_check(u32 phys_id, struct devdrv_pcie_write_para *pcie_write_para)
{
    if (pcie_write_para->type >= DEVDRV_MAX_PCIE_WRITE_TYPE) {
        devdrv_drv_err("Write type is invalid. (dev_id=%u; type=%d)\n", phys_id, pcie_write_para->type);
        return -EINVAL;
    }

    if ((pcie_write_para->type == DEVDRV_PCIE_WRITE_TYPE_KDUMP) &&
        !((pcie_write_para->offset <= DEVDRV_HBM_BBOX_KDUMP_LEN) &&
        ((u64)pcie_write_para->offset + (u64)pcie_write_para->len) <= DEVDRV_HBM_BBOX_KDUMP_LEN)) {
        devdrv_drv_err("Can not access hbm to write kdump flag. (dev_id=%u; offset=%u; len=%u)\n", phys_id, pcie_write_para->offset,
            pcie_write_para->len);
        return -EINVAL;
    }

    return 0;
}

STATIC int drv_get_write_data_addr_info(u32 phys_id, enum devdrv_pcie_write_type type, u32 *addr_type)
{
    if (type == DEVDRV_PCIE_WRITE_TYPE_KDUMP) {
        *addr_type = DEVDRV_ADDR_KDUMP_HBM_MEM;
        return 0;
    }

    return -EINVAL;
}

int drv_pcie_write_proc(struct devdrv_pcie_write_para* para)
{
    u32 type;
    int ret;
    u32 phys_id = para->devId;

    ret = drv_get_write_data_addr_info(phys_id, para->type, &type);
    if (ret) {
        devdrv_drv_err("get addr by dev_id failed. (devid=%u; type=%d; ret=%d)\n", phys_id, para->type, ret);
        return ret;
    }

    ret = devdrv_pcie_write_proc(para->devId, (enum devdrv_addr_type)type, para->offset, para->value, para->len);
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            devdrv_drv_err("Get address failed. (dev_id=%u; type=%d; ret=%d)\n", phys_id, para->type, ret);
        }
        return ret;
    }

    return 0;
}

int drv_pcie_write(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_BBOX_KDUMP
    struct devdrv_pcie_write_para pcie_write_para = {0};
    u32 phys_id;
    int ret;

    ret = devdrv_manager_check_permission();
    if (ret != 0) {
        return ret;
    }

    ret = copy_from_user_safe(&pcie_write_para, (void *)((uintptr_t)arg),
                              sizeof(struct devdrv_pcie_write_para));
    if (ret) {
        devdrv_drv_err("Copy pcie from user failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    phys_id = pcie_write_para.devId;
    if (!devdrv_manager_is_pf_device(phys_id)) {
        return -EOPNOTSUPP;
    }

    if (drv_pcie_write_para_check(phys_id, &pcie_write_para)) {
        devdrv_drv_err("Parameter check failed. (dev_id=%u)\n", phys_id);
        return -EINVAL;
    }

    ret = drv_pcie_write_proc(&pcie_write_para);
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            devdrv_drv_err("Write proc failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        }
        return ret;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &pcie_write_para, sizeof(struct devdrv_pcie_write_para));
    return ret;
#else
    (void)filep;
    (void)cmd;
    (void)arg;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int drv_pcie_bbox_imu_ddr_read_proc(struct devdrv_pcie_imu_ddr_read_para* para)
{
    int ret;

    ret = devdrv_pcie_read_proc(para->devId, DEVDRV_ADDR_IMU_LOG_BASE, para->offset, para->value, para->len);
    if (ret) {
        devdrv_drv_err("dev_id %u offset %d, ret %d.\n", para->devId, para->offset, ret);
        return ret;
    }

    return 0;
}

/* bbox imu ddr in bar0 ATU12 */
int drv_pcie_bbox_imu_ddr_read(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_SOC_PLATFORM_CLOUD
    struct devdrv_pcie_imu_ddr_read_para imu_ddr_read_para = {0};
    int ret;

    /* bbox is not support container */
    if (devdrv_manager_container_is_in_container()) {
        devdrv_drv_err("bbox imu ddr read interface is not support container\n");
        return -EPERM;
    }
    ret = copy_from_user_safe(&imu_ddr_read_para, (void *)((uintptr_t)arg),
                              sizeof(struct devdrv_pcie_imu_ddr_read_para));
    if (ret) {
        devdrv_drv_err("copy pcie ddr from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = drv_pcie_bbox_imu_ddr_read_proc(&imu_ddr_read_para);
    if (ret) {
        devdrv_drv_err("dev %d imu ddr read proc fail, ret %d.", imu_ddr_read_para.devId, ret);
        return ret;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &imu_ddr_read_para, sizeof(struct devdrv_pcie_imu_ddr_read_para));

    return ret;
#else
    return 0;
#endif
}

int devdrv_manager_p2p_attr_op(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_p2p_attr p2p_attr;
    int ret, pid;
    struct devdrv_manager_context *dev_manager_context = NULL;
    u32 peer_phys_id;
    u32 phys_id = 0;
    u32 vfid = 0;

    if (filep == NULL) {
        devdrv_drv_err("filep is NULL.\n");
        return -EINVAL;
    }

    if (filep->private_data == NULL) {
        devdrv_drv_err("filep private_data is NULL.\n");
        return -EINVAL;
    }

    dev_manager_context = filep->private_data;
    pid = dev_manager_context->pid;

    ret = copy_from_user_safe(&p2p_attr, (void *)((uintptr_t)arg), sizeof(struct devdrv_p2p_attr));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (devdrv_manager_container_logical_id_to_physical_id(p2p_attr.dev_id, &phys_id, &vfid) != 0) {
        devdrv_drv_err("can't transform virt id %u \n", p2p_attr.dev_id);
        return -EFAULT;
    }

    if (devdrv_manager_check_is_vf(phys_id, vfid)) {
        return -EOPNOTSUPP;
    }

    peer_phys_id = p2p_attr.peer_dev_id;

    ret = dms_hotreset_task_cnt_increase(phys_id);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    ret = -EINVAL;

    switch (p2p_attr.op) {
        case DEVDRV_P2P_ADD:
            ret = devdrv_enable_p2p(pid, phys_id, peer_phys_id);
            break;
        case DEVDRV_P2P_DEL:
            ret = devdrv_disable_p2p(pid, phys_id, peer_phys_id);
            break;
        case DEVDRV_P2P_QUERY:
            if (devdrv_is_p2p_enabled(phys_id, peer_phys_id)) {
                ret = 0;
            }
            break;
        case DEVDRV_P2P_CAPABILITY_QUERY:
            ret = devdrv_get_p2p_access_status(phys_id, peer_phys_id, &p2p_attr.status);
            if (ret) {
                devdrv_drv_err("devid %d devdrv_get_p2p_access_status fail %u\n", phys_id, ret);
                dms_hotreset_task_cnt_decrease(phys_id);
                return ret;
            }
            ret = copy_to_user_safe((void *)(uintptr_t)arg, &p2p_attr, sizeof(struct devdrv_p2p_attr));
            break;
        default:
            break;
    }
    dms_hotreset_task_cnt_decrease(phys_id);
    return ret;
}

int devdrv_manager_get_p2p_capability(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_p2p_capability p2p_capability;
    u32 phys_id = 0;
    u32 vfid = 0;
    int ret;

    ret = copy_from_user_safe(&p2p_capability, (void *)((uintptr_t)arg), sizeof(struct devdrv_p2p_capability));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    if (devdrv_manager_container_logical_id_to_physical_id(p2p_capability.dev_id, &phys_id, &vfid) != 0) {
        devdrv_drv_err("can't transform virt id %u \n", p2p_capability.dev_id);
        return -EFAULT;
    }

    ret = dms_hotreset_task_cnt_increase(phys_id);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    ret = devdrv_get_p2p_capability(phys_id, &p2p_capability.capability);
    if (ret) {
        devdrv_drv_err("devid %d devdrv_get_p2p_capability fail %u\n", p2p_capability.dev_id, ret);
        dms_hotreset_task_cnt_decrease(phys_id);
        return ret;
    }
    ret = copy_to_user_safe((void *)(uintptr_t)arg, &p2p_capability, sizeof(struct devdrv_p2p_capability));

    dms_hotreset_task_cnt_decrease(phys_id);
    return ret;
}

#define DEVDRV_MAX_FILE_SIZE (1024 * 100)
#define DEVDRV_STR_MAX_LEN 100
#define DEVDRV_CONFIG_OK 0
#define DEVDRV_CONFIG_FAIL 1
#define DEVDRV_CONFIG_NO_MATCH 1

int drv_get_device_boot_status(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_get_device_boot_status_para get_device_boot_status_para = {0};
    int ret;
    u32 phys_id;
    ret = copy_from_user_safe(&get_device_boot_status_para, (void *)((uintptr_t)arg),
                              sizeof(struct devdrv_get_device_boot_status_para));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    phys_id = get_device_boot_status_para.devId;
    if (phys_id >= DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_err("phys_id %d is invalid\n", phys_id);
        return -EINVAL;
    }

    /* only judge the physical id is available in container */
    if (devdrv_manager_container_is_in_container()) {
        if (!uda_task_can_access_udevid_inherit(current, phys_id)) {
            devdrv_drv_err("device phyid %u is not belong to current docker\n", phys_id);
            return -EFAULT;
        }
    }

    ret = devdrv_get_device_boot_status(phys_id, &get_device_boot_status_para.boot_status);
    if (ret == -ENXIO) {
        devdrv_drv_warn("no such device, ret(%d), dev_id(%u).\n", ret, phys_id);
    } else if (ret) {
        devdrv_drv_err("can not get device boot status, ret(%d), dev_id(%u).\n", ret, phys_id);
        return -EINVAL;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &get_device_boot_status_para,
                            sizeof(struct devdrv_get_device_boot_status_para));

    return ret;
}

int devdrv_manager_get_host_phy_mach_flag(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_get_host_phy_mach_flag_para para = {0};
    u32 phys_id = 0;
    u32 vfid = 0;
    int ret;

    ret = copy_from_user_safe(&para, (void *)((uintptr_t)arg), sizeof(struct devdrv_get_host_phy_mach_flag_para));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(para.devId, &phys_id, &vfid);
    if (ret) {
        devdrv_drv_err("can't transform virt id %u ret(%d)\n", para.devId, ret);
        return -EFAULT;
    }

    if (devdrv_manager_is_mdev_vm_mode(phys_id)) {
        para.host_flag = DEVDRV_HOST_VM_MACH_FLAG;
    } else {
        ret = devdrv_get_host_phy_mach_flag(phys_id, &para.host_flag);
        if (ret) {
            devdrv_drv_err("can not get host flag, dev_id(%u).\n", para.devId);
            return -EINVAL;
        }
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &para, sizeof(struct devdrv_get_host_phy_mach_flag_para));

    return ret;
}

int devdrv_manager_get_master_device_in_the_same_os(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_get_device_os_para get_device_os_para = {0};
    u32 phyid = 0;
    u32 vfid = 0;
    int ret;

    ret = devdrv_manager_container_logical_id_to_physical_id(0, &phyid, &vfid);
    if (ret) {
        devdrv_drv_err("logical_id to physical id failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (devdrv_manager_check_is_vf(phyid, vfid)) {
        return -EOPNOTSUPP;
    }

    ret = copy_from_user_safe(&get_device_os_para, (void *)((uintptr_t)arg),
                              sizeof(struct devdrv_get_device_os_para));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = devdrv_get_master_devid_in_the_same_os(get_device_os_para.dev_id, &(get_device_os_para.master_dev_id));
    if (ret) {
        devdrv_drv_err("failed, ret(%d), dev_id(%u).\n", ret, get_device_os_para.dev_id);
        return ret;
    }

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &get_device_os_para,
                            sizeof(struct devdrv_get_device_os_para));

    return ret;
}

bool devdrv_manager_is_pf_device(unsigned int dev_id)
{
#ifdef CFG_FEATURE_SRIOV
    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return false;
    }
#endif

    return true;
}

bool devdrv_manager_is_mdev_vm_mode(unsigned int dev_id)
{
    return devdrv_is_mdev_vm_boot_mode(dev_id);
}

bool devdrv_manager_is_mdev_vf_vm_mode(unsigned int dev_id)
{
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    return ((devdrv_get_env_boot_type(dev_id) == DEVDRV_MDEV_VF_VM_BOOT) ||
        (devdrv_get_env_boot_type(dev_id) == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT));
#endif
}

int devdrv_manager_is_pm_boot_mode(unsigned int dev_id, bool *is_pm_boot)
{
    unsigned int host_flag;
    int ret;

    ret = devdrv_get_host_phy_mach_flag(dev_id, &host_flag);
    if (ret != 0) {
        devdrv_drv_err("Get host phy_mach_flag failed. (devid=%u; ret=0x%x)\n", dev_id, ret);
        return ret;
    }

    if (host_flag == DEVDRV_HOST_PHY_MACH_FLAG) {
        *is_pm_boot = true;
        return 0;
    }

    *is_pm_boot = false;
    return 0;
}

bool devdrv_manager_is_sriov_support(unsigned int dev_id)
{
    return devdrv_is_sriov_support(dev_id);
}

#endif /* CFG_FEATURE_RC_MODE */


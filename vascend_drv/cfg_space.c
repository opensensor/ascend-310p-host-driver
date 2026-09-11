/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "dvt.h"
#include "vfio_ops.h"
#include "priv_ops.h"

#define DAVINCI_PCI_BASE_CLASS     0xb
#define DAVINCI_COMMON_CFG_COMMAND_IO_ENABLE    1
#define DAVINCI_COMMON_CFG_COMMAND_MEM_ENABLE    (1 << 1)
#define DAVINCI_COMMON_CFG_COMMAND_BUSMASTER_ENABLE    (1 << 2)
#define DAVINCI_COMMON_CFG_COMMAND_PARERR_ENABLE    (1 << 6)
#define DAVINCI_COMMON_CFG_COMMAND_SERR_ENABLE    (1 << 8)
#define DAVINCI_COMMON_CFG_COMMAND_DISINTX_ENABLE    (1 << 10)
#define DAVINCI_COMMON_CFG_COMMAND    (DAVINCI_COMMON_CFG_COMMAND_IO_ENABLE | \
                DAVINCI_COMMON_CFG_COMMAND_MEM_ENABLE | \
                DAVINCI_COMMON_CFG_COMMAND_BUSMASTER_ENABLE | \
                DAVINCI_COMMON_CFG_COMMAND_PARERR_ENABLE | \
                DAVINCI_COMMON_CFG_COMMAND_SERR_ENABLE | \
                DAVINCI_COMMON_CFG_COMMAND_DISINTX_ENABLE)

#define DAVINCI_COMMON_CFG_STATUS_CAP_ENABLE    (1 << 4)
#define DAVINCI_COMMON_CFG_STATUS    DAVINCI_COMMON_CFG_STATUS_CAP_ENABLE
#define DAVINCI_COMMON_CFG_REV_ID    0x71
#define DAVINCI_COMMON_CFG_BASE_CLASS    0x12
#define DAVINCI_COMMON_CFG_CACHE_LINE_SIZE    0x8
#define DAVINCI_COMMON_CFG_BAR_64B    (1 << 2)
#define DAVINCI_COMMON_CFG_BAR_PREFETCHABLE    (1 << 3)
#define DAVINCI_COMMON_CFG_BAR_0    (DAVINCI_COMMON_CFG_BAR_64B | \
                DAVINCI_COMMON_CFG_BAR_PREFETCHABLE)
#define DAVINCI_COMMON_CFG_BAR_2    (DAVINCI_COMMON_CFG_BAR_64B)
#define DAVINCI_COMMON_CFG_BAR_4    (DAVINCI_COMMON_CFG_BAR_64B | \
                DAVINCI_COMMON_CFG_BAR_PREFETCHABLE)
#define DAVINCI_COMMON_CFG_SUBSYSTEM_ID    0x01000300
#define DAVINCI_COMMON_CFG_INT_LINE    0xff
#define DAVINCI_COMMON_CFG_INT_PIN     0x1

#define DAVINCI_PCI_EXP     0x40
#define DAVINCI_PCI_EXP_NEXT_CAP_POINTER    (DAVINCI_PCI_EXP + 1)
#define DAVINCI_PCI_EXP_FLAGS    (DAVINCI_PCI_EXP + PCI_EXP_FLAGS)
#define DAVINCI_PCI_EXP_DEVCAP    (DAVINCI_PCI_EXP + PCI_EXP_DEVCAP)
#define DAVINCI_PCI_EXP_DEVCTL    (DAVINCI_PCI_EXP + PCI_EXP_DEVCTL)
#define DAVINCI_PCI_EXP_LNKCAP    (DAVINCI_PCI_EXP + PCI_EXP_LNKCAP)
#define DAVINCI_PCI_EXP_LNKCTL    (DAVINCI_PCI_EXP + PCI_EXP_LNKCTL)
#define DAVINCI_PCI_EXP_LNKSTA    (DAVINCI_PCI_EXP + PCI_EXP_LNKSTA)
#define DAVINCI_PCI_EXP_DEVCAP2    (DAVINCI_PCI_EXP + PCI_EXP_DEVCAP2)
#define DAVINCI_PCI_EXP_LNKCAP2    (DAVINCI_PCI_EXP + PCI_EXP_LNKCAP2)
#define DAVINCI_PCI_EXP_LNKCTL2    (DAVINCI_PCI_EXP + PCI_EXP_LNKCTL2)
#define DAVINCI_PCI_EXP_LNKSTA2    (DAVINCI_PCI_EXP + PCI_EXP_LNKSTA2)
#define DAVINCI_PCI_EXP_SLTCTL2    (DAVINCI_PCI_EXP + PCI_EXP_SLTCTL2)

#define DAVINCI_EXP_CAP_CFG_CAP_REG     0x0002
#define DAVINCI_EXP_CAP_CFG_DEV_CAP_REG     0x10008fe2
#define DAVINCI_EXP_CAP_CFG_DEV_CONTROL_REG     0x291f
#define DAVINCI_EXP_CAP_CFG_LINK_CAP_REG     0x0043f043
#define DAVINCI_EXP_CAP_CFG_LINK_CONTROL_REG     0x0008
#define DAVINCI_EXP_CAP_CFG_LINK_STATUS_REG     0x0043
#define DAVINCI_EXP_CAP_CFG_DEV_2_CAP_REG     0x00100000
#define DAVINCI_EXP_CAP_CFG_LINK_2_CAP_REG     0x0000000e
#define DAVINCI_EXP_CAP_CFG_LINK_2_CONTROL_REG     0x0003
#define DAVINCI_EXP_CAP_CFG_LINK_2_STATUS_REG     0x001e
#define DAVINCI_EXP_CAP_CFG_SLOT_2_CONTROL_REG     0x001e

#define DAVINCI_MSIX_CAP_CFG_TABLE_SIZE     0x7f
#define DAVINCI_MSIX_CAP_CFG_TABLE_SIZE_VF  0xff
#define DAVINCI_MSIX_CAP_CFG_MSIX_ENABLE    (1 << 15)
#define DAVINCI_MSIX_CAP_CFG_CONTROL    (DAVINCI_MSIX_CAP_CFG_TABLE_SIZE | \
                DAVINCI_MSIX_CAP_CFG_MSIX_ENABLE)
#define DAVINCI_MSIX_CAP_CFG_CONTROL_VF  (DAVINCI_MSIX_CAP_CFG_TABLE_SIZE_VF | \
                DAVINCI_MSIX_CAP_CFG_MSIX_ENABLE)
#define DAVINCI_MSIX_CAP_CFG_MSIX_TABLE_OFFSET    0x00010000
#define DAVINCI_MSIX_CAP_CFG_PBA_TABLE_OFFSET    0x00014000
#define DAVINCI_MSIX_CAP_CFG_MSIX_TABLE_OFFSET_VF   0x7000000
#define DAVINCI_MSIX_CAP_CFG_PBA_TABLE_OFFSET_VF    0x7004000

struct hw_vdavinci_cfg_init_ops {
    unsigned short device;
    void (*init_cfg_space)(struct hw_vdavinci *vdavinci);
};

static void init_910b_cfg_space(struct hw_vdavinci *vdavinci);
static void init_910_93_cfg_space(struct hw_vdavinci *vdavinci);
static void init_950_cfg_space(struct hw_vdavinci *vdavinci);
static void init_common_cfg_space(struct hw_vdavinci *vdavinci);

STATIC const struct hw_vdavinci_cfg_init_ops vdavinci_cfg_init_ops[] = {
    { PCI_DEVICE_ID_ASCEND910B, init_910b_cfg_space },
    { PCI_DEVICE_ID_ASCEND910_93, init_910_93_cfg_space },
    { PCI_DEVICE_ID_ASCEND950, init_950_cfg_space },
    { PCI_ANY_ID, init_common_cfg_space },
    { }
};

static inline void hw_vdavinci_write_pci_bar(struct hw_vdavinci *vdavinci,
                                             u32 offset, u32 val, bool low)
{
    u32 *pval;
    u32 pval_offset;

    /* BAR offset should be 32 bits algiend */
    pval_offset = rounddown(offset, BAR_OFFSET_ALIGN);
    pval = (u32 *)(vdavinci_cfg_space(vdavinci) + pval_offset);

    if (low) {
        /*
         * only update bit 31 - bit 4,
         * leave the bit 3 - bit 0 unchanged.
         */
        *pval = (val & GENMASK(MASK_HIGH_BIT, MASK_MID_LOW_BIT)) | (*pval & GENMASK(MASK_MID_HIGH_BIT, 0));
    } else {
        *pval = val;
    }
}

/**
 * vdavinci_pci_cfg_mem_write - write virtual cfg space memory
 * @vdavinci: target vdavinci
 * @off: offset
 * @src: src ptr to write
 * @bytes: number of bytes
 *
 * Use this function to write virtual cfg space memory.
 * For standard cfg space, only RW bits can be changed,
 * and we emulates the RW1C behavior of PCI_STATUS register.
 */
STATIC void vdavinci_pci_cfg_mem_write(struct hw_vdavinci *vdavinci, unsigned int off,
                                       u8 *src, unsigned int bytes)
{
    int ret;
    ret = memcpy_s(vdavinci_cfg_space(vdavinci) + off,
                   sizeof(vdavinci_cfg_space(vdavinci)) - off, src, bytes);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "write to vdavinic pci cfg failed, "
            "vid: %u, ret: %d\n", vdavinci->id, ret);
    }
}

int hw_vdavinci_emulate_cfg_read(struct hw_vdavinci *vdavinci,
                                 unsigned int offset, void *buf, unsigned int bytes)
{
    int ret;
    unsigned int maxsize;

    if (WARN_ON(bytes > 8)) {
        return -EINVAL;
    }

    if (WARN_ON(offset + bytes > PCI_CFG_SPACE_EXP_SIZE)) {
        return -EINVAL;
    }

    maxsize = PCI_CFG_SPACE_EXP_SIZE - offset;
    maxsize = maxsize < bytes ? maxsize : bytes;
    ret = memcpy_s(buf, bytes, vdavinci_cfg_space(vdavinci) + offset, maxsize);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "read vdavinci cfg failed, "
                "err happen in memcpy_s, vid: %u, ret: %d,"
                "bytes: %u, minsize: %u\n", vdavinci->id, ret, bytes, maxsize);
    }
    return 0;
}

STATIC int emulate_pci_bar_write(struct hw_vdavinci *vdavinci,
                                 unsigned int offset, const void *p_data, unsigned int bytes)
{
    u32 new = *(u32 *)(p_data);
    bool lo = IS_ALIGNED(offset, BAR_SIZE_ALIGN);
    u64 size;
    int ret = 0;
    struct hw_vdavinci_pci_bar *bars = vdavinci->cfg_space.bar;

    /*
     * Power-up software can determine how much addr
     * space the device requires by writing a value of
     * all 1's to the register and then reading the value
     * back. The device will return 0's in all don't-care
     * address bits.
     */
    if (new == 0xffffffff) {
        switch (offset) {
            case PCI_BASE_ADDRESS_0:
            case PCI_BASE_ADDRESS_1:
                size = ~(bars[VFIO_PCI_BAR0_REGION_INDEX].size - 1);
                hw_vdavinci_write_pci_bar(vdavinci, offset,
                                          size >> (lo ? 0 : BAR_OFFSET_LENGTH), lo);
                break;
            case PCI_BASE_ADDRESS_2:
            case PCI_BASE_ADDRESS_3:
                size = ~(bars[VFIO_PCI_BAR2_REGION_INDEX].size - 1);
                hw_vdavinci_write_pci_bar(vdavinci, offset,
                                          size >> (lo ? 0 : BAR_OFFSET_LENGTH), lo);
                break;
            case PCI_BASE_ADDRESS_4:
            case PCI_BASE_ADDRESS_5:
                size = ~(bars[VFIO_PCI_BAR4_REGION_INDEX].size - 1);
                hw_vdavinci_write_pci_bar(vdavinci, offset,
                                          size >> (lo ? 0 : BAR_OFFSET_LENGTH), lo);
                break;
            default:
                /* Unimplemented BARs vascend_err */
                vascend_err(vdavinci_to_dev(vdavinci), "PCI config write @0x%x of "
                    "%d bytes not handled, vid: %u\n", offset, bytes, vdavinci->id);
        }
    } else {
        hw_vdavinci_write_pci_bar(vdavinci, offset, new, lo);
    }
    return ret;
}

STATIC int vdavinci_func_level_reset(struct hw_vdavinci *vdavinci)
{
    int ret;

    if (!(*(u32 *)&vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_DEVCAP]
                & PCI_EXP_DEVCAP_FLR)) {
        vascend_info(vdavinci_to_dev(vdavinci), "FLR isn't supported.\n");
        return 0;
    }

    vascend_info(vdavinci_to_dev(vdavinci), "Begin to FLR, vid: %u.\n", vdavinci->id);
    mutex_lock(&vdavinci->vdavinci_lock);
    ret = vdavinci_priv_vdev_flr(vdavinci);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "reset vdavinci failed, call vdavinci_flr failed, "
                    "vid: %u, ret: %d\n", vdavinci->id, ret);
        mutex_unlock(&vdavinci->vdavinci_lock);
        return ret;
    }
    mutex_unlock(&vdavinci->vdavinci_lock);
    vascend_info(vdavinci_to_dev(vdavinci), "FLR End, vid: %u.\n", vdavinci->id);
    return 0;
}

STATIC int vdavinci_devctl_handle_write(struct hw_vdavinci *vdavinci, void *buf)
{
    /* FLR control, now our flr buf is 16 bits */
    if (*(u16 *)buf & PCI_EXP_DEVCTL_BCR_FLR) {
        return vdavinci_func_level_reset(vdavinci);
    }
    return 0;
}

int hw_vdavinci_emulate_cfg_write(struct hw_vdavinci *vdavinci,
                                  unsigned int offset, void *buf, unsigned int bytes)
{
    if (WARN_ON(bytes > 4)) {
        return -EINVAL;
    }

    if (WARN_ON(offset + bytes > vdavinci->dvt->device_info.cfg_space_size)) {
        return -EINVAL;
    }

    switch (rounddown(offset, 4)) {
        case PCI_BASE_ADDRESS_0 ... PCI_BASE_ADDRESS_5:
            if (WARN_ON(!IS_ALIGNED(offset, 4))) {
                return -EINVAL;
            }
            return emulate_pci_bar_write(vdavinci, offset, buf, bytes);
        case DAVINCI_PCI_EXP_DEVCTL:
            if (vdavinci_devctl_handle_write(vdavinci, buf)) {
                return -EINVAL;
            }
            break;
        default:
            vdavinci_pci_cfg_mem_write(vdavinci, offset, buf, bytes);
            break;
    }
    return 0;
}

STATIC void init_910b_cfg_space(struct hw_vdavinci *vdavinci)
{
    struct pci_dev *pdev = to_pci_dev(vdavinci_resource_dev(vdavinci));
    /* VF BAR */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_0],
               DAVINCI_COMMON_CFG_BAR_0);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR0_REGION_INDEX].size = VF_MMIO_BAR0_SIZE_910B;

    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_2],
               DAVINCI_COMMON_CFG_BAR_2 | DAVINCI_COMMON_CFG_BAR_PREFETCHABLE);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR2_REGION_INDEX].size = (u64)pci_resource_len(pdev,
        VFIO_PCI_BAR2_REGION_INDEX);

    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_4],
               DAVINCI_COMMON_CFG_BAR_4);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR4_REGION_INDEX].size = (u64)pci_resource_len(pdev,
        VFIO_PCI_BAR4_REGION_INDEX);

    /* Subsystem ID for VF */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_SUBSYSTEM_VENDOR_ID],
               pdev->subsystem_vendor);
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_SUBSYSTEM_ID],
               pdev->subsystem_device);

    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_VENDOR_ID],
               pdev->vendor);
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_DEVICE_ID],
               pdev->device);
    /* Base class : 12 */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_BASE_CLASS] = DAVINCI_COMMON_CFG_BASE_CLASS;
}

STATIC void init_910_93_cfg_space(struct hw_vdavinci *vdavinci)
{
    struct pci_dev *pdev = to_pci_dev(vdavinci_resource_dev(vdavinci));
    /* VF BAR */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_0],
               DAVINCI_COMMON_CFG_BAR_0);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR0_REGION_INDEX].size = VF_MMIO_BAR0_SIZE_910_93;

    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_2],
               DAVINCI_COMMON_CFG_BAR_2 | DAVINCI_COMMON_CFG_BAR_PREFETCHABLE);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR2_REGION_INDEX].size = (u64)pci_resource_len(pdev,
        VFIO_PCI_BAR2_REGION_INDEX);

    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_4],
               DAVINCI_COMMON_CFG_BAR_4);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR4_REGION_INDEX].size = (u64)pci_resource_len(pdev,
        VFIO_PCI_BAR4_REGION_INDEX);

    /* Subsystem ID for VF */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_SUBSYSTEM_VENDOR_ID],
               pdev->subsystem_vendor);
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_SUBSYSTEM_ID],
               pdev->subsystem_device);

    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_VENDOR_ID],
               pdev->vendor);
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_DEVICE_ID],
               pdev->device);
    /* Base class : 12 */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_BASE_CLASS] = DAVINCI_COMMON_CFG_BASE_CLASS;
}

STATIC void init_950_cfg_space(struct hw_vdavinci *vdavinci)
{
    struct pci_dev *pdev = to_pci_dev(vdavinci_resource_dev(vdavinci));
    /* VF BAR */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_0],
               DAVINCI_COMMON_CFG_BAR_0);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR0_REGION_INDEX].size = VF_MMIO_BAR0_SIZE_950;
 
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_2],
               DAVINCI_COMMON_CFG_BAR_2 | DAVINCI_COMMON_CFG_BAR_PREFETCHABLE);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR2_REGION_INDEX].size = (u64)pci_resource_len(pdev,
        VFIO_PCI_BAR2_REGION_INDEX);
 
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_4],
               DAVINCI_COMMON_CFG_BAR_4);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR4_REGION_INDEX].size = (u64)pci_resource_len(pdev,
        VFIO_PCI_BAR4_REGION_INDEX);
 
    /* Subsystem ID for VF */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_SUBSYSTEM_VENDOR_ID],
               pdev->subsystem_vendor);
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_SUBSYSTEM_ID],
               pdev->subsystem_device);
 
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_VENDOR_ID],
               pdev->vendor);
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_DEVICE_ID],
               pdev->device);
    /* Base class : 12 */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_BASE_CLASS] = DAVINCI_COMMON_CFG_BASE_CLASS;
}

static void init_common_cfg_space(struct hw_vdavinci *vdavinci)
{
    /* base address registers */
    /* Region 0: (64-bit, prefetchable) */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_0],
               DAVINCI_COMMON_CFG_BAR_0);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR0_REGION_INDEX].size = vdavinci->type->bar0_size;

    /* Region 2: (64-bit, non-prefetchable) */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_2],
               DAVINCI_COMMON_CFG_BAR_2);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR2_REGION_INDEX].size = vdavinci->type->bar2_size;

    /* Region 4: (64-bit, prefetchable) */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_BASE_ADDRESS_4],
               DAVINCI_COMMON_CFG_BAR_4);
    vdavinci->cfg_space.bar[VFIO_PCI_BAR4_REGION_INDEX].size = vdavinci->type->bar4_size;

    /* Subsystem ID */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[PCI_SUBSYSTEM_VENDOR_ID],
               DAVINCI_COMMON_CFG_SUBSYSTEM_ID);

    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_VENDOR_ID],
               vdavinci->dvt->vendor);
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_DEVICE_ID],
               vdavinci->dvt->device);
    /* Base class : 12 */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_BASE_CLASS] = DAVINCI_COMMON_CFG_BASE_CLASS;
}

static void vdavinci_init_common_cfg_space(struct hw_vdavinci *vdavinci)
{
    /* I/O+ Mem+ BusMaster+ SpecCycle- MemWINV- VGASnoop- ParErr+
     * Stepping- SERR+ FastB2B- DisINTx-
     */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_COMMAND],
               DAVINCI_COMMON_CFG_COMMAND);
    /* Status: Cap+ 66MHz- UDF- FastB2B- ParErr- DEVSEL=fast >TAbort-
     * <TAbort- <MAbort- >SERR- <PERR- INTx-
     */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[PCI_STATUS],
               DAVINCI_COMMON_CFG_STATUS);

    /* Rev ID:rev 71 */
    vdavinci_cfg_space(vdavinci)[PCI_REVISION_ID] = DAVINCI_COMMON_CFG_REV_ID;
    /* Cache Line Size */
    vdavinci_cfg_space(vdavinci)[PCI_CACHE_LINE_SIZE] = DAVINCI_COMMON_CFG_CACHE_LINE_SIZE;

    /* Capabilities Pointer */
    vdavinci_cfg_space(vdavinci)[PCI_CAPABILITY_LIST] =  DAVINCI_PCI_EXP;
    /* Interrupt Line : ff */
    vdavinci_cfg_space(vdavinci)[PCI_INTERRUPT_LINE] =  DAVINCI_COMMON_CFG_INT_LINE;
    /* interrupt pin (INTA#) */
    vdavinci_cfg_space(vdavinci)[PCI_INTERRUPT_PIN] =  DAVINCI_COMMON_CFG_INT_PIN;

    vdavinci->cfg_space.init_cfg_space(vdavinci);
}

STATIC void vdavinci_init_express_cap_cfg_space(struct hw_vdavinci *vdavinci)
{
    /* PCI Express Capability List Register Capability ID */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP] =  PCI_CAP_ID_EXP;
    /* PCI Express Capability List Register Next Capability Pointer */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_NEXT_CAP_POINTER] =  DAVINCI_PCI_MSIX;
    /* PCI Express Capabilities Register */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_FLAGS],
               DAVINCI_EXP_CAP_CFG_CAP_REG);

    /* Device Capabilities Register */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_DEVCAP],
               DAVINCI_EXP_CAP_CFG_DEV_CAP_REG);
    /* Device Control Register */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_DEVCTL],
               DAVINCI_EXP_CAP_CFG_DEV_CONTROL_REG);
    /* Link Capabilities Register */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_LNKCAP],
               DAVINCI_EXP_CAP_CFG_LINK_CAP_REG);
    /* Link Control Register */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_LNKCTL],
               DAVINCI_EXP_CAP_CFG_LINK_CONTROL_REG);
    /* Link Status Register */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_LNKSTA],
               DAVINCI_EXP_CAP_CFG_LINK_STATUS_REG);
    /* Device Capabilities 2 Register */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_DEVCAP2],
               DAVINCI_EXP_CAP_CFG_DEV_2_CAP_REG);
    /* Link Capabilities 2 Register */
    STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_LNKCAP2],
               DAVINCI_EXP_CAP_CFG_LINK_2_CAP_REG);
    /* Link Control 2 Register */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_LNKCTL2],
               DAVINCI_EXP_CAP_CFG_LINK_2_CONTROL_REG);
    /* Link Status 2 Register */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_LNKSTA2],
               DAVINCI_EXP_CAP_CFG_LINK_2_STATUS_REG);
    /* Slot Control 2 Register */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_EXP_SLTCTL2],
               DAVINCI_EXP_CAP_CFG_SLOT_2_CONTROL_REG);
}

STATIC void vdavinci_init_msix_cap_cfg_space(struct hw_vdavinci *vdavinci)
{
    /* Capability ID for MSI-X */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX] =  PCI_CAP_ID_MSIX;
    /* Next Pointer for MSI-X */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_NEXT_CAP_POINTER] =  DAVINCI_PCI_PM;
    /* Message Control for MSI-X */
    if (vdavinci->is_passthrough) {
        STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_FLAGS],
                   DAVINCI_MSIX_CAP_CFG_CONTROL_VF);
        /* Message Upper Address for MSI-X */
        STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_TABLE],
                   DAVINCI_MSIX_CAP_CFG_MSIX_TABLE_OFFSET_VF);
        /* Table Offset/BIR for MSI-X
         * offset should be less than the size of bar
         */
        STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_PBA],
                   DAVINCI_MSIX_CAP_CFG_PBA_TABLE_OFFSET_VF);
    } else {
        STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_FLAGS],
                   DAVINCI_MSIX_CAP_CFG_CONTROL);
        /* Message Upper Address for MSI-X */
        STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_TABLE],
                   DAVINCI_MSIX_CAP_CFG_MSIX_TABLE_OFFSET);
        /* Table Offset/BIR for MSI-X
         * offset should be less than the size of bar
         */
        STORE_LE32((u32 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_PBA],
                   DAVINCI_MSIX_CAP_CFG_PBA_TABLE_OFFSET);
    }
}

STATIC void vdavinci_init_pm_cap_cfg_space(struct hw_vdavinci *vdavinci)
{
    /* Power Management Register */
    /* Capability Identifier */
    vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_PM] =  PCI_CAP_ID_PM;
    /* PMC - Power Management Capabilities */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_PM_PMC],
               DAVINCI_PM_CAP_CFG_CAP);
    /* PMCSR - Power Management Control/Status */
    STORE_LE16((u16 *) &vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_PM_CTRL],
               DAVINCI_PM_CAP_CFG_CSR);
}

STATIC void vdavinci_init_ops(struct hw_vdavinci *vdavinci)
{
    int i;
    struct hw_vdavinci_cfg_space *cfg = &(vdavinci->cfg_space);
 
    for (i = 0; vdavinci_cfg_init_ops[i].init_cfg_space != NULL; i++) {
        if (vdavinci->dvt->device == vdavinci_cfg_init_ops[i].device ||
            vdavinci_cfg_init_ops[i].device == (unsigned short)PCI_ANY_ID) {
            cfg->init_cfg_space = vdavinci_cfg_init_ops[i].init_cfg_space;
            break;
        }
    }
}

void hw_vdavinci_init_cfg_space(struct hw_vdavinci *vdavinci)
{
    vdavinci_init_ops(vdavinci);
    vdavinci_init_common_cfg_space(vdavinci);
    vdavinci_init_express_cap_cfg_space(vdavinci);
    vdavinci_init_msix_cap_cfg_space(vdavinci);
    vdavinci_init_pm_cap_cfg_space(vdavinci);
}

void hw_vdavinci_reset_cfg_space(struct hw_vdavinci *vdavinci)
{
    hw_vdavinci_init_cfg_space(vdavinci);
}

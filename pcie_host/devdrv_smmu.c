/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

#include <linux/device.h>
#include <linux/iommu.h>

#include "devdrv_ctrl.h"
#include "devdrv_util.h"
#include "devdrv_msg.h"
#include "devdrv_mem_alloc.h"
#include "devdrv_smmu.h"

#ifdef CFG_FEATURE_AGENT_SMMU
STATIC inline int devdrv_smmu_host_pa_range_check(phys_addr_t pa)
{
    /* host valid phy address segment */
    if (((pa >= DEVDRV_PEH_HOST_CPU0_PA0_START) && (pa < DEVDRV_PEH_HOST_CPU0_PA0_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU0_PA1_START) && (pa < DEVDRV_PEH_HOST_CPU0_PA1_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU1_PA_START) && (pa < DEVDRV_PEH_HOST_CPU1_PA_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU2_PA_START) && (pa < DEVDRV_PEH_HOST_CPU2_PA_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU3_PA_START) && (pa < DEVDRV_PEH_HOST_CPU3_PA_END))) {
        return 0;
    } else {
        return -ENOMEM;
    }
}
#endif

int devdrv_smmu_iova_to_phys_proc(struct devdrv_pci_ctrl *pci_ctrl, dma_addr_t *va, u32 va_cnt, phys_addr_t *pa)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    struct devdrv_host_dma_addr_to_pa_cmd *cmd_data = NULL;
    u32 data_len;
    int ret;
    int i;

    if (pci_ctrl->connect_protocol != CONNECT_PROTOCOL_HCCS) {
        for (i = 0; i < va_cnt; i++) {
            pa[i] = va[i];
        }
        return 0;
    }

    data_len = sizeof(struct devdrv_host_dma_addr_to_pa_cmd) + sizeof(u64) * va_cnt;
    cmd_data = (struct devdrv_host_dma_addr_to_pa_cmd *)devdrv_kzalloc(data_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (cmd_data == NULL) {
        devdrv_err("Alloc cmd_data fail. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }

    cmd_data->sub_cmd = DEVDRV_PEH_HOST_VA_TO_PA;
    cmd_data->host_devid = pci_ctrl->dev_id;
    cmd_data->cnt = va_cnt;
    for (i = 0; i < va_cnt; i++) {
        cmd_data->dma_addr[i] = va[i];
    }

    ret = devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_HCCS_HOST_DMA_ADDR_MAP, cmd_data, data_len,
        cmd_data, data_len);
    if (ret != 0) {
        devdrv_kfree(cmd_data);
        devdrv_err("Dma addr to pa fail(va_cnt=%u, devid=%u, ret=%d, data_len=%u\n",
            va_cnt, pci_ctrl->dev_id, ret, data_len);
        return -ENOMEM;
    }
    for (i = 0; i < va_cnt; i++) {
        pa[i] = cmd_data->dma_addr[i];
        if (unlikely(devdrv_smmu_host_pa_range_check(pa[i]) != 0)) {
            devdrv_warn("Agent smmu va to pa is abnormal. (dev_id=%u)\n", pci_ctrl->dev_id);
        }
    }
    devdrv_kfree(cmd_data);
#endif
    return 0;
}

int devdrv_smmu_iova_to_phys(u32 dev_id, dma_addr_t *va, u32 va_cnt, phys_addr_t *pa)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret;

    if ((va == NULL) || (pa == NULL)) {
        devdrv_err("va or pa is null. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }
    if (va_cnt > DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM ) {
        devdrv_err("va_cnt is invalid. (dev_id=%d, va_cnt=%u)\n", dev_id, va_cnt);
        return -EINVAL;
    }
    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id((u32)dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_smmu_iova_to_phys_proc(pci_ctrl, va, va_cnt, pa);
    if (ret != 0) {
        devdrv_err("devdrv_smmu_iova_to_phys_proc failed. (dev_id=%d, ret=%d)\n", dev_id, ret);
        return ret;
    }
#endif
    return 0;
}
EXPORT_SYMBOL(devdrv_smmu_iova_to_phys);

#ifdef CFG_FEATURE_AGENT_SMMU
STATIC int devdrv_smmu_non_trans_ctrl_msg_recv(void *msg_chan, void *data, u32 in_data_len,
    u32 out_data_len, u32 *real_out_len)
{
    devdrv_info("Smmu rx_msg_process\n");
    return 0;
}

STATIC struct devdrv_non_trans_msg_chan_info devdrv_smmu_non_trans_msg_chan_info = {
    .msg_type = devdrv_msg_client_smmu,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_HIGH,
    .s_desc_size = DEVDRV_SMMU_NON_TRANS_MSG_DESC_SIZE,
    .c_desc_size = DEVDRV_SMMU_NON_TRANS_MSG_DESC_SIZE,
    .rx_msg_process = devdrv_smmu_non_trans_ctrl_msg_recv,
};
#endif

void devdrv_pdev_sid_init(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    struct devdrv_pci_ctrl *pci_ctrl_pf = NULL;
    int pf_dev_id;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
    struct iommu_domain *domain = NULL;
#endif

    if (pci_ctrl->connect_protocol != CONNECT_PROTOCOL_HCCS) {
        pci_ctrl->shr_para->sid = 0;
        return;
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        devdrv_info("In vm, get host pdev sid=0x%x, devid=%u.\n", pci_ctrl->shr_para->sid, pci_ctrl->dev_id);
        return;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
    devdrv_warn("kernel version is low, can not get pdev sid (devid=%u)\n", pci_ctrl->dev_id);
    return;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
    if (pci_ctrl->pdev->dev.iommu_fwspec != NULL) {
        domain = iommu_get_domain_for_dev(&pci_ctrl->pdev->dev);
        /* if domain->type == IOMMU_DOMAIN_IDENTITY, mean os enable iommu pass through */
        if ((domain != NULL) && (domain->type != IOMMU_DOMAIN_IDENTITY)) {
            pci_ctrl->shr_para->sid = pci_ctrl->pdev->dev.iommu_fwspec->ids[0];
        } else {
            pci_ctrl->shr_para->sid = 0;
        }
    } else {
        pci_ctrl->shr_para->sid = 0;
    }
#else
    if ((pci_ctrl->pdev->dev.iommu != NULL) && (pci_ctrl->pdev->dev.iommu->fwspec != NULL)) {
        domain = iommu_get_domain_for_dev(&pci_ctrl->pdev->dev);
        /* if domain->type == IOMMU_DOMAIN_IDENTITY, mean os enable iommu pass through */
        if ((domain != NULL) && (domain->type != IOMMU_DOMAIN_IDENTITY)) {
            pci_ctrl->shr_para->sid = pci_ctrl->pdev->dev.iommu->fwspec->ids[0];
        } else {
            pci_ctrl->shr_para->sid = 0;
        }
    } else {
        pci_ctrl->shr_para->sid = 0;
    }
#endif
    /* hccs peh, virtual machine passthrough can not get sid */
    if ((pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_PF) && (pci_ctrl->shr_para->sid == 0) &&
        (pci_ctrl->pdev->is_physfn == 0)) {
        pci_ctrl->shr_para->sid = DEVDRV_NPU_SID_START -
            DEVDRV_NPU_CHIP_SID_OFFSET * (pci_ctrl->dev_id / DEVDRV_DIE_NUM_OF_ONE_CHIP) +
            DEVDRV_NPU_DIE_SID_OFFSET * (pci_ctrl->dev_id % DEVDRV_DIE_NUM_OF_ONE_CHIP);
    }

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        pf_dev_id = devdrv_sriov_get_pf_devid_by_vf_ctrl(pci_ctrl);
        pci_ctrl_pf = devdrv_get_bottom_half_pci_ctrl_by_id((u32)pf_dev_id);
        if (pci_ctrl_pf != NULL && pci_ctrl_pf->shr_para->sid == 0) {
            pci_ctrl->shr_para->sid = 0;
        }
    }

    devdrv_info("Get host pdev sid=0x%x, devid=%u.\n", pci_ctrl->shr_para->sid, pci_ctrl->dev_id);
#endif
    return;
}

void devdrv_pdev_sid_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    if (pci_ctrl->connect_protocol != CONNECT_PROTOCOL_HCCS) {
        return;
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        return;
    }

    pci_ctrl->shr_para->sid = 0;
#endif
}

int devdrv_smmu_init_msg_chan(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    /* pcie link no need transform dma addr to pa by agent smmu */
    if (pci_ctrl->connect_protocol != CONNECT_PROTOCOL_HCCS) {
        return 0;
    }

    /* only hccs peh's virt pass-through transform dma addr to pa by agent smmu */
    if ((pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) &&
        ((pci_ctrl->shr_para->phy_match_flag != 0) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT))) {
        return 0;
    }

    pci_ctrl->msg_dev->agent_smmu_chan =
        devdrv_pcimsg_alloc_non_trans_queue(pci_ctrl->dev_id, &devdrv_smmu_non_trans_msg_chan_info);
    if (pci_ctrl->msg_dev->agent_smmu_chan == NULL) {
        devdrv_err("Calling agent_smmu_chan failed. (dev_id=%d)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }
#endif
    return 0;
}

void devdrv_smmu_uninit_msg_chan(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    /* pcie link no need transform dma addr to pa by agent smmu */
    if (pci_ctrl->connect_protocol != CONNECT_PROTOCOL_HCCS) {
        return;
    }

    /* only hccs peh's virt pass-through transform dma addr to pa by agent smmu */
    if ((pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) &&
        ((pci_ctrl->shr_para->phy_match_flag != 0) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT))) {
        return;
    }

    if (pci_ctrl->msg_dev->agent_smmu_chan != NULL) {
        devdrv_pcimsg_free_non_trans_queue(pci_ctrl->msg_dev->agent_smmu_chan);
        pci_ctrl->msg_dev->agent_smmu_chan = NULL;
    }
#endif
}

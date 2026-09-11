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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif

#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include <asm/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/jiffies.h>
#include <linux/iommu.h>
#include <linux/kallsyms.h>
#include <linux/dma-mapping.h>
#include <linux/hugetlb.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

#include "devdrv_pcie_link_info.h"
#include "devdrv_ctrl.h"
#include "devdrv_dma.h"
#include "devdrv_pci.h"
#include "devdrv_msg.h"
#include "devdrv_util.h"
#include "res_drv.h"
#include "devdrv_vpc.h"
#include "devdrv_mem_alloc.h"
#include "devdrv_smmu.h"

struct devdrv_ctrl g_ctrls[MAX_DEV_CNT];
struct devdrv_dev_state_ctrl g_state_ctrl;
struct devdrv_pci_state_ctrl g_pci_state_ctrl;
STATIC struct devdrv_client *g_clients[DEVDRV_CLIENT_TYPE_MAX] = {
    NULL,
};
struct mutex g_clients_mutex[DEVDRV_CLIENT_TYPE_MAX];
struct devdrv_black_callback g_black_box = {
    .callback = NULL
};
STATIC struct devdrv_client_instance (*g_clients_instance)[DEVDRV_CLIENT_TYPE_MAX] = NULL;

STATIC struct devdrv_p2p_attr_info (*g_p2p_attr_info)[DEVDRV_P2P_SUPPORT_MAX_DEVICE] = NULL;

STATIC struct devdrv_h2d_attr_info (*g_h2d_attr_info)[DEVDRV_H2D_MAX_ATU_NUM] = NULL;

struct mutex g_devdrv_ctrl_mutex; /* Used to walk the hash */
struct mutex g_devdrv_remove_rescan_mutex;
struct mutex g_devdrv_p2p_mutex;
STATIC u64 g_devdrv_ctrl_hot_reset_status = 0;
STATIC u32 g_env_boot_mode[MAX_PF_DEV_CNT] = {0};

struct devdrv_pci_ctrl_mng g_pci_ctrl_mng;

struct devdrv_ctrl *get_devdrv_ctrl(void)
{
    return g_ctrls;
}

struct devdrv_pci_ctrl_mng *devdrv_get_pci_ctrl_mng(void)
{
    return &g_pci_ctrl_mng;
}

struct mutex *devdrv_get_ctrl_mutex(void)
{
    return &g_devdrv_ctrl_mutex;
}

bool devdrv_is_dev_hot_reset(void)
{
    if (g_devdrv_ctrl_hot_reset_status != 0) {
        return true;
    }
    return false;
}

int devdrv_pci_ctrl_mng_init(void)
{
    int i = 0;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        rwlock_init(&g_pci_ctrl_mng.lock[i]);
    }

    g_pci_ctrl_mng.mng_table =
        (struct devdrv_pci_ctrl **)devdrv_kzalloc(sizeof(struct devdrv_pci_ctrl *) * MAX_DEV_CNT, GFP_KERNEL);
    if (g_pci_ctrl_mng.mng_table == NULL) {
        devdrv_err("The mng_table devdrv_kzalloc failed.\n");
        return -ENOMEM;
    }

    return 0;
}

void devdrv_pci_ctrl_mng_uninit(void)
{
    devdrv_kfree(g_pci_ctrl_mng.mng_table);
    g_pci_ctrl_mng.mng_table = NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static inline void *dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
    void *ret = devdrv_ka_dma_alloc_coherent(dev, size, dma_handle, flag | __GFP_ZERO);
    return ret;
}
#endif

#ifndef readq
static inline u64 readq(void __iomem *addr)
{
    return readl(addr) + ((u64)readl(addr + 4) << 32);
}
#endif

#ifndef writeq
static inline void writeq(u64 value, volatile void *addr)
{
    *(volatile u64 *)addr = value;
}
#endif

void devdrv_pci_ctrl_add(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl)
{
    write_lock(&g_pci_ctrl_mng.lock[dev_id]);
    if (g_pci_ctrl_mng.mng_table[dev_id] == pci_ctrl) {
        write_unlock(&g_pci_ctrl_mng.lock[dev_id]);
        return;
    }
    g_pci_ctrl_mng.mng_table[dev_id] = pci_ctrl;
    atomic_add(1, &pci_ctrl->ref_cnt);
    write_unlock(&g_pci_ctrl_mng.lock[dev_id]);
}

void devdrv_pci_ctrl_del_wait(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl)
{
    u32 wait_cnt = 0;
    int ref_cnt;

    write_lock(&g_pci_ctrl_mng.lock[dev_id]);
    if (g_pci_ctrl_mng.mng_table[dev_id] != NULL) {
        g_pci_ctrl_mng.mng_table[dev_id] = NULL;
        atomic_sub(1, &pci_ctrl->ref_cnt);
    }
    write_unlock(&g_pci_ctrl_mng.lock[dev_id]);

    ref_cnt = atomic_read(&pci_ctrl->ref_cnt);
    while ((ref_cnt > 0) && (wait_cnt < REF_CNT_CHECK_LIMIT)) {
        if ((wait_cnt % REF_CNT_CHECK_PRINT_FREQUENCY) == 0) {
            devdrv_info("Device is being used, please wait. (dev_id=%u; ref_cnt=%d; wait_cnt=%u)\n", dev_id,
                ref_cnt, wait_cnt);
        }
        wait_cnt++;
        usleep_range(REF_CNT_CHECK_WAIT_L, REF_CNT_CHECK_WAIT_H);
        ref_cnt = atomic_read(&pci_ctrl->ref_cnt);
    }
    devdrv_info("Device is ready for hotreset. (dev_id=%u; ref_cnt=%d; wait_cnt=%u)\n", dev_id, ref_cnt, wait_cnt);
}

void devdrv_pci_ctrl_del(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl)
{
    write_lock(&g_pci_ctrl_mng.lock[dev_id]);
    atomic_set(&pci_ctrl->ref_cnt, 0);
    g_pci_ctrl_mng.mng_table[dev_id] = NULL;
    write_unlock(&g_pci_ctrl_mng.lock[dev_id]);
}

static inline struct devdrv_pci_ctrl *devdrv_pci_ctrl_find(u32 dev_id)
{
    return g_pci_ctrl_mng.mng_table[dev_id];
}

struct devdrv_pci_ctrl *devdrv_pci_ctrl_get(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    read_lock(&g_pci_ctrl_mng.lock[dev_id]);
    pci_ctrl = devdrv_pci_ctrl_find(dev_id);
    if (pci_ctrl != NULL) {
        atomic_add(1, &pci_ctrl->ref_cnt);
    }
    read_unlock(&g_pci_ctrl_mng.lock[dev_id]);

    return pci_ctrl;
}

struct devdrv_pci_ctrl *devdrv_pci_ctrl_get_no_ref(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    read_lock(&g_pci_ctrl_mng.lock[dev_id]);
    pci_ctrl = devdrv_pci_ctrl_find(dev_id);
    read_unlock(&g_pci_ctrl_mng.lock[dev_id]);

    return pci_ctrl;
}

void devdrv_pci_ctrl_put(struct devdrv_pci_ctrl *pci_ctrl)
{
    atomic_sub(1, &pci_ctrl->ref_cnt);
}

int devdrv_alloc_attr_info(void)
{
    g_h2d_attr_info = devdrv_kzalloc(MAX_DEV_CNT * DEVDRV_H2D_MAX_ATU_NUM * sizeof(struct devdrv_h2d_attr_info), GFP_KERNEL);
    if (g_h2d_attr_info == NULL) {
        devdrv_err("Alloc h2d_attr_info failed.\n");
        return -ENOMEM;
    }

    g_p2p_attr_info =  devdrv_kzalloc(DEVDRV_P2P_SUPPORT_MAX_DEVICE * DEVDRV_P2P_SUPPORT_MAX_DEVICE *
        sizeof(struct devdrv_p2p_attr_info), GFP_KERNEL);
    if (g_p2p_attr_info == NULL) {
        devdrv_kfree(g_h2d_attr_info);
        g_h2d_attr_info = NULL;
        devdrv_err("Alloc p2p_attr_info failed.\n");
        return -ENOMEM;
    }

    return 0;
}

void devdrv_free_attr_info(void)
{
    if (g_h2d_attr_info != NULL) {
        devdrv_kfree(g_h2d_attr_info);
        g_h2d_attr_info = NULL;
    }
    if (g_p2p_attr_info != NULL) {
        devdrv_kfree(g_p2p_attr_info);
        g_p2p_attr_info = NULL;
    }
    return;
}

int devdrv_get_device_vfid(u32 dev_id, u32 *vf_en, u32 *vf_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if ((vf_en == NULL) || (vf_id == NULL)) {
        devdrv_err("Params is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *vf_en = pci_ctrl->virtfn_flag;
    *vf_id = pci_ctrl->res.dma_res.vf_id;

    return 0;
}
EXPORT_SYMBOL(devdrv_get_device_vfid);

int devdrv_get_devid_by_pfvf_id(u32 pf_id, u32 vf_id, u32 *dev_id)
{
    if ((dev_id == NULL) || (pf_id >= MAX_PF_DEV_CNT) || (vf_id > MAX_VF_CNT_OF_PF)) {
        devdrv_err("Params is err. (pf_id=%u, vf_id=%u)\n", pf_id, vf_id);
        return -EINVAL;
    }

#ifdef CFG_FEATURE_SRIOV
    if (vf_id > 0) {
        *dev_id = pf_id * MAX_VF_CNT_OF_PF + (vf_id - 1) + DEVDRV_SRIOV_VF_DEVID_START;
    } else {
        *dev_id = pf_id;
    }
#else
    *dev_id = pf_id;
#endif
    return 0;
}
EXPORT_SYMBOL(devdrv_get_devid_by_pfvf_id);

int devdrv_get_pfvf_id_by_devid(u32 dev_id, u32 *pf_id, u32 *vf_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if ((pci_ctrl == NULL) || (pci_ctrl->shr_para == NULL)) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if ((pf_id == NULL) || (vf_id == NULL)) {
        devdrv_err("Params is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        *vf_id = pci_ctrl->shr_para->vf_id;
        *pf_id = ((pci_ctrl->shr_para->host_dev_id - DEVDRV_SRIOV_VF_DEVID_START) + 1 - *vf_id) / MAX_VF_CNT_OF_PF;
    } else {
        *vf_id = 0;
        *pf_id = dev_id;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_get_pfvf_id_by_devid);

int devdrv_get_pfvf_type_by_devid(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;
    int pfvf_type;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if (pci_ctrl->virtfn_flag == 1) {
        pfvf_type = DEVDRV_SRIOV_TYPE_VF;
    } else {
        pfvf_type = DEVDRV_SRIOV_TYPE_PF;
    }

    return pfvf_type;
}
EXPORT_SYMBOL(devdrv_get_pfvf_type_by_devid);

int devdrv_sriov_init_instance(u32 dev_id)
{
    int ret, type;

    type = devdrv_get_pfvf_type_by_devid(dev_id);
    if (type != DEVDRV_SRIOV_TYPE_VF) {
        devdrv_err("Verify pfvf type failed. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }

    ret = devdrv_vf_half_probe(dev_id);
    if (ret != 0) {
        devdrv_err("Vf half probe failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_sriov_init_instance);

int devdrv_sriov_uninit_instance(u32 dev_id)
{
    if (devdrv_get_pfvf_type_by_devid(dev_id) != DEVDRV_SRIOV_TYPE_VF) {
        return -EINVAL;
    }

    (void)devdrv_vf_half_free(dev_id);

    return 0;
}
EXPORT_SYMBOL(devdrv_sriov_uninit_instance);

bool devdrv_is_sriov_support(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int total_vfs = 0;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return false;
    }

    if (pci_ctrl->pdev->is_physfn != 0) {
        total_vfs = pci_sriov_get_totalvfs(pci_ctrl->pdev);
        if (total_vfs != 0) {
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(devdrv_is_sriov_support);

STATIC int devdrv_sriov_event_notify(struct devdrv_msg_dev *msg_dev, u32 devid, u32 status)
{
    struct devdrv_sriov_event_notify_cmd cmd_data;
    int ret;

    cmd_data.devid = devid;
    cmd_data.status = status;

    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_SRIOV_EVENT_NOTIFY, &cmd_data, sizeof(cmd_data), NULL, 0);
    if (ret != 0) {
        devdrv_err("Sriov notify fail(devid=%u, status=%u, ret=%d)\n", devid, status, ret);
    }
    return ret;
}

STATIC int devdrv_sriov_event_disable_notify(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    ret = devdrv_sriov_event_notify(pci_ctrl->msg_dev, pci_ctrl->dev_id, DEVDRV_SRIOV_DISABLE);
    if (ret != 0) {
        devdrv_err("Sriov disable notify fail\n");
        return -EINVAL;
    }

    if (pci_ctrl->ops.unbind_irq != NULL) {
        pci_ctrl->ops.unbind_irq(pci_ctrl);
    }
    (void)devdrv_sriov_dma_init_chan(pci_ctrl->dma_dev);
    if (pci_ctrl->ops.bind_irq != NULL) {
        pci_ctrl->ops.bind_irq(pci_ctrl);
    }
    devdrv_res_dma_traffic(pci_ctrl->dma_dev);

    return 0;
}

int devdrv_sriov_enable(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int total_vfs = 0;
    int ret = 0;
    int i;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (pci_ctrl->device_status != DEVDRV_DEVICE_ALIVE) {
        devdrv_err("Device is abnormal, can not enable sriov. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (pci_ctrl->pdev->is_physfn == 0) {
        devdrv_err("Not pf, not support sriov\n");
        return -EINVAL;
    }

    total_vfs = pci_sriov_get_totalvfs(pci_ctrl->pdev);
    if (total_vfs <= DEVDRV_MAX_SRIOV_INSTANCE) {
        devdrv_err("Vf total num less than max_num.(dev_id=%u, total_vfs=%d)\n", dev_id, total_vfs);
        return -EINVAL;
    }

    pci_ctrl->is_sriov_enabled = DEVDRV_SRIOV_ENABLE;
    devdrv_sriov_pf_dma_traffic(pci_ctrl->dma_dev);
    for (i = 1; i < pci_ctrl->dma_dev->remote_chan_num; i++) {
        devdrv_set_dma_chan_status(&pci_ctrl->dma_dev->dma_chan[i], DEVDRV_DMA_CHAN_DISABLED);
    }

    if (pci_ctrl->ops.unbind_irq != NULL) {
        pci_ctrl->ops.unbind_irq(pci_ctrl);
    }
    devdrv_dma_exit(pci_ctrl->dma_dev, DEVDRV_SRIOV_ENABLE);
    if (pci_ctrl->ops.bind_irq != NULL) {
        pci_ctrl->ops.bind_irq(pci_ctrl);
    }

    ret = pci_enable_sriov(pci_ctrl->pdev, DEVDRV_MAX_SRIOV_INSTANCE);
    if (ret != 0) {
        (void)devdrv_sriov_event_disable_notify(pci_ctrl);
        devdrv_err("Enable sriov fail. (ret=%d, dev_id=%u)\n", ret, dev_id);
        return -EINVAL;
    }

    ret = devdrv_sriov_event_notify(pci_ctrl->msg_dev, dev_id, DEVDRV_SRIOV_ENABLE);
    if (ret != 0) {
        devdrv_err("Sriov enable notify fail (ret=%d, dev_id=%u)\n", ret, dev_id);
        devdrv_sriov_disable(dev_id);
        return -EINVAL;
    }
    return 0;
}
EXPORT_SYMBOL(devdrv_sriov_enable);

/* when disable sriov, unrecord vf dev num */
STATIC void devdrv_vf_dev_state_unrecord(u32 dev_id)
{
    u32 vf_dev_id = dev_id * MAX_VF_CNT_OF_PF + DEVDRV_SRIOV_VF_DEVID_START;
    u32 i;
    u32 j;
    u32 k;

    mutex_lock(&g_devdrv_ctrl_mutex);
    for (i = 0; i < DEVDRV_MAX_SRIOV_INSTANCE; i++) {
        g_state_ctrl.state_flag[vf_dev_id + i] = DEVDRV_STATE_UNPROBE;
    }
    for (j = 0; j < g_state_ctrl.dev_num; j++) {
        if (g_state_ctrl.devid[j] == vf_dev_id) {
            break;
        }
    }
    for (k = j + DEVDRV_MAX_SRIOV_INSTANCE; k < g_state_ctrl.dev_num; k++) {
        g_state_ctrl.devid[k - DEVDRV_MAX_SRIOV_INSTANCE] =
            g_state_ctrl.devid[k];
    }
    g_state_ctrl.dev_num = g_state_ctrl.dev_num - DEVDRV_MAX_SRIOV_INSTANCE;
    mutex_unlock(&g_devdrv_ctrl_mutex);
}

int devdrv_sriov_disable(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 vf_dev_id = 0;
    u32 func_id;
    int ret = 0;

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (pci_ctrl->device_status != DEVDRV_DEVICE_ALIVE) {
        devdrv_err("Device is abnormal, can not disable sriov. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (pci_vfs_assigned(pci_ctrl->pdev) != 0) {
        devdrv_err("Can not disable sriov when vfs are assigned\n");
        return -EINVAL;
    }

    devdrv_vf_dev_state_unrecord(dev_id);
    /* set pf dma queue pause */
    devdrv_set_pf_dma_queue_pause(pci_ctrl->dma_dev, true);
    pci_disable_sriov(pci_ctrl->pdev);
    /* exit pf dma queue pause */
    devdrv_set_pf_dma_queue_pause(pci_ctrl->dma_dev, false);

    ret = devdrv_sriov_event_disable_notify(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Sriov disable notify fail\n");
        return -EINVAL;
    }

    for (func_id = 1; func_id < MAX_VF_CNT_OF_PF; func_id++) {
        ret = devdrv_get_devid_by_pfvf_id(dev_id, func_id, &vf_dev_id);
        if (ret != 0) {
            continue;
        }
        (void)memset_s(&g_ctrls[vf_dev_id], sizeof(struct devdrv_ctrl), 0, sizeof(struct devdrv_ctrl));
    }

    pci_ctrl->is_sriov_enabled = DEVDRV_SRIOV_DISABLE;

    return 0;
}
EXPORT_SYMBOL(devdrv_sriov_disable);

int devdrv_get_pci_enabled_vf_num(u32 dev_id, int *vf_num)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    if (vf_num == NULL) {
        devdrv_err("input param is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if ((pci_ctrl == NULL) || (pci_ctrl->pdev == NULL)) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *vf_num = pci_num_vf(pci_ctrl->pdev);
    return 0;
}
EXPORT_SYMBOL(devdrv_get_pci_enabled_vf_num);

int devdrv_set_sriov_and_mdev_mode(u32 dev_id, u32 boot_mode)
{
    u32 pf_id, vf_id;
    int ret = 0;

    ret = devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id);
    if (ret != 0) {
        devdrv_err("Get_pfvf_id_by_devid fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    g_env_boot_mode[pf_id] = boot_mode;
    return 0;
}
EXPORT_SYMBOL(devdrv_set_sriov_and_mdev_mode);

int devdrv_get_sriov_and_mdev_mode(u32 dev_id, u32 *boot_mode)
{
    u32 pf_id, vf_id;
    int ret = 0;

    ret = devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id);
    if (ret != 0) {
        devdrv_err("Get_pfvf_id_by_devid fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *boot_mode = g_env_boot_mode[pf_id];
    return 0;
}

int devdrv_mdev_pm_init_msi_interrupt(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret;
#ifdef CFG_FEATURE_AGENT_SMMU
    struct devdrv_host_dma_addr_to_pa_cmd cmd_data = {0};
    struct devdrv_pci_ctrl *pci_ctrl_pf = NULL;
    int pf_dev_id;
    u32 data_len;
#endif

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_init_interrupt_normal(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Init interrupt failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

#ifdef CFG_FEATURE_AGENT_SMMU
    /* hccs peh's msi-x table need retranslation by agent-smmu agter request */
    if ((pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) && (devdrv_is_mdev_pm_boot_mode(dev_id) == true)) {
        /* vf has no smg chan in pm，so must use pf's admin chan */
        pf_dev_id = devdrv_sriov_get_pf_devid_by_vf_ctrl(pci_ctrl);
        pci_ctrl_pf = devdrv_get_bottom_half_pci_ctrl_by_id((u32)pf_dev_id);
        if ((pci_ctrl_pf == NULL) || (pci_ctrl_pf->msg_dev == NULL)) {
            devdrv_uninit_interrupt_normal(pci_ctrl);
            return -EINVAL;
        }

        data_len = sizeof(struct devdrv_host_dma_addr_to_pa_cmd);
        cmd_data.sub_cmd = DEVDRV_PEH_VF_MSI_TABLE_REFRESH;
        cmd_data.host_devid = pci_ctrl->dev_id;
        ret = devdrv_admin_msg_chan_send(pci_ctrl_pf->msg_dev, DEVDRV_HCCS_HOST_DMA_ADDR_MAP, &cmd_data, data_len,
            NULL, data_len);
        if (ret != 0) {
            devdrv_uninit_interrupt_normal(pci_ctrl);
            devdrv_err("Msi irq table reset fail(devid=%u, ret=%d\n", pci_ctrl->dev_id, ret);
            return ret;
        }
    }
#endif

    return 0;
}
EXPORT_SYMBOL(devdrv_mdev_pm_init_msi_interrupt);

int devdrv_mdev_pm_uninit_msi_interrupt(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    devdrv_uninit_interrupt_normal(pci_ctrl);

    return 0;
}
EXPORT_SYMBOL(devdrv_mdev_pm_uninit_msi_interrupt);

/* must free vf's dma sq/cq addr on pm before hypervisor dma pool uninit */
void devdrv_mdev_free_vf_dma_sqcq_on_pm(u32 devid)
{
    struct devdrv_dma_channel *dma_chan = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_dma_dev *dma_dev = NULL;
    int i;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return;
    }

    if (pci_ctrl->dma_dev == NULL) {
        devdrv_info("dma_dev has been free. (dev_id=%u)\n", devid);
        return;
    }

    if ((pci_ctrl->env_boot_mode != DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT) &&
        (pci_ctrl->env_boot_mode != DEVDRV_MDEV_VF_PM_BOOT)) {
        devdrv_info("Not mdev vf dma sqcq on pm, return now. (dev_id=%u)\n", devid);
        return;
    }

    dma_dev = pci_ctrl->dma_dev;
    for (i = 0; i < dma_dev->remote_chan_num; i++) {
        dma_chan = &dma_dev->dma_chan[i];
        devdrv_free_dma_sq_cq(dma_chan);
    }

    return;
}
EXPORT_SYMBOL(devdrv_mdev_free_vf_dma_sqcq_on_pm);

int devdrv_get_env_boot_type(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err_spinlock("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    return pci_ctrl->env_boot_mode;
}
EXPORT_SYMBOL(devdrv_get_env_boot_type);

bool devdrv_is_mdev_pm_boot_mode(u32 dev_id)
{
    int env_boot_type = devdrv_get_env_boot_type(dev_id);
    if ((env_boot_type == DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT) ||
        (env_boot_type == DEVDRV_MDEV_VF_PM_BOOT)) {
        return true;
    }
    return false;
}
EXPORT_SYMBOL(devdrv_is_mdev_pm_boot_mode);

bool devdrv_is_mdev_vm_boot_mode(u32 dev_id)
{
    int env_boot_type = devdrv_get_env_boot_type(dev_id);
    if ((env_boot_type == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT) ||
        (env_boot_type == DEVDRV_MDEV_VF_VM_BOOT)) {
        return true;
    }
    return false;
}
EXPORT_SYMBOL(devdrv_is_mdev_vm_boot_mode);

bool devdrv_is_mdev_vm_full_spec(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return false;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        return false;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if ((pci_ctrl->ops.is_mdev_vm_full_spec != NULL) &&
        (pci_ctrl->ops.is_mdev_vm_full_spec(pci_ctrl) == true)) {
        return true;
    }

    return false;
}
EXPORT_SYMBOL(devdrv_is_mdev_vm_full_spec);

int devdrv_mdev_set_pm_iova_addr_range(int devid, dma_addr_t iova_base, u64 iova_size)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id((u32)devid);
    if ((pci_ctrl == NULL) || (pci_ctrl->iova_range == NULL)) {
        devdrv_err("Get pci_ctrl failed, or iova is null. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    pci_ctrl->iova_range->start_addr = iova_base;
    pci_ctrl->iova_range->end_addr = iova_base + iova_size;
    if ((iova_base == 0) && (iova_size == 0)) {
        pci_ctrl->iova_range->init_flag = DEVDRV_DMA_IOVA_RANGE_UNINIT;
        devdrv_info("Uninit iova addr range success. (dev_id=%u)\n", devid);
    } else {
        pci_ctrl->iova_range->init_flag = DEVDRV_DMA_IOVA_RANGE_INIT;
        devdrv_info("Init iova addr range success. (dev_id=%u)\n", devid);
    }
    return 0;
}
EXPORT_SYMBOL(devdrv_mdev_set_pm_iova_addr_range);

int devdrv_get_connect_protocol(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err_spinlock("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return CONNECT_PROTOCOL_UNKNOWN;
    }

    return pci_ctrl->connect_protocol;
}
EXPORT_SYMBOL(devdrv_get_connect_protocol);

int devdrv_get_connect_protocol_by_dev(struct device *dev)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = (struct devdrv_pdev_ctrl *)dev_get_drvdata(dev);

    if (pdev_ctrl == NULL) {
        devdrv_err_spinlock("Input parameter is invalid.\n");
        return CONNECT_PROTOCOL_UNKNOWN;
    }

    if (pdev_ctrl->pci_ctrl[0]->pdev != to_pci_dev(dev)) {
        devdrv_err_spinlock("Not valid pci_dev.\n");
        return CONNECT_PROTOCOL_UNKNOWN;
    }

    return pdev_ctrl->pci_ctrl[0]->connect_protocol;
}

struct device *devdrv_get_pci_dev_by_devid(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    return &pci_ctrl->pdev->dev;
}
EXPORT_SYMBOL(devdrv_get_pci_dev_by_devid);

struct pci_dev *devdrv_get_pci_pdev_by_devid(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    return pci_ctrl->pdev;
}
EXPORT_SYMBOL(devdrv_get_pci_pdev_by_devid);

u32 devdrv_get_dev_chip_type(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err_spinlock("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return HISI_CHIP_UNKNOWN;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err_spinlock("Find msg_chan by id failed. (dev_id=%u)\n", dev_id);
        return HISI_CHIP_UNKNOWN;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    return pci_ctrl->chip_type;
}
EXPORT_SYMBOL(devdrv_get_dev_chip_type);

STATIC u32 devdrv_get_total_func_num(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return 0;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Find msg_chan by id failed. (dev_id=%u)\n", dev_id);
        return 0;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    return (u32)pci_ctrl->shr_para->total_func_num;
}

STATIC void devdrv_dev_startup_ctrl_init(void)
{
    int i;
    g_state_ctrl.dev_num = 0;
    g_state_ctrl.first_flag = 0;
    g_state_ctrl.startup_callback = NULL;
    g_state_ctrl.state_notifier_callback = NULL;
    g_state_ctrl.reset_devid = MAX_DEV_CNT;
    for (i = 0; i < MAX_DEV_CNT; i++) {
        g_state_ctrl.devid[i] = MAX_DEV_CNT;
        g_state_ctrl.state_flag[i] = DEVDRV_STATE_UNPROBE;
    }
}

STATIC int devdrv_clients_instance_init(void)
{
    u32 i, j;

    g_clients_instance = devdrv_kzalloc(sizeof(struct devdrv_client_instance) * (u32)DEVDRV_CLIENT_TYPE_MAX * MAX_DEV_CNT,
        GFP_KERNEL);
    if (g_clients_instance == NULL) {
        devdrv_err("Alloc g_clients_instance failed.\n");
        return -ENOMEM;
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        for (j = 0; j < DEVDRV_CLIENT_TYPE_MAX; j++) {
            mutex_init(&g_clients_instance[i][j].flag_mutex);
        }
    }

    return 0;
}

void devdrv_clients_instance_uninit(void)
{
    u32 i, j;

    if (g_clients_instance == NULL) {
        return;
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        for (j = 0; j < DEVDRV_CLIENT_TYPE_MAX; j++) {
            mutex_destroy(&g_clients_instance[i][j].flag_mutex);
        }
    }

    devdrv_kfree(g_clients_instance);
    g_clients_instance = NULL;
}

int devdrv_ctrl_init(void)
{
    u32 i;
    int ret;
    struct rw_semaphore *common_msg_rw_sem = devdrv_get_common_msg_rw_sem();

    if ((memset_s(g_ctrls, sizeof(g_ctrls), 0, sizeof(g_ctrls)) != 0) ||
        (memset_s(g_clients, sizeof(g_clients), 0, sizeof(g_clients)) != 0)) {
        devdrv_err("memset_s failed.\n");
        return -EINVAL;
    }

    ret = devdrv_clients_instance_init();
    if (ret != 0) {
        devdrv_err("Clients instance init failed.\n");
        return -EINVAL;
    }

    for (i = 0; i < DEVDRV_CLIENT_TYPE_MAX; i++) {
        mutex_init(&g_clients_mutex[i]);
    }
    for (i = 0; i < DEVDRV_COMMON_MSG_TYPE_MAX; i++) {
        init_rwsem(&common_msg_rw_sem[i]);
    }
    mutex_init(&g_devdrv_ctrl_mutex);
    mutex_init(&g_devdrv_p2p_mutex);
    mutex_init(&g_devdrv_remove_rescan_mutex);
    devdrv_dev_startup_ctrl_init();
    devdrv_peer_ctrl_init();

    return 0;
}

void devdrv_ctrl_uninit(void)
{
    devdrv_clients_instance_uninit();
    return;
}

void devdrv_set_ctrl_priv(u32 dev_id, void *priv)
{
    g_ctrls[dev_id].priv = priv;
}

void devdrv_set_devctrl_startup_flag(u32 dev_id, enum devdrv_dev_startup_flag_type flag)
{
    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return;
    }
    g_ctrls[dev_id].startup_flg = flag;
}

int devdrv_alloc_devid_check_ctrls(const struct devdrv_ctrl *ctrl_this)
{
    int i;
    int dev_id = -1;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        /* bus may be already inited probe at first time, hot reset happen. */
        if ((g_ctrls[i].bus != NULL) && (g_ctrls[i].bus == ctrl_this->bus) &&
            (g_ctrls[i].slot_id == ctrl_this->slot_id) &&
            (g_ctrls[i].func_id == ctrl_this->func_id) &&
            (g_ctrls[i].startup_flg == DEVDRV_DEV_STARTUP_UNPROBED)) {
            g_ctrls[i].startup_flg = DEVDRV_DEV_STARTUP_PROBED;
            dev_id = i;
            break;
        }
    }

    return dev_id;
}

int devdrv_sriov_get_pf_devid_by_vf_ctrl(struct devdrv_pci_ctrl *pci_ctrl)
{
    int pf_dev_id = -1;
    int i;

    for (i = 0; i < MAX_PF_DEV_CNT; i++) {
        if ((g_ctrls[i].bus == NULL) || (pci_ctrl == NULL) || (pci_ctrl->pdev == NULL) ||
            (pci_ctrl->pdev->bus == NULL)) {
            continue;
        }

        if ((g_ctrls[i].bus == pci_ctrl->pdev->bus) &&
            (g_ctrls[i].slot_id == 0) && (g_ctrls[i].func_id == 0) &&
            (g_ctrls[i].startup_flg != DEVDRV_DEV_STARTUP_UNPROBED)) {
            pf_dev_id = (int)g_ctrls[i].dev_id;
            break;
        }
    }

    return pf_dev_id;
}

int devdrv_alloc_devid_inturn(u32 beg, u32 stride)
{
    int dev_id = -1;
    u32 i;

    for (i = beg; i < MAX_DEV_CNT; i = i + stride) {
        if (g_ctrls[i].startup_flg == DEVDRV_DEV_STARTUP_UNPROBED) {
            g_ctrls[i].startup_flg = DEVDRV_DEV_STARTUP_PROBED;
            dev_id = (int)i;
            break;
        }
    }
    return dev_id;
}

STATIC int devdrv_get_devid_by_funcid(u32 state, int func_id_check, int func_id_this, int i, int *dev_id)
{
    if ((func_id_check == 0) || (func_id_check == 1)) {
        devdrv_info("Function match. (this_func=%d; func_id_check=%d)\n", func_id_this, func_id_check);
        if (state != DEVDRV_DEV_STARTUP_UNPROBED) {
            /* bus same ,has startup, function check */
            if (func_id_check == func_id_this) {
                /* bus same, has startup, func match */
                devdrv_err("All already is wrong.\n");
                return -EINVAL;
            } else {
                /* new func */
                *dev_id = (i / DEVDRV_MAX_FUNC_NUM) * DEVDRV_MAX_FUNC_NUM + func_id_this;
                devdrv_info("New func. (dev_id=%d; new_dev_id=%d)\n", i, *dev_id);
                return 1;
            }
        } else {
            /* host reset */
            if (func_id_check == func_id_this) {
                *dev_id = i;
                devdrv_info("Bus match but state change, then use new device. (dev_id=%u)\n", *dev_id);
                return 1;
            } else {
                /* new func, alloc dev id, but go on to find match id. */
                *dev_id = (i / DEVDRV_MAX_FUNC_NUM) * DEVDRV_MAX_FUNC_NUM + func_id_this;
                devdrv_info("New func. (dev_id=%d; new_dev_id=%d)\n", i, *dev_id);
            }
        }
    } else {
        devdrv_err("Wrong func. (func_id_check=%d)\n", func_id_check);
        return -EINVAL;
    }
    return 0;
}

int devdrv_alloc_devid_stride_2(struct devdrv_ctrl *ctrl_this)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)ctrl_this->priv;
    int dev_id_check = -1;
    int dev_id = -1;
    int func_id_this = (int)pci_ctrl->func_id;
    int func_id_check = -1;
    int i;
    u32 state = 0;
    int ret;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if ((g_ctrls[i].bus != NULL) && (g_ctrls[i].bus == ctrl_this->bus) &&
            (g_ctrls[i].slot_id == ctrl_this->slot_id)) {
            /* bus and slot are same, means another function finish or host reset or 1PF2P. */
            if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
                dev_id_check = i + 1; /* For 1PF2P */
                break;
            }
            func_id_check = (int)g_ctrls[i].func_id;
            state = (u32)g_ctrls[i].startup_flg;
            devdrv_info("Bus and slot match in this devdrv_ctrl. (dev_id=%u; func_id_check=%d; state=%d)\n",
                        i, func_id_check, state);
            ret = devdrv_get_devid_by_funcid(state, func_id_check, func_id_this, i, &dev_id_check);
            if (ret < 0) {
                devdrv_err("Get devid failed. (func_check=%d; func_this=%d)\n", func_id_check, func_id_this);
                return ret;
            } else if (ret > 0) {
                break;
            }
        }
    }

    if ((dev_id_check == -1) || (dev_id_check >= MAX_DEV_CNT)) {
        devdrv_debug("No match dev_id.(dev_id_check=%d)\n", dev_id_check);
        /* no bus match, then alloc in turn by func */
        dev_id = devdrv_alloc_devid_inturn((u32)func_id_this, 2);
    } else {
        dev_id = dev_id_check;
        g_ctrls[dev_id].startup_flg = DEVDRV_DEV_STARTUP_PROBED;
        devdrv_info("Match device id success. (dev_id=%u)\n", dev_id);
    }
    return dev_id;
}

STATIC int devdrv_alloc_devid(struct devdrv_ctrl *ctrl_this)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int dev_id = -1;

    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl_this->priv;

    if (pci_ctrl->ops.alloc_devid != NULL) {
        mutex_lock(&g_devdrv_ctrl_mutex);
        dev_id = pci_ctrl->ops.alloc_devid(ctrl_this);
        mutex_unlock(&g_devdrv_ctrl_mutex);
    }

    return dev_id;
}

/* called after probed */
struct devdrv_ctrl *devdrv_get_top_half_devctrl_by_id(u32 dev_id)
{
    u32 i;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (((g_ctrls[i].startup_flg == DEVDRV_DEV_STARTUP_TOP_HALF_OK) ||
             (g_ctrls[i].startup_flg == DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK)) &&
            (g_ctrls[i].dev_id == dev_id)) {
            return &g_ctrls[i];
        }
    }

    return NULL;
}

/* called after half probed */
struct devdrv_ctrl *devdrv_get_bottom_half_devctrl_by_id(u32 dev_id)
{
    u32 i;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if ((g_ctrls[i].startup_flg == DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK) && (g_ctrls[i].dev_id == dev_id)) {
            return &g_ctrls[i];
        }
    }

    return NULL;
}

struct devdrv_ctrl *devdrv_get_devctrl_by_id(u32 i)
{
    if (i < MAX_DEV_CNT) {
        return &g_ctrls[i];
    }

    return NULL;
}

struct devdrv_pci_ctrl *devdrv_get_bottom_half_pci_ctrl_by_id(u32 dev_id)
{
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input dev_id is invalid. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn("Can not get pci_ctrl. (dev_id=%u)\n", dev_id);
        } else {
            devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        }

        return NULL;
    }

    return (struct devdrv_pci_ctrl *)ctrl->priv;
}

struct devdrv_pci_ctrl *devdrv_get_top_half_pci_ctrl_by_id(u32 dev_id)
{
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err_spinlock("Input dev_id is invalid. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err_spinlock("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    return (struct devdrv_pci_ctrl *)ctrl->priv;
}

/* device boot status */
int devdrv_get_device_boot_status(u32 devid, u32 *boot_status)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (boot_status == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_warn("Get devctrl by id failed. (dev_id=%u)\n", devid);
        *boot_status = DSMI_BOOT_STATUS_UNINIT;
        return -ENXIO;
    }

    pci_ctrl = ctrl->priv;

    *boot_status = pci_ctrl->device_boot_status;
    return 0;
}
EXPORT_SYMBOL(devdrv_get_device_boot_status);

/* record probed mini */
void drvdrv_dev_startup_record(u32 dev_id)
{
    u32 find_flag = 0;
    u32 i;

    mutex_lock(&g_devdrv_ctrl_mutex);
    /* not the first time probe(hot reset) */
    for (i = 0; i < g_state_ctrl.dev_num; i++) {
        if (dev_id == g_state_ctrl.devid[i]) {
            find_flag = 1;
            break;
        }
    }
    /* the first time probe */
    if (find_flag == 0) {
        g_state_ctrl.devid[g_state_ctrl.dev_num] = dev_id;
        if (g_state_ctrl.dev_num < MAX_DEV_CNT) {
            g_state_ctrl.state_flag[dev_id] = DEVDRV_STATE_PROBE;
            g_state_ctrl.dev_num++;
        }
        devdrv_info("Probe new device, add to report. (dev_id=%u; dev_num=%u)\n", dev_id, g_state_ctrl.dev_num);
    } else {
        /* not first time to enable sriov */
        if (g_state_ctrl.state_flag[dev_id] == DEVDRV_STATE_UNPROBE) {
            g_state_ctrl.state_flag[dev_id] = DEVDRV_STATE_PROBE;
            g_state_ctrl.dev_num++;
        }
        devdrv_info("Device needn't record, just report. (dev_id=%u; dev_num=%u)\n", dev_id, g_state_ctrl.dev_num);
    }

    mutex_unlock(&g_devdrv_ctrl_mutex);
}

/*
report fomat(eg.8mini):
when first register,probe 5mini:
5,[0,1,2,3,4],5
then:
6,[5],1
7,[6],1
8,[7],1
after hot reset devid=2:
8,[2],1
after hot reset devid=4:
8,[4],1
*/
void drvdrv_dev_startup_report(u32 dev_id)
{
    mutex_lock(&g_devdrv_ctrl_mutex);
    if (g_state_ctrl.startup_callback != NULL) {
        /* report all probed mini when first time reset */
        if (g_state_ctrl.first_flag == 0) {
            if (g_state_ctrl.dev_num != 0) {
                (g_state_ctrl.startup_callback)(g_state_ctrl.dev_num, g_state_ctrl.devid, MAX_DEV_CNT,
                                                g_state_ctrl.dev_num);
            }
            devdrv_info("Device startup report. (dev_num=%u)\n", g_state_ctrl.dev_num);
            g_state_ctrl.first_flag = 1;
        } else {
            /* report one probed mini when hotreset or vf probe */
            g_state_ctrl.reset_devid = dev_id;
            (g_state_ctrl.startup_callback)(g_state_ctrl.dev_num, &g_state_ctrl.reset_devid, MAX_DEV_CNT, 1);
            devdrv_info("Device startup report. (dev_id=%u)\n", dev_id);
        }
    } else {
        devdrv_info("Device startup without report. (dev_id=%u)\n", dev_id);
    }
    mutex_unlock(&g_devdrv_ctrl_mutex);

    return;
}

void drvdrv_dev_state_notifier(struct devdrv_pci_ctrl *pci_ctrl)
{
    u32 dev_id;

    if (pci_ctrl == NULL) {
        devdrv_info("Input parameter is null.\n");
        return;
    }

    /* when pcie ko removed, stop notify dev manager, and set callback NULL */
    if (pci_ctrl->module_exit_flag == DEVDRV_REMOVE_CALLED_BY_MODULE_EXIT) {
        devdrv_info("Do not notify dev manager driver remove.\n");
        mutex_lock(&g_devdrv_ctrl_mutex);
        g_state_ctrl.state_notifier_callback = NULL;
        mutex_unlock(&g_devdrv_ctrl_mutex);
        return;
    }

    dev_id = pci_ctrl->dev_id;
    if (dev_id >= MAX_DEV_CNT) {
        devdrv_info("dev_id is invalid. (dev_id=%u)\n", dev_id);
        return;
    }

    mutex_lock(&g_devdrv_ctrl_mutex);
    if (g_state_ctrl.state_notifier_callback != NULL) {
        (g_state_ctrl.state_notifier_callback)(g_state_ctrl.dev_num, dev_id, (u32)GOING_TO_DISABLE_DEV);
    } else {
        devdrv_info("state_notifier_callback is NULL. (dev_id=%u)\n", dev_id);
    }
    mutex_unlock(&g_devdrv_ctrl_mutex);
}

/* for dev manager to register when insmod,then pcie report dev startup info to dev manager */
void drvdrv_dev_startup_register(devdrv_dev_startup_notify startup_callback)
{
    if (startup_callback == NULL) {
        devdrv_warn("Startup callback is NULL.\n");
        return;
    }
    devdrv_info("Startup register, call startup report.\n");
    mutex_lock(&g_devdrv_ctrl_mutex);
    g_state_ctrl.startup_callback = startup_callback;
    mutex_unlock(&g_devdrv_ctrl_mutex);
    drvdrv_dev_startup_report(MAX_DEV_CNT);
}
EXPORT_SYMBOL(drvdrv_dev_startup_register);

void drvdrv_dev_startup_unregister(void)
{
    mutex_lock(&g_devdrv_ctrl_mutex);
    g_state_ctrl.startup_callback = NULL;
    mutex_unlock(&g_devdrv_ctrl_mutex);
}
EXPORT_SYMBOL(drvdrv_dev_startup_unregister);

/* for dev manager to register when insmod,then pcie report dev remove or other info to dev manager */
void drvdrv_dev_state_notifier_register(devdrv_dev_state_notify state_callback)
{
    if (state_callback == NULL) {
        devdrv_warn("State callback is null.\n");
        return;
    }
    devdrv_info("State notifier register.\n");
    mutex_lock(&g_devdrv_ctrl_mutex);
    g_state_ctrl.state_notifier_callback = state_callback;
    mutex_unlock(&g_devdrv_ctrl_mutex);
}
EXPORT_SYMBOL(drvdrv_dev_state_notifier_register);

void devdrv_dev_state_notifier_unregister(void)
{
    mutex_lock(&g_devdrv_ctrl_mutex);
    g_state_ctrl.state_notifier_callback = NULL;
    mutex_unlock(&g_devdrv_ctrl_mutex);
}
EXPORT_SYMBOL(devdrv_dev_state_notifier_unregister);

void devdrv_pci_suspend_check_register(devdrv_suspend_pre_check suspend_check)
{
    if (suspend_check == NULL) {
        devdrv_warn("suspend pre check is null.\n");
        return;
    }
    devdrv_info("suspend pre check register.\n");
    g_pci_state_ctrl.suspend_pre_check = suspend_check;
}
EXPORT_SYMBOL(devdrv_pci_suspend_check_register);

void devdrv_pci_suspend_check_unregister(void)
{
    g_pci_state_ctrl.suspend_pre_check = NULL;
}
EXPORT_SYMBOL(devdrv_pci_suspend_check_unregister);

void devdrv_peer_fault_notifier_register(devdrv_peer_fault_notify fault_notifier)
{
    if (fault_notifier == NULL) {
        devdrv_warn("peer fault notifier is null.\n");
        return;
    }
    devdrv_info("peer fault notifier register.\n");
    g_pci_state_ctrl.peer_fault_notify = fault_notifier;
}
EXPORT_SYMBOL(devdrv_peer_fault_notifier_register);

void devdrv_peer_fault_notifier_unregister(void)
{
    g_pci_state_ctrl.peer_fault_notify = NULL;
}
EXPORT_SYMBOL(devdrv_peer_fault_notifier_unregister);

#if defined(CFG_SOC_PLATFORM_MDC_V11) || defined(CFG_SOC_PLATFORM_MDC_V51_LITE)
int devdrv_hdc_suspend_precheck(int count)
{
    int ret = 0;

    if (g_pci_state_ctrl.suspend_pre_check == NULL) {
        if ((count % DEVDRV_SUSPEND_ONE_SECOND) == 0) {
            devdrv_warn("hdc does not register suspend pre-check.\n");
        }
    } else {
        if ((count % DEVDRV_SUSPEND_ONE_SECOND) == 0) {
            ret = g_pci_state_ctrl.suspend_pre_check(1); /* hdc log flow ctrl */
        } else {
            ret = g_pci_state_ctrl.suspend_pre_check(0);
        }
    }
    return ret;
}
#else
int devdrv_hdc_suspend_precheck(int count)
{
    return 0;
}
#endif

void devdrv_peer_fault_notifier(u32 status)
{
    if (g_pci_state_ctrl.peer_fault_notify == NULL) {
        devdrv_warn("hdc does not register peer fault notifier.\n");
    } else {
        g_pci_state_ctrl.peer_fault_notify(status);
    }
}

void devdrv_hb_broken_stop_msg_send(u32 devid)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_info("Input parameter is invalid. (dev_id=%u)\n", devid);
        return;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_info("Invaild dev_ctrl. (dev_id=%u)\n", devid);
        return;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    devdrv_set_device_status(pci_ctrl, DEVDRV_DEVICE_DEAD);

    devdrv_info("Heartbeat broken, stop message send.\n");
}
EXPORT_SYMBOL(devdrv_hb_broken_stop_msg_send);

void devdrv_set_device_boot_status(struct devdrv_pci_ctrl *pci_ctrl, u32 status)
{
    pci_ctrl->device_boot_status = status;
}

static void devdrv_pci_stop_and_remove_bus_device_locked(struct pci_dev *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    pci_stop_and_remove_bus_device_locked(dev);
#else
    mutex_lock(&g_devdrv_remove_rescan_mutex);
    pci_stop_and_remove_bus_device(dev);
    mutex_unlock(&g_devdrv_remove_rescan_mutex);
#endif
}

static u32 devdrv_pci_rescan_bus_locked(struct pci_bus *bus)
{
    u32 ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    pci_lock_rescan_remove();
    ret = pci_rescan_bus(bus);
    pci_unlock_rescan_remove();
#else
    mutex_lock(&g_devdrv_remove_rescan_mutex);
    ret = pci_rescan_bus(bus);
    mutex_unlock(&g_devdrv_remove_rescan_mutex);
#endif
    return ret;
}

int devdrv_pcie_reinit(u32 dev_id)
{
    struct pci_bus *bus = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    bus = g_ctrls[dev_id].bus;

    if (bus == NULL) {
        devdrv_err("Bus does not exist. (dev_id=%u)\n", dev_id);
        return -EIO;
    }
    devdrv_info("Call devdrv_pcie_reinit. (dev_id=%u)\n", dev_id);

    /* In some virtual mach, bus->self is NULL */
    if (bus->self != NULL) {
        bus = bus->self->bus;
    }

    (void)devdrv_pci_rescan_bus_locked(bus);

    return 0;
}
EXPORT_SYMBOL(devdrv_pcie_reinit);

struct devdrv_dma_dev *devdrv_get_dma_dev(u32 dev_id)
{
    struct devdrv_ctrl *ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err_spinlock("Get devdrv_ctrl failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    pci_ctrl = ctrl->priv;

    return pci_ctrl->dma_dev;
}

u32 devdrv_get_devid_by_dev(const struct devdrv_msg_dev *msg_dev)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 i;

    if (msg_dev == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return MAX_DEV_CNT;
    }
    for (i = 0; i < MAX_DEV_CNT; i++) {
        pci_ctrl = (struct devdrv_pci_ctrl *)g_ctrls[i].priv;
        if (pci_ctrl == NULL) {
            continue;
        }
        if (pci_ctrl->msg_dev == msg_dev) {
            return g_ctrls[i].dev_id;
        }
    }

    devdrv_err("Find dev_id failed.\n");

    return MAX_DEV_CNT;
}

/* Store the ctrl info, and return an id to register caller */
STATIC int devdrv_slave_dev_add(struct devdrv_ctrl *ctrl)
{
    int dev_id;

    dev_id = devdrv_alloc_devid(ctrl);
    if ((dev_id < 0) || (dev_id >= MAX_DEV_CNT)) {
        devdrv_err("Device link already full. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    g_ctrls[(u32)dev_id].dev_id = (u32)dev_id;
    g_ctrls[(u32)dev_id].priv = ctrl->priv;
    g_ctrls[(u32)dev_id].dev_type = ctrl->dev_type;
    g_ctrls[(u32)dev_id].dev = ctrl->dev;
    g_ctrls[(u32)dev_id].pdev = ctrl->pdev;
    g_ctrls[(u32)dev_id].bus = ctrl->bus;
    g_ctrls[(u32)dev_id].slot_id = ctrl->slot_id;
    g_ctrls[(u32)dev_id].func_id = ctrl->func_id;
    g_ctrls[(u32)dev_id].virtfn_flag = ctrl->virtfn_flag;

    return dev_id;
}

void devdrv_slave_dev_delete(u32 dev_id)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    u32 i;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return;
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (g_ctrls[i].dev_id == dev_id) {
            dev_ctrl = &g_ctrls[i];
            break;
        }
    }

    if (i == MAX_DEV_CNT) {
        devdrv_err("Find devctrl failed. (dev_id=%u)\n", dev_id);
        return;
    }

    if (dev_ctrl != NULL) {
        dev_ctrl->startup_flg = DEVDRV_DEV_STARTUP_UNPROBED;
        dev_ctrl->priv = NULL;
        dev_ctrl->ops = NULL;
        dev_ctrl->dev = NULL;
    }
}

void *devdrv_pcimsg_alloc_trans_queue(u32 dev_id, struct devdrv_trans_msg_chan_info *chan_info)
{
    struct devdrv_msg_chan *chan = NULL;
    struct devdrv_ctrl *ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if (ctrl == NULL) {
        devdrv_err("Can't find devdrv_ctrl. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    if (chan_info == NULL) {
        devdrv_err("chan_info is null. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    chan = ctrl->ops->alloc_trans_chan(ctrl->priv, chan_info);
    if (chan == NULL) {
        return NULL;
    }

    return devdrv_generate_msg_handle(chan);
}
EXPORT_SYMBOL(devdrv_pcimsg_alloc_trans_queue);

int devdrv_pcimsg_realease_trans_queue(void *msg_chan)
{
    struct devdrv_msg_chan *chan = NULL;
    if (msg_chan == NULL) {
        devdrv_err("msg_chan is invalid.\n");
        return -EINVAL;
    }
    chan = devdrv_find_msg_chan(msg_chan);
    if (chan == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("msg_chan is null.\n");
        } else {
            devdrv_err_limit("msg_chan is null.\n");
        }
        return -EINVAL;
    }

    return devdrv_free_trans_queue(chan);
}
EXPORT_SYMBOL(devdrv_pcimsg_realease_trans_queue);

/* non-trans msg chan */
void *devdrv_pcimsg_alloc_non_trans_queue_inner(u32 dev_id, struct devdrv_non_trans_msg_chan_info *chan_info)
{
    struct devdrv_ctrl *ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if (ctrl == NULL) {
        devdrv_err("Can't find devdrv_ctrl. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    if (chan_info == NULL) {
        devdrv_err("chan_info is null. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    return (void *)(ctrl->ops->alloc_non_trans_chan(ctrl->priv, chan_info));
}

void *devdrv_pcimsg_alloc_non_trans_queue(u32 dev_id, struct devdrv_non_trans_msg_chan_info *chan_info)
{
    struct devdrv_msg_chan *chan = devdrv_pcimsg_alloc_non_trans_queue_inner(dev_id, chan_info);
    if (chan == NULL) {
        return NULL;
    }

    return devdrv_generate_msg_handle(chan);
}
EXPORT_SYMBOL(devdrv_pcimsg_alloc_non_trans_queue);

int devdrv_pcimsg_free_non_trans_queue_inner(void *msg_chan)
{
    if (msg_chan == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }

    return devdrv_free_non_trans_queue((struct devdrv_msg_chan *)msg_chan);
}

int devdrv_pcimsg_free_non_trans_queue(void *msg_chan)
{
    struct devdrv_msg_chan *chan = NULL;
    if (msg_chan == NULL) {
        devdrv_err("msg chan is invalid.\n");
        return -EINVAL;
    }
    chan = devdrv_find_msg_chan(msg_chan);
    if (chan == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("msg_chan is null.\n");
        } else {
            devdrv_err_limit("msg_chan is null.\n");
        }
        return -EINVAL;
    }

    return devdrv_pcimsg_free_non_trans_queue_inner(chan);
}
EXPORT_SYMBOL(devdrv_pcimsg_free_non_trans_queue);

int devdrv_register_black_callback(struct devdrv_black_callback *black_callback)
{
    if (black_callback == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }

    if (black_callback->callback == NULL) {
        devdrv_err("callback is null.\n");
        return -EINVAL;
    }

    g_black_box.callback = black_callback->callback;
    devdrv_info("bbox callback is register.\n");

    return 0;
}
EXPORT_SYMBOL(devdrv_register_black_callback);

void devdrv_unregister_black_callback(const struct devdrv_black_callback *black_callback)
{
    if (black_callback == NULL) {
        devdrv_err("black_callback is null.\n");
        return;
    }

    if (black_callback->callback != g_black_box.callback) {
        devdrv_err("callback is not same.\n");
        return;
    }

    g_black_box.callback = NULL;

    return;
}
EXPORT_SYMBOL(devdrv_unregister_black_callback);

int devdrv_register_client(struct devdrv_client *client)
{
    int ret;
    u32 dev_id;
    struct devdrv_client_instance *instance = NULL;

    if (client == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }

    if (client->type >= DEVDRV_CLIENT_TYPE_MAX) {
        devdrv_err("client_type is error. (client_type=%d)\n", (int)client->type);
        return -EINVAL;
    }

    if (g_clients[client->type] != NULL) {
        devdrv_err("client_type is already registered. (client_type=%d)\n", (int)client->type);
        return -EINVAL;
    }

    mutex_lock(&g_clients_mutex[client->type]);
    for (dev_id = 0; dev_id < MAX_DEV_CNT; dev_id++) {
        if (g_ctrls[dev_id].startup_flg != DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK) {
            continue;
        }
        instance = &g_clients_instance[dev_id][client->type];
        if (instance == NULL) {
            continue;
        }
        instance->dev_ctrl = &g_ctrls[dev_id];
        if (client->init_instance == NULL) {
            continue;
        }
        devdrv_info("devdrv_register_client, before init_instance. (type=%d; dev_id=%u)\n", (int)client->type, dev_id);
        mutex_lock(&instance->flag_mutex);
        if (instance->flag == 0) {
            instance->flag = 1;
            mutex_unlock(&instance->flag_mutex);
            ret = client->init_instance(instance);
            devdrv_info("devdrv_register_client, after init_instance. (dev_id=%u)\n", dev_id);
            if (ret != 0) {
                mutex_lock(&instance->flag_mutex);
                instance->flag = 0;
                mutex_unlock(&instance->flag_mutex);
                mutex_unlock(&g_clients_mutex[client->type]);
                devdrv_err("Init instance ctrl failed,delete client. (dev_id=%u)\n", dev_id);
                return ret;
            }
        } else {
            mutex_unlock(&instance->flag_mutex);
        }
    }

    g_clients[client->type] = client;
    mutex_unlock(&g_clients_mutex[client->type]);

    return 0;
}
EXPORT_SYMBOL(devdrv_register_client);

int devdrv_unregister_client(const struct devdrv_client *client)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    struct devdrv_client_instance *instance = NULL;
    int dev_id;

    if (client == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }

    if (client->type >= DEVDRV_CLIENT_TYPE_MAX) {
        devdrv_err("client_type is error. (client_type=%d)\n", (int)client->type);
        return -EINVAL;
    }

    if (g_clients_instance == NULL) {
        devdrv_err("Clients_instance is null.\n");
        return -EINVAL;
    }
    mutex_lock(&g_clients_mutex[client->type]);
    for (dev_id = 0; dev_id < MAX_DEV_CNT; dev_id++) {
        instance = &g_clients_instance[dev_id][client->type];
        if (instance == NULL) {
            continue;
        }
        dev_ctrl = instance->dev_ctrl;
        if (dev_ctrl != NULL) {
            if (client->uninit_instance == NULL) {
                continue;
            }
            mutex_lock(&instance->flag_mutex);
            instance->flag = 0;
            mutex_unlock(&instance->flag_mutex);
            devdrv_info("Uninit instance start. (dev_id=%d; client_type=%u)\n", dev_id, (u32)client->type);
            client->uninit_instance(instance);
            devdrv_info("Uninit instance end. (dev_id=%d; client_type=%u)\n", dev_id, (u32)client->type);
            instance->dev_ctrl = NULL;
            instance->priv = NULL;
        }
    }
    g_clients[client->type] = NULL;
    mutex_unlock(&g_clients_mutex[client->type]);

    return 0;
}
EXPORT_SYMBOL(devdrv_unregister_client);

STATIC int devdrv_get_addr_info_para_check(u32 index, const u64 *addr, const size_t *size)
{
    if ((index != 0) || (addr == NULL) || (size == NULL)) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }
    return 0;
}

STATIC int devdrv_get_addr_info_by_type(struct devdrv_pci_ctrl *pci_ctrl, enum devdrv_addr_type type, u64 *addr,
                                        size_t *size)
{
    int ret = 0;
    switch (type) {
        case DEVDRV_ADDR_TS_DOORBELL:
            *addr = pci_ctrl->res.ts_db.addr;
            *size = pci_ctrl->res.ts_db.size;
            break;
        case DEVDRV_ADDR_TS_SRAM:
            *addr = pci_ctrl->res.ts_sram.addr;
            *size = pci_ctrl->res.ts_sram.size;
            break;
        case DEVDRV_ADDR_TS_SQ_BASE:
            *addr = pci_ctrl->res.ts_sq.addr;
            *size = pci_ctrl->res.ts_sq.size;
            break;
        case DEVDRV_ADDR_TEST_BASE:
            *addr = pci_ctrl->res.test.addr;
            *size = pci_ctrl->res.test.size;
            break;
        case DEVDRV_ADDR_LOAD_RAM:
            *addr = pci_ctrl->res.load_sram.addr;
            *size = pci_ctrl->res.load_sram.size;
            break;
        case DEVDRV_ADDR_HWTS:
            *addr = pci_ctrl->res.hwts.addr;
            *size = pci_ctrl->res.hwts.size;
            break;
        case DEVDRV_ADDR_IMU_LOG_BASE:
            *addr = pci_ctrl->res.imu_log.addr;
            *size = pci_ctrl->res.imu_log.size;
            break;
        case DEVDRV_ADDR_HDR_BASE:
            *addr = pci_ctrl->res.hdr.addr;
            *size = pci_ctrl->res.hdr.size;
            break;
        case DEVDRV_ADDR_BBOX_BASE:
            *addr = pci_ctrl->res.bbox.addr;
            *size = pci_ctrl->res.bbox.size;
            break;
        case DEVDRV_ADDR_REG_SRAM_BASE:
            *addr = pci_ctrl->res.reg_sram.addr;
            *size = pci_ctrl->res.reg_sram.size;
            break;
        case DEVDRV_ADDR_TSDRV_RESV_BASE:
            *addr = pci_ctrl->res.tsdrv_resv.addr;
            *size = pci_ctrl->res.tsdrv_resv.size;
            break;
        case DEVDRV_ADDR_DEVMNG_RESV_BASE:
            *addr = pci_ctrl->res.devmng_resv.addr;
            *size = pci_ctrl->res.devmng_resv.size;
            break;
        case DEVDRV_ADDR_DEVMNG_INFO_MEM_BASE:
            *addr = pci_ctrl->res.devmng_info_mem.addr;
            *size = pci_ctrl->res.devmng_info_mem.size;
            break;
        case DEVDRV_ADDR_HBM_ECC_MEM_BASE:
            *addr = pci_ctrl->res.hbm_ecc_mem.addr;
            *size = pci_ctrl->res.hbm_ecc_mem.size;
            break;
        case DEVDRV_ADDR_VF_BANDWIDTH_BASE:
            *addr = pci_ctrl->res.vf_bandwidth.addr;
            *size = pci_ctrl->res.vf_bandwidth.size;
            break;
        case DEVDRV_ADDR_HBOOT_SRAM_MEM:
            if ((pci_ctrl->device_boot_status == DSMI_BOOT_STATUS_OS) ||
                (pci_ctrl->device_boot_status == DSMI_BOOT_STATUS_FINISH)) {
                *addr = 0;
                *size = 0;
                break;
            }
            *addr = pci_ctrl->res.l3d_sram.addr;
            *size = pci_ctrl->res.l3d_sram.size;
            break;
        case DEVDRV_ADDR_KDUMP_HBM_MEM:
            *addr = pci_ctrl->res.kdump.addr;
            *size = pci_ctrl->res.kdump.size;
            break;
        case DEVDRV_ADDR_VMCORE_STAT_HBM_MEM:
            *addr = pci_ctrl->res.vmcore.addr;
            *size = pci_ctrl->res.vmcore.size;
            break;
        case DEVDRV_ADDR_BBOX_DDR_DUMP_MEM:
            *addr = pci_ctrl->res.bbox_ddr_dump.addr;
            *size = pci_ctrl->res.bbox_ddr_dump.size;
            break;
        case DEVDRV_ADDR_TS_LOG_MEM:
            *addr = pci_ctrl->res.ts_log.addr;
            *size = pci_ctrl->res.ts_log.size;
            break;
        case DEVDRV_ADDR_CHIP_DFX_FULL_MEM:
            *addr = pci_ctrl->res.chip_dfx.addr;
            *size = pci_ctrl->res.chip_dfx.size;
            break;
        default:
            devdrv_err("Parameter type is error. (dev_id=%d; type=%d)\n", pci_ctrl->dev_id, (int)type);
            ret = -EINVAL;
            break;
    }

    return ret;
}

STATIC int devdrv_get_stars_addr_info(struct devdrv_pci_ctrl *pci_ctrl, enum devdrv_addr_type type, u64 *addr,
                                      size_t *size)
{
    int ret = 0;
    switch (type) {
        case DEVDRV_ADDR_STARS_SQCQ_BASE:
            *addr = pci_ctrl->res.stars_sqcq.addr;
            *size = pci_ctrl->res.stars_sqcq.size;
            break;
        case DEVDRV_ADDR_STARS_TOPIC_SCHED_BASE:
            *addr = pci_ctrl->res.stars_topic_sched.addr;
            *size = pci_ctrl->res.stars_topic_sched.size;
            break;
        case DEVDRV_ADDR_STARS_CDQM_BASE:
            *addr = pci_ctrl->res.stars_cdqm.addr;
            *size = pci_ctrl->res.stars_cdqm.size;
            break;
        case DEVDRV_ADDR_STARS_SQCQ_INTR_BASE:
            *addr = pci_ctrl->res.stars_sqcq_intr.addr;
            *size = pci_ctrl->res.stars_sqcq_intr.size;
            break;
        case DEVDRV_ADDR_STARS_TOPIC_SCHED_RES_MEM_BASE:
            *addr = pci_ctrl->res.stars_topic_sched_rsv_mem.addr;
            *size = pci_ctrl->res.stars_topic_sched_rsv_mem.size;
            break;
        case DEVDRV_ADDR_STARS_INTR_BASE:
            *addr = pci_ctrl->res.stars_intr.addr;
            *size = pci_ctrl->res.stars_intr.size;
            break;
        case DEVDRV_ADDR_TS_SHARE_MEM:
            *addr = pci_ctrl->res.ts_share_mem.addr;
            *size = pci_ctrl->res.ts_share_mem.size;
            break;
        case DEVDRV_ADDR_TS_NOTIFY_TBL_BASE:
            *addr = pci_ctrl->res.ts_notify.addr;
            *size = pci_ctrl->res.ts_notify.size;
            break;
        case DEVDRV_ADDR_TS_EVENT_TBL_NS_BASE:
            *addr = pci_ctrl->res.ts_event.addr;
            *size = pci_ctrl->res.ts_event.size;
            break;
        default:
            devdrv_err("Parameter type is error. (dev_id=%d; type=%d)\n", pci_ctrl->dev_id, (int)type);
            ret = -EINVAL;
            break;
    }

    return ret;
}

STATIC int devdrv_get_mdev_addr_info(struct devdrv_pci_ctrl *pci_ctrl, enum devdrv_addr_type type,
    u64 *addr, size_t *size)
{
    int ret = 0;
    switch (type) {
        case DEVDRV_ADDR_DVPP_BASE:
            *addr = pci_ctrl->res.dvpp.addr;
            *size = pci_ctrl->res.dvpp.size;
            break;
        default:
            devdrv_err("Parameter type is error. (dev_id=%d; type=%d)\n", pci_ctrl->dev_id, (int)type);
            ret = -EINVAL;
            break;
    }

    return ret;
}

int devdrv_get_addr_info(u32 devid, enum devdrv_addr_type type, u32 index, u64 *addr, size_t *size)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret;

    ret = devdrv_get_addr_info_para_check(index, addr, size);
    if (ret != 0) {
        devdrv_err("Parameter is error. (dev_id=%d; ret=%d)\n", devid, ret);
        return ret;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Parameter is error. (dev_id=%d)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    *size = 0;

    if (type < DEVDRV_ADDR_STARS_SQCQ_BASE) {
        ret = devdrv_get_addr_info_by_type(pci_ctrl, type, addr, size);
    } else if (type < DEVDRV_ADDR_DVPP_BASE) {
        ret = devdrv_get_stars_addr_info(pci_ctrl, type, addr, size);
    } else {
        ret = devdrv_get_mdev_addr_info(pci_ctrl, type, addr, size);
    }

    if (*size == 0) {
        ret = -EOPNOTSUPP;
    }

    return ret;
}
EXPORT_SYMBOL(devdrv_get_addr_info);

int devdrv_pcie_read_proc(u32 dev_id, enum devdrv_addr_type type, u32 offset, unsigned char *value, u32 len)
{
    u64 phy_addr;
    size_t size;
    int ret;
    u32 i;
    void __iomem *mem_base = NULL;

    if ((value == NULL) || (len == 0) || (len > DEVDRV_VALUE_SIZE)) {
        devdrv_err("Input parameter is invalid. (len=%d; offset=%d; dev_id=%u)\n", len, offset, dev_id);
        return -EINVAL;
    }

    ret = devdrv_get_addr_info(dev_id, type, 0, &phy_addr, &size);
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            devdrv_err("Get address information failed. (dev_id=%u; type=%d; ret=%d)\n", dev_id, (int)type, ret);
        }
        return ret;
    }

    if (((u64)offset >= size) || ((u64)len > size) || ((u64)offset + (u64)len > size)) {
        devdrv_err("Parameter offset len check failed. (dev_id=%u; offset=%d; len=%d)\n", dev_id, offset, len);
        return -EINVAL;
    }

    mutex_lock(&g_devdrv_ctrl_mutex);

    if (g_devdrv_ctrl_hot_reset_status == 1) {
        mutex_unlock(&g_devdrv_ctrl_mutex);
        devdrv_err("Pcie hotreset, but device mask on process. (dev %d; mask=%llu)\n",
                   dev_id, g_devdrv_ctrl_hot_reset_status);
        return -EAGAIN;
    }

    mem_base = ioremap(phy_addr, size);
    if (mem_base == NULL) {
        mutex_unlock(&g_devdrv_ctrl_mutex);
        devdrv_err("Ioremap failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    for (i = 0; i < len; i++) {
        value[i] = *((u8 *)mem_base + offset + i);
    }

    iounmap(mem_base);
    mem_base = NULL;

    mutex_unlock(&g_devdrv_ctrl_mutex);
    return 0;
}
EXPORT_SYMBOL(devdrv_pcie_read_proc);

int devdrv_pcie_write_proc(u32 dev_id, enum devdrv_addr_type type, u32 offset, unsigned char *value, u32 len)
{
    u64 phy_addr;
    size_t size;
    int ret;
    u32 i;
    void __iomem *mem_base = NULL;

    if ((value == NULL) || (len == 0) || (len > DEVDRV_VALUE_SIZE)) {
        devdrv_err("Input parameter is invalid. (len=%d; offset=%d; dev_id=%u)\n", len, offset, dev_id);
        return -EINVAL;
    }

    ret = devdrv_get_addr_info(dev_id, type, 0, &phy_addr, &size);
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            devdrv_err("Get address information failed. (dev_id=%u; type=%d; ret=%d)\n", dev_id, (int)type, ret);
        }
        return ret;
    }

    if (((u64)offset >= size) || ((u64)len > size) || ((u64)offset + (u64)len > size)) {
        devdrv_err("Parameter offset len check failed. (dev_id=%u; offset=%d; len=%d)\n", dev_id, offset, len);
        return -EINVAL;
    }

    mutex_lock(&g_devdrv_ctrl_mutex);

    if (g_devdrv_ctrl_hot_reset_status == 1) {
        mutex_unlock(&g_devdrv_ctrl_mutex);
        devdrv_err("Pcie hotreset, but device mask on process. (dev %d; mask=%llu)\n",
                   dev_id, g_devdrv_ctrl_hot_reset_status);
        return -EAGAIN;
    }

    mem_base = ioremap(phy_addr, size);
    if (mem_base == NULL) {
        mutex_unlock(&g_devdrv_ctrl_mutex);
        devdrv_err("Ioremap failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    for (i = 0; i < len; i++) {
        *((u8 *)mem_base + offset + i) = value[i];
    }

    iounmap(mem_base);
    mem_base = NULL;

    mutex_unlock(&g_devdrv_ctrl_mutex);
    return 0;
}
EXPORT_SYMBOL(devdrv_pcie_write_proc);

int devdrv_get_ts_drv_irq_vector_id(u32 devid, u32 index, unsigned int *entry)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 id;
    u32 i;

    if (entry == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Can not get dev_ctrl. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    id = 0;
    for (i = 0; i < pci_ctrl->msix_irq_num; i++) {
        if (devdrv_is_tsdrv_irq(&pci_ctrl->res.intr, (int)i) != true) {
            continue;
        }

        if (id == index) {
            if ((devdrv_get_host_type() == HOST_TYPE_ARM_3559) ||
                (devdrv_get_host_type() == HOST_TYPE_ARM_3519)) {
                *entry = i;
            } else {
                *entry = pci_ctrl->msix_ctrl.entries[i].entry;
            }
            return 0;
        }
        id++;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(devdrv_get_ts_drv_irq_vector_id);

int devdrv_get_topic_sched_irq_vector_id(u32 devid, u32 index, unsigned int *entry)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (entry == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if (index >= pci_ctrl->res.intr.topic_sched_irq_num) {
        devdrv_err("Index overflow. (dev_id=%d; index=%d; max=%d)\n",
                   devid, index, pci_ctrl->res.intr.topic_sched_irq_num);
        return -EINVAL;
    }

    *entry = pci_ctrl->res.intr.topic_sched_irq_base + index;
    return 0;
}
EXPORT_SYMBOL(devdrv_get_topic_sched_irq_vector_id);

int devdrv_get_cdqm_irq_vector_id(u32 devid, u32 index, unsigned int *entry)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (entry == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if (index >= pci_ctrl->res.intr.cdqm_irq_num) {
        devdrv_err("Index overflow. (dev_id=%d; index=%d; max=%d)\n", devid, index, pci_ctrl->res.intr.cdqm_irq_num);
        return -EINVAL;
    }

    *entry = pci_ctrl->res.intr.cdqm_irq_base + index;
    return 0;
}
EXPORT_SYMBOL(devdrv_get_cdqm_irq_vector_id);

int devdrv_get_irq_vector(u32 devid, u32 entry, unsigned int *irq)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 main_entry;

    if (irq == NULL) {
        devdrv_info("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_info("Can not get dev_ctrl. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if (entry >= pci_ctrl->msix_irq_num * (pci_ctrl->dev_id_in_pdev + 1)) {
        devdrv_err("Parameter entry is error. (dev_id=%u; dev_id_in_pdev=%u; entry=%u)\n",
            devid, pci_ctrl->dev_id_in_pdev, entry);
        return -EINVAL;
    }

    if ((devdrv_get_host_type() == HOST_TYPE_ARM_3559) ||
        (devdrv_get_host_type() == HOST_TYPE_ARM_3519)) {
        *irq = entry;
    } else {
        main_entry = entry - pci_ctrl->msix_irq_num * pci_ctrl->dev_id_in_pdev;
        *irq = pci_ctrl->msix_ctrl.entries[main_entry].vector;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_get_irq_vector);

int devdrv_register_irq_by_vector_index(u32 dev_id, int vector_index,
    irqreturn_t (*callback_func)(int, void *), void *para, const char *name)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    struct devdrv_host_dma_addr_to_pa_cmd cmd_data = {0};
    u32 data_len;
#endif
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret = 0;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_info("Can not get pci_ctrl. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_register_irq_func((void *)pci_ctrl, vector_index, callback_func, para, name);
    if (ret != 0) {
        devdrv_err("devdrv_register_irq_func failed. (dev_id=%d; ret=%d)\n", pci_ctrl->dev_id, ret);
        return ret;
    }

    /* hccs peh's Virtualization pass-through or smmu enabled, msi-x table will be va,
       need retranslation to pa by agent-smmu agter request */
#ifdef CFG_FEATURE_AGENT_SMMU
    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        return 0;
    }

    if ((pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) &&
        (((pci_ctrl->pdev->is_physfn == 0) && (pci_ctrl->pdev->is_virtfn == 0)) ||
        ((pci_ctrl->env_boot_mode == DEVDRV_SRIOV_VF_BOOT) && (pci_ctrl->shr_para->sid != 0)))) {
        data_len = sizeof(struct devdrv_host_dma_addr_to_pa_cmd);
        cmd_data.sub_cmd = DEVDRV_PEH_MSI_TABLE_REFRESH;
        cmd_data.host_devid = pci_ctrl->dev_id;
        ret = devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_HCCS_HOST_DMA_ADDR_MAP, &cmd_data, data_len,
            NULL, data_len);
        if (ret != 0) {
            (void)devdrv_unregister_irq_func((void *)pci_ctrl, vector_index, para);
            devdrv_err("Msi irq table reset fail(devid=%u, ret=%d\n", pci_ctrl->dev_id, ret);
            return ret;
        }
    }
#endif

    return ret;
}
EXPORT_SYMBOL(devdrv_register_irq_by_vector_index);

int devdrv_unregister_irq_by_vector_index(u32 dev_id, int vector_index, void *para)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_unregister_irq_func((void *)pci_ctrl, vector_index, para);
    if (ret != 0) {
        devdrv_err("devdrv_register_irq_func failed. (dev_id=%d; ret=%d)\n", pci_ctrl->dev_id, ret);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_unregister_irq_by_vector_index);

int devdrv_register_irq_func_expand(u32 dev_id, int vector, irqreturn_t (*callback_func)(int, void *), void *para,
    const char *name)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret = 0;

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_info("Can not get dev_ctrl. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if (vector < 0) {
        devdrv_err("Vector index less than zero. (vector=%d)\n", vector);
        return -EINVAL;
    }

    if ((devdrv_get_host_type() == HOST_TYPE_ARM_3559) || (devdrv_get_host_type() == HOST_TYPE_ARM_3519)) {
        if (vector >= DEVDRV_MSI_MAX_VECTORS) {
            devdrv_err("Invalid vector. (vector=%d)\n", vector);
            return -EINVAL;
        }
        pci_ctrl->msi_info[vector].callback_func = callback_func;
        pci_ctrl->msi_info[vector].data = para;
    } else {
        ret = request_irq((unsigned int)vector, callback_func, 0, name, para);
    }

    return ret;
}
EXPORT_SYMBOL(devdrv_register_irq_func_expand);

int devdrv_unregister_irq_func_expand(u32 dev_id, int vector, void *para)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if (vector < 0) {
        devdrv_err("Vector index less than zero. (vector=%d)\n", vector);
        return -EINVAL;
    }

    if ((devdrv_get_host_type() == HOST_TYPE_ARM_3559) || (devdrv_get_host_type() == HOST_TYPE_ARM_3519)) {
        if (vector >= DEVDRV_MSI_MAX_VECTORS) {
            devdrv_err("Invalid vector. (dev_id=%d; vector=%d)\n", pci_ctrl->dev_id, vector);
            return -EINVAL;
        }
        pci_ctrl->msi_info[vector].callback_func = NULL;
    } else {
        (void)irq_set_affinity_hint((unsigned int)vector, NULL);
        (void)free_irq((unsigned int)vector, para);
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_unregister_irq_func_expand);

int devdrv_get_pci_dev_info(u32 devid, struct devdrv_pci_dev_info *dev_info)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (dev_info == NULL) {
        devdrv_info("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_info("Can not get dev_ctrl. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;
    dev_info->bus_no = (u8)pci_ctrl->pdev->bus->number;
    dev_info->device_no = (u8)((pci_ctrl->pdev->devfn >> DEVDRV_DEVFN_BIT) & DEVDRV_DEVFN_DEV_VAL);
    dev_info->function_no = (u8)(pci_ctrl->pdev->devfn & DEVDRV_DEVFN_FN_VAL);

    return 0;
}
EXPORT_SYMBOL(devdrv_get_pci_dev_info);

int devdrv_get_pcie_id_info(u32 devid, struct devdrv_pcie_id_info *dev_info)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (dev_info == NULL) {
        devdrv_info("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_info("Can not get dev_ctrl. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;
    dev_info->venderid = pci_ctrl->pdev->vendor;
    dev_info->subvenderid = pci_ctrl->pdev->subsystem_vendor;
    dev_info->deviceid = pci_ctrl->pdev->device;
    dev_info->subdeviceid = pci_ctrl->pdev->subsystem_device;
    dev_info->bus = pci_ctrl->pdev->bus->number;
    dev_info->device = (pci_ctrl->pdev->devfn >> DEVDRV_DEVFN_BIT) & DEVDRV_DEVFN_DEV_VAL;
    dev_info->fn = (pci_ctrl->pdev->devfn) & DEVDRV_DEVFN_FN_VAL;
    dev_info->domain = pci_domain_nr(pci_ctrl->pdev->bus);

    return 0;
}
EXPORT_SYMBOL(devdrv_get_pcie_id_info);

STATIC struct devdrv_dma_desc_rbtree_node *devdrv_dma_desc_node_search(spinlock_t *lock,
    struct rb_root *root, u64 hash_va)
{
    struct rb_node *node = NULL;
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;

    if (lock != NULL) {
        spin_lock_bh(lock);
    }

    node = root->rb_node;
    while (node != NULL) {
        dma_desc_node = rb_entry(node, struct devdrv_dma_desc_rbtree_node, node);
        if (hash_va < dma_desc_node->hash_va) {
            node = node->rb_left;
        } else if (hash_va > dma_desc_node->hash_va) {
            node = node->rb_right;
        } else {
            if (lock != NULL) {
                spin_unlock_bh(lock);
            }
            return dma_desc_node;
        }
    }

    if (lock != NULL) {
        spin_unlock_bh(lock);
    }

    return NULL;
}

STATIC int devdrv_dma_desc_node_insert(spinlock_t *lock, struct rb_root *root,
    struct devdrv_dma_desc_rbtree_node *dma_desc_node)
{
    struct devdrv_dma_desc_rbtree_node *this = NULL;
    struct rb_node *parent = NULL;
    struct rb_node **new_node = NULL;

    spin_lock_bh(lock);
    new_node = &(root->rb_node);
    while ((*new_node) != NULL) {
        this = rb_entry(*new_node, struct devdrv_dma_desc_rbtree_node, node);
        parent = *new_node;
        if (dma_desc_node->hash_va < this->hash_va) {
            new_node = &((*new_node)->rb_left);
        } else if (dma_desc_node->hash_va > this->hash_va) {
            new_node = &((*new_node)->rb_right);
        } else {
            spin_unlock_bh(lock);
            return -EINVAL;
        }
    }

    rb_link_node(&dma_desc_node->node, parent, new_node);
    rb_insert_color(&dma_desc_node->node, root);

    spin_unlock_bh(lock);
    return 0;
}

STATIC void devdrv_dma_desc_node_erase(spinlock_t *lock, struct rb_root *root,
    struct devdrv_dma_desc_rbtree_node *dma_desc_node)
{
    if (lock != NULL) {
        spin_lock_bh(lock);
    }

    rb_erase(&dma_desc_node->node, root);

    if (lock != NULL) {
        spin_unlock_bh(lock);
    }
}

STATIC int devdrv_dma_link_prepare_para_check(u32 devid, const struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (dma_node == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (node_cnt == 0) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    return 0;
}

STATIC int devdrv_dma_link_alloc_sq_cq_addr(struct devdrv_dma_prepare *dma_prepare, struct device *dev, u32 node_cnt)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
    gfp_t gfp = GFP_KERNEL | __GFP_ACCOUNT | __GFP_NOWARN | __GFP_RETRY_MAYFAIL;
#else
    gfp_t gfp = GFP_KERNEL | __GFP_ACCOUNT | __GFP_NOWARN;
#endif
    u64 size;

    size = ((u64)node_cnt + DEVDRV_RESERVE_NUM) * sizeof(struct devdrv_dma_sq_node);
    dma_prepare->sq_size = size;
    dma_prepare->sq_base = devdrv_dma_zalloc_coherent(dev, dma_prepare->sq_size, &dma_prepare->sq_dma_addr, gfp);
    if (dma_prepare->sq_base == NULL) {
        dma_prepare->sq_base = devdrv_dma_zalloc_coherent(dev, dma_prepare->sq_size, &dma_prepare->sq_dma_addr,
            GFP_DMA32 | gfp);
        if (dma_prepare->sq_base == NULL) {
            devdrv_warn("DMA sq alloc failed. (dev_id=%u)\n", dma_prepare->devid);
            return -ENOMEM;
        }
    }

    size = ((u64)node_cnt + DEVDRV_RESERVE_NUM) * sizeof(struct devdrv_dma_cq_node);
    dma_prepare->cq_size = size;
    dma_prepare->cq_base = devdrv_dma_zalloc_coherent(dev, dma_prepare->cq_size, &dma_prepare->cq_dma_addr, gfp);
    if (dma_prepare->cq_base == NULL) {
        dma_prepare->cq_base = devdrv_dma_zalloc_coherent(dev, dma_prepare->cq_size, &dma_prepare->cq_dma_addr,
            GFP_DMA32 | gfp);
        if (dma_prepare->cq_base == NULL) {
            devdrv_warn("DMA cq alloc failed. (dev_id=%u)\n", dma_prepare->devid);
            devdrv_dma_free_coherent(dev, dma_prepare->sq_size, dma_prepare->sq_base, dma_prepare->sq_dma_addr);
            dma_prepare->sq_base = NULL;
            return -ENOMEM;
        }
    }
    return 0;
}

STATIC void devdrv_dma_desc_node_free(struct devdrv_dma_desc_rbtree_node *dma_desc_node)
{
    devdrv_kfree(dma_desc_node);
    dma_desc_node = NULL;
}

STATIC int devdrv_dma_prepare_alloc_sq_addr_by_vpc(u32 devid, u32 node_cnt, struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_vpc_msg vpc_msg = {0};
    int ret;

    vpc_msg.cmd_data.dma_info.dev_id = devid;
    vpc_msg.cmd_data.dma_info.node_cnt = node_cnt;
    vpc_msg.error_code = 0;
    ret = devdrv_vpc_msg_send(devid, DEVDRV_VPC_MSG_TYPE_DMA_LINK_SQ_ALLOC, &vpc_msg, sizeof(struct devdrv_vpc_msg),
        DEVDRV_VPC_MSG_DEFAULT_TIMEOUT);
    if ((ret != 0) || (vpc_msg.error_code != 0)) {
        devdrv_err("VPC msg send fail. (dev_id=%u, ret=%d, error_code=%d)\n", devid, ret, vpc_msg.error_code);
        return -EINVAL;
    }

    dma_prepare->devid = devid;
    dma_prepare->sq_dma_addr = vpc_msg.cmd_data.dma_info.dma_desc_info.sq_dma_addr;
    dma_prepare->sq_size = vpc_msg.cmd_data.dma_info.dma_desc_info.sq_size;
    dma_prepare->sq_base = NULL;
    dma_prepare->cq_dma_addr = (~(dma_addr_t)0);
    dma_prepare->cq_size = 0;
    dma_prepare->cq_base = NULL;

    return 0;
}

int devdrv_dma_prepare_alloc_sq_addr(u32 devid, u32 node_cnt, struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u64 size;
    int ret;

    if ((devid >= MAX_DEV_CNT) || (node_cnt) == 0 || (dma_prepare == NULL)) {
        devdrv_err("dma_prepare is null. (dev_id=%u, node_cnt=%u)\n", devid, node_cnt);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_warn("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (node_cnt > (pci_ctrl->res.dma_res.sq_depth - pci_ctrl->res.dma_res.sq_rsv_num)) {
        devdrv_err_spinlock("node_cnt is illegal. (dev_id=%u; node_cnt=%u)\n", devid, node_cnt);
        return -EINVAL;
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        return devdrv_dma_prepare_alloc_sq_addr_by_vpc(devid, node_cnt, dma_prepare);
    }

    size = ((u64)node_cnt + DEVDRV_RESERVE_NUM) * sizeof(struct devdrv_dma_sq_node);
    dma_prepare->sq_size = size;
    dma_prepare->sq_base = devdrv_dma_zalloc_coherent(&pci_ctrl->pdev->dev, dma_prepare->sq_size,
        &dma_prepare->sq_dma_addr, GFP_KERNEL | __GFP_ACCOUNT | __GFP_NOWARN);
    if (dma_prepare->sq_base == NULL) {
        dma_prepare->sq_base = devdrv_dma_zalloc_coherent(&pci_ctrl->pdev->dev, dma_prepare->sq_size,
            &dma_prepare->sq_dma_addr, GFP_DMA32 | GFP_KERNEL | __GFP_ACCOUNT | __GFP_NOWARN);
        if (dma_prepare->sq_base == NULL) {
            devdrv_warn("DMA sq alloc failed. (dev_id=%u)\n", devid);
            return -ENOMEM;
        }
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_PM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT)) {
        dma_desc_node = devdrv_kzalloc(sizeof(struct devdrv_dma_desc_rbtree_node), GFP_KERNEL | __GFP_ACCOUNT);
        if (dma_desc_node == NULL) {
            devdrv_dma_free_coherent(&pci_ctrl->pdev->dev, dma_prepare->sq_size, dma_prepare->sq_base,
                dma_prepare->sq_dma_addr);
            devdrv_err("Alloc dma_desc_node failed. (dev_id=%u)\n", devid);
            return -ENOMEM;
        }
        dma_desc_node->dma_prepare = dma_prepare;
        dma_desc_node->hash_va = dma_prepare->sq_dma_addr;

        ret = devdrv_dma_desc_node_insert(&pci_ctrl->dma_desc_rblock, &pci_ctrl->dma_desc_rbtree, dma_desc_node);
        if (ret != 0) {
            devdrv_dma_free_coherent(&pci_ctrl->pdev->dev, dma_prepare->sq_size, dma_prepare->sq_base,
                dma_prepare->sq_dma_addr);
            devdrv_kfree(dma_desc_node);
            devdrv_err("Insert dma desc node failed. (dev_id=%u)\n", devid);
            return -EINVAL;
        }
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_dma_prepare_alloc_sq_addr);

STATIC void devdrv_dma_prepare_free_sq_addr_by_vpc(u32 devid, struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_vpc_msg vpc_msg = {0};
    int ret;

    vpc_msg.cmd_data.dma_info.dev_id = devid;
    vpc_msg.cmd_data.dma_info.dma_desc_info.sq_dma_addr = dma_prepare->sq_dma_addr;
    vpc_msg.cmd_data.dma_info.dma_desc_info.sq_size = dma_prepare->sq_size;
    vpc_msg.error_code = 0;
    ret = devdrv_vpc_msg_send(devid, DEVDRV_VPC_MSG_TYPE_DMA_LINK_SQ_FREE, &vpc_msg, sizeof(struct devdrv_vpc_msg),
        DEVDRV_VPC_MSG_DEFAULT_TIMEOUT);
    if ((ret != 0) || (vpc_msg.error_code != 0)) {
        devdrv_err("VPC msg send fail. (dev_id=%u, ret=%d, error_code=%d)\n", devid, ret, vpc_msg.error_code);
    }

    return;
}

void devdrv_dma_prepare_free_sq_addr(u32 devid, struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;
    struct devdrv_dma_prepare *dma_prepare_tmp = dma_prepare;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (dma_prepare_tmp == NULL) {
        devdrv_err("dma_prepare is null. (dev_id=%u)\n", devid);
        return;
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_warn("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return;
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        devdrv_dma_prepare_free_sq_addr_by_vpc(devid, dma_prepare_tmp);
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_PM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT)) {
        spin_lock_bh(&pci_ctrl->dma_desc_rblock);
        dma_desc_node = devdrv_dma_desc_node_search(NULL, &pci_ctrl->dma_desc_rbtree, dma_prepare_tmp->sq_dma_addr);
        if (dma_desc_node == NULL) {
            spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
            devdrv_err("Search dma desc node failed. (dev_id=%u)\n", devid);
            return;
        }

        devdrv_dma_desc_node_erase(NULL, &pci_ctrl->dma_desc_rbtree, dma_desc_node);
        spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
        dma_prepare_tmp = dma_desc_node->dma_prepare;
    }

    if (dma_prepare_tmp->sq_base != NULL) {
        devdrv_dma_free_coherent(&pci_ctrl->pdev->dev, dma_prepare_tmp->sq_size, dma_prepare_tmp->sq_base,
            dma_prepare_tmp->sq_dma_addr);
        dma_prepare_tmp->sq_base = NULL;
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_PM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT)) {
        devdrv_kfree(dma_prepare_tmp);
        dma_prepare_tmp = NULL;
        devdrv_dma_desc_node_free(dma_desc_node);
        dma_desc_node = NULL;
    }
}
EXPORT_SYMBOL(devdrv_dma_prepare_free_sq_addr);

int devdrv_dma_link_sq_node_num(const struct devdrv_dma_prepare *dma_prepare)
{
    return (int)(dma_prepare->sq_size / sizeof(struct devdrv_dma_sq_node) - DEVDRV_RESERVE_NUM);
}

int devdrv_dma_get_sq_cq_desc_size(u32 devid, u32 *sq_desc_size, u32 *cq_desc_size)
{
    if ((sq_desc_size == NULL) || (cq_desc_size == NULL)) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    *sq_desc_size = (u32)sizeof(struct devdrv_dma_sq_node);
    *cq_desc_size = (u32)sizeof(struct devdrv_dma_cq_node);

    return 0;
}
EXPORT_SYMBOL(devdrv_dma_get_sq_cq_desc_size);

STATIC int devdrv_dma_fill_sqs_desc_check(u32 devid, const struct devdrv_dma_prepare *dma_prepare,
    const struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (dma_prepare == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (dma_node == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (node_cnt == 0) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    return 0;
}

STATIC void devdrv_dma_link_prepare_vpc_msg_init(struct devdrv_vpc_msg *vpc_msg, u32 devid,
    enum devdrv_dma_data_type type, u32 fill_status)
{
    vpc_msg->cmd_data.dma_info.dev_id = devid;
    vpc_msg->cmd_data.dma_info.type = type;
    vpc_msg->cmd_data.dma_info.fill_status = fill_status;
    vpc_msg->cmd_data.dma_info.dma_desc_info.cq_dma_addr = 0;
    vpc_msg->cmd_data.dma_info.dma_desc_info.cq_size = 0;
    vpc_msg->cmd_data.dma_info.dma_desc_info.sq_dma_addr = 0;
    vpc_msg->cmd_data.dma_info.dma_desc_info.sq_size = 0;
}

STATIC void devdrv_dma_link_prepare_dma_node_init(struct devdrv_vpc_msg *vpc_msg,
    struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    u32 node_index;

    vpc_msg->cmd_data.dma_info.node_cnt = node_cnt;
    for (node_index = 0; node_index < node_cnt; node_index++) {
        vpc_msg->cmd_data.dma_info.dma_node[node_index].src_addr = dma_node[node_index].src_addr;
        vpc_msg->cmd_data.dma_info.dma_node[node_index].dst_addr = dma_node[node_index].dst_addr;
        vpc_msg->cmd_data.dma_info.dma_node[node_index].size = dma_node[node_index].size;
        vpc_msg->cmd_data.dma_info.dma_node[node_index].direction = dma_node[node_index].direction;
        vpc_msg->cmd_data.dma_info.dma_node[node_index].loc_passid = dma_node[node_index].loc_passid;
    }
}

STATIC int devdrv_dma_fill_desc_of_sq_by_vpc(u32 devid, struct devdrv_dma_prepare *dma_prepare,
    struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status)
{
    struct devdrv_vpc_msg *vpc_msg = NULL;
    u32 dma_info_buf_len;
    int ret;

    if (node_cnt > DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT) {
        devdrv_err("DMA node cnt is too big. (dev_id=%u, node_cnt=%u)\n", devid, node_cnt);
        return -EINVAL;
    }

    dma_info_buf_len = (u32)(sizeof(struct devdrv_dma_node) * node_cnt + sizeof(struct devdrv_vpc_msg));
    vpc_msg = (struct devdrv_vpc_msg *)devdrv_kvzalloc(dma_info_buf_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (vpc_msg == NULL) {
        devdrv_err("Alloc vpc_msg fail. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    devdrv_dma_link_prepare_vpc_msg_init(vpc_msg, devid, 0, fill_status);
    devdrv_dma_link_prepare_dma_node_init(vpc_msg, dma_node, node_cnt);
    vpc_msg->cmd_data.dma_info.dma_desc_info.sq_dma_addr = dma_prepare->sq_dma_addr;
    vpc_msg->cmd_data.dma_info.dma_desc_info.sq_size = dma_prepare->sq_size;
    vpc_msg->error_code = 0;
    ret = devdrv_vpc_msg_send(devid, DEVDRV_VPC_MSG_TYPE_DMA_SQ_DESC_FILL, vpc_msg, dma_info_buf_len,
        DEVDRV_VPC_MSG_DEFAULT_TIMEOUT);
    if ((ret != 0) || (vpc_msg->error_code != 0)) {
        devdrv_err("VPC msg send fail. (dev_id=%u, ret=%d, error_code=%d)\n", devid, ret, vpc_msg->error_code);
        ret = -EINVAL;
    }

    devdrv_kvfree(vpc_msg);
    return ret;
}

int devdrv_dma_fill_desc_of_sq(u32 devid, struct devdrv_dma_prepare *dma_prepare, struct devdrv_dma_node *dma_node,
    u32 node_cnt, u32 fill_status)
{
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;
    struct devdrv_dma_sq_node *current_sq_node = NULL;
    struct devdrv_dma_prepare *dma_prepare_tmp = dma_prepare;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    void *sq_base = NULL;
    int connect_protocol;
    u32 i;
    int ret;

    ret = devdrv_dma_fill_sqs_desc_check(devid, dma_prepare_tmp, dma_node, node_cnt);
    if (ret != 0) {
        devdrv_err("DMA fill sqs desc parameter check failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        return devdrv_dma_fill_desc_of_sq_by_vpc(devid, dma_prepare, dma_node, node_cnt, fill_status);
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_PM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT)) {
        spin_lock_bh(&pci_ctrl->dma_desc_rblock);
        dma_desc_node = devdrv_dma_desc_node_search(NULL, &pci_ctrl->dma_desc_rbtree, dma_prepare->sq_dma_addr);
        if (dma_desc_node == NULL) {
            spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
            devdrv_err("Search dma desc node failed. (dev_id=%u)\n", dma_prepare->devid);
            return -EINVAL;
        }
        spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
        dma_prepare_tmp = dma_desc_node->dma_prepare;
    }

    connect_protocol = pci_ctrl->connect_protocol;
    sq_base = dma_prepare_tmp->sq_base;
    for (i = 0; i < node_cnt; i++) {
        if (connect_protocol == CONNECT_PROTOCOL_HCCS) {
            ret = devdrv_peh_dma_node_addr_check(&dma_node[i]);
            if (ret != 0) {
                devdrv_err("Peh dma addr range check.(dev_id=%u, dma_node index=%d)\n", devid, i);
                return -EINVAL;
            }
        }

        current_sq_node = (struct devdrv_dma_sq_node *)sq_base + i;
        current_sq_node->src_addr_l = (u32)(dma_node[i].src_addr & 0xFFFFFFFFU);
        current_sq_node->src_addr_h = (u32)(dma_node[i].src_addr >> DEVDRV_MOVE_BIT_32);
        current_sq_node->dst_addr_l = (u32)(dma_node[i].dst_addr & 0xFFFFFFFFU);
        current_sq_node->dst_addr_h = (u32)(dma_node[i].dst_addr >> DEVDRV_MOVE_BIT_32);
        current_sq_node->length = dma_node[i].size;
        if (connect_protocol == CONNECT_PROTOCOL_HCCS) {
            current_sq_node->opcode = DEVDRV_DMA_LOOP;
        } else if (dma_node[i].direction == DEVDRV_DMA_DEVICE_TO_HOST) {
            current_sq_node->opcode = DEVDRV_DMA_WRITE;
        } else if (dma_node[i].direction == DEVDRV_DMA_HOST_TO_DEVICE) {
            current_sq_node->opcode = DEVDRV_DMA_READ;
        } else {
            devdrv_err("DMA direction parameter is invalid. (dev_id=%u)\n", devid);
            return -EINVAL;
        }
        current_sq_node->pf = pci_ctrl->ep_pf_index;
        current_sq_node->vf = pci_ctrl->res.dma_res.vf_id;
        current_sq_node->vfen = pci_ctrl->virtfn_flag;
        if ((fill_status == DEVDRV_DMA_DESC_FILL_FINISH) && (i == node_cnt - 1)) {
            current_sq_node->ldie = 1;
        }
        /* enable RO.remote for RD.remote np and WD.remote p */
        current_sq_node->attr = DEVDRV_DMA_SQ_ATTR_RO;
        /* enable RO.local for RD.local p and WD.local np */
        current_sq_node->attr_d = DEVDRV_DMA_SQ_ATTR_RO;

        if ((connect_protocol == CONNECT_PROTOCOL_PCIE) || (dma_node[i].direction == DEVDRV_DMA_HOST_TO_DEVICE)) {
            current_sq_node->pa_rmt = DEVDRV_DMA_DES_PA_RMT_PA;
            current_sq_node->pa_loc = DEVDRV_DMA_DES_PA_LOC_VA;
            current_sq_node->addrt_d = DEVDRV_DMA_DES_AT_LOC_VA;
            current_sq_node->flow_id_rmt = dma_node[i].loc_passid & DEVDRV_DMA_DES_FLOW_ID_RMT_MASK;
            current_sq_node->flow_id_loc_l =
                (dma_node[i].loc_passid >> DEVDRV_DMA_DES_FLOW_ID_LOC_L_SHIFT) & DEVDRV_DMA_DES_FLOW_ID_LOC_L_MASK;
            current_sq_node->flow_id_loc_h =
                (dma_node[i].loc_passid >> DEVDRV_DMA_DES_FLOW_ID_LOC_H_SHIFT) & DEVDRV_DMA_DES_FLOW_ID_LOC_H_MASK;
        } else {
            current_sq_node->pa_rmt = DEVDRV_DMA_DES_PA_RMT_VA;
            current_sq_node->pa_loc = DEVDRV_DMA_DES_PA_LOC_PA;
            current_sq_node->addrt_d = DEVDRV_DMA_DES_AT_LOC_PA;
            if (dma_node[i].loc_passid != DEVDRV_DMA_PASSID_DEFAULT) {
                current_sq_node->pasid = dma_node[i].loc_passid;
                current_sq_node->prfen = DEVDRV_DMA_DES_PA_RMT_VA;
            }
        }

        ret = devdrv_vpc_dma_iova_addr_check(pci_ctrl, current_sq_node, dma_node[i].direction);
        if (ret != 0) {
            devdrv_err("Vm's dma_iova_addr_check check failed.(dev_id=%u)\n", pci_ctrl->dev_id);
            return -EINVAL;
        }

        if (pci_ctrl->ops.flush_cache != NULL) {
            pci_ctrl->ops.flush_cache((u64)current_sq_node, sizeof(struct devdrv_dma_sq_node), CACHE_CLEAN);
        }
    }
    return 0;
}
EXPORT_SYMBOL(devdrv_dma_fill_desc_of_sq);

STATIC struct devdrv_dma_prepare *devdrv_dma_link_prepare_by_vpc(u32 devid, enum devdrv_dma_data_type type,
    struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status)
{
    struct devdrv_dma_prepare *dma_prepare = NULL;
    struct devdrv_vpc_msg *vpc_msg = NULL;
    u32 dma_info_buf_len;
    int ret;

    if (node_cnt > DEVDRV_VPC_MAX_SQ_DMA_NODE_COUNT) {
        devdrv_err("DMA node cnt is too big. (dev_id=%u, node_cnt=%u)\n", devid, node_cnt);
        return NULL;
    }

    dma_info_buf_len = (u32)(sizeof(struct devdrv_dma_node) * node_cnt + sizeof(struct devdrv_vpc_msg));
    vpc_msg = (struct devdrv_vpc_msg *)devdrv_kvzalloc(dma_info_buf_len, GFP_KERNEL | __GFP_ACCOUNT);
    if (vpc_msg == NULL) {
        devdrv_err("Alloc vpc_msg fail. (dev_id=%u)\n", devid);
        return NULL;
    }

    dma_prepare = devdrv_kvzalloc(sizeof(struct devdrv_dma_prepare), GFP_KERNEL | __GFP_ACCOUNT);
    if (dma_prepare == NULL) {
        devdrv_err("Alloc dma_prepare fail. (dev_id=%u)\n", devid);
        goto FREE_VPC_MSG;
    }

    devdrv_dma_link_prepare_vpc_msg_init(vpc_msg, devid, type, fill_status);
    devdrv_dma_link_prepare_dma_node_init(vpc_msg, dma_node, node_cnt);
    vpc_msg->error_code = 0;
    ret = devdrv_vpc_msg_send(devid, DEVDRV_VPC_MSG_TYPE_DMA_LINK_PREPARE, vpc_msg, dma_info_buf_len,
        DEVDRV_VPC_MSG_DEFAULT_TIMEOUT);
    if ((ret != 0) || (vpc_msg->error_code != 0)) {
        devdrv_err("VPC msg send fail. (dev_id=%u, ret=%d, error_code=%d)\n", devid, ret, vpc_msg->error_code);
        goto FREE_DMA_PREPARE;
    }
    dma_prepare->cq_dma_addr = vpc_msg->cmd_data.dma_info.dma_desc_info.cq_dma_addr;
    dma_prepare->cq_size = vpc_msg->cmd_data.dma_info.dma_desc_info.cq_size;
    dma_prepare->sq_dma_addr = vpc_msg->cmd_data.dma_info.dma_desc_info.sq_dma_addr;
    dma_prepare->sq_size = vpc_msg->cmd_data.dma_info.dma_desc_info.sq_size;
    dma_prepare->devid = devid;
    dma_prepare->sq_base = NULL;
    dma_prepare->cq_base = NULL;

    devdrv_kvfree(vpc_msg);
    vpc_msg = NULL;
    return dma_prepare;
FREE_DMA_PREPARE:
    devdrv_kvfree(dma_prepare);
    dma_prepare = NULL;

FREE_VPC_MSG:
    devdrv_kvfree(vpc_msg);
    vpc_msg = NULL;

    return NULL;
}

STATIC void devdrv_dma_link_prepare_free(struct device *dev, struct devdrv_dma_prepare *dma_prepare)
{
    if (dma_prepare->cq_base != NULL) {
        devdrv_dma_free_coherent(dev, dma_prepare->cq_size, dma_prepare->cq_base, dma_prepare->cq_dma_addr);
        dma_prepare->cq_base = NULL;
    }

    if (dma_prepare->sq_base != NULL) {
        devdrv_dma_free_coherent(dev, dma_prepare->sq_size, dma_prepare->sq_base, dma_prepare->sq_dma_addr);
        dma_prepare->sq_base = NULL;
    }

    devdrv_kfree(dma_prepare);
    dma_prepare = NULL;
}

/* async DMA link prepare */
struct devdrv_dma_prepare *devdrv_dma_link_prepare(u32 devid, enum devdrv_dma_data_type type,
    struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status)
{
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;
    struct devdrv_dma_prepare *dma_prepare = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret;

    ret = devdrv_dma_link_prepare_para_check(devid, dma_node, node_cnt);
    if (ret != 0) {
        devdrv_err("Parameter check failed. (dev_id=%u)\n", devid);
        return NULL;
    }

    if (devdrv_is_mdev_vm_boot_mode(devid) == true) {
        return devdrv_dma_link_prepare_by_vpc(devid, type, dma_node, node_cnt, fill_status);
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return NULL;
    }

    dma_prepare = devdrv_kzalloc(sizeof(struct devdrv_dma_prepare), GFP_KERNEL | __GFP_ACCOUNT);
    if (dma_prepare == NULL) {
        devdrv_err("Alloc dma_prepare failed. (dev_id=%u)\n", devid);
        return NULL;
    }

    dma_prepare->devid = devid;

    ret = devdrv_dma_link_alloc_sq_cq_addr(dma_prepare, &pci_ctrl->pdev->dev, node_cnt);
    if (ret != 0) {
        devdrv_err("Alloc sq cq addr failed. (dev_id=%u)\n", devid);
        goto free_prepare;
    }

    dma_desc_node = devdrv_kzalloc(sizeof(struct devdrv_dma_desc_rbtree_node), GFP_KERNEL | __GFP_ACCOUNT);
    if (dma_desc_node == NULL) {
        devdrv_err("Alloc dma_desc_node failed. (dev_id=%u)\n", devid);
        goto free_prepare;
    }
    dma_desc_node->dma_prepare = dma_prepare;
    dma_desc_node->hash_va = dma_prepare->sq_dma_addr;

    ret = devdrv_dma_desc_node_insert(&pci_ctrl->dma_desc_rblock, &pci_ctrl->dma_desc_rbtree, dma_desc_node);
    if (ret != 0) {
        devdrv_err("Insert dma desc node failed. (dev_id=%u)\n", devid);
        goto free_dma_desc_node;
    }

    ret = devdrv_dma_fill_desc_of_sq(devid, dma_prepare, dma_node, node_cnt, fill_status);
    if (ret != 0) {
        devdrv_err("Fill sq node failed. (dev_id=%u)\n", devid);
        goto fill_desc_of_sq;
    }
    return dma_prepare;
fill_desc_of_sq:
    spin_lock_bh(&pci_ctrl->dma_desc_rblock);
    devdrv_dma_desc_node_erase(NULL, &pci_ctrl->dma_desc_rbtree, dma_desc_node);
    spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
free_dma_desc_node:
    devdrv_kfree(dma_desc_node);
    dma_desc_node = NULL;
free_prepare:
    devdrv_dma_link_prepare_free(&pci_ctrl->pdev->dev, dma_prepare);
    return NULL;
}
EXPORT_SYMBOL(devdrv_dma_link_prepare);

STATIC int devdrv_dma_link_free_by_vpc(struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_vpc_msg vpc_msg = {0};
    int ret;

    vpc_msg.cmd_data.dma_info.dma_desc_info.cq_dma_addr = dma_prepare->cq_dma_addr;
    vpc_msg.cmd_data.dma_info.dma_desc_info.cq_size = dma_prepare->cq_size;
    vpc_msg.cmd_data.dma_info.dma_desc_info.sq_dma_addr = dma_prepare->sq_dma_addr;
    vpc_msg.cmd_data.dma_info.dma_desc_info.sq_size = dma_prepare->sq_size;
    vpc_msg.error_code = 0;
    ret = devdrv_vpc_msg_send(dma_prepare->devid, DEVDRV_VPC_MSG_TYPE_DMA_LINK_FREE, &vpc_msg,
        (u32)sizeof(struct devdrv_vpc_msg), DEVDRV_VPC_MSG_DEFAULT_TIMEOUT);
    if ((ret != 0) || (vpc_msg.error_code != 0)) {
        devdrv_err("VPC send fail. (dev_id=%u, ret=%d, error_code=%d)\n", dma_prepare->devid, ret, vpc_msg.error_code);
        return -EINVAL;
    }

    devdrv_kfree(dma_prepare);
    dma_prepare = NULL;

    return 0;
}

/* async DMA link release */
int devdrv_dma_link_free(struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (dma_prepare == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }

    if (devdrv_is_mdev_vm_boot_mode(dma_prepare->devid) == true) {
        return devdrv_dma_link_free_by_vpc(dma_prepare);
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dma_prepare->devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dma_prepare->devid);
        return -EINVAL;
    }

    spin_lock_bh(&pci_ctrl->dma_desc_rblock);
    dma_desc_node = devdrv_dma_desc_node_search(NULL, &pci_ctrl->dma_desc_rbtree,
        dma_prepare->sq_dma_addr);
    if (dma_desc_node == NULL) {
        spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
        devdrv_err("Search dma desc node failed. (dev_id=%u)\n", dma_prepare->devid);
        return -EINVAL;
    }

    devdrv_dma_desc_node_erase(NULL, &pci_ctrl->dma_desc_rbtree, dma_desc_node);
    spin_unlock_bh(&pci_ctrl->dma_desc_rblock);

    devdrv_dma_link_prepare_free(&pci_ctrl->pdev->dev, dma_desc_node->dma_prepare);
    devdrv_dma_desc_node_free(dma_desc_node);

    return 0;
}
EXPORT_SYMBOL(devdrv_dma_link_free);

void devdrv_dma_desc_node_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;
    struct rb_node *node = NULL;

    spin_lock_bh(&pci_ctrl->dma_desc_rblock);
    while ((node = rb_first(&pci_ctrl->dma_desc_rbtree)) != NULL) {
        dma_desc_node = rb_entry(node, struct devdrv_dma_desc_rbtree_node, node);
        rb_erase(&dma_desc_node->node, &pci_ctrl->dma_desc_rbtree);
        spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
        devdrv_dma_link_prepare_free(&pci_ctrl->pdev->dev, dma_desc_node->dma_prepare);
        devdrv_dma_desc_node_free(dma_desc_node);
        spin_lock_bh(&pci_ctrl->dma_desc_rblock);
    }
    spin_unlock_bh(&pci_ctrl->dma_desc_rblock);
}

int devdrv_dma_sqcq_desc_check(u32 devid, struct devdrv_dma_desc_info *dma_desc_info)
{
    struct devdrv_dma_desc_rbtree_node *dma_desc_node = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (dma_desc_info == NULL) {
        devdrv_err("dma_desc_info is invalid.\n");
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    dma_desc_node = devdrv_dma_desc_node_search(&pci_ctrl->dma_desc_rblock, &pci_ctrl->dma_desc_rbtree,
        dma_desc_info->sq_dma_addr);
    if (dma_desc_node == NULL) {
        devdrv_err("Search dma desc node failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (dma_desc_info->sq_size > dma_desc_node->dma_prepare->sq_size) {
        devdrv_err("Sq size is invaild. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_dma_sqcq_desc_check);

static const struct devdrv_ops g_ops = {
    .alloc_trans_chan = devdrv_alloc_trans_queue,
    .realease_trans_chan = devdrv_free_trans_queue,
    .alloc_non_trans_chan = devdrv_alloc_non_trans_queue,
    .release_non_trans_chan = devdrv_free_non_trans_queue
};

int devdrv_register_pci_devctrl(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_ctrl pci_dev_ctrl;
    int dev_id;

    pci_dev_ctrl.priv = pci_ctrl;
    pci_dev_ctrl.dev_type = DEV_TYPE_PCI;
    pci_dev_ctrl.dev = &pci_ctrl->pdev->dev;
    pci_dev_ctrl.pdev = pci_ctrl->pdev;
    pci_dev_ctrl.bus = pci_ctrl->pdev->bus;
    pci_dev_ctrl.slot_id = pci_ctrl->slot_id;
    pci_dev_ctrl.func_id = pci_ctrl->func_id;
    pci_dev_ctrl.virtfn_flag = pci_ctrl->virtfn_flag;
    dev_id = devdrv_slave_dev_add(&pci_dev_ctrl);
    devdrv_info("Get bus value. (dev_id=%d; bus=%pK)\n", dev_id, pci_dev_ctrl.bus);

    if (dev_id < 0) {
        devdrv_err("Pci device register failed. (dev_id=%u)\n", dev_id);
        return -ENOSPC;
    } else {
        pci_ctrl->dev_id = (u32)dev_id;
        return 0;
    }
}

void devdrv_register_half_devctrl(struct devdrv_pci_ctrl *pci_ctrl)
{
    u32 dev_id;

    dev_id = pci_ctrl->dev_id;
    g_ctrls[dev_id].ops = &g_ops;
    g_ctrls[dev_id].startup_flg = DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK;
}

int devdrv_dev_online(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_pci_ctrl *ctrl = NULL;
    int devdrv_p2p_support_max_devnum;
    int i;

    devdrv_info("Device online. (dev_id=%d; chip_type=%d)\n", pci_ctrl->dev_id, pci_ctrl->shr_para->chip_type);
    mutex_lock(&g_devdrv_ctrl_mutex);

    devdrv_p2p_support_max_devnum = pci_ctrl->ops.get_p2p_support_max_devnum();
    /* config tx atu & p2p msg base */
    for (i = 0; i < devdrv_p2p_support_max_devnum; i++) {
        if (g_ctrls[i].startup_flg != DEVDRV_DEV_STARTUP_BOTTOM_HALF_OK) {
            continue;
        }

        ctrl = (struct devdrv_pci_ctrl *)g_ctrls[i].priv;

        if (pci_ctrl->dev_id == ctrl->dev_id) {
            continue;
        }

        /* notice device */
        (void)devdrv_notify_dev_online(ctrl->msg_dev, pci_ctrl->dev_id, DEVDRV_DEV_ONLINE);
    }

    (void)devdrv_notify_dev_online(pci_ctrl->msg_dev, pci_ctrl->dev_id, DEVDRV_DEV_ONLINE);

    mutex_unlock(&g_devdrv_ctrl_mutex);
    return 0;
}

void devdrv_dev_offline(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_ctrl pci_dev_ctrl;

    devdrv_info("Device offline. (dev_id=%d)\n", pci_ctrl->dev_id);

    mutex_lock(&g_devdrv_ctrl_mutex);

    /* register this pci device to devdrv_ctrls */
    pci_dev_ctrl.dev_id = pci_ctrl->dev_id;

    /* notice device offline */
    if (pci_ctrl->load_status_flag == DEVDRV_LOAD_HALF_PROBE_STATUS) {
        (void)devdrv_notify_dev_online(pci_ctrl->msg_dev, pci_ctrl->dev_id, DEVDRV_DEV_OFFLINE);
    }

    mutex_unlock(&g_devdrv_ctrl_mutex);

    return;
}

STATIC bool devdrv_is_alloc_pa_addr(struct devdrv_pci_ctrl *pci_ctrl)
{
    /* HCCS adapt CI, return pa addr, others return dma addr */
    if (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) {
        return true;
    } else {
        return false;
    }
}

void *devdrv_dma_alloc_coherent(struct device *dev, size_t size,
    dma_addr_t *dma_addr, gfp_t gfp)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    void *addr = NULL;

    if ((size == 0) || (dev == NULL) || (dma_addr == NULL)) {
        devdrv_err("Input parameter is invalid. (size=%lu, dev is %s, dma_addr is %s)\n",
            size, dev == NULL ? "null" : "not null", dma_addr == NULL ? "null" : "not null");
        return NULL;
    }
    pdev_ctrl = (struct devdrv_pdev_ctrl *)dev_get_drvdata(dev);
    if (pdev_ctrl == NULL) {
        devdrv_err("pdev_ctrl is invalid.\n");
        return NULL;
    }

    pci_ctrl = pdev_ctrl->pci_ctrl[0];
    if (pci_ctrl == NULL) {
        devdrv_err("pci_ctrl is invalid.\n");
        return NULL;
    }

    if (devdrv_is_alloc_pa_addr(pci_ctrl) == true) {
        addr = devdrv_kmalloc(size, GFP_KERNEL | __GFP_ACCOUNT); // Avoiding to alloc host low 2G addr.
        if (addr == NULL) {
            return NULL;
        }

        *dma_addr = devdrv_dma_map_single(dev, addr, size, DMA_BIDIRECTIONAL);
        if (dma_mapping_error(dev, *dma_addr) != 0) {
            devdrv_kfree(addr);
            addr = NULL;
        }
    } else {
        if (devdrv_is_mdev_pm_boot_mode(pci_ctrl->dev_id) == true) {
#ifdef CFG_FEATURE_SRIOV
            addr = hw_dvt_hypervisor_dma_alloc_coherent(dev, size, dma_addr, gfp);
#endif
        } else {
            addr = devdrv_ka_dma_alloc_coherent(dev, size, dma_addr, gfp);
        }
    }

    return addr;
}
EXPORT_SYMBOL(devdrv_dma_alloc_coherent);

void *devdrv_dma_zalloc_coherent(struct device *dev, size_t size,
    dma_addr_t *dma_addr, gfp_t gfp)
{
    void *addr = devdrv_dma_alloc_coherent(dev, size, dma_addr, gfp);

    if (addr != NULL) {
        if (memset_s(addr, size, 0, size) != 0) {
            devdrv_warn("memset_s failed.\n");
        }
    }

    return addr;
}
EXPORT_SYMBOL(devdrv_dma_zalloc_coherent);

void devdrv_dma_free_coherent(struct device *dev, size_t size, void *addr, dma_addr_t dma_addr)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if ((dev == NULL) || (addr == NULL) || (size == 0)) {
        devdrv_err("Input parameter is invalid. (size=%lu, dev is %s, addr is %s)\n",
            size, dev == NULL ? "null" : "not null", addr == NULL ? "null" : "not null");
        return;
    }
    pdev_ctrl = (struct devdrv_pdev_ctrl *)dev_get_drvdata(dev);
    if (pdev_ctrl == NULL) {
        devdrv_err("pdev_ctrl is invalid.\n");
        return;
    }

    pci_ctrl = pdev_ctrl->pci_ctrl[0];
    if (pci_ctrl == NULL) {
        devdrv_err("pci_ctrl is invalid.\n");
        return;
    }

    if (devdrv_is_alloc_pa_addr(pci_ctrl) == true) {
        devdrv_dma_unmap_single(dev, dma_addr, size, DMA_BIDIRECTIONAL);
        devdrv_kfree(addr);
    } else {
        if (devdrv_is_mdev_pm_boot_mode(pci_ctrl->dev_id) == true) {
#ifdef CFG_FEATURE_SRIOV
            hw_dvt_hypervisor_dma_free_coherent(dev, size, addr, dma_addr);
#endif
        } else {
            devdrv_ka_dma_free_coherent(dev, size, addr, dma_addr);
        }
    }
}
EXPORT_SYMBOL(devdrv_dma_free_coherent);

dma_addr_t devdrv_dma_map_single(struct device *dev, void *ptr, size_t size,
    enum dma_data_direction dir)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    dma_addr_t dma_addr = (~(dma_addr_t)0);
    dma_addr_t dma_tmp = (~(dma_addr_t)0);
    int ret;

    if ((dev == NULL) || (ptr == NULL) || (size == 0)) {
        devdrv_err("Input parameter is invalid. (size=%lu, dev is %s, ptr is %s)\n",
            size, dev == NULL ? "null" : "not null", ptr == NULL ? "null" : "not null");
        return (~(dma_addr_t)0);
    }
    pdev_ctrl = (struct devdrv_pdev_ctrl *)dev_get_drvdata(dev);
    if (pdev_ctrl == NULL) {
        devdrv_err("pdev_ctrl is invalid.\n");
        return (~(dma_addr_t)0);
    }

    pci_ctrl = pdev_ctrl->pci_ctrl[0];
    if (pci_ctrl == NULL) {
        devdrv_err("pci_ctrl is invalid.\n");
        return (~(dma_addr_t)0);
    }

    if (devdrv_is_alloc_pa_addr(pci_ctrl) == true) {
        /* hccs peh's Virtualization pass-through */
        if ((pci_ctrl->pdev->is_physfn == 0) && (pci_ctrl->pdev->is_virtfn == 0)) {
            dma_tmp = (dma_addr_t)virt_to_phys(ptr);
            ret = devdrv_smmu_iova_to_phys_proc(pci_ctrl, &dma_tmp, 1, &dma_addr);
            if (ret != 0) {
                return (~(dma_addr_t)0);
            }
        } else {
            dma_addr = (dma_addr_t)virt_to_phys(ptr);
        }

        if (dma_addr > DEVDRV_PEH_PHY_ADDR_MAX_VALUE) {
            devdrv_warn("dma_addr is invalid.(dev_id=%u)\n", pci_ctrl->dev_id);
#ifdef CFG_BUILD_DEBUG
            dump_stack();
#endif
            return (~(dma_addr_t)0);
        }
    } else {
        if (devdrv_is_mdev_pm_boot_mode(pci_ctrl->dev_id) == true) {
#ifdef CFG_FEATURE_SRIOV
            dma_addr = hw_dvt_hypervisor_dma_map_single(dev, ptr, size, dir);
#endif
        } else {
            dma_addr = dma_map_single(dev, ptr, size, dir);
        }
    }

    return dma_addr;
}
EXPORT_SYMBOL(devdrv_dma_map_single);

void devdrv_dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if ((dev == NULL) || (size == 0)) {
        devdrv_err_spinlock("Input parameter is invalid. (size=%lu)\n", size);
        return;
    }
    pdev_ctrl = (struct devdrv_pdev_ctrl *)dev_get_drvdata(dev);
    if (pdev_ctrl == NULL) {
        devdrv_err_spinlock("pdev_ctrl is invalid.\n");
        return;
    }

    pci_ctrl = pdev_ctrl->pci_ctrl[0];
    if (pci_ctrl == NULL) {
        devdrv_err_spinlock("pci_ctrl is invalid.\n");
        return;
    }

    if (devdrv_is_alloc_pa_addr(pci_ctrl) == true) {
        return;
    }

    if (devdrv_is_mdev_pm_boot_mode(pci_ctrl->dev_id) == true) {
#ifdef CFG_FEATURE_SRIOV
        hw_dvt_hypervisor_dma_unmap_single(dev, addr, size, dir);
#endif
    } else {
        dma_unmap_single(dev, addr, size, dir);
    }
}
EXPORT_SYMBOL(devdrv_dma_unmap_single);

dma_addr_t devdrv_dma_map_page(struct device *dev, struct page *page,
    size_t offset, size_t size, enum dma_data_direction dir)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    dma_addr_t dma_addr = (~(dma_addr_t)0);
    dma_addr_t dma_tmp = (~(dma_addr_t)0);
    int ret;

    if ((dev == NULL) || (page == NULL) || (size == 0) || (offset > HPAGE_SIZE)) {
        devdrv_err("Input parameter is invalid. (size=%lu, dev is %s, page is %s)\n",
            size, dev == NULL ? "null" : "not null", page == NULL ? "null" : "not null");
        return (~(dma_addr_t)0);
    }
    pdev_ctrl = (struct devdrv_pdev_ctrl *)dev_get_drvdata(dev);
    if (pdev_ctrl == NULL) {
        devdrv_err("pdev_ctrl is invalid.\n");
        return (~(dma_addr_t)0);
    }

    pci_ctrl = pdev_ctrl->pci_ctrl[0];
    if (pci_ctrl == NULL) {
        devdrv_err("pci_ctrl is invalid.\n");
        return (~(dma_addr_t)0);
    }

    if (devdrv_is_alloc_pa_addr(pci_ctrl) == true) {
        /* hccs peh's Virtualization pass-through */
        if ((pci_ctrl->pdev->is_physfn == 0) && (pci_ctrl->pdev->is_virtfn == 0)) {
            dma_tmp = (dma_addr_t)(page_to_phys(page) + offset);
            ret = devdrv_smmu_iova_to_phys_proc(pci_ctrl, &dma_tmp, 1, &dma_addr);
            if (ret != 0) {
                return (~(dma_addr_t)0);
            }
        } else {
            dma_addr = (dma_addr_t)(page_to_phys(page) + offset);
        }

        if (dma_addr > DEVDRV_PEH_PHY_ADDR_MAX_VALUE) {
            devdrv_warn("dma_addr is invalid.(dev_id=%u)\n", pci_ctrl->dev_id);
#ifdef CFG_BUILD_DEBUG
            dump_stack();
#endif
            return (~(dma_addr_t)0);
        }
    } else {
        if (devdrv_is_mdev_pm_boot_mode(pci_ctrl->dev_id) == true) {
#ifdef CFG_FEATURE_SRIOV
            dma_addr = hw_dvt_hypervisor_dma_map_page(dev, page, offset, size, dir);
#endif
        } else {
            dma_addr = dma_map_page(dev, page, offset, size, dir);
        }
    }

    return dma_addr;
}
EXPORT_SYMBOL(devdrv_dma_map_page);

void devdrv_dma_unmap_page(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (dev == NULL || size == 0) {
        devdrv_err("Input parameter is invalid. (size=%lu).\n", size);
        return;
    }
    pdev_ctrl = (struct devdrv_pdev_ctrl *)dev_get_drvdata(dev);
    if (pdev_ctrl == NULL) {
        devdrv_err("pdev_ctrl is invalid.\n");
        return;
    }

    pci_ctrl = pdev_ctrl->pci_ctrl[0];
    if (pci_ctrl == NULL) {
        devdrv_err("pci_ctrl is invalid.\n");
        return;
    }

    if (devdrv_is_alloc_pa_addr(pci_ctrl) == true) {
        return;
    }

    if (devdrv_is_mdev_pm_boot_mode(pci_ctrl->dev_id) == true) {
#ifdef CFG_FEATURE_SRIOV
        hw_dvt_hypervisor_dma_unmap_page(dev, addr, size, dir);
#endif
    } else {
        dma_unmap_page(dev, addr, size, dir);
    }
}
EXPORT_SYMBOL(devdrv_dma_unmap_page);

STATIC int devdrv_iommu_map_addr(struct devdrv_pci_ctrl *src_pci_ctrl, struct devdrv_pci_ctrl *dst_pci_ctrl,
    u64 addr, u64 size, u64 *dma_addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    struct iommu_domain *domain = NULL;
    dma_addr_t p_dma_addr;
    int ret;

    if (devdrv_is_alloc_pa_addr(dst_pci_ctrl) == true) {
        /* hccs peh's Virtualization pass-through, need convert by agent-smmu */
        if ((dst_pci_ctrl->pdev->is_physfn == 0) && (dst_pci_ctrl->pdev->is_virtfn == 0)) {
            ret = devdrv_smmu_iova_to_phys_proc(dst_pci_ctrl, &addr, 1, dma_addr);
            if (ret != 0) {
                devdrv_err("Call devdrv_smmu_iova_to_phys failed. (dev_id=%u)\n", dst_pci_ctrl->dev_id);
                return ret;
            }
        } else {
            *dma_addr = addr;
        }

        return 0;
    }

    domain = iommu_get_domain_for_dev(&src_pci_ctrl->pdev->dev);
    if (domain != NULL) {
        p_dma_addr = dma_map_resource(&src_pci_ctrl->pdev->dev, addr, size, DMA_BIDIRECTIONAL, 0);
        if (dma_mapping_error(&src_pci_ctrl->pdev->dev, p_dma_addr) != 0) {
            devdrv_err("dma_map_resource failed. (dev_id=%u; size=0x%llx)\n", src_pci_ctrl->dev_id, size);
            return -ENOMEM;
        }
        *dma_addr = p_dma_addr;
    } else {
        *dma_addr = addr;
    }
#else
    *dma_addr = addr;
#endif

    return 0;
}

STATIC void devdrv_iommu_unmap_addr(struct devdrv_pci_ctrl *pci_ctrl, u64 dma_addr, u64 size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    struct iommu_domain *domain = NULL;

    if (devdrv_is_alloc_pa_addr(pci_ctrl) == true) {
        return;
    }

    domain = iommu_get_domain_for_dev(&pci_ctrl->pdev->dev);
    if ((domain != NULL) && (dma_addr != 0)) {
        dma_unmap_resource(&pci_ctrl->pdev->dev, dma_addr, size, DMA_BIDIRECTIONAL, 0);
        devdrv_info("Iommu_unmap success. (dev_id=%u)\n", pci_ctrl->dev_id);
    }
#endif
    return;
}

STATIC int devdrv_add_p2p_msg_chan(struct devdrv_pci_ctrl *pci_ctrl_src, struct devdrv_pci_ctrl *pci_ctrl_dst)
{
    struct devdrv_msg_dev *msg_dev = pci_ctrl_src->msg_dev;
    struct devdrv_p2p_msg_chan_cfg_cmd cmd_data;
    int ret;
    u32 dst_dev = pci_ctrl_dst->dev_id;
    u64 db_dma_addr, db_size, msg_dma_addr, msg_size, phy_addr;

    phy_addr = (u64)pci_ctrl_dst->res.msg_db.addr;
    db_size = (u64)pci_ctrl_dst->res.msg_db.size;
    ret = devdrv_iommu_map_addr(pci_ctrl_src, pci_ctrl_dst, phy_addr, db_size, &db_dma_addr);
    if (ret != 0) {
        devdrv_err("Iommu map db mem failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        return ret;
    }

    phy_addr = (u64)pci_ctrl_dst->res.msg_mem.addr;
    msg_size = (u64)pci_ctrl_dst->res.msg_mem.size;
    ret = devdrv_iommu_map_addr(pci_ctrl_src, pci_ctrl_dst, phy_addr, msg_size, &msg_dma_addr);
    if (ret != 0) {
        devdrv_iommu_unmap_addr(pci_ctrl_src, db_dma_addr, db_size);
        devdrv_err("Iommu map msg mem failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        return ret;
    }

    pci_ctrl_src->shr_para->p2p_db_base_addr[dst_dev] = db_dma_addr;
    pci_ctrl_src->target_bar[dst_dev].db_dma_addr = db_dma_addr;
    pci_ctrl_src->target_bar[dst_dev].db_phy_size = db_size;

    pci_ctrl_src->shr_para->p2p_msg_base_addr[dst_dev] = msg_dma_addr;
    pci_ctrl_src->target_bar[dst_dev].msg_mem_dma_addr = msg_dma_addr;
    pci_ctrl_src->target_bar[dst_dev].msg_mem_phy_size = msg_size;

    cmd_data.op = DEVDRV_OP_ADD;
    cmd_data.devid = dst_dev;

    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_CFG_P2P_MSG_CHAN, &cmd_data, sizeof(cmd_data), NULL, 0);
    if (ret != 0) {
        devdrv_err("p2p msg chan cfg failed. (dev_id=%u; dst_dev_id=%d; ret=%d)\n", pci_ctrl_src->dev_id, dst_dev, ret);
        devdrv_iommu_unmap_addr(pci_ctrl_src, msg_dma_addr, msg_size);
        devdrv_iommu_unmap_addr(pci_ctrl_src, db_dma_addr, db_size);
    }

    return ret;
}

STATIC int devdrv_del_p2p_msg_chan(struct devdrv_pci_ctrl *pci_ctrl_src, u32 dst_dev)
{
    struct devdrv_msg_dev *msg_dev = pci_ctrl_src->msg_dev;
    struct devdrv_p2p_msg_chan_cfg_cmd cmd_data;
    int ret;
    u64 dma_addr;
    u64 size;

    cmd_data.op = DEVDRV_OP_DEL;
    cmd_data.devid = dst_dev;

    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_CFG_P2P_MSG_CHAN, &cmd_data, sizeof(cmd_data), NULL, 0);
    if (ret != 0) {
        devdrv_err("p2p msg chan cfg failed. (dev_id=%u; dst_dev_id=%d; ret=%d)\n", pci_ctrl_src->dev_id, dst_dev, ret);
    }

    dma_addr = pci_ctrl_src->target_bar[dst_dev].db_dma_addr;
    size = pci_ctrl_src->target_bar[dst_dev].db_phy_size;
    pci_ctrl_src->target_bar[dst_dev].db_dma_addr = 0;
    pci_ctrl_src->target_bar[dst_dev].db_phy_size = 0;
    devdrv_iommu_unmap_addr(pci_ctrl_src, dma_addr, size);

    dma_addr = pci_ctrl_src->target_bar[dst_dev].msg_mem_dma_addr;
    size = pci_ctrl_src->target_bar[dst_dev].msg_mem_phy_size;
    pci_ctrl_src->target_bar[dst_dev].msg_mem_dma_addr = 0;
    pci_ctrl_src->target_bar[dst_dev].msg_mem_phy_size = 0;
    devdrv_iommu_unmap_addr(pci_ctrl_src, dma_addr, size);

    return ret;
}

STATIC int devdrv_add_p2p_tx_atu(struct devdrv_pci_ctrl *pci_ctrl_src, struct devdrv_pci_ctrl *pci_ctrl_dst,
    u32 atu_type)
{
    struct devdrv_tx_atu_cfg_cmd cmd_data = {0};
    u32 dst_dev = pci_ctrl_dst->dev_id;
    int ret;

    if (atu_type == ATU_TYPE_TX_MEM) {
        cmd_data.phy_addr = (u64)pci_ctrl_dst->mem_phy_base;
        cmd_data.target_addr = pci_ctrl_src->target_bar[dst_dev].mem_dma_addr;
        cmd_data.target_size = pci_ctrl_src->target_bar[dst_dev].mem_phy_size;
    } else if (atu_type == ATU_TYPE_TX_IO) {
        cmd_data.phy_addr = (u64)pci_ctrl_dst->io_phy_base;
        cmd_data.target_addr = pci_ctrl_src->target_bar[dst_dev].io_dma_addr;
        cmd_data.target_size = pci_ctrl_src->target_bar[dst_dev].io_phy_size;
    } else {
        devdrv_err("Invaild atu_type. (dev_id=%u; type=%u)\n", pci_ctrl_src->dev_id, atu_type);
        return -EINVAL;
    }

    cmd_data.op = DEVDRV_OP_ADD;
    cmd_data.devid = dst_dev;
    cmd_data.atu_type = atu_type;
    ret = devdrv_admin_msg_chan_send(pci_ctrl_src->msg_dev, DEVDRV_CFG_TX_ATU, &cmd_data, sizeof(cmd_data),
        &cmd_data, sizeof(cmd_data));
    if (ret != 0) {
        devdrv_err("Config p2p tx_atu failed. (dev_id=%u; dst_dev_id=%d; type=%u; ret=%d)\n",
            pci_ctrl_src->dev_id, dst_dev, atu_type, ret);
        return ret;
    }

    if (atu_type == ATU_TYPE_TX_MEM) {
        pci_ctrl_src->target_bar[dst_dev].mem_txatu_base = cmd_data.atu_base_addr;
    } else if (atu_type == ATU_TYPE_TX_IO) {
        pci_ctrl_src->target_bar[dst_dev].io_txatu_base = cmd_data.atu_base_addr;
    }

    return 0;
}

STATIC int devdrv_del_p2p_tx_atu(struct devdrv_pci_ctrl *pci_ctrl_src, u32 dst_dev, u32 atu_type)
{
    struct devdrv_tx_atu_cfg_cmd cmd_data = {0};
    int ret;

    cmd_data.op = DEVDRV_OP_DEL;
    cmd_data.devid = dst_dev;
    cmd_data.atu_type = atu_type;
    ret = devdrv_admin_msg_chan_send(pci_ctrl_src->msg_dev, DEVDRV_CFG_TX_ATU, &cmd_data, sizeof(cmd_data), NULL, 0);
    if (ret != 0) {
        devdrv_err("Delete p2p txatu cfg failed. (dev_id=%u; dst_dev_id=%d; type=%u; ret=%d)\n",
            pci_ctrl_src->dev_id, dst_dev, atu_type, ret);
        return ret;
    }

    if (atu_type == ATU_TYPE_TX_MEM) {
        pci_ctrl_src->target_bar[dst_dev].mem_txatu_base = 0;
    } else if (atu_type == ATU_TYPE_TX_IO) {
        pci_ctrl_src->target_bar[dst_dev].io_txatu_base = 0;
    }

    return 0;
}

STATIC int devdrv_iommu_map_mem_addr(struct devdrv_pci_ctrl *pci_ctrl_src, struct devdrv_pci_ctrl *pci_ctrl_dst)
{
    u64 size = pci_ctrl_dst->mem_phy_size;
    u32 dst_dev = pci_ctrl_dst->dev_id;
    u64 dma_addr;
    int ret;

    ret = devdrv_iommu_map_addr(pci_ctrl_src, pci_ctrl_dst, (u64)pci_ctrl_dst->mem_phy_base, size, &dma_addr);
    if (ret != 0) {
        devdrv_err("Iommu map tx mem failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        return ret;
    }
    pci_ctrl_src->target_bar[dst_dev].mem_dma_addr = dma_addr;
    pci_ctrl_src->target_bar[dst_dev].mem_phy_size = size;

    return 0;
}

STATIC void devdrv_iommu_unmap_mem_addr(struct devdrv_pci_ctrl *pci_ctrl_src, u32 dst_dev)
{
    u64 dma_addr = pci_ctrl_src->target_bar[dst_dev].mem_dma_addr;
    u64 size = pci_ctrl_src->target_bar[dst_dev].mem_phy_size;

    pci_ctrl_src->target_bar[dst_dev].mem_dma_addr = 0;
    pci_ctrl_src->target_bar[dst_dev].mem_phy_size = 0;
    devdrv_iommu_unmap_addr(pci_ctrl_src, dma_addr, size);
}

STATIC void devdrv_iommu_unmap_io_addr(struct devdrv_pci_ctrl *pci_ctrl_src, u32 dst_dev)
{
    u64 dma_addr = pci_ctrl_src->target_bar[dst_dev].io_dma_addr;
    u64 size = pci_ctrl_src->target_bar[dst_dev].io_phy_size;

    pci_ctrl_src->target_bar[dst_dev].io_dma_addr = 0;
    pci_ctrl_src->target_bar[dst_dev].io_phy_size = 0;
    devdrv_iommu_unmap_addr(pci_ctrl_src, dma_addr, size);
}

STATIC int devdrv_iommu_map_io_addr(struct devdrv_pci_ctrl *pci_ctrl_src, struct devdrv_pci_ctrl *pci_ctrl_dst)
{
    u64 size = pci_ctrl_dst->io_phy_size;
    u32 dst_dev = pci_ctrl_dst->dev_id;
    u64 dma_addr;
    int ret;

    ret = devdrv_iommu_map_addr(pci_ctrl_src, pci_ctrl_dst, (u64)pci_ctrl_dst->io_phy_base, size, &dma_addr);
    if (ret != 0) {
        devdrv_err("Iommu map tx mem failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        return ret;
    }
    pci_ctrl_src->target_bar[dst_dev].io_dma_addr = dma_addr;
    pci_ctrl_src->target_bar[dst_dev].io_phy_size = size;

    return 0;
}

STATIC int devdrv_init_p2p_io_mem(struct devdrv_pci_ctrl *pci_ctrl_src, struct devdrv_pci_ctrl *pci_ctrl_dst)
{
    u32 dst_dev = pci_ctrl_dst->dev_id;
    int ret;

    /* hccs connect no need iommu map */
    if (pci_ctrl_src->connect_protocol == CONNECT_PROTOCOL_HCCS) {
        return 0;
    }

    ret = devdrv_iommu_map_mem_addr(pci_ctrl_src, pci_ctrl_dst);
    if (ret != 0) {
        devdrv_err("Iommu map tx mem failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        return ret;
    }

    ret = devdrv_iommu_map_io_addr(pci_ctrl_src, pci_ctrl_dst);
    if (ret != 0) {
        devdrv_err("Iommu map tx io failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        goto IO_MAP_FAIL;
    }

    /* super node no need cfg tx atu */
    if (pci_ctrl_src->addr_mode == DEVDRV_ADMODE_FULL_MATCH) {
        return 0;
    }

    ret = devdrv_add_p2p_tx_atu(pci_ctrl_src, pci_ctrl_dst, ATU_TYPE_TX_MEM);
    if (ret != 0) {
        devdrv_err("Add mem tx_atu failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        goto ADD_MEM_ATU_FAIL;
    }

    ret = devdrv_add_p2p_tx_atu(pci_ctrl_src, pci_ctrl_dst, ATU_TYPE_TX_IO);
    if (ret != 0) {
        devdrv_err("Add io tx_atu failed. (dev_id=%u)\n", pci_ctrl_src->dev_id);
        goto ADD_IO_ATU_FAIL;
    }

    return 0;

ADD_IO_ATU_FAIL:
    devdrv_del_p2p_tx_atu(pci_ctrl_src, dst_dev, ATU_TYPE_TX_MEM);

ADD_MEM_ATU_FAIL:
    devdrv_iommu_unmap_io_addr(pci_ctrl_src, dst_dev);

IO_MAP_FAIL:
    devdrv_iommu_unmap_mem_addr(pci_ctrl_src, dst_dev);

    return ret;
}

STATIC int devdrv_uninit_p2p_io_mem(struct devdrv_pci_ctrl *pci_ctrl_src, u32 dst_dev)
{
    int ret_mem, ret_io;

    /* hccs connect no need iommu map */
    if (pci_ctrl_src->connect_protocol == CONNECT_PROTOCOL_HCCS) {
        return 0;
    }

    devdrv_iommu_unmap_mem_addr(pci_ctrl_src, dst_dev);
    devdrv_iommu_unmap_io_addr(pci_ctrl_src, dst_dev);

    /* super node no need cfg tx atu */
    if (pci_ctrl_src->addr_mode == DEVDRV_ADMODE_FULL_MATCH) {
        return 0;
    }

    ret_mem = devdrv_del_p2p_tx_atu(pci_ctrl_src, dst_dev, ATU_TYPE_TX_MEM);
    ret_io = devdrv_del_p2p_tx_atu(pci_ctrl_src, dst_dev, ATU_TYPE_TX_IO);
    return ret_mem | ret_io;
}

STATIC int devdrv_p2p_cfg_enable(u32 dev_id, u32 peer_dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = g_ctrls[dev_id].priv;
    struct devdrv_pci_ctrl *pci_ctrl_peer = g_ctrls[peer_dev_id].priv;
    int ret;

    ret = devdrv_add_p2p_msg_chan(pci_ctrl, pci_ctrl_peer);
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_init_p2p_io_mem(pci_ctrl, pci_ctrl_peer);
    if (ret != 0) {
        devdrv_del_p2p_msg_chan(pci_ctrl, peer_dev_id);
        return ret;
    }

    ret = devdrv_add_p2p_msg_chan(pci_ctrl_peer, pci_ctrl);
    if (ret != 0) {
        devdrv_uninit_p2p_io_mem(pci_ctrl, peer_dev_id);
        devdrv_del_p2p_msg_chan(pci_ctrl, peer_dev_id);
        return ret;
    }

    ret = devdrv_init_p2p_io_mem(pci_ctrl_peer, pci_ctrl);
    if (ret != 0) {
        devdrv_del_p2p_msg_chan(pci_ctrl_peer, dev_id);
        devdrv_uninit_p2p_io_mem(pci_ctrl, peer_dev_id);
        devdrv_del_p2p_msg_chan(pci_ctrl, peer_dev_id);
        return ret;
    }

    return ret;
}

STATIC int devdrv_p2p_cfg_disable(u32 dev_id, u32 peer_dev_id)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl_peer = NULL;
    int ret = 0;

    /* Should support p2p cfg disable after a device hangs */
    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl != NULL) && (ctrl->priv != NULL)) {
        pci_ctrl = ctrl->priv;
        ret = devdrv_uninit_p2p_io_mem(pci_ctrl, peer_dev_id);
        ret += devdrv_del_p2p_msg_chan(pci_ctrl, peer_dev_id);
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(peer_dev_id);
    if ((ctrl != NULL) && (ctrl->priv != NULL)) {
        pci_ctrl_peer = ctrl->priv;
        ret += devdrv_uninit_p2p_io_mem(pci_ctrl_peer, dev_id);
        ret += devdrv_del_p2p_msg_chan(pci_ctrl_peer, dev_id);
    }

    return ret;
}

STATIC int devdrv_is_two_dev_on_same_chip(u32 dev_id, u32 peer_dev_id)
{
    u32 dev_func_num, peer_dev_func_num;
    u32 pf_id, peer_pf_id;
    u32 vf_id, peer_vf_id;
    int ret;

    dev_func_num = devdrv_get_total_func_num(dev_id);
    peer_dev_func_num = devdrv_get_total_func_num(peer_dev_id);
    if ((dev_func_num == 0) || (peer_dev_func_num == 0)) {
        devdrv_err("func_num is error. (dev_func_num=%u; peer_dev_func_num=%u)\n", dev_func_num, peer_dev_func_num);
        return -EINVAL;
    }

    /* 2DIe : 1PCIe 2PF */
    if ((dev_func_num == DEVDRV_MAX_FUNC_NUM) || (peer_dev_func_num == DEVDRV_MAX_FUNC_NUM)) {
        if ((dev_id / dev_func_num) == (peer_dev_id / peer_dev_func_num)) {
            devdrv_warn("dev_id and peer_dev_id is on the same chip. (dev_id=%d; peer_dev_id=%d)\n",
                dev_id, peer_dev_id);
            return -EPERM;
        }
    }

    ret = devdrv_get_pfvf_id_by_devid(dev_id, &pf_id, &vf_id);
    if (ret != 0) {
        devdrv_err("Get pfvf id failed. (dev_id=%u; peer_dev_id=%u)\n", dev_id, peer_dev_id);
        return -EINVAL;
    }

    ret = devdrv_get_pfvf_id_by_devid(peer_dev_id, &peer_pf_id, &peer_vf_id);
    if (ret != 0) {
        devdrv_err("Get peer pfvf id failed. (dev_id=%u; peer_dev_id=%u)\n", dev_id, peer_dev_id);
        return -EINVAL;
    }

    if (pf_id == peer_pf_id) {
        devdrv_err("dev_id and peer_dev_id is same. (dev_id=%u; peer_dev_id=%u)\n", dev_id, peer_dev_id);
        return -EINVAL;
    }

    if ((vf_id != 0) || (peer_vf_id != 0)) {
        devdrv_warn("Vf not support p2p. (vf_id=%u; peer_vf_id=%u)\n", vf_id, peer_vf_id);
        return -EINVAL;
    }

    return 0;
}

int devdrv_p2p_para_check(int pid, u32 dev_id, u32 peer_dev_id)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    struct devdrv_ctrl *dev_ctrl_peer = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl_peer = NULL;
    int devdrv_p2p_support_max_devnum;

    dev_ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    dev_ctrl_peer = devdrv_get_bottom_half_devctrl_by_id(peer_dev_id);
    if ((dev_ctrl == NULL) || (dev_ctrl->priv == NULL) ||
        (dev_ctrl_peer == NULL) || (dev_ctrl_peer->priv == NULL)) {
            devdrv_err("Device is not init. (dev_id=%u)\n", dev_id);
            return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
    pci_ctrl_peer = (struct devdrv_pci_ctrl *)dev_ctrl_peer->priv;

    devdrv_p2p_support_max_devnum = pci_ctrl->ops.get_p2p_support_max_devnum();
    if ((dev_id >= devdrv_p2p_support_max_devnum) || (peer_dev_id >= devdrv_p2p_support_max_devnum)) {
        devdrv_warn("Not support p2p. (pid=%d; dev_id=%d; peer_dev_id=%d; support_max_devnum=%d)\n",
            pid, dev_id, peer_dev_id, devdrv_p2p_support_max_devnum);
        return -EINVAL;
    }

    if (pci_ctrl->pdev == pci_ctrl_peer->pdev) {
        devdrv_warn("dev_id and peer_dev_id is on the same chip. (pid=%d; dev_id=%d; peer_dev_id=%d)\n",
            pid, dev_id, peer_dev_id);
        return -EINVAL;
    }

    if (devdrv_is_two_dev_on_same_chip(dev_id, peer_dev_id) != 0) {
        devdrv_warn("dev_id and peer_dev_id on same chip. (pid=%d; dev_id=%d; peer_dev_id=%d)\n",
            pid, dev_id, peer_dev_id);
        return -EINVAL;
    }

    return 0;
}

STATIC int devdrv_p2p_dev_valid_check(int pid, u32 dev_id, u32 peer_dev_id)
{
    struct devdrv_ctrl *ctrl = NULL;

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (pid=%d; dev_id=%d; peer_dev_id=%d)\n",
                   pid, dev_id, peer_dev_id);
        return -ENXIO;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(peer_dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (pid=%d; dev_id=%d; peer_dev_id=%d)\n",
                   pid, dev_id, peer_dev_id);
        return -ENXIO;
    }

    return 0;
}

STATIC struct devdrv_p2p_attr_info *devdrv_get_p2p_attr(u32 dev_id, u32 peer_dev_id)
{
    return &g_p2p_attr_info[dev_id][peer_dev_id];
}

STATIC int devdrv_get_p2p_attr_proc_id(const struct devdrv_p2p_attr_info *p2p_attr, int pid)
{
    int index, i;
    index = -1;

    for (i = 0; i < DEVDRV_P2P_MAX_PROC_NUM; i++) {
        if ((p2p_attr->proc_ref[i] > 0) && (p2p_attr->pid[i] == pid)) {
            index = i;
            break;
        }
    }

    return index;
}

STATIC int devdrv_get_idle_p2p_attr_proc_id(const struct devdrv_p2p_attr_info *p2p_attr)
{
    int index, i;
    index = -1;

    for (i = 0; i < DEVDRV_P2P_MAX_PROC_NUM; i++) {
        if (p2p_attr->proc_ref[i] == 0) {
            index = i;
            break;
        }
    }

    return index;
}

int devdrv_enable_p2p(int pid, u32 dev_id, u32 peer_dev_id)
{
    struct devdrv_p2p_attr_info *p2p_attr = NULL;
    struct devdrv_p2p_attr_info *p2p_peer_attr = NULL;
    int ret, index;

    ret = devdrv_p2p_para_check(pid, dev_id, peer_dev_id);
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_p2p_dev_valid_check(pid, dev_id, peer_dev_id);
    if (ret != 0) {
        devdrv_err("Device valid check failed. (pid=%d; dev_id=%d; peer_dev_id=%d)\n", pid, dev_id, peer_dev_id);
        return ret;
    }

    p2p_attr = devdrv_get_p2p_attr(dev_id, peer_dev_id);

    mutex_lock(&g_devdrv_p2p_mutex);

    index = devdrv_get_p2p_attr_proc_id(p2p_attr, pid);
    /* The process is configured for the first time */
    if (index < 0) {
        index = devdrv_get_idle_p2p_attr_proc_id(p2p_attr);
        if (index < 0) {
            devdrv_err("dev_id used up. (pid=%d; dev_id=%d; peer_dev_id=%d)\n", pid, dev_id, peer_dev_id);
            ret = -ENOMEM;
            goto OUT;
        }
        p2p_peer_attr = devdrv_get_p2p_attr(peer_dev_id, dev_id);
        /* First configuration, and peer allready configuration */
        if ((p2p_attr->ref == 0) && (p2p_peer_attr->ref > 0)) {
            /* Both directions are configured to take effect */
            ret = devdrv_p2p_cfg_enable(dev_id, peer_dev_id);
            if (ret != 0) {
                devdrv_err("Device configured failed. (pid=%d; dev_id=%d; peer_dev_id=%d)\n",
                           pid, dev_id, peer_dev_id);
                goto OUT;
            }

            devdrv_event("Enable p2p success. (pid=%d; dev_id=%d; peer_dev_id=%d)\n",
                         pid, dev_id, peer_dev_id);
        }
        p2p_attr->pid[index] = pid;
    }

    p2p_attr->proc_ref[index]++;
    p2p_attr->ref++;

OUT:
    mutex_unlock(&g_devdrv_p2p_mutex);

    return ret;
}
EXPORT_SYMBOL(devdrv_enable_p2p);

STATIC void devdrv_device_p2p_restore_ref(struct devdrv_p2p_attr_info *p2p_attr, int index, int pid)
{
    if (p2p_attr->proc_ref[index] == 0) {
        p2p_attr->pid[index] = pid;
    }
    p2p_attr->proc_ref[index]++;
    p2p_attr->ref++;
}

STATIC int devdrv_device_p2p_del_ref(struct devdrv_p2p_attr_info *p2p_attr, int index)
{
    int pid = 0;

    p2p_attr->proc_ref[index]--;
    p2p_attr->ref--;

    if (p2p_attr->proc_ref[index] == 0) {
        pid = p2p_attr->pid[index];
        p2p_attr->pid[index] = 0;
    }

    return pid;
}

int devdrv_disable_p2p(int pid, u32 dev_id, u32 peer_dev_id)
{
    struct devdrv_p2p_attr_info *p2p_attr = NULL;
    struct devdrv_p2p_attr_info *p2p_peer_attr = NULL;
    int ret, index;
    int pid_temp = 0;

    ret = devdrv_p2p_para_check(pid, dev_id, peer_dev_id);
    if (ret != 0) {
        devdrv_err("Parameter check failed. (pid=%d; dev_id=%d; peer_dev_id=%d)\n", pid, dev_id, peer_dev_id);
        return ret;
    }

    p2p_attr = devdrv_get_p2p_attr(dev_id, peer_dev_id);

    mutex_lock(&g_devdrv_p2p_mutex);

    index = devdrv_get_p2p_attr_proc_id(p2p_attr, pid);
    /* The process has not been configured with p2p */
    if (index < 0) {
        ret = -ESRCH;
        goto OUT;
    }

    if ((p2p_attr->proc_ref[index] <= 0) || (p2p_attr->ref <= 0)) {
        devdrv_err("p2p_attr is error. (dev_id=%d; peer_dev_id=%d; pid=%d; proc_ref=%d; total_ref=%d)\n",
            pid, dev_id, peer_dev_id, p2p_attr->proc_ref[index], p2p_attr->ref);
        ret = -ESRCH;
        goto OUT;
    }

    pid_temp = devdrv_device_p2p_del_ref(p2p_attr, index);
    /* All configurations are canceled, delete tx atu */
    if (p2p_attr->ref == 0) {
        /* peer not yet cancel configuration */
        p2p_peer_attr = devdrv_get_p2p_attr(peer_dev_id, dev_id);
        if (p2p_peer_attr->ref > 0) {
            ret = devdrv_p2p_cfg_disable(dev_id, peer_dev_id);
            if (ret != 0) {
                devdrv_device_p2p_restore_ref(p2p_attr, index, pid_temp);
                devdrv_err("Disable p2p failed, rollback. (pid=%d; dev_id=%d; peer_dev_id=%d; proc_ref=%d; pid=%d; "
                    "ref=%d; peer_ref=%d)\n", pid, dev_id, peer_dev_id, p2p_attr->proc_ref[index], p2p_attr->pid[index],
                    p2p_attr->ref, p2p_peer_attr->ref);
            } else {
                devdrv_event("Disable p2p success. (pid=%d; dev_id=%d; peer_dev_id=%d)\n", pid, dev_id, peer_dev_id);
            }
        }
    }

OUT:
    mutex_unlock(&g_devdrv_p2p_mutex);

    return ret;
}
EXPORT_SYMBOL(devdrv_disable_p2p);

bool devdrv_is_p2p_enabled(u32 dev_id, u32 peer_dev_id)
{
    struct devdrv_p2p_attr_info *p2p_attr = NULL;
    struct devdrv_p2p_attr_info *p2p_peer_attr = NULL;
    int ret;

    ret = devdrv_p2p_para_check(0, dev_id, peer_dev_id);
    if (ret != 0) {
        return false;
    }

    p2p_attr = devdrv_get_p2p_attr(dev_id, peer_dev_id);
    p2p_peer_attr = devdrv_get_p2p_attr(peer_dev_id, dev_id);

    return ((p2p_attr->ref > 0) && (p2p_peer_attr->ref > 0));
}
EXPORT_SYMBOL(devdrv_is_p2p_enabled);

/* Restore the configuration of the process when the process exits */
void devdrv_flush_p2p(int pid)
{
    struct devdrv_p2p_attr_info *p2p_attr = NULL;
    int num, index;
    u32 i, j;

    for (i = 0; i < DEVDRV_P2P_SUPPORT_MAX_DEVICE; i++) {
        for (j = 0; j < DEVDRV_P2P_SUPPORT_MAX_DEVICE; j++) {
            if (i == j) {
                continue;
            }

            /* get pid attr */
            p2p_attr = devdrv_get_p2p_attr(i, j);
            index = devdrv_get_p2p_attr_proc_id(p2p_attr, pid);
            if (index < 0) {
                continue;
            }
            /* pid has config p2p */
            num = 0;
            while (devdrv_disable_p2p(pid, i, j) == 0) {
                num++;
            }

            if (num > 0) {
                devdrv_event("Get devdrv_disable_p2p dev_num. (dev_id=%d; peer_dev_id=%d; pid=%d; recyle=%d)\n",
                             i, j, pid, num);
            }
        }
    }
}
EXPORT_SYMBOL(devdrv_flush_p2p);

int devdrv_get_p2p_capability(u32 dev_id, u64 *capability)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *dev_ctrl = NULL;
    int vndr;
    u32 val[DEVDRV_MAX_CAPABILITY_VALUE_NUM] = {0};
    int ret, i;

    if ((dev_id >= MAX_DEV_CNT) || (capability == NULL)) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    dev_ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((dev_ctrl == NULL) || (dev_ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
    vndr = pci_find_capability(pci_ctrl->pdev, PCI_CAP_ID_VNDR);
    if (vndr == 0) {
        devdrv_err("Get capability failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    for (i = 0; i < DEVDRV_MAX_CAPABILITY_VALUE_NUM; i++) {
        ret = pci_bus_read_config_dword(pci_ctrl->pdev->bus, pci_ctrl->pdev->devfn,
            vndr + i * DEVDRV_MAX_CAPABILITY_VALUE_OFFSET, &val[i]);
        if (ret != 0) {
            devdrv_err("Get config failed. (dev_id=%u; i=%d; ret=%d)\n", dev_id, i, ret);
            return ret;
        }
    }

    *capability = ((u64)val[1] << DEVDRV_P2P_CAPABILITY_SHIFT_32) | val[0];

    return 0;
}
EXPORT_SYMBOL(devdrv_get_p2p_capability);

STATIC bool devdrv_pci_topo_support_p2p(const struct pci_dev *upper, const struct pci_dev *peer_upper)
{
    if (upper->bus->self == NULL || peer_upper->bus->self == NULL) {
        devdrv_info("One or both device under root port, not support p2p.\n");
        return false;
    }

    if (upper->bus->self == peer_upper->bus->self) {
        devdrv_info("Both device under same switch, support p2p.\n");
        return true;
    }

    return false;
}

STATIC int devdrv_get_p2p_access_status_physical(u32 devid, u32 peer_devid, int *status)
{
    struct devdrv_pci_ctrl *peer_pci_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;
    int p2p_status;

    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -ENXIO;
    }
    pci_ctrl = ctrl->priv;

    ctrl = devdrv_get_bottom_half_devctrl_by_id(peer_devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (peer_devid=%u)\n", peer_devid);
        return -ENXIO;
    }
    peer_pci_ctrl = ctrl->priv;

    if (pci_ctrl->ops.is_p2p_access_cap != NULL) {
        p2p_status = pci_ctrl->ops.is_p2p_access_cap(pci_ctrl, peer_pci_ctrl);
        if (p2p_status != DEVDRV_P2P_ACCESS_UNKNOWN) {
            *status = p2p_status;
            return 0;
        }
    }

    if (devdrv_pci_topo_support_p2p(pci_ctrl->pdev->bus->self, peer_pci_ctrl->pdev->bus->self)) {
        *status = DEVDRV_P2P_ACCESS_ENABLE;
        devdrv_info("Topo support p2p. (dev_id=%u; peer_devid=%u)\n", devid, peer_devid);
    } else {
        *status = DEVDRV_P2P_ACCESS_DISABLE;
        devdrv_info("Topo not support p2p. (dev_id=%u; peer_devid=%u)\n", devid, peer_devid);
    }

    return 0;
}

STATIC int devdrv_get_p2p_access_status_virtual(u32 devid, u32 peer_devid, int *status)
{
    struct devdrv_pci_ctrl *peer_pci_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u64 capability, peer_capability;
    u64 group, peer_group;
    u64 sign, peer_sign;
    int p2p_status;
    int ret;

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return -ENXIO;
    }

    peer_pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(peer_devid);
    if (peer_pci_ctrl == NULL) {
        devdrv_err("Get peer_pci_ctrl failed. (peer_devid=%u)\n", peer_devid);
        return -ENXIO;
    }

    if (pci_ctrl->ops.is_p2p_access_cap != NULL) {
        p2p_status = pci_ctrl->ops.is_p2p_access_cap(pci_ctrl, peer_pci_ctrl);
        if (p2p_status != DEVDRV_P2P_ACCESS_UNKNOWN) {
            *status = p2p_status;
            return 0;
        }
    }

    ret = devdrv_get_p2p_capability(devid, &capability);
    if (ret != 0) {
        devdrv_err("Get p2p capability failed. (dev_id=%d; ret=%d)\n", devid, ret);
        return ret;
    }

    ret = devdrv_get_p2p_capability(peer_devid, &peer_capability);
    if (ret != 0) {
        devdrv_err("Get p2p capability failed. (peer_devid=%d; ret=%d)\n", peer_devid, ret);
        return ret;
    }

    sign = (capability >> DEVDRV_P2P_CAPA_SIGN_BIT) & DEVDRV_P2P_CAPA_SIGN_VAL;
    group = (capability >> DEVDRV_P2P_GROUP_SIGN_BIT) & DEVDRV_P2P_GROUP_SIGN_VAL;
    peer_sign = (peer_capability >> DEVDRV_P2P_CAPA_SIGN_BIT) & DEVDRV_P2P_CAPA_SIGN_VAL;
    peer_group = (peer_capability >> DEVDRV_P2P_GROUP_SIGN_BIT) & DEVDRV_P2P_GROUP_SIGN_VAL;

    if ((sign != DEVDRV_VIRT_MACH_SIGN) || (peer_sign != DEVDRV_VIRT_MACH_SIGN)) {
        *status = DEVDRV_P2P_ACCESS_DISABLE;
        devdrv_info("Not virt mach signature. (dev_id=%u; sign=%llx; peer_dev=%u; sign=%llx)\n",
                    devid, sign, peer_devid, peer_sign);
        return 0;
    }

    if (group != peer_group) {
        *status = DEVDRV_P2P_ACCESS_DISABLE;
        devdrv_info("Different group. (dev_id=%u; group=%llu; peer_dev=%u; group=%llu)\n",
                    devid, group, peer_devid, peer_group);
        return 0;
    }

    *status = DEVDRV_P2P_ACCESS_ENABLE;
    return 0;
}

int devdrv_get_p2p_access_status(u32 devid, u32 peer_devid, int *status)
{
    unsigned int host_flag;
    int ret;

    if ((devid >= MAX_DEV_CNT) || (peer_devid >= MAX_DEV_CNT) || (status == NULL) || (devid == peer_devid)) {
        devdrv_err("Input parameter is invalid. (dev_id=%d; peer_dev_id=%d)\n", devid, peer_devid);
        return -EINVAL;
    }

    ret = devdrv_get_host_phy_mach_flag(devid, &host_flag);
    if (ret != 0) {
        devdrv_err("Get phy mach flag failed. (dev_id=%d; ret=%d)\n", devid, ret);
        return ret;
    }

    if (host_flag == DEVDRV_HOST_PHY_MACH_FLAG) {
        ret = devdrv_get_p2p_access_status_physical(devid, peer_devid, status);
        if (ret != 0) {
            devdrv_err("Physic mach get p2p access failed. (dev_id=%d; peer_dev_id=%d; ret=%d)\n",
                       devid, peer_devid, ret);
            return ret;
        }
    } else {
        ret = devdrv_get_p2p_access_status_virtual(devid, peer_devid, status);
        if (ret != 0) {
            devdrv_err("Virtual mach get p2p access failed. (dev_id=%d; peer_dev_id=%d; ret=%d)\n",
                       devid, peer_devid, ret);
            return ret;
        }
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_get_p2p_access_status);

void devdrv_clear_p2p_resource(u32 devid)
{
    struct devdrv_p2p_attr_info *p2p_attr = NULL;
    u32 i, j;

    for (i = 0; i < DEVDRV_P2P_SUPPORT_MAX_DEVICE; i++) {
        for (j = 0; j < DEVDRV_P2P_SUPPORT_MAX_DEVICE; j++) {
            if (i == j) {
                continue;
            }

            if ((i == devid) || (j == devid)) {
                p2p_attr = devdrv_get_p2p_attr(i, j);
                mutex_lock(&g_devdrv_p2p_mutex);
                (void)memset_s((void *)p2p_attr, sizeof(struct devdrv_p2p_attr_info), 0,
                    sizeof(struct devdrv_p2p_attr_info));
                mutex_unlock(&g_devdrv_p2p_mutex);
            }
        }
    }
}

int devdrv_devmem_addr_bar_to_dma(u32 devid, u32 dst_devid, phys_addr_t host_bar_addr, dma_addr_t *dma_addr)
{
    int ret;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl_dst = NULL;

    if (dma_addr == NULL) {
        devdrv_err("Input parameter dma_addr is null. (dev_id=%d; peer_dev_id=%d)\n", devid, dst_devid);
        return -EINVAL;
    }

    ret = devdrv_p2p_dev_valid_check(0, devid, dst_devid);
    if (ret != 0) {
        devdrv_err("Dev ctrl check failed. (dev_id=%d; peer_dev_id=%d)\n", devid, dst_devid);
        return ret;
    }

    ret = devdrv_p2p_para_check(0, devid, dst_devid);
    if (ret != 0) {
        devdrv_warn("Src_dev and dst_dev not support p2p. (dev_id=%d; peer_dev_id=%d)\n", devid, dst_devid);
        return -EPERM;
    }

    pci_ctrl = g_ctrls[devid].priv;
    pci_ctrl_dst = g_ctrls[dst_devid].priv;

    if ((pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) ||
        (pci_ctrl_dst->connect_protocol == CONNECT_PROTOCOL_HCCS)) {
        *dma_addr = host_bar_addr;
        return 0;
    }

    if ((pci_ctrl_dst->mem_phy_base <= host_bar_addr) &&
        ((pci_ctrl_dst->mem_phy_base + pci_ctrl_dst->mem_phy_size) > host_bar_addr)) {
        /* iommu has maped */
        if (pci_ctrl->target_bar[dst_devid].mem_phy_size == pci_ctrl_dst->mem_phy_size) {
            *dma_addr = host_bar_addr - pci_ctrl_dst->mem_phy_base + pci_ctrl->target_bar[dst_devid].mem_dma_addr;
            return 0;
        }
    }

    devdrv_warn("May not enable p2p, please check. (dev_id=%d; peer_dev_id=%d; enable status=%d)\n",
        devid, dst_devid, devdrv_is_p2p_enabled(devid, dst_devid) == true ? 1 : 0);
    return -EINVAL;
}
EXPORT_SYMBOL(devdrv_devmem_addr_bar_to_dma);

static struct devdrv_pci_ctrl *devdrv_get_pci_ctrl_and_prepare_for_hotreset(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_pci_ctrl_get_no_ref(dev_id);
    if (pci_ctrl != NULL) {
        devdrv_pci_ctrl_del_wait(dev_id, pci_ctrl);
    }
    return pci_ctrl;
}

int devdrv_pcie_prereset(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 dev_id_tmp = dev_id;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_pci_ctrl_and_prepare_for_hotreset(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Device is not init. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    mutex_lock(&g_devdrv_ctrl_mutex);
    if (dev_id < MAX_DEV_CNT) {
        if (((g_devdrv_ctrl_hot_reset_status >> dev_id_tmp) & 1U) == 1) {
            mutex_unlock(&g_devdrv_ctrl_mutex);
            devdrv_err("devdrv_pcie_prereset, but device on process. (dev_id=%u; dev_mask=%llu)\n",
                dev_id, g_devdrv_ctrl_hot_reset_status);
            return -EAGAIN;
        } else {
            g_devdrv_ctrl_hot_reset_status |= (1ULL << dev_id_tmp);
        }
    }
    mutex_unlock(&g_devdrv_ctrl_mutex);

    devdrv_info("Call devdrv_pcie_prereset start. (dev_id=%u)n", dev_id);
    devdrv_pci_stop_and_remove_bus_device_locked(pci_ctrl->pdev);

    mutex_lock(&g_devdrv_ctrl_mutex);
    if (dev_id == 0xff) {
        g_devdrv_ctrl_hot_reset_status = 0;
    } else {
        g_devdrv_ctrl_hot_reset_status &= ~(1ULL << dev_id_tmp);
    }
    mutex_unlock(&g_devdrv_ctrl_mutex);

    return 0;
}
EXPORT_SYMBOL(devdrv_pcie_prereset);

int devdrv_get_bar_wc_flag(u32 devid, u32 *value)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input devid is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (value == NULL) {
        devdrv_err("Input value is null. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    pci_ctrl = ctrl->priv;

    *value = pci_ctrl->bar_wc_flag;

    return 0;
}
EXPORT_SYMBOL(devdrv_get_bar_wc_flag);

void devdrv_set_bar_wc_flag(u32 devid, u32 value)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return;
    }
    pci_ctrl = ctrl->priv;

    pci_ctrl->shr_para->bar_wc_flag = value;
    pci_ctrl->bar_wc_flag = pci_ctrl->shr_para->bar_wc_flag;
    return;
}
EXPORT_SYMBOL(devdrv_set_bar_wc_flag);

int devdrv_get_host_phy_mach_flag(u32 devid, u32 *host_flag)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;
    void __iomem *base = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (host_flag == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    pci_ctrl = ctrl->priv;

    base = pci_ctrl->res.phy_match_flag_addr;
    *host_flag = readl(base);

    return 0;
}
EXPORT_SYMBOL(devdrv_get_host_phy_mach_flag);

/* set host flag */
void devdrv_set_host_phy_mach_flag(u32 devid, u32 host_flag)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;
    void __iomem *base = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return;
    }
    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return;
    }

    pci_ctrl = ctrl->priv;

    base = pci_ctrl->res.phy_match_flag_addr;
    writel(host_flag, base);

    return;
}
EXPORT_SYMBOL(devdrv_set_host_phy_mach_flag);

STATIC int devdrv_get_h2d_target_addr_index(u32 devid, dma_addr_t host_dma_addr)
{
    int i;
    for (i = 0; i < DEVDRV_H2D_MAX_ATU_NUM; i++) {
        if ((g_h2d_attr_info[devid][i].valid == 1) && (g_h2d_attr_info[devid][i].addr == host_dma_addr)) {
            return i;
        }
    }
    return -EINVAL;
}

STATIC int devdrv_get_idle_h2d_target_addr_index(u32 devid)
{
    int i;

    for (i = 0; i < DEVDRV_H2D_MAX_ATU_NUM; i++) {
        if (g_h2d_attr_info[devid][i].valid == 0) {
            return i;
        }
    }
    return -EINVAL;
}

STATIC struct devdrv_h2d_attr_info *devdrv_get_h2d_attr(u32 devid, dma_addr_t host_dma_addr)
{
    int index = devdrv_get_h2d_target_addr_index(devid, host_dma_addr);
    if (index < 0) {
        index = devdrv_get_idle_h2d_target_addr_index(devid);
        if (index < 0) {
            devdrv_err("Space used up. (dev_id=%u)\n", devid);
            return NULL;
        }
    }

    return &g_h2d_attr_info[devid][index];
}

STATIC int devdrv_get_h2d_attr_proc_id(const struct devdrv_h2d_attr_info *h2d_attr, int pid)
{
    int index, i;
    index = -1;

    for (i = 0; i < DEVDRV_H2D_MAX_PROC_NUM; i++) {
        if ((h2d_attr->proc_ref[i] > 0) && (h2d_attr->pid[i] == pid)) {
            index = i;
            break;
        }
    }

    return index;
}

STATIC int devdrv_get_idle_h2d_attr_proc_id(const struct devdrv_h2d_attr_info *h2d_attr)
{
    int index, i;
    index = -1;

    for (i = 0; i < DEVDRV_H2D_MAX_PROC_NUM; i++) {
        if (h2d_attr->proc_ref[i] == 0) {
            index = i;
            break;
        }
    }

    return index;
}

STATIC int devdrv_device_txatu_cfg_get_index(struct devdrv_h2d_attr_info *h2d_attr, int pid, u32 devid)
{
    int index;

    index = devdrv_get_h2d_attr_proc_id(h2d_attr, pid);
    /* The process is configured for the first time */
    if (index < 0) {
        index = devdrv_get_idle_h2d_attr_proc_id(h2d_attr);
        if (index < 0) {
            devdrv_err("Device id used up. (pid=%d; dev_id=%d)\n", pid, devid);
            return index;
        }
        h2d_attr->pid[index] = pid;
    }

    return index;
}

STATIC int devdrv_device_txatu_cfg_msg_proc(struct devdrv_msg_dev *msg_dev, dma_addr_t host_dma_addr, u64 size)
{
    struct devdrv_tx_atu_cfg_cmd cmd_data;
    int ret;

    cmd_data.op = DEVDRV_OP_ADD;
    cmd_data.devid = (u32)-1;
    cmd_data.atu_type = ATU_TYPE_TX_HOST;
    cmd_data.phy_addr = (u64)host_dma_addr;
    cmd_data.target_addr = (u64)host_dma_addr;
    cmd_data.target_size = size;

    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_CFG_TX_ATU, &cmd_data, sizeof(cmd_data), NULL, 0);

    return ret;
}

STATIC void devdrv_device_txatu_cfg_ref_proc(struct devdrv_h2d_attr_info *h2d_attr, int index)
{
    h2d_attr->proc_ref[index]++;
    h2d_attr->ref++;
}

/* one device only support a txatu item, repeat configuration will overwrite previous
   devid: host devid, size <= 128MB */
int devdrv_device_txatu_config(int pid, u32 devid, dma_addr_t host_dma_addr, u64 size)
{
    struct devdrv_ctrl *ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    struct devdrv_h2d_attr_info *h2d_attr = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret = 0;
    int index;

    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (pid=%d; dev_id=%u)\n", pid, devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        devdrv_warn("Vf not support tx atu. (pid=%d; dev_id=%u)\n", pid, devid);
        return -EOPNOTSUPP;
    }

    if ((size == 0) || ((size % ATU_SIZE_ALIGN) != 0)) {
        devdrv_err("Size is illegal. (pid=%d; dev_id=%u; size=0x%llx)\n", pid, devid, size);
        return -EINVAL;
    }

    mutex_lock(&g_devdrv_p2p_mutex);
    h2d_attr = devdrv_get_h2d_attr(devid, host_dma_addr);
    if (h2d_attr == NULL) {
        devdrv_err("No space atu. (pid=%d; dev_id=%u)\n", pid, devid);
        ret = -EINVAL;
        goto OUT;
    }
    index = devdrv_device_txatu_cfg_get_index(h2d_attr, pid, devid);
    if (index < 0) {
        devdrv_err("Get index error. (pid=%d; dev_id=%u)\n", pid, devid);
        ret = -ENOMEM;
        goto OUT;
    }
    if (h2d_attr->valid == DEVDRV_H2D_ATTR_INVALID) {
        ret = devdrv_device_txatu_cfg_msg_proc(pci_ctrl->msg_dev, host_dma_addr, size);
        if (ret != 0) {
            devdrv_err("p2p send cfg msg failed. (pid=%d; dev_id=%u; ret=%d)\n", pid, devid, ret);
            goto OUT;
        }
        h2d_attr->addr = host_dma_addr;
        h2d_attr->size = size;
        h2d_attr->valid = DEVDRV_H2D_ATTR_VALID;
        devdrv_event("Enable h2d success. (pid=%d; dev_id=%u)\n", pid, devid);
    }

    devdrv_device_txatu_cfg_ref_proc(h2d_attr, index);

    devdrv_info("Device txatu config ok. (pid=%d; dev_id=%d; ref=%d; total_ref=%d)\n",
                pid, devid, h2d_attr->proc_ref[index], h2d_attr->ref);
OUT:
    mutex_unlock(&g_devdrv_p2p_mutex);
    return ret;
}
EXPORT_SYMBOL(devdrv_device_txatu_config);

STATIC void devdrv_device_txatu_del_ref_proc(struct devdrv_h2d_attr_info *h2d_attr, int index)
{
    h2d_attr->proc_ref[index]--;
    h2d_attr->ref--;
    if (h2d_attr->proc_ref[index] == 0) {
        h2d_attr->pid[index] = 0;
    }
}

STATIC int devdrv_device_txatu_del_msg_proc(struct devdrv_msg_dev *msg_dev, dma_addr_t host_dma_addr)
{
    struct devdrv_tx_atu_cfg_cmd cmd_data;
    int ret;

    cmd_data.op = DEVDRV_OP_DEL;
    cmd_data.devid = (u32)-1;
    cmd_data.atu_type = ATU_TYPE_TX_HOST;
    cmd_data.phy_addr = (u64)host_dma_addr;
    cmd_data.target_addr = (u64)host_dma_addr;
    cmd_data.target_size = 0;

    ret = devdrv_admin_msg_chan_send(msg_dev, DEVDRV_CFG_TX_ATU, &cmd_data, sizeof(cmd_data), NULL, 0);

    return ret;
}

int devdrv_device_txatu_del(int pid, u32 devid, dma_addr_t host_dma_addr)
{
    struct devdrv_ctrl *ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    struct devdrv_h2d_attr_info *h2d_attr = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret = 0;
    int index;

    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get ctrl failed. (pid=%d; dev_id=%u)\n", pid, devid);
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    mutex_lock(&g_devdrv_p2p_mutex);
    index = devdrv_get_h2d_target_addr_index(devid, host_dma_addr);
    if (index < 0) {
        ret = -ESRCH;
        goto OUT;
    }
    h2d_attr = &g_h2d_attr_info[devid][index];

    index = devdrv_get_h2d_attr_proc_id(h2d_attr, pid);
    /* The process is configured for the first time */
    if (index < 0) {
        ret = -ESRCH;
        goto OUT;
    }

    if ((h2d_attr->ref <= 0) || (h2d_attr->proc_ref[index] <= 0)) {
        devdrv_err("h2d_attr information is error. (pid=%d; dev_id=%d; proc_ref=%d; total_ref=%d)\n",
            pid, devid, h2d_attr->proc_ref[index], h2d_attr->ref);
        ret = -ESRCH;
        goto OUT;
    }
    devdrv_device_txatu_del_ref_proc(h2d_attr, index);
    if (h2d_attr->ref == 0) {
        ret = devdrv_device_txatu_del_msg_proc(pci_ctrl->msg_dev, host_dma_addr);
        if (ret != 0) {
            devdrv_warn("Delete proc failed. (pid=%d; dev_id=%u; ret=%d)\n", pid, devid, ret);
        } else {
            devdrv_event("Disable h2d success. (pid=%d; dev_id=%u)\n", pid, devid);
        }
        h2d_attr->valid = 0;
    }

    devdrv_info("Device txatu delete ok. (pid=%d; dev_id=%d; proc_ref=%d; total_ref=%d)\n",
                pid, devid, h2d_attr->proc_ref[index], h2d_attr->ref);
OUT:
    mutex_unlock(&g_devdrv_p2p_mutex);
    return ret;
}
EXPORT_SYMBOL(devdrv_device_txatu_del);

void devdrv_clear_h2d_txatu_resource(u32 devid)
{
    int i;

    for (i = 0; i < DEVDRV_H2D_MAX_ATU_NUM; i++) {
        if (g_h2d_attr_info[devid][i].valid == DEVDRV_H2D_ATTR_VALID) {
            mutex_lock(&g_devdrv_p2p_mutex);
            (void)memset_s((void *)(&g_h2d_attr_info[devid][i]), sizeof(struct devdrv_h2d_attr_info), 0,
                sizeof(struct devdrv_h2d_attr_info));
            mutex_unlock(&g_devdrv_p2p_mutex);
        }
    }
}

/* This function called by host to get device index with host device id
 * on cloud,  an smp system has 4 chips, so the device index is 0 ~ 3
 * on mini, device index is 0
 */
int devdrv_get_device_index(u32 host_dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    ctrl = devdrv_get_bottom_half_devctrl_by_id(host_dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (host_dev_id=%u)\n", host_dev_id);
        return -EINVAL;
    }

    pci_ctrl = ctrl->priv;

    return (int)pci_ctrl->remote_dev_id;
}
EXPORT_SYMBOL(devdrv_get_device_index);

bool devdrv_get_device_boot_finish(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return false;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_warn("Dev ctrl not probe finish now. (dev_id=%u)\n", dev_id);
        return false;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;

    return (pci_ctrl->startup_status.status == DEVDRV_STARTUP_STATUS_FINISH);
}
EXPORT_SYMBOL(devdrv_get_device_boot_finish);

bool devdrv_check_half_probe_finish(u32 dev_id)
{
    struct devdrv_ctrl *ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return false;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        return false;
    }

    return true;
}
EXPORT_SYMBOL(devdrv_check_half_probe_finish);

STATIC void devdrv_set_device_boot_finish(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_startup_status *startup_status = &(pci_ctrl->startup_status);
    u32 module_init_finish = 0;

    if (startup_status->status != DEVDRV_STARTUP_STATUS_FINISH) {
        return;
    }

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_PM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT)) {
        module_init_finish = 1;
    } else if ((pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) &&
        ((startup_status->module_bit_map & DEVDRV_HOST_VF_MODULE_MASK) == DEVDRV_HOST_VF_MODULE_MASK)) {
        module_init_finish = 1;
    } else if ((pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_PF) &&
        ((startup_status->module_bit_map & DEVDRV_HOST_MODULE_MASK) == DEVDRV_HOST_MODULE_MASK)) {
        module_init_finish = 1;
    } else {
        module_init_finish = 0;
    }

    if (module_init_finish == 1) {
        /* set device boot status:boot finish */
        devdrv_set_device_boot_status(pci_ctrl, DSMI_BOOT_STATUS_FINISH);
        devdrv_info("Set device boot finish. (dev_id=%u)\n", pci_ctrl->dev_id);
    }
}

void devdrv_set_startup_status(struct devdrv_pci_ctrl *pci_ctrl, int status)
{
    struct devdrv_startup_status *startup_status = &(pci_ctrl->startup_status);

    if (status == DEVDRV_STARTUP_STATUS_INIT) {
        startup_status->module_bit_map = 0;
        devdrv_info("Startup status init. (dev_id=%u; jiffies=%ld)\n", pci_ctrl->dev_id, jiffies);
    } else {
        devdrv_info("Set new status. (dev_id=%u; pre_status=%d; current=%d; use_time=%dms)\n",
                    pci_ctrl->dev_id, startup_status->status, status,
                    jiffies_to_msecs(jiffies - startup_status->timestamp));
    }

    startup_status->status = status;
    startup_status->timestamp = jiffies;

    devdrv_set_device_boot_finish(pci_ctrl);
}

int devdrv_set_module_init_finish(int dev_id, int module)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_startup_status *startup_status = NULL;

    ctrl = devdrv_get_bottom_half_devctrl_by_id((u32)dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    pci_ctrl = ctrl->priv;
    startup_status = &(pci_ctrl->startup_status);

    mutex_lock(&g_devdrv_ctrl_mutex);

    startup_status->module_bit_map |= (0x1UL << (u32)module);

    devdrv_set_device_boot_finish(pci_ctrl);

    mutex_unlock(&g_devdrv_ctrl_mutex);

    devdrv_info("Module init finish. (dev_id=%d; module=%d)\n", dev_id, module);

    return 0;
}
EXPORT_SYMBOL(devdrv_set_module_init_finish);

STATIC bool devdrv_can_hotreset(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_startup_status *startup_status = NULL;
    bool ret = false;

    mutex_lock(&g_devdrv_ctrl_mutex);
    startup_status = &(pci_ctrl->startup_status);

    if (startup_status->status == DEVDRV_STARTUP_STATUS_FINISH) {
        if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_PM_BOOT) ||
            (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT)) {
            ret = true;
        } else if ((pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) &&
            ((startup_status->module_bit_map & DEVDRV_HOST_VF_MODULE_MASK) == DEVDRV_HOST_VF_MODULE_MASK)) {
            /* all vf module init finish */
            ret = true;
        } else if ((pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_PF) &&
            ((startup_status->module_bit_map & DEVDRV_HOST_MODULE_MASK) == DEVDRV_HOST_MODULE_MASK)) {
            /* all pf module init finish */
            ret = true;
        } else if (jiffies_to_msecs(jiffies - startup_status->timestamp) > DEVDRV_MODULE_FINISH_TIMEOUT) {
            /* module init timeout */
            ret = true;
        }
    } else if (startup_status->status == DEVDRV_STARTUP_STATUS_TIMEOUT) {
        /* load timeout */
        ret = true;
    } else {
        if (jiffies_to_msecs(jiffies - startup_status->timestamp) > DEVDRV_MODULE_INIT_TIMEOUT) {
            /* slave chip no load */
            ret = true;
        }
    }

    mutex_unlock(&g_devdrv_ctrl_mutex);

    devdrv_info("Can hotreset ok. (status=%d; module_bit_map=0x%x; timestamp=%ld; cur=%ld; ret=%d)\n",
                startup_status->status, startup_status->module_bit_map, startup_status->timestamp, jiffies, ret);

    return ret;
}

struct pci_dev *devdrv_get_device_pf(struct pci_dev *pdev, unsigned int pf_num)
{
    unsigned int devfn;
    int domain_nr;
    struct pci_dev *dev = NULL;

    devfn = PCI_DEVFN(PCI_SLOT(pdev->devfn), pf_num);
    domain_nr = pci_domain_nr(pdev->bus);
    dev = pci_get_domain_bus_and_slot(domain_nr, pdev->bus->number, devfn);

    return dev;
}

STATIC void devdrv_device_unbind_driver(struct pci_dev *pdev_b, u32 virtfn_flag)
{
    struct pci_dev *pdev = NULL;

    if ((virtfn_flag == DEVDRV_SRIOV_TYPE_PF) && ((u32)PCI_FUNC(pdev_b->devfn) != 0)) {
        /* network pf0 */
        pdev = devdrv_get_device_pf(pdev_b, NETWORK_PF_0);
        if (pdev != NULL) {
            devdrv_info("Pcie hotreset, unbind driver. (bdf=%02x:%02x.%d)\n", pdev->bus->number,
                PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
            device_release_driver(&pdev->dev);
            pci_dev_put(pdev);
        }

        /* network pf1 */
        pdev = devdrv_get_device_pf(pdev_b, NETWORK_PF_1);
        if (pdev != NULL) {
            devdrv_info("Pcie hotreset, unbind driver. (bdf=%02x:%02x.%d)\n", pdev->bus->number,
                PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
            device_release_driver(&pdev->dev);
            pci_dev_put(pdev);
        }
    }

    /* davinci pf */
    pdev = pdev_b;
    devdrv_info("Pcie hotreset, unbind driver. (bdf=%02x:%02x.%d)\n", pdev->bus->number, PCI_SLOT(pdev->devfn),
                PCI_FUNC(pdev->devfn));
    device_release_driver(&pdev->dev);
}

STATIC void devdrv_device_bus_rescan(struct devdrv_ctrl *dev_ctrl)
{
    struct pci_dev *pdev = NULL;
    u32 num;

    if (dev_ctrl->startup_flg != DEVDRV_DEV_STARTUP_UNPROBED) {
        devdrv_info("Device has been already scaned. (dev_id=%u)\n", dev_ctrl->dev_id);
        return;
    }

    if (dev_ctrl->bus->self != NULL) {
        pdev = dev_ctrl->bus->self;
    } else {
        pdev = dev_ctrl->pdev; /* In some virtual machine, bus->self is NULL. */
    }

    num = devdrv_pci_rescan_bus_locked(pdev->bus);
    devdrv_event("Pcie hotreset, rescan device. (bdf=%02x:%02x.%d; dev_num=%u)\n", pdev->bus->number,
        PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), num);
}

STATIC void devdrv_device_bus_remove(struct pci_dev *pdev_r, u32 virtfn_flag)
{
    struct pci_dev *pdev = NULL;

    u8 func_num = PCI_FUNC(pdev_r->devfn);
    if ((virtfn_flag == DEVDRV_SRIOV_TYPE_PF) && ((u32)func_num != 0)) {
        /* network pf0 */
        pdev = devdrv_get_device_pf(pdev_r, NETWORK_PF_0);
        if (pdev != NULL) {
            devdrv_info("Pcie hotreset, bus remove. (bdf=%02x:%02x.%d)\n", pdev->bus->number, PCI_SLOT(pdev->devfn),
                        PCI_FUNC(pdev->devfn));
            devdrv_pci_stop_and_remove_bus_device_locked(pdev);
            pci_dev_put(pdev);
        }

        /* network pf1 */
        pdev = devdrv_get_device_pf(pdev_r, NETWORK_PF_1);
        if (pdev != NULL) {
            devdrv_info("Pcie hotreset, bus remove. (bdf=%02x:%02x.%d)\n", pdev->bus->number, PCI_SLOT(pdev->devfn),
                        PCI_FUNC(pdev->devfn));
            devdrv_pci_stop_and_remove_bus_device_locked(pdev);
            pci_dev_put(pdev);
        }
    }

    /* davinci pf */
    pdev = pdev_r;
    devdrv_info("Pcie hotreset, bus remove. (bdf=%02x:%02x.%d)\n", pdev->bus->number, PCI_SLOT(pdev->devfn),
                PCI_FUNC(pdev->devfn));

    devdrv_pci_stop_and_remove_bus_device_locked(pdev);
}

STATIC void devdrv_device_bridge_bus_reset(struct pci_bus *bus)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    pci_bridge_secondary_bus_reset_func birdge_reset_func;
#else
    pci_reset_bridge_secondary_bus_func reset_bridge_func;
#endif
    struct pci_dev *pdev = bus->self;
    if (pdev == NULL) {
        devdrv_err("Pcie bridge bus reset fail, bus->self is NULL.\n");
        return;
    }
    devdrv_info("Pcie hotreset parent, reset bridge secondary bus. (bdf=%02x:%02x.%d)\n", pdev->bus->number,
        PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    birdge_reset_func = symbol_get(pci_bridge_secondary_bus_reset);
    if (birdge_reset_func != NULL) {
        birdge_reset_func(pdev);
        devdrv_info("Pcie hotreset parent. (bdf=%02x:%02x.%d)\n", pdev->bus->number, PCI_SLOT(pdev->devfn),
                    PCI_FUNC(pdev->devfn));
        symbol_put(pci_bridge_secondary_bus_reset);
    }
#else
    reset_bridge_func = (pci_reset_bridge_secondary_bus_func)(uintptr_t)__symbol_get("pci_reset_bridge_secondary_bus");
    if (reset_bridge_func != NULL) {
        reset_bridge_func(pdev);
        devdrv_info("Pcie hotreset parent. (bdf=%02x:%02x.%d)\n", pdev->bus->number, PCI_SLOT(pdev->devfn),
            PCI_FUNC(pdev->devfn));
        symbol_put(pci_reset_bridge_secondary_bus);
    }
#endif
}

STATIC void devdrv_hotreset_set_timestamp(struct devdrv_ctrl *dev_ctrl)
{
    dev_ctrl->timestamp = (unsigned long)jiffies;
}

STATIC unsigned long devdrv_hotreset_get_timetamp(struct devdrv_ctrl *dev_ctrl)
{
    return dev_ctrl->timestamp;
}

void devdrv_probe_wait(int devid)
{
    struct devdrv_ctrl *dev_ctrl = &g_ctrls[devid];
    unsigned long timestamp;
    unsigned int timedif;

    timestamp = devdrv_hotreset_get_timetamp(dev_ctrl);
    if (timestamp == 0) {
        return;
    }
check:
    timedif = jiffies_to_msecs(jiffies - timestamp);
    if (timedif < DEVDRV_LOAD_FILE_DELAY) {
        msleep(1);
        goto check;
    }
    return;
}

STATIC bool devdrv_pre_hotreset_check(int dev_num)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int i, ready_dev_num;
    bool ret = false;

    ready_dev_num = 0;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        dev_ctrl = &g_ctrls[i];

        pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
        if (pci_ctrl != NULL) {
            if (devdrv_can_hotreset(pci_ctrl)) {
                ready_dev_num++;
            } else {
                devdrv_info("Device cannot hotreset.(dev_id=%d; status=%d)\n",
                            dev_ctrl->dev_id, pci_ctrl->startup_status.status);
            }
        }
    }

    devdrv_info("Get ready_dev_num. (dev_num=%d; ready_dev_num=%d)\n", dev_num, ready_dev_num);

    if (ready_dev_num == dev_num) {
        ret = true;
    }

    return ret;
}

STATIC bool devdrv_hot_reset_is_master_dev(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
        return false;
    }

    if (pci_ctrl->shr_para->chip_id > 0) {
        return false;
    }

    return true;
}

STATIC void devdrv_before_hot_reset(void)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int i;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        dev_ctrl = &g_ctrls[i];
        /* nP in one OS */
        if (dev_ctrl->startup_flg != DEVDRV_DEV_STARTUP_UNPROBED) {
            pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
            /* 1PF2P only has 1 pdev and no need to remove */
            if ((pci_ctrl->shr_para->chip_id > 0) &&
                (DEVDRV_DAVINCI_DEV_NUM_1PF1P == devdrv_get_davinci_dev_num_by_pdev(dev_ctrl->pdev))) {
                devdrv_info("Pcie hotreset before hot reset remove. (dev_id=%u)\n", dev_ctrl->dev_id);
                dev_ctrl->master_flag = 0;
                devdrv_device_bus_remove(dev_ctrl->pdev, dev_ctrl->virtfn_flag);
                msleep(1); /* free cpu */
            }
        }
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        dev_ctrl = &g_ctrls[i];
        /* nPF in one P */
        if (dev_ctrl->startup_flg != DEVDRV_DEV_STARTUP_UNPROBED) {
            pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
            if (devdrv_hot_reset_is_master_dev(pci_ctrl)) {
                devdrv_info("Pcie hotreset before hot reset unbind. (dev_id=%u)\n", dev_ctrl->dev_id);
                dev_ctrl->master_flag = 1;
                pci_ctrl->shr_para->hot_reset_pcie_flag = DEVDRV_PCIE_HOT_RESET_FLAG;
                devdrv_device_unbind_driver(dev_ctrl->pdev, dev_ctrl->virtfn_flag);
                msleep(1); /* free cpu */
            }
        }
    }
}

STATIC int devdrv_pci_try_reset_function(struct pci_dev *pdev, u32 dev_id)
{
    u32 retry_cnt = 0;
    int ret;

    do {
        ret = pci_try_reset_function(pdev);
        if ((ret == -EAGAIN) && (retry_cnt < 10)) { // retry 10 times
            retry_cnt++;
            msleep(10); // 10 ms
        } else {
            break;
        }
    } while (1);

    if (ret < 0) {
        devdrv_warn("Reset function abnormal. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
    return ret;
}

STATIC void devdrv_pci_hot_reset(struct devdrv_ctrl *dev_ctrl)
{
    u8 func_num;
    int ret;

    if (dev_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        #ifndef DRV_UT
        pm_runtime_get_sync(&dev_ctrl->pdev->dev);
        ret = pci_reset_function(dev_ctrl->pdev);
        pm_runtime_put(&dev_ctrl->pdev->dev);
        if (ret < 0) {
            devdrv_info("Reset vf function. (dev_id=%d; ret=%d)\n", dev_ctrl->dev_id, ret);
        }
        #endif
        return;
    }

    func_num = PCI_FUNC(dev_ctrl->pdev->devfn);
    if ((u32)func_num == PCIE_PF_NUM) {
        devdrv_device_bridge_bus_reset(dev_ctrl->bus);
    } else {
        ret = devdrv_pci_try_reset_function(dev_ctrl->pdev, dev_ctrl->dev_id);
        devdrv_info("Reset function. (dev_id=%d; ret=%d)\n", dev_ctrl->dev_id, ret);
    }
    return;
}

STATIC void devdrv_do_hot_reset(void)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    int i;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        dev_ctrl = &g_ctrls[i];

        if (dev_ctrl->master_flag == 1) {
            if (dev_ctrl->bus != NULL) {
                devdrv_info("Device do hot reset. (dev_id=%u)\n", dev_ctrl->dev_id);
                devdrv_pci_hot_reset(dev_ctrl);
            }
        }
    }
}

STATIC void devdrv_after_hot_reset(int num)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    int delay_count = 0;
    int i;
    int num_after;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        dev_ctrl = &g_ctrls[i];
        if (dev_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
            continue;
        }
        if (dev_ctrl->master_flag == 1) {
            dev_ctrl->master_flag = 0;
            if (dev_ctrl->pdev != NULL) {
                devdrv_info("Device bus remove. (dev_id=%u)\n", dev_ctrl->dev_id);
                devdrv_device_bus_remove(dev_ctrl->pdev, dev_ctrl->virtfn_flag);
                msleep(1); /* free cpu */
            }
        }
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        dev_ctrl = &g_ctrls[i];
        if (dev_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
            continue;
        }
        devdrv_hotreset_set_timestamp(dev_ctrl);
    }
    /* make sure rescan device num equal with num before hot reset */
    do {
        for (i = 0; i < MAX_DEV_CNT; i++) {
            dev_ctrl = &g_ctrls[i];
            devdrv_hotreset_set_timestamp(dev_ctrl);
            if (dev_ctrl->bus == NULL) {
                continue;
            }
            devdrv_info("Device bus rescan. (dev_id=%u)\n", dev_ctrl->dev_id);
            if (dev_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
                num_after = device_attach(&dev_ctrl->pdev->dev);
            } else {
                devdrv_device_bus_rescan(dev_ctrl);
            }
        }
        devdrv_init_dev_num();
        num_after = devdrv_get_dev_num();
        if (num == num_after) {
            break;
        }
        delay_count++;
        ssleep(DEVDRV_HOT_RESET_DELAY);
    } while (delay_count < DEVDRV_MAX_DELAY_COUNT);

    devdrv_info("Get rescan cost time. (delay_count=%d; num_before=%d; num_after=%d)\n", delay_count, num, num_after);
}

void devdrv_set_hccs_link_status(u32 dev_id, u32 val)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(dev_id);
    if ((pci_ctrl == NULL) || (pci_ctrl->shr_para == NULL)) {
        devdrv_err("Gan pci_ctrl fail. (dev_id=%u)\n", dev_id);
        return;
    }

    pci_ctrl->shr_para->hccs_status = val;
}

STATIC int devdrv_get_hccs_link_status(u32 dev_id, u32 *val)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if ((pci_ctrl == NULL) || (pci_ctrl->shr_para == NULL)) {
        devdrv_err("Gan pci_ctrl fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    *val = pci_ctrl->shr_para->hccs_status;
    return 0;
}

STATIC bool devdrv_is_amp_system(u32 dev_id, int node_id, int chip_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_shr_para __iomem *shr_para = NULL;
    u32 hccs_status;
    int ret;
    u32 i;

    if (devdrv_get_dev_chip_type(dev_id) == HISI_CLOUD_V2) {
        ret = devdrv_get_hccs_link_status(dev_id, &hccs_status);
        if ((ret == 0) && (hccs_status == 0)) {
            return true;
        } else {
            return false;
        }
    }

    if (devdrv_get_dev_chip_type(dev_id) != HISI_CLOUD_V1) {
        return true;
    }

    if (chip_id != 0) {
        return false;
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (dev_id == i) {
            continue;
        }
        pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(i);
        if ((pci_ctrl == NULL) || (pci_ctrl->shr_para == NULL)) {
            continue;
        }
        shr_para = pci_ctrl->shr_para;
        if ((shr_para->node_id == node_id) && (shr_para->chip_id != 0)) {
            return false;
        }
    }

    return true;
}

STATIC int devdrv_hot_reset_check_all_davinci_dev_in_pdev(struct pci_dev *pdev)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = (struct devdrv_pdev_ctrl *)pci_get_drvdata(pdev);
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int i;

    for (i = 0; i < pdev_ctrl->dev_num; i++) {
        pci_ctrl = pdev_ctrl->pci_ctrl[i];
        if (devdrv_can_hotreset(pci_ctrl) == false) {
            devdrv_info("Device cannot hotreset. (dev_id=%u; status=%d)\n",
                pci_ctrl->dev_id, pci_ctrl->startup_status.status);
            return -EAGAIN;
        }
    }

    return 0;
}

STATIC void devdrv_device_rescan(struct devdrv_ctrl *dev_ctrl)
{
    int delay_count = 0;

    if (dev_ctrl->priv != NULL) {
        devdrv_info("Pcie hotreset, after bus remove. (dev_id=%u)\n", dev_ctrl->dev_id);
    }

    do {
        devdrv_hotreset_set_timestamp(dev_ctrl);
        devdrv_device_bus_rescan(dev_ctrl);
        if (dev_ctrl->priv != NULL) {
            break;
        }
        delay_count++;
        ssleep(3); /* sleep 3s for next rescan */
    } while (delay_count < DEVDRV_MAX_DELAY_COUNT);
}

STATIC void devdrv_device_pf_bus_remove_and_rescan(struct pci_dev *pdev, struct devdrv_ctrl *dev_ctrl)
{
    /* after hotreset */
    devdrv_device_bus_remove(pdev, dev_ctrl->virtfn_flag);
    devdrv_device_rescan(dev_ctrl);
}

STATIC void devdrv_device_vf_bus_attach(struct devdrv_ctrl *dev_ctrl)
{
    int delay_count = 0;
    u32 num;

    do {
        num = (u32)device_attach(&dev_ctrl->pdev->dev);
        devdrv_info("Pcie vf flr reset, rescan device. (bdf=%02x:%02x.%d; dev_num=%u)\n", dev_ctrl->pdev->bus->number,
            PCI_SLOT(dev_ctrl->pdev->devfn), PCI_FUNC(dev_ctrl->pdev->devfn), num);
        if (dev_ctrl->priv != NULL) {
            break;
        }
        delay_count++;
        ssleep(3);
    } while (delay_count < DEVDRV_MAX_DELAY_COUNT);

    return;
}

STATIC int devdrv_hot_reset_single_device(u32 dev_id)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct pci_dev *pdev = NULL;
    u32 delay = DEVDRV_HOT_RESET_DELAY;
    int chip_id, node_id, ret;
    u32 main_dev_id;

    pci_ctrl = devdrv_pci_ctrl_get(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_info("Can not get dev_ctrl. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) {
        delay = DEVDRV_HOT_RESET_LONG_DELAY;
    }

    /* get master pci_ctrl */
    pdev = pci_ctrl->pdev;
    main_dev_id = devdrv_get_main_davinci_devid_by_pdev(pdev);
    if (main_dev_id != dev_id) {
        devdrv_pci_ctrl_put(pci_ctrl);
        pci_ctrl = devdrv_pci_ctrl_get(main_dev_id);
        if (pci_ctrl == NULL) {
            devdrv_info("Can not get dev_ctrl. (dev_id=%u)\n", dev_id);
            return -EINVAL;
        }
    }
    dev_ctrl = &g_ctrls[pci_ctrl->dev_id];

    ret = devdrv_hot_reset_check_all_davinci_dev_in_pdev(pdev);
    if (ret != 0) {
        devdrv_pci_ctrl_put(pci_ctrl);
        return ret;
    }

    node_id = pci_ctrl->shr_para->node_id;
    rmb(); /* Prevent compiler optimizing ldp instruction, which not support in some virtual machine */
    chip_id = pci_ctrl->shr_para->chip_id;

    if (devdrv_is_amp_system(dev_id, node_id, chip_id) == false) {
        devdrv_pci_ctrl_put(pci_ctrl);
        devdrv_info("Device is not amp system. (dev_id=%u)\n", dev_id);
        return -EOPNOTSUPP;
    }

    devdrv_pci_ctrl_put(pci_ctrl);

    (void)devdrv_get_pci_ctrl_and_prepare_for_hotreset(dev_id);
    if (main_dev_id != dev_id) {
        (void)devdrv_get_pci_ctrl_and_prepare_for_hotreset(main_dev_id);
    }

    /* before hotreset: unbind driver */
    pci_ctrl->shr_para->hot_reset_pcie_flag = DEVDRV_PCIE_HOT_RESET_FLAG;
    devdrv_device_unbind_driver(pdev, pci_ctrl->virtfn_flag);

    /* do hotreset */
    devdrv_info("Device hot reset begin. (dev_id=%u)\n", dev_id);
    devdrv_pci_hot_reset(dev_ctrl);

    ssleep(delay);

    if (dev_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        devdrv_device_vf_bus_attach(dev_ctrl);
    } else {
        devdrv_device_pf_bus_remove_and_rescan(pdev, dev_ctrl);
    }
    return 0;
}

STATIC void devdrv_get_main_slave_dev_id(u32 dev_id, u32 *main_dev_id, u32 *slave_dev_id)
{
    if (dev_id % DEVDRV_DAVINCI_DEV_NUM_1PF2P == 0) {
        *main_dev_id = dev_id;
        *slave_dev_id = dev_id + 1;
    } else {
        *main_dev_id = dev_id - 1;
        *slave_dev_id = dev_id;
    }
}

STATIC int devdrv_unbind_single_chip(u32 main_dev_id, u32 slave_dev_id)
{
    struct devdrv_pci_ctrl *main_pci_ctrl = NULL;
    struct devdrv_pci_ctrl *slave_pci_ctrl = NULL;

    main_pci_ctrl = devdrv_pci_ctrl_get(main_dev_id);
    if (main_pci_ctrl == NULL) {
        devdrv_info("Can not get main dev_ctrl. (dev_id=%u)\n", main_dev_id);
        return -EINVAL;
    }

    slave_pci_ctrl = devdrv_pci_ctrl_get(slave_dev_id);
    if (slave_pci_ctrl == NULL) {
        devdrv_info("Can not get slave dev_ctrl. (dev_id=%u)\n", slave_dev_id);
        devdrv_pci_ctrl_put(main_pci_ctrl);
        return -EINVAL;
    }

    if (devdrv_can_hotreset(main_pci_ctrl) == false) {
        devdrv_info("Main device cannot hotreset. (dev_id=%u; status=%d)\n",
            main_dev_id, main_pci_ctrl->startup_status.status);
        devdrv_pci_ctrl_put(main_pci_ctrl);
        devdrv_pci_ctrl_put(slave_pci_ctrl);
        return -EAGAIN;
    }

    if (devdrv_can_hotreset(slave_pci_ctrl) == false) {
        devdrv_info("Slave device cannot hotreset. (dev_id=%u; status=%d)\n",
            slave_dev_id, slave_pci_ctrl->startup_status.status);
        devdrv_pci_ctrl_put(main_pci_ctrl);
        devdrv_pci_ctrl_put(slave_pci_ctrl);
        return -EAGAIN;
    }

    devdrv_pci_ctrl_put(main_pci_ctrl);
    devdrv_pci_ctrl_put(slave_pci_ctrl);

    (void)devdrv_get_pci_ctrl_and_prepare_for_hotreset(main_dev_id);
    (void)devdrv_get_pci_ctrl_and_prepare_for_hotreset(slave_dev_id);

    /* before hotreset: unbind driver */
    slave_pci_ctrl->shr_para->hot_reset_pcie_flag = DEVDRV_PCIE_HOT_RESET_FLAG;
    devdrv_info("Unbind slave device. (dev_id=%u)\n", slave_dev_id);
    devdrv_device_unbind_driver(slave_pci_ctrl->pdev, slave_pci_ctrl->virtfn_flag);

    main_pci_ctrl->shr_para->hot_reset_pcie_flag = DEVDRV_PCIE_HOT_RESET_FLAG;
    devdrv_info("Unbind main device. (dev_id=%u)\n", main_dev_id);
    devdrv_device_unbind_driver(main_pci_ctrl->pdev, main_pci_ctrl->virtfn_flag);

    return 0;
}

STATIC int devdrv_hot_reset_single_chip(u32 dev_id)
{
    struct devdrv_ctrl *main_dev_ctrl = NULL;
    struct devdrv_ctrl *slave_dev_ctrl = NULL;
    u32 delay = DEVDRV_HOT_RESET_DELAY;
    u32 main_dev_id, slave_dev_id;
    int ret;

    devdrv_get_main_slave_dev_id(dev_id, &main_dev_id, &slave_dev_id);

    if (devdrv_get_connect_protocol(main_dev_id) == CONNECT_PROTOCOL_HCCS) {
        delay = DEVDRV_HOT_RESET_LONG_DELAY;
    }

    /* unbind main & slave device */
    ret = devdrv_unbind_single_chip(main_dev_id, slave_dev_id);
    if (ret != 0) {
        devdrv_info("Not ready to unbind device. (main_dev_id=%u; slave_dev_id=%u)\n", main_dev_id, slave_dev_id);
        return ret;
    }

    /* remove slave device */
    slave_dev_ctrl = &g_ctrls[slave_dev_id];
    devdrv_info("Device bus remove. (slave_dev_id=%u)\n", slave_dev_id);
    devdrv_device_bus_remove(slave_dev_ctrl->pdev, slave_dev_ctrl->virtfn_flag);
    slave_dev_ctrl->pdev = NULL;
    msleep(1); /* sleep 1ms for free cpu */

    /* do hotreset */
    devdrv_info("Device hot reset begin. (main_dev_id=%u; slave_dev_id=%u)\n", main_dev_id, slave_dev_id);

    main_dev_ctrl = &g_ctrls[main_dev_id];
    devdrv_pci_hot_reset(main_dev_ctrl);

    ssleep(delay);

    /* remove main device */
    devdrv_info("Device bus remove. (main_dev_id=%u)\n", main_dev_id);
    devdrv_device_bus_remove(main_dev_ctrl->pdev, main_dev_ctrl->virtfn_flag);
    main_dev_ctrl->pdev = NULL;
    msleep(1); /* sleep 1ms for free cpu */

    /* rescan main and slave device */
    devdrv_device_rescan(slave_dev_ctrl);
    devdrv_device_rescan(main_dev_ctrl);

    return 0;
}

STATIC int devdrv_hot_reset_all_device(void)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 delay = DEVDRV_HOT_RESET_DELAY;
    int num_before;
    u32 i;

    num_before = devdrv_get_dev_num();
    if (num_before <= 0) {
        devdrv_err("Pcie hotreset all device. (dev_num=%d)\n", num_before);
        return -EAGAIN;
    }

    if (devdrv_pre_hotreset_check(num_before) == false) {
        devdrv_err("Can not hotreset. (dev_num=%d)\n", num_before);
        return -EAGAIN;
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        pci_ctrl = devdrv_get_pci_ctrl_and_prepare_for_hotreset(i);
        if ((pci_ctrl != NULL) && (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS)) {
            delay = DEVDRV_HOT_RESET_LONG_DELAY;
        }
    }

    devdrv_before_hot_reset();

    devdrv_do_hot_reset();

    ssleep(delay);

    devdrv_after_hot_reset(num_before);

    return 0;
}

STATIC int devdrv_pre_smp_hotreset_check(u64 smp_device_bitmap, u32 smp_device_number)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int i, ready_dev_num;
    int ret = false;

    ready_dev_num = 0;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (((smp_device_bitmap >> i) & 1) != 1) {
            continue;
        }
        dev_ctrl = &g_ctrls[i];

        pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
        if (pci_ctrl != NULL) {
            if (devdrv_can_hotreset(pci_ctrl)) {
                ready_dev_num++;
            } else {
                devdrv_info("Device cannot hotreset.(dev_id=%d; status=%d)\n",
                            dev_ctrl->dev_id, pci_ctrl->startup_status.status);
            }
        }
    }

    devdrv_info("Get ready_dev_num. (dev_num=%d; ready_dev_num=%d)\n", smp_device_number, ready_dev_num);

    if (ready_dev_num == smp_device_number) {
        ret = true;
    }

    return ret;
}

STATIC void devdrv_before_smp_hot_reset(u64 smp_device_bitmap, u32 smp_device_number)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int i;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (((smp_device_bitmap >> i) & 1) != 1) {
            continue;
        }
        dev_ctrl = &g_ctrls[i];
        /* nP in one OS */
        if (dev_ctrl->startup_flg != DEVDRV_DEV_STARTUP_UNPROBED) {
            pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
            /* 1PF2P only has 1 pdev and no need to remove */
            if ((pci_ctrl->shr_para->chip_id > 0) &&
                (DEVDRV_DAVINCI_DEV_NUM_1PF1P == devdrv_get_davinci_dev_num_by_pdev(dev_ctrl->pdev))) {
                devdrv_info("Pcie hotreset before hot reset remove. (dev_id=%u)\n", dev_ctrl->dev_id);
                dev_ctrl->master_flag = 0;
                devdrv_device_bus_remove(dev_ctrl->pdev, dev_ctrl->virtfn_flag);
                msleep(1); /* free cpu */
            }
        }
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (((smp_device_bitmap >> i) & 1) != 1) {
            continue;
        }
        dev_ctrl = &g_ctrls[i];
        /* nPF in one P */
        if (dev_ctrl->startup_flg != DEVDRV_DEV_STARTUP_UNPROBED) {
            pci_ctrl = (struct devdrv_pci_ctrl *)dev_ctrl->priv;
            if (devdrv_hot_reset_is_master_dev(pci_ctrl)) {
                devdrv_info("Pcie hotreset before hot reset unbind. (dev_id=%u)\n", dev_ctrl->dev_id);
                dev_ctrl->master_flag = 1;
                pci_ctrl->shr_para->hot_reset_pcie_flag = DEVDRV_PCIE_HOT_RESET_FLAG;
                devdrv_device_unbind_driver(dev_ctrl->pdev, dev_ctrl->virtfn_flag);
                msleep(1); /* free cpu */
            }
        }
    }
}

STATIC void devdrv_do_smp_hot_reset(u64 smp_device_bitmap)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    int i;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (((smp_device_bitmap >> i) & 1) != 1) {
            continue;
        }
        dev_ctrl = &g_ctrls[i];

        if (dev_ctrl->master_flag == 1) {
            if (dev_ctrl->bus != NULL) {
                devdrv_info("Device do hot reset. (dev_id=%u)\n", dev_ctrl->dev_id);
                devdrv_pci_hot_reset(dev_ctrl);
            }
        }
    }
}

STATIC void devdrv_after_smp_hot_reset(u64 smp_device_bitmap, u32 smp_device_number)
{
    struct devdrv_ctrl *dev_ctrl = NULL;
    int delay_count = 0;
    int i;
    int num_after;

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (((smp_device_bitmap >> i) & 1) != 1) {
            continue;
        }
        dev_ctrl = &g_ctrls[i];
        if (dev_ctrl->master_flag == 1) {
            dev_ctrl->master_flag = 0;
            if (dev_ctrl->pdev != NULL) {
                devdrv_info("Device bus remove. (dev_id=%u)\n", dev_ctrl->dev_id);
                devdrv_device_bus_remove(dev_ctrl->pdev, dev_ctrl->virtfn_flag);
                msleep(1); /* free cpu */
            }
        }
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        dev_ctrl = &g_ctrls[i];
        devdrv_hotreset_set_timestamp(dev_ctrl);
    }
    /* make sure rescan device num equal with num before hot reset */
    do {
        num_after = 0;
        for (i = 0; i < MAX_DEV_CNT; i++) {
            if (((smp_device_bitmap >> i) & 1) != 1) {
                continue;
            }
            dev_ctrl = &g_ctrls[i];
            devdrv_hotreset_set_timestamp(dev_ctrl);
            if (dev_ctrl->bus != NULL) {
                devdrv_info("Device bus rescan. (dev_id=%u)\n", dev_ctrl->dev_id);
                devdrv_device_bus_rescan(dev_ctrl);
            }
        }
        for (i = 0; i < MAX_DEV_CNT; i++) {
            if (((smp_device_bitmap >> i) & 1) != 1) {
                continue;
            }
            dev_ctrl = &g_ctrls[i];
            devdrv_hotreset_set_timestamp(dev_ctrl);
            devdrv_device_bus_rescan(dev_ctrl);
            if (dev_ctrl->priv != NULL) {
                num_after++;
            }
        }
        if (num_after == smp_device_number) {
            break;
        }
        delay_count++;
        ssleep(3);
    } while (delay_count < DEVDRV_MAX_DELAY_COUNT);

    devdrv_info("Get rescan cost time. (delay_count=%d; num_before=%d; num_after=%d)\n", delay_count, smp_device_number, num_after);
}

STATIC int devdrv_hot_reset_all_smp_device(u64 smp_device_bitmap, u32 smp_device_num)
{
    u32 i;

    if (devdrv_pre_smp_hotreset_check(smp_device_bitmap, smp_device_num) == false) {
        devdrv_err("Can not hotreset. (dev_num=%d)\n", smp_device_num);
        return -EAGAIN;
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (((smp_device_bitmap >> i) & 1) == 1) {
        (void)devdrv_get_pci_ctrl_and_prepare_for_hotreset(i);
        }
    }

    devdrv_before_smp_hot_reset(smp_device_bitmap, smp_device_num);
    devdrv_do_smp_hot_reset(smp_device_bitmap);
    ssleep(DEVDRV_HOT_RESET_DELAY);
    devdrv_after_smp_hot_reset(smp_device_bitmap, smp_device_num);
    return 0;
}

int devdrv_hot_reset_smp_device(u64 smp_device_bitmap, u32 smp_device_num)
{
    int ret = 0;
    int i;

    mutex_lock(&g_devdrv_ctrl_mutex);
    for (i = 0; i < MAX_DEV_CNT; i++) {
        if ((((g_devdrv_ctrl_hot_reset_status >> i) & 1) & ((smp_device_bitmap >> i) & 1)) == 1) {
            mutex_unlock(&g_devdrv_ctrl_mutex);
            devdrv_err("Pcie hotreset, but device on process. (dev_id=%u; dev_mask=%llu)\n",
                i, g_devdrv_ctrl_hot_reset_status);
            return -EAGAIN;
        } else if ((((smp_device_bitmap >> i) & 1)) == 1) {
            g_devdrv_ctrl_hot_reset_status |= (1ULL << i);
            mutex_unlock(&g_devdrv_ctrl_mutex);
        } else {
            mutex_unlock(&g_devdrv_ctrl_mutex);
        }
    }

    ret = devdrv_hot_reset_all_smp_device(smp_device_bitmap, smp_device_num);
    if (ret == 0) {
        devdrv_info("Pcie hotreset, hot reset success. (smp_device_bitmap%llu)\n", smp_device_bitmap);
    }

    mutex_lock(&g_devdrv_ctrl_mutex);
    for (i = 0; i < MAX_DEV_CNT; i++) {
        if ((((g_devdrv_ctrl_hot_reset_status >> i) & 1) & ((smp_device_bitmap >> i) & 1)) == 1) {
        g_devdrv_ctrl_hot_reset_status &= ~(1ULL << i);
    }
    }
    mutex_unlock(&g_devdrv_ctrl_mutex);

    return ret;
}
EXPORT_SYMBOL(devdrv_hot_reset_smp_device);

int devdrv_hot_reset_device(u32 dev_id)
{
    int ret = 0;
    u32 dev_id_tmp = dev_id;
    u32 main_dev_id, slave_dev_id;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    mutex_lock(&g_devdrv_ctrl_mutex);

    if (dev_id == 0xff) {
        if (g_devdrv_ctrl_hot_reset_status != 0) {
            mutex_unlock(&g_devdrv_ctrl_mutex);
            devdrv_err("Pcie hotreset, but device on process. (dev_id=%u; dev_mask=%llu)\n",
                dev_id, g_devdrv_ctrl_hot_reset_status);
            return -EAGAIN;
        } else {
            g_devdrv_ctrl_hot_reset_status = DEVDRV_PCIE_HOT_RESET_ALL_DEVICE_MASK;
        }
    } else if (dev_id < MAX_DEV_CNT) {
        pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
        if (pci_ctrl == NULL) {
            mutex_unlock(&g_devdrv_ctrl_mutex);
            devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", dev_id);
            return -EINVAL;
        }
        if (pci_ctrl->multi_die == DEVDRV_MULTI_DIE_ONE_CHIP) {
            devdrv_get_main_slave_dev_id(dev_id, &main_dev_id, &slave_dev_id);
            dev_id_tmp = main_dev_id;
        }

        if (((g_devdrv_ctrl_hot_reset_status >> dev_id_tmp) & 1U) == 1) {
            mutex_unlock(&g_devdrv_ctrl_mutex);
            devdrv_err("Pcie hotreset, but device on process. (dev_id=%u; dev_mask=%llu)\n",
                dev_id, g_devdrv_ctrl_hot_reset_status);
            return -EAGAIN;
        } else {
            g_devdrv_ctrl_hot_reset_status |= (1ULL << dev_id_tmp);
        }
    } else {
        mutex_unlock(&g_devdrv_ctrl_mutex);
        devdrv_err("Pcie hotreset error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    mutex_unlock(&g_devdrv_ctrl_mutex);

    if (dev_id == 0xff) {
        ret = devdrv_hot_reset_all_device();
    } else if (pci_ctrl->multi_die == DEVDRV_MULTI_DIE_ONE_CHIP) {
        ret = devdrv_hot_reset_single_chip(dev_id);
    } else {
        ret = devdrv_hot_reset_single_device(dev_id);
    }

    if (ret == 0) {
        devdrv_info("Pcie hotreset, hot reset success. (dev_id=%u)\n", dev_id);
    }

    mutex_lock(&g_devdrv_ctrl_mutex);
    if (dev_id == 0xff) {
        g_devdrv_ctrl_hot_reset_status = 0;
    } else {
        g_devdrv_ctrl_hot_reset_status &= ~(1ULL << dev_id_tmp);
    }
    mutex_unlock(&g_devdrv_ctrl_mutex);

    return ret;
}
EXPORT_SYMBOL(devdrv_hot_reset_device);

int devdrv_get_master_devid_in_the_same_os(u32 dev_id, u32 *master_dev_id)
{
    struct devdrv_ctrl *ctrl = NULL;
    struct devdrv_ctrl *peer_ctrl = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_pci_ctrl *peer_pci_ctrl = NULL;
    struct devdrv_shr_para __iomem *shr_para = NULL;
    struct devdrv_shr_para __iomem *peer_shr_para = NULL;
    u32 i;

    if ((dev_id >= MAX_DEV_CNT) || (master_dev_id == NULL)) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ctrl = devdrv_get_bottom_half_devctrl_by_id(dev_id);
    if ((ctrl == NULL) || (ctrl->priv == NULL)) {
        return -EINVAL;
    }
    pci_ctrl = (struct devdrv_pci_ctrl *)ctrl->priv;
    shr_para = pci_ctrl->shr_para;
    if (pci_ctrl->remote_dev_id == 0) {
        *master_dev_id = dev_id;
        return 0;
    }

    if ((pci_ctrl->chip_type == HISI_MINI_V2) || (pci_ctrl->chip_type == HISI_CLOUD_V2)) {
        *master_dev_id = dev_id - 1;
        return 0;
    }

    for (i = 0; i < MAX_DEV_CNT; i++) {
        if (dev_id == i) {
            continue;
        }
        peer_ctrl = devdrv_get_bottom_half_devctrl_by_id(i);
        if ((peer_ctrl == NULL) || (peer_ctrl->priv == NULL)) {
            continue;
        }
        peer_pci_ctrl = (struct devdrv_pci_ctrl *)peer_ctrl->priv;
        peer_shr_para = peer_pci_ctrl->shr_para;
        if ((shr_para->node_id == peer_shr_para->node_id) && (peer_shr_para->chip_id == 0)) {
            *master_dev_id = i;
            return 0;
        }
    }
    devdrv_err("No find master dev_id. (dev_id=%u)\n", dev_id);
    return -EINVAL;
}
EXPORT_SYMBOL(devdrv_get_master_devid_in_the_same_os);

STATIC void *devdrv_get_devdrv_priv_with_dev_index(struct pci_dev *pdev)
{
    struct devdrv_pdev_ctrl *pdev_ctrl = NULL;

    if (pdev == NULL) {
        devdrv_err("pdev is NULL\n");
        return NULL;
    }
    pdev_ctrl = (struct devdrv_pdev_ctrl *)pci_get_drvdata(pdev);
    if (pdev_ctrl == NULL) {
        devdrv_err("pdev_ctrl is NULL.\n");
        return NULL;
    }

    return (void *)&(pdev_ctrl->vdavinci_priv);
}

void *devdrv_get_devdrv_priv(struct pci_dev *pdev)
{
    return devdrv_get_devdrv_priv_with_dev_index(pdev);
}
EXPORT_SYMBOL(devdrv_get_devdrv_priv);

int devdrv_get_dev_id_by_pdev_with_dev_index(struct pci_dev *pdev, int dev_index)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (pdev == NULL) {
        devdrv_err("pdev is NULL.\n");
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_dev_by_index_in_pdev(pdev, dev_index);
    if (pci_ctrl == NULL) {
        devdrv_err("pci_ctrl is NULL.\n");
        return -EINVAL;
    }

    return (int)pci_ctrl->dev_id;
}
EXPORT_SYMBOL(devdrv_get_dev_id_by_pdev_with_dev_index);

int devdrv_get_dev_id_by_pdev(struct pci_dev *pdev)
{
    return devdrv_get_dev_id_by_pdev_with_dev_index(pdev, 0);
}
EXPORT_SYMBOL(devdrv_get_dev_id_by_pdev);

int devdrv_get_hccs_link_status_and_group_id(u32 devid, u32 *hccs_status, u32 hccs_group_id[], u32 group_id_num)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int i;

    if ((devid >= MAX_DEV_CNT) || (group_id_num > HCCS_GROUP_SUPPORT_MAX_CHIPNUM)) {
        devdrv_err("Invalid devid or group id num. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if ((hccs_status == NULL) || (hccs_group_id == NULL)) {
        devdrv_err("Hccs_status or hccs_group_id is null. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (pci_ctrl->ops.get_hccs_link_info == NULL) {
        devdrv_warn("Not support to get hccs link info. (dev_id=%u)\n", devid);
        return -EPERM;
    }

    if (pci_ctrl->hccs_status == -1) {
        pci_ctrl->ops.get_hccs_link_info(pci_ctrl);
    }

    *hccs_status = pci_ctrl->hccs_status;
    for (i = 0; i < group_id_num; i++) {
        hccs_group_id[i] = pci_ctrl->hccs_group_id[i];
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_get_hccs_link_status_and_group_id);

int devdrv_get_runtime_runningplat(u32 devid, u64 *running_plat)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (running_plat == NULL) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if (ctrl == NULL) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -ENODEV;
    }

    pci_ctrl = ctrl->priv;
    *running_plat = pci_ctrl->shr_para->runtime_runningplat;
    rmb();

    return 0;
}
EXPORT_SYMBOL(devdrv_get_runtime_runningplat);

int devdrv_set_runtime_runningplat(u32 devid, u64 running_plat)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (devid >= MAX_DEV_CNT) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }

    ctrl = devdrv_get_top_half_devctrl_by_id(devid);
    if (ctrl == NULL) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -ENODEV;
    }

    pci_ctrl = ctrl->priv;
    pci_ctrl->shr_para->runtime_runningplat = running_plat;
    wmb();

    return 0;
}
EXPORT_SYMBOL(devdrv_set_runtime_runningplat);

int devdrv_get_heartbeat_count(u32 devid, u64* count)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    if (count == NULL) {
        devdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }
    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if (ctrl == NULL) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -ENODEV;
    }
    pci_ctrl = ctrl->priv;
    *count = pci_ctrl->shr_para->heartbeat_count;
    return 0;
}
EXPORT_SYMBOL(devdrv_get_heartbeat_count);

int devdrv_set_heartbeat_count(u32 devid, u64 count)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_ctrl *ctrl = NULL;

    ctrl = devdrv_get_bottom_half_devctrl_by_id(devid);
    if (ctrl == NULL) {
        devdrv_err("Get dev_ctrl failed. (dev_id=%u)\n", devid);
        return -ENODEV;
    }
    pci_ctrl = ctrl->priv;
    pci_ctrl->shr_para->host_heartbeat_count = count;

    return 0;
}
EXPORT_SYMBOL(devdrv_set_heartbeat_count);

STATIC u64 g_pci_link_speed[DEVDRV_BANDW_PCIESPD_GEN_MAX] = {
    DEVDRV_BANDW_INVALID_SPEED,
    DEVDRV_BANDW_X1_SPEED_GEN1,
    DEVDRV_BANDW_X1_SPEED_GEN2,
    DEVDRV_BANDW_X1_SPEED_GEN3,
    DEVDRV_BANDW_X1_SPEED_GEN4,
    DEVDRV_BANDW_X1_SPEED_GEN5,
    DEVDRV_BANDW_X1_SPEED_GEN6,
};

STATIC u64 devdrv_link_speed_transfer(u32 link_speed_raw)
{
    if (link_speed_raw >= DEVDRV_BANDW_PCIESPD_GEN_MAX) {
        devdrv_err("link_speed_raw is invalid. (link_speed_raw=%u)\n", link_speed_raw);
        return 0;
    }
    return g_pci_link_speed[link_speed_raw];
}

u64 devdrv_get_bandwidth_info(struct pci_dev *pdev)
{
    u64 link_speed, link_width;
    u16 link_status;
    struct pci_bus *bus;
    int pos;

    bus = pdev->bus;
    pos = pci_bus_find_capability(bus, pdev->devfn, PCI_CAP_ID_EXP);
    pci_bus_read_config_word(bus, pdev->devfn, pos + PCI_EXP_LNKSTA, &link_status);

    link_speed = devdrv_link_speed_transfer((u32)(link_status & PCI_EXP_LNKSTA_CLS));
    link_width = (u64)((link_status & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT);

    return link_speed * link_width;
}

int devdrv_get_theoretical_capability(u32 devid, u64 *bandwidth, u64 *packspeed)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if ((bandwidth == NULL) || (packspeed == NULL)) {
        devdrv_err("Parameter bandwidth is null. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    *bandwidth = pci_ctrl->res.link_info.bandwidth;
    *packspeed = pci_ctrl->res.link_info.packspeed;
    if ((*bandwidth == 0) || (*packspeed == 0)) {
        devdrv_warn("Invalid bandwidth or packspeed. (devid=%u)\n", devid);
        return -ENODATA;
    }

    return 0;
}
EXPORT_SYMBOL(devdrv_get_theoretical_capability);

int devdrv_get_real_capability_ratio(u32 devid, u32 *bandwidth_ratio, u32 *packspeed_ratio)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if ((bandwidth_ratio == NULL) || (packspeed_ratio == NULL)) {
        devdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(devid);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    *bandwidth_ratio = pci_ctrl->res.link_info.bandwidth_ratio;
    *packspeed_ratio = pci_ctrl->res.link_info.packspeed_ratio;

    return 0;
}
EXPORT_SYMBOL(devdrv_get_real_capability_ratio);

#define TOPOLOGY_PIX 1
#define TOPOLOGY_PIB 2
#define TOPOLOGY_PHB 3
#define TOPOLOGY_SYS 4

static int devdrv_get_topology_in_vm_scene(u32 devid, u32 peer_devid, int *topo_type)
{
    u32 host_flag = 0;
    int ret;

    ret = devdrv_get_host_phy_mach_flag(devid, &host_flag);
    if (ret != 0) {
        devdrv_err("Get phy mach flag failed. (devid=%u;ret=%d)\n", devid, ret);
        return ret;
    }
    if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
        *topo_type = TOPOLOGY_SYS;
        return 0;
    }

    ret = devdrv_get_host_phy_mach_flag(peer_devid, &host_flag);
    if (ret != 0) {
        devdrv_err("Get phy mach flag failed. (peer_devid=%u;ret=%d)\n", peer_devid, ret);
        return ret;
    }
    if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
        *topo_type = TOPOLOGY_SYS;
        return 0;
    }
    return 0;
}

int devdrv_get_dev_topology(u32 devid, u32 peer_devid, int *topo_type)
{
    struct pci_dev *pdevA, *pdevB, *bridgeA, *bridgeB;
    int ret;

    if ((devid >= MAX_DEV_CNT) || (peer_devid >= MAX_DEV_CNT) || (topo_type == NULL)) {
        devdrv_err("Invalid devid or topo_type is NULL. (devid=%u;peer_devid=%u)\n", devid, peer_devid);
        return -EINVAL;
    }

    if (devid == peer_devid) {
        devdrv_err("Input devid and peer_devid is equal, invalid.(devid=%u;peer_devid=%u)\n", devid, peer_devid);
        return -EINVAL;
    }

    if ((devdrv_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_VF) ||
        (devdrv_get_pfvf_type_by_devid(peer_devid) == DEVDRV_SRIOV_TYPE_VF)) {
        *topo_type = TOPOLOGY_SYS;
        return 0;
    }

    ret = devdrv_get_topology_in_vm_scene(devid, peer_devid, topo_type);
    if ((ret != 0) || (*topo_type == TOPOLOGY_SYS)) {
        return ret;
    }

    pdevA = devdrv_get_pci_pdev_by_devid(devid);
    pdevB = devdrv_get_pci_pdev_by_devid(peer_devid);
    if ((pdevA == NULL) || (pdevB == NULL)) {
        devdrv_err("Get pdev failed. (%s=NULL;devid=%u;peer_devid=%u)\n",
            pdevA == NULL ? "pdevA" : "pdevB", devid, peer_devid);
        return -EINVAL;
    }

    bridgeA = pci_upstream_bridge(pdevA);
    bridgeB = pci_upstream_bridge(pdevB);
    if ((bridgeA != NULL) && (bridgeB != NULL)) {
        // If two devices in the same switch, return PIX
        if ((bridgeA->bus->self != NULL) && (bridgeA->bus->self == bridgeB->bus->self)) {
            *topo_type = TOPOLOGY_PIX;
            return 0;
        }
        // If two devices have no switch but in the same bus, return PHB
        if (bridgeA->bus == bridgeB->bus) {
            *topo_type = TOPOLOGY_PHB;
            return 0;
        }
    }

    // If two devices in the same NUMA, return PHB
    // In this way, PIB topology type is considered PHB topology type
    if ((dev_to_node(&pdevA->dev) != NUMA_NO_NODE) &&
        (dev_to_node(&pdevA->dev) == dev_to_node(&pdevB->dev))) {
        *topo_type = TOPOLOGY_PHB;
        return 0;
    }
    *topo_type = TOPOLOGY_SYS;
    return 0;
}
EXPORT_SYMBOL(devdrv_get_dev_topology);

int devdrv_get_p2p_addr(u32 dev_id, u32 remote_dev_id, enum devdrv_p2p_addr_type type,
    phys_addr_t *phy_addr, size_t *size)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if ((phy_addr == NULL) || (size == NULL)) {
        devdrv_err("Input phy_addr or size is NULL. (dev_id=%u; remote_dev_id=%u)\n", dev_id, remote_dev_id);
        return -EINVAL;
    }

    if (remote_dev_id >= DEVDRV_P2P_SUPPORT_MAX_DEVICE) {
        devdrv_err("Remote dev_id is invalid. (dev_id=%u; remote_dev_id=%u)\n", dev_id, remote_dev_id);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u; remote_dev_id=%u)\n", dev_id, remote_dev_id);
        return -EINVAL;
    }

    if (pci_ctrl->ops.get_p2p_addr == NULL) {
        devdrv_err("No support get p2p addr. (dev_id=%u; remote_dev_id=%u)\n", dev_id, remote_dev_id);
        return -EINVAL;
    }

    return pci_ctrl->ops.get_p2p_addr(pci_ctrl, remote_dev_id, type, phy_addr, size);
}
EXPORT_SYMBOL(devdrv_get_p2p_addr);

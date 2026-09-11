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

#include <linux/vmalloc.h>
#include <linux/log2.h>

#include "dvt.h"
#include "hw_vdavinci.h"
#include "mmio.h"
#include "domain_manage.h"
#include "vfio_ops.h"
#include "priv_ops.h"

static void hw_vdavinci_reset_debugfs_info(struct hw_vdavinci *vdavinci)
{
    int ret;
    size_t destmax_count = vdavinci->debugfs.nvec * sizeof(unsigned long long);

    if (vdavinci->debugfs.msix_count) {
        ret = memset_s(vdavinci->debugfs.msix_count, destmax_count, 0, destmax_count);
        if (ret)
            vascend_err(vdavinci_to_dev(vdavinci), "vdavinci reset msix_count failed, "
                "ret: %d\n", ret);
    }

    vdavinci->debugfs.notify_count = 0;
}

STATIC void hw_dvt_vdavinci_init(struct hw_vdavinci *vdavinci, struct hw_dvt *dvt,
                                 struct hw_vdavinci_type *type, unsigned int id)
{
    vdavinci->id = id;
    vdavinci->dvt = dvt;
    vdavinci->type = type;
    vdavinci->dev.dev = dvt->vdavinci_priv->dev;
    vdavinci->dev.dev_index = type->dev_index;
    vdavinci->dev.fid = vdavinci->id;
    vdavinci->dev.resource_dev = dvt->vdavinci_priv->dev;
    vdavinci->vf_index = -1;
    mutex_init(&vdavinci->vdavinci_lock);
}

LIST_HEAD(g_vdavinci_list);

STATIC struct list_head *get_vdavinci_list(void)
{
    return &g_vdavinci_list;
}

struct hw_vdavinci *find_vdavinci(struct device *dev)
{
    struct hw_vdavinci *vdavinci = NULL, *next = NULL;
    struct list_head *head = get_vdavinci_list();

    if (dev == NULL) {
        return NULL;
    }

    list_for_each_entry_safe(vdavinci, next, head, list) {
        if (vdavinci->dev.resource_dev == dev) {
            return vdavinci;
        }
    }
    return NULL;
}

STATIC void remove_vdavinci_node(struct hw_vdavinci *vdavinci)
{
    struct hw_vdavinci *vdavinci_node = NULL, *next = NULL;
    struct list_head *head = get_vdavinci_list();

    list_for_each_entry_safe(vdavinci_node, next, head, list) {
        if (vdavinci_node == vdavinci) {
            list_del(&vdavinci_node->list);
        }
    }
}

STATIC int hw_get_available_vf(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    unsigned int i;
    for (i = 0; i < dvt->sriov.vf_num; i++) {
        if (!dvt->sriov.vf_array[i].used) {
            vdavinci->vf_index = i;
            dvt->sriov.vf_array[i].used = true;
            dvt->sriov.vf_array[i].vdavinci = vdavinci;
            return i;
        }
    }

    return -EINVAL;
}

STATIC int hw_dvt_alloc_vf(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    struct device *dev = dvt->vdavinci_priv->dev;
    struct list_head *head = get_vdavinci_list();
    int index;

    if (dvt->sriov.vf_used >= dvt->sriov.vf_num) {
        vascend_err(dev, "No available VFs.\n");
        return -ENODEV;
    }
    index = hw_get_available_vf(vdavinci);
    if (index < 0) {
        vascend_err(dev, "No available VFs.\n");
        return -ENODEV;
    }
    vdavinci->vf.pdev = dvt->sriov.vf_array[index].vf;
    vdavinci->dev.resource_dev = &vdavinci->vf.pdev->dev;
    dvt->sriov.vf_used += 1;
    vdavinci->vf.irq_type = VFIO_PCI_NUM_IRQS;
    vdavinci->is_passthrough = true;
    list_add_tail(&(vdavinci->list), head);
    return 0;
}

STATIC int hw_dvt_reclaim_vf(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    int index;
    index = vdavinci->vf_index;
    if (index < 0) {
        return 0;
    }
    dvt->sriov.vf_array[index].vdavinci = NULL;
    dvt->sriov.vf_array[index].used = false;
    dvt->sriov.vf_used -= 1;
    remove_vdavinci_node(vdavinci);
    vdavinci->dev.resource_dev = NULL;
    vdavinci->vf.pdev = NULL;
    vdavinci->is_passthrough = false;
    return 0;
}

int init_vdavinci_type(struct hw_vdavinci_type *type,
                       struct vdavinci_type *tp)
{
    int ret = 0;

    if (type == NULL || tp == NULL) {
        return -EINVAL;
    }
    ret = snprintf_s(tp->template_name, HW_DVT_MAX_TYPE_NAME,
                     HW_DVT_MAX_TYPE_NAME - 1, "%s", type->template_name);
    if (ret < 0) {
        pr_err("vdavinci type init failed, ret: %d\n", ret);
        return ret;
    }
    tp->type = type->type;
    tp->bar0_size = type->bar0_size;
    tp->bar2_size = type->bar2_size;
    tp->bar4_size = type->bar4_size;
    tp->aicore_num = type->aicore_num;
    tp->mem_size = type->mem_size;
    tp->aicpu_num = type->aicpu_num;
    tp->vpc_num = type->vpc_num;
    tp->jpegd_num = type->jpegd_num;
    tp->jpege_num = type->jpege_num;
    tp->venc_num = type->venc_num;
    tp->vdec_num = type->vdec_num;
    tp->share = type->share;
    tp->ddrmem_size = type->ddrmem_size;
    tp->hbmmem_size = type->hbmmem_size;

    return 0;
}

STATIC int hw_dvt_ops_create_vdavinci(struct hw_vdavinci *vdavinci,
                                      struct hw_vdavinci_type *type,
                                      uuid_le uuid)
{
    struct vdavinci_type tp;
    int ret = 0;

    ret = init_vdavinci_type(type, &tp);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "init vdavinci type failed\n");
        return ret;
    }
    ret = vdavinci_priv_vdev_create(vdavinci, &tp, uuid);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "create vdavinci failed, call vdavinci_create failed, "
                    "pf : %u, vid: %u, ret: %d\n", vdavinci->dev.dev_index, vdavinci->id, ret);
        return ret;
    }

    return 0;
}

struct hw_vdavinci *hw_dvt_create_vdavinci(struct hw_dvt *dvt,
                                           struct hw_vdavinci_type *type, uuid_le uuid)
{
    struct hw_vdavinci *vdavinci;
    int ret;
    struct hw_pf_info *pf_info = &dvt->pf[type->dev_index];

    vdavinci = vzalloc(sizeof(*vdavinci));
    if (vdavinci == NULL) {
        return ERR_PTR(-ENOMEM);
    }

    ret = idr_alloc(&pf_info->vdavinci_idr, vdavinci, 0,
                    DVT_MAX_VDAVINCI, GFP_KERNEL);
    if (ret < 0)
        goto free_vdavinci;

    hw_dvt_vdavinci_init(vdavinci, dvt, type, (u32)ret);

    if (dvt->is_sriov_enabled) {
        ret = hw_dvt_alloc_vf(vdavinci);
        if (ret) {
            goto clean_idr;
        }
    }

    ret = dvt->mmio_init(vdavinci);
    if (ret) {
        goto put_vf;
    }

    hw_vdavinci_init_cfg_space(vdavinci);
    ret = hw_dvt_ops_create_vdavinci(vdavinci, type, uuid);
    if (ret != 0) {
        goto mmio_uninit;
    }
    hw_dvt_debugfs_add_vdavinci(vdavinci);
    mutex_init(&vdavinci->ioeventfds_lock);
    INIT_LIST_HEAD(&vdavinci->ioeventfds_list);

    return vdavinci;

mmio_uninit:
    dvt->mmio_uninit(vdavinci);
put_vf:
    hw_dvt_reclaim_vf(vdavinci);
clean_idr:
    idr_remove(&pf_info->vdavinci_idr, vdavinci->id);
free_vdavinci:
    vfree(vdavinci);
    return ERR_PTR(ret);
}

void hw_dvt_destroy_vdavinci(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    struct hw_pf_info *pf_info = &dvt->pf[vdavinci->dev.dev_index];

    vdavinci_priv_vdev_destroy(vdavinci);
    dvt->mmio_uninit(vdavinci);
    hw_dvt_reclaim_vf(vdavinci);
    idr_remove(&pf_info->vdavinci_idr, vdavinci->id);

    hw_dvt_debugfs_remove_vdavinci(vdavinci);
    mutex_destroy(&vdavinci->ioeventfds_lock);

    if (vdavinci->debugfs.msix_count) {
        kfree(vdavinci->debugfs.msix_count);
        vdavinci->debugfs.msix_count = NULL;
    }
    vfree(vdavinci);
}

void hw_dvt_release_vdavinci(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;

    hw_dvt_deactivate_vdavinci(vdavinci);

    mutex_lock(&vdavinci->vdavinci_lock);
    if (dvt->vdavinci_priv->ops &&
        dvt->vdavinci_priv->ops->vdavinci_release) {
        dvt->vdavinci_priv->ops->vdavinci_release(&vdavinci->dev);
    }

    mutex_unlock(&vdavinci->vdavinci_lock);
}

int hw_dvt_reset_vdavinci(struct hw_vdavinci *vdavinci)
{
    int ret = -1;

    vascend_info(vdavinci_to_dev(vdavinci),
                 "enter reset vdavinci, pf : %u, vid: %u\n", vdavinci->dev.dev_index, vdavinci->id);

    mutex_lock(&vdavinci->vdavinci_lock);
    ret = vdavinci_priv_vdev_reset(vdavinci);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "reset vdavinci failed, call vdavinci_reset failed, "
                    "pf : %u, vid: %u, ret: %d\n", vdavinci->dev.dev_index, vdavinci->id, ret);
        goto out;
    }

    hw_vdavinci_reset_mmio(vdavinci);
    hw_vdavinci_reset_cfg_space(vdavinci);
    hw_vdavinci_reset_debugfs_info(vdavinci);

    vascend_info(vdavinci_to_dev(vdavinci),
                 "leave reset vdavinci, pf : %u, vid: %u\n", vdavinci->dev.dev_index, vdavinci->id);

out:
    mutex_unlock(&vdavinci->vdavinci_lock);
    return ret;
}

void hw_dvt_activate_vdavinci(struct hw_vdavinci *vdavinci)
{
    mutex_lock(&vdavinci->dvt->lock);
    vdavinci->active = true;
    mutex_unlock(&vdavinci->dvt->lock);
}

void hw_dvt_deactivate_vdavinci(struct hw_vdavinci *vdavinci)
{
    mutex_lock(&vdavinci->dvt->lock);
    vdavinci->active = false;
    mutex_unlock(&vdavinci->dvt->lock);
}

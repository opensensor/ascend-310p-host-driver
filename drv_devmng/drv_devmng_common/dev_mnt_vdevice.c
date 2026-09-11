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

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <linux/version.h>

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"

#include "dbl/uda.h"
#include "devdrv_user_common.h"
#include "devdrv_manager.h"
#include "devdrv_common.h"
#include "devdrv_pcie.h"
#include "dev_mnt_vdevice.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_container.h"
#include "dbl_mem_alloc_interface.h"
#include "virtmng_interface.h"
#include "dms_event_distribute.h"

#ifndef CFG_FEATURE_RC_MODE
#include "hvdevmng_init.h"
#include "devdrv_manager_msg.h"
#endif

#define DEVDRV_DESTROY_ALL_VDEVICE 0xffff
#define VDEVMNG_AICPU_BITMAP_LEN 16

#ifdef CFG_FEATURE_VF_SINGLE_AICORE
#define VDEV_TOKEN_TIME_1MS 0xFFFFFULL
#define VDEV_TOKEN_TIME_2MS 0x1FFFFFULL
#define VDEV_TASK_TIMEOUT   0x3200000000 /* It is a fixed value. */
#define VDEV_PF_AICORE_NUM 1
#define MAX_SPLIT_PIECE 4

#define VDEV_TOKEN_MAP_VALUE(x) ((x) >> 20)

struct dev_mnt_vdev_pieces {
    u32 splitted_total;
    u8 vdev_pieces[MAX_VDEV_NUM];
};

struct dev_mnt_vdev_pieces g_split_details = {0};
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define VDAVINCI_SUPPORT_DIVIDE_AICORE_NUM 25
const u32 g_vdev_dtype[VDAVINCI_SUPPORT_DIVIDE_AICORE_NUM + 1] = {0};
#elif (defined CFG_FEATURE_VFIO) && (defined CFG_FEATURE_RC_MODE)
const u32 g_vdev_dtype[VDAVINCI_MAX_VFID_NUM + 1] = {
    MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_C1_4, MNT_VDAVINCI_TYPE_C2_4, MNT_VDAVINCI_TYPE_MAX,
    MNT_VDAVINCI_TYPE_MAX
};
#else
#define VDAVINCI_SUPPORT_DIVIDE_AICORE_NUM 16
const u32 g_vdev_dtype[VDAVINCI_SUPPORT_DIVIDE_AICORE_NUM + 1] = {
    MNT_VDAVINCI_TYPE_MAX,  MNT_VDAVINCI_TYPE_C1,  MNT_VDAVINCI_TYPE_C2,  MNT_VDAVINCI_TYPE_MAX,
    MNT_VDAVINCI_TYPE_C4,  MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_MAX,
    MNT_VDAVINCI_TYPE_C8,  MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_MAX,
    MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_MAX, MNT_VDAVINCI_TYPE_MAX,
    MNT_VDAVINCI_TYPE_C16
};
#endif

STATIC struct dev_mnt_vdev_inform g_vdev_inform;
STATIC DEV_MNT_VDEV_CB_T *g_mnt_vdev_cb = NULL;
static struct file_operations dev_mnt_vdev_fops_real = {
    .owner = KA_THIS_MODULE,
    .open = NULL,
    .release = NULL,
    .unlocked_ioctl = NULL,
    .mmap = NULL,
};

STATIC void dev_mnt_vdevice_inform_release(void);

void dev_mnt_vdev_set_ops(const struct file_operations *ops)
{
    dev_mnt_vdev_fops_real.open = ops->open;
    dev_mnt_vdev_fops_real.release = ops->release;
    dev_mnt_vdev_fops_real.unlocked_ioctl = ops->unlocked_ioctl;
    dev_mnt_vdev_fops_real.mmap = ops->mmap;
}

void dev_mnt_vdev_unset_ops(void)
{
    dev_mnt_vdev_fops_real.open = NULL;
    dev_mnt_vdev_fops_real.release = NULL;
    dev_mnt_vdev_fops_real.unlocked_ioctl = NULL;
    dev_mnt_vdev_fops_real.mmap = NULL;
}

STATIC inline u32 dev_mnt_vdevice_get_vdev_id(u32 phy_id, u32 vfid)
{
    return ((phy_id * VDAVINCI_MAX_VFID_NUM) + (vfid - 1));
}

STATIC inline int dev_mnt_vdevice_get_aicore_num(u32 phy_id, u32 *ai_core_num)
{
    struct devdrv_info *info = NULL;

    info = devdrv_manager_get_devdrv_info(phy_id);
    if (info == NULL) {
        devdrv_drv_err("get device info failed, phy_id:%u\n", phy_id);
        return -EINVAL;
    }
    *ai_core_num = info->ai_core_num;
    return 0;
}

STATIC inline u32 dev_mnt_vdevice_get_aicore_used(u32 phy_id)
{
    u32 next_count = (phy_id + 1) * VDAVINCI_MAX_VFID_NUM;
    u32 core_count = 0;
    u32 i;

    for (i = (phy_id * VDAVINCI_MAX_VFID_NUM); i < next_count; i++) {
        if (g_mnt_vdev_cb->vdev[i].alloc_state == VDAVINCI_USED) {
            core_count += g_mnt_vdev_cb->vdev[i].info.spec_info.core_num;
        }
    }

    return core_count;
}

STATIC inline u32 dev_mnt_vdevice_get_vdevice_alloced(u32 phy_id)
{
    u32 next_count = (phy_id + 1) * VDAVINCI_MAX_VFID_NUM;
    u32 vdev_num = 0;
    u32 i;

    for (i = (phy_id * VDAVINCI_MAX_VFID_NUM); i < next_count; i++) {
        if (g_mnt_vdev_cb->vdev[i].alloc_state == VDAVINCI_USED) {
            vdev_num++;
        }
    }

    return vdev_num;
}

#ifdef CFG_FEATURE_VFIO
STATIC struct vdavinci_info *dev_mnt_vdevice_alloc(u32 phy_id, u32 core_num,
    u32 vfid, u32 *vdev_id)
{
    u32 vdev_index;

    vdev_index = dev_mnt_vdevice_get_vdev_id(phy_id, vfid);
    mutex_lock(&g_mnt_vdev_cb->vdev[vdev_index].lock);
    if (g_mnt_vdev_cb->vdev[vdev_index].alloc_state == VDAVINCI_IDLE) {
        g_mnt_vdev_cb->vdev[vdev_index].alloc_state = VDAVINCI_INIT;
        g_mnt_vdev_cb->vdev[vdev_index].info.phy_devid = phy_id;
        g_mnt_vdev_cb->vdev[vdev_index].info.vfid = vfid;
        g_mnt_vdev_cb->vdev[vdev_index].info.spec_info.core_num = core_num;
        *vdev_id = vdev_index;
        mutex_unlock(&g_mnt_vdev_cb->vdev[vdev_index].lock);
        return &g_mnt_vdev_cb->vdev[vdev_index].info;
    }
    mutex_unlock(&g_mnt_vdev_cb->vdev[vdev_index].lock);
    return NULL;
}

STATIC void dev_mnt_vdevice_free(u32 vdev_index)
{
    int ret;
    struct vdavinci_info *info;
    info = &g_mnt_vdev_cb->vdev[vdev_index].info;

    g_mnt_vdev_cb->vdev[vdev_index].alloc_state = VDAVINCI_IDLE;
    ret = memset_s((void *)info, sizeof(struct vdavinci_info), 0, sizeof(struct vdavinci_info));
    if (ret) {
        devdrv_drv_err("memset_s failed, ret(%d).\n", ret);
    }
    return;
}
#endif

#ifdef CFG_FEATURE_VFIO
STATIC int dev_mnt_destory_vdevice_single(u32 phy_id, u32 vdev_id)
{
    struct vdavinci_info *info_cb = NULL;
    int ret;

    /* check para */
    if (vdev_id >= MAX_VDEV_NUM) {
        return -EINVAL;
    }

    info_cb = &g_mnt_vdev_cb->vdev[vdev_id].info;
    mutex_lock(&g_mnt_vdev_cb->vdev[vdev_id].lock);
    if (g_mnt_vdev_cb->vdev[vdev_id].alloc_state == VDAVINCI_IDLE) {
        mutex_unlock(&g_mnt_vdev_cb->vdev[vdev_id].lock);
        devdrv_drv_warn("vdev_id(%u) is not alloced.\n", VDAVINCI_VDEV_OFFSET_PLUS(vdev_id));
        return -ENODEV;
    }

    if ((info_cb->phy_devid != phy_id) ||
        (g_mnt_vdev_cb->vdev[vdev_id].info.used == VDAVINCI_STATE_INUSED)) {
        mutex_unlock(&g_mnt_vdev_cb->vdev[vdev_id].lock);
            devdrv_drv_warn("vdev_id(%u) is inused. phy_id(%u), info_phy_id(%d)\n",
                VDAVINCI_VDEV_OFFSET_PLUS(vdev_id), phy_id, info_cb->phy_devid);
        return -EINVAL;
    }

    /*
     * @attention: vmng_destroy_container_vdev will call
     *             dev_mnt_vdev_unregister_client function
     */
#ifdef CFG_FEATURE_RC_MODE
    ret = vmng_destory_container_vdev(phy_id, g_mnt_vdev_cb->vdev[vdev_id].info.vfid);
#else
    ret = vmngh_destory_container_vdev(phy_id, g_mnt_vdev_cb->vdev[vdev_id].info.vfid);
#endif
    if (ret != 0) {
        mutex_unlock(&g_mnt_vdev_cb->vdev[vdev_id].lock);
        devdrv_drv_err("destroy_container failed, phy_id:%u,vfid:%u,ret:%d\n", phy_id,
            g_mnt_vdev_cb->vdev[vdev_id].info.vfid, ret);
        return ret;
    }
    dev_mnt_vdevice_free(vdev_id);
#ifdef CFG_FEATURE_VF_SINGLE_AICORE
    g_split_details.splitted_total -= g_split_details.vdev_pieces[vdev_id];
    g_split_details.vdev_pieces[vdev_id] = 0;
#endif
    mutex_unlock(&g_mnt_vdev_cb->vdev[vdev_id].lock);
    return 0;
}

STATIC int dev_mnt_destory_all_vdevice(u32 phy_id)
{
    u32 next_count = (phy_id + 1) * VDAVINCI_MAX_VFID_NUM;
    bool destroy_succ = true;
    int ret;
    u32 i;

    mutex_lock(&g_mnt_vdev_cb->global_lock);
    for (i = (phy_id * VDAVINCI_MAX_VFID_NUM); i < next_count; i++) {
        ret = dev_mnt_destory_vdevice_single(phy_id, i);
        if ((ret != 0) && (ret != -ENODEV)) {
            destroy_succ = false;
            continue;
        }
    }
    mutex_unlock(&g_mnt_vdev_cb->global_lock);
    if (destroy_succ) {
        return 0;
    }
    devdrv_drv_err("all vdevice of dev(%u) are not alloced.\n", phy_id);
    return -ENODEV;
}
#endif

int dev_mnt_create_one_vm_vdevice(unsigned int phy_id, unsigned int fid)
{
    u32 vdev_id = phy_id * VDAVINCI_MAX_VFID_NUM + (fid - 1);
    g_mnt_vdev_cb->vdev[vdev_id].alloc_state = VDAVINCI_INIT;
    g_mnt_vdev_cb->vdev[vdev_id].info.phy_devid = phy_id;
    g_mnt_vdev_cb->vdev[vdev_id].info.vfid = fid;

    return 0;
}

void dev_mnt_released_one_vm_vdevice(unsigned int phy_id, unsigned int fid)
{
    u32 vdev_id = phy_id * VDAVINCI_MAX_VFID_NUM + (fid - 1);

    if (!fid) {
        return; // invalid fid
    }
    g_mnt_vdev_cb->vdev[vdev_id].alloc_state = VDAVINCI_IDLE;
}

#ifdef CFG_FEATURE_VFIO
static bool dev_mnt_check_vdev_id_is_valid(u32 vdev_id, u32 phy_id)
{
    if (vdev_id > VDAVINCI_MAX_VDEV_ID || vdev_id < VDAVINCI_VDEV_OFFSET) {
        return false;
    }
    if (phy_id != ((vdev_id - VDAVINCI_VDEV_OFFSET) / VDAVINCI_MAX_VFID_NUM)) {
        return false;
    }

    return true;
}

static int dev_mnt_get_vfid_from_vdevid(u32 vdev_id, u32 phy_id, u32 *vfid)
{
    if (vdev_id == (u32)-1) { // if manager do not assign vdev_id
        *vfid = 0;
        return 0;
    }

    if (dev_mnt_check_vdev_id_is_valid(vdev_id, phy_id)) {
        *vfid = (vdev_id - VDAVINCI_VDEV_OFFSET) % VDAVINCI_MAX_VFID_NUM + 1;
    } else {
        devdrv_drv_err("input vdev_id is invalid, (vdevid=%d, phyid=%d)\n", vdev_id, phy_id);
        return -EINVAL;
    }

    return 0;
}

static void dev_mnt_config_vf_info_for_vmng(struct vmng_vf_res_info *vf_info, const struct vdev_create_info *vinfo)
{
    (void)strcpy_s(vf_info->name, VMNG_VF_TEMP_NAME_LEN, vinfo->vdev_info.name);
    vf_info->vfg.vfg_id = vinfo->vdev_info.base.vfg_id;
    vf_info->stars_static.aic = vinfo->spec.core_num;
    vf_info->stars_refresh.device_aicpu = vinfo->vdev_info.computing.device_aicpu;

    vf_info->stars_refresh.jpegd = vinfo->vdev_info.media.jpegd;
    vf_info->stars_refresh.jpege = vinfo->vdev_info.media.jpege;
    vf_info->stars_refresh.vpc = vinfo->vdev_info.media.vpc;
    vf_info->stars_refresh.vdec = vinfo->vdev_info.media.vdec;
    vf_info->stars_refresh.pngd = vinfo->vdev_info.media.pngd;
    vf_info->stars_refresh.venc = vinfo->vdev_info.media.venc;
    vf_info->memory.size = vinfo->vdev_info.computing.memory_size;
    vf_info->vfg.vfg_refresh.token = vinfo->vdev_info.base.token;
    vf_info->vfg.vfg_refresh.token_max = vinfo->vdev_info.base.token_max;
    vf_info->vfg.vfg_refresh.task_timeout = vinfo->vdev_info.base.task_timeout;
}

STATIC int dev_mnt_create_vdevice_single(u32 phy_id, struct vdev_create_info *vinfo)
{
    struct vdavinci_info *info = NULL;
    u32 vfid = 0;
    u32 dtype;
    int ret;
    struct vmng_vf_res_info vf_info;
    unsigned int core_num = vinfo->spec.core_num;

#ifdef CFG_FEATURE_VF_SINGLE_AICORE
    dtype = g_vdev_dtype[VDEV_TOKEN_MAP_VALUE(vinfo->vdev_info.base.token) + 1];
#else
    dtype = g_vdev_dtype[core_num];
#endif
    if (dev_mnt_get_vfid_from_vdevid(vinfo->vdevid, phy_id, &vfid) != 0) {
        devdrv_drv_err("get vfid from vdevid failed, (vdevid=%d, phyid=%d)\n", vinfo->vdevid, phy_id);
        return -EINVAL;
    }

    /*
     * @attention: vmng_create_container_vdev will call
     *             dev_mnt_vdev_register_client function
     */
    (void)memset_s(&vf_info, sizeof(struct vmng_vf_res_info), 0, sizeof(struct vmng_vf_res_info));
    dev_mnt_config_vf_info_for_vmng(&vf_info, vinfo);
#ifdef CFG_FEATURE_RC_MODE
    ret = vmng_create_container_vdev(phy_id, dtype, &vfid, &vf_info);
#else
    ret = vmngh_create_container_vdev(phy_id, dtype, &vfid, &vf_info);
#endif
    if (ret != 0 || vfid == 0 || vfid > VDAVINCI_MAX_VFID_NUM) {
        devdrv_drv_err("create vfid failed, phy_id:%u, core_num:%u, vfid:%u, ret:%d\n",
            phy_id, core_num, vfid, ret);
        return (ret == -EBUSY ? -EBUSY : -EINVAL);
    }
    vinfo->vdev_info.base.vfg_id = vf_info.vfg.vfg_id;

    info = dev_mnt_vdevice_alloc(phy_id, core_num, vfid, &(vinfo->vdevid));
    if (info == NULL) {
#ifdef CFG_FEATURE_RC_MODE
        vmng_destory_container_vdev(phy_id, vfid);
#else
        vmngh_destory_container_vdev(phy_id, vfid);
#endif
        devdrv_drv_err("alloc vdevice failed, phy_id:%u, core_num:%u, ret:%d\n",
            phy_id, core_num, ret);
        return -EINVAL;
    }

    mutex_lock(&g_mnt_vdev_cb->vdev[vinfo->vdevid].lock);
    g_mnt_vdev_cb->vdev[vinfo->vdevid].alloc_state = VDAVINCI_USED;
    mutex_unlock(&g_mnt_vdev_cb->vdev[vinfo->vdevid].lock);
    return 0;
}

STATIC int dev_mnt_destory_vdevice(u32 phy_id, u32 vdev_id)
{
    int ret = -EINVAL;
    u32 offset_id;

    if (vdev_id == DEVDRV_DESTROY_ALL_VDEVICE) {
        ret = dev_mnt_destory_all_vdevice(phy_id);
    } else {
        offset_id = VDAVINCI_VDEV_OFFSET_SUB(vdev_id);
        mutex_lock(&g_mnt_vdev_cb->global_lock);
        ret = dev_mnt_destory_vdevice_single(phy_id, offset_id);
        mutex_unlock(&g_mnt_vdev_cb->global_lock);
    }

    return ret;
}
#endif
int dev_mnt_vdevice_logical_id_to_phy_id(u32 logical_id, u32 *phy_id, u32 *vfid)
{
    u32 vdev_id = 0;
    int ret;

    ret = uda_devid_to_udevid(logical_id, &vdev_id);
    if (ret != 0) {
        devdrv_drv_err("get vdevid failed, logical_id:%u, ret:%d\n", logical_id, ret);
        return ret;
    }

    ret = devmng_get_vdavinci_info(vdev_id, phy_id, vfid);
    if (ret != 0) {
        devdrv_drv_err("get phy id failed, logical_id:%u,vdev_id:%d, ret:%d\n",
                       logical_id, vdev_id, ret);
        return ret;
    }

    if (*phy_id >= DEVDRV_MAX_DAVINCI_NUM) {
        devdrv_drv_err("physical_dev_id(%u) is invalid.\n", *phy_id);
        return -ENODEV;
    }
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dev_mnt_vdevice_logical_id_to_phy_id);

#ifdef CFG_FEATURE_VFIO
STATIC inline void dev_mng_get_media_info(struct devdrv_media_resource_vdev *media,
                                          struct vmng_stars_res_refresh *stars_refresh)
{
    media->jpegd = stars_refresh->jpegd;
    media->jpege = stars_refresh->jpege;
    media->vpc = stars_refresh->vpc;
    media->vdec = stars_refresh->vdec;
    media->pngd = stars_refresh->pngd;
    media->venc = stars_refresh->venc;
}

#define WAIT_VDEV_READY_TIMEOUT_SECOND 4
STATIC int get_single_vdev_info(struct vdev_query_info *query_info, struct vmng_soc_resource_enquire *vm_info)
{
    unsigned long aicpu_bitmap = 0;
    struct vmng_stars_res_refresh *stars_refresh;
#ifdef CFG_FEATURE_VDEV_MEMORY_FROM_DEVMNG_H2D
    struct devdrv_manager_msg_resource_info info = {0};

    info.info_type = DEVDRV_DEV_HBM_TOTAL;
    (void)hvdevmng_get_dev_resource(query_info->vdev_id_single, 0, &info);
    query_info->computing.memory_size = (info.value >> 10ULL); /* 10: KB to MB */
#else
    query_info->computing.memory_size = vm_info->each.memory.size;
#endif
    stars_refresh = &vm_info->each.stars_refresh;
    aicpu_bitmap = (unsigned long)stars_refresh->device_aicpu;
    query_info->computing.device_aicpu = bitmap_weight(&aicpu_bitmap, VDEVMNG_AICPU_BITMAP_LEN);
    query_info->base.vfg_id = vm_info->each.vfg.vfg_id;
    query_info->base.vip_mode = vm_info->each.vfg.vfg_type;
    query_info->base.token = vm_info->each.vfg.vfg_refresh.token;
    query_info->base.token_max = vm_info->each.vfg.vfg_refresh.token_max;
    query_info->base.task_timeout = vm_info->each.vfg.vfg_refresh.task_timeout;
    dev_mng_get_media_info(&query_info->media, stars_refresh);

    if (strcpy_s(query_info->name, DSMI_VDEV_RES_NAME_LEN, vm_info->each.name)) {
        devdrv_drv_err("Call strcpy_s failed.\n");
        return -EINVAL;
    }
    return 0;
}

STATIC int get_device_total_info(u32 phy_id, struct vdev_query_info *query_info,
    struct vmng_soc_resource_enquire *vm_info)
{
    struct vmng_stars_res_refresh *stars_refresh;
    unsigned long aicpu_bitmap = 0;

#ifdef CFG_FEATURE_VDEV_MEMORY_FROM_DEVMNG_H2D
    int ret;
    struct devdrv_manager_msg_resource_info info = {0};

    info.info_type = DEVDRV_DEV_HBM_TOTAL;
    ret = hvdevmng_get_dev_resource(phy_id, 0, &info);
    if (ret != 0) {
        devdrv_drv_err("Get vdev memory total resource failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    query_info->computing.memory_size = (info.value_ext >> 10ULL); /* 10: KB to MB */
#else
    query_info->computing.memory_size = vm_info->total.base.memory;
#endif
    stars_refresh = &vm_info->total.stars_refresh;
    aicpu_bitmap = (unsigned long)stars_refresh->device_aicpu;
    query_info->computing.device_aicpu = bitmap_weight(&aicpu_bitmap, VDEVMNG_AICPU_BITMAP_LEN);
    dev_mng_get_media_info(&query_info->media, stars_refresh);

    return 0;
}

#define DEV_MNT_VFREE(p) \
    do {                     \
        dbl_vfree(p);            \
        p = NULL;            \
    } while (0)

STATIC int get_device_free_memory_info(u32 phy_id, struct vdev_query_info *query_info,
    struct vmng_soc_resource_enquire *vm_info)
{
#ifdef CFG_FEATURE_VDEV_MEMORY_FROM_DEVMNG_H2D
    int ret, i;
    u32 vdev_num = 0;
    u32 *vdevices = NULL;
    u64 memory_total;
    u64 memory_used = 0;
    struct devdrv_manager_msg_resource_info info_tmp = {0};

    info_tmp.info_type = DEVDRV_DEV_HBM_TOTAL;
    ret = hvdevmng_get_dev_resource(phy_id, 0, &info_tmp);
    if (ret != 0) {
        devdrv_drv_err("Get dev memory total resource failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    memory_total = info_tmp.value_ext;
    vdevices = (u32 *)dbl_vzalloc(sizeof(u32) * VDAVINCI_MAX_VFID_NUM);
    if (vdevices == NULL) {
        devdrv_drv_err("Alloc memory for vdevice failed. (phy_id=%u)\n", phy_id);
        return -ENOMEM;
    }

    mutex_lock(&g_mnt_vdev_cb->global_lock);
    for (i = (phy_id * VDAVINCI_MAX_VFID_NUM); i < (phy_id + 1) * VDAVINCI_MAX_VFID_NUM; i++) {
        if (g_mnt_vdev_cb->vdev[i].alloc_state == VDAVINCI_USED) {
            vdevices[vdev_num] = VDAVINCI_VDEV_OFFSET_PLUS(i);
            vdev_num++;
        }
    }

    mutex_unlock(&g_mnt_vdev_cb->global_lock);
    for (i = 0; i < vdev_num; i++) {
        info_tmp.info_type = DEVDRV_DEV_HBM_TOTAL;
        ret = hvdevmng_get_dev_resource(vdevices[i], 0, &info_tmp);
        if (ret != 0) {
            devdrv_drv_err("Get vdev memory total resource failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
            DEV_MNT_VFREE(vdevices);
            return ret;
        }

        memory_used += info_tmp.value;
    }

    if (memory_used > memory_total) {
        devdrv_drv_err("Memory used is larger than total. (phy_id=%u; used=%lld; total=%lld)\n",
            phy_id, memory_used, memory_total);
        DEV_MNT_VFREE(vdevices);
        return -EINVAL;
    }

    query_info->computing.memory_size = ((memory_total - memory_used) >> 10ULL); /* 10: KB to MB */
    DEV_MNT_VFREE(vdevices);
#else
    query_info->computing.memory_size = vm_info->remain.base.memory;
#endif

    return 0;
}

STATIC int get_device_free_info(u32 phy_id, struct vdev_query_info *query_info,
    struct vmng_soc_resource_enquire *vm_info)
{
    int ret;
    struct vmng_stars_res_refresh *stars_refresh;
    unsigned long aicpu_bitmap = 0;

    ret = get_device_free_memory_info(phy_id, query_info, vm_info);
    if (ret != 0) {
        devdrv_drv_err("Get free memory info failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    stars_refresh = &vm_info->remain.stars_refresh;
    aicpu_bitmap = (unsigned long)stars_refresh->device_aicpu;
    query_info->computing.device_aicpu = bitmap_weight(&aicpu_bitmap, VDEVMNG_AICPU_BITMAP_LEN);
    dev_mng_get_media_info(&query_info->media, stars_refresh);

    return 0;
}

#ifndef CFG_FEATURE_RC_MODE
STATIC int get_single_vdev_activity(u32 phy_id, struct vdev_query_info *query_info)
{
    int ret;
    u32 vfid = 0;
    u32 dev_id = query_info->vdev_id_single;
    u32 phys_id = DEVDRV_MAX_DAVINCI_NUM + 1;
    struct devdrv_core_utilization util_info = {0};

    ret = devdrv_manager_container_logical_id_to_physical_id(dev_id, &phys_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Transform virt id failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return -EFAULT;
    }

    util_info.dev_id = phys_id;
    util_info.vfid = vfid;
    ret = devdrv_manager_h2d_sync_get_core_utilization(&util_info);
    if (ret != 0) {
        devdrv_drv_err("Core utilization get failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    query_info->computing.vdev_aicore_utilization = util_info.utilization;

    return 0;
}
#endif

STATIC int dev_mnt_get_computing_info(u32 phy_id, struct vdev_query_info *query_info,
    struct vmng_soc_resource_enquire *vm_info)
{
    int ret;

    switch (query_info->cmd_type) {
        case DEV_VMNG_SUB_CMD_GET_VDEV_RESOURCE:
        case DEV_VMNG_SUB_CMD_GET_VDEV_TOPS_PERCENTAGE:
            ret = get_single_vdev_info(query_info, vm_info);
            break;
        case DEV_VMNG_SUB_CMD_GET_TOTAL_RESOURCE:
            ret = get_device_total_info(phy_id, query_info, vm_info);
            break;
        case DEV_VMNG_SUB_CMD_GET_FREE_RESOURCE:
            ret = get_device_free_info(phy_id, query_info, vm_info);
            break;
#ifndef CFG_FEATURE_RC_MODE
        case DEV_VMNG_SUB_CMD_GET_VDEV_ACTIVITY:
            ret = get_single_vdev_activity(phy_id, query_info);
            break;
#endif
        default:
            devdrv_drv_err("Invalid query type.(phy_id=%u; sub_cmd=%u)\n", phy_id, query_info->cmd_type);
            return -EINVAL;
    }

    if (ret != 0) {
        devdrv_drv_err("Get computing info failed.(phy_id=%u; sub_cmd=%u, ret=%d)\n",
            phy_id, query_info->cmd_type, ret);
        return ret;
    }
    return 0;
}

STATIC int dev_mnt_get_vdevice_info(u32 phy_id, struct vdev_query_info *vdev_info)
{
    u32 next_count = (phy_id + 1) * VDAVINCI_MAX_VFID_NUM;
    struct vdev_info_st *p = NULL;
    u32 core_total, i;
    u32 core_used = 0;
    int ret;
    struct vmng_soc_resource_enquire *vm_info = NULL;
#ifdef CFG_FEATURE_SRIOV
    u32 vf_dev_id = 0;
#endif

    vm_info = dbl_kzalloc(sizeof(struct vmng_soc_resource_enquire), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (vm_info == NULL) {
        devdrv_drv_err("Kzalloc failed. (phy_id=%u)\n", phy_id);
        return -ENOMEM;
    }

    /* check phyid vdev_info */
    p = vdev_info->vdev;
    vdev_info->vdev_num = 0;
    core_total = 0;
    ret = dev_mnt_vdevice_get_aicore_num(phy_id, &core_total);
    if (ret != 0) {
        devdrv_drv_err("get device info failed, phy_id:%u,ret:%d\n", phy_id, ret);
        ret = -EINVAL;
        goto VM_INFO_KFREE;
    }

    (void)memset_s(vm_info, sizeof(struct vmng_soc_resource_enquire), 0, sizeof(struct vmng_soc_resource_enquire));
#ifdef CFG_FEATURE_SRIOV
    ret = devdrv_get_devid_by_pfvf_id(phy_id, vdev_info->vfid, &vf_dev_id);
    if (ret != 0) {
        devdrv_drv_err("Get vf device id failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        goto VM_INFO_KFREE;
    }

    ret = vmngh_enquire_soc_resource(vf_dev_id, 0, vm_info);
#elif (defined CFG_FEATURE_VFIO) && (defined CFG_FEATURE_RC_MODE)
    ret = vmngd_enquire_soc_resource(phy_id, vdev_info->vfid, vm_info);
#else
    ret = vmngh_enquire_soc_resource(phy_id, vdev_info->vfid, vm_info);
#endif
    if (ret) {
        devdrv_drv_err("Get vmngh_enquire_soc_resource info failed; (phy_id=%u; ret=%d)\n", phy_id, ret);
        ret = -EINVAL;
        goto VM_INFO_KFREE;
    }

    ret = dev_mnt_get_computing_info(phy_id, vdev_info, vm_info);
    if (ret != 0) {
        goto VM_INFO_KFREE;
    }

    mutex_lock(&g_mnt_vdev_cb->global_lock);
    for (i = (phy_id * VDAVINCI_MAX_VFID_NUM); i < next_count; i++) {
        if (i >= MAX_VDEV_NUM) {
            break;
        }
        if (g_mnt_vdev_cb->vdev[i].alloc_state == VDAVINCI_USED) {
            vdev_info->vdev_num++;
            p->status = g_mnt_vdev_cb->vdev[i].info.used;
            p->id = VDAVINCI_VDEV_OFFSET_PLUS(i);
            p->vfid = g_mnt_vdev_cb->vdev[i].info.vfid;
            p->cid = g_mnt_vdev_cb->vdev[i].info.container_id;
            p->spec.core_num = g_mnt_vdev_cb->vdev[i].info.spec_info.core_num;
            core_used += g_mnt_vdev_cb->vdev[i].info.spec_info.core_num;
            p++;
        }
    }
#ifdef CFG_FEATURE_VF_SINGLE_AICORE
    core_used = ((g_split_details.splitted_total == 0) ? 0 : VDEV_PF_AICORE_NUM);
#endif
    mutex_unlock(&g_mnt_vdev_cb->global_lock);
    if (core_used > core_total) {
        devdrv_drv_err("ai core num error,phyid:%u core_used:%u,total:%d\n", phy_id, core_used, core_total);
        ret = -EINVAL;
        goto VM_INFO_KFREE;
    }
#ifdef CFG_FEATURE_VF_SINGLE_AICORE
    vdev_info->aic_unused = ((g_split_details.splitted_total == MAX_SPLIT_PIECE) ? 0 : core_total);
#else
    vdev_info->aic_unused = core_total - core_used;
#endif
    vdev_info->aic_total = core_total;

VM_INFO_KFREE:
    dbl_kfree(vm_info);
    vm_info = NULL;
    return ret;
}
#endif

int dev_mnt_vdevice_init(void)
{
    int i;

    g_mnt_vdev_cb = dbl_kzalloc(sizeof(DEV_MNT_VDEV_CB_T), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_mnt_vdev_cb == NULL) {
        devdrv_drv_err("stream alloc_chrdev_region failed.\n");
        return -ENOMEM;
    }

    ka_task_mutex_init(&g_mnt_vdev_cb->global_lock);
    for (i = 0; i < MAX_VDEV_NUM; i++) {
        ka_task_mutex_init(&g_mnt_vdev_cb->vdev[i].lock);
    }

    INIT_LIST_HEAD(&g_vdev_inform.vdev_action_head);
    ka_task_spin_lock_init(&g_vdev_inform.spinlock);
    return 0;
}

void dev_mnt_vdevice_uninit(void)
{
    u32 i;

    if (g_mnt_vdev_cb == NULL) {
        return;
    }

    mutex_lock(&g_mnt_vdev_cb->global_lock);
    for (i = 0; i < MAX_VDEV_NUM; i++) {
        mutex_destroy(&g_mnt_vdev_cb->vdev[i].lock);
    }
    mutex_unlock(&g_mnt_vdev_cb->global_lock);

    dev_mnt_vdevice_inform_release();
    dbl_kfree(g_mnt_vdev_cb);
    g_mnt_vdev_cb = NULL;
    return;
}

int dev_mnt_vdev_register_client(u32 phy_id, u32 vfid,
    const struct file_operations *ops)
{
    if (ops == NULL || phy_id >= DEVDRV_MAX_DAVINCI_NUM || vfid > VDAVINCI_MAX_VFID_NUM) {
        devdrv_drv_err("invalid para phy_id(%u), vfid(%u).\n", phy_id, vfid);
        return -EINVAL;
    }

    dev_mnt_vdev_set_ops(ops);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dev_mnt_vdev_register_client);

int dev_mnt_vdev_unregister_client(u32 phy_id, u32 vfid)
{
    if (phy_id >= DEVDRV_MAX_DAVINCI_NUM || vfid > VDAVINCI_MAX_VFID_NUM) {
        devdrv_drv_err("invalid para phy_id(%u), vfid(%u).\n", phy_id, vfid);
        return -EINVAL;
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(dev_mnt_vdev_unregister_client);

#if (defined CFG_FEATURE_VFIO) && (defined CFG_FEATURE_RC_MODE)
STATIC int dev_mnt_check_create_para_single(u32 phy_id, struct vdev_create_info *vinfo)
{
    u64 token = vinfo->vdev_info.base.token, token_max = vinfo->vdev_info.base.token_max;
    u64 task_timeout = vinfo->vdev_info.base.task_timeout;
    u32 ratio = 0;

    if (vinfo->spec.core_num != VDEV_PF_AICORE_NUM) { /*  */
        devdrv_drv_err("Invalid parameters. (aicore=%u)\n", vinfo->spec.core_num);
        return -EINVAL;
    }

    if (token != token_max || task_timeout != VDEV_TASK_TIMEOUT) {
        devdrv_drv_err("Invalid parameters. (token=%llu; token_max=%llu; is_valid_task_timeout=%d)\n",
            token, token_max, task_timeout == VDEV_TASK_TIMEOUT);
        return -EINVAL;
    }

    if (token != VDEV_TOKEN_TIME_1MS && token != VDEV_TOKEN_TIME_2MS) {
        devdrv_drv_err("Invalid parameters. (token=%llu)\n", token);
        return -EINVAL;
    }

    ratio = VDEV_TOKEN_MAP_VALUE(token) + 1;
    if (g_split_details.splitted_total + ratio > MAX_SPLIT_PIECE) {
        devdrv_drv_err("It is over current tops. (splitted_total=%u; ratio=%u)\n",
            g_split_details.splitted_total, ratio);
        return -EINVAL;
    }

    return 0;
}
#elif (defined CFG_FEATURE_VFIO)
STATIC int dev_mnt_check_create_para_single(u32 phy_id, struct vdev_create_info *vinfo)
{
    u32 core_total = 0;
    u32 core_need = 0;
    u32 core_used;
    u32 vdev_num;
    u32 dtype;
    u32 core_num;
    int ret;

    core_num = vinfo->spec.core_num;
    vdev_num = dev_mnt_vdevice_get_vdevice_alloced(phy_id);
    vdev_num++;
    if (vdev_num > VDAVINCI_MAX_VFID_NUM) {
        devdrv_drv_err("devid(%u) invalid,total_num(%u)\n", phy_id, vdev_num);
        return -EINVAL;
    }

    core_used = dev_mnt_vdevice_get_aicore_used(phy_id);
    if (core_num > VDAVINCI_SUPPORT_DIVIDE_AICORE_NUM) {
        devdrv_drv_err("input aicore num is invalid, core_num:%u\n", core_num);
        return -EINVAL;
    }

    dtype = g_vdev_dtype[core_num];
    if (dtype == MNT_VDAVINCI_TYPE_MAX) {
        devdrv_drv_err("get dtype by core num failed, core_num:%u\n", core_num);
        return -EINVAL;
    }
    core_need += core_num;

    ret = dev_mnt_vdevice_get_aicore_num(phy_id, &core_total);
    if (ret != 0) {
        devdrv_drv_err("get total core num failed, phy_id:%u,ret:%d\n", phy_id, ret);
        return ret;
    }

    if ((core_used + core_need) > core_total) {
        devdrv_drv_err("check aicore num failed, phy_id:%u, core_need:%u, used:%u, total:%u\n",
            phy_id, core_need, core_used, core_total);
        return -EINVAL;
    }

    return 0;
}
#endif

STATIC void dev_mnt_vdevice_bind(u32 vdev_id, struct mnt_namespace *ns, u64 container_id)
{
    struct vdavinci_info *info = NULL;
    u32 offset_id;

    if (!VDAVINCI_IS_VDEV(vdev_id)) {
        return;
    }

    offset_id = VDAVINCI_VDEV_OFFSET_SUB(vdev_id);
    if (offset_id >= MAX_VDEV_NUM) {
        devdrv_drv_err_spinlock("check vdev_id failed. vdev_id(%u)\n", vdev_id);
        return;
    }

    if (g_mnt_vdev_cb->vdev[offset_id].alloc_state != VDAVINCI_USED) {
        devdrv_drv_err_spinlock("vdev_id(%u) is not alloc\n", vdev_id);
        return;
    }

    mutex_lock(&g_mnt_vdev_cb->vdev[offset_id].lock);
    info = &g_mnt_vdev_cb->vdev[offset_id].info;
    if (info->used == VDAVINCI_STATE_INUSED) {
        mutex_unlock(&g_mnt_vdev_cb->vdev[offset_id].lock);
        devdrv_drv_err_spinlock("bind failed, vdev_id(%u) is in use\n", vdev_id);
        return;
    }

    info->container_id = container_id;
    info->ns = ns;
    info->used = VDAVINCI_STATE_INUSED;
    mutex_unlock(&g_mnt_vdev_cb->vdev[offset_id].lock);
}

STATIC void dev_mnt_vdevice_unbind(u32 vdev_id)
{
    struct vdavinci_info *info = NULL;
    u32 offset_id;

    if (!VDAVINCI_IS_VDEV(vdev_id)) {
        return;
    }

    offset_id = VDAVINCI_VDEV_OFFSET_SUB(vdev_id);
    if (offset_id >= MAX_VDEV_NUM) {
        devdrv_drv_err_spinlock("check vdev_id failed. vdev_id(%u)\n", vdev_id);
        return;
    }

    if (g_mnt_vdev_cb->vdev[offset_id].alloc_state != VDAVINCI_USED) {
        devdrv_drv_err_spinlock("vdev_id(%u) is not alloc\n", vdev_id);
        return;
    }

    mutex_lock(&g_mnt_vdev_cb->vdev[offset_id].lock);
    info = &g_mnt_vdev_cb->vdev[offset_id].info;
    if (info->used == VDAVINCI_STATE_IDLE) {
        mutex_unlock(&g_mnt_vdev_cb->vdev[offset_id].lock);
        devdrv_drv_err_spinlock("unbind failed, vdev_id(%u) had not bind\n", vdev_id);
        return;
    }

    info->container_devid = 0;
    info->container_id = 0;
    info->ns = NULL;
    info->used = VDAVINCI_STATE_IDLE;
    mutex_unlock(&g_mnt_vdev_cb->vdev[offset_id].lock);
}

int dev_mnt_vdevice_add_inform(unsigned int vdev_id, vdev_action action, struct mnt_namespace *ns, u64 container_id)
{
    struct dev_mnt_vdev_action *new_vdev_action = NULL;

    if (!VDAVINCI_IS_VDEV(vdev_id) || (ns == devdrv_manager_get_host_mnt_ns())) {
        return 0;
    }

    new_vdev_action = dbl_kzalloc(sizeof(struct dev_mnt_vdev_action), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (new_vdev_action == NULL) {
        devdrv_drv_err_spinlock("kzalloc new_vdev_action failed.\n");
        return -ENOMEM;
    }
    new_vdev_action->vdev_id = vdev_id;
    new_vdev_action->action = action;
    new_vdev_action->ns = ns;
    new_vdev_action->container_id = container_id;

    ka_task_spin_lock(&g_vdev_inform.spinlock);
    ka_list_add_tail(&new_vdev_action->list_node, &g_vdev_inform.vdev_action_head);
    ka_task_spin_unlock(&g_vdev_inform.spinlock);
    return 0;
}

void dev_mnt_vdevice_inform(void)
{
    struct dev_mnt_vdev_action *vdev_action = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct list_head head;

    if (ka_list_empty(&g_vdev_inform.vdev_action_head)) {
        return;
    }

    ka_task_spin_lock(&g_vdev_inform.spinlock);
    list_replace(&g_vdev_inform.vdev_action_head, &head);
    INIT_LIST_HEAD(&g_vdev_inform.vdev_action_head);
    ka_task_spin_unlock(&g_vdev_inform.spinlock);

    ka_list_for_each_safe(pos, n, &head) {
        vdev_action = ka_list_entry(pos, struct dev_mnt_vdev_action, list_node);
        if (vdev_action->action == VDEV_BIND) {
            dev_mnt_vdevice_bind(vdev_action->vdev_id, vdev_action->ns,
                                 vdev_action->container_id);
        } else if (vdev_action->action == VDEV_UNBIND) {
            dev_mnt_vdevice_unbind(vdev_action->vdev_id);
        } else {
            devdrv_drv_err("Invalid vdev action. (action = %d)\n", vdev_action->action);
        }
        ka_list_del(&vdev_action->list_node);
        dbl_kfree(vdev_action);
        vdev_action = NULL;
    }
}

STATIC void dev_mnt_vdevice_inform_release(void)
{
    struct dev_mnt_vdev_action *vdev_action = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    if (ka_list_empty(&g_vdev_inform.vdev_action_head)) {
        return;
    }

    ka_task_spin_lock(&g_vdev_inform.spinlock);
    ka_list_for_each_safe(pos, n, &g_vdev_inform.vdev_action_head) {
        vdev_action = ka_list_entry(pos, struct dev_mnt_vdev_action, list_node);
        ka_list_del(&vdev_action->list_node);
        dbl_kfree(vdev_action);
        vdev_action = NULL;
    }
    ka_task_spin_unlock(&g_vdev_inform.spinlock);
}

#ifndef CFG_FEATURE_RC_MODE
int devmng_get_vdavinci_info(u32 vdev_id, u32 *phy_id, u32 *vfid)
{
#ifndef CFG_FEATURE_SRIOV
    struct vdavinci_info *info = NULL;
    u32 offset_id;
    u32 device_status;
#endif

    if ((phy_id == NULL) || (vfid == NULL)) {
        devdrv_drv_err_spinlock("invalid para vdev_id(%u)\n", vdev_id);
        return -EINVAL;
    }

#ifdef CFG_FEATURE_SRIOV
    *phy_id = vdev_id;
    *vfid = 0;
#else
    if (!VDAVINCI_IS_VDEV(vdev_id)) {
        *phy_id = vdev_id;
        *vfid = 0;
        return 0;
    }

    offset_id = VDAVINCI_VDEV_OFFSET_SUB(vdev_id);
    if (offset_id >= MAX_VDEV_NUM) {
        devdrv_drv_err_spinlock("check vdev_id failed. vdev_id(%u)\n", vdev_id);
        return -ENODEV;
    }

    device_status = g_mnt_vdev_cb->vdev[offset_id].alloc_state;
    if (device_status == VDAVINCI_IDLE) {
        devdrv_drv_err_spinlock("Device was not alloced,(vdev_id=%u)\n", vdev_id);
        return -ENODEV;
    }

    info = &g_mnt_vdev_cb->vdev[offset_id].info;
    *phy_id = info->phy_devid;
    *vfid = info->vfid;
#endif

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devmng_get_vdavinci_info);
#endif

#if (defined CFG_FEATURE_VFIO) && (!defined CFG_FEATURE_RC_MODE)
STATIC int devmng_check_host_split(u32 dev_id)
{
    unsigned int split_mode;

#ifdef CFG_FEATURE_SRIOV
    bool is_pm_boot = false;
#endif

    if (devdrv_manager_is_mdev_vf_vm_mode(dev_id)) {
        devdrv_drv_info("Vdevice is not allowed! (dev_id=%u)\n", dev_id);
        return -EPERM;
    }

    split_mode = vmng_get_device_split_mode(dev_id);
    if (split_mode == VMNG_VIRTUAL_SPLIT_MODE) {
        devdrv_drv_info("Vdevice is not allowed! (dev_id=%u; split_mode=%u)\n", dev_id, split_mode);
        return -EPERM;
    }

#ifdef CFG_FEATURE_SRIOV
    if (devdrv_manager_is_pm_boot_mode(dev_id, &is_pm_boot)) {
        devdrv_drv_err("Get vm boot mode failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (!is_pm_boot) {
        return -EOPNOTSUPP;
    }

#endif

    return 0;
}

STATIC int dev_check_support_vdev(u32 dev_id)
{
#ifdef CFG_FEATURE_SMP_VDEV_NOT_SUPPORT
    u32 amp_or_smp = DEVMNG_AMP_MODE;

    if (devdrv_manager_get_amp_smp_mode(&amp_or_smp)) {
        devdrv_drv_err("Get amp smp mode failed. (dev_id=%u)\n", dev_id);
        return -EFAULT;
    }

    if (amp_or_smp == DEVMNG_SMP_MODE) {
        return -EOPNOTSUPP;
    }
#endif

    return 0;
}
#endif

int devdrv_manager_ioctl_create_vdev(struct file *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_VFIO
    struct vdev_create_info vinfo = {0};
    u32 phy_id = 0;
    u32 vfid;
    int ret;

    if (devdrv_manager_check_permission()) {
        devdrv_drv_err("check permission failed.\n");
        return -EPERM;
    }
#ifndef CFG_FEATURE_RC_MODE
    ret = dev_check_support_vdev(phy_id);
    if (ret != 0) {
        return ret;
    }
#endif
    if (copy_from_user_safe(&vinfo, (void *)(uintptr_t)arg, sizeof(struct vdev_create_info))) {
        return -EFAULT;
    }

    /* logical_id_to_physical_id had checked the devid range, do not need check it again */
    ret = devdrv_manager_container_logical_id_to_physical_id(vinfo.devid, &phy_id, &vfid);
    if (ret) {
        devdrv_drv_err("Failed to get phy_id. (dev_id=%u; ret=%d)\n", vinfo.devid, ret);
        return ret;
    }
#ifndef CFG_FEATURE_RC_MODE
    ret = devmng_check_host_split(phy_id);
    if (ret != 0) {
        devdrv_drv_err("Check physical or virtual direct failed, it is not supported. (phy_id=%u)\n", phy_id);
        return ret;
    }
#endif

    mutex_lock(&g_mnt_vdev_cb->global_lock);
    ret = dev_mnt_check_create_para_single(phy_id, &vinfo);
    if (ret) {
        mutex_unlock(&g_mnt_vdev_cb->global_lock);
        devdrv_drv_err("Failed to create vdev info. (dev_id=%u; ret=%d)\n", vinfo.devid, ret);
        return ret;
    }

    ret = dev_mnt_create_vdevice_single(phy_id, &vinfo);
    if (ret) {
        mutex_unlock(&g_mnt_vdev_cb->global_lock);
        devdrv_drv_err("Failed to create vdevice. (dev_id=%u; ret=%d)\n", vinfo.devid, ret);
        return ret;
    }
#ifdef CFG_FEATURE_VF_SINGLE_AICORE
    g_split_details.splitted_total += VDEV_TOKEN_MAP_VALUE(vinfo.vdev_info.base.token) + 1;
    g_split_details.vdev_pieces[vinfo.vdevid] = VDEV_TOKEN_MAP_VALUE(vinfo.vdev_info.base.token) + 1;
#endif
    vinfo.vdevid = VDAVINCI_VDEV_OFFSET_PLUS(vinfo.vdevid);
    mutex_unlock(&g_mnt_vdev_cb->global_lock);

#if (defined CFG_HOST_ENV) && (defined CFG_FEATURE_VF_USE_DEVID)
    /* It is to wait for device vf to go online, or flr may fail and it should skip in RC mode. */
    ret = devdrv_wait_device_ready(vinfo.vdevid, WAIT_VDEV_READY_TIMEOUT_SECOND);
    if (ret != 0) {
        (void)dev_mnt_destory_vdevice(phy_id, vinfo.vdevid);
        return ret;
    }
#endif
    if (copy_to_user_safe((void *)(uintptr_t)arg, &vinfo, sizeof(struct vdev_create_info))) {
        return -EFAULT;
    }

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

/*
 * vdevid = 0xffff, destroy all the vdevice created by devid.
 * vdevid != 0xffff destroy the specified vdevid num vdevice.
 */
int devdrv_manager_ioctl_destroy_vdev(struct file *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_VFIO
    struct devdrv_vdev_id_info vinfo = {0};
    u32 phy_id = 0;
    u32 vfid;
    int ret;

    if (devdrv_manager_check_permission()) {
        devdrv_drv_err("check permission failed.\n");
        return -EPERM;
    }
#ifndef CFG_FEATURE_RC_MODE
    ret = dev_check_support_vdev(phy_id);
    if (ret != 0) {
        return ret;
    }
#endif
    if (copy_from_user_safe(&vinfo, (void *)(uintptr_t)arg, sizeof(struct devdrv_vdev_id_info))) {
        return -EFAULT;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(vinfo.devid, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Failed to convert the logical_id to the physical_id. (devid=%u; ret=%d)\n", vinfo.devid, ret);
        return ret;
    }

#ifdef CFG_FEATURE_SRIOV
    ret = devmng_check_host_split(phy_id);
    if (ret != 0) {
        devdrv_drv_err("Check physical or virtual direct failed, it is not supported. (phy_id=%u)\n", phy_id);
        return ret;
    }
#endif

    if (vinfo.vdevid != DEVDRV_DESTROY_ALL_VDEVICE) {
        if (!dev_mnt_check_vdev_id_is_valid(vinfo.vdevid, phy_id)) {
            devdrv_drv_err("Invalid parameter. (devid=%u; vdevid=%d)\n", vinfo.devid, vinfo.vdevid);
            return -ENODEV;
        }

#ifdef CFG_FEATURE_SRIOV
        ret = devdrv_get_pfvf_id_by_devid(vinfo.vdevid, &phy_id, &vfid);
#elif (defined CFG_FEATURE_RC_MODE)
        ret = vmngd_get_pfvf_id_by_devid(vinfo.vdevid, &phy_id, &vfid);
#else
        ret = devmng_get_vdavinci_info(vinfo.vdevid, &phy_id, &vfid);
#endif
        if (ret != 0) {
            devdrv_drv_err("Invalid parameter. (devid=%u; vdevid=%d; ret=%d)\n", vinfo.devid, vinfo.vdevid, ret);
            return ret;
        }
    }
#if !defined CFG_FEATURE_RC_MODE && !defined CFG_FEATURE_SRIOV
    ret = devmng_check_host_split(phy_id);
    if (ret != 0) {
        devdrv_drv_err("Check physical or virtual direct failed, it is not supported. (phy_id=%u)\n", phy_id);
        return ret;
    }
#endif
    uda_release_idle_ns_by_vdev_id(phy_id, vinfo.vdevid);
    ret = dev_mnt_destory_vdevice(phy_id, vinfo.vdevid);
    if (ret) {
        devdrv_drv_err("destroy_vdevice fail, devid(%u) vdevid(%d), ret(%d).\n", vinfo.devid, vinfo.vdevid, ret);
        return ret;
    }

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

int devdrv_manager_ioctl_get_vdevinfo(struct file *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_VFIO
    u32 phy_id = 0;
    u32 vfid;
    int ret;
    struct vdev_query_info *vinfo = NULL;

    vinfo = dbl_kzalloc(sizeof(struct vdev_query_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (vinfo == NULL) {
        devdrv_drv_err("Kzalloc failed.\n");
        return -EPERM;
    }

    if (!devdrv_manager_container_is_host_system(current->nsproxy->mnt_ns) && !devdrv_manager_container_is_admin()) {
        devdrv_drv_err("check permission failed.\n");
        ret = -EPERM;
        goto EXIT;
    }
#ifndef CFG_FEATURE_RC_MODE
    ret = dev_check_support_vdev(phy_id);
    if (ret != 0) {
        goto EXIT;
    }
#endif
    if (copy_from_user_safe(vinfo, (void *)(uintptr_t)arg, sizeof(struct vdev_query_info))) {
        ret = -EFAULT;
        goto EXIT;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(vinfo->devid, &phy_id, &vfid);
    if (ret) {
        devdrv_drv_err("logical_id_to_physical_id fail, devid(%u) ret(%d).\n", vinfo->devid, ret);
        goto EXIT;
    }
#ifndef CFG_FEATURE_RC_MODE
    ret = devmng_check_host_split(phy_id);
    if (ret != 0) {
        devdrv_drv_err("Check physical or virtual direct failed, it is not supported. (phy_id=%u)\n", phy_id);
        goto EXIT;
    }
#endif
    vinfo->vfid = vfid;

    if (vinfo->vdev_id_single != 0 &&
        dev_mnt_get_vfid_from_vdevid(vinfo->vdev_id_single, phy_id, &vinfo->vfid) != 0) {
        devdrv_drv_err("Get vfid from vdevid failed, (vdevid=%d; phyid=%d)\n", vinfo->vdev_id_single, phy_id);
        ret = -EINVAL;
        goto EXIT;
    }

    ret = dev_mnt_get_vdevice_info(phy_id, vinfo);
    if (ret) {
        devdrv_drv_err("Dev_mnt_get_vdevice_info fail. (devid=%u; ret(%d)\n", vinfo->devid, ret);
        goto EXIT;
    }

    if (copy_to_user_safe((void *)(uintptr_t)arg, vinfo, sizeof(struct vdev_query_info))) {
        ret = -EFAULT;
        goto EXIT;
    }
    dbl_kfree(vinfo);
    return 0;
EXIT:
    dbl_kfree(vinfo);
    return ret;
#else
    return -EOPNOTSUPP;
#endif
}

/**
 * this function allow to get all vdevice resource info
 * only the root user of host machine can use this interface.
 **/
#ifndef CFG_FEATURE_VFIO_SOC
int devdrv_manager_get_vdev_resource_info(struct devdrv_resource_info *dinfo)
{
#ifdef CFG_FEATURE_VFIO
    struct devdrv_manager_msg_resource_info info;
    u32 phy_id = 0;
    u32 vfid = 0;
    int ret;

    if (dinfo == NULL) {
        devdrv_drv_err("input resource info is NULL.\n");
        return -EINVAL;
    }

    if ((dinfo->resource_type == DEVDRV_DEV_PROCESS_PID) || (dinfo->resource_type == DEVDRV_DEV_PROCESS_MEM) ||
        (dinfo->resource_type == DEVDRV_DEV_PROCESS_CONTAINER_PID)) {
        return -EOPNOTSUPP;
    }

#ifndef CFG_FEATURE_DDR
    if ((dinfo->resource_type == DEVDRV_DEV_DDR_TOTAL) || (dinfo->resource_type == DEVDRV_DEV_DDR_FREE)) {
        return -EOPNOTSUPP;
    }
#endif

    if (devdrv_manager_check_permission()) {
        devdrv_drv_err("check permission failed.\n");
        return -EPERM;
    }

    if (!VDAVINCI_IS_VDEV(dinfo->devid)) {
        devdrv_drv_err("devid(%u) is invalid, only virtual devices are supported.\n", dinfo->devid);
        return -EINVAL;
    }

    ret = devmng_get_vdavinci_info(dinfo->devid, &phy_id, &vfid);
#ifdef CFG_FEATURE_VF_USE_DEVID
    if (ret != 0) {
#else
    if (ret != 0 || vfid == 0) {
#endif
        devdrv_drv_err("get phy id failed, vdev_id:%d, ret:%d\n", dinfo->devid, ret);
        return -EINVAL;
    }

    info.vfid = vfid;
    info.info_type = dinfo->resource_type;
    info.owner_id = dinfo->owner_id;
    ret = hvdevmng_get_dev_resource(phy_id, dinfo->tsid, &info);
    if (ret) {
        devdrv_drv_err("get resource info failed, devid(%u), resource type (%u) ret(%d).\n",
            dinfo->devid, dinfo->resource_type, ret);
        return ret;
    }
    *((u64 *)dinfo->buf) = info.value;

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}
#endif

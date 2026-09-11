/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#define pr_fmt(fmt) "XSMEM_SP: <%s:%d> " fmt, __func__, __LINE__

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/mman.h>
#include <linux/share_pool.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/nodemask.h>
#include <linux/oom.h>

#include "xsmem_framework_log.h"
#include "xsmem_framework.h"
#include "ascend_kernel_hal.h"
#include "dbl/chip_config.h"
#include "kernel_version_adapt.h"
#include "devdrv_manager_comm.h"
#include "virtmng_interface.h"
#include "devdrv_interface.h"
#include "xsmem_algo_sp.h"

struct sp_ctrl {
    int sp_id;
    int init_pid;
};

#ifndef SPG_ID_AUTO
#define SPG_ID_AUTO 200000
#endif
#ifndef SP_SPEC_NODE_ID
#define SP_SPEC_NODE_ID (1 << 3)
#endif
#ifndef NODE_ID_SHIFT
#define NODE_ID_SHIFT 36
#endif
#define SP_NID_MAX_NUM 32

/* Normal node: can be managed by os */
#ifndef EMU_ST
static inline bool sp_is_normal_node(int nid)
{
    return (node_state(nid, N_NORMAL_MEMORY) != 0);
}
#endif

#ifdef CFG_SUPPORT_UPDATE_NUMA_ORDER
static void sp_algo_update_numa_order(int *nids, u32 nid_num)
{
    int temp;

    /* numa info
     * devid    1p:0   2P:master,slave
     * numa id
     * OS DDR   0      0(master),1(slave)
     * P2P DDR  1      2(master),3(slave)
     * TS DDR   2      4(master),5(slave)
     */
    /* control cpu scene, update numa alloc order: p2p(0)->os(1)->ts(2) */
    temp = nids[0];
    nids[0] = nids[1];
    nids[1] = temp;
}
#endif

/* all alloc from cgroup node */
static int sp_algo_get_node_id(u32 devid, int *nids, u32 nid_num)
{
    int nid_cnt = dbl_get_ai_nid(devid, nids, (int)nid_num);
    if (nid_cnt < 0) {
        xsmem_err("Can not get nid. (devid=%u)\n", devid);
    }

    return nid_cnt;
}

static int sp_algo_devpid_to_devid_and_scene(u32 devpid, u32 *devid, u32 *vfid, bool *is_ccpu)
{
    enum devdrv_process_type cp_type;
    u32 host_pid;
    int ret;

    *vfid = 0;
    ret = devdrv_query_process_host_pid(devpid, devid, vfid, &host_pid, &cp_type);
    if (ret != 0) {
        /* ccpu scene doesn't have host pid */
        *is_ccpu = true;
    }
    return ret;
}

static int sp_algo_get_pfvf_type_by_devid(u32 devid)
{
    int pfvf_type = DEVDRV_SRIOV_TYPE_PF;

#ifndef CFG_FEATURE_SOC
        pfvf_type = devdrv_get_pfvf_type_by_devid(devid);
#endif
    return pfvf_type;
}

static bool sp_algo_dev_is_pf(u32 devid, u32 vfid)
{
    if ((vfid != 0) || ((vfid == 0) && (sp_algo_get_pfvf_type_by_devid(devid) == DEVDRV_SRIOV_TYPE_VF))) {
        return false;
    }

    return true;
}

static void sp_algo_update_devid_and_scene(u32 *devid, bool *is_pf, bool *is_ccpu)
{
    u32 tmp_devid, vfid;
    int ret;

    xsmem_debug("Start. (devid=%u; is_pf=%d).\n", *devid, *is_pf);

    ret = sp_algo_devpid_to_devid_and_scene((u32)current->tgid, &tmp_devid, &vfid, is_ccpu);
    if (ret != 0) {
        /* buff hlt donot use host bind api */
        return;
    }

    if (sp_algo_dev_is_pf(tmp_devid, vfid) == false) {
        *devid = tmp_devid;
        *is_pf = false;
    }

    xsmem_debug("End. (devid=%u; is_pf=%d; vfid=%u; is_ccpu=%u).\n", *devid, *is_pf, vfid, *is_ccpu);
}

static void sp_algo_nids_adapt(u32 devid, bool is_pf, bool is_ccpu, int nids[], u32 nid_num)
{
    if ((is_pf == false) && (dbl_nid_get_memctrl_type() == DBL_NID_MEMCTRL_SHARED_TYPE)) {
        int cgroup_nid;
        u32 i;

        /* get cgroup nid */
        cgroup_nid = cpu_to_node(raw_smp_processor_id());
        /* vf not use cdm nid, now host svm management cdm node */
        for (i = 0; i < nid_num; i++) {
#ifndef EMU_ST
            if ((nids[i] == NUMA_NO_NODE) || (sp_is_normal_node(nids[i]) == false)) {
                nids[i] = cgroup_nid;
            }
#endif
        }
    }

#ifdef CFG_SUPPORT_UPDATE_NUMA_ORDER
    if (is_ccpu) {
        sp_algo_update_numa_order(nids, nid_num);
    }
#endif
}

static int sp_algo_nids_get(u32 devid, bool is_pf, bool is_ccpu, int nids[], u32 *nid_num)
{
    int get_nid_num;

    get_nid_num = sp_algo_get_node_id(devid, nids, *nid_num);
    if (get_nid_num < 0) {
        xsmem_err("Get nid info failed.\n");
        return -ENXIO;
    }
    sp_algo_nids_adapt(devid, is_pf, is_ccpu, nids, get_nid_num);
    *nid_num = (u32)get_nid_num;

    return 0;
}

static void sp_algo_set_nid_to_flag(int nid, unsigned long *sp_flags)
{
    *sp_flags = (*sp_flags) | (((unsigned long)(unsigned int)nid) << NODE_ID_SHIFT) | SP_SPEC_NODE_ID;
}

void *sp_inst_create(void)
{
    struct sp_ctrl *ctrl = NULL;
    unsigned long prop;

    ctrl = xsmem_drv_kzalloc(sizeof(*ctrl), GFP_KERNEL | __GFP_ACCOUNT);
    if (ctrl == NULL) {
        xsmem_err("Alloc memory for sp_ctrl failed.\n");
        return NULL;
    }

    prop = (PROT_WRITE | PROT_READ);
    ctrl->init_pid = (int)current->tgid;
    ctrl->sp_id = mg_sp_group_add_task(ctrl->init_pid, prop, SPG_ID_AUTO);
    if (ctrl->sp_id < 0) {
        xsmem_err("Add task failed. (pid=%d; sp_id=%d)\n", ctrl->init_pid, ctrl->sp_id);
        xsmem_drv_kfree(ctrl);
        return NULL;
    }

    xsmem_info("Add task success. (pid=%d; sp_id=%d)\n", ctrl->init_pid, ctrl->sp_id);
    return (void *)ctrl;
}

static int sp_algo_pool_init(struct xsm_pool *xp, struct xsm_reg_arg *arg)
{
    void *ctrl = NULL;

    ctrl = sp_inst_create();
    if (ctrl == NULL) {
        return -EFAULT;
    }

    xp->private = ctrl;
    return 0;
}

int sp_inst_destroy(void *sp_ctrl)
{
    struct sp_ctrl *ctrl = (struct sp_ctrl *)sp_ctrl;
    xsmem_drv_kfree(ctrl);
    return 0;
}

static int sp_algo_pool_free(struct xsm_pool *xp)
{
    sp_inst_destroy(xp->private);
    xp->private = NULL;
    return 0;
}

int sp_perm_add(void *sp_ctrl, int pid, unsigned long prop)
{
    struct sp_ctrl *ctrl = (struct sp_ctrl *)sp_ctrl;
    int sp_id;

    if (pid == ctrl->init_pid) {
        sp_id = ctrl->sp_id;
    } else {
        sp_id = mg_sp_group_add_task(pid, prop, ctrl->sp_id);
        if (sp_id < 0) {
            xsmem_err("Add task fail. (pid=%d; ctrl_sp_id=%d; sp_id=%d)\n", pid, ctrl->sp_id, sp_id);
            return -EFAULT;
        }
    }

    xsmem_info("Sp_perm_add. (proc=%s; tgid=%d; current_pid=%d; add_task_pid=%d; sp_id=%d; ctrl_sp_id=%d)\n",
        current->comm, current->tgid, current->pid, pid, sp_id, ctrl->sp_id);
    return 0;
}

static int sp_algo_pool_perm_add(struct xsm_pool *xp, int pid, unsigned long prop)
{
    return sp_perm_add(xp->private, pid, prop);
}

#ifdef CFG_SUPPORT_ALLOC_NODEMASK
static void *sp_algo_alloc_nodemask(unsigned long alloc_size, unsigned long flag, int sp_id,
    int *nids, u32 nid_num)
{
    unsigned long sp_flags = flag;
    nodemask_t nodemask;
    u32 tmp_nid_num = nid_num;
    void *vaddr = NULL;
    u32 i;

#ifdef CFG_SUPPORT_UPDATE_NUMA_ORDER
    /* miniv2 ccpu: p2p->os->ts, can't set ts */
    tmp_nid_num--;
#endif

    nodes_clear(nodemask);
    for (i = 0; i < tmp_nid_num; i++) {
        node_set(nids[i], nodemask);
    }
    sp_algo_set_nid_to_flag(nids[0], &sp_flags);
    vaddr = mg_sp_alloc_nodemask(alloc_size, sp_flags, sp_id, nodemask);
#ifdef CFG_SUPPORT_UPDATE_NUMA_ORDER
    if (IS_ERR_OR_NULL(vaddr)) {
        unsigned long sp_flag = flag;
        /* miniv2 ccpu: p2p->os->ts, set ts */
        sp_algo_set_nid_to_flag(nids[tmp_nid_num], &sp_flag);
        return mg_sp_alloc(alloc_size, sp_flags, sp_id);
    }
#endif
    return vaddr;
}
#endif

static void *_sp_algo_alloc(unsigned long alloc_size, unsigned long flag, int sp_id,
    int *nids, u32 nid_num)
{
    void *vaddr = NULL;
    u32 i;

    for (i = 0; i < nid_num; i++) {
        unsigned long sp_flags = flag;
        sp_algo_set_nid_to_flag(nids[i], &sp_flags);
        vaddr = mg_sp_alloc(alloc_size, sp_flags, sp_id);
        if (IS_ERR_OR_NULL(vaddr)) {
            continue;
        }
        break;
    }
    return vaddr;
}

static bool sp_is_alloc_nodemask(u32 devid, bool is_ccpu)
{
    bool support_update_numa_order = false;
    bool is_alloc_nodemask = false;

#ifdef CFG_SUPPORT_ALLOC_NODEMASK
    is_alloc_nodemask = true;
#endif

#ifdef CFG_SUPPORT_UPDATE_NUMA_ORDER
    support_update_numa_order = true;
#endif

    /* miniv2 ep alloc by node, ccpu alloc by nodemask */
    return ((is_ccpu == false) && support_update_numa_order) ? false : is_alloc_nodemask;
}

static inline unsigned long sp_clear_xsmem_self_define_flag(unsigned long flag)
{
    return (flag & (~XSMEM_BLK_NOT_AUTO_RECYCLE) & (~XSMEM_BLK_ALLOC_FROM_OS));
}

int sp_algo_alloc(void *sp_ctrl, unsigned long alloc_size, unsigned long flag,
    unsigned long *addr, unsigned long *real_size)
{
    struct sp_ctrl *ctrl = (struct sp_ctrl *)sp_ctrl;
    unsigned long real_size_tmp, sp_flags;
    int nids[SP_NID_MAX_NUM] = {0};
    u32 nid_num = SP_NID_MAX_NUM;
    void *vaddr = NULL;
    bool is_ccpu = false;
    bool is_pf = true;
    u32 devid;
    int ret;

    real_size_tmp = ALIGN(alloc_size, PAGE_SIZE);
    if (unlikely(real_size_tmp < alloc_size)) {
        return -EOVERFLOW;
    }
    sp_flags = sp_clear_xsmem_self_define_flag(flag);
    devid = sp_get_device_id_from_flags(sp_flags);
    sp_algo_update_devid_and_scene(&devid, &is_pf, &is_ccpu);

    ret = sp_algo_nids_get(devid, is_pf, is_ccpu, nids, &nid_num);
    if (ret != 0) {
        xsmem_err("Get nids fail. (devid=%u)\n", devid);
        return ret;
    }

    if (sp_is_alloc_nodemask(devid, is_ccpu)) {
#ifdef CFG_SUPPORT_ALLOC_NODEMASK
        vaddr = sp_algo_alloc_nodemask(real_size_tmp, sp_flags, ctrl->sp_id, nids, nid_num);
#endif
    } else {
        vaddr = _sp_algo_alloc(real_size_tmp, sp_flags, ctrl->sp_id, nids, nid_num);
    }
    if (IS_ERR_OR_NULL(vaddr) == false) {
        *real_size = real_size_tmp;
        *addr = (unsigned long)(uintptr_t)vaddr;
        return 0;
    }

    xsmem_err("Sp alloc failed. (size=0x%lx; flag=0x%lx; sp_id=%d; devid=%u; nid_num=%u, err=%ld)\n",
        real_size_tmp, sp_flags, ctrl->sp_id, devid, nid_num, PTR_ERR(vaddr));
    if ((PTR_ERR(vaddr)) == -ENOMEM) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
        (void)oom_type_notifier_call(OOM_TYPE_CGROUP, NULL);
#else
        (void)hisi_oom_notifier_call(HISI_OOM_TYPE_CGROUP, NULL);
#endif
    }
    return -ENOMEM;
}

static int sp_algo_block_alloc(struct xsm_pool *xp, struct xsm_block *blk)
{
    return sp_algo_alloc(xp->private, blk->alloc_size, blk->flag, &blk->offset, &blk->real_size);
}

int sp_algo_free(void *sp_ctrl, unsigned long addr)
{
#ifdef CFG_FEATURE_SP_GRP_STANDALONE_VA
    struct sp_ctrl *ctrl = (struct sp_ctrl *)sp_ctrl;

    return mg_sp_free(addr, ctrl->sp_id);
#else
    return mg_sp_free(addr);
#endif
}

int sp_algo_is_sharepool_addr(unsigned long va)
{
    return mg_is_sharepool_addr(va);
}

static int sp_algo_block_free(struct xsm_pool *xp, struct xsm_block *blk)
{
    return sp_algo_free(xp->private, blk->offset);
}

static int sp_algo_va_check(struct xsm_pool *xp, unsigned long va, int *result)
{
    *result = sp_algo_is_sharepool_addr(va);
    return 0;
}

static struct xsm_pool_algo sp_algo = {
    .num = XSMEM_ALGO_SP,
    .name = "sp_algo",
    .xsm_pool_init = sp_algo_pool_init,
    .xsm_pool_free = sp_algo_pool_free,
    .xsm_pool_perm_add = sp_algo_pool_perm_add,
    .xsm_pool_va_check = sp_algo_va_check,
    .xsm_block_alloc = sp_algo_block_alloc,
    .xsm_block_free = sp_algo_block_free,
};

struct xsm_pool_algo *xsm_get_sp_algo(void)
{
    return &sp_algo;
}

int hal_kernel_numa_get_nid(unsigned int device_id, unsigned int type)
{
    return device_id;
}
EXPORT_SYMBOL_GPL(hal_kernel_numa_get_nid);


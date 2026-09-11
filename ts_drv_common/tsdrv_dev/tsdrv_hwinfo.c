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
#include "tsdrv_hwinfo.h"
#include "tsdrv_log.h"
#include "tsdrv_device.h"
#include "tsdrv_common.h"
#ifndef CFG_MANAGER_HOST_ENV
#include "devdrv_parse_pdata.h"
#endif /* CFG_MANAGER_HOST_ENV */

STATIC struct tsdrv_dev_hwinfo *g_dev_hwinfo[TSDRV_MAX_DAVINCI_NUM];

STATIC_INLINE struct tsdrv_tsfw_hwinfo *tsdrv_get_tsfw_hwinfo_l(u32 devid, u32 tsid)
{
    return &g_dev_hwinfo[devid]->hwinfo[tsid].tsfw_hwinfo;
}

STATIC_INLINE struct tsdrv_mbox_hwinfo *tsdrv_get_mbox_hwinfo_l(u32 devid, u32 tsid)
{
    return g_dev_hwinfo[devid]->hwinfo[tsid].mbox_hwinfo;
}

STATIC_INLINE struct tsdrv_sq_hwinfo *tsdrv_get_sq_hwinfo_l(u32 devid, u32 tsid)
{
    return &g_dev_hwinfo[devid]->hwinfo[tsid].sq_hwinfo;
}

struct tsdrv_cq_hwinfo *tsdrv_get_cq_hwinfo(u32 devid, u32 tsid)
{
    return &g_dev_hwinfo[devid]->hwinfo[tsid].cq_hwinfo;
}

void tsdrv_get_db_phy_info(u32 devid, u32 tsid, phys_addr_t *paddr, size_t *size)
{
    *paddr = g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.db_paddr;
    *size = g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.db_size;
}

struct tsdrv_db_hwinfo_t *tsdrv_get_db_hwinfo_t(u32 devid, u32 tsid, enum tsdrv_db_type db_type)
{
    return &g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.db_hwinfo_t[db_type];
}

struct tsdrv_db_type_info {
    u32 offset;
    u32 num;
} g_db_type_info[TSDRV_MAX_DB] = {
    {TSDRV_SQ_DB_OFFSET, TSDRV_SQ_DB_NUM},
    {TSDRV_CQ_DB_OFFSET, TSDRV_CQ_DB_NUM},
    {TSDRV_HCCL_DB_OFFSET, TSDRV_HCCL_DB_NUM},
    {TSDRV_ONLINE_DFX_SQ_DB_OFFSET, TSDRV_ONLINE_DFX_SQ_DB_NUM},
    {TSDRV_ONLINE_DFX_CQ_DB_OFFSET, TSDRV_ONLINE_DFX_CQ_DB_NUM},
    {TSDRV_ONLINE_MBOX_SEND_DB_OFFSET, TSDRV_ONLINE_MBOX_SEND_DB_NUM},
    {TSDRV_ONLINE_MBOX_READ_DB_OFFSET, TSDRV_ONLINE_MBOX_READ_DB_NUM},
    {TSDRV_OFFLINE_DFX_SQ_DB_OFFSET, TSDRV_OFFLINE_DFX_SQ_DB_NUM},
    {TSDRV_OFFLINE_DFX_CQ_DB_OFFSET, TSDRV_OFFLINE_DFX_CQ_DB_NUM},
    {TSDRV_OFFLINE_MBOX_SEND_DB_OFFSET, TSDRV_OFFLINE_MBOX_SEND_DB_NUM},
    {TSDRV_OFFLINE_MBOX_READ_DB_OFFSET, TSDRV_OFFLINE_MBOX_READ_DB_NUM}
};

STATIC void tsdrv_set_db_info_t(u32 devid, u32 tsid, enum tsdrv_db_type db_type, struct tsdrv_db_info *db_info)
{
    struct tsdrv_db_hwinfo_t *db_hwinfo_t = tsdrv_get_db_hwinfo_t(devid, tsid, db_type);

    db_hwinfo_t->dbs = db_info->dbs + (g_db_type_info[db_type].offset * db_info->db_stride);
    db_hwinfo_t->num = g_db_type_info[db_type].num;
    db_hwinfo_t->db_stride = db_info->db_stride;
    db_hwinfo_t->db_type = db_type;
    TSDRV_PRINT_DEBUG("Doorbell info. (devid=%u; db_type=%d; dbs=0x%pK; num=%u; stride=0x%lx)\n",
        devid, (unsigned int)db_type, db_hwinfo_t->dbs, db_hwinfo_t->num, db_hwinfo_t->db_stride);
}

static void tsdrv_set_mbox_db_info_t(u32 devid, u32 tsid, enum tsdrv_db_type db_type, u32 offset,
    struct tsdrv_db_info *db_info)
{
    struct tsdrv_db_hwinfo_t *db_hwinfo_t = tsdrv_get_db_hwinfo_t(devid, tsid, db_type);

    db_hwinfo_t->dbs = db_info->dbs + db_info->db_stride * offset;
    db_hwinfo_t->num = 1;
    db_hwinfo_t->db_stride = db_info->db_stride;
    db_hwinfo_t->db_type = db_type;
    TSDRV_PRINT_DEBUG("Mailbox doorbell. (devid=%u; db_type=%d; offset=%u; dbs=0x%pK)\n", devid, (unsigned int)db_type,
        offset, db_hwinfo_t->dbs);
}

void tsdrv_set_mbox_db_info(u32 devid, u32 tsid, struct tsdrv_db_info *db_info)
{
    if (tsid < DEVDRV_MAX_TS_NUM) {
        g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.ts_db_paddr =  db_info->db_paddr;
        g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.ts_db_size = db_info->db_size;
        tsdrv_set_mbox_db_info_t(devid, tsid, TSDRV_ONLINE_MBOX_SEND_DB, 0, db_info);
    }
}

struct tsdrv_stars_db_type_info {
    u32 offset;
    u32 num;
} g_stars_db_type_info[TSDRV_MAX_DB] = {
    {TSDRV_STARS_SQ_DB_OFFSET, TSDRV_STARS_SQ_DB_NUM},
    {TSDRV_STARS_CQ_DB_OFFSET, TSDRV_STARS_CQ_DB_NUM},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}
};

STATIC void tsdrv_set_stars_db_info_t(u32 devid, u32 tsid, enum tsdrv_db_type db_type, struct tsdrv_db_info *db_info)
{
    struct tsdrv_db_hwinfo_t *db_hwinfo_t = tsdrv_get_db_hwinfo_t(devid, tsid, db_type);

    db_hwinfo_t->dbs = db_info->dbs + (g_stars_db_type_info[db_type].offset * db_info->db_stride);
    db_hwinfo_t->num = g_stars_db_type_info[db_type].num;
    db_hwinfo_t->db_stride = db_info->db_stride;
    db_hwinfo_t->db_type = db_type;
    TSDRV_PRINT_DEBUG("Stars doorbell info. (devid=%u; db_type=%d; dbs=0x%pK; num=%u; stride=0x%lx)\n",
        devid, (unsigned int)db_type, db_hwinfo_t->dbs, db_hwinfo_t->num, db_hwinfo_t->db_stride);
}

int tsdrv_set_ts_mbox_hwinfo(u32 devid, u32 tsid, struct tsdrv_mbox_hwinfo *mbox_hwinfo)
{
    struct tsdrv_mbox_hwinfo *mbox_hwinfo_l = NULL;
    u32 chann_id;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u)\n", devid, tsid);
        return -EFAULT;
#endif
    }
    if (mbox_hwinfo == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("mbox_hwinfo is NULL, devid(%u) tsid(%u)\n", devid, tsid);
        return -ENODEV;
#endif
    }
    chann_id = mbox_hwinfo->chann_id;
    if (chann_id >= TSDRV_MAX_MBOX_CHANN) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid chann_id(%u) devid(%u) tsid(%u)\n", mbox_hwinfo->chann_id, devid, tsid);
        return -ENOMEM;
#endif
    }
    mbox_hwinfo_l = tsdrv_get_mbox_hwinfo_l(devid, tsid);
    mbox_hwinfo_l[chann_id].chann_id = chann_id;
    mbox_hwinfo_l[chann_id].mbox_ack_irq = mbox_hwinfo->mbox_ack_irq;
    mbox_hwinfo_l[chann_id].mbox_ack_request_irq = mbox_hwinfo->mbox_ack_request_irq;
    mbox_hwinfo_l[chann_id].mbox_rx_irq = mbox_hwinfo->mbox_rx_irq;
    mbox_hwinfo_l[chann_id].mbox_rx_request_irq = mbox_hwinfo->mbox_rx_request_irq;
    mbox_hwinfo_l[chann_id].mbox_rx = mbox_hwinfo->mbox_rx;
    mbox_hwinfo_l[chann_id].mbox_tx = mbox_hwinfo->mbox_tx;
    mbox_hwinfo_l[chann_id].mbox_size = mbox_hwinfo->mbox_size;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_set_ts_mbox_hwinfo);

struct tsdrv_mbox_hwinfo *tsdrv_get_ts_mbox_hwinfo(u32 devid, u32 tsid, u32 chann_id)
{
    struct tsdrv_mbox_hwinfo *mbox_hwinfo_l = NULL;

    mbox_hwinfo_l = tsdrv_get_mbox_hwinfo_l(devid, tsid);
    return &mbox_hwinfo_l[chann_id];
}

int tsdrv_set_tsfw_hwinfo(u32 devid, u32 tsid, struct tsdrv_tsfw_hwinfo *tsfw_hwinfo)
{
    struct tsdrv_tsfw_hwinfo *tsfw_hwinfo_l = NULL;

    if (tsfw_hwinfo == NULL) {
        TSDRV_PRINT_ERR("sq_hwinfo is NULL\n");
        return -ENOMEM;
    }
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u)\n", devid, tsid);
        return -EINVAL;
    }
    tsfw_hwinfo_l = tsdrv_get_tsfw_hwinfo_l(devid, tsid);
    tsfw_hwinfo_l->vaddr = tsfw_hwinfo->vaddr;
    tsfw_hwinfo_l->size = tsfw_hwinfo->size;
    return 0;
}

struct tsdrv_tsfw_hwinfo *tsdrv_get_tsfw_hwinfo(u32 devid, u32 tsid)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u)\n", devid, tsid);
        return NULL;
    }
    return tsdrv_get_tsfw_hwinfo_l(devid, tsid);
}

int tsdrv_set_ts_sq_hwinfo(u32 devid, u32 tsid, struct tsdrv_sq_hwinfo *sq_hwinfo)
{
    struct tsdrv_sq_hwinfo *sq_hwinfo_l = NULL;

    if (sq_hwinfo == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("sq_hwinfo is NULL\n");
        return -ENOMEM;
#endif
    }
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u)\n", devid, tsid);
        return -EINVAL;
#endif
    }
    sq_hwinfo_l = tsdrv_get_sq_hwinfo_l(devid, tsid);
    sq_hwinfo_l->bar_addr = sq_hwinfo->bar_addr;
    sq_hwinfo_l->paddr = sq_hwinfo->paddr;
    sq_hwinfo_l->size = sq_hwinfo->size;
    return 0;
}

struct tsdrv_sq_hwinfo *tsdrv_get_ts_sq_hwinfo(u32 devid, u32 tsid)
{
    return tsdrv_get_sq_hwinfo_l(devid, tsid);
}

int tsdrv_get_dfx_cq_irq_vector(u32 devid, u32 tsid)
{
    struct tsdrv_cq_hwinfo *cq_hwinfo;

    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
        TSDRV_PRINT_ERR("invalid param, devid[%u] tsid[%u]\n", devid, tsid);
        return -EINVAL;
    }

    cq_hwinfo = tsdrv_get_cq_hwinfo(devid, tsid);

    return cq_hwinfo->dfx_cq_irq;
}
KA_EXPORT_SYMBOL_GPL(tsdrv_get_dfx_cq_irq_vector);

int tsdrv_set_ts_cq_hwinfo(u32 devid, u32 tsid, struct tsdrv_cq_hwinfo *cq_hwinfo)
{
    struct tsdrv_cq_hwinfo *cq_hwinfo_l = NULL;
    u32 cq_irq_id;

    if (cq_hwinfo == NULL) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("sq_hwinfo is NULL\n");
        return -ENOMEM;
#endif
    }
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u)\n", devid, tsid);
        return -EINVAL;
#endif
    }
    cq_hwinfo_l = tsdrv_get_cq_hwinfo(devid, tsid);
    cq_hwinfo_l->dfx_cq_irq = cq_hwinfo->dfx_cq_irq;
    cq_hwinfo_l->dfx_cq_request_irq = cq_hwinfo->dfx_cq_request_irq;
    cq_hwinfo_l->cq_irq_num = cq_hwinfo->cq_irq_num;
    cq_hwinfo_l->thread_bind_irq_num = 0;
    for (cq_irq_id = 0; cq_irq_id < cq_hwinfo_l->cq_irq_num; cq_irq_id++) {
        cq_hwinfo_l->cq_irq[cq_irq_id] = cq_hwinfo->cq_irq[cq_irq_id];
        cq_hwinfo_l->cq_request_irq[cq_irq_id] = cq_hwinfo->cq_request_irq[cq_irq_id];
    }

    /* support a thread bind a irq in host when cq irq num is enough */
    if ((tsdrv_get_env_type() == TSDRV_ENV_ONLINE) && (cq_hwinfo_l->cq_irq_num > NORMAL_IRQ_MIN_NUM)) {
        cq_hwinfo_l->thread_bind_irq_num = cq_hwinfo_l->cq_irq_num - NORMAL_IRQ_MIN_NUM;
        cq_hwinfo_l->cq_irq_num = NORMAL_IRQ_MIN_NUM;
    }

    return 0;
}

STATIC void tsdrv_set_db_hwinfo(u32 devid, u32 tsid, struct tsdrv_db_info *db_info)
{
    enum tsdrv_db_type db_type;

    /* replace db_paddr, db_size into proper struct  */
    g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.db_paddr =  db_info->db_paddr;
    g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.dbs = db_info->dbs;
    g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.db_size = db_info->db_size;
    for (db_type = TSDRV_SQ_DB; db_type < TSDRV_MAX_DB; db_type++) {
        tsdrv_set_db_info_t(devid, tsid, db_type, db_info);
    }
}

struct tsdrv_db_hwinfo *tsdrv_get_db_hwinfo(u32 devid, u32 tsid)
{
    return &g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo;
}

int tsdrv_set_ts_db_info(u32 devid, u32 tsid, struct tsdrv_db_info *db_info)
{
    if ((devid >= TSDRV_MAX_DAVINCI_NUM) || (tsid >= DEVDRV_MAX_TS_NUM)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid param, devid(%u) tsid(%u)\n", devid, tsid);
        return -EINVAL;
#endif
    }
    if (db_info == NULL) {
        TSDRV_PRINT_ERR("sq_hwinfo is NULL\n");
        return -ENOMEM;
    }
    if (db_info->num < TSDRV_DB_NUM) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid db num(%u)\n", db_info->num);
        return -EFAULT;
#endif
    }
    if ((db_info->dbs == NULL) || (db_info->db_stride == 0)) {
#ifndef TSDRV_UT
        TSDRV_PRINT_ERR("invalid db_info, size(%lu)\n", db_info->db_stride);
        return -ENODEV;
#endif
    }

    tsdrv_set_db_hwinfo(devid, tsid, db_info);
    return 0;
}

int tsdrv_set_stars_db_info(u32 devid, u32 tsid, struct tsdrv_db_info *db_info)
{
    enum tsdrv_db_type db_type;

    /* replace db_paddr, db_size into stars ctrl mem */
    g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.db_paddr =  db_info->db_paddr;
    g_dev_hwinfo[devid]->hwinfo[tsid].db_hwinfo.db_size = db_info->db_size;
    for (db_type = TSDRV_SQ_DB; db_type <= TSDRV_CQ_DB; db_type++) {
        tsdrv_set_stars_db_info_t(devid, tsid, db_type, db_info);
    }

    return 0;
}

STATIC void tsdrv_mbox_hwinfo_init(struct tsdrv_mbox_hwinfo *mbox_hwinfo, u32 chann_num)
{
#ifndef TSDRV_UT
    u32 chann_id;

    for (chann_id = 0; chann_id < chann_num; chann_id++) {
        mbox_hwinfo[chann_id].chann_id = chann_id;
        mbox_hwinfo[chann_id].mbox_tx = NULL;
        mbox_hwinfo[chann_id].mbox_rx = NULL;
        mbox_hwinfo[chann_id].mbox_ack_irq = 0;
        mbox_hwinfo[chann_id].mbox_rx_irq = 0;
        mbox_hwinfo[chann_id].mbox_size = 0;
    }
#endif
}

STATIC void tsdrv_cq_hwinfo_init(struct tsdrv_cq_hwinfo *cq_hwinfo)
{
#ifndef TSDRV_UT
    u32 cqid;

    for (cqid = 0; cqid < DEVDRV_CQ_IRQ_NUM; cqid++) {
        cq_hwinfo->cq_irq[cqid] = 0;
    }
    cq_hwinfo->cq_irq_num = 0;
    cq_hwinfo->dfx_cq_irq = 0;
#endif
}

STATIC void tsdrv_sq_hwinfo_init(struct tsdrv_sq_hwinfo *sq_hwinfo)
{
#ifndef TSDRV_UT
    sq_hwinfo->size = 0;
    sq_hwinfo->paddr = 0;
#endif
}

STATIC void tsdrv_db_hwinfo_init(struct tsdrv_db_hwinfo *db_hwinfo)
{
#ifndef TSDRV_UT
    u32 db_type_num_l;

    db_hwinfo->db_paddr = 0;
    db_hwinfo->db_size = 0;
    for (db_type_num_l = 0; db_type_num_l < TSDRV_MAX_DB; db_type_num_l++) {
        db_hwinfo->db_hwinfo_t[db_type_num_l].dbs = NULL;
        db_hwinfo->db_hwinfo_t[db_type_num_l].num = 0;
        db_hwinfo->db_hwinfo_t[db_type_num_l].db_stride = 0;
    }
#endif
}

STATIC void tsdrv_hwinfo_init(struct tsdrv_hwinfo *hwinfo, u32 ts_num)
{
#ifndef TSDRV_UT
    u32 tsid;

    for (tsid = 0; tsid < ts_num; tsid++) {
        hwinfo[tsid].tsid = tsid;
        tsdrv_mbox_hwinfo_init(hwinfo[tsid].mbox_hwinfo, TSDRV_MAX_MBOX_CHANN);
        tsdrv_cq_hwinfo_init(&hwinfo[tsid].cq_hwinfo);
        tsdrv_db_hwinfo_init(&hwinfo[tsid].db_hwinfo);
        tsdrv_sq_hwinfo_init(&hwinfo[tsid].sq_hwinfo);
    }
#endif
}

int tsdrv_dev_hwinfo_init(u32 devid)
{
    if (g_dev_hwinfo[devid] == NULL) {
        g_dev_hwinfo[devid] = tsdrv_vzalloc(sizeof(struct tsdrv_hwinfo));
        if (g_dev_hwinfo[devid] == NULL) {
#ifndef TSDRV_UT
            TSDRV_PRINT_ERR("Alloc mem failed. (devid=%u)\n", devid);
            return -ENOMEM;
#endif
        }
    }

    g_dev_hwinfo[devid]->devid = devid;
    tsdrv_hwinfo_init(g_dev_hwinfo[devid]->hwinfo, DEVDRV_MAX_TS_NUM);
    return 0;
}

void tsdrv_dev_hwinfo_exit(u32 devid)
{
    if (g_dev_hwinfo[devid] == NULL) {
        return;
    }

    tsdrv_vfree(g_dev_hwinfo[devid]);
    g_dev_hwinfo[devid] = NULL;
}

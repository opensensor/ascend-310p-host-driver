/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-6-29
*/

#include <linux/securec.h>
#include "hdcdrv_pfstat.h"
#include "hdcdrv_core_com.h"

static struct hdcdrv_pfstat_priv hdcdrv_pfstat_priv_data = { 0 };

STATIC struct hdcdrv_pfstat_priv *hdcdrv_pfstat_get_priv(void)
{
	return &hdcdrv_pfstat_priv_data;
}

STATIC int hdcdrv_pfstat_init_prepare(struct pfstat_latency_info *latency_info)
{
	if (latency_info->pf_latency_attr != NULL) {
		return -ENOMEM;
	}

	latency_info->pf_latency_attr =
		(struct pfstat_latency_attr *)hdcdrv_kzalloc(sizeof(struct pfstat_latency_attr) * HDCDRV_PFSTATE_MAX_CHAN,
		GFP_KERNEL, KA_SUB_MODULE_TYPE_0);
	if (latency_info->pf_latency_attr == NULL) {
		hdcdrv_warn("malloc pf_latency_attr not success.\n");
		return -ENOMEM;
	}
	return 0;
}

STATIC void hdcdrv_pfstat_exit_prepare(struct pfstat_latency_info *latency_info)
{
	if (latency_info == NULL) {
		hdcdrv_warn("latency_info para invalid.\n");
		return;
	}

	if (latency_info->pf_latency_attr == NULL) {
		hdcdrv_warn("pf_latency_attr para invalid.\n");
		return;
	}

	hdcdrv_kfree(latency_info->pf_latency_attr, KA_SUB_MODULE_TYPE_0);
	latency_info->pf_latency_attr = NULL;
	return;
}

STATIC void hdcdrv_pfstat_mutex_init(struct pfstat_priv_info *priv_info)
{
	int i = 0;
	for (i = 0; i < HDCDRV_PFSTATE_MAX_CHAN; i++) {
		mutex_init(&priv_info->mutex);
		priv_info++;
	}
}

STATIC void hdcdrv_pfstat_put_opt_hook(struct pfstat_opt_hook *pfstat_opts)
{
	pfstat_opts->opt_avail = false;

	if (pfstat_opts->pfstat_create_latency != NULL) {
		__symbol_put("pfstat_create_latency");
		pfstat_opts->pfstat_create_latency = NULL;
	}

	if (pfstat_opts->pfstat_delete_latency != NULL) {
		__symbol_put("pfstat_delete_latency");
		pfstat_opts->pfstat_delete_latency = NULL;
	}

	if (pfstat_opts->pfstat_get_timestamp != NULL) {
		__symbol_put("pfstat_get_timestamp");
		pfstat_opts->pfstat_get_timestamp = NULL;
	}

	if (pfstat_opts->pfstat_get_period != NULL) {
		__symbol_put("pfstat_get_period");
		pfstat_opts->pfstat_get_period = NULL;
	}

	if (pfstat_opts->pfstat_update_latency_item != NULL) {
		__symbol_put("pfstat_update_latency_item");
		pfstat_opts->pfstat_update_latency_item = NULL;
	}
}

STATIC int hdcdrv_pfstat_get_opt_hook(struct pfstat_opt_hook *pfstat_opts)
{
	pfstat_opts->pfstat_create_latency =
		(FP_PFSTAT_CREATE_LATENCY)(uintptr_t)__symbol_get("pfstat_create_latency");
	if (pfstat_opts->pfstat_create_latency == NULL) {
		return -EAGAIN;
	}

	pfstat_opts->pfstat_delete_latency =
		(FP_PFSTAT_DELETE_LATENCY)(uintptr_t)__symbol_get("pfstat_delete_latency");
	if (pfstat_opts->pfstat_delete_latency == NULL) {
		hdcdrv_pfstat_put_opt_hook(pfstat_opts);
		return -EAGAIN;
	}

	pfstat_opts->pfstat_get_timestamp =
		(FP_PFSTAT_GET_TIMESTAMP)(uintptr_t)__symbol_get("pfstat_get_timestamp");
	if (pfstat_opts->pfstat_get_timestamp == NULL) {
		hdcdrv_pfstat_put_opt_hook(pfstat_opts);
		return -EAGAIN;
	}

	pfstat_opts->pfstat_get_period =
		(FP_PFSTAT_GET_PERIOD)(uintptr_t)__symbol_get("pfstat_get_period");
	if (pfstat_opts->pfstat_get_period == NULL) {
		hdcdrv_pfstat_put_opt_hook(pfstat_opts);
		return -EAGAIN;
	}

	pfstat_opts->pfstat_update_latency_item =
		(FP_PFSTAT_UPDATE_LATENCY_ITEM)(uintptr_t)__symbol_get("pfstat_update_latency_item");
	if (pfstat_opts->pfstat_update_latency_item == NULL) {
		hdcdrv_pfstat_put_opt_hook(pfstat_opts);
		return -EAGAIN;
	}

	pfstat_opts->opt_avail = true;
	return 0;
}

STATIC inline bool hdcdrv_pfstat_ready_check(struct hdcdrv_pfstat_priv *pfstat_priv)
{
	return pfstat_priv->pfstat_opts.opt_avail;
}

int hdcdrv_pfstat_delete_stats_handle(u32 chan_id)
{
	struct hdcdrv_pfstat_priv *pfstat_priv = hdcdrv_pfstat_get_priv();
    struct pfstat_priv_info *info = pfstat_priv->priv_info;
	int ret = 0;

	if (!hdcdrv_pfstat_ready_check(pfstat_priv)) {
		return ret;
	}

	mutex_lock(&info[chan_id].mutex);
	if (info[chan_id].update_handle == NULL) {
		mutex_unlock(&info[chan_id].mutex);
		return ret;
	}
	ret = pfstat_priv->pfstat_opts.pfstat_delete_latency(info[chan_id].update_handle);
	if (ret != 0) {
		mutex_unlock(&info[chan_id].mutex);
		hdcdrv_warn("delete hdcdrv[%u]_stats_handle not success, ret: %d.\n", chan_id, ret);
		return ret;
	}
	info[chan_id].update_handle = NULL;
	mutex_unlock(&info[chan_id].mutex);
    hdcdrv_info("delete hdcdrv[%u]_stats_handle success.\n", chan_id);
	return 0;
}

int hdcdrv_pfstat_create_stats_handle(u32 chan_id)
{
	struct hdcdrv_pfstat_priv *pfstat_priv = hdcdrv_pfstat_get_priv();
    struct pfstat_priv_info *info = pfstat_priv->priv_info;
	struct pfstat_latency_info *latency_info = &pfstat_priv->latency_info;
    struct pfstat_base_attr base_attr = {0};
	int ret = 0;

	if (!hdcdrv_pfstat_ready_check(pfstat_priv)) {
		return 0;
	}
	if (chan_id >= HDCDRV_PFSTATE_MAX_CHAN) {
		hdcdrv_warn("chan id invalid. (chan_id=%u)\n", chan_id);
		return -1;
	}
	mutex_lock(&info[chan_id].mutex);
	if (info[chan_id].update_handle != NULL) {
		mutex_unlock(&info[chan_id].mutex);
		return 0;
	}

	ret = sprintf_s(base_attr.inst_name, PFSTAT_INST_NAME_SIZE, "hdcdrv-%u", chan_id);
	if (ret <= 0) {
		mutex_unlock(&info[chan_id].mutex);
		hdcdrv_warn("sprintf_s the hdcdrv-%u not success.\n", chan_id);
		return -1;
	}

	latency_info->pf_latency_attr[chan_id].base_attr = base_attr;
	latency_info->pf_latency_attr[chan_id].item_size = PF_MSG_CHAN_LATENCY_STATS_ITEM;
	latency_info->pf_latency_attr[chan_id].threshold = HDCDRV_TX_FINISH_TH;

    ret = pfstat_priv->pfstat_opts.pfstat_create_latency(&latency_info->pf_latency_attr[chan_id], &info[chan_id].update_handle);
    if (ret != 0) {
		mutex_unlock(&info[chan_id].mutex);
        hdcdrv_warn("create hdcdrv[%u]_stats_handle not success. (ret=%d)", chan_id, ret);
		return -1;
    }
	mutex_unlock(&info[chan_id].mutex);
    hdcdrv_info("create hdcdrv[%u]_stats_handle success. (ret=%d)", chan_id, ret);
    return 0;
}

int hdcdrv_pfstat_init(void)
{
	int ret;
	struct hdcdrv_pfstat_priv *pfstat_priv = hdcdrv_pfstat_get_priv();
	(void)memset_s(pfstat_priv, sizeof(struct hdcdrv_pfstat_priv),
		0, sizeof(struct hdcdrv_pfstat_priv));
	ret = hdcdrv_pfstat_init_prepare(&pfstat_priv->latency_info);
	if (ret != 0) {
		hdcdrv_err("pfstat init_prepare failed.\n");
		return ret;
	}

	hdcdrv_pfstat_mutex_init(pfstat_priv->priv_info);

	ret = hdcdrv_pfstat_get_opt_hook(&pfstat_priv->pfstat_opts);
	if (ret != 0) {
        hdcdrv_err("pfstat get_opt_hook failed. (ret=%d)\n", ret);
		return ret;
	}
	hdcdrv_info("pfstat_hooks_init ok!\n");
	return 0;
}

void hdcdrv_pfstat_exit(void)
{
	struct hdcdrv_pfstat_priv *pfstat_priv = hdcdrv_pfstat_get_priv();
	hdcdrv_pfstat_put_opt_hook(&pfstat_priv->pfstat_opts);
	hdcdrv_pfstat_exit_prepare(&pfstat_priv->latency_info);
	hdcdrv_info("pfstat exit succuss.\n");
}

void hdcdrv_pfstat_update_latency(u32 chan_id, enum latency_dot_idx latency_dot, u64 before_time)
{
	struct hdcdrv_pfstat_priv *pfstat_priv = hdcdrv_pfstat_get_priv();
	struct pfstat_opt_hook *opt = &pfstat_priv->pfstat_opts;
	u64 current_time, delay;
	int ret = 0;

	if (!hdcdrv_pfstat_ready_check(pfstat_priv)) {
		return;
	}

	ret = opt->pfstat_get_timestamp(&current_time);
	if (ret != 0) {
		hdcdrv_warn_limit("pfstat_get_timestamp no success. (ret=%d)\n", ret);
		return;
	}

	delay = opt->pfstat_get_period(before_time, current_time);

	ret = opt->pfstat_update_latency_item(pfstat_priv->priv_info[chan_id].update_handle, (u32)latency_dot, delay);
	if (ret != 0) {
		hdcdrv_warn_limit("pfstat_update_latency_item not success. (chan_id=%u, ret=%d)\n", chan_id, ret);
	}
	return;
}

void hdcdrv_pfstat_get_timestamp(u64 *timestamp)
{
	struct hdcdrv_pfstat_priv *pfstat_priv = hdcdrv_pfstat_get_priv();
	struct pfstat_opt_hook *opt = &pfstat_priv->pfstat_opts;
	int ret = 0;

	if (!hdcdrv_pfstat_ready_check(pfstat_priv)) {
		return;
	}

	ret = opt->pfstat_get_timestamp(timestamp);
	if (ret != 0) {
		hdcdrv_warn_limit("pfstat_get_timestamp no success. (ret=%d)\n", ret);
		return;
	}
	return;
}

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
 * Create: 2023-2-20
 */

#ifndef UDA_DEV_H
#define UDA_DEV_H

#include "ka_base_pub.h"

#include "securec.h"

#include "uda_access.h"

#define UDA_STATUS_INIT_MASK (0x1 << 0)
#define UDA_STATUS_SUSPEND_MASK (0x1 << 1)
#define UDA_STATUS_MIA_MASK (0x1 << 2)
#define UDA_STATUS_HOT_RESET_MASK (0x1 << 3)
#define UDA_STATUS_REMOVED_MASK (0x1 << 4)
#define UDA_STATUS_PRE_HOT_RESET_MASK (0x1 << 5)

struct uda_dev_inst {
    ka_kref_t ref;
    u32 udevid;
    u32 inst_id;
    u32 status;
    u32 agent_flag;
    struct uda_dev_type type;
    struct uda_dev_para para;
    struct uda_mia_dev_para mia_para;
    struct uda_access access;
    void *agent_dev;
};

static inline bool uda_is_init_status(u32 status)
{
    return ((status & UDA_STATUS_INIT_MASK) != 0);
}

static inline bool uda_is_suspend_status(u32 status)
{
    return ((status & UDA_STATUS_SUSPEND_MASK) != 0);
}

static inline bool uda_is_mia_status(u32 status)
{
    return ((status & UDA_STATUS_MIA_MASK) != 0);
}

static inline bool uda_is_hotreset_status(u32 status)
{
    return ((status & UDA_STATUS_HOT_RESET_MASK) != 0);
}

static inline bool uda_is_pre_hotreset_status(u32 status)
{
    return ((status & UDA_STATUS_PRE_HOT_RESET_MASK) != 0);
}

static inline bool uda_is_removed_status(u32 status)
{
    return ((status & UDA_STATUS_REMOVED_MASK) != 0);
}

static inline bool uda_is_init_status_conflict(u32 status, enum uda_notified_action action)
{
    return ((uda_is_init_status(status) && (action == UDA_INIT))
        || (!uda_is_init_status(status) && (action == UDA_UNINIT)));
}

static inline bool uda_is_suspend_status_conflict(u32 status, enum uda_notified_action action)
{
    return ((uda_is_suspend_status(status) && (action == UDA_SUSPEND))
        || (!uda_is_suspend_status(status) && (action == UDA_RESUME)));
}

static inline bool uda_is_mia_status_conflict(u32 status, enum uda_notified_action action)
{
    return ((uda_is_mia_status(status) && (action == UDA_TO_MIA))
        || (!uda_is_mia_status(status) && (action == UDA_TO_SIA)));
}

static inline bool uda_is_hotreset_status_conflict(u32 status, enum uda_notified_action action)
{
    return ((uda_is_hotreset_status(status) && (action == UDA_HOTRESET))
        || (!uda_is_hotreset_status(status) && (action == UDA_HOTRESET_CANCEL)));
}

static inline bool uda_is_pre_hotreset_status_conflict(u32 status, enum uda_notified_action action)
{
    return ((uda_is_pre_hotreset_status(status) && (action == UDA_PRE_HOTRESET))
        || (!uda_is_pre_hotreset_status(status) && (action == UDA_PRE_HOTRESET_CANCEL)));
}

static inline bool uda_is_action_conflict(u32 status, enum uda_notified_action action)
{
    return (uda_is_init_status_conflict(status, action) || uda_is_suspend_status_conflict(status, action)
        || uda_is_mia_status_conflict(status, action) || uda_is_hotreset_status_conflict(status, action)
        || uda_is_pre_hotreset_status_conflict(status, action));
}

static inline void uda_update_status_by_action(u32 *status, enum uda_notified_action action)
{
    if (action == UDA_INIT) {
        *status = UDA_STATUS_INIT_MASK;
    } else if (action == UDA_UNINIT) {
        *status &= ~UDA_STATUS_INIT_MASK;
    } else if (action == UDA_SUSPEND) {
        *status |= UDA_STATUS_SUSPEND_MASK;
    } else if (action == UDA_RESUME) {
        *status &= ~UDA_STATUS_SUSPEND_MASK;
    } else if (action == UDA_TO_MIA) {
        *status |= UDA_STATUS_MIA_MASK;
    } else if (action == UDA_TO_SIA) {
        *status &= ~UDA_STATUS_MIA_MASK;
    } else if (action == UDA_HOTRESET) {
        *status |= UDA_STATUS_HOT_RESET_MASK;
    } else if (action == UDA_HOTRESET_CANCEL) {
        *status &= ~UDA_STATUS_HOT_RESET_MASK;
    } else if (action == UDA_REMOVE) {
        *status |= UDA_STATUS_REMOVED_MASK;
    } else if (action == UDA_PRE_HOTRESET) {
        *status |= UDA_STATUS_PRE_HOT_RESET_MASK;
    } else if (action == UDA_PRE_HOTRESET_CANCEL) {
        *status &= ~UDA_STATUS_PRE_HOT_RESET_MASK;
    }
}

static inline int uda_dev_type_valid_check(struct uda_dev_type *type)
{
    if ((type->hw < 0) || (type->hw >= UDA_HW_MAX) || (type->object < 0) || (type->object >= UDA_OBJECT_MAX)
        || (type->location < 0) || (type->location >= UDA_LOCATION_MAX)
        || (type->prop < 0) || (type->prop >= UDA_PROP_MAX)) {
        uda_err("Invalid value. (hw=%d; object=%d; location=%d; prop=%d)\n",
            type->hw, type->object, type->location, type->prop);
        return -EINVAL;
    }

    return 0;
}

static inline bool uda_dev_type_is_match(struct uda_dev_type *type1, struct uda_dev_type *type2)
{
    return ((type1->hw == type2->hw) && (type1->object == type2->object)
        && (type1->location == type2->location) && (type1->prop == type2->prop));
}

#define TYPE_BUF_LEN 64
void uda_type_to_string(struct uda_dev_type *type, char buf[], u32 buf_len);
static inline void uda_show_type(const char *reason, struct uda_dev_type *type)
{
    char buf[TYPE_BUF_LEN];
    uda_type_to_string(type, buf, TYPE_BUF_LEN);
    uda_info("%s: %s\n", reason, buf);
}

struct uda_dev_inst *uda_dev_inst_get(u32 udevid);
void uda_dev_inst_put(struct uda_dev_inst *dev_inst);
int uda_udevid_to_remote_udevid(u32 udevid, u32 *remote_udevid);
int uda_remote_udevid_to_udevid(u32 remote_udevid, u32 *udevid);

int uda_dev_init(void);
void uda_dev_uninit(void);

#endif


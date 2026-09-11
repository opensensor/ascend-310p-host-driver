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

#include "ka_common_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"

#include "uda_notifier.h"
#include "dbl_mem_alloc_interface.h"
#include "uda_dev.h"

static ka_mutex_t uda_dev_mutex;
static ka_rwlock_t uda_dev_lock;
static struct uda_dev_inst *dev_insts[UDA_UDEV_MAX_NUM];

void uda_type_to_string(struct uda_dev_type *type, char buf[], u32 buf_len)
{
    int ret = 0;
    static const char *uda_hw_name[UDA_HW_MAX] = {
        [UDA_DAVINCI] = "davinci",
        [UDA_KUNPENG] = "kunpeng",
    };

    static const char *uda_obj_name[UDA_OBJECT_MAX] = {
        [UDA_ENTITY] = "entity",
        [UDA_AGENT] = "agent",
    };

    static const char *uda_loc_name[UDA_LOCATION_MAX] = {
        [UDA_LOCAL] = "local",
        [UDA_NEAR] = "near",
        [UDA_REMOTE] = "remote",
    };

    static const char *uda_prop_name[UDA_PROP_MAX] = {
        [UDA_REAL] = "real",
        [UDA_VIRTUAL] = "virtual",
        [UDA_REAL_SEC_EH] = "real_sec_eh",
    };

    ret = sprintf_s(buf, buf_len, "%s %s %s %s",
        uda_hw_name[type->hw], uda_obj_name[type->object], uda_loc_name[type->location], uda_prop_name[type->prop]);
    if (ret <= 0) {
        uda_err("Invoke sprintf_s error.\n");
    }
}

u32 uda_get_udev_max_num(void)
{
    return UDA_UDEV_MAX_NUM;
}
KA_EXPORT_SYMBOL(uda_get_udev_max_num);

bool uda_is_phy_dev(u32 udevid)
{
    return (udevid < UDA_MAX_PHY_DEV_NUM);
}
KA_EXPORT_SYMBOL(uda_is_phy_dev);

bool uda_is_pf_dev(u32 udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    bool is_pf = true;

    if (udevid >= UDA_MAX_PHY_DEV_NUM) {
        return false;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return false;
    }

    is_pf = dev_inst->para.pf_flag == 1 ? true : false;
    uda_dev_inst_put(dev_inst);
    return is_pf;
}
EXPORT_SYMBOL(uda_is_pf_dev);

static int uda_alloc_udevid(u32 start, u32 end, u32 *udevid) // [start, end)
{
    u32 i;

    for (i = start; i < end; i++) {
        if (dev_insts[i] == NULL) {
            *udevid = i;
            return 0;
        }
    }

    return -ENOSPC;
}

u32 uda_make_udevid(struct uda_mia_dev_para *mia_para)
{
    return mia_para->phy_devid * UDA_SUB_DEV_MAX_NUM + mia_para->sub_devid + UDA_MIA_UDEV_OFFSET;
}
KA_EXPORT_SYMBOL(uda_make_udevid);

static struct uda_dev_inst *uda_create_dev_inst(struct uda_dev_type *type, struct uda_dev_para *para)
{
    static u32 inst_id = 0;
    struct uda_dev_inst *dev_inst = dbl_kzalloc(sizeof(struct uda_dev_inst), KA_GFP_KERNEL);
    if (dev_inst == NULL) {
        uda_err("Out of memory.\n");
        return NULL;
    }

    dev_inst->type = *type;
    dev_inst->para = *para;
    dev_inst->agent_flag = 0;
    dev_inst->inst_id = inst_id++;
    ka_base_kref_init(&dev_inst->ref);

    return dev_inst;
}

static void uda_destroy_dev_inst(struct uda_dev_inst *dev_inst)
{
    dbl_kfree(dev_inst);
}

struct uda_dev_inst *uda_dev_inst_get(u32 udevid)
{
    struct uda_dev_inst *dev_inst = NULL;

    if (udevid >= UDA_UDEV_MAX_NUM) {
        return NULL;
    }

    ka_task_read_lock_bh(&uda_dev_lock);
    dev_inst = dev_insts[udevid];
    if (dev_inst != NULL) {
        ka_base_kref_get(&dev_inst->ref);
    }
    ka_task_read_unlock_bh(&uda_dev_lock);

    return dev_inst;
}

static void uda_dev_inst_release(ka_kref_t *kref)
{
    struct uda_dev_inst *dev_inst = ka_container_of(kref, struct uda_dev_inst, ref);

    uda_info("Remove dev success. (udevid=%u; agent_flag=%u)\n", dev_inst->udevid, dev_inst->agent_flag);
    uda_destroy_dev_inst(dev_inst);
}

void uda_dev_inst_put(struct uda_dev_inst *dev_inst)
{
    ka_base_kref_put(&dev_inst->ref, uda_dev_inst_release);
}

static int _uda_add_dev_ex(struct uda_dev_type *type, struct uda_dev_para *para,
    struct uda_mia_dev_para *mia_para, u32 *udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if (type->object == UDA_ENTITY) {
        if ((para->udevid != UDA_INVALID_UDEVID) || (mia_para != NULL)) {
            *udevid = (mia_para == NULL) ? para->udevid : uda_make_udevid(mia_para);
            if (dev_insts[*udevid] != NULL) {
                uda_warn("Has add entity dev. (udevid=%u)\n", *udevid);
                dev_insts[*udevid]->para.dev = para->dev;
                dev_insts[*udevid]->status = 0;
                return 0;
            }
        } else {
            ret = uda_alloc_udevid(0, UDA_MIA_UDEV_OFFSET, udevid);
            if (ret != 0) {
                uda_err("No udevid res\n");
                return ret;
            }
        }
    } else { /* UDA_AGENT */
        *udevid = para->udevid;
        if (dev_insts[para->udevid] == NULL) {
            uda_err("No entity dev. (udevid=%u)\n", para->udevid);
            return -EINVAL;
        }

        if (dev_insts[para->udevid]->agent_dev != NULL) {
            uda_warn("Has add agent dev. (udevid=%u)\n", para->udevid);
            ((struct uda_dev_inst *)dev_insts[para->udevid]->agent_dev)->para.dev = para->dev;
            ((struct uda_dev_inst *)dev_insts[para->udevid]->agent_dev)->status = 0;
            return 0;
        }
    }

    dev_inst = uda_create_dev_inst(type, para);
    if (dev_inst == NULL) {
        return -ENOMEM;
    }

    dev_inst->udevid = *udevid;
    if (mia_para != NULL) {
        dev_inst->mia_para = *mia_para;
    }

    if (type->object == UDA_ENTITY) {
#ifdef CFG_FEATURE_DEVID_TRANS
        ret = uda_access_add_dev(dev_inst->udevid, &dev_inst->access);
        if (ret != 0) {
            uda_destroy_dev_inst(dev_inst);
            return ret;
        }
#endif
        dev_insts[*udevid] = dev_inst;
    } else {
        dev_inst->agent_flag = 1;
        dev_insts[para->udevid]->agent_dev = dev_inst;
        if (dev_inst->para.remote_udevid != UDA_INVALID_UDEVID) {
            dev_insts[para->udevid]->para.remote_udevid = dev_inst->para.remote_udevid;
        }
    }

    uda_info("Add dev success. (udevid=%u; agent_flag=%u)\n", *udevid, dev_inst->agent_flag);
    uda_show_type("Add", type);
    return 0;
}

static int uda_add_dev_ex(struct uda_dev_type *type, struct uda_dev_para *para,
    struct uda_mia_dev_para *mia_para, u32 *udevid)
{
    int ret;

    if ((type == NULL) || (para == NULL) || (udevid == NULL)) {
        uda_err("Null ptr.\n");
        return -EINVAL;
    }

    ret = uda_dev_type_valid_check(type);
    if (ret != 0) {
        uda_err("Invalid type.\n");
        return ret;
    }

    if (type->object == UDA_ENTITY) {
        if (((para->udevid != UDA_INVALID_UDEVID) && !uda_is_phy_dev(para->udevid))) {
            uda_err("Invalid para. (udevid=%u)\n", para->udevid);
            return -EINVAL;
        }
    } else {
        if (para->udevid >= UDA_UDEV_MAX_NUM) {
            uda_err("Invalid para. (udevid=%u)\n", para->udevid);
            return -EINVAL;
        }
    }

    ka_task_mutex_lock(&uda_dev_mutex);
    ret = _uda_add_dev_ex(type, para, mia_para, udevid);
    ka_task_mutex_unlock(&uda_dev_mutex);
    if (ret == 0) {
        (void)uda_notifier_call(*udevid, type, UDA_INIT);
    }

    return ret;
}

int uda_add_dev(struct uda_dev_type *type, struct uda_dev_para *para, u32 *udevid)
{
    return uda_add_dev_ex(type, para, NULL, udevid);
}
KA_EXPORT_SYMBOL(uda_add_dev);

static int uda_mia_dev_check(struct uda_mia_dev_para *mia_para)
{
    if (!uda_is_phy_dev(mia_para->phy_devid) || (mia_para->sub_devid >= UDA_SUB_DEV_MAX_NUM)
        || (uda_make_udevid(mia_para) >= UDA_UDEV_MAX_NUM)) {
        uda_err("Invalid para. (phy_devid=%u; sub_devid=%u)\n", mia_para->phy_devid, mia_para->sub_devid);
        return -EINVAL;
    }

    return 0;
}

int uda_add_mia_dev(struct uda_dev_type *type, struct uda_dev_para *para,
    struct uda_mia_dev_para *mia_para, u32 *udevid)
{
    if ((para == NULL) || (mia_para == NULL)) {
        uda_err("Null ptr.\n");
        return -EINVAL;
    }

    if (para->udevid != UDA_INVALID_UDEVID) {
        uda_err("Mia udevid cannot be specified. (udevid=%u)\n", para->udevid);
        return -EINVAL;
    }

    if (uda_mia_dev_check(mia_para) != 0) {
        return -EINVAL;
    }

    para->pf_flag = 0;
    return uda_add_dev_ex(type, para, mia_para, udevid);
}
KA_EXPORT_SYMBOL(uda_add_mia_dev);

int _uda_remove_dev(struct uda_dev_inst *dev_inst, struct uda_dev_type *type)
{
    if (uda_dev_type_is_match(type, &dev_inst->type)) {
#ifdef CFG_FEATURE_DEVID_TRANS
        int ret = uda_access_remove_dev(dev_inst->udevid, &dev_inst->access);
        if (ret != 0) {
            return ret;
        }
#endif
        ka_task_write_lock_bh(&uda_dev_lock);
        dev_insts[dev_inst->udevid] = NULL;  /* set inst invalid, so other thread will not get it */
        ka_task_write_unlock_bh(&uda_dev_lock);
        if (dev_inst->agent_dev != NULL) {
            struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
            uda_warn("Not remove agent dev. (udevid=%d)\n", dev_inst->udevid);
            ka_base_kref_put(&agent_dev_inst->ref, uda_dev_inst_release);
        }
        ka_base_kref_put(&dev_inst->ref, uda_dev_inst_release);
    } else {
        struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
        dev_inst->agent_dev = NULL;
        ka_base_kref_put(&agent_dev_inst->ref, uda_dev_inst_release);
    }

    return 0;
}

int uda_remove_dev(struct uda_dev_type *type, u32 udevid)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if ((type == NULL) || (udevid >= UDA_UDEV_MAX_NUM)) {
        uda_err("Invalid para. (udevid=%u)\n", udevid);
        return  -EINVAL;
    }

    ret = uda_dev_type_valid_check(type);
    if (ret != 0) {
        uda_err("Invalid type.\n");
        return ret;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (!uda_dev_type_is_match(type, &dev_inst->type)) {
        struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
        if ((agent_dev_inst == NULL) || (!uda_dev_type_is_match(type, &agent_dev_inst->type))) {
            uda_err("Remove type mismatch. (udevid=%d)\n", udevid);
            uda_show_type("Remove type", type);
            uda_show_type("Dev type", &dev_inst->type);
            uda_dev_inst_put(dev_inst);
            return -ENODEV;
        }
    }

    (void)uda_notifier_call(udevid, type, UDA_UNINIT);

#ifndef EMU_ST
    if (!uda_is_phy_dev(udevid)) {
#else
    {
#endif
        ka_task_mutex_lock(&uda_dev_mutex);
        ret = _uda_remove_dev(dev_inst, type);
        ka_task_mutex_unlock(&uda_dev_mutex);
    }
    dev_inst->para.dev = NULL;
    uda_update_status_by_action(&dev_inst->status, UDA_REMOVE);

    if (ret != 0) {
        (void)uda_notifier_call(udevid, type, UDA_INIT);
    }

    uda_dev_inst_put(dev_inst);

    return ret;
}
KA_EXPORT_SYMBOL(uda_remove_dev);

static void uda_dev_ctrl_restore(struct uda_dev_inst *dev_inst, enum uda_dev_ctrl_cmd cmd)
{
    static enum uda_notified_action ctrl_cmd_to_restore_action[UDA_CTRL_MAX] = {
        [UDA_CTRL_SUSPEND] = UDA_RESUME,
        [UDA_CTRL_RESUME] = UDA_SUSPEND,
        [UDA_CTRL_TO_MIA] = UDA_TO_SIA,
        [UDA_CTRL_TO_SIA] = UDA_TO_MIA,
        [UDA_CTRL_MIA_CHANGE] = UDA_ACTION_MAX,
        [UDA_CTRL_REMOVE] = UDA_ACTION_MAX,
        [UDA_CTRL_HOTRESET] = UDA_HOTRESET_CANCEL,
        [UDA_CTRL_HOTRESET_CANCEL] = UDA_ACTION_MAX,
        [UDA_CTRL_SHUTDOWN] = UDA_ACTION_MAX,
        [UDA_CTRL_PRE_HOTRESET] = UDA_PRE_HOTRESET_CANCEL,
        [UDA_CTRL_PRE_HOTRESET_CANCEL] = UDA_ACTION_MAX,
    };

    if (ctrl_cmd_to_restore_action[cmd] != UDA_ACTION_MAX) {
        (void)uda_notifier_call(dev_inst->udevid, &dev_inst->type, ctrl_cmd_to_restore_action[cmd]);
    }
}

static int _uda_dev_ctrl(struct uda_dev_inst *dev_inst, enum uda_dev_ctrl_cmd cmd)
{
    int ret;
    static enum uda_notified_action ctrl_cmd_to_action[UDA_CTRL_MAX] = {
        [UDA_CTRL_SUSPEND] = UDA_SUSPEND,
        [UDA_CTRL_RESUME] = UDA_RESUME,
        [UDA_CTRL_TO_MIA] = UDA_TO_MIA,
        [UDA_CTRL_TO_SIA] = UDA_TO_SIA,
        [UDA_CTRL_MIA_CHANGE] = UDA_MIA_CHANGE,
        [UDA_CTRL_REMOVE] = UDA_REMOVE,
        [UDA_CTRL_HOTRESET] = UDA_HOTRESET,
        [UDA_CTRL_HOTRESET_CANCEL] = UDA_HOTRESET_CANCEL,
        [UDA_CTRL_SHUTDOWN] = UDA_SHUTDOWN,
        [UDA_CTRL_PRE_HOTRESET] = UDA_PRE_HOTRESET,
        [UDA_CTRL_PRE_HOTRESET_CANCEL] = UDA_PRE_HOTRESET_CANCEL,
        [UDA_CTRL_COMMUNICATION_LOST] = UDA_COMMUNICATION_LOST,
    };

    if (uda_is_action_conflict(dev_inst->status, ctrl_cmd_to_action[cmd])) {
        uda_err("Invalid cmd. (udevid=%u; cmd=%d; status=%x)\n", dev_inst->udevid, cmd, dev_inst->status);
        return -EINVAL;
    }

    ret = uda_notifier_call(dev_inst->udevid, &dev_inst->type, ctrl_cmd_to_action[cmd]);
    if (ret != 0) {
        uda_dev_ctrl_restore(dev_inst, cmd);
        uda_err("Notifier call failed. (udevid=%d; cmd=%d; ret=%d)\n", dev_inst->udevid, cmd, ret);
        return ret;
    }

    uda_update_status_by_action(&dev_inst->status, ctrl_cmd_to_action[cmd]);

    uda_info("Dev ctrl success. (udevid=%d; cmd=%d)\n", dev_inst->udevid, cmd);
    return 0;
}

int uda_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if ((cmd < 0) || (cmd >= UDA_CTRL_MAX)) {
        uda_err("Invalid cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
        return -EINVAL;
    }

    if (uda_is_phy_dev(udevid)) {
        if ((cmd == UDA_CTRL_MIA_CHANGE) || (cmd == UDA_CTRL_REMOVE)) {
            uda_err("Phy dev not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    } else {
        if ((cmd == UDA_CTRL_SUSPEND) || (cmd == UDA_CTRL_RESUME)
            || (cmd == UDA_CTRL_TO_MIA) || (cmd == UDA_CTRL_TO_SIA)
            || (cmd == UDA_CTRL_SHUTDOWN)) {
            uda_err("Mia dev not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        return -EINVAL;
    }

    ret = _uda_dev_ctrl(dev_inst, cmd);
    uda_dev_inst_put(dev_inst);

    return ret;
}
KA_EXPORT_SYMBOL(uda_dev_ctrl);

int uda_agent_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd)
{
    struct uda_dev_inst *dev_inst = NULL;
    int ret;

    if ((cmd < 0) || (cmd >= UDA_CTRL_MAX)) {
        uda_err("Invalid cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
        return -EINVAL;
    }

    if (uda_is_phy_dev(udevid)) {
        if ((cmd == UDA_CTRL_MIA_CHANGE) || (cmd == UDA_CTRL_REMOVE)) {
            uda_err("Phy dev not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    } else {
        if ((cmd == UDA_CTRL_SUSPEND) || (cmd == UDA_CTRL_RESUME)
            || (cmd == UDA_CTRL_TO_MIA) || (cmd == UDA_CTRL_TO_SIA)) {
            uda_err("Mia dev not support cmd. (udevid=%u; cmd=%d)\n", udevid, cmd);
            return -EINVAL;
        }
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("uda_dev_inst_get fail. (udevid=%u; cmd=%d)\n", udevid, cmd);
        return -EINVAL;
    }

    if (dev_inst->agent_dev == NULL) {
        uda_err("agent dev not exist. (udevid=%u; cmd=%d)\n", udevid, cmd);
        uda_dev_inst_put(dev_inst);
        return -EINVAL;
    }

    ret = _uda_dev_ctrl((struct uda_dev_inst *)dev_inst->agent_dev, cmd);
    uda_dev_inst_put(dev_inst);

    return ret;
}
KA_EXPORT_SYMBOL(uda_agent_dev_ctrl);

int uda_dev_set_remote_udevid(u32 udevid, u32 remote_udevid)
{
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    dev_inst->para.remote_udevid = remote_udevid;
    uda_dev_inst_put(dev_inst);

    return 0;
}
KA_EXPORT_SYMBOL(uda_dev_set_remote_udevid);

int uda_dev_get_remote_udevid(u32 udevid, u32 *remote_udevid)
{
    return uda_udevid_to_remote_udevid(udevid, remote_udevid);
}
KA_EXPORT_SYMBOL(uda_dev_get_remote_udevid);

bool uda_is_support_udev_mng(void)
{
#ifdef CFG_FEATURE_SURPORT_UDEV_MNG
    return true;
#else
    return false;
#endif
}
KA_EXPORT_SYMBOL(uda_is_support_udev_mng);

static ka_device_t *_uda_get_device(u32 udevid, bool is_agent)
{
    ka_device_t *dev = NULL;
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return NULL;
    }

    if (is_agent) {
        struct uda_dev_inst *agent_dev_inst = dev_inst->agent_dev;
        if (agent_dev_inst != NULL) {
            dev = agent_dev_inst->para.dev;
        }
    } else {
        dev = dev_inst->para.dev;
    }

    uda_dev_inst_put(dev_inst);

    return dev;
}

ka_device_t *uda_get_device(u32 udevid)
{
    return _uda_get_device(udevid, false);
}
KA_EXPORT_SYMBOL(uda_get_device);

ka_device_t *uda_get_agent_device(u32 udevid)
{
    return _uda_get_device(udevid, true);
}
KA_EXPORT_SYMBOL(uda_get_agent_device);

int uda_set_chip_type(u32 udevid, u32 chip_type)
{
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    dev_inst->para.chip_type = chip_type;
    uda_dev_inst_put(dev_inst);

    return 0;
}
KA_EXPORT_SYMBOL(uda_set_chip_type);

u32 uda_get_chip_type(u32 udevid)
{
    u32 chip_type = HISI_CHIP_UNKNOWN;
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return HISI_CHIP_UNKNOWN;
    }

    chip_type = dev_inst->para.chip_type;
    uda_dev_inst_put(dev_inst);

    return chip_type;
}
KA_EXPORT_SYMBOL(uda_get_chip_type);

int uda_udevid_to_mia_devid(u32 udevid, struct uda_mia_dev_para *mia_para)
{
    struct uda_dev_inst *dev_inst = NULL;

    if (mia_para == NULL) {
        uda_err("Null ptr\n");
        return -EINVAL;
    }

    if (udevid < UDA_MIA_UDEV_OFFSET) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    *mia_para = dev_inst->mia_para;

    uda_dev_inst_put(dev_inst);
    return 0;
}
KA_EXPORT_SYMBOL(uda_udevid_to_mia_devid);

int uda_mia_devid_to_udevid(struct uda_mia_dev_para *mia_para, u32 *udevid)
{
    struct uda_dev_inst *dev_inst = NULL;

    if ((mia_para == NULL) || (udevid == NULL)) {
        uda_err("Null ptr\n");
        return -EINVAL;
    }

    if (uda_mia_dev_check(mia_para) != 0) {
        return -EINVAL;
    }

    *udevid = uda_make_udevid(mia_para);

    dev_inst = uda_dev_inst_get(*udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid para. (phy_devid=%u; sub_devid=%u)\n", mia_para->phy_devid, mia_para->sub_devid);
        return -EINVAL;
    }

    uda_dev_inst_put(dev_inst);
    return 0;
}
KA_EXPORT_SYMBOL(uda_mia_devid_to_udevid);

int uda_udevid_to_remote_udevid(u32 udevid, u32 *remote_udevid)
{
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        uda_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    /* current only support ep device, rc is itself */
    *remote_udevid = ((dev_inst->agent_dev != NULL) || (dev_inst->para.remote_udevid != UDA_INVALID_UDEVID)) ?
        dev_inst->para.remote_udevid : udevid;
    uda_dev_inst_put(dev_inst);

    if (*remote_udevid == UDA_INVALID_UDEVID) {
        uda_warn("Remote udevid not config. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    return 0;
}

int uda_remote_udevid_to_udevid(u32 remote_udevid, u32 *udevid)
{
    u32 i;

    for (i = 0; i < UDA_UDEV_MAX_NUM; i++) {
        struct uda_dev_inst *dev_inst = uda_dev_inst_get(i);
        if (dev_inst == NULL) {
            continue;
        }

        if (dev_inst->para.remote_udevid == remote_udevid) {
            *udevid = i;
            uda_dev_inst_put(dev_inst);
            return 0;
        }
        uda_dev_inst_put(dev_inst);
    }

    return -EINVAL;
}

u32 uda_get_dev_num(void)
{
    u32 udevid, udev_num = 0;

    for (udevid = 0; udevid < UDA_MAX_PHY_DEV_NUM; udevid++) {
        if (dev_insts[udevid] != NULL) {
            udev_num++;
        }
    }

    return udev_num;
}
KA_EXPORT_SYMBOL(uda_get_dev_num);

u32 uda_get_mia_dev_num(void)
{
    u32 udevid, udev_num = 0;

    for (udevid = UDA_MIA_UDEV_OFFSET; udevid < UDA_UDEV_MAX_NUM; udevid++) {
        if (dev_insts[udevid] != NULL) {
            udev_num++;
        }
    }

    return udev_num;
}
KA_EXPORT_SYMBOL(uda_get_mia_dev_num);

bool uda_is_udevid_exist(u32 udevid)
{
    struct uda_dev_inst *dev_inst = uda_dev_inst_get(udevid);
    if (dev_inst == NULL) {
        return false;
    }
    uda_dev_inst_put(dev_inst);

    return true;
}
KA_EXPORT_SYMBOL(uda_is_udevid_exist);

int uda_dev_init(void)
{
    (void)memset_s(dev_insts, sizeof(dev_insts), 0, sizeof(dev_insts));
    ka_task_mutex_init(&uda_dev_mutex);
    ka_task_rwlock_init(&uda_dev_lock);

    return 0;
}

void uda_dev_uninit(void)
{
    u32 udevid;

    for (udevid = 0; udevid < UDA_UDEV_MAX_NUM; udevid++) {
        if (dev_insts[udevid] != NULL) {
            if (dev_insts[udevid]->agent_dev != NULL) {
                uda_destroy_dev_inst(dev_insts[udevid]->agent_dev);
            }
            uda_destroy_dev_inst(dev_insts[udevid]);
        }
    }
}


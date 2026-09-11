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
* Create: 2023-2-16
*/

#ifndef UDA_H__
#define UDA_H__

#include <linux/device.h>

/*
    UDA: unified device access
    phy_devid: Indicates the device that is added by uda_add_dev, it is a complete device,
               excluding devices split by mdev or sriov.
    udevid: unified device id, include davinci physical device, mia device and kunpeng device,
            phy_devid is equal to udevid
    devid: logical device id from the user view
*/


/* Method of the device prober */

enum uda_dev_hw {
    UDA_DAVINCI,
    UDA_KUNPENG,
    UDA_HW_MAX
};

static const char *uda_dev_hw_name[UDA_HW_MAX] = {"Davinci", "Kunpeng"};

static inline u32 uda_get_dev_hw_type_by_name(const char *name)
{
    u32 dev_hw_type;

    for (dev_hw_type = UDA_DAVINCI; dev_hw_type < UDA_HW_MAX; dev_hw_type++) {
        if (strcmp(uda_dev_hw_name[dev_hw_type], name) == 0) {
            break;
        }
    }

    return dev_hw_type;
}

enum uda_dev_object {
    UDA_ENTITY, /* Entity of the device */
    UDA_AGENT, /* Agent of the entity device */
    UDA_OBJECT_MAX
};

enum uda_dev_location {
    UDA_LOCAL, /* Local device, with all control capabilities of the SOC */
    UDA_NEAR, /* Near devices connected through buses such as PCIe and Ub, with partial control capabilities of SOC. */
    UDA_REMOTE, /* Remote device connected through the network cannot directly control the SOC. */
    UDA_LOCATION_MAX
};

enum uda_dev_prop {
    UDA_REAL, /* Indicates the device that is directly reported by the hardware, include phy device and sriov device */
    UDA_VIRTUAL, /* Indicates the virtual devices split from real device, use methods such as mdev. */
    UDA_REAL_SEC_EH, /* Indicates sriov device which need host do security Hardening */
    UDA_PROP_MAX
};

struct uda_dev_type {
    enum uda_dev_hw hw;
    enum uda_dev_object object;
    enum uda_dev_location location;
    enum uda_dev_prop prop;
};

#define UDA_INVALID_UDEVID 0xffffffffU

struct uda_dev_para {
    u32 udevid; /* 0xffffffff, uda distribute devid, else use this devid if not conflict;
                  UDA_AGENT set existing devid; */
    u32 remote_udevid;
    u32 chip_type;
    struct device *dev;
    int pf_flag;  /* 1: pf; 0: vf */
};

struct uda_mia_dev_para {
    u32 phy_devid;
    u32 sub_devid;
};

static inline void uda_dev_type_pack(struct uda_dev_type *type,
    enum uda_dev_hw hw, enum uda_dev_object object, enum uda_dev_location location, enum uda_dev_prop prop)
{
    type->hw = hw;
    type->object = object;
    type->location = location;
    type->prop = prop;
}

static inline void uda_davinci_local_real_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_LOCAL, UDA_REAL);
}

static inline void uda_davinci_local_virtual_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_LOCAL, UDA_VIRTUAL);
}

static inline void uda_davinci_near_real_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL);
}

static inline void uda_davinci_near_virtual_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_VIRTUAL);
}

static inline void uda_davinci_near_virtual_agent_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_AGENT, UDA_NEAR, UDA_VIRTUAL);
}

static inline void uda_davinci_local_real_agent_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_AGENT, UDA_LOCAL, UDA_REAL);
}

static inline void uda_davinci_remote_real_entity_type_pack(struct uda_dev_type *type)
{
    uda_dev_type_pack(type, UDA_DAVINCI, UDA_ENTITY, UDA_REMOTE, UDA_REAL);
}

static inline void uda_dev_para_pack(struct uda_dev_para *para,
    u32 udevid, u32 remote_udevid, u32 chip_type, struct device *dev)
{
    para->udevid = udevid;
    para->remote_udevid = remote_udevid;
    para->chip_type = chip_type;
    para->dev = dev;
    para->pf_flag = 1;   /* default pf dev */
}

static inline void uda_mia_dev_para_pack(struct uda_mia_dev_para *mia_para, u32 phy_devid, u32 sub_devid)
{
    mia_para->phy_devid = phy_devid;
    mia_para->sub_devid = sub_devid;
}

int uda_add_dev(struct uda_dev_type *type, struct uda_dev_para *para, u32 *udevid); /* uda_dev_prop must be real */
int uda_add_mia_dev(struct uda_dev_type *type, struct uda_dev_para *para,
    struct uda_mia_dev_para *mia_para, u32 *udevid);
int uda_remove_dev(struct uda_dev_type *type, u32 udevid);

enum uda_dev_ctrl_cmd {
    UDA_CTRL_SUSPEND,
    UDA_CTRL_RESUME,
    UDA_CTRL_TO_MIA, /* sia dev mode to mia mode */
    UDA_CTRL_TO_SIA, /* mia dev mode to sia mode */
    UDA_CTRL_MIA_CHANGE = 4, /* The computing power changes. */
    UDA_CTRL_REMOVE, /* sriov dev */
    UDA_CTRL_HOTRESET = 6,
    UDA_CTRL_HOTRESET_CANCEL,
    UDA_CTRL_SHUTDOWN,
    UDA_CTRL_MAX
};

int uda_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd);
int uda_agent_dev_ctrl(u32 udevid, enum uda_dev_ctrl_cmd cmd);

/* Method of the device notifier */

enum uda_priority { /* high Priority to be notified first */
    UDA_PRI0 = 0, /* hign */
    UDA_PRI1 = 1,
    UDA_PRI2 = 2,
    UDA_PRI3 = 3, /* low */
    UDA_PRI_MAX
};

enum uda_notified_action {
    UDA_INIT, /* phy dev, mia dev */
    UDA_UNINIT, /* phy dev, mia dev */
    UDA_SUSPEND, /* phy dev */
    UDA_RESUME, /* phy dev */
    UDA_TO_MIA, /* phy dev */
    UDA_TO_SIA, /* phy dev */
    UDA_MIA_CHANGE = 6, /* The computing power changes. mia dev */
    UDA_REMOVE, /* sriov mia dev */
    UDA_HOTRESET, /* phy dev, sriov mia dev */
    UDA_HOTRESET_CANCEL, /* phy dev, sriov mia dev */
    UDA_SHUTDOWN = 10, /* phy dev */
    UDA_OCCUPY, /* phy dev, mia dev. call when user occpy /dev/davinci* in docker */
    UDA_UNOCCUPY, /* phy dev, mia dev. call when docker exit */
    UDA_ACTION_MAX
};

typedef int (*uda_notify)(u32 udevid, enum uda_notified_action action);
int uda_notifier_register(const char *notifier, struct uda_dev_type *type, enum uda_priority pri, uda_notify func);
int uda_notifier_unregister(const char *notifier, struct uda_dev_type *type);

struct device *uda_get_device(u32 udevid);
struct device *uda_get_agent_device(u32 udevid);

/* Some modules do not separate the drivers of real devices and virtual devices.
   Therefore, simples methods are provided here. */
static inline int uda_real_virtual_notifier_register(const char *notifier, struct uda_dev_type *type,
    enum uda_priority pri, uda_notify func)
{
    int ret = uda_notifier_register(notifier, type, pri, func);
    if (ret == 0) {
        type->prop = UDA_VIRTUAL;
        ret = uda_notifier_register(notifier, type, pri, func);
        if (ret != 0) {
            type->prop = UDA_REAL;
            (void)uda_notifier_unregister(notifier, type);
        }
    }

    return ret;
}

static inline int uda_real_virtual_notifier_unregister(const char *notifier, struct uda_dev_type *type)
{
    int ret = uda_notifier_unregister(notifier, type);
    if (ret == 0) {
        type->prop = UDA_VIRTUAL;
        ret = uda_notifier_unregister(notifier, type);
    }

    return ret;
}

/* Method of the device probe or notifier */
bool uda_is_support_udev_mng(void); /* obp not surport, milan surport */
bool uda_is_phy_dev(u32 udevid); /* Check whether the device is a physical device, not mia device. */
bool uda_is_udevid_exist(u32 udevid);

int uda_dev_set_remote_udevid(u32 udevid, u32 remote_udevid);
int uda_dev_get_remote_udevid(u32 udevid, u32 *remote_udevid);

/* mia dev trans */
int uda_udevid_to_mia_devid(u32 udevid, struct uda_mia_dev_para *mia_para);
int uda_mia_devid_to_udevid(struct uda_mia_dev_para *mia_para, u32 *udevid);

int uda_devid_to_udevid(u32 devid, u32 *udevid);
int uda_devid_to_phy_devid(u32 devid, u32 *phy_devid, u32 *vfid);

/* recommend to use uda_devid_to_udevid instead of this,
    when outband hotreset, device offline, can only use this interface */
int uda_ns_node_devid_to_udevid(u32 devid, u32 *udevid);

bool uda_can_access_udevid(u32 udevid);
bool uda_proc_can_access_udevid(int tgid, u32 udevid);
bool uda_task_can_access_udevid_inherit(struct task_struct *task, u32 udevid);

/* detected dev num, it is not related to whether the device is online */
int uda_set_detected_phy_dev_num(u32 dev_num);
u32 uda_get_detected_phy_dev_num(void);

u32 uda_get_udev_max_num(void); /* System udeivce specifications */

u32 uda_get_dev_num(void); /* online phy dev num */
u32 uda_get_mia_dev_num(void); /* online mia dev num */

u32 uda_get_cur_ns_dev_num(void);
int uda_get_cur_ns_udevids(u32 udevids[], u32 num);

/* set/get udev ns id */
int uda_set_dev_ns_identify(u32 udevid, u64 identify); /* should call in notifier call ctx */
int uda_get_dev_ns_identify(u32 udevid, u64 *identify);
int uda_get_cur_ns_id(u32 *ns_id);

int uda_set_dev_share(u32 udevid, u8 share_flag);
int uda_get_dev_share(u32 udevid, u8 *share_flag);

/***************************** chip type * ********************************/
#define HISI_MINI_V1        0
#define HISI_CLOUD_V1       1
#define HISI_MINI_V2        2
#define HISI_CLOUD_V2       3
#define HISI_MINI_V3        4
#define HISI_MDC_V2         5
#define HISI_CHIP_NUM       6
#define HISI_CHIP_UNKNOWN   7

/* only surport phy device which udevid < 100 */
int uda_set_chip_type(u32 udevid, u32 chip_type);
u32 uda_get_chip_type(u32 udevid);

#endif


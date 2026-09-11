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

#ifndef UDA_CMD_H
#define UDA_CMD_H

#define UDA_CHAR_DEV_NAME "uda"

#define UDA_HW_DAVINCI 0
#define UDA_HW_KUNPENG 1

struct uda_user_info {
    unsigned int admin_flag; /* 1: admin right, can manage mia dev; 0: run in docker, no admin right */
    unsigned int local_flag; /* 1: local device; 0: not local */
    unsigned int max_dev_num; /* max logic dev num, host is 100, device is 32 */
    unsigned int max_udev_num; /* admin right valid, max unique dev numhost is 1124, device is 64 */
    unsigned int support_udev_mng; /* obp not surport, milan surport */
};

struct uda_setup_table {
    unsigned int dev_num; /* input */
};

struct uda_logic_dev {
    unsigned char valid;
    unsigned char hw_type;
    unsigned short phy_devid;
    unsigned short devid;
    unsigned short udevid;
};

struct uda_dev_list {
    unsigned int start_devid; /* input */
    unsigned int end_devid; /* input */
    struct uda_logic_dev *logic_dev;
};

struct uda_devid_trans {
    unsigned int raw_devid; /* input */
    unsigned int trans_devid; /* output */
};

#define UDA_GET_USER_INFO           _IOR('U', 0, struct uda_user_info)
#define UDA_SETUP_DEV_TABLE         _IOW('U', 1, struct uda_setup_table)
#define UDA_GET_DEV_LIST            _IOWR('U', 2, struct uda_dev_list)
#define UDA_UDEVID_TO_DEVID         _IOWR('U', 3, struct uda_devid_trans) /* unique devid to logic devid */
#define UDA_DEVID_TO_UDEVID         _IOWR('U', 4, struct uda_devid_trans) /* logic devid to unique devid */
#define UDA_LUDEVID_TO_RUDEVID      _IOWR('U', 5, struct uda_devid_trans) /* local udevid to remote udevid */
#define UDA_RUDEVID_TO_LUDEVID      _IOWR('U', 6, struct uda_devid_trans) /* remote udevid to local udevid */

#define UDA_MAX_CMD        8
#endif


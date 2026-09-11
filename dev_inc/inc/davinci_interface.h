/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2020-12-6
 * File Name     : davinci_interface.h
 * Version       : Initial Draft
 * Author        :
 * Created       : 2020-05-06
 * Last Modified :
 * Description   : head file

 *  History       :
 * 1.Date        : 2020-12-06
 *    Author      :
 *    Modification: Created file
 */

#ifndef __DAVINCI_INTERFACE_H__
#define __DAVINCI_INTERFACE_H__

#include <linux/fs.h>

#define DAVINIC_MODULE_NAME_MAX              256

#define DAVINCI_INTF_DEV_NAME               "davinci_manager"
#define DAVINCI_INTF_DEV_PATH               "/dev/davinci_manager"
#define DAVINCI_INTF_NPU_DEV_CUST_PATH      "/dev/npu_device_cust"

#define DAVINCI_INTF_MODULE_DEVMNG          "DEVMNG"
#define DAVINCI_INTF_MODULE_TSDRV           "TSDRV"
#define DAVINCI_INTF_MODULE_TRS_SHR_ID      "TRS_SHR_ID"
#define DAVINCI_INTF_MODULE_CDQM            "CDQM"


/* Use 'Z' as magic number */
#define DAVINCI_INTF_IOC_MAGIC 'Z'
#define DAVINCI_INTF_IOCTL_OPEN        _IO(DAVINCI_INTF_IOC_MAGIC, 0)
#define DAVINCI_INTF_IOCTL_CLOSE       _IO(DAVINCI_INTF_IOC_MAGIC, 1)
#define DAVINCI_INTF_IOCTL_GET_MODULE_STATUS  _IO(DAVINCI_INTF_IOC_MAGIC, 2)

#define DAVINCI_INTF_IOCTL_CMD_MAX_NR 3

struct davinci_intf_open_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};

struct davinci_intf_close_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};

struct davinci_intf_check_no_use_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    unsigned int status; /* 0--FALSE ,1 --TRUE means not used */
};

enum {
    DAVINCI_STATUS_TYPE_PROCESS,
    DAVINCI_STATUS_TYPE_DEVICE
};

/* device status */
#define DAVINCI_INTF_DEVICE_CLEAR_ALL_STATUS 0xffffffff
#define DAVINCI_INTF_DEVICE_CLEAR_STATUS (1<<31)
#define DAVINCI_INTF_DEVICE_STATUS_TS_DOWN (1<<0)
#define DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST (1<<1)
#define DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL (1<<2)

#define DAVINCI_INTF_PROCESS_CLEAR_ALL_STATUS 0xffffffff
#define DAVINCI_INTF_PROCESS_CLEAR_STATUS (1<<31)
#define DAVINCI_INTF_PROCESS_SVM_HUNG (1<<0)
#define DAVINCI_INTF_PROCESS_HDC_CONNECT_CLOSE (1<<1)

#if (defined TSDRV_UT) || (defined DMS_UT)
static inline const char *davinci_intf_get_dev_path(void)
{
    return DAVINCI_INTF_DEV_PATH;
}
#endif

#endif

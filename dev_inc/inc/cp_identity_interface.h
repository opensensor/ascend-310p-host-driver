/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2020-12-6
 * File Name     : cp_identity_interface.h
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
#ifndef CP_IDENTITY_INTERFACE_H__
#define CP_IDENTITY_INTERFACE_H__
#include <linux/types.h>
int devdrv_get_dev_process(pid_t devpid);
void devdrv_put_dev_process(pid_t devpid);
#endif

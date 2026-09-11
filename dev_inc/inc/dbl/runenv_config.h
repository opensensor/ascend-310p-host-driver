/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Create: 2022-6-23
 */
#ifndef DBL_RUNENV_CONFIG_H
#define DBL_RUNENV_CONFIG_H
#include <linux/types.h>

// for rc_ep_mode
#define DBL_RC_MODE     0
#define DBL_EP_MODE     1

// for deployment_mode
#define DBL_HOST_DEPLOYMENT    0
#define DBL_DEVICE_DEPLOYMENT  1

/**
 * @driver base layer interface
 * @description: Set device RC/EP mode info.
 * @attention  : For device, called by PCIE module_init only.
 * @param [in] : mode(u32), RC/EP mode.
 * @param [out]: NULL
 * @return     : 0(success) or -EINVAL(invalid mode error)
 */
int dbl_set_rc_ep_mode(u32 mode);
/**
 * @driver base layer interface
 * @description: Get device RC/EP mode info.
 * @attention  : For device.
 * @param [in] : NULL
 * @param [out]: NULL
 * @return     : DBL_RC_MODE or DBL_EP_MODE
 */
u32 dbl_get_rc_ep_mode(void);

/**
 * @driver base layer interface
 * @description: Get product deployment mode (host/device).
 * @attention  : For host & device.
 * @param [in] : NULL
 * @param [out]: NULL
 * @return     : DBL_HOST_DEPLOYMENT or DBL_DEVICE_DEPLOYMENT
 */
u32 dbl_get_deployment_mode(void);

#define DBL_IN_PHYSICAL_MACH  0xaaa
#define DBL_IN_NORMAL_DOCKER  0xbbb
#define DBL_IN_ADMIN_DOCKER   0xccc
#define DBL_IN_VIRTUAL_MACH   0xddd
#define DBL_IN_UNKNOWN_MACH   0xfff

/**
* @driver base layer interface
* @description: Get the run_env of the current process
* @attention  : NULL
* @param [in] : NULL
* @param [out]: NULL
* @return     : DBL_IN_PHYSICAL_MACH or DBL_IN_NORMAL_DOCKER or DBL_IN_ADMIN_DOCKER or DBL_IN_VIRTUAL_MACH
*/
int dbl_get_run_env(void);

/**
* @driver base layer interface
* @description: Get the run_env of the current process
* @attention  : NULL
* @param [in] : NULL
* @param [out]: NULL
* @return     : True(in normal docker, not include admin docker) or False(not in normal docker)
*/
bool run_in_normal_docker(void);

/**
* @driver base layer interface
* @description: Get the run_env of the current process
* @attention  : NULL
* @param [in] : NULL
* @param [out]: NULL
* @return     : True(in docker, include normal docker and admin docker) or False(not in docker)
*/
bool run_in_docker(void);

/**
* @driver base layer interface
* @description: Get the run_env of the current process
* @attention  : NULL
* @param [in] : NULL
* @param [out]: NULL
* @return     : return if current have admin permission(host or admin docker will return true)
*/
bool dbl_current_is_admin(void);

/**
* @driver base layer interface
* @description: Judge whether the current process is running in virtual machine
* @attention  : NULL
* @param [in] : NULL
* @param [out]: NULL
* @return     : True (in virtual machine) or False(not in virtual machine)
*/
bool run_in_virtual_mach(void);

#define DBL_CUST_OP_ENHANCE_DISABLE 0
#define DBL_CUST_OP_ENHANCE_ENABLE  1

/**
* @driver base layer interface
* @description: Get the custom op enhance mode
* @attention  : NULL
* @param [in] : udevid(u32)
* @param [out]: mode(u32), enable/disable
* @return     : 0(success) or other (failed)
*/
int dbl_get_cust_op_enhance_mode(unsigned int udevid,  unsigned int *mode);

/**
* @driver base layer interface
* @description: Set the custom op enhance mode
* @attention  : NULL
* @param [in] : udevid(u32)
* @param [in] : mode(u32), enable/disable
* @param [out]: NULL
* @return     : 0(success) or other (failed)
*/
int dbl_set_cust_op_enhance_mode(unsigned int udevid, unsigned int mode);

#endif /* DBL_RUNENV_CONFIG_H */


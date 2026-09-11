/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2020-12-6
 * File Name     : user_cfg_interface.h
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

#ifndef __USER_CFG_INTERFACE_H__
#define __USER_CFG_INTERFACE_H__

#define PKCS_SIGN_TYPE_OFF  1
#define PKCS_SIGN_TYPE_ON   0

/* same as struct agentdrv_cpu_info */
typedef struct dev_cpu_nums_cfg_stru {
    unsigned int ccpu_num;
    unsigned int ccpu_os_sched;
    unsigned int dcpu_num;
    unsigned int dcpu_os_sched;
    unsigned int aicpu_num;
    unsigned int aicpu_os_sched;
    unsigned int tscpu_num;
    unsigned int tscpu_os_sched;
} dev_cpu_nums_cfg_t;

int devdrv_config_set_pss_cfg(unsigned int dev_id, int sign);
int devdrv_config_get_pss_cfg(unsigned int dev_id, int *sign);


int dev_user_cfg_get_cpu_number(unsigned int dev_id, dev_cpu_nums_cfg_t *cpu_nums_cfg);
int devdrv_config_get_mac_info(unsigned int dev_id, unsigned char *buf, unsigned int buf_size, unsigned int *info_size);
#endif

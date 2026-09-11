/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-05-24
 */

#ifndef DBL_CHIP_CONFIGT_H
#define DBL_CHIP_CONFIGT_H

#include <linux/types.h>

#define DBL_NUMA_ID_MAX_NUM    64

#define DBL_MEMTYPE_ALL        0
#define DBL_MEMTYPE_DDR        1
#define DBL_MEMTYPE_HBM        2
#define DBL_MEMTYPE_NUM        3

#define DBL_SUB_MEMTYPE_ALL    0
#define DBL_SUB_MEMTYPE_P2P    1
#define DBL_SUB_MEMTYPE_TS     2
#define DBL_SUB_MEMTYPE_AI     3
#define DBL_SUB_MEMTYPE_CTRL   4
#define DBL_SUB_MEMTYPE_SYS    5
#define DBL_SUB_MEMTYPE_NUM    6

#define DBL_NID_MEMCTRL_SHARED_TYPE       0
#define DBL_NID_MEMCTRL_UNSHARED_TYPE     1

typedef struct nid_size {
    u64 total_size;
    u64 free_size;
} nid_size_t;

typedef struct nid_info {
    int nid;
    nid_size_t size;
    u32 nid_mask;
} nid_info_t;

/**
* @driver base layer interface
* @description: Add the available NID information of the sub_memory type of the device.
* @attention  : For device. The repeated NIDs will not be added to the device.
* @param [in] : devid(u32), memtype(u32), sub_memtype(u32), nids_info[], num(int)
* @param [out]: NULL
* @return     : 0(success) or -EINVAL, -ENOMEM, -ENOENT(invalid param[in], set nid failed)
*/
int dbl_nid_add_dev(u32 devid, u32 memtype, u32 sub_memtype, nid_info_t nids_info[], int num);

/**
* @driver base layer interface
* @description: Delete the sub_memory type that has been set for the device.
* @attention  : For device.
*               Device will be delete if memtype is set to MEMTYPE_NUM and sub_memtype is set to SUB_MEMTYPE_NUM
* @param [in] : devid(u32), memtype(u32), sub_memtype(u32)
* @param [out]: NULL
* @return     : 0(success) or -EINVAL(invalid param[in], Mem nid is not set)
*/
int dbl_nid_del_dev(u32 devid, u32 memtype, u32 sub_memtype);

/**
* @driver base layer interface
* @description: Query the number of available NIDs of the sub_memory type of device.
* @attention  : For device. If sub_memtype is set to SUB_MEMTYPE_NUM, all sub_memory types will be queried.
* @param [in] : devid(u32), memtype(u32), sub_memtype(u32)
* @param [out]: NULL
* @return     : nid_num(success) or -EINVAL(invalid param[in])
*/
int dbl_nid_get_nid_num(u32 devid, u32 memtype, u32 sub_memtype);

/**
* @driver base layer interface
* @description: Query the of available NIDs of the sub_memory type of device.
* @attention  : For device. If sub_memtype is set to SUB_MEMTYPE_NUM, all sub_memory types will be queried.
* @param [in] : devid(u32), memtype(u32), sub_memtype(u32), nids(int *), num(int)
* @param [out]: nids
* @return     : count(success) or -EINVAL(invalid param[in])
*/
int dbl_nid_get_nid(u32 devid, u32 memtype, u32 sub_memtype, int nids[], int num);

/**
* @driver base layer interface
* @description: Query the number of sub_memory types that have been set on the device.
* @attention  : For device.
* @param [in] : devid(u32), memtype(u32)
* @param [out]: NULL
* @return     : mem_num(success) or -EINVAL(invalid param[in])
*/
int dbl_nid_get_sub_memtype_num(u32 devid, u32 memtype);

/**
* @driver base layer interface
* @description: Query the sub_memory type that has been set for the device.
* @attention  : For device.
* @param [in] : devid(u32), memtype(u32)
* @param [out]: sub_memtype(u32 *)
* @return     : 0(success) or -EINVAL(invalid param[in])
*/
int dbl_nid_get_sub_memtype(u32 devid, u32 memtype, u32 sub_memtype[], int num);

/**
* @driver base layer interface
* @description: Query the memory information of a device Node.
* @attention  : For device.
* @param [in] : devid(u32), nid(int)
* @param [out]: size
* @return     : 0(success, information will be written to size) or -EINVAL(invalid param[in])
*/
int dbl_nid_get_nid_size(u32 devid, int nid, nid_size_t *size);

/**
* @driver base layer interface
* @description: Occupy a certain size of the device's NID memory.
* @attention  : For device.
* @param [in] : devid(u32), nid(int), memsize(u64)
* @param [out]: NULL
* @return     : NULL
*/
int dbl_nid_mem_occupy(u32 devid, int nid, u64 memsize);

/**
* @driver base layer interface
* @description: Return a certain size of the device's NID memory.
* @attention  : For device.
* @param [in] : devid(u32), nid(int), memsize(u64)
* @param [out]: NULL
* @return     : NULL
*/
void dbl_nid_mem_return(u32 devid, int nid, u64 memsize);

/**
* @driver base layer interface
* @description: set numa id.
* @attention  : For device.
* @param [in] : chip_type(u32), dev_num(u32), devid(u32)
* @param [out]: NULL
* @return     : NULL
*/
int dbl_nid_auto_set_nid(u32 chip_type, u32 dev_num, u32 devid);

/**
* @driver base layer interface
* @description: get one ts numa id.
* @attention  : For device.
* @param [in] : devid(u32)
* @return     : nid(int)
*/
int dbl_get_ts_default_nid(u32 devid);

/**
* @driver base layer interface
* @description: get ai numa id.
* @attention  : For device.
* @param [in] : devid(u32), nids(int *), num(int)
* @param [out]: nids
* @return     : count(success) or -EINVAL(invalid param[in])
*/
int dbl_get_ai_nid(u32 devid, int nids[], int num);

/**
* @driver base layer interface
* @description: get ctrl numa id.
* @attention  : For device.
* @param [in] : devid(u32), nids(int *), num(int)
* @param [out]: nids
* @return     : count(success) or -EINVAL(invalid param[in])
*/
int dbl_get_ctrl_nid(u32 devid, int nids[], int num);

/**
* @driver base layer interface
* @description: get ts numa id.
* @attention  : For device.
* @param [in] : devid(u32), nids(int *), num(int)
* @param [out]: nids
* @return     : count(success) or -EINVAL(invalid param[in])
*/
int dbl_get_ts_nid(u32 devid, int nids[], int num);

/**
* @driver base layer interface
* @description: get memctrl type.
* @attention  : For device.
* @param [in] : NULL
* @param [out]: NULL
* @return     : memctrl type
*/
int dbl_nid_get_memctrl_type(void);

#endif /* DBL_CHIP_CONFIGT_H */

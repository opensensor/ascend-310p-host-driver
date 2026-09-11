/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2021-7-20
 * File Name     : dms_interface.h
 * Version       : Initial Draft
 * Author        :
 * Created       : 2021-7-20
 * Last Modified :
 * Description   :
 *  History       :
 * 1.Date        : 2021-7-20
 *    Author      :
 *    Modification: Created file
 */
#ifndef __DMS_INTERFACE_H__
#define __DMS_INTERFACE_H__

/************************************* sensor interface ********************************************************/

/* 统计型传感器的产生门限、恢复门限类型 */
#define DMS_SENSOR_THRES_TYPE_PERIOD             0 /* 门限类型:周期统计型 */
#define DMS_SENSOR_THRES_TYPE_CONTINUED          1 /* 门限类型:连续检测型 */
#define DMS_SENSOR_OCCUR_THRES_TYPE_PERIOD       DMS_SENSOR_THRES_TYPE_PERIOD    /* 产生门限类型:周期统计型 */
#define DMS_SENSOR_OCCUR_THRES_TYPE_CONTINUED    DMS_SENSOR_THRES_TYPE_CONTINUED /* 产生门限类型:连续检测型 */
#define DMS_SENSOR_RESUME_THRES_TYPE_PERIOD      DMS_SENSOR_THRES_TYPE_PERIOD    /* 恢复门限类型:周期统计型 */
#define DMS_SENSOR_RESUME_THRES_TYPE_CONTINUED   DMS_SENSOR_THRES_TYPE_CONTINUED /* 恢复门限类型:连续检测型 */

/* 传感器分类定义 */
#define DMS_STATICSTIC_SENSOR_CLASS         0x70 /* 统计型传感器 */
#define DMS_GENERAL_THRESHOLD_SENSOR_CLASS  0x01 /* 一般门限型传感器 */
#define DMS_DISCRETE_SENSOR_CLASS           0x6f /* 离散型传感器 */
#define DMS_PER_SENSOR_CLASS                0x71 /* 性能统计类型的传感器 */

/* 传感器使能标志 */
#define DMS_SENSOR_ENABLE_FALG               1 /* 使能 */
#define DMS_SENSOR_DISABLE_FALG              0 /* 禁止 */

#define DMS_SENSOR_CHECK_TIMER_LEN           100                         /* 传感器检测定时器时长，100毫秒 */
#define DMS_SENSOR_CHECK_INTERVAL_TIME      DMS_SENSOR_CHECK_TIMER_LEN     /* 传感器检测定时器时长 */

/* 附加数据最大长度 */
#define DMS_MAX_EVENT_NAME_LENGTH 256
#define DMS_MAX_EVENT_DATA_LENGTH 32

/* 传感器状态 */
#define DMS_SENSOR_STATUS_GOOD               0U /* 正常 */
#define DMS_SENSOR_STATUS_FAULT              1U /* 故障 */
#define DMS_SENSOR_STATUS_INVALID            2U /* 无效 */
#define DMS_SENSOR_STATUS_LOW_MINOR_FAULT    3U /* 下轻微故障 */
#define DMS_SENSOR_STATUS_LOW_MAJOR_FAULT    4U /* 下严重故障 */
#define DMS_SENSOR_STATUS_LOW_CRITICAL_FAULT 5U /* 下致命故障 */
#define DMS_SENSOR_STATUS_UP_MINOR_FAULT     6U /* 上轻微故障 */
#define DMS_SENSOR_STATUS_UP_MAJOR_FAULT     7U /* 上严重故障 */
#define DMS_SENSOR_STATUS_UP_CRITICAL_FAULT  8U /* 上致命故障 */
#define DMS_SENSOR_STATUS_CFG_FAULT          0xf9U /* 配置失败 */

/*
事件状态
*/
#define DMS_ES_UNSPECIFIED (unsigned int)0x0000

/*
事件种类=DMS_EC_THRESHOLD (门限类)
时的事件状态定义
*/
#define DMS_ES_LOWER_MINOR 0x0001U
#define DMS_ES_LOWER_MAJOR 0x0002U
#define DMS_ES_LOWER_CRIT  0x0004U
#define DMS_ES_UPPER_MINOR 0x0008U
#define DMS_ES_UPPER_MAJOR 0x0010U
#define DMS_ES_UPPER_CRIT  0x0020U

/* 事件类型:消失、产生 */
enum {
    DMS_EVENT_TYPE_RESUME   = 0, /* 消失 */
    DMS_EVENT_TYPE_OCCUR    = 1, /* 产生 */
    DMS_EVENT_TYPE_ONE_TIME = 2, /* 一次性 */
    DMS_EVENT_TYPE_MAX
};

#define TOPOLOGY_HCCS       0
#define TOPOLOGY_PIX        1
#define TOPOLOGY_PIB        2
#define TOPOLOGY_PHB        3
#define TOPOLOGY_SYS        4
#define TOPOLOGY_SIO        5
#define TOPOLOGY_HCCS_SW    6
/***************************************************************************************************************/

int dms_get_devid_from_data(void *data);
int dms_get_dev_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type);

#endif

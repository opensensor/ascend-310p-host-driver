/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
* Create: 2019-10-15
*/

#ifndef __DMS_DEFINE_H__
#define __DMS_DEFINE_H__

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/sched/clock.h>
#ifdef AOS_LLVM_BUILD
#include <linux/atomic.h>
#include "devdrv_user_common.h"
#endif

#include "ka_dfx_pub.h"

#include "ascend_hal_error.h"
#include "dms_sensor.h"
#include "dms_event.h"
#include "dms_node_type.h"
#include "drv_log.h"
#ifndef CFG_SOC_PLATFORM_MDC_V51_LITE
#include "drv_share_log.h"
#endif
#include "devdrv_manager_comm.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC                     static
#endif

#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_SRIOV)
#define DEVICE_NUM_MAX 1124
#else
#define DEVICE_NUM_MAX  64
#endif
#define DEVICE_NODE_NUM_MAX  4
/* The maximum number of information that the sub-device can query is: infoType 0 ~ dms_MAX_INFO_TYPE_NUM */
#define DMS_MAX_INFO_TYPE_NUM   20
/* Maximum character length of device name */
#define DMS_MAX_DEV_NAME_LEN    16

/* Maximum event number */
#define DMS_MAX_EVENT_NUM 256

#ifdef AOS_LLVM_BUILD
#define VMNG_VDEV_MAX_PER_PDEV 17
#define DEVMNG_SHM_INFO_EVENT_CODE_LEN 128

#endif

/* Data attribute definition */
struct dms_dev_data_attr {
    /* Data type */
    int info_type;
    /* Data life cycle 0: always valid 1: valid once */
    int info_life;
    /* Data reading permission, which permission or higher can read this data */
    int read_permission;
    /* Data write permission, which permission can read this data */
    int write_permission;
};
struct dms_node_operations;
/* Device management node structure */
struct dms_node {
    struct list_head list;
    /* Device node (global edition) */
    int node_type;
    int node_id;   /* global node index in chip */
    int pid;
    /* Device name */
    char node_name[DMS_MAX_DEV_NAME_LEN];
    /* Each bit represents a capability */
    unsigned long long capacity;
    /* Device permissions */
    int permission;
    /* The ID of the parent device to which the child device belongs, corresponding to the device ID of the chip */
    int owner_devid;
    /* The parent node of the device */
    struct dms_node *owner_device;
    /* node index in owner device, valid when @owner_device is not null */
    int inner_node_id;

    struct dms_node *sub_node[DEVICE_NODE_NUM_MAX];
    int state;
    /* Control and operation interface for equipment */
    struct dms_node_operations *ops;
};

struct dms_node_operations {
    int (*init)(struct dms_node *device);
    void (*uninit)(struct dms_node *device);
    int (*get_info_list)(struct dms_node *device, struct dms_dev_data_attr *InfoList);
    /* Query setting status */
    int (*get_state)(struct dms_node *device, unsigned int *state);
    int (*get_init_state)(struct dms_node *device, unsigned int *init_state);
    /* Query capability, each bit represents a capability */
    int (*get_capacity)(struct dms_node *device, unsigned long long *Capacity);
    /* Set the power state of the device to support the power-off, power-on, sleep, and reset of the device */
    int (*set_power_state)(struct dms_node *device, DSMI_POWER_STATE power_state);
    /* Scan the status of each device and sensor */
    int (*scan)(struct dms_node *device, int *state);
    /* Run test to diagnose whether the object is monitored */
    int (*fault_diag)(struct dms_node *device, int *state);
    /* Alarm notification sub-devices such as RAS */
    int (*event_notify)(struct dms_node *device, int event);
    /* Query the status of two device nodes */
    int (*get_link_state)(struct dms_node *device1,
        struct dms_node *device2, unsigned int *state);
    /* Query the status of two device nodes */
    int (*set_link_state)(struct dms_node *device1,
        struct dms_node *device2, unsigned int state);
    int (*fault_inject)(dms_fault_inject_t info);
    int (*get_fault_inject_info)(struct dms_node *node, dms_fault_inject_t *buf,
        unsigned int buf_cnt, unsigned int *real_cnt);

    int (*enable_device)(struct dms_node *device);
    int (*disable_device)(struct dms_node *device);
    int (*set_power_info)(struct dms_node *device, void *buf, unsigned int size);
    int (*get_power_info)(struct dms_node *device, void *buf, unsigned int size);
};

/* Interconnected device node item */
struct dms_dev_link_item {
    struct dms_node dev1;
    struct dms_node dev2;
    /* The connection status of the two devices */
    int link_state;
    /* Type of connection between devices 0: HCCS, 1: PCIE, 2, USB, 3, I2C */
    int link_type;
};

struct dms_link_node {
    /* Device code majorDevID (high 2 bytes) + minorDevID (low 2 bytes) */
    int node_id;
    char major_dev_name[DMS_MAX_DEV_NAME_LEN];
    /* Device topology table, unique to interconnect nodes */
    struct dms_dev_link_item dev_topo_table;
};

/* Interconnect device node registration interface */
struct dms_link_node_operations {
    /* Scan the status of each device and sensor */
    int (*scan)(struct dms_node *device, int *state);
    /* Run test to diagnose whether the object is monitored */
    int (*fault_diag)(struct dms_node *device, int *state);
    /* Alarm notification sub-devices such as RAS */
    int (*event_notify)(struct dms_node *device, int event);
    /* Query the status of two device nodes */
    int (*get_link_state)(struct dms_node *device1,
    struct dms_node *device2, unsigned int *state);
    /* Query the status of two device nodes */
    int (*set_link_state)(struct dms_node *device1,
        struct dms_node *device2, unsigned int state);
};

struct dms_event_ctrl {
    struct dms_converge_event_list event_list;
    struct dms_sensor_reported_list reported_list;
    struct dms_event_dfx_table dfx_table;
};

/* The data view of the device, data snapshot, used to store the data of the sub-device of the device on host side */
struct dms_dev_data_view {
    /* Device handle */
    struct dms_node *device;
    /* The number of data in the view in the sub-device */
    int dev_data_num;
    struct {
        struct dms_dev_data_attr dataItem;
        void *data_value;
        unsigned int data_size;
    } *dev_data_items;
};

struct dms_dev_ctrl_block {
    u32 state;
    void *dev_info;
    atomic_t work_count;
    int sub_node_num;
    struct dms_event_ctrl dev_event_cb;
    struct mutex node_lock;
    /* List of registered devices, manage all sub-devices on this device on the system */
    struct list_head dev_node_list;
    /* Sensor control block of each chip */
    struct dms_dev_sensor_cb dev_sensor_cb;
};

struct dms_system_ctrl_block {
    int dev_id;
    /* Sensor scanning task */
    struct task_struct *sensor_scan_task;
    /* Scan task synchronization signal */
    wait_queue_head_t sensor_scan_wait;
    int sensor_scan_task_state;
    /* This flag records whehter system suspend, if true, the sensor scan task will not run */
    int sensor_scan_suspend_flag;

    struct timer_list dms_sensor_check_timer;
    /*
     * Device data group, initial allocation, call for 1 device (device id = 0x1000) on the host side,
     * if device, AMP is 1 device, 910 SMP is 4 devices
     */
    struct dms_dev_ctrl_block base_cb;
    struct dms_dev_ctrl_block dev_cb_table[DEVICE_NUM_MAX];
};

struct state_item {
    uint32_t node_type;
    uint32_t node_id;
    struct dms_node *node;
};

struct dms_state_table {
    uint32_t num;
    struct state_item *item;
};

struct dms_dev_ctrl_block *dms_get_dev_cb(int dev_id);
bool dms_is_devid_valid(int dev_id);
struct dms_state_table *dms_get_state_table(void);

#define MODULE_DMS "dms_module"
#ifdef UT_VCAST
#define dms_err(fmt, ...) drv_err(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_warn(fmt, ...) drv_warn(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_info(fmt, ...) drv_info(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_event(fmt, ...) drv_event(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_debug(fmt, ...) drv_pr_debug(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_err_ratelimited(fmt, ...) drv_err_ratelimited(MODULE_DMS, fmt, ##__VA_ARGS__)
#else
#define dms_err(fmt, ...) do { \
    drv_err(MODULE_DMS, "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
    share_log_err(DEVMNG_SHARE_LOG_START, fmt, ##__VA_ARGS__); \
} while (0)
#define dms_warn(fmt, ...) drv_warn(MODULE_DMS, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dms_info(fmt, ...) drv_info(MODULE_DMS, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dms_event(fmt, ...) drv_event(MODULE_DMS, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dms_debug(fmt, ...) drv_pr_debug(MODULE_DMS, \
    "<%s:%d:%d> " fmt, current->comm, current->tgid, current->pid, ##__VA_ARGS__)
#define dms_err_ratelimited(fmt, ...) do { \
    drv_err_ratelimited(MODULE_DMS, "<%s:%d:%d> " fmt, \
        current->comm, current->tgid, current->pid, ##__VA_ARGS__); \
} while (0)
#endif
#ifdef CFG_FEATURE_HOST_LOG
#define ONE_TIME_EVENT 2
#define dms_fault_mng_event(assertion, fmt, ...) do {                       \
    if (((assertion) == ONE_TIME_EVENT)) {                                  \
        drv_event("fault_manager", fmt, ##__VA_ARGS__);                     \
    } else {                                                                \
        drv_err("fault_manager", fmt, ##__VA_ARGS__);                       \
    }                                                                       \
} while (0)
#else
#define dms_fault_mng_event(assertion, fmt, ...) do {                       \
    u64 ts = local_clock();                                                 \
    u64 rem_nsec = do_div(ts, 1000000000);                                  \
    drv_slog_event("fault_manager", "[%5lu.%06lu] " fmt, ts, rem_nsec / 1000, ##__VA_ARGS__);   \
} while (0)
#endif
#define dms_fmng_event(args...) (void)printk(KA_KERN_NOTICE "[fault_manager] " args)
#define dms_fmng_err(args...) (void)printk(KA_KERN_ERR "[fault_manager] " args)

#define dms_ex_notsupport_err(ret, fmt, ...) do {                      \
    if (((ret) != (int)DRV_ERROR_NOT_SUPPORT) && ((ret) != -EOPNOTSUPP)) {  \
        dms_err(fmt, ##__VA_ARGS__);                                   \
    }                                                                  \
} while (0)

extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
extern int strcpy_s(char *strDest, size_t destMax, const char *strSrc);
extern int strcat_s(char *strDest, size_t destMax, const char *strSrc);
extern int strncat_s(char *strDest, size_t destMax, const char *strSrc, size_t count);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);

typedef struct dms_sensor_event {
    /* The index of the sensor, the current coding rule is SensorType + SensorInstance */
    unsigned short                   sensor_num;
    char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
    /* Sensor type */
    unsigned char                    sensor_type;
    /* 0-resume, 1-occur, 2-one time */
    unsigned char                    assertion;
    /* Current state */
    unsigned char                    event_state;
    unsigned char                    param_len;
    char                             param_buffer[DMS_MAX_EVENT_DATA_LENGTH];
    unsigned char                    event_info[DMS_MAX_EVENT_INFO_LENGTH];
} DMS_SENSOR_EVENT_T;

typedef union {
    DMS_SENSOR_EVENT_T             sensor_event;
} DMS_EVENT_UNION_T;

typedef enum {
    DMS_ET_SENSOR,
    DMS_ET_OEM,
} DMS_EVENT_TYPE_T;

typedef enum {
    DMS_EVENT_OK = 0,
    DMS_EVENT_MINOR = 1,
    DMS_EVENT_MAJOR = 2,
    DMS_EVENT_CRITICAL = 3,
    DMS_EVENT_MAX
} DMS_SEVERITY_T;

typedef enum dsmi_dev_init_state {
    DMS_DEV_INIT_NO_ERROR = 0x0,
    DMS_DEV_INIT_ABNORMAL = 0x1,
    DMS_DEV_INIT_NOT_REGISTER = 0xf,
} DMS_INIT_STATE_T;

typedef enum {
    DMS_RC_MODE,
    DMS_EP_MODE
} DMS_MODE_T;

struct dms_event_obj {
    unsigned short              deviceid;    /* host的device id */
    unsigned int node_type;
    unsigned int node_id;
    unsigned int sub_node_type;
    unsigned int sub_node_id;
    DMS_EVENT_TYPE_T            event_type;
    unsigned long long          time_stamp; /*  event time  */
    DMS_SEVERITY_T              severity;
    DMS_EVENT_UNION_T           event;
    /* Alarm serial number */
    unsigned int alarm_serial_num;
    int pid;
};

#define DMS_FAULT_CONVERGE_CONFIG(sub_id, mod_id, sec_type, code, s_type, err_type, str) \
{ \
        .subsys_id = (sub_id), \
        .module_id = (mod_id), \
        .section_type = (sec_type), \
        .ras_code.err_status = (code), \
        .describe = (str), \
        .sensor_type = (s_type), \
        .error_type = (err_type), \
    }, \

/* ras first converge table item */
struct ras_fault_converge_item {
    unsigned char subsys_id;      /*  subsys:hbm-0x48  */
    unsigned char module_id;      /*  hbmc/ddrc  */
    unsigned int section_type;    /*  MEM/OEM/PCIE/ARM ...  */
    union {
        unsigned int err_type; /*  section_type:MEM  */
        unsigned int int_status; /*  section_type:ARM  */
        unsigned long long err_status; /*  section_type:OEM  */
    } ras_code;
    unsigned char describe[DMS_MAX_EVENT_DATA_LENGTH]; /*  detail  */
    unsigned char sensor_type;    /*  ras sensor 0xC0  */
    unsigned char error_type;     /*  unified type definition  */
};
struct dms_system_ctrl_block *dms_get_sys_ctrl_cb(void);
int dms_check_device_id(int dev_id);

int dms_event_report(struct dms_event_obj *event_obj);

int dms_sensor_get_health_events(struct dms_dev_sensor_cb *dev_sensor_cb, struct dms_event_obj *event_buff,\
    unsigned int input_count, unsigned int *output_count);
DMS_MODE_T dms_get_rc_ep_mode(void);

unsigned int dms_fill_event_data(const struct dms_sensor_object_cb *psensor_obj_cb,
    DMS_EVENT_LIST_ITEM *event_item, unsigned char assertion, struct dms_event_obj *p_event_obj);

#endif

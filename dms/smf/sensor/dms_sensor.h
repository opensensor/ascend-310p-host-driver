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

#ifndef __DMS_SENSOR_H__
#define __DMS_SENSOR_H__

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include "ka_system_pub.h"

#include "dms_kernel_version_adapt.h"
#include "dms_interface.h"
#include "kernel_version_adapt.h"
#include "ascend_hal_error.h"
#ifndef AOS_LLVM_BUILD
#include "drv_type.h"
#endif
/* ******************************************Interface definition
 * start******************************************************* */
#define DMS_SENSOR_CMD_NAME "DMS_SENSOR"
typedef struct dms_fault_inject_info {
    unsigned int device_id;
    unsigned int node_type;
    unsigned int node_id;
    unsigned int sub_node_type;
    unsigned int sub_node_id;
    unsigned int fault_index;
    unsigned int event_id;
    unsigned int reserve1;
    unsigned int reserve2;
} dms_fault_inject_t;
/* Sensor description length */
#define DMS_SENSOR_DESCRIPT_LENGTH 20

/* Maximum number of events supported by the sensor */
#define DMS_MAX_SENSOR_EVENT_COUNT 16

/* Event parameter length */
#define DMS_EVENT_LENGTH 3


/* Sensor enable flag */
#define DMS_SENSOR_ENABLE_FALG 1
#define DMS_SENSOR_DISABLE_FALG 0

/* Sensor event report enable status */
#define DMS_SENSOR_EVENT_REPORT_ENABLE 1
#define DMS_SENSOR_EVENT_REPORT_DISABLE 0


/* Sensor detection processing flag */
#define DMS_SENSOR_PROC_ENABLE_FLAG 0
#define DMS_SENSOR_PROC_DISABLE_FLAG 1

/* Detection enable flag of sensor instance */
#define DMS_SENSOR_OBJECT_ENABLE_FLAG 1
#define DMS_SENSOR_OBJECT_DISABLE_FLAG 0


/* Sensor properties */
#define DMS_SENSOR_ATTRIB_THRES_SET_ENABLE 0x01 /* Sensor threshold can be set */
#define DMS_SENSOR_ATTRIB_THRES_NONE 0          /* No property settings */


/* Repeat mark of sensor information table */
#define DMS_SENSOR_TABLE_REPEAT 1
#define DMS_SENSOR_TABLE_NOT_REPEAT 0

#define DMS_MASK_16_BIT 16
/* General threshold type sensor */
struct dms_general_sensor {
    /* Sensor properties */
    unsigned int attribute;
    /* Supported threshold types, each BIT represents a threshold type, currently supports
    Six types, including:DMS_SENSOR_THRES_LOW_MINOR_SERIES,DMS_SENSOR_THRES_LOW_MAJOR_SERIES
    DMS_SENSOR_THRES_LOW_CRITICAL_SERIES,DMS_SENSOR_THRES_UP_MINOR_SERIES,
    DMS_SENSOR_THRES_UP_MAJOR_SERIES, DMS_SENSOR_THRES_UP_CRITICAL_SERIES */
    unsigned int thres_series;
    /* Lower critical threshold */
    int low_critical;
    /* Lower critical threshold */
    int low_major;
    /* Lower minor threshold */
    int low_minor;
    /* Upper critical threshold */
    int up_critical;
    /* Upper severe threshold */
    int up_major;
    /* Upper minor threshold */
    int up_minor;
    /* is a positive value, a positive hysteresis value, which means that when a certain type of failure (critical,
     * serious, minor) is restored, the difference between the sensor value and the lower threshold after restoration */
    int pos_thd_hysteresis;
    /* is a positive value and a negative hysteresis value, which means that when a certain type of upper (critical,
     * serious, minor) fault is restored, the upper threshold minus the difference of the sensor value after
     * restoration */
    int neg_thd_hysteresis;
    /* The maximum legal value of the threshold */
    int max_thres;
    /* The minimum legal value of the threshold */
    int min_thres;
};

/* Discrete sensor */
struct dms_discrete_sensor {
    /* Sensor properties */
    unsigned int attribute;
    /* Anti-shake frequency attribute */
    unsigned int debounce_time;
};

/* Statistical threshold sensor */
struct dms_statistic_sensor {
    /* Generate threshold type, 0-periodic statistical type, 1-continuous detection type */
    unsigned short occur_thres_type;
    /* Recovery threshold type, 0-periodic statistics type, 1-continuous detection type */
    unsigned short resume_thres_type;
    /* Maximum statistical period */
    unsigned int max_stat_time;
    /* Minimum statistical period */
    unsigned int min_stat_time;
    /* Generate threshold statistical period */
    unsigned int occur_stat_time;
    /* Recovery threshold statistical period */
    unsigned int resume_stat_time;
    /* Sensor properties */
    unsigned int attribute;
    /* Maximum generation threshold */
    unsigned int max_occur_thres;
    /* Minimum generation threshold */
    unsigned int min_occur_thres;
    /* Maximum recovery threshold */
    unsigned int max_resume_thres;
    /* Minimum recovery threshold */
    unsigned int min_resume_thres;
    /* Generation threshold */
    unsigned int occur_thres;
    /* Recovery threshold */
    unsigned int resume_thres;
};

union dms_sensor_union {
    /* Discrete sensor */
    struct dms_discrete_sensor discrete_sensor;
    /* General threshold type sensor */
    struct dms_general_sensor general_sensor;
    /* Statistical threshold sensor */
    struct dms_statistic_sensor statistic_sensor;
};

#define DMS_MAX_EVENT_INFO_LENGTH 4
struct dms_sensor_event_data_item {
    /* Event offset of the sensor */
    int current_value;
    /* Event additional data length */
    unsigned short data_size;
    /* Event description */
    unsigned char event_data[DMS_MAX_EVENT_DATA_LENGTH];
    /* Event additional data
       Now just support on discrete sensor,
       statistical sensor and general threshold type sensor, temporarily */
    unsigned char event_info[DMS_MAX_EVENT_INFO_LENGTH];
};
struct dms_sensor_event_data {
    unsigned char event_count;
    struct dms_sensor_event_data_item sensor_data[DMS_MAX_SENSOR_EVENT_COUNT];
};

enum {
    DMS_SERSOR_SCAN_PERIOD = 0,
    DMS_SERSOR_SCAN_NOTIFY = 1,
    DMS_SERSOR_SCAN_MODULE_MAX
};

/* Register the configuration of sensor input */
struct dms_sensor_object_cfg {
    /* sensor type */
    unsigned char sensor_type;
    /* Sensor name description */
    char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
    /* The sensor information table types include 3 types: threshold type, discrete type, and statistical type */
    unsigned short sensor_class;
    /* Sensor type combination */
    union dms_sensor_union sensor_class_cfg;
    /* Scan module, 0 is period, 1 is notify */
    unsigned int scan_module;
    /* Detection period, in milliseconds */
    unsigned int scan_interval;
    /* Detection processing flag, 0 processing, 1 not processing */
    unsigned int proc_flag;
    /* Enable flag, 0 is forbidden, 1 is for enable */
    unsigned int enable_flag;
    /* Detect function pointer */
    int (*pf_scan_func)(unsigned long long private_data, struct dms_sensor_event_data *pevent_data);
    /* Reserved function parameters */
    unsigned long long private_data;
    /* A flag indicating whether it is possible to report a fault event: 1-enable, 0-forbid */
    unsigned int assert_event_mask;
    unsigned int deassert_event_mask;
    int pid;
    /* Clear sensor events callback */
    int (*pf_clear_event_func)(unsigned long long private_data);
};


/* ******************************************Interface definition
 * end******************************************************* */

/* Overload time of one description */
#define DMS_SCAN_OUT_TIME_MAX 70 // ms

/* The sensor periodically detects whether the processing time is recorded or not */
#define DMS_SENSOR_CHECK_RECORD 1
#define DMS_SENSOR_CHECK_NOT_RECORD 0

/* The sensor information table has the correct format and illegal flag */
#define DMS_SENSOR_TABLE_VALID 1
#define DMS_SENSOR_TABLE_INVALID 0

/* Registration environment type */
#define DMS_SENSOR_ENV_KERNEL_SPACE 0
#define DMS_SENSOR_ENV_USER_SPACE 1

/* The state of the sensor state, has changed, has not changed */
#define DMS_SENSOR_STATUS_CHANGED 1
#define DMS_SENSOR_STATUS_NO_CHANGE 0

/* The maximum number of entries recorded by the sensor timing detection execution time */
#define DMS_MAX_TIME_RECORD_COUNT 20

/* The maximum number of  sensor num every node */
#define DMS_MAX_NODE_SENSOR_COUNT 64


/* The mask used to get the lowest bit value */
#define DMS_BITS_MASK 0x00000001

/* Add SYS_TIME structure definition */
typedef struct dms_sys_time {
    unsigned short year;  /* if set to OS time the scope is 1970 ~ 2038, or
                           the scope is 1970 ~ 2100 */
    unsigned char month;  /* scope is 1 - 12 */
    unsigned char date;   /* scope is 1 - 31 */
    unsigned char hour;   /* scope is 0 - 23 */
    unsigned char minute; /* scope is 0 - 59 */
    unsigned char second; /* scope is 0 - 59 */
    unsigned char week;   /* scope is 0 - 6  */
} SYS_TIME;

typedef struct tag_dms_event_list_item {
    /* Whether to use 0 = not used */
    unsigned char in_use : 1;
    /* Whether it is a newly generated event or inherits
    the old event 1 = Old event 0 = New event, used when comparing event lists */
    unsigned char is_report : 7;
    unsigned char event_data;
    unsigned int sensor_num;
    char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
    unsigned long long timestamp;
    /* The serial number generated by the alarm */
    unsigned int alarm_serial_num;
    struct tag_dms_event_list_item *p_next;
    /* Additional data for the event */
    unsigned short para_len;
    /* The length of the additional data of the event */
    unsigned char *event_paras;
    /* 事件的其他信息 */
    unsigned char event_info[DMS_MAX_EVENT_INFO_LENGTH];
    /* Continuous generation times */
    unsigned char continued_count;
} DMS_EVENT_LIST_ITEM;

union dms_sensor_class_cb {
    /* Discrete sensor */
    struct dms_discrete_sen_cb {
        /* For discrete sensors, this field indicates the last state of the sensor */
        unsigned int pre_status;
    } discrete_cb;
    /* General threshold type sensor */
    struct dms_general_sen_cb {
        unsigned int pre_status;
    } general_cb;
    /* Statistical threshold sensor */
    struct dms_statistic_sen_cb {
        /* It is dedicated to statistical sensors. The parameters used for statistical sensors to report events
        updated by the alarm processing module have a fixed length of 20 bytes, and the effective data length is
        determined by the length of the event parameters above. The first part of the parameter stores the key field
        parameters.
         */
        unsigned char stat_event_paras[DMS_MAX_EVENT_DATA_LENGTH];
        /* The statistics sensor is dedicated to indicate the current cycle count,
        which is less than the statistics cycle. After the sensor state changes
         or the statistical period arrives, the DMS common module sets the initial value.
         */
        unsigned int stat_time_counter;
        /* Special for statistical sensors, current state bit count variable, record previous current state,
        each bit represents the state once,
         the previous 32 states are cached at most, and the lowest bit is the most recent state
         */
        unsigned int current_bit_count;
        /* Statistic sensor dedicated, alarm recovery times */
        unsigned int alarm_clear_times;
        /* State counter. In the fault state, it indicates the count of the normal state;
        in the normal state, it indicates the count of the fault state */
        unsigned int status_counter;
        /* Current sensor status change time */
        SYS_TIME object_op_state_ch_time;
        /* Reason for current sensor status change */
        unsigned int object_op_state_chg_cause;
    } statistic_cb;
};

struct dms_sensor_object_cb {
    /* Instance index */
    unsigned short object_index;

    unsigned int sensor_num;

    /* Node ID to which the sensor belongs */
    unsigned int owner_node_id;
    unsigned int owner_node_type;
    struct dms_node_sensor_cb *p_owner_cb;

    /* Sensor instance configuration, from the input configuration of the registered sensor */
    struct dms_sensor_object_cfg sensor_object_cfg;
    struct dms_sensor_object_cfg orig_obj_cfg;

    /* The remaining time, in milliseconds */
    unsigned int remain_time;

    /* Indicates the current sensor value, the latest value read by the sensor
    Discrete sensor: is the event offset value of the sensor
    Threshold sensor: the actual reading of the sensor, such as temperature value
    Statistics sensor: is the value of statistics */
    int current_value;

    /* Record the status of the most recently reported event */
    unsigned int event_status;
    /* The health status of the sensor, collecting all events of the sensor,
    and the most serious level as the status of the sensor */
    unsigned int fault_status;

    struct list_head list;

    DMS_EVENT_LIST_ITEM *p_event_list;
    /* The parameter of the currently detected event updated by the detection function has a
    fixed length of 20 bytes, and the effective data length is determined by the length of the above event parameter.
    The first part of the parameter stores the key field parameters.
     */
    unsigned int paras_len;
    unsigned char event_paras[DMS_MAX_EVENT_DATA_LENGTH];

    union dms_sensor_class_cb class_cb;
};

/* Public sensor detection information record structure definition, external interface */
struct dms_sensor_scan_fail_record {
    unsigned int node_type;
    unsigned int node_id;
    /* The current polled query table handle */
    unsigned int current_handle;
    /* Detect function pointer in current query table */
    void *current_scan_func;
    /* Number of failures to obtain synchronization semaphore */
    unsigned int syn_sema_fail;
    /* Number of failures to obtain mutex semaphore */
    unsigned int mux_sema_fail;
    /* Number of failures to obtain data from the linked list node */
    unsigned int get_data_from_node_fail;
    /* The number of times the obtained pTable structure is invalid */
    unsigned int table_error_fail;
    /* The obtained detection function is empty or the sensor table pointer is empty times */
    unsigned int null_scan_func_fail;
    /* The number of failed calls to the detection function */
    unsigned int call_scan_func_fail;
    unsigned int scan_func_date_error;
    /* The number of errors when the instance table is empty */
    unsigned int null_object_fail;
    /* Number of failures to call alarm processing */
    unsigned int sensor_process_fail;
    /* Number of failed reporting events */
    unsigned int report_event_fail;
};

/* sensor overload time of one scan */
#define DMS_SENSOR_SCAN_OUT_TIME 1000 // 1ms
/* device overload time of one scan */
#define DMS_DEV_SENSOR_SCAN_OUT_TIME 50000 // 50ms

struct dms_dev_sensor_cb;

/* Sensor timing detection execution time recorder */
struct dms_sensor_scan_time_recorder {
    /* The sensor periodically detects whether the processing time is recorded or not, 0 means no recording, 1 means
     * recording */
    unsigned int record_scan_time_flag;
    unsigned int sensor_out_time_count;
    unsigned int sensor_record_index;
    /* Statistics of each sensor execution time record table */
    struct dms_sensor_scan_time_item {
        /* sensor name */
        char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
        /* The timing detection execution time of the sensor information table corresponding to the query table, in
         * milliseconds */
        unsigned long exec_time;
    } sensor_scan_time_record[DMS_MAX_TIME_RECORD_COUNT], max_sensor_scan_record;

    unsigned int dev_out_time_count;
    unsigned long max_dev_scan_record;
    unsigned int dev_record_index;
    unsigned long dev_scan_time_record[DMS_MAX_TIME_RECORD_COUNT];
    /* Start recording the execution time of the sensor scan function */
    void (*start_sensor_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb);
    /* Stop recording the execution time of the device scan function */
    void (*stop_sensor_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb,
        struct dms_sensor_object_cb *psensor_obj_cb);
    /* Start recording the execution time of the device scan function */
    void (*start_dev_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb);
    /* Start recording the execution time of the device scan function */
    void (*stop_dev_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb);
    ka_ktime_t sensor_start_time;
    ka_ktime_t dev_start_time;
};


/* Each child node has an instance of the structure, and the sensor registration of the child node will be attached to
 * this type of table pointer */
struct dms_node_sensor_cb {
    /* Device node ID node type + id */
    unsigned int node_id;
    unsigned short node_type;
    /* User mode sensor registration process pid */
    int pid;
    /* User mode or kernel mode registered function */
    unsigned short env_type;
    /* Sensor information table version */
    unsigned short version;
    /* ID of the device to which the node belongs */
    struct dms_node *owner_node;
    struct list_head list;
    /* Number of sensor types */
    unsigned int sensor_object_num;
    unsigned short health;
    /* Sensor instance table */
    struct list_head sensor_object_table;
};

/* Sensor control block of each chip device */
struct dms_dev_sensor_cb {
    /* device id, 0 in AMP mode, 0~3 in 910 SMP mode */
    int deviceid;
    /* Resource lock */
    struct mutex dms_sensor_mutex;
    int node_cb_num;
    unsigned short health;
    /* Device data group, initial allocation, call for 1 device (device id = 0x1000) on the host side, if device, AMP is
     * 1 device, 910 SMP is 4 devices */
    struct list_head dms_node_sensor_cb_list;
    /* Task scanning process information record */
    struct dms_sensor_scan_fail_record sensor_scan_fail_record;
    /* Sensor timing detection execution time recorder */
    struct dms_sensor_scan_time_recorder scan_time_recorder;
};

#ifndef EVENT_INFO_CONFIG_PATH
#ifdef CFG_HOST_ENV
#define EVENT_INFO_CONFIG_PATH "/etc/dms_events_conf.lst"
#else
#define EVENT_INFO_CONFIG_PATH "/var/dms_events_conf.lst"
#endif
#endif
#define EVENT_INFO_ARRAY_MAX 2000

struct dms_event_config {
    unsigned int event_code;
    unsigned int severity;
};

struct dms_eventinfo_from_config {
    struct dms_event_config event_configs[EVENT_INFO_ARRAY_MAX];
    int config_cnt;
};

extern struct dms_eventinfo_from_config g_event_configs;

unsigned int dms_init_dev_sensor_cb(int deviceid, struct dms_dev_sensor_cb *sensor_cb);

void dms_clean_dev_sensor_cb(struct dms_dev_sensor_cb *sensor_cb);

void dms_sensor_scan_proc(struct dms_dev_sensor_cb *dev_sensor_cb);

void dms_printf_sensor_time_recorder(struct dms_dev_sensor_cb *pdev_sen_cb);
ssize_t dms_sensor_print_sensor_list(char *buf);

unsigned int dms_sensor_check_result_process(unsigned short sensor_class, struct dms_sensor_object_cb *psensor_obj_cb,
    unsigned int *status, unsigned char *event_type);

int dms_add_one_sensor_event(struct dms_sensor_object_cb *psensor_obj_cb,
    struct dms_sensor_event_data_item *pevent_data);
int dms_get_event_string(unsigned char sensor_type, unsigned char event_offset, char *event_str, int inbuf_size);
int dms_get_event_severity(unsigned int node_type, unsigned char sensor_type, unsigned char event_offset,
    unsigned int *severity);
int dms_resume_all_sensor_event(struct dms_sensor_object_cb *psensor_obj_cb);
int dms_check_sensor_data(struct dms_sensor_object_cb *sensor_obj_cb, unsigned char event_data);
unsigned int dms_process_statis_result_report(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cb *psensor_obj_cb, struct dms_sensor_event_data *pevent_data);
DMS_EVENT_LIST_ITEM *sensor_add_event_to_list(DMS_EVENT_LIST_ITEM **pp_event_list,
    struct dms_sensor_event_data_item *pevent_data, struct dms_sensor_object_cb *psensor_obj_cb);
int dms_update_sensor_list(struct dms_sensor_object_cb *p_sensor_obj_cb, DMS_EVENT_LIST_ITEM *p_new_list);
unsigned int dms_sensor_register(struct dms_node *owner_node, struct dms_sensor_object_cfg *psensor_obj_cfg);
unsigned int dms_sensor_register_for_userspace(struct dms_node *owner_node,
    struct dms_sensor_object_cfg *psensor_obj_cfg);
int dms_sensor_event_notify(unsigned int dev_id, struct dms_sensor_object_cfg *psensor_obj_cfg);
int dms_sensor_get_dev_health(struct dms_dev_sensor_cb *dev_sensor_cb);
int dms_get_sensor_type_name(unsigned char sensor_type, char *type_name, int inbuf_size);
int dms_sensor_clean_health_events(struct dms_dev_sensor_cb *dev_sensor_cb);
int dms_sensor_mask_events(struct dms_dev_sensor_cb *dev_sensor_cb, u8 mask,
    u16 node_type, u8 sensor_type, u8 event_state);
bool dms_sensor_check_mask_enable(unsigned int mask, unsigned int offset);
unsigned int dms_sen_init_sensor(void);
unsigned int dms_sen_exit_sensor_event(void);
unsigned int dms_init_dev_sensor_cb(int deviceid, struct dms_dev_sensor_cb *sensor_cb);
void dms_exit_dev_sensor_cb(struct dms_dev_sensor_cb *dev_sensor_cb);
unsigned int dms_sensor_node_unregister(struct dms_node *owner_node);
unsigned int dms_sensor_object_unregister(struct dms_node *owner_node, struct dms_sensor_object_cfg *psensor_obj_cfg);
int dms_sensor_scan_one_node_object(struct dms_dev_sensor_cb *dev_sensor_cb,
    struct dms_node_sensor_cb *node_sensor_cb, struct dms_sensor_object_cb *psensor_obj_cb,
    struct dms_sensor_scan_time_recorder *ptime_recorder);
unsigned long dms_get_time_change(ka_ktime_t start, ka_ktime_t end);
unsigned long dms_get_time_change_ms(ka_ktime_t start, ka_ktime_t end);
void dms_sensor_release(int owner_pid);
int dms_mgnt_clockid_init(void);
int dms_sensor_inject_fault(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_sensor_get_fault_inject_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif

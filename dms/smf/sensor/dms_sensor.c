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

#include <linux/slab.h>
#include <linux/kernel.h>

#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "dms_kernel_version_adapt.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_define.h"
#include "dms_dev_node.h"
#include "dms_sensor_discrete.h"
#include "dms_sensor_general.h"
#include "dms_sensor_statis.h"
#include "dms_sensor_type.h"
#include "dms_sensor_notify.h"
#include "dms_event_converge.h"
#include "kernel_version_adapt.h"
#include "dms_sensor.h"

#ifdef CFG_FEATURE_MDC_CLOCKID_CONFIG
#include <linux/virt_wall_time.h>
#endif

#define print_sysfs (void)ka_dfx_printk

#define CLOCK_ID_STRLEN 9
#define CMDLINE_BUFFER_SIZE 1000
#define CLOCK_VIRTUAL 100
#define CLOCK_REAL 0
#define DECIMAL 10
#define CMDLINE_FILE_PATH   "/proc/cmdline"

static int g_dms_mgnt_clock_id = CLOCK_VIRTUAL;

/* used for generates fault IDs. */
#define FAULT_SENSOR_OFFSET     (9U)
#define FAULT_EVENT_CODE_HOST (0x1U << 30)
#define FAULT_EVENT_CODE_DEVICE (0x2U << 30)
#define FAULT_EVENT_NODETYPE_TO_CODE(nodetype) (((nodetype) & 0x7ff) << 17)

struct dms_eventinfo_from_config g_event_configs = {{{0}}};
KA_EXPORT_SYMBOL(g_event_configs);

/* Globally unique number */
static atomic_t g_dms_sensor_alarm_serial = KA_BASE_ATOMIC_INIT(0);

DMS_EVENT_LIST_ITEM g_event_list_cb[DMS_MAX_EVENT_NUM];
struct mutex g_sensor_event_list_mutex;

static void sensor_init_event_list_cb(void)
{
    (void)memset_s(&g_event_list_cb, sizeof(DMS_EVENT_LIST_ITEM) * DMS_MAX_EVENT_NUM,\
                   0, sizeof(DMS_EVENT_LIST_ITEM) * DMS_MAX_EVENT_NUM);
    ka_task_mutex_init(&g_sensor_event_list_mutex);
}
static void sensor_exit_event_list_cb(void)
{
    ka_task_mutex_destroy(&g_sensor_event_list_mutex);
}

unsigned long dms_get_time_change(ka_ktime_t start, ka_ktime_t end)
{
    unsigned long time_use;
    time_use = (unsigned long)(ka_system_ktime_to_ns(end) - ka_system_ktime_to_ns(start)) / NSEC_PER_USEC;
    return time_use;
}

unsigned long dms_get_time_change_ms(ka_ktime_t start, ka_ktime_t end)
{
    unsigned long time_use;
    time_use = (unsigned long)(ka_system_ktime_to_ns(end) - ka_system_ktime_to_ns(start)) / KA_NSEC_PER_MSEC;
    return time_use;
}

static void dms_start_record_sensor_scan_time(struct dms_dev_sensor_cb *pdev_sen_cb)
{
    pdev_sen_cb->scan_time_recorder.sensor_start_time = ka_system_ktime_get();
}

static void dms_stop_record_sensor_scan_time(struct dms_dev_sensor_cb *pdev_sen_cb,
    struct dms_sensor_object_cb *psensor_obj_cb)
{
    ka_ktime_t current_time;
    unsigned long all_time_use;
    int ret;
    struct dms_sensor_scan_time_recorder *ptime_recorder;
    ptime_recorder = &pdev_sen_cb->scan_time_recorder;
    current_time = ka_system_ktime_get();
    all_time_use = dms_get_time_change(ptime_recorder->sensor_start_time, current_time);
    if (all_time_use > DMS_SENSOR_SCAN_OUT_TIME) {
        if (ptime_recorder->sensor_record_index >= DMS_MAX_TIME_RECORD_COUNT) {
            ptime_recorder->sensor_record_index = 0;
        }
        ptime_recorder->sensor_scan_time_record[ptime_recorder->sensor_record_index].exec_time = all_time_use;
        ret = strcpy_s(ptime_recorder->sensor_scan_time_record[ptime_recorder->sensor_record_index].sensor_name,
            DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cb->sensor_object_cfg.sensor_name);
        if (ret != 0) {
            dms_err("strcpy_s error. (ret=%d)\n", ret);
        }
        /* Record the maximum time consumption */
        if (all_time_use > ptime_recorder->max_sensor_scan_record.exec_time) {
            ptime_recorder->max_sensor_scan_record.exec_time = all_time_use;
            ret = strcpy_s(ptime_recorder->max_sensor_scan_record.sensor_name, DMS_SENSOR_DESCRIPT_LENGTH,
                psensor_obj_cb->sensor_object_cfg.sensor_name);
            if (ret != 0) {
                dms_err("strcpy_s error. (ret=%d)\n", ret);
            }
        }
        /* Cumulative count of timeout */
        ptime_recorder->sensor_out_time_count++;
        ptime_recorder->sensor_record_index++;
    }
}

static void dms_start_record_dev_scan_time(struct dms_dev_sensor_cb *pdev_sen_cb)
{
    pdev_sen_cb->scan_time_recorder.dev_start_time = ka_system_ktime_get();
}

static void dms_stop_record_dev_scan_time(struct dms_dev_sensor_cb *pdev_sen_cb)
{
    ka_ktime_t current_time;
    unsigned long all_time_use;
    struct dms_sensor_scan_time_recorder *ptime_recorder;
    ptime_recorder = &pdev_sen_cb->scan_time_recorder;
    current_time = ka_system_ktime_get();
    all_time_use = dms_get_time_change(ptime_recorder->dev_start_time, current_time);
    if (all_time_use > DMS_DEV_SENSOR_SCAN_OUT_TIME) {
        if (ptime_recorder->dev_record_index >= DMS_MAX_TIME_RECORD_COUNT) {
            ptime_recorder->dev_record_index = 0;
        }
        ptime_recorder->dev_scan_time_record[ptime_recorder->dev_record_index] = all_time_use;
        /* Record the maximum time consumption */
        if (all_time_use > ptime_recorder->max_dev_scan_record) {
            ptime_recorder->max_dev_scan_record = all_time_use;
        }
        /* Cumulative all count of timeout */
        ptime_recorder->dev_out_time_count++;
        ptime_recorder->dev_record_index++;
    }
}

static void dms_init_sensor_time_recorder(struct dms_dev_sensor_cb *pdev_sen_cb)
{
    struct dms_sensor_scan_time_recorder *ptime_recorder;
    ptime_recorder = &pdev_sen_cb->scan_time_recorder;

    (void)memset_s((void *)ptime_recorder, sizeof(struct dms_sensor_scan_time_recorder), 0,
        sizeof(struct dms_sensor_scan_time_recorder));
    ptime_recorder->record_scan_time_flag = 1;
    ptime_recorder->start_sensor_scan_record = dms_start_record_sensor_scan_time;
    ptime_recorder->stop_sensor_scan_record = dms_stop_record_sensor_scan_time;
    ptime_recorder->start_dev_scan_record = dms_start_record_dev_scan_time;
    ptime_recorder->stop_dev_scan_record = dms_stop_record_dev_scan_time;
}

static struct dms_dev_sensor_cb *dms_get_sensor_cb(struct dms_node *owner_node)
{
    struct dms_dev_ctrl_block *temp_dev_cb = NULL;
    temp_dev_cb = dms_get_dev_cb(owner_node->owner_devid);
    if (temp_dev_cb == NULL) {
        return NULL;
    }
    return (&temp_dev_cb->dev_sensor_cb);
}

STATIC int dms_get_node_sensor_cb_by_nodeid(struct dms_dev_sensor_cb *dev_sensor_cb, unsigned int node_type,
    unsigned int node_id, struct dms_node_sensor_cb **node_sensor_cb)
{
    int result;
    struct dms_node_sensor_cb *pnode_ctrl = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;
    result = dms_check_node_type(node_type);
    if (result != DRV_ERROR_NONE) {
        dms_err("Invalid node type. (node_type=0x%x, node_id=%u)\n", node_type, node_id);
        return DRV_ERROR_PARA_ERROR;
    }
    *node_sensor_cb = NULL;
    list_for_each_entry_safe(pnode_ctrl, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list)
    {
        if ((pnode_ctrl->node_id == node_id) && (pnode_ctrl->node_type == node_type)) {
            *node_sensor_cb = pnode_ctrl;
            break;
        }
    }
    return DRV_ERROR_NONE;
}

static struct dms_node_sensor_cb *dms_get_or_create_node_sensor_cb(struct dms_dev_sensor_cb *dev_sensor_cb,
    struct dms_node *owner_node, int env_type)
{
    int result;
    int pid;
    struct dms_node_sensor_cb *node_sensor_cb = NULL;

    result = dms_get_node_sensor_cb_by_nodeid(dev_sensor_cb, owner_node->node_type,
        owner_node->node_id, &node_sensor_cb);
    if (result != DRV_ERROR_NONE) {
        dms_err("get node sensor cb failed!\n");
        return NULL;
    }
    /* for kernel space, not used the pid, set default value -1 */
    pid = (env_type == DMS_SENSOR_ENV_USER_SPACE) ? (current->tgid) : (-1);
    if (node_sensor_cb != NULL) {
        if (node_sensor_cb->env_type != env_type) {
            /* The currently registered environment (user mode, kernel mode) is inconsistent with the previously created
             * node */
            dms_err("env is not match\n");
            return NULL;
        }
        if (node_sensor_cb->pid != pid) {
            /* The currently registered process (user mode, kernel mode) is inconsistent with the previously created
             * process */
            dms_err("pid is not match. (node_type=0x%x, node_id=%d, owner_pid=%d, current_pid=%d\n",
                owner_node->node_type, owner_node->node_id, node_sensor_cb->pid, pid);
            return NULL;
        }
        return node_sensor_cb;
    }

    /* Allocate memory for sensor status query table node data */
    node_sensor_cb = (struct dms_node_sensor_cb *)dbl_kzalloc(sizeof(struct dms_node_sensor_cb),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node_sensor_cb == NULL) {
        /* Print error message: memory request failed */
        dms_err("Malloc memory failed!\n");

        return NULL;
    }
    /* Add a clear operation to pdev_sensor_table */
    (void)memset_s((void *)node_sensor_cb, sizeof(struct dms_node_sensor_cb), 0, sizeof(struct dms_node_sensor_cb));
    node_sensor_cb->env_type = env_type;
    node_sensor_cb->pid = pid;
    node_sensor_cb->sensor_object_num = 0;
    node_sensor_cb->node_id = owner_node->node_id;
    node_sensor_cb->node_type = owner_node->node_type;
    node_sensor_cb->owner_node = owner_node;
    node_sensor_cb->version = 0;
    node_sensor_cb->health = 0;
    INIT_LIST_HEAD(&node_sensor_cb->sensor_object_table);
    /* Add the created node to the node list of the device */
    ka_list_add(&node_sensor_cb->list, &dev_sensor_cb->dms_node_sensor_cb_list);
    dev_sensor_cb->node_cb_num++;
    return node_sensor_cb;
}

static unsigned int dms_sensor_class_init(unsigned short sensor_class, struct dms_sensor_object_cb *psensor_obj)
{
    /* Process different types of sensor types separately */
    if (sensor_class == DMS_STATICSTIC_SENSOR_CLASS) {
        /* Statistical threshold sensor */
        return dms_sensor_class_init_statis(psensor_obj);
    } else if (sensor_class == DMS_GENERAL_THRESHOLD_SENSOR_CLASS) {
        /* General threshold type sensor */
        return dms_sensor_class_init_general(psensor_obj);
    } else if (sensor_class == DMS_DISCRETE_SENSOR_CLASS) {
        /* Discrete sensor format is legal check */
        return dms_sensor_class_init_discrete(psensor_obj);
    } else {
        return DRV_ERROR_PARA_ERROR;
    }
}

STATIC unsigned int dms_add_sensor_object_table(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cfg *psensor_cfg)
{
    struct dms_sensor_object_cb *temp_sensor = NULL;

    if (node_sensor_cb->sensor_object_num >= DMS_MAX_NODE_SENSOR_COUNT) {
        dms_err("Sensor object too many. (object_num=%u, max_num=%u)\n",
            node_sensor_cb->sensor_object_num, DMS_MAX_NODE_SENSOR_COUNT);
        return DRV_ERROR_NO_RESOURCES;
    }
    /* Allocate memory for sensor status query table node data */
    temp_sensor = (struct dms_sensor_object_cb *)dbl_kzalloc(sizeof(struct dms_sensor_object_cb),
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (temp_sensor == NULL) {
        /* Print error message: memory request failed */
        dms_err("Malloc memory failed!\n");
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    /* Add a clear operation to pdev_sensor_table */
    (void)memset_s((void *)temp_sensor, sizeof(struct dms_sensor_object_cb), 0, sizeof(struct dms_sensor_object_cb));

    /* Initial configuration */
    temp_sensor->sensor_object_cfg = *psensor_cfg;
    temp_sensor->orig_obj_cfg = *psensor_cfg;
    temp_sensor->object_index = node_sensor_cb->sensor_object_num + 1;
    temp_sensor->sensor_num = (psensor_cfg->sensor_type << DMS_MASK_16_BIT) + temp_sensor->object_index;
    temp_sensor->owner_node_id = node_sensor_cb->node_id;
    temp_sensor->owner_node_type = node_sensor_cb->node_type;
    temp_sensor->p_owner_cb = node_sensor_cb;

    temp_sensor->event_status = DMS_SENSOR_STATUS_GOOD;
    temp_sensor->fault_status = DMS_SENSOR_STATUS_GOOD;
    temp_sensor->current_value = 0;
    temp_sensor->remain_time = psensor_cfg->scan_interval;
    temp_sensor->p_event_list = NULL;
    (void)dms_sensor_class_init(psensor_cfg->sensor_class, temp_sensor);

    /* Add the created node to the node list of the device */
    ka_list_add(&temp_sensor->list, &node_sensor_cb->sensor_object_table);
    node_sensor_cb->sensor_object_num++;
    return DRV_ERROR_NONE;
}

static unsigned int dms_sensor_class_check(unsigned short sensor_class, struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    /* Process different types of sensor types separately */
    if (sensor_class == DMS_STATICSTIC_SENSOR_CLASS) {
        /* Statistical threshold sensor */
        return dms_sensor_class_check_statis(psensor_obj_cfg);
    } else if (sensor_class == DMS_GENERAL_THRESHOLD_SENSOR_CLASS) {
        /* General threshold type sensor */
        return dms_sensor_class_check_general(psensor_obj_cfg);
    } else if (sensor_class == DMS_DISCRETE_SENSOR_CLASS) {
        /* Discrete sensor format is legal check */
        return dms_sensor_class_check_discrete(psensor_obj_cfg);
    } else {
        return DRV_ERROR_PARA_ERROR;
    }
}

STATIC int dms_check_sensor_type(unsigned char sensor_type)
{
    int i, count;
    struct tag_dms_sensor_type *dms_sensor_type;

    if (sensor_type == DMS_SENSOR_RESERVED_FOR_PRODUCT) {
        dms_err("sensor type is reserved. (sensor_type=0x%x)\n", sensor_type);
        return DRV_ERROR_PARA_ERROR;
    }

    count = dms_get_sensor_type_count();
    dms_sensor_type = dms_get_sensor_type();
    for (i = 0; i < count; i++) {
        if (dms_sensor_type[i].sensor_type == sensor_type) {
            return DRV_ERROR_NONE;
        }
    }
    dms_err("sensor type error. (sensor_type=0x%x)\n", sensor_type);
    return DRV_ERROR_PARA_ERROR;
}
bool dms_sensor_check_mask_enable(unsigned int mask, unsigned int offset)
{
    if ((mask & (1 << offset)) != 0) {
        return true;
    } else {
        return false;
    }
}

static unsigned int dms_sensor_cfg_check(struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    unsigned int result;
    unsigned short sensor_class;

    /* If the enable flag is illegal */
    if ((psensor_obj_cfg->enable_flag != DMS_SENSOR_ENABLE_FALG) &&
        (psensor_obj_cfg->enable_flag != DMS_SENSOR_DISABLE_FALG)) {
        /* Print error message: illegal enable flag */
        dms_err("Invalid parameter ulEnable. (enable_flag=0x%x)\n", psensor_obj_cfg->enable_flag);
        return DRV_ERROR_PARA_ERROR;
    }

    /* If the scan_interval is illegal */
    if ((psensor_obj_cfg->scan_interval < DMS_SENSOR_CHECK_TIMER_LEN) &&
        (psensor_obj_cfg->scan_module == DMS_SERSOR_SCAN_PERIOD)) {
        dms_err("Invalid parameter scan_interval. (scan_interval=%ums; scan_module=%u)\n",
            psensor_obj_cfg->scan_interval, psensor_obj_cfg->scan_module);
        return DRV_ERROR_PARA_ERROR;
    }

    sensor_class = psensor_obj_cfg->sensor_class;
    if ((sensor_class != DMS_STATICSTIC_SENSOR_CLASS) && (sensor_class != DMS_GENERAL_THRESHOLD_SENSOR_CLASS) &&
        (sensor_class != DMS_DISCRETE_SENSOR_CLASS) && (sensor_class != DMS_PER_SENSOR_CLASS)) {
        /* Print error message: the type of information table is illegal */
        dms_err("Sensor Table Check Failed(sensor_class=%u)\n", sensor_class);
        return DRV_ERROR_PARA_ERROR;
    }

    if (dms_check_sensor_type(psensor_obj_cfg->sensor_type) != DRV_ERROR_NONE) {
        dms_err("sensor type is not support\n");
        return DRV_ERROR_PARA_ERROR;
    }

    /* The detection processing flag needs to be judged whether it is legal */
    if ((psensor_obj_cfg->proc_flag != DMS_SENSOR_PROC_ENABLE_FLAG) &&
        (psensor_obj_cfg->proc_flag != DMS_SENSOR_PROC_DISABLE_FLAG)) {
        /* Print error message: the detection processing flag is illegal */
        dms_err("Invalid Sensor Table Process Flag. (proc_flag=0x%x)\n", psensor_obj_cfg->proc_flag);
        return DRV_ERROR_PARA_ERROR;
    }
    /* The kernel detection function pointer needs to determine whether it is equal to the null pointer 0 */
    if (psensor_obj_cfg->pf_scan_func == NULL) {
        /* Print error message: the detection function is illegal */
        dms_err("Invalid Sensor Table Scan Function - NULL Pointer\n");
        return DRV_ERROR_PARA_ERROR;
    }

    /* Check whether the sensor class format is correct */
    result = dms_sensor_class_check(psensor_obj_cfg->sensor_class, psensor_obj_cfg);
    if (result != DRV_ERROR_NONE) {
        /* Print error message: the format check of the message table failed */
        dms_err("Sensor Table Check Failed!\n");
        return DRV_ERROR_PARA_ERROR;
    }
    return DRV_ERROR_NONE;
}

static bool dms_compare_sensor_object(struct dms_sensor_object_cfg *psensor_obj_cfg1,
    struct dms_sensor_object_cfg *psensor_obj_cfg2)
{
    if ((psensor_obj_cfg1->sensor_type == psensor_obj_cfg2->sensor_type) &&
        (psensor_obj_cfg1->pf_scan_func == psensor_obj_cfg2->pf_scan_func) &&
        (ka_base_strcmp(psensor_obj_cfg1->sensor_name, psensor_obj_cfg2->sensor_name) == 0)) {
        return true;
    } else {
        return false;
    }
}

static unsigned int dms_is_sensor_object_repeat(struct dms_dev_sensor_cb *pdev_sen_cb, int node_type, int nodeid,
    struct dms_sensor_object_cfg *psensor_obj_cfg, unsigned int *repeat)
{
    struct dms_sensor_object_cb *sensor_type_item = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;
    struct dms_node_sensor_cb *pnode_sen_cb = NULL;
    int rec;
    *repeat = DMS_SENSOR_TABLE_NOT_REPEAT;

    rec = dms_get_node_sensor_cb_by_nodeid(pdev_sen_cb, node_type, nodeid, &pnode_sen_cb);
    if (rec != DRV_ERROR_NONE) {
        dms_err("get node sensor cb fail. (nodeid=%d)\n", nodeid);
        return rec;
    }
    /* Traverse all sensor types and all sensor instances under each type for detection. If the detection result is an
     * event that needs to be reported, then report the event */
    if (pnode_sen_cb != NULL) {
        list_for_each_entry_safe(sensor_type_item, tmp_sensor_ctl, &(pnode_sen_cb->sensor_object_table), list)
        {
            if (dms_compare_sensor_object(&sensor_type_item->sensor_object_cfg, psensor_obj_cfg) == true) {
                *repeat = DMS_SENSOR_TABLE_REPEAT;
                dms_err("add repeat sensor, (new sensor type:0x%x; name:%.*s;"
                    "old sensor type:0x%x;name:%.*s\n", psensor_obj_cfg->sensor_type,
                    DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cfg->sensor_name,
                    sensor_type_item->sensor_object_cfg.sensor_type,
                    DMS_SENSOR_DESCRIPT_LENGTH, sensor_type_item->sensor_object_cfg.sensor_name);
                return DRV_ERROR_NONE;
            }
        }
    }
    return DRV_ERROR_NONE;
}

static unsigned int dms_sensor_add_obj_node(struct dms_node *owner_node, struct dms_dev_sensor_cb *dev_sensor_cb,
    struct dms_sensor_object_cfg *psensor_obj_cfg, int env_type)
{
    unsigned int result;
    struct dms_node_sensor_cb *node_sensor_cb = NULL;
    unsigned int repeat = DMS_SENSOR_TABLE_NOT_REPEAT;

    /* Find whether the sensor information table has been registered */
    result = dms_is_sensor_object_repeat(dev_sensor_cb, owner_node->node_type,
                                         owner_node->node_id, psensor_obj_cfg, &repeat);
    if (result != DRV_ERROR_NONE) {
        /* Print error information: determine whether the sensor information table repeatedly fails */
        dms_err("Judge whether Sensor Table is repeated failed. (nodeid=%d)\n", owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (repeat == DMS_SENSOR_TABLE_REPEAT) {
        /* Print error message: duplicate sensor information table */
        dms_err("Repeated sensor object. (nodeid=%d)\n", owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }

    node_sensor_cb = dms_get_or_create_node_sensor_cb(dev_sensor_cb, owner_node, env_type);
    if (node_sensor_cb == NULL) {
        /* Print error message: duplicate sensor information table */
        dms_err("add or get sensor cb fail. (node_type=0x%x, node_id=%d)\n",
            owner_node->node_type, owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }

    result = dms_add_sensor_object_table(node_sensor_cb, psensor_obj_cfg);
    if (result != DRV_ERROR_NONE) {
        /* Print error message: duplicate sensor information table */
        dms_err("add sensor type fail. (node_type=0x%x, node_id=%d)\n", owner_node->node_type, owner_node->node_id);
        if (node_sensor_cb->sensor_object_num == 0) {
            ka_list_del(&node_sensor_cb->list);
            dbl_kfree(node_sensor_cb);
            node_sensor_cb = NULL;
            dev_sensor_cb->node_cb_num--;
        }
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static unsigned int dms_sensor_register_all(struct dms_node *owner_node, struct dms_sensor_object_cfg *psensor_obj_cfg,
    int env_type)
{
    unsigned int result;
    struct dms_dev_sensor_cb *dev_sensor_cb = NULL;

    /* The information table pointer is illegal */
    if ((psensor_obj_cfg == NULL) || (owner_node == NULL)) {
        /* Print error message: illegal parameter */
        dms_err("Invalid parameter\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if ((env_type != DMS_SENSOR_ENV_KERNEL_SPACE) && (env_type != DMS_SENSOR_ENV_USER_SPACE)) {
        /* Print error message: illegal parameter */
        dms_err("env Invalid parameter\n");
        return DRV_ERROR_PARA_ERROR;
    }
    /* Check the format of the sensor information table */
    result = dms_sensor_cfg_check(psensor_obj_cfg);
    if (result != DRV_ERROR_NONE) {
        /* Print error message: the format check of the message table failed */
        dms_err("Invalid sensor format. (node_id=%d)\n", owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }
    dev_sensor_cb = dms_get_sensor_cb(owner_node);
    if (dev_sensor_cb == NULL) {
        /* Print error message: the format check of the message table failed */
        dms_err("dms get sensor cb fail. (nodeid=%d)\n", owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }
    /* Get mutex semaphore */
    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);

    /* Find whether the sensor information table has been registered */
    result = dms_sensor_add_obj_node(owner_node, dev_sensor_cb, psensor_obj_cfg, env_type);
    if (result != DRV_ERROR_NONE) {
        ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
        /* Print error message: duplicate sensor information table */
        dms_err("add sensor type fail. (nodeid=%d)\n", owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }
    /* Release the mutex semaphore */
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);

    return DRV_ERROR_NONE;
}

unsigned int dms_fill_event_data(const struct dms_sensor_object_cb *psensor_obj_cb,
    DMS_EVENT_LIST_ITEM *event_item, unsigned char assertion, struct dms_event_obj *p_event_obj)
{
    struct dms_node *owner_node = NULL;
    int ret;
    unsigned int severity = DMS_EVENT_OK;

    (void)memset_s((void *)p_event_obj, sizeof(struct dms_event_obj), 0, sizeof(struct dms_event_obj));

    /* Get the sensor number */
    ret = dms_get_event_severity(psensor_obj_cb->owner_node_type, psensor_obj_cb->sensor_object_cfg.sensor_type,
        event_item->event_data, &severity);
    if (ret != 0) {
        dms_warn("get event severity. (ret=%d)\n", ret);
    }

    if ((psensor_obj_cb->p_owner_cb == NULL) || (psensor_obj_cb->p_owner_cb->owner_node == NULL)) {
        return DRV_ERROR_INVALID_HANDLE;
    }

    owner_node = psensor_obj_cb->p_owner_cb->owner_node;

    p_event_obj->pid = psensor_obj_cb->p_owner_cb->pid;
    p_event_obj->event_type = DMS_ET_SENSOR;
    p_event_obj->deviceid = psensor_obj_cb->p_owner_cb->owner_node->owner_devid;
    /* Fill node ID */
    if (owner_node->owner_device == NULL) {
        p_event_obj->node_type = psensor_obj_cb->owner_node_type;
        p_event_obj->node_id = psensor_obj_cb->owner_node_id;
        p_event_obj->sub_node_type = 0;
        p_event_obj->sub_node_id = 0;
    } else {
        p_event_obj->node_type = owner_node->owner_device->node_type;
        p_event_obj->node_id = owner_node->owner_device->node_id;
        p_event_obj->sub_node_type = psensor_obj_cb->owner_node_type;
        /* The node ID of owner_node cannot be used because it is a global ID.
         * Need to use the node ID in the current owner_device, that is inner_node_id.
         * For example, if there are 10 AICs, each AIC has an SMMU. The @node_id is
         * globally unique for AIC_SMMU. However, for a single AIC, sub_node_id is the
         * ID of the current AIC and the value is 0. */
        p_event_obj->sub_node_id = owner_node->inner_node_id;
    }

    p_event_obj->severity = severity;
    /* Fill in the sensor number */
    p_event_obj->event.sensor_event.sensor_num = psensor_obj_cb->sensor_num;
    /* Fill in the sensor type */
    p_event_obj->event.sensor_event.sensor_type = psensor_obj_cb->sensor_object_cfg.sensor_type;
    /* Fill in the event type (generation, recovery or one-time) */
    p_event_obj->event.sensor_event.assertion = assertion;
    p_event_obj->event.sensor_event.event_state = event_item->event_data;
    p_event_obj->time_stamp = event_item->timestamp;
    /* fill alarm_serial number */
    p_event_obj->alarm_serial_num = event_item->alarm_serial_num;
    ka_dfx_pr_debug("[dms_module][dms_fill_event_data] node type(0x%x) event type(0x%x)"
        "serial num(%u) sensor name:%.*s\n", psensor_obj_cb->owner_node_type, event_item->event_data,
        event_item->alarm_serial_num, DMS_SENSOR_DESCRIPT_LENGTH, event_item->sensor_name);
    ret = strcpy_s(p_event_obj->event.sensor_event.sensor_name, DMS_SENSOR_DESCRIPT_LENGTH, event_item->sensor_name);
    if (ret != 0) {
        dms_err("Strcpy_s fail. (ret=%d)\n", ret);
        return ret;
    }
    /* Fill in the parameter pointer and parameter length */
    if ((event_item->para_len) > 0 && (event_item->event_paras != NULL)) {
        p_event_obj->event.sensor_event.param_len = event_item->para_len;
        ret = memcpy_s((void *)p_event_obj->event.sensor_event.param_buffer, DMS_MAX_EVENT_DATA_LENGTH,
            (const void *)(event_item->event_paras), event_item->para_len);
        if (ret != 0) {
            dms_err("memcpy_s fail. (ret=%d)\n", ret);
            return ret;
        }
    }

    ret = memcpy_s(p_event_obj->event.sensor_event.event_info, sizeof(p_event_obj->event.sensor_event.event_info),
                   event_item->event_info, sizeof(event_item->event_info));
    if (ret != 0) {
        dms_err("memcpy_s fail. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

static unsigned int dms_sensor_report_event(struct dms_sensor_object_cb *psensor_obj_cb,
    DMS_EVENT_LIST_ITEM *event_item, unsigned char assertion)
{
    unsigned int result;
    struct dms_event_obj sensor_event;
    u32 event_code = 0;

    (void)dms_fill_event_data(psensor_obj_cb, event_item, assertion, &sensor_event);
    /* Call the interface of the event processing module to process the event */
    result = dms_event_report(&sensor_event);
    if (result != DRV_ERROR_NONE) {
        /* Print error message: event processing failed */
        dms_err("Event Process Failed. (Node Type=%#x; Sensor num=%#x; name=\"%.*s\"; "
            "Sensor Type=%#x; Sensor Object=%#x; assertion:%u, severity:%u)\n",
            psensor_obj_cb->owner_node_type, psensor_obj_cb->sensor_num,
            DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cb->sensor_object_cfg.sensor_name,
            psensor_obj_cb->sensor_object_cfg.sensor_type, psensor_obj_cb->object_index,
            assertion, sensor_event.severity);
        return DRV_ERROR_INNER_ERR;
    }
    result = dms_event_obj_to_error_code(sensor_event, &event_code);
    if (result != 0) {
        dms_err("Get event code failed. (result=%d)\n", result);
    }
    dms_event("Event Process Success. (Event id=0x%x; Node Type=%#x; Sensor num=%#x; name=\"%.*s\"; "
        "Sensor Type=%#x; Sensor Object=%#x; assertion=%u; severity=%u; event_state=0x%x; private_data=0x%llx)\n",
        event_code, psensor_obj_cb->owner_node_type, psensor_obj_cb->sensor_num,
        DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cb->sensor_object_cfg.sensor_name,
        psensor_obj_cb->sensor_object_cfg.sensor_type, psensor_obj_cb->object_index,
        assertion, sensor_event.severity, sensor_event.event.sensor_event.event_state,
        psensor_obj_cb->sensor_object_cfg.private_data);

    return DRV_ERROR_NONE;
}

/* The sensor generates an alarm flow */
static int dms_generated_alarm_serial(unsigned int *alarm_serial_no)
{
    *alarm_serial_no = atomic_inc_return(&g_dms_sensor_alarm_serial);
    return DRV_ERROR_NONE;
}

int dms_mgnt_clockid_init(void)
{
    char buffer[CMDLINE_BUFFER_SIZE] = {0};
    struct file *fp = NULL;
    char *ptr = NULL;
    long read_num;
    loff_t pos = 0;
    fp = ka_fs_filp_open(CMDLINE_FILE_PATH, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL(fp)) {
        dms_err("Open file failed. (file=%s; errno=%ld)\n", CMDLINE_FILE_PATH, PTR_ERR(fp));
        return -EINVAL;
    }
#ifndef AOS_LLVM_BUILD
    read_num = ka_fs_kernel_read(fp, buffer, CMDLINE_BUFFER_SIZE - 1, &pos);
#else
    read_num = kernel_read(fp, buffer, CMDLINE_BUFFER_SIZE - 1, &pos);
#endif
    if ((read_num <= 0) || (read_num >= CMDLINE_BUFFER_SIZE)) {
        dms_err("Read len error. (read_num=%ld; valid_range=[%d, %d])\n", read_num, 0, CMDLINE_BUFFER_SIZE - 1);
        (void)filp_close(fp, NULL);
        fp = NULL;
        return -EINVAL;
    }
    buffer[read_num] = '\0';
    ptr = ka_base_strstr(buffer, "dpclk=100");
    if ((ptr != NULL) && ((*(ptr + CLOCK_ID_STRLEN) == '\0') || (*(ptr + CLOCK_ID_STRLEN) == ' '))) {
        g_dms_mgnt_clock_id = CLOCK_REAL;
    }
    dms_info("Parse clock config. (clock_id=%d)\n", g_dms_mgnt_clock_id);
    (void)filp_close(fp, NULL);
    fp = NULL;
    return 0;
}

/* Get system time */
static int dms_sensor_get_timestamp(unsigned long long *timestamp)
{
#ifdef CFG_FEATURE_MDC_CLOCKID_CONFIG
    struct timespec64 ts;
    struct timeval sys_time = {0};
    if (timestamp == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }
    if (g_dms_mgnt_clock_id == CLOCK_VIRTUAL) {
        ktime_get_virtual_ts64(&ts);
    } else {
        ktime_get_real_ts64(&ts);
    }
    sys_time.tv_sec = ts.tv_sec;
    sys_time.tv_usec = ts.tv_nsec / NSEC_PER_USEC;
    *timestamp = (sys_time.tv_sec * MSEC_PER_SEC) + (sys_time.tv_usec / USEC_PER_MSEC);

#else
    ka_ktime_t sys_time = {0};
    if (timestamp == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }
    sys_time = ka_system_ktime_get();
    *timestamp = (unsigned long long)ka_system_ktime_to_ns(sys_time) / KA_NSEC_PER_MSEC;
#endif

    return DRV_ERROR_NONE;
}

static DMS_EVENT_LIST_ITEM *sensor_alloc_event_list_cb(void)
{
    int i;

    /* Add a mutex lock to prevent simultaneous access between multiple threads and apply for the same memory space,
    which will cause an alarm exception */
    ka_task_mutex_lock(&g_sensor_event_list_mutex);

    for (i = 0; i < DMS_MAX_EVENT_NUM; i++) {
        if (!g_event_list_cb[i].in_use) {
            g_event_list_cb[i].in_use = 1;
            ka_task_mutex_unlock(&g_sensor_event_list_mutex);
            return &g_event_list_cb[i];
        }
    }
    ka_task_mutex_unlock(&g_sensor_event_list_mutex);

    dms_err("No enough sensor event space! (event_num_limit=%d)\n", DMS_MAX_EVENT_NUM);
    return 0;
}

static int sensor_delete_event_list(DMS_EVENT_LIST_ITEM *p_list)
{
    if (p_list == NULL) {
        /* Empty means no event, which is normal */
        return DRV_ERROR_NONE;
    }
    if (p_list->p_next != NULL) {
        (void)sensor_delete_event_list(p_list->p_next);
    }
    ka_task_mutex_lock(&g_sensor_event_list_mutex);
    p_list->in_use = 0;
    p_list->p_next = NULL;
    if (p_list->event_paras != NULL) {
        dbl_kfree(p_list->event_paras);
        p_list->event_paras = NULL;
        p_list->para_len = 0;
    }
    ka_task_mutex_unlock(&g_sensor_event_list_mutex);
    return DRV_ERROR_NONE;
}

STATIC int sensor_init_event_node(struct dms_sensor_event_data_item *pevent_data,
    struct dms_sensor_object_cb *psensor_obj_cb, DMS_EVENT_LIST_ITEM *p_list)
{
    int ret;

    p_list->p_next = 0;
    p_list->is_report = false;
    p_list->event_data = (unsigned char)pevent_data->current_value;
    p_list->sensor_num = psensor_obj_cb->sensor_num;
    /* Get Time */
    (void)dms_sensor_get_timestamp(&p_list->timestamp);
    /* Fill in the serial number */
    (void)dms_generated_alarm_serial(&p_list->alarm_serial_num);
    /* init continued generation times */
    p_list->continued_count = 1;

    ret = strcpy_s(p_list->sensor_name, DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cb->sensor_object_cfg.sensor_name);
    if (ret != 0) {
        dms_err("Copy sensor name failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = memcpy_s(p_list->event_info, sizeof(p_list->event_info),
                   pevent_data->event_info, sizeof(pevent_data->event_info));
    if (ret != 0) {
        dms_err("Copy event para failed. (ret=%d)\n", ret);
        return ret;
    }

    if ((pevent_data->data_size > 0) && (pevent_data->data_size <= DMS_MAX_EVENT_DATA_LENGTH)) {
        p_list->event_paras = (unsigned char *)dbl_kmalloc(pevent_data->data_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (p_list->event_paras == NULL) {
            /* If the memory is not enough, give up recording additional parameters and continue processing */
            p_list->para_len = 0;
            dms_err("sensor_add_event_to_list malloc failed. (sensor_num=%u; para_len=%u)\n",
                psensor_obj_cb->sensor_num, pevent_data->data_size);
            return -ENOMEM;
        }
        ret = memcpy_s((void *)p_list->event_paras, pevent_data->data_size,
                       pevent_data->event_data, pevent_data->data_size);
        if (ret != 0) {
            dms_err("Copy event para failed. (ret=%d)\n", ret);
            dbl_kfree(p_list->event_paras);
            p_list->event_paras = NULL;
            p_list->para_len = 0;
            return ret;
        }
        p_list->para_len = pevent_data->data_size;
    }

    return DRV_ERROR_NONE;
}

DMS_EVENT_LIST_ITEM *sensor_add_event_to_list(DMS_EVENT_LIST_ITEM **pp_event_list,
    struct dms_sensor_event_data_item *pevent_data, struct dms_sensor_object_cb *psensor_obj_cb)
{
    int ret;
    DMS_EVENT_LIST_ITEM *p_list = NULL;
    DMS_EVENT_LIST_ITEM *p_tail = NULL;

    if (pp_event_list == NULL) {
        dms_err("sensor_add_event_to_list para error\n");
        return NULL;
    }
    if ((*pp_event_list) == NULL) {
        p_list = sensor_alloc_event_list_cb();
        if (p_list == NULL) {
            return NULL;
        }
        *pp_event_list = p_list;
    } else {
        /* Confirm that p_list is not empty */
        p_list = (*pp_event_list);
        /* p_tail may not be initialized before being used */
        p_tail = p_list;
        /* Check for recurring events */
        while (p_list != NULL) {
            if ((p_list->event_data == (unsigned char)pevent_data->current_value) &&
                (p_list->sensor_num == psensor_obj_cb->sensor_num)) {
                return NULL;
            }
            p_tail = p_list;
            p_list = p_list->p_next;
        }
        /* Find the tail */
        p_list = p_tail;
        /* Request a sensor event cb */
        p_list->p_next = sensor_alloc_event_list_cb();
        if (p_list->p_next == NULL) {
            return NULL;
        }
        p_list = p_list->p_next;
    }

    ret = sensor_init_event_node(pevent_data, psensor_obj_cb, p_list);
    if (ret != 0) {
        dms_err("Sensor init event failed. (ret=%d)\n", ret);
        return NULL;
    }
    return p_list;
}

static void sensor_proc_diff_list(struct dms_sensor_object_cb *p_sensor_obj_cb, DMS_EVENT_LIST_ITEM *p_new_list)
{
    bool matched = false;
    DMS_EVENT_LIST_ITEM *p_work_list = NULL;
    DMS_EVENT_LIST_ITEM *p_old_list = NULL;
    p_old_list = p_sensor_obj_cb->p_event_list;

    /* The new and old event linked lists are not empty, you have to compare them one by one */
    p_work_list = p_new_list;
    /* Compare one by one, first check that there is no old event to see if it still exists in the new event list */
    while (p_old_list != NULL) {
        matched = false;
        while (p_work_list != NULL) {
            if (p_work_list->event_data == p_old_list->event_data) {
                /* If it is found, the new event inherits the old event */
                p_work_list->is_report = p_old_list->is_report;
                p_work_list->timestamp = p_old_list->timestamp;
                p_work_list->alarm_serial_num = p_old_list->alarm_serial_num;
                matched = true;
                /* inherit continued count */
                p_work_list->continued_count = p_old_list->continued_count + 1;
                break;
            }
            p_work_list = p_work_list->p_next;
        }
        /* If the old event is not found in the new list, it means that the event has disappeared, and report the
         * recovery event */
        if ((false == matched) && dms_sensor_check_mask_enable(p_sensor_obj_cb->sensor_object_cfg.deassert_event_mask,
            p_old_list->event_data)) {
            if (p_old_list->is_report == true) {
                /* If the recovery mask is enabled, the recovery event is not reported */
                (void)dms_sensor_report_event(p_sensor_obj_cb, p_old_list, DMS_EVENT_TYPE_RESUME);
            }
        }
        p_old_list = p_old_list->p_next;
        p_work_list = p_new_list;
    }
}

static inline unsigned char sensor_event_state_convert_assertion(struct dms_sensor_object_cb *p_sensor_obj_cb,
    unsigned char event_state)
{
    struct dms_sensor_object_cfg *obj_cfg = &p_sensor_obj_cb->sensor_object_cfg;

    if ((obj_cfg->assert_event_mask & (1 << event_state)) &&
        !(obj_cfg->deassert_event_mask & (1 << event_state))) {
        return DMS_EVENT_TYPE_ONE_TIME;
    }

    return DMS_EVENT_TYPE_OCCUR;
}

static bool sensor_need_report_event(struct dms_sensor_object_cb *p_sensor_obj_cb,
    DMS_EVENT_LIST_ITEM *event, unsigned char assertion)
{
    unsigned int debounce_time;

    if (assertion == DMS_EVENT_TYPE_ONE_TIME) {
        return true;
    }

    if (p_sensor_obj_cb->sensor_object_cfg.sensor_class != DMS_DISCRETE_SENSOR_CLASS) {
        return !event->is_report; /* need report(return true) if not reported(is_report==false) */
    }

    debounce_time = p_sensor_obj_cb->sensor_object_cfg.sensor_class_cfg.discrete_sensor.debounce_time;
    if ((assertion == DMS_EVENT_TYPE_OCCUR) && (event->is_report == false) &&
        (event->continued_count >= debounce_time)) {
        return true;
    }

    return false;
}

/* Replace with the event table and add sensor events in batches */
static void sensor_report_diff_event_list(struct dms_sensor_object_cb *p_sensor_obj_cb,
    DMS_EVENT_LIST_ITEM *p_new_list)
{
    unsigned char assertion;
    unsigned int severity;
    unsigned int fault_status = DMS_GEN_SEN_STATUS_GOOD;

    sensor_proc_diff_list(p_sensor_obj_cb, p_new_list);
    while (p_new_list != NULL) {
        int ret;
        severity = DMS_GEN_SEN_STATUS_GOOD;
        ret = dms_get_event_severity(p_sensor_obj_cb->owner_node_type, p_sensor_obj_cb->sensor_object_cfg.sensor_type,
            p_new_list->event_data, &severity);
        if (ret != DRV_ERROR_NONE) {
            dms_err("Get event severity fail. (sensor type=0x%x; offset=%u)\n",
                p_sensor_obj_cb->sensor_object_cfg.sensor_type, p_new_list->event_data);
        }

        assertion = sensor_event_state_convert_assertion(p_sensor_obj_cb, p_new_list->event_data);
        if (sensor_need_report_event(p_sensor_obj_cb, p_new_list, assertion)) {
            (void)dms_sensor_report_event(p_sensor_obj_cb, p_new_list, assertion);
            (void)dms_sensor_get_timestamp(&p_new_list->timestamp);
            p_new_list->is_report = true;
            p_sensor_obj_cb->event_status = severity;
        }

        if ((p_new_list->is_report) != 0) {
            fault_status = fault_status > severity ? fault_status : severity;
        }
        p_new_list = p_new_list->p_next;
    }

    /* Update sensor fault status by most serious event from present reported events */
    p_sensor_obj_cb->fault_status = fault_status;
}

int dms_add_one_sensor_event(struct dms_sensor_object_cb *psensor_obj_cb,
    struct dms_sensor_event_data_item *pevent_data)
{
    int rec;
    DMS_EVENT_LIST_ITEM *p_event = NULL;

    /* Add event to sensor event list */
    p_event = sensor_add_event_to_list(&psensor_obj_cb->p_event_list, pevent_data, psensor_obj_cb);
    if (p_event == NULL) {
        dms_warn("dms_add_one_sensor_event is not add event to list\n");
        return DRV_ERROR_NONE;
    }
    /* Add a new encapsulation function for reporting events */
    rec = dms_sensor_report_event(psensor_obj_cb, p_event, DMS_EVENT_TYPE_OCCUR);
    if (rec != DRV_ERROR_NONE) {
        dms_err("add one sensor event report event fail\n");
        return rec;
    }
    p_event->is_report = true;
    /* After the reported event is successful, update the reported event status in the instance to the fault status */
    psensor_obj_cb->event_status = psensor_obj_cb->fault_status;
    return DRV_ERROR_NONE;
}

int dms_update_sensor_list(struct dms_sensor_object_cb *p_sensor_obj_cb, DMS_EVENT_LIST_ITEM *p_new_list)
{
    DMS_EVENT_LIST_ITEM *p_old_event_list;

    p_old_event_list = p_sensor_obj_cb->p_event_list;

    /* Compare the original event list with the new event list, and report inconsistent events */
    sensor_report_diff_event_list(p_sensor_obj_cb, p_new_list);

    /* Replace with a new event list */
    p_sensor_obj_cb->p_event_list = p_new_list;

    /* Delete the old event list */
    (void)sensor_delete_event_list(p_old_event_list);
    return DRV_ERROR_NONE;
}

int dms_resume_all_sensor_event(struct dms_sensor_object_cb *psensor_obj_cb)
{
    int result;
    DMS_EVENT_LIST_ITEM *p_empty_list = NULL;
    sensor_report_diff_event_list(psensor_obj_cb, p_empty_list);

    result = sensor_delete_event_list(psensor_obj_cb->p_event_list);
    if (result != DRV_ERROR_NONE) {
        dms_err("delete event list fail. (result=%d)\n", result);
        return result;
    }
    psensor_obj_cb->p_event_list = NULL;
    return DRV_ERROR_NONE;
}

STATIC int dms_proc_sensor_data(struct dms_dev_sensor_cb *dev_sensor_cb, struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cb *psensor_obj_cb, struct dms_sensor_event_data *pevent_data)
{
    unsigned short sensor_class;
    unsigned int result = DRV_ERROR_NONE;
    sensor_class = psensor_obj_cb->sensor_object_cfg.sensor_class;
    /* Determine whether the instance detection is enabled, if it is disabled, skip the processing of the instance */
    if (psensor_obj_cb->sensor_object_cfg.enable_flag == DMS_SENSOR_OBJECT_DISABLE_FLAG) {
        /* If the setting flag is forbidden and the previous fault state is fault, you also need to restore the fault
         * state to normal and report the recovery event */
        if (psensor_obj_cb->fault_status != DMS_SENSOR_STATUS_INVALID) {
            /* There are existing events for recovering data */
            result = dms_resume_all_sensor_event(psensor_obj_cb);
            if (result != DRV_ERROR_NONE) {
                dms_err("resume all sensor event fail (result=%u)\n", result);
                return result;
            }
            /* Restore the fault state to normal */
            psensor_obj_cb->fault_status = DMS_SENSOR_STATUS_GOOD;
            /* The reported event status should also be restored to normal */
            psensor_obj_cb->event_status = DMS_SENSOR_STATUS_GOOD;
        }
        /* The sensor is disabled and does not process events */
        return result;
    }
    switch (sensor_class) {
            /* Statistical sensor */
        case DMS_STATICSTIC_SENSOR_CLASS:
            result = dms_process_statis_result_report(node_sensor_cb, psensor_obj_cb, pevent_data);
            if (result != DRV_ERROR_NONE) {
                /* Print error message: Failed to process the detection result of statistical sensor */
                dms_err("Process Statistic Sensor Check Result Failed.\n");
                /* Update statistics */
                dev_sensor_cb->sensor_scan_fail_record.sensor_process_fail++;
                return result;
            }
            break;
            /* General threshold type sensor */
        case DMS_GENERAL_THRESHOLD_SENSOR_CLASS:
            result = dms_process_general_result_report(node_sensor_cb, psensor_obj_cb, pevent_data);
            if (result != DRV_ERROR_NONE) {
                /* Print error message: General threshold sensor detection result processing failed */
                dms_err("Process General Sensor Check Result Failed\n");
                /* Update statistics */
                dev_sensor_cb->sensor_scan_fail_record.sensor_process_fail++;
                return result;
            }
            break;
            /* Discrete sensor */
        case DMS_DISCRETE_SENSOR_CLASS:
            result = dms_process_discrete_result_report(node_sensor_cb, psensor_obj_cb, pevent_data);
            if (result != DRV_ERROR_NONE) {
                /* Print error message: Failed to process the detection result of statistical sensor */
                dms_err("Process Discrete Sensor Check Result Failed\n");
                /* Update statistics */
                dev_sensor_cb->sensor_scan_fail_record.sensor_process_fail++;
                return result;
            }
            break;
        default:
            /* Print error message: sensor type is not supported */
            dms_err("Sensor Class NOT Support. (sensor_class=%u)\n", sensor_class);
            return DRV_ERROR_PARA_ERROR;
    }
    return result;
}

static int dms_check_sensor_event_offset(unsigned char sensor_type, unsigned char event_offset)
{
    int i;
    int count = dms_get_sensor_type_count();
    struct tag_dms_sensor_type *dms_sensor_type = NULL;
    dms_sensor_type = dms_get_sensor_type();
    for (i = 0; i < count; i++) {
        if (dms_sensor_type[i].sensor_type == sensor_type) {
            if (dms_sensor_type[i].sensor_event_count > event_offset) {
                return DRV_ERROR_NONE;
            } else {
                dms_err("event offset error. (event—count=%u,sensor_type=0x%x; event_offset=%u)\n",
                    dms_sensor_type[i].sensor_event_count, sensor_type, event_offset);
                return DRV_ERROR_PARA_ERROR;
            }
        }
    }
    return DRV_ERROR_PARA_ERROR;
}

int dms_check_sensor_data(struct dms_sensor_object_cb *sensor_obj_cb, unsigned char event_data)
{
    return dms_check_sensor_event_offset((unsigned char)sensor_obj_cb->sensor_object_cfg.sensor_type, event_data);
}

STATIC void dms_printf_sensor_class_info(int class_val, const union dms_sensor_union *psensor_class)
{
    switch (class_val) {
            /* Statistical sensor */
        case DMS_STATICSTIC_SENSOR_CLASS:
            print_sysfs("   occur_thres_type: %d\n", psensor_class->statistic_sensor.occur_thres_type);
            print_sysfs("   resume_thres_type: %d\n", psensor_class->statistic_sensor.resume_thres_type);
            print_sysfs("   max_stat_time: %u\n", psensor_class->statistic_sensor.max_stat_time);
            print_sysfs("   min_stat_time: %u\n", psensor_class->statistic_sensor.min_stat_time);
            print_sysfs("   occur_stat_time: %u\n", psensor_class->statistic_sensor.occur_stat_time);
            print_sysfs("   attribute: %u\n", psensor_class->statistic_sensor.attribute);
            print_sysfs("   max_occur_thres: %u\n", psensor_class->statistic_sensor.max_occur_thres);
            print_sysfs("   min_occur_thres: %u\n", psensor_class->statistic_sensor.min_occur_thres);
            print_sysfs("   max_resume_thres: %u\n", psensor_class->statistic_sensor.max_resume_thres);
            print_sysfs("   min_resume_thres: %u\n", psensor_class->statistic_sensor.min_resume_thres);
            print_sysfs("   occur_thres: %u\n", psensor_class->statistic_sensor.occur_thres);
            print_sysfs("   resume_thres: %u\n", psensor_class->statistic_sensor.resume_thres);
            break;
            /* General threshold type sensor */
        case DMS_GENERAL_THRESHOLD_SENSOR_CLASS:
            print_sysfs("   attribute: %u\n", psensor_class->general_sensor.attribute);
            print_sysfs("   thresSeries: %u\n", psensor_class->general_sensor.thres_series);
            print_sysfs("   low_critical: %d\n", psensor_class->general_sensor.low_critical);
            print_sysfs("   low_major: %d\n", psensor_class->general_sensor.low_major);
            print_sysfs("   low_minor: %d\n", psensor_class->general_sensor.low_minor);
            print_sysfs("   up_critical: %d\n", psensor_class->general_sensor.up_critical);
            print_sysfs("   up_major: %d\n", psensor_class->general_sensor.up_major);
            print_sysfs("   up_minor: %d\n", psensor_class->general_sensor.up_minor);
            print_sysfs("   pos_thd_hysteresis: %d\n", psensor_class->general_sensor.pos_thd_hysteresis);
            print_sysfs("   neg_thd_hysteresis: %d\n", psensor_class->general_sensor.neg_thd_hysteresis);
            print_sysfs("   max_thres: %d\n", psensor_class->general_sensor.max_thres);
            print_sysfs("   min_thres: %d\n", psensor_class->general_sensor.min_thres);
            break;
            /* Discrete sensor */
        case DMS_DISCRETE_SENSOR_CLASS:
            print_sysfs("   attribute: %u\n", psensor_class->discrete_sensor.attribute);
            print_sysfs("   debounce_time: %u\n", psensor_class->discrete_sensor.debounce_time);
            break;
        default:
            /* Print error message: sensor type is not supported */
            dms_err("Sensor Class NOT Support. (class_val=0x%x!)\n", (unsigned int)class_val);
            return;
    }
    return;
}

static void dms_printf_sensor_object_info(struct dms_sensor_object_cfg *pobj_cfg)
{
    print_sysfs("   sensor_type: 0x%x\n", pobj_cfg->sensor_type);
    print_sysfs("   sensor_name: %s\n", pobj_cfg->sensor_name);
    print_sysfs("   sensor_class: %d\n", pobj_cfg->sensor_class);
    dms_printf_sensor_class_info(pobj_cfg->sensor_class, &pobj_cfg->sensor_class_cfg);
    print_sysfs("   scan_interval: %u\n", pobj_cfg->scan_interval);
    print_sysfs("   proc_flag: %u\n", pobj_cfg->proc_flag);
    print_sysfs("   enable_flag: %u\n", pobj_cfg->enable_flag);
    print_sysfs("   pf_scan_func: %d\n", pobj_cfg->pf_scan_func != NULL);
    print_sysfs("   assert_event_mask: 0x%x\n", pobj_cfg->assert_event_mask);
    print_sysfs("   deassert_event_mask: 0x%x\n", pobj_cfg->deassert_event_mask);
}

static void dms_exit_sensor_object_cb(struct dms_sensor_object_cb *p_object_cb)
{
    DMS_EVENT_LIST_ITEM *p_event_list = NULL;

    p_event_list = p_object_cb->p_event_list;
    while (p_event_list != NULL) {
        (void)dms_sensor_report_event(p_object_cb, p_event_list, DMS_EVENT_TYPE_RESUME);
        p_event_list = p_event_list->p_next;
    }

    if (p_object_cb->p_event_list != NULL) {
        (void)sensor_delete_event_list(p_object_cb->p_event_list);
        p_object_cb->p_event_list = NULL;
    }
}

static void dms_exit_node_sensor_cb(struct dms_node_sensor_cb *node_sensor_cb)
{
    struct dms_sensor_object_cb *pobject_cb = NULL;
    struct dms_sensor_object_cb *tmp_ctl = NULL;
    list_for_each_entry_safe(pobject_cb, tmp_ctl, &(node_sensor_cb->sensor_object_table), list)
    {
        ka_list_del(&pobject_cb->list);
        dms_exit_sensor_object_cb(pobject_cb);
        dbl_kfree(pobject_cb);
        pobject_cb = NULL;
    }
}

STATIC int dms_check_sensor_type_table(void)
{
    int i;
    int count = dms_get_sensor_type_count();
    struct tag_dms_sensor_type *dms_sensor_type = NULL;
    dms_sensor_type = dms_get_sensor_type();
    for (i = 0; i < count; i++) {
        if (dms_sensor_type[i].sensor_type == DMS_SENSOR_RESERVED_FOR_PRODUCT) {
            dms_err("The sensor type has be reserved for product.(sensor_type=0x%x)\n",
                dms_sensor_type[i].sensor_type);
            return DRV_ERROR_PARA_ERROR;
        }

        if (strlen(dms_sensor_type[i].type_name) > DMS_MAX_SENSOR_TYPE_NAME_SIZE) {
            dms_err("type name too long. (sensor_type=0x%x)\n",
                dms_sensor_type[i].sensor_type);
            return DRV_ERROR_PARA_ERROR;
        }
    }
    return DRV_ERROR_NONE;
}

/* *************************************************************************
Function:        dms_printf_sensor_obj_info
Description: printf
Calls: dms_printf_sensor_object_info
************************************************************************ */
STATIC void dms_printf_sensor_obj_info(struct dms_dev_sensor_cb *dev_sensor_cb)
{
    unsigned int i;

    struct dms_node_sensor_cb *node_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;

    struct dms_sensor_object_cb *sensor_type_item = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;

    /* Sensor list resource lock acquires mutex semaphore */
    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);
    dms_info("node_sensor_cb info. device id: %u\n", dev_sensor_cb->deviceid);
    /* Traverse the list of module nodes */
    list_for_each_entry_safe(node_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list)
    {
        print_sysfs("==================================\n");
        print_sysfs("node_id: %u\n", node_sensor_cb->node_id);
        print_sysfs("node_type: 0x%x\n", node_sensor_cb->node_type);
        if (node_sensor_cb->owner_node != NULL) {
            print_sysfs("node_name: %s\n", node_sensor_cb->owner_node->node_name);
        }
        print_sysfs("pid: %d\n", node_sensor_cb->pid);
        print_sysfs("env_type: %u\n", node_sensor_cb->env_type);
        print_sysfs("version: %u\n", node_sensor_cb->version);
        print_sysfs("sensor_object_num: %u\n", node_sensor_cb->sensor_object_num);

        /* Traverse all sensor types and all sensor instances under each type for detection. If the detection result is
         * an event that needs to be reported, then report the event */
        list_for_each_entry_safe(sensor_type_item, tmp_sensor_ctl, &(node_sensor_cb->sensor_object_table), list)
        {
            print_sysfs(" - object_index: %u\n", sensor_type_item->object_index);
            print_sysfs("   owner_node_id: %u\n", sensor_type_item->owner_node_id);
            dms_printf_sensor_object_info(&sensor_type_item->sensor_object_cfg);
            print_sysfs("   current_status: %d\n", sensor_type_item->current_value);
            print_sysfs("   remain_time: %u\n", sensor_type_item->remain_time);
            print_sysfs("   event_status: %u\n", sensor_type_item->event_status);
            print_sysfs("   fault_status: %u\n", sensor_type_item->fault_status);

            print_sysfs("   event_paras: ");
            for (i = 0; i < sensor_type_item->paras_len; i++) {
                print_sysfs(" %x", sensor_type_item->event_paras[i]);
            }
            if (sensor_type_item->sensor_object_cfg.sensor_class != DMS_STATICSTIC_SENSOR_CLASS) {
                continue;
            }
            print_sysfs("\n");

            print_sysfs("   object_op_state_ch_time: Y:%u M:%u\n",
                sensor_type_item->class_cb.statistic_cb.object_op_state_ch_time.year,
                sensor_type_item->class_cb.statistic_cb.object_op_state_ch_time.month);
            print_sysfs("   object_op_state_chg_cause: %u\n",
                sensor_type_item->class_cb.statistic_cb.object_op_state_chg_cause);
            print_sysfs("   alarm_clear_times: %u\n", sensor_type_item->class_cb.statistic_cb.alarm_clear_times);
            print_sysfs("   status_counter: %u\n", sensor_type_item->class_cb.statistic_cb.status_counter);
            print_sysfs("   stat_time_counter: %u\n", sensor_type_item->class_cb.statistic_cb.stat_time_counter);
            print_sysfs("   current_bit_count: %u\n", sensor_type_item->class_cb.statistic_cb.current_bit_count);
            print_sysfs("   stat_event_paras:");
            for (i = 0; i < sensor_type_item->paras_len; i++) {
                print_sysfs(" %x", sensor_type_item->class_cb.statistic_cb.stat_event_paras[i]);
            }
            print_sysfs("\n");
        }
    }
    /* Sensor list resource lock acquires mutex semaphore */
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
}

/* *************************************************************************
Function:        dms_printf_sensor_time_recorder
Description:  printf sensor time

************************************************************************ */
void dms_printf_sensor_time_recorder(struct dms_dev_sensor_cb *pdev_sen_cb)
{
    unsigned int i;
    struct dms_sensor_scan_time_recorder *ptime_recorder;
    ptime_recorder = &pdev_sen_cb->scan_time_recorder;
    dms_info("\n sensor_time_recorder deviceid: %d", pdev_sen_cb->deviceid);
    dms_info("\n record_scan_time_flag: %u", ptime_recorder->record_scan_time_flag);
    dms_info("\n sensor_out_time_count: %u", ptime_recorder->sensor_out_time_count);
    dms_info("\n sensor_record_index: %u", ptime_recorder->sensor_record_index);
    dms_info("\n sensor_max_exec_time: %lu", ptime_recorder->max_sensor_scan_record.exec_time);
    dms_info("\n sensor_max_exec_sensor: %.*s", DMS_SENSOR_DESCRIPT_LENGTH,
        ptime_recorder->max_sensor_scan_record.sensor_name);
    for (i = 0; i < ptime_recorder->sensor_record_index; i++) {
        dms_info("\n sensor: name: %.*s", DMS_SENSOR_DESCRIPT_LENGTH,
            ptime_recorder->sensor_scan_time_record[i].sensor_name);
        dms_info("\n sensor:exec_time: %lu", ptime_recorder->sensor_scan_time_record[i].exec_time);
    }
    dms_info("\n dev:dev_out_time_count: %u", ptime_recorder->dev_out_time_count);
    dms_info("\n dev:max_dev_scan_record: %lu", ptime_recorder->max_dev_scan_record);
    dms_info("\n dev:dev_record_index: %u", ptime_recorder->dev_record_index);
    for (i = 0; i < ptime_recorder->dev_record_index; i++) {
        dms_info("\n dev:dev_scan_time_record: %lu", ptime_recorder->dev_scan_time_record[i]);
    }
}

ssize_t dms_sensor_print_sensor_list(char *buf)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    ssize_t buf_ret = 0;

    if (buf == NULL) {
        buf_ret += strcpy_s(buf, KA_MM_PAGE_SIZE, "buffer is null\n");
        goto _out;
    }

    dev_cb = dms_get_dev_cb(0);
    if (dev_cb == NULL) {
        buf_ret += strcpy_s(buf, KA_MM_PAGE_SIZE, "device cb is null\n");
        goto _out;
    }

    /* print the sensor info to kernel logs */
    ka_task_mutex_lock(&dev_cb->node_lock);
    dms_printf_sensor_obj_info(&dev_cb->dev_sensor_cb);
    ka_task_mutex_unlock(&dev_cb->node_lock);

    buf_ret += strcpy_s(buf, KA_MM_PAGE_SIZE,
        "the sensor info has saved to the kernel log.\n");

_out:
    return buf_ret;
}
KA_EXPORT_SYMBOL(dms_sensor_print_sensor_list);

static int dms_get_event_def_severity(unsigned char sensor_type, unsigned char event_offset, unsigned int *severity)
{
    int i;
    int count = dms_get_sensor_type_count();
    struct tag_dms_sensor_type *dms_sensor_type = NULL;
    dms_sensor_type = dms_get_sensor_type();
    for (i = 0; i < count; i++) {
        if (dms_sensor_type[i].sensor_type == sensor_type) {
            if (dms_sensor_type[i].sensor_event_count > event_offset) {
                *severity = (unsigned int)dms_sensor_type[i].sensor_event[event_offset].severity;
                return DRV_ERROR_NONE;
            } else {
                dms_err("event offset error. (sensor_type=0x%x; offset=%u)\n", sensor_type,
                    event_offset);
                return DRV_ERROR_PARA_ERROR;
            }
        } else if (sensor_type < dms_sensor_type[i].sensor_type) {
            dms_err("sensor type error. (sensor_type=0x%x; offset=%u)", sensor_type, event_offset);
            return DRV_ERROR_PARA_ERROR;
        }
    }
    return DRV_ERROR_PARA_ERROR;
}

static u32 dms_gen_event_id(u32 node_type, u8 sensor_type, u8 error_type)
{
    u32 fault_id = 0;

#ifdef CFG_HOST_ENV
    fault_id |= FAULT_EVENT_CODE_HOST;
#else
    fault_id |= FAULT_EVENT_CODE_DEVICE;
#endif
    fault_id |= FAULT_EVENT_NODETYPE_TO_CODE(node_type);
    fault_id |= (sensor_type << FAULT_SENSOR_OFFSET);
    fault_id |= (u32)error_type;
    return fault_id;
}

STATIC int get_event_severity_from_config(u32 event_code, unsigned int *severity)
{
    int left = 0;
    int right = g_event_configs.config_cnt - 1;

    while (left <= right) {
        int mid = (right + left) / 2;
        if (g_event_configs.event_configs[mid].event_code == event_code) {
            *severity = g_event_configs.event_configs[mid].severity;
            return 0;
        } else if (g_event_configs.event_configs[mid].event_code < event_code) {
            left = mid + 1;
        } else if (g_event_configs.event_configs[mid].event_code > event_code) {
            right = mid - 1;
        }
    }

#ifndef CFG_SUPPORT_DEFAULT_SEVERITY
    /* for DC, use low severity while not config the event_code severity in dms_events_conf.lst */
    *severity = DMS_EVENT_MINOR;
    return 0;
#else
    return EEXIST;
#endif
}

int dms_get_event_severity(unsigned int node_type, unsigned char sensor_type, unsigned char event_offset,
    unsigned int *severity)
{
    int ret;
    u32 event_code;

    event_code = dms_gen_event_id(node_type, (u8)sensor_type, (u8)event_offset);

    ret = get_event_severity_from_config(event_code, severity);
    if (ret == 0) {
        dms_debug("get_event_severity_from_config success. (event_code=0x%x; *severity=%u)\n", event_code, *severity);
        return 0;
    }

    return dms_get_event_def_severity(sensor_type, event_offset, severity);
}

KA_EXPORT_SYMBOL_GPL(dms_get_event_severity);

/* *************************************************************************
Function:        int dms_get_event_string
Description:    Print all sensor information
Calls:  dms_get_sensor_type_count
************************************************************************ */
int dms_get_event_string(unsigned char sensor_type, unsigned char event_offset, char *event_str, int inbuf_size)
{
    int i, ret;
    int count = dms_get_sensor_type_count();
    struct tag_dms_sensor_type *dms_sensor_type = NULL;
    dms_sensor_type = dms_get_sensor_type();
    for (i = 0; i < count; i++) {
        if (dms_sensor_type[i].sensor_type == sensor_type) {
            if (dms_sensor_type[i].sensor_event_count > event_offset) {
                break;
            } else {
                dms_err("event offset error. (sensor_type=0x%x; offset=%u)\n", sensor_type,
                    event_offset);
                return DRV_ERROR_PARA_ERROR;
            }
        } else if (sensor_type < dms_sensor_type[i].sensor_type) {
            dms_err("sensor type error. (sensor_type=0x%x; offset=%u)\n", sensor_type, event_offset);
            return DRV_ERROR_PARA_ERROR;
        }
    }
    if (i != count) {
        ret = strcpy_s(event_str, inbuf_size, dms_sensor_type[i].sensor_event[event_offset].eventstring);
        if (ret != 0) {
            dms_err("strcpy_s error. (ret=%d)\n", ret);
            return DRV_ERROR_PARA_ERROR;
        }
        return DRV_ERROR_NONE;
    }
    return DRV_ERROR_PARA_ERROR;
}

/* *************************************************************************
Function:        int dms_get_event_string
Description:    Print all sensor

************************************************************************ */
int dms_get_sensor_type_name(unsigned char sensor_type, char *type_name, int inbuf_size)
{
    int i, ret;
    int count = dms_get_sensor_type_count();
    struct tag_dms_sensor_type *dms_sensor_type = NULL;
    dms_sensor_type = dms_get_sensor_type();
    for (i = 0; i < count; i++) {
        if (dms_sensor_type[i].sensor_type == sensor_type) {
            ret = strcpy_s(type_name, inbuf_size, dms_sensor_type[i].type_name);
            if (ret != 0) {
                dms_err("strcpy_s error. (ret=%d)\n", ret);
                return DRV_ERROR_PARA_ERROR;
            }
            return DRV_ERROR_NONE;
        } else if (sensor_type < dms_sensor_type[i].sensor_type) {
            dms_err("sensor type error. (sensor_type=0x%x)\n", sensor_type);
            return DRV_ERROR_PARA_ERROR;
        }
    }
    dms_err("sensor type error. (sensor type=0x%x)\n", sensor_type);
    return DRV_ERROR_PARA_ERROR;
}

int dms_sensor_scan_one_node_object(struct dms_dev_sensor_cb *dev_sensor_cb,
    struct dms_node_sensor_cb *node_sensor_cb, struct dms_sensor_object_cb *psensor_obj_cb,
    struct dms_sensor_scan_time_recorder *ptime_recorder)
{
    int result;
    struct dms_sensor_event_data event_data = {0};

    dev_sensor_cb->sensor_scan_fail_record.current_scan_func =
        (void *)psensor_obj_cb->sensor_object_cfg.pf_scan_func;
    dev_sensor_cb->sensor_scan_fail_record.node_id = node_sensor_cb->node_id;
    dev_sensor_cb->sensor_scan_fail_record.node_type = node_sensor_cb->node_type;
    /* Determine whether to enable detection */
    if (psensor_obj_cb->sensor_object_cfg.enable_flag == DMS_SENSOR_DISABLE_FALG) {
        return 0;
    }
    /* Determine whether the detection time is reached */
    if (psensor_obj_cb->remain_time > DMS_SENSOR_CHECK_INTERVAL_TIME) {
        /* If the detection time has not arrived, update the remaining time in the table */
        psensor_obj_cb->remain_time -= DMS_SENSOR_CHECK_INTERVAL_TIME;
        /* Then exit the processing of this entry */
        return 0;
    }
    /* The remaining time is set to the initial detection cycle */
    psensor_obj_cb->remain_time = psensor_obj_cb->sensor_object_cfg.scan_interval;
    /* Determine whether it is necessary to record the detection time */
    if (ptime_recorder->record_scan_time_flag == DMS_SENSOR_CHECK_RECORD) {
        /* Record scan task time */
        ptime_recorder->start_sensor_scan_record(dev_sensor_cb);
    }

    /* Call the detection function to complete the detection */
    result =
        psensor_obj_cb->sensor_object_cfg.pf_scan_func(psensor_obj_cb->sensor_object_cfg.private_data, &event_data);
    /* If an error occurs during the detection process */
    if (result != DRV_ERROR_NONE) {
        /* Print error message: detect function execution error */
        dms_err_ratelimited("call function fail. (node_type=0x%x; node_id=%u; ret=%d)\n",
            node_sensor_cb->node_type, node_sensor_cb->node_id, result);
        /* Determine whether it is necessary to record the detection time */
        if (ptime_recorder->record_scan_time_flag == DMS_SENSOR_CHECK_RECORD) {
            /* Record scan task time */
            ptime_recorder->stop_sensor_scan_record(dev_sensor_cb, psensor_obj_cb);
        }
        /* Update statistics */
        dev_sensor_cb->sensor_scan_fail_record.call_scan_func_fail++;
        return result;
    }

    /* Determine whether it is necessary to record the detection time */
    if (ptime_recorder->record_scan_time_flag == DMS_SENSOR_CHECK_RECORD) {
        /* Record scan task time */
        ptime_recorder->stop_sensor_scan_record(dev_sensor_cb, psensor_obj_cb);
    }
    /* There is a problem with the returned data */
    if (event_data.event_count > DMS_MAX_SENSOR_EVENT_COUNT) {
        dms_err_ratelimited("pf_scan_func return error. (sensor_name=%.*s; status=%d)\n",
            DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cb->sensor_object_cfg.sensor_name, event_data.event_count);
        dev_sensor_cb->sensor_scan_fail_record.scan_func_date_error++;
        return -EINVAL;
    }
    /* Do you need to perform alarm processing after this test */
    if (psensor_obj_cb->sensor_object_cfg.proc_flag != DMS_SENSOR_PROC_ENABLE_FLAG) {
        return 0;
    }
    /* Process all sensor instances in the information table */
    if (dms_proc_sensor_data(dev_sensor_cb, node_sensor_cb, psensor_obj_cb, &event_data) != DRV_ERROR_NONE) {
        dms_err_ratelimited("proc sensor data return error. (sensor_name=%.*s)\n",
            DMS_SENSOR_DESCRIPT_LENGTH, psensor_obj_cb->sensor_object_cfg.sensor_name);
        return -EINVAL;
    }
    return 0;
}

static void dms_sensor_scan_node_sensor(struct dms_dev_sensor_cb *dev_sensor_cb,
    struct dms_node_sensor_cb *node_sensor_cb)
{
    struct dms_sensor_object_cb *psensor_obj_cb = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;
    unsigned short node_health = 0;
    int result;
    struct dms_sensor_scan_time_recorder *ptime_recorder;

    ptime_recorder = &dev_sensor_cb->scan_time_recorder;
    list_for_each_entry_safe(psensor_obj_cb, tmp_sensor_ctl, &(node_sensor_cb->sensor_object_table), list) {
        dms_sensor_notify_event_proc(DMS_SERSOR_SCAN_PERIOD); /* quickly process all sensor notify */
        if (psensor_obj_cb->sensor_object_cfg.pf_scan_func == NULL) {
            dev_sensor_cb->sensor_scan_fail_record.null_scan_func_fail++;
            dms_err_ratelimited("scan func Pointer is NULL\n");
            continue;
        }

        result = dms_sensor_scan_one_node_object(dev_sensor_cb, node_sensor_cb,
            psensor_obj_cb, ptime_recorder);
        if (result != 0) {
            dms_err_ratelimited("Scan one node object failed.(ret=%d)\n", result);
        }

        /* update node health */
        node_health = node_health > psensor_obj_cb->fault_status ? node_health : psensor_obj_cb->fault_status;
    }

    node_sensor_cb->health = node_health;
#if (defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)) && (!defined(DRV_SOC_MISC_UT))
    node_sensor_cb->owner_node->state = node_sensor_cb->health;
#endif
}

/* *************************************************************************
Function:        void dms_sensor_scan_proc(struct dms_dev_sensor_cb *dev_sensor_cb)
Description:     Sensor timing detection, processing the sensors of all nodes under each device

************************************************************************ */
void dms_sensor_scan_proc(struct dms_dev_sensor_cb *dev_sensor_cb)
{
    struct dms_node_sensor_cb *node_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;

    struct dms_sensor_scan_time_recorder *ptime_recorder;
    unsigned short dev_health;
    ptime_recorder = &dev_sensor_cb->scan_time_recorder;
    /* Determine whether it is necessary to record the detection time */
    if (ptime_recorder->record_scan_time_flag == DMS_SENSOR_CHECK_RECORD) {
        /* Record scan task time */
        ptime_recorder->start_dev_scan_record(dev_sensor_cb);
    }
    /* Sensor list resource lock acquires mutex semaphore */
    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);
    dev_health = 0;
    /* Traverse the list of module nodes */
    list_for_each_entry_safe(node_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list)
    {
        if (node_sensor_cb->sensor_object_num == 0) {
            dms_warn("dms_sensor_scan_task: not sensor type and obj! (nodetype=0x%x, nodeid = 0x%x)",
                node_sensor_cb->node_type, node_sensor_cb->node_id);
            dev_sensor_cb->sensor_scan_fail_record.get_data_from_node_fail++;
            continue;
        }
        /* From user mode, but the process PID is invalid */
        if ((node_sensor_cb->env_type == DMS_SENSOR_ENV_USER_SPACE) && (node_sensor_cb->pid <= 0)) {
            dms_err_ratelimited("env error. (nodeid=0x%x; pid=%d)\n", node_sensor_cb->env_type,
                node_sensor_cb->pid);
            dev_sensor_cb->sensor_scan_fail_record.get_data_from_node_fail++;
            continue;
        }
        /* Traverse all sensor types and all sensor instances under each type for detection. If the detection result is
         * an event that needs to be reported, then report the event */
        dms_sensor_scan_node_sensor(dev_sensor_cb, node_sensor_cb);
        dev_health = dev_health > node_sensor_cb->health ? dev_health : node_sensor_cb->health;
    }
    dev_sensor_cb->health = dev_health;
    /* Release the mutex semaphore */
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
    /* Determine whether it is necessary to record the detection time */
    if (ptime_recorder->record_scan_time_flag == DMS_SENSOR_CHECK_RECORD) {
        /* Record scan task time */
        ptime_recorder->stop_dev_scan_record(dev_sensor_cb);
    }
}

STATIC int dms_sensor_get_one_node_health_events(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_event_obj *event_buff, unsigned int input_count, unsigned int *output_count)
{
    unsigned int temp_out_count = *output_count;
    unsigned int assert_mask, deassert_mask;
    struct dms_sensor_object_cb *psensor_obj_cb = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;
    DMS_EVENT_LIST_ITEM *p_temp_event_list = NULL;

    if (node_sensor_cb->sensor_object_num == 0) {
        dms_warn("dms_sensor_scan_task: not sensor type and obj! nodeid = %u", node_sensor_cb->node_id);
        return 0;
    }
    /* From user mode, but the process PID is invalid */
    if ((node_sensor_cb->env_type == DMS_SENSOR_ENV_USER_SPACE) && (node_sensor_cb->pid <= 0)) {
        dms_warn("env warn! (nodeid = %u, pid:%d)\n", node_sensor_cb->env_type, node_sensor_cb->pid);
        return 0;
    }
    /* Traverse all sensor types and all sensor instances under each type for detection. If the detection result is
        * an event that needs to be reported, then report the event */
    list_for_each_entry_safe(psensor_obj_cb, tmp_sensor_ctl, &(node_sensor_cb->sensor_object_table), list) {
        p_temp_event_list = psensor_obj_cb->p_event_list;
        if (p_temp_event_list == NULL) {
            continue;
        }
        /* Traverse the list of all events and get the events */
        while (p_temp_event_list != NULL) {
            if (temp_out_count >= input_count) {
                return -EINVAL;
            }
            assert_mask = psensor_obj_cb->sensor_object_cfg.assert_event_mask;
            deassert_mask = psensor_obj_cb->sensor_object_cfg.deassert_event_mask;
            if (dms_sensor_check_mask_enable(assert_mask, p_temp_event_list->event_data) &&
                dms_sensor_check_mask_enable(deassert_mask, p_temp_event_list->event_data)) {
                (void)dms_fill_event_data(psensor_obj_cb, p_temp_event_list,
                                    DMS_EVENT_TYPE_OCCUR, &event_buff[temp_out_count]);
                temp_out_count++;
            }
            p_temp_event_list = p_temp_event_list->p_next;
        }
    }

    *output_count = temp_out_count;
    return 0;
}

/* *************************************************************************
Function:        int dms_sensor_get_health_events
Description:     Get all events of the sensor events
************************************************************************ */
int dms_sensor_get_health_events(struct dms_dev_sensor_cb *dev_sensor_cb, struct dms_event_obj *event_buff,
    unsigned int input_count, unsigned int *output_count)
{
    int ret;
    unsigned int temp_out_count = 0;
    struct dms_node_sensor_cb *node_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;

    if ((dev_sensor_cb == NULL) || (event_buff == NULL) || (output_count == NULL)) {
        return DRV_ERROR_PARA_ERROR;
    }
    /* Sensor list resource lock acquires mutex semaphore */
    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);

    /* Traverse the list of module nodes */
    list_for_each_entry_safe(node_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list) {
        ret = dms_sensor_get_one_node_health_events(node_sensor_cb, event_buff, input_count, &temp_out_count);
        if (ret != 0) {
            goto sensor_exit;
        }
    }
sensor_exit:
    /* Release the mutex semaphore */
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
    *output_count = temp_out_count;
    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_sensor_get_health_events);

STATIC void dms_sensor_clean_report_event_list(struct dms_sensor_object_cb *psensor_obj_cb)
{
    int ret;
    if (psensor_obj_cb->sensor_object_cfg.pf_clear_event_func == NULL) {
        dms_debug("pf_clear_event_func is NULL, report module not registered. (node_type=0x%x; node_id=%u; "
                  "private_data=0x%llx)\n",
            psensor_obj_cb->owner_node_type,
            psensor_obj_cb->owner_node_id,
            psensor_obj_cb->sensor_object_cfg.private_data);
        return;
    }

    ret = psensor_obj_cb->sensor_object_cfg.pf_clear_event_func(psensor_obj_cb->sensor_object_cfg.private_data);
    if (ret != 0) {
        dms_warn("clean report event list not OK. (node_type=0x%x; node_id=%u; private_data=0x%llx; ret=%d)\n",
            psensor_obj_cb->owner_node_type,
            psensor_obj_cb->owner_node_id,
            psensor_obj_cb->sensor_object_cfg.private_data,
            ret);
    }
    return;
}

int dms_sensor_clean_health_events(struct dms_dev_sensor_cb *dev_sensor_cb)
{
    struct dms_node_sensor_cb *node_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;
    struct dms_sensor_object_cb *psensor_obj_cb = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;

    if (dev_sensor_cb == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }

    /* Sensor list resource lock acquires mutex semaphore */
    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);

    /* Traverse the list of module nodes */
    list_for_each_entry_safe(node_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list)
    {
        if (node_sensor_cb->sensor_object_num == 0) {
            dms_warn("dms_sensor_scan_task: not sensor type and obj! nodeid = %u", node_sensor_cb->node_id);
            continue;
        }
        /* From user mode, but the process PID is invalid */
        if ((node_sensor_cb->env_type == DMS_SENSOR_ENV_USER_SPACE) && (node_sensor_cb->pid <= 0)) {
            dms_err("env error. (nodeid=%u; pid=%d)\n", node_sensor_cb->env_type,
                node_sensor_cb->pid);
            continue;
        }
        /* Traverse all sensor types and all sensor instances under each type for detection. If the detection result is
         * an event that needs to be reported, then report the event */
        list_for_each_entry_safe(psensor_obj_cb, tmp_sensor_ctl, &(node_sensor_cb->sensor_object_table), list)
        {
            /* if the sensor has fault event */
            if (psensor_obj_cb->p_event_list != NULL) {
                /* clean device report event list */
                dms_sensor_clean_report_event_list(psensor_obj_cb);
                /* clean device local event list */
                (void)sensor_delete_event_list(psensor_obj_cb->p_event_list);
                psensor_obj_cb->p_event_list = NULL;
                /* set sensor object is health */
                psensor_obj_cb->fault_status = DMS_SENSOR_STATUS_GOOD;
            }
        }
        /* set node object is health */
        node_sensor_cb->health = DMS_SENSOR_STATUS_GOOD;
#if (defined(CFG_SOC_PLATFORM_MDC_V51) || defined(CFG_SOC_PLATFORM_MDC_V11)) && (!defined(DRV_SOC_MISC_UT))
        node_sensor_cb->owner_node->state = node_sensor_cb->health;
#endif
    }
    /* set dev is health */
    dev_sensor_cb->health = DMS_SENSOR_STATUS_GOOD;
    /* Release the mutex semaphore */
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
    return DRV_ERROR_NONE;
}

int dms_sensor_mask_events(struct dms_dev_sensor_cb *dev_sensor_cb, u8 mask,
    u16 node_type, u8 sensor_type, u8 event_state)
{
    struct dms_node_sensor_cb *node_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;
    struct dms_sensor_object_cb *psensor_obj_cb = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;

    if (dev_sensor_cb == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);
    /* Traverse the list of module nodes */
    list_for_each_entry_safe(node_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list) {
        if ((node_sensor_cb->node_type != node_type) || (node_sensor_cb->sensor_object_num == 0)) {
            continue;
        }
        list_for_each_entry_safe(psensor_obj_cb, tmp_sensor_ctl, &(node_sensor_cb->sensor_object_table), list) {
            if (psensor_obj_cb->sensor_object_cfg.sensor_type !=  sensor_type) {
                continue;
            }
            /* event_mask bit -> 0:disable 1:enable */
            psensor_obj_cb->sensor_object_cfg.deassert_event_mask &= ~(1U << event_state);
            psensor_obj_cb->sensor_object_cfg.assert_event_mask &= ~(1U << event_state);
            if (mask == 0) { /* mask -> 0:enable 1:disable */
                psensor_obj_cb->sensor_object_cfg.deassert_event_mask |=
                    ((1U << event_state) & psensor_obj_cb->orig_obj_cfg.deassert_event_mask);
                psensor_obj_cb->sensor_object_cfg.assert_event_mask |=
                    ((1U << event_state) & psensor_obj_cb->orig_obj_cfg.assert_event_mask);
            }
        }
    }
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);

    return DRV_ERROR_NONE;
}

int dms_sensor_get_dev_health(struct dms_dev_sensor_cb *dev_sensor_cb)
{
    return dev_sensor_cb->health;
}

/* *************************************************************************
Function:       unsigned int dms_sensor_register(struct dms_dev_node *owner_node, struct dms_sensor_type *psensor)
Description:     Kernel registration sensor information table

Data Accessed:   gpDmsSensorQueryTable
Data Updated:    gpDmsSensorQueryTable
 ************************************************************************ */
unsigned int dms_sensor_register(struct dms_node *owner_node, struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    return (dms_sensor_register_all(owner_node, psensor_obj_cfg, DMS_SENSOR_ENV_KERNEL_SPACE));
}
KA_EXPORT_SYMBOL_GPL(dms_sensor_register);

/* *************************************************************************
Function:       unsigned int dms_sensor_register(struct dms_dev_node *owner_node, struct dms_sensor_type *psensor)
Description:     User mode registration sensor interface

Data Accessed:   gpDmsSensorQueryTable
Data Updated:    gpDmsSensorQueryTable
 ************************************************************************ */
unsigned int dms_sensor_register_for_userspace(struct dms_node *owner_node,
    struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    return dms_sensor_register_all(owner_node, psensor_obj_cfg, DMS_SENSOR_ENV_USER_SPACE);
}
KA_EXPORT_SYMBOL(dms_sensor_register_for_userspace);

/* *************************************************************************
Function:    unsigned int dms_sensor_node_unregister(struct dms_node *owner_node)
Description: unregister sensor object
************************************************************************ */
unsigned int dms_sensor_object_unregister(struct dms_node *owner_node, struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    struct dms_dev_sensor_cb *dev_sensor_cb = NULL;
    struct dms_sensor_object_cb *sensor_type_item = NULL;
    struct dms_sensor_object_cb *tmp_sensor_ctl = NULL;
    struct dms_node_sensor_cb *pnode_sen_cb = NULL;
    int rec;
    dev_sensor_cb = dms_get_sensor_cb(owner_node);
    if (dev_sensor_cb == NULL) {
        /* Print error message: the format check of the message table failed */
        dms_err("dms get sensor cb fail. (nodeid=%d)\n", owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }
    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);

    rec = dms_get_node_sensor_cb_by_nodeid(dev_sensor_cb, owner_node->node_type, owner_node->node_id, &pnode_sen_cb);
    if (rec != DRV_ERROR_NONE) {
        dms_err("get node sensor cb fail. (nodeid=%d, nodetype=%d)\n", owner_node->node_type, owner_node->node_id);
        ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
        return rec;
    }
    /* Traverse all sensor types and all sensor instances under each type for detection. If the detection result is an
     * event that needs to be reported, then report the event */
    if (pnode_sen_cb != NULL) {
        list_for_each_entry_safe(sensor_type_item, tmp_sensor_ctl, &(pnode_sen_cb->sensor_object_table), list)
        {
            if (dms_compare_sensor_object(&sensor_type_item->sensor_object_cfg, psensor_obj_cfg) == true) {
                ka_list_del(&sensor_type_item->list);
                dms_exit_sensor_object_cb(sensor_type_item);
                dbl_kfree(sensor_type_item);
                sensor_type_item = NULL;
                pnode_sen_cb->sensor_object_num -= (pnode_sen_cb->sensor_object_num >= 1) ? 1 : 0;
                ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
                return DRV_ERROR_NONE;
            }
        }
    }
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_sensor_object_unregister);

/* *************************************************************************
Function:    unsigned int dms_sensor_node_unregister(struct dms_node *owner_node)
Description: unregister sensor node
************************************************************************ */
unsigned int dms_sensor_node_unregister(struct dms_node *owner_node)
{
    struct dms_dev_sensor_cb *dev_sensor_cb = NULL;
    struct dms_node_sensor_cb *pnode_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;

    if (owner_node == NULL) {
        dms_err("owner_node is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    dev_sensor_cb = dms_get_sensor_cb(owner_node);
    if (dev_sensor_cb == NULL) {
        /* Print error message: the format check of the message table failed */
        dms_err("dms get sensor cb fail. (nodeid=%d)\n", owner_node->node_id);
        return DRV_ERROR_PARA_ERROR;
    }
    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);

    list_for_each_entry_safe(pnode_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list)
    {
        if ((pnode_sensor_cb->node_id == (unsigned int)(owner_node->node_id)) &&
            (pnode_sensor_cb->node_type == owner_node->node_type)) {
            ka_list_del(&pnode_sensor_cb->list);
            dms_exit_node_sensor_cb(pnode_sensor_cb);
            dbl_kfree(pnode_sensor_cb);
            pnode_sensor_cb = NULL;
            ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
            return DRV_ERROR_NONE;
        }
    }
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL_GPL(dms_sensor_node_unregister);

/* *************************************************************************
Function:        unsigned int dms_init_sensor(void)
Description:     sensor Processing module initialization
************************************************************************ */
unsigned int dms_init_dev_sensor_cb(int deviceid, struct dms_dev_sensor_cb *sensor_cb)
{
    if (sensor_cb == NULL) {
        dms_err("info input fail\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_init(&sensor_cb->dms_sensor_mutex);
    INIT_LIST_HEAD(&sensor_cb->dms_node_sensor_cb_list);
    sensor_cb->node_cb_num = 0;
    sensor_cb->deviceid = deviceid;
    sensor_cb->health = 0;

    (void)memset_s((void *)&(sensor_cb->sensor_scan_fail_record), sizeof(struct dms_sensor_scan_fail_record), 0,
        sizeof(struct dms_sensor_scan_fail_record));

    dms_init_sensor_time_recorder(sensor_cb);
    return DRV_ERROR_NONE;
}

/* *************************************************************************
Function:        unsigned int dms_init_sensor(void)
************************************************************************ */
unsigned int dms_sen_init_sensor(void)
{
    u32 i;
    dms_info("dms init sensor module\n");

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        struct dms_dev_ctrl_block* dev_cb = dms_get_dev_cb(i);
        if (dev_cb == NULL) {
            dms_info("the device is not initialized.(dev id=%u)\n", i);
            continue;
        }
        (void)dms_init_dev_sensor_cb(i, &dev_cb->dev_sensor_cb);
    }

    sensor_init_event_list_cb();
    if (dms_check_sensor_type_table() != DRV_ERROR_NONE) {
        dms_err("dms_check_sensor_type_table fail\n");
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}
unsigned int dms_sen_exit_sensor_event(void)
{
    u32 i;
    dms_info("dms exit sensor module\n");

    sensor_exit_event_list_cb();

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        struct dms_dev_ctrl_block* dev_cb = dms_get_dev_cb(i);
        if (dev_cb == NULL) {
            dms_info("the device is not initialized.(dev id=%u)\n", i);
            continue;
        }
        dms_exit_dev_sensor_cb(&dev_cb->dev_sensor_cb);
    }

    return DRV_ERROR_NONE;
}
/* *************************************************************************
Function:        void dms_exit_dev_sensor_cb(struct dms_dev_sensor_cb *dev_sensor_cb)
Description:     Clean up the sensor module
************************************************************************ */
void dms_exit_dev_sensor_cb(struct dms_dev_sensor_cb *dev_sensor_cb)
{
    struct dms_node_sensor_cb *pnode_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;

    ka_task_mutex_lock(&dev_sensor_cb->dms_sensor_mutex);
    list_for_each_entry_safe(pnode_sensor_cb, tmp_ctl, &(dev_sensor_cb->dms_node_sensor_cb_list), list)
    {
        ka_list_del(&pnode_sensor_cb->list);
        dms_exit_node_sensor_cb(pnode_sensor_cb);
        dbl_kfree(pnode_sensor_cb);
        pnode_sensor_cb = NULL;
    }
    ka_task_mutex_unlock(&dev_sensor_cb->dms_sensor_mutex);
    dev_sensor_cb->node_cb_num = 0;
    ka_task_mutex_destroy(&dev_sensor_cb->dms_sensor_mutex);
}

void dms_sensor_release(int owner_pid)
{
    int i;
    struct dms_dev_ctrl_block *cb = NULL;
    struct dms_node_sensor_cb *pnode_sensor_cb = NULL;
    struct dms_node_sensor_cb *tmp_ctl = NULL;

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        cb = dms_get_dev_cb(i);
        if (cb == NULL) {
            continue;
        }
        ka_task_mutex_lock(&cb->dev_sensor_cb.dms_sensor_mutex);
        list_for_each_entry_safe(pnode_sensor_cb, tmp_ctl, &(cb->dev_sensor_cb.dms_node_sensor_cb_list), list)
        {
            if ((pnode_sensor_cb->env_type == DMS_SENSOR_ENV_USER_SPACE) && (pnode_sensor_cb->pid == owner_pid)) {
                dms_info("release sensor. (node_type=0x%x; owner_pid=%d)\n", pnode_sensor_cb->node_type, owner_pid);
                ka_list_del(&pnode_sensor_cb->list);
                dms_exit_node_sensor_cb(pnode_sensor_cb);
                dbl_kfree(pnode_sensor_cb);
                pnode_sensor_cb = NULL;
            }
        }
        ka_task_mutex_unlock(&cb->dev_sensor_cb.dms_sensor_mutex);
    }

    return;
}
KA_EXPORT_SYMBOL(dms_sensor_release);

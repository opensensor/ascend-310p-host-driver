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

#include <linux/errno.h>
#include <linux/slab.h>

#include "ka_base_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_list_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "dms_define.h"
#include "dms_event_distribute.h"
#include "dms_event_converge.h"
#include "dms_event_distribute_proc.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_dev_node.h"
#include "dms_event_dfx.h"
#ifndef DMS_DEVICE_ST
#include "dms_converge_config.h"
#else
#include "dms_converge_config_st.h"
#endif

#ifndef BBOX_ERRSTR_LEN
#define BBOX_ERRSTR_LEN (48)
#endif
#define DMS_EVENT_NAME_BUFF_LEN 128

#define EVENT_ID_TO_NODE_TYPE(event_id) ((event_id) >> 17)
#define EVENT_ID_TO_HASH_KEY(event_id) (EVENT_ID_TO_NODE_TYPE(event_id) & EVENT_ID_HASH_TABLE_MASK)

#define EVENT_CONVERGE_CHILD_HEAD 1
#define EVENT_CONVERGE_CHILD_TAIL(event_id_num) ((event_id_num) - 1)

#define EVENT_CONVERGE_GET_BRO_NEXT(idx, event_id_num) (((idx) + 1) == (event_id_num) ? 1 : ((idx) + 1))

#define EVENT_CONVERGE_OPT_IS_VALID(opt) (((opt) == 'a') || ((opt) == 'm') || ((opt) == 's'))

struct dms_converge_htable g_converge_htable;
static atomic_t g_dms_notify_serial_num = KA_BASE_ATOMIC_INIT(0);

static int dms_event_converge_occur_add_to_cb(struct dms_dev_ctrl_block *dev_cb,
    DMS_EVENT_NODE_STRU *exception_node);

int dms_event_get_notify_serial_num(void)
{
    return ka_base_atomic_inc_return(&g_dms_notify_serial_num);
}
KA_EXPORT_SYMBOL(dms_event_get_notify_serial_num);

inline unsigned int dms_event_get_owner_node_type(const struct dms_event_obj *event_obj)
{
    /* If the value of sub node type is not 0, means that a parent node exists.
     * In this case, sub node type indicates the type of the faulty device */
    if (event_obj->sub_node_type != 0) {
        return event_obj->sub_node_type;
    } else {
        return event_obj->node_type;
    }
}

inline unsigned int dms_event_get_owner_node_id(const struct dms_event_obj *event_obj)
{
    if (event_obj->sub_node_type != 0) {
        return event_obj->sub_node_id;
    } else {
        return event_obj->node_id;
    }
}

static int dms_event_obj_to_event_name(struct dms_event_obj *event_obj,
    char *event_name, u32 name_len)
{
    unsigned int owner_node_type;
    unsigned int owner_node_id;
    char node_type[DMS_EVENT_NAME_BUFF_LEN] = { 0 };
    char sensor_type[DMS_EVENT_NAME_BUFF_LEN] = {0};
    char event_state[DMS_EVENT_NAME_BUFF_LEN] = {0};
    int ret;

    owner_node_type =  dms_event_get_owner_node_type(event_obj);
    owner_node_id = dms_event_get_owner_node_id(event_obj);
    ret = dms_get_node_type_str(owner_node_type, node_type, DMS_EVENT_NAME_BUFF_LEN);
    if (ret != 0) {
        dms_err("Get dev node type string failed. (node_type=0x%x; ret=%d)\n",
                event_obj->node_type, ret);
        return ret;
    }

    ret = dms_get_sensor_type_name(event_obj->event.sensor_event.sensor_type,
                                   sensor_type, DMS_EVENT_NAME_BUFF_LEN);
    if (ret != 0) {
        dms_err("Get name of sensor_type failed. (devid=%u; sensor_type=%u; ret=%d)\n",
                event_obj->deviceid, event_obj->event.sensor_event.sensor_type, ret);
        return ret;
    }

    ret = dms_get_event_string(event_obj->event.sensor_event.sensor_type,
                               event_obj->event.sensor_event.event_state,
                               event_state, DMS_EVENT_NAME_BUFF_LEN);
    if (ret != 0) {
        dms_err("Get name of event_state failed. (devid=%u; sensor_type=%u; event_state=%u; ret=%d)\n",
                event_obj->deviceid, event_obj->event.sensor_event.sensor_type,
                event_obj->event.sensor_event.event_state, ret);
        return ret;
    }

    ret = snprintf_s(event_name, DMS_MAX_EVENT_NAME_LENGTH, DMS_MAX_EVENT_NAME_LENGTH - 1,
                     "node type=%s, node id=%u, sensor type=%s, sensor name=%s, event state=%s, "
                     "time=%llu ms, event assertion=%u.", node_type, owner_node_id, sensor_type,
                     event_obj->event.sensor_event.sensor_name, event_state, event_obj->time_stamp,
                     event_obj->event.sensor_event.assertion);
    if (ret <= 0) {
        dms_err("Call snprintf_s failed. (deviceid=%u; node type=\"%s\"; node id=%u; sensor type=\"%s\";"
                " sensor name=\"%s\"; event state=\"%s\"; time=%llu ms; event_assertion=%d; ret=%d)\n",
                event_obj->deviceid, node_type, owner_node_id, sensor_type,
                event_obj->event.sensor_event.sensor_name, event_state, event_obj->time_stamp,
                event_obj->event.sensor_event.assertion, ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

static int dms_event_obj_to_event_code(struct dms_event_obj *event_obj,
    DMS_EVENT_NODE_STRU *exception_node)
{
    unsigned int owner_node_type;
#ifndef AOS_LLVM_BUILD
    exception_node->event.event_code = 0;
#ifdef CFG_HOST_ENV
    exception_node->event.event_code |= DMS_EVENT_CODE_ENVIRONMENT_HOST;
#else
    exception_node->event.event_code |= DMS_EVENT_CODE_ENVIRONMENT_DEVICE;
#endif
#else
    exception_node->event.event_code = 0;
    exception_node->event.event_code |= DMS_EVENT_CODE_ENVIRONMENT_DEVICE;
#endif

    owner_node_type = dms_event_get_owner_node_type(event_obj);

    exception_node->event.event_code |= \
        DMS_EVENT_OBJ_NODETYPE_TO_CODE(owner_node_type);
    exception_node->event.event_code |= \
        DMS_EVENT_OBJ_SENSOR_TYPE_TO_CODE(event_obj->event.sensor_event.sensor_type);
    exception_node->event.event_code |= \
        DMS_EVENT_OBJ_EVENT_STATE_TO_CODE(event_obj->event.sensor_event.event_state);
    return DRV_ERROR_NONE;
}

static int dms_event_obj_to_event_node(struct dms_event_obj *event_obj,
    DMS_EVENT_NODE_STRU *exception_node)
{
    int ret;

    exception_node->event.pid = event_obj->pid;
    exception_node->event.event_id = exception_node->event.event_code;
    exception_node->event.deviceid = event_obj->deviceid;
    exception_node->event.node_type = (unsigned short)event_obj->node_type;
    exception_node->event.node_id = (unsigned char)event_obj->node_id;
    exception_node->event.sub_node_type = (unsigned short)event_obj->sub_node_type;
    exception_node->event.sub_node_id = (unsigned char)event_obj->sub_node_id;
    exception_node->event.severity = event_obj->severity;
    exception_node->event.event_serial_num = event_obj->alarm_serial_num;
    exception_node->event.alarm_raised_time = event_obj->time_stamp;
    exception_node->event.assertion = event_obj->event.sensor_event.assertion;
    exception_node->event.sensor_num = event_obj->event.sensor_event.sensor_num;

    ret = memcpy_s(exception_node->event.additional_info, sizeof(exception_node->event.additional_info),
        event_obj->event.sensor_event.param_buffer, sizeof(event_obj->event.sensor_event.param_buffer));
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = memcpy_s(exception_node->event.event_info, sizeof(exception_node->event.event_info),
        event_obj->event.sensor_event.event_info, sizeof(event_obj->event.sensor_event.event_info));
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = dms_event_obj_to_event_name(event_obj, exception_node->event.event_name, DMS_MAX_EVENT_NAME_LENGTH);
    if (ret != 0) {
        dms_err("Get name of sensor_num failed. (devid=%u; sensor_num=%u; ret=%d)\n",
                event_obj->deviceid, event_obj->event.sensor_event.sensor_num, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

int dms_event_obj_to_exception(struct dms_event_obj *event_obj,
    DMS_EVENT_NODE_STRU *exception_node)
{
    int ret;

    /* event_obj --> event_code */
    ret = dms_event_obj_to_event_code(event_obj, exception_node);
    if (ret != 0) {
        dms_err("Transform event_obj to event_code failed. (devid=%u; nodeid=%u; ret=%d)\n",
                event_obj->deviceid, event_obj->node_id, ret);
        return ret;
    }

    /* event_obj --> dms_event_node */
    ret = dms_event_obj_to_event_node(event_obj, exception_node);
    if (ret != 0) {
        dms_err("Transform event_obj to dms_event failed. (devid=%u; nodeid=%u; ret=%d)\n",
                event_obj->deviceid, event_obj->node_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int dms_sensor_reported_resume_event(struct dms_event_sensor_reported *event_node,
    DMS_EVENT_NODE_STRU *exception_node)
{
    int i;

    if (event_node->event_id == exception_node->event.event_id) {
        for (i = 0; i < DMS_MAX_EVENT_NUM; i++) {
            if (event_node->sub_event_serial[i] == exception_node->event.event_serial_num) {
                event_node->sub_event_serial[i] = 0;
                event_node->event_serial_cnt--;
                break;
            }
        }
        if (event_node->event_serial_cnt == 0) {
            exception_node->event.event_serial_num = event_node->main_event_serial;
            list_del(&event_node->node);
            dbl_kfree(event_node);
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_EVENT_NOT_MATCH;
}

static int dms_sensor_reported_resume(struct dms_sensor_reported_list *reported_list,
    DMS_EVENT_NODE_STRU *exception_node)
{
    struct dms_event_sensor_reported *event_node = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    mutex_lock(&reported_list->lock);
    if (ka_list_empty_careful(&reported_list->head) != DRV_ERROR_NONE) {
        mutex_unlock(&reported_list->lock);
        return DRV_ERROR_EVENT_NOT_MATCH;
    }

    ka_list_for_each_safe(pos, n, &reported_list->head) {
        event_node = list_entry(pos, struct dms_event_sensor_reported, node);
        if (dms_sensor_reported_resume_event(event_node, exception_node) == DRV_ERROR_NONE) {
            reported_list->reported_num--;
            mutex_unlock(&reported_list->lock);
            return DRV_ERROR_NONE;
        }
    }
    mutex_unlock(&reported_list->lock);

    return DRV_ERROR_EVENT_NOT_MATCH;
}

static int dms_sensor_reported_occur_event(struct dms_event_sensor_reported *event_node,
    DMS_EVENT_NODE_STRU *exception_node)
{
    int i;

    if (event_node->event_id == exception_node->event.event_id) {
        for (i = 0; i < DMS_MAX_EVENT_NUM; i++) {
            if (event_node->sub_event_serial[i] == 0) {
                event_node->sub_event_serial[i] = exception_node->event.event_serial_num;
                event_node->event_serial_cnt++;
                return DRV_ERROR_EVENT_NOT_MATCH;
            }
        }
    }

    return DRV_ERROR_NONE;
}

static int dms_sensor_reported_occur(struct dms_sensor_reported_list *reported_list,
    DMS_EVENT_NODE_STRU *exception_node)
{
    struct dms_event_sensor_reported *event_node = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    mutex_lock(&reported_list->lock);
    if (reported_list->reported_num > DMS_MAX_EVENT_NUM) {
        dms_err("reported_num is too many. (reported_num=%u)\n", reported_list->reported_num);
        mutex_unlock(&reported_list->lock);
        return DRV_ERROR_EVENT_NOT_MATCH;
    }
    if (!ka_list_empty_careful(&reported_list->head)) {
        ka_list_for_each_safe(pos, n, &reported_list->head) {
            event_node = list_entry(pos, struct dms_event_sensor_reported, node);
            if (dms_sensor_reported_occur_event(event_node, exception_node) == DRV_ERROR_EVENT_NOT_MATCH) {
                mutex_unlock(&reported_list->lock);
                return DRV_ERROR_EVENT_NOT_MATCH;
            }
        }
    }

    event_node = dbl_kzalloc(sizeof(struct dms_event_sensor_reported), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (event_node == NULL) {
        dms_err("Alloc memory of event_node failed.\n");
        mutex_unlock(&reported_list->lock);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    event_node->pid = exception_node->event.pid;
    event_node->event_id = exception_node->event.event_id;
    event_node->main_event_serial = exception_node->event.event_serial_num;
    event_node->sub_event_serial[0] = exception_node->event.event_serial_num;
    event_node->event_serial_cnt++;
    ka_list_add_tail(&event_node->node, &reported_list->head);
    reported_list->reported_num++;
    mutex_unlock(&reported_list->lock);

    return DRV_ERROR_NONE;
}

STATIC int dms_event_obj_filter(DMS_EVENT_NODE_STRU *exception_node)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;

    if (g_converge_htable.converge_switch == false) {
        return DRV_ERROR_NONE;
    }

    dev_cb = dms_get_dev_cb(exception_node->event.deviceid);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", exception_node->event.deviceid);
        return DRV_ERROR_NO_DEVICE;
    }

    switch (exception_node->event.assertion) {
        case DMS_EVENT_TYPE_RESUME:
            return dms_sensor_reported_resume(&dev_cb->dev_event_cb.reported_list, exception_node);
        case DMS_EVENT_TYPE_OCCUR:
            return dms_sensor_reported_occur(&dev_cb->dev_event_cb.reported_list, exception_node);
        case DMS_EVENT_TYPE_ONE_TIME:
            return DRV_ERROR_NONE;
        default:
            dms_err("Invalid parameter. (event_assertion=%u)\n", exception_node->event.assertion);
            break;
    }

    return DRV_ERROR_PARA_ERROR;
}

STATIC EVENT_CONVERGE_NODE_T *dms_event_get_convergent_node(unsigned int event_id)
{
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;
    unsigned int key;

    key = EVENT_ID_TO_HASH_KEY(event_id);

    ka_hash_for_each_possible(g_converge_htable.htable, convergent_node, hnode, key) {
        if (convergent_node->event_id == event_id) {
            return convergent_node;
        }
    }

    return NULL;
}

static inline void dms_event_get_attribute_info(u32 devid, char opt,
    EVENT_CONVERGE_NODE_T *convergent_node, int *attribute_info)
{
    *attribute_info = (opt == 'a' ? convergent_node->assertion[devid] : *attribute_info);
    *attribute_info = (opt == 'm' ? convergent_node->mask[devid] : *attribute_info);
    *attribute_info = (opt == 's' ? convergent_node->event_serial_num[devid] : *attribute_info);
}

static int dms_event_print_convergent_next_layer(EVENT_CONVERGE_NODE_T *parent_node,
    u32 devid, char opt, int *avl_len, char **str)
{
    EVENT_CONVERGE_NODE_T *parent_bro = NULL;
    EVENT_CONVERGE_NODE_T *current_node = NULL;
    EVENT_CONVERGE_NODE_T *current_bro = NULL;
    int protect_i = 0;
    int protect_j = 0;
    int attribute_info = 0;
    int len;
    bool print_flag = false;

    parent_bro = parent_node;
    do {
        current_node = parent_bro->child_head;
        if (current_node != NULL) {
            current_bro = current_node->bro_next;
            dms_event_get_attribute_info(devid, opt, current_node, &attribute_info);
            len = snprintf_s(*str, *avl_len, *avl_len - 1, "  (0x%X)""{0x%X(%c:%d)",
                             parent_bro->event_id, current_node->event_id, opt, attribute_info);
            EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
            *str += len;
            *avl_len -= len;

            protect_j = 0;
            while ((protect_j++ <= EVENT_CONVERGE_ID_NUM_MAX) && (current_bro != current_node)) {
                dms_event_get_attribute_info(devid, opt, current_bro, &attribute_info);
                len = snprintf_s(*str, *avl_len, *avl_len - 1, " - 0x%X(%c:%d)",
                                 current_bro->event_id, opt, attribute_info);
                EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
                *str += len;
                *avl_len -= len;
                current_bro = current_bro->bro_next;
            }
            len = snprintf_s(*str, *avl_len, *avl_len - 1, "}");
            EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
            *str += len;
            *avl_len -= len;

            print_flag = true;
        }
        parent_bro = parent_bro->bro_next;
    } while ((protect_i++ <= EVENT_CONVERGE_ID_NUM_MAX) && (parent_bro != parent_node));

    return print_flag;
}

STATIC int dms_event_print_converge_one_tree(EVENT_CONVERGE_NODE_T *top_node,
    u32 devid, char opt, int *avl_len, char **str)
{
    EVENT_CONVERGE_NODE_T *parent_node = NULL;
    EVENT_CONVERGE_NODE_T *parent_bro = NULL;
    EVENT_CONVERGE_NODE_T *current_node = NULL;
    int protect_i = 0;
    int attribute_info = 0;
    int ret, len;
    char *refill_buf = *str;

    /* convergent event_node of layer 0 print */
    dms_event_get_attribute_info(devid, opt, top_node, &attribute_info);
    len = snprintf_s(*str, *avl_len, *avl_len - 1, "Top node: (event_id=0x%X)\n""  0x%X(%c:%d)\n""    |\n",
                     top_node->event_id, top_node->event_id, opt, attribute_info);
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
    *str += len;
    *avl_len -= len;

    /* convergent event_node of layer 1 print */
    ret = dms_event_print_convergent_next_layer(top_node, devid, opt, avl_len, str);
    EVENT_DFX_CHECK_DO_SOMETHING(ret == false, goto out_false);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, return ret);

    len = snprintf_s(*str, *avl_len, *avl_len - 1, "\n    |\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
    *str += len;
    *avl_len -= len;

    /* convergent event_node of layer 2 print */
    ret = dms_event_print_convergent_next_layer(top_node->child_head, devid, opt, avl_len, str);
    EVENT_DFX_CHECK_DO_SOMETHING(ret == false, goto out_false);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, return ret);

    len = snprintf_s(*str, *avl_len, *avl_len - 1, "\n    |\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
    *str += len;
    *avl_len -= len;

    /* convergent event_node of layer 3 print */
    parent_node = top_node->child_head;
    parent_bro = parent_node;
    do {
        current_node = parent_bro->child_head;
        if (current_node != NULL) {
            ret = dms_event_print_convergent_next_layer(current_node, devid, opt, avl_len, str);
            EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, return ret);
        }
        parent_bro = parent_bro->bro_next;
    } while ((protect_i++ <= EVENT_CONVERGE_ID_NUM_MAX) && (parent_bro != parent_node));

    len = snprintf_s(*str, *avl_len, *avl_len - 1, "\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
    *str += len;
    *avl_len -= len;

out_false:
    len = snprintf_s(*str, *avl_len, *avl_len - 1, "---------------------------------------------------------\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
    *str += len;
    return *str - refill_buf;
}

ssize_t dms_event_print_convergent_diagrams(u32 devid, char opt, char *str)
{
    int len, ret;
    int avl_len = EVENT_DFX_BUF_SIZE_MAX;
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;
    struct hlist_node *local_node = NULL;
    char *refill_buf = str;
    u32 bkt;

    len = snprintf_s(str, avl_len, avl_len - 1, "Convergent diagrams config: (switch=%s; validity=%s; dev_id=%u)\n",
        dms_event_is_converge() ? "on" : "off", g_converge_htable.converge_validity ? "true" : "false", devid);
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
    str += len;
    avl_len -= len;

    if ((devid >= DEVICE_NUM_MAX) || !EVENT_CONVERGE_OPT_IS_VALID(opt)) {
        len = snprintf_s(str, avl_len, avl_len - 1, "Invalid parameter, opt:a(assertion), m(mask), "
                         "s(event_serial_num). (dev_id=%u; opt='%c')\n", devid, opt);
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
        str += len;
        return str - refill_buf;
    }

    if (g_converge_htable.converge_validity == false) {
        return str - refill_buf;
    }

    mutex_lock(&g_converge_htable.lock);

    ka_hash_for_each_safe(g_converge_htable.htable, bkt, local_node, convergent_node, hnode) {
        if (convergent_node->parent != NULL) {
            /* this  convergent node isn't top node */
            continue;
        }

        ret = dms_event_print_converge_one_tree(convergent_node, devid, opt, &avl_len, &str);
        if (ret < 0) {
            dms_warn("Print convergent one tree warn. (event_id=0x%X)\n", convergent_node->event_id);
            goto out;
        }
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "Convergent diagrams end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    mutex_unlock(&g_converge_htable.lock);
    return str - refill_buf;
out:
    mutex_unlock(&g_converge_htable.lock);
    dms_warn("snprintf_s warn.\n");
    return 0;
}

ssize_t dms_event_print_event_list(u32 devid, char *str)
{
    int len;
    int avl_len = EVENT_DFX_BUF_SIZE_MAX;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct list_head *pos = NULL, *n = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;
    char *refill_buf = str;

    dev_cb = dms_get_dev_cb(devid);
    if (dev_cb == NULL) {
        len = snprintf_s(str, avl_len, avl_len - 1, "Invalid device id. (dev_id=%u)\n", devid);
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
        str += len;
        return str - refill_buf;
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "Print event list begin: (dev_id=%u)\n", devid);
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
    str += len;
    avl_len -= len;

    mutex_lock(&dev_cb->dev_event_cb.event_list.lock);
    if (ka_list_empty_careful(&dev_cb->dev_event_cb.event_list.head) != 0) {
        mutex_unlock(&dev_cb->dev_event_cb.event_list.lock);
        len = snprintf_s(str, avl_len, avl_len - 1, "Event list is empty.\n");
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
        str += len;
        return str - refill_buf;
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "device%u list: \n", devid);
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;
    avl_len -= len;

    ka_list_for_each_safe(pos, n, &dev_cb->dev_event_cb.event_list.head) {
        event_node = list_entry(pos, DMS_EVENT_NODE_STRU, node);
        len = snprintf_s(str, avl_len, avl_len - 1, "  0x%X", event_node->event.event_id);
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
        str += len;
        avl_len -= len;
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "\nPrint event list end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    mutex_unlock(&dev_cb->dev_event_cb.event_list.lock);
    return str - refill_buf;
out:
    mutex_unlock(&dev_cb->dev_event_cb.event_list.lock);
    dms_warn("snprintf_s warn. (dev_id=%u)\n", devid);
    return 0;
}

int dms_event_code_validity_check(u32 event_code)
{
    char event_str[DMS_MAX_EVENT_NAME_LENGTH] = {0};

    if (DMS_EVENT_CODE_GET_SERVERITY_ASSERTION(event_code) != 0) {
        dms_err("Invalid server and assertion of event code. (event_code=0x%x)\n", event_code);
        return DRV_ERROR_NO_EVENT;
    }

    if (dms_event_id_to_error_string(0, event_code, event_str, DMS_MAX_EVENT_NAME_LENGTH) != 0) {
        dms_err("Invalid type of event code. (event_code=0x%x)\n", event_code);
        return DRV_ERROR_NO_EVENT;
    }

    return DRV_ERROR_NONE;
}

static int dms_event_convergent_item_check(unsigned int item_converge_id[], unsigned int event_id_num)
{
    u32 i;

    if ((event_id_num < EVENT_CONVERGE_ID_NUM_MIN) || (event_id_num > EVENT_CONVERGE_ID_NUM_MAX)) {
        dms_err("Invalid event_id_num. (event_id_num=%u)\n", event_id_num);
        for (i = 0; i < event_id_num; i++) {
            dms_err("Invalid event_id. (event_id[%u]=0x%x)\n", i, item_converge_id[i]);
        }
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < event_id_num; i++) {
        if (dms_event_code_validity_check(item_converge_id[i]) != 0) {
            dms_err("Invalid event_id. (event_id[%u]=0x%x)\n", i, item_converge_id[i]);
            return DRV_ERROR_PARA_ERROR;
        }
    }

    return DRV_ERROR_NONE;
}

static int dms_event_convergent_item_add(unsigned int item_converge_id[], unsigned int event_id_num)
{
    EVENT_CONVERGE_NODE_T *event_node = NULL;
    EVENT_CONVERGE_NODE_T **child_node = NULL;
    EVENT_CONVERGE_NODE_T *parent_node = NULL;
    unsigned int i, bro_idx, key;
    int ret = DRV_ERROR_NONE;

    child_node = (EVENT_CONVERGE_NODE_T **)dbl_kzalloc(sizeof(EVENT_CONVERGE_NODE_T *) * event_id_num,
                                                   KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (child_node == NULL) {
        dms_err("Alloc memory of child_node failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    mutex_lock(&g_converge_htable.lock);
    for (i = 0; i < event_id_num; i++) {
        event_node = dms_event_get_convergent_node(item_converge_id[i]);
        if (event_node == NULL) {
            event_node = dbl_kzalloc(sizeof(EVENT_CONVERGE_NODE_T), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
            if (event_node == NULL) {
                dms_err("Alloc memory of event_node failed.\n");
                ret = DRV_ERROR_OUT_OF_MEMORY;
                goto out;
            }
            key = EVENT_ID_TO_HASH_KEY(item_converge_id[i]);
            event_node->event_id = item_converge_id[i];
            ka_hash_add(g_converge_htable.htable, &event_node->hnode, key);
        }
        parent_node = (event_node->event_id == item_converge_id[0]) ? event_node : parent_node;
        child_node[i] = (event_node->event_id == item_converge_id[i]) ? event_node : child_node[i];
    }

    for (i = EVENT_CONVERGE_CHILD_HEAD; i < event_id_num; i++) {
        bro_idx = EVENT_CONVERGE_GET_BRO_NEXT(i, event_id_num);
        if ((child_node[i]->parent !=  NULL) ||
            ((child_node[i]->bro_next != NULL) && (child_node[i]->bro_next != child_node[i]))) {
            dms_warn("One event_id converge to many event_id.\n");
            ret = DRV_ERROR_PARA_ERROR;
            goto out;
        }
        child_node[i]->parent = parent_node;
        child_node[i]->bro_next = child_node[bro_idx];
    }
    parent_node->child_head = child_node[EVENT_CONVERGE_CHILD_HEAD];
    parent_node->bro_next = parent_node;

out:
    mutex_unlock(&g_converge_htable.lock);
    dbl_kfree(child_node);
    return ret;
}

static unsigned int dms_event_convergent_item_fix(unsigned int item_converge_id[],
    unsigned int event_id_num)
{
    EVENT_CONVERGE_NODE_T *event_node = NULL;
    u32 converge_id_buf, i, event_num_fix;

    event_num_fix = event_id_num;
    for (i = EVENT_CONVERGE_CHILD_HEAD; i < EVENT_CONVERGE_CHILD_TAIL(event_id_num); i++) {
        if (item_converge_id[i] == item_converge_id[0]) {
            event_num_fix = EVENT_CONVERGE_CHILD_TAIL(event_id_num);
            break;
        }
    }

    mutex_lock(&g_converge_htable.lock);
    event_node = dms_event_get_convergent_node(item_converge_id[0]);
    if (event_node != NULL) {
        event_num_fix = EVENT_CONVERGE_CHILD_TAIL(event_id_num);
    }

    item_converge_id[0] = EVENT_CONVERGE_ID_ADD_CONVERGE_FLAG(item_converge_id[0]);
    for (i = EVENT_CONVERGE_CHILD_HEAD; i < EVENT_CONVERGE_CHILD_TAIL(event_id_num); i++) {
        converge_id_buf = EVENT_CONVERGE_ID_ADD_CONVERGE_FLAG(item_converge_id[i]);
        event_node = dms_event_get_convergent_node(converge_id_buf);
        if (event_node != NULL) {
            item_converge_id[i] = converge_id_buf;
        }
    }
    mutex_unlock(&g_converge_htable.lock);

    return event_num_fix;
}

int dms_event_convergent_item_init(unsigned int item_converge_id[], unsigned int event_id_num)
{
    if (dms_event_convergent_item_check(item_converge_id, event_id_num) != 0) {
        dms_warn("Invalid parameter.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    event_id_num = dms_event_convergent_item_fix(item_converge_id, event_id_num);
    if (dms_event_convergent_item_add(item_converge_id, event_id_num) != 0) {
        dms_warn("Add event convergent item warn.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    return DRV_ERROR_NONE;
}

STATIC bool dms_event_convergent_diagrams_layer_check(void)
{
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;
    struct hlist_node *local_node = NULL;
    u32 protect_i = 0;
    u32 bkt, layer;

    mutex_lock(&g_converge_htable.lock);
    ka_hash_for_each_safe(g_converge_htable.htable, bkt, local_node, convergent_node, hnode) {
        if (convergent_node->child_head != NULL) {
            /* this convergent node isn't bottom node */
            continue;
        }

        layer = 0;
        protect_i = 0;
        while ((protect_i++ <= EVENT_CONVERGE_ID_NUM_MAX) && (convergent_node->parent != NULL)) {
            convergent_node = convergent_node->parent;
            layer++;
        }
        if (layer > EVENT_CONVERGE_DIAGRAMS_LAYER_MAX) {
            mutex_unlock(&g_converge_htable.lock);
            dms_warn("Convergent diagrams layer is invalid. (layer=%u)\n", layer);
            return false;
        }
    }
    mutex_unlock(&g_converge_htable.lock);
    return true;
}

int dms_event_convergent_diagrams_init(void)
{
    char *str_buf = NULL;
    int ret;

    g_converge_htable.converge_switch = DMS_EVENT_CONVERGE_SWITCH;
    g_converge_htable.converge_validity = true;
    g_converge_htable.need_free = true;
    mutex_init(&g_converge_htable.lock);

    ret = EVENT_CONVERGE_CFG_INIT();
    if (ret != 0) {
        g_converge_htable.converge_switch = false;
        g_converge_htable.converge_validity = false;
        dms_warn("Event convergent config init warn, turn off converge switch. (ret=%d)\n", ret);
        return ret;
    }

    if (dms_event_convergent_diagrams_layer_check() == false) {
        g_converge_htable.converge_switch = false;
        g_converge_htable.converge_validity = false;
        dms_warn("Event convergent layer is invalid, turn off converge switch.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    str_buf = (char *)dbl_kzalloc(EVENT_DFX_BUF_SIZE_MAX, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (str_buf != NULL) {
        ret = dms_event_print_convergent_diagrams(0, 'a', str_buf);
        str_buf[EVENT_DFX_BUF_SIZE_MAX - 1] = '\0'; /* force to add \0 */
        if (ret > 0) {
            if (g_converge_htable.converge_switch == true) {
                dms_event("%s", str_buf);
            }
        } else {
            dms_warn("Print convergent diagrams warn.\n");
        }
        dbl_kfree(str_buf);
    } else {
        dms_warn("kzalloc str_buf warn.\n");
    }

    dms_event("Convergent diagrams config: (switch=%s; validity=%s)\n",
              dms_event_is_converge() ? "on" : "off",
              g_converge_htable.converge_validity ? "true" : "false");
    return DRV_ERROR_NONE;
}

void dms_event_convergent_diagrams_exit(void)
{
    EVENT_CONVERGE_NODE_T *event_node = NULL;
    struct hlist_node *local_node = NULL;
    u32 bkt, i;

    /* to avoid double free */
    if (g_converge_htable.need_free == false) {
        return;
    }

    mutex_lock(&g_converge_htable.lock);
    ka_hash_for_each_safe(g_converge_htable.htable, bkt, local_node, event_node, hnode) {
        for (i = 0; i < DEVICE_NUM_MAX; i++) {
            if (event_node->exception_data[i] != NULL) {
                dbl_kfree(event_node->exception_data[i]);
                event_node->exception_data[i] = NULL;
            }
        }
        hash_del(&event_node->hnode);
        dbl_kfree(event_node);
        event_node = NULL;
    }
    mutex_unlock(&g_converge_htable.lock);
    mutex_destroy(&g_converge_htable.lock);

    g_converge_htable.need_free = false;
}

int dms_event_convergent_diagrams_clear(u32 devid, bool hotreset)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    EVENT_CONVERGE_NODE_T *event_node = NULL;
    struct hlist_node *local_node = NULL;
    u32 bkt;

    dev_cb = dms_get_dev_cb(devid);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", devid);
        return DRV_ERROR_NO_DEVICE;
    }

    dms_event_ctrl_converge_list_free(&dev_cb->dev_event_cb.event_list);
    dms_event_sensor_reported_list_free(&dev_cb->dev_event_cb.reported_list);

    if (g_converge_htable.converge_validity == false) {
        return DRV_ERROR_NONE;
    }

    mutex_lock(&g_converge_htable.lock);
    ka_hash_for_each_safe(g_converge_htable.htable, bkt, local_node, event_node, hnode) {
        event_node->event_serial_num[devid] = 0;
        event_node->assertion[devid] = DMS_EVENT_TYPE_RESUME;
        event_node->mask[devid] = hotreset ? EVENT_CONVERGE_NODE_ENABLE : event_node->mask[devid];
        if (event_node->exception_data[devid] != NULL) {
            dbl_kfree(event_node->exception_data[devid]);
            event_node->exception_data[devid] = NULL;
        }
    }
    mutex_unlock(&g_converge_htable.lock);

    return DRV_ERROR_NONE;
}

STATIC void dms_event_clear_reported_list(struct dms_sensor_reported_list *reported_list, int owner_pid)
{
    struct dms_event_sensor_reported *event_node = NULL;
    struct list_head *pos = NULL, *n = NULL;

    mutex_lock(&reported_list->lock);
    ka_list_for_each_safe(pos, n, &reported_list->head) {
        event_node = list_entry(pos, struct dms_event_sensor_reported, node);
        if (event_node->pid == owner_pid) {
            dms_debug("clear reported_list success. (pid=%d; event_id=0x%x)\n", owner_pid, event_node->event_id);
            list_del(&event_node->node);
            dbl_kfree(event_node);
            event_node = NULL;
            reported_list->reported_num--;
        }
    }
    mutex_unlock(&reported_list->lock);

    return;
}

void dms_event_cb_release(int owner_pid)
{
    u32 i;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;
    struct dms_converge_event_list *converge_list = NULL;

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        dev_cb = dms_get_dev_cb(i);
        if (dev_cb == NULL) {
            dms_warn("Get dev_ctrl block warn. (dev_id=%u)\n", i);
            continue;
        }
        dms_event_clear_reported_list(&dev_cb->dev_event_cb.reported_list, owner_pid);
        converge_list = &dev_cb->dev_event_cb.event_list;
        mutex_lock(&converge_list->lock);
        converge_list->health_code = 0;

        ka_list_for_each_safe(pos, n, &converge_list->head) {
            event_node = list_entry(pos, DMS_EVENT_NODE_STRU, node);
            if (event_node->event.pid == owner_pid) {
                dms_debug("release cb success. (pid=%d; event_id=0x%x)\n", owner_pid, event_node->event.event_id);
                list_del(&event_node->node);
                dbl_kfree(event_node);
                event_node = NULL;
                converge_list->event_num--;
                continue;
            }
            converge_list->health_code = event_node->event.severity > converge_list->health_code ? \
                                        event_node->event.severity : converge_list->health_code;
        }
        mutex_unlock(&converge_list->lock);
    }

    return;
}
KA_EXPORT_SYMBOL(dms_event_cb_release);

void dms_event_mask_del_to_event_cb(u32 phyid, struct dms_dev_ctrl_block *dev_cb, u32 event_id)
{
    struct dms_converge_event_list *converge_list = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;

    converge_list = &dev_cb->dev_event_cb.event_list;

    mutex_lock(&converge_list->lock);
    converge_list->health_code = 0;
    if (ka_list_empty_careful(&converge_list->head) != 0) {
        converge_list->event_num = 0;
        mutex_unlock(&converge_list->lock);
        return;
    }

    ka_list_for_each_safe(pos, n, &converge_list->head) {
        event_node = list_entry(pos, DMS_EVENT_NODE_STRU, node);
        if (event_node->event.event_id == event_id) {
            list_del(&event_node->node);
            dbl_kfree(event_node);
            event_node = NULL;
            converge_list->event_num--;
            continue;
        }
        converge_list->health_code = event_node->event.severity > converge_list->health_code ? \
                                     event_node->event.severity : converge_list->health_code;
    }
    mutex_unlock(&converge_list->lock);
}

void dms_event_mask_add_to_event_cb(u32 phyid, struct dms_dev_ctrl_block *dev_cb, u32 event_id)
{
    u32 converge_event_id = EVENT_CONVERGE_ID_ADD_CONVERGE_FLAG(event_id);
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;
    DMS_EVENT_NODE_STRU exception_data = {{0}};

    if (g_converge_htable.converge_validity == false) {
        return;
    }

    mutex_lock(&g_converge_htable.lock);
    convergent_node = dms_event_get_convergent_node(converge_event_id);
    /* top convergent node isn't occur */
    if ((convergent_node == NULL) || (convergent_node->exception_data[phyid] == NULL)) {
        mutex_unlock(&g_converge_htable.lock);
        return;
    }
    /* top convergent node isn't disable before */
    if (convergent_node->mask[phyid] != EVENT_CONVERGE_NODE_DISENABLE) {
        mutex_unlock(&g_converge_htable.lock);
        return;
    }

    if (memcpy_s(&exception_data, sizeof(DMS_EVENT_NODE_STRU),
                 convergent_node->exception_data[phyid], sizeof(DMS_EVENT_NODE_STRU)) != 0) {
        mutex_unlock(&g_converge_htable.lock);
        return;
    }
    mutex_unlock(&g_converge_htable.lock);

    (void)dms_event_converge_occur_add_to_cb(dev_cb, &exception_data);
    if (dms_event_distribute_handle(&exception_data, DMS_DISTRIBUTE_PRIORITY0)) {
        dms_err("Distribute exception failed. (dev_id=%u; event_id=0x%x)\n",
                exception_data.event.deviceid, exception_data.event.event_id);
    }
}

void dms_event_add_to_mask_list(struct dms_dev_ctrl_block *dev_cb, u32 event_id)
{
    struct dms_mask_event *mask_event = NULL;

    mask_event = (struct dms_mask_event *)dbl_kzalloc(sizeof(struct dms_mask_event), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (mask_event == NULL) {
        dms_err("kzalloc failed. (event_id=0x%X)\n", event_id);
        return;
    }

    mask_event->event_id = event_id;

    mutex_lock(&dev_cb->dev_event_cb.dfx_table.lock);
    ka_list_add_tail(&mask_event->node, &dev_cb->dev_event_cb.dfx_table.mask_list);
    mutex_unlock(&dev_cb->dev_event_cb.dfx_table.lock);
}

void dms_event_del_to_mask_list(struct dms_dev_ctrl_block *dev_cb, u32 event_id)
{
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct dms_mask_event *mask_event = NULL;

    mutex_lock(&dev_cb->dev_event_cb.dfx_table.lock);
    if (ka_list_empty_careful(&dev_cb->dev_event_cb.dfx_table.mask_list) != 0) {
        mutex_unlock(&dev_cb->dev_event_cb.dfx_table.lock);
        return;
    }

    ka_list_for_each_safe(pos, n, &dev_cb->dev_event_cb.dfx_table.mask_list) {
        mask_event = list_entry(pos, struct dms_mask_event, node);
        if (mask_event->event_id == event_id) {
            list_del(&mask_event->node);
            dbl_kfree(mask_event);
            mask_event = NULL;
        }
    }
    mutex_unlock(&dev_cb->dev_event_cb.dfx_table.lock);
}

void dms_event_mask_list_clear(struct dms_dev_ctrl_block *dev_cb)
{
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct dms_mask_event *mask_event = NULL;

    mutex_lock(&dev_cb->dev_event_cb.dfx_table.lock);
    if (!ka_list_empty_careful(&dev_cb->dev_event_cb.dfx_table.mask_list)) {
        ka_list_for_each_safe(pos, n, &dev_cb->dev_event_cb.dfx_table.mask_list) {
            mask_event = list_entry(pos, struct dms_mask_event, node);
            list_del(&mask_event->node);
            dbl_kfree(mask_event);
            mask_event = NULL;
        }
    }
    mutex_unlock(&dev_cb->dev_event_cb.dfx_table.lock);
}

ssize_t dms_event_print_mask_list(u32 devid, char *str)
{
    int len;
    int avl_len = EVENT_DFX_BUF_SIZE_MAX;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct dms_mask_event *mask_event = NULL;
    char *refill_buf = str;

    dev_cb = dms_get_dev_cb(devid);
    if (dev_cb == NULL) {
        len = snprintf_s(str, avl_len, avl_len - 1, "Invalid device id. (dev_id=%u)\n", devid);
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
        str += len;
        return str - refill_buf;
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "Print mask list begin: (dev_id=%u)\n", devid);
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
    str += len;
    avl_len -= len;

    mutex_lock(&dev_cb->dev_event_cb.dfx_table.lock);
    if (ka_list_empty_careful(&dev_cb->dev_event_cb.dfx_table.mask_list) != 0) {
        mutex_unlock(&dev_cb->dev_event_cb.dfx_table.lock);
        len = snprintf_s(str, avl_len, avl_len - 1, "Mask list is empty.\n");
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
        str += len;
        return str - refill_buf;
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "device%u list: \n", devid);
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;
    avl_len -= len;

    ka_list_for_each_safe(pos, n, &dev_cb->dev_event_cb.dfx_table.mask_list) {
        mask_event = list_entry(pos, struct dms_mask_event, node);
        len = snprintf_s(str, avl_len, avl_len - 1, "  0x%X", mask_event->event_id);
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
        str += len;
        avl_len -= len;
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "\nPrint mask list end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    mutex_unlock(&dev_cb->dev_event_cb.dfx_table.lock);
    return str - refill_buf;
out:
    mutex_unlock(&dev_cb->dev_event_cb.dfx_table.lock);
    dms_warn("snprintf_s warn. (dev_id=%u)\n", devid);
    return 0;
}

void dms_event_convergent_diagrams_mask(u32 devid, u32 event_id, u8 mask)
{
    u32 converge_event_id = EVENT_CONVERGE_ID_ADD_CONVERGE_FLAG(event_id);
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;

    if (g_converge_htable.converge_validity == false) {
        return;
    }

    mutex_lock(&g_converge_htable.lock);
    convergent_node = dms_event_get_convergent_node(converge_event_id);
    if (convergent_node != NULL) {
        convergent_node->mask[devid] = mask;
        mutex_unlock(&g_converge_htable.lock);
        return;
    }

    convergent_node = dms_event_get_convergent_node(event_id);
    if (convergent_node != NULL) {
        convergent_node->mask[devid] = mask;
    }
    mutex_unlock(&g_converge_htable.lock);
}

STATIC int dms_event_obj_converge_one_time(DMS_EVENT_NODE_STRU *exception_node)
{
    unsigned short deviceid = exception_node->event.deviceid;
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;
    u32 protect_i = 0;

    mutex_lock(&g_converge_htable.lock);
    convergent_node = dms_event_get_convergent_node(exception_node->event.event_id);
    if (convergent_node == NULL) {
        mutex_unlock(&g_converge_htable.lock);
        return DRV_ERROR_NONE;
    }

    do {
        /* don't report occur event when the event_id has been disabled */
        if (convergent_node->mask[deviceid] == EVENT_CONVERGE_NODE_DISENABLE) {
            mutex_unlock(&g_converge_htable.lock);
            return DRV_ERROR_EVENT_NOT_MATCH;
        }
        exception_node->event.event_id = convergent_node->event_id;
        convergent_node = convergent_node->parent;
    } while ((protect_i++ <= EVENT_CONVERGE_ID_NUM_MAX) && (convergent_node != NULL));

    mutex_unlock(&g_converge_htable.lock);
    return DRV_ERROR_NONE;
}

STATIC int dms_event_obj_converge_occur(DMS_EVENT_NODE_STRU *exception_node)
{
    unsigned short deviceid = exception_node->event.deviceid;
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;
    u32 protect_i = 0;
    u32 severity;

    mutex_lock(&g_converge_htable.lock);
    convergent_node = dms_event_get_convergent_node(exception_node->event.event_id);
    if (convergent_node == NULL) {
        mutex_unlock(&g_converge_htable.lock);
        return DRV_ERROR_NONE;
    }

    do {
        /* store the information of top convergent node to report when event_id of top convergent node be enabled */
        if ((convergent_node->parent == NULL) && (convergent_node->exception_data[deviceid] == NULL)) {
            convergent_node->exception_data[deviceid] = dbl_kzalloc(sizeof(DMS_EVENT_NODE_STRU),
                KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
            if (convergent_node->exception_data[deviceid] == NULL) {
                mutex_unlock(&g_converge_htable.lock);
                dms_err("kzalloc failed. (dev_id=%u)\n", exception_node->event.deviceid);
                return DRV_ERROR_OUT_OF_MEMORY;
            }
            (void)memcpy_s(convergent_node->exception_data[deviceid], sizeof(DMS_EVENT_NODE_STRU),
                           exception_node, sizeof(DMS_EVENT_NODE_STRU));
            convergent_node->exception_data[deviceid]->event.event_id = convergent_node->event_id;
            severity = exception_node->event.severity;
            (void)dms_get_event_severity(DMS_EVENT_ID_TO_NODE_TYPE(convergent_node->event_id),
                                         DMS_EVENT_ID_TO_SENSOR_TYPE(convergent_node->event_id),
                                         DMS_EVENT_ID_TO_EVENT_STATE(convergent_node->event_id), &severity);
            convergent_node->exception_data[deviceid]->event.severity = (unsigned char)severity;
        }
        /* don't report occur event when the event_id has been reported before
         * don't report occur event when the event_id has been disabled */
        if ((convergent_node->assertion[deviceid] == DMS_EVENT_TYPE_OCCUR) ||
            (convergent_node->mask[deviceid] == EVENT_CONVERGE_NODE_DISENABLE)) {
            mutex_unlock(&g_converge_htable.lock);
            return DRV_ERROR_EVENT_NOT_MATCH;
        }
        convergent_node->assertion[deviceid] = DMS_EVENT_TYPE_OCCUR;
        convergent_node->event_serial_num[deviceid] = exception_node->event.event_serial_num;
        exception_node->event.event_id = convergent_node->event_id;
        convergent_node = convergent_node->parent;
    } while ((protect_i++ <= EVENT_CONVERGE_ID_NUM_MAX) && (convergent_node != NULL));
    mutex_unlock(&g_converge_htable.lock);

    return DRV_ERROR_NONE;
}

STATIC int dms_event_obj_converge_resume(DMS_EVENT_NODE_STRU *exception_node)
{
    unsigned short deviceid = exception_node->event.deviceid;
    EVENT_CONVERGE_NODE_T *convergent_node = NULL;
    EVENT_CONVERGE_NODE_T *bro_node = NULL;
    u32 protect_i = 0;
    u32 protect_j = 0;
    bool mask_flag;

    mutex_lock(&g_converge_htable.lock);
    convergent_node = dms_event_get_convergent_node(exception_node->event.event_id);
    if (convergent_node == NULL) {
        mutex_unlock(&g_converge_htable.lock);
        return DRV_ERROR_NONE;
    }

    /* don't report resume event when the occur event_id hasn't been reported before */
    if (convergent_node->assertion[deviceid] != DMS_EVENT_TYPE_OCCUR) {
        dms_warn("This event hasn't been reported before. (dev_id=%u; event_id=0x%x)\n",
                 deviceid, exception_node->event.event_id);
        mutex_unlock(&g_converge_htable.lock);
        return DRV_ERROR_EVENT_NOT_MATCH;
    }
    convergent_node->assertion[deviceid] = DMS_EVENT_TYPE_RESUME;
    convergent_node->event_serial_num[deviceid] = 0;
    mask_flag = convergent_node->mask[deviceid] == EVENT_CONVERGE_NODE_DISENABLE ? true : false;

    /* parent layer traversal */
    while ((protect_i++ <= EVENT_CONVERGE_ID_NUM_MAX) && (convergent_node->parent != NULL)) {
        /* current layer traversal */
        bro_node = convergent_node->bro_next;
        protect_j = 0;
        while ((protect_j++ <= EVENT_CONVERGE_ID_NUM_MAX) && (bro_node != convergent_node)) {
            /* there is one node that haven't been recovered in current layer */
            if (bro_node->assertion[deviceid] == DMS_EVENT_TYPE_OCCUR) {
                mutex_unlock(&g_converge_htable.lock);
                return DRV_ERROR_EVENT_NOT_MATCH;
            }
            bro_node = bro_node->bro_next;
        }
        convergent_node = convergent_node->parent;
        mask_flag = convergent_node->mask[deviceid] == EVENT_CONVERGE_NODE_DISENABLE ? true : mask_flag;
        exception_node->event.event_id = convergent_node->event_id;
        exception_node->event.event_serial_num = convergent_node->event_serial_num[deviceid];
        convergent_node->assertion[deviceid] = DMS_EVENT_TYPE_RESUME;
        convergent_node->event_serial_num[deviceid] = 0;
    }
    /* free the information of top convergent node when resumed */
    if (convergent_node->exception_data[deviceid] != NULL) {
        exception_node->event.event_serial_num = convergent_node->exception_data[deviceid]->event.event_serial_num;
        exception_node->event.severity = convergent_node->exception_data[deviceid]->event.severity;
        dbl_kfree(convergent_node->exception_data[deviceid]);
        convergent_node->exception_data[deviceid] = NULL;
    }
    mutex_unlock(&g_converge_htable.lock);

    return mask_flag ? DRV_ERROR_EVENT_NOT_MATCH : DRV_ERROR_NONE;
}

static int dms_event_converge_resume_add_to_cb(struct dms_dev_ctrl_block *dev_cb,
    DMS_EVENT_NODE_STRU *exception_node)
{
    struct dms_converge_event_list *converge_list = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;

    converge_list = &dev_cb->dev_event_cb.event_list;

    mutex_lock(&converge_list->lock);
    if (ka_list_empty_careful(&converge_list->head) != 0) {
        converge_list->event_num = 0;
        mutex_unlock(&converge_list->lock);
        dms_warn("Event list is empty. (dev_id=%u)\n", exception_node->event.deviceid);
        return DRV_ERROR_EVENT_NOT_MATCH;
    }

    converge_list->health_code = 0;
    ka_list_for_each_safe(pos, n, &converge_list->head) {
        event_node = list_entry(pos, DMS_EVENT_NODE_STRU, node);
        if (exception_node->event.event_serial_num == event_node->event.event_serial_num) {
            exception_node->event.node_type = event_node->event.node_type;
            exception_node->event.node_id = event_node->event.node_id;
            exception_node->event.sub_node_type = event_node->event.sub_node_type;
            exception_node->event.sub_node_id = event_node->event.sub_node_id;
            exception_node->event.severity = event_node->event.severity;
            (void)memcpy_s(exception_node->event.event_name, sizeof(exception_node->event.event_name),
                           event_node->event.event_name, sizeof(event_node->event.event_name));
            (void)memcpy_s(exception_node->event.additional_info, sizeof(exception_node->event.additional_info),
                           event_node->event.additional_info, sizeof(event_node->event.additional_info));
            list_del(&event_node->node);
            dbl_kfree(event_node);
            event_node = NULL;
            converge_list->event_num--;
            continue;
        }
        converge_list->health_code = event_node->event.severity > converge_list->health_code ? \
                                     event_node->event.severity : converge_list->health_code;
    }
    mutex_unlock(&converge_list->lock);

    return DRV_ERROR_NONE;
}

static int dms_event_converge_occur_add_to_cb(struct dms_dev_ctrl_block *dev_cb,
    DMS_EVENT_NODE_STRU *exception_node)
{
    struct dms_converge_event_list *converge_list = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;
    DMS_EVENT_NODE_STRU *tmp_node = NULL;

    event_node = dbl_kzalloc(sizeof(DMS_EVENT_NODE_STRU), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (event_node == NULL) {
        dms_err("kzalloc failed. (dev_id=%u)\n", exception_node->event.deviceid);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (memcpy_s((void *)&event_node->event, sizeof(struct dms_event_para),
                 (void *)&exception_node->event, sizeof(struct dms_event_para)) != 0) {
        dms_err("memcpy_s failed. (dev_id=%u)\n", exception_node->event.deviceid);
        dbl_kfree(event_node);
        return DRV_ERROR_INNER_ERR;
    }

    converge_list = &dev_cb->dev_event_cb.event_list;
    mutex_lock(&converge_list->lock);
    ka_list_for_each_safe(pos, n, &converge_list->head) {
        tmp_node = list_entry(pos, DMS_EVENT_NODE_STRU, node);
        if (tmp_node->event.event_id == exception_node->event.event_id) {
            mutex_unlock(&converge_list->lock);
            dbl_kfree(event_node);
            return DRV_ERROR_EVENT_NOT_MATCH;
        }
    }
    ka_list_add_tail(&event_node->node, &converge_list->head);
    converge_list->event_num++;
    converge_list->health_code = event_node->event.severity > converge_list->health_code ? \
                                 event_node->event.severity : converge_list->health_code;

    if (converge_list->event_num > DMS_MAX_EVENT_NUM) {
        tmp_node = ka_list_first_entry(&converge_list->head, DMS_EVENT_NODE_STRU, node);
        dms_event("Dms event is covered. (dev_id=%u; event_id=0x%X; event_serial_num=%d; notify_serial_num=%d)\n",
                  tmp_node->event.deviceid, tmp_node->event.event_id, tmp_node->event.event_serial_num,
                  tmp_node->event.notify_serial_num);
        list_del(&tmp_node->node);
        dbl_kfree(tmp_node);
        converge_list->event_num--;
        converge_list->health_code = 0;
        ka_list_for_each_safe(pos, n, &converge_list->head) {
            tmp_node = list_entry(pos, DMS_EVENT_NODE_STRU, node);
            converge_list->health_code = tmp_node->event.severity > converge_list->health_code ? \
                                         tmp_node->event.severity : converge_list->health_code;
        }
    }
    mutex_unlock(&converge_list->lock);

    return DRV_ERROR_NONE;
}

STATIC int dms_event_obj_converge(DMS_EVENT_NODE_STRU *exception_node)
{
    if (g_converge_htable.converge_switch == false) {
        return DRV_ERROR_NONE;
    }

    switch (exception_node->event.assertion) {
        case DMS_EVENT_TYPE_RESUME:
            return dms_event_obj_converge_resume(exception_node);
        case DMS_EVENT_TYPE_OCCUR:
            return dms_event_obj_converge_occur(exception_node);
        case DMS_EVENT_TYPE_ONE_TIME:
            return dms_event_obj_converge_one_time(exception_node);
        default:
            dms_err("Invalid parameter. (event_assertion=%u)\n", exception_node->event.assertion);
            break;
    }
    return DRV_ERROR_PARA_ERROR;
}

int dms_event_converge_add_to_event_cb(DMS_EVENT_NODE_STRU *exception_node)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    int ret = DRV_ERROR_NONE;

    if (g_converge_htable.converge_switch == false) {
        return DRV_ERROR_NONE;
    }

    dev_cb = dms_get_dev_cb(exception_node->event.deviceid);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", exception_node->event.deviceid);
        return DRV_ERROR_NO_DEVICE;
    }

    switch (exception_node->event.assertion) {
        case DMS_EVENT_TYPE_RESUME:
            ret = dms_event_converge_resume_add_to_cb(dev_cb, exception_node);
            break;
        case DMS_EVENT_TYPE_OCCUR:
            ret = dms_event_converge_occur_add_to_cb(dev_cb, exception_node);
            break;
        case DMS_EVENT_TYPE_ONE_TIME:
            ret = DRV_ERROR_NONE;
            break;
        default:
            dms_err("Invalid parameter. (event_assertion=%u)\n", exception_node->event.assertion);
            return DRV_ERROR_PARA_ERROR;
    }

    if (ret == DRV_ERROR_NONE) {
        exception_node->event.notify_serial_num = ka_base_atomic_inc_return(&g_dms_notify_serial_num);
    }

    return ret;
}

int dms_get_event_code_from_event_cb(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;
    unsigned int i = 0;

    if (dms_get_code_from_sensor_check(devid, health_code, health_len, event_code, event_len) != 0) {
        dms_err("Invalid parameter.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    dev_cb = dms_get_dev_cb(devid);
    if (dev_cb == NULL) {
        dms_err("Get dev_ctrl block failed. (dev_id=%u)\n", devid);
        return DRV_ERROR_NO_DEVICE;
    }

    mutex_lock(&dev_cb->dev_event_cb.event_list.lock);
    if (ka_list_empty_careful(&dev_cb->dev_event_cb.event_list.head) != 0) {
        mutex_unlock(&dev_cb->dev_event_cb.event_list.lock);
        return DRV_ERROR_NONE;
    }

    ka_list_for_each_safe(pos, n, &dev_cb->dev_event_cb.event_list.head) {
        event_node = list_entry(pos, DMS_EVENT_NODE_STRU, node);
        health_code[0] = event_node->event.severity > health_code[0] ? \
                         event_node->event.severity : health_code[0];
        if (i < event_len) {
            event_code[i].event_code = event_node->event.event_id;
            event_code[i].fid = 0;
        }
        i++;
    }
    mutex_unlock(&dev_cb->dev_event_cb.event_list.lock);

    if (i > event_len) {
        dms_warn("Event_list is larger than event_code array. (dev_id=%u; event_num=%u)\n", devid, i);
    }
    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_get_event_code_from_event_cb);

int dms_event_is_converge(void)
{
    return g_converge_htable.converge_switch;
}
KA_EXPORT_SYMBOL(dms_event_is_converge);

int dms_event_obj_to_error_code(struct dms_event_obj event_obj, u32 *error_code)
{
    DMS_EVENT_NODE_STRU exception_node = {{0}};
    int ret;

    if (error_code == NULL) {
        dms_err("Pointer of error_code is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_event_obj_to_event_code(&event_obj, &exception_node);
    if (ret != 0) {
        dms_err("Transform event_obj to event_code failed. (devid=%u; nodeid=%u; ret=%d)\n",
                event_obj.deviceid, event_obj.node_id, ret);
        return ret;
    }

    *error_code = exception_node.event.event_code;
    return DRV_ERROR_NONE;
}

int dms_event_id_to_error_string(u32 devid, u32 event_id, char *event_str, u32 name_len)
{
    u32 dev_node_type, dev_sensor_type, dev_event_state;
    char node_type[DMS_EVENT_NAME_BUFF_LEN] = {0};
    char sensor_type[DMS_EVENT_NAME_BUFF_LEN] = {0};
    char event_state[DMS_EVENT_NAME_BUFF_LEN] = {0};
    int ret;

    if ((event_str == NULL) || (name_len < BBOX_ERRSTR_LEN)) {
        dms_err("Invalid parameter. (event_str=\"%s\"; name_len=%u)\n",
                ((event_str == NULL) ? "NULL" : "OK"), name_len);
        return DRV_ERROR_PARA_ERROR;
    }

    if (!(DMS_EVENT_CODE_IS_HOST(event_id) || DMS_EVENT_CODE_IS_DEVICE(event_id))) {
        dms_err("Invalid side of event code. (event_code=0x%x)\n", event_id);
        return DRV_ERROR_PARA_ERROR;
    }

    dev_node_type = DMS_EVENT_ID_TO_NODE_TYPE(event_id);
    dev_sensor_type = DMS_EVENT_ID_TO_SENSOR_TYPE(event_id);
    dev_event_state = DMS_EVENT_ID_TO_EVENT_STATE(event_id);

    ret = dms_get_node_type_str(dev_node_type, node_type, DMS_EVENT_NAME_BUFF_LEN);
    if (ret != 0) {
        dms_err("Get dev node type string failed. (devid=%u; node_type=0x%x; ret=%d)\n",
                devid, dev_node_type, ret);
        return ret;
    }

    ret = dms_get_sensor_type_name(dev_sensor_type, sensor_type, DMS_EVENT_NAME_BUFF_LEN);
    if (ret != 0) {
        dms_err("Get name of sensor_type failed. (event_id=0x%x; ret=%d)\n", event_id, ret);
        return ret;
    }

    ret = dms_get_event_string(dev_sensor_type, dev_event_state, event_state, DMS_EVENT_NAME_BUFF_LEN);
    if (ret != 0) {
        dms_err("Get name of event_state failed. (event_id=0x%x; ret=%d)\n", event_id, ret);
        return ret;
    }

    if (name_len >= DMS_MAX_EVENT_NAME_LENGTH) {
        ret = snprintf_s(event_str, name_len, name_len - 1, "node type=%s, sensor type=%s, event state=%s",
                         node_type, sensor_type, event_state);
    } else {
        ret = snprintf_s(event_str, name_len, name_len - 1, "node type=0x%x, sensor type=0x%x, event=%u",
                         dev_node_type, dev_sensor_type, dev_event_state);
    }

    if (ret <= 0) {
        dms_err("Call snprintf_s failed. (node_type=\"%s\"; sensor_type=\"%s\";"
                " event_state=\"%s\"; event_id=0x%x; ret=%d)\n",
                node_type, sensor_type, event_state, event_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

int dms_event_converge_to_exception(struct dms_event_obj *event_obj,
    DMS_EVENT_NODE_STRU *exception_node)
{
    unsigned int severity = 0;
    int ret;

    if ((event_obj == NULL) || (exception_node == NULL)) {
        dms_err("Invalid parameter. (event_obj=\"%s\"; exception_node=\"%s\")\n",
                (event_obj == NULL) ? "NULL" : "OK", (exception_node == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    (void)memset_s((void *)exception_node, sizeof(DMS_EVENT_NODE_STRU),
                   0, sizeof(DMS_EVENT_NODE_STRU));

    ret = dms_event_obj_to_exception(event_obj, exception_node);
    if (ret != 0) {
        dms_err("Transform event_obj to exception failed. (devid=%u; nodeid=%u; ret=%d)\n",
                event_obj->deviceid, event_obj->node_id, ret);
        return ret;
    }

    ret = dms_event_obj_filter(exception_node);
    if (ret != 0) {
        dms_event("Dms event is filtered. (dev_id=%u; event_id=0x%X; event_serial_num=%d; notify_serial_num=%d)\n",
                  exception_node->event.deviceid, exception_node->event.event_id,
                  exception_node->event.event_serial_num, exception_node->event.notify_serial_num);
        return ret;
    }

    ret = dms_event_obj_converge(exception_node);
    if (ret == DRV_ERROR_EVENT_NOT_MATCH) {
        return DRV_ERROR_EVENT_NOT_MATCH;
    } else if (ret != 0) {
        dms_err("Converge the event_obj failed. (devid=%u; nodeid=%u; ret=%d)\n",
                event_obj->deviceid, event_obj->node_id, ret);
        return ret;
    }

    ret = dms_get_event_severity(DMS_EVENT_ID_TO_NODE_TYPE(exception_node->event.event_id),
                                 DMS_EVENT_ID_TO_SENSOR_TYPE(exception_node->event.event_id),
                                 DMS_EVENT_ID_TO_EVENT_STATE(exception_node->event.event_id), &severity);
    if (ret != 0) {
        dms_warn("Get event severity. (ret=%d)\n", ret);
    } else {
        exception_node->event.severity = (unsigned char)severity;
    }

    return DRV_ERROR_NONE;
}


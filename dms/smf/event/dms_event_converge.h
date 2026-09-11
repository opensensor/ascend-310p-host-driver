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

#ifndef __DMS_EVENT_CONVERGE_H__
#define __DMS_EVENT_CONVERGE_H__

#include "devmng_shm_info.h"
#include "dms_event_distribute.h"

#define DMS_EVENT_CODE_ENVIRONMENT_HOST (0x1U << 30)
#define DMS_EVENT_CODE_ENVIRONMENT_DEVICE (0x2U << 30)
#define DMS_EVENT_OBJ_NODETYPE_TO_CODE(nodetype) (((nodetype) & 0x7ff) << 17)
#define DMS_EVENT_OBJ_SENSOR_TYPE_TO_CODE(sensor_type) (((sensor_type) & 0xff) << 9)
#define DMS_EVENT_OBJ_EVENT_STATE_TO_CODE(event_state) (((event_state) & 0x1ff) << 0)

#define DMS_EVENT_ID_TO_NODE_TYPE(event_id) (((event_id) >> 17) & 0x7ffU)
#define DMS_EVENT_ID_TO_SENSOR_TYPE(event_id) (((event_id) >> 9) & 0xffU)
#define DMS_EVENT_ID_TO_EVENT_STATE(event_id) ((event_id) & 0x1ffU)
#define DMS_EVENT_ID_IS_BBOX_CODE(event_id) ((event_id) & (0x3U << 28U)) /* bit28~29 is used for bbox code judgement */

#define DMS_EVENT_CODE_GET_SERVERITY_ASSERTION(code) ((code) & (0x1fU << 25U))
#define DMS_EVENT_ERROR_CODE_TO_SEVERITY(code) (((code) >> 25U) & 0x7U)

#define DMS_EVENT_NOTIFY_NUM_MASK_HOST (0x1U << 30U)

#define EVENT_CONVERGE_ID_ADD_CONVERGE_FLAG(event_id) ((event_id) | (1U << 28))
#define EVENT_CONVERGE_ID_NUM_MIN (1U + 2U)
#define EVENT_CONVERGE_ID_NUM_MAX (128U + 2U)
#define EVENT_CONVERGE_DIAGRAMS_LAYER_MAX 3U

struct dms_mask_event {
    struct list_head node;
    unsigned int event_id;
};

struct dms_event_sensor_reported {
    struct list_head node;
    unsigned int event_id;
    int pid;
    int main_event_serial;
    unsigned int event_serial_cnt;
    int sub_event_serial[DMS_MAX_EVENT_NUM];
};

enum {
    EVENT_CONVERGE_NODE_ENABLE = 0,
    EVENT_CONVERGE_NODE_DISENABLE
};

typedef struct dms_event_converge_node {
    struct dms_event_converge_node *parent;
    struct dms_event_converge_node *child_head;
    struct dms_event_converge_node *bro_next;
    struct hlist_node hnode;
    unsigned int event_id;

    int event_serial_num[DEVICE_NUM_MAX];
    unsigned char assertion[DEVICE_NUM_MAX]; /* 0:RESUME 1:OCCUR 2:ONE_TIME */
    unsigned char mask[DEVICE_NUM_MAX]; /* 0:enable 1:disable */
    DMS_EVENT_NODE_STRU *exception_data[DEVICE_NUM_MAX];
} EVENT_CONVERGE_NODE_T;

int dms_event_convergent_item_init(unsigned int item_converge_id[], unsigned int event_id_num);

#define EVENT_CONVERGE_ITEM_DECLARATION(converge_id, ...)                       \
    unsigned int _dms_event_converge_item_##converge_id[] = {                   \
        converge_id, ##__VA_ARGS__, converge_id                                 \
    }

#define EVENT_CONVERGE_ITEM_INIT(converge_id)                                   \
do {                                                                            \
    if (dms_event_convergent_item_init(_dms_event_converge_item_##converge_id,  \
        sizeof(_dms_event_converge_item_##converge_id) /                        \
        sizeof(_dms_event_converge_item_##converge_id[0])) != 0) {              \
        return DRV_ERROR_INNER_ERR;                                             \
    }                                                                           \
} while (0)

#define EVENT_CONVERGE_DECLARATION_BEGIN()                                      \
    static int _dms_event_convergent_diagrams_init(void) {

#define EVENT_CONVERGE_DECLARATION_END()                                        \
        return DRV_ERROR_NONE;                                                  \
    }

#define EVENT_CONVERGE_CFG_INIT()                                               \
    _dms_event_convergent_diagrams_init()

int dms_event_code_validity_check(u32 event_code);
int dms_event_obj_to_error_code(struct dms_event_obj event_obj, u32 *error_code);
int dms_event_id_to_error_string(u32 devid, u32 event_id, char *event_str, u32 name_len);
int dms_event_converge_to_exception(struct dms_event_obj *event_obj, DMS_EVENT_NODE_STRU *exception_node);
int dms_event_converge_add_to_event_cb(DMS_EVENT_NODE_STRU *exception_node);
int dms_get_code_from_sensor_check(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len);
int dms_get_event_code_from_event_cb(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len);
int dms_get_event_code_from_sensor(u32 devid, u32 *health_code, u32 health_len,
    struct shm_event_code *event_code, u32 event_len);
int dms_event_is_converge(void);
int dms_event_get_notify_serial_num(void);
int dms_event_convergent_diagrams_init(void);
void dms_event_convergent_diagrams_exit(void);
int dms_event_convergent_diagrams_clear(u32 devid, bool hotreset);
void dms_event_convergent_diagrams_mask(u32 devid, u32 event_code, u8 mask);
void dms_event_mask_del_to_event_cb(u32 phyid, struct dms_dev_ctrl_block *dev_cb, u32 event_id);
void dms_event_mask_add_to_event_cb(u32 phyid, struct dms_dev_ctrl_block *dev_cb, u32 event_id);
void dms_event_add_to_mask_list(struct dms_dev_ctrl_block *dev_cb, u32 event_id);
void dms_event_del_to_mask_list(struct dms_dev_ctrl_block *dev_cb, u32 event_id);
void dms_event_mask_list_clear(struct dms_dev_ctrl_block *dev_cb);
void dms_event_ctrl_converge_list_free(struct dms_converge_event_list *event_list);
void dms_event_sensor_reported_list_free(struct dms_sensor_reported_list *reported_list);
int dms_event_obj_to_exception(struct dms_event_obj *event_obj,
    DMS_EVENT_NODE_STRU *exception_node);
unsigned int dms_event_get_owner_node_type(const struct dms_event_obj *event_obj);
unsigned int dms_event_get_owner_node_id(const struct dms_event_obj *event_obj);
#endif

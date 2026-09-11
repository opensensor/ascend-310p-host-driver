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

#include "dms_sensor.h"
#include "dms_define.h"
#include "dms_sensor_general.h"
#include "dms_sensor_statis.h"


unsigned int dms_sensor_class_init_statis(struct dms_sensor_object_cb *psensor_obj)
{
    psensor_obj->class_cb.statistic_cb.alarm_clear_times = 0;
    psensor_obj->class_cb.statistic_cb.status_counter = 0;
    psensor_obj->class_cb.statistic_cb.stat_time_counter = 0;
    psensor_obj->class_cb.statistic_cb.current_bit_count = 0;
    psensor_obj->class_cb.statistic_cb.object_op_state_chg_cause = 0;
    return 0;
}
unsigned int dms_process_statis_result_report(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cb *psensor_obj_cb, struct dms_sensor_event_data *pevent_data)
{
    /* Whether the sensor has a status change flag */
    unsigned int status, result;
    struct dms_sensor_event_data_item event_item;
    unsigned char event_data[4];
    unsigned int severity = 0;
    int rec;

    event_data[0] = 0x0F;
    event_data[1] = DMS_ES_UNSPECIFIED;
    status = DMS_SENSOR_STATUS_NO_CHANGE;

    rec = memcpy_s(psensor_obj_cb->event_paras, DMS_MAX_EVENT_DATA_LENGTH, pevent_data->sensor_data[0].event_data,
        pevent_data->sensor_data[0].data_size);
    if (rec != DRV_ERROR_NONE) {
        dms_err("memcpy_s failed. (ret=%d)\n", rec);
        return false;
    }

    psensor_obj_cb->paras_len = pevent_data->sensor_data[0].data_size;
    psensor_obj_cb->current_value = pevent_data->sensor_data[0].current_value;

    result = dms_process_stat_sensor_check_result(psensor_obj_cb, &status, event_data);
    if (result != DRV_ERROR_NONE) {
        dms_err("dms_process_statis_result_report: Sensor Check Time Result Process Failed! (sensor_type=%u)\n",
            psensor_obj_cb->sensor_object_cfg.sensor_type);
        return false;
    }
    event_item.current_value = event_data[0];
    if ((pevent_data->sensor_data[0].data_size) > 0 &&
        (pevent_data->sensor_data[0].data_size < DMS_MAX_EVENT_DATA_LENGTH)) {
        event_item.data_size = pevent_data->sensor_data[0].data_size;
        rec = memcpy_s(event_item.event_data, DMS_MAX_EVENT_DATA_LENGTH, pevent_data->sensor_data[0].event_data,
            pevent_data->sensor_data[0].data_size);
        if (rec != DRV_ERROR_NONE) {
            dms_err("memcpy_s failed. (ret=%d)\n", rec);
            return false;
        }
    }

    /* Determine whether an event needs to be generated */
    if (status == DMS_SENSOR_STATUS_CHANGED) {
        /* »Ö¸´ÏÖÓÐÊÂ¼þ£¬ÃÅÏÞ´«¸ÐÆ÷Ö»ÓÐÒ»¸öÊÂ¼þ */
        result = dms_resume_all_sensor_event(psensor_obj_cb);
        if (result != DRV_ERROR_NONE) {
            dms_err("dms_process_statis_result_report resume all sensor event failed. (ret=%u)\n", result);
            return result;
        }
        /* the event is mask */
        if (!dms_sensor_check_mask_enable(psensor_obj_cb->sensor_object_cfg.assert_event_mask,
            (unsigned int)event_data[0])) {
            return DRV_ERROR_NONE;
        }
        /* Ìí¼ÓÐÂµÄÊÂ¼þ */
        rec = dms_get_event_severity(psensor_obj_cb->owner_node_type, psensor_obj_cb->sensor_object_cfg.sensor_type,
            pevent_data->sensor_data[0].current_value, &severity);
        if (rec != DRV_ERROR_NONE) {
            dms_err("dms_add_one_sensor_event report event failed. (ret=%u; severity=%u)\n", rec, severity);
            return rec;
        }
        psensor_obj_cb->fault_status =
            psensor_obj_cb->fault_status > severity ? psensor_obj_cb->fault_status : severity;
        (void)dms_add_one_sensor_event(psensor_obj_cb, &event_item);
    }
    return 0;
}

static unsigned int dms_sensor_class_check_statis_occur_thres(struct dms_statistic_sensor *psensor_statis)
{
    unsigned int occur_type;

    /* ÅÐ¶Ï²úÉúÃÅÏÞÀàÐÍÊÇ·ñºÏ·¨ */
    occur_type = psensor_statis->occur_thres_type;
    if ((occur_type != DMS_SENSOR_OCCUR_THRES_TYPE_PERIOD) && (occur_type != DMS_SENSOR_OCCUR_THRES_TYPE_CONTINUED)) {
        /* ´òÓ¡´íÎóÐÅÏ¢:ÃÅÏÞÀàÐÍ´íÎó */
        dms_err("dms_sensor_class_check_statis: Occur Threshold Type[%u] Error!\n", occur_type);
        return DRV_ERROR_PARA_ERROR;
    }

    /* ÅÐ¶Ï×î´ó²úÉúÃÅÏÞ¡¢×îÐ¡²úÉúÃÅÏÞÊÇ·ñºÏ·¨(²»ÄÜµÈÓÚ0£¬ÇÒ×îÐ¡²úÉúÃÅÏÞ²»ÄÜ´óÓÚ×î´ó²úÉúÃÅÏÞ) */
    if ((psensor_statis->max_occur_thres == 0) || (psensor_statis->min_occur_thres == 0) ||
        (psensor_statis->min_occur_thres > psensor_statis->max_occur_thres)) {
        /* ´òÓ¡´íÎóÐÅÏ¢:²úÉúÃÅÏÞ·Ç·¨ */
        dms_err("dms_sensor_class_check_statis: Invalid Occur Threshold!\n");
        dms_err("Max Occur Threshold - %u\n", psensor_statis->max_occur_thres);
        dms_err("Min Occur Threshold - %u\n", psensor_statis->min_occur_thres);
        return DRV_ERROR_PARA_ERROR;
    }

    /* ÅÐ¶ÏÖ÷°å²úÉúÃÅÏÞÊÇ·ñÔÚºÏ·¨·¶Î§ÄÚ */
    if ((psensor_statis->occur_thres > psensor_statis->max_occur_thres) ||
        (psensor_statis->occur_thres < psensor_statis->min_occur_thres)) {
        /* ´òÓ¡´íÎóÐÅÏ¢:Ö÷°å²úÉúÃÅÏÞ·Ç·¨ */
        dms_err("dms_sensor_class_check_statis: Invalid Master Occur Threshold - %u\n", psensor_statis->occur_thres);
        return DRV_ERROR_PARA_ERROR;
    }

    if (occur_type == DMS_SENSOR_OCCUR_THRES_TYPE_PERIOD) {
        /* ÅÐ¶Ï²úÉúÃÅÏÞÍ³¼ÆÖÜÆÚÊÇ·ñÔÚºÏ·¨·¶Î§ÄÚ */
        if ((psensor_statis->occur_stat_time > psensor_statis->max_stat_time) ||
            (psensor_statis->occur_stat_time < psensor_statis->min_stat_time)) {
            /* ´òÓ¡´íÎóÐÅÏ¢:²úÉúÃÅÏÞÍ³¼ÆÖÜÆÚ·Ç·¨ */
            dms_err("dms_sensor_class_check_statis:  Invalid Occur Threshold Statistic Time - %u\n",
                psensor_statis->occur_stat_time);
            return DRV_ERROR_PARA_ERROR;
        }
        /* ×î´ó²úÉúÃÅÏÞ´óÓÚ²úÉúÃÅÏÞÍ³¼ÆÖÜÆÚÒ²ÊÇÒ»ÖÖ·Ç·¨Çé¿ö */
        if (psensor_statis->max_occur_thres > psensor_statis->occur_stat_time) {
            /* ´òÓ¡´íÎóÐÅÏ¢:×î´ó²úÉúÃÅÏÞ´óÓÚ²úÉúÃÅÏÞÍ³¼ÆÖÜÆÚ */
            dms_err("dms_sensor_class_check_statis:   Max Occur Threshold Larger than Occur Statistic Time\n");
            dms_err("Max Occur Threshold - %u\n", psensor_statis->max_occur_thres);
            dms_err("Occur Statistic Time  - %u\n", psensor_statis->occur_stat_time);
            return DRV_ERROR_PARA_ERROR;
        }
    }

    return DRV_ERROR_NONE;
}

static unsigned int dms_sensor_class_check_statis_resume_thres(struct dms_statistic_sensor *psensor_statis)
{
    unsigned int resume_type;

    /* ÅÐ¶Ï»Ö¸´ÃÅÏÞÀàÐÍÊÇ·ñºÏ·¨ */
    resume_type = psensor_statis->resume_thres_type;
    if ((resume_type != DMS_SENSOR_RESUME_THRES_TYPE_PERIOD) &&
        (resume_type != DMS_SENSOR_RESUME_THRES_TYPE_CONTINUED)) {
        /* ´òÓ¡´íÎóÐÅÏ¢:ÃÅÏÞÀàÐÍ´íÎó */
        dms_err("dms_sensor_class_check_statis: Resume Threshold Type[%u] Error!\n", resume_type);
        return DRV_ERROR_PARA_ERROR;
    }

    /* ÅÐ¶Ï×î´ó»Ö¸´ÃÅÏÞ¡¢×îÐ¡»Ö¸´ÃÅÏÞÊÇ·ñºÏ·¨(²»ÄÜµÈÓÚ0£¬ÇÒ×îÐ¡»Ö¸´ÃÅÏÞ²»ÄÜ´óÓÚ×î´ó»Ö¸´ÃÅÏÞ) */
    if ((psensor_statis->max_resume_thres == 0) || (psensor_statis->min_resume_thres == 0) ||
        (psensor_statis->min_resume_thres > psensor_statis->max_resume_thres)) {
        /* ´òÓ¡´íÎóÐÅÏ¢:»Ö¸´ÃÅÏÞ·Ç·¨ */
        dms_err("dms_sensor_class_check_statis:  Invalid Resume Threshold!\n");
        dms_err("Max Resume Threshold - %u\n", psensor_statis->max_resume_thres);
        dms_err("Min Resume Threshold - %u\n", psensor_statis->min_resume_thres);
        return DRV_ERROR_PARA_ERROR;
    }

    /* ÅÐ¶Ï»Ö¸´ÃÅÏÞÊÇ·ñÔÚºÏ·¨·¶Î§ÄÚ */
    if ((psensor_statis->resume_thres > psensor_statis->max_resume_thres) ||
        (psensor_statis->resume_thres < psensor_statis->min_resume_thres)) {
        /* ´òÓ¡´íÎóÐÅÏ¢:Ö÷°å»Ö¸´ÃÅÏÞ·Ç·¨ */
        dms_err("dms_sensor_class_check_statis: Invalid Master Resume Threshold - %u\n", psensor_statis->resume_thres);
        return DRV_ERROR_PARA_ERROR;
    }

    if (resume_type == DMS_SENSOR_RESUME_THRES_TYPE_PERIOD) {
        /* ÅÐ¶Ï»Ö¸´ÃÅÏÞÍ³¼ÆÖÜÆÚÊÇ·ñÔÚºÏ·¨·¶Î§ÄÚ */
        if ((psensor_statis->resume_stat_time > psensor_statis->max_stat_time) ||
            (psensor_statis->resume_stat_time < psensor_statis->min_stat_time)) {
            /* ´òÓ¡´íÎóÐÅÏ¢:»Ö¸´ÃÅÏÞÍ³¼ÆÖÜÆÚ·Ç·¨ */
            dms_err("dms_sensor_class_check_statis: Invalid Resume Threshold Statistic Time - %u\n",
                psensor_statis->resume_stat_time);
            return DRV_ERROR_PARA_ERROR;
        }
        /* ×î´ó»Ö¸´ÃÅÏÞ´óÓÚ»Ö¸´ÃÅÏÞÍ³¼ÆÖÜÆÚÒ²ÊÇÒ»ÖÖ·Ç·¨Çé¿ö */
        if (psensor_statis->max_resume_thres > psensor_statis->resume_stat_time) {
            dms_err("dms_sensor_class_check_statis: Max Resume Threshold Larger than Resume Statistic Time!\n");
            dms_err("Max Resume Threshold - %u\n", psensor_statis->max_resume_thres);
            dms_err("Resume Statistic Time - %u\n", psensor_statis->resume_stat_time);
            return DRV_ERROR_PARA_ERROR;
        }
    }

    return DRV_ERROR_NONE;
}

/* *************************************************************************
Function:        unsigned int dms_sensor_class_check_statis(struct dms_sensor_type *psensor_type)
Description:     ¼ì²éÍ³¼Æ´«¸ÐÆ÷ÀàÐÍ¸ñÊ½ÊÇ·ñÕýÈ·
************************************************************************ */
unsigned int dms_sensor_class_check_statis(struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    unsigned int ret;
    unsigned int occur_type, resume_type;
    struct dms_statistic_sensor *psensor_statis;
    psensor_statis = (struct dms_statistic_sensor *)&(psensor_obj_cfg->sensor_class_cfg.statistic_sensor);
    /* ÅÐ¶Ï´«¸ÐÆ÷ÊôÐÔÊÇ·ñºÏ·¨ */
    if ((psensor_statis->attribute != DMS_SENSOR_ATTRIB_THRES_SET_ENABLE) &&
        (psensor_statis->attribute != DMS_SENSOR_ATTRIB_THRES_NONE)) {
        /* ´òÓ¡´íÎóÐÅÏ¢:ÊôÐÔ·Ç·¨ */
        dms_err("dms_sensor_class_check_statis: Invalid Sensor Attribute - %u SensorType is %u\n",
            psensor_statis->attribute, psensor_obj_cfg->sensor_type);
        return DRV_ERROR_PARA_ERROR;
    }

    /* Ö»ÓÐÖÜÆÚÍ³¼ÆÐÍ´«¸ÐÆ÷ÐèÒªÅÐ¶ÏÍ³¼ÆÖÜÆÚÊÇ·ñºÏ·¨ */
    occur_type = psensor_statis->occur_thres_type;
    resume_type = psensor_statis->resume_thres_type;
    if ((occur_type == DMS_SENSOR_OCCUR_THRES_TYPE_PERIOD) || (resume_type == DMS_SENSOR_RESUME_THRES_TYPE_PERIOD)) {
        /* ÅÐ¶Ï×î´óÍ³¼ÆÖÜÆÚ¡¢×îÐ¡Í³¼ÆÖÜÆÚÊÇ·ñºÏ·¨(²»ÄÜµÈÓÚ0£¬ÇÒ×îÐ¡Í³¼ÆÖÜÆÚ²»ÄÜ´óÓÚ×î´óÍ³¼ÆÖÜÆÚ) */
        if ((psensor_statis->max_stat_time == 0) || (psensor_statis->min_stat_time == 0) ||
            (psensor_statis->min_stat_time > psensor_statis->max_stat_time)) {
            /* ´òÓ¡´íÎóÐÅÏ¢:Í³¼ÆÖÜÆÚ·Ç·¨ */
            dms_err("dms_sensor_class_check_statis:  Invalid Statistic Time! SensorType is %u\n",
                psensor_obj_cfg->sensor_type);
            dms_err("Max Statistic Time - %u\n", psensor_statis->max_stat_time);
            dms_err("Min Statistic Time - %u\n", psensor_statis->min_stat_time);
            return DRV_ERROR_PARA_ERROR;
        }
    }

    ret = dms_sensor_class_check_statis_occur_thres(psensor_statis);
    if (ret != 0) {
        dms_err("Dms check statis occur threshold fail.(ret=%u, sensor_type=%u)\n", ret, psensor_obj_cfg->sensor_type);
        return ret;
    }

    ret = dms_sensor_class_check_statis_resume_thres(psensor_statis);
    if (ret != 0) {
        dms_err("Dms check statis resume threshold fail.(ret=%u, sensor_type=%u)\n", ret, psensor_obj_cfg->sensor_type);
        return ret;
    }

    return DRV_ERROR_NONE;
}

unsigned int dms_process_stat_sensor_check_result(struct dms_sensor_object_cb *pobject, unsigned int *status,
    unsigned char *event_type)
{
    *status = DMS_SENSOR_STATUS_CHANGED;
    return DRV_ERROR_NONE;
}

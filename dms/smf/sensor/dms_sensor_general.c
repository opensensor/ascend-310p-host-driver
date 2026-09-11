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

#include "dms_define.h"
#include "dms_sensor.h"
#include "dms_sensor_general.h"

static int dms_get_offset_from_fault_status(unsigned int fault_status)
{
    int trans_table[] = {
        DMS_SENSOR_FAULT_LEVEL_LOW_MINOR,    DMS_SENSOR_FAULT_LEVEL_LOW_MAJOR,
        DMS_SENSOR_FAULT_LEVEL_LOW_CRITICAL, DMS_SENSOR_FAULT_LEVEL_UP_MINOR,
        DMS_SENSOR_FAULT_LEVEL_UP_MAJOR,     DMS_SENSOR_FAULT_LEVEL_UP_CRITICAL
    };

    if ((fault_status >= DMS_GEN_SEN_STATUS_LOW_MINOR_FAULT) &&
        (fault_status <= DMS_GEN_SEN_STATUS_UP_CRITICAL_FAULT)) {
        return trans_table[fault_status - DMS_GEN_SEN_STATUS_LOW_MINOR_FAULT];
    }

    dms_debug("(fault_status: %u)\n", fault_status);

    // Normally, It shouldn't run here
    return DMS_SENSOR_FAULT_LEVEL_LOW_MINOR;
}

unsigned int dms_sensor_class_init_general(struct dms_sensor_object_cb *psensor_obj)
{
    return DRV_ERROR_NONE;
}

static void dms_get_general_sensor_fault_type(int current_value, unsigned int *fault_status,
    const struct dms_general_sensor thres_info)
{
    if ((current_value > thres_info.up_minor) && (current_value <= thres_info.up_major)) {
        *fault_status = DMS_GEN_SEN_STATUS_UP_MINOR_FAULT;
    } else if ((current_value >= thres_info.low_major) && (current_value < thres_info.low_minor)) {
        *fault_status = DMS_GEN_SEN_STATUS_LOW_MINOR_FAULT;
    } else if ((current_value > thres_info.up_major) && (current_value <= thres_info.up_critical)) {
        *fault_status = DMS_GEN_SEN_STATUS_UP_MAJOR_FAULT;
    } else if ((current_value >= thres_info.low_critical) && (current_value < thres_info.low_major)) {
        *fault_status = DMS_GEN_SEN_STATUS_LOW_MAJOR_FAULT;
    } else if (current_value > thres_info.up_critical) {
        *fault_status = DMS_GEN_SEN_STATUS_UP_CRITICAL_FAULT;
    } else if (current_value < thres_info.low_critical) {
        *fault_status = DMS_GEN_SEN_STATUS_LOW_CRITICAL_FAULT;
    } else if ((current_value <= thres_info.up_minor) && (current_value >= thres_info.low_minor)) {
        *fault_status = DMS_GEN_SEN_STATUS_GOOD;
    } else {
        /* Normally, It's impossible to run here */
    }

    return;
}

static __inline unsigned int dms_sensor_check_status_good(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    if ((curr_value <= thres_info->up_minor) && (curr_value >= thres_info->low_minor)) {
        return DMS_SENSOR_STATUS_NO_CHANGE;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static __inline unsigned int dms_sensor_check_status_low_minor(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    if (((thres_info->thres_series & DMS_SENSOR_THRES_LOW_MINOR_SERIES) != 0) &&
        (curr_value < (thres_info->low_minor + thres_info->pos_thd_hysteresis)) &&
        (curr_value >= thres_info->low_major)) {
        return DMS_SENSOR_STATUS_NO_CHANGE;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static __inline unsigned int dms_sensor_check_status_low_major(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    if (((thres_info->thres_series & DMS_SENSOR_THRES_LOW_MAJOR_SERIES) != 0) &&
        (curr_value < (thres_info->low_major + thres_info->pos_thd_hysteresis)) &&
        (curr_value >= thres_info->low_critical)) {
        return DMS_SENSOR_STATUS_NO_CHANGE;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static __inline unsigned int dms_sensor_check_status_low_critical(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    if (((thres_info->thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) != 0) &&
        (curr_value < (thres_info->low_critical + thres_info->pos_thd_hysteresis))) {
        dms_debug("!!!!!!not changed, (low_critical: %d, pos_thd_hysteresis: %d)\n",
            thres_info->low_critical, thres_info->pos_thd_hysteresis);
        return DMS_SENSOR_STATUS_NO_CHANGE;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static __inline unsigned int dms_sensor_check_status_up_minor(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    if (((thres_info->thres_series & DMS_SENSOR_THRES_UP_MINOR_SERIES) != 0) &&
        (curr_value <= thres_info->up_major) &&
        (curr_value > thres_info->up_minor - thres_info->neg_thd_hysteresis)) {
        return DMS_SENSOR_STATUS_NO_CHANGE;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static __inline unsigned int dms_sensor_check_status_up_major(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    if (((thres_info->thres_series & DMS_SENSOR_THRES_UP_MAJOR_SERIES) != 0) &&
        (curr_value <= thres_info->up_critical) &&
        (curr_value > thres_info->up_major - thres_info->neg_thd_hysteresis)) {
        return DMS_SENSOR_STATUS_NO_CHANGE;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static __inline unsigned int dms_sensor_check_status_up_critical(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    if (((thres_info->thres_series & DMS_SENSOR_THRES_UP_MAJOR_SERIES) != 0) &&
        (curr_value > thres_info->up_critical - thres_info->neg_thd_hysteresis)) {
        return DMS_SENSOR_STATUS_NO_CHANGE;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static int dms_get_sensor_status_changed(const struct dms_general_sensor *thres_info,
    unsigned int pre_status, int curr_value)
{
    dms_debug("(pre_status: %u, curr_value: %d)\n", pre_status, curr_value);
    switch (pre_status) {
        case DMS_GEN_SEN_STATUS_GOOD:
            return dms_sensor_check_status_good(thres_info, pre_status, curr_value);
        case DMS_GEN_SEN_STATUS_LOW_MINOR_FAULT:
            return dms_sensor_check_status_low_minor(thres_info, pre_status, curr_value);
        case DMS_GEN_SEN_STATUS_LOW_MAJOR_FAULT:
            return dms_sensor_check_status_low_major(thres_info, pre_status, curr_value);
        case DMS_GEN_SEN_STATUS_LOW_CRITICAL_FAULT:
            return dms_sensor_check_status_low_critical(thres_info, pre_status, curr_value);
        case DMS_GEN_SEN_STATUS_UP_MINOR_FAULT:
            return dms_sensor_check_status_up_minor(thres_info, pre_status, curr_value);
        case DMS_GEN_SEN_STATUS_UP_MAJOR_FAULT:
            return dms_sensor_check_status_up_major(thres_info, pre_status, curr_value);
        case DMS_GEN_SEN_STATUS_UP_CRITICAL_FAULT:
            return dms_sensor_check_status_up_critical(thres_info, pre_status, curr_value);
        default:
            break;
    }

    return DMS_SENSOR_STATUS_CHANGED;
}

static int dms_process_general_sensor_check_result(struct dms_sensor_object_cb *pobject,
    unsigned int *status, int *event_offset)
{
    unsigned int new_fault_status;
    struct dms_general_sensor thres_info = { 0 };
    unsigned int pre_status;

    if (pobject == NULL || status == NULL || event_offset == NULL) {
        dms_err("NULL pointer. (pobject=%d; status=%d; event_offset=%d)\n", pobject != NULL, status != NULL,
            event_offset != NULL);
        return DRV_ERROR_INVALID_VALUE;
    }

    pre_status = pobject->class_cb.general_cb.pre_status;
    if (pre_status > DMS_GEN_SEN_STATUS_UP_CRITICAL_FAULT) {
        dms_err("invalid fault status:%u\n", pobject->fault_status);
        return DRV_ERROR_INVALID_VALUE;
    }

    thres_info.up_minor = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.up_minor;
    thres_info.up_major = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.up_major;
    thres_info.up_critical = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.up_critical;
    thres_info.low_minor = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.low_minor;
    thres_info.low_major = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.low_major;
    thres_info.low_critical = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.low_critical;
    thres_info.neg_thd_hysteresis = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.neg_thd_hysteresis;
    thres_info.pos_thd_hysteresis = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.pos_thd_hysteresis;
    thres_info.thres_series = pobject->sensor_object_cfg.sensor_class_cfg.general_sensor.thres_series;

    if ((thres_info.thres_series & DMS_GENERAL_SENSOR_THRESSERIES_MASK) == 0) {
        dms_err("threshold series[0x%x] error!\n", thres_info.thres_series);
        return DRV_ERROR_INVALID_VALUE;
    }

    new_fault_status = pre_status;
    *status = dms_get_sensor_status_changed(&thres_info, pre_status, pobject->current_value);
    if (*status == DMS_SENSOR_STATUS_CHANGED) {
        dms_get_general_sensor_fault_type(pobject->current_value, &new_fault_status, thres_info);
        pobject->class_cb.general_cb.pre_status = new_fault_status;
        *event_offset = dms_get_offset_from_fault_status(new_fault_status);
    }

    dms_debug("general sensor check finish, (curr_val: %d, change_status: %u, event_offset: %d, fault_status: %u)\n",
        pobject->current_value, *status, *event_offset, pobject->class_cb.general_cb.pre_status);

    return DRV_ERROR_NONE;
}

static void dms_adjust_general_sensor_up_thres(struct dms_general_sensor *thres_info)
{
    unsigned int thres_series = thres_info->thres_series;

    if ((thres_series & DMS_SENSOR_THRES_UP_MINOR_SERIES) == 0) {
        if ((thres_series & DMS_SENSOR_THRES_UP_MAJOR_SERIES) == 0) {
            if ((thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) == 0) {
                /* All up thresholds are not supported */
                thres_info->up_critical = DMS_GENERAL_SENSOR_MAX_THRES;
                thres_info->up_major = DMS_GENERAL_SENSOR_MAX_THRES;
                thres_info->up_minor = DMS_GENERAL_SENSOR_MAX_THRES;
                thres_info->neg_thd_hysteresis = 0;
            } else {
                /* Only up critical faults are supported */
                thres_info->up_major = thres_info->up_critical;
                thres_info->up_minor = thres_info->up_critical;
            }
        } else {
            if ((thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) == 0) {
                /* Only up major faults are supported */
                thres_info->up_critical = DMS_GENERAL_SENSOR_MAX_THRES;
                thres_info->up_minor = thres_info->up_major;
            } else {
                /* Only up major and up up_critical faults are supported */
                thres_info->up_minor = thres_info->up_major;
            }
        }
    } else {
        if ((thres_series & DMS_SENSOR_THRES_UP_MAJOR_SERIES) == 0) {
            if ((thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) == 0) {
                /* Only up minor faults are supported */
                thres_info->up_critical = DMS_GENERAL_SENSOR_MAX_THRES;
                thres_info->up_major = DMS_GENERAL_SENSOR_MAX_THRES;
            } else {
                /* Only up minor and up critical faults are supported */
                thres_info->up_major = thres_info->up_critical;
            }
        } else {
            if ((thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) == 0) {
                /* Only up minor and up major faults are supported */
                thres_info->up_critical = DMS_GENERAL_SENSOR_MAX_THRES;
            } else {
                /* Up threshold all support */
            }
        }
    }
}

static void dms_adjust_general_sensor_low_thres(struct dms_general_sensor *thres_info)
{
    unsigned int thres_series = thres_info->thres_series;

    if ((thres_series & DMS_SENSOR_THRES_LOW_MINOR_SERIES) == 0) {
        if ((thres_series & DMS_SENSOR_THRES_LOW_MAJOR_SERIES) == 0) {
            if ((thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) == 0) {
                /* All low thresholds are not supported */
                thres_info->low_critical = DMS_GENERAL_SENSOR_MIN_THRES;
                thres_info->low_major = DMS_GENERAL_SENSOR_MIN_THRES;
                thres_info->low_minor = DMS_GENERAL_SENSOR_MIN_THRES;

                thres_info->pos_thd_hysteresis = 0;
            } else {
                /* Only low critical faults are supported */
                thres_info->low_major = thres_info->low_critical;
                thres_info->low_minor = thres_info->low_critical;
            }
        } else {
            if ((thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) == 0) {
                /* Only low major faults are supported */
                thres_info->low_critical = DMS_GENERAL_SENSOR_MIN_THRES;
                thres_info->low_minor = thres_info->low_major;
            } else {
                /* Only low major and low critical faults are supported */
                thres_info->low_minor = thres_info->low_major;
            }
        }
    } else {
        if ((thres_series & DMS_SENSOR_THRES_LOW_MAJOR_SERIES) == 0) {
            if ((thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) == 0) {
                /* Only low minor faults are supported */
                thres_info->low_critical = DMS_GENERAL_SENSOR_MIN_THRES;
                thres_info->low_major = DMS_GENERAL_SENSOR_MIN_THRES;
            } else {
                /* Only low minor and low critical faults are supported */
                thres_info->low_major = thres_info->low_critical;
            }
        } else {
            if ((thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) == 0) {
                /* Only low minor and low major faults are supported */
                thres_info->low_critical = DMS_GENERAL_SENSOR_MIN_THRES;
            } else {
                /* Low threshold all support */
            }
        }
    }
}

static void dms_adjust_general_sensor_thres(struct dms_general_sensor *thres_info)
{
    dms_adjust_general_sensor_up_thres(thres_info);
    dms_adjust_general_sensor_low_thres(thres_info);

    return ;
}

static int dms_compare_thres(int big_thres, int small_thres, int thdhysteresis)
{
    if (big_thres <= small_thres) {
        dms_err("Threshold NOT in Valid Scope!\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (thdhysteresis >= (big_thres - small_thres)) {
        dms_err("Threshold NOT in Valid Scope!\n");
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static int dms_thres_scope_check_up_series(const struct dms_general_sensor *thres_info)
{
    if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_MINOR_SERIES) != 0) {
        if ((thres_info->up_minor < thres_info->min_thres) || (thres_info->up_minor > thres_info->max_thres)) {
            dms_err("UpMinor Threshold NOT in Valid Scope!\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }

    if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_MAJOR_SERIES) != 0) {
        if ((thres_info->up_major < thres_info->min_thres) || (thres_info->up_major > thres_info->max_thres)) {
            dms_err("UpMajor Threshold NOT in Valid Scope!\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }

    if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) != 0) {
        if ((thres_info->up_critical < thres_info->min_thres) || (thres_info->up_critical > thres_info->max_thres)) {
            dms_err("UpCritical Threshold NOT in Valid Scope!\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }

    return DRV_ERROR_NONE;
}

static int dms_thres_scope_check_low_series(const struct dms_general_sensor *thres_info)
{
    if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_MINOR_SERIES) != 0) {
        if ((thres_info->low_minor < thres_info->min_thres) || (thres_info->low_minor > thres_info->max_thres)) {
            dms_err("LowMinor Threshold NOT in Valid Scope!\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }

    if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_MAJOR_SERIES) != 0) {
        if ((thres_info->low_major < thres_info->min_thres) || (thres_info->low_major > thres_info->max_thres)) {
            dms_err("LowMajor Threshold NOT in Valid Scope!\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }

    if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) != 0) {
        if ((thres_info->low_critical < thres_info->min_thres) || (thres_info->low_critical > thres_info->max_thres)) {
            dms_err("LowCritical Threshold NOT in Valid Scope!\n");
            return DRV_ERROR_PARA_ERROR;
        }
    }

    return DRV_ERROR_NONE;
}

static int dms_thres_scope_check(const struct dms_general_sensor *thres_info)
{
    int ret;

    ret = dms_thres_scope_check_up_series(thres_info);
    if (ret != DRV_ERROR_NONE) {
        dms_err("Up series thres scope check failed.(ret=%d)\n", ret);
        return ret;
    }

    ret = dms_thres_scope_check_low_series(thres_info);
    if (ret != DRV_ERROR_NONE) {
        dms_err("Low series thres scope check failed.(ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int dms_is_general_sonsor_up_thres_valid(const struct dms_general_sensor *thres_info)
{
    int result;

    if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_MINOR_SERIES) != 0) {
        if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_MAJOR_SERIES) != 0) {
            result = dms_compare_thres(thres_info->up_major, thres_info->up_minor, thres_info->neg_thd_hysteresis);
            if (result != DRV_ERROR_NONE) {
                dms_err("Compare UpMajor with UpMinor failed! result: %d\n", result);
                return result;
            }

            if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) != 0) {
                result =
                    dms_compare_thres(thres_info->up_critical, thres_info->up_major, thres_info->neg_thd_hysteresis);
                if (result != DRV_ERROR_NONE) {
                    dms_err("Compare UpCritical with UpMajor failed! result: %d\n", result);
                    return result;
                }
            }
        } else if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) != 0) {
            result = dms_compare_thres(thres_info->up_critical, thres_info->up_minor, thres_info->neg_thd_hysteresis);
            if (result != DRV_ERROR_NONE) {
                dms_err("Compare UpCritical with UpMinor failed! result: %d\n", result);
                return result;
            }
        }
    } else if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_MAJOR_SERIES) != 0) {
        if ((thres_info->thres_series & DMS_SENSOR_THRES_UP_CRITICAL_SERIES) != 0) {
            result = dms_compare_thres(thres_info->up_critical, thres_info->up_major, thres_info->neg_thd_hysteresis);
            if (result != DRV_ERROR_NONE) {
                dms_err("Compare UpCritical with UpMajor failed! result: %d\n", result);
                return result;
            }
        }
    } else {
        ;
    }

    return DRV_ERROR_NONE;
}

static int dms_is_general_sonsor_low_thres_valid(const struct dms_general_sensor *thres_info)
{
    int result;

    if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_MINOR_SERIES) != 0) {
        if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_MAJOR_SERIES) != 0) {
            result = dms_compare_thres(thres_info->low_minor, thres_info->low_major, thres_info->pos_thd_hysteresis);
            if (result != DRV_ERROR_NONE) {
                dms_err("Compare LowMinor with LowMajor failed! result: %d\n", result);
                return result;
            }

            if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) != 0) {
                result =
                    dms_compare_thres(thres_info->low_major, thres_info->low_critical, thres_info->pos_thd_hysteresis);
                if (result != DRV_ERROR_NONE) {
                    dms_err("Compare LowMajor with LowCritical failed! result: %d\n", result);
                    return result;
                }
            }
        } else if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) != 0) {
            result = dms_compare_thres(thres_info->low_minor, thres_info->low_critical, thres_info->pos_thd_hysteresis);
            if (result != DRV_ERROR_NONE) {
                dms_err("Compare LowMinor with LowCritical failed! result: %d\n", result);
                return result;
            }
        }
    } else if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_MAJOR_SERIES) != 0) {
        if ((thres_info->thres_series & DMS_SENSOR_THRES_LOW_CRITICAL_SERIES) != 0) {
            result = dms_compare_thres(thres_info->low_major, thres_info->low_critical, thres_info->pos_thd_hysteresis);
            if (result != DRV_ERROR_NONE) {
                dms_err("Compare LowMinor with LowMajor failed! result: %d\n", result);
                return result;
            }
        }
    } else {
        ;
    }

    return DRV_ERROR_NONE;
}


static int dms_is_general_sensor_thres_valid(const struct dms_general_sensor *thres_info)
{
    int result;

    if ((thres_info->thres_series & DMS_GENERAL_SENSOR_THRESSERIES_MASK) == 0) {
        dms_err("Threshold Series[0x%x] Error!\n", thres_info->thres_series);
        return DRV_ERROR_PARA_ERROR;
    }

    if (thres_info->min_thres >= thres_info->max_thres) {
        dms_err("Invalid Threshold! Min Threshold: %d, Max Threshold: %d\n", thres_info->min_thres,
            thres_info->max_thres);
        return DRV_ERROR_PARA_ERROR;
    }

    /* Hysteresis must be non negative */
    if ((thres_info->pos_thd_hysteresis < 0) || (thres_info->neg_thd_hysteresis < 0)) {
        dms_err("Invalid Threshold! PosThdHysteresis: %d, NegThdHysteresis: %d\n", thres_info->pos_thd_hysteresis,
            thres_info->neg_thd_hysteresis);
        return DRV_ERROR_PARA_ERROR;
    }

    /* case1: all threshold values are not between the maximum threshold and the minimum threshold */
    result = dms_thres_scope_check(thres_info);
    if (result != DRV_ERROR_NONE) {
        dms_err("thres scope check fail\n");
        return result;
    }

    /* case2: the upper threshold of the high level is less than the upper threshold of the low level */
    result = dms_is_general_sonsor_up_thres_valid(thres_info);
    if (result != DRV_ERROR_NONE) {
        dms_err("up thres check fail! result: %d\n", result);
        return result;
    }

    /* case3: The lower threshold of the high level is greater than the lower threshold of the low level */
    result = dms_is_general_sonsor_low_thres_valid(thres_info);
    if (result != DRV_ERROR_NONE) {
        dms_err("low thres check fail! result: %d\n", result);
        return result;
    }

    return DRV_ERROR_NONE;
}

int dms_sensor_thres_set(unsigned int sensor_type, const struct dms_general_sensor *thres_info)
{
    return DRV_ERROR_NONE;
}

int dms_sensor_class_check_general(struct dms_sensor_object_cfg *psensor_obj_cfg)
{
    int result;
    struct dms_general_sensor *thres_info = NULL;

    if (psensor_obj_cfg == NULL) {
        dms_err("NULL pointer error!\n");
        return DRV_ERROR_PARA_ERROR;
    }

    thres_info = &psensor_obj_cfg->sensor_class_cfg.general_sensor;
    if ((psensor_obj_cfg->sensor_class_cfg.general_sensor.attribute != DMS_SENSOR_ATTRIB_THRES_SET_ENABLE) &&
        (psensor_obj_cfg->sensor_class_cfg.general_sensor.attribute != DMS_SENSOR_ATTRIB_THRES_NONE)) {
        dms_err("Invalid sensor attribute, (attribute: 0x%x, sensor_type: 0x%x)\n",
            psensor_obj_cfg->sensor_class_cfg.general_sensor.attribute, psensor_obj_cfg->sensor_type);
        return DRV_ERROR_PARA_ERROR;
    }

    result = dms_is_general_sensor_thres_valid(thres_info);
    if (result != DRV_ERROR_NONE) {
        dms_err("Thres of general sensor invalid! (sensor_type: 0x%x, result: %d)\n",
            psensor_obj_cfg->sensor_type, result);
        return result;
    }

    dms_adjust_general_sensor_thres(thres_info);

    return DRV_ERROR_NONE;
}

static void get_new_event_list(DMS_EVENT_LIST_ITEM **pp_event_list, struct dms_sensor_object_cb *psen_obj_cb,
    struct dms_sensor_event_data_item *event_item)
{
    DMS_EVENT_LIST_ITEM *p_event = NULL;
    unsigned int fault_status, fault_status_start;

    fault_status = psen_obj_cb->class_cb.general_cb.pre_status;
    if (fault_status == DMS_GEN_SEN_STATUS_GOOD) {
        return;
    }

    if (fault_status >= DMS_GEN_SEN_STATUS_UP_MINOR_FAULT) {
        fault_status_start = DMS_GEN_SEN_STATUS_UP_MINOR_FAULT;
    } else {
        fault_status_start = DMS_GEN_SEN_STATUS_LOW_MINOR_FAULT;
    }

    for (; fault_status_start <= fault_status; fault_status_start++) {
        event_item->current_value = dms_get_offset_from_fault_status(fault_status_start);
        if (!dms_sensor_check_mask_enable(psen_obj_cb->sensor_object_cfg.assert_event_mask,
            event_item->current_value)) {
            dms_warn("event_offset is disable. (offset=%d; sen_num=%u; sen_type=%#x)\n", event_item->current_value,
                psen_obj_cb->sensor_num, psen_obj_cb->sensor_object_cfg.sensor_type);
            continue;
        }
        p_event = sensor_add_event_to_list(pp_event_list, event_item, psen_obj_cb);
        if (p_event == NULL) {
            dms_warn("sensor_add_event_to_list warn.\n");
            return;
        }
    }
}

unsigned int dms_process_general_result_report(struct dms_node_sensor_cb *node_sensor_cb,
    struct dms_sensor_object_cb *psen_obj_cb, struct dms_sensor_event_data *pevent_data)
{
    unsigned int status = DMS_SENSOR_STATUS_NO_CHANGE;
    struct dms_sensor_event_data_item event_item = {0};
    int result;
    int event_offset = 0;
    DMS_EVENT_LIST_ITEM *p_new_event_list = NULL;

    result = memcpy_s(psen_obj_cb->event_paras, DMS_MAX_EVENT_DATA_LENGTH, pevent_data->sensor_data[0].event_data,
        pevent_data->sensor_data[0].data_size);
    if (result != 0) {
        dms_err("memcpy_s fail, (result: %d)\n", result);
        return result;
    }
    psen_obj_cb->current_value = pevent_data->sensor_data[0].current_value;

    result = dms_process_general_sensor_check_result(psen_obj_cb, &status, &event_offset);
    if (result != DRV_ERROR_NONE) {
        dms_err("dms process general sensor check fail! (result: %d)\n", result);
        return result;
    }
    event_item.current_value = event_offset;
    if ((pevent_data->sensor_data[0].data_size > 0) &&
        (pevent_data->sensor_data[0].data_size <= DMS_MAX_EVENT_DATA_LENGTH)) {
        event_item.data_size = pevent_data->sensor_data[0].data_size;
        result = memcpy_s(event_item.event_data, DMS_MAX_EVENT_DATA_LENGTH, pevent_data->sensor_data[0].event_data,
            pevent_data->sensor_data[0].data_size);
        if (result != 0) {
            dms_err("memcpy_s fail, (result: %d)\n", result);
            return result;
        }
    }

    /* Determine whether an event needs to be generated */
    if (status == DMS_SENSOR_STATUS_CHANGED) {
        get_new_event_list(&p_new_event_list, psen_obj_cb, &event_item);
        (void)dms_update_sensor_list(psen_obj_cb, p_new_event_list);
    }

    return 0;
}


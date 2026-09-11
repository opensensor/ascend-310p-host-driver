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

#ifndef __DMS_TEMPLATE_H__
#define __DMS_TEMPLATE_H__

#include "dms_define.h"
#include "dms_feature.h"
#include "dms_dev_node.h"

#define FILTER_DEVICE_INFO "main_cmd=0x8"  // MAIN_CMD_LP
#define FILTER_TEMP_INFO "main_cmd=0x32"   // MAIN_CMD_TEMP

#define BEGIN_DMS_MODULE_DECLARATION(module) \
    static DMS_FEATURE_S *_feature_table = NULL; \
    static u32 _feature_table_size = 0; \
    static void _init_feature_table(void); \
    static int _init_module_##module(void) \
    { \
        int ret; \
        unsigned int i; \
        _init_feature_table(); \
        for (i = 0; i < _feature_table_size; i++) { \
            ret = dms_feature_register(&_feature_table[i]); \
            if (ret != 0) { \
                return ret; \
            } \
        } \
        return 0; \
    } \
    int init_module_##module(void); \
    int init_module_##module(void) \
    { \
        return _init_module_##module(); \
    } \
    static int _exit_module_##module(void) \
    { \
        int ret; \
        unsigned int i; \
        for (i = 0; i < _feature_table_size; i++) { \
            ret = dms_feature_unregister(&_feature_table[i]); \
            if (ret != 0) { \
                return ret; \
            } \
        } \
        return 0; \
    } \
    int exit_module_##module(void); \
    int exit_module_##module(void) \
    { \
        return _exit_module_##module(); \
    }

#define END_MODULE_DECLARATION() \

#define BEGIN_FEATURE_COMMAND() static DMS_FEATURE_S _feature_def[] = {
#define ADD_FEATURE_COMMAND(o, m, s, f, l, p, handler)  {o, m, s, f, l, p, handler},
#define END_FEATURE_COMMAND() }; \
    static void _init_feature_table(void) \
    { \
        _feature_table = _feature_def; \
        _feature_table_size = sizeof(_feature_def) / sizeof(_feature_def[0]); \
    }

#define INIT_MODULE_FUNC(module) \
    int init_module_##module(void)

#define EXIT_MODULE_FUNC(module) \
    int exit_module_##module(void)

#define CALL_INIT_MODULE(module) \
    (void)init_module_##module()

#define CALL_EXIT_MODULE(module) \
    (void)exit_module_##module()
#endif

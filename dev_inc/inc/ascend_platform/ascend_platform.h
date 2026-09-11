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
* Create: 2022-8-29
*/

#ifndef ASCEND_PLATFORM_H
#define ASCEND_PLATFORM_H

#ifdef CONFIG_PLATFORM_310
#include "ascend_platform_310.h"
#endif
#ifdef CONFIG_PLATFORM_310P
#include "ascend_platform_310P.h"
#endif
#ifdef CONFIG_PLATFORM_910
#include "ascend_platform_910.h"
#endif
#ifdef CONFIG_PLATFORM_310B
#include "ascend_platform_310B.h"
#endif
#ifdef CONFIG_PLATFORM_310M
#include "ascend_platform_310B.h"
#endif
#ifdef CONFIG_PLATFORM_910B
#include "ascend_platform_910B.h"
#endif
#ifdef CONFIG_PLATFORM_910_95
#include "ascend_platform_910_95.h"
#endif

#endif
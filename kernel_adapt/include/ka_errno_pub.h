// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-11-05
 */

#ifndef _KA_PUB_ERR_H
#define _KA_PUB_ERR_H


#define KA_IS_ERR_VALUE(x) IS_ERR_VALUE(x)

#define KA_ERR_PTR(error) ERR_PTR(error)

#define KA_PTR_ERR(ptr) PTR_ERR(ptr)

#define KA_IS_ERR(ptr) IS_ERR(ptr)

#define KA_IS_ERR_OR_NULL(ptr) IS_ERR_OR_NULL(ptr)

#define KA_PTR_ERR_OR_ZERO(ptr) PTR_ERR_OR_ZERO(ptr)

#endif
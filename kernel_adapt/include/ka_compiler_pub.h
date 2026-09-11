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
 * Create: 2024-6-13
 */

#ifndef _KA_COMPILER_H
#define _KA_COMPILER_H

#define __ka_user __user
#define ka_likely(x)    likely(x)
#define ka_unlikely(x)  unlikely(x)
#define KA_READ_ONCE(x)       READ_ONCE(x)
#define KA_WRITE_ONCE(x, val) WRITE_ONCE(x, val)

#endif

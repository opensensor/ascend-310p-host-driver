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
#ifndef KA_DEFINE_H
#define KA_DEFINE_H

#ifndef __user
    #define __user
#endif

#ifndef __iomem
    #define __iomem
#endif

#ifndef NULL
    #define NULL 0
#endif

#ifndef __must_check
    #define __must_check
#endif

#ifndef likely
    # define likely(x)		__builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
    # define unlikely(x)		__builtin_expect(!!(x), 0)
#endif

#endif
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
* Create: 2023-11-4
*/


#ifndef DRV_MUTEX_H
#define DRV_MUTEX_H

#include <linux/mutex.h>
#include <linux/delay.h>

#if defined(STATIC_SKIP)
#define STATIC
#else
#define STATIC static
#endif

#define RET_LOCK_SUCC ((int)1)

/*
 * desc: Try to acquire the mutex, with retry
 * pram: struct mutex *lock: lock pointer
 *       unsigned int retry_times: retry times
 *       unsigned int retry_interval_ms: retry interval time (ms)
 * return: 0 if the mutex has been acquired successfully, and -EBUSY on contention.
 */
STATIC inline int mutex_trylock_with_retry(struct mutex *lock,
    unsigned int retry_times, unsigned int retry_interval_ms)
{
    while (retry_times > 0) {
        if (mutex_trylock(lock) == RET_LOCK_SUCC) {
            return 0;
        }
        msleep(retry_interval_ms);
        retry_times--;
    }
    return -EBUSY;
}

#endif
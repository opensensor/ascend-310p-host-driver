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

#ifndef URD_NOTIFIER_H
#define URD_NOTIFIER_H

#include "ka_dfx_pub.h"

#define URD_NOTIFIER_INIT    (0x1)
#define URD_NOTIFIER_RELEASE (0x2)
#define URD_NOTIFIER_RELEASE_PREPARE  (0x3)

/*
 * URD interface notification chain, which is used to
 * notify other modules of user process startup and exit events
 *
 * @mode: URD_NOTIFIER_INIT or URD_NOTIFIER_RELEASE
 * @data: refer to struct @urd_file_private_stru
 */
int urd_notifier_call(u64 mode, void* data);

int urd_register_notifier(ka_notifier_block_t *nb);

int urd_unregister_notifier(ka_notifier_block_t *nb);

#endif /* URD_NOTIFIER_H */


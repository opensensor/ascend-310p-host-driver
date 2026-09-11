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

#include "virtmng_interface.h"
#include "virtmng_public_def.h"

#include "ka_kernel_def_pub.h"

int vmngh_common_msg_send(u32 dev_id, u32 fid, enum vmng_msg_common_type cmn_type,
    struct vmng_tx_msg_proc_info *tx_info)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_common_msg_send);

int vmngh_register_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_register_common_msg_client);

int vmngh_unregister_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmngh_unregister_common_msg_client);



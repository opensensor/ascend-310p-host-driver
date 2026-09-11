/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-12-13
*/
#include "vmng_mem_alloc_interface.h"
#include "vpc_interface.h"

#include "ka_kernel_def_pub.h"

int vpc_register_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmngh_vpc_register_client(dev_id, fid, vpc_client);
}
KA_EXPORT_SYMBOL(vpc_register_client);

int vpc_register_client_safety(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmngh_vpc_register_client_safety(dev_id, fid, vpc_client);
}
KA_EXPORT_SYMBOL(vpc_register_client_safety);

int vpc_unregister_client(u32 dev_id, u32 fid, const struct vmng_vpc_client *vpc_client)
{
    return vmngh_vpc_unregister_client(dev_id, fid, vpc_client);
}
KA_EXPORT_SYMBOL(vpc_unregister_client);

int vpc_msg_send(u32 dev_id, u32 fid, enum vmng_vpc_type vpc_type,
    struct vmng_tx_msg_proc_info *tx_info, u32 timeout)
{
    return vmngh_vpc_msg_send(dev_id, fid, vpc_type, tx_info, timeout);
}
KA_EXPORT_SYMBOL(vpc_msg_send);

int vpc_register_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    return vmngh_register_common_msg_client(dev_id, fid, msg_client);
}
KA_EXPORT_SYMBOL(vpc_register_common_msg_client);

int vpc_unregister_common_msg_client(u32 dev_id, u32 fid, const struct vmng_common_msg_client *msg_client)
{
    return vmngh_unregister_common_msg_client(dev_id, fid, msg_client);
}
KA_EXPORT_SYMBOL(vpc_unregister_common_msg_client);

int vpc_common_msg_send(u32 dev_id, u32 fid, enum vmng_msg_common_type cmn_type,
                        struct vmng_tx_msg_proc_info *tx_info)
{
    return vmngh_common_msg_send(dev_id, fid, cmn_type, tx_info);
}
KA_EXPORT_SYMBOL(vpc_common_msg_send);

int vmng_vpc_init(struct vmng_vpc_unit *unit_in, int server_type)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmng_vpc_init);

int vmng_vpc_uninit(struct vmng_vpc_unit *unit_in, int server_type)
{
    return 0;
}
KA_EXPORT_SYMBOL(vmng_vpc_uninit);

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
 * Create: 2024-11-11
 */

#include <linux/etherdevice.h>

#include "ka_net_pub.h"
#include "ka_net.h"

ka_net_device_t *ka_net_alloc_netdev(int sizeof_priv, const char *ndev_name)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
    return alloc_netdev(sizeof_priv, ndev_name, NET_NAME_USER, ether_setup);
#else
    return alloc_netdev(sizeof_priv, ndev_name, ether_setup);
#endif
}
EXPORT_SYMBOL_GPL(ka_net_alloc_netdev);

void ka_net_netif_napi_add(ka_net_device_t *dev, ka_napi_struct_t *napi, int (*poll)(ka_napi_struct_t *, int), int weight)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
    netif_napi_add_weight(dev, napi, poll, weight);
#else
    netif_napi_add(dev, napi, poll, weight);
#endif
}
EXPORT_SYMBOL_GPL(ka_net_netif_napi_add);

void ka_net_ether_addr_copy(ka_net_device_t *ndev, const unsigned char *mac)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    dev_addr_set(ndev, mac);
#else
    ether_addr_copy(ndev->dev_addr, mac);
#endif
}
EXPORT_SYMBOL_GPL(ka_net_ether_addr_copy);
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

#ifndef KA_NET_PUB_H
#define KA_NET_PUB_H

#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/if_ether.h>
#include <linux/mdio.h>
#include <linux/uio.h>
#include <linux/fs.h>
#include <linux/types.h>

#ifdef CONFIG_CGROUP_RDMA
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/cgroup_rdma.h>
#endif
#endif

#include "ka_macro_pub.h"
#include "ka_common_pub.h"

#define KA_GRO_MERGED      GRO_MERGED
#define KA_GRO_MERGED_FREE GRO_MERGED_FREE
#define KA_GRO_HELD        GRO_HELD
#define KA_GRO_NORMAL      GRO_NORMAL
#define KA_GRO_DROP        GRO_DROP
#define KA_GRO_CONSUMED    GRO_CONSUMED
#define ka_gro_result_t gro_result_t

#define KA_SKB_REASON_CONSUMED SKB_REASON_CONSUMED
#define KA_SKB_REASON_DROPPED  SKB_REASON_DROPPED
typedef enum skb_free_reason ka_skb_free_reason_t;

#define KA_ETH_ALEN ETH_ALEN
#define KA_ETH_GSTRING_LEN ETH_GSTRING_LEN
#define KA_ETH_SS_STATS ETH_SS_STATS
#define KA_ETH_SS_TEST ETH_SS_TEST
#define KA_ETH_HLEN ETH_HLEN

#define KA_NETIF_F_GRO NETIF_F_GRO
#define KA_NETIF_F_HIGHDMA NETIF_F_HIGHDMA
#define KA_NETIF_F_GSO NETIF_F_GSO
typedef struct phy_device ka_phy_device_t;

typedef struct msghdr ka_msghdr_t;
typedef struct neighbour ka_neighbour_t;
typedef struct ifreq ka_ifreq_t;
typedef struct rdma_cgroup ka_rdma_cgroup_t;
typedef struct rdmacg_device ka_rdmacg_device_t;
typedef struct mdio_device ka_mdio_device_t;
typedef struct netdev_hw_addr_list ka_netdev_hw_addr_list_t;
typedef struct ethtool_link_ksettings ka_ethtool_link_ksettings_t;

typedef struct sock ka_sock_t;
typedef struct net ka_net_t;
typedef struct net_device ka_net_device_t;
typedef struct netdev_queue ka_netdev_queue_t;
typedef struct sk_buff ka_sk_buff_t;
typedef struct sk_buff_head ka_sk_buff_head_t;
typedef struct napi_struct ka_napi_struct_t;

typedef struct dst_entry ka_dst_entry_t;
typedef struct in_device ka_in_device_t;
typedef struct nlmsghdr ka_nlmsghdr_t;
typedef struct netlink_ext_ack ka_netlink_ext_ack_t;
typedef struct netlink_kernel_cfg ka_netlink_kernel_cfg_t;
typedef struct netlink_dump_control ka_netlink_dump_control_t;

typedef struct in6_addr ka_in6_addr_t;
typedef struct inet6_dev ka_inet6_dev_t;
typedef struct ethtool_ops ka_ethtool_ops_t;
typedef struct ethtool_drvinfo ka_ethtool_drvinfo_t;
typedef struct ethtool_stats ka_ethtool_stats_t;
#define ka_ethtool_n_get_drvinfo(n_get_drvinfo) \
    .get_drvinfo = n_get_drvinfo,
#define ka_ethtool_n_get_link(n_get_link) \
    .get_link = n_get_link,
#define ka_ethtool_n_get_strings(n_get_strings) \
    .get_strings = n_get_strings,
#define ka_ethtool_n_get_sset_count(n_get_sset_count) \
    .get_sset_count = n_get_sset_count,
#define  ka_ethtool_n_get_ethtool_stats(n_get_ethtool_stats) \
    .get_ethtool_stats = n_get_ethtool_stats,

typedef struct net_device_ops ka_net_device_ops_t;
#define ka_net_n_ndo_open(n_ndo_open) \
    .ndo_open = n_ndo_open,
#define ka_net_n_ndo_stop(n_ndo_stop) \
    .ndo_stop = n_ndo_stop,
#define ka_net_n_ndo_start_xmit(n_ndo_start_xmit) \
    .ndo_start_xmit = n_ndo_start_xmit,
#ifndef RHEL_RELEASE_CODE
#define ka_net_n_ndo_change_mtu(n_ndo_change_mtu) \
    .ndo_change_mtu = n_ndo_change_mtu,
#else
#define ka_net_n_ndo_change_mtu(n_ndo_change_mtu)
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define ka_net_n_ndo_tx_timeout(n_ndo_tx_timeout_new, n_ndo_tx_timeout) \
    .ndo_tx_timeout = n_ndo_tx_timeout_new,
#else
#define ka_net_n_ndo_tx_timeout(n_ndo_tx_timeout_new, n_ndo_tx_timeout) \
    .ndo_tx_timeout = n_ndo_tx_timeout,
#endif
#define ka_net_n_ndo_get_stats(n_ndo_get_stats) \
    .ndo_get_stats = n_ndo_get_stats,

typedef struct net_device_stats ka_net_device_stats_t;
static inline void ka_net_netdev_set_stats(ka_net_device_t *dev, ka_net_device_stats_t *stats)
{
    dev->stats = *stats;
}

static inline ka_net_device_stats_t *ka_net_netdev_get_stats(ka_net_device_t *dev)
{
    return &dev->stats;
}

static inline unsigned long ka_net_netdev_get_stats_tx_packets(ka_net_device_t *dev)
{
    return dev->stats.tx_packets;
}

static inline void ka_net_netdev_set_trans_start(ka_net_device_t *dev)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 2, 0)
#ifndef RHEL_RELEASE_CODE
    dev->trans_start = jiffies;
#endif
#endif
}

static inline void ka_net_netdev_tx_dropped_add(ka_net_device_t *dev)
{
    dev->stats.tx_dropped++;
}

static inline void ka_net_netdev_tx_fifo_errors_add(ka_net_device_t *dev)
{
    dev->stats.tx_fifo_errors++;
}

static inline void ka_net_netdev_tx_packets_add(ka_net_device_t *dev)
{
    dev->stats.tx_packets++;
}

static inline void ka_net_netdev_tx_bytes_add(ka_net_device_t *dev, unsigned int len)
{
    dev->stats.tx_bytes += len;
}

static inline void ka_net_netdev_tx_errors_add(ka_net_device_t *dev)
{
    dev->stats.tx_errors++;
}

static inline void ka_net_netdev_set_last_rx(ka_net_device_t *dev)
{
#ifdef RHEL_RELEASE_CODE
#if (RHEL_RELEASE_VERSION(7, 5) > RHEL_RELEASE_CODE)
    dev->last_rx = jiffies;
#endif
#else
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 2, 0)
    dev->last_rx = jiffies;
#endif
#endif
}

static inline void ka_net_netdev_set_mtu(ka_net_device_t *dev, unsigned int mtu)
{
    dev->mtu = mtu;
}

static inline unsigned int ka_net_netdev_get_mtu(ka_net_device_t *dev)
{
    return dev->mtu;
}

static inline size_t ka_net_copy_to_iter(void *addr, size_t bytes, ka_iov_iter_t *i)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    return copy_to_iter(addr, bytes, i);
#else
    (void)addr;
    (void)bytes;
    (void)i;
    return 0;
#endif
}

static inline long long ka_net_get_ki_pos(ka_kiocb_t* iocb)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    return iocb->ki_pos;
#else
    (void)iocb;
    return 0;
#endif
}

static inline void ka_net_set_ki_pos(ka_kiocb_t* iocb, long long ki_pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    iocb->ki_pos = ki_pos;
#else
    (void)iocb;
    (void)ki_pos;
    return;
#endif
}

#define ka_net_mdiobus_register(bus) mdiobus_register(bus)
#define ka_net_mdiobus_unregister(bus) mdiobus_unregister(bus)
#define ka_net_mdiobus_register_device(mdiodev) mdiobus_register_device(mdiodev)
#define ka_net_mdiobus_unregister_device(mdiodev) mdiobus_unregister_device(mdiodev)
#define ka_net_mdiobus_get_phy(bus, addr) mdiobus_get_phy(bus, addr)
#define ka_net_mdiobus_read(bus, addr, regnum) mdiobus_read(bus, addr, regnum)
#define ka_net_mdiobus_read_nested(bus, addr, regnum) mdiobus_read_nested(bus, addr, regnum)
#define ka_net_mdiobus_write(bus, addr, regnum, val) mdiobus_write(bus, addr, regnum, val)
#define ka_net_mdiobus_write_nested(bus, addr, regnum, val) mdiobus_write_nested(bus, addr, regnum, val)
#define ka_net_phy_ethtool_ksettings_set(phydev, cmd) phy_ethtool_ksettings_set(phydev, cmd)
#define ka_net_phy_ethtool_ksettings_get(phydev, cmd) phy_ethtool_ksettings_get(phydev, cmd)
#define ka_net_phy_mii_ioctl(phydev, ifr, cmd) phy_mii_ioctl(phydev, ifr, cmd)
#define ka_net_phy_start_aneg(phydev) phy_start_aneg(phydev)
#define ka_net_phy_start(phydev) phy_start(phydev)
#define ka_net_phy_stop(phydev) phy_stop(phydev)
#define ka_net_phy_disconnect(phydev) phy_disconnect(phydev)
#define ka_net_genphy_restart_aneg(phydev) genphy_restart_aneg(phydev)
#define ka_net_skb_copy_datagram_iter(skb, offset, to, len) skb_copy_datagram_iter(skb, offset, to, len)
#define ka_net_skb_copy_datagram_from_iter(skb, offset, from, len) skb_copy_datagram_from_iter(skb, offset, from, len)
#define ka_net_skb_copy_and_csum_datagram_msg(skb, hlen, msg) skb_copy_and_csum_datagram_msg(skb, hlen, msg)

#define __ka_net_ethtool_get_link_ksettings(kdev, link_ksettings) __ethtool_get_link_ksettings(kdev, link_ksettings)

#define ka_net_neigh_destroy(neigh) neigh_destroy(neigh)
#define ka_net_neigh_event_send(neigh, skb) neigh_event_send(neigh, skb)

#define ka_net_dev_get_by_name(net, name) dev_get_by_name(net, name)
#define ka_net_dev_get_by_name_rcu(net, name) dev_get_by_name_rcu(net, name)
#define ka_net_dev_get_by_index(net, ifindex) dev_get_by_index(net, ifindex)
#define ka_net_dev_get_by_index_rcu(net, ifindex) dev_get_by_index_rcu(net, ifindex)
#define ka_net_register_netdevice_notifier(nb) register_netdevice_notifier(nb)
#define ka_net_unregister_netdevice_notifier(nb) unregister_netdevice_notifier(nb)
#define ka_net_netif_set_xps_queue(kdev, mask, index) netif_set_xps_queue(kdev, mask, index)
#define ka_net_netdev_reset_tc(kdev) netdev_reset_tc(kdev)
#define ka_net_netdev_set_tc_queue(kdev, tc, count, offset) netdev_set_tc_queue(kdev, tc, count, offset)
#define ka_net_netdev_set_num_tc(kdev, num_tc) netdev_set_num_tc(kdev, num_tc)
#define ka_net_netif_set_real_num_tx_queues(kdev, txq) netif_set_real_num_tx_queues(kdev, txq)
#define ka_net_netif_set_real_num_rx_queues(kdev, rxq) netif_set_real_num_rx_queues(kdev, rxq)
#define ka_net_netif_schedule_queue(txq) netif_schedule_queue(txq)
#define ka_net_netif_tx_wake_queue(dev_queue) netif_tx_wake_queue(dev_queue)
#define ka_net_dev_kfree_skb_any(skb) dev_kfree_skb_any(skb)
#define ka_net_skb_checksum_help(skb) skb_checksum_help(skb)
#define ka_net_napi_gro_flush(napi, flush_old) napi_gro_flush(napi, flush_old)
#define __ka_net_napi_schedule(n) __napi_schedule(n)
#define ka_net_napi_schedule_prep(n) napi_schedule_prep(n)
#define __ka_net_napi_schedule_irqoff(n) __napi_schedule_irqoff(n)
#define ka_net_napi_complete_done(n, work_done) napi_complete_done(n, work_done)
#define ka_net_napi_complete(n) napi_complete(n)
#define ka_net_netif_napi_del(napi) netif_napi_del(napi)
#define ka_net_napi_disable(n) napi_disable(n)
#define ka_net_netdev_has_upper_dev_all_rcu(kdev, kupper_dev) netdev_has_upper_dev_all_rcu(kdev, kupper_dev)
#define ka_net_netdev_master_upper_dev_get_rcu(kdev) netdev_master_upper_dev_get_rcu(kdev)
#define ka_net_netif_tx_stop_all_queues(kdev) netif_tx_stop_all_queues(kdev)
#define ka_net_register_netdevice(kdev) register_netdevice(kdev)
#define ka_net_register_netdev(kdev) register_netdev(kdev)
#define ka_net_free_netdev(kdev) free_netdev(kdev)
#define ka_net_synchronize_net() synchronize_net()
#define ka_net_unregister_netdevice_queue(kdev, head) unregister_netdevice_queue(kdev, head)
#define ka_net_unregister_netdevice_many(head) unregister_netdevice_many(head)
#define ka_net_unregister_netdev(kdev) unregister_netdev(kdev)
#define ka_net_alloc_skb(size, gfp_mask) alloc_skb(size, gfp_mask)
#define ka_net_netdev_alloc_skb(kdev, len) netdev_alloc_skb(kdev, len)
#define ka_net_dev_alloc_skb(len) dev_alloc_skb(len)
#define ka_net_napi_alloc_skb(napi, len) napi_alloc_skb(napi, len)
#define ka_net_skb_add_rx_frag(skb, i, page, off, size, truesize) skb_add_rx_frag(skb, i, page, off, size, truesize)
#define ka_net_kfree_skb(skb) kfree_skb(skb)
#define ka_net_kfree_skb_list(segs) kfree_skb_list(segs)
#define ka_net_consume_skb(skb) consume_skb(skb)
#define ka_net_skb_copy_header(new_skb, old_skb) skb_copy_header(new_skb, old_skb)
#define ka_net_skb_copy(skb, gfp_mask) skb_copy(skb, gfp_mask)
#define ka_net_pskb_expand_head(skb, nhead, ntail, gfp_mask) pskb_expand_head(skb, nhead, ntail, gfp_mask)
#define ka_net_skb_copy_expand(skb, newheadroom, newtailroom, priority) skb_copy_expand(skb, newheadroom, newtailroom, priority)
#define ka_net_skb_put(skb, len) skb_put(skb, len)
#define ka_net_skb_push(skb, len) skb_push(skb, len)
#define ka_net_skb_pull(skb, len) skb_pull(skb, len)
#define ka_net_skb_trim(skb, len) skb_trim(skb, len)
#define ka_net_skb_copy_bits(skb, offset, to, len) skb_copy_bits(skb, offset, to, len)
#define ka_net_skb_copy_and_csum_dev(skb, to) skb_copy_and_csum_dev(skb, to)
#define ka_net_skb_dequeue(list) skb_dequeue(list)
#define ka_net_skb_dequeue_tail(list) skb_dequeue_tail(list)
#define ka_net_skb_queue_purge(list) skb_queue_purge(list)
#define ka_net_skb_queue_tail(list, newsk) skb_queue_tail(list, newsk)
#define ka_net_kfree_skb_partial(skb, head_stolen) kfree_skb_partial(skb, head_stolen)

#define ka_net_dst_release(dst) dst_release(dst)
#define ka_net_dst_release_immediate(dst) dst_release_immediate(dst)

#define ka_net_vlan_dev_real_dev(kdev) vlan_dev_real_dev(kdev)
#define ka_net_vlan_dev_vlan_id(kdev) vlan_dev_vlan_id(kdev)
#define ka_net_netif_carrier_on(kdev) netif_carrier_on(kdev)
#define ka_net_netif_carrier_off(kdev) netif_carrier_off(kdev)
#define ka_net_eth_type_trans(skb, kdev) eth_type_trans(skb, kdev)
#define ka_net_ether_setup(kdev) ether_setup(kdev)
#define ka_net_alloc_etherdev_mqs(sizeof_priv, txqs, rxqs) alloc_etherdev_mqs(sizeof_priv, txqs, rxqs)
#define ka_net_netlink_ns_capable(skb, user_ns, cap) netlink_ns_capable(skb, user_ns, cap)
#define ka_net_netlink_capable(skb, cap) netlink_capable(skb, cap)
#define ka_net_netlink_unicast(ssk, skb, portid, nonblock) netlink_unicast(ssk, skb, portid, nonblock)
#define ka_net_netlink_broadcast(ssk, skb, portid, group, allocation) netlink_broadcast(ssk, skb, portid, group, allocation)
#define ka_net_netlink_kernel_create(net, unit, cfg) netlink_kernel_create(net, unit, cfg)
#define ka_net_netlink_kernel_release(sk) netlink_kernel_release(sk)
#define ka_net_nlmsg_put(skb, portid, seq, type, len, flags) nlmsg_put(skb, portid, seq, type, len, flags)
#define ka_net_netlink_dump_start(ssk, skb, nlh, control) netlink_dump_start(ssk, skb, nlh, control)
ka_net_device_t *ka_net_alloc_netdev(int sizeof_priv, const char *ndev_name);
void ka_net_netif_napi_add(ka_net_device_t *dev, ka_napi_struct_t *napi, int (*poll)(ka_napi_struct_t *, int), int weight);
#ifndef EMU_ST
#define ka_net_napi_gro_receive(napi, skb) napi_gro_receive(napi, skb)
#define ka_net_netif_start_queue(kdev) netif_start_queue(kdev)
#define ka_net_netif_stop_queue(kdev) netif_stop_queue(kdev)
#define ka_net_netdev_priv(kdev) netdev_priv(kdev)
#define ka_net_skb_queue_head_init(skbq) skb_queue_head_init(skbq)
#define ka_net_napi_enable(napi) napi_enable(napi)
#define ka_net_skb_mac_header(skb) skb_mac_header(skb)
#define ka_net_netdev_sent_queue(kdev, bytes) netdev_sent_queue(kdev, bytes)
#define ka_net_netdev_completed_queue(kdev, pkts, bytes) netdev_completed_queue(kdev, pkts, bytes)
#define ka_net_dev_consume_skb_any(skb) dev_consume_skb_any(skb)
#define ka_net_netif_queue_stopped(kdev) netif_queue_stopped(kdev)
#define ka_net_netif_wake_queue(kdev) netif_wake_queue(kdev)
#define ka_net_is_valid_ether_addr(mac) is_valid_ether_addr(mac)
#endif

typedef struct nlattr ka_nlattr_t;
typedef struct sk_buff ka_sk_buff_t;
#define ka_net_nla_put(skb, attrtype, attrlen, data) nla_put(skb, attrtype, attrlen, data)
#define ka_net_nla_put_64bit(skb, attrtype, attrlen, data, padattr) nla_put_64bit(skb, attrtype, attrlen, data, padattr)
#define ka_net_nla_put_nohdr(skb, attrlen, data) nla_put_nohdr(skb, attrlen, data)

#define KA_NETDEV_TX_OK   NETDEV_TX_OK
#define KA_NETDEV_TX_BUSY NETDEV_TX_BUSY
#define ka_netdev_tx_t    netdev_tx_t
 
#define KA_CHECKSUM_NONE  CHECKSUM_NONE

void ka_net_ether_addr_copy(ka_net_device_t *ndev, const unsigned char *mac);
#endif


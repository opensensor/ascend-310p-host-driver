/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Create: 2025-07-14
 */
#include <linux/dma-mapping.h>

#include "ka_memory_pub.h"
#include "ka_task_pub.h"

#include "securec.h"
#include "ascend_kernel_hal.h"
#include "urd_acc_ctrl.h"
#include "uda.h"
#include "devdrv_user_common.h"
#include "dbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"
#include "udis_msg.h"

#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#define UDIS_DEVICE_NOTIFIER "udis_device"
#define UDIS_VNPU_BASE_UDEVID 32

STATIC void udis_info_buf_uninit(unsigned int udevid, struct udis_ctrl_block *ucb)
{
    if (ucb->udis_info_buf == NULL) {
        udis_info("udis_info_buf has been freed. (udevid=%u)\n", udevid);
        return;
    }

    if (uda_is_phy_dev(udevid)) {
        ka_mm_iounmap((void __iomem *)ucb->udis_info_buf);
    } else {
        dbl_kfree(ucb->udis_info_buf);
    }
    ucb->udis_info_buf = NULL;
}

STATIC int udis_info_buf_dma_map_pf(unsigned int udevid, struct udis_ctrl_block *ucb, struct device *dev)
{
    void *addr = NULL;
    addr = (void *)(uintptr_t)hal_kernel_get_dev_phy_base_addr(udevid, UDIS_INFO_MEM_ADDR);
    if ((phys_addr_t)addr == ULLONG_MAX) {
        udis_err("get phy base addr failed. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    ucb->udis_info_buf_dma = dma_map_resource(dev, (phys_addr_t)(uintptr_t)addr, UDIS_DEVICE_SPACE_SIZE, DMA_BIDIRECTIONAL, 0);
    if (ka_mm_dma_mapping_error(dev, ucb->udis_info_buf_dma) != 0) {
        udis_err("dma map resource failed. (dev_id=%u)\n", udevid);
        return -ENOMEM;
    }

    return 0;
}

STATIC int udis_info_buf_dma_map_vf(unsigned int udevid, struct udis_ctrl_block *ucb, struct device *dev)
{
    ucb->udis_info_buf_dma = devdrv_dma_map_single(dev, ucb->udis_info_buf, UDIS_DEVICE_SPACE_SIZE, DMA_BIDIRECTIONAL);
    if (ka_mm_dma_mapping_error(dev, ucb->udis_info_buf_dma) != 0) {
        udis_err("dma map single failed. (dev_id=%u)\n", udevid);
        return -ENOMEM;
    }

    return 0;
}

STATIC int udis_info_buf_dma_map(unsigned int udevid)
{
    int ret;
    struct udis_ctrl_block *ucb = NULL;
    struct device *dev = NULL;

    dev = uda_get_agent_device(udevid);
    if (dev == NULL) {
        udis_err("uda get agent device info failed. (dev_id=%u)\n", udevid);
        return -ENODEV;
    }

    udis_cb_write_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_cb_write_unlock(udevid);
        udis_err("udis_ctrl_block not init.(udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (uda_is_phy_dev(udevid)) {
        ret = udis_info_buf_dma_map_pf(udevid, ucb, dev);
    } else {
        ret = udis_info_buf_dma_map_vf(udevid, ucb, dev);
    }

    if (ret != 0) {
        udis_cb_write_unlock(udevid);
        udis_err("failed to map udis info buf addr to dma addr. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    udis_cb_write_unlock(udevid);

    return 0;
}

STATIC void udis_info_buf_dma_unmap(unsigned int udevid)
{
    struct udis_ctrl_block *ucb = NULL;
    struct device *dev = NULL;

    dev = uda_get_agent_device(udevid);
    if (dev == NULL) {
        udis_err("uda get agent device info failed. (dev_id=%u)\n", udevid);
        return;
    }

    udis_cb_write_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_cb_write_unlock(udevid);
        udis_err("udis_ctrl_block not init.(udevid=%u)\n", udevid);
        return;
    }

    if (uda_is_phy_dev(udevid)) {
        dma_unmap_resource(dev, ucb->udis_info_buf_dma, UDIS_DEVICE_SPACE_SIZE, DMA_BIDIRECTIONAL, 0);
    } else {
        devdrv_dma_unmap_single(dev, ucb->udis_info_buf_dma, UDIS_DEVICE_SPACE_SIZE, DMA_BIDIRECTIONAL);
    }

    ucb->udis_info_buf_dma = UDIS_BAD_DMA_ADDR;
    udis_cb_write_unlock(udevid);
}

STATIC int udis_info_buf_init(unsigned int udevid, struct udis_ctrl_block *ucb)
{
    void* addr = NULL;
    int ret = 0;

    if (ucb->udis_info_buf != NULL) {
        udis_err("udis_info_buf already init. (dev_id=%d)\n", udevid);
        return -EEXIST;
    }

    if (uda_is_phy_dev(udevid)) {
        addr = hal_kernel_get_dev_phy_base_addr(udevid, UDIS_INFO_MEM_ADDR);
        ucb->udis_info_buf = (struct udis_info_stu __iomem *)ioremap_cache(addr, UDIS_DEVICE_SPACE_SIZE);
    } else {
        ucb->udis_info_buf = (struct udis_info_stu *)dbl_kzalloc (UDIS_DEVICE_SPACE_SIZE, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    }
    if (ucb->udis_info_buf == NULL) {
        udis_err("udis_info_buf ioremap or malloc failed. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = memset_s(ucb->udis_info_buf, UDIS_DEVICE_SPACE_SIZE, 0, UDIS_DEVICE_SPACE_SIZE);
    if (ret != 0 ) {
        udis_info_buf_uninit(udevid, ucb);
        udis_err("memset the udis_info_buf failed. (udevid=%u)\n", udevid);
        ret = -EIO;
    }

    return ret;
}

STATIC int udis_make_up_addr_info(unsigned int udevid, struct udis_ctrl_block *ucb, int index,
    struct udis_addr_info *addr_info)
{
    int ret = 0;

    addr_info->data_len = UDIS_MODULE_LEN;
    addr_info->update_type = UPDATE_PERIOD_LEVEL_1;
    addr_info->acc_ctrl = DMS_SUPPORT_ALL;
    addr_info->dma_addr = ucb->udis_info_buf_dma + index * UDIS_MODULE_OFFSET;

    ret = strcpy_s(addr_info->name, UDIS_MAX_NAME_LEN, UDIS_UNIFIED_MODULE_INFO);
    if (ret != 0) {
        return -EIO;
    }
    return 0;
}

STATIC int udis_register_module_init(unsigned int udevid)
{
    int i, j;
    int ret = 0;
    struct udis_ctrl_block *ucb = NULL;
    struct udis_addr_info addr_info = {0};

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        ret = -EINVAL;
        udis_err("udis_ctrl_block not init.(udevid=%u;)\n", udevid);
        goto free_cb_lock;
    }

    for (i = 0; i < UDIS_MODULE_MAX; i++) {
        ret = udis_make_up_addr_info(udevid, ucb, i, &addr_info);
        if (ret != 0) {
            udis_err("make up addr info failed. (udevid=%u; module_id=%d; ret=%d)\n", udevid, i, ret);
            goto unregister_addr;
        }

        ret = hal_kernel_register_udis_addr(udevid, i, &addr_info);
        if (ret != 0) {
            udis_err("register udis_addr_info failed. (udevid=%u; module_id=%d; ret=%d)\n", udevid, i, ret);
            goto unregister_addr;
        }
    }

    udis_cb_read_unlock(udevid);
    return 0;

unregister_addr:
    for (j = i - 1; j >= 0; --j) {
        (void)hal_kernel_unregister_udis_addr(udevid, j, UDIS_UNIFIED_MODULE_INFO);
    }

free_cb_lock:
    udis_cb_read_unlock(udevid);
    return ret;
}

STATIC int udis_init_ucb_members(u32 udevid, struct udis_ctrl_block *ucb)
{
    int i;
    int ret = 0;

    ka_task_init_rwsem(&(ucb->addr_list_lock));
    for (i = 0; i < UPDATE_TYPE_MAX; i++) {
        INIT_LIST_HEAD(&ucb->addr_list[i]);
    }

    ka_task_init_rwsem(&(ucb->udis_info_lock));
    ret = udis_info_buf_init(udevid, ucb);
    if (ret != 0) {
        udis_err("set udis_info failed. (udevid=%u)\n", udevid);
    }

    return ret;
}

STATIC int udis_ctrl_block_init(u32 udevid)
{
    int ret = 0;
    struct udis_ctrl_block *ucb = NULL;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Device id invalid. (udevid=%u).\n", udevid);
        return -EINVAL;
    }

    udis_cb_write_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb != NULL) {
        ret = -EEXIST;
        udis_err("udis_ctrl_block already init.(udevid=%u)\n", udevid);
        goto cb_unlock;
    }

    ucb = (struct udis_ctrl_block *)dbl_kzalloc(sizeof(struct udis_ctrl_block), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ucb == NULL) {
        ret = -ENOMEM;
        udis_err("kzalloc ctrl block failed.(udevid=%u)\n", udevid);
        goto cb_unlock;
    }

    ret = udis_init_ucb_members(udevid, ucb);
    if (ret != 0) {
        dbl_kfree(ucb);
        ucb = NULL;
        udis_err("init ucb members failed. (udevid=%u; ret=%d)\n", udevid, ret);
        goto cb_unlock;
    }

    udis_set_ctrl_block(udevid, ucb);
cb_unlock:
    udis_cb_write_unlock(udevid);
    return ret;
}

STATIC void udis_ctrl_block_uninit(u32 udevid)
{
    struct udis_ctrl_block *ucb = NULL;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Device id invalid. (udev_id=%u).\n", udevid);
        return;
    }

    udis_cb_write_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    udis_set_ctrl_block(udevid, NULL);
    udis_cb_write_unlock(udevid);

    if (ucb == NULL) {
        return;
    }

    udis_info_buf_uninit(udevid, ucb);
    dbl_kfree(ucb);
    ucb = NULL;

    udis_info("udis ctrl block uninit success. (udevid=%u)\n", udevid);
    return;
}

STATIC void udis_release_all_dma_node(u32 udevid)
{
    int i = 0;
    struct udis_dma_node *node, *node_tmp = NULL;
    struct udis_ctrl_block *ucb = NULL;

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_cb_read_unlock(udevid);
        return;
    }

    down_write(&ucb->addr_list_lock);
    for(i = 0; i < UPDATE_TYPE_MAX; i++) {
        list_for_each_entry_safe(node, node_tmp, &ucb->addr_list[i], list) {
            list_del(&node->list);
            dbl_kfree(node);
            node = NULL;
        }
    }
    up_write(&ucb->addr_list_lock);

    udis_cb_read_unlock(udevid);

    return;
}

STATIC int udis_uda_notifier_phy_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Device id invalid. (udev_id=%u).\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = udis_info_buf_dma_map(udevid);
        if (ret != 0) {
            udis_err("udis info buf dma map failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }

        ret = udis_register_module_init(udevid);
        if (ret != 0) {
            udis_info_buf_dma_unmap(udevid);
            udis_err("register module init failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
        udis_msg_send_work_init(udevid);
    } else if (action == UDA_UNINIT) {
        udis_msg_send_work_uninit(udevid);
        udis_release_all_dma_node(udevid);
        udis_info_buf_dma_unmap(udevid);
    }

    udis_info("udis phy notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

STATIC void udis_unregister_all_node(int phy_dev_num)
{
    int i = 0;
    struct udis_ctrl_block *ucb = NULL;

    for(i = 0; i < phy_dev_num; i++) {
        udis_cb_read_lock(i);
        ucb = udis_get_ctrl_block(i);
        if (ucb == NULL) {
            udis_cb_read_unlock(i);
            return;
        }

        udis_all_dma_node_op_to_host(i, ucb, UDIS_CHAN_D2H_UNREGIST);
        udis_cb_read_unlock(i);
    }

    return;
}

STATIC int udis_uda_notifier_virtual_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Device id invalid. (udev_id=%u).\n", udevid);
        return -EINVAL;
    }

#ifdef CFG_UDIS_SUPPORT_VF
    if (action == UDA_INIT) {
        ret = udis_ctrl_block_init(udevid);
        if (ret != 0) {
            udis_err("udis_ctrl_init init failed. (udev_id=%u; action=%d; ret=%d)\n", udevid, action, ret);
            return ret;
        }

        ret = udis_info_buf_dma_map(udevid);
        if (ret != 0) {
            udis_ctrl_block_uninit(udevid);
            udis_err("udis info buf dma map failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }

        ret = udis_register_module_init(udevid);
        if (ret != 0) {
            udis_info_buf_dma_unmap(udevid);
            udis_ctrl_block_uninit(udevid);
            udis_err("register module init failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
        udis_msg_send_work_init(udevid);
    } else if (action == UDA_UNINIT) {
        udis_msg_send_work_uninit(udevid);
        udis_release_all_dma_node(udevid);
        udis_info_buf_dma_unmap(udevid);
        udis_ctrl_block_uninit(udevid);
    }
#endif

    udis_info("udis virtual notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

STATIC int __init udis_init(void)
{
    int ret, i;
    struct uda_dev_type type;
    int phy_dev_num = uda_get_detected_phy_dev_num();

    for (i = 0; i < UDIS_DEVICE_UDEVID_MAX; ++i) {
        udis_cb_rwlock_init(i);
    }

    for(i = 0; i < phy_dev_num; i++) {
        ret = udis_ctrl_block_init(i);
        if (ret != 0) {
            udis_err("udis_ctrl_init init failed.\n");
            goto udis_cb_uninit;
        }
    }

    uda_davinci_local_real_agent_type_pack(&type);
    ret = uda_notifier_register(UDIS_DEVICE_NOTIFIER, &type, UDA_PRI0, udis_uda_notifier_phy_func);
    if (ret != 0) {
        udis_err("register uda notifier for pf failed. (ret=%d)\n", ret);
        goto udis_cb_uninit;
    }

    uda_davinci_local_virtual_agent_type_pack(&type);
    ret = uda_notifier_register(UDIS_DEVICE_NOTIFIER, &type, UDA_PRI0, udis_uda_notifier_virtual_func);
    if (ret != 0) {
        udis_err("register uda notifier for vf failed. (ret=%d)\n", ret);
        goto uda_phy_unregister;
    }

    ret = udis_common_chan_init();
    if (ret != 0) {
        udis_err("udis msg channel init failed. (ret=%d)\n", ret);
        goto uda_virsh_unregister;
    }
    udis_cmd_init();
    udis_info("udis init success.\n");
    return 0;

uda_virsh_unregister:
    uda_davinci_local_virtual_agent_type_pack(&type);
    (void)uda_notifier_unregister(UDIS_DEVICE_NOTIFIER, &type);
uda_phy_unregister:
    uda_davinci_local_real_agent_type_pack(&type);
    (void)uda_notifier_unregister(UDIS_DEVICE_NOTIFIER, &type);
udis_cb_uninit:
    for(i = 0; i < phy_dev_num; i++) {
        udis_ctrl_block_uninit(i);
    }
    return ret;
}
STATIC void __exit udis_exit(void)
{
    int i;
    struct uda_dev_type type;
    int phy_dev_num = uda_get_detected_phy_dev_num();

    udis_cmd_exit();
    udis_unregister_all_node(phy_dev_num);
    udis_common_chan_uninit();

    uda_davinci_local_virtual_agent_type_pack(&type);
    (void)uda_notifier_unregister(UDIS_DEVICE_NOTIFIER, &type);

    uda_davinci_local_real_agent_type_pack(&type);
    (void)uda_notifier_unregister(UDIS_DEVICE_NOTIFIER, &type);

    for(i = 0; i < phy_dev_num; i++) {
        udis_ctrl_block_uninit(i);
    }

    udis_info("Udis driver exit success.\n");
    return;
}

module_init(udis_init);
module_exit(udis_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI driver");

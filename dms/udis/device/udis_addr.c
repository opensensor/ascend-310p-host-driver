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

#include "securec.h"
#include "uda.h"
#include "dbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"
#include "udis_msg.h"

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

STATIC int udis_check_data_valid(UDIS_MODULE_TYPE module_type, unsigned int udevid, struct udis_addr_info *addr_info)
{
    unsigned int name_len = 0;

    if (addr_info->update_type >= UPDATE_TYPE_MAX || addr_info->update_type < 0 || module_type >= UDIS_MODULE_MAX) {
        udis_err("Invalid data. (devid=%u; update_type=%d; update_type_max=%d; module_type=%d; module_type_max=%d)\n",
            udevid, addr_info->update_type, UPDATE_TYPE_MAX, module_type, UDIS_MODULE_MAX);
         return -EINVAL;
    }

    name_len = strnlen(addr_info->name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        udis_err("name length is invalid. (udevid=%u; module_type=%u; name_len=%u; min_len=%u; max_len=%u)\n",
                udevid, module_type, name_len, 1, UDIS_MAX_NAME_LEN - 1);
        return -EINVAL;
    }

    if (strncmp(addr_info->name, UDIS_UNIFIED_MODULE_INFO, UDIS_MAX_NAME_LEN) == 0) {
        if (addr_info->data_len != UDIS_MODULE_OFFSET) {
            udis_err("Unified udis info date len check failed. (udevid=%u; module_type=%d; name=%s; data_len=%u)\n",
                udevid, module_type, addr_info->name, addr_info->data_len);
            return -EINVAL;
        }
    } else {
        if (addr_info->data_len > UDIS_MAX_DATA_LEN) {
            udis_err("Discrete udis info date len check failed. (udevid=%u; module_type=%d; name=%s data_len=%u)\n",
                udevid, module_type, addr_info->name, addr_info->data_len);
            return -EINVAL;
        }
    }

    if (addr_info->dma_addr == UDIS_BAD_DMA_ADDR) {
        udis_err("Invalid dma addr. (udevid=%u; module_type=%u; name=%s)\n",
            udevid, module_type, addr_info->name);
        return -EINVAL;
    }

    return 0;
}

STATIC struct udis_dma_node *udis_make_up_dma_node(unsigned int udevid, UDIS_MODULE_TYPE module_type,
    struct udis_addr_info *addr_info)
{
    int ret = 0;
    struct udis_dma_node *dma_node = NULL;

    dma_node = (struct udis_dma_node *)dbl_kzalloc(sizeof(struct udis_dma_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dma_node == NULL) {
        udis_err("kzalloc dma node failed. (dev_id=%u)\n", udevid);
        return NULL;
    }

    dma_node->data_len = addr_info->data_len;
    dma_node->module_type = module_type;
    dma_node->update_type = addr_info->update_type;
    dma_node->host_dma_addr = 0;
    dma_node->acc_ctrl = addr_info->acc_ctrl;
    dma_node->dev_dma_addr = addr_info->dma_addr;

    ret = strcpy_s(dma_node->name, UDIS_MAX_NAME_LEN, addr_info->name);
    if (ret != 0) {
        udis_err("strcpy_s failed. (module_name=\"%s\"; ret=%d)\n", addr_info->name, ret);
        goto free_dma_node;
    }

    return dma_node;

free_dma_node:
    dbl_kfree(dma_node);
    dma_node = NULL;
    return NULL;
}

STATIC struct udis_dma_node *udis_find_same_node(struct udis_ctrl_block *ucb, UDIS_MODULE_TYPE module_type, char name[])
{
    int i;
    struct udis_dma_node *node, *node_tmp = NULL;

    for(i = 0; i < UPDATE_TYPE_MAX; i++) {
        list_for_each_entry_safe(node, node_tmp, &ucb->addr_list[i], list) {
            if ((module_type == node->module_type) &&
                (strncmp(node->name, name, UDIS_MAX_NAME_LEN) == 0)) {
                    return node;
            }
        }
    }

    return NULL;
}

STATIC int udis_attach_node_and_send_to_host(unsigned int udevid, struct udis_dma_node *dma_node)
{
    int ret;
    struct udis_ctrl_block *ucb = NULL;

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_err("udis ctrl block uninit. (udevid=%u)\n", udevid);
        ret = -ENOMEM;
        goto unlock_ucb;
    }

    down_write(&ucb->addr_list_lock);
    if (udis_find_same_node(ucb, dma_node->module_type, dma_node->name) != NULL) {
        udis_err("The same node is already register. (udevid=%u; module_type=%d; info_name=\"%s\")\n",
            udevid, dma_node->module_type, dma_node->name);
        ret = -ENOMEM;
        goto unlock_addr_list;
    }

    ret = udis_send_dma_node_to_host(udevid, dma_node, UDIS_CHAN_D2H_REGIST);
    if (ret != 0) {
        udis_err("udis send dma node to host failed. (ret=%d; udevid=%u)\n", ret, udevid);
        goto unlock_addr_list;
    }

    list_add(&dma_node->list, &ucb->addr_list[dma_node->update_type]);
unlock_addr_list:
    up_write(&ucb->addr_list_lock);
unlock_ucb:
    udis_cb_read_unlock(udevid);
    return ret;
}

int hal_kernel_register_udis_addr(unsigned int udevid, UDIS_MODULE_TYPE module_type, struct udis_addr_info *addr_info)
{
    int ret = 0;
    struct udis_dma_node *dma_node = NULL;

#ifndef CFG_UDIS_SUPPORT_VF
    if (!uda_is_phy_dev(udevid)) {
        return -EOPNOTSUPP;
    }
#endif

    if (udevid >= UDIS_DEVICE_UDEVID_MAX || addr_info == NULL) {
        udis_err("Invalid para. (udevid=%u; udevin_max=%u; addr_info=%s)\n",
            udevid, module_type, (addr_info == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    ret = udis_check_data_valid(module_type, udevid, addr_info);
    if (ret != 0) {
        udis_err("udis check data valid failed. (ret=%d; udevid=%u; module_type=%d; info_name=\"%s\")\n",
            ret, udevid, module_type, addr_info->name);
        return ret;
    }

    dma_node = udis_make_up_dma_node(udevid, module_type, addr_info);
    if (dma_node == NULL) {
        udis_err("udis make up dma node failed. (udevid=%u; module_type=%d; info_name=\"%s\")\n",
            udevid, module_type, addr_info->name);
        return -EINVAL;
    }

    ret = udis_attach_node_and_send_to_host(udevid, dma_node);
    if (ret != 0) {
        udis_err("udis attach node and send to host failed. (ret=%d; udevid=%u; module_type=%d; info_name=\"%s\")\n",
            ret, udevid, module_type, addr_info->name);
        dbl_kfree(dma_node);
        dma_node = NULL;
        return ret;
    }

    udis_info("udis info register successful. (devid=%u; module_type=%d; info_name=\"%s\")\n",
        udevid, module_type, addr_info->name);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_register_udis_addr);

STATIC int udis_unregister_input_check(unsigned int udevid, UDIS_MODULE_TYPE module_type, char name[])
{
    unsigned int name_len = 0;
    if (udevid >= UDIS_DEVICE_UDEVID_MAX || module_type >= UDIS_MODULE_MAX) {
        udis_err("Invalid data. (devid=%u; devid_max=%d; module_type=%d; module_type_max=%d)\n",
            udevid, UDIS_DEVICE_UDEVID_MAX, module_type, UDIS_MODULE_MAX);
        return -EINVAL;
    }

    if (name == NULL) {
        udis_err("name is NULL. (udevid=%u; module_type=%u)\n", udevid, module_type);
        return -EINVAL;
    }

    name_len = strnlen(name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        udis_err("name length is invalid. (udevid=%u; module_type=%u; name_len=%u; min_len=%u; max_len=%u)\n",
                udevid, module_type, name_len, 1, UDIS_MAX_NAME_LEN - 1);
        return -EINVAL;
    }

    return 0;
}

int hal_kernel_unregister_udis_addr(unsigned int udevid, UDIS_MODULE_TYPE module_type, char name[])
{
    int ret;
    struct udis_ctrl_block *ucb = NULL;
    struct udis_dma_node *node = NULL;

#ifndef CFG_UDIS_SUPPORT_VF
    if (!uda_is_phy_dev(udevid)) {
        return -EOPNOTSUPP;
    }
#endif

    ret = udis_unregister_input_check(udevid, module_type, name);
    if (ret != 0) {
        return ret;
    }

    udis_cb_read_lock(udevid);
    ucb = udis_get_ctrl_block(udevid);
    if (ucb == NULL) {
        udis_err("udis ctrl block uninit. (devid=%u)\n", udevid);
        ret = -EINVAL;
        goto cb_unlock;
    }

    down_write(&ucb->addr_list_lock);
    node = udis_find_same_node(ucb, module_type, name);
    if (node == NULL) {
        udis_info("udis unregister cannot find the same node. (devid=%u; module_type=%d; info_name=\"%s\")\n",
            udevid, module_type, name);
        /* In the `udis_all_dma_node_op_to_host` function, if sending the DMA node to the host side fails, we will free the DMA node.
         * Other drivers are not aware of this, so they might attempt to unregister a DMA address that has already been freed by us.
         * Therefore, we directly return 0 here.
         */
        ret = 0;
        goto addr_list_unlock;
    }

    ret = udis_send_dma_node_to_host(udevid, node, UDIS_CHAN_D2H_UNREGIST);
    if (ret != 0) {
        udis_err("udis unregister send node to host failed. (devid=%u; module_type=%d; info_name=\"%s\")\n",
            udevid, module_type, name);
        goto addr_list_unlock;
    }
    list_del(&node->list);
    dbl_kfree(node);
    node = NULL;

    udis_info("udis info unregister successful. (devid=%u; module_type=%d; info_name=\"%s\")\n",
        udevid, module_type, name);

addr_list_unlock:
    up_write(&ucb->addr_list_lock);
cb_unlock:
    udis_cb_read_unlock(udevid);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_unregister_udis_addr);
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

#include "virtmngagent_ctrl.h"
#include "virtmngagent_unit.h"
#include "virtmng_interface.h"
#include "virtmng_public_def.h"

#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"

struct vmnga_client *g_vmnga_clients[VMNG_CLIENT_TYPE_MAX] = {NULL};
struct vmnga_client_instance g_vmnga_clients_instance[VMNG_CLIENT_TYPE_MAX][VMNG_PDEV_MAX];
struct vmnga_ctrl g_vmnga_ctrls[VMNG_PDEV_MAX];
ka_mutex_t g_vmnga_ctrl_mutex;

void vmnga_set_ctrl_startup_flag(u32 dev_id, enum vmng_startup_flag_type flag)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return;
    }
    ka_task_mutex_lock(&g_vmnga_ctrl_mutex);
    g_vmnga_ctrls[dev_id].startup_flag = flag;
    ka_task_mutex_unlock(&g_vmnga_ctrl_mutex);
}

int vmnga_get_ctrl_startup_flag(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    return g_vmnga_ctrls[dev_id].startup_flag;
}

ka_pci_dev_t *vmnga_get_pdev_by_id(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    return g_vmnga_ctrls[dev_id].pdev;
}

struct vmnga_unit *vmnga_get_top_half_unit_by_id(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    if (((g_vmnga_ctrls[dev_id].startup_flag == VMNG_STARTUP_TOP_HALF_OK) ||
        (g_vmnga_ctrls[dev_id].startup_flag == VMNG_STARTUP_BOTTOM_HALF_OK)) &&
        (g_vmnga_ctrls[dev_id].dev_id == dev_id)) {
        return (struct vmnga_unit *)g_vmnga_ctrls[dev_id].unit;
    }
    return NULL;
}

struct vmnga_unit *vmnga_get_bottom_half_unit_by_id(u32 dev_id)
{
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    if (g_vmnga_ctrls[dev_id].startup_flag != VMNG_STARTUP_BOTTOM_HALF_OK) {
        vmng_err("startup_flag is invalid. (dev_id=%u; startup_flag=%u)\n", dev_id, g_vmnga_ctrls[dev_id].startup_flag);
        return NULL;
    }
    return (struct vmnga_unit *)g_vmnga_ctrls[dev_id].unit;
}

/* when callby is -1 means err. */
STATIC int vmnga_alloc_dev_id(void)
{
    int dev_id = VMNG_CTRL_DEVICE_ID_INIT;
    int i;

    ka_task_mutex_lock(&g_vmnga_ctrl_mutex);
    for (i = 0; i < VMNG_PDEV_MAX; i++) {
        if (g_vmnga_ctrls[i].startup_flag == VMNG_STARTUP_UNPROBED) {
            g_vmnga_ctrls[i].startup_flag = VMNG_STARTUP_PROBED;
            dev_id = i;
            break;
        }
    }
    ka_task_mutex_unlock(&g_vmnga_ctrl_mutex);

    return dev_id;
}

STATIC void vmnga_assign_ctrl(struct vmnga_unit *unit, struct vmnga_ctrl *ctrl)
{
    ka_task_mutex_lock(&g_vmnga_ctrl_mutex);
    ctrl->pdev = unit->pdev;
    ctrl->dev_id = unit->dev_id;
    ctrl->bus = unit->pdev->bus;
    ctrl->dev = &(unit->pdev->dev);
    ctrl->unit = (void *)unit;
    ka_task_mutex_unlock(&g_vmnga_ctrl_mutex);
}

int vmnga_register_ctrls(struct vmnga_unit *unit)
{
    struct vmnga_ctrl *ctrl = NULL;
    int dev_id_alloc;

    /* allocate new device id */
    dev_id_alloc = vmnga_alloc_dev_id();
    if (dev_id_alloc < 0) {
        vmng_err("Call vmnga_alloc_dev_id failed. (dev_id=%d)\n", dev_id_alloc);
        return -ENOSPC;
    }
    unit->dev_id = (u32)dev_id_alloc;

    /* assign ctrl with unit. */
    ctrl = &g_vmnga_ctrls[dev_id_alloc];
    vmnga_assign_ctrl(unit, ctrl);

    return 0;
}

void vmnga_unregister_ctrls(const struct vmnga_unit *unit)
{
    u32 dev_id = unit->dev_id;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("device_id is invalid. (dev_id=%u)\n", dev_id);
        return;
    }
    ka_task_mutex_lock(&g_vmnga_ctrl_mutex);
    g_vmnga_ctrls[dev_id].unit = NULL;
    g_vmnga_ctrls[dev_id].dev = NULL;
    g_vmnga_ctrls[dev_id].bus = NULL;
    g_vmnga_ctrls[dev_id].pdev = NULL;
    g_vmnga_ctrls[dev_id].dev_id = (u32)VMNG_CTRL_DEVICE_ID_INIT;
    g_vmnga_ctrls[dev_id].startup_flag = VMNG_STARTUP_UNPROBED;
    ka_task_mutex_unlock(&g_vmnga_ctrl_mutex);
}

void vmnga_register_ctrls_half(struct vmnga_unit *unit)
{
    vmnga_set_ctrl_startup_flag(unit->dev_id, VMNG_STARTUP_BOTTOM_HALF_OK);
    g_vmnga_ctrls[unit->dev_id].dtype = unit->shr_para->dtype;
    g_vmnga_ctrls[unit->dev_id].core_num = unit->shr_para->aicore_num;
    g_vmnga_ctrls[unit->dev_id].ddr_size = unit->shr_para->ddrmem_size;
    g_vmnga_ctrls[unit->dev_id].hbm_size = unit->shr_para->hbmmem_size;
}

void vmnga_unregister_ctrls_half(struct vmnga_unit *unit)
{
    vmnga_set_ctrl_startup_flag(unit->dev_id, VMNG_STARTUP_TOP_HALF_OK);
}

STATIC int vmnga_init_instance_proc(u32 dev_id, u32 type, struct vmnga_client_instance *instance,
    const struct vmnga_client *client)
{
    int ret;

    /* if client have not been registered. */
    if (client == NULL) {
        vmng_debug("Input parameter is invalid. (dev_id=%u; client=%u)\n", dev_id, type);
        return 0;
    }
    if (client->init_instance == NULL) {
        vmng_debug("Input parameter is invalid. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
        return 0;
    }
    ka_task_mutex_lock(&instance->flag_mutex);
    if (instance->flag == VMNG_INSTANCE_FLAG_UNINIT) {
        instance->flag = VMNG_INSTANCE_FLAG_INIT;
        ka_task_mutex_unlock(&instance->flag_mutex);
        vmng_info("Device client init begin. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
        ret = client->init_instance(instance);
        if (ret != 0) {
            ka_task_mutex_lock(&instance->flag_mutex);
            instance->flag = VMNG_INSTANCE_FLAG_UNINIT;
            ka_task_mutex_unlock(&instance->flag_mutex);
            vmng_err("Device client init failed. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
            return ret;
        }
        vmng_info("Device client init success. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
    } else {
        ka_task_mutex_unlock(&instance->flag_mutex);
    }

    return 0;
}

STATIC int vmnga_uninit_instance_proc(u32 dev_id, u32 type, struct vmnga_client_instance *instance,
    const struct vmnga_client *client)
{
    int ret;

    if (client == NULL) {
        vmng_warn("Client is NULL. (dev_id=%u; client=%u)\n", dev_id, type);
        return 0;
    }
    /* dev_ctrl is not NULL, means instance init have done. */
    if (instance->dev_ctrl != NULL) {
        if (client->uninit_instance == NULL) {
            vmng_warn("uninit_instance is NULL. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
            return 0;
        }
        ka_task_mutex_lock(&instance->flag_mutex);
        instance->flag = VMNG_INSTANCE_FLAG_UNINIT;
        ka_task_mutex_unlock(&instance->flag_mutex);
        vmng_info("Device client uninit begin. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
        ret = client->uninit_instance(instance);
        if (ret != 0) {
            vmng_err("Device client uninit failed. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
            return ret;
        }
        instance->dev_ctrl = NULL;
        instance->priv = NULL;
        vmng_info("Device client uninit success. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
    }

    return 0;
}

STATIC int vmnga_suspend_instance_proc(u32 dev_id, u32 type, struct vmnga_client_instance *instance,
    const struct vmnga_client *client)
{
    int ret;

    if (client == NULL) {
        vmng_debug("Input parameter is invalid. (dev_id=%u; client=%u)\n", dev_id, type);
        return 0;
    }
    if (instance->dev_ctrl != NULL) {
        if (client->suspend == NULL) {
            vmng_info("Input parameter is invalid. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
            return 0;
        }
        vmng_info("Device client suspend begin. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
        ret = client->suspend(instance);
        if (ret != 0) {
            vmng_err("Device client suspend failed. (dev_id=%u; client_type=%u; ret=%d)\n", dev_id, client->type, ret);
            return ret;
        }
        vmng_info("Device client suspend success. (dev_id=%u; client_type=%u)\n", dev_id, client->type);
    }

    return 0;
}


/* call after probe finish. */
STATIC int vmnga_init_instance_all_client(u32 dev_id)
{
    struct vmnga_client_instance *instance = NULL;
    u32 type;
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    for (type = 0; type < VMNG_CLIENT_TYPE_MAX; type++) {
        instance = &g_vmnga_clients_instance[type][dev_id];
        instance->dev_ctrl = &g_vmnga_ctrls[dev_id];
        ret = vmnga_init_instance_proc(dev_id, type, instance, g_vmnga_clients[type]);
        if (ret != 0) {
            vmng_err("Call vmnga_init_instance_proc failed. (client=%u; ret=%d)\n", type, ret);
            return ret;
        }
    }
    return 0;
}


STATIC int vmnga_init_instance_all_dev(u32 type, struct vmnga_client *client)
{
    struct vmnga_client_instance *instance = NULL;
    u32 dev_id;
    int ret;

    if (type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", type);
        return -EINVAL;
    }
    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        if (g_vmnga_ctrls[dev_id].startup_flag == VMNG_STARTUP_BOTTOM_HALF_OK) {
            instance = &g_vmnga_clients_instance[type][dev_id];
            instance->dev_ctrl = &g_vmnga_ctrls[dev_id];
            ret = vmnga_init_instance_proc(dev_id, type, instance, client);
            if (ret != 0) {
                vmng_err("Call vmnga_init_instance_proc failed. (client_type=%u; ret=%d)\n", type, ret);
                return ret;
            }
        }
    }
    return 0;
}

/* call after probe finish. */
STATIC int vmnga_uninit_instance_all_client(u32 dev_id)
{
    struct vmnga_client_instance *instance = NULL;
    u32 type;
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    for (type = 0; type < VMNG_CLIENT_TYPE_MAX; type++) {
        instance = &g_vmnga_clients_instance[type][dev_id];
        ret = vmnga_uninit_instance_proc(dev_id, type, instance, g_vmnga_clients[type]);
        if (ret != 0) {
            vmng_err("Call vmnga_uninit_instance_proc failed. (client_type=%u; ret=%d)\n", type, ret);
            return ret;
        }
    }
    return 0;
}

STATIC int vmnga_uninit_instance_all_dev(u32 type, struct vmnga_client *client)
{
    int ret;
    struct vmnga_client_instance *instance = NULL;
    u32 dev_id;

    if (type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", type);
        return -EINVAL;
    }
    for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
        instance = &g_vmnga_clients_instance[type][dev_id];
        ret = vmnga_uninit_instance_proc(dev_id, type, instance, client);
        if (ret != 0) {
            vmng_err("Call vmnga_uninit_instance_proc failed. (client_type=%u; ret=%d)\n", type, ret);
            return ret;
        }
    }
    return 0;
}

/* call after probe finish. */
int vmnga_suspend_instance_all_client(u32 dev_id)
{
    struct vmnga_client_instance *instance = NULL;
    u32 type;
    int ret;

    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    for (type = 0; type < VMNG_CLIENT_TYPE_MAX; type++) {
        instance = &g_vmnga_clients_instance[type][dev_id];
        ret = vmnga_suspend_instance_proc(dev_id, type, instance, g_vmnga_clients[type]);
        if (ret != 0) {
            vmng_err("Call vmnga_suspend_instance_proc failed. (client_type=%u; ret=%d)\n", type, ret);
            return ret;
        }
    }
    return 0;
}

int vmnga_init_instance_after_probe(u32 dev_id)
{
    int ret;

    ret = vmnga_init_instance_all_client(dev_id);
    if (ret != 0) {
        vmng_err("Call vmnga_init_instance_all_client failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    return 0;
}

void vmnga_uninit_instance_remove_pdev(u32 dev_id)
{
    int ret;

    ret = vmnga_uninit_instance_all_client(dev_id);
    if (ret != 0) {
        vmng_err("Call vmnga_uninit_instance_all_client failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
}

void vmnga_agent_stop(u32 dev_id)
{
    int ret;

    ret = vmnga_suspend_instance_all_client(dev_id);
    if (ret != 0) {
        vmng_err("Call vmnga_suspend_instance_all_client failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
}

int vmnga_register_client(struct vmnga_client *client)
{
    int ret;

    if (client == NULL) {
        vmng_err("Input parameter is error.\n");
        return -EINVAL;
    }
    if (client->type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", client->type);
        return -EINVAL;
    }
    if (g_vmnga_clients[client->type] != NULL) {
        vmng_err("Client is already registered. (client_type=%u)\n", client->type);
        return -EINVAL;
    }

    ret = vmnga_init_instance_all_dev(client->type, client);
    if (ret != 0) {
        vmng_err("Call vmnga_init_instance_all_dev failed. (client_type=%u; ret=%d)\n", client->type, ret);
        return ret;
    }
    g_vmnga_clients[client->type] = client; /* store for callby */

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_register_client);

int vmnga_unregister_client(struct vmnga_client *client)
{
    int ret;

    if (client == NULL) {
        vmng_err("Input parameter is error.");
        return -EINVAL;
    }
    if (client->type >= VMNG_CLIENT_TYPE_MAX) {
        vmng_err("Input parameter is error. (client_type=%u)\n", client->type);
        return -EINVAL;
    }

    ret = vmnga_uninit_instance_all_dev(client->type, client);
    if (ret != 0) {
        vmng_err("Call vmnga_uninit_instance_all_dev failed. (client_type=%u; ret=%d)\n", client->type, ret);
        return ret;
    }
    g_vmnga_clients[client->type] = NULL;

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_unregister_client);

/* device startup report and devcie state change notify */
struct vmnga_dev_state_record g_vmnga_dev_state;

void vmnga_record_dev_startup(u32 dev_id)
{
    u32 i;

    ka_task_mutex_lock(&g_vmnga_ctrl_mutex);
    for (i = 0; i < g_vmnga_dev_state.dev_num; i++) {
        if (dev_id == g_vmnga_dev_state.startup_devids[i]) {
            vmng_info("Device no need to record, just report it. (dev_id=%u; dev_num=%u)\n",
                      dev_id, g_vmnga_dev_state.dev_num);
            goto OUT;
        }
    }
    if (g_vmnga_dev_state.dev_num < VMNG_PDEV_MAX) {
        g_vmnga_dev_state.startup_devids[g_vmnga_dev_state.dev_num] = dev_id;
        g_vmnga_dev_state.dev_num++;
        vmng_info("Add to report. (dev_id=%u; dev_num=%u)\n", dev_id, g_vmnga_dev_state.dev_num);
        goto OUT;
    } else {
        vmng_warn("Report overflow, (dev_id=%u; dev_num=%u)\n", dev_id, g_vmnga_dev_state.dev_num);
        goto OUT;
    }
OUT:
    ka_task_mutex_unlock(&g_vmnga_ctrl_mutex);
}

void vmnga_report_dev_startup(u32 dev_id)
{
    int ret;

    ka_task_mutex_lock(&g_vmnga_ctrl_mutex);
    if (g_vmnga_dev_state.startup_notify != NULL) {
        ret = (g_vmnga_dev_state.startup_notify)(g_vmnga_dev_state.dev_num, g_vmnga_dev_state.startup_devids,
            VMNG_PDEV_MAX, g_vmnga_dev_state.dev_num);
        if (ret != 0) {
            vmng_err("Report is error. (dev_id=%u; ret=%d)\n", dev_id, ret);
            goto OUT;
        }
        vmng_info("Startup report. (dev_id=%u; num=%u)\n", dev_id, g_vmnga_dev_state.dev_num);
    } else {
        vmng_info("No report object. (dev_id=%u)\n", dev_id);
    }
OUT:
    ka_task_mutex_unlock(&g_vmnga_ctrl_mutex);
}

void vmnga_register_dev_startup_callback(vmnga_dev_startup_notify startup_notify)
{
    if (startup_notify == NULL) {
        vmng_warn("Device startup notify is NULL.\n");
        return;
    }
    g_vmnga_dev_state.startup_notify = startup_notify;
    vmng_info("device startup notify callback register\n");
    vmnga_report_dev_startup(VMNG_PDEV_MAX);
}
KA_EXPORT_SYMBOL(vmnga_register_dev_startup_callback);

void vmnga_dev_state_notifier(struct vmnga_unit *unit)
{
    u32 dev_id;
    int ret;

    if (unit == NULL) {
        vmng_err("Input parameter is error.\n");
        return;
    }

    /* when ko remove, stop to notify others, and set callback to NULL */
    if (unit->module_exit_flag == VMNG_MODULE_REMOVE_BY_MODULE_EXIT) {
        vmng_info("Do not notify state change, driver had been remove.\n");
        g_vmnga_dev_state.state_notify = NULL;
        return;
    }

    dev_id = unit->dev_id;
    if (dev_id >= VMNG_PDEV_MAX) {
        vmng_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return;
    }
    if (g_vmnga_dev_state.state_notify != NULL) {
        ret = (g_vmnga_dev_state.state_notify)(dev_id, VMNG_GOING_TO_DISABLE_DEV);
        if (ret != 0) {
            vmng_err("Notify disable is callback error. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return;
        }
        ka_system_msleep(1000); // sleep 1000 ms
    } else {
        vmng_info("State change notify is not registered. (dev_id=%u)\n", dev_id);
    }
}

void vmnga_register_dev_state_callback(vmnga_dev_state_notify state_notify)
{
    if (state_notify == NULL) {
        vmng_warn("state_notify is NULL.\n");
        return;
    }
    g_vmnga_dev_state.state_notify = state_notify;
    vmng_info("Device state change notify register.\n");
}
KA_EXPORT_SYMBOL(vmnga_register_dev_state_callback);

STATIC void vmnga_init_dev_state_record(void)
{
    u32 i;

    g_vmnga_dev_state.dev_num = 0;
    g_vmnga_dev_state.startup_notify = NULL;
    g_vmnga_dev_state.state_notify = NULL;

    for (i = 0; i < VMNG_PDEV_MAX; i++) {
        g_vmnga_dev_state.startup_devids[i] = VMNG_PDEV_MAX;
    }
}

/* ctrl init */
int vmnga_ctrl_init(void)
{
    enum vmng_client_type type;
    u32 dev_id;

    if ((memset_s(g_vmnga_ctrls, sizeof(g_vmnga_ctrls), 0, sizeof(g_vmnga_ctrls)) != EOK) ||
        (memset_s(g_vmnga_clients, sizeof(g_vmnga_clients), 0, sizeof(g_vmnga_clients)) != EOK) ||
        (memset_s(g_vmnga_clients_instance, sizeof(g_vmnga_clients_instance), 0, sizeof(g_vmnga_clients_instance)) !=
        EOK)) {
        vmng_err("Call memset_s failed.\n");
        return -EINVAL;
    }
    for (type = VMNG_CLIENT_TYPE_DEVMNG; type < VMNG_CLIENT_TYPE_MAX; type++) {
        for (dev_id = 0; dev_id < VMNG_PDEV_MAX; dev_id++) {
            g_vmnga_clients_instance[type][dev_id].type = type;
            g_vmnga_clients_instance[type][dev_id].flag = VMNG_INSTANCE_FLAG_UNINIT;
            ka_task_mutex_init(&g_vmnga_clients_instance[type][dev_id].flag_mutex);
        }
    }
    vmnga_init_dev_state_record();
    ka_task_mutex_init(&g_vmnga_ctrl_mutex);

    return 0;
}

int vmnga_get_physicl_addr_info(u32 dev_id, enum vmng_get_addr_type type, phys_addr_t *addr, u64 *size)
{
    struct vmnga_unit *unit = NULL;

    if (addr == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; addr_type=%u)\n", dev_id, type);
        return -EINVAL;
    }
    if (size == NULL) {
        vmng_err("Input parameter is error. (dev_id=%u; addr_type=%u)\n", dev_id, type);
        return -EINVAL;
    }

    unit = vmnga_get_top_half_unit_by_id(dev_id);
    if (unit == NULL) {
        vmng_err("Device is not ready. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    switch (type) {
        case VMNG_GET_ADDR_TYPE_TSDRV:
            *addr = unit->mmio.bar4_base + VMNG_BAR4_TSDRV_BASE;
            *size = unit->mmio.bar4_size;
            break;
        default:
            vmng_err("addr_type is invalid. (dev_id=%u; addr_type=%u)\n", dev_id, type);
            return -EINVAL;
    }

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_get_physicl_addr_info);

int vmnga_get_pci_dev_info(u32 dev_id, struct vmnga_pci_dev_info *dev_info)
{
    struct vmnga_unit *unit = NULL;

    if (dev_info == NULL) {
        vmng_err("Input parameter is error. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    unit = vmnga_get_top_half_unit_by_id(dev_id);
    if (unit == NULL) {
        vmng_err("Device is not ready. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    dev_info->bus_no = ka_pci_get_bus_number(unit->pdev);
    dev_info->device_no = (u8)((unit->pdev->devfn >> VMNGA_DEVFN_BIT) & VMNGA_DEVFN_DEV_VAL);
    dev_info->function_no = (u8)(unit->pdev->devfn & VMNGA_DEVFN_FN_VAL);

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_get_pci_dev_info);

int vmnga_get_pcie_id_info(u32 dev_id, struct vmnga_pcie_id_info *dev_info)
{
    struct vmnga_unit *unit = NULL;

    if (dev_info == NULL) {
        vmng_err("Input parameter is error. (devid=%u)\n", dev_id);
        return -EINVAL;
    }

    unit = vmnga_get_top_half_unit_by_id(dev_id);
    if (unit == NULL) {
        vmng_err("Device is not ready. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    dev_info->venderid = ka_pci_get_vendor_id(unit->pdev);
    dev_info->deviceid = ka_pci_get_device_id(unit->pdev);
    dev_info->subvenderid = ka_pci_get_subsystem_vendor_id(unit->pdev);
    dev_info->subdeviceid = ka_pci_get_subsystem_device_id(unit->pdev);
    dev_info->bus = ka_pci_get_bus_number(unit->pdev);
    dev_info->device = KA_PCI_SLOT(unit->pdev->devfn);
    dev_info->fn = KA_PCI_FUNC(unit->pdev->devfn);

    return 0;
}
KA_EXPORT_SYMBOL(vmnga_get_pcie_id_info);

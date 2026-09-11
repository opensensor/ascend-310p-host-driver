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
* Create: 2023-2-20
*/

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"

#include "uda_dev.h"
#include "dbl_mem_alloc_interface.h"
#include "uda_notifier.h"

#define UDA_PRI_DEC_ORDER 0 /* priority is high to low */
#define UDA_PRI_INC_ORDER 1 /* priority is low to high */

static struct uda_notifiers uda_notifiers[UDA_HW_MAX][UDA_OBJECT_MAX][UDA_LOCATION_MAX][UDA_PROP_MAX];

static struct uda_notifiers *uda_get_notifiers(struct uda_dev_type *type)
{
    return &uda_notifiers[type->hw][type->object][type->location][type->prop];
}

static int uda_get_action_notifier_order(enum uda_notified_action action)
{
    return ((action == UDA_UNINIT) || (action == UDA_RESUME) ||
        (action == UDA_SHUTDOWN) || (action == UDA_HOTRESET_CANCEL) ||
        (action == UDA_PRE_HOTRESET_CANCEL)) ? UDA_PRI_DEC_ORDER : UDA_PRI_INC_ORDER;
}

static bool uda_is_ctrl_action(enum uda_notified_action action)
{
    return ((action >= UDA_SUSPEND) && (action < UDA_ACTION_MAX));
}

static int uda_single_notifier_call(struct uda_notifier_node *nf, u32 udevid, enum uda_notified_action action)
{
    int ret;

    if (uda_is_action_conflict(nf->status[udevid], action)) {
        uda_info("Conflict action. (notifier=%s; action=%d; udevid=%u; status=%x)\n",
            nf->notifier, action, udevid, nf->status[udevid]);
        return 0;
    }
    nf->call_count++;
    ret = nf->func(udevid, action);
    if (ret == 0) {
        uda_update_status_by_action(&nf->status[udevid], action);
    }
    nf->call_finish++;

    return ret;
}

static int uda_single_pri_notifier_call(ka_list_head_t *nf_head, u32 udevid, enum uda_notified_action action)
{
    struct uda_notifier_node *nf = NULL;

    ka_list_for_each_entry(nf, nf_head, node) {
        int ret = uda_single_notifier_call(nf, udevid, action);
        if (ret != 0) {
            uda_warn("Notifier call warn. (notifier=%s; action=%d; ret=%d)\n", nf->notifier, action, ret);
            if (uda_is_ctrl_action(action)) {
                return ret;
            }
        }
    }

    return 0;
}

static int _uda_notifier_call(struct uda_notifiers *notifiers, u32 udevid, enum uda_notified_action action)
{
    int i;

    for (i = 0; i < UDA_PRI_MAX; i++) {
        int pri = (uda_get_action_notifier_order(action) == UDA_PRI_DEC_ORDER) ? (UDA_PRI_MAX - 1 - i) : i;
        int ret = uda_single_pri_notifier_call(&notifiers->pri_head[pri], udevid, action);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

int uda_notifier_call(u32 udevid, struct uda_dev_type *type, enum uda_notified_action action)
{
    struct uda_notifiers *notifiers = uda_get_notifiers(type);
    int ret;

    ka_task_down_read(&notifiers->sem);
    ret = _uda_notifier_call(notifiers, udevid, action);
    ka_task_up_read(&notifiers->sem);

    return ret;
}

static struct uda_notifier_node *uda_find_notifier_node(struct uda_notifiers *notifiers, const char *notifier)
{
    int i;

    for (i = 0; i < UDA_PRI_MAX; i++) {
        ka_list_head_t *nf_head = &notifiers->pri_head[i];
        struct uda_notifier_node *nf = NULL;

        ka_list_for_each_entry(nf, nf_head, node) {
            if (strcmp(nf->notifier, notifier) == 0) {
                return nf;
            }
        }
    }

    return NULL;
}

static struct uda_notifier_node *uda_create_notifier(const char *notifier, uda_notify func)
{
    struct uda_notifier_node *nf = dbl_kzalloc(sizeof(struct uda_notifier_node), KA_GFP_KERNEL);
    if (nf == NULL) {
        uda_err("Out of memory.\n");
        return NULL;
    }

    nf->notifier = notifier;
    nf->func = func;
    return nf;
}

static void uda_destroy_notifier(struct uda_notifier_node *nf)
{
    dbl_kfree(nf);
}

static void uda_notifier_change_action(struct uda_notifier_node *nf, struct uda_dev_type *type,
    enum uda_notified_action action)
{
    u32 i;
    struct uda_dev_inst *dev_inst = NULL;
    struct uda_dev_inst *agent_dev_inst = NULL;

    for (i = 0; i < UDA_UDEV_MAX_NUM; i++) {
        dev_inst = uda_dev_inst_get(i);
        if (dev_inst == NULL) {
            continue;
        }

        if (!uda_is_removed_status(dev_inst->status)) {
            if (uda_dev_type_is_match(type, &dev_inst->type)) {
                uda_warn("Action. (notifier=%s; udevid=%d; action=%d)\n", nf->notifier, i, action);
                (void)uda_single_notifier_call(nf, i, action);
            }

            agent_dev_inst = dev_inst->agent_dev;
            if ((agent_dev_inst != NULL) && (uda_dev_type_is_match(type, &agent_dev_inst->type))) {
                uda_warn("Action agent. (notifier=%s; udevid=%d; action=%d)\n", nf->notifier, i, action);
                (void)uda_single_notifier_call(nf, i, action);
            }
        }
        uda_dev_inst_put(dev_inst);
    }
}

static int _uda_notifier_register(struct uda_notifiers *notifiers, struct uda_dev_type *type,
    const char *notifier, enum uda_priority pri, uda_notify func)
{
    struct uda_notifier_node *nf = uda_find_notifier_node(notifiers, notifier);
    if (nf != NULL) {
        uda_err("Repeat register. (notifier=%s)\n", notifier);
        return -EINVAL;
    }

    nf = uda_create_notifier(notifier, func);
    if (nf == NULL) {
        uda_err("Create notifier node failed. (notifier=%s)\n", notifier);
        return -ENOMEM;
    }

    ka_list_add_tail(&nf->node, &notifiers->pri_head[pri]);

    /* The priority cannot be guaranteed, just for testing(manually insmod) purposes */
    uda_notifier_change_action(nf, type, UDA_INIT);

    uda_info("Register notifier success. (notifier=%s; pri=%d)\n", notifier, pri);

    return 0;
}

int uda_notifier_register(const char *notifier, struct uda_dev_type *type, enum uda_priority pri, uda_notify func)
{
    struct uda_notifiers *notifiers = NULL;
    int ret;

    if ((notifier == NULL) || (type == NULL) || (func == NULL)) {
        uda_err("Null ptr.\n");
        return -EINVAL;
    }

    ret = uda_dev_type_valid_check(type);
    if (ret != 0) {
        uda_err("Invalid type. (notifier=%s)\n", notifier);
        return ret;
    }

    if ((pri < 0) || (pri >= UDA_PRI_MAX)) {
        uda_err("Invalid pri. (notifier=%s; pri=%d)\n", notifier, pri);
        return -EINVAL;
    }

    notifiers = uda_get_notifiers(type);

    ka_task_down_write(&notifiers->sem);
    ret = _uda_notifier_register(notifiers, type, notifier, pri, func);
    ka_task_up_write(&notifiers->sem);

    return ret;
}
KA_EXPORT_SYMBOL(uda_notifier_register);

static int _uda_notifier_unregister(struct uda_notifiers *notifiers, struct uda_dev_type *type, const char *notifier)
{
    struct uda_notifier_node *nf = NULL;

    nf = uda_find_notifier_node(notifiers, notifier);
    if (nf == NULL) {
        uda_err("Repeat unregister. (notifier=%s)\n", notifier);
        return -EINVAL;
    }

    uda_notifier_change_action(nf, type, UDA_UNINIT);

    ka_list_del(&nf->node);
    uda_destroy_notifier(nf);

    uda_info("Unregister notifier success. (notifier=%s)\n", notifier);
    return 0;
}

int uda_notifier_unregister(const char *notifier, struct uda_dev_type *type)
{
    struct uda_notifiers *notifiers = NULL;
    int ret;

    if ((notifier == NULL) || (type == NULL)) {
        uda_err("Null ptr.\n");
        return -EINVAL;
    }

    ret = uda_dev_type_valid_check(type);
    if (ret != 0) {
        uda_err("Invalid type. (notifier=%s)\n", notifier);
        return ret;
    }

    notifiers = uda_get_notifiers(type);

    ka_task_down_write(&notifiers->sem);
    ret = _uda_notifier_unregister(notifiers, type, notifier);
    ka_task_up_write(&notifiers->sem);

    return ret;
}
KA_EXPORT_SYMBOL(uda_notifier_unregister);

void uda_for_each_notifiers(void *priv,
    void (*func)(struct uda_dev_type *type, struct uda_notifiers *notifiers, void *priv))
{
    struct uda_dev_type type;

    for (type.hw = 0; type.hw < UDA_HW_MAX; type.hw++) {
        for (type.object = 0; type.object < UDA_OBJECT_MAX; type.object++) {
            for (type.location = 0; type.location < UDA_LOCATION_MAX; type.location++) {
                for (type.prop = 0; type.prop < UDA_PROP_MAX; type.prop++) {
                    func(&type, uda_get_notifiers(&type), priv);
                }
            }
        }
    }
}

static void _uda_notifiers_init(struct uda_dev_type *type, struct uda_notifiers *notifiers, void *priv)
{
    int i;

    for (i = 0; i < UDA_PRI_MAX; i++) {
        KA_INIT_LIST_HEAD(&notifiers->pri_head[i]);
    }
    ka_task_init_rwsem(&notifiers->sem);
}

int uda_notifier_init(void)
{
    uda_for_each_notifiers(NULL, _uda_notifiers_init);
    return 0;
}

static void _uda_notifiers_uninit(struct uda_dev_type *type, struct uda_notifiers *notifiers, void *priv)
{
    int i;

    for (i = 0; i < UDA_PRI_MAX; i++) {
        ka_list_head_t *nf_head = &notifiers->pri_head[i];
        struct uda_notifier_node *nf = NULL, *n = NULL;

        ka_list_for_each_entry_safe(nf, n, nf_head, node) {
            ka_list_del(&nf->node);
            uda_destroy_notifier(nf);
        }
    }
}

void uda_notifier_uninit(void)
{
    uda_for_each_notifiers(NULL, _uda_notifiers_uninit);
}


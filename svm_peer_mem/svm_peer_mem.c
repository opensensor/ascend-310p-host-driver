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

#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/dma-mapping.h>
#include <linux/thread_info.h>
#include <linux/sched.h>
#include <linux/pci.h>

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "drv_log.h"
#include "kernel_version_adapt.h"
#include "svm_peer_mem.h"
#include "svm_mem_query.h"
#include "devdrv_manager_comm.h"
#include "ka_kernel_def_pub.h"

#define DRV_PEER_MEM_NAME "SVM_PEER_MEM"
#define DRV_PEER_MEM_VERSION "1.0"

#define SVM_PEER_INITED_FLAG 0xF1234567UL

#ifndef __GFP_ACCOUNT

#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif

#endif

#ifndef DEVMM_UT
#define svm_peer_mem_err(fmt, ...) \
    drv_err("svm_peer_mem", "<%d,%d> " fmt, current->tgid, current->pid, ##__VA_ARGS__)
#else
#define svm_peer_mem_err(fmt, ...)
#endif
int svm_peer_mem_acquire(unsigned long addr, size_t size, void *peer_mem_data,
    char *peer_mem_name, void **client_context);
int svm_peer_mem_get_pages(unsigned long addr, size_t size, int write, int force, ka_sg_table_t *sg_head,
    void *context, u64 core_context);
int svm_peer_mem_dma_map(ka_sg_table_t *sg_head, void *context,
    ka_device_t *dma_device, int dmasync, int *nmap);
int svm_peer_mem_dma_unmap(ka_sg_table_t *sg_head, void *context, ka_device_t *dma_device);
void svm_peer_mem_put_pages(ka_sg_table_t *sg_head, void *context);

struct svm_peer_mem_context {
    struct devmm_svm_process_id process_id;
    ka_mutex_t context_mutex;
    ka_sg_table_t *sg_head;
    u32 inited_flag;
    u32 get_flag;
    u64 va;
    u64 len;
    u64 va_aligned_start;
    u64 aligned_size;
    u32 page_size;
    u32 pa_num;
    u64 pa_list[];
};

DECLARE_RWSEM(svm_context_sem);

#define PCI_VENDOR_ID_HUAWEI 0x19e5
static const struct pci_device_id g_svm_peermem_drv_tbl[] = {
    { KA_PCI_VDEVICE(HUAWEI, 0xa126), 0 },
    { KA_PCI_VDEVICE(HUAWEI, 0xd801), 0 },
    {}
};
KA_MODULE_DEVICE_TABLE(pci, g_svm_peermem_drv_tbl); //lint !e14 !e508

static inline void svm_peer_mem_get_process_id_by_mem_data(struct peer_memory_data *memory_data,
    struct devmm_svm_process_id *process_id)
{
    process_id->hostpid = memory_data->host_pid;
    process_id->devid = memory_data->devid;
    process_id->vfid = memory_data->vfid;
}

unsigned long svm_peer_mem_get_page_size(void *mm_context)
{
    struct svm_peer_mem_context *context = (struct svm_peer_mem_context *)mm_context;
    unsigned long page_size;

    ka_task_down_read(&svm_context_sem);
    if ((context == NULL) || (context->inited_flag != SVM_PEER_INITED_FLAG)) {
#ifndef EMU_ST
        svm_peer_mem_err("Mm_context is NULL or isn't inited. (context_is_null=%d; inited_flag=%u)\n",
            (context == NULL), (context != NULL ? context->inited_flag : 0));
        ka_task_up_read(&svm_context_sem);
#endif
        return 0;
    }
    page_size = context->page_size;
    ka_task_up_read(&svm_context_sem);

    return page_size;
}
KA_EXPORT_SYMBOL_GPL(svm_peer_mem_get_page_size);

int svm_peer_mem_acquire(unsigned long addr, size_t size, void *peer_mem_data,
    char *peer_mem_name, void **client_context)
{
    struct peer_memory_data *memory_data = (struct peer_memory_data *)peer_mem_data;
    u64 va_aligned_start, va_aligned_end, aligned_size, page_size;
    struct svm_peer_mem_context *mm_context = NULL;
    struct devmm_svm_process_id process_id;
    u32 page_num;
    int ret;

    if ((memory_data == NULL) || (client_context == NULL)) {
        svm_peer_mem_err("Input parameter is null. "
            "(memory_data_is_null=%d; client_context_is_null=%d)\n", (memory_data == NULL), (client_context == NULL));
        return false;
    }

    ret = devmm_check_thread_valid(memory_data->host_pid, memory_data->pid_sign, PROCESS_SIGN_LENGTH);
    if (ret != 0) {
        svm_peer_mem_err("Check sign failed. (hostpid=%u; devid=%u; vfid=%u; addr=0x%lx)\n",
            memory_data->host_pid, memory_data->devid, memory_data->vfid, addr);
        return false;
    }
    svm_peer_mem_get_process_id_by_mem_data(memory_data, &process_id);
    page_size = devmm_get_mem_page_size(&process_id, addr, size);
    if (page_size == 0) {
        svm_peer_mem_err("Get memory page size failed. (hostpid=%u; devid=%u; vfid=%u; add=0x%lx; size=%lu)\n",
            memory_data->host_pid, memory_data->devid, memory_data->vfid, addr, size);
        return false;
    }
    va_aligned_start = ka_base_round_down(addr, page_size);
    va_aligned_end = ka_base_round_up((addr + size), page_size);
    aligned_size = va_aligned_end - va_aligned_start;
    if (devmm_check_addr_valid(&process_id, va_aligned_start, aligned_size) == false) {
        svm_peer_mem_err("Acquire error. (hostpid=%u; devid=%u; vfid=%u; add=0x%lx)\n",
            memory_data->host_pid, memory_data->devid, memory_data->vfid, addr);
        return false;
    }
    page_num = (aligned_size) / page_size;
    mm_context = (struct svm_peer_mem_context *)vzalloc(sizeof(struct svm_peer_mem_context) +
        sizeof(u64) * page_num);
    if (mm_context == NULL) {
        svm_peer_mem_err("Vzalloc error. (hostpid=%u; devid=%u; vfid=%u; add=0x%lx; page_num=%u)\n",
            memory_data->host_pid, memory_data->devid, memory_data->vfid, addr, page_num);
        return false;
    }
    mm_context->sg_head = NULL;
    mm_context->process_id = process_id;
    mm_context->va = addr;
    mm_context->len = size;
    mm_context->va_aligned_start = va_aligned_start;
    mm_context->aligned_size = aligned_size;
    mm_context->page_size = page_size;
    mm_context->pa_num = page_num;
    ka_task_mutex_init(&mm_context->context_mutex);
    mm_context->inited_flag = SVM_PEER_INITED_FLAG;
    *client_context = (void *)mm_context;

    return true;
}
KA_EXPORT_SYMBOL_GPL(svm_peer_mem_acquire);

int svm_peer_mem_get_pages(unsigned long addr, size_t size, int write, int force, ka_sg_table_t *sg_head,
    void *context, u64 core_context)
{
    struct svm_peer_mem_context *mm_context = (struct svm_peer_mem_context *)context;
    int ret;

    ka_task_down_read(&svm_context_sem);
    if ((mm_context == NULL) || (mm_context->inited_flag != SVM_PEER_INITED_FLAG)) {
#ifndef EMU_ST
        svm_peer_mem_err("Mm_context is null or isn't inited. (mm_context=%pK; inited_flag=%u)\n",
            mm_context, (mm_context != NULL ? mm_context->inited_flag : 0));
        ka_task_up_read(&svm_context_sem);
#endif
        return -EINVAL;
    }
    if (addr != mm_context->va || size != mm_context->len) {
        ka_task_up_read(&svm_context_sem);
        svm_peer_mem_err("Address or size is invalid. (addr=0x%lx; size=%lu)\n", addr, size);
        return -EINVAL;
    }

    ka_task_mutex_lock(&mm_context->context_mutex);
    if (mm_context->get_flag == 1) {
        svm_peer_mem_err("Already got pages. (hostpid=%d; devid=%d; vfid=%d; va=0x%lx)\n",
            mm_context->process_id.hostpid, mm_context->process_id.devid, mm_context->process_id.vfid, addr);
        ka_task_mutex_unlock(&mm_context->context_mutex);
        ka_task_up_read(&svm_context_sem);
        return -EINVAL;
    }

    ret = devmm_get_mem_pa_list(&mm_context->process_id, mm_context->va_aligned_start, mm_context->aligned_size,
        mm_context->pa_list, mm_context->pa_num);
    if (ret == 0) {
        mm_context->get_flag = 1;
    }

    ka_task_mutex_unlock(&mm_context->context_mutex);
    ka_task_up_read(&svm_context_sem);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(svm_peer_mem_get_pages);

int svm_peer_mem_dma_map(ka_sg_table_t *sg_head, void *context,
    ka_device_t *dma_device, int dmasync, int *nmap)
{
    struct svm_peer_mem_context *mm_context = (struct svm_peer_mem_context *)context;
    struct scatterlist *sg = NULL;
    u32 i = 0;
    int ret;

    ka_task_down_read(&svm_context_sem);
    if ((mm_context == NULL) || (mm_context->inited_flag != SVM_PEER_INITED_FLAG) ||
        (sg_head == NULL) || (nmap == NULL)) {
        svm_peer_mem_err("Input parameter is null or mm_context isn't inited. "
            "(mm_context_is_null=%d; inited_flag=%u; sg_head_is_null=%d; nmap_is_null=%d)\n",
            (mm_context == NULL), (mm_context != NULL ? mm_context->inited_flag : 0),
            (sg_head == NULL), (nmap == NULL));
        ka_task_up_read(&svm_context_sem);
        return -EINVAL;
    }
    ka_task_mutex_lock(&mm_context->context_mutex);
    if (mm_context->sg_head != NULL) {
        svm_peer_mem_err("Sg has been allocated. (hostpid=%d; devid=%d; vfid=%d; va=0x%llx)\n",
            mm_context->process_id.hostpid, mm_context->process_id.devid, mm_context->process_id.vfid,
            mm_context->va);
        ret = -EINVAL;
        goto dma_map_exit;
    }
    ret = ka_base_sg_alloc_table(sg_head, mm_context->pa_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ret != 0) {
        svm_peer_mem_err("Alloc error. (pa_num=%u; hostpid=%d; devid=%d; vfid=%d)\n", mm_context->pa_num,
            mm_context->process_id.hostpid, mm_context->process_id.devid, mm_context->process_id.vfid);
        goto dma_map_exit;
    }
    ka_base_for_each_sg(sg_head->sgl, sg, mm_context->pa_num, i)
    {
        if (sg == NULL) {
            ka_base_sg_free_table(sg_head);
            svm_peer_mem_err("Sg is null.\n");
            ret = -ENOMEM;
            goto dma_map_exit;
        }
        ka_base_sg_set_page(sg, NULL, mm_context->page_size, 0);
        /* if netcard smmu opened, dma_address need use dma_device to get dma addr */
        sg->dma_address = mm_context->pa_list[i];
        sg->dma_length = mm_context->page_size;
    }
    *nmap = mm_context->pa_num;
    mm_context->sg_head = sg_head;

dma_map_exit:
    ka_task_mutex_unlock(&mm_context->context_mutex);
    ka_task_up_read(&svm_context_sem);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(svm_peer_mem_dma_map);

int svm_peer_mem_dma_unmap(ka_sg_table_t *sg_head, void *context, ka_device_t *dma_device)
{
    struct svm_peer_mem_context *mm_context = (struct svm_peer_mem_context *)context;

    ka_task_down_read(&svm_context_sem);
    if ((mm_context == NULL)  || (sg_head == NULL) ||
        (mm_context->inited_flag != SVM_PEER_INITED_FLAG)) {
        svm_peer_mem_err("Mm_context or sg_head is null, or mm_context isn't inited. "
            "(mm_context_is_null=%d; inited_flag=%u; sg_head_is_null=%d)\n",
            (mm_context == NULL), (mm_context != NULL ? mm_context->inited_flag : 0), (sg_head == NULL));
        ka_task_up_read(&svm_context_sem);
        return -EINVAL;
    }
    ka_task_mutex_lock(&mm_context->context_mutex);
    if (sg_head != mm_context->sg_head) {
        ka_task_mutex_unlock(&mm_context->context_mutex);
        svm_peer_mem_err("Sg_head is not equal to map.\n");
        ka_task_up_read(&svm_context_sem);
        return -EINVAL;
    }

    ka_base_sg_free_table(sg_head);
    mm_context->sg_head = NULL;
    ka_task_mutex_unlock(&mm_context->context_mutex);
    ka_task_up_read(&svm_context_sem);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(svm_peer_mem_dma_unmap);

void svm_peer_mem_put_pages(ka_sg_table_t *sg_head, void *context)
{
    struct svm_peer_mem_context *mm_context = (struct svm_peer_mem_context *)context;

    ka_task_down_read(&svm_context_sem);
    if ((mm_context == NULL) || (mm_context->inited_flag != SVM_PEER_INITED_FLAG)) {
        svm_peer_mem_err("Mm_context is null or isn't inited. (mm_context_is_null=%d; inited_flag=%u)\n",
            (mm_context == NULL), (mm_context != NULL ? mm_context->inited_flag : 0));
        ka_task_up_read(&svm_context_sem);
        return;
    }
    ka_task_mutex_lock(&mm_context->context_mutex);
    if (mm_context->get_flag == 1) {
        mm_context->get_flag = 0;
        devmm_put_mem_pa_list(&mm_context->process_id, mm_context->va_aligned_start,
            mm_context->aligned_size, mm_context->pa_list, mm_context->pa_num);
    }

    ka_task_mutex_unlock(&mm_context->context_mutex);
    ka_task_up_read(&svm_context_sem);
}
KA_EXPORT_SYMBOL_GPL(svm_peer_mem_put_pages);

void svm_peer_mem_release(void *mm_context)
{
    struct svm_peer_mem_context *context = (struct svm_peer_mem_context *)mm_context;

    ka_task_down_write(&svm_context_sem);
    if ((context != NULL) && (context->inited_flag == SVM_PEER_INITED_FLAG)) {
        context->inited_flag = 0;
        if (context->sg_head != NULL) {
            ka_base_sg_free_table(context->sg_head);
            context->sg_head = NULL;
        }
        if (context->get_flag == 1) {
            context->get_flag = 0;
            devmm_put_mem_pa_list(&context->process_id, context->va_aligned_start,
                context->aligned_size, context->pa_list, context->pa_num);
        }
        ka_mm_vfree(context);
    }
    ka_task_up_write(&svm_context_sem);
}
KA_EXPORT_SYMBOL_GPL(svm_peer_mem_release);

#if defined(CFG_SUPPORT_REGISTER_PEER_MEM) && !defined(EMU_ST)

static struct peer_memory_client svm_mem_client = {
    .name = DRV_PEER_MEM_NAME,
    .version = DRV_PEER_MEM_VERSION,
    .acquire = svm_peer_mem_acquire,
    .get_pages = svm_peer_mem_get_pages,
    .dma_map = svm_peer_mem_dma_map,
    .dma_unmap = svm_peer_mem_dma_unmap,
    .put_pages = svm_peer_mem_put_pages,
    .get_page_size = svm_peer_mem_get_page_size,
    .release = svm_peer_mem_release,
};


static int *reg_handle = NULL;
#endif

STATIC int svm_peer_mem_ib_register_peer(void)
{
#if defined(CFG_SUPPORT_REGISTER_PEER_MEM) && !defined(EMU_ST)
    static invalidate_peer_memory mem_invalidate_callback;

    reg_handle = ib_register_ai_peer_memory_client(&svm_mem_client, &mem_invalidate_callback);
#endif
    return 0;
}

STATIC int __init svm_peer_mem_client_init(void)
{
    if (devmm_svm_need_ib_register_peer() == false) {
        return 0;
    }
    return svm_peer_mem_ib_register_peer();
}

STATIC void svm_peer_mem_ib_unregister_peer(void)
{
#if defined(CFG_SUPPORT_REGISTER_PEER_MEM) && !defined(EMU_ST)

    if (reg_handle != NULL) {
        ib_unregister_ai_peer_memory_client(reg_handle);
    }
#endif
}

STATIC void __exit svm_peer_mem_client_uninit(void)
{
    if (devmm_svm_need_ib_register_peer() == false) {
        return;
    }
    svm_peer_mem_ib_unregister_peer();
}

#ifndef EMU_ST
ka_module_init(svm_peer_mem_client_init);
ka_module_exit(svm_peer_mem_client_uninit);
#endif
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_LICENSE("GPL");
KA_MODULE_DESCRIPTION("svm peer mem driver");

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

#include <linux/export.h>
#include <linux/semaphore.h>

#include "dbl/ka_memory_query.h"
#include "svm_cmd.h"
#include "devmm_common.h"
#include "svm_proc_mng.h"
#include "svm_heap_mng.h"
#include "svm_mem_query.h"
#include "svm_dynamic_addr.h"

static int devmm_get_addr_type(struct devmm_svm_process_id *process_id, u64 addr, size_t size, unsigned int *addr_flag)
{
    if (((addr < DEVMM_SVM_MEM_START) && ((addr + size) < DEVMM_SVM_MEM_START)) ||
        (addr >= (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE))) {
        struct devmm_svm_process *svm_proc = devmm_svm_proc_get_by_process_id_ex(process_id);
        if (svm_proc != NULL) {
            *addr_flag = (svm_is_da_addr(svm_proc, addr, size)) ? DEVMM_SVM_ADDR : DEVMM_SHM_ADDR;
            devmm_svm_proc_put(svm_proc);
        } else {
            *addr_flag = DEVMM_SHM_ADDR;
        }
    } else if ((addr >= DEVMM_SVM_MEM_START) && (addr < (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE)) &&
        ((addr + size) <= (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE))) {
        *addr_flag = DEVMM_SVM_ADDR;
    } else {
        /* can not print log */
        return -EINVAL;
    }

    return 0;
}

bool devmm_check_addr_valid(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    u32 addr_flag = 0;
    int ret;

    if (process_id == NULL) {
        devmm_drv_err("Process_id is NULL. (addr=0x%llx; size=%llu)\n", addr, size);
        return false;
    }

    if (devmm_get_addr_type(process_id, addr, size, &addr_flag) != 0) {
        devmm_drv_err("Address error. (addr=0x%llx; size=%llu; addr_flag=%u)\n", addr, size, addr_flag);
        return false;
    }

    if (process_id->vfid != 0) {
        devmm_drv_err("Not support vm. (vf=%d)\n", process_id->vfid);
        return false;
    }
    if (addr_flag == DEVMM_SVM_ADDR) {
        ret = devmm_svm_check_addr_valid(process_id, addr, size);
    } else {
        ret = devmm_shm_check_addr_valid(process_id, addr, size);
    }
    if (ret != 0) {
        /* Error case handled as not svm */
        devmm_drv_err("Acquire error. (hostpid=%d; devid=%d; vfid=%d; addr=0x%llx; size=%llu)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, addr, size);
        return false;
    }

    return true;
}
KA_EXPORT_SYMBOL_GPL(devmm_check_addr_valid);

int devmm_get_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size,
    u64 *pa_list, u32 pa_num)
{
    u32 addr_flag = 0;
    int ret;

    if (process_id == NULL || pa_list == NULL) {
        devmm_drv_err("Process_id or pa_list is NULL. (addr=0x%llx; size=%llu)\n", addr, size);
        return -EINVAL;
    }

    if (devmm_get_addr_type(process_id, addr, size, &addr_flag) != 0) {
        devmm_drv_err("Address error. (addr=0x%llx; size=%llu; addr_flag=%u)\n", addr, size, addr_flag);
        return -EINVAL;
    }

    if (addr_flag == DEVMM_SVM_ADDR) {
        ret = devmm_svm_get_and_pin_pa_list(process_id, addr, size, pa_list, pa_num);
    } else {
        ret = devmm_shm_get_pa_list(process_id, addr, size, pa_list, pa_num);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(devmm_get_mem_pa_list);

void devmm_put_mem_pa_list(struct devmm_svm_process_id *process_id, u64 addr, u64 size,
    u64 *pa_list, u32 pa_num)
{
    u32 addr_flag = 0;

    if (process_id == NULL || pa_list == NULL) {
        devmm_drv_err("Process_id or pa_list is NULL. (addr=0x%llx; size=%llu)\n", addr, size);
        return;
    }

    if (devmm_get_addr_type(process_id, addr, size, &addr_flag) != 0) {
        devmm_drv_err("Address error. (addr=0x%llx; size=%llu; addr_flag=%u)\n", addr, size, addr_flag);
        return;
    }

    if (addr_flag == DEVMM_SVM_ADDR) {
        devmm_svm_put_pa_list(process_id, addr, pa_list, pa_num);
    } else {
        devmm_shm_put_pa_list(process_id, addr, pa_list, pa_num);
    }
}
KA_EXPORT_SYMBOL_GPL(devmm_put_mem_pa_list);

STATIC u32 devmm_svm_get_dev_mem_page_size(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    struct devmm_svm_process *svm_process = NULL;
    struct devmm_svm_heap *heap = NULL;
    u32 page_size;

    svm_process = devmm_svm_proc_get_by_process_id_ex(process_id);
    if (svm_process == NULL) {
        devmm_drv_err("Get svm process fail. (va=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n",
            addr, process_id->hostpid, process_id->devid, process_id->vfid);
        return 0;
    }
    heap = devmm_svm_heap_get(svm_process, addr);
    if (heap == NULL) {
        devmm_svm_proc_put(svm_process);
        devmm_drv_err("Get heap fail. (va=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n",
            addr, process_id->hostpid, process_id->devid, process_id->vfid);
        return 0;
    }
    page_size = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;
    devmm_svm_heap_put(heap);
    devmm_svm_proc_put(svm_process);

    return page_size;
}

u32 devmm_get_mem_page_size(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    u32 addr_flag = 0;

    if (process_id == NULL) {
        devmm_drv_err("Process_id is NULL. (addr=0x%llx; size=%llu)\n", addr, size);
        return 0;
    }
    if (devmm_get_addr_type(process_id, addr, size, &addr_flag) != 0) {
        devmm_drv_err("Address error. (addr=0x%llx; size=%llu; addr_flag=%u)\n", addr, size, addr_flag);
        return 0;
    }

    if (addr_flag == DEVMM_SVM_ADDR) {
        return devmm_svm_get_dev_mem_page_size(process_id, addr, size);
    } else {
        return devmm_shm_get_page_size(process_id, addr, size);
    }
}
KA_EXPORT_SYMBOL_GPL(devmm_get_mem_page_size);

static int devmm_get_svm_mem_pa_list_proc(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list)
{
#ifndef EMU_ST
    struct devmm_svm_process_id process_id = {.hostpid = tgid, .devid = devid, .vfid = 0};

    return devmm_get_mem_pa_list(&process_id, addr, size, pa_list, pa_num);
#endif
}

static int devmm_put_svm_mem_pa_list_proc(u32 devid, int tgid, u64 addr, u64 size, u32 pa_num, u64 *pa_list)
{
#ifndef EMU_ST
    struct devmm_svm_process_id process_id = {.hostpid = tgid, .devid = devid, .vfid = 0};

    devmm_put_mem_pa_list(&process_id, addr, size, pa_list, pa_num);

    return 0;
#endif
}
 
static u32 devmm_get_svm_mem_page_size_proc(u32 devid, int tgid, u64 addr, u64 size)
{
#ifndef EMU_ST
    struct devmm_svm_process_id process_id = {.hostpid = tgid, .devid = devid, .vfid = 0};
 
    return devmm_get_mem_page_size(&process_id, addr, size);
#endif
}

int devmm_svm_mem_query_ops_register(void)
{
    struct svm_mem_query_ops ops = {
        .get_svm_mem_pa = devmm_get_svm_mem_pa_list_proc,
        .put_svm_mem_pa = devmm_put_svm_mem_pa_list_proc,
        .get_svm_mem_page_size = devmm_get_svm_mem_page_size_proc
    };

    return hal_kernel_register_mem_query_ops(&ops);
}

void devmm_svm_mem_query_ops_unregister(void)
{
    hal_kernel_unregister_mem_query_ops();
}

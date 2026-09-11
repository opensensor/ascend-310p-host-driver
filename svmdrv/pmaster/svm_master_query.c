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
#include <linux/types.h>

#include "ascend_kernel_hal.h"
#include "devdrv_interface.h"
#include "dbl/feature_loader.h"
#include "kernel_version_adapt.h"
#include "svm_cmd.h"
#include "svm_interface.h"
#include "devmm_common.h"
#include "devmm_proc_info.h"
#include "devmm_page_cache.h"
#include "devmm_channel.h"
#include "svm_heap_mng.h"
#include "svm_proc_mng.h"
#include "svm_master_proc_mng.h"

#ifdef EMU_ST
int __thread g_no_need_free_preprocess = 0;
#endif
struct svm_p2p_mem_info {
    u64 va;
    u64 len;
    u32 hostpid;
    u32 udevid;
    void (*free_callback)(void *data);
    void *data;
    struct p2p_page_table page_table;
    ka_list_head_t node;
    ka_kref_t ref;
};

struct svm_mem_info {
    u64 va;
    u64 len;
};

int devmm_svm_check_addr_valid(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    struct devmm_svm_process *svm_process = NULL;
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    int ret;

    devmm_drv_debug("Check address details. (addr=0x%llx; size=%llu)\n", addr, size);
    ret = devmm_svm_proc_and_heap_get(process_id, addr, &svm_process, &heap);
    if (ret != 0) {
        devmm_drv_err("Process exit or heap error. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx)\n",
                      process_id->hostpid, process_id->devid, process_id->vfid, addr);
        return ret;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, addr);
    if ((page_bitmap == NULL) || (devmm_check_va_add_size_by_heap(heap, addr, size) != 0)) {
        devmm_svm_proc_and_heap_put(svm_process, heap);
        devmm_drv_err("Bitmap is error. (va=0x%llx; size=%llu)\n", addr, size);
        return -EADDRNOTAVAIL;
    }
    if (!devmm_page_bitmap_is_page_available(page_bitmap) ||
        !devmm_page_bitmap_is_dev_mapped(page_bitmap)) {
        devmm_svm_proc_and_heap_put(svm_process, heap);
        return -EADDRNOTAVAIL;
    }

    devmm_svm_proc_and_heap_put(svm_process, heap);
    return 0;
}

int devmm_shm_check_addr_valid(struct devmm_svm_process_id *process_id, u64 addr, u64 size)
{
    return -EINVAL;
}

static int devmm_get_host_addr_by_dev_pa(u32 dev_id, u64 va, u64 dev_pa, u64* host_addr)
{
    int ret = 0;

    /* read only addr just return dev pa, otherwise host can access by bar addr */
    if ((va >= DEVMM_READ_ONLY_ADDR_START) && (va <= DEVMM_DEV_READ_ONLY_ADDR_END)) {
        *host_addr = dev_pa;
    } else {
        ret = devdrv_devmem_addr_d2h(dev_id, dev_pa, host_addr);
    }
    return ret;
}

static int devmm_svm_get_host_pa_list(struct devmm_svm_process *svm_process, u64 addr, u64 size,
    u64 *pa_list, u32 pa_num)
{
    ka_vm_area_struct_t *vma = NULL;
    int ret;

    ka_task_down_read(get_mmap_sem(ka_task_get_current_mm()));
    vma = devmm_find_vma(svm_process, (unsigned long)addr);
    if ((vma == NULL) || (ka_mm_get_vm_start(vma) > addr)) {
        ka_task_up_read(get_mmap_sem(ka_task_get_current_mm()));
        devmm_drv_err("Find vma failed.\n");
        return -EINVAL;
    }

    ret = devmm_va_to_palist(vma, addr, size, pa_list, &pa_num);
    if (ret != 0) {
        ka_task_up_read(get_mmap_sem(ka_task_get_current_mm()));
        devmm_drv_err("Failed to convert va to pa. (ret=%d; pa_num=%u)\n", ret, pa_num);
        return ret;
    }

    ka_task_up_read(get_mmap_sem(ka_task_get_current_mm()));
    devmm_drv_debug("Convert va to pa. (ret=%d; pa_num=%u)\n", ret, pa_num);
    return 0;
}

static int devmem_svm_get_dev_pa_list(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
    struct svm_mem_info *mem_info, u64 *pa_list, u32 pa_num)
{
    u32 logic_id, dev_id, i, page_num, page_size;
    u32 stamp = (u32)ka_jiffies;
    u64 aligned_va = mem_info->va;
    u64 aligned_size = mem_info->len;
    u32 *page_bitmap = NULL;
    u64 dev_pa, va;
    int ret;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, aligned_va);
    if ((page_bitmap == NULL) || (devmm_check_va_add_size_by_heap(heap, aligned_va, aligned_size) != 0)) {
        devmm_drv_err("Bitmap is NULL. (va=0x%llx; size=%llx)\n", aligned_va, aligned_size);
        return -EINVAL;
    }
    page_size = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? devmm_svm->device_hpage_size
                                                          : devmm_svm->device_page_size;
    page_num = (u32)(aligned_size / page_size);
    if ((page_num > pa_num) || (page_num == 0)) {
        devmm_drv_err("Page_num is invalid. (va=0x%llx; page_num=%u; pa_num=%u)\n\n", aligned_va, page_num, pa_num);
        return -EINVAL;
    }

    logic_id = devmm_page_bitmap_get_devid(page_bitmap);
    dev_id = devmm_get_phyid_devid_from_svm_process(svm_process, logic_id);
    for (i = 0, va = aligned_va; i < page_num; i++, va += page_size) {
        ret = devmm_find_pa_cache(svm_process, logic_id, va, page_size, &dev_pa);
        if (ret != 0) {
            devmm_drv_err("Find pa cache failed. (va=0x%llx; num=%u)\n", aligned_va, i);
            return ret;
        }
        if (devmm_get_host_addr_by_dev_pa(dev_id, va, dev_pa, &pa_list[i]) != 0) {
            devmm_drv_err("Convert host address failed. (devid=%u; va=0x%llx; num=%u)\n", logic_id, aligned_va, i);
            return -ENOMEM;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

int devmm_svm_get_and_pin_pa_list(struct devmm_svm_process_id *process_id, u64 aligned_va, u64 aligned_size,
    u64 *pa_list, u32 pa_num)
{
    struct devmm_svm_process *svm_process = NULL;
    struct devmm_svm_heap *heap = NULL;
    int ret;

    ret = devmm_svm_proc_and_heap_get(process_id, aligned_va, &svm_process, &heap);
    if (ret != 0) {
        devmm_drv_err("Process exit or heap error. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx)\n",
                      process_id->hostpid, process_id->devid, process_id->vfid, aligned_va);
        return -EINVAL;
    }

    if (heap->heap_type == DEVMM_HEAP_PINNED_HOST) {
        ret = devmm_svm_get_host_pa_list(svm_process, aligned_va, aligned_size, pa_list, pa_num);
    } else {
        struct svm_mem_info mem_info = {.va = aligned_va, .len = aligned_size};
        ret = devmem_svm_get_dev_pa_list(svm_process, heap, &mem_info, pa_list, pa_num);
    }
    if (ret != 0) {
        devmm_svm_proc_and_heap_put(svm_process, heap);
        devmm_drv_err("Fail to get pa list. (hostpid=%d; devid=%u; vfid=%u; va=0x%llx; size=%llx; heap_type=%d)\n",
            process_id->hostpid, process_id->devid, process_id->vfid, aligned_va, aligned_size, heap->heap_type);
        return ret;
    }

    /*
     * inc ref of cur va to avoid free addr when it is in used.
     */
    if (devmm_inc_page_ref(svm_process, aligned_va, aligned_size) != 0) {
        devmm_svm_proc_and_heap_put(svm_process, heap);
        devmm_drv_err("Address reference failed. (pid=%u; devid=%u; va=0x%llx)\n",
            process_id->hostpid, process_id->devid, aligned_va);
        return -EINVAL;
    }

    devmm_svm_proc_and_heap_put(svm_process, heap);
    return 0;
}

int devmm_shm_get_pa_list(struct devmm_svm_process_id *process_id, u64 aligned_va, u64 aligned_size,
    u64 *pa_list, u32 pa_num)
{
    return -EINVAL;
}

void devmm_svm_put_pa_list(struct devmm_svm_process_id *process_id, u64 va, u64 *pa_list, u32 pa_num)
{
    struct devmm_svm_process *svm_process = NULL;
    u64 size = pa_num << KA_MM_PAGE_SHIFT;

    svm_process = devmm_svm_proc_get_by_process_id_ex(process_id);
    if (svm_process == NULL) {
        devmm_drv_err("Process is exit. (va=0x%llx; hostpid=%d; devid=%u; vfid=%u)\n",
                      va, process_id->hostpid, process_id->devid, process_id->vfid);
        return;
    }
    devmm_dec_page_ref(svm_process, va, size);
    devmm_svm_proc_put(svm_process);
}

void devmm_shm_put_pa_list(struct devmm_svm_process_id *process_id, u64 va, u64 *pa_list, u32 pa_num)
{
}

bool devmm_svm_need_ib_register_peer(void)
{
    return false;
}
KA_EXPORT_SYMBOL_GPL(devmm_svm_need_ib_register_peer);

static bool devmm_is_support_p2p_get_pages(struct devmm_svm_process *svm_proc, u64 va, u64 len)
{
    struct devmm_memory_attributes attr = {0};
    struct devmm_svm_heap *heap = NULL;
    u32 *bitmap = NULL;
    u32 page_size;
    int ret;

    heap = devmm_svm_heap_get(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return false;
    }

    if ((heap->heap_sub_type == SUB_HOST_TYPE) || (heap->heap_sub_type == SUB_SVM_TYPE)) {
        devmm_drv_err("No support heap_sub_type. (va=0x%llx; len=0x%llx; heap_sub_type=%u)\n", va, len, heap->heap_sub_type);
        devmm_svm_heap_put(heap);
        return false;
    }

    page_size = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? devmm_svm->device_hpage_size : devmm_svm->device_page_size;
    if (KA_MM_IS_ALIGNED(va, page_size) == false) {
        devmm_drv_err("Va is not aligned. (va=0x%llx; len=0x%llx; page_size=%u)\n", va, len, page_size);
        devmm_svm_heap_put(heap);
        return false;
    }

    ret = devmm_check_va_add_size_by_heap(heap, va, len);
    if (ret != 0) {
        devmm_drv_err("Len is invalid. (va=0x%llx; len=0x%llx; ret=%d)\n", va, len, ret);
        devmm_svm_heap_put(heap);
        return false;
    }

    bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (bitmap == NULL) {
        devmm_drv_err("Get bitmap error. (va=0x%llx; len=0x%llx; heap_sub_type=%u)\n", va, len, heap->heap_sub_type);
        devmm_svm_heap_put(heap);
        return false;
    }

    if (devmm_page_bitmap_is_ipc_open_mem(bitmap) || devmm_page_bitmap_is_advise_readonly(bitmap)) {
        devmm_drv_err("No support attr. (va=%llx; len=0x%llx; bitmap=%u; heap_type=%u)\n", va, len, *bitmap, heap->heap_type);
        devmm_svm_heap_put(heap);
        return false;
    }

    ret = devmm_get_svm_mem_attrs(svm_proc, va, &attr);
    if (ret != 0) {
        devmm_drv_err("Get mem_attr failed. (addr=0x%llx; ret=%d)\n", va, ret);
        devmm_svm_heap_put(heap);
        return false;
    }
    devmm_svm_heap_put(heap);

    if (attr.is_mem_import) {
        devmm_drv_err("No support attr. (va=0x%llx; len=0x%llx)\n", va, len);
        return false;
    }

    return true;
}

static int devmm_fill_p2p_pages_info(struct devmm_svm_process *svm_proc, struct svm_p2p_mem_info *mem_info,
    struct p2p_page_info *info)
{
    u32 logic_devid = devmm_svm_va_to_devid(svm_proc, mem_info->va);
    u32 page_size = mem_info->page_table.page_size;
    u64 aligned_va = ka_base_round_down(mem_info->va, page_size);
    u64 page_num = mem_info->page_table.page_num;
    u32 stamp = (u32)ka_jiffies;
    u64 i, tmp_va, dev_pa;
    int ret;

    for (i = 0, tmp_va = aligned_va; i < page_num; i++, tmp_va += page_size) {
        ret = devmm_find_pa_cache(svm_proc, logic_devid, tmp_va, page_size, &dev_pa);
        if (ret != 0) {
            devmm_drv_err("Find pa cache failed. (i=%llu; va=0x%llx; page_size=%u; logic_devid=%u; ret=%d)\n",
                i, tmp_va, page_size, logic_devid, ret);
            return ret;
        }
        ret = devdrv_devmem_addr_d2h(mem_info->udevid, (phys_addr_t)dev_pa, &info[i].pa);
        if (ret != 0) {
            devmm_drv_err("Transform addr d2h failed. (i=%llu; va=0x%llx; page_size=%u; devid=%u; ret=%d)\n",
                i, tmp_va, page_size, mem_info->udevid, ret);
            return ret;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
}

static int devmm_init_p2p_page_table(struct devmm_svm_process *svm_proc, struct p2p_page_table *page_table)
{
    struct svm_p2p_mem_info *mem_info = ka_container_of(page_table, struct svm_p2p_mem_info, page_table);
    struct devmm_svm_heap *heap = NULL;
    u64 aligned_va, aligned_len;
    int ret;

    heap = devmm_svm_heap_get(svm_proc, mem_info->va);
    if (heap == NULL) {
        devmm_drv_err("Get heap fail. (va=0x%llx; hostpid=%u)\n", mem_info->va, svm_proc->process_id.hostpid);
        return -EINVAL;
    }
    page_table->version = P2P_GET_PAGE_VERSION;
    page_table->page_size = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ?
        devmm_svm->device_hpage_size : devmm_svm->device_page_size;
    devmm_svm_heap_put(heap);
    aligned_va = ka_base_round_down(mem_info->va, page_table->page_size);
    aligned_len = ka_base_round_up(mem_info->len + (mem_info->va - aligned_va), page_table->page_size);
    page_table->page_num = aligned_len / page_table->page_size;
    page_table->pages_info = (struct p2p_page_info *)
        devmm_kvzalloc(page_table->page_num * sizeof(struct p2p_page_info));
    if (page_table->pages_info == NULL) {
        devmm_drv_err("Kvzalloc failed. (size=%llu)\n", page_table->page_num * sizeof(struct p2p_page_info));
        return -ENOMEM;
    }

    devmm_drv_debug("Init p2p page table. (va=0x%llx; aligned_va=0x%llx; len=0x%llx; aligned_len=0x%llx; page_size=%llu; page_num=%llu)\n",
        mem_info->va, aligned_va, mem_info->len, aligned_len, page_table->page_size, page_table->page_num);
    ret = devmm_fill_p2p_pages_info(svm_proc, mem_info, page_table->pages_info);
    if (ret != 0) {
        devmm_kvfree(page_table->pages_info);
        return ret;
    }
    return 0;
}

static void devmm_uninit_p2p_page_table(struct p2p_page_table *page_table)
{
    if (page_table->pages_info != NULL) {
        devmm_kvfree(page_table->pages_info);
        page_table->pages_info = NULL;
        page_table->page_num = 0;
        page_table->page_size = 0;
    }
}

static struct svm_p2p_mem_info *devmm_create_p2p_mem_info(struct devmm_svm_process *svm_proc,
    u64 va, u64 len, void (*free_callback)(void *data), void *data)
{
    struct svm_p2p_mem_info *mem_info = NULL;
    u32 logic_devid;
    int ret;

    logic_devid = devmm_svm_va_to_devid(svm_proc, va);
    if (logic_devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Logic devid is invalid. (va=0x%llx; logic_devid=%d)\n", va, logic_devid);
        return NULL;
    }

    mem_info = (struct svm_p2p_mem_info *)devmm_kvzalloc(sizeof(struct svm_p2p_mem_info));
    if (mem_info == NULL) {
        devmm_drv_err("Kvalloc failed. (size=%llu)\n", sizeof(struct svm_p2p_mem_info));
        return NULL;
    }

    mem_info->va = va;
    mem_info->len = len;
    mem_info->hostpid = svm_proc->process_id.hostpid;
    mem_info->udevid = devmm_get_phyid_devid_from_svm_process(svm_proc, logic_devid);
    mem_info->free_callback = free_callback;
    mem_info->data = data;
    KA_INIT_LIST_HEAD(&mem_info->node);
    ka_base_kref_init(&mem_info->ref);

    ret = devmm_init_p2p_page_table(svm_proc, &mem_info->page_table);
    if (ret != 0) {
        devmm_drv_err("Init p2p page table failed. (va=0x%llx; len=0x%llx; ret=%d)\n", va, len, ret);
        devmm_kvfree(mem_info);
        return NULL;
    }

    return mem_info;
}

static void devmm_p2p_mem_info_get(struct svm_p2p_mem_info *mem_info)
{
    ka_base_kref_get(&mem_info->ref);
}

static void devmm_p2p_mem_info_release(ka_kref_t *kref)
{
    struct svm_p2p_mem_info *mem_info = ka_container_of(kref, struct svm_p2p_mem_info, ref);
    devmm_uninit_p2p_page_table(&mem_info->page_table);
    devmm_kvfree(mem_info);
}

static void devmm_p2p_mem_info_put(struct svm_p2p_mem_info *mem_info)
{
    ka_base_kref_put(&mem_info->ref, devmm_p2p_mem_info_release);
}

static void devmm_destroy_p2p_mem_info(struct svm_p2p_mem_info *mem_info)
{
    devmm_p2p_mem_info_put(mem_info);
}

static struct p2p_page_table *devmm_create_p2p_page_table(struct devmm_svm_process *svm_proc,
    u64 va, u64 len, void (*free_callback)(void *data), void *data)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct svm_p2p_mem_info *mem_info = NULL;

    mem_info = devmm_create_p2p_mem_info(svm_proc, va, len, free_callback, data);
    if (mem_info == NULL) {
        return NULL;
    }
    ka_task_mutex_lock(&master_data->p2p_mem_mng[mem_info->udevid].lock);
    ka_list_add_tail(&mem_info->node, &master_data->p2p_mem_mng[mem_info->udevid].list_head);
    master_data->p2p_mem_mng[mem_info->udevid].get_cnt++;
    ka_task_mutex_unlock(&master_data->p2p_mem_mng[mem_info->udevid].lock);

    devmm_drv_debug("Success. (va=0x%llx; len=0x%llx)\n", va, len);
    return &mem_info->page_table;
}

static void devmm_destroy_p2p_page_table(struct p2p_page_table **page_table)
{
    struct svm_p2p_mem_info *mem_info = ka_container_of(*page_table, struct svm_p2p_mem_info, page_table);
    struct devmm_svm_process_id process_id = {.hostpid = mem_info->hostpid, .devid = 0, .vfid = 0};
    struct devmm_svm_proc_master *master_data = NULL;
    struct devmm_svm_process *svm_proc = NULL;

    svm_proc = devmm_svm_proc_get_by_process_id(&process_id);
    if (svm_proc == NULL) {
        devmm_drv_debug("Process has exited. (hostpid=%d; va=0x%llx; len=0x%llx)\n",
            mem_info->hostpid, mem_info->va, mem_info->len);
        goto OUT;
    }

    master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    ka_task_mutex_lock(&master_data->p2p_mem_mng[mem_info->udevid].lock);
    if (!ka_list_empty_careful(&mem_info->node)) {
        ka_list_del(&mem_info->node);
    }

    master_data->p2p_mem_mng[mem_info->udevid].put_cnt++;
    ka_task_mutex_unlock(&master_data->p2p_mem_mng[mem_info->udevid].lock);
    devmm_svm_proc_put(svm_proc);
OUT:
    devmm_drv_debug("Success. (va=0x%llx; len=0x%llx)\n", mem_info->va, mem_info->len);
    devmm_destroy_p2p_mem_info(mem_info);
    *page_table = NULL;
}

static void devmm_show_p2p_mem_stat(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_master_p2p_mem_mng *mem_mng = &master_data->p2p_mem_mng[devid];

    ka_task_mutex_lock(&mem_mng->lock);
    if (mem_mng->get_cnt == 0) {
        ka_task_mutex_unlock(&mem_mng->lock);
        return;
    }
    if ((mem_mng->get_cnt == mem_mng->put_cnt)) {
        devmm_drv_debug("Stat show. (devid=%u; get_cnt=%llu; put_cnt=%llu; free_callback_cnt=%llu)\n",
            devid, mem_mng->get_cnt, mem_mng->put_cnt, mem_mng->free_cb_cnt);
    } else {
        devmm_drv_run_info("Stat show. (devid=%u; get_cnt=%llu; put_cnt=%llu; free_callback_cnt=%llu)\n",
            devid, mem_mng->get_cnt, mem_mng->put_cnt, mem_mng->free_cb_cnt);
    }
    ka_task_mutex_unlock(&mem_mng->lock);
}

typedef bool (*preprocess_cmp_fun)(struct svm_p2p_mem_info *mem_info, u64 va, u64 len);
static bool devmm_p2p_mem_info_va_is_overlap(struct svm_p2p_mem_info *mem_info, u64 free_va, u64 free_len)
{
    if ((free_va + free_len <= mem_info->va) || (free_va >= mem_info->va + mem_info->len)) {
        return false;
    }
    return true;
}

static struct svm_p2p_mem_info *_devmm_p2p_mem_info_erase_and_get(struct devmm_master_p2p_mem_mng *mem_mng,
    preprocess_cmp_fun cmp_fun, u64 free_va, u64 free_len)
{
    u32 stamp = (u32)ka_jiffies;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    if (ka_list_empty_careful(&mem_mng->list_head)) {
        return NULL;
    }

    ka_list_for_each_safe(pos, n, &mem_mng->list_head) {
        struct svm_p2p_mem_info *mem_info = ka_list_entry(pos, struct svm_p2p_mem_info, node);
        bool need_erase = (cmp_fun != NULL) ? cmp_fun(mem_info, free_va, free_len) : true;
        if (need_erase) {
            ka_list_del(&mem_info->node);
            KA_INIT_LIST_HEAD(&mem_info->node);
            mem_mng->free_cb_cnt++;
            devmm_drv_debug("Erase info. (devid=%u; va=%llx; len=%llx; free_cb_cnt=%llu)",
                mem_info->udevid, mem_info->va, mem_info->len, mem_mng->free_cb_cnt);
            devmm_p2p_mem_info_get(mem_info);
            return mem_info;
        }
        devmm_try_cond_resched(&stamp);
    }

    return NULL;
}

static struct svm_p2p_mem_info *devmm_p2p_mem_info_erase_and_get(struct devmm_master_p2p_mem_mng *mem_mng,
    preprocess_cmp_fun cmp_fun, u64 free_va, u64 free_len)
{
    struct svm_p2p_mem_info *mem_info = NULL;

    ka_task_mutex_lock(&mem_mng->lock);
    mem_info = _devmm_p2p_mem_info_erase_and_get(mem_mng, cmp_fun, free_va, free_len);
    ka_task_mutex_unlock(&mem_mng->lock);

    return mem_info;
}

static void devmm_mem_free_preprocess_inner(struct devmm_svm_process *svm_proc, u32 devid,
    preprocess_cmp_fun cmp_fun, u64 free_va, u64 free_len)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    struct devmm_master_p2p_mem_mng *mem_mng = &master_data->p2p_mem_mng[devid];
    struct svm_p2p_mem_info *mem_info = NULL;
    u32 stamp = (u32)ka_jiffies;

    do {
        mem_info = devmm_p2p_mem_info_erase_and_get(mem_mng, cmp_fun, free_va, free_len);
        if (mem_info == NULL) {
            break;
        }
        /* if hal_kernel_p2p_put_pages is called in free_callback, mem_info may been free */
        if (mem_info->free_callback != NULL) {
            mem_info->free_callback(mem_info->data);
            devmm_drv_debug("free_callback finish.\n");
        }
        devmm_p2p_mem_info_put(mem_info);
        devmm_try_cond_resched(&stamp);
    } while (1);
}

void devmm_mem_free_preprocess_by_dev_and_va(struct devmm_svm_process *svm_proc, u32 devid, u64 free_va, u64 free_len)
{
    devmm_mem_free_preprocess_inner(svm_proc, devid, devmm_p2p_mem_info_va_is_overlap, free_va, free_len);
}

void devmm_mem_free_preprocess_by_dev(struct devmm_svm_process *svm_proc, u32 devid)
{
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    devmm_drv_debug("Free preprocess by dev. (devid=%u)\n", devid);
    if (devid == SVM_MAX_AGENT_NUM) {
        for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
            devmm_mem_free_preprocess_inner(svm_proc, i, NULL, 0, 0);
            devmm_show_p2p_mem_stat(svm_proc, i);
            devmm_try_cond_resched(&stamp);
        }
    } else {
        if (devid < SVM_MAX_AGENT_NUM) {
#ifdef EMU_ST
            if (g_no_need_free_preprocess) {
                return;
            }
#endif
            devmm_mem_free_preprocess_inner(svm_proc, devid, NULL, 0, 0);
            devmm_show_p2p_mem_stat(svm_proc, devid);
        }
    }
}

int hal_kernel_p2p_get_pages(u64 va, u64 len, void (*free_callback)(void *data), void *data,
    struct p2p_page_table **page_table)
{
    struct devmm_svm_process_id process_id = {.hostpid = ka_task_get_current_tgid(), .devid = 0, .vfid = 0};
    struct devmm_svm_process *svm_proc = NULL;

    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        return -EPERM;
    }

    ka_task_might_sleep();
    devmm_drv_debug("Start. (va=0x%llx; len=0x%llx)\n", va, len);
    if ((devmm_va_is_in_svm_range(va) == false) || (len == 0) || (va + len < va) ||
        (devmm_va_is_in_svm_range(va + len - 1) == false) || (free_callback == NULL) || (page_table == NULL)) {
        devmm_drv_err("Invalid para. (va=0x%llx; len=0x%llx; page_table_is_null=%u; free_callback_is_null=%u)\n",
            va, len, free_callback == NULL, page_table == NULL);
        ka_system_module_put(KA_THIS_MODULE);
        return -EINVAL;
    }

    svm_proc = devmm_svm_proc_get_by_process_id(&process_id);
    if (svm_proc == NULL) {
        devmm_drv_err("Can not find svm_proc. (hostpid=%d; va=0x%llx; len=0x%llx)\n", process_id.hostpid, va, len);
        ka_system_module_put(KA_THIS_MODULE);
        return -ESRCH;
    }

    if (devmm_is_support_p2p_get_pages(svm_proc, va, len) == false) {
        devmm_svm_proc_put(svm_proc);
        ka_system_module_put(KA_THIS_MODULE);
        return -EOPNOTSUPP;
    }

    *page_table = devmm_create_p2p_page_table(svm_proc, va, len, free_callback, data);
    devmm_svm_proc_put(svm_proc);
    ka_system_module_put(KA_THIS_MODULE);

    return (*page_table == NULL) ? -EINVAL : 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_p2p_get_pages);

int hal_kernel_p2p_put_pages(struct p2p_page_table *page_table)
{
    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        return -EBUSY;
    }

    if (page_table == NULL) {
        devmm_drv_err("Invalid para. (page_table_is_null=%u)\n", page_table == NULL);
        ka_system_module_put(KA_THIS_MODULE);
        return -EINVAL;
    }

    ka_task_might_sleep();
    devmm_destroy_p2p_page_table(&page_table);

    ka_system_module_put(KA_THIS_MODULE);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_p2p_put_pages);

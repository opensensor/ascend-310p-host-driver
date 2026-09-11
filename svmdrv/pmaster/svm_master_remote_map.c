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
#include <linux/version.h>
#if defined(__arm__) || defined(__aarch64__)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
#include <asm/pgtable-prot.h>
#else
#include <asm/pgtable.h>
#endif
#elif defined(__sw_64__)
#include <linux/mm_types.h>
#include <asm/pgtable.h>
#else
#include <asm/pgtable_types.h>
#endif
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "devdrv_manager_comm.h"
#include "kernel_version_adapt.h"
#include "devmm_proc_info.h"
#include "devmm_channel.h"
#include "devmm_common.h"
#include "svm_msg_client.h"
#include "svm_proc_mng.h"
#include "svm_heap_mng.h"
#include "devmm_page_cache.h"
#include "svm_mem_mng.h"
#include "svm_master_dev_capability.h"
#include "devmm_mem_alloc_interface.h"
#include "devmm_proc_mem_copy.h"
#include "devmm_dev.h"
#include "svm_master_remote_map.h"

#define DEVMM_TXATU_DISABLE 0
#define DEVMM_IOMEM "/proc/iomem"
#define DEVMM_SYS_MEM_MAX_LEN 256
#define DEVMM_SYS_MEM_STR_LEN 32
#define DEVMM_SHM_DOUBLE 2
#define DEVMM_SYS_MEM_MEMBER_NUM 4
#define DEVMM_SYS_RAM "SystemRAM"
#define DEVMM_SYS_STR "System"
#define DEVMM_SHM_FREE_SIZE_PRE_MSG (0x280000000) /* 10G */
#define DEVMM_GET_2M_PAGE_NUM 512u      /* the normal page_num of 2M */

struct devmm_palist_info {
    u64 *pa_blk;    /* inpara, outpara */
    u32 blk_num;    /* inpara */
    u32 got_num;    /* outpara */
    u32 page_size;  /* outpara */
};

struct devmm_sys_mem_info {
    u32 succ_flag;
    u32 config_index;
    u64 mem_addr[DEVMM_MAX_TXATU];
    u64 memsz[DEVMM_MAX_TXATU];
};

#define DEVMM_LOCAL_HOST_MEM 0U
#define DEVMM_LOCKED_HOST_MEM 1U
#define DEVMM_LOCAL_DEV_MEM 2U
#define DEVMM_LOCKED_DEV_MEM 3U
#define DEVMM_MAX_MEM_TYPE 4U

static struct devmm_sys_mem_info g_mem_info;

static inline bool devmm_is_local_dev_mem_type(u32 map_type)
{
    return (map_type == DEV_MEM_MAP_HOST);
}

static bool devmm_is_mem_map_by_pcie_th(u32 map_type)
{
    return ((map_type == HOST_MEM_MAP_DEV_PCIE_TH) || (map_type == HOST_IO_MAP_DEV));
}

static u32 devmm_get_remote_mem_type(struct devmm_memory_attributes *attr, u32 map_type)
{
    if (devmm_is_local_dev_mem_type(map_type)) {
        return DEVMM_LOCAL_DEV_MEM;
    } else if (attr->is_local_host) {
        return DEVMM_LOCAL_HOST_MEM;
    } else if (attr->is_locked_host) {
        return DEVMM_LOCKED_HOST_MEM;
    } else if (attr->is_locked_device) {
        return DEVMM_LOCKED_DEV_MEM;
    } else {
        return DEVMM_MAX_MEM_TYPE;
    }
}

void devmm_get_sys_mem(void)
{
    struct devmm_sys_mem_info *mem_info = &g_mem_info;
    char mem_str[DEVMM_SYS_MEM_STR_LEN * DEVMM_SHM_DOUBLE] = {0};
    char mem_buf[DEVMM_SYS_MEM_MAX_LEN] = {0};
    ka_file_t *fp = NULL;
    u64 addr_start = 0;
    u64 addr_end = 0;
    u64 temp_addr_end = 0;
    loff_t offset = 0;
    u32 ret;

    fp = ka_fs_filp_open(DEVMM_IOMEM, KA_O_RDONLY, DEVMM_DEVICE_AUTHORITY);
    if (KA_IS_ERR((void const *)fp) || (fp == NULL)) {
        devmm_drv_err("Create file error, cannot use shm. (errno=%ld)\n", KA_PTR_ERR(fp));
        return;
    }

    while (devmm_read_line(fp, &offset, mem_buf, DEVMM_SYS_MEM_MAX_LEN) != NULL) {
        ret = (u32)sscanf_s(mem_buf, "%llx-%llx : %s %s", &addr_start, &addr_end, mem_str, DEVMM_SYS_MEM_STR_LEN,
            (mem_str + ka_base_strlen(DEVMM_SYS_STR)), DEVMM_SYS_MEM_STR_LEN);
        if ((ret != DEVMM_SYS_MEM_MEMBER_NUM) || (ka_base_strcmp(DEVMM_SYS_RAM, mem_str) != 0)) {
            continue;
        }

        if (temp_addr_end == 0) {
#ifndef EMU_ST
            mem_info->mem_addr[mem_info->config_index] = ka_base_round_down(addr_start, KA_HPAGE_SIZE);
#endif
            temp_addr_end = mem_info->mem_addr[mem_info->config_index] + CONFIG_TXATU_MAX_LEN;
        }
        while (addr_end > temp_addr_end) {
            mem_info->memsz[mem_info->config_index] = CONFIG_TXATU_MAX_LEN;
            mem_info->config_index++;
            if (mem_info->config_index >= DEVMM_MAX_TXATU) {
                devmm_drv_err("Out of range, cannot use shm. (index=%u)\n", mem_info->config_index);
                ka_fs_filp_close(fp, NULL);
                fp = NULL;
                return;
            }
#ifndef EMU_ST
            mem_info->mem_addr[mem_info->config_index] =
                ka_base_round_down(((addr_start > temp_addr_end) ? addr_start : temp_addr_end), KA_HPAGE_SIZE);
#endif
            temp_addr_end = mem_info->mem_addr[mem_info->config_index] + CONFIG_TXATU_MAX_LEN;
        }
        /* reset the context in mem_str */
        mem_str[0] = 0;
        devmm_drv_debug("System ram iomem. (addr_start=0x%llx; addr_end=%llx)\n", addr_start, addr_end);
    }
    mem_info->memsz[mem_info->config_index] = CONFIG_TXATU_MAX_LEN;
    mem_info->succ_flag = 1;
    ka_fs_filp_close(fp, NULL);
    fp = NULL;

    return;
}

static void devmm_del_txatu_by_dev_id(struct devmm_txatu_info *txatu_info, u32 txatu_num, u32 hostpid, u32 devid)
{
    u32 i;
    int ret;

    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        return;
    }
    for (i = 0; i < txatu_num; i++) {
        if ((txatu_info[i].txatu_flag & (1UL << devid)) == 0) {
            continue;
        }
        ret = devdrv_device_txatu_del((int)hostpid, devid, txatu_info[i].addr);
        if (ret != 0) {
            devmm_drv_warn("Txatu delete failed. (devid=%u; addr=0x%llx; hostpid=%d; txatu_flag=0x%llx; ret=%d)\n",
                devid, txatu_info[i].addr, hostpid, txatu_info[i].txatu_flag, ret);
            continue;
        }
        txatu_info[i].txatu_flag &= ~(1UL << devid);
        devmm_drv_info("Delete txatu success. (devid=%u; addr=0x%llx; hostpid=%d; txatu_flag=0x%llx)\n",
            devid, txatu_info[i].addr, hostpid, txatu_info[i].txatu_flag);
    }
}

static void devmm_del_txatu_by_proc(struct devmm_txatu_info *txatu_info, u32 txatu_num, u32 hostpid)
{
    u32 devid;

    for (devid = 0; devid < DEVMM_MAX_DEVICE_NUM; devid++) {
        devmm_del_txatu_by_dev_id(txatu_info, txatu_num, hostpid, devid);
    }

    return;
}

void devmm_delete_shm_pro_node(struct devmm_shm_pro_node *shm_pro_node)
{
    if (shm_pro_node == NULL) {
        return;
    }

    if (ka_list_empty_careful(&shm_pro_node->shm_head.list) != 0) {
        devmm_del_txatu_by_proc(&shm_pro_node->txatu_info[0], DEVMM_MAX_TXATU, shm_pro_node->hostpid);
        ka_list_del(&shm_pro_node->list);
        devmm_kfree_ex(shm_pro_node);
    }
    return;
}

static struct devmm_shm_node *devmm_get_shm_node(struct devmm_shm_head *shm_head,
    u64 src_va, u32 devid, u32 vfid)
{
    struct devmm_shm_node *node = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    head = &shm_head->list;

    ka_list_for_each_safe(pos, n, head) {
        node = ka_list_entry(pos, struct devmm_shm_node, list);
        if ((node->dev_id == devid) && (node->vfid == vfid) && (node->src_va == src_va)) {
            devmm_drv_debug("Get shm success. (node_src_va=0x%llx; dst_va=0x%llx; size=%llu)\n",
                node->src_va, node->dst_va, node->size);
            return node;
        }
    }

    return NULL;
}

struct devmm_shm_node *devmm_get_shm_node_by_dva(struct devmm_shm_head *shm_head,
    u64 dst_va, u64 size, u32 devid, u32 vfid)
{
    struct devmm_shm_node *node = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    head = &shm_head->list;

    ka_list_for_each_safe(pos, n, head) {
        node = ka_list_entry(pos, struct devmm_shm_node, list);
        if ((node->dev_id == devid) && (node->vfid == vfid)  && (dst_va >= node->dst_va) &&
            (size <= node->size) && ((dst_va + size) <= (node->dst_va + node->size))) {
            devmm_drv_debug("Get shm success. (node_src_va=0x%llx; device_va=0x%llx; size=%llu)\n",
                node->src_va, node->dst_va, node->size);
            return node;
        }
    }

    return NULL;
}

static int devmm_shm_query_pagesize(struct devmm_svm_process *svm_proc, u64 va, u32 *page_size)
{
    ka_vm_area_struct_t *vma = NULL;
    ka_pmd_t *pmd = NULL;
    int ret;

    ka_task_down_read(get_mmap_sem(svm_proc->mm));
    vma = ka_mm_find_vma(svm_proc->mm, va);
    if ((vma == NULL) || (ka_mm_get_vm_start(vma) > va)) {
#ifndef EMU_ST
        ka_task_up_read(get_mmap_sem(svm_proc->mm));
        devmm_drv_err("Vma not exist. (host_va=0x%llx)\n", va);
        return -EINVAL;
#endif
    }

    *page_size = KA_HPAGE_SIZE;
    ret = devmm_va_to_pmd(vma, va, DEVMM_TRUE, &pmd);
    if (ret != 0) {
        /* don't return err, just return KA_MM_PAGE_SIZE */
        *page_size = KA_MM_PAGE_SIZE;
    }
    ka_task_up_read(get_mmap_sem(svm_proc->mm));

    return 0;
}

static int devmm_shm_try_query_hpagesize(struct devmm_svm_process *svm_proc,
    struct devmm_shm_node *node, u64 va, u64 dst_va, u32 *page_size)
{
    u64 queried_num = (va - node->src_va) / KA_MM_PAGE_SIZE;
    int cycle_num = devmm_svm->device_hpage_size / KA_MM_PAGE_SIZE;
    u64 total_num = node->page_num;
    u64 pre_pa = 0;
    u64 pa;
    u32 host_flag;
    int ret;

    ret = devmm_get_host_phy_mach_flag(node->dev_id, &host_flag);
    if (ret != 0) {
#ifndef EMU_ST
        return ret;
#endif
    }

    /* if host_va is not aligned by KA_HPAGE_SIZE, return KA_MM_PAGE_SIZE */
    if ((node->src_va & (KA_HPAGE_SIZE - 1)) != 0) {
        *page_size = KA_MM_PAGE_SIZE;
    } else {
        ret = devmm_shm_query_pagesize(svm_proc, va, page_size);
        if (ret != 0) {
            devmm_drv_err("Get page failed. (devid=%u; host_va=0x%llx)\n", node->dev_id, va);
            return ret;
        }
    }

#ifndef EMU_ST
    /* try to merge normal page to hpage */
    if ((*page_size == KA_MM_PAGE_SIZE) && (cycle_num > 0) &&
        ((dst_va == 0) || ((dst_va & (devmm_svm->device_hpage_size - 1)) == 0))) {
        int i;
        for (i = 0; (i < cycle_num) && (queried_num + i < total_num); i++) {
            if (devmm_is_hccs_vm_scene(node->dev_id, host_flag)) {
                pa = (u64)node->dma_info[queried_num + i].dma_addr;
            } else if (!node->src_pa_is_ram) {
                pa = node->pa_list[queried_num + i];
            } else {
                pa = (u64)ka_mm_page_to_phys(node->pages[queried_num + i]);
            }
            if ((i == 0) && ((pa & (devmm_svm->device_hpage_size - 1)) != 0)) {
                break;
            }
            if ((i != 0) && (pre_pa + KA_MM_PAGE_SIZE != pa)) {
                break;
            }
            pre_pa = pa;
        }

        *page_size = (i == cycle_num) ? devmm_svm->device_hpage_size : KA_MM_PAGE_SIZE;
    }
#endif
    return 0;
}

static int devmm_fill_pa_in_shm_node(u64 perpared_id, u64 queried_id, u32 page_size,
    struct devmm_shm_node *node, struct devmm_chan_remote_map *send_info)
{
    if (devmm_is_mem_map_by_pcie_th(node->map_type)) {
        int ret;
        ka_device_t *dev = devmm_device_get_by_devid(node->dev_id);
        if (dev == NULL) {
            devmm_drv_err("Device get by devid failed. (dev_id=%u)\n", node->dev_id);
            return -ENODEV;
        }
        if (!node->src_pa_is_ram) {
            send_info->src_pa[perpared_id] = ka_mm_dma_map_resource(dev, node->pa_list[queried_id],
                page_size, KA_DMA_BIDIRECTIONAL, 0);
        } else {
            send_info->src_pa[perpared_id] = devdrv_dma_map_page(dev, node->pages[queried_id], 0, page_size,
                KA_DMA_BIDIRECTIONAL);
        }
        ret = ka_mm_dma_mapping_error(dev, send_info->src_pa[perpared_id]);
        devmm_device_put_by_devid(node->dev_id);
        if (ret != 0) {
            devmm_drv_err("Dma_mapping_error. (ret=%d)\n", ret);
            return ret;
        }
        /* Store dma_info and release dma when unregister  */
        node->dma_info[node->dma_cnt].dma_addr = send_info->src_pa[perpared_id];
        node->dma_info[node->dma_cnt].dma_size = page_size;
        devmm_drv_debug("Dma addr. (dst_va=0x%llx, devid=%d; num=%u; page_size=%d)\n",
            send_info->dst_va, node->dev_id, node->dma_cnt, page_size);

        node->dma_cnt++;
    } else {
        send_info->src_pa[perpared_id] = (u64)ka_mm_page_to_phys(node->pages[queried_id]);
    }

    return 0;
}

#ifndef EMU_ST
static int devmm_hccs_vm_fill_pa_in_shm_node(struct devmm_shm_node *node)
{
    ka_device_t *dev = NULL;
    ka_dma_addr_t dma_addr;
    int ret;
    u64 i;

    dev = devmm_device_get_by_devid(node->dev_id);
    if (dev == NULL) {
        devmm_drv_err("Device get by devid failed. (dev_id=%u)\n", node->dev_id);
        return -ENODEV;
    }

    for (i = 0; i < node->size / KA_MM_PAGE_SIZE; ++i) {
        dma_addr = node->src_pa_is_ram ? 
            devdrv_dma_map_page(dev, node->pages[i], 0, KA_MM_PAGE_SIZE, KA_DMA_BIDIRECTIONAL) :
            ka_mm_dma_map_resource(dev, node->pa_list[i], KA_MM_PAGE_SIZE, KA_DMA_BIDIRECTIONAL, 0);

        ret = ka_mm_dma_mapping_error(dev, dma_addr);
        if (ret != 0) {
            devmm_device_put_by_devid(node->dev_id);
            devmm_drv_err("Dma_mapping_error. (ret=%d)\n", ret);
            return ret;
        }

        node->dma_info[node->dma_cnt].dma_addr = dma_addr;
        node->dma_info[node->dma_cnt].dma_size = KA_MM_PAGE_SIZE;
        node->dma_cnt++;
    }
    devmm_device_put_by_devid(node->dev_id);
    return 0;
}
#endif

static int devmm_shm_send_info_to_dev(struct devmm_svm_process *svm_proc,
    struct devmm_chan_remote_map *send_info, struct devmm_shm_node *node)
{
    /* pre_page_size means the page_size of send_info->host_pa[] */
    u64 host_va, send_max_len, perpared_num, queried_num, queried_size, sent_size;
    u64 remained_num = node->page_num;
    u64 start_va = node->src_va;
    u64 total_size = node->size;
    u64 pagenum_pre_dev_hpage = devmm_svm->device_hpage_size / KA_MM_PAGE_SIZE;
    u32 pre_page_size = 0;
    u32 page_size = KA_MM_PAGE_SIZE;
    u32 host_flag;
    int ret;

    ret = devmm_get_host_phy_mach_flag(node->dev_id, &host_flag);
    if (ret != 0) {
        return ret;
    }

#ifndef EMU_ST
    if (devmm_is_hccs_vm_scene(node->dev_id, host_flag)) {
        ret = devmm_hccs_vm_fill_pa_in_shm_node(node);
        if (ret != 0) {
            return ret;
        }
    }
#endif

    /* The address may be mixed(normal_page and huge_page), the address sent each time cannot be mixed */
    for (perpared_num = 0, queried_num = 0, queried_size = 0, sent_size = 0; sent_size < total_size;) {
        host_va = start_va + queried_size;
        ret = devmm_shm_try_query_hpagesize(svm_proc, node, host_va, node->dst_va + sent_size, &page_size);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Get page failed. (devid=%u; host_va=0x%llx)\n", node->dev_id, host_va);
            return ret;
#endif
        }

        if ((remained_num < pagenum_pre_dev_hpage) || (pagenum_pre_dev_hpage == 0)) {
            page_size = KA_MM_PAGE_SIZE;
        }

        if (page_size == pre_page_size || pre_page_size == 0) {
            if (devmm_is_hccs_vm_scene(node->dev_id, host_flag)) {
                send_info->src_pa[perpared_num] = (u64)node->dma_info[queried_num].dma_addr;
            } else {
                ret = devmm_fill_pa_in_shm_node(perpared_num, queried_num, page_size, node, send_info);
                if (ret != 0) {
#ifndef EMU_ST
                    devmm_drv_err("Get pa failed. (perpared_num=%llu; queried_num=%llu; page_size=%u)\n",
                        perpared_num, queried_num, page_size);
                    return ret;
#endif
                }
            }
            queried_num += page_size / KA_MM_PAGE_SIZE;
            remained_num -= page_size / KA_MM_PAGE_SIZE;
            queried_size += page_size;
            pre_page_size = page_size;
            perpared_num++;
        }
        ret = ((page_size != pre_page_size) ||
               (perpared_num == DEVMM_ACCESS_H2D_PAGE_NUM) || (queried_size == total_size));
        if (ret != 0) {
            send_info->page_size = pre_page_size;
            send_info->head.dev_id = (u16)node->dev_id;
            send_info->head.process_id.vfid = (u16)node->vfid;
            send_info->head.extend_num = (u16)perpared_num;
            send_info->dst_va = node->dst_va + sent_size;
            send_max_len = (u64)sizeof(struct devmm_chan_remote_map) +
                           (u64)sizeof(unsigned long long) * (u64)(send_info->head.extend_num);
            ret = devmm_chan_msg_send(send_info, (u32)send_max_len, (u32)sizeof(struct devmm_chan_remote_map));
            if (ret != 0) {
                devmm_drv_err("Send message failed. (devid=%d; send_va=0x%llx; ret=%d)\n",
                    node->dev_id, send_info->dst_va, ret);
                return ret;
            }

            /* host will recive dst_va at the first msg send */
            node->dst_va = (node->dst_va == 0) ? send_info->dst_va : node->dst_va;
            sent_size += perpared_num * pre_page_size;
            pre_page_size = 0;
            perpared_num = 0;
            devmm_drv_debug("Send_info success. (devid=%d; host_va=0x%llx; send_va=0x%llx; dst_va=0x%llx; payload=%u; "
                "map_type=%u; page_size=%u; sent_size=%llu)\n", node->dev_id, host_va, send_info->dst_va, node->dst_va,
                send_info->head.extend_num, node->map_type, send_info->page_size, sent_size);
        }
    }
    return 0;
}

int devmm_shm_config_txatu(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_shm_pro_node *shm_pro_node = devmm_get_shm_pro_node(svm_proc, HOST_MEM_MAP_DEV);
    u32 i;

    /* just jugde the first txatu config to make sure config or not */
    if ((shm_pro_node->txatu_info[0].txatu_flag & (1UL << devid)) == 0) {
        if (g_mem_info.succ_flag == 0) {
            return -EINVAL;
        }

        for (i = 0; i <= g_mem_info.config_index; i++) {
            int ret;
            ret = devdrv_device_txatu_config(svm_proc->process_id.hostpid, devid, g_mem_info.mem_addr[i],
                                             g_mem_info.memsz[i]);
            if (ret != 0) {
#ifndef EMU_ST
                devmm_drv_err_if((ret != -EOPNOTSUPP), "Config failed. (start_addr=0x%llx; memsize=%llu; devid=%d; ret=%d)\n",
                    g_mem_info.mem_addr[i], g_mem_info.memsz[i], devid, ret);
#endif
                return ret;
            }
            shm_pro_node->txatu_info[i].addr = g_mem_info.mem_addr[i];
            shm_pro_node->txatu_info[i].txatu_flag |= (1UL << devid);

            devmm_drv_info("Config txatu success. (devid=%u; addr[%u]=0x%llx; memsz[%u]=%llu; txatu_flag=0x%llx)\n",
                devid, i, g_mem_info.mem_addr[i], i,
                g_mem_info.memsz[i], shm_pro_node->txatu_info[i].txatu_flag);
        }
    }

    return 0;
}

static bool devmm_judge_shm_node_overlap(struct devmm_shm_head *shm_head, struct devmm_mem_remote_map_para *map_para,
    struct devmm_devid *devids)
{
    struct devmm_shm_node *node = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    head = &shm_head->list;

    ka_list_for_each_safe(pos, n, head) {
        node = ka_list_entry(pos, struct devmm_shm_node, list);
        if ((node->dev_id == devids->devid) && (node->vfid == devids->vfid) &&
            (map_para->src_va < node->src_va + node->size) &&
            (node->src_va < map_para->src_va + map_para->size)) {
            devmm_drv_debug("Get shm success. (node_src_va=0x%llx; device_va=0x%llx; size=%llu)\n",
                node->src_va, node->dst_va, node->size);
            return true;
        }
    }

    return false;
}

ka_semaphore_t *devmm_get_shm_sem(struct devmm_svm_process *svm_proc, u32 map_type)
{
    return &svm_proc->register_sem[map_type];
}

struct devmm_shm_pro_node *devmm_get_shm_pro_node(struct devmm_svm_process *svm_proc, u32 map_type)
{
    return svm_proc->shm_pro_node[map_type];
}

struct devmm_shm_process_head *devmm_get_shm_pro_head(u32 map_type)
{
    return &devmm_svm->shm_pro_head[map_type];
}

static void devmm_remote_unmap_and_delete_node_list(struct devmm_svm_process *svm_proc, ka_list_head_t *head)
{
    struct devmm_shm_node *node = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp;

    stamp = (u32)ka_jiffies;
    ka_list_for_each_safe(pos, n, head) {
        node = ka_list_entry(pos, struct devmm_shm_node, list);
        if (node->ref == 0) {
            devmm_remote_unmap_and_delete_node(svm_proc, node);
            node = NULL;
        }
        devmm_try_cond_resched(&stamp);
    }
}

static void devmm_shm_pro_node_add_to_head(struct devmm_shm_pro_node *shm_pro_node, u32 map_type)
{
    struct devmm_shm_process_head *shm_process_head = NULL;

    shm_process_head = devmm_get_shm_pro_head(map_type);
    ka_task_mutex_lock(&shm_process_head->node_lock);
    ka_list_add(&shm_pro_node->list, &shm_process_head->head);
    ka_task_mutex_unlock(&shm_process_head->node_lock);
}

void devmm_destory_shm_pro_node(struct devmm_svm_process *svm_proc)
{
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    for (i = 0; i < HOST_REGISTER_MAX_TPYE; i++) {
        ka_semaphore_t *shm_sem = devmm_get_shm_sem(svm_proc, i);
        struct devmm_shm_pro_node *shm_pro_node = NULL;
        ka_list_head_t *head = NULL;

        ka_task_down(shm_sem);
        shm_pro_node = devmm_get_shm_pro_node(svm_proc, i);
        if (shm_pro_node == NULL) {
            ka_task_up(shm_sem);
            continue;
        }

        head = &shm_pro_node->shm_head.list;
        devmm_remote_unmap_and_delete_node_list(svm_proc, head);

        if (ka_list_empty_careful(head) == 0) {
            devmm_shm_pro_node_add_to_head(shm_pro_node, i);
        } else {
            devmm_del_txatu_by_proc(&shm_pro_node->txatu_info[0], DEVMM_MAX_TXATU,
                (u32)svm_proc->process_id.hostpid);
            devmm_kfree_ex(svm_proc->shm_pro_node[i]);
        }
        svm_proc->shm_pro_node[i] = NULL;
        ka_task_up(shm_sem);
        devmm_try_cond_resched(&stamp);
    }

    return;
}

static void devmm_uninit_shm_node_by_devid(struct devmm_shm_pro_node *shm_pro_node, u32 dev_id)
{
    struct devmm_shm_node *node = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp = (u32)ka_jiffies;

    head = &shm_pro_node->shm_head.list;
    ka_list_for_each_safe(pos, n, head) {
        node = ka_list_entry(pos, struct devmm_shm_node, list);
        if (node->dev_id == dev_id) {
            devmm_remote_unmap_and_delete_node(NULL, node);
        }
        devmm_try_cond_resched(&stamp);
    }

    devmm_del_txatu_by_dev_id(&shm_pro_node->txatu_info[0], DEVMM_MAX_TXATU, shm_pro_node->hostpid, dev_id);

    return;
}

static void devmm_uninit_dev_svm_shm(u32 dev_id, u32 map_type)
{
    struct devmm_shm_process_head *shm_process_head = NULL;
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    ka_list_head_t *head = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp = (u32)ka_jiffies;

    shm_process_head = devmm_get_shm_pro_head(map_type);

    ka_task_mutex_lock(&shm_process_head->node_lock);
    head = &shm_process_head->head;
    ka_list_for_each_safe(pos, n, head) {
        shm_pro_node = ka_list_entry(pos, struct devmm_shm_pro_node, list);
        devmm_uninit_shm_node_by_devid(shm_pro_node, dev_id);
        devmm_delete_shm_pro_node(shm_pro_node);
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&shm_process_head->node_lock);

    return;
}

void devmm_uninit_shm_by_devid(u32 dev_id)
{
    u32 i;

    for (i = 0; i < HOST_REGISTER_MAX_TPYE; i++) {
        devmm_uninit_dev_svm_shm(dev_id, i);
    }
}

static void devmm_clear_remote_map_status(u32 *bitmap, u64 page_bitmap_num, bool is_locked_host)
{
    u32 stamp = (u32)ka_jiffies;
    u32 *bitmap_ = bitmap;
    u64 i;

    devmm_page_bitmap_clear_flag(bitmap_, DEVMM_PAGE_REMOTE_MAPPED_FIRST_MASK);
    for (i = 0; i < page_bitmap_num; i++, bitmap_++) {
        devmm_page_bitmap_clear_flag(bitmap_, DEVMM_PAGE_REMOTE_MAPPED_MASK);
        /* master svm shm map should clear target logical_devid */
        if (is_locked_host) {
            devmm_page_bitmap_set_devid(bitmap_, DEVMM_MAX_DEVICE_NUM + 1);
        }
        devmm_try_cond_resched(&stamp);
    }
}

static void devmm_clear_svm_remote_map_status(struct devmm_svm_process *svm_proc,
    u64 va, u64 page_bitmap_num, bool is_locked_host)
{
    struct devmm_svm_heap *heap = NULL;
    u32 *bitmap = NULL;

    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Get heap error. (va=0x%llx)\n", va);
        return;
    }
    bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (bitmap == NULL) {
        devmm_drv_err("Get bitmap error. (va=0x%llx)\n", va);
        return;
    }
    devmm_clear_remote_map_status(bitmap, page_bitmap_num, is_locked_host);
}

static int devmm_check_and_set_remote_map_state(
    struct devmm_svm_process *svm_proc,
    struct devmm_memory_attributes *attr,
    struct devmm_mem_remote_map_para *map_para,
    struct devmm_devid *devids,
    u64 page_bitmap_num)
{
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u32 *bitmap = NULL;
    bool is_locked_host = ((attr != NULL) && (attr->is_locked_host));
    u64 i, va;

    if (devmm_svm_mem_is_enable(svm_proc) == false) {
        devmm_drv_err("Host mmap failed, can't insert host page.\n");
        return -EINVAL;
    }

    va = devmm_is_local_dev_mem_type(map_para->map_type) ? map_para->dst_va : map_para->src_va;
    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        devmm_drv_err("Get heap error. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    if ((heap->heap_sub_type == SUB_RESERVE_TYPE) && is_locked_host) {
#ifndef EMU_ST
        devmm_drv_run_info("No support remote map vmm host addr.\n");
        return -EOPNOTSUPP;
#endif
    }

    if (devmm_check_va_add_size_by_heap(heap, va, map_para->size) != 0) {
        devmm_drv_err("Size is error. (va=0x%llx, size=%llu)\n", va, map_para->size);
        return -EINVAL;
    }

    if ((va & (u64)(heap->chunk_page_size - 1)) != 0) {
        devmm_drv_err("Va should be aligned by chunk_page_size. (src_va=0x%llx; chunk_page_size=%u)\n",
            va, heap->chunk_page_size);
        return -EINVAL;
    }

    bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (bitmap == NULL) {
        devmm_drv_err("Get bitmap error. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    /* set first mask before mapped mask to avoid other tasks get current mapped page */
    if (devmm_page_bitmap_check_and_set_flag(bitmap, DEVMM_PAGE_REMOTE_MAPPED_FIRST_MASK) != 0) {
        devmm_drv_err("Va is already mapped by remote. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    for (i = 0; i < page_bitmap_num; i++) {
        if (devmm_page_bitmap_is_advise_readonly(bitmap + i)) {
            devmm_drv_err("Readonly mem, not allowed remote map. (va=0x%llx; bitmap=0x%x; page_idx=%llu)\n",
                va, *bitmap, i);
            goto err_exit;
        }
        if (devmm_page_bitmap_check_and_set_flag(bitmap + i, DEVMM_PAGE_REMOTE_MAPPED_MASK) != 0) {
            devmm_drv_err("Va is already mapped by remote. (va=0x%llx; bitmap=0x%x; page_idx=%llu)\n",
                va, *bitmap, i);
            goto err_exit;
        }
        /* master svm shm map should set target logical_devid */
        if (is_locked_host == true) {
            devmm_page_bitmap_set_devid(bitmap + i, devids->logical_devid);
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;

err_exit:
    devmm_clear_remote_map_status(bitmap, i, is_locked_host);
    return -EINVAL;
}

static struct devmm_shm_node *devmm_alloc_shm_node(u64 page_num, u32 map_type)
{
    struct devmm_shm_node *node = NULL;

    node = devmm_kzalloc_ex(sizeof(struct devmm_shm_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        devmm_drv_err("Kzalloc shm_node fail, out of memory. (alloc_size=%lu)\n", sizeof(struct devmm_shm_node));
        return NULL;
    }

    if (map_type == HOST_IO_MAP_DEV) {
        node->pa_list = (u64 *)__devmm_vmalloc_ex(sizeof(u64) * page_num,
            KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT, PAGE_KERNEL);
        if (node->pa_list == NULL) {
            devmm_drv_err("Vzalloc pa_list failed, out of memory. (alloc_size=%llu)\n", sizeof(u64) * page_num);
            devmm_kfree_ex(node);
            return NULL;
        }
    } else if (map_type != DEV_MEM_MAP_HOST) {
        node->pages = (ka_page_t **)__devmm_vmalloc_ex(sizeof(ka_page_t *) * page_num,
            KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
        if (node->pages == NULL) {
            devmm_drv_err("Vzalloc failed, out of memory. (alloc_size=%llu)\n", sizeof(ka_page_t *) * page_num);
            devmm_kfree_ex(node);
            return NULL;
        }
    }

    if (devmm_is_mem_map_by_pcie_th(map_type)) {
        node->dma_info = (struct devmm_dma_info *)__devmm_vmalloc_ex(sizeof(struct devmm_dma_info) * page_num,
            KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
        if (node->dma_info == NULL) {
            devmm_drv_err("Vzalloc dma_info failed, out of memory. (alloc_size=%llu)\n",
                sizeof(struct devmm_dma_info) * page_num);
            if (node->pages != NULL) {
                devmm_vfree_ex(node->pages);
            }
            if (node->pa_list != NULL) {
                devmm_vfree_ex(node->pa_list);
            }
            devmm_kfree_ex(node);
            return NULL;
        }
    }

    return node;
}

static int devmm_init_shm_node(struct devmm_shm_head *shm_head, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_shm_node *shm_node)
{
    shm_node->src_va = map_para->src_va;
    shm_node->size = map_para->size;
    shm_node->page_num = map_para->size >> KA_MM_PAGE_SHIFT;
    shm_node->dev_id = devids->devid;
    shm_node->logical_devid = devids->logical_devid;
    shm_node->vfid = devids->vfid;
    shm_node->dst_va = map_para->dst_va;
    shm_node->ref = 0;
    shm_node->map_type = map_para->map_type;
    shm_node->proc_type = map_para->proc_type;
    shm_node->src_va_is_pfn_map = false;
    shm_node->src_pa_is_ram = true;
    KA_INIT_LIST_HEAD(&shm_node->list);
    ka_list_add(&shm_node->list, &shm_head->list);

    return 0;
}

static struct devmm_shm_node *devmm_create_shm_node(struct devmm_shm_pro_node *shm_pro_node,
    struct devmm_devid *devids, struct devmm_mem_remote_map_para *map_para)
{
    struct devmm_shm_head *shm_head = &shm_pro_node->shm_head;
    struct devmm_shm_node *node = NULL;
    u64 page_num;
    int ret;

    page_num = map_para->size >> KA_MM_PAGE_SHIFT;
    node = devmm_alloc_shm_node(page_num, map_para->map_type);
    if (node == NULL) {
        devmm_drv_err("Create shm_node fail. (page_num=%llu)\n", page_num);
        return NULL;
    }

    ret = devmm_init_shm_node(shm_head, devids, map_para, node);
    if (ret != 0) {
        devmm_destory_shm_node(node);
        return NULL;
    }

    return node;
}

void devmm_destory_shm_node(struct devmm_shm_node *node)
{
    ka_list_del(&node->list);
    if (node->dma_info != NULL) {
        devmm_vfree_ex(node->dma_info);
    }
    if (node->pages != NULL) {
        devmm_vfree_ex(node->pages);
    }
    if (node->pa_list != NULL) {
        devmm_vfree_ex(node->pa_list);
    }
    devmm_kfree_ex(node);
}

int devmm_init_shm_pro_node(struct devmm_svm_process *svm_proc)
{
    u32 j, i;

    for (j = 0; j < HOST_REGISTER_MAX_TPYE; j++) {
        svm_proc->shm_pro_node[j] = devmm_kzalloc_ex(sizeof(struct devmm_shm_pro_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (svm_proc->shm_pro_node[j] == NULL) {
#ifndef EMU_ST
            goto alloc_fail;
#endif
        }

        svm_proc->shm_pro_node[j]->hostpid = (u32)svm_proc->process_id.hostpid;
        KA_INIT_LIST_HEAD(&svm_proc->shm_pro_node[j]->list);
        KA_INIT_LIST_HEAD(&svm_proc->shm_pro_node[j]->shm_head.list);
        for (i = 0; i < DEVMM_MAX_TXATU; i++) {
            svm_proc->shm_pro_node[j]->txatu_info[i].addr = 0;
            svm_proc->shm_pro_node[j]->txatu_info[i].txatu_flag = DEVMM_TXATU_DISABLE;
        }
    }
    return 0;

#ifndef EMU_ST
alloc_fail:
    for (i = 0; i < j; i++) {
        devmm_kfree_ex(svm_proc->shm_pro_node[i]);
        svm_proc->shm_pro_node[i] = NULL;
    }
    return -ENOMEM;
#endif
}

void devmm_unint_shm_pro_node(struct devmm_svm_process *svm_proc)
{
    u32 i;

    for (i = 0; i < HOST_REGISTER_MAX_TPYE; i++) {
        devmm_kfree_ex(svm_proc->shm_pro_node[i]);
        svm_proc->shm_pro_node[i] = NULL;
    }
}

/* Temporarily change the name, unify later. */
static void devmm_dma_unmap_pages_by_node(struct devmm_shm_node *node)
{
    struct devmm_dma_info *dma_info = node->dma_info;
    u64 i, dma_num = node->dma_cnt;
    ka_device_t *dev = NULL;
    u32 stamp = (u32)ka_jiffies;

    if (dma_info == NULL) {
        return;
    }
    dev = devmm_device_get_by_devid(node->dev_id);
    if (dev != NULL) {
        for (i = 0; i < dma_num; i++) {
            if (ka_mm_dma_mapping_error(dev, dma_info[i].dma_addr) == 0) {
                if (!node->src_pa_is_ram){
                    ka_mm_dma_unmap_resource(dev, dma_info[i].dma_addr, dma_info[i].dma_size, KA_DMA_BIDIRECTIONAL, 0);
                } else {
                    devdrv_dma_unmap_page(dev, dma_info[i].dma_addr, dma_info[i].dma_size, KA_DMA_BIDIRECTIONAL);
                }
            }
            devmm_try_cond_resched(&stamp);
        }
        devmm_device_put_by_devid(node->dev_id);
    }
}

void devmm_remote_unmap_and_delete_node(struct devmm_svm_process *svm_proc, struct devmm_shm_node *node)
{
    if ((svm_proc != NULL) && (node->map_type == DEV_MEM_MAP_HOST)) {
        devmm_unmap_mem(svm_proc, node->dst_va, node->size);
        devmm_clear_svm_remote_map_status(svm_proc, node->dst_va, node->page_num, false);
    }
    devmm_dma_unmap_pages_by_node(node);
    if (node->src_va_is_pfn_map) {
        if (node->src_pa_is_ram) {
            devmm_put_user_pages(node->pages, node->page_num, node->page_num);
        }
    } else {
        devmm_unpin_user_pages(node->pages, node->page_num, node->page_num);
    }
    devmm_destory_shm_node(node);
}

static int devmm_agent_va_to_master_pa(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, u64 agent_va, u64 page_size, u64 *master_pa)
{
    u64 agent_pa;
    int ret;

    ret = devmm_find_pa_cache(svm_proc, devids->logical_devid, agent_va, (u32)page_size, &agent_pa);
    if (ret != 0) {
        devmm_drv_err("Find agent_pa fail. (agent_va=0x%llx; page_size=%llu; logical_devid=%u; ret=%d)\n",
            agent_va, page_size, devids->logical_devid, ret);
        return -EINVAL;
    }

    if (!devmm_is_host_agent(devids->devid)) {
        ret = devdrv_devmem_addr_d2h(devids->devid, agent_pa, master_pa);
        if (ret != 0) {
            devmm_drv_err("Agent_pa to pcie_pa fail. (agent_va=0x%llx; map_size=%llu; devid=%u; ret=%d)\n",
                agent_va, page_size, devids->devid, ret);
            return -EINVAL;
        }
    } else {
        *master_pa = agent_pa;
    }

    return 0;
}

static inline bool devmm_is_read_only_mem(u32 flag)
{
    return flag & MEM_REGISTER_READ_ONLY;
}

static int devmm_map_agent_svm_mem_to_master(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 page_num, master_pa, tmp_va;
    u32 stamp = (u32)ka_jiffies;
    ka_pgprot_t page_prot;
    u64 i, j;
    int ret;

    vma = devmm_find_vma(svm_proc, map_para->src_va);
    if (vma == NULL) {
        devmm_drv_err("Can't find vma. (svm_addr=0x%llx)\n", map_para->src_va);
        return -EINVAL;
    }

    page_num = map_para->size / attr->page_size;
    map_para->dst_va = map_para->src_va;
    page_prot = devmm_make_remote_pgprot(0);
    if (devmm_is_read_only_mem(map_para->access)) {
        page_prot = devmm_make_readonly_pgprot(page_prot);
    }

    for (i = 0, tmp_va = map_para->src_va; i < page_num; i++, tmp_va += attr->page_size) {
        ret = devmm_agent_va_to_master_pa(svm_proc, devids, tmp_va, attr->page_size, &master_pa);
        if (ret != 0) {
            devmm_drv_err("Find agent_pa fail. (i=%llu; svm_ptr=0x%llx; map_size=%llu; logical_devid=%u)\n",
                i, map_para->src_va, map_para->size, devids->logical_devid);
            goto clear_pfn_range;
        }

        ret = ka_mm_remap_pfn_range(vma, tmp_va, KA_MM_PFN_DOWN(master_pa), attr->page_size, page_prot);
        if (ret != 0) {
            devmm_drv_err("Remap_pfn_range fail. (i=%llu; va=0x%llx; page_size=%u; ret=%d)\n",
                i, tmp_va, attr->page_size, ret);
            goto clear_pfn_range;
        }
        devmm_try_cond_resched(&stamp);
    }

    return 0;
clear_pfn_range:
    for (j = 0, tmp_va = map_para->src_va; j < i; j++, tmp_va += attr->page_size) {
        devmm_zap_vma_ptes(vma, tmp_va, attr->page_size);
        devmm_try_cond_resched(&stamp);
    }
    return ret;
}

int devmm_unmap_mem(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 tmp_va, tmp_pa, page_num, i;
    u32 stamp = (u32)ka_jiffies;

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Can't find vma. (svm_addr=0x%llx)\n", va);
        return -EINVAL;
    }

    page_num = size >> KA_MM_PAGE_SHIFT;
    for (i = 0, tmp_va = va; i < page_num; i++, tmp_va += KA_MM_PAGE_SIZE) {
        int ret;

        ret = devmm_va_to_pa(vma, tmp_va, &tmp_pa);
        if (ret != 0) {
            continue;
        }
        devmm_zap_vma_ptes(vma, tmp_va, KA_MM_PAGE_SIZE);
        devmm_try_cond_resched(&stamp);
    }

    return  0;
}

static int devmm_check_and_unmap_mem(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 tmp_va, tmp_pa, page_num, i;
    u32 stamp = (u32)ka_jiffies;

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Can't find vma. (svm_addr=0x%llx)\n", va);
        return -EINVAL;
    }

    page_num = size >> KA_MM_PAGE_SHIFT;
    for (i = 0, tmp_va = va; i < page_num; i++, tmp_va += KA_MM_PAGE_SIZE) {
        int ret = devmm_va_to_pa(vma, tmp_va, &tmp_pa);
        if (ret != 0) {
            return -EINVAL;
        }

        devmm_try_cond_resched(&stamp);
    }

#ifndef EMU_ST
    devmm_zap_vma_ptes(vma, va, KA_MM_PAGE_SIZE * page_num);
#else
    for (i = 0, tmp_va = va; i < page_num; i++, tmp_va += KA_MM_PAGE_SIZE) {
        devmm_zap_vma_ptes(vma, tmp_va, KA_MM_PAGE_SIZE);
    }
#endif
    return 0;
}

STATIC struct devmm_chan_remote_map *devmm_alloc_init_send_info(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, struct devmm_mem_remote_map_para *map_para, u32 page_size)
{
    struct devmm_chan_remote_map *send_info = NULL;
    u64 send_max_len;

    send_max_len = sizeof(struct devmm_chan_remote_map) + sizeof(unsigned long long) * DEVMM_ACCESS_H2D_PAGE_NUM;
    /* send_info should contiguous memory */
    send_info = devmm_kzalloc_ex(send_max_len, KA_GFP_KERNEL);
    if (send_info == NULL) {
        devmm_drv_err("Palist devmm_vzalloc_ex failed. (malloc_size=%llu)\n", send_max_len);
        return NULL;
    }

    send_info->head.msg_id = DEVMM_CHAN_REMOTE_MAP_ID;
    send_info->head.process_id.hostpid = svm_proc->process_id.hostpid;
    send_info->head.dev_id = (u16)devids->devid;
    send_info->src_va = map_para->src_va;
    send_info->access = map_para->access;
    send_info->size = map_para->size;
    send_info->page_size = page_size;
    send_info->dst_va = map_para->dst_va;
    send_info->map_type = map_para->map_type;
    send_info->proc_type = map_para->proc_type;
    return send_info;
}

STATIC void devmm_free_send_info(struct devmm_chan_remote_map *send_info)
{
    devmm_kfree_ex(send_info);
}

STATIC int devmm_agent_va_to_master_palist(struct devmm_svm_process *svm_proc,
    struct devmm_memory_attributes *attr, u64 size, u64 *pa, u64 *num)
{
    struct devmm_devid devids = {attr->logical_devid, attr->devid, attr->vfid};
    u32 page_size = attr->page_size;
    u64 agent_va = attr->va;
    u64 vaddr, paddr;
    u64 end_addr;
    u64 pg_num = 0;
    int ret = 0;

    end_addr = ka_base_round_up(agent_va + size, page_size);
    for (vaddr = ka_base_round_down(agent_va, page_size); vaddr < end_addr; vaddr += page_size) {
        if (devmm_agent_va_to_master_pa(svm_proc, &devids, vaddr, page_size, &paddr) != 0) {
            /* too much log, not print */
            ret = -ENOENT;
            break;
        }
        if (pg_num >= *num) {
            /* va size more then array num */
            break;
        }
        pa[pg_num++] = paddr;
    }
    *num = pg_num;
    return ret;
}

STATIC int devmm_merge_palist_to_hpalist(u64 *pa, u32 pa_num, u32 *hpa_num, u32 adap_num, u32 page_size)
{
    u32 blk = pa_num / adap_num;
    u32 i;

    if ((pa_num % adap_num) != 0) {
        return -EINVAL;
    }

    for (i = 0; i < blk; i++) {
        if (devmm_palist_is_continuous(&pa[i * adap_num], adap_num, page_size) == false) {
            return -EINVAL;
        }
    }

    /* merge to hpalist */
    for (i = 0; i < blk; i++) {
        pa[i] = pa[i * adap_num];
    }
    *hpa_num = blk;

    return 0;
}

static int devmm_try_get_hpalist(const ka_vm_area_struct_t *vma, u64 va, u64 sz, u32 devid, struct devmm_palist_info *palist)
{
    u64 dev_hpage_size = devmm_svm->device_hpage_size;
    u32 blk_num = palist->blk_num;
    u32 host_flag;
    int ret;

    ret = devmm_get_host_phy_mach_flag(devid, &host_flag);
    if (ret != 0) {
        return ret;
    }
    palist->got_num = 0;
    palist->page_size = KA_MM_PAGE_SIZE;

    ret = devmm_va_to_palist(vma, va, sz, palist->pa_blk, &blk_num);
    if (ret != 0) {
        devmm_drv_err("Va to palist fail. (ret=%d)\n", ret);
        return ret;
    }

#ifndef EMU_ST
    if (devmm_is_hccs_vm_scene(devid, host_flag)) {
        ret = devmm_vm_pa_to_pm_pa(devid, palist->pa_blk, palist->blk_num, palist->pa_blk, palist->blk_num);
        if (ret != 0) {
            return ret;
        }
    }
#endif

    palist->got_num = blk_num;
    if ((palist->pa_blk[0] & (dev_hpage_size - 1)) != 0 || (dev_hpage_size % KA_MM_PAGE_SIZE) != 0) {
        return 0;
    }

#ifndef EMU_ST
    ret = devmm_merge_palist_to_hpalist(palist->pa_blk, blk_num, &palist->got_num,
        dev_hpage_size / KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE);
    if (ret == 0) {
        palist->page_size = dev_hpage_size;
    } else {
        palist->got_num = blk_num;
    }
#endif
    return 0;
}

static int devmm_send_map_info_to_agent(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_chan_remote_map *send_info, struct devmm_mem_remote_map_para *map_para,
    struct devmm_memory_attributes *attr)
{
    struct devmm_palist_info palist = {.pa_blk = send_info->src_pa, .blk_num = DEVMM_ACCESS_H2D_PAGE_NUM};
    ka_vm_area_struct_t *vma = NULL;
    u64 tmp_va, remaining_size, send_max_len, num;
    u32 page_size = send_info->page_size;

    vma = devmm_find_vma(svm_proc, map_para->src_va);
    if (vma == NULL) {
        devmm_drv_err("Find vma error. (src_va=0x%llx)\n", map_para->src_va);
        return -EINVAL;
    }

    map_para->dst_va = map_para->src_va;
    send_info->dst_va = map_para->dst_va;
    for (tmp_va = map_para->src_va, remaining_size = map_para->size; remaining_size > 0;
        tmp_va += num * page_size, remaining_size -= num * page_size) {
        int ret;

        num = DEVMM_ACCESS_H2D_PAGE_NUM;
        /*
         * When attr is dev_agent, prefetch dev_agent ptr to host_agent;
         * when attr is others, prefetch host_agent ptr to dev_agent or remote_map master ptr to agent.
         */
        if (devmm_is_device_agent(attr)) {
            ret = devmm_agent_va_to_master_palist(svm_proc, attr, remaining_size, send_info->src_pa, &num);
        } else {
            ret = devmm_try_get_hpalist(vma, tmp_va, remaining_size, devids->devid, &palist);
            num = palist.got_num;
            page_size = palist.page_size;
        }
        if (ret != 0) {
            devmm_drv_err("Find pa fail. (tmp_va=0x%llx; ret=%d)\n", tmp_va, ret);
            return ret;
        }

        devmm_drv_debug("Send map info. (tmp_va=0x%llx; num=%llu; page_size=%u)\n", tmp_va, num, page_size);
        send_info->page_size = page_size;
        send_info->head.extend_num = (u16)num;
        send_max_len = (u64)sizeof(struct devmm_chan_remote_map) + (u64)sizeof(u64) * (u64)(send_info->head.extend_num);
        ret = devmm_chan_msg_send(send_info, (u32)send_max_len, (u32)sizeof(struct devmm_chan_remote_map));
        if (ret != 0) {
            devmm_drv_err("Send message failed. (tmp_va=0x%llx; num=%llu; ret=%d)\n", tmp_va, num, ret);
            return ret;
        }
        send_info->dst_va += num * page_size;
    }

    return 0;
}

STATIC int devmm_send_unmap_info_to_agent(struct devmm_chan_remote_unmap *send_info,
    struct devmm_devid *devids, u64 size, u64 dst_va)
{
    u64 pre_free_size = DEVMM_SHM_FREE_SIZE_PRE_MSG;
    u64 current_va, left_size;

    for (current_va = dst_va, left_size = size; left_size > 0;) {
        int ret;

        send_info->dst_va = current_va;
        send_info->size = ka_base_min(left_size, pre_free_size);
        current_va += send_info->size;
        left_size -= send_info->size;

        send_info->head.dev_id = (u16)devids->devid;
        send_info->head.process_id.vfid = (u16)devids->vfid;
        ret = devmm_chan_msg_send(send_info, sizeof(struct devmm_chan_remote_unmap), 0);
        if (ret != 0) {
            devmm_drv_warn("Send message failed. (src_va=0x%llx; devid=%u; ret=%d)\n",
                send_info->src_va, devids->devid, ret);
            return ret;
        }
    }

    return 0;
}

static int devmm_local_host_unmap(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, struct devmm_shm_node *node)
{
    struct devmm_chan_remote_unmap send_info = {{{0}}};

    send_info.src_va = node->src_va;
    send_info.head.msg_id = DEVMM_CHAN_REMOTE_UNMAP_ID;
    send_info.head.process_id.hostpid = svm_proc->process_id.hostpid;
    send_info.map_type = node->map_type;
    send_info.proc_type = node->proc_type;

    return devmm_send_unmap_info_to_agent(&send_info, devids, node->size, node->dst_va);
}

int devmm_locked_host_unmap(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, u64 src_va, u64 size, u64 dst_va)
{
    struct devmm_chan_remote_unmap send_info = {{{0}}};

    send_info.src_va = src_va;
    send_info.head.msg_id = DEVMM_CHAN_REMOTE_UNMAP_ID;
    send_info.head.process_id.hostpid = svm_proc->process_id.hostpid;
    send_info.map_type = HOST_SVM_MAP_DEV;
    send_info.proc_type = DEVDRV_PROCESS_CP1;

    return devmm_send_unmap_info_to_agent(&send_info, devids, size, dst_va);
}

int devmm_map_svm_mem_to_agent(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    struct devmm_chan_remote_map *send_info = NULL;
    int ret;

    send_info = devmm_alloc_init_send_info(svm_proc, devids, map_para, attr->page_size);
    if (send_info == NULL) {
        devmm_drv_err("Alloc send_info fail.\n");
        return -ENOMEM;
    }

    ret = devmm_send_map_info_to_agent(svm_proc, devids, send_info, map_para, attr);
    devmm_free_send_info(send_info);
    return ret;
}

STATIC int devmm_map_master_svm_mem_to_agent(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    return devmm_map_svm_mem_to_agent(svm_proc, devids, map_para, attr);
}

STATIC int devmm_user_va_to_pa_list(struct devmm_svm_process *svm_proc, u64 va, u64 total_num, u64 *pa_list)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 vaddr, paddr, pg_num;
    u32 stamp = (u32)ka_jiffies;

    ka_task_down_read(ka_mm_get_mmap_sem(svm_proc->mm));
    vma = ka_mm_find_vma(svm_proc->mm, va);
    if ((vma == NULL) || (vma->vm_start > va)) {
        ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
        devmm_drv_err("Can not find vma.(va=[0x%llx];hostpid=[%d]).\n", va, svm_proc->process_id.hostpid);
        return -EINVAL;
    }

    for (vaddr = va, pg_num = 0; pg_num < total_num; vaddr += KA_MM_PAGE_SIZE, pg_num++) {
        if (devmm_va_to_pa(vma, vaddr, &paddr) != 0) {
            ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
            return -ENOENT;
        }
        pa_list[pg_num] = paddr;
        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));

    return 0;
}

#ifndef EMU_ST
STATIC int devmm_user_va_to_pages_list(struct devmm_svm_process *svm_proc, u64 va, u64 total_num, ka_page_t **pages)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 vaddr, paddr, pg_num;
    u32 stamp = (u32)ka_jiffies;

    ka_task_down_read(ka_mm_get_mmap_sem(svm_proc->mm));
    vma = ka_mm_find_vma(svm_proc->mm, va);
    if ((vma == NULL) || (ka_mm_get_vm_start(vma) > va)) {
        ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
        devmm_drv_err("Can not find vma.(va=[0x%llx];hostpid=[%d]).\n", va, svm_proc->process_id.hostpid);
        return -EINVAL;
    }

    for (vaddr = va, pg_num = 0; pg_num < total_num; vaddr += KA_MM_PAGE_SIZE, pg_num++) {
        if (devmm_va_to_pa(vma, vaddr, &paddr) != 0) {
            ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
            return -ENOENT;
        }
        pages[pg_num] = devmm_pa_to_page(paddr);
        devmm_try_cond_resched(&stamp);
    }
    devmm_pin_user_pages(pages, pg_num);
    ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));

    return 0;
}

static bool devmm_vma_is_pfn_map(ka_vm_area_struct_t *vma)
{
    return ((ka_mm_get_vm_flags(vma) & KA_VM_PFNMAP) != 0);
}
#endif

static bool devmm_pfn_is_io_mem(struct devmm_svm_process *svm_proc, u64 va, u64 total_num, bool *is_ram_mem)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 vaddr,aligned_va, pg_num, pfn, kpg_size;
    u32 stamp = (u32)ka_jiffies;
 
    ka_task_down_read(ka_mm_get_mmap_sem(svm_proc->mm));
    vma = ka_mm_find_vma(svm_proc->mm, va);
    if ((vma == NULL) || (vma->vm_start > va)) {
        ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
        devmm_drv_err("Can not find vma.(va=[0x%llx];hostpid=[%d]).\n", va, svm_proc->process_id.hostpid);
        return -EINVAL;
    }
 
    for (vaddr = va, pg_num = 0; pg_num < total_num; vaddr += PAGE_SIZE, pg_num++) {
        aligned_va = ka_base_round_down(va, PAGE_SIZE);
        if (devmm_va_to_pfn(vma, aligned_va, &pfn, &kpg_size) != 0) {
            ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
            return -ENOENT;
        }
        if (page_is_ram(pfn)) {
            ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
#ifndef EMU_ST
            return 0;
#endif
        }
        devmm_try_cond_resched(&stamp);
    }

    ka_task_up_read(ka_mm_get_mmap_sem(svm_proc->mm));
    *is_ram_mem = false;
 
    return 0;
}

static int devmm_local_va_to_pages_list(struct devmm_svm_process *svm_proc, struct devmm_shm_node *node)
{
    ka_vm_area_struct_t *vma = NULL;
    bool is_pfn_map = false;
    int write = 1;  /* write is 1 */
    int ret;

    ka_task_down_read(get_mmap_sem(svm_proc->mm));
    vma = ka_mm_find_vma(svm_proc->mm, node->src_va);
    if ((vma == NULL) || (ka_mm_get_vm_start(vma) > node->src_va)) {
#ifndef EMU_ST
        ka_task_up_read(get_mmap_sem(svm_proc->mm));
        devmm_drv_err("Can not find vma.(va=[0x%llx];hostpid=[%d]).\n", node->src_va, svm_proc->process_id.hostpid);
        return -EFAULT;
#endif
    }
    is_pfn_map = devmm_vma_is_pfn_map(vma);
    ka_task_up_read(get_mmap_sem(svm_proc->mm));

    /* memory remap by remap_pfn_rang, get user page fast can not get page addr */
    if (is_pfn_map) {
        struct devmm_svm_heap *heap = devmm_svm_get_heap(svm_proc, node->src_va);
#ifndef EMU_ST
        if ((heap != NULL) && (heap->heap_sub_type == SUB_RESERVE_TYPE)) {
            devmm_drv_run_info("No support remote map vmm host addr.\n");
            return -EOPNOTSUPP;
        }
#endif
        if ((heap == NULL) || (heap->heap_type != DEVMM_HEAP_PINNED_HOST)) {
            devmm_drv_err("Invalid heap. (va=0x%llx)\n", node->src_va);
            return -EINVAL;
        }
        if (devmm_check_va_add_size_by_heap(heap, node->src_va, node->size) != 0) {
            devmm_drv_err("Size is error. (va=0x%llx, size=%llu)\n", node->src_va, node->size);
            return -EINVAL;
        }

        node->src_va_is_pfn_map = true;
        ret = devmm_user_va_to_pages_list(svm_proc, node->src_va, node->page_num, node->pages);
    } else {
        node->src_va_is_pfn_map = false;
        ret = devmm_pin_user_pages_fast(node->src_va, node->page_num, write, node->pages);
    }
    if (ret != 0) {
        devmm_drv_err("Get pages fail. (va=0x%llx; expected_num=%llu; ret=%d)\n",
            node->src_va, node->page_num, ret);
        return -EFAULT;
    }
    return 0;
}

static int devmm_io_va_to_pages_list(struct devmm_svm_process *svm_proc, struct devmm_shm_node *node)
{
    ka_vm_area_struct_t *vma = NULL;
    bool is_ram_mem = true;
    int ret;
 
    ka_task_down_read(get_mmap_sem(svm_proc->mm));
    vma = ka_mm_find_vma(svm_proc->mm, node->src_va);
    if ((vma == NULL) || (vma->vm_start > node->src_va)) {
#ifndef EMU_ST
        ka_task_up_read(get_mmap_sem(svm_proc->mm));
        devmm_drv_err("Can not find vma.(va=[0x%llx];hostpid=[%d]).\n", node->src_va, svm_proc->process_id.hostpid);
        return -EFAULT;
#endif
    }
    if (!devmm_vma_is_pfn_map(vma)) {
        devmm_drv_err("IO memory is not pfn map.(va=[0x%llx];hostpid=[%d]).\n",
            node->src_va, svm_proc->process_id.hostpid);
        ka_task_up_read(get_mmap_sem(svm_proc->mm));
        return -EINVAL;
    }
    ka_task_up_read(get_mmap_sem(svm_proc->mm));
 
    node->src_va_is_pfn_map = true;
    node->src_pa_is_ram = false;
    ret = devmm_user_va_to_pa_list(svm_proc, node->src_va, node->page_num, node->pa_list);
    if (ret != 0) {
        devmm_drv_err("Get pages fail. (va=0x%llx; expected_num=%llu; ret=%d)\n",
            node->src_va, node->page_num, ret);
        return -EFAULT;
    }
    ret = devmm_pfn_is_io_mem(svm_proc, node->src_va, node->page_num, &is_ram_mem);
    if (ret || is_ram_mem) {
        devmm_drv_err("IO memory is ram. (va=0x%llx; hostpid=%d; ret=%d)\n",
            node->src_va, svm_proc->process_id.hostpid, ret);
        return -EINVAL;
    }
 
    return 0;
}

static int devmm_map_host_mem_to_device(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_shm_node *node)
{
    struct devmm_chan_remote_map *send_info = NULL;
    int ret;

    send_info = devmm_alloc_init_send_info(svm_proc, devids, map_para, KA_MM_PAGE_SIZE);
    if (send_info == NULL) {
        devmm_drv_err("Alloc send_info fail.\n");
        return -ENOMEM;
    }

    if (map_para->map_type == HOST_IO_MAP_DEV) {
        ret = devmm_io_va_to_pages_list(svm_proc, node);
    } else {
        ret = devmm_local_va_to_pages_list(svm_proc, node);
    }
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Get pages fail. (va=0x%llx; expected_num=%llu; ret=%d)\n",
            node->src_va, node->page_num, ret);
#endif
        goto exit;
    }
    ret = devmm_shm_send_info_to_dev(svm_proc, send_info, node);
    if (ret != 0) {
        devmm_drv_err("Send map_para to agent error. (src_va=0x%llx; map_size=%llu; devid=%u; ret=%d)\n",
            map_para->src_va, map_para->size, devids->devid, ret);
        devmm_dma_unmap_pages_by_node(node);
        if (node->src_va_is_pfn_map) {
            /* not_ram_mem just use pa, not use page*/
            if (node->src_pa_is_ram) {
                devmm_put_user_pages(node->pages, node->page_num, node->page_num);
            }
        } else {
            devmm_unpin_user_pages(node->pages, node->page_num, node->page_num);
        }
        goto exit;
    }

    map_para->dst_va = node->dst_va;

exit:
    devmm_free_send_info(send_info);
    return ret;
}

static struct devmm_palist_info *devmm_alloc_palist_info(struct devmm_shm_node *node)
{
    struct devmm_palist_info *palist_info = NULL;
    u64 *pa_blk = NULL;

    palist_info = (struct devmm_palist_info *)devmm_kvzalloc(sizeof(struct devmm_palist_info));
    if (palist_info == NULL) {
        devmm_drv_err("Alloc palist info fail. (palist_len=%lu)\n", sizeof(struct devmm_palist_info));
        return NULL;
    }
    pa_blk = devmm_kvzalloc(sizeof(u64) * node->page_num);
    if (pa_blk == NULL) {
        devmm_kvfree(palist_info);
        devmm_drv_err("Alloc pa blk fail. (page_num=%llu)\n", node->page_num);
        return NULL;
    }

    palist_info->pa_blk = pa_blk;
    palist_info->blk_num = (u32)node->page_num;
    return palist_info;
}

static void devmm_free_palist_info(struct devmm_palist_info *palist_info)
{
    devmm_kvfree(palist_info->pa_blk);
    devmm_kvfree(palist_info);
}

static int devmm_get_remote_pa(u64 va, u64 remain_num, u32 *num, struct devmm_chan_remote_map *send_info)
{
    u32 adap_num = KA_MM_PAGE_SIZE / send_info->page_size; /* device page size 4K */
    u64 remain_remote_num = remain_num * adap_num;
    u16 tem_num = (u16)ka_base_min(remain_remote_num, DEVMM_ACCESS_H2D_PAGE_NUM); /* max 512 */
    u32 send_max_len;
    int ret;

    send_info->src_va = va;
    send_info->head.extend_num = tem_num;
    send_max_len = (u32)(sizeof(struct devmm_chan_remote_map) + sizeof(u64) * (u32)(send_info->head.extend_num));
    ret = devmm_chan_msg_send(send_info, send_max_len, send_max_len);
    if (ret != 0) {
        devmm_drv_err("Send message failed. (va=0x%llx; num=%u; send_info_num=%u; ret=%d)\n",
            va, tem_num, send_info->head.extend_num, ret);
        return ret;
    }
    if (send_info->head.extend_num != tem_num) {
        devmm_drv_err("Page num is error. (va=0x%llx; expect_num=%u; real_num=%u)\n",
            va, tem_num, send_info->head.extend_num);
        return -ERANGE;
    }

    ret = devmm_merge_palist_to_hpalist(send_info->src_pa, tem_num, num, adap_num, send_info->page_size);
    if (ret != 0) {
        devmm_drv_err("Page is not continuous. (va=0x%llx; expect_num=%u; real_num=%u; adap_num=%u)\n",
            va, tem_num, send_info->head.extend_num, adap_num);
        return ret;
    }

    return 0;
}

int devmm_remote_pa_to_bar_pa(u32 devid, u64 *remote_pa, u64 pa_num, u64 *bar_pa)
{
    u64 i;
    int ret;

    for (i = 0; i < pa_num; i++) {
        ret = devdrv_devmem_addr_d2h(devid, remote_pa[i], &bar_pa[i]);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}

static void devmm_put_remote_page(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, struct devmm_shm_node *node)
{
    struct devmm_chan_remote_unmap send_info = {{{0}}};
    int ret;

    send_info.head.msg_id = DEVMM_CHAN_REMOTE_UNMAP_ID;
    send_info.head.process_id.hostpid = svm_proc->process_id.hostpid;
    send_info.head.dev_id = (u16)devids->devid;
    send_info.head.process_id.vfid = (u16)devids->vfid;
    send_info.src_va = node->src_va;
    send_info.dst_va = node->dst_va;
    send_info.size = node->size;
    send_info.map_type = node->map_type;
    send_info.proc_type = node->proc_type;

    ret = devmm_chan_msg_send(&send_info, sizeof(struct devmm_chan_remote_unmap), 0);
    if (ret != 0) {
        devmm_drv_warn("Send message. (src_va=0x%llx; size=%llu; dst_va=0x%llx; map_type=%u; proc_type=%u "
            "devid=%u; ret=%d)\n", send_info.src_va, node->size, send_info.dst_va, send_info.map_type,
            send_info.proc_type, devids->devid, ret);
    }
}

static int devmm_fill_palist(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_shm_node *node, struct devmm_palist_info *palist_info)
{
    struct devmm_chan_remote_map *send_info = NULL;
    u64 total_page_num = map_para->size >> KA_MM_PAGE_SHIFT;
    u64 tmp_va, ramaining_num;
    u32 got_num = 0; /* max DEVMM_ACCESS_H2D_PAGE_NUM */
    u64 j = 0;
    int ret;

    send_info = devmm_alloc_init_send_info(svm_proc, devids, map_para, devmm_svm->device_page_size);
    if (send_info == NULL) {
        devmm_drv_err("Alloc send_info fail.\n");
        return -ENOMEM;
    }

    for (tmp_va = map_para->src_va, ramaining_num = total_page_num; ramaining_num > 0;
        tmp_va += (u64)got_num * KA_MM_PAGE_SIZE, ramaining_num -= got_num, j += got_num) {
        ret = devmm_get_remote_pa(tmp_va, ramaining_num, &got_num, send_info);
        if (ret != 0) {
            goto send_msg_fail;
        }
        ret = devmm_remote_pa_to_bar_pa(devids->devid, send_info->src_pa, got_num, &palist_info->pa_blk[j]);
        if (ret != 0) {
            /* The log cannot be modified, because in the failure mode library. */
            devmm_drv_err("Va not bar addr. (va=0x%llx; src_va=0x%llx; devid=%u; ret=%d)\n",
                tmp_va, map_para->src_va, devids->devid, ret);
            goto send_msg_fail;
        }
    }
    devmm_free_send_info(send_info);
    return 0;

send_msg_fail:
    devmm_put_remote_page(svm_proc, devids, node);
    devmm_free_send_info(send_info);
    return ret;
}

static int devmm_remap_addrs_with_palist(struct devmm_svm_process *svm_proc, u64 va, u64 page_num,
     bool is_read_only_mem, struct devmm_palist_info *palist_info)
{
    ka_pgprot_t page_prot = devmm_make_nocache_pgprot(0);
    ka_vm_area_struct_t *vma = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 i, tmp_va, pa_addr;
    int ret;

    if (is_read_only_mem) {
        page_prot = devmm_make_readonly_pgprot(page_prot);
    }

    vma = devmm_find_vma(svm_proc, va);
    if (vma == NULL) {
        devmm_drv_err("Can't find vma. (svm_addr=0x%llx)\n", va);
        return -EINVAL;
    }

    ka_task_down_write(&svm_proc->host_fault_sem);
    for (i = 0, tmp_va = va; i < page_num; i++, tmp_va += KA_MM_PAGE_SIZE) {
        ret = devmm_va_to_pa(vma, tmp_va, &pa_addr);
        if (ret == 0) {
            ka_task_up_write(&svm_proc->host_fault_sem);
            devmm_drv_err("Address has been remapped. (va=0x%llx; tmp_va=0x%llx)\n", va, tmp_va);
            goto remap_pfn_range_fail;
        }
        ret = ka_mm_remap_pfn_range(vma, tmp_va, KA_MM_PFN_DOWN(palist_info->pa_blk[i]), KA_MM_PAGE_SIZE, page_prot);
        if (ret != 0) {
            ka_task_up_write(&svm_proc->host_fault_sem);
            devmm_drv_err("Remap_pfn_range fail. (i=%llu; va=0x%llx; page_num=%llu; ret=%d)\n",
                i, tmp_va, page_num, ret);
            goto remap_pfn_range_fail;
        }
        devmm_try_cond_resched(&stamp);
    }
    ka_task_up_write(&svm_proc->host_fault_sem);
    return 0;

remap_pfn_range_fail:
    if (i != 0) {
        devmm_zap_vma_ptes(vma, va, i * KA_MM_PAGE_SIZE);
    }
    return ret;
}

static int devmm_map_local_dev_mem_to_host(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_shm_node *node)
{
    struct devmm_palist_info *palist_info = NULL;
    int ret;

    ret = devmm_check_and_set_remote_map_state(svm_proc, NULL, map_para, devids, node->page_num);
    if (ret != 0) {
        return ret;
    }

    palist_info = devmm_alloc_palist_info(node);
    if (palist_info == NULL) {
        ret = -ENOMEM;
        goto alloc_palist_fail;
    }

    ret = devmm_fill_palist(svm_proc, devids, map_para, node, palist_info);
    if (ret != 0) {
        goto fill_palist_fail;
    }

    ret = devmm_remap_addrs_with_palist(svm_proc, map_para->dst_va, node->page_num,
        devmm_is_read_only_mem(map_para->access), palist_info);
    if (ret != 0) {
        goto remap_addrs_fail;
    }
    devmm_free_palist_info(palist_info);
    return 0;

remap_addrs_fail:
    devmm_put_remote_page(svm_proc, devids, node);
fill_palist_fail:
    devmm_free_palist_info(palist_info);
alloc_palist_fail:
    devmm_clear_svm_remote_map_status(svm_proc, node->dst_va, node->page_num, false);
    return ret;
}

static int devmm_svm_mem_map_para_check(struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    u32 map_type = map_para->map_type;
    u32 proc_type = map_para->proc_type;

    if ((attr->is_locked_host && (map_type != HOST_SVM_MAP_DEV) && (devmm_is_mem_map_by_pcie_th(map_type) == false )) ||
        (attr->is_locked_device && (map_type != DEV_SVM_MAP_HOST))) {
        devmm_drv_err("Map_type not match with va_attr. (map_type=%u; is_locked_host=%d; is_locked_device=%d)\n",
            map_type, attr->is_locked_host, attr->is_locked_device);
        return -EINVAL;
    }

    if (map_para->size == 0) {
        devmm_drv_err("Map size is zero. (map_type=%u)\n", map_para->map_type);
        return -EINVAL;
    }

    if (devmm_is_host_agent(devids->devid) && (attr->is_locked_host || attr->is_svm_remote_maped)) {
        devmm_drv_run_info("Not support host agent. (devid=%u; map_type=%u; locked_host=%u; remote_maped=%u)\n",
            devids->devid, map_type, attr->is_locked_host, attr->is_svm_remote_maped);
        return -EOPNOTSUPP;
    }

    if (proc_type != DEVDRV_PROCESS_CP1) {
        devmm_drv_run_info("Svm mem not support current proc_type. (devid=%u; proc_type=%u)\n",
            devids->devid, proc_type);
        return -EOPNOTSUPP;
    }

    if ((map_type == HOST_IO_MAP_DEV) && devmm_is_hccs_connect(devids->devid)) {
        devmm_drv_run_info("Not support hccs scene. (dev_id=%u; map_type=%u)\n", devids->devid, map_type);
        return -EOPNOTSUPP;
    }

    return 0;
}

static int devmm_dev_svm_map_para_check(struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    int ret;

    ret = devmm_svm_mem_map_para_check(devids, map_para, attr);
    if (ret != 0) {
        return ret;
    }

    if ((devids->devid < DEVMM_MAX_DEVICE_NUM) && (devids->devid != SVM_HOST_AGENT_ID)) {
        if (attr->is_svm_huge) {
            if (devmm_dev_capability_support_bar_huge_mem(devids->devid) == false) {
                devmm_drv_run_info("Not support pcie bar huge remote map. (svm_addr=0x%llx; devid=%d)\n",
                    map_para->src_va, devids->devid);
                return -EOPNOTSUPP;
            }
        } else {
            if (devmm_dev_capability_support_bar_mem(devids->devid) == false) {
                devmm_drv_run_info("Not support pcie bar remote map. (svm_addr=0x%llx; devid=%d)\n",
                    map_para->src_va, devids->devid);
                return -EOPNOTSUPP;
            }
        }
    }

    return 0;
}

static int devmm_local_dev_map_para_check(struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    u32 map_type = map_para->map_type;
    u32 proc_type = map_para->proc_type;
    u64 src_va = map_para->src_va;
    u64 dst_va = map_para->dst_va;
    u64 size = map_para->size;

    if (devmm_is_host_agent(devids->devid)) {
        devmm_drv_err("Map type not support host agent. (devid=%u; map_type=%u)\n",
            devids->devid, map_type);
        return -EOPNOTSUPP;
    }

    if (devmm_va_is_in_svm_range(src_va)) {
        devmm_drv_run_info("Src va can't be svm addr. (src_va=0x%llx)\n", src_va);
        return -EOPNOTSUPP;
    }

    if (devmm_va_is_in_svm_range(dst_va) == false) {
        devmm_drv_run_info("Dst va must be svm addr. (dst_va=0x%llx)\n", dst_va);
        return -EOPNOTSUPP;
    }

    if ((src_va == 0) || (KA_MM_PAGE_ALIGNED(src_va) == false)) {
        devmm_drv_err("Src_va is zero or not page alignment. (src_va=0x%llx)\n", src_va);
        return -EINVAL;
    }

    if ((size == 0) || (KA_MM_PAGE_ALIGNED(size) == false)) {
        devmm_drv_err("Size is zero or not page alignment. (size=0x%llx; page_size=%lu)\n",
            size, KA_MM_PAGE_SIZE);
        return -EINVAL;
    }

    if (proc_type != DEVDRV_PROCESS_HCCP) {
        devmm_drv_run_info("Local dev not support current proc_type. (current_proc_type=%u; expect_proc_type=%u)\n",
            proc_type, DEVDRV_PROCESS_HCCP);
        return -EOPNOTSUPP;
    }

    if ((devmm_dev_capability_support_dev_mem_map_host(devids->devid) != true) &&
        (devmm_dev_capability_support_bar_huge_mem(devids->devid) != true)) {
        devmm_drv_run_info("Not support dev mem map host and bar huge mem. (dev_id=%u)\n", devids->devid);
        return -EOPNOTSUPP;
    }
    return 0;
}

static int devmm_local_host_map_para_check(struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    u32 map_type = map_para->map_type;
    u32 proc_type = map_para->proc_type;
    u64 src_va = map_para->src_va;
    u64 size = map_para->size;

    if ((map_type != HOST_MEM_MAP_DEV) && devmm_is_mem_map_by_pcie_th(map_type) == false) {
        devmm_drv_err("Map_type not match with va_attr. (map_type=%u)\n", map_type);
        return -EINVAL;
    }

    if (devmm_is_host_agent(devids->devid)) {
        devmm_drv_run_info("Map type not support host agent. (devid=%u; map_type=%u)\n",
            devids->devid, map_type);
        return -EOPNOTSUPP;
    }

    if ((src_va == 0) || (KA_MM_PAGE_ALIGNED(src_va) == false) || (size == 0)) {
        devmm_drv_err("Src_va is zero or not page alignment, or map size is zero. (src_va=0x%llx; "
            "page_size=%lu; size=%llu)\n", src_va, KA_MM_PAGE_SIZE, size);
        return -EINVAL;
    }

    if (proc_type != DEVDRV_PROCESS_CP1) {
        devmm_drv_run_info("Local host not support current proc_type. (devid=%u; proc_type=%u)\n",
            devids->devid, proc_type);
        return -EOPNOTSUPP;
    }

    if ((map_type == HOST_IO_MAP_DEV) && devmm_is_hccs_connect(devids->devid)) {
        devmm_drv_run_info("Not support hccs scene. (dev_id=%u; map_type=%u)\n", devids->devid, map_type);
        return -EOPNOTSUPP;
    }
 
    if (devmm_is_mem_map_by_pcie_th(map_type)) {
        if ((devmm_dev_capability_support_pcie_th(devids->devid) != true)) {
            devmm_drv_run_info("Not support pcie through scene. (dev_id=%u)\n", devids->devid);
            return -EOPNOTSUPP;
        }
    } else if (map_type == HOST_MEM_MAP_DEV) {
        if (devmm_dev_capability_support_pcie_th(devids->devid)) {
            devmm_drv_run_info("Please use pcie through scene. (dev_id=%u)\n", devids->devid);
            return -EOPNOTSUPP;
        }
    } else {
        devmm_drv_run_info("Local host Not support current map type. (dev_id=%u; map_type=%u)\n",
            devids->devid, map_type);
        return -EOPNOTSUPP;
    }
    return 0;
}

typedef int (*devmm_remote_map_para_check)(struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr);

static int devmm_check_map_para(struct devmm_devid *devids, struct devmm_memory_attributes *attr,
    struct devmm_mem_remote_map_para *map_para)
{
    static const devmm_remote_map_para_check map_para_check[DEVMM_MAX_MEM_TYPE] = {
        [DEVMM_LOCAL_HOST_MEM] = devmm_local_host_map_para_check,
        [DEVMM_LOCKED_HOST_MEM] = devmm_svm_mem_map_para_check,
        [DEVMM_LOCAL_DEV_MEM] = devmm_local_dev_map_para_check,
        [DEVMM_LOCKED_DEV_MEM] = devmm_dev_svm_map_para_check,
    };
    u32 mem_type;

    mem_type = devmm_get_remote_mem_type(attr, map_para->map_type);
    if (mem_type >= DEVMM_MAX_MEM_TYPE) {
        devmm_drv_run_info("Current src_va isn't support remote map. (src_va=0x%llx)\n", map_para->src_va);
        return -EOPNOTSUPP;
    }

    return map_para_check[mem_type](devids, map_para, attr);
}

static int devmm_mem_remote_map_of_local_mem(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para)
{
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    struct devmm_shm_node *node = NULL;
    ka_semaphore_t *shm_sem = NULL;
    u32 map_type = map_para->map_type;
    bool result = false;
    int ret;

    map_para->size = ka_base_round_up(map_para->size, KA_MM_PAGE_SIZE);
    shm_sem = devmm_get_shm_sem(svm_proc, map_type);
    ka_task_down(shm_sem);
    shm_pro_node = devmm_get_shm_pro_node(svm_proc, map_type);
    result = devmm_judge_shm_node_overlap(&shm_pro_node->shm_head, map_para, devids);
    if (result == true) {
        ka_task_up(shm_sem);
        devmm_drv_err("Local memory has been mapped. (hostPtr=0x%llx; size=%llu; devid=%u; map_type=%u)\n",
            map_para->src_va, map_para->size, devids->devid, map_type);
        return -EINVAL;
    }

    node = devmm_create_shm_node(shm_pro_node, devids, map_para);
    if (node == NULL) {
        ka_task_up(shm_sem);
        devmm_drv_err("Create shm node error. (src_va=0x%llx; map_type=%u)\n",
            map_para->src_va, map_type);
        return -ENOMEM;
    }
    (node->ref)++;
    ka_task_up(shm_sem);

    if ((map_type == HOST_MEM_MAP_DEV) || devmm_is_mem_map_by_pcie_th(map_type)) {
        ret = devmm_map_host_mem_to_device(svm_proc, devids, map_para, node);
    } else { /* DEV_MEM_MAP_HOST */
        ret = devmm_map_local_dev_mem_to_host(svm_proc, devids, map_para, node);
    }

    devmm_drv_debug("Remote_map local memory. (src_va=0x%llx; dst_va=0x%llx; size=%llu; devid=%u; map_type=%d)\n",
        map_para->src_va, map_para->dst_va, map_para->size, devids->devid, map_type);

    ka_task_down(shm_sem);
    (node->ref)--;
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Map shm fail. (src_va=0x%llx; size=%llu; devid=%u; map_type=%u; ret=%d)\n",
            map_para->src_va, map_para->size, devids->devid, map_type, ret);
        devmm_destory_shm_node(node);
    }
    ka_task_up(shm_sem);

    return ret;
}

STATIC int devmm_svm_mem_remote_map(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    if (attr->is_locked_host) {
        return devmm_map_master_svm_mem_to_agent(svm_proc, devids, map_para, attr);
    } else {
        return devmm_map_agent_svm_mem_to_master(svm_proc, devids, map_para, attr);
    }
}

static int devmm_mem_remote_map_of_svm_mem(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_map_para *map_para, struct devmm_memory_attributes *attr)
{
    u64 page_bitmap_num;
    int ret;

    map_para->size = ka_base_round_up(map_para->size, attr->page_size); /* updata size to align up page size */
    page_bitmap_num = map_para->size / attr->granularity_size;

    ret = devmm_check_and_set_remote_map_state(svm_proc, attr, map_para, devids, page_bitmap_num);
    if (ret != 0) {
        devmm_drv_err("Map_para is invalid. (src_va=0x%llx; map_size=%llu; devid=%u; bitmap=%u; ret=%d)\n",
            map_para->src_va, map_para->size, devids->devid, attr->bitmap, ret);
        return ret;
    }

    ret = devmm_svm_mem_remote_map(svm_proc, devids, map_para, attr);
    if (ret != 0) {
        devmm_drv_err("Remote_map svm memory fail. (src_va=0x%llx; size=%llu; is_local_host=%u; devid=%u; ret=%d)\n",
            map_para->src_va, map_para->size, attr->is_local_host, devids->devid, ret);
        goto err_exit;
    }

    devmm_drv_debug("Remote_map svm memory. (src_va=0x%llx; size=%llu; is_local_host=%u; devid=%u)\n",
        map_para->src_va, map_para->size, attr->is_locked_host, devids->devid);
    return 0;

err_exit:
    devmm_clear_svm_remote_map_status(svm_proc, attr->va, page_bitmap_num, attr->is_locked_host);
    return ret;
}

STATIC int devmm_host_remap_to_agent_enable_config(struct devmm_svm_process *svm_proc, struct devmm_devid *devids, u32 map_type)
{
    if (!devmm_is_host_agent(devids->devid) && devmm_is_pcie_connect(devids->devid)) {
        ka_semaphore_t *shm_sem = devmm_get_shm_sem(svm_proc, HOST_MEM_MAP_DEV);
        u32 host_flag;
        int ret;

        if (devmm_smmu_is_opening()) {
#ifndef EMU_ST
            devmm_drv_run_info("Mem_remote_map not support SMMU open. (smmu_status=%u)\n", devmm_svm->smmu_status);
#endif
            return -EOPNOTSUPP;
        }

#ifndef EMU_ST
        if (map_type == HOST_SVM_MAP_DEV) {
            ret = devdrv_get_host_phy_mach_flag(devids->devid, &host_flag);
            if (ret != 0) {
                devmm_drv_err("Get host physics flag failed.(devid=%u;ret=%d).\n", devids->devid, ret);
                return ret;
            }

            if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG) { /* vm config txatu may cause security issues. */
                devmm_drv_run_info("Shm config txatu not support virt machine. (devid=%u)\n", devids->devid);
                return -EOPNOTSUPP;
            }
        }
#endif

        ka_task_down(shm_sem);
        ret = devmm_shm_config_txatu(svm_proc, devids->devid);
        ka_task_up(shm_sem);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err_if((ret != -EOPNOTSUPP), "Config txatu fail. (succ_flag=%u; ret=%d)\n", g_mem_info.succ_flag, ret);
#endif
            return ret;
        }
    }

    return 0;
}

static bool devmm_is_local_remote_map(u32 map_type, struct devmm_memory_attributes *attr)
{
    return (devmm_is_local_dev_mem_type(map_type) || attr->is_local_host ||
        (attr->is_locked_host && devmm_is_mem_map_by_pcie_th(map_type)));
}

int devmm_ioctl_mem_remote_map(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_remote_map_para *map_para = &arg->data.map_para;
    struct devmm_memory_attributes attr = {0};
    struct devmm_devid devids = arg->head; /* logic devid will updata to devid and vfid */
    u32 map_type = map_para->map_type;
    int ret;

    if (map_type >= HOST_REGISTER_MAX_TPYE) {
        devmm_drv_err("Map type is invalid. (map_type=%u)\n", map_type);
        return -EINVAL;
    }

    devmm_drv_debug("Remote map info. (map_type=%u; proc_type=%u; src_va=0x%llx; dst_va=0x%llx; size=%llu)\n",
        map_type, map_para->proc_type, map_para->src_va, map_para->dst_va, map_para->size);

    if (devmm_is_local_dev_mem_type(map_type) == false) {
        ret = devmm_get_memory_attributes(svm_proc, map_para->src_va, &attr);
        if (ret != 0) {
            devmm_drv_err("Get attributes fail. (src_va=0x%llx; ret=%d)\n", map_para->src_va, ret);
            return ret;
        }
    }

    /* remote_map device_agent_mem didn't pass devid in, so we should update devids */
    if (attr.is_locked_device) {
        devmm_update_devids(&devids, attr.logical_devid, attr.devid, attr.vfid);
    }

    ret = devmm_check_map_para(&devids, &attr, map_para);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP), "Check map para fail.\n");
        return ret;
    }

    /* device map to host use bar, do not need config txatu, ignore smmu open or not */
    if ((attr.is_local_host || attr.is_locked_host) && (devmm_is_mem_map_by_pcie_th(map_type) == false ) &&
        (devmm_is_local_dev_mem_type(map_type) == false)) {
        ret = devmm_host_remap_to_agent_enable_config(svm_proc, &devids, map_type);
        if (ret != 0) {
            devmm_drv_err_if((ret != -EOPNOTSUPP), "Enable host config fail.\n");
            return ret;
        }
    }

    devmm_drv_debug("Remote_map info. (map_type=%u; proc_type=%u; src_va=0x%llx; dst_va=0x%llx; size=%llu; "
        "logical_devid=%u; is_local_host=%d; is_locked_host=%d; is_locked_device=%d)\n",
        map_type, map_para->proc_type, map_para->src_va, map_para->dst_va, map_para->size, attr.logical_devid,
        attr.is_local_host, attr.is_locked_host, attr.is_locked_device);

    if (devmm_is_local_remote_map(map_type, &attr)) {
        return devmm_mem_remote_map_of_local_mem(svm_proc, &devids, map_para);
    } else {
        return devmm_mem_remote_map_of_svm_mem(svm_proc, &devids, map_para, &attr);
    }
}

STATIC int devmm_check_unmap_para_of_svm_mem(struct devmm_svm_process *svm_proc,
    struct devmm_memory_attributes *attr, u64 src_va)
{
    if ((src_va & (attr->page_size - 1)) != 0) {
        devmm_drv_err("Src_va is not page alignment. (src_va=0x%llx; page_size=%u)\n", src_va, attr->page_size);
        return -EINVAL;
    }

    if (!devmm_page_bitmap_is_remote_mapped_first(&attr->bitmap)) {
        devmm_drv_err("Src_va hasn't been remote mapped. (src_va=0x%llx; bitmap=%u)\n", src_va, attr->bitmap);
        return -EINVAL;
    }

    return 0;
}

STATIC int devmm_check_unmap_para(u32 unmap_type)
{
    if (unmap_type >= HOST_REGISTER_MAX_TPYE) {
        devmm_drv_err("Unmap type is invalid. (unmap_type=%u)\n", unmap_type);
        return -EINVAL;
    }

    return 0;
}

static void devmm_unmap_local_host_mem(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_shm_node *node)
{
    int ret;

    ret = devmm_local_host_unmap(svm_proc, devids, node);
    if (ret != 0) {
        devmm_drv_warn("Send message fail. (src_va=0x%llx; size=%llu; dst_va=0x%llx; devid=%u; ret=%d)\n",
            node->src_va, node->size, node->dst_va, devids->devid, ret);
        /* host should unpin page when msg send failed */
    }
    devmm_dma_unmap_pages_by_node(node);
    if (node->src_va_is_pfn_map) {
        if (node->src_pa_is_ram) {
            devmm_put_user_pages(node->pages, node->page_num, node->page_num);
        }
    } else {
        devmm_unpin_user_pages(node->pages, node->page_num, node->page_num);
    }
}

static void devmm_unmap_local_dev_mem(struct devmm_svm_process *svm_proc,
    struct devmm_devid *devids, struct devmm_shm_node *node)
{
    devmm_unmap_mem(svm_proc, node->dst_va, node->size);
    devmm_put_remote_page(svm_proc, devids, node);
    devmm_clear_svm_remote_map_status(svm_proc, node->dst_va, node->page_num, false);
}

#define DEVMM_LOCAL_MEM_MAX_NUM 3
static void devmm_get_local_mem_unmap_type_info(u32 unmap_type, u32 *unmap_type_num, u32 *unmap_type_info)
{
    if (unmap_type == DEV_MEM_MAP_HOST) {
        *unmap_type_num = 1;
        unmap_type_info[0] = DEV_MEM_MAP_HOST;
    } else {
        *unmap_type_num = DEVMM_LOCAL_MEM_MAX_NUM;
        unmap_type_info[0] = HOST_MEM_MAP_DEV;
        unmap_type_info[1] = HOST_MEM_MAP_DEV_PCIE_TH;
        unmap_type_info[2] = HOST_IO_MAP_DEV;
    }
}

static int _devmm_unmap_local_mem(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_shm_node *node)
{
    if (node->ref > 0) {
        devmm_drv_debug("Shm still used by device. (src_va=0x%llx; src_va=0x%llx; ref=%llu; map_type=%u; devid=%u)\n",
            node->src_va, node->dst_va, node->ref, node->map_type, devids->devid);
        return -EBUSY;
    }
    devmm_drv_debug("Unmap local mem. (src_va=0x%llx; dst_va=0x%llx; unmap_type=%u; size=%llu; devid=%u)\n",
        node->src_va, node->dst_va, node->map_type, node->size, devids->devid);

    if ((node->map_type == HOST_MEM_MAP_DEV) || devmm_is_mem_map_by_pcie_th(node->map_type)) {
        devmm_unmap_local_host_mem(svm_proc, devids, node);
    } else if (node->map_type == DEV_MEM_MAP_HOST) {
        devmm_unmap_local_dev_mem(svm_proc, devids, node);
    }
    return 0;
}

static int devmm_unmap_local_mem(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_unmap_para *unmap_para, u32 unmap_type)
{
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    struct devmm_shm_node *node = NULL;
    int ret;

    shm_pro_node = devmm_get_shm_pro_node(svm_proc, unmap_type);
    node = devmm_get_shm_node(&shm_pro_node->shm_head, unmap_para->src_va, devids->devid, devids->vfid);
    if (node == NULL || node->dst_va == 0) {
        return -ENOENT;
    }
    ret = _devmm_unmap_local_mem(svm_proc, devids, node);
    if (ret == 0) {
        unmap_para->dst_va = node->dst_va;
        devmm_destory_shm_node(node);
    }
    return ret;
}

static int devmm_mem_remote_unmap_of_local_mem(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_unmap_para *unmap_para)
{
    ka_semaphore_t *shm_sem = NULL;
    u32 unmap_type_info[DEVMM_LOCAL_MEM_MAX_NUM];
    u32 unmap_type_num, i;
    int ret;

    devmm_get_local_mem_unmap_type_info(unmap_para->map_type, &unmap_type_num, unmap_type_info);
    for (i = 0; i < unmap_type_num; i++) {
        shm_sem = devmm_get_shm_sem(svm_proc, unmap_type_info[i]);
        ka_task_down(shm_sem);
        ret = devmm_unmap_local_mem(svm_proc, devids, unmap_para, unmap_type_info[i]);
        ka_task_up(shm_sem);
        if ((ret == -EBUSY) || (ret == 0)) {
            return ret;
        }
    }
    devmm_drv_err("Get shm node fail. (host_va=0x%llx; devid=%u; unmap_type=%u; ret=%d)\n",
        unmap_para->src_va, devids->devid, unmap_para->map_type, ret);
    return ret;
}

static int devmm_svm_mem_remote_unmap(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_memory_attributes *attr, u64 src_va, u64 size)
{
    if (attr->is_locked_host) {
        return devmm_locked_host_unmap(svm_proc, devids, src_va, size, src_va);
    } else {
        return devmm_check_and_unmap_mem(svm_proc, src_va, size);
    }
}

STATIC u64 devmm_get_remote_map_size_from_va(struct devmm_svm_heap *heap, u32 *bitmap, u64 va)
{
    u64 alloc_page_num, seek_page_num, i, alloced_va, size;
    int ret;

    ret = devmm_get_alloced_va_with_heap(heap, va, &alloced_va);
    if (ret != 0) {
        return 0;
    }

    alloc_page_num = devmm_get_page_num_from_va(heap, va);
    seek_page_num = alloc_page_num - ((va - alloced_va) / heap->chunk_page_size);

    for (i = 1; i < seek_page_num; i++) {  /* va is already check */
        if (!devmm_page_bitmap_is_remote_mapped(bitmap + i) ||
            devmm_page_bitmap_is_remote_mapped_first(bitmap + i)) {
            break;
        }
    }
    size = i * heap->chunk_page_size;

    return size;
}

static int devmm_mem_remote_unmap_of_svm_mem(struct devmm_svm_process *svm_proc, struct devmm_devid *devids,
    struct devmm_mem_remote_unmap_para *unmap_para, struct devmm_memory_attributes *attr)
{
    struct devmm_svm_heap *heap = NULL;
    u64 page_bitmap_num, size;
    u32 *bitmap = NULL;
    int ret;

    ret = devmm_check_unmap_para_of_svm_mem(svm_proc, attr, unmap_para->src_va);
    if (ret != 0) {
        devmm_drv_err("Check unmap_para fail. (src_va=0x%llx; devid=%u; ret=%d)\n",
            unmap_para->src_va, devids->devid, ret);
        return ret;
    }

    heap = devmm_svm_get_heap(svm_proc, unmap_para->src_va);
    if (heap == NULL) {
        devmm_drv_err("Get heap error. (va=0x%llx)\n", unmap_para->src_va);
        return -EINVAL;
    }

    bitmap = devmm_get_page_bitmap_with_heap(heap, unmap_para->src_va);
    if (bitmap == NULL) {
        devmm_drv_err("Get bitmap error. (va=0x%llx)\n", unmap_para->src_va);
        return -EINVAL;
    }

    size = devmm_get_remote_map_size_from_va(heap, bitmap, unmap_para->src_va);
    if (size == 0) {
        devmm_drv_err("Get alloced_size fail. (src_va=0x%llx)\n", unmap_para->src_va);
        return -EINVAL;
    }

    ret = devmm_svm_mem_remote_unmap(svm_proc, devids, attr, unmap_para->src_va, size);
    if (ret != 0) {
        devmm_drv_err("Unmap mem fail. (src_va=0x%llx; size=%llu; is_locked_host=%u; devid=%u; ret=%d)\n",
            unmap_para->src_va, size, attr->is_locked_host, devids->devid, ret);
        return ret;
    }

    page_bitmap_num = size / heap->chunk_page_size;
    devmm_clear_remote_map_status(bitmap, page_bitmap_num, attr->is_locked_host);

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        devmm_vmmas_occupy_dec(&heap->vmma_mng, unmap_para->src_va, size);
    }

    devmm_drv_debug("Unmap svm mem. (src_va=0x%llx; size=%llu; is_locked_host=%u; devid=%u)\n",
        unmap_para->src_va, size, attr->is_locked_host, devids->devid);

    return 0;
}

int devmm_ioctl_mem_remote_unmap(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_remote_unmap_para *unmap_para = &arg->data.unmap_para;
    struct devmm_memory_attributes attr = {0};
    struct devmm_devid devids = arg->head; /* logic devid will updata to devid and vfid */
    u32 unmap_type = unmap_para->map_type;
    int ret;

    ret = devmm_check_unmap_para(unmap_type);
    if (ret != 0) {
        return ret;
    }

    if (devmm_is_local_dev_mem_type(unmap_type) == false) {
        ret = devmm_get_memory_attributes(svm_proc, unmap_para->src_va, &attr);
        if (ret != 0) {
            devmm_drv_err("Get attributes fail. (src_va=0x%llx; ret=%d)\n", unmap_para->src_va, ret);
            return ret;
        }
    }

    /* remote_unmap didn't pass devid in, so we should update devids */
    if (attr.is_locked_device || (attr.is_locked_host && (devmm_is_mem_map_by_pcie_th(unmap_type) == false))) {
        devmm_update_devids(&devids, attr.logical_devid, attr.devid, attr.vfid);
    }

    devmm_drv_debug("Remote_unmap info. (src_va=0x%llx; unmap_type=%u; logical_devid=%u; "
        "is_local_host=%d; is_locked_host=%d; is_locked_device=%d)\n", unmap_para->src_va,
        unmap_type, attr.logical_devid, attr.is_local_host, attr.is_locked_host,
        attr.is_locked_device);
    if (devmm_is_local_remote_map(unmap_type, &attr)) {
        return devmm_mem_remote_unmap_of_local_mem(svm_proc, &devids, unmap_para);
    } else if (attr.is_locked_host || attr.is_locked_device) {
        return devmm_mem_remote_unmap_of_svm_mem(svm_proc, &devids, unmap_para, &attr);
    } else {
#ifndef EMU_ST
        devmm_drv_run_info("Current src_va isn't support remote unmap. (src_va=0x%llx)\n", unmap_para->src_va);
#endif
        return -EOPNOTSUPP;
    }
}

void devmm_try_destroy_remote_map_nodes(struct devmm_svm_process *svm_proc, u32 log_dev, u32 phy_dev, u32 vfid)
{
    u32 hostpid = (u32)svm_proc->process_id.hostpid;
    struct devmm_shm_pro_node *shm_pro_node = NULL;
    struct devmm_shm_node *node = NULL;
    ka_semaphore_t *shm_sem = NULL;
    struct devmm_devid devids = {0};
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    int ret;
    u32 i;

    devmm_update_devids(&devids, log_dev, phy_dev, vfid);
    for (i = 0; i < HOST_REGISTER_MAX_TPYE; i++) {
        shm_sem = devmm_get_shm_sem(svm_proc, i);
        ka_task_down(shm_sem);
        shm_pro_node = devmm_get_shm_pro_node(svm_proc, i);
        if (shm_pro_node == NULL) {
            ka_task_up(shm_sem);
            continue;
        }
        /* try destory_shm_node */
        if (!ka_list_empty_careful(&shm_pro_node->shm_head.list)) {
            ka_list_for_each_safe(pos, n, &shm_pro_node->shm_head.list) {
                node = ka_list_entry(pos, struct devmm_shm_node, list);
                if ((node->logical_devid == log_dev) && (node->dev_id == phy_dev) && (node->vfid == vfid)) {
                    ret = _devmm_unmap_local_mem(svm_proc, &devids, node);
                    if (ret == 0) {
                        devmm_destory_shm_node(node);
                    }
                }
                devmm_try_cond_resched(&stamp);
            }
        }
        /* try destory_shm_node finish, try del_txatu */
        if (ka_list_empty_careful(&shm_pro_node->shm_head.list)) {
            devmm_del_txatu_by_dev_id(&shm_pro_node->txatu_info[0], DEVMM_MAX_TXATU, hostpid, phy_dev);
        }

        ka_task_up(shm_sem);
        devmm_try_cond_resched(&stamp);
    }

    return;
}

void devmm_destory_remote_map_mem_by_devid(struct devmm_svm_process *svm_proc,
    struct devmm_svm_heap *heap, u32 logical_devid)
{
    u64 page_cnt = heap->heap_size / heap->chunk_page_size;
    u32 *bitmap = heap->page_bitmap;
    u64 tmp_va = heap->start;
    u32 stamp = (u32)ka_jiffies;
    u64 i;

    for (i = 0; i < page_cnt; i++, tmp_va += heap->chunk_page_size) {
        if (devmm_page_bitmap_is_remote_mapped(bitmap + i) &&
            (devmm_page_bitmap_get_devid(bitmap + i) == logical_devid)) {
            devmm_page_bitmap_clear_flag(bitmap + i, DEVMM_PAGE_REMOTE_MAPPED_MASK);
            if (devmm_page_bitmap_is_locked_host(bitmap + i)) {
                devmm_page_bitmap_set_devid(bitmap + i, DEVMM_MAX_DEVICE_NUM + 1);
            }
            if (devmm_page_bitmap_is_locked_device(bitmap + i)) {
                (void)devmm_unmap_mem(svm_proc, tmp_va, heap->chunk_page_size);
            }
        }
        devmm_try_cond_resched(&stamp);
    }
}

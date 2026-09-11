/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Create: 2022-08-13
 */

#ifndef __SVM_CMD_H__
#define __SVM_CMD_H__

#include <linux/types.h>

#include "ascend_hal_define.h"
#include "svm_define.h"

#ifdef DEVMM_UT
#define STATIC
#define INLINE
#else
#define STATIC static
#define INLINE inline
#endif

#define DEVMM_SVM_MAGIC 'M'

struct devmm_svm_process_id {
    int32_t hostpid;
    union {
        uint16_t devid;
        uint16_t vm_id;
    };
    uint16_t vfid;
};

struct devmm_mem_alloc_host_para {
    uint64_t p;
    size_t size;
};

struct devmm_mem_free_host_para {
    uint64_t p;
};

struct devmm_mem_alloc_para {
    uint64_t p;
    size_t size;
};

struct devmm_mem_copy_para {
    uint64_t dst;
    uint64_t src;
    size_t ByteCount;
    bool is_support_dev_local_addr;
    enum devmm_copy_direction direction;
};

struct devmm_mem_copy_batch_para {
    uint64_t *dst;
    uint64_t *src;
    size_t *size;
    uint32_t addr_count;
};

struct devmm_mem_async_copy_para {
    uint64_t dst;
    uint64_t src;
    size_t byte_count;
    uint64_t start_time;
    volatile uint64_t cpy_state;
    int task_id;
};

struct devmm_mem_convert_copy_para {
    struct DMA_ADDR dmaAddr;
    int sync_flag;
};

struct devmm_mem_copy2d_para {
    uint64_t dst;
    uint64_t src;
    uint64_t dpitch;
    uint64_t spitch;
    uint64_t width;
    uint64_t height;
    enum devmm_copy_direction direction;
};

struct devmm_mem_convrt_addr_para {
    uint64_t pSrc;
    uint64_t pDst;
    uint64_t spitch;
    uint64_t dpitch;
    uint64_t len;
    uint64_t height;
    uint64_t fixed_size;
    enum devmm_copy_direction direction;
    struct DMA_ADDR dmaAddr;
    uint32_t dma_node_num;
    uint32_t virt_id;           /* store logic id to destroy addr */
    uint32_t dev_id;            /* used in virt machine, to get sqcq addr in pm */
    uint32_t destroy_flag;      /* used in virt machine, to destroy the res */
    uint64_t convert_id;  /* for vm safety check */
    bool need_write;
    /* pa_list must be the last one, cann't be changed */
    struct devmm_pa_batch *pa_batch;  /* used in virt machine, to destroy pa list */
};

struct devmm_mem_destroy_addr_para {
    uint64_t pSrc;  /* pSrc pDst just use in virt machine */
    uint64_t pDst;
    uint64_t spitch;
    uint64_t dpitch;
    uint64_t len;
    uint64_t height;
    uint64_t fixed_size;
    struct DMA_ADDR dmaAddr; /* user to kernel */
    pid_t host_pid;
};

struct devmm_destroy_addr_batch_para {
    struct DMA_ADDR **dmaAddr;
    uint32_t num;
};

struct devmm_mem_advise_para {
    uint64_t ptr;
    size_t count;
    uint32_t advise;
};

struct devmm_mem_prefetch_para {
    uint64_t devPtr;
    size_t count;
};

struct devmm_mem_memset_para {
    uint64_t dst;
    uint64_t value;
    uint64_t count;
    uint32_t hostmapped;
};

struct devmm_mem_translate_para {
    uint64_t vptr;
    uint64_t pptr;
    uint32_t addr_in_device;
};

#define DEVMM_MAX_NAME_SIZE 65
struct devmm_mem_ipc_create_para {
    uint64_t vptr;
    size_t len;
    char name[DEVMM_MAX_NAME_SIZE];
    uint32_t name_len;
};

#define DEVMM_SECONDARY_PROCESS_NUM (32)
struct devmm_mem_ipc_set_pid_para {
    char name[DEVMM_MAX_NAME_SIZE];
    int32_t set_pid[DEVMM_SECONDARY_PROCESS_NUM];
    uint32_t num;
    uint32_t sdid; /* UINT32_MAX means current pod */
};

struct devmm_mem_ipc_destroy_para {
    char name[DEVMM_MAX_NAME_SIZE];
};

struct devmm_mem_query_size_para {
    char name[DEVMM_MAX_NAME_SIZE];
    int32_t is_huge;
    uint32_t phy_devid;
    size_t len;
};

struct devmm_mem_ipc_open_para {
    uint64_t vptr;
    char name[DEVMM_MAX_NAME_SIZE];
};
struct devmm_mem_ipc_close_para {
    uint64_t vptr;
};

struct devmm_mem_ipc_set_attr_para {
    uint32_t type;                    /* input */
    uint64_t attr;                    /* input */
    char name[DEVMM_MAX_NAME_SIZE];   /* input */
};

struct devmm_mem_ipc_get_attr_para {
    uint32_t type;                    /* input */
    uint64_t attr;                    /* output */
    char name[DEVMM_MAX_NAME_SIZE];   /* input */
};

struct devmm_init_process_para {
    uint32_t svm_page_size;
    uint32_t local_page_size;
    uint32_t huge_page_size;
    bool is_enable_host_giant_page;
};

struct devmm_update_heap_para {
    uint32_t op;
    uint32_t heap_type;
    uint32_t heap_sub_type;
    uint32_t heap_idx;
    uint64_t heap_size;
};

struct devmm_query_process_status_para {
    processType_t process_type;
    processStatus_t pid_status;
    uint32_t status_result;
};

struct devmm_mmap_addr_seg {
    uint64_t va;
    uint64_t size;
};

struct devmm_get_mmap_para {
    uint32_t seg_num;
    bool is_need_map_nptmv;
    int32_t hostpid; /* for custom dynamic */
    int32_t aicpupid; /* for custom dynamic */
    struct devmm_mmap_addr_seg *segs;
};

#define DEVMM_DEFAULT_SECTION_ORDER (7)

struct devmm_free_pages_para {
    uint64_t va;
};

struct devmm_dev_init_process_para {
    struct devmm_svm_process_id process_id;
    int32_t devpid;
};

struct devmm_dev_bind_sibling_para {
    int32_t hostpid;
    int32_t aicpupid;
    uint32_t vfid;
    uint32_t devid;
};

struct devmm_status_va_info_para {
    uint64_t va;
    uint32_t devid;
    uint32_t mem_type;
};

struct devmm_status_va_check_para {
    uint64_t va;
    uint64_t count;
    int32_t msg_id;
    uint32_t devid;
    uint32_t heap_idx;
};

struct devmm_query_device_mem_usedinfo {
    uint32_t mem_type;
    size_t normal_total_size;
    size_t normal_free_size;
    size_t huge_total_size;
    size_t huge_free_size;
    size_t giant_total_size;
    size_t giant_free_size;
};

struct devmm_lock_cmd {
    uint64_t devPtr;
    uint32_t lockType;
};

struct devmm_polling_cmd {
    pid_t hostpid;
    pid_t devpid;
    uint32_t devid;
    uint32_t devnum;
    uint32_t cmd;
};
struct devmm_program_load_cmd {
    uint32_t devid;
    int is_loaded;
};

struct devmm_set_read_count {
    uint64_t addr;
    size_t size;
    uint32_t rc;
};

struct devmm_dbg_info {
    pid_t hostpid;
    pid_t user_devpid;
    uint32_t user_dbg_state;
};

struct devmm_mem_remote_map_para {
    uint64_t src_va;
    uint64_t dst_va;
    uint64_t size;
    uint32_t map_type;
    uint32_t proc_type;
    uint32_t access;
};

struct devmm_mem_remote_unmap_para {
    uint64_t src_va;
    uint32_t map_type;
    uint32_t proc_type;
    uint64_t dst_va; /* DEV_MEM_MAP_HOST return */
};

struct devmm_mem_map_cap_para {
    uint32_t acc_module_type;
    uint32_t mem_map_capability;
};

struct devmm_register_dma_para {
    uint64_t vaddr;
    uint64_t size;
};
struct devmm_unregister_dma_para {
    uint64_t vaddr;
};

/* user mode: devid means logic_id
   kernel mode: devid means phyid
 */
struct devmm_devid {
    uint32_t logical_devid;
    uint32_t devid;
    uint32_t vfid;
};

#define DEVMM_DEV_ADDR_NUM_MAX 1024 /* the limit of address count is 1024 */
struct devmm_check_mem_info {
    uint64_t *va;
    uint32_t cnt;
    uint32_t heap_subtype_mask;
};

struct devmm_map_dev_reserve_para {
    uint32_t addr_type; /* l2buff or c2c_ctrl or rtsq_reg */
    uint64_t va;
    uint64_t len;
};

struct devmm_setup_dev_para {
    uint64_t dvpp_mem_size;
    uint32_t support_bar_mem;
    uint32_t support_dev_read_only;
    uint32_t support_dev_mem_map_host;
    uint32_t support_bar_huge_mem;
    uint32_t host_support_pin_user_pages_interface;
    uint32_t support_host_rw_dev_ro;
    uint32_t support_host_pin_pre_register;
    uint32_t support_host_mem_pool;
    uint64_t double_pgtable_offset;
    uint64_t mem_stats_va;
    uint32_t support_agent_giant_page;
    uint32_t support_remote_mmap;
    uint32_t support_shmem_map_exbus;
    uint32_t support_mem_host_uva;
};

struct devmm_mem_create_para {
    int id;
    uint32_t host_numa_id;
    uint64_t size;
    uint64_t flag;
    uint32_t side;
    uint32_t module_id;
    uint32_t pg_type;
    uint32_t mem_type;
    uint64_t pg_num;
};

struct devmm_mem_release_para {
    int id;
    uint32_t side;
    uint64_t pg_num;

    uint32_t handle_type;
};

struct devmm_mem_map_para {
    uint64_t va;
    uint64_t size;

    int id;
    uint32_t side;
    uint32_t pg_type;
    uint32_t module_id; /* No actual use, just for handle verify. */
    uint64_t pg_num;    /* No actual use, just for handle verify. */
};

struct devmm_mem_unmap_para {
    uint64_t va;         /* input */
    uint64_t unmap_size; /* output */
    uint32_t side;            /* output */
    uint32_t logic_devid;     /* output */
};

/* va/size must be same with struct devmm_mem_map_para */
struct devmm_mem_query_owner_para {
    uint64_t va; /* intput */
    uint64_t size; /* intput */
    uint32_t devid; /* output: phy devid, host is halGetHostID */
    uint32_t local_handle_flag; /* output */
};

/* va/size must be same with struct devmm_mem_map_para */
struct devmm_mem_set_access_para {
    uint64_t va; /* intput */
    uint64_t size; /* intput */
    uint32_t logic_devid; /* intput: host is halGetHostID */
    drv_mem_access_type type; /* intput */
};

/* va/size must be same with struct devmm_mem_map_para */
struct devmm_mem_get_access_para {
    uint64_t va; /* intput */
    uint64_t size; /* intput/output */
    uint32_t logic_devid; /* intput: host is halGetHostID */
    drv_mem_access_type type; /* output */
};

struct devmm_mem_export_para {
    int side;                                 /* input */
    int id;                                 /* input */
    int share_id;                           /* output */
};

struct devmm_mem_import_para {
    int share_id;                           /* input */
    uint32_t share_sdid;                    /* input */
    uint32_t share_devid;                   /* input */
    uint32_t share_phy_devid;               /* input */
    drv_mem_handle_type handle_type;        /* input */

    int id;                                 /* output */
    uint32_t side;                          /* output */
    uint32_t module_id;                     /* output */
    uint32_t pg_type;                       /* output */
    uint64_t pg_num;                        /* output */
    uint32_t mem_type;                      /* output */
};

struct devmm_mem_set_pid_para {
    int share_id;
    uint32_t pid_num;
    int *pid_list;
};

struct devmm_mem_set_attr_para {
    int share_id;                /* input */
    uint32_t type;               /* input */
    struct ShareHandleAttr attr; /* input */
};

struct devmm_mem_get_attr_para {
    int share_id;                /* input */
    uint32_t share_devid;        /* input */
    uint32_t type;               /* input */
    struct ShareHandleAttr attr; /* output */
};

struct devmm_mem_get_info_para {
    int share_id;                    /* input */
    uint32_t share_devid;            /* input */
    struct ShareHandleGetInfo info;  /* output */
};

struct devmm_mem_repair_para {
    uint32_t count;
    struct MemRepairAddr repair_addrs[MEM_REPAIR_MAX_CNT];
};

struct devmm_resv_addr_info_query_para {
    uint64_t va;            /* input */
    uint64_t start;         /* output */
    uint64_t end;           /* output */
    uint32_t module_id;     /* output */
    uint32_t devid;         /* output */
};

struct devmm_ioctl_arg {
    struct devmm_devid head;
    union {
        struct devmm_setup_dev_para setup_dev_para;
        struct devmm_mem_alloc_host_para alloc_para;
        struct devmm_mem_free_host_para free_para;
        struct devmm_mem_alloc_para alloc_svm_para;
        struct devmm_mem_copy_para copy_para;
        struct devmm_mem_copy_batch_para copy_batch_para;
        struct devmm_mem_copy2d_para copy2d_para;
        struct devmm_mem_async_copy_para async_copy_para;
        struct devmm_mem_convert_copy_para convert_copy_para;
        struct devmm_mem_convrt_addr_para convrt_para;
        struct devmm_mem_destroy_addr_para desty_para;
        struct devmm_destroy_addr_batch_para destroy_batch_para;
        struct devmm_mem_advise_para advise_para;
        struct devmm_mem_advise_para prefetch_para;
        struct devmm_mem_memset_para memset_para;
        struct devmm_mem_translate_para translate_para;
        struct devmm_mem_ipc_open_para ipc_open_para;
        struct devmm_mem_ipc_close_para ipc_close_para;

        struct devmm_mem_ipc_create_para ipc_create_para;
        struct devmm_mem_ipc_set_pid_para ipc_set_pid_para;
        struct devmm_mem_ipc_destroy_para ipc_destroy_para;
        struct devmm_mem_ipc_set_attr_para ipc_set_attr_para;
        struct devmm_mem_ipc_get_attr_para ipc_get_attr_para;
        struct devmm_mem_query_size_para query_size_para;

        struct devmm_init_process_para init_process_para;
        struct devmm_update_heap_para update_heap_para;
        struct devmm_query_process_status_para query_process_status_para;
        struct devmm_free_pages_para free_pages_para;
        struct devmm_dev_init_process_para dev_init_process_para;
        struct devmm_dev_bind_sibling_para dev_bind_sibling_para;

        struct devmm_status_va_info_para status_va_info_para;
        struct devmm_status_va_check_para status_va_check_para;
        struct devmm_query_device_mem_usedinfo query_device_mem_usedinfo_para;

        struct devmm_lock_cmd lock_cmd_para;

        struct devmm_program_load_cmd program_load_cmd;
        struct devmm_set_read_count set_read_count;
        struct devmm_dbg_info dbg_info;
        struct devmm_mem_remote_map_para map_para;
        struct devmm_mem_remote_unmap_para unmap_para;
        struct devmm_mem_map_cap_para mem_map_cap_para;
        struct devmm_register_dma_para register_dma_para;
        struct devmm_unregister_dma_para unregister_dma_para;
        struct devmm_check_mem_info check_meminfo_para;

        struct devmm_map_dev_reserve_para map_dev_reserve_para;
        struct devmm_get_mmap_para mmap_para;

        struct devmm_mem_create_para mem_create_para;
        struct devmm_mem_release_para mem_release_para;

        struct devmm_mem_map_para mem_map_para;
        struct devmm_mem_unmap_para mem_unmap_para;
        struct devmm_mem_query_owner_para mem_query_owner_para;
        struct devmm_mem_set_access_para mem_set_access_para;
        struct devmm_mem_get_access_para mem_get_access_para;

        struct devmm_mem_export_para mem_export_para;
        struct devmm_mem_import_para mem_import_para;
        struct devmm_mem_set_pid_para mem_set_pid_para;
        struct devmm_mem_set_attr_para mem_set_attr_para;
        struct devmm_mem_get_attr_para mem_get_attr_para;
        struct devmm_mem_get_info_para mem_get_info_para;

        struct devmm_mem_repair_para mem_repair_para;
        struct devmm_resv_addr_info_query_para resv_addr_info_query_para;
    } data;
};

#define DEVMM_SVM_MEM_QUERY_OWNER           _IOWR(DEVMM_SVM_MAGIC, 0, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_SET_ACCESS            _IOW(DEVMM_SVM_MAGIC, 1, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_GET_ACCESS            _IOWR(DEVMM_SVM_MAGIC, 2, struct devmm_ioctl_arg)

#define DEVMM_SVM_ALLOC                     _IOW(DEVMM_SVM_MAGIC, 3, struct devmm_ioctl_arg)
#define DEVMM_SVM_FREE_PAGES                _IOW(DEVMM_SVM_MAGIC, 4, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEMCPY                    _IOWR(DEVMM_SVM_MAGIC, 5, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEMCPY2D                  _IOWR(DEVMM_SVM_MAGIC, 6, struct devmm_ioctl_arg)
#define DEVMM_SVM_ASYNC_MEMCPY              _IOWR(DEVMM_SVM_MAGIC, 7, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEMCPY_RESLUT_REFRESH     _IOWR(DEVMM_SVM_MAGIC, 8, struct devmm_ioctl_arg)
#define DEVMM_SVM_SUMBIT_CONVERT_CPY        _IOW(DEVMM_SVM_MAGIC, 9, struct devmm_ioctl_arg)
#define DEVMM_SVM_WAIT_CONVERT_CPY_RESLUT   _IOW(DEVMM_SVM_MAGIC, 10, struct devmm_ioctl_arg)
#define DEVMM_SVM_CONVERT_ADDR              _IOWR(DEVMM_SVM_MAGIC, 11, struct devmm_ioctl_arg)
#define DEVMM_SVM_DESTROY_ADDR              _IOW(DEVMM_SVM_MAGIC, 12, struct devmm_ioctl_arg)
#define DEVMM_SVM_ADVISE                    _IOW(DEVMM_SVM_MAGIC, 13, struct devmm_ioctl_arg)
#define DEVMM_SVM_PREFETCH                  _IOW(DEVMM_SVM_MAGIC, 14, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEMSET8                   _IOWR(DEVMM_SVM_MAGIC, 15, struct devmm_ioctl_arg)
#define DEVMM_SVM_DESTROY_ADDR_BATCH        _IOW(DEVMM_SVM_MAGIC, 16, struct devmm_ioctl_arg)
#define DEVMM_SVM_TRANSLATE                 _IOWR(DEVMM_SVM_MAGIC, 17, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_MAP_CAP               _IOWR(DEVMM_SVM_MAGIC, 18, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_OPEN              _IOW(DEVMM_SVM_MAGIC, 21, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_CLOSE             _IOW(DEVMM_SVM_MAGIC, 22, struct devmm_ioctl_arg)
#define DEVMM_SVM_SETUP_DEVICE              _IOWR(DEVMM_SVM_MAGIC, 26, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_CREATE            _IOWR(DEVMM_SVM_MAGIC, 27, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_DESTROY           _IOW(DEVMM_SVM_MAGIC, 28, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_QUERY             _IOWR(DEVMM_SVM_MAGIC, 29, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_SET_PID           _IOW(DEVMM_SVM_MAGIC, 32, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_SET_PID_POD       _IOW(DEVMM_SVM_MAGIC, 33, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_SET_ATTR          _IOW(DEVMM_SVM_MAGIC, 34, struct devmm_ioctl_arg)
#define DEVMM_SVM_IPC_MEM_GET_ATTR          _IOWR(DEVMM_SVM_MAGIC, 35, struct devmm_ioctl_arg)
#define DEVMM_SVM_INIT_PROCESS              _IOWR(DEVMM_SVM_MAGIC, 36, struct devmm_ioctl_arg)
#define DEVMM_SVM_UPDATE_HEAP               _IOW(DEVMM_SVM_MAGIC, 37, struct devmm_ioctl_arg)
#define DEVMM_SVM_GET_PROC_STATUS           _IOW(DEVMM_SVM_MAGIC, 38, struct devmm_ioctl_arg)
#define DEVMM_SVM_PROCESS_STATUS_QUERY      _IOWR(DEVMM_SVM_MAGIC, 39, struct devmm_ioctl_arg)
#define DEVMM_SVM_DBG_VA_STATUS             _IOWR(DEVMM_SVM_MAGIC, 41, struct devmm_ioctl_arg)
#define DEVMM_SVM_CLOSE_DEVICE              _IOW(DEVMM_SVM_MAGIC, 42, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_REMOTE_MAP            _IOWR(DEVMM_SVM_MAGIC, 45, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_REMOTE_UNMAP          _IOWR(DEVMM_SVM_MAGIC, 46, struct devmm_ioctl_arg)
#define DEVMM_SVM_QUERY_MEM_USEDINFO        _IOWR(DEVMM_SVM_MAGIC, 47, struct devmm_ioctl_arg)
#define DEVMM_SVM_CHECK_MEMINFO             _IOW(DEVMM_SVM_MAGIC, 48, struct devmm_ioctl_arg)
#define DEVMM_SVM_MAP_DEV_RESERVE           _IOWR(DEVMM_SVM_MAGIC, 49, struct devmm_ioctl_arg)

#define DEVMM_SVM_MEM_CREATE                _IOWR(DEVMM_SVM_MAGIC, 50, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_RELEASE               _IOWR(DEVMM_SVM_MAGIC, 51, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_MAP                   _IOW(DEVMM_SVM_MAGIC, 52, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_UNMAP                 _IOWR(DEVMM_SVM_MAGIC, 53, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_EXPORT                _IOWR(DEVMM_SVM_MAGIC, 54, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_IMPORT                _IOWR(DEVMM_SVM_MAGIC, 55, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_SET_PID               _IOW(DEVMM_SVM_MAGIC, 56, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_SET_ATTR              _IOW(DEVMM_SVM_MAGIC, 57, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_GET_ATTR              _IOWR(DEVMM_SVM_MAGIC, 58, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_GET_INFO              _IOWR(DEVMM_SVM_MAGIC, 59, struct devmm_ioctl_arg)

#define DEVMM_SVM_REGISTER_DMA              _IOW(DEVMM_SVM_MAGIC, 60, struct devmm_ioctl_arg)
#define DEVMM_SVM_UNREGISTER_DMA            _IOW(DEVMM_SVM_MAGIC, 61, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEM_REPLAIR               _IOW(DEVMM_SVM_MAGIC, 62, struct devmm_ioctl_arg)
#define DEVMM_SVM_RESERVE_ADDR_INFO_QUERY   _IOWR(DEVMM_SVM_MAGIC, 63, struct devmm_ioctl_arg)
#define DEVMM_SVM_PREPARE_CLOSE_DEVICE      _IOW(DEVMM_SVM_MAGIC, 64, struct devmm_ioctl_arg)
#define DEVMM_SVM_MEMCPY_BATCH              _IOW(DEVMM_SVM_MAGIC, 65, struct devmm_ioctl_arg)

#define DEVMM_SVM_CMD_USE_PRIVATE_MAX_CMD   66 /* above this svm process must inited */

#define DEVMM_SVM_ALLOC_PROC_STRUCT         _IOW(DEVMM_SVM_MAGIC, 67, struct devmm_ioctl_arg)
#define DEVMM_SVM_DEV_SET_SIBLING           _IOW(DEVMM_SVM_MAGIC, 68, struct devmm_ioctl_arg)
#define DEVMM_SVM_DEV_BIND_SIBLING          _IOW(DEVMM_SVM_MAGIC, 69, struct devmm_ioctl_arg)
#define DEVMM_SVM_GET_MMAP_INFO             _IOWR(DEVMM_SVM_MAGIC, 70, struct devmm_ioctl_arg)

#define DEVMM_SVM_CMD_MAX_CMD               72     /* max cmd id */

#define DEVMM_SVM_GET_DEVPID_BY_HOSTPID 0 /* define for dbg server compile */
#define DEVMM_SVM_WAIT_DEVICE_PROCESS   0 /* define for dbg server compile */

#endif

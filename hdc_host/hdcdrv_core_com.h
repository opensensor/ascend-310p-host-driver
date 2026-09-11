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

#ifndef _HDCDRV_CORE_COM_H_
#define _HDCDRV_CORE_COM_H_

#include "hdcdrv_cmd.h"
#include "hdcdrv_log.h"
#include "hdcdrv_mem_com.h"

#include "dbl/ka_memory.h"
#include "ascend_hal_define.h"

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_fs_pub.h"

/*
    KA_SUB_MODULE_TYPE_0: for init/uninit when probe/remove or instance/uninstance
    KA_SUB_MODULE_TYPE_1: for normal channel, session link, process open/release
    KA_SUB_MODULE_TYPE_2: for fast channel
    KA_SUB_MODULE_TYPE_3: for epoll
    KA_SUB_MODULE_TYPE_4: for sysfs
*/
#define hdcdrv_kvzalloc(size, flags, level) ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_ka_kvfree(addr, level) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_kmalloc(size, flags, level) ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_kzalloc(size, flags, level) ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_kfree(addr, level) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

// replace vzalloc
#define hdcdrv_vzalloc(size, level)  ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
// replace vmalloc
#define hdcdrv_vmalloc(size, gfp_mask, prot, level) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_vfree(addr, level) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_alloc_pages(gfp_mask, order, level) \
    ka_alloc_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_free_pages(addr, order, level) \
    ka_free_pages(addr, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_alloc_pages_node_ex(nid, gfp_mask, order, level) \
    ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_free_pages_ex(page, order, level) \
    __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_kzalloc_node(size, flags, node, level) \
    ka_kzalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
// use hdcdrv_kfree for hdcdrv_kzalloc_node

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

#define hdcdrv_err_limit_spinlock(fmt, ...)
#define hdcdrv_warn_limit_spinlock(fmt, ...)
#define hdcdrv_info_limit_spinlock(fmt, ...)
#define hdcdrv_err_spinlock(fmt, ...)
#define hdcdrv_warn_spinlock(fmt, ...)
#define hdcdrv_info_spinlock(fmt, ...)
#define hdcdrv_critical_info_spinlock(fmt, ...)
#define hdcdrv_dbg_spinlock(fmt, ...)

#ifdef HDC_ENV_DEVICE
#define HDCDRV_DEV_MODE 0600
#else
#define HDCDRV_DEV_MODE 0640
#endif

#define HDCDRV_VALID 0xABCD6842U
#define HDCDRV_INVALID 0
#define HDCDRV_HOTRESET_FLAG_SET 1
#define HDCDRV_HOTRESET_FLAG_UNSET 0

#define HDCDRV_CDEV_COUNT   1
#define HDCDRV_LISTEN_STATUS_IDLE 2  // mdc sense wake up accept wait

#define HDCDRV_MEM_POOL_TYPE_NUM 2 /* TX or RX */

#define HDCDRV_MEM_POOL_TYPE_TX 0
#define HDCDRV_MEM_POOL_TYPE_RX 1
#define HDCDRV_RESERVE_MEM_POOL_TYPE_TX 2
#define HDCDRV_RESERVE_MEM_POOL_TYPE_RX 3

#define HDCDRV_PACKET_SEGMENT (32 * 1024)
#define HDCDRV_INVALID_PACKET_SEGMENT (-1)
#define HDCDRV_INVALID_HDC_VERSION (-1)

#define HDCDRV_RBTREE_PID 16
#define HDCDRV_RBTREE_CNT_BIT 24
#define HDCDRV_RBTREE_PID_MASK 0x000000000000FFFFull
#define HDCDRV_RBTREE_ADDR_MASK_H 0x0000FFFFFFFFFFFFull
#define HDCDRV_RBTREE_ADDR_MASK_L 0xFFFFFFFFFFFF0000ull

#define HDCDRV_SMALL_PACKET_SEGMENT (4 * 1024)
#define HDCDRV_HUGE_PACKET_SEGMENT (512 * 1024)
#ifdef CFG_FEATURE_MIRROR
#define HDCDRV_SMALL_PACKET_NUM 512 /* power of 2 */
#else
#define HDCDRV_SMALL_PACKET_NUM 1024U /* power of 2 */
#endif

#ifdef CFG_FEATURE_HDC_REG_MEM
# define HDCDRV_HUGE_PACKET_NUM 2U    /* power of 2 */
#else
#ifdef CFG_FEATURE_SMALL_HUGE_POOL
# define HDCDRV_HUGE_PACKET_NUM 16    /* power of 2 */
#else
# define HDCDRV_HUGE_PACKET_NUM 64    /* power of 2 */
#endif
#endif

#define HDCDRV_RESERVE_HUGE_PACKET_NUM 4
#define HDCDRV_VF_RESERVE_HUGE_PACKET_NUM 2

#define HDCDRV_USLEEP_RANGE_2000 2000
#define HDCDRV_USLEEP_RANGE_3000 3000

#define HDCDRV_LIST_CACHE   32

/*
 * 5a5a is magic
 * For example, add a new version number
 * #define HDC_VERSION_0002 0x5a5a0002u
 * #define HDC_VERSION HDC_VERSION_0002
 */
#define HDC_VERSION_0000 0
#define HDC_VERSION_0001 0x5a5a0001u    // HDC support mutiply pkt recv
#define HDC_VERSION_0002 0x5a5a0002u    // HDC support service query session dfx
#define HDC_VERSION HDC_VERSION_0002    // HDC Current Version

extern u32 hdcdrv_cmd_size_table[HDCDRV_CMD_MAX];

struct hdcdrv_cdev {
    ka_cdev_t cdev;
    ka_dev_t dev_no;
    ka_class_t *cdev_class;
    ka_device_t *dev;
};

enum VHDC_CTRL_MSG_TYPE {
    VHDC_CTRL_MSG_TYPE_SEGMENT = 1,
    VHDC_CTRL_MSG_TYPE_OPEN,
    VHDC_CTRL_MSG_TYPE_RELEASE,
    VHDC_CTRL_MSG_TYPE_ALLOC_MEM,
    VHDC_CTRL_MSG_TYPE_FREE_MEM,
    VHDC_CTRL_MSG_TYPE_ALLOC_HUGE,
    VHDC_CTRL_MSG_TYPE_FREE_HUGE,
    VHDC_CTRL_MSG_TYPE_MEMCOPY,
    VHDC_CTRL_MSG_TYPE_POOL_CHECK,
    VHDC_CTRL_MSG_TYPE_HDC_VERSION,
    VHDC_CTRL_MSG_TYPE_MAX
};

struct vhdc_ctrl_msg_get_segment {
    int segment;
};

struct vhdc_ctrl_msg_open {
};

struct vhdc_ctrl_msg_release {
    unsigned long long hash;
};

struct vhdca_alloc_mem_para {
    int pool_type;
    int dev_id;
    int len;
    u32 fid;
};

struct vhdc_ctrl_msg_alloc_mempool {
    struct vhdca_alloc_mem_para mem_para;
    void *buf;
    u64 addr;
};

struct vhdc_ctrl_msg_free_mempool {
    void *buf;
};

struct vhdc_ctrl_msg_alloc_huge {
};
struct vhdc_ctrl_msg_free_huge {
};

struct vhdc_ctrl_msg_memcopy {
    void *dest;
    unsigned long destMax;
    const void *src;
    unsigned long count;
    int mode;
};

struct vhdc_ctrl_msg_pool_check {
    u32 size;
    u32 segment;
    u32 sg_cnt;
    ka_dma_addr_t addr[HDCDRV_HUGE_PACKET_NUM];
    unsigned char map[HDCDRV_HUGE_PACKET_NUM];
};

struct vhdc_ctrl_msg_hdc_version {
    int pm_version;
    int vm_version;
};

struct vhdc_ctrl_msg {
    enum VHDC_CTRL_MSG_TYPE type;
    int error_code;
    union {
        struct vhdc_ctrl_msg_get_segment vhdc_segment;
        struct vhdc_ctrl_msg_open vhdc_open;
        struct vhdc_ctrl_msg_release vhdc_release;
        struct vhdc_ctrl_msg_alloc_mempool alloc_mempool;
        struct vhdc_ctrl_msg_free_mempool free_mempool;
        struct vhdc_ctrl_msg_alloc_huge alloc_huge;
        struct vhdc_ctrl_msg_free_huge free_huge;
        struct vhdc_ctrl_msg_memcopy mem_copy;
        struct vhdc_ctrl_msg_pool_check pool_check;
        struct vhdc_ctrl_msg_hdc_version hdc_version;
    };
};

struct vhdc_ioctl_msg {
    unsigned int cmd;
    unsigned long long hash;
    int copy_flag;
    int error_code;
    union hdcdrv_cmd cmd_data;
};

struct hdcdrv_dma_mem {
    u64 addr;
    u32 len;
    u32 resv;
};

struct hdcdrv_ctrl_msg_sync_mem_info {
    int type;
    int error_code;
    int flag;
    int phy_addr_num;
    int mem_type;
    u32 alloc_len;
    u64 hash_va;
    long long pid;
#ifdef CFG_FEATURE_HDC_REG_MEM
    u32 align_size;
    u32 register_offset;
    u64 user_va;
#endif
    struct hdcdrv_dma_mem mem[];
};

struct hdc_hotreset_task_info {
    int dev_id;
    int hdc_valid;
    int hotreset_flag;
    int msg_chan_refcnt;
    ka_task_spinlock_t task_rw_lock;
};

/* ctrl msg type */
#define HDCDRV_CTRL_MSG_TYPE_CONNECT 1U
#define HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY 2
#define HDCDRV_CTRL_MSG_TYPE_CLOSE 3
#define HDCDRV_CTRL_MSG_TYPE_SYNC 4
#define HDCDRV_CTRL_MSG_TYPE_RESET 5
#define HDCDRV_CTRL_MSG_TYPE_CHAN_SET 6
#define HDCDRV_CTRL_MSG_TYPE_SYNC_MEM_INFO 7
#define HDCDRV_CTRL_MSG_TYPE_GET_DEV_SESSION_STAT 8
#define HDCDRV_CTRL_MSG_TYPE_GET_DEV_CHAN_STAT 9
#define HDCDRV_CTRL_MSG_TYPE_GET_DEV_LINK_STAT 10

/* ctrl msg status */
enum HDC_LINK_CTRL_MSG_STATUS_TYPE {
    HDCDRV_LINK_CTRL_MSG_SEND_SUCC = 0,
    HDCDRV_LINK_CTRL_MSG_SEND_FAIL = 1,
    HDCDRV_LINK_CTRL_MSG_RECV_SUCC = 2,
    HDCDRV_LINK_CTRL_MSG_RECV_FAIL = 3,
    HDCDRV_LINK_CTRL_MSG_WAIT_SUCC = 4,
    HDCDRV_LINK_CTRL_MSG_WAIT_TIMEOUT = 5,
    HDCDRV_LINK_CTRL_MSG_STATUS_MAX
};

struct hdcdrv_link_ctrl_msg_stats {
    u32 count[HDCDRV_LINK_CTRL_MSG_STATUS_MAX];
    int last_err[HDCDRV_LINK_CTRL_MSG_STATUS_MAX];
};

#ifdef CFG_FEATURE_SIMPLIFY_MODE
#define HDCDRV_NON_TRANS_MSG_S_DESC_SIZE 0x10000 /* 64k, pcie support */
#define HDCDRV_NON_TRANS_MSG_C_DESC_SIZE 0x10000 /* 64k, pcie support */
#else
#define HDCDRV_NON_TRANS_MSG_S_DESC_SIZE 0x100000 /* 1M */
#define HDCDRV_NON_TRANS_MSG_C_DESC_SIZE 0x100000 /* 1M */
#endif

/* reserved 1k mem for non trans msg head and hdc ctrl msg head */
#define HDCDRV_MAX_DMA_NODE ((int)((HDCDRV_NON_TRANS_MSG_S_DESC_SIZE - 0x400) / sizeof(struct hdcdrv_dma_mem)))
#define HDCDRV_CTRL_MEM_MAX_PHY_NUM (HDCDRV_CTRL_MEM_MAX_LEN / HDCDRV_MEM_MIN_LEN)
#define HDCDRV_MEM_MAX_PHY_NUM (HDCDRV_MAX_DMA_NODE - HDCDRV_CTRL_MEM_MAX_PHY_NUM)

#define HDCDRV_NODE_IDLE 0
#define HDCDRV_NODE_BUSY 1
#define HDCDRV_ADD_FLAG 0
#define HDCDRV_DEL_FLAG 1
#define HDCDRV_ADD_REGISTER_FLAG 2
#define HDCDRV_DEL_REGISTER_FLAG 3
#define HDCDRV_FALSE_FLAG 0
#define HDCDRV_TRUE_FLAG 1

#define HDCDRV_RBTREE_SIDE_LOCAL 0
#define HDCDRV_RBTREE_SIDE_REMOTE 1

#define HDCDRV_RUNNING_ENV_ARM_3559 0
#define HDCDRV_RUNNING_ENV_X86_NORMAL 1
#define HDCDRV_RUNNING_ENV_DAVICI 2

#define HDCDRV_KERNEL_DEFAULT_PID 0x7FFFFFFFULL
#define HDCDRV_INVALID_PID 0x7FFFFFFEULL
#define HDCDRV_RAW_PID_MASK 0xFFFFFFFFULL
#define HDCDRV_KERNEL_DEFAULT_START_TIME 0ULL

#define HDCDRV_LINK_NORMAL 0
#define HDCDRV_PCIE_LINK_DOWN 1
#define HDCDRV_PCIE_DISCONNECT 2
#define HDCDRV_HDC_DISCONNECT 3

#define HDCDRV_NODE_WAIT_FREE_MAX_TIMES (20U)
#define HDCDRV_NODE_WAIT_FREE_MS (5U)  /* 5ms */

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define HDCDRV_NODE_BUSY_WARING     2000 /* 2s for fpga */
#define HDCDRV_NODE_BUSY_TIMEOUT    50000 /* 50s for fpga */

#define HDCDRV_NODE_WAIT_TIME_MIN   0
#define HDCDRV_NODE_WAIT_TIME_MAX   30000 /* 30s for fpga */
#define HDCDRV_NODE_RELEASE_TIME_MAX 100 /* ms */

#define HDCDRV_SESSION_DEFAULT_TIMEOUT 120000 /* 120s for fpga */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT (10 * KA_HZ) /* 10s for fpga */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT_MS 10000 /* 10s for fpga */
#define HDCDRV_CONN_TIMEOUT (100 * KA_HZ) /* 100s for fpga */
#define HDCDRV_SESSION_RECLAIM_TIMEOUT (150 * KA_HZ) /* 150s for fpga */

#define HDCDRV_TASKLET_STATUS_CHECK_TIME 10 /* 10s for fpga */
#else
#define HDCDRV_NODE_BUSY_WARING     100 /* ms */
#define HDCDRV_NODE_BUSY_TIMEOUT    5000 /* ms */

#define HDCDRV_NODE_WAIT_TIME_MIN   0
#define HDCDRV_NODE_WAIT_TIME_MAX   3000 /* ms */

#define HDCDRV_NODE_FREE_WAIT_TIME  1500 /* ms */
#define HDCDRV_NODE_FREE_SLEEP_TIME 1000 /* us */
#define HDCDRV_NODE_FREE_SLEEP_RANGE 10 /* us */
#define HDCDRV_NODE_RELEASE_TIME_MAX 100 /* ms */

#define HDCDRV_SESSION_DEFAULT_TIMEOUT 3000 /* ms */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT (3 * KA_HZ) /* 3s */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT_MS 3000 /* 3s */
#define HDCDRV_CONN_TIMEOUT (30 * KA_HZ) /* 30s */
#define HDCDRV_SESSION_RECLAIM_TIMEOUT (60 * KA_HZ) /* 60s */

#define HDCDRV_TASKLET_STATUS_CHECK_TIME 3
#endif
#define HDCDRV_CONN_MIN_TIMEOUT (1 * KA_HZ) /* 1s */

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

extern u64 hdcdrv_get_hash(u64 user_va, u64 pid, u32 fid);
extern u32 hdcdrv_get_mem_list_len(void);
extern void hdcdrv_node_status_init(struct hdcdrv_fast_node *node);
extern void hdcdrv_node_status_busy(struct hdcdrv_fast_node *node);
extern void hdcdrv_node_status_idle(struct hdcdrv_fast_node *node);
extern void hdcdrv_node_status_idle_by_mem(struct hdcdrv_fast_mem *f_mem);
extern bool hdcdrv_node_is_timeout(int node_stamp);
extern bool hdcdrv_node_is_busy(const struct hdcdrv_fast_node *node);
extern int hdcdrv_send_mem_info(struct hdcdrv_fast_mem *mem, int devid, int flag);
extern int hdcdrv_get_running_env(void);
extern void hdcdrv_unbind_mem_ctx(struct hdcdrv_fast_node *f_node);
extern long hdcdrv_non_trans_ctrl_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);

int hdccom_register_cdev(struct hdcdrv_cdev *hcdev, const ka_file_operations_t *fops);
void hdccom_free_cdev(struct hdcdrv_cdev *hcdev);
u64 hdcdrv_get_pid(void);
u64 hdcdrv_get_ppid(void);
int hdcdrv_rebuild_raw_pid(u64 pid);
ka_mutex_t *hdcdrv_get_sync_mem_lock(int dev_id);
extern bool hdcdrv_mem_is_notify(const struct hdcdrv_fast_mem *f_mem);
void* hdcdrv_get_sync_mem_buf(int dev_id);
void hdcdrv_release_free_mem(struct hdcdrv_ctx_fmem *ctx_fmem);
void hdcdrv_release_unmap_failed_fast_mem(struct hdcdrv_ctx_fmem *ctx_fmem);
extern void hdcdrv_fast_mem_free_abnormal(const struct hdcdrv_mem_node_info *f_info);
void hdcdrv_fast_mem_quick_proc(const struct hdcdrv_mem_node_info *f_info);
void hdcdrv_bind_mem_ctx(struct hdcdrv_ctx_fmem *ctx_fmem, struct hdcdrv_fast_node *f_node,
                         struct hdcdrv_mem_fd_list *new_node);
void hdcdrv_add_to_async_ctx(struct hdcdrv_ctx_fmem *async_ctx, struct hdcdrv_fast_node *f_node);
int hdcdrv_mmap_param_check(ka_file_t *filep, ka_vm_area_struct_t *vma);
struct hdcdrv_mem_fd_list *hdcdrv_release_get_free_mem_entry(struct hdcdrv_ctx_fmem *ctx_fmem);

ka_page_t *hdcdrv_alloc_pages_node(u32 dev_id, ka_gfp_t gfp_mask, u32 order);
void *hdcdrv_kzalloc_mem_node(u32 dev_id, ka_gfp_t gfp_mask, u32 size, u32 level);
int hdcdrv_get_running_status(void);
u64 hdcdrv_get_task_start_time(void);

void hdcdrv_peer_status_init(void);
int hdcdrv_get_peer_status(void);
void hdcdrv_set_peer_status(u32 status);
void hdcdrv_set_quice_release_flag(int *flag);
bool hdcdrv_release_is_quick(int flag);

#endif /* _HDCDRV_CORE_COM_H_ */

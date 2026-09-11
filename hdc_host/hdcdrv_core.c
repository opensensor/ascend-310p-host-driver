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

#include "kernel_version_adapt.h"
#include "hdcdrv_sysfs.h"
#include "hdcdrv_interface.h"
#include "davinci_interface.h"
#include "hdcdrv_core.h"
#ifdef CFG_FEATURE_HDC_REG_MEM
#include "hdcdrv_status.h"
#endif
#ifdef CFG_FEATURE_PFSTAT
#include "hdcdrv_pfstat.h"
#endif

#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_driver_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_barrier_pub.h"
#include "ka_fs_pub.h"
#include "ka_compiler_pub.h"

struct hdcdrv_ctrl *hdc_ctrl = NULL;
struct hdcdrv_node_tree_ctrl *hdc_node_tree = NULL;

struct hdcdrv_session_notify g_session_notify[HDCDRV_SUPPORT_MAX_SERVICE];

u32 hdcdrv_dev_num = 0;
report_process_status hdcdrv_report_process_status_func = NULL;
bandwidth_limit_check hdcdrv_bandwidth_limit_check_func = NULL;

ka_vm_operations_struct_t hdcdrv_vm_ops_managed;

STATIC int g_rx_notify_sched_mode = HDCDRV_RX_SCHED_TASKLET;

struct hdcdrv_register_symbol g_hdcdrv_register_symbol = {.module_ptr = NULL, .wake_up_context_status = NULL};
ka_rw_semaphore_t g_symbol_lock;

struct hdcdrv_peer_st_ctrl g_peer_status;
STATIC ka_workqueue_struct_t *async_release_workqueue = NULL;
static struct {
    char str[HDCDRV_STR_NAME_LEN];
} hdcdrv_service_type_str[HDCDRV_SUPPORT_MAX_SERVICE + 1] = {
    { "service_dmp" },                      /* HDCDRV_SERVICE_TYPE_DMP */
    { "service_profiling" },                /* HDCDRV_SERVICE_TYPE_PROFILING */
    { "service_IDE1" },                     /* HDCDRV_SERVICE_TYPE_IDE1 */
    { "service_file_trans" },               /* HDCDRV_SERVICE_TYPE_FILE_TRANS */
    { "service_IDE2" },                     /* HDCDRV_SERVICE_TYPE_IDE2 */
    { "service_log" },                      /* HDCDRV_SERVICE_TYPE_LOG */
    { "service_rdma" },                     /* HDCDRV_SERVICE_TYPE_RDMA */
    { "service_bbox" },                     /* HDCDRV_SERVICE_TYPE_BBOX */
    { "service_framework" },                /* HDCDRV_SERVICE_TYPE_FRAMEWORK */
    { "service_TSD" },                      /* HDCDRV_SERVICE_TYPE_TSD */
    { "service_TDT" },                      /* HDCDRV_SERVICE_TYPE_TDT */
    { "service_PROF" },                     /* HDCDRV_SERVICE_TYPE_PROF */
    { "service_IDE_file_trans" },           /* HDCDRV_SERVICE_TYPE_IDE_FILE_TRANS */
    { "service_dump" },                     /* HDCDRV_SERVICE_TYPE_DUMP */
    { "service_user3" },                    /* HDCDRV_SERVICE_TYPE_USER3 */
    { "service_DVPP" },                     /* HDCDRV_SERVICE_TYPE_DVPP */
    { "service_queue" },                    /* HDCDRV_SERVICE_TYPE_QUEUE */
    { "service_upgrade" },                  /* HDCDRV_SERVICE_TYPE_UPGRADE */
    { "service_rdma_v2" },                  /* HDCDRV_SERVICE_TYPE_RDMA_V2 */
    { "service_test"},                      /* HDCDRV_SERVICE_TYPE_TEST */
    { "service_kms"},                       /* HDCDRV_SERVICE_TYPE_KMS */
};

#define HDC_CRC_CHECH_RETRY_CNT  5
#define CRC_POLYNOMIAL      0x1021
#define NULL_USHORT         0xFFFF
#define CRC_BITS_PER_BYTE        8
#define CRC_BIT15           0x8000
STATIC inline u32 hdcdrv_calculate_crc(unsigned char *data_head, u32 data_len)
{
    u16 val = NULL_USHORT;
    const u16 poly = CRC_POLYNOMIAL;
    uint8_t ch;
    int i;

    if (data_head == NULL) {
        return (u32)(NULL_USHORT);
    }

    while (data_len--) {
        ch = *(data_head++);
        val ^= (ch << CRC_BITS_PER_BYTE);
        for (i = 0; i < CRC_BITS_PER_BYTE; i++) {
            if (val & CRC_BIT15) {
                val = (val << 1) ^ poly;
            } else {
                val = val << 1;
            }
        }
    }

    return (u32)val;
}

void hdcdrv_peer_status_init(void)
{
    ka_task_rwlock_init(&g_peer_status.lock);
    g_peer_status.current_state = DEVDRV_PEER_STATUS_NORMAL;
}

int hdcdrv_get_peer_status(void)
{
    int status;
    ka_task_read_lock(&g_peer_status.lock);
    status = (int)g_peer_status.current_state;
    ka_task_read_unlock(&g_peer_status.lock);
    return status;
}

/* peer status transition machine
#              NORMAL
#               /  \
#              /    \
#             v      v
#         RESET-----> LINKDOWN
*/
void hdcdrv_set_peer_status(u32 status)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdcdrv_info("set hdc peer status. (curStat=%u, setStat=%u)\n",
                g_peer_status.current_state, status);
    ka_task_write_lock(&g_peer_status.lock);
    switch (status) {
        case DEVDRV_PEER_STATUS_RESET:
            if (g_peer_status.current_state != DEVDRV_PEER_STATUS_LINKDOWN) {
                g_peer_status.current_state = status;
            }
            break;
        case DEVDRV_PEER_STATUS_LINKDOWN:
#ifdef DRV_UT
        case DEVDRV_PEER_STATUS_NORMAL:
#endif
            g_peer_status.current_state = status;
            break;
        default:
            break;
    }
    ka_task_write_unlock(&g_peer_status.lock);
#endif
}

/*
  When local detect the peer reboot/panic fault, local should return an
  fault code agreed with hicaid, which is HDCDRV_SESSION_HAS_CLOSED. This
  function takes effect only in the MDC scenario.
*/
STATIC void hdcdrv_peer_fault_proc(u32 cmd, long *result)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    if (result == NULL) {
        return;
    }

    if ((hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) &&
        ((cmd == HDCDRV_CMD_SEND) || (cmd == HDCDRV_CMD_RECV) ||
         (cmd == HDCDRV_CMD_FAST_SEND) || (cmd == HDCDRV_CMD_FAST_RECV) ||
         (cmd == HDCDRV_CMD_WAIT_MEM) || (cmd == HDCDRV_CMD_RECV_PEEK) ||
         (cmd == HDCDRV_CMD_ACCEPT) || (cmd == HDCDRV_CMD_CONNECT))) {
        hdcdrv_warn_limit("peer status is abnormal, cmd cannot proc.(cmd=0x%x)\n", cmd);
        *result = HDCDRV_PEER_REBOOT;
    }
#endif
}
static void hdcdrv_fill_service_type_str(void)
{
    int i, ret;

    for (i = HDCDRV_SERVICE_TYPE_USER_START; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        ret = snprintf_s(hdcdrv_service_type_str[i].str, HDCDRV_STR_NAME_LEN, HDCDRV_STR_NAME_LEN - 1,
            "service_user_%d", i - HDCDRV_SERVICE_TYPE_USER_START);
        if (ret < 0) {
            hdcdrv_err("snprintf_s error, service %d can not print error string in log.\n",
                i - HDCDRV_SERVICE_TYPE_USER_START);
        }
    }
}

static struct {
    const char str[HDCDRV_STR_NAME_LEN];
} hdcdrv_close_state_str[HDCDRV_CLOSE_TYPE_MAX + 1] = {
    { "state_none" },                       /* HDCDRV_CLOSE_TYPE_NONE */
    { "closed_by_user" },                   /* HDCDRV_CLOSE_TYPE_USER */
    { "closed_by_user_form_kernel" },       /* HDCDRV_CLOSE_TYPE_KERNEL */
    { "closed_by_release" },                /* HDCDRV_CLOSE_TYPE_RELEASE */
    { "closed_by_set_owner_timeout" },      /* HDCDRV_CLOSE_TYPE_NOT_SET_OWNER */
    { "remote_close_post" },                /* HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST */
    { "state_max" }                         /* HDCDRV_CLOSE_TYPE_MAX */
};

void hdcdrv_get_mempool_size(u32 *small_packet_num, u32 *huge_packet_num)
{
#ifdef HDC_ENV_DEVICE
    *small_packet_num = HDCDRV_SMALL_PACKET_NUM;
    *huge_packet_num = HDCDRV_HUGE_PACKET_NUM;
#else
    u32 mempool_level = hdcdrv_get_hdc_mempool_level();
    if (mempool_level >= HDC_MEM_POOL_LEVEL_INVALID) {
        *small_packet_num = HDCDRV_SMALL_PACKET_NUM;
        *huge_packet_num = HDCDRV_HUGE_PACKET_NUM;
    } else {
       *small_packet_num = HDCDRV_SMALL_PACKET_NUM >> mempool_level;
       *huge_packet_num = HDCDRV_HUGE_PACKET_NUM >> mempool_level;
    }
#endif
}

/* only [vfio] and [mdev+sriov's vm] use workqueue, others use tasklet */
void hdcdrv_rx_msg_schedule_task(struct hdcdrv_msg_chan *msg_chan)
{
    if ((g_rx_notify_sched_mode == HDCDRV_RX_SCHED_WORK) || (devdrv_is_mdev_vm_boot_mode(msg_chan->dev_id) == true)) {
        ka_task_queue_work(msg_chan->rx_workqueue, &msg_chan->rx_notify_work);
    } else {
        ka_system_tasklet_schedule(&msg_chan->rx_notify_task);
    }
}

#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_VFIO_DEVICE)
STATIC int hdcdrv_bandwidth_limit_check(struct hdcdrv_session *session, u32 dir, u32 data_len, u32 node_cnt)
{
    struct vmng_bandwidth_check_info check_info;

    check_info.dir = dir;
    check_info.vfid = session->local_fid;
    check_info.dev_id = (u32)session->dev_id;
    check_info.data_len = data_len;
    check_info.node_cnt = node_cnt;

#ifdef CFG_FEATURE_VFIO
    check_info.handle_mode = VMNG_BW_BANDWIDTH_CHECK_SLEEPABLE;
    return vmng_bandwidth_limit_check(&check_info);
#else
    check_info.handle_mode = VMNG_BW_BANDWIDTH_CHECK_NON_SLEEP;
    return vmng_bandwidth_limit_check(&check_info);
#endif
}
#endif

STATIC u32 hdcdrv_gen_unique_value(void)
{
    u32 value;

    value = (u32)ka_base_atomic_inc_return(&hdc_ctrl->unique_val);
#ifdef HDC_ENV_DEVICE
    value = HDCDRV_SESSION_UNIQUE_VALUE_DEVICE_FLAG | (value & HDCDRV_SESSION_UNIQUE_VALUE_MASK);
#else
    value = HDCDRV_SESSION_UNIQUE_VALUE_HOST_FLAG | (value & HDCDRV_SESSION_UNIQUE_VALUE_MASK);
#endif
    return value;
}

STATIC int hdcdrv_session_pre_alloc(u32 dev_id, u32 fid, int service_type)
{
#ifdef CFG_FEATURE_VFIO
    if (fid != HDCDRV_DEFAULT_PM_FID) {
        return vhdch_session_pre_alloc(dev_id, fid, service_type);
    } else {
        return HDCDRV_OK;
    }
#else
    return HDCDRV_OK;
#endif
}

STATIC void hdcdrv_session_post_free(u32 dev_id, u32 fid, int service_type)
{
#ifdef CFG_FEATURE_VFIO
    if (fid != HDCDRV_DEFAULT_PM_FID) {
        vhdch_session_free(dev_id, fid, service_type);
    }
#endif
}

int hdcdrv_session_task_start_time_compare(const u64 proc_start_time, const u64 session_start_time)
{
    if (proc_start_time != session_start_time)  {
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

int hdcdrv_check_session_owner(const struct hdcdrv_session *session, u64 check_pid)
{
    u64 task_start_time = hdcdrv_get_task_start_time();

    if (session->owner_pid != check_pid) {
        return HDCDRV_NO_PERMISSION;
    }

    if (hdcdrv_session_task_start_time_compare(task_start_time, session->task_start_time)) {
        return HDCDRV_NO_PERMISSION;
    }

    return HDCDRV_OK;
}

STATIC const char *hdcdrv_sevice_str(int service_type)
{
    if ((service_type < HDCDRV_SERVICE_TYPE_DMP) || (service_type >= HDCDRV_SUPPORT_MAX_SERVICE)) {
        return "service_invaild";
    } else if ((service_type < HDCDRV_SERVICE_TYPE_USER_START) && (service_type >= HDCDRV_SERVICE_TYPE_APPLY_MAX)) {
        return "service_reserved";
    } else {
        return hdcdrv_service_type_str[service_type].str;
    }
}

STATIC const char *hdcdrv_close_str(int close_state)
{
    if ((close_state < HDCDRV_CLOSE_TYPE_NONE) || (close_state > HDCDRV_CLOSE_TYPE_MAX)) {
        return hdcdrv_close_state_str[HDCDRV_CLOSE_TYPE_MAX].str;
    } else {
        return hdcdrv_close_state_str[close_state].str;
    }
}

STATIC void hdcdrv_wakeup_record_resq_time(u32 dev_id, int service_type, u64 stamp_time, u64 timeout,
    const char *stamp_str)
{
    u64 resq_time;

    resq_time = ka_system_jiffies_to_msecs(ka_jiffies - stamp_time);
    if (resq_time > timeout) {
        hdcdrv_warn_limit("Stamp time. (devid=%u; stamp=%llu; service_type=\"%s\"; func=\"%s\")\n",
            dev_id, resq_time, hdcdrv_sevice_str(service_type), stamp_str);
    }
}

STATIC void hdcdrv_print_status(int service_type)
{
    u32 count[HDCDRV_SESSION_STATUS_MAX] = {0};
    struct hdcdrv_session *sessions = NULL;
    u32 session_cancel_not_clean = 0;
    u32 *server_count = NULL;
    u32 conn_cnt;
    int status;
    int idx;
    int i;

    server_count = (u32 *)hdcdrv_kvmalloc((int)sizeof(u32) * HDCDRV_SESSION_STATUS_MAX * HDCDRV_SUPPORT_MAX_SERVICE,
        KA_SUB_MODULE_TYPE_1);
    if (server_count == NULL) {
        return;
    }

    idx = hdc_ctrl->cur_alloc_session;
    for (i = 0; i < HDCDRV_REAL_MAX_SESSION; i++) {
        sessions = &hdc_ctrl->sessions[i];
        status = hdcdrv_get_session_status(sessions);
        if (status == HDCDRV_SESSION_STATUS_IDLE) {
            count[HDCDRV_SESSION_STATUS_IDLE]++;
            if (sessions->work_cancel_cnt != 0) {
                session_cancel_not_clean++;
            }
        } else if (status == HDCDRV_SESSION_STATUS_CONN) {
            count[HDCDRV_SESSION_STATUS_CONN]++;
            server_count[HDCDRV_SESSION_STATUS_CONN * HDCDRV_SUPPORT_MAX_SERVICE + sessions->service_type]++;
        } else if (status == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) {
            count[HDCDRV_SESSION_STATUS_REMOTE_CLOSED]++;
            server_count[HDCDRV_SESSION_STATUS_REMOTE_CLOSED * HDCDRV_SUPPORT_MAX_SERVICE + sessions->service_type]++;
        } else {
            count[HDCDRV_SESSION_STATUS_CLOSING]++;
            server_count[HDCDRV_SESSION_STATUS_CLOSING * HDCDRV_SUPPORT_MAX_SERVICE + sessions->service_type]++;
        }
    }

    for (i = 0; i < HDCDRV_SERVICE_TYPE_APPLY_MAX; i++) {
        conn_cnt = server_count[HDCDRV_SESSION_STATUS_CONN * HDCDRV_SUPPORT_MAX_SERVICE + i];
        if (conn_cnt < (HDCDRV_DFX_MAX_PROCESS_NUM * hdcdrv_dev_num)) {
            continue;
        }
        hdcdrv_warn_limit("Service:%s, conn cnt:%u; remote closing cnt:%u; closing cnt:%u\n", hdcdrv_sevice_str(i), 
            conn_cnt, server_count[HDCDRV_SESSION_STATUS_REMOTE_CLOSED * HDCDRV_SUPPORT_MAX_SERVICE + i],
            server_count[HDCDRV_SESSION_STATUS_CLOSING * HDCDRV_SUPPORT_MAX_SERVICE + i]);
    }

    hdcdrv_kvfree(server_count, KA_SUB_MODULE_TYPE_1);

    hdcdrv_warn_limit("No session, session info. (idx=%d; state_idle=%u; connected=%u; remote_close=%u; others=%u)\n",
        idx, count[HDCDRV_SESSION_STATUS_IDLE], count[HDCDRV_SESSION_STATUS_CONN],
        count[HDCDRV_SESSION_STATUS_REMOTE_CLOSED], count[HDCDRV_SESSION_STATUS_CLOSING]);
    if (session_cancel_not_clean != 0) {
        hdcdrv_warn_limit("session free, but not clean num: %d\n", session_cancel_not_clean);
    }
}

#ifdef CFG_FEATURE_MIRROR
int hdcdrv_alloc_mem_mirror(int pool_type, int dev_id, int len, struct hdcdrv_buf_desc *desc,
    ka_list_head_t *wait_head)
{
    int ret;
    struct mirror_alloc_para para;
    struct hdcdrv_session *session = NULL;
    int block_capacity = HDCDRV_HUGE_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE;

    if ((desc->local_session >= HDCDRV_REAL_MAX_SESSION) || (desc->local_session < 0)) {
        para.service_type = -1;
    } else {
        session = &hdc_ctrl->sessions[desc->local_session];
        para.service_type = session->service_type;
    }

    para.dev_id = dev_id;
    para.len = len;
    para.wait_head = wait_head;

    if (get_pool_type(len) == HDCDRV_USE_POOL) {
        ret = alloc_mem(pool_type, dev_id, len, &desc->buf, &desc->addr, wait_head);
    } else {
        if (ka_unlikely((len > block_capacity) || (len <= 0))) {
            hdcdrv_err_limit("data len invalid. (len=%d, capacity=%d)\n", len, block_capacity);
            return HDCDRV_ERR;
        }
        ret = alloc_mem_page(pool_type, para, &desc->buf, &desc->addr, &desc->mem_id);
        if ((ret == HDCDRV_MEM_ALLOC_FAIL) && (para.service_type == HDCDRV_SERVICE_TYPE_DMP)) {
            if (pool_type == HDCDRV_MEM_POOL_TYPE_TX) {
                pool_type = HDCDRV_RESERVE_MEM_POOL_TYPE_TX;
            } else {
                pool_type = HDCDRV_RESERVE_MEM_POOL_TYPE_RX;
            }
            ret = alloc_mem(pool_type, dev_id, len, &desc->buf, &desc->addr, wait_head);
        }
    }

    return ret;
}

void hdcdrv_free_mem_mirror(void* buf, int mem_id, int len)
{
    struct hdcdrv_mem_block_head *block_head = NULL;

    if (get_pool_type(len) == HDCDRV_USE_POOL) {
        free_mem(buf);
        return;
    }

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err_spinlock("Block head check failed.\n");
        return;
    }
    block_head = HDCDRV_BLOCK_HEAD(buf);
    if ((block_head->type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) ||
        (block_head->type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
        free_mem(buf);
    } else {
        free_mem_page(buf, mem_id);
    }
}
#endif

STATIC int hdcdrv_tx_alloc_mem(struct hdcdrv_session *session, const struct hdcdrv_cmd_send *cmd,
    struct hdcdrv_buf_desc *desc)
{
    int ret = HDCDRV_OK;

#ifdef CFG_FEATURE_VFIO
    struct hdccom_alloc_mem_para para = {0};

    if (session->owner == HDCDRV_SESSION_OWNER_VM) {
        if ((cmd->pool_buf == NULL) || (cmd->pool_addr == 0)) {
            hdcdrv_err("Input pararmeter is error.\n");
            return HDCDRV_PARA_ERR;
        }

        desc->buf = cmd->pool_buf;
        ret = hdcdrv_dma_map_guest_page((u32)session->dev_id, session->local_fid,
            cmd->pool_addr, (unsigned long)cmd->len, desc);
        return ret;
    }

    if (session->owner == HDCDRV_SESSION_OWNER_CT) {
        para.pool_type = HDCDRV_MEM_POOL_TYPE_TX;
        para.dev_id = session->dev_id;
        para.fid = session->local_fid;
        para.len = cmd->len;
        para.wait_head = NULL;
        ret = vhdch_alloc_mem_container(&para, desc);
        return ret;
    }
#endif

#ifdef CFG_FEATURE_MIRROR
    ret = hdcdrv_alloc_mem_mirror(HDCDRV_MEM_POOL_TYPE_TX, session->dev_id, cmd->len, desc, NULL);
#else
    ret = alloc_mem(HDCDRV_MEM_POOL_TYPE_TX, session->dev_id, cmd->len, &desc->buf, &desc->addr, NULL);
#endif
    return ret;
}

STATIC int hdcdrv_rx_alloc_mem(const struct hdcdrv_session *session, struct hdccom_alloc_mem_para *para,
    struct hdcdrv_buf_desc *desc)
{
    int ret;

#ifdef CFG_FEATURE_VFIO
    if (session->owner == HDCDRV_SESSION_OWNER_VM) {
        para->pool_type = HDCDRV_MEM_POOL_TYPE_RX;
        para->dev_id = session->dev_id;
        para->fid = session->local_fid;
        ret = vhdch_alloc_mem_vm(para, desc);
        return ret;
    }

    if (session->owner == HDCDRV_SESSION_OWNER_CT) {
        para->pool_type = HDCDRV_MEM_POOL_TYPE_RX;
        para->dev_id = session->dev_id;
        para->fid = session->local_fid;
        ret = vhdch_alloc_mem_container(para, desc);
        return ret;
    }
#endif

#ifdef CFG_FEATURE_MIRROR
    ret = hdcdrv_alloc_mem_mirror(HDCDRV_MEM_POOL_TYPE_RX, session->dev_id, para->len, desc, para->wait_head);
#else
    ret = alloc_mem(HDCDRV_MEM_POOL_TYPE_RX, session->dev_id, para->len,
        &desc->buf, &desc->addr, para->wait_head);
#endif
    return ret;
}

void hdcdrv_free_mem(struct hdcdrv_session *session, void *buf, int flag,
    struct hdcdrv_buf_desc *desc)
{
#ifdef CFG_FEATURE_VFIO
    struct hdcdrv_session_work_node *entry = NULL;

    if ((desc != NULL) && (desc->fid > HDCDRV_DEFAULT_PM_FID) && (session->owner == HDCDRV_SESSION_OWNER_VM)) {
        if (flag == HDCDRV_MEMPOOL_FREE_IN_PM) {
            entry = (struct hdcdrv_session_work_node *)hdcdrv_kvzalloc(sizeof(struct hdcdrv_session_work_node),
                KA_GFP_ATOMIC | __KA_GFP_ACCOUNT, KA_SUB_MODULE_TYPE_1);
            if (entry == NULL) {
                hdcdrv_err_spinlock("Calling malloc failed.\n");
                return;
            }

            entry->buf = buf;
            entry->dma_sgt = desc->dma_sgt;
            entry->dev_id = desc->dev_id;
            entry->fid = desc->fid;

            ka_task_spin_lock_bh(&session->session_work.lock);
            ka_list_add(&entry->list, &session->session_work.root_list);
            ka_task_spin_unlock_bh(&session->session_work.lock);

            ka_task_schedule_work(&session->session_work.swork);
        } else {
            /* memory pool buffer will free in VM : vhdca_top_half_free */
            hdcdrv_dma_unmap_guest_page(desc->dev_id, desc->fid, desc->dma_sgt);
        }
        return;
    }

    if (session->owner == HDCDRV_SESSION_OWNER_CT) {
        vhdch_free_mem_container((u32)session->dev_id, session->local_fid, session->chan_id, buf);
        return;
    }
#endif
#ifdef CFG_FEATURE_MIRROR
    hdcdrv_free_mem_mirror(buf, desc->mem_id, desc->len);
#else
    free_mem(buf);
#endif
}

STATIC void hdcdrv_desc_clear(struct hdcdrv_buf_desc *desc)
{
    if (desc == NULL) {
        return;
    }

    desc->dma_sgt = NULL;
    desc->dev_id = 0;
    desc->fid = 0;
    desc->buf = NULL;
    desc->addr = 0;
    desc->len = 0;
}

STATIC int hdcdrv_copy_from_user(struct hdcdrv_session *session, void *dest,
    const struct hdcdrv_cmd_send *cmd, int mode)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    int ret = HDCDRV_OK;

#ifdef CFG_FEATURE_VFIO
    if (session->owner == HDCDRV_SESSION_OWNER_VM) {
        return ret;
    }
#endif

    block_head = (struct hdcdrv_mem_block_head *)dest;
    if ((block_head == NULL) || (block_head->dma_buf == NULL) || (cmd->src_buf == NULL) || (cmd->len <= 0)) {
        hdcdrv_err("Input params Invalid. (devid=%d)\n", session->dev_id);
        return HDCDRV_PARA_ERR;
    }
    if (mode == HDCDRV_MODE_USER) {
        if (ka_base_copy_from_user(block_head->dma_buf, (void __ka_user *)cmd->src_buf, (unsigned long)cmd->len) != 0) {
            return HDCDRV_COPY_FROM_USER_FAIL;
        }
    } else {
        ret = memcpy_s(block_head->dma_buf, (size_t)cmd->len, cmd->src_buf, (size_t)cmd->len);
    }

    return ret;
}

STATIC int hdcdrv_copy_to_user(const struct hdcdrv_session *session, struct hdcdrv_cmd_recv *cmd,
    int len, int mode, int* mem_id_list)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    int ret = HDCDRV_OK;
    int count = 0;
    unsigned int offset = 0;

#ifdef CFG_FEATURE_VFIO
    if (session->owner == HDCDRV_SESSION_OWNER_VM) {
        cmd->pool_buf = cmd->buf_list[0];
        cmd->out_len = len;
        ret = HDCDRV_CMD_CONTINUE;
        return ret;
    }
#endif

    if (mode == HDCDRV_MODE_USER) {
        for (count = 0; count < cmd->buf_count; count++) {
            block_head = (struct hdcdrv_mem_block_head *)cmd->buf_list[count];
            if ((block_head == NULL) || (block_head->dma_buf == NULL) || (cmd->dst_buf == NULL) || (cmd->buf_len[count] == 0)) {
                hdcdrv_err("Input params Invalid. (devid=%d)\n", session->dev_id);
                return HDCDRV_PARA_ERR;
            }
            if (ka_base_copy_to_user((void __ka_user *)cmd->dst_buf + offset, block_head->dma_buf,
                (unsigned long)cmd->buf_len[count]) != 0) {
                hdcdrv_err("Calling ka_base_copy_to_user failed. (dev_id=%d; session=%d)\n",  session->dev_id, cmd->session);
                ret = HDCDRV_COPY_TO_USER_FAIL;
            }
            offset += cmd->buf_len[count];
        }
    } else {
        // kernel_recv now use normal_recv, so the buf_index is fixed 0
        block_head = (struct hdcdrv_mem_block_head *)cmd->buf_list[0];
        if (memcpy_s(cmd->dst_buf, (size_t)cmd->len, block_head->dma_buf, (size_t)(u32)len) != EOK) {
            hdcdrv_err("Calling memcpy_s failed.\n");
            ret = HDCDRV_ERR;
        }
    }

#ifdef CFG_FEATURE_VFIO
    if (session->owner == HDCDRV_SESSION_OWNER_CT) {
        for (count = 0; count < cmd->buf_count; count++) {
            vhdch_free_mem_container((u32)session->dev_id, session->local_fid, session->chan_id, cmd->buf_list[count]);
            cmd->buf_list[count] = NULL;
        }
        cmd->out_len = len;
        return ret;
    }
#endif
    for (count = 0; count < cmd->buf_count; count++) {
#ifdef CFG_FEATURE_MIRROR
        hdcdrv_free_mem_mirror(cmd->buf_list[count], mem_id_list[count], cmd->buf_len[count]);
#else
        free_mem(cmd->buf_list[count]);
        (void)mem_id_list;
#endif
        cmd->buf_list[count] = NULL;
    }
    cmd->pool_buf = NULL;
    cmd->out_len = len;
    return ret;
}

int hdcdrv_get_service_level(int service_type)
{
    return hdc_ctrl->service_attr[service_type].level;
}

int hdcdrv_get_service_conn_feature(int service_type)
{
    return hdc_ctrl->service_attr[service_type].conn_feature;
}

int hdcdrv_service_level_init(int service_type)
{
    if ((service_type == HDCDRV_SERVICE_TYPE_RDMA) || (service_type == HDCDRV_SERVICE_TYPE_FRAMEWORK) ||
        (service_type == HDCDRV_SERVICE_TYPE_TSD) || (service_type == HDCDRV_SERVICE_TYPE_TDT)) {
        return HDCDRV_SERVICE_HIGH_LEVEL;
    }

    return HDCDRV_SERVICE_LOW_LEVEL;
}

int hdcdrv_service_conn_feature_init(int service_type)
{
    if (service_type == HDCDRV_SERVICE_TYPE_DMP) {
        return HDCDRV_SERVICE_SHORT_CONN;
    }

    return HDCDRV_SERVICE_LONG_CONN;
}

STATIC int hdcdrv_tasklet_status_check(struct hdcdrv_msg_chan_tasklet_status *tasklet_status)
{
    if (tasklet_status->schedule_in_last != tasklet_status->schedule_in) {
        tasklet_status->schedule_in_last = tasklet_status->schedule_in;
        tasklet_status->no_schedule_cnt = 0;
        return HDCDRV_OK;
    }

    tasklet_status->no_schedule_cnt++;

    if (tasklet_status->no_schedule_cnt <= HDCDRV_TASKLET_STATUS_CHECK_TIME) {
        return HDCDRV_OK;
    }

    tasklet_status->no_schedule_cnt = 0;

    return HDCDRV_ERR;
}

STATIC int hdcdrv_get_wait_flag(int timeout)
{
    int wait_flag;

    if (timeout < 0) {
        wait_flag = HDCDRV_WAIT_ALWAYS;
    } else if (timeout == 0) {
        wait_flag = HDCDRV_NOWAIT;
    } else {
        wait_flag = HDCDRV_WAIT_TIMEOUT;
    }

    return wait_flag;
}

struct hdcdrv_service *hdcdrv_search_service(u32 dev_id, u32 fid, int service_type, u64 host_pid)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *next = NULL;
    struct hdcdrv_service *service = NULL;

#ifdef CFG_FEATURE_VFIO
    if ((fid != HDCDRV_DEFAULT_PM_FID) && (fid < VMNG_VDEV_MAX_PER_PDEV)) {
        return vhdch_search_service(dev_id, fid, service_type, host_pid);
    }
#endif

    service = &hdc_ctrl->devices[dev_id].service[service_type];

    if (hdc_ctrl->service_attr[service_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return service;
    }

#ifdef CFG_FEATURE_VFIO_DEVICE
    if ((service_type == HDCDRV_SERVICE_TYPE_TDT) && (fid == HDCDRV_DEFAULT_PM_FID)) {
        return service;
    }
#endif
    ka_task_mutex_lock(&hdc_ctrl->devices[dev_id].mutex);
    if (ka_list_empty_careful(&service->serv_list) == 0) {
        ka_list_for_each_safe(pos, next, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_pid == host_pid) {
                ka_task_mutex_unlock(&hdc_ctrl->devices[dev_id].mutex);
                return &node->service;
            }
        }
    }
    ka_task_mutex_unlock(&hdc_ctrl->devices[dev_id].mutex);

    return service;
}

STATIC struct hdcdrv_service *hdcdrv_alloc_service(u32 dev_id, u32 fid, int service_type, u64 host_pid)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *next = NULL;
    struct hdcdrv_service *service = NULL;
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[dev_id];

#ifdef CFG_FEATURE_VFIO
    if ((fid > HDCDRV_DEFAULT_PM_FID) && (fid < VMNG_VDEV_MAX_PER_PDEV)) {
        return vhdch_alloc_service(dev_id, fid, service_type, host_pid);
    }
#endif

    service = &dev->service[service_type];

    if (hdc_ctrl->service_attr[service_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return service;
    }

#ifdef CFG_FEATURE_VFIO_DEVICE
    if ((service_type == HDCDRV_SERVICE_TYPE_TDT) && (fid == HDCDRV_DEFAULT_PM_FID)) {
        return service;
    }
#endif

    ka_task_mutex_lock(&dev->mutex);

    /* Checking whether the process server with the Same hostpid exists */
    if (ka_list_empty_careful(&service->serv_list) == 0) {
        ka_list_for_each_safe(pos, next, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_pid == host_pid) {
                ka_task_mutex_unlock(&dev->mutex);
                return &node->service;
            }
        }
    }

    /* Searching for an idle server */
    if (ka_list_empty_careful(&service->serv_list) == 0) {
        ka_list_for_each_safe(pos, next, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_pid == HDCDRV_INVALID) {
                node->service.listen_pid = host_pid;
                ka_task_mutex_unlock(&dev->mutex);
                return &node->service;
            }
        }
    }
    ka_task_mutex_unlock(&dev->mutex);

    return NULL;
}

STATIC void hdcdrv_free_service(struct hdcdrv_service *service)
{
    ka_task_mutex_lock(&service->mutex);
    service->listen_pid = HDCDRV_INVALID;
    ka_task_mutex_unlock(&service->mutex);
}

STATIC void hdcdrv_delay_work_set(struct hdcdrv_session *session, ka_delayed_work_t *work,
    unsigned int bit_offset, int timeout)
{
    int delay;

    if (hdc_ctrl->debug_state.valid == HDCDRV_VALID) {
        delay = HDCDRV_DEBUG_MODE_TIMEOUT;
    } else {
        delay = timeout;
    }

    ka_task_spin_lock_bh(&session->lock);
    if ((session->delay_work_flag & KA_BASE_BIT(bit_offset)) != 0) {
        ka_task_spin_unlock_bh(&session->lock);
        hdcdrv_warn("Session work is setted. (flag_offset=%d)\n", bit_offset);
        return;
    }
    session->delay_work_flag |= KA_BASE_BIT(bit_offset);
    ka_task_spin_unlock_bh(&session->lock);

    (void)ka_task_schedule_delayed_work(work, (unsigned long)delay);
}

STATIC void hdcdrv_delay_work_cancel(struct hdcdrv_session *session, ka_delayed_work_t *work,
    unsigned int bit_offset)
{
    ka_task_spin_lock_bh(&session->lock);
    // Once the session enters the cancel close_unknown_session process, cnt++.
    if (bit_offset == HDCDRV_DELAY_UNKNOWN_SESSION_BIT) {
        session->work_cancel_cnt++;
    }
    if (!(session->delay_work_flag & KA_BASE_BIT(bit_offset))) {
        if (bit_offset == HDCDRV_DELAY_UNKNOWN_SESSION_BIT) {
            // Once the session exits the cancel close_unknown_session process, cnt--.
            session->work_cancel_cnt--;
        }
        ka_task_spin_unlock_bh(&session->lock);
        return;
    }

    session->delay_work_flag &= ~KA_BASE_BIT(bit_offset);
    ka_task_spin_unlock_bh(&session->lock);

    (void)ka_task_cancel_delayed_work_sync(work);

    if (bit_offset == HDCDRV_DELAY_UNKNOWN_SESSION_BIT) {
        // After the cancel close_unknow_session process is executed for a session, cnt--.
        ka_task_spin_lock_bh(&session->lock);
        session->work_cancel_cnt--;
        ka_task_spin_unlock_bh(&session->lock);
    }
}

STATIC void hdcdrv_delay_work_flag_clear(struct hdcdrv_session *session, unsigned int bit_offset)
{
    ka_task_spin_lock_bh(&session->lock);
    session->delay_work_flag &= ~KA_BASE_BIT(bit_offset);
    ka_task_spin_unlock_bh(&session->lock);
}

STATIC struct hdcdrv_service *hdcdrv_link_ctrl_msg_get_service(u32 devid, int service_type,
    enum HDC_LINK_CTRL_MSG_STATUS_TYPE status_type)
{
    struct hdcdrv_dev *dev = NULL;

    if ((status_type < HDCDRV_LINK_CTRL_MSG_SEND_SUCC) || (status_type >= HDCDRV_LINK_CTRL_MSG_STATUS_MAX)) {
        return NULL;
    }

    if (hdcdrv_dev_para_check((int)devid, service_type) != HDCDRV_OK) {
        return NULL;
    }

    dev = &hdc_ctrl->devices[devid];
    return &dev->service[service_type];
}

STATIC inline void hdcdrv_conncet_ctrl_msg_stats_add(struct hdcdrv_service *service,
    enum HDC_LINK_CTRL_MSG_STATUS_TYPE status_type, int last_err)
{
    service->service_stat.connect_msg_stat.count[status_type]++;
    if (last_err != HDCDRV_OK) {
        service->service_stat.connect_msg_stat.last_err[status_type] = last_err;
    }
}

STATIC inline void hdcdrv_reply_ctrl_msg_stats_add(struct hdcdrv_service *service,
    enum HDC_LINK_CTRL_MSG_STATUS_TYPE status_type, int last_err)
{
    service->service_stat.reply_msg_stat.count[status_type]++;
    if (last_err != HDCDRV_OK) {
        service->service_stat.reply_msg_stat.last_err[status_type] = last_err;
    }
}

STATIC inline void hdcdrv_close_ctrl_msg_stats_add(struct hdcdrv_service *service,
    enum HDC_LINK_CTRL_MSG_STATUS_TYPE status_type, int last_err)
{
    service->service_stat.close_msg_stat.count[status_type]++;
    if (last_err != HDCDRV_OK) {
        service->service_stat.close_msg_stat.last_err[status_type] = last_err;
    }
}

STATIC void hdcdrv_link_ctrl_msg_stats_add(u32 devid, int service_type, u32 msg_type,
    enum HDC_LINK_CTRL_MSG_STATUS_TYPE status_type, int last_err)
{
    struct hdcdrv_service *service = NULL;

    service = hdcdrv_link_ctrl_msg_get_service(devid, service_type, status_type);
    if (service == NULL) {
        return;
    }

    ka_task_mutex_lock(&service->mutex);
    switch (msg_type) {
        case HDCDRV_CTRL_MSG_TYPE_CONNECT:
            hdcdrv_conncet_ctrl_msg_stats_add(service, status_type, last_err);
            break;
        case HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY:
            hdcdrv_reply_ctrl_msg_stats_add(service, status_type, last_err);
            break;
        case HDCDRV_CTRL_MSG_TYPE_CLOSE:
            hdcdrv_close_ctrl_msg_stats_add(service, status_type, last_err);
            break;
        default:
            break;
    }
    ka_task_mutex_unlock(&service->mutex);
}

long hdcdrv_session_alive_check(int session_fd, int dev_id, u32 unique_val)
{
    struct hdcdrv_session *session = NULL;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0)) {
        hdcdrv_err("session_fd is illegal. (dev_id=%d, session_fd=%d)\n", dev_id, session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];

    if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) {
        hdcdrv_link_ctrl_msg_stats_add((u32)dev_id, session->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
            HDCDRV_LINK_CTRL_MSG_RECV_FAIL, session->local_close_state);
        if (session->service_type != HDCDRV_SERVICE_TYPE_DMP) {
            hdcdrv_warn("Session has closed. (dev_id=%d; service_type=\"%s\"; l_session_fd=%d; r_session_fd=%d)\n",
                session->dev_id, hdcdrv_sevice_str(session->service_type),
                session->local_session_fd, session->remote_session_fd);
            hdcdrv_warn("Session has closed. (dev_id=%d; session=%d; l_close_state=\"%s\"; r_close_state=\"%s\")\n",
                session->dev_id, session_fd, hdcdrv_close_str(session->local_close_state),
                hdcdrv_close_str(session->remote_close_state));
        }
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    if ((session->dev_id != dev_id) || (session->unique_val != unique_val)) {
        hdcdrv_link_ctrl_msg_stats_add((u32)dev_id, session->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
            HDCDRV_LINK_CTRL_MSG_RECV_FAIL, HDCDRV_SESSION_ID_MISS_MATCH);
        hdcdrv_warn("Session has closed. (session_fd=%d; dev_id=%d; session_devid=%d; unique_val=%u; "
            "session_unique_val=%u)\n", session_fd, dev_id, session->dev_id, unique_val, session->unique_val);
        return HDCDRV_SESSION_ID_MISS_MATCH;
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_session_state_to_remote_close(int session_fd, u32 unique_val)
{
    struct hdcdrv_session *session = NULL;
    int session_state;

    ka_task_mutex_lock(&hdc_ctrl->mutex);
    session = &hdc_ctrl->sessions[session_fd];

    session_state = hdcdrv_get_session_status(session);
    if ((session_state == HDCDRV_SESSION_STATUS_IDLE) ||
        (session_state == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) ||
        (session_state == HDCDRV_SESSION_STATUS_CLOSING)) {
        ka_task_mutex_unlock(&hdc_ctrl->mutex);
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    if (session->unique_val != unique_val) {
        ka_task_mutex_unlock(&hdc_ctrl->mutex);
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_REMOTE_CLOSED);
    ka_task_mutex_unlock(&hdc_ctrl->mutex);

    return HDCDRV_OK;
}

STATIC void hdcdrv_unbind_session_ctx(struct hdcdrv_session *session);
STATIC long hdcdrv_session_state_to_closing(const struct hdcdrv_cmd_close *cmd, int *last_status, int close_state)
{
    struct hdcdrv_session *session = NULL;
    int session_state;

    ka_task_mutex_lock(&hdc_ctrl->mutex);

    session = &hdc_ctrl->sessions[cmd->session];

    session_state = hdcdrv_get_session_status(session);
    if ((session_state == HDCDRV_SESSION_STATUS_IDLE) ||
        (session_state == HDCDRV_SESSION_STATUS_CLOSING) ||
        (session->session_cur_alloc_idx != cmd->session_cur_alloc_idx)) {
        ka_task_mutex_unlock(&hdc_ctrl->mutex);
        if (session->service_type != HDCDRV_SERVICE_TYPE_DMP) {
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x0C, "Session has closed. (l_session=%d; r_session=%d; "
                "pid=%llu, dev_id=%d; service_type=\"%s\")\n", cmd->session, session->remote_session_fd,
                session->owner_pid, session->dev_id, hdcdrv_sevice_str(session->service_type));
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x14, "Session has closed. (l_close=\"%s\"; r_close=\"%s\"; "
                "session_state=%d)\n", hdcdrv_close_str(session->local_close_state),
                hdcdrv_close_str(session->remote_close_state), session_state);
        }
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    if (hdcdrv_session_task_start_time_compare(cmd->task_start_time, session->task_start_time) != HDCDRV_OK) {
        ka_task_mutex_unlock(&hdc_ctrl->mutex);
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x0D, "Task time not match. (dev_id=%d; session=%d; pid=%llu;"
            " close_pid=%llu; task_time=%llu, session_time=%llu, close_state=%d)\n", session->dev_id, cmd->session,
            session->owner_pid, cmd->pid, cmd->task_start_time, session->task_start_time, close_state);
        return HDCDRV_PARA_ERR;
    }

    if (session->owner_pid != cmd->pid) {
        ka_task_mutex_unlock(&hdc_ctrl->mutex);
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x0E, "Owner pid not match. (session=%d; pid=%llu; "
            "close_pid=%llu; dev_id=%d)\n", cmd->session, session->owner_pid, cmd->pid, session->dev_id);
        return HDCDRV_PARA_ERR;
    }

    if ((close_state == HDCDRV_CLOSE_TYPE_USER) && (hdcdrv_get_container_id() != session->container_id)) {
        ka_task_mutex_unlock(&hdc_ctrl->mutex);
        hdcdrv_err("Container id verify failed. (session=%d, pid=%llu)\n", cmd->session, session->owner_pid);
        return HDCDRV_PARA_ERR;
    }

    *last_status = session_state;
    hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_CLOSING);
    hdcdrv_unbind_session_ctx(session);

    ka_task_mutex_unlock(&hdc_ctrl->mutex);

    return HDCDRV_OK;
}

long hdcdrv_session_para_check(int session_fd, int device_id)
{
    struct hdcdrv_session *session = NULL;
    int session_status;
    u32 container_id;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0)) {
        hdcdrv_err_limit("session_fd is illegal. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];

    session_status = hdcdrv_get_session_status(session);
    if ((session_status == HDCDRV_SESSION_STATUS_IDLE) || (session_status == HDCDRV_SESSION_STATUS_CLOSING)) {
        hdcdrv_warn_limit("session is invalid. (devid=%d; session_fd=%d; r_session_fd=%d; service_type=\"%s\")\n",
            session->dev_id, session_fd, session->remote_session_fd, hdcdrv_sevice_str(session->service_type));
        hdcdrv_warn_limit("session is invalid. (devid=%d; session_fd=%d; l_close_state=\"%s\"; r_close_state=\"%s\")\n",
            session->dev_id, session_fd, hdcdrv_close_str(session->local_close_state),
            hdcdrv_close_str(session->remote_close_state));
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    if ((session->owner == HDCDRV_SESSION_OWNER_VM) && (session->dev_id != device_id)) {
        hdcdrv_err("dev_id verify failed. (session=%d; session devid=%d; devid=%d; owner=%u)\n",
            session_fd, session->dev_id, device_id, session->owner);
        return HDCDRV_PARA_ERR;
    }

    container_id = hdcdrv_get_container_id();
    if (container_id != session->container_id) {
        if (hdcdrv_get_session_status(session) != HDCDRV_SESSION_STATUS_CONN) {
            hdcdrv_warn_limit("Session closed. (devid=%d; session_fd=%d; s_ct_id=%u; ct_id=%u; l_close_state=%d)\n",
                session->dev_id, session_fd, session->container_id, container_id, session->local_close_state);
        } else {
            hdcdrv_err("Container ID verify failed. (session=%d; device_id=%d; session ct_id=%u; ct_id=%u)\n",
                session_fd, session->dev_id, session->container_id, container_id);
        }
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

long hdcdrv_session_alloc_idx_check(int session_fd, u32 alloc_idx)
{
    struct hdcdrv_session *session = NULL;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0)) {
        hdcdrv_err_limit("session_fd is illegal. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];
    if (session->session_cur_alloc_idx != alloc_idx) {
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    if (session->remote_session_close_flag == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) {
        hdcdrv_warn_limit("Remote session has been closed. (devid=%d; session_fd=%d)\n", session->dev_id, session_fd);
        return HDCDRV_SESSION_HAS_CLOSED;
    }
    return 0;
}

long hdcdrv_session_valid_check(int session_fd, int device_id, u64 check_pid)
{
    struct hdcdrv_session *session = NULL;
    long ret;

    ret = hdcdrv_session_para_check(session_fd, device_id);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    session = &hdc_ctrl->sessions[session_fd];
    ret = hdcdrv_check_session_owner(session, check_pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    if (hdcdrv_get_device_status(session->dev_id) != HDCDRV_VALID) {
        return HDCDRV_DEVICE_RESET;
    }

    return HDCDRV_OK;
}

long hdcdrv_session_inner_check(int session_fd, u32 checker)
{
    struct hdcdrv_session *session = NULL;
    int session_status;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0)) {
        hdcdrv_err("Input pararmeter is error. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];
    session_status = hdcdrv_get_session_status(session);
    if ((session_status == HDCDRV_SESSION_STATUS_IDLE) || (session_status == HDCDRV_SESSION_STATUS_CLOSING)) {
        hdcdrv_warn("Session has closed. (dev_id=%d; service_type=\"%s\"; l_session_fd=%d; r_session_fd=%d)\n",
            session->dev_id, hdcdrv_sevice_str(session->service_type),
            session->local_session_fd, session->remote_session_fd);
        hdcdrv_warn("Session has closed. (dev_id=%d; l_close_state=\"%s\"; r_close_state=\"%s\")\n",
            session->dev_id, hdcdrv_close_str(session->local_close_state), hdcdrv_close_str(session->remote_close_state));
        return HDCDRV_PARA_ERR;
    }

    if (session->inner_checker != checker) {
        hdcdrv_warn("Session inner check not match, session may closed before. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC struct hdcdrv_session_fast_rx *hdcdrv_get_session_fast_rx(int session)
{
    return &hdc_ctrl->sessions[session].fast_rx;
}

STATIC void hdcdrv_bind_sessoin_ctx(struct hdcdrv_ctx *ctx, struct hdcdrv_session *session)
{
    if (ctx == HDCDRV_KERNEL_WITHOUT_CTX) {
        return;
    }

    /* this func no need ka_task_mutex_lock(&session->mutex), hdcdrv_set_session_owner has been added */
    ctx->session_fd = session->local_session_fd;
    ctx->pid = session->owner_pid;
    ctx->task_start_time = session->task_start_time;
    ctx->service = NULL;
    ctx->session = session;
    ctx->dev_id = session->dev_id;
    session->ctx = ctx;

    return;
}

STATIC void hdcdrv_unbind_session_ctx(struct hdcdrv_session *session)
{
    ka_task_mutex_lock(&session->mutex);
    if (session->ctx != NULL) {
        session->ctx->session = NULL;
        session->ctx = NULL;
    }
    ka_task_mutex_unlock(&session->mutex);
}

STATIC void hdcdrv_bind_server_ctx(struct hdcdrv_ctx *ctx, struct hdcdrv_service *service,
    int dev_id, int service_type)
{
    if (ctx == HDCDRV_KERNEL_WITHOUT_CTX) {
        return;
    }

    ctx->dev_id = dev_id;
    ctx->pid = service->listen_pid;
    ctx->service_type = service_type;
    ctx->session = NULL;
    ctx->service = service;

    service->ctx = ctx;

    return;
}

STATIC void hdcdrv_unbind_server_ctx(struct hdcdrv_service *server)
{
    if (server->ctx != NULL) {
        server->ctx->service = NULL;
        server->ctx = NULL;
    }
}

STATIC u64 hdcdrv_set_send_timeout(const struct hdcdrv_session *session, unsigned int timeout)
{
    if (timeout == 0) {
        return session->timeout_jiffies.send_timeout;
    } else {
        return ka_system_msecs_to_jiffies(timeout);
    }
}

STATIC long hdcdrv_set_fast_send_timeout(const struct hdcdrv_session *session, unsigned int timeout)
{
    if (timeout == 0) {
        return (long)session->timeout_jiffies.fast_send_timeout;
    } else {
        return (long)ka_system_msecs_to_jiffies(timeout);
    }
}

STATIC void hdcdrv_inner_checker_set(struct hdcdrv_session *session)
{
    session->inner_checker = session->unique_val;
}

STATIC void hdcdrv_inner_checker_clear(struct hdcdrv_session *session)
{
    session->inner_checker = 0;
}

STATIC void hdcdrv_fill_sq_desc(struct hdcdrv_sq_desc *sq_desc,
    const struct hdcdrv_session *session, struct hdcdrv_buf_desc *buf_d, int type, int tail)
{
    sq_desc->local_session = session->local_session_fd;
    sq_desc->remote_session = session->remote_session_fd;
    sq_desc->src_data_addr = (u64)buf_d->addr;
    sq_desc->data_len = buf_d->len;
    sq_desc->inner_checker = session->unique_val;
#ifdef CFG_FEATURE_PFSTAT
    sq_desc->trans_id = tail;
#endif
    if (type == HDCDRV_MSG_CHAN_TYPE_FAST) {
        sq_desc->dst_data_addr = buf_d->dst_data_addr;
        sq_desc->dst_ctrl_addr = buf_d->dst_ctrl_addr;
        sq_desc->ctrl_len = buf_d->ctrl_len;
        sq_desc->src_fid = buf_d->fid;
        sq_desc->src_pid = buf_d->pid;
        sq_desc->src_data_addr = buf_d->src_data_addr_va;
        sq_desc->src_ctrl_addr = buf_d->src_ctrl_addr_va;
    }
    sq_desc->desc_crc = hdcdrv_calculate_crc((unsigned char *)sq_desc, HDCDRV_SQ_DESC_CRC_LEN);
    ka_wmb();
    sq_desc->valid = HDCDRV_VALID;
}

STATIC void hdcdrv_msg_chan_tx_static(struct hdcdrv_msg_chan *msg_chan)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    u32 delay;
    delay = ka_system_jiffies_to_msecs(ka_jiffies - msg_chan->dfx_tx_stamp);
    if (delay > (u32)HDCDRV_CHAN_DFX_DELAY_TIME) {
        msg_chan->dfx_tx_stamp = (u32)ka_jiffies;
        hdcdrv_info("chan tx statistic. (chan_id=%d, total=%llu, finish=%llu, full=%llu, "
            "tx_unsuccess=%llu, alloc_unsuccess=%llu)\n",
            msg_chan->chan_id, msg_chan->stat.tx, msg_chan->stat.tx_finish,
            msg_chan->stat.tx_full, msg_chan->stat.tx_fail, msg_chan->stat.alloc_mem_err);
    }
#endif
    return;
}

STATIC void hdcdrv_msg_chan_rx_static(struct hdcdrv_msg_chan *msg_chan)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    u32 delay;
    delay = ka_system_jiffies_to_msecs(ka_jiffies - msg_chan->dfx_rx_stamp);
    if (delay > (u32)HDCDRV_CHAN_DFX_DELAY_TIME) {
        msg_chan->dfx_rx_stamp = (u32)ka_jiffies;
        hdcdrv_info("chan rx statistic. (chan_id=%d, rx_total=%llu, finish=%llu, full=%llu, rx_unsuccess=%llu)\n",
            msg_chan->chan_id, msg_chan->stat.rx, msg_chan->stat.rx_finish,
            msg_chan->stat.rx_full, msg_chan->stat.rx_fail);
    }
#endif
    return;
}

STATIC long hdcdrv_msg_chan_send_wait(struct hdcdrv_msg_chan *msg_chan, struct hdcdrv_session *session,
    int wait_flag, u64 timeout)
{
    long ret;
    struct hdcdrv_dev *dev = NULL;
    struct hdcdrv_service *service = NULL;
    u32 unique_val = session->unique_val;

    dev = &hdc_ctrl->devices[msg_chan->dev_id];
    service = &hdc_ctrl->devices[msg_chan->dev_id].service[session->service_type];

    if (wait_flag == HDCDRV_NOWAIT) {
        return HDCDRV_NO_BLOCK;
    } else if (wait_flag == HDCDRV_WAIT_ALWAYS) {
        /*lint -e666*/
        ret = (long)ka_task_wait_event_interruptible_exclusive(msg_chan->send_wait,
            (!(hdcdrv_w_sq_full_check(msg_chan->chan))) ||
            (dev->valid != HDCDRV_VALID) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
            (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) ||
            (session->unique_val != unique_val));
        /*lint +e666*/
    } else {
        /*lint -e666*/
        ret = ka_task_wait_event_interruptible_timeout(msg_chan->send_wait,
            (!(hdcdrv_w_sq_full_check(msg_chan->chan))) ||
            (dev->valid != HDCDRV_VALID) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
            (session->unique_val != unique_val),
            (long)timeout);
        /*lint +e666*/
        if (ret == 0) {
            HDC_LOG_WARN_LIMIT(&service->service_stat.send_print_cnt, &service->service_stat.send_jiffies,
                "Send timeout, device=%u; session=%d; service=\"%s\";status=%d; timeout=%llu; full_check=%u\n",
                dev->dev_id, session->local_session_fd, hdcdrv_sevice_str(session->service_type),
                hdcdrv_get_session_status(session), timeout, hdcdrv_w_sq_full_check(msg_chan->chan));
            HDC_LOG_WARN_LIMIT(&service->service_stat.send_print_cnt1, &service->service_stat.send_jiffies1,
                "Send timeout, device=%u; session=%d; chanid=%d; send=%llu, notify1=%llu; task7=%llu;\n",
                dev->dev_id, session->local_session_fd, msg_chan->chan_id, msg_chan->dbg_stat.hdcdrv_msg_chan_send1,
                msg_chan->dbg_stat.hdcdrv_tx_finish_notify1, msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task7);
            return HDCDRV_TX_TIMEOUT;
        } else if (ret > 0) {
            ret = 0;
        }
    }

    if (ret != 0) {
        hdcdrv_warn("Get wait_ret. (device=%u; session=%d; service_type=\"%s\"; wait_ret=%ld)\n", dev->dev_id,
            session->local_session_fd, hdcdrv_sevice_str(session->service_type), ret);
        return ret;
    }

    if (dev->valid != HDCDRV_VALID) {
        /* Clear the packets in the waiting queue */
        ka_task_wake_up_interruptible(&msg_chan->send_wait);
        hdcdrv_err("Device reset. (session=%d; service_type=\"%s\"; device=%u)\n",
            session->local_session_fd, hdcdrv_sevice_str(session->service_type), dev->dev_id);

        return HDCDRV_DEVICE_RESET;
    }

    if (session->unique_val != unique_val) {
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x07, "Unique val not match.(dev=%d; service=\"%s\"; "
            "l_fd=%d; r_fd=%d)\n", session->dev_id, hdcdrv_sevice_str(session->service_type),
            session->local_session_fd, session->remote_session_fd);
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x13, "Unique val not match.(dev=%d; l_state=\"%s\"; "
            "r_state=\"%s\")\n", session->dev_id, hdcdrv_close_str(session->local_close_state),
            hdcdrv_close_str(session->remote_close_state));
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    if (hdcdrv_get_session_status(session) != HDCDRV_SESSION_STATUS_CONN) {
        hdcdrv_warn("Session not connect. (device=%u; chan_id=%d; service_type=\"%s\"; l_session_fd=%d; "
            "r_session_fd=%d)\n", dev->dev_id, msg_chan->chan_id, hdcdrv_sevice_str(session->service_type),
            session->local_session_fd, session->remote_session_fd);
        hdcdrv_warn("Session not connect. (device=%u; session=%d; l_close_state=\"%s\"; r_close_state=\"%s\")\n",
            dev->dev_id, session->local_session_fd, hdcdrv_close_str(session->local_close_state),
            hdcdrv_close_str(session->remote_close_state));
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_msg_chan_send(struct hdcdrv_msg_chan *msg_chan, struct hdcdrv_buf_desc *buf_d,
    int wait_flag, u64 timeout)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_service *service = NULL;
    struct hdcdrv_sq_desc *sq_desc = NULL;
    struct hdcdrv_dev *dev = NULL;
    u32 tail = 0;
    long ret;

    if ((msg_chan == NULL) || (msg_chan->chan == NULL) || (buf_d == NULL)) {
        hdcdrv_err("Input params msg_chan Invalid.\n");
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[buf_d->local_session];
    service = session->service;
    if (service == NULL) {
        hdcdrv_err("Input params service Invalid. (devid=%d)\n", session->dev_id);
        return HDCDRV_PARA_ERR;
    }

    dev = &hdc_ctrl->devices[msg_chan->dev_id];

    ka_task_mutex_lock(&msg_chan->mutex);
#ifndef HDC_ENV_DEVICE
    ret = (long)hdcdrv_get_msgchan_refcnt(msg_chan->dev_id);
    if (ret != 0) {
        hdcdrv_err("Can not send message while hot reset flag is set. (ret=%ld, dev_id=%d)\n", ret, msg_chan->dev_id);
        ka_task_mutex_unlock(&msg_chan->mutex);
        return ret;
    }
#endif
    msg_chan->dbg_stat.hdcdrv_msg_chan_send1++;

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&buf_d->latency_info.wait_sq_timestamp);
#endif
    /* Wait until there is space to send */
    while (hdcdrv_w_sq_full_check(msg_chan->chan) &&
           (hdcdrv_get_peer_status() == DEVDRV_PEER_STATUS_NORMAL)) {
        msg_chan->stat.tx_full++;
        session->stat.tx_full++;
        service->data_stat.tx_full++;
        ka_task_mutex_unlock(&msg_chan->mutex);
#ifdef CFG_FEATURE_HDC_REG_MEM
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x107, "tx sq full. (chan_id=%d, dev_id=%u, chan_type=%d)\n",
            msg_chan->chan_id, msg_chan->dev_id, msg_chan->type);
#endif
        ret = hdcdrv_msg_chan_send_wait(msg_chan, session, wait_flag, timeout);
        if (ret != HDCDRV_OK) {
            return ret;
        }
        ka_task_mutex_lock(&msg_chan->mutex);
    }
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, TX_WAIT_SQ_LATENCY, buf_d->latency_info.wait_sq_timestamp);
#endif

    if (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) {
        hdcdrv_warn("peer status abnormal, stop send\n");
        ka_task_mutex_unlock(&msg_chan->mutex);
        return HDCDRV_PEER_REBOOT;
    }

    sq_desc = hdcdrv_get_w_sq_desc(msg_chan->chan, &tail);
    if (sq_desc == NULL) {
        hdcdrv_err("Calling hdcdrv_get_w_sq_desc failed.\n");
        ka_task_mutex_unlock(&msg_chan->mutex);
        return HDCDRV_SQ_DESC_NULL;
    }

    /* stored locally for release after subsequent sending */
    msg_chan->tx[tail] = *buf_d;

    hdcdrv_fill_sq_desc(sq_desc, session, buf_d, msg_chan->type, tail);

    msg_chan->stat.tx++;
    msg_chan->stat.tx_bytes += (u32)buf_d->len;
    session->stat.tx++;
    session->stat.tx_bytes += (u32)buf_d->len;
    service->data_stat.tx++;
    service->data_stat.tx_bytes += (u32)buf_d->len;
    ka_wmb();
#ifdef CFG_FEATURE_HDC_REG_MEM
    ret = (long)hdcdrv_copy_sq_desc_to_remote(msg_chan, sq_desc, msg_chan->data_type);
    if (ret != 0) {
        hdcdrv_err("copy sq to remote fail.\n");
        ka_task_mutex_unlock(&msg_chan->mutex);
        return HDCDRV_DMA_COPY_FAIL;
    }
#else
    /* data and ctrl use diff dma channel */
    (void)hdcdrv_copy_sq_desc_to_remote(msg_chan, sq_desc, DEVDRV_DMA_DATA_TRAFFIC);
#endif
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, TX_SEND_LATENCY, buf_d->latency_info.send_timestamp);
#endif
    hdcdrv_msg_chan_tx_static(msg_chan);
    ka_task_mutex_unlock(&msg_chan->mutex);
    return HDCDRV_OK;
}

#ifdef CFG_FEATURE_HDC_REG_MEM
STATIC void hdcdrv_tx_add_mem_to_wait_fifo(struct hdcdrv_session *session, struct hdcdrv_buf_desc *tx_desc, int status)
{
    u32 result_type = session->result_type;

    struct hdcdrv_wait_mem_fin_msg in_msg = {
        .dataAddr = tx_desc->src_data_addr_va,
        .dataLen = (u32)tx_desc->len,
        .ctrlAddr = tx_desc->src_ctrl_addr_va,
        .ctrlLen = (u32)tx_desc->ctrl_len,
        .status = status
    };

    if ((result_type == (u32)HDC_WAIT_ALL) ||
        ((in_msg.status != 0) && (result_type == (u32)HDC_WAIT_ONLY_EXCEPTION)) ||
        ((in_msg.status == 0) && (result_type == (u32)HDC_WAIT_ONLY_SUCCESS))) {
        hdcdrv_update_session_release_mem_event(session, &in_msg);
    }
}
#endif

STATIC void hdcdrv_tx_finish_handle(struct hdcdrv_msg_chan *msg_chan, const struct hdcdrv_cq_desc *cq_desc)
{
    struct hdcdrv_buf_desc *tx_desc = NULL;
    u16 tx_head = (u16)cq_desc->sq_head;
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_service *service = NULL;

    if ((cq_desc->session >= HDCDRV_REAL_MAX_SESSION) || (cq_desc->session < 0)) {
        msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task6++;
        hdcdrv_err("Input pararmeter is error. (device=%u; session=%d; sq_head=%d; valid=0x%x)\n",
            msg_chan->dev_id, cq_desc->session, tx_head, cq_desc->valid);
        goto OUT;
    }

    session = &hdc_ctrl->sessions[cq_desc->session];
    tx_desc = &msg_chan->tx[tx_head];
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, TX_FINISH_LATENCY, tx_desc->latency_info.send_timestamp);
#endif
    if (msg_chan->type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
        if (ka_unlikely(tx_desc->buf == NULL)) {
            msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task5++;
            session->dbg_stat.hdcdrv_tx_finish_notify_task5++;
            hdcdrv_err("tx_desc_buf is invalid. (device=%u; session=%d; tx_head=%d; valid=0x%x)\n",
                msg_chan->dev_id, cq_desc->session, tx_head, cq_desc->valid);
            goto OUT;
        }

        hdcdrv_free_mem(session, tx_desc->buf, HDCDRV_MEMPOOL_FREE_IN_PM, tx_desc);
    }
#ifdef CFG_FEATURE_HDC_REG_MEM
    if (msg_chan->type == HDCDRV_MSG_CHAN_TYPE_FAST) {
        hdcdrv_tx_add_mem_to_wait_fifo(session, tx_desc, cq_desc->status);
    }
#endif

    service = session->service;
    if (cq_desc->status != 0) {
        msg_chan->stat.tx_fail++;
        session->stat.tx_fail++;
        service->data_stat.tx_fail++;

        if (cq_desc->status == HDCDRV_SESSION_HAS_CLOSED) {
            hdcdrv_warn("TX unsuccess, remote session closed. (device=%u; session=%d; service_type=\"%s\"; "
                "type=%d; tx_head=%d; status=%d; ctrl_len=%d; data_len=%d; src_pid=%u)\n",
                msg_chan->dev_id, cq_desc->session, hdcdrv_sevice_str(session->service_type),
                msg_chan->type, tx_head, cq_desc->status, tx_desc->ctrl_len, tx_desc->len, tx_desc->pid);
        } else {
            hdcdrv_err("TX failed. (device=%u; session=%d; service_type=\"%s\"; type=%d; tx_head=%d; status=%d; "
                "ctrl_len=%d; data_len=%d; src_pid=%u)\n",
                msg_chan->dev_id, cq_desc->session, hdcdrv_sevice_str(session->service_type),
                msg_chan->type, tx_head, cq_desc->status, tx_desc->ctrl_len, tx_desc->len, tx_desc->pid);
        }
    } else {
        msg_chan->stat.tx_finish++;
        session->stat.tx_finish++;
        service->data_stat.tx_finish++;
    }
    session->dbg_stat.hdcdrv_tx_finish_notify_task7++;

OUT:
    hdcdrv_desc_clear(tx_desc);
}

STATIC void hdcdrv_tx_finish_record_resq_time(struct hdcdrv_msg_chan *msg_chan)
{
    u32 resq_time;

    if (ka_jiffies < msg_chan->hdcdrv_tx_stamp) {
        resq_time = 0;
    } else {
        resq_time = ka_system_jiffies_to_msecs(ka_jiffies - msg_chan->hdcdrv_tx_stamp);
    }
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, TX_FINISH_SCHE_LATENCY, msg_chan->hdcdrv_tx_stamp);
#endif
    msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task_delay_new = resq_time;

    if (resq_time > HDCDRV_TIME_COMPARE_2MS) {
        msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task_delay_over2ms++;
    }
    if (resq_time > HDCDRV_TIME_COMPARE_4MS) {
        msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task_delay_over4ms++;
    }
    if (resq_time > msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task_delay_max) {
        msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task_delay_max = resq_time;
    }
}

STATIC void hdcdrv_tx_finish_notify_task(unsigned long data)
{
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)((uintptr_t)data);
    u32 retry_cnt = 0;
    u16 tx_head;
    int cnt = 0;

    struct hdcdrv_cq_desc *cq_desc = NULL;

    msg_chan->tx_finish_task_status.schedule_in++;

    msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task1++;
    hdcdrv_tx_finish_record_resq_time(msg_chan);
    do {
        cq_desc = hdcdrv_get_r_cq_desc(msg_chan->chan);
        if ((cq_desc == NULL) || (cq_desc->valid != HDCDRV_VALID)) {
            break;
        }

        if (cq_desc->session == HDCDRV_SESSION_FD_INVALID) {
            msg_chan->dbg_stat.hdcdrv_tx_finish_notify_session_no_update++;
            break;
        }

        if (cq_desc->sq_head == HDCDRV_INVALID_SQ_HEAD) {
            msg_chan->dbg_stat.hdcdrv_tx_finish_notify_sq_head_no_update++;
            break;
        }

        if (cq_desc->desc_crc != hdcdrv_calculate_crc((unsigned char *)cq_desc, HDCDRV_CQ_DESC_CRC_LEN)) {
            if (retry_cnt < HDC_CRC_CHECH_RETRY_CNT) {
                retry_cnt++;
                continue;
            }
            msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task8++;
        }
        ka_rmb();

        msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task2++;

        /* Reach the threshold and schedule out */
        if (cnt >= HDCDRV_TX_BUDGET) {
            msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task3++;
            ka_system_tasklet_schedule(&msg_chan->tx_finish_task);
            break;
        }

        tx_head = (u16)cq_desc->sq_head;

        /* the branch should not enter */
        if (tx_head >= HDCDRV_DESC_QUEUE_DEPTH) {
            msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task4++;
            hdcdrv_err("tx_head is invalid. (device=%d; sq_head=%d; session_id=%d; valid=0x%x)\n",
                msg_chan->dev_id, tx_head, cq_desc->session, cq_desc->valid);
            break;
        }

        hdcdrv_tx_finish_handle(msg_chan, cq_desc);

        cq_desc->session = HDCDRV_SESSION_FD_INVALID;
        cq_desc->valid = HDCDRV_INVALID;
        cq_desc->sq_head = HDCDRV_INVALID_SQ_HEAD;
        hdcdrv_move_r_cq_desc(msg_chan->chan);

        /* update the sq head pointer to continue send packet */
        tx_head = (tx_head + 1) % HDCDRV_DESC_QUEUE_DEPTH;
        hdcdrv_set_w_sq_desc_head(msg_chan->chan, tx_head);
        msg_chan->sq_head = tx_head;
        cnt++;
        ka_wmb();
        ka_task_wake_up_interruptible(&msg_chan->send_wait);
        msg_chan->dbg_stat.hdcdrv_tx_finish_notify_task7++;

#ifndef HDC_ENV_DEVICE
        (void)hdcdrv_put_msgchan_refcnt(msg_chan->dev_id);
#endif
    } while (1);
}

void hdcdrv_tx_finish_notify(void *chan)
{
    struct hdcdrv_dev *dev = NULL;
    struct hdcdrv_msg_chan *msg_chan = hdcdrv_get_msg_chan_priv(chan);

    if (msg_chan == NULL) {
        hdcdrv_err("Input pararmeter is error.");
        return;
    }

    dev = &hdc_ctrl->devices[msg_chan->dev_id];

    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("tx_finish_notify device reset. (dev_id=%u)\n", dev->dev_id);
        return;
    }

    msg_chan->dbg_stat.hdcdrv_tx_finish_notify1++;
    msg_chan->hdcdrv_tx_stamp = ka_jiffies;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&msg_chan->hdcdrv_tx_stamp);
#endif
    ka_wmb();

    ka_system_tasklet_schedule(&msg_chan->tx_finish_task);
}

STATIC void hdcdrv_tx_finish_task_check(struct hdcdrv_msg_chan *msg_chan)
{
    struct hdcdrv_msg_chan_tasklet_status *tasklet_status = NULL;
    struct hdcdrv_cq_desc *cq_desc = NULL;

    tasklet_status = &msg_chan->tx_finish_task_status;

    msg_chan->dbg_stat.hdcdrv_tx_finish_task_check1++;

    if (hdcdrv_tasklet_status_check(tasklet_status) == HDCDRV_OK) {
        return;
    }

    cq_desc = hdcdrv_get_r_cq_desc(msg_chan->chan);
    if (cq_desc == NULL) {
        return;
    }

    /* no valid cq */
    if (cq_desc->valid != HDCDRV_VALID) {
        return;
    }

    msg_chan->dbg_stat.hdcdrv_tx_finish_task_check2++;

    /* has finish cq but not schedule */
    ka_system_tasklet_schedule(&msg_chan->tx_finish_task);
}

STATIC void hdcdrv_response_cq(struct hdcdrv_msg_chan *msg_chan, u32 sq_head, int status, int local_session,
                        int remote_session)
{
    struct hdcdrv_cq_desc *cq_desc = NULL;

    /* response cq */
    cq_desc = hdcdrv_get_w_cq_desc(msg_chan->chan);
    if (cq_desc == NULL) {
        hdcdrv_err("Calling hdcdrv_get_w_cq_desc error. (dev_id=%u)\n", msg_chan->dev_id);
        return;
    }

    cq_desc->sq_head = sq_head;
    cq_desc->status = status;
    cq_desc->session = remote_session;
    cq_desc->desc_crc = hdcdrv_calculate_crc((unsigned char *)cq_desc, HDCDRV_CQ_DESC_CRC_LEN);
    ka_wmb();
    cq_desc->valid = HDCDRV_VALID;
    ka_wmb();

#ifdef CFG_FEATURE_HDC_REG_MEM
    hdcdrv_copy_cq_desc_to_remote(msg_chan, cq_desc, msg_chan->data_type);
#else
    /* data and ctrl use diff dma channel */
    hdcdrv_copy_cq_desc_to_remote(msg_chan, cq_desc, DEVDRV_DMA_DATA_TRAFFIC);
#endif
}

STATIC bool hdcdrv_session_rx_list_is_full(const int session_fd, const int chan_type)
{
    struct hdcdrv_session *session = &hdc_ctrl->sessions[session_fd];

    if (chan_type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
        return ((session->normal_rx.tail + 1) % HDCDRV_SESSION_RX_LIST_MAX_PKT == session->normal_rx.head);
    }

    return ((session->fast_rx.tail + 1) % HDCDRV_BUF_MAX_CNT == session->fast_rx.head);
}

STATIC int hdcdrv_rebuild_notify_pid(u64 owner_pid, u64 create_pid)
{
    u64 local_pid;

    local_pid = (owner_pid == HDCDRV_INVALID_PID) ? create_pid : owner_pid;

    return hdcdrv_rebuild_raw_pid(local_pid);
}
STATIC void hdcdrv_msg_chan_recv_update_status(struct hdcdrv_msg_chan *msg_chan,
    struct hdcdrv_service *service, struct hdcdrv_session *session, const struct hdcdrv_buf_desc *rx_desc)
{
    msg_chan->stat.rx++;
    msg_chan->stat.rx_bytes += (unsigned long long)rx_desc->len;
    session->stat.rx++;
    session->stat.rx_bytes += (unsigned long long)rx_desc->len;
    service->data_stat.rx++;
    service->data_stat.rx_bytes += (unsigned long long)rx_desc->len;
}

STATIC int hdcdrv_connect_notify(int service_type, int dev_id, int vfid, int peer_pid, int local_pid)
{
    int ret = HDCDRV_OK;

    if (g_session_notify[service_type].connect_notify == NULL) {
        return ret;
    }

    ret = g_session_notify[service_type].connect_notify(dev_id, vfid, peer_pid, local_pid);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Connect notify failed. (dev=%d; vfid=%d; peer_pid=%d; local_pid=%d; service_type=\"%s\")\n",
            dev_id, vfid, peer_pid, local_pid, hdcdrv_sevice_str(service_type));
    }

    return ret;
}

STATIC int hdcdrv_close_notify(int service_type, int dev_id, int vfid, int peer_pid, int local_pid)
{
    int ret = HDCDRV_OK;

    if (g_session_notify[service_type].close_notify == NULL) {
        return ret;
    }

    ret = g_session_notify[service_type].close_notify(dev_id, vfid, peer_pid, local_pid);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Close notify failed. (dev=%d; vfid=%d; peer_pid=%d; local_pid=%d; service_type=\"%s\")\n",
            dev_id, vfid, peer_pid, local_pid, hdcdrv_sevice_str(service_type));
    }

    return ret;
}

STATIC int hdcdrv_data_in_notify(int service_type, int dev_id, int vfid, int local_pid,
    struct hdcdrv_data_info data_info)
{
    int ret = HDCDRV_RX_CONTINUE;

    if (g_session_notify[service_type].data_in_notify == NULL) {
        return ret;
    }

    ret = g_session_notify[service_type].data_in_notify(dev_id, vfid, local_pid, data_info);

    return ret;
}

STATIC int hdcdrv_msg_chan_recv_handle(struct hdcdrv_msg_chan *msg_chan, struct hdcdrv_buf_desc *rx_desc)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    struct hdcdrv_fast_rx *rx_buf = NULL;
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_service *service = NULL;
    struct hdcdrv_data_info data_info = {0};
    int ret = HDCDRV_RX_CONTINUE;

    session = &hdc_ctrl->sessions[rx_desc->local_session];
    service = session->service;

    ka_task_spin_lock_bh(&session->lock);
    if ((g_session_notify[session->service_type].data_in_notify != NULL) && (session->owner == HDCDRV_SESSION_OWNER_PM)) {
        block_head = (struct hdcdrv_mem_block_head *)rx_desc->buf;
        data_info.data_type = msg_chan->type;
        data_info.src_addr =
            ((msg_chan->type == HDCDRV_MSG_CHAN_TYPE_NORMAL) ? (u64)(uintptr_t)block_head->dma_buf : rx_desc->dst_data_addr);
        data_info.len = (u32)rx_desc->len;
        data_info.session_fd = rx_desc->local_session;

        ret = hdcdrv_data_in_notify(session->service_type, session->dev_id, (int)session->remote_fid,
            hdcdrv_rebuild_notify_pid(session->owner_pid, session->create_pid), data_info);
        if (ret == HDCDRV_RX_FINISH) {
            hdcdrv_msg_chan_recv_update_status(msg_chan, service, session, rx_desc);
            ka_task_spin_unlock_bh(&session->lock);
            if (msg_chan->type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
                hdcdrv_free_mem(session, rx_desc->buf, HDCDRV_MEMPOOL_FREE_IN_PM, rx_desc);
            }
            return HDCDRV_OK;
        }
    }

    /* The session received too many packets, stop recv...
       Wait for the upper layer to take the packet first
       Note: There will be mutual influence between different
       sessions in the same queue */
    if (hdcdrv_session_rx_list_is_full(rx_desc->local_session, msg_chan->type)) {
        msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task6++;
        msg_chan->stat.rx_full++;
        session->stat.rx_full++;
        session->dbg_stat.hdcdrv_msg_chan_recv_task6++;
        service->data_stat.rx_full++;
        msg_chan->rx_trigger_flag = HDCDRV_VALID;
        ka_task_spin_unlock_bh(&session->lock);
#ifdef CFG_FEATURE_HDC_REG_MEM
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x106, "rx list full. (chan_id=%d, dev_id=%u, chan_type=%d)\n",
            msg_chan->chan_id, msg_chan->dev_id, msg_chan->type);
#endif
        if (ret != HDCDRV_RX_CONTINUE) {
            hdcdrv_err("Get data_in_notify ret. (dev=%d; vfid=%d; local_pid=%d; service_type=\"%s\"; ret=%d)\n",
                session->dev_id, session->remote_fid, hdcdrv_rebuild_notify_pid(session->owner_pid,
                session->create_pid), hdcdrv_sevice_str(session->service_type), ret);
        }
        return HDCDRV_ERR;
    }

    /* update stats */
    hdcdrv_msg_chan_recv_update_status(msg_chan, service, session, rx_desc);

    /* Insert into the tail of the receive list */
    if (msg_chan->type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
        session->normal_rx.rx_list[session->normal_rx.tail] = *rx_desc;
        session->normal_rx.tail = (session->normal_rx.tail + 1) % HDCDRV_SESSION_RX_LIST_MAX_PKT;
    } else {
        rx_buf = &session->fast_rx.rx_list[session->fast_rx.tail];
        rx_buf->data_addr = rx_desc->dst_data_addr;
        rx_buf->ctrl_addr = rx_desc->dst_ctrl_addr;
        rx_buf->data_len = rx_desc->len;
        rx_buf->ctrl_len = rx_desc->ctrl_len;
        session->fast_rx.tail = (session->fast_rx.tail + 1) % HDCDRV_BUF_MAX_CNT;
    }
    ka_task_spin_unlock_bh(&session->lock);

    if (ret != HDCDRV_RX_CONTINUE) {
        hdcdrv_err("Get data_in_notify ret. (dev=%d; vfid=%d; local_pid=%d; service_type=\"%s\"; ret=%d)\n",
            session->dev_id, session->remote_fid, hdcdrv_rebuild_notify_pid(session->owner_pid,
            session->create_pid), hdcdrv_sevice_str(session->service_type), ret);
    }

    session->dbg_stat.hdcdrv_msg_chan_recv_task7++;
    msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task7++;
    session->recv_peek_stamp = ka_jiffies;
    ka_wmb();

    /* Trigger the upper reception process */
    ka_task_wake_up_interruptible(&session->wq_rx);
    hdcdrv_epoll_wake_up(session->epfd);

    return HDCDRV_OK;
}

STATIC void hdcdrv_msg_chan_recv_task(unsigned long data)
{
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)((uintptr_t)data);
    struct hdcdrv_buf_desc *rx_desc = NULL;
    struct hdcdrv_session *session = NULL;
    void *buf = NULL;
    int rx_head, status, local_session, remote_session;
    long ret;

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, RX_TASK_SCHE_LATENCY, msg_chan->rx[msg_chan->dma_head].latency_info.rx_task_timestamp);
#endif
    msg_chan->rx_task_status.schedule_in++;
    msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task1++;
    while (msg_chan->rx_head != msg_chan->dma_head) {
        rx_head = (msg_chan->rx_head + 1) % HDCDRV_DESC_QUEUE_DEPTH;
        rx_desc = &msg_chan->rx[rx_head];
        ka_rmb();
        buf = rx_desc->buf;
        status = rx_desc->status;
        msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task2++;

        local_session = rx_desc->local_session;
        remote_session = rx_desc->remote_session;

        if (rx_desc->skip_flag == HDCDRV_VALID) {
            msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task3++;
            goto NEXT;
        }

        /* This branch should not enter */
        if (ka_unlikely((msg_chan->type == HDCDRV_MSG_CHAN_TYPE_NORMAL) && (buf == NULL))) {
            msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task4++;
            hdcdrv_err("Parameter buf is null. (dev=%u; session=%d; rx_head=%d)\n",
                msg_chan->dev_id, local_session, rx_head);
            msg_chan->rx_head = rx_head;
            continue;
        }

        ret = hdcdrv_session_inner_check(local_session, rx_desc->inner_checker);
        if ((ret != HDCDRV_OK) || (status != 0)) {
            msg_chan->stat.rx_fail++;
            msg_chan->dbg_stat.hdcdrv_msg_chan_recv_task5++;
            if (msg_chan->type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
                session = &hdc_ctrl->sessions[local_session];
                session->dbg_stat.hdcdrv_msg_chan_recv_task5++;
                hdcdrv_free_mem(session, buf, HDCDRV_MEMPOOL_FREE_IN_PM, rx_desc);
            }
            hdcdrv_warn("Calling session_inner_check not success. (device=%u; local_session=%d; "
                "remote_session=%d; chan_id=%d; rx_head=%d; status=%u; ret=%ld)\n",
                msg_chan->dev_id, local_session, remote_session, msg_chan->chan_id, rx_head, status, ret);
            goto NEXT;
        }

        if (hdcdrv_msg_chan_recv_handle(msg_chan, rx_desc) != HDCDRV_OK) {
            break;
        }

    NEXT:
        hdcdrv_desc_clear(rx_desc);
        hdcdrv_response_cq(msg_chan, (u32)rx_head, status, local_session, remote_session);
        msg_chan->rx_head = rx_head;
    }
}

STATIC void hdcdrv_rx_msg_callback(void *data, u32 trans_id, u32 status)
{
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)data;
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[msg_chan->dev_id];
    struct hdcdrv_session *session;

    if (msg_chan->dma_need_submit_flag == HDCDRV_VALID) {
        hdcdrv_rx_msg_schedule_task(msg_chan);
        hdcdrv_info("Device message channel schedule. (dev=%d; msg_chan=%d)\n", dev->dev_id, msg_chan->chan_id);
    }

    msg_chan->dbg_stat.hdcdrv_rx_msg_callback1++;
    /* This branch should not enter */
    if (ka_unlikely(trans_id >= HDCDRV_DESC_QUEUE_DEPTH)) {
        msg_chan->dbg_stat.hdcdrv_rx_msg_callback2++;
        hdcdrv_err("trans_id is invalid. (device=%u; trans_id=%u)\n", dev->dev_id, trans_id);
        return;
    }

    session = &hdc_ctrl->sessions[msg_chan->rx[trans_id].local_session];
    session->dbg_stat.hdcdrv_rx_msg_callback1++;

    if (msg_chan->type == HDCDRV_MSG_CHAN_TYPE_FAST) {
        hdcdrv_node_status_idle_by_mem(msg_chan->rx[trans_id].src_data);
        hdcdrv_node_status_idle_by_mem(msg_chan->rx[trans_id].dst_data);
        hdcdrv_node_status_idle_by_mem(msg_chan->rx[trans_id].src_ctrl);
        hdcdrv_node_status_idle_by_mem(msg_chan->rx[trans_id].dst_ctrl);

        msg_chan->rx[trans_id].src_data = NULL;
        msg_chan->rx[trans_id].dst_data = NULL;
        msg_chan->rx[trans_id].src_ctrl = NULL;
        msg_chan->rx[trans_id].dst_ctrl = NULL;
    }
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, DMA_COPY_LATENCY,
                                     msg_chan->rx[trans_id].latency_info.dma_copy_timestamp);
#endif

    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("Rx msg callback device reset. (dev_id=%u)\n", dev->dev_id);
        return;
    }

    msg_chan->rx[trans_id].status = (int)status;

    ka_wmb();
    msg_chan->dma_head = (int)trans_id;
    msg_chan->dbg_stat.hdcdrv_rx_msg_callback3++;
    session->dbg_stat.hdcdrv_rx_msg_callback3++;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&msg_chan->rx[trans_id].latency_info.rx_task_timestamp);
#endif
    ka_system_tasklet_schedule(&msg_chan->rx_task);
}

STATIC void hdcdrv_rx_msg_task_check(struct hdcdrv_msg_chan *msg_chan)
{
    struct hdcdrv_msg_chan_tasklet_status *tasklet_status = NULL;

    tasklet_status = &msg_chan->rx_task_status;

    msg_chan->dbg_stat.hdcdrv_rx_msg_task_check1++;

    if (hdcdrv_tasklet_status_check(tasklet_status) == HDCDRV_OK) {
        return;
    }

    /* no valid msg */
    if (msg_chan->rx_head == msg_chan->dma_head) {
        return;
    }

    msg_chan->dbg_stat.hdcdrv_rx_msg_task_check2++;

    /* has msg but not schedule */
    ka_system_tasklet_schedule(&msg_chan->rx_task);
}

STATIC void hdcdrv_msg_chan_tx_sq_task(unsigned long data)
{
#ifdef CFG_FEATURE_SEC_COMM_L3
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)((uintptr_t)data);

    /* trigger doorbell irq */
    devdrv_msg_ring_doorbell(msg_chan->chan);
#endif
}

STATIC void hdcdrv_msg_chan_tx_cq_task(unsigned long data)
{
#ifdef CFG_FEATURE_SEC_COMM_L3
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)((uintptr_t)data);

    /* trigger doorbell irq */
    devdrv_msg_ring_cq_doorbell(msg_chan->chan);
#endif
}

STATIC int hdcdrv_normal_dma_copy(struct hdcdrv_msg_chan *msg_chan, u32 head, const struct hdcdrv_sq_desc *sq_desc,
    struct devdrv_asyn_dma_para_info *para, enum devdrv_dma_data_type data_type)
{
    /* the receiving end cannot apply for memory, too many receive
       buffers, and waits for the upper layer to receive packets.
       Block live send.
       malloc fixed len memory to avoid memory fragmentation */
    struct hdccom_alloc_mem_para alloc_para;
    struct hdcdrv_buf_desc *rx_desc = &msg_chan->rx[head];
    struct hdcdrv_session *session = &hdc_ctrl->sessions[sq_desc->remote_session];
    int ret;

#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_VFIO_DEVICE)
    ret = hdcdrv_bandwidth_limit_check(session, hdcdrv_get_dma_direction(), (u32)sq_desc->data_len, 1);
    if (ret != HDCDRV_OK) {
        return HDCDRV_DMA_QUE_FULL;
    }
#endif

    alloc_para.len = sq_desc->data_len;
    alloc_para.wait_head = &msg_chan->wait_mem_list;
    ret = hdcdrv_rx_alloc_mem(session, &alloc_para, rx_desc);
    if (ret != HDCDRV_OK) {
        session->dbg_stat.hdcdrv_normal_dma_copy1++;
        msg_chan->dbg_stat.hdcdrv_normal_dma_copy1++;
        return HDCDRV_DMA_MEM_ALLOC_FAIL;
    }

    /* record recv buf desc */
    rx_desc->len = sq_desc->data_len;
    rx_desc->local_session = sq_desc->remote_session;
    rx_desc->remote_session = sq_desc->local_session;

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&msg_chan->rx[head].latency_info.dma_copy_timestamp);
#endif

    /* copy the packet */
    ret = devdrv_dma_async_copy_plus(msg_chan->dev_id, data_type, msg_chan->chan_id,
        (ka_dma_addr_t)sq_desc->src_data_addr, rx_desc->addr, (u32)sq_desc->data_len, hdcdrv_get_dma_direction(), para);
    if (ret == -ENOSPC) {
        hdcdrv_free_mem(session, rx_desc->buf, HDCDRV_MEMPOOL_FREE_IN_PM, rx_desc);
        ret = HDCDRV_DMA_QUE_FULL;
    } else if (ret != 0) {
        ret = HDCDRV_DMA_COPY_FAIL;
    }

    return ret;
}

STATIC u64 hdcdrv_rebuild_hash(u32 rb_side, u64 hash_va, u32 fid)
{
    u64 hash_va_new = hash_va;
    u64 fid_t = (u32)fid;

    if (rb_side == HDCDRV_RBTREE_SIDE_LOCAL) {
        hash_va_new = (fid_t << HDCDRV_FRBTREE_FID_BEG) | (hash_va & HDCDRV_HASH_VA_PIDADDR_MASK);
    }

    return hash_va_new;
}

void* hdcdrv_get_sync_mem_buf(int dev_id)
{
    return hdc_ctrl->devices[dev_id].sync_mem_buf;
}

ka_mutex_t *hdcdrv_get_sync_mem_lock(int dev_id)
{
    return &(hdc_ctrl->devices[dev_id].sync_mem_mutex);
}

static void hdcdrv_get_fnode_from_msg(struct hdcdrv_fast_node *f_node, u64 hash_va_wfid,
    const struct hdcdrv_ctrl_msg_sync_mem_info *msg, int devid)
{
    u32 i;

    f_node->fast_mem.devid = devid;
    f_node->fast_mem.phy_addr_num = msg->phy_addr_num;
    f_node->fast_mem.alloc_len = msg->alloc_len;
    f_node->fast_mem.mem_type = msg->mem_type;
    f_node->fast_mem.page_type = HDCDRV_PAGE_TYPE_NONE;
    f_node->fast_mem.hash_va = hash_va_wfid;
    f_node->pid = msg->pid;
    f_node->hash_va = hash_va_wfid;
#ifdef CFG_FEATURE_HDC_REG_MEM
    f_node->fast_mem.align_size = msg->align_size;
    f_node->fast_mem.register_inner_page_offset = msg->register_offset;
    f_node->fast_mem.user_va = msg->user_va;
#endif
    for (i = 0; i < (u32)msg->phy_addr_num; i++) {
        f_node->fast_mem.mem[i].addr = msg->mem[i].addr;
        f_node->fast_mem.mem[i].len = msg->mem[i].len;
    }
}

STATIC struct hdcdrv_fast_node *hdcdrv_remote_node_alloc(int num)
{
    struct hdcdrv_fast_node *f_node = NULL;
    struct hdcdrv_mem_f* mem = NULL;
    u64 len_mem;

    f_node = (struct hdcdrv_fast_node *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_fast_node), KA_SUB_MODULE_TYPE_2);
    if (f_node == NULL) {
        hdcdrv_err("Calling kzalloc failed.\n");
        return NULL;
    }

    len_mem = (u64)sizeof(struct hdcdrv_mem_f) * (u32)num;
    mem = (struct hdcdrv_mem_f *)hdcdrv_kvmalloc(len_mem, KA_SUB_MODULE_TYPE_2);
    if (mem == NULL) {
        hdcdrv_fast_node_free(f_node);
        hdcdrv_err("Calling kzalloc failed. (size=%lld)\n", len_mem);
        return NULL;
    }

    f_node->fast_mem.mem = mem;
    return f_node;
}

STATIC void hdcdrv_fast_node_msg_info_fill(int devid,
    const struct hdcdrv_ctrl_msg_sync_mem_info *sync_msg, struct hdcdrv_fast_node_msg_info *msg)
{
    msg->dev_id = (u32)devid;
    msg->pid = (u64)sync_msg->pid;
    msg->fid = (u32)(sync_msg->hash_va >> HDCDRV_FRBTREE_FID_BEG) & HDCDRV_FRBTREE_FID_MASK;

    msg->len = sync_msg->alloc_len;
#ifdef CFG_FEATURE_HDC_REG_MEM
    msg->va_addr = sync_msg->user_va;
    msg->process_stage = HDCDRV_SEARCH_NODE_REMOTE;
#endif
    msg->hash_val = sync_msg->hash_va;
}

STATIC int hdcdrv_add_mem_info(int devid, u32 fid, u32 rb_side, struct hdcdrv_ctrl_msg_sync_mem_info *msg)
{
    struct hdcdrv_fast_node *f_node = NULL;
    int ret;
    u32 fid_tmp;
    bool fast_node_flg = false;
    struct hdcdrv_fast_node_msg_info node_msg;
    struct hdcdrv_dev_fmem *dev_fmem = hdcdrv_get_dev_fmem_sep(devid);
    ka_rb_root_t* rbtree = hdcdrv_get_rbtree(dev_fmem, rb_side);
    u64 hash_va_wfid = hdcdrv_rebuild_hash(rb_side, msg->hash_va, fid);

    f_node = hdcdrv_fast_node_search_timeout(&dev_fmem->rb_lock, rbtree,
                                             hash_va_wfid, HDCDRV_NODE_WAIT_TIME_MAX);
    if ((f_node == NULL) && (msg->flag == HDCDRV_ADD_REGISTER_FLAG)) {
        hdcdrv_fast_node_msg_info_fill(devid, msg, &node_msg);
        f_node = hdcdrv_fast_node_search_from_new_tree(rb_side, HDCDRV_NODE_FREE_WAIT_TIME, &node_msg);
        fast_node_flg = true;
    }
    if (f_node != NULL) {
        hdcdrv_warn("This fast node has exit, free it firstly.\n");
        if (fast_node_flg) {
            hdcdrv_fast_node_erase_from_new_tree((u64)f_node->pid, fid, devid, rb_side, f_node);
        } else {
            hdcdrv_fast_node_erase(&dev_fmem->rb_lock, rbtree, f_node);
        }
        hdcdrv_node_status_init(f_node);
        hdcdrv_kvfree(f_node->fast_mem.mem, KA_SUB_MODULE_TYPE_2);
        hdcdrv_fast_node_free(f_node);
    }

    f_node = hdcdrv_remote_node_alloc(msg->phy_addr_num);
    if (f_node == NULL) {
        hdcdrv_err("Calling kzalloc failed. (dev=%d)\n", devid);
        return HDCDRV_MEM_ALLOC_FAIL;
    }

    hdcdrv_get_fnode_from_msg(f_node, hash_va_wfid, msg, devid);

    /* if add rb is not remote, so is vm mode, map iova and send to remote. */
    if (rb_side == HDCDRV_RBTREE_SIDE_LOCAL) {
        ret = hdcdrv_iova_fmem_map((u32)devid, fid, &(f_node->fast_mem));
        if (ret != HDCDRV_OK) {
            hdcdrv_err("Calling hdcdrv_iova_fmem_map failed. (dev=%d; fid=%u; ret=%d)\n", devid, fid, ret);
            goto IOVA_FAIL;
        }

        ret = hdcdrv_send_mem_info(&(f_node->fast_mem), devid, HDCDRV_ADD_FLAG);
        if (ret != HDCDRV_OK) {
            hdcdrv_err("Calling hdcdrv_send_mem_info failed. (dev=%d; fid=%u; ret=%d)\n", devid, fid, ret);
            goto ADD_REMOTE_FAIL;
        }
    }

    if (msg->flag == HDCDRV_ADD_REGISTER_FLAG) {
        fid_tmp = (u32)((msg->hash_va >> HDCDRV_FRBTREE_FID_BEG) & HDCDRV_FRBTREE_FID_MASK);
        ret = hdcdrv_fast_node_insert_new_tree(devid, (u64)msg->pid, fid_tmp, rb_side, f_node);
        if (ret != HDCDRV_OK) {
            hdcdrv_err_limit("fast insert fail. (devid=%d; msg_type=%d; hash_va=0x%llx; len=%d; pid=0x%llx)\n",
                devid, msg->type, msg->hash_va, msg->alloc_len, msg->pid);
            goto INS_FAIL;
        }
    } else {
        ret = hdcdrv_fast_node_insert(&dev_fmem->rb_lock, rbtree, f_node, HDCDRV_SEARCH_WITH_HASH);
        if (ret != HDCDRV_OK) {
            hdcdrv_err_limit("Insert failed, node exist. (devid=%d; msg_type=%d; flag=%d; mem_num=%d; pid=0x%llx)\n",
                devid, msg->type, msg->flag, msg->phy_addr_num, msg->pid);
            goto INS_FAIL;
        }
    }
    return HDCDRV_OK;

INS_FAIL:
    if (rb_side == HDCDRV_RBTREE_SIDE_LOCAL) {
        ret = hdcdrv_send_mem_info(&(f_node->fast_mem), devid, HDCDRV_DEL_FLAG);
    }
ADD_REMOTE_FAIL:
    if (rb_side == HDCDRV_RBTREE_SIDE_LOCAL) {
        hdcdrv_iova_fmem_unmap((u32)devid, fid, &(f_node->fast_mem), (u32)f_node->fast_mem.phy_addr_num);
    }
IOVA_FAIL:
    hdcdrv_kvfree(f_node->fast_mem.mem, KA_SUB_MODULE_TYPE_2);
    hdcdrv_fast_node_free(f_node);
    return ret;
}

STATIC int hdcdrv_del_mem_info(int devid, u32 fid, u32 rb_side, const struct hdcdrv_ctrl_msg_sync_mem_info *msg)
{
    struct hdcdrv_fast_node *f_node = NULL;
    struct hdcdrv_dev_fmem *dev_fmem = hdcdrv_get_dev_fmem_sep(devid);
    ka_rb_root_t* rbtree = hdcdrv_get_rbtree(dev_fmem, rb_side);
    u64 hash_va_wfid = hdcdrv_rebuild_hash(rb_side, msg->hash_va, fid);
    int ret;
    bool fast_node_flg = false;
    struct hdcdrv_fast_node_msg_info node_msg;

    f_node = hdcdrv_fast_node_search_timeout(&dev_fmem->rb_lock, rbtree,
                                             hash_va_wfid, HDCDRV_NODE_WAIT_TIME_MAX);
    if ((f_node == NULL) && (msg->flag == HDCDRV_DEL_REGISTER_FLAG)) {
        hdcdrv_fast_node_msg_info_fill(devid, msg, &node_msg);
        f_node = hdcdrv_fast_node_search_from_new_tree(rb_side, HDCDRV_NODE_FREE_WAIT_TIME, &node_msg);
        fast_node_flg = true;
    }
    if (f_node == NULL) {
        hdcdrv_warn("Calling hdcdrv_fast_node_search_timeout. (dev=%d rb_side=%d hash=0x%llx)\n",
            devid, rb_side, hash_va_wfid);
#ifdef CFG_FEATURE_HDC_REG_MEM
        return HDCDRV_OK;
#else
        return HDCDRV_F_NODE_SEARCH_FAIL;
#endif
    }

    /* if add rb is not remote, so is vm mode, map iova and send to remote. */
    if (rb_side == HDCDRV_RBTREE_SIDE_LOCAL) {
        ret = hdcdrv_send_mem_info(&(f_node->fast_mem), devid, HDCDRV_DEL_FLAG);
        if (ret != HDCDRV_OK) {
            hdcdrv_err("Calling hdcdrv_send_mem_info failed. (dev=%d; fid=%u; ret=%d)\n", devid, fid, ret);
        }
        hdcdrv_iova_fmem_unmap((u32)devid, fid, &(f_node->fast_mem), (u32)f_node->fast_mem.phy_addr_num);
    }

    if (fast_node_flg) {
        hdcdrv_fast_node_erase_from_new_tree((u64)f_node->pid, fid, devid, rb_side, f_node);
    } else {
        hdcdrv_fast_node_erase(&dev_fmem->rb_lock, rbtree, f_node);
    }
    hdcdrv_node_status_init(f_node);
    hdcdrv_kvfree(f_node->fast_mem.mem, KA_SUB_MODULE_TYPE_2);
    hdcdrv_fast_node_free(f_node);

    return HDCDRV_OK;
}

int hdcdrv_set_mem_info(int devid, u32 fid, u32 rb_side, struct hdcdrv_ctrl_msg_sync_mem_info *msg)
{
    if ((devid >= hdcdrv_get_max_support_dev())
#ifdef CFG_FEATURE_VFIO
        || (fid >= VMNG_VDEV_MAX_PER_PDEV)
#endif
        ) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d; fid=%u)\n", devid, fid);
        return HDCDRV_PARA_ERR;
    }

    if (((u32)msg->phy_addr_num > HDCDRV_MEM_MAX_PHY_NUM) || (msg->phy_addr_num == 0)) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d; phy_addr_num=%d)\n", devid, msg->phy_addr_num);
        return HDCDRV_PARA_ERR;
    }

    if ((msg->flag == HDCDRV_ADD_FLAG) || (msg->flag == HDCDRV_ADD_REGISTER_FLAG)) {
        return hdcdrv_add_mem_info(devid, fid, rb_side, msg);
    } else {
        return hdcdrv_del_mem_info(devid, fid, rb_side, msg);
    }
}

STATIC void hdcdrv_fast_mem_sep_uninit(ka_task_spinlock_t *lock, const ka_rb_root_t *root)
{
    ka_rb_node_t *node = NULL;
    struct hdcdrv_fast_node *f_node = NULL;
    int ret = 0;
    int devid;
    u32 fid;

    /* only uninit free, suspend status not free */
    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return;
    }

    ka_task_spin_lock_bh(lock);

    node = ka_base_rb_first(root);
    while (node != NULL) {
        f_node = ka_base_rb_entry(node, struct hdcdrv_fast_node, node);
        node = ka_base_rb_next(node);

        devid = f_node->fast_mem.devid;
        fid = (u32)(f_node->fast_mem.hash_va >> HDCDRV_FRBTREE_FID_BEG);
        ka_task_spin_unlock_bh(lock);
        ret = hdcdrv_send_mem_info(&(f_node->fast_mem), devid, HDCDRV_DEL_FLAG);
        if (ret != HDCDRV_OK) {
            hdcdrv_err("Calling hdcdrv_send_mem_info failed. (dev=%d; fid=%u; ret=%d)\n", devid, fid, ret);
        }
        hdcdrv_iova_fmem_unmap((u32)devid, fid, &(f_node->fast_mem), (u32)f_node->fast_mem.phy_addr_num);
        ka_task_spin_lock_bh(lock);
    }

    ka_task_spin_unlock_bh(lock);
}

int hdcdrv_mem_adapter(const struct hdcdrv_fast_addr_info *src_info, const struct hdcdrv_fast_addr_info *dst_info,
    struct devdrv_dma_node *node, int *node_idx, int len)
{
    int idx;
    u32 total_len;
    enum devdrv_dma_direction dir;
    struct hdcdrv_fast_mem *src = src_info->f_mem;
    struct hdcdrv_fast_mem *dst = dst_info->f_mem;
#ifdef CFG_FEATURE_HDC_REG_MEM
    u32 i = src_info->page_start_idx;
    u32 j = dst_info->page_start_idx;
    u32 src_offset = src_info->page_start_idx == 0 ?\
        (src_info->send_inner_page_offset - src->register_inner_page_offset) : src_info->send_inner_page_offset;
    u32 dst_offset = dst_info->page_start_idx == 0 ?\
        (dst_info->send_inner_page_offset - dst->register_inner_page_offset) : dst_info->send_inner_page_offset;
    ka_dma_addr_t src_addr = (ka_dma_addr_t)src->mem[i].addr + src_offset;
    ka_dma_addr_t dst_addr = (ka_dma_addr_t)dst->mem[j].addr + dst_offset;
    u32 src_len = src->mem[i].len - src_offset;
    u32 dst_len = dst->mem[j].len - dst_offset;
#else
    u32 i = 0;
    u32 j = 0;
    ka_dma_addr_t src_addr = (ka_dma_addr_t)src->mem[i].addr;
    ka_dma_addr_t dst_addr = (ka_dma_addr_t)dst->mem[j].addr;
    u32 src_len = src->mem[i].len;
    u32 dst_len = dst->mem[j].len;
#endif
    i++;
    j++;

    dir = hdcdrv_get_dma_direction();
    total_len = (u32)len;
    if ((total_len > dst->alloc_len) || (total_len > src->alloc_len)) {
        hdcdrv_err("Input pararmeter is error. (send_len=%d; src_len %d; dst_len=%d)\n",
            total_len, src->alloc_len, dst->alloc_len);
        return HDCDRV_PARA_ERR;
    }

    while (total_len > 0) {
        idx = *node_idx;
        if (idx >= HDCDRV_MAX_DMA_NODE) {
            hdcdrv_err("Need too many nodes, out of the range.\n");
            return HDCDRV_PARA_ERR;
        }

        if (src_len == dst_len) {
            if (total_len < src_len) {
                src_len = total_len;
            }

            node[idx].size = src_len;
            node[idx].src_addr = src_addr;
            node[idx].dst_addr = dst_addr;
            total_len -= src_len;

            if (total_len == 0) {
                goto FINISH;
            }

            src_addr = (ka_dma_addr_t)src->mem[i].addr;
            src_len = src->mem[i].len;
            i++;

            dst_addr = (ka_dma_addr_t)dst->mem[j].addr;
            dst_len = dst->mem[j].len;
            j++;
        } else if (src_len < dst_len) {
            if (total_len < src_len) {
                src_len = total_len;
            }

            node[idx].size = src_len;
            node[idx].src_addr = src_addr;
            node[idx].dst_addr = dst_addr;
            total_len -= src_len;

            if (total_len == 0) {
                goto FINISH;
            }

            dst_addr += src_len;
            dst_len -= src_len;

            src_addr = (ka_dma_addr_t)src->mem[i].addr;
            src_len = src->mem[i].len;
            i++;
        } else {
            if (total_len < dst_len) {
                dst_len = total_len;
            }

            node[idx].size = dst_len;
            node[idx].src_addr = src_addr;
            node[idx].dst_addr = dst_addr;
            total_len -= dst_len;

            if (total_len == 0) {
                goto FINISH;
            }

            src_addr += dst_len;
            src_len -= dst_len;

            dst_addr = (ka_dma_addr_t)dst->mem[j].addr;
            dst_len = dst->mem[j].len;
            j++;
        }

    FINISH:
        node[idx].direction = dir;
        *node_idx = (*node_idx) + 1;
    }

    return HDCDRV_OK;
}

STATIC int hdcdrv_get_dma_node_info(struct hdcdrv_fast_addr_info *src, struct hdcdrv_fast_addr_info *dst,
    struct hdcdrv_msg_chan *msg_chan, int *node_idx, int len)
{
    if ((src->f_mem != NULL) && (dst->f_mem != NULL)) {
        return hdcdrv_mem_adapter(src, dst, msg_chan->node, node_idx, len);
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_set_node_to_idle(struct hdcdrv_fast_mem_info *fmem_info)
{
    hdcdrv_node_status_idle_by_mem(fmem_info->src_data.f_mem);
    hdcdrv_node_status_idle_by_mem(fmem_info->dst_data.f_mem);
    hdcdrv_node_status_idle_by_mem(fmem_info->src_ctrl.f_mem);
    hdcdrv_node_status_idle_by_mem(fmem_info->dst_ctrl.f_mem);
}

STATIC int hdcdrv_fast_mem_valid_check(const struct hdcdrv_sq_desc *sq_desc, const struct hdcdrv_fast_mem *mem_src,
    const struct hdcdrv_fast_mem *mem_dst)
{
    if ((mem_src != NULL) && (mem_dst == NULL)) {
        hdcdrv_err("Fast dst buffer error. (local_session_id=%d; remote_session_id=%d; dst_buf=0x%llx)\n",
            sq_desc->remote_session, sq_desc->local_session, sq_desc->dst_ctrl_addr);
        return HDCDRV_PARA_ERR;
    }

    if ((mem_src == NULL) && (mem_dst != NULL)) {
        hdcdrv_err("Fast src buffer error. (local_session_id=%d; remote_session_id=%d; src_buf=0x%llx)\n",
            sq_desc->remote_session, sq_desc->local_session, sq_desc->src_ctrl_addr);
        return HDCDRV_PARA_ERR;
    }

    if ((mem_dst != NULL) && (mem_dst->dma_map == 0)) {
        hdcdrv_err("Fast dst buffer not map. (local_session_id=%d; remote_session_id=%d; dst_data_buf=0x%llx)\n",
            sq_desc->remote_session, sq_desc->local_session, sq_desc->dst_ctrl_addr);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}
STATIC int hdcdrv_fast_mem_check(struct hdcdrv_sq_desc *sq_desc, struct hdcdrv_fast_mem_info *fmem_info)
{
    int ret;
    const struct hdcdrv_fast_mem *mem_src_data = fmem_info->src_data.f_mem;
    const struct hdcdrv_fast_mem *mem_src_ctrl = fmem_info->src_ctrl.f_mem;
    const struct hdcdrv_fast_mem *mem_dst_data = fmem_info->dst_data.f_mem;
    const struct hdcdrv_fast_mem *mem_dst_ctrl = fmem_info->dst_ctrl.f_mem;

    if ((mem_src_data == NULL) && (mem_dst_data == NULL) && (mem_src_ctrl == NULL) && (mem_dst_ctrl == NULL)) {
        hdcdrv_err("Input pararmeter is error. (local_session_id=%d; remote_session_id=%d; "
            "remote_data_addr=0x%llx; remote_ctrl_addr=0x%llx; local_data_addr=0x%llx; local_ctrl_addr=0x%llx)\n",
            sq_desc->remote_session, sq_desc->local_session, sq_desc->src_data_addr,
            sq_desc->src_ctrl_addr, sq_desc->dst_data_addr, sq_desc->dst_ctrl_addr);
        return HDCDRV_PARA_ERR;
    }

    ret = hdcdrv_fast_mem_valid_check(sq_desc, mem_src_data, mem_dst_data);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling check data failed.\n");
        return HDCDRV_PARA_ERR;
    }

    ret = hdcdrv_fast_mem_valid_check(sq_desc, mem_src_ctrl, mem_dst_ctrl);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling check ctrl failed.\n");
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

void hdcdrv_node_msg_info_fill(u32 pid, u32 fid, int len, u64 addr, u32 process_stage,
    struct hdcdrv_fast_node_msg_info *msg)
{
    msg->pid = (u64)pid;
    msg->fid = fid;
    msg->len = (u32)len;
    msg->va_addr = addr;
    msg->hash_val = hdcdrv_get_hash(addr, (u64)pid, fid);
    msg->process_stage = process_stage;
}

STATIC int hdcdrv_get_fast_mem_info(struct hdcdrv_sq_desc *sq_desc, struct hdcdrv_fast_mem_info *fmem_info)
{
    struct hdcdrv_dev_fmem *dev_fmem_lo = NULL;
    struct hdcdrv_dev_fmem *dev_fmem_re = NULL;
    int ret = HDCDRV_OK;
    int devid;
    u32 pid;
    u32 fid;
    struct hdcdrv_fast_node_msg_info node_msg = {0};

    devid = hdc_ctrl->sessions[sq_desc->remote_session].dev_id;
    pid = (u32)hdc_ctrl->sessions[sq_desc->remote_session].owner_pid;
    fid = (hdc_ctrl->sessions[sq_desc->remote_session].owner == HDCDRV_SESSION_OWNER_VM) ?
        hdc_ctrl->sessions[sq_desc->remote_session].local_fid : 0;

    dev_fmem_lo = hdcdrv_get_dev_fmem_ex(devid, fid, HDCDRV_RBTREE_SIDE_LOCAL);
    dev_fmem_re = hdcdrv_get_dev_fmem_sep(devid);

    node_msg.dev_id = (u32)devid;
    if (sq_desc->src_data_addr != 0) {
        hdcdrv_node_msg_info_fill(sq_desc->src_pid, sq_desc->src_fid, sq_desc->data_len, sq_desc->src_data_addr,
                                  HDCDRV_SEARCH_NODE_SENDRECV, &node_msg);
        hdcdrv_get_fast_mem(dev_fmem_re, HDCDRV_FAST_MEM_TYPE_TX_DATA, &node_msg, &fmem_info->src_data);
    }

    if (sq_desc->src_ctrl_addr != 0) {
        hdcdrv_node_msg_info_fill(sq_desc->src_pid, sq_desc->src_fid, sq_desc->ctrl_len, sq_desc->src_ctrl_addr,
                                  HDCDRV_SEARCH_NODE_SENDRECV, &node_msg);
        hdcdrv_get_fast_mem(dev_fmem_re, HDCDRV_FAST_MEM_TYPE_TX_CTRL, &node_msg, &fmem_info->src_ctrl);
    }

    if (sq_desc->dst_data_addr != 0) {
        hdcdrv_node_msg_info_fill(pid, fid, sq_desc->data_len, sq_desc->dst_data_addr,
                                  HDCDRV_SEARCH_NODE_SENDRECV, &node_msg);
        hdcdrv_get_fast_mem(dev_fmem_lo, HDCDRV_FAST_MEM_TYPE_RX_DATA, &node_msg, &fmem_info->dst_data);
    }

    if (sq_desc->dst_ctrl_addr != 0) {
        hdcdrv_node_msg_info_fill(pid, fid, sq_desc->ctrl_len, sq_desc->dst_ctrl_addr,
                                  HDCDRV_SEARCH_NODE_SENDRECV, &node_msg);
        hdcdrv_get_fast_mem(dev_fmem_lo, HDCDRV_FAST_MEM_TYPE_RX_CTRL, &node_msg, &fmem_info->dst_ctrl);
    }

    if (hdcdrv_fast_mem_check(sq_desc, fmem_info) != HDCDRV_OK) {
        hdcdrv_set_node_to_idle(fmem_info);
        ret = HDCDRV_F_NODE_SEARCH_FAIL;
    }

    return ret;
}

/* fast node unlock is in hdcdrv_rx_msg_callback function */
STATIC int hdcdrv_fast_dma_copy(struct hdcdrv_msg_chan *msg_chan, u32 head, struct hdcdrv_sq_desc *sq_desc,
    struct devdrv_asyn_dma_para_info *para, enum devdrv_dma_data_type data_type)
{
    struct hdcdrv_fast_mem_info fmem_info;
    struct hdcdrv_buf_desc *rx_desc = &msg_chan->rx[head];
#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_VFIO_DEVICE)
    struct hdcdrv_session *session = &hdc_ctrl->sessions[sq_desc->remote_session];
#endif
    int node_idx = 0;
    int ret;

    fmem_info.src_data.f_mem = NULL;
    fmem_info.dst_data.f_mem = NULL;
    fmem_info.src_ctrl.f_mem = NULL;
    fmem_info.dst_ctrl.f_mem = NULL;

    ret = hdcdrv_get_fast_mem_info(sq_desc, &fmem_info);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_get_fast_mem_info failed.\n");
        return ret;
    }

    if (msg_chan->node == NULL) {
        msg_chan->node = hdcdrv_kvzalloc((u64)HDCDRV_MAX_DMA_NODE * sizeof(struct devdrv_dma_node),
            KA_GFP_ATOMIC | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT, KA_SUB_MODULE_TYPE_2);
        if (msg_chan->node == NULL) {
            hdcdrv_err("Alloc msg_chan->node failed. (dev=%u; size=%u)\n", msg_chan->dev_id,
                (u32)(HDCDRV_MAX_DMA_NODE * sizeof(struct devdrv_dma_node)));
            ret = HDCDRV_MEM_ALLOC_FAIL;
            goto CHECK_ERR;
        }
    }

    rx_desc->dst_data_addr = sq_desc->dst_data_addr;
    rx_desc->len = sq_desc->data_len;
    rx_desc->dst_ctrl_addr = sq_desc->dst_ctrl_addr;
    rx_desc->ctrl_len = sq_desc->ctrl_len;
    rx_desc->local_session = sq_desc->remote_session;
    rx_desc->remote_session = sq_desc->local_session;

    rx_desc->src_data = fmem_info.src_data.f_mem;
    rx_desc->dst_data = fmem_info.dst_data.f_mem;
    rx_desc->src_ctrl = fmem_info.src_ctrl.f_mem;
    rx_desc->dst_ctrl = fmem_info.dst_ctrl.f_mem;

    if (hdcdrv_get_dma_node_info(&fmem_info.src_data, &fmem_info.dst_data, msg_chan, &node_idx,
        sq_desc->data_len) != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_get_dma_node_info failed. (dev_id=%u)\n", msg_chan->dev_id);
        ret = HDCDRV_MEM_NOT_MATCH;
        goto CHECK_ERR;
    }

    if (hdcdrv_get_dma_node_info(&fmem_info.src_ctrl, &fmem_info.dst_ctrl, msg_chan, &node_idx,
        sq_desc->ctrl_len) != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_get_dma_node_info failed. (dev_id=%u)\n", msg_chan->dev_id);
        ret = HDCDRV_MEM_NOT_MATCH;
        goto CHECK_ERR;
    }

#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_VFIO_DEVICE)
    ret = hdcdrv_bandwidth_limit_check(session, hdcdrv_get_dma_direction(),
                                       (u32)(sq_desc->data_len + sq_desc->ctrl_len), (u32)node_idx);
    if (ret != HDCDRV_OK) {
        ret = HDCDRV_DMA_QUE_FULL;
        goto CHECK_ERR;
    }
#endif

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&msg_chan->rx[head].latency_info.dma_copy_timestamp);
#endif
    ret = devdrv_dma_async_link_copy_plus(msg_chan->dev_id, data_type, msg_chan->chan_id, msg_chan->node,
                                          (u32)node_idx, para);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling devdrv_dma_async_link_copy_plus failed. (dev_id=%u)\n", msg_chan->dev_id);
        ret = (ret == -ENOSPC) ? HDCDRV_DMA_QUE_FULL : HDCDRV_DMA_COPY_FAIL;
        goto CHECK_ERR;
    }

    return HDCDRV_OK;

CHECK_ERR:
    hdcdrv_set_node_to_idle(&fmem_info);

    msg_chan->rx[head].src_data = NULL;
    msg_chan->rx[head].dst_data = NULL;
    msg_chan->rx[head].src_ctrl = NULL;
    msg_chan->rx[head].dst_ctrl = NULL;

    return ret;
}

STATIC int hdcdrv_rx_msg_notify_task_handle(struct hdcdrv_msg_chan *msg_chan,
    struct hdcdrv_sq_desc *sq_desc, u32 head)
{
    struct devdrv_asyn_dma_para_info para;
    struct hdcdrv_session *session;
    long ret;

    para.interrupt_and_attr_flag = DEVDRV_LOCAL_IRQ_FLAG;
    para.priv = (void *)msg_chan;
    para.finish_notify = hdcdrv_rx_msg_callback;

    msg_chan->rx[head].local_session = sq_desc->remote_session;
    msg_chan->rx[head].remote_session = sq_desc->local_session;

    if ((ret = hdcdrv_session_inner_check(sq_desc->remote_session, sq_desc->inner_checker)) != HDCDRV_OK) {
        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task4++;

        hdcdrv_warn("Calling hdcdrv_session_inner_check not success. (dev=%u; session_id=%d)\n",
            msg_chan->dev_id, sq_desc->remote_session);
        msg_chan->rx[head].status = HDCDRV_SESSION_HAS_CLOSED;
        msg_chan->rx[head].skip_flag = HDCDRV_VALID;
        goto JUMP_NEXT;
    }

    session = &hdc_ctrl->sessions[sq_desc->remote_session];
    // if normal, here equals msg->dbg_stat.hdcdrv_rx_msg_notify_task2++
    session->dbg_stat.hdcdrv_rx_msg_notify_task2++;
    msg_chan->rx[head].inner_checker = sq_desc->inner_checker;
    para.trans_id = head;

    if (msg_chan->type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
        ret = hdcdrv_normal_dma_copy(msg_chan, head, sq_desc, &para, msg_chan->data_type);
        if (ret == HDCDRV_DMA_MEM_ALLOC_FAIL) {
            /* wait schedule again when free mem */
            session->dbg_stat.hdcdrv_rx_msg_notify_task5++;
            msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task5++;
            return HDCDRV_ERR;
        }
    } else {
        ret = hdcdrv_fast_dma_copy(msg_chan, head, sq_desc, &para, msg_chan->data_type);
    }

    if (ret == HDCDRV_DMA_QUE_FULL) {
        /* dma que full, wait sched again in the following cases:
           1. next send request in hdcdrv_rx_msg_notify
           2. dma completion interrupt in hdcdrv_rx_msg_callback
           3. received monitoring work in hdcdrv_rx_msg_notify_task_check  */
        session->dbg_stat.hdcdrv_rx_msg_notify_task6++;
        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task6++;
        msg_chan->dma_need_submit_flag = HDCDRV_VALID;
        return HDCDRV_ERR;
    } else if (ret != HDCDRV_OK) {
        session->dbg_stat.hdcdrv_rx_msg_notify_task7++;
        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task7++;
        hdcdrv_err("Dma copy failed. (ret=%ld; msg_chan_type=%d; device=%u; notify_head=%d; ctrl_len=%d; data_len=%d; "
            "l_session=%d; r_session=%d; src_pid=%u)\n", ret, msg_chan->type, msg_chan->dev_id, head, sq_desc->ctrl_len,
            sq_desc->data_len, sq_desc->local_session, sq_desc->remote_session, sq_desc->src_pid);

        msg_chan->rx[head].skip_flag = HDCDRV_VALID;
        msg_chan->rx[head].status = (int)ret;
        goto JUMP_NEXT;
    }

#ifdef CFG_FEATURE_MIRROR
    ka_mm_dma_sync_single_for_cpu(msg_chan->dev, msg_chan->rx[head].addr, (u32)msg_chan->rx[head].len, KA_DMA_FROM_DEVICE);
#endif

    msg_chan->submit_dma_head = (int)head;
    msg_chan->rx[head].skip_flag = HDCDRV_INVALID;
    session->dbg_stat.hdcdrv_rx_msg_notify_task8++;
JUMP_NEXT:
    if (msg_chan->rx[head].skip_flag == HDCDRV_VALID) {
        if (msg_chan->submit_dma_head == msg_chan->dma_head) {
            msg_chan->dma_head = (int)head;
            msg_chan->submit_dma_head = (int)head;
            ka_system_tasklet_schedule(&msg_chan->rx_task);
        }
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_rx_msg_record_resq_time(struct hdcdrv_msg_chan *msg_chan)
{
    u32 resq_time;

    if (ka_jiffies < msg_chan->hdcdrv_rx_stamp) {
        resq_time = 0;
    } else {
        resq_time = ka_system_jiffies_to_msecs(ka_jiffies - msg_chan->hdcdrv_rx_stamp);
    }
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_update_latency(msg_chan->chan_id, RX_RECV_SCHE_LATENCY, msg_chan->hdcdrv_rx_stamp);
#endif
    msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task_delay_new = resq_time;
    if (resq_time > HDCDRV_TIME_COMPARE_2MS) {
        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task_delay_over2ms++;
    }
    if (resq_time > HDCDRV_TIME_COMPARE_4MS) {
        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task_delay_over4ms++;
    }
    if (resq_time > msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task_delay_max) {
        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task_delay_max = resq_time;
    }
}

STATIC void hdcdrv_rx_msg_notify_proc(struct hdcdrv_msg_chan *msg_chan)
{
    struct hdcdrv_sq_desc *sq_desc = NULL;
    struct hdcdrv_sq_desc sq_desc_tmp = {0};
    u32 retry_cnt = 0;
    u32 head = 0;
    int cnt = 0;
    long ret;

    msg_chan->dma_need_submit_flag = HDCDRV_INVALID;

    msg_chan->rx_notify_task_status.schedule_in++;
    msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task1++;
    hdcdrv_rx_msg_record_resq_time(msg_chan);
    do {
        /* Reach the threshold and schedule out */
        if (cnt >= HDCDRV_RX_BUDGET) {
            msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task3++;
            hdcdrv_rx_msg_schedule_task(msg_chan);
            break;
        }

        sq_desc = hdcdrv_get_r_sq_desc(msg_chan->chan, &head);
        if ((sq_desc == NULL) || (sq_desc->valid != HDCDRV_VALID)) {
            break;
        }

        if (sq_desc->desc_crc != hdcdrv_calculate_crc((unsigned char *)sq_desc, HDCDRV_SQ_DESC_CRC_LEN)) {
            if (retry_cnt < HDC_CRC_CHECH_RETRY_CNT) {
                retry_cnt++;
                continue;
            }
            msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task9++;
        }
        ka_rmb();
        sq_desc_tmp = *sq_desc;
        ka_wmb();
        sq_desc->remote_session = HDCDRV_SESSION_FD_INVALID;
        sq_desc->local_session = HDCDRV_SESSION_FD_INVALID;
        sq_desc->valid = HDCDRV_INVALID;
        ka_wmb();

        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task2++;

        ret = hdcdrv_rx_msg_notify_task_handle(msg_chan, &sq_desc_tmp, head);
        if (ret != HDCDRV_OK) {
            sq_desc->remote_session = sq_desc_tmp.remote_session;
            sq_desc->local_session = sq_desc_tmp.local_session;
            ka_wmb();
            sq_desc->valid = HDCDRV_VALID;
            break;
        }

        msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task8++;

        hdcdrv_move_r_sq_desc(msg_chan->chan);
        cnt++;
        ka_wmb();
    } while (1);
}

STATIC void hdcdrv_rx_msg_notify_work(ka_work_struct_t *p_work)
{
    struct hdcdrv_msg_chan *msg_chan = ka_container_of(p_work, struct hdcdrv_msg_chan, rx_notify_work);
    hdcdrv_rx_msg_notify_proc(msg_chan);
}

STATIC void hdcdrv_rx_msg_notify_task(unsigned long data)
{
    struct hdcdrv_msg_chan *msg_chan = (struct hdcdrv_msg_chan *)((uintptr_t)data);
    hdcdrv_rx_msg_notify_proc(msg_chan);
}

void hdcdrv_rx_msg_notify(void *chan)
{
    struct hdcdrv_dev *dev = NULL;
    struct hdcdrv_msg_chan *msg_chan = hdcdrv_get_msg_chan_priv(chan);

    if (msg_chan == NULL) {
        hdcdrv_err("Input pararmeter is error.");
        return;
    }

    dev = &hdc_ctrl->devices[msg_chan->dev_id];

    msg_chan->dbg_stat.hdcdrv_rx_msg_notify1++;

    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("Rx msg notify device reset. (dev_id=%u)\n", dev->dev_id);
        return;
    }

    msg_chan->hdcdrv_rx_stamp = ka_jiffies;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&msg_chan->hdcdrv_rx_stamp);
#endif
    ka_wmb();
    hdcdrv_rx_msg_schedule_task(msg_chan);
}

STATIC void hdcdrv_rx_msg_notify_task_check(struct hdcdrv_msg_chan *msg_chan)
{
    struct hdcdrv_msg_chan_tasklet_status *tasklet_status = NULL;
    struct hdcdrv_sq_desc *sq_desc = NULL;
    u32 head = 0;

    tasklet_status = &msg_chan->rx_notify_task_status;

    msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task_check1++;

    if (msg_chan->dma_need_submit_flag == HDCDRV_VALID) {
        hdcdrv_rx_msg_schedule_task(msg_chan);
        return;
    }

    if (hdcdrv_tasklet_status_check(tasklet_status) == HDCDRV_OK) {
        return;
    }

    sq_desc = hdcdrv_get_r_sq_desc(msg_chan->chan, &head);
    if (sq_desc == NULL) {
        return;
    }

    /* no valid msg */
    if (sq_desc->valid != HDCDRV_VALID) {
        return;
    }
    ka_rmb();
    msg_chan->dbg_stat.hdcdrv_rx_msg_notify_task_check2++;

    /* there has msg but not schedule */
    hdcdrv_rx_msg_schedule_task(msg_chan);
}

STATIC void hdcdrv_clear_session_rx_buf(int session_fd)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_buf_desc *buf_desc = NULL;
    struct hdcdrv_msg_chan *msg_chan = NULL;
    struct hdcdrv_msg_chan *fast_msg_chan = NULL;

    /* Release the memory of a locally unclosed session */
    session = &hdc_ctrl->sessions[session_fd];

    if (session->chan_id < hdc_ctrl->devices[session->dev_id].normal_chan_num) {
        msg_chan = hdc_ctrl->devices[session->dev_id].msg_chan[session->chan_id];
    }

    if (session->fast_chan_id < (u32)hdc_ctrl->devices[session->dev_id].msg_chan_cnt) {
        fast_msg_chan = hdc_ctrl->devices[session->dev_id].msg_chan[session->fast_chan_id];
    }

    ka_task_spin_lock_bh(&session->lock);
    while (session->normal_rx.head != session->normal_rx.tail) {
        buf_desc = &session->normal_rx.rx_list[session->normal_rx.head];
        session->normal_rx.head = (session->normal_rx.head + 1) % HDCDRV_SESSION_RX_LIST_MAX_PKT;
        hdcdrv_free_mem(session, buf_desc->buf, HDCDRV_MEMPOOL_FREE_IN_PM, buf_desc);
        hdcdrv_desc_clear(buf_desc);
    }

    session->normal_rx.head = 0;
    session->normal_rx.tail = 0;
    /* fast session clear */
    session->fast_rx.head = 0;
    session->fast_rx.tail = 0;

    if (msg_chan != NULL) {
        if (msg_chan->rx_trigger_flag == HDCDRV_VALID) {
            msg_chan->rx_trigger_flag = HDCDRV_INVALID;
            ka_system_tasklet_schedule(&msg_chan->rx_task);
        }
    }

    if (fast_msg_chan != NULL) {
        if (fast_msg_chan->rx_trigger_flag == HDCDRV_VALID) {
            fast_msg_chan->rx_trigger_flag = HDCDRV_INVALID;
            ka_system_tasklet_schedule(&fast_msg_chan->rx_task);
        }
    }
    ka_task_spin_unlock_bh(&session->lock);
}

STATIC int hdcdrv_alloc_session(int service_type, int run_env, int root_privilege, u64 check_pid)
{
    int i, max, base, fd;
    int session_fd = -1;
    int conn_feature = hdcdrv_get_service_conn_feature(service_type);
    int *cur = NULL;

    ka_task_mutex_lock(&hdc_ctrl->mutex);

AGAIN:
    if (conn_feature == HDCDRV_SERVICE_LONG_CONN) {
        max = (int)HDCDRV_REAL_MAX_LONG_SESSION;
        base = 0;
        cur = &hdc_ctrl->cur_alloc_session;
    } else {
        max = (int)HDCDRV_REAL_MAX_SHORT_SESSION;
        base = (int)HDCDRV_REAL_MAX_LONG_SESSION;
        cur = &hdc_ctrl->cur_alloc_short_session;
    }

    for (i = 0; i < max; i++) {
        fd = (*cur + i) % max + base;
        if ((hdc_ctrl->sessions[fd].create_pid != check_pid) &&
            (hdcdrv_get_session_status(&hdc_ctrl->sessions[fd]) == HDCDRV_SESSION_STATUS_IDLE) &&
            (hdc_ctrl->sessions[fd].work_cancel_cnt == 0) && (hdc_ctrl->sessions[fd].epfd == NULL)) {
            session_fd = fd;
            hdc_ctrl->sessions[fd].local_close_state = HDCDRV_CLOSE_TYPE_NONE;
            hdc_ctrl->sessions[fd].remote_close_state = HDCDRV_CLOSE_TYPE_NONE;
            hdc_ctrl->sessions[fd].session_cur_alloc_idx++;
            hdc_ctrl->sessions[fd].remote_session_close_flag = HDCDRV_SESSION_STATUS_CONN;
            hdcdrv_set_session_status(&hdc_ctrl->sessions[fd], HDCDRV_SESSION_STATUS_CONN);
            *cur = (fd + 1) % max + base;
            break;
        }
    }

    if ((session_fd == -1) && (conn_feature == HDCDRV_SERVICE_SHORT_CONN) &&
        (run_env != HDCDRV_SESSION_RUN_ENV_PHYSICAL_CONTAINER) &&
        (run_env != HDCDRV_SESSION_RUN_ENV_VIRTUAL_CONTAINER) && (root_privilege != 0)) {
        conn_feature = HDCDRV_SERVICE_LONG_CONN;
        goto AGAIN;
    }

    if (session_fd < 0) {
        hdcdrv_print_status(service_type);
    }
    ka_task_mutex_unlock(&hdc_ctrl->mutex);

    return session_fd;
}

void hdcdrv_alloc_msg_chan(int dev_id, int service_type, u32 *normal_chan_id, u32 *fast_chan_id)
{
    u32 chan_start;
    u32 chan_end;
    if ((service_type >= HDCDRV_MANAGE_MIN_SERVICE_TYPE) && (service_type <= HDCDRV_MANAGE_MAX_SERVICE_TYPE)) {
        chan_start = (u32)hdc_ctrl->devices[dev_id].msg_chan_cnt - HDCDRV_MANAGE_SERVICE_CHAN_NUM;
        chan_end = (u32)hdc_ctrl->devices[dev_id].msg_chan_cnt;
        *normal_chan_id = (hdc_ctrl->devices[dev_id].normal_chan_num - HDCDRV_MANAGE_SERVICE_CHAN_NUM) +
            (u32)service_type % HDCDRV_MANAGE_SERVICE_CHAN_NUM;
    } else if ((service_type >= HDCDRV_DATA_MIN_SERVICE_TYPE) && (service_type <= HDCDRV_DATA_MAX_SERVICE_TYPE)) {
        chan_start = hdc_ctrl->devices[dev_id].normal_chan_num;
        chan_end = (u32)hdc_ctrl->devices[dev_id].msg_chan_cnt - HDCDRV_MANAGE_SERVICE_CHAN_NUM;
        *normal_chan_id = (u32)service_type % (hdc_ctrl->devices[dev_id].normal_chan_num -
            HDCDRV_MANAGE_SERVICE_CHAN_NUM);
    } else {
        chan_start = hdc_ctrl->devices[dev_id].normal_chan_num;
        chan_end = (u32)hdc_ctrl->devices[dev_id].msg_chan_cnt;
        *normal_chan_id = (u32)service_type % hdc_ctrl->devices[dev_id].normal_chan_num;
        hdcdrv_warn("service_type is not in data and manage service range. service_type=%d\n", service_type);
    }
    *fast_chan_id = hdcdrv_alloc_fast_msg_chan(dev_id, service_type, chan_start, chan_end);
    return;
}

u32 hdcdrv_alloc_fast_msg_chan(int dev_id, int service_type, u32 chan_start, u32 chan_end)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[dev_id];
    u32 chan_id = chan_start;
    int session_cnt;
    u32 i;

    ka_task_mutex_lock(&dev->mutex);
    /* Find the msg chan with the least number of sessions */
    session_cnt = dev->msg_chan[chan_start]->session_cnt;
    for (i = chan_start; i < chan_end; i++) {
        if (session_cnt > dev->msg_chan[i]->session_cnt) {
            session_cnt = dev->msg_chan[i]->session_cnt;
            chan_id = i;
        }
    }
    ka_task_mutex_unlock(&dev->mutex);

    return chan_id;
}

STATIC void hdcdrv_mod_msg_chan_session_cnt(int dev_id, u32 normal_chan_id, u32 fast_chan_id, int num)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[dev_id];

    if (normal_chan_id >= HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN || fast_chan_id >= HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN) {
        hdcdrv_warn("Normal or fast chan id is invalid. (dev=%d; normal=%d; fast=%d)\n",
            dev_id, normal_chan_id, fast_chan_id);
        return;
    }

    if ((dev->msg_chan[normal_chan_id] == NULL) || (dev->msg_chan[fast_chan_id] == NULL)) {
        hdcdrv_warn("Input pararmeter is is invalid. (dev=%d)\n", dev_id);
        return;
    }

    ka_task_mutex_lock(&dev->mutex);
    dev->msg_chan[normal_chan_id]->session_cnt += num;
    dev->msg_chan[fast_chan_id]->session_cnt += num;
    ka_task_mutex_unlock(&dev->mutex);
}

STATIC enum devdrv_dma_data_type hdcdrv_get_dma_data_type(int msg_chan_type, int chan_id,
    int normal_chan_num)
{
    enum devdrv_dma_data_type data_type;

#ifdef CFG_FEATURE_HDC_REG_MEM
#define HDCDRV_TRANS_CHAN_NUM 16
    if (msg_chan_type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
        if (chan_id < normal_chan_num - HDCDRV_MANAGE_SERVICE_CHAN_NUM) {
            data_type = DEVDRV_DMA_DATA_COMMON;
        } else {
            data_type = DEVDRV_DMA_DATA_MANAGE;
        }
    } else {
        if (chan_id < HDCDRV_TRANS_CHAN_NUM - HDCDRV_MANAGE_SERVICE_CHAN_NUM) {
            data_type = DEVDRV_DMA_DATA_TRAFFIC;
        } else {
            data_type = DEVDRV_DMA_DATA_MANAGE;
        }
    }
#else
    /* Low-level use common type */
    if (msg_chan_type == HDCDRV_MSG_CHAN_TYPE_NORMAL) {
        if (hdcdrv_get_service_level(chan_id) == HDCDRV_SERVICE_HIGH_LEVEL) {
            data_type = DEVDRV_DMA_DATA_TRAFFIC;
        } else {
            data_type = DEVDRV_DMA_DATA_COMMON;
        }
    } else {
        data_type = DEVDRV_DMA_DATA_TRAFFIC;
    }
#endif
    return data_type;
}

STATIC void hdcdrv_session_stat_clear(struct hdcdrv_session *session)
{
    session->stat.rx = 0;
    session->stat.rx_bytes = 0;
    session->stat.rx_fail = 0;
    session->stat.rx_finish = 0;
    session->stat.rx_full = 0;
    session->stat.rx_total = 0;

    session->stat.tx = 0;
    session->stat.tx_bytes = 0;
    session->stat.tx_fail = 0;
    session->stat.tx_finish = 0;
    session->stat.tx_full = 0;

    session->stat.alloc_mem_err = 0;
}

STATIC void hdcdrv_session_dbg_stat_clear(struct hdcdrv_session *session)
{
    session->dbg_stat.hdcdrv_mem_avail1 = 0;
    session->dbg_stat.hdcdrv_msg_chan_recv_task1 = 0;
    session->dbg_stat.hdcdrv_msg_chan_recv_task2 = 0;
    session->dbg_stat.hdcdrv_msg_chan_recv_task3 = 0;
    session->dbg_stat.hdcdrv_msg_chan_recv_task4 = 0;
    session->dbg_stat.hdcdrv_msg_chan_recv_task5 = 0;
    session->dbg_stat.hdcdrv_msg_chan_recv_task6 = 0;
    session->dbg_stat.hdcdrv_msg_chan_recv_task7 = 0;
    session->dbg_stat.hdcdrv_msg_chan_send1 = 0;
    session->dbg_stat.hdcdrv_normal_dma_copy1 = 0;
    session->dbg_stat.hdcdrv_recv_data_times = 0;
    session->dbg_stat.hdcdrv_rx_msg_callback1 = 0;
    session->dbg_stat.hdcdrv_rx_msg_callback2 = 0;
    session->dbg_stat.hdcdrv_rx_msg_callback3 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify1 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task1 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task2 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task3 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task4 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task5 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task6 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task7 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task8 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task9 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task_check1 = 0;
    session->dbg_stat.hdcdrv_rx_msg_notify_task_check2 = 0;
    session->dbg_stat.hdcdrv_rx_msg_task_check1 = 0;
    session->dbg_stat.hdcdrv_rx_msg_task_check2 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify1 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task1 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task2 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task3 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task4 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task5 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task6 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task7 = 0;
    session->dbg_stat.hdcdrv_tx_finish_notify_task8 = 0;
    session->dbg_stat.hdcdrv_tx_finish_task_check1 = 0;
    session->dbg_stat.hdcdrv_tx_finish_task_check2 = 0;
    session->dbg_stat.hdcdrv_wait_mem_normal = 0;
    session->dbg_stat.hdcdrv_wait_mem_fifo_full = 0;
}

STATIC void hdcdrv_session_init(int local_session, int remote_session, int dev_id, int service_type, int run_env)
{
    struct hdcdrv_session *session = &hdc_ctrl->sessions[local_session];
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];

    hdcdrv_clear_session_rx_buf(local_session);
    /* after hdcdrv_sid_copy */
    hdcdrv_inner_checker_set(session);
    session->local_session_fd = local_session;
    session->remote_session_fd = remote_session;
    session->dev_id = dev_id;
    session->local_fid = HDCDRV_SESSION_OWNER_PM;
    session->remote_fid = HDCDRV_SESSION_OWNER_PM;
    session->service_type = service_type;
    session->service = &hdc_dev->service[service_type];
    session->run_env = run_env;
    session->timeout_jiffies.send_timeout = ka_system_msecs_to_jiffies(HDCDRV_SESSION_DEFAULT_TIMEOUT);
    session->timeout_jiffies.fast_send_timeout = ka_system_msecs_to_jiffies(HDCDRV_SESSION_DEFAULT_TIMEOUT);

    if (memset_s((void *)&session->stat, sizeof(session->stat), 0, sizeof(session->stat)) != EOK) {
        hdcdrv_err("Calling memset_s failed. (dev=%d)\n", dev_id);
        return;
    }

    session->owner_pid = HDCDRV_INVALID_PID;
    session->pid_flag = 0;
    session->delay_work_flag = 0;

    hdcdrv_session_stat_clear(session);
    hdcdrv_session_dbg_stat_clear(session);
}

STATIC int hdcdrv_session_chan_update(int local_session, int dev_id)
{
    struct hdcdrv_session *session = &hdc_ctrl->sessions[local_session];
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];

    if ((session->chan_id >= hdc_dev->normal_chan_num) || (session->fast_chan_id >= (u32)hdc_dev->msg_chan_cnt)) {
        return HDCDRV_PARA_ERR;
    }

    session->msg_chan = hdc_dev->msg_chan[session->chan_id];
    session->fast_msg_chan = hdc_dev->msg_chan[session->fast_chan_id];
    return HDCDRV_OK;
}

STATIC void hdcdrv_session_uid(int local_session, int current_euid, int current_uid, int current_root_privilege)
{
    struct hdcdrv_session *session = &hdc_ctrl->sessions[local_session];

    session->euid = current_euid;
    session->uid = current_uid;
    session->root_privilege = current_root_privilege;
}

STATIC void hdcdrv_free_service_conn_req(struct hdcdrv_service *service)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    struct hdcdrv_session *session = NULL;
#endif
    struct hdcdrv_connect_list *connect_t = NULL;

    ka_task_mutex_lock(&service->mutex);

    while (service->conn_list_head != NULL) {
        connect_t = service->conn_list_head;
        service->conn_list_head = service->conn_list_head->next;
#ifdef CFG_FEATURE_HDC_REG_MEM
        if ((connect_t->session_fd < HDCDRV_REAL_MAX_SESSION) && (connect_t->session_fd >= 0)) {
            session = &hdc_ctrl->sessions[connect_t->session_fd];
            hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x109, "session free.(session_fd=%d)\n",
                connect_t->session_fd);
        }
#endif
        hdcdrv_kvfree(connect_t, KA_SUB_MODULE_TYPE_1);
        connect_t = NULL;
    }

    ka_task_mutex_unlock(&service->mutex);
}

#ifdef HDC_ENV_DEVICE
STATIC bool hdcdrv_check_run_env_is_phy(u32 dev_id)
{
    u32 host_flag = 0;
    bool is_in = false;
    int ret;
    int vf_flag = DEVDRV_SRIOV_TYPE_PF;

#ifdef CFG_FEATURE_SRIOV
    vf_flag = devdrv_get_pfvf_type_by_devid(dev_id);
#endif

    ret = agentdrv_get_host_phy_mach_flag(dev_id, &host_flag);
    if ((ret == 0) && (host_flag == DEVDRV_HOST_PHY_MACH_FLAG) && (vf_flag == DEVDRV_SRIOV_TYPE_PF)) {
        is_in = true;
    }

    return is_in;
}
#endif

STATIC int hdcdrv_ctrl_msg_connect_handle(const struct hdcdrv_ctrl_msg *msg, struct hdcdrv_service *service,
    u32 devid)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_connect_list *connect_n = NULL;
    struct hdcdrv_connect_list *connect_t = NULL;
    int session_fd, run_env;
    int service_type = msg->connect_msg.service_type;
    u32 normal_chan_id;
    u32 fast_chan_id;
    u64 listen_pid = service->listen_pid;
    int ret;

#ifdef HDC_ENV_DEVICE
    if ((msg->connect_msg.run_env == HDCDRV_SESSION_RUN_ENV_PHYSICAL) && (!hdcdrv_check_run_env_is_phy(devid))) {
        hdcdrv_err_limit("Permission not correct, connect failed.(dev_id=%u)\n", devid);
        return HDCDRV_NO_PERMISSION;
    }
#endif

    connect_n = (struct hdcdrv_connect_list *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_connect_list), KA_SUB_MODULE_TYPE_1);
    if (ka_unlikely(connect_n == NULL)) {
        hdcdrv_err("Alloc connect_t failed. (dev=%u; client_session=%d)\n", devid, msg->connect_msg.client_session);
        return HDCDRV_MEM_ALLOC_FAIL;
    }

    ret = hdcdrv_session_pre_alloc(devid, msg->connect_msg.fid, service_type);
    if (ret != HDCDRV_OK) {
        hdcdrv_kvfree(connect_n, KA_SUB_MODULE_TYPE_1);
        connect_n = NULL;
        hdcdrv_err("Calling hdcdrv_session_pre_alloc failed. (dev_id=%d; fid=%d; service_type=\"%s\"; ret=%d)\n",
            devid, msg->connect_msg.fid, hdcdrv_sevice_str(service_type), ret);
        return ret;
    }

    session_fd = hdcdrv_alloc_session(service_type, msg->connect_msg.run_env,
        msg->connect_msg.root_privilege, listen_pid);
    if (session_fd < 0) {
        hdcdrv_kvfree(connect_n, KA_SUB_MODULE_TYPE_1);
        connect_n = NULL;
        hdcdrv_err_limit("Calling hdcdrv_alloc_session failed. (dev_id=%u; service_type=\"%s\"; listen_pid=%llu)\n",
            devid, hdcdrv_sevice_str(service_type), listen_pid);
        hdcdrv_session_post_free(devid, msg->connect_msg.fid, service_type);
        return HDCDRV_NO_SESSION;
    }

    session = &hdc_ctrl->sessions[session_fd];
    session->unique_val = msg->connect_msg.unique_val;

    if ((msg->connect_msg.fast_chan_id != HDCDRV_INVALID_VALUE) &&
        (msg->connect_msg.normal_chan_id != HDCDRV_INVALID_VALUE)) {
        fast_chan_id = msg->connect_msg.fast_chan_id;
        normal_chan_id = msg->connect_msg.normal_chan_id;
    } else {
        hdcdrv_alloc_session_chan((int)devid, (int)msg->connect_msg.fid, service_type, &normal_chan_id, &fast_chan_id);
    }

    run_env = msg->connect_msg.run_env;

    hdcdrv_session_uid(session_fd, msg->connect_msg.euid, msg->connect_msg.uid, msg->connect_msg.root_privilege);
    hdcdrv_session_init(session_fd, msg->connect_msg.client_session, (int)devid, service_type, run_env);

    session->remote_fid = msg->connect_msg.fid;
    session->peer_create_pid = msg->connect_msg.local_pid;
    session->chan_id = normal_chan_id;
    session->fast_chan_id = fast_chan_id;

    ret = hdcdrv_session_chan_update(session_fd, (int)devid);
    if (ret != HDCDRV_OK) {
        hdcdrv_kvfree(connect_n, KA_SUB_MODULE_TYPE_1);
        connect_n = NULL;
        hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
        hdcdrv_session_post_free(devid, msg->connect_msg.fid, service_type);
        hdcdrv_err("Calling hdcdrv_session_chan_update error. (device=%d; service=\"%s\"; "
            "error_ret=%d; local_session_fd=%x; remote_session_fd=%x; pid=%llu)\n",
            devid, hdcdrv_sevice_str(service_type), (int)ret, session->local_session_fd,
            session->remote_session_fd, session->owner_pid);
        return ret;
    }
    hdcdrv_mod_msg_chan_session_cnt((int)devid, session->chan_id, session->fast_chan_id, 1);

    connect_n->session_fd = session_fd;
    connect_n->next = NULL;

    /* wakeup accept thread */
    ka_task_mutex_lock(&service->mutex);
    if (service->conn_list_head == NULL) {
        service->conn_list_head = connect_n;
    } else {
        connect_t = service->conn_list_head;
        while (connect_t->next != NULL) {
            connect_t = connect_t->next;
        }
        connect_t->next = connect_n;
    }
    ka_task_mutex_unlock(&service->mutex);

    hdcdrv_delay_work_set(session, &session->close_unknow_session, HDCDRV_DELAY_UNKNOWN_SESSION_BIT,
        HDCDRV_SESSION_RECLAIM_TIMEOUT);
    ka_wmb();
    (void)hdcdrv_connect_notify(service_type, (int)devid, msg->connect_msg.fid,
        hdcdrv_rebuild_raw_pid(msg->connect_msg.local_pid), hdcdrv_rebuild_raw_pid(service->listen_pid));
    service->service_stat.accept_wait_stamp = ka_jiffies;
    ka_wmb();

    ka_task_wake_up_interruptible(&service->wq_conn_avail);
    hdcdrv_epoll_wake_up(service->epfd);

    return HDCDRV_OK;
}

STATIC int hdcdrv_ctrl_msg_connect(u32 devid, struct hdcdrv_ctrl_msg *msg)
{
    struct hdcdrv_dev *hdc_dev = NULL;
    struct hdcdrv_service *service = NULL;
    int service_type = msg->connect_msg.service_type;
    u32 fast_chan_id = msg->connect_msg.fast_chan_id;
    u32 normal_chan_id = msg->connect_msg.normal_chan_id;
    u64 remote_pid = msg->connect_msg.local_pid;
    u64 peer_pid = msg->connect_msg.peer_pid;
    u32 fid = msg->connect_msg.fid;
    u64 create_pid;
    int ret;

    hdc_dev = &hdc_ctrl->devices[devid];

    /* in virtual case msg chan_id maybe equal HDCDRV_INVALID_CHAN_ID */
    if (((fast_chan_id >= (u32)hdc_dev->msg_chan_cnt) && (fast_chan_id != HDCDRV_INVALID_CHAN_ID)) ||
        ((normal_chan_id >= hdc_dev->normal_chan_num) && (normal_chan_id != HDCDRV_INVALID_CHAN_ID))) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%u; fast_chan_id=%d; normal_chan_id=%d)\n",
            devid, fast_chan_id, normal_chan_id);
        return HDCDRV_PARA_ERR;
    }

    if ((service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (service_type < 0)) {
        hdcdrv_err_limit("service_type is error. (dev_id=%u; service_type=\"%s\")\n",
            devid, hdcdrv_sevice_str(service_type));
        return HDCDRV_PARA_ERR;
    }

    peer_pid = hdcdrv_get_peer_pid(devid, remote_pid, fid, peer_pid, service_type);
    create_pid = hdcdrv_rebuild_pid(devid, fid, peer_pid);
    service = hdcdrv_search_service(devid, fid, service_type, create_pid);
    if (service->listen_status == HDCDRV_INVALID) {
        /* send to remote to info local service not listen. */
        return HDCDRV_REMOTE_SERVICE_NO_LISTENING;
    }

    ret = hdcdrv_ctrl_msg_connect_handle(msg, service, devid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    hdcdrv_link_ctrl_msg_stats_add(devid, service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT,
        HDCDRV_LINK_CTRL_MSG_RECV_SUCC, HDCDRV_OK);
    return HDCDRV_OK;
}

STATIC int hdcdrv_ctrl_msg_connect_reply(u32 devid, const struct hdcdrv_ctrl_msg *msg)
{
    struct hdcdrv_session *session = NULL;
    int client_session = msg->connect_msg_reply.client_session;
    int server_session = msg->connect_msg_reply.server_session;
    u32 unique_val = msg->connect_msg_reply.unique_val;
    int ret;

    ret = (int)hdcdrv_session_alive_check(client_session, (int)devid, unique_val);
    if (ret != HDCDRV_OK) {
        /* if the session is closed, need to return exception code. */
        return ret;
    }

    session = &hdc_ctrl->sessions[client_session];

#ifdef HDC_ENV_DEVICE
    if ((msg->connect_msg_reply.run_env == HDCDRV_SESSION_RUN_ENV_PHYSICAL) && (!hdcdrv_check_run_env_is_phy(devid))) {
        hdcdrv_err_limit("Permission not correct, connect failed.(dev_id=%u)\n", devid);
        return HDCDRV_NO_PERMISSION;
    }
#endif

    session->run_env = msg->connect_msg_reply.run_env;
    session->remote_fid = msg->connect_msg_reply.fid;
    session->peer_create_pid = msg->connect_msg_reply.local_pid;

    if ((session->fast_chan_id == HDCDRV_INVALID_VALUE) &&
        (session->chan_id == HDCDRV_INVALID_VALUE)) {
        session->fast_chan_id = msg->connect_msg_reply.fast_chan_id;
        session->chan_id = msg->connect_msg_reply.normal_chan_id;
    }

    ret = hdcdrv_session_chan_update(client_session, (int)devid);
    if (ret == HDCDRV_OK) {
        session->remote_session_fd = server_session;
    } else {
        session->remote_session_fd = HDCDRV_INVALID_REMOTE_SESSION_ID;
    }
    session->connect_wait_reply_stamp = ka_jiffies;
    ka_wmb();

    ka_task_wake_up_interruptible(&session->wq_conn);
    hdcdrv_link_ctrl_msg_stats_add(devid, session->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
        HDCDRV_LINK_CTRL_MSG_RECV_SUCC, HDCDRV_OK);

    if (hdcdrv_get_session_status(session) != HDCDRV_SESSION_STATUS_CONN) {
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    return ret;
}

STATIC void hdcdrv_remote_close_work(ka_work_struct_t *p_work)
{
    long ret;
    u32 delay;
    struct hdcdrv_cmd_close close_cmd;
    struct hdcdrv_session *session = ka_container_of(p_work, struct hdcdrv_session, remote_close.work);

    hdcdrv_delay_work_flag_clear(session, HDCDRV_DELAY_REMOTE_CLOSE_BIT);

    delay = ka_system_jiffies_to_msecs(ka_jiffies - session->remote_close_jiff);
    if (delay < HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT_MS &&
        ((session->remote_close_state == HDCDRV_CLOSE_TYPE_USER) ||
        (session->remote_close_state == HDCDRV_CLOSE_TYPE_KERNEL))) {
        ka_system_msleep(HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT_MS - delay);
    }

    if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) {
        close_cmd.session = session->local_session_fd;
        close_cmd.pid = session->owner_pid;
        close_cmd.unique_val = session->unique_val;
        close_cmd.task_start_time = session->task_start_time;
        close_cmd.session_cur_alloc_idx = session->session_cur_alloc_idx;

        ret = hdcdrv_close(&close_cmd, HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST);
        if (ret != HDCDRV_OK) {
            hdcdrv_warn_limit("Close session not success. (devid=%d; chanid=%d; sessionid=%d)\n",
                session->dev_id, session->chan_id, session->local_session_fd);
        }
    }
}

STATIC void hdcdrv_wake_up_context_status(const struct hdcdrv_session *session)
{
#ifndef CFG_DISABLE_TSD_NOTIFY
    ka_task_down_read(&g_symbol_lock);
    if ((g_hdcdrv_register_symbol.module_ptr != NULL) && (session->owner_pid != HDCDRV_INVALID_PID) &&
        (session->service_type == HDCDRV_SERVICE_TYPE_TSD)) {
        __ka_system_module_get(g_hdcdrv_register_symbol.module_ptr);
        (void)g_hdcdrv_register_symbol.wake_up_context_status((int)session->owner_pid, (u32)session->dev_id,
            DEVDRV_STATUS_HDC_CLOSE_FLAG);
        ka_system_module_put(g_hdcdrv_register_symbol.module_ptr);
    }
    ka_task_up_read(&g_symbol_lock);
#endif
    return;
}

STATIC int hdcdrv_ctrl_msg_close(u32 devid, const struct hdcdrv_ctrl_msg *msg)
{
    struct hdcdrv_session *session = NULL;
    int session_fd = msg->close_msg.remote_session;
    long ret;
    int timeout;

    if ((session_fd >= HDCDRV_REAL_MAX_SESSION) || (session_fd < 0)) {
        hdcdrv_err("session_fd is illegal. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    session = &hdc_ctrl->sessions[session_fd];

    ret = hdcdrv_session_state_to_remote_close(session_fd, msg->close_msg.unique_val);
    if (ret != HDCDRV_OK) {
        if (ret == HDCDRV_SESSION_HAS_CLOSED) {
            hdcdrv_link_ctrl_msg_stats_add(devid, session->service_type, HDCDRV_CTRL_MSG_TYPE_CLOSE,
                HDCDRV_LINK_CTRL_MSG_RECV_SUCC, (int)ret);
            return HDCDRV_OK;
        } else {
            hdcdrv_link_ctrl_msg_stats_add(devid, session->service_type, HDCDRV_CTRL_MSG_TYPE_CLOSE,
                HDCDRV_LINK_CTRL_MSG_RECV_FAIL, (int)ret);
            return (int)ret;
        }
    }

    (void)hdcdrv_close_notify(session->service_type, (int)devid, (int)session->remote_fid,
        hdcdrv_rebuild_raw_pid(session->peer_create_pid), hdcdrv_rebuild_raw_pid(session->owner_pid));

    session->remote_close_state = msg->close_msg.session_close_state;

    timeout = ((session->remote_close_state == HDCDRV_CLOSE_TYPE_USER) ||
        (session->remote_close_state == HDCDRV_CLOSE_TYPE_KERNEL)) ? HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT : 0;
    session->remote_close_jiff = ka_jiffies;

    hdcdrv_delay_work_set(session, &session->remote_close, HDCDRV_DELAY_REMOTE_CLOSE_BIT, timeout);
    hdcdrv_wake_up_context_status(session);

    hdcdrv_link_ctrl_msg_stats_add(devid, session->service_type, HDCDRV_CTRL_MSG_TYPE_CLOSE,
        HDCDRV_LINK_CTRL_MSG_RECV_SUCC, HDCDRV_OK);

    return HDCDRV_OK;
}

void hdcdrv_set_device_status(int devid, u32 valid)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[devid];
    dev->valid = valid;
    hdcdrv_info("Set status finished.(devid=%d; status=%d)\n", devid, valid);
}

u32 hdcdrv_get_device_status(int devid)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[devid];
    return dev->valid;
}

STATIC void hdcdrv_set_device_reset(u32 devid)
{
    hdcdrv_del_dev(devid);
}

void hdcdrv_set_device_para(u32 devid, int normal_chan_num)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[devid];
    dev->normal_chan_num = 1; // default value set to 1
    if ((normal_chan_num > 0) && (normal_chan_num <= HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN)) {
        dev->normal_chan_num = (u32)normal_chan_num;
    }
    hdcdrv_info("Set normal chan number ok.(dev_id=%d; normal_chan_num=%d)\n", devid, normal_chan_num);
}

STATIC int hdcdrv_ctrl_msg_sync(u32 devid, struct hdcdrv_ctrl_msg *msg)
{
    int segment = msg->sync_msg.segment;
    int peer_dev_id = msg->sync_msg.peer_dev_id;

    if ((segment <= 0) || (segment > HDCDRV_HUGE_PACKET_SEGMENT)) {
        hdcdrv_err("segment is invalid. (dev_id=%d; segment=%d)\n", devid, segment);
        return HDCDRV_PARA_ERR;
    }
    if (hdc_ctrl->devices[devid].dev == NULL) {
        hdcdrv_warn("Device is not init instance. (dev_id=%d)\n", devid);
        return -EUNATCH;
    }

    hdcdrv_set_segment(segment);
    hdcdrv_set_peer_dev_id((int)devid, peer_dev_id);

    /* notice host device devid */
    msg->sync_msg.peer_dev_id = (int)devid;
    hdcdrv_info("Calling hdcdrv_ctrl_msg_sync succeeded.(dev_id=%d; peer_dev_id=%d)\n", devid, peer_dev_id);
    return HDCDRV_OK;
}

STATIC int hdcdrv_ctrl_edge_check(u32 devid, void *data, u32 in_data_len, u32 out_data_len, const u32 *p_real_out_len)
{
    struct hdcdrv_ctrl_msg *msg = (struct hdcdrv_ctrl_msg *)data;
    struct hdcdrv_ctrl_msg_sync_mem_info *mem = NULL;
    struct hdcdrv_sysfs_ctrl_msg *sysfs_msg = NULL;
    u32 in_len_min = 0;
    u32 out_len_min = 0;

    /* Here check 3 structures: hdcdrv_ctrl_msg_sync_mem_info, hdcdrv_sysfs_ctrl_msg, hdcdrv_ctrl_msg.
        The first element of the three structures is int, indicating the type of the msg.
        For security purposes, ensure that the length of in_data_len exceeds sizeof(int) before using msg->type. */
    if ((devid >= (u32)hdcdrv_get_max_support_dev()) || (msg == NULL) || (p_real_out_len == NULL)
        || (in_data_len < sizeof(int))) {
        hdcdrv_err("Input pararmeter is error.\n");
        return HDCDRV_PARA_ERR;
    }

    if (msg->type == HDCDRV_CTRL_MSG_TYPE_SYNC_MEM_INFO) {
        if (in_data_len < sizeof(struct hdcdrv_ctrl_msg_sync_mem_info)) {
            hdcdrv_err("in_data_len is smaller than expected (dev_id=%d)\n", devid);
            return HDCDRV_PARA_ERR;
        }
        mem = (struct hdcdrv_ctrl_msg_sync_mem_info *)data;
        if ((u32)mem->phy_addr_num > HDCDRV_MEM_MAX_PHY_NUM) {
            hdcdrv_err("phy_addr_num is biger than expected (dev_id=%d; phy_addr_num=%d)\n", devid, mem->phy_addr_num);
            return HDCDRV_PARA_ERR;
        }
        in_len_min = (u32)sizeof(struct hdcdrv_ctrl_msg_sync_mem_info) +
            mem->phy_addr_num * sizeof(struct hdcdrv_dma_mem);
        out_len_min = (u32)sizeof(struct hdcdrv_ctrl_msg_sync_mem_info);
    } else if (msg->type == HDCDRV_CTRL_MSG_TYPE_GET_DEV_SESSION_STAT) {
        sysfs_msg = (struct hdcdrv_sysfs_ctrl_msg *)data;
        if (sysfs_msg->head.print_type == HDC_DFX_PRINT_IN_LOG) {
            in_len_min = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg);
        } else {
            in_len_min = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg) + HDCDRV_SYSFS_DATA_MAX_LEN;
        }
        out_len_min = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg);
    }  else if ((msg->type == HDCDRV_CTRL_MSG_TYPE_GET_DEV_CHAN_STAT) ||
        (msg->type == HDCDRV_CTRL_MSG_TYPE_GET_DEV_LINK_STAT)) {
        in_len_min = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg) + HDCDRV_SYSFS_DATA_MAX_LEN;
        out_len_min = (u32)sizeof(struct hdcdrv_sysfs_ctrl_msg);
    } else {
        in_len_min = (u32)sizeof(struct hdcdrv_ctrl_msg);
        out_len_min = (u32)sizeof(struct hdcdrv_ctrl_msg);
    }

    if ((in_data_len < in_len_min) || (out_data_len < out_len_min)) {
        hdcdrv_err("Input pararmeter is error. (dev=%d; data_in_len=%d; min_size=%d; out_len=%d, min_size=%d)\n",
            devid, in_data_len, in_len_min, out_data_len, out_len_min);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_ctrl_msg_recv_exception_report(u32 devid, const struct hdcdrv_ctrl_msg *msg)
{
    if ((msg->error_code != HDCDRV_OK) && (msg->error_code != -EUNATCH) &&
        (msg->type != HDCDRV_CTRL_MSG_TYPE_CONNECT) && (msg->type != HDCDRV_CTRL_MSG_TYPE_CLOSE)) {
        hdcdrv_warn_limit("Command executed unexpected. (dev=%d; cmd=%d; ret_code=%d)\n",
            devid, msg->type, msg->error_code);
    }
}

void hdcdrv_notify_msg_recv(u32 devid, void *data, u32 in_data_len)
{
    struct hdcdrv_ctrl_msg *msg = (struct hdcdrv_ctrl_msg *)data;
    struct hdcdrv_session *session = NULL;
    int session_fd = -1;
    int session_state;

    if ((msg == NULL) || (in_data_len < sizeof(int))) {
        hdcdrv_err("Input pararmeter is error. (devid=%u, in_data_len=%u)\n", devid, in_data_len);
        return;
    }

    if (msg->type == HDCDRV_CTRL_MSG_TYPE_CLOSE) {
        if (in_data_len < (u32)sizeof(struct hdcdrv_ctrl_msg)) {
            hdcdrv_err("Input in_data_len is error. (devid=%u, data_in_len=%d;)\n", devid, in_data_len);
            return;
        }

        session_fd = msg->close_msg.remote_session;
        if ((session_fd < 0) || (session_fd >= HDCDRV_REAL_MAX_SESSION)) {
            hdcdrv_err("Session fd is invalid. (devid=%u, session_fd=%d)", devid, session_fd);
            return;
        }
        session = &hdc_ctrl->sessions[session_fd];

        session_state = hdcdrv_get_session_status(session);
        if ((session_state == HDCDRV_SESSION_STATUS_IDLE) || (session_state == HDCDRV_SESSION_STATUS_CLOSING)) {
            return;
        }

        if ((session->local_session_fd != msg->close_msg.remote_session) || (session->remote_session_fd != msg->close_msg.local_session)) {
            return;
        }

        if (session->unique_val != msg->close_msg.unique_val) {
            return;
        }

        /* slove aipcu process does not exit problem after host process exit */
        if (session->service_type == HDCDRV_SERVICE_TYPE_TSD) {
            session->remote_session_close_flag = HDCDRV_SESSION_STATUS_REMOTE_CLOSED;
        }
    }
    return;
}

int hdcdrv_ctrl_msg_recv(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct hdcdrv_ctrl_msg *msg = (struct hdcdrv_ctrl_msg *)data;

    if (hdcdrv_ctrl_edge_check(devid, data, in_data_len, out_data_len, real_out_len) != HDCDRV_OK) {
        hdcdrv_err("Input parameters check failed. (dev=%d)\n", devid);
        return HDCDRV_PARA_ERR;
    }

    msg->error_code = HDCDRV_OK;
    *real_out_len = (sizeof(struct hdcdrv_ctrl_msg));

    switch (msg->type) {
        case HDCDRV_CTRL_MSG_TYPE_CONNECT:
            msg->error_code = hdcdrv_ctrl_msg_connect(devid, msg);
            break;
        case HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY:
            msg->error_code = hdcdrv_ctrl_msg_connect_reply(devid, msg);
            break;
        case HDCDRV_CTRL_MSG_TYPE_CLOSE:
            msg->error_code = hdcdrv_ctrl_msg_close(devid, msg);
            break;
        case HDCDRV_CTRL_MSG_TYPE_SYNC:
            msg->error_code = hdcdrv_ctrl_msg_sync(devid, msg);
            break;
        case HDCDRV_CTRL_MSG_TYPE_RESET:
            hdcdrv_set_device_reset(devid);
            break;
        case HDCDRV_CTRL_MSG_TYPE_CHAN_SET:
            hdcdrv_set_device_para(devid, msg->chan_set_msg.normal_chan_num);
            break;
        case HDCDRV_CTRL_MSG_TYPE_SYNC_MEM_INFO:
            *real_out_len = sizeof(struct hdcdrv_ctrl_msg_sync_mem_info);
            msg->error_code = hdcdrv_set_mem_info((int)devid, 0, HDCDRV_RBTREE_SIDE_REMOTE,
                (struct hdcdrv_ctrl_msg_sync_mem_info *)data);
            break;
        case HDCDRV_CTRL_MSG_TYPE_GET_DEV_SESSION_STAT:
            msg->error_code = hdcdrv_sysfs_ctrl_msg_get_session_stat(devid, data, real_out_len);
            break;
        case HDCDRV_CTRL_MSG_TYPE_GET_DEV_CHAN_STAT:
            msg->error_code = hdcdrv_sysfs_ctrl_msg_get_chan_stat(devid, data, real_out_len);
            break;
        case HDCDRV_CTRL_MSG_TYPE_GET_DEV_LINK_STAT:
            msg->error_code = hdcdrv_sysfs_ctrl_msg_get_connect_stat(devid, data, real_out_len);
            break;
        default:
            hdcdrv_err_limit("Device command is illegal. (dev=%d; cmd=%d)\n", devid, msg->type);
            return HDCDRV_PARA_ERR;
    }

    hdcdrv_ctrl_msg_recv_exception_report(devid, msg);

    return HDCDRV_OK;
}

long hdcdrv_dev_para_check(int dev_id, int service_type)
{
    if ((dev_id >= hdcdrv_get_max_support_dev()) || (dev_id < 0)) {
        hdcdrv_err_limit("Input pararmeter is error. (dev_id=%d)\n", dev_id);
        return HDCDRV_PARA_ERR;
    }

    if ((service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (service_type < 0)) {
        hdcdrv_err_limit("Input pararmeter is error. (dev_id=%d; service_type=%d)\n", dev_id, service_type);
        return HDCDRV_PARA_ERR;
    }

    if (hdc_ctrl->devices[dev_id].valid != HDCDRV_VALID) {
        return HDCDRV_DEVICE_NOT_READY;
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_cmd_get_peer_dev_id(struct hdcdrv_cmd_get_peer_dev_id *cmd)
{
    long ret;

    /* reuse dev check func with default service type */
    ret = hdcdrv_dev_para_check(cmd->dev_id, 0);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    cmd->peer_dev_id = hdcdrv_get_peer_dev_id(cmd->dev_id);

    return HDCDRV_OK;
}

STATIC long hdcdrv_config(struct hdcdrv_cmd_config *cmd)
{
    if (hdc_ctrl->segment == HDCDRV_INVALID_PACKET_SEGMENT) {
        return HDCDRV_PARA_ERR;
    }

    if (cmd->segment > hdcdrv_mem_block_capacity()) {
        cmd->segment = hdcdrv_mem_block_capacity();
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_set_service_level(const struct hdcdrv_cmd_set_service_level *cmd)
{
    int service_type = cmd->service_type;

    if ((service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (service_type < 0)) {
        hdcdrv_err("Input pararmeter is error. (service_type=%d)\n", service_type);
        return HDCDRV_PARA_ERR;
    }

    if ((cmd->level != HDCDRV_SERVICE_HIGH_LEVEL) && (cmd->level != HDCDRV_SERVICE_LOW_LEVEL)) {
        hdcdrv_err("service level is illegal. (level=%d)\n", cmd->level);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_server_create(struct hdcdrv_ctx *ctx, const struct hdcdrv_cmd_server_create *cmd, u32 fid)
{
    struct hdcdrv_service *service = NULL;
    long ret;
    u32 create_fid;

    if (ctx == NULL) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%d; service_type=\"%s\")\n",
            cmd->dev_id, hdcdrv_sevice_str(cmd->service_type));
        return HDCDRV_PARA_ERR;
    }

    ret = hdcdrv_dev_para_check(cmd->dev_id, cmd->service_type);
    if (ret != 0) {
        return ret;
    }

    create_fid = fid;
#ifdef CFG_FEATURE_VFIO_DEVICE
    if (cmd->service_type == HDCDRV_SERVICE_TYPE_TDT) {
        create_fid = hdcdrv_get_fid(hdcdrv_get_pid());
    }
#endif

    service = hdcdrv_alloc_service((u32)cmd->dev_id, create_fid, cmd->service_type, cmd->pid);
    if (service == NULL) {
        hdcdrv_err("Cann't create server. (dev_id=%d; service_type=\"%s\")\n", cmd->dev_id,
            hdcdrv_sevice_str(cmd->service_type));
        return HDCDRV_MEM_ALLOC_FAIL;
    }

    ka_task_mutex_lock(&service->mutex);

    if (service->listen_status == HDCDRV_VALID) {
        ka_task_mutex_unlock(&service->mutex);
        return HDCDRV_SERVICE_LISTENING;
    }

    service->listen_pid = cmd->pid;
    service->listen_status = HDCDRV_VALID;
    service->fid = create_fid;
    if (memset_s((void *)&service->data_stat, sizeof(service->data_stat), 0, sizeof(service->data_stat)) != EOK) {
        service->listen_pid = 0;
        service->listen_status = HDCDRV_INVALID;
        ka_task_mutex_unlock(&service->mutex);
        hdcdrv_free_service(service);
        hdcdrv_err("Calling memset_s failed.\n");
        return HDCDRV_SAFE_MEM_OP_FAIL;
    }

    hdcdrv_bind_server_ctx(ctx, service, cmd->dev_id, cmd->service_type);

    ka_task_mutex_unlock(&service->mutex);

    hdcdrv_info_share("Server create success. (dev_id=%d; fid=%u; pid=%llu; service_type=\"%s\")\n",
        cmd->dev_id, fid, cmd->pid, hdcdrv_sevice_str(cmd->service_type));

    return HDCDRV_OK;
}

STATIC long hdcdrv_server_para_check(int dev_id, int service_type)
{
    if ((dev_id >= hdcdrv_get_max_support_dev()) || (dev_id < 0)) {
        hdcdrv_err("Input pararmeter dev_id is error. (dev_id=%d)\n", dev_id);
        return HDCDRV_PARA_ERR;
    }

    if ((service_type >= HDCDRV_SUPPORT_MAX_SERVICE)  || (service_type < 0)) {
        hdcdrv_err("Input pararmeter service_type is error. (dev_id=%d, service_type=%d)\n",
            dev_id, service_type);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

long hdcdrv_server_free(struct hdcdrv_service *service, int dev_id, int service_type)
{
    ka_task_mutex_lock(&service->mutex);

    if (service->listen_status == HDCDRV_INVALID) {
        ka_task_mutex_unlock(&service->mutex);
        hdcdrv_warn("Server has not been created. (dev_id=%d; service_type=\"%s\")\n",
            dev_id, hdcdrv_sevice_str(service_type));
        return HDCDRV_SERVICE_NO_LISTENING;
    }

    hdcdrv_unbind_server_ctx(service);
    service->listen_status = HDCDRV_INVALID;

    ka_task_mutex_unlock(&service->mutex);

    ka_task_wake_up_interruptible(&service->wq_conn_avail);
    hdcdrv_free_service_conn_req(service);
    hdcdrv_free_service(service);

    return HDCDRV_OK;
}

STATIC long hdcdrv_server_destroy(const struct hdcdrv_ctx *ctx, const struct hdcdrv_cmd_server_destroy *cmd, u32 fid)
{
    struct hdcdrv_service *service = NULL;
    long ret;
    u32 create_fid;

    ret = hdcdrv_server_para_check(cmd->dev_id, cmd->service_type);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    create_fid = fid;
#ifdef CFG_FEATURE_VFIO_DEVICE
    if (cmd->service_type == HDCDRV_SERVICE_TYPE_TDT) {
        create_fid = hdcdrv_get_fid(cmd->pid);
    }
#endif

    service = hdcdrv_search_service((u32)cmd->dev_id, create_fid, cmd->service_type, cmd->pid);
    if (service->listen_pid != cmd->pid) {
        hdcdrv_warn("Server has already been destroyed. (dev_id=%d; service=\"%s\"; pid=%llu; owner_pid=%llu)\n",
            cmd->dev_id, hdcdrv_sevice_str(cmd->service_type), cmd->pid, service->listen_pid);
        return HDCDRV_OK;
    }

    if ((service->ctx != ctx) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
        hdcdrv_err("service_ctx not match. (dev_id=%d; fid=%u; service_type=\"%s\")\n",
            cmd->dev_id, fid, hdcdrv_sevice_str(cmd->service_type));
        return HDCDRV_PARA_ERR;
    }

    hdcdrv_info_share("Server destroy success. (dev_id=%d; fid=%u; service=\"%s\"; pid=%llu)\n",
        cmd->dev_id, fid, hdcdrv_sevice_str(cmd->service_type), cmd->pid);

    return hdcdrv_server_free(service, cmd->dev_id, cmd->service_type);
}

STATIC long hdcdrv_server_wakeup_wait(const struct hdcdrv_cmd_server_wakeup_wait *cmd, u32 fid)
{
    struct hdcdrv_service *service = NULL;
    long ret;

    ret = hdcdrv_server_para_check(cmd->dev_id, cmd->service_type);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    service = hdcdrv_search_service((u32)cmd->dev_id, fid, cmd->service_type, cmd->pid);
    if (service->listen_pid != cmd->pid) {
        hdcdrv_warn("Server has already been destroyed. (dev_id=%d; service=\"%s\"; pid=%llu; owner_pid=%llu)\n",
            cmd->dev_id, hdcdrv_sevice_str(cmd->service_type), cmd->pid, service->listen_pid);
        return HDCDRV_OK;
    }

    ka_task_mutex_lock(&service->mutex);
    if (service->listen_status == HDCDRV_INVALID) {
        ka_task_mutex_unlock(&service->mutex);
        hdcdrv_warn("Server has not been created. (dev_id=%d; service_type=\"%s\")\n",
            cmd->dev_id, hdcdrv_sevice_str(cmd->service_type));
        return HDCDRV_SERVICE_NO_LISTENING;
    }

    service->listen_status = HDCDRV_LISTEN_STATUS_IDLE;
    ka_task_mutex_unlock(&service->mutex);
    ka_task_wake_up_interruptible(&service->wq_conn_avail);
    hdcdrv_info("Server wake up success. (dev_id=%d; fid=%u; service=\"%s\"; pid=%llu)\n",
        cmd->dev_id, fid, hdcdrv_sevice_str(cmd->service_type), cmd->pid);
    return HDCDRV_OK;
}

STATIC long hdcdrv_accept_wait(const struct hdcdrv_dev *dev, struct hdcdrv_service *service, int service_type,
    u64 check_pid, int *session_fd)
{
    long ret;
    struct hdcdrv_connect_list *connect_t = NULL;

    ka_task_mutex_lock(&service->mutex);

    if (service->listen_status == HDCDRV_INVALID) {
        ka_task_mutex_unlock(&service->mutex);
        hdcdrv_limit_exclusive(err, HDCDRV_LIMIT_LOG_0x01, "Service has not been created. "
            "(dev_id=%d; service_type=\"%s\")\n", dev->dev_id, hdcdrv_sevice_str(service_type));
        return HDCDRV_SERVICE_NO_LISTENING;
    }

    if (service->listen_pid != check_pid) {
        ka_task_mutex_unlock(&service->mutex);
        hdcdrv_err("Device has no permission for service. (dev_id=%d; pid=%llu; service_type=\"%s\"; pid=%llu)\n",
            dev->dev_id, hdcdrv_get_pid(), hdcdrv_sevice_str(service_type), service->listen_pid);
        return HDCDRV_NO_PERMISSION;
    }

    while (service->conn_list_head == NULL) {
        ka_task_mutex_unlock(&service->mutex);

        ret = (long)ka_task_wait_event_interruptible(service->wq_conn_avail,
            ((service->conn_list_head != NULL) || (dev->valid != HDCDRV_VALID) ||
            (service->listen_status == HDCDRV_INVALID) || (service->listen_status == HDCDRV_LISTEN_STATUS_IDLE) ||
            (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL)));
        hdcdrv_wakeup_record_resq_time(dev->dev_id, service_type, service->service_stat.accept_wait_stamp,
            HDCDRV_WAKE_UP_WAIT_TIMEOUT , "hdc accept wait wake up stamp");

        if (ret != 0) {
            hdcdrv_warn("Accept wait interruptible exit. (dev_id=%d; service_type=\"%s\"; ret=%d)\n",
                dev->dev_id, hdcdrv_sevice_str(service_type), (int)ret);
            return ret;
        }

        if (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) {
            hdcdrv_info("peer status abnormal, quit accept. (dev_id=%d; service_type=\"%s\")\n",
                dev->dev_id, hdcdrv_sevice_str(service_type));
            return HDCDRV_PEER_REBOOT;
        }

        if ((dev->valid != HDCDRV_VALID) || (service->listen_status == HDCDRV_INVALID) ||
            (service->listen_status == HDCDRV_LISTEN_STATUS_IDLE)) {
            hdcdrv_info("Accept wait quit. (dev_id=%d; service_type=\"%s\"; dev_status=%d; listen_status=%d)\n",
                dev->dev_id, hdcdrv_sevice_str(service_type), dev->valid, service->listen_status);
            return HDCDRV_DEVICE_RESET;
        }

        ka_task_mutex_lock(&service->mutex);
    }

    connect_t = service->conn_list_head;
    service->conn_list_head = service->conn_list_head->next;

    ka_task_mutex_unlock(&service->mutex);

    *session_fd = connect_t->session_fd;
    hdcdrv_kvfree(connect_t, KA_SUB_MODULE_TYPE_1);
    connect_t = NULL;

    return HDCDRV_OK;
}

static inline u32 hdccom_get_fid_owner(u32 fid, u32 container_id)
{
    if ((fid > HDCDRV_DEFAULT_PM_FID) && (container_id == HDCDRV_PHY_HOST_ID)) {
        return HDCDRV_SESSION_OWNER_VM;
    }

    if ((fid > HDCDRV_DEFAULT_PM_FID) && (container_id < HDCDRV_DOCKER_MAX_NUM)) {
        return HDCDRV_SESSION_OWNER_CT;
    }

    return HDCDRV_SESSION_OWNER_PM;
}

STATIC void hdcdrv_set_session_default_owner(struct hdcdrv_session *session, u64 cmd_pid)
{
    ka_task_mutex_lock(&session->mutex);
    if (session->pid_flag == 0) {
        session->owner_pid = cmd_pid;
        session->task_start_time = hdcdrv_get_task_start_time();
    }
    ka_task_mutex_unlock(&session->mutex);
}

STATIC void hdcdrv_set_session_default_owner_invalid(struct hdcdrv_session *session)
{
    ka_task_mutex_lock(&session->mutex);
    if (session->pid_flag == 0) {
        session->owner_pid = HDCDRV_INVALID_PID;
        session->task_start_time = HDCDRV_KERNEL_DEFAULT_START_TIME;
    }
    ka_task_mutex_unlock(&session->mutex);
}

#ifdef CFG_FEATURE_PFSTAT
STATIC void hdcdrv_pfstat_session_init(u32 dev_id, u32 normal_chan_id, u32 fast_chan_id)
{
    struct hdcdrv_dev *dev = &hdc_ctrl->devices[dev_id];

    if (normal_chan_id >= HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN || fast_chan_id >= HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN) {
        hdcdrv_warn("Normal or fast chan id is invalid. (dev=%d; normal=%d; fast=%d)\n",
            dev_id, normal_chan_id, fast_chan_id);
        return;
    }

    if ((dev->msg_chan[normal_chan_id] == NULL) || (dev->msg_chan[fast_chan_id] == NULL)) {
        hdcdrv_warn("Input pararmeter is is invalid. (dev=%d)\n", dev_id);
        return;
    }

    (void)hdcdrv_pfstat_create_stats_handle(normal_chan_id);
    (void)hdcdrv_pfstat_create_stats_handle(fast_chan_id);
}
#endif

STATIC long hdcdrv_accept(struct hdcdrv_cmd_accept *cmd, u32 fid)
{
    struct hdcdrv_service *service = NULL;
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_dev *dev = NULL;
    struct hdcdrv_ctrl_msg msg;
    u32 container_id;
    int session_fd;
    u32 create_fid;
    u32 len = 0;
    long ret;
#ifdef CFG_FEATURE_HDC_REG_MEM
    unsigned long flags;
    if (hdc_ctrl->segment == HDCDRV_INVALID_PACKET_SEGMENT) {
        hdcdrv_warn("hdcdrv_connect pcie not ready (dev_id=%d; fid=%u; service_type=\"%s\")\n",
            cmd->dev_id, fid, hdcdrv_sevice_str(cmd->service_type));
        return HDCDRV_DEVICE_NOT_READY;
    }
#endif

    ret = hdcdrv_dev_para_check(cmd->dev_id, cmd->service_type);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    container_id = hdcdrv_get_container_id();
    if (container_id >= HDCDRV_DOCKER_MAX_NUM) {
        hdcdrv_err("Parameter container_id illegal. (dev_id=%d; service_type=\"%s\"; container_id=%d)\n",
            cmd->dev_id, hdcdrv_sevice_str(cmd->service_type), container_id);
        return HDCDRV_PARA_ERR;
    }

    dev = &hdc_ctrl->devices[cmd->dev_id];

    create_fid = fid;
#ifdef CFG_FEATURE_VFIO_DEVICE
    if (cmd->service_type == HDCDRV_SERVICE_TYPE_TDT) {
        create_fid = hdcdrv_get_fid(hdcdrv_get_pid());
    }
#endif

    service = hdcdrv_search_service((u32)cmd->dev_id, create_fid, cmd->service_type, cmd->pid);
RETRY_NEXT:
    ret = hdcdrv_accept_wait(dev, service, cmd->service_type, cmd->pid, &session_fd);
    if (ret != HDCDRV_OK) {
        hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT,
            HDCDRV_LINK_CTRL_MSG_WAIT_TIMEOUT, (int)ret);
        return ret;
    }
    hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT,
        HDCDRV_LINK_CTRL_MSG_WAIT_SUCC, HDCDRV_OK);

    session = &hdc_ctrl->sessions[session_fd];
    if (hdcdrv_get_session_status(session) != HDCDRV_SESSION_STATUS_CONN) {
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    session->create_pid = cmd->pid;
    session->container_id = container_id;
    session->local_fid = fid;
    session->owner = hdccom_get_fid_owner(session->local_fid, session->container_id);

#ifdef CFG_FEATURE_HDC_REG_MEM
    if (session->mem_release_fifo.buf != NULL) {
        ka_task_spin_lock_irqsave(&session->mem_release_lock, flags);
        ka_base_kfifo_reset(&session->mem_release_fifo);
        ka_task_spin_unlock_irqrestore(&session->mem_release_lock, flags);
    }
#endif

#ifndef HDC_ENV_DEVICE
    session->run_env = hdcdrv_get_session_run_env((u32)cmd->dev_id, fid);
#endif

    hdcdrv_set_session_default_owner(session, cmd->pid);
    msg.type = HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY;
    msg.connect_msg_reply.client_session = session->remote_session_fd;
    msg.connect_msg_reply.server_session = session->local_session_fd;
    msg.connect_msg_reply.run_env = session->run_env;
    msg.connect_msg_reply.unique_val = session->unique_val;
    msg.connect_msg_reply.fast_chan_id = session->fast_chan_id;
    msg.connect_msg_reply.normal_chan_id = session->chan_id;
    msg.connect_msg_reply.fid = fid;
    msg.connect_msg_reply.local_pid = cmd->pid;
    msg.error_code = HDCDRV_OK;

    ret = hdcdrv_non_trans_ctrl_msg_send((u32)cmd->dev_id, (void *)&msg, sizeof(msg), sizeof(msg), &len);
    if ((ret != HDCDRV_OK) || (len != sizeof(msg)) || (msg.error_code != HDCDRV_OK)) {
        hdcdrv_mod_msg_chan_session_cnt(session->dev_id, session->chan_id, session->fast_chan_id, -1);
        hdcdrv_session_post_free((u32)session->dev_id, fid, cmd->service_type);
        hdcdrv_delay_work_cancel(session, &session->close_unknow_session, HDCDRV_DELAY_UNKNOWN_SESSION_BIT);
        session->create_pid = HDCDRV_INVALID_PID;
        ka_wmb();
        hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
        if (msg.error_code == HDCDRV_SESSION_HAS_CLOSED) {
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x00, "Remote session has closed. "
                                  "(dev=%d; service=\"%s\"; ret=%ld; len=%d; ret_code=%d)\n",
                                  cmd->dev_id, hdcdrv_sevice_str(cmd->service_type), ret, len, msg.error_code);
        } else {
            hdcdrv_limit_exclusive(err, HDCDRV_LIMIT_LOG_0x00, "Reply message send failed. "
                                  "(dev=%d; service=\"%s\"; ret=%ld; len=%d; error_code=%d)\n",
                                  cmd->dev_id, hdcdrv_sevice_str(cmd->service_type), ret, len, msg.error_code);
        }
        if (((ret == 0)) && (len == sizeof(msg)) && (msg.error_code != HDCDRV_OK) &&
            (service->conn_list_head != NULL)) {
            hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
                HDCDRV_LINK_CTRL_MSG_SEND_FAIL, msg.error_code);
            goto RETRY_NEXT;
        }

        hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
            HDCDRV_LINK_CTRL_MSG_SEND_FAIL, (int)ret);

        hdcdrv_set_session_default_owner_invalid(session);
        return HDCDRV_SEND_CTRL_MSG_FAIL;
    }

    hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
        HDCDRV_LINK_CTRL_MSG_SEND_SUCC, HDCDRV_OK);

    /* response cmd */
    cmd->session = session->local_session_fd;
    cmd->session_cur_alloc_idx = session->session_cur_alloc_idx;

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_session_init(session->dev_id, session->chan_id, session->fast_chan_id);
#endif
    ka_task_mutex_lock(&service->mutex);
    service->service_stat.accept_session_num++;
    ka_task_mutex_unlock(&service->mutex);

    return HDCDRV_OK;
}

STATIC u64 hdcdrv_set_connect_timeout(unsigned int timeout)
{
    u64 new_timeout = timeout * KA_HZ;
    u64 wait_timeout = HDCDRV_CONN_TIMEOUT;

    if ((new_timeout <= HDCDRV_CONN_TIMEOUT) && (new_timeout >= HDCDRV_CONN_MIN_TIMEOUT)) {
        wait_timeout = new_timeout;
    } else {
        hdcdrv_warn_limit("Connect wait timeout invalid. (wait_timeout=%lld)\n", new_timeout);
    }
    return wait_timeout;
}

STATIC long hdcdrv_connect_wait_reply(const struct hdcdrv_dev *dev,
    struct hdcdrv_session *session, u32 timeout)
{
    long ret;
    u64 wait_timeout;

    wait_timeout = hdcdrv_set_connect_timeout(timeout);
    ret = ka_task_wait_event_interruptible_timeout(session->wq_conn,
        (session->remote_session_fd != HDCDRV_INVALID_VALUE) || (dev->valid != HDCDRV_VALID),
        (long)wait_timeout);
    hdcdrv_wakeup_record_resq_time(dev->dev_id, session->service_type, session->connect_wait_reply_stamp,
        HDCDRV_WAKE_UP_WAIT_TIMEOUT , "hdc connect wait reply wake up stamp");

    if (ret <= 0) {
        hdcdrv_warn("Connect wait. (dev=%d; ret=%d; l_fd=%u; r_fd=0x%x; pid=%llu)\n",
            dev->dev_id, (int)ret, session->local_session_fd, session->remote_session_fd, session->create_pid);
        return (ret == 0) ? HDCDRV_CONNECT_TIMEOUT : ret;
    }

    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("Device invalid. (device=%d; connect_ret=%d; remote_session=%d)\n", dev->dev_id, (int)ret,
            session->remote_session_fd);
        return HDCDRV_DEVICE_RESET;
    }

    if (session->remote_session_fd == HDCDRV_INVALID_REMOTE_SESSION_ID) {
        hdcdrv_warn("Channel is invalid. (dev=%d; local_session_fd=%x)\n", dev->dev_id, session->local_session_fd);
        return HDCDRV_SESSION_CHAN_INVALID;
    }

    if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) {
        hdcdrv_warn("Session has already closed. (dev_id=%d; local_session_fd=%d; remote_session_fd=%d)\n",
            session->dev_id, session->local_session_fd, session->remote_session_fd);
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    return HDCDRV_OK;
}

bool hdcdrv_is_vf_device(u32 dev_id, u32 fid)
{
    if (fid != HDCDRV_DEFAULT_PM_FID) {
        return true;
    }

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return true;
    }

    return false;
}

STATIC void hdcdrv_uid_privilege_get(struct hdcdrv_ctrl_msg_connect *msg_connect, u32 dev_id, u32 fid)
{
    if ((hdcdrv_is_vf_device(dev_id, fid)) || (ka_task_get_current_cred() == NULL)) {
        msg_connect->euid = -1;
        msg_connect->uid =  -1;
    } else {
        msg_connect->euid = (int)ka_task_get_current_cred_euid();
        msg_connect->uid = (int)ka_task_get_current_cred_uid();
    }

    msg_connect->root_privilege = (msg_connect->euid == 0) ? 1 : 0;
}

STATIC int hdcdrv_close_remote_session(struct hdcdrv_session *session, int close_state)
{
    struct hdcdrv_ctrl_msg msg;
    u32 len = 0;
    long ret;

    msg.type = HDCDRV_CTRL_MSG_TYPE_CLOSE;
    msg.close_msg.local_session = session->local_session_fd;
    msg.close_msg.remote_session = session->remote_session_fd;
    msg.close_msg.session_close_state = close_state;
    msg.close_msg.unique_val = session->unique_val;
    msg.error_code = HDCDRV_ERR;  // clear coverity warning

    ret = hdcdrv_non_trans_ctrl_msg_send((u32)session->dev_id, (void *)&msg, (u32)sizeof(msg), (u32)sizeof(msg), &len);
    if ((ret != HDCDRV_OK) || (len != sizeof(msg)) || (msg.error_code != HDCDRV_OK)) {
        hdcdrv_link_ctrl_msg_stats_add((u32)session->dev_id, session->service_type, HDCDRV_CTRL_MSG_TYPE_CLOSE,
            HDCDRV_LINK_CTRL_MSG_SEND_FAIL, msg.error_code);
        hdcdrv_warn("Close msg send unsuccess. (dev_id=%d; pid=%llu, session=%d; service_type=\"%s\"; ret=%ld; len=%d;"
            "err_code=%d)\n", session->dev_id, session->owner_pid, session->local_session_fd,
            hdcdrv_sevice_str(session->service_type), ret, len, msg.error_code);
    } else {
        hdcdrv_link_ctrl_msg_stats_add((u32)session->dev_id, session->service_type, HDCDRV_CTRL_MSG_TYPE_CLOSE,
            HDCDRV_LINK_CTRL_MSG_SEND_SUCC, HDCDRV_OK);
    }

    return (int)ret;
}

STATIC long hdcdrv_client_destroy(const struct hdcdrv_cmd_client_destroy *cmd, u32 fid)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    int id = 0;
    int session_num = 0;
    struct hdcdrv_session *session = NULL;

    if ((cmd->service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (cmd->service_type < 0)) {
        hdcdrv_err("Input pararmeter is error. (service_type=%d)\n", cmd->service_type);
        return HDCDRV_PARA_ERR;
    }

    for (id = 0; id < HDCDRV_REAL_MAX_SESSION; id++) {
        session = &hdc_ctrl->sessions[id];
        if (session == NULL) {
            hdcdrv_warn_limit("client destroy, session(id=%d) is null.\n", id);
            return HDCDRV_ERR;
        }
        if ((hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CONN)
            && (session->create_pid == cmd->pid)
            && (session->service_type == cmd->service_type)) {
            if (session->owner_pid == HDCDRV_INVALID_PID) {
                ka_task_wake_up_interruptible(&session->wq_conn);
            }
            session_num++;
            hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x101, "session set idle. (session_fd=%d)\n",
                                   session->local_session_fd);
        }
    }
    hdcdrv_info("client destroy success.(session_num=%d; service=\"%s\"; pid=%llu)\n",
        session_num, hdcdrv_sevice_str(cmd->service_type), cmd->pid);
#endif
    return HDCDRV_OK;
}

STATIC long hdcdrv_client_wakeup_wait(const struct hdcdrv_cmd_client_wakeup_wait *cmd, u32 fid)
{
    int id = 0;
    int session_num = 0;
    struct hdcdrv_session *session = NULL;

    if ((cmd->service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (cmd->service_type < 0)) {
        hdcdrv_err("Input pararmeter is error. (service_type=%d)\n", cmd->service_type);
        return HDCDRV_PARA_ERR;
    }

    for (id = 0; id < HDCDRV_REAL_MAX_SESSION; id++) {
        session = &hdc_ctrl->sessions[id];
        if (session == NULL) {
            hdcdrv_warn_limit("client wakeup wait, session(id=%d) is null.\n", id);
            continue;
        }
        if ((hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CONN)
            && (session->create_pid == cmd->pid)
            && (session->service_type == cmd->service_type)) {
            if (session->owner_pid == HDCDRV_INVALID_PID) {
                session->remote_session_fd = HDCDRV_INVALID_REMOTE_SESSION_ID;
                ka_task_wake_up_interruptible(&session->wq_conn);
                session_num++;
                hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
                hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x102, "session set idle. (session_fd=%d)\n",
                                       session->local_session_fd);
            }
        }
    }
    hdcdrv_info_limit("client wakeup connect wait success.(session_num=%d; service=\"%s\"; pid=%llu)\n",
        session_num, hdcdrv_sevice_str(cmd->service_type), cmd->pid);

    return HDCDRV_OK;
}

STATIC long hdcdrv_connect(struct hdcdrv_cmd_connect *cmd, u32 fid)
{
    struct hdcdrv_dev *dev = NULL;
    struct hdcdrv_service *service = NULL;
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_ctrl_msg msg;
    u32 normal_chan_id;
    u32 fast_chan_id;
    u32 container_id;
    u32 connect_fid;
    u64 origin_pid;
    int session_fd;
    int run_env;
    u32 len = 0;
    long ret;

#ifdef CFG_FEATURE_HDC_REG_MEM
    unsigned long flags;

    if (hdc_ctrl->segment == HDCDRV_INVALID_PACKET_SEGMENT) {
        hdcdrv_warn("hdcdrv_connect pcie not ready (dev_id=%d; fid=%u; service_type=\"%s\")\n",
            cmd->dev_id, fid, hdcdrv_sevice_str(cmd->service_type));
        return HDCDRV_DEVICE_NOT_READY;
    }

#endif

    ret = hdcdrv_dev_para_check(cmd->dev_id, cmd->service_type);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    container_id = hdcdrv_get_container_id();
    if (container_id >= HDCDRV_DOCKER_MAX_NUM) {
        hdcdrv_err("container_id is illegal. (dev_id=%d; service_type=\"%s\"; container_id=%d)\n",
            cmd->dev_id, hdcdrv_sevice_str(cmd->service_type), container_id);
        return HDCDRV_PARA_ERR;
    }

    ret = hdcdrv_session_pre_alloc((u32)cmd->dev_id, fid, cmd->service_type);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling session_resouce_check failed. (dev_id=%d; fid=%u; service_type=\"%s\"; ret=%ld)\n",
            cmd->dev_id, fid, hdcdrv_sevice_str(cmd->service_type), ret);
        return ret;
    }

    dev = &hdc_ctrl->devices[cmd->dev_id];
    service = &dev->service[cmd->service_type];

    ka_task_mutex_lock(&service->mutex);
    service->service_stat.connect_session_num_total++;
    ka_task_mutex_unlock(&service->mutex);

    connect_fid = fid;

    /* host can get run env, set it here when connect in host */
#ifdef HDC_ENV_DEVICE
    run_env = HDCDRV_SESSION_RUN_ENV_UNKNOW;
    if ((cmd->service_type == HDCDRV_SERVICE_TYPE_FRAMEWORK) ||
        (cmd->service_type == HDCDRV_SERVICE_TYPE_PROFILING) ||
        (cmd->service_type == HDCDRV_SERVICE_TYPE_DUMP) ||
        (cmd->service_type == HDCDRV_SERVICE_TYPE_LOG)) {
        connect_fid = hdcdrv_get_fid(hdcdrv_get_pid());
    }
#else
    run_env = hdcdrv_get_session_run_env((u32)cmd->dev_id, fid);
#endif

    hdcdrv_uid_privilege_get(&msg.connect_msg, (u32)cmd->dev_id, fid);

    session_fd = hdcdrv_alloc_session(cmd->service_type, run_env, msg.connect_msg.root_privilege, cmd->pid);
    if (session_fd < 0) {
        hdcdrv_err_limit("No session resources available. (dev_id=%d; service_type=\"%s\")\n",
            cmd->dev_id, hdcdrv_sevice_str(cmd->service_type));
        hdcdrv_session_post_free((u32)cmd->dev_id, fid, cmd->service_type);
        return HDCDRV_NO_SESSION;
    }

    session = &hdc_ctrl->sessions[session_fd];

#ifdef CFG_FEATURE_HDC_REG_MEM
    if (session->mem_release_fifo.buf != NULL) {
        ka_task_spin_lock_irqsave(&session->mem_release_lock, flags);
        ka_base_kfifo_reset(&session->mem_release_fifo);
        ka_task_spin_unlock_irqrestore(&session->mem_release_lock, flags);
    }
#endif

    session->unique_val = hdcdrv_gen_unique_value();

    hdcdrv_alloc_session_chan(cmd->dev_id, (int)fid, cmd->service_type, &normal_chan_id, &fast_chan_id);

    msg.type = HDCDRV_CTRL_MSG_TYPE_CONNECT;
    msg.error_code = HDCDRV_OK;
    msg.connect_msg.client_session = session_fd;
    msg.connect_msg.service_type = cmd->service_type;
    msg.connect_msg.fast_chan_id = fast_chan_id;
    msg.connect_msg.normal_chan_id = normal_chan_id;
    msg.connect_msg.run_env = run_env;
    msg.connect_msg.fid = connect_fid;
    msg.connect_msg.unique_val = session->unique_val;
    msg.connect_msg.local_pid = cmd->pid;
    msg.connect_msg.peer_pid = (u64)cmd->peer_pid;

    hdcdrv_session_uid(session_fd, msg.connect_msg.euid, msg.connect_msg.uid, msg.connect_msg.root_privilege);
    hdcdrv_session_init(session_fd, HDCDRV_INVALID_VALUE, cmd->dev_id, cmd->service_type, run_env);

    session->container_id = container_id;
    session->local_fid = fid;
    session->owner = hdccom_get_fid_owner(session->local_fid, session->container_id);
    session->chan_id = normal_chan_id;
    session->fast_chan_id = fast_chan_id;

    (void)hdcdrv_session_chan_update(session_fd, cmd->dev_id);
    origin_pid = session->create_pid;
    session->create_pid = cmd->pid;

    ret = hdcdrv_non_trans_ctrl_msg_send((u32)cmd->dev_id, (void *)&msg, (u32)sizeof(msg), (u32)sizeof(msg), &len);
    if ((ret != HDCDRV_OK) || (len != sizeof(msg)) || (msg.error_code != HDCDRV_OK)) {
        session->create_pid = origin_pid;
        hdcdrv_set_session_status(&hdc_ctrl->sessions[session_fd], HDCDRV_SESSION_STATUS_IDLE);
        hdcdrv_session_post_free((u32)cmd->dev_id, fid, cmd->service_type);
        if ((msg.error_code == HDCDRV_REMOTE_SERVICE_NO_LISTENING) || (msg.error_code == HDCDRV_NO_SESSION) ||
            (msg.error_code == HDCDRV_NO_PERMISSION)) {
            hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT,
                HDCDRV_LINK_CTRL_MSG_SEND_FAIL, msg.error_code);
            return (long)msg.error_code;
        }

        hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT,
            HDCDRV_LINK_CTRL_MSG_SEND_FAIL, (int)ret);
        return HDCDRV_SEND_CTRL_MSG_FAIL;
    }
    hdcdrv_link_ctrl_msg_stats_add((u32)cmd->dev_id, cmd->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT,
        HDCDRV_LINK_CTRL_MSG_SEND_SUCC, HDCDRV_OK);

    /* conn reply msg maybe come first */
    ret = hdcdrv_connect_wait_reply(dev, session, cmd->timeout);
    if (ret != HDCDRV_OK) {
        session->create_pid = origin_pid;
        hdcdrv_link_ctrl_msg_stats_add(dev->dev_id, session->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
            HDCDRV_LINK_CTRL_MSG_WAIT_TIMEOUT, (int)ret);
        if ((session->remote_session_fd != HDCDRV_INVALID_VALUE) &&
            (session->remote_session_fd != HDCDRV_INVALID_REMOTE_SESSION_ID)) {
            (void)hdcdrv_close_remote_session(session, HDCDRV_CLOSE_TYPE_NOT_SET_OWNER);
        }
        hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
        hdcdrv_session_post_free((u32)cmd->dev_id, fid, cmd->service_type);
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x02, "Wait_reply unsuccess. (device=%d; service=\"%s\"; ret=%d; "
            "l_fd=%x; r_fd=%x; pid=%llu; peer_pid=%d)\n", cmd->dev_id, hdcdrv_sevice_str(cmd->service_type), (int)ret,
            session->local_session_fd, session->remote_session_fd, cmd->pid, cmd->peer_pid);
        return ret;
    }

    hdcdrv_link_ctrl_msg_stats_add(dev->dev_id, session->service_type, HDCDRV_CTRL_MSG_TYPE_CONNECT_REPLY,
        HDCDRV_LINK_CTRL_MSG_WAIT_SUCC, HDCDRV_OK);
    ka_rmb();
    hdcdrv_mod_msg_chan_session_cnt(session->dev_id, session->chan_id, session->fast_chan_id, 1);
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_session_init(session->dev_id, session->chan_id, session->fast_chan_id);
#endif

    ka_task_mutex_lock(&service->mutex);
    service->service_stat.connect_session_num++;
    ka_task_mutex_unlock(&service->mutex);

    /* response cmd */
    cmd->session = session_fd;
    cmd->session_cur_alloc_idx = session->session_cur_alloc_idx;
    hdcdrv_delay_work_set(session, &session->close_unknow_session, HDCDRV_DELAY_UNKNOWN_SESSION_BIT,
        HDCDRV_SESSION_RECLAIM_TIMEOUT);
    hdcdrv_set_session_default_owner(session, cmd->pid);

    return HDCDRV_OK;
}

STATIC void hdcdrv_session_free(struct hdcdrv_session *session, int close_state)
{
    struct hdcdrv_service *service = NULL;

    session->container_id = HDCDRV_DOCKER_MAX_NUM;
    session->unique_val = 0;
    session->owner_pid = HDCDRV_INVALID_PID;
    session->create_pid = HDCDRV_INVALID_PID;
    session->pid_flag = 0;
    session->task_start_time = HDCDRV_KERNEL_DEFAULT_START_TIME;

    if (close_state != HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST) {
        hdcdrv_delay_work_cancel(session, &session->remote_close, HDCDRV_DELAY_REMOTE_CLOSE_BIT);
    } else {
        (void)ka_task_cancel_delayed_work(&session->remote_close);
    }
    if (close_state != HDCDRV_CLOSE_TYPE_NOT_SET_OWNER) {
        hdcdrv_delay_work_cancel(session, &session->close_unknow_session, HDCDRV_DELAY_UNKNOWN_SESSION_BIT);
    } else {
        (void)ka_task_cancel_delayed_work(&session->close_unknow_session);
    }
#ifdef CFG_FEATURE_VFIO
    hdcdrv_session_work_free(&session->session_work);
#endif
    hdcdrv_mod_msg_chan_session_cnt(session->dev_id, session->chan_id, session->fast_chan_id, -1);
    hdcdrv_inner_checker_clear(session);
    hdcdrv_clear_session_rx_buf(session->local_session_fd);
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdcdrv_destroy_session_del_mem_release_event(session);
#endif
    hdcdrv_session_post_free((u32)session->dev_id, session->local_fid, session->service_type);

    ka_task_wake_up_interruptible(&session->wq_rx);
    ka_task_wake_up_interruptible(&session->wq_conn);

    if (session->msg_chan != NULL) {
        ka_task_wake_up_interruptible(&session->msg_chan->send_wait);
    }

    if (session->fast_msg_chan != NULL) {
        ka_task_wake_up_interruptible(&session->fast_msg_chan->send_wait);
    }

    hdcdrv_epoll_wake_up(session->epfd);

    service = session->service;
    if (service != NULL) {
        ka_task_mutex_lock(&service->mutex);
        service->service_stat.close_session_num++;
        ka_task_mutex_unlock(&service->mutex);
    }
    hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
}

/* this functions are called in remote work and upper layers */
long hdcdrv_close(const struct hdcdrv_cmd_close *cmd, int close_state)
{
    struct hdcdrv_session *session = NULL;
    int last_status = 0;
    int retry_cnt = 0;
    long ret;

    if ((cmd->session >= HDCDRV_REAL_MAX_SESSION) || (cmd->session < 0)) {
        hdcdrv_err_limit("Input pararmeter is error. (session=%d)\n", cmd->session);
        return HDCDRV_PARA_ERR;
    }
    session = &hdc_ctrl->sessions[cmd->session];
    ret = hdcdrv_session_state_to_closing(cmd, &last_status, close_state);
    if (ret != HDCDRV_OK) {
        if (ret == HDCDRV_SESSION_HAS_CLOSED) {
            return HDCDRV_OK;
        } else {
            return ret;
        }
    }

    session->local_close_state = close_state;

    /* In case of connection notification peer */
    if (last_status == HDCDRV_SESSION_STATUS_CONN) {
        do {
            ret = hdcdrv_close_remote_session(session, close_state);
            if (ret != HDCDRV_OK) {
                retry_cnt++;
            } else {
                break;
            }
        } while (retry_cnt <= HDCDRV_CLOSE_RMT_SESSION_RETRY_CNT);
    }

    hdcdrv_session_free(session, close_state);
    return HDCDRV_OK;
}

STATIC long hdcdrv_send(struct hdcdrv_cmd_send *cmd, int mode)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_buf_desc buf_d = {0};
    u64 timeout;
    long ret;
    int retry_cnt = 0;

    if ((cmd == NULL) || (cmd->src_buf == NULL)) {
        hdcdrv_err("Input pararmeter is error.\n");
        return HDCDRV_PARA_ERR;
    }

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&buf_d.latency_info.send_timestamp);
#endif

AGAIN:
    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    if (ka_unlikely((cmd->len > hdcdrv_mem_block_capacity()) || (cmd->len <= 0))) {
        hdcdrv_err("Send length is too bigger. (session=%d; send_len=%d; max_segment=%d)\n", cmd->session,
            cmd->len, hdcdrv_mem_block_capacity());
        return HDCDRV_TX_LEN_ERR;
    }

    session = &hdc_ctrl->sessions[cmd->session];

    if (hdcdrv_get_session_status(session) != HDCDRV_SESSION_STATUS_CONN) {
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x0A, "Session remote close. (dev=%d; session_fd=%d; "
            "service_type=\"%s\"; l_session_fd=%d; r_session_fd=%d)\n", session->dev_id, cmd->session,
            hdcdrv_sevice_str(session->service_type), session->local_session_fd, session->remote_session_fd);
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x15, "Session remote close. (dev=%d; session_fd=%d; "
            "l_close_state=\"%s\"; r_close_state=\"%s\")\n", session->dev_id, cmd->session,
            hdcdrv_close_str(session->local_close_state), hdcdrv_close_str(session->remote_close_state));
        return HDCDRV_TX_REMOTE_CLOSE;
    }

    ret = hdcdrv_tx_alloc_mem(session, cmd, &buf_d);
    if (ret != HDCDRV_OK) {
        retry_cnt++;
        session->stat.alloc_mem_err++;
        if (session->msg_chan != NULL) {
            ka_task_mutex_lock(&session->msg_chan->mutex);
            session->msg_chan->stat.alloc_mem_err++;
            ka_task_mutex_unlock(&session->msg_chan->mutex);
        }
        if (retry_cnt > HDCDRV_SEND_ALLOC_MEM_RETRY_TIME) {
            hdcdrv_err_limit("Send alloc dma memory failed. (dev=%d; session=%d; service_type=\"%s\")\n",
                session->dev_id, cmd->session, hdcdrv_sevice_str(session->service_type));
            return HDCDRV_DMA_MEM_ALLOC_FAIL;
        }

        ka_system_msleep(5);  // sleep 5 ms
        goto AGAIN;
    }

    ret = hdcdrv_copy_from_user(session, buf_d.buf, cmd, mode);
    if (ret != HDCDRV_OK) {
        hdcdrv_free_mem(session, buf_d.buf, HDCDRV_MEMPOOL_FREE_IN_VM, &buf_d);
        hdcdrv_err("Calling copy_from_user failed. (dev=%d; session=%d; service_type=\"%s\"; ret=%ld)\n",
            session->dev_id, cmd->session, hdcdrv_sevice_str(session->service_type), ret);
        return ret;
    }

    buf_d.len = cmd->len;
    buf_d.local_session = session->local_session_fd;
    buf_d.remote_session = session->remote_session_fd;
    buf_d.skip_flag = HDCDRV_INVALID;
    timeout = hdcdrv_set_send_timeout(session, cmd->timeout);

#ifdef CFG_FEATURE_MIRROR
    ka_mm_dma_sync_single_for_device(session->msg_chan->dev, buf_d.addr, (u32)buf_d.len, KA_DMA_TO_DEVICE);
#endif

    ret = hdcdrv_msg_chan_send(session->msg_chan, &buf_d, cmd->wait_flag, timeout);
    if (ret != HDCDRV_OK) {
        hdcdrv_free_mem(session, buf_d.buf, HDCDRV_MEMPOOL_FREE_IN_VM, &buf_d);
    }

    return ret;
}

STATIC long hdcdrv_recv_peek_wait(struct hdcdrv_session *session, struct hdcdrv_cmd_recv_peek *cmd)
{
    struct hdcdrv_service *service = NULL;
    u32 unique_val = session->unique_val;
    int session_status;
    int retry_cnt = 0;
    u64 timeout;
    long ret;

    timeout = hdcdrv_set_send_timeout(session, cmd->timeout);
    if (cmd->wait_flag == HDCDRV_NOWAIT) {
        while ((session->service_type == HDCDRV_SERVICE_TYPE_TSD) && (retry_cnt < HDCDRV_NOWAIT_RETRY_TIMES)) {
            if (session->normal_rx.head != session->normal_rx.tail) {
                return 0;
            }
            ka_system_usleep_range(HDCDRV_NOWAIT_USLEEP_MIN, HDCDRV_NOWAIT_USLEEP_MAX);
            retry_cnt++;

            session_status = hdcdrv_get_session_status(session);
            if ((session->remote_session_close_flag == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) &&
                (session_status == HDCDRV_SESSION_STATUS_REMOTE_CLOSED || session_status == HDCDRV_SESSION_STATUS_CONN)) {
                /* if session_status is conn or remote close, need retry recv */
                continue;
            }

            if ((session_status == HDCDRV_SESSION_STATUS_IDLE || session_status == HDCDRV_SESSION_STATUS_CLOSING) ||
                (hdcdrv_session_alloc_idx_check(cmd->session, cmd->session_cur_alloc_idx) == HDCDRV_SESSION_HAS_CLOSED) ||
                (unique_val != session->unique_val)) {
                hdcdrv_warn_limit("Session has close.(dev=%d; session=%d, status=%d, service=%d; l_state=%d; "
                    "r_state=%d; l_fd=%d; r_fd=%d.\n",
                    session->dev_id, cmd->session, session_status, session->service_type, session->local_close_state,
                    session->remote_close_state, session->local_session_fd, session->remote_session_fd);
                hdcdrv_warn_limit("Session has close.(dev=%d; session=%d, unique_val=(%u,%u); alloc_idx=(%u,%u); "
                    "remote_close_flag=%d.\n",
                    session->dev_id, cmd->session, unique_val, session->unique_val, cmd->session_cur_alloc_idx,
                    session->session_cur_alloc_idx, session->remote_session_close_flag);
                return HDCDRV_SESSION_HAS_CLOSED;
            }
        };

        return HDCDRV_NO_BLOCK;
    } else if (cmd->wait_flag == HDCDRV_WAIT_ALWAYS) {
        /*lint -e666*/
        ret = (long)ka_task_wait_event_interruptible(session->wq_rx,
            (session->normal_rx.head != session->normal_rx.tail) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
            (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) ||
            (session->unique_val != unique_val));
        /*lint +e666*/
    } else {
        /*lint -e666*/
        ret = ka_task_wait_event_interruptible_timeout(session->wq_rx,
            (session->normal_rx.head != session->normal_rx.tail) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
            (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
            (session->unique_val != unique_val),
            (long)timeout);
        /*lint +e666*/
        if (ret == 0) {
            if ((hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
                (hdcdrv_session_alloc_idx_check(cmd->session, cmd->session_cur_alloc_idx) ==
                HDCDRV_SESSION_HAS_CLOSED)) {
                return HDCDRV_SESSION_HAS_CLOSED;
            }
            return HDCDRV_RX_TIMEOUT;
        } else if (ret > 0) {
            ret = 0;
        }
    }
    hdcdrv_wakeup_record_resq_time(session->dev_id, session->service_type, session->recv_peek_stamp,
        HDCDRV_WAKE_UP_WAIT_TIMEOUT , "hdc recv peek wake up stamp");

    if (ret != 0) {
        if (session->service_type == HDCDRV_SERVICE_TYPE_LOG) {
#ifndef DRV_UT
            service = &hdc_ctrl->devices[session->dev_id].service[session->service_type];
            HDC_LOG_WARN_LIMIT(&service->service_stat.recv_print_cnt, &service->service_stat.recv_jiffies,
                "Recv peek wait.(dev=%d; ret=%d; head=%d; tail=%d; l_fd=%d; r_fd=%d)\n",
                session->dev_id, (int)ret, session->normal_rx.head, session->normal_rx.tail,
                session->local_session_fd, session->remote_session_fd);
            HDC_LOG_WARN_LIMIT(&service->service_stat.recv_print_cnt1, &service->service_stat.recv_jiffies1,
                "Recv peek wait.(dev=%d; service_type=\"%s\"; l_state=\"%s\"; r_state=\"%s\")\n",
                session->dev_id, hdcdrv_sevice_str(session->service_type), hdcdrv_close_str(session->local_close_state),
                hdcdrv_close_str(session->remote_close_state));
#endif
        } else {
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x08, "Recv peek wait.(dev=%d; ret=%d; "
                "head=%d; tail=%d; l_fd=%d; r_fd=%d)\n", session->dev_id, (int)ret, session->normal_rx.head,
                session->normal_rx.tail, session->local_session_fd, session->remote_session_fd);
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x11, "Recv peek wait.(dev=%d; service=\"%s\"; "
                "l_state=\"%s\"; r_state=\"%s\")\n", session->dev_id, hdcdrv_sevice_str(session->service_type),
                hdcdrv_close_str(session->local_close_state), hdcdrv_close_str(session->remote_close_state));
        }
        return ret;
    }

    if (session->unique_val != unique_val) {
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x09, "Unique val not match.(dev=%d; service=\"%s\"; "
            "l_fd=%d; r_fd=%d)\n", session->dev_id, hdcdrv_sevice_str(session->service_type),
            session->local_session_fd, session->remote_session_fd);
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x12, "Unique val not match.(dev=%d; l_state=\"%s\"; "
            "r_state=\"%s\")\n", session->dev_id, hdcdrv_close_str(session->local_close_state),
            hdcdrv_close_str(session->remote_close_state));
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    return 0;
}

STATIC long hdcdrv_recv_peek(struct hdcdrv_cmd_recv_peek *cmd)
{
    struct hdcdrv_session *session = NULL;
    long ret;
    int tmp_head;

    ret = hdcdrv_session_alloc_idx_check(cmd->session, cmd->session_cur_alloc_idx);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    session = &hdc_ctrl->sessions[cmd->session];
    ka_task_spin_lock_bh(&session->lock);

    while (((hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CONN) ||
        (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_REMOTE_CLOSED)) &&
        (session->normal_rx.head == session->normal_rx.tail) &&
        (hdcdrv_get_peer_status() == DEVDRV_PEER_STATUS_NORMAL)) {
        ka_task_spin_unlock_bh(&session->lock);

        ret = hdcdrv_recv_peek_wait(session, cmd);
        if (ret != 0) {
            return ret;
        }
        ka_task_spin_lock_bh(&session->lock);
    }

    if (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) {
        ka_task_spin_unlock_bh(&session->lock);
        hdcdrv_info_limit("peer status abnormal, stop recv\n");
        return HDCDRV_PEER_REBOOT;
    }

    /* session is closed */
    if (session->normal_rx.head == session->normal_rx.tail) {
        cmd->len = 0;
        cmd->count = 0;
        ka_task_spin_unlock_bh(&session->lock);
        hdcdrv_info_limit("Session close. (dev=%d; l_close_state=\"%s\"; r_close_state=\"%s\"; l_fd=%d; r_fd=%d)\n",
            session->dev_id, hdcdrv_close_str(session->local_close_state),
            hdcdrv_close_str(session->remote_close_state), session->local_session_fd, session->remote_session_fd);
        return HDCDRV_OK;
    }

    if (cmd->group_flag == 0) {  // 0 is normal recv, return one rx len to user
        cmd->len = session->normal_rx.rx_list[session->normal_rx.head].len;
        cmd->count = 1;
    } else {
        tmp_head = session->normal_rx.head;
        cmd->len = 0;
        cmd->count = 0;
        while (tmp_head != session->normal_rx.tail) {
            cmd->len += session->normal_rx.rx_list[tmp_head].len;
            cmd->count++;
            tmp_head = (tmp_head + 1) % HDCDRV_SESSION_RX_LIST_MAX_PKT;
        }
    }

    ka_task_spin_unlock_bh(&session->lock);
    return HDCDRV_OK;
}

STATIC long hdcdrv_recv(struct hdcdrv_cmd_recv *cmd, int mode)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_dev *hdc_dev = NULL;
    struct hdcdrv_buf_desc *buf_desc = NULL;
    struct hdcdrv_msg_chan *msg_chan = NULL;
#ifdef CFG_FEATURE_VFIO
    ka_sg_table_t *dma_sgt[HDCDRV_SESSION_RX_LIST_MAX_PKT] = {0};
    u32 dev_id[HDCDRV_SESSION_RX_LIST_MAX_PKT] = {0};
    u32 fid[HDCDRV_SESSION_RX_LIST_MAX_PKT] = {0};
#endif
#ifdef CFG_FEATURE_MIRROR
    int mem_id_list[HDCDRV_SESSION_RX_LIST_MAX_PKT] = {0};
#endif
    long ret;
    int len, count = 0, rx_count = 0, rx_index = 0, real_len = 0;

    if (cmd == NULL) {
        hdcdrv_err("Input pararmeter cmd is null.\n");
        return HDCDRV_PARA_ERR;
    }

    if ((cmd->dst_buf == NULL) || (cmd->buf_count >= HDCDRV_SESSION_RX_LIST_MAX_PKT) || (cmd->buf_count <= 0)) {
        hdcdrv_err("Input pararmeter is error.(buf_count=%d)\n", cmd->buf_count);
        return HDCDRV_PARA_ERR;
    }
    if (mode != HDCDRV_MODE_KERNEL) {
        ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
        if (ret != 0) {
            return ret;
        }
    }
    session = &hdc_ctrl->sessions[cmd->session];

    hdc_dev = &hdc_ctrl->devices[session->dev_id];
    msg_chan = hdc_dev->msg_chan[session->chan_id];

    ka_task_mutex_lock(&msg_chan->mutex);
    msg_chan->dbg_stat.hdcdrv_recv_data_times++;
    session->dbg_stat.hdcdrv_recv_data_times++;
    ka_task_mutex_unlock(&msg_chan->mutex);

    ka_task_spin_lock_bh(&session->lock);
    session->stat.rx_total++;
    /* session is closed or no data here */
    /* Some services use the length as the termination receiving condition, so no log is printed */
    if (session->normal_rx.head == session->normal_rx.tail) {
        cmd->out_len = 0;
        ka_task_spin_unlock_bh(&session->lock);
        return HDCDRV_OK;
    }

    /* we should get buf addr from the head postion of rx_list firstly.
       when move head, unlock, in the old head postion of rx_list will be use in other thread */
    rx_index = session->normal_rx.head;

    while ((rx_index != session->normal_rx.tail) && (rx_count < cmd->buf_count)) {
        buf_desc = &session->normal_rx.rx_list[rx_index];
        len = buf_desc->len;
        real_len += len;
        if (real_len > cmd->len) {                // The actual required length exceeds the reserved length
            ka_task_spin_unlock_bh(&session->lock);
            hdcdrv_err("cmd_len is invalid. (dev=%d; session=%d; service type=\"%s\"; "
                "recv_buf=%d; len=%d; buf_count=%d; now_buf=%d)\n", session->dev_id, cmd->session,
                hdcdrv_sevice_str(session->service_type), cmd->len, real_len, cmd->buf_count, rx_count);
            return HDCDRV_RX_BUF_SMALL;
        }
        cmd->buf_list[rx_count] = buf_desc->buf;
        cmd->buf_len[rx_count] = (unsigned int)buf_desc->len;
        rx_count++;
        rx_index = (session->normal_rx.head + rx_count) % HDCDRV_SESSION_RX_LIST_MAX_PKT;
    }

    if (rx_count != cmd->buf_count) {
        ka_task_spin_unlock_bh(&session->lock);
        hdcdrv_err("buf count is invalid. (dev=%d; session=%d; service type=\"%s\"; "
            "cmd->buf_count=%d; real buf_count=%d).\n", session->dev_id, cmd->session,
            hdcdrv_sevice_str(session->service_type), cmd->buf_count, rx_count);
        return HDCDRV_RX_BUF_SMALL;
    }

    count = 0;
    while (count < rx_count) {
        rx_index = (session->normal_rx.head + count) % HDCDRV_SESSION_RX_LIST_MAX_PKT;
        buf_desc = &session->normal_rx.rx_list[rx_index];
#ifdef CFG_FEATURE_VFIO
        dma_sgt[count] = buf_desc->dma_sgt;
        dev_id[count] = buf_desc->dev_id;
        fid[count] = buf_desc->fid;
#endif
#ifdef CFG_FEATURE_MIRROR
        mem_id_list[count] = buf_desc->mem_id;
#endif
        hdcdrv_desc_clear(buf_desc);
        count++;
    }

    session->normal_rx.head = (session->normal_rx.head + rx_count) % HDCDRV_SESSION_RX_LIST_MAX_PKT;
    if (msg_chan->rx_trigger_flag == HDCDRV_VALID) {
        msg_chan->rx_trigger_flag = HDCDRV_INVALID;
        ka_system_tasklet_schedule(&msg_chan->rx_task);
    }

    ka_task_spin_unlock_bh(&session->lock);

#ifdef CFG_FEATURE_VFIO
        if (session->owner == HDCDRV_SESSION_OWNER_VM) {
            for (count = 0; count < rx_count; count++) {
                hdcdrv_dma_unmap_guest_page(dev_id[count], fid[count], dma_sgt[count]);
            }
        }
#endif
    ka_task_mutex_lock(&msg_chan->mutex);
    hdcdrv_msg_chan_rx_static(msg_chan);
    ka_task_mutex_unlock(&msg_chan->mutex);
#ifdef CFG_FEATURE_MIRROR
    ret = hdcdrv_copy_to_user(session, cmd, real_len, mode, mem_id_list);
#else
    ret = hdcdrv_copy_to_user(session, cmd, real_len, mode, NULL);
#endif
    return ret;
}

STATIC int hdcdrv_calc_page_start_idx(u64 send_va, int len, struct hdcdrv_fast_mem *f_mem,
    struct hdcdrv_fast_page_info *addr_info)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    u32 page_nums;
    u32 page_size = f_mem->align_size;

    if (page_size == 0) {
        addr_info->page_end = (u32)f_mem->phy_addr_num;
        addr_info->send_inner_page_offset = 0;
        addr_info->page_start = 0;
        return HDCDRV_OK;
    }
    if (send_va < f_mem->user_va) {
        hdcdrv_err("hdcdrv_calc_page_start_idx, (send_va=0x%llx; va=0x%llx; page_size=%u)\n",
                   send_va, f_mem->user_va, page_size);
        return HDCDRV_ERR;
    }

    addr_info->page_start = (u32)(send_va - (f_mem->user_va - f_mem->register_inner_page_offset)) / page_size;
    page_nums = (u32)((len + (send_va % page_size) + page_size - 1) / page_size);
    addr_info->send_inner_page_offset = send_va % page_size;
    addr_info->page_end = addr_info->page_start + page_nums;
    if (addr_info->page_end > (u32)f_mem->phy_addr_num) {
        hdcdrv_err("hdcdrv_calc_page_start_idx, (send_va=0x%llx; va=0x%llx len=%d)\n", send_va,
            f_mem->user_va, len);
        return HDCDRV_ERR;
    }
#else
    (void)send_va;
    (void)len;
    addr_info->page_start = 0;
    addr_info->page_end = (u32)f_mem->phy_addr_num;
#endif
    return HDCDRV_OK;
}

STATIC int hdcdrv_sync_used_fast_mem(struct hdcdrv_fast_mem *f_mem, ka_device_t *dev,
    int type, int len, u64 user_va)
{
    int ret;
    u32 i, dma_len, total_len;
    ka_dma_addr_t addr;
    struct hdcdrv_fast_page_info addr_info = {0};

    ret = hdcdrv_calc_page_start_idx(user_va, len, f_mem, &addr_info);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    total_len = 0;
    for (i = addr_info.page_start; i < addr_info.page_end; i++) {
        addr = f_mem->mem[i].addr;
        dma_len = f_mem->mem[i].len;
#ifdef CFG_FEATURE_HDC_REG_MEM
        if (i == addr_info.page_start) {
            if (addr_info.page_start == 0) {
                addr = f_mem->mem[i].addr + (addr_info.send_inner_page_offset - f_mem->register_inner_page_offset);
            } else {
                addr = f_mem->mem[i].addr + addr_info.send_inner_page_offset;
            }

            dma_len = ka_base_min((u32)len, f_mem->align_size - addr_info.send_inner_page_offset);
        } else if (i == addr_info.page_end - 1) {
            dma_len = (u32)(len - total_len);
        }
#endif
        /* dma_map_page or dma_map_single address, cache consistency is not
        guaranteed(arm), need to cooperate with dma_sync_single */
        if ((type == HDCDRV_FAST_MEM_TYPE_TX_DATA) || (type == HDCDRV_FAST_MEM_TYPE_TX_CTRL)) {
            ka_mm_dma_sync_single_for_device(dev, addr, dma_len, KA_DMA_TO_DEVICE);
        } else {
            ka_mm_dma_sync_single_for_cpu(dev, addr, dma_len, KA_DMA_FROM_DEVICE);
        }

        total_len += dma_len;
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_sync_fast_mem(const struct hdcdrv_session *session,
    int type, u64 user_va, int len, u64 hash_va)
{
    struct hdcdrv_fast_mem *f_mem = NULL;
    long ret;
#ifdef CFG_FEATURE_HDC_REG_MEM
    if ((len == 0) || (user_va == 0)) {
        return HDCDRV_PARA_ERR;
    }
#endif

    f_mem = hdcdrv_get_fast_mem_timeout(session->dev_id, type, len, hash_va, user_va);
    if (f_mem == NULL) {
        return HDCDRV_F_NODE_SEARCH_FAIL;
    }

    ret = hdcdrv_sync_used_fast_mem(f_mem, session->fast_msg_chan->dev, type, len, user_va);
    if (ret != HDCDRV_OK) {
        hdcdrv_node_status_idle_by_mem(f_mem);
        return ret;
    }

    hdcdrv_node_status_idle_by_mem(f_mem);
    return HDCDRV_OK;
}

STATIC long hdcdrv_fast_send_check(const struct hdcdrv_cmd_fast_send *cmd)
{
    long ret;

    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    if (((cmd->src_data_addr == 0) && (cmd->src_ctrl_addr == 0)) ||
        ((cmd->data_len == 0) && (cmd->ctrl_len == 0))) {
        hdcdrv_err("Fast send addr or data len not correct.\n");
        return HDCDRV_PARA_ERR;
    }
#ifdef CFG_FEATURE_HDC_REG_MEM
    if ((cmd->src_data_addr != ka_base_round_down(cmd->src_data_addr, HDCDRV_MEM_CACHE_LINE)) ||
        (cmd->src_ctrl_addr != ka_base_round_down(cmd->src_ctrl_addr, HDCDRV_MEM_CACHE_LINE))) {
        hdcdrv_err("Fast send addr not align cacheline, src_data_addr=%llx src_ctrl_addr=%llx.\n",
            cmd->src_data_addr, cmd->src_ctrl_addr);
        return HDCDRV_PARA_ERR;
    }

    if ((cmd->dst_data_addr != ka_base_round_down(cmd->dst_data_addr, HDCDRV_MEM_CACHE_LINE)) ||
        (cmd->dst_ctrl_addr != ka_base_round_down(cmd->dst_ctrl_addr, HDCDRV_MEM_CACHE_LINE))) {
        hdcdrv_err("Fast send addr not align cacheline, dst_data_addr=%llx dst_ctrl_addr=%llx.\n",
            cmd->dst_data_addr, cmd->dst_ctrl_addr);
        return HDCDRV_PARA_ERR;
    }

    if (cmd->ctrl_len > HDCDRV_CTRL_MEM_SEND_MAX_LEN) {
        hdcdrv_err("Fast send ctrl len not correct, ctrl_len=%u.\n", cmd->ctrl_len);
        return HDCDRV_PARA_ERR;
    }
#endif
    if ((cmd->data_len < 0) || (cmd->ctrl_len < 0)) {
        hdcdrv_err("Fast send length invalid.\n");
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_fast_send(const struct hdcdrv_cmd_fast_send *cmd)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_buf_desc buf_d = {0};
    u64 timeout;
    u32 fid;
    long ret;
    long data_ret;
    long ctrl_ret;

#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_get_timestamp(&buf_d.latency_info.send_timestamp);
#endif
    ret = hdcdrv_fast_send_check(cmd);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    session = &hdc_ctrl->sessions[cmd->session];

    if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_REMOTE_CLOSED) {
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x04, "Session remote closed. (dev=%d; session_fd=%d; "
            "service_type=\"%s\"; l_close_state=\"%s\"; r_close_state=\"%s\"; l_session_fd=%d; r_session_fd=%d)\n",
            session->dev_id, cmd->session, hdcdrv_sevice_str(session->service_type),
            hdcdrv_close_str(session->local_close_state), hdcdrv_close_str(session->remote_close_state),
            session->local_session_fd, session->remote_session_fd);
        return HDCDRV_TX_REMOTE_CLOSE;
    }

    fid = (session->owner == HDCDRV_SESSION_OWNER_VM) ? session->local_fid : 0;

    buf_d.local_session = session->local_session_fd;
    buf_d.remote_session = session->remote_session_fd;

    /* data pair */
    buf_d.addr = hdcdrv_get_hash(cmd->src_data_addr, cmd->pid, fid);
    buf_d.src_data_addr_va = cmd->src_data_addr;
    buf_d.len = cmd->data_len;

    buf_d.dst_data_addr = cmd->dst_data_addr;

    /* strl msg pair */
    buf_d.src_ctrl_addr = hdcdrv_get_hash(cmd->src_ctrl_addr, cmd->pid, fid);
    buf_d.src_ctrl_addr_va = cmd->src_ctrl_addr;
    buf_d.ctrl_len = cmd->ctrl_len;

    buf_d.dst_ctrl_addr = cmd->dst_ctrl_addr;
    buf_d.skip_flag = HDCDRV_INVALID;
    buf_d.buf = NULL;
    buf_d.status = 0;
    buf_d.pid = (u32)cmd->pid;
    buf_d.fid = fid;
    timeout = (u64)hdcdrv_set_fast_send_timeout(session, cmd->timeout);

    data_ret = hdcdrv_sync_fast_mem(session, HDCDRV_FAST_MEM_TYPE_TX_DATA,
        cmd->src_data_addr, cmd->data_len, buf_d.addr);
    if (data_ret != HDCDRV_OK) {
        hdcdrv_warn_limit("fast tx data send not success. (dev=%d; session=%d; data_ret=%ld; data_len=%d)\n",
            session->dev_id, cmd->session, data_ret, cmd->data_len);
    }

    ctrl_ret = hdcdrv_sync_fast_mem(session, HDCDRV_FAST_MEM_TYPE_TX_CTRL,
        cmd->src_ctrl_addr, cmd->ctrl_len, buf_d.src_ctrl_addr);
    if (data_ret != HDCDRV_OK) {
        hdcdrv_warn_limit("fast tx ctrl send not success. (dev=%d; session=%d; ctrl_ret=%ld; ctrl_len=%d)\n",
            session->dev_id, cmd->session, data_ret, cmd->ctrl_len);
    }

    if ((data_ret != HDCDRV_OK) && (ctrl_ret != HDCDRV_OK)) {
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x05, "fast send not success. (dev=%d; session=%d; "
            "data_ret=%ld; ctrl_ret=%ld)\n", session->dev_id, cmd->session, data_ret, ctrl_ret);
        return HDCDRV_F_NODE_SEARCH_FAIL;
    }
    return hdcdrv_msg_chan_send(session->fast_msg_chan, &buf_d, cmd->wait_flag, timeout);
}

STATIC long hdcdrv_fast_recv(struct hdcdrv_cmd_fast_recv *cmd)
{
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_msg_chan *msg_chan = NULL;
    struct hdcdrv_session_fast_rx *fast_rx = NULL;
    struct hdcdrv_fast_rx *rx_buf = NULL;
    u32 unique_val = 0;
    u64 timeout;
    u32 fid;
    long ret;

    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    session = &hdc_ctrl->sessions[cmd->session];
    msg_chan = hdc_ctrl->devices[session->dev_id].msg_chan[session->fast_chan_id];
    unique_val = session->unique_val;

    ka_task_mutex_lock(&msg_chan->mutex);
    msg_chan->dbg_stat.hdcdrv_recv_data_times++;
    ka_task_mutex_unlock(&msg_chan->mutex);

    fast_rx = hdcdrv_get_session_fast_rx(cmd->session);

    ka_task_spin_lock_bh(&session->lock);
    session->stat.rx_total++;
    timeout = hdcdrv_set_send_timeout(session, cmd->timeout);
    while (
        ((hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CONN) ||
        (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_REMOTE_CLOSED)) &&
        (fast_rx->head == fast_rx->tail) &&
        (hdcdrv_get_peer_status() == DEVDRV_PEER_STATUS_NORMAL)) {
        ka_task_spin_unlock_bh(&session->lock);
        if (cmd->wait_flag == HDCDRV_NOWAIT) {
            return HDCDRV_NO_BLOCK;
        } else if (cmd->wait_flag == HDCDRV_WAIT_ALWAYS) {
            /*lint -e666*/
            ret = (long)ka_task_wait_event_interruptible(session->wq_rx,
                (fast_rx->head != fast_rx->tail) ||
                (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
                (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
                (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) ||
                (session->unique_val != unique_val));
            /*lint +e666*/
        } else {
            /*lint -e666*/
            ret = ka_task_wait_event_interruptible_timeout(session->wq_rx,
                (fast_rx->head != fast_rx->tail) ||
                (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CLOSING) ||
                (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) ||
                (session->unique_val != unique_val),
                (long)timeout);
            /*lint +e666*/
            if (ret == 0) {
                return HDCDRV_RX_TIMEOUT;
            } else if (ret > 0) {
                ret = 0;
            }
        }

        if (ret != 0) {
            hdcdrv_warn("Get session. (dev=%d; session=%d; service_type=\"%s\"; fast_recv_wait_head=%d; "
                "tail=%d; ret=%d; l_close_state=\"%s\"; r_close_state=\"%s\"; l_session_fd=%d; r_session_fd=%d)\n",
                session->dev_id, cmd->session, hdcdrv_sevice_str(session->service_type), fast_rx->head, fast_rx->tail,
                (int)ret, hdcdrv_close_str(session->local_close_state), hdcdrv_close_str(session->remote_close_state),
                session->local_session_fd, session->remote_session_fd);
            return ret;
        }

        if (session->unique_val != unique_val) {
            hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x03, "Unique val not match.(dev=%d; service=\"%s\"; "
                "l_state=\"%s\"; r_state=\"%s\"; l_fd=%d; r_fd=%d)\n", session->dev_id,
                hdcdrv_sevice_str(session->service_type), hdcdrv_close_str(session->local_close_state),
                hdcdrv_close_str(session->remote_close_state), session->local_session_fd, session->remote_session_fd);
            return HDCDRV_SESSION_HAS_CLOSED;
        }

        ka_task_spin_lock_bh(&session->lock);
    }

    if (hdcdrv_get_peer_status() != DEVDRV_PEER_STATUS_NORMAL) {
        ka_task_spin_unlock_bh(&session->lock);
        hdcdrv_info_limit("peer status abnormal, stop recv\n");
        return HDCDRV_PEER_REBOOT;
    }

    /* session is closed */
    if (fast_rx->head == fast_rx->tail) {
        ka_task_spin_unlock_bh(&session->lock);
        cmd->ctrl_addr = 0;
        cmd->data_addr = 0;
        cmd->data_len = 0;
        cmd->ctrl_len = 0;

        hdcdrv_info_limit("Session close. (dev=%d; l_state=\"%s\"; r_state=\"%s\"; l_session_fd=%d; r_session_fd=%d)\n",
            session->dev_id, hdcdrv_close_str(session->local_close_state),
            hdcdrv_close_str(session->remote_close_state), session->local_session_fd, session->remote_session_fd);
        return HDCDRV_OK;
    }

    rx_buf = &fast_rx->rx_list[fast_rx->head];

    /* Insert into the tail of the receive list */
    cmd->data_addr = (u64)rx_buf->data_addr;
    cmd->ctrl_addr = (u64)rx_buf->ctrl_addr;
    cmd->data_len = rx_buf->data_len;
    cmd->ctrl_len = rx_buf->ctrl_len;
    fast_rx->head = (fast_rx->head + 1) % HDCDRV_BUF_MAX_CNT;
    if (msg_chan->rx_trigger_flag == HDCDRV_VALID) {
        msg_chan->rx_trigger_flag = HDCDRV_INVALID;
        ka_system_tasklet_schedule(&msg_chan->rx_task);
    }
    ka_task_spin_unlock_bh(&session->lock);

    ka_task_mutex_lock(&msg_chan->mutex);
    msg_chan->stat.rx_finish++;
    hdcdrv_msg_chan_rx_static(msg_chan);
    ka_task_mutex_unlock(&msg_chan->mutex);

    fid = (session->owner == (u32)HDCDRV_SESSION_OWNER_VM) ? session->local_fid : 0;

    ret = hdcdrv_sync_fast_mem(session, HDCDRV_FAST_MEM_TYPE_RX_DATA,
        cmd->data_addr, cmd->data_len, hdcdrv_get_hash(cmd->data_addr, session->owner_pid, fid));
    if (ret != HDCDRV_OK) {
        hdcdrv_warn_limit("fast rx data sync not success. (dev=%d; session=%d; data_ret=%ld; data_len=%d)\n",
            session->dev_id, cmd->session, ret, cmd->data_len);
    }

    ret = hdcdrv_sync_fast_mem(session, HDCDRV_FAST_MEM_TYPE_RX_CTRL,
        cmd->ctrl_addr, cmd->ctrl_len, hdcdrv_get_hash(cmd->ctrl_addr, session->owner_pid, fid));
    if (ret != HDCDRV_OK) {
        hdcdrv_warn_limit("fast rx ctrl sync not success. (dev=%d; session=%d; ctrl_ret=%ld; ctrl_len=%d)\n",
            session->dev_id, cmd->session, ret, cmd->ctrl_len);
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_set_session_owner(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_set_session_owner *cmd, u32 fid)
{
    struct hdcdrv_session *session = NULL;
    int session_status;
    long ret;

    if (ctx == NULL) {
        hdcdrv_err("Input pararmeter is error. (session_id=%d)\n", cmd->session);
        return HDCDRV_PARA_ERR;
    }

    ret = hdcdrv_session_para_check(cmd->session, cmd->dev_id);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    if (fid == HDCDRV_DEFAULT_PM_FID) {
        cmd->ppid = hdcdrv_rebuild_pid((u32)cmd->dev_id, fid, hdcdrv_get_ppid());
    }

    session = &hdc_ctrl->sessions[cmd->session];
    ka_task_mutex_lock(&session->mutex);

    if ((session->create_pid != cmd->pid) && (session->create_pid != cmd->ppid)) {
        ka_task_mutex_unlock(&session->mutex);
        hdcdrv_err("No permission for session. (session=%d; creat_pid=%llu; cmd_pid=%llu; ppid=%llu)\n",
            cmd->session, session->create_pid, cmd->pid, cmd->ppid);
        return HDCDRV_NO_PERMISSION;
    }

    if (session->pid_flag != 0) {
        ka_task_mutex_unlock(&session->mutex);
        hdcdrv_err("Current pid is invalid. (Current_pid=%llu; session_id=%d; owner_pid=%llu)\n",
            cmd->pid, cmd->session, session->owner_pid);
        return HDCDRV_ERR;
    }

    if ((session->delay_work_flag & KA_BASE_BIT(HDCDRV_DELAY_UNKNOWN_SESSION_BIT)) == 0) {
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x0F, "delay_work_cancel not success. (status=%d, "
            "local_close=%d, remote_close=%d, session=%d,service=%d).\n", hdcdrv_get_session_status(session),
            session->local_close_state, session->remote_close_state, cmd->session, session->service_type);
    }

    hdcdrv_delay_work_cancel(session, &session->close_unknow_session, HDCDRV_DELAY_UNKNOWN_SESSION_BIT);

    session_status = hdcdrv_get_session_status(session);
    if ((session_status == HDCDRV_SESSION_STATUS_IDLE) || (session_status == HDCDRV_SESSION_STATUS_CLOSING)) {
        ka_task_mutex_unlock(&session->mutex);
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x06, "Session not conn when set owner. (status=%d, "
            "local_close=%d, remote_close=%d, session=%d,service=%d).\n", hdcdrv_get_session_status(session),
            session->local_close_state, session->remote_close_state, cmd->session, session->service_type);
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    session->owner_pid = cmd->pid;
    session->task_start_time = hdcdrv_get_task_start_time();
    hdcdrv_bind_sessoin_ctx(ctx, session);
    session->pid_flag = 1;

    ka_task_mutex_unlock(&session->mutex);

    return HDCDRV_OK;
}

STATIC int hdcdrv_attr_get_session_status(struct hdcdrv_cmd_get_session_attr *cmd)
{
    int ret;

    if ((cmd->session >= HDCDRV_REAL_MAX_SESSION) || (cmd->session < 0)) {
        hdcdrv_err_limit("session_fd is illegal. (session_fd=%d)\n", cmd->session);
        return HDCDRV_PARA_ERR;
    }

    ret = (int)hdcdrv_session_alloc_idx_check(cmd->session, cmd->session_cur_alloc_idx);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return HDCDRV_SESSION_HAS_CLOSED;
    }

    /* session is connect */
    return HDCDRV_OK;
}

STATIC long hdcdrv_get_session_dfx(struct hdcdrv_cmd_get_session_attr *cmd)
{
    ssize_t offset = 0;
    int ret;
    u32 msg_len, out_len;
    struct hdcdrv_sysfs_ctrl_msg msg;
    struct hdcdrv_session *session = NULL;
    void* buf = NULL;

    session = &hdc_ctrl->sessions[cmd->session];
    buf = hdcdrv_kvmalloc(HDCDRV_LOG_MAX_LEN, KA_SUB_MODULE_TYPE_1);
    if (buf == NULL) {
        hdcdrv_err("Calling kzalloc failed.\n");
        return HDCDRV_ERR;
    }

    // get local session dfx
    ret = hdcdrv_sysfs_get_session_inner(buf, HDCDRV_LOG_MAX_LEN, (u32)cmd->session, &offset, HDC_DFX_PRINT_IN_LOG);
    if (ret != HDCDRV_OK) {
        goto dfx_buf_exit;
    }

    // get remote session dfx
    msg_len = sizeof(struct hdcdrv_sysfs_ctrl_msg);
    msg.head.type = HDCDRV_CTRL_MSG_TYPE_GET_DEV_SESSION_STAT;
    msg.head.error_code = HDCDRV_OK;
    msg.head.para = (u32)session->remote_session_fd;
    msg.head.msg_len = msg_len;
    msg.head.print_type = HDC_DFX_PRINT_IN_LOG;
    (void)hdcdrv_ctrl_msg_send((u32)session->dev_id, &msg, msg_len, msg_len, &out_len);

dfx_buf_exit:
    hdcdrv_kvfree(buf, KA_SUB_MODULE_TYPE_1);
    buf = NULL;

    return (long)ret;
}

STATIC long hdcdrv_get_session_attr(struct hdcdrv_cmd_get_session_attr *cmd)
{
    struct hdcdrv_session *session = NULL;
    long ret;

    /* get session status should before hdcdrv_session_valid_check */
    if (cmd->cmd_type == HDCDRV_SESSION_ATTR_STATUS) {
        ret = hdcdrv_attr_get_session_status(cmd);
        if (ret == HDCDRV_PARA_ERR) {
            return ret;
        }
        cmd->output = (int)ret;
        return HDCDRV_OK;
    }

    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    session = &hdc_ctrl->sessions[cmd->session];

    switch (cmd->cmd_type) {
        case HDCDRV_SESSION_ATTR_RUN_ENV:
            cmd->output = session->run_env;
            break;
        case HDCDRV_SESSION_ATTR_VFID:
            cmd->output = (int)session->remote_fid;
            break;
        case HDCDRV_SESSION_ATTR_LOCAL_CREATE_PID:
            cmd->output = hdcdrv_rebuild_raw_pid(session->create_pid);
            break;
        case HDCDRV_SESSION_ATTR_PEER_CREATE_PID:
            cmd->output = hdcdrv_rebuild_raw_pid(session->peer_create_pid);
            break;
        case HDCDRV_SESSION_ATTR_DFX:
            cmd->output = (int)hdcdrv_get_session_dfx(cmd);
            break;
        default:
            hdcdrv_err("Input pararmeter is error.\n");
            ret = HDCDRV_ERR;
            break;
    }

    return ret;
}

STATIC long hdcdrv_set_session_timeout(const struct hdcdrv_cmd_set_session_timeout *cmd)
{
    struct hdcdrv_session *session = NULL;
    long ret;
    u64 send_timeout;
    u64 recv_timeout;
    u64 fast_send_timeout;
    u64 fast_recv_timeout;

    ret = hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    session = &hdc_ctrl->sessions[cmd->session];

    send_timeout = ka_system_msecs_to_jiffies(cmd->timeout.send_timeout);
    recv_timeout = ka_system_msecs_to_jiffies(cmd->timeout.recv_timeout);
    fast_send_timeout = ka_system_msecs_to_jiffies(cmd->timeout.fast_send_timeout);
    fast_recv_timeout = ka_system_msecs_to_jiffies(cmd->timeout.fast_recv_timeout);

    if (cmd->timeout.send_timeout != 0) {
        session->timeout_jiffies.send_timeout = send_timeout;
    }

    if (cmd->timeout.fast_send_timeout != 0) {
        session->timeout_jiffies.fast_send_timeout = fast_send_timeout;
    }

    return HDCDRV_OK;
}

STATIC long hdcdrv_get_uid_state(struct hdcdrv_cmd_get_uid_stat *cmd)
{
    struct hdcdrv_session *session = NULL;
    int ret;

    ret = (int)hdcdrv_session_valid_check(cmd->session, cmd->dev_id, cmd->pid);
    if (ret != HDCDRV_OK) {
        hdcdrv_info("Parameters checked invalid.\n");
        return ret;
    }
    session = &hdc_ctrl->sessions[cmd->session];

    cmd->euid = (unsigned int)session->euid;
    cmd->uid = (unsigned int)session->uid;
    cmd->root_privilege = session->root_privilege;

    return HDCDRV_OK;
}

STATIC long hdcdrv_get_session_info(struct hdcdrv_cmd_get_session_info *cmd)
{
    struct hdcdrv_session *session = NULL;
    long ret;

    ret = hdcdrv_session_valid_check(cmd->session_fd, cmd->dev_id, cmd->pid);
    if (ret != 0) {
        return ret;
    }

    session = &hdc_ctrl->sessions[cmd->session_fd];

    if (session->owner != HDCDRV_SESSION_OWNER_PM) {
        hdcdrv_err("No right to get fid.\n");
        return HDCDRV_NO_PERMISSION;
    }

    cmd->fid = session->remote_fid;

    return HDCDRV_OK;
}

STATIC long hdcdrv_convert_id_from_vir_to_phy(u32 drv_cmd, union hdcdrv_cmd *cmd_data, u32 *vfid)
{
    long ret = HDCDRV_OK;

#ifndef HDC_ENV_DEVICE
    int *p_devid = NULL;
    int local_devid = 0;
    switch (drv_cmd) {
        case HDCDRV_CMD_GET_PEER_DEV_ID:
        case HDCDRV_CMD_CLIENT_DESTROY:
        case HDCDRV_CMD_SERVER_CREATE:
        case HDCDRV_CMD_SERVER_DESTROY:
        case HDCDRV_CMD_ACCEPT:
        case HDCDRV_CMD_CONNECT:
        case HDCDRV_CMD_ALLOC_MEM:
        case HDCDRV_CMD_DMA_MAP:
        case HDCDRV_CMD_DMA_REMAP:
            p_devid = &cmd_data->cmd_com.dev_id;
            break;
        case HDCDRV_CMD_EPOLL_ALLOC_FD:
            p_devid = &local_devid;
            break;
        case HDCDRV_CMD_EPOLL_CTL:
            p_devid = hdcdrv_epoll_get_dev_id_ptr(cmd_data);
            if (p_devid == NULL) {
                return HDCDRV_OK;
            }
            break;
        default:
            return ret;
    }
    ret = hdcdrv_container_vir_to_phs_devid((u32)(*p_devid), (u32 *)p_devid, vfid);
#endif

    return ret;
}

STATIC long hdcdrv_operation(struct hdcdrv_ctx *ctx, u32 drv_cmd, union hdcdrv_cmd *cmd_data,
    bool *copy_to_user_flag, u32 fid)
{
    long ret = HDCDRV_OK;

    switch (drv_cmd) {
        case HDCDRV_CMD_SERVER_WAKEUP_WAIT:
            ret = hdcdrv_server_wakeup_wait(&cmd_data->server_wakeup_wait, fid);
            break;
        case HDCDRV_CMD_CLIENT_WAKEUP_WAIT:
            ret = hdcdrv_client_wakeup_wait(&cmd_data->client_wakeup_wait, fid);
            break;
        case HDCDRV_CMD_GET_PEER_DEV_ID:
            ret = hdcdrv_cmd_get_peer_dev_id(&cmd_data->get_peer_dev_id);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_CONFIG:
            ret = hdcdrv_config(&cmd_data->config);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_SET_SERVICE_LEVEL:
            ret = hdcdrv_set_service_level(&cmd_data->set_level);
            break;
        case HDCDRV_CMD_CLIENT_DESTROY:
            ret = hdcdrv_client_destroy(&cmd_data->client_destroy, fid);
            break;
        case HDCDRV_CMD_SERVER_CREATE:
            ret = hdcdrv_server_create(ctx, &cmd_data->server_create, fid);
            break;
        case HDCDRV_CMD_SERVER_DESTROY:
            ret = hdcdrv_server_destroy(ctx, &cmd_data->server_destroy, fid);
            break;
        case HDCDRV_CMD_ACCEPT:
            ret = hdcdrv_accept(&cmd_data->accept, fid);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_CONNECT:
            ret = hdcdrv_connect(&cmd_data->conncet, fid);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_CLOSE:
            cmd_data->close.task_start_time = hdcdrv_get_task_start_time();
            ret = hdcdrv_close(&cmd_data->close, HDCDRV_CLOSE_TYPE_USER);
            break;
        case HDCDRV_CMD_SEND:
            ret = hdcdrv_send(&cmd_data->send, HDCDRV_MODE_USER);
            break;
        case HDCDRV_CMD_RECV_PEEK:
            ret = hdcdrv_recv_peek(&cmd_data->recv_peek);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_RECV:
            ret = hdcdrv_recv(&cmd_data->recv, HDCDRV_MODE_USER);
            *copy_to_user_flag = true;
            break;
        default:
            hdcdrv_err_limit("drv_cmd is illegal. (cmd=%u)\n", drv_cmd);
            return HDCDRV_PARA_ERR;
    }

    return ret;
}

STATIC long hdcdrv_cfg_operation(struct hdcdrv_ctx *ctx, u32 drv_cmd, union hdcdrv_cmd *cmd_data,
    bool *copy_to_user_flag, u32 fid)
{
    long ret = HDCDRV_OK;

    switch (drv_cmd) {
        case HDCDRV_CMD_SET_SESSION_OWNER:
            ret = hdcdrv_set_session_owner(ctx, &cmd_data->set_owner, fid);
            break;
        case HDCDRV_CMD_GET_SESSION_ATTR:
            ret = hdcdrv_get_session_attr(&cmd_data->get_session_attr);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_SET_SESSION_TIMEOUT:
            ret = hdcdrv_set_session_timeout(&cmd_data->set_session_timeout);
            break;
        case HDCDRV_CMD_GET_SESSION_UID:
            ret = hdcdrv_get_uid_state(&cmd_data->get_uid_stat);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_GET_PAGE_SIZE:
            ret = hdcdrv_get_page_size(&cmd_data->get_page_size);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_GET_SESSION_INFO:
            ret = hdcdrv_get_session_info(&cmd_data->get_session_info);
            *copy_to_user_flag = true;
            break;
        default:
            hdcdrv_err_limit("drv_cmd is illegal. (cmd=%u)\n", drv_cmd);
            return HDCDRV_PARA_ERR;
    }

    return ret;
}

STATIC long hdcdrv_fast_operation(struct hdcdrv_ctx *ctx, u32 drv_cmd,
    union hdcdrv_cmd *cmd_data, bool *copy_to_user_flag)
{
    long ret = HDCDRV_OK;

    switch (drv_cmd) {
        case HDCDRV_CMD_FAST_SEND:
            ret = hdcdrv_fast_send(&cmd_data->fast_send);
            break;
        case HDCDRV_CMD_FAST_RECV:
            ret = hdcdrv_fast_recv(&cmd_data->fast_recv);
            *copy_to_user_flag = true;
            break;
#ifndef CFG_FEATURE_HDC_REG_MEM
        case HDCDRV_CMD_ALLOC_MEM:
            ret = hdcdrv_fast_alloc_mem(ctx, &cmd_data->alloc_mem);
            break;
        case HDCDRV_CMD_FREE_MEM:
            ret = hdcdrv_fast_free_mem((void *)ctx, &cmd_data->free_mem);
            *copy_to_user_flag = true;
            break;
        case HDCDRV_CMD_DMA_MAP:
            ret = hdcdrv_fast_dma_map(&cmd_data->dma_map);
            break;
        case HDCDRV_CMD_DMA_UNMAP:
            ret = hdcdrv_fast_dma_unmap(&cmd_data->dma_unmap);
            break;
        case HDCDRV_CMD_DMA_REMAP:
            ret = hdcdrv_fast_dma_remap(&cmd_data->dma_remap);
            break;
#else
        case HDCDRV_CMD_REGISTER_MEM:
            ret = hdcdrv_fast_register_mem(ctx, &cmd_data->register_mem);
            break;
        case HDCDRV_CMD_UNREGISTER_MEM:
            ret = hdcdrv_fast_unregister_mem((void *)ctx, &cmd_data->unregister_mem);
            break;
        case HDCDRV_CMD_WAIT_MEM:
            ret = hdcdrv_fast_wait_mem(&cmd_data->wait_mem);
            *copy_to_user_flag = true;
            break;
#endif
        default:
            hdcdrv_err_limit("drv_cmd is illegal. (cmd=%u)\n", drv_cmd);
            ret = HDCDRV_PARA_ERR;
            break;
    }

    return ret;
}

long hdcdrv_ioctl_com(struct hdcdrv_ctx *ctx, unsigned int cmd, union hdcdrv_cmd *cmd_data,
    bool *copy_flag, u32 fid)
{
    u32 drv_cmd = _KA_IOC_NR(cmd);
    long ret = HDCDRV_OK;

    if (_KA_IOC_TYPE(cmd) != HDCDRV_CMD_MAGIC) {
        hdcdrv_err("Command type is error. (cmd=%x)\n", cmd);
        return HDCDRV_PARA_ERR;
    }

    hdcdrv_peer_fault_proc(drv_cmd, &ret);
    if (ret != HDCDRV_OK) {
        return ret;
    }
    if (drv_cmd < HDCDRV_CMD_SET_SESSION_OWNER) {
        ret = hdcdrv_operation(ctx, drv_cmd, cmd_data, copy_flag, fid);
    } else if (drv_cmd < HDCDRV_CMD_ALLOC_MEM) {
        ret = hdcdrv_cfg_operation(ctx, drv_cmd, cmd_data, copy_flag, fid);
    } else if (drv_cmd < HDCDRV_CMD_EPOLL_ALLOC_FD) {
        ret = hdcdrv_fast_operation(ctx, drv_cmd, cmd_data, copy_flag);
    } else {
        ret = hdcdrv_epoll_operation(ctx, drv_cmd, cmd_data, copy_flag, fid);
    }

    return ret;
}

STATIC long hdcdrv_ioctl(ka_file_t *file, unsigned int cmd, unsigned long arg)
{
    void __ka_user *argp = (void __ka_user *)((uintptr_t)arg);
    struct hdcdrv_ctx *ctx = NULL;
    u32 drv_cmd = _KA_IOC_NR(cmd);
    bool copy_flag = false;
    union hdcdrv_cmd cmd_data = {{0}};
    u32 vfid = HDCDRV_DEFAULT_PM_FID;
    long ret;
    int running_status;

    if ((file == NULL) || (argp == NULL)) {
        hdcdrv_err("Input pararmeter is error.\n");
        return HDCDRV_PARA_ERR;
    }
    running_status = hdcdrv_get_running_status();
    if ((running_status == HDCDRV_RUNNING_SUSPEND) || (running_status == HDCDRV_RUNNING_SUSPEND_ENTERING)) {
        return HDCDRV_DEVICE_NOT_READY;
    }

    if (ka_base_copy_from_user(&cmd_data, argp, sizeof(cmd_data)) != 0) {
        hdcdrv_err("Calling ka_base_copy_from_user failed. (cmd=%u)\n", drv_cmd);
        return HDCDRV_COPY_FROM_USER_FAIL;
    }

    if (hdcdrv_convert_id_from_vir_to_phy(drv_cmd, &cmd_data, &vfid) != HDCDRV_OK) {
        hdcdrv_err("Convert virtual id to physical id failed. (drv_cmd=%u)\n", drv_cmd);
        return HDCDRV_CONV_FAILED;
    }

    ctx = (struct hdcdrv_ctx *)ka_fs_get_file_private_data(file);
    cmd_data.cmd_com.pid = hdcdrv_rebuild_pid((u32)cmd_data.cmd_com.dev_id, vfid, hdcdrv_get_pid());

    ret = hdcdrv_ioctl_com(ctx, cmd, &cmd_data, &copy_flag, vfid);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    if ((ret == HDCDRV_OK) && copy_flag) {
        if (ka_base_copy_to_user(argp, &cmd_data, sizeof(cmd_data)) != 0) {
            hdcdrv_err("ka_base_copy_to_user failed. (drv_cmd=%d)\n", drv_cmd);
            return HDCDRV_COPY_TO_USER_FAIL;
        }
    }

    return HDCDRV_OK;
}

STATIC ka_vm_fault_t hdcdrv_svm_vmf_fault_host(ka_vm_fault_struct_t *vmf)
{
    /* Page fault interrupt mapping pa is not supported for hdc mmap */
    hdcdrv_warn("hdc mmap not supported page fault");
    return HDCDRV_FAULT_ERROR;
}

STATIC int hdcdrv_svm_vm_fault_host(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf)
{
    /* Page fault interrupt mapping pa is not supported for hdc mmap */
    hdcdrv_warn("hdc mmap not supported page fault");
    return (int)HDCDRV_FAULT_ERROR;
}

STATIC int hdcdrv_vm_mremap(ka_vm_area_struct_t *vma)
{
    hdcdrv_err("mremap not supported\n");
    return -EINVAL;
}

STATIC void hdcdrv_svm_mmap_init_vm_struct(ka_vm_operations_struct_t *ops_managed)
{
    ka_mm_set_vm_fault_host(ops_managed, hdcdrv_svm_vmf_fault_host, hdcdrv_svm_vm_fault_host);
    ka_mm_set_vm_mremap(ops_managed, hdcdrv_vm_mremap);
}

/* Avoid the user process calling mlockall (MCL_FUTURE) to establish the page table in advance,
   which will cause our subsequent page table creation to fail */
STATIC int hdcdrv_mmap(ka_file_t *filep, ka_vm_area_struct_t *vma)
{
    int ret;

    ret = hdcdrv_mmap_param_check(filep, vma);
    if (ret != 0) {
        hdcdrv_err("mmap param check failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    ka_mm_set_vm_private_data(vma, filep->private_data);
    hdcdrv_svm_mmap_init_vm_struct(&hdcdrv_vm_ops_managed);
    ka_mm_set_vm_ops(vma, &hdcdrv_vm_ops_managed);
    ka_mm_set_vm_flags(vma, KA_VM_DONTEXPAND| KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_IO | KA_VM_PFNMAP);

    return HDCDRV_OK;
}

void hdcdrv_set_debug_mode(int flag)
{
    ka_task_mutex_lock(&hdc_ctrl->mutex);
    if (flag != 0) {
        hdc_ctrl->debug_state.valid = HDCDRV_VALID;
        hdcdrv_info("Step into adjustment mode.\n");
    } else {
        hdc_ctrl->debug_state.valid = HDCDRV_INVALID;
        hdcdrv_info("Step into adjustment mode.\n");
    }
    hdc_ctrl->debug_state.pid = (long long)hdcdrv_get_pid();
    ka_task_mutex_unlock(&hdc_ctrl->mutex);

    return;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_set_debug_mode);

STATIC int hdcdrv_open(ka_inode_t *node, ka_file_t *file)
{
    struct hdcdrv_ctx *ctx = hdcdrv_alloc_ctx();
    if (ctx == NULL) {
        hdcdrv_err("Calling kzalloc failed.\n");
        return HDCDRV_MEM_ALLOC_FAIL;
    }

    ka_fs_set_file_private_data(file, ctx);

    return HDCDRV_OK;
}

STATIC void hdcdrv_release_close_session(struct hdcdrv_ctx *ctx)
{
    int ret = 0;
    struct hdcdrv_cmd_close close_cmd;
    struct hdcdrv_session *session = ctx->session;

    /* session free */
    if (ctx->session != NULL) {
        hdcdrv_info("Release session. (dev=%d; task_pid=%llu; local session_id=%d; remote_id=%d; session_status=%d)\n",
            ctx->dev_id, ctx->pid, ctx->session_fd, session->remote_session_fd,
            hdcdrv_get_session_status(session));

        close_cmd.session = ctx->session_fd;
        close_cmd.pid = ctx->pid;
        close_cmd.unique_val = session->unique_val;
        close_cmd.task_start_time = ctx->task_start_time;
        close_cmd.session_cur_alloc_idx = session->session_cur_alloc_idx;
        ret = (int)hdcdrv_close(&close_cmd, HDCDRV_CLOSE_TYPE_RELEASE);
        if (ret != HDCDRV_OK) {
            hdcdrv_warn("Close session not success. (dev=%d; task_pid=%llu; local_id=%d; remote_id=%d; status=%d)\n",
                ctx->dev_id, ctx->pid, ctx->session_fd, session->remote_session_fd,
                hdcdrv_get_session_status(session));
        }
        ctx->session = NULL;
    }
}

STATIC void hdcdrv_release_destroy_server(struct hdcdrv_ctx *ctx)
{
    int ret = 0;
    struct hdcdrv_cmd_server_destroy destroy_cmd;

    /* service free */
    if (ctx->service != NULL) {
        if (hdc_ctrl->service_attr[ctx->service_type].log_limit != HDCDRV_SERVICE_LOG_LIMIT) {
            hdcdrv_info("Release server. (task_pid=%llu; dev_id=%d; service_type=\"%s\")\n",
                ctx->service->listen_pid, ctx->dev_id, hdcdrv_sevice_str(ctx->service_type));
        }
        destroy_cmd.dev_id = ctx->dev_id;
        destroy_cmd.service_type = ctx->service_type;
        destroy_cmd.pid = ctx->pid;
        ret = (int)hdcdrv_server_destroy(ctx, &destroy_cmd, ctx->service->fid);
        if (ret != HDCDRV_OK) {
            hdcdrv_warn("Release server. (task_pid=%llu; dev_id=%d; service_type=\"%s\")\n",
                ctx->pid, ctx->dev_id, hdcdrv_sevice_str(ctx->service_type));
        }

        ctx->service = NULL;
    }
}

STATIC int hdcdrv_release(ka_inode_t *node, ka_file_t *file)
{
    struct hdcdrv_ctx *ctx = ka_fs_get_file_private_data(file);

    if (ctx == NULL) {
        hdcdrv_err("Input pararmeter is error. (task pid=%llu)", hdcdrv_get_pid());
        return HDCDRV_PARA_ERR;
    }

    if (hdcdrv_get_peer_status() == DEVDRV_PEER_STATUS_LINKDOWN) {
        hdcdrv_set_quice_release_flag(&ctx->ctx_fmem.quick_flag);
    }
    hdcdrv_release_by_ctx(ctx);
    if (hdcdrv_release_is_quick(ctx->ctx_fmem.quick_flag)) {
        (void)ka_task_queue_work(async_release_workqueue, &ctx->async_release_work);
    } else {
        hdcdrv_free_ctx(ctx);
    }
    ka_fs_set_file_private_data(file, NULL);

    if (hdc_ctrl->debug_state.pid == hdcdrv_get_pid()) {
        hdcdrv_set_debug_mode(HDCDRV_INVALID);
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_release_mem_async_work(ka_work_struct_t *p_work)
{
    struct hdcdrv_ctx *ctx = ka_container_of(p_work, struct hdcdrv_ctx, async_release_work);
    hdcdrv_release_free_mem(&ctx->async_ctx);
    hdcdrv_free_ctx(ctx);
}

struct hdcdrv_ctx *hdcdrv_alloc_ctx(void)
{
    struct hdcdrv_ctx *ctx = hdcdrv_kvmalloc(sizeof(struct hdcdrv_ctx), KA_SUB_MODULE_TYPE_1);
    if (ctx != NULL) {
        ctx->service = NULL;
        ctx->session = NULL;
        ctx->epfd = NULL;

        ka_task_spin_lock_init(&ctx->ctx_fmem.mem_lock);
        KA_INIT_LIST_HEAD(&ctx->ctx_fmem.mlist.list);
        ka_task_spin_lock_init(&ctx->abnormal_ctx_fmem.mem_lock);
        KA_INIT_LIST_HEAD(&ctx->abnormal_ctx_fmem.mlist.list);
        ka_task_spin_lock_init(&ctx->async_ctx.mem_lock);
        KA_INIT_LIST_HEAD(&ctx->async_ctx.mlist.list);

        KA_TASK_INIT_WORK(&ctx->async_release_work, hdcdrv_release_mem_async_work);
    }

    return ctx;
}

void hdcdrv_free_ctx(const struct hdcdrv_ctx *ctx)
{
    hdcdrv_kvfree(ctx, KA_SUB_MODULE_TYPE_1);
    ctx = NULL;
}

void hdcdrv_release_by_ctx(struct hdcdrv_ctx *ctx)
{
    hdcdrv_release_close_session(ctx);
    hdcdrv_release_destroy_server(ctx);
    hdcdrv_release_free_mem(&ctx->ctx_fmem);
    hdcdrv_release_unmap_failed_fast_mem(&ctx->abnormal_ctx_fmem);
    hdcdrv_epoll_recycle_fd(ctx);
}

/* The following interface is provided for the kernel to call hdc */
bool hdcdrv_is_service_init(unsigned int service_type)
{
    if (service_type >= HDCDRV_SUPPORT_MAX_SERVICE) {
        return false;
    }

    if (hdc_ctrl == NULL) {
        return false;
    }

    return true;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_is_service_init);

/* for ide type, set level to 1 */
long hdcdrv_kernel_set_service_level(int service_type, int level)
{
    struct hdcdrv_cmd_set_service_level cmd;

    cmd.service_type = service_type;
    cmd.level = level;

    return hdcdrv_set_service_level(&cmd);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_set_service_level);

void hdcdrv_set_segment(int segment)
{
    hdc_ctrl->segment = segment;
    hdcdrv_info("Set segment success. (hdc_segment=%d)\n", hdc_ctrl->segment);
}

int hdcdrv_get_segment(void)
{
    return hdcdrv_mem_block_capacity();
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_get_segment);

void hdcdrv_set_peer_dev_id(int dev_id, int peer_dev_id)
{
    hdc_ctrl->devices[dev_id].peer_dev_id = peer_dev_id;
}

int hdcdrv_get_peer_dev_id(int dev_id)
{
    return hdc_ctrl->devices[dev_id].peer_dev_id;
}

long hdcdrv_kernel_server_create(int dev_id, int service_type)
{
    struct hdcdrv_cmd_server_create cmd;

    cmd.dev_id = dev_id;
    cmd.service_type = service_type;
    cmd.pid = hdcdrv_get_pid();

    return hdcdrv_server_create(HDCDRV_KERNEL_WITHOUT_CTX, &cmd, HDCDRV_DEFAULT_PM_FID);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_server_create);

long hdcdrv_kernel_server_destroy(int dev_id, int service_type)
{
    struct hdcdrv_cmd_server_destroy cmd;

    cmd.dev_id = dev_id;
    cmd.service_type = service_type;
    cmd.pid = hdcdrv_get_pid();

    return hdcdrv_server_destroy(HDCDRV_KERNEL_WITHOUT_CTX, &cmd, HDCDRV_DEFAULT_PM_FID);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_server_destroy);

long hdcdrv_kernel_accept(int dev_id, int service_type, int *session, const char *session_id)
{
    struct hdcdrv_cmd_set_session_owner set_pid_cmd;
    struct hdcdrv_cmd_accept cmd;
    long ret;
    long res;

    (void)session_id;
    if (session == NULL) {
        return HDCDRV_PARA_ERR;
    }

    cmd.dev_id = dev_id;
    cmd.service_type = service_type;
    cmd.pid = hdcdrv_get_pid();

    ret = hdcdrv_accept(&cmd, HDCDRV_DEFAULT_PM_FID);
    if (ret == HDCDRV_OK) {
        *session = cmd.session;
        set_pid_cmd.session = cmd.session;
        set_pid_cmd.pid = hdcdrv_get_pid();
        set_pid_cmd.dev_id = dev_id;

        res = hdcdrv_set_session_owner(HDCDRV_KERNEL_WITHOUT_CTX, &set_pid_cmd, HDCDRV_DEFAULT_PM_FID);
        if (res != HDCDRV_OK) {
            hdcdrv_err("Set owner failed. (dev=%d; session=%d)\n", dev_id, cmd.session);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_accept);

long hdcdrv_kernel_connect(int dev_id, int service_type, int *session, const char *session_id)
{
    struct hdcdrv_cmd_set_session_owner set_pid_cmd;
    struct hdcdrv_cmd_connect cmd;
    long ret;
    long res;

    (void)session_id;
    if (session == NULL) {
        return HDCDRV_PARA_ERR;
    }

    cmd.dev_id = dev_id;
    cmd.service_type = service_type;
    cmd.peer_pid = HDCDRV_INVALID_PEER_PID;
    cmd.pid = hdcdrv_get_pid();

    ret = hdcdrv_connect(&cmd, HDCDRV_DEFAULT_PM_FID);
    if (ret == HDCDRV_OK) {
        *session = cmd.session;
        set_pid_cmd.session = cmd.session;
        set_pid_cmd.pid = hdcdrv_get_pid();
        set_pid_cmd.dev_id = dev_id;

        res = hdcdrv_set_session_owner(HDCDRV_KERNEL_WITHOUT_CTX, &set_pid_cmd, HDCDRV_DEFAULT_PM_FID);
        if (res != HDCDRV_OK) {
            hdcdrv_err("Set owner failed. (dev=%d; session=%d)\n", dev_id, cmd.session);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_connect);

long hdcdrv_kernel_close(int session, const char *session_id)
{
    struct hdcdrv_cmd_close cmd;

    (void)session_id;
    if ((session >= HDCDRV_REAL_MAX_SESSION) || (session < 0)) {
        hdcdrv_err("Input pararmeter is error. (session_fd=%d)\n", session);
        return HDCDRV_PARA_ERR;
    }

    cmd.session = session;
    cmd.pid = hdcdrv_get_pid();
    cmd.unique_val = hdc_ctrl->sessions[session].unique_val;
    cmd.task_start_time = hdc_ctrl->sessions[session].task_start_time;
    cmd.session_cur_alloc_idx = hdc_ctrl->sessions[session].session_cur_alloc_idx;

    return hdcdrv_close(&cmd, HDCDRV_CLOSE_TYPE_KERNEL);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_close);

long hdcdrv_kernel_send_timeout(int session, const char *session_id, void *buf, int len, int timeout)
{
    struct hdcdrv_session *ksession = NULL;
    struct hdcdrv_cmd_send cmd;

    (void)session_id;
    if ((buf == NULL)) {
        return HDCDRV_PARA_ERR;
    }

    if ((session < 0) || (session >= HDCDRV_REAL_MAX_SESSION)) {
        hdcdrv_err_limit("Input pararmeter is error. (session_fd=%d)\n", session);
        return HDCDRV_PARA_ERR;
    }

    ksession = &hdc_ctrl->sessions[session];

    cmd.session = session;
    cmd.dev_id = ksession->dev_id;
    cmd.src_buf = buf;
    cmd.len = len;
    cmd.wait_flag = hdcdrv_get_wait_flag(timeout);
    cmd.timeout = (unsigned int)timeout;
    cmd.pool_buf = NULL;
    cmd.pid = hdcdrv_get_pid();

    return hdcdrv_send(&cmd, HDCDRV_MODE_KERNEL);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_send_timeout);

long hdcdrv_kernel_send(int session, const char *session_id, void *buf, int len)
{
    /* send block */
    return hdcdrv_kernel_send_timeout(session, session_id, buf, len, -1);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_send);

long hdcdrv_kernel_recv_peek_timeout(int session, const char *session_id, int *len, int timeout)
{
    struct hdcdrv_session *ksession = NULL;
    struct hdcdrv_cmd_recv_peek cmd;
    long ret;

    (void)session_id;
    if (len == NULL) {
        return HDCDRV_PARA_ERR;
    }

    if ((session < 0) || (session >= HDCDRV_REAL_MAX_SESSION)) {
        hdcdrv_err_limit("Input pararmeter is error. (session_fd=%d)\n", session);
        return HDCDRV_PARA_ERR;
    }

    ksession = &hdc_ctrl->sessions[session];

    cmd.session = session;
    cmd.dev_id = ksession->dev_id;
    cmd.wait_flag = hdcdrv_get_wait_flag(timeout);
    cmd.timeout = (unsigned int)timeout;
    cmd.pid = hdcdrv_get_pid();
    cmd.group_flag = 0;
    cmd.session_cur_alloc_idx = ksession->session_cur_alloc_idx;
    ret = hdcdrv_recv_peek(&cmd);
    if (ret == HDCDRV_OK) {
        *len = cmd.len;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_recv_peek_timeout);

long hdcdrv_kernel_recv_peek(int session, const char *session_id, int *len)
{
    /* recv peek block */
    return hdcdrv_kernel_recv_peek_timeout(session, session_id, len, -1);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_recv_peek);

long hdcdrv_kernel_recv(int session, const char *session_id, void *buf, int len, int *out_len)
{
    struct hdcdrv_session *ksession = NULL;
    struct hdcdrv_cmd_recv cmd;
    long ret;

    (void)session_id;
    if ((buf == NULL) || (out_len == NULL)) {
        return HDCDRV_PARA_ERR;
    }

    if ((session < 0) || (session >= HDCDRV_REAL_MAX_SESSION)) {
        hdcdrv_err_limit("Input pararmeter is error. (session_fd=%d)\n", session);
        return HDCDRV_PARA_ERR;
    }

    ksession = &hdc_ctrl->sessions[session];

    cmd.session = session;
    cmd.dev_id = ksession->dev_id;
    cmd.dst_buf = buf;
    cmd.len = len;
    cmd.pid = hdcdrv_get_pid();
    cmd.buf_count = 1;          // kernel_recv use normal recv, the buf_cout is 1

    ret = hdcdrv_recv(&cmd, HDCDRV_MODE_KERNEL);
    if (ret == HDCDRV_OK) {
        *out_len = cmd.out_len;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_recv);

STATIC long hdcdrv_kernel_get_session_attr(int session, int attr, int *value)
{
    struct hdcdrv_cmd_get_session_attr cmd;
    struct hdcdrv_session *ksession = NULL;
    long ret;

    if (value == NULL) {
        hdcdrv_err("Input pararmeter is error.\n");
        return HDCDRV_PARA_ERR;
    }

    if ((session < 0) || (session >= HDCDRV_REAL_MAX_SESSION)) {
        hdcdrv_err("Input pararmeter is error. (session_fd=%d)\n", session);
        return HDCDRV_PARA_ERR;
    }

    ksession = &hdc_ctrl->sessions[session];

    cmd.cmd_type = attr;
    cmd.session = session;
    cmd.dev_id = ksession->dev_id;
    cmd.pid = hdcdrv_get_pid();
    cmd.session_cur_alloc_idx = ksession->session_cur_alloc_idx;
    ret = hdcdrv_get_session_attr(&cmd);
    if (ret == HDCDRV_OK) {
        *value = cmd.output;
    }

    return ret;
}

long hdcdrv_kernel_get_session_run_env(int session, const char *session_id, int *run_env)
{
    (void)session_id;
    return hdcdrv_kernel_get_session_attr(session, HDCDRV_SESSION_ATTR_RUN_ENV, run_env);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_get_session_run_env);

long hdcdrv_kernel_get_session_vfid(int session, int *value)
{
    return hdcdrv_kernel_get_session_attr(session, HDCDRV_SESSION_ATTR_VFID, value);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_get_session_vfid);

long hdcdrv_kernel_get_session_local_create_pid(int session, int *value)
{
    return hdcdrv_kernel_get_session_attr(session, HDCDRV_SESSION_ATTR_LOCAL_CREATE_PID, value);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_get_session_local_create_pid);

long hdcdrv_kernel_get_session_peer_create_pid(int session, int *value)
{
    return hdcdrv_kernel_get_session_attr(session, HDCDRV_SESSION_ATTR_PEER_CREATE_PID, value);
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_kernel_get_session_peer_create_pid);

void hdcdrv_session_notify_register(int service_type, struct hdcdrv_session_notify *notify)
{
    if ((notify == NULL) || (service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (service_type < 0)) {
        hdcdrv_err("Input pararmeter is error. (service_type=\"%s\")\n", hdcdrv_sevice_str(service_type));
        return ;
    }

    g_session_notify[service_type].connect_notify = notify->connect_notify;
    g_session_notify[service_type].close_notify = notify->close_notify;
    g_session_notify[service_type].data_in_notify = notify->data_in_notify;

    return;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_session_notify_register);

void hdcdrv_session_notify_unregister(int service_type)
{
    if ((service_type >= HDCDRV_SUPPORT_MAX_SERVICE) || (service_type < 0)) {
        hdcdrv_err("Input pararmeter is error. (service_type=\"%s\")\n", hdcdrv_sevice_str(service_type));
        return ;
    }

    g_session_notify[service_type].connect_notify = NULL;
    g_session_notify[service_type].close_notify = NULL;
    g_session_notify[service_type].data_in_notify = NULL;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_session_notify_unregister);

/* value 0 means rx list is not full, value 1 means rx list is full */
int hdcdrv_get_session_rx_list_status(int session_fd, int *value)
{
    if (value == NULL) {
        hdcdrv_err("Input pararmeter is error.\n");
        return HDCDRV_PARA_ERR;
    }

    if (session_fd < 0 || session_fd >= HDCDRV_REAL_MAX_SESSION) {
        hdcdrv_err("session_fd is illegal. (session_fd=%d)\n", session_fd);
        return HDCDRV_PARA_ERR;
    }

    *value = hdcdrv_session_rx_list_is_full(session_fd, hdc_ctrl->sessions[session_fd].msg_chan->type);

    return HDCDRV_OK;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_get_session_rx_list_status);

void hdcdrv_register_symbol_from_tsdrv(struct hdcdrv_register_symbol *module_symbol)
{
    if (module_symbol == NULL) {
        hdcdrv_err("invalid arg.\n");
        return;
    }

    ka_task_down_write(&g_symbol_lock);
    if (g_hdcdrv_register_symbol.module_ptr != NULL) {
        hdcdrv_err("The module has registered symbols.\n");
        ka_task_up_write(&g_symbol_lock);
        return;
    }

    g_hdcdrv_register_symbol.module_ptr = module_symbol->module_ptr;
    g_hdcdrv_register_symbol.wake_up_context_status = module_symbol->wake_up_context_status;
    ka_task_up_write(&g_symbol_lock);
    return;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_register_symbol_from_tsdrv);

void hdcdrv_unregister_symbol_from_tsdrv(void)
{
    ka_task_down_write(&g_symbol_lock);
    g_hdcdrv_register_symbol.module_ptr = NULL;
    g_hdcdrv_register_symbol.wake_up_context_status = NULL;
    ka_task_up_write(&g_symbol_lock);
    return;
}
KA_EXPORT_SYMBOL_GPL(hdcdrv_unregister_symbol_from_tsdrv);

STATIC void hdcdrv_guard_work(ka_work_struct_t *p_work)
{
    int i, j;
    struct hdcdrv_msg_chan *msg_chan = NULL;
    ka_delayed_work_t *delayed_work = ka_container_of(p_work, ka_delayed_work_t, work);

    for (i = 0; i < hdcdrv_get_max_support_dev(); i++) {
        if (hdc_ctrl->devices[i].valid != HDCDRV_VALID) {
            continue;
        }

        for (j = 0; j < HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN; j++) {
            msg_chan = hdc_ctrl->devices[i].msg_chan[j];
            if (msg_chan == NULL) {
                continue;
            }

            hdcdrv_tx_finish_task_check(msg_chan);
            hdcdrv_rx_msg_task_check(msg_chan);
            hdcdrv_rx_msg_notify_task_check(msg_chan);
        }
    }

    (void)ka_task_schedule_delayed_work(delayed_work, 1 * KA_HZ);
}

STATIC void hdcdrv_close_unknow_session_work(ka_work_struct_t *p_work)
{
    int ret = 0;
    struct hdcdrv_cmd_close close_cmd;
    struct hdcdrv_session *session = ka_container_of(p_work, struct hdcdrv_session, close_unknow_session.work);

    hdcdrv_delay_work_flag_clear(session, HDCDRV_DELAY_UNKNOWN_SESSION_BIT);
    if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_CONN) {
        hdcdrv_info_limit("Close unknow session. (dev=%d; service=\"%s\"; l_fd=%d; r_fd=%d; pid=%llu; peer_pid=%llu; owner_flag=%d)\n",
            session->dev_id, hdcdrv_sevice_str(session->service_type), session->local_session_fd, session->remote_session_fd,
            session->owner_pid, session->peer_create_pid, session->pid_flag);

        close_cmd.session = session->local_session_fd;
        close_cmd.pid = session->owner_pid;
        close_cmd.unique_val = session->unique_val;
        close_cmd.task_start_time = session->task_start_time;
        close_cmd.session_cur_alloc_idx = session->session_cur_alloc_idx;

        ret = (int)hdcdrv_close(&close_cmd, (int)HDCDRV_CLOSE_TYPE_NOT_SET_OWNER);
        if (ret != HDCDRV_OK) {
            hdcdrv_warn("Calling hdcdrv_close not success. (dev_id=%d; ret=%d; pid=%llu; peer_create_pid=%llu)\n",
                session->dev_id, ret, session->owner_pid, session->peer_create_pid);
        }
    }
}

#ifdef CFG_FEATURE_VFIO
STATIC void hdcdrv_session_work_list_free(struct hdcdrv_session_work *s_work)
{
    struct hdcdrv_session_work_node *entry[HDCDRV_LIST_CACHE] = {NULL};
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    int count;
    int i;

    while (!ka_list_empty_careful(&s_work->root_list)) {
        count = 0;
        ka_task_spin_lock_bh(&s_work->lock);
        ka_list_for_each_safe(pos, n, &s_work->root_list) {
            entry[count] = ka_list_entry(pos, struct hdcdrv_session_work_node, list);
            ka_list_del(&entry[count]->list);
            count++;
            if (count >= HDCDRV_LIST_CACHE) {
                break;
            }
        }
        ka_task_spin_unlock_bh(&s_work->lock);

        for (i = 0; i < count; i++) {
            vhdch_free_mem_vm(entry[i]->dev_id, entry[i]->fid, entry[i]->buf);
            hdcdrv_dma_unmap_guest_page(entry[i]->dev_id, entry[i]->fid, entry[i]->dma_sgt);

            hdcdrv_ka_kvfree(entry[i], KA_SUB_MODULE_TYPE_1);
            entry[i] = NULL;
        }
    }
}

STATIC void hdcdrv_session_work_task(ka_work_struct_t *p_work)
{
    struct hdcdrv_session *session = ka_container_of(p_work, struct hdcdrv_session, session_work.swork);

    hdcdrv_session_work_list_free(&session->session_work);
}

STATIC void hdcdrv_session_work_init(struct hdcdrv_session_work *s_work)
{
    KA_TASK_INIT_WORK(&s_work->swork, hdcdrv_session_work_task);
    ka_task_spin_lock_init(&s_work->lock);
    KA_INIT_LIST_HEAD(&s_work->root_list);
}

void hdcdrv_session_work_free(struct hdcdrv_session_work *s_work)
{
    ka_task_cancel_work_sync(&s_work->swork);
    hdcdrv_session_work_list_free(s_work);
}
#endif

int hdcdrv_add_ctrl_msg_chan_to_dev(u32 dev_id, void *chan)
{
    struct hdcdrv_dev *hdc_dev = NULL;

    if (dev_id >= (u32)hdcdrv_get_max_support_dev()) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%u)\n", dev_id);
        return HDCDRV_PARA_ERR;
    }

    hdc_dev = &hdc_ctrl->devices[dev_id];
    hdc_dev->ctrl_msg_chan = chan;
    return HDCDRV_OK;
}

int hdcdrv_add_msg_chan_to_dev(u32 dev_id, void *chan)
{
    struct hdcdrv_dev *hdc_dev = NULL;
    struct hdcdrv_msg_chan *msg_chan = NULL;
    int i;

    if (dev_id >= (u32)hdcdrv_get_max_support_dev()) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%u)\n", dev_id);
        return HDCDRV_PARA_ERR;
    }

    hdc_dev = &hdc_ctrl->devices[dev_id];

    ka_task_mutex_lock(&hdc_dev->mutex);

    if ((hdc_dev->msg_chan_cnt >= (int)HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN) || (hdc_dev->msg_chan_cnt < 0)) {
        ka_task_mutex_unlock(&hdc_dev->mutex);
        hdcdrv_err("Parameter msg_chan_cnt out of range. (msg_chan_cnt=%d)\n", hdc_dev->msg_chan_cnt);
        return HDCDRV_PARA_ERR;
    }

    msg_chan = hdc_dev->msg_chan[hdc_dev->msg_chan_cnt];
    if (msg_chan == NULL) {
        msg_chan = (struct hdcdrv_msg_chan *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_msg_chan), KA_SUB_MODULE_TYPE_0);
        if (msg_chan == NULL) {
            ka_task_mutex_unlock(&hdc_dev->mutex);
            hdcdrv_err("Calling alloc failed. (dev=%u; size=%ld)\n", dev_id, sizeof(struct hdcdrv_msg_chan));
            return HDCDRV_MEM_ALLOC_FAIL;
        }

        if (memset_s(msg_chan, sizeof(struct hdcdrv_msg_chan), 0, sizeof(struct hdcdrv_msg_chan)) != 0) {
            ka_task_mutex_unlock(&hdc_dev->mutex);
            hdcdrv_kvfree(msg_chan, KA_SUB_MODULE_TYPE_0);
            msg_chan = NULL;
            hdcdrv_err("Calling memset_s failed.\n");
            return HDCDRV_SAFE_MEM_OP_FAIL;
        }

        ka_task_mutex_init(&msg_chan->mutex);
    }

    if (hdc_dev->msg_chan_cnt == 0) {
        #ifndef DRV_UT
        if (hdcdrv_init_mem_pool(hdc_dev->dev_id) != HDCDRV_OK) {
            ka_task_mutex_unlock(&hdc_dev->mutex);
            hdcdrv_kvfree(msg_chan, KA_SUB_MODULE_TYPE_0);
            msg_chan = NULL;
            return HDCDRV_DMA_MEM_ALLOC_FAIL;
        }
        #endif
    }

    msg_chan->chan_id = hdc_dev->msg_chan_cnt;
    msg_chan->dev_id = dev_id;
    msg_chan->dev = hdc_dev->dev;
    msg_chan->dma_head = HDCDRV_DESC_QUEUE_DEPTH - 1;
    msg_chan->rx_head = HDCDRV_DESC_QUEUE_DEPTH - 1;
    msg_chan->submit_dma_head = HDCDRV_DESC_QUEUE_DEPTH - 1;
    msg_chan->chan = chan;
    msg_chan->session_cnt = 0;
    msg_chan->dfx_tx_stamp = (u32)ka_jiffies;
    msg_chan->dfx_rx_stamp = (u32)ka_jiffies;

    for (i = 0; i < HDCDRV_DESC_QUEUE_DEPTH; i++) {
        msg_chan->tx[i].buf = NULL;
        msg_chan->rx[i].buf = NULL;
    }

    msg_chan->rx_trigger_flag = HDCDRV_INVALID;
    msg_chan->dma_need_submit_flag = HDCDRV_INVALID;

    ka_system_tasklet_init(&msg_chan->tx_finish_task, hdcdrv_tx_finish_notify_task, (uintptr_t)msg_chan);
    ka_system_tasklet_init(&msg_chan->rx_task, hdcdrv_msg_chan_recv_task, (uintptr_t)msg_chan);
    ka_system_tasklet_init(&msg_chan->tx_sq_task, hdcdrv_msg_chan_tx_sq_task, (uintptr_t)msg_chan);
    ka_system_tasklet_init(&msg_chan->tx_cq_task, hdcdrv_msg_chan_tx_cq_task, (uintptr_t)msg_chan);

    /* only [vfio] and [mdev+sriov's vm] use workqueue, others use tasklet */
    if ((g_rx_notify_sched_mode == HDCDRV_RX_SCHED_WORK) || (devdrv_is_mdev_vm_boot_mode(dev_id) == true)) {
        msg_chan->rx_workqueue = ka_task_create_singlethread_workqueue(HDCDRV_RX_MSG_NORIFY_WORK_NAME);
        if (msg_chan->rx_workqueue == NULL) {
            ka_task_mutex_unlock(&hdc_dev->mutex);
            hdcdrv_err("Calling ka_task_create_singlethread_workqueue failed. (name=\"%s\")\n", HDCDRV_RX_MSG_NORIFY_WORK_NAME);
            hdcdrv_uninit_mem_pool(hdc_dev->dev_id);
            hdcdrv_kvfree(msg_chan, KA_SUB_MODULE_TYPE_0);
            msg_chan = NULL;
            return HDCDRV_MEM_ALLOC_FAIL;
        }
        KA_TASK_INIT_WORK(&msg_chan->rx_notify_work, hdcdrv_rx_msg_notify_work);
    } else {
        ka_system_tasklet_init(&msg_chan->rx_notify_task, hdcdrv_rx_msg_notify_task, (uintptr_t)msg_chan);
    }

    if ((u32)msg_chan->chan_id < hdc_dev->normal_chan_num) {
        msg_chan->type = HDCDRV_MSG_CHAN_TYPE_NORMAL;
    } else {
        msg_chan->type = HDCDRV_MSG_CHAN_TYPE_FAST;
    }

    msg_chan->data_type = hdcdrv_get_dma_data_type(msg_chan->type, msg_chan->chan_id,
        (int)hdc_dev->normal_chan_num);
    msg_chan->wait_mem_list.next = NULL;
    msg_chan->wait_mem_list.prev = NULL;

    ka_task_init_waitqueue_head(&msg_chan->send_wait);

    (void)hdcdrv_set_msg_chan_priv(chan, msg_chan);

    hdc_dev->msg_chan[msg_chan->chan_id] = msg_chan;
    ka_wmb();
    hdc_dev->msg_chan_cnt++;
    ka_task_mutex_unlock(&hdc_dev->mutex);

    return HDCDRV_OK;
}

void hdcdrv_service_res_uninit(struct hdcdrv_service *service, int server_type)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;

    if (hdc_ctrl->service_attr[server_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return;
    }

    if (!ka_list_empty_careful(&service->serv_list)) {
        ka_list_for_each_safe(pos, n, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            ka_list_del(&node->list);
            hdcdrv_kvfree(node, KA_SUB_MODULE_TYPE_0);
            node = NULL;
        }
    }

    return;
}

void hdcdrv_service_init(struct hdcdrv_service *service)
{
    ka_task_mutex_init(&service->mutex);
    ka_task_init_waitqueue_head(&service->wq_conn_avail);
    KA_INIT_LIST_HEAD(&service->serv_list);

    service->listen_pid = HDCDRV_INVALID;
    service->conn_list_head = NULL;
}

STATIC int hdcdrv_service_res_init(struct hdcdrv_service *service, int server_type)
{
    struct hdcdrv_serv_list_node *node = NULL;
    int i = 0;

    if (hdc_ctrl->service_attr[server_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return HDCDRV_OK;
    }

    if (ka_list_empty_careful(&service->serv_list) != 0) {
        for (i = 0; i < HDCDRV_SERVER_PROCESS_MAX_NUM; i++) {
            node = (struct hdcdrv_serv_list_node *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_serv_list_node), KA_SUB_MODULE_TYPE_0);
            if (ka_unlikely(node == NULL)) {
                hdcdrv_err("Calling kzalloc failed. (process_num=%d; server_type=%d)\n", i, server_type);
                return HDCDRV_ERR;
            }

            hdcdrv_service_init(&node->service);
            ka_list_add(&node->list, &service->serv_list);
        }
    }

    return HDCDRV_OK;
}

struct hdcdrv_dev *hdcdrv_add_dev(ka_device_t *dev, u32 dev_id)
{
    struct hdcdrv_dev *hdc_dev = NULL;
    int i;

    if (dev_id >= (u32)hdcdrv_get_max_support_dev()) {
        hdcdrv_err("Input pararmeter is error. (dev_id=%u; max=%d)\n", dev_id, hdcdrv_get_max_support_dev());
        return NULL;
    }

    hdc_dev = &hdc_ctrl->devices[dev_id];

    if (hdc_dev->sync_mem_buf == NULL) {
        hdc_dev->sync_mem_buf = (void *)hdcdrv_vzalloc(HDCDRV_NON_TRANS_MSG_S_DESC_SIZE, KA_SUB_MODULE_TYPE_0);
        if (hdc_dev->sync_mem_buf == NULL) {
            hdcdrv_err("Calling alloc failed. (dev_id=%d)\n", dev_id);
            return NULL;
        }
    }
    /* suspend&resume: server not free */
    if (hdcdrv_get_running_status() == HDCDRV_RUNNING_NORMAL) {
        ka_task_mutex_lock(&hdc_dev->mutex);
        for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
            if (hdcdrv_service_res_init(&hdc_dev->service[i], i) != 0) {
                ka_task_mutex_unlock(&hdc_dev->mutex);
                hdcdrv_err("Server resure init failed. (dev_id=%d; server=%d)\n", dev_id, i);
                goto OUT;
            }
        }
        ka_task_mutex_unlock(&hdc_dev->mutex);
    }
    hdc_dev->dev = dev;
    hdc_dev->dev_id = dev_id;

    return hdc_dev;
OUT:
    if (hdc_dev->sync_mem_buf != NULL) {
        hdcdrv_vfree(hdc_dev->sync_mem_buf, KA_SUB_MODULE_TYPE_0);
        hdc_dev->sync_mem_buf = NULL;
    }

    ka_task_mutex_lock(&hdc_dev->mutex);
    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        hdcdrv_service_res_uninit(&hdc_dev->service[i], i);
    }
    ka_task_mutex_unlock(&hdc_dev->mutex);
    return NULL;
}

STATIC void hdcdrv_reset_session(int dev_id)
{
    struct hdcdrv_session *session = NULL;
    int id;

    /* only uninit free, suspend status session not free */
    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return;
    }
    for (id = 0; id < HDCDRV_REAL_MAX_SESSION; id++) {
        session = &hdc_ctrl->sessions[id];
        if (session->dev_id != dev_id) {
            continue;
        }
        if (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE) {
            if (ka_task_delayed_work_pending(&session->remote_close) != 0) {
                (void)ka_task_cancel_delayed_work_sync(&session->remote_close);
            }
            continue;
        }
        hdcdrv_info("Get reset info. (dev=%d; session=%d)\n", dev_id, id);
        hdcdrv_unbind_session_ctx(session);
        hdcdrv_session_free(session, HDCDRV_CLOSE_TYPE_NONE);
    }
}

STATIC void hdcdrv_reset_process_server(struct hdcdrv_dev *dev, int service_type)
{
    struct hdcdrv_serv_list_node *node = NULL;
    ka_list_head_t *pos = NULL, *n = NULL;
    struct hdcdrv_service *service = NULL;

    service = &dev->service[service_type];

    if (hdc_ctrl->service_attr[service_type].service_scope == HDCDRV_SERVICE_SCOPE_GLOBAL) {
        return;
    }

    ka_task_mutex_lock(&dev->mutex);
    if (!ka_list_empty_careful(&service->serv_list)) {
        ka_list_for_each_safe(pos, n, &service->serv_list) {
            node = ka_list_entry(pos, struct hdcdrv_serv_list_node, list);
            if (node->service.listen_status == HDCDRV_VALID) {
                (void)hdcdrv_server_free(&node->service, (int)dev->dev_id, service_type);
            }
        }
    }
    ka_task_mutex_unlock(&dev->mutex);

    return;
}

STATIC void hdcdrv_reset_service(struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_service *service = NULL;
    int i;
    int ret = 0;

    /* only uninit free, suspend status server not free */
    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return;
    }

    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        service = &hdc_dev->service[i];
        if (service->listen_status == HDCDRV_VALID) {
            hdcdrv_info("Service wakeup accept. (dev=%d; service=%d)\n", hdc_dev->dev_id, i);
            ret = (int)hdcdrv_server_free(service, (int)hdc_dev->dev_id, i);
            if (ret != HDCDRV_OK) {
                hdcdrv_warn("Reset not success. (dev=%d; service=%d)\n", hdc_dev->dev_id, i);
            }
        }

        hdcdrv_reset_process_server(hdc_dev, i);
    }
}

STATIC void hdcdrv_reset_msgchain(struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_msg_chan *msg_chan = NULL;
    int i;

    for (i = 0; i < hdc_dev->msg_chan_cnt; i++) {
        msg_chan = hdc_dev->msg_chan[i];
        /* Clear the packets in the waiting queue */
        ka_task_wake_up_interruptible(&msg_chan->send_wait);
    }
}

STATIC void hdcdrv_kill_task(struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_msg_chan *msg_chan = NULL;
    int msg_chan_num = hdc_dev->msg_chan_cnt;
    int i;

    for (i = 0; i < msg_chan_num; i++) {
        msg_chan = hdc_dev->msg_chan[i];
        (void)hdcdrv_set_msg_chan_priv(msg_chan->chan, NULL);
        ka_system_tasklet_kill(&msg_chan->tx_sq_task);
        ka_system_tasklet_kill(&msg_chan->tx_finish_task);
        if ((g_rx_notify_sched_mode == HDCDRV_RX_SCHED_WORK) ||
            (devdrv_is_mdev_vm_boot_mode(msg_chan->dev_id) == true)) {
            ka_task_destroy_workqueue(msg_chan->rx_workqueue);
        } else {
            ka_system_tasklet_kill(&msg_chan->rx_notify_task);
        }
        ka_system_tasklet_kill(&msg_chan->rx_task);
        ka_system_tasklet_kill(&msg_chan->tx_cq_task);
#ifdef CFG_FEATURE_PFSTAT
        (void)hdcdrv_pfstat_delete_stats_handle(msg_chan->chan_id);
#endif
    }
#ifdef CFG_FEATURE_PFSTAT
    (void)hdcdrv_pfstat_delete_stats_handle(HDCDRV_PFSTATE_NON_TRANS_CHAN_ID);
#endif
}

void hdcdrv_free_msg_chan(struct hdcdrv_dev *hdc_dev)
{
    int msg_chan_num = hdc_dev->msg_chan_cnt;
    int i;

    for (i = 0; i < msg_chan_num; i++) {
        ka_task_mutex_lock(&hdc_dev->mutex);
        hdc_dev->msg_chan_cnt--;
        ka_task_mutex_unlock(&hdc_dev->mutex);
    }
}

STATIC void hdcdrv_free_msg_chan_mem_handle(struct hdcdrv_buf_desc *desc)
{
    if (desc->buf != NULL) {
        if (desc->fid == HDCDRV_DEFAULT_PM_FID) {
#ifdef CFG_FEATURE_MIRROR
            hdcdrv_free_mem_mirror(desc->buf, desc->mem_id, desc->len);
#else
            free_mem(desc->buf);
#endif
        }
        desc->buf = NULL;
    }
}
STATIC void hdcdrv_free_msg_chan_mem(struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_msg_chan *msg_chan = NULL;
    int msg_chan_num = hdc_dev->msg_chan_cnt;
    int i, j;

    for (i = 0; i < msg_chan_num; i++) {
        msg_chan = hdc_dev->msg_chan[i];
        for (j = 0; j < HDCDRV_DESC_QUEUE_DEPTH; j++) {
            hdcdrv_free_msg_chan_mem_handle(&msg_chan->tx[j]);
            hdcdrv_free_msg_chan_mem_handle(&msg_chan->rx[j]);
        }
    }
}

void hdcdrv_stop_work(struct hdcdrv_dev *hdc_dev)
{
    int dev_id = (int)hdc_dev->dev_id;

    hdcdrv_reset_session(dev_id);
    hdcdrv_reset_service(hdc_dev);
    hdcdrv_kill_task(hdc_dev);
}

void hdcdrv_remove_dev(struct hdcdrv_dev *hdc_dev)
{
    int dev_id = (int)hdc_dev->dev_id;
    hdcdrv_free_msg_chan_mem(hdc_dev);
    hdcdrv_free_msg_chan(hdc_dev);
    hdcdrv_uninit_mem_pool((u32)dev_id);
    hdcdrv_fast_mem_uninit(&(hdc_ctrl->devices[dev_id].fmem.rb_lock),
        &(hdc_ctrl->devices[dev_id].fmem.rbtree_re), HDCDRV_TRUE_FLAG, HDCDRV_DEL_FLAG);
    hdcdrv_fast_mem_sep_uninit(&(hdc_ctrl->devices[dev_id].fmem.rb_lock),
        &(hdc_ctrl->devices[dev_id].fmem.rbtree));
}

struct hdcdrv_dev *hdcdrv_get_dev(u32 dev_id)
{
    return &hdc_ctrl->devices[dev_id];
}

void hdcdrv_free_dev_mem(u32 dev_id)
{
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];
    int i;

    for (i = 0; i < HDCDRV_SUPPORT_MAX_DEV_MSG_CHAN; i++) {
        if (hdc_dev->msg_chan[i] != NULL) {
            if (hdc_dev->msg_chan[i]->node != NULL) {
                hdcdrv_ka_kvfree(hdc_dev->msg_chan[i]->node, KA_SUB_MODULE_TYPE_2);
                hdc_dev->msg_chan[i]->node = NULL;
            }
            hdcdrv_kvfree(hdc_dev->msg_chan[i], KA_SUB_MODULE_TYPE_0);
            hdc_dev->msg_chan[i] = NULL;
        }
    }

    if (hdc_dev->sync_mem_buf != NULL) {
        hdcdrv_vfree(hdc_dev->sync_mem_buf, KA_SUB_MODULE_TYPE_0);
        hdc_dev->sync_mem_buf = NULL;
    }
}

void hdcdrv_del_dev(u32 dev_id)
{
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];

    if (hdc_dev->valid == HDCDRV_INVALID) {
        return;
    }

    hdcdrv_reset_dev(hdc_dev);
    ka_system_msleep(500);  // sleep 500 ms
    hdcdrv_stop_work(hdc_dev);
    hdcdrv_remove_dev(hdc_dev);
}

void hdcdrv_reset_dev(struct hdcdrv_dev *hdc_dev)
{
    hdcdrv_info("Reset device. (dev_id=%u)\n", hdc_dev->dev_id);

    hdcdrv_set_device_status((int)hdc_dev->dev_id, HDCDRV_INVALID);
    ka_system_msleep(10);  // sleep 10 ms
    hdcdrv_reset_msgchain(hdc_dev);
    hdcdrv_reset_session((int)hdc_dev->dev_id);
    hdcdrv_reset_service(hdc_dev);
}

STATIC unsigned long hdcdrv_get_unmapped_area(ka_file_t *filp, unsigned long addr, unsigned long len,
    unsigned long pgoff, unsigned long flags)
{
    if ((flags & KA_MAP_FIXED) != 0) {
        hdcdrv_err("hdc not support KA_MAP_FIXED\n");
        return -EINVAL;
    }

    if (filp == NULL) {
        hdcdrv_warn("filp is NULL in hdcdrv_get_unmapped_area\n");
        return -ENODEV;
    }

    return ka_task_get_current_get_unmapped_area(filp, addr, len, pgoff, flags);
}

const ka_file_operations_t hdcdrv_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_unlocked_ioctl(hdcdrv_ioctl)
    ka_fs_init_f_open(hdcdrv_open)
    ka_fs_init_f_mmap(hdcdrv_mmap)
    ka_fs_init_f_release(hdcdrv_release)
    ka_fs_init_f_get_unmapped_area(hdcdrv_get_unmapped_area)
};

STATIC int hdcdrv_register_cdev(void)
{
    int ret;

    ret = hdccom_register_cdev(&hdc_ctrl->hdc_cdev, &hdcdrv_fops);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("HDC create char device failed. (ret=%d)\n", ret);
        return ret;
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_free_cdev(void)
{
    hdccom_free_cdev(&hdc_ctrl->hdc_cdev);
}

int hdcdrv_get_running_status(void)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    return hdc_ctrl->running_statue;
#else
    return 0;
#endif
}

void hdcdrv_set_running_status(int statue)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdc_ctrl->running_statue = statue;
#else
    return;
#endif
}


int hdcdrv_session_free_check(int show_log)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    int id = 0;
    struct hdcdrv_session *session = NULL;

    /* check session status: before suspend, all session must be closed */
    for (id = 0; id < HDCDRV_REAL_MAX_SESSION; id++) {
        session = &hdc_ctrl->sessions[id];
        if (session == NULL) {
            if (show_log == 1) {
                hdcdrv_err("hdcdrv_suspend fail, session(id=%d) is null.\n", id);
            }
            return HDCDRV_ERR;
        }
        if (hdcdrv_get_session_status(session) != HDCDRV_SESSION_STATUS_IDLE) {
            if (session->owner_pid == HDCDRV_INVALID_PID) {
                ka_task_wake_up_interruptible(&session->wq_conn);
                hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
                hdcdrv_warn_limit("session free.(session_fd=%d)\n", session->local_session_fd);
                continue;
            }
            if (show_log == 1) {
                hdcdrv_err("hdcdrv_suspend fail, have session(id=%d, owner_pid=%llu) not release.\n",
                    session->local_session_fd, session->owner_pid);
            }
            return HDCDRV_ERR;
        }
    }
#endif
    return HDCDRV_OK;
}

#ifdef CFG_FEATURE_HDC_REG_MEM
STATIC void hdcdrv_session_work_wakeup(void)
{
    struct hdcdrv_service *service = NULL;
    struct hdcdrv_session *session = NULL;
    struct hdcdrv_dev *hdc_dev = NULL;
    u32 dev_id, i;

    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return;
    }

    /* wake wait_always work */
    for (i = 0; i < HDCDRV_REAL_MAX_SESSION; i++) {
        session = &hdc_ctrl->sessions[i];
        if ((session == NULL) || (hdcdrv_get_session_status(session) == HDCDRV_SESSION_STATUS_IDLE)) {
            continue;
        }

        hdcdrv_peer_fault_del_mem_release_event(session);
        ka_task_wake_up_interruptible(&session->wq_rx);

        if (session->msg_chan != NULL) {
            ka_task_wake_up_interruptible(&session->msg_chan->send_wait);
        }

        if (session->fast_msg_chan != NULL) {
            ka_task_wake_up_interruptible(&session->fast_msg_chan->send_wait);
        }
    }

    /* wake accept work */
    for (dev_id = 0; dev_id < (u32)hdcdrv_get_max_support_dev(); dev_id++) {
        hdc_dev = &hdc_ctrl->devices[dev_id];
        if (hdc_dev->valid == HDCDRV_INVALID) {
            continue;
        }
        for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
            service = &hdc_dev->service[i];
            if (service->listen_status != HDCDRV_VALID) {
                continue;
            }

            hdcdrv_info("Service wakeup accept. (dev=%d; service=%d)\n", hdc_dev->dev_id, i);
            ka_task_wake_up_interruptible(&service->wq_conn_avail);
        }
    }
}
#endif

int hdcdrv_peer_fault_notify(u32 status)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdcdrv_warn("peer status is abnormal, notify hdc. (status=%u)\n", status);
    // set status, stop in-comming hdc service
    hdcdrv_set_peer_status(status);
    // stop in-processing hdc service
    hdcdrv_session_work_wakeup();
#endif
    return 0;
}


int hdcdrv_suspend(u32 dev_id)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    int ret;
    u32 dev_idx;

    hdcdrv_set_running_status(HDCDRV_RUNNING_SUSPEND_ENTERING);
    hdcdrv_info("hdcdrv suspend start. (dev_id=%u)\n", dev_id);

    /* check session status: before suspend, all session must be closed */
    ret = hdcdrv_session_free_check(1);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("hdcdrv_suspend fail.\n");
        return HDCDRV_ERR;
    }

    (void)ka_task_cancel_delayed_work_sync(&hdc_ctrl->recycle);
    (void)ka_task_cancel_delayed_work_sync(&hdc_ctrl->recycle_mem);

    ret = hdcdrv_uninit_instance(dev_id);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("hdcdrv_suspend fail.\n");
        return HDCDRV_ERR;
    }

    for (dev_idx = 0; dev_idx < (u32)hdcdrv_get_max_support_dev(); dev_idx++) {
        hdcdrv_free_dev_mem(dev_idx);
    }
    hdcdrv_set_running_status(HDCDRV_RUNNING_SUSPEND);
#endif
    hdcdrv_info("hdcdrv suspend success. (dev_id=%u)\n", dev_id);
    return 0;
}

int hdcdrv_resume(u32 dev_id, ka_device_t *dev)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    int ret;

    hdcdrv_info("hdcdrv resume start. (dev_id=%u)\n", dev_id);
    hdcdrv_set_running_status(HDCDRV_RUNNING_RESUME);
    KA_TASK_INIT_DELAYED_WORK(&hdc_ctrl->recycle, hdcdrv_guard_work);
    KA_TASK_INIT_DELAYED_WORK(&hdc_ctrl->recycle_mem, hdcdrv_recycle_mem_work);
    (void)ka_task_schedule_delayed_work(&hdc_ctrl->recycle, HDCDRV_RECYCLE_DELAY_TIME * KA_HZ);

    ret = hdcdrv_init_instance(dev_id, dev);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("hdcdrv_suspend fail.\n");
        return HDCDRV_ERR;
    }
#endif
    hdcdrv_info("hdcdrv resume success. (dev_id=%u)\n", dev_id);
    return 0;
}

STATIC void hdcdrv_service_attr_init(void)
{
    int i = 0;

    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        hdc_ctrl->service_attr[i].level = hdcdrv_service_level_init(i);
        hdc_ctrl->service_attr[i].conn_feature = hdcdrv_service_conn_feature_init(i);
        hdc_ctrl->service_attr[i].service_scope = hdcdrv_service_scope_init(i);
        hdc_ctrl->service_attr[i].log_limit = hdcdrv_service_log_limit_init(i);
    }
}

STATIC void hdcdrv_session_pre_init(struct hdcdrv_session *session)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    int ret = 0;
#endif
    ka_task_spin_lock_init(&session->lock);
    ka_task_mutex_init(&session->mutex);
    ka_task_init_waitqueue_head(&session->wq_conn);
    ka_task_init_waitqueue_head(&session->wq_rx);
#ifdef CFG_FEATURE_HDC_REG_MEM
    ret = hdcdrv_init_session_release_mem_event(session);
    if (ret != HDCDRV_OK) {
        hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
        hdcdrv_err("Init send mem finish event fifo failed.\n");
        return;
    }
#endif
    KA_TASK_INIT_DELAYED_WORK(&session->remote_close, hdcdrv_remote_close_work);
    KA_TASK_INIT_DELAYED_WORK(&session->close_unknow_session, hdcdrv_close_unknow_session_work);
#ifdef CFG_FEATURE_VFIO
    hdcdrv_session_work_init(&session->session_work);
#endif
    hdcdrv_set_session_status(session, HDCDRV_SESSION_STATUS_IDLE);
    session->owner = (u32)HDCDRV_INVALID_VALUE;
    session->owner_pid = HDCDRV_INVALID_PID;
    session->create_pid = HDCDRV_INVALID_PID;
    session->peer_create_pid = HDCDRV_INVALID_PID;
    session->local_close_state = HDCDRV_CLOSE_TYPE_NONE;
    session->remote_close_state = HDCDRV_CLOSE_TYPE_NONE;
    session->pid_flag = 0;
    session->task_start_time = HDCDRV_KERNEL_DEFAULT_START_TIME;
}
#ifdef CFG_FEATURE_HDC_REG_MEM
/* alloc node buff and init params */
STATIC struct hdcdrv_node_tree_ctrl *hdcdrv_new_tree_init(void)
{
    int i;
    int j;
    struct hdcdrv_node_tree *node_tree = NULL;
    hdc_node_tree = (struct hdcdrv_node_tree_ctrl *)hdcdrv_vzalloc(sizeof(struct hdcdrv_node_tree_ctrl),
        KA_SUB_MODULE_TYPE_0);
    if (hdc_node_tree == NULL) {
        hdcdrv_err("hdc_node_tree calling alloc failed. (size=%ld)\n", sizeof(struct hdcdrv_node_tree_ctrl));
        return NULL;
    }

    ka_task_rwlock_init(&hdc_node_tree->lock);
    for (i = 0; i < HDCDRV_SUPPORT_MAX_FID_PID; i++) {
        node_tree = &hdc_node_tree->node_tree[i];
        node_tree->local_tree.tree_info.pid = HDCDRV_INVALID_PID;
        node_tree->local_tree.tree_info.fid = HDCDRV_INVALID_FID;
        ka_base_atomic_set(&node_tree->local_tree.tree_info.refcnt, 0);
        node_tree->local_tree.tree_info.idx = i;
        node_tree->local_tree.fmem.rbtree = KA_RB_ROOT;
        node_tree->local_tree.fmem.rbtree_re = KA_RB_ROOT;
        ka_task_spin_lock_init(&node_tree->local_tree.fmem.rb_lock);
        ka_task_spin_lock_init(&node_tree->local_tree.fmem.mem_dfx_stat.lock);

        node_tree->re_tree.tree_info.pid = HDCDRV_INVALID_PID;
        node_tree->re_tree.tree_info.fid = HDCDRV_INVALID_FID;
        ka_base_atomic_set(&node_tree->re_tree.tree_info.refcnt, 0);
        node_tree->re_tree.tree_info.idx = i;
        for (j = 0; j < hdcdrv_get_max_support_dev(); j++) {
            node_tree->re_tree.devices[j].rbtree = KA_RB_ROOT;
            node_tree->re_tree.devices[j].rbtree_re = KA_RB_ROOT;
            ka_task_spin_lock_init(&node_tree->re_tree.devices[j].rb_lock);
            ka_task_spin_lock_init(&node_tree->re_tree.devices[j].mem_dfx_stat.lock);
        }
    }
    hdcdrv_info("hdc_node_tree init success.\n");
    return hdc_node_tree;
}
#endif

STATIC struct hdcdrv_ctrl *hdcdrv_ctrl_init(void)
{
    int i, j;
    int dev_num;

    dev_num = devdrv_get_davinci_dev_num();
    if (dev_num <= 0) {
        hdcdrv_info("No pci dev, set dev num %d to 1\n", dev_num);
        dev_num = 1;
    }
    if (dev_num > HDCDRV_SUPPORT_MAX_DEV) {
        hdcdrv_info("change dev num %d to max dev num %d\n", dev_num, HDCDRV_SUPPORT_MAX_DEV);
        dev_num = HDCDRV_SUPPORT_MAX_DEV;
    }

    hdcdrv_dev_num = (u32)dev_num;

    hdc_ctrl = (struct hdcdrv_ctrl *)hdcdrv_vzalloc(sizeof(struct hdcdrv_ctrl), KA_SUB_MODULE_TYPE_0);
    if (hdc_ctrl == NULL) {
        hdcdrv_err("Calling alloc failed. (size=%ld)\n", sizeof(struct hdcdrv_ctrl));
        return NULL;
    }

    hdc_ctrl->sessions = (struct hdcdrv_session *)hdcdrv_vzalloc(sizeof(struct hdcdrv_session) * dev_num *
        HDCDRV_SINGLE_DEV_MAX_SESSION, KA_SUB_MODULE_TYPE_0);
    if (hdc_ctrl->sessions == NULL) {
        hdcdrv_vfree(hdc_ctrl, KA_SUB_MODULE_TYPE_0);
        hdc_ctrl = NULL;
        hdcdrv_err("alloc sessions failed, size %ld dev_num %d single session %d\n", sizeof(struct hdcdrv_session),
            dev_num, HDCDRV_SINGLE_DEV_MAX_SESSION);
        return NULL;
    }

    hdcdrv_info("Sessions size %lu; dev_num %d; session num %d\n", sizeof(struct hdcdrv_session), dev_num,
        HDCDRV_REAL_MAX_SESSION);

    ka_task_mutex_init(&hdc_ctrl->mutex);
    hdc_ctrl->running_statue = HDCDRV_RUNNING_NORMAL;
    hdc_ctrl->segment = HDCDRV_HUGE_PACKET_SEGMENT;
    hdc_ctrl->cur_alloc_short_session = (int)HDCDRV_REAL_MAX_LONG_SESSION;

    hdc_ctrl->fmem.rbtree = KA_RB_ROOT;
    hdc_ctrl->fmem.rbtree_re = KA_RB_ROOT;
    ka_task_spin_lock_init(&hdc_ctrl->fmem.rb_lock);
    ka_task_spin_lock_init(&hdc_ctrl->fmem.mem_dfx_stat.lock);

    for (i = 0; i < hdcdrv_get_max_support_dev(); i++) {
        hdcdrv_set_peer_dev_id(i, HDCDRV_INVALID_VALUE);
        ka_task_mutex_init(&hdc_ctrl->devices[i].sync_mem_mutex);
        ka_task_mutex_init(&hdc_ctrl->devices[i].mutex);

        hdc_ctrl->devices[i].fmem.rbtree = KA_RB_ROOT;
        hdc_ctrl->devices[i].fmem.rbtree_re = KA_RB_ROOT;
        ka_task_spin_lock_init(&hdc_ctrl->devices[i].fmem.rb_lock);
        ka_task_spin_lock_init(&hdc_ctrl->devices[i].fmem.mem_dfx_stat.lock);

        for (j = 0; j < HDCDRV_SUPPORT_MAX_SERVICE; j++) {
            hdcdrv_service_init(&hdc_ctrl->devices[i].service[j]);
        }
    }
    hdc_ctrl->debug_state.valid = HDCDRV_INVALID;
    hdc_ctrl->debug_state.pid = HDCDRV_INVALID_VALUE;

    hdcdrv_service_attr_init();

    for (i = 0; i < hdcdrv_get_max_support_dev(); i++) {
        hdc_ctrl->devices[i].valid = HDCDRV_INVALID;
    }

    for (i = 0; i < HDCDRV_REAL_MAX_SESSION; i++) {
        hdcdrv_session_pre_init(&hdc_ctrl->sessions[i]);
    }

    hdc_ctrl->pm_version = (int)HDC_VERSION;
    hdcdrv_info("HDC PM_VERSION = %d.\n", hdc_ctrl->pm_version);

    return hdc_ctrl;
}

STATIC void hdcdrv_session_notify_init(void)
{
    int i = 0;

    for (i = 0; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        g_session_notify[i].connect_notify = NULL;
        g_session_notify[i].close_notify = NULL;
        g_session_notify[i].data_in_notify = NULL;
    }
}

int hdcdrv_init(void)
{
    int ret;

#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_MIRROR)
    g_rx_notify_sched_mode = HDCDRV_RX_SCHED_WORK;
#endif

    hdcdrv_fill_service_type_str();

    hdc_ctrl = hdcdrv_ctrl_init();
    if (hdc_ctrl == NULL) {
        hdcdrv_err("Calling hdcdrv_ctrl_init failed. (size=%ld)\n", sizeof(struct hdcdrv_ctrl));
        goto HDC_CTRL_INIT_FAIL;
    }
    hdcdrv_peer_status_init();
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdc_node_tree = hdcdrv_new_tree_init();
    if (hdc_node_tree == NULL) {
        hdcdrv_err("Calling hdcdrv_ctrl_arry_init failed. (size=%ld)\n", sizeof(struct hdcdrv_node_tree_ctrl));
        goto HDC_CTRL_ARRY_INIT_FAIL;
    }
    hdcdrv_dsmi_feature_init();
#ifdef HDC_ENV_DEVICE
    agentdrv_pci_suspend_check_register(hdcdrv_session_free_check);
    agentdrv_peer_fault_notifier_register(hdcdrv_peer_fault_notify);
#else
    devdrv_pci_suspend_check_register(hdcdrv_session_free_check);
    devdrv_peer_fault_notifier_register(hdcdrv_peer_fault_notify);
#endif
#endif
    if (hdcdrv_register_cdev() != 0) {
        hdcdrv_err("Calling hdcdrv_register_cdev failed.\n");
        goto REGISTER_CDEV_FAIL;
    }

    hdcdrv_session_notify_init();

    ret = hdcdrv_epoll_init(&hdc_ctrl->epolls);
    if (ret != 0) {
        hdcdrv_err("Calling hdcdrv_epoll_init failed.\n");
        goto HDC_EPOLL_INIT_FAIL;
    }

    hdcdrv_sysfs_init(hdc_ctrl->hdc_cdev.dev);

    async_release_workqueue = ka_task_create_singlethread_workqueue(HDCDRV_ASYNC_RELEASE_WORK_NAME);
    if (async_release_workqueue == NULL) {
        hdcdrv_err("Calling ka_task_create_singlethread_workqueue failed.\n");
        goto HDC_CREATE_WORKQUEUE_FAIL;
    }
    KA_TASK_INIT_DELAYED_WORK(&hdc_ctrl->recycle, hdcdrv_guard_work);
    KA_TASK_INIT_DELAYED_WORK(&hdc_ctrl->recycle_mem, hdcdrv_recycle_mem_work);
    (void)ka_task_schedule_delayed_work(&hdc_ctrl->recycle, HDCDRV_RECYCLE_DELAY_TIME * KA_HZ);
    ka_task_init_rwsem(&g_symbol_lock);

    hdcdrv_info("Get sq cq desc size.(sq_size=%d; cq_size=%d)\n", (int)HDCDRV_SQ_DESC_SIZE, (int)HDCDRV_CQ_DESC_SIZE);
    return HDCDRV_OK;

HDC_CREATE_WORKQUEUE_FAIL:
    hdcdrv_epoll_uninit(&hdc_ctrl->epolls);
HDC_EPOLL_INIT_FAIL:
    hdcdrv_free_cdev();
REGISTER_CDEV_FAIL:
#ifdef CFG_FEATURE_HDC_REG_MEM
    hdcdrv_vfree(hdc_node_tree, KA_SUB_MODULE_TYPE_0);
    hdc_node_tree = NULL;
HDC_CTRL_ARRY_INIT_FAIL:
#endif
    hdcdrv_vfree(hdc_ctrl->sessions, KA_SUB_MODULE_TYPE_0);
    hdc_ctrl->sessions = NULL;
    hdcdrv_vfree(hdc_ctrl, KA_SUB_MODULE_TYPE_0);
    hdc_ctrl = NULL;

HDC_CTRL_INIT_FAIL:
    return HDCDRV_MEM_ALLOC_FAIL;
}

void hdcdrv_uninit(void)
{
    u32 dev_id;
    int server_type;
#ifdef CFG_FEATURE_HDC_REG_MEM
    int id = 0;
#endif

    (void)ka_task_cancel_delayed_work_sync(&hdc_ctrl->recycle);
    (void)ka_task_cancel_delayed_work_sync(&hdc_ctrl->recycle_mem);

    ka_task_destroy_workqueue(async_release_workqueue);
    hdcdrv_sysfs_uninit(hdc_ctrl->hdc_cdev.dev);
    hdcdrv_free_cdev();
#ifdef CFG_FEATURE_HDC_REG_MEM
#ifdef HDC_ENV_DEVICE
    agentdrv_pci_suspend_check_unregister();
    agentdrv_peer_fault_notifier_unregister();
#else
    devdrv_pci_suspend_check_unregister();
    devdrv_peer_fault_notifier_unregister();
#endif
    hdcdrv_fast_mem_arry_uninit();
    hdcdrv_dsmi_feature_uninit();
#endif

    for (dev_id = 0; dev_id < (u32)hdcdrv_get_max_support_dev(); dev_id++) {
        hdcdrv_del_dev(dev_id);
        hdcdrv_free_dev_mem(dev_id);
        hdcdrv_fast_mem_sep_uninit(&(hdc_ctrl->devices[dev_id].fmem.rb_lock),
            &(hdc_ctrl->devices[dev_id].fmem.rbtree));
        ka_task_mutex_lock(&hdc_ctrl->devices[dev_id].mutex);
        for (server_type = 0; server_type < HDCDRV_SUPPORT_MAX_SERVICE; server_type++) {
            hdcdrv_service_res_uninit(&hdc_ctrl->devices[dev_id].service[server_type], server_type);
        }
        ka_task_mutex_unlock(&hdc_ctrl->devices[dev_id].mutex);
    }

    hdcdrv_fast_mem_uninit(&(hdc_ctrl->fmem.rb_lock), &(hdc_ctrl->fmem.rbtree), HDCDRV_TRUE_FLAG, HDCDRV_DEL_FLAG);

    hdcdrv_epoll_uninit(&hdc_ctrl->epolls);

    // free mem release fifo buffer when hdc driver uninit
#ifdef CFG_FEATURE_HDC_REG_MEM
    for (id = 0; id < HDCDRV_REAL_MAX_SESSION; id++) {
        hdcdrv_free_session_release_mem_event(&hdc_ctrl->sessions[id]);
    }

    hdcdrv_vfree(hdc_node_tree, KA_SUB_MODULE_TYPE_0);
    hdc_node_tree = NULL;
#endif

#if defined(CFG_FEATURE_VFIO) || defined(CFG_FEATURE_MIRROR)
        g_rx_notify_sched_mode = HDCDRV_RX_SCHED_TASKLET;
#endif

    hdcdrv_vfree(hdc_ctrl->sessions, KA_SUB_MODULE_TYPE_0);
    hdc_ctrl->sessions = NULL;
    hdcdrv_vfree(hdc_ctrl, KA_SUB_MODULE_TYPE_0);
    hdc_ctrl = NULL;
}

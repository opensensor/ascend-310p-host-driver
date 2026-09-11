#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/string.h>
#include <asm/io.h>
#include <linux/kthread.h>

#include <linux/mutex.h>
#include <linux/cpumask.h>

#include "drv_mailbox.h"
#include "devdrv_functional_cqsq_api.h"
#include "drv_log.h"

struct workqueue_struct *poll_queue = NULL;

/* 回调函数没有信息devid, 暂时不统一信号量的使用 */
struct semaphore ts_wait_sema;
spinlock_t work_queue_lock;
u8 ts_callback_buffer[DEBUG_CALLBACK_MAILBOX_LEN] = { 0 };

struct devdrv_cqsq debug_cqsq[DEV_NUM] = { 0 };

void devdrv_cq_report_debug(u32 device_id, u32 tsid, const u8 *cq_buf, u8 *sq_buf)
{
    struct ts_cmd *cmd_info = NULL;
    int ret;

    if ((cq_buf == NULL) || (sq_buf == NULL)) {
        mailbox_err("buf is null.\n");
        return;
    }

    cmd_info = (struct ts_cmd *)sq_buf;

    if (cmd_info->cmd_verify == ((struct ts_cmd_report *)cq_buf)->cmd_verify) {
        ret = memcpy_s(cmd_info->ts_report, sizeof(struct ts_cmd_report),
                       cq_buf, sizeof(struct ts_cmd_report));
        if (ret != 0) {
            mailbox_err("memcpy_s unsuccessful ret= %d.\n", ret);
            return;
        }

        up(&debug_cqsq[cmd_info->dev_id].sync_wait_sema);
    }

    return;
}

/* callback; sq_buf is null */
void devdrv_ts_callback_debug(u32 device_id, u32 tsid, const u8 *cq_buf, u8 *sq_buf)
{
    int ret;

    if (cq_buf == NULL) {
        mailbox_err("buf is null.\n");
        return;
    }

    ret = memcpy_s(ts_callback_buffer, DEBUG_CALLBACK_MAILBOX_LEN,
                   cq_buf, sizeof(struct ts2driver_info));
    if (ret != 0) {
        mailbox_err("memcpy_s unsuccessful ret= %d.\n", ret);
        return;
    }

    up(&ts_wait_sema);

    return;
}

s32 devdrv_cqsq_init_dbg(u32 devid, enum drv_type flag)
{
    void (*cq_callback)(u32 device_id, u32 tsid, const u8 *cq_slot, u8 *sq_slot) = NULL;
    void (*ts_callback)(u32 device_id, u32 tsid, const u8 *cq_slot, u8 *sq_slot) = NULL;
    void (*aicpu_callback)(u32 device_id, u32 tsid, u8 *cq_slot, u8 *sq_slot) = NULL;
    struct devdrv_mailbox_cqsq cqsq_mailbox;
    struct devdrv_cqsq *cqsq = NULL;
    u32 sq_index = 0;
    u32 cq_index = 0;
    u32 ts_cq_index = 0;
    u64 addr_sq = 0;
    u64 addr_cq = 0;
    u64 addr_ts_cq = 0;
    u16 cmd;
    int ret;
    u32 tsid = 0;

    if (devid >= DEV_NUM) {
        mailbox_err("devid = %d\n", devid);
        return IDE_DRV_ERROR;
    }

    switch (flag) {
        case DRV_DEBUG:
            cq_callback = devdrv_cq_report_debug;
            ts_callback = devdrv_ts_callback_debug;
            aicpu_callback = NULL;
            cmd = DEBUG_CQSQ_CREATE;
            cqsq = &debug_cqsq[devid];
            break;
        default:
            mailbox_err("invalid flag.\n");
            return IDE_DRV_ERROR;
    }

    /* 初始化信号量和互斥量 */
    sema_init(&cqsq->sync_wait_sema, 0);
    sema_init(&cqsq->callback_wait_sema, 0);

    ret = devdrv_create_functional_sq(devid, tsid, DEBUG_MAILBOX_LEN, &sq_index, &addr_sq);
    if (ret != IDE_DRV_OK) {
        mailbox_err("devdrv_create_functinal_sq ret = %d.\n", ret);
        return ret;
    }

    ret = devdrv_create_functional_cq(devid, tsid, DEBUG_MAILBOX_LEN, report_cq, cq_callback, &cq_index, &addr_cq);
    if (ret != IDE_DRV_OK) {
        mailbox_err("devdrv_create_functinal_cq ret = %d.\n", ret);
        devdrv_destroy_functional_sq(devid, tsid, sq_index);
        return ret;
    }

    ret = devdrv_create_functional_cq(devid, tsid, DEBUG_CALLBACK_MAILBOX_LEN, callback_cq, ts_callback,
                                      &ts_cq_index, &addr_ts_cq);
    if (ret != IDE_DRV_OK) {
        mailbox_err("devdrv_create_functinal_cq ret = %d.\n", ret);
        devdrv_destroy_functional_sq(devid, tsid, sq_index);
        devdrv_destroy_functional_cq(devid, tsid, cq_index);
        return ret;
    }

    ret = memset_s(&cqsq_mailbox, sizeof(struct devdrv_mailbox_cqsq),
                   0, sizeof(struct devdrv_mailbox_cqsq));
    if (ret != 0) {
        mailbox_err("memset_s unsuccessful ret= %d.\n", ret);
        devdrv_destroy_functional_sq(devid, tsid, sq_index);
        devdrv_destroy_functional_cq(devid, tsid, cq_index);
        devdrv_destroy_functional_cq(devid, tsid, ts_cq_index);
        return ret;
    }

    cqsq_mailbox.cmd_type = cmd;
    cqsq_mailbox.sq_index = (u16)sq_index;
    cqsq_mailbox.sq_addr = addr_sq;
    cqsq_mailbox.cq0_index = (u16)cq_index;
    cqsq_mailbox.cq0_addr = addr_cq;
    cqsq_mailbox.cq1_index = (u16)ts_cq_index;
    cqsq_mailbox.cq1_addr = addr_ts_cq;

    ret = devdrv_mailbox_send_cqsq(devid, tsid, &cqsq_mailbox);
    if (ret != IDE_DRV_OK) {
        mailbox_err("devdrv_mailbox_send_cqsq ret = %d.\n", ret);
        devdrv_destroy_functional_sq(devid, tsid, sq_index);
        devdrv_destroy_functional_cq(devid, tsid, cq_index);
        devdrv_destroy_functional_cq(devid, tsid, ts_cq_index);

        return ret;
    }

    cqsq->used = DEV_USED;
    cqsq->flag = flag;
    cqsq->sq_handle = sq_index;
    cqsq->cq_handle = cq_index;
    cqsq->ts_handle = ts_cq_index;

    return ret;
}

struct devdrv_cqsq_info
{
    u32 devid;
    int cpuid;
    enum drv_type flag;
    s32 ret;
    struct semaphore sync_sema;
};

s32 devdrv_cqsq_init_dbg_thread(void *para)
{
    struct devdrv_cqsq_info *info = (struct devdrv_cqsq_info *)para;

    info->ret = devdrv_cqsq_init_dbg(info->devid, info->flag);

    up(&info->sync_sema);
    return 0;
}

static int dbg_get_affinity_cpuid(u32 dev_id)
{
#if (!defined CFG_SOC_PLATFORM_CLOUD) && (!defined CFG_SOC_PLATFORM_CLOUD_v2)
    return -1;
#else
    u32 node_per_cpu;
    u32 devnum;

    devnum = (u32)cpu_to_node(num_online_cpus() - 1) + 1;
    node_per_cpu = num_online_cpus() / devnum;
    mailbox_info("Bind to cpu_id. (device_id=%d; cpu_id=%d) \n", dev_id, node_per_cpu * dev_id);

    return node_per_cpu * dev_id;
#endif
}

s32 devdrv_dbg_cqsq_init_entry(u32 devid, enum drv_type flag)
{
    int cpu_id = -1;
    struct task_struct *task = NULL;
    struct devdrv_cqsq_info info;

    info.devid = devid;
    info.cpuid = cpu_id;
    info.flag = flag;
    info.ret = 0;
    sema_init(&info.sync_sema, 0);

    task = kthread_create(devdrv_cqsq_init_dbg_thread, (void *)&info,
                          "dbg_kthread_%d", devid);
    if (IS_ERR(task)) {
        mailbox_err("kthread_run failed, device_id = %d.\n", devid);
        return IDE_DRV_ERROR;
    }

#ifdef DBG_DEVICE
    cpu_id = dbg_get_affinity_cpuid(devid);

    if (cpu_id != -1) {
        kthread_bind(task, cpu_id);
        mailbox_info("kthread_bind: device_id = %d to cpu_id = %d.\n", devid, cpu_id);
    }
#endif
	(void)wake_up_process(task);

    down(&info.sync_sema);

    return info.ret;
}

/* free device cqsq; desory function of cqsq has no return-value */
void devdrv_cqsq_exit(enum drv_type flag)
{
    struct devdrv_mailbox_cqsq cqsq_mailbox;
    struct devdrv_cqsq *cqsq = NULL;
    u32 devid;
    u16 cmd;
    int ret;
    u32 tsid = 0;

    switch (flag) {
        case DRV_DEBUG:
            cmd = DEBUG_CQSQ_RELEASE;
            cqsq = &debug_cqsq[0];
            break;
        default:
            mailbox_err("invalid flag.\n");
            return;
    }

    ret = memset_s(&cqsq_mailbox, sizeof(struct devdrv_mailbox_cqsq),
                   0, sizeof(struct devdrv_mailbox_cqsq));
    if (ret != 0) {
        mailbox_err("memset_s unsuccessful ret= %d.\n", ret);
        return;
    }

    cqsq_mailbox.cmd_type = cmd;

    for (devid = 0; devid < DEV_NUM; devid++) {
        if (cqsq[devid].used == DEV_USED) {
            cqsq_mailbox.sq_index = cqsq[devid].sq_handle;
            cqsq_mailbox.cq0_index = cqsq[devid].cq_handle;
            cqsq_mailbox.cq1_index = cqsq[devid].ts_handle;
            cqsq_mailbox.cq2_index = cqsq[devid].aicpu_handle;

            (void)devdrv_mailbox_send_cqsq(devid, tsid, &cqsq_mailbox);

            devdrv_destroy_functional_sq(devid, tsid, cqsq[devid].sq_handle);
            devdrv_destroy_functional_cq(devid, tsid, cqsq[devid].cq_handle);
            devdrv_destroy_functional_cq(devid, tsid, cqsq[devid].ts_handle);

            cqsq[devid].used = DEV_UNUSED;
        }
    }
}

s32 devdrv_cqsq_send(u32 devid, enum drv_type flag, u8 *buffer)
{
    u32 sq_index;
    s32 ret;
    u32 tsid = 0;

    switch (flag) {
        case DRV_DEBUG:
            sq_index = debug_cqsq[devid].sq_handle;
            break;
        default:
            mailbox_err("invalid flag.\n");
            return IDE_DRV_ERROR;
    }

    ret = devdrv_functional_send_sq(devid, tsid, sq_index, buffer, sizeof(struct ts_cmd));

    return ret;
}

/* 增强的send接口，用于判断power manage */
s32 devdrv_cqsq_send_pm(u32 devid, enum drv_type flag, u8 *buffer)
{
    s32 ret;
    enum prof_pm_type pm_status;

    pm_status = devdrv_get_pm(devid, flag);
    if (pm_status == PM_DISABLE) {
        ret = devdrv_cqsq_send(devid, flag, buffer);
        if (ret != IDE_DRV_OK) {
            mailbox_err("devdrv_mailbox_send_cqsq ret = %d.\n", ret);
            return ret;
        }
    } else if (pm_status == PM_ENABLE) {
        return IDE_DRV_ERROR;
    }

    return IDE_DRV_OK;
}

s32 devdrv_check_cqsq(u32 devid, enum drv_type flag)
{
    struct devdrv_cqsq *cqsq = NULL;
    s32 ret = IDE_DRV_OK;

    switch (flag) {
        case DRV_DEBUG:
            cqsq = &debug_cqsq[devid];
            break;
        default:
            mailbox_err("invalid flag.\n");
            return IDE_DRV_ERROR;
    }

    if (cqsq->used == DEV_UNUSED) {
        ret = devdrv_dbg_cqsq_init_entry(devid, flag);
        if (ret != IDE_DRV_OK) {
            mailbox_err("devdrv_cqsq_init error. devid = %u \n", devid);
            return IDE_DRV_ERROR;
        }
    }

    return ret;
}

enum prof_pm_type devdrv_get_pm(u32 devid, enum drv_type flag)
{
    struct devdrv_cqsq *cqsq = NULL;
    enum prof_pm_type pm_status = PM_DISABLE;

    switch (flag) {
        case DRV_DEBUG:
            cqsq = &debug_cqsq[0];
            break;
        default:
            mailbox_err("invalid flag.\n");
            return PM_DISABLE;
    }

    if (devid < DEV_NUM) {
        mutex_lock(&cqsq[devid].pm_mutex);
        pm_status = (enum prof_pm_type)cqsq[devid].pm_flag;
        mutex_unlock(&cqsq[devid].pm_mutex);
    }

    if (pm_status == PM_DISABLE)
        return PM_DISABLE;
    else
        return PM_ENABLE;
}

int devdrv_dbg_init(void)
{
    sema_init(&ts_wait_sema, 0);
    return IDE_DRV_OK;
}


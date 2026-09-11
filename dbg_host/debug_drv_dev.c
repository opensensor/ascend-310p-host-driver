#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

#include <linux/pvclock_gtod.h>
#include <linux/notifier.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/timekeeper_internal.h>
#include <linux/rtc.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/barrier.h>
#include <linux/delay.h>
#include <linux/version.h>

#include <linux/hugetlb.h>

#include "debug_drv_dev.h"
#include "drv_mailbox.h"

#ifdef UT_TEST
#define STATIC
#else
#define STATIC static
#endif

struct char_device {
    struct class *dev_class;
    struct cdev cdev;
    dev_t devno;
};

STATIC struct char_device char_dev = { 0 };

extern int svm_get_pasid(pid_t vpid, int dev_id);
extern struct devdrv_pm *devdrv_manager_register_pm(int run_stage, int (*suspend)(u32 devid), int (*resume)(u32 devid));
extern int devdrv_manager_unregister_pm(struct devdrv_pm *pm);
extern int devdrv_is_ts_ready(int devid);
extern int devdrv_get_devnum(unsigned int *num_dev);
extern int devdrv_get_devinfo(unsigned int devid, struct devdrv_device_info *info);
#if !defined(TEMP_MINIRC)
extern int hdcdrv_set_debug_mode(int flag);
#endif

extern struct semaphore ts_sema;
extern struct semaphore ts_wait_sema;

extern u32 dev_used[DEV_NUM];
extern u8 ts_callback_buffer[DEBUG_CALLBACK_MAILBOX_LEN];

extern struct devdrv_cqsq debug_cqsq[DEV_NUM];
STATIC struct devdrv_pm *debug_pm = NULL;

/*lint -e508*/
static const struct pci_device_id dbg_driver_tbl[] = {
    { PCI_VDEVICE(HUAWEI, 0Xd100),           0 },
    { PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0 },
    { PCI_VDEVICE(HUAWEI, 0xd801),           0 },
    {}
};
MODULE_DEVICE_TABLE(pci, dbg_driver_tbl);
/*lint +e508*/

STATIC s32 debug_drv_callback(struct debug_drv_info *devdrv_debug_info)
{
    int ret;

    /* ignore return value for cce-dbgproxy's exit(0)
	 */
    ret = down_interruptible(&ts_wait_sema);
    if (ret == -EINTR) {
        ret = IDE_DRV_OK;
    }

    if (ret != IDE_DRV_OK) {
        debug_err("down_interruptible fail, ret= %d.\n", ret);
        return ret;
    }

    ret = memcpy_s(&(devdrv_debug_info->ts_cb_info), sizeof(struct ts2driver_info),
                   ts_callback_buffer, sizeof(struct ts2driver_info));
    if (ret != 0) {
        debug_err("memcpy_s unsuccessful ret= %d.\n", ret);
        return ret;
    }

    return IDE_DRV_OK;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
static pte_t *devmm_pte_offset_map(pmd_t *pmd, unsigned long addr, pmd_t *pmdvalp)
{
    pmd_t pmdval;

    /* rcu_read_lock() to be added later */
    pmdval = pmdp_get_lockless(pmd);
    if (pmdvalp) {
        *pmdvalp = pmdval;
    }
    if (unlikely(pmd_none(pmdval) || is_pmd_migration_entry(pmdval))) {
        goto nomap;
    }
    if (unlikely(pmd_trans_huge(pmdval) || pmd_devmap(pmdval))) {
        goto nomap;
    }
    if (unlikely(pmd_bad(pmdval))) {
        pmd_ERROR(*pmd);
        pmd_clear(pmd);
        goto nomap;
    }
    return __pte_map(&pmdval, addr);

nomap:
    /* rcu_read_unlock() to be added later */
    return NULL;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
STATIC char *debug_devnode(const struct device *dev, umode_t *mode)
#else
STATIC char *debug_devnode(struct device *dev, umode_t *mode)
#endif
{
    if (mode != NULL)
        *mode = 0666;
    return NULL;
}

/* MMU four level */
STATIC u64 debug_drv_va2pa(u64 pid, u64 vaddr)
{
    u64 paddr = 0;
    pgd_t *pgd = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d_t *p4d = NULL;
#endif
    pud_t *pud = NULL;
    pmd_t *pmd = NULL;
    pte_t *pte = NULL;
    struct task_struct *task = NULL;
    struct mm_struct *mm = NULL;
    struct pid *pid_info = NULL;

    pid_info = find_get_pid(pid);
    task = get_pid_task(pid_info, PIDTYPE_PID);
    if (task == NULL) {
        debug_err("get_pid_task error.\n");
        return (EINVAL);
    }

    mm = task->mm;

    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return (EINVAL);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d)) {
        return EINVAL;
    }

    /* if kernel version is above 4.11.0,then 5 level pt arrived.
		pud_offset(pgd,va) changed to pud_offset(p4d,va) for kernel.
		 */
    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        return EINVAL;
    }
#else
    pud = pud_offset(pgd, vaddr);
    if (pud_none(*pud) || pud_bad(*pud))
        return (EINVAL);
#endif

    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return (EINVAL);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    pte = devmm_pte_offset_map(pmd, (uintptr_t)vaddr, NULL);
#else
    pte = pte_offset_map(pmd, (uintptr_t)vaddr);
#endif
    if (pte_none(*pte) || !pte_present(*pte))
        return (EINVAL);

    paddr = PFN_PHYS(pte_pfn(*pte));

    paddr += vaddr & (~PAGE_MASK);

    return paddr;
}

STATIC s32 debug_interface_resume_pc(struct ts_cmd *info)
{
    u64 phy_addr;
    phy_addr = debug_drv_va2pa(info->pid, info->virt_pc);
    if (phy_addr == EINVAL) {
        debug_err ("debug_drv_va2pa error. pid = 0x%pK, virt_addr = 0x%pK\n", (void *)(uintptr_t)info->pid, (void *)(uintptr_t)info->virt_addr);
        return IDE_DRV_ERROR;
    }

    info->phy_addr = phy_addr;
    return IDE_DRV_OK;
}

STATIC s32 debug_interface_set_bp(struct ts_cmd *info)
{
    u64 phy_addr;
    phy_addr = debug_drv_va2pa(info->pid, info->virt_addr);
    if (phy_addr == EINVAL) {
        debug_err("debug_drv_va2pa error.pid = 0x%pK, virt_addr = 0x%pK\n",
                 (void *)(uintptr_t)info->pid, (void *)(uintptr_t)info->virt_addr);
        return IDE_DRV_ERROR;
    }

    info->phy_addr = phy_addr;

#ifdef DBG_DEVICE
    s32 stream_id;
    stream_id = svm_get_pasid(info->pid, info->dev_id);
    if (stream_id < 0) {
        debug_err("svm_get_pasid error. ret = %d\n", stream_id);
        return IDE_DRV_ERROR;
    }
    info->smmu_substream_id = stream_id;
#endif
    return IDE_DRV_OK;
}

STATIC s32 debug_interface_unset_bp(struct ts_cmd *info)
{
#ifdef DBG_DEVICE
    s32 stream_id;

    stream_id = svm_get_pasid(info->pid, info->dev_id);
    if (stream_id < 0) {
        debug_err("svm_get_pasid error. ret = %d\n", stream_id);
        return IDE_DRV_ERROR;
    }
    info->smmu_substream_id = stream_id;
#endif
    return IDE_DRV_OK;
}

STATIC s32 debug_get_devinfo(struct debug_drv_info *devdrv_debug_info)
{
    s32 ret;
    struct devdrv_device_info info;

    /* get device 's info */
    ret = devdrv_get_devinfo(devdrv_debug_info->cmd_info.dev_id, &info);
    if (ret != IDE_DRV_OK) {
        debug_err("devdrv_get_core_num error. ret = %d \n", ret);
        return IDE_DRV_ERROR;
    }

    devdrv_debug_info->ts_report.data.ts_info.dev_info.aicore_num = info.ai_core_num;
    devdrv_debug_info->ts_report.data.ts_info.dev_info.aicpu_num = info.ai_cpu_core_num;
    devdrv_debug_info->ts_report.data.ts_info.dev_info.aicpu_bitmap = info.aicpu_occupy_bitmap;
    /* fill command result */
    devdrv_debug_info->ts_report.cmd_result = IDE_DRV_OK;

    return IDE_DRV_OK;
}

STATIC s32 debug_get_dev_num(struct debug_drv_info *devdrv_debug_info)
{
    s32 ret;
    u32 dev_num;

    ret = devdrv_get_devnum(&dev_num);
    if (ret != IDE_DRV_OK) {
        debug_err("devdrv_get_devnum error: %d\n", ret);
        return IDE_DRV_ERROR;
    }

    devdrv_debug_info->ts_report.data.ts_info.dev_info.dev_num = dev_num;

    /* fill command result */
    devdrv_debug_info->ts_report.cmd_result = IDE_DRV_OK;

    return IDE_DRV_OK;
}

STATIC s32 debug_check_cmd(unsigned int cmd, struct debug_drv_info *devdrv_debug_info)
{
    s32 ret;
    u32 devid;

    devid = devdrv_debug_info->cmd_info.dev_id;
    if (devid >= DEV_NUM) {
        debug_err("devid=%d error.\n", devid);
        return IDE_DRV_ERROR;
    }

    /* the cmd of DEBUG_SET_DEV_STATUS is processed at debug_send_mode() */
    if ((cmd == DEBUG_GET_INFO) || (cmd == DEBUG_GET_DEVNUM) || (cmd == DEBUG_SET_DEV_STATUS) ||
        (cmd == DEBUG_CALLBACK)) {
        ;
    } else {
        ret = down_timeout(&debug_cqsq[devid].sync_wait_sema, TS2DRV_TIMEOUT);
        if (unlikely(ret)) {
            debug_err("down_timeout. please check semaphore. ret = %d, cmd = 0x%x\n", ret, cmd);
            devdrv_debug_info->cmd_info.ts_report->cmd_result = IDE_DRV_TIMEOUT;
            return IDE_DRV_ERROR;
        }
    }

    return IDE_DRV_OK;
}

/* when debug, send command to hdc and TS;
	the cmd of DEBUG_SET_DEV_STATUS is different for cases: ioctl and release */
STATIC s32 debug_send_mode(u32 devid, enum debug_mode flag, enum debug_cmd_type cmd_type)
{
#ifdef DBG_DEVICE
    s32 ret = 0;
#endif
    struct ts_cmd cmd_info = { 0 };
    struct ts_cmd_report ts_report = { 0 };

    cmd_info.cmd_type = DEBUG_SET_DEV_STATUS;
    cmd_info.dev_id = devid;
    cmd_info.dev_flag = flag;

    /* in sq, add ts_report's address */
    cmd_info.ts_report = &ts_report;

    /* temp add for hdcdrv_set_debug_mode() when minirc there is no drv_pcie_hdc.ko */
#if !defined(TEMP_MINIRC)
    hdcdrv_set_debug_mode(flag);
#endif

#ifdef DBG_DEVICE
    /* cmd verify = 0 */
    devdrv_cqsq_send_pm(devid, DRV_DEBUG, (u8 *)&cmd_info);

    /* when ioctl cmd, ignore user's cmd_result */
    ret = down_timeout(&debug_cqsq[devid].sync_wait_sema, TS2DRV_TIMEOUT);
    if (unlikely(ret)) {
        debug_err("release down_timeout. please check semaphore. ret = %d, cmd = 0x%x\n",
                  ret, DEBUG_SET_DEV_STATUS);
        return IDE_DRV_ERROR;
    }
#endif

    return IDE_DRV_OK;
}

STATIC s32 debug_cmd_dispatch(u32 devid, u32 cmd, struct debug_drv_info *devdrv_debug_info)
{
    s32 ret = IDE_DRV_OK;

    if (devdrv_debug_info == NULL) {
        debug_info("devdrv_debug_info is null\n");
        return IDE_DRV_ERROR;
    }

    switch (cmd) {
        case DEBUG_RESUME_PC:
            debug_interface_resume_pc(&devdrv_debug_info->cmd_info);
            devdrv_cqsq_send_pm(devid, DRV_DEBUG, (u8 *)&devdrv_debug_info->cmd_info);
            break;

        case DEBUG_SET_BP:
            debug_interface_set_bp(&devdrv_debug_info->cmd_info);
            devdrv_cqsq_send_pm(devid, DRV_DEBUG, (u8 *)&devdrv_debug_info->cmd_info);
            break;

        case DEBUG_UNSET_BP:
            debug_interface_unset_bp(&devdrv_debug_info->cmd_info);
            devdrv_cqsq_send_pm(devid, DRV_DEBUG, (u8 *)&devdrv_debug_info->cmd_info);
            break;

        /* common command, no more input param */
        case DEBUG_SUSPEND_DEVICE:
        case DEBUG_RESUME_DEVICE:
        case DEBUG_SINGLE_STEP:
        case DEBUG_READ_TASK:
        case DEBUG_READ_CORE:
        case DEBUG_READ_REG:
        case DEBUG_READ_LOCAL:

        case DEBUG_READ_SHARED:
        case DEBUG_READ_GLOBAL:
        case DEBUG_WRITE_REG:
        case DEBUG_WRITE_LOCAL:
        case DEBUG_WRITE_SHARED:
        case DEBUG_WRITE_GLOBAL:
        case DEBUG_EXCEPTION_AICORES:
        case DEBUG_EXCEPTION_REASON:
            devdrv_cqsq_send_pm(devid, DRV_DEBUG, (u8 *)&devdrv_debug_info->cmd_info);
            break;

        case DEBUG_CALLBACK:
            ret = debug_drv_callback(devdrv_debug_info);
            break;

        case DEBUG_GET_INFO:
            debug_get_devinfo(devdrv_debug_info);
            break;

        case DEBUG_GET_DEVNUM:
            debug_get_dev_num(devdrv_debug_info);
            break;

        /* when debug, send command to hdc and TS ; */
        case DEBUG_SET_DEV_STATUS:
            ret = debug_send_mode(devid, devdrv_debug_info->cmd_info.dev_flag, COMMAND_IOCTL);
            break;

        default:
            debug_err("debug ioctl cmd(0x%x) illegal.\n", cmd);
            return IDE_DRV_ERROR;
    }

    return ret;
}

/* the command for TS is a sync cmd */
STATIC long debug_drv_ioctl(struct file *file, unsigned int cmd,
                            unsigned long arg)
{
    static u8 cmd_verify = 0;
    s32 ret = 0;
    u32 devid = 0;
    void __user *parg = NULL;
    /* when sync command, need down_timeout().
	if don't wait sema, need static variable, or report memory is error; */
    struct debug_drv_info devdrv_debug_info;

    parg = (void __user *)(uintptr_t)arg;
    if (parg == NULL) {
        debug_err("user arg error.parg is null \n");
        return IDE_DRV_ERROR;
    }

    ret = memset_s((void *)&devdrv_debug_info, sizeof(struct debug_drv_info),
                   0, sizeof(struct debug_drv_info));
    if (ret != 0) {
        debug_err("memset_s unsuccessful ret= %d.\n", ret);
        return IDE_DRV_ERROR;
    }

    if (copy_from_user(&devdrv_debug_info, parg,
                       sizeof(struct debug_drv_info))) {
        debug_err("copy_from_user error.\n");
        return IDE_DRV_ERROR;
    }

    // check device's cqsq
    devid = devdrv_debug_info.cmd_info.dev_id;
    if (devid >= DEV_NUM) {
        debug_err("devid=%d error.\n", devid);
        return IDE_DRV_ERROR;
    }

    /* sqcq resource are builded at command for debug's online apply;
		and send_mutex is used for multi commands at the same time;
	 */
    mutex_lock(&debug_cqsq[devid].send_mutex);
    ret = devdrv_check_cqsq(devid, DRV_DEBUG);
    if (ret != IDE_DRV_OK) {
        debug_err("devdrv_check_cqsq error, ret = %d, devid = %d\n", ret, devid);
        mutex_unlock(&debug_cqsq[devid].send_mutex);
        return ret;
    }
    mutex_unlock(&debug_cqsq[devid].send_mutex);

    /* in sq, add ts_report's address */
    devdrv_debug_info.cmd_info.ts_report = &(devdrv_debug_info.ts_report);

    /* fill cmd_verify */
    devdrv_debug_info.cmd_info.cmd_verify = cmd_verify;
    cmd_verify++;

    ret = debug_cmd_dispatch(devid, cmd, &devdrv_debug_info);
    if (ret != IDE_DRV_OK) {
        debug_err("debug_cmd_dispatch error, ret = %d, devid = %d, cmd = 0x%x\n", ret, devid, cmd);
        return ret;
    }

    ret = debug_check_cmd(cmd, &devdrv_debug_info);
    if (ret != IDE_DRV_OK) {
        debug_err("debug_check_cmd error, ret = %d, cmd = 0x%x\n", ret, cmd);
        return ret;
    }

    if (copy_to_user(parg, (void *)&devdrv_debug_info,
                     sizeof(struct debug_drv_info)) != 0) {
        debug_err("copy_to_user error.\n");
        return IDE_DRV_ERROR;
    }

    return ret;
}

STATIC s32 debug_suspend_device(u32 devid)
{
    if (devid < DEV_NUM) {
        mutex_lock(&debug_cqsq[devid].pm_mutex);
        debug_cqsq[devid].pm_flag = PM_ENABLE;
        mutex_unlock(&debug_cqsq[devid].pm_mutex);
    }

    return IDE_DRV_OK;
}

STATIC s32 debug_resume_device(u32 devid)
{
    if (devid < DEV_NUM) {
        mutex_lock(&debug_cqsq[devid].pm_mutex);
        debug_cqsq[devid].pm_flag = PM_DISABLE;
        mutex_unlock(&debug_cqsq[devid].pm_mutex);
    }

    return IDE_DRV_OK;
}

/* init resources for driver */
STATIC s32 debug_init(void)
{
    u32 devid = 0;
    s32 ret;

    for (devid = 0; devid < DEV_NUM; devid++) {
        mutex_init(&debug_cqsq[devid].send_mutex);
        mutex_init(&debug_cqsq[devid].pm_mutex);
    }

    debug_pm = devdrv_manager_register_pm(0, debug_suspend_device, debug_resume_device);
    if (debug_pm == NULL) {
        debug_err("devdrv_manager_register_pm error\n");
        return IDE_DRV_ERROR;
    }

    ret = devdrv_dbg_init();
    if (ret != IDE_DRV_OK) {
        debug_err("devdrv_dbg_init error %d\n", ret);
        return ret;
    }

    return IDE_DRV_OK;
}

STATIC s32 debug_uninit(void)
{
    /* free device cqsq */
    devdrv_cqsq_exit(DRV_DEBUG);

    if (debug_pm != NULL) {
        devdrv_manager_unregister_pm(debug_pm);
        debug_pm = NULL;
    }

    return IDE_DRV_OK;
}

#ifdef __aarch64__
void log_flush_cache_dbg(u64 base, u32 len)
{
    int i, l = len / LOG_CACHE_LINE;
    if (len % LOG_CACHE_LINE != 0) {
        l += 1;
    }

    asm volatile("dsb st" : : : "memory");
    for (i = 0; i < l; i++) {
        asm volatile("DC CIVAC ,%x0" ::"r"(base + i * LOG_CACHE_LINE));
        mb();
    }
    asm volatile("dsb st" : : : "memory");
}
#endif

STATIC long ide_drv_ioctl(struct file *file, unsigned int cmd,
                          unsigned long arg)
{
    int ret = 0;
    u32 drv_cmd;
    enum drv_type type = DRV_TYPE_MAX;

    drv_cmd = _IOC_NR(cmd);
    if (drv_cmd < DEBUG_SUSPEND_DEVICE) {
        type = DRV_LOG;
    } else if (drv_cmd < DEBUG_PLATFORM) {
        type = DRV_DEBUG;
    } else if (drv_cmd < DEBUG_CMD_MAX) {
        type = DRV_PROFILE;
    }

    switch (type) {
        case DRV_LOG:
            break;
        case DRV_DEBUG:
            ret = debug_drv_ioctl(file, drv_cmd, arg);
            break;
        case DRV_PROFILE:
            break;
        default:
            debug_err("driver type is illegal(%d)\n", type);
            return IDE_DRV_ERROR;
    }

    return ret;
}

STATIC int dbg_drv_release(struct inode *inode, struct file *filp)
{
    u32 devid;

    for (devid = 0; devid < DEV_NUM; devid++) {
        if (debug_cqsq[devid].used == DEV_USED) {
            (void)debug_send_mode(devid, DEBUG_MODE_DISABLE, COMMAND_RELEASE);
        }
    }

    return IDE_DRV_OK;
}

STATIC const struct file_operations dbg_drv_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = ide_drv_ioctl,
    .release = dbg_drv_release,
};

STATIC int dbg_drv_register_cdev(void)
{
    int ret;
    unsigned int major_dev;
    dev_t devno;

    struct char_device *priv = (struct char_device *)&char_dev;

    priv->devno = 0;
    ret = alloc_chrdev_region(&priv->devno, 0, 1, CHAR_DRIVER_NAME);
    if (ret < 0)
        return IDE_DRV_ERROR;

    /* init and add char device */
    major_dev = MAJOR(priv->devno);
    devno = MKDEV(major_dev, 0);
    cdev_init(&priv->cdev, &dbg_drv_fops);
    priv->cdev.owner = THIS_MODULE;
    priv->cdev.ops = &dbg_drv_fops;

    if (cdev_add(&priv->cdev, devno, 1)) {
        unregister_chrdev_region(devno, 1);
        return IDE_DRV_ERROR;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    priv->dev_class = class_create(CHAR_DRIVER_NAME);
#else
    priv->dev_class = class_create(THIS_MODULE, CHAR_DRIVER_NAME);
#endif
    if (IS_ERR(priv->dev_class)) {
        (void)unregister_chrdev_region(devno, 1);
        (void)cdev_del(&priv->cdev);
        return IDE_DRV_ERROR;
    }
    if (priv->dev_class != NULL) {
        priv->dev_class->devnode = debug_devnode;
    }
    (void)device_create(priv->dev_class, NULL, devno, NULL,
                        CHAR_DRIVER_NAME);

    return IDE_DRV_OK;
}

STATIC void dbg_drv_free_cdev(void)
{
    struct char_device *priv = &char_dev;

    (void)device_destroy(priv->dev_class, priv->devno);
    (void)class_destroy(priv->dev_class);
    (void)unregister_chrdev_region(priv->devno, 1);
    (void)cdev_del(&priv->cdev);
}

int __init dbg_drv_module_init(void)
{
    int ret;

    ret = dbg_drv_register_cdev();
    if (ret != IDE_DRV_OK) {
        debug_err("create character device fail.\n");
        return IDE_DRV_ERROR;
    }

    ret = debug_init();
    if (ret) {
        debug_err("debug module init failed!\n");

        dbg_drv_free_cdev();
        return IDE_DRV_ERROR;
    }

    debug_info("dbg_drv load ok.\n");
    return IDE_DRV_OK;
}

void __exit dbg_drv_module_exit(void)
{
    (void)debug_uninit();

    /* char device must be the last */
    dbg_drv_free_cdev();
}

module_init(dbg_drv_module_init);
module_exit(dbg_drv_module_exit);

MODULE_DESCRIPTION("dbg driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");


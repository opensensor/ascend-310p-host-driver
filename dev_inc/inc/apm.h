/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2024-03-15
 */

#ifndef APM_H
#define APM_H

#include <linux/types.h>
#include <linux/module.h>
#include <linux/notifier.h>

#include "ascend_hal_define.h"

/************************ task group *****************************/
static inline int apm_trans_proc_type_from_bitmap(unsigned int proc_type_bitmap)
{
    int proc_type;

    for (proc_type = 0; proc_type < PROCESS_CPTYPE_MAX; proc_type++) {
        if ((proc_type_bitmap & (0x1 << proc_type)) != 0) {
            break;
        }
    }

    return proc_type;
}

int apm_query_slave_tgid_by_master(int master_tgid, u32 udevid, processType_t proc_type, int *slave_tgid);
int apm_query_master_info_by_slave(int slave_tgid, int *master_tgid, u32 *udevid, int *mode, u32 *proc_type_bitmap);
static inline int apm_query_master_tgid_by_slave(int slave_tgid, int *master_tgid)
{
    u32 udevid, proc_type_bitmap;
    int mode;

    return apm_query_master_info_by_slave(slave_tgid, master_tgid, &udevid, &mode, &proc_type_bitmap);
}

static inline int apm_query_proc_type_bitmap_by_slave(int slave_tgid, u32 *proc_type_bitmap)
{
    u32 udevid;
    int master_tgid, mode;

    return apm_query_master_info_by_slave(slave_tgid, &master_tgid, &udevid, &mode, proc_type_bitmap);
}

static inline int apm_query_proc_type_by_slave(int slave_tgid, processType_t *proc_type)
{
    u32 proc_type_bitmap;
    int ret = apm_query_proc_type_bitmap_by_slave(slave_tgid, &proc_type_bitmap);
    if (ret == 0) {
        *proc_type = (processType_t)apm_trans_proc_type_from_bitmap(proc_type_bitmap);
    }

    return ret;
}

/* call on device side, only surport user proc */
int devdrv_query_master_pid_by_host_slave(int slave_pid, u32 *master_pid);

/* call on host side, only surport cp proc */
int devdrv_query_master_pid_by_device_slave(u32 udevid, int slave_pid, u32 *master_pid);

/************************ task exit *****************************/
/* apm task exit control sequence:
   1: master or slave do exit, in this stage module can recycle self resouce
   2: slave(cp) stop stream
   3: master stop stream
   4: slave(cp) recycle resource
   5: master recycle resource
   n->notifier_call: int exit_notify(struct notifier_block *self, unsigned long val, void *data)
                     val : stage+pid, data : null
   n->priority: 0, high pri */
enum apm_exit_stage {
    APM_STAGE_DO_EXIT = 1,
    APM_STAGE_STOP_STREAM,
    APM_STAGE_RECYCLE_RES,
    APM_STAGE_MAX
};

#define APM_STAGE_OFFSET 32
static inline int apm_get_exit_stage(unsigned long val)
{
    return (int)((val >> APM_STAGE_OFFSET) & 0xFFFFFFFF);
}

static inline int apm_get_exit_tgid(unsigned long val)
{
    return (int)((val) & 0xFFFFFFFF);
}

static inline unsigned long apm_get_exit_val(int stage, int tgid)
{
    return (((unsigned long)(stage) << APM_STAGE_OFFSET) | ((tgid) & 0xFFFFFFFF));
}

int apm_master_exit_register(struct notifier_block *n);
void apm_master_exit_unregister(struct notifier_block *n);
int apm_slave_exit_register(struct notifier_block *n);
void apm_slave_exit_unregister(struct notifier_block *n);
int apm_remote_master_exit_register(struct notifier_block *n);
void apm_remote_master_exit_unregister(struct notifier_block *n);

/************************ resouce map *****************************/
/*
    ep: master in host
        host master res map: host curent master task check, device get res addr
    ep/rc: master in device
        device master res map: device curent master task check, device get res addr
        device slave res map: device curent slave task check, device current get res addr
*/
#define APM_RES_MAP_MAX_PA_NUM 16 /* suport 16 * 4k = 64k mem map */
struct apm_res_map_ops {
    struct module *owner;
    bool (*res_is_belong_to_proc)(int master_tgid, int slave_tgid, u32 udevid, struct res_addr_info *res_info);
    int (*get_res_addr)(u32 udevid, struct res_addr_info *res_info, u64 *pa, u32 *len);
    /* res is mem, may have multy pages, every pa len is one page, len res total len, must page align */
    int (*get_res_addr_array)(u32 udevid, struct res_addr_info *res_info, u64 pa[], u32 num, u32 *len);
};

int apm_res_map_ops_register(enum res_addr_type res_type, struct apm_res_map_ops *ops);
void apm_res_map_ops_unregister(enum res_addr_type res_type);

int apm_res_addr_map(u32 udevid, struct res_addr_info *res_info, u64 *va, u32 *len);
int apm_res_addr_unmap(u32 udevid, struct res_addr_info *res_info);
int apm_res_addr_query(u32 udevid, struct res_addr_info *res_info, u64 *va, u32 *len);

/* stub */
enum devdrv_process_type {
    DEVDRV_PROCESS_CP1 = 0,   /* aicpu_scheduler */
    DEVDRV_PROCESS_CP2,       /* custom_process */
    DEVDRV_PROCESS_DEV_ONLY,  /* TDT */
    DEVDRV_PROCESS_QS,        /* queue_scheduler */
    DEVDRV_PROCESS_HCCP,      /* hccp server */
    DEVDRV_PROCESS_USER,      /* user proc, can bind many on host or device. */
    DEVDRV_PROCESS_CPTYPE_MAX
};

typedef struct {
    unsigned int vaild_num;
    unsigned int chip_id[DEVDRV_PROCESS_CPTYPE_MAX];
    unsigned int vfid[DEVDRV_PROCESS_CPTYPE_MAX];
    pid_t host_pids[DEVDRV_PROCESS_CPTYPE_MAX];
    unsigned int cp_type[DEVDRV_PROCESS_CPTYPE_MAX];
} devdrv_host_pids_info_t;

int devdrv_query_process_by_host_pid(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid);
int devdrv_query_process_by_host_pid_kernel(unsigned int host_pid,
    unsigned int chip_id, enum devdrv_process_type cp_type, unsigned int vfid, int *pid);
int devdrv_query_process_host_pid(int pid, unsigned int *chip_id, unsigned int *vfid, unsigned int *host_pid,
    enum devdrv_process_type *cp_type);
/* multi identify bind */
int devdrv_query_process_host_pids_by_pid(int pid, devdrv_host_pids_info_t *host_pids_info);

/* not surport */
int devdrv_check_hostpid(pid_t hostpid, unsigned int chip_id, unsigned int vfid); /* svm/hdc use */
int devdrv_check_sign(pid_t hostpid, const char *sign, u32 len); /* svm use */

#endif


/*************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
 * Description: dms_pid_map.h
 * FileName: dms_pid_map.h
 * Author: huawei
 * Created Time: Tue 09 Aug 2022 10:22:19 AM CST
 ************************************************************************/

/**
 * brief description about this document.
 * points to focus on.
 */

#ifndef _DMS_PID_MAP_H
#define _DMS_PID_MAP_H

#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include "devdrv_manager_comm.h"


#define DMS_PID_INVALID (-1)
#define DMS_PID_MAP_UPDATE_UNBIND 0
#define DMS_PID_MAP_UPDATE_BIND 1
#define DMS_PID_MAP_OFFLINE_MODE 1

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

typedef struct {
    int host_pid;
    int devpid[DEVDRV_PROCESS_CPTYPE_MAX];
    struct hlist_node node;
} pid_map_t;

#define PROCESS_SIGN_LENGTH  49
#define PROCESS_RESV_LENGTH  4
struct dev_process_sign {
    int tgid;
    char sign[PROCESS_SIGN_LENGTH];
    char resv[PROCESS_RESV_LENGTH];
};

void dms_pid_map_init(void);
void dms_pid_map_exit(void);
int dms_drv_bind_host_pid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_drv_unbind_host_pid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_drv_query_devpid_by_hostpid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_drv_query_hostpid_by_devpid(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_drv_get_process_sign(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
void dms_pidmap_release_process_id(pid_t pid);
int dms_get_hccl_device_info(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_get_core_spec_info(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_get_core_inuse_info(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);

#endif


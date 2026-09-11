/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2020-01-15
 */
#ifndef HWTS_DRV_TASK_INFO_H
#define HWTS_DRV_TASK_INFO_H

struct hwts_ts_kernel {
    pid_t pid;
    unsigned short kernel_type : 8;
    unsigned short batchMode : 1; // default 0
    unsigned short satMode : 1;
    unsigned short rspMode : 1;
    unsigned short resv : 5;
    unsigned short streamID;
    unsigned long long kernelName;
    unsigned long long kernelSo;
    unsigned long long paramBase;
    unsigned long long l2VaddrBase;
    unsigned long long l2Ctrl;
    unsigned short blockId;
    unsigned short blockNum;
    unsigned int l2InMain;
    unsigned long long taskID;
};

/* Not allow hwts_ts_task to be parameter passing in aicpufw and drv */
struct hwts_ts_task {
    unsigned int mailbox_id;
    volatile unsigned long long serial_no;
    struct hwts_ts_kernel kernel_info;
};

typedef enum hwts_task_status {
    TASK_SUCC = 0,
    TASK_FAIL = 1,
    TASK_OVERFLOW = 2,
    TASK_STATUS_MAX,
} HWTS_TASK_STATUS;


#define HWTS_RESPONSE_RSV   3
struct hwts_response {
    unsigned int result;
    unsigned int mailbox_id;
    unsigned long long serial_no;
    unsigned int status;
    int rsv[HWTS_RESPONSE_RSV];
    char* msg;
    int len;
};

#endif


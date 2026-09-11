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

#ifndef XSMEM_CMD_H
#define XSMEM_CMD_H

#include "ascend_hal_define.h"
#ifdef __KERNEL__
#include "ka_ioctl_pub.h"
#else
#include <sys/ioctl.h>
#define _KA_IOW  _IOW
#define _KA_IO   _IO
#define _KA_IOWR _IOWR
#endif

#define XSMEM_BLOCK_MAX_NUM       262144  /* 512 * 512 */

#define XSHM_KEY_SIZE 128U

#define BUFF_NOCACHE 0U
#define BUFF_CACHE 1U

struct xsm_reg_arg {
    int algo;
    unsigned int priv_flag;
    unsigned long pool_size;
    unsigned int block_size; /* used for blk algorithm */
    unsigned int key_len;
    char *key;
};

struct xsm_cache_create_arg {
    int poolId;
    unsigned int devId;
    unsigned int memFlag;
    unsigned long long memSize;
    unsigned long long alloc_max_size;
};

struct xsm_cache_destroy_arg {
    int poolId;
    unsigned int devId;
};

struct xsm_block_alloc_arg {
    unsigned long flag; /* input */
    unsigned long size; /* input */
    unsigned long offset; /* output */
    unsigned int blkid;
};

struct xsm_block_free_arg { /* for free and put */
    unsigned long offset; /* input */
};

struct xsm_block_get_arg {
    unsigned long offset; /* input, offset in the range of alloc offset */
    unsigned long alloc_size; /* output */
    unsigned long alloc_offset; /* output */
    unsigned int blkid;
};

struct xsm_block_arg {
    int pool_id; /* get: output, others: input */
    union {
        struct xsm_block_alloc_arg alloc;
        struct xsm_block_free_arg free;
        struct xsm_block_get_arg get;
    };
};

struct xsm_task_attach_arg {
    int pool_id;
    int timeout;
    unsigned long long uid;
    unsigned int cache_type;
};

struct xsm_task_arg {
    int pool_id;
    int pid;
    GroupShareAttr attr;
};

struct xsm_pool_query_arg {
    char *key;
    unsigned int key_len;
    int pool_id;
};

struct xsm_query_pool_task_arg {
    int pool_id;
    int *pid;
    int pid_num; /* intput, output  */
};

struct xsm_query_cache_arg {
    int poolId;                     /* intput */
    unsigned int devId;             /* intput */
    unsigned int cache_cnt;         /* intput, output */
    GrpQueryGroupAddrInfo *cache_buff;  /* output  */
};

struct xsm_query_pool_flag_arg {
    int pool_id;                    /* intput */
    unsigned int priv_flag;         /* output */
};

struct xsm_check_va_arg {
    unsigned long va;               /* intput */
    int pool_id;                    /* intput */
    int result;                    /* output */
};

#define XSMEM_TASK_IN_POOL 0
#define XSMEM_TASK_ADDING_TO_POOL 1

struct xsm_query_task_pool_arg {
    union {
        int type;
        int query_num;
    };
    int pid;
    int *pool_id;
    int pool_num; /* intput, output  */
};

#define XSMEM_PROP_OP_SET 0
#define XSMEM_PROP_OP_GET 1
#define XSMEM_PROP_OP_DEL 2

#define XSMEM_PROP_OWNER_TASK 0
#define XSMEM_PROP_OWNER_TASK_GRP 1

struct xsm_prop_op_arg {
    char *prop;
    unsigned int prop_len;
    int owner;
    int pool_id;
    int op;
    unsigned long value;
};

struct xsm_poll_exit_task_arg {
    int pool_id;
    unsigned long long uid;
};

enum xsm_share_mem_type {
    SHARE_MEM_TYPE_BUF = 0,
    SHARE_MEM_TYPE_MAX
};

enum xsm_usr_cfg_type {
    XSM_USR_CFG_MBUF_TIME_OUT = 0,
    XSM_USR_CFG_MAX
};

#define XSMEM_POOL_REGISTER         _KA_IOW('X', 1, struct xsm_reg_arg)
#define XSMEM_POOL_UNREGISTER       _KA_IO('X', 2)
#define XSMEM_POOL_TASK_ADD         _KA_IOW('X', 3, struct xsm_task_arg)
#define XSMEM_POOL_TASK_DEL         _KA_IOW('X', 4, struct xsm_task_arg)
#define XSMEM_POOL_ATTACH           _KA_IOWR('X', 5, struct xsm_task_attach_arg)
#define XSMEM_POOL_DETACH           _KA_IO('X', 6)
#define XSMEM_BLOCK_ALLOC           _KA_IOWR('X', 7, struct xsm_block_arg)
#define XSMEM_BLOCK_FREE            _KA_IOW('X', 8, struct xsm_block_arg)
#define XSMEM_BLOCK_GET             _KA_IOWR('X', 9, struct xsm_block_arg)
#define XSMEM_BLOCK_PUT             _KA_IOW('X', 10, struct xsm_block_arg)
#define XSMEM_POOL_ID_QUERY         _KA_IOWR('X', 11, struct xsm_pool_query_arg)
#define XSMEM_POOL_NAME_QUERY       _KA_IOWR('X', 12, struct xsm_pool_query_arg)
#define XSMEM_POOL_TASK_QUERY       _KA_IOWR('X', 13, struct xsm_query_pool_task_arg)
#define XSMEM_POOL_TASK_ATTR_QUERY  _KA_IOWR('X', 14, struct xsm_task_arg)
#define XSMEM_TASK_POOL_QUERY       _KA_IOWR('X', 15, struct xsm_query_task_pool_arg)
#define XSMEM_PROP_OP               _KA_IOWR('X', 16, struct xsm_prop_op_arg)
#define XSMEM_POLL_EXIT_TASK        _KA_IOWR('X', 17, struct xsm_poll_exit_task_arg)
#define XSMEM_CACHE_CREATE          _KA_IOW('X', 18, struct xsm_cache_create_arg)
#define XSMEM_CACHE_DESTROY         _KA_IOW('X', 19, struct xsm_cache_destroy_arg)
#define XSMEM_CACHE_QUERY           _KA_IOWR('X', 20, struct xsm_query_cache_arg)
#define XSMEM_POOL_FLAG_QUERY       _KA_IOWR('X', 21, struct xsm_query_pool_flag_arg)
#define XSMEM_VADDR_CHECK           _KA_IOWR('X', 22, struct xsm_check_va_arg)

#define XSMEM_MAX_CMD        23

#define XSMEM_ALGO_EMPTY        (-1)
#define XSMEM_ALGO_VMA          0
#define XSMEM_ALGO_SP           1
#define XSMEM_ALGO_CACHE_VMA    2
#define XSMEM_ALGO_CACHE_SP     3
#define XSMEM_ALGO_MAX          4

#ifndef EMU_ST
#define TASK_BLK_MAX_GET_NUM (32 * 1024)
#else
#define TASK_BLK_MAX_GET_NUM (1024)
#endif
#define DAVINCI_XSMEM_SUB_MODULE_NAME "XSMEM"

#endif

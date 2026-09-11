/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description:
 * Author: huawei
 * Create: 2025-06-10
 */
#ifndef BUFF_MANAGE_BASE_H
#define BUFF_MANAGE_BASE_H

#include "ascend_hal_define.h"

#ifdef __cplusplus
extern "C" {
#endif

drvError_t buffPoolAlgoVmaRegister(const char *name, unsigned long poolSize,
    unsigned int privMbuflfag, int *poolId);
drvError_t buffPoolAlgoSpRegister(const char *name, unsigned long poolSize,
    unsigned int privMbuflfag, int *poolId);
drvError_t buffPoolAlgoCacheVmaRegister(const char *name, unsigned long poolSize,
    unsigned int privMbuflfag, int *poolId);
drvError_t buffPoolAlgoCacheSpRegister(const char *name, unsigned long poolSize,
    unsigned int privMbuflfag, int *poolId);
drvError_t buffPoolUnregister(int poolId);
drvError_t buffPoolCacheCreate(int poolId, unsigned int devId, unsigned int memFlag,
    unsigned long long memSize, unsigned long long alloc_max_size);
drvError_t buffPoolCacheDestroy(int poolId, unsigned int devId);
drvError_t buffPoolAddProc(int poolId, int pid, GroupShareAttr attr);
drvError_t buffPoolDelProc(int poolId, int pid);
drvError_t buffPoolAttach(int poolId, int timeout);
drvError_t buffPoolAttachEx(int poolId, int timeout, unsigned long long *procUid, unsigned int *cacheType);
drvError_t buffPoolDetach(int poolId);
drvError_t buffPoolBlkAlloc(int poolId, unsigned long size, unsigned long flag, unsigned long *ptr, uint32_t *blkId);
drvError_t buffPoolBlkFree(int poolId, unsigned long ptr);
drvError_t buffPoolBlkGet(unsigned long ptr, int *poolId, unsigned long *alloc_ptr,
    unsigned long *alloc_size, uint32_t *blkId);
drvError_t buffPoolBlkPut(int poolId, unsigned long ptr);

drvError_t buffPoolIdQuery(const char *name, int *poolId);
drvError_t buffPoolNameQuery(int poolId, char *name, int nameLen);
drvError_t buffCacheInfoQuery(int poolId, unsigned int devId, GrpQueryGroupAddrInfo *cache_buff,
    unsigned int cache_cnt, unsigned int *query_cnt);
drvError_t buffPoolPrivFlagQuery(int poolId, unsigned int *flag);
drvError_t buffPoolVaCheck(int poolId, unsigned long va, int *result);
drvError_t buffPoolTaskQuery(int poolId, int *pid, int pidNum, int *queryNum);
drvError_t buffPoolTaskAttrQuery(int poolId, int pid, GroupShareAttr *attr);
drvError_t buffTaskPoolQuery(int pid, int *poolId, int poolNum, int *queryNum);
drvError_t buffTaskAddingPoolQuery(int pid, int *poolId, int poolNum, int *queryNum);

drvError_t buffPoolSetProp(int poolId, const char *propName, unsigned long value);
drvError_t buffPoolSetGrpProp(int poolId, const char *propName, unsigned long value);
drvError_t buffPoolGetProp(int poolId, const char *propName, unsigned long *value);
drvError_t buffPoolDelProp(int poolId, const char *propName);
int buffPoolPollExitTask(int poolId, unsigned long long *procUid);

#ifdef __cplusplus
}
#endif
#endif

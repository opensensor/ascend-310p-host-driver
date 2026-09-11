/**
 * @file ascend_hal.h
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description:
 * Author: huawei
 * Create: 2020-01-21
 * @brief driver interface.
 * @version 1.0
 *
 */


#ifndef ASCEND_EXTERNAL_H
#define ASCEND_EXTERNAL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

#ifndef __linux
#include "mmpa_api.h"
#define ASCEND_HAL_WEAK
#else
#define ASCEND_HAL_WEAK __attribute__((weak))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "ascend_hal_define.h"

#ifdef __linux
#define DLLEXPORT
#else
#define DLLEXPORT _declspec(dllexport)
#ifndef pid_t
typedef int pid_t;
#endif
#endif

/**
* @ingroup driver
* @brief set the state of progress
* @attention null
* @param [in]  int index : which index you want to set(0-1022)
* @param [in]  int value : the valve you want to set
* @return DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t halCentreNotifySet(int index, int value);
/**
* @ingroup driver
* @brief get the state of progress
* @attention null
* @param [in]  int index : which index you want to get(0-1023)
* @param [out] int *value  : the valve you want to get
* @return DRV_ERROR_NONE, DRV_ERROR_INVALID_VALUE
*/
DLLEXPORT drvError_t halCentreNotifyGet(int index, int *value);

/**
 * @ingroup driver
 * @brief Buffer MACRO
 */
#define BUFF_SP_NORMAL 0
#define BUFF_SP_HUGEPAGE_PRIOR (1 << 0)
#define BUFF_SP_HUGEPAGE_ONLY (1 << 1)
#define BUFF_SP_DVPP (1 << 2)
#define BUFF_SP_SVM (1 << 3)

/**
 * @ingroup driver
 * @brief Buffer Pool Statistics.
 */
struct buffPoolStat {
    pid_t owner;                         /**< pool create pid */
    unsigned int align;                  /**< pool addr align */
    unsigned int status;                 /**< pool status: normal 0, destroyed 1 */
    unsigned int totalBlkNum;            /**< total block num in pool */
    unsigned int freeBlkNum;             /**< free block num in pool */
    unsigned int blkSize;                /**< block size */
    unsigned int overTimeValue;          /**< overtime value of block in pool */
    unsigned int overTimeBlkNumSum;      /**< overtime block num accumulative total */
    unsigned int overTimeBlkNumCur;      /**< overtime block num current using */
    unsigned int maxBusyBlkNum;          /**< max block num in use */
    unsigned long maxBusyTime;           /**< max using time of block in pool */
    unsigned long allocFailCount;        /**< alloc fail num of block in pool */
    int reserve[BUFF_RESERVE_LEN];       /**< for reserve */
};

struct buff_cfg {
    unsigned int cfg_id;         /**< cfg id, 0~7 */
    unsigned int total_size;  /**< memzone total size, must below 256M */
    unsigned int blk_size;       /**< the size of each blk in this memzone  */
    unsigned int max_buf_size;   /**< max buff size could allocte, */
    unsigned int page_type;      /**< page type of memzone, could be PAGE_NORMAL or PAGE_HUGET_ONLY */
    int reserve[BUFF_RESERVE_LEN];   /**< for reserve */
};

/**
 * @ingroup driver
 * @brief Buffer Mbuf info.
 */
typedef struct mempool_t* poolHandle;//lint !e565
typedef struct Mbuf Mbuf;//lint !e565
typedef struct mp_attr {
    int devid;
    int mGroupId;
    unsigned int blkSize;
    unsigned int blkNum;        /**< if blkNum*blkSize is more than the size of os memory may return fail*/
    unsigned int align;         /**< must be power of 2 */
    unsigned long hugePageFlag; /**< huge page support */
    char poolName[BUFF_POOL_NAME_LEN];
    int reserve[BUFF_RESERVE_LEN];
}mpAttr;

struct memzone_buff_cfg {
    unsigned int num;
    struct buff_cfg *cfg_info;
};

/**
* @ingroup driver
* @brief free buff addr
* @attention null
* @param [in] *buff: buff addr to free
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffFree(void *buff);
/**
* @ingroup driver
* @brief create mempool
* @attention null
* @param [in] struct mp_attr *attr: mempool config info
* @param [out] struct mempool_t **mp: mempool alloced
* @return   0 for success, others for fail
*/

DLLEXPORT int halBuffCreatePool(struct mp_attr *attr, struct mempool_t **mp);
/**
* @ingroup driver
* @brief delete mempool
* @attention set the pointer mp to null after delete mempool
* @param [in] struct mempool_t *mp: mempool to delete
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffDeletePool(struct mempool_t *mp);

/**
* @ingroup driver
* @brief Mbuf alloc interface
* @attention null
* @param [in] unsigned int size: size of Mbuf to alloc
* @param [out] Mbuf **mbuf: Mbuf alloced
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufAlloc(uint64_t size, Mbuf **mbuf) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief Mbuf alloc from Pool interface
* @attention null
* @param [in] poolHandle pHandle: pool handle
* @param [out] Mbuf **mbuf: Mbuf alloced
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufAllocByPool(poolHandle pHandle, Mbuf **mbuf);

/**
* @ingroup driver
* @brief Mbuf free interface
* @attention null
* @param [in] Mbuf *mbuf: Mbuf to free
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufFree(Mbuf *mbuf) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief get Data addr of Mbuf
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [out] void **buf: Mbuf data addr
* @param [out] unsigned int *size: size of the Mbuf data
* @return   0 for success, others for fail
*/
// Deprecated by 2021-09-17
DLLEXPORT int halMbufGetDataPtr(Mbuf *mbuf, void **buf, uint64_t *size);

/**
* @ingroup driver
* @brief get Data addr of Mbuf
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [out] void **buf: Mbuf data addr
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufGetBuffAddr(Mbuf *mbuf, void **buf) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief get total Buffer size of Mbuf
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [out] unsigned int *totalSize: total buffer size of Mbuf
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufGetBuffSize(Mbuf *mbuf, uint64_t *totalSize) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief set Data len of Mbuf
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [in] unsigned int len: data len
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufSetDataLen(Mbuf *mbuf, uint64_t len);
/**
* @ingroup driver
* @brief get Data len of Mbuf
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [out] unsigned int *len: len of the Mbuf data
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufGetDataLen(Mbuf *mbuf, uint64_t *len);
/**
* @ingroup driver
* @brief Apply for a Mbuf on the shared pool, and then assign the data area of
*        the source Mbuf to the newly applied Mbuf data area
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [out] Mbuf **newMbuf: new Mbuf addr
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufCopyRef(Mbuf *mbuf, Mbuf **newMbuf);

/**
* @ingroup driver
* @brief Get the address and length of its user_data from the specified Mbuf
* @attention null
* @param [in] Mbuf *mbuf: Mbuf addr
* @param [out] void **priv: address of its user_data
* @param [out]  unsigned int *size: length of its user_data
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufGetPrivInfo (Mbuf *mbuf,  void **priv, unsigned int *size) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief append mbuf to mbuf chain
* @attention null
* @param [inout] mbufChainHead, the mbuf chain head
* @param [out] mbuf, the mbuf to append
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufChainAppend(Mbuf *mbufChainHead, Mbuf *mbuf);

/**
* @ingroup driver
* @brief get mbuf num in mbuf chain
* @attention null
* @param [in] mbufChainHead, the mbuf chain head to free
* @param [out] num, the mbuf num
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufChainGetMbufNum(Mbuf *mbufChainHead, unsigned int *num);

/**
* @ingroup driver
* @brief get mbuf num in mbuf chain
* @attention null
* @param [in] mbufChainHead, the mbuf chain head to free
* @param [in] index, the mbuf index which to get in chain
* @param [out] mbuf, the mbuf to get
* @return   0 for success, others for fail
*/
DLLEXPORT int halMbufChainGetMbuf(Mbuf *mbufChainHead, unsigned int index, Mbuf **mbuf);

/**
* @ingroup driver
* @brief create proc grp
* @attention null
* @param [in] name, grp name
* @param [in] cfg, grp cfg
* @return   0 for success, others for fail
*/
DLLEXPORT int halGrpCreate(const char *name, GroupCfg *cfg) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief add process to grp
* @attention null
* @param [in] name, grp name
* @param [in] pid, process id
* @param [in] attr, process permission in grp
* @return   0 for success, others for fail
*/
DLLEXPORT int halGrpAddProc(const char *name, int pid, GroupShareAttr attr) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief attach proccess to check permission in grp
* @attention null
* @param [in] name, grp name
* @param [in] timeout, time out ms
* @return   0 for success, others for fail
*/
DLLEXPORT int halGrpAttach(const char *name, int timeout) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief buff init
* @attention null
* @param [in] cfg, init cfg
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffInit(BuffCfg *cfg) ASCEND_HAL_WEAK;

/**
 * @ingroup driver
 * @brief buffer config
 * @attention null
 * @param [in] enum BuffSetCmdType cmd, set cmd
 * @param [in] void*  data, data to set
 * @param [in] uint32 len, len of data
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT int halBuffCfg(enum BuffConfCmdType cmd, void *data, unsigned int len);

/**
 * @ingroup driver
 * @brief buffer get info
 * @attention null
 * @param [in] enum BuffGetCmdType cmd, get cmd
 * @param [in] void*  inBuff, para for get
               cmd is BUFF_GET_MBUF_TIMEOUT_INFO, inBuff can set null
               cmd is BUFF_GET_MBUF_USE_INFO, inBuff set Mbuf **
               cmd is BUFF_GET_MBUF_TYPE_INFO, inBuff set Mbuf **
               cmd is BUFF_GET_BUFF_TYPE_INFO, inBuff set Buff **
               cmd is BUFF_GET_MEMPOOL_INFO, inBuff set mempool_t **
               cmd is BUFF_GET_MEMPOOL_BLK_AVAILABLE, inBuff set mempool_t **
 * @param [in] uint32 inLen, len of inBuff
 * @param [out] void*  outBuff, address to save get result
                cmd is BUFF_GET_MBUF_TIMEOUT_INFO, outBuff set struct MbufDebugInfo*
                cmd is BUFF_GET_MBUF_USE_INFO, outBuff set struct MbufUseInfo *
                cmd is BUFF_GET_MBUF_TYPE_INFO, outBuff set struct MbufTypeInfo *
                cmd is BUFF_GET_BUFF_TYPE_INFO, outBuff set struct BuffTypeInfo *
                cmd is BUFF_GET_MEMPOOL_INFO, outBuff set MemPoolInfo *
                cmd is BUFF_GET_MEMPOOL_BLK_AVAILABLE, outBuff set MpBlkAvailable *
 * @param [inout] uint32 outLen, len of outLen
 * @return   0   success
 * @return   other  fail
 */
DLLEXPORT int halBuffGetInfo(enum BuffGetCmdType cmd, void *inBuff, unsigned int inLen,
    void *outBuff, unsigned int *outLen);

/**
* @ingroup driver
* @brief free buff by pid
* @attention null
* @param [in] int pid: pid of the process
* @return   0 for success, others for fail
*/
DLLEXPORT int halBuffRecycleByPid(int pid);

/*=========================== Event Sched ===========================*/
struct event_info_common {
    EVENT_ID event_id;
    unsigned int subevent_id;
    int pid;
    int host_pid;
    unsigned int grp_id;
    unsigned long long submit_timestamp; /* The timestamp when the Event is submitted */
    unsigned long long sched_timestamp; /* The timestamp when the event is scheduled */
};

struct event_info_priv {
    unsigned int msg_len;
    char msg[EVENT_MAX_MSG_LEN];
};

struct event_info {
    struct event_info_common comm;
    struct event_info_priv priv;
};

#define EVENT_SUMMARY_RSV 3

struct event_summary {
    int pid; /* dst PID */
    unsigned int grp_id;
    EVENT_ID event_id;
    unsigned int subevent_id;
    unsigned int msg_len;
    char *msg; /* sync event should point to struct event_sync_msg */
    unsigned int dst_engine; /* dst system cpu type, SCHEDULE_DST_ENGINE */
    SCHEDULE_POLICY policy;
    unsigned int tid; /* id of specific thread to which publisher submit */
    int rsv[EVENT_SUMMARY_RSV];
};

#define ESCHED_GRP_PARA_RSV 3

struct esched_grp_para {
    GROUP_TYPE type;
    uint32_t threadNum;  /* threadNum range: [1, 1024] */
    char grp_name[EVENT_MAX_GRP_NAME_LEN];
    int rsv[ESCHED_GRP_PARA_RSV];
};

/**
* @ingroup driver
* @brief  attach one process to a device
* @attention null
* @param [in] devId: logic devid
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedAttachDevice(unsigned int devId) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  create a group for process
* @attention A process can create up to 32 groups
* @param [in] devId: logic devid
* @param [in] grpId: group id [0, 31]
* @param [in] GROUP_TYPE type: group type
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedCreateGrp(unsigned int devId, unsigned int grpId, GROUP_TYPE type) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  Subscribe events by thread
* @attention null
* @param [in] devId: logic devid
* @param [in] grpId: group id
* @param [in] threadId: thread id
* @param [in] eventBitmap: event bitmap
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedSubscribeEvent(unsigned int devId, unsigned int grpId,
    unsigned int threadId, unsigned long long eventBitmap) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  set the priority of pid
* @attention null
* @param [in] devId: logic devid
* @param [in] priority: pid priority
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedSetPidPriority(unsigned int devId, SCHEDULE_PRIORITY priority);

/**
* @ingroup driver
* @brief  Wait for an event to be scheduled
* @attention null
* @param [in] devId: logic devid
* @param [in] grpId: group id
* @param [in] threadId: thread id
* @param [in] timeout: timeout value
* @param [out] event: The event that is scheduled
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halEschedWaitEvent(unsigned int devId, unsigned int grpId,
    unsigned int threadId, int timeout, struct event_info *event) ASCEND_HAL_WEAK;


/*=========================== Queue Manage ===========================*/
#define QUEUE_MAX_STR_LEN        128
#define QUEUE_RESERVE_LEN 8

typedef enum queue_status {
    QUEUE_NORMAL = 0,
    QUEUE_EMPTY,
    QUEUE_FULL,
}QUEUE_STATUS;

typedef enum queue_work_mode {
    QUEUE_MODE_PUSH = 1,
    QUEUE_MODE_PULL,
}QUEUE_WORK_MODE;

typedef enum queue_type {
    QUEUE_TYPE_GROUP = 1,
    QUEUE_TYPE_SINGLE,
}QUEUE_TYPE;

typedef struct {
    unsigned long long enqueCnt;       // statistics of the successful enqueues
    unsigned long long dequeCnt;       // statistics of the successful dequeues
    unsigned long long enqueFailCnt;   // statistics of the failed enqueues
    unsigned long long dequeFailCnt;   // statistics of the failed dequeues
    unsigned long long enqueEventOk;   // statistics of the successful enqueue events
    unsigned long long enqueEventFail; // statistics of the failed enqueue events
    unsigned long long f2nfEventOk;    // statistics of the successful full-to-not-full events
    unsigned long long f2nfEventFail;  // statistics of the failed full-to-not-full events
    struct timeval lastEnqueTime;      // last enqueue time, not supported yet
    struct timeval lastDequeTime;      // last dequeue time, not supported yet
    int reserve[QUEUE_RESERVE_LEN];
}QueueStat;

typedef struct {
    int id;
    char name[QUEUE_MAX_STR_LEN];
    int size;
    int depth;
    int status;
    int workMode;
    int type;
    int subGroupId;
    int subPid;
    int subF2NFGroupId;
    int subF2NFPid;
    void* headDataPtr;
    int reserve[QUEUE_RESERVE_LEN];
    QueueStat stat;
}QueueInfo;

#define CLIENT_QUEUE_DEPLOY 0U
#define LOCAL_QUEUE_DEPLOY  1U

typedef struct {
    char name[QUEUE_MAX_STR_LEN];
    unsigned int depth;
    unsigned int workMode;
    bool flowCtrlFlag;
    unsigned int flowCtrlDropTime;
    bool overWriteFlag;
    unsigned int deploy_type : 1;
    unsigned int resv : 31;
}QueueAttr;

typedef struct {
    unsigned int *qids;
    unsigned int queNum;
    unsigned int reserve[QUEUE_RESERVE_LEN];
}QidsOfPid;

typedef struct {
    unsigned long long enqueDropCnt;
    unsigned long long dequeDropCnt;
}QUEUE_DROP_PKT_STAT;

typedef enum queue_query_item {
    QUERY_QUEUE_DEPTH,
    QUERY_QUEUE_STATUS,
    QUERY_QUEUE_DROP_STAT,
    QUERY_QUEUE_BUTTOM,
}QUEUE_QUERY_ITEM;

#define QUEUE_MAX_RESERV 5

struct QueueSubscriber {
    unsigned int devId;
    int pid;
    int spGrpId;
    int groupId;
    int rsv[QUEUE_MAX_RESERV];
};

typedef struct {
    unsigned int manage : 1;
    unsigned int read : 1;
    unsigned int write : 1;
    unsigned int rsv : 29;
} QueueShareAttr;

typedef struct {
    unsigned int devid;
    unsigned int qid;
    int pid;
    QueueShareAttr attr;
} QueueGrantPara;

typedef struct {
    unsigned int devid;
    unsigned int qid;
    int timeout;
} QueueAttachPara;

typedef enum {
    QUEUE_QUERY_QUE_ATTR_OF_CUR_PROC,
    QUEUE_QUERY_QUES_OF_CUR_PROC,
    QUEUE_QUERY_QUEUE_MBUF_INFO,
    QUEUE_QUERY_MAX_IOVEC_NUM,
    QUEUE_QUERY_DEPLOY_TYPE,
    QUEUE_QUERY_CMD_MAX,
} QueueQueryCmdType;

typedef struct {
    void *inBuff;
    unsigned int inLen;
} QueueQueryInputPara;

typedef struct {
    void *outBuff;
    unsigned int outLen;
} QueueQueryOutputPara;

typedef struct {
    int qid;
} QueQueryQueueAttr;

typedef struct {
    unsigned int qid;
} QueQueryQueueMbuf;

typedef struct {
    unsigned int qid;
} QueQueryDeploy;

typedef union {
    QueQueryQueueAttr queQueryQueueAttr;
    QueQueryQueueMbuf queQueryQueueMbuf;
    QueQueryDeploy queQueryDeploy;
} QueueQueryInput;

typedef struct {
    QueueShareAttr attr;
} QueQueryQueueAttrInfo;

typedef struct {
    int qid;
    QueueShareAttr attr;
} QueQueryQuesOfProcInfo;

typedef struct {
    unsigned long long timestamp;
} QueQueryQueueMbufInfo;

typedef struct {
    unsigned int count;
} QueQueryMaxIovecNum;

typedef struct {
    unsigned int deployType;
} QueQueryDeployInfo;

#define MAX_QUEUE_NUM 8192
typedef union {
    QueQueryQueueAttrInfo queQueryQueueAttrInfo;
    QueQueryQuesOfProcInfo queQueryQuesOfProcInfo[MAX_QUEUE_NUM];
    QueQueryQueueMbufInfo queQueryQueueMbufInfo;
    QueQueryMaxIovecNum queQueryMaxIovecNum;
    QueQueryDeployInfo queQueryDeployInfo;
} QueueQueryOutput;

typedef enum {
    QUEUE_SET_WORK_MODE,
    QUEUE_ENABLE_LOCAL_QUEUE,
    /* When the AICPU is busy, the agent queue events sent from the host can be processed by the Ctrl CPU.
         The process must call halDrvEventThreadInit to start the thread waiting event on the ctrl CPU, and then call
         this API to enable event multicast. (In addition to the original destination gid, it is also sent
         to the gid of the newly started thread.) */
    QUEUE_ENABLE_CLIENT_EVENT_MCAST,
    QUEUE_SET_CMD_MAX,
} QueueSetCmdType;

typedef struct {
    void *inBuff;
    unsigned int inLen;
} QueueSetInputPara;

typedef struct {
    unsigned int qid;
    unsigned int workMode;
} QueueSetWorkMode;

typedef union {
    QueueSetWorkMode queSetWorkMode;
} QueueSetInput;
/*lint +e116 +e17*/

typedef enum queue_event_type {
    QUEUE_ENQUE_EVENT = 0,
    QUEUE_F2NF_EVENT,
    QUEUE_EVENT_TYPE_MAX,
} QUEUE_EVENT_TYPE;

#define QUEUE_SUB_FLAG_SPEC_THREAD (0x1 << 0)
#define QUEUE_SUB_FLAG_SPEC_DST_DEVID (0x1 << 1)
struct QueueSubPara {
    unsigned int devId;
    unsigned int qid;
    QUEUE_TYPE queType;
    QUEUE_EVENT_TYPE eventType;
    unsigned int groupId;
    unsigned int flag;
    unsigned int threadId;
    unsigned int dstDevId; /* esched wait event para */
    int rev[QUEUE_RESERVE_LEN - 1];
};

struct QueueUnsubPara {
    unsigned int devId;
    unsigned int qid;
    QUEUE_EVENT_TYPE eventType;
    int rev[QUEUE_RESERVE_LEN];
};

#define EVENT_SRC_LOCATION_DEVICE 0
#define EVENT_SRC_LOCATION_HOST 1
struct QueueEventMsg {
    unsigned int src_location; /* 0: device; 1: host */
    unsigned int src_udevid;
};

typedef enum {
    QUEUE_PEEK_DATA_COPY_REF = 0,
    QUEUE_PEEK_DATA_TYPE_MAX,
} QueuePeekDataType;

/**
* @ingroup driver
* @brief  queue init
* @attention null
* @param [in] devId: logic devid
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueInit(unsigned int devId) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  destroy queue
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueDestroy(unsigned int devId, unsigned int qid) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  queue clean
* @attention null
* @param [in] qid: qid
* @param [in] devId: logic devid
* @return 0 for success, others for fail
**/
DLLEXPORT drvError_t halQueueReset(unsigned int devId, unsigned int qid) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  get queue status
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [in] queryItem: query item
* @param [in] len: size of query result
* @param [out] data: query result
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueGetStatus(unsigned int devId, unsigned int qid, QUEUE_QUERY_ITEM queryItem,
    unsigned int len,  void *data) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  query queue info
* @attention null
* @param [in] devId: logic devid
* @param [in] qid: queue id
* @param [out] queInfo: queue info
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueQueryInfo(unsigned int devId, unsigned int qid, QueueInfo *queInfo) ASCEND_HAL_WEAK;

/**
* @ingroup driver
* @brief  get qids by pid
* @attention null
* @param [in] devId: logic devid
* @param [in] pid: pid
* @param [in] maxQueSize: max number of return queue
* @param [inout] info: qids is an array for output result and queNum is the number of all qids
* @return   0 for success, others for fail
*/
DLLEXPORT drvError_t halQueueGetQidsbyPid(unsigned int devId, unsigned int pid,
       unsigned int maxQueSize, QidsOfPid *info) ASCEND_HAL_WEAK;

#ifdef __cplusplus
}
#endif
#endif


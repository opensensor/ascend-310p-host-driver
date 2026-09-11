/**
 * @file ascend_inpackage_hal.h
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description:
 * Author: huawei
 * Create: 2020-06-13
 * @brief driver interface.
 * @version 1.0
 *
 */

#ifndef __ASCEND_INPACKAGE_HAL_H__
#define __ASCEND_INPACKAGE_HAL_H__

#include <stdint.h>
#include "ascend_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

enum tagAicpufwPlat {
    AICPUFW_ONLINE_PLAT = 0,
    AICPUFW_OFFLINE_PLAT,
    AICPUFW_MAX_PLAT,
};

/* verify type for select, include soc and cms */
typedef enum {
    VERIFY_TYPE_SOC = 0,
    VERIFY_TYPE_CMS,
    VERIFY_TYPE_MAX
} HAL_VERIFY_TYPE;//lint !e116 !e17

/* image id for cms verification */
typedef enum {
    ITEE_IMG_ID = 0,
    DTB_IMG_ID,
    ZIMAGE_ID,
    FS_IMG_ID,
    SD_PEK_DTB_IMG_ID,
    SD_IMG_ID,
    PEK_IMG_ID,
    DP_IMG_ID,
    ROOTFS_IMG_ID,
    APP_IMG_ID,
    DTB_DP_PEK_IMG_ID,
    DTB_SD_PEK_IMG_ID,
    DP_PEK_IMG_ID,
    SD_PEK_IMG_ID,
    DP_CORE_IMG_ID,
    ABL_PATCH_IMG_ID,
    IMAGE_ID_MAX
} HAL_IMG_ID;//lint !e116 !e17

typedef enum {
    HAL_VMNGD_EVENT_CREATE_VF = 100,
    HAL_VMNGD_EVENT_DESTROY_VF,
    HAL_VMNGD_EVENT_MAX
}HAL_VMNGD_SUBEVENT_ID;//lint !e116 !e17

struct drvVmngdEventMsg {
    uint32_t dev_id;
    uint32_t vfid;
    uint32_t core_num;
    uint32_t total_core_num;
};

typedef enum {
    HAL_IMG_HEAD_TYPE_ROOT_HASH = 0,
    HAL_IMG_HEAD_TYPE_MAX
}HAL_IMG_HEAD_TYPE;//lint !e116 !e17

#define HAL_VERIFY_MODE_COVER_WITH_HEAD_OFF (1<<0) /* cover file with head off */

/**
* @ingroup driver
* @brief Initialize Device Memory
* @attention Must have a paired hostpid
* @param [in] hostpid Paired host side pid
* @param [in] vfid Paired device virtual function id
* @param [in] dev_id device id
* @return DRV_ERROR_NONE : success
* @return DV_ERROR_XXX : init fail
*/
DV_ONLINE DVresult drvMemInitSvmDevice(int hostpid, unsigned int vfid, unsigned int dev_id);

/**
 * @ingroup driver
 * @brief get borad id
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function.
 * @param [in] dev_id device id
 * @param [out] board_id  board id number
 * @return   0   success
 * @return   -1  fail
 */
int devdrv_get_board_id(unsigned int dev_id, unsigned int *board_id);
/**
 * @ingroup driver
 * @brief get vnic ip
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function.
 * @param [in] dev_id phy_id in host
 * @param [out] ip_addr vnic ip address
 * @return  0  success, return others fail
 */
int devdrv_get_vnic_ip(unsigned int dev_id, unsigned int *ip_addr);

/**
 * @ingroup driver
 * @brief get vnic ip by sdid
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function.
 * @param [in] sdid super pod SDID
 * @param [out] ip_addr vnic ip address
 * @return  0  success, return others fail
 */
int devdrv_get_vnic_ip_by_sdid(unsigned int sdid, unsigned int *ip_addr);

/**
 * @ingroup driver
 * @brief get eth_id by device index
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function.
 * @param [in] dev_id device id
 * @param [in] port_id port id in device
 * @param [out] eth_id ethnet id in device
 * @return   0   success
 * @return   -1  fail
 */
int drvDeviceGetEthIdByIndex(uint32_t dev_id, uint32_t port_id, uint32_t *eth_id);

/**
 * @ingroup driver
 * @brief verify image, including soc and cms verify operation
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function.
 * @param [in] verify_type  choose soc or cms
 * @param [in] image_id  image id, only use in cms verification.
  * @param [in] img_path  verify image path.
 * @param [in] mode choose cover file soc head or not.
 * @return  0  success, return others fail
 */
int halVerifyImg(HAL_VERIFY_TYPE verify_type, HAL_IMG_ID image_id, const char *img_path, int mode);

typedef enum {
    TSFW_HOT_PATCH_LOAD = 0,
    TSFW_HOT_PATCH_UNLOAD,
    TSFW_DRV_PLUGIN_LOAD,
    TSFW_PLUGIN_LOAD,
    TSFW_LOAD_MAX,
} TSFW_LOAD_TYPE;

/**
 * @ingroup driver
 * @brief load Ts Pkg
 * @attention only called by tsdaemon process
 * @param [in] dev_id device id
 * @param [in] load_type load pkg type, see TSFW_LOAD_TYPE
 * @param [in] ex_type reserved
 * @return  0  success, return others fail
 */
int halTsPkgLoad(unsigned int dev_id, TSFW_LOAD_TYPE load_type, unsigned int ex_type);

/**
 * @ingroup driver
 * @brief only for tsd register virtmng client
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function.
 * @param [in] verify_type  choose soc or cms
 * @param [in] image_id  image id, only use in cms verification.
  * @param [in] img_path  verify image path.
 * @param [in] mode choose cover file soc head or not.
 * @return  0  success, return others fail
 */
int halRegisterVmngClient(void);

/**
* @ingroup driver
* @brief record wait event or notify
* @attention only called by cp process
* @param [in] devId  device id
* @param [in] record_type  event or notify
* @param [in] record_Id
* @return 0  success, return others fail
*/
int tsDevRecord(unsigned int devId, unsigned int tsId, unsigned int record_type, unsigned int record_Id);

/**
 * @ingroup driver
 * @brief get image roothash with dm-verify of cms verify, only support rootfs and app image.
 * @attention This function is only can be called by components in driver of device,
 *  if the components is not in driver of device, don't use this function.
 * @param [in] image_id  image id, only use in cms verification.
 * @param [in] img_path  verify image path.
 * @param [in] cmd_type  head type.
 * @param [in] buf  the buff to save head info.
 * @param [in] buf_len  input buff length, when proc succ, the value should be change to actural value length
 * @return  0  success, return others fail
 */
int halGetImgHeadInfo(HAL_IMG_ID image_id, const char *img_path, HAL_IMG_HEAD_TYPE cmd_type, char *buf, int* buf_len);

typedef enum {
    UADK_CFG_ADD_PID_CMD = 0,
    UADK_CFG_DEL_PID_CMD,
    UADK_CFG_DISABLE_APP_CFG_PID_CMD,
} UADK_CFG_CMD_TYPE;

typedef enum {
    UADK_SEC_DEV = 0,
    UADK_ZIP_DEV,
} UADK_DEV_TYPE;

struct uadk_certified_info {
    unsigned int pid;
    UADK_DEV_TYPE dev_type;
    int rsv[3];
};

/**
 * @ingroup driver
 * @brief config pid to sec driver
 * @attention This function is only can be SecMgr
 * @param [in] cmd  config cmd
 * @param [in] info  config info
 * @return  0  success, return others fail
 */
int uadk_config_certified_info(UADK_CFG_CMD_TYPE cmd, struct uadk_certified_info *info);

typedef enum  {
    UADK_DIGEST_SHA256,
    UADK_DIGEST_AES_CMAC,
    UADK_DIGEST_SM3,
    UADK_DIGEST_ALG_MAX,
} UADK_DIGEST_ALG;

typedef struct {
    UADK_DIGEST_ALG alg;
    unsigned int task_mode; /**< 0:loop query, 1:intrrupt nofity */
    int rsv[4];
} uadk_digest_param;

typedef void* DIGEST_CTX;

/**
 * @ingroup driver
 * @brief init digest handle
 * @attention
 * @param [out] handle  digest context handle
 * @param [in] param  input param
 * @return  0  success, return others fail
 */
DLLEXPORT int uadk_digest_init(DIGEST_CTX *handle, uadk_digest_param *param);

/**
 * @ingroup driver
 * @brief alloc digest memery
 * @attention
 * @param [in] handle  digest context handle
 * @param [in] len  buffer length
 * @param [out] buff  input buffer
 * @return  0  success, return others fail
 */
DLLEXPORT int uadk_digest_alloc(DIGEST_CTX handle, unsigned int len, unsigned char **buff);

/**
 * @ingroup driver
 * @brief set digest key
 * @attention
 * @param [in] handle  digest context handle
 * @param [in] key  input key
 * @param [in] len  input key length
 * @return  0  success, return others fail
 */
DLLEXPORT int uadk_digest_set_key(DIGEST_CTX handle, unsigned char *key, unsigned int len);

/**
 * @ingroup driver
 * @brief digest process
 * @attention
 * @param [in] handle  digest context handle
 * @param [in] len  length of input data
 * @return  0  success, return others fail
 */
DLLEXPORT int uadk_digest_update(DIGEST_CTX handle, const unsigned int len);

/**
 * @ingroup driver
 * @brief get digest result
 * @attention
 * @param [in] handle  digest context handle
 * @param [out] digest  digest result
 * @param [in] len  length of input digest buffer
 * @param [out] out_len  length of output result
 * @return  0  success, return others fail
 */
DLLEXPORT int uadk_digest_final(DIGEST_CTX handle, unsigned char *digest,
    const unsigned int len, unsigned int *out_len);

/**
 * @ingroup driver
 * @brief uninit digest context handle
 * @attention
 * @param [in] handle  digest context handle
 * @return  0  success, return others fail
 */
DLLEXPORT int uadk_digest_uninit(DIGEST_CTX handle);

/**
 * @ingroup driver
 * @brief Query phycical device id by logical device id
 * @param [in]  dev_id  Logical device id
 * @param [out] phy_dev_id phycical device id
 * @return  0  success, return others fail
 */
drvError_t halGetPhyDevIdByLogicDevId(unsigned int dev_id, unsigned int *phy_dev_id);


/**
 * @ingroup driver
 * @brief ZIP MACRO
 */
#define HZIP_LEVEL_DEFAULT          0
#define HZIP_VERSION                "1.0.1"
#define HZIP_METHOD_DEFAULT         0
#define HZIP_WINDOWBITS_GZIP        16
#define HZIP_MEM_LEVEL_DEFAULT      0
#define HZIP_STRATEGY_DEFAULT       0
#define HZIP_FLUSH_TYPE_SYNC_FLUSH  2
#define HZIP_FLUSH_TYPE_FINISH      3
#define HZIP_OK                     0
#define HZIP_STREAM_END             1
#define HZIP_STREAM_NEED_AGAIN      2

/**
 * @ingroup driver
 * @brief zip stream param
 */
struct drv_zip_stream {
    void            *next_in;   /**< next input byte */
    unsigned long   avail_in;   /**< number of bytes available at next_in */
    unsigned long   total_in;   /**< total nb of input bytes read so far */
    void            *next_out;  /**< next output byte should be put there */
    unsigned long   avail_out;  /**< remaining free space at next_out */
    unsigned long   total_out;  /**< total nb of bytes output so far */
    char            *msg;       /**< last error message, NULL if no error */
    void            *workspace; /**< memory allocated for this stream */
    int             data_type;  /**< the data type: ascii or binary */
    unsigned long   adler;      /**< adler32 value of the uncompressed data */
    void            *reserved;  /**< reserved for future use */
};

/**
 * @ingroup driver
 * @brief zlib deflate init
 * @attention null
 * @param [inout] zstrm   zip stream
 * @param [in] level    HZIP_LEVEL_DEFAULT
 * @param [in] version  HZIP_VERSION
 * @param [in] stream_size  size of zstrm
 * @return   HZIP_OK   success
 * @return   other  fail
 */
DLLEXPORT int drv_hw_deflateInit_(struct drv_zip_stream *zstrm, int level, const char *version, int stream_size);

/**
 * @ingroup driver
 * @brief gzip deflate init
 * @attention null
 * @param [inout] zstrm  zip stream
 * @param [in] level   HZIP_LEVEL_DEFAULT
 * @param [in] method  HZIP_METHOD_DEFAULT
 * @param [in] windowBits  HZIP_WINDOWBITS_GZIP
 * @param [in] memLevel HZIP_MEM_LEVEL_DEFAULT
 * @param [in] strategy HZIP_STRATEGY_DEFAULT
 * @param [in] version  HZIP_VERSION
 * @param [in] stream_size  size of zstrm
 * @return   HZIP_OK   success
 * @return   other  fail
 */
DLLEXPORT int drv_hw_deflateInit2_(struct drv_zip_stream *zstrm, int level, int method, int windowBits,
    int memLevel, int strategy, const char *version, int stream_size);

/**
 * @ingroup driver
 * @brief deflat data
 * @attention null
 * @param [inout] zstrm  zip stream
 * @param [in] flush  HZIP_FLUSH_TYPE_SYNC_FLUSH/HZIP_FLUSH_TYPE_FINISH
 * @return   HZIP_OK   success
 * @return   HZIP_STREAM_END   stream end
 * @return   HZIP_STREAM_NEED_AGAIN  need again
 * @return   other  fail
 */
DLLEXPORT int drv_hw_deflate(struct drv_zip_stream *zstrm, int flush);

/**
 * @ingroup driver
 * @brief deflate end
 * @attention null
 * @param [inout] zstrm  zip stream
 * @return   HZIP_OK   sucess
 * @return   other  fail
 */
DLLEXPORT int drv_hw_deflateEnd(struct drv_zip_stream *zstrm);

/**
 * @ingroup driver
 * @brief zlib deflate init
 * @attention null
 * @param [inout] zstrm  zip stream
 * @param [in] version  HZIP_VERSION
 * @param [in] stream_size  size of zstrm
 * @return   HZIP_OK   success
 * @return   other  fail
 */
DLLEXPORT int drv_hw_inflateInit_(struct drv_zip_stream *zstrm, const char *version, int stream_size);

/**
 * @ingroup driver
 * @brief gzip inflate init
 * @attention null
 * @param [inout] zstrm  zip stream
 * @param [in] windowBits  HZIP_WINDOWBITS_GZIP
 * @param [in] version  HZIP_VERSION
 * @param [in] stream_size  size of zstrm
 * @return   HZIP_OK   success
 * @return   other  fail
 */
DLLEXPORT int drv_hw_inflateInit2_(struct drv_zip_stream *zstrm, int windowBits, const char *version, int stream_size);

/**
 * @ingroup driver
 * @brief inflate data
 * @attention null
 * @param [inout] zstrm  zip stream
 * @param [in] flush  HZIP_FLUSH_TYPE_SYNC_FLUSH/HZIP_FLUSH_TYPE_FINISH
 * @return   HZIP_OK   success
 * @return   HZIP_STREAM_END   stream end
 * @return   HZIP_STREAM_NEED_AGAIN  need again
 * @return   other  fail
 */
DLLEXPORT int drv_hw_inflate(struct drv_zip_stream *zstrm, int flush);

/**
 * @ingroup driver
 * @brief inflate end
 * @attention null
 * @param [inout] zstrm  zip stream
 * @return   HZIP_OK   sucess
 * @return   other  fail
 */
DLLEXPORT int drv_hw_inflateEnd(struct drv_zip_stream *zstrm);

#define PROF_SAMPLE_RSV_NUM 8
struct prof_sample_start_para {
    unsigned int dev_id;
    unsigned int sub_chan_id;
    int target_pid;
    void *user_data;       /* sample configuration information */
    unsigned int user_data_len;     /* sample length of the configuration data */
    unsigned int rsv[PROF_SAMPLE_RSV_NUM];
};

#define SAMPLE_DATA_ONLY        0x0      /* not the first sample, only data needs to be reported */
#define SAMPLE_DATA_WITH_HEADER 0x1      /* for the first sample, the data description header needs to be filled for some channels */
struct prof_sample_para {
    unsigned int dev_id;
    unsigned int sub_chan_id;
    int target_pid;
    unsigned int sample_flag;       /* SAMPLE_ONLY_DATA or SAMPLE_WITH_HEADER */
    void *buff;                     /* sample buff address */
    unsigned int buff_len;          /* total length of the sample buff */
    unsigned int report_len;        /* return value: actual reported data volume */
    unsigned int rsv[PROF_SAMPLE_RSV_NUM];
};

struct prof_sample_flush_para {
    unsigned int dev_id;
    unsigned int sub_chan_id;
    unsigned int rsv[PROF_SAMPLE_RSV_NUM];
};

struct prof_sample_stop_para {
    unsigned int dev_id;
    unsigned int sub_chan_id;
    unsigned int rsv[PROF_SAMPLE_RSV_NUM];
};

struct prof_sample_ops {
    int (*start_func)(struct prof_sample_start_para *para);
    int (*sample_func)(struct prof_sample_para *para);            /* NULL: sampler_period must equals to 0 */
    int (*flush_func)(struct prof_sample_flush_para *para);       /* not must */
    int (*stop_func)(struct prof_sample_stop_para *para);
};

struct prof_sample_register_para {
    unsigned int sub_chan_num;              /* multi-instance */
    struct prof_sample_ops ops;
};

/**
 * @ingroup driver
 * @brief register prof channel smaple handle
 * @attention null
 * @param [in] dev_id : device id
 * @param [in] chan_id : channel id
 * @param [in] para : information to be registered with the channel
 * @return  0 for success, others for fail
 */
DLLEXPORT int halProfSampleRegister(unsigned int dev_id, unsigned int chan_id, struct prof_sample_register_para *para);

/**
 * @ingroup driver
 * @brief query the rest of the prof channel
 * @attention The registration process calls to query the remaining writable length of the buffer.
 *  Only the process that registers this channel in user space is supported; others will return not support.
 * @param [in] dev_id : device id
 * @param [in] chan_id : channel id
 * @param [in/out] buff_avail_len : the amount of remaining data that can be written to the buff.
 * @return   DRV_ERROR_NONE   success
 * @return   other  fail
 */
DLLEXPORT int halProfQueryAvailBufLen(unsigned int dev_id, unsigned int chan_id, unsigned int *buff_avail_len);

struct prof_data_report_para {
    void *data;
    unsigned int data_len;
};

/**
 * @ingroup driver
 * @brief report prof channel data
 * @attention null
 * @param [in] dev_id : device id
 * @param [in] chan_id : channel id
 * @param [in] sub_chan_id : sub_channel_id
 * @param [in] para : data to be reported
 * @return  0 for success, others for fail
 */
DLLEXPORT int halProfSampleDataReport(unsigned int dev_id, unsigned int chan_id, unsigned int sub_chan_id,
    struct prof_data_report_para *para);

#ifdef __cplusplus
}
#endif
#endif


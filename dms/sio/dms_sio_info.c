/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
* Create: 2024-03-01
*/

#include <linux/uaccess.h>
#include "ascend_platform.h"

#include "ka_memory_pub.h"

#include "dbl/uda.h"
#include "dbl/feature_loader.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "ascend_kernel_hal.h"
#include "devdrv_user_common.h"
#include "devmng_shm_info.h"
#include "devdrv_info.h"

int dms_get_sio_crc_err_statistics(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#define DMS_MODULE_SIO_INFO "dms_sio_info"
#define DSMI_SIO_SUB_CMD_CRC_ERR_STATISTICS "main_cmd=0x38,sub_cmd=0x0"  // MAIN_CMD_SIO_INFO

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_SIO_INFO)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_SIO_INFO, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, DSMI_SIO_SUB_CMD_CRC_ERR_STATISTICS,
    NULL, DMS_ACC_NOT_LIMIT_USER | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT, dms_get_sio_crc_err_statistics)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION();

STATIC int dms_sio_parameter_check(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_get_sllc_bit_err_info(unsigned int phy_id, struct dms_sio_crc_err_info *info)
{
    int ret;
    unsigned int sllc_index, value;
    unsigned int tx_crc_err_cnt = 0;
    unsigned int rx_crc_err_cnt = 0;
    unsigned long long sllc_reg_addr = 0;
    unsigned long long sllc_reg_base[] = {SIO_SLLC0_RETRY_BASE_ADDR, SIO_SLLC1_RETRY_BASE_ADDR};

    sllc_index = info->sllc_index;
    if (sllc_index >= SIO_SLLC_MAX_NUM) {
        dms_err("Invalid parameter. (phy_id=%u; sllc_index=%u)\n", phy_id, sllc_index);
        return -EINVAL;
    }

    sllc_reg_addr = hal_kernel_get_dev_phy_base_addr(phy_id, sllc_reg_base[sllc_index]);

    ret = devdrv_reg_op(DEVDRV_REG_RD, sllc_reg_addr, SIO_SLLC_TX_RETRY_ST_CNT_OFFSET, KA_MM_PAGE_SIZE, &value);
    if (ret != 0) {
        dms_err("Register operation failed. (phy_id=%u; sllc_index=%u; ret=%d)\n", phy_id, sllc_index, ret);
        return ret;
    }
    tx_crc_err_cnt = value & SIO_SLLC_RETRY_CNT_MASK;

    ret = devdrv_reg_op(DEVDRV_REG_RD, sllc_reg_addr, SIO_SLLC_RX_RETRY_ST_CNT_OFFSET, KA_MM_PAGE_SIZE, &value);
    if (ret != 0) {
        dms_err("Register operation failed. (phy_id=%u; sllc_index=%u; ret=%d)\n", phy_id, sllc_index, ret);
        return ret;
    }
    rx_crc_err_cnt = value & SIO_SLLC_RETRY_CNT_MASK;

    info->tx_error_count = tx_crc_err_cnt;
    info->rx_error_count = rx_crc_err_cnt;

    return 0;
}

int dms_get_sio_crc_err_statistics(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int udevid = 0;
    unsigned int soc_type = CHIP_END;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = NULL;
    struct dms_sio_crc_err_info bit_err_info = {0};

    ret = dms_sio_parameter_check(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_err("Invalid parameter.\n");
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if ((input->buff == NULL) || (input->buff_size < sizeof(struct dms_sio_crc_err_info))) {
        dms_err("Invalid input parameter. (dev_id=%u; buff=%d; buff_size=%u)\n",
            input->dev_id, (input->buff != NULL), input->buff_size);
        return -EINVAL;
    }

    ret = copy_from_user(&bit_err_info, (void*)((uintptr_t)input->buff), sizeof(struct dms_sio_crc_err_info));
    if (ret != 0) {
        dms_err("copy_from_user failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    output = (struct dms_get_device_info_out *)out;

    ret = hal_kernel_get_soc_type(input->dev_id, &soc_type);
    if (soc_type != CHIP_CLOUD_V3) {
        return -EOPNOTSUPP;
    }

    ret = uda_devid_to_udevid(input->dev_id, &udevid);
    if (ret != 0) {
        dms_err("Failed to convert the logical ID to the physical ID. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    ret = dms_get_sllc_bit_err_info(udevid, &bit_err_info);
    if (ret != 0) {
        dms_err("Failed to get bit err statistics info. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    ret = copy_to_user(input->buff, &bit_err_info, sizeof(struct dms_sio_crc_err_info));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }
    output->out_size = sizeof(struct dms_sio_crc_err_info);

    return 0;
}

int dms_sio_init(void)
{
    dms_info("dms sio info init.\n");
    CALL_INIT_MODULE(DMS_MODULE_SIO_INFO);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_sio_init, FEATURE_LOADER_STAGE_5);

void dms_sio_exit(void)
{
    dms_info("dms sio info exit.\n");
    CALL_EXIT_MODULE(DMS_MODULE_SIO_INFO);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_sio_exit, FEATURE_LOADER_STAGE_5);

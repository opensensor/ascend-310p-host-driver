/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description:
 * Author: Huawei
 * Create: 2022-3-24
 * File Name     : dms_product.c
 * Version       : Initial product
 * Author        :
 * Created       : 2022-3-24
 * Last Modified :
 * Description   :

 *  History       :
 * 1.Date        : 2022-3-24
 *    Author      :
 *    Modification: Created file
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>

#include "dms_template.h"
#include "dms_cmd_def.h"
#include "dms_acc_ctrl.h"
#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "devdrv_interface.h"
#include "dms_product.h"
#ifdef CFG_FEATURE_DMS_PRODUCT_HOST
#include "dms_product_host.h"
#endif
#include <linux/ktime.h>
BEGIN_DMS_MODULE_DECLARATION(DMS_PRODUCT_CMD_NAME)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME, DMS_MAIN_CMD_PRODUCT, DMS_SUBCMD_GET_PCIE_ID_INFO_ALL, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_pcie_id_all)
#ifdef CFG_FEATURE_DMS_PRODUCT_HOST
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME, DMS_MAIN_CMD_PRODUCT, DMS_SUBCMD_GET_WORK_MODE, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_work_mode)
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME, DMS_MAIN_CMD_PRODUCT, DMS_SUBCMD_GET_CARD_MULTI_DIE_POLICY, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_card_multi_die_policy)
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME, DMS_MAIN_CMD_PRODUCT, DMS_SUBCMD_SET_CARD_MULTI_DIE_POLICY, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_set_card_multi_die_policy)
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME, DMS_MAIN_CMD_PRODUCT, DMS_SUBCMD_GET_HCCS_STATUS, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_hccs_status)
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME, DMS_MAIN_CMD_PRODUCT, DMS_SUBCMD_GET_MAINBOARD_ID, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_mainboard_id)
#endif
#ifdef CFG_FEATURE_HCCS_BANDWIDTH
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME,
    DMS_MAIN_CMD_PRODUCT,
    DMS_SUBCMD_GET_HCCS_BANDWIDTH_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_feature_get_hccs_bandwidth_info)
#endif
#ifdef CFG_FEATURE_PCIE_HCCS_BANDWIDTH
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME,
    DMS_MAIN_CMD_PRODUCT,
    DMS_SUBCMD_GET_PCIE_BANDWIDTH_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_feature_get_pcie_bandwidth_info)
#endif
#if (defined(CFG_FEATURE_HBM_MANUFACTURER_ID) && !defined(CFG_FEATURE_DMS_PRODUCT_HOST))
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME,
    DMS_MAIN_CMD_PRODUCT,
    DMS_SUBCMD_GET_HBM_MANUFACTURER_ID,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_feature_get_hbm_manufacturer_id)
#endif
#ifdef CFG_FEATURE_HCCS_LINK_ERROR_INFO
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME,
    DMS_MAIN_CMD_PRODUCT,
    DMS_SUBCMD_GET_HCCS_LINK_ERROR_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_feature_get_hccs_link_error_info)
#endif

#ifdef CFG_FEATURE_PCIE_LINK_ERROR_INFO
ADD_FEATURE_COMMAND(DMS_PRODUCT_CMD_NAME,
    DMS_MAIN_CMD_PRODUCT,
    DMS_SUBCMD_GET_PCIE_LINK_ERROR_INFO,
    NULL,
    NULL,
    DMS_SUPPORT_ALL,
    dms_feature_get_pcie_link_error_info)
#endif

END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_product_init(void)
{
    CALL_INIT_MODULE(DMS_PRODUCT_CMD_NAME);
    return 0;
}

void dms_product_exit(void)
{
    CALL_EXIT_MODULE(DMS_PRODUCT_CMD_NAME);
}

int devdrv_get_pcie_id_all(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dmanage_pcie_id_info_all *pcie_id_info = NULL;
    struct devdrv_pcie_id_info id_info = {0};
    unsigned int dev_id, virt_id, vfid;
    int ret;

    if (in == NULL || (in_len != sizeof(unsigned int))) {
        dms_err("Input char is NULL or in_len is wrong.");
        return -EINVAL;
    }

    if (out == NULL || (out_len != sizeof(struct dmanage_pcie_id_info_all))) {
        dms_err("Out char is NULL or in_len is wrong.");
        return -EINVAL;
    }

    virt_id = *(unsigned int *)in;
    ret = devdrv_manager_container_logical_id_to_physical_id(virt_id, &dev_id, &vfid);
    if (ret != 0) {
        dms_err("Can't transfor virt id, (virt id = %u, ret = %d)\n", virt_id, ret);
        return ret;
    }

    ret = devdrv_get_pcie_id_info(dev_id, &id_info);
    if (ret != 0) {
        dms_err("Devdrv_get_pcie_id_info failed.\n");
        return ret;
    }

    pcie_id_info = (struct dmanage_pcie_id_info_all *)out;
    pcie_id_info->venderid = id_info.venderid;
    pcie_id_info->subvenderid = id_info.subvenderid;
    pcie_id_info->deviceid = id_info.deviceid;
    pcie_id_info->subdeviceid = id_info.subdeviceid;
    pcie_id_info->domain = id_info.domain;
    pcie_id_info->bus = id_info.bus;
    pcie_id_info->device = id_info.device;
    pcie_id_info->fn = id_info.fn;

    return 0;
}

int devdrv_get_mainboard_id(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int board_id, mainboard_id;
    unsigned int device_id;
    struct devdrv_info *device_info = NULL;

    if (in == NULL || (in_len != sizeof(unsigned int))) {
        dms_err("In char is NULL or in_len is wrong.\n");
        return -EINVAL;
    }

    if (out == NULL || (out_len != sizeof(unsigned int))) {
        dms_err("Out char is NULL or out_len is wrong.\n");
        return -EINVAL;
    }

    device_id = *(unsigned int*)in;
    device_info = devdrv_manager_get_devdrv_info(device_id);
    if (device_info == NULL) {
        dms_err("The device_info is Null. (device_id=%u)\n", device_id);
        return -EINVAL;
    }
    board_id = device_info->board_id >> A2_BOARD_TYPE_SHIFT;
    if ((board_id <= A2_CARD_BOARD_TYPE_MAX) && (board_id >= A2_CARD_BOARD_TYPE_MIN)) {
        // A2标卡形态，mainboardid置为默认非法值
        mainboard_id = A2_CARD_DEFAULT_MAINBOARD_ID;
    } else {
        mainboard_id = device_info->mainboard_id;
    }

    ret = memcpy_s((void *)out, out_len, (void *)&mainboard_id, sizeof(unsigned int));
    if (ret) {
        dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    return ret;
}

#ifdef CFG_FEATURE_HCCS_BANDWIDTH
typedef struct {
    struct hrtimer timer;          // 高精度定时器
    int info_status;               // 状态信息
    wait_queue_head_t waitqueue;   // 等待队列
} hccs_context_t;

STATIC const unsigned long hccs_hdlc_phy_addr[LINK_NUM] = {
    HCCS_HDLC2_BASE_ADDR, HCCS_HDLC2_BASE_ADDR,
    HCCS_HDLC3_BASE_ADDR, HCCS_HDLC3_BASE_ADDR,
    HCCS_HDLC0_BASE_ADDR, HCCS_HDLC0_BASE_ADDR,
    HCCS_HDLC1_BASE_ADDR, HCCS_HDLC1_BASE_ADDR,
};

STATIC void dms_hccs_bandwidth_trans(hccs_statistic_info *hccs_statistic_infofir,
    hccs_statistic_info *hccs_statistic_infolast, hccs_link_bandwidth_t *hccs_link_bandwidth)
{
    int i;
    unsigned int tx_diff;
    unsigned int rx_diff;
    for (i = 0; i < HCCS_MAX_PCS_NUM; i++) {
        if (hccs_statistic_infolast->tx_cnt[i] < hccs_statistic_infofir->tx_cnt[i]) {
            tx_diff = (U32_MAX - hccs_statistic_infofir->tx_cnt[i] +
                hccs_statistic_infolast->tx_cnt[i] + OVERFLOWADD);
        } else {
            tx_diff = hccs_statistic_infolast->tx_cnt[i] - hccs_statistic_infofir->tx_cnt[i];
        }
        hccs_link_bandwidth->tx_bandwidth[i] = tx_diff;

        if (hccs_statistic_infolast->rx_cnt[i] < hccs_statistic_infofir->rx_cnt[i]) {
            rx_diff = (U32_MAX - hccs_statistic_infofir->rx_cnt[i] +
                hccs_statistic_infolast->rx_cnt[i] + OVERFLOWADD);
        } else {
            rx_diff = hccs_statistic_infolast->rx_cnt[i] - hccs_statistic_infofir->rx_cnt[i];
        }
        hccs_link_bandwidth->rx_bandwidth[i] = rx_diff;
    }
}

STATIC enum hrtimer_restart hccs_bandwidth_hrtimer_callback(struct hrtimer *timer)
{
    hccs_context_t *ctx = container_of(timer, hccs_context_t, timer); // 获取上下文
    ctx->info_status = INFO_TRUE;  // 设置状态
    wake_up_interruptible(&ctx->waitqueue);
    return HRTIMER_NORESTART;
}

STATIC void init_hrtimer(hccs_link_bandwidth_t *hccs_link_bandwidth, hccs_context_t *ctx)
{
    ktime_t interval;
    unsigned long nsecs;
    int profiling_time = hccs_link_bandwidth->profiling_time;
    nsecs = (unsigned long)profiling_time * MS_TO_NS;
    interval = ktime_set(0, nsecs);
    hrtimer_init(&ctx->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    ctx->timer.function = hccs_bandwidth_hrtimer_callback;
    hrtimer_start(&ctx->timer, interval, HRTIMER_MODE_REL);
}

STATIC int dms_get_hccs_bandwidth_info(unsigned int dev_id, hccs_statistic_info *hccs_statistic)
{
    unsigned int i = 0;
    int ret;
    unsigned long link_index;
    void __iomem *hccs_base_addr = NULL;
    devdrv_hardware_info_t hardware_info = {0};

    ret = hal_kernel_get_hardware_info(dev_id, &hardware_info);
    if (ret != 0) {
        dms_err("Failed to invoke hal_kernel_get_hardware_info. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    for (i = 0; i < LINK_NUM; i++) {
        hccs_base_addr = ioremap(hccs_hdlc_phy_addr[i] + hardware_info.phy_addr_offset, HCCS_HDLC_REG_MAP_SIZE);
        if (hccs_base_addr == NULL) {
            dms_err("Failed to ioremap for hdlc info. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }
        link_index = i % HDLC_LINK_HCCS_NUM;
        hccs_statistic->tx_cnt[i] =  hccs_reg_read(((unsigned long)(uintptr_t)hccs_base_addr) +
            (HCCS_HDLC_TX_CNT_ADDR + (link_index * HCCS_HDLC_REG_SIZE)));

        hccs_statistic->rx_cnt[i] =  hccs_reg_read(((unsigned long)(uintptr_t)hccs_base_addr) +
            (HCCS_HDLC_RX_CNT_ADDR + (link_index * HCCS_HDLC_REG_SIZE)));
        iounmap(hccs_base_addr);
        hccs_base_addr = NULL;
    }
    return 0;
}


STATIC int dms_get_hccs_bandwidth_by_dev_id(unsigned int dev_id, hccs_link_bandwidth_t *hccs_link_bandwidth)
{
    int ret;
    hccs_context_t ctx = {0}; // 局部变量，存储上下文信息
    hccs_statistic_info hccs_statistic_infofir = {0};
    hccs_statistic_info hccs_statistic_infolast = {0};
    // 初始化等待队列
    init_waitqueue_head(&ctx.waitqueue);
    if ((hccs_link_bandwidth->profiling_time < PROFILING_TIME_HCCS_MIN) ||
        (hccs_link_bandwidth->profiling_time > PROFILING_TIME_HCCS_MAX)) {
        dms_err("Get hccs bandwidth profiling time invalid. (device_id=%u; profiling_time=%d)\n",
            dev_id, hccs_link_bandwidth->profiling_time);
            return -EINVAL;
    }
    ret = dms_get_hccs_bandwidth_info(dev_id, &hccs_statistic_infofir);
    if (ret != 0) {
        return ret;
    }

    init_hrtimer(hccs_link_bandwidth, &ctx);
    ret = wait_event_interruptible_timeout(ctx.waitqueue,  ctx.info_status == INFO_TRUE,
        msecs_to_jiffies(hccs_link_bandwidth->profiling_time));
    // 任务完成，关闭定时器
    hrtimer_cancel(&ctx.timer);
    if (ret == 0) {
        dms_err("Profiling time out\n");
        return -ETIMEDOUT;
    } else if (ret < 0) {
        // 处理信号中断
        dms_err("Wait queue interrupted by signal\n");
        return -EINTR;
    }
    ctx.info_status = INFO_FALSE; // 重置状态

    ret = dms_get_hccs_bandwidth_info(dev_id, &hccs_statistic_infolast);
    if (ret != 0) {
        return ret;
    }

    dms_hccs_bandwidth_trans(&hccs_statistic_infofir, &hccs_statistic_infolast, hccs_link_bandwidth);
    return 0;
}

STATIC int dms_get_hccs_bandwidth(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    hccs_link_bandwidth_t hccs_link_bandwidth = {0};

    ret = copy_from_user(&hccs_link_bandwidth, (void *)(uintptr_t)in->buff, sizeof(hccs_link_bandwidth_t));
    if (ret != 0) {
        dms_err("Copy_from_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    ret = dms_get_hccs_bandwidth_by_dev_id(in->dev_id, &hccs_link_bandwidth);
    if (ret != 0) {
        return ret;
    }

    ret = copy_to_user((void *)(uintptr_t)in->buff, &hccs_link_bandwidth, sizeof(hccs_link_bandwidth_t));
    if (ret != 0) {
        dms_err("Copy_from_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    *out_size = sizeof(hccs_link_bandwidth_t);
    return 0;
}

int dms_feature_get_hccs_bandwidth_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = {0};
    unsigned int out_size = 0;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if ((input->buff == NULL) || (input->buff_size < sizeof(hccs_link_bandwidth_t))) {
        dms_err("Input buffer is null or buffer size is not valild. (buff_is_null=%d; buff_size=%u)\n",
            (input->buff == NULL), input->buff_size);
        return -EINVAL;
    }

    output = (struct dms_get_device_info_out *)out;
    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null or out_len is wrong. (buff_is_null=%d; out_len=%u)\n",
            (out == NULL), out_len);
        return -EINVAL;
    }

    ret = dms_get_hccs_bandwidth(input, &out_size);
    if (ret != 0) {
        dms_err("Dms get hccs bandwidth failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    output->out_size = out_size;
    return 0;
}
#endif

#ifdef CFG_FEATURE_PCIE_HCCS_BANDWIDTH
STATIC void dms_pcie_bandwidth_trans(pcie_link_bandwidth_t *pcie_link_bandwidth)
{
    /* trans bandwith from bytes/us to Mbytes/ms */
    int i;

    for (i = 0; i < AGENTDRV_PROF_DATA_NUM; i++) {
        pcie_link_bandwidth->rx_cpl_bw[i] = (pcie_link_bandwidth->rx_cpl_bw[i] * PROFILING_US_TRANS_TO_MS) /
            PROFILING_BYTE_TRANS_TO_MBYTE;
        pcie_link_bandwidth->rx_np_bw[i] = (pcie_link_bandwidth->rx_np_bw[i]  * PROFILING_US_TRANS_TO_MS) /
            PROFILING_BYTE_TRANS_TO_MBYTE;
        pcie_link_bandwidth->rx_p_bw[i] = (pcie_link_bandwidth->rx_p_bw[i]  * PROFILING_US_TRANS_TO_MS) /
            PROFILING_BYTE_TRANS_TO_MBYTE;
        pcie_link_bandwidth->tx_cpl_bw[i] = (pcie_link_bandwidth->tx_cpl_bw[i]  * PROFILING_US_TRANS_TO_MS) /
            PROFILING_BYTE_TRANS_TO_MBYTE;
        pcie_link_bandwidth->tx_np_bw[i] = (pcie_link_bandwidth->tx_np_bw[i]  * PROFILING_US_TRANS_TO_MS) /
            PROFILING_BYTE_TRANS_TO_MBYTE;
        pcie_link_bandwidth->tx_p_bw[i] = (pcie_link_bandwidth->tx_p_bw[i]  * PROFILING_US_TRANS_TO_MS) /
            PROFILING_BYTE_TRANS_TO_MBYTE;
    }

    return;
}

STATIC int dms_get_pcie_bandwidth_by_dev_id(unsigned int dev_id, pcie_link_bandwidth_t *pcie_link_bandwidth)
{
    int ret;
    struct prof_peri_para prof_para = {0};
    struct agentdrv_profiling_buf agentdrv_profiling_buf_temp = {0};

    if ((pcie_link_bandwidth->profiling_time < 0) || (pcie_link_bandwidth->profiling_time > PROFILING_TIME_MAX)) {
        dms_err("Get pcie bandwidth profiling time invalid. (device_id=%u; profiling_time=%d)\n",
            dev_id, pcie_link_bandwidth->profiling_time);
            return -EINVAL;
    }

    prof_para.device_id = dev_id;
    prof_para.buff = &agentdrv_profiling_buf_temp;
    prof_para.buff_len = sizeof(struct agentdrv_profiling_buf);

    ret = agentdrv_pcie_profiling_open(prof_para);
    if (ret != 0) {
        dms_err("Start pcie bandwidth profiling failed. (device_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    msleep(pcie_link_bandwidth->profiling_time);

    ret = agentdrv_pcie_profiling_sampling(prof_para);
    if (ret < 0) {
        dms_err("Get pcie bandwidth profiling data failed. (device_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = agentdrv_pcie_profiling_close(prof_para);
    if (ret != 0) {
        dms_err("Stop pcie bandwidth profiling failed. (device_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = memcpy_s(pcie_link_bandwidth->tx_p_bw, sizeof(struct agentdrv_profiling_buf) - sizeof(u32) - sizeof(u64),
        agentdrv_profiling_buf_temp.tx_p_bw, sizeof(struct agentdrv_profiling_buf) - sizeof(u32) - sizeof(u64));
    if (ret != 0) {
        dms_err("Copy pcie bandwidth profiling result failed. (device_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    dms_pcie_bandwidth_trans(pcie_link_bandwidth);

    return 0;
}

STATIC int dms_get_pcie_bandwidth(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    pcie_link_bandwidth_t pcie_link_bandwidth = {0};

    ret = copy_from_user(&pcie_link_bandwidth, (void *)(uintptr_t)in->buff, sizeof(pcie_link_bandwidth_t));
    if (ret != 0) {
        dms_err("copy_from_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    ret = dms_get_pcie_bandwidth_by_dev_id(in->dev_id, &pcie_link_bandwidth);
    if (ret != 0) {
        return ret;
    }

    ret = copy_to_user((void *)(uintptr_t)in->buff, &pcie_link_bandwidth, sizeof(pcie_link_bandwidth_t));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    *out_size = sizeof(pcie_link_bandwidth_t);
    return 0;
}

int dms_feature_get_pcie_bandwidth_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = {0};
    unsigned int out_size = 0;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if ((input->buff == NULL) || (input->buff_size < sizeof(pcie_link_bandwidth_t))) {
        dms_err("Input buffer is null or buffer size is not valild. (buff_is_null=%d; buff_size=%u)\n",
            (input->buff != NULL), input->buff_size);
        return -EINVAL;
    }

    output = (struct dms_get_device_info_out *)out;
    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    ret = dms_get_pcie_bandwidth(input, &out_size);
    if (ret != 0) {
        dms_err("Dms get pcie bandwidth failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    output->out_size = out_size;
    return 0;
}
#endif

#if (defined(CFG_FEATURE_HBM_MANUFACTURER_ID) && !defined(CFG_FEATURE_DMS_PRODUCT_HOST))
STATIC int dms_get_chip_die_offset(u32 dev_id, u64 *chip_base_addr, u64 *chip_offset, u64 *die_offset)
{
    u32 connect_type;

    connect_type = devdrv_get_connect_protocol(dev_id);
    if (connect_type == CONNECT_PROTOCOL_PCIE) {
        *chip_base_addr = FULLMESH_CHIP_BASE_ADDR;
        *chip_offset = FULLMESH_CHIP_OFFSET;
        *die_offset = FULLMESH_DIE_OFFSET;
    } else if (connect_type == CONNECT_PROTOCOL_HCCS) {
        *chip_base_addr = HCCS_CHIP_BASE_ADDR;
        *chip_offset = HCCS_CHIP_OFFSET;
        *die_offset = HCCS_DIE_OFFSET;
    } else {
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_get_local_sram_base_addr(u32 dev_id, u64 *base)
{
    int ret;
    u32 chip_id, die_id;
    u64 chip_base_addr, chip_offset, die_offset;

    ret = devdrv_get_chip_die_id(dev_id, &chip_id, &die_id);    /* get chip id and die id by device id */
    if (ret != 0) {
        dms_err("Get chip id and die id failed, dev_id = %u\n", dev_id);
        return ret;
    }

    ret = dms_get_chip_die_offset(dev_id, &chip_base_addr, &chip_offset, &die_offset);
    if (ret != 0) {
        dms_err("Get chip and die offset failed, dev_id = %u\n", dev_id);
        return ret;
    }
    // 主die从die的hbm厂家相同，当前bios值写入主die数据，统一从主die获取，不添加从die偏移
    *base = chip_base_addr + (uint64_t)chip_id * chip_offset;
    return 0;
}

int dms_feature_get_hbm_manufacturer_id(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int dev_id;
    unsigned int *manufacturer_id = NULL;
    u32 tmp_manufacturer_id = MANUFACTURER_ID_ERROR;
    u64 chip_base = CHIP_BASE_ADDR;
    u64 load_sram_base;
    void __iomem *manufacturer_sram_base = NULL;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input char is NULL or in_len is wrong.");
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(unsigned int))) {
        dms_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;
    manufacturer_id = (unsigned int *)out;

    ret = dms_get_local_sram_base_addr(dev_id, &chip_base);
    if (ret != 0) {
        dms_err("Get local sram base addr failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    load_sram_base = chip_base + DEVDRV_LOAD_SRAM_ADDR;
    manufacturer_sram_base = ioremap(load_sram_base + DEVDRV_MANUFACTURER_ID_OFFSET, DEVDRV_LOAD_SRAM_SIZE);
    if (manufacturer_sram_base == NULL) {
        dms_err("Sram ioremap error.\n");
        return -EINVAL;
    }

    tmp_manufacturer_id = readl((void __iomem *)manufacturer_sram_base);

    iounmap(manufacturer_sram_base);
    manufacturer_sram_base = NULL;
    *manufacturer_id = tmp_manufacturer_id;
    return 0;
}
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
int devdrv_get_hccs_status(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int device_id;
    unsigned int hccs_group_id[HCCS_GROUP_SUPPORT_MAX_CHIPNUM] = {0};
    unsigned int hccs_link_status = 0;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("In char is NULL or in_len is wrong.\n");
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(unsigned int))) {
        dms_err("Out char is NULL or out_len is wrong.\n");
        return -EINVAL;
    }

    device_id = *(unsigned int*)in;
    ret = devdrv_get_hccs_link_status_and_group_id(device_id, &hccs_link_status, hccs_group_id,
                                                   HCCS_GROUP_SUPPORT_MAX_CHIPNUM);
    if (ret != 0) {
        dms_err("Get hccs link status and group id failed, ret=[%d] (device_id=[%u])\n", ret, device_id);
        return -EINVAL;
    }

    ret = memcpy_s((void *)out, out_len, (void *)&hccs_link_status, sizeof(unsigned int));
    if (ret) {
        dms_err("Call memcpy_s failed. (ret=[%d])\n", ret);
        return -EINVAL;
    }

    return ret;
}
#endif

#ifdef CFG_FEATURE_HCCS_LINK_ERROR_INFO
STATIC int dms_get_link_error_info_details(unsigned int dev_id, struct hccs_statistic_info *hccs_statistic)
{
    int ret;
    void __iomem *hccs_base_addr = NULL;
    devdrv_hardware_info_t hardware_info = {0};

    ret = hal_kernel_get_hardware_info(dev_id, &hardware_info);
    if (ret != 0) {
        dms_err("Failed to invoke hal_kernel_get_hardware_info. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    hccs_base_addr = ioremap(HDLC0_BASE_ADDR + hardware_info.phy_addr_offset, HDLC_REG_MAP_SIZE);
    if (hccs_base_addr == NULL) {
        dms_err("Failed to ioremap for hdlc info. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }
    hccs_statistic->retry_cnt[0] = hccs_reg_read(((unsigned long)(uintptr_t)hccs_base_addr) +
        HDLC_RETRY_CNT_ADDR);

    hccs_statistic->crc_err_cnt[0] = hccs_reg_read(((unsigned long)(uintptr_t)hccs_base_addr) +
        HDLC_CRC_ERR_CNT_ADDR);
    iounmap(hccs_base_addr);
    return 0;
}

STATIC int dms_get_hccs_link_error_info_by_dev_id(unsigned int dev_id, struct hccs_link_err_info_t *hccs_link_info)
{
    int ret;
    struct hccs_statistic_info info = {0};

    ret = dms_get_link_error_info_details(dev_id, &info);
    if (ret != 0) {
        dms_err("dms_get_link_error_info_details failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    hccs_link_info->crc_err_cnt = info.crc_err_cnt[0];
    hccs_link_info->retry_cnt   = info.retry_cnt[0];
    return 0;
}

STATIC int dms_get_hccs_link_error_info(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    struct hccs_link_err_info_t hccs_link_info = {0};

    ret = copy_from_user(&hccs_link_info, (void *)(uintptr_t)in->buff, sizeof(struct hccs_link_err_info_t));
    if (ret != 0) {
        dms_err("dms_get_hccs_link_error_info Copy_from_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    ret = dms_get_hccs_link_error_info_by_dev_id(in->dev_id, &hccs_link_info);
    if (ret != 0) {
        dms_err("dms_get_hccs_link_error_info_by_dev_id failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    ret = copy_to_user((void *)(uintptr_t)in->buff, &hccs_link_info, sizeof(struct hccs_link_err_info_t));
    if (ret != 0) {
        dms_err("dms_get_hccs_link_error_info Copy_from_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    *out_size = sizeof(struct hccs_link_err_info_t);
    return 0;
}

int dms_feature_get_hccs_link_error_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = NULL;
    unsigned int out_size = 0;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if ((input->buff == NULL) || (input->buff_size < sizeof(struct hccs_link_err_info_t))) {
        dms_err("Input buffer is null or buffer size is not valild. (buff_is_null=%d; buff_size=%u)\n",
            (input->buff == NULL), input->buff_size);
        return -EINVAL;
    }

    output = (struct dms_get_device_info_out *)out;
    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null or out_len is wrong. (buff_is_null=%d; out_len=%u)\n",
            (out == NULL), out_len);
        return -EINVAL;
    }

    ret = dms_get_hccs_link_error_info(input, &out_size);
    if (ret != 0) {
        dms_err("Dms get hccs bandwidth failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    output->out_size = out_size;
    return 0;
}
#endif

#ifdef CFG_FEATURE_PCIE_LINK_ERROR_INFO
STATIC int dms_pcie_link_err_dump(u32 dev_id, struct soc_misc_pcie_link_err_info *info)
{
    int ret;
    devdrv_hardware_info_t hardware_info = {0};
    void __iomem *reg_virt_addr = NULL;
    u64 reg_phy_addr;

    ret = hal_kernel_get_hardware_info(dev_id, &hardware_info);
    if (ret != 0) {
        dms_err("Failed to get hal_kernel_get_hardware_info. (dev_id=%u)\n", dev_id);
        return ret;
    }

    reg_phy_addr = hardware_info.phy_addr_offset + RAS_PCIE_DFX_LCRC_ERR_NUM;
    reg_virt_addr = ioremap(reg_phy_addr, PCIE_RAS_REG_SIZE);
    if (reg_virt_addr == NULL) {
        dms_err("Failed to ioremap.(dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }
    info->lcrc_err_cnt = readl(reg_virt_addr);
    iounmap(reg_virt_addr);

    reg_phy_addr = 0;
    reg_virt_addr = NULL;
    reg_phy_addr = hardware_info.phy_addr_offset + RAS_PCIE_DFX_RETRY_CNT;
    reg_virt_addr = ioremap(reg_phy_addr, PCIE_RAS_REG_SIZE);
    if (reg_virt_addr == NULL) {
        dms_err("Failed to ioremap.(dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }
    info->retry_cnt = readl(reg_virt_addr);
    iounmap(reg_virt_addr);
    return 0;
}

STATIC int dms_get_pcie_link_error_info(struct dms_get_device_info_in *in, unsigned int *out_size)
{
    int ret;
    struct soc_misc_pcie_link_err_info pcie_link_info = {0};

    ret = copy_from_user(&pcie_link_info, (void *)(uintptr_t)in->buff, sizeof(struct soc_misc_pcie_link_err_info));
    if (ret != 0) {
        dms_err("Copy_from_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    ret = dms_pcie_link_err_dump(in->dev_id, &pcie_link_info);
    if (ret != 0) {
        dms_err("dms_pcie_link_err_dump failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    ret = copy_to_user((void *)(uintptr_t)in->buff, &pcie_link_info, sizeof(struct soc_misc_pcie_link_err_info));
    if (ret != 0) {
        dms_err("Copy_from_user failed. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

    *out_size = sizeof(struct soc_misc_pcie_link_err_info);
    return 0;
}

int dms_feature_get_pcie_link_error_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = {0};
    unsigned int out_size = 0;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if ((input->buff == NULL) || (input->buff_size < sizeof(struct soc_misc_pcie_link_err_info))) {
        dms_err("Input buffer is null or buffer size is not valild. (buff_is_null=%d; buff_size=%u)\n",
            (input->buff == NULL), input->buff_size);
        return -EINVAL;
    }

    output = (struct dms_get_device_info_out *)out;
    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null or out_len is wrong. (buff_is_null=%d; out_len=%u)\n",
            (out == NULL), out_len);
        return -EINVAL;
    }

    ret = dms_get_pcie_link_error_info(input, &out_size);
    if (ret != 0) {
        dms_err("Dms get hccs bandwidth failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    output->out_size = out_size;
    return 0;
}
#endif
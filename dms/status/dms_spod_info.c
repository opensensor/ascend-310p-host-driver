/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
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
* Create: 2023-12-21
*/

#include "linux/uaccess.h"
#include "dbl/spod_info.h"
#include "dbl/uda.h"
#include "dbl/feature_loader.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms_dev_info_check.h"
#include "devdrv_common.h"
#include "ascend_kernel_hal.h"
#include "dbl_mem_alloc_interface.h"
#include "dms_notifier.h"

#ifdef CFG_HOST_ENV
#include "devdrv_manager_container.h"
#include "devdrv_interface.h"
#endif

#include "ka_memory_pub.h"

int dms_get_spod_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_spod_ping_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_get_spod_node_status(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_set_spod_node_status(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#define DMS_MODULE_SPOD_INFO "dms_spod_info"
BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_SPOD_INFO)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_SPOD_INFO, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD,
    "main_cmd=0xc,sub_cmd=0x1", NULL, DMS_SUPPORT_ALL_USER, dms_get_spod_info)
ADD_FEATURE_COMMAND(DMS_MODULE_SPOD_INFO, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD,
    "main_cmd=0xc,sub_cmd=0x2", NULL,
    DMS_ACC_ALL | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT,
    dms_get_spod_node_status)
ADD_FEATURE_COMMAND(DMS_MODULE_SPOD_INFO, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD,
    "main_cmd=0xc,sub_cmd=0x2", NULL,
    DMS_ACC_ROOT_ONLY | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT,
    dms_set_spod_node_status)
ADD_FEATURE_COMMAND(DMS_MODULE_SPOD_INFO, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD,
    "main_cmd=0x10,sub_cmd=0x2", NULL, DMS_SUPPORT_ALL, dms_get_spod_ping_info)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION();

#define DMS_SPOD_MAX_SERVER_NUM (48)
#define DMS_SPOD_MAX_CHIP_NUM (8)
#define DMS_SPOD_MAX_DIE_NUM (2)
#define DMS_SPOD_MAX_UDEVID_NUM (DMS_SPOD_MAX_CHIP_NUM * DMS_SPOD_MAX_DIE_NUM)
#define DMS_SPOD_MAX_NUM (DMS_SPOD_MAX_SERVER_NUM * DMS_SPOD_MAX_UDEVID_NUM)

#define DMS_SPOD_CHECK_SDID(sdid_parse_info, ret)                                                                  \
    do {                                                                                                           \
        if ((sdid_parse_info.server_id >= DMS_SPOD_MAX_SERVER_NUM) ||                                              \
            (sdid_parse_info.chip_id >= DMS_SPOD_MAX_CHIP_NUM) ||                                                  \
            (sdid_parse_info.die_id >= DMS_SPOD_MAX_DIE_NUM) ||                                                    \
            (sdid_parse_info.udevid >= DMS_SPOD_MAX_UDEVID_NUM) ||                                                 \
            (sdid_parse_info.chip_id * DMS_SPOD_MAX_DIE_NUM + sdid_parse_info.die_id != sdid_parse_info.udevid)) { \
            ret = -EINVAL;                                                                                         \
        }                                                                                                          \
    }while(0)

struct spod_node_status {
    u8 status[DMS_SPOD_MAX_NUM];
};

struct sdid_status {
    unsigned int sdid;
    HAL_KERNEL_SPOD_NODE_STATUS status;
};

typedef int (*dms_spod_notifier)(unsigned int udevid, unsigned int soc_type);
#ifdef CFG_HOST_ENV
STATIC struct spod_node_status *g_spod_node_status[DEVDRV_PF_DEV_MAX_NUM] = {NULL};
STATIC struct rw_semaphore g_spod_node_status_lock[DEVDRV_PF_DEV_MAX_NUM];
#endif

int dms_dev_info_args_check(const struct dms_get_device_info_in *input, const u32 in_len,
    const struct dms_get_device_info_out *output, const u32 out_len, const u32 min_buff)
{
    if (input == NULL || in_len != sizeof(struct dms_get_device_info_in)) {
        dms_err("Invalid parameter. (in_is_null=%d; in_len_invalid=%u; in_len_valid=%zu)\n",
            input == NULL, in_len, sizeof(struct dms_get_device_info_in));
        return -EINVAL;
    }

    if (input->buff == NULL || input->buff_size < min_buff) {
        dms_err("Invalid parameter. (devid=%u; buff_is_null=%d; buff_size_invalid=%u; min_size=%u)\n",
            input->dev_id, input->buff == NULL, input->buff_size, min_buff);
        return -EINVAL;
    }

    if (output == NULL || out_len != sizeof(struct dms_get_device_info_out)) {
        dms_err("Invalid parameter. (out_is_null=%d; out_len_invalid=%u; out_len_valid=%zu)\n",
            output == NULL, out_len, sizeof(struct dms_get_device_info_out));
        return -EINVAL;
    }

    return 0;
}

#ifdef CFG_HOST_ENV
static int dms_root_physical_check(u32 udevid)
{
    int ret;
    u32 host_flag;

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    /* user check and keep errno same with the dms. */
    if ((u32)(current->cred->euid.val) != 0) {
        dms_err("Operation not permitted. (udevid=%u)\n", udevid);
        return -EPERM;
    }
#endif

    if (devdrv_manager_is_pf_device(udevid) == false || devdrv_manager_container_is_in_container()) {
        return -EOPNOTSUPP;
    }

    ret = devdrv_get_host_phy_mach_flag(udevid, &host_flag);
    if (ret != 0) {
        dms_err("Failed to invoke devdrv_get_host_phy_mach_flag for physical-machine check. (udevid=%u)\n", udevid);
        return ret;
    }

    if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
        return -EOPNOTSUPP;
    }
    return 0;
}
#endif

int dms_get_spod_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int udevid = 0;
    struct dms_get_device_info_in *input = (struct dms_get_device_info_in *)in;
    struct dms_get_device_info_out *output = (struct dms_get_device_info_out *)out;
    struct spod_info spod_stru = {0};

    ret = dms_dev_info_args_check(input, in_len, output, out_len, sizeof(struct spod_info));
    if (ret != 0) {
        return ret;
    }

    ret = uda_devid_to_udevid(input->dev_id, &udevid);
    if (ret != 0) {
        dms_err("Failed to convert the logical ID to the physical ID. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    if (!devdrv_manager_is_pf_device(udevid)) {
        return -EOPNOTSUPP;
    }

    ret = dbl_get_spod_info(udevid, &spod_stru);
    if (ret != 0) {
        dms_err("Failed to get spod info. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }
    ret = copy_to_user(input->buff, &spod_stru, sizeof(spod_stru));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }
    output->out_size = sizeof(spod_stru);

    return 0;
}

int dms_get_spod_ping_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
#ifdef CFG_HOST_ENV
    int ret;
    u32 udevid = 0, sdid = 0;
    struct dms_get_device_info_in *input = (struct dms_get_device_info_in *)in;
    struct dms_get_device_info_out *output = (struct dms_get_device_info_out *)out;

    ret = dms_dev_info_args_check(input, in_len, output, out_len, sizeof(u32));
    if (ret != 0) {
        return ret;
    }

    ret = uda_devid_to_udevid(input->dev_id, &udevid);
    if (ret != 0) {
        dms_err("Failed to convert the logical ID to the physical ID. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    ret = dms_root_physical_check(udevid);
    if (ret != 0) {
        return ret;
    }

    ret = copy_from_user(&sdid, (void *)((uintptr_t)input->buff), sizeof(u32));
    if (ret != 0) {
        dms_err("Failed to invoke copy_from_user for sdid. (devid=%u; udevid=%u; ret=%d)\n",
            input->dev_id, udevid, ret);
        return ret;
    }

    ret = devdrv_s2s_npu_link_check(udevid, sdid);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to invoke devdrv_s2s_npu_link_check for hccs link status. (devid=%u)\n",
            udevid);
        return ret;
    }
    output->out_size = 0;

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

#ifdef CFG_HOST_ENV
STATIC int dms_spod_node_status_check_soc_type(unsigned int udevid)
{
    int ret;
    unsigned int soc_type = SOC_TYPE_MAX;

    ret = hal_kernel_get_soc_type(udevid, &soc_type);
    if (ret != 0) {
        return ret;
    }

    if (soc_type != SOC_TYPE_CLOUD_V3) {
        return -EOPNOTSUPP;
    }

    return 0;
}
#endif

int hal_kernel_get_spod_node_status(unsigned int local_udevid, unsigned int remote_sdid, unsigned int *status)
{
#ifdef CFG_HOST_ENV
    int ret = 0;
    struct sdid_parse_info sdid_info = {0};
    struct spod_node_status *nodes_status = NULL;
    int index = 0;

    if (!devdrv_manager_is_pf_device(local_udevid)) {
        return -EOPNOTSUPP;
    }

    if (local_udevid >= DEVDRV_PF_DEV_MAX_NUM) {
        return -EINVAL;
    }

    if (status == NULL) {
        return -EFAULT;
    }

    ret = dbl_parse_sdid(remote_sdid, &sdid_info);
    if (ret != 0) {
        return ret;
    }

    DMS_SPOD_CHECK_SDID(sdid_info, ret);
    if (ret != 0) {
        return -ERANGE;
    }

    ret = dms_spod_node_status_check_soc_type(local_udevid);
    if (ret != 0) {
        return (ret == -EOPNOTSUPP ? ret : -ENODEV);
    }

    index = sdid_info.server_id * DMS_SPOD_MAX_UDEVID_NUM + sdid_info.udevid;

    down_read(&g_spod_node_status_lock[local_udevid]);

    nodes_status = g_spod_node_status[local_udevid];
    if (nodes_status == NULL) {
        up_read(&g_spod_node_status_lock[local_udevid]);
        return -ENODATA;
    }

    *status = nodes_status->status[index];
    up_read(&g_spod_node_status_lock[local_udevid]);
    return 0;
#else
    (void)local_udevid;
    (void)remote_sdid;
    (void)status;
    return -EOPNOTSUPP;
#endif
}

STATIC int dms_spod_set_node_status(unsigned int local_udevid, unsigned int remote_sdid, unsigned int status)
{
#ifdef CFG_HOST_ENV
    int ret;
    struct sdid_parse_info sdid_info = {0};
    struct spod_node_status *nodes_status = NULL;
    unsigned int index = 0;

    if ((local_udevid >= DEVDRV_PF_DEV_MAX_NUM) || (status >= DMS_SPOD_NODE_STATUS_MAX)) {
        dms_err("Invalid parameter. (local_udevid=%u; status=%d)\n", local_udevid, status);
        return -EINVAL;
    }

    ret = dbl_parse_sdid(remote_sdid, &sdid_info);
    if (ret != 0) {
        dms_err("Failed to parse remote_sdid. (remote_sdid=0x%x; ret=%d)\n", remote_sdid, ret);
        return ret;
    }

    DMS_SPOD_CHECK_SDID(sdid_info, ret);
    if (ret != 0) {
        dms_err("Invalid sdid. (sdid_server_id=%u; sdid_chip_id=%u; sdid_die_id=%u; sdid_udevid=%u)\n",
                    sdid_info.server_id, sdid_info.chip_id, sdid_info.die_id, sdid_info.udevid);
        return ret;
    }

    ret = dms_spod_node_status_check_soc_type(local_udevid);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "check soc type failed. (local_udevid=%u; ret=%d)\n", local_udevid, ret);
        return ret;
    }

    index = sdid_info.server_id * DMS_SPOD_MAX_UDEVID_NUM + sdid_info.udevid;

    down_write(&g_spod_node_status_lock[local_udevid]);

    nodes_status = g_spod_node_status[local_udevid];
    if (nodes_status == NULL) {
        up_write(&g_spod_node_status_lock[local_udevid]);
        dms_err("Spod node status has not been inited. (local_udevid=%u)\n", local_udevid);
        return -ENODATA;
    }

    nodes_status->status[index] = status;

    up_write(&g_spod_node_status_lock[local_udevid]);

    return 0;
#else
    (void)local_udevid;
    (void)remote_sdid;
    (void)status;
    return -EOPNOTSUPP;
#endif
}

int dms_get_spod_node_status(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int udevid = 0;
    unsigned int sdid = 0;
    unsigned int status = 0;
    struct dms_get_device_info_in *input = (struct dms_get_device_info_in *)in;
    struct dms_get_device_info_out *output = (struct dms_get_device_info_out *)out;
    (void)feature;

    ret = dms_dev_info_args_check(input, in_len, output, out_len, sizeof(unsigned int));
    if (ret != 0) {
        return ret;
    }

    ret = uda_devid_to_udevid(input->dev_id, &udevid);
    if (ret != 0) {
        dms_err("Failed to convert the logical ID to the physical ID. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    ret = copy_from_user(&sdid, (void *)((uintptr_t)input->buff), sizeof(u32));
    if (ret != 0) {
        dms_err("Failed to invoke copy_from_user for sdid. (devid=%u; udevid=%u; ret=%d)\n",
            input->dev_id, udevid, ret);
        return ret;
    }

    ret = hal_kernel_get_spod_node_status(udevid, sdid, &status);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to get sdid's status. (udevid=%u; sdid=0x%x; ret=%d)\n",
            udevid, sdid, ret);
         return (ret != -ERANGE ? ret : -EINVAL);
    }

    ret = copy_to_user((void *)((uintptr_t)input->buff), &status, sizeof(u32));
    if (ret != 0) {
        dms_err("Failed to invoke copy_to_user for status. (devid=%u; udevid=%u; ret=%d)\n",
            input->dev_id, udevid, ret);
        return ret;
    }
    output->out_size = sizeof(u32);
    return 0;
}

int dms_set_spod_node_status(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int udevid = 0;
    struct sdid_status para = {0};
    struct dms_set_device_info_in *input = (struct dms_set_device_info_in *)in;
    (void)feature;
    (void)out;
    (void)out_len;

    if ((input == NULL) || (in_len != sizeof(struct dms_set_device_info_in))) {
        dms_err("Invalid parameter. (in_is_null=%d; in_len_invalid=%u; in_len_valid=%zu)\n",
            input == NULL, in_len, sizeof(struct dms_set_device_info_in));
        return -EINVAL;
    }

    if ((input->buff == NULL) || (input->buff_size < sizeof(struct sdid_status))) {
        dms_err("Invalid parameter. (devid=%u; buff_is_null=%d; buff_size_invalid=%u; min_size=%zu)\n",
            input->dev_id, input->buff == NULL, input->buff_size, sizeof(struct sdid_status));
        return -EINVAL;
    }

    ret = uda_devid_to_udevid(input->dev_id, &udevid);
    if (ret != 0) {
        dms_err("Failed to convert the logical ID to the physical ID. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    ret = copy_from_user(&para, (void *)((uintptr_t)input->buff), sizeof(struct sdid_status));
    if (ret != 0) {
        dms_err("Failed to invoke copy_from_user for sdid and status. (devid=%u; udevid=%u; ret=%d)\n",
            input->dev_id, udevid, ret);
        return ret;
    }

    ret = dms_spod_set_node_status(udevid, para.sdid, para.status);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to set sdid's status. (udevid=%u; sdid=0x%x; ret=%d)\n",
            udevid, para.sdid, ret);
    }
    return ret;
}

STATIC int dms_spod_node_status_init(unsigned int udevid, unsigned int soc_type)
{
#ifdef CFG_HOST_ENV
    struct spod_node_status *nodes_status = NULL;

    if ((soc_type != SOC_TYPE_CLOUD_V3) || (!devdrv_manager_is_pf_device(udevid))) {
        return 0;
    }

    down_write(&g_spod_node_status_lock[udevid]);

    if (g_spod_node_status[udevid] != NULL) {
        up_write(&g_spod_node_status_lock[udevid]);
        dms_info("Spod node status has been inited. (udevid=%u)", udevid);
        return 0;
    }

    nodes_status = (struct spod_node_status *)dbl_kzalloc(sizeof(struct spod_node_status), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (nodes_status == NULL) {
        up_write(&g_spod_node_status_lock[udevid]);
        dms_err("malloc spod node status failed. (udevid=%u)", udevid);
        return -ENOMEM;
    }

    g_spod_node_status[udevid] = nodes_status;
    up_write(&g_spod_node_status_lock[udevid]);
#endif
    return 0;
}

STATIC void dms_spod_node_status_uninit(unsigned int udevid, unsigned int soc_type)
{
#ifdef CFG_HOST_ENV
    if ((soc_type != SOC_TYPE_CLOUD_V3) || (!devdrv_manager_is_pf_device(udevid))) {
        return;
    }

    down_write(&g_spod_node_status_lock[udevid]);

    if (g_spod_node_status[udevid] == NULL) {
        up_write(&g_spod_node_status_lock[udevid]);
        return;
    }

    dbl_kfree(g_spod_node_status[udevid]);
    g_spod_node_status[udevid] = NULL;

    up_write(&g_spod_node_status_lock[udevid]);
#endif
    return;
}

STATIC int dms_spod_device_up_notify(unsigned int udevid, unsigned int soc_type)
{
    int ret = 0;

    if (udevid >= DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = dms_spod_node_status_init(udevid, soc_type);
    if (ret != 0) {
        dms_err("Call dms spod node status init failed. (udevid=%u; soc_type=%u; ret=%d)\n",
            udevid, soc_type, ret);
    }

    return ret;
}

STATIC int dms_spod_device_down_notify(unsigned int udevid, unsigned int soc_type)
{
    int ret = 0;

    if (udevid >= DEVDRV_MAX_DAVINCI_NUM) {
        dms_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    dms_spod_node_status_uninit(udevid, soc_type);
    return ret;
}

STATIC dms_spod_notifier dms_spod_notifier_handle_func[DMS_DEVICE_NOTIFIER_MAX] = {
    [DMS_DEVICE_UP3] = dms_spod_device_up_notify,
    [DMS_DEVICE_DOWN3] = dms_spod_device_down_notify,
};

STATIC int dms_spod_notifier_handle(struct notifier_block *nb, unsigned long mode, void *data)
{
    int ret;
    unsigned int soc_type = SOC_TYPE_MAX;
    struct devdrv_info *dev_info = (struct devdrv_info *)data;

    if ((data == NULL) || (mode == DMS_DEVICE_NOTIFIER_MIN) ||
        (mode >= DMS_DEVICE_NOTIFIER_MAX)) {
        dms_err("Invalid parameter. (mode=0x%lx; data=\"%s\")\n",
                mode, data == NULL ? "NULL" : "OK");
        return -EINVAL;
    }

    if (mode != DMS_DEVICE_UP3 && mode != DMS_DEVICE_DOWN3) {
        return 0;
    }

    soc_type = (dev_info->multi_die == 1) ? SOC_TYPE_CLOUD_V3 : CHIP_CLOUD_V2;

    ret = dms_spod_notifier_handle_func[mode](dev_info->dev_id, soc_type);
    if (ret != 0) {
        dms_err("Call dms spod notifier handle func failed. (dev_id=%u; mode=%ld; ret=%d)\n",
            dev_info->dev_id, mode, ret);
        return ret;
    }

    return 0;
}

STATIC struct notifier_block g_dms_spod_notifier = {
    .notifier_call = dms_spod_notifier_handle,
};

STATIC int dms_spod_status_module_init(void)
{
    int ret = 0;

#ifdef CFG_HOST_ENV
    int i;
    for (i = 0; i < DEVDRV_PF_DEV_MAX_NUM; ++i) {
        init_rwsem(&g_spod_node_status_lock[i]);
    }
#endif

    ret = dms_register_notifier(&g_dms_spod_notifier);
    if (ret != 0) {
        dms_err("register dms spod notifier failed. (ret=%d)\n", ret);
    }

    return ret;
}

STATIC void dms_spod_status_module_exit(void)
{
    (void)dms_unregister_notifier(&g_dms_spod_notifier);
}

int dms_spod_init(void)
{
    int ret;

    dms_info("dms spod info init.\n");
    CALL_INIT_MODULE(DMS_MODULE_SPOD_INFO);
    ret = dms_spod_status_module_init();
    if (ret != 0) {
        dms_err("call dms_spod_module_init failed.  (ret=%d)\n", ret);
    }
    return ret;
}
DECLAER_FEATURE_AUTO_INIT(dms_spod_init, FEATURE_LOADER_STAGE_5);

void dms_spod_exit(void)
{
    dms_info("dms spod info exit.\n");
    dms_spod_status_module_exit();
    CALL_EXIT_MODULE(DMS_MODULE_SPOD_INFO);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_spod_exit, FEATURE_LOADER_STAGE_5);


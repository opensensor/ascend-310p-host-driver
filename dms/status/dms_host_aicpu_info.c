/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
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
* Create: 2023-02-18
*/

#include <linux/vmalloc.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "ka_fs_pub.h"
#include "ka_base_pub.h"
#include "ka_memory_pub.h"

#include "dms_define.h"
#include "dms_template.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dbl_mem_alloc_interface.h"
#include "devdrv_user_common.h"
#include "dms_basic_info.h"
#include "dms_host_aicpu_info.h"

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_HOST_AICPU)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_HOST_AICPU, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD,
    "main_cmd=0x11", NULL, DMS_SUPPORT_ROOT_ONLY, dms_set_host_aicpu_info)
ADD_FEATURE_COMMAND(DMS_MODULE_HOST_AICPU, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD,
    "main_cmd=0x11", NULL, DMS_SUPPORT_ALL, dms_get_host_aicpu_info)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

struct dms_host_aicpu_info *g_host_aicpu_info = NULL;
static unsigned int g_host_aicpu_freq;

int dms_set_host_aicpu_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int phy_id = 0;
    unsigned int vf_id = 0;
    struct dms_set_device_info_in *input = NULL;

    if ((in == NULL) || (in_len != sizeof(struct dms_set_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_set_device_info_in *)in;
    if ((input->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (input->buff == NULL) ||
        (input->buff_size < sizeof(struct dms_host_aicpu_info))) {
        dms_err("Invalid input parameter. (dev_id=%u; buff=%d; buff_size=%u)\n",
            input->dev_id, (input->buff != NULL), input->buff_size);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(input->dev_id, &phy_id, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical ID to the physical ID. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    ret = copy_from_user(&g_host_aicpu_info[phy_id], (void*)((uintptr_t)input->buff),
        sizeof(struct dms_host_aicpu_info));
    if (ret != 0) {
        dms_err("copy_from_user failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC char* dms_read_cpuinfo_file(void)
{
    struct file *fp = NULL;
    size_t buf_len = CPUINFO_FILE_LEN;
    ssize_t read_len;
    char *buf = NULL;
    loff_t pos = 0;

    fp = filp_open("/proc/cpuinfo", KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL(fp)) {
        dms_err("Unable to open file. (errno=%ld)\n", PTR_ERR(fp));
        return NULL;
    }

    buf = dbl_vmalloc(buf_len + 1, KA_GFP_KERNEL|__KA_GFP_HIGHMEM|__KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (buf == NULL) {
        dms_err("malloc failed.\n");
        (void)filp_close(fp, NULL);
        return NULL;
    }

    read_len = ka_fs_kernel_read(fp, buf, buf_len, &pos);
    if (read_len <= 0) {
        dms_err("Kernel read fail. (read len=%ld)\n", read_len);
        (void)filp_close(fp, NULL);
        dbl_vfree(buf);
        return NULL;
    }

    (void)filp_close(fp, NULL);
    buf[read_len] = '\0';
    return buf;
}

STATIC void dms_free_memory(char *buf)
{
    if (buf != NULL) {
        dbl_vfree(buf);
        buf = NULL;
    }
}

/*
* Obtain the value of cpu MHz from the /proc/cpuinfo file. The format: cpu MHz : xxxx.xx.
*/
STATIC int dms_get_host_aicpu_frequency(void)
{
    char match_str[] = "cpu MHz";
    char* buf = NULL;
    unsigned int aicpu_freq = 0;
    int pos = 0;

    buf = dms_read_cpuinfo_file();
    if (buf == NULL) {
        dms_err("Read /proc/cpuinfo failed.\n");
        return -EINVAL;
    }

    while ((buf[pos] != '\0') && (ka_base_strncmp(match_str, &buf[pos], ka_base_strlen(match_str)) != 0)) {
        pos++;
    }

    while ((buf[pos] != '\0') && (buf[pos] <= '0' || buf[pos] > '9')) {
        pos++;
    }

    while ((buf[pos] != '\0') && (buf[pos] != '.')) {
        aicpu_freq = aicpu_freq * 10 + (buf[pos] - '0'); /* 10:char-to-int */
        pos++;
    }

    dms_free_memory(buf);
    g_host_aicpu_freq = aicpu_freq;

    return 0;
}

int dms_get_host_aicpu_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int phy_id = 0;
    unsigned int vf_id = 0;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = NULL;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    input = (struct dms_get_device_info_in *)in;
    if ((input->dev_id >= DEVDRV_MAX_DAVINCI_NUM) || (input->buff == NULL) ||
        (input->buff_size < sizeof(struct dms_host_aicpu_info))) {
        dms_err("Invalid input parameter. (dev_id=%u; buff=%d; buff_size=%u)\n",
            input->dev_id, (input->buff != NULL), input->buff_size);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }
    output = (struct dms_get_device_info_out *)out;

    ret = devdrv_manager_container_logical_id_to_physical_id(input->dev_id, &phy_id, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical ID to the physical ID. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    g_host_aicpu_info[phy_id].frequency = (g_host_aicpu_info[phy_id].num == 0) ? 0 : g_host_aicpu_freq;

    ret = copy_to_user((void *)((uintptr_t)input->buff), &g_host_aicpu_info[phy_id],
        sizeof(struct dms_host_aicpu_info));
    if (ret != 0) {
        dms_err("copy_to_user failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }
    output->out_size = input->buff_size;

    return 0;
}

int dms_host_aicpu_init(void)
{
    int ret = 0;
    dms_info("dms host aicpu init.\n");
    ret = dms_get_host_aicpu_frequency();
    if (ret != 0) {
        dms_err("Get host aicpu frequency failed. (ret=%d)\n", ret);
        return ret;
    }
    g_host_aicpu_info = dbl_kzalloc(sizeof(struct dms_host_aicpu_info) * DEVDRV_MAX_DAVINCI_NUM,
        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (g_host_aicpu_info == NULL) {
        dms_err("kzalloc failed.\n");
        return -EINVAL;
    }
    CALL_INIT_MODULE(DMS_MODULE_HOST_AICPU);
    return 0;
}

void dms_host_aicpu_exit(void)
{
    dms_info("dms host aicpu exit.\n");
    dbl_kfree(g_host_aicpu_info);
    g_host_aicpu_info = NULL;
    CALL_EXIT_MODULE(DMS_MODULE_HOST_AICPU);
}

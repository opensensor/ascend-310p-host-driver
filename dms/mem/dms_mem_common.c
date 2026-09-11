/*
* Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
* Create: 2022-11-15
*/
#include <linux/fs.h>
#include <linux/mmzone.h>
#include <linux/version.h>
#include <linux/slab.h>

#include "ka_memory_pub.h"
#include "ka_fs_pub.h"

#include "securec.h"
#include "dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms_mem_info.h"
#include "dms_mem_common.h"
#include "dbl_mem_alloc_interface.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
ssize_t mem_read_file(struct file *fp, char *dst_addr, loff_t fsize, loff_t *pos)
{
    return kernel_read(fp, dst_addr, fsize, pos);
}
#else
ssize_t mem_read_file(struct file *fp, char *dst_addr, loff_t fsize, loff_t *pos)
{
    mm_segment_t old_fs;
    ssize_t ret;

    old_fs = get_fs();
    /*lint -emacro(501,KERNEL_DS)*/
    set_fs((mm_segment_t)KERNEL_DS); /*lint!e501*/
    ret = vfs_read(fp, dst_addr, fsize, pos);
    set_fs(old_fs);
    return ret;
}
#endif

STATIC char *mem_read_line(struct file *fp, char *buf, unsigned int buf_len)
{
    long ret;
    unsigned int i = 0;

    ret = mem_read_file(fp, buf, buf_len - 1, &fp->f_pos);
    if (ret <= 0) {
        dms_err("File filestring not right. (ret=%ld; buf_len=%u)\n", ret, buf_len);
        return NULL;
    }

    while (buf[i++] != '\n') {
        if (i >= ret) {
            break;
        }
    }

    if (i < ret) {
        fp->f_pos += i - ret;
    }

    if (i < buf_len) {
        buf[i] = '\0';
    }

    return buf;
}

STATIC int mem_info_read_line(struct file *fp, u64 *read_value)
{
    char mem_buf[MEMINFO_PATH_LEN] = {0};
    char tmp[MEMINFO_PATH_LEN] = {0};
    u64 hbm_size;
    int ret;

    if (mem_read_line(fp, mem_buf, MEMINFO_PATH_LEN) == NULL) {
        dms_err("Read line failed.\n");
        return -EINVAL;
    }

    ret = sscanf_s(mem_buf, "%s %s %s %llu %s",
        tmp, sizeof(tmp), tmp, sizeof(tmp), tmp, sizeof(tmp), &hbm_size, tmp, sizeof(tmp));
    if (ret != MEMINFO_READ_INFO_NUM) {
        dms_err("File sscanf not right. (file=%s, ret=%d)\n", mem_buf, ret);
        return -EINVAL;
    }
    *read_value = hbm_size;
    return 0;
}

STATIC int mem_info_read_hugepage_line(struct file *fp, u32 *read_value)
{
    char mem_buf[MEMINFO_PATH_LEN] = {0};
    char tmp[MEMINFO_PATH_LEN] = {0};
    u32 hugepage_value;
    int ret;

    if (mem_read_line(fp, mem_buf, MEMINFO_PATH_LEN) == NULL) {
        dms_err("read line failed.\n");
        return -EINVAL;
    }

    ret = sscanf_s(mem_buf, "%s %s %s %u",
        tmp, sizeof(tmp), tmp, sizeof(tmp), tmp, sizeof(tmp), &hugepage_value);
    if (ret != MEMINFO_HUGEPAGE_READ_INFO_NUM) {
        dms_err("file %s sscanf not right, ret=%d.\n", mem_buf, ret);
        return -EINVAL;
    }
    *read_value = hugepage_value;
    return 0;
}

int mem_get_info_single(u32 node_index, u64 *total_size, u64 *free_size)
{
    char mem_buf[MEMINFO_PATH_LEN] = {0};
    char node_name[MEMINFO_PATH_LEN] = {0};
    struct file *fp = NULL;
    u32 hugepage_free_num = 0;
    int cnt;
    int ret;

    ret = sprintf_s(node_name, MEMINFO_PATH_LEN, "/sys/devices/system/node/node%u/meminfo", node_index);
    if (ret < 0) {
        dms_err("sprintf_s failed. (ret=%d; node=%u)\n", ret, node_index);
        return -EINVAL;
    }

    fp = filp_open(node_name, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL((void const *)fp)) {
        dms_err("File open failed. (file=%s; errno=%ld)\n", node_name, PTR_ERR((void const *)fp));
        return -EINVAL;
    }

    /* read first row line */
    ret = mem_info_read_line(fp, total_size);
    if (ret != 0) {
        (void)filp_close(fp, NULL);
        return ret;
    }

    /* read second row line */
    ret = mem_info_read_line(fp, free_size);
    if (ret != 0) {
        (void)filp_close(fp, NULL);
        return ret;
    }

    /* skip off the unused line */
    for (cnt = 0; cnt < UNUSED_MEMINFO_LINE_NUM; cnt++) {
        if (mem_read_line(fp, mem_buf, MEMINFO_PATH_LEN) == NULL) {
            (void)filp_close(fp, NULL);
            return -EINVAL;
        }
    }

    /* read hugepage free row line */
    ret = mem_info_read_hugepage_line(fp, &hugepage_free_num);
    if (ret != 0) {
        (void)filp_close(fp, NULL);
        return ret;
    }

#ifndef CFG_FEATURE_GET_HPAGE_2M_INFO
    *free_size = *free_size + hugepage_free_num * HUGE_PAGE_UNIT_SIZE;
#endif

    (void)filp_close(fp, NULL);
    return 0;
}

typedef enum {
    HUGE_PAGE_TYPE_2M_TOTAL = 0,
    HUGE_PAGE_TYPE_2M_FREE,
    HUGE_PAGE_TYPE_1G_TOTAL,
    HUGE_PAGE_TYPE_1G_FREE,
    HUGE_PAGE_TYPE_MAX
} HUGE_PAGE_TYPE;

#define HUGE_PAGE_2M_TOTAL_PATH "/sys/devices/system/node/node%u/hugepages/hugepages-2048kB/nr_hugepages"
#define HUGE_PAGE_2M_FREE_PATH "/sys/devices/system/node/node%u/hugepages/hugepages-2048kB/free_hugepages"
#define HUGE_PAGE_1G_TOTAL_PATH "/sys/devices/system/node/node%u/hugepages/hugepages-1048576kB/nr_hugepages"
#define HUGE_PAGE_1G_FREE_PATH "/sys/devices/system/node/node%u/hugepages/hugepages-1048576kB/free_hugepages"
const char *huge_page_path[HUGE_PAGE_TYPE_MAX] = {HUGE_PAGE_2M_TOTAL_PATH, HUGE_PAGE_2M_FREE_PATH,\
    HUGE_PAGE_1G_TOTAL_PATH, HUGE_PAGE_1G_FREE_PATH};

STATIC int mem_get_huge_page_info_single(u32 node_index, HUGE_PAGE_TYPE hpage_type, u64 *hpage_num)
{
    char hpage_info_buf[MEMINFO_PATH_LEN] = {0};
    char node_name[MEMINFO_PATH_LEN] = {0};
    struct file *fp = NULL;
    u64 hugepage_temp_num = 0;
    int ret = 0;

    switch (hpage_type) {
        case HUGE_PAGE_TYPE_2M_TOTAL:
            ret = sprintf_s(node_name, MEMINFO_PATH_LEN, HUGE_PAGE_2M_TOTAL_PATH, node_index);
            break;
        case HUGE_PAGE_TYPE_2M_FREE:
            ret = sprintf_s(node_name, MEMINFO_PATH_LEN, HUGE_PAGE_2M_FREE_PATH, node_index);
            break;
        case HUGE_PAGE_TYPE_1G_TOTAL:
            ret = sprintf_s(node_name, MEMINFO_PATH_LEN, HUGE_PAGE_1G_TOTAL_PATH, node_index);
            break;
        case HUGE_PAGE_TYPE_1G_FREE:
            ret = sprintf_s(node_name, MEMINFO_PATH_LEN, HUGE_PAGE_1G_FREE_PATH, node_index);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    
    if (ret < 0) {
        dms_err("sprintf_s failed. (ret=%d; node=%u; huge_page_type=%d)\n", ret, node_index, hpage_type);
        return -EINVAL;
    }

    fp = filp_open(node_name, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL((void const *)fp)) {
        dms_err("File open failed. (file=%s; errno=%ld)\n", node_name, PTR_ERR((void const *)fp));
        return -EINVAL;
    }

    if (mem_read_line(fp, hpage_info_buf, MEMINFO_PATH_LEN) == NULL) {
        dms_err("Read line failed.(file=%s)\n", node_name);
        ret = -EINVAL;
        goto free_file;
    }

    ret = sscanf_s(hpage_info_buf, "%llu", &hugepage_temp_num);
    if (ret != 1) {
        dms_err("File sscanf not right. (file=%s; ret=%d)\n", node_name, ret);
        ret = -EINVAL;
        goto free_file;
    }

    *hpage_num = hugepage_temp_num;
    ret = 0;

free_file:
    (void)filp_close(fp, NULL);
    fp = NULL;
    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
STATIC int mem_get_hpage_1G_info_single(u32 node_index, struct mem_hpage_1G *hpage_1G_info)
{
    int ret;

    ret = mem_get_huge_page_info_single(node_index, HUGE_PAGE_TYPE_1G_TOTAL, &hpage_1G_info->huge_1G_total);
    if (ret != 0) {
        return ret;
    }

    ret = mem_get_huge_page_info_single(node_index, HUGE_PAGE_TYPE_1G_FREE, &hpage_1G_info->huge_1G_free);
    if (ret != 0) {
        return ret;
    }

    ret = mem_get_huge_page_info_single(node_index, HUGE_PAGE_TYPE_2M_TOTAL, &hpage_1G_info->huge_2M_total);
    if (ret != 0) {
        return ret;
    }

    ret = mem_get_huge_page_info_single(node_index, HUGE_PAGE_TYPE_2M_FREE, &hpage_1G_info->huge_2M_free);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
#else
#define DMS_HUGE_CONTROL_MAX_LEN 4096
#define DMS_HUGE_PATH_LEN 64
#define DMS_TARGET_NODE_LEN 64
#define DMS_TMP_BUF_LEN 64
#define DMS_HUGE_CONTROL_NODE_STR "node: %d"
#define DMS_HUGE_1G_PATH "/proc/huge_control"
#define DMS_HUGE_1G_MEMBER_NUM 2
STATIC int mem_get_hpage_info_from_string(char *file_string, char *target_string, u64 *hpage_num)
{
    char *p_str = NULL;
    u64 tmp_num = 0;
    int ret;
    char tmp[DMS_TMP_BUF_LEN] = {0};

    if (file_string == NULL || target_string == NULL ||  hpage_num == NULL) {
        dms_err("Invalid parameter. (file_string_is_NULL=%d, target_string_is_NULL=%d; hpage_num_is_NULL=%d)\n",
            (file_string == NULL), (target_string == NULL), (hpage_num == NULL));
        return -EINVAL;
    }

    p_str = strstr(file_string, target_string);
    if (p_str == NULL) {
        dms_err("Can not find string. (target_string=%s)\n", target_string);
        return -EINVAL;
    }

    ret = sscanf_s(p_str, "%s %llu", tmp, DMS_TMP_BUF_LEN - 1, &tmp_num);
    if (ret != DMS_HUGE_1G_MEMBER_NUM) {
        dms_err("sscanf_s not right. (ret=%d)\n", ret);
        return -EINVAL;
    }

    *hpage_num = tmp_num;
    return 0;
}

STATIC int mem_get_hpage_1G_info_summary(char *node_line, struct mem_hpage_1G *hpage_1G_info)
{
    int ret = 0;
    u64 huge_1G_total_num = 0;
    u64 huge_2M_total_num = 0;
    u64 huge_2M_free_num = 0;
    u64 huge_1G_free_num = 0;

    if (node_line == NULL || hpage_1G_info == NULL) {
        dms_err("Invalid parameter. (node_line_is_NULL=%d; hpage_1G_info_is_NULL=%d)\n",
            (node_line == NULL), (hpage_1G_info == NULL));
        return -EINVAL;
    }

    ret = mem_get_hpage_info_from_string(node_line, "Huge_2M_total:", &huge_2M_total_num);
    if (ret != 0) {
        dms_err("Get Huge_2M_total info failed. (ret=%d)\n", ret);
        return ret;
    }
    ret = mem_get_hpage_info_from_string(node_line, "Huge_2M_free:", &huge_2M_free_num);
    if (ret != 0) {
        dms_err("Get Huge_2M_free info failed. (ret=%d)\n", ret);
        return ret;
    }
    ret = mem_get_hpage_info_from_string(node_line, "Huge_1G_total:", &huge_1G_total_num);
    if (ret != 0) {
        dms_err("Get Huge_1G_total info failed. (ret=%d)\n", ret);
        return ret;
    }
    ret = mem_get_hpage_info_from_string(node_line, "Huge_1G_free:", &huge_1G_free_num);
    if (ret != 0) {
        dms_err("Get Huge_1G_free info failed. (ret=%d)\n", ret);
        return ret;
    }

    hpage_1G_info->huge_1G_total = huge_1G_total_num;
    hpage_1G_info->huge_1G_free = huge_1G_free_num;
    hpage_1G_info->huge_2M_total = huge_2M_total_num;
    hpage_1G_info->huge_2M_free = huge_2M_free_num;

    return 0;
}

/*
 * Example of the file="/proc_huge_control" node35 huge page info:
 * ... ...
 * node: 35
 * Huge_2M_total:   512
 * Huge_2M_free :   511
 * Huge_1G_total:   2
 * Huge_1G_free :   1
 * Huge_1G_split:   1
 * node: 36
 * Huge_2M_total:   0
 * ... ...
 * total_hugepage_1G_num: Huge_1G_total
 * free_hugepage_1G_num: Huge_1G_free + Huge_2M_free
 */
STATIC int mem_get_hpage_1G_info_single(u32 node_index, struct mem_hpage_1G *hpage_1G_info)
{
    int ret;
    loff_t pos = 0;
    char *buffer = NULL;
    struct file *fp = NULL;
    char *node_line = NULL;
    char target_node[DMS_TARGET_NODE_LEN] = {0};
    char huge_path[DMS_HUGE_PATH_LEN] = DMS_HUGE_1G_PATH;

    fp = filp_open(huge_path, KA_O_RDONLY, 0);
    if (IS_ERR_OR_NULL((void const *)fp)) {
        dms_err("Huge page info file open failed. (file=\"%s\", errno=%ld)\n", huge_path, PTR_ERR((void const *)fp));
        return -EINVAL;
    }

    buffer = (char*)dbl_kzalloc(DMS_HUGE_CONTROL_MAX_LEN, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buffer == NULL) {
        dms_err("kzalloc failed. (node_index=%u)\n", node_index);
        ret = -ENOMEM;
        goto free_file;
    }

    ret = mem_read_file(fp, buffer, DMS_HUGE_CONTROL_MAX_LEN - 1, &pos);
    if (ret < 0) {
        dms_err("File read failed. (file=\"%s\"; node_index=%u, ret=%d)\n", huge_path, node_index, ret);
        ret = -EINVAL;
        goto free_buf;
    }

    ret = sprintf_s(target_node, DMS_TARGET_NODE_LEN, DMS_HUGE_CONTROL_NODE_STR, node_index);
    if (ret < 0) {
        dms_err("sprintf_s failed. (node_index=%u)\n", node_index);
        ret = -EINVAL;
        goto free_buf;
    }

    node_line = strstr(buffer, target_node);
    if (node_line == NULL) {
        dms_err("Can not find target node. (file=\"%s\"; target_node=\"%s\")\n", huge_path, target_node);
        ret = -EINVAL;
        goto free_buf;
    }

    ret = mem_get_hpage_1G_info_summary(node_line, hpage_1G_info);
    if (ret != 0) {
        dms_err("Can not get huge page 1G info. (file=\"%s\"; target_node=\"%s\"; ret=%d)\n",
            huge_path, target_node, ret);
        goto free_buf;
    }

free_buf:
    dbl_kfree(buffer);
    buffer = NULL;
free_file:
    filp_close(fp, NULL);
    fp = NULL;
    return ret;
}
#endif

int mem_get_hpage_info(u32 node_index, u64 *hpage_2M_num, struct mem_hpage_1G *hpage_1G_info)
{
    int ret = 0;

    if (hpage_2M_num == NULL || hpage_1G_info == NULL) {
        dms_err("Invalid parameter. (hpage_2M_num_is_NULL=%d, hpage_1G_info_is_NULL=%d)\n",
            (hpage_2M_num == NULL), (hpage_1G_info == NULL));
        return -EINVAL;
    }

    ret = mem_get_huge_page_info_single(node_index, HUGE_PAGE_TYPE_2M_TOTAL, hpage_2M_num);
    if (ret != 0) {
        return ret;
    }

    ret = mem_get_hpage_1G_info_single(node_index, hpage_1G_info);
    if (ret != 0) {
        return ret;
    }

    return ret;
}
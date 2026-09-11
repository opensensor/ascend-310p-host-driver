/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
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
* Create: 2019-10-15
*/

#include <linux/types.h>

#include "securec.h"
#include "drv_type.h"
#include "dms_interface.h"
#include "dms_define.h"
#include "dms_event_dfx.h"
#include "ka_kernel_def_pub.h"

#define EVENT_DFX_SCANF_ONE_ELEMENT 1
#define EVENT_DFX_SCANF_TWO_ELEMENT 2

struct dms_event_dfx_sysfs {
    u32 channel_devid;
    u32 event_list_devid;
    u32 mask_list_devid;
    u32 diagrams_devid;
    char diagrams_opt;
};

static struct dms_event_dfx_sysfs g_event_dfx = {
    .channel_devid = 0,
    .event_list_devid = 0,
    .mask_list_devid = 0,
    .diagrams_devid = 0,
    .diagrams_opt = 'a'
};

ssize_t dms_event_dfx_channel_flux_store(const char *buf, size_t count)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;

    EVENT_DFX_CHECK_DO_SOMETHING(buf == NULL, return count);
#ifndef AOS_LLVM_BUILD
    /* judge return value one element */
    if (sscanf_s(buf, "%u", &g_event_dfx.channel_devid) != EVENT_DFX_SCANF_ONE_ELEMENT) {
        dms_warn("Get dev_id warn. (dev_id=%u; buf=%.*s; count=%lu)\n", g_event_dfx.channel_devid,
            (int)count, buf, count);
        return count;
    }
#endif
    dev_cb = dms_get_dev_cb(g_event_dfx.channel_devid);
    if (dev_cb == NULL) {
        dms_warn("Get dev_ctrl block warn. (dev_id=%u)\n", g_event_dfx.channel_devid);
        return count;
    }

    dms_info("Store dfx channel flux succ. (dev_id=%u)\n", g_event_dfx.channel_devid);
    return count;
}
KA_EXPORT_SYMBOL(dms_event_dfx_channel_flux_store);

static int dms_event_dfx_channel_flux_show_item(const char *flux_name,
    const char *event_type, atomic_t *channel, char **str, int *avl_len)
{
    int len;

    len = snprintf_s(*str, *avl_len, *avl_len - 1, "%s[%s]:\t\t\t%d\n",
                     flux_name, event_type, atomic_read(channel));
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return len);
    *str += len;
    *avl_len -= len;

    return len;
}

/*
  Print channel flux example

  Print channel flux begin: (dev_id=0)
  recv_from_sensor[RESUME]:        100
  recv_from_sensor[OCCUR]:         100
  recv_from_sensor[ONE_TIME]:      100
  report_to_consumer[RESUME]:      100
  report_to_consumer[OCCUR]:       100
  report_to_consumer[ONE_TIME]:    100
  Print channel flux end.
 */
ssize_t dms_event_dfx_channel_flux_show(char *str)
{
    int len, ret, avl_len = EVENT_DFX_BUF_SIZE_MAX;
    struct dms_event_dfx_table *dfx_table = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    char *refill_buf = str;

    EVENT_DFX_CHECK_DO_SOMETHING(str == NULL, return 0);

    dev_cb = dms_get_dev_cb(g_event_dfx.channel_devid);
    if (dev_cb == NULL) {
        len = snprintf_s(str, avl_len, avl_len - 1, "Invalid device id. (dev_id=%u)\n", g_event_dfx.channel_devid);
        EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
        str += len;
        return str - refill_buf;
    }
    dfx_table = &dev_cb->dev_event_cb.dfx_table;

    len = snprintf_s(str, avl_len, avl_len - 1, "Print channel flux begin: (dev_id=%u)\n", g_event_dfx.channel_devid);
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;
    avl_len -= len;

    ret = dms_event_dfx_channel_flux_show_item("recv_from_sensor", "RESUME",
        &dfx_table->recv_from_sensor[DMS_EVENT_TYPE_RESUME], &str, &avl_len);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, goto out);

    ret = dms_event_dfx_channel_flux_show_item("recv_from_sensor", "OCCUR",
        &dfx_table->recv_from_sensor[DMS_EVENT_TYPE_OCCUR], &str, &avl_len);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, goto out);

    ret = dms_event_dfx_channel_flux_show_item("recv_from_sensor", "ONE_TIME",
        &dfx_table->recv_from_sensor[DMS_EVENT_TYPE_ONE_TIME], &str, &avl_len);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, goto out);

    ret = dms_event_dfx_channel_flux_show_item("report_to_consumer", "RESUME",
        &dfx_table->report_to_consumer[DMS_EVENT_TYPE_RESUME], &str, &avl_len);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, goto out);

    ret = dms_event_dfx_channel_flux_show_item("report_to_consumer", "OCCUR",
        &dfx_table->report_to_consumer[DMS_EVENT_TYPE_OCCUR], &str, &avl_len);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, goto out);

    ret = dms_event_dfx_channel_flux_show_item("report_to_consumer", "ONE_TIME",
        &dfx_table->report_to_consumer[DMS_EVENT_TYPE_ONE_TIME], &str, &avl_len);
    EVENT_DFX_CHECK_DO_SOMETHING(ret < 0, goto out);

    len = snprintf_s(str, avl_len, avl_len - 1, "Print channel flux end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    return str - refill_buf;
out:
    dms_warn("snprintf_s warn. (dev_id=%u)\n", g_event_dfx.channel_devid);
    return 0;
}
KA_EXPORT_SYMBOL(dms_event_dfx_channel_flux_show);

ssize_t dms_event_dfx_convergent_diagrams_store(const char *buf, size_t count)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;

    EVENT_DFX_CHECK_DO_SOMETHING(buf == NULL, return count);
#ifndef AOS_LLVM_BUILD
    /* judge return value two element */
    if (sscanf_s(buf, "%u %c", &g_event_dfx.diagrams_devid, &g_event_dfx.diagrams_opt,
                 sizeof(char)) != EVENT_DFX_SCANF_TWO_ELEMENT) {
        dms_warn("Get dev_id warn. (dev_id=%u; buf=%.*s; count=%lu)\n", g_event_dfx.diagrams_devid,
            (int)count, buf, count);
        return count;
    }
#endif
    dev_cb = dms_get_dev_cb(g_event_dfx.diagrams_devid);
    if (dev_cb == NULL) {
        dms_warn("Get dev_ctrl block warn. (dev_id=%u)\n", g_event_dfx.diagrams_devid);
        return count;
    }

    dms_info("Store dfx convergent diagrams succ. (dev_id=%u; opt='%c')\n",
             g_event_dfx.diagrams_devid, g_event_dfx.diagrams_opt);
    return count;
}
KA_EXPORT_SYMBOL(dms_event_dfx_convergent_diagrams_store);

/*
  convergent diagrams example
           |--- 0x10     |--- 0x7
           |--- 0x11     |--- 0x8      |--- 0x1(a:1)
  0x111 <--|--- 0x12     |--- 0x9      |--- 0x2
           |--- 0x789 <--|--- 0x123 <--|--- 0x3
                         |
                         |--- 0x4'  <--|--- 0x4
                                       |--- 0x5
                                       |--- 0x6

  Print convergent diagrams example
  Convergent diagrams begin: (dev_id=0)
  Top node: (event_id=0x111)
  0x111(a:1)
    |
  (0x111){0x10(a:0) - 0x11(a:0) - 0x12(a:0) - 0x789(a:1)}
    |
  (0x789){0x7(a:0) - 0x8(a:0) - 0x9(a:0) - 0x123(a:1) - 0x4'(a:0)}
    |
  (0x123){0x1(a:1) - 0x2(a:0) - 0x3(a:0)}  (0x4'){0x4(a:0) - 0x5(a:0) - 0x6(a:0)}
  ----------------------------------------------------------------------------------
  Convergent diagrams end.
 */
ssize_t dms_event_dfx_convergent_diagrams_show(char *str)
{
    EVENT_DFX_CHECK_DO_SOMETHING(str == NULL, return 0);
    return dms_event_print_convergent_diagrams(g_event_dfx.diagrams_devid, g_event_dfx.diagrams_opt, str);
}
KA_EXPORT_SYMBOL(dms_event_dfx_convergent_diagrams_show);

ssize_t dms_event_dfx_event_list_store(const char *buf, size_t count)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;

    EVENT_DFX_CHECK_DO_SOMETHING(buf == NULL, return count);
#ifndef AOS_LLVM_BUILD
    /* judge return value one element */
    if (sscanf_s(buf, "%u", &g_event_dfx.event_list_devid) != EVENT_DFX_SCANF_ONE_ELEMENT) {
        dms_warn("Get dev_id warn. (dev_id=%u; buf=%.*s; count=%lu)\n", g_event_dfx.event_list_devid,
            (int)count, buf, count);
        return count;
    }
#endif
    dev_cb = dms_get_dev_cb(g_event_dfx.event_list_devid);
    if (dev_cb == NULL) {
        dms_warn("Get dev_ctrl block warn. (dev_id=%u)\n", g_event_dfx.event_list_devid);
        return count;
    }

    dms_info("Store dfx event list succ. (dev_id=%u)\n", g_event_dfx.event_list_devid);
    return count;
}
KA_EXPORT_SYMBOL(dms_event_dfx_event_list_store);

/*
  Print convergent event_list example

  Print event list begin: (dev_id=0)
  device%u list: 0x111
  Print event list end. (dev_id=0)
 */
ssize_t dms_event_dfx_event_list_show(char *str)
{
    EVENT_DFX_CHECK_DO_SOMETHING(str == NULL, return 0);
    return dms_event_print_event_list(g_event_dfx.event_list_devid, str);
}
KA_EXPORT_SYMBOL(dms_event_dfx_event_list_show);

ssize_t dms_event_dfx_mask_list_store(const char *buf, size_t count)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;

    EVENT_DFX_CHECK_DO_SOMETHING(buf == NULL, return count);
#ifndef AOS_LLVM_BUILD
    /* judge return value one element */
    if (sscanf_s(buf, "%u", &g_event_dfx.mask_list_devid) != EVENT_DFX_SCANF_ONE_ELEMENT) {
        dms_warn("Get dev_id warn. (dev_id=%u; buf=%.*s; count=%lu)\n", g_event_dfx.mask_list_devid,
            (int)count, buf, count);
        return count;
    }
#endif
    dev_cb = dms_get_dev_cb(g_event_dfx.mask_list_devid);
    if (dev_cb == NULL) {
        dms_warn("Get dev_ctrl block warn. (dev_id=%u)\n", g_event_dfx.mask_list_devid);
        return count;
    }

    dms_info("Store dfx mask list succ. (dev_id=%u)\n", g_event_dfx.mask_list_devid);
    return count;
}
KA_EXPORT_SYMBOL(dms_event_dfx_mask_list_store);

/*
  Print convergent mask_list example

  Print mask list begin: (dev_id=0)
  device%u list: 0x111
  Print mask list end. (dev_id=0)
 */
ssize_t dms_event_dfx_mask_list_show(char *str)
{
    EVENT_DFX_CHECK_DO_SOMETHING(str == NULL, return 0);
    return dms_event_print_mask_list(g_event_dfx.mask_list_devid, str);
}
KA_EXPORT_SYMBOL(dms_event_dfx_mask_list_show);

/*
  Print subscribe handle function example

  Print subscribe handle function begin
  event handle[0]=0xFFFFFFF
  Print event list end.
 */
ssize_t dms_event_dfx_subscribe_handle_show(char *str)
{
    EVENT_DFX_CHECK_DO_SOMETHING(str == NULL, return 0);
    return dms_event_print_subscribe_handle(str);
}
KA_EXPORT_SYMBOL(dms_event_dfx_subscribe_handle_show);

/*
  Print subscribe handle function example

  Print subscribe process begin
  process[0]: tgid=100; pid=1000; event_num=1
  process[1]: tgid=101; pid=1001; event_num=2
  Print subscribe process end.
 */
ssize_t dms_event_dfx_subscribe_process_show(char *str)
{
    EVENT_DFX_CHECK_DO_SOMETHING(str == NULL, return 0);
    return dms_event_print_subscribe_process(str);
}
KA_EXPORT_SYMBOL(dms_event_dfx_subscribe_process_show);


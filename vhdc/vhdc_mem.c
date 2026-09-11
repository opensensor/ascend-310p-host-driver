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

#include "vhdc_mem.h"
#include "vhdc_agent.h"
#include "hdcdrv_mem_com.h"

#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_compiler_pub.h"

STATIC struct hdcdrv_mem_pool *vhdca_get_pool(struct vhdca_pdev *dev, int pool_type, u32 data_len)
{
    struct hdcdrv_mem_pool *pool = NULL;

    if (data_len <= (HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE)) {
        pool = &dev->small_mem_pool[pool_type];
    } else {
        pool = &dev->huge_mem_pool[pool_type];
    }

    return pool;
}

STATIC struct hdcdrv_mem_pool *vhdca_get_pool_by_segment(struct vhdca_pdev *dev, int pool_type, u32 seg_len)
{
    struct hdcdrv_mem_pool *pool = NULL;

    if (seg_len <= HDCDRV_SMALL_PACKET_SEGMENT) {
            pool = &dev->small_mem_pool[pool_type];
    } else {
            pool = &dev->huge_mem_pool[pool_type];
    }

    return pool;
}

int vhdca_alloc_mem(struct vhdca_alloc_mem_para *para, void **buf, ka_dma_addr_t *addr)
{
    struct vhdca_pdev *dev = &g_vhdca_ctrl->pdev[para->dev_id];
    struct hdcdrv_mem_pool *pool = NULL;
    int retry = 0;
    int ret;

    if ((para->pool_type < 0) || (para->pool_type >= HDCDRV_MEM_POOL_TYPE_NUM)) {
        hdcdrv_err("Input pararmeter is error. (pool_type=%d)\n", para->pool_type);
        return HDCDRV_PARA_ERR;
    }
    pool = vhdca_get_pool(dev, para->pool_type, (u32)para->len);

    if (ka_unlikely((para->len <= 0) || (para->len > dev->segment))) {
        hdcdrv_err_limit("data len invalid. (len=%d)\n", para->len);
        return HDCDRV_PARA_ERR;
    }

TRY_ALLOC:

    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("dev_id is invalid. (dev_id=%d)\n", para->dev_id);
        return HDCDRV_DEVICE_NOT_READY;
    }

    ret = hdccom_alloc_mem(pool, buf, addr);
    if ((ret != HDCDRV_OK) && (ret != HDCDRV_DMA_MEM_ALLOC_FAIL)) {
        hdcdrv_err("Calling hdccom_alloc_mem failed. (pool_type=%d; device=%d; ret=%d)\n",
                   para->pool_type, para->dev_id, ret);
        return ret;
    }

    if (ret == HDCDRV_DMA_MEM_ALLOC_FAIL) {
        if (retry++ > VHDCA_ALLOC_MEM_RETRY_TIME) {
            hdcdrv_err("Alloc mem 100 times failed. (pool_type=%d; device=%d; ret=%d)\n",
                para->pool_type, para->dev_id, ret);
            return HDCDRV_DMA_MEM_ALLOC_FAIL;
        }
        ka_system_usleep_range(VHDCA_ALLOC_MEM_SLEEP_MIN, VHDCA_ALLOC_MEM_SLEEP_MAX);
        goto TRY_ALLOC;
    }

    vhdca_vdev_user_inc(dev);
    return HDCDRV_OK;
}

int vhdca_free_mem(void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    struct hdcdrv_mem_pool *pool = NULL;
    struct vhdca_pdev *dev = NULL;
    int ret = HDCDRV_ERR;

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err("Block head check failed.\n");
        return ret;
    }

    block_head = HDCDRV_BLOCK_HEAD(buf);
    if (block_head->devid >= VMNG_PDEV_MAX || block_head->type >= HDCDRV_MEM_POOL_TYPE_NUM) {
        hdcdrv_err("Parameter devid or pool_type are out of range. (devid=%d; pool_type=%d)\n",
                   block_head->devid, block_head->type);
        return HDCDRV_PARA_ERR;
    }

    dev = &g_vhdca_ctrl->pdev[block_head->devid];
    pool = vhdca_get_pool(dev, (int)block_head->type, block_head->size);
    if (pool->ring == NULL) {
        hdcdrv_warn("Pool ring has freed.\n");
        return ret;
    }

    ret = hdccom_free_mem(pool, buf);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_free_mem failed. (pool_type=%d; device=%d; ret=%d)\n",
            block_head->type, block_head->devid, ret);
        return ret;
    }

    vhdca_vdev_user_dec(dev);
    return ret;
}

STATIC void vhdca_delay_free_mem_work(ka_work_struct_t *p_work)
{
    struct hdcdrv_mem_work *m_work = ka_container_of(p_work, struct hdcdrv_mem_work, dwork.work);

    if (m_work->buf != NULL) {
        vhdca_free_mem(m_work->buf);
        m_work->buf = NULL;
    }

    hdcdrv_kvfree(m_work, KA_SUB_MODULE_TYPE_1);
    m_work = NULL;
}

void vhdca_delay_free_mem(void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    struct hdcdrv_mem_work *m_work = NULL;
    int ret;

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err("Block head check failed.\n");
        return;
    }

    block_head = HDCDRV_BLOCK_HEAD(buf);
    ret = (int)ka_base_atomic_add_return(HDCDRV_BLOCK_STATE_OCCUPY, &block_head->status);
    if (ret != HDCDRV_BLOCK_STATE_OCCUPY) {
        /* The state setter will release the resource */
        return;
    }

    /* m_work was freed in vhdca_delay_free_mem_work, so do not execute ka_task_cancel_delayed_work_sync */
    m_work = (struct hdcdrv_mem_work *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_mem_work), KA_SUB_MODULE_TYPE_1);
    if (m_work == NULL) {
        vhdca_free_mem(buf);
        hdcdrv_err("Calling kzalloc failed.\n");
        return;
    }

    m_work->buf = buf;
    KA_TASK_INIT_DELAYED_WORK(&m_work->dwork, vhdca_delay_free_mem_work);

    ka_task_schedule_delayed_work(&m_work->dwork, VHDCA_MEM_WORK_TIMEOUT);
    return;
}

/*
 * vhdca_host_free_mem and vhdca_delay_free_mem may be concurrent,
 * and atomic operation is used to ensure mutual exclusion.
 */
int vhdca_host_free_mem(void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    int state;

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err("Block head check failed.\n");
        return HDCDRV_ERR;
    }

    block_head = HDCDRV_BLOCK_HEAD(buf);
    state = (int)ka_base_atomic_add_return(HDCDRV_BLOCK_STATE_OCCUPY, &block_head->status);
    if (state != HDCDRV_BLOCK_STATE_OCCUPY) {
        /* The state setter will release the resource */
        return HDCDRV_OK;
    }

    return vhdca_free_mem(buf);
}

STATIC int vhdca_get_mem_pool_sg(const struct vhdca_pdev *hdc_dev, struct hdcdrv_mem_pool *pool,
    struct vhdc_ctrl_msg *msg)
{
    struct hdcdrv_mem_block_head *head = NULL;
    struct vmng_tx_msg_proc_info tx_info;
    int ret;
    int i;

    if (pool->size != HDCDRV_HUGE_PACKET_NUM) {
        hdcdrv_err("Input pararmeter is error. (pool_size=%u)\n", pool->size);
        return HDCDRV_ERR;
    }

    for (i = 0; i < HDCDRV_HUGE_PACKET_NUM; i++) {
        head = pool->ring[i].buf;
        msg->pool_check.addr[i] = head->dma_addr;
        msg->pool_check.map[i] = 0;
    }

    msg->pool_check.size = pool->size;
    msg->pool_check.sg_cnt = 0;
    msg->pool_check.segment = pool->segment;

    msg->type = VHDC_CTRL_MSG_TYPE_POOL_CHECK;
    msg->error_code = HDCDRV_ERR;

    tx_info.data = msg;
    tx_info.in_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.out_data_len = sizeof(struct vhdc_ctrl_msg);
    tx_info.real_out_len = 0;
    ret = vmnga_common_msg_send(hdc_dev->dev_id, VMNG_MSG_COMMON_TYPE_HDC, &tx_info);
    if ((ret != 0) || (msg->error_code != HDCDRV_OK)) {
        hdcdrv_err("Calling vmnga_common_msg_send failed. (dev_id=%u; ret=%d; error_code=%d)\n",
            hdc_dev->dev_id, ret, msg->error_code);
        return HDCDRV_SEND_CTRL_MSG_FAIL;
    }

    return HDCDRV_OK;
}

STATIC int vhdca_free_mem_pool_sg(const struct vhdca_pdev *hdc_dev, struct hdcdrv_mem_pool *pool,
    struct vhdc_ctrl_msg *msg)
{
    struct vhdc_ctrl_msg_pool_check *pool_check = NULL;
    struct hdcdrv_mem_block_head *block_head = NULL;
    u32 count = 0;
    u32 i;

    pool_check = &msg->pool_check;

    for (i = 0; i < pool->size; i++) {
        if (pool_check->map[i] != VHDC_MEM_POOL_SG_FLAG) {
            continue;
        }

        /* free sg */
        block_head = pool->ring[i].buf;
        free_mem_pool_single(hdc_dev->dev, pool->segment, block_head, block_head->dma_addr);
        pool->ring[i].buf = NULL;
        count++;
    }

    if (msg->pool_check.sg_cnt != count) {
        hdcdrv_err("sg_cnt is error. (sg_cnt=%u; cnt=%u)\n", msg->pool_check.sg_cnt, count);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

STATIC void vhdca_reinit_mem_pool(struct hdcdrv_mem_pool *pool, struct vhdc_ctrl_msg *msg)
{
    struct vhdc_ctrl_msg_pool_check *pool_check = NULL;
    u32 new_size;
    int i, j;

    pool_check = &msg->pool_check;
    new_size = pool->size - pool_check->sg_cnt;

    j = (int)pool->size - 1;
    for (i = 0; i < j; i++) {
        if (pool->ring[i].buf != NULL) {
            continue;
        }

        for (; j > i; j--) {
            if (pool->ring[j].buf != NULL) {
                pool->ring[i].buf = pool->ring[j].buf;
                pool->ring[j].buf = NULL;
                break;
            }
        }
    }

    pool->size = new_size;
    pool->mask = 0;
    pool->head = 0;
    pool->tail = new_size;

    if (pool->size < VHDCA_MEM_POOL_SIZE_MIN) {
        /* Memory fragmentation is too severe, close huge packet */
        g_vhdca_ctrl->segment = HDCDRV_SMALL_PACKET_SEGMENT;
        pool->valid = HDCDRV_INVALID;
    }

    hdcdrv_warn("Calling vhdca_reinit_mem_pool success. (dev_id=%u; pool_size=%u)\n", pool->dev_id, pool->size);
    return;
}

STATIC int vhdca_adjust_mem_pool(struct vhdca_pdev *hdc_dev, int pool_type, u32 segment)
{
    struct hdcdrv_mem_pool *pool = NULL;
    struct vhdc_ctrl_msg msg;
    int ret;

    pool = vhdca_get_pool_by_segment(hdc_dev, pool_type, segment);

    ret = vhdca_get_mem_pool_sg(hdc_dev, pool, &msg);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_get_mem_pool_sg failed. (ret=%d)\n", ret);
        return ret;
    }

    if (msg.pool_check.sg_cnt > pool->size) {
        hdcdrv_err("Memory pool buffer sg_cnt is illegal. (sg_count=%u)\n", msg.pool_check.sg_cnt);
        return HDCDRV_ERR;
    }

    if (msg.pool_check.sg_cnt > 0) {
        ret = vhdca_free_mem_pool_sg(hdc_dev, pool, &msg);
        if (ret != HDCDRV_OK) {
            hdcdrv_err("Calling vhdca_free_mem_pool_sg failed. (ret=%d)\n", ret);
            return ret;
        }

        vhdca_reinit_mem_pool(pool, &msg);
    }

    return HDCDRV_OK;
}

STATIC int vhdca_alloc_mem_pool(struct vhdca_pdev *hdc_dev, int pool_type, u32 segment, u32 num)
{
    struct hdcdrv_mem_pool *pool = NULL;
    struct hdccom_mem_init init_mem;
    int ret;

    pool = vhdca_get_pool_by_segment(hdc_dev, pool_type, segment);

    init_mem.dev = hdc_dev->dev;
    init_mem.pool_type = pool_type;
    init_mem.dev_id = (int)hdc_dev->dev_id;
    init_mem.segment = segment;
    init_mem.num = num;

    ret = hdccom_init_mem_pool(pool, &init_mem);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_init_mem_pool failed. (dev_id=%d; pool_type=%d)\n", hdc_dev->dev_id, pool_type);
        return ret;
    }

    hdcdrv_info("Alloc mem pool OK. (dev_id=%d; segment=%u; pool_size=%u)\n", hdc_dev->dev_id, segment, pool->size);
    return HDCDRV_OK;
}

STATIC void vhdca_free_mem_pool(struct vhdca_pdev *hdc_dev, int pool_type, u32 segment)
{
    struct hdcdrv_mem_pool *pool = NULL;
    int ret;

    pool = vhdca_get_pool_by_segment(hdc_dev, pool_type, segment);
    ret = hdccom_free_mem_pool(pool, hdc_dev->dev, segment);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Free mem_pool failed. (dev_id=%d; pool_type=%d; ret=%d)\n", hdc_dev->dev_id, pool_type, ret);
        return;
    }

    return;
}

int vhdca_init_mempool(struct vhdca_pdev *pdev)
{
    int ret;

    ret = vhdca_alloc_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_TX, HDCDRV_SMALL_PACKET_SEGMENT, HDCDRV_SMALL_PACKET_NUM);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_alloc_mem_pool failed. (pdev_id=%u; ret=%d)\n", pdev->dev_id, ret);
        goto FREE_SMALL_TX;
    }

    ret = vhdca_alloc_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_RX, HDCDRV_SMALL_PACKET_SEGMENT, HDCDRV_SMALL_PACKET_NUM);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_alloc_mem_pool failed. (pdev_id=%u; ret=%d)\n", pdev->dev_id, ret);
        goto FREE_SMALL_RX;
    }

    ret = vhdca_alloc_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_TX, (u32)pdev->segment, HDCDRV_HUGE_PACKET_NUM);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_alloc_mem_pool failed. (pdev_id=%u; ret=%d)\n", pdev->dev_id, ret);
        goto FREE_HUGE_TX;
    }

    ret = vhdca_alloc_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_RX, (u32)pdev->segment, HDCDRV_HUGE_PACKET_NUM);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_alloc_mem_pool failed. (pdev_id=%u; ret=%d)\n", pdev->dev_id, ret);
        goto FREE_HUGE_RX;
    }

    ret = vhdca_adjust_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_TX, (u32)pdev->segment);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_adjust_mem_pool failed. (pdev_id=%u; ret=%d)\n", pdev->dev_id, ret);
        goto FREE_ADJUST_FAILED;
    }

    ret = vhdca_adjust_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_RX, (u32)pdev->segment);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling vhdca_adjust_mem_pool failed. (pdev_id=%u; ret=%d)\n", pdev->dev_id, ret);
        goto FREE_ADJUST_FAILED;
    }

    return HDCDRV_OK;

FREE_ADJUST_FAILED:
FREE_HUGE_RX:
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_RX, (u32)pdev->segment);
FREE_HUGE_TX:
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_TX, (u32)pdev->segment);
FREE_SMALL_RX:
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_RX, HDCDRV_SMALL_PACKET_SEGMENT);
FREE_SMALL_TX:
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_TX, HDCDRV_SMALL_PACKET_SEGMENT);

    return HDCDRV_ERR;
}

void vhdca_uninit_mempool(struct vhdca_pdev *pdev)
{
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_RX, (u32)pdev->segment);
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_TX, (u32)pdev->segment);
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_RX, HDCDRV_SMALL_PACKET_SEGMENT);
    vhdca_free_mem_pool(pdev, HDCDRV_MEM_POOL_TYPE_TX, HDCDRV_SMALL_PACKET_SEGMENT);
}

int vhdca_mem_block_capacity(void)
{
    return g_vhdca_ctrl->segment - HDCDRV_MEM_BLOCK_HEAD_SIZE;
}

ka_delayed_work_t *hdcdrv_get_recycle_mem(void)
{
    return &(g_vhdca_ctrl->recycle_mem);
}

ka_device_t* hdcdrv_get_pdev_dev(int dev_id)
{
    if (g_vhdca_ctrl->pdev[dev_id].valid != HDCDRV_VALID) {
        hdcdrv_err("Device is not ready. (dev_id=%d)\n", dev_id);
        return NULL;
    }
    return g_vhdca_ctrl->pdev[dev_id].dev;
}

struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_uni(void)
{
    return &(g_vhdca_ctrl->fmem);
}

bool hdcdrv_mem_is_notify(const struct hdcdrv_fast_mem *f_mem)
{
    /* agent must send mem info to host. */
    return true;
}
#ifndef HDC_UT
struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_ex(int devid, u32 fid, u32 side)
{
    return &(g_vhdca_ctrl->fmem);
}
void hdcdrv_node_msg_info_fill(u32 pid, u32 fid, int len, u64 addr, u32 process_stage,
    struct hdcdrv_fast_node_msg_info *msg)
{
    return;
}
int hdcdrv_get_running_status(void)
{
    return 0;
}
#endif
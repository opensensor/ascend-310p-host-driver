/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "dvt.h"
#include "mmio.h"
#include "vfio_ops.h"

struct position {
    unsigned int index;
    uint64_t pos;
    size_t count;
};

STATIC int _hw_vdavinci_rw(struct hw_vdavinci *vdavinci, struct position *p, char *buf, bool write)
{
    u64 bar_start;
    int ret;
    unsigned int index = p->index;
    uint64_t pos = p->pos;
    size_t count = p->count;

    switch (index) {
        case VFIO_PCI_CONFIG_REGION_INDEX:
            if (write) {
                ret = g_hw_vdavinci_ops.emulate_cfg_write(vdavinci, pos, buf, count);
            } else {
                ret = g_hw_vdavinci_ops.emulate_cfg_read(vdavinci, pos, buf, count);
            }

            break;
        case VFIO_PCI_BAR0_REGION_INDEX:
            bar_start = (*(u64 *)(vdavinci->cfg_space.config + PCI_BASE_ADDRESS_0)) &
                PCI_BASE_ADDRESS_MEM_MASK;
            if (write) {
                ret = g_hw_vdavinci_ops.emulate_mmio_write(vdavinci,
                        bar_start + pos, buf, count);
            } else {
                ret = g_hw_vdavinci_ops.emulate_mmio_read(vdavinci,
                        bar_start + pos, buf, count);
            }
            break;

        case VFIO_PCI_BAR2_REGION_INDEX:
        case VFIO_PCI_BAR1_REGION_INDEX:
        case VFIO_PCI_BAR3_REGION_INDEX:
        case VFIO_PCI_BAR4_REGION_INDEX:
        case VFIO_PCI_BAR5_REGION_INDEX:
        case VFIO_PCI_VGA_REGION_INDEX:
        case VFIO_PCI_ROM_REGION_INDEX:
        default:
            vascend_err(vdavinci_to_dev(vdavinci), "unsupported region: %u, vid: %u\n",
                index, vdavinci->id);
            return -EINVAL;
    }

    return ret == 0 ? count : ret;
}

ssize_t hw_vdavinci_rw(struct hw_vdavinci *vdavinci, char *buf,
                       size_t count, loff_t *ppos, bool write)
{
    unsigned int index;
    uint64_t pos;
    struct position p;

    if (*ppos < 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "invalid pos: %lld\n", *ppos);
        return -EINVAL;
    }

    pos = (*(u64 *)ppos) & VFIO_PCI_OFFSET_MASK;
    index = VFIO_PCI_OFFSET_TO_INDEX(*(u64 *)ppos);
    if (index >= VFIO_PCI_NUM_REGIONS) {
        vascend_err(vdavinci_to_dev(vdavinci), "invalid index: %u, vid: %u\n",
            index, vdavinci->id);
        return -EINVAL;
    }

    p.index = index;
    p.pos = pos;
    p.count = count;

    return _hw_vdavinci_rw(vdavinci, &p, buf, write);
}

#if ((IS_VDAVINCI_KERNEL_VERSION_SUPPORT) || (defined(DRV_UT)))
STATIC int vdavinci_ioeventfd_handler(void *opaque, void *unused)
{
    struct vdavinci_ioeventfd *ioeventfd = opaque;
    struct position p;

    p.index = ioeventfd->bar;
    p.pos = ioeventfd->pos;
    p.count = ioeventfd->count;

    _hw_vdavinci_rw(ioeventfd->vdavinci, &p, (char *)&ioeventfd->data, true);
    return 0;
}

STATIC struct vdavinci_ioeventfd *
ioeventfd_find_exist(struct hw_vdavinci *vdavinci, loff_t pos,
                     int bar, uint64_t data, int count)
{
    struct vdavinci_ioeventfd *ioeventfd;

    list_for_each_entry(ioeventfd, &vdavinci->ioeventfds_list, next) {
        if (ioeventfd->pos == pos && ioeventfd->bar == bar &&
            ioeventfd->data == data && ioeventfd->count == count) {
            return ioeventfd;
        }
    }

    return NULL;
}

void hw_vdavinci_ioeventfd_deactive(struct hw_vdavinci *vdavinci,
                                    struct vdavinci_ioeventfd *ioeventfd)
{
    vfio_virqfd_disable(&ioeventfd->virqfd);
    list_del(&ioeventfd->next);
    kfree(ioeventfd);
    vdavinci->ioeventfds_nr--;
}

long hw_vdavinci_set_ioeventfd(struct hw_vdavinci *vdavinci, loff_t offset, uint64_t data,
                               int count, int fd)
{
    int ret = 0;
    struct vdavinci_ioeventfd *ioeventfd;
    int bar = VFIO_PCI_OFFSET_TO_INDEX(offset);
    /* offset: offset from the device fd
     * pos: offset from the bar base
     */
    loff_t pos = offset & VFIO_PCI_OFFSET_MASK;

    if (bar != VFIO_PCI_BAR0_REGION_INDEX) {
        return -EINVAL;
    }

    if (pos + count > DOORBELL_MAX * DOORBELL_SIZE) {
        return -EINVAL;
    }

    mutex_lock(&vdavinci->ioeventfds_lock);
    ioeventfd = ioeventfd_find_exist(vdavinci, pos, bar, data, count);
    if (ioeventfd) {
        if (fd == -1) {
            hw_vdavinci_ioeventfd_deactive(vdavinci, ioeventfd);
        } else {
            ret = -EEXIST;
            vascend_err(vdavinci_to_dev(vdavinci), "ioeventfd exist, bar %d, pos %lld,"
                         " count %d, data %llu fd %d\n", bar, pos, count, data, fd);
        }
        goto out_unlock;
    }

    if (fd < 0) {
        ret = -ENODEV;
        goto out_unlock;
    }

    if (vdavinci->ioeventfds_nr >= DOORBELL_MAX) {
        ret = -ENOSPC;
        goto out_unlock;
    }

    ioeventfd = kzalloc(sizeof(*ioeventfd), GFP_KERNEL);
    if (!ioeventfd) {
        ret = -ENOMEM;
        goto out_unlock;
    }

    ioeventfd->vdavinci = vdavinci;
    ioeventfd->data = data;
    ioeventfd->pos = pos;
    ioeventfd->count = count;

    ret = vfio_virqfd_enable(ioeventfd, vdavinci_ioeventfd_handler, NULL,
                             NULL, &ioeventfd->virqfd, fd);
    if (ret) {
        kfree(ioeventfd);
        goto out_unlock;
    }

    list_add(&ioeventfd->next, &vdavinci->ioeventfds_list);
    vdavinci->ioeventfds_nr++;

    vascend_info(vdavinci_to_dev(vdavinci), "set ioeventfd success, bar %d, pos %lld, "
                 "count %d, fd %d\n", bar, pos, count, fd);

out_unlock:
    mutex_unlock(&vdavinci->ioeventfds_lock);
    return ret;
}
#endif

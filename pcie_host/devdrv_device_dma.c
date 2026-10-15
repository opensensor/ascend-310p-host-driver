// SPDX-License-Identifier: GPL-2.0-only
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "ascend_device_dma.h"
#include "devdrv_dma.h"
#include "devdrv_pci.h"
#include "devdrv_ctrl.h"
#include "devdrv_device_dma.h"
#include "devdrv_device_dma_source.h"

struct devdrv_device_dma_file {
    struct devdrv_dma_dev *dma_dev;
    u64 generation;
    u32 entry;
    struct devdrv_device_dma_source *source;
    u64 source_sequence;
};

/* Only setup/teardown uses this mutex. Host DMA dispatch never takes it. */
static DEFINE_MUTEX(device_dma_mutex);
static LIST_HEAD(device_dma_devices);
static u64 device_dma_generation;

/* Pure planner: never changes shared routing ranges or instance identities. */
static int devdrv_device_dma_choose_channel(const struct devdrv_dma_dev *dev, u32 *entry)
{
    const struct data_type_chan *traffic = &dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC];
    u32 candidate, type, i;

    for (i = 0; i < dev->remote_chan_num; i++) {
        if (READ_ONCE(dev->dma_chan[i].device_dma_reserved)) {
            return -EBUSY;
        }
    }
    if ((traffic->chan_num < 2) || (traffic->chan_start_id >= dev->remote_chan_num) ||
        (traffic->chan_num > dev->remote_chan_num - traffic->chan_start_id)) {
        return -ENOSPC;
    }
    candidate = traffic->chan_start_id + traffic->chan_num - 1;
    for (type = 0; type < DEVDRV_DMA_DATA_TYPE_MAX; type++) {
        const struct data_type_chan *pool = &dev->data_chan[type];

        if ((pool->chan_start_id > dev->remote_chan_num) ||
            (pool->chan_num > dev->remote_chan_num - pool->chan_start_id)) {
            return -EINVAL;
        }
        if ((type != DEVDRV_DMA_DATA_TRAFFIC) && (candidate >= pool->chan_start_id) &&
            (candidate - pool->chan_start_id < pool->chan_num)) {
            return -EBUSY;
        }
    }
    *entry = candidate;
    return 0;
}

/* cq_lock waits out completion callbacks; lock excludes descriptor submission.
 * If still busy, admission fails without stopping traffic or stealing a ring.
 * No waiting for DMA while holding a spinlock. The caller may retry at startup.
 */
static int devdrv_device_dma_try_claim(struct devdrv_dma_channel *chan,
    struct devdrv_dma_channel *redirect)
{
    int ret = 0;

    if ((redirect == chan) || READ_ONCE(redirect->host_dma_redirect) ||
        READ_ONCE(redirect->device_dma_reserved) ||
        (READ_ONCE(redirect->chan_status) != DEVDRV_DMA_CHAN_ENABLED)) {
        return -EINVAL;
    }
    spin_lock_bh(&chan->cq_lock);
    spin_lock(&chan->lock);
    if (chan->chan_status != DEVDRV_DMA_CHAN_ENABLED) {
        ret = -ENODEV;
    } else if (chan->device_dma_reserved || (chan->sq_head != chan->sq_tail)) {
        ret = -EBUSY;
    } else {
        /* The old queue is drained. Never move these instances back on release:
         * work may still be queued on redirect then. Submission checks this
         * pointer under the same SQ lock, including callers selected before us.
         */
        WRITE_ONCE(chan->host_dma_redirect, redirect);
        WRITE_ONCE(chan->device_dma_reserved, true);
    }
    spin_unlock(&chan->lock);
    spin_unlock_bh(&chan->cq_lock);
    return ret;
}

static int devdrv_device_dma_drop(struct devdrv_device_dma_file *owner)
{
    struct devdrv_dma_dev *dev = owner->dma_dev;
    struct devdrv_dma_channel *chan;
    int ret = 0;

    if (dev == NULL) {
        return -ENODEV;
    }
    chan = &dev->dma_chan[owner->entry];
    spin_lock_bh(&chan->cq_lock);
    spin_lock(&chan->lock);
    /* ABI v1 exposes no producer; a live lease must remain empty. Future DMA
     * activation requires producer revocation and hardware drain before here.
     */
    if (chan->sq_head != chan->sq_tail) {
        ret = -EBUSY;
    } else {
        WRITE_ONCE(chan->device_dma_reserved, false);
        dev->device_dma_owner = NULL;
        owner->dma_dev = NULL;
    }
    spin_unlock(&chan->lock);
    spin_unlock_bh(&chan->cq_lock);
    if (ret == 0) {
        devdrv_device_dma_source_destroy(owner->source);
        owner->source = NULL;
    }
    return ret;
}

void devdrv_device_dma_attach(struct devdrv_dma_dev *dev)
{
    mutex_lock(&device_dma_mutex);
    if (!dev->device_dma_registered && (dev->pci_ctrl->chip_type == HISI_MINI_V2) &&
        (devdrv_get_env_boot_type(dev->dev_id) == DEVDRV_PHY_BOOT) &&
        (dev->sq_cq_side == DEVDRV_DMA_REMOTE_SIDE)) {
        list_add_tail(&dev->device_dma_registry, &device_dma_devices);
        dev->device_dma_registered = true;
    }
    mutex_unlock(&device_dma_mutex);
}

void devdrv_device_dma_detach(struct devdrv_dma_dev *dev)
{
    mutex_lock(&device_dma_mutex);
    if (dev->device_dma_registered) {
        list_del_init(&dev->device_dma_registry);
        dev->device_dma_registered = false;
    }
    if (dev->device_dma_owner != NULL) {
        /* Source mappings are not exported or submitted in this ABI. Producer
         * activation must add revocation/drain before this cleanup is allowed.
         */
        devdrv_device_dma_source_destroy(dev->device_dma_owner->source);
        dev->device_dma_owner->source = NULL;
        /* Invalidate the FD before ring storage is freed, even on reset. */
        dev->device_dma_owner->dma_dev = NULL;
        dev->device_dma_owner = NULL;
    }
    mutex_unlock(&device_dma_mutex);
}

int devdrv_device_dma_suspend(struct devdrv_dma_dev *dev)
{
    u32 i;
    int ret = 0;

    mutex_lock(&device_dma_mutex);
    for (i = 0; i < dev->remote_chan_num; i++) {
        if (READ_ONCE(dev->dma_chan[i].device_dma_reserved)) {
            ret = -EBUSY;
            goto out;
        }
    }
    if (dev->device_dma_registered) {
        list_del_init(&dev->device_dma_registry);
        dev->device_dma_registered = false;
    }
out:
    mutex_unlock(&device_dma_mutex);
    return ret;
}

static int devdrv_device_dma_acquire(struct devdrv_device_dma_file *owner,
    const struct ascend_device_dma_lease *request)
{
    struct devdrv_dma_dev *dev;
    u32 entry;
    int ret;

    if (owner->dma_dev != NULL) {
        return -EBUSY;
    }
    if (request->generation || request->channel || request->capabilities ||
        request->sq_depth || request->cq_depth) {
        return -EINVAL;
    }
    if (device_dma_generation == U64_MAX) {
        return -EOVERFLOW;
    }
    list_for_each_entry(dev, &device_dma_devices, device_dma_registry) {
        if (dev->dev_id != request->dev_id) {
            continue;
        }
        if ((dev->dev_status != DEVDRV_DMA_ALIVE) ||
            (READ_ONCE(dev->pci_ctrl->device_status) != DEVDRV_DEVICE_ALIVE) ||
            (READ_ONCE(dev->pci_ctrl->is_sriov_enabled) != DEVDRV_SRIOV_DISABLE)) {
            return -ENODEV;
        }
        ret = devdrv_device_dma_choose_channel(dev, &entry);
        if (ret != 0) {
            return ret;
        }
        ret = devdrv_device_dma_try_claim(&dev->dma_chan[entry],
            &dev->dma_chan[dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id]);
        if (ret != 0) {
            return ret;
        }
        owner->dma_dev = dev;
        owner->entry = entry;
        owner->generation = ++device_dma_generation;
        dev->device_dma_owner = owner;
        return 0;
    }
    return -ENODEV;
}

static void devdrv_device_dma_describe(const struct devdrv_device_dma_file *owner,
    struct ascend_device_dma_lease *reply)
{
    const struct devdrv_dma_channel *chan = &owner->dma_dev->dma_chan[owner->entry];

    memset(reply, 0, sizeof(*reply));
    reply->version = ASCEND_DEVICE_DMA_ABI_VERSION;
    reply->dev_id = owner->dma_dev->dev_id;
    reply->channel = chan->chan_id;
    reply->generation = owner->generation;
    reply->sq_depth = chan->sq_depth;
    reply->cq_depth = chan->cq_depth;
    reply->capabilities = ASCEND_DEVICE_DMA_CAP_HOST_ROUTING_ISOLATED |
        ASCEND_DEVICE_DMA_CAP_SOURCE_REGISTRATION;
    /* Still no device access or DMA submission. */
}

static long devdrv_device_dma_source_ioctl(struct devdrv_device_dma_file *owner,
    unsigned int cmd, void __user *user)
{
    struct ascend_device_dma_source request;
    int ret;

    if (copy_from_user(&request, user, sizeof(request))) {
        return -EFAULT;
    }
    if (request.version != ASCEND_DEVICE_DMA_ABI_VERSION || request.flags ||
        request.reserved[0] || request.reserved[1] || request.page_count || request.segment_count) {
        return -EINVAL;
    }
    mutex_lock(&device_dma_mutex);
    if (owner->dma_dev == NULL) {
        ret = -ENODEV;
        goto out;
    }
    if (request.generation != owner->generation) {
        ret = -ESTALE;
        goto out;
    }
    if (owner->dma_dev->dev_status != DEVDRV_DMA_ALIVE ||
        READ_ONCE(owner->dma_dev->pci_ctrl->device_status) != DEVDRV_DEVICE_ALIVE) {
        ret = -ENODEV;
        goto out;
    }
    if (cmd == ASCEND_DEVICE_DMA_SOURCE_REGISTER) {
        if (request.region_id) {
            ret = -EINVAL;
            goto out;
        }
        if (owner->source != NULL) {
            ret = -EBUSY;
            goto out;
        }
        if (owner->source_sequence == U64_MAX) {
            ret = -EOVERFLOW;
            goto out;
        }
        request.region_id = owner->source_sequence + 1;
        ret = devdrv_device_dma_source_create(owner->dma_dev->dma_chan[owner->entry].dev,
            &request, &owner->source);
        if (ret != 0) {
            goto out;
        }
        owner->source_sequence = request.region_id;
    } else {
        if (request.address || request.length) {
            ret = -EINVAL;
            goto out;
        }
        if (owner->source == NULL || request.region_id != owner->source_sequence) {
            ret = -ESTALE;
            goto out;
        }
    }
    if (cmd == ASCEND_DEVICE_DMA_SOURCE_RELEASE) {
        devdrv_device_dma_source_destroy(owner->source);
        owner->source = NULL;
        ret = 0;
        goto out;
    }
    devdrv_device_dma_source_describe(owner->source, &request);
    ret = 0;
    if (copy_to_user(user, &request, sizeof(request))) {
        if (cmd == ASCEND_DEVICE_DMA_SOURCE_REGISTER) {
            devdrv_device_dma_source_destroy(owner->source);
            owner->source = NULL;
        }
        ret = -EFAULT;
    }
out:
    mutex_unlock(&device_dma_mutex);
    return ret;
}

static long devdrv_device_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct devdrv_device_dma_file *owner = file->private_data;
    struct ascend_device_dma_lease request;
    void __user *user = (void __user *)arg;
    int ret;

    if ((cmd == ASCEND_DEVICE_DMA_SOURCE_REGISTER) || (cmd == ASCEND_DEVICE_DMA_SOURCE_QUERY) ||
        (cmd == ASCEND_DEVICE_DMA_SOURCE_RELEASE)) {
        return devdrv_device_dma_source_ioctl(owner, cmd, user);
    }
    if ((cmd != ASCEND_DEVICE_DMA_ACQUIRE) && (cmd != ASCEND_DEVICE_DMA_QUERY) &&
        (cmd != ASCEND_DEVICE_DMA_RELEASE)) {
        return -ENOTTY;
    }
    if (copy_from_user(&request, user, sizeof(request))) {
        return -EFAULT;
    }
    if ((request.version != ASCEND_DEVICE_DMA_ABI_VERSION) || request.flags ||
        request.reserved[0] || request.reserved[1] || request.reserved[2]) {
        return -EINVAL;
    }
    mutex_lock(&device_dma_mutex);
    if (cmd == ASCEND_DEVICE_DMA_ACQUIRE) {
        ret = devdrv_device_dma_acquire(owner, &request);
        if (ret != 0) {
            goto out;
        }
    } else if (owner->dma_dev == NULL) {
        ret = -ENODEV;
        goto out;
    }
    if (cmd == ASCEND_DEVICE_DMA_RELEASE) {
        if ((request.generation != owner->generation) ||
            (request.dev_id != owner->dma_dev->dev_id) ||
            (request.channel != owner->dma_dev->dma_chan[owner->entry].chan_id)) {
            ret = -ESTALE;
        } else {
            ret = devdrv_device_dma_drop(owner);
        }
        goto out;
    }
    devdrv_device_dma_describe(owner, &request);
    ret = 0;
    if (copy_to_user(user, &request, sizeof(request))) {
        if (cmd == ASCEND_DEVICE_DMA_ACQUIRE) {
            (void)devdrv_device_dma_drop(owner);
        }
        ret = -EFAULT;
    }
out:
    mutex_unlock(&device_dma_mutex);
    return ret;
}

static int devdrv_device_dma_open(struct inode *inode, struct file *file)
{
    int ret;

    file->private_data = kzalloc(sizeof(struct devdrv_device_dma_file), GFP_KERNEL);
    if (file->private_data == NULL) {
        return -ENOMEM;
    }
    ret = nonseekable_open(inode, file);
    if (ret != 0) {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    return ret;
}

static int devdrv_device_dma_close(struct inode *inode, struct file *file)
{
    struct devdrv_device_dma_file *owner = file->private_data;

    (void)inode;
    mutex_lock(&device_dma_mutex);
    if (owner->dma_dev != NULL) {
        if (devdrv_device_dma_drop(owner) != 0) {
            /* An uncertain queue is quarantined until device reset, not reused. */
            owner->dma_dev->device_dma_owner = NULL;
            owner->dma_dev = NULL;
        }
    }
    /* No source address has been exposed to a producer or placed in a queue. */
    devdrv_device_dma_source_destroy(owner->source);
    owner->source = NULL;
    mutex_unlock(&device_dma_mutex);
    kfree(owner);
    return 0;
}

static const struct file_operations device_dma_fops = {
    .owner = THIS_MODULE,
    .open = devdrv_device_dma_open,
    .release = devdrv_device_dma_close,
    .unlocked_ioctl = devdrv_device_dma_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = compat_ptr_ioctl,
#endif
};

static struct miscdevice device_dma_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ascend_device_dma",
    .fops = &device_dma_fops,
    .mode = 0600,
};

int devdrv_device_dma_init(void)
{
    return misc_register(&device_dma_misc);
}

void devdrv_device_dma_uninit(void)
{
    misc_deregister(&device_dma_misc);
}

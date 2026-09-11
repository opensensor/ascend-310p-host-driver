/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ASCEND_LINUX_COMPAT_H
#define ASCEND_LINUX_COMPAT_H

#include <linux/version.h>

/* Linux 6.15 removed the old timer API aliases. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
#define del_timer timer_delete
#define del_timer_sync timer_delete_sync
#define from_timer timer_container_of

/* Vendor callers install timer->function separately, before starting it. */
#define hrtimer_init(timer, clock_id, mode) \
    hrtimer_setup(timer, NULL, clock_id, mode)
#endif

#endif /* ASCEND_LINUX_COMPAT_H */

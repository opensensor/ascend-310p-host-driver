/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2024-07-17
 */

#ifndef KREF_SAFE_H
#define KREF_SAFE_H

#include <linux/kref.h>
#include <linux/preempt.h>
#include <linux/workqueue.h>
#include <linux/version.h>

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_common_pub.h"

struct kref_safe {
    ka_kref_t kref;
    void (*release)(struct kref_safe *kref_s);
    ka_work_struct_t work;
};

static void kref_safe_release_work(struct work_struct *work)
{
    struct kref_safe *kref_s = ka_container_of(work, struct kref_safe, work);
    kref_s->release(kref_s);
}

static inline void kref_safe_init(struct kref_safe *kref_s)
{
    KA_TASK_INIT_WORK(&kref_s->work, kref_safe_release_work);
	ka_base_kref_init(&kref_s->kref);
}

static inline unsigned int kref_safe_read(const struct kref_safe *kref_s)
{
	return ka_base_kref_read((const ka_kref_t *)&kref_s->kref);
}

static inline void kref_safe_get(struct kref_safe *kref_s)
{
	ka_base_kref_get(&kref_s->kref);
}

static inline int kref_safe_put(struct kref_safe *kref_s, void (*release)(struct kref_safe *kref_s))
{
    ka_kref_t *kref = &kref_s->kref;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
    if (refcount_dec_and_test(&kref->refcount)) {
#else
    if (ka_base_atomic_sub_and_test(1, &kref->refcount)) {
#endif
        if (ka_system_in_interrupt()) {
            /* The blocking function may be invoked in release.
               If the blocking function is invoked during the
               interruption, an exception occurs. Therefore,
               the release work is started. */
            kref_s->release = release;
            ka_task_schedule_work(&kref_s->work);
        } else {
            release(kref_s);
        }
		return 1;
	}
	return 0;
}

static inline int __must_check kref_safe_get_unless_zero(struct kref_safe *kref_s)
{
	return ka_base_kref_get_unless_zero(&kref_s->kref);
}

#endif


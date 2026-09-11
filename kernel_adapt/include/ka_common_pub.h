/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
 * Create: 2024-11-05
 */

#ifndef KA_COMMON_PUB_H
#define KA_COMMON_PUB_H

#ifndef __cplusplus
typedef struct class ka_class_t;
#endif

typedef struct device ka_device_t;
typedef struct module ka_module_t;
typedef struct page ka_page_t;
typedef struct cpumask ka_cpumask_t;
typedef struct mii_bus ka_mii_bus_t;
typedef struct pci_dev ka_pci_dev_t;
typedef struct inode ka_inode_t;
typedef struct device_node ka_device_node_t;
typedef struct task_struct ka_task_struct_t;
typedef struct sched_param ka_sched_param_t;
typedef struct rw_semaphore ka_rw_semaphore_t;
typedef struct fs_struct ka_fs_struct_t;
typedef struct file ka_file_t;
typedef struct path ka_path_t;
typedef struct dentry ka_dentry_t;
typedef struct seq_file ka_seq_file_t;
typedef struct cdev ka_cdev_t;
typedef struct kobject ka_kobject_t;
typedef struct kstat ka_kstat_t;
typedef struct mem_cgroup ka_mem_cgroup_t;
typedef struct kiocb ka_kiocb_t;
typedef struct iov_iter ka_iov_iter_t;
typedef struct hlist_node ka_hlist_node_t;

#define ka_kuid_t kuid_t
#define ka_kgid_t kgid_t
#define ka_uuid_le_t uuid_le

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */

#define ka_offsetof(TYPE, MEMBER) offsetof(TYPE, MEMBER)

#define ka_container_of(ptr, type, member) container_of(ptr, type, member)

#endif

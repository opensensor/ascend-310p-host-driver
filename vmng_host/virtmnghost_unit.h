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

#ifndef VIRTMNGHOST_UNIT_H
#define VIRTMNGHOST_UNIT_H

#include "virtmng_interface.h"
#include "hw_vdavinci.h"
#include "ka_pci_pub.h"

#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_memory_pub.h"

#define VMNGA_VD_DEV_ALIVE_NOK 0
#define VMNGA_VD_DEV_ALIVE_OK 1

struct vmngh_start_dev {
    ka_timer_list_t wd_timer;
    ka_atomic_t start_flag;
    u32 msix_irq;
    int timer_remain;
    int timer_cycle;
};

/* unit <== pdev <== vd_dev <= bar space */
struct vmngh_mmio_addr {
    void *io_base;
    int io_size;
    void *mem_base;
    int mem_size;
};

struct vmngh_vdev_dfx {
    u32 create_bottom_cnt;
    u32 reset_cnt;
};

struct vmngh_db_entry {
    u32 index;
    u32 db_count;
    db_handler_t handler;
    void *data;
};

struct vmngh_db_mng {
    u32 db_num;
    struct vmngh_db_entry *db_entries;
};

struct vm_msix_struct {
    void *vdavinci;
    int irq_vector;
};

enum VMNGH_MDEV_STATE_FLAG {
    VMNGH_MDEV_RUNNING_STATE = 0,
    VMNGH_MDEV_VM_RESET_STATE,          // vm reboot
    VMNGH_MDEV_FLR_STATE                // vf flr in vm
};

struct vmngh_vd_dev {
    void *vm_pdev;
    void *vdavinci;
    ka_device_t *resource_dev;        // for sriov resource dev
    ka_dma_addr_t iova_addr;
    size_t size;
    struct vmngh_db_mng db_mng;
    struct vmngh_mmio_addr mm_res;
    struct vmngh_vdev_dfx dfx;
    struct vmngh_start_dev start_dev;
    ka_work_struct_t start_work;
    struct vmng_shr_para *shr_para;
    struct vmng_msg_dev *msg_dev;
    ka_uuid_le_t vascend_uuid;
    struct vdavinci_type dtype;
    u32 init_status;
    u32 dev_id;
    u32 fid;
    u32 vm_pid;
    u32 vm_id;
    u32 agent_device;
    u32 agent_dev_id;
    u32 agent_bdf;
    u64 bandwidth_limit;
    struct vm_msix_struct *vm_msix;
    struct vmngh_vpc_unit *vpc_unit;
    ka_atomic_t reset_ref_flag;
};

struct vmngh_pci_dev {
    void *unit_priv;
    ka_pci_dev_t *pdev;
    ka_device_t *dev;
    struct vmngh_vd_dev *vd_dev[VMNG_VDEV_MAX_PER_PDEV];
    ka_mutex_t vddev_mutex[VMNG_VDEV_MAX_PER_PDEV];
    struct dvt_devinfo dev_info;
    ka_mutex_t vpdev_mutex;
    u32 dev_id; /* dev_id in pcie module */
    u32 vdev_num;
    u32 status;
    u32 ep_devic_id;
    u32 vdev_ref;
    u32 valid;
    u32 peer_dev_id;
    void *ctrl_msg_chan;
    struct vmng_bandwith_ctrl bw_ctrl;
    u32 vm_full_spec_enable;
};

struct vmngh_unit {
    struct vmngh_pci_dev vmngh_pcidev[VMNG_PDEV_MAX];
    u32 phy_dev_num;
};

int vmngh_dev_id_check(u32 dev_id, u32 fid);
int vmngh_vm_id_check(u32 vm_id, u32 vm_devid);
struct vmngh_vd_dev *vmngh_get_vddev_by_id(u32 dev_id, u32 fid);
ka_mutex_t *vmngh_get_vdev_lock_from_unit(u32 dev_id, u32 fid);
struct vmngh_vd_dev *vmngh_get_vdev_from_unit(u32 dev_id, u32 fid);
struct vmngh_pci_dev *vmngh_get_pdev_from_unit(u32 dev_id);
void vmngh_free_vdev(struct vmngh_vd_dev *vd_dev);
u32 vmngh_vd_dev_alive(void);
int vmngh_init_unit(void);
void vmngh_print_uuid(u32 dev_id, u32 fid, ka_uuid_le_t uuid);
void vmngh_set_device_status(u32 dev_id, u32 valid);
u32 vmngh_get_device_status(u32 dev_id);
void vmngh_set_peer_dev_id(u32 dev_id, int peer_dev_id);

#endif

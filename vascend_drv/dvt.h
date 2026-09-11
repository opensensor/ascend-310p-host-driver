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
#ifndef DVT_H_
#define DVT_H_

#include <linux/version.h>
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)) || (defined(DRV_UT)))
#include <linux/pci_regs.h>
#endif
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/hashtable.h>
#include <linux/vfio.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/kvm_types.h>
#include <linux/iommu.h>
#include <linux/eventfd.h>
#include <linux/iova.h>

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)) || (defined(DRV_UT)))
#include <linux/mdev.h>
#endif

#include "hw_vdavinci.h"
#include "log.h"

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)) && (!defined(DRV_UT)))
#define PCI_CFG_SPACE_SIZE      256
#define VFIO_DEVICE_API_PCI_STRING              "vfio-pci"
#endif

#ifndef PCI_CFG_SPACE_EXP_SIZE
#define PCI_CFG_SPACE_EXP_SIZE  4096
#endif

#define DVT_MAX_VDAVINCI 16
#define BYTES_TO_KB(b) ((b) >> 10ULL)
#define HW_DVT_MAX_DEV_NUM 2
#define HW_DVT_MAX_BAR_NUM 6
#define F_UNALIGN      (1 << 6)

#define DAVINCI_PCI_MSIX     0xa0
#define DAVINCI_PCI_MSIX_NEXT_CAP_POINTER      (DAVINCI_PCI_MSIX + 1)
#define DAVINCI_PCI_MSIX_FLAGS      (DAVINCI_PCI_MSIX + PCI_MSIX_FLAGS)
#define DAVINCI_PCI_MSIX_TABLE      (DAVINCI_PCI_MSIX + PCI_MSIX_TABLE)
#define DAVINCI_PCI_MSIX_PBA      (DAVINCI_PCI_MSIX + PCI_MSIX_PBA)

#define DAVINCI_PCI_PM     0xb0
#define DAVINCI_PCI_PM_PMC     (DAVINCI_PCI_PM + PCI_PM_PMC)
#define DAVINCI_PCI_PM_CTRL     (DAVINCI_PCI_PM + PCI_PM_CTRL)
#define DAVINCI_PM_CAP_CFG_CAP    0xf803
#define DAVINCI_PM_CAP_CFG_CSR    0x0008
#define PCI_VENDOR_ID_HUAWEI      0x19e5
#ifdef DAVINCI_TEST
#define PCI_DEVICE_ID_ASCEND310   0xd100
#endif
#define PCI_DEVICE_ID_ASCEND310P  0xd500
#define PCI_DEVICE_ID_ASCEND910   0xd801
#define PCI_DEVICE_ID_ASCEND910B  0xd802
#define PCI_DEVICE_ID_ASCEND910_93  0xd803
#define PCI_DEVICE_ID_ASCEND950  0xd806
#define DVT_MMIO_BAR0_SIZE      0x20000
#define DVT_MMIO_BAR2_SIZE      0x2000000
#define DVT_MMIO_BAR4_SIZE      0x4500000
#define VF_BAR0_SPARSE_SIZE     6
#define VF_BAR0_DOORBELL_OFFSET 0
#define VF_BAR0_DOORBELL_SIZE   0x20000
#define VF_BAR0_VPC_OFFSET      0x20000
#define VF_BAR0_VPC_SIZE        0x4000000
#define VF_BAR0_DVPP_OFFSET     0x4020000
#define VF_BAR0_DVPP_SIZE       0x1800000
#define VF_BAR0_MSG_OFFSET      0x6000000
#define VF_BAR0_MSG_SIZE        0x1000000
#define VF_BAR0_TOPIC_OFFSET   0x7000000
#define VF_BAR0_TOPIC_SIZE     0x10000
#define VF_BAR0_MSIX_OFFSET    0x7010000
#define VF_BAR0_MSIX_SIZE      0x4000
#define VF_BAR2_SPARSE_SIZE         5
#define VF_BAR2_STARS_OFFSET        0x8000
#define VF_BAR2_STARS_SIZE          0x2000000
#define VF_BAR2_TS_DOORBELL_OFFSET  0x2008000
#define VF_BAR2_TS_DOORBELL_SIZE    0x40000
#define VF_BAR2_HWTS_OFFSET         0x2408000
#define VF_BAR2_HWTS_SIZE           0x10000
#define VF_BAR2_SOC_DOORBELL_OFFSET 0x2808000
#define VF_BAR2_SOC_DOORBELL_SIZE   0x1000
#define VF_BAR2_PARA_OFFSET         0x2908000
#define VF_BAR2_PARA_SIZE           0x4000
#define VF_BAR4_SPARSE_SIZE         1
#define VF_BAR4_HBM_OFFSET          0
#define VF_BAR4_HBM_SIZE            0x100000000
#define VF_MMIO_BAR0_SIZE_910B      0x8000000
#define VF_MMIO_BAR2_SIZE_910B      0x4000000
#define VF_MMIO_BAR4_SIZE_910B      0x100000000
#define VF_MMIO_BAR0_SIZE_910_93      0x8000000
#define VF_MMIO_BAR2_SIZE_910_93      0x4000000
#define VF_MMIO_BAR4_SIZE_910_93      0x100000000
#define VF_MMIO_BAR0_SIZE_950      0x8000000
#define VF_MMIO_BAR2_SIZE_950      0x4000000
#define VF_MMIO_BAR4_SIZE_950      0x100000000
#define VDAVINCI_NAME           "vnpu"
#define VDAVINCI_PREFIX         VDAVINCI_NAME"-"
#define VDAVINCI_VFG_MAX        4
#define VDAVINCI_VF_MAX         12
#define STORE_LE16(addr, val)   (*(u16 *)addr = val)
#define STORE_LE32(addr, val)   (*(u32 *)addr = val)
#define STORE_LE64(addr, val)   (*(u64 *)addr = val)
#define MASK_HIGH_BIT           31
#define MASK_MID_LOW_BIT        4
#define MASK_MID_HIGH_BIT       3
#define BAR_OFFSET_ALIGN        4
#define BAR_OFFSET_LENGTH       32
#define BAR_SIZE_ALIGN          8
#define VFIO_PCI_OFFSET_SHIFT   40
#define VFIO_PCI_OFFSET_TO_INDEX(off)   ((off) >> VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_INDEX_TO_OFFSET(index) ((u64)(index) << VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_OFFSET_MASK    (((u64)(1) << VFIO_PCI_OFFSET_SHIFT) - 1)

struct pci_sriov {
    u16     padding[10];
    u16     offset;
    u16     stride;
};

struct page_info_list {
    unsigned int elem_num;
    struct list_head head;
};

struct page_info_entry {
    unsigned int length;
    unsigned long gfn;
    struct page *page;
    struct list_head list;
};

struct hw_dvt_device_info {
    unsigned int cfg_space_size;
    unsigned int mmio_size;
    unsigned int mmio_bar;
};

struct hw_pf_info {
    unsigned int dev_index;
    unsigned int reserved_aicore_num;
    unsigned int reserved_aicpu_num;
    unsigned int reserved_jpegd_num;
    unsigned long reserved_mem_size;
    unsigned int instance_num;
    unsigned int aicore_num;
    unsigned long mem_size;
    unsigned int aicpu_num;
    unsigned int vpc_num;
    unsigned int jpegd_num;
    unsigned int jpege_num;
    unsigned int venc_num;
    unsigned int vdec_num;
    unsigned long ddrmem_size;
    unsigned long hbmmem_size;

    /* vDavinci IDR pool */
    struct idr vdavinci_idr;
};

struct hw_vdavinci_type {
    char template_name[HW_DVT_MAX_TYPE_NAME];
    int type;
    unsigned int bar0_size;
    unsigned int bar2_size;
    unsigned long bar4_size;
    unsigned int aicore_num;
    unsigned long mem_size;
    unsigned int aicpu_num;
    unsigned int vpc_num;
    unsigned int jpegd_num;
    unsigned int jpege_num;
    unsigned int venc_num;
    unsigned int vdec_num;
    unsigned int share;
    unsigned int vf_num;
    unsigned long ddrmem_size;
    unsigned long hbmmem_size;
    unsigned int dev_index;
    unsigned int vfg_id;
    unsigned int avail_instance;
    char name[HW_DVT_MAX_TYPE_NAME];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
    struct mdev_type mtype;
#endif
};

struct vf_used_map {
    bool used;
    struct pci_dev *vf;
    struct hw_vdavinci *vdavinci;
};

struct hw_dvt {
    struct mutex lock;
    struct hw_dvt_device_info device_info;
    struct hw_vdavinci_type *types;
    struct attribute_group **groups;
    struct mdev_type **mdev_types;
    unsigned short vendor;
    unsigned short device;
    int (*mmio_init)(struct hw_vdavinci *vdavinci);
    void (*mmio_uninit)(struct hw_vdavinci *vdavinci);
    struct vdavinci_priv *vdavinci_priv;
    bool is_sriov_enabled;
    struct {
        unsigned int vf_num;
        unsigned int vf_used;
        struct vf_used_map *vf_array;
    } sriov;
    struct dentry *debugfs_root;
    unsigned int dev_num;
    unsigned int vdavinci_type_num;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
    struct mdev_parent parent;
#else
    struct mdev_parent_ops *vdavinci_mdev_ops;
#endif /* KERNEL_VERSION(6,1,0) */
    struct mdev_driver *drv;
    struct hw_pf_info pf[HW_DVT_MAX_DEV_NUM];
};

struct hw_vdavinci_pci_bar {
    unsigned long size;
    bool tracked;
};

struct hw_vdavinci_cfg_space {
    unsigned char config[PCI_CFG_SPACE_EXP_SIZE];
    struct hw_vdavinci_pci_bar bar[HW_DVT_MAX_BAR_NUM];
    void (*init_cfg_space)(struct hw_vdavinci *vdavinci);
};

#define vdavinci_cfg_space(vdavinci) ((vdavinci)->cfg_space.config)

struct hw_vdavinci_mmio {
    void *msix_base;
    void *io_base;
    void *mem_base;
    struct vdavinci_mapinfo bar0_sparse;
    struct vdavinci_mapinfo bar2_sparse;
    struct vdavinci_mapinfo bar4_sparse;
    struct vdavinci_mapinfo mem_sparse;
};

struct hw_vf_info {
    struct pci_dev        *pdev;
    int                   irq_type;
    struct iommu_domain   *domain;
    struct iova_domain    iovad;
};

struct vdavinci_ioeventfd {
    struct list_head next;
    struct hw_vdavinci *vdavinci;
    struct virqfd *virqfd;
    uint64_t data;
    loff_t pos;
    int bar;
    int count;
};

struct hw_vdavinci {
    struct list_head list;
    struct hw_dvt *dvt;
    struct hw_vdavinci_type *type;
    struct mutex vdavinci_lock;
    struct vdavinci_dev dev;
    unsigned int vfg_id;
    u32 id;
    /* vDavinci handle used by hypervisor MPT modules */
    uintptr_t handle;
    bool active;

    struct hw_vdavinci_cfg_space cfg_space;
    struct hw_vdavinci_mmio mmio;
    struct task_struct *qemu_task;
    cpumask_t vm_cpus_mask;

    struct {
        struct dentry *debugfs;
        unsigned long long notify_count;
        unsigned long long *msix_count;
        int nvec;
        struct dentry *debugfs_cache_info;
    } debugfs;

    struct {
        struct mdev_device *mdev;
        struct vfio_region *region;
        u32 num_regions;
        struct eventfd_ctx *intx_trigger;
        struct eventfd_ctx **msix_triggers;
        struct mutex cache_lock;
        struct notifier_block iommu_notifier;
        struct notifier_block group_notifier;
        struct kvm *kvm;
        struct work_struct release_work;
        atomic_t released;
        struct vfio_device *vfio_device;
        struct vfio_group *vfio_group;
        void *domain;
        struct list_head dev_dma_info_list_head;
    } vdev;

    bool is_passthrough;
    struct hw_vf_info vf;
    int vf_index;
    /* eventfd */
    int ioeventfds_nr;
    struct mutex ioeventfds_lock;
    struct list_head ioeventfds_list;
};

struct hw_vdavinci *find_vdavinci(struct device *dev);
typedef struct vfio_group *(*get_vfio_group)(struct device *dev);
typedef void (*put_vfio_group)(struct vfio_group *group);
typedef int (*dma_rw)(struct vfio_group *group, dma_addr_t user_iova,
                      void *data, size_t len, bool write);

struct mmio_init_ops {
    unsigned short device;
    int (*mmio_init)(struct hw_vdavinci *vdavinci);
    void (*mmio_uninit)(struct hw_vdavinci *vdavinci);
};

bool handle_valid(uintptr_t handle);
struct vdavinci_priv *kdev_to_davinci(struct device *kdev);
int hw_dvt_init_vdavinci_types(struct hw_dvt *dvt);
void hw_dvt_clean_vdavinci_types(struct hw_dvt *dvt);
void hw_dvt_update_vdavinci_types(struct hw_dvt *dvt, unsigned int dev_index);

struct hw_vdavinci *hw_dvt_create_vdavinci(struct hw_dvt *dvt,
                                           struct hw_vdavinci_type *type, uuid_le uuid);
void hw_dvt_destroy_vdavinci(struct hw_vdavinci *vdavinci);
void hw_dvt_release_vdavinci(struct hw_vdavinci *vdavinci);
int hw_dvt_reset_vdavinci(struct hw_vdavinci *vdavinci);
void hw_dvt_activate_vdavinci(struct hw_vdavinci *vdavinci);
void hw_dvt_deactivate_vdavinci(struct hw_vdavinci *vdavinci);

int hw_vdavinci_emulate_cfg_read(struct hw_vdavinci *vdavinci,
                                 unsigned int offset, void *buf, unsigned int bytes);
int hw_vdavinci_emulate_cfg_write(struct hw_vdavinci *vdavinci,
                                  unsigned int offset, void *buf, unsigned int bytes);
int hw_vdavinci_emulate_mmio_read(struct hw_vdavinci *vdavinci,
                                  uint64_t pa, void *buf, unsigned int bytes);
int hw_vdavinci_emulate_mmio_write(struct hw_vdavinci *vdavinci,
                                   uint64_t pa, void *buf, unsigned int bytes);
void hw_vdavinci_init_cfg_space(struct hw_vdavinci *vdavinci);
void hw_vdavinci_reset_cfg_space(struct hw_vdavinci *vdavinci);
void hw_dvt_debugfs_add_vdavinci(struct hw_vdavinci *vdavinci);
void hw_dvt_debugfs_remove_vdavinci(struct hw_vdavinci *vdavinci);
void hw_dvt_debugfs_init(struct hw_dvt *dvt);
void hw_dvt_debugfs_clean(struct hw_dvt *dvt);
bool davinci_vfg_support(unsigned short vendor, unsigned short device);
int get_reserve_iova_for_check(struct device *dev, dma_addr_t *iova_addr, size_t *size);

struct hw_vdavinci_ops {
    int (*emulate_cfg_read)(struct hw_vdavinci *vdavinci, unsigned int offset,
            void *buf, unsigned int bytes);
    int (*emulate_cfg_write)(struct hw_vdavinci *vdavinci, unsigned int offset,
            void *buf, unsigned int bytes);
    int (*emulate_mmio_read)(struct hw_vdavinci *vdavinci, uint64_t pa,
            void *buf, unsigned int bytes);
    int (*emulate_mmio_write)(struct hw_vdavinci *vdavinci, uint64_t pa,
            void *buf, unsigned int bytes);
    struct hw_vdavinci *(*vdavinci_create)(struct hw_dvt *dvt,
            struct hw_vdavinci_type *type, uuid_le uuid);
    void (*vdavinci_destroy)(struct hw_vdavinci *vdavinci);
    void (*vdavinci_release)(struct hw_vdavinci *vdavinci);
    int (*vdavinci_reset)(struct hw_vdavinci *vdavinci);
    void (*vdavinci_activate)(struct hw_vdavinci *vdavinci);
    void (*vdavinci_deactivate)(struct hw_vdavinci *vdavinci);
    struct hw_vdavinci_type *(*dvt_find_vdavinci_type)(struct hw_dvt *dvt,
            const char *name);
};

struct  vdavinci_drv_ops {
    int (*vdavinci_init)(void *vdavinci_priv);
    int (*vdavinci_uninit)(void *vdavinci_priv);
    int (*vdavinci_hypervisor_inject_msix)(void *__vdavinci, u32 vector);
    int (*vdavinci_hypervisor_read_gpa)(void *__vdavinci, unsigned long gpa, void *buf, unsigned long len);
    int (*vdavinci_hypervisor_write_gpa)(void *__vdavinci, unsigned long gpa, void *buf, unsigned long len);
    unsigned long (*vdavinci_hypervisor_gfn_to_mfn)(void *__vdavinci, unsigned long gfn);
    int (*vdavinci_hypervisor_dma_pool_init)(void *__vdavinci);
    void (*vdavinci_hypervisor_dma_pool_uninit)(void *__vdavinci);
    int (*vdavinci_hypervisor_dma_map_guest_page)(void *__vdavinci, unsigned long gfn, unsigned long size,
        struct sg_table **dma_sgt);
    void (*vdavinci_hypervisor_dma_unmap_guest_page)(void *__vdavinci, struct sg_table *dma_sgt);
    bool (*vdavinci_hypervisor_dma_pool_active)(void *__vdavinci);
    int (*vdavinci_hypervisor_dma_map_guest_page_batch)(void *__vdavinci, unsigned long *gfn, unsigned long *dma_addr,
        unsigned long count);
    void (*vdavinci_hypervisor_dma_unmap_guest_page_batch)(void *__vdavinci, unsigned long *gfn,
        unsigned long *dma_addr, unsigned long count);
    bool (*vdavinci_hypervisor_is_valid_gfn)(void *__vdavinci, unsigned long gfn);
    int (*vdavinci_hypervisor_mmio_get)(void **dst, int *size, void *__vdavinci, int bar);
    void *(*vdavinci_hypervisor_dma_alloc_coherent)(struct device *dev, size_t size,
        dma_addr_t *dma_handle, gfp_t gfp);
    void (*vdavinci_hypervisor_dma_free_coherent)(struct device *dev, size_t size,
        void *cpu_addr, dma_addr_t dma_handle);
    dma_addr_t (*vdavinci_hypervisor_dma_map_single)(struct device *dev, void *ptr, size_t size,
        enum dma_data_direction dir);
    void (*vdavinci_hypervisor_dma_unmap_single)(struct device *dev, dma_addr_t addr, size_t size,
        enum dma_data_direction dir);
    dma_addr_t (*vdavinci_hypervisor_dma_map_page)(struct device *dev, struct page *page, size_t offset,
        size_t size, enum dma_data_direction dir);
    void (*vdavinci_hypervisor_dma_unmap_page)(struct device *dev, dma_addr_t addr, size_t size,
        enum dma_data_direction dir);
    int (*vdavinci_get_reserve_iova_for_check)(struct device *dev, dma_addr_t *iova_addr, size_t *size);
};

extern struct hw_vdavinci_ops g_hw_vdavinci_ops;
struct device *vdavinci_resource_dev(struct hw_vdavinci *vdavinci);
struct device *vdavinci_get_device(struct hw_vdavinci *vdavinci);
struct device *vdavinci_to_dev(struct hw_vdavinci *vdavinci);
extern int hw_dvt_get_mode(int *mode);
extern struct hw_kvmdt_ops g_hw_kvmdt_ops;
bool hw_vdavinci_sriov_support(struct hw_dvt *dvt);
bool hw_vdavinci_is_enabled(struct hw_dvt *dvt);
bool hw_vdavinci_vf_used_num_zero(struct hw_dvt *dvt);
bool hw_vdavinci_priv_callback_check(struct vdavinci_priv *vdavinci_priv);
int hw_dvt_init_host(void);
int hw_dvt_init_device(struct vdavinci_priv *vdavinci_priv);
int hw_dvt_init_dev_pf_info(struct hw_dvt *dvt);
void hw_dvt_uninit_dev_pf_info(struct hw_dvt *dvt);
int hw_dvt_uninit_device(struct vdavinci_priv *vdavinci_priv);
extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);
int register_vdavinci_virtual_ops(struct vdavinci_drv_ops *ops);
void unregister_vdavinci_virtual_ops(void);
int hw_dvt_set_mmio_ops(struct hw_dvt *dvt, struct mmio_init_ops *ops);
int hw_dvt_sriov_enable(struct pci_dev *dev, int num_vfs);
extern struct mmio_init_ops vdavinci_mmio_pf_devices_ops[];
extern struct mmio_init_ops vdavinci_mmio_vf_devices_ops[];
unsigned int hw_dvt_get_used_aicpu_num(struct hw_dvt *dvt, unsigned int dev_index);

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)) || (defined(DRV_UT)))
/* vdavinci only enables eventfd support for BAR0 doorbell region */
long hw_vdavinci_set_ioeventfd(struct hw_vdavinci *vdavinci, loff_t offset, uint64_t data,
                               int count, int fd);
void hw_vdavinci_ioeventfd_deactive(struct hw_vdavinci *vdavinci,
                                    struct vdavinci_ioeventfd *ioeventfd);
#endif

ssize_t hw_vdavinci_rw(struct hw_vdavinci *vdavinci, char *buf,
                       size_t count, loff_t *ppos, bool write);
int init_vdavinci_type(struct hw_vdavinci_type *type,
                       struct vdavinci_type *tp);
#endif /* DVT_H_ */

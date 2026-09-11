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

#ifndef HW_VDAVINCI_H_
#define HW_VDAVINCI_H_

#include <linux/version.h>
#include <linux/list.h>
#include <linux/scatterlist.h>
#include <linux/uuid.h>
#include <linux/dma-mapping.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
#include <linux/mei_uuid.h>
#endif

#define RESERVE_SIZE 40
#define HW_DVT_MAX_TYPE_NAME 32
#define HW_BAR_SPARSE_MAP_MAX 16

enum ASCEND_310I_TYPE {
    TYPE_VIR01_310I,
    TYPE_VIR02_310I,
    TYPE_VIR02_1C_310I,
    TYPE_VIR04_310I,
    TYPE_VIR04_3C_310I,
    TYPE_VIR04_3C_NDVPP_310I,
    TYPE_VIR04_4C_DVPP_310I,
    TYPE_MAX_310I
};

enum ASCEND_310V_TYPE {
    TYPE_VIR01_310V,
    TYPE_VIR02_310V,
    TYPE_VIR02_1C_310V,
    TYPE_VIR04_310V,
    TYPE_VIR04_3C_310V,
    TYPE_VIR04_3C_NDVPP_310V,
    TYPE_VIR04_4C_DVPP_310V,
    TYPE_MAX_310V
};

enum ASCEND_910_TYPE {
    TYPE_VIR02_910,
    TYPE_VIR04_910,
    TYPE_VIR08_910,
    TYPE_VIR16_910,
    TYPE_MAX_910
};

/* 24core, 64G */
enum HW_VDAVINCI_TYPE_910B_V1 {
    TYPE_VIR06_2C_16G,
    TYPE_VIR06_1C_16G,
    TYPE_VIR12_3C_32G_NM,
    TYPE_VIR12_4C_32G_M,
    TYPE_VIR12_3C_32G,
    TYPE_VIR12_4C_32G,
    TYPE_MAX_910B_V1
};

/* 20core, 32G */
enum HW_VDAVINCI_TYPE_910B_V2 {
    TYPE_VIR05_1C_8G,
    TYPE_VIR05_2C_8G,
    TYPE_VIR10_3C_16G_NM,
    TYPE_VIR10_4C_16G_M,
    TYPE_VIR10_3C_16G,
    TYPE_VIR10_4C_16G,
    TYPE_MAX_910B_V2
};

/* 20core, 64G */
enum HW_VDAVINCI_TYPE_910B_V3 {
    TYPE_VIR05_1C_16G,
    TYPE_VIR05_2C_16G,
    TYPE_VIR10_3C_32G_NM,
    TYPE_VIR10_4C_32G_M,
    TYPE_VIR10_3C_32G,
    TYPE_VIR10_4C_32G,
    TYPE_MAX_910B_V3
};

/* 24core, 64G */
enum HW_VDAVINCI_TYPE_910_93_V1 {
    TYPE_VIR06_2C_16G_910_93_V1,
    TYPE_VIR06_1C_16G_910_93_V1,
    TYPE_VIR12_3C_32G_NM_910_93_V1,
    TYPE_VIR12_4C_32G_M_910_93_V1,
    TYPE_VIR12_3C_32G_910_93_V1,
    TYPE_VIR12_4C_32G_910_93_V1,
    TYPE_VIR24_7C_64G_910_93_V1,
    TYPE_MAX_910_93_V1
};

/* 20core, 32G */
enum HW_VDAVINCI_TYPE_910_93_V2 {
    TYPE_VIR05_1C_8G_910_93_V2,
    TYPE_VIR05_2C_8G_910_93_V2,
    TYPE_VIR10_3C_16G_NM_910_93_V2,
    TYPE_VIR10_4C_16G_M_910_93_V2,
    TYPE_VIR10_3C_16G_910_93_V2,
    TYPE_VIR10_4C_16G_910_93_V2,
    TYPE_VIR20_7C_32G_910_93_V2,
    TYPE_MAX_910_93_V2
};

/* 20core, 64G */
enum HW_VDAVINCI_TYPE_910_93_V3 {
    TYPE_VIR05_1C_16G_910_93_V3,
    TYPE_VIR05_2C_16G_910_93_V3,
    TYPE_VIR10_3C_32G_NM_910_93_V3,
    TYPE_VIR10_4C_32G_M_910_93_V3,
    TYPE_VIR10_3C_32G_910_93_V3,
    TYPE_VIR10_4C_32G_910_93_V3,
    TYPE_VIR20_7C_64G_910_93_V3,
    TYPE_MAX_910_93_V3
};

/* 36core, 128G */
enum HW_VDAVINCI_TYPE_950_BIN0 {
    TYPE_VIR16_7C_60G_BIN0,
    TYPE_VIR08_3C_30G_BIN0,
    TYPE_VIR04_1C_15G_BIN0,
    TYPE_MAX_950_BIN0
};
 
/* 32core, 128G */
enum HW_VDAVINCI_TYPE_950_BIN1 {
    TYPE_VIR16_7C_60G_BIN1,
    TYPE_VIR08_3C_30G_BIN1,
    TYPE_VIR04_1C_15G_BIN1,
    TYPE_MAX_950_BIN1
};
 
/* 28core, 128G */
enum HW_VDAVINCI_TYPE_950_BIN2 {
    TYPE_VIR14_5C_60G_BIN2,
    TYPE_VIR07_2C_30G_BIN2,
    TYPE_MAX_950_BIN2
};
 
/* 28core, 112G */
enum HW_VDAVINCI_TYPE_950_BIN3 {
    TYPE_VIR14_5C_52G_BIN3,
    TYPE_VIR07_2C_26G_BIN3,
    TYPE_MAX_950_BIN3
};

struct dvt_devinfo {
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
};

struct vdavinci_type {
    char template_name[HW_DVT_MAX_TYPE_NAME];
    int type;
    unsigned int bar0_size;
    unsigned int bar2_size;
    unsigned long bar4_size;    /* if non-zero, use this value first */
    unsigned int aicore_num;
    unsigned long mem_size;
    unsigned int aicpu_num;
    unsigned int vpc_num;
    unsigned int jpegd_num;
    unsigned int jpege_num;
    unsigned int venc_num;
    unsigned int vdec_num;
    unsigned int share;         /* the minimum value is 1 */
    unsigned long ddrmem_size;
    unsigned long hbmmem_size;
};

/**
 * There are 3 types of vdavinci bar space memory implementation:
 * trap: no back-end memory, simulating read and write instructions;
 * backend: apply for a piece of memory on the host and map it to the virtual machine;
 * passthrough: pass through the physical device bar space memory to the virtual machine
 */
enum HW_MAP_TYPE {
    MAP_TYPE_TRAP,
    MAP_TYPE_BACKEND,
    MAP_TYPE_PASSTHROUGH,
    MAP_TYPE_INVALID,
};

typedef enum {
    VDAVINCI_DOCKER,
    VDAVINCI_VM,
    VDAVINCI_MODE_MAX,
} VDAVINCI_MODE;

struct vdavinci_bar_map {
    size_t offset;
    enum HW_MAP_TYPE map_type;
    union {
        u64 paddr;      /* vaild when map_type is MAP_TYPE_PASSTHROUGH */
        void *vaddr;    /* vaild when map_type is MAP_TYPE_BACKEND */
    };
    size_t size;
};

struct vdavinci_mapinfo {
    u64 num;
    struct vdavinci_bar_map map_info[HW_BAR_SPARSE_MAP_MAX];
};

struct vdavinci_dev {
    struct device *dev;
    u32 dev_index;
    u32 fid;
    struct device *resource_dev;
};

/**
 * dev_index value [0, 1 ...]
 */
struct vdavinci_priv_ops {
    int (*vdavinci_create)(struct vdavinci_dev *dev, void *vdavinci,
                           struct vdavinci_type *type, uuid_le uuid);
    void (*vdavinci_destroy)(struct vdavinci_dev *dev);
    void (*vdavinci_release)(struct vdavinci_dev *dev);
    int (*vdavinci_reset)(struct vdavinci_dev *dev);
    int (*vdavinci_flr)(struct vdavinci_dev *dev);
    void (*vdavinci_notify)(struct vdavinci_dev *dev, int db_index);
    int (*vdavinci_getmapinfo)(struct vdavinci_dev *dev,
                               struct vdavinci_type *type, u32 bar_id,
                               struct vdavinci_mapinfo *mapinfo);
    int (*vdavinci_putmapinfo)(struct vdavinci_dev *dev);
    int (*davinci_getdevnum)(struct device *dev);
    int (*davinci_getdevinfo)(struct device *dev, u32 dev_index,
            struct dvt_devinfo* dev_info);
    int (*vascend_enable_sriov)(struct pci_dev *pdev, int numvfs);
};

struct vdavinci_priv {
    struct device *dev;
    void *dvt;
    struct vdavinci_priv_ops *ops;
    char reserve[RESERVE_SIZE];
};

int hw_dvt_init(void *vdavinci_priv);
int hw_dvt_uninit(void *vdavinci_priv);

/**
 * hw_dvt_hypervisor_inject_msix - inject a MSIX interrupt into vdavinci
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_inject_msix(void *__vdavinci, u32 vector);

/**
 * hw_dvt_hypervisor_read_gpa - copy data from GPA to host data buffer
 * @vdavinci: a vdavinci
 * @gpa: guest physical addr
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_read_gpa(void *__vdavinci, unsigned long gpa,
                               void *buf, unsigned long len);

/**
 * hw_dvt_hypervisor_write_gpa - copy data from host data buffer to GPA
 * @vdavinci: a vdavinci
 * @gpa: guest physical addr
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_write_gpa(void *__vdavinci, unsigned long gpa,
                                void *buf, unsigned long len);

/**
 * hw_dvt_hypervisor_gfn_to_mfn - translate a GFN to MFN
 * @vdavinci: a vdavinci
 * @gpfn: guest pfn
 *
 * Returns:
 * MFN on success, hw_dvt_INVALID_ADDR if failed.
 */
unsigned long hw_dvt_hypervisor_gfn_to_mfn(void *__vdavinci,
                                           unsigned long gfn);

/**
 * hw_dvt_hypervisor_dma_pool_init - dma pool init
 * @vdavinci: a vdavinci
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_pool_init(void *__vdavinci);

/**
 * hw_dvt_hypervisor_dma_pool_uninit - dma pool uninit
 * @vdavinci: a vdavinci
 */
void hw_dvt_hypervisor_dma_pool_uninit(void *__vdavinci);

/**
 * hw_dvt_hypervisor_dma_map_guest_page - setup dma map for guest page
 * @vdavinci: a vdavinci
 * @gfn: guest pfn
 * @size: page size
 * @dma_sgt: retrieve allocated dma addr list(sg_table)
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_map_guest_page(void *__vdavinci, unsigned long gfn,
                                         unsigned long size,
                                         struct sg_table **dma_sgt);

/**
 * hw_dvt_hypervisor_dma_unmap_guest_page - cancel dma map for guest page
 * @vdavinci: a vdavinci
 * @dma_sgt: the dma addr list(sg_table)
 */
void hw_dvt_hypervisor_dma_unmap_guest_page(void *__vdavinci,
                                            struct sg_table *dma_sgt);

/**
 * hw_dvt_hypervisor_dma_pool_active - dma pool active or not
 * @vdavinci: a vdavinci
 *
 * Returns:
 * true on active, false on inactive.
 */
bool hw_dvt_hypervisor_dma_pool_active(void *__vdavinci);

/**
 * hw_dvt_hypervisor_dma_map_guest_page_batch
 * - setup dma map for guest page when dma pool is active
 * @vdavinci: a vdavinci
 * @gfn: guest pfn array
 * @dma_addr: retrieve dma addr array
 * @count: array size
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_map_guest_page_batch(void *__vdavinci,
                                               unsigned long *gfn,
                                               unsigned long *dma_addr,
                                               unsigned long count);

/**
 * hw_dvt_hypervisor_dma_unmap_guest_page_batch
 * - cancel dma map for guest page
 * @vdavinci: a vdavinci
 * @gfn: guest pfn array
 * @dma_addr: retrieve dma addr array
 * @count: array size
 */
void hw_dvt_hypervisor_dma_unmap_guest_page_batch(void *__vdavinci,
                                                  unsigned long *gfn,
                                                  unsigned long *dma_addr,
                                                  unsigned long count);
/**
 * hw_dvt_hypervisor_is_valid_gfn - check if a visible gfn
 * @vdavinci: a vdavinci
 * @gfn: guest PFN
 *
 * Returns:
 * true on valid gfn, false on not.
 */
bool hw_dvt_hypervisor_is_valid_gfn(void *__vdavinci, unsigned long gfn);

/**
 * hw_dvt_hypervisor_mmio_get - get the mmio address and size of specified bar
 * @dst: return the mmio address to host driver
 * @size: retrun the mmio size to host driver
 * @vdavinci: a vdavinci
 * @bar: specify the bar[2/4]
 *
 * Returns:
 * true on valid gfn, false on not.
 */

int hw_dvt_hypervisor_mmio_get(void **dst, int *size, void *__vdavinci, int bar);


void *vdavinci_dma_alloc_coherent(struct device *dev, size_t size,
    dma_addr_t *dma_handle, gfp_t flags);
void vdavinci_dma_free_coherent(struct device *dev, size_t size,
    void *vaddr, dma_addr_t dma_handle);
dma_addr_t vdavinci_dma_map_single(struct device *dev, void *ptr, size_t size,
    enum dma_data_direction dir);
dma_addr_t vdavinci_dma_map_page(struct device *dev, struct page *page, size_t offset,
    size_t size, enum dma_data_direction dir);
void vdavinci_dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir);
void vdavinci_dma_unmap_page(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir);

#endif /* HW_VDAVINCI_H_ */

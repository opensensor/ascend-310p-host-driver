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

#ifndef HW_VDAVINCI_H_
#define HW_VDAVINCI_H_

#include "ka_driver_pub.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_pci_pub.h"

#define RESERVE_SIZE 40
#define HW_DVT_MAX_TYPE_NAME 32
#define HW_BAR_SPARSE_MAP_MAX 16

enum HW_VDAVINCI_TYPE {
    HW_VDAVINCI_TYPE_C1,
    HW_VDAVINCI_TYPE_C2,
    HW_VDAVINCI_TYPE_C4,
    HW_VDAVINCI_TYPE_C8,
    HW_VDAVINCI_TYPE_C16,
    HW_VDAVINCI_TYPE_MAX
};

enum ASCEND_310I_TYPE {
    TYPE_VIR01_310I,
    TYPE_VIR02_310I,
    TYPE_VIR02_1U_310I,
    TYPE_VIR04_310I,
    TYPE_VIR04_3U_310I,
    TYPE_VIR04_3U_NM_310I,
    TYPE_VIR04_4U_M_310I,
    TYPE_MAX_310I
};

enum ASCEND_310V_TYPE {
    TYPE_VIR01_310V,
    TYPE_VIR02_310V,
    TYPE_VIR02_1U_310V,
    TYPE_VIR04_310V,
    TYPE_VIR04_3U_310V,
    TYPE_VIR04_3U_NM_310V,
    TYPE_VIR04_4U_M_310V,
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
    TYPE_VIR03_HC_8G,
    TYPE_VIR06_1C_16G,
    TYPE_VIR12_3C_32G_NM,
    TYPE_VIR12_3C_32G_M,
    TYPE_VIR12_3C_32G,
    TYPE_MAX_910B_V1
};

/* 20core, 32G */
enum HW_VDAVINCI_TYPE_910B_V2 {
    TYPE_VIR05_1C_8G,
    TYPE_VIR10_3C_16G_NM,
    TYPE_VIR10_3C_16G_M,
    TYPE_VIR10_3C_16G,
    TYPE_MAX_910B_V2
};

/* 20core, 64G */
enum HW_VDAVINCI_TYPE_910B_V3 {
    TYPE_VIR05_1C_16G,
    TYPE_VIR10_3C_32G_NM,
    TYPE_VIR10_3C_32G_M,
    TYPE_VIR10_3C_32G,
    TYPE_MAX_910B_V3
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
    ka_device_t *dev;
    u32 dev_index;
    u32 fid;
    ka_device_t *resource_dev;
};

struct vdavinci_priv_ops {
    int (*vdavinci_create)(struct vdavinci_dev *dev, void *vdavinci,
                           struct vdavinci_type *type, ka_uuid_le_t uuid);
    void (*vdavinci_destroy)(struct vdavinci_dev *dev);
    void (*vdavinci_release)(struct vdavinci_dev *dev);
    int (*vdavinci_reset)(struct vdavinci_dev *dev);
    int (*vdavinci_flr)(struct vdavinci_dev *dev);
    void (*vdavinci_notify)(struct vdavinci_dev *dev, int db_index);
    int (*vdavinci_getmapinfo)(struct vdavinci_dev *dev,
                               struct vdavinci_type *type, u32 bar_id,
                               struct vdavinci_mapinfo *mapinfo);
    int (*vdavinci_putmapinfo)(struct vdavinci_dev *dev);
    int (*davinci_getdevnum)(ka_device_t *dev);
    int (*davinci_getdevinfo)(ka_device_t *dev, u32 dev_index,
            struct dvt_devinfo* dev_info);
    int (*vascend_enable_sriov)(ka_pci_dev_t *pdev, int numvfs);
};

struct vdavinci_priv {
	ka_device_t *dev;
	void *dvt;
	struct vdavinci_priv_ops *ops;
	char reserve[RESERVE_SIZE];
};

struct vdavinci_drv_ops {
    int (*vdavinci_init)(void *vdavinci_priv);
    int (*vdavinci_uninit)(void *vdavinci_priv);
    int (*vdavinci_hypervisor_inject_msix)(void *__vdavinci, u32 vector);
    int (*vdavinci_hypervisor_read_gpa)(void *__vdavinci, unsigned long gpa, void *buf, unsigned long len);
    int (*vdavinci_hypervisor_write_gpa)(void *__vdavinci, unsigned long gpa, void *buf, unsigned long len);
    unsigned long (*vdavinci_hypervisor_gfn_to_mfn)(void *__vdavinci, unsigned long gfn);
    int (*vdavinci_hypervisor_dma_pool_init)(void *__vdavinci);
    void (*vdavinci_hypervisor_dma_pool_uninit)(void *__vdavinci);
    int (*vdavinci_hypervisor_dma_map_guest_page)(void *__vdavinci, unsigned long gfn, unsigned long size,
        ka_sg_table_t **dma_sgt);
    void (*vdavinci_hypervisor_dma_unmap_guest_page)(void *__vdavinci, ka_sg_table_t *dma_sgt);
    bool (*vdavinci_hypervisor_dma_pool_active)(void *__vdavinci);
    int (*vdavinci_hypervisor_dma_map_guest_page_batch)(void *__vdavinci, unsigned long *gfn, unsigned long *dma_addr,
        unsigned long count);
    void (*vdavinci_hypervisor_dma_unmap_guest_page_batch)(void *__vdavinci, unsigned long *gfn,
        unsigned long *dma_addr, unsigned long count);
    bool (*vdavinci_hypervisor_is_valid_gfn)(void *__vdavinci, unsigned long gfn);
    int (*vdavinci_hypervisor_mmio_get)(void **dst, int *size, void *__vdavinci, int bar);
    void *(*vdavinci_hypervisor_dma_alloc_coherent)(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_handle, ka_gfp_t gfp);
    void (*vdavinci_hypervisor_dma_free_coherent)(ka_device_t *dev, size_t size,
                                                  void *cpu_addr, ka_dma_addr_t dma_handle);
    ka_dma_addr_t (*vdavinci_hypervisor_dma_map_single)(ka_device_t *dev, void *ptr,
                                                     size_t size, ka_dma_data_direction_t dir);
    void (*vdavinci_hypervisor_dma_unmap_single)(ka_device_t *dev, ka_dma_addr_t addr,
                                                 size_t size, ka_dma_data_direction_t dir);
    ka_dma_addr_t (*vdavinci_hypervisor_dma_map_page)(ka_device_t *dev, ka_page_t *page, size_t offset,
        size_t size, ka_dma_data_direction_t dir);
    void (*vdavinci_hypervisor_dma_unmap_page)(ka_device_t *dev, ka_dma_addr_t addr, size_t size,
        ka_dma_data_direction_t dir);
    int (*vdavinci_get_reserve_iova_for_check)(ka_device_t *dev, ka_dma_addr_t *iova_addr, size_t *size);

};

typedef enum {
    VDAVINCI_DOCKER,
    VDAVINCI_VM,
    VDAVINCI_MODE_MAX,
}VDAVINCI_MODE;

struct vdavinci_mode_info {
    VDAVINCI_MODE mode;
    ka_rw_semaphore_t rw_sem;
};

struct vdavinci_drv_ops *get_vdavinci_virtual_ops(void);
bool hw_dvt_check_is_vm_mode(void);
int hw_dvt_init(void *dev_priv);
int hw_dvt_uninit(void *dev_priv);

int register_vdavinci_virtual_ops(struct vdavinci_drv_ops *ops);
void unregister_vdavinci_virtual_ops(void);
int hw_dvt_set_mode(int mode);
int hw_dvt_get_mode(int *mode);

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
 * @gpa: guest physical address
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_read_gpa(void *vdavinci, unsigned long gpa,
			       void *buf, unsigned long len);

/**
 * hw_dvt_hypervisor_write_gpa - copy data from host data buffer to GPA
 * @vdavinci: a vdavinci
 * @gpa: guest physical address
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_write_gpa(void *vdavinci, unsigned long gpa,
				void *buf, unsigned long len);

/**
 * hw_dvt_hypervisor_gfn_to_mfn - translate a GFN to MFN
 * @vdavinci: a vdavinci
 * @gpfn: guest pfn
 *
 * Returns:
 * MFN on success, hw_dvt_INVALID_ADDR if failed.
 */
unsigned long hw_dvt_hypervisor_gfn_to_mfn(void *vdavinci,
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
					ka_sg_table_t **dma_sgt);

/**
 * hw_dvt_hypervisor_dma_unmap_guest_page - cancel dma map for guest page
 * @vdavinci: a vdavinci
 * @dma_sgt: the dma addr list(sg_table)
 */
void hw_dvt_hypervisor_dma_unmap_guest_page(void *__vdavinci,
					ka_sg_table_t *dma_sgt);

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

int hw_dvt_hypervisor_mmio_get(void **dst, int *size, void *vdavinci, int bar);

void *hw_dvt_hypervisor_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_handle, ka_gfp_t gfp);
void hw_dvt_hypervisor_dma_free_coherent(ka_device_t *dev, size_t size, void *cpu_addr, ka_dma_addr_t dma_handle);
ka_dma_addr_t hw_dvt_hypervisor_dma_map_single(ka_device_t *dev, void *ptr, size_t size,
    ka_dma_data_direction_t dir);
void hw_dvt_hypervisor_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size,
    ka_dma_data_direction_t dir);
ka_dma_addr_t hw_dvt_hypervisor_dma_map_page(ka_device_t *dev, ka_page_t *page, size_t offset,
                                          size_t size, ka_dma_data_direction_t dir);
void hw_dvt_hypervisor_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir);

int hw_dvt_hypervisor_get_reserve_iova(ka_device_t *dev, ka_dma_addr_t *iova_addr, size_t *size);

#endif /* HW_VDAVINCI_H_ */

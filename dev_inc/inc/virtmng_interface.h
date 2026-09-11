/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
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

#ifndef VIRTMNG_INTERFACE_H
#define VIRTMNG_INTERFACE_H
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>

#ifndef LOG_UT
#ifndef AOS_LLVM_BUILD
#include <linux/scatterlist.h>
#endif
#endif
#include "vpc_interface.h"

#define VMNG_PDEV_MAX 64
#define VMNG_VDEV_MAX_PER_PDEV 17 /* 0: physical others: virtual */
#define VMNG_VF_MAX_PER_PF 12     /* 0: physical others: virtual */
#define VMNG_VDEV_FIRST_VFID 1
#define VMNG_VM_MAX 128

#define VMNG_AICORE_MAX_NUM 32
#define VMNG_NUMA_MAX_NUM   25

#define VMNGH_VM 1
#define VMNGH_CONTAINER 2

#define VMNG_PCIE_FLOW_H2D 0
#define VMNG_PCIE_FLOW_D2H 1
#define VMNG_NOT_SUPPORT_BW_CTRL (-1)
#define VMNG_BW_BANDWIDTH_RENEW_CNT 10

#define VMNG_BW_BANDWIDTH_CHECK_LEN 0x400   /* 1k */
#define VMNG_BW_BANDWIDTH_CHECK_WAIT_TIME 5 /* 5ms */
#define VMNG_BW_BANDWIDTH_CHECK_MAX_CNT 900 /* total:4.5s */

#define VMNG_BW_BANDWIDTH_CHECK_SLEEPABLE 0
#define VMNG_BW_BANDWIDTH_CHECK_NON_SLEEP 1

#define VMNG_BANDW_DUPLEX_PERCENTAGE 140
#define VMNG_BANDW_PERCENTAGE_BASE 100

#define VMNGD_TYPE_PF 0
#define VMNGD_TYPE_VF 1

/* Public Define */
enum vmng_vdev_status {
    VMNG_VDEV_STATUS_FREE = 0,
    VMNG_VDEV_STATUS_ALLOC,
    VMNG_VDEV_STATUS_CLIENT_INIT,
    VMNG_VDEV_STATUS_CLIENT_UNINIT,
    VMNG_VDEV_STATUS_REFRESH,
    VMNG_VDEV_STATUS_RESET,
    VMNG_VDEV_STATUS_MAX
};
enum vmng_client_type {
    VMNG_CLIENT_TYPE_DEVMNG = 0,
    VMNG_CLIENT_TYPE_HDC,
    VMNG_CLIENT_TYPE_DEVMM,
    VMNG_CLIENT_TYPE_TSDRV,
    VMNG_CLIENT_TYPE_DVPP,
    VMNG_CLIENT_TYPE_ESCHED,
    VMNG_CLIENT_TYPE_MAX
};
enum vmng_startup_flag_type {
    VMNG_STARTUP_UNPROBED = 0,
    VMNG_STARTUP_PROBED,
    VMNG_STARTUP_TOP_HALF_OK,
    VMNG_STARTUP_BOTTOM_HALF_OK
};
/* dtype for cloud v1 */
enum VMNG_TYPE {
    VMNG_TYPE_C1,
    VMNG_TYPE_C2,
    VMNG_TYPE_C4,
    VMNG_TYPE_C8,
    VMNG_TYPE_C16,
    VMNG_TYPE_MAX
};
/* dtype for cloud v2 */
enum VMNG_HW_TYPE {
    VMNG_HW_TYPE_C1,
    VMNG_HW_TYPE_C2,
    VMNG_HW_TYPE_C4,
    VMNG_HW_TYPE_C6,
    VMNG_HW_TYPE_C12,
    VMNG_HW_TYPE_C24,
    VMNG_HW_TYPE_MAX
};
/* dtype for mini v3 */
enum VMNG_HW_TYPE_EXTRA {
    VMNG_HW_TYPE_C1_4 = 10,
    VMNG_HW_TYPE_C2_4,
    VMNG_HW_TYPE_EXTRA_MAX
};
struct vmng_numa_address {
    u64 start;
    u64 end;
};
struct vmng_vdev_res_ele {
    unsigned long bitmap;
};
struct vmng_vf_memory_info {
    u32 number;
    u64 size;
    struct vmng_vdev_res_ele numa_id;
    struct vmng_numa_address address[VMNG_NUMA_MAX_NUM];
};
struct vmng_stars_res_refresh {
    u32 aiv;
    u32 dsa;
    u32 rtsq;
    u32 cdqm;
    u32 topic_aicpu_slot;
    u32 host_ctrl_cpu;
    u32 device_aicpu;
    u32 host_aicpu;
    u32 jpegd;
    u32 jpege;
    u32 vpc;
    u32 vdec;
    u32 pngd;
    u32 venc;
};

struct vmng_stars_res_static {
    u32 aic;
    u32 c_core;
    u32 ffts;
    u32 sdma;
    u32 pcie_dma;
    u32 acsq;
    u32 event_id;
    u32 notify_id;
    u32 topic_ctrl_cpu_slot;
};
struct vmng_soc_res_base {
    u32 memory;
    u32 memory_spec;
    u64 numa_bitmap;
};

struct vmng_vf_group_refresh {
    u32 vfg_mode; /* 0:strick 1:relax */
    u64 token;
    u64 token_max;
    u64 task_timeout;
};

enum vmng_vf_group_type {
    VMNG_VF_GROUP_TYPE_NORMAL = 0, /* Need to apply for new star resources,add to an existing VFG */
    VMNG_VF_GROUP_TYPE_VIP,        /* Need to apply for new star resources,add to an empty VFG */
    VMNG_VF_GROUP_TYPE_SHARE,      /* Not need to apply for new star resources,share existing VFG */
    VMNG_VF_GROUP_TYPE_MAX
};
enum vmng_vf_group_mode {
    VMNG_VF_GROUP_MODE_STRICT = 0,
    VMNG_VF_GROUP_MODE_RELAX,
    VMNG_VF_GROUP_MODE_MAX
};

struct vmng_vf_sync_remote_id {
    u32 udevid;
};

struct vmng_vf_group_info {
    u32 vfg_id; /* 0~5 */
    u32 vfg_type;
    struct vmng_vf_group_refresh vfg_refresh;
};

#define VMNG_VF_TEMP_NAME_LEN 16
struct vmng_vf_res_info {
    char name[VMNG_VF_TEMP_NAME_LEN];
    u32 vm_full_spec_enable;
    u32 dev_id;
    u32 vfid;
    struct vmng_vf_memory_info memory;
    struct vmng_vf_group_info vfg;
    struct vmng_stars_res_refresh stars_refresh;
    struct vmng_stars_res_static stars_static;
};

struct vmng_soc_res_info {
    struct vmng_soc_res_base base;
    struct vmng_stars_res_refresh stars_refresh;
    struct vmng_stars_res_static stars_static;
};

struct vmng_soc_resource_enquire {
    struct vmng_vf_res_info each;
    struct vmng_soc_res_info remain;
    struct vmng_soc_res_info total;
    struct vmng_soc_res_info vfg;
};
struct vmng_soc_resource_refresh {
    u64 memory;
    struct vmng_stars_res_refresh stars_refresh;
};

#pragma pack(push)
#pragma pack (1)

typedef struct vf_id_info {
    u8 vf_id;
    u8 vfg_mode; /* 0:strict, 1:relax */
    u8 vfg_id; /* pool_id and vfg_id are the same */
    u8 vip;
    u32 reserved0;
    u64 token;
    u64 token_max;
    u64 task_timeout;
    u32 reserved[4];
} vf_id_info_t;

typedef struct vf_ac_info {
    u64 aiv_bitmap;
    u32 aic_bitmap;
    u32 c_core_bitmap;
    u32 dsa_bitmap;
    u32 ffts_bitmap;
    u32 sdma_bitmap;
    u32 pcie_dma_bitmap;
    u32 acsq_slice_bitmap; /* 128/16=8 0x1 meas 0-7 */
    u32 rtsq_slice_bitmap;
    u32 event_slice_bitmap;
    u32 notify_slice_bitmap;
    u32 cdq_slice_bitmap;
    u32 cmo_slice_bitmap;
    u32 reserved[3]; /* reserved space */
} vf_ac_info_t;

typedef struct vf_dvpp_info {
    u32 jpegd_bitmap;
    u32 jpege_bitmap;
    u32 vpc_bitmap;
    u32 vdec_bitmap;
    u32 pngd_bitmap;
    u32 venc_bitmap;
    u32 reserved[4];
} vf_dvpp_info_t;

typedef struct vf_cpu_info {
    u32 topic_aicpu_slot_bitmap;
    u32 topic_ctrl_cpu_slot_bitmap; /* host/device ctrl cpu/tscpu/dvpp cpu/data cpu */
    u32 host_ctrl_cpu_bitmap;
    u32 device_aicpu_bitmap;
    u64 host_aicpu_bitmap;
    u32 reserved[4];
} vf_cpu_info_t;

typedef struct vmng_vf_cfg {
    u32 capbility;
    vf_id_info_t id;   /* identity */
    vf_ac_info_t accelerator;
    vf_cpu_info_t cpu;
    vf_dvpp_info_t dvpp;
    u32 reserved[4];
} vmng_vf_cfg_t;
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct {
    u8 flag; /* 0:No related configuration, 1:configuration exists, 2: bitMap updated */
    u8 totalNum; /* physical core total num, 0:Do not follow this resource */
    u8 minNum; /* Minimum available quantity required */
    u8 reserved;
    u32 freq; /* core working frequency */
    u64 bitMap; /* 1:good, 0:bad */
} vmng_common_pg_info;

typedef struct {
    vmng_common_pg_info cpuPara;
    vmng_common_pg_info aicPara;
    vmng_common_pg_info aivPara;
    vmng_common_pg_info hbmPara;
    vmng_common_pg_info vpcPara;
    vmng_common_pg_info jpegdPara;
    vmng_common_pg_info dvppPara;
    vmng_common_pg_info sioPara;
    vmng_common_pg_info hccsPara;
    vmng_common_pg_info mataPara;
    vmng_common_pg_info l2Para;
    vmng_common_pg_info gpuPara;
} vmng_common_pg;
#pragma pack(pop)

struct vmng_vdev_ctrl {
    u32 status;
    u32 dev_id;
    u32 vfid;
    u32 vm_devid;
    u32 vm_id;
    u32 dtype;
    u32 core_num;
    u32 total_core_num;
    u64 ddr_size;
    u64 hbm_size;
    u64 mem_size;
    u64 bar0_size;
    u64 bar2_size;
    u64 bar4_size;
    vmng_vf_cfg_t vf_cfg;
};
struct vf_bandwidth_ctrl_remote {
    u64 flow_limit;
    u64 hostcpu_flow_cnt[2]; /* 0-H2D 1-D2H */
    u64 ctrlcpu_flow_cnt[2];
    u64 tscpu_flow_cnt[2];
    u64 pack_limit;
    u64 hostcpu_pack_cnt[2];
    u64 ctrlcpu_pack_cnt[2];
    u64 tscpu_pack_cnt[2];
    u64 reserved[2]; /* TS cache refresh must be 64byte aligned  */
};
struct vf_bandwidth_ctrl_local {
    u64 hostcpu_flow_cnt[2];
    u64 ctrlcpu_flow_cnt[2];
    u64 tscpu_flow_cnt[2];
    u64 hostcpu_pack_cnt[2];
    u64 ctrlcpu_pack_cnt[2];
    u64 tscpu_pack_cnt[2];
    u32 read_cnt;
    u32 write_cnt;
};
/* used for pcie bandwidth limit */
struct vmng_bandwith_ctrl {
    struct vf_bandwidth_ctrl_remote *io_base_bwctrl;
    u64 bandwidth;
    u64 packspeed;
    u64 flow_limit[VMNG_VDEV_MAX_PER_PDEV];
    u64 pack_limit[VMNG_VDEV_MAX_PER_PDEV];
    struct vf_bandwidth_ctrl_local local_data[VMNG_VDEV_MAX_PER_PDEV];
};
struct vmng_bandwidth_check_info {
    u32 dev_id;
    u32 vfid;
    u32 dir;
    u32 data_len;
    u32 node_cnt;
    u32 handle_mode;
};
struct vmngh_client_instance {
    void *priv;
    struct vmng_vdev_ctrl *dev_ctrl;
    struct mutex flag_mutex;
    enum vmng_client_type type;
    u32 flag;
    u32 vdev_type;
};
struct vmngh_bar_map {
    size_t offset;
    u64 paddr;
    size_t size;
};
struct vmngh_map_info {
    u64 num;
    struct vmngh_bar_map *map_info;
};
enum vmng_pf_sriov_status {
    VMNGH_PF_SRIOV_DISABLE = 0,
    VMNGH_PF_SRIOV_ENABLE,
    VMNGH_PF_STATUS_MAX
};
struct vmng_sriov_info {
    unsigned int dev_id;
    enum vmng_pf_sriov_status sriov_status;
};
struct vmngh_client {
    enum vmng_client_type type;
    int (*init_instance)(struct vmngh_client_instance *instance);
    int (*uninit_instance)(struct vmngh_client_instance *instance);
    int (*init_container_instance)(struct vmngh_client_instance *instance);
    int (*uninit_container_instance)(struct vmngh_client_instance *instance);
    int (*suspend)(struct vmngh_client_instance *instance);
    int (*sriov_instance)(struct vmng_sriov_info *sriov_info);
};
int vmngh_register_client(struct vmngh_client *client);
int vmngh_unregister_client(struct vmngh_client *client);

struct vmngh_vascend_client {
    enum vmng_client_type type;
    int (*get_map_info)(struct vmngh_client_instance *instance, struct vmngh_map_info *map_info);
    int (*put_map_info)(struct vmngh_client_instance *instance);
};
int vmngh_register_vascend_client(struct vmngh_vascend_client *client);
int vmngh_unregister_vascend_client(struct vmngh_vascend_client *client);

enum vmng_split_mode {
    VMNG_NORMAL_NONE_SPLIT_MODE = 0,
    VMNG_VIRTUAL_SPLIT_MODE,
    VMNG_CONTAINER_SPLIT_MODE,
    VMNG_INVALID_SPLIT_MODE
};

/* @Function: vmng_get_device_split_mode
 * @Description: get device runtime environment
 * @Returun: 0: normal, none split 1: vritual split 2: container split
 */
enum vmng_split_mode vmng_get_device_split_mode(u32 dev_id);

/* agent client */
struct vmnga_ctrl {
    struct device *dev;
    struct pci_bus *bus;
    struct pci_dev *pdev;
    void *unit;
    enum vmng_startup_flag_type startup_flag;
    u32 dev_id;
    u32 dtype;
    u32 core_num;
    u64 ddr_size;
    u64 hbm_size;
};

struct vmnga_client_instance {
    void *priv;
    struct vmnga_ctrl *dev_ctrl;
    struct mutex flag_mutex;
    enum vmng_client_type type;
    u32 flag;
};

struct vmnga_client {
    enum vmng_client_type type;
    int (*init_instance)(struct vmnga_client_instance *instance);
    int (*uninit_instance)(struct vmnga_client_instance *instance);
    int (*suspend)(struct vmnga_client_instance *instance);
};
int vmnga_register_client(struct vmnga_client *client);
int vmnga_unregister_client(struct vmnga_client *client);

/* startup report and state change notify */
typedef enum {
    VMNG_GOING_TO_S0 = 0,
    VMNG_GOING_TO_SUSPEND,
    VMNG_GOING_TO_S3,
    VMNG_GOING_TO_S4,
    VMNG_GOING_TO_D0,
    VMNG_GOING_TO_D3,
    VMNG_GOING_TO_DISABLE_DEV,
    VMNG_GOING_TO_ENABLE_DEV,
    VMNG_STATE_MAX
} vmnga_dev_state;

typedef int (*vmnga_dev_startup_notify)(u32 probe_num, const u32 dev_ids[], u32 array_len, u32 dev_num);
typedef int (*vmnga_dev_state_notify)(u32 dev_id, vmnga_dev_state state);
void vmnga_register_dev_startup_callback(vmnga_dev_startup_notify startup_notify);
void vmnga_register_dev_state_callback(vmnga_dev_state_notify state_notify);

/* interrupt region, doorbell and msix */
enum vmng_get_irq_type {
    VMNG_GET_IRQ_TYPE_TSDRV = 0,
    VMNG_IRQ_TYPE_MAX
};

typedef int (*db_handler_t)(int, void *);
int vmngh_get_local_db(u32 dev_id, u32 fid, enum vmng_get_irq_type type, u32 *db_base, u32 *db_num);
int vmngh_register_local_db(u32 dev_id, u32 fid, u32 db_index, db_handler_t handler, void *data);
int vmngh_unregister_local_db(u32 dev_id, u32 fid, u32 db_index, const void *data);
int vmnga_get_remote_db(u32 dev_id, enum vmng_get_irq_type type, u32 *db_base, u32 *db_num);
int vmnga_trigger_remote_db(u32 dev_id, u32 db_index);
int vmngh_get_remote_msix(u32 dev_id, u32 fid, enum vmng_get_irq_type type, u32 *msix_base, u32 *msix_num);
int vmngh_trigger_remote_msix(u32 dev_id, u32 fid, u32 msix_index);
int vmnga_get_local_msix(u32 dev_id, enum vmng_get_irq_type type, u32 *msix_base, u32 *msix_num);
int vmnga_register_local_msix(u32 dev_id, u32 msix_index, irq_handler_t handler, void *data, const char *name);
int vmnga_unregister_local_msix(u32 dev_id, u32 msix_index, void *data);

/* addr info : alloc bar4 to external modules */
enum vmng_get_addr_type {
    VMNG_GET_ADDR_TYPE_TSDRV = 0,
    VMNG_GET_ADDR_TYPE_MAX
};

/* note host returan virtual addr, agent return physical addr */
int vmngh_get_virtual_addr_info(u32 dev_id, u32 fid, enum vmng_get_addr_type type, u64 *addr, u64 *size);
int vmnga_get_physicl_addr_info(u32 dev_id, enum vmng_get_addr_type type, phys_addr_t *addr, u64 *size);

#ifndef AOS_LLVM_BUILD
#define DMA_MAP_ERROR (~(dma_addr_t)0)
/* vm dma_addr change to host pa */
dma_addr_t vmngh_dma_map_guest_page(u32 dev_id, u32 fid, unsigned long addr, unsigned long size,
    struct sg_table **dma_sgt);
void vmngh_dma_unmap_guest_page(u32 dev_id, u32 fid, struct sg_table *dma_sgt);
#endif
bool vmngh_dma_pool_active(u32 dev_id, u32 fid);
int vmngh_dma_map_guest_page_batch(u32 dev_id, u32 fid, unsigned long *gfn, unsigned long *dma_addr,
    unsigned long count);
void vmngh_dma_unmap_guest_page_batch(u32 dev_id, u32 fid, unsigned long *gfn, unsigned long *dma_addr,
    unsigned long count);
void *vmngh_get_vdavinci_by_id(u32 dev_id, u32 fid);
void *vmngh_dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp);
void vmngh_dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle);
// inject msix irq to vm
int vmngh_hypervisor_inject_msix(unsigned int dev_id, unsigned int irq_vector);
int vmngh_check_vdev_phy_address(unsigned int dev_id, u64 phy_address, u64 length);
int vmng_check_vdev_iova_address(unsigned int dev_id, dma_addr_t iova_addr, size_t size);
/* @Function: vmngh_ctrl_get_vm_id
 * @Description: get vm id
 * @Returun: 0~31: normal -1: error
 */
#define VMNGH_VM_ID_DEFAULT (-1)
int vmngh_ctrl_get_vm_id(u32 dev_id, u32 fid);
int vmngh_ctrl_get_devid_fid(u32 vm_id, u32 vm_devid, u32 *dev_id, u32 *fid);
int vmngh_enable_sriov(u32 dev_id);
int vmngh_disable_sriov(u32 dev_id);

/* dma code */
struct vmng_para {
    u32 devid;
    u32 fid;
    void *para1;
    void *para2;
    void *para3;
    void *para4;
};

enum vmngh_dev_info_type {
    VMNGH_DEV_CORE_NUM = 0,
    VMNGH_DEV_DDR_SIZE,
    VMNGH_DEV_HBM_SIZE,
    VMNGH_DEV_TYPE_MAX
};

void vmngh_set_dev_info(u32 dev_id, enum vmngh_dev_info_type type, u64 val);
void vmngh_set_total_core_num(u32 dev_id, u32 total_core_num);

struct vmnga_pci_dev_info {
    u8 bus_no;
    u8 device_no;
    u8 function_no;
};

struct vmnga_pcie_id_info {
    unsigned int venderid;
    unsigned int subvenderid;
    unsigned int deviceid;
    unsigned int subdeviceid;
    unsigned int bus;
    unsigned int device;
    unsigned int fn;
};
int vmnga_get_pci_dev_info(u32 dev_id, struct vmnga_pci_dev_info *dev_info);
int vmnga_get_pcie_id_info(u32 dev_id, struct vmnga_pcie_id_info *dev_info);

enum vmngd_client_type {
    VMNGD_CLIENT_TYPE_DEVMNG = 0,
    VMNGD_CLIENT_TYPE_TSDRV,
    VMNGD_CLIENT_TYPE_ESCHED,
    VMNGD_CLIENT_TYPE_DEVMM,
    VMNGD_CLIENT_TYPE_DVPP,
    VMNGD_CLIENT_TYPE_TSD,
    VMNGD_CLIENT_TYPE_QOS,
    VMNGD_CLIENT_TYPE_PROFILING,
    VMNGD_CLIENT_TYPE_HDC,
    VMNGD_CLIENT_TYPE_QUEUE,
    VMNGD_CLIENT_TYPE_VRESOURCE_MGR,
    VMNGD_CLIENT_TYPE_MAX
};

static const char *vmngd_client_name[VMNGD_CLIENT_TYPE_MAX] = {
    [VMNGD_CLIENT_TYPE_DEVMNG] = "devmng",
    [VMNGD_CLIENT_TYPE_TSDRV] = "tsdrv",
    [VMNGD_CLIENT_TYPE_ESCHED] = "esched",
    [VMNGD_CLIENT_TYPE_DEVMM] = "devmm",
    [VMNGD_CLIENT_TYPE_DVPP] = "dvpp",
    [VMNGD_CLIENT_TYPE_TSD] = "tsd",
    [VMNGD_CLIENT_TYPE_QOS] = "qos",
    [VMNGD_CLIENT_TYPE_PROFILING] = "profiling",
    [VMNGD_CLIENT_TYPE_HDC] = "hdc",
    [VMNGD_CLIENT_TYPE_QUEUE] = "queue",
    [VMNGD_CLIENT_TYPE_VRESOURCE_MGR] = "resource_mgr"
};

static inline const char *get_client_name(enum vmngd_client_type type)
{
    if (type >= VMNGD_CLIENT_TYPE_MAX) {
        return "Invalid type";
    }
    return vmngd_client_name[type];
}

struct vmngd_client_instance {
    enum vmngd_client_type type;
    u32 flag;
    struct mutex mutex;
    struct vmng_vdev_ctrl vdev_ctrl;
};

struct vmng_mdev_iova_info {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int action;    // online or offline
    dma_addr_t iova_base;   // vf iova of pm
    size_t size;
};

struct vmngd_client {
    enum vmngd_client_type type;
    int (*init_instance)(struct vmngd_client_instance *instance);
    int (*uninit_instance)(struct vmngd_client_instance *instance);
    int (*update_vfg)(struct vmng_soc_resource_enquire *vf_info);
    int (*reset_instance)(struct vmngd_client_instance *instance);
    int (*sriov_instance)(struct vmng_sriov_info *sriov_info);
};

int vmngd_register_client(struct vmngd_client *client);
int vmngd_unregister_client(struct vmngd_client *client);
int vmngd_get_dtype(u32 dev_id, u32 vfid, u32 *dtype);
int vmngd_register_vmng_client(void);
int vmngd_get_pfvf_id_by_devid(u32 dev_id, u32 *pf_id, u32 *vf_id);
int vmngd_get_devid_by_pfvf_id(u32 pf_id, u32 vf_id, u32 *dev_id);
int vmngd_get_pfvf_type_by_devid(u32 dev_id);
int vmngh_create_container_vdev(u32 dev_id, u32 dtype, u32 *vfid, struct vmng_vf_res_info *vf_resource);
int vmngh_destory_container_vdev(u32 dev_id, u32 vfid);
int vmng_bandwidth_limit_check(struct vmng_bandwidth_check_info *info);
int vmngd_get_device_vf_max(u32 dev_id, u32 *vf_max_num);
int vmngd_get_device_vf_list(u32 dev_id, u32 *vf_list, u32 list_len, u32 *vf_num);
int vmngh_enquire_soc_resource(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info);
int vmngd_enquire_soc_resource(u32 dev_id, u32 vfid, struct vmng_soc_resource_enquire *info);
int vmngd_enquire_vfg_resource(u32 dev_id, u32 vfid, struct vmng_soc_res_info *vfg_info);
int vmngh_refresh_vdev_resource(u32 dev_id, u32 vfid, struct vmng_soc_resource_refresh *info);
int vmngh_sriov_reset_vdev(u32 dev_id, u32 vfid);
int vmngd_get_device_vf_core_info(u32 dev_id, u32 vf_id,
    u32 *total_core, u32 *core_count, u64 *mem_size);
#if (defined CFG_FEATURE_VFIO) && (defined CFG_FEATURE_RC_MODE)
int vmng_create_container_vdev(u32 dev_id, u32 dtype, u32 *vfid, struct vmng_vf_res_info *vf_resource);
int vmng_destory_container_vdev(u32 dev_id, u32 vfid);
#endif
#endif

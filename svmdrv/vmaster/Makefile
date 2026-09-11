ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifeq ($(ENABLE_ASAN),true)
    EXTRA_CFLAGS += -DCFG_ENABLE_ASAN
endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif
ccflags-y += -Wall -Werror

ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y)
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster/comm
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster/comm/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster/common
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster/common/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD

			ifeq ($(TARGET_CHIP_ID), hi1980b)
                EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
			endif
		else ifeq ($(TARGET_PRODUCT),mini)
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
				EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
			endif
		endif
	else
		ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
                ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
	endif

	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_VM
	EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
	EXTRA_CFLAGS += -DDRV_HOST
	drv_vdevmm-objs := devmm_vm_adapt.o devmm_vm_vpc.o svm_vm_version_adapt.o
	drv_vdevmm-objs += common/devmm_dev.o common/devmm_common.o common/svm_proc_mng.o common/svm_heap_mng.o common/svm_mmu_notifier.o common/svm_page_cnt_stats.o common/svm_dynamic_addr.o
	drv_vdevmm-objs += common/svm_pci_dev_tbl.o common/svm_module_ops.o common/svm_mem_mng.o common/svm_res_idr.o common/svm_version_adapt.o common/svm_proc_fs.o
    drv_vdevmm-objs += common/svm_rbtree.o common/svm_srcu_work.o common/svm_gfp.o common/svm_dma_map.o common/svm_mem_split.o common/devmm_register_dma.o
    drv_vdevmm-objs += common/svm_proc_gfp.o common/svm_phy_addr_blk_mng.o common/svm_mem_create.o common/svm_mem_share.o common/svm_mem_map.o common/svm_vmma_mng.o common/svm_log.o
	drv_vdevmm-objs += comm/svm_master_mem_mng.o comm/svm_master_mem_create.o comm/svm_master_mem_map.o comm/svm_master_addr_ref_ops.o
	drv_vdevmm-objs += comm/devmm_proc_info.o comm/devmm_channel.o comm/svm_master_memset.o comm/svm_master_vma_ops.o comm/svm_master_convert.o comm/svm_master_memcpy.o comm/svm_master_mem_share.o
	drv_vdevmm-objs += comm/svm_master_advise.o comm/svm_master_get_host_info.o comm/svm_master_cgroup.o comm/svm_master_proc_mng.o comm/svm_master_proc_fs.o comm/svm_master_mem_stats.o comm/svm_master_feature_proc_fs.o
    drv_vdevmm-objs += comm/svm_dev_res_mng.o comm/svm_task_dev_res_mng.o comm/svm_master_dev_capability.o
        drv_vdevmm-objs += comm/svm_recycle_thread.o comm/svm_ref_server_occupier.o
        obj-m := drv_vdevmm.o
else
	ifeq ($(DAVINCI_HIAI_DKMS),y)

	else
                ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
                ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
		EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/comm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/comm/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/vmaster
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/vmaster/master_comm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/vmaster/svm_common
		EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
		ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
        ifeq ($(PRODUCT),ascend910B)
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2               #temporary
        endif
		ifneq ($(filter $(PRODUCT),  ascend910 ascend910B),)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
		else ifneq ($(filter $(PRODUCT),  ascend310p ascend310B ascend310Besl ascend310Bemu),)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
		endif
	endif

	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_VM
	EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
	EXTRA_CFLAGS += -DDRV_HOST
	drv_vdevmm-objs := svm_common/devmm_dev.o svm_common/devmm_common.o svm_common/svm_proc_mng.o svm_common/svm_heap_mng.o svm_common/svm_mmu_notifier.o svm_common/svm_mem_mng.o svm_common/svm_dynamic_addr.o
	drv_vdevmm-objs += svm_common/svm_page_cnt_stats.o svm_common/svm_pci_dev_tbl.o svm_common/svm_module_ops.o svm_common/svm_res_idr.o svm_common/svm_version_adapt.o svm_common/svm_proc_fs.o
    drv_vdevmm-objs += svm_common/svm_rbtree.o svm_common/svm_srcu_work.o svm_common/svm_gfp.o svm_common/svm_dma_map.o svm_common/svm_mem_split.o svm_common/devmm_register_dma.o
    drv_vdevmm-objs += svm_common/svm_proc_gfp.o svm_common/svm_phy_addr_blk_mng.o svm_common/svm_mem_create.o svm_common/svm_mem_share.o svm_common/svm_mem_map.o svm_common/svm_vmma_mng.o svm_common/svm_log.o
	drv_vdevmm-objs += master_comm/svm_master_mem_mng.o master_comm/svm_master_mem_create.o master_comm/svm_master_mem_map.o master_comm/svm_master_addr_ref_ops.o
	drv_vdevmm-objs += master_comm/devmm_proc_info.o master_comm/devmm_channel.o master_comm/svm_master_memset.o master_comm/svm_master_vma_ops.o master_comm/svm_master_convert.o master_comm/svm_master_memcpy.o master_comm/svm_master_mem_share.o
	drv_vdevmm-objs += master_comm/svm_master_advise.o master_comm/svm_master_get_host_info.o master_comm/svm_master_cgroup.o master_comm/svm_master_proc_mng.o master_comm/svm_master_proc_fs.o master_comm/svm_master_mem_stats.o master_comm/svm_master_feature_proc_fs.o
    drv_vdevmm-objs += master_comm/svm_dev_res_mng.o master_comm/svm_task_dev_res_mng.o master_comm/svm_master_dev_capability.o master_comm/svm_master_phy_allocator.o
        drv_vdevmm-objs += master_comm/svm_recycle_thread.o master_comm/svm_ref_server_occupier.o
        drv_vdevmm-objs += devmm_vm_adapt.o devmm_vm_vpc.o svm_vm_version_adapt.o
	obj-m := drv_vdevmm.o
endif

ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

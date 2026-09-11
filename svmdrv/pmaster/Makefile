ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifeq ($(ENABLE_ASAN),true)
    EXTRA_CFLAGS += -DCFG_ENABLE_ASAN
endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wno-missing-prototypes -Wno-missing-declarations

ifeq ($(TOP_DIR),)

	ifeq ($(DAVINCI_HIAI_DKMS),y)
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/common
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/master/
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/comm
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/comm/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/common
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/common/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/msg
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/pm_adapt

		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/memory

		EXTRA_CFLAGS += -DDRV_HOST
		EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
		# ascend910
		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			ifeq ($(TARGET_CHIP_ID), hi1980)
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster
				drv_devmm_host-objs += pm_adapt/devmm_pm_adapt.o pm_adapt/devmm_pm_vpc.o pm_adapt/svm_pm_version_adapt.o
			endif
			ifeq ($(TARGET_CHIP_ID), hi1980b)
                EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
                drv_devmm_host-objs += svm_shmem_node_pod.o
			endif
		# ascend310 + ascend310p + ascend310b
		else ifeq ($(TARGET_PRODUCT),mini)
			# ascend310p
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
				EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster
				drv_devmm_host-objs += pm_adapt/devmm_pm_adapt.o pm_adapt/devmm_pm_vpc.o pm_adapt/svm_pm_version_adapt.o
			endif
		endif

		drv_devmm_host-objs += msg/svm_msg_client.o msg/svm_host_msg_client.o msg/svm_device_msg_client.o
		drv_devmm_host-objs += comm/devmm_proc_info.o comm/devmm_channel.o comm/svm_master_memset.o comm/svm_master_addr_map.o comm/svm_master_vma_ops.o comm/svm_master_convert.o comm/svm_master_memcpy.o comm/svm_master_mem_repair.o comm/svm_master_phy_allocator.o
		drv_devmm_host-objs += comm/svm_master_advise.o comm/svm_master_get_host_info.o comm/svm_master_mem_mng.o comm/svm_master_cgroup.o comm/svm_master_proc_mng.o comm/svm_master_proc_fs.o comm/svm_master_mem_stats.o comm/svm_master_dev_capability.o
        drv_devmm_host-objs += comm/svm_dev_res_mng.o comm/svm_task_dev_res_mng.o comm/svm_master_dma_desc_mng.o comm/svm_master_mem_create.o comm/svm_master_mem_map.o comm/svm_master_addr_ref_ops.o comm/svm_master_mem_share.o comm/svm_master_feature_proc_fs.o
                drv_devmm_host-objs += comm/svm_recycle_thread.o comm/svm_ref_server_occupier.o
                drv_devmm_host-objs += svm_master_query.o svm_shmem_interprocess.o devmm_proc_mem_copy.o svm_master_remote_map.o svm_master_pm_proc_mng.o svm_dma_prepare_pool.o
		drv_devmm_host-objs += common/svm_dma.o common/devmm_dev.o common/devmm_common.o common/devmm_page_cache.o common/svm_proc_mng.o common/svm_mem_query.o common/svm_heap_mng.o common/svm_mmu_notifier.o common/svm_dynamic_addr.o
		drv_devmm_host-objs += common/svm_page_cnt_stats.o common/svm_pci_dev_tbl.o common/svm_module_ops.o common/svm_mem_mng.o common/svm_res_idr.o common/svm_version_adapt.o common/svm_proc_fs.o common/svm_log.o
        drv_devmm_host-objs += common/svm_rbtree.o common/svm_srcu_work.o common/svm_gfp.o common/svm_dma_map.o common/svm_mem_split.o common/devmm_register_dma.o
        drv_devmm_host-objs += common/svm_proc_gfp.o common/svm_phy_addr_blk_mng.o common/svm_mem_create.o common/svm_mem_share.o common/svm_mem_map.o common/svm_vmma_mng.o
		drv_devmm_host-objs += svm_master_register_ops.o svm_hot_reset.o svm_master_process_status_mng.o svm_pmaster_mem_map.o svm_shmem_node.o svm_shmem_procfs.o
	else
		ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
		ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
	endif

	obj-m := drv_devmm_host.o
else
	ifeq ($(DAVINCI_HIAI_DKMS),y)

	else
		ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
		ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
                EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
		EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/comm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/comm/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/svm_common
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/master_comm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/msg
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/pm_adapt
	
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
		EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/memory

		EXTRA_CFLAGS += -DDRV_HOST
		EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
		ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
		ifneq ($(filter $(PRODUCT),  ascend910 ascend910B),)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			ifeq ($(PRODUCT),ascend910)
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/vmaster

				drv_devmm_host-objs += pm_adapt/devmm_pm_adapt.o pm_adapt/devmm_pm_vpc.o pm_adapt/svm_pm_version_adapt.o
			endif
			ifeq ($(PRODUCT),ascend910B)
				ifeq ($(ADAPT_KP_OS_FOR_EMU_TEST),ON)
					EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL_FPGA
					EXTRA_CFLAGS += -DADAPT_KP_OS_FOR_EMU_TEST
				endif
				drv_devmm_host-objs += svm_shmem_node_pod.o
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2               #temporary
			endif
		else ifneq ($(filter $(PRODUCT),  ascend310p ascend310B ascend310Besl ascend310Bemu),)
			ifeq  ($(PRODUCT),ascend310p)
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/vmaster

				drv_devmm_host-objs += pm_adapt/devmm_pm_adapt.o pm_adapt/devmm_pm_vpc.o pm_adapt/svm_pm_version_adapt.o
			endif
		endif
		drv_devmm_host-objs += msg/svm_msg_client.o msg/svm_host_msg_client.o msg/svm_device_msg_client.o
		drv_devmm_host-objs += master_comm/svm_master_mem_mng.o master_comm/svm_master_proc_fs.o master_comm/svm_master_mem_stats.o master_comm/svm_master_feature_proc_fs.o
		drv_devmm_host-objs += master_comm/devmm_proc_info.o master_comm/devmm_channel.o master_comm/svm_master_addr_map.o master_comm/svm_master_memset.o master_comm/svm_master_vma_ops.o master_comm/svm_master_mem_repair.o master_comm/svm_master_dev_capability.o
		drv_devmm_host-objs += master_comm/svm_master_convert.o master_comm/svm_master_advise.o master_comm/svm_master_get_host_info.o master_comm/svm_master_cgroup.o master_comm/svm_master_proc_mng.o master_comm/svm_master_memcpy.o master_comm/svm_master_phy_allocator.o
        drv_devmm_host-objs += master_comm/svm_dev_res_mng.o master_comm/svm_task_dev_res_mng.o master_comm/svm_master_dma_desc_mng.o master_comm/svm_master_mem_create.o master_comm/svm_master_mem_map.o master_comm/svm_master_addr_ref_ops.o master_comm/svm_master_mem_share.o
                drv_devmm_host-objs += master_comm/svm_recycle_thread.o master_comm/svm_ref_server_occupier.o
                drv_devmm_host-objs += devmm_proc_mem_copy.o svm_master_remote_map.o svm_shmem_interprocess.o svm_master_pm_proc_mng.o svm_dma_prepare_pool.o
		drv_devmm_host-objs += svm_common/svm_dma.o svm_common/svm_mem_query.o svm_common/devmm_page_cache.o svm_common/svm_proc_mng.o svm_common/devmm_dev.o svm_common/devmm_common.o svm_common/svm_heap_mng.o svm_common/svm_mmu_notifier.o svm_common/svm_dynamic_addr.o
		drv_devmm_host-objs += svm_common/svm_pci_dev_tbl.o svm_common/svm_module_ops.o svm_common/svm_mem_mng.o svm_common/svm_res_idr.o svm_common/svm_version_adapt.o svm_common/svm_proc_fs.o svm_common/devmm_register_dma.o
		drv_devmm_host-objs += svm_common/svm_rbtree.o svm_common/svm_srcu_work.o svm_common/svm_page_cnt_stats.o svm_common/svm_gfp.o  svm_common/svm_dma_map.o svm_common/svm_mem_split.o svm_common/svm_log.o
		drv_devmm_host-objs += svm_common/svm_proc_gfp.o svm_common/svm_phy_addr_blk_mng.o svm_common/svm_mem_create.o svm_common/svm_mem_share.o svm_common/svm_mem_map.o svm_common/svm_vmma_mng.o
		drv_devmm_host-objs += svm_master_query.o
		drv_devmm_host-objs += svm_master_register_ops.o svm_hot_reset.o svm_master_process_status_mng.o svm_pmaster_mem_map.o svm_shmem_node.o svm_shmem_procfs.o
	endif

	obj-m := drv_devmm_host.o
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

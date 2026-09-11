ifeq ($(TARGET_BUILD_TYPE),debug)
	ccflags-y += -DCFG_BUILD_DEBUG
endif

ifeq ($(ENABLE_ASAN),true)
    ccflags-y += -DCFG_ENABLE_ASAN
endif

ifneq ($(NOT_SUPPORT_SP), y)
	ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wno-missing-prototypes -Wno-missing-declarations

ifeq ($(TOP_DIR),)

	ifeq ($(DAVINCI_HIAI_DKMS),y)
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
		ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
		ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/common
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/master/
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/comm
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/comm/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/common
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/common/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/msg
		ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/pmaster/pm_adapt

		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/memory

		ccflags-y += -DDRV_HOST
		ccflags-y += -DCFG_FEATURE_SHARE_LOG
        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
		# ascend910
		ifeq ($(TARGET_PRODUCT),cloud)
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
			ifeq ($(TARGET_CHIP_ID), hi1980)
				ccflags-y += -DCFG_FEATURE_VFIO
				ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster
				drv_devmm_host-objs += pm_adapt/devmm_pm_adapt.o pm_adapt/devmm_pm_vpc.o pm_adapt/svm_pm_version_adapt.o
			endif
			ifeq ($(TARGET_CHIP_ID), hi1980b)
                ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_V2
                drv_devmm_host-objs += svm_shmem_node_pod.o
			endif
		# ascend310 + ascend310p + ascend310b
		else ifeq ($(TARGET_PRODUCT),mini)
			# ascend310p
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_FEATURE_VFIO
				ccflags-y += -DCFG_FEATURE_HOST_LOG
				ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
				ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/vmaster
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
                ccflags-y += -I$(TOP_DIR)/inc/driver
		ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
		ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/comm
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/comm/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/svm_common
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/master_comm
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/msg
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/pmaster/pm_adapt
	
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
		ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/memory

		ccflags-y += -DDRV_HOST
		ccflags-y += -DCFG_FEATURE_SHARE_LOG
		ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
		ifneq ($(filter $(PRODUCT),  ascend910 ascend910B),)
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
			ifeq ($(PRODUCT),ascend910)
				ccflags-y += -DCFG_FEATURE_VFIO
				ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/vmaster

				drv_devmm_host-objs += pm_adapt/devmm_pm_adapt.o pm_adapt/devmm_pm_vpc.o pm_adapt/svm_pm_version_adapt.o
			endif
			ifeq ($(PRODUCT),ascend910B)
				ifeq ($(ADAPT_KP_OS_FOR_EMU_TEST),ON)
					ccflags-y += -DCFG_SOC_PLATFORM_ESL_FPGA
					ccflags-y += -DADAPT_KP_OS_FOR_EMU_TEST
				endif
				drv_devmm_host-objs += svm_shmem_node_pod.o
				ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_V2               #temporary
			endif
		else ifneq ($(filter $(PRODUCT),  ascend310p ascend310B ascend310Besl ascend310Bemu),)
			ifeq  ($(PRODUCT),ascend310p)
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_FEATURE_VFIO
				ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/master/vmaster

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

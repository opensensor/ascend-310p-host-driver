ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
	endif
else #for CMake
	include $(FEATURE_MK_PATH)
	ccflags-y += $(CONFIG_DEFINES)

	ifeq ($(PRODUCT_SIDE), host)
	else
		$(MODULE_NAME)-y += mem/dms_mem_info.o
		$(MODULE_NAME)-y += mem/dms_mem_common.o
		$(MODULE_NAME)-y += mem/dms_ddr_info.o

		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/core
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/mem
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc
		ccflags-y += -DCFG_FEATURE_MEM

		ifneq ($(filter $(PRODUCT), ascend910B),)
			$(MODULE_NAME)-y += mem/dms_hbm_info.o
			ccflags-y += -DCFG_FEATURE_SRIOV
			ccflags-y += -DCFG_FEATURE_HBM
			ccflags-y += -DSUPPORT_VDEV_MEM
			ccflags-y += -DCFG_MEMORY_NODE_CONFIG_3
			ccflags-y += -DCFG_FEATURE_GET_MEM_FROM_VIRMNG
			ccflags-y += -DCFG_FEATURE_GET_HPAGE_2M_INFO
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
		endif
		ifneq ($(filter $(PRODUCT), ascend910),)
			$(MODULE_NAME)-y += mem/dms_hbm_info.o
			ccflags-y += -DCFG_FEATURE_HBM
			ccflags-y += -DSUPPORT_VDEV_MEM
			ccflags-y += -DCFG_MEMORY_NODE_CONFIG_1
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
		endif
		ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc),)
			ccflags-y += -DCFG_MEMORY_NODE_CONFIG_2
			ccflags-y += -DCFG_FEATURE_ADD_SYSTEM_MEMORY
		endif
		ifneq ($(filter $(PRODUCT), ascend310p),)
			ccflags-y += -DSUPPORT_VDEV_MEM
			ccflags-y += -DCFG_MEMORY_NODE_CONFIG_2
			ccflags-y += -DCFG_FEATURE_ADD_SYSTEM_MEMORY
		endif
		ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
			ccflags-y += -DCFG_MEMORY_NODE_CONFIG_5
			ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
		endif
		ifneq ($(filter $(PRODUCT), helper310p),)
			ccflags-y += -DCFG_MEMORY_NODE_CONFIG_4
			ccflags-y += -DCFG_SOC_PLATFORM_HELPER
			ccflags-y += -DCFG_FEATURE_ADD_SYSTEM_MEMORY
		endif
		ifneq ($(filter $(PRODUCT), as31xm1),)
			ccflags-y += -DCFG_MEMORY_NODE_CONFIG_2
		endif
	endif
endif

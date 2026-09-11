ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
	endif
else #for CMake
	include $(FEATURE_MK_PATH)
	EXTRA_CFLAGS += $(CONFIG_DEFINES)

	ifeq ($(PRODUCT_SIDE), host)
	else
		$(MODULE_NAME)-y += mem/dms_mem_info.o
		$(MODULE_NAME)-y += mem/dms_mem_common.o
		$(MODULE_NAME)-y += mem/dms_ddr_info.o

		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/core
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/mem
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc
		EXTRA_CFLAGS += -DCFG_FEATURE_MEM

		ifneq ($(filter $(PRODUCT), ascend910B),)
			$(MODULE_NAME)-y += mem/dms_hbm_info.o
			EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
			EXTRA_CFLAGS += -DCFG_FEATURE_HBM
			EXTRA_CFLAGS += -DSUPPORT_VDEV_MEM
			EXTRA_CFLAGS += -DCFG_MEMORY_NODE_CONFIG_3
			EXTRA_CFLAGS += -DCFG_FEATURE_GET_MEM_FROM_VIRMNG
			EXTRA_CFLAGS += -DCFG_FEATURE_GET_HPAGE_2M_INFO
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
		endif
		ifneq ($(filter $(PRODUCT), ascend910),)
			$(MODULE_NAME)-y += mem/dms_hbm_info.o
			EXTRA_CFLAGS += -DCFG_FEATURE_HBM
			EXTRA_CFLAGS += -DSUPPORT_VDEV_MEM
			EXTRA_CFLAGS += -DCFG_MEMORY_NODE_CONFIG_1
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
		endif
		ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc),)
			EXTRA_CFLAGS += -DCFG_MEMORY_NODE_CONFIG_2
			EXTRA_CFLAGS += -DCFG_FEATURE_ADD_SYSTEM_MEMORY
		endif
		ifneq ($(filter $(PRODUCT), ascend310p),)
			EXTRA_CFLAGS += -DSUPPORT_VDEV_MEM
			EXTRA_CFLAGS += -DCFG_MEMORY_NODE_CONFIG_2
			EXTRA_CFLAGS += -DCFG_FEATURE_ADD_SYSTEM_MEMORY
		endif
		ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
			EXTRA_CFLAGS += -DCFG_MEMORY_NODE_CONFIG_5
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
		endif
		ifneq ($(filter $(PRODUCT), helper310p),)
			EXTRA_CFLAGS += -DCFG_MEMORY_NODE_CONFIG_4
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_HELPER
			EXTRA_CFLAGS += -DCFG_FEATURE_ADD_SYSTEM_MEMORY
		endif
		ifneq ($(filter $(PRODUCT), as31xm1),)
			EXTRA_CFLAGS += -DCFG_MEMORY_NODE_CONFIG_2
		endif
	endif
endif

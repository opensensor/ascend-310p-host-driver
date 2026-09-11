ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		drv_devmng_host-y += product/dms_product.o
		drv_devmng_host-y += product/dms_product_host.o
		EXTRA_CFLAGS += -DCFG_FEATURE_DMS_PRODUCT_HOST
		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
		endif
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/product
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
		drv_devmng_host-y += product/dms_product.o
		drv_devmng_host-y += product/dms_product_host.o
		EXTRA_CFLAGS += -DCFG_FEATURE_DMS_PRODUCT_HOST
	else
		drv_devmng-y += product/dms_product.o
		ifneq ($(filter $(PRODUCT), ascend910B ascend920esl ascend920emu),)
			EXTRA_CFLAGS += -DCFG_FEATURE_PCIE_HCCS_BANDWIDTH
			EXTRA_CFLAGS += -DCFG_FEATURE_HBM_MANUFACTURER_ID
			EXTRA_CFLAGS += -DCFG_FEATURE_HCCS_BANDWIDTH
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_pcie_device/pcie_slave/ctrls
			EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		endif
		ifneq ($(filter $(PRODUCT), ascend310p),)
			EXTRA_CFLAGS += -DCFG_FEATURE_HCCS_LINK_ERROR_INFO
			EXTRA_CFLAGS += -DCFG_FEATURE_PCIE_LINK_ERROR_INFO
		endif
	endif
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/product
endif

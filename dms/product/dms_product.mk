ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		drv_devmng_host-y += product/dms_product.o
		drv_devmng_host-y += product/dms_product_host.o
		ccflags-y += -DCFG_FEATURE_DMS_PRODUCT_HOST
		ifeq ($(TARGET_PRODUCT),cloud)
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
		endif
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/product
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
		drv_devmng_host-y += product/dms_product.o
		drv_devmng_host-y += product/dms_product_host.o
		ccflags-y += -DCFG_FEATURE_DMS_PRODUCT_HOST
	else
		drv_devmng-y += product/dms_product.o
		ifneq ($(filter $(PRODUCT), ascend910B ascend920esl ascend920emu),)
			ccflags-y += -DCFG_FEATURE_PCIE_HCCS_BANDWIDTH
			ccflags-y += -DCFG_FEATURE_HBM_MANUFACTURER_ID
			ccflags-y += -DCFG_FEATURE_HCCS_BANDWIDTH
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_pcie_device/pcie_slave/ctrls
			ccflags-y += -I$(TOP_DIR)/inc/driver
		endif
		ifneq ($(filter $(PRODUCT), ascend310p),)
			ccflags-y += -DCFG_FEATURE_HCCS_LINK_ERROR_INFO
			ccflags-y += -DCFG_FEATURE_PCIE_LINK_ERROR_INFO
		endif
	endif
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/product
endif

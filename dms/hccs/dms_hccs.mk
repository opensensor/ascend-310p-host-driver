ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ifeq ($(TARGET_CHIP_ID), hi1980b)
			ccflags-y += -I$(HIAI_DKMS_DIR)/dms/hccs
			drv_devmng_host-y += hccs/dms_hccs_init.o
			drv_devmng_host-y += hccs/dms_hccs_credit.o
		endif
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
		ifneq ($(filter $(PRODUCT), ascend910B),)
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/hccs
			drv_devmng_host-y += hccs/dms_hccs_init.o
			drv_devmng_host-y += hccs/dms_hccs_credit.o
		endif
	else
		ifneq ($(filter $(PRODUCT), ascend910B ascend310p),)
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/hccs
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
			$(MODULE_NAME)-y += hccs/dms_hccs_feature.o
			$(MODULE_NAME)-y += hccs/dms_hccs_init.o

			ifneq ($(filter $(PRODUCT), ascend910B),)
				ccflags-y += -DCFG_FEATURE_GET_PCS_BITMAP_BY_BOARD_TYPE
				ccflags-y += -DCFG_FEATURE_HCCS_GET_STATUS
				ccflags-y += -DCFG_FEATURE_HCCS_GET_LANE_INFO

				$(MODULE_NAME)-y += hccs/dms_hccs_credit.o
			endif

			ifneq ($(filter $(PRODUCT), ascend310p),)
				ccflags-y += -DCFG_FEATURE_HCCS_GET_STATISTIC_BY_CHANNEL
			endif
		endif
	endif
endif


ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		ifneq ($(TARGET_CHIP_ID), hi1910)
			drv_devmng_host-y += urd_forward/dms_urd_forward.o

			ccflags-y += -I$(HIAI_DKMS_DIR)/dms/urd_forward
			ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
			ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
			ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/uda
			ccflags-y += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
			ccflags-y += -I$(HIAI_DKMS_DIR)/dms/power
			ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
			ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl

			ccflags-y += -DCFG_FEATURE_SDK_EX_VERSION

			ifeq ($(TARGET_CHIP_ID), hi1910b)
				ccflags-y += -DCFG_FEATURE_QUERY_FREQ_INFO
				ccflags-y += -DCFG_FEATURE_QUERY_QOS_CFG_INFO
			endif

			ifeq ($(TARGET_CHIP_ID), hi1980b)
				ccflags-y += -DCFG_FEATURE_QUERY_FREQ_INFO
				ccflags-y += -DCFG_FEATURE_QUERY_QOS_CFG_INFO
				ccflags-y += -DCFG_FEATURE_GET_QOS_MASTER_CFG
				ccflags-y += -DCFG_FEATURE_QUERY_VA_INFO
			endif

			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				ccflags-y += -DCFG_FEATURE_QUERY_FREQ_INFO
			endif
		endif
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
		ifeq ($(filter $(PRODUCT), ascend310),)
			drv_devmng_host-y += urd_forward/dms_urd_forward.o

			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/urd_forward
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/power
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/uda
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/drv_devmng/drv_devmng_host/ascend910

			ccflags-y += -DCFG_FEATURE_SDK_EX_VERSION

			ifneq ($(filter $(PRODUCT), ascend310p ascend310B ascend910B),)
				ccflags-y += -DCFG_FEATURE_QUERY_FREQ_INFO
			endif
			ifneq ($(filter $(PRODUCT), ascend310B ascend910B),)
				ccflags-y += -DCFG_FEATURE_QUERY_QOS_CFG_INFO
			endif
			ifneq ($(filter $(PRODUCT), ascend910B),)
				ccflags-y += -DCFG_FEATURE_QUERY_VA_INFO
				ccflags-y += -DCFG_FEATURE_GET_QOS_MASTER_CFG
			endif
		endif
	endif
endif
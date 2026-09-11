ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		ifneq ($(TARGET_CHIP_ID), hi1910)
			drv_devmng_host-y += urd_forward/dms_urd_forward.o

			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/urd_forward
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/uda
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/power
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl

			EXTRA_CFLAGS += -DCFG_FEATURE_SDK_EX_VERSION

			ifeq ($(TARGET_CHIP_ID), hi1910b)
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_FREQ_INFO
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_QOS_CFG_INFO
			endif

			ifeq ($(TARGET_CHIP_ID), hi1980b)
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_FREQ_INFO
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_QOS_CFG_INFO
				EXTRA_CFLAGS += -DCFG_FEATURE_GET_QOS_MASTER_CFG
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_VA_INFO
			endif

			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_FREQ_INFO
			endif
		endif
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
		ifeq ($(filter $(PRODUCT), ascend310),)
			drv_devmng_host-y += urd_forward/dms_urd_forward.o

			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/urd_forward
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/power
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/uda
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/drv_devmng/drv_devmng_host/ascend910

			EXTRA_CFLAGS += -DCFG_FEATURE_SDK_EX_VERSION

			ifneq ($(filter $(PRODUCT), ascend310p ascend310B ascend910B),)
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_FREQ_INFO
			endif
			ifneq ($(filter $(PRODUCT), ascend310B ascend910B),)
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_QOS_CFG_INFO
			endif
			ifneq ($(filter $(PRODUCT), ascend910B),)
				EXTRA_CFLAGS += -DCFG_FEATURE_QUERY_VA_INFO
				EXTRA_CFLAGS += -DCFG_FEATURE_GET_QOS_MASTER_CFG
			endif
		endif
	endif
endif
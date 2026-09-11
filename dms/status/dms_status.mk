ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		drv_devmng_host-y += status/dms_basic_info.o
		drv_devmng_host-y += status/dms_chip_info.o
		drv_devmng_host-y += status/dms_dev_topology.o
		drv_devmng_host-y += status/dms_vdev.o
		ifeq ($(TARGET_CHIP_ID), hi1980b)
			drv_devmng_host-y += status/dms_host_aicpu_info.o
			drv_devmng_host-y += status/dms_host_notify_ready.o
		endif
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			drv_devmng_host-y += status/dms_osc_freq.o
		endif
		ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
			drv_devmng_host-y += status/dms_osc_freq.o
		endif
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/status
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
		drv_devmng_host-y += status/dms_basic_info.o
		drv_devmng_host-y += status/dms_chip_info.o
		drv_devmng_host-y += status/dms_dev_topology.o
		drv_devmng_host-y += status/dms_vdev.o
		ifneq ($(filter $(PRODUCT), ascend910B),)
			drv_devmng_host-y += status/dms_host_aicpu_info.o
			drv_devmng_host-y += status/dms_host_notify_ready.o
		endif

		ifneq ($(filter $(PRODUCT), ascend310p ascend310B ascend910 ascend910B),)
			drv_devmng_host-y += status/dms_osc_freq.o
		endif
	else
		$(MODULE_NAME)-y += status/dms_basic_info.o
		$(MODULE_NAME)-y += status/dms_chip_info.o
		$(MODULE_NAME)-y += status/dms_vdev.o
		$(MODULE_NAME)-y += status/dms_pg_info.o
		$(MODULE_NAME)-y += status/dms_bbox.o
		ifneq ($(filter $(PRODUCT), ascend910B ascend910 ascend310 ascend310p ascend310B),)
			$(MODULE_NAME)-y += status/dms_dev_topology.o
		endif
		ifneq ($(filter $(PRODUCT), helper310p ascend310Brc ascend610 ascend610Lite ascend610Liteesl as31xm1),)
			$(MODULE_NAME)-y += status/dms_osc_freq.o
		endif

		ifneq ($(filter $(PRODUCT), ascend910B),)
			EXTRA_CFLAGS += -DCFG_FEATURE_GET_DEV_UUID
		endif

		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/time/device
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/power
		ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910 ascend910B),)
			EXTRA_CFLAGS += -DCFG_FEATURE_SDK_EX_VERSION
			$(MODULE_NAME)-y += status/dms_sdk_ex_version.o
		endif
	endif
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/status
	EXTRA_CFLAGS += -I${TOP_DIR}/abl/bbox/inc/bbox/device/
endif

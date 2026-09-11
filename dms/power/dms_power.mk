ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ccflags-y += -DCFG_FEATURE_HOTRESET
		drv_devmng_host-y += power/dms_power.o
		drv_devmng_host-y += power/dms_hotreset.o

		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/power
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/core
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/dms/time
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend310
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
		ccflags-y += -DCFG_FEATURE_HOTRESET
		drv_devmng_host-y += power/dms_power.o
		drv_devmng_host-y += power/dms_hotreset.o

		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend310
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend910
	else
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend310
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910

		ifneq ($(filter $(PRODUCT), as31xm1 ascend310Brc ascend310Brcesl ascend610Lite),)
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/ipcdrv
			$(MODULE_NAME)-y += power/dms_pm.o
		endif

		ifneq ($(filter $(PRODUCT), as31xm1 ascend310Brc ascend310Brcesl ascend610Lite),)
			ccflags-y += -DCFG_FEATURE_POWER_STATE
		endif
		ifneq ($(filter $(PRODUCT), ascend610 ascend610Liteesl ascend610Lite bs9sx1a),)
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/mdc/ts_platform/ts_platform_device/ascend910
		else
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
		endif
		ifneq ($(filter $(PRODUCT), as31xm1 ascend610Lite),)
			$(MODULE_NAME)-y += power/dms_pm_register.o
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/core
		endif
	endif
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/power
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/dms/include
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/dms/core
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/dms/time
endif

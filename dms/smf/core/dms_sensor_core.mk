ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ccflags-y += -DCFG_HOST_ENV
		ascend_dms_smf-y += core/dms_sensor_init.o
		ascend_dms_smf-y += core/dms_notifier.o
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/event
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
		ccflags-y += -DCFG_HOST_ENV
		ascend_dms_smf-y += core/dms_sensor_init.o
		ascend_dms_smf-y += core/dms_notifier.o
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend910
	else
		ascend_dms_smf-y += core/dms_sensor_init.o
		ascend_dms_smf-y += core/dms_notifier.o
		ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
			 ascend_dms_smf-y += core/drv_cpu_type.o
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/common
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/mdc/ts_platform/ts_platform_device/ascend610
		else
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
		endif
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
		ccflags-y += -I$(TOP_DIR)/abl/bbox/inc/bbox
	endif
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng
endif

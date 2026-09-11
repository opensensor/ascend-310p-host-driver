ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		drv_devmng_host-y += event/host/dms_event_host.o
		drv_devmng_host-y += event/dms_adapt.o

		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/event
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/event
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
		drv_devmng_host-y += event/host/dms_event_host.o
		drv_devmng_host-y += event/dms_adapt.o
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend910
	else
		ifeq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Liteesl ascend610Lite as31xm1 ascend310Brc helper310p),)
			ccflags-y += -DCFG_SURPPORT_PCIE_HOST_DEVICE_COMM
		endif
		$(MODULE_NAME)-y += event/device/dms_event_device.o
		$(MODULE_NAME)-y += event/dms_adapt.o
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
		ifneq ($(filter $(PRODUCT), ascend610 ascend610Liteesl ascend610Lite bs9sx1a),)
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/mdc/ts_platform/ts_platform_device/ascend910
		else
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
		endif
		ccflags-y += -I$(TOP_DIR)/abl/bbox/inc/bbox
	endif
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/event
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng
endif

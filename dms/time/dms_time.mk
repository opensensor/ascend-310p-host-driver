ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		drv_devmng_host-y += time/host/dms_time_host.o
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/time/host
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/time
		ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		ccflags-y += -DCFG_FEATURE_TIMESYNC
	endif
else #for CMake & ctrl cpu open
	ifneq ($(filter $(PRODUCT), ascend310 ascend310B ascend310Besl ascend310Bemu ascend310p ascend910 ascend910B),)
		ifeq ($(PRODUCT_SIDE), host)
			drv_devmng_host-y += time/host/dms_time_host.o
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/time/host
		else
			$(MODULE_NAME)-y += time/device/dms_time_device.o
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/time/device
		endif
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/time
		ccflags-y += -DCFG_FEATURE_TIMESYNC
	endif
    ifeq ($(PRODUCT_SIDE), device)
        $(MODULE_NAME)-y += time/device/dms_device_time_zone.o
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/common
    endif
endif

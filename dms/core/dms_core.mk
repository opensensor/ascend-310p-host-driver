

ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		drv_devmng_host-y += core/dms_init.o
		drv_devmng_host-y += core/dms_probe.o
		drv_devmng_host-y += core/dms_sysfs.o
		drv_devmng_host-y += core/dms_timer.o
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/core
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/product
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/urd_forward
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
		drv_devmng_host-y += core/dms_init.o
		drv_devmng_host-y += core/dms_probe.o
		drv_devmng_host-y += core/dms_sysfs.o
		drv_devmng_host-y += core/dms_timer.o
	else
		$(MODULE_NAME)-y += core/dms_init.o
		$(MODULE_NAME)-y += core/dms_probe.o
		$(MODULE_NAME)-y += core/dms_sysfs.o
		$(MODULE_NAME)-y += core/dms_timer.o
		ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
			$(MODULE_NAME)-y += core/workqueue_affinity.o
			$(MODULE_NAME)-y += core/drv_cpu_type.o
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/common
		endif
	endif
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/core
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/product
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/urd_forward
endif

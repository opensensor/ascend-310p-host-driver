ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
        ifneq ($(filter $(TARGET_CHIP_ID), hi1980b),)
		    drv_devmng_host-y += custom/dms_custom_host.o
		    drv_devmng_host-y += custom/dms_custom_common.o
		endif
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/custom
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/urd_forward
		ccflags-y += -I$(TOP_DIR)/drivers/cmscbb/include
	endif
else #for CMake & ctrl cpu open
    ifneq ($(filter $(PRODUCT), ascend910B),)
		ifeq ($(PRODUCT_SIDE), host)
			drv_devmng_host-y += custom/dms_custom_host.o
			drv_devmng_host-y += custom/dms_custom_common.o
		else
			$(MODULE_NAME)-y += custom/dms_custom_common.o
			$(MODULE_NAME)-y += custom/dms_custom_device.o
		endif
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/custom
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/urd_forward
		ccflags-y += -I$(TOP_DIR)/drivers/cmscbb/include
	endif	
endif

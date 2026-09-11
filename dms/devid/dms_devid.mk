

ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/devid
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
	else
	endif
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/devid
endif

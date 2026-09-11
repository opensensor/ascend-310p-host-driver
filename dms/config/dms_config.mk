ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/config
	endif
else #for CMake & ctrl cpu open
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/config
endif

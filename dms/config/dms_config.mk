ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/config
	endif
else #for CMake & ctrl cpu open
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/config
endif

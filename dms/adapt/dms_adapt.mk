
ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/adapt
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
	else
	endif

	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/adapt
endif

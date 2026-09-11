ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/status
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
	else
		$(MODULE_NAME)-y += xpc/dms_xpc_common.o
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/status
	endif
endif

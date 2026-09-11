ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
	else
		$(MODULE_NAME)-y += flash/dms_flash.o
	endif
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/flash
endif

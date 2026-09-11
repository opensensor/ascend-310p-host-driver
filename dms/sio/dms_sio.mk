ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS

    endif
else #for CMake & ctrl cpu open
    ifeq ($(PRODUCT_SIDE), host)

    else
        $(MODULE_NAME)-y += sio/dms_sio_info.o
    endif
endif
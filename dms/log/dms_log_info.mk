ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            drv_devmng_host-y += log/dms_log_info.o
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/log
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
            ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
            ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/uda
            ccflags-y += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/power
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
            ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
            ccflags-y += -I$(HIAI_DKMS_DIR)/logdrv/host
        endif
        ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
            ifneq ($(filter $(PRODUCT),  helper310p),)
            else
                drv_devmng_host-y += log/dms_log_info.o
                ccflags-y += -I$(HIAI_DKMS_DIR)/dms/log
                ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
                ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
                ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/uda
                ccflags-y += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
                ccflags-y += -I$(HIAI_DKMS_DIR)/dms/power
                ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
                ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
                ccflags-y += -I$(HIAI_DKMS_DIR)/logdrv/host
            endif
        endif
    endif
else #for CMake
    ifeq ($(PRODUCT_SIDE), host)
        ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            drv_devmng_host-y += log/dms_log_info.o
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/log
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/power
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/uda
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/logdrv/host
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/drv_devmng/drv_devmng_host/ascend910
        endif
    endif
endif
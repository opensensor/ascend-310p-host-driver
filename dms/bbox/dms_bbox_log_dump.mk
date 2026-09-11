ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
        drv_devmng_host-y += bbox/dms_bbox_log_dump.o
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/log
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/uda
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/power
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/logdrv/host
        ifeq ($(TARGET_CHIP_ID), hi1980b)
            EXTRA_CFLAGS += -DCFG_FEATURE_LOG_DUMP_FROM_PCIE
        endif
    endif
else #for CMake
    ifeq ($(PRODUCT_SIDE), host)
        drv_devmng_host-y += bbox/dms_bbox_log_dump.o
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/log
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/power
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/uda
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/logdrv/host
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/drv_devmng/drv_devmng_host/ascend910
        ifneq ($(filter $(PRODUCT), ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_LOG_DUMP_FROM_PCIE
        endif
    endif
endif
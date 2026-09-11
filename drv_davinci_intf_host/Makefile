ifeq ($(TOP_DIR),)
    ifneq ($(NOT_SUPPORT_SP), y)
        EXTRA_CFLAGS += -fstack-protector-all
    endif

    ccflags-y += -Wall -Werror
    obj-m   += drv_davinci_intf_host.o
    drv_davinci_intf_host-objs := davinci_intf_init.o davinci_intf_process.o
    EXTRA_CFLAGS += -DCFG_HOST_ENV
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=4
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=1
    ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    endif
    ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    endif
    ifeq ($(TARGET_CHIP_ID), hi1980b)
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
    endif
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_davinci_intf_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    else
        ccflags-y += -funsigned-char -Wextra -Wformat=2 -Wfloat-equal -Wcast-align -Wvla -Wundef -Wstack-usage=2048 $(WDATE_TIME)
        ccflags-y += -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
        TOPDIR_ABS := $(KERNEL_DIR)
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../abl/libc_sec/include
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/inc
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/src/drv_davinci_intf/drv_davinci_intf_host
		EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common
    endif
else
    ifneq ($(NOT_SUPPORT_SP), y)
        EXTRA_CFLAGS += -fstack-protector-all
    endif

    ccflags-y += -Wall -Werror
    obj-m   += drv_davinci_intf_host.o
    drv_davinci_intf_host-objs := davinci_intf_init.o davinci_intf_process.o
    EXTRA_CFLAGS += -DCFG_HOST_ENV
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=4
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=1

    ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    endif

    ifneq ($(filter $(PRODUCT),  ascend910B),)
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
    endif

    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_davinci_intf_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    else
        ccflags-y += -funsigned-char -Wextra -Wformat=2 -Wfloat-equal -Wcast-align -Wvla -Wundef -Wstack-usage=2048 $(WDATE_TIME)
        ccflags-y += -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
        EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_davinci_intf
        EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_davinci_intf/drv_davinci_intf_host
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common
    endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

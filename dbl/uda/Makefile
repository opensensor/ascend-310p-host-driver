ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ifeq ($(TOP_DIR),)
    #for dkms
    DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)

    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -DDRV_HOST
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=3
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=1
    ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    endif
    ifneq ($(filter $(TARGET_CHIP_ID), hi1980b),)
        EXTRA_CFLAGS += -DCFG_FEATUR_PROCESS_GROUP
    endif

    ifeq ($(TARGET_PRODUCT),cloud)
        ifneq ($(TARGET_CHIP_ID), hi1980)
            EXTRA_CFLAGS += -DCFG_FEATURE_SURPORT_UDEV_MNG
        endif
    else
        ifneq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            EXTRA_CFLAGS += -DCFG_FEATURE_SURPORT_UDEV_MNG
        endif
    endif

    ifneq ($(TARGET_CHIP_ID),hi1910)
        EXTRA_CFLAGS += -DCFG_FEATURE_DEVID_TRANS
    endif
    ifeq ($(TARGET_CHIP_ID), hi1980b)
        ascend_uda-objs += spod_info.o
    endif
    ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
        EXTRA_CFLAGS += -DCFG_FEATURE_KA
    endif
    ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA
    endif
else
    DRIVER_SRC_BASE_DIR := $(DRIVER_KERNEL_DIR)/src

    EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=3
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=1
    ifeq ($(PRODUCT_SIDE), host)
        EXTRA_CFLAGS += -DDRV_HOST
        ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
        endif
        ifneq ($(filter $(PRODUCT), ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATUR_PROCESS_GROUP
        endif
    endif

    ifeq ($(filter $(PRODUCT), ascend310p ascend910),)
        EXTRA_CFLAGS += -DCFG_FEATURE_SURPORT_UDEV_MNG
    endif
    ifeq ($(filter $(PRODUCT), ascend310 ascend310rc),)
        EXTRA_CFLAGS += -DCFG_FEATURE_DEVID_TRANS
    endif
    ifneq ($(filter $(PRODUCT), ascend310Brc ascend310Brcesl helper310p),)
        EXTRA_CFLAGS += -DCFG_FEATURE_VFIO_SOC
    endif
    ifneq ($(filter $(PRODUCT), ascend910B),)
        ascend_uda-objs += spod_info.o
    endif
    ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910 ascend910B),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif
	ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend310Brc ascend910 ascend910B),)
		EXTRA_CFLAGS += -DCFG_FEATURE_KA
	endif
endif

EXTRA_CFLAGS += -Wfloat-equal
EXTRA_CFLAGS += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

obj-m += ascend_uda.o
ascend_uda-objs += uda_access.o uda_dev.o uda_fops.o uda_notifier.o uda_proc_fs.o

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

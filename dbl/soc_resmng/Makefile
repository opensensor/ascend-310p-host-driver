ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif

ifeq ($(TOP_DIR),)
	#for dkms
	DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
	ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
		EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
	endif
	ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
		ifneq ($(filter $(PRODUCT),  helper310p),)
		else
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
		endif
	endif

	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
	DRIVER_SRC_BASE_DIR := $(DRIVER_KERNEL_DIR)/src
	EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/rms/trs_drv/tsmng
	EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/inc/dbl
	EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space_open/inc/depends/inc_driver
	ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
		ifeq ($(PRODUCT_SIDE), host)
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
		endif
	endif

	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
	EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
	EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver

endif

EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
EXTRA_CFLAGS += -I$(DRIVER_SRC_BASE_DIR)/rms/trs_drv/inc
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
obj-m += ascend_soc_resmng.o
ascend_soc_resmng-objs := soc_resmng.o soc_subsys_ts.o

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
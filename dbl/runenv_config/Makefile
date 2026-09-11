ifeq ($(DAVINCI_HIAI_DKMS), y)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/runenv_config
    EXTRA_CFLAGS += -DCFG_FEATURE_HOST_ENV

    ifeq ($(TARGET_CHIP_ID), hi1980b)
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
    endif
else
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/runenv_config

    ifeq ($(PRODUCT_SIDE), device)
        ifneq ($(filter $(PRODUCT), helper310p ascend310Brc ascend310Brcesl ascend310Brcemu as31xm1),)
            EXTRA_CFLAGS += -DCFG_FEATURE_RC_MODE
        endif
    endif

    ifneq ($(filter $(PRODUCT), ascend910B), )
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
    endif
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef
obj-m += dbl_runenv_config.o
dbl_runenv_config-objs := runenv_config_module.o docker_query.o vm_query.o

include $(FEATURE_MK_PATH)
EXTRA_CFLAGS += $(CONFIG_DEFINES)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform

ifeq ($(PRODUCT_SIDE), device)
    EXTRA_CFLAGS += -DCFG_FEATURE_DEVICE_ENV
    dbl_runenv_config-objs += device/runenv_custom.o
    dbl_runenv_config-objs += device/runenv_device.o
else
    EXTRA_CFLAGS += -DCFG_FEATURE_HOST_ENV
endif

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

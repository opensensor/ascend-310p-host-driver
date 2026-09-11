ifeq ($(DAVINCI_HIAI_DKMS), y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/runenv_config
    ccflags-y += -DCFG_FEATURE_HOST_ENV

    ifeq ($(TARGET_CHIP_ID), hi1980b)
        ccflags-y += -DCFG_FEATURE_SRIOV
    endif
else
    ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/runenv_config

    ifeq ($(PRODUCT_SIDE), device)
        ifneq ($(filter $(PRODUCT), helper310p ascend310Brc ascend310Brcesl ascend310Brcemu as31xm1),)
            ccflags-y += -DCFG_FEATURE_RC_MODE
        endif
    endif

    ifneq ($(filter $(PRODUCT), ascend910B), )
        ccflags-y += -DCFG_FEATURE_SRIOV
    endif
endif

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef
obj-m += dbl_runenv_config.o
dbl_runenv_config-objs := runenv_config_module.o docker_query.o vm_query.o

include $(FEATURE_MK_PATH)
ccflags-y += $(CONFIG_DEFINES)
ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform

ifeq ($(PRODUCT_SIDE), device)
    ccflags-y += -DCFG_FEATURE_DEVICE_ENV
    dbl_runenv_config-objs += device/runenv_custom.o
    dbl_runenv_config-objs += device/runenv_device.o
else
    ccflags-y += -DCFG_FEATURE_HOST_ENV
endif

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

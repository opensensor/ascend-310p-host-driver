ifeq ($(TARGET_BUILD_TYPE),debug)
    ccflags-y += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ifeq ($(TOP_DIR),)
    #for dkms
    DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/ascend_platform
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/udis
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/config
    ccflags-y += -I$(HIAI_DKMS_DIR)/common
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl/dev_urd
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
    ccflags-y += -DDRV_HOST
    ccflags-y += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=4
    ccflags-y += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=2
    ifneq ($(filter $(TARGET_CHIP_ID), hi1980b),)
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCONFIG_PLATFORM_910B
        ifneq ($(ASCEND910_93_EX),true)
            ccflags-y += -DCFG_UDIS_SUPPORT_VF
        endif
    endif

    obj-m += ascend_udis.o
    ascend_udis-objs += udis_management.o udis_timer.o udis_cmd.o host/udis_msg.o host/udis_data.o host/udis_init.o host/udis_addr.o
else
    DRIVER_SRC_BASE_DIR := $(DRIVER_KERNEL_DIR)/src
    ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
    ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/udis
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/config
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/common
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
    ccflags-y += -I$(TOP_DIR)/inc/driver
    ccflags-y += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=4
    ccflags-y += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=2
    ifeq ($(PRODUCT_SIDE), host)
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
        ccflags-y += -DDRV_HOST
        ccflags-y += -DCFG_FEATURE_HOST_LOG
    else
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    endif
    ifneq ($(filter $(PRODUCT), ascend910B),)
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        ifneq ($(ASCEND910_93_EX),true)
            ccflags-y += -DCFG_UDIS_SUPPORT_VF
        endif
    endif

    include $(FEATURE_MK_PATH)
    ccflags-y += $(CONFIG_DEFINES)

    obj-m += ascend_udis.o
    ascend_udis-objs += udis_management.o udis_timer.o  udis_cmd.o

    ifeq ($(PRODUCT_SIDE), host)
        ascend_udis-objs += host/udis_msg.o host/udis_data.o host/udis_init.o host/udis_addr.o
    else
        ascend_udis-objs += device/udis_msg.o device/udis_data.o device/udis_init.o device/udis_addr.o
    endif
endif

ccflags-y += -Wfloat-equal
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

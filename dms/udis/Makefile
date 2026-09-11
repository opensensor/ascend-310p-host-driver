ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ifeq ($(TOP_DIR),)
    #for dkms
    DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/ascend_platform
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/udis
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/config
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/common
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl/dev_urd
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
    EXTRA_CFLAGS += -DDRV_HOST
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=4
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=2
    ifneq ($(filter $(TARGET_CHIP_ID), hi1980b),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
        EXTRA_CFLAGS += -DCONFIG_PLATFORM_910B
        ifneq ($(ASCEND910_93_EX),true)
            EXTRA_CFLAGS += -DCFG_UDIS_SUPPORT_VF
        endif
    endif

    obj-m += ascend_udis.o
    ascend_udis-objs += udis_management.o udis_timer.o udis_cmd.o host/udis_msg.o host/udis_data.o host/udis_init.o host/udis_addr.o
else
    DRIVER_SRC_BASE_DIR := $(DRIVER_KERNEL_DIR)/src
    EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
    EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/udis
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/config
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/common
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
    EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=4
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=2
    ifeq ($(PRODUCT_SIDE), host)
        EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -DDRV_HOST
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    else
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    endif
    ifneq ($(filter $(PRODUCT), ascend910B),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        ifneq ($(ASCEND910_93_EX),true)
            EXTRA_CFLAGS += -DCFG_UDIS_SUPPORT_VF
        endif
    endif

    include $(FEATURE_MK_PATH)
    EXTRA_CFLAGS += $(CONFIG_DEFINES)

    obj-m += ascend_udis.o
    ascend_udis-objs += udis_management.o udis_timer.o  udis_cmd.o

    ifeq ($(PRODUCT_SIDE), host)
        ascend_udis-objs += host/udis_msg.o host/udis_data.o host/udis_init.o host/udis_addr.o
    else
        ascend_udis-objs += device/udis_msg.o device/udis_data.o device/udis_init.o device/udis_addr.o
    endif
endif

EXTRA_CFLAGS += -Wfloat-equal
EXTRA_CFLAGS += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

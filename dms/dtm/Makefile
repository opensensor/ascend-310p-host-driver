KERNEL_DIR :=$(srctree)

ifeq ($(TOP_DIR),)         # for DKMS cmake
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/dtm
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/config
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/sensor
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/event
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    EXTRA_CFLAGS += -DCFG_HOST_ENV
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=5
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0
    ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    endif

    ifeq ($(TARGET_PRODUCT),mdc)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
    endif

    ifeq ($(TARGET_PRODUCT),mini)
        EXTRA_CFLAGS += -DCFG_FEATURE_EP_MODE
    endif
    ifeq ($(TARGET_PRODUCT),cloud)
        EXTRA_CFLAGS += -DCFG_FEATURE_EP_MODE
    endif
    ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
        EXTRA_CFLAGS += -DCFG_FEATURE_EP_MODE
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    endif

    ifeq ($(TARGET_CHIP_ID), hi1980b)
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
        EXTRA_CFLAGS += -DCFG_FEATURE_VFG
        EXTRA_CFLAGS += -DCFG_CHIP_INFO_FROM_DEVINFO
        EXTRA_CFLAGS += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_STATUS
    else
        EXTRA_CFLAGS += -DCFG_FEATURE_TOPOLOGY_BY_SMP
    endif

else     # for cmake

    ifeq ($(PRODUCT_SIDE), host)
        EXTRA_CFLAGS += -DCFG_HOST_ENV
        ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
        endif
    else
        ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910 ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

    EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/dtm
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/config
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
    EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include

    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=5
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0

    ifneq ($(filter $(PRODUCT),  ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
    endif
    ifneq ($(filter $(PRODUCT),  as31xm1),)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V11
    endif
    ifneq ($(filter $(PRODUCT), ascend910B), )
        EXTRA_CFLAGS += -DCFG_FEATURE_VFG
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
        EXTRA_CFLAGS += -DCFG_CHIP_INFO_FROM_DEVINFO
        EXTRA_CFLAGS += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_STATUS
    endif
    ifneq ($(filter $(PRODUCT), ascend310 ascend310p ascend910 ascend910B ascend310B ascend310Besl ascend310Bemu),)
        EXTRA_CFLAGS += -DCFG_FEATURE_EP_MODE
    endif
endif

ccflags-y += -Wall -Werror

ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

obj-m += ascend_dms_dtm.o
ascend_dms_dtm-objs := dms_dev_node.o
ascend_dms_dtm-objs += dms_node_type.o
ascend_dms_dtm-objs += dms_dtm_init.o

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

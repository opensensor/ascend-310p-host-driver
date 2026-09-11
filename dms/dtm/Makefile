KERNEL_DIR :=$(srctree)

ifeq ($(TOP_DIR),)         # for DKMS cmake
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/dtm
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/config
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/sensor
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/event
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    ccflags-y += -DCFG_HOST_ENV
    ccflags-y += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=5
    ccflags-y += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0
    ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        ccflags-y += -DCFG_FEATURE_HOST_LOG
    endif

    ifeq ($(TARGET_PRODUCT),mdc)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
    endif

    ifeq ($(TARGET_PRODUCT),mini)
        ccflags-y += -DCFG_FEATURE_EP_MODE
    endif
    ifeq ($(TARGET_PRODUCT),cloud)
        ccflags-y += -DCFG_FEATURE_EP_MODE
    endif
    ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
        ccflags-y += -DCFG_FEATURE_EP_MODE
        ccflags-y += -DCFG_FEATURE_HOST_LOG
    endif

    ifeq ($(TARGET_CHIP_ID), hi1980b)
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_FEATURE_VFG
        ccflags-y += -DCFG_CHIP_INFO_FROM_DEVINFO
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_STATUS
    else
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_SMP
    endif

else     # for cmake

    ifeq ($(PRODUCT_SIDE), host)
        ccflags-y += -DCFG_HOST_ENV
        ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -DCFG_FEATURE_HOST_LOG
        endif
    else
        ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910 ascend910B),)
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

    ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/dtm
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/config
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
    ccflags-y += -I$(TOP_DIR)/inc/driver
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include

    ccflags-y += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=5
    ccflags-y += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0

    ifneq ($(filter $(PRODUCT),  ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
    endif
    ifneq ($(filter $(PRODUCT),  as31xm1),)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V11
    endif
    ifneq ($(filter $(PRODUCT), ascend910B), )
        ccflags-y += -DCFG_FEATURE_VFG
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_CHIP_INFO_FROM_DEVINFO
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_STATUS
    endif
    ifneq ($(filter $(PRODUCT), ascend310 ascend310p ascend910 ascend910B ascend310B ascend310Besl ascend310Bemu),)
        ccflags-y += -DCFG_FEATURE_EP_MODE
    endif
endif

ccflags-y += -Wall -Werror

ifeq ($(TARGET_BUILD_TYPE),debug)
    ccflags-y += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
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

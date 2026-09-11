KERNEL_DIR :=$(srctree)
ccflags-y += -Wframe-larger-than=2048

ifeq ($(TOP_DIR),)         # for DKMS cmake
    ifneq ($(NOT_SUPPORT_SP), y)
        ccflags-y += -fstack-protector-all
    endif

    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/dtm
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/config
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/sensor
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/event
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/core
    ccflags-y += -I$(HIAI_DKMS_DIR)/common
    ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
    ccflags-y += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    ccflags-y += -DCFG_HOST_ENV
    ccflags-y += -DCFG_FEATURE_LOG_STANDARD_FAULT_EVENT
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
        ccflags-y += -DCFG_FEATURE_PG
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_FEATURE_VFG
        ccflags-y += -DCFG_FEATURE_HOST_AICPU
        ccflags-y += -DCFG_FEATURE_NEW_EVENT_CODE
        ccflags-y += -DCFG_CHIP_INFO_FROM_DEVINFO
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
        ccflags-y += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
    else
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_SMP
    endif

    ifeq ($(TARGET_CHIP_ID), hi1910b)
        ccflags-y += -DCFG_FEATURE_NEW_EVENT_CODE
    endif

    ifeq ($(TARGET_CHIP_ID), hi1910)
        ccflags-y += -DCFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
    endif
    ccflags-y += -Wall -Werror

    ifeq ($(TARGET_BUILD_TYPE),debug)
        ccflags-y += -DCFG_BUILD_DEBUG
    endif

    obj-m += ascend_dms_smf.o
    ascend_dms_smf-objs += dms_smf_init.o
    ascend_dms_smf-objs += logdrv/log_drv_write_ring_buffer.o
    include $(HIAI_DKMS_DIR)/dms/smf/core/dms_sensor_core.mk
    include $(HIAI_DKMS_DIR)/dms/smf/event/dms_event.mk
    include $(HIAI_DKMS_DIR)/dms/smf/sensor/dms_sensor.mk

else     # for cmake

    ifeq ($(PRODUCT_SIDE), host)
        ccflags-y += -DCFG_HOST_ENV
        ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -DCFG_FEATURE_HOST_LOG
        endif
    else
        ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910B ascend910),)
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

    ifneq ($(filter $(PRODUCT), ascend310),)
        ccflags-y += -DCFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
    endif

    ifneq ($(NOT_SUPPORT_SP), y)
        ccflags-y += -fstack-protector-all
    endif

    ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
    endif

    ifneq ($(filter $(PRODUCT), as31xm1),)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V11
    endif

    ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl as31xm1),)
        ccflags-y += -DCFG_SUPPORT_DEFAULT_SEVERITY
    endif

    ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend910B as31xm1),)
        ccflags-y += -DCFG_FEATURE_NEW_EVENT_CODE
    endif
    ifeq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl as31xm1),)
        ccflags-y += -DCFG_FEATURE_LOG_STANDARD_FAULT_EVENT
    endif
    ifneq ($(filter $(PRODUCT), ascend910B), )
        ccflags-y += -DCFG_FEATURE_PG
        ccflags-y += -DCFG_FEATURE_VFG
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
        ccflags-y += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
    endif
    ifneq ($(filter $(PRODUCT), as31xm1 ascend610 ascend610Lite bs9sx1a), )
        ccflags-y += -DCFG_FEATURE_MDC_CLOCKID_CONFIG
    endif
    ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl ascend310p helper310p),)
        ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
    endif

    ifneq ($(filter $(PRODUCT), ascend310p ascend910 ascend910B ascend310B),)
        ccflags-y += -DCFG_FEATURE_EP_MODE
    endif

    ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
    ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/dtm
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/config
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/core
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/common
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/tsdrv/ts_drv/ts_drv_common
    ccflags-y += -I$(TOP_DIR)/inc/driver
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    ccflags-y += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=5
    ccflags-y += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0

    ccflags-y += -Wall -Werror

    ifeq ($(TARGET_BUILD_TYPE),debug)
        ccflags-y += -DCFG_BUILD_DEBUG
    endif

    obj-m += ascend_dms_smf.o
    ascend_dms_smf-objs += dms_smf_init.o
    ascend_dms_smf-objs += logdrv/log_drv_write_ring_buffer.o
    include ${DRIVER_KERNEL_DIR}/src/dms/smf/core/dms_sensor_core.mk
    include ${DRIVER_KERNEL_DIR}/src/dms/smf/event/dms_event.mk
    include ${DRIVER_KERNEL_DIR}/src/dms/smf/sensor/dms_sensor.mk
    include ${DRIVER_KERNEL_DIR}/src/dms/smf/report/dfm_report.mk
endif

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

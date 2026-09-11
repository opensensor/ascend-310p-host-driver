KERNEL_DIR :=$(srctree)
EXTRA_CFLAGS += -Wframe-larger-than=2048

ifeq ($(TOP_DIR),)         # for DKMS cmake
    ifneq ($(NOT_SUPPORT_SP), y)
        EXTRA_CFLAGS += -fstack-protector-all
    endif

    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/dtm
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/config
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/sensor
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/event
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/core
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/common
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    EXTRA_CFLAGS += -DCFG_HOST_ENV
    EXTRA_CFLAGS += -DCFG_FEATURE_LOG_STANDARD_FAULT_EVENT
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
        EXTRA_CFLAGS += -DCFG_FEATURE_PG
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
        EXTRA_CFLAGS += -DCFG_FEATURE_VFG
        EXTRA_CFLAGS += -DCFG_FEATURE_HOST_AICPU
        EXTRA_CFLAGS += -DCFG_FEATURE_NEW_EVENT_CODE
        EXTRA_CFLAGS += -DCFG_CHIP_INFO_FROM_DEVINFO
        EXTRA_CFLAGS += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
        EXTRA_CFLAGS += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
    else
        EXTRA_CFLAGS += -DCFG_FEATURE_TOPOLOGY_BY_SMP
    endif

    ifeq ($(TARGET_CHIP_ID), hi1910b)
        EXTRA_CFLAGS += -DCFG_FEATURE_NEW_EVENT_CODE
    endif

    ifeq ($(TARGET_CHIP_ID), hi1910)
        EXTRA_CFLAGS += -DCFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
    endif
    ccflags-y += -Wall -Werror

    ifeq ($(TARGET_BUILD_TYPE),debug)
        EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
    endif

    obj-m += ascend_dms_smf.o
    ascend_dms_smf-objs += dms_smf_init.o
    ascend_dms_smf-objs += logdrv/log_drv_write_ring_buffer.o
    include $(HIAI_DKMS_DIR)/dms/smf/core/dms_sensor_core.mk
    include $(HIAI_DKMS_DIR)/dms/smf/event/dms_event.mk
    include $(HIAI_DKMS_DIR)/dms/smf/sensor/dms_sensor.mk

else     # for cmake

    ifeq ($(PRODUCT_SIDE), host)
        EXTRA_CFLAGS += -DCFG_HOST_ENV
        ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
        endif
    else
        ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910B ascend910),)
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

    ifneq ($(filter $(PRODUCT), ascend310),)
        EXTRA_CFLAGS += -DCFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
    endif

    ifneq ($(NOT_SUPPORT_SP), y)
        EXTRA_CFLAGS += -fstack-protector-all
    endif

    ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
    endif

    ifneq ($(filter $(PRODUCT), as31xm1),)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V11
    endif

    ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl as31xm1),)
        EXTRA_CFLAGS += -DCFG_SUPPORT_DEFAULT_SEVERITY
    endif

    ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend910B as31xm1),)
        EXTRA_CFLAGS += -DCFG_FEATURE_NEW_EVENT_CODE
    endif
    ifeq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl as31xm1),)
        EXTRA_CFLAGS += -DCFG_FEATURE_LOG_STANDARD_FAULT_EVENT
    endif
    ifneq ($(filter $(PRODUCT), ascend910B), )
        EXTRA_CFLAGS += -DCFG_FEATURE_PG
        EXTRA_CFLAGS += -DCFG_FEATURE_VFG
        EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
        EXTRA_CFLAGS += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
        EXTRA_CFLAGS += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
    endif
    ifneq ($(filter $(PRODUCT), as31xm1 ascend610 ascend610Lite bs9sx1a), )
        EXTRA_CFLAGS += -DCFG_FEATURE_MDC_CLOCKID_CONFIG
    endif
    ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl ascend310p helper310p),)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
    endif

    ifneq ($(filter $(PRODUCT), ascend310p ascend910 ascend910B ascend310B),)
        EXTRA_CFLAGS += -DCFG_FEATURE_EP_MODE
    endif

    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
    EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/dtm
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/config
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/core
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/common
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/tsdrv/ts_drv/ts_drv_common
    EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=5
    EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0

    ccflags-y += -Wall -Werror

    ifeq ($(TARGET_BUILD_TYPE),debug)
        EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
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

ifeq ($(TARGET_BUILD_TYPE),debug)
	ccflags-y += -DCFG_BUILD_DEBUG
endif

ifeq ($(TOP_DIR),)

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -DCFG_FEATURE_SHARE_LOG

ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    ifeq ($(TARGET_PRODUCT),cloud)
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        ifeq ($(TARGET_CHIP_ID), hi1980b)
            ccflags-y += -DCFG_FEATURE_SRIOV
            ccflags-y += -DCFG_FEATURE_SIMPLIFY_MODE
            ccflags-y += -DCFG_DISABLE_TSD_NOTIFY
        else
            ccflags-y += -DCFG_FEATURE_VFIO
        endif
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
            ccflags-y += -DCFG_FEATURE_VFIO
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        else ifeq ($(TARGET_CHIP_ID),hi1910b)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        else
            ccflags-y += -DCFG_SOC_PLATFORM_MINIV1
            ccflags-y += -DCFG_FEATURE_SIMPLIFY_MODE
            ccflags-y += -DCFG_FEATURE_PROCESS_NUM_LITE
            ifeq ($(filter $(PRODUCT),as31xm1 ascend610Lite ascend610Liteesl),)
                ccflags-y += -DCFG_FEATURE_NOT_SUPPORT_UDA
            endif
        endif
    endif

else
    CUR_MAKEFILE_PATH := $(strip \
                $(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
                $(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
                )

    ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_inc
    ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../abl/libc_sec/include
    ifeq ($(TARGET_PRODUCT),cloud)
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
            ccflags-y += -DCFG_FEATURE_VFIO
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        else ifeq ($(TARGET_CHIP_ID),hi1910b)
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        else
            ccflags-y += -DCFG_SOC_PLATFORM_MINIV1
            ccflags-y += -DCFG_FEATURE_SIMPLIFY_MODE
            ccflags-y += -DCFG_FEATURE_PROCESS_NUM_LITE
            ifeq ($(filter $(PRODUCT),as31xm1 ascend610Lite ascend610Liteesl),)
                ccflags-y += -DCFG_FEATURE_NOT_SUPPORT_UDA
            endif
        endif
    endif

endif

ifeq ($(CUSTOM_OS_CCFLAGS), )
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
else
ccflags-y += $(CUSTOM_OS_CCFLAGS)
endif

obj-m += drv_pcie_hdc_host.o

ifeq ($(TARGET_PRODUCT),cloud)
    drv_pcie_hdc_host-objs := hdcdrv_core.o hdcdrv_core_com.o hdcdrv_host.o hdcdrv_mem.o hdcdrv_mem_com.o hdcdrv_epoll.o hdcdrv_sysfs.o
    ifneq ($(TARGET_CHIP_ID), hi1980b)
        drv_pcie_hdc_host-objs += hdcdrv_adapter.o
    endif
else
    ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
        drv_pcie_hdc_host-objs := hdcdrv_core.o hdcdrv_core_com.o hdcdrv_host.o hdcdrv_mem.o hdcdrv_mem_com.o hdcdrv_epoll.o hdcdrv_sysfs.o hdcdrv_adapter.o
    else
        drv_pcie_hdc_host-objs := hdcdrv_core.o hdcdrv_core_com.o hdcdrv_host.o hdcdrv_mem.o hdcdrv_mem_com.o hdcdrv_epoll.o hdcdrv_sysfs.o
    endif
endif

else # ifeq ($(TOP_DIR),)

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -DCFG_FEATURE_SHARE_LOG

ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    ifneq ($(filter $(PRODUCT),  ascend910B),)
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_DISABLE_TSD_NOTIFY
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_SIMPLIFY_MODE
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else ifeq ($(PRODUCT),ascend910)
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else ifeq ($(PRODUCT),ascend310p)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else ifeq ($(PRODUCT),ascend310B)
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ccflags-y += -DCFG_SOC_PLATFORM_MINIV1
        ccflags-y += -DCFG_FEATURE_PROCESS_NUM_LITE
        ifeq ($(filter $(PRODUCT),as31xm1 ascend610Lite ascend610Liteesl),)
            ccflags-y += -DCFG_FEATURE_NOT_SUPPORT_UDA
        endif
    endif

    ifeq ($(TARGET_PRODUCT),cloud)
        ifeq ($(TARGET_CHIP_ID), hi1980b)
            ccflags-y += -DCFG_FEATURE_SRIOV
            ccflags-y += -DCFG_DISABLE_TSD_NOTIFY
        endif
    endif
else

    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
    ccflags-y += -I$(TOP_DIR)/inc/driver
    ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include

    ifeq ($(PRODUCT),ascend910B)
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_DISABLE_TSD_NOTIFY
        ccflags-y += -DCFG_FEATURE_SIMPLIFY_MODE
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else ifeq ($(PRODUCT),ascend910)
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else ifeq ($(PRODUCT),ascend310p)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else ifneq ($(filter $(PRODUCT),  ascend310B),)
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ccflags-y += -DCFG_SOC_PLATFORM_MINIV1
        ccflags-y += -DCFG_FEATURE_SIMPLIFY_MODE
        ccflags-y += -DCFG_FEATURE_PROCESS_NUM_LITE
        ifeq ($(filter $(PRODUCT),as31xm1 ascend610Lite ascend610Liteesl),)
            ccflags-y += -DCFG_FEATURE_NOT_SUPPORT_UDA
        endif
    endif

    ifneq ($(filter $(PRODUCT), as31xm1 ascend610Lite ascend610Liteesl),)
        ccflags-y += -DCFG_FEATURE_HDC_REG_MEM
    endif
    ifneq ($(filter $(PRODUCT), as31xm1),)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V11
    endif
	ifneq ($(filter $(PRODUCT),  ascend610Lite),)
		ccflags-y += -DCFG_SOC_PLATFORM_MDC_LITE_FPGA
        ccflags-y += -DCFG_FEATURE_PFSTAT
        ccflags-y += -DCFG_FEATURE_SEC_COMM_L3
	endif
endif

ifeq ($(CUSTOM_OS_CCFLAGS), )
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
else
ccflags-y += $(CUSTOM_OS_CCFLAGS)
endif

obj-m += drv_pcie_hdc_host.o

ifneq ($(filter $(PRODUCT), ascend910 ascend310p),)
drv_pcie_hdc_host-objs := hdcdrv_core.o hdcdrv_core_com.o hdcdrv_host.o hdcdrv_mem.o hdcdrv_mem_com.o hdcdrv_epoll.o hdcdrv_sysfs.o hdcdrv_adapter.o
else
drv_pcie_hdc_host-objs := hdcdrv_core.o hdcdrv_core_com.o hdcdrv_host.o hdcdrv_mem.o hdcdrv_mem_com.o hdcdrv_epoll.o hdcdrv_sysfs.o
endif

ifneq ($(filter $(PRODUCT), as31xm1 ascend610Lite ascend610Liteesl),)
    ccflags-y += -I$(TOP_DIR)/inc/driver/
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include/
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/config/
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/event/
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/dfm_template/
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/core/
    drv_pcie_hdc_host-objs += hdcdrv_status.o
endif

ifneq ($(filter $(PRODUCT), ascend610Lite),)
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_pfstat/include
    drv_pcie_hdc_host-objs += hdcdrv_pfstat.o
endif

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif

endif # ifeq ($(TOP_DIR),)
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

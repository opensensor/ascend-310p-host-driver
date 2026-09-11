ifeq ($(TARGET_BUILD_TYPE),debug)
	ccflags-y += -DCFG_BUILD_DEBUG
endif

ifeq ($(TOP_DIR),)
ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    ifeq ($(TARGET_PRODUCT),cloud)
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            ccflags-y += -DCFG_SOC_PLATFORM_ESL_FPGA
            ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
            ccflags-y += -DCFG_FEATURE_VFIO
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
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
            ccflags-y += -DCFG_SOC_PLATFORM_ESL_FPGA
            ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
            ccflags-y += -DCFG_FEATURE_VFIO
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

endif

ifneq ($(NOT_SUPPORT_SP), y)
	ccflags-y += -fstack-protector-all
endif
ccflags-y += -DSHARE_LOG_ENABLE
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

obj-m += drv_vhdc.o
drv_vhdc-objs := vhdc_agent.o vhdc_core.o vhdc_mem.o hdcdrv_core_com.o hdcdrv_mem_com.o
else # ifeq ($(TOP_DIR),)

ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    ifeq ($(TARGET_PRODUCT),cloud)
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            ccflags-y += -DCFG_SOC_PLATFORM_ESL_FPGA
            ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
            ccflags-y += -DCFG_FEATURE_VFIO
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

else

    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(TOP_DIR)/inc/driver
    ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
    ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include

    ifeq ($(PRODUCT),ascend910)
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
        ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
        ccflags-y += -DCFG_FEATURE_VFIO
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(PRODUCT),ascend310p)
            ccflags-y += -DCFG_SOC_PLATFORM_ESL_FPGA
            ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
            ccflags-y += -DCFG_FEATURE_VFIO
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

endif

ifneq ($(NOT_SUPPORT_SP), y)
	ccflags-y += -fstack-protector-all
endif
ccflags-y += -DCFG_FEATURE_SHARE_LOG
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

obj-m += drv_vhdc.o
drv_vhdc-objs := vhdc_agent.o vhdc_core.o vhdc_mem.o hdcdrv_core_com.o hdcdrv_mem_com.o


endif # ifeq ($(TOP_DIR),)
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

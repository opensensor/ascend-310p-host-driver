ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifeq ($(TOP_DIR),)
ifeq ($(DAVINCI_HIAI_DKMS),y)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    ifeq ($(TARGET_PRODUCT),cloud)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
        EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL_FPGA
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
            EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

else
    CUR_MAKEFILE_PATH := $(strip \
                $(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
                $(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
                )

    EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_inc
    EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../abl/libc_sec/include
    ifeq ($(TARGET_PRODUCT),cloud)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
        EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL_FPGA
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
            EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif
EXTRA_CFLAGS += -DSHARE_LOG_ENABLE
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

obj-m += drv_vhdc.o
drv_vhdc-objs := vhdc_agent.o vhdc_core.o vhdc_mem.o hdcdrv_core_com.o hdcdrv_mem_com.o
else # ifeq ($(TOP_DIR),)

ifeq ($(DAVINCI_HIAI_DKMS),y)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

    ifeq ($(TARGET_PRODUCT),cloud)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
        EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL_FPGA
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
            EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

else

    EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
    EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include

    ifeq ($(PRODUCT),ascend910)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
        EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    else
        ifeq ($(PRODUCT),ascend310p)
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL_FPGA
            EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51
            EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
    endif

endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif
EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

obj-m += drv_vhdc.o
drv_vhdc-objs := vhdc_agent.o vhdc_core.o vhdc_mem.o hdcdrv_core_com.o hdcdrv_mem_com.o


endif # ifeq ($(TOP_DIR),)
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

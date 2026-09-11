ifeq ($(TOP_DIR),)

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif

ifeq ($(DAVINCI_HIAI_DKMS),y)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc/dbl
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/pcievnic_common_h
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/pcievnic_host
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
	ifeq ($(TARGET_PRODUCT),cloud)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
		EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
		EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        ifeq ($(TARGET_CHIP_ID), hi1980b)
            EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
            EXTRA_CFLAGS += -DCFG_FEATURE_AGENT_SMMU
        endif
	else
		ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL_FPGA
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
			EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
		else ifeq ($(TARGET_CHIP_ID),hi1910b)
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
			EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV1
		endif
	endif
else
	CUR_MAKEFILE_PATH := $(strip \
                $(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
                $(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
                )

	EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_inc
	EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../inc/driver/
	EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../abl/libc_sec/include
	EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/pcivnic/pcievnic_common_h
	EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/pcivnic/pcievnic_host
	ifeq ($(TARGET_PRODUCT),cloud)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
	else
		ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL_FPGA
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV1
		endif
	endif
endif

ifeq ($(CUSTOM_OS_CCFLAGS), )
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-empty-body
else
ccflags-y += $(CUSTOM_OS_CCFLAGS)
endif

obj-m += drv_pcie_vnic_host.o
drv_pcie_vnic_host-objs := pcivnic_main.o pcivnic_host.o pcivnic_init.o
else

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif

ifeq ($(DAVINCI_HIAI_DKMS),y)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc/dbl
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/pcievnic_common_h
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/pcievnic_host
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
	ifeq ($(PRODUCT),ascend910)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
	endif
	ifeq ($(PRODUCT),ascend910B)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
		EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
        EXTRA_CFLAGS += -DCFG_FEATURE_AGENT_SMMU
	endif
	ifeq ($(PRODUCT),ascend310)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV1
	endif
else
    EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/dbl
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
	EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver/
	EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/pcivnic/pcievnic_common_h
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/pcivnic/pcievnic_host
	EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
	ifeq ($(PRODUCT),ascend910)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
	endif
	ifeq ($(PRODUCT),ascend910B)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
		EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
        EXTRA_CFLAGS += -DCFG_FEATURE_AGENT_SMMU
	endif

	ifeq ($(PRODUCT),ascend310)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV1
	endif

	ifneq ($(filter $(PRODUCT), ascend310B ascend910 ascend910B ascend310p),)
		EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
		EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
	endif
endif

ifeq ($(CUSTOM_OS_CCFLAGS), )
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-empty-body
else
ccflags-y += $(CUSTOM_OS_CCFLAGS)
endif
obj-m += drv_pcie_vnic_host.o
drv_pcie_vnic_host-objs := pcivnic_main.o pcivnic_host.o pcivnic_init.o

endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

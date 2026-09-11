ifeq ($(TARGET_PRODUCT),cloud)
    ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
endif

ifneq ($(filter $(PRODUCT), ascend910),)
	ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
else ifneq ($(filter $(PRODUCT), ascend910B),)
	ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
	ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_V2
	ccflags-y += -DCFG_FEATURE_ROOTKEY_STATUS
else ifneq ($(filter $(PRODUCT), helper310p ascend310p),)
	ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
	ccflags-y += -DCFG_FEATURE_ROOTKEY_STATUS
else ifeq ($(PRODUCT),ascend310)
	ccflags-y += -DCFG_SOC_PLATFORM_MINI
else ifeq ($(PRODUCT),ascend310rc)
	ccflags-y += -DCFG_SOC_PLATFORM_MINI
	ccflags-y += -DCFG_SOC_PLATFORM_MINI_RC
else ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc),)
	ccflags-y += -DCFG_SOC_PLATFORM_MINIV3
	ccflags-y += -DCFG_SOC_PLATFORM_MINIV3_RC
endif
ifeq ($(PRODUCT), ascend310p)
	ccflags-y += -DCFG_FEATURE_DECRYPTION
endif
ifneq ($(filter $(PRODUCT), helper310p),)
	ccflags-y += -DCFG_SOC_PLATFORM_HELPER
endif

ccflags-y += -I$(TOP_DIR)/inc/driver/
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/common/
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/prof/
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/prof/prof_inc
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/prof/prof_mem_sample
ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/
ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include

ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/prof/ascd910B
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/prof/prof_virt
ccflags-y += -DCFG_FEATURE_KTHREAD_BIND_CORE
ccflags-y += -DCFG_FEATURE_HOST_COLLECTION
ccflags-y += -DCFG_FEATURE_STARS
ccflags-y += -DCFG_FEATURE_SRIOV

# ccflags-y += -Wall -Werror
ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char  -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/debug/

ifeq ($(TOP_DIR),)
    #for dkms
    ccflags-y += -I$(HIAI_DKMS_DIR)/rms/trs_drv/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/dbl
    ccflags-y += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver/
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl/
    ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_common/
	ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
	ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt
	ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
endif
obj-m += ts_debug.o
obj-m += debug_switch.o
ts_debug-objs := debug_drv_dev.o debug_dma.o
ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/rms/trs_drv/inc
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

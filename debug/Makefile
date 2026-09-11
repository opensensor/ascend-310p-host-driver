ifeq ($(TARGET_PRODUCT),cloud)
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
endif

ifneq ($(filter $(PRODUCT), ascend910),)
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
else ifneq ($(filter $(PRODUCT), ascend910B),)
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
	EXTRA_CFLAGS += -DCFG_FEATURE_ROOTKEY_STATUS
else ifneq ($(filter $(PRODUCT), helper310p ascend310p),)
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
	EXTRA_CFLAGS += -DCFG_FEATURE_ROOTKEY_STATUS
else ifeq ($(PRODUCT),ascend310)
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
else ifeq ($(PRODUCT),ascend310rc)
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI_RC
else ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc),)
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3_RC
endif
ifeq ($(PRODUCT), ascend310p)
	EXTRA_CFLAGS += -DCFG_FEATURE_DECRYPTION
endif
ifneq ($(filter $(PRODUCT), helper310p),)
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_HELPER
endif

EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/common/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/prof/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/prof/prof_inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/prof/prof_mem_sample
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/prof/ascd910B
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/prof/prof_virt
EXTRA_CFLAGS += -DCFG_FEATURE_KTHREAD_BIND_CORE
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_COLLECTION
EXTRA_CFLAGS += -DCFG_FEATURE_STARS
EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV

# ccflags-y += -Wall -Werror
ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char  -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/debug/

ifeq ($(TOP_DIR),)
    #for dkms
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/rms/trs_drv/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/dbl
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/tsdrv/ts_drv/ts_drv_common
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver/
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl/
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
endif
obj-m += ts_debug.o
obj-m += debug_switch.o
ts_debug-objs := debug_drv_dev.o debug_dma.o
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/rms/trs_drv/inc
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

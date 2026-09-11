EXTRA_CFLAGS += -fstack-protector-all

ifeq ($(TOP_DIR),)
ifeq ($(CONFIG_HOST_DBG_DEVDRV),)
	ifeq ($(CONFIG_NPU_DEVDRV),)
	CONFIG_HOST_DBG_DEVDRV=m
	endif
endif

ifeq ($(DAVINCI_HIAI_DKMS),y)
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/npu_inc/
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
ifeq ($(TARGET_PRODUCT),cloud)
EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
endif
else
KERNEL_DIR :=$(srctree)
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/dev_inc/
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/dev_core/device_manager/device_manager_inc
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/npu_inc/
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
ifeq ($(TARGET_PRODUCT),cloud)
EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
endif
endif

obj-$(CONFIG_HOST_DBG_DEVDRV)   += drv_dbg_host.o
drv_dbg_host-objs := drv_mailbox.o debug_drv_dev.o

else

ifeq ($(CONFIG_HOST_DBG_DEVDRV),)
	ifeq ($(CONFIG_NPU_DEVDRV),)
	CONFIG_HOST_DBG_DEVDRV=m
	endif
endif

ifeq ($(DAVINCI_HIAI_DKMS),y)
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/npu_inc/
EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
ifneq ($(filter $(PRODUCT),  ascend910 ascend910B ascend920esl ascend920emu),)
EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
endif
else
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
	EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
ifneq ($(filter $(PRODUCT),  ascend910 ascend910B ascend920esl ascend920emu),)
EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
endif
endif

obj-$(CONFIG_HOST_DBG_DEVDRV)   += drv_dbg_host.o
drv_dbg_host-objs := drv_mailbox.o debug_drv_dev.o

endif

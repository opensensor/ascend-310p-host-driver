ccflags-y += -fstack-protector-all

ifeq ($(TOP_DIR),)
ifeq ($(CONFIG_HOST_DBG_DEVDRV),)
	ifeq ($(CONFIG_NPU_DEVDRV),)
	CONFIG_HOST_DBG_DEVDRV=m
	endif
endif

ifeq ($(DAVINCI_HIAI_DKMS),y)
ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host
ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_common
ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc/
ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
ifeq ($(TARGET_PRODUCT),cloud)
ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
endif
else
KERNEL_DIR :=$(srctree)
ccflags-y += -I$(KERNEL_DIR)/../../drivers/dev_inc/
ccflags-y += -I$(KERNEL_DIR)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host
ccflags-y += -I$(KERNEL_DIR)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common
ccflags-y += -I$(KERNEL_DIR)/../../drivers/dev_core/device_manager/device_manager_inc
ccflags-y += -I$(KERNEL_DIR)/../../drivers/npu_inc/
ccflags-y += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
ifeq ($(TARGET_PRODUCT),cloud)
ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
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
ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host
ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_common
ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc/
ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
ifneq ($(filter $(PRODUCT),  ascend910 ascend910B ascend920esl ascend920emu),)
ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
endif
else
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
	ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/
	ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
ifneq ($(filter $(PRODUCT),  ascend910 ascend910B ascend920esl ascend920emu),)
ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
endif
endif

obj-$(CONFIG_HOST_DBG_DEVDRV)   += drv_dbg_host.o
drv_dbg_host-objs := drv_mailbox.o debug_drv_dev.o

endif

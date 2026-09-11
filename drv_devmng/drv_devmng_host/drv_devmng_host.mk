ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif

ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) # for DKMS compile
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/npu_inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include/
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/time
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/soft_fault
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/power
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/urd_forward
		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_common
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
			EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
			EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
			ifeq ($(TARGET_CHIP_ID), hi1980)
				EXTRA_CFLAGS += -DCFG_FEATURE_DDR
				EXTRA_CFLAGS += -DCFG_FEATURE_DMS_SVM_DEV
				EXTRA_CFLAGS += -DCFG_FEATURE_SMP_VDEV_NOT_SUPPORT
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/logdrv/host
			endif
			ifeq ($(TARGET_CHIP_ID), hi1980b)
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
				EXTRA_CFLAGS += -DCFG_FEATURE_CHIP_DIE
				EXTRA_CFLAGS += -DCFG_FEATURE_PCIE_BBOX_ADDR
				EXTRA_CFLAGS += -DCFG_FEATURE_VF_USE_DEVID
				EXTRA_CFLAGS += -DCFG_FEATURE_VDEV_MEMORY_FROM_DEVMNG_H2D
				EXTRA_CFLAGS += -DCFG_FEATURE_BIND_TGID
				EXTRA_CFLAGS += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
				EXTRA_CFLAGS += -DCFG_FEATURE_BBOX_KDUMP
				EXTRA_CFLAGS += -DCFG_FEATURE_ADMIN_CONTAINER_PID
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/logdrv/host
			endif
		else ifeq ($(TARGET_PRODUCT),mini)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			EXTRA_CFLAGS += -DCFG_FEATURE_DDR
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -DCFG_FEATURE_VFG
				EXTRA_CFLAGS += -DCFG_FEATURE_DMS_SVM_DEV
				EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND
				EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
				EXTRA_CFLAGS += -DCFG_FEATURE_DEVICE_SHARE
				EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
				EXTRA_CFLAGS += -DCFG_FEATURE_CHIP_REG_FORCED_EXPORT
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_common
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
				ifneq ($(filter $(PRODUCT),  helper310p),)
				else
					EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/logdrv/host
				endif
			else ifeq ($(TARGET_CHIP_ID), hi1910b)
				EXTRA_CFLAGS += -DCFG_FEATURE_PCIE_BBOX_ADDR
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_common
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/logdrv/host
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend310
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend310
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
			endif
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			EXTRA_CFLAGS += -DCFG_FEATURE_DDR
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_common
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_common
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend310
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend310
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
			endif
		endif
	else # ifeq ($(DAVINCI_HIAI_DKMS),y)
		CUR_MAKEFILE_PATH := $(strip \
				$(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
				$(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
				)

		EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../inc/driver
		EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../abl/libc_sec/include/
		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND
			EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
			ifeq ($(TARGET_CHIP_ID), $(A9XXGEN02M01))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
				EXTRA_CFLAGS += -DCFG_FEATURE_CHIP_DIE
			else
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -DCFG_FEATURE_SMP_VDEV_NOT_SUPPORT
			endif

			EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_device/ascend910
			EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend910
			EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/
			EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND
				EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
				EXTRA_CFLAGS += -DCFG_FEATURE_DEVICE_SHARE
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/
			else
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/ascend310
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend310
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/ts_drv_common/
			endif
		endif
	endif # ifeq ($(DAVINCI_HIAI_DKMS),y)

	EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
	EXTRA_CFLAGS += -DCFG_SURPPORT_PCIE_HOST_DEVICE_COMM

	ifeq ($(TARGET_PRODUCT), cloud)
		BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend910
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pcie.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_init.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_cmd_proc.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_pid_map.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_chip_dev_map.o
		drv_devmng_host-y += drv_devmng/drv_devmng_common/dev_mnt_vdevice.o
	endif
	ifeq ($(TARGET_PRODUCT), mdc)
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
			BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend910
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pcie.o
		else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
			BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend910
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pcie.o
		else
			BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend310
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_host_drvops.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_nsm.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_container_noplugin.o
		endif
	endif
	ifeq ($(TARGET_PRODUCT), mini)
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend910
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pcie.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_init.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_cmd_proc.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_pid_map.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_chip_dev_map.o
			drv_devmng_host-y += drv_devmng/drv_devmng_common/dev_mnt_vdevice.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_dev_share.o
		else ifeq ($(TARGET_CHIP_ID), hi1910b)
			BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend910
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pcie.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_init.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_cmd_proc.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_pid_map.o
			drv_devmng_host-y += drv_devmng/drv_devmng_common/dev_mnt_vdevice.o
		else
			BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend310
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_host_drvops.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_nsm.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_container_noplugin.o
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_dev_share.o
		endif
	endif

	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_msg.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pm.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_ipc_msg.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_driver_pm.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_rand.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_black_box.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_container.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_device_online.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/drv_log.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/tsdrv_status.o

	ifeq ($(TARGET_PRODUCT),cloud)
		ccflags-y += -Wall -Werror
	endif

else # ifeq ($(TOP_DIR),)

	ifeq ($(DAVINCI_HIAI_DKMS),y) # reserve
		ifeq ($(filter $(PRODUCT),  ascend310 ascend310rc),)
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
		else
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
		endif

		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/npu_inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include/
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/time
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/soft_fault
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/power
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/event
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/urd_forward

		ifeq ($(TARGET_PRODUCT),ascend910)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
			EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND
			EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
			EXTRA_CFLAGS += -DCFG_FEATURE_SMP_VDEV_NOT_SUPPORT
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend910

		else ifneq ($(filter $(PRODUCT), ascend310 ascend310p),)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(PRODUCT),ascend310p)
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
				EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND
				EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
				EXTRA_CFLAGS += -DCFG_FEATURE_DEVICE_SHARE
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend910
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend310
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend310
			endif
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifneq ($(filter $(PRODUCT), ascend610 ascend610Lite ascend610Liteesl ascend310B ascend310Besl ascend310Bemu),)
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend910
			else ifeq ($(PRODUCT),bs9sx1a)
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend910
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend310
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend310
			endif
		endif
	else # ifeq ($(DAVINCI_HIAI_DKMS),y)
# for CMake compile
		ifeq ($(filter $(PRODUCT),  ascend310 ascend310rc),)
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/
		else
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common
		endif

		ifeq ($(PRODUCT),bs9sx1a)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
		endif

		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/event
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/time
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/soft_fault
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/power
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/uda
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/urd_forward
		ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/logdrv/host
		endif

		ifneq ($(filter $(PRODUCT),  ascend910 ascend910B),)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
			EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND

			ifeq ($(PRODUCT), ascend910)
				EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
				EXTRA_CFLAGS += -DCFG_FEATURE_SMP_VDEV_NOT_SUPPORT
				EXTRA_CFLAGS += -DCFG_FEATURE_DDR
			endif
			ifneq ($(filter $(PRODUCT),  ascend910B),)
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
				EXTRA_CFLAGS += -DCFG_FEATURE_CHIP_DIE
				EXTRA_CFLAGS += -DCFG_FEATURE_PCIE_BBOX_ADDR
				EXTRA_CFLAGS += -DCFG_FEATURE_VF_USE_DEVID
				EXTRA_CFLAGS += -DCFG_FEATURE_VDEV_MEMORY_FROM_DEVMNG_H2D
				EXTRA_CFLAGS += -DCFG_FEATURE_BIND_TGID
				EXTRA_CFLAGS += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
				EXTRA_CFLAGS += -DCFG_FEATURE_BBOX_KDUMP
				EXTRA_CFLAGS += -DCFG_FEATURE_ADMIN_CONTAINER_PID
			endif

			EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_common
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl ascend310Bemu ascend610 ascend610Lite ascend610Liteesl bs9sx1a ascend310p),)
				ifeq ($(PRODUCT), ascend310p)
					EXTRA_CFLAGS += -DCFG_FEATURE_VASCEND
					EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
					EXTRA_CFLAGS += -DCFG_FEATURE_VFG
					EXTRA_CFLAGS += -DCFG_FEATURE_NOTIFY_REBOOT
					EXTRA_CFLAGS += -DCFG_FEATURE_DEVICE_SHARE
					EXTRA_CFLAGS += -DCFG_FEATURE_CHIP_REG_FORCED_EXPORT
				endif

				ifneq ($(findstring $(PRODUCT), ascend310B ascend310Besl ascend310Bemu),)
					EXTRA_CFLAGS += -DCFG_FEATURE_PCIE_BBOX_ADDR
				endif
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_DDR
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_common
			else
				EXTRA_CFLAGS += -I$(TOP_DIR)/abl/bbox/inc/bbox/device
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend310
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend310
			endif
		endif

	endif # ifeq ($(DAVINCI_HIAI_DKMS),y)

	EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
	EXTRA_CFLAGS += -DCFG_SURPPORT_PCIE_HOST_DEVICE_COMM
	ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
		EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
	endif

	ifneq ($(filter $(PRODUCT),  ascend910 ascend910B),)
		BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend910
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pcie.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_init.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_cmd_proc.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_pid_map.o
		drv_devmng_host-y += drv_devmng/drv_devmng_common/dev_mnt_vdevice.o

		ifneq ($(filter $(PRODUCT),  ascend910B),)
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_chip_dev_map.o
		endif
	else ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl ascend310Bemu ascend610 ascend610Lite ascend610Liteesl ascend610bs9sx1a ascend310p),)
		BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend910
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pcie.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_init.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/hvdevmng_cmd_proc.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_pid_map.o
		drv_devmng_host-y += drv_devmng/drv_devmng_common/dev_mnt_vdevice.o
		ifeq ($(PRODUCT), ascend310p)
			drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_dev_share.o
		endif
	else
		BUILD_PREFIX_DIR := drv_devmng/drv_devmng_host/ascend310
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_host_drvops.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_nsm.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_container_noplugin.o
		drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_dev_share.o
	endif

	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_msg.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_pm.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_ipc_msg.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_driver_pm.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_rand.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_black_box.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_manager_container.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/devdrv_device_online.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/drv_log.o
	drv_devmng_host-y += $(BUILD_PREFIX_DIR)/tsdrv_status.o

	ifeq ($(PRODUCT),ascend910)
		ccflags-y += -Wall -Werror
	endif

	ifneq ($(filter $(PRODUCT),ascend910 ascend310p),)
		EXTRA_CFLAGS += -DCFG_FEATURE_DMS_SVM_DEV
	endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

ifeq ($(TARGET_BUILD_TYPE),debug)
        ccflags-y += -DCFG_BUILD_DEBUG
endif

ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) # for DKMS compile
		ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_driver_host
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/devdrv_manager_host
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host
		ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include/
		ifeq ($(TARGET_PRODUCT),cloud)
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
			ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/cloud
			ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_common
			ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
			ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else ifeq ($(TARGET_PRODUCT),mini)
			ccflags-y += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_FEATURE_VFG
				ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_common
				ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		else
			ccflags-y += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		endif
	else # for reserve
		CUR_MAKEFILE_PATH := $(strip \
				$(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
				$(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
				)

		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../inc/driver
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../tsdrv/ts_drv/ts_drv_common/
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../drv_devmng_host/ascend910
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../dev_core/device_manager/device_manager_inc/
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../abl/libc_sec/include/

		ifeq ($(TARGET_PRODUCT),cloud)
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
			ccflags-y += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_device/ascend910
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else
			ccflags-y += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_FEATURE_VFG
				ccflags-y += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend610
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend610
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				ccflags-y += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend610
			else
				ccflags-y += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend310
			endif
		endif
	endif
	# for DKMS compile
ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif
	ccflags-y += -DCFG_FEATURE_SHARE_LOG
	ccflags-y += -DCFG_FEATURE_HOST_LOG

	BUILD_VDEVMNG_DIR := drv_devmng/drv_devmng_vm

	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/vdevmng_init.o
	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/vdevmng_agent.o
	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/vdevmng_cmd_proc.o
	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/drv_log.o

	ifeq ($(TARGET_PRODUCT),cloud)
		ccflags-y += -Wall -Werror
	endif

else # for CMake compile

	ifeq ($(DAVINCI_HIAI_DKMS),y) # for reserve
		ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_driver_host
		ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_manager_host
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host
		ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include/
		ifeq ($(TARGET_PRODUCT),cloud)
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
			ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/cloud
			ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_common
			ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
			ccflags-y += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend910
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else ifeq ($(TARGET_PRODUCT),mini)
			ccflags-y += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_FEATURE_VFG
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		else
			ccflags-y += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				ccflags-y += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		endif
	else # for CMake compile
		ifeq ($(PRODUCT),bs9sx1a)
			ccflags-y += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
		endif
		ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
		ccflags-y += -I$(TOP_DIR)/inc/driver
		ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910/
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event/
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include/
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor/
		ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include/

		ifeq ($(PRODUCT),ascend910)
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
			ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else
			ccflags-y += -DCFG_SOC_PLATFORM_MINI
			ifneq ($(filter $(PRODUCT),  ascend610 ascend610Lite ascend610Liteesl ascend610bs9sx1a ascend310p),)
				ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
				ccflags-y += -DCFG_FEATURE_VFG
				ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend610
			else
				ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend310
			endif
		endif
	endif
	# for CMake compile

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif
	ccflags-y += -DCFG_FEATURE_SHARE_LOG
	ccflags-y += -DCFG_FEATURE_HOST_LOG
	BUILD_VDEVMNG_DIR := drv_devmng/drv_devmng_vm

	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/vdevmng_init.o
	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/vdevmng_agent.o
	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/vdevmng_cmd_proc.o
	drv_vdevmng-y += $(BUILD_VDEVMNG_DIR)/drv_log.o

	ifeq ($(PRODUCT),ascend910)
		ccflags-y += -Wall -Werror
	endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

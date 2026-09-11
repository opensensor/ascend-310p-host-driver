ifeq ($(TARGET_BUILD_TYPE),debug)
        EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) # for DKMS compile
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/npu_inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_driver_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/devdrv_manager_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include/
		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/cloud
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else ifeq ($(TARGET_PRODUCT),mini)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFG
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		endif
	else # for reserve
		CUR_MAKEFILE_PATH := $(strip \
				$(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
				$(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
				)

		EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../inc/driver
		EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../tsdrv/ts_drv/ts_drv_common/
		EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../drv_devmng_host/ascend910
		EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../dev_core/device_manager/device_manager_inc/
		EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../abl/libc_sec/include/

		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_device/ascend910
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFG
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend610
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend610
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend610
			else
				EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../tsdrv/ts_platform/ts_platform_host/ascend310
			endif
		endif
	endif
	# for DKMS compile
ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif
	EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
	EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG

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
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/npu_inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_driver_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_manager_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include/
		ifeq ($(TARGET_PRODUCT),cloud)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/cloud
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_host/ascend910
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else ifeq ($(TARGET_PRODUCT),mini)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFG
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/miniv2
			else
				EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/devdrv_platform_host/mini
			endif
		endif
	else # for CMake compile
		ifeq ($(PRODUCT),bs9sx1a)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_BS9SX1A
		endif
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor/
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include/

		ifeq ($(PRODUCT),ascend910)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
		else
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
			ifneq ($(filter $(PRODUCT),  ascend610 ascend610Lite ascend610Liteesl ascend610bs9sx1a ascend310p),)
				EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
				EXTRA_CFLAGS += -DCFG_FEATURE_VFG
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend610
			else
				EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend310
			endif
		endif
	endif
	# for CMake compile

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif
	EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
	EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
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

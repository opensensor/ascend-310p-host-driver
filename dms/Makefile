ifeq ($(TARGET_TYPE), ascend610Lite)
	MODULE_NAME = drv_devmng_v3
else
	MODULE_NAME = drv_devmng
endif
ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		include $(FEATURE_MK_PATH)
		ccflags-y += $(CONFIG_DEFINES)

		ifneq ($(NOT_SUPPORT_SP), y)
			ccflags-y += -fstack-protector-all
		endif

		ccflags-y += -Wall -Wno-missing-prototypes -Wno-missing-declarations
		ccflags-y += -Wno-array-bounds
		ifeq ($(Driver_Install_Mode),vnpu_guest)
			ifeq ($(TARGET_CHIP_ID), hi1980b)
				obj-m += drv_devmng_host.o
				include $(HIAI_DKMS_DIR)/dms/time/dms_time.mk
				include $(HIAI_DKMS_DIR)/dms/urd_forward/dms_urd_forward.mk
				include $(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/drv_devmng_host.mk
				include $(HIAI_DKMS_DIR)/dms/adapt/dms_adapt.mk
				include $(HIAI_DKMS_DIR)/dms/core/dms_core.mk
				include $(HIAI_DKMS_DIR)/dms/custom/dms_custom.mk
				include $(HIAI_DKMS_DIR)/dms/devid/dms_devid.mk
				include $(HIAI_DKMS_DIR)/dms/event/dms_event_adapt.mk
				include $(HIAI_DKMS_DIR)/dms/power/dms_power.mk
				#include $(HIAI_DKMS_DIR)/dms/smf/sensor/dms_sensor.mk
				include $(HIAI_DKMS_DIR)/dms/status/dms_status.mk
				include $(HIAI_DKMS_DIR)/dms/config/dms_config.mk
				include $(HIAI_DKMS_DIR)/dms/product/dms_product.mk
				include $(HIAI_DKMS_DIR)/dms/xpc/dms_xpc.mk
				include $(HIAI_DKMS_DIR)/dms/bbox/dms_bbox_log_dump.mk
			else
				obj-m += drv_vdevmng.o
				include $(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_vm/drv_vdevmng.mk
			endif
		else
			obj-m += drv_devmng_host.o
			include $(HIAI_DKMS_DIR)/dms/time/dms_time.mk
			include $(HIAI_DKMS_DIR)/dms/urd_forward/dms_urd_forward.mk
			include $(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/drv_devmng_host.mk
			include $(HIAI_DKMS_DIR)/dms/adapt/dms_adapt.mk
			include $(HIAI_DKMS_DIR)/dms/core/dms_core.mk
			include $(HIAI_DKMS_DIR)/dms/custom/dms_custom.mk
			include $(HIAI_DKMS_DIR)/dms/devid/dms_devid.mk
			include $(HIAI_DKMS_DIR)/dms/event/dms_event_adapt.mk
			include $(HIAI_DKMS_DIR)/dms/power/dms_power.mk
			#include $(HIAI_DKMS_DIR)/dms/smf/sensor/dms_sensor.mk
			include $(HIAI_DKMS_DIR)/dms/status/dms_status.mk
			include $(HIAI_DKMS_DIR)/dms/config/dms_config.mk
			include $(HIAI_DKMS_DIR)/dms/product/dms_product.mk
			include $(HIAI_DKMS_DIR)/dms/xpc/dms_xpc.mk
			include $(HIAI_DKMS_DIR)/dms/bbox/dms_bbox_log_dump.mk
			ifeq ($(TARGET_CHIP_ID), hi1980b)
				include $(HIAI_DKMS_DIR)/dms/hccs/dms_hccs.mk
				drv_devmng_host-y += drv_devmng/drv_devmng_common/devdrv_spod_info.o
				drv_devmng_host-y += status/dms_spod_info.o
			endif
		endif
		ccflags-y += -DCFG_HOST_ENV
		ccflags-y += -DCFG_FEATURE_HEART_BEAT
		ccflags-y += -DCFG_FEATURE_HEALTH_ERR_CODE
		ccflags-y += -DCFG_FEATURE_DEV_TOPOLOGY
		ccflags-y += -DCFG_SURPPORT_PCIE_HOST_DEVICE_COMM
		ccflags-y += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=3
		ccflags-y += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0

		ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
		ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms
		ccflags-y += -I$(HIAI_DKMS_DIR)/../inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/sensor
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/dtm
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/ipc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/lpm
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/smf/event
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/event
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/power
		ccflags-y += -I$(HIAI_DKMS_DIR)/src/dms/xpc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/dev_urd
		ccflags-y += -I$(HIAI_DKMS_DIR)/common
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/status
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/bbox
		ccflags-y += -I$(TOP_DIR)/drivers/firmware/hiss/inc
		ifeq ($(TARGET_CHIP_ID), hi1980b)
			ccflags-y += -DCFG_FEATURE_UDIS
			ccflags-y += -DCFG_FEATURE_PG
			ccflags-y += -DCFG_FEATURE_SRIOV
			ccflags-y += -DCFG_FEATURE_VFG
			ccflags-y += -DCFG_FEATURE_HOST_AICPU
			ccflags-y += -DCFG_FEATURE_NEW_EVENT_CODE
			ccflags-y += -DCFG_CHIP_INFO_FROM_DEVINFO
			ccflags-y += -DCFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
			ccflags-y += -DCFG_TRS_HB_REFACTOR_FEATURE
			ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
			ccflags-y += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
		else
			ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_SMP
		endif

		ifeq ($(TARGET_CHIP_ID), hi1910b)
			ccflags-y += -DCFG_FEATURE_PG
			ccflags-y += -DCFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
			ccflags-y += -DCFG_TRS_HB_REFACTOR_FEATURE
			ccflags-y += -DCFG_FEATURE_NEW_EVENT_CODE
		endif

		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			ccflags-y += -DCFG_CHIP_INFO_FROM_VMNG
			ccflags-y += -DCFG_FEATURE_OSC_FREQ
			ccflags-y += -DCFG_FEATURE_OLD_ALL_DEVICE_RESET_FLAG
			ccflags-y += -DCFG_FEATURE_KA
			ifneq ($(filter $(PRODUCT),  helper310p),)
			else
				include $(HIAI_DKMS_DIR)/dms/log/dms_log_info.mk
			endif
		endif

		ifeq ($(TARGET_CHIP_ID), hi1980)
			ccflags-y += -DCFG_CHIP_INFO_FROM_VMNG
			ccflags-y += -DCFG_FEATURE_OLD_ALL_DEVICE_RESET_FLAG
		endif

		ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
			ccflags-y += -DCFG_FEATURE_OSC_FREQ
			ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
			include $(HIAI_DKMS_DIR)/dms/log/dms_log_info.mk
		endif

		ifeq ($(TARGET_CHIP_ID), hi1910)
			ccflags-y += -DCFG_FEATURE_OLD_ALL_DEVICE_RESET_FLAG
			ccflags-y += -DCFG_FEATURE_UNSUPPORT_FAULT_MANAGE
			ccflags-y += -DCFG_FEATURE_UNSUPPORT_BASIC_INFO
		endif
		ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
			ccflags-y += -DCFG_FEATURE_KA
		endif
	endif
else #for CMake & ctrl cpu open
	include $(FEATURE_MK_PATH)
	ccflags-y += $(CONFIG_DEFINES)
	ifneq ($(NOT_SUPPORT_SP), y)
		ccflags-y += -fstack-protector-all
	endif

	ifeq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl as31xm1 ascend310Brc helper310p),)
		ccflags-y += -DCFG_SURPPORT_PCIE_HOST_DEVICE_COMM
	endif

	ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
		ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51
		ccflags-y += -DCFG_FEATURE_IPC_CRC
		ccflags-y += -DCFG_FEATURE_PARTIAL_GOOD
		ccflags-y += -DDRV_SUPPORT_SYSCOUNT
	endif
	ifeq ($(TARGET_TYPE), ascend610Lite)
		ccflags-y += -DCFG_SOC_PLATFORM_MDC_LITE_ESL
		ccflags-y += -DCFG_SOC_MDC_V51_LITE
	endif
	ifneq ($(filter $(PRODUCT), ascend610Lite ascend610Liteesl ascend610 bs9sx1a as31xm1),)
		ccflags-y += -DCFG_FEATURE_REBOOT_REASON
	endif
	ifneq ($(filter $(PRODUCT), as31xm1),)
		ccflags-y += -DCFG_SOC_PLATFORM_MDC_V11
	endif
	ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend310Brc ascend910 ascend910B),)
		ccflags-y += -DCFG_FEATURE_KA
	endif

	ccflags-y += -Wall -funsigned-char -Wextra -Werror -Wformat=2 -Wfloat-equal -Wcast-align -Wundef $(WDATE_TIME)
	ccflags-y += -Wno-unused-parameter -Wno-sign-compare  -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body -Wno-missing-prototypes -Wno-missing-declarations
	ccflags-y += -Wno-array-bounds

	ifeq ($(PRODUCT_SIDE), host)
		ccflags-y += -DCFG_HOST_ENV
		ccflags-y += -DCFG_FEATURE_DEV_TOPOLOGY
		obj-m += drv_devmng_host.o
		include ${DRIVER_KERNEL_DIR}/src/dms/drv_devmng/drv_devmng_host/drv_devmng_host.mk
		include ${DRIVER_KERNEL_DIR}/src/dms/power/dms_power.mk
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/power

		ifneq ($(filter $(PRODUCT), ascend910 ascend310p),)
			ccflags-y += -DCFG_CHIP_INFO_FROM_VMNG
			ccflags-y += -DCFG_FEATURE_OLD_ALL_DEVICE_RESET_FLAG
			obj-m += drv_vdevmng.o
			include ${DRIVER_KERNEL_DIR}/src/dms/drv_devmng/drv_devmng_vm/drv_vdevmng.mk
		endif
		ifneq ($(filter $(PRODUCT), ascend910B),)
			ccflags-y += -DCFG_FEATURE_HOST_AICPU
			ccflags-y += -DCFG_CHIP_INFO_FROM_DEVINFO
			ccflags-y += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
			include ${DRIVER_KERNEL_DIR}/src/dms/hccs/dms_hccs.mk
		endif
		ifneq ($(filter $(PRODUCT), ascend310p ascend310B ascend910 ascend910B),)
			ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
			ccflags-y += -DCFG_FEATURE_OSC_FREQ
			include $(DRIVER_KERNEL_DIR)/src/dms/log/dms_log_info.mk
		endif
		ifneq ($(filter $(PRODUCT), ascend310),)
			ccflags-y += -DCFG_FEATURE_OLD_ALL_DEVICE_RESET_FLAG
		endif
	else
		obj-m += $(MODULE_NAME).o
		include ${DRIVER_KERNEL_DIR}/src/dms/drv_devmng/drv_devmng_device/drv_devmng.mk
		include ${DRIVER_KERNEL_DIR}/src/dms/ipc/dms_ipc.mk
		include ${DRIVER_KERNEL_DIR}/src/dms/lpm/dms_lpm.mk
		include ${DRIVER_KERNEL_DIR}/src/dms/ts/dms_ts.mk
		include ${DRIVER_KERNEL_DIR}/src/dms/mem/dms_mem_info.mk
		include ${DRIVER_KERNEL_DIR}/src/dms/power/dms_power.mk
		ifneq ($(filter $(PRODUCT), ascend310p ascend910 ascend910B),)
			ccflags-y += -DSUPPORT_VDEV_MEM
		endif
		ifneq ($(filter $(PRODUCT), ascend310p helper310p ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
			ccflags-y += -DCFG_FEATURE_AIC_AIV_UTIL_FROM_TS
		endif

		ifneq ($(filter $(PRODUCT), ascend910B),)
			include ${DRIVER_KERNEL_DIR}/src/dms/hccs/dms_hccs.mk
			include ${DRIVER_KERNEL_DIR}/src/dms/sio/dms_sio.mk
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/hccs
			ccflags-y += -DCFG_FEATURE_AIC_AIV_UTIL_FROM_TS
			ccflags-y += -DCFG_FEATURE_IPC_FORMAT_V2
			ccflags-y += -DCFG_FEATURE_HCCS
			ccflags-y += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
			ccflags-y += -DCFG_FEATURE_SUPPORT_NPU_UTIL
		endif
		ifneq ($(filter $(PRODUCT), ascend310p),)
			include ${DRIVER_KERNEL_DIR}/src/dms/hccs/dms_hccs.mk
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/hccs
			ccflags-y += -DCFG_FEATURE_HCCS
		endif
		ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310Besl ascend310Brcesl ascend310Bemu as31xm1),)
			ccflags-y += -DCFG_FEATURE_AIC_AIV_UTIL_FROM_TS
			ccflags-y += -DCFG_FEATURE_IPC_FORMAT_V2
		endif
		ifneq ($(filter $(PRODUCT), ascend310Brc),)
			ccflags-y += -DCFG_CHIP_INFO_FROM_VMNG
		endif
		ifneq ($(filter $(PRODUCT), ascend910),)
			ccflags-y += -DCFG_FEATURE_AI_UTIL_BY_CALCULATE
			ccflags-y += -DCFG_FEATURE_TS_FAULT_MASK
		endif
		ifneq ($(filter $(PRODUCT), ascend310 ascend310rc),)
			ccflags-y += -DCFG_FEATURE_AI_UTIL_FROM_IPC
		endif
		ifneq ($(filter $(PRODUCT), helper310p ascend310Brc ascend610 ascend610Lite ascend610Liteesl as31xm1),)
			ccflags-y += -DCFG_FEATURE_OSC_FREQ
		endif
		ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend310Brc ascend910 ascend910B),)
			ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
		endif
		ifneq ($(filter $(PRODUCT), ascend610Lite as31xm1),)
			include ${DRIVER_KERNEL_DIR}/src/dms/flash/dms_flash.mk
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/flash
        endif
	endif
	include ${DRIVER_KERNEL_DIR}/src/dms/adapt/dms_adapt.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/core/dms_core.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/custom/dms_custom.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/devid/dms_devid.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/event/dms_event_adapt.mk
	#include ${DRIVER_KERNEL_DIR}/src/dms/smf/sensor/dms_sensor.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/status/dms_status.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/config/dms_config.mk
	include $(DRIVER_KERNEL_DIR)/src/dms/product/dms_product.mk
	include $(DRIVER_KERNEL_DIR)/src/dms/xpc/dms_xpc.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/time/dms_time.mk
	include ${DRIVER_KERNEL_DIR}/src/dms/bbox/dms_bbox_log_dump.mk
	include $(DRIVER_KERNEL_DIR)/src/dms/urd_forward/dms_urd_forward.mk

	ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
	ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/dev_urd
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/event
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor/config
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/xpc
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/common
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/soft_fault
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/bbox
	ccflags-y += -I$(TOP_DIR)/inc/driver
	ccflags-y += -I$(TOP_DIR)/drivers/firmware/hiss/inc
	ccflags-y += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=3
	ccflags-y += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0

	ifneq ($(filter $(PRODUCT),  ascend310 ascend310p ascend910 ascend910B ascend310B),)
		ccflags-y += -DCFG_FEATURE_HEART_BEAT
		ccflags-y += -DCFG_FEATURE_HEALTH_ERR_CODE
	endif
	ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend910B as31xm1),)
		ccflags-y += -DCFG_FEATURE_NEW_EVENT_CODE
	endif

	ifneq ($(filter $(PRODUCT),  ascend310B ascend310Brc ascend910B),)
		ccflags-y += -DCFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
		ccflags-y += -DCFG_TRS_HB_REFACTOR_FEATURE
	endif

    ifneq ($(filter $(PRODUCT), ascend910 ascend310 ascend310p ascend310B ascend310Besl ascend310Bemu), )
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_SMP
    endif
    ifneq ($(filter $(PRODUCT), ascend910B), )
        ccflags-y += -DCFG_FEATURE_UDIS
        ccflags-y += -DCFG_FEATURE_PG
        ccflags-y += -DCFG_FEATURE_VFG
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
        ccflags-y += -DCFG_FEATURE_GET_CURRENT_EVENTINFO
        drv_devmng_host-y += drv_devmng/drv_devmng_common/devdrv_spod_info.o
        drv_devmng_host-y += status/dms_spod_info.o
        drv_devmng-y += drv_devmng/drv_devmng_common/devdrv_spod_info.o
        drv_devmng-y += status/dms_spod_info.o
    endif
	ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl ascend310Brc ascend310Brcesl as31xm1), )
		ccflags-y += -DCFG_FEATURE_PG
		ifeq ($(PRODUCT),as31xm1)
			ccflags-y += -DCFG_SOC_PLATFORM_MDC_V11
		endif
	endif
    ifneq ($(filter $(PRODUCT), ascend310Besl),)
        ccflags-y += -DCFG_FEATURE_HEALTH_ERR_CODE
    endif
    # support device container
    ifneq ($(filter $(PRODUCT), ascend310rc helper310p ascend310Brc ascend310Brcesl ascend310Brcemu),)
        ccflags-y += -DCFG_FEATURE_DEVICE_CONTAINER
    endif
    ifneq ($(filter $(PRODUCT), ascend310Besl ascend310Brcesl),)
        ccflags-y += -DCFG_ENV_ESL
    endif

    ifneq ($(filter $(PRODUCT), ascend310 ascend310rc),)
        ccflags-y += -DCFG_FEATURE_UNSUPPORT_FAULT_MANAGE
        ccflags-y += -DCFG_FEATURE_UNSUPPORT_BASIC_INFO
    endif

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif

endif #end for CMake
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

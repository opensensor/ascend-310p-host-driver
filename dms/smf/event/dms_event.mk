ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		ascend_dms_smf-y += event/dms_event.o
		ascend_dms_smf-y += event/dms_event_converge.o
		ascend_dms_smf-y += event/dms_event_distribute.o
		ascend_dms_smf-y += event/dms_event_distribute_proc.o
		ascend_dms_smf-y += event/dms_event_dfx.o
		ascend_dms_smf-y += event/smf_event_adapt.o

		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
		EXTRA_CFLAGS += -I${HIAI_DKMS_DIR}/ts_drv_common
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/event
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend910
		EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/src/tsdrv/ts_drv/ts_drv_common
	else
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_device/ascend910
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/bbox/inc/bbox
		ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a ascend610Lite ascend610Liteesl),)
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/mdc/ts_platform/ts_platform_device/ascend610
			EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/src/tsdrv/mdc/ts_drv/ts_drv_common
		else
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_device/ascend910
			EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/src/tsdrv/ts_drv/ts_drv_common
		endif
	endif
		 ascend_dms_smf-y += event/dms_event.o
		 ascend_dms_smf-y += event/dms_event_converge.o
		 ascend_dms_smf-y += event/dms_event_distribute.o
		 ascend_dms_smf-y += event/dms_event_distribute_proc.o
		 ascend_dms_smf-y += event/dms_event_dfx.o
		 ascend_dms_smf-y += event/smf_event_adapt.o
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dms/drv_devmng/drv_devmng_inc
endif

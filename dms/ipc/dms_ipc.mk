ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
	endif
else #for CMake
	ifeq ($(PRODUCT_SIDE), host)
	else
		$(MODULE_NAME)-y += ipc/dms_ipc_common.o

		ifneq ($(filter $(PRODUCT), ascend910),)
			EXTRA_CFLAGS += -DCONFIG_IPC_COMM_FROM_IMU_V1
		endif
		ifneq ($(filter $(PRODUCT), ascend610 ascend610Lite ascend610Liteesl bs9sx1a ascend310p helper310p),)
			EXTRA_CFLAGS += -DCONFIG_IPC_COMM_FROM_LP
		endif
		ifneq ($(filter $(PRODUCT), ascend310p),)
			EXTRA_CFLAGS += -DCFG_EDP_MSG
		endif
		ifneq ($(filter $(PRODUCT), ascend310 ascend310rc),)
			EXTRA_CFLAGS += -DCONFIG_IPC_COMM_FROM_LPM
		endif
		ifneq ($(filter $(PRODUCT), ascend910B),)
			EXTRA_CFLAGS += -DCONFIG_IPC_COMM_FROM_IMU_V2
		endif
		ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl ascend310Bemu ascend310Brc ascend310Brcesl ascend310Brcemu as31xm1),)
			EXTRA_CFLAGS += -DCONFIG_IPC_COMM_FROM_IMU_STUB
		endif
	endif
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/ipc
endif

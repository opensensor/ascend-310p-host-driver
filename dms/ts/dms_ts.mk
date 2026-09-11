ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
	else
		$(MODULE_NAME)-y += ts/dms_ts_info.o
		ifneq ($(filter $(PRODUCT), ascend910 ascend310 ascend310rc),)
			$(MODULE_NAME)-y += ts/dms_ts_scan.o
		endif
		ifneq ($(filter $(PRODUCT), ascend910B),)
			$(MODULE_NAME)-y += ts/dms_ts_npu_utilization.o
		endif
	endif
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/ts
	ccflags-y += -DCFG_FEATURE_TS
endif
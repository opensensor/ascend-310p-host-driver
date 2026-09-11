
ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifeq ($(ENABLE_ASAN),true)
    EXTRA_CFLAGS += -DCFG_ENABLE_ASAN
endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -Werror

ifeq ($(TOP_DIR),)

	ifeq ($(DAVINCI_HIAI_DKMS),y)
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dp_proc_mng/
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dp_proc_mng/common
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dp_proc_mng/pmaster
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dp_proc_mng/pmaster/comm
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dp_proc_mng/pmaster/common
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/memory
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

		EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG

        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif

        ifeq ($(TARGET_CHIP_ID), hi1980b)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
	    endif

        ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
	
		drv_dp_proc_mng_host-objs += dp_proc_mng_msg_client.o common/dp_proc_mng_module_ops.o comm/dp_proc_mng_proc_info.o comm/dp_proc_mng_master_register_ops.o comm/dp_proc_mng_channel.o common/dp_proc_mng_pci_dev_tbl.o
		drv_dp_proc_mng_host-objs += comm/dp_proc_mng_mem_sharing.o
	else
        	ccflags-y += -Wtrampolines -Wfloat-equal -Wvla -Wundef
	endif

	obj-m := drv_dp_proc_mng_host.o
else
	ifeq ($(DAVINCI_HIAI_DKMS),y)

	else
		ccflags-y += -Wtrampolines -Wfloat-equal -Wvla -Wundef
        EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
		EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
		EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/memory
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dp_proc_mng/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dp_proc_mng/master
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dp_proc_mng/common
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dp_proc_mng/master/comm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dp_proc_mng/master/pmaster
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dp_proc_mng/master/pmaster/dppg_comm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dp_proc_mng/master/pmaster/master_comm

		EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
        ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif

		ifeq ($(PRODUCT), ascend910B)
    		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
		endif

		drv_dp_proc_mng_host-objs += dp_proc_mng_msg_client.o dppg_comm/dp_proc_mng_module_ops.o master_comm/dp_proc_mng_proc_info.o master_comm/dp_proc_mng_master_register_ops.o master_comm/dp_proc_mng_channel.o dppg_comm/dp_proc_mng_pci_dev_tbl.o
		drv_dp_proc_mng_host-objs += master_comm/dp_proc_mng_mem_sharing.o
	endif

	obj-m := drv_dp_proc_mng_host.o
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

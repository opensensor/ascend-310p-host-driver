ccflags-y += -Wno-missing-prototypes -Wno-missing-declarations
ifeq ($(TARGET_BUILD_TYPE),debug)
	ccflags-y += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
	ccflags-y += -fstack-protector-all
endif
ccflags-y += -Wall -Werror -Wno-missing-prototypes -Wno-missing-declarations -Wno-missing-declarations -Wno-incompatible-pointer-types

ifeq ($(TOP_DIR),)
    ccflags-y += -DCFG_MANAGER_HOST_ENV
    ccflags-y += -DCFG_FEATURE_SHARE_LOG
    ccflags-y += -DCFG_TSDRV_VM_ENV
    ccflags-y += -DCFG_FEATURE_VM_GRACEFUL_RELEASE
	ccflags-y += -DCFG_FEATURE_HOST_LOG
	ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    CFG_FEATURE_RUNTIME_NO_THREAD := n
	ifeq ($(DAVINCI_HIAI_DKMS),y)
		ifeq ($(TARGET_PRODUCT),cloud)
			ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
		else ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
		    CFG_FEATURE_RUNTIME_NO_THREAD := y
		    ccflags-y += -DCFG_FEATURE_RUNTIME_NO_THREAD
			ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
		endif
		ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
		ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm
		ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_virt
		ccflags-y += -I$(HIAI_DKMS_DIR)/drv_devmng_inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common/tsdrv_dev
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common/tsdrv_id
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common/tsdrv_nvme
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common/tsdrv_nvme/logic
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common/tsdrv_sqcq_dfx
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common/tsdrv_nvme/shm
		ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm/vm_common/ts_drv_common/devdrv_callback
		ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

		obj-m += drv_vtsdrv.o
		drv_vtsdrv-objs := vtsdrv_init.o vtsdrv_proc.o vtsdrv_sqcq.o vtsdrv_cb.o vtsdrv_logic_cq.o vtsdrv_stub.o

		drv_vtsdrv-$(CFG_FEATURE_RUNTIME_NO_THREAD) += vtsdrv_shm_sq.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/devdrv_callback/devdrv_cb.o vm_common/ts_drv_common/devdrv_callback/devdrv_cbctrl.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_dev/tsdrv_hwinfo.o vm_common/ts_drv_common/tsdrv_dev/tsdrv_device.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_dev/tsdrv_ctx.o vm_common/ts_drv_common/tsdrv_dev/tsdrv_dev_dfx.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_dev/tsdrv_capacity.o vm_common/ts_drv_common/tsdrv_dev/tsdrv_id_dfx.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_dev/tsdrv_mailbox_dfx.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_nvme/logic/logic_cq.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_dev/tsdrv_ctx_dfx.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_dev/tsdrv_id_config_dfx.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_dev/tsdrv_event_id_dfx.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_sqcq_dfx/tsdrv_sqcq_timestamp.o
		drv_vtsdrv-objs += vm_common/ts_drv_common/tsdrv_id/tsdrv_id.o
	else
		CUR_MAKEFILE_PATH := $(strip \
				$(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
				$(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
				)

		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/inc
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/inc/dbl
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_vm
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../../abl/libc_sec/include
		ccflags-y += -I$(CUR_MAKEFILE_PATH)/../../../../../inc/driver
		obj-m += drv_vtsdrv.o
		drv_vtsdrv-objs := vtsdrv_init.o vtsdrv_proc.o vtsdrv_sqcq.o

                ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
                ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
	endif
else
    CFG_FEATURE_RUNTIME_NO_THREAD := y
    ccflags-y += -DCFG_FEATURE_VM_GRACEFUL_RELEASE
	ccflags-y += -DCFG_MANAGER_HOST_ENV
	ccflags-y += -DCFG_FEATURE_SHARE_LOG
	ccflags-y += -DCFG_TSDRV_VM_ENV
	ccflags-y += -DCFG_FEATURE_HOST_LOG
	ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
	ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
	ifeq ($(DAVINCI_HIAI_DKMS),y)
		ifeq ($(PRODUCT),ascend910)
			CFG_FEATURE_RUNTIME_NO_THREAD := n
			ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
        else  ifeq ($(PRODUCT),ascend310p)
            ccflags-y += -DCFG_FEATURE_RUNTIME_NO_THREAD
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
		endif
		ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
		ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_vm
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_id
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme/logic
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_sqcq_dfx
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme/shm
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/devdrv_callback
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/tsdrv_virt
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_core/device_manager/device_manager_inc
		ccflags-y += -I$(HIAI_DKMS_DIR)/drivers/dev_platform/drv_devmng/drv_devmng_host/ascend910
	else
		ifeq ($(PRODUCT),ascend910)
		    CFG_FEATURE_RUNTIME_NO_THREAD := n
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend910
		else  ifeq ($(PRODUCT),ascend310p)
            ccflags-y += -DCFG_FEATURE_RUNTIME_NO_THREAD
			ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend610
		endif
		ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
		ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
		ccflags-y += -I$(TOP_DIR)/inc/driver
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_vm
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_id
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme/logic
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme/shm
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/devdrv_callback
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_virt
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_sqcq_dfx
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
		ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
	endif

	obj-m += drv_vtsdrv.o
	drv_vtsdrv-objs := vtsdrv_init.o vtsdrv_proc.o vtsdrv_sqcq.o vtsdrv_cb.o vtsdrv_logic_cq.o vtsdrv_stub.o
    drv_vtsdrv-$(CFG_FEATURE_RUNTIME_NO_THREAD) += vtsdrv_shm_sq.o
	drv_vtsdrv-objs += ts_drv_common/devdrv_callback/devdrv_cb.o ts_drv_common/devdrv_callback/devdrv_cbctrl.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_dev/tsdrv_hwinfo.o ts_drv_common/tsdrv_dev/tsdrv_device.o ts_drv_common/tsdrv_dev/tsdrv_ctx.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_dev/tsdrv_dev_dfx.o ts_drv_common/tsdrv_dev/tsdrv_capacity.o ts_drv_common/tsdrv_dev/tsdrv_id_dfx.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_dev/tsdrv_mailbox_dfx.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_nvme/logic/logic_cq.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_dev/tsdrv_ctx_dfx.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_dev/tsdrv_event_id_dfx.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_dev/tsdrv_id_config_dfx.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_id/tsdrv_id.o
	drv_vtsdrv-objs += ts_drv_common/tsdrv_sqcq_dfx/tsdrv_sqcq_timestamp.o

	ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char  -Wformat=2 -Wstack-usage=2048 -Wcast-align
        ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

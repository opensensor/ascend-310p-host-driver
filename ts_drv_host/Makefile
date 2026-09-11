ccflags-y += -Wno-missing-prototypes -Wno-missing-declarations
ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -Werror

ifeq ($(TOP_DIR),)
CUR_MAKEFILE_PATH := $(strip \
                     $(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
                     $(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
                     )
TOPDIR_ABS := $(realpath $(CUR_MAKEFILE_PATH)/../../../)
EXTRA_CFLAGS += -DCFG_MANAGER_HOST_ENV
EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_IPC_NOTIFY
EXTRA_CFLAGS += -DCFG_FEATURE_HOTRESET
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
ifeq ($(DAVINCI_HIAI_DKMS),y)

	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/npu_inc
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/drv_devmng_inc
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_osal
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_sqcq_dfx
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/include
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_hotreset
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/sensor
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/sensor/config
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/config
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/event
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
	ifeq ($(TARGET_PRODUCT),mini)
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/devdrv_callback
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_dev
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_id
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_virt
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_nvme
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_nvme/shm
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_nvme/logic
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_vm
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_conflict_check
		else
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/devdrv_callback
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_vm
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_dev/
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_id
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_virt
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_nvme
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_nvme/shm
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_nvme/logic
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_nvme/sync_cb
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_conflict_check
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend310
		endif
	else
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/devdrv_callback
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_dev
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_id
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_virt
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_nvme
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_nvme/shm
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_nvme/logic
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_vm
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_conflict_check
	endif

	ifeq ($(TARGET_PRODUCT),cloud)
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/soc/cloud_v1
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_conflict_check/ascend910
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
		EXTRA_CFLAGS += -DCFG_FEATURE_USE_MAILBOX_RECYCLE
		EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
		EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE

		ifeq ($(TARGET_CHIP_ID), hi1980)
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
		endif

		ifeq ($(TARGET_CHIP_ID), hi1980b)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
			EXTRA_CFLAGS += -DCFG_FEATURE_CDQM
			EXTRA_CFLAGS += -DCFG_FEATURE_CALLBACK_EVENT
			EXTRA_CFLAGS += -DCFG_FEATURE_SQ_HOST_MEM
			EXTRA_CFLAGS += -DCFG_FEATURE_CQ_HOST_MEM
			EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_VSQ_MNG
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -DCFG_FEATURE_STARS
			EXTRA_CFLAGS += -DVER_CDQM_B607
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_cdqm
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_cdqm
		endif

	else ifeq ($(TARGET_PRODUCT),mini)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/soc/mini_v1
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
			EXTRA_CFLAGS += -DCFG_FEATURE_CQ_HOST_MEM
			EXTRA_CFLAGS += -DCFG_FEATURE_IPC_NOTIFY_WITH_EVENTID
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -DCFG_FEATURE_ID_MAPING
			EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
			EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG_INNER
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
			EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
			EXTRA_CFLAGS += -DCFG_FEATURE_USE_MAILBOX_RECYCLE
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_conflict_check/ascend610
		else
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend310
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_conflict_check/ascend310
		endif
	else
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/soc/mini_v1
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
			EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_conflict_check/ascend610
		endif
	endif

	ifeq ($(TARGET_PRODUCT),cloud)
		obj-m += drv_devdrv_host.o
	else ifeq ($(TARGET_PRODUCT),mini)
		obj-m += drv_devdrv_host.o
	else
		obj-$(CONFIG_HOST_DEVDRV_DEVDRIVER) += drv_devdrv_host.o
	endif

	ifeq ($(TARGET_PRODUCT),mini)
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			drv_devdrv_host-objs := ts_drv_common/devdrv_cqsq.o ts_drv_common/devdrv_devinit.o tsdrv_get_ssid.o
			drv_devdrv_host-objs += ts_drv_common/devdrv_functional_cqsq.o ts_drv_common/devdrv_mailbox.o ts_drv_common/tsdrv_delay_recycle.o
			drv_devdrv_host-objs += ts_drv_common/devdrv_id.o ts_drv_common/devdrv_recycle.o ts_drv_common/devdrv_shm.o
			drv_devdrv_host-objs += ts_drv_common/devdrv_callback/devdrv_cb.o ts_drv_common/devdrv_callback/devdrv_cbctrl.o ts_drv_common/devdrv_callback/tsdrv_cb_pm.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_device.o ts_drv_common/tsdrv_dev/tsdrv_ctx.o ts_drv_common/tsdrv_dev/tsdrv_capacity.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo_platform.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_dev_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_mailbox_dfx.o ts_drv_common/tsdrv_dev/tsdrv_comm_fops.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_ctx_dfx.o ts_drv_common/tsdrv_dev/tsdrv_event_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_config_dfx.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_id/tsdrv_id.o ts_drv_common/tsdrv_id/tsdrv_sync.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_nvme/tsdrv_nvme.o ts_drv_common/tsdrv_nvme/shm/shm_sqcq.o ts_drv_common/tsdrv_nvme/logic/logic_cq.o ts_drv_common/tsdrv_nvme/logic/tsdrv_logic_cq.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_id/hvtsdrv_id.o ts_drv_common/tsdrv_id/hvtsdrv_id_event.o ts_drv_common/tsdrv_id/hvtsdrv_cqsq.o
			drv_devdrv_host-objs += ts_drv_common/devdrv_ipc_notify.o ts_drv_common/devdrv_nsm.o
			drv_devdrv_host-objs += tsdrv_conflict_check/tsdrv_bar_init.o devdrv_config_p2p.o tsdrv_drvops.o tsdrv_notify.o ascend310p/tsdrv_shr_id_event_update.o
			drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_init.o tsdrv_virt/hvtsdrv_proc.o tsdrv_virt/hvtsdrv_cb.o tsdrv_virt/hvtsdrv_logic_cq.o tsdrv_virt/hvtsdrv_shm_sqcq.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend610/tsdrv_set_runtime_conflict_check.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_vsq.o ts_drv_common/hvtsdrv_tsagent.o
		else
			drv_devdrv_host-objs := ts_drv_common/devdrv_cqsq.o ts_drv_common/devdrv_devinit.o tsdrv_get_ssid.o
			drv_devdrv_host-objs += ts_drv_common/devdrv_functional_cqsq.o ts_drv_common/devdrv_mailbox.o ts_drv_common/tsdrv_delay_recycle.o
			drv_devdrv_host-objs += ts_drv_common/devdrv_id.o ts_drv_common/devdrv_recycle.o ts_drv_common/devdrv_shm.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_nvme/sync_cb/devdrv_cbcq.o ts_drv_common/tsdrv_nvme/sync_cb/devdrv_sync_cbctrl.o ts_drv_common/tsdrv_nvme/sync_cb/devdrv_cbsq.o ts_drv_common/tsdrv_nvme/sync_cb/devdrv_cb_stub.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_device.o ts_drv_common/tsdrv_dev/tsdrv_ctx.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_capacity.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo_platform.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_dev_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_mailbox_dfx.o ts_drv_common/tsdrv_dev/tsdrv_comm_fops.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_ctx_dfx.o ts_drv_common/tsdrv_dev/tsdrv_event_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_config_dfx.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_id/tsdrv_id.o ts_drv_common/tsdrv_id/tsdrv_sync.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_nvme/tsdrv_nvme.o ts_drv_common/tsdrv_nvme/shm/shm_sqcq.o ts_drv_common/tsdrv_nvme/logic/logic_cq.o ts_drv_common/tsdrv_nvme/logic/tsdrv_logic_cq.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_id/hvtsdrv_id.o ts_drv_common/tsdrv_id/hvtsdrv_id_event.o ts_drv_common/tsdrv_id/hvtsdrv_cqsq.o
			drv_devdrv_host-objs += ts_drv_common/devdrv_ipc_notify.o
			drv_devdrv_host-objs += tsdrv_drvops.o devdrv_config_p2p.o tsdrv_notify.o
			drv_devdrv_host-objs += tsdrv_conflict_check/tsdrv_bar_init.o
			drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_init.o tsdrv_virt/hvtsdrv_proc.o tsdrv_virt/hvtsdrv_cb.o tsdrv_virt/hvtsdrv_logic_cq.o tsdrv_virt/hvtsdrv_shm_sqcq.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend310/tsdrv_set_runtime_conflict_check.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_vsq.o ts_drv_common/hvtsdrv_tsagent.o
		endif
	else
		drv_devdrv_host-objs := ts_drv_common/devdrv_cqsq.o ts_drv_common/devdrv_devinit.o tsdrv_get_ssid.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_functional_cqsq.o ts_drv_common/devdrv_mailbox.o ts_drv_common/tsdrv_delay_recycle.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_id.o ts_drv_common/devdrv_recycle.o ts_drv_common/devdrv_shm.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_callback/devdrv_cb.o ts_drv_common/devdrv_callback/devdrv_cbctrl.o ts_drv_common/devdrv_callback/tsdrv_cb_pm.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_device.o ts_drv_common/tsdrv_dev/tsdrv_ctx.o ts_drv_common/tsdrv_dev/tsdrv_capacity.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo_platform.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_dev_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_mailbox_dfx.o ts_drv_common/tsdrv_dev/tsdrv_comm_fops.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_ctx_dfx.o ts_drv_common/tsdrv_dev/tsdrv_event_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_config_dfx.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_id/tsdrv_id.o ts_drv_common/tsdrv_id/tsdrv_sync.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_nvme/tsdrv_nvme.o ts_drv_common/tsdrv_nvme/shm/shm_sqcq.o ts_drv_common/tsdrv_nvme/logic/logic_cq.o ts_drv_common/tsdrv_nvme/logic/tsdrv_logic_cq.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_id/hvtsdrv_id.o ts_drv_common/tsdrv_id/hvtsdrv_id_event.o ts_drv_common/tsdrv_id/hvtsdrv_cqsq.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_ipc_notify.o ts_drv_common/devdrv_nsm.o
		drv_devdrv_host-objs += tsdrv_conflict_check/tsdrv_bar_init.o devdrv_config_p2p.o tsdrv_drvops.o tsdrv_notify.o
		drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_init.o tsdrv_virt/hvtsdrv_proc.o tsdrv_virt/hvtsdrv_cb.o tsdrv_virt/hvtsdrv_logic_cq.o tsdrv_virt/hvtsdrv_shm_sqcq.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend910/tsdrv_set_runtime_conflict_check.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_vsq.o ts_drv_common/hvtsdrv_tsagent.o
	endif

	drv_devdrv_host-objs += ts_drv_common/tsdrv_sqcq_dfx/tsdrv_sqcq_timestamp.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_osal/tsdrv_osal_intr.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_osal/tsdrv_osal_mm.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_hotreset/tsdrv_hotreset.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_log.o

	ifeq ($(TARGET_PRODUCT),cloud)
		drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend910/tsdrv_set_runtime_conflict_check.o
		ifeq ($(TARGET_CHIP_ID), hi1980)
		    drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_mdev.o
		endif
		ifeq ($(TARGET_CHIP_ID), hi1980b)
		    drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_sriov.o
			drv_devdrv_host-objs += tsdrv_cdqm/tsdrv_cdqm_host.o tsdrv_cdqm/cdq_manager.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_cdqm/tsdrv_cdqm_module.o
		endif
	else ifeq ($(TARGET_PRODUCT),mini)
	    drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_mdev.o
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
			drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend610/tsdrv_set_runtime_conflict_check.o
		else
			drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend310/tsdrv_set_runtime_conflict_check.o
		endif
	else ifeq ($(TARGET_PRODUCT),mdc)
	    drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_mdev.o
		ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_MDC_V10))
			drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend610/tsdrv_set_runtime_conflict_check.o
		else ifeq ($(TARGET_CHIP_ID), $(ASCEND_MDC_BS9SX1A))
			drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend610/tsdrv_set_runtime_conflict_check.o
		endif
	endif
else
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
endif

else   #########end module.mk
ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char  -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

EXTRA_CFLAGS += -DCFG_MANAGER_HOST_ENV
EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_IPC_NOTIFY
EXTRA_CFLAGS += -DCFG_FEATURE_HOTRESET
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/rms/trs_drv/inc
ifeq ($(DAVINCI_HIAI_DKMS),y)

else

	ifeq ($(CONFIG_HOST_DEVDRV_DEVDRIVER),)
		ifeq ($(CONFIG_NPU_DEVDRV),)
		CONFIG_HOST_DEVDRV_DEVDRIVER=m
		endif
	endif

	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_sqcq_dfx
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_osal
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/include
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_hotreset
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor/config
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/config
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/event
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
	EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
	ifeq ($(filter $(PRODUCT),  ascend310 ascend310rc),)
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/bbox/inc/bbox
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_virt
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/devdrv_callback
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_id
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme/logic
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_nvme/shm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_vm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_conflict_check
	else
		EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_virt
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/devdrv_callback/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/tsdrv_dev/
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/tsdrv_id
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/tsdrv_nvme
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/tsdrv_nvme/logic
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/tsdrv_nvme/shm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/tsdrv_nvme/sync_cb
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_conflict_check
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_vm
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend310
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
	endif

	ifneq ($(filter $(PRODUCT),  ascend910 ascend910B),)
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/soc/cloud_v1
		EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend910
		EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_conflict_check/ascend910
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
		EXTRA_CFLAGS += -DCFG_FEATURE_USE_MAILBOX_RECYCLE
		EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
		EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
		EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
		ifeq ($(PRODUCT),ascend910)
		    EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
		endif
		ifneq ($(filter $(PRODUCT),  ascend910B),)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
			EXTRA_CFLAGS += -DCFG_FEATURE_CDQM
			EXTRA_CFLAGS += -DCFG_FEATURE_CALLBACK_EVENT
			EXTRA_CFLAGS += -DCFG_FEATURE_SQ_HOST_MEM
			EXTRA_CFLAGS += -DCFG_FEATURE_CQ_HOST_MEM
			EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_VSQ_MNG
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -DCFG_FEATURE_STARS
			EXTRA_CFLAGS += -DVER_CDQM_B607
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_cdqm
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_cdqm
		endif
	else ifneq ($(filter $(PRODUCT), ascend310 ascend310rc ascend310B ascend310Besl ascend310p ),)
		EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINI
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/soc/mini_v1
		ifeq ($(PRODUCT),ascend310p)
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
			EXTRA_CFLAGS += -DCFG_FEATURE_CQ_HOST_MEM
			EXTRA_CFLAGS += -DCFG_FEATURE_IPC_NOTIFY_WITH_EVENTID
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_HOST
			EXTRA_CFLAGS += -DCFG_FEATURE_VFIO
			EXTRA_CFLAGS += -DCFG_FEATURE_ID_MAPING
			EXTRA_CFLAGS += -DCFG_FEATURE_USE_MAILBOX_RECYCLE
			EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
			EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG_INNER
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend610
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_conflict_check/ascend610
		else ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl),)
			EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_HOST
			EXTRA_CFLAGS += -DCFG_FEATURE_CDQM
			EXTRA_CFLAGS += -DCFG_FEATURE_CMO
			EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_VSQ_MNG
			EXTRA_CFLAGS += -DCFG_FEATURE_USE_MAILBOX_RECYCLE
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -DCFG_FEATURE_STARS
			EXTRA_CFLAGS += -DCFG_FEATURE_CALLBACK_EVENT
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_cdqm
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_cdqm
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend610
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_conflict_check/ascend610
		else
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend310
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_conflict_check/ascend310
		endif
	else
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
		EXTRA_CFLAGS += -I$(TOP_DIR)/inc/soc/mini_v1
		ifneq ($(filter $(PRODUCT), ascend610 bs9sx1a),)
			EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV2
			EXTRA_CFLAGS += -DCFG_FEATURE_RUNTIME_NO_THREAD
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend610
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_conflict_check/ascend610
		else
			EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend310
		endif
	endif

	ifneq ($(filter $(PRODUCT), ascend310 ascend310rc ascend310B ascend310Besl ascend310p  ascend910 ascend910B),)
		obj-m += drv_devdrv_host.o
	else
		obj-$(CONFIG_HOST_DEVDRV_DEVDRIVER) += drv_devdrv_host.o
	endif

	obj-$(CONFIG_NPU_DEVDRV_DEVDRIVER) += ../ts_dev_device/

	ifeq ($(filter $(PRODUCT),  ascend310 ascend310rc),)
		drv_devdrv_host-objs := ts_drv_common/devdrv_cqsq.o ts_drv_common/devdrv_devinit.o tsdrv_get_ssid.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_functional_cqsq.o ts_drv_common/devdrv_mailbox.o ts_drv_common/tsdrv_delay_recycle.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_id.o ts_drv_common/devdrv_recycle.o ts_drv_common/devdrv_shm.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_callback/devdrv_cb.o ts_drv_common/devdrv_callback/devdrv_cbctrl.o ts_drv_common/devdrv_callback/tsdrv_cb_pm.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_device.o ts_drv_common/tsdrv_dev/tsdrv_ctx.o ts_drv_common/tsdrv_dev/tsdrv_capacity.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo_platform.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_dev_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_mailbox_dfx.o ts_drv_common/tsdrv_dev/tsdrv_comm_fops.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_ctx_dfx.o ts_drv_common/tsdrv_dev/tsdrv_event_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_config_dfx.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_id/tsdrv_id.o ts_drv_common/tsdrv_id/tsdrv_sync.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_nvme/tsdrv_nvme.o ts_drv_common/tsdrv_nvme/shm/shm_sqcq.o ts_drv_common/tsdrv_nvme/logic/logic_cq.o ts_drv_common/tsdrv_nvme/logic/tsdrv_logic_cq.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_id/hvtsdrv_id.o ts_drv_common/tsdrv_id/hvtsdrv_id_event.o ts_drv_common/tsdrv_id/hvtsdrv_cqsq.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_ipc_notify.o ts_drv_common/devdrv_nsm.o
		drv_devdrv_host-objs += tsdrv_conflict_check/tsdrv_bar_init.o devdrv_config_p2p.o tsdrv_drvops.o tsdrv_notify.o
		drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_init.o tsdrv_virt/hvtsdrv_proc.o tsdrv_virt/hvtsdrv_cb.o tsdrv_virt/hvtsdrv_logic_cq.o tsdrv_virt/hvtsdrv_shm_sqcq.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_vsq.o ts_drv_common/hvtsdrv_tsagent.o
	else
		drv_devdrv_host-objs := ts_drv_common/devdrv_cqsq.o ts_drv_common/devdrv_devinit.o tsdrv_get_ssid.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_functional_cqsq.o ts_drv_common/devdrv_mailbox.o ts_drv_common/tsdrv_delay_recycle.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_id.o ts_drv_common/devdrv_recycle.o ts_drv_common/devdrv_shm.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_nvme/sync_cb/devdrv_cbcq.o ts_drv_common/tsdrv_nvme/sync_cb/devdrv_sync_cbctrl.o ts_drv_common/tsdrv_nvme/sync_cb/devdrv_cbsq.o ts_drv_common/tsdrv_nvme/sync_cb/devdrv_cb_stub.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_device.o ts_drv_common/tsdrv_dev/tsdrv_ctx.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_capacity.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo.o ts_drv_common/tsdrv_dev/tsdrv_hwinfo_platform.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_dev_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_mailbox_dfx.o ts_drv_common/tsdrv_dev/tsdrv_comm_fops.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_dev/tsdrv_ctx_dfx.o ts_drv_common/tsdrv_dev/tsdrv_event_id_dfx.o ts_drv_common/tsdrv_dev/tsdrv_id_config_dfx.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_id/tsdrv_id.o ts_drv_common/tsdrv_id/tsdrv_sync.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_nvme/tsdrv_nvme.o ts_drv_common/tsdrv_nvme/shm/shm_sqcq.o ts_drv_common/tsdrv_nvme/logic/logic_cq.o ts_drv_common/tsdrv_nvme/logic/tsdrv_logic_cq.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_id/hvtsdrv_id.o ts_drv_common/tsdrv_id/hvtsdrv_id_event.o ts_drv_common/tsdrv_id/hvtsdrv_cqsq.o
		drv_devdrv_host-objs += devdrv_config_p2p.o tsdrv_drvops.o tsdrv_notify.o
		drv_devdrv_host-objs += ts_drv_common/devdrv_ipc_notify.o
		drv_devdrv_host-objs += tsdrv_conflict_check/tsdrv_bar_init.o
		drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_init.o tsdrv_virt/hvtsdrv_proc.o tsdrv_virt/hvtsdrv_cb.o tsdrv_virt/hvtsdrv_logic_cq.o tsdrv_virt/hvtsdrv_shm_sqcq.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_vsq.o ts_drv_common/hvtsdrv_tsagent.o
	endif

	drv_devdrv_host-objs += ts_drv_common/tsdrv_sqcq_dfx/tsdrv_sqcq_timestamp.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_osal/tsdrv_osal_intr.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_osal/tsdrv_osal_mm.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_hotreset/tsdrv_hotreset.o
	drv_devdrv_host-objs += ts_drv_common/tsdrv_log.o

	ifneq ($(filter $(PRODUCT),  ascend910 ascend910B),)
		drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend910/tsdrv_set_runtime_conflict_check.o
		ifneq ($(filter $(PRODUCT),  ascend910B),)
			drv_devdrv_host-objs += tsdrv_cdqm/tsdrv_cdqm_host.o tsdrv_cdqm/cdq_manager.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_cdqm/tsdrv_cdqm_module.o
		else
			drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_mdev.o
		endif
	else ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl ascend610 bs9sx1a ascend310p),)
		drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend610/tsdrv_set_runtime_conflict_check.o
		drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_mdev.o ascend310p/tsdrv_shr_id_event_update.o
		ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl),)
			drv_devdrv_host-objs += tsdrv_cdqm/tsdrv_cdqm_host.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_cmo_id.o
			drv_devdrv_host-objs += ts_drv_common/tsdrv_cdqm/tsdrv_cdqm_module.o
			drv_devdrv_host-objs += tsdrv_cdqm/cdq_manager.o
		endif
	else
		drv_devdrv_host-objs += tsdrv_virt/hvtsdrv_mdev.o
		drv_devdrv_host-objs += ts_drv_common/tsdrv_conflict_check/ascend310/tsdrv_set_runtime_conflict_check.o
	endif
endif

endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

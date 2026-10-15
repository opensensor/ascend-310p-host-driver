ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/pcie_host/soc_adapt
    ccflags-y += -I$(HIAI_DKMS_DIR)/pcie_host
    ccflags-y += -I$(HIAI_DKMS_DIR)/common_h
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc/depends/
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc/depends/dbl
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc/depends/log
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc/depends/vpc
    ccflags-y += -I$(HIAI_DKMS_DIR)/pcie_host/interfaces
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include

    ifeq ($(TARGET_PRODUCT),cloud)
        ifeq ($(TARGET_CHIP_ID), $(A9XXGEN02M01))
        else
            ccflags-y += -DCFG_FEATURE_DMI
        endif
        ifeq ($(TARGET_CHIP_ID), hi1980b)
            ccflags-y += -DCFG_FEATURE_SRIOV
            ccflags-y += -DCFG_FEATURE_PASSID
            ccflags-y += -DCFG_FEATURE_AGENT_SMMU
            ccflags-y += -DCFG_FEATURE_AGENT_S2S
        endif
    else
        ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -DCFG_FEATURE_P2P_EXTEND
        endif
    endif

    ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif
else
    ifeq ($(ENABLE_OPEN_SRC),y)
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/depends/inc_driver
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/depends/sec
    else
        ccflags-y += -I$(TOP_DIR)/inc/driver/
        ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
    endif
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/depends/
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/depends/dbl
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/depends/vpc
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/depends/log
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/src/drv_pcie/common_h/
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/src/drv_pcie/pcie_host/
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/src/drv_pcie/pcie_host/soc_adapt/

    ifneq ($(filter $(PRODUCT),  ascend910),)
        ccflags-y += -DCFG_FEATURE_DMI
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif

    ifneq ($(filter $(PRODUCT),  ascend910B),)
        ccflags-y += -DCFG_FEATURE_DMI
        ccflags-y += -DCFG_FEATURE_SRIOV
        ccflags-y += -DCFG_FEATURE_PASSID
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_AGENT_SMMU
        ccflags-y += -DCFG_FEATURE_AGENT_S2S
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif

    ifneq ($(filter $(PRODUCT),  as31xm1),)
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V11
        ccflags-y += -DCFG_FEATURE_PCIE_LINK_INFO
        ccflags-y += -DCFG_FEATURE_DUMP_PCIE_DFX
    endif

    ifneq ($(filter $(PRODUCT),  ascend610Lite),)
        ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/dms/include
        ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/inc
        ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/dms/config
        ccflags-y += -DCFG_SOC_PLATFORM_MDC_V51_LITE
        ccflags-y += -DCFG_FEATURE_PCIE_LINK_INFO
        ccflags-y += -DCFG_FEATURE_SYSFS_DUMP_DFX
        ccflags-y += -DCFG_FEATURE_DUMP_PCIE_DFX
        ccflags-y += -DCFG_FEATURE_PCIE_SENTRY
        ccflags-y += -DCFG_FEATURE_SEC_COMM_L3
    endif

    ifneq ($(filter $(PRODUCT),  ascend310B),)
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif

    ifneq ($(filter $(PRODUCT),  ascend310p),)
        ccflags-y += -DCFG_FEATURE_HOST_LOG
        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        ccflags-y += -DCFG_FEATURE_P2P_EXTEND
    endif
endif

ifeq ($(strip $(TARGET_BUILD_TYPE)),debug)
    ccflags-y += -DCFG_BUILD_DEBUG
endif

ccflags-y += -DCFG_HOST
ccflags-y += -DCFG_FEATURE_SHARE_LOG
ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ifeq ($(ENABLE_ASAN),true)
    ccflags-y += -DCFG_ENABLE_ASAN
endif
ifeq ($(CUSTOM_OS_CCFLAGS), )
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
else
ccflags-y += $(CUSTOM_OS_CCFLAGS)
endif
obj-m   += drv_pcie_host.o
drv_pcie_host-objs := devdrv_dma.o devdrv_msg.o devdrv_ctrl.o devdrv_sysfs.o devdrv_pci.o devdrv_common_msg.o devdrv_device_load.o
drv_pcie_host-objs += devdrv_atu_interface.o devdrv_vpc.o
drv_pcie_host-objs += interfaces/devdrv_dma_interface.o devdrv_device_dma.o devdrv_device_dma_source.o
drv_pcie_host-objs += devdrv_smmu.o
drv_pcie_host-objs += devdrv_pcie_link_info.o
drv_pcie_host-objs += devdrv_pm.o

ifneq ($(filter $(PRODUCT), ascend910B),)
    drv_pcie_host-objs += devdrv_s2s_msg.o
endif

# for dkms use, do not delete
ifeq ($(TARGET_CHIP_ID), hi1980b)
    drv_pcie_host-objs += devdrv_s2s_msg.o
endif

drv_pcie_host-objs += soc_adapt/res_drv.o soc_adapt/dma_adapt.o soc_adapt/nvme_adapt.o soc_adapt/res_drv_mini_v1.o soc_adapt/res_drv_mini_v2.o \
                    soc_adapt/res_drv_cloud_v1.o soc_adapt/res_drv_cloud_v2.o soc_adapt/res_drv_mini_v3.o soc_adapt/res_drv_mdc_v2.o

ifneq ($(filter $(PRODUCT),  as31xm1 ascend610Lite),)
    drv_pcie_host-objs += devdrv_pcie_dump_dfx.o
endif

ifneq ($(filter $(PRODUCT), ascend610Lite),)
    drv_pcie_host-objs += devdrv_sentry.o
endif

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

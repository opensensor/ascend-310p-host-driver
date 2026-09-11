ccflags-y += -Wall -Werror
ccflags-y += -Wno-missing-prototypes -Wno-missing-declarations

ifeq ($(TOP_DIR),)
    ccflags-y += -DCFG_MANAGER_HOST_ENV
    ccflags-y += -DCFG_FEATURE_SHARE_LOG
    ifneq ($(NOT_SUPPORT_SP), y)
        ccflags-y += -fstack-protector-all
    endif

    obj-m += drv_tsdrv_platform_host.o
    drv_tsdrv_platform_host-objs := tsdrv_platform.o
    drv_tsdrv_platform_host-objs += tsdrv_plat_dev.o tsdrv_parse/tsdrv_parse.o
    drv_tsdrv_platform_host-objs += tsdrv_h2d_chan/tsdrv_h2d_chan.o tsdrv_h2d_chan/pci/tsdrv_pci_chan.o

    KERNEL_DIR := $(srctree)
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        ccflags-y += -I$(HIAI_DKMS_DIR)/drv_devmng_inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_h2d_chan
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_h2d_chan/pci
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/tsdrv_virt
        ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_common/tsdrv_dev
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        ifeq ($(TARGET_PRODUCT),mini)
            ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
                ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
                ccflags-y += -DCFG_FEATURE_HOST_LOG
                ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_id
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_dev
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
                ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse/ascend910
                drv_tsdrv_platform_host-objs += tsdrv_parse/ascend910/tsdrv_irq_parse.o tsdrv_parse/ascend910/tsdrv_addr_parse.o
            else
                ccflags-y += -DCFG_SOC_PLATFORM_MINI
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_id
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_dev
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend310
                ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend310
                ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse/ascend310
                drv_tsdrv_platform_host-objs += tsdrv_parse/ascend310/tsdrv_irq_parse.o tsdrv_parse/ascend310/tsdrv_addr_parse.o
            endif
        endif
        ifeq ($(TARGET_PRODUCT),cloud)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_id
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_dev
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse/ascend910
            ifeq ($(TARGET_CHIP_ID), hi1980b)
                ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_V2
                ccflags-y += -DCFG_FEATURE_SUPPORT_VSQ_MNG
            endif
            drv_tsdrv_platform_host-objs += tsdrv_parse/ascend910/tsdrv_irq_parse.o tsdrv_parse/ascend910/tsdrv_addr_parse.o
        endif
    else
        TOPDIR_ABS := $(KERNEL_DIR)
        ccflags-y += -I$(TOPDIR_ABS)/../../abl/libc_sec/include
        ccflags-y += -I$(TOPDIR_ABS)/../../libc_sec/include
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/ai_sdk/arc/linux/kernel_space/dev_inc
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/ai_sdk/arc/linux/kernel_space/npu_inc
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_core/tsdrv/tsdrv_inc
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_vm
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_core/device_manager/device_manager_inc
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/tsdrv_parse
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/tsdrv_h2d_chan
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/tsdrv_h2d_chan/pci
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/tsdrv_virt
        ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
        ifeq ($(TARGET_PRODUCT),cloud)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common
            ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_id
            ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend910
            ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/ascend910
            ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/tsdrv_parse/ascend910
            ifeq ($(TARGET_CHIP_ID), $(A9XXGEN02M01))
                ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_V2
            endif
        else ifeq ($(TARGET_PRODUCT),mini)
            ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
                ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
                ccflags-y += -DCFG_FEATURE_HOST_LOG
                ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_id
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend910
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/ascend610
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/tsdrv_parse/ascend910
            else
                ccflags-y += -DCFG_SOC_PLATFORM_MINI
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_common/tsdrv_id
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host/ts_drv_common
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_drv/ts_drv_host
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/ascend310
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/drv_devmng/drv_devmng_host/ascend310
                ccflags-y += -I$(TOPDIR_ABS)/../../drivers/dev_platform/tsdrv/ts_platform/ts_platform_host/tsdrv_parse/ascend310
            endif
        endif
    endif

else
    ccflags-y += -DCFG_MANAGER_HOST_ENV
    ccflags-y += -DCFG_FEATURE_SHARE_LOG
    ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
    ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/rms/trs_drv/inc
    ifneq ($(NOT_SUPPORT_SP), y)
        ccflags-y += -fstack-protector-all
    endif

    ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
    ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

    obj-m += drv_tsdrv_platform_host.o
    drv_tsdrv_platform_host-objs := tsdrv_platform.o
    drv_tsdrv_platform_host-objs += tsdrv_plat_dev.o tsdrv_parse/tsdrv_parse.o
    drv_tsdrv_platform_host-objs += tsdrv_h2d_chan/tsdrv_h2d_chan.o tsdrv_h2d_chan/pci/tsdrv_pci_chan.o

    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/npu_inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        ccflags-y += -I$(HIAI_DKMS_DIR)/tsdrv/tsdrv_inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_vm
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_h2d_chan
        ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_h2d_chan/pci
        ccflags-y += -I$(HIAI_DKMS_DIR)/dms/include
        ifneq ($(filter $(PRODUCT), ascend910 ascend910B),)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_id
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_dev
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend910
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse/ascend910
            drv_tsdrv_platform_host-objs += tsdrv_parse/ascend910/tsdrv_irq_parse.o tsdrv_parse/ascend910/tsdrv_addr_parse.o
        else ifneq ($(filter $(PRODUCT),  ascend310 ascend310rc),)
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_id
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_dev
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend310
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend310
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse/ascend310

            drv_tsdrv_platform_host-objs += tsdrv_parse/ascend310/tsdrv_irq_parse.o tsdrv_parse/ascend310/tsdrv_addr_parse.o
        else ifneq ($(filter $(PRODUCT), ascend310p),)
            ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_id
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_drv_host/ts_drv_common/tsdrv_dev
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/ascend610
            ccflags-y += -I$(HIAI_DKMS_DIR)/dms/drv_devmng/drv_devmng_host/ascend910
            ccflags-y += -I$(HIAI_DKMS_DIR)/ts_platform_host/tsdrv_parse/ascend910

            drv_tsdrv_platform_host-objs += tsdrv_parse/ascend910/tsdrv_irq_parse.o tsdrv_parse/ascend910/tsdrv_addr_parse.o
        endif
    else
        ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
        ccflags-y += -I$(TOP_DIR)/libc_sec/include
        ccflags-y += -I$(TOP_DIR)/inc/driver
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_inc/
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/include
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_vm
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/tsdrv_parse
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/tsdrv_h2d_chan
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/tsdrv_h2d_chan/pci
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/tsdrv_virt
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
        ifneq ($(filter $(PRODUCT), ascend910 ascend910B),)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_id
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/tsdrv_parse/ascend910
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend910
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
            ifneq ($(filter $(PRODUCT), ascend910B),)
                ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_V2
                ccflags-y += -DCFG_FEATURE_SUPPORT_VSQ_MNG
            endif
            drv_tsdrv_platform_host-objs += tsdrv_parse/ascend910/tsdrv_irq_parse.o tsdrv_parse/ascend910/tsdrv_addr_parse.o
        else ifneq ($(filter $(PRODUCT), ascend310 ascend310rc),)
            ccflags-y += -DCFG_SOC_PLATFORM_MINI
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common/tsdrv_id
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_host/ts_drv_common
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend310
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend310
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/tsdrv_parse/ascend310
            drv_tsdrv_platform_host-objs += tsdrv_parse/ascend310/tsdrv_irq_parse.o tsdrv_parse/ascend310/tsdrv_addr_parse.o
        else ifneq ($(filter $(PRODUCT), ascend310p ascend310B ascend310Besl),)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl),)
                ccflags-y += -DCFG_SOC_PLATFORM_MINIV3
                ccflags-y += -DCFG_FEATURE_SUPPORT_VSQ_MNG
            else
                ccflags-y += -DCFG_SOC_PLATFORM_MINIV2
            endif
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_drv/ts_drv_common/tsdrv_id
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/tsdrv_parse/ascend910
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/tsdrv/ts_platform/ts_platform_host/ascend610
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/drv_devmng/drv_devmng_host/ascend910
            drv_tsdrv_platform_host-objs += tsdrv_parse/ascend910/tsdrv_irq_parse.o tsdrv_parse/ascend910/tsdrv_addr_parse.o
        endif
    endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

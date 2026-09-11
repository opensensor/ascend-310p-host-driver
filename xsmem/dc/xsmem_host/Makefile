ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
        ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
            ifneq ($(filter $(PRODUCT),  helper310p),)
            else
                ccflags-y += -DCFG_FEATURE_HOST_LOG
                ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            endif
        endif
        ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/xsmem/dc/xsmem_host
        ccflags-y += -Wno-stringop-overread
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)
    else
        ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-implicit-fallthrough -Wno-stringop-overread
        TOPDIR_ABS := $(KERNEL_DIR)
        ccflags-y += -I$(TOPDIR_ABS)/../../../../../inc/driver
        ccflags-y += -I$(TOPDIR_ABS)/../../../../../abl/libc_sec/include
        ccflags-y += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/inc
        ccflags-y += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/src/xsmem/dc/xsmem_host
        ccflags-y += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
        ccflags-y += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include/arch/$(ARCH)
    endif
else
    MAKE_PRODUCT := $(PRODUCT)

    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/xsmem/dc/xsmem_host
        ccflags-y += -Wno-stringop-overread
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)
    else
        ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-implicit-fallthrough -Wno-stringop-overread
        ccflags-y += -I$(TOP_DIR)/inc/driver/
        ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/xsmem/dc/xsmem_host
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)
    endif
endif

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror
obj-m   += ascend_xsmem.o
ascend_xsmem-objs := xsmem_framework.o xsmem_ns_adapt.o xsmem_proc_fs.o xsmem_prop.o xsmem_res_idr.o xsmem_res_dispatch.o xsmem_algo_vma.o xsmem_algo_cache_vma.o multi_rbtree.o
ifeq ($(filter $(PRODUCT), helper310p),)
    ccflags-y += -DCFG_FEATURE_EXTERNAL_CDEV
    ascend_xsmem-objs += xsmem_modprobe_adapt.o
endif
ccflags-y += -DCFG_FEATURE_SUPPORT_VMA
ccflags-y += -DCFG_FEATURE_SUPPORT_CACHE
ccflags-y += -DCFG_FEATURE_SHARE_LOG
ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
    ccflags-y += -DCFG_FEATURE_HOST_LOG
    ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

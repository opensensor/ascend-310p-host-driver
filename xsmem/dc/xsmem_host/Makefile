ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif
        ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
            ifneq ($(filter $(PRODUCT),  helper310p),)
            else
                EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
                EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
            endif
        endif
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/xsmem/dc/xsmem_host
        ccflags-y += -Wno-stringop-overread
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)
    else
        ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-implicit-fallthrough -Wno-stringop-overread
        TOPDIR_ABS := $(KERNEL_DIR)
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../inc/driver
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../abl/libc_sec/include
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/inc
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_space/src/xsmem/dc/xsmem_host
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
        EXTRA_CFLAGS += -I$(TOPDIR_ABS)/../../../../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include/arch/$(ARCH)
    endif
else
    MAKE_PRODUCT := $(PRODUCT)

    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/xsmem/dc/xsmem_host
        ccflags-y += -Wno-stringop-overread
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)
    else
        ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-implicit-fallthrough -Wno-stringop-overread
        EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver/
        EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/xsmem/dc/xsmem_host
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)
    endif
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -Werror
obj-m   += ascend_xsmem.o
ascend_xsmem-objs := xsmem_framework.o xsmem_ns_adapt.o xsmem_proc_fs.o xsmem_prop.o xsmem_res_idr.o xsmem_res_dispatch.o xsmem_algo_vma.o xsmem_algo_cache_vma.o multi_rbtree.o
ifeq ($(filter $(PRODUCT), helper310p),)
    EXTRA_CFLAGS += -DCFG_FEATURE_EXTERNAL_CDEV
    ascend_xsmem-objs += xsmem_modprobe_adapt.o
endif
EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_VMA
EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_CACHE
EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
    EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

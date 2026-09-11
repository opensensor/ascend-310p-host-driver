ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-stringop-overread

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG

ifeq ($(TOP_DIR),)
    KERNEL_DIR :=$(srctree)
    EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../inc/driver
    EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../abl/libc_sec/include
    EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_space/inc
    EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_space/src/xsmem/dc/xsmem_device
    EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
    EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include/arch/$(ARCH)

    EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_SP
    EXTRA_CFLAGS += -DCFG_FEATURE_EXTERNAL_CDEV

    obj-m   += ascend_xsmem.o
    ascend_xsmem-objs := xsmem_framework.o xsmem_ns_adapt.o xsmem_proc_fs.o xsmem_algo_sp.o xsmem_prop.o xsmem_res_idr.o xsmem_res_dispatch.o xsmem_algo_cache_sp.o xsmem_algo_vma.o multi_rbtree.o
else
    MAKE_PRODUCT := $(PRODUCT)

    ifeq ($(filter $(HELPER_KERNAL), openeuler),)
        EXTRA_CFLAGS += -DCFG_FEATURE_SP_GRP_STANDALONE_VA
    endif

    ifneq ($(filter $(MAKE_PRODUCT), ascend310p),)
        EXTRA_CFLAGS += -DCFG_SUPPORT_UPDATE_NUMA_ORDER
    endif

    ifneq ($(filter $(MAKE_PRODUCT),  ascend910B ascend310p),)
        EXTRA_CFLAGS += -DCFG_SUPPORT_ALLOC_NODEMASK
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif

    ifneq ($(filter $(MAKE_PRODUCT),  ascend310rc ascend310Brc helper310p),)
        EXTRA_CFLAGS += -DCFG_FEATURE_SOC
    endif

    ifneq ($(filter $(MAKE_PRODUCT),  ascend310B ascend910 ascend310Brc),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif

    EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver/
    EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/xsmem/dc/xsmem_device
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include/
    EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include/arch/$(ARCH)

    EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_SP
    EXTRA_CFLAGS += -DCFG_FEATURE_EXTERNAL_CDEV
    EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_CACHE

    obj-m   += ascend_xsmem.o
    ascend_xsmem-objs := xsmem_framework.o xsmem_ns_adapt.o xsmem_proc_fs.o xsmem_algo_sp.o xsmem_prop.o xsmem_res_idr.o xsmem_res_dispatch.o xsmem_algo_cache_sp.o xsmem_algo_vma.o multi_rbtree.o
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

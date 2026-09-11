ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
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
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/logdrv/host
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/logdrv
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)

        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE

        obj-m   += ascend_logdrv.o    # host
        ascend_logdrv-objs := log_drv_agent.o
    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal
        ccflags-y += -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        KERNEL_DIR := $(srctree)
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../dev_inc/inc
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../dev_inc/inc/dbl
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../inc/driver
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../src/logdrv/host/
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../src/logdrv/
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../abl/libc_sec/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)

        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif
else
    ifeq ($(DAVINCI_HIAI_DKMS),y)

    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal
        ccflags-y += -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include/
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/logdrv/host/
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/logdrv
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)

        EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE

        obj-m   += ascend_logdrv.o    # host
        ascend_logdrv-objs := log_drv_agent.o
    endif
endif

ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

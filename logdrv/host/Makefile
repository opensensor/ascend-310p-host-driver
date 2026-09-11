ifeq ($(TARGET_BUILD_TYPE),debug)
    ccflags-y += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror

ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
        ccflags-y += -I$(HIAI_DKMS_DIR)/logdrv/host
        ccflags-y += -I$(HIAI_DKMS_DIR)/logdrv
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)

        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE

        obj-m   += ascend_logdrv.o    # host
        ascend_logdrv-objs := log_drv_agent.o
    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal
        ccflags-y += -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        KERNEL_DIR := $(srctree)
        ccflags-y += -I$(KERNEL_DIR)/../../dev_inc/inc
        ccflags-y += -I$(KERNEL_DIR)/../../dev_inc/inc/dbl
        ccflags-y += -I$(KERNEL_DIR)/../../inc/driver
        ccflags-y += -I$(KERNEL_DIR)/../../src/logdrv/host/
        ccflags-y += -I$(KERNEL_DIR)/../../src/logdrv/
        ccflags-y += -I$(KERNEL_DIR)/../../abl/libc_sec/include
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)

        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    endif
else
    ifeq ($(DAVINCI_HIAI_DKMS),y)

    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal
        ccflags-y += -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        ccflags-y += -I$(TOP_DIR)/inc/driver
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include/
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/logdrv/host/
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/logdrv
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)

        ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE

        obj-m   += ascend_logdrv.o    # host
        ascend_logdrv-objs := log_drv_agent.o
    endif
endif

ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

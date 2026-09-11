ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror

ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
        ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/common
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        KERNEL_DIR := $(srctree)
        ccflags-y += -I$(KERNEL_DIR)/../../dev_inc/inc
        ccflags-y += -I$(KERNEL_DIR)/../../inc/driver
        ccflags-y += -I$(KERNEL_DIR)/../../src/svmdrv/common/
        ccflags-y += -I$(KERNEL_DIR)/../../abl/libc_sec/include
    endif
    ccflags-y += -DCFG_FEATURE_SHARE_LOG

    drv_svm_peermem-objs := svm_peer_mem.o
    obj-m := drv_svm_peermem.o
else
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
        ccflags-y += -I$(HIAI_DKMS_DIR)/svmdrv/common
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        ccflags-y += -I$(TOP_DIR)/inc/driver
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common/
        ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
        ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
    endif
    ccflags-y += -DCFG_FEATURE_SHARE_LOG

    ifeq ($(PRODUCT_SIDE), device)
    	ifeq ($(PRODUCT), ascend910)
            ccflags-y += -DCFG_SUPPORT_REGISTER_PEER_MEM
        endif
    endif

    drv_svm_peermem-objs := svm_peer_mem.o
    obj-m := drv_svm_peermem.o
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

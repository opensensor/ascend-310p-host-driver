ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -Werror

ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/common
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        KERNEL_DIR := $(srctree)
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../dev_inc/inc
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../inc/driver
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../src/svmdrv/common/
        EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../abl/libc_sec/include
    endif
    EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG

    drv_svm_peermem-objs := svm_peer_mem.o
    obj-m := drv_svm_peermem.o
else
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/svmdrv/common
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral
        EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/svmdrv/common/
        EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
        EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
    endif
    EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG

    ifeq ($(PRODUCT_SIDE), device)
    	ifeq ($(PRODUCT), ascend910)
            EXTRA_CFLAGS += -DCFG_SUPPORT_REGISTER_PEER_MEM
        endif
    endif

    drv_svm_peermem-objs := svm_peer_mem.o
    obj-m := drv_svm_peermem.o
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

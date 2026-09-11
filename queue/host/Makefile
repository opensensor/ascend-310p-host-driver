ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wno-missing-prototypes -Wno-missing-declarations

ccflags-y += -DCFG_FEATURE_SHARE_LOG
ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
    ccflags-y += -DCFG_FEATURE_HOST_LOG
    ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ifeq ($(TOP_DIR),)
    MAKE_PRODUCT := $(PRODUCT)
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
        ccflags-y += -DDRV_HOST
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc \
            -I$(HIAI_DKMS_DIR)/dev_inc/inc \
            -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl \
            -I$(HIAI_DKMS_DIR)/queue/host/common \
            -I$(HIAI_DKMS_DIR)/queue/host \
            -I$(HIAI_DKMS_DIR)/inc/driver \
            -I$(HIAI_DKMS_DIR)/libc_sec/include \
            -I$(HIAI_DKMS_DIR)/kernel_adapt/include \
            -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)

        obj-m   += ascend_queue.o    # host
        ascend_queue-objs := common/queue_module.o common/queue_context.o common/queue_dma.o queue_fops.o queue_host_msg.o queue_ctx_private.o common/queue_proc_fs.o common/queue_status_record.o common/queue_channel.o
    else
       ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
       ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
    endif
else
    MAKE_PRODUCT := $(PRODUCT)
    ifeq ($(DAVINCI_HIAI_DKMS),y)

    else
        ccflags-y += -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
        ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
        ccflags-y += -DDRV_HOST
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc \
            -I$(DRIVER_KERNEL_DIR)/inc \
            -I$(DRIVER_KERNEL_DIR)/inc/dbl \
            -I$(DRIVER_KERNEL_DIR)/src/queue/host/common \
            -I$(DRIVER_KERNEL_DIR)/src/queue/host \
            -I$(TOP_DIR)/inc/driver \
            -I$(TOP_DIR)/abl/libc_sec/include \
            -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include \
            -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)

        obj-m   += ascend_queue.o    # host
        ascend_queue-objs := common/queue_module.o common/queue_context.o common/queue_dma.o common/queue_proc_fs.o common/queue_status_record.o queue_fops.o queue_host_msg.o queue_ctx_private.o common/queue_channel.o
    endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

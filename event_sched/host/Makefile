##########################################################################
# 1. ascend_event_sched_host.ko in host
###########################################################################
ifeq ($(TARGET_BUILD_TYPE),debug)
    ccflags-y += -DCFG_BUILD_DEBUG
endif

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
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver/
        ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/
        ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)

        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            ccflags-y += -DCFG_FEATURE_HOST_LOG
            ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif

        ifneq ($(filter $(PRODUCT),  helper310p),)
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += comm/esched_table_intf.o comm/soft_sched/irq_sched/esched_table.o comm/soft_sched/irq_sched/esched_table_proc_fs.o
            ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/soft_sched/irq_sched/
            ccflags-y += -DESCHED_HOST
            ccflags-y += -DCFG_FEATURE_DEEPER_EVENT_QUE
        else
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += esched_host_msg.o

            ccflags-y += -DESCHED_HOST
            ccflags-y += -DCFG_FEATURE_IDENTIFY_CP -DCFG_FEATURE_REMOTE_SUBMIT -DCFG_FEATURE_EXTERNAL_CDEV
            ifeq ($(TARGET_CHIP_ID), hi1980b)
                ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/
                ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/
                ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                ccflags-y += -I$(HIAI_DKMS_DIR)/rms/trs_drv/inc/
                ccflags-y += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY -DCFG_FEATURE_HARDWARE_MIA

                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif

            ifeq ($(TARGET_CHIP_ID), hi1910b)
                ccflags-y += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY
                ccflags-y += -DCFG_FEATURE_SIA_MAP_TOPIC_TABLE
                ccflags-y += -DCFG_SOC_PLATFORM_MINIV3
                ccflags-y += -I$(HIAI_DKMS_DIR)/rms/trs_drv/inc/
                ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/
                ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/
                ccflags-y += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif
            ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
                ccflags-y += -DCFG_FEATURE_HOST_LOG

                ifneq ($(filter $(PRODUCT),  helper310p),)
                else
                    ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
                endif
            endif
        endif
    endif
else
    ifeq ($(DAVINCI_HIAI_DKMS),y)

    else
        ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char  -Wformat=2 -Wstack-usage=2048 -Wcast-align
        ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/
        ccflags-y += -I${DRIVER_USER_DIR}/src/event_sched/
        ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/
        ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        ccflags-y += -I$(TOP_DIR)/inc/driver/
        ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)

        ifneq ($(filter $(PRODUCT),  helper310p),)
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += comm/esched_table_intf.o comm/soft_sched/irq_sched/esched_table.o comm/soft_sched/irq_sched/esched_table_proc_fs.o
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/soft_sched/irq_sched/
            ccflags-y += -DESCHED_HOST
            ccflags-y += -DCFG_FEATURE_DEEPER_EVENT_QUE
        else
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += esched_host_msg.o

            ccflags-y += -DESCHED_HOST
            ccflags-y += -DCFG_FEATURE_IDENTIFY_CP -DCFG_FEATURE_REMOTE_SUBMIT -DCFG_FEATURE_EXTERNAL_CDEV

            ifneq ($(filter $(PRODUCT),  ascend910B),)
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/rms/trs_drv/inc/
                ccflags-y += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY -DCFG_FEATURE_HARDWARE_MIA
                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif

            ifneq ($(filter $(PRODUCT),  ascend310B ascend310Besl ascend310Bemu),)
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/rms/trs_drv/inc/
                ccflags-y += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY
                ccflags-y += -DCFG_FEATURE_SIA_MAP_TOPIC_TABLE
                ccflags-y += -DCFG_SOC_PLATFORM_MINIV3
                ifneq ($(filter $(PRODUCT),  ascend310Besl),)
                    ccflags-y += -DCFG_SOC_PLATFORM_ESL
                endif
                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif
        endif
    endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

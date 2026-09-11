##########################################################################
# 1. ascend_event_sched_host.ko in host
###########################################################################
ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wno-missing-prototypes -Wno-missing-declarations

EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
ifneq ($(filter $(PRODUCT), ascend310B ascend310p ascend910 ascend910B),)
    EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver/
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include/arch/$(ARCH)

        ifneq ($(filter $(TARGET_CHIP_ID), hi1910b hi1980 hi1980b),)
            EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
            EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
        endif

        ifneq ($(filter $(PRODUCT),  helper310p),)
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += comm/esched_table_intf.o comm/soft_sched/irq_sched/esched_table.o comm/soft_sched/irq_sched/esched_table_proc_fs.o
            EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/soft_sched/irq_sched/
            EXTRA_CFLAGS += -DESCHED_HOST
            EXTRA_CFLAGS += -DCFG_FEATURE_DEEPER_EVENT_QUE
        else
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += esched_host_msg.o

            EXTRA_CFLAGS += -DESCHED_HOST
            EXTRA_CFLAGS += -DCFG_FEATURE_IDENTIFY_CP -DCFG_FEATURE_REMOTE_SUBMIT -DCFG_FEATURE_EXTERNAL_CDEV
            ifeq ($(TARGET_CHIP_ID), hi1980b)
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/rms/trs_drv/inc/
                EXTRA_CFLAGS += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY -DCFG_FEATURE_HARDWARE_MIA

                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif

            ifeq ($(TARGET_CHIP_ID), hi1910b)
                EXTRA_CFLAGS += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY
                EXTRA_CFLAGS += -DCFG_FEATURE_SIA_MAP_TOPIC_TABLE
                EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/rms/trs_drv/inc/
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/
                EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif
            ifeq ($(TARGET_CHIP_ID), $(ASCEND_FLORENCE_DC_V10))
                EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG

                ifneq ($(filter $(PRODUCT),  helper310p),)
                else
                    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
                endif
            endif
        endif
    endif
else
    ifeq ($(DAVINCI_HIAI_DKMS),y)

    else
        ccflags-y += $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char  -Wformat=2 -Wstack-usage=2048 -Wcast-align
        ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body

        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/
        EXTRA_CFLAGS += -I${DRIVER_USER_DIR}/src/event_sched/
        EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc/
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
        EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver/
        EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include/arch/$(ARCH)

        ifneq ($(filter $(PRODUCT),  helper310p),)
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += comm/esched_table_intf.o comm/soft_sched/irq_sched/esched_table.o comm/soft_sched/irq_sched/esched_table_proc_fs.o
            EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/soft_sched/irq_sched/
            EXTRA_CFLAGS += -DESCHED_HOST
            EXTRA_CFLAGS += -DCFG_FEATURE_DEEPER_EVENT_QUE
        else
            obj-m += ascend_event_sched_host.o
            ascend_event_sched_host-objs := comm/esched_fops.o comm/esched.o comm/esched_sysfs.o
            ascend_event_sched_host-objs += esched_host_msg.o

            EXTRA_CFLAGS += -DESCHED_HOST
            EXTRA_CFLAGS += -DCFG_FEATURE_IDENTIFY_CP -DCFG_FEATURE_REMOTE_SUBMIT -DCFG_FEATURE_EXTERNAL_CDEV

            ifneq ($(filter $(PRODUCT),  ascend910B),)
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/rms/trs_drv/inc/
                EXTRA_CFLAGS += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY -DCFG_FEATURE_HARDWARE_MIA
                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif

            ifneq ($(filter $(PRODUCT),  ascend310B ascend310Besl ascend310Bemu),)
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/event_sched/host/comm/hard_sched/soc/topic_sched_v1/
                EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/rms/trs_drv/inc/
                EXTRA_CFLAGS += -DCFG_FEATURE_HARDWARE_SCHED -DCFG_FEATURE_SOFT_NON_SCHED_ONLY
                EXTRA_CFLAGS += -DCFG_FEATURE_SIA_MAP_TOPIC_TABLE
                EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3
                ifneq ($(filter $(PRODUCT),  ascend310Besl),)
                    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_ESL
                endif
                ascend_event_sched_host-objs += esched_drv_adapt.o comm/hard_sched/esched_drv.o
                ascend_event_sched_host-objs += comm/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
            endif
        endif
    endif
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

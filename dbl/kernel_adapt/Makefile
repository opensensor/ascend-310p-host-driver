ifeq ($(TOP_DIR),)
    KERNEL_DIR :=$(srctree)
    # for dkms
    DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/memory
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/task
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/fs
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/system
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/base
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/pci
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/net
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/dfx
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/memory
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/task
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/fs
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/system
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/base
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/driver
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/pci
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/net
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/dfx
    EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
    EXTRA_CFLAGS += -I${TOP_DIR}/inc/driver
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
endif

ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

EXTRA_CFLAGS += -Wall -Werror

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
ccflags-y += -Wno-unused-parameter -O2 -Wno-missing-field-initializers -Wno-sign-compare -Wno-type-limits

obj-m += ascend_kernel_adapt.o

ascend_kernel_adapt-objs := ka_module_init.o event_notify_proc.o
ascend_kernel_adapt-objs += /memory/ka_memory_mng.o /memory/ka_rbtree.o /memory/ka_memory.o /memory/ka_memory_query.o
ascend_kernel_adapt-objs += ka_proc_fs.o /task/ka_task.o /fs/ka_fs.o /system/ka_system.o /base/ka_base.o
ascend_kernel_adapt-objs += /driver/ka_driver.o /pci/ka_pci.o /net/ka_net.o /dfx/ka_dfx.o

ifeq ($(PRODUCT_SIDE), device)
    ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910 ascend910B),)
        EXTRA_CFLAGS += -DCFG_FEATURE_KA_EVENT_NOTIFY_PROC
    endif

    ascend_kernel_adapt-objs += /memory/ka_memory_device.o
else

endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

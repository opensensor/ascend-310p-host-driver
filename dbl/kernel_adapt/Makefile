ifeq ($(TOP_DIR),)
    KERNEL_DIR :=$(srctree)
    # for dkms
    DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc/dbl
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/memory
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/task
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/fs
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/system
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/base
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/pci
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/net
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/kernel_adapt/dfx
    ccflags-y += -I$(HIAI_DKMS_DIR)/libc_sec/include
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc/dbl
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/memory
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/task
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/fs
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/system
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/base
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/driver
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/pci
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/net
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/kernel_adapt/dfx
    ccflags-y += -I$(TOP_DIR)/abl/libc_sec/include
    ccflags-y += -I${TOP_DIR}/inc/driver
    ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
endif

ifeq ($(TARGET_BUILD_TYPE),debug)
    ccflags-y += -DCFG_BUILD_DEBUG
endif

ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
ccflags-y += -Wno-unused-parameter -O2 -Wno-missing-field-initializers -Wno-sign-compare -Wno-type-limits

obj-m += ascend_kernel_adapt.o

ascend_kernel_adapt-objs := ka_module_init.o event_notify_proc.o
ascend_kernel_adapt-objs += /memory/ka_memory_mng.o /memory/ka_rbtree.o /memory/ka_memory.o /memory/ka_memory_query.o
ascend_kernel_adapt-objs += ka_proc_fs.o /task/ka_task.o /fs/ka_fs.o /system/ka_system.o /base/ka_base.o
ascend_kernel_adapt-objs += /driver/ka_driver.o /pci/ka_pci.o /net/ka_net.o /dfx/ka_dfx.o

ifeq ($(PRODUCT_SIDE), device)
    ifneq ($(filter $(PRODUCT), ascend310B ascend310Brc ascend310p ascend910 ascend910B),)
        ccflags-y += -DCFG_FEATURE_KA_EVENT_NOTIFY_PROC
    endif

    ascend_kernel_adapt-objs += /memory/ka_memory_device.o
else

endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

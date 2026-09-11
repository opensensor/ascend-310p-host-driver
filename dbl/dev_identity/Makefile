ifeq ($(DAVINCI_HIAI_DKMS), y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dbl/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dbl/inc
    ccflags-y += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
endif

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wtrampolines -Wfloat-equal -Wvla -Wundef $(WDATE_TIME)
obj-m += dbl_dev_identity.o
dbl_dev_identity-objs := dev_identity_module.o

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

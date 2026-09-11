ifeq ($(DAVINCI_HIAI_DKMS), y)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dbl/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dbl/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_OPEN_DIR)/src/kernel_adapt/include
    ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
    ccflags-y += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

obj-m += dbl_algorithm.o
dbl_algorithm-objs := comm_algorithm_module.o

ifeq ($(use_hitest),kernel_true)
    export HITESTCC := $(CC)
    export HITESTLD := $(LD)
    CC = hitest_gcc
    LD = hitest_ld
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

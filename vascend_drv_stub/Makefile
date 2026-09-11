# SPDX-License-Identifier: GPL-2.0
ifeq ($(TOP_DIR),)

ifneq ($(NOT_SUPPORT_SP), y)
ccflags-y += -fstack-protector-all
endif

DVT_DIR=$(shell pwd)
UNAME=$(shell uname -r)
KERNEL_DIR=/lib/modules/$(UNAME)/build
KERNEL_SOURCE_DIR=/lib/modules/$(UNAME)/source

ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include

ifeq ($(DAVINCI_TEST),y)
    ccflags-y += -DDAVINCI_TEST
endif

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

obj-m := drv_vascend_stub.o
ifeq ($(DAVINCI_TEST),y)
	drv_vascend_stub-objs := interface_stub.o hw_dvt_stub.o
else
	drv_vascend_stub-objs := interface_stub.o hw_dvt_stub.o
endif

default:
	$(MAKE) -C $(KERNEL_DIR) M=$(DVT_DIR) modules

.PHONY:clean
clean:
	@$(RM) *.o *.ko *.mod.* Module.* .*.cmd *.order
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` $@

else

ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include

ifneq ($(NOT_SUPPORT_SP), y)
ccflags-y += -fstack-protector-all
endif

ifeq ($(DAVINCI_TEST),y)
    ccflags-y += -DDAVINCI_TEST
endif

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

obj-m := drv_vascend_stub.o
ifeq ($(DAVINCI_TEST),y)
	drv_vascend_stub-objs := interface_stub.o hw_dvt_stub.o
else
	drv_vascend_stub-objs := interface_stub.o hw_dvt_stub.o
endif

endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

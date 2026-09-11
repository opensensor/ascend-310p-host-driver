ifeq ($(TOP_DIR),)
ifeq ($(DAVINCI_HIAI_DKMS),y)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_public
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_agent
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_agent
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
endif
else
    EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_public
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_agent
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv/vpc_agent
    EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
endif

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif
EXTRA_CFLAGS += -DVIRTMNG_AGENT
EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
obj-m += drv_vpcie.o
drv_vpcie-objs := virtmngagent_pci.o virtmngagent_ctrl.o virtmngagent_unit.o
drv_vpcie-objs += virtmngagent_extension.o
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

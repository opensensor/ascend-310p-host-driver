ifeq ($(TOP_DIR),)
ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_public
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_agent
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_agent
	ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
	ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
endif
else
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_public
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_agent
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv/vpc_agent
    ccflags-y += -I$(TOP_DIR)/inc/driver
    ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
endif

ifneq ($(NOT_SUPPORT_SP), y)
    ccflags-y += -fstack-protector-all
endif
ccflags-y += -DVIRTMNG_AGENT
ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
ccflags-y += -DCFG_FEATURE_HOST_LOG

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
obj-m += drv_vpcie.o
drv_vpcie-objs := virtmngagent_pci.o virtmngagent_ctrl.o virtmngagent_unit.o
drv_vpcie-objs += virtmngagent_extension.o
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

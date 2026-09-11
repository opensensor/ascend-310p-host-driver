
ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_agent
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_agent
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_agent
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv/vpc_agent
	ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
endif

drv_vpc_agent-objs += vpc_agent/virtmngagent_msg.o vpc_agent/virtmngagent_msg_common.o vpc_agent/virtmngagent_msg_admin.o
drv_vpc_agent-objs += vpc_agent/virtmngagent_vpc.o vpc_agent/virtmngagent_vpc_unit.o
drv_vpc_agent-objs += vpc_agent/vmnga_vpc_interface.o
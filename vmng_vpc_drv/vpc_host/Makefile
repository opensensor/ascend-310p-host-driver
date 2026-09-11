ifeq ($(DAVINCI_HIAI_DKMS),y)
	ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_host
	ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_host
	ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_host/adapt
	ccflags-y += -I$(HIAI_DKMS_DIR)/vascend_drv_stub
	ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_host
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv/vpc_host
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv/vpc_host/adapt
	ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/vascend_drv/vascend_drv_stub
	ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
endif

drv_vpc_host-objs += vpc_host/virtmnghost_msg.o vpc_host/virtmnghost_msg_common.o vpc_host/virtmnghost_msg_admin.o
drv_vpc_host-objs += vpc_host/virtmnghost_vpc.o vpc_host/virtmnghost_vpc_unit.o
drv_vpc_host-objs += vpc_host/adapt/virtmnghost_resource_default.o vpc_host/adapt/virtmnghost_resource_sriov.o
drv_vpc_host-objs += vpc_host/adapt/vpc_soc_adapt.o
drv_vpc_host-objs += vpc_host/vmngh_vpc_interface.o


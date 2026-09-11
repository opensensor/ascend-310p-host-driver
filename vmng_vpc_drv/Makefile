
ifneq ($(NOT_SUPPORT_SP),y)
    ccflags-y += -fstack-protector-all
endif

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

ifeq ($(DAVINCI_HIAI_DKMS),y)
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_public
    ccflags-y += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv
    ccflags-y += -I$(HIAI_DKMS_DIR)/inc/driver
    ccflags-y += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
    ifeq (${Driver_Install_Mode},vnpu_guest)
        SERVER_TYPE := agent
    else
        SERVER_TYPE := host
    endif
else
    ccflags-y += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/inc
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_public
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv
    ccflags-y += -I$(TOP_DIR)/inc/driver
    ccflags-y += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
endif

drv_module_name := drv_vpc_${SERVER_TYPE}
obj-m += ${drv_module_name}.o
${drv_module_name}-objs := virtmng_msg_pub.o virtmng_msg_common.o virtmng_msg_admin.o virtmng_extension.o
${drv_module_name}-objs += virtmng_stack.o virtmng_resource.o

ifeq ($(DAVINCI_HIAI_DKMS),y)
    ifeq ($(SERVER_TYPE), host)
    include $(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_host/Makefile
    else
    include $(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_agent/Makefile
    endif
else
    ifeq ($(SERVER_TYPE), host)
    include ${DRIVER_KERNEL_DIR}/src/virtmng/vmng_vpc_drv/vpc_host/Makefile
    else
    include ${DRIVER_KERNEL_DIR}/src/virtmng/vmng_vpc_drv/vpc_agent/Makefile
    endif
endif

ccflags-y += -DCFG_FEATURE_KA_ALLOC_INTERFACE
ccflags-y += -DCFG_FEATURE_HOST_LOG
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

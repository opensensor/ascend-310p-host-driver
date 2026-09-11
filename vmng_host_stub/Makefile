
ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y)
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_public
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -DVIRTMNG_HOST
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
	else
	CUR_MAKEFILE_PATH := $(strip \
		$(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
		$(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
		)
	EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_inc
	EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../vmng_public
	EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
	EXTRA_CFLAGS += -DVIRTMNG_HOST
	endif
else
	ifeq ($(DAVINCI_HIAI_DKMS),y)
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_public
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
		EXTRA_CFLAGS += -DVIRTMNG_HOST
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
	else
	EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_public
	EXTRA_CFLAGS += -DVIRTMNG_HOST
	EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
	endif
endif

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif
ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-empty-body
obj-m += drv_virtmng_host_stub.o
drv_virtmng_host_stub-objs := virtmnghost_ctrl_stub.o virtmnghost_msg_common_stub.o virtmnghost_vpc_stub.o
EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

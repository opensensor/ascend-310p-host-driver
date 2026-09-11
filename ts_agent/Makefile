# ################################################################################################################
# AR20241204266067 ts_agent不能直接包含除<linux/types.h>外的内核头文件
# ################################################################################################################
ifneq ($(TARGET_SYSTEM_NAME), KpStars)
TMP_TOP_DIR :=
TMP_CURRENT_DIR :=
TMP_DEV_INC_DIR :=
TMP_OPEN_INC_DIR :=
TMP_LIBC_SEC_BASE_DIR :=
TMP_OPEN_KA_INC_DIR :=
STARS_FLAG = n
ifneq ($(TOP_DIR),)
    TMP_TOP_DIR = $(TOP_DIR)
    TMP_CURRENT_DIR = $(TMP_TOP_DIR)/drivers/ts_agent
    TMP_DEV_INC_DIR = $(TMP_TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/inc
    TMP_OPEN_INC_DIR = $(TMP_TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space_open/inc
    TMP_LIBC_SEC_BASE_DIR = $(TMP_TOP_DIR)/abl
    TMP_OPEN_KA_INC_DIR = $(TMP_TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
else ifeq ($(DAVINCI_HIAI_DKMS), y)
    TMP_TOP_DIR = $(HIAI_DKMS_DIR)
    TMP_CURRENT_DIR = $(TMP_TOP_DIR)/ts_agent
    TMP_DEV_INC_DIR = $(TMP_TOP_DIR)/dev_inc/inc
    TMP_OPEN_INC_DIR = $(TMP_TOP_DIR)/dev_inc_open/inc
    TMP_OPEN_KA_INC_DIR = $(TMP_TOP_DIR)/kernel_adapt/include
    TMP_LIBC_SEC_BASE_DIR = $(TMP_TOP_DIR)
    ifeq ($(TARGET_CHIP_ID), hi1980b)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_STARS
        STARS_FLAG = y
    endif
    ifeq ($(TARGET_CHIP_ID), hi1910b)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_STARS
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3_EP
        STARS_FLAG = y
    endif
else
    TMP_CURRENT_DIR = $(strip \
        $(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
        $(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
    )
    TMP_TOP_DIR = $(TMP_CURRENT_DIR)/../../
    TMP_DEV_INC_DIR = $(TMP_TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/inc
    TMP_OPEN_INC_DIR = $(TMP_TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space_open/inc
    TMP_LIBC_SEC_BASE_DIR = $(TMP_TOP_DIR)/abl
    TMP_OPEN_KA_INC_DIR = $(TMP_TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
endif

ifneq ($(filter $(PRODUCT), ascend910B ascend920esl ascend920emu),)
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_STARS
    STARS_FLAG = y
else ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl ascend310Bemu ascend310Brc ascend310Brcesl ascend310Brcemu as31xm1),)
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_STARS
    STARS_FLAG = y
    ifneq ($(filter $(PRODUCT), ascend310B ascend310Besl ascend310Bemu),)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3_EP
    endif
    ifneq ($(filter $(PRODUCT), ascend310Brc ascend310Brcesl ascend310Brcemu as31xm1),)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MINIV3_RC
    endif
else
endif

ifeq ($(DAVINCI_HIAI_DKMS), y)
    EXTRA_CFLAGS += -DCFG_HOST_ENV
    EXTRA_CFLAGS += -I$(TMP_OPEN_INC_DIR)
    EXTRA_CFLAGS += -I$(TMP_DEV_INC_DIR)
else
ifeq ($(PRODUCT_SIDE), host)
    EXTRA_CFLAGS += -DCFG_HOST_ENV
    EXTRA_CFLAGS += -I$(TMP_OPEN_INC_DIR)
    EXTRA_CFLAGS += -I$(TMP_DEV_INC_DIR)
else
    EXTRA_CFLAGS += -DCFG_DEVICE_ENV
    EXTRA_CFLAGS += -I$(TMP_DEV_INC_DIR)
    EXTRA_CFLAGS += -I$(TMP_OPEN_INC_DIR)
endif
endif

EXTRA_CFLAGS += -I$(TMP_CURRENT_DIR)/inc/ \
                -I$(TMP_TOP_DIR) \
                -I$(TMP_TOP_DIR)/inc \
                -I$(TMP_TOP_DIR)/inc/driver \
                -I$(TMP_TOP_DIR)/ace/csruntime/inc \
                -I$(TMP_OPEN_KA_INC_DIR) \
                -I$(TMP_OPEN_KA_INC_DIR)/ka_arch/$(ARCH) \
                -I$(TMP_LIBC_SEC_BASE_DIR)/libc_sec/include/

ccflags-y += -Wall -Werror
obj-m := ts_agent.o

ifeq ($(STARS_FLAG),y)
    ifeq ($(DAVINCI_HIAI_DKMS), y)
        EXTRA_CFLAGS += -I$(TMP_TOP_DIR)/rms/trs_drv/inc/
    else
        EXTRA_CFLAGS += -I$(TMP_TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/rms/trs_drv/inc/
    endif
ts_agent-objs := src/ts_agent_module.o \
                 src/ts_agent_update_sqe.o
else
ts_agent-objs := src/ts_agent_module.o \
                 src/ts_agent_interface.o \
                 src/ts_agent_resource.o \
                 src/ts_agent_vsq_worker.o \
                 src/ts_agent_vsq_proc.o
endif
else
KERNELDIR  := $(KDIR)
MODULE_DIR := $(shell pwd)

CONFIG_TS_AGENT = m

# Add security build options
ccflags-y += -pipe -Wstrict-prototypes -Wtrampolines -Wshadow
ccflags-y += -D_FORTIFY_SOURCE=2
ccflags-y += -fvisibility=hidden -freg-struct-return -fstrong-eval-order -fsigned-char
ccflags-y += -Wformat=2 -Wfloat-equal -Wextra -Wcast-qual -Wcast-align
ccflags-y += -Wdate-time -Wswitch-default -Wconversion -Wunused
ccflags-y += -Wl,-z,relro,-z,now
ccflags-y += -Wl,-z,noexecstack

ccflags-y += -DCFG_SOC_PLATFORM_CLOUD_V2
ccflags-y += -DCFG_SOC_PLATFORM_STARS
ccflags-y += -DCFG_DEVICE_ENV
ccflags-y += -DCFG_SOC_PLATFORM_KPSTARS

ccflags-y +=    -I$(src)/inc
ccflags-y +=    -I${src}
ccflags-y +=    -I${TOP_DIR}/component/stars_sdk/inc/
ccflags-y +=    -I${TOP_DIR}/component/stars_sdk/kernel/inc/
ccflags-y +=    -I${TOP_DIR}/component/stars_sdk/kernel/src/rms/trs_drv/inc
ccflags-y +=    -I${TOP_DIR}/component/stars_sdk/user/kernel/inc/
ccflags-y +=    -I${src}/../driver
ccflags-y +=    -I${TOP_DIR}/open_source/huawei_secure_c/include
ccflags-y +=    -I${TOP_DIR}/open_source/openeuler/kernel_5.10/drivers/infiniband/hw/hns
obj-$(CONFIG_TS_AGENT) += ts_agent.o

ts_agent-y := src/ts_agent_module.o src/ts_agent_update_sqe.o src/ts_agent_kstars_stub.o src/ts_agent_kp_update_sqe.o

all:
	$(MAKE) modules -C $(KERNELDIR) M=$(MODULE_DIR)

clean:
	$(MAKE) clean -C $(KERNELDIR) M=$(MODULE_DIR)
endif
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

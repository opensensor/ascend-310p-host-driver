
ifeq ($(TOP_DIR),)

    ifneq ($(NOT_SUPPORT_SP), y)
        EXTRA_CFLAGS += -fstack-protector-all
    endif

    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_public
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_host
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_host/soc_adapt
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_host
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vascend_drv_stub
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        EXTRA_CFLAGS += -DVIRTMNG_HOST
    else
        CUR_MAKEFILE_PATH := $(strip \
            $(eval LOCAL_MODULE_MAKEFILE := $$(lastword $$(MAKEFILE_LIST))) \
            $(patsubst %/,%, $(dir $(LOCAL_MODULE_MAKEFILE))) \
            )
        EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../../drivers/dev_inc
        EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../vmng_public
        EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../vmng_host
        EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../vmng_host/soc_adapt
        EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../vascend_drv/vascend_drv_stub
        EXTRA_CFLAGS += -DVIRTMNG_HOST
        ifeq ($(TARGET_BUILD_TYPE),debug)
            EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
            EXTRA_CFLAGS += -I$(CUR_MAKEFILE_PATH)/../../../unreleased/debug
        endif
    endif
else
    ifneq ($(NOT_SUPPORT_SP), y)
        EXTRA_CFLAGS += -fstack-protector-all
    endif

    ifeq ($(DAVINCI_HIAI_DKMS),y)
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc_open/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_public
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_host
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_vpc_drv/vpc_host
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vmng_host/soc_adapt
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/vascend_drv_stub
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
        EXTRA_CFLAGS += -DVIRTMNG_HOST
    else
        EXTRA_CFLAGS += -I$(DRIVER_OPEN_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_public
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_vpc_drv/vpc_host
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_host
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/virtmng/vmng_host/soc_adapt
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/vascend_drv/vascend_drv_stub
        EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
        EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include
        EXTRA_CFLAGS += -DVIRTMNG_HOST
        ifeq ($(TARGET_BUILD_TYPE),debug)
            EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
            EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/unreleased/debug
        endif
    endif
endif

ifeq ($(TARGET_PRODUCT),cloud)
    ifeq ($(TARGET_CHIP_ID), hi1980b)
        EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
    else
        EXTRA_CFLAGS += -DCFG_FEATURE_VDEV_NOT_SUPPROT_SMP
    endif
endif

ifneq ($(filter $(PRODUCT),  ascend910B),)
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2
endif

ifneq ($(filter $(PRODUCT),  ascend910),)
    EXTRA_CFLAGS += -DCFG_FEATURE_VDEV_NOT_SUPPROT_SMP
endif

EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align
ccflags-y += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-empty-body
obj-m += drv_virtmng_host.o
drv_virtmng_host-objs := virtmnghost_pci.o virtmnghost_ctrl.o virtmnghost_unit.o
drv_virtmng_host-objs += virtmnghost_external.o virtmnghost_sysfs.o virtmnghost_proc_fs.o
drv_virtmng_host-objs += soc_adapt/virtmng_res_drv.o soc_adapt/virtmng_res_cloud_v1.o soc_adapt/virtmng_res_cloud_v2.o soc_adapt/virtmng_res_mini_v2.o
ccflags-y += -Wno-type-limits -Wno-unused-variable -Wno-unused-function

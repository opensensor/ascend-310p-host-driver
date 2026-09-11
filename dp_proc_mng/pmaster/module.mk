
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_dp_proc_mng_host

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
LOCAL_DEPEND_KO := drv_pcie_host drv_devmng_host drv_davinci_intf_host drv_virtmng_host
else
LOCAL_DEPEND_KO := drv_pcie_host drv_devmng_host drv_davinci_intf_host
endif
LOCAL_INSTALLED_KO_FILES := drv_dp_proc_mng_host.ko

include $(BUILD_HOST_KO)


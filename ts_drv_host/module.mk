LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_devdrv_host

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
ifeq ($(TARGET_CHIP_ID),$(ASCEND_FLORENCE_DC_V10))
LOCAL_DEPEND_KO:= drv_seclib_host drv_virtmng_host drv_pcie_host drv_davinci_intf_host
else
LOCAL_DEPEND_KO:= drv_seclib_host drv_pcie_host drv_davinci_intf_host
endif
LOCAL_INSTALLED_KO_FILES := drv_devdrv_host.ko

include $(BUILD_HOST_KO)

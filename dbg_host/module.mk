
LOCAL_PATH := $(call my-dir)

#compile for host
include $(CLEAR_VARS)

LOCAL_MODULE := drv_dbg_host

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_DEPEND_KO:=drv_devdrv_host drv_devmng_host drv_pcie_hdc_host
LOCAL_INSTALLED_KO_FILES := drv_dbg_host.ko

include $(BUILD_HOST_KO)

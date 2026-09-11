LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ts_debug
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_INSTALLED_KO_FILES := ts_debug.ko

include $(CLEAR_VARS)
LOCAL_MODULE := debug_switch
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_INSTALLED_KO_FILES := debug_switch.ko

include $(BUILD_HOST_KO)
LOCAL_DEPEND_KO := ascend_trs_pm_adapt ascend_uda ascend_trs_nvme_chan ascend_trs_core drv_pcie_host ascend_kernel_open_adapt

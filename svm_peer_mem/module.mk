LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_svm_peermem

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_DEPEND_KO := drv_devmm_host
LOCAL_INSTALLED_KO_FILES := drv_svm_peermem.ko

include $(BUILD_HOST_KO)

ifeq ($(build_device), true)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_svm_peermem

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_DEPEND_KO := drv_devmm
LOCAL_INSTALLED_KO_FILES := drv_svm_peermem.ko

include $(BUILD_DEVICE_KO)

endif


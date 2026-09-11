
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_vhdc

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)

LOCAL_DEPEND_KO := drv_vpcie

LOCAL_INSTALLED_KO_FILES := drv_vhdc.ko

include $(BUILD_HOST_KO)

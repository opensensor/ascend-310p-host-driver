LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ascend_uda
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
LOCAL_INSTALLED_KO_FILES := ascend_uda.ko

ifeq ($(PRODUCT_SIDE), device)
include $(BUILD_DEVICE_KO)
else
LOCAL_DEPEND_KO:=drv_seclib_host
include $(BUILD_HOST_KO)
endif

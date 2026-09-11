LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ascend_kernel_open_adapt

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)

LOCAL_INSTALLED_KO_FILES := ascend_kernel_open_adapt.ko

ifeq ($(build_device), true)
PRODUCT_SIDE := device
include $(BUILD_DEVICE_KO)
else
PRODUCT_SIDE := host
include $(BUILD_HOST_KO)
endif


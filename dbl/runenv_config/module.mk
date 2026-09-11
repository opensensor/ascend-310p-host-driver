LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := dbl_runenv_config

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)

LOCAL_INSTALLED_KO_FILES := dbl_runenv_config.ko

ifeq ($(PRODUCT_SIDE), device)
include $(BUILD_DEVICE_KO)
else
include $(BUILD_HOST_KO)
endif


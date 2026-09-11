LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_vpc_host

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)

LOCAL_INSTALLED_KO_FILES := drv_vpc_host.ko

export SERVER_TYPE=host

include $(BUILD_HOST_KO)

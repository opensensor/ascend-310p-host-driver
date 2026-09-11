# SPDX-License-Identifier: GPL-2.0
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_vascend_stub

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)

LOCAL_INSTALLED_KO_FILES := drv_vascend_stub.ko

include $(BUILD_HOST_KO)

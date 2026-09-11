LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ts_agent

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
ifeq ($(PRODUCT_SIDE), host)
LOCAL_DEPEND_KO := drv_devdrv_host drv_devmng_host
else
    ifneq ($(filter ascend310B ascend310Brc ascend910B as31xm1, $(PRODUCT)), )
        LOCAL_DEPEND_KO := drv_devmng
    else
        LOCAL_DEPEND_KO := drv_devdrv drv_devmng
    endif
endif
LOCAL_INSTALLED_KO_FILES := ts_agent.ko

ifeq ($(PRODUCT_SIDE), host)
include $(BUILD_HOST_KO)
else
include $(BUILD_DEVICE_KO)
endif


LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := drv_pcie_vnic_host

ifeq ($(DAVINCI_HIAI_DKMS),y)
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)/../../../../drivers/unreleased/pcivnic/pcievnic_host
else
ifeq ($(strip $(TARGET_BUILD_TYPE)),debug)
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)/../../../../drivers/unreleased/pcivnic/pcievnic_host
else
LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
endif
endif

LOCAL_DEPEND_KO := drv_pcie_host drv_seclib_host

LOCAL_INSTALLED_KO_FILES := drv_pcie_vnic_host.ko

include $(BUILD_HOST_KO)

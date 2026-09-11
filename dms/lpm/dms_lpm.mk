ifeq ($(TOP_DIR),)
else #for CMake
    include $(FEATURE_MK_PATH)
    ccflags-y += $(CONFIG_DEFINES)

    ifeq ($(PRODUCT_SIDE), host)
    else
        ifneq ($(filter $(PRODUCT), ascend910 ascend910B),)
            ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/lpm
            $(MODULE_NAME)-y += lpm/dms_lpm_pass_through_mcu.o
            ccflags-y += -DCFG_FEATURE_PASS_THROUGH_MCU_BY_IMU
        endif
    endif
endif

ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
	endif
else #for CMake & ctrl cpu open
    ifeq ($(PRODUCT_SIDE), device)
        ifneq ($(filter $(PRODUCT), ascend310, ascend310rc, ascend310Brc ascend310B as31xm1 ascend310p helper310p ascend610 bs9sx1a ascend610Lite ascend610Liteesl ascend910 ascend910B),)
             ascend_dms_smf-y += report/dfm_dev_register.o
             ascend_dms_smf-y += report/dfm_report.o
             ascend_dms_smf-y += report/dfm_safety_report.o
            EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/fpdc

            ifeq ($(strip $(TARGET_BUILD_TYPE)),debug)
                EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
            endif
        endif
    endif
endif

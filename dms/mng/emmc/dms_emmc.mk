ifeq ($(TOP_DIR),)
else #for CMake & ctrl cpu open
    ascend_dms_mng-objs += emmc/dms_emmc_info.o
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/mng/emmc
endif

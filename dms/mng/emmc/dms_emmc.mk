ifeq ($(TOP_DIR),)
else #for CMake & ctrl cpu open
    ascend_dms_mng-objs += emmc/dms_emmc_info.o
    ccflags-y += -I$(DRIVER_KERNEL_DIR)/src/dms/mng/emmc
endif

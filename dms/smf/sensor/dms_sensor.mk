ifeq ($(TOP_DIR),)
    ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
        ascend_dms_smf-y += sensor/dms_sensor.o
        ascend_dms_smf-y += sensor/dms_sensor_notify.o
        ascend_dms_smf-y += sensor/dms_sensor_statis.o
        ascend_dms_smf-y += sensor/dms_sensor_discrete.o
        ascend_dms_smf-y += sensor/dms_sensor_general.o
        ascend_dms_smf-y += sensor/config/dms_sensor_type.o
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/sensor
        EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/smf/sensor/config
    endif
else #for CMake & ctrl cpu open

    ascend_dms_smf-y += sensor/dms_sensor.o
    ascend_dms_smf-y += sensor/dms_sensor_notify.o
    ascend_dms_smf-y += sensor/dms_sensor_statis.o
    ascend_dms_smf-y += sensor/dms_sensor_discrete.o
    ascend_dms_smf-y += sensor/dms_sensor_general.o
    ascend_dms_smf-y += sensor/config/dms_sensor_type.o

    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/smf/sensor/config
endif

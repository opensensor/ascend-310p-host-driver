ifeq ($(TOP_DIR),)
	ifeq ($(DAVINCI_HIAI_DKMS),y) #for DKMS
		drv_devmng_host-y += sensor/dms_sensor.o
		drv_devmng_host-y += sensor/dms_sensor_notify.o
		drv_devmng_host-y += sensor/dms_sensor_statis.o
		drv_devmng_host-y += sensor/dms_sensor_discrete.o
		drv_devmng_host-y += sensor/dms_sensor_general.o
		drv_devmng_host-y += sensor/config/dms_sensor_type.o
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/sensor
		EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dms/sensor/config
	endif
else #for CMake & ctrl cpu open
	ifeq ($(PRODUCT_SIDE), host)
		drv_devmng_host-y += sensor/dms_sensor.o
		drv_devmng_host-y += sensor/dms_sensor_notify.o
		drv_devmng_host-y += sensor/dms_sensor_statis.o
		drv_devmng_host-y += sensor/dms_sensor_discrete.o
		drv_devmng_host-y += sensor/dms_sensor_general.o
		drv_devmng_host-y += sensor/config/dms_sensor_type.o
	else
		$(MODULE_NAME)-y += sensor/dms_sensor.o
		$(MODULE_NAME)-y += sensor/dms_sensor_notify.o
		$(MODULE_NAME)-y += sensor/dms_sensor_statis.o
		$(MODULE_NAME)-y += sensor/dms_sensor_discrete.o
		$(MODULE_NAME)-y += sensor/dms_sensor_general.o
		$(MODULE_NAME)-y += sensor/config/dms_sensor_type.o
	endif
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/sensor
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/dms/sensor/config
endif

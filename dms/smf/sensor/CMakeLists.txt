target_sources(ascend_dms_smf_obj PRIVATE
    dms_sensor_discrete.c
    dms_sensor_general.c
    dms_sensor_statis.c
    dms_sensor.c
    dms_sensor_notify.c
)

add_subdirectory(config)
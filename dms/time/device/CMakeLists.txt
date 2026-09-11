if (AOSCORE_LLVM_MUSL)
    target_sources(drv_devmng_obj PRIVATE
        dms_device_time_zone.c
    )
endif()
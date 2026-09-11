if (AOSCORE_LLVM_MUSL)
    target_sources(drv_devmng_obj PRIVATE
        dms_pid_map.c
    )

    target_include_directories(drv_devmng_obj PRIVATE
        ${DRIVER_KERNEL_DIR}/src/dbl/dev_urd
    )
endif()

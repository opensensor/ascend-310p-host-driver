set(ascend_dms_smf_depends )
list(APPEND MAKE_ARGS
    KBUILD_MODPOST_WARN=1
)

if(AOSCORE_LLVM_MUSL)
    add_library(ascend_dms_smf_obj OBJECT)
    set(DEVICE_LOCAL_MODULE ascend_dms_smf)
    set(TARGET_KO_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
	add_subdirectory(event)
	add_subdirectory(core)
	add_subdirectory(sensor)
	add_subdirectory(report)
    target_sources(ascend_dms_smf_obj PRIVATE
        dms_smf_init.c
        ${DRIVER_KERNEL_DIR}/src/dms/smf/logdrv/log_drv_write_ring_buffer.c
    )

    target_compile_definitions(ascend_dms_smf_obj PRIVATE
        AOS_LLVM_BUILD
        CFG_SOC_PLATFORM_MDC_V51
        CFG_SUPPORT_DEFAULT_SEVERITY
        COMMON_FUNC_PREFIX=sils
        CFG_FEATURE_MDC_CLOCKID_CONFIG
    )

    target_link_libraries(ascend_dms_smf_obj PRIVATE
        $<BUILD_INTERFACE:utils_intf_pub>
    )

    target_include_directories(ascend_dms_smf_obj PRIVATE
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/abl/bbox/inc/bbox
        ${TOP_DIR}/abl/ascend_hal/user_space/src/dms
        ${TOP_DIR}/abl/libc_sec/include
        ${DRIVER_KERNEL_DIR}/inc
        ${DRIVER_KERNEL_DIR}/inc/dbl
        ${DRIVER_KERNEL_DIR}/src/dms/dtm
        ${DRIVER_KERNEL_DIR}/src/dms/include
        ${DRIVER_KERNEL_DIR}/src/dms/config
        ${DRIVER_KERNEL_DIR}/src/dms/smf/sensor
        ${DRIVER_KERNEL_DIR}/src/dms/smf/event
		${DRIVER_KERNEL_DIR}/src/dms/smf/core
		${DRIVER_KERNEL_DIR}/src/dbl/dev_urd
		${DRIVER_KERNEL_DIR}/src/fpdc
		${DRIVER_KERNEL_DIR}/src/common
		${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/drv_devmng/drv_devmng_inc
		${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/tsdrv/mdc/ts_drv/ts_drv_common
		${DRIVER_KERNEL_DIR}/src/tsdrv/mdc/ts_platform/ts_platform_device/ascend610
    )

    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        target_compile_definitions(ascend_dms_smf_obj PRIVATE CFG_BUILD_DEBUG)
    endif()

    add_custom_command(
        OUTPUT ${DEVICE_LOCAL_MODULE}.ko
        COMMAND ${CMAKE_LINKER} -r $<TARGET_OBJECTS:ascend_dms_smf_obj> -o ${DEVICE_LOCAL_MODULE}.ko
        DEPENDS ascend_dms_smf_obj
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )

    add_custom_target(ascend_dms_smf ALL DEPENDS ascend_urd ascend_dms_dtm ascend_dms_smf_obj ${DEVICE_LOCAL_MODULE}.ko)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DEVICE_LOCAL_MODULE}.ko
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
else()
    if (${PRODUCT_SIDE} STREQUAL host)
        set(ascend_dms_smf_depends drv_seclib_host ascend_dms_dtm ascend_urd ascend_kernel_open_adapt)
        set(product_list "ascend310B" "ascend310p" "ascend910" "ascend910B")
        if(${PRODUCT} IN_LIST product_list)
            list(APPEND ascend_dms_smf_depends ascend_logdrv ascend_kernel_adapt)
        endif()
        list(APPEND MAKE_ARGS
            KBUILD_MODPOST_WARN=1
        )

        add_host_ko(LOCAL_MODULE ascend_dms_smf
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ascend_dms_smf_depends}")
    else ()
        set(ascend_dms_smf_depends ascend_dms_dtm ascend_urd ascend_kernel_open_adapt)
        set(product_list "ascend310B" "ascend310Brc" "ascend310p" "ascend910" "ascend910B")
        if(${PRODUCT} IN_LIST product_list)
            list(APPEND ascend_dms_smf_depends ascend_kernel_adapt)
        endif()
        list(APPEND MAKE_ARGS
            KBUILD_MODPOST_WARN=1
        )
        add_device_ko(LOCAL_MODULE ascend_dms_smf
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ascend_dms_smf_depends}")
    endif()
endif()
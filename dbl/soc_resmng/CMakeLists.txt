if(AOSCORE_LLVM_MUSL)
    add_library(ascend_soc_resmng_obj OBJECT)
    set(DEVICE_LOCAL_MODULE ascend_soc_resmng)
    set(TARGET_KO_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

    target_sources(ascend_soc_resmng_obj PRIVATE
        soc_resmng.c
        soc_subsys_ts.c
    )

    target_compile_definitions(ascend_soc_resmng_obj PRIVATE
	    AOS_LLVM_BUILD
	    CFG_SOC_PLATFORM_MDC_V51
    )

    target_link_libraries(ascend_soc_resmng_obj PRIVATE
	    $<BUILD_INTERFACE:utils_intf_pub>
    )

    target_include_directories(ascend_soc_resmng_obj PRIVATE
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/abl/libc_sec/include
        ${DRIVER_KERNEL_DIR}/inc
        ${DRIVER_KERNEL_DIR}/src/common
    )

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )  # ascend610/bs9sx1a
        target_compile_definitions(ascend_soc_resmng_obj PRIVATE CFG_BUILD_DEBUG)
    endif()

    add_custom_command(
        OUTPUT ${DEVICE_LOCAL_MODULE}.ko
        COMMAND ${CMAKE_LINKER} -r $<TARGET_OBJECTS:ascend_soc_resmng_obj> -o ${DEVICE_LOCAL_MODULE}.ko
        DEPENDS ascend_soc_resmng_obj
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )

    add_custom_target(ascend_soc_resmng ALL DEPENDS ascend_soc_resmng_obj ${DEVICE_LOCAL_MODULE}.ko)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DEVICE_LOCAL_MODULE}.ko
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
else()
    set(product_list "ascend310B" "ascend910B" "ascend910" "ascend310p")

    if("${PRODUCT}" STREQUAL helper310p)
        set(ascend_soc_resmng_device_depends)
        set(ascend_soc_resmng_host_depends)
    elseif ("${PRODUCT}" IN_LIST product_list)
        set(ascend_soc_resmng_device_depends ascend_kernel_open_adapt)
        set(ascend_soc_resmng_host_depends ascend_logdrv ascend_kernel_open_adapt)
    else()
        set(ascend_soc_resmng_device_depends ascend_kernel_open_adapt)
        set(ascend_soc_resmng_host_depends ascend_kernel_open_adapt)
    endif()

    if (${PRODUCT_SIDE} STREQUAL host)
        add_host_ko(LOCAL_MODULE ascend_soc_resmng
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ascend_soc_resmng_host_depends}")
    else ()
        add_device_ko(LOCAL_MODULE ascend_soc_resmng
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ascend_soc_resmng_device_depends}")
    endif()
endif()

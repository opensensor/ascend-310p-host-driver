set(ascend_dms_mng_depends )
list(APPEND MAKE_ARGS
    KBUILD_MODPOST_WARN=1
)

if(AOSCORE_LLVM_MUSL)
    add_library(ascend_dms_mng_obj OBJECT)
    set(DEVICE_LOCAL_MODULE ascend_dms_mng)
    set(TARGET_KO_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

    target_sources(ascend_dms_mng_obj PRIVATE
        dms_mng_init.c
    )

    target_compile_definitions(ascend_dms_mng_obj PRIVATE
        AOS_LLVM_BUILD
    )

    target_link_libraries(ascend_dms_mng_obj PRIVATE
        $<BUILD_INTERFACE:utils_intf_pub>
    )

    target_include_directories(ascend_dms_mng_obj PRIVATE
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/abl/bbox/inc/bbox
        ${TOP_DIR}/abl/libc_sec/include
        ${DRIVER_KERNEL_DIR}/inc
        ${DRIVER_KERNEL_DIR}/src/dms/include
        ${DRIVER_KERNEL_DIR}/src/dms/config
    )

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )  # ascend610/bs9sx1a
        target_compile_definitions(ascend_dms_mng_obj PRIVATE CFG_BUILD_DEBUG)
    endif()

    add_custom_command(
        OUTPUT ${DEVICE_LOCAL_MODULE}.ko
        COMMAND ${CMAKE_LINKER} -r $<TARGET_OBJECTS:ascend_dms_mng_obj> -o ${DEVICE_LOCAL_MODULE}.ko
        DEPENDS ascend_dms_mng_obj
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )

    add_custom_target(ascend_dms_mng ALL DEPENDS ascend_dms_mng_obj ${DEVICE_LOCAL_MODULE}.ko)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DEVICE_LOCAL_MODULE}.ko
                DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
else()
    if (${PRODUCT_SIDE} STREQUAL host)
        set(ascend_dms_mng_depends drv_seclib_host)
        list(APPEND MAKE_ARGS
            KBUILD_MODPOST_WARN=1
        )

        add_host_ko(LOCAL_MODULE ascend_dms_mng
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ascend_dms_mng_depends}")
    else ()
        add_device_ko(LOCAL_MODULE ascend_dms_mng
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "")
    endif()
endif()

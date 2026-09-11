if (BUILD_OPEN_SRC) # for build.sh script build
    MESSAGE( STATUS "curr path = ${CMAKE_CURRENT_SOURCE_DIR}.")
    MESSAGE( STATUS "CUSTOM_KERNEL_PATH = ${CUSTOM_KERNEL_PATH}")

    function(complie_module obj)
        set(TARGET_NAME ${obj})
        if ("${CUSTOM_KERNEL_PATH}" STREQUAL "")
            set(KDIR /lib/modules/${CMAKE_HOST_SYSTEM_VERSION}/build)
        else()
            set(KDIR ${CUSTOM_KERNEL_PATH})
        endif()

        add_custom_target(${TARGET_NAME} ALL
            COMMAND echo "KDIR = ${KDIR}"
            COMMAND echo "Start complie ascend_kernel_open_adapt"
        )

        if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL aarch64)
            set(SYS_ARCH arm64)
        elseif( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL x86_64)
            set(SYS_ARCH x86)
        else()
            set(SYS_ARCH ${CMAKE_HOST_SYSTEM_PROCESSOR})
        endif()

        add_custom_command(TARGET ${TARGET_NAME}
            COMMAND ${CMAKE_MAKE_PROGRAM} -C ${KDIR} M=${CMAKE_CURRENT_SOURCE_DIR} ARCH=${SYS_ARCH} BUILD_OPEN_SRC=y ${MAKE_ARGS} DRIVER_KERNEL_OPEN_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../.. modules
            COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/${obj}.ko ${CMAKE_CURRENT_SOURCE_DIR}/../../output
            COMMAND ${CMAKE_MAKE_PROGRAM} -C ${KDIR} M=${CMAKE_CURRENT_SOURCE_DIR} ARCH=${SYS_ARCH} BUILD_OPEN_SRC=y ${MAKE_ARGS} DRIVER_KERNEL_OPEN_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../.. clean
            COMMAND rm -rf ${CMAKE_CURRENT_SOURCE_DIR}/*/*.ur-safe
        )
    endfunction()

    complie_module(ascend_kernel_open_adapt)
else() # for ci build
    if (${PRODUCT_SIDE} STREQUAL device)
        add_device_ko(LOCAL_MODULE ascend_kernel_open_adapt
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
        )
    else()
        add_host_ko(LOCAL_MODULE ascend_kernel_open_adapt
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
        )
    endif()
endif()

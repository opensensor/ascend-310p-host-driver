set(comm_algorithm_depends ascend_kernel_open_adapt)

if (${PRODUCT_SIDE} STREQUAL device)
    add_device_ko(LOCAL_MODULE dbl_algorithm
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${comm_algorithm_depends}")
else()
    add_host_ko(LOCAL_MODULE dbl_algorithm
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${comm_algorithm_depends}")
endif()


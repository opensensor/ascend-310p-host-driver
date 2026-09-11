if (${PRODUCT_SIDE} STREQUAL device)
    add_device_ko(LOCAL_MODULE ascend_kernel_adapt
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
    )
else()
    add_host_ko(LOCAL_MODULE ascend_kernel_adapt
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
    )
endif()
set(dbl_runenv_config_depends ascend_kernel_open_adapt)

if (${PRODUCT_SIDE} STREQUAL device)
    add_device_ko(LOCAL_MODULE dbl_runenv_config
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${dbl_runenv_config_depends}")
else()
    add_host_ko(LOCAL_MODULE dbl_runenv_config
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${dbl_runenv_config_depends}")
endif()


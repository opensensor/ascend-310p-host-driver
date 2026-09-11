if (${PRODUCT_SIDE} STREQUAL host)
    add_host_ko(LOCAL_MODULE ts_debug
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS})

    add_host_ko(LOCAL_MODULE debug_switch
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS})
endif()

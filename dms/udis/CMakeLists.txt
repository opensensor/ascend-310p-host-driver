if (${PRODUCT_SIDE} STREQUAL host)
    if (${PRODUCT} STREQUAL ascend910B)
        set(ascend_udis_depends drv_seclib_host ascend_kernel_open_adapt ascend_logdrv ascend_kernel_adapt drv_pcie_host drv_davinci_intf_host ascend_uda dbl_runenv_config)
    endif()
    add_host_ko(LOCAL_MODULE ascend_udis
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${ascend_udis_depends}")
else ()
    if (${PRODUCT} STREQUAL ascend910B)
        set(ascend_udis_depends ascend_kernel_adapt ascend_kernel_open_adapt drv_pcie ascend_uda)
    endif()

    add_device_ko(LOCAL_MODULE ascend_udis
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${ascend_udis_depends}")
endif()

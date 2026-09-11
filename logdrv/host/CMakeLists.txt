set(product_list "ascend310B" "ascend310p" "ascend910" "ascend910B")
if (${PRODUCT} IN_LIST product_list)
    set(ascend_logdrv_depends drv_seclib_host ascend_kernel_adapt ascend_kernel_open_adapt)
else ()
    set(ascend_logdrv_depends drv_seclib_host ascend_kernel_open_adapt)
endif ()

add_host_ko(LOCAL_MODULE ascend_logdrv
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ascend_logdrv_depends}")


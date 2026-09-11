
if (${PRODUCT_SIDE} STREQUAL host)
    if (${PRODUCT} STREQUAL helper310p)
        set(ascend_uda_depends drv_seclib_host)
    else ()
        set(ascend_uda_depends drv_seclib_host ascend_kernel_open_adapt drv_davinci_intf_host)
        set(product_list "ascend310B" "ascend310p" "ascend910" "ascend910B")
        if(${PRODUCT} IN_LIST product_list)
            list(APPEND ascend_uda_depends ascend_logdrv ascend_kernel_adapt)
        endif()
    endif()
    add_host_ko(LOCAL_MODULE ascend_uda
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${ascend_uda_depends}")
    add_host_patch_ko(LOCAL_MODULE ascendudapatch
        ORIGIN_TARGET_NAME ascend_uda
        SOURCE_FILE_LIST uda_access.c
        PATCH_FILE_LIST uda_access.patch)
else ()
    set(ascend_uda_depends ascend_kernel_open_adapt)
    set(product_list "ascend310B" "ascend310Brc" "ascend310p" "ascend910" "ascend910B")
    if(${PRODUCT} IN_LIST product_list)
        list(APPEND ascend_uda_depends ascend_kernel_adapt)
    endif()

    add_device_ko(LOCAL_MODULE ascend_uda
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${ascend_uda_depends}")
    add_device_patch_ko(LOCAL_MODULE ascendudapatch
        ORIGIN_TARGET_NAME ascend_uda
        SOURCE_FILE_LIST uda_access.c
        PATCH_FILE_LIST uda_access.patch)
endif()

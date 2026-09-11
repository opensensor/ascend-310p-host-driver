if( ${PRODUCT_SIDE} STREQUAL host)
    set(drv_svm_peermem_depends drv_seclib_host drv_devmm_host)
    add_definitions(-DCFG_SOC_PLATFORM_HOST)
    add_host_ko(LOCAL_MODULE drv_svm_peermem
                KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
                MAKE_ARGS ${MAKE_ARGS}
                TARGETE_DPENDS "${drv_svm_peermem_depends}")
else ()
    set(drv_svm_peermem_depends drv_devmm hns_roce)
    add_device_ko(LOCAL_MODULE drv_svm_peermem
                KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
                MAKE_ARGS ${MAKE_ARGS}
                TARGETE_DPENDS "${drv_svm_peermem_depends}")
endif ()

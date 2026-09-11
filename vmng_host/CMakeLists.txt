set(product_list "ascend310p" "ascend910" "ascend910B" "ascend310B" "ascend310Besl")
if (${PRODUCT} IN_LIST product_list)
        set(drv_virtmng_host_depends drv_pcie_host drv_seclib_host drv_vascend_stub drv_vpc_host ascend_uda
        ascend_soc_resmng ascend_kernel_open_adapt)
else ()
        set(drv_virtmng_host_depends drv_pcie_host drv_seclib_host ascend_uda ascend_soc_resmng ascend_kernel_open_adapt)
endif ()

set(virtmng_host_log_support_product_list "ascend910" "ascend310p" "ascend910B" "ascend310B")
if (${PRODUCT} IN_LIST virtmng_host_log_support_product_list)
	set(virtmng_host_log_support_depends ascend_logdrv)
endif ()

list(APPEND drv_virtmng_host_depends ${drv_mem_dfx_depend})

add_host_ko(LOCAL_MODULE drv_virtmng_host
    KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
    MAKE_ARGS ${MAKE_ARGS}
    TARGETE_DPENDS "${drv_virtmng_host_depends}" "${virtmng_host_log_support_depends}")


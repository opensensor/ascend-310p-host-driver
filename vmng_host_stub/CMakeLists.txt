set(drv_virtmng_host_stub_depends drv_pcie_host drv_seclib_host ascend_uda ascend_kernel_open_adapt)

set(vstub_host_log_support_product_list "ascend910" "ascend310p" "ascend910B" "ascend310B")
if (${PRODUCT} IN_LIST vstub_host_log_support_product_list)
	set(vstub_host_log_support_depends ascend_logdrv)
endif ()

list(APPEND drv_virtmng_host_stub_depends ${drv_mem_dfx_depend})

add_host_ko(LOCAL_MODULE drv_virtmng_host_stub
	KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
	MAKE_ARGS ${MAKE_ARGS}
	TARGETE_DPENDS "${drv_virtmng_host_stub_depends}" "${vstub_host_log_support_depends}")



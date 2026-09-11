set(drv_vpcie_depends drv_seclib_host drv_vpc_agent ascend_uda ascend_kernel_open_adapt)

set(vpcie_host_log_support_product_list "ascend910" "ascend310p" "ascend910B" "ascend310B")
if (${PRODUCT} IN_LIST vpcie_host_log_support_product_list)
	set(vpcie_host_log_support_depends ascend_logdrv)
endif ()

list(APPEND drv_vpcie_depends ${drv_mem_dfx_depend})

add_host_ko(LOCAL_MODULE drv_vpcie
			KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
			MAKE_ARGS ${MAKE_ARGS}
			TARGETE_DPENDS "${drv_vpcie_depends}" "${vpcie_host_log_support_depends}")

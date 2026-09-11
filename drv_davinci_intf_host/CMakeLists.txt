set(drv_davinci_intf_host_depends drv_seclib_host ascend_kernel_open_adapt)
set(product_list "ascend310B" "ascend310p" "ascend910" "ascend910B")
if(${PRODUCT} IN_LIST product_list)
	list(APPEND drv_davinci_intf_host_depends ascend_logdrv ascend_kernel_adapt)
endif()

add_host_ko(LOCAL_MODULE drv_davinci_intf_host
			KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
			MAKE_ARGS ${MAKE_ARGS}
			TARGETE_DPENDS "${drv_davinci_intf_host_depends}")

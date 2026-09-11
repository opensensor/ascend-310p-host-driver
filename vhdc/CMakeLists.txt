set(drv_vhdc_depends ascend_kernel_open_adapt drv_seclib_host drv_vpcie drv_vpc_agent ascend_uda)

list(APPEND drv_vhdc_depends ${drv_mem_dfx_depend})

add_host_ko(LOCAL_MODULE drv_vhdc
	KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
	MAKE_ARGS ${MAKE_ARGS}
	TARGETE_DPENDS "${drv_vhdc_depends}")

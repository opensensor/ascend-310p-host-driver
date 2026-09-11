set(drv_vtsdrv_depends drv_seclib_host drv_vpcie drv_vdevmng drv_davinci_intf_host drv_vpc_agent ascend_uda ascend_logdrv ascend_kernel_adapt ascend_kernel_open_adapt)
add_host_ko(LOCAL_MODULE drv_vtsdrv
	KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
	MAKE_ARGS ${MAKE_ARGS}
	TARGETE_DPENDS "${drv_vtsdrv_depends}")

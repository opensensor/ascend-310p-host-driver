set(product_list "ascend310p" "ascend910" "ascend310B" "ascend910B")
if (${PRODUCT} IN_LIST product_list)
set(drv_tsdrv_platform_host_depends drv_seclib_host drv_pcie_host drv_devmng_host drv_devdrv_host ascend_uda ascend_logdrv ascend_kernel_adapt ascend_kernel_open_adapt)
else ()
set(drv_tsdrv_platform_host_depends drv_seclib_host drv_pcie_host drv_devmng_host drv_devdrv_host ascend_uda ascend_kernel_open_adapt)
endif ()

add_host_ko(LOCAL_MODULE drv_tsdrv_platform_host
	KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
	MAKE_ARGS ${MAKE_ARGS}
	TARGETE_DPENDS "${drv_tsdrv_platform_host_depends}")


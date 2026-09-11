if ((${PRODUCT} STREQUAL as31xm1) OR (${PRODUCT} STREQUAL ascend610Lite))
	set(drv_pcie_hdc_host_depends drv_pcie_host)
	add_device_ko(LOCAL_MODULE drv_pcie_hdc_host
		KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
		MAKE_ARGS ${MAKE_ARGS}
		TARGETE_DPENDS "${drv_pcie_hdc_host_depends}")
else ()

set(product_list "ascend910" "ascend310p" "ascend910B")
if (${PRODUCT} IN_LIST product_list)
	set(drv_pcie_hdc_host_depends ascend_kernel_open_adapt drv_pcie_host drv_seclib_host drv_virtmng_host drv_vpc_host ascend_uda ascend_soc_resmng dbl_runenv_config drv_davinci_intf_host)
else ()
	if (${PRODUCT} STREQUAL ascend310)
		set(drv_pcie_hdc_host_depends ascend_kernel_open_adapt drv_pcie_host drv_seclib_host ascend_uda ascend_soc_resmng drv_devmng_host dbl_runenv_config drv_davinci_intf_host)
	else ()
		set(drv_pcie_hdc_host_depends ascend_kernel_open_adapt drv_pcie_host drv_seclib_host ascend_uda ascend_soc_resmng dbl_runenv_config drv_davinci_intf_host)
	endif()
endif ()

set(hdc_host_log_support_product_list "ascend910" "ascend310p" "ascend910B" "ascend310B")
if (${PRODUCT} IN_LIST hdc_host_log_support_product_list)
	set(hdc_host_log_support_depends ascend_logdrv)
endif ()

list(APPEND drv_pcie_hdc_host_depends ${drv_mem_dfx_depend})

add_host_ko(LOCAL_MODULE drv_pcie_hdc_host
	KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
	MAKE_ARGS ${MAKE_ARGS}
	TARGETE_DPENDS "${drv_pcie_hdc_host_depends}" "${hdc_host_log_support_depends}")

endif ()
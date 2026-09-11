if (${PRODUCT_SIDE} STREQUAL host)
	set(drv_soft_fault_depends drv_seclib_host ascend_kernel_open_adapt ascend_urd ascend_uda ascend_dms_dtm
		ascend_dms_smf drv_devmng_host drv_pcie_host drv_davinci_intf_host)
	set(product_list "ascend310B" "ascend310p" "ascend910" "ascend910B")
	if(${PRODUCT} IN_LIST product_list)
		list(APPEND drv_soft_fault_depends ascend_logdrv ascend_kernel_adapt)
	endif()
	add_host_ko(LOCAL_MODULE drv_soft_fault
		KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
		MAKE_ARGS ${MAKE_ARGS}
		TARGETE_DPENDS "${drv_soft_fault_depends}")
else ()
	set(drv_soft_fault_depends ascend_kernel_open_adapt ascend_urd ascend_uda ascend_dms_dtm ascend_dms_smf drv_devmng)
	if( ${PRODUCT} STREQUAL ascend310p)
		list(APPEND drv_soft_fault_depends drv_pcie hsm_firmware_update hsm_flash hsm_heartbeat ascend_kernel_adapt)
	elseif( ${PRODUCT} STREQUAL ascend910B)
		list(APPEND drv_soft_fault_depends drv_pcie ascend_kernel_adapt)
	elseif( ${PRODUCT} STREQUAL helper310p)
		# none append
	elseif( ${PRODUCT} STREQUAL ascend310Brc)
		set(drv_soft_fault_depends ascend_kernel_adapt)
	elseif( ${PRODUCT} STREQUAL ascend910Desl OR ${PRODUCT} STREQUAL ascend910D)
		# none append
	elseif( ${PRODUCT} STREQUAL ascend310B OR ${PRODUCT} STREQUAL ascend910)
		list(APPEND drv_soft_fault_depends drv_pcie ascend_kernel_adapt)
	else ()
		list(APPEND drv_soft_fault_depends drv_pcie)
	endif ()
	add_device_ko(LOCAL_MODULE drv_soft_fault
		KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
		MAKE_ARGS ${MAKE_ARGS}
		TARGETE_DPENDS "${drv_soft_fault_depends}")
endif ()

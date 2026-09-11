set(product_list "ascend310p" "ascend910")
if (${PRODUCT} IN_LIST product_list)
	set(drv_devdrv_host_depends drv_devmng_host drv_seclib_host drv_virtmng_host drv_pcie_host drv_pcie_hdc_host drv_davinci_intf_host drv_vpc_host ascend_uda ascend_soc_resmng
	ascend_dms_smf ascend_event_sched_host ascend_logdrv ascend_kernel_adapt ascend_kernel_open_adapt)
else ()
	set(drv_devdrv_host_depends drv_devmng_host drv_seclib_host drv_pcie_host drv_pcie_hdc_host drv_davinci_intf_host ascend_uda ascend_soc_resmng
	ascend_dms_smf ascend_event_sched_host ascend_kernel_open_adapt)
endif ()

list(APPEND MAKE_ARGS
    KBUILD_MODPOST_WARN=1
)

add_host_ko(LOCAL_MODULE drv_devdrv_host
	KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
	MAKE_ARGS ${MAKE_ARGS}
	TARGETE_DPENDS "${drv_devdrv_host_depends}")

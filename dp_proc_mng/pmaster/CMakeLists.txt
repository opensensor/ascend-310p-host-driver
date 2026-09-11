set(product_list "ascend310p" "ascend910" "ascend910B")
if (${PRODUCT} IN_LIST product_list)
    set(drv_dp_proc_mng_host_depends drv_seclib_host drv_pcie_host drv_davinci_intf_host drv_devmng_host drv_virtmng_host ascend_uda ascend_logdrv ascend_kernel_adapt ascend_kernel_open_adapt)
elseif (${PRODUCT} STREQUAL ascend310B)
	set(drv_dp_proc_mng_host_depends drv_seclib_host drv_pcie_host drv_davinci_intf_host drv_devmng_host ascend_uda ascend_logdrv ascend_kernel_adapt ascend_kernel_open_adapt)
else ()
    set(drv_dp_proc_mng_host_depends drv_seclib_host drv_pcie_host drv_davinci_intf_host drv_devmng_host ascend_uda ascend_kernel_open_adapt)
endif ()

if(DEFINED ENV{KERNEL_VERSION})
    if (${KERNEL_VERSION} STREQUAL 4.19.140)
        list(APPEND MAKE_ARGS ADAPT_KP_OS_FOR_EMU_TEST=ON)
    endif ()
endif ()

add_host_ko(LOCAL_MODULE drv_dp_proc_mng_host
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${drv_dp_proc_mng_host_depends}")

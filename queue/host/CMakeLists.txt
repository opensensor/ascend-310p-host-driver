if (("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend310B"))
    set(drv_devdrv_host)
else()
    set(drv_devdrv_host drv_devdrv_host)
endif()

set(product_list "ascend310B" "ascend310p" "ascend910" "ascend910B")
if(${PRODUCT} IN_LIST product_list)
    set(drv_queue_host_depends ascend_logdrv drv_pcie_host drv_pcie_hdc_host ${drv_devdrv_host} drv_devmm_host drv_davinci_intf_host drv_devmng_host drv_seclib_host ascend_uda ascend_event_sched_host ascend_kernel_adapt ascend_kernel_open_adapt)
else ()
    set(drv_queue_host_depends drv_pcie_host drv_pcie_hdc_host ${drv_devdrv_host} drv_devmm_host drv_davinci_intf_host drv_devmng_host drv_seclib_host ascend_uda ascend_event_sched_host ascend_kernel_open_adapt)
endif()

add_host_ko(LOCAL_MODULE ascend_queue
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${drv_queue_host_depends}")
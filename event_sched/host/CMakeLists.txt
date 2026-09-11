if (${PRODUCT} STREQUAL helper310p)
set(ascend_event_sched_host_depends drv_seclib_host ascend_kernel_open_adapt)
elseif (${PRODUCT} STREQUAL ascend910B)
set(ascend_event_sched_host_depends drv_devmng_host drv_seclib_host ascend_logdrv drv_pcie_host drv_davinci_intf_host drv_virtmng_host ascend_trs_nvme_chan ascend_uda ascend_kernel_adapt ascend_kernel_open_adapt)
elseif (${PRODUCT} STREQUAL ascend310B)
set(ascend_event_sched_host_depends drv_devmng_host drv_seclib_host ascend_logdrv drv_pcie_host drv_davinci_intf_host ascend_trs_nvme_chan ascend_uda ascend_kernel_adapt ascend_kernel_open_adapt)
elseif (${PRODUCT} STREQUAL ascend310p)
set(ascend_event_sched_host_depends drv_devmng_host drv_seclib_host ascend_logdrv drv_pcie_host drv_davinci_intf_host ascend_uda ascend_kernel_adapt ascend_kernel_open_adapt)
elseif (${PRODUCT} STREQUAL ascend910)
set(ascend_event_sched_host_depends drv_devmng_host drv_seclib_host ascend_logdrv drv_pcie_host drv_davinci_intf_host ascend_uda ascend_kernel_adapt ascend_kernel_open_adapt)
else ()
set(ascend_event_sched_host_depends drv_devmng_host drv_seclib_host drv_pcie_host drv_davinci_intf_host ascend_uda ascend_kernel_open_adapt)
endif()

add_host_ko(LOCAL_MODULE ascend_event_sched_host
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ascend_event_sched_host_depends}")

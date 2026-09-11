set(soc_scene_list "ascend310rc" "helper310p")
set(product_list "ascend310p" "ascend910" "ascend910B" "ascend310B")
if (${PRODUCT} IN_LIST soc_scene_list)
    set(ascend_xsmem_depends drv_davinci_intf drv_devmng ascend_kernel_open_adapt)
elseif (${PRODUCT} STREQUAL ascend310Brc)
    set(ascend_xsmem_depends drv_davinci_intf drv_devmng ascend_kernel_adapt ascend_kernel_open_adapt)
elseif (${PRODUCT} IN_LIST product_list)
    set(ascend_xsmem_depends drv_davinci_intf drv_devmng drv_pcie ascend_kernel_adapt ascend_kernel_open_adapt)
else ()
    set(ascend_xsmem_depends drv_davinci_intf drv_devmng drv_pcie ascend_kernel_open_adapt)
endif()

add_device_ko(LOCAL_MODULE ascend_xsmem
              KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
              MAKE_ARGS ${MAKE_ARGS}
              TARGETE_DPENDS "${ascend_xsmem_depends}")

set(product_list "ascend310p" "ascend910")
if (${PRODUCT} IN_LIST product_list)
    set(drv_vdevmm_depends drv_seclib_host drv_vtsdrv drv_devmng_host drv_vpcie drv_davinci_intf_host drv_vpc_agent ascend_uda ascend_dms_smf ascend_dms_dtm dbl_runenv_config ascend_logdrv ascend_kernel_adapt ascend_kernel_open_adapt)
endif ()

add_host_ko(
    LOCAL_MODULE        drv_vdevmm
    KO_SRC_FOLDER       ${CMAKE_CURRENT_SOURCE_DIR}
    MAKE_ARGS           ${MAKE_ARGS}
    TARGETE_DPENDS      "${drv_vdevmm_depends}"
)

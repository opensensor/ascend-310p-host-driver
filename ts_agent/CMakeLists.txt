set(stars_platform "ascend310B" "ascend310Besl" "ascend310Bemu" "ascend310Brc" "ascend310Brcesl" "ascend310Brcemu" "ascend910B" "ascend920esl" "ascend920emu" "as31xm1")
if (${PRODUCT} IN_LIST stars_platform)
    if (${PRODUCT_SIDE} STREQUAL host)
        set(stars_depends ascend_trs_pm_adapt drv_pcie_host ascend_trs_shrid)
    else ()
        set(stars_depends ascend_trs_pm_adapt)
    endif ()
endif ()

if (${PRODUCT_SIDE} STREQUAL host)
    list(APPEND MAKE_ARGS
        PRODUCT_SIDE=host
    )
    if (("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend310B"))
        set(drv_devdrv_host)
    else()
        set(drv_devdrv_host drv_devdrv_host)
    endif()
    set(ts_agent_depends ${drv_devdrv_host} drv_devmng_host drv_seclib_host drv_virtmng_host drv_devmm_host ${stars_depends} ascend_uda dbl_runenv_config ascend_kernel_open_adapt)
    add_host_ko(LOCAL_MODULE ts_agent
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${ts_agent_depends}")
else ()
    list(APPEND MAKE_ARGS
        PRODUCT_SIDE=device
    )

    if (${PRODUCT} STREQUAL ascend910B)
        set(ts_agent_depends drv_devmng ${stars_depends} ascend_uda dbl_runenv_config ascend_kernel_open_adapt)
        add_device_ko(LOCAL_MODULE ts_agent
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ts_agent_depends}")
    else ()
        set(product_list "ascend310B" "ascend310Brc" "as31xm1")
        if(${PRODUCT} IN_LIST product_list)
            set(ts_agent_depends drv_devmng ${stars_depends} ascend_uda ascend_kernel_open_adapt)
        else()
            set(ts_agent_depends drv_devdrv drv_devmng ${stars_depends} ascend_uda ascend_kernel_open_adapt)
        endif()
        add_device_ko(LOCAL_MODULE ts_agent
            KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
            MAKE_ARGS ${MAKE_ARGS}
            TARGETE_DPENDS "${ts_agent_depends}")
    endif ()
endif ()

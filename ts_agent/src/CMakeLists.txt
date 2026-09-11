list(APPEND MAKE_ARGS
    PRODUCT_SIDE=host
)
set(ts_agent_vm_depends drv_devmng_host drv_seclib_host drv_virtmng_host_stub drv_devmm_host ascend_trs_sec_eh_adapt ascend_uda dbl_runenv_config ascend_kernel_open_adapt)

add_host_ko(LOCAL_MODULE ts_agent_vm
    KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
    MAKE_ARGS ${MAKE_ARGS}
    TARGETE_DPENDS "${ts_agent_vm_depends}")

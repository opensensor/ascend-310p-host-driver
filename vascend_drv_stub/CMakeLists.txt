set(drv_vascend_stub_depends drv_seclib_host ascend_kernel_open_adapt)
add_host_ko(LOCAL_MODULE drv_vascend_stub
	OUTPUT_NAME drv_vascend_stub
	KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
	MAKE_ARGS ${MAKE_ARGS}
	TARGETE_DPENDS "${drv_vascend_stub_depends}")



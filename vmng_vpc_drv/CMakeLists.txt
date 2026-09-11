set(drv_vpc_host_depends drv_seclib_host drv_vascend_stub ascend_kernel_open_adapt)
set(vpc_host_log_support_product_list "ascend910" "ascend310p" "ascend910B" "ascend310B")
if (${PRODUCT} IN_LIST vpc_host_log_support_product_list)
	set(vpc_host_log_support_depends ascend_logdrv)
endif ()

set(HOST_MAKE_ARGS
	${MAKE_ARGS}
	SERVER_TYPE=host
)

list(APPEND drv_vpc_host_depends ${drv_mem_dfx_depend})

add_host_ko(LOCAL_MODULE drv_vpc_host
			KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
			MAKE_ARGS ${HOST_MAKE_ARGS}
			TARGETE_DPENDS "${drv_vpc_host_depends}" "${vpc_host_log_support_depends}")


set(drv_vpc_agent_depends drv_seclib_host ascend_kernel_open_adapt)
set(AGENT_MAKE_ARGS
	${MAKE_ARGS}
	SERVER_TYPE=agent
)

list(APPEND drv_vpc_agent_depends ${drv_mem_dfx_depend})

add_host_ko(LOCAL_MODULE drv_vpc_agent
			KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
			MAKE_ARGS ${AGENT_MAKE_ARGS}
			TARGETE_DPENDS "${drv_vpc_agent_depends}" "${vpc_host_log_support_depends}")
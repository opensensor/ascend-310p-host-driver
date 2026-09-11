if (${PRODUCT_SIDE} STREQUAL device)
add_subdirectory(xsmem_device)
endif()

if (${PRODUCT_SIDE} STREQUAL host)
add_subdirectory(xsmem_host)
endif()

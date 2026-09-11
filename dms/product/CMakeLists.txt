if (${PRODUCT_SIDE} STREQUAL host)

    target_include_directories(drv_devmng_obj PRIVATE
        ${TOP_DIR}/inc/driver/
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/inc
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/sensor
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/smf/sensor
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/event
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/smf/event
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/config
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/include
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/core
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/product
    )

    target_compile_definitions(drv_devmng_obj PRIVATE
        CFG_FEATURE_DMS_PRODUCT_HOST
        $<$<STREQUAL:${PRODUCT},ascend910>:CFG_SOC_PLATFORM_CLOUD>
    )

    target_sources(drv_devmng_obj PRIVATE
        dms_product_host.c
        dms_product.c
    )

else()

    target_include_directories(drv_devmng_obj PRIVATE
        ${TOP_DIR}/inc/driver/
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/inc
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/sensor
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/smf/sensor
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/event
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/smf/event
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/config
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/include
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/core
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/product
        ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/src/dms/hccs
    )

    target_compile_definitions(drv_devmng_obj PRIVATE
        $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_PCIE_HCCS_BANDWIDTH>
        $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_HBM_MANUFACTURER_ID>
        $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_HCCS_BANDWIDTH>
        $<$<STREQUAL:${PRODUCT},ascend310p>:CFG_FEATURE_HCCS_LINK_ERROR_INFO>
        $<$<STREQUAL:${PRODUCT},ascend310p>:CFG_FEATURE_PCIE_LINK_ERROR_INFO>
    )

    target_sources(drv_devmng_obj PRIVATE
        dms_product.c
    )
endif ()
add_library(debug_drv_obj OBJECT)
set(DEVICE_LOCAL_MODULE ts_debug)
set(TARGET_KO_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)

target_sources(debug_drv_obj PRIVATE
    debug_drv_dev.c
    debug_dma.c
)

target_include_directories(debug_drv_obj PRIVATE
    ${TOP_DIR}/inc/driver/
    ${DRIVER_KERNEL_DIR}/src/common/
    ${DRIVER_KERNEL_DIR}/src/prof/
    ${DRIVER_KERNEL_DIR}/src/prof/prof_inc
    ${DRIVER_KERNEL_DIR}/inc/
    ${DRIVER_KERNEL_DIR}/src/drv_devmng/drv_devmng_inc/
    ${DRIVER_KERNEL_DIR}/src/tsdrv/ts_drv/ts_drv_common/tsdrv_dev
    ${DRIVER_KERNEL_DIR}/src/tsdrv/ts_drv/ts_drv_common
    ${DRIVER_KERNEL_DIR}/src/tsdrv/ts_drv/ts_drv_device
    ${DRIVER_KERNEL_DIR}/src/tsdrv/ts_platform/ts_platform_device/ascend610
    ${DRIVER_KERNEL_DIR}/src/tsdrv/ts_drv/ts_drv_device/ascend610
    ${DRIVER_KERNEL_DIR}/src/prof/ascd610
)
target_compile_definitions(debug_drv_obj PRIVATE CFG_SOC_PLATFORM_MDC_V51)
target_compile_definitions(debug_drv_obj PRIVATE AOS_LLVM_BUILD)
target_compile_options(debug_drv_obj PRIVATE
    -Wall
)

target_link_libraries(debug_drv_obj PRIVATE $<BUILD_INTERFACE:utils_intf_pub>)

add_custom_command(
    OUTPUT ${DEVICE_LOCAL_MODULE}.ko
    COMMAND ${CMAKE_LINKER} -r $<TARGET_OBJECTS:debug_drv_obj> -o ${DEVICE_LOCAL_MODULE}.ko
    DEPENDS debug_drv_obj
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND_EXPAND_LISTS
)
add_custom_target(ts_debug ALL DEPENDS debug_drv_obj ${DEVICE_LOCAL_MODULE}.ko)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DEVICE_LOCAL_MODULE}.ko
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)

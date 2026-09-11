add_library(ascend_event_sched_obj OBJECT)
set(DEVICE_LOCAL_MODULE ascend_event_sched)
set(TARGET_KO_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
target_sources(ascend_event_sched_obj PRIVATE
    esched.c
    esched_fops.c
    esched_sysfs.c
    ../bind_cpu/drv_cpu_type.c
)

target_compile_definitions(ascend_event_sched_obj PRIVATE AOS_LLVM_BUILD)
target_compile_definitions(ascend_event_sched_obj PRIVATE CFG_FEATURE_MORE_PID_PRIORITY)
target_compile_definitions(ascend_event_sched_obj PRIVATE CFG_FEATURE_SCHEDULE_REALTIME)
target_compile_definitions(ascend_event_sched_obj PRIVATE CFG_FEATURE_TIMEOUT_PROCESS)
target_compile_definitions(ascend_event_sched_obj PRIVATE CFG_FEATURE_TRACE_RECOED)
target_compile_definitions(ascend_event_sched_obj PRIVATE CFG_FEATURE_THREAD_SWAPOUT)
target_compile_definitions(ascend_event_sched_obj PRIVATE CFG_SOC_PLATFORM_MDC_V51)
target_compile_definitions(ascend_event_sched_obj PRIVATE CFG_SOC_PLATFORM_MDC_V11)

target_link_libraries(ascend_event_sched_obj PRIVATE $<BUILD_INTERFACE:utils_intf_pub>)

target_include_directories(ascend_event_sched_obj PRIVATE
    ${DRIVER_KERNEL_DIR}/src/event_sched/device/comm/
    ${DRIVER_KERNEL_DIR}/src/event_sched/device/bind_cpu/
    ${DRIVER_USER_DIR}/src/event_sched/
    ${DRIVER_KERNEL_DIR}/inc/
    ${TOP_DIR}/inc/driver/
    ${TOP_DIR}/inc/driver/errcode/
    ${TOP_DIR}/inc/common/
    ${TOP_DIR}/abl/libc_sec/include
)

add_custom_command(
    OUTPUT ${DEVICE_LOCAL_MODULE}.ko
    COMMAND ${CMAKE_LINKER} -r $<TARGET_OBJECTS:ascend_event_sched_obj> -o ${DEVICE_LOCAL_MODULE}.ko
    DEPENDS ascend_event_sched_obj
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND_EXPAND_LISTS
)
add_custom_target(ascend_event_sched ALL DEPENDS ascend_event_sched_obj ${DEVICE_LOCAL_MODULE}.ko)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DEVICE_LOCAL_MODULE}.ko
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)

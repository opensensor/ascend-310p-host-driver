
if (AOSCORE_LLVM_MUSL)
    add_subdirectory(device)
endif ()

target_sources(drv_devmng_obj PRIVATE
    dms_adapt.c
)

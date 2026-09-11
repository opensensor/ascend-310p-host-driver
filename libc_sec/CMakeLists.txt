cmake_minimum_required(VERSION 3.14)
project(Securec)

file(GLOB SRC_LIST RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    "src/vsprintf_s.c"
    "src/wmemmove_s.c"
    "src/strncat_s.c"
    "src/vsnprintf_s.c"
    "src/fwscanf_s.c"
    "src/scanf_s.c"
    "src/strcat_s.c"
    "src/sscanf_s.c"
    "src/secureprintoutput_w.c"
    "src/wmemcpy_s.c"
    "src/wcsncat_s.c"
    "src/secureprintoutput_a.c"
    "src/secureinput_w.c"
    "src/memcpy_s.c"
    "src/fscanf_s.c"
    "src/vswscanf_s.c"
    "src/secureinput_a.c"
    "src/memmove_s.c"
    "src/swscanf_s.c"
    "src/snprintf_s.c"
    "src/vscanf_s.c"
    "src/vswprintf_s.c"
    "src/wcscpy_s.c"
    "src/vfwscanf_s.c"
    "src/memset_s.c"
    "src/wscanf_s.c"
    "src/vwscanf_s.c"
    "src/strtok_s.c"
    "src/wcsncpy_s.c"
    "src/vfscanf_s.c"
    "src/vsscanf_s.c"
    "src/wcstok_s.c"
    "src/securecutil.c"
    "src/gets_s.c"
    "src/swprintf_s.c"
    "src/strcpy_s.c"
    "src/wcscat_s.c"
    "src/strncpy_s.c"
)

include_directories(./include)
include_directories(./src)

# ------ shared_c_sec ------
if((NOT "${EXTEND_TOOLCHAIN}" STREQUAL "ccec") AND (NOT "${ENABLE_SECUREC_SHARED}" STREQUAL "OFF"))
    add_library(shared_c_sec SHARED
        ${SRC_LIST}
        $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:src/sprintf_s.c>
    )

    target_compile_options(shared_c_sec PRIVATE
        $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:-Wall>
        $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:-Werror>
        $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:-O1>
    )

    target_compile_definitions(shared_c_sec PRIVATE
        NDEBUG
        SECUREC_SUPPORT_STRTOLD=1
        $<$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>:SECUREC_USING_STD_SECURE_LIB=0>
    )

    set_target_properties(shared_c_sec
        PROPERTIES
        OUTPUT_NAME $<IF:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>,libc_sec,c_sec>
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    )

    install(TARGETS shared_c_sec OPTIONAL
        DESTINATION lib
    )
endif()

add_library(static_c_sec STATIC
    ${SRC_LIST}
    $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:src/sprintf_s.c>
)

target_compile_options(static_c_sec PRIVATE
    $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:-Wall>
    $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:-Werror>
    $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:-O1>
    $<$<NOT:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>>:-fPIC>
)

target_compile_definitions(static_c_sec PRIVATE
    NDEBUG
    SECUREC_SUPPORT_STRTOLD=1
    $<$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>:SECUREC_USING_STD_SECURE_LIB=0>
)

set_target_properties(static_c_sec
    PROPERTIES
    OUTPUT_NAME $<IF:$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>,libc_sec_static,c_sec>
)

install(TARGETS static_c_sec OPTIONAL
    DESTINATION lib
)

install(FILES
    "./include/securec.h"
    "./include/securectype.h"
    DESTINATION include
)

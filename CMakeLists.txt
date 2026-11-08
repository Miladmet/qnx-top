cmake_minimum_required(VERSION 3.20)
project(qnx-top VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to a build type that keeps debug symbols. Tests do not depend on
# assert(), so they also work under Release (NDEBUG) builds.
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(QNX_TOP_WERROR "Treat compiler warnings as errors" OFF)

# Compiler Warnings
if(MSVC)
    add_compile_options(/W4)
    if(QNX_TOP_WERROR)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(QNX_TOP_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

include_directories(include)

# Detect QNX target builds. The QNX toolchain files set CMAKE_SYSTEM_NAME to QNX
# and qcc predefines __QNX__, so no manual define is needed. Merely having
# QNX_TARGET in the environment must not switch a host build into QNX mode.
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    message(STATUS "[qnx-top] Configuring build for BlackBerry QNX Neutrino 8.0 Target")
else()
    message(STATUS "[qnx-top] Configuring build in Host Compatibility Mode (${CMAKE_SYSTEM_NAME})")
endif()

# Core library
add_library(qnx_top_core STATIC
    src/qnx_collector.c
    src/qnx_resmgr.c
)
target_include_directories(qnx_top_core PUBLIC include)

# pthreads: part of libc on QNX; a separate library on other UNIX hosts.
if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(qnx_top_core PUBLIC Threads::Threads)
endif()

# Main executable
add_executable(qnx-top src/main.c)
target_link_libraries(qnx-top PRIVATE qnx_top_core)

# Standard installation paths
install(TARGETS qnx-top
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Unit Tests (BUILD_TESTING is provided by include(CTest), default ON)
if(BUILD_TESTING)
    add_executable(test_qnx_top tests/test_state_mapping.c)
    target_link_libraries(test_qnx_top PRIVATE qnx_top_core)
    add_test(NAME StateMappingAndSnapshotTest COMMAND test_qnx_top)
endif()

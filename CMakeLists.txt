cmake_minimum_required(VERSION 3.20)
project(qnx-top VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler Warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(include)

# Detect QNX Target Environment
if(CMAKE_SYSTEM_NAME MATCHES "QNX" OR DEFINED ENV{QNX_TARGET})
    message(STATUS "[qnx-top] Configuring build for BlackBerry QNX Neutrino 8.0 Target")
    set(QNX_LIBS socket)
else()
    message(STATUS "[qnx-top] Configuring build in Host Compatibility Mode (${CMAKE_SYSTEM_NAME})")
    set(QNX_LIBS "")
endif()

# Core library
set(LIB_SOURCES
    src/qnx_collector.c
    src/qnx_resmgr.c
)

add_library(qnx_top_core STATIC ${LIB_SOURCES})
target_include_directories(qnx_top_core PUBLIC include)

if(QNX_LIBS)
    target_link_libraries(qnx_top_core PUBLIC ${QNX_LIBS})
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(qnx_top_core PUBLIC pthread)
endif()

# Main executable
add_executable(qnx-top src/main.c)
target_link_libraries(qnx-top PRIVATE qnx_top_core)

# Standard installation paths
install(TARGETS qnx-top
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Unit Tests
if(BUILD_TESTING)
    enable_testing()
    add_executable(test_qnx_top tests/test_state_mapping.c)
    target_link_libraries(test_qnx_top PRIVATE qnx_top_core)
    add_test(NAME StateMappingAndSnapshotTest COMMAND test_qnx_top)
endif()

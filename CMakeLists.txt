cmake_minimum_required(VERSION 3.20)
project(qnx-top VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

# Check if compiling under QNX SDP
if(CMAKE_SYSTEM_NAME STREQUAL "QNX" OR DEFINED ENV{QNX_TARGET})
    message(STATUS "[qnx-top] Configuring build for BlackBerry QNX Neutrino 8.0 Target")
    add_compile_definitions(__QNX__)
    # Link against QNX microkernel libraries
    set(QNX_LIBS socket)
else()
    message(STATUS "[qnx-top] Configuring build in Host Compatibility Mode")
    set(QNX_LIBS "")
endif()

# Source files
set(SOURCES
    src/main.c
    src/qnx_collector.c
    src/qnx_resmgr.c
)

add_executable(qnx-top ${SOURCES})
target_include_directories(qnx-top PRIVATE include)

if(QNX_LIBS)
    target_link_libraries(qnx-top PRIVATE ${QNX_LIBS})
endif()

# Installation target
install(TARGETS qnx-top DESTINATION /usr/bin)

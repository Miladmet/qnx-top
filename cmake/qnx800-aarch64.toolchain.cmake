# CMake Toolchain File for BlackBerry QNX SDP 8.0 (aarch64 target)
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0.0)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Set QNX Cross Compilers
if(NOT DEFINED ENV{QNX_HOST})
    message(FATAL_ERROR "QNX_HOST environment variable not set. Please source qnxsdp-env.sh first.")
endif()

set(CMAKE_C_COMPILER "$ENV{QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "$ENV{QNX_HOST}/usr/bin/q++")

set(CMAKE_C_FLAGS "-Vgcc_ntoaarch64le -O2 -Wall -Wextra" CACHE STRING "QNX C Flags")
set(CMAKE_CXX_FLAGS "-Vgcc_ntoaarch64le -O2 -Wall -Wextra" CACHE STRING "QNX CXX Flags")

set(CMAKE_FIND_ROOT_PATH "$ENV{QNX_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

cmake_minimum_required(VERSION 3.24)
project(rlcd_host_test C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# --------------------------------------------------------------------------
# Catch2 v3.x via FetchContent
# --------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

# --------------------------------------------------------------------------
# Paths used by subdirectory CMakeLists
# --------------------------------------------------------------------------
set(RLCD_ROOT       "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(RLCD_COMPONENTS "${RLCD_ROOT}/esp32_rendering/components")
set(RLCD_SHIMS      "${CMAKE_CURRENT_SOURCE_DIR}/shims")

# 1bit-display library (for tests that link pattern.cpp / pattern_tiles.cpp etc).
set(ONEBIT_LIB_DIR "$ENV{ONEBIT_LIB_DIR}" CACHE PATH "Path to 1bit-display library")
if(NOT ONEBIT_LIB_DIR OR NOT EXISTS "${ONEBIT_LIB_DIR}")
    set(ONEBIT_LIB_DIR "${RLCD_ROOT}/../1bit-display" CACHE PATH "Path to 1bit-display library" FORCE)
endif()
set(ONEBIT_INCLUDE_DIR "${ONEBIT_LIB_DIR}/include" CACHE PATH "1bit-display headers")

include(CTest)
include(Catch)
include(cmake/AddHostTest.cmake)

# --------------------------------------------------------------------------
# Subdirectories — one per component. Explicit, no glob.
# --------------------------------------------------------------------------
add_subdirectory(smoke)
add_subdirectory(ble_hid)
add_subdirectory(app)
add_subdirectory(buttons)
add_subdirectory(ssh_client)

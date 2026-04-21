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

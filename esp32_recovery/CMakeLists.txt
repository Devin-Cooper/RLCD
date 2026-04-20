cmake_minimum_required(VERSION 3.16)

# Mirror the ONEBIT_LIB_DIR pattern from esp32_rendering/CMakeLists.txt
# so recovery can share the onebit, rendering, and st7305 components
# with the main app without duplication.
set(ONEBIT_LIB_DIR "$ENV{ONEBIT_LIB_DIR}" CACHE PATH "Path to 1bit-display library")
if(NOT ONEBIT_LIB_DIR)
    set(ONEBIT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../1bit-display")
endif()

# Point at INDIVIDUAL component directories (not the whole components/
# folder) so we pull in only rendering + st7305 and avoid auto-registering
# the big components — ssh_client, wifi_manager, ble_hid, libssh2_esp —
# that would otherwise get compiled into the recovery binary.
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../esp32_rendering/components/rendering"
    "${CMAKE_CURRENT_SOURCE_DIR}/../esp32_rendering/components/st7305"
    "${ONEBIT_LIB_DIR}/platform/onebit"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_recovery)

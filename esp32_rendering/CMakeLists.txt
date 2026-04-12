# ESP32-S3 RLCD Terminal
# For Waveshare ESP32-S3-RLCD-4.2 board

cmake_minimum_required(VERSION 3.16)

set(ONEBIT_LIB_DIR "$ENV{ONEBIT_LIB_DIR}" CACHE PATH "Path to 1bit-display library")
if(NOT ONEBIT_LIB_DIR)
    set(ONEBIT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../1bit-display")
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${ONEBIT_LIB_DIR}/platform/onebit"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_terminal)

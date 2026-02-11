# ESP32-S3 Rendering Toolkit
# For Waveshare ESP32-S3-RLCD-4.2 board

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_rendering)

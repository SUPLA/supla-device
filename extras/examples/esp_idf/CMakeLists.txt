# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED ENV{SUPLA_DEVICE_PATH})
  message(FATAL_ERROR "SUPLA_DEVICE_PATH env variable is not set")
endif()

set(EXTRA_COMPONENT_DIRS
  "$ENV{SUPLA_DEVICE_PATH}/extras/esp-idf/supla-device"
  # add here paths to other components you want to use
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(supla_esp32)

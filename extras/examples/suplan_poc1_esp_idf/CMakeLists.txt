cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{SUPLA_DEVICE_PATH})
  message(FATAL_ERROR "SUPLA_DEVICE_PATH must point at supla-device")
endif()

set(SUPLA_DEVICE_ROOT "$ENV{SUPLA_DEVICE_PATH}")
set(EXTRA_COMPONENT_DIRS
  "${SUPLA_DEVICE_ROOT}/extras/esp-idf/supla-device"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(suplan_poc1_esp_idf)

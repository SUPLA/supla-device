cmake_minimum_required(VERSION 3.15)

project(supla-device-linux LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set(CMAKE_C_COMPILER_LAUNCHER ccache)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

set(SUPLA_DEVICE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../")
get_filename_component(SUPLA_DEVICE_PATH "${SUPLA_DEVICE_PATH}" ABSOLUTE)

include(FetchContent)

include(${SUPLA_DEVICE_PATH}/cmake/SuplaDeviceSources.cmake)
include(${SUPLA_DEVICE_PATH}/cmake/SuplaCommonSources.cmake)
include(${SUPLA_DEVICE_PATH}/cmake/SuplaTools.cmake)
include(${SUPLA_DEVICE_PATH}/extras/porting/linux/SuplaLinux.cmake)

add_library(supladevicelib STATIC)
add_library(suplalinuxlib STATIC)
add_executable(supla-device-linux main.cpp)

supla_device(supladevicelib)
supla_common(supladevicelib)
supla_linux(suplalinuxlib)

target_compile_definitions(supladevicelib PRIVATE SUPLA_LINUX)
target_compile_definitions(supla-device-linux PRIVATE MQTT_USE_BIO)

target_include_directories(supladevicelib PUBLIC
  "${SUPLA_DEVICE_PATH}/src"
  "${SUPLA_DEVICE_PATH}/extras/porting/linux"  # used for linux_timers.h
)

target_include_directories(suplalinuxlib PUBLIC
  "${SUPLA_DEVICE_PATH}/src"
  "${SUPLA_DEVICE_PATH}/extras/porting/linux"
)

target_link_libraries(suplalinuxlib PUBLIC supladevicelib)
target_link_libraries(supla-device-linux PRIVATE supladevicelib suplalinuxlib)

# stdc++fs only for old GCC
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(suplalinuxlib PRIVATE stdc++fs)
endif()

#supla_apply_warnings(supladevicelib)
supla_apply_warnings(suplalinuxlib)
supla_apply_warnings(supla-device-linux)

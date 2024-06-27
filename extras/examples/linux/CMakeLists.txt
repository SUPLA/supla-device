cmake_minimum_required(VERSION 3.13)

project(supla-device-linux)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_BUILD_TYPE Debug)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif(CCACHE_FOUND)

SET( CMAKE_EXPORT_COMPILE_COMMANDS ON )
# Current board sources
set(BOARD_SRC
  main.cpp
  )

link_libraries( "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:-lstdc++fs>" )
# supladevice library target is used in porting/linux cmake

if(NOT DEFINED ENV{SUPLA_DEVICE_PATH})
  set(ENV{SUPLA_DEVICE_PATH} "../../../")
  #  message(FATAL_ERROR "SUPLA_DEVICE_PATH env variable is not set")
endif()

add_definitions(-DMQTT_USE_BIO -DSUPLA_DEVICE)
set(MQTT_C_OpenSSL_SUPPORT ON CACHE BOOL "Build MQTT-C with OpenSSL support?")
set(MQTT_C_EXAMPLES OFF CACHE BOOL "Build MQTT-C examples?")

include(FetchContent)

FetchContent_Declare(
        mqttc
        GIT_REPOSITORY https://github.com/LiamBindle/MQTT-C.git
        GIT_TAG        v1.1.6
)

FetchContent_GetProperties(mqttc)
if(NOT mqttc_POPULATED)  # Have we downloaded mqttc yet?
  set(MQTT_C_OpenSSL_SUPPORT ON CACHE BOOL "Build MQTT-C with OpenSSL support?")
  set(MQTT_C_EXAMPLES OFF CACHE BOOL "Build MQTT-C examples?")
  FetchContent_Populate(mqttc)
  add_subdirectory(${mqttc_SOURCE_DIR} ${mqttc_BINARY_DIR})
endif()

include_directories(
        .
        ${mqttc_SOURCE_DIR}/include
)

add_library(supladevice "")
add_subdirectory($ENV{SUPLA_DEVICE_PATH} supla-device)
add_subdirectory($ENV{SUPLA_DEVICE_PATH}/extras/porting/linux supla-porting-linux)


add_executable(supla-device-linux
  ${BOARD_SRC}
)

set_target_properties(supla-device-linux PROPERTIES LINK_LIBRARIES -pthread)
#target_include_directories(supla-device-linux PRIVATE ${mqttc_SOURCE_DIR}/src/include)

find_package(OpenSSL REQUIRED)

target_link_libraries(supla-device-linux LINK_PUBLIC supladevice mqttc OpenSSL::SSL OpenSSL::Crypto -ldl)

#target_compile_options(supla-device-linux PRIVATE -Wall -Wextra -Wpedantic -Werror)

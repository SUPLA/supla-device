cmake_minimum_required(VERSION 3.15)

project(supla-device-linux)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif(CCACHE_FOUND)

SET( CMAKE_EXPORT_COMPILE_COMMANDS ON )

link_libraries( "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:-lstdc++fs>" )
# supladevice library target is used in porting/linux cmake

set(SUPLA_DEVICE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../")
get_filename_component(SUPLA_DEVICE_PATH "${SUPLA_DEVICE_PATH}" ABSOLUTE)

#if(NOT DEFINED ENV{SUPLA_DEVICE_PATH})
#  set(ENV{SUPLA_DEVICE_PATH} "../../../")
#  #  message(FATAL_ERROR "SUPLA_DEVICE_PATH env variable is not set")
#endif()

include("${SUPLA_DEVICE_PATH}/cmake/SuplaDeviceSources.cmake")
include("${SUPLA_DEVICE_PATH}/cmake/SuplaCommonSources.cmake")
include("${SUPLA_DEVICE_PATH}/extras/porting/linux/SuplaLinux.cmake")

#include("${SUPLA_DEVICE_PATH}/cmake/NettleSources.cmake")

FetchContent_Declare(
        mqttc
        GIT_REPOSITORY https://github.com/LiamBindle/MQTT-C.git
        GIT_TAG        v1.1.6
)

set(MQTT_C_OpenSSL_SUPPORT ON CACHE BOOL "Build MQTT-C with OpenSSL support?")
set(MQTT_C_EXAMPLES OFF CACHE BOOL "Build MQTT-C examples?")
FetchContent_MakeAvailable(mqttc)

add_library(supladevicelib)

#add_subdirectory($ENV{SUPLA_DEVICE_PATH} supladevice)
#add_subdirectory($ENV{SUPLA_DEVICE_PATH}/extras/porting/linux supla-porting-linux)

add_executable(supla-device-linux main.cpp )

supla_device(supladevicelib)
supla_common(supladevicelib)

supla_linux(supla-device-linux)

target_compile_definitions(supladevicelib PRIVATE SUPLA_LINUX)
target_compile_definitions(supla-device-linux PRIVATE MQTT_USE_BIO SUPLA_LINUX)

target_include_directories(supladevicelib PRIVATE
  ${SUPLA_DEVICE_PATH}/extras/porting/linux
)
target_include_directories(supla-device-linux PRIVATE
  ${SUPLA_DEVICE_PATH}/extras/porting/linux
)

target_include_directories(supla-device-linux PRIVATE
  .
  ${mqttc_SOURCE_DIR}/include
  "$ENV{SUPLA_DEVICE_PATH}/src"
)

set_target_properties(supla-device-linux PROPERTIES LINK_LIBRARIES -pthread)
#target_include_directories(supla-device-linux PRIVATE ${mqttc_SOURCE_DIR}/src/include)

find_package(OpenSSL REQUIRED)

target_link_libraries(supla-device-linux LINK_PUBLIC
  supladevicelib
  mqttc
  OpenSSL::SSL
  OpenSSL::Crypto
  -ldl
  cxxopts::cxxopts
  nlohmann_json::nlohmann_json
  yaml-cpp
)

#target_compile_options(supla-device-linux PRIVATE -Wall -Wextra -Wpedantic -Werror)

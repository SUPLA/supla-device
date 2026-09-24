cmake_minimum_required(VERSION 3.15)
project(suplan_poc1_linux LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(SUPLA_DEVICE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.."
                       ABSOLUTE)
include("${SUPLA_DEVICE_ROOT}/cmake/SupLanSources.cmake")
include("${SUPLA_DEVICE_ROOT}/cmake/SuplaDeviceSources.cmake")
include("${SUPLA_DEVICE_ROOT}/cmake/SuplaCommonSources.cmake")
include("${SUPLA_DEVICE_ROOT}/cmake/SuplaTools.cmake")
find_package(OpenSSL REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(supla-device-poc STATIC)
supla_device(supla-device-poc)
supla_common(supla-device-poc)
target_sources(supla-device-poc PRIVATE
  "${SUPLA_DEVICE_ROOT}/extras/porting/linux/linux_client.cpp"
  "${SUPLA_DEVICE_ROOT}/extras/porting/linux/linux_log.c"
  "${SUPLA_DEVICE_ROOT}/extras/porting/linux/linux_platform.cpp"
  "${SUPLA_DEVICE_ROOT}/extras/porting/linux/linux_timers.cpp"
)
target_include_directories(supla-device-poc PUBLIC
  "${SUPLA_DEVICE_ROOT}/src"
  "${SUPLA_DEVICE_ROOT}/extras/porting/linux"
)
target_compile_definitions(supla-device-poc PUBLIC SUPLA_LINUX SUPLA_DEVICE)

add_executable(suplan-poc1-linux
  main.cpp
  ${SUPLA_DEVICE_ROOT}/extras/porting/linux/suplan_crypto_openssl.cpp
  ${SUPLA_DEVICE_ROOT}/extras/porting/linux/suplan_udp_linux.cpp
)
target_include_directories(suplan-poc1-linux PRIVATE
  "${SUPLA_DEVICE_ROOT}/src"
  "${SUPLA_DEVICE_ROOT}/extras/examples/suplan_poc1_common"
  "${SUPLA_DEVICE_ROOT}/extras/porting/linux"
)
target_link_libraries(suplan-poc1-linux PRIVATE
  supla-device-poc
  yaml-cpp
  OpenSSL::SSL
  OpenSSL::Crypto
)

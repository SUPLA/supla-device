cmake_minimum_required(VERSION 3.20)

project(supladevice LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_FOUND ccache)
# set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE include-what-you-use)
if(CCACHE_FOUND)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
  set(CMAKE_C_COMPILER_LAUNCHER ccache)
endif(CCACHE_FOUND)

enable_testing()

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/SuplaDeviceSources.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/SuplaTools.cmake)

mark_as_advanced(
  BUILD_GMOCK
  BUILD_GTEST
)

include(FetchContent)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.12.0
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW TRUE
  )

FetchContent_MakeAvailable(json googletest)

option(SUPLA_TEST_ENABLE_CURL_HTTP
  "Enable sd4linux HTTP source tests when CURL is available" ON)
option(SUPLA_TEST_REQUIRE_CURL
  "Fail test configuration when CURL is missing" OFF)

find_package(CURL QUIET)
set(SUPLA_TEST_CURL_HTTP_ENABLED OFF)
if(CURL_FOUND AND SUPLA_TEST_ENABLE_CURL_HTTP)
  set(SUPLA_TEST_CURL_HTTP_ENABLED ON)
elseif(SUPLA_TEST_REQUIRE_CURL)
  message(FATAL_ERROR
    "CURL is required for sd4linux HTTP source tests. "
    "Install libcurl development package or disable SUPLA_TEST_REQUIRE_CURL.")
endif()

file(GLOB TEST_SRC CONFIGURE_DEPENDS
  UptimeTests/*.cpp
  ChannelTests/*.cpp
  IoTests/*.cpp
  ElementTests/*.cpp
  LocalActionTests/*.cpp
  SensorTests/*.cpp
  ChannelElementTests/*.cpp
  InternalPinOutputTests/*.cpp
  BlinkingLedTests/*.cpp
  PinStatusLedTests/*.cpp
  ConditionTests/*.cpp
  RgbwDimmerTests/*.cpp
  ButtonTests/*.cpp
  StorageTests/*.cpp
  SuplaDeviceTests/*.cpp
  CorrectionTests/*.cpp
  ActionTriggerTests/*.cpp
  ElectricityMeterTests/*.cpp
  ToolsTests/*.cpp
  Mqtt/*.cpp
  Html/*.cpp
  HvacTests/*.cpp
  RollerShutterTests/*.cpp
  NotifTests/*.cpp
  RelayTests/*.cpp
  GpmTests/*.cpp
  SwUpdateTests/*.cpp
  ModbusTests/*.cpp
  SupletTests/*.cpp
  DebugTests/*.cpp
  )

file(GLOB SD4LINUX_TEST_SRC CONFIGURE_DEPENDS
  LinuxPortTests/*.cpp
  )

if(NOT SUPLA_TEST_CURL_HTTP_ENABLED)
  list(REMOVE_ITEM SD4LINUX_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/LinuxPortTests/sd4linux_http_source_tests.cpp)
endif()

list(APPEND TEST_SRC ../../src/supla-common/proto_check.cpp)

file(GLOB DOUBLE_SRC CONFIGURE_DEPENDS doubles/*.cpp)

set(SD4LINUX_PORT_SRC
  ../porting/linux/linux_channel_factory.cpp
  ../porting/linux/supla/control/cmd_relay.cpp
  ../porting/linux/supla/control/control_payload.cpp
  ../porting/linux/supla/control/custom_relay.cpp
  ../porting/linux/supla/control/cmd_roller_shutter.cpp
  ../porting/linux/supla/control/rgbcct_parsed.cpp
  ../porting/linux/supla/control/action_trigger_parsed.cpp
  ../porting/linux/supla/parser/json.cpp
  ../porting/linux/supla/parser/parser.cpp
  ../porting/linux/supla/sensor/sensor_parsed.cpp
  ../porting/linux/supla/sensor/binary_parsed.cpp
  ../porting/linux/supla/sensor/general_purpose_measurement_parsed.cpp
  ../porting/linux/supla/sensor/impulse_counter_parsed.cpp
  ../porting/linux/supla/sensor/thermometer_parsed.cpp
  ../porting/linux/supla/payload/payload.cpp
  )

if(SUPLA_TEST_CURL_HTTP_ENABLED)
  list(APPEND SD4LINUX_PORT_SRC ../porting/linux/supla/source/http.cpp)
endif()

add_library(supladevicelib SHARED)
supla_device(supladevicelib)
target_sources(supladevicelib PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/doubles/fill_random_stub.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/pv/solaredge.cpp
)

target_include_directories(supladevicelib PUBLIC
  ${SUPLA_DEVICE_SRC_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/doubles
)

add_executable(supladevicetests ${TEST_SRC} ${DOUBLE_SRC})

add_executable(sd4linuxtests
  ${SD4LINUX_TEST_SRC}
  ${DOUBLE_SRC}
  ${SD4LINUX_PORT_SRC}
  )

target_include_directories(supladevicetests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/doubles
)

target_include_directories(sd4linuxtests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/doubles
  ${CMAKE_CURRENT_SOURCE_DIR}/../porting/linux
)

target_link_libraries(supladevicetests
  PRIVATE
    supladevicelib
    nlohmann_json::nlohmann_json
    gtest
    gmock
    gtest_main
  )

target_link_libraries(sd4linuxtests
  PRIVATE
    supladevicelib
    nlohmann_json::nlohmann_json
    gtest
    gmock
    gtest_main
  )

if(SUPLA_TEST_CURL_HTTP_ENABLED)
  target_link_libraries(sd4linuxtests PRIVATE CURL::libcurl)
  target_compile_definitions(sd4linuxtests
    PRIVATE SUPLA_LINUX_HTTP_SOURCE_ENABLED)
endif()

add_test(NAME supladevicetests COMMAND supladevicetests)
add_test(NAME sd4linuxtests COMMAND sd4linuxtests)


supla_apply_warnings(supladevicetests)
supla_apply_warnings(sd4linuxtests)
supla_apply_warnings(supladevicelib)

target_compile_definitions(supladevicetests PRIVATE
  SUPLA_TEST
  SUPLA_INSECURE_DEBUG_INTERFACE=1
)
target_compile_definitions(sd4linuxtests PRIVATE
  SUPLA_TEST
  SUPLA_INSECURE_DEBUG_INTERFACE=1
)
target_compile_definitions(supladevicelib PRIVATE
  SUPLA_TEST
  SUPLA_INSECURE_DEBUG_INTERFACE=1
)

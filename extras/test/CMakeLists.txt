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
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW TRUE
  )

FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SRC CONFIGURE_DEPENDS
  UptimeTests/*.cpp
  ChannelTests/*.cpp
  IoTests/*.cpp
  ElementTests/*.cpp
  LocalActionTests/*.cpp
  SensorTests/*.cpp
  ChannelElementTests/*.cpp
  InternalPinOutputTests/*.cpp
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
  )

list(APPEND TEST_SRC ../../src/supla-common/proto_check.cpp)

file(GLOB DOUBLE_SRC CONFIGURE_DEPENDS doubles/*.cpp)

add_library(supladevicelib SHARED)
supla_device(supladevicelib)

target_include_directories(supladevicelib PUBLIC
  ${SUPLA_DEVICE_SRC_DIR}
)

add_executable(supladevicetests ${TEST_SRC} ${DOUBLE_SRC})

target_include_directories(supladevicetests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/doubles
)

target_link_libraries(supladevicetests
  PRIVATE
    supladevicelib
    gtest
    gmock
    gtest_main
  )

add_test(NAME supladevicetests COMMAND supladevicetests)


supla_apply_warnings(supladevicetests)
supla_apply_warnings(supladevicelib)

target_compile_definitions(supladevicetests PRIVATE SUPLA_TEST)
target_compile_definitions(supladevicelib PRIVATE SUPLA_TEST)


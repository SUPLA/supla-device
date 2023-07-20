cmake_minimum_required(VERSION 3.11)

project(supladevice)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_BUILD_TYPE Debug)
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif(CCACHE_FOUND)

include_directories(../../src)
include_directories(doubles)

add_subdirectory(../../src/ build)

mark_as_advanced(
BUILD_GMOCK
BUILD_GTEST
)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        origin/main
  )

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

file(GLOB TEST_SRC
  UptimeTests/*.cpp
  ChannelTests/*cpp
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
  )

file(GLOB DOUBLE_SRC doubles/*.cpp)

add_executable(supladevicetests ${TEST_SRC} ${DOUBLE_SRC})

target_link_libraries(supladevicetests
  gmock
  gtest
  gtest_main
  supladevicelib
  )

add_test(NAME supladevicetests
  COMMAND supladevicetests)

target_compile_options(supladevicelib PRIVATE -Werror -Wall -Wextra -DSUPLA_TEST)

cmake_minimum_required(VERSION 3.20)

project(supladevice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_FOUND ccache)
# set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE include-what-you-use)
if(CCACHE_FOUND)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif(CCACHE_FOUND)

enable_testing()

add_subdirectory(../../src/ build)

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
  GpmTests/*.cpp
  SwUpdateTests/*.cpp
  ../../src/supla-common/proto_check.cpp
  )

file(GLOB DOUBLE_SRC doubles/*.cpp)

add_executable(supladevicetests ${TEST_SRC} ${DOUBLE_SRC})

target_include_directories(supladevicetests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src
  ${CMAKE_CURRENT_SOURCE_DIR}/doubles
)

target_include_directories(supladevicelib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

target_link_libraries(supladevicetests
  PRIVATE
    supladevicelib
    gtest
    gmock
    gtest_main
  )

add_test(NAME supladevicetests COMMAND supladevicetests)

set(WARNINGS
  -Wall
  -Wextra
  -Werror
  -Wformat=2
  -Wcast-align
  -Wcast-qual
  -Wmissing-field-initializers
  -Wpointer-arith
  -Wredundant-decls
  -Wstrict-aliasing=2
  -Wunreachable-code
  -Wunused
  -Wunused-parameter
  -Wvariadic-macros
  -Wwrite-strings
)

target_compile_options(supladevicelib PRIVATE ${WARNINGS})
target_compile_options(supladevicetests PRIVATE
  ${WARNINGS}
  -Wstringop-truncation
  -Werror=stringop-truncation
)

target_compile_definitions(supladevicetests PRIVATE SUPLA_TEST SUPLA_DEVICE)
target_compile_definitions(supladevicelib PRIVATE SUPLA_TEST SUPLA_DEVICE)


include(FetchContent)

find_package(yaml-cpp REQUIRED)
find_package(OpenSSL REQUIRED)

if(${CMAKE_VERSION} VERSION_LESS "3.24.0") 
  FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz)
  FetchContent_Declare(cxxopts URL https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.0.0.tar.gz)
else()
  FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  FetchContent_Declare(cxxopts
    URL https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.0.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
endif()
#FetchContent_MakeAvailable(json cxxopts)

# NOTE: Where possible, prefer to use FetchContent_MakeAvailable()
#       instead of custom logic like this

# Check if population has already been performed
FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR})
endif()
FetchContent_GetProperties(cxxopts)
if(NOT cxxopts_POPULATED)
  FetchContent_Populate(cxxopts)
  add_subdirectory(${cxxopts_SOURCE_DIR} ${cxxopts_BINARY_DIR})
endif()

set(SUPLA_DEVICE_LINUX_SRCS
  linux_network.cpp
  linux_yaml_config.cpp
  linux_platform.cpp
  linux_file_state_logger.cpp
  linux_client.cpp
  linux_file_storage.cpp

  linux_timers.cpp
  linux_clock.cpp

  supla/source/cmd.cpp
  supla/source/file.cpp

  supla/parser/parser.cpp
  supla/parser/simple.cpp
  supla/parser/json.cpp

  supla/sensor/sensor_parsed.cpp
  supla/sensor/thermometer_parsed.cpp
  supla/sensor/impulse_counter_parsed.cpp
  supla/sensor/electricity_meter_parsed.cpp
  supla/sensor/binary_parsed.cpp
  supla/sensor/humidity_parsed.cpp
  supla/sensor/pressure_parsed.cpp
  supla/sensor/rain_parsed.cpp
  supla/sensor/wind_parsed.cpp
  supla/sensor/therm_hygro_meter_parsed.cpp
  supla/sensor/general_purpose_measurement_parsed.cpp
  supla/sensor/general_purpose_meter_parsed.cpp

  supla/control/cmd_relay.cpp
  supla/control/action_trigger_parsed.cpp
  supla/control/hvac_parsed.cpp

  ../../../src/supla/pv/fronius.cpp
  ../../../src/supla/pv/afore.cpp

  ../../../src/supla-common/tools.c
  ../../../src/supla-common/eh.c

  linux_log.c
)

target_sources(supladevice
  PRIVATE
  ${SUPLA_DEVICE_LINUX_SRCS}
  )

target_include_directories (supladevice
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${YAML_INCLUDE_DIRS}
  )

target_compile_definitions(supladevice PUBLIC SUPLA_LINUX)

#target_compile_options(supladevice PRIVATE -Wall -Wextra -Wpedantic -Werror)

target_link_libraries (supladevice PUBLIC
  nlohmann_json::nlohmann_json
  cxxopts::cxxopts
  OpenSSL::SSL
  yaml-cpp
  )

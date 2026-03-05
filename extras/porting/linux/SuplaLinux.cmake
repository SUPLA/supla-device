include_guard()

include(FetchContent)

find_package(OpenSSL REQUIRED)
find_package(yaml-cpp REQUIRED)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG v3.2.0
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  mqttc
  GIT_REPOSITORY https://github.com/LiamBindle/MQTT-C.git
  GIT_TAG v1.1.6
  GIT_SHALLOW TRUE
)
set(MQTT_C_OpenSSL_SUPPORT ON CACHE BOOL "Build MQTT-C with OpenSSL support?" FORCE)
set(MQTT_C_EXAMPLES OFF CACHE BOOL "Build MQTT-C examples?" FORCE)

FetchContent_MakeAvailable(json cxxopts mqttc)

# Bazowy katalog tego portingu
get_filename_component(SUPLA_LINUX_PORT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

get_filename_component(_SUPLA_ROOT_FROM_LINUX "${SUPLA_LINUX_PORT_DIR}/../../.." ABSOLUTE)

set(SUPLA_DEVICE_LINUX_SRCS
  ${SUPLA_LINUX_PORT_DIR}/linux_network.cpp
  ${SUPLA_LINUX_PORT_DIR}/linux_yaml_config.cpp
  ${SUPLA_LINUX_PORT_DIR}/linux_platform.cpp
  ${SUPLA_LINUX_PORT_DIR}/linux_file_state_logger.cpp
  ${SUPLA_LINUX_PORT_DIR}/linux_client.cpp
  ${SUPLA_LINUX_PORT_DIR}/linux_file_storage.cpp
  ${SUPLA_LINUX_PORT_DIR}/linux_mqtt_client.cpp
  ${SUPLA_LINUX_PORT_DIR}/mqtt_client.cpp

  ${SUPLA_LINUX_PORT_DIR}/linux_timers.cpp
  ${SUPLA_LINUX_PORT_DIR}/linux_clock.cpp

  ${SUPLA_LINUX_PORT_DIR}/supla/custom_channel.cpp

  ${SUPLA_LINUX_PORT_DIR}/supla/source/cmd.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/source/file.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/source/mqtt_src.cpp

  ${SUPLA_LINUX_PORT_DIR}/supla/parser/parser.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/parser/simple.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/parser/json.cpp

  ${SUPLA_LINUX_PORT_DIR}/supla/output/cmd.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/output/file.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/output/mqtt.cpp

  ${SUPLA_LINUX_PORT_DIR}/supla/payload/payload.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/payload/simple.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/payload/json.cpp

  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/sensor_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/thermometer_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/impulse_counter_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/electricity_meter_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/binary_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/humidity_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/pressure_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/rain_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/wind_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/weight_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/distance_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/therm_hygro_meter_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/general_purpose_measurement_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/general_purpose_meter_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/sensor/container_parsed.cpp

  ${SUPLA_LINUX_PORT_DIR}/supla/control/cmd_relay.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/cmd_roller_shutter.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/rgbcct_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/cmd_valve.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/action_trigger_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/hvac_parsed.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/custom_hvac.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/custom_relay.cpp
  ${SUPLA_LINUX_PORT_DIR}/supla/control/control_payload.cpp

  ${_SUPLA_ROOT_FROM_LINUX}/src/supla/pv/fronius.cpp
  ${_SUPLA_ROOT_FROM_LINUX}/src/supla/pv/afore.cpp

  ${_SUPLA_ROOT_FROM_LINUX}/src/supla-common/tools.c
  ${_SUPLA_ROOT_FROM_LINUX}/src/supla-common/eh.c
  ${_SUPLA_ROOT_FROM_LINUX}/src/supla-common/proto_check.cpp

  ${SUPLA_LINUX_PORT_DIR}/linux_log.c
)

function(supla_linux target_name)
  find_package(OpenSSL REQUIRED)
  find_package(Threads REQUIRED)

  target_sources(${target_name} PRIVATE ${SUPLA_DEVICE_LINUX_SRCS})

  target_include_directories(${target_name} PUBLIC
    ${SUPLA_LINUX_PORT_DIR}
  )

  target_compile_definitions(${target_name} PUBLIC SUPLA_LINUX SUPLA_DEVICE)

  target_link_libraries(${target_name} PUBLIC
    nlohmann_json::nlohmann_json
    cxxopts::cxxopts
    OpenSSL::SSL
    yaml-cpp
    mqttc
    OpenSSL::Crypto
    Threads::Threads
    ${CMAKE_DL_LIBS}
  )
endfunction()


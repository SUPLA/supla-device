include_guard()

include(FetchContent)

find_package(OpenSSL REQUIRED)
find_package(yaml-cpp REQUIRED)

option(SUPLA_LINUX_ENABLE_HTTP_SOURCE
  "Enable sd4linux HTTP source support when CURL is available" ON)
option(SUPLA_LINUX_REQUIRE_CURL
  "Fail configuration when sd4linux HTTP source support needs CURL but CURL is missing" OFF)

find_package(CURL QUIET)
set(SUPLA_LINUX_HTTP_SOURCE_ENABLED OFF)
if(CURL_FOUND AND SUPLA_LINUX_ENABLE_HTTP_SOURCE)
  set(SUPLA_LINUX_HTTP_SOURCE_ENABLED ON)
elseif(SUPLA_LINUX_REQUIRE_CURL)
  message(FATAL_ERROR
    "CURL is required for sd4linux HTTP source support. "
    "Install libcurl development package or disable SUPLA_LINUX_REQUIRE_CURL.")
endif()

set(SUPLA_LINUX_EXTENSION_DIRS "" CACHE STRING
  "Semicolon-separated sd4linux extension directories")
set(SUPLA_LINUX_EXTENSION_SRCS "")
set(SUPLA_LINUX_EXTENSION_INCLUDE_DIRS "")
set(SUPLA_LINUX_EXTENSION_LIBRARIES "")
set(SUPLA_LINUX_EXTENSION_INIT_FUNCTIONS "")

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
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
  ${SUPLA_LINUX_PORT_DIR}/linux_channel_factory.cpp

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
  ${_SUPLA_ROOT_FROM_LINUX}/src/supla/pv/solaredge.cpp
  ${_SUPLA_ROOT_FROM_LINUX}/src/supla/pv/afore.cpp

  ${_SUPLA_ROOT_FROM_LINUX}/src/supla-common/tools.c
  ${_SUPLA_ROOT_FROM_LINUX}/src/supla-common/eh.c
  ${_SUPLA_ROOT_FROM_LINUX}/src/supla-common/proto_check.cpp

  ${SUPLA_LINUX_PORT_DIR}/linux_log.c
)

if(SUPLA_LINUX_HTTP_SOURCE_ENABLED)
  list(APPEND SUPLA_DEVICE_LINUX_SRCS
    ${SUPLA_LINUX_PORT_DIR}/supla/source/http.cpp
  )
endif()

function(supla_linux_register_extension)
  cmake_parse_arguments(EXTENSION
    ""
    "NAME;INIT_FUNCTION"
    "SOURCES;INCLUDE_DIRS;LIBRARIES"
    ${ARGN}
  )

  if(NOT EXTENSION_NAME)
    message(FATAL_ERROR "supla_linux_register_extension requires NAME")
  endif()
  if(NOT EXTENSION_INIT_FUNCTION)
    message(FATAL_ERROR
      "supla_linux_register_extension requires INIT_FUNCTION")
  endif()

  list(APPEND SUPLA_LINUX_EXTENSION_SRCS
    ${EXTENSION_SOURCES}
  )
  list(APPEND SUPLA_LINUX_EXTENSION_INCLUDE_DIRS
    ${EXTENSION_INCLUDE_DIRS}
  )
  list(APPEND SUPLA_LINUX_EXTENSION_LIBRARIES
    ${EXTENSION_LIBRARIES}
  )
  list(APPEND SUPLA_LINUX_EXTENSION_INIT_FUNCTIONS
    ${EXTENSION_INIT_FUNCTION}
  )

  set(SUPLA_LINUX_EXTENSION_SRCS
    "${SUPLA_LINUX_EXTENSION_SRCS}" PARENT_SCOPE)
  set(SUPLA_LINUX_EXTENSION_INCLUDE_DIRS
    "${SUPLA_LINUX_EXTENSION_INCLUDE_DIRS}" PARENT_SCOPE)
  set(SUPLA_LINUX_EXTENSION_LIBRARIES
    "${SUPLA_LINUX_EXTENSION_LIBRARIES}" PARENT_SCOPE)
  set(SUPLA_LINUX_EXTENSION_INIT_FUNCTIONS
    "${SUPLA_LINUX_EXTENSION_INIT_FUNCTIONS}" PARENT_SCOPE)
endfunction()

foreach(extension_dir IN LISTS SUPLA_LINUX_EXTENSION_DIRS)
  set(extension_dir_candidates)
  if(IS_ABSOLUTE "${extension_dir}")
    list(APPEND extension_dir_candidates "${extension_dir}")
  else()
    list(APPEND extension_dir_candidates
      "${CMAKE_BINARY_DIR}/${extension_dir}"
      "${CMAKE_SOURCE_DIR}/${extension_dir}"
      "${SUPLA_LINUX_PORT_DIR}/${extension_dir}"
    )
  endif()

  set(extension_dir_abs "")
  foreach(extension_dir_candidate IN LISTS extension_dir_candidates)
    get_filename_component(extension_dir_candidate_abs
      "${extension_dir_candidate}" ABSOLUTE)
    if(EXISTS
        "${extension_dir_candidate_abs}/supla_linux_extension.cmake")
      set(extension_dir_abs "${extension_dir_candidate_abs}")
      break()
    endif()
  endforeach()

  if(NOT extension_dir_abs)
    get_filename_component(extension_dir_abs "${extension_dir}" ABSOLUTE)
  endif()

  set(extension_file "${extension_dir_abs}/supla_linux_extension.cmake")
  if(NOT EXISTS "${extension_file}")
    message(FATAL_ERROR
      "sd4linux extension file not found: ${extension_file}")
  endif()
  include("${extension_file}")
endforeach()

function(supla_linux target_name)
  find_package(OpenSSL REQUIRED)
  find_package(Threads REQUIRED)

  set(extension_init_file
    "${CMAKE_CURRENT_BINARY_DIR}/supla_linux_extensions_init_${target_name}.cpp")
  set(extension_init_declarations "")
  set(extension_init_calls "")
  foreach(init_function IN LISTS SUPLA_LINUX_EXTENSION_INIT_FUNCTIONS)
    string(APPEND extension_init_declarations
      "void ${init_function}();\n")
    string(APPEND extension_init_calls "  ${init_function}();\n")
  endforeach()
  file(WRITE "${extension_init_file}"
    "#include \"linux_extension_init.h\"\n"
    "\n"
    "namespace Supla {\n"
    "namespace Linux {\n"
    "\n"
    "${extension_init_declarations}"
    "\n"
    "void initExtensions() {\n"
    "  static bool initialized = false;\n"
    "  if (initialized) {\n"
    "    return;\n"
    "  }\n"
    "  initialized = true;\n"
    "${extension_init_calls}"
    "}\n"
    "\n"
    "}  // namespace Linux\n"
    "}  // namespace Supla\n")

  target_sources(${target_name} PRIVATE
    ${SUPLA_DEVICE_LINUX_SRCS}
    ${SUPLA_LINUX_EXTENSION_SRCS}
    ${extension_init_file}
  )

  target_include_directories(${target_name} PUBLIC
    ${SUPLA_LINUX_PORT_DIR}
    ${SUPLA_LINUX_EXTENSION_INCLUDE_DIRS}
  )

  target_compile_definitions(${target_name} PUBLIC SUPLA_LINUX SUPLA_DEVICE)

  if(SUPLA_LINUX_HTTP_SOURCE_ENABLED)
    target_compile_definitions(${target_name}
      PUBLIC SUPLA_LINUX_HTTP_SOURCE_ENABLED)
  endif()

  target_link_libraries(${target_name} PUBLIC
    nlohmann_json::nlohmann_json
    cxxopts::cxxopts
    OpenSSL::SSL
    yaml-cpp
    mqttc
    OpenSSL::Crypto
    Threads::Threads
    ${CMAKE_DL_LIBS}
    ${SUPLA_LINUX_EXTENSION_LIBRARIES}
  )

  if(SUPLA_LINUX_HTTP_SOURCE_ENABLED)
    target_link_libraries(${target_name} PUBLIC CURL::libcurl)
  endif()
endfunction()

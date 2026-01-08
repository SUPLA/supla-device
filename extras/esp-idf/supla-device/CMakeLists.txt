include(${COMPONENT_DIR}/../../../cmake/SuplaDeviceSources.cmake)
include(${COMPONENT_DIR}/../../../cmake/SuplaCommonSources.cmake)
include(${COMPONENT_DIR}/../../../cmake/NettleSources.cmake)

set(SUPLA_DEVICE_PORTING_SRCS
  ../../porting/esp-idf/esp_platform.cpp
  ../../porting/esp-idf/esp_idf_wifi.cpp
  ../../porting/esp-idf/esp_idf_gpio.cpp
  ../../porting/esp-idf/spiffs_storage.cpp
  ../../porting/esp-idf/nvs_config.cpp
  ../../porting/esp-idf/esp_idf_web_server.cpp
  ../../porting/esp-idf/esp_idf_ota.cpp
  ../../porting/esp-idf/esp_idf_client.cpp
  ../../porting/esp-idf/esp_mqtt.cpp
  ../../porting/esp-idf/esp_idf_network_common.cpp
  ../../porting/esp-idf/esp_idf_sector_wl_storage.cpp
  ../../porting/esp-idf/esp_idf_watchdog.cpp
  ../../porting/esp-idf/esp_idf_security_logger.cpp
)


idf_component_register(SRCS
  ${SUPLA_DEVICE_PORTING_SRCS}
  REQUIRES
    esp-tls
    nvs_flash
    spiffs
    esp_http_server
    esp_https_server
    app_update
    esp_http_client
    esp_wifi
    esp_driver_gpio
    mqtt
    esp_timer
    esp_partition
    mbedtls
  EMBED_TXTFILES supla_org_cert.pem supla_3rd_cert.pem
)

supla_device(${COMPONENT_LIB})
supla_common(${COMPONENT_LIB})
nettle(${COMPONENT_LIB})

target_include_directories(${COMPONENT_LIB} PUBLIC
  ${SUPLA_DEVICE_SRC_DIR}
  ${COMPONENT_DIR}/../../porting/esp-idf
)


target_compile_definitions(${COMPONENT_LIB} PRIVATE
  SUPLA_DEVICE
  SUPLA_DEVICE_ESP32
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DEVICE_ESP32)
if (CONFIG_SUPLA_DEBUG_BUILD)
  target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DEBUG)
  if (CONFIG_SUPLA_DISABLE_LOGS)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DISABLE_LOGS)
  endif()
else()
  target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DISABLE_LOGS)
endif()


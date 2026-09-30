idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS
    "${SUPLA_DEVICE_ROOT}/src"
    "${SUPLA_DEVICE_ROOT}/extras/porting/esp-idf"
    "${SUPLA_DEVICE_ROOT}/extras/examples/suplan_poc1_common"
  REQUIRES
    supla-device
    esp_event
    esp_netif
    esp_wifi
    lwip
    nvs_flash
)

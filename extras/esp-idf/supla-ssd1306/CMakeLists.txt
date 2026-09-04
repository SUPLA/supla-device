idf_component_register(
  SRCS "ssd1306.cpp"
  INCLUDE_DIRS "."
  PRIV_REQUIRES supla-device esp_driver_i2c
  REQUIRES supla-i2c-driver
)

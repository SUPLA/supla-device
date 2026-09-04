idf_component_register(
  SRCS "clock_pcf85063a.cpp"
  INCLUDE_DIRS "."
  PRIV_REQUIRES supla-device esp_driver_i2c
  REQUIRES supla-i2c-driver
)

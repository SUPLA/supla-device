if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.2")
  set(supla_modbus_uart_component esp_driver_uart)
else()
  set(supla_modbus_uart_component driver)
endif()

idf_component_register(SRCS "modbus_configurator_esp_idf.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES supla-device ${supla_modbus_uart_component}
                    )


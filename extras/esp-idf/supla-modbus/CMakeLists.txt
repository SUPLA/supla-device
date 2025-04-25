idf_component_register(SRCS "modbus_configurator_esp_idf.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES driver
                    PRIV_REQUIRES supla-device esp-modbus
                    )


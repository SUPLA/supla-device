
idf_component_register(SRCS "esp_ds18b20.cpp" "ds18b20_parameters.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES supla-device esp32-owb esp32-ds18b20
                    )


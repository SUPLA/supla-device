
idf_component_register(SRCS "esp_ds18b20.cpp" "ds18b20_parameters.cpp"
                              "../../porting/esp-idf/multi_ds_handler_esp_idf.cpp"
                    INCLUDE_DIRS "." "../../porting/esp-idf"
                    REQUIRES supla-device esp32-owb esp32-ds18b20
                    )

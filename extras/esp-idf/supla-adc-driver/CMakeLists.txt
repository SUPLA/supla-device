
idf_component_register(SRCS "esp_adc_driver.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES
                      supla-device
                    REQUIRES
                      esp_adc
                    )

